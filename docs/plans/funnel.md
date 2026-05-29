# Funnel — the asset pipeline (rip → BangDev production formats)

**Status:** design / proposed 2026-05-28. Generalizes the two existing WC1
one-shot converters into a multi-source, multi-kind asset funnel that emits the
project's canonical production formats (consumed by Bang 9 + the games via
`image_load` / the level loader). Tool-side only — **no engine change**, so no
bootstrap impact (Tonify Rule 31: the converter emits the canonical format; the
game never reads a foreign one).

## The shape: a `source × kind` matrix

Funnel is not one converter — it is a matrix. A **frontend** decodes a source
game's bytes into a per-kind **intermediate representation (IR)**; a **backend**
emits the canonical format for that kind. This is the compiler's frontend/backend
split (semantics in the frontend, emission in the backend) applied to assets.

|  source ↓ \ kind →     | **sprite** → Aseprite | **tileset** → atlas_grid | **map** → level JSON | sound |
|------------------------|-----------------------|--------------------------|----------------------|-------|
| **WC1** (war1gus)      | `wc1_sprite_convert` ✓ | `terrain.json` (trivial) | `wc1_map_convert` ✓  | —     |
| **Wolf3D** (VSWAP/GMAPS)| NEW (VSWAP sprites)   | NEW (VSWAP walls)        | NEW (GAMEMAPS)       | later |
| doom / sc1 / …         | future                | future                   | future               | —     |

CLI: `funnel <source> <kind> <input> <out_dir> [opts]`, e.g.
`funnel wc1 sprite peasant.png out/ 5 4 3 32 32`,
`funnel wolf3d sprite VSWAP.WL1 out/`,
`funnel wc1 map forest1.sms out/`.
A `(source, kind)` pair selects one `(frontend, backend)` lane.

## The three canonical output formats (already shipped — do not change)

1. **Aseprite sprite** (`peasant.json`): `frames[]` (per-frame pixel rects +
   duration) + `meta.image` + `meta.frameTags[]` (named anims, `from`/`to`,
   direction). Consumed by `image_load` → `_image_load_packed_format`.
2. **atlas_grid tileset** (`terrain.json`): `{type:"atlas_grid", tile_w, tile_h,
   image}` — a uniform grid, no per-tile metadata. Consumed by `image_load` +
   `tile_bind_image`.
3. **level** (`forest1.json`): `{type:"level", version:2, width, height, tileset,
   tileset_tw/th, tiles:[[…]], entities:[{kind,x,y,player}]}`. Cell-centered
   entity coords (`gx+0.5`). Consumed by `wc1_map_load` / the level editor /
   Bang 9 `_panel_levels`.

## The IR (designed FROM two real frontends, not in the abstract)

Per Rule 20/28: do not invent a speculative "any format" IR. Shape each IR by
refactoring the WC1 lane **and** writing the Wolf3D lane together, so the
abstraction is grounded in two concrete frontends.

- **Sprite-bank IR** — a list of frames `{ rgba_pixels, w, h }` grouped into
  named sprites, plus animation tags `{ name, from, to, duration_ms, direction }`
  and an optional pivot/origin. (Aseprite is essentially this serialized.)
- **Tilemap IR** — `{ width, height, tile_w, tile_h, tiles[h][w], entities[] }`.
- **Tileset IR** — `{ tile_w, tile_h, frames[]:rgba }` (a sprite-bank with grid
  layout; the atlas_grid backend just records the grid + writes the PNG).

## Frontends

- **`wc1 sprite`** (exists, refactor): war1gus already de-palettizes into an RGBA
  PNG grid, so this frontend reads dims + applies the war1gus round-robin schema
  (`GetFrameNumbers`) and copies the PNG byte-for-byte. **Key fact: it does NOT
  pack** — the source is pre-packed.
- **`wc1 map`** (exists): parses the Stratagus `.sms` (Lua-ish `SetTile` /
  `CreateUnit` / `SetStartView`) into the tilemap IR.
- **`wolf3d sprite/tileset`** (NEW): decode `VSWAP` directly in B++ (we own the
  extraction — VSWAP is simpler + better-documented than the WC1 formats, no
  external tool needed). Header: `chunk_count`, `sprite_start`, `sound_start` +
  `offset[]`/`length[]`. Chunks `[0,sprite_start)` = walls (64×64 raw,
  column-major). `[sprite_start,sound_start)` = sprites (compiled-sprite: leftpix
  / rightpix + column-offset table + transparent RLE posts). Both palette-indexed
  → RGBA via the fixed Wolf3D VGA palette (embed the 256-colour table). Unlike
  WC1, Wolf3D gives **individual sprites** → needs the atlas packer (below).
- **`wolf3d map`** (NEW, later): `GAMEMAPS` + `MAPHEAD` (RLEW + Carmack
  decompression) → tilemap IR.

## Backends

- **Aseprite emitter** (extract from `wc1_sprite_convert`'s `_emit_*`): take the
  sprite-bank IR, write the PNG, emit `frames[]` + `meta.frameTags[]`.
- **Atlas packer (NEW — the one genuinely-new piece of infra).** WC1 skips it
  (war1tool pre-packs). Wolf3D's individual sprites must be packed into a sheet:
  a simple shelf/row packer is plenty (frames are uniform-ish). Writes the sheet
  PNG via `pixels_save_png` (already exists — Modulab uses it). This is the
  meatiest new work and the thing that makes funnel more than a rename.
- **atlas_grid emitter** (trivial): grid PNG + the 4-line `terrain.json`.
- **level emitter** (exists in `wc1_map_convert`): tilemap IR → level JSON.

## Phasing

- **S0 — prove the format (inert, no asset committed).** `vswap_dump`: read a
  `VSWAP.WL1` header, list chunk counts (walls / sprites / sounds) + per-chunk
  sizes. Confirms we own the format. **Needs a real `VSWAP.WL1` to run** — the
  shareware data (`.WL1`) is freely distributable (Apogee/id shareware licence);
  the registered `.WL6` requires owning the game. (The engine is GPL; the
  registered *data* is not — same provenance care as the WC1 CD rip.)
  **DONE 2026-05-28** (`tools/funnel/vswap_dump.bpp`): header parse verified
  against a synthetic 56-byte fixture (5 chunks → 2 walls / 2 sprites / 1
  sound, offsets + lengths exact). Awaiting a real `.WL1` to confirm on game
  data.
- **S1 — walls → RGBA.** Decode 64×64 raw walls + apply the VGA palette → PNG.
  Validates palette + `pixels_save_png`. Emit an atlas_grid tileset.
- **S2 — sprites → RGBA.** Decode the compiled-sprite post format + transparency.
- **S3 — the shared backend.** Extract the Aseprite emitter + write the atlas
  packer; refactor `wc1_sprite_convert` to be the first frontend over it, Wolf3D
  the second. (IR shape falls out of having both.)
- **S4 — Wolf3D anim schema.** Port the ECWolf / Wolf4SDL sprite enums (which
  chunk ranges form `walk_N` / `attack` / `death`, by direction) into a schema
  table, analogous to WC1's `GetFrameNumbers`.
- **S5 — maps (later).** GAMEMAPS/MAPHEAD frontend → level JSON.

## Why this is worth doing (and not premature)

- **Doctrine fit:** frontend/backend split; Rule 31 (offline canonical
  converter); the 2-consumer gate (Rule 20/28) is satisfied — WC1 (consumer 1) +
  Wolf3D / the FPS (consumer 2, concrete + named, per the Frankenstein vision).
- **Risk is tool-only:** the engine, `image_load`, hot-reload, the level loader
  are all UNCHANGED. No bootstrap / tripod impact.
- **It makes Bang 9 a real pipeline hub:** `rip → funnel → Aseprite/level → edit
  in Bang 9 → hot-reload in the game`, for any classic game's assets — the
  BangDev-studio vision.

## The one real blocker

The Wolf3D lanes need a real `VSWAP.WL1` (and later `GAMEMAPS.WL1`/`MAPHEAD.WL1`)
to write *and verify* the decoders against. Until that file is present, S0–S2 can
be written but not proven. Everything before that (the plan, the atlas packer
with a synthetic sprite-bank test, the wc1-lane refactor) is verifiable without
the data.
