# Sidequest brief: the allocator's "last push" — coalescing vs arenas vs chunk-scavenging

**For:** the external research agent. **Pattern:** same as the free-list and
`file_stat` studies — study how the cousins (systems languages + game dev +
audio + graphics tooling) actually handle this, come back with a comparative
table + a phased recommendation (α/β/γ-style), and we pick.

## The one-line ask

B++'s heap is now a size-class free-list with reuse (shipped 2026-05-27).
What is the **right final optimization** for it — object-level **coalescing**
(the thing we kept saying "later" while the reuse bug was open), per-frame
**arenas**, allocator-level **chunk-scavenging / return-to-OS**, or some
combination — given how game engines, audio tools, graphics tools, and the
modern systems-language allocators actually solve memory fragmentation and
memory-return? The user's framing: "coalescing is market-standard in game
dev / audio / graphics tooling, right? how do our cousins handle it?"

## Where B++'s allocator is NOW (so you don't re-derive it)

Lives in `src/bpp_mem.bsm` (portable policy, Rule 41) + per-OS page
primitives `_mem_alloc_pages` / `_mem_free_pages` in `_core_<os>.bsm`.

- **Small requests (≤ 4096 B):** 9 size classes (16, 32, … 4096 = `16<<c`),
  carved from 64 KB chunks. `free` pushes the block onto its class's
  intrusive free-list; `malloc` pops (zeroing on pop). `_MEM_REUSE=1` is the
  default as of `f13cc5b`.
- **Large requests (> 4096 B):** direct `mmap` per alloc, `munmap` on free.
- **`_MEM_DEBUG` mode (A0):** poison-on-free (0xBE) + no-zero-on-pop, a
  Zig-GPA-style UAF probe. Default off.
- **16-byte header:** `[raw_base | marker]`; marker = class index (small) or
  total bytes (large), disjoint ranges.

**What it does NOT do (the gap):**
- **No cross-class reuse.** Memory freed into class-64's list is stranded
  there — a later class-4096 request can't use it; it carves a new chunk.
- **No return-to-OS for small blocks.** A 64 KB chunk, once carved, is never
  `munmap`'d even when all its blocks are free. Peak small-heap is sticky.
- **No coalescing of any kind.** Blocks are fixed-size per class; never split
  or merged.

For a steady allocation pattern this is fine (each class's high-water mark
bounds it). It bites a **long-running program with phase-varying allocation**
(Bang 9 editing for hours; rts1 load-phase vs combat-phase having different
class mixes).

## What B++ ALREADY has (don't recommend reinventing these)

- **`bpp_arena.bsm`** — `arena_new / arena_alloc / arena_reset / arena_free`.
  Bump allocator, O(1) free-all via reset. The compiler's AST arena uses it.
- **`stbpool.bsm`** — fixed-size object pool (`pool_new/get/put/free`).
- **`@safe`** annotation — the compiler PROVES a function (e.g. an audio
  callback or worker) never reaches `malloc`/IO/syscall through its call
  graph (W026). The two CoreAudio callbacks are the canonical consumers.
- **`stbecs`** — entity storage is already DOD handles-into-stable-arrays.

## My current analysis / hypothesis (validate or refute this)

I believe the user's premise needs a **reframe**: object-level coalescing is
**not** a game-dev pattern — it's a legacy *general-purpose-malloc* feature,
and the domains in the question largely avoid the general heap in hot paths.

1. **Modern general allocators (jemalloc / tcmalloc / mimalloc / Go runtime)**
   use **segregated size classes for objects (NO object coalescing)** +
   **page/span-level coalescing** + **return-to-OS** (jemalloc *decay* via
   `madvise`, Go *scavenger*, tcmalloc/mimalloc page/segment reclamation).
   The industry largely abandoned dlmalloc-style object coalescing for small
   objects. B++'s small path already matches this design.
2. **Legacy general (dlmalloc / glibc main bins)** does object coalescing via
   boundary tags — but even glibc keeps **non-coalescing fast bins** for small
   objects (speed) and only consolidates periodically.
3. **Game dev (Jai `temp_allocator`, Odin `context.temp_allocator`, Unreal
   mem stacks, idTech frame allocators)** is dominated by **arenas (bump +
   reset per frame/level)** and **pools (fixed-size)**. Reset frees everything
   → zero fragmentation, coalescing irrelevant.
4. **Real-time audio** allocates **nothing** in the callback (pre-alloc + ring
   buffers + pools). No alloc → no fragmentation. (B++'s `@safe` enforces it.)
5. **Graphics** sub-allocates GPU memory (VMA-style buddy/pool/linear) — but
   CPU-side it's arenas/pools.

**My recommendation, in order of value:**
- **(1) Arenas for the per-frame churn — the real win, and B++ already has
  `bpp_arena`.** The rts1 profile showed per-frame `malloc` churn; a frame
  arena reset each tick is the textbook fix (zero fragmentation, O(1)
  free-all). This is the game-dev answer and was flagged as a deferred task
  during the free-list arc. More impactful than any coalescing.
- **(2) Empty-chunk scavenging (page-level) — if/when long-session stranding
  bites.** When a 64 KB chunk's blocks are all free, return it to a global
  chunk pool or `munmap` it. Needs a per-chunk live-count. This is the
  jemalloc-decay / Go-scavenger pattern, **not** object coalescing.
- **(3) Skip object-level coalescing.** Boundary tags + split/merge reintroduce
  exactly the invariant-heavy bug surface that just bit us (the reuse
  corruption took the whole arc to find). Little benefit at our scale
  (segregated classes don't fragment within a class).

## Concrete research questions for you

1. For each of jemalloc, tcmalloc, mimalloc, Go runtime: do they coalesce
   **objects**, or only **pages/spans**? What's the **return-to-OS** mechanism
   and its trigger (decay time, scavenge pacing)? Cite.
2. Game engines (Unreal, Unity, idTech/Doom, Bevy, Our Machinery/Zig
   gamedev): what allocators dominate — arena/linear/stack/pool vs a
   general heap? Is there ANY production game allocator that leans on
   object-level coalescing? Frame-allocator design specifics.
3. Real-time audio (JUCE, CLAP/VST hosts, PortAudio guidance): confirm the
   "no allocation in the audio callback" doctrine + the standard pre-alloc /
   lock-free patterns.
4. Graphics tooling / GPU memory (VMA, D3D12MA): is their sub-allocation
   (which sometimes DOES coalesce within a GPU heap) relevant to a CPU-side
   small-object allocator like B++'s, or a different problem?
5. The honest cost/benefit: at B++'s scale (games + tools + a self-hosting
   compiler, single-threaded-ish, ≤ low-MB working sets), does object
   coalescing buy anything that arenas + chunk-scavenging don't? When would
   it actually pay off?

## Constraints any recommendation must respect

- **Rule 41 (additive portability):** the policy lives in `bpp_mem.bsm`; the
  per-OS page primitives stay untouched so every target inherits it for free.
- **Bootstrap byte-stability** (gen1==gen2) + suite green are non-negotiable
  for any `bpp_mem` change (it's compiled into every binary, incl. the
  compiler).
- **The reuse-bug lesson:** adding split/merge/boundary-tag invariants is
  exactly the bug-surface class that cost us the 2026-05-27 hunt. The bar for
  adding such invariants is HIGH — prefer designs with fewer invariants
  (arenas have ~none; chunk-scavenging has one counter; object coalescing has
  many).
- **`_MEM_DEBUG` interaction:** whatever ships should compose with the poison
  mode (or be documented as orthogonal).

## Deliverable shape (mirror the file_stat study)

A comparative table (who coalesces objects vs pages vs neither; who returns to
OS how) + a phased recommendation (e.g. α = frame-arena adoption in rts1 only;
β = a reusable frame-arena pattern + chunk-scavenging in bpp_mem; γ = anything
heavier) with "do now vs defer" and the consumer that justifies each (Rule
36b — name the consumer). If the conclusion is "arenas + scavenging, skip
coalescing," say so plainly; if the market actually argues for coalescing at
our scale, show the evidence.

## Cross-references
- `docs/plans/sidequest_freelist_reuse_corruption.md` — the free-list arc +
  the reuse-bug post-mortem (why invariant-heavy designs scare us).
- `docs/plans/sidequest_debug_allocator.md` — `_MEM_DEBUG` (A0) + open A1/A2/B0.
- `src/bpp_mem.bsm`, `src/bpp_arena.bsm`, `stb/stbpool.bsm` — current state.
- `docs/manual/tonify_checklist.md` Rule 41 (portability), Rule 40 (hot-path
  malloc jitter — the per-frame-churn pressure that motivates arenas).
