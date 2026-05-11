# Wolf3D — Phase 2 Prep Doc

**Phase 1 closed:** 2026-05-04. `games/fps/fps_3d.bpp` walks at 60 FPS
through a textured 2.5D maze. See `docs/journal.md` entry
`2026-05-04` for the full Phase 1 retrospective.

This file is the entry brief for the agent picking up Phase 2.
Phase 2 = Wolfenstein-3D Minimum: 1 enemy type, 1 weapon, 1 door
type, 1 level, ASCII map loader v2 with entity grammar. Authentic
features (3-4 enemies, 4 weapons, key + door pairs, multi-level,
secret walls, voice acting) are independent add-ons in Phase 3+.

---

## Phase 1 → Phase 2 file migration plan

When Phase 2 ships content-rich Wolf3D, `games/fps/fps_3d.bpp`
migrates to `examples/fps_3d.bpp` as the canonical 2.5D raycasting
template every future B++ raycaster forks from. The file that
replaces it in `games/fps/` becomes the actual Wolf3D game with
sprites + enemies + levels + audio.

That migration happens at the END of Phase 2. Until then,
`games/fps/fps_3d.bpp` keeps growing as the Phase 2 development
target.

---

## What's already in place from Phase 1

- `games/fps/fps_3d.bpp` — entry point (~360 LOC). Walks, turns,
  collides, profiles. Spawn point (3.5, 8.5) facing east in a 16×16
  test maze with four wall types ('#' brick, '@' stone, '%' wood,
  '!' solid magenta debug).
- `stb/stbtexture.bsm` — procedural materials. New patterns
  (e.g. tile checkerboard, gradient, perlin) drop in as additional
  `texture_X_to_buf` + factory pairs.
- `stb/stbraycast.bsm` — DDA + RayHit + projection. Wall_type
  dispatch is owned by the game; cartridge stays content-blind.
- `stb/stbprofile.bsm` — Tier 1 profile HUD. Tier 2/3 (sparkline,
  per-thread, GPU timing, scoped zones) is the pre-Phase-2 polish
  sidequest if appetite holds.
- `stb/stbphys.bsm` Chapter FPS — FPSBody + fps_walk + fps_turn.
- `bpp_math.bsm` — sin_f / cos_f / abs_f / floor_f public,
  auto-injected.

Suite at Phase 1 close: **131 / 0 / 12 native + 110 / 0 / 33 C**.
Bootstrap byte-stable at `bpp = 20e75f653dabf309bcfdef7a9d738756815b2682`.

---

## Phase 2 — Three decisions to register before Session 0

These were aligned in the Phase 1 close meta-planning round.
Re-confirm with the user before deviating.

### D1 — Entity storage

**Decision: new cartridge `stb/stbentity.bsm`.**

Game-local `entities.bsm` was option (a), extending `stbphys` with
an EntityList chapter was option (c). The two-consumer rule fires
in advance: Wolf3D + future DOOM-clone-or-similar = two consumers
expected, justifying the promotion to stb without waiting.

**Cartridge surface (proposed):**

```bpp
struct Entity {
    kind: byte,           // 1=enemy, 2=item, 3=projectile, 4=...
    state: byte,          // FSM state index, kind-specific
    flags: byte,          // alive bit + flags
    sprite_id: byte,      // index into game's sprite atlas
    x: float, y: float,   // world position
    vx: float, vy: float, // velocity
    hp: half,             // health if applicable
    cooldown: half        // generic countdown timer (frames)
}

ent_pool_new(cap)            // EntityPool with cap slots
ent_spawn(pool, kind, x, y)  // returns entity handle or 0
ent_kill(pool, handle)
ent_alive(pool, handle)@base
ent_at(pool, handle): Entity ptr
ent_count(pool)@base
ent_each(pool, fn_ptr)       // iterate alive entities, call fn(handle)
ent_sort_by_depth(pool, px, py) // for sprite z-sort against player
```

Hitscan helper:
```bpp
ent_hitscan_ray(pool, x0, y0, dir_x, dir_y, max_dist) // returns hit handle or 0
```

### D2 — AI substrate

**Decision: hand-rolled FSM per enemy type for Phase 2 Minimum.**

Phase 2 ships ONE enemy type. A generic `stbfsm.bsm` is overkill at
N=1; the cost flips at N=3+. When enemy variety grows (Phase 3
Authentic), reopen with profile data showing the FSM pattern is
real shared infrastructure.

For now, the enemy AI lives game-local in
`games/fps/wolf3d.bpp` (or a sibling `games/fps/ai.bsm` if it
grows past ~150 LOC).

### D3 — Door / per-cell state

**Decision: extend `stbtile.Tilemap` with a per-cell state slot of
WORD width.**

Word, not byte. Open/closed is a bit; open/closing/closed/opening
+ timer is byte; physics state (lift Y-offset, rotating wall angle,
animated wall phase) is word. Word future-proofs for free at
2 KB extra per 16×16 map (negligible).

**Tilemap struct addition:**

```bpp
struct Tilemap {
    w, h, tw, th,
    data,           // existing: type-id byte per cell
    solid_mask,
    tileset, tile_count, remap,
    state           // NEW: word per cell, all uses (door anim, lift y, etc.)
}
```

API:
```bpp
tile_state_get(tm, gx, gy)@base
tile_state_set(tm, gx, gy, val)
```

Doors then use the state slot for animation phase (0=closed, 1000=fully
open, intermediate=animating). The renderer reads state to offset the
column projection or skip the wall entirely while open.

---

## Phase 2 sessions (~7 sessions, ~780 LOC)

| # | Session | Ships | stb impact | LOC |
|---|---------|-------|------------|-----|
| 0 | Map loader v2 | ASCII → tilemap + entity list (`# @ % ! e d k`) | extend stbtile (entity grammar parsing) | ~80 |
| 1 | Sprites + depth buffer | Billboard sprite renders, walls occlude correctly | extend stbraycast (depth buf + draw_sprite) | ~200 |
| 2 | Enemies (visible + AI) | Enemy sprite walks toward player via LoS raycast | new stbentity (D1) | ~150 |
| 3 | Combat | Fire action, hitscan, damage, death | extend stbphys FPS (hitscan_ray) | ~100 |
| 4 | Audio | Gunfire / scream / footsteps; volume by distance | uses existing stbsound + stbmixer | ~80 |
| 5 | Doors + use | Tilemap state slot (D3), use action, slide-open animation | extend stbtile (per-cell state) | ~120 |
| 6 | Polish + profile | Full level, profile under enemy load, tonify sweep | none | ~50 |

---

## Sidequest queue (between Phase 1 close and Phase 2 attack)

**Hold these until appetite returns; none block Session 0.**

### 1. Profile HUD Tier 2/3

- **Tier 2 sparkline** — last N frames as a bar chart underneath
  the FPS line. ~30 LOC of `render_rect` calls in stbprofile,
  driven from the existing frame ring.
- **Tier 3a per-thread breakdown** — bucketize profile_dump samples
  by `_job_thread_idx` so the top-N shows main vs each worker.
  Needs ~20 LOC in stbprofile + `_prof_capture_at` already
  records the thread index.
- **Tier 3b GPU timing** — Metal `MTLCommandBuffer.GPUStartTime`
  + `GPUEndTime` events. Layer 4 territory; depends on what the
  Metal driver exposes through `objc_msgSend`. Skip until profile
  shows GPU-side bottleneck.
- **Tier 3c scoped zones** — `@profile_zone("name") { ... }`
  parser annotation that injects zone enter / exit calls. Real
  compiler work (~50 LOC parser + codegen + ~30 LOC stbprofile).
  Phase 7 polish stretch.

### 2. `_to_buf` API split sweep across other stb cartridges

Candidates (same architectural gain as stbtexture got — testable
headless API + reusable CPU-side path):

- `stbpal._fill_*` (currently private static helpers — promote)
- `stbimage` PNG decode (`png_decode_to_buf` separate from upload)
- `stbsound` WAV decode (`wav_decode_to_buf` separate from
  audio upload)

~30 min audit + ~1 h implementation.

### 3. SIGPROF dump-noise residual

The 64 KB resolver guard from Phase 1 covers most cases. If any
phantom samples persist in profile_dump output (e.g. attributing
to dump_profile or profile_dump itself), add a sample-time filter:
reject any captured PC whose resolved name is `profile_dump` /
`_runtime_resolve_pc` / similar before tallying. Can also harden
profile_stop to fence outstanding signals before returning.

### 4. Maestro callback dt — pass µs instead of ms

Phase 1 fix kept the callback contract at ms (every existing caller
expects ms). When refactoring callers en masse, switch `solo` /
`base` callbacks to receive `dt` in µs and update fps_walk /
fps_turn / similar to do `dt / 1_000_000` instead of
`dt / 1_000`. Eliminates the 1 ms truncation residual that makes
each physics step nominally 16 ms when the real value is 16.667 ms.

### 5. Diagnostic for `stat fps`-style HUD outside profile mode

A standalone "always-on FPS counter" via
`profile_hud_draw_fps_only(x, y, sz, color)` that renders just the
FPS line independent of the profile toggle. Useful for shipping
games that want a permanent overlay without the recording UX.

---

## Phase 2 Session 0 starting checklist

When the next agent picks up:

1. Read `docs/journal.md` entry `2026-05-04` (Phase 1 close).
2. Read this file (you are here).
3. Read D1/D2/D3 above. Confirm with the user before deviating.
4. Open `stb/stbtile.bsm` — add the `state` field + accessors.
5. Write `tests/test_stbtile_state.bpp` to lock the contract.
6. Update `games/fps/fps_3d.bpp` ASCII map to use entity glyphs
   (`#@%! e d k`), and add the loader.
7. After Session 0: `games/fps/fps_3d.bpp` should still build +
   walk + profile, plus the loader handles the entity glyphs even
   if Phase 2 hasn't placed any yet.

Commit cadence: **one commit per session**, message format
`fps3d phase2 sN: <one-line summary>`.

---

## What NOT to do at Session 0

- Don't pre-commit to the entity rendering shape. Session 1 owns
  sprite + depth buffer; Session 0 just parses entity glyphs into
  an array of `{kind, x, y}` records that Session 1 picks up.
- Don't move `fps_3d.bpp` to `examples/` yet. The migration
  happens at end of Phase 2 once content-rich Wolf3D replaces it
  in `games/fps/`.
- Don't open Tier F based on the Session 5 profile — explicitly
  decided NOT to (CPU is mostly vsync idle; gargalo is GPU
  presentation, not compute).

---

## Reference materials

- Lode Vandevenne raycasting tutorial: lodev.org/cgtutor/raycasting
  — Part 1 (DDA, already shipped), Part 3 (sprites, Session 1
  reference).
- `docs/journal.md` 2026-05-04 — Phase 1 retrospective.
- `docs/how_to_dev_b++.md` "2.5D engine + textures + profiler" —
  cartridge tour.
- `docs/tonify_checklist.md` Rule 20 — promote-on-second-consumer.
- `docs/warning_error_log.md` Known compiler diagnostic gaps —
  W027 + diagnostic candidates.

Boa caçada na Phase 2.

---

## Update 2026-05-11 — Session 0 supersedes to Caminho A (level_editor entity layer)

After the fxlab arc closed (commits `5a9f05d` → `f5fbaeb` + polish
batch `350604f`), Bang 9 was canonized as the engine/IDE of the
B++ ecosystem — see new `docs/bang9_space_manual.md` for the full
architectural premise. Wolf3D Phase 2 maps now go through that
flow, not through standalone ASCII text editing.

**Session 0 shifts** from "ASCII glyph map (`#@%!edk`) + stbtile
grammar parser" → **"level_editor extended with entity layer
(Caminho A) + JSON schema v2 + fps_wolf3d loader walks entities[]"**.

Reasoning: the two-consumer rule already fired. The user + agents
are already editing Wolf3D maps inside Bang 9 (same flow used for
sprites via modulab, effects via fxlab). Caminho B (Wolf3D-style
glyph-range single-grid) would force a refactor in Phase 3 when
"wall + entity in same cell" becomes a hard requirement. Caminho
A is the canonical Tiled / LDtk pattern and reusable across every
future game (RPG, RTS, Adventure all need entity layers).

### Updated Session 0 scope

Files touched:

| File | What |
|---|---|
| `stb/stbtile.bsm` | NO state field yet — D3 moves to Session 5 (Doors) where it's actually consumed |
| `tools/level_editor/level_editor_lib.bsm` | Second layer `_lvl_entities`. UI toggle `Tiles ↔ Objects`. Object palette panel. JSON v2 read+write. |
| `assets/levels/level1.level.json` (Wolf3D) | New file with player_spawn + 1 enemy + 1 door for Phase 2 demo scope |
| `games/pathfind/assets/levels/level1.level.json` | **Migrate in-place to v2** (add `"entities": []`). No backward compat code anywhere. |
| `bang9/level1.level.json` | Same in-place migration if present |
| `games/fps/fps_wolf3d.bpp` | Walk `entities[]` at init, populate internal `EntityList` (renderer is Session 1's problem) |
| `tests/test_level_editor_v2.bpp` | Lock schema v2 contract (write + read round-trip with entities + tiles) |

### Realistic LOC estimate (corrects the original ~80 LOC line)

- stbtile state field: 0 (deferred to Session 5)
- level_editor entity layer state: ~30
- UI toggle Tiles/Objects: ~30
- Object palette UI: ~50
- JSON schema v2 read+write: ~50
- v1 → v2 migration of existing 2-3 files: 0 LOC (manual edit, ~5 min)
- fps_wolf3d entity loader: ~30
- Tests: ~50
- **Total: ~240 LOC**, not 80. Plan accordingly.

### Four nuances locked

**(1) State field deferred.** D3 (`stbtile.state` word per cell)
is a runtime concern (door animation, lift Y-offset). It stays on
Session 5 (Doors) where the renderer first reads it. Session 0
keeps stbtile untouched.

**(2) Object palette uses `kind` as string, not enum.** Entity
records are `{kind: "player_spawn", x, y, props: {...}}`. Phase 2
demo ships 3 kinds (`player_spawn`, `enemy`, `door`); Phase 3+
adds variants (`enemy_guard`, `enemy_dog`, etc) without schema
refactor. UI v1 shows only kind + position; advanced props edited
via JSON until the editor earns rich UI per kind.

**(3) JSON schema v2 — no backward compat.** Migrate the 2-3
existing v1 level files in-place (add `"entities": []` field).
Loader only knows v2 going forward. Rationale: backward compat
shim is forever-debt for a 5-minute migration; mental tax of "is
this v1 or v2?" compounds across every asset type the engine will
support. Tonify spirit — don't build a shim graveyard.

**(4) Bang 9 panel side already wired.** `_panel_levels` in
`bang9/panels.bsm` already calls
`level_editor_lib_init(g_proj_root)`. Session 0 just extends what
the lib does; no Bang 9 surgery needed. (Same pattern fxlab arc
established — embed contract is canonical.)

### What stays unchanged from the original handoff

- **D1 (`stbentity` cartridge)** — still right. Lands in Session
  2 when enemies become real entities at runtime. Session 0 just
  produces the entity list shape that Session 2 will consume.
- **D2 (hand-rolled FSM for Phase 2)** — still right.
- **D3 (per-cell state word)** — still right, but moved from
  Session 0 to Session 5 (Doors). State field is meaningless
  without a consumer; Session 5 is the consumer.
- **Sidequest queue (Tier 2/3 profile, etc.)** — unchanged.
- **Commit cadence "one commit per session"** — unchanged.

### Reference for the next agent

- `docs/bang9_space_manual.md` — engine/IDE convergence model,
  embed contract, project layout convention, hot-reload backbone.
  Read first. Establishes WHY Caminho A is the move.
- `docs/journal.md` 2026-05-10 entry — fxlab polish milestone +
  Tonify lessons graduated (`@base` only for PURE, dual-watch
  pattern, project-sacred-install-seed).
- `tools/level_editor/level_editor_lib.bsm` — current state
  (single-layer tiles), pattern to extend from.
- `tools/fxlab/fxlab_lib.bsm` — embed contract reference (proj_root
  parameter, host owns UI boot, lib reads/writes
  `<proj_root>/effects/`).

Same boa caçada — agora com map editor visual no Bang 9.

