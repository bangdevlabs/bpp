# B++ Asset Formats — Canonical Spec

Authoritative reference for every JSON / atlas / palette / tileset
format Bang 9 + Modulab author and the games consume. When a new
game arc opens, this is the file that says "use THESE shapes."

If you arrived here from **Tonify Rule 31** or **how_to_dev_b++.md
Cap 16**, you're in the right place — those rules name the
convention; this file defines the schemas. New asset categories
land here at the same time as their first consumer.

---

## Level JSON

Used by every game that ships a tile-grid level: pathfind, fps,
rts1 (WC1 mod), future games. Authored by Bang 9's Level Editor
tab. Loaded at runtime via `bpp_json` (auto-injected).

The level format has **two modes** chosen via the schema:

| Mode | Tile semantics | Picker UI | When to use |
|------|----------------|-----------|-------------|
| **`palette`** | Tile id is an index into an N-color palette (default MCU-8 = 8 colors) | 8 colored circles | Schematic / colour-blocked games — pathfind walls, fps wolfenstein-style tiles, anything where 8 colours is enough vocabulary |
| **`tileset`** | Tile id is a 1-based row-major index into a PNG tileset image | Scrollable grid of actual tile graphics | Games with real tile art — rts1's WC1 forest (320 tiles), future rts2 Red Alert, any time the visual vocabulary exceeds a flat palette |

A level file declares exactly ONE mode by which top-level field it
carries: `"palette"` (palette mode) or `"tileset"` (tileset mode).
Missing both → palette mode with default MCU-8 (backward compat).

### Palette mode (pathfind / fps style)

```json
{
  "type": "level",
  "version": 2,
  "width": 20,
  "height": 11,
  "palette": "MCU-8",
  "tiles": [
    [0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 6, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 1, 0, 0]
  ],
  "entities": [
    { "kind": "player_spawn", "x": 2.5, "y": 8.5 },
    { "kind": "enemy",         "x": 11.5, "y": 9.5 }
  ]
}
```

Field reference:

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `type` | string | yes | Always `"level"` |
| `version` | int | yes | Schema version. Current = `2`. Older versions get a clear migration error from the loader |
| `width` | int | yes | Grid width in cells |
| `height` | int | yes | Grid height in cells |
| `palette` | string | optional | Palette name. Currently only `"MCU-8"` is implemented. Missing = `MCU-8` default |
| `tiles` | array of arrays of int | yes | Row-major grid. `tiles[y][x]` is the cell at column `x`, row `y`. Values are palette indices `0..N-1` (clamped on load) |
| `entities` | array of objects | yes (may be empty) | Spawn points + game-specific objects placed on the grid. See "Entity entries" below |

**MCU-8 palette** (the only palette implemented today):

| Index | ARGB | Name |
|-------|------|------|
| 0 | `0x00000000` | transparent (empty cell) |
| 1 | `0xFF000000` | black |
| 2 | `0xFF8C1A1A` | dark red |
| 3 | `0xFF808080` | gray |
| 4 | `0xFFFFA500` | orange |
| 5 | `0xFF008000` | green |
| 6 | `0xFF800080` | purple |
| 7 | `0xFFFFFF00` | yellow |

Game-side semantics are the game's call — pathfind treats every
non-zero index as a wall; fps assigns specific colours to wall /
floor / door tiles. The palette is just the visual.

### Tileset mode (rts1 / WC1 style)

```json
{
  "type": "level",
  "version": 2,
  "width": 64,
  "height": 64,
  "tileset": "assets/converted/graphics/tilesets/forest/terrain.png",
  "tileset_tw": 16,
  "tileset_th": 16,
  "tiles": [
    [94, 94, 80, 79, 109, 109, ...],
    [80, 93, 79, 109, 109, 109, ...]
  ],
  "entities": [
    { "kind": "start_view", "player": 0, "x": 16.0, "y": 16.0 }
  ]
}
```

Field reference (differences from palette mode):

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `tileset` | string | yes (tileset mode) | Path to the source PNG, **relative to the project's `assets/` directory** (same anchor convention `path_asset` uses) |
| `tileset_tw` | int | optional | Tile width in pixels. Default `16` |
| `tileset_th` | int | optional | Tile height in pixels. Default `16` |
| `tiles` | array of arrays of int | yes | Same shape as palette mode, but values are 1-based row-major indices into the tileset PNG. `0` = empty cell (no draw) |

**Tile-id convention:** the tileset PNG is read row-major top-to-
bottom, left-to-right. Tile `1` is at PNG `(0, 0)`. Tile `N` lives
at PNG `((N - 1) % cols * tw, (N - 1) / cols * th)`. ID `0` is
reserved for "empty" and never references a PNG cell.

This is the SAME convention `stbtile`'s `tile_load_set` produces.
Tile IDs round-trip cleanly: war1tool → `wc1_map_convert` → JSON
→ Level Editor → game.

### Entity entries

Every level (both modes) carries an `entities` array. Each entry
is an object with at least `kind` + position. Position fields
depend on the game's coordinate convention — pathfind / fps use
cell-centered floats (`x = gx + 0.5`, `y = gy + 0.5`); rts1
games typically use whole-cell integers.

Common kinds the Level Editor recognises:

| Kind | Used by | Meaning |
|------|---------|---------|
| `player_spawn` | pathfind, fps | Player start position |
| `enemy` | fps | Enemy spawn |
| `start_view` | rts1 | Camera initial centre per player. Extra field: `player` (int) |

New games add new kinds by listing them in the editor's
`_init_kinds()` (see `tools/level_editor/level_editor_lib.bsm`).

---

## Atlas Pack JSON (sprite atlases)

Used by every game shipping sprites: pathfind cat/rat, fps player /
enemy / weapon, rts1 unit portraits, future rts2 Red Alert units.
Authored by Modulab's "Atlas" topbar button, loaded at runtime
via `image_load`.

### Schema

```json
{
  "image": "pathfind.atlas.png",
  "tw": 16,
  "th": 16,
  "frames": [
    { "id": "rat",  "x": 0,  "y": 0, "w": 16, "h": 16 },
    { "id": "cat",  "x": 16, "y": 0, "w": 16, "h": 16 }
  ]
}
```

Field reference:

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `image` | string | yes | Path to the combined PNG, relative to the atlas JSON's directory |
| `tw`, `th` | int | optional | Default tile/frame dimensions in the atlas — used when individual frames omit `w` / `h` |
| `frames` | array | yes | Named regions in the PNG. Each has `id` (string), `x`, `y` and optionally `w`, `h` |

Consumers do:

```bpp
auto atlas;
atlas = image_load("assets/sprites/pathfind.json");
auto rat_id; rat_id = image_named_id(atlas, "rat");
image_draw_size(atlas, rat_id, sx, sy, 16, 16);
```

The atlas pack supports hot-reload via
`image_hot_reload_enable(atlas)` — Modulab edits the PNG, the
running game picks up the new pixels within ~30ms.

---

## Sprite Sheet JSON (Aseprite-compatible)

Canonical format for sprites that carry **animation metadata** —
WC1 units (peasant + future footman / archer / ...), and any
sprite imported from a tool that exports the Aseprite shape
(Aseprite itself, TexturePacker, war1tool via
`tools/wc1_sprite_convert`). The runtime consumes the LITERAL
Aseprite "Export Sprite Sheet > JSON Array > Frame Tags" output
— no bespoke field names, no extra envelope.

Adopted as canonical 2026-05-16 (sidequest
`docs/sidequest_wc1_modulab_pipeline.md`) — Modulab repositions
as IDE-companion to Aseprite, not Aseprite-replacement.

### Schema

```json
{
  "frames": [
    {
      "filename": "peasant 0",
      "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
      "rotated": false,
      "trimmed": false,
      "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
      "sourceSize": {"w": 32, "h": 32},
      "duration": 100
    },
    {"filename": "peasant 1", "frame": {"x": 32, "y": 0, ...}, ...}
  ],
  "meta": {
    "image": "peasant.png",
    "size": {"w": 160, "h": 416},
    "scale": "1",
    "format": "RGBA8888",
    "frameTags": [
      {"name": "idle_S",  "from": 4, "to": 4,  "direction": "forward"},
      {"name": "walk_N",  "from": 5, "to": 9,  "direction": "forward"},
      {"name": "walk_NE", "from": 10, "to": 14, "direction": "forward"}
    ]
  }
}
```

Field reference:

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `frames` | array | yes | Per-frame bounding boxes inside the source PNG. Each carries `filename` (string id, accessed via `image_named_id`), `frame` (rect inside the PNG), `duration` (ms). `rotated` / `trimmed` / `spriteSourceSize` / `sourceSize` are honoured if present, defaulted to the un-rotated/un-trimmed shape otherwise |
| `meta.image` | string | yes | Path to the source PNG, **resolved relative to the JSON file's directory** (same convention as atlas_grid) |
| `meta.size` | `{w,h}` | optional | Source PNG dimensions; informational |
| `meta.frameTags` | array | optional | Named animation ranges. Each: `name` (string, looked up via `image_anim_id`), `from` / `to` (inclusive frame index range), `direction` (one of `forward` / `reverse` / `pingpong` — only `forward` honoured in v1; non-forward is parsed but plays as forward) |

### Consumers

```bpp
auto atlas;
atlas = image_load("assets/sprites/wc1/peasant.json");

// Cache anim ids once at startup
auto walk_n_id;
walk_n_id = image_anim_id(atlas, "walk_N");

// Per-frame: resolve current sprite from the cycle
auto sprite_id;
sprite_id = image_anim_frame(atlas, walk_n_id, elapsed_ms);
image_draw_size(atlas, sprite_id, sx, sy, 32, 32);

// Or mirror for the western half (WC1 5-facing convention)
image_draw_size_flipped(atlas, sprite_id, sx, sy, 32, 32);
```

### Detection

`image_load` dispatches by content shape, not extension. The
sniffer routes the JSON to `_image_load_packed_format` when it
finds a top-level `frames` array AND no `type` field — exactly
Aseprite's output. Hand-authored Modulab packs (`type:
"atlas_pack"`) and uniform grids (`type: "atlas_grid"`) keep
their existing paths.

### Variable per-frame duration

The schema supports it (`duration` is per-frame, not per-tag),
but v1 of the engine reads ONE duration per tag (the first frame's)
and applies it uniformly across the cycle. War1tool imports and
typical Aseprite exports use uniform-rate per tag anyway. Variable-
rate cycles inside a tag would require walking the durations array
on every `image_anim_frame` call — schema bump when a real consumer
needs it.

### Aseprite Hash format (NOT supported)

Aseprite's "JSON Hash" export keys `frames` by filename string
instead of by integer position. The runtime loader does NOT
parse that shape — use "JSON Array" instead. The error message
on Hash is clear: "missing `frames` array (Hash format not
supported)".

---

## Atlas Grid JSON (sprite sheets)

Used by games that ship sprite sheets where every tile is the
same fixed size — rts1's WC1 unit strips (5 columns × 13 rows
of 32×32 frames), future tilesheet-style sprites. Authored
indirectly: the source PNG comes from an external tool (war1tool
for rts1, future hand-painted sheets for other games) and a tiny
sister JSON declares the slice geometry. The sister JSON IS
editable in any text editor or Bang 9; in Modulab it lives
read-only because Modulab's canvas authors per-pixel sprites,
not pre-baked PNGs.

### Schema

```json
{
  "type": "atlas_grid",
  "version": 2,
  "tile_w": 32,
  "tile_h": 32,
  "image": "../../converted/graphics/human/units/peasant.png"
}
```

Field reference:

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `type` | string | yes | Always `"atlas_grid"` |
| `version` | int | yes | Current = `2` |
| `tile_w`, `tile_h` | int | yes | Tile dimensions in pixels (uniform across the whole sheet) |
| `image` | string | yes | Path to source PNG, **resolved relative to the JSON file's directory** (see "Compatibility rules" below). `..` segments allowed and resolved by the OS at `open()` time |

The runtime slices the source PNG row-major into uniform
`tile_w × tile_h` tiles. Frame count = `(image_w / tile_w) *
(image_h / tile_h)`. Frames are addressed by **integer index
0..N-1**, no named accessor — human-readable names live as
`const` declarations in the consuming game's `.bsm`:

```bpp
const PEASANT_IDLE_N  = 0;
const PEASANT_IDLE_NE = 1;
const PEASANT_IDLE_E  = 2;
const PEASANT_IDLE_SE = 3;
const PEASANT_IDLE_S  = 4;
```

Consumers do:

```bpp
auto atlas;
atlas = image_load("assets/sprites/wc1/peasant.json");
image_draw_size(atlas, PEASANT_IDLE_S, sx, sy, 32, 32);
```

**When to use vs. Atlas Pack JSON:** atlas_grid expects ONE source
PNG with uniform tiles, indexed by position. Atlas Pack JSON
composes MULTIPLE per-sprite Modulab files into a named manifest.
Pick atlas_grid when the source is a pre-baked sheet (war1tool
output, Aseprite spritesheet export, etc.); pick Atlas Pack when
each frame is authored as its own Modulab sprite.

Hot-reload: atlas_grid honours `image_hot_reload_enable(atlas)`
on the sister JSON itself; editing the PNG independently does
NOT trigger reload today (only the JSON's bytes are watched —
see todo for the PNG-watch extension).

---

## Audio

Two simple formats, no JSON envelope, no custom schema:

| Asset | Format | Loader | Authoring |
|-------|--------|--------|-----------|
| One-shot sound effects | `.wav` (16-bit PCM) | `sound_load_wav` (stbsound) | External editor (Audacity / Reaper). war1tool emits these directly from WC1's binary asset bundle |
| Background music | `.mid` (Format 0 or 1; SMPTE division rejected — stbmidi only supports musical-time division per the 2026-05-12 ship) | `midi_play_file` (stbmidi) | External sequencer / DAW |

Audio assets are NOT edited inside Bang 9 today. If a game needs
in-engine audio editing, that's a separate tool arc (none planned
as of 2026-05-13).

---

## Compatibility rules

**Version field is mandatory in every JSON envelope.** Current
version everywhere is `2`. Bumping to `3` means a schema-breaking
change — the loader produces a clear migration error rather than
silently misinterpreting old fields.

**Optional fields default sensibly.** A missing `palette` defaults
to `MCU-8`, a missing `tileset_tw` defaults to `16`, a missing
`entities` array defaults to empty. Older files keep working
without rewriting them.

**One mode per level.** A level declares `palette` OR `tileset`,
never both. Mixed-mode levels are a future feature (per-cell
"this cell uses tileset, that cell uses palette") with no current
consumer — Rule 28 gates the addition until a game actually needs
it.

**Path fields are JSON-relative.** Every path inside a JSON
(atlas `image`, level `tileset`) resolves against the directory
containing the JSON file itself — NOT via `path_asset`. The
runtime concatenates the JSON's directory with the path string
verbatim; relative segments (`..`) are allowed and resolved by
the OS at `open()` time. Absolute paths (starting with `/`)
bypass the JSON-dir prefix entirely.

This convention means siblings ship side-by-side cleanly
(`peasant.json` + `peasant.png` in the same dir, `image:
"peasant.png"`), and authored JSONs in `assets/sprites/` can
point back into `../../converted/...` without the runtime
needing a project-root concept.

---

## How to extend this spec

When a new asset shape lands:

1. Add a section here describing the canonical schema + field
   reference + a worked example.
2. Update Tonify Rule 31's "canonical asset → tool → format →
   loader" table to mention the new shape.
3. Update `how_to_dev_b++.md` Cap 16 "Asset infra" if the
   convention point changes.
4. Ship the corresponding Level Editor / Modulab support and
   the first consumer game in the same arc.

Asset shapes only graduate into this spec when at least TWO
consumers exist (Rule 20 two-consumer rule) — until then the
shape can live game-local and migrate up later.

---

## Cross-references

- `docs/manual/tonify_checklist.md` Rule 31 — discipline rule that says
  "games consume Bang 9 / Modulab formats."
- `docs/manual/how_to_dev_b++.md` Cap 16 "Asset infra" — short paragraph
  pointing back at this file.
- `tools/level_editor/level_editor_lib.bsm` — Level Editor source,
  consumes both level modes.
- `tools/modulab/modulab_lib.bsm` — Modulab source, authors atlas
  packs.
- `tools/wc1_map_convert/wc1_map_convert.bpp` — offline converter
  that produces tileset-mode level JSON from war1tool SMS files.
- `stb/stbtile.bsm` `tile_load_set` — runtime tileset loader,
  same row-major id convention as the spec.
- `stb/stbimage.bsm` `image_load` — runtime atlas pack loader.
- `src/bpp_json.bsm` — auto-injected JSON reader / writer used by
  every consumer above.
