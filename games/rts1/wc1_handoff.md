# Warcraft 1 in B++ — mod plan

A B++ RTS that runs on the *Warcraft: Orcs & Humans* (Blizzard,
1994) asset bundle. **Not a 1:1 port** — the constraint is the
assets, the gameplay is ours. We use the converted PNG / WAV / MID
/ map files in `games/rts_1.0/assets/converted/` as the "skin and
soundtrack"; balance, AI, and feel are creative choices we make as
the game comes together. Zero non-B++ runtime dependencies — no Lua
interpreter, no SDL, no Stratagus, no FFI to C libraries. The b++
stb cartridge family is the entire stack.

This document is the canonical anchor for the multi-session arc.
Each session below is sized to fit one focused work block; every
session ships visible value and leaves the tree green.

**Scope clarifier (2026-05-12):** the original WC1 numbers are a
starting point, not a spec. Where war1gus / DOSBox mechanics make
the game more fun, we copy them. Where they don't (or where we
just have a better idea), we deviate. "Plays like a Warcraft 1
mod" is the bar, not "passes a frame-by-frame DOSBox diff."

**Meta goal — Bang 9 edits the mod assets (2026-05-12):** WC1
sprites and levels are authored in Bang 9's existing tabs
(sprite + level_editor), not in a parallel WC1-only editor.
Concretely:

- **Maps** ship in the same JSON shape `level_editor` reads/writes
  (`{width, height, tiles[][]}`, identical to
  `games/pathfind/assets/levels/level1.json`). Session 2's
  converter outputs THAT format directly — no custom `.b1map`.
- **Sprites** ship as Modulab atlas-pack files (`*.atlas.json`,
  same shape pathfind uses). Session 3 loads via `image_load` +
  `image_hot_reload_enable`; Bang 9's sprite tab authors them.
- **Hot-reload via `file_watch_register` from day 1** —
  pathfind ↔ level_editor proves the rail; WC1 rides the same
  rail for both maps and sprites.

This rules out: WC1-only level editor, custom binary map format,
WC1-only sprite editor, direct-PNG sprite loading bypassing the
atlas-pack convention. Tooling investment compounds across
projects.

## Why this is the right shape now

The RTS Stress Arc closed earlier the same week (2026-05-12) with
five infrastructure sessions covering ECS query, archetype storage,
SIMD batching, flow-field pathfinding, and the system scheduler.
Plus stbmidi just shipped (the missing audio piece). Plus the
asset bundle is already in `games/rts_1.0/assets/converted/`
(229 PNG + 111 WAV + 45 MID + maps + scripts, 17 MB total).

The B++ stack now carries every primitive Warcraft 1 needs:

| Need | Cartridge | Status |
|---|---|---|
| Window + input + frame loop | `stbgame` / `stbwindow` | shipped |
| Tile + sprite rendering | `stbrender` / `stbsprite` / `stbtile` / `stbpal` | shipped |
| Indexed-palette graphics | `stbpal` + `stbimage` | shipped |
| ECS entity model | `stbecs` (archetype path) | shipped (Stress Arc S2) |
| System scheduler | `stbecs` SYS_SERIAL/PARALLEL | shipped (Stress Arc S5) |
| Per-unit pathfinding | `stbpath` (A*) | shipped |
| Crowd pathfinding (rally points) | `stbflow` (BFS field) | shipped (Stress Arc S4) |
| SIMD batching for hot loops | `vec_*4` + `_vec_axpy_f32` pattern | shipped (Stress Arc S3) |
| Audio mixing | `stbmixer` over `stbaudio` SPSC ring | shipped |
| MIDI music playback | `stbmidi` | shipped (today) |
| WAV SFX + speech | `stbsound` | shipped |
| Asset hot-reload | `file_watch_register` | shipped |

Nothing on this list needs new infrastructure. The arc is
**composition**, not invention.

## Source material

| Source | Where | Role |
|---|---|---|
| Converted assets | `games/rts_1.0/assets/converted/` (in this repo) | **the actual constraint** — PNG / WAV / MID / SMS / SMP / TXT files our game consumes at runtime |
| Original DOS executables | `/Users/Codes/Warcraft1/Warcraft_Orcs_and_Humans_DOS_Files_EN/Game Files/` | run in DOSBox if we want to *feel* a specific mechanic before deciding what to do with it; not a binding spec |
| `war1gus` repo | `/Users/Codes/Warcraft1/war1gus/` (Wargus's WC1 port to Stratagus engine) | readable reference for asset format + how the data was originally shaped; **not the original Blizzard source** (Blizzard never released it, war1gus is a clean-room reimplementation) |
| `war1gus/scripts/*.lua` | inside the war1gus repo | starting numbers for units / buildings / missiles / animations / spells / AI — copy what's good, change what's not |
| `war1gus/scripts/balancing.lua` | inside the war1gus repo | war1gus-specific multiplayer rebalances (header says so). Useful as one data point, not gospel. We pick our own numbers anyway. |
| `war1gus/war1gus.cpp` + `war1tool.cpp` | inside the war1gus repo | engine glue (Stratagus C++) and asset converter. `war1tool` already populated `assets/converted/`; engine glue is reading material when we get curious. |

**Where to look for what:**

- "What does a peasant cost / how many HP / what's its sprite?"
  → `war1gus/scripts/units.lua` for a starting point; tweak as
  the game shapes up.
- "How much damage does a footman do to a grunt?"
  → pick numbers that feel right; `balancing.lua` is one suggestion.
  DOSBox is a sanity check, not a specification.
- "What animation frames per direction?"
  → `war1gus/scripts/anim.lua` — the visual data is faithful and
  we'll generally just use it (the assets are sliced for it).
- "AI behaviour grammar?"
  → `war1gus/scripts/ai.lua` is a starting point. We're free to
  write something simpler, smarter, weirder — whatever makes the
  matches fun.

The Lua scripts read like data sheets. We port the data into B++
structures and the behaviour into B++ functions; we do **not** embed
Lua. License notice (war1gus is GPL-2): we treat it as readable
reference, not as code we copy. Clean-room reimplementation in B++.

**The asset bundle is the contract; the rest is design space.**

## Module layout (proposed)

```
games/rts_1.0/
├── rts.bpp                  -- main entry; calls game_init / game_run
├── wc1_units.bsm            -- unit type table (port of units.lua, 307 entries)
├── wc1_buildings.bsm        -- building type table (port of buildings.lua, 540 LOC)
├── wc1_missiles.bsm         -- missile types (port of missiles.lua)
├── wc1_anims.bsm            -- animation tables (port of anim.lua, 512 LOC)
├── wc1_spells.bsm           -- spell definitions (port of spells.lua, 408 LOC)
├── wc1_balancing.bsm        -- damage / cost / training-time tables (balancing.lua, 2.4K LOC)
├── wc1_map.bsm              -- SMS / SMP loader; tile grid lookup
├── wc1_render.bsm           -- tile + sprite render pipeline
├── wc1_camera.bsm           -- viewport scroll + tile↔screen transform
├── wc1_input.bsm            -- selection box, click-to-target, hotkeys
├── wc1_movement.bsm         -- per-unit pathing via stbpath / stbflow
├── wc1_combat.bsm           -- attack / damage / death; missile flight
├── wc1_resources.bsm        -- gold / wood gather + deposit
├── wc1_production.bsm       -- training queue in barracks; build queue
├── wc1_ai.bsm               -- enemy AI (port of ai.lua, 537 LOC)
├── wc1_hud.bsm              -- resource bar + minimap + command card + portrait
├── wc1_audio.bsm            -- music selection per scene; SFX dispatch
├── wc1_mission.bsm          -- mission-load orchestrator; victory / defeat
└── assets/converted/        -- already imported (commit daffd2c)
```

Module discipline per Tonify Rule 0:

- Sibling `.bsm` files in `games/rts_1.0/` are loaded with
  `load "wc1_*.bsm"` — they share the game's namespace.
- The stb cartridges are pulled with `import "stb*.bsm"` —
  cross-project infrastructure.
- `rts.bpp` is the only `.bpp` (compiler entry); everything else
  is `.bsm`.

## Sessions

Each session ships a self-contained slice of the game, leaves the
suite green, and is verifiable on screen. Estimated effort listed
per session; sessions can be re-scoped mid-flight per Rule 28.

**Status (2026-05-13):**

- ✅ Session 1 — Tile renderer + map walk (CLOSED, commit `15ee36f`)
- ✅ Session 2 — Native map format + offline converter + tileset
  Level Editor + bang9 auto-load (CLOSED 2026-05-13). Hot-reload
  validated end-to-end: edit `forest1.json` in Bang 9's Levels
  tab → running `rts.bpp` reflects the new tile in ~30 ms.
- ⏭ Session 3 — Unit sprite rendering + ECS spawn (next).

### Session 0 — stbmidi audio fidelity smoke (~30 min, pre-flight)

**Goal**: hook one of the 45 converted MIDIs (`assets/converted/music/00.mid`)
into `midi_play_file` and confirm it actually sounds like Warcraft 1
music — not just that the parser walks events without crashing
(`tests/test_stbmidi_wc1.bpp` already proves the latter).

**Why before Session 1**: discovering an XMI-quirk in stbmidi
during Session 12 (audio integration) wastes a render-side context.
30 minutes of "play one .mid through stbmixer → speakers" closes the
audio risk now, leaving Sessions 1-11 free to focus on visuals +
gameplay.

**Files**: `examples/wc1_midi_smoke.bpp` (~30 LOC) — `init_audio` +
`init_midi` + `midi_play_file("games/rts_1.0/assets/converted/music/00.mid", 0)` +
sleep loop until ESC.

**Verification**: it sounds musical (recognisable melody, no drift,
no glitch, tempo changes land cleanly). Negative result → file
stbmidi sidequest before Session 1.

### Session 1 — Tile renderer + map walk (~3-4h)

**Goal**: open a window, load the forest tileset PNG (256×320,
16×16 tiles), paint a hand-authored 32×32 tile grid on screen
using `stbtile`. No units, no input beyond window-close. Success
= "the screen looks like a Warcraft 1 map."

**Files**:
- `wc1_map.bsm` — thin wrapper around `stbtile`: `wc1_map_new(w,h)`,
  `wc1_map_set_tile`, hard-coded test map factory.
- `rts.bpp` — `game_init` + `tile_load_set` + `tile_bind_set` +
  `game_frame_begin` / `tile_draw` loop.

**Verification**:
- Bootstrap byte-stable, suite green.
- Manual visual check: `./bpp games/rts_1.0/rts.bpp -o /tmp/rts && /tmp/rts`
  shows a Warcraft-style forest landscape on screen.
- Smoke commit: tile pipeline proven independent of game logic.

### Session 2 — Native map format + offline converter (~3-4h)

**Goal**: render real WC1 maps without the runtime ever touching
the war1tool-emitted SMS / SMP files. Pipeline: SMS → offline
B++ converter → native `.b1map` (or JSON via `bpp_json`) → game
loads native format. Zero Lua at runtime, zero string-parsing
of foreign syntax at runtime — the converter does all of that
once, ahead of time.

**Why offline conversion, not runtime parse**: the war1tool
output happens to be written in Lua-call syntax (`SetTile(94, 0,
0, 0)`), which is convenient for war1gus's Stratagus engine but
irrelevant to us. Embedding ANY Lua-syntax recognition in the
runtime drags the wrong shape of work into the hot path. The
conversion is one-shot; the game ships with `.b1map` files, not
`.sms` files. Same staging principle as war1tool itself
(`DATA.WAR` → PNG/WAV is offline; the game never opens
`DATA.WAR`).

**Files**:
- `tools/wc1_map_convert/wc1_map_convert.bpp` — offline tool.
  Reads an SMS file, scans for `SetTile(id, x, y, _)` /
  `SetStartView(_, x, y)` / `SetPlayerData(player, "Resources",
  "gold|wood", n)` / `PresentMap(_, players, w, h, _)` via a
  trivial token scanner (skip whitespace, match identifier,
  skip `(`, read integer args until `)`), and writes a JSON
  file in the **level_editor format** (`{"width": w, "height":
  h, "tiles": [[...]], "spawns": [...], "resources": {...}}`).
  Scanner is ~100 LOC of B++ — no parser generator, no Lua
  semantics, just "find function name, grab the integer args."
  Output JSON shape extends pathfind's level1.json with `spawns`
  + `resources` arrays; level_editor and WC1 game both read it
  cleanly.
- `games/rts_1.0/wc1_map.bsm` — gain `wc1_map_load(path: ptr)
  -> WC1Map` that reads the JSON via auto-injected `bpp_json`
  (same path pathfind takes). Spawn-point list, player resource
  defaults, and tile grid all populated from the JSON. Hot-
  reload via `file_watch_register` from day 1.

**Why level_editor format (not custom `.b1map`)**: the meta goal
above commits the mod to Bang 9 as the authoring surface. A custom
binary format would fork from level_editor and force a parallel
editor. JSON via bpp_json keeps the round-trip
`Bang 9 → file → game → file_watch → game` working from day 1.

**Verification**: `forest1.json` loads + renders identical to the
SMS-driven war1gus reference. Converter is idempotent (re-running
on the same SMS produces byte-stable output). Bang 9's Levels
tab opens the converted JSON, edits a cell, saves, the running
game reflects the change in ~30ms (pathfind hot-reload pattern).
`tools/wc1_map_convert/` lands as a sibling tool to `war1tool` —
runs once per SMS file at asset-prep time.

**Verification**: WC1 forest1 / swamp1 / dungeon1 maps render
recognisably (the layouts are unmistakable). Pixel-perfect match
against DOSBox is not required.

### Session 3 — Unit sprite rendering + ECS spawn (~3-4h)

**Goal**: spawn one peasant unit at a fixed map position via the
stbecs archetype API, render its sprite at the right pixel offset.
No animation yet beyond the idle frame; no movement.

**Files**:
- `wc1_units.bsm` — first 4-8 unit type entries from units.lua
  ported to a B++ struct table (HitPoints, Armor, Speed, Image,
  Size).
- `assets/wc1_units.atlas.json` — Modulab-style atlas pack
  authored from the converted PNG sprites (or generated via a
  one-shot `tools/sprite16_to_atlas.sh`-style pre-process).
  Loaded via `image_load("...atlas.json")` + hot-reloaded via
  `image_hot_reload_enable` per the meta goal above. Bang 9
  sprite tab can open and edit this file — same workflow as
  `pathfind.atlas.json`.
- Component registration in `wc1_render.bsm`: register Pos, Sprite,
  UnitType components; build the Combatant archetype.
- Render path: archetype walk → `image_draw_size(atlas,
  sprite_id, sx, sy, w, h)` (smart-bind batches into one
  drawPrimitives via Phase 3.2).

**Verification**: peasant visible on the map at the spawn point.
Edit the peasant sprite in Bang 9's sprite tab → ~30ms later the
running game shows the new pixels (atlas hot-reload).

### Session 4 — Camera + input + selection (~3-4h)

**Goal**: arrow keys (or edge-pan) scroll the viewport. Left-click
selects a unit (highlight ring); right-click is the move-command.
Selected unit stores a target tile, no movement yet.

**Files**:
- `wc1_camera.bsm` — viewport offset; tile↔screen transform.
- `wc1_input.bsm` — selection bookkeeping; click-to-target.
- HUD foundations in `wc1_hud.bsm` (selection ring overlay).

**Verification**: click peasant → ring appears; right-click ground
→ debug "target = (gx, gy)" prints.

### Session 5 — Movement (per-unit pathing) (~3-4h)

**Goal**: selected unit walks from its current tile to the right-
clicked target along an A* path. Animation cycles through walk
frames. Multiple units can be commanded independently.

**Files**:
- `wc1_movement.bsm` — A* via stbpath; per-unit cursor along the
  returned waypoint list; smooth pixel interpolation between
  tiles for the 60fps render.
- Animation hookup in `wc1_anims.bsm` — pull walk frames from
  the sprite atlas based on facing direction (8 directions, per
  the Lua animation table).

**Verification**: 3 peasants on the map, click each, send them to
different corners — all walk along their A* paths simultaneously.

### Session 6 — Flow-field crowd movement (~2-3h)

**Goal**: when N > 5 units are selected and given the same target,
switch from N × A* to one `stbflow` field. Validates the Stress
Arc S4 work in a real game context.

**Files**: extend `wc1_movement.bsm` with the per-tick flow-vs-A*
decision (matches the Stress Arc S4 bench's threshold).

**Verification**: 50-unit rush to a rally point completes without
the per-frame budget spike that 50 × A* would cause.

### Session 7 — Combat basics (~3-4h)

**Goal**: attack-move command. Units engage enemies in range, deal
damage, missile units fire arrows, dead units spawn corpses.

**Files**:
- `wc1_combat.bsm` — damage formula (start from balancing.lua's
  basic + piercing model, then tune for feel), missile entity
  lifecycle.
- `wc1_missiles.bsm` — missile type table.

**Verification**: footman vs grunt deathmatch resolves with the
right "weight" (a few hits, not one-shots, not slogs); archer vs
grunt has missile flight on screen. We iterate on numbers until
the engagements feel good — not until they match DOSBox.

### Session 8 — Buildings + construction (~3-4h)

**Goal**: place foundation, peasant walks to it, construction
animation runs, building completes. Town hall, farm, barracks
playable.

**Files**:
- `wc1_buildings.bsm` — building type table (port of buildings.lua).
- `wc1_production.bsm` — construction state machine.

### Session 9 — Resources (~3-4h)

**Goal**: gold mines + forests harvestable. Peasants gather, return
to town hall, deposit. Resource counter in HUD ticks up.

**Files**:
- `wc1_resources.bsm` — gather state machine; deposit logic.
- HUD updates in `wc1_hud.bsm`.

### Session 10 — Production (training + research) (~3-4h)

**Goal**: barracks trains footmen with gold cost + training time.
Initial values cribbed from balancing.lua; we adjust as the
economy tells us what's fun. Train queue UI in command card.

### Session 11 — AI baseline (~4-5h)

**Goal**: enemy player builds, gathers, attacks. Pick a strategy
that gives the player a fair fight on mission 1 — could be
inspired by ai.lua, could be simpler (timed waves) or weirder
(opportunistic harasser). Whatever lets us call this a finished
game first; sophistication is iterable later.

### Session 12 — Audio integration (~2-3h)

**Goal**: BGM via stbmidi (one of the 45 converted MIDIs cycles per
mission). UI + combat SFX via stbsound. Voice clips on selection
and command acknowledgement.

**Files**: `wc1_audio.bsm` — track selection per mission state;
voice-acknowledge bus per faction.

### Session 13 — Mission 1 playable (~4-5h capstone)

**Goal**: a human-side scenario playable end-to-end on the
mission-1 map (or our own remix of it). Briefing text shown,
briefing voice plays, gameplay loads, victory or defeat condition
triggers cleanly. The "B++ cantando" proof — and the first time
a player can sit down and *play* this thing.

**Files**: `wc1_mission.bsm` — mission orchestrator; load
`assets/converted/campaigns/human/01.sms` + `01.smp` (or a
hand-authored alternative if our mechanics call for it).

## Engineering principles

### Use what stb offers; evolve stb when it doesn't

Every session starts by asking: "does an existing stb cartridge
cover this?" — `stbecs`, `stbpath`, `stbflow`, `stbmixer`,
`stbmidi`, `stbsound`, `stbsprite`, `stbtile`, `stbpal`,
`stbrender`, `stbgame`, `stbwindow`. If yes, consume it as-is.

If a session genuinely needs something the cartridge does not
expose, the right move is **to evolve the cartridge**, not to
build a one-off helper inside `wc1_*.bsm`. The cartridge already
has the right surface; the new feature lives where the next
consumer can re-use it. Two checks before adding the feature:

- Killer-use-case test (Tonify Rule 28): does this catch a real
  bug class for at least one consumer beyond WC1, or is it
  WC1-specific?
- Smaller-tool test: can the existing cartridge surface be
  composed differently to avoid the addition?

WC1-specific game logic stays in `games/rts_1.0/wc1_*.bsm`. Cross-
project infrastructure that just happens to be discovered by WC1
graduates to `stb/`. The line moves with consumer count, not with
intuition.

**Pre-existing W032 sweep is parallel work, not a blocker.** The
~57 W032 hits in older stb cartridges (`stbimage`, `stbforge`,
`stbasset`, `stbsound` path-receiving sites) are tracked in
`docs/todo.md` and can be cleaned up in any spare half-hour
between sessions — they don't gate the wc1 arc. New code in
`wc1_*.bsm` written under Rule 13 emits zero W032 from day one.

### Tonify checklist on every new function

Mandatory pre-flight per Tonify Rule 14 / Rule 25 / Rule 30 /
Rule 13 / Rule 27 (leaves-first ordering) / Rule 17 (`put`
smart dispatch) / Rule 28 (killer-use-case gate). The checklist
is the pocket manual. Open `docs/tonify_checklist.md` alongside
the editor; do not write a function without referencing it.

Specific rules that bite hardest in game code:

- Rule 13 (`: ptr` / `: float` / `: word` annotations on public API
  params) — every `wc1_*` function exposed to other modules.
- Rule 27 (leaves-first definition ordering) — V3 inference
  needs callees defined before callers in the same file.
- Rule 28 (killer-use-case gate) — guards every "we should
  probably also add X" temptation mid-session.
- Rule 30 (ECS layout) — defaults to SoA flat; this game's
  heterogeneous entity classes (Tile / Combatant / Building /
  Missile) fire the AoSoA trigger from Session 3 onwards (component
  set differs per class + sparse "iterate all Combatants" queries
  filter most of the world). Switch via `ecs_archetype` /
  `ecs_spawn_at` from Session 3.
- Rule 17 (`put` / `put_err` smart dispatch) — never bypass to
  raw `putnum_err` unless explicit integer intent; W032 catches
  the lapse.

## Cross-cutting design decisions

### Coordinates

WC1 grid is 16×16 pixels per tile (verified via `file
graphics/tilesets/forest/terrain.png` → 256×320, with 16 cols × 20
rows of 16-pixel tiles = 320 tiles total). Standard convention:

- `gx`, `gy` — tile coordinates (integer, range determined by map;
  forest1 is 64×64 per `forest1.smp`)
- `px`, `py` — pixel coordinates in world space (gx * 16 + sub)
- `sx`, `sy` — pixel coordinates in screen space (after camera)

Camera transform lives in `wc1_camera.bsm`; render code calls
`wc1_world_to_screen(px, py)` to convert.

### Numeric model

Speed / position in milli-units (×1000) the same way `stbecs`
already encodes pos_x / vel_x. Combat damage in plain integer (HP
in roughly the 1..400 range like the original — convenient size
for tuning, fits in a halfword). No floats in the simulation hot
path — audio + render do their own float math internally.

### Save state

Out of scope for v1. Each session ships gameplay; save/load is a
post-1.0 sidequest tracked in `docs/todo.md`.

### Unit caps

Original WC1 capped at 75 units per side — fine starting point,
not a constraint. The engine has no hard limit (the
`_ECS_SYS_MAX = 32` in the scheduler is system-count, not
entity-count); raise the cap if a scenario calls for it.

### Pathing strategy

- N == 1 selected unit moving: A* via stbpath.
- N >= 6 selected units to same target: one `flow_compute` from
  the goal + per-unit `flow_step` sample. Threshold validated by
  the Stress Arc S4 bench.

### AI cadence

Enemy AI ticks at 5 Hz (every 12th 60-fps frame), not per-frame.
Plenty for the "build orcs, scout, attack at minute 8" rhythm of
WC1 missions.

## What is intentionally NOT in v1

Per Rule 28 — defer until concrete demand:

- Multiplayer / lockstep determinism. Single-player only.
- Save / load games.
- Custom map editor in-engine. Edit maps in the original WC1
  editor (or its successor) and convert via war1tool.
- Modding / scripting hooks. Game logic is compiled B++.
- Network sync. Single machine, single player vs AI.
- Replay system.
- High-resolution rendering (modern aspect ratios, scaling).
  Native 320×200 → upscale via existing `stbrender` blit.
- Internationalisation. English text only (the original DOS
  ReadMe.txt corpus we have).

## Verification at every session

Standing gates per Tonify discipline:

- Bootstrap byte-stable (`./bpp src/bpp.bpp -o /tmp/g1 &&
  /tmp/g1 src/bpp.bpp -o /tmp/g2 && diff <(md5 -q /tmp/g1)
  <(md5 -q /tmp/g2)`).
- Native suite + C-emit suite green (current floor: 163/0/12 +
  130/0/45).
- Manual visual smoke check on the new feature.
- No new W032 / W026 / W031 hits introduced (sweep at session
  close if any). **Pre-existing W032 noise from older stb
  cartridges (`stbimage`, `stbforge`, `stbasset`, `stbsound`
  call sites that pre-date the `: ptr` annotation, ~57 sites)
  is tracked separately in `docs/todo.md` and is OK to ignore
  during the wc1 arc.** New `wc1_*.bsm` code must emit ZERO
  W032 by annotating every path-receiving param `: ptr`.
- Commit before the next session starts.

## Cross-references

- `docs/rts_stress_arc.md` (now `legacy_docs/rts_stress_arc.md`) —
  the infrastructure arc that unblocked this port.
- `docs/stbmidi_plan.md` — companion design doc for the cartridge
  consumed in Session 12.
- `games/rts_1.0/assets/README.md` — asset provenance, conversion
  reproduction steps, license notice.
- `/Users/Codes/Warcraft1/war1gus/scripts/` — Lua specification we
  port from (read; do not link).
- `/Users/Codes/Warcraft1/Warcraft_Orcs_and_Humans_DOS_Files_EN/` —
  original DOS executables; fire up in DOSBox when you want to
  feel a specific mechanic before deciding what to do with it.

## Status

**Plan saved 2026-05-12; mod-framing clarification added same
day.** Implementation pending — Session 1 slot in when ready to
attack. Day was dense (12 commits) — fresh context recommended
before opening the first session.
