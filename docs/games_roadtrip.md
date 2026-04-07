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

## Status

| Milestone | State | Date |
|---|---|---|
| B++ 0.21 | shipped | 2026-04-06 |
| Rhythm Teacher | not started | — |
| Wolf3D Phase 1 | not started | — |
| Wolf3D Phase 2 | not started | — |
| RTS v1.0 → B++ 1.0 | not started | — |
