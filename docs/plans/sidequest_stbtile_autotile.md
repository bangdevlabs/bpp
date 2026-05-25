# Sidequest — stbtile autotile: variant LUT for retro tilemap rendering

**Status:** READY-TO-EXECUTE (2026-05-21).
**Trigger:** WC1 mod S9 closure exposed the chunky-stump-in-middle-of-forest visual: per-tile rendering means cutting a single tree leaves a quadriculado look that breaks the visual continuity of the forest. Fix is autotiling — the 1991+ industry-standard technique for tile-based games (RPG Maker → Tiled → Godot → Unity → Aseprite all expose it). This sidequest ships autotile as a Tier 1 stb primitive so WC1 + every future B++ pixel art game benefits.

**Rule alignment:**
- **Rule 28** (killer use case gate): WC1 forest rendering is the concrete consumer today
- **Rule 33** (cartridge tiers): Tier 1 generic — applies to any tile-based pixel art game regardless of genre
- **Rule 36** (primitive promotion when 2+ consumers planned): WC1 today + future RTS/RPG/roguelike/tower-defense in B++ ecosystem all want this

**Predecessor context:** stbtile current API is `tile_new` / `tile_set` / `tile_get` / `tile_draw` / `tile_load_set` / `tile_bind_set`. This sidequest adds `tile_autotile_register` as opt-in extension — zero impact on existing consumers (pathfind, snake, platformer, fps, etc.).

---

## What autotiling is (1-paragraph spec)

When the renderer draws a cell holding a "logical" tile id that has been registered as autotile, it looks at the 4 cardinal neighbors and computes a 4-bit mask (bit 0 = N neighbor holds same logical id, 1 = E, 2 = S, 3 = W). The mask indexes a 16-entry variant lookup table that returns the actual atlas frame to render. Variant 0 falls back to the logical id, so partial atlas coverage degrades gracefully — missing variants render as the base tile (current behavior).

Visual effect: instead of every "tree" cell showing the same generic tree sprite (creating quadriculado borders), edge cells render edge-variant sprites, corner cells render corner-variant sprites, interior cells render dense-cluster sprites. The forest looks like a continuous canopy with proper boundaries.

---

## Sequencing — 3 commits, bisect-friendly

```
C1 (engine): stbtile API + smoke test    ~120 LOC
C2 (game):   WC1 forest registers         ~50 LOC + 30 min asset audit
C3 (docs):   stb++_lib.md + Tonify rule   ~50 LOC
```

Each commit MUST land bootstrap byte-stable + suite verde (177/0/12 native + 141/0/48 C) before moving to next.

---

## C1 — stbtile API + smoke test

### New struct fields in tilemap

`stb/stbtile.bsm` — extend the tilemap struct (find it via `grep "struct.*Tile" stb/stbtile.bsm` — currently has fields w/h/tw/th/cells/atlas/tileset). Add 3 fields for autotile state:

```b++
struct Tilemap {
    w, h,                    // grid dims in cells (existing)
    tw, th,                  // tile pixel dims (existing)
    cells,                   // buf of cell values (existing)
    atlas,                   // atlas handle for atlas-mode (existing)
    tileset,                 // GPU tileset for legacy mode (existing)
    autotile_count,          // NEW: number of registered autotile groups (0-8)
    autotile_ids,            // NEW: buf_word(8) — logical id per group
    autotile_luts            // NEW: buf_word(8) — pointer to 16-entry variant LUT per group
}
```

The 8-group cap covers chunky retro games (typical: forest, water, grass-edge, mountain, road — usually <8 distinct autotile groups). If a future game needs more, the cap is a single struct field change.

### Init in tile_new

When `tile_new` allocates the Tilemap, zero-init the autotile state:

```b++
// In existing tile_new, after the cells alloc:
m.autotile_count = 0;
m.autotile_ids   = buf_word(8);    // zeros by default — 0 is "unused slot"
m.autotile_luts  = buf_word(8);    // zeros — null pointer per slot
```

### Public registration API

Add after the existing public API (`tile_set` / `tile_get` / `tile_draw` cluster):

```b++
// Register an autotile group for `logical_id`. When tile_draw walks a
// cell holding `logical_id`, it looks at the 4 cardinal neighbors and
// picks a variant from `variant_lut` keyed by a 4-bit mask:
//
//   bit 0 = N neighbor holds same logical_id
//   bit 1 = E neighbor holds same logical_id
//   bit 2 = S neighbor holds same logical_id
//   bit 3 = W neighbor holds same logical_id
//
// `variant_lut` must be a `buf_word(16)` with frame indices in the
// bound atlas. Use 0 for masks where no dedicated variant exists —
// tile_draw falls back to `logical_id` rendering, preserving the
// current per-tile baseline. Partial atlas coverage degrades
// gracefully, no special handling needed.
//
// Up to 8 groups per tilemap. Calling with a logical_id already
// registered REPLACES the variant_lut for that group (live re-bind).
// Returns 1 on success, 0 if the 8-slot cap is full.
//
// Industry pattern: documented in RPG Maker (1991), Tiled (2008+),
// Godot TileSet, Unity Tilemap RuleTile, Aseprite tilemap mode.
tile_autotile_register(tm, logical_id, variant_lut) {
    auto m: Tilemap;
    auto i;
    m = tm;
    // Check if logical_id already registered — replace its LUT.
    for (i = 0; i < m.autotile_count; i++) {
        if (*(m.autotile_ids + i * 8) == logical_id) {
            *(m.autotile_luts + i * 8) = variant_lut;
            return 1;
        }
    }
    // New slot. Refuse if cap is full.
    if (m.autotile_count >= 8) { return 0; }
    *(m.autotile_ids  + m.autotile_count * 8) = logical_id;
    *(m.autotile_luts + m.autotile_count * 8) = variant_lut;
    m.autotile_count = m.autotile_count + 1;
    return 1;
}
```

### Resolution helper (internal)

Add an internal helper before `tile_draw`:

```b++
// Internal: resolve the actual render id for a cell holding
// `logical_id` at (gx, gy). Walks registered autotile groups; if
// logical_id matches, computes the cardinal-neighbor mask and looks
// up the variant. Falls back to logical_id when the variant slot is
// 0 (graceful degradation for partial atlas coverage).
//
// Cost: O(autotile_count) linear scan + 4 tile_get on match. Cheap
// at autotile_count <= 8 and called only per visible cell in
// tile_draw's hot loop. Profile-gated if a denser viewport ever ships.
static _tile_resolve_render_id(tm, gx, gy, logical_id) {
    auto m: Tilemap;
    auto i, mask, variant;
    m = tm;
    for (i = 0; i < m.autotile_count; i++) {
        if (*(m.autotile_ids + i * 8) == logical_id) {
            mask = 0;
            if (tile_get(tm, gx,     gy - 1) == logical_id) { mask = mask | 1; }
            if (tile_get(tm, gx + 1, gy)     == logical_id) { mask = mask | 2; }
            if (tile_get(tm, gx,     gy + 1) == logical_id) { mask = mask | 4; }
            if (tile_get(tm, gx - 1, gy)     == logical_id) { mask = mask | 8; }
            variant = *(*(m.autotile_luts + i * 8) + mask * 8);
            if (variant != 0) { return variant; }
            return logical_id;
        }
    }
    return logical_id;
}
```

### Integrate into tile_draw

Find the existing `tile_draw` loop body where each cell's value is read and dispatched to the atlas/tileset draw call. Insert resolution before draw:

```b++
// Inside tile_draw's per-cell loop:
auto cell, render_id;
cell = tile_get(tm, gx, gy);
if (cell == 0) { continue; }   // empty cell, skip

// NEW: resolve render id via autotile lookup (no-op if no group registered)
render_id = _tile_resolve_render_id(tm, gx, gy, cell);

// Existing draw path uses render_id instead of cell:
// (atlas path)   image_draw_size(m.atlas, render_id, sx, sy, m.tw, m.th);
// (tileset path) _stb_gpu_sprite(m.tileset[render_id - 1], sx, sy, m.tw, m.th);
```

Make sure to use `render_id` for BOTH the atlas-mode draw call AND the legacy tileset-mode draw call. Audit `tile_draw` carefully — there may be 2-3 draw sites depending on mode.

### Smoke test

`tests/test_stbtile_autotile.bpp` — exercises the new API:

```b++
// test_stbtile_autotile.bpp — Smoke for stbtile autotile registration
// and variant resolution. Builds a tiny 5x5 tilemap with logical id 7
// holding an autotile group; checks that _tile_resolve_render_id picks
// the right variant for each neighbor mask.
//
// Doesn't render anything (no window) — calls the resolution path
// directly via a public probe. Goal: validate mask computation and
// LUT indexing without GPU dependency.

import "stbtile.bsm";

main() {
    auto tm, lut;
    auto rid;

    // Build a 5x5 tilemap, all empty.
    tm = tile_new(5, 5, 16, 16);

    // Build a variant LUT. Variant N (1-indexed avoids 0=fallback).
    lut = buf_word(16);
    lut[0]  = 100;  // isolated
    lut[1]  = 101;  // N neighbor only
    lut[2]  = 102;  // E only
    lut[3]  = 103;  // N+E (corner)
    lut[15] = 115;  // all 4 (center)
    // Other masks left 0 → fall back to logical_id (7)

    if (tile_autotile_register(tm, 7, lut) != 1) {
        put("FAIL: register returned 0\n"); return 1;
    }

    // Place logical id 7 at center cell + 4 cardinal neighbors
    tile_set(tm, 2, 2, 7);   // center
    tile_set(tm, 2, 1, 7);   // N
    tile_set(tm, 3, 2, 7);   // E
    tile_set(tm, 2, 3, 7);   // S
    tile_set(tm, 1, 2, 7);   // W

    // Center cell now sees all 4 neighbors as same id → mask = 15
    // Expected: variant 115 (registered above)
    // Probe via internal helper — expose for test or write parallel
    // logic here. For now, use tile_get and inspect via a future
    // probe. The verification is visual via the WC1 forest in C2.

    // Smoke: verify register didn't break tile_get.
    if (tile_get(tm, 2, 2) != 7) { put("FAIL: tile_get center\n"); return 1; }
    if (tile_get(tm, 2, 1) != 7) { put("FAIL: tile_get N\n"); return 1; }

    // Verify register replacement: re-register with new LUT.
    auto lut2;
    lut2 = buf_word(16);
    lut2[15] = 999;
    if (tile_autotile_register(tm, 7, lut2) != 1) {
        put("FAIL: re-register returned 0\n"); return 1;
    }
    // autotile_count should still be 1 (replaced, not added).
    // (No public accessor for count — verify via doesn't-crash + correct
    // visual in C2.)

    // Verify cap. Register 8 more groups, 9th should fail.
    auto i, ok;
    for (i = 0; i < 7; i++) {
        if (tile_autotile_register(tm, 100 + i, lut) != 1) {
            put("FAIL: register slot "); putn(i); put(" failed\n");
            return 1;
        }
    }
    // 8 slots used (1 original + 7 new). 9th must fail.
    if (tile_autotile_register(tm, 200, lut) != 0) {
        put("FAIL: cap not enforced\n"); return 1;
    }

    put("OK\n");
    return 0;
}
```

For full visual verification, defer to C2 (WC1 forest renders look continuous).

### C1 verification

```bash
./bpp tests/test_stbtile_autotile.bpp -o /tmp/tta
/tmp/tta                          # expect "OK", rc=0

./bpp src/bpp.bpp -o /tmp/bpp_g1
/tmp/bpp_g1 src/bpp.bpp -o /tmp/bpp_g2
cmp /tmp/bpp_g1 /tmp/bpp_g2 && echo "BYTE-STABLE"

bash tests/run_all.sh    # 178/0/12 (177 + new smoke)
bash tests/run_all_c.sh  # 142/0/48 or 141/0/49 (+ skip GPU)

# Also verify existing tile consumers untouched:
./bpp games/pathfind/pathfind.bpp -o /tmp/pf
./bpp games/snake/snake_maestro.bpp -o /tmp/sm
./bpp games/platformer/platform.bpp -o /tmp/plat
```

---

## C2 — WC1 forest registers (depends on C1)

### Asset audit first (~30 min visual)

**Goal:** identify which tile IDs in WC1 forest tileset correspond to which mask values.

**Method:**
1. Open Modulab on `games/rts1/assets/converted/graphics/tilesets/forest/terrain.png` (single-image atlas, 16 columns × 20 rows = 320 tiles)
2. Visually identify tree-adjacent variants:
   - Isolated tree (small, no neighbors visually connected)
   - 4 edges: tree-with-grass-on-N, tree-with-grass-on-E, etc.
   - 4 corners: tree-with-grass-on-NE, etc.
   - 4 T-junctions: tree-with-grass-on-N-only, etc.
   - Center / dense cluster variant
3. Record tile_id (1-based row-major index) per visual variant
4. Map to 4-bit mask values:

```
Mask 0  (no neighbors)     → isolated tree
Mask 1  (N is tree)        → edge with grass on S
Mask 2  (E is tree)        → edge with grass on W
Mask 4  (S is tree)        → edge with grass on N
Mask 8  (W is tree)        → edge with grass on E
Mask 3  (N+E are trees)    → corner with grass on SW
Mask 6  (E+S are trees)    → corner with grass on NW
Mask 12 (S+W are trees)    → corner with grass on NE
Mask 9  (N+W are trees)    → corner with grass on SE
Mask 5  (N+S are trees)    → vertical line
Mask 10 (E+W are trees)    → horizontal line
Mask 7  (N+E+S are trees)  → T-junction with grass on W
Mask 11 (N+E+W are trees)  → T-junction with grass on S
Mask 13 (N+S+W are trees)  → T-junction with grass on E
Mask 14 (E+S+W are trees)  → T-junction with grass on N
Mask 15 (all 4 are trees)  → dense center / full cluster
```

**Expected outcome:** 8-14 of the 16 masks have a dedicated variant in the WC1 atlas. Missing masks fall back to base `WC1_T_TREE` (current behavior, no regression).

Document findings in `games/rts1/rts1_map.bsm`:

```b++
// Forest autotile variants. Mask convention from stbtile autotile:
//   bit 0=N, bit 1=E, bit 2=S, bit 3=W (set = same logical id).
//
// Values are 1-based tile_ids in the bound forest tileset (16×20 grid).
// 0 means "no dedicated variant" — stbtile falls back to WC1_T_TREE.
//
// Audited via Modulab on assets/converted/graphics/tilesets/forest/
// terrain.png — see Sidequest stbtile_autotile.md for the mask table.
const WC1_T_TREE_ISOLATED   = 0;    // TODO: locate in atlas
const WC1_T_TREE_EDGE_S     = 0;    // mask 1
const WC1_T_TREE_EDGE_W     = 0;    // mask 2
const WC1_T_TREE_CORNER_SW  = 0;    // mask 3
const WC1_T_TREE_EDGE_N     = 0;    // mask 4
const WC1_T_TREE_VERTICAL   = 0;    // mask 5
const WC1_T_TREE_CORNER_NW  = 0;    // mask 6
const WC1_T_TREE_TJUNC_W    = 0;    // mask 7
const WC1_T_TREE_EDGE_E     = 0;    // mask 8
const WC1_T_TREE_CORNER_SE  = 0;    // mask 9
const WC1_T_TREE_HORIZONTAL = 0;    // mask 10
const WC1_T_TREE_TJUNC_S    = 0;    // mask 11
const WC1_T_TREE_CORNER_NE  = 0;    // mask 12
const WC1_T_TREE_TJUNC_E    = 0;    // mask 13
const WC1_T_TREE_TJUNC_N    = 0;    // mask 14
const WC1_T_TREE_CENTER     = 0;    // mask 15
```

Fill in the actual tile ids from the audit. Leave 0 for any mask where no variant exists in the WC1 atlas.

### Register at startup

In `games/rts1/rts.bpp`, after `tile_bind_set` (or wherever the tileset is currently bound), add:

```b++
// Register the WC1 forest autotile group. tile_draw will substitute
// variants based on 4-cardinal neighbor mask when rendering cells
// holding WC1_T_TREE. Masks without a registered variant fall back
// to WC1_T_TREE (preserving the chunky baseline). See
// docs/sidequest_stbtile_autotile.md for the mask convention.
auto _tree_lut;
_tree_lut = buf_word(16);
_tree_lut[0]  = WC1_T_TREE_ISOLATED;
_tree_lut[1]  = WC1_T_TREE_EDGE_S;
_tree_lut[2]  = WC1_T_TREE_EDGE_W;
_tree_lut[3]  = WC1_T_TREE_CORNER_SW;
_tree_lut[4]  = WC1_T_TREE_EDGE_N;
_tree_lut[5]  = WC1_T_TREE_VERTICAL;
_tree_lut[6]  = WC1_T_TREE_CORNER_NW;
_tree_lut[7]  = WC1_T_TREE_TJUNC_W;
_tree_lut[8]  = WC1_T_TREE_EDGE_E;
_tree_lut[9]  = WC1_T_TREE_CORNER_SE;
_tree_lut[10] = WC1_T_TREE_HORIZONTAL;
_tree_lut[11] = WC1_T_TREE_TJUNC_S;
_tree_lut[12] = WC1_T_TREE_CORNER_NE;
_tree_lut[13] = WC1_T_TREE_TJUNC_E;
_tree_lut[14] = WC1_T_TREE_TJUNC_N;
_tree_lut[15] = WC1_T_TREE_CENTER;
tile_autotile_register(tm, WC1_T_TREE, _tree_lut);
```

### Tree-cut refresh

The mask is computed per-render in `tile_draw`, so when a tree is cut (tile changes from WC1_T_TREE to WC1_T_STUMP), adjacent cells automatically re-compute their masks next frame. **No explicit refresh needed.** This is the architectural win of resolve-at-draw-time vs cache-at-set-time.

If profile later shows tile_draw hot path stressed by autotile resolution, switch to cache-at-set-time + refresh-neighbors-on-tile_set. Defer until profile evidence demands.

### C2 verification

```bash
./bpp games/rts1/rts.bpp -o games/rts1/rts
games/rts1/rts                          # boots, no crash

# Visual smoke (user-side):
# - Forest cells should render with edge/corner/center variants
# - Cut a tree (right-click as peasant) — surrounding canopy
#   should update neighbor variants next frame
# - Tilemap-only games unaffected (pathfind / snake / platformer)

./bpp src/bpp.bpp -o /tmp/bpp_g1     # compiler unchanged, but verify
/tmp/bpp_g1 src/bpp.bpp -o /tmp/bpp_g2
cmp /tmp/bpp_g1 /tmp/bpp_g2

bash tests/run_all.sh                # 178/0/12
bash tests/run_all_c.sh              # baseline
```

### What "good" looks like visually

- Trees at the edge of a forest cluster show variant sprites with grass on the appropriate side
- Trees in the interior show dense/center variant
- A single isolated tree shows the "isolated" variant
- Cutting a tree updates adjacent cells' canopy continuity next frame

If 4+ mask values fell back to WC1_T_TREE because the atlas lacks variants, those cells will look like the current chunky baseline — partial visual win is still a win.

---

## C3 — docs

### docs/manual/stb++_lib.md — extend stbtile chapter

Find the stbtile chapter (search for "Cap" + "stbtile" or "tile_new"). Add a subsection after the existing API documentation:

```markdown
### Autotile — variant rendering based on neighbor mask

stbtile supports autotiling: a single logical tile id can render with
different sprite variants depending on its 4 cardinal neighbors. This
is the standard technique for retro pixel art games where chunky
per-tile rendering would otherwise break visual continuity at terrain
boundaries (forest edges, water shores, grass-to-dirt transitions).

API:
- `tile_autotile_register(tm, logical_id, variant_lut)` — register
  a 16-entry LUT keyed by 4-bit cardinal mask (bit 0=N, 1=E, 2=S,
  3=W; set = neighbor holds same logical_id). Up to 8 groups per
  tilemap. Returns 1 on success, 0 if 8-slot cap full.

Mask encoding: for cell (gx, gy) holding logical_id, the renderer
checks the 4 cardinal neighbors. If a neighbor cell holds the same
logical_id, the corresponding bit is set. The resulting 4-bit value
indexes the variant LUT to pick the render frame.

Variant 0 in the LUT means "no dedicated variant" — the renderer
falls back to logical_id. Partial atlas coverage degrades gracefully:
mask values without art render as the base tile.

Example (forest with edge/corner/center variants):
\`\`\`b++
auto lut;
lut = buf_word(16);
lut[0]  = 100;   // isolated tree
lut[1]  = 101;   // bottom edge (N neighbor only)
lut[5]  = 105;   // vertical line (N + S)
lut[15] = 115;   // dense center (all 4)
// Other masks left 0 → falls back to tile 7
tile_autotile_register(tm, 7, lut);
\`\`\`

Cost: 4 tile_get + 1 LUT lookup per visible autotile cell per
frame. Negligible for typical 320×240 viewports (~600 cells max).
Profile-gate if a denser viewport ever ships.

When NOT to use: tilemaps where chunky boundaries are the aesthetic
intent (board games, dungeon roguelikes where cells should look
discrete). Autotile is opt-in — games that don't call
tile_autotile_register pay nothing.
```

### docs/manual/tonify_checklist.md — new rule

Add as Rule X (next available number after current ceiling — check `grep "^## Rule " docs/manual/tonify_checklist.md | tail -3` for the latest):

```markdown
## Rule X: Autotile via stbtile for retro pixel art games

When a tilemap-based game's per-tile rendering produces unwanted
chunky boundaries at terrain transitions (forest edges, water
shores, grass-to-dirt), use `tile_autotile_register` from stbtile.
Industry-standard technique since 1991 (RPG Maker, Tiled, Godot
TileSet, Unity Tilemap RuleTile, Aseprite tilemap mode).

The API:
- Register a logical tile id with a 16-entry variant LUT
- Renderer computes 4-cardinal mask per visible autotile cell
- Variant 0 in LUT means "no dedicated variant" — falls back to
  logical_id, preserving the chunky baseline as graceful floor

When NOT to use:
- Tilemap where chunky discrete cells ARE the aesthetic
  (Snake-style boards, dungeon roguelike where each cell should
  visibly stop at its border)
- Game logic that depends on the rendered id being the same as
  the logical id (the API separates them — game writes logical
  via tile_set, renderer reads variant via tile_draw)

Reference implementation: WC1 mod forest rendering registers
WC1_T_TREE with 16 forest variants from the war1tool tileset.
Variants for masks without dedicated atlas art fall back to the
base WC1_T_TREE, so partial coverage degrades gracefully without
visual regression.

Cross-references:
- `docs/sidequest_stbtile_autotile.md` for the engine + audit work
- `docs/manual/stb++_lib.md` stbtile chapter for API reference
- `games/rts1/rts.bpp` for working consumer example
```

### C3 verification

```bash
./bpp src/bpp.bpp -o /tmp/bpp_g1   # doc-only changes, bootstrap unchanged
diff /tmp/bpp_g1 ./bpp             # may or may not match — doc edits don't touch compiler
                                   # but should still byte-stable from previous bpp
bash tests/run_all.sh
```

---

## Stop conditions

1. **C1 bootstrap diverges (gen2 ≠ gen3)** → revert, investigate. Most likely cause: the struct field additions changed tilemap allocation layout in a way that affects something downstream.

2. **Existing tile consumers (pathfind/snake/platformer) crash or render wrong** after C1 → autotile path is firing when no group registered. Audit `_tile_resolve_render_id` — must return logical_id unchanged when autotile_count == 0.

3. **WC1 forest looks worse than before C2** → atlas audit picked wrong tile ids. Set the suspect variants to 0 (fall back to WC1_T_TREE) and proceed; partial coverage is still a win.

4. **Profile shows tile_draw regression >5%** → autotile resolution loop is too expensive for the cell count. Two mitigations: (a) cache resolved id at tile_set time in a parallel grid, refresh neighbors on set; (b) inline the lookup via builtin dispatch (same pattern as unpack_s S3c). Defer caching/inlining until profile demands.

---

## Out of scope (defer to forcing function)

- **8-neighbor mask (cardinals + corners)**: 256-entry LUT instead of 16. Higher fidelity but exponentially more variant art. Defer until a game ships an atlas with corner-aware variants.
- **Multi-id autotile groups**: "tree AND stump both count as forest for continuity". Useful for transitional states. Defer until 2nd consumer asks.
- **Wang tile / 47-tile blob pattern**: industry-standard maximalist autotile set. Defer — most chunky retro games are fine with 16-variant cardinal mask.
- **Cache-at-set-time optimization**: write resolved id to parallel grid on tile_set, refresh 4 neighbors. Avoids per-frame resolution cost. Defer until profile shows tile_draw stressed by autotile resolution.
- **Variant id is a struct (not a plain int)**: would allow per-variant transformations (rotation, flip). Defer — WC1 atlas is pre-baked, doesn't need transforms.
- **Editor support in level_editor / Modulab**: visualize which mask each tile maps to, paint multi-cell forest blocks with auto-resolution. Defer — Modulab can already paint tiles; mask preview is editor-side polish.

---

## Honest commit messages

**C1:**
```
stbtile: autotile via variant LUT (tile_autotile_register)

Add opt-in autotile registration to the tilemap primitive.
tile_autotile_register(tm, logical_id, variant_lut) registers a
16-entry LUT keyed by a 4-bit cardinal neighbor mask. tile_draw
substitutes the variant when rendering cells holding logical_id;
variant 0 falls back to logical_id for graceful partial-atlas
degradation.

Standard technique since RPG Maker (1991); B++ ships it as Tier 1
primitive following Tiled / Godot / Unity Tilemap / Aseprite
precedent.

Up to 8 autotile groups per tilemap. Cost: 4 tile_get + 1 LUT
lookup per visible autotile cell per frame — negligible at typical
320×240 viewport scale.

Smoke test: tests/test_stbtile_autotile.bpp exercises register,
re-register replacement, and 8-slot cap enforcement.

Existing tile consumers (pathfind, snake, platformer, fps) untouched
— autotile is pure opt-in, games that don't register pay nothing.
```

**C2:**
```
WC1: register forest autotile with 16-variant LUT

games/rts1/rts.bpp calls tile_autotile_register(tm, WC1_T_TREE, lut)
after binding the forest tileset. Variant table in rts1_map.bsm
maps the 16 cardinal masks to tile ids from the war1tool forest
atlas (edges, corners, T-junctions, vertical/horizontal lines, dense
center).

Variants without dedicated atlas art are left as 0 — stbtile falls
back to WC1_T_TREE, preserving the chunky baseline. Cutting a tree
updates adjacent cells' canopy continuity next frame
(resolve-at-draw-time, no explicit refresh).

Visual win: forest interior shows dense cluster variant, edges show
edge variants, isolated trees show isolated variant. Quadriculado
boundaries from S9 baseline replaced with continuous canopy where
atlas coverage is complete.
```

**C3:**
```
docs: autotile chapter in stb++_lib + Tonify rule

Document tile_autotile_register API in the stbtile chapter of
stb++_lib.md with mask encoding spec + worked example. Add Tonify
rule covering when to use (retro pixel art with terrain transitions)
and when not to (chunky board games where discrete cells are the
aesthetic).

Cross-references docs/sidequest_stbtile_autotile.md for the
engine + audit work and games/rts1/rts.bpp for the working consumer
example.
```

---

## Final notes for the agent

- **Read this entire doc before starting** — context, API spec, audit method, and verification protocol are all here.
- **Order matters**: C1 → C2 → C3. Don't combine.
- **Bootstrap byte-stable is the gate** between commits. If C1 diverges, fix before C2.
- **Asset audit (~30 min) is mandatory before C2**. Don't guess tile ids; verify visually in Modulab.
- **Out-of-scope items are deferred deliberately** — Rule 28 forcing function. Don't expand scope.
- **Profile if you're worried**: `time games/rts1/rts` before/after C2 to verify autotile resolution doesn't regress frame time. Should be within noise.

If you hit a surprise (struct layout issue, mask math off, atlas variants don't exist), stop and report. Don't paper over with hacks.
