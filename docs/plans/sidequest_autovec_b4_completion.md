# Sidequest — Autovec / Phase B4 completion

**Status:** PROPOSED 2026-05-22, design-phase open.

**Trigger:** Phase B4 shipped explicit `: double` SIMD (11
`vec_*` builtins, NEON + SSE2 native, 2026-04-15) as the
*manual* form. The *automatic* form — autovectorization, where
the compiler detects SIMD-friendly loops and emits packed
operations without programmer intervention — was never built.
Smart-dispatch outlining (closed 2026-05-22) gives N-cores
parallelism; autovec adds M-lanes SIMD per iteration. Together
they're the compose-multiplicatively story this perf arc has
been building toward.

**Benchmark anchor candidates:** the canonical game-dev hot
loops that AVX-512-era C++ engines deliver as a matter of
course:

```bpp
// 1. ECS update — pos += vel * dt, scalar today, 4-wide SIMD
//    via SSE2 / NEON if autovec catches it.
for (i = 0; i < n_units; i++) {
    units[i].pos_x = units[i].pos_x + units[i].vel_x * dt;
    units[i].pos_y = units[i].pos_y + units[i].vel_y * dt;
}

// 2. Pixel pipeline — per-channel float math over a row.
for (x = 0; x < w; x++) {
    out[x] = (in_r[x] * 0.3) + (in_g[x] * 0.59) + (in_b[x] * 0.11);
}

// 3. Audio mixer — per-sample add into a destination buffer.
for (i = 0; i < n_frames; i++) {
    dst[i] = dst[i] + src[i] * volume;
}
```

Each pattern is canonical SIMD-friendly. Each would benefit
~4x from packed 4-wide ops on either NEON or SSE2. None
auto-vectorizes today in B++. All three are common in game
code that B++ targets.

---

## Where Phase B4 already is (foundation)

Shipped 2026-04-15 (commit-era; see `docs/journal.md`
2026-04-15 entry "Mini Cooper Phase B4 — `: double` SIMD"):

| Component | Status | File |
|---|---|---|
| `SL_DOUBLE = 11` slice constant | shipped | `src/bpp_defs.bsm:227` |
| `TY_FLOAT_D = 0xB3` type code | shipped | `src/bpp_defs.bsm` |
| `: double` parser annotation | shipped | `bpp_parser.bsm:1116` |
| 16-byte stack slot for `auto v: double` | shipped | `cg_var_*` frame layout |
| 11 `vec_*` builtins | shipped | `bpp_codegen.bsm` (vec_load4/store4/add4/sub4/mul4/div4/min4/max4/splat4/dot4/get4) |
| ARM64 NEON encoders | shipped | `a64_enc.bsm` (13 new encoders) |
| x86_64 SSE2 encoders | shipped | `x64_enc.bsm` (packed family + MOVHLPS path) |
| `emit_node` return convention extends to SIMD (rty=2) | shipped | spine + chip emitters |
| E231 diagnostic — SIMD into non-`: double` slot | shipped | type-check pass |
| Frame alignment for SIMD slots (absolute 16-byte) | shipped | frame layout |
| 4 canonical tests | shipped | `test_vec_simd`, `test_vec_align`, `test_vec_align_stack`, `test_vec_struct` |
| Cross-compile NEON / SSE2 parity | shipped | macOS ARM64 + Linux x86_64 docker |

What's NOT shipped (the actual "completion" surface):

| Gap | Where it bites | Effort |
|---|---|---|
| **Autovectorization pass** | Scalar loop → packed SIMD inference. The big gap. C++/Rust devs assume this works; B++ requires manual `vec_*` calls. | Major (~600-1000 LOC) |
| C emitter SIMD path | `bpp --c` emits scalar `long long` for everything, including `: double`. Cross-compile to C drops SIMD silently. Deferred 2026-04-15 with TODO. | Medium (~150 LOC) |
| Wider vectors (AVX2/AVX-512) | SSE2 only (128-bit, 4 lanes float). Modern x86 has 256-bit (8 lanes) and 512-bit (16 lanes). Apple Silicon has SVE eventually. | Major (separate arc) |
| Aligned-memory type slice | `malloc_aligned` exists; type-system-level alignment hint doesn't. Programmer can't say "this array is 16-byte aligned" to unlock MOVAPS. | Small (~50 LOC) |
| Composition with outlining | Worker bodies emitted by outlining don't yet trigger autovec inside the worker. | Medium (~80 LOC) |
| SoA-friendly iteration | Game-dev SoA pattern (`units.pos_x[i]`, `units.pos_y[i]`) needs autovec to span multiple parallel array reads. | Medium (~100 LOC) |

---

## Scope decision — what THIS sidequest ships

Several scope tiers are viable. The doc proposes Tier A + B as
the sidequest target; C-E listed for context as future arcs.

**Tier A — C emitter SIMD path (defensive, portability).**
The deferred work from 2026-04-15. `bpp --c` should emit
`__m128 / __m256` types for `: double` locals and `_mm_*`
intrinsics for the 11 `vec_*` builtins. Without this,
cross-compile-to-C silently drops SIMD; with it, the
guarantee is "vec_* compiles and runs identically across all
three backends." ~150 LOC, low-medium risk.

**Tier B — Minimum-viable autovec.** A pass that recognises
the THREE patterns at the top of this doc — "array iteration
with constant stride, all reads/writes via `arr[i]` shape, all
ops are SIMD-friendly" — and lowers them to chunked `vec_*`
calls. NOT a general autovectorizer (those are 10k+ LOC in
GCC/LLVM); a *target-pattern recognizer* for the canonical
game-dev cases. ~600 LOC, medium-high risk.

**Tier C — Aligned-memory hint.** `auto buf: aligned;` or
similar annotation. Compiler emits MOVAPS instead of MOVUPS,
which on some chips is ~10% faster. ~50 LOC, low risk.
Probably part of THIS sidequest because it's small and
unblocks max-perf SIMD on aligned data.

**Tier D — Autovec inside outlined workers.** When smart-
dispatch synth-generates a worker function, run autovec on
its body too. Multiplies the gains. ~80 LOC, medium risk.
Worth folding into this sidequest if Tier B lands cleanly.

**Tier E — Wider vectors (AVX2/AVX-512).** Out of scope for
this sidequest. Separate work — requires runtime CPU feature
dispatch (which B++ explicitly doesn't have today, per the
2026-04-15 entry), and the perf payoff scales sub-linearly
(2-4× on bandwidth-bound code, less elsewhere).

**Tier-A + B + C + D = this sidequest. ~880 LOC estimate,
multi-session.**

---

## Industry research — autovectorization

Five reference points surveyed before locking the design.

### GCC tree-ssa vectorizer

The original aggressive autovectorizer. Lives in
`gcc/tree-vect-*.c` (~30 k LOC across multiple files). Phases:

1. *Loop analysis* — detect "vectorizable" candidates. Stride-1
   array access, no cross-iter deps (alias analysis), no
   control flow (or only simple `if`), no side effects.
2. *Dependence analysis* — uses ZIV/SIV/MIV chain analysis +
   GCD test + Banerjee test to prove no aliasing.
3. *Vector length selection* — pick lane width based on target
   ISA + element size.
4. *Vector code generation* — replace scalar loop with vector
   loop + scalar epilogue (for remaining iterations after the
   vector loop ends).

Source: GCC source `gcc/tree-vect-loop.c`, `tree-vect-data-refs.c`,
plus the canonical paper "Auto-vectorization in GCC" by Dorit
Nuzman et al, GCC Developers Summit 2003.

### LLVM Loop Vectorizer

Newer, more aggressive. `llvm/lib/Transforms/Vectorize/LoopVectorize.cpp`
(~10 k LOC). Two-step:

1. *Legality check* — same shape as GCC's (alias, dep,
   structure).
2. *Cost model* — estimates speedup; only vectorizes if
   profitable. Modern LLVM cost model is the killer feature —
   it AVOIDS vectorizing when scalar would be faster (small
   trip count, branchy bodies, register pressure).

Source: Vector-related papers from Hal Finkel et al at LLVM
Dev Meetings 2013-2020.

### Intel ICC (legacy)

The original aggressive autovectorizer. Heuristic-heavy, less
formally grounded but historically achieved highest
performance on Intel chips. Deprecated 2023 in favour of LLVM
backend.

### ISPC (Intel SPMD Program Compiler)

Different angle: programmer writes scalar-looking code, but
the compiler treats every variable as an implicit SIMD lane
("Program Multiple Data"). Each lane is one execution; the
compiler generates lock-step SIMD code automatically. Doesn't
need autovec because the IR is already SIMD-by-construction.

Source: Matt Pharr, "ISPC: A SPMD Compiler For High-Performance
CPU Programming", InPar 2012.

### Halide

Decouples *algorithm* from *schedule*. Programmer writes
algorithm declaratively (`output(x, y) = input(x, y) + 1;`),
then chooses a schedule (`output.vectorize(x, 4).parallel(y);`).
Compiler generates the SIMD + parallel code from the algorithm
+ schedule pair.

Source: Jonathan Ragan-Kelley et al, "Halide: A language and
compiler for optimizing parallelism, locality, and recomputation
in image processing pipelines", PLDI 2013.

### Universal patterns

Every aggressive autovectorizer:

  * Picks a SET of canonical patterns to recognise (not arbitrary
    code).
  * Uses dep analysis to PROVE the transformation is safe.
  * Generates a vector loop + scalar tail (for trip counts that
    aren't a multiple of the vector lane width).
  * Has a COST MODEL to decide whether to vectorize at all.
  * Falls back to scalar gracefully (loop runs serially if
    pattern doesn't match).

B++ Tier B autovec adopts: pattern-recognition + scalar tail
+ no-aliasing assumption (we won't do GCD/Banerjee — too
complex; instead reject candidates where aliasing isn't
provable from declared types).

---

## Design questions (to resolve before P0)

### Q1 — Pattern matcher scope

Three patterns to recognise:

```
P1: arr[i] = arr[i] OP literal_or_loop_invariant
P2: arr[i] = lhs[i] OP rhs[i]
P3: arr[i] = lhs[i] OP rhs[i] OP rhs2[i]   (3-source fused)
```

All three are extremely common in game code. P1 handles
mixer-style "scale by volume". P2 handles vector add /
subtract / multiply. P3 handles "FMA-like" patterns.

Recommend: P1 + P2 for v1; P3 if v1 lands cleanly.

### Q2 — Aliasing assumption

Two arrays `arr[i]` and `lhs[i]` might be aliased pointers
to the same memory. Aggressive vectorizers prove they aren't
(via dep analysis); B++ Tier B will assume non-aliasing
unless the parser can prove otherwise from declared types.

Risk: if a programmer aliases two `arr`-typed pointers, the
vectorized code computes wrong results. Game devs rarely do
this on purpose, but the failure mode is silent miscompile.

Mitigation: add a "no autovec" annotation on loops where
the programmer wants conservative behaviour (probably
`@seq` revived — Rule 38 already documents the explicit-vs-
implicit decision; adding `@seq` for "don't autovec this"
is the same shape).

Alternative mitigation: only autovec on declared types we
can statically prove disjoint (`auto a, b: ptr_to_4xfloat`
where the types disagree). Stricter, less coverage.

Recommend: assume non-aliasing + `@seq` opt-out. Same
discipline as the outlining sidequest.

### Q3 — Vector lane width

SSE2 / NEON both deliver 4 × 32-bit float. The 11 `vec_*`
builtins are all 4-wide. Tier B emits 4-wide code throughout.

When AVX2/AVX-512 land (Tier E, future), the same matcher
generates 8-wide or 16-wide code per target.

### Q4 — Scalar tail

A 4-wide loop processes iterations [0, n / 4 * 4) in SIMD,
then [n / 4 * 4, n) scalar. The scalar tail is the original
loop body with the index range adjusted. Easy to generate
but doubles the binary footprint of every vectorized loop.

Cost model decision: vectorize only if `n` is statically
known to be large (e.g., `n >= 16`), OR runtime-test the
bound. Recommend: runtime test, since static `n` is rare in
game code.

### Q5 — Where in the pipeline does autovec run?

Three options:

  * Parse-level: rewrite the loop's AST before dispatch
    analysis sees it. Bad: dispatch can no longer reason
    about the original loop shape.
  * Dispatch-level: alongside smart-dispatch outlining. Good:
    same pass, similar analysis. Autovec runs ONLY on loops
    that smart-dispatch rejected (or both can run; smart-
    dispatch picks one).
  * Codegen-level: per-chip emitter detects pattern at emit
    time. Bad: different impl per chip; can't share analysis.

Recommend: dispatch-level, parallel to smart-dispatch. New
function `_fdc_autovec_candidate(wh)` mirror of
`_fdc_try_candidate` shape.

### Q6 — Cost model

Real GCC/LLVM cost models are huge. For Tier B, simple
heuristic:

  * Vectorize if loop body has ≥ 2 SIMD-friendly ops.
  * Vectorize if loop trip count is statically ≥ 16, OR
    arrays are typed as long-known-to-be-large.
  * Skip if body has function calls (no autovec into calls;
    those go through scalar).

### Q7 — Composition with smart-dispatch outlining

If a loop both auto-paralelizes (outlining) AND auto-
vectorizes (autovec), the outlined worker function's body
gets autovec applied. The worker processes `chunk / 4`
SIMD iterations + scalar tail.

Same pattern matcher runs on the synth function's body.
~50 LOC integration.

### Q8 — Bench acceptance gates

Mirror outlining's gates (Rule 37):

1. Bootstrap byte-stable.
2. Suite 178/0/12 native + 141/0/49 C-emit at every phase.
3. One smoke test per autovec pattern proves correctness
   (output matches scalar reference).
4. Bench delta: at least one example program shows ≥ 2x
   speedup on a SIMD-friendly hot loop (closes the cores ×
   lanes story).

---

## Phase plan (proposed, ~880 LOC, multi-session)

| Phase | What | LOC est. | Risk |
|---|---|---|---|
| P0 | Design lock (this doc) | ~30 | low |
| P1 | C emitter SIMD path — `__m128` for `: double`, `_mm_*` for vec_* | ~150 | low |
| P2 | Pattern matcher P1 — `arr[i] = arr[i] OP literal` recognition | ~150 | medium |
| P3 | Pattern matcher P2 — `arr[i] = lhs[i] OP rhs[i]` | ~150 | medium |
| P4 | Aligned-memory hint — `auto buf: aligned;` + MOVAPS path | ~50 | low |
| P5 | Code emission — vector loop + scalar tail generator | ~200 | medium |
| P6 | Outlining composition — autovec inside synth worker bodies | ~80 | medium |
| Pn | Bench + iterate — measure on representative loops | ~80 | depends |

**Total: ~890 LOC, 4-5 sessions estimate.**

Risk peaks at P5 (vector loop + scalar tail generator —
classic compiler-textbook code-gen pattern but
implementation-heavy).

---

## Out of scope explicitly

- **AVX2 / AVX-512** — 256-bit / 512-bit wider vectors.
  Requires runtime CPU feature dispatch which B++ doesn't
  have today. Separate arc.
- **Pattern P3** (3-source fused) — recognise after P1/P2
  prove the matcher works.
- **General loop vectorization** — GCC/LLVM-scale dependence
  analysis (Banerjee, GCD, ZIV/SIV/MIV). Far past B++'s
  scope; the canonical-pattern matcher is enough for
  game-dev cases.
- **Reduction vectorization** — `acc = acc + arr[i]` over a
  loop. Conceptually related to smart-dispatch reductions;
  belongs in a reduction-outlining arc.
- **Loop unrolling** — `for (i ...; i += 4) { body; body+1; body+2; body+3; }`
  is a different transformation (programmer-controlled or
  compiler-decided). Autovec can fold unrolling implicitly
  but explicit unroll is its own arc.
- **Cross-module autovec** — same as outlining: B++ is
  whole-program by design.

---

## Acceptance gates (summary)

1. Bootstrap byte-stable across every phase commit.
2. Native suite 178+/0/12 + C-emit suite 141+/0/49 green
   throughout.
3. New smoke tests `tests/test_autovec_p1.bpp`,
   `tests/test_autovec_p2.bpp` exercise each pattern with
   scalar reference comparison.
4. Bench: at least one representative loop shows ≥ 2×
   speedup on either chip.
5. C-emit path produces equivalent SIMD code (`__m128`-based)
   via `_mm_*` intrinsics — `bpp --c` becomes a SIMD-aware
   transpile target.
6. Composition with outlining: when a loop is both
   parallelizable AND vectorizable, the synth worker body
   uses SIMD ops.

---

## Open questions to resolve at design phase kickoff

- Does autovec need a per-loop `@simd` opt-in annotation, or
  is implicit-when-safe enough? Rule 4 says no new
  annotations without bug class evidence; the "silent
  miscompile on aliased pointers" risk is the evidence
  if we want one. Probably defer — Tier B + `@seq` opt-out
  handles 99 % of cases.
- Cost model: simple heuristic or full analysis? Recommend
  simple heuristic for v1; revisit after first bench
  iteration shows where it misses.
- C-emit SIMD: emit `__m128` directly or use a typedef +
  fallback macro for portability? Most production C SIMD
  uses `_mm_*` intrinsics with `<emmintrin.h>` directly.
  Mirror that.
- AVX256 path: should we wire SCAFFOLDING for AVX-256 now
  (Tier E), even without emitting it? Probably no — YAGNI
  per Rule 28.

---

## Composition target (the full story)

The whole reason this arc matters:

```
Baseline scalar single-thread                           1x
+ S4 inline (shipped 2026-05-21)                       ~1.1x
+ Outlining (shipped 2026-05-22)                       ~8x (8 cores)
+ Autovec Tier B + D (this sidequest)                  ~4x (4 SIMD lanes)
                                                      ─────────
Total combined hot-loop ceiling                        ~35x
```

For a game programmer who writes natural code:

```bpp
void update_all(world, dt) {
    auto i;
    for (i = 0; i < world.n_units; i++) {
        units[i].pos = units[i].pos + units[i].vel * dt;
    }
}
```

After this sidequest:
- Outlining synthesises `__synth_K(idx, _cap_ptr)`.
- Stage 3 publishes `world`, `dt` into a capture struct,
  calls `job_parallel_for_data(world.n_units, fn, &cap)`.
- Autovec recognises the loop body matches P2 (`pos[i] = ...
  + vel[i] * dt`), emits 4-wide SIMD inside the worker.
- 8 cores × 4 SIMD lanes = 32 parallel updates per cycle.

This is the perf ceiling Burst / Unity DOTS deliver. B++
reaches it via the same compose-multiplicatively decomposition
they use, adapted to B++'s single-pass architecture.

---

## Sources (industry research, 2026-05-22)

GCC:
- "Auto-vectorization in GCC" — Dorit Nuzman et al, GCC
  Developers Summit 2003
- GCC source `gcc/tree-vect-*.c`

LLVM:
- LLVM Loop Vectorizer — `llvm/lib/Transforms/Vectorize/`
- LLVM Dev Meeting talks 2013-2020

ISPC:
- Matt Pharr & William Mark, "ISPC: A SPMD Compiler For
  High-Performance CPU Programming", InPar 2012

Halide:
- Ragan-Kelley et al, "Halide: A language and compiler for
  optimizing parallelism, locality, and recomputation in
  image processing pipelines", PLDI 2013

Foundational:
- Allen + Cocke (1972) on optimization transformations
  (extended in Aho/Lam/Sethi/Ullman Chapter 11)
- David Padua et al, "Vector and Parallel Languages"
  surveys

B++ anchor files (foundation this sidequest extends):
- `src/bpp_defs.bsm:227` — `SL_DOUBLE = 11`
- `src/backend/chip/aarch64/a64_enc.bsm` — NEON encoders
- `src/backend/chip/x86_64/x64_enc.bsm` — SSE2 encoders
- `src/backend/c/bpp_emitter.bsm` — C transpile path (the
  Tier-A target)
- `src/bpp_dispatch.bsm` — smart-dispatch (Tier D
  composition target)
- `docs/journal.md` 2026-04-15 entry — Phase B4 initial
  ship
- `docs/manual/tonify_checklist.md` Rule 38 — implicit
  vs explicit parallel (the doctrine autovec inherits)
