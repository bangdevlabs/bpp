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

fragment float4 fps_frag(VOut in [[stage_in]],
                         constant CamUniforms& cam [[buffer(0)]],
                         constant uchar*       map [[buffer(1)]],
                         texture2d<float> tex_brick [[texture(0)]],
                         texture2d<float> tex_stone [[texture(1)]],
                         texture2d<float> tex_wood  [[texture(2)]],
                         texture2d<float> tex_solid [[texture(3)]]) {
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
    float3 sky_col   = float3(0.125, 0.157, 0.188);
    float3 floor_col = float3(0.220, 0.157, 0.125);
    if (hit == 0) {
        return fy < half_h ? float4(sky_col, 1.0) : float4(floor_col, 1.0);
    }

    // Perpendicular wall distance — same formula as Lode's tutorial
    // and the CPU-side stbraycast cartridge. Avoids fish-eye that
    // raw ray distance would produce.
    float wall_dist;
    if (side == 0) {
        wall_dist = (float(mx) - cam.px + (1.0 - float(step_x)) * 0.5) / dx;
    } else {
        wall_dist = (float(my) - cam.py + (1.0 - float(step_y)) * 0.5) / dy;
    }
    if (wall_dist < 0.0001) {
        wall_dist = 0.0001;
    }

    // Project the wall to screen-space — line_height is the on-screen
    // pixel extent of a 1.0-world-unit-tall wall at this distance.
    float line_height = sh_f / wall_dist;
    float wall_top = half_h - line_height * 0.5;
    float wall_bot = half_h + line_height * 0.5;

    if (fy < wall_top) {
        return float4(sky_col, 1.0);
    }
    if (fy > wall_bot) {
        return float4(floor_col, 1.0);
    }

    // Texture UV — Lode's standard formulation:
    //   u: where the ray hit on the wall in tile-local coordinates
    //      (0..1). Derived from the perpendicular hit point on the
    //      opposite axis from the wall's facing.
    //   v: vertical position down the wall column, normalised to
    //      [0..1] across the on-screen wall extent.
    // Side-flip: walls hit from the opposite side need a mirrored u
    // to keep the texture orientation consistent regardless of which
    // face the ray entered from. The conditional below preserves
    // Wolf3D-style continuity across cell boundaries.
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

    // Sample the texture matching wall_type. Metal's branch divergence
    // cost is irrelevant here — neighbouring fragments in a single
    // screen column always hit the same texture (one wall per column).
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
    // E-W walls (side == 1) read 30 % darker so adjacent faces of a
    // wall block read as different surfaces — the standard Wolf3D
    // shading trick.
    if (side == 1) {
        wall_col *= 0.7;
    }
    return float4(wall_col, 1.0);
}
