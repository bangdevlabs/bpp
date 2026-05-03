# Wolf3D — Handoff Report for Phase 1 Implementation

**Date:** 2026-05-02
**Author:** Claude (preceding session)
**Recipient:** Emacs agent picking up Phase 1 implementation
**Tree state at handoff:** Phase 6 (panic + profiler) closed, suite native 126/0/12 + C 105/0/33, byte-stable bootstrap.

---

## TL;DR

Phase 1 of B++'s Wolf3D port is now scaffolded but not implemented.
This handoff gives the next agent everything needed to start Session 1
without re-deriving context. Three skeleton files are in place; a
detailed 6-session plan covers Phase 1 from blank screen to playable
textured maze with WASD movement.

The strategic value: Wolf3D is the **first real workload** for the
Phase 6 profiler. Everything before this (snake, rhythm teacher) was
too simple to surface optimization questions. Wolf3D's ray caster has
a genuine hot loop (38400 rays/sec × ~30 float ops each), texture
sampling that stresses cache behaviour, and column-parallel work that
exercises the auto-dispatch. Implementing it answers the deferred
question "does B++ need Tier F compiler optimization, and if so where?"

---

## Architectural decision: stbraycast composes existing stb cartridges

The first scaffold drafts went through three iterations before the
right shape emerged:

**Draft 1**: `raycast.bsm` and `render.bsm` inside `games/wolf3d/`.
WRONG — DDA algorithm is generic, belongs in stb (Layer 2).

**Draft 2**: `stb/stbraycast.bsm` reinventing map storage + colour
palette internally. WRONG — duplicates the `Tilemap` struct (from
`stbtile.bsm`) and the `Palette` struct (from `stbpal.bsm`), both of
which already exist and are reusable.

> **Notation note for new agents:** B++ has a single flat namespace —
> all imported symbols live in one global pool, no per-file
> qualification. When this document mentions something like "the
> `Palette` struct (from `stbpal.bsm`)", that parenthetical is
> documentation prose for cross-file traceability, NOT syntax. In
> code you just write `Palette`, never `stbpal.Palette`.

**Draft 3 (current)**: `stb/stbraycast.bsm` is minimal and composes
existing cartridges:

| Cartridge | Role in raycast | Pre-existing |
|---|---|---|
| `stbtile.bsm` | Map storage (Tilemap struct, tile_set/get, solid_mask) | yes |
| `stbpal.bsm` | Wall colour palette (PAL_DB_32, PAL_PICO_8, etc.) | yes |
| `stbasset.bsm` | PNG texture loading (Phase 2) | yes |
| `stbrender.bsm` | Pixel writes via render_vertical_strip / render_textured_strip | yes (extended this session) |
| `stbraycast.bsm` | DDA + RayHit + projection (~150 LOC final) | NEW |

What stbraycast OWNS exclusively:
- DDA loop math (cast_ray)
- RayHit struct (compact, sub-word slices)
- Projection math (raycast_draw_column)

What stbraycast DELEGATES:
- Map → `Tilemap` (from stbtile); cast_ray accepts a Tilemap pointer
- Colours → `Palette` (from stbpal); raycast_draw_column accepts a
  Palette pointer
- Pixel writes → `render_vertical_strip` / `render_textured_strip`
  (from stbrender)
- Textures → stbasset (Phase 2)

What stays in `games/fps/wolf3d.bpp`:
- Maestro phase wiring (init/solo/base/render/quit)
- Tilemap content (which cells are walls — the level layout)
- Palette choice (which built-in or custom palette)
- Player state (position, angle, FOV — too FPS-specific for stb)
- Input mapping (Session 3)
- Asset paths (Phase 2)

### Player state strategy: stbphys grows a chapter, not a sibling file

Today's `stbphys.bsm` has a single `Body` struct that's
**platformer-oriented**: gravity, jump, on_ground flag, axis-aligned
hitbox. That doesn't fit the FPS player (rotation as primary input,
no gravity, no jump, circular hitbox for wall sliding).

Two ways to evolve:

- **Multiple files**: `stbphys_platformer.bsm`, `stbphys_fps.bsm`,
  `stbphys_topdown.bsm`. Cleaner per-file but more imports and a
  naming convention that doesn't exist yet.
- **Chapters in one file**: keep `stbphys.bsm`, add sectioned
  bodies (PlatformerBody / FPSBody / TopdownBody) with shared
  primitives at the top. Single mental model, easy to find, splits
  organically when the file passes ~1500 LOC.

**Decision: chapters in one file, until size dictates a split.**

For Phase 1 Session 3 (player movement), the right move is to add
a Chapter to stbphys:

```bpp
// stbphys.bsm — append after the existing platformer Body section.

// ── Chapter: FPS (rotation + grid collision) ────────────────
struct FPSBody {
    x: float, y: float,         // world position in cell units
    angle: float,               // facing radians
    radius: float,              // circular hitbox for wall sliding
    move_spd: float,            // forward/strafe speed
    turn_spd: float             // angular speed
}

void fps_step(body: FPSBody, dt: float, tm) { ... }
void fps_walk(body: FPSBody, forward: float, strafe: float, dt: float, tm) { ... }
void fps_turn(body: FPSBody, da: float) { ... }
```

**Status:** the FPS chapter is already in place. `stb/stbphys.bsm`
ships `FPSBody` + `fps_body` (constructor) + `fps_walk` / `fps_turn`
stubs (Session 3 fills the bodies). `games/fps/wolf3d.bpp` already
declares `extrn player;`, allocates it via `fps_body(8.0, 8.0, 0.0,
0.3, 3.0, 2.0)` in the init phase, and reads `p.x` / `p.y` /
`p.angle` in the render phase. Session 3 just wires input keys into
`fps_walk` / `fps_turn`.

The shared physics primitives (`rect_overlap` from `stbcol`,
integrate_position) stay in stbphys's core chapter; each
game-shape chapter consumes them.

### Why the line count drops

With reuse instead of reinvention, wolf3d.bpp drops from the original
700-1000 LOC estimate to ~400-600 LOC final. Reuse kills 30-40% of
the code Phase 1 would have written from scratch. This is the
killer-feature payoff of tight-bound stb cartridges — every new
game pays back the investment of the previous ones.

## Files already in place

```
stb/stbrender.bsm            — EXTENDED this session.
                               Two new generic primitives any
                               column-based renderer can use:
                               render_vertical_strip(col, y_start, y_end, color)
                               render_textured_strip(col, y_start, y_end,
                                                     tex, tex_x, tex_y_step)

stb/stbraycast.bsm           — NEW Layer 2 cartridge.
                               RayHit struct (distance:half float, tex_x:half,
                               wall_type:byte, side:bit).
                               cast_ray(hit, col, screen_w, px, py, pang, fov, tm)
                                 — fills caller-allocated RayHit. Reads tm via
                                   tile_get / tile_is_solid. DDA TODO Session 1.
                               raycast_draw_column(hit, col, sw, sh, pal)
                                 — projects + flat-fills via render_vertical_strip,
                                   colour from palette_get(pal, hit.wall_type).
                                   Body TODO Session 1, textured swap Session 2.

games/fps/
  wolf3d.bpp       — Entry point. Imports stbgame + stbtile + stbpal +
                     stbrender + stbasset + stbraycast. maestro_register_*
                     bindings + game_run() open the window.

                     Phases:
                       wolf3d_init_phase (@io)    — allocate Tilemap,
                                                    pick palette, alloc
                                                    RayHit buffer, set
                                                    player position
                       wolf3d_solo_phase (@solo)  — input → player
                                                    (Session 3 TODO)
                       wolf3d_base_phase (@base)  — parallel work (Phase 2)
                       wolf3d_render_phase (@gpu) — per-column dispatch
                                                    into stbraycast
                       wolf3d_quit_phase (@io)    — cleanup

                     Globals:
                       FOV (float, ≈1.0472 = 60°)
                       player (FPSBody*, allocated by fps_body in init)
                       world_map (Tilemap*)
                       wall_palette (Palette*)
                       ray_hit (RayHit buffer, allocated once at init)

                     Stubs:
                       _wolf3d_init_map  — Session 1 implements with
                                           tile_set + tile_solid loop
                                           over the layout in the comment
                       _wolf3d_load_textures — Phase 2

  assets/          — Empty. Reserved for Phase 2 wall textures.
  HANDOFF.md       — This file.
```

The smoke test passes: `./bpp games/fps/wolf3d.bpp -o /tmp/wolf3d`
compiles cleanly with one pre-existing W026 warning from
`stb/stbsound.bsm` (unrelated, ours-untouched). The binary opens a
window and runs the maestro loop with empty render — black screen.
Session 1 is the first session that produces visual output.

Each file follows tonify discipline from line 1:
- `global` for cross-callback mutable state, annotated `: float`
  where they hold floats (FOV, plus the float fields inside FPSBody)
- `extrn` for write-once-after-init data (`player`, `world_map`,
  `wall_palette`, `ray_hit`)
- `const` for compile-time literals (SCREEN_W, FPS, N_WORKERS)
- `static` on every callback (file-private — only `main` calls them)
- All callbacks have phase annotations matching their semantics:
  init/quit @io, solo @solo, base @base, render @gpu
- Cast_ray is `@io` (reads map via peek); cast_and_draw_column is
  `@gpu` (writes pixels)
- `: float` on float locals/parameters everywhere
- `: half` / `: byte` slice annotations on RayHit struct fields for
  compact storage
- TODO markers calling out which session implements each piece

### Maestro API used (matches snake_maestro.bpp pattern)

```
maestro_set_workers(N)               // worker thread count
maestro_register_init(fn_ptr(...))   // one-shot
maestro_register_solo(fn_ptr(...))   // each tick, main thread
maestro_register_base(fn_ptr(...))   // each tick, parallelisable
maestro_register_render(fn_ptr(...)) // each frame, GPU
maestro_register_quit(fn_ptr(...))   // shutdown
game_run(W, H, "Title", FPS)         // opens window + runs loop
```

Do NOT use the `stbgame_run(init, step, render)` shape — that signature
doesn't exist in the current stbgame. The maestro_register_* / game_run
combo is the canonical entry point.

---

## Session-by-session plan

### Session 1 — DDA + flat walls (no textures, no movement)

**Goal:** open window, see flat-coloured walls in correct 3D
projection. Player static. ~250 LOC.

**Files to fill in:**
- `stb/stbraycast.bsm` — `cast_ray()` body. Lode tutorial section
  "DDA" is the exact algorithm. Walk integer grid steps until the
  map cell at `peek(map + map_y * map_w + map_x)` is non-zero.
  Compute perpendicular distance (avoid fish-eye effect by
  projecting onto view direction, not raw ray length). Return
  RayHit populated.
- `stb/stbraycast.bsm` — `raycast_draw_column()` body. Compute
  `line_height = screen_h / hit.distance`, clamp to screen_h,
  derive `draw_start = (screen_h - line_height) / 2`, paint vertical
  span with a colour keyed off `hit.wall_type` (e.g. type 1 = light
  grey, type 2 = mid grey). Use `render_rect(col, y, 1, height,
  color)` for the column — it's the simplest stbrender primitive
  that works (Phase 1 doesn't need a dedicated vertical-strip
  primitive yet).
- `games/fps/wolf3d.bpp::_wolf3d_init_map` — populate `world_map`
  via `tile_set` + `tile_solid` calls following the hardcoded
  layout in the file's comment (16x16 maze).

**Verification:**
```bash
./bpp games/fps/wolf3d.bpp -o /tmp/wolf3d
/tmp/wolf3d
# Expected: window opens, see grey walls in foreshortened 3D from
# player position (8.0, 8.0) looking +x. Player doesn't move.
# 60fps rock-solid (workload is tiny).
```

**Profile pass:**
```bash
# Modify wolf3d.bpp briefly to call sys_profile_start at frame 0,
# stop at frame 600 (10 sec), dump top-N. Expected dominant samples:
#   - cast_ray (or whatever its codegen synth name is)
#   - cast_and_draw_column
#   - layer_set / draw primitives
# If sin/cos appear in top 5, plan native sin/cos primitive for
# Session 5 optimization pass. If sqrt appears, same.
```

### Session 2 — Texture mapping

**Goal:** walls show 64×64 textures sampled per column. Same maze,
recognisable wall surfaces. ~150 LOC.

**Files added/modified:**
- `stb/stbrender.bsm` — extend with two generic column-drawing
  primitives that benefit any column-based renderer (not just
  raycasters):
    `render_vertical_strip(col, y_start, y_end, color) @gpu`
    `render_textured_strip(col, y_start, y_end, tex, tex_x, tex_y_step: float) @gpu`
  These are GENERIC (per the Six-Layer Cake rule — useful for many
  programs → Layer 2). 2D parallax scrollers, heatmaps, custom
  column visualizers all benefit. Adding them to stbrender keeps
  the surface area honest: any future ray-cast game (DOOM clone,
  maze explorer) imports the same primitives, no copy-paste.
- `stb/stbraycast.bsm` — `raycast_draw_column()` body switches
  from `render_rect(col, y, 1, h, color)` (Phase 1 flat fill) to
  `render_textured_strip(col, y_start, y_end, tex, hit.tex_x, step)`
  (Phase 2 textured).
- `games/fps/textures.bsm` (new, game-local) — load 4-8 PNG textures
  via stbasset, store as 64×64 RGBA layers. Game-specific because
  the texture set IS the game's identity. Public API
  `texture_sample(tex_id, tex_x, tex_y)` returns RGBA pixel.
- `games/fps/wolf3d.bpp::_wolf3d_load_textures` — wire textures.bsm
  calls.

**Asset sourcing:** download free wall textures from kenney.nl
(opengameart.org also works) sized 64×64. Place under
`games/fps/assets/` as `wall_brick.png`, `wall_stone.png`,
`wall_wood.png`, etc. Or hand-paint in any image editor — 64×64
RGBA is small enough that 5 minutes per texture is fine.

The id Software original Wolf3D shareware textures (VSWAP.WL1) are
gratis-redistributable but require a parser for the 1992 disk
format. Use modern PNGs for Session 2 simplicity. Authentic id
textures can be a Phase 2 stretch goal ("retro mode").

### Session 3 — Player movement + input (fill stbphys FPS chapter)

**Goal:** WASD walks, arrow keys turn, collision detection prevents
walking through walls. ~100 LOC, but ~60 of those land in `stbphys`
(reusable) and ~40 in `wolf3d.bpp` (game-specific input wiring).

The FPS chapter scaffold is already in `stb/stbphys.bsm` (struct
`FPSBody`, constructor `fps_body`, stubs for `fps_walk` and
`fps_turn`). Session 3 fills the stub bodies and wires input.

**Files modified:**
- `stb/stbphys.bsm` — implement `fps_walk` and `fps_turn`:
  ```bpp
  void fps_walk(body, forward: float, strafe: float,
                dt: float, tm) @base { ... }
  void fps_turn(body, da: float, dt: float) @base { ... }
  ```
  Reuse `rect_overlap` from `stbcol` for collision (or implement
  circle-vs-grid sliding directly — circle-on-axis-aligned-cell is
  a 4-line check).

- `games/fps/wolf3d.bpp` — solo phase reads input (`key_pressed` for
  WASD + arrows) and calls `fps_walk` / `fps_turn` against the
  already-allocated `player`. Tilemap is passed for collision.

**Collision (inside fps_walk):**
- Compute candidate (new_x, new_y) from forward + strafe + angle.
- For each axis independently: check `tile_is_solid(tm,
  tile_get(tm, floor(new_x), floor(player.y)))`. If solid, reject
  X movement only. Same for Y. Sliding falls out naturally.
- Optional: subtract `body.radius` from movement to keep the
  player out of corners.

**Why stbphys gets a chapter (not a sibling file or game-local code):**
See "Player state strategy: stbphys grows a chapter, not a sibling
file" section above. Bottom line: when the second 2.5D shooter
appears, it imports stbphys, gets fps_walk + fps_turn for free,
zero copy-paste.

### Session 4 — Multi-wall map + ASCII map loader

**Goal:** load level from `assets/wolf3d/maps/level1.txt` instead
of hardcoded layout. Multiple wall types per map. ~100 LOC.

**Files added/modified:**
- `map.bsm` (new) — `load_map(path)` reads ASCII grid, parses
  characters to wall type ids. `'#'` = type 1, `'@'` = type 2, etc.
  Spaces / dots = empty.
- `wolf3d.bpp::_wolf3d_init_map` — call `load_map("assets/wolf3d/maps/level1.txt")`.
- `assets/wolf3d/maps/level1.txt` (new) — 16×16 ASCII map.

### Session 5 — Profile + optimize

**Goal:** confirm 60fps in M4, identify bottlenecks if any. ~50 LOC
optimization + report. Decision point for opening Tier F.

**Approach:**
- Run Phase 6 cooperative profiler around 600 frames of gameplay
  (player auto-walks via test harness or manual play).
- Dump top-20 functions by sample count.
- If `cast_ray` is dominant and float math is hot path, consider:
  - SIMD parallelism via `vec_*` primitives (4 rays at once)
  - Native sin/cos primitives if Taylor series shows up high
  - Loop CSE if same expression recomputed per ray (Tier F start)
- If GPU upload dominates, that's stbrender / Cocoa territory, not
  compiler.
- If the profile is flat (no clear dominant), Phase 1 is shipped —
  no optimization needed, move to Phase 2 or Sprint 5.

**Output:** journal entry summarising findings + decision on whether
Tier F gets opened next.

### Session 6 (optional) — Polish + tonify pass

**Goal:** apply tonify_checklist to all wolf3d/ files, doc updates,
thematic commit series. ~50 LOC cleanup.

---

## Reference materials

### Algorithm

**Lode Vandevenne's raycasting tutorial — lodev.org/cgtutor/raycasting.html**

Sections needed for Phase 1:
- Part 1 "Raycasting" — DDA explained step by step, ~30 lines of
  pseudocode that translate directly to B++.
- Part 2 "Textured raycaster" — texture x derivation from wall hit
  point, vertical scaling math.

Skip for Phase 1 (Phase 2+):
- Part 3 "Sprites" — billboarding, depth sort.
- Part 4 "Doors" — animated map cells.
- Part 5 "Floor + ceiling" — two extra ray passes; Phase 2 stretch.

### Original source (historical reference, NOT for line-by-line port)

**github.com/id-Software/wolf3d** — id Software's 1995 GPL release.
30K+ lines of C with 1992 DOS-isms (segmented memory, soft blitting,
hand-tuned x86 asm). Useful for understanding decisions but NOT the
right reference for a clean modern port. Lode's tutorial is the
canonical source for that.

### Assets

- **Free game-ready textures:** kenney.nl (CC0 license, no
  attribution required), opengameart.org (mixed licenses, check each).
- **Wolf3D shareware files:** legally redistributable but need a
  decoder for VSWAP.WL1. Skip for Phase 1; consider for "retro
  authenticity" Phase 2 mode.

---

## Architectural risks to watch

### 1. Float annotation discipline (Tonify Rule 12)

Ray casting is float-heavy. Every local that holds a float MUST be
annotated `: float` or it silently degrades to int. The pattern:

```bpp
// WRONG — silent truncation, ray cast produces garbage:
auto distance;
distance = sqrt(dx * dx + dy * dy);

// RIGHT — keeps IEEE bits:
auto distance: float;
distance = sqrt(dx * dx + dy * dy);
```

This is the #1 silent failure mode of ray cast in B++. Every helper
in `raycast.bsm` and `render.bsm` should be reviewed for float
annotations on every local.

### 2. sin/cos cost (probably non-issue, profile to confirm)

`bpp_math.bsm` provides `sin` and `cos` via Taylor series approx
(no FPU sin/cos in ARM64). Each call is ~50 cycles vs ~5 for a
native primitive. Per ray = 1 sin + 1 cos call → 38400 × 2 × 50 =
3.84M cycles/sec = ~0.1% on M4. Negligible.

If it shows up dominant in profile (unlikely), the fix is a
chip primitive `_a64_emit_fsin` / `_a64_emit_fcos` that emits a
direct FPU instruction. ARM64 doesn't have FSIN, so the actual
fallback is a precomputed lookup table or higher-order polynomial.
Either way, post-Phase 1 work, only if profile demands.

### 3. Worker auto-promote inside ray cast

Phase 6 closed the recursive deadlock guard, so calling
`job_parallel_for` inside a ray cast won't deadlock — it'll just
run serial. But the per-column ray cast IS the natural worker
parallelism. If Phase 1 wraps the column loop in
`job_parallel_for(SCREEN_W, fn_ptr(cast_and_draw_column))`,
columns dispatch to workers automatically. Worth profiling
serial vs parallel in Session 5 to see if 4 workers matters at
this workload.

Caveat: drawing into a shared layer from multiple threads needs
either per-worker scratch buffers or atomic pixel writes (which
B++ doesn't have for sub-word). Probably easier to keep the loop
serial in Phase 1; revisit for Phase 2 when sprite + enemy work
makes parallelism worth the bookkeeping.

### 4. Profiler integration from Session 1

DON'T wait for Phase 1 completion to use the profiler. From
Session 1's first frame, the workflow should be:

```bash
./bpp games/fps/wolf3d.bpp -o /tmp/wolf3d
/tmp/wolf3d  # runs interactively, prints profile summary on exit
```

Profile reveals architectural surprises early — if `peek` /
`buf_word_at` / similar primitive shows dominant on a "should be
float math" workload, something is off in codegen. Catching that
in Session 1 is easier than after 6 sessions of accumulated code.

---

## Decision point at end of Phase 1

After Session 5's profile pass, three branches:

1. **60fps confirmed, no clear bottleneck** → ship Phase 1, open
   Sprint 5 (Linux dynlink) or start Phase 2 (sprites + enemies).
2. **Clear hot path in B++ codegen** (e.g. CSE opportunity in ray
   loop) → open Tier F with concrete justification.
3. **Bottleneck in stbrender / GPU upload** → optimization is in
   stb cartridge land, not compiler — stash Tier F, fix stb and
   continue.

The profile data IS the deliverable of Session 5. Don't ship
Phase 1 without that report.

---

## Tonify checklist reminder

Every new function in this project MUST follow `docs/tonify_checklist.md`.
The skeleton files already do. Pattern reminders:

- File-scope vars: `extrn` (set-once) | `global` (cross-module) |
  `const` (literal) | `static auto` (file-private). No bare `auto`.
- Functions called only inside their file: `static`.
- Side-effect helpers (no return value): `void` + bare `return`.
- Phase annotation on every function: `@io` / `@base` / `@gpu` /
  `@time` / `@solo`.
- Float locals: `: float` annotation always.
- Public API params: full type hints, `(x: float, n: half)` not
  `(x, n)`.
- Use `peek_h/q/w` / `peekfloat_h/peekfloat` — never byte-assemble.
- Use `buf_fill` / `buf_copy` / `buf_cmp` instead of hand-rolled loops.
- Output via `put` / `put_err` (smart dispatch), not legacy
  `putstr`/`putnum`.
- String building via `strbuf_*`, not manual malloc.

If you find yourself bare-`auto`-ing a file-scope variable or
omitting `:float` on a float local, stop and re-read Rules 1, 2, 12.

---

## Next session entry point

Open `stb/stbraycast.bsm` and start with Session 1:

1. Read Lode's raycasting tutorial Part 1 (~10 minutes).
2. Translate the DDA pseudocode into the `cast_ray` body.
3. Fill in `_wolf3d_init_map` with the 16x16 layout.
4. Implement `cast_and_draw_column` flat-fill version.
5. Compile, run, take screenshot of the first textureless walls.
6. Profile a 600-frame run, save top-N to journal.
7. Commit: `wolf3d Phase 1 Session 1: DDA + flat walls`.

Estimated session length: 1.5–2 hours including profile pass.

Bootstrap byte-stable check after the commit:
```bash
BPP_BUILD_ID=00000000000000000000000000000000 ./bpp src/bpp.bpp -o /tmp/g1
BPP_BUILD_ID=00000000000000000000000000000000 /tmp/g1 src/bpp.bpp -o /tmp/g2
diff <(xxd /tmp/g1) <(xxd /tmp/g2)   # must be silent
```

(Bootstrap shouldn't change just from adding a game source — but
verify because the wolf3d.bpp `import` of `stbasset` etc. exercises
auto-injection paths.)

Suite check after commit:
```bash
bash tests/run_all.sh        # expect 126/0/12 (no new tests yet)
bash tests/run_all_c.sh      # expect 105/0/33
```

---

## Memory file to write at end of Session 1

After Session 1 ships, write a memory note at
`~/.claude/projects/-Users-Codes-b--/memory/project_wolf3d_session_<N>.md`:

```
# project_wolf3d_session_1.md

Wolf3D Phase 1 Session 1 SHIPPED. Flat walls + DDA render correctly,
player static at (8.0, 8.0). 60fps confirmed in M4 (~98% idle).
Profile top-3: cast_ray, cast_and_draw_column, layer_set_pixel.
Float annotation discipline held — no silent int degradation.
Next: Session 2 = texture mapping. Files: <list>. Commit: <sha>.
```

Each subsequent session gets its own memory file, so the project
stays cold-context-resumable.

---

## Closing note

Wolf3D is the deliverable that converts B++ from "well-engineered
language with games" to "language that ships Wolfenstein". Phase 6's
profiler exists exactly to make this profile-driven. Don't optimize
anything in Phase 1 without profile data. Don't open Tier F before
Phase 1 ships. Don't open Sprint 5 (Linux) until Phase 1 is on macOS.

The ouroboros closes another full turn here: profiler → game → profile
data → compiler decision. That sequence is the project's identity.

Boa caçada.
