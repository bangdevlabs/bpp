// zoomer.metal — fullscreen textured quad with a zoom/pan transform.
//
// The bound texture is the captured desktop, uploaded as BGRA8 (the
// native byte order of macOS CoreGraphics capture), so the fragment
// stage needs NO channel swizzle. The zoom uniform gives a focus point
// `center` (in UV [0,1]) and a `scale`; the visible region shrinks
// around `center` as `scale` grows. Pixels outside the texture (when
// zoomed/panned past an edge) are drawn black.
//
// Pair with: gpu_pipeline_load + gpu_bind_texture_frag +
// gpu_bind_sampler_frag + gpu_uniform_set_frag(0, ...).

#include <metal_stdlib>
using namespace metal;

struct VOut {
    float4 position [[position]];
    float2 uv;
};

struct ZoomU {
    float2 center;   // look-at point in UV [0,1], shown at screen centre
    float  scale;    // > 1 zooms in around the look-at
    float  pad;
};

// Fullscreen triangle from vertex_id (no vertex buffer needed). Same
// shape as stb's textured.metal: NDC bottom-left maps to UV (0,1)
// because Metal texture coordinates have their origin at the top-left.
vertex VOut zoom_vert(uint vid [[vertex_id]]) {
    float2 corners[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    float2 uvs[3] = {
        float2(0.0,  1.0),
        float2(2.0,  1.0),
        float2(0.0, -1.0)
    };
    VOut o;
    o.position = float4(corners[vid], 0.0, 1.0);
    o.uv       = uvs[vid];
    return o;
}

fragment float4 zoom_frag(VOut in [[stage_in]],
                          texture2d<float> tex [[texture(0)]],
                          sampler smp [[sampler(0)]],
                          constant ZoomU& u [[buffer(0)]]) {
    // Map this fragment's screen UV to a texture UV: the look-at sits at
    // screen centre (0.5), and the sampled region shrinks by `scale`.
    float2 uv = u.center + (in.uv - 0.5) / u.scale;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    return tex.sample(smp, uv);
}
