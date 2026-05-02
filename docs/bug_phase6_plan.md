# bug Phase 6 — Profiler + Runtime Symbolication

## Status (2026-05-02)

| Stage | Status | Commits |
|---|---|---|
| 6.1 — Minisym section emission | ✅ SHIPPED 2026-04-30 | `f163596` |
| 6.2a — Runtime locate (Mach-O / ELF walk) | ✅ SHIPPED 2026-05-01 | `f163596` |
| 6.2b — `_runtime_resolve_pc` + name unpack helpers | ✅ SHIPPED 2026-05-01 | `f163596` |
| 6.2c — `caller_pc` builtin + `caller_name` function | ✅ SHIPPED 2026-05-01 | (in tree, pending commit) |
| 6.3 — `panic(msg)` builtin with stack trace | 🟡 NEXT (~1 session, ~100 LOC) | — |
| 6.4 — Multi-thread profiler (hybrid coop+signal) | 🟡 PLANNED (~250 LOC, 2 sessions) | — |
| 6.5 — `caller(n)` sugar | 🟡 PLANNED (~50 LOC, ½ session) | — |

Phase 6.2 done. The pair `caller_name(caller_pc(N))` resolves any
frame in the FP chain without external `.bug`. End-to-end probe
(`tests/test_caller.bpp`) walks 3 frames deep main→deep_a→deep_b→deep_c
and resolves each level. Suite native 124/0/11, C 105/0/30, byte-stable.

Stage 6.3 (panic) is the natural next consumer of the
caller_pc-walk-then-resolve pattern Phase 6.2c just enabled.

## Context

Phase 5 step 3 shipped (the_bug GUI live debugger). Phase 6 is
the next bug-side meta: **runtime symbolication**. Letting a
running B++ program resolve its own PCs to function names
without depending on the external `.bug` file unlocks three
features:

- A sampling profiler (`perf`-style)
- A `panic()` builtin with stack traces
- `caller(n)` introspection

None of these can rely on `.bug` being present at runtime
(containers, deployments, version skew). The sketch lived in
`bug_viz_plan.md` Phase 6 section; this file replaces it with a
detailed plan.

The `.bug` file's v4 build_id (LC_UUID / PT_NOTE GNU) gives us
cross-version safety so a profiler can refuse to symbolicate
against a stale `.bug`.

### Stage 6.4 — Multi-thread profiler — hybrid cooperative + signal (~250 LOC, 2 sessions)

**This is where the original sketch was wrong.** B++ programs
are already multi-threaded (audio callbacks, `bpp_job` workers).
A single-thread profiler shows a biased main-thread view. Time
spent in workers / audio / render thread = invisible. Snake
maestro auto-promotes 2 loops to workers; a single-thread
profiler reports them as "0% main" — actively wrong.

#### Why hybrid

| Approach | LOC | Coverage | AS-safety risk |
|---|---|---|---|
| Cooperative only (sample at job/phase boundaries) | ~170 | bpp_job workers + maestro phases | ✅ zero |
| Signal-based universal (original sketch) | ~250 | every thread | ⚠️ very fragile (every handler) |
| **Cooperative + signal supplement** | ~250 | every thread | ⚠️ only audio/main threads use handler |

Cooperative-only misses:
- **Main thread** (no job loop) — but maestro tick has natural
  phase boundaries (solo/base/render); sample at each.
- **Audio callback thread** — no internal loop, runs 1× per
  buffer. Cooperative gets entry-only granularity; for
  fine-grained inside the callback, signal-based is required.
- **Render thread** (if separate) — same as maestro: sample at
  tick boundary.

The hybrid: bpp_job workers + maestro phases use cooperative
(zero AS-safety risk). Audio callback thread (and any future
non-job pthread) gets a signal-based handler. Best of both.

#### Builtins

- `sys_profile_start(rate_hz)` — install timer; for hybrid
  threads, also install per-thread SIGPROF handlers via
  `sys_sigaction` + `pthread_kill`.
- `sys_profile_stop()` — uninstall timer + handlers, drain all
  per-thread ring buffers, aggregate.
- `sys_profile_dump(buf, cap)` — copy aggregated `(name, count)`
  pairs into caller's buffer; returns entry count.

#### Files

- `src/bpp_runtime.bsm` (extend): per-thread ring buffer
  (1024 samples, no malloc in hot path), aggregator that walks
  buffers post-stop, lock-free flag set by timer signal.
- `src/bpp_job.bsm`: each worker_loop gets a check between jobs:
  ```
  while (running) {
      job = queue_pop();
      if (sample_due) { capture_fp_chain(self_buffer); sample_due = 0; }
      execute(job);
  }
  ```
  ~30 LOC.
- `src/bpp_maestro.bsm`: similar boundary checks at start of
  each phase. ~20 LOC.
- `src/backend/os/macos/_core_macos.bsm`: timer setup
  (`sys_setitimer`, macOS syscall 83) + per-thread SIGPROF
  install + handler shell that captures FP chain into the
  thread-local ring. ~80 LOC.
- `src/backend/os/linux/_core_linux.bsm`: same with Linux
  syscalls (setitimer 38, sigaction 13). ~60 LOC. Only effective
  after Linux thread parity ships (depends on ELF dynlink).
- New builtins (4-layer canonical wiring):
  `sys_setitimer`, `sys_sigaction`, `sys_profile_start`,
  `sys_profile_stop`, `sys_profile_dump`. ~60 LOC across
  spine + a64 + x64 + validate + C emitter.

#### Async-signal-safety

Signal handler runs in restricted context. The handler:

- Writes only to a pre-allocated thread-local ring buffer (no
  malloc).
- Reads `_runtime_resolve_pc` ONLY at aggregation time
  (post-`sys_profile_stop`), not inside the handler. Handler
  captures raw PCs; symbolication happens later.
- Walks FP chain with depth cap 8 (cheap, bounded).
- Sets a flag and returns; no syscalls beyond the trap return.

Cooperative path doesn't have this constraint at all — it runs
in normal user code between jobs.

#### Limitation (cooperative side)

Tight loops inside a single bpp_job won't be sub-sampled by
cooperative — only "this job took X ms" granularity. Workaround:
either split big jobs into smaller ones (good practice anyway)
or opt into signal-based for that thread via a future
`sys_profile_thread_signal(tid)` knob.

#### Verification

`tests/test_profile.bpp` — busy loop in `hot_fn` for 100 ms with
profiler at 1 kHz, expect ≥80 samples attributed to `hot_fn`.
Multi-thread variant runs the same loop in a `bpp_job` worker;
profiler should still attribute ≥80 samples to `hot_fn` (proves
worker coverage).

---

### Stage 6.5 — `caller(n)` introspection sugar (~50 LOC, ½ session)

Tiny composition over Stage 6.2 primitives.

**API:** `caller(n)` returns packed name `n` frames up.
`caller(0)` = "my own name", `caller(1)` = "who called me".

**Implementation:** thin builtin that wraps `caller_pc(n)` +
`caller_name(...)`. Land when a real consumer in user code wants
it (otherwise YAGNI per `feedback_no_fallback.md` philosophy).

---

## Critical files

| Stage | File | Change |
|---|---|---|
| 6.1 | `src/bpp_bug.bsm` | +60 LOC `bug_emit_minisym` |
| 6.1 | `src/backend/target/aarch64_macos/a64_macho.bsm` | +30 LOC |
| 6.1 | `src/backend/target/x86_64_linux/x64_elf.bsm` | +30 LOC |
| 6.2 | `src/backend/os/macos/_core_macos.bsm` | +40 LOC |
| 6.2 | `src/backend/os/linux/_core_linux.bsm` | +40 LOC |
| 6.2 | `src/bpp_runtime.bsm` (new) | +100 LOC |
| 6.2 | builtin wiring (`caller_pc`, `caller_name`, 4 layers) | +25 LOC |
| 6.3 | `src/bpp_runtime.bsm` | +60 LOC |
| 6.3 | `panic` builtin wiring (4 layers) | +20 LOC |
| 6.4 | `src/bpp_runtime.bsm` | +120 LOC sampler + aggregator |
| 6.4 | `src/bpp_job.bsm` | +30 LOC cooperative hook |
| 6.4 | `src/bpp_maestro.bsm` | +20 LOC cooperative hook |
| 6.4 | `_core_<os>.bsm` per-OS | macOS +80 / Linux +60 |
| 6.4 | builtin wiring (`sys_setitimer`, `sys_sigaction`, `sys_profile_*`) | +60 LOC |
| 6.5 | `caller` builtin wiring | +20 LOC |

**Total: ~700 LOC + 1 new module (`src/bpp_runtime.bsm`).**

## Risks (consolidated)

1. **Mach-O page_count regression.** New `__minisym` section
   grows `__DATA`. The fixed math
   (`(mo_data_size + 16383) / 16384`) handles it, but the GOT
   page offset in chained fixups uses the old size. Recheck.
   Reference: `feedback_page_count_bug.md`.
2. **C emitter incompatibility.** Minisym can't be emitted by
   `bpp --c` (instruction layout decided by gcc).
   `_runtime_resolve_pc` returns 0 in C path; `panic()` falls
   back to `libc abort + perror`.
3. **Async-signal-safety in SIGPROF handler.** Pre-allocated
   per-thread ring buffer + no malloc + no locks + no
   symbolication inside handler. The single most fragile piece
   of Phase 6.
4. **Stack walk corruption.** Wild FP crashes the runtime.
   Depth cap (32 for panic, 8 for profiler) + stack-range check
   bound the damage.
5. **Build_id mismatch.** Minisym carries first 4 bytes of
   build_id; `_runtime_resolve_pc` could refuse to operate if
   the binary's runtime build_id (read from LC_UUID / PT_NOTE at
   startup) doesn't match. Belt-and-suspenders, optional.
6. **Linux thread parity dependency.** Linux profiler is a
   single-thread no-op until ELF dynlink ships and `bpp_thread`
   becomes real on Linux. macOS works today.

## Sequencing

```
1. Phase 6.1 + 6.2  (foundation, 1 session, macOS+Linux)
   → minisym + _runtime_resolve_pc + caller_pc/caller_name

2. Phase 6.3        (panic, 1 session)
   → user-facing crash with stack trace

3. Phase 6.4 part 1 (cooperative profiler, 1 session, macOS)
   → bpp_job + maestro instrumentation, no signal handler yet
   → enough to profile worker-heavy programs

4. Phase 6.4 part 2 (signal-based supplement, 1 session, macOS)
   → audio callback + main thread coverage
   → setitimer + per-thread SIGPROF

5. Phase 6.5        (caller(n) sugar, ½ session)
   → land when first user code wants it

6. Linux thread parity  (separate epic, depends on ELF dynlink)
   → unlocks Phase 6.4 on Linux
```

6.1+6.2 first because everything else builds on them. 6.3 next
because it's small and fastest user-visible win. 6.4 split into
cooperative-first + signal-supplement so the AS-safety risk is
isolated to its own session and tested independently.

## Verification (end-to-end)

After 6.1+6.2:
```bash
echo 'fn_a() { return 7; } main() { return fn_a(); }' > /tmp/p.bpp
./bpp /tmp/p.bpp -o /tmp/p
otool -l /tmp/p | grep __minisym       # macOS
readelf -n /tmp/p_lin | grep BPPMINI   # Linux

cat > /tmp/probe.bpp <<'EOF'
fn_a() {
    putmsg(caller_name(caller_pc(0)));   # "fn_a"
    putmsg(caller_name(caller_pc(1)));   # "main"
}
main() { fn_a(); }
EOF
./bpp /tmp/probe.bpp -o /tmp/probe && /tmp/probe
```

After 6.3: `tests/test_panic.bpp` exercises nested panic; expects
stack trace + rc=134.

After 6.4 part 1: `tests/test_profile_cooperative.bpp` runs
parallel-for over 100k iterations of `hot_fn`; expects ≥80% of
samples attributed to `hot_fn` across worker buffers.

After 6.4 part 2: `tests/test_profile_audio.bpp` runs an audio
program with a deliberately slow filter; expects samples
attributed to the filter function inside the audio thread.

## Cross-references

- `docs/bug_viz_plan.md` — Phase 6 sketch (now superseded by this doc)
- `docs/multicore_state_report.md` — Section 4 Gap 1 (Linux
  thread parity) blocks Phase 6.4 on Linux
- `feedback_page_count_bug.md` — Mach-O page_count regression
  history; relevant to Stage 6.1
- `feedback_audio_quality.md` — audio callback constraints;
  relevant to Stage 6.4 signal-based supplement
- `bug_phase5_step3_plan.md` — recently shipped Phase 5 step 3,
  which gave us the `.bug` v4 build_id we cross-check here
