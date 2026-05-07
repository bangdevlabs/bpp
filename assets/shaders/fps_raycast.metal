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

fragment float4 fps_frag(VOut in [[stage_in]],
                         constant CamUniforms& cam [[buffer(0)]],
                         constant uchar*       map [[buffer(1)]]) {
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

    // Wall colour by tile type — matches the fps_3d.bpp legend.
    float3 wall_col;
    if (wall_type == 1) {
        wall_col = float3(0.69, 0.16, 0.21);  // brick red
    } else if (wall_type == 2) {
        wall_col = float3(0.55, 0.55, 0.55);  // stone grey
    } else if (wall_type == 3) {
        wall_col = float3(0.55, 0.36, 0.20);  // wood brown
    } else if (wall_type == 4) {
        wall_col = float3(0.78, 0.20, 0.78);  // magenta solid
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
