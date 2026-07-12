# The B++ Compiler — Internals & Optimization Map

A study-and-orientation guide to how the B++ compiler works: the
pipeline, the architecture that lets one binary target two CPUs, every
optimization phase, and where each thing lives in the source. It is
written for two readers:

- **Someone learning B++ and its compiler** who wants a map of the
  machinery instead of 370 KB of dispatch code to reverse-engineer.
- **An agent or engineer about to change the compiler** who needs to
  situate fast: which file, which function, which layer, what to
  bootstrap.

For the *usage* language, read `bpp_manual.md` / `how_to_dev_b++.md`
first. For the *change ritual* (bootstrap cycle, the golden rule, the
six-layer cake), read `bootstrap_manual.md` — this document is the "how
it works", that one is the "how to work on it". For the two-backend
contract, read `backend_parity.md`. For the day-by-day history behind
every phase named here, read `journal.md`.

---

## 1. The 90-second model

- **Self-hosting.** `bpp` is written in B++ and compiles itself. Every
  compiler change goes through the bootstrap cycle (gen1 → gen2 → gen3,
  gen2==gen3 is stability). Full self-compile is ~0.3 s.
- **One process, one pass-ish.** Source → merged source → tokens → AST →
  types → dispatch/classify → machine code, straight into a byte buffer,
  then a binary writer. **There is no module cache** (removed in
  0.23.x); every invocation compiles every module from source.
- **A baseline accumulator machine, lifted in layers.** The unoptimized
  core is a stack + accumulator code generator (≈ `gcc -O0`): every
  expression evaluates into a single accumulator register (`x0` / `rax`
  for ints, `d0` / `xmm0` for floats), pushing intermediates on a value
  stack. On top of that sit the optimization layers — register
  promotion, compute-in-place, the inliner, linear-scan allocation,
  SIMD, strength reduction — that together pull hot code up toward
  `gcc -O2` parity. Understanding this "baseline + layers" split is the
  single most useful mental model: most of the code you will read is a
  layer deciding *when it is safe to do better than the accumulator*.
- **The spine + chip-primitive architecture.** AST-shape decisions live
  once, in `bpp_codegen.bsm` (the *spine*). The CPU-specific part is a
  table of function pointers (`ChipPrimitives`, a.k.a. `cg_prim`) that
  each backend fills in at init. The spine walks the AST and calls
  `cg_prim` for the atoms; the chip decides only *how* to emit, never
  *what* to emit. This is why one binary links both a64 and x64 and why
  a new optimization usually lands in one place and works on both.

---

## 2. The six-layer cake (orientation)

Risk and skill rise as you go down. Full detail in `bootstrap_manual.md`.

```
 1  PROGRAMS   games/ examples/ tools/        (daily; breaks one program)
 2  STB        stb/  auto-injected stdlib     (weekly)
 3  COMPILER   src/*.bsm frontend + runtime   (monthly; bootstrap mandatory)
 4  BACKEND    src/backend/ chip + os + target(quarterly; breaks every binary)
```

This document is about **Layer 3 (the compiler pipeline + spine)** and
**Layer 4 (the chip/os/target backends)**. A change to how the compiler
*decides* to emit is Layer 3 (the spine); a change to the *bytes* of an
instruction is Layer 4 (a chip encoder).

---

## 3. The pipeline

`main()` in `src/bpp.bpp` runs the whole thing. After the `init_*` block
(every subsystem registers its state and, for the chips, fills `cg_prim`),
the stages are:

| # | Stage | File(s) | What it produces |
|---|-------|---------|------------------|
| 1 | **Import resolution** | `bpp_import.bsm` | Topologically-sorted module list (`mod_topo`); auto-injects the runtime (bpp_array, bpp_io, bpp_mem, brt0, …) and, for programs, stb cartridges. Cycles fail the build. |
| 2 | **Lex** | `bpp_lexer.bsm` | Tokens as *packed refs* — one 64-bit word `(offset<<32)|len` into the shared source buffer, zero heap per token. |
| 3 | **Parse** | `bpp_parser.bsm` | The AST: `Node` records with an `ntype` (T_*) and typed slots. Structs, functions, globals registered into hash-backed tables. |
| 4 | **Type inference** | `bpp_types.bsm` (`infer_module`) | Per-declarator types, float/int propagation, `: type` slice widths, the T2 *smart* `: float` promotion (the compiler writes the annotation), str/consensus parameters. |
| 5 | **Dispatch / classify** | `bpp_dispatch.bsm` | Phase + effect classification (`@safe` enforcement, the internal AUTO/BASE/SOLO model), the inliner's pre-registration walk, and the smart-dispatch *outlining* transform (`synthesize_loop_fn` + `rewrite_dispatch_loops`) that turns eligible `@safe` loops into parallel worker functions. Biggest file in the tree (372 KB). |
| 6 | **Validate** | `bpp_validate.bsm`, `bpp_diag.bsm` | The `E###` / `W###` diagnostics. `diag_check_errors()` aborts if any error fired. |
| 7 | **Emit** | `bpp_codegen.bsm` (spine) + `backend/chip/<arch>/` | Machine code straight into the encoder buffer, one module at a time in topo order (`emit_module_arm64` / `emit_module_x86_64`). |
| 8 | **Resolve cross-module calls** | `bo_resolve_calls_arm64` / `_x64` | Patch call sites now that every function's address is known. |
| 9 | **Write binary** | `backend/target/aarch64_macos/a64_macho.bsm`, `backend/target/x86_64_linux/x64_elf.bsm` | Consume the encoder buffer + the string/float/global tables → Mach-O or ELF. |

The `--c` flag replaces stages 7–9 with `backend/c/bpp_emitter.bsm`,
which emits portable C99 — a second, independent codegen path that
catches a different class of bug (the C compiler rejects what the native
backend silently accepted). Both must stay green; `tests/run_all_c.sh`
is its suite.

---

## 4. The codegen architecture — spine + chip primitives

After the **Wave 20/21 refactor (May 2026)**, AST-shape decisions live
in one place. Three concepts:

- **The spine — `src/bpp_codegen.bsm`.** Owns the two dispatchers:
  - `cg_emit_stmt(nd)` — the statement walker (T_DECL, T_NOP, T_BLOCK,
    T_BREAK, T_CONTINUE, T_IF, T_WHILE, T_RET, T_ASSIGN, T_MEMST,
    T_SWITCH, and the expression-statement fall-through).
  - `cg_emit_node(nd)` — the expression walker (T_LIT, T_VAR, T_TERNARY,
    T_UNARY, T_CALL, T_MEMLD, T_ADDR, T_BINOP).
  Plus `cg_builtin_dispatch` (peek/poke/malloc/sys_*/vec_* lowered
  inline instead of as calls), the compute-in-place walkers
  (`cg_emit_int_into`, `cg_emit_float_into`), and the inline splicer
  (`cg_emit_inline_multi`).

- **`ChipPrimitives` (`cg_prim`).** A struct of function pointers — the
  ISA-atomic operations the spine calls: `emit_add`, `emit_sub`,
  `emit_iop_into`, `emit_ishl_imm_into`, `emit_store_var_typed`,
  `int_temp_alloc`, `emit_jump`, `cur_text_pos`, and ~100 more. Each
  backend fills it at init (`init_codegen_arm64` / `init_codegen_x86_64`
  in `<arch>_codegen.bsm`). The spine calls a primitive with
  `call(pp.emit_add, …)`; it never knows which chip answered.

- **The three-file chip split** under `backend/chip/<arch>/`:
  - `<arch>_codegen.bsm` — registers the primitives; owns the pieces
    that are genuinely chip-aware (the T_CALL handler, because the call
    ABI + inline gating differ; the prologue/epilogue; the B3 register
    selectors).
  - `<arch>_primitives.bsm` — the ISA-atomic emit functions the table
    points at (`_a64_emit_iop_into`, `_x64_emit_ishl_imm_into`, …).
  - `<arch>_enc.bsm` — raw instruction encoding: functions that pack a
    32-bit a64 word or an x64 REX/opcode/ModRM/SIB sequence into
    `enc_buf` at `enc_pos`. This is the only layer that knows bit
    positions.

**Deciding where a change goes** (the standing rule): does the `bpp`
binary need *both* variants linked? If the decision is shape-shared
across chips → spine. If it is one ISA op both chips need → a new
`ChipPrimitives` field implemented per chip. If it is genuinely
chip-only (ABI-fragile, encoding) → the chip file with a comment saying
why. Worked example from 2026-07-12: strength-reducing `x*3` is a spine
decision (`cg_const_shl_add` detects the shape); the actual
`add xd,xn,xn,lsl #k` vs `lea` is a chip primitive
(`emit_imul_shladd_into`).

---

## 5. The codegen model — an accumulator machine, optimized in layers

The baseline: every expression computes into the accumulator (`x0`/`rax`
int, `d0`/`xmm0` float), and a `T_BINOP` that needs to keep a left
operand across a right subtree pushes it (the *value stack*, depth
tracked in `cg_depth`). An assignment then stores the accumulator into
the variable's home. This always works and is always correct; it is also
slow, because every operation round-trips through one register.

Every optimization below is a layer that proves *it is safe to skip part
of that round-trip*:

- **B3 promotion** gives a hot local a home *in a register* instead of on
  the stack (`cg_var_promote[vi]` = physical reg, or −1 for stack). Then
  reads/writes are register moves, not loads/stores.
- **Compute-in-place (CIP)** emits an expression tree *directly into a
  destination register* with no accumulator shuttle — `a = b*c + d`
  becomes one `madd` into `a`'s register. The spine walkers
  `cg_emit_int_into(nd, dreg)` / `cg_emit_float_into(nd, dreg)` do this;
  the admission gate `cg_assign_into_dest_ok` decides when the shape is
  safe (single instruction reading register/const leaves, so the
  destination can even be one of the sources).
- **RegAlloc v2** replaces B3's greedy ref-count pick with liveness-based
  linear scan, so two locals that are never alive at once *share* one
  register.
- **The inliner** removes the call frame entirely for hot small callees,
  exposing their bodies to all the above.

When you read the codegen, you are almost always reading one of these
gates asking "can I do better than the accumulator here?". `x0`/`rax`
appearing in a hot loop is the tell that some gate declined.

---

## 6. The optimization phases

Named by their historical labels (the ones the journal and
`bootstrap_manual.md` use). "both" = shipped on a64 and x64.

### B0 — Constant folding + strength reduction — spine, both (+ C via gcc)
Folds compile-time constants and rewrites cheap-for-expensive:
`x * 2^k → x << k` (`cg_const_pow2`), and since 2026-07-12
`x * (2^k±1) → x + (x<<k)` / `(x<<k) − x` (`cg_const_shl_add` /
`cg_const_shl_sub` → `emit_imul_shladd_into` / `emit_imul_shlsub_into`;
a64 shifted-add & `lsl;sub`, x64 `lea` & three-operand `imul`). Lives in
`cg_emit_int_into`'s `*` case.

### B1 — Expression freelist — chip primitives
A `T_BINOP` left operand or `T_MEMST` store value that would push/pop the
stack is parked in a free *caller-saved* register instead — when the
sibling subtree has no call to clobber it. Depth differs by ABI: a64
`x9..x15` (7 deep), x64 `r11` (1) widening to `r11+r8+r9` (3) in leaf
functions. This shallow x64 freelist is why some CIP shapes that need a
temp are gated off on x64 (a throughput ceiling, not a correctness one).

### B2 / S4 / VI — The inliner — spine splicer + dispatch pre-reg walk
The arc that dominated June–July. Three shapes:
- **B2** — trivial inline: a single-`T_RET`, ≤5-node, ≤3-param callee is
  spliced at the call site (chip T_CALL fast path).
- **S4** — cost-model multi-statement inline: `cg_emit_inline_multi`
  (spine) splices a multi-statement body; `bpp_dispatch.bsm`'s
  `_inline_pre_reg_walk` + `classify_inlineable` pre-register the
  callee's locals as *mangled slots* (`_inl<N>_<name>`) in the caller so
  register allocation sees them. Control-flow leaf bodies, early
  returns, and constant descent all fall under this (levers 1–4).
- **VI** — void / statement-context inline: same splicer, the void shape
  detected at the last-statement check.

The hard-won invariant: **the pressure account**. A loop-carrying callee
is admitted only while `arr_len(cg_vars) + callee_slot_need <
INLINE_PRESSURE_CAP` (14, the a64 non-leaf budget), so inlining never
over-subscribes the register file. Denial is registration-time and rides
the `.e == 0 → real bl` convention, so registration and splice can never
disagree. `BPP_INLINE_PROBE=1` prints the classifications and
admit/deny decisions.

### B3 — Local register promotion — spine selector + chip pool
Ranks locals (a Sethi-Ullman-flavored ref-count walk) and homes the
hottest in callee-saved registers: a64 `x19..x28` (10 non-leaf, widened
to 14 with `x9..x12` in leaf functions), x64 `rbx/r12..r15` (5).
`cg_var_promote[vi]` records the pick. Constant promotion (Stage 2a)
puts hot *constants* in registers the same way (`cg_const_reg`).

### RegAlloc v2 — liveness → linear scan — `src/bpp_regalloc.bsm`, both
The "big-league" allocator that supersedes B3's greedy pick when it can
prove a win. Pipeline: CFG construction → liveness dataflow → RPO
linearization → live-interval construction → linear-scan assignment. Two
locals with non-overlapping intervals *share* a register. Shipped as
staged increments:
- **Stage A–F** (June): the analysis, then the codegen swap (Stage E),
  then use-count spill-victim selection (Stage F). Gated by a systematic
  `regalloc_compare_vs_b3` sweep + a no-live-range-splitting check;
  falls back to B3 whenever unsure.
- **Phases 3–6**: teach the interval builder to see *through inlined call
  sites* (the mangled slots), including control flow inside an expanded
  callee.
- **M1–M6** (July): M1 activated linear scan on **x64** (it had been a
  `return 0` stub); M2 added caller-saved spilling (promote into a
  caller-saved band in non-leaf functions, save/restore around each
  call); M3 the cost model that decides when that wrap is worth it
  (`refs > 2× loop-weighted call count`); M4 cross-call *float*
  promotion (the wrap is the only way x86-64 keeps a float in a register
  across a call — SysV has no callee-saved XMM); M5 live-range wrap
  masking (skip band entries provably dead at a call); M6 the promotion
  gate that charges what M5 emits.

The degenerate-interval trap (`_rg_var_span_degenerate`): a
struct-pointer inline local can get a single-point interval if the
expansion sees its def but misses later field reads; the fix refuses the
swap on any promoted mangled slot whose total span is one position.

### CIP — Compute-in-place — spine walkers, both
`cg_emit_int_into` / `cg_emit_float_into` emit a `+ - * / & | ^` (float:
`+ - * /`) tree straight into a destination register. Leaves are
register-homed locals, hot constants, or memory loads; the right subtree
gets a freelist temp. The **float** side rides `d8..d15` on a64 (the
`peekfloat` leaf and `fmadd` fusion — FP scheduler S1 — took `bench_conv`
from 3.4× to parity). The **integer** side (2026-07-07) is destination-
driven with no `x0` shuttle; the const-leaf gate (2026-07-12) lets
`var = var OP const` compute in place too. x64 CIP is real but scope-
capped by its 1-deep freelist.

### B4 — SIMD — spine builtin dispatch + chip emit, both
`: double` 128-bit locals, the float `vec_*` family (4×f32), and the byte
`vec_load16b … vec_movemask` kit (16×u8). a64 NEON, x64 SSE2, C via
intrinsics.

### Smart-dispatch outlining — shared transform in `bpp_dispatch.bsm`
Auto-parallelizes an eligible `@safe` loop that references the caller
frame: `synthesize_loop_fn` builds a worker function, `rewrite_dispatch_
loops` rewrites the loop into a parallel dispatch (macOS threads today;
serial-correct on Linux until ELF threads ship). Runs before per-chip
emit, so both backends inherit it.

### Autovec + loop-head alignment
A loop pattern recognizer lifts eligible loops to `vec_*`
(`bench_autovec_gate.sh` is the gate). `emit_align_loop` pads loop heads
to a 16-byte fetch boundary on both backends — a real lever, since a
tight 20 M-iteration loop can swing 14% purely on where its head lands
(the M3 layout-luck lesson).

---

## 7. Key data structures

| Name | Where | What |
|------|-------|------|
| `Node` + `T_*` | parser | The AST. `ntype` selects the shape; slots `.a`–`.e` hold operands/operator/targets (meaning is per-`T_*`). |
| packed ref | lexer/parser | `(offset<<32)\|len` into the source buffer. `unpack_s` / `unpack_l` / `packed_eq` / `make_packed_tok`. Zero-alloc tokens. |
| `cg_prim` (`ChipPrimitives`) | spine + chips | The fn-ptr table the spine calls for every ISA atom. |
| `cg_var_promote[vi]` | codegen | Physical register for local `vi`, or −1 for a stack home. The output of B3 / RegAlloc v2. |
| `cg_var_off` / `cg_var_stack` / `cg_var_forced_ty` | codegen | Frame offset, address-taken flag, and slice type of each local. |
| `enc_buf` / `enc_pos` / `enc_cap` | `<arch>_enc.bsm` | The growable code byte buffer, current write offset, capacity. `enc_emit32` / `enc_patch32`. |
| `cg_depth` | codegen | Value-stack depth (accumulator spills). |
| `cg_str_tbl` / `cg_flt_tbl` | codegen | String- and float-literal tables the binary writer materializes. |
| `mod_topo` | import | Topologically-sorted module indices. |

---

## 8. "I need to change X — where do I look"

| Task | Start here |
|------|-----------|
| New operator / control-flow sugar | `bpp_parser.bsm` (lower it in the frontend; backends never see it) |
| New builtin (`peek_h`, `vec_add`, a syscall) | 4 places: `a64_codegen.bsm` + `x64_codegen.bsm` (emit) + `bpp_validate.bsm` (`val_is_builtin`) + `backend/c/bpp_emitter.bsm` (C). See bootstrap_manual "Adding a New Builtin". |
| New optimization on an AST shape | the spine (`cg_emit_stmt` / `cg_emit_node` / `cg_emit_int_into`), with a `ChipPrimitives` field if it needs an ISA op |
| A codegen-quality gap on one kernel | disassemble it (`bug --disasm` or `otool -tv`/`objdump`), find the shape, add a spine gate. Compare against `gcc -O2` (the oracle). |
| Register allocation behavior | `bpp_regalloc.bsm` (v2) and the chip B3 selectors in `<arch>_codegen.bsm` |
| Inliner admission / a splice bug | `bpp_dispatch.bsm` (`classify_inlineable`, `_inline_pre_reg_walk`, the pressure account) + `cg_emit_inline_multi` in the spine. `BPP_INLINE_PROBE=1`. |
| An instruction encodes wrong | `<arch>_enc.bsm` — verify the bytes against the ISA manual, then a disasm A/B |
| A new diagnostic | `bpp_validate.bsm` (`E###`/`W###`), documented in `warning_error_log.md` |
| Binary-format issue (relocs, sections) | `backend/target/*/` (`a64_macho.bsm` / `x64_elf.bsm`) |
| A `--c` divergence | `backend/c/bpp_emitter.bsm` |

---

## 9. File map (Layer 3 + 4, by weight)

```
src/bpp.bpp            pipeline entry (main): init → parse → infer → dispatch → validate → emit → write
src/bpp_dispatch.bsm   phase/effect classification, inliner pre-reg, outlining        (372 KB — biggest)
src/bpp_codegen.bsm    THE SPINE: cg_emit_stmt/node, CIP walkers, builtins, splicer   (277 KB)
src/bpp_parser.bsm     tokens → AST, struct/func/global tables                        (147 KB)
src/bpp_regalloc.bsm   RegAlloc v2: CFG→liveness→intervals→linear-scan                (139 KB)
src/bpp_types.bsm      type inference, float/str propagation, T2 promotion            ( 76 KB)
src/bpp_validate.bsm   E###/W### diagnostics                                          ( 68 KB)
src/bpp_import.bsm     module graph, topo sort, auto-injection                        ( 61 KB)
src/bpp_lexer.bsm      tokenizer (packed refs)                                        ( 26 KB)
src/bpp_internal.bsm   shared compiler structs + packed-ref/buf_eq helpers            ( 13 KB)
src/bpp_defs.bsm       T_*, E###, slice-type constants

src/backend/chip/aarch64/   a64_codegen.bsm · a64_primitives.bsm · a64_enc.bsm
src/backend/chip/x86_64/    x64_codegen.bsm · x64_primitives.bsm · x64_enc.bsm
src/backend/target/         aarch64_macos/a64_macho.bsm · x86_64_linux/x64_elf.bsm
src/backend/os/             macos/ · linux/  (syscalls, startup, platform, audio)
src/backend/c/              bpp_emitter.bsm  (--c path)

src/bug*.bsm / src/bug.bpp  the debugger (see below) — its own subsystem, not the compiler proper
```

Auto-injected runtime (compiled into every program, not the compiler
pipeline): `bpp_array`, `bpp_buf`, `bpp_hash`, `bpp_io`, `bpp_math`,
`bpp_mem`, `bpp_str`, `bpp_time`, `bpp_job`, `bpp_maestro`, `brt0`.

---

## 10. Working on it — the ritual and the debugger

**The ritual** (full version in `bootstrap_manual.md`):
1. Edit source. Tonify as you write (`tonify_checklist.md`).
2. Build gen1 from the current `./bpp`, gen2 from gen1. A codegen change
   makes gen1 ≠ gen2 (normal 1-cycle oscillation); **gen2 == gen3** is
   the stability criterion (`tests/test_bootstrap_stable.sh`).
3. **Never overwrite `./bpp` with an untested binary** (the golden rule).
   Install gen2 only after it is validated.
4. Suites: `tests/run_all.sh` (native) + `tests/run_all_c.sh` (C emit).
   For a codegen change also re-run the benchmark catalog and check the
   audio md5 (`f61fac72…`), and self-host x64 under Docker.

**The debugger — `bug`** (`src/bug*.bsm`). Compile with `--bug` to emit a
`.bug` map, then:
- `bug --disasm <bin> <fn>` — the truth about what was emitted. The
  standing discipline: disassemble both your output and the oracle
  before naming a gap; a benchmark delta between two compiler builds is
  as often layout luck as a real change.
- `bug --tui` / `bug --watch <vars>` — inspect variables *by name* at a
  breakpoint (which raw lldb registers cannot give), the tool that has
  cracked several "silently wrong output" codegen bugs.

---

## 11. The measurement discipline

The compiler's optimization work is held to `gcc -O2` (the *oracle*) via
`docs/manual/benchmarks.md`. Two rules that keep it honest, both learned
the hard way:

- **Measure the reference, not just our output.** Disassemble gcc's loop
  before assuming what it does. Believing gcc auto-vectorized a dot
  product (it didn't — it unrolled and scheduled) wasted an arc.
- **On a latency-bound kernel, count critical-path latency, not
  instructions.** `manylive` (2026-07-12) proved it: removing the
  multiplies from the serial chain moved the clock; removing off-path
  constant-movs (which the out-of-order core hides) did not, however
  much smaller the loop looked. The residual gap there is algebraic
  reassociation — an optimizer pass b++ does not yet have — not a
  peephole.

A ratio gate against an improving baseline decays on its own; when one
flickers, decompose numerator vs denominator before touching code, and
never call a FAIL "noise" until the split proves it.

---

## 12. Further reading

- `bootstrap_manual.md` — the change ritual, the six-layer cake, adding
  a builtin/module, the optimization-phases chapter this doc expands.
- `backend_parity.md` — the a64/x64 capability table; what is genuinely
  architectural (float family on x64) vs implementable.
- `tonify_checklist.md` — the idiom rules every new compiler function
  gets (storage class, visibility, `void`, switch/for, comments).
- `spine_analysis.md` — the design essay on whether the spine is a
  "portable assembly" and what becoming a real optimizing IR would take.
- `journal.md` — the dated narrative behind every phase named here.
- `warning_error_log.md` — the `E###` / `W###` catalogue.
