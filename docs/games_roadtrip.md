# B++ Games Roadtrip

The path from B++ 0.22 to B++ 1.0. Five games across six phases, in
order, each one proving that B++ can do something the previous one
could not. **Each game is a demo-level vertical slice**, not a complete
clone. The goal is to prove the production pipeline — that B++ can
carry a real game loop end to end — not to ship commercial-quality
titles. The language, the toolchain, and **bangscript** (the adventure
game DSL) are what ship at 1.0. The games are the proof.

## Vision

B++ is a language for making games. Not a toy language with a few demos.
Not a research project. A real tool that can carry a complete indie
retro game from first line of code to shipped binary, with the engine,
the art, the audio and the runtime all written in B++.

The roadtrip below is the proof. Each game forces the language, the
standard library, AND the developer workflow to grow exactly where the
next game will need them.

### Scope discipline: demo level, not full clone

The five games are proof points, not products. A "demo level" means:

- **Rhythm Teacher**: 3 songs, 1 difficulty, core loop complete. Not a
  song pack. Not a leaderboard. Not multiplayer.
- **Wolfenstein 3D**: 1 playable level with 1 enemy type, 1 weapon, and
  the core raycaster working at 60 fps. Not the full E1 campaign.
- **RPG Dungeon**: 1 dungeon room, 1 NPC, 1 enemy, 1 item, dialogue
  and inventory functional. Not a full Zelda overworld.
- **RTS**: 1 map, 2 unit types, 1 building, core combat + pathfinding.
  Not the full tech tree. Not the AI opponent with build orders.
- **Adventure Puzzle**: 1 scene, 3-5 hotspots, 1 multi-step puzzle,
  verb system and cutscene functional. Not a full Monkey Island chapter.

This discipline is load-bearing. Building the full games would take
years. Building one well-chosen vertical slice of each proves the
pipeline in weeks to months. When a player downloads the Adventure
demo, solves the puzzle, watches the cutscene, and the demo is
complete from intro to credits without ever thinking about the
language it was written in, the proof is in. That's B++ 1.0.

```
B++ 0.22
   │
   ▼
┌──────────────────────────────────┐
│  Dev loop foundation (see below) │  ← multi-error, debugger, profiler
└────────┬─────────────────────────┘
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
         │                             (hot reload lands here)
         ▼
┌──────────────────┐
│  RPG Dungeon     │  ← UI foundation: dialogue, inventory, menu, save
└────────┬─────────┘
         │                             (metaprogramming lands here)
         ▼
┌──────────────────┐
│  RTS demo        │  ← scale + concurrency graduation
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  bangscript      │  ← adventure game DSL (see bangscript_plan.md)
│  Adventure Puzzle│  ← the cherry on top
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

4-6 weeks for the demo level (1 level, 1 enemy type, 1 weapon, playable
start-to-finish). The pure CPU raycaster is well-understood (Lode
Vandevenne's raycasting tutorial is the canonical reference) and the
modules it depends on (`stbgame`, `stbdraw`, `stbimage`, `stbaudio`,
`stbecs`) are all in place by then. **Profiler must land before this
phase starts** — without it, hitting 60 fps becomes Sherlock with
printf.

### Phase 2 estimate

2-3 weeks. Mechanical translation of the CPU rendering loop into GPU
quad emission. No new modules required. Hot reload should land here or
between Phase 2 and the RTS — tuning the feel (weapon recoil, enemy
speed, level layout) is where hot reload pays for itself.

---

## Game 3 — RTS demo

**Goal**: ship a vertical slice that defines B++ 1.0. A playable
real-time strategy scenario with tilemap, unit pathfinding, audio,
and enough game state to prove the production pipeline end to end.

Demo scope: **1 map, 2 unit types, 1 building, core combat**. No full
tech tree. No build-order AI. No victory conditions beyond "destroy
the enemy base". This is deliberately narrow — the goal is to prove
that B++ can carry a simultaneously-complex game loop (rendering +
pathfinding + audio + AI + UI all ticking at 60 fps), not to ship
a commercial RTS.

Everything the previous games exercised lands here at the same time:
tilemap rendering, pathfinding, ECS with dozens of units, audio mixer
with several concurrent sounds, fog of war, UI, and a simple AI
opponent. Metaprogramming lands during this phase because the RTS is
where boilerplate hurts the most (multiple unit types × save/load/
update/render/debug).

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

### Capabilities this demo forces

| Capability | Module |
|---|---|
| Tilemap rendering with camera scroll | `stbtile.bsm` (already done) |
| A* pathfinding for several units | `stbpath.bsm` (already done) |
| ECS with dozens of entities | `stbecs.bsm` (already done) |
| Sprite animation (frame sequences, direction) | new `stbanim.bsm` or extend `stbsprite.bsm` |
| Selection box, command queue | game-side, no new module |
| Fog of war (per-unit visibility) | game-side, ~150 lines |
| Resource counters, minimap | `stbui.bsm` (already done) |
| Audio mixer with several concurrent sounds | `stbaudio.bsm` (already done by then) |
| Simple AI opponent (reactive, not strategic) | game-side |
| Save and load scenario state | new `stbsave.bsm`, ~150 lines |

### Estimate

2-4 months for the demo vertical slice. Shorter than a full RTS would
ever be, because the scope is locked at "one map, two units, basic
combat". Will likely surface a dozen small language and standard
library improvements as it grows.

---

## Game 4 — RPG Dungeon

**Goal**: build the UI foundation that Adventure and RTS both need —
dialogue, inventory, menus, and save/load — by making a game that
depends on all four.

A top-down dungeon room (Zelda / Pokemon style). One NPC with a
dialogue tree, one enemy, one item to collect, one treasure to find.
The game proves that B++ can handle narrative + UI-heavy gameplay, not
just action.

### Design (minimum playable)

- 320×240 window. Top-down tilemap, 1 room (16×12 tiles).
- Player moves with arrow keys, interacts with E key.
- 1 NPC with branching dialogue (stbdialogue).
- 1 item in a chest (stbinventory).
- 1 enemy that follows the player (stbpath for A*).
- Pause menu with save/load (stbmenu + stbsave).
- Victory condition: collect item, talk to NPC, exit room.

### Capabilities this game forces

| Capability | Where it lands |
|---|---|
| Dialogue boxes with word wrap and choices | new `stbdialogue.bsm`, ~200 lines |
| Inventory with item slots | new `stbinventory.bsm`, ~150 lines |
| Nested menus with keyboard navigation | new `stbmenu.bsm`, ~200 lines |
| Save and load game state | new `stbsave.bsm`, ~150 lines |
| Tilemap collision and movement | `stbtile.bsm` + `stbphys.bsm` (already done) |
| NPC pathfinding | `stbpath.bsm` (already done) |
| Font rendering for dialogue | `stbfont.bsm` (already done) |
| Audio: ambient + SFX | `stbaudio.bsm` (already done by then) |

### Assets

Clean legal path: Tuxemon (CC-BY-SA), LPC sprites (CC0), Kenney
dungeon tileset (CC0). All freely usable for demo purposes.

### What ships

- `games/rpg/rpg.bpp` — the game
- `games/rpg/assets/` — tileset, sprites, dialogue JSON
- `stb/stbdialogue.bsm` — dialogue module (reused by Adventure)
- `stb/stbinventory.bsm` — inventory module (reused by Adventure)
- `stb/stbmenu.bsm` — menu module (reused by RTS command UI)
- `stb/stbsave.bsm` — save/load module (reused by RTS and Adventure)

### Estimate

2-3 weeks. The gameplay is simple; the value is the four new UI
modules that every subsequent game inherits.

---

## Game 5 — Adventure Puzzle (the cherry on top)

**Goal**: prove that B++ can express narrative-driven, puzzle-based
gameplay through **bangscript**, B++'s embedded adventure game DSL.
This is the game the project was born to make.

See `docs/bangscript_plan.md` for the full bangscript specification.

An adventure scene in the style of Indiana Jones: Fate of Atlantis,
Monkey Island 2, and Gabriel Knight. Point-and-click verb system,
hotspot interactions, inventory puzzles, and a cinematic cutscene.
The game proves that B++ can handle expressiveness, not just
performance.

### Design (prologue scope — 1.0 target)

- 640×360 window. Pre-rendered background, character sprites.
- SCUMM-style verb bar at the bottom (LOOK, USE, TALK, PICK UP, etc.).
- Mouse-driven: click verb, click hotspot, watch result.
- 1 scene with 3-5 interactive hotspots.
- 1 multi-step puzzle (find item → combine → use on target).
- 1 short cutscene (walk, talk, camera pan, fade).
- Inventory panel with 3-4 items.
- Save point.

### Stretch: episode zero (~20-30 min gameplay)

If the prologue ships cleanly and the artist delivers assets for
multiple scenes: 3-4 scenes, multiple interconnected puzzles,
inventory progression, and a cinematic intro. This is the demo people
will remember.

### Capabilities this game forces

| Capability | Where it lands |
|---|---|
| Coroutine scheduler (frame-spanning jobs) | new `stbbangs.bsm`, ~400 lines |
| Scene graph (rooms, hotspots, exits) | new `stbscene.bsm`, ~200 lines |
| Cutscene primitives (walk_to, say, cam_pan) | new `stbcutscene.bsm`, ~250 lines |
| Verb/interaction system (USE X WITH Y) | new `stbverb.bsm`, ~200 lines |
| Cursor state machine (walk/look/use modes) | new `stbcursor.bsm`, ~80 lines |
| Compiler: `yield`/`await`/`wait` keywords | ~200 lines in lexer/parser/codegen |
| Compiler: time literals (`3s`, `500ms`, `2f`) | ~50 lines in lexer |
| Compiler: `scene { }` / `cutscene { }` blocks | ~150 lines in parser |
| Compiler: simple pattern match for verb system | ~300 lines in parser/metaprog |
| Dialogue trees with branches and flags | `stbdialogue.bsm` (from RPG) |
| Inventory with item combinations | `stbinventory.bsm` (from RPG) |
| Save/load puzzle state | `stbsave.bsm` (from RPG) |

### Assets

Original art by a confirmed artist. No copyright issues. The asset
pipeline uses `stbimage.bsm` for PNG backgrounds and `stbsprite.bsm`
for character sprites — both already shipping.

### What ships

- `games/adventure/adventure.bpp` — the game (bangscript syntax)
- `games/adventure/assets/` — backgrounds, sprites, audio
- `stb/stbbangs.bsm` — coroutine scheduler
- `stb/stbscene.bsm` — scene graph
- `stb/stbcutscene.bsm` — cutscene primitives
- `stb/stbverb.bsm` — verb system
- `stb/stbcursor.bsm` — cursor state machine
- Compiler extensions: coroutines, time literals, block parsing,
  pattern match (~700 lines across lexer/parser/codegen)

### What B++ 1.0 looks like

When a player downloads the Adventure demo, solves the puzzle, watches
the cutscene play out with walking actors, panning camera, and fading
music, and the demo is complete from intro to credits without ever
thinking about the language it was written in — that is B++ 1.0.

Five demo games covering five fundamental game categories:
- Audio + timing precision (Rhythm Teacher)
- CPU rendering + 3D math (Wolf3D Phase 1)
- GPU composition (Wolf3D Phase 2)
- Narrative + UI (RPG Dungeon)
- Scale + concurrency (RTS)
- Expressiveness + DSL (Adventure via bangscript)

A production pipeline proven, a scripting language born, and a
community that sees what B++ is for.

### Estimate

4-6 weeks after bangscript compiler extensions and runtime modules
ship. ~500-800 lines of game code. The prologue is achievable; the
episode zero depends on asset delivery pace.

---

## Module Dependencies

| Module | Status | Built during | Used by |
|---|---|---|---|
| `stbaudio.bsm` | TODO | Rhythm Teacher | All games |
| `stbdialogue.bsm` | TODO | RPG Dungeon | RPG, Adventure |
| `stbinventory.bsm` | TODO | RPG Dungeon | RPG, Adventure |
| `stbmenu.bsm` | TODO | RPG Dungeon | RPG, RTS |
| `stbsave.bsm` | TODO | RPG Dungeon | RPG, RTS, Adventure |
| `stbanim.bsm` | TODO | RTS | RTS, RPG, Adventure |
| `stbbangs.bsm` | TODO | bangscript | Adventure |
| `stbscene.bsm` | TODO | bangscript | Adventure |
| `stbcutscene.bsm` | TODO | bangscript | Adventure |
| `stbverb.bsm` | TODO | bangscript | Adventure |
| `stbcursor.bsm` | TODO | bangscript | Adventure |
| `stbart.bsm` | TODO | post-1.0 | optional asset creation |
| `stbtile.bsm` | DONE | 0.21 | Wolf3D (loose), RPG, RTS |
| `stbphys.bsm` | DONE | 0.21 | RPG collision |
| `stbpath.bsm` | DONE | 0.21 | RPG enemies, RTS units |
| `stbsprite.bsm` | DONE | 0.21 | All games |
| `stbecs.bsm` | DONE | 0.21 | Wolf3D, RPG, RTS |
| `stbimage.bsm` | DONE | 0.20 | Asset loaders |
| `stbrender.bsm` | DONE | 0.20 | Wolf3D Phase 2, RTS, RPG, Adventure |
| `stbdraw.bsm` | DONE | 0.20 | Wolf3D Phase 1, fallback |
| ELF dynamic linking | TODO (P1) | — | Vulkan on Linux |
| Vulkan platform layer | TODO (P3) | — | Linux GPU |

---

## Performance Targets

| Game | Frame budget | Resolution | Notes |
|---|---|---|---|
| Rhythm Teacher | 16 ms (60 FPS) | 320×180 | Audio latency under 20 ms is the real constraint |
| Wolfenstein 3D Phase 1 (pure CPU) | 16 ms (60 FPS) | 320×180 | The honest stress test |
| Wolfenstein 3D Phase 2 (hybrid) | 4 ms (240 FPS-capable) | 640×360 or higher | GPU has plenty of headroom |
| RPG Dungeon | 16 ms (60 FPS) | 320×240 | UI-bound, not GPU-bound |
| RTS | 16 ms (60 FPS) | 640×360 or higher | Hundreds of units + pathfinding + audio + AI |
| Adventure Puzzle | 16 ms (60 FPS) | 640×360 | Cutscene coroutines are the constraint, not rendering |

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
4. **RPG fourth.** The RPG is simpler than what follows but forces the
   four UI modules (dialogue, inventory, menu, save) that both the RTS
   and Adventure need. Building these inside an RTS or adventure game
   would mean fighting UI design and game logic at the same time.
5. **RTS fifth.** The scale graduation project needs every module the
   previous games delivered, plus animation. The pathfinding, audio,
   and UI modules arrive battle-tested from earlier games.
6. **Adventure last.** The cherry on top. bangscript is the biggest
   language addition in 1.0. It needs metaprogramming, coroutines,
   hot reload, and every UI module to be mature. Placing it after the
   RTS means the entire infrastructure is proven before the DSL goes
   live. The adventure game is why B++ exists — it ships last because
   it deserves the strongest foundation.

The roadtrip ends when a player downloads the Adventure demo, solves
the puzzle, watches the cutscene, and never thinks once about the
language it was written in.

---

## Dev Loop Foundation (all five ship WITH 1.0)

The roadtrip surfaced a realization that changes the shape of 1.0: the
bottleneck on the path to a shippable game is not "can B++ express
this?" (it can) but "how many iterations per hour can the developer
do?" (not enough yet). A solo developer building the roadtrip spends
more time waiting on the dev loop than writing code, which is both a
velocity problem AND a motivation problem for a multi-month solo
project.

Five dev loop capabilities are therefore **blockers for 1.0**, not
nice-to-haves deferred to 1.1. They land in a specific order aligned
with the games that need them:

| # | Item | Ships before | Why |
|---|---|---|---|
| 1 | Multi-error + warning log | Wolf3D Phase 1 | Compiler today exits on first `diag_fatal` via `sys_exit(1)`. Real code has multiple errors per compile; fix-one-recompile is painful at scale. Fixing the diagnostic system first makes the other four dev loop items much less painful to build. |
| 2 | `bug` with breakpoints | Wolf3D Phase 1 | Wolf3D has state-machine enemy AI and collision logic. Without step-through, a "why did the enemy walk through the wall at frame 523?" bug takes hours instead of minutes. B++ already has `bug` v2 with debugserver/gdbserver — this is adding a breakpoint API on top, reusing the existing infrastructure. |
| 3 | Profiler (minimal + sampling) | Wolf3D Phase 1 runs | Wolf3D CPU at 60 fps is the honest benchmark. If it drops to 30 fps, without a profiler the developer is Sherlock with printf. Two tiers: a `bench("label") { ... }` builtin for manual measurement (~30 lines), and a sampling profiler that captures stack traces at N ms intervals and aggregates by function (~300 lines, reuses the `bug` stack walker). The sampling profiler answers "where is the hot spot?" without any instrumentation. |
| 4 | Hot reload (watch mode) | RTS demo starts | `bpp --watch game.bpp` detects source changes, recompiles to a fresh `/tmp` path, kills the old process, restarts. Not true live code injection (hard), just full restart automation. Cuts 80% of the iteration atrito for the fraction of cost. Hot reload becomes critical when tuning gameplay feel — weapon recoil, enemy speed, level geometry, audio filters — where the dev needs to see the effect in under 5 seconds. |
| 5 | Metaprogramming (`$T` generics + compile-time reflection) | RTS demo mid-development | The RTS is where boilerplate hurts most: N unit types × (save / load / update / render / debug) = NxM hand-written functions. Jai is the reference — `$T` generic parameters + `#run` compile-time execution + struct field reflection is enough to generate all of it from a schema. B++ already has `const FUNC` compile-time execution; the leap is exposing struct field metadata and letting compile-time code emit new functions. Biggest scope of the 5 (~1500-2500 lines) but the one with the most multiplicative payoff on the RTS. |

### Why this order is load-bearing

Each item unlocks the next game to be built with less pain:

- Items 1-3 land before Wolf3D Phase 1 because Wolf3D is where "can B++
  hit 60 fps on CPU?" becomes a live question. Without error recovery
  the dev loop is slow, without the debugger game logic bugs take
  hours, without the profiler frame timing is blind.
- Item 4 lands before RTS because tuning an RTS (unit balance, combat
  feel, economy pacing) needs iteration counts that only hot reload
  delivers.
- Item 5 lands during the RTS because that's the first game with
  enough entity types to feel the boilerplate pain. Landing it earlier
  would be premature (Wolf3D has 1-2 unit types, metaprogramming pays
  nothing).

The total scope of the five is meaningful (~3000-4000 lines of
compiler and tooling work combined) but they share infrastructure with
the games themselves (the debugger reuses `bug`, the profiler reuses
the stack walker, metaprogramming reuses `const FUNC`). The actual
marginal cost of adding the five is lower than it sounds because the
foundation is already there.

### Threading for audio — RESOLVED by maestro

An earlier draft of this document listed "threading model for audio"
as an open architectural question. That question is now resolved: the
maestro pattern (`stbmaestro` + `stbjob` + `stbbeat`) lands before
Rhythm Teacher and provides the full threading story. The original
`mem_barrier()` primitive is now one of the handful of new builtins
added alongside the maestro work. The audio callback runs as a
thread-pool consumer using the same SPSC ring buffer pattern described
in the earlier draft, just wrapped in the standard maestro/stbjob API.
See `docs/maestro_plan.md` for the full specification.

### Regression catching — RESOLVED by `tests/run_all.sh` gate

The earlier draft also listed regression catching as open. Resolved:
`tests/run_all.sh` is commit 0 of the maestro plan and blocks every
commit that follows. Also wired into `install.sh` so installation
itself is gated on tests passing.

---

## Status

| Milestone | State | Date |
|---|---|---|
| B++ 0.22 | shipped | 2026-04-08 |
| `tests/run_all.sh` + install.sh gate | done (maestro plan commit 0) | 2026-04-08 |
| Maestro Phase 1 (stbmaestro/stbjob/stbbeat) | done | 2026-04-08 |
| Function dedup + mod0_real_start fix | done | 2026-04-09 |
| Maestro batch 2 (standalone runtime) | in progress | — |
| `extrn` keyword + auto smart promotion | pending | — |
| Multi-error + warning log | pending (blocks Wolf3D Phase 1) | — |
| `bug` with breakpoints | pending (blocks Wolf3D Phase 1) | — |
| Profiler (minimal + sampling) | pending (blocks Wolf3D Phase 1 runs) | — |
| Rhythm Teacher demo | not started | — |
| Wolf3D Phase 1 demo level (1 level, 1 enemy) | not started | — |
| Wolf3D Phase 2 hybrid GPU | not started | — |
| Hot reload watch mode | pending (blocks RPG demo start) | — |
| RPG Dungeon demo (1 room, dialogue, inventory) | not started | — |
| Metaprogramming (`$T` + compile-time reflection) | pending (lands during RTS) | — |
| RTS demo (1 map, 2 unit types) | not started | — |
| bangscript compiler extensions + runtime | not started | — |
| Adventure Puzzle demo → B++ 1.0 | not started | — |
