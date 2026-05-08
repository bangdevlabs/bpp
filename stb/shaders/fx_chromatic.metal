// fx_chromatic.metal — Standalone RGB-split chromatic aberration.
//
// Samples R and B with a horizontal offset, leaves G centered.
// The classic "weak signal on a CRT" or "lens fringing" look. Pair
// with fx_scanlines for a budget CRT, or use solo for a
// glitch / impact accent on top of an otherwise clean image.
//
// Uniform layout (ChromU): 2 floats, padded to 16 bytes.
//   amount       — split distance multiplier (0.0..1.0); 0.5 typical
//   horizontal   — 1.0 = pure horizontal split, 0.0 = pure vertical

#include <metal_stdlib>
using namespace metal;

struct ChromU {
    float amount;
    float horizontal;
    float pad0;
    float pad1;
};

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V chrom_vert(uint vid [[vertex_id]]) {
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;
    V o;
    o.position = float4(fx * 2.0 - 1.0, 1.0 - fy * 2.0, 0.0, 1.0);
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp(filter::nearest, address::clamp_to_edge);

fragment float4 chrom_frag(V in [[stage_in]],
                            constant ChromU &u [[buffer(0)]],
                            texture2d<float> tex [[texture(0)]]) {
    float off = u.amount * 0.008;
    float2 dir_h = float2(off * u.horizontal, off * (1.0 - u.horizontal));
    float r = tex.sample(smp, in.uv + dir_h).r;
    float g = tex.sample(smp, in.uv).g;
    float b = tex.sample(smp, in.uv - dir_h).b;
    float a = tex.sample(smp, in.uv).a;
    return float4(r, g, b, a);
}
