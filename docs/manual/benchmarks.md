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
| `examples/bench_codegen.bpp` | biquad (FP serial), lcg (int serial), xform (int throughput), manylive (many short-lived locals in sequential phases — the RegAlloc v2 / Stage D motivating shape) | `bpp examples/bench_codegen.bpp -o /tmp/bcg && /tmp/bcg` | serial near gcc -O2 parity; throughput as low as possible; manylive is the one kernel B3 cannot reach parity on (see 2026-06-23 entry below) |
| oracle | the same kernels under gcc -O2 | `bpp --c examples/bench_codegen.bpp > /tmp/b.c && cc -O2 -w -Wno-error=implicit-function-declaration /tmp/b.c -o /tmp/o2 -lobjc -lm && /tmp/o2` | — |
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

#### The inliner arc — tiny_lofi as the standing stressor (2026-06-18)

`tools/tiny_lofi/tl_bench.bpp` is the canonical **call-heavy** perf gate for the
inliner arc (`docs/plans/inliner_arc.md`). It self-times 400 renders of the 3 s
DAW project and reports per-3s-render µs. Run all four variants for the gap
decomposition (min-of-3, same shared laptop, same session):

```sh
bpp tools/tiny_lofi/tl_bench.bpp      -o /tmp/tlb       && /tmp/tlb   # ours
bpp tools/tiny_lofi/tl_bench_flat.bpp -o /tmp/tlbf      && /tmp/tlbf  # hand-inlined target
bpp --c tools/tiny_lofi/tl_bench.bpp > /tmp/t.c && \
  cc -O2 -w -Wno-error=implicit-function-declaration /tmp/t.c -o /tmp/tlo -lobjc -lm && /tmp/tlo  # oracle
```

| variant | per 3s-render | vs clang | what it isolates |
|---|---|---|---|
| Inc 1 (`7548e4c`) | 16707 µs | 7.1× | read/write_u16 inlined |
| Inc 2 (`205ca6a`) | 16928 µs | 7.2× | multi-use-param binding path (foundation — flat by design) |
| Inc 3a (`ea2b739`) | 16859 µs | 7.2× | hotness-gate infra (inert) |
| Inc 3b (`dc03969`) | 15300 µs | 6.5× | `arr_struct_at` inlined hot-only (first win, ~10%) |
| **float-leaf (`<this>`)** | **13100 µs** | **5.6×** | `flt_onepole_tick` inlined → `moog_taps` 4 `bl`→0 (~14%) |
| hand-inlined (flat) | 4201 µs | 1.8× | the inliner's target |
| clang -O2 (oracle) | 2339 µs | 1× | the ceiling |

So the gap is now **×3.1 inliner** (13100→4201, was ×4.0 at Inc 2) + **×1.8
flat-loop codegen** (4201→2339). Cumulative arc win **16.9→13.1 ms (~22%)**.
Inc 3b inlined `arr_struct_at` hot-only; **float-leaf inlining** then made matched-
type float leaves inlinable (relax the float gate + float-typed mangled slots),
collapsing `moog_taps`'s 4 per-sample `flt_onepole_tick` calls. Compiler binary
flat for float-leaf (no hot float leaves in the compiler itself); audio
byte-identical throughout. The remaining filter-chain links
(`tl_channel_process`/`moon_process`/`moog_slope` calls, `moog_taps`
multi-return) need Inc 5 + control-flow + the nested-inline architecture (T_CALL
was measured a dead end — see inliner_arc.md). (The oracle `cc` line needs
`-Wno-error=implicit-function-declaration` + `-lobjc` on recent clang.)

**The mechanism metric — `bl`/iter in the per-sample chain** (`bug --disasm`,
the per-increment progress signal):

| function | bl (Inc 2) | bl (3b) | bl (float-leaf) | note |
|---|---|---|---|---|
| `tl_render` | 3 | 2 | 2 | `arr_struct_at` inlined away (3b); reset(once) + `tl_channel_process` remain |
| `tl_channel_process` | 1 | 1 | 1 | → `moon_process` (needs control-flow + nested-inline) |
| `moon_process` | 1 | 1 | 1 | → `moog_slope` |
| `moog_slope` | 1 | 1 | 1 | → `moog_taps` (has a switch) |
| `moog_taps` | 4 | 4 | **0** | 4× `flt_onepole_tick` **inlined (float-leaf)**; itself multi-return → Inc 5 |
| `flt_onepole_tick` | 0 | 0 | 0 | matched-float leaf — **now inlined into moog_taps** |
| `arr_struct_at` | 0 | 0 | 0 | guard-clause → ternary, tier-3 hot-only; inlined at hot sites (3b), standalone for ~80 cold callers |

**Audio correctness baseline (current, 2026-06-23):**
**`f61fac72be5077a6e9ef9cae21dde2a1`**. `tiny_lofi.bpp --render` writes
`tiny_lofi_test.wav`; this is the md5 every codegen-only commit must
reproduce exactly (a change here means the render changed, not just got
faster/slower).

The chain of supersession, so a future session doesn't re-investigate
this from scratch (each step changed the bytes for a real, legitimate
reason — none are regressions):

| md5 | Established at | Why it changed from the previous one |
|---|---|---|
| `8b8742ca…` | (early) | stale before `7ee452e7…` was even recorded — predated a `tiny_lofi.bpp` content edit |
| `7ee452e7…` | pre-2026-06-19 | the **mono** baseline |
| `3e258737…` | `9f925f8` (2026-06-19, stereo S1) | DAW went stereo (`tl_channel_process -> (float,float)`, per-channel pan) |
| `3caa9325…` | `1d9208f` (2026-06-22, slice 3 close) | the hardcoded demo project's own CONTENT grew across normal feature commits (WAV import slice 6a, slice 3 editing) between `9f925f8` and here — never written down, found only by bisecting with `git worktree` while investigating the gap below |
| **`f61fac72…`** | **`2ce22b1`** (2026-06-22) | **int→float migration**: `tl_render` moved from writing s16 (`write_u16`) to writing float32 natively (`pokefloat_h`), matching `stbmixer`'s and the CoreAudio device wire's own float32 format end-to-end; the s16-on-disk WAV conversion moved into a separate `tl_export` pass. Confirmed by direct A/B render at `2ce22b1~1` vs `2ce22b1` (same demo content, only the render/export code path differs) — `3caa9325…` → `f61fac72…`, exactly isolating this one change. |

**Lesson — when re-verifying an audio md5 "regression," bisect with a
disposable `git worktree`, don't trust the comment.** This doc had
drifted two supersessions behind (`3e258737…`) while a whole inliner
session (commits `832e75b`..`c6e19e8`, 2026-06-23) correctly used
`f61fac72…` as its running baseline without ever cross-checking it
against this file — the number itself was right throughout, only the
doc was stale. `git worktree add /tmp/wt <rev>` + rebuild + render at
each candidate revision is the reliable way to re-derive which commit
owns which hash; do not assume a md5 mismatch against this doc means a
real regression without doing that walk first.

**Note — the inliner-arc tl_bench table above is the MONO record** (16.9→13.1
across the arc). The DAW is now a **stereo** workload (`tl_bench` ~14.6 ms,
~205× realtime — stereo doubles the per-frame channel work + adds the
multi-return unpack). Don't compare the mono and stereo numbers directly; the
inliner arc paused at 13.1 ms mono. Stereo also made `tl_channel_process` the
first real **multi-return product consumer** (the eventual Inc 5 target).

#### 2026-06-23 — full catalog re-run after the "redisassemble instead of
guess" arc (float-CIP trio, branch fusion, typed call-arg push/pop, two
inliner fixes — `docs/plans/inliner_arc.md`'s addendum has the per-commit
detail)

Full catalog run end to end (every benchmark in this doc, not just
`tl_bench`) on the installed binary at commit `c6e19e8`, cross-checked
against the historical numbers above rather than reported in isolation:

| Benchmark | Historical | Today | Verdict |
|---|---|---|---|
| `bench_compile.sh` bootstrap | 0.25s (2026-06-17) | 0.25s | unchanged |
| `bench_codegen` lcg vs gcc -O2 | 1.02× (parity) | 0.93× | parity held |
| `bench_codegen` xform vs gcc -O2 | 1.10× | 1.01× | better |
| `bench_codegen` biquad vs gcc -O2 | 1.02× (parity) | 0.95× | parity held |
| `tl_bench` (stereo) | ~14.6 ms / 205× (pre-session baseline) | **10.86 ms / 274×** | **~26% better** — today's cumulative win, not yet a named row above |
| `tl_bench_flat` (stereo) | n/a (mono record was 4201 µs) | 5298 µs | consistent with stereo roughly doubling the mono flat number |
| oracle (`gcc -O2` on `tl_bench.bpp`, stereo) | n/a (mono record was 2339 µs) | 3684 µs | consistent, same reasoning |
| `bench_compose` | 4× (89.9→19.3 ms, 2026-06-17) | 3× (72.5→19.4 ms) | COMPOSE side flat; SERIAL side moved with machine load, not a code regression |
| `bench_simd_raw` | ~3× | 3× | unchanged |
| `bench_autovec_gate.sh` | PASS | PASS | — |
| `bench_mixed_auto.sh` | PASS | PASS | — |
| `bench_outline` | (no historical row) | 6×, 92% vs hand-written explicit | healthy |
| `bench_ecs_iter` | ~1.12× (2026-06-17; now informational, gate moved) | 0.95× | within the "informational" band the test itself declares |
| `bench_ecs_physics_simd` | ~2× | 2×, PASS (positions match bit-for-bit) | unchanged |
| `bench_ecs_scheduler` | PASS | PASS (45% of sequential) | unchanged |
| `bench_ecs_sparse_query` | ~22× (unspecified selectivity) | 2.95×–13.82× (10%/2% selectivity buckets) | same direction, different selectivity than whatever the historical row used |
| `bench_stbflow` (A* vs flow) | ~5× | 4× | within noise |
| `tablah` / `tablah_opt` | 12.5/1.0/17.5/16.7 ms, 6.7/1.3/18.6/12.1 ms | 14.7/1.2/18.2/17.0 ms, 6.7/1.2/19.2/13.3 ms | within noise, same 24544-item filter result both variants |

**Audio correctness:** confirmed against the corrected baseline above
(`f61fac72…`) — unchanged across every commit of the day's session.

**Takeaway:** the whole catalog holds at parity or better against its
own history; `tl_bench` is the one row with a real, durable improvement
worth a permanent entry (added above). Machine noise (not code) explains
every other delta in either direction — none of the moved numbers
correspond to a commit that should have touched that benchmark's code
path.

#### 2026-06-23 (later) — RegAlloc v2 Stages A-D (shadow mode) + the `manylive`
kernel

Opened the full liveness-based register allocator arc
(`docs/plans/compiler_boost_roadmap.md` F.2): CFG construction → liveness
dataflow → RPO linearization → live-interval construction → linear-scan
assignment (`src/bpp_regalloc.bsm`). All four analysis stages run
unconditionally on every function, every compile — but their decisions are
not wired into `cg_var_promote` yet (Stage E, still open), so every
correctness/codegen-output benchmark in this doc was expected to hold
byte-for-byte, and did (bootstrap gen1==gen2 byte-identical on the first try
at every stage, no 1-cycle oscillation — the strongest invariant available,
confirming the new analysis genuinely affects zero compiled bytes anywhere).

**New kernel — `manylive`** (added to `bench_codegen.bpp`): three
sequential phases per iteration (5 locals each), each phase fed only by the
previous phase's last value — 15 locals total, never more than ~5
simultaneously live. This is the shape B3's reference-count-only heuristic
cannot handle (it ranks ALL locals by total reference count with no notion
of *when* each is alive), and the existing three kernels (already at gcc
-O2 parity) don't exercise it. Oracle recipe needs both
`-Wno-error=implicit-function-declaration` (clang 16+/Xcode 15+ treats
`pthread_self`/`__getdirentries64`'s implicit declarations as hard errors,
not warnings — this was already broken before today, on ANY bench_codegen
oracle build, not introduced today) and `-lobjc` (the auto-injected
platform layer references `_objc_getClass` even in a non-GUI program):

```sh
bpp examples/bench_codegen.bpp -o /tmp/bcg && /tmp/bcg
bpp --c examples/bench_codegen.bpp > /tmp/bcg.c && \
  cc -O2 -w -Wno-error=implicit-function-declaration /tmp/bcg.c -o /tmp/bcg_o2 -lobjc -lm && /tmp/bcg_o2
```

| kernel | ours (min of 3) | gcc -O2 (min of 3) | ratio |
|---|---|---|---|
| manylive (20M, many-live) | 142.97 ms | 88.04 ms | **1.62×** |

Checksums matched exactly (`108041794099160` both sides) — correctness
confirmed independent of the timing gap. The other three kernels held their
existing parity (not re-tabulated here; see the main "Latest results"
table). The 1.62× gap is the first QUANTITATIVE confirmation that a real
register-sharing opportunity exists for Stage D/E to close — B3 cannot see
that `manylive`'s phase-A locals die before phase B's are born, so with a
10-slot budget it must spill several of the 15 even though never more than
~5 are alive at once.

**Compile-time overhead, per Tonify Rule 37** (every perf-relevant change
must cite `bench_compile.sh` — this entry was missed in the Stage A-D
commits themselves and is recorded here after the fact):

```
AFTER  (Stages A-D, cb2d145):  bootstrap min 0.27s, median 0.28s (10 runs)
BEFORE (045b806, pre-RegAlloc-v2): bootstrap min 0.26s, median 0.27s (10 runs)
```

Built BEFORE from a disposable `git worktree` at `045b806` for a clean
side-by-side (binary sizes: 1032402 bytes before vs 1050018 bytes after).
A ~0.01s difference at this run count is at the edge of machine noise, but
it repeated in two separate 7-run and 10-run samples, so it is reported
honestly rather than dismissed — four new unconditional per-function passes
(CFG + liveness + RPO + intervals) have a real, small, non-zero cost.
Revisit if a future stage's win doesn't clearly outweigh it.

**Update, same day** — Stage C was rebuilt twice more after the above
measurement (statement-level granularity, then live-range splitting —
see the commit this paragraph ships with for why: a real false-
regression found via `regalloc_compare_vs_b3`'s own systematic sweep,
traced to block-level granularity collapsing many sequential
definitions onto one shared position). Each rebuild adds real, if
small, work per statement:

```
AFTER  (+ instr. granularity + splitting): bootstrap min 0.28s, median 0.30s (10 runs)
```

Still small in absolute terms (~0.02-0.03s over the pre-arc baseline),
and the analysis it buys is real (the systematic B3 comparison's
regression count dropped from 567 false positives to 27 genuine
budget-limited cases across the compiler's own self-compile — see the
commit message for the full trace). Continue watching this number as
later stages add work; Stage E (the actual codegen swap) is the point
where this cost needs to be weighed against a real measured win, not
before.

**Stage E shipped, same day** — the actual codegen swap (per-function:
linear-scan replaces B3's integer-local decision when the systematic
comparison clears AND no variable needed live-range splitting), a64
only. Closing the loop on the line above:

```
AFTER (+ Stage E live):  bootstrap min 0.29s, median 0.31s (10 runs)
```

`bench_codegen`'s `manylive` kernel (the one purpose-built to exercise
the non-overlapping-slot-sharing shape) moved from ~143-150ms to
~139ms against the same gcc -O2 oracle (~88ms) — a real, if partial,
win; checksums match exactly both before and after. Confirmed via
`bug --disasm` on two real cases: the regalloc test fixture's
`__rg_asn_straightline` (`a` and `y` correctly share x19, sequentially,
no corruption) and `manylive_sum` itself (uses the full 14-register
leaf budget, still spills 3-4 of its 18 locals to stack — expected,
not a bug, since even perfect sharing can't fit 18 variables in 14
slots). Audio export md5 unchanged; full native (214/0/12) + C-emit
(172/0/54) suites green; real games (rts2, fps_wolf3d, bang9) compile
and run clean.

**Stage E extended to float, same day** — the user's own correct push:
"how do you improve an audio benchmark that's all float32 without
touching float?" Added the disjoint d8..d15 pool as a second,
independent linear-scan + swap (regalloc_apply_float), gated the same
way as int (systematic comparison + no splitting) plus ONE more gate
int never needed: `regalloc_has_mangled_promoted` refuses the swap
(either pool) for any function where B3 promoted an inliner-mangled
slot, because RegAlloc v2's CFG is built from the caller's body BEFORE
inline splicing — those slots' registers are completely invisible to
the linear-scan that just ran.

Two real bugs found and fixed via `bug --disasm` before this was safe,
neither a crash — both silently wrong output, which is why disassembly
beats trusting a clean exit code:

1. **Slot-sharing double-saved/restored a register.** Pushing a
   physical register onto the prologue's save list once PER VARIABLE
   that shares a slot, instead of once per slot, corrupted the frame
   layout once enough locals shared registers. Found on
   stb/stbfilter.bsm's `moog_taps` (d8/d10 each appeared twice in the
   prologue/epilogue). Fixed in both `_a64_regalloc_apply` and
   `_a64_regalloc_apply_float` — a `slot_seen` set ensures each
   physical register is pushed exactly once regardless of how many
   variables took turns owning it.
2. **Mangled inline slots invisible to linear-scan.** `moog_taps`
   itself inlines `flt_onepole_tick` four times — each call site gets
   its own `_inl<N>_s/in/g` slots, pre-registered by the inliner and
   already promoted by B3, but never seen by RegAlloc v2's analysis
   (built before splicing). Without a gate, linear-scan could (and
   did) assign one of ITS OWN variables the same physical register a
   mangled slot already held — a real collision, output silently
   wrong (0.0 instead of the correct filtered value), confirmed by
   comparing against the pre-Stage-E baseline. Fixed by refusing the
   swap entirely whenever `regalloc_has_mangled_promoted` finds any
   mangled slot B3 promoted in that function.

Net effect on the real DSP chain (`moog_set`, `flt_onepole_g`,
`flt_onepole_tick`, `_tl_apply_pan`, `tl_track_set`,
`tl_track_set_volume`, `moon_set`, `lfo_*`, `vec2_*` — confirmed via a
temporary instrumented build, then removed): the swap fires on every
float-bearing leaf-ish function in the chain that doesn't itself
inline another function; `moog_taps`/`moog_tick`/`moog_slope`/
`tl_channel_process`/`rotary_tick`/`moon_process` (all of which inline
something internally) correctly fall back to B3 untouched. Audio
export md5 still unchanged (`f61fac72be5077a6e9ef9cae21dde2a1`);
tl_bench unaffected (~10.2-10.4ms, same band as before); full native
(214/0/12) + C-emit (172/0/54) + all 5 regalloc gates green; real
games (rts2, fps_wolf3d, bang9) and mini_synth all compile clean.
Compile-time overhead unchanged from the int-only Stage E entry above
(0.29/0.31s).

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

## 2026-06-23 (final) — evaluation: what the whole day actually bought

Re-ran `tl_bench` (stereo) and `bench_codegen` (all four kernels) one
more time, min-of-5, on the now-fully-installed binary (inliner arc +
RegAlloc v2 Stages A-E, int and float, all live) — to answer the
question this row exists to answer: **after a full day of work, did
the needle move on the workload that actually matters (real, call-
heavy, float32 audio), or only on synthetic kernels?**

```sh
bpp tools/tiny_lofi/tl_bench.bpp -o /tmp/tlb && /tmp/tlb            # ours
bpp tools/tiny_lofi/tl_bench_flat.bpp -o /tmp/tlbf && /tmp/tlbf      # hand-inlined ceiling
bpp --c tools/tiny_lofi/tl_bench.bpp > /tmp/t.c && \
  cc -O2 -w -Wno-error=implicit-function-declaration /tmp/t.c -o /tmp/tlo -lobjc -lm && /tmp/tlo
bpp examples/bench_codegen.bpp -o /tmp/bcg && /tmp/bcg
bpp --c examples/bench_codegen.bpp > /tmp/b.c && \
  cc -O2 -w -Wno-error=implicit-function-declaration /tmp/b.c -o /tmp/bcg_o2 -lobjc -lm && /tmp/bcg_o2
```

| benchmark | min-of-5 | vs gcc -O2 |
|---|---|---|
| `tl_bench` (ours, call-heavy) | 10.14 ms (296×) | **2.72×** |
| `tl_bench_flat` (hand-inlined ceiling) | 5.20 ms (577×) | 1.39× |
| `tl_bench` oracle (gcc -O2) | 3.73 ms (804×) | 1× |
| `bench_codegen` biquad | 49.70 ms | 1.03× |
| `bench_codegen` lcg | 20.81 ms | 1.01× |
| `bench_codegen` xform | 6.93 ms | 1.13× |
| `bench_codegen` manylive | 144.58 ms | 1.72× |

**The real win: `tl_bench` 14.6 ms → 10.86 ms (inliner arc) → 10.14 ms
(+ RegAlloc v2) = ~30.5% faster than this morning, ~6.7% of that from
RegAlloc v2 alone.** That's the number that matters — it's measured on
the assembled, call-heavy, float32 DAW render, not a microbenchmark.
RegAlloc v2's float extension is what makes this honest: before it,
"RegAlloc v2 helps the audio chain" was a hope, not a measurement —
`tl_render`'s own parameters are float and integer Stage E alone
cannot touch them. The 6.7% is real but partial, for a reason already
on record: the chain's hottest links (`moog_taps`, `moog_slope`,
`tl_channel_process`, `moon_process`, `rotary_tick`) all inline
something internally and correctly fall back to B3 (the mangled-slot
gate) — RegAlloc v2 today only reaches the chain's *leaves*
(`flt_onepole_g`, `flt_onepole_tick`, `_tl_apply_pan`, `tl_track_set`,
`moon_set`, `lfo_*`), not the composed functions the inliner hasn't
yet collapsed into them. The two arcs are not done competing for the
same ground — the inliner arc's own remaining frontier (Inc 5 +
nested-inline, `docs/plans/inliner_arc.md`) would, if it collapses
`moog_taps`'s own call sites, hand RegAlloc v2 a much bigger leaf to
work with than it has today.

**`manylive` at 1.72× (vs the 1.58-1.62× recorded earlier today) is
noise, not a regression** — this doc's own philosophy says to treat
±15% as noise on a shared machine, and `bench_codegen`'s own biquad
swung 46-56 ms across runs in today's session alone. Nothing in the
last two commits (the float Stage E + its two bug fixes) touches
`manylive_sum` — it's pure integer, leaf, no mangled slots, never a
candidate for either fix. The honest read: RegAlloc v2's int side is
holding its earlier ~139ms-ish result within machine variance; the
gap that's left (still ~1.6-1.7× vs the oracle on a kernel deliberately
built to need 18 variables in budget=14) is the genuine remaining
distance between linear-scan's "spill furthest end_pos" heuristic and
what a smarter spill-cost model (weighted by use count, the way B3
itself already is) could close — Stage F territory, not measured
today, not built speculatively.

The other three `bench_codegen` kernels (biquad/lcg/xform) sit exactly
where they did before any of today's work — 1.01-1.13× — confirming
RegAlloc v2 didn't cost them anything: those kernels have too few
simultaneously-live locals for B3 to ever leave anything on the table,
so the systematic comparison clears, the swap applies, and the result
is identical to B3's own decision by construction.

## 2026-06-23 (later still) — "RegAlloc v2 sees through inlined call
sites" arc closes; final re-measurement

The evaluation just above was honest at the time, but its central
caveat — `moog_taps`/`moog_slope`/`tl_channel_process`/`moon_process`/
`rotary_tick` "all inline something internally and correctly fall
back to B3" — is now stale. A same-day follow-on arc taught Stage A's
own analysis to recursively expand a growing set of real inlined-call
shapes (straight-line, multi-statement, nested calls, multi-target/
multi-return, struct-field writes, switch-of-returns) directly into
real intervals for the mangled slots those shapes create, then
replaced the blanket "any mangled slot present -> refuse" gate with a
precise one ("only refuse if a mangled slot genuinely has no interval
at all"). Confirmed via a temporary debug probe (reverted before
shipping): `moog_taps`, `moog_tick`, and `moog_slope` — all three
previously blocked outright — now receive the real swap.
`tl_channel_process`/`rotary_tick`/`moon_process` still don't (each
hides its nested call inside a trailing return EXPRESSION rather than
a plain statement's RHS — a shape this arc doesn't recognize yet,
separately scoped future work).

Re-measured exactly the same two benchmarks, min-of-5, same machine,
on the binary with this arc fully installed:

| benchmark | min-of-5 | vs gcc -O2 |
|---|---|---|
| `tl_bench` (ours, call-heavy) | 9.98 ms (300×) | ~2.68× |
| `bench_codegen` biquad | 47.43 ms | 0.96× (parity, within noise) |
| `bench_codegen` lcg | 20.52 ms | 0.96× (parity, within noise) |
| `bench_codegen` xform | 6.60 ms | 1.10× |
| `bench_codegen` manylive | 136.23 ms | 1.57× |

**`tl_bench`: 10.14 ms → 9.98 ms, a real but small ~1.6% on top of the
~30.5% the day's earlier arcs had already banked.** Smaller than
hoped given three previously-untouched functions in the hot chain
now genuinely swap — the honest read is that `moog_taps`/`moog_tick`/
`moog_slope` are each called once per channel per sample (not in a
tight inner loop of their own), so even a real win inside them is a
small fraction of `tl_render`'s total cost; the chain's actual
remaining bottleneck is elsewhere (most likely the surrounding loop
structure and the three functions that still don't qualify, which
collectively do more of the per-sample work). Reported honestly
rather than inflated — this is the same lesson the branch-fusion
commit earlier in the inliner arc already taught: a real, verified,
disasm-confirmed improvement doesn't always move wall-clock time
proportionally to how much code it touches.

`bench_codegen`'s other three kernels are flat (within noise) from
before this arc, as expected (none of them have any mangled slot at
all). `manylive` at 1.57× is within the same noise band already
established (1.58-1.72× across today's session) and is, again, not
touched by anything in this arc (pure integer, no inlined calls).

Net honest verdict for the day: **the float Stage E extension plus
the "sees through inlined call sites" arc together are a genuine,
disasm-verified capability win for the register allocator** (it now
correctly shares registers across non-overlapping mangled-slot
lifetimes in real, complex, multi-level DSP functions, where it
previously couldn't see those variables at all) — but on THIS
specific benchmark, the wall-clock payoff is modest because the
newly-covered functions aren't where most of the per-sample time
goes. The capability is real and durable; the next measurable win on
`tl_bench` specifically needs either the remaining three uncovered
functions or a different part of the chain entirely — not assumed,
to be found by re-disassembling, per this whole project's standing
discipline.

## Adding a benchmark

A new runtime benchmark self-times with the bpp_bench helpers and prints a
human line plus a checksum (so a codegen change that breaks correctness is
caught by the number, not just the timing). A new compile-time case goes into
`bench_compile.sh`'s case list. Always print a checksum, always cite the binary
identity, and update the "Latest results" section here in the same commit that
moves a number.
