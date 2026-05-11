# Wolf3D — Phase 2 Status

**Phase 2 = Wolfenstein-3D Minimum**: 1 enemy type, 1 weapon, 1 door
type, 1 level, edited visually inside Bang 9. Authentic features
(3-4 enemies, 4 weapons, key + door pairs, multi-level, secret
walls, voice acting) move to Phase 3+.

Entry file: `games/fps/fps_wolf3d.bpp`. Map: `games/fps/assets/levels/level1.json`.
Tooling: Bang 9 (Levels tab + Effects tab) — see `docs/bang9_space_manual.md`.

---

## Sessions

| # | Session | Status | LOC | Ships |
|---|---|---|---|---|
| 0 | Map loader + entity layer | **✅ shipped 2026-05-11** | ~240 | level_editor entity layer + JSON schema v2 + fps_wolf3d loader |
| 1 | Sprites + depth buffer | **→ next** | ~200 | Billboard sprites render, walls occlude correctly |
| 2 | Enemies (visible + AI) | pending | ~150 | Enemy walks toward player via LoS raycast |
| 3 | Combat | pending | ~100 | Fire action, hitscan, damage, death |
| 4 | Audio | pending | ~80 | Gunfire / scream / footsteps; volume by distance |
| 5 | Doors + use | pending | ~120 | Tilemap state slot, use action, slide-open animation |
| 6 | Polish + profile | pending | ~50 | Full level, profile under enemy load, tonify sweep |

Commit cadence: one commit per session, message format
`wolf3d phase2 sN: <one-line summary>`.

---

## Next: Session 1 — Sprites + depth buffer

### What Session 0 already set up

Run `fps_wolf3d` and you have:

- **Map loaded from JSON v2** — `assets/levels/level1.json` parsed
  on init. Tiles populate `world_map`; entities populate the
  `wolf_entities` array (kind / x / y per record). `player_spawn`
  entity already applied to the player FPSBody, so the spawn point
  comes from the level file, not hardcoded.
- **3 entities placed in the demo level**: `player_spawn`, `enemy`
  (one), `door` (one). Inspect with the Bang 9 Levels tab — Tab key
  toggles between Tiles and Objects mode.
- **No renderer for entities yet** — that's this session's job.

### What this session ships

A billboard sprite renderer that:

1. Walks `wolf_entities` each frame, sorts by depth (back-to-front
   relative to the player).
2. For each entity: project its world position into screen space
   using the same FPSBody / FOV math `fps_render_phase` uses for
   walls. Skip if behind the camera.
3. Compute the column range it covers, then for each column
   compare the sprite's depth against the wall's depth at that
   column (the depth buffer Session 1 also introduces). Draw
   the sprite column only where it's in front of the wall.
4. Pick a sprite atlas by `kind`: solid colored quad is fine for
   V1 (red square = enemy, brown square = door). Authored
   sprites land in Session 4+ when audio + content polish hits.

### Files this session touches

| File | Change |
|---|---|
| `stb/stbraycast.bsm` | Extend with depth buffer + `draw_sprite` helper. ~80 LOC. |
| `games/fps/fps_wolf3d.bpp` | Renderer walks `wolf_entities`, calls draw_sprite. ~50 LOC. |
| `assets/shaders/fps_raycast.metal` | Depth output from the wall pass + sprite quad pass on top. ~40 LOC. |
| `tests/test_stbraycast_sprite.bpp` | Lock the depth-comparison contract (spawn a sprite behind a wall, assert it doesn't render). ~30 LOC. |

LOC realistic: ~200.

### Implementation hints

- Reference: Lode Vandevenne raycasting tutorial Part 3 (sprites).
  Same DDA + projection that Phase 1 used for walls; the new piece
  is the column-by-column depth comparison.
- Don't pre-commit to texture atlases. Solid colored quads
  validate the renderer in isolation; sprite art swaps in via
  `image_load` once the geometry is correct.
- The depth buffer is **per column**, not per pixel — it stores
  the perpendicular wall distance for that screen column. Sprites
  compare per column too, not per pixel.
- Player position + facing already pull from the level via the
  `player_spawn` entity, so changing the spawn requires only
  editing the level JSON in Bang 9 — no rebuild.

### What NOT to do this session

- Don't add enemy AI. That's Session 2's whole job (D1 lands then).
- Don't try to make sprites animate (idle / walk / attack frames).
  Session 4 territory.
- Don't add the `stbtile.state` field. That's Session 5 (Doors).

---

## Architectural decisions (locked)

These are the contracts every later session inherits. Re-confirm
with the user before deviating.

### D1 — Entity storage cartridge

`stb/stbentity.bsm` lands in **Session 2** when enemies become
runtime objects with state + AI. Session 0 produces the static
entity list (`wolf_entities` array of `{kind, x, y}`); Session 1
just renders them; Session 2 promotes that list into a real
`EntityPool` with kind-specific FSM state.

Cartridge surface (proposed, can drift in Session 2):

```bpp
struct Entity {
    kind: byte,           // 1=enemy, 2=item, 3=projectile, ...
    state: byte,          // FSM state index, kind-specific
    flags: byte,          // alive bit + flags
    sprite_id: byte,      // index into game's sprite atlas
    x: float, y: float,   // world position
    vx: float, vy: float, // velocity
    hp: half,             // health if applicable
    cooldown: half        // generic countdown timer (frames)
}

ent_pool_new(cap)
ent_spawn(pool, kind, x, y) → handle
ent_kill(pool, handle)
ent_alive(pool, handle)@base
ent_at(pool, handle): Entity ptr
ent_count(pool)@base
ent_each(pool, fn_ptr)
ent_sort_by_depth(pool, px, py)
ent_hitscan_ray(pool, x0, y0, dir_x, dir_y, max_dist) → handle
```

Justified by the two-consumer rule (Wolf3D + future DOOM-clone
expected); no need to wait for the second consumer to ship before
promoting.

### D2 — AI substrate

**Hand-rolled FSM per enemy type for Phase 2.** Phase 2 ships ONE
enemy type — a generic `stbfsm.bsm` is overkill at N=1. AI lives
game-local in `games/fps/fps_wolf3d.bpp` (or sibling `ai.bsm` if it
grows past ~150 LOC). Promote to stb when N≥3 with profile data
showing the FSM pattern is shared.

### D3 — Per-cell state for doors

`stb/stbtile.Tilemap` gets a `state` field (WORD per cell) in
**Session 5 (Doors)**, not earlier. State is a runtime concern —
shipping it before there's a consumer is dead weight. Session 5's
renderer reads it for door animation phase (0 = closed,
1000 = fully open, intermediate = animating).

```bpp
struct Tilemap {
    w, h, tw, th,
    data, solid_mask,
    tileset, tile_count, remap,
    state           // ← NEW in Session 5: word per cell
}

tile_state_get(tm, gx, gy)@base
tile_state_set(tm, gx, gy, val)
```

### D4 — Entity `kind` is a string, not an enum

`{kind: "player_spawn", x, y}`. Locked Session 0. Lets Phase 3+
introduce variants (`enemy_guard`, `enemy_dog`, etc.) without
schema churn or a central enum table to maintain. Cost is one
string compare per entity per relevant lookup — fine at N≤32.

### D5 — Level JSON schema v2 is the only format

No backward compat. v1 files were migrated in-place during
Session 0. Loader only knows v2. Per Tonify spirit — backward compat
shims are forever-debt for a one-time migration.

---

## Sidequest queue

Hold these until appetite returns; none block any session.

### 1. Profile HUD Tier 2/3

- **Tier 2 sparkline** — last N frames as a bar chart underneath
  the FPS line. ~30 LOC of `render_rect` calls in stbprofile,
  driven from the existing frame ring.
- **Tier 3a per-thread breakdown** — bucketize profile_dump samples
  by `_job_thread_idx` so the top-N shows main vs each worker.
  Needs ~20 LOC in stbprofile + `_prof_capture_at` already records
  the thread index.
- **Tier 3b GPU timing** — Metal `MTLCommandBuffer.GPUStartTime` +
  `GPUEndTime` events. Skip until profile shows GPU-side
  bottleneck.
- **Tier 3c scoped zones** — already shipped as `@profile("name") { ... }`
  in Phase 6.3 (May 2026). This entry is closed.

### 2. `_to_buf` API split sweep

Same architectural gain as stbtexture got — testable headless API
+ reusable CPU-side path. Candidates:

- `stbpal._fill_*` (currently private static helpers — promote)
- `stbimage` PNG decode (`png_decode_to_buf` separate from upload)
- `stbsound` WAV decode (`wav_decode_to_buf` separate from
  audio upload)

~30 min audit + ~1 h implementation.

### 3. SIGPROF dump-noise residual

The 64 KB resolver guard from Phase 1 covers most cases. If any
phantom samples persist in profile_dump output, add a sample-time
filter rejecting captured PCs whose resolved name is
`profile_dump` / `_runtime_resolve_pc` / similar before tallying.

### 4. Maestro callback dt — pass µs instead of ms

Current contract: ms (truncated). Switch `solo` / `base` callbacks
to receive µs and update fps_walk / fps_turn callers to do
`dt / 1_000_000` instead of `dt / 1_000`. Eliminates the 1 ms
truncation residual that makes each physics step nominally 16 ms
when the real value is 16.667 ms.

### 5. Standalone "always-on FPS counter"

`profile_hud_draw_fps_only(x, y, sz, color)` that renders just the
FPS line independent of the profile toggle. Useful for shipping
games that want a permanent overlay without the recording UX.

---

## File layout (Wolf3D + adjacent)

```
games/fps/
├── HANDOFF.md                       ← this file
├── fps_wolf3d.bpp                   ← entry, Session 1+ extends
└── assets/
    └── levels/
        └── level1.json              ← schema v2, Bang 9 Levels tab edits

examples/
├── fps_3d_cpu.bpp                   ← Phase 1 CPU baseline (graduated 2026-05-08)
└── fps_3d_gpu.bpp                   ← Phase 2.4 GPU baseline (graduated 2026-05-08)

stb/
├── stbraycast.bsm                   ← Session 1 extends with depth buf + sprites
├── stbentity.bsm                    ← Session 2 NEW
├── stbtile.bsm                      ← Session 5 adds state field
├── stbphys.bsm Chapter FPS          ← Session 3 adds hitscan_ray
├── stbtexture.bsm                   ← already shipped (Phase 1)
└── stbprofile.bsm                   ← already shipped (Phase 1)

tools/
├── level_editor/                    ← multi-layer tilemap+objects (Session 0)
└── fxlab/                           ← effect tuner (independent arc, May 2026)
```

---

## Reference materials

- `docs/bang9_space_manual.md` — engine/IDE premise + embed
  contract + project layout. Read first when joining the project.
- `docs/journal.md` — chronological context. Most relevant entries:
  `2026-05-04` (Phase 1 close), `2026-05-10` (fxlab arc),
  `2026-05-11` (Wolf3D Phase 2 Session 0).
- `docs/how_to_dev_b++.md` "2.5D engine + textures + profiler" —
  cartridge tour for the engine subsystems Wolf3D leans on.
- `docs/tonify_checklist.md` — apply on every change (storage
  class, visibility, return type, phase annotation, slice types,
  comments-as-user-manual).
- Lode Vandevenne raycasting tutorial: lodev.org/cgtutor/raycasting
  — Part 1 (DDA, shipped Phase 1), Part 3 (sprites, Session 1
  reference).

---

## Phase 1 retrospective

Phase 1 closed 2026-05-04 with `fps_3d.bpp` walking at 60 FPS
through a textured 2.5D maze. Suite at close: 131/0/12 native +
110/0/33 C, bootstrap byte-stable. Full retrospective in
`docs/journal.md` 2026-05-04. The two demo files (`fps_3d.bpp`
CPU, `fps_3d_gpu.bpp` GPU) graduated to `examples/` 2026-05-08
when `fps_wolf3d.bpp` branched to become the Phase 2 dev target.

Boa caçada na Phase 2.
