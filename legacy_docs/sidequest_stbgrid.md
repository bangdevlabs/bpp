# Sidequest — `stbgrid` primitive + stbflow refactor + WC1 collision + fbuf streaming + stbui drift revert

**Status:** CLOSED 2026-05-18 — C1 + C2 + C3 + C4 shipped, docs
landed as Tonify Rule 33 (`docs/manual/tonify_checklist.md`) plus
tier-intro note in `docs/manual/stb++_lib.md`. C0 (fbuf streaming)
skipped without triggering symptoms during the arc; remains a
separate sidequest if a deeper import chain surfaces overflow
again. C5 (level_editor S4 migration ride) not executed — no
`tools/level_editor/level_editor_lib.bsm` modifications were
pending in the working tree at execution time. See commits
`2b7c8d4` → `0a6209d` (5 commits) for the worked-example arc that
Rule 33 references.

**Status (original):** READY-TO-EXECUTE (escrito 2026-05-17).
**Trigger:** WC1 movement needs unit-occupancy grid to prevent peasant stacking. Question surfaced during implementation: "where does the grid live?". Discussion converged on creating `stb/stbgrid.bsm` as agnostic primitive.
**Predecessor context:** earlier in the session we audited stbcol, stbflow, and the broader cartridge ecology. Findings + decisions captured below so the implementing agent doesn't need conversation history.

---

## The question the agent raised

> "But could the WC1 unit-occupancy grid live inside `stbcol` so future programmers have access to it?"

Agent suggested 3 paths: (1) new stbgrid cartridge, (2) extend stbcol, (3) keep local in wc1_movement.

## Why we converged on a NEW `stbgrid` cartridge

Discussion findings (verified against source):

1. **stbcol is leaf-math, must stay leaf-math.** Header literal: "Pure math, zero dependencies. Kept as a standalone leaf module so tests can exercise collision geometry without pulling the full physics/tile/image/render chain." Adding malloc/free + stateful grid breaks that identity.

2. **stbflow already has 3 ad-hoc grids.** `stbflow.bsm` allocates `blocked` (byte array w*h), `dist` (word array w*h), `queue` (word array w*h). Grid storage is being reinvented per-cartridge.

3. **Industry precedent for tile-occupancy is mixed but clear.** General engines (Unity, Unreal, Godot) provide TERRAIN collision only — RTS devs roll their own occupancy. **RTS-specific engines (Stratagus, OpenRA, Spring, 0AD) all have engine-level tile-occupancy primitives** (UnitCache, OccupiedCells, footprints, obstruction manager). B++ has cartridges that lean RTS-friendly (stbflow exists), so this pattern is industry-aligned, not speculation.

4. **User feedback shaped the final design:** cartridges should be **agnostic within their area**, not locked to a genre. A grid is a grid — useful for RTS occupancy, roguelike entity tracking, tower defense placement, Sokoban puzzle state, Snake tail tracking, Tetris-board state, etc. Hardcoding tile-state semantics inside stbflow would prevent reuse for non-RTS grid games.

5. **Anti-speculation guard:** stbgrid is a STORAGE PRIMITIVE. It must not gain semantics ("occupancy", "fog-of-war", "influence map"). Those live in consumer cartridges/games. This is the exact lesson from the recent `stbui v2 ui_image` drift that caused fbuf overflow.

---

## Plan — 5 commits, ordered, bisect-friendly

### Commit 1 — `stb/stbgrid.bsm` (new primitive, ~80 LOC + ~50 LOC smoke)

**Cartridge identity:** generic 2D grid storage primitive. Pure data structure, zero dependencies. Same leaf-module status as stbcol.

**Two variants (mirroring `buf_byte` / `buf_word` pattern in bpp_buf):**

```b++
struct Grid {
    w, h,           // dimensions in cells
    cell_size,      // 1 (byte grid) or 8 (word grid)
    cells           // ptr to w*h*cell_size bytes
}

grid_new_word(w, h)           // allocate w*h*8 bytes, zero-initialized
grid_new_byte(w, h)           // allocate w*h*1 bytes, zero-initialized
grid_free(g)                  // release the allocation, frees struct too

// Bounds-safe accessors. OOB writes are no-ops; OOB reads return 0.
grid_set(g, x, y, val)        // dispatches on g.cell_size
grid_get(g, x, y)             // dispatches on g.cell_size

// Helpers
grid_clear(g)                 // zero all cells
grid_in_bounds(g, x, y)       // 1 if (0 <= x < w) AND (0 <= y < h)
grid_w(g)                     // dimensions accessors
grid_h(g)
```

**Header docstring REQUIRED — anti-speculation guard literal:**

```b++
// stbgrid.bsm — Generic 2D grid storage primitive.
//
// Pure data structure, zero dependencies. Same leaf-module status as
// stbcol — kept primitive so any consumer (RTS unit occupancy,
// roguelike entity tracking, Sokoban puzzle state, Snake tail buffer,
// Tetris board, tower defense placement, etc.) can pull it without
// dragging unrelated semantics.
//
// This cartridge stores BYTES OR WORDS PER CELL. Nothing more. Consumers
// give those bytes/words their own meaning — eid+1 for occupancy, tile
// IDs for terrain, timer ticks for bomb fuses, layer indices for fog of
// war. The grid does not know what is in its cells.
//
// DO NOT add semantic concerns here. If you need "is this cell
// occupied by an enemy faction", "is this cell visible to player N",
// "what tile texture lives here" — those are CONSUMER concerns, not
// grid concerns. Build them on top of stbgrid in the consumer.
//
// This restraint is the lesson from the stbui v2 ui_image drift
// (commit dedfb04, reverted) — speculative widgets on primitive
// cartridges create real architectural pain.
```

**Smoke test:** `tests/test_stbgrid.bpp` (~50 LOC) — exercises grid_new_word/byte, set/get round-trips, OOB bounds-safety, clear, both variants, free.

**Verification:**
```bash
./bpp tests/test_stbgrid.bpp -o /tmp/t_grid
/tmp/t_grid              # expect "OK", exit 0
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
cmp /tmp/bpp_gen1 /tmp/bpp_gen2 && echo "BYTE-STABLE"
bash tests/run_all.sh       # +1 test
bash tests/run_all_c.sh
```

---

### Commit 2 — stbflow refactors to use stbgrid (~60 LOC delta)

**Why bundle this**: stbflow has 3 ad-hoc grids that ARE the existing in-tree consumer of "grid storage" pattern. Bringing it onto stbgrid in the same arc:

- Validates stbgrid's API design against a real second consumer (not just WC1)
- Reduces stbflow LOC (drops manual buf_byte/buf_word + bounds logic)
- Establishes the pattern for future cartridges

**Refactor:**

`stb/stbflow.bsm` — internal storage migrates:

```b++
// BEFORE:
struct FlowField {
    w, h,
    blocked,      // buf_byte(w * h)
    dist,         // buf_word(w * h)
    queue         // buf_word(w * h)
}

flow_new(w, h) {
    auto f: FlowField;
    f.w = w; f.h = h;
    f.blocked = buf_byte(w * h);
    f.dist    = buf_word(w * h);
    f.queue   = buf_word(w * h);
    return f;
}

flow_set_blocked(field, gx, gy, b) {
    if (gx < 0 || gy < 0 || gx >= field.w || gy >= field.h) { return; }
    poke(field.blocked + gy * field.w + gx, b);
}

// (...etc for is_blocked, get_dist, BFS internals...)

// AFTER:
import "stbgrid.bsm";

struct FlowField {
    w, h,
    blocked,      // Grid (byte variant)
    dist,         // Grid (word variant)
    queue         // Grid (word variant)
}

flow_new(w, h) {
    auto f: FlowField;
    f.w = w; f.h = h;
    f.blocked = grid_new_byte(w, h);
    f.dist    = grid_new_word(w, h);
    f.queue   = grid_new_word(w, h);
    return f;
}

flow_set_blocked(field, gx, gy, b) {
    grid_set(field.blocked, gx, gy, b);   // bounds-safe inside grid
}

flow_is_blocked(field, gx, gy) {
    return grid_get(field.blocked, gx, gy);
}

// (...BFS internals use grid_set/grid_get throughout...)
```

**flow_free** drops to `grid_free` per field + frees the FlowField struct.

**Audit needed in `stbflow.bsm`:** every direct poke/peek on the 3 buffers. Replace with grid_set/grid_get. Bounds checks become redundant (grid is bounds-safe) — remove them at call sites.

**No behavioral change.** stbflow's public API (`flow_new`, `flow_set_blocked`, `flow_compute`, `flow_step`, etc.) stays identical. Internal storage swap only.

**Verification:**
- All flow_* tests pass unchanged
- Bootstrap byte-stable
- `tests/test_flow.bpp` (or equivalent) still green

**Risk:** medium. stbflow is hot for any game using crowd movement. Refactor touches BFS internals which are subtle. Validate carefully.

---

### Commit 3 — WC1 unit collision (uses stbgrid + Path.tile_gx/tile_gy)

**Goal:** prevent peasant stacking on convergent goals via tile-claim mechanism (Stratagus / OpenRA pattern).

**Files touched:**

`games/rts1/wc1_render.bsm` — Path component grows + spawn registers tile:

```b++
struct Path {
    mode, buf, count, cursor,
    tile_gx, tile_gy              // CURRENT TILE CLAIM in shared grid
}

ecs_spawn_peasant(world, atlas, gx, gy) {
    // ... existing setup ...
    auto pa: Path;
    pa = ecs_get(world, path_comp, eid);
    pa.buf     = buf_word(MAX_WAYPOINTS * 2);
    pa.count   = 0;
    pa.cursor  = 0;
    pa.mode    = 0;
    pa.tile_gx = gx;
    pa.tile_gy = gy;
    wc1_collision_register(eid, gx, gy);   // NEW
    // ...
}
```

`games/rts1/wc1_movement.bsm` — new collision module + claim-transfer in step:

```b++
import "stbgrid.bsm";

// Module-private occupancy grid. Cells store eid+1 (0 = empty).
// Lifetime: created in wc1_collision_init at game start, freed at
// shutdown. NOT a Grid<Path>; just a Grid that we give occupancy
// semantics in the wrapper helpers below.
extrn _occupied;

void wc1_collision_init(w, h) {
    _occupied = grid_new_word(w, h);
}

void wc1_collision_free() {
    if (_occupied != 0) { grid_free(_occupied); _occupied = 0; }
}

void wc1_collision_register(eid, gx, gy) {
    grid_set(_occupied, gx, gy, eid + 1);
}

void wc1_collision_release(gx, gy) {
    grid_set(_occupied, gx, gy, 0);
}

// Returns 1 if the tile is occupied by SOMEONE ELSE (not eid). Used
// by the step logic so a unit doesn't block ITS OWN claim during the
// transfer window.
wc1_collision_is_blocked_for(eid, gx, gy) {
    auto claim;
    claim = grid_get(_occupied, gx, gy);
    if (claim == 0)        { return 0; }   // empty
    if (claim == eid + 1)  { return 0; }   // same unit's own claim
    return 1;                              // someone else
}
```

`_step_unit` claim-transfer logic — when micro-target tile changes:

```b++
// Inside _step_unit, just before advancing to next waypoint:
auto next_gx, next_gy;
next_gx = waypoint_x(pa, pa.cursor);
next_gy = waypoint_y(pa, pa.cursor);

if (pa.tile_gx != next_gx || pa.tile_gy != next_gy) {
    // Micro-target changed. Check if next tile is free for us.
    if (wc1_collision_is_blocked_for(eid, next_gx, next_gy) != 0) {
        // Blocked. Idle this frame; try again next frame.
        return;
    }
    // Transfer claim atomically: release old, claim new.
    wc1_collision_release(pa.tile_gx, pa.tile_gy);
    wc1_collision_register(eid, next_gx, next_gy);
    pa.tile_gx = next_gx;
    pa.tile_gy = next_gy;
}
// (existing position interpolation continues)
```

`games/rts1/rts.bpp` — wire init + free:

```b++
main() {
    // ... existing setup ...
    map = wc1_map_load(...);
    wc1_collision_init(map.w, map.h);   // NEW
    // ...
    while (!quit) { ... }
    wc1_collision_free();                // NEW
    // ... cleanup ...
}
```

**Verification:**
- Spawn 3 peasants, send them all to the same tile via right-click
- Expected: peasants converge but don't stack — one occupies the goal tile, others queue around it
- Existing single-peasant movement unchanged

**Behavioral test:** add `tests/test_wc1_collision.bpp` or smoke via `./games/rts1/rts` manual verify.

---

### Commit 4 — Revert stbui v2 `ui_image` drift (~30 LOC delta)

**Why:** unblocks fbuf concerns + Rule 23 hygiene. Drift commit dedfb04 added `import "stbimage.bsm"` to stbui core for `ui_image` widget that has ZERO consumers.

**Pre-flight:** confirm zero consumers via grep across games/, tools/, examples/, tests/:
```bash
grep -rn "ui_image\b" /Users/Codes/b++/games /Users/Codes/b++/tools /Users/Codes/b++/examples /Users/Codes/b++/tests
```

Expect: zero hits OR only hits inside stbui.bsm itself (the widget definition). If hits elsewhere, STOP and report — those consumers need to be migrated first (likely via `ui_custom` + consumer-side image draw callback).

**Changes to `stb/stbui.bsm`:**

1. Remove `import "stbimage.bsm";` (line 7)
2. Remove `void ui_image(img, frame_idx)` function (lines 2070-2086)
3. Remove `UI_KIND_IMAGE` const + dispatch case in `ui_render` (~line 2329-2333)
4. Update header docstring if it mentions ui_image as a shipped widget

**Verification:**
- Bootstrap byte-stable
- Suite green (no test should reference ui_image)
- Pathfind / wolf3d / wc1 / platformer all build clean

---

### Commit 5 — Level editor S4 migration ride (already in working tree)

User already has `tools/level_editor/level_editor_lib.bsm` modified with S4 stbui v2 migration. Tool-only change, rides this arc to keep working-tree clean.

If level_editor uses `ui_image` widget specifically, this commit needs Commit 4's removal to happen first (cleanup the unused widget) OR it might fail if it relied on the removed dispatch. Verify before staging.

**Verification:**
- `./bpp tools/level_editor/level_editor.bpp -o tools/level_editor/level_editor` builds clean
- Open in Bang 9 — Level Editor tab still loads + paints tiles

---

## fbuf streaming context (was a separate sidequest, see below)

Earlier in the session we identified that `bpp_import.bsm` uses a fixed 256K stack buffer (fbuf) that can overflow under deep import chains, causing silent heap corruption. The fix involves migrating to **per-file heap allocation** (malloc-per-file, like gcc/clang's standard TU handling).

**That fix is a SEPARATE sidequest** documented in earlier conversation. Estimated ~17 LOC delta, single commit in `bpp_import.bsm`. **It can be sequenced before OR after this arc** — independent.

**Recommendation:** ship fbuf streaming FIRST as its own commit (let's call it C0), since it's compiler infrastructure that unblocks any future cartridge growth. After fbuf C0 lands and is bootstrap-verified, this arc's C1-C5 ship on top.

If implementing this arc without the fbuf fix first, the agent should watch carefully for the SIGSEGV / heap-corruption symptom and prioritize fbuf fix before continuing.

---

## Anti-speculation guards (read before adding ANY extra feature)

This arc has a recent precedent of speculation causing real harm: commit `dedfb04` added `ui_image` widget to stbui v2 with zero consumers, which dragged stbimage as a transitive import, which crashed fbuf via 286K import chain depth.

**DO NOT add to stbgrid:**
- "grid_iter / grid_walk" iterators — until 2 consumers ask
- "grid_fill" mass-set — until consumer asks
- Byte-grid variant with explicit type tagging (just keep cell_size internal)
- Any semantic API ("grid_set_occupant", "grid_is_blocked", "grid_visibility_layer")

**DO NOT add to stbflow during refactor:**
- New features beyond storage-shape change
- Behavioral changes to BFS / flow_step
- Performance "improvements" not tied to a profile

**DO NOT extend stbcol, stbpath, or other primitives:**
- They are leaf-modules. Keep them that way.

**ALWAYS verify** Tonify Rule 20 (two-consumer rule) before adding API surface. If there's only one consumer, defer.

---

## Suggested commit sequence

```
C0 (separate sidequest, optional but recommended FIRST): fbuf streaming
C1: stbgrid.bsm new + tests/test_stbgrid.bpp
C2: stbflow.bsm refactor to use stbgrid
C3: WC1 unit collision (stbgrid consumer #2)
C4: revert stbui v2 ui_image drift (independent, can ship anywhere)
C5: level_editor S4 migration ride
```

Each commit:
- Bootstrap byte-stable (run gen1 vs gen2 diff)
- `bash tests/run_all.sh` green
- `bash tests/run_all_c.sh` green
- Touch only the files listed; no scope creep

If any commit cannot byte-stable, STOP and report. Likely cause: parser/codegen interaction with new struct shapes.

---

## Honest commit messages (one each — no marketing)

**C1:**
```
stbgrid: generic 2D grid storage primitive

New leaf cartridge for w*h cell storage with byte and word variants
(grid_new_byte / grid_new_word + bounds-safe set/get/clear).

Anti-speculation guard in the header: this is a STORAGE primitive.
Semantic concerns (occupancy, fog-of-war, tile classes) live in
consumer cartridges/games, not here. Lesson from the dedfb04 stbui
ui_image drift.

Existing consumer pattern that this consolidates: stbflow has 3
ad-hoc grids (blocked/dist/queue) currently using buf_byte and
buf_word directly. Next commit refactors stbflow to use stbgrid.

Smoke: tests/test_stbgrid.bpp — set/get/clear/bounds for both
variants.
```

**C2:**
```
stbflow: refactor 3 ad-hoc grids to use stbgrid

Replaces direct buf_byte/buf_word + manual bounds checks with
stbgrid handles. No behavioral change — same public API, same BFS
semantics. Validates stbgrid's API against a real second consumer.

LOC delta: -NN +MM. Bounds-check removal at call sites is the main
simplification.
```

**C3:**
```
WC1 unit collision via stbgrid occupancy claim

Adds tile-claim mechanism (Stratagus/OpenRA pattern) to prevent
peasant stacking when convergent goals are commanded. Each Path
component tracks tile_gx/tile_gy as the unit's current claim;
_step_unit transfers the claim atomically when the micro-target
changes; blocked transfers cause the unit to idle one frame and
retry.

Storage: stbgrid word-grid module-private to wc1_movement.bsm,
storing eid+1 (0 = empty). Semantics live in wc1_collision_* wrapper
helpers — stbgrid stays storage-only per its anti-speculation guard.

Manual verify: spawn 3 peasants in forest1.json, right-click them
all to the same goal — peasants converge but don't stack.
```

**C4:**
```
stbui: revert ui_image drift, drop stbimage import

Reverts commit dedfb04 which added ui_image widget + stbimage
import to stbui core. The widget had zero consumers in games/,
tools/, examples/, or tests/. The transitive import dragged
stbimage's ~106KB into every stbgame consumer's fbuf chain,
contributing to overflow on deeper import paths.

Removed: import "stbimage.bsm" (line 7), void ui_image(...) (lines
2070-2086), UI_KIND_IMAGE const + dispatch case (line ~2329).

Tonify Rule 23 hygiene: cartridges ship floor only; widgets requiring
heavy transitive imports earn their slot only with concrete consumers.
```

**C5:**
```
level_editor: complete S4 stbui v2 migration

Tool-only ride along the stbgrid arc. Migrates the tilemap painter
panel from gui_* helpers to the declarative ui_* layout API
introduced in stbui v2.
```

---

## Open question for the implementing agent

The C0 fbuf streaming work was discussed and audited earlier in this session. The audit concluded: per-file malloc with ~17 LOC delta to `bpp_import.bsm`, bringing B++ in line with gcc/clang TU handling.

**Question:** does the implementing agent want to land C0 first as a separate commit (recommended), or push the entire arc and watch for fbuf overflow during C1-C5? Either is valid; C0-first is safer.

The fbuf-streaming fix is documented in conversation but **not as a separate doc file**. If the agent needs the full audit (5 site map of `fbuf + my_file` access points, sys_lseek + malloc + free flow), ask the user to provide it OR re-derive from `bpp_import.bsm` audit. The earlier audit found:

- 5 sites access `fbuf + my_file + X` for content: lines 612, 615 (sys_read), 632 (fnv1a hash), 1110 (peek), 1119 (check_file_import), 1177 (emit_ch)
- All sites use (ptr, len) APIs, agnostic about buffer source
- Solution: malloc(fsize+1) per process_file invocation, free at end
- Path storage (`my_path`, ~50 bytes per file) stays in fbuf — paths are tiny

---

## Final note

The user is delegating implementation to the agent because the design discussion converged. Agent should execute C0 → C5 with bootstrap byte-stability gating between commits. If any surprise (LOC delta drift, byte-stability fail, unexpected consumer of ui_image), pause and report before continuing.
