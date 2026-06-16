# B++ Speedup Roadmap

This document collects every "make B++ faster" effort under one roof.
It started life as the Tier F (heavy compiler passes) spec, then
evolved as profile data accumulated. Three tier families now coexist:

| Tier | What it targets | Status |
|---|---|---|
| **D** (single-node parser opts) | Strength reduction, identity peephole, inline trivials. Shipped, parser-level. | DONE 2026-04-22 |
| **S / M / Z** (runtime + instrumentation) | Worker-pool overhead, allocator hot path, intra-frame zone breakdown. Pointed at by profile.txt. | OPPORTUNISTIC SIDEQUEST (next) |
| **F** (multi-node codegen passes) | CSE, register allocator v2, auto-vectorization. Requires IR / CFG / liveness infra. | DEFERRED (gated on profile) |

The flow is profile-driven: whatever the sampling profiler points at,
that's what opens next. Today (2026-05-14) it points at Tier S/M, not
Tier F.

## Context

B++'s compiler is a tree-walker with no IR. Waves 18-21 moved the
tree walker and statement dispatcher into the spine (`bpp_codegen.bsm`),
unblocking optimizations that previously had to be duplicated per chip
or weren't feasible at all.

Phases D-1 through D-4 (2026-04-22) landed the **single-node**
optimizations that fit the tree-walker model: strength reduction,
identity peephole, inline trivials for `buf_new` / `buf_new_w`, and
infrastructure cleanup (buf_fill / buf_copy / read_u*be across stb).

Tier F collects the **multi-node** optimizations — passes that require
analyzing relationships BETWEEN AST nodes rather than rewriting a
single binop at parse time. These are genuinely large projects; this
document scopes them honestly so the roadmap is visible without
pretending they fit into a single session.

## Why Tier F Is Deferred (not skipped)

The user feedback on the 2026-04-22 plan was explicit: "Tier F needs
profile data before commitment." Without a workload showing scalar
math as its bottleneck, Tier F optimizations are speculation. Once a
real workload points at one specifically, its project opens.

**Profile data now exists** (sampling profiler shipped Phase 6.2,
`@profile` zones shipped Phase 6.3). The most recent capture
(`profile.txt`, wolf3d run) shows:

```
1577 _job_worker_main      ← scheduler / worker loop
 825 maestro_run           ← frame orchestrator
 164 _mem_alloc_pages      ← allocator
   3 _stb_gpu_vertex
   2 write_u16
   1 _stb_gpu_flush  _wolf3d_tex_for  sin_f  abs_f  tile_get
```

None of the three Tier F passes are justified by this data:

| Pass | Would need | Profile shows | Status |
|---|---|---|---|
| F.1 CSE | Subexpressions repeated in hot loops | `sin_f` / `abs_f` / `tile_get` = 1 sample each | ❌ not justified |
| F.2 Reg-alloc v2 | Spill / reload pressure dominant | No load/store sequence in the ranking | ❌ not justified |
| F.3 Auto-vec | Scalar inner loops dominant | `write_u16` = 2 samples, `_stb_gpu_vertex` = 3 | ❌ not justified |

The plan below stays the spec for when a future workload DOES surface
one of those bottlenecks. Today's bottlenecks live in Tier S / M
(below).

## F.1 — CSE (Common Subexpression Elimination)

### What it does

`(a + b) * (a + b)` computes `a + b` once, reuses the result. Applies
to any expression tree where a subexpression appears more than once
and its value doesn't change between occurrences (pure expressions
of non-mutating variables).

### Why it's hard in B++

Tree-walker compilers normally walk each occurrence independently. To
CSE, we need:

1. **Structural hash of AST subtrees** — two T_BINOP nodes with same
   operator, same LHS, same RHS hash identically regardless of memory
   location. Need a stable hash that ignores node pointers, hashes
   over semantic content (operator, operand hashes, recursive).
2. **Value numbering per scope** — map from hash → first-occurrence
   offset. When a repeat is detected, emit a load from the cached
   slot instead of re-evaluating.
3. **Escape analysis** — a subexpression is CSE-safe only if none of
   its inputs are mutated between occurrences. For B++ that means:
   - No assignment to any variable in the subexpression
   - No call between occurrences that could mutate via aliasing
4. **Scratch slot management** — where does the cached value live?
   - Register if free (complicates register allocator)
   - Stack slot if no reg (requires frame-size bookkeeping)
   - Need to choose before emission of the FIRST occurrence

### Scope estimate

- Hash function + caching: 80 lines
- Scope tracking + value numbering: 150 lines
- Mutation detection: 200 lines (needs AST walker that tracks writes)
- Emission coordination (scratch slots, reg choice): 150 lines
- Testing + debugging edge cases: hard to estimate — CSE is notorious
  for correctness bugs in aliased structs, nested calls, etc.

**Total: 2-3 weeks.**

### Prerequisite

- Wave 21 real takeover (DONE — `cg_emit_node` owns the walker)
- AST node augmentation: each Node needs a field for "CSE slot index"
  (or a side table keyed by node pointer)

### When to unblock

- Profile of a real plugin shows scalar-arithmetic hot path
- OR: game code audit finds common patterns of
  `sqrt((x-cx)^2 + (y-cy)^2) ...` or similar repeat-heavy patterns

### Expected gain

- 5-15% on math-heavy code (physics, geometry)
- 0% on I/O- or allocation-bound code

## F.2 — Register Allocator v2 (SSA-lite + liveness)

### Phasing (added 2026-06-13 — grounded in measurement)

> **✅ OUTCOME (2026-06-14): F.2.c SHIPPED in `c099438` (1.27× on the
> biquad). Label reconciliation — read this first, the original a/b/c
> premise was overturned by measurement:**
> - **F.2.c (compute-in-place)** = the real lever = **DONE.** Shipped as
>   "Stage 1": a spine path emits a float `+ - * /` tree straight into its
>   destination registers, no x0/d0 shuttle. Worth ~0% *alone* (the shuttle
>   it removes is store-forwarded / off the critical path) but it is the
>   foundation the next item needs.
> - **F.2.a (float promotion)** = rejected ALONE (12.7% slower, below) but
>   **REVIVED on the F.2.c foundation** as "Stage 2" — operands are now read
>   directly from promoted d8..d15 registers, no offsetting fmov, and it
>   WINS. The two are synergistic: neither alone helps, together = 1.27×.
> - **F.2.b (loop-weight)** = **skipped** (moot).
>
> So we did F.2.c + a working F.2.a, not "F.2.a + F.2.b". The narrative
> below is the measurement trail that led here — still true, kept for the
> record. Remaining gap to `gcc -O1`: only 8 of 12 floats fit d8..d15, and
> the int loop control is still accumulator-shaped (a separate lever).

A 2026-06-13 study measured the real codegen gap (see
`docs/manual/bootstrap_manual.md` "Codegen Quality and the
Register-Allocation Frontier"): b++ native is ~on par with `gcc -O0`,
and ~2.1× behind `gcc -O2` on serial-dependent scalar loops, the gap
dominated by register allocation. A biquad IIR kernel split that gap:
**~half is float binop intermediates** (fixed in `c54a19b` — the float
path now uses the SIMD freelist, 1.86×) and **~half is local-variable
frame traffic** (this section). Crucially, the study found that the
"promote hottest locals to callee-saved registers" machinery **already
exists** — `_a64_b3_select()` ranks vars by `cg_var_refs` and promotes
the top 6 to `x19..x24`. So F.2 is NOT green-field; it is three slices
from cheap-and-sure to big-and-precise:

- **F.2.a — extend B3 to FLOAT locals. ❌ TRIED, MEASURED, REVERTED
  (2026-06-13).** Implemented end to end — a second B3 selection pass to
  `d8..d15`, float-aware load / store / zero-init / arg-copy, and prologue
  save/restore — and it is **12.7% SLOWER** on the biquad (1.081s →
  1.218s, min-of-8, identical output) despite cutting the 34 float frame
  loads/stores per sample to **zero**. Why: the `ldr/str d, [x29]` it
  removes are **store-forwarded** (a load from a just-written stack slot
  is satisfied from the store buffer in ~0–1 cycles), so they were nearly
  free; the `fmov d<reg>` that replaces them costs an **FP port**, and a
  float-heavy DSP loop is already FP-port-bound, so it piles onto the
  bottleneck. **Counting frame accesses overstated their cost; measuring
  time corrected it.** (Same lesson as the step-1 residual-fmov removal,
  which was also slower.) Reverted; the consumer-site list it surfaced
  (load, store, zero-init, arg-copy-flt all read `cg_var_promote`) is kept
  here in case a future, port-aware version revisits it.
- **F.2.b — loop-weight the ref count.** Moot: the float case (F.2.a) is
  rejected, and int register-caching (B3) already removes its frame
  traffic, so re-ranking which locals win the budget does not touch the
  real gap (below).
- **F.2.c — compute-in-place (the original live-interval design below).**
  The **only** lever that addresses the real gap. The big arc.

**The gap is the accumulator MODEL, not frame traffic (corrected
2026-06-13 by measurement).** The earlier "two gaps" framing was wrong:
frame traffic is store-forwarded and nearly free, so removing it (F.2.a)
does not help — and trading it for FP-port `fmov` hurts. The entire ~2×
to -O2 is the **accumulator shuttle**: every op loads its operands into
x0/d0, computes, then stores the result back, emitting ~2–3× the ops
-O2 needs. -O2 wins by **computing in place** (`fadd d_dst, d_a, d_b`
with the operands already in their registers) — fewer ops on the
bottleneck port — NOT by "keeping values in registers" (b++ already does
that via B3 + the freelist; promoting MORE values, as F.2.a showed, can
even lose).

So the honest conclusion: **there is no cheap slice.** Whole-function
register promotion (F.2.a/b) cannot close the gap, because the gap is not
where the values live — it is the accumulator evaluation order. Only
F.2.c (ops writing their destination register directly, no x0/d0
shuttle) addresses it, and that is the big arc — it replaces the
accumulator model, not a heuristic tweak.

**Integer F.2.c — Stages 1 + 2a + 2b ALL SHIPPED (2026-06-16,
`86a7bab`→`5483f51`).** The whole arc landed:
- **Stage 1 (CIP, `86a7bab` + `7e7279d`):** `cg_emit_int_into` (a64) emits
  `+ - * & | ^` integer trees over local / constant / full-word memory-load
  (`arr[i]`) leaves destination-driven, killing the x0 shuttle. Gated
  `cg_int_tree_need <= int_temp_count` (GP freelist x9..x15). Division /
  shifts / sub-word loads stay on the accumulator path (signedness /
  extension the type system owns).
- **Stage 2a (constant promotion, `64648fe`):** loop-weighted constant table
  (a loop constant counts 1000x) → `_a64_b3_select_const` promotes the
  hottest to spare x19..x24, materialised once in the prologue, read by all
  three use-sites (CIP leaf, CIP iconst, accumulator literal). Reuses the
  proven a64_promoted_regs save/restore + spill machinery.
- **Stage 2b (param caching, `5483f51`):** loop-weight the *local* ref count
  in cg_b3_walk so a loop-invariant param (array base, loop bound) used once
  per iteration clears B3's promotion threshold instead of reloading from the
  frame. Backend-agnostic (both a64 + x64 benefit).

Cumulative, warm: lcg 24.3 → 20.8ms (gap to gcc -O2 1.35x → **1.14x**);
throughput xform 22.9 → 14.6ms (3.71x → **2.35x**). The xform's residual gap
is gcc's auto-vectorisation/unroll of the int64 kernel (F.3, separate
PhD-level lever) — NOT the shuffle, which the arc removed. Each stage:
byte-stable 1-cycle bootstrap, native 192/0/12, C-emit 155/0/49, x64
self-host stable, compile-time 0.25s unchanged. **x64 integer-CIP parity
deferred** (SysV's 1-deep GP freelist makes destination-driven trees
register-pressure-bound — only ≤1-temp trees would fire; like the float
CIP's x64 opt-out). x64 still gets Stage 2b via the shared B3. The
decomposition below is the original measurement that drove all of it.

**Integer F.2.c — the throughput measurement (2026-06-16).** The float
F.2.c (`cg_emit_float_into`) shipped; the INTEGER accumulator shuttle is
still live, and a new committed benchmark proves it dominates. `examples/
bench_codegen.bpp` adds a throughput-bound `xform` kernel
(`out[i] = (in[i]*A + C) >> 17` over a cache-resident array): native
**3.71× behind gcc -O2** (22.9ms vs 6.2ms), versus only 1.35–1.52× on the
serial biquad/lcg where OoO hides the shuttle. A controlled isolation
experiment (big-vs-small constants; params-vs-locals) decomposed the
throughput gap:
  - accumulator shuffle (the `add x0,r,#0; mov x9,x0` x0 funnel): **~58%**
  - loop-invariant 64-bit constant rematerialisation (mov+movk/iter): **~29%**
  - param reloads from the frame each iteration: **~13%**
So neither constant nor param hoisting is the dominant lever — the x0 funnel
is. Order: integer-CIP FIRST (mirrors the float arc — Stage-1 CIP is the
foundation Stage-2 promotion reads from, and unlike floats it is NOT
OoO-hidden so it wins standalone), then constant promotion + param caching as
the integer Stage-2. Build: `cg_int_leaf_kind` / `cg_int_tree_need` /
`cg_emit_int_into` mirroring the float trio; chip primitives `emit_iop_into` /
`emit_iload_var_into` / `emit_iconst_into` drawing temps from the existing GP
freelist (x9..x15, `a64_gp_alloc`). Staged by leaf shape: local+constant
leaves first (helps lcg) → memory-load leaves + shift ops (helps xform).
Gated by `bench_codegen` (biquad/lcg/xform vs gcc -O2) + byte-stable
bootstrap + Linux x64, each increment.

Why step 1 (the SIMD freelist, `c54a19b`) WAS a win where F.2.a was not:
it removed the **value-stack** push/pop (`str/ldr d, [sp, #-0x10]!` with
the pre/post-index SP write), genuinely more expensive than a
store-forwarded frame slot, and added no offsetting `fmov` on the hot
path. The rule: target the expensive memory ops (stack push/pop, the
shuttle), never the cheap ones (store-forwarded frame slots).

Each slice is gated the same way: bench the biquad + the serial LCG loop
against the `--c` → `gcc -O2` oracle, confirm byte-stable bootstrap +
full suite + Linux x64 (B3 touches both backends' var access and
prologue). Eligibility rules are load-bearing for correctness: a local
whose address is taken (`&x`) CANNOT be promoted (it must have a frame
slot) — `cg_b3_eligible` already encodes this for the GP path; the float
pass must honour the same exclusions (address-taken, struct, sliced).

### What it does

Replace the current B3 "count refs + promote hottest N" heuristic
with a **live-interval tracker** that assigns registers based on when
variables are alive across basic blocks, not just total reference count.

### Why it's hard in B++

B++ today has:
- Single-pass walker with no block structure explicit in AST
- B3 does a pre-pass to rank variables, then promotes up to N of them
- Spills happen ad-hoc when a call sequence needs specific registers

A v2 allocator needs:

1. **Explicit CFG** — basic blocks with edges. Currently B++ walks
   T_IF / T_WHILE / T_SWITCH recursively; converting to a flat CFG is
   a significant AST-to-CFG pass.
2. **Liveness analysis** — for each basic block, which variables are
   live-in, live-out. Classic dataflow algorithm (iterate until fixed
   point).
3. **Live intervals** — flatten per-variable liveness into time
   ranges over the linear emission order.
4. **Linear-scan allocation** (Brooks algorithm) — walk intervals,
   assign registers, spill when out of free regs.
5. **Coalescing** — avoid copy moves between variables that don't
   live simultaneously.

### Scope estimate

- AST-to-CFG conversion: 300 lines
- Liveness analysis: 150 lines
- Live-interval construction: 100 lines
- Linear-scan allocator: 200 lines
- Spill code emission rewrite: 150 lines
- Integration with existing chip primitives: 100 lines
- Testing against existing code (MUST produce identical semantics, not
  necessarily identical bytes): extensive

**Total: 3-4 weeks.**

### Prerequisite

- Wave 21 DONE
- F.1 CSE nice but not required
- AST node augmentation: variable references need stable IDs, not
  just name-packed strings

### When to unblock

- Profile shows stack traffic (spill/reload) as a significant fraction
  of hot-loop time
- OR: a specific workload (RTS with 1000 units, dense ECS) demands
  tighter register reuse

**Partially unblocked 2026-06-13:** the biquad measurement above IS that
profile — frame traffic is ~half the measured ~2× gap on arithmetic-bound
float loops, and the audio/DSP direction is the workload that demands it.
Start at F.2.a (extend B3 to float); F.2.c (the full live-interval
allocator below) stays gated until a/b are measured and a real gap
remains.

### Expected gain

- 10-25% on register-pressure-heavy code (physics, transforms)
- 5-10% on typical game code
- Negligible on I/O-bound or trivially-allocated code

## F.3 — Auto-Vectorization

### What it does

Detect loops whose body operates independently on sequential array
elements and emit SIMD operations (NEON 128-bit on aarch64, SSE2/AVX
on x86_64) that process 4 elements at once.

### Why it's hard (PhD-level)

1. **Dependency analysis** — a loop body may or may not have
   cross-iteration dependencies. `arr[i] = arr[i-1] + 1` can't be
   vectorized (each element depends on previous). `arr[i] = arr[i] *
   2` can. Detection requires SSA-style analysis or explicit
   dependency graph.
2. **Alignment reasoning** — SIMD loads prefer 16-byte-aligned
   addresses. B++ buffers aren't guaranteed aligned. Need either:
   - Proof that the buffer is aligned (analysis)
   - Runtime alignment prologue (process first few elements scalar
     until aligned)
   - Use unaligned SIMD loads (slower but always correct)
3. **Vectorizable operation set** — not every scalar op has a SIMD
   equivalent. Branches, function calls, type conversions need
   fallback to scalar.
4. **Epilogue for remainder** — if loop count isn't a multiple of 4,
   the last 0-3 elements need scalar handling.
5. **Chip-specific codegen** — NEON and SSE2 have different
   instruction shapes. Needs a mini-vectorization-IR that both chips
   can consume.

### Scope estimate

Honestly: **2-3 months full-time.**

### Prerequisite

- F.1 CSE + F.2 Reg allocator v2
- `vec_*` primitives already present (NEON + SSE2 families)
- AST loop detection working
- Profile data showing scalar inner loops as the bottleneck

### When to unblock

The only realistic trigger: a real-time DSP plugin (convolution reverb,
FFT-based EQ, physical modeling synth) where the user DEMONSTRATES
that scalar B++ code is 4× slower than a hand-SIMD'd equivalent. Even
then, the escape hatch of writing `vec_*` intrinsics by hand
(currently available) is usually more efficient per-developer-hour than
making the compiler auto-vectorize.

### Expected gain

- 2-4× on numerically-dense inner loops that fit the model
- 0× on control flow, allocation, I/O
- Negative (code bloat) if applied to loops that don't meet the
  criteria

## Tier S — Scheduling overhead (OPPORTUNISTIC NEXT)

The 2026-05-14 profile capture has `_job_worker_main` at 1577 samples
and `maestro_run` at 825 — together ~65% of the top-10. That is
scheduler / orchestrator overhead, not codegen quality. Three concrete
sub-investigations, each small enough to fit one sidequest session:

### S.1 — Worker idle vs dispatch breakdown

Wrap `_job_worker_main`'s body in two `@profile` zones:
`worker_idle` (the spin / wait branch when no job is ready) and
`worker_dispatch` (the actual work invocation). A wolf3d run with HUD
zones up shows which dominates.

- If idle dominates → workers starve for jobs; the right fix is
  either coarser-grained dispatch (larger chunks per `job_parallel_for`)
  or fewer workers in default config.
- If dispatch dominates → real work is happening but the per-job
  overhead is the cost. Goes to S.2 / S.3.

**Cost:** ~10 LOC instrumentation + 1 capture. No codegen change.

### S.2 — Memory barrier audit

`bpp_job` brackets every cross-thread queue op with full
`mem_barrier` (DMB ISH on ARM64, MFENCE on x86_64). That was an
explicit conservative choice (`multicore_state_report.md` §1)
to avoid acquire/release subtle bugs. If S.1 shows dispatch
overhead is hot, the next question is whether the full fences are
the cost — relaxing to LDAR/STLR (ARM64) and load-acquire /
store-release fences (x86_64) drops cycles per op.

**Cost:** ~30 LOC across `a64_primitives.bsm` /
`x64_primitives.bsm`, plus paranoid validation (stress test under
sanitizer-equivalent). The acquire/release bug class is real;
worth doing only if S.1 confirms the cost.

### S.3 — Submit contention

`job_submit` round-robins between workers via a shared counter.
Under heavy submit traffic (per-frame dispatches in `base` phase)
the counter can become a contention point. Mitigations:

- Per-producer index, only one producer (main thread): no
  contention at all. Already true for `job_parallel_for`. May
  not be true for `job_submit` if called from multiple sites.
- Thread-local "next worker" hint, reconciled lazily.

**Cost:** ~20 LOC. Lowest priority of the three — only matters if
S.1 + S.2 don't close the gap.

## Tier M — Allocator hot path (OPPORTUNISTIC NEXT)

`_mem_alloc_pages` at 164 samples is the third-hottest function in
the wolf3d capture. Steady-state wolf3d (after warmup) should not be
allocating pages — that points at a hidden per-frame allocation.

### M.1 — `_mem_alloc_pages` caller audit

Grep callers of `_mem_alloc_pages` (probably via `_mem_alloc` →
`buf_new` / `arr_new` / `arr_push` grow path). Wrap each suspect in
a `@profile("alloc_<name>")` zone. Re-run wolf3d. Whichever zone
dominates the 164 samples is the leak.

Common suspects:
- `arr_push` triggering grow inside a per-frame loop
- `buf_new` called by an asset loader that should have been
  hoisted out of the frame
- A temp buffer in a hot path that should be a pre-allocated arena
- String building (`buf_word` + `poke`) in HUD draw

### M.2 — Arena reuse audit

`bpp_arena` infrastructure exists (`project_session_20260402`).
Once M.1 identifies the offender, the fix is usually either:
- Move the allocation out of the frame (one-shot init)
- Replace with an arena scoped to the frame, freed at frame end
- Replace with a recycled pool keyed by structure size

**Cost:** M.1 = ~10 LOC instrumentation. M.2 = depends on what
M.1 finds; typically 20-50 LOC per offender.

## Tier Z — Intra-frame zone breakdown (ZERO COST DIAGNOSTIC)

The wolf3d profile capture in `profile.txt` is the FUNCTION top-10
ranking. We don't yet have the `@profile` ZONES capture from the same
run — `ray_cast` / `hud_overlay` / `crt_effect` already exist in the
source but the dump wasn't recorded.

### Z.1 — Capture wolf3d HUD zones

1. Run `./fps_wolf3d`
2. Press `P` to toggle profile HUD (brings up both the function top-N
   panel AND the zone breakdown panel — both gate on
   `_profile_hud_active`).
3. Note the three zone numbers from `profile_zones_hud_draw`.
4. Paste numbers into the issue / journal entry that triggers the
   next sidequest.

That gives a per-zone budget breakdown for the frame: how much of
the wolf3d frame is GPU draw (ray_cast) vs HUD overlay (CPU blits)
vs post-process (crt_effect). Combined with the function top-10,
it tells you whether the scheduler overhead is concentrated in one
zone or spread.

**Cost:** zero LOC. Five minutes of clicking and writing down
numbers.

## Phase D++ kill-list (2026-05-14)

Following Phase D's success (D.1-D.4 shipped 2026-04-22), a tentative
"D++" was floated — more single-node parser optimizations that would
not require IR / CFG. The 2026-05-14 profile data killed four of
them. Recording here so they don't get re-proposed:

| Proposal | What it would do | Profile says | Verdict |
|---|---|---|---|
| **D.5** mul-by-small-constant decomposition (`x*3 → (x<<1)+x`, `x*7 → (x<<3)-x`) | Save 1-2 cycles per `mul` with small literal | `mul` not in top samples | ❌ DROPPED |
| **D.6** signed div/mod by 2^k with negative-correction trick | Save cycles in `sdiv` by power-of-2 | `sdiv` not in top samples | ❌ DROPPED |
| **D.8** algebraic simplification (`(x+a)-a → x`, `~~x → x`) | Eliminate redundant trips | Profile points elsewhere; audit-heavy for signed overflow | ❌ DROPPED |
| **D.9** combine adjacent shifts/masks (`(x<<a)<<b → x<<(a+b)`) | Useful in decoders (PNG/WAV) | No decoder shows up in the ranking | ❌ DROPPED (re-evaluate if a decoder ever appears in the top samples) |

The exception, kept alive for a different reason:

| Proposal | What it would do | Why it stays | Destination |
|---|---|---|---|
| **D.7** unsigned strength-reduction via `: u_word` annotation | Cheap div/mod 2^k when LHS is unsigned (no negative case) | Not a perf opt against current code — a capability of the unsigned-annotation arc | **MOVED** — folds into `sidequest_unsigned_annotations.md` as a natural part of `: u_word` semantics, not a standalone D++ item |

## Recommended Gating

**Tier S / M / Z** (this doc, new) — open opportunistically. Z.1 is
zero-cost and gives signal for S.1 / M.1. Each subsequent step
gates on the previous step's data.

**Tier F** (this doc, original) — do not open until:

1. A real workload exists, runs, and is profiled.
2. The profile shows one of:
   - Common-subexpression waste (F.1 territory)
   - Register spill / reload time (F.2 territory)
   - Scalar-inner-loop dominance (F.3 territory)
3. Someone owns the resulting project end-to-end (design → impl →
   test → integration).

Today (2026-05-14) condition 2 is NOT met for any F-pass. The Phase
D work already captured the single-node wins that don't need
profile evidence; Tier S / M now captures the runtime wins that
the profile DOES point at. Tier F stays parked.

## Related Work

- **Wave 22 candidate: D.4 expansion / E.3 non-trivial wrappers** — if
  Tier F is too big, the smaller next step is expanding D.4 to
  handle functions with locals (via alpha-renaming during AST
  substitution). Wraps `arr_get`, `str_len` style functions. Medium
  project (~2 weeks), big win on wrapper-heavy code.
- **Wave 23 candidate: tail-call optimization** — specialized case of
  no-arg re-call. Useful for recursive descent parsers and functional
  style, not games. Small project (~1 week). Parser first, games
  second.
- **Wave 24 candidate: profile-guided optimization** — instrument
  hot paths, emit feedback, re-compile with hints. Much further down
  the roadmap.

## Updated 2026-04-22

- Phase D-1 through D-4 shipped (bootstrap stable at `72e1b793`,
  suite 111/0 non-GPU).
- Tier F remains explicitly deferred per this document's own gating.

## Updated 2026-05-07

Status snapshot at end of GPU pipeline arc:

- **Tier F still NOT opened** — none of the gating conditions have
  fired. No plugin exists yet (bangscript runtime is the canonical
  near-term plugin host, still ahead on the roadmap). No profile
  data points at scalar-arithmetic dominance. The single biggest
  consumer of compiler optimization wins in the last fortnight has
  been the renderer / GPU pipeline, where Tier F doesn't apply
  (fragment shaders run on the GPU; scalar code in `_stb_*` paths
  is not the bottleneck).
- **Phase D-style single-node opts continue to land opportunistically** —
  e.g. the parser's inline-trivials family expanded for stb wrappers
  during the V3 sprint. Anything reaching the "two-week project"
  threshold for Tier F is still a year away.
- **Adjacent compiler work that DID ship since 2026-04-22**:
  - V3 function-pointer type checking (entire fixpoint pass for
    `func(...)` type propagation across the call graph)
  - Scoped zones `@profile("name") { ... }` annotation (parser
    lowering + runtime + HUD)
  - T_BLOCK pre_reg recursion fix (codegen gap surfaced by
    Phase 6.3)
  - `cd "$REPO_ROOT"` in tests/run_all.sh (env, not codegen)

None of those share infrastructure with Tier F. The CSE / RA v2 /
auto-vec backlog is unmoved. When a real consumer files a profile
showing one of the three patterns, this doc is the spec to start
from.

## Updated 2026-05-14

Profile audit (sampling profiler shipped Phase 6.2 + `@profile`
zones shipped Phase 6.3) revealed:

- **Tier F still parked** — the most recent capture
  (`profile.txt`, wolf3d run) points at scheduler / allocator
  overhead, not scalar codegen. None of F.1 / F.2 / F.3 have
  their gating condition met.
- **Phase D++ killed** — four tentative single-node parser
  extensions (D.5 mul decomposition, D.6 signed div/mod 2^k,
  D.8 algebraic simplification, D.9 combined shifts) explicitly
  dropped against the profile data. Kill-list above documents
  the reasoning so they don't get re-proposed.
- **D.7 (unsigned strength-reduction) re-homed** — it was the
  one D++ item with merit, but its merit is semantic (capability
  for `: u_word` types), not perf. Moved to
  `docs/sidequest_unsigned_annotations.md` as part of the
  unsigned-annotation arc; removed from this doc as a standalone
  item.
- **Tier S / M / Z added** — the three opportunistic sidequests
  that the profile actually points at:
  - **Tier S** (scheduling overhead): S.1 idle-vs-dispatch
    breakdown, S.2 memory barrier audit, S.3 submit contention.
  - **Tier M** (allocator hot path): M.1 caller audit for
    `_mem_alloc_pages`, M.2 arena reuse migration.
  - **Tier Z** (intra-frame zones): Z.1 capture wolf3d HUD
    zones — zero LOC, five minutes, gives signal for the rest.

Title changed from "Tier F — Heavy Compiler Optimization Passes"
to "B++ Speedup Roadmap" to reflect that the doc now tracks every
speedup tier, not just compiler-codegen. The tier table at the
top is the entry point.

**Suggested ordering when bandwidth opens for this work:**

1. Z.1 first (zero cost, gives direction)
2. S.1 + M.1 in parallel — both are ~10 LOC instrumentation,
   each captures a different signal
3. Whichever of S.2 / S.3 / M.2 the data justifies, in priority
   order

Each step is bisect-friendly and ships under its own commit. No
multi-week project until the data demands one.
