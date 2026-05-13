# Warcraft 1 in B++ — port plan

A clean-room port of *Warcraft: Orcs & Humans* (Blizzard, 1994) from
the C/C++ Stratagus engine + Lua game scripts (war1gus) into pure
B++. The deliverable is a playable WC1 with **zero non-B++ runtime
dependencies** — no Lua interpreter, no SDL, no Stratagus, no FFI to
C libraries. The b++ stb cartridge family is the entire stack.

This document is the canonical anchor for the multi-session arc.
Each session below is sized to fit one focused work block; every
session ships visible value and leaves the tree green.

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
| Original DOS executables | `/Users/Codes/Warcraft1/Warcraft_Orcs_and_Humans_DOS_Files_EN/Game Files/` | reference behaviour (run in DOSBox to verify a port-day question) |
| `war1gus` engine + scripts | `/Users/Codes/Warcraft1/war1gus/` (cloned upstream) | reference implementation — Stratagus C++ engine plus ~9.5K LOC of Lua scripts encoding every game rule |
| Converted assets | `games/rts_1.0/assets/converted/` (in this repo) | the actual PNG / WAV / MID / SMS / SMP / TXT files our port consumes |

The Lua scripts in `war1gus/scripts/` are the "specification" — they
read like data sheets for every unit, building, missile, animation,
spell, and AI subroutine. We port the data into B++ structures and
the behaviour into B++ functions; we do not embed Lua.

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

### Session 1 — Tile renderer + map walk (~3-4h)

**Goal**: open a window, load the forest tileset PNG, paint a hand-
authored 32×32 tile grid on screen. No units, no input beyond
window-close. Success = "the screen looks like a Warcraft 1 map."

**Files**:
- `wc1_render.bsm` — palette + atlas wiring around the forest
  tileset PNG.
- `wc1_map.bsm` — minimal grid struct + a hard-coded test map.
- `rts.bpp` — game_init wires a render loop, game_render paints
  the grid.

**Verification**:
- Bootstrap byte-stable, suite green.
- Manual visual check: `./bpp games/rts_1.0/rts.bpp -o /tmp/rts && /tmp/rts`
  shows a Warcraft-style forest landscape on screen.
- Smoke commit: tile pipeline proven independent of game logic.

### Session 2 — SMS / SMP map loader (~3-4h)

**Goal**: parse one of the converted skirmish maps from
`assets/converted/maps/forest1.{sms,smp}` and render IT instead of
the hard-coded grid. Real Warcraft map terrain on screen.

**Files**:
- `wc1_map.bsm` — gain `wc1_map_load(path) -> WC1Map` + struct
  with width, height, tile-id grid, spawn-point list.
- SMS is text Lua-style (the war1tool output); we port the
  small subset needed for terrain (the SetMapTile-like calls)
  into a tiny B++ parser. SMP is the binary tile grid.

**Verification**: real WC1 forest1 / swamp1 / dungeon1 maps render
faithfully against the original game (DOSBox side-by-side check).

### Session 3 — Unit sprite rendering + ECS spawn (~3-4h)

**Goal**: spawn one peasant unit at a fixed map position via the
stbecs archetype API, render its sprite at the right pixel offset.
No animation yet beyond the idle frame; no movement.

**Files**:
- `wc1_units.bsm` — first 4-8 unit type entries from units.lua
  ported to a B++ struct table (HitPoints, Armor, Speed, Image,
  Size).
- Component registration in `wc1_render.bsm`: register Pos, Sprite,
  UnitType components; build the Combatant archetype.
- Render path: archetype walk → sprite blit at world-to-screen
  position.

**Verification**: peasant visible on the map at the spawn point.

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
BasicDamage / PiercingDamage per balancing.lua, missile units fire
arrows, dead units spawn corpses.

**Files**:
- `wc1_combat.bsm` — damage formula (port of balancing.lua section
  5), missile entity lifecycle.
- `wc1_missiles.bsm` — missile type table.

**Verification**: footman vs grunt deathmatch resolves correctly;
archer vs grunt has missile flight on screen.

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

**Goal**: barracks trains footmen with gold cost + training time
from balancing.lua. Train queue UI in command card.

### Session 11 — AI baseline (~4-5h)

**Goal**: enemy player builds, gathers, attacks. Port of ai.lua's
basic strategy script.

### Session 12 — Audio integration (~2-3h)

**Goal**: BGM via stbmidi (one of the 45 converted MIDIs cycles per
mission). UI + combat SFX via stbsound. Voice clips on selection
and command acknowledgement.

**Files**: `wc1_audio.bsm` — track selection per mission state;
voice-acknowledge bus per faction.

### Session 13 — Mission 1 playable (~4-5h capstone)

**Goal**: human campaign mission 1 playable end-to-end. Briefing
text shown, briefing voice plays, gameplay loads, victory or defeat
condition triggers cleanly. The "B++ cantando" proof.

**Files**: `wc1_mission.bsm` — mission orchestrator; load
`assets/converted/campaigns/human/01.sms` + `01.smp`.

## Cross-cutting design decisions

### Coordinates

WC1 grid is 32×32 pixels per tile. Standard convention:

- `gx`, `gy` — tile coordinates (integer, range determined by map)
- `px`, `py` — pixel coordinates in world space (gx * 32 + sub)
- `sx`, `sy` — pixel coordinates in screen space (after camera)

Camera transform lives in `wc1_camera.bsm`; render code calls
`wc1_world_to_screen(px, py)` to convert.

### Numeric model

Speed / position in milli-units (×1000) the same way `stbecs`
already encodes pos_x / vel_x. Combat damage in plain integer (HP
is 1..400 in the unit table). No floats in the simulation hot path
— audio + render do their own float math internally.

### Save state

Out of scope for v1. Each session ships gameplay; save/load is a
post-1.0 sidequest tracked in `docs/todo.md`.

### Unit caps

Original WC1 capped at 75 units per side. We honour that initially
but the engine has no hard limit — the `_ECS_SYS_MAX = 32` in the
scheduler is system-count, not entity-count.

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
  close if any).
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
  original DOS executables; run in DOSBox for behavioural ground
  truth on disputed mechanics.

## Status

**Plan saved 2026-05-12.** Implementation pending — Session 1
slot in when ready to attack. Day was dense (12 commits) — fresh
context recommended before opening the first session.
