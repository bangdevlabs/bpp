# Sidequest — `stbcamera2d.bsm` (factor 2D camera state)

**Status:** READY-TO-EXECUTE (escrito 2026-05-14).
**Trigger:** Audit de Session 4 do WC1 mod descobriu que `_camera_clamp`
em rts1 + `update_camera` em platformer têm o MESMO clamp idêntico,
e stbscene já tem state de câmera separado. 3 consumers existentes,
copy-paste em 2 deles. Rule 28 (killer use case gate) batido com folga.
**Predecessor history:** Session 4 commit `c736261` shipou camera
inline em rts.bpp justificando "1 consumer, defer per Rule 28" — a
justificativa estava errada porque o agente não comparou contra
platformer (que tem o mesmo clamp) nem mencionou stbscene.

---

## Pre-execution audit (verified by reading source 2026-05-14)

### Consumidor 1 — rts1 (`games/rts1/rts.bpp:62-72, 255-290`)

```b++
global cam_x, cam_y;
const CAM_SPEED_PX_S = 200;

static void _camera_clamp(map) {
    auto max_x, max_y;
    max_x = wc1_map_w(map) * 16 - SCREEN_W;
    max_y = wc1_map_h(map) * 16 - SCREEN_H;
    if (max_x < 0) { max_x = 0; }
    if (max_y < 0) { max_y = 0; }
    if (cam_x < 0) { cam_x = 0; }
    if (cam_y < 0) { cam_y = 0; }
    if (cam_x > max_x) { cam_x = max_x; }
    if (cam_y > max_y) { cam_y = max_y; }
}

static void _camera_pan_tick() {
    auto dx, dy, step;
    step = CAM_SPEED_PX_S * game_dt_ms() / 1000;
    dx = 0; dy = 0;
    if (key_down(KEY_LEFT)  != 0) { dx = dx - step; }
    // ... 8 keys total ...
    cam_x = cam_x + dx;
    cam_y = cam_y + dy;
    _camera_clamp(map);
}
```

Apply at render (`rts.bpp:193`): `tile_draw(tm, cam_x, cam_y, SCREEN_W, SCREEN_H)`.

Apply at sprite render (`wc1_render.bsm:42-43, 92-94`):
```b++
extrn cam_x, cam_y;
// ...
image_draw_size(s.atlas, s.frame,
                p.x * 16 - 8  - cam_x,
                p.y * 16 - 16 - cam_y,
                32, 32);
```

### Consumidor 2 — platformer (`games/platformer/platform.bpp:83, 153-161`)

```b++
auto CAM_X, CAM_Y;

void update_camera() {
    auto target_x, target_y, scr_w, scr_h;
    scr_w = 320; scr_h = 180;
    target_x = phys_px_x(player) - scr_w / 2 + 8;
    target_y = phys_px_y(player) - scr_h / 2 + 8;
    CAM_X = clamp(target_x, 0, MAP_W * TILE_W - scr_w);
    CAM_Y = clamp(target_y, 0, MAP_H * TILE_H - scr_h);
}
```

Apply at render (`platform.bpp:367`): `tile_draw(tilemap, CAM_X, CAM_Y, 320, 180)`.

Apply at sprite render (`platform.bpp:222-223`):
```b++
sx = phys_px_x(player) - CAM_X;
sy = phys_px_y(player) - CAM_Y;
```

Apply at parallax bg (`platform.bpp:254`):
```b++
mx = 0 - CAM_X / 3;
```

### Consumidor 3 — stbscene (`stb/stbscene.bsm:68-69, 134-136`)

```b++
static extrn _bg_cam_x: float;
static extrn _bg_cam_y: float;

void bg_set_camera(cam_x: float, cam_y: float) {
    _bg_cam_x = cam_x;
    _bg_cam_y = cam_y;
}
```

Esse é float (parallax precisa de subpixel pra factor < 1.0). Não migra
de imediato (escopo separado). Mas o briefing menciona pra ficar
documentado.

### Common pattern extraído

| Aspecto | rts1 | platformer | stbscene | Factor? |
|---|---|---|---|---|
| State (x, y) | int globais | int globais | float static | **SIM** (int por agora) |
| Clamp `world - screen` floored at 0 | ✓ | ✓ | — | **SIM** |
| Update mode | input pan | target follow | extern set | **2 funcs diferentes** |
| Apply at tile_draw | ✓ | ✓ | — | wrapper opcional |
| Apply at sprite blit | sprite_x - cam_x | sprite_x - CAM_X | — | helper opcional |

### Consumidores FORA do escopo

- **fps_3d_gpu / fps_wolf3d**: camera state é o `FPSBody` (px, py, angle). Vive
  dentro do player physics. Projection 3D + yaw/pitch. Shape genuinamente
  diferente. NÃO migra.
- **pathfind**: single-screen fixed viewport (verified: zero `cam_x` ou similar).
- **snake**: idem, single-screen fixed.

---

## Plan — 1 commit

### Mudanças em `stb/stbcamera2d.bsm` (NOVO arquivo, ~80 LOC)

```b++
// stbcamera2d.bsm — 2D camera state + clamp for top-down / side-scroll games.
//
// Captures the THREE things every 2D camera shares:
//   1. State (cam.x, cam.y in integer pixels — sub-pixel smoothing
//      lives in a future v2; today every consumer uses ints because
//      tile_draw + image_draw expect int pixel coords).
//   2. Bounds (world size + screen size) so the clamp is uniform.
//   3. Two update primitives — pan (input-driven, RTS) and follow
//      (target-driven, side-scroll / top-down zelda). Each consumer
//      picks which to call.
//
// What this cartridge does NOT cover:
//   - 3D camera (yaw/pitch/projection) — fps_3d / fps_wolf3d keep
//     their FPSBody-driven setup; the math is different shape.
//   - Cinematic state machine (Cinemachine-style) — Rule 28 when a
//     real consumer asks for it.
//   - Shake / smoothing / zoom — same Rule 28 reason.
//   - Edge-of-screen mouse pan — adds later as cam2d_pan_edge(...)
//     when RTS selection lands (S5+).
//   - Parallax bg camera (stbscene._bg_cam_x is float for sub-pixel
//     factor math); stbscene keeps its own state for now. Future
//     refactor: bg_set_camera reads from a Cam2D handle.

import "bpp_math.bsm";   // for clamp

struct Cam2D {
    x, y,                  // top-left of viewport in pixels (int)
    screen_w, screen_h,    // viewport size
    world_w, world_h       // map size in pixels (already multiplied by tile size)
}

// Allocate a fresh camera anchored at (0, 0) sized for the given
// viewport + world. Pass world_w/world_h as ZERO if the world size
// isn't known yet at creation time (e.g. before map_load); call
// cam2d_set_world later. With world == 0 the clamp is a no-op so
// the camera floats freely until bounds arrive.
cam2d_new(screen_w, screen_h, world_w, world_h) {
    auto c: Cam2D;
    c = malloc(sizeof(Cam2D));
    c.x = 0; c.y = 0;
    c.screen_w = screen_w; c.screen_h = screen_h;
    c.world_w  = world_w;  c.world_h  = world_h;
    return c;
}

// Free the camera handle. Pairs 1:1 with cam2d_new.
void cam2d_free(cam) {
    free(cam);
}

// Update world dimensions — used when a map loads after the camera
// was created, or when the map gets resized via hot-reload. Re-clamps
// the camera against the new bounds so the viewport never exposes
// off-map area after a shrink.
void cam2d_set_world(cam, world_w, world_h) {
    auto c: Cam2D;
    c = cam;
    c.world_w = world_w;
    c.world_h = world_h;
    _cam2d_clamp(c);
}

// Move the camera by (dx, dy) pixels. RTS-style free pan: each consumer
// computes dx/dy from input keys + dt and calls this. Clamping is
// folded in so callers never see off-map coords.
void cam2d_pan(cam, dx, dy) {
    auto c: Cam2D;
    c = cam;
    c.x = c.x + dx;
    c.y = c.y + dy;
    _cam2d_clamp(c);
}

// Center the camera on a world-space target point. Side-scroll /
// top-down follow: the consumer passes player position (or selection
// centroid, etc) and the camera centers on it, then clamps.
// For zero-deadzone snap-follow; deadzone variant lives in
// cam2d_follow_deadzone (v2 if a game asks).
void cam2d_follow(cam, target_x, target_y) {
    auto c: Cam2D;
    c = cam;
    c.x = target_x - c.screen_w / 2;
    c.y = target_y - c.screen_h / 2;
    _cam2d_clamp(c);
}

// Direct set (used by start_view seeding etc — set a desired x/y,
// then clamp). Skips the "compute via target" math of cam2d_follow
// because the caller already has the final pixel coords.
void cam2d_set(cam, x, y) {
    auto c: Cam2D;
    c = cam;
    c.x = x;
    c.y = y;
    _cam2d_clamp(c);
}

// Read accessors. Callers DON'T unpack the struct — keeps the API
// stable if Cam2D grows fields later (smoothing accumulators, etc).
cam2d_x(cam) { auto c: Cam2D; c = cam; return c.x; }
cam2d_y(cam) { auto c: Cam2D; c = cam; return c.y; }

// World → screen and screen → world helpers. The world→screen form
// is `world - cam`; the screen→world form is `screen + cam`. These
// will earn their slot once mouse-click selection lands (Session 4
// completion / S5) — the click handler needs screen→world to map
// mouse coords to game tile coords.
cam2d_world_to_screen_x(cam, wx) { auto c: Cam2D; c = cam; return wx - c.x; }
cam2d_world_to_screen_y(cam, wy) { auto c: Cam2D; c = cam; return wy - c.y; }
cam2d_screen_to_world_x(cam, sx) { auto c: Cam2D; c = cam; return sx + c.x; }
cam2d_screen_to_world_y(cam, sy) { auto c: Cam2D; c = cam; return sy + c.y; }

// Clamp the camera so the viewport never exposes off-map area.
// world_size - screen_size is the max coord; floor at 0 so a map
// smaller than the viewport pins to (0, 0). When world dimensions
// are zero (map not loaded yet) clamping is a no-op so the camera
// floats freely until cam2d_set_world arrives.
static void _cam2d_clamp(c: Cam2D) {
    auto max_x, max_y;
    if (c.world_w == 0 && c.world_h == 0) { return; }
    max_x = c.world_w - c.screen_w;
    max_y = c.world_h - c.screen_h;
    if (max_x < 0) { max_x = 0; }
    if (max_y < 0) { max_y = 0; }
    if (c.x < 0) { c.x = 0; }
    if (c.y < 0) { c.y = 0; }
    if (c.x > max_x) { c.x = max_x; }
    if (c.y > max_y) { c.y = max_y; }
}
```

### Mudanças em `games/rts1/rts.bpp` (migra cam_x/cam_y globais → Cam2D)

**Substituições:**

1. Linha ~62-72 — substituir os globals + const por:
```b++
// Camera state — handle owned by main, read by tile_draw and the
// per-entity render callback (which extrn's it). cam2d_pan / set
// keep the clamp uniform with the rest of the engine.
extrn cam;
const CAM_SPEED_PX_S = 200;
```

2. Linha ~165-175 — depois de `ecs_wire(world)`, substituir:
```b++
cam_x = 0; cam_y = 0;
_seed_camera_from_level(map);
```
por:
```b++
cam = cam2d_new(SCREEN_W, SCREEN_H, wc1_map_w(map) * 16, wc1_map_h(map) * 16);
_seed_camera_from_level(map);
```

3. Linha ~193 — `tile_draw(tm, cam_x, cam_y, ...)` vira `tile_draw(tm, cam2d_x(cam), cam2d_y(cam), SCREEN_W, SCREEN_H)`.

4. Linha ~199-203 — antes de `wc1_map_free(map)` adicionar `cam2d_free(cam);`.

5. Linha ~237-249 — `_seed_camera_from_level` vira:
```b++
static void _seed_camera_from_level(map) {
    auto i, n, k;
    n = wc1_map_entity_count(map);
    for (i = 0; i < n; i++) {
        k = wc1_map_entity_kind(map, i);
        if (str_eq(k, "start_view") != 0) {
            cam2d_set(cam,
                      wc1_map_entity_gx(map, i) * 16 - SCREEN_W / 2,
                      wc1_map_entity_gy(map, i) * 16 - SCREEN_H / 2);
            return;
        }
    }
}
```

6. Linha ~255-265 — `_camera_clamp` **DELETA INTEIRO** (vive na lib agora).

7. Linha ~274-290 — `_camera_pan_tick` vira:
```b++
static void _camera_pan_tick() {
    auto dx, dy, step;
    step = CAM_SPEED_PX_S * game_dt_ms() / 1000;
    dx = 0; dy = 0;
    if (key_down(KEY_LEFT)  != 0) { dx = dx - step; }
    if (key_down(KEY_A)     != 0) { dx = dx - step; }
    if (key_down(KEY_RIGHT) != 0) { dx = dx + step; }
    if (key_down(KEY_D)     != 0) { dx = dx + step; }
    if (key_down(KEY_UP)    != 0) { dy = dy - step; }
    if (key_down(KEY_W)     != 0) { dy = dy - step; }
    if (key_down(KEY_DOWN)  != 0) { dy = dy + step; }
    if (key_down(KEY_S)     != 0) { dy = dy + step; }
    cam2d_pan(cam, dx, dy);
}
```

8. No topo do arquivo, adicionar `import "stbcamera2d.bsm";`

### Mudanças em `games/rts1/wc1_render.bsm`

1. Linha ~42 — `extrn cam_x, cam_y;` vira:
```b++
import "stbcamera2d.bsm";
extrn cam;
```

2. Linha ~92-94 — `_render_unit` ajusta para:
```b++
image_draw_size(s.atlas, s.frame,
                p.x * 16 - 8  - cam2d_x(cam),
                p.y * 16 - 16 - cam2d_y(cam),
                32, 32);
```

### Mudanças em `games/platformer/platform.bpp`

1. Linha ~83 — `auto CAM_X, CAM_Y;` vira:
```b++
extrn cam;
```

2. No topo do arquivo, adicionar `import "stbcamera2d.bsm";`

3. Linha ~153-161 — `update_camera` vira:
```b++
void update_camera() {
    cam2d_follow(cam,
                 phys_px_x(player) + 8,    // +8 pra centrar no sprite
                 phys_px_y(player) + 8);
}
```

(O `+ 8` é o offset que o platformer já usava — fica preservado dentro do target).

4. Linha ~222-223 — sprite blit vira:
```b++
sx = phys_px_x(player) - cam2d_x(cam);
sy = phys_px_y(player) - cam2d_y(cam);
```

5. Linha ~254 — parallax bg vira:
```b++
mx = 0 - cam2d_x(cam) / 3;
```

6. Linha ~367 — `tile_draw(tilemap, CAM_X, CAM_Y, 320, 180)` vira:
```b++
tile_draw(tilemap, cam2d_x(cam), cam2d_y(cam), 320, 180);
```

7. Linha ~337-338 — `CAM_X = 0; CAM_Y = 0;` vira:
```b++
cam = cam2d_new(320, 180, MAP_W * TILE_W, MAP_H * TILE_H);
```

**Verificar lifecycle do platformer**: se ele tem reset/restart logic
que zerava CAM_X/CAM_Y mid-game, o equivalente vira `cam2d_set(cam, 0, 0)`.
Não precisa de cam2d_free a menos que platformer destrua/recria
mid-game.

---

## Smoke / verification

```bash
# 1. Build novos binaries
./bpp games/rts1/rts.bpp -o games/rts1/rts
./bpp games/platformer/platform.bpp -o games/platformer/platform

# 2. Bootstrap byte-stable
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2) | head

# 3. Tools dependentes
./bpp bang9/bang9.bpp -o /tmp/bang9_test
./bpp tools/level_editor/level_editor.bpp -o /tmp/le_test
./bpp tools/wc1_map_convert/wc1_map_convert.bpp -o /tmp/wmc_test

# 4. Suites
sh tests/run_all.sh    # expect 172/0/12 (or +1 if new smoke)
sh tests/run_all_c.sh  # expect 137/0/47

# 5. Visual (user-side, NOT agent's job to run)
./games/rts1/rts          # camera continua centrando em start_view,
                          # arrow/WASD pan continua smooth
./games/platformer/platform  # camera continua seguindo player,
                             # parallax bg continua scrollando
```

---

## Roteiro de commit

UM commit. Single-purpose:

```
feat: stbcamera2d — factor 2D camera state + clamp

Cartridge for the 2D camera state every top-down / side-scroll game
re-implements: viewport position (x, y in pixels), world bounds
(map size), screen size, and two update primitives (pan + follow)
that share the same clamp.

Motivation — audit of three existing consumers showed copy-paste:
- rts1 (free-pan top-down): cam_x/cam_y globals + _camera_clamp.
- platformer (follow side-scroll): CAM_X/CAM_Y globals + same
  world_size - screen_size floored-at-0 clamp.
- stbscene (parallax bg): own _bg_cam_x/_bg_cam_y state — kept
  separate for now because it's float (sub-pixel factor math);
  future refactor reads from a Cam2D handle.

Out of scope:
- 3D camera (fps_3d / fps_wolf3d) — FPSBody-driven, yaw/pitch +
  projection are a different shape.
- Cinematic / shake / smoothing / zoom — Rule 28 when a real
  game asks.
- Edge-of-screen mouse pan — adds when RTS selection lands.

Migrations:
- rts1: cam_x/cam_y globals → cam: Cam2D; _camera_clamp deleted;
  _camera_pan_tick uses cam2d_pan; _seed_camera_from_level uses
  cam2d_set; wc1_render reads via cam2d_x/cam2d_y.
- platformer: CAM_X/CAM_Y globals → cam: Cam2D; update_camera
  uses cam2d_follow; sprite + parallax + tile_draw reads via
  cam2d_x/cam2d_y.

Verification: bootstrap byte-stable, suite 172/0/12 native +
137/0/47 C. Visual smoke (user-side): rts1 camera continues
centered on player 0 start_view; arrow/WASD pan still smooth;
platformer camera continues following player with parallax
behind it.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

---

## Stop conditions

1. **Bootstrap não byte-stable** → reverter, investigar. stbcamera2d
   não está no compile path (é cartridge de jogo), mas se rts1/
   platformer mudarem o emit por causa de struct alignment diferente,
   reverter o migrate e investigar `sizeof(Cam2D)`.

2. **Platformer mid-game reset behaviour** → se houver código que
   destrói e recria o player ou reseta a câmera (não verificado
   profundamente, mas grep `CAM_X = 0` mostrou só a init), tratar
   no migrate. Não criar cam2d_free chamado fora de shutdown.

3. **stbscene não migra agora** — explicitamente fora do escopo. Se
   o agente sentir tentação de unificar, parar. Float vs int + lazy
   init order é refactor à parte.

4. **wc1_render globals captures** — `extrn cam` ao invés de
   `extrn cam_x, cam_y` muda 1 símbolo. Verificar que o link resolve
   (stbecs callbacks são fn_ptr, então o `cam` global é capturado
   pela escope normal, não pelo ecs_query_each).

---

## O que NÃO está nesse sidequest

- Selection (left-click → ring overlay) que faltou da Session 4 — fica
  como sidequest separada DEPOIS do stbcamera2d. Razão: selection vai
  precisar de `cam2d_screen_to_world_x/y` que essa lib já expõe; ter
  o helper pronto facilita implementar selection sem reinventar a
  transform.
- v2 deadzone follow (`cam2d_follow_deadzone(cam, x, y, dz_w, dz_h)`)
  — quando platformer ou outro consumer pedir explicitamente.
- v2 smoothing (lerp toward target) — Rule 28 quando game pedir.
- stbscene migrate — out of scope (float vs int, lazy init order).
- pathfind / snake — verified zero camera state, NÃO migram.

---

## Resumo do que justifica esse sidequest agora

| Critério | Status |
|---|---|
| Killer use cases (Rule 28) | 3 consumers existentes — rts1 + platformer + stbscene |
| Copy-paste verified | Clamp idêntico em rts1 + platformer (linhas auditadas) |
| LOC líquido | ~80 LOC lib novo vs ~25 LOC removido nos consumers = +55 LOC investido, mas elimina divergência futura |
| Cascading risk | Baixo — out-of-scope é explícito (3D, stbscene migrate) |
| Habilita Session 4 completion | Sim — `cam2d_screen_to_world_*` deixa selection trivial de fazer depois |
| Bootstrap risk | Baixo — cartridge novo + 2 game migrates, não toca compile path |
