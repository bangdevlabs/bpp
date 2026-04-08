# B++ Games Roadtrip

The path from B++ 0.21 to B++ 1.0. Three games, in order, each one
proving that B++ can do something the previous one could not. The
language reaches version 1.0 the day a complete indie retro game ships,
end to end, in pure B++.

## Vision

B++ is a language for making games. Not a toy language with a few demos.
Not a research project. A real tool that can carry a complete commercial
game from first line of code to shipped binary, with the engine, the
art, the audio and the runtime all written in B++.

The roadtrip below is the proof. Each game forces the language and the
standard library to grow exactly where the next game will need them.

```
B++ 0.21
   │
   ▼
┌──────────────────┐
│  Rhythm Teacher  │  ← drives stbaudio (WAV loader, mixer, timing)
└────────┬─────────┘
         │
         ▼
┌────────────────────────────────┐
│  Wolfenstein 3D (Phase 1: CPU) │  ← stress test CPU rendering
│  Wolfenstein 3D (Phase 2: GPU) │  ← engine composition + polish
└────────┬───────────────────────┘
         │
         ▼
┌──────────────────┐
│  RTS v1.0        │  ← graduation game
└──────────────────┘
       B++ 1.0
```

---

## Game 1 — Rhythm Teacher

**Goal**: drive `stbaudio.bsm` from zero to production-ready by building
a game that absolutely depends on audio.

A rhythm game cannot exist without precise timing, low-latency audio
playback, and a loader for real sound files. Snake can fake everything
with a few `putchar` calls. A rhythm game cannot.

### Design (minimum playable)

- 320×180 window. One song at a time. Notes scroll down four lanes.
- Player hits keys A, S, D, F as notes reach the target line.
- Timing accuracy graded as PERFECT / GOOD / OK / MISS.
- Score, combo counter, accuracy percentage.
- Background music loops in time with note chart.
- "Teacher mode": the game shows you which key to hit one bar before,
  so it doubles as a practice tool — hence the name.

### Capabilities this game forces

| Capability | Where it lands |
|---|---|
| WAV file loader (PCM 16-bit) | `stbaudio.bsm` — `audio_load(path)` |
| Mixer with N voice slots | `stbaudio.bsm` — `audio_play(voice, sample)` |
| Sample-accurate timing | `stbaudio.bsm` — frame counter from device |
| Streaming background music | `stbaudio.bsm` — looped voice slot |
| Audio platform layer (CoreAudio / ALSA) | `_stb_platform_macos.bsm` + `_stb_platform_linux.bsm` |
| JSON note chart loader | reuse `stbsprite.bsm` parser pattern |
| Frame-perfect input timing | already in `stbinput.bsm`, just stress-test |

### What ships

- `games/rhythm/rhythm.bpp` — the game
- `games/rhythm/assets/songs/*.wav` + `*.json` — at least 3 songs
- `stb/stbaudio.bsm` — production-ready audio module
- Updated `_stb_platform_macos.bsm` with CoreAudio AudioQueue setup
- Updated `_stb_platform_linux.bsm` with ALSA SND_PCM setup

### Estimate

1-2 weeks. Small game, high impact. Audio is a foundational module —
once it ships, every following game gets it for free.

---

## Game 2 — Wolfenstein 3D

**Goal**: prove that B++ can carry a real CPU rendering workload, then
upgrade the engine to hybrid CPU+GPU to prove that the existing 2D GPU
pipeline composes cleanly into a 3D-feeling renderer.

This is the "B++ is a language for games, not a toy" milestone.

### Phase 1 — Pure CPU Raycaster

The honest stress test. No GPU. Every pixel is computed and written by
B++ code. If this runs at 60 FPS on Apple Silicon, the language is
proven for real game work.

Modern hardware is on our side: Apple M4 has roughly 1000× the compute
of the 386 that ran the original Wolfenstein 3D at 70 FPS. Even with
B++ adding overhead vs C, there is comfortable headroom.

#### Architecture (CPU only)

1. Player has `(px, py)` and angle `theta` in a grid map (64×64 cells).
2. For each screen column (320 of them):
   - Compute the ray direction from `theta` and column index
   - Walk the grid using DDA (Digital Differential Analyzer) until a
     wall cell is hit (~4-10 steps)
   - Compute the perpendicular distance to the wall
   - Compute the wall's projected screen height
   - Sample the wall texture column at the hit point
   - Write the column of pixels to `_stb_fb` with vertical scaling
3. For each visible sprite (enemies, items):
   - Compute distance and screen-space angle
   - Skip if behind player or off-screen
   - Distance-sort, then draw back-to-front via the same scaled-column
     blit, sampling from the sprite texture with alpha tests
4. Present `_stb_fb` via `_stb_present()` (CPU framebuffer path that
   already exists for stbdraw)

#### Estimated CPU budget

| Work per frame | Operations |
|---|---|
| 320 raycasts × ~6 grid steps × ~10 ops | ~19K |
| 320 columns × ~80 pixels × ~5 ops/pixel | ~128K |
| Sprite billboards (50 sprites × ~200 ops) | ~10K |
| AI state machines + game logic | ~5K |
| **Total per frame** | **~162K ops** |
| At 60 FPS | **~10M ops/sec** |

Apple M4 single-core peak is roughly 10 billion ops/sec for simple
integer work. Even at 10× B++ overhead vs C, that leaves about 100×
headroom. The math says yes.

#### Phase 1 milestones

| Step | What | Proves |
|---|---|---|
| 1 | One hardcoded level, walls flat-color, no texture | DDA + projection math correct |
| 2 | Texture sampling per column, single texture | Per-pixel CPU rendering at speed |
| 3 | Multiple textures, grid-cell type lookup | Map decoding pipeline |
| 4 | Sprites: stationary enemies, billboards, Z-sort | Depth ordering by hand |
| 5 | Player movement, collision against grid | Game feels playable |
| 6 | `.WL1` shareware loader (vswap, maps, audio) | Real assets pipeline |
| 7 | Enemy AI: state machine via stbecs | ECS at scale |
| 8 | Doors, push walls, items, ammo, HUD | Complete vertical slice |
| 9 | Audio: gunshots, footsteps, dying enemies, music | stbaudio in production |

### Phase 2 — Hybrid CPU+GPU

After Phase 1 ships and proves B++ can render in pure software, the
engine upgrades to use the existing GPU pipeline. The CPU still does
all the math (raycaster, projection, sort) but the GPU handles the
expensive part: texture sampling and blending.

#### Architecture (hybrid)

```
CPU (B++)                          GPU (existing Metal/Vulkan pipeline)
─────────────                      ────────────────────────────────────
1. Raycaster math (DDA)            1. Receives ~320 textured quads
2. Per column: emit a vertical        (one per column)
   textured quad to the GPU        2. Receives ~50 sprite quads
   batch via _stb_gpu_vertex          (billboards with alpha)
3. Per visible sprite: emit a      3. Renders all of it in one batch
   billboard quad with depth-         draw call via _stb_gpu_present()
   based scale and texture UV
4. _stb_gpu_present() flushes
```

This is exactly how Doom 64 worked: software raycasting, hardware
texture mapping. The CPU is freed from per-pixel work, the GPU does
what it was built to do.

#### What this proves

- B++'s 2D GPU pipeline composes into a 3D-feeling renderer with no
  shader changes, no Vulkan-specific code, no platform-specific tricks.
- The same source compiles to native Metal on macOS and (when ELF
  dynamic linking lands) Vulkan on Linux.
- Engine modules in `stb/` are flexible enough to support genres they
  were never designed for.

#### Phase 2 milestones

| Step | What |
|---|---|
| 1 | Replace the CPU framebuffer blit with a `_stb_gpu_vertex` quad per column |
| 2 | Move sprite drawing to GPU billboards with alpha discard |
| 3 | Profile and confirm the hybrid is at least as fast as pure CPU |
| 4 | Optional polish: texture filtering, fog (per-vertex color), HUD overlays |

### Assets

Wolfenstein 3D was released as freeware in 1992 (Episode 1 only). The
shareware files `vswap.wl1`, `maplist.wl1`, `audiohed.wl1`, `audiot.wl1`
are freely distributable and contain ten complete levels with all the
original textures, sprites, sounds, and music.

The `.WL1` format is simple and well documented: a list of 64×64 chunks
with palette indices, plus a small map header. A loader in B++ is about
~150 lines and reuses the patterns already in `stbimage.bsm`.

Alternative paths if shareware files become a problem:
- CC0 fan-made Wolf3D-style sprite packs (OpenGameArt, itch.io)
- Original B++ assets, made with the future `stbart.bsm` sprite editor

The original Wolfenstein 3D source code was released under GPL by id
Software in 1995. It is freely readable as a reference for the
algorithm, but the B++ port will be written from scratch in idiomatic
B++ — not a line-by-line port.

### Phase 1 estimate

6-8 weeks. The pure CPU raycaster is well-understood (Lode Vandevenne's
raycasting tutorial is the canonical reference) and the modules it
depends on (`stbgame`, `stbdraw`, `stbimage`, `stbaudio`, `stbecs`) are
all in place by then.

### Phase 2 estimate

2-3 weeks. Mechanical translation of the CPU rendering loop into GPU
quad emission. No new modules required.

---

## Game 3 — RTS v1.0

**Goal**: ship the game that defines B++ 1.0. A complete, playable,
strategically interesting real-time strategy game with AI opponent,
written entirely in B++.

This is the graduation project. Everything the previous games
exercised lands here at the same time: tilemap rendering, pathfinding
at scale, ECS with hundreds of units, audio mixer with many concurrent
sounds, fog of war, UI, and a competent AI player.

### Reference candidates (open assets)

The original Warcraft 1 source code is not officially released. Three
realistic candidates with open or freely usable assets:

| Reference | Status | Notes |
|---|---|---|
| **Warcraft 1 / 2 (Stratagus + Wargus)** | Reverse-engineered C++ engine, fan project | Original assets need to be ripped from a purchased copy of Warcraft 2 — not freely distributable |
| **Command & Conquer / Red Alert (OpenRA)** | EA released the original game assets as freeware in 2008 | Fully legal, all assets free, OpenRA is the canonical free RTS reference |
| **0 A.D.** | Wildfire Games, fully open | All code and assets are CC-BY-SA. Cleanest legal path but not a 1990s nostalgia game |

The pragmatic recommendation is **OpenRA-style** (Red Alert assets,
freely distributed by EA) for nostalgia plus legal cleanliness. Final
decision deferred until Wolfenstein 3D ships.

### Capabilities this game forces

| Capability | Module |
|---|---|
| Tilemap rendering with camera scroll | `stbtile.bsm` (already done) |
| A* pathfinding for many units | `stbpath.bsm` (already done) |
| ECS with hundreds of entities | `stbecs.bsm` (already done) |
| Sprite animation (frame sequences, direction) | new `stbanim.bsm` or extend `stbsprite.bsm` |
| Selection box, command queue, formations | game-side, no new module |
| Fog of war (per-unit visibility, cached) | game-side, ~200 lines |
| Resource counters, build menus, minimap | `stbui.bsm` (already done) |
| Audio mixer with many concurrent sounds | `stbaudio.bsm` (already done by then) |
| AI opponent (build orders, tactical, strategic) | game-side, the hardest part |
| Save and load game state | new `stbsave.bsm`, ~150 lines |

### What B++ 1.0 looks like

- A complete RTS that two humans can play against the AI for an hour
  without it feeling like a tech demo
- Three or four playable maps
- A tutorial mission
- Build orders, unit production, combat resolution, victory conditions
- Sound effects, music with state transitions
- A `bpp` binary, an `rts` binary, and a folder of assets
- All written in pure B++, compiled by the bootstrapped B++ compiler,
  rendered by the B++ standard library, audio played by the B++ audio
  module
- Runs natively on macOS (Metal) and Linux (X11 + future Vulkan)

That is B++ 1.0.

### Estimate

6-12 months. The graduation project. Will likely surface dozens of
small language and standard library improvements as it grows.

---

## Module Dependencies

| Module | Status | Needed for | Notes |
|---|---|---|---|
| `stbaudio.bsm` | TODO | Rhythm, Wolf3D, RTS | Built during Game 1 |
| `stbanim.bsm` | TODO | RTS | Sprite animation frames + direction tables |
| `stbsave.bsm` | TODO | RTS | Save/load game state |
| `stbart.bsm` | TODO | optional asset creation | Not a hard dependency for any game |
| `stbtile.bsm` | DONE | Wolf3D map grid (loose), RTS | Already shipped |
| `stbphys.bsm` | DONE | optional | Wolf3D uses raycaster collision, not stbphys |
| `stbpath.bsm` | DONE | RTS | Already shipped |
| `stbsprite.bsm` | DONE | All three | Animation extension may be needed for RTS |
| `stbecs.bsm` | DONE | Wolf3D enemies, RTS units | Already proven in Pathfinder, Snake |
| `stbimage.bsm` | DONE | Asset loaders | Already shipped |
| `stbrender.bsm` | DONE | Wolf3D Phase 2, RTS | Already shipped |
| `stbdraw.bsm` | DONE | Wolf3D Phase 1, fallback | Already shipped |
| ELF dynamic linking | TODO (P0) | Vulkan on Linux | Blocks Linux GPU rendering for Phase 2 + RTS |
| Vulkan platform layer | TODO (P4) | Linux GPU | Depends on ELF dynamic linking |

---

## Performance Targets

| Game | Frame budget | Resolution | Notes |
|---|---|---|---|
| Rhythm Teacher | 16 ms (60 FPS) | 320×180 | Audio latency under 20 ms is the real constraint |
| Wolfenstein 3D Phase 1 (pure CPU) | 16 ms (60 FPS) | 320×180 | The honest stress test |
| Wolfenstein 3D Phase 2 (hybrid) | 4 ms (240 FPS-capable) | 640×360 or higher | GPU has plenty of headroom |
| RTS v1.0 | 16 ms (60 FPS) | 640×360 or higher | Hundreds of units + pathfinding + audio + AI |

If any game misses its budget, treat it as compiler optimization
information, not as a reason to lower the target. The point of the
roadtrip is to find where B++ needs to grow.

---

## Why this order

1. **Audio first.** A music game forces `stbaudio.bsm` to be real, not
   a stub. Every following game uses it heavily. Doing audio inside an
   FPS or RTS would be debugging two systems at once.
2. **CPU rendering second.** The pure CPU Wolfenstein 3D is the moment
   B++ stops being a toy. Modern hardware lets the language survive
   that test honestly.
3. **GPU composition third.** Once CPU rendering ships and works, the
   hybrid upgrade is mechanical. It demonstrates that the existing 2D
   GPU pipeline composes into 3D without shader changes.
4. **RTS last.** The graduation project needs every module the previous
   games delivered, plus animation and save state. Doing it before
   Wolfenstein would mean fighting AI design and CPU performance at
   the same time as fighting the audio module.

The roadtrip ends when a player downloads the RTS, plays it for an
hour, and never thinks once about the language it was written in.

---

## Open Architectural Questions

The roadtrip has two blocking design questions that must be resolved
before Game 1 is written. These are not implementation details — they
are decisions about the shape of B++ itself, and they affect every
game that follows.

### Threading Model for Audio (BLOCKING Rhythm Teacher)

B++ is single-threaded by design. CoreAudio (macOS) and ALSA (Linux)
both call your audio callback from a realtime audio thread that the
OS created. The callback has a hard deadline (typically 1–5 ms) and
must not allocate memory, take locks, or do blocking syscalls. It
must read audio samples from somewhere and copy them into a buffer
the OS provides.

The main thread needs to produce those samples (play a note, advance
a song cursor, mix voices) and hand them off to the callback. That
is a producer/consumer relationship across two threads.

C and C++ solve this with a single-producer single-consumer lock-free
ring buffer. The producer writes data, then advances a write index.
The consumer reads up to the write index, then advances a read index.
Aligned word-sized loads and stores are atomic at the hardware level
on both ARM64 and x86_64. The only thing missing is a memory barrier
between the data write and the index update — without it, the CPU
can reorder and the consumer sees the new index before the data.

**The minimum B++ needs** to support this pattern:

1. **One new builtin**: `mem_barrier()`. Emits `DMB ISH` on ARM64,
   `MFENCE` on x86_64. Five lines per codegen file.
2. **Function pointers are already addresses**: `fn_ptr(callback)`
   already returns the right thing for CoreAudio or ALSA to call. The
   audio thread can execute B++ code as long as that code follows
   audio callback rules (no malloc, no syscalls except the audio
   output write).

**The standard pattern** for `stbaudio.bsm` with this primitive:

```bpp
// Pre-allocated at init time. Size is a power of two so wrap is
// just a bitmask — no modulo, no branch.
auto _audio_ring;        // byte buffer, capacity = RING_SIZE
auto _audio_read_idx;    // consumer (audio thread) advances this
auto _audio_write_idx;   // producer (main thread) advances this

// Main thread: produce samples by writing to the ring.
audio_push(sample) {
    auto next;
    next = (_audio_write_idx + 1) & (RING_SIZE - 1);
    if (next == _audio_read_idx) { return 0; }   // ring full
    poke(_audio_ring + _audio_write_idx, sample);
    mem_barrier();                                // data visible before index
    _audio_write_idx = next;
    return 1;
}

// Audio thread: CoreAudio/ALSA callback runs this. No malloc, no
// syscalls (except the audio output write that the OS is already
// doing for us).
_audio_callback(out_buf, out_len) {
    auto i;
    for (i = 0; i < out_len; i = i + 1) {
        if (_audio_read_idx == _audio_write_idx) {
            poke(out_buf + i, 0);                 // underrun: silence
        } else {
            poke(out_buf + i, peek(_audio_ring + _audio_read_idx));
            mem_barrier();                        // data consumed before index
            _audio_read_idx = (_audio_read_idx + 1) & (RING_SIZE - 1);
        }
    }
    return 0;
}
```

This is the smallest addition to B++ that unblocks a professional
audio module. Everything else the audio module needs is already in
the language. The pattern generalizes: the same ring-buffer shape
works for network I/O callbacks, worker-thread messages, and any
other OS-threaded boundary B++ might meet later.

**Alternative: polling only.** If `mem_barrier()` is too much to add
for Rhythm Teacher, the fallback is to run audio entirely on the
main thread and poll an audio syscall every frame. This caps audio
latency to the frame time (~16 ms at 60 FPS), which works for most
games but is borderline for a rhythm game. Real rhythm games expect
5–10 ms total latency, reachable only with a proper callback thread.

**The decision**: add `mem_barrier()` as a builtin before Game 1
starts. It is the highest-leverage primitive addition possible —
five lines of compiler code per backend unlock decades of lock-free
concurrent programming patterns, and every audio / networking /
worker-pool module that comes after inherits it for free.

### CI and regression catching (BLOCKING everything beyond Game 1)

Today the 52-test suite passes because the maintainer remembers to
run it. This worked for 0.21 because the maintainer wrote most of it
in one head. As the roadtrip grows (5+ new modules during Rhythm
Teacher alone, many more later), regression catching-in-the-head
stops scaling.

**The minimum setup** that buys a lot:

1. A single shell script `tests/run_all.sh` that compiles and runs
   every `tests/test_*.bpp` and reports `52/52 passing` or lists
   failures. This already exists in the README as a snippet — move
   it into a committed file.
2. The script runs at the end of every bootstrap cycle in
   `install.sh`, so installation itself is gated on tests passing.
3. Optional: a Git pre-push hook that runs the script. Manual but
   cheap to add.
4. Later: a GitHub Actions workflow that runs the same script on
   macOS and Ubuntu runners on every push and PR.

**Not in scope for the roadtrip**: elaborate CI infrastructure, test
coverage reporting, benchmark tracking. The goal is "don't regress
silently between games", not "solve testing as a discipline".

**The decision**: `tests/run_all.sh` + `install.sh` integration
before Rhythm Teacher ships. GitHub Actions whenever the project
goes public.

---

## Status

| Milestone | State | Date |
|---|---|---|
| B++ 0.21 | shipped | 2026-04-06 |
| `mem_barrier()` builtin | pending (blocks Game 1 threading) | — |
| `tests/run_all.sh` + install.sh gate | pending (blocks Game 1 regression safety) | — |
| Rhythm Teacher | not started | — |
| Wolf3D Phase 1 | not started | — |
| Wolf3D Phase 2 | not started | — |
| RTS v1.0 → B++ 1.0 | not started | — |
