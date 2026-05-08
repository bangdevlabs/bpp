// fx_dither.metal — 4×4 ordered Bayer dither.
//
// Quantises each colour channel to a small number of levels,
// using a 4×4 Bayer matrix as the offset so the quantisation
// noise turns into the classic checkerboard / cross-hatch
// pattern instead of flat banding. Reduces 24-bit colour to
// roughly 4-bit per channel without looking blocky — same
// trick early consoles used for gradient skies.
//
// Uniform layout (DitherU): 2 floats, padded to 16 bytes.
//   intensity  — blend amount (0.0 = passthrough, 1.0 = full dither)
//   levels     — quantisation steps per channel (2.0..16.0); 4.0 typical
//
// 4×4 Bayer matrix (normalised 0..15 -> 0..1):
//     0   8   2  10
//    12   4  14   6
//     3  11   1   9
//    15   7  13   5

#include <metal_stdlib>
using namespace metal;

struct DitherU {
    float intensity;
    float levels;
    float pad0;
    float pad1;
};

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V dither_vert(uint vid [[vertex_id]]) {
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;
    V o;
    o.position = float4(fx * 2.0 - 1.0, 1.0 - fy * 2.0, 0.0, 1.0);
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp(filter::nearest, address::clamp_to_edge);

fragment float4 dither_frag(V in [[stage_in]],
                             constant DitherU &u [[buffer(0)]],
                             texture2d<float> tex [[texture(0)]]) {
    float4 c = tex.sample(smp, in.uv);
    float bayer[16] = {
         0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
        12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
         3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
        15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
    };
    uint2 px = uint2(in.position.xy);
    uint  bx = px.x & 3u;
    uint  by = px.y & 3u;
    float threshold = bayer[by * 4u + bx] - 0.5;

    float n = max(u.levels, 2.0);
    float3 dithered;
    dithered.r = floor(c.r * n + threshold + 0.5) / n;
    dithered.g = floor(c.g * n + threshold + 0.5) / n;
    dithered.b = floor(c.b * n + threshold + 0.5) / n;
    dithered = clamp(dithered, 0.0, 1.0);

    float3 mixed = mix(c.rgb, dithered, u.intensity);
    return float4(mixed, c.a);
}
