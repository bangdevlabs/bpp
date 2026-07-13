# The B++ Compiler — How It Works

A precise map of how the B++ compiler is wired **as it stands now**: the
stages, the data that flows between them, each half traced function by
function, and where every mechanism lives. This is a present-tense wiring
diagram, not a history — for *why* a thing is the way it is, read
`journal.md`; for the *change ritual*, read `bootstrap_manual.md`; for
the two-CPU contract, read `backend_parity.md`.

Written for the reader who is about to open the compiler and needs to
know which function calls which, what each reads and writes, and which
layer a change belongs in.

---

## 1. The mental model

The compiler is a single process that turns source into a native binary
in one forward pass: **source → tokens → AST → types → classification →
machine code → binary**. There is no intermediate on-disk form and no
module cache; every run compiles everything from source (a full
self-compile is ~0.3 s).

The design has one organizing principle: **push every decision as early
in the pipeline as it can go**, so fewer stages carry its weight. The
parser folds constants and lowers `&&`/`||`, so no backend sees them. The
type pass writes inferred `: float` back into the source-level
declaration, so codegen just reads a type. The classification pass
decides purity and inlineability once, so both CPU backends inherit the
verdict. By the time codegen runs, most of the hard decisions are already
made and recorded on the AST or in side tables.

The code generator is **a floor plus layers** — this is the key thing to
get right, because the floor alone would be an unoptimized compiler and it
is emphatically *not* the whole story.

The **floor** is a stack + accumulator machine — the fallback that is
always available and always correct: an expression evaluates into one
accumulator (`x0`/`rax` for ints, `d0`/`xmm0` for floats), and operands
that must survive a subexpression push onto a **value stack**. The floor
by itself is roughly `gcc -O0`.

The **layers** sit on top and do the real work: register promotion,
compute-in-place (evaluate a tree straight into its destination register,
no accumulator), strength reduction, inlining, sharing one register
between two non-overlapping locals, `fmadd`/SIMD fusion. Each layer is a
*gate that proves it is safe to skip part of the accumulator round-trip*.
On a **tight inner loop the gates all fire at once** — the locals are
register-resident, the arithmetic computes in place and fuses, the calls
inline away — and the result reaches **`gcc -O2` parity** (this is where
the biquad / lcg / conv parity numbers come from). The floor only
resurfaces where a gate *declines* — a spilled local, an expression shape
no gate covers — so **`x0`/`rax` in a hot loop is exactly the tell that a
layer gave up there** (and the place to look for the next optimization).

Two honesty notes this framing forces. "`gcc -O2` parity" is a claim about
hot inner loops where every layer fires, not about cold or glue code
(which sits nearer `-O0`, and is not the bottleneck). And where a layer
simply does not exist yet — `manylive` needs algebraic reassociation the
compiler has no pass for — there is a real, measured gap; the benchmark
catalog shows both sides.

One binary targets two CPUs. Machine-independent decisions live in a
single **spine**; CPU-specific emission is reached through a **table of
function pointers** the two backends fill in. That is why most
optimizations land once and work on both a64 (ARM64/macOS) and x64
(x86-64/Linux).

---

## 2. The pipeline

`main()` in `src/bpp.bpp` runs the stages in order. After an `init_*`
block where every subsystem sets up its state — and each backend fills
the primitive table (`init_codegen_arm64` / `init_codegen_x86_64`) — the
flow is:

| Stage | Entry point(s) | File | Reads → Writes |
|-------|----------------|------|----------------|
| **Imports** | import resolution | `bpp_import.bsm` | source files → topologically sorted module list (`mod_topo`); auto-injects the runtime + (for programs) stb cartridges. Cycles fail. |
| **Lex** | tokenizer | `bpp_lexer.bsm` | source bytes → tokens as *packed refs* |
| **Parse** | `parse_program` | `bpp_parser.bsm` | tokens → the AST (`Node` records) + the struct/function/global tables |
| **Type inference** | `infer_module(mi)`, `infer_str_params` | `bpp_types.bsm` | AST → per-node/local types; inferred `: float` written back; the function signature tables |
| **Classify / transform** | `find_dispatch_candidates`, `synthesize_loop_fn`, `rewrite_dispatch_loops`, `run_dispatch` | `bpp_dispatch.bsm` | AST → effect classification (`@safe`), inline pre-registration, the outlining rewrite |
| **Validate** | `run_validate`, `diag_check_errors` | `bpp_validate.bsm`, `bpp_diag.bsm` | AST → `E###`/`W###`; aborts on any error |
| **Emit** | `emit_module_arm64/x86_64` → `cg_emit_func` | `bpp_codegen.bsm` + `backend/chip/<arch>/` | AST → machine code in the encoder buffer, one module at a time |
| **Resolve calls** | `bo_resolve_calls_arm64/_x64` | codegen | patch call sites once all addresses are known |
| **Write binary** | Mach-O / ELF writer | `backend/target/*/` | encoder buffer + tables → the output file |

`--c` replaces Emit/Resolve/Write with `backend/c/bpp_emitter.bsm`, a
second independent codegen path (the C compiler rejects what the native
backend silently accepts). Its suite is `tests/run_all_c.sh`.

---

## 3. The data that flows: the AST

The whole middle and back end operate on one record:

```
struct Node { ntype: byte, a, b, c, d, e, dispatch: byte, itype: quarter, src_tok }
```

`ntype` is a `T_*` tag (`bpp_defs.bsm`: `T_LIT=0, T_VAR=1, T_BINOP=2,
T_UNARY=3, T_ASSIGN=4, …`). The `.a`–`.e` slots hold operands, operator
chars, or target arrays — **meaning is per-`ntype`**, so every walker
switches on `ntype` first. Two families matter for codegen: expression
nodes that produce a value (`T_LIT`, `T_VAR`, `T_TERNARY`, `T_UNARY`,
`T_CALL`, `T_MEMLD`, `T_ADDR`, `T_BINOP`) and statement nodes with side
effects (`T_DECL`, `T_NOP`, `T_BLOCK`, `T_BREAK`, `T_CONTINUE`, `T_IF`,
`T_WHILE`, `T_RET`, `T_ASSIGN`, `T_MEMST`, `T_SWITCH`).

For a `T_BINOP`, `.a` packs the operator (read with `unpack_l` /
`unpack_s`), `.b` is left, `.c` right. `src_tok` maps the node back to its
source line (the debugger's line table); `itype`/`dispatch` are annotated
by later passes. Nodes are arena-allocated (`make_node` in
`bpp_internal.bsm`) so the AST is one bump-pointer region freed at once.

Alongside the AST, codegen keeps parallel per-local arrays keyed by a
variable index `vi` — the tables allocation writes and emission reads
(§10).

---

## 4. The front half: lex and parse

**Lex (`bpp_lexer.bsm`).** The tokenizer emits tokens as *packed refs* —
one 64-bit word `(offset<<32)|len` pointing into the shared source
buffer, with zero heap allocation per token. `unpack_s` / `unpack_l`
recover the slice; `packed_eq` compares two tokens' bytes directly. A
token also carries a kind (`TK_OP`, `TK_NUM`, `TK_ID`, …) the parser
switches on.

**Parse (`bpp_parser.bsm`).** `parse_program` is the top loop: it reads
top-level declarations and calls `parse_function(is_stub, is_static,
is_void)` for each, which parses the signature and then the body's
statements (if / while / for, assignments, returns, blocks) into `Node`
arrays. Statements register their locals as `T_DECL` nodes; the function
record stores its parameter, body, and return-arity metadata.

Expressions parse by **precedence climbing (Pratt)**: `parse_binops(left,
min_prec)` loops while the next operator's precedence (`get_op_prec` —
`||`=1 … `*`/`%`=10) is at least `min_prec`, recursing for higher-binding
right operands. `maybe_ternary` wraps the result in a right-associative
`?:` below every binary operator. Two **canonical rewrites happen here,
in the frontend, on purpose**:

1. **Constant folding** — `lit op lit` collapses to a single literal, so
   no backend folds integer constants.
2. **Short-circuit lowering** — `a && b` and `a || b` become `T_TERNARY`,
   so no backend ever sees `&&`/`||` and short-circuit semantics are
   identical everywhere.

`make_node(T_x)` bump-allocates a zeroed `Node` and stamps `src_tok`. The
struct/function/global tables are hash-backed for O(1) lookup;
`add_struct_def` / `add_struct_field` precompute each field's *byte*
offset at definition time (the sliced-struct layout — bit/byte/quarter/
half packing), so a `.field` access later is a direct load with no
runtime layout math.

The output of this half is a complete but untyped, unclassified AST plus
the symbol tables.

---

## 5. Type inference — `infer_module(mi)`

Runs per module. It first rebuilds the function-name hash, then registers
**extern return types** (a signature ending `double` → `TY_FLOAT`,
`float` → `TY_FLOAT_H`, else `TY_WORD`) so calls into other modules know
their result width. It then walks each body inferring a type for every
node and local, propagating float-ness through arithmetic (a float leaf
makes its binop float; the compiler chooses int vs float instructions
from this).

The load-bearing mechanism is **the compiler writing annotations back**:
a local that is inferred to hold a float has `: float` written into its
own declaration (unless the programmer locked it with an explicit hint).
That single fact is what both codegen (float register homes, `d`-register
CIP) and dispatch read downstream — the type is on the declaration, not
re-derived at every use. A second pass, `infer_str_params`, settles
string-parameter types by consensus across all call sites (a parameter
used as a string at any call becomes a str parameter everywhere).

The products consumed later are the per-local types (`cg_var_forced_ty`
via `ty_set_var_type`) and the function signature tables (`fn_ret_types`
/ `fn_par_types`, read through `get_fn_ret_type` / `get_fn_par_type`),
which drive type-correct loads/stores and the float-vs-int codegen split.

---

## 6. Classification and transforms — `bpp_dispatch.bsm`

The largest file. It does three things to the typed AST before codegen.

**Effect classification (the purity lattice).** Every function gets a
`PHASE_*` effect in `fn_effect`: `PHASE_BASE` (pure — no global writes, no
impure calls) or `PHASE_SOLO` (side effects, direct or transitive). It is
a height-1 lattice (BASE → SOLO), computed as a fixpoint over the call
graph: builtins are seeded with known effects (`add_extern_effect`:
`peek` → BASE, an unregistered extern → SOLO as the safe catch-all), and
the pass propagates upward until stable. This drives two things:
`@safe` enforcement — a function marked `@safe` must reach only BASE
through its transitive call graph, or **W026** fires (this is the "an
audio callback provably never mallocs" guarantee) — and the internal
AUTO/BASE/SOLO decisions the inliner and outliner consult.

**Inline candidate selection (`classify_inlineable`).** Decides which
callees may be spliced. Gates include: the body must end in a `T_RET` for
the value path (so the splice has a result to bind); a node/parameter cost
budget; a float parameter forces a higher tier (it needs a real
conversion); a void candidate is rejected if any statement is a `T_RET`.
Accepted call sites are pre-registered later, inside `cg_emit_func`, via
`_inline_pre_reg_walk` (§13). It runs to a fixpoint (a splice can expose
the next candidate).

**Outlining (auto-parallelization).** `find_dispatch_candidates` finds
`@safe` loops eligible to run in parallel; `synthesize_loop_fn` builds a
worker function from the loop body; `rewrite_dispatch_loops` rewrites the
loop into a parallel dispatch over the worker (real threads on macOS,
serial-correct on Linux until ELF threads ship). This is a source-to-
source AST transform that runs *before* per-chip emit, so both backends
inherit it for free. `run_dispatch` then makes the per-loop dispatch
decisions per function (`analyze_body`), skipping cached bodies.

After this stage the AST is typed, classified, and rewritten; `run_validate`
emits the diagnostics and `diag_check_errors` aborts on any error.

---

## 7. The spine and the chip-primitive table

Three concepts carry the two-backend design:

- **The spine — `src/bpp_codegen.bsm`.** Owns every AST-shape codegen
  decision: the per-function driver (`cg_emit_func`), the statement walker
  (`cg_emit_stmt`), the expression walker (`cg_emit_node`), the
  compute-in-place walkers (`cg_emit_int_into` / `cg_emit_float_into`),
  builtin lowering (`cg_builtin_dispatch`), and the inline splicer
  (`cg_emit_inline_multi`). It decides *what* to emit.

- **`ChipPrimitives` — the table, held in `cg_prim`.** A struct of
  function pointers: the ISA-atomic operations (`emit_add`,
  `emit_iop_into`, `emit_ishl_imm_into`, `emit_store_var_typed`,
  `emit_push_acc`, `emit_bank_pop`, `int_temp_alloc`, `b3_select`,
  `regalloc_apply`, `emit_prologue_full`, `cur_text_pos`, …). The spine
  calls `call(pp.emit_add, …)` and never knows which CPU answered. Each
  backend installs its pointers at init.

- **The three-file chip split** under `backend/chip/<arch>/`:
  `<arch>_codegen.bsm` installs the primitives and owns the genuinely
  CPU-aware pieces (the `T_CALL` handler — the call ABI and inline gating
  differ — the prologue/epilogue, the register selectors);
  `<arch>_primitives.bsm` holds the ISA-atomic emit functions;
  `<arch>_enc.bsm` is the only place that knows bit positions, packing
  instruction words into `enc_buf` at `enc_pos`.

**Where a change goes:** if the `bpp` binary needs *both* variants linked,
it is a spine decision (with a `ChipPrimitives` field if it needs a new
ISA op); one instruction both CPUs need is a new primitive implemented per
chip; ABI/encoding specifics are a chip file. Concrete: recognizing that
`x*3` can be a shifted add is a spine decision (`cg_const_shl_add`);
emitting `add xd,xn,xn,lsl #k` vs `lea` is a chip primitive.

---

## 8. The per-function driver — `cg_emit_func(fi)`

The center of the back end. `emit_module_*` loops over a module's
functions and calls `cg_emit_func(fi)` for each; the source numbers its
own steps:

1. **Bind parameters** into the per-local tables (`cg_var_add`) with any
   declared hints.
2. **Pre-register body locals** — `fn_pre_reg_vars` walks the body,
   pre-declares every `auto` (including block-nested) and assigns frame
   offsets, and in the same pass stamps inlineable multi-statement
   `T_CALL` sites (`T_CALL.e`) and registers each accepted callee's locals
   as **mangled slots** (`_inl<N>_<name>`) in this function (§13).
3. **B3 reference walk** — `cg_b3_walk` recurses the body tallying each
   local's reference count (a use inside a loop weighs 1000×), tracking
   `cg_fn_leaf` (cleared by any `T_CALL`), and filling the call-weight
   hash.
4. **Build the allocator analysis** — `regalloc_build_cfg` →
   `regalloc_compute_liveness` → `regalloc_compute_rpo` →
   `regalloc_compute_intervals` (`bpp_regalloc.bsm`): live intervals in
   RPO position units.
5. **B3 selection** — `call(p.b3_select)`: the chip ranks locals by
   reference count and writes the hottest into physical registers in
   `cg_var_promote[vi]` (−1 = stack). Hot *constants* go the same way into
   `cg_const_reg`. A leaf function widens its pool into caller-saved
   registers.
6. **Linear-scan swap** — the interval allocator runs and, per pool (int,
   then float — disjoint register banks), *replaces* B3's picks in
   `cg_var_promote`, but only when three gates clear: the systematic
   B3-vs-scan comparison says not worse, no promoted mangled inline slot
   is left uncovered, and no variable needed live-range splitting. Any
   failure leaves B3 untouched. This is why register sharing never
   regresses: it ships only when proven no worse.
7. **Reserve spill + frame** — `fn_reserve_spill` lays out the save area;
   the frame rounds to 16 bytes.
8. **Prologue** — `emit_prologue_full` saves the callee-saved registers
   the allocation actually used (and, for `main`, stashes argc/argv/envp).
9. **Emit the body** — `call(p.emit_body, …)` walks statements through
   `cg_emit_stmt` (§9). Epilogues are emitted at the `T_RET` sites.

`cg_var_promote` is the contract between allocation (steps 3–6) and
emission (step 9): everything after step 6 reads it, nothing before step
5 exists.

---

## 9. Emitting a body — the two walkers

`emit_body` calls **`cg_emit_stmt(nd)`** per statement, switching on
`ntype`: `T_IF`/`T_WHILE`/`T_SWITCH` evaluate a condition and emit
structured branches through the chip label primitives (`T_WHILE` aligns
the loop head to 16 bytes and pushes break/continue labels); `T_RET`
evaluates into the accumulator (or the multi-value bank) and emits the
epilogue; `T_MEMST` is a store through a pointer, split by SIMD /
scalar-float / int. **`T_ASSIGN`** is the busiest case — it tries, in
order: a self-op immediate (`x = x + c`), integer compute-in-place into a
promoted target (§11), float compute-in-place, and only if none apply
falls back to evaluating the RHS into the accumulator (`emit_node`) and
storing it (`emit_store_var_typed`).

Expressions go through **`cg_emit_node(nd)`** over the 8 expression tags.
Its `T_BINOP` case is the accumulator machine: evaluate the left into the
accumulator, park it (in a freelist register if a sibling has no call to
clobber it, else push it on the value stack with `emit_push_acc` /
`emit_bank_pop`), evaluate the right, retrieve the left, emit the op.
`T_CALL` lives in the chip layer (the ABI is there); `cg_builtin_dispatch`
intercepts recognized builtins (`peek`/`poke`/`malloc`/`sys_*`/`vec_*`)
and lowers them inline. `cg_depth` tracks value-stack depth.

---

## 10. Where locals live — the per-local tables and register pools

Keyed by variable index `vi`:

| Table | Written by | Meaning |
|-------|-----------|---------|
| `cg_var_promote[vi]` | B3 + linear-scan swap | physical register, or −1 = stack home |
| `cg_var_off[vi]` | pre-register | frame offset for a stack home |
| `cg_var_stack[vi]` | pre-register/types | address-taken or struct (never a register leaf) |
| `cg_var_forced_ty[vi]` | types | slice width / float-ness (load/store width) |
| `cg_const_reg` | B3 const promotion | registers holding hot constants |

Register pools:
- **Integer promotion:** a64 `x19..x28` (10), widened to 14 with
  `x9..x12` in leaf functions; x64 `rbx/r12..r15` (5), plus `r8/r9` as a
  caller-saved band in non-leaf functions.
- **Expression freelist (B1):** a64 `x9..x15` (7); x64 `r11` (1),
  widening to `r11+r8+r9` (3) in leaf functions. The shallow x64 freelist
  is why some CIP shapes that need a temp are gated off there.
- **Float promotion:** a64 callee-saved `d8..d15` (survive calls free);
  x64 has no callee-saved XMM, so a promoted float survives a call only by
  a save/restore wrap around the call site.

`int_temp_alloc`/`int_temp_free`/`int_temp_count` hand out freelist
registers; a spine gate that borrows a temp must check `need <= count`
before committing — an encoder handed register −1 emits garbage.

---

## 11. Compute-in-place — skipping the accumulator

`cg_emit_int_into(nd, dreg)` / `cg_emit_float_into(nd, dreg)` emit an
arithmetic tree straight into a destination register with no shuttle
through `x0`/`d0`. The left operand reuses `dreg` down the left spine;
each right operand takes a freelist temp; leaves are register-homed
locals (`cg_int_leaf_kind` returns the register), hot constants, or
memory loads. Fusions live here: integer `madd`, float `fmadd` (a64), and
strength reduction (`x*2^k → x<<k`, `x*(2^k±1) →` shifted add/sub via
`cg_const_shl_add`/`cg_const_shl_sub`).

The gate `cg_assign_into_dest_ok(rhs)` admits a single binop (or `madd`)
whose *left* operand is a register leaf — so `dreg` is never written
before the final op reads its sources, which means `dreg` can safely *be*
one of those sources (`x = x*3 + y`). The right operand may be a register,
constant, or frame leaf; a `cg_int_tree_need <= int_temp_count` check
keeps a shallow freelist from over-committing. When it fires, `a = b*c +
d` becomes one instruction into `a`'s register; when it declines, the
assignment falls back to accumulator + store.

---

## 12. Register allocation — B3 and the linear scan

Two allocators cooperate through `cg_var_promote`:

- **B3** is a reference-count ranker (`cg_b3_walk` tallies loop-weighted
  uses; the chip selector homes the top-ranked locals). Greedy, with no
  notion of *when* a local is alive, so with more locals than registers it
  spills some even if they are never all live at once. Always runs, always
  valid.
- **The linear scan** (`bpp_regalloc.bsm`) is liveness-based, letting two
  locals with non-overlapping live ranges *share* one register. Its result
  replaces B3's only when the three gates of §8 step 6 clear. It also
  promotes into a caller-saved band in non-leaf functions and
  saves/restores across each call — a cost model (`cg_caller_saved_worth`,
  shared in the spine) decides when that per-call save pays (`refs > 2×
  loop-weighted call count`), and a call-liveness mask skips band entries
  provably dead at a call.

The scan is five passes, all in `bpp_regalloc.bsm`, run from
`cg_emit_func` step 4:

1. **`regalloc_build_cfg`** walks the body into `CfgBlock` records:
   control flow (`T_IF`/`T_WHILE`) splits blocks and wires successor edges
   (break/continue via stacks), and inline call sites are *expanded in
   place* — the callee's statements are cloned with its locals translated
   to the caller's mangled slots (`_rg_clone_translate`), so the CFG sees
   inlined code exactly as it will be emitted.
2. **`regalloc_compute_liveness`** first computes each block's own
   use/def bitsets with kill-then-gen sequencing (a read is a block-level
   *use* only if no earlier statement in the block already defined that
   variable), then solves backward dataflow to a fixpoint:
   `live_out = ∪ successors' live_in`, `live_in = use ∪ (live_out & ~def)`,
   looping until nothing changes. Variables are tracked as bits in one
   64-bit word — **so a function with more than 64 locals falls back to
   B3** (the scan simply doesn't cover it).
3. **`regalloc_compute_rpo`** orders blocks in reverse post-order, giving
   each statement a global *position* faithful to emission order.
4. **`regalloc_compute_intervals`** turns liveness into `LiveInterval`
   records: for each block (in RPO), a backward pass reconstructs each
   statement's "live-after" set from `live_out`, then a forward pass
   *touches* every live variable at its own global position, opening and
   extending intervals. A variable alive in two disjoint stretches gets
   two sub-intervals (a *split*).
5. **`regalloc_linear_scan(intervals, budget, refs)`** sorts intervals by
   start position and walks them: **expire** active entries whose end has
   passed (freeing their slot), then **allocate** a free slot or, if none,
   **spill** the worst active interval (ranked by reference count and end
   position). Split variables are handled by holding a slot through the
   whole *envelope* (max end across the variable's sub-intervals) and a
   `var_decided` cache so every sub-interval of one variable reaches the
   same slot. Output is `RegAssign` records (var_idx → slot, or −1 =
   spill), which the chip's `regalloc_apply` maps to physical registers
   (`_a64_b3_reg_at`) and writes into `cg_var_promote`.

The invariant the swap gates protect: inlined mangled slots are
pre-registered and B3-promoted, but the interval builder must see *all*
their uses — a struct-pointer inline local showing a def but no field
reads gets a degenerate single-point interval, and the swap is refused on
any such slot (`_rg_var_span_degenerate`) rather than freeing a
still-live register.

---

## 13. The inliner — removing the call frame

Splicing is driven from two places. **`bpp_dispatch.bsm`** selects
candidates (`classify_inlineable`, §6) and pre-registers each accepted
call site's locals as mangled slots via `_inline_pre_reg_walk` — the pass
that runs inside `cg_emit_func` step 2. Admission is bounded by a
**register-pressure account**: a loop-carrying callee is spliced only
while the caller's live slot count (its own locals plus every
already-admitted splice's slots, tracked in `cg_vars`) plus the
candidate's need stays under `INLINE_PRESSURE_CAP` (14, the a64 non-leaf
budget). Denial is registration-time and rides the `T_CALL.e == 0 → emit
a real bl` convention, so registration and splice can never disagree.

**`cg_emit_inline_multi`** (spine) performs the splice at emit time:
parameters bind to the mangled slots, the body's statements are walked
like the caller's own, and returns become assignments to the call's
result slot. Trivial single-return callees take a faster path in the chip
`T_CALL` handler; void callees are detected at the last-statement check.
`BPP_INLINE_PROBE=1` prints the classifications and admit/deny decisions.

---

## 14. The rest of the optimization surface

- **Constant folding + immediate/strength selection:** compile-time
  folding (in the parser), `x*2^k → x<<k`, `x*(2^k±1) → shift+add/sub`,
  immediate operand encoding — the spine const/binop paths, both chips
  inheriting through primitives.
- **SIMD:** `: double` 128-bit locals and the `vec_*` builtins (4×f32,
  16×u8) via `cg_builtin_dispatch`, emitted NEON (a64) / SSE2 (x64).
- **Autovec + loop alignment:** a loop pattern recognizer lifts eligible
  loops to `vec_*`; `emit_align_loop` pads loop heads to 16 bytes (a tight
  20 M-iteration loop swings double digits on head placement alone).

---

## 15. The back of the pipeline — encoders and writers

**Encoders.** A primitive like `_a64_emit_iop_into` ends in an encoder
call (`enc_add_reg`, `enc_lsl_imm`, …) that packs the instruction into the
code buffer `enc_buf` at `enc_pos`; the buffer grows geometrically.
Targets not yet known — forward branches, calls — are recorded as fixups
and patched once their label resolves (`enc_patch32`); cross-module calls
are resolved after every module has emitted (`bo_resolve_calls_*`), and
external (FFI) references are recorded as relocations against a GOT.

**The Mach-O writer** (`backend/target/aarch64_macos/a64_macho.bsm`,
`write_macho(filename, main_label, gl_names, gl_count)`): it takes
`code_size = enc_pos`, calls `mo_compute_layout` to assign file offsets
and virtual addresses to every section, sets the entry point to `_main`'s
offset, and resolves relocations now that addresses are known
(`mo_resolve_relocations` patches the ADRP+ADD pairs that reference
globals/strings/floats and the GOT). It then writes the header and load
commands: `__PAGEZERO` (a 4 GB unmapped guard at address 0), `__TEXT`
(header + load commands + `__text` code, at base `0x1_0000_0000`),
`__DATA` (the `__data` section = globals + floats + strings + GOT, plus a
`__minisym` blob the runtime reads for symbolication), and `__LINKEDIT`;
then `LC_SYMTAB` (the `nlist_64` symbol table), `LC_MAIN` (the entry
offset), one `LC_LOAD_DYLIB` per linked library (`libSystem` is ordinal
1), `LC_UUID` (carrying the build id so the `bug` debugger can match a
binary to its `.bug` map), and chained-fixups commands. A SHA-256 pass
(`sha256`, implemented in-file) fills the code-signature slot so macOS
will run the binary unsigned-but-adhoc.

**The ELF writer** (`backend/target/x86_64_linux/x64_elf.bsm`,
`write_elf` / `write_elf_dyn`): base address `0x400000`; the file is the
ELF header + program headers, then a `PT_NOTE` region (always present —
it carries the build id), then the code and data. It emits two or three
program headers: `PT_LOAD` for text (`r-x`), `PT_LOAD` for data (`rw-`,
only if there is any), and the note. `e_entry` is the entry virtual
address. A purely static program uses `write_elf`; a program with FFI
externs uses `write_elf_dyn`, which adds the dynamic-linking machinery
(PLT/GOT, `.dynamic`, `.dynsym`, relocations) so `ld.so` resolves the
external symbols at load. Both writers consume the same `enc_buf` and the
same string/float/global tables — only the container format differs,
which is the whole point of keeping code emission (§8–14) format-agnostic.

---

## 16. Key structures at a glance

| Name | Where | What |
|------|-------|------|
| `Node` + `T_*` | parser / `bpp_defs.bsm` | the AST record; `ntype` selects the shape of `.a`–`.e` |
| packed ref | lexer / `bpp_internal.bsm` | `(offset<<32)\|len` into the source; `unpack_s`/`unpack_l`/`packed_eq` |
| `fn_effect` | dispatch | per-function purity (`PHASE_BASE`/`PHASE_SOLO`), the `@safe` basis |
| `fn_ret_types` / `fn_par_types` | types | function signature tables |
| `cg_prim` (`ChipPrimitives`) | spine + chips | the fn-ptr table the spine calls for every ISA atom |
| `cg_var_promote[vi]` | codegen | register for local `vi`, or −1; the allocation↔emission contract |
| `enc_buf` / `enc_pos` | `<arch>_enc.bsm` | the growable code byte buffer + write cursor |
| `cg_depth` | codegen | value-stack depth (accumulator spills) |
| `cg_fn_leaf` / `cg_fn_call_wt` | codegen | leaf flag + loop-weighted call count for this function |
| `mod_topo` | import | topologically sorted module indices |

---

## 17. "I need to change X — where do I look"

| Task | Start here |
|------|-----------|
| New operator / control-flow sugar | `bpp_parser.bsm` — lower it in the frontend; backends never see it |
| New builtin (`peek_h`, `vec_add`, a syscall) | four files: `a64_codegen.bsm` + `x64_codegen.bsm` (emit) + `bpp_validate.bsm` (`val_is_builtin`) + `backend/c/bpp_emitter.bsm` (C) |
| Type inference / a `: float` propagation bug | `bpp_types.bsm` (`infer_module`, `infer_str_params`) |
| Purity / `@safe` (W026) / inline eligibility | `bpp_dispatch.bsm` (`fn_effect` fixpoint, `classify_inlineable`) |
| A new optimization on an AST shape | the spine (`cg_emit_stmt` / `cg_emit_node` / `cg_emit_int_into`), + a `ChipPrimitives` field if it needs an ISA op |
| A codegen-quality gap on one kernel | disassemble it (`bug --disasm`, `otool -tv`/`objdump`) and the `gcc -O2` oracle, find the AST shape, add a spine gate |
| Register allocation behavior | `bpp_regalloc.bsm` (intervals/scan) + the chip B3 selectors in `<arch>_codegen.bsm` |
| An inliner admission / splice bug | `bpp_dispatch.bsm` (`_inline_pre_reg_walk`, pressure account) + `cg_emit_inline_multi`; `BPP_INLINE_PROBE=1` |
| An instruction encodes wrong | `<arch>_enc.bsm` — verify bytes against the ISA, then a disasm A/B |
| A new diagnostic | `bpp_validate.bsm` (`E###`/`W###`), catalogued in `warning_error_log.md` |
| Binary format (relocations, sections) | `backend/target/*/` |
| A `--c`-only divergence | `backend/c/bpp_emitter.bsm` |

---

## 18. File map (by weight)

```
src/bpp.bpp            main(): the pipeline
src/bpp_dispatch.bsm   effect classification, inliner selection, outlining      (372 KB)
src/bpp_codegen.bsm    THE SPINE: cg_emit_func, cg_emit_stmt/node, CIP, splicer (277 KB)
src/bpp_parser.bsm     tokens -> AST (Pratt); struct/func/global tables         (147 KB)
src/bpp_regalloc.bsm   CFG -> liveness -> intervals -> linear scan              (139 KB)
src/bpp_types.bsm      type inference, float/str propagation, `: float` write-back(76 KB)
src/bpp_validate.bsm   E###/W### diagnostics                                    ( 68 KB)
src/bpp_import.bsm     module graph, topo sort, auto-injection                  ( 61 KB)
src/bpp_lexer.bsm      tokenizer (packed refs)                                  ( 26 KB)
src/bpp_internal.bsm   make_node, shared structs, packed-ref/buf_eq helpers
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

## 19. Working on it

Full ritual: `bootstrap_manual.md`. In short: edit; build gen1 from
`./bpp`, gen2 from gen1; a codegen change makes gen1 ≠ gen2 (normal), and
**gen2 == gen3** is the stability criterion
(`tests/test_bootstrap_stable.sh`); never overwrite `./bpp` with an
untested binary; then `tests/run_all.sh` + `tests/run_all_c.sh`, the
benchmark catalog, the audio md5, and an x64 self-host under Docker.

The debugger `bug` (compile with `--bug`) is the truth source:
`bug --disasm <bin> <fn>` shows exactly what was emitted (disassemble your
output *and* the oracle before naming a gap); `bug --tui --watch <vars>`
reads b++ variables by name at a breakpoint — which cracks "silently
wrong output" bugs a clean exit code hides.

The measurement discipline (`benchmarks.md`): the oracle is `gcc -O2`;
measure the reference, not just our output; on a latency-bound kernel
count critical-path latency, not instructions.

---

## 20. Further reading

- `bootstrap_manual.md` — the change ritual, the six-layer cake, adding a
  builtin/module.
- `backend_parity.md` — the a64/x64 capability table; permanent gaps (the
  float family on x64) vs implementable.
- `tonify_checklist.md` — the idiom rules every new compiler function
  follows.
- `spine_analysis.md` — the design essay on what the spine is.
- `journal.md` — the dated narrative behind every mechanism named here.
- `warning_error_log.md` — the `E###` / `W###` catalogue.
