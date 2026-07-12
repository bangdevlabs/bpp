# The B++ Compiler — How It Works

A precise map of how the B++ compiler is wired **as it stands now**: the
stages, the data that flows between them, the codegen core function by
function, and where every mechanism lives in the source. This is a
present-tense wiring diagram, not a history — for *why* a thing is the
way it is, read `journal.md`; for the *change ritual* (bootstrap cycle,
the golden rule), read `bootstrap_manual.md`; for the two-CPU contract,
read `backend_parity.md`.

Written for the reader who is about to open the compiler and needs to
know which function calls which, what each one reads and writes, and
which layer a change belongs in.

---

## 1. The mental model

The compiler is a single process that turns source into a native binary
in one forward pass: **source → tokens → AST → types → classification →
machine code → binary**. There is no intermediate on-disk form and no
module cache; every run compiles everything from source (a full
self-compile is ~0.3 s).

The code generator is, at its floor, a **stack + accumulator machine**
(the shape of an unoptimized C compiler): every expression evaluates into
one accumulator register — `x0`/`rax` for integers, `d0`/`xmm0` for
floats — and any operand that must survive a subexpression is pushed onto
a **value stack**. On top of that floor sit the optimizations, and each
one is a *gate that proves it is safe to skip part of the accumulator
round-trip*: keep a hot local in a register (promotion), compute an
expression tree straight into its destination (compute-in-place), share
one register between two locals that are never both live (linear-scan
allocation), or drop a call frame entirely (inlining). When you read the
codegen, you are almost always reading one such gate asking "can I do
better than the accumulator here?" — `x0`/`rax` in a hot loop is the tell
that a gate declined.

One binary targets two CPUs. The machine-independent decisions live in a
single **spine** file; the CPU-specific instruction emission is reached
through a **table of function pointers** the two backends fill in. This
is the structural fact that makes most optimizations land once and work
on both a64 (ARM64/macOS) and x64 (x86-64/Linux).

---

## 2. The pipeline

`main()` in `src/bpp.bpp` runs the stages in order. After an `init_*`
block where every subsystem sets up its state — and, crucially, each
backend fills the primitive table (`init_codegen_arm64` /
`init_codegen_x86_64`) — the flow is:

| Stage | Entry point(s) | File | Reads → Writes |
|-------|----------------|------|----------------|
| **Imports** | import resolution | `bpp_import.bsm` | source files → a topologically sorted module list (`mod_topo`); auto-injects the runtime + (for programs) stb cartridges. Cycles fail. |
| **Lex** | tokenizer | `bpp_lexer.bsm` | source bytes → tokens as *packed refs* (one 64-bit word `(offset<<32)\|len` into the source buffer; no per-token allocation). |
| **Parse** | parser | `bpp_parser.bsm` | tokens → the AST (`Node` records); fills the struct/function/global tables (hash-backed). |
| **Type inference** | `infer_module(mi)` | `bpp_types.bsm` | AST → per-node/per-local types; float↔int propagation; slice widths; the smart `: float` local promotion the compiler writes back; str/consensus parameters. |
| **Classify / transform** | `find_dispatch_candidates`, `synthesize_loop_fn`, `rewrite_dispatch_loops`, `run_dispatch` | `bpp_dispatch.bsm` | AST → effect/phase classification (`@safe` enforcement), the inliner's pre-registration data, and the outlining rewrite (an eligible `@safe` loop becomes a parallel worker). Largest file in the tree. |
| **Validate** | `run_validate`, `diag_check_errors` | `bpp_validate.bsm`, `bpp_diag.bsm` | AST → `E###`/`W###` diagnostics; aborts on any error. |
| **Emit** | `emit_module_arm64(mi)` / `emit_module_x86_64(mi)` | `bpp_codegen.bsm` + `backend/chip/<arch>/` | AST → machine code in the encoder byte buffer, one module at a time in topo order. |
| **Resolve calls** | `bo_resolve_calls_arm64` / `_x64` | codegen | patch every call site now that all function addresses are known. |
| **Write binary** | Mach-O / ELF writer | `backend/target/aarch64_macos/a64_macho.bsm`, `backend/target/x86_64_linux/x64_elf.bsm` | encoder buffer + string/float/global tables → the output file. |

`--c` replaces Emit/Resolve/Write with `backend/c/bpp_emitter.bsm`, which
prints C99 — a second, independent codegen path (the C compiler rejects
what the native backend silently accepts, so it catches a different class
of bug). Its suite is `tests/run_all_c.sh`.

---

## 3. The data that flows: the AST

The whole middle and back end operate on one record type:

```
struct Node { ntype: byte, a, b, c, d, e, dispatch: byte, itype: quarter, src_tok }
```

`ntype` is a `T_*` tag (`bpp_defs.bsm`: `T_LIT=0, T_VAR=1, T_BINOP=2,
T_UNARY=3, T_ASSIGN=4, …`). The `.a`–`.e` slots hold operands, operator
chars, or target arrays — **their meaning is per-`ntype`**, so every
walker switches on `ntype` first. Two families matter for codegen:

- **Expression nodes** (produce a value): `T_LIT`, `T_VAR`, `T_TERNARY`,
  `T_UNARY`, `T_CALL`, `T_MEMLD`, `T_ADDR`, `T_BINOP`.
- **Statement nodes** (side effects): `T_DECL`, `T_NOP`, `T_BLOCK`,
  `T_BREAK`, `T_CONTINUE`, `T_IF`, `T_WHILE`, `T_RET`, `T_ASSIGN`,
  `T_MEMST`, `T_SWITCH`.

For a `T_BINOP`, `.a` packs the operator (`unpack_l`/`unpack_s` read its
length/offset into the source), `.b` is the left operand, `.c` the right.
`src_tok` maps a node back to its source line (the debugger's line
table). Literals carry a numeric shape so the const path can tell an
integer from a float from a string.

Alongside the AST, codegen maintains parallel per-local arrays keyed by a
variable's index `vi` — the tables the register allocator writes and the
emitter reads (see §7).

---

## 4. The spine and the chip-primitive table

Three concepts carry the two-backend design:

- **The spine — `src/bpp_codegen.bsm`.** Owns every AST-shape decision:
  the per-function driver (`cg_emit_func`), the statement walker
  (`cg_emit_stmt`), the expression walker (`cg_emit_node`), the
  compute-in-place walkers (`cg_emit_int_into` / `cg_emit_float_into`),
  the builtin lowering (`cg_builtin_dispatch`), and the inline splicer
  (`cg_emit_inline_multi`). It decides *what* to emit.

- **`ChipPrimitives` — the table, held in `cg_prim`.** A struct of
  function pointers: the ISA-atomic operations (`emit_add`, `emit_sub`,
  `emit_iop_into`, `emit_ishl_imm_into`, `emit_store_var_typed`,
  `emit_push_acc`, `emit_bank_pop`, `int_temp_alloc`, `emit_jump`,
  `b3_select`, `regalloc_apply`, `emit_prologue_full`, `cur_text_pos`,
  …). The spine invokes them with `call(pp.emit_add, …)` and never knows
  which CPU answered. Each backend installs its pointers at init.

- **The three-file chip split** under `backend/chip/<arch>/`:
  - `<arch>_codegen.bsm` — installs the primitives; owns the pieces that
    are genuinely CPU-aware: the `T_CALL` handler (the call ABI and
    inline gating differ), the prologue/epilogue, the register selectors.
  - `<arch>_primitives.bsm` — the ISA-atomic emit functions the table
    points at (`_a64_emit_iop_into`, `_x64_emit_ishl_imm_into`, …).
  - `<arch>_enc.bsm` — raw encoding: the only place that knows bit
    positions. Writes 32-bit a64 words or x64 REX/opcode/ModRM/SIB
    sequences into the encoder buffer `enc_buf` at `enc_pos`
    (`enc_emit32`, `enc_patch32`).

**Deciding where a change goes:** if the `bpp` binary needs *both*
variants linked, it is a spine decision (with a `ChipPrimitives` field if
it needs a new ISA op); if it is one instruction both CPUs need, it is a
new primitive implemented per chip; if it is ABI- or encoding-specific,
it is a chip file. Concrete: recognizing that `x*3` can be a shifted add
is a spine decision (`cg_const_shl_add`); emitting `add xd,xn,xn,lsl #k`
vs `lea` is a chip primitive (`emit_imul_shladd_into`).

---

## 5. The per-function driver — `cg_emit_func(fi)`

This is the center of the compiler. `emit_module_*` loops over the
module's functions and calls `cg_emit_func(fi)` for each; everything
about how one function becomes code is sequenced here (the source
numbers its own steps):

1. **Bind parameters** into the per-local tables (`cg_var_add`), applying
   any declared type hints.
2. **Pre-register body locals** — `call(p.fn_pre_reg_vars, …)` walks the
   body, pre-declares every `auto` (including those nested in blocks) and
   assigns frame offsets, and — the same pass — finds inlineable
   multi-statement `T_CALL` sites, stamps `T_CALL.e`, and registers the
   callee's locals as **mangled slots** (`_inl<N>_<name>`) in this
   function so they get allocated like real locals (see §9).
3. **B3 reference walk** — `cg_b3_walk` recurses the body tallying each
   local's reference count, tracking loop depth (a ref inside a loop
   weighs 1000×, the same currency the cost model uses), clearing
   `cg_fn_leaf` on any `T_CALL`, and filling the call-weight hash.
4. **Build the allocator's analysis** — `regalloc_build_cfg` →
   `regalloc_compute_liveness` → `regalloc_compute_rpo` →
   `regalloc_compute_intervals` (`bpp_regalloc.bsm`). This produces live
   intervals in RPO position units.
5. **B3 selection** — `call(p.b3_select)`: the chip ranks locals by
   reference count and writes the hottest into physical registers in
   `cg_var_promote[vi]` (or leaves −1 for a stack home). Hot *constants*
   are promoted the same way into `cg_const_reg`. A leaf function widens
   its pool into caller-saved registers.
6. **Linear-scan swap** — the interval-based allocator runs and, per pool
   (int, then float, which are disjoint physical register banks),
   *replaces* B3's picks in `cg_var_promote` — but only when three gates
   all clear: the systematic B3-vs-linear-scan comparison says it is not
   worse, no promoted mangled inline slot is left uncovered, and no
   variable needed live-range splitting. Any failure leaves B3's decision
   untouched (the always-correct fallback). This is why register sharing
   is a strict improvement: it never ships unless proven no worse.
7. **Reserve spill + frame size** — `fn_reserve_spill` lays out the
   spill/save area; the frame is rounded to 16 bytes.
8. **Prologue** — `emit_prologue_full` saves the callee-saved registers
   the allocation actually used, sets up the frame, and (for `main`)
   stashes argc/argv/envp.
9. **Emit the body** — `call(p.emit_body, body_arr, body_cnt)` walks the
   statements through `cg_emit_stmt` (§6). The epilogue is emitted at the
   `T_RET` sites and the fall-through end.

The takeaway: `cg_var_promote` is the contract between allocation
(steps 3–6) and emission (step 9). Everything after step 6 reads it;
nothing before step 5 exists yet.

---

## 6. Emitting a body — the two walkers

`emit_body` calls **`cg_emit_stmt(nd)`** per statement. It switches on
`ntype`:

- `T_IF` / `T_WHILE` / `T_SWITCH` — evaluate the condition, emit
  structured branches through the chip's label primitives; `T_WHILE`
  aligns the loop head to a 16-byte boundary first and pushes
  break/continue labels.
- `T_RET` — evaluate the return expression into the accumulator (or the
  multi-value register bank), emit the epilogue.
- `T_ASSIGN` — the busiest case. It tries, in order: a self-op immediate
  (`x = x + c`), then integer compute-in-place into a promoted target
  (§8), then float compute-in-place, and only if none apply falls back to
  evaluating the RHS into the accumulator (`emit_node`) and storing it
  (`emit_store_var_typed`).
- `T_MEMST` — `*(ptr) = val`, split by SIMD / scalar-float / int.
- an expression statement — evaluate for its side effects, discard.

Expressions go through **`cg_emit_node(nd)`**, which switches on the 8
expression tags. Its `T_BINOP` case is the accumulator machine: evaluate
the left into the accumulator, park it (in a freelist register if a
sibling has no call to clobber it, else push it on the value stack via
`emit_push_acc` / `emit_bank_pop`), evaluate the right, retrieve the
left, emit the operation. `T_CALL` is handled in the chip layer (the ABI
lives there); `cg_builtin_dispatch` intercepts recognized builtins
(`peek`/`poke`/`malloc`/`sys_*`/`vec_*`) and lowers them inline instead
of as calls.

`cg_depth` tracks value-stack depth; the freelist depth differs by ABI
(§7), which is why the same expression can park in a register on a64 and
spill to the stack on x64.

---

## 7. Where locals live — the per-local tables and the register pools

Keyed by variable index `vi`:

| Table | Written by | Meaning |
|-------|-----------|---------|
| `cg_var_promote[vi]` | B3 select + linear-scan swap | physical register, or −1 = stack home |
| `cg_var_off[vi]` | pre-register | frame offset for a stack home |
| `cg_var_stack[vi]` | pre-register/types | address-taken or struct (never a register leaf) |
| `cg_var_forced_ty[vi]` | types | slice width / float-ness (drives load/store width) |
| `cg_const_reg` | B3 const promotion | registers holding hot constants |

The **register pools** (the physical registers the selectors draw from):

- **Integer promotion (B3 / linear scan):** a64 `x19..x28` (10), widened
  to 14 with `x9..x12` in leaf functions; x64 `rbx/r12..r15` (5), plus
  `r8/r9` as a caller-saved band in non-leaf functions.
- **Expression freelist (B1):** the caller-saved scratch used to park a
  binop's left operand — a64 `x9..x15` (7 deep); x64 `r11` (1), widening
  to `r11+r8+r9` (3) in leaf functions. The shallow x64 freelist is why
  some compute-in-place shapes that need a temp are gated off on x64 (a
  throughput ceiling, not a correctness one).
- **Float promotion:** a64 callee-saved `d8..d15` (survive calls for
  free); x64 has no callee-saved XMM, so a promoted float is kept across
  a call only by saving it around the call site (the caller-saved wrap).

The `int_temp_alloc` / `int_temp_free` / `int_temp_count` primitives hand
out freelist registers; a spine gate that borrows a temp must check
`need <= count` before committing, because an encoder handed register −1
emits garbage.

---

## 8. Compute-in-place — skipping the accumulator

`cg_emit_int_into(nd, dreg)` and `cg_emit_float_into(nd, dreg)` emit an
arithmetic tree *directly into a destination register*, with no shuttle
through `x0`/`d0`. The left operand reuses `dreg` down the left spine;
each right operand takes a freelist temp; leaves are register-homed
locals (`cg_int_leaf_kind` returns the physical register), hot constants,
or memory loads. Fusions live here: integer `madd` (`(a*b)+c`), the float
`fmadd` (a64), and strength reduction (`x*2^k → x<<k`, and
`x*(2^k±1) →` shifted add/sub via `cg_const_shl_add`/`cg_const_shl_sub`).

The gate that lets an assignment use this path is
`cg_assign_into_dest_ok(rhs)`: it admits a single binop (or a `madd`
shape) whose left operand is a register leaf — so `dreg` is never written
before the final op has read its sources, which means `dreg` can safely
*be* one of those sources (`x = x*3 + y`). The right operand may be a
register, a constant, or a frame leaf; a `cg_int_tree_need <=
int_temp_count` check keeps a shallow freelist from over-committing. When
this fires, `a = b*c + d` becomes one instruction writing `a`'s register.
When it declines, the assignment falls back to the accumulator + store.

---

## 9. Register allocation — B3 and the linear scan

Two allocators cooperate through `cg_var_promote`:

- **B3** is a reference-count ranker: `cg_b3_walk` tallies uses
  (loop-weighted), the chip selector homes the highest-ranked locals into
  the promotion pool. It is greedy and has no notion of *when* a local is
  alive — so with more locals than registers it spills some even if they
  are never all live at once. B3 always runs and is always a valid
  answer.
- **The linear scan** (`bpp_regalloc.bsm`) is liveness-based: it builds a
  CFG, computes liveness, flattens live intervals, and assigns registers
  by scanning intervals in order, letting two locals with non-overlapping
  intervals *share* one register. Its result *replaces* B3's in
  `cg_var_promote` only when the three gates of `cg_emit_func` step 6
  clear (not worse, no uncovered mangled slot, no split). It also
  promotes locals into a caller-saved band in non-leaf functions and
  saves/restores them around each call — a cost model
  (`cg_caller_saved_worth`, shared in the spine) decides when that
  per-call save is worth it (`refs > 2× loop-weighted call count`), and a
  call-liveness mask lets a call skip band entries provably dead there.

The subtle invariant the gates protect: inlined callees' mangled slots
are pre-registered (step 2) and promoted by B3, but the interval builder
must see *all* their uses — a struct-pointer inline local that shows a
def but no field reads gets a degenerate single-point interval, and the
swap is refused on any such slot (`_rg_var_span_degenerate`) rather than
letting the scan free a still-live register.

---

## 10. The inliner — removing the call frame

Splicing a callee's body into its caller is driven from two places:

- **`bpp_dispatch.bsm`** classifies candidates (`classify_inlineable`)
  and pre-registers each accepted call site's locals as mangled slots via
  `_inline_pre_reg_walk` — this is what runs inside `cg_emit_func` step 2.
  Admission is bounded by a **register-pressure account**: a
  loop-carrying callee is spliced only while the caller's live slot count
  (its own locals plus every already-admitted splice's slots, which
  pre-registration tracks in `cg_vars`) plus the candidate's own need
  stays under `INLINE_PRESSURE_CAP` (14, the a64 non-leaf budget).
  Denial is registration-time and rides the `T_CALL.e == 0 → emit a real
  bl` convention, so registration and splice can never disagree.
- **`cg_emit_inline_multi`** (spine) performs the splice at emit time:
  parameters bind to the mangled slots, the body's statements are walked
  like the caller's own, and returns become assignments to the call's
  result slot. Trivial single-return callees take a faster path in the
  chip `T_CALL` handler; void/statement-context callees are detected at
  the last-statement check.

`BPP_INLINE_PROBE=1` prints the classifications and admit/deny decisions.

---

## 11. The rest of the optimization surface

- **Constant folding + immediate/strength selection (B0):** compile-time
  folding, `x*2^k → x<<k`, `x*(2^k±1) → shift+add/sub`, and immediate
  operand encoding — all in the spine's const/binop paths, both chips
  inheriting through primitives.
- **SIMD (B4):** `: double` 128-bit locals and the `vec_*` builtins (4×f32
  and 16×u8) dispatched by `cg_builtin_dispatch`, emitted as NEON (a64) /
  SSE2 (x64).
- **Outlining:** `synthesize_loop_fn` + `rewrite_dispatch_loops` in
  `bpp_dispatch.bsm` turn an eligible `@safe` loop that touches the caller
  frame into a parallel worker function, before per-chip emit, so both
  backends inherit it.
- **Autovec + loop alignment:** a loop pattern recognizer lifts eligible
  loops to `vec_*`; `emit_align_loop` pads loop heads to 16 bytes (a tight
  20 M-iteration loop can swing double digits purely on where its head
  lands).

---

## 12. The back of the pipeline — encoders and writers

A primitive like `_a64_emit_iop_into` ends in an encoder call
(`enc_add_reg`, `enc_lsl_imm`, …) that packs the instruction word into
`enc_buf` at `enc_pos`; the buffer grows geometrically. Branch and call
targets that aren't known yet are recorded as fixups and patched later
(`enc_patch32`), and cross-module calls are resolved after all modules
emit (`bo_resolve_calls_*`). The binary writer
(`a64_macho.bsm` / `x64_elf.bsm`) then consumes `enc_buf` plus the
string, float, and global tables and lays out the Mach-O or ELF file —
sections, relocations, symbol table, entry point.

---

## 13. Key structures at a glance

| Name | Where | What |
|------|-------|------|
| `Node` + `T_*` | parser / `bpp_defs.bsm` | the AST record; `ntype` selects the shape of `.a`–`.e` |
| packed ref | lexer / `bpp_internal.bsm` | `(offset<<32)\|len` into the source; `unpack_s`/`unpack_l`/`packed_eq` |
| `cg_prim` (`ChipPrimitives`) | spine + chips | the fn-ptr table the spine calls for every ISA atom |
| `cg_var_promote[vi]` | codegen | register for local `vi`, or −1; the allocation↔emission contract |
| `enc_buf` / `enc_pos` | `<arch>_enc.bsm` | the growable code byte buffer + write cursor |
| `cg_depth` | codegen | value-stack depth (accumulator spills) |
| `cg_fn_leaf` / `cg_fn_call_wt` | codegen | leaf flag + loop-weighted call count for this function |
| `cg_str_tbl` / `cg_flt_tbl` | codegen | literal tables the binary writer materializes |
| `mod_topo` | import | topologically sorted module indices |

---

## 14. "I need to change X — where do I look"

| Task | Start here |
|------|-----------|
| New operator / control-flow sugar | `bpp_parser.bsm` — lower it in the frontend; the backends never see it |
| New builtin (`peek_h`, `vec_add`, a syscall) | four files: `a64_codegen.bsm` + `x64_codegen.bsm` (emit) + `bpp_validate.bsm` (`val_is_builtin`) + `backend/c/bpp_emitter.bsm` (C). See bootstrap_manual "Adding a New Builtin". |
| A new optimization on an AST shape | the spine (`cg_emit_stmt` / `cg_emit_node` / `cg_emit_int_into`), plus a `ChipPrimitives` field if it needs an ISA op |
| A codegen-quality gap on one kernel | disassemble it (`bug --disasm`, `otool -tv` / `objdump`) and the `gcc -O2` oracle, find the AST shape, add a spine gate |
| Register allocation behavior | `bpp_regalloc.bsm` (intervals/scan) and the chip B3 selectors in `<arch>_codegen.bsm` |
| An inliner admission or splice bug | `bpp_dispatch.bsm` (`classify_inlineable`, `_inline_pre_reg_walk`, the pressure account) + `cg_emit_inline_multi`; `BPP_INLINE_PROBE=1` |
| An instruction encodes wrong | `<arch>_enc.bsm` — verify the bytes against the ISA, then a disasm A/B |
| A new diagnostic | `bpp_validate.bsm` (`E###`/`W###`), catalogued in `warning_error_log.md` |
| Binary format (relocations, sections) | `backend/target/*/` (`a64_macho.bsm` / `x64_elf.bsm`) |
| A `--c`-only divergence | `backend/c/bpp_emitter.bsm` |

---

## 15. File map (by weight)

```
src/bpp.bpp            main(): the pipeline
src/bpp_dispatch.bsm   classification, inliner pre-registration, outlining     (372 KB)
src/bpp_codegen.bsm    THE SPINE: cg_emit_func, cg_emit_stmt/node, CIP, splicer(277 KB)
src/bpp_parser.bsm     tokens -> AST; struct/func/global tables                (147 KB)
src/bpp_regalloc.bsm   CFG -> liveness -> intervals -> linear scan             (139 KB)
src/bpp_types.bsm      type inference, float/str propagation, `: float` promo  ( 76 KB)
src/bpp_validate.bsm   E###/W### diagnostics                                   ( 68 KB)
src/bpp_import.bsm     module graph, topo sort, auto-injection                 ( 61 KB)
src/bpp_lexer.bsm      tokenizer (packed refs)                                 ( 26 KB)
src/bpp_internal.bsm   shared structs + packed-ref/buf_eq helpers
src/bpp_defs.bsm       T_*, E###, slice-type constants; the Node struct

src/backend/chip/aarch64/   a64_codegen.bsm · a64_primitives.bsm · a64_enc.bsm
src/backend/chip/x86_64/    x64_codegen.bsm · x64_primitives.bsm · x64_enc.bsm
src/backend/target/         aarch64_macos/a64_macho.bsm · x86_64_linux/x64_elf.bsm
src/backend/os/             macos/ · linux/   (syscalls, startup, platform, audio)
src/backend/c/              bpp_emitter.bsm   (the --c path)

src/bug*.bsm / src/bug.bpp  the debugger — its own subsystem (see below)
```

The auto-injected runtime (`bpp_array`, `bpp_buf`, `bpp_hash`, `bpp_io`,
`bpp_math`, `bpp_mem`, `bpp_str`, `bpp_time`, `bpp_job`, `bpp_maestro`,
`brt0`) is compiled into every *program*, not part of the compiler
pipeline.

---

## 16. Working on it

The change ritual, in full, is `bootstrap_manual.md`. In short: edit,
build gen1 from `./bpp`, gen2 from gen1; a codegen change makes gen1 ≠
gen2 (normal), and **gen2 == gen3** is the stability criterion
(`tests/test_bootstrap_stable.sh`); never overwrite `./bpp` with an
untested binary; then `tests/run_all.sh` + `tests/run_all_c.sh`, the
benchmark catalog, the audio md5, and an x64 self-host under Docker.

The debugger `bug` (compile with `--bug`) is the truth source:
`bug --disasm <bin> <fn>` shows exactly what was emitted (disassemble
your output *and* the oracle before naming a gap), and
`bug --tui --watch <vars>` reads b++ variables by name at a breakpoint —
which is what cracks "silently wrong output" codegen bugs that a clean
exit code hides.

The measurement discipline (`benchmarks.md`): the oracle is `gcc -O2`;
measure the reference, not just our output; and on a latency-bound kernel
count critical-path latency, not instructions — an off-path instruction
cut is invisible on the clock because the out-of-order core hides it.

---

## 17. Further reading

- `bootstrap_manual.md` — the change ritual, the six-layer cake, adding a
  builtin/module.
- `backend_parity.md` — the a64/x64 capability table; what is permanent
  (the float family on x64) vs implementable.
- `tonify_checklist.md` — the idiom rules every new compiler function
  follows.
- `spine_analysis.md` — the design essay on what the spine is and what
  becoming a full optimizing IR would take.
- `journal.md` — the dated narrative behind every mechanism named here.
- `warning_error_log.md` — the `E###` / `W###` catalogue.
