# Sidequest — Cost-model inliner (S4)

**Status:** PROPOSED 2026-05-21, design-phase pending.
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

## Why this is its own sidequest, not a tack-on

Three honest options were on the table at the close of the hot-
path arc:

1. **S4 minimum (~200 LOC, single-return-no-calls).** Would not
   catch xorshift64 (multi-statement). Win on tablah ~0. Useful
   only as throwaway infra study before the real version.
2. **S4 serious (~700 LOC, multi-stmt + locals + alpha-rename +
   side-effect tracking).** Real ~18% win on tablah. 1-2
   dedicated sessions worth of work.
3. **Ship the arc, defer.** Picked. Closes ~41% gain cleanly,
   gives S4 the design phase it deserves instead of a hurry-up
   tack-on at end of a long arc.

The minimum version is rejected up-front because its scope is
defined by what's easy, not what's profile-justified. The
serious version is the only one with a real anchor benchmark.

---

## Goal

Generalise `cg_builtin_dispatch`-style inlining to handle:

- Multi-statement bodies (sequence of assignments + arithmetic)
- Local-variable bodies (with alpha-renaming at the call site
  to avoid name collisions)
- Side-effect tracking (global writes are OK; reads of globals
  the caller might have modified intra-expression are NOT)
- Return-via-last-expression OR return-via-last-statement

Out of scope (Tier F roadmap):

- Bodies with calls (recursive inlining — separate problem)
- Bodies with loops / branches (control-flow inlining — wants
  CFG rewrite)
- Cross-module inlining (linker phase, not parser/codegen)

The goal shape is: any non-recursive function whose body is a
straight-line block of assignments + arithmetic + a return,
small enough by some cost metric, gets inlined at every call
site with locals renamed to fresh names.

---

## Design questions to resolve before any code

1. **Where does the inliner live in the pipeline?** Options:
   - Parser-level T_CALL → T_BLOCK rewrite. Pros: reuses
     existing codegen; cons: shape detection runs on every call
     site even if function not picked.
   - Codegen-level: `cg_builtin_dispatch` extension that
     synthesises an inline AST on the fly. Pros: lazy; cons:
     dispatch is one site; doesn't compose with multi-body
     functions naturally.
   - New pass between parser and codegen: scan all functions,
     mark inline candidates, rewrite call sites. Pros: clean;
     cons: new pass = new infra.

   Recommend: parser-level rewrite + cost-gate cache. The
   shape-detection cost amortises once per function definition,
   not once per call site.

2. **Cost metric — what counts?** Candidates:
   - LOC of body (~3-5 stmts ceiling)
   - AST node count (more robust; ignores comments/style)
   - Estimated emit-byte count (most accurate but expensive)
   - Hard whitelist seeded by profile

   Recommend: AST node count ≤ N where N is tunable. Start
   N=15, profile, adjust.

3. **Alpha-rename strategy.** Local `auto x` in inlined body
   collides with caller's `x`. Options:
   - Mangle: `_inl_<callsite>_<orig>` per call site
   - SSA-style: rename with monotonic counter
   - Block-scoped via T_BLOCK with own symbol table

   Recommend: T_BLOCK with own symbol table. Existing parser
   infra. No mangling, no counter. Symbols just don't escape.

4. **Side-effect rules.** xorshift64 mutates global `rng_state`.
   That's fine in isolation but breaks if the call appears in
   an expression that ALSO reads `rng_state`:
   ```
   x = rng_state + xorshift64();  // BEFORE: read, then write
   ```
   Inlining naively would interleave the read and write. The
   safe rule: if body mutates global G AND caller's enclosing
   expression also references G, refuse to inline (fall back
   to call). Sequence-point preserved.

5. **fn_ptr(foo) parity.** When `foo` is inlined at all call
   sites, the B++ function body still has to exist for
   `fn_ptr(foo)` takers. Mirror S3b pattern: keep the source
   definition, add inline dispatch in front. No regression.

6. **Bench protocol.** tablah.bpp + tablah_opt.bpp are the anchor
   pair. Acceptance gate: tablah.bpp must close to within 5%
   of tablah_opt.bpp on bench_compile.sh. Bootstrap byte-stable
   is non-negotiable.

---

## Phase plan (after design lock)

| Phase | What | LOC est. | Risk |
|---|---|---|---|
| P0 | Design doc + spike on xorshift64 by hand (parser rewrite by hand, verify correctness + perf) | ~100 | low |
| P1 | Shape detector — walk function body, return cost + alpha-rename feasibility | ~150 | low |
| P2 | Side-effect analyser — find global reads/writes in body | ~120 | medium |
| P3 | Rewriter — T_CALL → T_BLOCK with renamed locals | ~200 | medium |
| P4 | Cost-gate cache + integration into compile pipeline | ~80 | low |
| P5 | Bench + iterate | ~50 | depends |

Total ~700 LOC. Ship gate per phase = bootstrap byte-stable +
suite green + no regression on `bench_compile.sh`. Risk peaks
at P2/P3 — alpha-rename + side-effect analysis are the
classically subtle parts of any inliner.

---

## Out of scope explicitly

- Inlining bodies with calls (no recursion handling, no call
  cost propagation).
- Inlining loops or branches (no control-flow inlining).
- Cross-module inlining (linker work, separate problem).
- Auto-vectorisation of inlined arithmetic (Tier F.3, months).
- Profile-guided inline choices (would need a feedback loop;
  start with static cost model).

---

## Acceptance gates

1. Bootstrap byte-stable: `./bpp src/bpp.bpp -o /tmp/g1` and
   `/tmp/g1 src/bpp.bpp -o /tmp/g2`, then `cmp g2 g3`.
2. Suite 177+/0/12 native + 141+/0/48 C-emit green.
3. `examples/tablah.bpp` within 5% of `examples/tablah_opt.bpp`
   on `bench_compile.sh`.
4. No regression > 10% on any other bench case (bootstrap,
   small, medium).
5. `fn_ptr(foo)` still resolves for any `foo` that got inlined.
6. C-emit path also handles the inlined functions (or falls
   back gracefully to the call form — acceptable).

---

## Open questions to ask at design phase kickoff

- Is the cost model fixed at compile time or tunable via flag?
- Does the inliner emit a diagnostic when it inlines (debug
  visibility) or stays silent?
- How does `bug --watch` show inlined functions in the
  stepper — as the source function or as the inlined block?
- What's the interaction with `@profile` zones — does an
  inlined function's `@profile` annotation propagate or get
  dropped?

These are not blockers but need answers before P3 lands.
