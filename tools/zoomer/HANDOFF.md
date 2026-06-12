# zoomer — a B++ port of Tsoding's boomer (handoff)

**What this is.** A screen zoomer/magnifier for "zoom while programming" —
capture the desktop, render it on a GPU quad with mouse-driven zoom/pan and a
flashlight spotlight. A B++ port of Tsoding's **boomer** (MIT, Nim/X11/OpenGL),
the way the Jai video re-did it — but targeting B++ instead of Jai.

Status: **evaluated, pre-port.** No zoomer code yet.

## Source / spec

boomer is cloned (MIT) at **`/Users/Codes/boomer`** (sibling dir, like
`/Users/Codes/Quake`). It is small — ~855 lines of Nim + two GLSL shaders:

| File | Role | External dep |
|---|---|---|
| `src/boomer.nim` (594) | main: X11 window + GLX/OpenGL context, event loop, input wiring | X11 + GLX |
| `src/screenshot.nim` (143) | **screen capture** — `XGetImage` / `XShmGetImage` (MIT-SHM fast path) from the root window; PPM save | X11 |
| `src/vert.glsl` + `src/frag.glsl` | textured fullscreen quad + zoom transform + flashlight effect | OpenGL |
| `src/navigation.nim` (34) | zoom/pan/velocity state with friction (the "camera") | — |
| `src/config.nim` (43) | `$HOME/.config/boomer/config` parser | — |
| `src/la.nim` (41) | small vec/mat linear algebra | — |

Features (from the README): drag to pan, scroll / `=`/`-` to zoom (with
friction), `f` flashlight toggle, `Ctrl`+scroll flashlight radius, `m` mirror,
`0` reset, `r` reload config, `q`/`ESC` quit. Config params: `min_scale`,
`scroll_speed`, `drag_friction`, `scale_friction`.

## B++ capability map

| Piece | B++ status |
|---|---|
| Window + input (drag / scroll / keys) | ✅ `stbwindow` (+ the macOS gesture-event-loss self-heal already in place) |
| Textured quad + custom shader (zoom/flashlight) | ✅ `stbshader` (loads a `.metal` pipeline, custom uniforms, fullscreen — exactly this use case) |
| Nav (zoom/pan/friction) + config + vec/mat | ✅ trivial in B++ (vec/mat overlaps the `stbmath3d` we build for Quake) |
| **Screen capture** | ❌ **the one new piece** — neither macOS nor X11 capture exists yet |

The earlier grep "hits" (`CGDisplayPixelsWide/High`) are display *size*, not
capture — but they show the platform already `dlsym`s CoreGraphics, so adding
capture is the same pattern.

## Port direction — generic core, arm64/macOS first

Port **generically** (the B++ way: platform-agnostic core + per-OS backends),
**first working target macOS arm64**:

- **Platform-agnostic core (write once):** navigation (zoom/pan/friction),
  config parse, vec/mat, and the render via `stbshader`. None of this is OS- or
  X11-specific. We do **not** copy boomer's X11/OpenGL approach wholesale — we
  take its *architecture* (capture → texture → zoom shader → input loop) and
  implement the divergent piece per-OS.
- **Screen capture = the per-OS backend (the only divergent piece):**
  - **macOS arm64 (first):** `CGDisplayCreateImage` (CoreGraphics) via `dlsym`
    — same pattern as the existing `CGDisplayPixelsWide/High` in
    `_stb_platform_macos.bsm` — → upload to a Metal texture.
  - **Linux/X11 (later, 2nd backend):** `XGetImage` / `XShmGetImage`, matching
    boomer.
- **Why arm64/macOS first:** it is the user's machine (where they actually zoom
  while programming) *and* B++'s strongest GPU path (Metal). The X11 path is a
  second capture backend, not the starting point.
- **Shaders:** GLSL → Metal. `stbshader` loads a `.metal` pipeline; the
  zoom-transform + flashlight logic is identical, only the shading language
  differs.

## The one reusable piece (engine-vs-tool, lightly)

Most of zoomer is the tool itself and stays in `tools/zoomer/`. The exception is
**screen capture**, which other tools/games might want — so it is written as a
small reusable **platform primitive** (`screen_capture` → buffer/texture, per-OS
backends), with the zoomer app on top. Don't over-engineer a ~200-line tool, but
the capture is born reusable.

## Scope

Small and fast — days, not months (contrast the Quake arc). A clean exerciser of
the `stbwindow` + `stbshader` GPU path that also yields the reusable
screen-capture primitive.

## Decision — where the capture primitive lives (settled)

`screen_capture` lives in **`stbwindow`** (the platform-facing cartridge for
tools), with the per-OS implementation in `_stb_platform_<os>.bsm` — **not** a
new `stbcapture` cartridge. Reason: today it is one function with one consumer
(zoomer); a cartridge would be premature (Rule 28). **Reserved:** if it grows the
boomer-class features (region capture, window tracking, live-frame streaming,
multi-monitor) AND a second consumer appears, graduate it to `stbcapture` — a
file move + rename, since the per-OS primitive and the surface stay clean. This
note is in the `stbwindow` capture comment so it travels with the code.

## Progress

- ✅ **Screen capture (arm64) DONE + verified.** `_stb_screen_capture` in
  `_stb_platform_macos.bsm` (CoreGraphics `CGDisplayCreateImage` →
  data-provider → tightly-packed BGRA buffer) + the `screen_capture` surface in
  `stbwindow`. Smoke ran on arm64 macOS: **captured 2560×1440 BGRA, real
  pixels.** Linux 0-stub keeps builds linkable until X11 (`XGetImage`).
- ✅ **BGRA texture (sealed approach b).** `_stb_gpu_create_texture_bgra`
  (MTLPixelFormatBGRA8Unorm = 80) in the macOS backend + a Linux 0-stub —
  per-OS primitive, so no shader swizzle and no CPU byte-swap. The capture's
  BGRA bytes upload honestly; reusable by any future capture consumer.
- ✅ **Render path DONE + verified.** `tools/zoomer/zoomer.metal` (fullscreen
  triangle + zoom/pan transform, BGRA texture, no swizzle) + `tools/zoomer/
  zoomer.bpp` (capture → BGRA texture → pipeline → loop: cursor = zoom focus,
  hold L/R mouse to zoom in/out → `gpu_uniform_set_frag` + `gpu_draw_full`).
  Non-looping `render_smoke.bpp` confirmed on arm64 macOS: capture 2560×1440 →
  BGRA texture handle → **`zoomer.metal` compiled, pipeline OK.** Suite 183/0/12.
- **Portable by construction (Rule 41):** the tool calls only per-OS-resolved
  primitives (`_stb_screen_capture`, `_stb_gpu_create_texture_bgra`) + the
  agnostic `gpu_*`/`render_*` API — both primitives have macOS impls + Linux
  stubs, so future Linux (X11 + Vulkan) backends slot in without touching the
  tool. No macOS-only code in `zoomer.bpp`.

- ✅ **Fullscreen overlay (arm64).** New per-OS primitive `_stb_set_fullscreen(f)`
  (macOS: borderless window the size of the main display, `setLevel: 1000` above
  the menu bar; Linux 0-stub for parity). The zoomer calls it before `game_init`,
  renders the capture at the screen's pixel resolution (crisp), and quits on
  `KEY_ESC` (a borderless window has no close box). Compiles + suite 183/0/12;
  the normal windowed path is unchanged (verified via `render_smoke`).

- ✅ **Navigation graduated to stb (`stbpanzoom`).** The engine part — a 2D
  pan/zoom viewport with momentum/friction (look-at + scale + velocities,
  zoom-toward-a-focus) — is a reusable cartridge `stb/stbpanzoom.bsm`, pure
  state + math (no GPU/platform/input). Distinct from `stbcamera` (a
  world-clamped, zoom-less *game* camera). CI test `tests/test_panzoom.bpp`
  (headless) — suite 184/0/12. The zoomer now consumes it: `panzoom_zoom_at` /
  `panzoom_pan` / `panzoom_update` / `panzoom_uniform`; the tool keeps only
  sourcing + input wiring + the window. Controls: `=`/`-` zoom toward cursor,
  drag = pan, `0` = reset, ESC = quit. Shader switched to the look-at transform
  (`uv = center + (in.uv - 0.5)/scale`).

## Next (the port — moving OFF the screenshot)

The frozen-snapshot base is proven; it's not the useful end state.

**DECIDED next step → in-app / in-game zoom** *(2026-06-11)*. Feed the zoom
render a texture that *is* the app's own content (a game render-target, Bang 9's
canvas) — no screen capture, no self-capture feedback, no Screen Recording
permission, and it works on Linux today (GPU texture in → magnified out). This
graduates the **zoom render** into a reusable effect (sniper scope, canvas
magnify) consumed by both this tool and any in-app zoom — the same engine-vs-tool
split as `stbpanzoom`. Concretely: a small cartridge (e.g. `stbzoom`, or a pass
in `stbfx`) that takes a texture + a `PanZoom` → a magnified draw, plus a smoke
that renders to an offscreen target and zooms it. The standalone screen zoomer
keeps working off the capture path.

**Alternative (deferred): live screen.** Re-capture each frame + update the
texture; the catch is the overlay capturing *itself* → feedback, fixed by
capturing only below our window (`CGWindowListCreateImage` +
`kCGWindowListOptionOnScreenBelowWindow`) or via **ScreenCaptureKit** (`SCStream`
→ IOSurface → Metal, zero-copy). Pick this up if a general OS-wide live magnifier
becomes the goal.

Smaller follow-ups: scroll-wheel input (a portable platform-additive feature
that would feed `panzoom_zoom_at` directly); flashlight effect; if the cursor
focus is off across the whole screen, switch the mouse→UV divisor to the live
window size (`_stb_win_w`/`_stb_win_h`).
2. **Pure-B++ core:** `navigation` (zoom/pan with velocity + friction, à la
   boomer) + `config` parser. Replaces the hold-to-zoom MVP with smooth scroll
   zoom + drag pan. Testable headless.
3. **Flashlight + fullscreen + controls:** add the flashlight uniform/effect to
   `zoomer.metal`, go fullscreen, wire the rest of boomer's control table
   (mirror, reset, flashlight toggle, keys).
4. **Later:** X11 capture + Vulkan texture Linux backends (fill the stubs);
   `-d:live` live refresh; window-track (`-d:select`).
