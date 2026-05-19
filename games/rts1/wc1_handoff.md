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

### Session 8.0 — Pre-flight sidequests (open at S7 closeout)

Caught during the manual smoke of the full S7 roster; landed in
the S7 closeout commit as pending issues to address before the
S8 building work begins.

**8.0a — Catapult animation polish.** The human catapult and orc
catapult atlases ship from war1tool byte-identical (faithful to
WC1 1994 — faction-distinction only landed in WC2). Visually
correct per source bundle, but the in-game catapult animation
under combat looks stiff vs the SC1-paced melee animations
around it. Two angles to investigate:

- Per-frame timing — the 2,5,3 schema's 5 attack frames may be
  cycling too fast (default 100 ms per frame ≈ 500 ms full attack
  cycle, but a catapult fires every 2000 ms per its cooldown,
  so the rest of the cycle is idle-static — looks like the
  catapult "twitches" once per cooldown).
- Faction tint — even if the original assets are byte-identical,
  a runtime palette swap on the orc catapult (red flags / orange
  tint) would help players read the unit at a glance without
  needing a sprite redraw.

Defer the palette swap until `stbpal` + atlas-runtime-recolor is
exercised in another consumer (Rule 28). Frame-timing fix is
local to `wc1_anims` + a per-state duration override.

**8.0b — Window resize breaks mouse centering.** Resizing the
running game's window mid-session (drag the window edge) breaks
the cursor → world-coord transform. The `cam_screen_to_world_x/y`
math reads from a SCREEN_W / SCREEN_H constant baked at game_init
time; the underlying NSView grows but the camera viewport stays
nailed to 320×240, so the cursor sample maps to the wrong tile
once the window doesn't match.

Two paths:
- Lock window size at game_init (cheapest — disables the user's
  drag-resize, but matches WC1 1994 fixed-resolution behaviour).
- Plumb a `game_window_size(&w, &h)` accessor through stbgame +
  recompute SCREEN_W / SCREEN_H + cam viewport on every frame
  (proper fix, but touches stbcamera + stbinput coordinate
  conversion). Worth doing right when a future game (rpg
  Dungeon, top-down adventure) wants resize support genuinely.

Likely the first path for v1; the second graduates into stb when
a real consumer needs runtime resize. See `feedback_phase_
overengineering_lesson` — don't build the generic fix without a
second consumer.

### Session 8 — Buildings + construction (~3-4h)

**Goal**: place foundation, peasant walks to it, construction
animation runs, building completes. Town hall, farm, barracks
playable.

**Files**:
- `wc1_buildings.bsm` — building type table (port of buildings.lua).
- `wc1_production.bsm` — construction state machine.

**Tool prep**: `wc1_sprite_convert` gains a `--mode building`
code path. Buildings differ structurally from units — single
still frame + collapse cycle, no per-direction layout, no
attack/walk/die round-robin. The construction animation (the
peasant scaffolding overlay) is a separate animation that lives
on top of the building sprite, not inside it.

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

**S8 (Buildings + construction) is next** on the game arc.
Foundations: town hall, farm, barracks. A peasant walks to the
foundation, plays the construction animation, building completes
and unlocks training. Building schema differs structurally from
unit schema (single still frame + collapse cycle, no per-
direction layout) so `wc1_sprite_convert` gains a separate
`--mode building` code path.

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
