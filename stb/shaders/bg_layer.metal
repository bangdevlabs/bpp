// bg_layer.metal — Parallax background layer shader.
//
// One pipeline shared by every layer in stbscene. The vertex stage
// emits a 4-vertex triangle strip covering the full window in NDC.
// The fragment stage maps each pixel back to "world" coordinates
// (window pixel + camera * parallax_factor), normalises against the
// texture size, and samples with NEAREST + repeat — so layers tile
// infinitely as the camera moves.
//
// Uniform layout (BgU): 12 floats, 48 bytes, 16-byte aligned.
//   win_w, win_h     — window pixels (the present rect, not the OS window)
//   tex_w, tex_h     — layer texture pixels
//   cam_x, cam_y     — global camera position (any units callers prefer)
//   factor_x/y       — parallax multipliers (0 = static, 1 = matches camera)
//   alpha            — per-layer opacity (output.a is texel.a * alpha)
//   pad0/1/2         — reserved; kept zero by the host

#include <metal_stdlib>
using namespace metal;

struct BgU {
    float win_w; float win_h;
    float tex_w; float tex_h;
    float cam_x; float cam_y;
    float factor_x; float factor_y;
    float alpha;
    float pad0; float pad1; float pad2;
};

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V bg_vert(uint vid [[vertex_id]]) {
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;
    V o;
    o.position = float4(fx * 2.0 - 1.0, 1.0 - fy * 2.0, 0.0, 1.0);
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp(filter::nearest, address::repeat);

fragment float4 bg_frag(V in [[stage_in]],
                        constant BgU &u [[buffer(0)]],
                        texture2d<float> tex [[texture(0)]]) {
    float wx = in.uv.x * u.win_w;
    float wy = in.uv.y * u.win_h;
    float sx = wx + u.cam_x * u.factor_x;
    float sy = wy + u.cam_y * u.factor_y;
    float2 t_uv = float2(sx / u.tex_w, sy / u.tex_h);
    float4 c = tex.sample(smp, t_uv);
    return float4(c.rgb, c.a * u.alpha);
}
