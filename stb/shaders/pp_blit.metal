// Pixel-perfect blit shader — Phase 4.1.2 of the GPU pipeline
// roadmap. Used by stbgame's final present pass: after the game
// has rendered the world to an offscreen MTLTexture at virtual
// resolution, this pipeline samples that texture with a NEAREST
// sampler and draws it as a quad inside the window drawable, at
// an integer-multiple destination rectangle. Letterbox / pillar-
// box bars are produced by the surrounding render_clear call (the
// quad covers only the dst rect; the rest of the window keeps the
// clear color).
//
// Vertex stage synthesises 4 corners from [[vertex_id]] (no
// vertex buffer required). Caller draws as a triangle strip with
// 4 vertices via _stb_gpu_draw_quad. The dst rect and the window
// dimensions arrive as uniform [[buffer(0)]] so the same pipeline
// handles any window size + virtual size combination.
//
// Fragment samples with sampler `smp_n(filter::nearest)` declared
// inline. Nearest is the contract for pixel-art games — bilinear
// would re-introduce the blur Phase 4 exists to eliminate.

#include <metal_stdlib>
using namespace metal;

struct BlitU {
    float dst_x;     // window-pixel x of the dst rect's top-left corner
    float dst_y;     // window-pixel y of the dst rect's top-left corner
    float dst_w;     // dst rect width in window pixels (= virtual_w * scale)
    float dst_h;     // dst rect height in window pixels
    float win_w;     // window width in pixels (drawable size)
    float win_h;     // window height in pixels
    float pad0;      // (16-byte alignment for Metal uniform buffers)
    float pad1;
};

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V pp_blit_vert(uint vid [[vertex_id]],
                      constant BlitU &u [[buffer(0)]]) {
    // vid 0..3 → triangle-strip corners:
    //   0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right.
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;

    // Pixel position in window coordinates (origin top-left, y down).
    float px = u.dst_x + fx * u.dst_w;
    float py = u.dst_y + fy * u.dst_h;

    // Convert to NDC. Metal NDC origin is bottom-left, y up.
    float ndc_x = (px / u.win_w) * 2.0 - 1.0;
    float ndc_y = 1.0 - (py / u.win_h) * 2.0;

    V o;
    o.position = float4(ndc_x, ndc_y, 0.0, 1.0);
    // Texture coordinate: top-left of the offscreen texture maps
    // to (0, 0); bottom-right to (1, 1). Metal texture origin is
    // top-left, so fy here matches v directly without flip.
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp_n(filter::nearest, address::clamp_to_edge);

fragment float4 pp_blit_frag(V in [[stage_in]],
                             texture2d<float> tex [[texture(0)]]) {
    return tex.sample(smp_n, in.uv);
}
