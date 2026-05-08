// fx_crt.metal — CRT TV post-process effect.
//
// Bundles three classic CRT-style distortions in one fragment
// shader: barrel-distort UVs outward from screen center, sample
// R/G/B with a horizontal offset for chromatic aberration, then
// modulate by a sine scanline pattern for the dimmed-row look.
//
// All three contribute proportional to the `intensity` uniform —
// 0.0 is a clean passthrough, 1.0 is full kitsch arcade-monitor
// vibes. Callers tune through the `effect_crt_*` setters in
// stbfx.bsm rather than touching the buffer layout directly.
//
// Uniform layout (CrtU): 4 floats, 16 bytes, 16-byte aligned.
//   intensity         — master amount (0.0..1.0)
//   aberration        — RGB split distance (0.0..1.0); typical 0.3
//   scanline_density  — scanline period in pixels (1.0..8.0); typical 2.0
//   pad

#include <metal_stdlib>
using namespace metal;

struct CrtU {
    float intensity;
    float aberration;
    float scanline_density;
    float pad;
};

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V crt_vert(uint vid [[vertex_id]]) {
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;
    V o;
    o.position = float4(fx * 2.0 - 1.0, 1.0 - fy * 2.0, 0.0, 1.0);
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp(filter::nearest, address::clamp_to_edge);

fragment float4 crt_frag(V in [[stage_in]],
                          constant CrtU &u [[buffer(0)]],
                          texture2d<float> tex [[texture(0)]]) {
    float2 uv = in.uv;

    // Barrel distortion: pull UVs outward from center proportional
    // to squared distance. Intensity scales the warp strength.
    float2 c = uv - 0.5;
    float r2 = dot(c, c);
    uv = 0.5 + c * (1.0 + r2 * u.intensity * 0.25);

    // Frame the warped image: fragments outside [0,1] in either
    // axis are the curved-screen vignette, drawn black.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // Chromatic aberration: sample R / G / B with a horizontal
    // offset. 0.005 is a tasteful baseline at intensity = 1.0.
    float aoff = u.intensity * u.aberration * 0.005;
    float r = tex.sample(smp, uv + float2( aoff, 0.0)).r;
    float g = tex.sample(smp, uv).g;
    float b = tex.sample(smp, uv - float2( aoff, 0.0)).b;

    // Scanlines: dim every Nth row. `scanline_density` is the
    // period in (pre-warp) pixels; with the texture height
    // implicit, we feed pixel-space y by multiplying by a
    // calibration constant. The 0.3 factor caps maximum dimming
    // so even at intensity=1 the picture stays readable.
    float scan_phase = uv.y * 540.0 / max(u.scanline_density, 1.0);
    float scan = 1.0 - u.intensity * 0.30
                * (0.5 + 0.5 * sin(scan_phase * 3.14159));

    return float4(r * scan, g * scan, b * scan, 1.0);
}
