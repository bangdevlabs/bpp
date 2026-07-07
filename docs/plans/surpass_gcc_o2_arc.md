# Plan — surpass gcc -O2 on FP reductions (multi-accumulator reassociation + x64 float)

**Opened 2026-07-07.** After S1+S2 closed the FP-serial gap to PARITY (conv
1.02× gcc -O2, bit-exact), a hand-written experiment showed the milestone the
user asked for is real and measured: a 4-accumulator FIR runs **33 ms vs gcc
-O2's 89 ms — 2.7× FASTER** (`scratchpad/bench_ring4.bpp`, ring-based so gcc
cannot hoist). gcc -O2 stays single-accumulator for correctness (only
`-ffast-math` reassociates); b++ doing the reassociation under an opt-in
surpasses it.

**Goal:** b++ beats gcc -O2 on real FP reduction kernels (dot product, FIR,
sum-of-squares) on BOTH backends.

## The two facts that shape the arc

1. **The win is instruction-level parallelism, not threads.** A serial
   accumulator `acc = acc + a*b` is latency-bound (~4 cycles/tap, each fmadd
   waits on the previous). N independent accumulators put N fmadds in flight →
   throughput-bound. Universal to any pipelined superscalar core (a64 AND x86).

2. **b++'s float codegen is a64-only today.** `_a64_simd_temp_count()` = 6;
   `_x64_simd_temp_count()` = **0**, which disables the entire float-CIP on x64
   (the `_x64_emit_fma` / `_x64_emit_fconst_into` primitives are empty stubs).
   Reason: SysV AMD64 has zero callee-saved xmm. So S1/S2 already benefit only
   a64; x64 float runs the generic value-stack. Filling x64 float is its own
   part of this arc.

## Correctness vs performance verification (no x86 machine)

Rosetta runs x86-64 ELF **correctly** (semantics-preserving), so x64
correctness is fully testable here via Docker + bit-exact checksums — the whole
x64 backend was built this way. Rosetta **timing is meaningless** for real x86
(it's our x86 translated to ARM). So x86 performance is verified STRUCTURALLY by
disassembly: compile the kernel with `bpp --linux64` and with
`gcc -O2 -ffast-math`, `objdump` both, and confirm our loop has the same
N-independent-accumulator FMA structure. Matching gcc's structure inherits its
perf by microarchitectural certainty (Agner Fog: breaking a dependency chain
helps every superscalar core). The disasm IS the proof; the wall-clock number
is the one luxury we forgo.

## Opt-in: a flag, not new language surface

Reassociation changes FP rounding (add is not associative), so it CANNOT be the
silent default. But after the annotation cleanup (killing the phantom `@fast`,
the vestigial `@seq`, the `: serial` category error), the last thing this
codebase wants is a new annotation. The established idiom is a **compiler flag**
— gcc/clang's `-ffast-math` is exactly this: a build choice, not a keyword.
`--fast-math` adds ZERO language surface, matches the C idiom, and is honest
(reassociation is a build decision). Default OFF → bit-exact stays the default.
If per-scope control is ever genuinely needed (reassociate the FIR but not a
reference filter in the same binary), an annotation earns its place THEN, with a
real second consumer (Rule 20).

## Increments

- **M1 — `--fast-math` flag + reduction reassociation (a64 milestone).**
  - M1a: `--fast-math` flag plumbing (`global flag_fast_math`, parse in bpp.bpp,
    default 0). DONE-marker here.
  - M1b: the transform. Detect a for-loop whose body is exactly a single float
    reduction `ACC = ACC + TERM` over a unit-stride int induction var, and (under
    the flag) rewrite to 4 independent accumulators + a ×4 main loop (TERM cloned
    with IV→IV+1..3) + a remainder loop + the final `ACC = (a0+a1)+(a2+a3)`.
    Runs as an AST pass before codegen, so it is backend-agnostic (a64 gets the
    win now via its float-CIP; x64 inherits it once M2 lands).

    **Implementation notes (investigated 2026-07-07, ready to build):**
    - **Template exists.** The autovec ×4 unroll (`_av_build_vector_loop` /
      `_av_build_scalar_tail`, bpp_dispatch.bsm ~3717) is the exact shape:
      cond `i+4 <= bound` via `pack(vbuf_pos,2)` "<=", step `i += 4`, a cloned
      remainder loop `i < bound` step `i += 1`. Copy its structure; the only
      difference is the BODY (4 accumulator statements, not a SIMD store).
    - **Helpers.** `_dsp_make_var(name_p)`, `_dsp_make_int_lit(n)` /
      `_outline_make_int_lit(n)`, `_dsp_make_binop(op,l,r)`,
      `_dsp_make_assign(lhs,rhs)`, `make_node(T_BINOP/T_ASSIGN/T_BLOCK)`,
      `ast_clone_subst(node, names_buf, cnt, replacements_buf)`.
    - **Detection.** A for-loop is a `T_WHILE` with `node.e != 0` (the step; the
      init is a separate stmt before it). Match: `node.a` == `T_BINOP '<'` with
      left `T_VAR(IV)`; `node.e` == `IV = IV + 1`; body is exactly one stmt
      `T_ASSIGN(lhs=T_VAR(ACC), rhs=T_BINOP('+'|'-', T_VAR(ACC), TERM))` where
      ACC is a float local and TERM does not assign ACC or IV.
    - **TERM[IV→IV+j].** `ast_clone_subst(TERM, [IV], 1, [makeBinop('+', var(IV),
      lit(j))])` — substitutes the IV var-node with `(IV+j)`; one pass, does not
      recurse into the replacement, so `arr[IV]` → `arr[IV+1]`. Verified this is
      how outlining uses it.
    - **Splice.** Wrap `[decl a1,a2,a3:float; a1=0;a2=0;a3=0; main_while;
      rem_while; ACC=(ACC+a1)+(a2+a3)]` in a `T_BLOCK` and replace the original
      while node in its body array with the block. pre_reg_vars RECURSES into
      T_BLOCK (a64_codegen.bsm:870), so the new locals register + get offsets.
    - **THE ONE OPEN DETAIL — float-typing the synth locals.** pre_reg reads the
      per-declarator hint `cg_decl_var_hint(n, j)` (T_DECL `n.e[j]`) to set
      `cg_var_forced_ty = TY_FLOAT`. So the synth `a1,a2,a3` need a T_DECL node
      carrying a float hint in n.e (mirror how the parser builds `auto (a,b):
      float`). Inference (`auto x; x=0.0` → float, verified working) runs during
      PARSE and will NOT re-run on post-parse synth locals, so the hint must be
      explicit. Resolve the T_DECL hint-slot encoding first (read the parser's
      grouped-typed-decl builder), then the rest is mechanical.
    - **Where it runs.** A new pass `reassoc_reduction_loops()` gated on
      `flag_fast_math`, in the dispatch phase alongside the other loop rewrites
      (call it in bpp.bpp before codegen, both dispatch sites). Off = the pass
      returns immediately → byte-identical.
    - **Risk.** AST construction is the class of change the codebase repeatedly
      finds only via Docker checksum — build incrementally, test the float-typed
      synth local in isolation FIRST, then a minimal `sum += a[i]*b[i]` loop,
      then stbconv, verifying bit-approx (epsilon) each step.
  - M1c: rewrite stbconv's inner loop to the index-reduction shape the transform
    recognizes, so the real consumer benefits. Measure a64 vs gcc -O2 → the
    milestone (target: beat 85 ms).
  - Gate: `--fast-math` off = byte-identical (the transform never fires). On =
    epsilon-checksum (reassociation changes low bits) with a documented tolerance.

- **M2 — x64 float codegen (so x64 also beats gcc -O2).** Fill the x64 float-CIP
  primitives using CALLER-saved xmm for loop-local floats (the accumulators and
  taps never cross a call inside a leaf DSP loop, so SysV's zero callee-saved
  xmm does not block them). `_x64_simd_temp_count` > 0, real `_x64_emit_fma` /
  `_x64_emit_fconst_into` / `_x64_fload_promoted_into` / fmov. Then S1, S2, and
  the M1 reassociation all apply to x64. Verify: Docker correctness (bit-exact)
  + disasm structure vs `gcc -O2 -ffast-math` x86 output.

## Gate + measurement per increment

Bit-exact where the flag is off (byte-identical binaries). With `--fast-math`,
epsilon checksum + tolerance. Full ritual each step: bootstrap gen2==gen3,
suites both backends, catalog re-run, Docker x64, zero warnings. The headline
number is a64 conv vs gcc -O2 (target < 85 ms → surpass); the x86 number is a
disasm-structure match, not a wall-clock.
