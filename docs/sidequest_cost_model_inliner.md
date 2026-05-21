# Sidequest — Cost-model inliner (S4)

**Status:** DESIGN-LOCKED 2026-05-21, ready-to-execute.
**Trigger:** Hot-path opt arc S1→S3k (closed 2026-05-21) shipped
~41% bootstrap improvement via mechanical `cg_builtin_dispatch`
inlining of trivial-body functions. The remaining wins on
real-game workloads need a generalised inliner with a cost
model — the dispatch lane has no shared register state across
primitive calls, so anything beyond a single arithmetic op
loses to stack-juggling overhead (see S3g revert in
`docs/sidequest_compiler_hotpath_opt.md`).

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
| `@inline` (force) annotation | ✓ | ✓ | ✓ | ✓ | ✓ |
| `@no_inline` (block) annotation | ✓ | ✓ | ✓ | ✓ | ✓ |
| PGO integration | ✓ | ✓ | ✓ | – | – |

**B++ S4 adopts:** cost model w/ bonuses + penalties, per-
callsite threshold adjustment, both caps (per-function +
per-binary), `@inline` / `@no_inline` annotations.

**B++ S4 skips:** PGO (no profile infra), comptime-driven
inlining (no comptime semantics in B++ today).

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
threshold = base_threshold              # default 5 (Phase B2 today)
if caller in hot path:    threshold *= 2
if callsite in loop:      threshold *= 1.5
if callee is @inline:     return ALWAYS  # override
if callee is @no_inline:  return NEVER   # override
```

Decision: inline if `cost ≤ threshold`, subject to the caps in
Q4.

**No ladder of hardcoded constants.** Each call site computes
its own threshold from static signals already in B++:
`_fn_callers[fi]` (line 92 of bpp_dispatch.bsm — inverse call
graph), `fn_phase[fi]`, loop depth (needs ~20 LOC parser
annotation on T_CALL nodes via `.d` field already reserved).

### Q3 — Alpha-rename

**Decision: add slots to `cg_vars` with mangled names
`_inl<N>_<orig>`, monotonic counter per call site. NOT a new
scoped symbol table.**

Rationale: `cg_vars` is flat per-function (line 914-918 of
bpp_codegen.bsm). Scoped sub-tables would require refactoring
`cg_var_idx` which scans linearly. Mangling with a monotonic
counter guarantees uniqueness without scope tracking —
`_inl42_y` never collides with `_inl43_y` or with caller's
own `y`. `fn_compute_offsets` (line 385) handles frame growth
naturally; existing `auto x; auto y;` codegen already grows
frames the same way.

Implementation: extend `ast_clone_subst` to also substitute
T_VAR references to callee locals with mangled-name T_VAR
nodes. Each callee local triggers a fresh `cg_var_add` in the
caller before splice.

### Q4 — Side-effect rule + caps

**Decision: use `fn_locally_impure[]` + `glob_writers[]` +
`glob_readers[]` already populated, plus two industry-standard
caps.**

Allow rule:
1. `fn_phase[i] != PHASE_IO` and `!= PHASE_GPU` (Phase B2
   already enforces; keep)
2. For each global G written by callee: caller's enclosing
   expression of the call site must not also reference G

Caps:
- **Per-function absolute ceiling: 50 nodes** (LLVM
  `OptSizeThreshold` style). No call site, no matter how hot,
  inlines a body larger than 50 nodes.
- **Per-binary growth cap: +20%** (GCC `inline-unit-growth`
  style). Inliner maintains a "bytes injected so far" counter
  in `run_dispatch()`. Once binary growth would exceed 20%,
  inliner stops accepting candidates regardless of individual
  cost.

xorshift64 (the anchor): writes `rng_state`, reads
`rng_state`. Typical call site is `x = xorshift64();` (pure
assignment, no concurrent read of the same global) — passes.
Pathological case `x = rng_state + xorshift64();` is statically
detected, rejected.

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

**Decision: three flags, mirror of Cap 48 patterns.**

| Flag | Default | Effect |
|---|---|---|
| `--inline-threshold=N` | 5 | `base_threshold` constant in Q2 formula |
| `--inline-budget=pct` | 20 | per-binary growth cap in Q4 (GCC style) |
| `--no-inline` | off | kill switch: disable Phase B2 + S4 entirely |
| `--show-inlines` | off | print decisions and exit (Q7) |

Plus two annotations at the function definition site (mirror of
`@safe` / `@profile`):

| Annotation | Effect |
|---|---|
| `@inline` | Force inline at every call site (override cost model upward) |
| `@no_inline` | Block inline at every call site (override cost model downward) |

`--no-inline` is essential for debugging and bisect: when a
bug surfaces after an S4 commit, first test is recompile with
`--no-inline` to isolate. Mirror of `gcc -fno-inline`.

NO profile-guided inlining (no infra in B++ today). NO
learning model. Static cost + bench-gated thresholds are
sufficient to close the tablah gap.

---

## Phase plan (with Phase B2 foundation acknowledged)

LOC estimates revised down from the original ~700 because
Phase B2 already supplies the classify/clone/splice scaffolding.

| Phase | What | LOC est. | Risk |
|---|---|---|---|
| P0 | Parser: annotate T_CALL nodes with `loop_depth` (field `.d` already reserved); add `@inline` / `@no_inline` to phase_hint parsing | ~50 | low |
| P1 | Cost function: extend `_inline_count_nodes` to compute penalty+bonus sum; add `_inline_const_arg_count`, `_inline_dead_branch_count` helpers | ~80 | low |
| P2 | Threshold function: lookup `_fn_callers` for fanout, `fn_phase` for hot/cold, T_CALL.d for loop depth, multipliers per Q2 | ~50 | low |
| P3a | Multi-statement body support: extend `classify_inlineable` to accept body_cnt > 1 (no globals yet); extend `ast_clone_subst` to clone T_BLOCK | ~80 | medium |
| P3b | Local-variable alpha-rename: detect T_DECL in body, mangle to `_inl<N>_<orig>`, cg_var_add at splice point in both codegens | ~100 | medium |
| P3c | Global write rule: check enclosing-expression rule from Q4 against `glob_readers[g]` for each `g` in callee's write set | ~60 | medium |
| P4 | Caps: per-function (50) hard ceiling check; per-binary growth counter in `run_dispatch`; rollback if cap exceeded mid-pass | ~40 | low |
| P5 | Flags: `--inline-threshold`, `--inline-budget`, `--no-inline`, `--show-inlines` in `bpp_args.bsm`; tests | ~50 | low |
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
- `src/bpp_parser.bsm:2437-2510` — `@profile` zone lowering (model for `@inline` parsing)
- `docs/manual/how_to_dev_b++.md` Cap 48 — flag conventions
- `docs/manual/tonify_checklist.md` Rule 37 — bench protocol
- `docs/sidequest_compiler_hotpath_opt.md` — preceding arc, including S3g revert lesson
