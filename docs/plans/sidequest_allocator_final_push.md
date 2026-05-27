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

---

# Market study + design lock (2026-05-27, session 2)

## Hypothesis validated

The original hypothesis ("object-level coalescing is NOT modern game-dev practice") is
**confirmed by the cousins**. The 4 modern general-purpose allocators all converged on
the same design: **size-class segregation for objects (no object coalescing) +
page/span-level coalescing + decay-based return-to-OS**. Game engines use arenas.
Audio realtime forbids allocation in callbacks. The only domains where object
coalescing is still the answer are (a) legacy general malloc (glibc/dlmalloc) and
(b) bare-metal embedded (FreeRTOS heap_4), and for very different reasons.

### Comparative table — modern general allocators

| Allocator | Object coalesce? | Page coalesce? | Return-to-OS | Trigger |
|---|---|---|---|---|
| jemalloc | ✗ | ✓ (extents) | `madvise MADV_FREE` (muzzy) / `MADV_DONTNEED` | `dirty_decay_ms=10s` default, decays over 200 steps |
| tcmalloc | ✗ | ✓ (spans) | `madvise MADV_FREE` | `scavenge_counter_` paced |
| mimalloc | ✗ | ✓ (segments + arenas) | reclaim whole segments | abandoned-segment bitmap |
| Go runtime | ✗ | ✓ (mheap span coalesce) | `MADV_DONTNEED` / `MADV_FREE` / `VirtualFree` | background scavenger |
| dlmalloc/glibc (legacy) | ✓ (boundary tags) | ✓ | sbrk shrink / madvise (limited) | malloc_consolidate periodic |
| Emscripten dlmalloc (WASM) | ✓ | ✓ | **N/A** (no munmap) | — |
| FreeRTOS heap_4 (bare-metal) | ✓ (adjacent merge) | N/A | **N/A** (no OS) | on every `free()` |
| B++ today | ✗ | ✗ | ✗ | — |

### Comparative table — game / audio / GPU consensus

| System | Pattern | Notes |
|---|---|---|
| Jai `temp_allocator` | Linear bump + reset, context-scoped | Built-in language primitive |
| Odin `context.temp_allocator` | Same shape | Built-in |
| Unreal `FMemStack` | Greedy stack frame allocator (segments + free-list) | Push/Pop via `FMemMark`; segments never returned to OS while alive |
| Unreal `FMallocArena` | Arena wrapper | Per-subsystem |
| idTech 4 Hunk Allocator | 2 stack arenas (permanent + temp) sharing a region | Highwater marks; Quake/Doom design |
| idTech 4 Zone Allocator | Smaller volatile allocations | Distinct from Hunk |
| JUCE / CLAP / pluginval | **Zero alloc in audio callback** | Doctrine codified; pluginval validates; community asks for assert-on-malloc on audio thread |
| Vulkan VMA | Buddy + suballocation with coalesce **inside** 256 MB blocks | Different problem: GPU memory sub-allocation, not CPU small-object |
| Bevy / Rust gamedev | Per-frame arenas + ECS DOD storage | Pattern identical to Jai/Odin |

**Zero production game engines centre their allocator on object-level coalescing.**
The pattern is universal: arena + pool + (sometimes) general malloc as escape hatch.

### The two nuances WASM + bare-metal reveal

- **WASM linear memory**: `memory.grow` is monotonic. **No `munmap`, no `madvise`.** Decay-based
  return-to-OS (the jem/tc/mi/Go strategy) simply does not exist. The only fragmentation
  defenses available are (a) coalescing or (b) arena discipline (peak working-set bound).
  Emscripten ships dlmalloc by default precisely because it coalesces — without that,
  fragmentation grows unbounded.
- **Bare-metal (FreeRTOS heap_4, BangOS-class targets)**: no OS to return to either, but
  working sets are KB not MB, so coalescing makes sense at that scale.

The arenas-first answer is the only pattern that scales across all four target categories
(macOS, Linux, WASM, bare-metal) **without** needing target-specific allocator logic. That
makes α the most portable answer per Rule 41.

## Decision (locked)

**Do α + β. Skip γ.**

### α — Frame-arena pattern for rts1 (and every future game) — **DO NOW**

Adopt `bpp_arena` (already exists) for per-frame allocation churn in rts1's `game_tick`.

**Mechanism**: one `_frame_arena` initialized in `main`, `arena_reset(_frame_arena)` at the
end of each tick. Per-frame transients (pathfinding scratch, ECS query intermediates, anything
that lives a single tick) call `arena_alloc(_frame_arena, n)` instead of `malloc(n)`.

**Why first**:
- Industry-validated pattern (Jai/Odin/Unreal/idTech all converge).
- `bpp_arena` exists; only adoption is needed. No risk in `bpp_mem`.
- Scales to 100% of target categories (hosted OS + WASM + bare-metal). Arenas work
  anywhere with contiguous memory.
- Covers ~90% of the rts1 profile's residual `_mem_alloc_pages` without touching the
  small-block allocator.

**Consumer** (Rule 36b): rts1's `game_tick` (profile already named the per-frame
allocators — `_path_*`, `ecs_query_each`, etc.). fps_wolf3d and snake-wasm follow when
they adopt the pattern.

**Validation gates**:
- rts1 profile after adoption: `_mem_alloc_pages` should drop further; new top will be
  game work (vertex emit, ECS, pathfinding).
- Bootstrap byte-stable (changes are in `games/rts1/` only).
- Suite 180/0/12.

### β — Chunk-scavenging in `bpp_mem` — **DO AFTER α validates**

Empty 64 KB small-class chunks return to the OS (or stay parked in a global free-chunk
list on no-OS targets).

**Mechanism**:
1. Reserve first 16 bytes of each carved 64 KB chunk for inline metadata:
   `[live_count: u64 | reserved: u64]`. The carved-block region starts at chunk_base + 16
   (already aligned by `_MEM_HEADER`).
2. `malloc` increments the chunk's `live_count` whenever it hands out a block from that
   chunk (both carve path and free-list pop). Chunk_base is recovered from any block
   pointer via `block_addr & ~(_MEM_CHUNK_SIZE - 1)` (chunks come page-aligned from
   `_mem_alloc_pages`).
3. `free` decrements the chunk's `live_count`.
4. When `live_count == 0`: the chunk is fully unused, but its blocks may still be on the
   class free-list. Walk the class's free-list, unlink any block whose chunk-base matches,
   then call `_mem_free_pages(chunk_base, _MEM_CHUNK_SIZE)`.
5. Per-OS behaviour:
   - macOS/Linux: `_mem_free_pages` is `munmap` — memory returned to the kernel.
   - WASM (future): `_mem_free_pages` is a no-op (linear memory can't shrink). The chunk
     is unlinked from the class but stays in linear memory — the next chunk request can
     re-carve it via a small "abandoned chunks" pool. Coherent with how Emscripten
     dlmalloc handles the constraint.
   - Bare-metal (future): `_mem_free_pages` is whatever the target defines (e.g. push to
     a kernel-level chunk pool). Layer-6 decides.

**Cost**: ~50 LOC in `bpp_mem.bsm`. One u64 counter per chunk (8 bytes of overhead per 64
KB = 0.012%). Free-list walk on chunk-empty event is O(blocks-in-class), but rare —
amortizes to O(1) per free.

**Invariants** (the bug-surface budget — keep small):
- Chunk metadata is at known offset; never moves; never invalidated until chunk is munmap'd.
- `live_count` must be incremented before user gets the pointer, decremented before
  free-list push (or after; pick one and assert).
- Free-list walk on empty-chunk event must run BEFORE the munmap, or the next malloc
  will pop a freed pointer.
- `_MEM_DEBUG` poison still composes — chunk's blocks were poisoned at free time, and
  walk-and-unlink just unlinks them without touching content.

**Consumer**: Bang 9 long-edit sessions + modulab loading/freeing large PNG buffers. Two
named consumers per Rule 36b.

**Validation gates**:
- Suite 180/0/12 with `_MEM_REUSE=1`.
- `_MEM_DEBUG=1` + `_MEM_REUSE=1` smoke pass (no false UAF detection from the walk).
- Bench `bench_compile.sh` — bootstrap stays ≤ 0.22s (target: no regression; chunk
  metadata is per-chunk, not per-allocation).
- Bootstrap byte-stable (gen1 == gen2).
- A repro test that allocates 200×16 B, frees all, checks that the chunk's pages
  reported by Mach `vm_region`/Linux `/proc/self/maps` actually shrink.

### γ — Object-level coalescing — **NOT DOING**

Industry abandoned it (jem/tc/mi/Go all skipped). Bug surface mirrors exactly what bit us
on the reuse hunt (boundary tags + split/merge invariants). At B++'s scale (1–100 MB
working sets), segregated classes plus β cover all observed fragmentation. The case for
γ would require workloads B++ is not planned to take on (long-running server-like, with
phase-varying allocations across many classes).

## Sources

- jemalloc decay/madvise: [GitHub issue #956](https://github.com/jemalloc/jemalloc/issues/956), [docs issue #2751](https://github.com/jemalloc/jemalloc/issues/2751), [How JeMalloc Works](https://yfractal.github.io/systems/memory-management/2022/10/04/jemalloc.html)
- TCMalloc: [Google design doc](https://google.github.io/tcmalloc/design.html), [DeepWiki overview](https://deepwiki.com/gperftools/gperftools/2-tcmalloc)
- mimalloc: [GitHub readme](https://github.com/microsoft/mimalloc), [src/segment.c](https://github.com/microsoft/mimalloc/blob/main/src/segment.c)
- Go runtime: [internals-for-interns](https://internals-for-interns.com/posts/go-memory-allocator/), [Memory Scavenger](https://medium.com/@AlexanderObregon/memory-scavenger-in-go-runtime-b517147c0928)
- Unreal `FMemStack`: [Greedy Stack Frame Allocator](https://nfrechette.github.io/2016/05/09/greedy_stack_frame_allocator/), [Epic docs](https://docs.unrealengine.com/4.26/en-US/API/Runtime/Core/Misc/FMemStackBase/)
- idTech: [Quake III memory](https://realityforge.org/Quake-III-Arena/idTech/memory.html), [Doom allocator overview](https://bookdown.org/robertness/doom_tour/3_1_memory_allocator.html)
- Audio realtime: [JUCE pluginval](https://forum.juce.com/t/pluginval-real-time-safety-checking/67439/1), [assert-on-malloc FR](https://forum.juce.com/t/fr-option-for-assert-on-audio-thread-allocation-or-syscall/58151)
- Vulkan VMA: [custom pools](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/custom_memory_pools.html), [Lumina's overview](https://www.dr-elliot.com/lumina/vulkanmemoryallocations/)
- WASM allocators: [Emscripten settings](https://emscripten.org/docs/tools_reference/settings_reference.html), [scaling multithreaded Wasm with mimalloc](https://web.dev/articles/scaling-multithreaded-webassembly-applications)
- Bare-metal: [FreeRTOS heap_1..5](https://medium.com/@akashkadamwork2026/memory-management-in-freertos-heap-1-to-heap-5-17ee7630788c), [Mastering FreeRTOS](https://freertos.gitbook.io/mastering-the-freertos-tm-real-time-kernel/mastering.ch03)
- Jai: [Way to Jai book — allocators](https://github.com/Ivo-Balbaert/The_Way_to_Jai/blob/main/book/21A_Memory_Allocators_and_Temporary_Storage.md), [Jai context (Lernö)](https://medium.com/@christoffer_99666/a-little-context-d06dfdec79a3)
