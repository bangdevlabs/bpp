# Sidequest: hedged reads + (eventual) DRAM-refresh tail latency for B++ audio

> **Inspiration**: [LaurieWired/tailslayer](https://github.com/LaurieWired/tailslayer)
> — a C++ library that reduces tail latency in RAM reads caused by DRAM
> refresh stalls via data replication across uncorrelated channels +
> hedged parallel reads + first-wins. Worth porting the *idea* even
> where the specific DRAM-channel mechanics overshoot what B++ needs
> today.

## The one-line ask

Open up the **hedged-read + replication** pattern as a B++-native
capability, **starting with the generic primitive** (`cas_word` builtin
+ `hedged_reader` cartridge) so any consumer (asset loads, hot-reload
polling, shader compile, A\* heuristics, **eventually** audio
hot-path) can reach for it. **Defer** the full DRAM-channel-aware
placement (the actual TailSlayer trick) until a measured profile asks
for it.

## Why this is opening as a sidequest now (and not a build-now arc)

Honest reality check first: B++ audio **does not have a DRAM-refresh-class
problem today**.

| Layer | Latency | DRAM refresh stall (~5 µs) absorbed? |
|---|---|---|
| SPSC ring (rhythm: 4096 frames @ 44.1k = 93 ms of buffer) | 10⁵ µs of slack | ✓ trivially |
| Audio callback period (512 frames @ 44.1k) | 11.6 ms | ✓ |
| `mixer_fill` per chunk (1024 frames) | sub-ms | ✓ |
| Per-sample wavetable lookup | ~ns | absorbed by the ring |

Every audible glitch the project actually hit in audio so far has been a
**ms-class** logic bug:
- `malloc` in the callback path (fixed by `@safe` — May 2026).
- Under-production (consumer drained the ring faster than the producer
  filled it — design issue, not stalls).
- `write_f32` + `peekfloat` width mismatch (wrong bits, not latency).

DRAM refresh stalls produce sub-µs glitches per sample; the ring absorbs
hundreds of µs. **The scale doesn't match.**

## When TailSlayer-class techniques START to matter

The audio path becomes refresh-sensitive in any of these futures:

1. **Live monitoring** — `mini_synth` at < 5 ms total end-to-end latency
   (key press → audible sound). The ring has to shrink to 64-128 frames,
   at which point each refresh stall becomes a visible % of the budget.
2. **Dense polyphony** — 32+ simultaneous voices each reading different
   wavetables; memory contention grows; collision with refresh sweeps
   increases linearly.
3. **Per-sample DSP** — IIR filters, convolution reverbs at high sample
   rates; each sample multiplies the read pressure.
4. **Pro-audio "compete with Ableton / Bitwig" target** — a hard
   < 10 ms latency contract end-to-end.

None of these are current priorities. But if B++ ever aims at "platform
for live synth performance," this card enters the deck.

## The technique, decomposed into B++-shaped pieces

TailSlayer C++ depends on four ingredients. Mapping each to B++:

### 1. Generic API (templates in C++ → fn-pointer struct in B++)

C++ uses `HedgedReader<T, signal_fn, work_fn>` templates. B++ has no
templates, but has function pointers + struct generics (the same shape
as `ChipPrimitives` in the compiler spine).

```b++
// stb/stbhedged.bsm (new cartridge)
struct HedgedReader {
    n_replicas,            // how many copies live in memory
    replicas,              // arr of replica base pointers
    signal_fn,             // fn_ptr — waits for event, returns the replica index
    work_fn,               // fn_ptr — processes the value at the returned index
    workers                // bpp_job pool dedicated to this reader
}

hedged_reader_new(n, signal_fn, work_fn) -> ptr;
hedged_reader_insert(hr, addr);
hedged_reader_start(hr);
hedged_reader_free(hr);
```

`T` becomes the B++ default — every value is a word; consumers cast at
the boundary. No template machinery needed.

### 2. CAS (compare-and-swap) primitive — currently missing

The "first replica to write a result wins" protocol needs atomic CAS.
B++ has `mem_barrier`; I haven't found atomic CAS. **It's worth adding
as a chip-level builtin regardless** — lock-free queues, atomic refcount
on handles, and several other infra patterns want it too.

```b++
// New builtin in cg_builtin_dispatch (src/bpp_codegen.bsm),
// emitted by each chip backend:
cas_word(addr, expected, new) -> word;   // returns the OLD value
```

Per-chip lowering:
- **a64**: `casal x_new, x_old, [x_addr]` (Armv8.1+ LSE — every Apple
  Silicon supports it).
- **x64**: `lock cmpxchg [rdi], rsi`.
- **wasm32** (future): `i64.atomic.rmw.cmpxchg`.

Per Rule 41 (additive portability): the builtin is defined in the spine;
each chip adds one branch in its primitive table; adding new chips is
zero modifications to existing chips.

### 3. Worker pool for hedged dispatch

✓ B++ has `bpp_job` already. Each read dispatches N parallel worker
jobs; the first one to CAS into the result slot wins. The pool can be
shared with the rest of the program or dedicated per-reader.

### 4. Channel-aware memory placement (the hard part — defer)

Original TailSlayer's secret sauce: put the replicas in **physically
distinct DRAM channels** so their refresh schedules can't collide. Uses
"channel scrambling offsets" — magic constants per CPU family that map
virtual address bits → physical channel.

B++ today has `_mem_alloc_pages` per OS that asks the kernel for pages
but does NOT pick a channel. Adding it:

```b++
// New per-OS primitive in src/backend/os/<os>/_core_<os>.bsm Section 2:
_mem_alloc_channel(size, channel) -> ptr;

// Cross-platform wrapper in src/bpp_mem.bsm:
malloc_channel(size, channel) -> ptr;
```

Per-OS difficulty:
- **macOS Apple Silicon**: LPDDR5 unified memory with 8 channels on M1
  Max / 4 on M-base. Apple does NOT document the virtual→physical layout.
  Would require **reverse engineering** TailSlayer-style (timing
  side-channel to infer mapping per chip generation).
- **Linux x86_64**: `numa_alloc_onnode` + `MAP_HUGETLB` is the foundation;
  scrambling offsets for AMD Zen / Intel Skylake / Sapphire Rapids are
  publicly mapped. **Doable.**
- **WASM**: N/A — linear memory doesn't expose hardware.
- **BangOS bare-metal**: trivial in principle (the OS owns the memory
  controller), but blocked on BangOS itself shipping.

**This is the 2-3 week piece**, with the macOS RE the riskiest leg.

## Phased plan

Each phase is bootstrap byte-stable + the three-runner tripod green. CAS
is a codegen change → bar is `bug --disasm` verification per chip.

### Phase H0 — `cas_word` builtin (~1 day, foundational)

- Spine dispatch in `cg_builtin_dispatch` (`bpp_codegen.bsm`).
- a64 chip primitive: `casal` LSE instruction.
- x64 chip primitive: `lock cmpxchg`.
- C-emit: `__sync_val_compare_and_swap` or `atomic_compare_exchange_strong`.
- Smoke test in `tests/test_cas.bpp` (multi-thread CAS contest).

**Justification independent of audio**: lock-free SPSC queue impl in
`bpp_job`, atomic refcount on `stbasset` handles, future signal-safe
counters in `bpp_runtime` panic path. The audio TailSlayer port is one
of several consumers.

### Phase H1 — `stbhedged` cartridge (~2 days, generic infra)

- `hedged_reader_new` / `_insert` / `_start` / `_free`.
- Internal: per-call, the `start` function fires N `job_submit(work_fn,
  replica_addr)` calls, each worker writes into a shared `result_idx`
  slot via `cas_word` (only first non-zero CAS wins), main thread waits
  for `result_idx != 0`, returns it.
- No DRAM-channel placement assumed — replicas are just N copies in
  regular `malloc` memory.
- Demo: `examples/hedged_smoke.bpp` reads from 3 replicas of the same
  buffer; introduces artificial delay in 2 of 3 workers; measures that
  the first reply wins consistently.

**Justification independent of audio**:

| Consumer | Hedged-read application |
|---|---|
| Hot-reload polling | `file_stat` the same file from 3 candidate search paths in parallel; use whichever resolves first |
| Asset loading | Load `atlas.json` + a fallback path in parallel; whichever returns first wins |
| Shader compile | Compile fast-path + full-quality variant in parallel; bind the first ready |
| A\* pathfinding | Run Manhattan + diagonal heuristic in parallel; commit to whichever finds a valid path first |
| Cross-module symbol lookup | Multiple hash tables sharded; query all, first hit wins (already done synchronously today; could parallelize) |

Rule 36b: 2+ named consumers needed to promote. The list above already
satisfies it (asset loads + hot reload). The audio consumer is bonus,
not the justification.

### Phase H2 — audio demo (~1 day, proof-of-life)

- Take `mini_synth`'s active wavetable, replicate into 2 separate
  `malloc` allocations.
- Hot loop in `_mix_voice` becomes a hedged read: 2 workers read the
  same sample from the 2 replicas, first wins.
- Measure: under live performance load, capture the p99 / p99.9 sample
  produce time. Compare against baseline.

**Expected result**: probably no measurable difference because the SPSC
ring absorbs everything. **That's a valid outcome.** Documents that the
technique is ready when the audio target tightens, but doesn't burn
infrastructure when it doesn't.

### Phase H3 — `_mem_alloc_channel` per-OS (~2-3 weeks, DEFER)

Only if a profile in some future arc demonstrates a refresh-stall-bound
workload. Risks:
- Apple Silicon channel mapping is undocumented; RE may not converge.
- Adds significant per-OS infra (Rule 41 cost) for niche payoff.
- Holds the bar of "real consumer pulling this primitive" (Rule 36b)
  before being built.

Mark as: **GATED on a measured workload that pages through the SPSC
ring fast enough that producer-side stalls matter.**

### Phase H4 — `stbmixer` integration (~2 days, after H3)

Opt-in `mixer_set_hedged(1)` flag. When enabled, the mixer routes its
wavetable / sample reads through `hedged_reader_new` with replicas
placed via `malloc_channel`. Default off (zero cost when unused).

### Phase H5 — bench + closeout (~1 day, after H4)

Side-by-side: `mini_synth` doing live performance, hedged off vs on.
Capture latency percentiles. Document the result honestly:
- If hedge measurably reduces tail: ship as default-off opt-in,
  graduate to default-on when 2+ consumers prove value (Rule 36b).
- If hedge is statistical noise: ship the cartridge as
  available-but-unused, retire mixer integration, keep the generic
  primitive for the non-audio consumers.

## Risks + mitigations

- **CAS codegen bug** (H0): blast radius is "every binary's atomic
  contract." Mitigation: verify via `bug --disasm` on the
  multi-thread smoke per chip; the disasm shows the `casal` /
  `lock cmpxchg` mnemonics directly, same diagnostic loop that found
  the May 2026 arr_len / arr_get bugs.
- **Lock-free correctness in `stbhedged`** (H1): hard to reason about
  by inspection. Mitigation: run smoke under ThreadSanitizer if
  available; use B++'s deterministic worker-pool seeding to make
  contention reproducible.
- **Apple Silicon RE failure** (H3): could spend 2 weeks and not
  converge. Mitigation: H3 is opt-in; H0-H2 stay valuable independently.
- **Replication memory cost** (always): 2x or more memory per replicated
  buffer. For wavetables this is ~MB; trivial. For larger assets this
  becomes a budget concern — document the trade in `stbhedged`'s header.

## Open questions for the external reviewer

When this gets reviewed (same pattern as the allocator + file_stat arcs):

1. Is `cas_word` the right primitive name, or should it be a family
   (`cas_word_acq`, `cas_word_rel`, `cas_word_seqcst`) matching the
   memory-order vocabulary the rest of `bpp_job` will need?
2. Should H1's `stbhedged` cartridge live in `stb/` (game-side, Tier 2)
   or as `bpp_hedged` in the auto-injected prelude (every program ships
   it)? Rule 33 tier decision — currently leaning Tier 2 since the
   non-game consumers above (asset, hot reload, shader compile) are all
   stb-or-tool consumers, not bare-language consumers.
3. Cousins survey: how do JUCE / CLAP-host frameworks / OpenAL Soft /
   Bitwig handle DRAM-refresh tail latency in production? Do they punt
   to "buffer larger" or invest in physical placement? Same kind of
   survey the allocator arc did for jemalloc / mimalloc / tcmalloc /
   game-engine arenas. If the consensus is "buffer absorbs everything
   in practice," that's strong evidence H3 stays deferred forever.
4. Worth doing H0 as a side-effect of an arc that has another reason to
   land CAS (e.g., bpp_job upgrades, atomic refcount in stbasset), so the
   CAS work doesn't get blamed on a niche feature? Survey current arc
   queue for a natural carrier.
5. For Apple Silicon RE (H3) — does the user's hardware (M-series, which
   exact?) let us measure channel scrambling, or is the LPDDR5 layout
   too opaque even for timing side-channels? Concrete experiment design
   needed before committing the 2-3 weeks.

## Constraints any recommendation must respect

- **Rule 41 (additive portability)**: CAS lives in spine + per-chip
  primitive table. Per-OS channel placement isolated to
  `_core_<os>.bsm` Section 2.
- **Rule 36b (named consumers)**: H1 (`stbhedged`) already has 4
  non-audio consumers in queue. H4 (mixer integration) needs a measured
  workload first.
- **Bootstrap byte-stability**: every CAS + cartridge commit must
  preserve gen1 == gen2.
- **`@safe` interaction**: `cas_word` is a syscall-free atomic
  instruction — provably `@safe`-clean. `stbhedged` itself dispatches
  worker jobs (allocates internally for worker contexts), so its
  public API is NOT `@safe` — but the work_fn it dispatches MUST be
  `@safe` (it runs on workers). Document and enforce.
- **The reuse-bug lesson**: lock-free protocols are exactly the
  invariant-heavy bug class that bit us in the free-list arc. Keep H1
  simple (single CAS for "first wins," no spin-lock loops, no priority
  inheritance) and lean on the disasm loop to verify codegen end-to-end.

## Cross-references

- [LaurieWired/tailslayer](https://github.com/LaurieWired/tailslayer) —
  the C++ reference implementation that triggered this brief.
- `src/bpp_mem.bsm` — current allocator state (free-list, Rule 41 layer).
- `src/bpp_job.bsm` — worker pool that H1 dispatches through.
- `stb/stbmixer.bsm`, `stb/stbaudio.bsm` — audio consumers that H4
  would integrate.
- `tools/mini_synth/` — the canonical live-performance consumer that
  would benefit most if H3 ever ships.
- `docs/plans/sidequest_allocator_final_push.md` — the arc-doc template
  this one follows.
- `docs/manual/tonify_checklist.md` — Rule 33 (tier system), Rule 36b
  (consumer requirement), Rule 41 (portability).
- `docs/journal.md` — 2026-05-27 free-list arc (the "invariant-heavy
  designs scare us" lesson).
