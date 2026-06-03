# B++ Funnel — Asset Conversion Manual

`tools/funnel/` is the **funnel**: the one place every offline asset
converter lives. A foreign game ships its art, maps and sound in a
binary format nobody else can read; a funnel lane reads those bytes
**once, at asset-prep time**, and emits the canonical Bang 9 /
Aseprite / level_editor shapes the games consume at runtime.

This is **Tonify Rule 31** made concrete:

> If an asset arrives in a foreign syntax (a compiled archive, a
> Lua-ish map script, a custom sprite container), write a one-shot
> offline converter under `tools/funnel/` that emits the canonical
> Bang 9 format. **The game NEVER reads the foreign format at
> runtime.**

Two consequences fall out of that rule and define everything below:

- **Zero non-B++ runtime dependencies.** No Lua interpreter, no SDL,
  no foreign-format parser linked into the game binary. The funnel
  absorbs the foreign format offline; the game only ever sees our
  JSON + PNG + WAV/MID.
- **The funnel reads bytes, it never executes them.** A funnel lane
  decodes a game's data files; it never runs the game's executables.
  Pointing a lane at a downloaded archive is safe by construction —
  worst case it produces garbage pixels, never code execution.

If you arrived here from **how_to_dev_b++.md Cap 16** or **Tonify
Rule 31**, this is the file that says *which lanes exist, how to run
them, how to add one, and what shapes they must emit.*

---

## The three lanes

The funnel currently funnels three games. Each is a different point
on the "how hard is the source format" spectrum, and together they
are the template for the next one.

| Lane | Source game | Container | Strategy | Tools |
|------|-------------|-----------|----------|-------|
| **WC1** | Warcraft: Orcs & Humans (rts1) | `DATA.WAR` | consume an external tool's PNG/SMS output | `wc1_sprite_convert`, `wc1_map_convert` |
| **Wolf3D** | Wolfenstein 3D (fps) | `VSWAP.WL1/WL6` | own B++ decoder (S0 dump-first) | `vswap_dump`, `vswap_walls`, `vswap_sprites` |
| **WC2** | Warcraft II: Tides of Darkness (rts2) | `MAINDAT.WAR` + `.PUD` | own B++ decoder, end-to-end | `war2_dump`, `war2_gfx`, `war2_tiles`, `war2_pud` (+ `war2_extract_rts2.sh`) |

Every lane is a standalone `.bpp` (no imports beyond `stbpixels` for
the PNG codec). A lane that has graduated to a one-command pipeline
adds a `.sh` driver that builds the tools and runs the whole roster.

### WC1 — consume an external tool

The WC1 source data (`DATA.WAR`) was unpacked by **unblzzrd** + the
**war1tool** converter (from the war1gus project) into PNG strips,
SMS maps and Lua scripts. The funnel then turns *those* into our
shapes — it never touches the binary archive itself.

```sh
# Unit sprite strip (war1tool PNG) -> Aseprite sheet sidecar.
# Optional walk/attack/death frame counts come from war1gus anim.lua;
# defaults are the peasant/peon schema (5 4 3).
bpp tools/funnel/wc1_sprite_convert.bpp -o /tmp/wc1_sprite
/tmp/wc1_sprite peasant.png assets/sprites/wc1/ 5 4 3

# Stratagus SMS map (Lua-ish SetTile calls) -> tileset-mode level JSON.
bpp tools/funnel/wc1_map_convert.bpp -o /tmp/wc1_map
/tmp/wc1_map forest1.sms assets/levels/forest1.json
```

The lesson WC1 teaches: when a competent external extractor already
exists, **let it do the binary unpack** and write only the
foreign-shape → canonical-shape converter. Cheapest path.

### Wolf3D — own decoder, dump-first

Wolfenstein 3D ships everything in one `VSWAP` page file (the
shareware `VSWAP.WL1` is freely redistributable, which is why this
lane needs no external tool). The funnel decodes the container
itself, and it does so in the order that defines the funnel's house
methodology: **prove the container before decoding a pixel.**

```sh
# S0 — report the chunk layout. No pixels decoded; just validates we
# own the header + offset/length tables. Verify against a small fixture
# first, then point it at a real VSWAP.
bpp tools/funnel/vswap_dump.bpp -o /tmp/vswap_dump
/tmp/vswap_dump VSWAP.WL1

# S1 — wall pages (64x64 column-major) -> atlas_grid tileset PNG+JSON.
bpp tools/funnel/vswap_walls.bpp -o /tmp/vswap_walls
/tmp/vswap_walls VSWAP.WL1 palette.pal out_dir/

# S2 — sprite pages (compiled column-posts) -> Aseprite atlas PNG+JSON.
bpp tools/funnel/vswap_sprites.bpp -o /tmp/vswap_sprites
/tmp/vswap_sprites VSWAP.WL1 palette.pal out_dir/
```

The palette is **input data** — the game's own VGA palette read from
the `.pal` file, never hardcoded. 6-bit VGA values (0..63) scale `<<2`
to 8-bit.

### WC2 — own decoder, end-to-end

Warcraft II (DOS Tides of Darkness) keeps graphics in `MAINDAT.WAR`
and maps as loose `.PUD` files. The `.WAR` container is the same
family as WC1's (a magic word, an entry count, an offset table) so
the funnel decodes it in pure B++ — no external tool, full zero-dep.
This is the most complete lane and the working template for any new
archive-based game.

```sh
# One command: builds the tools and extracts sprites + tilesets + maps
# into games/rts2/assets/{sprites,tiles,levels}/wc2/.
sh tools/funnel/war2_extract_rts2.sh [MAINDAT.WAR] [OUT_DIR]
```

Under the hood it runs four tools, each usable standalone:

```sh
# Container inspector — magic, entry count, offset table, integrity.
/tmp/war2_dump  MAINDAT.WAR

# Sprite/building chunk -> Aseprite sheet (PNG + .json sidecar).
#   <pal_idx> <gfx_idx> come from war2_maindat_index.md.
/tmp/war2_gfx   MAINDAT.WAR 2 47 peasant.png

# Terrain tileset -> atlas_grid 16x16 sheet (PNG + .json).
#   <pal> <mega> <mini> triple per tileset.
/tmp/war2_tiles MAINDAT.WAR 2 3 4 summer.png

# PUD map -> level_editor JSON.
/tmp/war2_pud   ALAMO.PUD alamo.json
```

Two companion files make the lane reproducible:

- **`war2_maindat_index.md`** — the index→asset+palette map (lifted
  from Wargus `wartool.h`, verified against our archive). It says
  which entry index is the peasant, which palette pairs with it,
  which triple makes each tileset.
- **`war2_extract_rts2.sh`** — the driver. Its `idx palette name`
  table is the **source of truth**: re-run after renaming a row and
  the asset regenerates with the new name, sidecar `meta.image`
  included — no manual JSON editing.

#### WC2 format notes (the bits that bit us)

- **Chunk compression:** entries with flag `0x20` in the high byte of
  their leading `u32` are LZSS-compressed (4096-byte ring window;
  control byte LSB-first: bit 1 = literal, bit 0 = back-reference
  `u16 = (len-3)<<12 | offset`). Same algorithm as WC1.
- **Sprite frame:** header is `u16 nframes, u16 width, u16 height`
  (WC1 used `u16, u8, u8`). Each frame is **RLE per row** — the frame
  data opens with `u16 rowoffset[height]`, then packets: `b & 0x80`
  transparent run `(b & 0x7F)`, `b & 0x40` run-length `(b & 0x3F)`
  copies of the next byte, else literal `b` bytes. Palette **2**.
  Transparent indices **0 and 255**.
- **Tileset:** a megatile table (`idx-1`, 8 bytes = 4×u16 per 16×16
  tile) over 8×8 minitiles (`idx`). Minitile byte offset =
  `(ref & 0xFFFC) << 4` (WC1 used `<<1`); `ref & 2` flips X,
  `ref & 1` flips Y.
- **PUD map:** tagged sections (`tag[4]` + `u32 len` + data). Read
  `ERA ` (tileset id 0–3), `DIM ` (w,h), `MTXM` (w×h `u16` tiles).
  **MTXM indexes the megatile sheet directly.**

---

## Anatomy of a lane — how to add a new game

Adding a fourth game (a new RTS, a new shooter) follows the WC2
template. The discipline, in order:

1. **S0 — dump the container first.** Write `<game>_dump.bpp` that
   reads only the header + offset/size tables and reports counts.
   Validate it against a **synthetic fixture** of known bytes before
   pointing it at real data. You do not decode a pixel until the
   container is proven. (`vswap_dump`, `war2_dump` are the worked
   examples.)

2. **Pick a strategy.** Does a competent external extractor already
   exist (a `war1tool`-class tool)? If yes, **consume its output**
   (WC1 path — cheapest). If not, or if zero-dep purity matters,
   **write your own decoder** (Wolf3D / WC2 path). The decision is
   per-lane and reversible.

3. **Decode to the canonical shapes.** Sprites → Aseprite sheet.
   Tilesets → atlas_grid. Maps → level JSON. Audio → WAV/MID. Never
   invent a new output shape unless a real consumer needs it
   (Rule 28) — the "Output format contract" below is the target.

4. **Verify visually.** Decode a known asset and *look at it* (the
   `Read` tool renders PNGs). A clean silhouette in the right colours
   is the only proof the bit-math is right. Wrong palette, wrong
   transparency index, raw-vs-RLE confusion all produce
   plausible-looking noise that only the eye catches.

5. **Make the table the source of truth.** A `.sh` driver with an
   `idx → palette → name` table, re-runnable, beats hand-naming
   files. Semantic names where confirmed; index fallback (`unit_NNN`)
   for the rest, renamed later by editing the table.

6. **Write the index map.** A `<game>_index.md` capturing which entry
   is which asset (+ palette pairing) is what makes the lane
   maintainable. Derive it from the source game's own tooling
   (`wartool.h`-class tables) or empirically via a contact sheet.

The single most important habit, across all three lanes: **dump
before decode, and verify by eye.**

---

## Output format contract

Every lane targets these canonical shapes. They are also what Bang 9's
Level Editor and Modulab author, so funnel output and hand-authored
content are byte-compatible. New asset categories land here at the
same time as their first consumer.

### Level JSON

Used by every tile-grid game: pathfind, fps, rts1, rts2. Authored by
Bang 9's Level Editor; loaded at runtime via `bpp_json`. Two modes,
chosen by which top-level field the file carries.

| Mode | Tile semantics | When |
|------|----------------|------|
| **`palette`** | tile id = index into an N-colour palette (default MCU-8 = 8 colours) | schematic / colour-blocked games (pathfind walls, fps tiles) |
| **`tileset`** | tile id = row-major index into a PNG tileset | real tile art (rts1 forest, rts2 WC2 terrain) |

`tileset`-mode example (what `wc1_map_convert` / `war2_pud` emit):

```json
{
  "type": "level",
  "version": 2,
  "width": 96,
  "height": 96,
  "tileset": "summer",
  "tiles": [[112, 112, 114, ...], [80, 93, 79, ...]]
}
```

| Field | Type | Meaning |
|-------|------|---------|
| `type` | string | always `"level"` |
| `version` | int | schema version (current `2`) |
| `width`/`height` | int | grid size in cells |
| `palette` *or* `tileset` | string | exactly one — palette name, or tileset name/path |
| `tiles` | int[][] | row-major grid; `tiles[y][x]` is the cell. Palette mode: index `0..N-1`. Tileset mode: index into the tileset sheet (`0` = empty) |
| `entities` | object[] | optional spawn points / objects (`kind` + position) |

The game pairs `tileset` with its sister tileset PNG (the
`atlas_grid` sidecar below) and binds it via
`stbtile.tile_bind_image` — one shared texture, coalesced into a
single draw call. **Avoid `tile_load_set`** (one texture per tile;
artifacts under smart-bind batching).

MCU-8 palette (palette mode): `0` transparent, `1` black, `2` dark
red, `3` gray, `4` orange, `5` green, `6` purple, `7` yellow.

### Atlas Grid JSON (uniform tile sheets)

For one PNG sliced into uniform tiles, addressed by integer index.
What `vswap_walls` and `war2_tiles` emit; also rts1 unit strips.

```json
{ "type": "atlas_grid", "tile_w": 16, "tile_h": 16, "image": "summer.png" }
```

Runtime slices row-major into `tile_w × tile_h` tiles; frame count =
`(image_w/tile_w) * (image_h/tile_h)`; addressed by index `0..N-1`.
Human names live as `const` in the consuming `.bsm`.

### Aseprite Sprite Sheet JSON (animation metadata)

The canonical format for sprites that carry animation. The runtime
consumes the **literal Aseprite "Export Sprite Sheet → JSON Array"**
shape — what `wc1_sprite_convert`, `vswap_sprites` and `war2_gfx`
emit, and what Aseprite itself exports.

```json
{
  "frames": [
    {"filename": "0", "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
     "rotated": false, "trimmed": false,
     "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
     "sourceSize": {"w": 32, "h": 32}}
  ],
  "meta": {
    "image": "peasant.png",
    "size": {"w": 160, "h": 416},
    "scale": "1",
    "frameTags": [
      {"name": "walk_S", "from": 5, "to": 9, "direction": "forward"}
    ]
  }
}
```

| Field | Meaning |
|-------|---------|
| `frames[]` | per-frame bbox in the PNG: `filename` (id via `image_named_id`), `frame` rect, optional `duration` ms |
| `meta.image` | source PNG, resolved **relative to the JSON's directory** |
| `meta.frameTags[]` | named animation ranges: `name` (via `image_anim_id`), inclusive `from`/`to`, `direction` (only `forward` honoured in v1) |

**Detection:** `image_load` routes a top-level `frames` array with no
`type` field to the Aseprite path. The **Hash** export (frames keyed
by name) is NOT supported — use **Array**.

`war2_gfx` currently emits the `frames[]` grid (index-addressable);
named `frameTags` per animation are the follow-up step, once the
per-unit walk/attack/death frame counts are known.

### Atlas Pack JSON (composed sprites)

For MULTIPLE per-sprite Modulab files composed into a named manifest
(`{ "image", "tw", "th", "frames": [{"id","x","y","w","h"}] }`).
Authored by Modulab's Atlas button. Pick this when each frame is
authored as its own sprite; pick `atlas_grid` when the source is a
pre-baked sheet (funnel output).

### Audio

No JSON envelope: `.wav` (16-bit PCM, via `sound_load_wav`) and
`.mid` (Format 0/1, musical-time division, via `midi_play_file`).
war1tool/wartool emit these directly; the funnel passes them through.

---

## Compatibility rules

- **`version` is mandatory** in every JSON envelope. Current = `2`.
  Bumping means a breaking change; the loader errors clearly rather
  than misreading old fields.
- **Optional fields default sensibly** (`palette`→MCU-8,
  `tile_w`→16, `entities`→empty). Old files keep working.
- **One mode per level** (`palette` XOR `tileset`).
- **Path fields are JSON-relative.** `meta.image` / level `tileset`
  resolve against the directory containing the JSON, not via
  `path_asset`. Siblings ship side-by-side (`peasant.json` +
  `peasant.png`, `image: "peasant.png"`).

---

## How to extend this manual

When a new lane or a new asset shape lands:

1. Add the lane to **The three lanes** (now four) with its commands.
2. If it emits a new shape, add a section under **Output format
   contract** (schema + example + which tool emits it).
3. Update **Tonify Rule 31**'s canonical table and **how_to_dev_b++
   Cap 16** if the convention point changes.
4. Asset shapes graduate into the contract only with two consumers
   (Rule 20); until then they live game-local.

---

## Cross-references

- `docs/manual/tonify_checklist.md` Rule 31 — the discipline rule.
- `docs/manual/how_to_dev_b++.md` Cap 16 "Asset infra".
- `tools/funnel/` — all lanes (`wc1_*`, `vswap_*`, `war2_*`).
- `tools/funnel/war2_maindat_index.md` — WC2 index→asset+palette map.
- `tools/funnel/wc2_handoff.md` — WC2 lane arc trace + open items.
- `tools/level_editor/level_editor_lib.bsm` — consumes level JSON.
- `tools/modulab/modulab_lib.bsm` — authors atlas packs.
- `stb/stbtile.bsm` / `stb/stbimage.bsm` — runtime loaders.
- `src/bpp_json.bsm` — auto-injected JSON reader/writer.
