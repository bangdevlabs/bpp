// Textured-pass smoke shader — Phase 2.2 of the GPU pipeline
// roadmap. Vertex stage emits a fullscreen triangle from
// vertex_id alone (so the host does not need to bind a vertex
// buffer) and outputs a UV that maps the visible NDC region
// [-1, 1] x [-1, 1] to texture coordinates [0, 1] x [0, 1].
// Fragment stage samples the bound texture through the bound
// sampler. Use with gpu_pipeline_load + gpu_bind_texture_frag +
// gpu_bind_sampler_frag from stbshader.

#include <metal_stdlib>
using namespace metal;

struct VOut {
    float4 position [[position]];
    float2 uv;
};

// Three vertices of a triangle covering NDC [-1, 1] x [-1, 1].
// The triangle extends past the viewport on the right and top so
// it is rasterized over the entire visible region after clipping.
// UV is mapped so that the bottom-left of NDC corresponds to UV
// (0, 1) — Metal texture coords have origin at top-left, so the
// y axis flips relative to NDC.
vertex VOut tex_vert(uint vid [[vertex_id]]) {
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

// Sample the bound texture at the interpolated UV. The sampler
// at slot 0 is supplied by the host; with gpu_sampler_nearest
// the result is a sharp pixel-art look, with gpu_sampler_linear
// it is smoothed bilinearly.
fragment float4 tex_frag(VOut in [[stage_in]],
                         texture2d<float> tex [[texture(0)]],
                         sampler smp [[sampler(0)]]) {
    return tex.sample(smp, in.uv);
}
