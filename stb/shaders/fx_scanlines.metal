// fx_scanlines.metal — Standalone scanline effect.
//
// Just the dimmed-row pattern from CRT, isolated for callers
// that want the retro-monitor vibe without barrel distortion or
// chromatic aberration. Useful as a subtle layer on top of any
// pixel-art game.
//
// Uniform layout (ScanU): 2 floats, 8 bytes. Padded to 16-byte
// alignment by Metal automatically; the host ships 16 bytes.
//   intensity  — dim amount (0.0 = no scanlines, 1.0 = max dim)
//   density    — period in pre-warp pixels (1.0..8.0); typical 2.0

#include <metal_stdlib>
using namespace metal;

struct ScanU {
    float intensity;
    float density;
    float pad0;
    float pad1;
};

struct V {
    float4 position [[position]];
    float2 uv;
};

vertex V scan_vert(uint vid [[vertex_id]]) {
    float fx = ((vid & 1u) == 0u) ? 0.0 : 1.0;
    float fy = (vid < 2u)         ? 0.0 : 1.0;
    V o;
    o.position = float4(fx * 2.0 - 1.0, 1.0 - fy * 2.0, 0.0, 1.0);
    o.uv = float2(fx, fy);
    return o;
}

constexpr sampler smp(filter::nearest, address::clamp_to_edge);

fragment float4 scan_frag(V in [[stage_in]],
                           constant ScanU &u [[buffer(0)]],
                           texture2d<float> tex [[texture(0)]]) {
    float4 c = tex.sample(smp, in.uv);
    float scan_phase = in.uv.y * 540.0 / max(u.density, 1.0);
    float scan = 1.0 - u.intensity * 0.40
                * (0.5 + 0.5 * sin(scan_phase * 3.14159));
    return float4(c.rgb * scan, c.a);
}
