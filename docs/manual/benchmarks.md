# B++ Benchmarks — how to run them, and the historical record

This is the single condensed source for every performance benchmark in the
repo: what each one measures, how to run it, what "good" looks like, the latest
numbers, and how the headline metrics have evolved over time.

## Philosophy

- **The oracle is `gcc -O2`.** For codegen quality, emit the same program to C
  (`bpp --c prog.bpp > prog.c`), build it `cc -O2 -w`, and compare against the
  identical self-timed kernel. b++ has no classic optimizer pass; the target is
  to match what a mature scalar compiler picks.
- **Measure the reference, not just our output.** Disassemble gcc's loop
  (`otool -tv` / `objdump`) before deciding what to build — a wrong assumption
  about what the oracle does wastes whole arcs (see the 2026-06-17 entry: the
  xform residual was blamed on auto-vectorisation that did not exist).
- **Min of N runs, same machine, same harness.** Wall-clock on a shared laptop
  is noisy; report the minimum over 5–7 runs and treat ±15% as noise.
- **Every perf claim cites a benchmark + the binary identity.** `bench_compile.sh`
  prints the bpp path / size / mtime so a number can never be silently attributed
  to the wrong binary.

## The catalog

### Compile-time

| Benchmark | Measures | Run | Good |
|---|---|---|---|
| `tests/bench_compile.sh` | self-host bootstrap + small + medium program compile time | `sh tests/bench_compile.sh --runs 5` | bootstrap ≤ 0.5s (per `bootstrap_manual.md`) |

### Codegen quality (runtime, vs gcc -O2)

| Benchmark | Measures | Run | Good |
|---|---|---|---|
| `examples/bench_codegen.bpp` | biquad (FP serial), lcg (int serial), xform (int throughput) — the canonical codegen gate | `bpp examples/bench_codegen.bpp -o /tmp/bcg && /tmp/bcg` | serial near gcc -O2 parity; throughput as low as possible |
| oracle | the same three kernels under gcc -O2 | `bpp --c examples/bench_codegen.bpp > /tmp/b.c && cc -O2 -w /tmp/b.c -o /tmp/o2 -lobjc -lm && /tmp/o2` | — |
| `tools/tiny_lofi/tl_bench.bpp` | a REAL assembled program — the tiny_lofi DAW render (3 s project, call-heavy) | `bpp tools/tiny_lofi/tl_bench.bpp -o /tmp/tlb && /tmp/tlb` | realtime factor ≫ 1× |

### Real-program codegen — the tiny_lofi finding (2026-06-18)

The microbenchmark parity (biquad 1.02×, lcg 1.02×) is measured on **tight,
already-flat kernels**. The tiny_lofi DAW render is the first **call-heavy real
program** measured the same way (`bpp --c` → `cc -O2`), and it does **NOT** hold
parity:

| | per 3 s render | realtime factor | vs clang -O2 |
|---|---|---|---|
| our codegen | 16.9 ms | 177× | — |
| clang -O2 (same source) | 2.3 ms | ~1300× | **7.3× faster** |

Decomposition (`tl_bench_flat.bpp` = the same render hand-inlined, 0 calls):

| variant | per 3 s render | vs clang |
|---|---|---|
| ours, call-heavy (`tl_render`) | 16.6 ms | 7.5× |
| ours, hand-inlined (flat) | 4.5 ms | 2.0× |
| clang -O2 (same source) | 2.2 ms | 1× |

The 7.5× is ~multiplicative: **×3.7 is the inliner** (16.6→4.5, pure call
overhead — disasm shows 6 `bl`/iter for us vs 1 for clang, the whole
`tl_channel_process → moon_process → moog_slope → moog_taps → flt_onepole_tick`
chain plus `arr_struct_at`/`read_u16`/`write_u16`), and **×2.0 is flat-loop
codegen** (4.5→2.2 — register allocation/liveness across a branchy triple-nested
loop; the tight-kernel biquad parity does NOT cover this shape).

**The two frontiers, prioritised by measured impact:**
1. **Inliner (×3.7).** The active inliner (Phase B2) rejects any body with a
   `T_CALL` — leaf-only. The cost-model inliner (S4, `bpp_dispatch.bsm` ~5600)
   is built but dormant (`classify_inlineable_v2` never landed). Wire it +
   bottom-up order (inline leaves first so chains collapse) + relax the
   all-or-nothing `T_CALL` gate + multi-value-return splice.
2. **RegAlloc v2 / liveness (×2.0).** Roadmap F.2. The remaining gap on branchy
   nested loops once the calls are gone.

**Honest takeaway:** "gcc -O2 parity" is a claim about tight inlined kernels,
NOT assembled call-heavy code. The DAW runs comfortably (177× realtime) but the
real benchmark exposed both frontiers cleanly. Measure beat believe — again.

### Autovectorisation + outlining (parallel)

| Benchmark | Measures | Run | Good |
|---|---|---|---|
| `examples/bench_compose.bpp` | outline × autovec compose (1M cells × 100 rounds, 8 workers) | `bpp examples/bench_compose.bpp -o /tmp/bc && /tmp/bc` | COMPOSE ≫ SERIAL, correctness PASS |
| `examples/bench_autovec.bpp` | 4-wide SIMD vs scalar on a float map | `bpp examples/bench_autovec.bpp -o /tmp/ba && /tmp/ba` | speedup where the workload is compute-bound |
| `examples/bench_autovec_compute.bpp` | compute-bound autovec variant | same shape | — |
| `examples/bench_outline.bpp` | outlining (loop → worker) in isolation | same shape | — |
| `tests/bench_autovec_gate.sh` | autovec correctness gate (disasm + run) | `sh tests/bench_autovec_gate.sh` | PASS |
| `tests/bench_mixed_auto.sh` | mixed scalar/SIMD path | `sh tests/bench_mixed_auto.sh` | PASS |
| `tests/bench_simd_raw.bpp` | raw SIMD vs scalar floor | `bpp tests/bench_simd_raw.bpp -o /tmp/x && /tmp/x` | ~3× |

### Engine / data-structure (runtime)

| Benchmark | Measures | Run | Good |
|---|---|---|---|
| `tests/bench_ecs_iter.bpp` | archetype vs legacy ECS iteration | `bpp tests/bench_ecs_iter.bpp -o /tmp/x && /tmp/x` | archetype ≥ legacy |
| `tests/bench_ecs_physics_simd.bpp` | SIMD physics over ECS | same | ~2× |
| `tests/bench_ecs_scheduler.bpp` | 2 systems × 10M ops scheduler | same | PASS |
| `tests/bench_ecs_sparse_query.bpp` | archetype direct walk vs sparse | same | archetype ≫ sparse |
| `tests/bench_stbflow.bpp` | flow-field vs A* pathfinding | same | ~5× |
| `examples/tablah_opt.bpp` | table-processing kernel variants | `bpp examples/tablah_opt.bpp -o /tmp/x && /tmp/x` | — |

## Latest results — 2026-06-17 (after the loop-control arc)

Machine: Apple-silicon laptop (shared, noisy); min of 7–12 runs. The
serial/FP ratios swing with machine load — treat the codegen structure
(verified by disassembly) as the ground truth and the wall-clock as
directional.

**Compile-time** (`bench_compile.sh`): bootstrap **0.25s**, small 0.03s, medium 0.04s.

**Codegen vs gcc -O2** (`bench_codegen`):

| kernel | b++ | gcc -O2 | ratio |
|---|---|---|---|
| lcg (int serial) | 18.9 ms | 18.5 ms | **1.02× — parity** |
| xform (int throughput) | 6.72 ms | 6.10 ms | **1.10×** |
| biquad (FP serial) | 50.1 ms | 48.9 ms | **1.02× — parity** |
| regheavy (register-heavy leaf) | 0.35 s | 0.30 s | ~1.17× |

(best-of-8–12 b++ / best-of-6–10 gcc, same shared laptop). After the loop-
control arc, the two register levers, and the float assign-into-destination
(FP-serial Phase 1), **all three bench_codegen kernels — integer serial,
integer throughput, and float serial — are at gcc -O2 parity.** The FP
scheduler (`docs/plans/fp_serial_scheduler.md` Phase 2) is deferred: the
measured gap is closed; it would be speculative until a real audio DSP workload
shows a scheduling-bound gap.

**Autovec / parallel / SIMD:**

| benchmark | result |
|---|---|
| bench_compose (8 workers) | SERIAL 89.9 ms → COMPOSE 19.3 ms = **4×**, PASS |
| bench_simd_raw | 3× |
| bench_ecs_physics_simd | 2× |
| bench_stbflow (A* vs flow) | 5× |
| bench_ecs_sparse_query | ~22× vs sparse |
| bench_ecs_iter (archetype vs legacy) | 1.12× |

**tablah** (Swift-port hashmap benchmark, 1M items → 24544 filtered), per phase:

| phase | `tablah.bpp` | `tablah_opt.bpp` (inline+unroll) |
|---|---|---|
| generate u64 vector | 12.5 ms | 6.7 ms |
| generate u32 vector | 1.0 ms | 1.3 ms |
| hashmap create (1M inserts) | 17.5 ms | 18.6 ms |
| hashmap filter | 16.7 ms | 12.1 ms |

## Historical evolution

### Bootstrap self-compile time

| Date | Milestone | Time |
|---|---|---|
| pre-2026-05-20 | baseline | 0.51s |
| 2026-05-20 | hot-path opt S1–S3b (`unpack_l` builtin, dispatch hash) | 0.37s (~27%) |
| 2026-05-21 | hot-path opt S3e–S3k | 0.30s (~41% cumulative) |
| 2026-05-25 | Wave 21 spine takeover | ~0.34–0.37s (briefly, then recovered) |
| 2026-06-17 | current (post register-alloc + induction) | **0.25s** |

### xform — integer throughput kernel, ratio to gcc -O2 (lower is better)

| Date | Milestone | ratio |
|---|---|---|
| 2026-06-16 | `bench_codegen` introduced; baseline | 3.71× |
| 2026-06-16 | integer compute-in-place + constant/param promotion | 2.35× |
| 2026-06-17 | instruction selection (immediate shift, CIP memory-leaf fix, strength reduction, integer madd) | ~1.6× |
| 2026-06-17 | induction-variable pointer walk | ~1.44× |
| 2026-06-17 | loop control (self-update `i++` + compare-and-branch fusion) | ~1.18× |
| 2026-06-17 | register levers (assign-into-destination + compare-promoted-directly) | **~1.10× (≈ parity)** |

### lcg — integer serial kernel, ratio to gcc -O2

| Date | Milestone | ratio |
|---|---|---|
| 2026-06-16 | baseline | 1.35× |
| 2026-06-16 | integer CIP | 1.14× |
| 2026-06-17 | integer madd fusion | 1.09× |
| 2026-06-17 | loop control + register levers | **1.02× (parity)** |

### bench_compose — outline × autovec speedup

| Date | Note | speedup |
|---|---|---|
| 2026-05-22 | autovec arc closed (SERIAL 303 → 19 ms at the time) | 6× |
| 2026-06-17 | current (SERIAL 89.9 → 19.3 ms) | 4× |

Bandwidth-bound at N=1M; compute-bound workloads approach the ~32× ceiling.

### tablah — Swift-port hashmap benchmark (total, both variants)

| Date | Note | `tablah` | `tablah_opt` |
|---|---|---|---|
| 2026-04-24 | introduced (external developer port) — drove `print_str` + the hash iteration API | — | — |
| ~2026-05 | hot-path era | ~49 ms | ~40 ms |
| 2026-06-17 | current | ~48 ms | ~39 ms |

tablah is hashmap-bound (insertion + filtering dominate), so it moves little
with scalar codegen work; the steady ~18–20 % `tablah` → `tablah_opt` gap is the
hand inline+unroll of `xorshift64`. It is the standing reference for "what a
real external workload costs," not a codegen micro-gate.

## Adding a benchmark

A new runtime benchmark self-times with the bpp_bench helpers and prints a
human line plus a checksum (so a codegen change that breaks correctness is
caught by the number, not just the timing). A new compile-time case goes into
`bench_compile.sh`'s case list. Always print a checksum, always cite the binary
identity, and update the "Latest results" section here in the same commit that
moves a number.
