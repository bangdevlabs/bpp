// Trivial pipeline smoke shader — Phase 2.1 of the GPU pipeline roadmap.
// Vertex stage emits a single fullscreen triangle from the vertex_id
// attribute alone, so the host side does not need to bind a vertex
// buffer for this pipeline. Fragment stage paints solid orange. The
// pair lets us prove that B++ programs can load a .metal file at
// runtime, build a custom MTLLibrary + MTLRenderPipelineState, and
// draw with it without disturbing the default stbrender pipeline.

#include <metal_stdlib>
using namespace metal;

// Three corners of a triangle that fully covers the clip-space
// rectangle [-1, 1] x [-1, 1]. Vertex IDs 0, 1, 2 pick each corner.
vertex float4 trivial_vert(uint vid [[vertex_id]]) {
    float2 corners[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    return float4(corners[vid], 0.0, 1.0);
}

// Constant orange (matches the stbdraw ORANGE color for visual
// consistency when the smoke test runs alongside other examples).
fragment float4 trivial_frag() {
    return float4(1.0, 0.55, 0.10, 1.0);
}
