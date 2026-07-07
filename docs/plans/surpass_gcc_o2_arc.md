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
