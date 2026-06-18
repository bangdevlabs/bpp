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

- **Inc 1 — wire the S4 cost-model hotness gate.** Move the inline decision from
  the callee-level `fn_inlineable` flag to a **per-call-site** decision: inline
  only when the call site is hot (in a loop / hot caller) per the dormant S4
  threshold (`bpp_dispatch.bsm` ~5640: base 30, ×2 hot caller, ×1.5 in-loop).
  Foundation: makes ALL later relaxations pay only where they help, no bloat.
  Verify the binary size does NOT grow on the existing inline set.

- **Inc 2 — inline-instruction builtins, now hotness-gated.** Re-add
  `_inline_is_inst_builtin` + relaxed `_inline_has_tcall`, but inlining now fires
  only at hot sites. read_u16/write_u16 inline in the DAW loop, NOT codebase-wide.
  Expect the DAW win WITHOUT the binary bloat.

- **Inc 3 — float leaf single-return inlining** (matched types, no coercion).
  Unblocks `flt_onepole_tick` and float leaves.

- **Inc 4 — control flow (T_IF) in single-return bodies.** Unblocks
  `arr_struct_at` — the **biggest single call source** (~3.17M/render).

- **Inc 5 — bottom-up + relax the T_CALL gate** (reverse-topo order, recursion
  guard) so a call to an already-inlinable function stops disqualifying its
  caller. Collapses the chain.

- **Inc 6 — multi-value-return splice.** Inline `moog_taps` (4 banked returns →
  4 result locals). Completes the filter-chain collapse.

**Target:** ~16.6 → ~4.5 ms (the hand-inlined `tl_bench_flat` number). The
remaining 4.5 → 2.2 ms is Frontier 2 (RegAlloc v2 / liveness, roadmap F.2).

## Discipline

Same as the codegen journey: one increment per commit; **the compiler is the
test harness** — `tl_bench` (perf) + `tiny_lofi --render` md5 (correctness) +
the two suites + byte-stable bootstrap, every step. Use `the_bug` (`--disasm` /
`--break`, see `docs/manual/debug_with_bug.md`) when a step regresses
byte-stability. Measure, don't believe.
