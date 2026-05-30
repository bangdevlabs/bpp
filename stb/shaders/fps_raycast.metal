// fps_raycast.metal — Phase 2.4 of the GPU pipeline roadmap.
//
// Vertex stage emits a fullscreen triangle from vertex_id alone
// (no vertex buffer needed on the host). Fragment stage runs a
// per-pixel DDA ray-cast against the tilemap supplied as a flat
// 16×16 byte buffer, mirrors fps_3d.bpp's CPU-side ray cast in
// shape but moves the work to the GPU. Per-pixel cost is one DDA
// walk plus a sky/floor/wall classification — well within budget
// at 320×180 or 640×480 on Apple Silicon.
//
// Uniform layout matches the B++ side exactly (see
// games/fps/fps_3d_gpu.bpp::pack_camera_uniforms). Writing to /
// reading from this layout in lockstep is the contract — adding
// a field requires changes on both sides.

#include <metal_stdlib>
using namespace metal;

struct CamUniforms {
    float px;     // camera world x
    float py;     // camera world y
    float angle;  // camera facing in radians
    float fov;    // horizontal field of view in radians
    uint  sw;     // screen width in pixels
    uint  sh;     // screen height in pixels
    uint  mw;     // map width in cells
    uint  mh;     // map height in cells
};

// Sprite uniforms — Wolf3D Phase 2 Session 1 (billboard sprites).
// One entry per visible entity (enemies, doors). Player_spawn entries
// MUST be filtered out on the host side; this struct expects only
// renderable entities. Cap is 16 — enough for Phase 2 Minimum (1 enemy
// + 1 door = 2). Raise both halves of the contract (this constant +
// SPRITE_BUF_SZ on B++ side) together when later sessions need more.
//
// Kind codes (must match B++ side fps_wolf3d.bpp::_pack_sprite_buf):
//   1 = enemy (textured from the sprite atlas, slot idx)
//   2 = door  (brown quad)
struct Sprite {
    float x;
    float y;
    uint  kind;
    uint  idx;    // sprite-atlas tile index (used when kind == 1); the
                  // atlas is a 64px-tile grid, tex_sprites at texture(4)
};

struct SpriteList {
    uint count;
    uint _pad[3];        // pad before the array starts at offset 16
    Sprite sprites[16];
};

struct VOut {
    float4 position [[position]];
};

// Fullscreen triangle. NDC corners (-1,-1), (3,-1), (-1,3) clip to
// the visible [-1, 1] x [-1, 1] region after rasterisation. The
// fragment stage gets the pixel position from in.position.xy
// directly, so there's no UV passthrough to worry about.
vertex VOut fps_vert(uint vid [[vertex_id]]) {
    float2 corners[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    VOut o;
    o.position = float4(corners[vid], 0.0, 1.0);
    return o;
}

// Phase 7 wall texturing — 4 textures bound at slots 0..3, one per
// wall_type. The sampler uses linear filter + repeat on U so the
// horizontal wrap when a ray crosses a tile boundary stays seamless;
// V is per-column, never wraps. Texture upload happens host-side via
// stbtexture's procedural generators (texture_brick / stone / wood /
// solid); future Wolf3D content arc swaps the generators for PNG
// loads through stbasset without touching this shader.
constexpr sampler wall_smp(filter::linear, address::repeat);

// Sprite atlas sampler — nearest filter (pixel-art, no blur) and clamp so a
// tile never bleeds into its neighbour at the 64px cell edges.
constexpr sampler spr_smp(filter::nearest, address::clamp_to_edge);

fragment float4 fps_frag(VOut in [[stage_in]],
                         constant CamUniforms& cam     [[buffer(0)]],
                         constant uchar*       map     [[buffer(1)]],
                         constant SpriteList&  sprites [[buffer(2)]],
                         texture2d<float> tex_brick   [[texture(0)]],
                         texture2d<float> tex_stone   [[texture(1)]],
                         texture2d<float> tex_wood    [[texture(2)]],
                         texture2d<float> tex_solid   [[texture(3)]],
                         texture2d<float> tex_sprites [[texture(4)]]) {
    float fx = in.position.x;
    float fy = in.position.y;
    float sw_f = float(cam.sw);
    float sh_f = float(cam.sh);
    float half_h = sh_f * 0.5;

    // Column-aware ray angle: -fov/2 at column 0, +fov/2 at column sw-1.
    float col_norm = fx / sw_f - 0.5;
    float ray_angle = cam.angle + cam.fov * col_norm;
    float dx = cos(ray_angle);
    float dy = sin(ray_angle);

    // DDA initial cell + delta-distances.
    int mx = int(cam.px);
    int my = int(cam.py);
    float ddx = abs(1.0 / dx);
    float ddy = abs(1.0 / dy);

    int step_x;
    int step_y;
    float side_dist_x;
    float side_dist_y;
    if (dx < 0.0) {
        step_x = -1;
        side_dist_x = (cam.px - float(mx)) * ddx;
    } else {
        step_x = 1;
        side_dist_x = (float(mx + 1) - cam.px) * ddx;
    }
    if (dy < 0.0) {
        step_y = -1;
        side_dist_y = (cam.py - float(my)) * ddy;
    } else {
        step_y = 1;
        side_dist_y = (float(my + 1) - cam.py) * ddy;
    }

    // March until the ray hits a non-zero map cell or runs off the
    // grid. 64 steps is comfortably more than the diagonal of a 16×16
    // map (≈ 22.6); shaders cannot break out of an unbounded loop on
    // Metal so the fixed bound matters.
    int hit = 0;
    int side = 0;
    int wall_type = 0;
    int mw_i = int(cam.mw);
    int mh_i = int(cam.mh);
    for (int i = 0; i < 64 && hit == 0; i++) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += ddx;
            mx += step_x;
            side = 0;
        } else {
            side_dist_y += ddy;
            my += step_y;
            side = 1;
        }
        if (mx < 0 || mx >= mw_i || my < 0 || my >= mh_i) {
            break;
        }
        wall_type = int(map[my * mw_i + mx]);
        if (wall_type > 0) {
            hit = 1;
        }
    }

    // Sky / floor for rays that escape the grid without hitting.
    // `wall_dist` initialised to a large sentinel here so the sprite
    // occlusion pass below still works when the ray misses every cell
    // (sprite in front of "infinity" always passes).
    float3 sky_col   = float3(0.125, 0.157, 0.188);
    float3 floor_col = float3(0.220, 0.157, 0.125);
    float3 base_col;
    float  wall_dist = 1.0e6;
    if (hit == 0) {
        base_col = (fy < half_h) ? sky_col : floor_col;
    }

    // Perpendicular wall distance + wall sampling — only when the
    // ray actually hit a cell. On a miss we leave `wall_dist` at the
    // 1e6 sentinel above so sprites still occlude correctly against
    // "infinity" (every sprite passes the depth test).
    if (hit == 1) {
        // Same formula as Lode's tutorial and the CPU-side
        // stbraycast cartridge. Avoids fish-eye that raw ray
        // distance would produce.
        if (side == 0) {
            wall_dist = (float(mx) - cam.px + (1.0 - float(step_x)) * 0.5) / dx;
        } else {
            wall_dist = (float(my) - cam.py + (1.0 - float(step_y)) * 0.5) / dy;
        }
        if (wall_dist < 0.0001) {
            wall_dist = 0.0001;
        }

        // Project the wall to screen-space — line_height is the
        // on-screen pixel extent of a 1.0-world-unit-tall wall at
        // this distance.
        float line_height = sh_f / wall_dist;
        float wall_top = half_h - line_height * 0.5;
        float wall_bot = half_h + line_height * 0.5;

        if (fy < wall_top) {
            base_col = sky_col;
        } else if (fy > wall_bot) {
            base_col = floor_col;
        } else {
            // Texture UV — Lode's standard formulation:
            //   u: where the ray hit on the wall in tile-local
            //      coordinates (0..1). Derived from the
            //      perpendicular hit point on the opposite axis
            //      from the wall's facing.
            //   v: vertical position down the wall column,
            //      normalised to [0..1] across the on-screen wall
            //      extent.
            // Side-flip: walls hit from the opposite side need a
            // mirrored u to keep the texture orientation consistent
            // regardless of which face the ray entered from. The
            // conditional below preserves Wolf3D-style continuity
            // across cell boundaries.
            float wall_x;
            if (side == 0) {
                wall_x = cam.py + wall_dist * dy;
            } else {
                wall_x = cam.px + wall_dist * dx;
            }
            wall_x = wall_x - floor(wall_x);
            if ((side == 0 && dx > 0.0) || (side == 1 && dy < 0.0)) {
                wall_x = 1.0 - wall_x;
            }
            float u = wall_x;
            float v = (fy - wall_top) / line_height;
            float2 uv = float2(u, v);

            // Sample the texture matching wall_type. Metal's branch
            // divergence cost is irrelevant — neighbouring fragments
            // in a column always hit the same texture.
            float3 wall_col;
            if (wall_type == 1) {
                wall_col = tex_brick.sample(wall_smp, uv).rgb;
            } else if (wall_type == 2) {
                wall_col = tex_stone.sample(wall_smp, uv).rgb;
            } else if (wall_type == 3) {
                wall_col = tex_wood.sample(wall_smp, uv).rgb;
            } else if (wall_type == 4) {
                wall_col = tex_solid.sample(wall_smp, uv).rgb;
            } else {
                wall_col = float3(0.50, 0.50, 0.50);
            }
            // E-W walls (side == 1) read 30 % darker so adjacent
            // faces of a wall block read as different surfaces —
            // the standard Wolf3D shading trick.
            if (side == 1) {
                wall_col *= 0.7;
            }
            base_col = wall_col;
        }
    }

    // ── Sprite pass — Wolf3D Phase 2 Session 1 ────────────────
    //
    // Per-fragment billboard sprite occlusion. For each visible
    // entity (host filtered to count <= 16): rotate its world
    // position into camera space so `rx` is the forward depth and
    // `ry` is the lateral offset, then test whether the current
    // fragment falls inside the sprite's screen rect AND the
    // sprite is closer than the wall behind it at this column.
    //
    // The closest-front-wins strategy is order-independent — no
    // host-side sort needed. Cost is O(count) per fragment; at
    // count<=16 and 320×180 = 57 600 fragments, that's ~922 K
    // sprite tests per frame, trivial on Apple GPU.
    //
    // Sprites are 1×1 world units, drawn centered at fy = half_h.
    // Screen size in pixels = sh / rx (same projection as walls),
    // which yields a square footprint that scales correctly with
    // distance.
    float  best_dist = wall_dist;
    float3 best_col  = base_col;
    float  cos_a     = cos(cam.angle);
    float  sin_a     = sin(cam.angle);
    float  fov_half  = cam.fov * 0.5;
    uint   ns        = sprites.count;
    if (ns > 16u) { ns = 16u; }
    for (uint si = 0u; si < ns; si++) {
        Sprite sp = sprites.sprites[si];
        float dxs = sp.x - cam.px;
        float dys = sp.y - cam.py;
        // Rotate (dxs, dys) by -cam.angle so rx is along the camera
        // forward and ry is the perpendicular sideways offset.
        float rx = dxs * cos_a + dys * sin_a;
        float ry = -dxs * sin_a + dys * cos_a;
        if (rx < 0.05) { continue; }                 // behind camera
        if (rx >= best_dist) { continue; }           // occluded by wall

        // Lateral angle relative to the camera centerline (0 = dead
        // ahead, ±fov/2 = screen edges). Pixels outside the FOV
        // wedge can't see this sprite at all.
        float ang = atan2(ry, rx);
        if (ang < -fov_half || ang > fov_half) { continue; }

        // Project sprite center to screen column. Same linear
        // mapping the wall pass uses (column_angle linearly spans
        // -fov/2 .. +fov/2 across 0..sw).
        float col_center = (ang / cam.fov + 0.5) * sw_f;
        float half_size  = sh_f / rx * 0.5;          // 1-unit sprite
        if (sp.kind == 3u) { half_size = half_size * 0.18; }  // bullets are small
        if (fx < col_center - half_size) { continue; }
        if (fx > col_center + half_size) { continue; }
        if (fy < half_h    - half_size) { continue; }
        if (fy > half_h    + half_size) { continue; }

        // Inside the sprite footprint. Local UV (0..1) across the square.
        float su = (fx - (col_center - half_size)) / (2.0 * half_size);
        float sv = (fy - (half_h    - half_size)) / (2.0 * half_size);

        float3 sp_col;
        if (sp.kind == 1u) {
            // Textured enemy: map the local UV into atlas tile `idx`. The
            // atlas is a grid of 64px tiles (columns = width / 64). A
            // transparent texel means this fragment isn't part of the sprite
            // here, so skip it and let the wall (or a nearer sprite) show.
            float aw   = float(tex_sprites.get_width());
            float ah   = float(tex_sprites.get_height());
            uint  cols = uint(aw) / 64u;
            float cx   = float(sp.idx % cols) * 64.0;
            float cy   = float(sp.idx / cols) * 64.0;
            float2 auv = float2((cx + su * 64.0) / aw, (cy + sv * 64.0) / ah);
            float4 texel = tex_sprites.sample(spr_smp, auv);
            if (texel.a < 0.5) { continue; }         // transparent — see behind
            sp_col = texel.rgb;
        } else if (sp.kind == 2u) {
            sp_col = float3(0.55, 0.35, 0.18);       // door brown
        } else if (sp.kind == 3u) {
            sp_col = float3(1.00, 0.95, 0.40);       // bullet — bright tracer
        } else {
            sp_col = float3(1.00, 0.00, 1.00);       // unknown — magenta
        }

        // Closest-front bookkeeping. Multiple sprites overlapping
        // the same pixel — the nearer one wins, walls bound the back.
        best_dist = rx;
        best_col  = sp_col;
    }

    return float4(best_col, 1.0);
}
