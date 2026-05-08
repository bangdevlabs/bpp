// fx_passthrough.metal — Identity post-process effect.
//
// The simplest possible stbfx effect: samples the source texture
// at the current UV and returns it unchanged. Used as the
// reference/null effect in tests — chaining N passthroughs should
// give a pixel-identical result to the source. Any deviation
// points at a bug in the chain orchestration (uniform binding,
// target swap, sampler choice) rather than the effect itself.
//
// Vertex stage emits a 4-vertex full-window triangle strip from
// vertex_id alone (no host buffer). Fragment samples with NEAREST
// + clamp_to_edge — the pixel-art canonical setting.

#include <metal_stdlib>
using namespace metal;

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V fx_pass_vert(uint vid [[vertex_id]]) {
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;
    V o;
    o.position = float4(fx * 2.0 - 1.0, 1.0 - fy * 2.0, 0.0, 1.0);
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp(filter::nearest, address::clamp_to_edge);

fragment float4 fx_pass_frag(V in [[stage_in]],
                              texture2d<float> tex [[texture(0)]]) {
    return tex.sample(smp, in.uv);
}
