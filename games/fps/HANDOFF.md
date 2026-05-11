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
| 1 | Sprites + depth buffer | **✅ shipped 2026-05-11** | ~250 | Per-fragment depth test + colored quads for enemy/door |
| 2 | Enemies (visible + AI) | **✅ shipped 2026-05-11** | ~430 | stbai cartridge (LoS/seek/move) + ai.bsm game module (FSM + A*) — wolf_world ECS via stbecs |
| 3 | Combat | pending | ~100 | Fire action, hitscan, damage, death |
| 4 | Audio | pending | ~80 | Gunfire / scream / footsteps; volume by distance |
| 5 | Doors + use | pending | ~120 | Tilemap state slot, use action, slide-open animation |
| 6 | Polish + profile | pending | ~50 | Full level, profile under enemy load, tonify sweep |

Commit cadence: one commit per session, message format
`wolf3d phase2 sN: <one-line summary>`.

---

## Next: Session 3 — Combat

### What Sessions 0–2 already set up

Run `fps_wolf3d` and you have:

- **Map loaded from JSON v2** with hot-reload from Bang 9's
  Levels tab (`assets/levels/level1.json`).
- **Sprites rendered as solid colored quads** — enemy red, door
  brown. Per-fragment depth test in `fps_raycast.metal` makes them
  occlude correctly behind walls.
- **Enemy AI** — each enemy chases the player. LoS clear ⇒
  straight-line seek (`stb/stbai.bsm`). LoS blocked but player
  in sight range ⇒ A* path via `stb/stbpath.bsm`, walks toward
  the next waypoint. Stops at melee range (0.8 cells). State
  is IDLE / CHASE; ATTACK lands in Session 3.
- **ECS entity storage** — `wolf_world` is an `stbecs.World`.
  Custom component `WolfEntPayload` (defined in `games/fps/ai.bsm`)
  holds kind / state / position / velocity / hp / cooldown per
  entity. Spawn / iteration happens via `ecs_spawn` / `ecs_alive` /
  `ecs_component_at`.

### What Session 3 ships

The player's side of combat:

1. Fire action (mouse left or KEY_SPACE) — emits a hitscan ray
   from the player's position along the facing vector.
2. Ray intersects against the closest enemy via
   `ai_los_clear`-style traversal *augmented* with entity-cell
   hit detection (per the ECS pool).
3. Enemy taking a hit decrements `hp` (already in payload). When
   `hp <= 0`, `ecs_kill` removes the entity from rendering + AI
   dispatch in the same frame.
4. Visual feedback: muzzle flash on the HUD overlay; brief
   colour-shift on the wounded enemy quad.

### Files this session touches

| File | Change |
|---|---|
| `stb/stbai.bsm` | `ai_hitscan(tm, x0, y0, dx, dy, max_dist, world, payload, payload_field_offsets) → handle` returning the closest hit entity ID, or -1. ~40 LOC. |
| `games/fps/combat.bsm` | NEW sibling module to `ai.bsm`. Owns the fire action handler + hit application + death. ~80 LOC. |
| `games/fps/fps_wolf3d.bpp` | Wire `combat_*` calls into solo phase; add muzzle-flash atlas tile. ~20 LOC. |

LOC realistic: ~140.

### Implementation hints

- Reuse `_ai_buf`-style scratch pattern for the hit result.
- `combat.bsm` should NOT touch the GPU sprite buffer directly —
  `_pack_sprite_buf` in fps_wolf3d.bpp keeps owning visual state.
- Tonify checklist applies (storage class, `@solo`, slice types
  where the struct says, comments-as-user-manual).

### What NOT to do this session

- No enemy melee back at the player. That's Session 3.5 if
  signal emerges; otherwise Session 6 polish.
- No animation frames. Session 4+ territory.
- No projectiles. Hitscan-only for V1 (matches Wolf3D's actual
  combat model — instant pistol/MP40, no flight time).

### Pitfall warnings for Session 3

- `write_f32` / `peekfloat` width mismatch (Session 2's biggest
  rabbit hole) is in `feedback_float_width_pair` memory. Use
  `write_f64` + `peekfloat` for B++-native scratch; the GPU pack
  path is the only consumer of `write_f32`.
- `_load_level`'s `tile_solid()` runs only for cell types 1..4 —
  if Session 5 (doors) introduces dynamic solidity, the
  PathFinder's `blocked` array needs a `wolf_ai_resync_walls`
  call when state flips.

---

## Architectural decisions (locked)

These are the contracts every later session inherits. Re-confirm
with the user before deviating.

### D1 — Entity storage uses existing `stb/stbecs.bsm`

Earlier drafts of this doc proposed shipping a new `stb/stbentity.bsm`
cartridge in Session 2. That was a verification-failure on the
author's part: `stb/stbecs.bsm` already exists and covers the
same shape — `ecs_new` / `ecs_spawn` / `ecs_kill` / `ecs_alive` /
`ecs_count` / `ecs_component_new` / `ecs_component_at`. Adding a
parallel cartridge would have been ~200 LOC of duplicate code.

Session 2 instead stores enemies via stbecs core (handle + alive
bit) plus one game-local custom component for Wolf3D-specific
fields:

```bpp
struct WolfEntPayload {
    kind:     byte,    // 1=enemy, 2=door, …
    state:    byte,    // FSM state index
    x: float, y: float,
    vx: float, vy: float,
    hp:       half,
    cooldown: half
}

wolf_world  = ecs_new(WOLF_MAX_ENTITIES);
wolf_payload = ecs_component_new(wolf_world, sizeof(WolfEntPayload));
```

The 16 bytes per entity that stbecs built-ins (`pos_x` /
`pos_y` / `vel_x` / `vel_y` / `flags` in milli-units) reserve and
Wolf3D doesn't use are negligible at our entity counts. Wolf3D's
positions live in cell-float units (FPSBody convention), so the
custom component owns position too — built-in pos slots stay zero.

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

### 6. Wolf3D visual asset ingestion (post-Phase-2)

Replace procedural placeholder textures + colored quads with
shareware visuals. Runs after Phase 2 closes (gameplay validated
with placeholders first) — decoupling gameplay risk from binary-
format risk.

**Workflow (manual, not agent-driven):**

1. User downloads Wolf3D shareware (legally free since 1992) to
   `~/Downloads/wolf3d_shareware/`
2. User clones + builds `HiPhish/Wolf3DExtract` once, runs it
   against the shareware dir → PPM dumps + WAV PCM
3. User runs `mogrify -format png` on the PPMs, picks the subset
   that matches the demo's visual budget, drops them into
   `games/fps/assets/wolf3d/{walls,sprites}/`
4. Agent writes `walls.json` + `sprites.json` manifests
   (id → human name → PNG path), swaps procedural textures in
   `fps_wolf3d.bpp` for `image_load` of the authentic assets

**Files this sidequest touches (one session, ~100 LOC):**

| File | Change |
|---|---|
| `games/fps/assets/wolf3d/walls.json` | NEW manifest |
| `games/fps/assets/wolf3d/sprites.json` | NEW manifest |
| `games/fps/fps_wolf3d.bpp` | Procedural texture path → image_load |

**Git policy:** `.WL1` source files live on the user's disk only,
NEVER in the repo. Extracted PNGs go in `.gitignore` (each
contributor extracts from their own shareware copy — same model
ECWolf / Wolf4SDL use). Only the JSON manifests + loader code get
committed. Avoids the copyright grey area entirely while keeping
the authentic look reachable for personal dev.

**Why we don't write our own extractor:** Wolf3DExtract exists,
works, ~5000 LOC of someone else's solved problem. Each retro
engine has a totally proprietary container format (Wolf3D VSWAP /
Doom WAD / Quake PAK / Build GRP) — code reuse across them is
~5 %. Investing in a B++-native extractor for a one-shot import
buys nothing.

**OPL2 (AdLib FM) emulator explicitly DROPPED.** Native B++ synth
descended from `mini_synth` covers all retro audio needs for
original games on the roadmap (Wolf3D-style FPS, Adventure-style
puzzle, RTS, etc.). OPL2 would only pay if we committed to
faithful DOS-era remakes — that's not the brand. Audio for
Phase 2 Session 4 will be B++ synth + (optionally) recorded
voice samples, no historical chip emulation.

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
