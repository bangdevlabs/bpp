# GPU Pipeline Roadmap — Phases 2-7 + Compiler Polish

**Generated:** 2026-05-05
**Scope:** Comprehensive plan for B++ GPU rendering foundation +
  pixel-art capabilities + compiler polish (scoped zones + GPU timing).
  Builds on Wolf3D Phase 1 closeout (`fps_3d.bpp`, CPU baseline) and
  V3 Sessions 0-5 closeout (fn-pointer typing infrastructure).

This document is the authoritative plan for taking B++ from "ships
CPU-rendered games" to "ships pixel-art-capable indie games on par
with Thimbleweed Park / Return to Monkey Island shader stacks."

---

## TL;DR

Six sequential phases (~2550 LOC, 18 sessions) building a complete
GPU foundation in B++. Validation through `fps_3d_gpu.bpp` (test
bench evolved per phase) and standalone examples (`snake_gpu.bpp`,
`adventure_demo_min.bpp`).

**By end of Phase 7:** B++ can ship games using Thimbleweed/RtMI-class
techniques: sprite batching, per-sprite tint, pixel-perfect integer
scaling, layered backgrounds with parallax, custom fragment shader
effects (CRT, palette quantize, dither, watercolor), all instrumented
via scoped zones + GPU timing.

**Phase 2 redirection from games_roadtrip.md:** original roadmap had
Wolf3D Phase 2 = sprites + enemies + audio + doors. Redirected to GPU
foundation first because:
1. Engine-completeness > content-first for B++ identity (language +
   runtime project, not Wolf3D clone)
2. Validates 2 rendering paths (CPU + GPU) before adding content
3. Wolf3D content (sprites etc.) is a SUBSET of pixel-art capabilities
   enabled here — Phase 8 picks Wolf3D content using Phase 3 sprite
   batching + Phase 5 layered HUD

Wolf3D content is now Phase 8+, pulling from this stack.

---

## Strategic framing

### Why now

Wolf3D Phase 1 closeout (2026-05-04) shipped `fps_3d.bpp` walking at
60 FPS via CPU ray casting. Profile shows ~10% M4 utilization — no
performance pressure forcing GPU migration.

The motivation is **architectural completeness**:
- B++ has CPU rendering (stbrender layer-based, stbsprite CPU path).
- B++ does not have native GPU shader pipeline.
- Without it, future games of any complexity (adventure, RTS,
  modern platformer) hit the ceiling of what CPU pixel-buffer
  rendering can do.

Building this foundation now positions B++ for the games_roadtrip
journey (Wolf3D content → adventure demo → RTS) without retrofitting
later.

### Why this scope (not less)

Each phase enables a class of capability:

| Phase | Class enabled | Without it B++ ships only... |
|---|---|---|
| 2 | Vertex+fragment foundation | nothing GPU-side |
| 3 | Sprite batching | one-sprite-at-a-time CPU rendering |
| 4 | Pixel-perfect rendering | non-integer-scaled blurry games |
| 5 | Layered backgrounds | flat single-layer scenes |
| 6 | Effect library | unstyled raw output |

Skipping any phase leaves a gap that breaks downstream phases. The
order is forced by dependencies: sprite batching needs foundation,
pixel-perfect needs render-to-texture, layered needs sprite batching,
effects need render-to-texture and multi-pass, scoped zones need
multi-pass to be useful.

### Why this scope (not more)

Compute shaders deferred (no use case in roadmap). Stack-arg routing
for fn-types deferred (V3 cap is 8 args, sufficient). Linux Vulkan
parity deferred (Metal-first ships now, parity is dedicated sidequest).
@poly annotation deferred (no polymorphic registry demand). Tier F
CPU optimizations deferred (profile-driven, see
`compiler_boost_roadmap.md`).

---

## Decisions locked

| ID | Choice | Rationale |
|---|---|---|
| Approach | Vertex+fragment pipeline (Approach A, fragment shader fullscreen quad for ray cast) | Universal foundation for 2D/2.5D, industry standard (Cruelty Squad, Lode tutorial, all WebGL raycasters, Thimbleweed/RtMI sprite stacks) |
| Compute shader | Not in this roadmap | YAGNI — no use case justifying. stbcompute remains backlog item. |
| stbrender CPU path | Coexists with GPU path | Backward compatibility for existing games. Migration is opt-in per game. |
| Cross-platform | Metal-first, Linux Vulkan as separate sidequest | Metal ships now, Vulkan parity scoped separately (~1500 LOC dedicated, paralelo ao Metal layer) |
| Asset paths | `assets/shaders/*.metal`, `assets/sprites/*.png`, `assets/atlases/*.json` | Standard hierarchy, leverages stbasset PNG loader |
| Render target | Off-screen `MTLTexture` for pixel-perfect path | Standard pixel-art technique |
| Validation strategy | `fps_3d_gpu.bpp` evolves per phase + small standalone tests | Test bench + gate per capability |
| Bootstrap | Byte-stable IMEDIATO after each session | Standard B++ discipline |
| Wolf3D content | Phase 8+, after foundation lands | Subset of pixel-art capabilities; not gating |

---

## Pre-flight (before Phase 2.1)

### 1. Confirm Metal infrastructure baseline

```bash
grep -n "_stb_gpu_create_texture\|MTLDevice\|MTLCommandBuffer" \
  src/backend/os/macos/_stb_platform_macos.bsm | head
```

Verify what Metal primitives already exist. `_stb_gpu_create_texture`
is shipped (used by stbtexture). Need to check for: `MTLDevice` access,
command queue, command buffer creation, render encoder.

### 2. Confirm stbrender layer path is preserved

Phase 2 does NOT remove `render_layer_*` API. Coexistence by design.
Confirm via grep that all CPU-rendered games still compile after
foundation lands.

eol### 3. Confirm shader file load capability

Phase 2.1 needs to load `.metal` files from `assets/shaders/`. Verify
stbasset (or platform layer) has file read primitive that handles
text content. If not, add `read_text_file(path)` primitive (~30 LOC).

### 4. Reserve W029, E247-E251, etc. ranges

Phase 6.3 (scoped zones) introduces new diagnostics. Reserve numbers
in `warning_error_log.md` upfront to avoid collision with parallel
work.

### 5. Confirm build determinism for shader compilation

Metal shader sources are compiled at runtime (or via Xcode toolchain
upfront). For bootstrap byte-stable, shader compilation must be
deterministic OR happen post-build (not part of `./bpp` output). Plan
for runtime compilation (deterministic per Metal spec).

---

## Phases

### Phase 2 — GPU Foundation + GPU Timing (~700 LOC, 4 sessions)

**Objective:** Vertex+fragment pipeline base + Metal infrastructure +
GPU timing (Tier 3b #6) integrated from day 1.

#### Session 2.1 — Pipeline scaffolding (~200 LOC) — **SHIPPED 2026-05-05**

New sibling cartridge `stb/stbshader.bsm` (carved out mid-session
from the original "stbrender extension" placement — pipeline
management is conceptually distinct from draw primitives, mirrors
the stbpal / stbtexture / stbtile pattern). Public API:
`gpu_pipeline_load`, `gpu_pipeline_use`, `gpu_pipeline_use_default`,
`gpu_uniform_set_frag`, `gpu_uniform_set_vert`, `gpu_draw_full`.
Backed by 6 new platform primitives in
`src/backend/os/macos/_stb_platform_macos.bsm` plus matching no-op
stubs in `src/backend/os/linux/_stb_platform_linux.bsm` so user
code links on either OS. Shared `_gpu_compile_pipeline` helper
extracted from `_stb_gpu_init`'s inline shader-compile dance
(Tonify Rule 20 — second-consumer extraction).

Vertex descriptor is intentionally NOT in 2.1: the smoke test
drives a fullscreen-triangle vertex stage that synthesises
positions from `[[vertex_id]]`, so no buffer attribute is bound.
Vertex descriptors arrive in Phase 3.1 when sprite batching needs
per-vertex pos / UV / tint attributes.

**Smoke test:** `examples/gpu_pipeline_smoke.bpp` +
`assets/shaders/trivial.metal` — opens 320×180 window, paints
solid orange via custom pipeline. Compiles to a 313 KB binary;
manual run verifies the pipeline path end-to-end.

#### Session 2.2 — Texture pipeline (~200 LOC) — **SHIPPED 2026-05-05**

Sampler descriptor builder + per-stage texture / sampler binding
shipped into `stb/stbshader.bsm`:

- `gpu_sampler_nearest()` / `gpu_sampler_linear()` — clamp-to-edge
  samplers; pixel-art and bilinear-smooth presets respectively.
  Both wrap a single `_stb_gpu_sampler_create(filter)` platform
  primitive (Tonify Rule 20-clean — one parameterised
  implementation, two named entry points).
- `gpu_bind_texture_frag(slot, tex)` /
  `gpu_bind_texture_vert(slot, tex)` — bind an MTLTexture for
  fragment- or vertex-stage sampling.
- `gpu_bind_sampler_frag(slot, sampler)` — bind a sampler for
  fragment-stage sampling. Vertex-stage sampler binding deferred
  until a vertex shader actually needs it (no current call site).

Texture upload still goes through the Phase 1 `_stb_gpu_create_texture`
primitive — unchanged, no new code path. Linux platform layer
gains four matching no-op stubs so stbshader-using cartridges
link cleanly across both OSes.

**Smoke test:** `examples/gpu_pipeline_textured_smoke.bpp` +
`assets/shaders/textured.metal` — generates a 64×64 orange/purple
checkerboard, uploads as RGBA texture, draws across the window
through a custom pipeline that samples the texture with the
nearest-neighbour sampler. Compiles to a 314 KB binary; manual run
shows a sharp pixel-art checkerboard scaled to the window.

#### Session 2.3 — Tier 3b GPU timing (~150 LOC) — **#6 from sidequest queue** — **SHIPPED 2026-05-05**

Opt-in CPU wall-clock measurement around the GPU present path.
Three macOS platform primitives:

- `_stb_gpu_timing_enable()@gpu` / `_stb_gpu_timing_disable()@gpu` —
  flip a static flag. Idempotent.
- `_stb_gpu_last_us()@base` — last measured frame time in µs.

`_stb_gpu_present` checks the flag and conditionally wraps
`commit + waitUntilCompleted` with `_time_now_ns` deltas, storing
the result in microseconds for cheap integer reads. When the flag
is off the present path is unchanged (fire-and-forget commit).

stbprofile public API:

- `profile_gpu_enable()` / `profile_gpu_disable()` — wrappers.
- `profile_gpu_us()` / `profile_gpu_ms()` — readers.
- `profile_hud_draw` renders a "gpu: X ms" column to the right of
  "max:" on the existing FPS / ms / max line, conditional on a
  non-zero last reading (so the HUD silently degrades on Linux
  and before the first timed frame).
- `_profile_hud_dump` (stderr summary on profile-stop) appends a
  "GPU (last frame): X us" line under the rule, same conditional.

Linux platform layer gains 3 matching no-op stubs so stbprofile
links cleanly. Smoke test
`examples/gpu_timing_smoke.bpp` enables timing and logs the
measured µs to stderr every 60 frames.

**Implementation note — real GPU timeline, no wall-clock, no blocking.**
The first cut of this session shipped a CPU wall-clock workaround
under the assumption that B++ FFI had no return-double path. That
turned out to be wrong: the codegen for FFI extern calls
(`_a64_emit_call_extern` in `src/backend/chip/aarch64/a64_primitives.bsm`
and the x86_64 mirror) unconditionally saves `d0` and `d1` to scratch
slots after every `bl <extern>`, and the existing `float_ret()` /
`float_ret2()` builtins recover those slots as float-typed values.
The pattern is already used for NSPoint reads (`locationInWindow`
in this same platform layer). So `MTLCommandBuffer.GPUStartTime` /
`GPUEndTime` are recoverable today: call `objc_msgSend(buf,
sel_registerName("GPUStartTime"))`, then `t = float_ret()` returns
the CFTimeInterval as a B++ float (64-bit IEEE 754).

The shipping implementation reads the previous frame's GPU times
at present time of the current frame — by then the previous
frame's GPU work is overwhelmingly complete (16.6 ms between
presents at 60 FPS, typical GPU frames complete in under 5 ms).
No `waitUntilCompleted`, no CPU/GPU serialisation, no extra cost
in the present path beyond two objc_msgSend calls per frame.
When the previous buffer hasn't completed yet (rare at 60 FPS,
possible under heavy load), GPUStartTime returns 0 and the readout
sticks at its last good value.

**Why integrated here, not deferred:** building GPU pipeline without
timing is blind. Profile-driven decisions require GPU data from day 1.
HANDOFF said "skip until GPU bottleneck observed" — but that
recommendation pre-dates the decision to build full GPU stack.

#### Session 2.4 — fps_3d_gpu Sessão A1 (~150 LOC) — **SHIPPED 2026-05-06**

`games/fps/fps_3d_gpu.bpp` mirrors `fps_3d.bpp`'s WASD + arrow input,
FPSBody state, and 16×16 tilemap, but moves the per-column ray-cast
into a single fragment shader pass via stbshader's custom-pipeline
API. Per-frame the render phase packs camera state (`px, py, angle,
fov, sw, sh, mw, mh`, 32 bytes) into a uniform buffer and the map
(256 bytes, byte per cell) into a second buffer, binds the custom
pipeline, then issues `gpu_draw_full()` — one fullscreen triangle
that runs `fps_raycast.metal`'s DDA per pixel.

`assets/shaders/fps_raycast.metal` matches `stbraycast`'s CPU-side
shape: same 16-step DDA, same perpendicular-distance projection,
same E-W darkening for side==1, same wall colours per type id (1=
brick red, 2=stone grey, 3=wood brown, 4=magenta solid). Uniform
struct `CamUniforms` is byte-for-byte aligned with the B++ side.

`profile_gpu_enable()` is called at init so the HUD's "gpu: X ms"
column populates from frame two onward via Phase 2.3's MTLCommandBuffer
GPUStartTime / GPUEndTime path (read off the previous frame's command
buffer at present time, no waitUntilCompleted, no CPU/GPU
serialisation).

**Tonify hits during this session:**
- `tile_load_set` in `stb/stbtile.bsm` was annotated `@io` but does
  IO + GPU + HEAP work. Per Rule 4 the right shape is leave it
  unannotated (classifier infers SOLO). The fix surfaced a
  pre-existing diag-location bug — see Phase 2.4 sidequest below.

**Sidequest closed during 2.4 — diag location reporting fix
(file + line):** the W026 above initially reported `stb/stbpal.bsm:2462`
even though `tile_load_set` lives at `stb/stbtile.bsm:202`. Root
cause: `mod_idx` is wired into both the SEMANTIC pipeline (used by
`tok_mod`, `func_mods`, `lex_module`'s per-module token
partitioning, and the codegen's per-module emit loop) and the
DIAGNOSTIC pipeline (used by `diag_loc` / `diag_show_line` for
file/line display). The interleaved-import model means a parent
module's content can occupy multiple disjoint outbuf ranges with
children's content in the middle — `mod_idx` returns the
registration owner (which lex_module needs to stay consistent with)
not the actual byte owner. Naive "fix" of `mod_idx` was caught by
`bug --dump` comparison (gen1 emitted 1399 funcs, gen2 only 1220 —
179 silently dropped) before any production damage.

Architectural fix shipped: `mod_idx` stays driven by
`diag_file_starts` (semantic). New `diag_mod_idx` /
`diag_mod_line_off` consume a boundary list (`mod_bnd_off`,
`mod_bnd_mi`, `mod_bnd_line_off`) populated at `process_file`
entry and after every recursive `process_file` return. Per-frame
`_source_line` counter in `process_file` advances once per line-loop
iteration so the recorded line offset reflects the parent's progress
through its OWN source file, not the merged-source line counter.
`diag_loc` / `diag_show_line` switch to the new lookups; everything
else (parser, validator, dispatch, codegen) is untouched. Result:
W026 now reports `stb/stbtile.bsm:202` correctly. ~70 LOC across
`bpp_import.bsm` + `bpp_diag.bsm`.

**Gate:** `./bpp games/fps/fps_3d_gpu.bpp -o /tmp/fps_gpu` compiles
clean (366,498 bytes, zero warnings); manual run opens 640×480
window, walks via WASD + arrows, shows solid-colour walls + sky/floor
ceiling at 60 FPS; profile HUD's "gpu: X ms" populates after first
frame. fps_3d.bpp (CPU baseline) keeps compiling and running clean
for side-by-side comparison.

---

### Phase 3 — Sprite Batching GPU (~500 LOC, 3 sessions)

**Objective:** stbsprite gains GPU batching path, atlas loading, per-sprite tint.

#### Session 3.1 — Atlas + UV pipeline (~150 LOC) — **SHIPPED 2026-05-06**

New cartridge `stb/stbatlas.bsm` (carved out from the doc-original
"stbtexture extension" placement — same domain-per-cartridge
rationale as the Phase 2.1 stbshader split). `Atlas` struct
(`tex, tex_w, tex_h, tile_w, tile_h, cols, rows, count`); public
API `atlas_load`, `atlas_create_rgba`, `atlas_count`,
`atlas_tile_w`, `atlas_tile_h`, `atlas_draw`, `atlas_draw_tinted`,
`atlas_free`. UV math is inlined in the draw helpers — col/row
computed from sprite_id, then four corners projected into the
0–65535 fixed-point range that `_stb_gpu_sprite_uv` consumes.

stbtile's `tile_load_set` (legacy "split spritesheet into N
GPU textures" path) coexists unchanged; an option-B sweep deferred
post-Phase 3 will rewrite it to use Atlas internally and migrate
all callers (platformer, pathfind) — that work is tracked but
intentionally not bundled with the foundation.

**Smoke test:** `examples/atlas_smoke.bpp` builds a 64×64 procedural
atlas (4×4 grid, 16 distinct solid-colour 16×16 tiles), uploads it
once, then issues 16 `atlas_draw` calls with sz=2 to lay out the
tiles in a 4×4 grid on screen. Visually verifies UV math: sprite 5
must render the colour at (col 1, row 1), not a neighbour.

Compiles clean to a 348 KB binary; bootstrap byte-stable
(gen1 == gen2, 888,786 bytes — unchanged from Phase 2.4 baseline
because stbatlas only links into programs that import it).

#### Session 3.2 — Sprite batch renderer (~80 LOC + Linux parity) — **SHIPPED 2026-05-06**

The doc-original spec called for a new stbsprite cartridge with
explicit batch_begin / batch_draw / batch_end lifecycle and a
per-atlas vertex buffer. Investigation revealed the existing macOS
platform layer already has the batch infrastructure: `_gpu_vbuf`
collects vertices across draws, `_stb_gpu_flush` issues a single
drawPrimitives call per batch. The reason atlas_draw was NOT
batching was simpler: the sprite primitives auto-rebind the font
texture on exit (a leftover convention from when text+sprites
mixed every frame), and `_stb_gpu_bind_texture` always flushed +
re-issued setFragmentTexture even when the requested texture was
already bound. Two flushes per sprite by construction.

Phase 3.2 fixes both anti-batch behaviours in-place:

- **Smart-bind tracking** — new statics `_gpu_cur_slot0_tex` and
  `_gpu_cur_slot1_tex` in `_stb_platform_macos.bsm` track the
  currently-bound fragment textures at slots 0 (sprite/atlas) and
  1 (palette). `_stb_gpu_bind_texture` and `_stb_gpu_bind_palette`
  short-circuit when the requested texture matches the tracker —
  no flush, no setFragmentTexture, no Metal API cost. State-change
  sites that go around the bind helpers (`_stb_gpu_begin`,
  `_stb_gpu_pipeline_use_default`, `_stb_gpu_bind_texture_frag`)
  refresh the trackers themselves so the cache invalidates
  correctly across pipeline transitions.

- **No more auto-rebind-font** — `_stb_gpu_sprite`,
  `_stb_gpu_sprite_uv`, `_stb_gpu_sprite_uv_tint`, and
  `_stb_gpu_sprite_indexed` no longer call `_stb_gpu_bind_font()`
  on exit. `render_text` in stbrender now calls
  `_stb_gpu_bind_font()` at entry, which is free (smart-bind
  no-op) when text follows text and a single state change when
  text follows sprites.

Net effect: a stretch of N atlas_draw calls against the same
atlas presents as ONE flush per atlas per frame (or zero — Metal
flushes the final batch at presentDrawable time anyway), not 2N.

**Linux platform parity** — the same investigation surfaced a
pre-existing gap: most `_stb_gpu_*` primitives that stbrender /
stbtile / stbatlas call were never stubbed in
`src/backend/os/linux/_stb_platform_linux.bsm`. Programs importing
stbrender failed to link on Linux. Filled in: `_stb_gpu_init`,
`_stb_gpu_begin`, `_stb_gpu_clear`, `_stb_gpu_present`,
`_stb_gpu_rect`, `_stb_gpu_vertex`, `_stb_gpu_flush`,
`_stb_gpu_bind_texture`, `_stb_gpu_bind_palette`,
`_stb_gpu_bind_font`, `_stb_gpu_sprite_indexed`,
`_stb_gpu_create_palette_texture`,
`_stb_gpu_update_palette_texture`, `_stb_gpu_create_texture_r8`,
`_stb_gpu_upload_atlas` — all no-op stubs matching macOS
signatures, mirroring the existing convention for the GPU stub
suite. Linux code linking is unblocked; runtime GPU on Linux
remains pending the Vulkan-parity sidequest scoped post-Phase 7.

**Smoke test:** `examples/atlas_batch_smoke.bpp` draws 1024
sprites from a single 4×4 atlas per frame and logs `gpu: X us`
every 60 frames via `profile_gpu_us` (Phase 2.3 timeline).
Compiles clean to a 365 KB binary; the workload is one
`drawPrimitives` per frame on macOS.

**Why ~80 LOC instead of 250:** the doc's estimate was for a
build-it-from-scratch path. The existing infrastructure was 90 %
of the way there — Phase 3.2 was finishing what was already
mostly built, not green-fielding a batch renderer. The tonify
review surfaced that an `stbsprite` carve-out would have created
duplication with the existing platform-layer batch buffer; better
to fix the platform layer directly than to add a parallel API.

#### Session 3.3 — Validation: snake_gpu + fps_3d_gpu Sessão A2 (~100 LOC)

`examples/snake_gpu.bpp`: snake migrated to sprite batching GPU. Same
gameplay, GPU rendering. fps_3d_gpu A2: HUD overlay (ammo, health)
using sprite batch on top of ray cast pass.

**Gate:** snake runs 60fps GPU. fps_3d_gpu shows HUD non-corrupted
over walls.

#### Cross-reference: multi-core dispatch

Per-sprite vertex generation in batch builder is a `for` loop scanning
sprite list. **If the loop body qualifies under Stage 1 criteria
(`multicore_state_report.md` Section 1)**, smart dispatch may
auto-parallelize it via `job_parallel_for`. Worth profiling at large
sprite counts to see if Stage 1 picks it up. No explicit annotation
needed — system is opt-in via effect classification.

---

### Phase 4 — Pixel-Perfect Render Pipeline (~810 LOC, 8 sessions)

**Objective:** Enforce architectural split (`stbgame` = games, `stbwindow`
= tools) AND ship pixel-perfect rendering as DEFAULT for stbgame.

**Restructured 2026-05-06** after audit revealed `stbgame` is currently
shared by both games AND tools (modulab, mini_synth, sprite_viewer,
level_editor, the_bug, bang9 all import stbgame and call game_init).
Pixel-perfect default in stbgame would silently break every tool.

The plan now is two-stage:
- **Phase 4.0 (Faxina, ~260 LOC, 4 sessions):** Migrate tools from
  `stbgame` to `stbwindow`. After this, `stbgame` consumers = ONLY
  games (all pixel-art).
- **Phase 4.1+ (Pixel-perfect default ON, ~550 LOC, 4 sessions):** Now
  safe to make `stbgame` pixel-perfect by default + ship render-to-texture
  + integer scaling.

---

### Phase 4.0 — Faxina: enforce stbgame/stbwindow split (~260 LOC, 4 sessions)

**Objective:** `stbwindow` gains parallel convenience layer to current
`stbgame` shape (minus game-specific timing/maestro hooks). All tools
migrate to `stbwindow`. Rule 23 enforced.

#### Pre-flight 4.0

- Confirm ownership of `_stb_fb`, `_stb_dt_ms`, `_stb_last_time` globals.
  Likely owned by `_stb_platform_<os>.bsm`. If owned by stbgame init
  logic, refactor to platform layer first so both cartridges can read
  via extrn without conflict.
- Audit which tools call `game_init` vs `window_open`:
  ```bash
  grep -l "game_init\|game_run" tools/*/*.bpp tools/*/*.bsm bang9/*.bpp bang9/*.bsm
  ```
  Expected hits: bang9, modulab, modulab_lib, mini_synth, sprite_viewer,
  level_editor, the_bug, font_forge.

#### Session 4.0.1 — `stbwindow` convenience layer (~80 LOC)

Add to `stb/stbwindow.bsm`:

```bpp
// Mirror of game_init MINUS game-specific concerns (no fps target,
// no maestro hooks, no pixel-perfect virtual res). Tools own their
// pacing, render at native resolution. Same 11-step subsystem init
// that game_init does today, plus framebuffer allocation.
void window_init_full(w, h, title) @io {
    init_math(); math_trig_init();
    init_input(); init_color(); init_draw();
    init_font(); init_ui(); init_str();

    _stb_fb = malloc(w * h * 4);   // shared with stbgame; refactor if
                                    // owned by stbgame init currently
    _stb_w = w; _stb_h = h;

    _stb_init_window(w, h, title);
    _stb_last_time = _stb_get_time();
}

// Per-frame poll + dt update. Replaces `game_frame_begin()` for tools.
void window_frame_step() @io {
    auto now;
    window_poll_events();
    now = _stb_get_time();
    _stb_dt_ms = now - _stb_last_time;
    _stb_last_time = now;
}

// dt accessors mirror game_dt_ms / game_dt
window_dt_ms() @base { return _stb_dt_ms; }
window_dt() @base { return _stb_dt_ms * 0.001; }
```

Bootstrap byte-stable. Smoke: standalone test creates window, polls,
draws via stbdraw, presents, closes.

#### Session 4.0.2 — Migrate batch 1: bang9 + modulab (~80 LOC editing)

Sources:
- `bang9/bang9.bpp`
- `tools/modulab/modulab.bpp`
- `tools/modulab/modulab_lib.bsm`

Each migration is mechanical replacement:
```bpp
// Before:
import "stbgame.bsm";
game_init(W, H, title, fps);
while (...) { game_frame_begin(); ... }
game_end();

// After:
import "stbwindow.bsm";
window_init_full(W, H, title);
while (window_should_close() == 0) {
    window_frame_step();
    ... draw ...
    window_present();
}
window_close();
```

Bootstrap byte-stable per tool. Each runs identically post-migration.

**Critical validation:** bang9 boots + Modulab panel embedded continues
working (modulab_lib_init / modulab_lib_frame / modulab_lib_shutdown
called from bang9's own loop unchanged).

#### Session 4.0.3 — Migrate batch 2: remaining tools (~80 LOC editing)

Sources:
- `tools/mini_synth/mini_synth.bpp`
- `tools/sprite_viewer/sprite_viewer.bpp`
- `tools/level_editor/level_editor.bpp` (already imports stbwindow,
  drop stbgame)
- `tools/the_bug/the_bug.bpp` (mixed mode, clean up)
- `tools/font_forge/txt_to_modulab.bpp` (verify usage)

Same mechanical replacement. Bootstrap byte-stable per tool.

#### Session 4.0.4 — Verification + Rule 23 + docs (~40 LOC)

```bash
# Audit lock: zero tools import stbgame
grep -l "stbgame" tools/*/*.bpp tools/*/*.bsm bang9/*.bpp bang9/*.bsm
# Expected: empty

# All tools run standalone
./bang9 && exit 0
./modulab && exit 0
./mini_synth && exit 0
./sprite_viewer && exit 0
./level_editor && exit 0

# Bang 9 panels with embedded tools work
./bang9   # open Modulab panel — should render
```

Docs:
- `tonify_checklist.md` — **Rule 23 NEW**: "Tools use `stbwindow`,
  games use `stbgame`. The split mirrors Rule 22's pattern of
  enforcement via cartridge boundaries: `stbgame` is for games (60fps
  loop + maestro + pixel-perfect default after Phase 4.1), `stbwindow`
  is for tools (window + events + manual loop, native resolution).
  Mixed imports drift the architecture; resist."
- `stbwindow.bsm` header expanded with `window_init_full` example
- `journal.md` Phase 4.0 closeout entry with migration list + bpp hash
- `gpu_pipeline_roadmap.md` (this doc) marked Phase 4.0 SHIPPED

**Gate:** Phase 4.0 closed when zero tools import stbgame, all tools
run identically, Bang 9 + embedded tools render, suites green,
bootstrap byte-stable.

---

### Phase 4.1.1 + 4.1.2 — CLOSED 2026-05-07

Phase 4.1 split into shippable sub-phases during execution.

**Phase 4.1.1 — Offscreen render-to-texture infrastructure (SHIPPED).**
`gpu_target_create / bind`, `gpu_present_target`, `gpu_draw_quad`,
`_stb_get_monitor_width / height`, the lazy-loaded `pp_blit`
shader at `assets/shaders/pp_blit.metal`. Linux backend ships
stubs with cross-OS contract comments. Validated end-to-end via
`examples/render_target_smoke.bpp`.

**Phase 4.1.2 — Smart-dispatch `render_clear` + multi-pass `_gpu_vbuf`
race fix (SHIPPED).** Single `render_clear(color)` API picks
state-set vs inline-quad based on `_gpu_in_pass` flag (Tonify
Rule 24). Runtime knows the dispatch state — no need for the
OpenGL/SDL idiom of separating `glClearColor` from `glClear`.

The same session uncovered a latent buffer-race bug:
`_gpu_flush_off` reset to 0 per pass instead of per frame, so
multi-pass scenarios overwrote bytes the GPU was still reading
async. Fixed by accumulating `_gpu_flush_off` across passes
within a frame (Pass 1 owns `[0..N1]`, Pass 2 owns `[N1..N2]`,
reset to 0 at window-pass present). See journal 2026-05-07 +
`tonify_checklist.md` Pitfall 7.

**Phase 4.1.3 — stbgame `game_init` virtual_w/h reinterpretation
(SHIPPED 2026-05-07).** `game_init(W, H, title, fps)` reinterprets
`(W, H)` as virtual rendering resolution. Window opens at the
largest integer scale fitting 80% of the monitor's visible frame
(sub-1× clamp so the canvas is never bilinear-shrunk). Three
opt-out APIs land alongside (`game_set_window_scale`,
`game_set_window_size`, `game_set_letterbox_color`), all
pre-`game_init`. `game_window_w / h` switched to read live
window dims (`_stb_win_w / _h`) so multi-pass blit math stays
correct at any non-1× scale; `SCREEN_W / H` remain the virtual
resolution. Existing games (snake, pathfind, fps_3d, etc.)
auto-scale with zero source change. See journal 2026-05-07.

**Phase 4.1.4 — Auto pixel-perfect render orchestration
(SHIPPED 2026-05-07).** Two deliveries closed the pixel-perfect-
default-on contract:

1. **Software path (auto, macOS):** `_stb_init_window` configures
   the NSImageView's backing CALayer with
   `magnificationFilter = "nearest"` and `setWantsLayer:YES`.
   Every software-rendered game (snake, pathfind, anything that
   writes into `_stb_fb`) now upscales pixel-art crisp at any
   integer scale. Linux ships a contract comment at
   `_stb_present` documenting the same NEAREST requirement
   for the future Vulkan / X11 hardware path.
2. **GPU path (opt-in helpers, cross-platform):**
   `game_render_begin` / `game_render_end` in stbgame abstract
   the "render to virtual canvas + auto-blit to window with
   NEAREST and letterbox" pattern that the Phase 4.1.2 smoke did
   manually. `game_render_begin` lazy-creates the offscreen
   target sized to `SCREEN_W × SCREEN_H`; `game_render_end`
   commits the virtual pass, opens a window pass, clears the
   letterbox colour, blits via `gpu_present_target`, and
   presents. Cross-platform by construction — uses only the
   abstract `gpu_target_*` API surface.

`render_init` made idempotent. `game_init` auto-calls it after
`_stb_init_window` so users opting into `game_render_begin / end`
don't have to remember the pre-init step. Existing explicit
calls in current games are no-ops.

Visual confirmed at default and resized window: BLACK letterbox
appears on aspect mismatch, pixel-art stays crisp at any
integer scale.

---

### Phase 4.1 — Pixel-perfect default ON in stbgame (~150 LOC, 1 session)

**Objective:** Now that all stbgame consumers are pixel-art games, flip
the default. Game's `game_init(W, H, title, fps)` reinterprets `(W, H)`
as **virtual resolution**. Window auto-scales to monitor.

`stb/stbgame.bsm` changes:

```bpp
// Reinterpret game_init args:
void game_init(virtual_w, virtual_h, title, fps) {
    // ... existing 11-step subsystem init ...

    // Pixel-perfect ON: virtual_w/h is the rendering resolution.
    // Window opens at max integer scale that fits 80% of monitor.
    auto monitor_w = _stb_get_monitor_width();
    auto monitor_h = _stb_get_monitor_height();
    auto scale_w = (monitor_w * 8 / 10) / virtual_w;     // 80% width
    auto scale_h = (monitor_h * 8 / 10) / virtual_h;
    auto scale = scale_w < scale_h ? scale_w : scale_h;
    if (scale < 1) { scale = 1; }                        // never sub-1

    SCREEN_W = virtual_w;
    SCREEN_H = virtual_h;
    _stb_w = virtual_w;
    _stb_h = virtual_h;
    _stb_window_w = virtual_w * scale;
    _stb_window_h = virtual_h * scale;

    _stb_fb = malloc(virtual_w * virtual_h * 4);    // virtual size
    _stb_init_window(_stb_window_w, _stb_window_h, title);
    // ... rest unchanged ...
}

// Optional override APIs:
void game_set_window_scale(s) @io { ... }   // force specific scale
void game_set_window_size(w, h) @io { ... } // force specific size, with letterbox
void game_set_letterbox_color(rgba) @io { ... }  // default BLACK
```

New platform primitives:
- `_stb_get_monitor_width()` / `_stb_get_monitor_height()` — Cocoa
  `[NSScreen mainScreen visibleFrame].size` on macOS, X11 query on
  Linux (or stub returning 1920×1080 fallback).

**Backward compat verified:** existing games' `game_init(320, 180, ...)`
calls automatically interpret 320×180 as virtual. Window opens 4x
scaled (1280×720 on 1512×982 M4 logical). Snake/pathfind/fps_3d/etc.
require ZERO source changes.

**Gate:** all games open at appropriate window size, render at virtual
res, integer scaled. No blur. Letterbox where aspect doesn't match.

---

### Phase 4.2 — Render target + offscreen pipeline (~200 LOC, 1 session)

**Owner: `stbshader.bsm` (extends, not creates a new cartridge).**
Render target is pipeline configuration — same domain as shader
load/uniform binding from Phase 2.1. Adding `gpu_target_*` to a
separate cartridge would split related concerns.

Off-screen `MTLTexture` as render target. New API in stbshader:
```bpp
auto target = gpu_target_create(320, 240);  // offscreen MTLTexture
gpu_target_bind(target);                    // route subsequent draws
render_clear(BLACK);
// ... game world draws ...
gpu_target_present(target);                 // finalize for sampling
gpu_target_bind(0);                         // back to screen
```

Backend: macOS creates 2nd `MTLRenderPassDescriptor` pointing at the
texture. Linux no-op stubs in `_stb_platform_linux.bsm` (sibling to
existing GPU stubs).

stbgame ties the offscreen target to virtual_w/h from Phase 4.1: every
game frame renders to offscreen virtual surface, then final pass blits
to window with integer scale + letterbox.

**Smoke test:** `examples/render_target_smoke.bpp` — draws checkerboard
to offscreen 320×240, blits to window, visually confirms NEAREST
sampling at 4x scale (sharp, not blurred).

---

### Phase 4.3 — Integer scaling final pass + letterbox (~150 LOC, 1 session)

Final blit pass: textured fullscreen quad with nearest-neighbor
sampling. Integer scale factor calculated in Phase 4.1's
`game_set_window_scale` logic. Letterbox bars for non-multiple ratios
(BLACK default, `game_set_letterbox_color(rgba)` opt-out).

stbgame's `game_frame_end` (or auto-tick from frame_begin) executes
the offscreen→window blit pass.

**Gate:** every game's window resizes preserving pixel-perfect.
Letterbox visible where aspect mismatches. Toggle key (debug) cycles
scales for verification.

---

### Phase 4.4 — Validation across games + journal (~50 LOC)

All games confirmed pixel-perfect:
- snake_maestro at 320×180 virtual → 4x window
- pathfind at 320×180 virtual → 4x window
- fps_3d_gpu at 320×240 virtual → 3x window (or 4x letterboxed)
- platformer at 320×180 virtual → 4x window
- rhythm at 320×180 virtual → 4x window

**Dev/debug toggle in fps_3d_gpu**: `P` key cycles through 1x / 2x /
3x / auto window scales for visual verification of pixel-perfect
preservation across sizes.

Profile HUD shows pixel-perfect overhead (one fullscreen quad blit per
frame ≈ negligible on M4).

Journal entry: Phase 4 closeout with bpp hash, suite counts, before/
after screenshots noted, decision log entry confirming pixel-perfect
shipped.

**Gate:** every game in repo runs pixel-perfect by default. fps_3d_gpu
demonstrates GPU pipeline + pixel-perfect together.

### Bonus capability inherited from Phase 3.5: shader hot-reload

The `file_watch_register` infrastructure shipped in Phase 3.5 enables
**live shader reload** as a follow-up sidequest after Phase 4 closes:

```bpp
file_watch_register("assets/shaders/wall.metal",
                    fn_ptr(reload_wall_shader));
```

Edit `.metal` file in any editor → game re-compiles MTLLibrary live,
swaps pipeline state, no restart needed. Same Unity/Bevy-class
workflow Phase 3.5.5 brought to images, now extending to shader code.

Estimated ~50 LOC sidequest. Not part of Phase 4 scope, but
architectural enabler is already in place.

---

### Phase 5 — Layered Backgrounds + Parallax (~300 LOC, 2 sessions)

**Objective:** stbscene cartridge for layered rendering with parallax
offsets.

#### Session 5.1 — stbscene basics (~200 LOC)

New cartridge `stb/stbscene.bsm`. Scene struct: array of layers
(background, midground, sprites, foreground, etc.). Per-layer texture,
scroll offset (x, y), z-order. Render pass: iterate layers back-to-
front, draw each as textured quad with offset uniform. Fragment shader:
simple texture sample with offset.

#### Session 5.2 — adventure_demo_min + integration (~100 LOC)

`examples/adventure_demo_min.bpp`: simple scene with 3 layers
(sky/midground/foreground), parallax via mouse hover or input.
Validates parallax math: foreground moves more than background per
input.

**Gate:** scene renders, parallax visible when user moves.

---

### Phase 6 — Effect Library + Scoped Zones (~550 LOC, 4 sessions)

**Objective:** stbfx cartridge composable + #7 scoped zones compiler
feature (Tier 3c).

#### Session 6.1 — stbfx cartridge basics (~150 LOC)

New cartridge `stb/stbfx.bsm`. Effect struct: fragment shader +
uniform setup function + apply API. Effect chain: array of effects
applied in sequence (each writes to next render target). Catalog of
effects: empty `_register_effect_*` slots awaiting fill.

#### Session 6.2 — Effect library catalog (~200 LOC)

- `effect_crt(intensity)`: scanlines + barrel distortion + chromatic
  aberration
- `effect_palette_quantize(palette_id)`: snap each pixel to nearest
  palette color (uses stbpal)
- `effect_dither(pattern)`: ordered dither matrix
- `effect_scanlines(density)`: simple horizontal lines
- `effect_chromatic(offset)`: RGB channel split

Each ~30-40 LOC: shader + uniform setup.

#### Session 6.3 — #7 Scoped zones compiler feature (~150 LOC) — **from sidequest queue**

Lexer: `@` prefix support (TK_AT — confirm if exists from Session 1's
TK_ARROW addition era). Parser: `@profile_zone("name") { ... }`
annotation preceding block. Codegen: emit `_prof_zone_enter(name_p)`
in block prologue, `_prof_zone_exit(name_p)` in epilogue.
- Handle early return: cleanup zone if function returns mid-block
- Handle panic: zone cleanup via panic handler

stbprofile: zone-aware aggregation in `profile_dump` (zones appear as
entries in top-N). HUD profiler: zone breakdown line below top-N
functions. Gate test: profile zone "render_pass" appears in top-N.

**Cross-reference:** Tonify Rule 21 (or new Rule 22) addition for `@`
annotations. Update `tonify_checklist.md`.

#### Session 6.4 — fps_3d_gpu Sessão A4 + final polish (~50 LOC)

fps_3d_gpu applies CRT effect via stbfx. Scoped zones around each
pipeline pass: `@profile_zone("ray_cast")`, `@profile_zone("hud_render")`,
`@profile_zone("crt_effect")`. Profile HUD shows each zone separately.

**Gate:** fps_3d_gpu with CRT visual, profile shows ray_cast /
hud_render / crt_effect timings as separate entries.

---

### Phase 7 — Closeout + Decision Point (~100 LOC + content)

**Objective:** Validate stack end-to-end, decision on next arc
(Wolf3D content vs adventure demo vs RTS).

#### Session 7.1 — Comparison + journaling (~100 LOC content)

Profile fps_3d (CPU baseline) vs fps_3d_gpu (full GPU stack) under
identical conditions. Document trade-offs: GPU usage, frame time,
memory, code size. Update `journal.md` with GPU pipeline closeout,
capabilities shipped. Update `games_roadtrip.md` reflecting new
ordering: foundation done, content arcs available.

#### Session 7.2 — Decision point

You decide:
- **Wolf3D content** (sprites + enemies + audio + doors): ~7 sessions,
  uses Phase 3 sprite batching + Phase 5 layered (HUD)
- **Adventure demo** (Thimbleweed-flavored): ~10 sessions, uses Phase
  5 parallax + Phase 6 effects (watercolor)
- **RTS content**: uses Phase 3 sprite batching + Phase 4 pixel-perfect
  (zoom)
- **Other**: platformer, etc.

Decision deferred to Phase 7 close — enabled by Phases 2-6 work. Plan
not committed today.

---

## Total scope

| Phase | Sessions | LOC |
|---|---|---|
| Phase 2 — GPU Foundation + Timing (#6) | 4 | ~700 |
| Phase 3 — Sprite Batching | 3 | ~500 |
| Phase 4 — Pixel-Perfect | 3 | ~400 |
| Phase 5 — Layered + Parallax | 2 | ~300 |
| Phase 6 — Effects + Scoped Zones (#7) | 4 | ~550 |
| Phase 7 — Closeout + Decision | 2 | ~100 |
| **Total** | **18 sessions** | **~2550 LOC** |

At B++ velocity (V3 + 8 sidequests in one day, 2026-05-05), estimated
**5-8 work-days focused**, with strategic pauses between phases.

---

## Cross-cutting concerns

### stbrender CPU/GPU coexistence

stbrender today has CPU layer + GPU upload final blit. After Phase 2,
gains **two paths**:
- `render_layer_*` (CPU pixel buffer, current)
- `render_gpu_*` (vertex+fragment pipeline, new)

Existing games (using `render_layer_*`) continue working unchanged.
GPU migration is per-game opt-in. Phase 3 validates with `snake_gpu`
(alternative version), does not replace `snake.bpp`.

Long-term: if GPU path proves superior, CPU path becomes
`render_legacy_*` and eventually deprecates. But that decision is
post-Phase 7+.

### Cross-platform Linux Vulkan

Each phase adds Metal-specific code. Linux GPU primitives remain stubs
(pre-existing gap). **Sidequest separate post-Phase 7**:
`stb/_stb_gpu_linux_vulkan.bsm` parity, parallel to Metal layer.
Estimated ~1500 LOC dedicated.

Does not block any phase — Phases 2-7 ship on macOS, Linux remains
"cross-compile but doesn't run GPU" until parity arrives.

### Asset pipeline robustness

Phase 2+ depends heavily on filesystem reads (`.metal` shaders,
`.png` atlases, `.json` sprite metadata). stbasset existing has PNG
load. Add as needed:
- `.metal` shader file load (string read, pass to MTLLibrary)
- Atlas grid metadata `.json` or inline in code first to not block

Graceful errors: "shader X not found, falling back to default" rather
than hard panic.

### Validation strategy: fps_3d_gpu evolves per phase

| Phase | fps_3d_gpu state |
|---|---|
| 2 | Solid color walls (validates pipeline) |
| 3 | + HUD sprite overlay (validates batching) |
| 4 | + 320×240 internal scaled (validates pixel-perfect) |
| 5 | + skybox layer (validates layered, weak parallax case) |
| 6 | + CRT effect + scoped zones (validates effects + compiler) |
| 7 | Polished comparison vs CPU baseline |

Each phase increments fps_3d_gpu. **Visualizable progress from Phase
2.4 onward** (~Day 1-2 at B++ pace).

---

## Cross-references

### `multicore_state_report.md` — complementary parallelism

CPU-side parallelism (smart dispatch, `job_parallel_for`,
auto-parallelization) is **orthogonal to GPU pipeline**. Both leverage
parallel hardware:
- Multi-core: CPU game logic (entity update, AI, physics)
- GPU: rendering work (shader execution, pixel processing)

**Where they interact:**
- Phase 3 sprite batching: per-sprite vertex generation loop may
  qualify for Stage 1 auto-promotion if effect classification deems
  it pure. Profile at large sprite counts.
- Phase 4 multi-pass: each pass is sequentially dependent on previous,
  not auto-parallelizable.
- Phase 6 effects: same as multi-pass — sequential by construction.

**No conflict, no overlap of effort.** GPU pipeline does not block or
get blocked by multi-core roadmap items in `multicore_state_report.md`
Section 5.

### `compiler_boost_roadmap.md` — Tier F deferred, profile-driven

GPU pipeline does not gate on Tier F (CSE, register allocator v2,
inline-aware specialization, etc.). Tier F is CPU scalar math
optimization.

**Where they interact:**
- Phase 7 profile data may surface CPU-side bottlenecks (vertex buffer
  building, command encoder construction, atlas iteration). If
  bottleneck is scalar math heavy, that's the trigger for opening
  specific Tier F item per the "profile-driven" rule.
- Most likely scenario: Phase 7 shows GPU is dominant, CPU is idle,
  Tier F remains deferred.

### `games_roadtrip.md` — needs update post-Phase 7

Original ordering: Rhythm → Wolfenstein → RTS. Phase 2 redirection
inserts foundation phases between Phase 1 (fps_3d CPU) and Wolf3D
content (now Phase 8+).

Updated ordering proposal (commit at Phase 7 close):
```
Phase 1 (DONE): fps_3d.bpp CPU baseline
Phase 2: GPU foundation (this doc)
Phase 3: Sprite batching GPU (this doc)
Phase 4: Pixel-perfect render pipeline (this doc)
Phase 5: Layered + parallax (this doc)
Phase 6: Effects + scoped zones (this doc)
Phase 7: Decision point (this doc)
Phase 8+: Wolf3D content / Adventure demo / RTS / other (TBD)
```

Wolf3D content as Phase 8 = "first complete game on top of GPU stack."
Adventure demo as Phase 8.5 = "second flavor on same stack." Both
validate same engine.

### `bug_viz_plan.md` — already shipped, consumes scoped zones

**Status verified 2026-05-05**: bug_viz_plan is closed (Phases 1-6
shipped, Stages 6.1 → 6.4.2 all landed per journal entry on
2026-05-04). The bug debugger has full visualization infrastructure:
TUI REPL, async observer, watch panels, viz_format_panel for typed
strbuf formatting, graphical visualizers via `@viz:` annotations.

**Integration with Phase 6.3 scoped zones (this roadmap):** when
scoped zones ship, the existing bug debugger panel framework can
display zone breakdown alongside watch/locals. No new bug debugger
work required — scoped zones become a new data source consumed by
the existing visualization stack.

Concretely: `bug --tui game.bug` running attached to a binary with
scoped zones populated would let user `v zones` (extending the `v
graph/vec2` command pattern from Phase 5 step 1) and see live
per-zone timing alongside watched variables.

This is **future enrichment of a shipped system**, not new debugger
work. Scoped zones (Phase 6.3 of GPU pipeline roadmap) is the only
new piece; bug debugger consumes it for free.

### `how_to_dev_b++.md` — Cap on GPU pipeline (post-Phase 7)

After Phase 7 close, add a Cap to the dev manual covering:
- vertex+fragment pipeline usage
- shader file format (`.metal` for now)
- sprite batching API
- pixel-perfect render setup
- effect chain composition
- scoped zones annotation

Estimated 200-300 lines of doc, lands in Session 7.1 closeout.

### `tonify_checklist.md` — Rule 22 candidate

Phase 6.3 introduces `@profile_zone("name") { ... }` annotation.
First instance of `@`-prefixed annotation in B++ that's not a phase
annotation (`@io`, `@base`, etc. are already there).

Rule 22 candidate: "annotation prefixes — `@phase_name` for compiler
phase annotations, `@profile_zone` for instrumentation, future `@poly`
for polymorphic registry opt-out (V3 backlog), etc. Reserve `@` prefix
for compiler-aware annotations only."

### `warning_error_log.md` — new diagnostics

Reserve at minimum:
- W030 (Phase 6.3): scoped zone within @gpu region — confirm safe
- E250-E255 (Phases 2-6): GPU-related diagnostics (shader load failure,
  pipeline state mismatch, atlas size mismatch, etc.)

Add entries as each phase emits new diagnostic.

### `journal.md` — closeout per phase

Each phase closeout writes a journal entry with date, hash, suite
counts, sessions shipped, capabilities enabled, next phase pointer.
Pattern matches V3 Sessions 0-5 closeout (2026-05-05 entry).

---

## Risks (filtered — only the real ones)

| Risk | Mitigation |
|---|---|
| Metal-specific code grows linearly per phase | Cross-platform Linux Vulkan sidequest budgeted, not blocking |
| Bootstrap not byte-stable after codegen change (Phase 6.3) | 1-cycle accepted for parser change; debug if 2-cycle |
| Effect chain rendering performance | Multi-pass overhead on mobile/old GPU; M4 trivial. Profile measures. |
| stbsprite GPU/CPU path divergence | Coexistence by design; tests cover both |
| Asset load failures (missing `.metal` files) | Graceful error with clear message: "shader X not found, falling back to default" |
| fps_3d_gpu regresses vs CPU baseline at some phase | Profile each phase, compare. Fallback is shipping CPU path in parallel. |
| Phase 6.3 scoped zones compiler bug | Dedicated session (not warm-up); strict bootstrap byte-stable check |
| Cross-platform Linux developers find broken GPU on this branch | Documented as macOS-only until Vulkan parity sidequest. README + journal note. |
| Asset organization sprawl (many .metal, .png files) | Convention locked: `assets/shaders/*.metal`, `assets/sprites/*.png`, `assets/atlases/*.json` |

---

## Sequence of attack

Each session: pre-flight grep + bootstrap byte-stable per step + suite
green + gate.

**Today's session**: starts Phase 2.1 (pipeline scaffolding). Foundation
first. fps_3d_gpu A1 emerges in Phase 2.4 — first "GPU sees walls"
milestone.

**Strategic pauses**: between each phase, journal entry + memory
snapshot + decision whether to continue immediately or pause for other
work. User controls pulse.

**No promised polished fps_3d_gpu until Phase 6.4**. But visualizable
progress from Phase 2.4 onward.

---

## Validation gate — V Plan complete (post-Phase 7)

```bash
# Bootstrap idempotent
shasum ./bpp /tmp/bpp_gen1 /tmp/bpp_gen2     # 3 hashes identical

# Suites
bash tests/run_all.sh        # all green, expanded count
bash tests/run_all_c.sh      # idem

# All examples run
./bpp examples/fps_3d.bpp -o /tmp/fps_cpu && /tmp/fps_cpu        # CPU baseline
./bpp examples/fps_3d_gpu.bpp -o /tmp/fps_gpu && /tmp/fps_gpu    # GPU full stack
./bpp examples/snake_gpu.bpp -o /tmp/snake_gpu && /tmp/snake_gpu # sprite batching valid
./bpp examples/adventure_demo_min.bpp -o /tmp/adv && /tmp/adv    # layered + parallax

# Profile comparison
/tmp/fps_cpu  → profile shows cast_ray dominant CPU
/tmp/fps_gpu  → profile shows GPU work in scoped zones, CPU near-idle

# Capabilities checklist — all covered
# - GPU pipeline shipped (Phase 2)
# - Sprite batching shipped (Phase 3)
# - Pixel-perfect shipped (Phase 4)
# - Layered + parallax shipped (Phase 5)
# - Effect library shipped (Phase 6.1-6.2)
# - Scoped zones shipped (Phase 6.3 — Tier 3c, sidequest #7)
# - GPU timing shipped (Phase 2.3 — Tier 3b, sidequest #6)
```

Phase 7 closeout journal entry confirms all of above + decision point
on next content arc.

---

## What is NOT in this roadmap

- **Compute shader pipeline** (stbcompute cartridge): backlog, no use
  case in current games_roadtrip
- **Linux Vulkan parity**: separate sidequest, ~1500 LOC, post-Phase 7
- **Tier F CPU compiler optimizations**: deferred per
  `compiler_boost_roadmap.md`, profile-driven
- **Wolf3D content (sprites + enemies + audio + doors)**: Phase 8+,
  uses this stack as foundation
- **Adventure game demo**: Phase 8+, uses this stack as foundation
- **stbrender CPU path deprecation**: post-Phase 7+, decision deferred
- **@poly annotation**: V3 backlog, not gating
- **Stack-arg routing for fn-types >8 args**: V3 cap, not gating

---

## Decision log

| Date | Decision | Rationale |
|---|---|---|
| 2026-05-05 | Phase 2 redirected from Wolf3D content to GPU foundation | Engine-completeness > content-first for B++ identity |
| 2026-05-05 | Approach A (fragment shader fullscreen quad) over Approach B (compute shader per-column) | Industry standard for 2.5D ray casting, universal foundation for 2D/2.5D, no stbcompute prerequisite |
| 2026-05-05 | #6 GPU timing integrated into Phase 2 (not deferred) | Building GPU pipeline without timing is blind; profile-driven decisions need GPU data day 1 |
| 2026-05-05 | #7 Scoped zones integrated into Phase 6 (not deferred) | Multi-pass rendering in Phase 4-6 makes scoped zones immediately useful for instrumenting pipeline phases |
| 2026-05-05 | Wolf3D content moves to Phase 8+ | Subset of pixel-art capabilities enabled by Phases 2-6 |
| 2026-05-05 | stbrender CPU path coexists with GPU path | Backward compatibility; per-game opt-in migration |
| 2026-05-05 | Linux Vulkan parity as separate sidequest, ~1500 LOC | Metal ships now; cross-platform parity scoped separately |
| 2026-05-05 | Phase 2.1 carved into NEW `stb/stbshader.bsm` instead of stbrender extension | Pipeline management is a distinct domain from draw primitives; matches stbpal / stbtexture / stbtile cartridge pattern. Extension placement would have inflated stbrender across Phases 3–6 with code that does not belong there. Carve-out cost in 2.1 was ~10 minutes; deferred carve-out at Phase 6 close would have been a multi-file refactor. |
| 2026-05-05 | Linux platform layer gains stub `_stb_gpu_pipeline_*` primitives upfront | Keeps the public stbshader API portable at the link level on day 1. No-op runtime behavior matches the existing Linux GPU stub story (`_stb_gpu_create_texture`, etc.) until a real Vulkan / DX12 / WebGPU backend lands. |

---

**Status:** Phase 3 (3.1 + 3.2 + 3.3) **CLOSED** 2026-05-06.
Bootstrap byte-stable across every session; latest 3-way shasum
verified `./bpp` == gen1 == gen2 (`a01c8e724df...`); suites
140/0/12 native + 114/0/38 C unchanged from baseline. Atlas API
validated in production: `snake_maestro` migrated end-to-end (30+
per-frame `atlas_draw_size` calls coalesce into one drawPrimitives
via Phase 3.2 smart-bind), `fps_3d_gpu` Sessão A2 ships HUD overlay
(crosshair + heart) on top of the ray-cast pass via mixed-pipeline
transition (custom raycast → default → atlas → font). Zero
warnings across both games; pre-existing `tile_load_set` /
`asset_hot_reload` W026 violations cleared in the same sweep.

#### Session 3.3 — Validation (~250 LOC + reorg work) — **SHIPPED 2026-05-06**

Two production migrations + folder reorg:

- `games/snake/snake_maestro.bpp` migrated: `render_rect` for
  apple/body/head replaced by `atlas_draw_size`. New
  `games/snake/snake_sprites.bsm` shared module owns the
  procedural atlas (4-tile sprite16-style: body, head, dead, apple)
  + the per-pixel painters (`_snake_put_px`,
  `_snake_fill_tile_bordered`, `build_snake_atlas`). Tonify Rule 20
  promotion since both `snake_gpu.bpp` (rect baseline, kept) and
  `snake_maestro.bpp` (atlas-using) consume the same sprite layout.
- `games/snake/snake_gpu.bpp` reverted to its original rect-only
  baseline and moved to `examples/snake_gpu.bpp` — preserved as the
  minimal "render_rect snake" template; the atlas-using version is
  `snake_maestro` now.
- `examples/` folder restored from HEAD (19 files were
  filesystem-deleted in a prior session, never committed; restored
  via `git restore examples/` on user direction).
- Driver folder rename: `drivers/` → `ffi/`. Both
  `src/bpp_import.bsm` (search paths) and `install.sh` updated to
  reflect the new name; bootstrap byte-stable through the rename.
  `ffi/examples/` cleaned of 12 duplicate non-FFI examples that had
  been mis-moved from `examples/`; only the 7 actual FFI demos
  (raylib + SDL2 backed) remain there as proof-of-FFI artifacts.
- `games/fps/fps_3d_gpu.bpp` Sessão A2 — HUD overlay drawn after
  `gpu_pipeline_use_default()` restores the default pipeline,
  validates the mixed-pipeline + mixed-texture state transitions
  (custom raycast pipeline → default pipeline → HUD atlas slot 0
  → font slot 0 for `profile_hud_draw`).

**Tonify hits during 3.3:**
- `_asset_reload_slot` + `asset_hot_reload` in `stb/stbasset.bsm`
  were annotated `@io` but reach SOLO via `_stb_gpu_create_texture`
  (same lattice violation pattern as `tile_load_set`'s earlier W026
  fix — IO + GPU sibling categories cannot coexist in a single
  annotation). Both un-annotated; classifier now infers SOLO.

#### Phase 3 → Phase 3.5 (Asset Pipeline) bridge

User-driven scope expansion — instead of going directly to Phase 4
(Pixel-Perfect Rendering), Phase 3.5 lands the **end-to-end asset
pipeline**: ModuLab (in-house pixel editor) gains atlas-pack export,
runtime gains `atlas_load_modulab` consumer, two production games
(`pathfind`, `platformer`) migrate to atlas-driven sprites. This
closes the loop ModuLab→atlas pack→runtime→games and finally
delivers the asset workflow B++ has been pointing at since 2026-04
(ModuLab 1.0 ship). Phase 4 picks up after with pixel-perfect
rendering atop the now-consistent atlas-using game stack.

### Phase 3.5 — Asset Pipeline (in progress)

**Sessions 3.5.1 + 3.5.2 SHIPPED 2026-05-06.** Pipeline foundation:
ModuLab gains atlas-pack export, runtime gains TWO atlas loaders
(ModuLab native + Aseprite Array format), Atlas struct extended
to support variable-sized frames + named lookups.

#### Session 3.5.1 — ModuLab atlas-pack export — **SHIPPED**

`tools/modulab/io.bsm::mlab_save_atlas_pack(path)` writes the
entire frame timeline as a single JSON pack file. Schema v1:
`{ "type":"atlas_pack", "version":1, "palette":"MCU-8",
"tile_w":N, "tile_h":N, "sprites":[{"name","data"},...] }`.
Layer 0 of each frame is the sprite — multi-layer compositing
deferred to v2 when a real consumer needs it. Topbar gains an
"Atlas" button (`UI_ACT_ATLAS`); writes `<prefix>.atlas.json`.

#### Session 3.5.2 — `atlas_load_modulab` + `atlas_load_aseprite` — **SHIPPED**

`stb/stbatlas.bsm` extended with two loaders + a foundation for
"asset pipeline" parity between in-house ModuLab art and
externally-authored Aseprite art. Atlas struct gained five new
fields: `names` (sprite labels), `frame_x` / `frame_y` /
`frame_w` / `frame_h` (per-sprite pixel rects for variable-sized
atlases — Aseprite's "trim" produces non-uniform frames).

- **`atlas_load_modulab(path)`** — reads `.atlas.json` pack files
  ModuLab writes, decodes palette indices through MCU-8 to RGBA,
  composites all sprites into a single horizontal strip MTLTexture.
  Names from the pack file go into `Atlas.names`.
- **`atlas_load_aseprite(json_path)`** — reads Aseprite Array-format
  exports (the engine consumes the JSON and the sister PNG
  referenced by `meta.image`, resolved relative to the JSON's
  directory). Per-frame rects from `frames[].frame.{x,y,w,h}`
  populate the `frame_x` / `frame_y` / `frame_w` / `frame_h`
  arrays. `frames[].filename` becomes the sprite name.

Public draw routines now branch on `tile_w > 0` to pick uniform-
grid (existing math) vs variable-frame (per-sprite arrays) UV
computation. Two new internal helpers `_atlas_uvs_for` and
`_atlas_native_size` factor out the duplication. New named draw
helpers: `atlas_draw_named`, `atlas_draw_named_size`,
`atlas_draw_named_tinted`, `atlas_draw_named_size_tinted` — each
internally does `atlas_named_id` + the corresponding id-based
draw, no-op when name doesn't match (visible failure: missing
sprite, never crashing).

Backward compat preserved: existing callers (snake_maestro,
fps_3d_gpu, atlas_smoke, atlas_batch_smoke) use uniform-grid
atlases via `atlas_create_rgba` — unchanged behaviour.

**Why this is the foundation, not just two more loaders:** the
Atlas struct is now POLYMORPHIC over source — uniform grids
(procedural / Modulab) and variable frames (Aseprite) coexist
behind one API. Games consume sprites by name; the source format
is the asset pipeline's concern, not the game's. ModuLab can
evolve to also emit Aseprite-format packs in the future without
breaking any game; conversely Aseprite-authored art lands today
without waiting for ModuLab parity. The "tunnel" is open.

#### Session 3.5.3 — Pathfind atlas migration — **SHIPPED**

`tools/sprite16_to_atlas.sh` (bash + jq) — one-shot converter that
combines N sprite16 JSON files into a single `.atlas.json` pack.
Validates tile dimensions match across inputs (fails loudly on
mismatch). Future B++ port: `tools/sprite16_to_atlas.bpp` when the
script accumulates real complexity.

`games/pathfind/assets/sprites/pathfind.atlas.json` generated from
the existing `cat_sprite.json` + `rat_sprite.json` via the
converter. `games/pathfind/pathfind.bpp` migrated:

- Removed: `pal_normal` / `pal_flash` / `pal_gpu` palette plumbing,
  `spr_rat_data` / `spr_cat_data` byte buffers,
  `tex_rat` / `tex_cat` indexed textures, `palette_gpu_update`
  swap dance.
- Added: `pf_atlas` (single MTLTexture), `rat_id` / `cat_id`
  (`atlas_named_id` lookups). Per-frame draws go through
  `atlas_draw_size` against the one atlas — Phase 3.2 smart-bind
  coalesces every sprite + every wall (tilemap was already in the
  no-tileset debug-rect mode, untouched) into one drawPrimitives.
- Damage flash mechanic re-implemented as `atlas_draw_size_tinted`
  with WHITE tint while `flash_ms > 0`. Visually equivalent to the
  former palette swap; runtime drops a GPU upload from the hot
  path.

#### Session 3.5.4 — Platformer atlas migration — **SHIPPED** (closes sidequest #25)

`stb/stbtile.bsm` extended with the atlas-bound rendering path:

- New field on Tilemap: `atlas` (Atlas pointer). When non-zero,
  `tile_draw` routes every visible cell through `atlas_draw_size`
  for batched single-flush rendering; falls back to the legacy
  `tileset` array path when only `tile_bind_set` was used.
- New entry point: `tile_bind_atlas(tm, atlas)` — sets the atlas
  + clears the tileset path for unambiguous dispatch in
  `tile_draw`. The `remap` table is reused unchanged — sprite_id
  semantics are identical between paths.
- `tile_bind_set` now clears `m.atlas` so a game that switches
  back to legacy after testing the atlas path doesn't end up in
  a mixed state.

`games/platformer/platform.bpp` migrated from `tile_load_set`
(legacy "split PNG into N MTLTextures") to atlas-based loading:

- Character sheet → `char_atlas = atlas_load(...)`. Player draws
  go through `atlas_draw_size(char_atlas, CHAR_PLAYER, ...)`
  instead of `_stb_gpu_sprite(char_sprites[CHAR_PLAYER], ...)`.
- Terrain tileset → `tile_atlas = atlas_load(...) +
  tile_bind_atlas(tilemap, tile_atlas)`. Every visible map cell
  per frame batches into one drawPrimitives.
- `init_game` un-annotated (was `@io`) — now correctly inferred
  SOLO since atlas_load reaches GPU.

**Phase 3 + 3.5 — Asset Pipeline CLOSED.** All four production
games now run on the atlas API:
- `snake_maestro` — procedural atlas (`atlas_create_rgba` via
  `snake_sprites.bsm`).
- `fps_3d_gpu` — procedural HUD atlas (`atlas_create_rgba`).
- `pathfind` — Modulab pack (`atlas_load_modulab`).
- `platformer` — Aseprite-shape PNG (`atlas_load`); Aseprite JSON
  metadata (`atlas_load_aseprite`) ready for the contracted
  artist's first delivery.

Bootstrap byte-stable across all sessions (3-way shasum
`a01c8e724df...` identical for `./bpp` == gen1 == gen2 — the
atlas work didn't touch the compiler binary, all changes live in
stb/ + game .bpp files). Suites 140/0/12 native + 114/0/38 C
unchanged from baseline through every step.

#### Session 3.5.5 — Image cartridge merge + manifest atlas — **SHIPPED**

Three threads landed together, driven by the discovery that
asset edits in Modulab were never reaching the running game:

**Cartridge merge.** `stbatlas.bsm` (998 LOC) absorbed into
`stbimage.bsm` (now 2099 LOC, single cartridge for every image-
shaped asset). One `import "stbimage.bsm"` covers raw pixel
decode AND atlas/Image management — the prior split forced a
choice between low-level `img_load` and high-level
`atlas_load` that confused every newcomer. `Atlas` struct
renamed to `Image`; `atlas_*` functions to `image_*`; raw
codec API renamed `img_load` → `pixels_load`,
`img_w/h/depth/channels` → `pixels_*`, `img_free` →
`pixels_free`, `img_save_png` → `pixels_save_png`. One
universal entry point: `image_load(path)`.

**Unified sprite shape.** Modulab's two-file authoring pattern
(`cat_sprite.modulab.json` for state + `cat_sprite.json` for
runtime export) collapsed into ONE canonical
`cat_sprite.json` carrying `type:"sprite"`, version 3, full
layer state, palette, AND a flattened composite. Modulab
saves and loads through it natively; runtime `image_load`
reads through the same `_find_sprite_data_idx` helper that
walks `frames[0].data` → `frames[0].layers[0].data` →
top-level `data`. The legacy `type:"modulab"` and
`type:"sprite16"` files keep loading via the same reader.

**Manifest atlas pack.** `pathfind.atlas.json` was a bundle —
every sprite's pixel data inlined, regenerated only by an
explicit Modulab "Atlas" export. Cycle break: edits to
`cat_sprite.json` never reached the bundle, so build/run kept
showing the cat with old colours. Phase 3.5.5 swaps the
schema to a manifest:

```json
{ "type":"atlas_pack", "version":2,
  "tile_w":16, "tile_h":16,
  "sprites":[
    { "name":"cat", "path":"cat_sprite.json" },
    { "name":"rat", "path":"rat_sprite.json" } ] }
```

`image_load` recursively dereferences each `sprites[].path`,
so the atlas is composed at load time from the live source
files. Editing `cat_sprite.json` → next launch picks up the
change. Bundle mode (`sprites[].data` inline) is still
readable for back-compat. Per-sprite mixed mode allowed
(some inline, some referenced) — useful during a one-asset-
at-a-time migration.

**Architectural cleanup** (sidequest, mid-merge): every stb
cartridge stripped of `import "bpp_*"` lines. The runtime
modules (`bpp_array`, `bpp_str`, `bpp_io`, `bpp_buf`,
`bpp_hash`, `bpp_file`, `bpp_math`, `bpp_arena`,
`bpp_maestro`, `bpp_beat`, `bpp_job`) are auto-injected by
`bpp_import.bsm` — explicit imports were noise. `bpp_json`
joined the auto-inject list (1-cycle bootstrap oscillation
expected, gen2 byte-stable from there). New tonify rule:
**stb cartridges MUST NOT import bpp_* runtime modules** —
auto-inject covers them.

`stbgame` ALSO gained `import "stbimage.bsm"` so its
`game_frame_begin` can auto-tick the image hot-reload
registry shipped in 3.5.6 — same dependency shape as
stbinput / stbdraw / stbui, the foundation peer set.

#### Session 3.5.6 — Live hot-reload (in-runtime) — **SHIPPED**

Games can opt into live edit-while-running for any loaded
Image:

```bpp
pf_image = image_load("pathfind.atlas.json");
image_hot_reload_enable(pf_image);
```

Mechanism: each registered `Image` carries `src_path` (path
captured at load time) and `last_hash` (FNV-1a content hash
covering the manifest plus every sibling sprite file). The
auto-tick from stbgame's `frame_begin` rehashes registered
images, fires `_image_reload(img)` when any hash advanced,
and updates the live struct in place — the `Image*` the game
holds stays valid, only `tex` (the GPU MTLTexture handle)
swaps to a new texture. Old GPU memory leaks deliberately
(stbasset pattern; acceptable dev-time cost).

Combined hash means edits to ANY referenced file
(`cat_sprite.json`, `rat_sprite.json`, the manifest itself)
trigger one reload of the whole atlas. No partial reloads —
simpler model, atomic visible state. Workflow result: edit
cat colour in Modulab → save → ~16 ms later the running
pathfind shows the new cat without restart.

Architectural rationale (industry alignment): asset
hot-reload lives in the GAME runtime, not the editor. Same
pattern as Unity (AssetDatabase), Unreal (Asset Manager),
Godot (EditorFileSystem), Bevy (AssetServer with `notify`).
Editor / IDE saves the file; runtime polls the filesystem
and reloads. Decouples game from editor — works with
Modulab, Aseprite, Vim, hex editor, anything that writes
the file. No IPC, no editor-runtime protocol.

#### Session 3.5.7 — Generic file_watch + level reload — **SHIPPED**

Hot-reload generalised beyond images. `file_watch_register`
takes a path + a function pointer; the callback fires
whenever the file's content hash changes. Same mechanism as
image_hot_reload, exposed for arbitrary assets:

```bpp
file_watch_register("assets/levels/level1.level.json",
                    fn_ptr(reload_level));

void reload_level() {
    auto gx, gy;
    for (gy = 0; gy < GRID_H; gy++) {
        for (gx = 0; gx < GRID_W; gx++) {
            tile_set(tm, gx, gy, T_EMPTY);
        }
    }
    load_arena_from_file("assets/levels/level1.level.json");
    sync_path_from_tilemap();
}
```

Pathfind wires it on init; Bang 9's level editor saves →
~16 ms later the running game shows the new walls AND the
cat re-routes via the updated path mask. Same pattern is
ready for audio cues, AI scripts, gameplay tuning JSON —
anything the game reads at startup.

Bug fix bundled in: pathfind's loader collapsed every
non-zero MCU-8 palette index to T_WALL (== 1), so colours
painted in the level editor all rendered identical in-game.
Fix preserves the cell value 1..7, marks each as solid via
a `tile_solid` loop, and binds a procedural 8-tile MCU-8
colour atlas (`build_mcu8_tile_atlas` in pathfind.bpp). Level
editor's MCU-8 palette now renders identically in-game.

#### Session 3.5.8 — Adaptive throttle — **SHIPPED**

The original poll cadence was a fixed 30 frames (~0.5 s).
Sufficient for most workflows but noticeable during a
sequence of paint strokes. Adaptive replacement: idle stays
at 30 frames; the moment any reload fires, the tick switches
to every-frame polling for a 60-frame burst (~1 s of "active
editing" responsiveness). Decay back to idle when no further
changes show up. Net effect: first edit takes the cold
0.5 s to detect; every subsequent save during a burst lands
in ~16 ms.

Polling chosen over kqueue / inotify event-driven watches
deliberately — agnostic over latency. The same code runs on
every backend B++ ever ports to (no FFI to libc, no per-OS
syscall numbers, no event-loop integration), and the poll
cost on small JSON files is well under a frame even in the
burst window. If a future profile flags polling as a hot
spot, kqueue / inotify slot in as a backend implementation
of the same `file_watch_*` API without changing any caller.

**Phase 3.5 — Asset Pipeline + Live Hot-Reload CLOSED.**
Workflow end-to-end: open `cat_sprite.json` in Modulab (any
sprite editor that writes the unified shape), edit, Ctrl+S
→ next build/run AND any already-running game show the
change within ~16 ms. Same for level edits in Bang 9's
level editor. No regen step, no atlas rebuild, no IPC, no
restart — the pattern professional engines (Unity, Godot,
Bevy) deliver, in B++ runtime that stays zero-dependency.

**Next action:** Phase 4 — Pixel-Perfect Rendering (~400 LOC, 3
sessions). Render-to-texture, integer scaling at present, multi-
pass infrastructure. Layers on top of the now-uniform atlas-using
game stack to deliver crisp pixel-perfect output at any window
size.
