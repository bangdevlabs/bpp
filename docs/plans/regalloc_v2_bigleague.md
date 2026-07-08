# Plan — RegAlloc v2 + caller-saved spilling (big-league general codegen)

**Opened 2026-07-08.** The goal is NOT faster hot loops — a measured study (below)
proved the performance-critical loops are already optimal. The goal is
**professional general-code register allocation**, matching the mature cousins
(gcc/clang/rustc/Jai/Zig) on the code that ISN'T a tight leaf loop.

## The measured driver (this is not speculation)

Instrumented the B3 selection to count, per function, hot int locals
(loop-weighted refs ≥ 1000) that overflow the promotion budget (a64: 10, x64: 5)
into memory. Compiled real workloads:

| workload | non-leaf functions with loop-hot overflow | worst |
|---|---|---|
| the compiler itself | 69 | synthesize_loop_fn (36), cg_emit_stmt (26) |
| fps_3d_cpu (a game) | 37 | asset-load, UI/text (render_text 17) |
| moog_demo (audio) | 19 | wav_decode, font/ttf loaders |

**Decisive finding:** the core hot loops — the 2.5D ray-cast per-column
(`stbraycast`), the DSP per-sample filter — are ABSENT from the overflow list.
They are leaf/tight and fit the budget. The overflow lives in **non-leaf general
code**: asset loading (runs once), UI/text rendering (per-frame but not the
bottleneck), and the compiler's own functions (build-time). `render_text` alone
has **117 frame-slot accesses out of 356 loads/stores** — spill-heavy, but it is
text rendering, not the pixel loop.

So: b++'s design is correctly focused for the hot path (NOT amateur there), and
measurably mediocre on general non-leaf code. RegAlloc v2 closes THAT gap. It is a
big-league general-quality investment, not a hot-path fix.

## What exists today

- **B3 promotion**: greedy, promotes the top-N hot int locals into callee-saved
  regs (survive calls). Budget = callee-saved pool.
- **Freelist**: caller-saved regs used ONLY for expression-scoped temporaries
  (never live across a call).
- **RegAlloc v2 (partial)**: `regalloc_linear_scan` + `regalloc_compare_vs_b3`
  exist (liveness → linear scan, shares one reg across non-overlapping live
  ranges, sees through inlined calls). a64 wires `regalloc_apply` /
  `regalloc_apply_float`; **x64 `_x64_regalloc_apply` is a `return 0` stub**.
- **Overflow → memory**: hot locals beyond the budget sit in the frame,
  loaded/stored each use.
- **`save_caller_saved_int` / `restore_caller_saved_int`**: empty stubs,
  reserved for exactly this arc (the caller-saved spill set around a call).

## The design (two mechanisms)

1. **Global allocation (linear scan) on both backends.** Activate the existing
   linear-scan comparison on x64 (the stub) so a register is shared across
   non-overlapping live ranges → more locals fit without widening the pool.
2. **Caller-saved spilling.** When a hot cross-call value can't get a
   callee-saved reg (pool exhausted), put it in a CALLER-saved reg and spill it
   around the calls it crosses (`save_caller_saved_int` before the call,
   `restore` after), instead of leaving it in memory. Wins when the value is used
   many times BETWEEN calls (register) and crosses FEW calls (few spills) — a
   cost model decides: `crossed_calls × spill_cost` vs `callee-saved prologue
   cost` vs `memory-access cost`.

## Increments

- **M1 — activate linear-scan on x64. ✅ DONE (2026-07-08, `a429a63`).** Filled
  `_x64_regalloc_apply` (mirror `_a64_regalloc_apply`) + `_x64_b3_reg_at` + a
  `regalloc_int_budget` chip primitive (the scan had run with a64's hardcoded
  10/14 budget; x64 needs 5). a64 byte-identical (same budget), x64 self-host
  stable. MEASURED on the x64 gen1: `emit_node` frame accesses 676 → 137 (−80%),
  `val_check_node` 258 → 92 (−64%); `cg_emit_stmt` declined by the gate (correct
  fallback). Register-sharing across non-overlapping live ranges is the win.
- **M2 — caller-saved spill set.** `cg_emit_call` computes the live caller-saved
  set at each call site and calls `save/restore_caller_saved_int`. Implement the
  primitives (push/pop the spill set). Gate on a value actually being promoted to
  a caller-saved reg across a call.
- **M3 — the cost model.** Extend B3/RegAlloc to CHOOSE caller-saved (with
  spilling) vs callee-saved vs memory per value, by the measured trade-off. This
  is where the general-quality win lands.
- **M4 — float.** Same for float cross-call values (x64: no callee-saved xmm, so
  caller-saved spilling is the ONLY way to keep a float live across a call in a
  register — this also finally gives x64 cross-call float promotion, closing the
  one "architectural" float gap through spilling instead of callee-saved regs).

## Gate + measurement per increment

Self-host byte-stable (gen2==gen3) both backends, suites both, catalog re-run,
zero warnings. THE measurement is the pressure study re-run: overflow-function
count and per-function overflow must DROP. Disasm a representative non-leaf
function (render_text, cg_emit_stmt) before/after and count the frame-slot
accesses eliminated. Hot-loop benchmarks (conv, raycast) must NOT regress (they
don't overflow, so they should be untouched — a guard against collateral damage).

## Honest scope note

This is a multi-session arc touching the allocator core. It does NOT speed up the
DSP/raster hot loops (already optimal). Its value is big-league general codegen
quality + (via M4) the one remaining float-parity gap on x64. Sequence it as a
deliberate arc, not a quick win.
