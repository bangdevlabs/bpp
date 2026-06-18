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

## Stages (each: bootstrap byte-stable + 192/0/12 + 155/0/49 + tl_bench + audio md5)

- **Inc 1 — inline-instruction builtins don't disqualify. ✅ DONE.**
  `peek`/`poke` family lower to ONE load/store, but the AST holds them as
  `T_CALL`, so wrappers like `read_u16{return peek_q(b+o);}` were rejected.
  `_inline_is_inst_builtin` + a relaxed `_inline_has_tcall` fix it. read_u16 /
  write_u16 now inline (tl_render: 6 bl/iter → 3). 16.6 → 16.08 ms. Unblocks
  every peek/poke wrapper codebase-wide. Audio byte-identical.

- **Inc 2 — float leaf single-return inlining** (matched types, no coercion).
  Unblocks `flt_onepole_tick` and float leaves. Won't move tl_bench alone (chain
  still calls), but is the foundation for the chain collapse; verify on a focused
  float-leaf microbench.

- **Inc 3 — control flow (T_IF) in single-return bodies.** Unblocks
  `arr_struct_at` — the **biggest single call source** (~3.17M/render: the clip
  loop re-scans every clip × channel × frame). Largest expected DAW move.

- **Inc 4 — bottom-up + relax the T_CALL gate.** Inline in reverse-topological
  call-graph order so leaves go first and a call to an *already-inlinable*
  function stops disqualifying its caller. Needs a **recursion/cycle guard**
  (never inline a function reachable from itself). Collapses the chain.

- **Inc 5 — multi-value-return splice.** Inline `moog_taps` (4 banked returns →
  4 result locals at the call site). Completes the filter-chain collapse.

**Target:** ~16.6 → ~4.5 ms (the hand-inlined `tl_bench_flat` number). The
remaining 4.5 → 2.2 ms is Frontier 2 (RegAlloc v2 / liveness, roadmap F.2).

## Discipline

Same as the codegen journey: one increment per commit; **the compiler is the
test harness** — `tl_bench` (perf) + `tiny_lofi --render` md5 (correctness) +
the two suites + byte-stable bootstrap, every step. Use `the_bug` (`--disasm` /
`--break`, see `docs/manual/debug_with_bug.md`) when a step regresses
byte-stability. Measure, don't believe.
