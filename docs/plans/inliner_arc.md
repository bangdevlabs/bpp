# Plan — the Inliner Arc (codegen Frontier 1)

**Goal:** close the **×3.7 inliner gap** the tiny_lofi DAW exposed (16.6 ms
`tl_render` vs 4.5 ms hand-inlined vs 2.2 ms clang -O2; see benchmarks.md). Real
call-heavy programs pay a per-call tax that the tight microbenchmark kernels hid.
Frontier 2 (RegAlloc v2, the residual ×2) follows this arc.

## The starting point (2026-06-18)

The **active** inliner (Phase B2, `classify_inlineable` in `bpp_dispatch.bsm`)
only inlines **leaf single-return** functions and rejects, in stacked criteria:
float params/returns (6081/6084), multi-value-return (6056), any `T_CALL` in the
body (6093), control flow (node-count 99), >3 params. A **cost-model inliner
(S4)** is built but **dormant** (`_inline_cost`/threshold exist; the consumer
`classify_inlineable_v2` was never wired).

Our DAW hot path is blocked everywhere: `flt_onepole_tick` (float),
`moog_taps` (float + multi-return), `moog_slope` (float + switch + call),
`tl_channel_process` (float + if + call), `arr_struct_at` (if + 2 returns).

## Lesson from the first attempt (2026-06-18) — gate before you relax

The first Inc 1 *relaxed eligibility* (let peek/poke wrappers inline) WITHOUT a
hotness gate. It was correct + byte-stable, but a measurement of the general
benchmark (the user's question) caught the trap: the active inliner inlines an
eligible function at **every** call site, and peek/poke wrappers are ubiquitous,
so the **compiler binary grew +132 KB (+13%)** with **no compile-time win**
(0.26 → 0.29 s, flat) — bloating hundreds of cold sites to speed up a few hot
loops (+3% DAW only). **Reverted.** Eligibility relaxation only pays at HOT call
sites; without the gate it's a net loss. So the gate comes first.

## Stages (each: bootstrap byte-stable + 192/0/12 + 155/0/49 + tl_bench + binary-size + audio md5)

Two gating mechanisms, used where each fits: a **size gate** (a thin body that's
size-neutral to inline can go everywhere) and a **hotness gate** (a bigger or
widely-used body inlines only at hot — in-loop — call sites). The size gate is
cheap and lands first; the hotness gate (per-call-site loop-depth) is built when
the first bigger/widely-used target needs it.

- **Inc 1 — size-gated inline-instruction builtins. ✅ DONE.**
  `_inline_is_inst_builtin` (peek/poke family) + a relaxed `_inline_has_tcall`
  (builtin calls don't disqualify a wrapper) + a tight `INLINE_BUILTIN_NODE_CAP`
  (10) so ONLY thin single-instruction wrappers qualify. read_u16/write_u16 now
  inline (tl_render 6 bl/iter → 3); DAW 16.7 → 15.9 ms (~4.6%); **binary flat**
  (998386 → 998482, +96 B vs the +132 KB of the first attempt). Audio
  byte-identical; 192/0/12 + 155/0/49; compile-time unchanged.

- **Inc 2 — float leaf single-return inlining** (matched types, no coercion).
  Unblocks `flt_onepole_tick` and float leaves. Thin → size gate covers it.

- **Inc 3 — control flow (T_IF) in single-return bodies + the HOTNESS GATE.**
  Unblocks `arr_struct_at` — the **biggest single call source** (~3.17M/render)
  but a *widely-used* accessor, so inlining it everywhere would bloat → this is
  where the per-call-site loop-depth gate gets built (inline at hot sites only).

- **Inc 4 — bottom-up + relax the T_CALL gate** (reverse-topo order, recursion
  guard) so a call to an already-inlinable function stops disqualifying its
  caller. Collapses the chain. Hotness-gated.

- **Inc 5 — multi-value-return splice.** Inline `moog_taps` (4 banked returns →
  4 result locals). Completes the filter-chain collapse.

**Target:** ~16.6 → ~4.5 ms (the hand-inlined `tl_bench_flat` number). The
remaining 4.5 → 2.2 ms is Frontier 2 (RegAlloc v2 / liveness, roadmap F.2).

## Discipline

Same as the codegen journey: one increment per commit; **the compiler is the
test harness** — `tl_bench` (perf) + `tiny_lofi --render` md5 (correctness) +
the two suites + byte-stable bootstrap, every step. Use `the_bug` (`--disasm` /
`--break`, see `docs/manual/debug_with_bug.md`) when a step regresses
byte-stability. Measure, don't believe.
