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

**Design vision — WC1 assets + SC1 mechanics crossover
(2026-05-18):** the game ships unique by leaning on **WC1's
visual identity** (peasant/peon sheets, forest tilesets, midi
soundtrack — see `assets/converted/`) while adopting **the
mechanical depth of Starcraft 1**:

- **Cooldown-based attacks** (frames between shots per unit
  kind), not WC1's tick-fixed melee.
- **Damage type × size class matrix** (Normal / Concussive /
  Explosive vs Small / Medium / Large) — gives unit
  composition meaning. Numbers cribbed from SC1's canon
  (Liquipedia / BWAPI) as a starting point; tuned for feel.
- **Layered resources** via `stbcharsheet` — HP for everyone,
  Shields for "tech-blessed" units (Knight upgrade?), Energy
  for casters (Conjurer / Warlock). Shields regen out of
  combat in SC1 — we follow that.
- **Supply cap** via farms (Human) and pig farms (Orc) — same
  mental model as SC1's depot / pylon / overlord.
- **Asymmetric factions** — the two sides don't share a unit
  template. Each gets its own quirks, mirroring SC1's three-
  race asymmetry.

The point is not "SC1 in a WC1 skin" — it is "what would
Warcraft have looked like if Blizzard had skipped the WC2-era
incremental polish and gone straight to SC1's mechanical
richness". WC1 + SC1 mechanics is our combinatorial novelty:
neither game shipped like that, the assets are already in the
tree, and the engine (`stbflow` crowd movement, `stbgrid`
occupancy, `stbcharsheet` resource bookkeeping) already
carries every primitive that mechanical jump needs.

**Meta goal — Bang 9 edits the mod assets (2026-05-12):** WC1
sprites and levels are authored in Bang 9's existing tabs
(sprite + level_editor), not in a parallel WC1-only editor.
Concretely:

- **Maps** ship in the same JSON shape `level_editor` reads/writes
  (`{width, height, tiles[][]}`, identical to
  `games/pathfind/assets/levels/level1.json`). Session 2's
  converter outputs THAT format directly — no custom `.b1map`.
- **Sprites** ship as Modulab atlas-pack files (`*.json`,
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

**Status (2026-05-18):**

- ✅ Sessions 1-7 CLOSED. See per-session blocks below for
  per-commit detail. S7 sub-arc 7.1 → 7.7 closed end-to-end:
  - 7.1 Faction + spawning + orc peon atlas
  - 7.2 Sheet ECS component + faction-aware right-click
  - 7.3 Combat tick + damage + chop/die animations + death
    (absorbed 7.5 — heap-free + tile-release + ecs_kill_at)
  - 7.4 Damage type × size matrix (SC1 canon — Normal /
    Concussive / Explosive vs Small / Medium / Large)
  - 7.5 Death (shipped inside 7.3, see commit message)
  - 7.6 Missile entities — new Tier-2 `stb/stbprojectile.bsm`
    (pool-based, built on `stbpool`) + RTS-side
    `wc1_missiles.bsm` (target-locked spawn, arrow atlas,
    damage matrix at impact). Archer + spearman wired.
  - 7.7 Energy + caster + shields regen — `RES_SHIELDS` +
    `RES_ENERGY` + `STAT_ENERGY_COST` slots, casters gated by
    energy pool, regen at SC1 cadence (7 HP/s shields,
    1 energy/s).
  - Bonus: full 14-unit roster wired (peasant/footman/knight/
    archer/cleric/conjurer/catapult/lothar/medivh on human side;
    peon/grunt/raider/spearman/necrolyte/warlock/catapult on
    orc side). Debug spawn keys 1-9 + 0/-/=/[/]/;/'. `wc1_
    sprite_convert` generalized to per-unit schema +
    configurable tile size (32 / 48 px).
- ⏭ Session 8 — Buildings + construction (next).

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
- `assets/wc1_units.json` — Modulab-style atlas pack
  authored from the converted PNG sprites (or generated via a
  one-shot `tools/sprite16_to_atlas.sh`-style pre-process).
  Loaded via `image_load("....json")` + hot-reloaded via
  `image_hot_reload_enable` per the meta goal above. Bang 9
  sprite tab can open and edit this file — same workflow as
  `pathfind.json`.
- Component registration in `wc1_render.bsm`: register Pos, Sprite,
  UnitType components; build the Combatant archetype.
- Render path: archetype walk → `image_draw_size(atlas,
  sprite_id, sx, sy, w, h)` (smart-bind batches into one
  drawPrimitives via Phase 3.2).

**Verification**: peasant visible on the map at the spawn point.
Edit the peasant sprite in Bang 9's sprite tab → ~30ms later the
running game shows the new pixels (atlas hot-reload).

### Session 4 — Camera + input + selection (~3-4h) — CLOSED

**Goal**: arrow keys (or edge-pan) scroll the viewport. Left-click
selects a unit (highlight ring); right-click is the move-command.
Selected unit stores a target tile, no movement yet.

**Files** (as shipped):
- Camera lives in `stb/stbcamera.bsm` instead of a per-game
  `wc1_camera.bsm` — factored after platformer was found to
  copy-paste the same clamp logic. Same functional goal met.
- `wc1_input.bsm` — selection bookkeeping; click-to-target.
- `wc1_hud.bsm` — selection ring overlay (HUD foundations
  for the minimap / info panel that S6+ extend).

**Verification PASSED**: click peasant → bright-yellow ring
appears around the 32×32 sprite; right-click ground → debug
`target = (gx, gy)` prints to stdout; click empty ground →
deselect; ESC stays bound to quit.

**Shipped in two passes**:
1. `c736261` — camera + start_view consumer (this commit got
   the macro framing wrong: I scoped to ~60% of the roadmap
   without flagging the deviation. The other half landed in
   the second pass after the gap was caught).
2. (this commit) — wc1_input + wc1_hud + Target component.

### Session 5 — Movement (per-unit pathing) (~3-4h) — CLOSED

**Goal**: selected unit walks from its current tile to the right-
clicked target along an A* path. Animation cycles through walk
frames. Multiple units can be commanded independently.

**Files** (as shipped):
- `wc1_movement.bsm` — A* via stbpath; per-unit Path component
  (waypoint buffer + cursor); pixel interpolation toward next
  waypoint at UnitDef.speed_pxs.
- `wc1_anims.bsm` — facing math (8-octant bucket from movement
  vector); state enum (idle / walk); cached anim ids from the
  loaded sprite sheet; flip-flag dispatch for W/NW/SW facings
  via image_draw_size_flipped.
- Sprite sheet pipeline migrated to Aseprite-compatible format
  by the WC1 modulab pipeline sidequest. The 10 named anims
  (5 idle + 5 walk) live in the sheet's frameTags; game-side
  reads them via image_anim_id at startup, image_anim_frame
  per render.

**Verification**: PASSED.
  - Movement: user reported ~28 right-clicks resolved correctly,
    peasant walks to each target, redirect mid-path works,
    arrival snaps to tile center.
  - Animation cycling: visual smoke (user-side) — peasant cycles
    through walk frames during movement, returns to idle frame
    on arrival, faces the direction of motion (8 facings via
    5 atlas variants + horizontal flip).
  - Multi-unit independence: forest1 spawns 2 peasants (player
    0 at 16,16 + player 2 at 16,48); each holds its own Path /
    Anim components and walks independently when commanded.

Shipped in 5 commits via `docs/sidequest_wc1_modulab_pipeline.md`:
  - Commit 1: WC1 S5 PARTIAL — movement (no animation)
  - Commit 2: convention cleanup `.atlas.json` → `.json`
  - Commit 3: wc1_sprite_convert + Aseprite-schema sidecar
  - Commit 4: stbimage parses Aseprite + game-side animation
    wires S5 (this commit)
  - Commits 5+6 (Modulab read/edit Aseprite sheets) ship
    separately to close the "Modulab as IDE-companion" promise
    end-to-end.

### Post-S5 tooling sidequests (2026-05-16 to 2026-05-17)

Game-side WC1 work paused after S5 to harden the Aseprite +
Modulab editing pipeline. Shipped before opening S6:

- `docs/sidequest_wc1_modulab_pipeline.md` Commits 5-6 —
  Modulab opens Aseprite sheets (`e5ece55` viewer +
  `9742457` slice editor).
- `docs/sidequest_aseprite_viewer_fixup.md` (`2afb7fa`) —
  double-click init bug + dirty discard prompt + per-frame
  dirty tracking + `tests/test_gpu_atlas_aseprite.bpp` smoke.
- Modulab UX polish — `cf68316` (Tab toggle + filename guard),
  `9005e7c` (untagged-frame static preview + sheet vertical
  pan + Cmd+Z/Cmd+Shift+Z), `69bdcaa` (Tonify polish).
- `docs/sidequest_aseprite_edit_main_canvas.md` (`834402d`) —
  killed the in-viewer mini-editor; double-click hands the
  frame off to Modulab's main canvas with full tool surface
  (brush/fill/line/rect/oval/palette/layers/undo). Cmd+S
  writes back to the sheet PNG.
- `9f59ca2` — wc1_sprite_convert column-major fix. War1tool
  emits walk cycles column-major (col=direction, row=frame);
  the original converter assumed row-major so `walk_S`
  played `walk_N` frames ("peasant looked like dying"). Fix
  reorders frame emission so tags stay contiguous; game-side
  required no change (only tag names referenced).
- `1f749c8` — auto-refit Modulab canvas zoom + origin when
  `g_canvas_n` changes (the 32×32 hand-off was overflowing
  the 800px panel at the default 24× zoom).
- (uncommitted at handoff time) — grid + zoom +/- controls
  in the Modulab status line.

**Open at handoff:**
- WC1 chop/die/carry frames (rows 6-12 of peasant.png) are
  emitted by the converter as UNTAGGED frames. Mapping to
  tag names needs a war1tool Lua-table cross-reference.
  Filed for whenever a consumer (S7 combat / S9 resources)
  asks. Modulab's untagged-frame click already shows static
  preview so users can inspect them visually today.
- **stbui v2 — Clay-inspired layout** (decision pending):
  Modulab zoom buttons trigger full UI reflow because side
  panels are anchored canvas-relative. Symptom of a deeper
  issue — stbui's "explicit pixel positioning" model doesn't
  scale to the ~500+ widget call sites across Bang 9 /
  Modulab / level_editor / fxlab / Aseprite viewer. User
  flagged Clay (Nic Barker, C) as inspiration for next stbui
  iteration. Sidequest doc + plan being written separately;
  do NOT open S6 before the stbui direction is decided —
  S6+ work will benefit from the new layout primitives.

### Session 6 — Flow-field crowd movement (~2-3h) — CLOSED

**Goal as planned**: when N > 5 units are selected and given the
same target, switch from N × A* to one `stbflow` field. Validates
the Stress Arc S4 work in a real game context.

**Goal as shipped (scope grew during execution):** marquee
multi-select + flow-vs-A* dispatch + **tile-claim collision** +
HUD ring centered on sprite slot. Collision was added mid-session
when the smoke test surfaced peasants stacking at convergent goals
(visually one sprite, terminal showed N=2). Ring centering was the
follow-on polish — peasants drifting "off-center" relative to the
ring once stacking was fixed.

**Files** (as shipped):
- `games/rts1/wc1_input.bsm` — marquee multi-select (drag rect
  AABB-tests every peasant; `selected_eids[]` + `selected_count`
  replace single `selected_eid`).
- `games/rts1/wc1_movement.bsm` — `wc1_movement_request_group`
  dispatches per group size: ≥ FLOW_THRESHOLD (6) → one shared
  `flow_compute` + per-unit `flow_step`; below threshold → N × A*.
  Adds `wc1_collision_init/_register/_release/_is_blocked_for`
  helpers backed by a shared `stbgrid` word-grid; `_step_unit`
  transfers the tile claim atomically when the micro-target
  changes; blocked = idle 1 frame.
- `games/rts1/wc1_render.bsm` — `Path` gains `mode` (A* vs
  FLOW) + `tile_gx/tile_gy` (current claim). `ecs_spawn_peasant`
  registers the spawn tile up front.
- `games/rts1/wc1_hud.bsm` — N rings centered on sprite slot
  via `stbcol.rect_center_x/y`; live marquee overlay while
  dragging.
- `games/rts1/rts.bpp` — `wc1_movement_init` reordered to run
  before unit spawn (so `_occupied` is allocated when spawn calls
  `wc1_collision_register`); `P`-key debug spawn drops a 5 × 2
  peasant cluster at the cursor so one tap crosses
  FLOW_THRESHOLD (debug affordance; comes out when a real
  production trigger lands).

**Cartridges graduated this session:**
- `stb/stbgrid.bsm` — generic 2D cell-storage primitive (byte +
  word variants). Was the right home for the occupancy grid —
  `stbcol` is leaf-math, `stbflow` is RTS-only. New fully-generic
  cartridge per Tonify Rule 33.
- `stbflow` refactored to consume `stbgrid` (validates the API
  as the second consumer beyond WC1).
- `stbcol.rect_center_x/y` — sprite-slot centering helper. Every
  future indicator that lands on top of a unit (HP bar, damage
  number, target arrow, build-progress overlay) reuses it.
- Tonify Rule 33 codified — fully-generic vs genre-generic
  cartridge tiers + the graduation pattern that produced this
  arc's work.

**Verification** (PASSED, manual smoke per user):
- One `P`-tap → 10 peasants → marquee → right-click distant
  goal → terminal prints `target = (gx, gy)  N=10`, peasants
  swarm via flow field, blob forms at goal WITHOUT stacking.
- Adjacent rings touch edge-to-edge without overlapping; each
  ring centered on its peasant body across walk frames + facing
  flips.
- Single-unit click + right-click still works (S5 behavior
  preserved).

**Commits**: `2b7c8d4` (stbgrid) → `94f6257` (stbflow refactor) →
`5bba3de` (WC1 S6 marquee + flow + collision) → `cfa1e77` (ring
centering). Plus `64e3aea` (stbui ui_image orphan const cleanup)
+ `0a6209d` (Tonify Rule 33 doc) + `ec2f4cd` (sidequest closeout)
shipped in the same arc.

### Session 7 — Combat basics (~3-4h) — CLOSED 2026-05-18

**Goal as planned**: attack-move command. Units engage enemies in
range, deal damage, missile units fire arrows, dead units spawn
corpses.

**Goal as shipped (scope grew during execution)**: full 7.1 → 7.7
sub-arc CLOSED in a single session — combat tick + damage matrix
+ death pipeline + missile entities + energy/shields + entire WC1
roster (14 unit kinds) wired and spawnable. Plus three new stb
graduations during the arc:

- **Tier-2 `stb/stbprojectile.bsm`** (NEW, ~215 LOC) — generic
  2D/3D pool of projectiles built on `stbpool`. Owns pos/vel/
  gravity_z/lifetime/payload_w bookkeeping; consumer-agnostic.
  Designed with three prospective consumers in view (rts1 now,
  fps Doom-mode phase 4+, rpg Game 4). Industry pattern: Doom
  `mobj_t` (1993), SC1 bullets, Stratagus missiles.
- **`wc1_sprite_convert` generalized** (Tier-3 tool) — was
  hardcoded peasant schema; now takes `[walk_n] [atk_n] [die_n]
  [tile_w] [tile_h]` positional args + ports
  `GetFrameNumbers(...)` verbatim from war1gus `anim.lua`.
  Handles every WC1 unit schema (5,4,3 / 5,5,3 / 5,5,5 / 5,2,3
  / 5,4,4 / 5,5,4 / 2,5,3) at 32 or 48 px tiles. Tag rename
  `chop_*` → `attack_*` (SC1 universal vocabulary).
- **`tile_bind_image` migration** — rts1 graduated from the
  legacy `tile_load_set` + `tile_bind_set` pair (one MTLTexture
  per tile, surfaced visual checkerboard artifacts under Phase
  3.2 smart-bind) to the modern atlas_grid path (single
  MTLTexture, batched draws). Added a `Loader pipeline`
  paragraph to `docs/manual/asset_formats.md` documenting the
  canonical loader for WC1-style tilesets.

**Files** (as shipped):
- `games/rts1/wc1_combat.bsm` — `_combat_step` with 4-branch
  state machine (dying / no-target / out-of-range / engaged) +
  matrix-aware melee + ranged dispatch + energy gate.
  `_disengage` helper resets state on target loss so units don't
  freeze in ATTACK after kill.
- `games/rts1/wc1_missiles.bsm` (NEW) — RTS-side glue around
  stbprojectile: target-locked launch, distance-based flight
  time, matrix damage at impact, arrow atlas + facing-based
  frame selection.
- `games/rts1/wc1_regen.bsm` (NEW) — per-frame shields + energy
  regen. SC1 cadence (7 HP/s shields, 1 energy/s).
- `games/rts1/wc1_units.bsm` — UnitDef populator compacted into
  `_def(...)` helper; 14 kinds populated with SC1-flavored
  stats. Added `STAT_ENERGY_COST`, `RES_SHIELDS`, `RES_ENERGY`
  slots + `start_energy / start_shields / energy_cost` UnitDef
  fields.
- `games/rts1/wc1_render.bsm` — `ecs_spawn_peasant` renamed
  `ecs_spawn_unit(kind)` to support multiple kinds. `_render_
  unit` reads `def.px_size` so 48 px sprites (knight, raider,
  lothar) render with their correct dims + offset math.
- `games/rts1/wc1_movement.bsm` — `_step_unit` early-returns
  for `UNIT_STATE_ATTACK` (parallel to existing DYING guard) so
  the combat tick's state assignment isn't clobbered.
- `games/rts1/rts.bpp` — 16 atlas loads compacted into
  `_load_wc1_atlas` helper; `_free_all_atlases` shutdown
  helper; debug spawn covers all 16 kinds via number keys +
  punctuation; full tile pipeline migration.
- `games/rts1/assets/sprites/wc1/*.{png,json}` — all 16 unit
  atlases + arrow missile atlas + atlas_grid sidecars.
- `games/rts1/assets/converted/graphics/tilesets/forest/
  terrain.json` (NEW) — atlas_grid sidecar so `image_load` can
  consume the tileset PNG.

**Verification**: full WC1 roster spawns + animates + attacks +
dies. Peasants chop, archers shoot arrows, conjurers gate on
energy + drain on cast + regen back. Catapults visible on screen
(human + orc share the same war1tool-extracted PNG, faithful to
WC1 original). Map renders as proper forest tilemap (no more
checkerboard artifacts). User-reported visual smoke PASSED at
end of session.

**Commits**: bundled at S7 closeout — `WC1 S7 CLOSED: full combat
arc + 14-unit roster + Tier-2 stbprojectile`.

### Session 8.0 — Pre-flight sidequests (CLOSED 2026-05-19)

Caught during the manual smoke of the full S7 roster; landed in
the S7 closeout commit as pending issues. Both shipped 2026-05-19
in the run-up to S8.

**8.0a — Catapult animation polish (CLOSED).** Smallest-tool fix
per Rule 28: bumped the attack-tag frame duration on
`assets/sprites/wc1/human_catapult.json` and `orc_catapult.json`
from 100 ms to 400 ms (only the tag-start frames — 10 / 15 / 20 /
25 / 30 — since `stbimage`'s loader reads the per-tag rate from
the first frame). Five frames × 400 ms = 2000 ms, which exactly
fills the catapult's 2000 ms cooldown, so the unit reads as
continuously swinging instead of twitching once per cooldown. No
code change; preserves byte-stable bootstrap. **Maintenance
note**: `wc1_sprite_convert` emits a global 100 ms duration for
every frame. Re-running the converter against the catapult atlases
would silently regress the tuning — re-apply the 400 ms bump after
any conversion pass or teach the converter a per-unit override
when the next siege unit lands.

Faction-tint variant (orc catapult palette swap) intentionally
deferred per Rule 28 — wait for `stbpal` + atlas-runtime-recolor
to earn its second consumer.

**8.0b — Window resize breaks mouse centering (CLOSED, 2026-05-19
→ 20).** Three iterations before the right fix landed; the final
shape is two surgical additions to the macOS platform layer +
a canvas-aspect bump in rts1.

**Iteration 1 (reverted):** added `game_set_window_nonresizable()`
opt-in in stbgame + macOS `_stb_set_window_resizable` clearing
the NSResizable bit. Rts1 locked its window. User pushed back —
losing drag-resize was a capability regression, not a feature.

**Iteration 2 (reverted):** rewrote the macOS mouse handler to
compute live integer scale + letterbox offsets every event,
mirroring `game_compute_present_rect`. Math was technically
correct, but a new bug surfaced: every time the user finished
a window drag-resize, a phantom marquee appeared and persisted
until the user clicked the map. Root cause was event-loss during
system gestures, not the math — Cocoa silently consumes the
`NSEventTypeLeftMouseUp` event that ends a window-resize drag,
so `_stb_mouse_btn` stayed at 1 even though the button was
physically released. The math rewrite exposed this more readily
because clicks worked correctly, so the latched button state
triggered drag-select on every subsequent mouse move.

**Iteration 3 (shipped, two-spot diff in `_stb_platform_macos.bsm`):**

1. Mouse handler reverted to the original `_plt_scale`-based math
   (no letterbox accounting — small visual offset on non-integer-
   scale resizes, but no math regression). The resize poll inside
   `_stb_poll_events` now recomputes `_plt_scale` live:
   `_plt_scale = max(1, min(cur_w / _stb_w, cur_h / _stb_h))`.
   Mouse coords stay aligned with the visible framebuffer under
   any user drag-resize.
2. End of `_stb_poll_events` syncs `_stb_mouse_btn` to
   `[NSEvent pressedMouseButtons]` — the OS-truth bitmask, masked
   to bits 0-2 (left / right / middle). Self-heals the
   LeftMouseUp events Cocoa swallows during system gestures
   (window resize, title-bar drag, dock interactions). No more
   phantom marquee after the user finishes a resize gesture.

Linux `_stb_platform_linux.bsm` mouse handler is unchanged this
pass — the X11 path has its own coordinate decode that already
reads from live state. When parity work lands it will mirror
the same `_plt_scale`-update + button-state-sync pattern.

**Bonus — canvas aspect change.** Bumped rts1's virtual canvas
from 320×240 (4:3) to **480×270** (16:9) in `rts.bpp:50-51`.
Modern monitors are 16:9; the old 4:3 canvas always letterboxed
on widescreen. New behaviour:

| Monitor | Scale | Letterbox |
|---|---|---|
| 1080p (1920×1080) | 4× | **none** |
| 4K (3840×2160) | 8× | **none** |
| 1440p (2560×1440) | 5× | thin (480 isn't a factor of 2560) |
| MacBook retina interior | 3× | thin |

Strategic-view side win: 30×17 tiles visible instead of 20×15 —
more battlefield, closer to a modern RTS feel. WC1 1994's
original 14×11-ish viewport stays a stylistic reference, not a
constraint.

The fully-letterbox-clean alternative would have been 320×180
(pathfind's canvas, fits every 16:9 monitor at exact integer
scale). Traded that for the strategic-view gain. Easy to flip
later if a 1440p user wants zero letterbox at the cost of fewer
visible tiles.

### Session 8 — Buildings + construction (CLOSED 2026-05-20)

**Goal hit**: place foundation, peasant walks to it (right-click
on the foundation while a peasant is selected), construction
animation plays while the peasant chops, building completes.
All 9 WC1 building kinds (town hall, farm, barracks, lumber
mill, blacksmith, stable / kennel, church / temple, tower,
keep / spire) wired for both factions; Shift+1..9 spawns human,
Ctrl+1..9 spawns orc.

**Shipped this session** (`games/rts1/`):
- `wc1_buildings.bsm` (new) — 9-kind BuildingDef table sourced
  from `war1gus/scripts/buildings.lua` numbers, Building ECS
  component, second archetype + mask-filtered query, hit-test
  helper, assignment helper that picks a free perimeter tile
  + commands the peasant via the existing movement rail.
- `wc1_production.bsm` (new) — construction state machine. The
  tick gates progress on a peasant being within 48 px of the
  building centroid; the worker enters `UNIT_STATE_BUILD` (a
  new state that reuses the chop-attack frames) on arrival and
  pops back to IDLE on completion.
- `wc1_render.bsm` — peasant_q query now filters by
  `1 << unit_comp`, isolating the unit pass from the building
  archetype.
- `wc1_input.bsm` — right-click on a friendly foundation
  assigns the first selected peasant via
  `wc1_building_assign_peasant`; left-click hit-tests buildings
  before units so a click on a building footprint always picks
  the building. New `selected_kind` global (UNITS / BUILDING /
  NONE) routes right-click handling per selection class.
- `wc1_hud.bsm` — HP bars (3-bracket green / yellow / red) over
  every damaged or selected unit + building; construction
  progress bars (amber) over CONSTRUCTING buildings; selection
  ring matches building footprint exactly; new bottom command
  card strip (60 px) with authentic WC1 portrait + icon_border
  + bottom_panel divider on top.
- `wc1_movement.bsm` — terrain-blocker seed at init walks the
  tilemap and marks tile 76 (canonical WC1 forest tree) as
  blocked in both path_find's `_pf` and the occupancy grid so
  peasants route around trees instead of through them.
- `wc1_anims.bsm` — `UNIT_STATE_BUILD` enum entry; the
  `wc1_anim_id_for` route reuses the attack-direction frames
  for build state.
- `wc1_combat.bsm` — combat tick skips `UNIT_STATE_BUILD` units
  so a peasant doesn't engage / dodge while parked at a
  foundation.
- `rts.bpp` — load wc1_buildings + wc1_production, 34 building
  atlas extrns + loads + frees (full forest set per faction),
  3 HUD atlas extrns (portrait_icons + icon_border +
  bottom_panel) + their loader, `_debug_spawn_building_tick`
  (Shift+1..9 human, Ctrl+1..9 orc).

**Engine wins (Tier 2):**
- `stb/stbecs.bsm` — `comp_bitmask` on ArchetypeRec computed at
  registration; `ecs_query_each` filters by archetype mask so
  multi-archetype games (units + buildings + future projectiles)
  don't leak iteration across types.
- `src/backend/os/macos/_stb_platform_macos.bsm` —
  - mouse handler now subtracts letterbox offset before
    dividing by integer scale so click coords stay correct
    under any window resize / aspect ratio,
  - `[NSEvent pressedMouseButtons]` syncs `_stb_mouse_btn` at
    end of poll (heals the LeftMouseUp events Cocoa swallows
    during window-resize gestures),
  - `[NSEvent modifierFlags]` syncs `_stb_keys[KEY_SHIFT/CTRL/
    ALT/META]` so modifier-key actions (Shift+N, Ctrl+N)
    actually fire.

**Tool wins:**
- `tools/wc1_sprite_convert/wc1_sprite_convert.bpp` — new
  `--mode building` codepath (input PNG of column-stacked
  square or rectangular frames, optional `frame_h` override).
  Used to produce 34 building sidecars + 7 HUD sidecars
  (portrait_icons, icon_border, bottom_panel, top_resource_bar,
  left_panel, right_panel, minimap) in one batch.

**Deferred to S8.5** (not blockers for S9):
- Bottom HUD polish — panels show but layout is rough; the
  WC1-authentic `left_panel` / `right_panel` / `top_resource_bar`
  / `minimap` PNGs are loaded but not yet composed into the
  bar. Pick this up alongside the resource counter in S9 (HUD
  work compounds).
- Wider terrain classifier — only tile 76 currently treated as
  a blocker; tiles 77 / 78 / 80-87 (other tree variants, cliff
  edges, water borders) stay walkable. Graduate a proper
  per-tile walkability table when S9 surfaces gold mines / forest
  patches the player needs to harvest (gather destinations have
  to be reachable, and gold mines + forests are the natural
  terrain-classifier consumers).
- Peasant-absorb on construction — the worker currently parks
  visible next to the foundation chopping; WC1 hid the peasant
  inside the building. Easier to add once portraits / selection
  feedback proves the worker is "occupied" by the construction
  job.

### HUD evolution — multi-session roadmap

**Where we are (post-S8.5):** the bottom command card draws a
single 60 px strip with the selected unit / building portrait
in a 56 × 56 box, framed by `icon_border`, on top of a
horizontally-tiled `bottom_panel` divider. HP bars (3-bracket
colour) float over damaged or selected entities; construction
progress bars (amber) sit above building footprints. That is
the entire visible HUD.

**What original WC1 actually shipped** (reference: any WC1
screenshot or `war1gus` UI overlay):
- Top **resource bar** spanning the full width — gold count,
  wood count, food count (used / cap), each with its own icon.
- Left **portrait + name pane** for the selected unit /
  building (large square, ~96 × 96) with the unit's bespoke
  portrait art (peasant, footman, knight, archer, conjurer,
  cleric, ogre, grunt, raider, axe-thrower, warlock, necrolyte,
  catapult, water elemental, daemon, scorpion, etc — every
  unit has its own face).
- Below the portrait: **stat strip** — name, current/max HP,
  attack / armour / range numbers, kills counter for combat
  units, "Building: <name>" + cost stack for foundations.
- **Command card** — 9-slot grid (3 × 3) of action buttons
  with WC1-style icons + hotkey letters underlined: move,
  stop, attack, patrol, return-cargo for units; train-
  peasant / train-footman / repair / cancel for buildings;
  build-menu sub-pages for peasants (farm, barracks, lumber
  mill, ...). Each button shows a tooltip on hover, gold-out
  greys itself when unaffordable.
- **Minimap** bottom-right — scaled-down render of the full
  map with team-coloured dots for units + buildings, viewport
  rectangle showing the camera frame, click-to-scroll.
- **Alert system** — flashing minimap dots + voice clip when
  a unit takes damage off-screen ("We're under attack!"); same
  channel for "Job's done", "Cannot build there", "Insufficient
  gold".

**Assets already converted** (S8 tool pass produced these,
loaded in rts.bpp, NOT yet composed):
- `assets/sprites/wc1/hud/top_resource_bar.{json,png}` — the
  full-width strip behind gold / wood / food counters.
- `assets/sprites/wc1/hud/left_panel.{json,png}` — backdrop
  for the portrait + stat column.
- `assets/sprites/wc1/hud/right_panel.{json,png}` — backdrop
  for the command card + minimap column.
- `assets/sprites/wc1/hud/minimap.{json,png}` — minimap frame
  (the dot rendering itself is procedural, not an atlas).
- `assets/sprites/wc1/hud/portrait_icons.png` — 27 × 19 unit
  portrait strip (already used for command-card thumbnails;
  needs a separate large-portrait variant for the left pane).
- `assets/sprites/wc1/hud/icon_border.{json,png}` — already
  used as command-card thumbnail frame; reuse for the large
  portrait + every command-card button.
- `assets/sprites/wc1/hud/bottom_panel.{json,png}` — already
  used as the divider strip; will retire once left/right
  panels take over the bottom-row real estate.

**Evolution sessions** (each ~2-4 h, slot into the natural
gameplay arc — Rule 35 says HUD work earns its hour when the
game's pressure surfaces a real gap, not on a calendar):

#### S8.6 — Resource bar (spawns naturally in S9)
- Top strip via `hud_image(top_resource_bar)` + 3 ×
  `hud_text` widgets (gold / wood / food). Numbers live as
  globals in `wc1_resources.bsm` (S9 owns them). Layout via
  `ui_row` (`hud_image` left-anchored, counters distributed).
- Acceptance: gold/wood/food ticks up when peasants deposit;
  values readable without overlapping sprites.
- **Lands inside S9.**

#### S8.7 — Left pane: large portrait + stat strip
- Convert a HIGH-RES portrait atlas from `war1gus`'s
  per-unit `.png` portraits (the small 27 × 19 strip we have
  is the command-card variant). Author `assets/sprites/wc1/
  hud/portrait_large.{json,png}`. One row per unit, 14 cells.
- Compose left pane: `hud_image(left_panel)` →
  `hud_image(portrait_large, sel.kind)` →
  `hud_text(sel.name)` → `hud_bar(hp_pct, w, h, green, dark)`
  → `hud_text("Atk N  Def N  Rng N")`. Switch to building
  card on building selection (no HP bar — show "Building"
  status + cost stack).
- Acceptance: selecting any unit OR building shows the right
  portrait + stats; faction-colour border on the portrait.
- **Lands as a focused S8.7 or as the visual half of S10
  (production), depending on which arc surfaces it first.**

#### S8.8 — Command card (action buttons)
- 9-slot grid via `ui_row` × 3 of `gui_button` (or a hud-
  flavoured equivalent that reuses `icon_border`). Each
  button binds to an action + a hotkey letter; tooltip on
  hover.
- Per-selection action set: peasants get move / stop / attack
  / build-menu / harvest; combat units get move / stop /
  attack / patrol / hold; buildings get train-{kind} /
  upgrade / cancel.
- Build-menu sub-page for peasants: 9 slots × 2 factions
  worth of building kinds (the same 9 we already wire under
  Shift / Ctrl debug spawns become real buttons here).
- Acceptance: clicking a button issues the same command the
  hotkey would; greyed-out state when unaffordable.
- **Lands in S10 (production) — train + research buttons
  are the killer use case.**

#### S8.9 — Minimap
- `hud_image(minimap)` frame at bottom-right of right pane.
  Inside the frame: a downscaled tile render via
  `tile_render_minimap(tm, mm_w, mm_h)` (new helper in
  `stbtile`?), team-coloured dots via `ecs_query_each`,
  viewport rect from camera state, click-to-scroll mapping
  pixels → world coords.
- Acceptance: unit movements visible on the minimap;
  clicking the minimap snaps the camera there; off-screen
  combat alerts (next session) light up dots.
- **Lands in S11 (AI baseline) or earlier if the player
  starts losing track of off-screen action.**

#### S8.10 — Alert system + voice
- Off-screen damage → flashing dot on minimap + voice clip
  ("We're under attack!"). Failed-command voice ("Cannot
  build there"). Acknowledgement clips ("Yes, my lord",
  "At your command") on selection / move-issue.
- Channel via `stbsound` voices on a dedicated `VOICE` bus
  in stbmixer. Sample bank under `assets/sounds/wc1/voice/`
  (extracted by an existing or new `war1tool` codepath).
- Acceptance: identifiable feedback on every primary command
  + every danger event without the player needing the screen.
- **Lands in S12 (audio integration).**

#### S8.11 — Polish pass (lands with mission 1)
- Faction-tinted portrait borders (gold for human, red for
  orc); pulsing low-HP indicators; resource counters flash
  red on insufficient-funds; mouse-over tooltips for ALL
  buttons + portraits; pause / objectives screen overlay.
- **Lands with S13 (mission 1 playable) — the polish is
  what makes the demo feel shippable, not a separate arc.**

**Why the splay across sessions, not one monolithic "HUD"
session:** the WC1 HUD is integrated with the game state at
every level — resource counters depend on S9, action buttons
depend on S10's production, minimap dots depend on S11's AI,
voice clips depend on S12's audio bus. Bundling them produces
~12 hours of UI work disconnected from the gameplay arc that
makes each piece visible. Spreading them keeps every HUD
deliverable paired with the game-side feature it surfaces —
that's how the bottom command card landed in S8 (you can only
SEE the portrait + icon border because S4 selection +
S8 building exist).

**Engine evolution path (stbhud Tier-2 + Tier-3
specializations):** every HUD addition above is composable
from `stbhud` primitives the S8.5 arc set up (`hud_image`,
`hud_bar`, `hud_render`) plus stbui layout primitives
(`ui_row`, `ui_col`, `ui_box`). The Tier-3 game module is
`wc1_hud.bsm` — it owns the WC1-specific composition (which
portrait, which colour, which layout per selection class),
while the Tier-2 primitive stays game-agnostic. A future RTS
or top-down adventure can compose its own HUD from the same
Tier-2 building blocks without forking `wc1_hud.bsm`.

**Cross-references:**
- `docs/manual/stb++_lib.md` — when stbhud's API grows past
  ~5 widget kinds it earns its own chapter there.
- Rule 33 (cartridge tier triage) — `wc1_hud` is Tier 3 and
  stays in `games/rts1/`; `stbhud` is Tier 2 and stays in
  `stb/`.
- Rule 35 (games as infra stress test) — every gap above
  surfaces a `stbhud` widget or layout primitive; promotion
  to Tier 2 only when the second consumer (fps Doom-mode,
  rpg Game 4) demonstrably needs the same shape.

### Session 9 — Resources (CLOSED 2026-05-20)

**Goal hit**: gold mines + forests harvestable end-to-end.
Peasants gather, walk back to the nearest town hall, deposit;
gold + wood counters tick up in the HUD. Mine reserves +
tree wood deplete on each cycle (felled trees flip to a
walkable stump tile). Food cap recomputes per tick from town
halls + farms.

**Polish landed inside the arc** (originally deferred to a
later session but the user folded it into S9): peasants
disappear into the mine while gathering (canonical WC1
"into-the-mine" feedback), peasants render with sacks of
gold / logs during the RETURNING_* walk back (atlas swap to
peasant_with_gold / peasant_with_wood per faction), trees
flip to stump after one chop cycle and unblock the
pathfinder.

**Shipped this session** (`games/rts1/`):
- `wc1_resources.bsm` (new) — gold / wood / food / food_cap
  globals + accessors + WC1-canonical starting values
  (400 / 400 / 0 / 4). `wc1_resources_deposit(yield_kind)`
  is the credit point the gather state machine calls;
  `wc1_resources_recompute_cap(world)` walks the building
  archetype every tick and rebuilds the food cap from
  BuildingDef.supply (so farms / town halls contribute
  dynamically). `wc1_resources_can_afford` / `_spend`
  are the production gate the S10 training UI will use.
- `wc1_gather.bsm` (new) — the per-peasant state machine.
  GATHER_TARGET_NONE / MINE / TREE + CARRY_NONE / GOLD /
  WOOD enums. `wc1_gather_assign_mine` / `_tree` are
  called by wc1_input.bsm's right-click handler;
  `wc1_gather_tick(world, dt)` advances every active gather
  in one walker (`peasant_q` with `target_kind != 0` early-
  return for non-gatherers). Includes the per-tile wood
  counter (`stbgrid<word>` keyed by tile linear idx) +
  `_find_nearest_town_hall` deposit-router + the tree-to-
  stump tile flip + the `wc1_movement_unblock_tile` call
  to free the pathfinder.
- `wc1_hud.bsm` — `_draw_top_resource_bar()` composes
  the 240×12 strip (tiled 2× across the canvas) + gold
  icon + lumber icon + 3 counters (gold, wood, food/cap).
  Called from `wc1_hud_render` every frame; renders AFTER
  the bottom command card so the strip sits cleanly above
  any HUD overlay.
- `wc1_buildings.bsm` — new `BLDG_GOLD_MINE` kind +
  `_WC1_BLDG_COUNT` bumped to 10. Building struct gains
  `gold_remaining` (only meaningful for mines; other kinds
  leave it at 0). `BLDG_STATE_DEPLETED` enum value for
  mines that hit 0 reserve.
- `wc1_movement.bsm` — `wc1_movement_unblock_tile(gx, gy)`
  drops a permanent terrain blocker from both the
  pathfinder AND the occupancy grid. Called by
  `wc1_gather` when a tree falls.
- `wc1_anims.bsm` — `UNIT_STATE_GATHER_GOLD/WOOD` +
  `UNIT_STATE_RETURNING_GOLD/WOOD` enum entries. The
  resolver dispatches GATHER_* to attack frames (same swing
  motion); RETURNING_* to walk frames (the carry happens at
  the atlas-handle level in wc1_render).
- `wc1_render.bsm` — Gather component added to the peasant
  archetype (10 components total now). `_render_unit`
  inlines the mine-hide skip + the carry-atlas swap (per
  faction). `wc1_render_set_carry_atlases` is the
  single-point setter rts.bpp calls at startup.
- `wc1_input.bsm` — right-click handler grew two new
  branches: gold-mine hit-test (every selected peasant
  enters the gather cycle) + tree-tile hit-test (every
  selected peasant enters the chop cycle). `_issue_move`
  now clears any in-flight gather assignment so a bare
  move command cancels the loop.
- `wc1_combat.bsm` — combat tick skips GATHER_* +
  RETURNING_* states (workers don't auto-engage while
  hauling resources, canonical WC1).
- `rts.bpp` — loads the new resource module + 7 new
  atlases (gold_mine + top_resource_bar + gold_icon +
  lumber_icon + 4 carry atlases). New
  `_spawn_buildings_from_level` runs BEFORE units, picks
  up `kind:"gold-mine"` entries (4 in forest1.json). New
  ticks: `wc1_gather_tick` + `wc1_resources_recompute_cap`.

**Asset additions** (`games/rts1/assets/sprites/wc1/`):
- `buildings/neutral/gold_mine.{json,png}` — 64×64 single
  frame.
- `hud/human/gold_icon.{json,png}` (13×6),
  `hud/human/lumber_icon.{json,png}` (9×9).
- `peasant_with_gold.{json,png}`, `peasant_with_wood.{json,png}`,
  `peon_with_gold.{json,png}`, `peon_with_wood.{json,png}` —
  same 160×416 Aseprite-shaped layout as the base atlases.

**Engine touch** (`stb/`):
- `stbecs.bsm` — `world.archetypes` migrated from
  `arr<ArchetypeRec*>` (per-archetype malloc) to
  `arr_struct<ArchetypeRec>` (inline AoS). User audit after
  the array-migration arc pushed the deferred-then-deleted
  task back open — rts1 is now at 2 archetypes (units +
  buildings) and S11 (AI + projectiles + resources +
  decorations) is the named consumer that justified the
  migration under Rule 36. Compiler binary byte-identical
  to the prior commit (stbecs is a stb cartridge, not
  imported by bpp.bpp).

**Validation**: bootstrap byte-stable. Native suite
177/0/12, C-emit 141/0/48 (last validated on the prior arc
commit; this arc didn't touch the compiler). ECS-specific
smoke tests (`test_ecs_archetype` / `_component` / `_query`
/ `_scheduler`) all pass against the new archetype storage.

**HUD evolution roadmap** — S8.6 (top resource bar) lands
with this session. S8.7 (left pane: large portrait + stat
strip) + S8.8 (command card action buttons) + S8.9
(minimap) + S8.10 (alert system + voice) + S8.11 (polish
pass) remain queued per the roadmap above; each gates on
the corresponding gameplay-arc trigger (S10 production for
the command card, S11 AI for the minimap, S12 audio for
alerts).

**Deferred to S10 / future polish**:
- Production gate not wired yet — `wc1_resources_can_afford`
  + `_spend` shipped as the API, but no UI today calls them.
  Lands with S10's training-button arc.
- Lumber mill drop-point — canonical WC1 routes wood to the
  lumber mill (if built) instead of the town hall. v1 sends
  every deposit to the nearest town hall; the lumber-mill
  variant is a one-line swap in `_find_nearest_town_hall`
  once the player has reason to build one.
- Gold mine "collapse" visuals — mines flip to
  `BLDG_STATE_DEPLETED` when the reserve hits 0 but the
  sprite stays the same. A depleted variant (or simply
  greying the atlas) lands with the eventual mine-sprite
  polish.

### Session 9.x — Gather polish arc (CLOSED 2026-05-21)

S9 shipped end-to-end gather/return/deposit (commit
`d91a110`) and the engine-side follow-ups (`a8e3b07` —
`world_map` NULL fix + `image_draw_*` defensive guards;
`c5e9890` — `ecs_spawn_building` param order). User
playtest on 2026-05-21 surfaced six follow-on issues that
landed today as the S9 polish arc:

**Bugs fixed:**

1. **Tree stump tile** — felled cell now flips to tile 95
   (`removed-tree` per `war1gus/scripts/tilesets/forest.lua`
   line 44) instead of plain grass (tile 109). Visually
   reads as a chopped stump; pathfinder + occupancy still
   unblocked so peasants can walk on it.

2. **Peasant stuck after gather cancel** — `_step_unit`
   early-returns for `UNIT_STATE_GATHER_GOLD/WOOD` so the
   chop animation cycles without the movement tick
   clobbering state. When the player issued a new command
   mid-gather, `wc1_gather_clear` cleared `target_kind`
   but left `an.state` at `GATHER_*` and the worker froze
   forever. New helper `_reset_anim_if_gathering` wired
   into `wc1_gather_clear` / `wc1_gather_assign_mine` /
   `wc1_gather_assign_tree` resets to `IDLE` on every
   re-assignment.

3. **Tree chop fired from 1–2 tiles away** — the 48 px
   reach radius was correct for the 4×4 mine but too
   generous for a single-tile tree. Replaced with
   tile-adjacency: peasant must be on a 4-neighbour of
   the tree (Manhattan distance == 1) before the axe
   swings. Mine still uses dist² against the 4×4
   footprint.

4. **Manual delivery via right-click on TH** — canonical
   WC1 fallback after the player cancels auto-deliver
   mid-trip. New API `wc1_gather_route_to_deposit(world,
   peasant_eid, th_eid)` + input handler branch detects
   "friendly DONE town hall + selected peasants
   carrying gold/wood" → routes each carrying worker to
   deposit at the clicked hall. `wc1_gather_clear` now
   preserves `carry` so the resource isn't dropped when
   the gather loop is interrupted; `_gather_visit`'s
   early-return relaxed from
   `target_kind == NONE` to
   `target_kind == NONE && carry == NONE` so the
   INBOUND branch keeps deposit logic alive when the
   gather assignment is gone but the carry isn't.

5. **Auto-deliver / mine-after-tree stuck mid-path** —
   path_find was emitting waypoint paths *through*
   building footprints because only forest tiles were
   path-blocked at init; `_step_unit` then stalled at
   each footprint boundary. New helper
   `wc1_movement_block_path_tile(gx, gy)` called from
   `_claim_building_footprint` marks every building
   tile path-blocked at spawn, forcing the A* to route
   around. Drop tile picking moved off the hard-coded
   `+48/16` (which sometimes landed inside a tree south
   of the mine footprint) onto the new
   `wc1_movement_find_walkable_perimeter` helper —
   scans every perimeter cell, returns the walkable one
   closest to the building's south-mid-perimeter
   reference. Town-hall seeder
   (`_seed_town_halls_for_players`) gained a `min_r=6`
   gate so the hall lands at least 6 tiles off the
   start-view / gold-mine (no more 4×4 footprints
   corner-touching).

6. **Mine entry "too far / wrong side"** — even with the
   south-biased perimeter pick, the dist² check inside
   `_gather_visit`'s outbound branch was firing the
   moment the peasant grazed the 48 px radius mid-walk,
   landing the chop animation at whichever radius edge
   the approach happened to cross first (usually the
   NW corner when approaching from the human TH). The
   gate now requires `pa.cursor >= pa.count` AS WELL
   AS the dist² check, so the gather only triggers once
   the peasant fully arrives at the perimeter pick.

**Engine wins (Tier 2):**
- `stb/stbpath.bsm` consumer pattern — buildings now
  publish their footprint to BOTH the collision grid
  (`stbgrid` via `wc1_collision_register`) AND the
  A* grid (`stbpath` via the new
  `wc1_movement_block_path_tile`). The two grids stay
  separate (Rule 33 — generic vs game-generic) but the
  game-side helper keeps them in sync at spawn time so
  the path planner respects what the simulator
  enforces.
- `wc1_movement_find_walkable_perimeter` — bias the
  pick by a configurable reference pixel (currently
  south-mid-perimeter for WC1's mine-doorway
  convention). The same helper transparently handles
  partial-tree-bound layouts (e.g. forest1's mine at
  (16,16) where the south face is mostly tree-walled —
  only the SW perimeter cell is walkable; the helper
  still returns it instead of failing).

**Validation:**
- Bootstrap byte-stable (game-side only — compiler not
  touched).
- Native suite + C-emit suite untouched; gather work
  lives entirely in `games/rts1/*.bsm`.
- Manual playtest: peasant mine → return → deposit →
  return → mine loops indefinitely; tree chop, auto-
  deliver, manual interrupt + right-click TH delivery,
  and switching between mine ↔ tree all work without
  stalling. Mine entry sprite now hides at the south
  perimeter pick (visually "under the mine rocks").

**Bonus — wc1_ → rts1_ rename:** User renamed every
`games/rts1/wc1_*.bsm` to `games/rts1/rts1_*.bsm` in
this same session ahead of the polish work. The
namespace prefix now matches the game directory; the
`wc1_*` function names inside the modules stay (they
identify the WC1 mod's domain language) but file
discovery aligns with the `rts1/` folder. Same
treatment for `wc1_handoff.md` → `rts1_handoff.md`
(this doc).

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
`docs/plans/todo.md` and can be cleaned up in any spare half-hour
between sessions — they don't gate the wc1 arc. New code in
`wc1_*.bsm` written under Rule 13 emits zero W032 from day one.

### Tonify checklist on every new function

Mandatory pre-flight per Tonify Rule 14 / Rule 25 / Rule 30 /
Rule 13 / Rule 27 (leaves-first ordering) / Rule 17 (`put`
smart dispatch) / Rule 28 (killer-use-case gate). The checklist
is the pocket manual. Open `docs/manual/tonify_checklist.md` alongside
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
post-1.0 sidequest tracked in `docs/plans/todo.md`.

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
  is tracked separately in `docs/plans/todo.md` and is OK to ignore
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

**Sessions 1-7 CLOSED.** S7 shipped the full combat arc in a
single late-2026-05-18 session: damage matrix + missile entity
pipeline (with `stb/stbprojectile` graduation) + energy & shields
slots + the entire 14-unit WC1 roster wired and spawnable. See
the Session 7 block above for the per-file breakdown.

**S9 (Resources) is next** on the game arc. Gold mines + forests
become harvestable; peasants gather, return to a town hall,
deposit. Resource counter ticks up in the HUD. The HUD work in
S9 dovetails with the S8.5 infra below — gold/wood/food counters
compose through `ui_text` widgets in a declarative layout
alongside `top_resource_bar` panel images.

**Recent activity (2026-05-20, afternoon):** S8.5 — HUD infra
arc CLOSED. **stbhud** Tier-2 cartridge created (`stb/stbhud.
bsm`, ~170 LOC) — layered extension of stbui: imports stbui for
the layout engine + stbrender + stbimage for GPU primitives,
adds `hud_image` / `hud_bar` widgets + `hud_render` GPU walker.
stbui core stays slim (Rule 23 preserved — Bang 9 / Modulab /
level_editor / the_bug / fxlab do NOT transitively pull
stbimage). **bpp_arr** Tier-1 prelude primitive added
(`src/bpp_arr.bsm`, ~80 LOC) — growable struct-sized array
sibling to bpp_array (which is word-only); stbui's UiNode pool
now grows on demand via `arr_struct_*` instead of hard-capping
at 256 with a silent drop. Fixed-point bootstrap achieved
(gen2 == gen3 after the auto-inject change; gen1 != gen2 is
transient). wc1_hud's bottom-panel divider strip refactored to
declarative layout (`ui_box` + `ui_row` + `hud_image`) as
proof-of-life. New rules: **Tonify Rule 35** (games as infra
stress test — games are canonical workloads exposing engine
gaps; the game's pressure is the forcing function) +
**Rule 36** (primitive promotion when two consumers are
PLANNED, not just shipped — refinement of Rule 20 that
prevents migration friction). Suite 177/0/12 + 140/0/48.

**Recent activity (2026-05-20, late morning):** S8 CLOSED. All 9
WC1 building kinds wired (BuildingDef + ECS archetype +
production tick + atlas pipeline), peasants walk to assigned
foundations + chop while progress accumulates, buildings complete
on time. HP bars + construction progress bars over every entity;
left-click selects buildings, command card shows authentic WC1
portraits (27×19 strip from `tilesets/forest/portrait_icons.png`,
scaled 2× into the 56×56 portrait box, framed by `icon_border`).
Tree-tile 76 marked as a permanent blocker so peasants route
around forest patches. Engine-side wins logged in the closeout
block: `stbecs` archetype mask filtering + platform-layer
modifier-key sync + letterbox-aware mouse coords. Suite 176/0/12
+ 140/0/48, bootstrap byte-stable.

**Recent activity (2026-05-19 → 20, earlier):** S8.0 pre-flight
sidequests
CLOSED after three iterations on 8.0b. Final shape: macOS
`_stb_platform_macos.bsm` got two surgical additions — live-
update of `_plt_scale` whenever the contentView resize poll
fires, and a `[NSEvent pressedMouseButtons]` sync at the end of
`_stb_poll_events` that self-heals the LeftMouseUp events Cocoa
silently consumes during window-resize gestures. The button-
state sync was the actual fix for the "phantom marquee after
resize" bug that iterations 1 (window lock, reverted) and 2
(letterbox-aware math rewrite, also reverted) missed. Iteration
2 was a near-miss — the math was correct but exposed the latent
event-loss bug. Rts1's virtual canvas bumped from 320×240 (4:3)
to 480×270 (16:9) so modern monitors get integer-scale
coverage; 1080p / 4K hit zero letterbox, 1440p has a small
margin (480 isn't a factor of 2560). 8.0a — catapult attack
JSON sidecars hand-tuned to 400 ms per attack-tag-start frame
so the 5-frame anim cycles the 2000 ms cooldown smoothly.
Suite 176/0/12 + 140/0/48, bootstrap byte-stable. Maintenance
note in S8.0a: `wc1_sprite_convert` re-runs against the catapult
atlases would regress the tuning until a per-unit override lands
in the tool.

**Recent activity (2026-05-18, evening):** WC1 S7 shipped end-to-
end. Tier-2 `stb/stbprojectile.bsm` graduated with three
prospective consumers in view (rts1, fps Doom-mode, rpg Game 4).
`wc1_sprite_convert` generalized to per-unit schema + configurable
tile size. Map render migrated from legacy `tile_load_set` to
modern `tile_bind_image` (single MTLTexture, smart-bind batched).
Doc `docs/manual/asset_formats.md` gained a "Loader pipeline"
section codifying the canonical WC1-style tileset path.

**Recent activity (2026-05-18, morning):** stbgrid arc + WC1 S6
shipped in 8 commits (`2b7c8d4` → `cfa1e77`). Tonify Rule 33
codifies fully-generic vs genre-generic cartridge taxonomy that
surfaced during the stbgrid graduation. Suite 175/0/12 + bootstrap
byte-stable throughout. Memory
`project_session_20260518_stbgrid_arc.md` has the worked-example
short form for cold-open.

**Earlier activity (2026-05-16 / 2026-05-17):** Aseprite + Modulab
pipeline hardened end-to-end; stbui v2 declarative layout shipped
(closed `aed57c0`); WC1 S6 unblocked end-to-end by stbui v2
closure.
