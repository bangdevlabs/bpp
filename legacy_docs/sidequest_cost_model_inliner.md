# Sidequest — Cost-model inliner (S4)

**Status:** IN PROGRESS 2026-05-21. P0a + P1 + P2 + P3a
shipped (loop_depth + cost function + threshold function +
unified classifier). P3b next.

**Target backend:** **ARM64 only.** The x86_64 backend has had
Phase B2 disabled since 2026-04-15 (`19da538`) pending
diagnosis of an emission bug under self-host; a separate
Linux x86_64 runtime regression surfaced 2026-05-21 during S4
discovery (`bpp_lin` segfaults at startup). Both feed into the
"Linux x86_64 health restoration" sidequest in `docs/todo.md`.
S4 ships ARM64-only until that arc closes — at which point
both Phase B2 and S4's multi-statement splice activate on
x86_64 in one go. The classifier (P3a) is chip-neutral; only
the per-chip splice point in `*/codegen.bsm` files diverges.

**Trigger:** Hot-path opt arc S1→S3k (closed 2026-05-21) shipped
~41% bootstrap improvement via mechanical `cg_builtin_dispatch`
inlining of trivial-body functions. The remaining wins on
real-game workloads need a generalised inliner with a cost
model — the dispatch lane has no shared register state across
primitive calls, so anything beyond a single arithmetic op
loses to stack-juggling overhead (see S3g revert in the
hot-path arc closure doc, now archived under `legacy_docs/`).

**Benchmark anchor:** `examples/tablah.bpp` (clean API) ~49ms
vs `examples/tablah_opt.bpp` (hand-unrolled xorshift64) ~40ms
= **~18% gap** that a real inliner could close. xorshift64 is
the canonical shape this sidequest targets: 7 statements, two
local mutations, one global rng_state mutation, no calls.

---

## Foundation — this is NOT from scratch

B++ already has a working inliner: **Phase B2**, in
`src/bpp_dispatch.bsm:3720-3905` (~185 LOC). It handles the
trivial single-expression case:

| Phase B2 file | What it does |
|---|---|
| `classify_inlineable()` (line 3807) | Marks `fn_inlineable[i] = 1` for eligible callees |
| `_inline_count_nodes()` (line 3740) | AST node count with hard threshold of 5 |
| `_inline_has_tcall()` (line 3763) | Rejects bodies containing any T_CALL |
| `_inline_param_refs()` (line 3782) | Rejects functions with params used more than once |
| `ast_clone_subst()` (line 3861) | Clones expression substituting params with arg nodes |
| `a64_codegen.bsm:1200-1212` + x64 equiv | Splice point: 12 lines per chip |

**Existing eligibility criteria (all must hold):**

1. `fn_phase[i] == PHASE_BASE` (pure by call-graph fixpoint)
2. `body_cnt == 1` and `body[0]` is a `T_RET`
3. `_inline_count_nodes(ret_expr) ≤ 5`
4. `param_cnt ≤ 3`
5. Zero `T_CALL` in body
6. Each param referenced at most once
7. No floats (return type or any param)

**S4 = generalisation of Phase B2**, reusing every piece of
the existing infrastructure. The work is extending — not
inventing.

---

## Industry research — what mature compilers do

Five reference points surveyed before locking the design.

### LLVM/Clang — context-adjusted thresholds

LLVM's `InlineCost` analysis assigns each call site its own
threshold based on context: `OptSizeThreshold = 50` for `-Os`,
default ~225 at `-O2`, ~375 at `-O3`. Separate thresholds
for cold call sites (lower), hot call sites (higher), and
callees with inline hints. Per-instruction cost in "points";
the decision is `cost ≤ threshold(callsite)`.

Source: [LLVM InlineCost.h](https://llvm.org/doxygen/InlineCost_8h_source.html),
[InlineCost.cpp](https://llvm.org/doxygen/InlineCost_8cpp_source.html).

### GCC — two-tier caps (per-function + per-binary)

GCC has two independent caps:

1. **`max-inline-insns-single = 500`** — per-function ceiling
   for `inline`-keyword candidates.
2. **`inline-unit-growth = 20%`** — total translation-unit
   growth ceiling. Once the binary has grown 20%, no more
   inlining regardless of individual candidate cost.

Plus `large-unit-insns = 10000` as a floor (small TUs treated
as if they were 10k before applying growth — protects small
modules from disproportional impact).

Source: [GCC Optimize Options](https://gcc.gnu.org/onlinedocs/gcc-4.2.4/gcc/Optimize-Options.html),
[Jan Hubicka — Increase inline-unit-growth to 20%](https://gcc.gnu.org/legacy-ml/gcc-patches/2015-04/msg00160.html).

### Rust MIR — bonuses AND penalties

The MIR `CostChecker` is the newest evolution. Instead of just
counting costs, it has **bonuses** that subtract from cost
when the inline enables further optimisation:

- `switchInt(const)` arg → bonus (branch eliminated after
  monomorphisation)
- `Unreachable` terminator → bonus (dead branch)
- runtime `memcpy` → large cost
- baseline cost per op (even "free" things consume compile
  time)

Plus three annotation levels: `#[inline]` (hint),
`#[inline(always)]` (force), `#[inline(never)]` (block); RFC
3711 adds `#[inline(required)]`.

Source: [rustc Inliner](https://doc.rust-lang.org/nightly/nightly-rustc/rustc_mir_transform/inline/struct.Inliner.html),
[Rust PR #123179](https://github.com/rust-lang/rust/pull/123179),
[Rust PR #126578](https://github.com/rust-lang/rust/pull/126578),
[Rust RFC #3711](https://github.com/rust-lang/rfcs/pull/3711).

### Jai — programmer-driven, no cost model

`#inline` forces, `#no_inline` blocks. No automatic
heuristic. Philosophy: give the programmer control over
low-level performance details, do not pretend the compiler
knows better than a careful human.

Source: [JaiPrimer (BSVino)](https://github.com/BSVino/JaiPrimer/blob/master/JaiPrimer.md).

### Zig — `inline fn` tied to comptime

`inline fn` forces inline. In stage2, semantics interleave
with comptime: args known at the call site become comptime
within the instantiation. No independent cost model — relies
on LLVM backend.

Source: [Zig PR #7647 (stage2 comptime + inline)](https://github.com/ziglang/zig/pull/7647),
[Loris Cro — What is Comptime?](https://kristoff.it/blog/what-is-zig-comptime/).

### Universal patterns

Every mature compiler ships:

| Pattern | LLVM | GCC | Rust | Jai | Zig |
|---|:-:|:-:|:-:|:-:|:-:|
| Cost model with bonuses + penalties | ✓ | ✓ (penalties only) | ✓ (newest) | – | – |
| Per-callsite threshold adjustment | ✓ | – | ✓ | – | – |
| Per-function cap | ✓ | ✓ | ✓ | – | – |
| Per-binary growth cap | – | ✓ | – | – | – |
| `@inline` (force) annotation | ✓ | ✓ | ✓ | ✓ | ✓ | (B++ skips — Rule 4) |
| `@no_inline` (block) annotation | ✓ | ✓ | ✓ | ✓ | ✓ | (B++ skips — default) |
| PGO integration | ✓ | ✓ | ✓ | – | – |

**B++ S4 adopts:** cost model w/ bonuses + penalties, per-
callsite threshold adjustment, both caps (per-function +
per-binary).

**B++ S4 deliberately deviates on the universal `@inline` /
`@no_inline` pattern.** Tonify Rule 4 (the 2026-05-11
Multics-drift collapse) reduced B++'s function-level
annotations to exactly `@safe` (compiler-verified safety
contract) + statement-level `@profile` (instrumentation
metadata). Rule 4 + Rule 28 require annotations to "earn
their keep" by catching a specific bug class via compiler
verification — `@inline` is a perf hint, not a contract,
and `@no_inline` is the default (anything the cost model
rejects stays a normal call). Adding both as user-facing
keywords would be exactly the kind of inert-tag growth the
collapse killed. **The cost model is the sole arbiter.**
Bisect/debug needs are served by the `--no-inline` flag
(Q10), not by per-function annotations.

**B++ S4 skips:** PGO (no profile infra), comptime-driven
inlining (no comptime semantics in B++ today), per-function
annotations (Rule 4 — see above).

---

## Design decisions (10 questions resolved)

### Q1 — Pipeline location

**Decision: extend Phase B2 in `bpp_dispatch.bsm`**, do NOT
create a new pass.

Rationale: `run_dispatch()` (line 3910) is already the pass
between parser and codegen. `fn_phase[]`, `fn_locally_impure[]`,
`fn_address_taken[]`, `fn_calls[]`, `glob_writers[]`, and
`glob_readers[]` are all populated by `call_graph_build()`
— S4 reads them and never recomputes. Codegen splice point
already exists (12 lines per chip); S4 extends it to handle
multi-statement bodies.

### Q2 — Cost metric (grownable, not laddered)

**Decision: extend `_inline_count_nodes` into a cost function
with bonuses and penalties (Rust MIR style) + context-adjusted
threshold (LLVM style).**

Cost function:

```
cost(callee) = node_count(callee.body)
             + 5  × indirect_call_count(callee)
             + 10 × global_write_count(callee)
             - 10 × const_arg_count(callsite)
             - 5  × dead_branch_count_after_subst(...)
             - 3  × null_guard_eliminated(...)
```

Threshold (per call site):

```
threshold = base_threshold              # default 30 (see "Sanity-check on xorshift64")
if caller in hot path:    threshold *= 2
if callsite in loop:      threshold *= 1.5
```

No `@inline` / `@no_inline` overrides — see "Industry research"
above for why B++ deviates from the universal pattern. The cost
model is the sole arbiter. Programs needing to disable inlining
globally for bisect/debug use the `--no-inline` flag (Q10).

Decision: inline if `cost ≤ threshold` AND `node_count(body)
≤ body_node_cap` (see Q4), subject to per-binary growth cap.

**No ladder of hardcoded constants.** Each call site computes
its own threshold from static signals already in B++:
`_fn_callers[fi]` (line 92 of bpp_dispatch.bsm — inverse call
graph), `fn_phase[fi]`, loop depth (needs ~20 LOC parser
annotation on T_CALL nodes via `.d` field already reserved).

### Sanity-check on xorshift64 (the anchor)

The whole point of S4 is to inline xorshift64 from
`examples/tablah.bpp`. Calibrate `base_threshold` against the
actual measured shape:

```
xorshift64 body: 6 statements + 1 T_RET, 1 global write
  s = rng_states[worker_id];      // T_ASSIGN + T_MEMLD
  s = s ^ (s << 13);
  s = s ^ (s >> 7);
  s = s ^ (s << 17);
  rng_states[worker_id] = s;      // T_MEMST (global write)
  return s;

Estimated node_count: ~31
cost = 31 + 10 (1 global write) + 0 - 0 - 0 - 0 = 41
```

Call site at `tablah.bpp:70` is inside a `for` loop, so
`threshold = base × 1.5`.

To inline: need `threshold ≥ 41` → `base_threshold ≥ 28`.

**Default lands at `base_threshold = 30`** (rounds up the
xorshift64 anchor with a small margin). Industry parallels:

- LLVM `OptSizeThreshold = 50`
- GCC `max-inline-insns-auto` historically ~30-80
- Phase B2 today: 5 (single-expression bodies only — different cost model)

If xorshift64's body grows past ~37 nodes, the margin
collapses; that's a known boundary and an acceptable design
constraint — going beyond means re-tuning `base_threshold`
upward (single constant, one commit), not a global refactor.

### Q3 — Alpha-rename + multi-statement splice mechanics

**Decision: add slots to `cg_vars` with mangled names
`_inl<N>_<orig>`, monotonic counter per call site. Frame
slots pre-registered via `fn_pre_reg_vars` extension, BEFORE
`fn_compute_offsets` runs.**

Rationale: `cg_vars` is flat per-function (line 914-918 of
bpp_codegen.bsm). Scoped sub-tables would require refactoring
`cg_var_idx` which scans linearly. Mangling with a monotonic
counter guarantees uniqueness — `_inl42_y` never collides with
`_inl43_y` or with caller's own `y`.

**Splice mechanics for multi-statement bodies** (the part Phase
B2 doesn't have to handle):

Phase B2's splice is trivial because the body is a single
expression — clone, substitute, emit. Multi-statement bodies
need to execute prelude statements (assignments, etc.) BEFORE
producing the call's return value.

Current emit pipeline (step-by-step from `cg_emit_func`, line
2225-2269 of bpp_codegen.bsm):

```
2. arr_clear(cg_vars)
5. cg_var_add for each param
6. fn_pre_reg_vars(body)    ← walks body, adds T_DECL'd locals
   fn_compute_offsets()      ← lays out frame offsets
7..14. emit prologue + body  ← per-node dispatch
```

The two-pass structure (pre-register then emit) is the key.
For S4, extend step 6:

```
6'. fn_pre_reg_vars walks body INCLUDING inlineable T_CALL sites.
    For each inlineable callsite C:
      - assign next monotonic ID N
      - for each T_DECL in callee body, cg_var_add("_inl<N>_<orig>")
      - record N on the T_CALL node (new field, or T_CALL.d
        if not used for loop_depth)
```

Then at emit time, when the codegen hits an inlineable T_CALL:

```
Splice procedure (extension of Phase B2 splice):
  for stmt in body[0 .. n-2]:          // all prelude statements
    cloned = ast_clone_subst(stmt, params, args, locals_map<N>)
    emit_node(cloned)                  // executes for side effects
  final_ret = body[n-1]                // the T_RET
  cloned_expr = ast_clone_subst(final_ret.a, ...)
  return emit_node(cloned_expr)        // value lands in acc
```

Why this works in B++ specifically: emit_node landing the
final value in `acc` is EXACTLY what regular T_CALL emission
does. Caller's enclosing expression doesn't care whether `acc`
came from a `bl callee` instruction or from inline splice —
the contract is the same.

`ast_clone_subst` is extended to take `locals_map<N>` — a
parallel array of (original-name, mangled-name) pairs. When it
sees a T_VAR matching a callee local, it rewrites to the
mangled name. The existing param-substitution logic stays
intact.

**No statement-lifting pass needed.** The splice happens
during expression emission with the codegen's normal flow.
Stack/register state across the inlined statements is
preserved because each statement emit ends with values
written to frame slots (the mangled locals) or to memory —
no live values across statement boundaries except via slots,
which is identical to non-inlined emission.

### Q4 — Side-effect rule + caps

**Decision: phase whitelist (BASE + AUTO), expression-level
"enclosing" check, plus two industry-standard caps.**

#### Phase eligibility (relaxation of Phase B2)

Phase B2 today requires `fn_phase[i] == PHASE_BASE` (pure).
That excludes xorshift64, which writes `rng_states` and
therefore is not PHASE_BASE.

S4 widens to **PHASE_BASE OR PHASE_AUTO**:

```
allowed_phase(fi):
  return fn_phase[fi] in { PHASE_BASE, PHASE_AUTO }

rejected_phase(fi):
  return fn_phase[fi] in { PHASE_IO, PHASE_GPU, PHASE_HEAP,
                           PHASE_SOLO, PHASE_REALTIME,
                           PHASE_SAFE, PHASE_PANIC }
```

Rationale: `PHASE_AUTO` is the lattice no-op (line 322 +
413-414 of `bpp_dispatch.bsm`) — functions that have side
effects but don't transitively reach malloc/IO/GPU stay
PHASE_AUTO. xorshift64 is precisely PHASE_AUTO: writes a
global, but the global isn't malloc'd by this function and no
realtime constraint is on it. Allowing PHASE_AUTO catches the
"tame impurity" case (global reads + global writes) without
opening the door to malloc/IO/GPU.

#### Enclosing-expression rule (precision)

For each global G that callee writes (lookup via `glob_writers`
reverse map), the call site is **rejected for inlining** if
any T_VAR / T_MEMLD referencing G appears anywhere in the
**enclosing expression** of the T_CALL.

**Enclosing expression** is defined as: the entire AST subtree
from the T_CALL upward, bounded by the nearest of:

- A statement boundary (T_ASSIGN's RHS root, T_MEMST's source,
  T_RET's value, T_IF condition, T_WHILE condition,
  statement-separator `;`)
- A sequence-point operator (`&&`, `||`, ternary `?:`) — the
  short-circuit boundary

Worked example for `char_idx = (xorshift64(worker_id) &
0x7FFFFFFFFFFFFFFF) % 62;` (tablah.bpp:70):

- T_CALL's parent: T_BINOP(&)
- Up: T_BINOP(%)
- Up: T_ASSIGN's RHS — statement boundary
- Enclosing = the entire RHS subtree
- Walk: no T_VAR(rng_states), no T_MEMLD on rng_states → **pass**

Adversarial example `x = rng_states[i] + xorshift64(j);`:

- Enclosing = RHS subtree
- Walk: finds T_MEMLD(rng_states[i]) → **reject**, fall back
  to regular call (caller's read-before-write semantics
  preserved)

Implementation: the check runs at **splice time** (per call
site), not at classify time (per function).
`classify_inlineable_v2` decides eligibility-in-principle; the
enclosing-expression check decides per-callsite. Implementation
captures the expression-root context during codegen tree walk
(no AST parent pointers needed — the recursion stack already
provides ancestor context). ~30 LOC in P3c.

#### Caps (two independent gates)

- **Per-function absolute ceiling: `body_node_cap = 50`**
  (LLVM `OptSizeThreshold` style). Hard gate on raw
  `node_count(body)`, regardless of bonuses or caller hotness.
  Independent from `threshold` (which is cost-with-bonuses).
- **Per-binary growth cap: +20%** (GCC `inline-unit-growth`
  style). Inliner maintains a "bytes injected so far" counter
  in `run_dispatch()`. Once binary growth would exceed 20%,
  inliner stops accepting candidates regardless of individual
  cost.

The two caps serve different protections:
- `body_node_cap` blocks individual blow-up (one huge function
  being inlined N times).
- `binary_growth` blocks aggregate blow-up (many small
  functions each individually fine, but together too much).

xorshift64 sits at ~31 nodes, well under the 50 cap. Margin of
~19 nodes — comfortable but not lavish. If a future workload's
hot function exceeds 50, the constant is bumped (one-line
tunable); this is a known design boundary.

### Q5 — `fn_ptr` parity

**Decision: mirror Phase B2 exactly.** Source emitted intact,
splice happens "ahead of" the call site at codegen. Already
implemented.

`fn_address_taken[i] == 1` means `fn_ptr(name)` appears
somewhere — Phase B2 already preserves the source body in that
case. S4 inherits this: marking `inlineable = 1` does not
prevent emission of the source. Anyone taking `fn_ptr(foo)`
still gets the address of the source function.

### Q6 — Bench acceptance gates

**Decision: adopt `tests/bench_compile.sh` (Tonify Rule 37)
with four thresholds.**

```
1. Bootstrap byte-stable: g2 == g3 (cmp obrigatório)
2. Suite verde: 178/0/12 native + 141/0/49 C-emit
3. Bench gates (bench_compile.sh, 5 runs, min):
   - tablah.bpp dentro de 5% de tablah_opt.bpp   (anchor)
   - bootstrap regression ≤ 3%
   - small (test_array) regression ≤ 10%
   - medium (test_stbmidi) regression ≤ 5%
```

Rule 37 was added 2026-05-20 precisely to prevent fake-perf
claims (one-time near-miss compared error-out time to compile
time). `bench_compile.sh` prints binary identity (path/size/
mtime) so substitution is impossible.

Rollback rule: any gate fails → revert that specific change.
Bisect-friendly = 1 commit per advancement (mirrors S1-S3k
discipline).

### Q7 — Diagnostic visibility

**Decision: silent by default + `--show-inlines` flag**,
mirror of `--show-promotions` / `--show-phases` / `--show-deps`
(Cap 48 of `how_to_dev_b++.md`).

Cap 48 establishes the pattern: `--show-*` flags print and
exit without producing a binary. Output shape:

```
inline xorshift64  (15 nodes, 1 global_write, 0 calls) → 7 sites
inline pack        (3 nodes,  0 globals, 0 calls)      → 142 sites
reject arr_struct_at (composite body, 30 prims est.)   — S3g pattern
reject huge_helper (200 nodes > cap 50)
```

Not a W-code: Rule 4 of `tonify_checklist.md` reserves W-codes
for user-fixable problems. Inlining is a compiler decision, not
a user issue.

### Q8 — `bug --watch` integration

**Decision: source-mapping via `fn_vt_locs` synonym table; v1
may show mangled name raw, v2 refines.**

`fn_vt_locs` (line 64 of bpp_dispatch.bsm) already maps
locals to kind (register vs frame offset) per function. S4
extends: when inlining `xorshift64` into caller `foo`, emit
synonym entries — `y@xorshift64 ↔ _inl42_y@foo`. `.bug` file
preserves the source-name mapping.

V1 ship rule: skip the synonym, show `_inl42_y` raw in watch.
Functional but ugly. Refine in v2 after the inliner stabilises.

Step-into becomes step-over visually (no call frame), but
variable inspection still works through the synonym.

### Q9 — `@profile` zones interaction

**Decision: propagate naturally.** Zones in the callee body
travel into every inlined call site as part of the cloned AST.

`@profile` lowering (parser line 2437-2510) creates a T_BLOCK
with prologue `T_CALL _prof_zone_enter("name")` + epilogue
`_prof_zone_exit`. Cloning the body in `ast_clone_subst` picks
this up automatically — the T_CALL nodes are preserved.

If a callee body is entirely wrapped in a zone, the zone
appears at each inlined call site. **This is the correct
behaviour** — each site's contribution shows up as a separate
sample in the profile, giving an honest aggregate cost.

Caveat: nested zones still aggregate FLAT (existing v1
limitation, deferred to v3 per `tonify_checklist.md` Rule 25).
S4 does not change that — it only adds more flat sites.

### Q10 — Flags (cost model tunability)

**Decision: four flags, mirror of Cap 48 patterns. No
function-level annotations** — see Industry-research section
for why B++ deviates from the universal `@inline` /
`@no_inline` pattern (Tonify Rule 4).

| Flag | Default | Effect |
|---|---|---|
| `--inline-threshold=N` | 30 | `base_threshold` constant in Q2 formula |
| `--inline-budget=pct` | 20 | per-binary growth cap in Q4 (GCC style) |
| `--no-inline` | off | kill switch: disable Phase B2 + S4 entirely |
| `--show-inlines` | off | print decisions and exit (Q7) |

`--no-inline` is essential for debugging and bisect: when a
bug surfaces after an S4 commit, first test is recompile with
`--no-inline` to isolate. Mirror of `gcc -fno-inline`. Since
B++ ships no per-function inline annotation, the flag is the
sole inliner control surface.

NO profile-guided inlining (no infra in B++ today). NO
learning model. Static cost + bench-gated thresholds are
sufficient to close the tablah gap.

---

## Phase plan (with Phase B2 foundation acknowledged)

LOC estimates revised down from the original ~700 because
Phase B2 already supplies the classify/clone/splice scaffolding.

| Phase | What | LOC est. | Risk |
|---|---|---|---|
| P0 | Parser: annotate T_CALL nodes with `loop_depth` via the `.d` field. SHIPPED as commit `5ca27fa`. Originally scoped to also parse `@inline` / `@no_inline` annotations, but Tonify Rule 4 (the 2026-05-11 Multics-drift collapse — "annotations earn their keep ONLY when they catch a specific bug class") rules them out: `@inline` is a perf hint, not a contract; `@no_inline` is the default. Cost model is the sole arbiter; `--no-inline` flag covers the bisect/debug need. P0 ships with loop_depth only (~17 LOC actual). | ~17 (shipped) | low |
| P1 | Cost function: extend `_inline_count_nodes`, `_inline_has_tcall`, `_inline_param_refs` to walk statement nodes (T_ASSIGN, T_MEMST, T_DECL, T_RET) — currently all four return 99 for non-expression types (Phase B2 assumed `body_cnt == 1` with single T_RET). Then layer penalty+bonus sum on top; add `_inline_const_arg_count`, `_inline_dead_branch_count` helpers. Keep `else → return 99` as the safety net for control-flow types not handled yet. | ~80 | low |
| P2 | Threshold function: lookup `_fn_callers` for fanout, `fn_phase` for hot/cold, T_CALL.d for loop depth, multipliers per Q2 | ~50 | low |
| P3a | Multi-statement body support: extend `classify_inlineable` to accept body_cnt > 1 AND `fn_phase in {BASE, AUTO}`. Extend `ast_clone_subst` symmetrically to clone T_ASSIGN, T_MEMST, T_DECL, T_RET (and the T_BLOCK statement sequence wrapping them). Reject during classify any body containing T_IF/T_WHILE/T_SWITCH/nested T_BLOCK — control-flow inlining is out of scope (Q1 caveat). | ~80 | medium |
| P3b | Local-variable alpha-rename: extend `fn_pre_reg_vars` to walk inlineable T_CALL sites; for each, assign monotonic ID and `cg_var_add("_inl<N>_<orig>")` for each callee local BEFORE `fn_compute_offsets` runs. Splice procedure in both codegens: emit prelude statements via `emit_node`, then final expression value lands in acc | ~100 | medium |
| P3c | Enclosing-expression rule: at splice time, walk caller's expression ancestor chain (captured via codegen recursion stack) bounded by statement/sequence-point; check no T_VAR/T_MEMLD on globals written by callee | ~60 | medium |
| P4 | Caps: per-function (50) hard ceiling check; per-binary growth counter in `run_dispatch`; rollback if cap exceeded mid-pass | ~40 | low |
| P5 | Flags: `--inline-threshold`, `--inline-budget`, `--no-inline`, `--show-inlines` in `bpp_args.bsm`; tests. No annotation parsing — see P0 note + Industry-research section for why B++ skips `@inline` / `@no_inline`. | ~50 | low |
| P6 | Bench iteration: tablah → 5% gate, regression sweep | ~20 | depends |

**Total: ~530 LOC** (down from original ~700 estimate, because
~170 LOC of Phase B2 is reused as-is).

Ship gate per phase: bootstrap byte-stable + suite green + no
regression > 10% on `bench_compile.sh`. Each phase is one
commit, bisect-friendly.

Risk peaks at P3b — alpha-rename is the classically subtle
part. The Phase B2 design avoided it entirely (param-refs ≤ 1
guarantees no duplication). S4 has to do it for real because
multi-statement bodies have locals that survive across
statements.

The two-pass codegen structure (pre-register then emit, steps
6 and 7 of `cg_emit_func`) is what makes this tractable:
P3b's `cg_var_add` calls land in step 6 before
`fn_compute_offsets`, so the frame is sized correctly when
step 7 starts emitting. No mid-emit frame resizing.

---

## Out of scope explicitly

- Inlining bodies with calls (recursion handling — separate
  problem, would need depth budget).
- Inlining loops or branches as control-flow (would need CFG
  rewrite; Phase B2 already accepts no-T_CALL no-T_IF bodies).
- Cross-module inlining (linker work).
- Auto-vectorisation of inlined arithmetic.
- Profile-guided inline choices (PGO — no profile infra in
  B++).
- Comptime-driven inlining à la Zig (no comptime semantics in
  B++).

---

## Acceptance gates (summary)

1. Bootstrap byte-stable: `./bpp src/bpp.bpp -o /tmp/g1` then
   `/tmp/g1 src/bpp.bpp -o /tmp/g2`, then `cmp g2 g3`.
2. Suite 178+/0/12 native + 141+/0/49 C-emit green.
3. `examples/tablah.bpp` within 5% of `examples/tablah_opt.bpp`
   on `bench_compile.sh`.
4. No regression > 10% on any other bench case (bootstrap,
   small, medium).
5. `fn_ptr(foo)` still resolves for any `foo` that got
   inlined.
6. C-emit path also handles the inlined functions (or falls
   back gracefully to the call form — acceptable).
7. `--no-inline` reproduces pre-S4 behaviour exactly.

---

## Sources (industry research, 2026-05-21)

LLVM:
- [LLVM InlineCost.h source](https://llvm.org/doxygen/InlineCost_8h_source.html)
- [LLVM InlineCost.cpp source](https://llvm.org/doxygen/InlineCost_8cpp_source.html)

GCC:
- [GCC Optimize Options (inline parameters)](https://gcc.gnu.org/onlinedocs/gcc-4.2.4/gcc/Optimize-Options.html)
- [Jan Hubicka — Increase inline-unit-growth to 20%](https://gcc.gnu.org/legacy-ml/gcc-patches/2015-04/msg00160.html)

Rust:
- [rustc Inliner (rustc_mir_transform::inline)](https://doc.rust-lang.org/nightly/nightly-rustc/rustc_mir_transform/inline/struct.Inliner.html)
- [Rust PR #123179 — Rework MIR inlining costs](https://github.com/rust-lang/rust/pull/123179)
- [Rust PR #126578 — Account for things that optimize out in inlining costs](https://github.com/rust-lang/rust/pull/126578)
- [Rust RFC #3711 — `#[inline(required)]`](https://github.com/rust-lang/rfcs/pull/3711)
- [Rust PR #134082 — `#[rustc_force_inline]`](https://github.com/rust-lang/rust/pull/134082)

Jai:
- [JaiPrimer (BSVino)](https://github.com/BSVino/JaiPrimer/blob/master/JaiPrimer.md)

Zig:
- [Zig PR #7647 — stage2 comptime + inline function calls](https://github.com/ziglang/zig/pull/7647)
- [Zig — What is Comptime? (Loris Cro)](https://kristoff.it/blog/what-is-zig-comptime/)
- [Zig Issue #7772 — Proposal: Inline Parameters](https://github.com/ziglang/zig/issues/7772)

B++ anchor files (foundation that S4 extends):
- `src/bpp_dispatch.bsm:3720-3905` — Phase B2 inliner
- `src/bpp_codegen.bsm:1408-…` — `cg_builtin_dispatch` lane
- `src/backend/chip/aarch64/a64_codegen.bsm:1200-1212` — splice site (a64)
- `src/bpp_parser.bsm:1840-1908` — `@safe` annotation parsing (canonical pattern; S4 deliberately does NOT add a sibling per Rule 4)
- `docs/manual/tonify_checklist.md` Rule 4 — annotation-collapse rationale (the 2026-05-11 "Multics-drift" lesson)
- `docs/manual/how_to_dev_b++.md` Cap 48 — flag conventions
- `docs/manual/tonify_checklist.md` Rule 37 — bench protocol
- `docs/sidequest_compiler_hotpath_opt.md` — preceding arc, including S3g revert lesson
