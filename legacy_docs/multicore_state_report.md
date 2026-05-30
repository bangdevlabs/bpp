# B++ Multi-Core State of the Union

**Generated:** 2026-04-30
**Scope:** Empirical validation of B++'s multi-thread infrastructure +
  philosophy + roadmap for the next phase of "alien controls host."

This document is the authoritative snapshot for the Emacs agent (and
future sessions) of where B++ stands on multi-thread parallelism. It
combines source-code investigation, empirical validation via test
runs, comparison with industry standards, and the design philosophy
discussions that justify the next steps.

---

## TL;DR

B++ already has **automatic loop parallelization via effect inference**
that is more sophisticated than what C, C++ (with OpenMP), and even
Rust+Rayon offer. It runs in the default compilation pipeline and
correctly transforms `for (i = 0; i < N; i++) { body }` into
`job_parallel_for(N, fn_ptr(synth))` when the body is provably safe.

**Empirically validated 2026-04-30:**
- 5/5 dispatch-related tests pass
- `test_smart_dispatch.bpp` confirms `bl _job_parallel_for` in disasm
- `snake_maestro.bpp` real-game candidate detection: 2 loops auto-promoted
- `pathfind.bpp` and `platform.bpp`: 0 loops promoted (strict criteria)

The gaps to "complete multi-core native" are smaller than they look:
~1000 LOC of incremental work spread across 7 well-defined items, none
architectural. B++ is on the right side of the C-vs-future-knowledge
divide.

---

## Section 1 — What B++ has TODAY (verified)

### The pipeline (default-on)

Every `bpp foo.bpp -o foo` invocation runs this in order
(`src/bpp.bpp:296-309`):

```
call_graph_build();           ← discover who calls whom
classify_all_functions();     ← fixpoint: PHASE_BASE | PHASE_SOLO
classify_inlineable();        ← marks trivials for inline
mark_reachable();             ← BFS from main()
promote_globals();            ← GLOB_AUTO → GLOB_GLOBAL when shared
find_dispatch_candidates();   ← STAGE 1: scanner
synthesize_loop_fn();         ← STAGE 2: synthesizer
rewrite_dispatch_loops();     ← STAGE 3: rewriter
annotate_temps();
run_dispatch();
```

### Effect classification (PHASE_BASE / PHASE_SOLO)

`classify_all_functions()` runs a fixpoint over the call graph
(`src/bpp_dispatch.bsm:1115`). Initial state: every function is
PHASE_BASE. Each pass: a function transitions to PHASE_SOLO if it
locally has impure ops (global writes, syscalls) OR if any callee is
PHASE_SOLO. Lattice height = 1, converges in O(call-chain-depth)
passes.

```
const PHASE_AUTO = 0;
const PHASE_BASE = 1;   // pure — safe on worker thread
const PHASE_SOLO = 2;   // impure — main thread only
```

The annotation `@base` is a programmer hint (recorded in
`fn_phase_hint[]`); the classifier uses it as a starting point but can
override or refine. The user-facing rule: write `@base` on a function
that is logically pure, the compiler verifies and uses the
classification.

### Loop scanner (Stage 1) — `find_dispatch_candidates`

Located at `src/bpp_dispatch.bsm:1826`. Scans every parsed function
body for `for` loops that qualify for transparent parallel dispatch.
Acceptance criteria (all must hold):

| Criterion | Detail |
|---|---|
| A | Desugared `for` loop (T_WHILE with step in `node.e`) |
| B | Step is `i++` (stride 1; stride != 1 currently rejected) |
| C | Condition is `i < N` where N is integer literal OR a global with `glob_class` GLOB_EXTRN/GLOB_GLOBAL |
| D | Body reads/writes ONLY: induction var, loop-locals, safe globals (GLOB_EXTRN, GLOB_GLOBAL) |
| E | Every T_CALL in body targets PHASE_BASE function (pure callee) |
| F | Body has no T_BREAK, T_CONTINUE, or T_RET (cannot cross worker boundary) |

Rejections are silent — the loop stays sequential, no warning. This
is by design: false negatives are safe, false positives are not.

### Synthesizer (Stage 2) — `synthesize_loop_fn`

Located at `src/bpp_dispatch.bsm:1992`. For each candidate, creates a
new internal function `__synth_<N>(i)` containing the loop body. The
function is appended to `funcs[]`, classified PHASE_BASE, and exposed
via `dispatch_cand_synth_idx` for Stage 3.

### Rewriter (Stage 3) — `rewrite_dispatch_loops`

Located at `src/bpp_dispatch.bsm:2110`. For each candidate, mutates
the original T_WHILE node IN PLACE: ntype flips to T_CALL, .a points
at packed name `"job_parallel_for"`, .b points at a 2-slot args
array (N, `fn_ptr(synth)`). Reachability graph is updated to follow
the new edge.

```
Source:                    AST after rewrite:
for (i=0; i<N; i++) {  →   job_parallel_for(N, fn_ptr(__synth_0));
    body;
}
```

### Graceful fallback contract

If `bpp_job.bsm` was not imported (so `job_parallel_for` doesn't exist
in `funcs[]`), Stage 3 SKIPS the rewrite. Loop stays sequential. No
error, no warning. Behaviour identical to before the dispatch system
existed.

If `bpp_job.bsm` IS imported but `job_init` was never called
(no workers spawned), `job_parallel_for` itself has a serial path:
runs the synth function N times on the main thread, returns. Identical
result, no concurrency. Test framework relies on this for
cross-platform tests (Linux thread support is not yet live).

### Runtime — bpp_job

`src/bpp_job.bsm`. Spawns N OS threads at `job_init(N)`. Each worker
has private SPSC queue (single-producer, single-consumer), main
thread is sole producer.

- `job_submit(fn_ptr, arg)` — round-robin push to next worker
- `job_wait_all()` — drain all queues
- `job_parallel_for(N, fn)` — split [0,N) across workers, wait

Memory ordering: every cross-thread queue op is bracketed by full
`mem_barrier()` (DMB ISH on ARM64, MFENCE on x86_64). Overkill vs
acquire/release but conservative — chosen explicitly to avoid the
acquire/release subtle bug class. Performance impact is negligible
because contention is structural-zero (no shared queue).

### Maestro orchestration

`src/bpp_maestro.bsm`. Implements Glenn Fiedler's "Fix Your Timestep"
pattern. Phase order per tick:

```
Frame N:
  1. Accumulate elapsed time into bucket
  2. While bucket >= fixed_dt:
     a. solo(fixed_dt)  ← main thread, mutates state, reads input
     b. base(fixed_dt)  ← main thread, dispatches parallel work
     c. job_wait_all() ← drain workers before next physics step
     d. bucket -= fixed_dt
  3. render(alpha)      ← interpolation factor 0-1000 between ticks
```

Phases bracket the parallel work cleanly. Workers cannot race with
solo phase (job_wait_all between them). Programmer model: write
sequential-looking solo/base/render callbacks; parallelism happens
inside base via auto-rewritten loops.

### Beat — monotonic clock

`src/bpp_beat.bsm`. Lazy-init clock anchored at first `beat_now_*`
call. Returns 64-bit ms or µs. Shared across threads so audio mixer,
animation, physics all agree on time. Lazy init is thread-safe under
the maestro topology because beat is captured at job_init time, before
workers are spawned.

---

## Section 2 — Empirical validation (2026-04-30)

### Test 1 — test_smart_dispatch.bpp

```bash
./bpp tests/test_smart_dispatch.bpp -o /tmp/test_smart
/tmp/test_smart                # rc=0
```

`--dispatch` flag confirms loop detection:

```
[dispatch] 1 candidate loop(s)
  fill_results: for (i = 0; i < 128; i = i + 1)
```

Disassembly of `fill_results`:

```
_fill_results:
   stp   x29, x30, [sp, #-0x30]!
   mov   x29, sp
   mov   x0, #0x0
   str   x0, [x29, #0x28]
   mov   x0, #0x80              ; load N=128
   str   x0, [sp, #-0x10]!      ; push N
   adr   x0, #344                ; address of __synth_0
   str   x0, [sp, #-0x10]!      ; push fn_ptr
   ldr   x1, [sp], #0x10         ; pop fn_ptr → x1
   ldr   x0, [sp], #0x10         ; pop N → x0
   bl    _job_parallel_for       ← AUTO-REWRITTEN CALL!
   mov   x0, #0x0
   ldp   x29, x30, [sp], #0x30
   ret
```

The synthesized function `__synth_0` (offset `0x1000298d0`) contains
the body of the original loop:

```
___synth_0:
   ; receives i in x0, computes results[i] = i*2
   ...
   lsl   x0, x9, x0               ; i << 1 = i * 2
   ...
   str   x9, [x0]                 ; store to results[i]
   ret
```

Verdict: PIPELINE WORKS END-TO-END. Programmer wrote a sequential loop,
compiler emitted parallel call.

### Test 2 — All dispatch-related tests

```
test_smart_dispatch:    compile=0, run=0
test_dispatch_maestro:  compile=0, run=0
test_job:               compile=0, run=0
test_maestro:           compile=0, run=0
test_put_dispatch:      compile=0, run=0
```

5/5 pass.

### Test 3 — Real games (candidate detection)

```bash
./bpp games/snake/snake_maestro.bpp --dispatch -o /tmp/...
```

```
[dispatch] 2 candidate loop(s)
  mixer_init: for (i = 0; i < _MX_BUS_COUNT; i = i + 1)
  init_game:  for (i = 0; i < 1000; i = i + 1)
```

`pathfind.bpp`: 0 candidates (4 for-loops, all rejected — start at i=1
or nested, not all pure callees).

`platform.bpp`: 0 candidates (similar pattern).

`bpp.bpp` (the compiler self-compile): 0 candidates (parser/codegen
are side-effect-heavy, classified PHASE_SOLO, correctly rejected).

Verdict: scanner is CONSERVATIVE. Real games hit candidates only for
the cleanest patterns. Pathfind and platform have parallel-friendly
work but use idioms outside the strict criteria.

---

## Section 3 — How B++ compares to industry

### Table 1 — Auto-parallelization mechanism

| Language / Tool | Programmer marks parallel? | Compiler enforces purity? | Default? |
|---|---|---|---|
| C with OpenMP | `#pragma omp parallel for` (explicit) | No (programmer responsibility) | Off until pragma |
| C++ with std::execution::par | `std::for_each(par, ...)` (explicit) | No | Off until flag |
| Rust + Rayon | `.par_iter()` (explicit) | Borrow checker (compile-time) | Off until method |
| Haskell + GHC | None for pure code | Type system (IO monad) | On (transparently) |
| **B++** | **None — pure annotation only** | **Effect lattice fixpoint** | **On (transparently)** |

B++ is in the same category as Haskell here: programmer doesn't write
parallel-specific syntax; compiler proves safety and lifts.

### Table 2 — Atomic primitives

| Language | Operations |
|---|---|
| C (since C11) | atomic_load, atomic_store, atomic_fetch_add, compare_exchange, ... |
| C++ (since C++11) | std::atomic<T>::load, store, fetch_add, compare_exchange_*, ... |
| Rust | std::sync::atomic::AtomicU64, AtomicPtr, etc. |
| **B++** | **mem_barrier (full fence) only** |

B++ has the FENCE primitive but NOT the atomic operations. Workaround:
the SPSC topology + full barrier is sufficient for the current
worker-pool pattern but cannot express:

- Atomic counter shared between threads
- Lock-free MPMC queue
- Reference counting across threads
- Stop flag / pause flag

### Table 3 — Inter-thread channels

| Language | Channels |
|---|---|
| Go | `ch := make(chan int)` (built-in) |
| Rust | `std::sync::mpsc` (stdlib) |
| Erlang | First-class messages |
| C / C++ | Programmer rolls own |
| **B++** | **None as primitive — use `bpp_job` for fan-out only** |

B++'s `bpp_job` is fan-out from main to N workers. There is no fan-in
primitive (workers → main, separately) and no SPSC/MPSC channel for
inter-thread comms outside the job system.

### Table 4 — Frame-stage parallelism

| Engine | Pattern |
|---|---|
| id Tech (Doom) | Frame stages: input → game → animation → physics → render |
| Naughty Dog | Fiber-based, ~7000 fibers across cores |
| Unreal 5 | Game/Render/RHI/Audio threads + N workers (TaskGraph) |
| Frostbite | Persistent workers, double-buffered game state |
| Unity DOTS | ECS chunks + Burst-compiled jobs |
| **B++ (maestro)** | **solo → base (parallel via auto-dispatch) → render** |

B++ is structurally aligned with id Tech and Unity DOTS. Maestro is
the orchestrator; auto-dispatch lifts loops in `base` phase to
parallel.

---

## Section 4 — Identified gaps (priority-ordered)

### Gap 1 — Linux thread parity (existential)

**Status:** `bpp_thread.bsm` on Linux is stub. pthread requires
libpthread.so loaded via dynamic linking; B++ Linux backend currently
only does static linking. Until ELF dynlink lands, all Linux programs
run single-threaded regardless of dispatch system.

**Impact:** Without this, `job_parallel_for` falls through to serial
on Linux ALWAYS, even with `job_init(8)`. Auto-dispatch is correct but
ineffective.

**Cost:** depends on ELF dynlink work (separately scheduled). ~500 LOC
once dynlink lands.

**Priority:** highest. Single change unlocks 8-32x parallelism on
Linux hosts.

### Gap 2 — Reduction support in scanner

**Status (2026-05-07):** PARTIALLY SHIPPED.

- **Sprint 2a** (detection) shipped 2026-04-30 — scanner now
  recognises `var = var OP value` reduction patterns and emits
  `[REDUCTION acc=...]` annotations under `--dispatch`.
- **Sprint 2b** (rewrite) shipped for the additive operator `+`
  end-to-end: the rewriter splits the loop into worker chunks
  with per-worker accumulators and an additive merge in the
  master. `tests/test_reduction_sum.bpp` and
  `tests/test_reduce_runtime.bpp` exercise the path.
- **Still serial** — reductions over `*`, `min`, `max`, `&`, `|`,
  `^`. The synthesiser hasn't learned their identity elements
  yet (`_dsp_red_identity_for(op_ch)` only handles `+`). Adding
  the others is ~30 LOC of identity-element + merge-op tables
  and ships when a real workload demands it.

Original Sprint 2 plan accomplished its high-priority goal.
Closeout note kept in the doc for context; the parallel `sum`
pattern (most common reduction case) is now auto-dispatched.

### Gap 3 — Stride != 1

**Status (2026-05-07):** DETECTION shipped 2026-04-30
(`dispatch_cand_stride` array carries the parsed stride literal,
`--dispatch` reports it). REWRITE still pending — when
`stride != 1` the dispatch path falls through to the serial
placeholder branch (search `if (sv != 0 || st != 1)` in
`bpp_dispatch.bsm` for the bail-out).

**What ships next when needed:** `job_parallel_for` gains a
chunked variant that takes a stride parameter, and the rewriter
stops bailing out for `st > 1`. Roughly the original 30-LOC
estimate plus the runtime variant in `bpp_job.bsm` (~20 more).

**Priority:** medium — same as before. SIMD-aware processing
and chunked pixel loops are still the motivating use cases.

### Gap 4 — Non-zero start index

**Status (2026-05-07):** DETECTION shipped 2026-04-30 alongside
Gap 3. The scanner records a literal start, `--dispatch` reports
it, and the dispatch table holds the offset. REWRITE still falls
through to serial when start ≠ 0 (same bail-out branch as Gap 3).

**What ships next when needed:** the chunked `job_parallel_for`
variant takes both a start offset and a stride, fixing Gaps 3
and 4 in one cut. Roughly 40 LOC combined.

**Priority:** medium.

### Gap 5 — Channels (chan_*)

**Status:** no first-class inter-thread channel primitives. Workers
inside `job_parallel_for` cannot signal back outside the wait group.

**Impact:** blocks patterns like:
- audio thread sending events to main thread
- network thread feeding game thread
- worker progress reporting

**Cost:** ~80 LOC. Implementation: SPSC ring (already proven in
bpp_job), `chan_send/recv/try_send/try_recv/close`.

**Priority:** medium-low (current games don't need it).

### Gap 6 — 3 atomic primitives

**Status:** only `mem_barrier()` (full fence) exists. Missing the
3 minimum-viable atomic ops:
- `atomic_load(p)` (LDAR ARM64 / MOV+MFENCE x86_64)
- `atomic_store(p, v)` (STLR / MOV+MFENCE)
- `atomic_fetch_add(p, delta)` (LDADD ARMv8.1 / LOCK XADD)

**Impact:** unlocks job dependency counters (Naughty Dog pattern),
ref counting across threads, atomic flags.

**Cost:** ~50 LOC. Per-chip primitive (a64 + x64), exposed as builtin
in dispatch.

**Priority:** medium. Aligns with Unity DOTS minimum.

### Gap 7 — CPU feature detection

**Status:** none. Compiler assumes baseline (NEON on ARM64, SSE2 on
x86_64). No way to detect AVX2, SVE, etc. at runtime for fast/slow
path dispatch.

**Cost:** ~300 LOC + per-OS detection (sysctl on macOS, /proc/cpuinfo
on Linux).

**Priority:** medium-low. Adapts B++ to richer hosts but not
existential.

### Gap 8 — Map/reduce sugar APIs

**Status:** would be `arr_map(in, n, transform, out)` and
`arr_reduce(in, n, op, init)` as stb-level helpers that internally
use the auto-dispatched for-loop pattern.

**Cost:** ~50 LOC. Pure sugar — capability already exists via Gap 2
fix.

**Priority:** low. Ergonomic, not capability.

---

## Section 5 — Total roadmap (updated 2026-05-07)

Sprint 1 (Gaps 3+4 detection) and Sprint 2 (Gap 2 detection +
additive rewrite) shipped 2026-04-30. The remaining backlog and
suggested phasing:

```
Remaining LOC for full multi-core native parity: ~1010 LOC
  Gap 1 (Linux threads):              depends on ELF dynlink (~500 LOC)
  Gap 2 (reduction — non-additive):   ~30 LOC (identity tables)
  Gap 3 (stride rewrite):             ~30 LOC (job_parallel_for variant)
  Gap 4 (start index rewrite):        ~10 LOC (folds with Gap 3)
  Gap 5 (channels):                   ~80 LOC
  Gap 6 (atomics):                    ~50 LOC
  Gap 7 (CPU detect):                 ~300 LOC
  Gap 8 (map/reduce sugar):           ~50 LOC

Already shipped:
  ✅ Gap 3 detection            ~30 LOC (2026-04-30)
  ✅ Gap 4 detection            ~10 LOC (2026-04-30)
  ✅ Gap 2 reduction (additive) ~120 LOC (2026-04-30, more than the
                                          original 100-LOC estimate
                                          because the AST surgery
                                          turned out to be ~50 LOC
                                          deeper than scoped)

Suggested phasing for what remains:

  Sprint A (1 session) — Stride / start rewrite
    - Gap 3 + Gap 4 (rewrite path)  ~40 LOC
    Test: rerun pathfind/platform with --dispatch + measure speedup

  Sprint B (0.5 sessions) — Reduction op family
    - Gap 2 non-additive (identity tables for *, min, max, &, |, ^)
                                    ~30 LOC
    Test: tests/test_reduction_*.bpp expanded

  Sprint C (1 session) — Channels
    - Gap 5 (chan_*)                ~80 LOC
    Test: audio/render thread comm

  Sprint D (1 session) — Atomics minimum
    - Gap 6 (3 atomics)             ~50 LOC
    Test: atomic counter across threads

  Sprint E (when ELF dynlink) — Linux parity
    - Gap 1 (linux pthread)         ~500 LOC after dynlink
    Test: run dispatch tests on Linux

  Sprint F (1 session) — CPU detection
    - Gap 7 (cpu_has)               ~300 LOC
    Test: SIMD fast path opt-in

  Sprint G (0.5 sessions) — Sugar
    - Gap 8 (map/reduce)            ~50 LOC
    Test: ergonomic before/after

Total remaining: ~5-6 sessions, scattered as foundation work
between game features.
```

---

## Section 6 — Philosophical anchors

Three principles that came up in the design discussions and should
guide implementation choices:

### 1. The "alien on Earth" hierarchy

B++ as an organism that lands on a host (chip + OS) and sends
tentacles into it:
- Eyes (sensors): peek/poke, file I/O, syscalls
- Tentacles (effectors): syscall builtins, codegen primitives, vec_*
- Mind (intent): the programmer's source code
- Body (host): the hardware + OS we adapt to

Multi-thread is a key effector: it lets the alien use the host's
8-32 cores instead of 1. Linux thread parity is the LITERAL UNLOCK
of this metaphor on Linux hosts.

### 2. The "tight-bound stb" insight

User's framing: "the stb being well-bound is the lance." B++'s
killer feature vs C is that the stdlib is auto-injected, idiom-aligned,
zero-impedance. This applies to multi-thread too: `bpp_job /
bpp_maestro / bpp_beat` are tight-bound. Programs get parallelism by
importing `stbgame` (which auto-injects all three) and using maestro
phases.

The implication: gaps 1-7 should NOT add new top-level libraries.
They extend the existing tight-bound family. `chan_*` goes inside
`bpp_job.bsm` or a sibling. Atomic primitives go inside the codegen
primitive family. CPU detection goes inside `_core_<os>.bsm`.

### 3. The "from the future" advantage

User's framing: B++ has knowledge of post-2005 multi-core that C/C++
designers in 1972/1985 lacked. The key bets B++ ALREADY made (correct
in hindsight):

- **Effect annotations as enforced part of the language** (C/C++
  retrofitted to C11/C++11 attribute hints)
- **Shared-nothing topology default** (C/C++ default is shared-mutable)
- **Auto-rewrite of pure loops to parallel calls** (C/C++ require
  explicit pragmas)
- **Memory barriers as primitives** (C/C++ retrofitted in 2011)
- **Self-hosted compiler** (control whole stack, no LLVM/GCC fights)
- **No OOP, data-oriented design idiomatic** (avoids the "shared
  mutable state by default" trap of OOP)

The remaining work in Section 4 doesn't change architecture; it
fills in expected primitives that the architecture demands. No
philosophical pivot needed.

### 4. The "feature needs concrete use case" rule

User's project memory `feedback_no_fallback.md` and `bpp_job` source
comments establish: don't add primitives preemptively. Wait for a
real workload to prove the need.

Apply to roadmap:
- Gaps 1-2 (Linux + reduction) have CONCRETE NEED today — implement.
- Gaps 3-4 (stride + start) have visible miss in pathfind — implement.
- Gap 5 (channels) is speculative — defer until audio/network thread
  proves it.
- Gap 6 (atomics) is speculative — defer until atomic counter need
  appears (likely with multi-thread Linux + dependency tracking).
- Gap 7 (CPU detect) is speculative — defer until a SIMD-heavy
  workload (audio convolution? image filter?) proves the need.
- Gap 8 (sugar) follows Gap 2 naturally.

---

## Section 7 — What the Emacs agent should do with this

This document gives the agent an **authoritative starting state**.
When working on multi-thread improvements:

1. **Read this doc first.** Section 1-3 prevent the agent from
   re-deriving what's already known. Section 4 shows the priority.

2. **Trust the empirical findings in Section 2.** The pipeline IS
   wired up, tests DO pass, disassembly DOES show `bl
   _job_parallel_for`. Don't re-investigate from scratch.

3. **Pick a Sprint from Section 5 based on user direction.** Don't
   bundle multiple sprints in one commit. Each sprint is bisect-friendly.

4. **Honor the philosophical anchors.** No new library families.
   No preemptive primitives. Concrete use case needed.

5. **Update this document at the end of each sprint.** Mark Gap N
   as "shipped 2026-MM-DD" with brief notes. The state-of-union
   evolves; this is the canonical place.

6. **When in doubt about an approach, run `--dispatch` on real
   programs.** That's the empirical signal of what currently
   parallelizes. Compare before-and-after of a change to validate.

---

## Section 8 — Honest meta-note

This document was generated 2026-04-30 after a session where the
assistant (Claude) initially claimed B++ was "single-threaded" and
that "compiler doesn't use @base annotation for parallelization."
The user pushed back ("we built dispatch for exactly this") and
forced empirical investigation. The investigation revealed both
claims were wrong: B++ has multi-thread via `bpp_job`, AND the
compiler DOES auto-parallelize via the dispatch pipeline.

The lesson is documented for future agents: **conversational memory
about a codebase is not authoritative; source is**. Before claiming
"B++ doesn't have X," `grep` first. The user's intuition about their
own codebase was right; the assistant's confident statement was lazy
shortcut.

This doc is the antidote: empirically validated, source-cited, honest
about what works and what doesn't. The Emacs agent inherits this
ground truth and is encouraged to update it the same way (verify
before claiming, cite sources, run tests).

---

## Appendix A — Source references

For cross-reference during implementation:

```
Pipeline orchestration:    src/bpp.bpp:296-309
Phase classification:      src/bpp_dispatch.bsm:1115-1190
Loop scanner:              src/bpp_dispatch.bsm:1826-1900
Loop synthesizer:          src/bpp_dispatch.bsm:1992-2050
Loop rewriter:             src/bpp_dispatch.bsm:2110-2200
Worker pool:               src/bpp_job.bsm
Maestro orchestration:     src/bpp_maestro.bsm
Beat clock:                src/bpp_beat.bsm
Memory barrier primitive:  src/backend/chip/aarch64/a64_primitives.bsm (search "mem_barrier")
                           src/backend/chip/x86_64/x64_primitives.bsm
Tests:                     tests/test_smart_dispatch.bpp
                           tests/test_dispatch_maestro.bpp
                           tests/test_job.bpp
                           tests/test_maestro.bpp
                           tests/test_put_dispatch.bpp
```

## Appendix B — Glossary

- **DSP_SEQ** (0): sequential execution, side effects or dependencies
- **DSP_PAR** (1): CPU parallel, independent iterations, no side effects
- **DSP_GPU** (2): GPU candidate, strided memory, no side effects
- **DSP_SPLIT** (3): mix of pure and impure
- **PHASE_AUTO** (0): not yet classified
- **PHASE_BASE** (1): pure, safe on worker thread
- **PHASE_SOLO** (2): impure, main thread only
- **PHASE_REALTIME** (3): time-critical (audio callback)
- **PHASE_IO** (4): does I/O
- **PHASE_GPU** (5): touches GPU
- **GLOB_AUTO** (0): default global, no sharing guarantee
- **GLOB_EXTRN** (1): assigned at init, frozen afterwards (worker-safe)
- **GLOB_GLOBAL** (2): explicitly opted into shared mutable state
