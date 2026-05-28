# Sidequest — stbui graceful clamp: clip rects + non-negative layout

**Status:** SHIPPED 2026-05-28. All three layers landed bottom-up + a
headless smoke test (`tests/test_clip.bpp`). Native suite 181/0/12.

> Note: implementing Layer 1 surfaced a latent **Mach-O chained-fixup
> page-straddle** bug — the added clip statics grew `__DATA` enough to push
> the GOT across a 16 KB page boundary, which dyld's per-page chains never
> rebased (`_time_now_ns -> pc=0x1`, SIGBUS in `test_modulab_select`). Found
> by dog-fooding `bug`; fixed separately in `a64_macho.bsm` (commit
> `4d0b9ea`) and is the true prerequisite for this arc on macOS.

**Goal:** give stbui/stbdraw the two primitives every real UI system has and stbui
lacks, so a widget can never paint outside its region and a too-small container
degrades cleanly instead of bleeding onto its neighbours. Surfaced by Bang 9 on
Linux: shrinking the window below its minimum collapsed the flex panel row to a
negative height, and the panel's content drew *upward* over the tab strip.

Fixing this in stbui (not in Bang 9's `_placeholder`) fixes every consumer at
once — Bang 9, modulab, fxlab, level_editor, the_bug — and closes the already-
documented gap in `stbui.bsm:1156` ("Scroll view … no clipping … widgets drawn
outside the viewport will [overflow]").

## Prior art (study, 2026-05-28)

Every system separates this into two concerns; stbui is missing both.

**Clipping (render-time):**
- **Dear ImGui** — a clip-rect *stack*: `PushClipRect(min, max, intersect)` /
  `PopClipRect`; each draw command carries its clip rect, applied as a GPU
  scissor. The same stack drives hit-testing + culling. Rectangles only.
- **Clay** — layout emits `SCISSOR_START{bbox}` / `SCISSOR_END` render commands
  the backend honours; opt-in per element, and pairing the clip with a
  `childOffset` is how Clay does scrolling (clip + shift children).
- **CSS** — `overflow: hidden|scroll|auto|visible`; clip is opt-in, default
  `visible` is the overflow-bleed (exactly stbui's current behaviour).

**Layout clamp (layout-time):**
- **Clay / flexbox** — GROW elements shrink toward their min to make room, sizes
  stay non-negative throughout; leftover overflow is then clipped or scrolled.
- **Flutter** — "constraints down, sizes up": a child sizes within
  `BoxConstraints(min,max)` and can't produce a negative/out-of-range size.
- The invariant everyone enforces and stbui doesn't: **a node size is never
  negative.** stbui's flex row goes negative when the window is shorter than
  menu+tabs+status, and that negative height drives the upward bleed.

**Takeaway for a software renderer:** ImGui's clip-rect stack, enforced at the
base pixel op, is the natural fit — `put_px` is already bounds-checked
(`stbdraw.bsm:48`), so a clip check there gives `draw_rect`/`draw_text`/
`draw_line` clipping for free. Combine with the non-negative size invariant and
you have the "graceful clamp."

## Design (bottom-up, three layers)

### Layer 1 — stbdraw: clip-rect stack
- File-scope current clip `[_clip_x0, _clip_x1) × [_clip_y0, _clip_y1)` (half-open),
  a small fixed-depth save stack, and a stack pointer.
- `draw_clip_push(x, y, w, h)` — intersect `[x, x+w) × [y, y+h)` with the current
  clip (ImGui-style), push the old clip, install the intersection. Empty/inverted
  intersections collapse to a zero-area rect (draws nothing — correct).
- `draw_clip_pop()` — restore the saved clip.
- `draw_clip_reset()` — clip = full framebuffer; called at frame begin / init.
- `put_px` — one extra test against the current clip before the framebuffer
  write. Every higher primitive inherits clipping.

### Layer 2 — stbui: ui_clip + non-negative clamp
- `ui_clip_push(idx)` = `draw_clip_push(ui_node_x/y/w/h(idx))`; `ui_clip_pop()`.
- Clamp `ui_node_w` / `ui_node_h` to ≥ 0 (the non-negative invariant) so a
  collapsed flex row reports 0, not a negative height.
- Scroll view pushes a clip = its viewport (closes the documented gap).

### Layer 3 — Bang 9: clip each panel
- `panels_draw(x, y, w, h)` wraps its dispatch in `draw_clip_push(x,y,w,h)` /
  `draw_clip_pop()`. With the non-negative clamp, a too-short window yields a
  0-height panel → empty clip → nothing draws (no bleed onto the tabs) instead
  of cramming. Keep the `_placeholder` spacing fix; the ad-hoc clamp is no
  longer needed.

## Phases / verification (each layer independently testable)

- **L1 stbdraw** — bootstrap byte-stable (stbdraw is auto-injected; must not
  perturb the compiler), native suite 180/0/12. Smoke: a draw_rect/draw_text
  outside a pushed clip is suppressed.
- **L2 stbui** — same gates; an existing stbui consumer still renders identically
  when no clip is pushed (clip defaults to full screen).
- **L3 Bang 9** — cross-compile to Linux, run in Docker/XQuartz: the sub-minimum
  window no longer bleeds onto the tab strip (panel content clips to its region);
  at normal size, unchanged.

## Notes
- Clip rects are rectangles only (matches ImGui; rounded-corner clipping is out
  of scope — the rounded-rect helper already pixel-steps its own corners).
- This is the general fix; it also benefits the Project-tab file list (scrolled,
  currently unclipped) and any future split-pane / overflowing panel.
