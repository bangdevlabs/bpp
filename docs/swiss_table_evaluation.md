# SwissTable vs linear probing for `bpp_hash` — an evaluation

**Date:** 2026-06-16
**Question:** the modern industry (Go 1.24, Zig, Abseil, Facebook F14, Rust
`hashbrown`) has standardised on SwissTables. Should `bpp_hash` follow —
especially now that b++ has the byte-wide NEON builtins that make a real
16-wide group scan possible?

**Verdict:** **No, not for `bpp_hash` as it is used today.** Measured, a
SwissTable is *slower* than the current linear-probing table across the
entire load range `bpp_hash` actually operates in. The NEON builtins make
the SwissTable as fast as it can be — they beat the scalar (SWAR) version at
every load and move the break-even point earlier — but the SwissTable still
loses to linear probing below ~87 % load, and `bpp_hash` never runs that
dense. The benchmark behind this is `examples/swiss_neon_bench.bpp`.

---

## Why the industry uses SwissTables (and why that reason doesn't transfer)

A SwissTable keeps one byte of control metadata per slot (a 7-bit hash
*fingerprint* plus empty/deleted flags) and compares a whole group of
fingerprints — 16 with SSE2/NEON, 8 with a 64-bit SWAR word — in parallel.
The payoff is **flat lookup cost at high load**: it can run at ~87.5 % full
and still find or reject a key in one or two group probes, where linear
probing and chaining degrade badly past ~75 %.

That is a **memory-density** win. Go, Abseil and friends adopted it because
they run their tables *dense* to save memory and cache footprint, and the
SIMD scan keeps them fast there.

`bpp_hash` makes the **opposite trade on purpose**. It resizes at 75 % load,
so it operates between ~37 % and 75 %, and its heaviest consumers pre-size
generously — `examples/tablah.bpp` runs a 1 M-entry table in 4 M slots,
i.e. **25 % load**. It spends memory to buy speed. At sparse load there are
no long probe chains for SIMD parallelism to amortise against, so the
SwissTable's per-lookup setup is pure overhead.

The compiler is the heaviest real consumer of the byte-keyed variant
(`hash_str_*`: the dispatch, validate, parser, types, codegen and C-emit
symbol tables), and those tables are small and sparse — squarely linear's
home turf.

## Method

`examples/swiss_neon_bench.bpp` runs both strategies through the **same
strong avalanche hash**, so the timing isolates the *probe strategy* and
nothing else. It fills a 2^21-slot table to a target load and times 2 M
INSerts and 2 M GETs for each strategy, cross-checking that both return the
same checksum. Three implementations were compared over the session:

* **linear** — the current `bpp_hash` strategy (open addressing, linear
  probe, one occupancy byte per slot).
* **swiss-SWAR** — 8-slot groups, a 64-bit control word, the classic
  has-zero-byte trick (`/tmp/swiss3.bpp`, scalar).
* **swiss-NEON** — 16-slot groups, real `vec_cmeq_byte` + `vec_movemask`
  (`examples/swiss_neon_bench.bpp`).

## Results

GET latency, milliseconds for 2 M lookups, Apple M-series, best of two runs:

| load | linear | swiss-SWAR | swiss-NEON | NEON vs linear |
|-----:|-------:|-----------:|-----------:|:---------------|
| 25 % |    9.3 |       19.3 |       16.3 | 1.75× slower   |
| 50 % |   21.9 |       40.1 |       34.5 | 1.58× slower   |
| 70 % |   42.0 |       57.1 |       52.5 | 1.25× slower   |
| 87 % |   66.5 |       76.3 |       67.4 | ~tied          |
| 95 % |   91.8 |       93.4 |       83.6 | 1.10× faster   |
| 99 % |  175.8 |      140.5 |      114.1 | 1.54× faster   |

Two things stand out:

1. **The NEON builtins did their job.** swiss-NEON beats swiss-SWAR at every
   load, and it pulls the linear/swiss **break-even point from ~95 % down to
   ~87 %**. The real 16-wide compare is genuinely cheaper than the SWAR
   has-zero-byte dance — exactly the hypothesis behind building the builtins.
2. **It still doesn't flip the verdict for us.** At 25–75 % load — where
   `bpp_hash` lives — swiss-NEON is **1.25–1.75× slower** than linear. At
   sparse load linear finds the key in ~1 probe (hash → one occupancy-byte
   peek → one key compare). Every SwissTable lookup instead pays a 128-bit
   control load, a `cmeq`, a `movemask` and a `ctz`, on a control cache line
   *separate* from the keys, with no probe chain to amortise it against.

(A b++-specific footnote: with no vector locals, the group vector can't be
held across the fingerprint compare and the empty-check, so the empty-check
reloads the group. It stays in L1, and present-key GET — the hot path — never
reaches it, hitting on the fingerprint compare first, so the GET numbers
above are fair.)

## Where a SwissTable *would* pay off

The measurement points straight at the consumer profile that wins: a hash
table that is **large, hot, and deliberately dense** (≥ ~87 % load). `bpp_hash`
is none of those. But other workloads are — including 3D games.

**Spatial hashing in a 3D engine (e.g. the planned `fps2` Quake port).**
Classic Quake itself wouldn't benefit: it uses BSP trees + PVS bitsets for
visibility (not hashing), a small fixed edict array, and a handful of
small name-keyed caches (textures, models, sounds, cvars) — all the
small/sparse regime where linear wins, just like `bpp_hash`.

A *modern* 3D engine built in that direction is different. The structures
that genuinely run large + hot + dense are:

* **Broadphase collision / spatial hash grids** — hashing 3D cell
  coordinates to the entities or triangles inside them, rebuilt and queried
  every frame. With many dynamic objects or particles this gets large and is
  hammered per-frame; keeping it dense saves a lot of memory and cache.
* **Particle / fluid neighbour queries** — the same spatial hash, queried
  millions of times per frame.
* **Voxel / destruction grids** — sparse-voxel sets keyed by cell index.
* **Mesh deduplication (vertex welding)** and **draw-call batching by
  material** — built per level or per frame over large vertex/instance sets.

Those are exactly the ≥ 87 % load, query-bound regime where the table above
shows swiss-NEON winning — and the byte builtins (`vec_load16b`,
`vec_splat_byte`, `vec_cmeq_byte`, `vec_movemask`) are already in both
backends, ready to build it. So the SwissTable work is not wasted; it simply
belongs to a *future* consumer (a spatial hash in `fps2`), not to the
compiler's symbol tables. When that consumer appears, this is the place to
revisit — and the right move there is probably a **separate, purpose-built
dense spatial hash**, leaving `bpp_hash` linear and sparse for the compiler.

## Reproducing

```sh
bpp examples/swiss_neon_bench.bpp -o /tmp/swiss_neon_bench && /tmp/swiss_neon_bench
```

The builtins are native-only (no C-emit lowering yet), so run it natively.
See `examples/simd_memchr.bpp` for the same builtins doing work where they do
win today: a 16-byte-at-a-time `memchr`, 11× (arm64) / 9.6× (x86_64) over the
scalar loop.
