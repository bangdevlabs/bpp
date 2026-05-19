# Sidequest — stbui v2: Clay-inspired layout for B++

**Status:** ARC CLOSED 2026-05-18 — **S1 → S6 + S8 (via S8.1) +
S9 + S9.1 SHIPPED**, **S7 DEFERRED** (mini_synth + sprite_viewer
— no killer-use-case under Rule 28; reopens when either tool
gains a Bang 9 panel embed). The Bang 9 shell now uses the v2
declarative tree alongside every embedded tool, because
`ui_frame_begin` decouples the per-frame click-swap from the
per-tree layout reset. v1 layout + Style helpers are gone from
`stb/stbui.bsm` (verified zero source consumers across
`tools/ bang9/ games/ examples/ tests/`).

**Shipping trail (2026-05-17):**
- S1 — `c6d8ee8` (plan + grid/zoom UI) + `dedfb04` (engine integrated)
- S2 — `324c419` (aseprite viewer)
- S3 — `54ba80d` (Modulab editor + mixed-sizing helpers)
- S4 — `b484a17` (level_editor)

**Shipping trail (2026-05-18):**
- S5 — fxlab declarative shell migration
- S6 — the_bug debug-map viewer declarative shell
- S7 — DEFERRED (mini_synth + sprite_viewer, Rule 28 no consumer)
- S8 — ATTEMPTED + REVERTED same session: ui_layout_begin not
  re-entrant; embedded tools' layouts and v2 button clicks broke
  visibly.
- S9 — PARTIAL: ui_demo.bpp deleted (lone v1-layout consumer)
- S8.1 — ui_frame_begin/ui_layout_begin split SHIPPED; Bang 9
  shell re-migrated to v2 chrome tree (S8 re-applied, now safe)
- S9.1 — v1 dead-code excised from stbui.bsm: struct Style + v1
  layout (`lay_*`, `_lay_advance`, `struct Layout`, `LAY_*`,
  `_lay_stack/top`) + dead widgets (`gui_panel_s`, `gui_label_s`,
  `gui_number_c`, `gui_button_s`, `gui_bar`, `style_new`) + the
  three `_stl_*` extrn caches in `init_ui`. ~280 LOC out of
  stbui.bsm. Mechanical sweep collapsed `rx = lay_x(x); ry =
  lay_y(y);` to `rx = x; ry = y;` and dropped every
  `_lay_advance(...)` call from surviving widgets via
  content-pattern sed. Suite 174/0/12 + 138/0/48 green,
  bootstrap byte-stable.
- Bonus closure: parser fbuf overflow that S1's `import stbimage`
  surfaced was killed by `cd6d00e` (heap-per-file refactor); the
  speculative `ui_image` widget itself reverted by `dba3e2a`
  (Rule 23 + Rule 28 — zero consumers, paying full import-budget
  tax). See journal entry 2026-05-17 for the full narrative.
**Trigger:** Modulab zoom +/- buttons reflow the entire panel
because side widgets are anchored canvas-relative. Symptom of a
deeper issue — stbui's "explicit pixel positioning" model
(every widget takes `x, y, w, h`) scales badly across the ~500+
call sites in Bang 9 / Modulab / level_editor / fxlab / Aseprite
viewer. User flagged Clay (Nic Barker's C UI layout library) as
the right direction.

## v1 layout audit (added 2026-05-17 after agent caught me)

**FIRST DRAFT OF THIS DOC WAS WRONG.** I wrote ~300 LOC of plan
claiming stbui v1 had no layout system. An audit agent caught
that `lay_push / lay_pop / lay_x / lay_y / lay_w` have shipped
in `stb/stbui.bsm` (lines 443-499) since 2026-04-22. Grep
across `tools/ bang9/ games/ examples/` shows **0 call sites**
of those helpers vs 35 raw `gui_button` calls. The layout
system was dead-on-arrival.

**Why v1 layout failed adoption** (read the source, don't
speculate):

```bpp
void lay_push(x, y, w, h, pad, dir) {   // <-- still demands x,y,w,h
    ...
}
```

`lay_push` is a cursor stack with auto-advance — saves typing
`+24` between buttons but doesn't relieve the consumer from
computing absolute coords. No FIT / GROW / PERCENT. Stack depth
fixed at 8. Net abstraction over `gui_button(x, y, w, h, label)`:
marginal. Net cost to learn: another API on top. Result: nobody
adopted.

**This is the real bar v2 has to clear.** Declaring "row of
buttons" must be visibly cheaper than computing offsets. If the
S2 migration (Aseprite viewer, smallest active consumer) does
not shave LOC AND clarify the code, v2 has the same fate.
**Abort gate: if S2 migration is ambiguous, do not proceed to
Modulab (S3).**

## Broader doc-drift audit (added 2026-05-17, ~25 min)

Spot-checked 7 cartridges adjacent to stbui (visual / render /
new). Method: extract public API per cartridge, grep call sites
across consumers, tally. Results:

| Cap | API funcs | 0 consumers | 1-3 (rare) | Heavy | "Dead" % |
|---|---|---|---|---|---|
| stbdraw | 15 | 5 | 5 | 5 | 33% |
| stbtile | 13 | 0 | 7 | 6 | 0% ✓ |
| stbrender | 18 | 6 | 4 | 8 | 33% |
| stbai | 4 | 0 | 2 | 2 | 0% ✓ |
| stbscene | 8 | 3 | 4 | 1 | 37% |
| stbecs | 28 | 7 | 9 | 12 | 25% |
| stbfx | 8 | 1 | 1 | 6 | 12% |
| **stbui v1 layout** | 5 | 5 | 0 | 0 | **100%** |

**Critical user correction (2026-05-17):** "0 consumers" is NOT
automatically a problem. The distinction is:

| Sub-type | Treatment |
|---|---|
| **Available-not-yet-used.** API correct, scope-aligned, just no consumer demand yet. `draw_gradient`, `bg_layer_set_alpha`, `ai_step_collide`. | Leave alone. Document well so future consumers discover it. Healthy infrastructure shelf. |
| **Broken — API shape blocks adoption.** Ergonomics defeat the purpose. `lay_push(x,y,w,h)` still demands coords → same effort as raw widgets → nobody adopts. | Fix or deprecate. This is the actual problem. |

Most of the 27 "dead" functions in the audit are
**available-not-yet-used** — solid shelved infra. stbui v1
layout is the extreme outlier: 100% dead + clear broken-shape
diagnosis. v2 targets ONLY that gap.

**Implication for v2:** discipline carries forward — build the
**minimum widget set** that real consumers need NOW, expand by
Tonify Rule 20 (two-consumer rule) as new consumers ask. Do NOT
ship `ui_shadow`, `ui_gradient`, `ui_animated_button` until a
real consumer asks. Start with `ui_text`, `ui_button`, `ui_box`,
`ui_image`, `ui_spacer` — that's the MVP.

## Why now

**Cost of the current model.** Every consumer hardcodes pixel
positions and re-derives them when something resizes. v1's
existing `lay_push / lay_pop` cursor stack doesn't help (audit
above) because it still demands explicit coords. Concrete pain
points already hit:

- Modulab side panels (palette / phase4 / layer / frame strip)
  anchored to `_canvas_right` / `_canvas_bottom`. Zoom changes
  canvas size → all panels move. Zoom +/- buttons we added in
  the status line trigger this every press.
- Bang 9 has ~7 tabs that each compute panel rects manually.
  Adding the FX tab cost ~80 LOC of positioning arithmetic.
- Aseprite viewer's sheet / tag list / preview panels do their
  own three-zone layout math (see `_render` in `aseprite_view.bsm`).
  Adding the dirty-tint pass meant chasing four origin calcs.
- fxlab slider arrangement: explicit y-offsets, one per slider.
- level_editor inspector positioning: hardcoded right column.

Aggregate: every new widget costs O(positioning math). With Clay-
style layout (declare structure + sizing rules → engine resolves
positions) the same widget is O(1) — drop into the parent's
flow, sizing rule does the rest.

**Why now vs deferring.** S6 (flow-field crowd movement) will
need a debug HUD overlay (vector field arrows, density heatmap,
selection bounding box). Building that on the old stbui means
yet another consumer hardcoding positions. Better to land the
new primitives first so S6+ benefits.

## Clay's design — the part we adopt

Cross-referenced via WebFetch on the Clay GitHub README. The
parts that translate cleanly to B++:

| Clay concept | B++ equivalent |
|---|---|
| Macros (`CLAY({ ... })`) wrapping nested blocks | Stack-based `ui_box_begin / ui_box_end` pairs (B++ has no macros) |
| `Clay_LayoutConfig` struct per element | Same — `UiBox` struct with sizing + padding + alignment fields |
| Sizing: FIT / GROW / FIXED / PERCENT | Same — encoded as `UiSize { mode, value }` or sentinel ints (`sz_grow()` returns -1, etc.) |
| `Clay_BeginLayout / EndLayout(dt)` returning render commands | `ui_layout_begin / ui_layout_end()` builds + flushes a render-command list |
| Arena memory (no per-frame alloc) | Reuse `ui_arena_reset` (already exists for current stbui widgets) |
| Hit testing via `Clay_Hovered()` / `Clay_PointerOver(id)` | Same — `ui_hovered(id)` queried during declaration |
| Single-pass O(n) layout | Same — first pass computes positions, second emits render commands. Actually two passes (Clay calls it single-pass conceptually because both run in `Clay_EndLayout`). |

Parts we DEFER (out of v1):
- `Clay_UpdateScrollContainers` — scroll handling. Aseprite
  viewer already has manual pan; do scroll later.
- Z-index for overlays — modals + tooltips. Most tools don't
  need them yet.
- Visibility culling — 8192-element pool is way more than any
  B++ tool needs; skip the optimization.

## Proposed API (B++ surface)

Stack-based immediate mode. No macros. Each frame the consumer
declares the tree via begin/end pairs:

```bpp
ui_layout_begin(panel_w, panel_h);

ui_box_begin(LAYOUT_COL, .padding=8, .gap=4);
    // Top bar — row of buttons + grow spacer + status
    ui_box_begin(LAYOUT_ROW, .size_w=ui_sz_grow(), .size_h=ui_sz_fixed(28), .gap=6);
        if (ui_button("Save"))     { _do_save(); }
        if (ui_button("Save As"))  { _do_save_as(); }
        if (ui_button("Open"))     { _do_open(); }
        ui_spacer(ui_sz_grow());
        ui_text(filename_or_placeholder());
    ui_box_end();

    // Main content — row: tool sidebar + canvas + right sidebar
    ui_box_begin(LAYOUT_ROW, .size_w=ui_sz_grow(), .size_h=ui_sz_grow(), .gap=4);
        ui_box_begin(LAYOUT_COL, .size_w=ui_sz_fixed(120), .gap=2);
            // Tool buttons
            for_each_tool { if (ui_button(tool.name)) { g_tool = tool; } }
        ui_box_end();

        // Canvas viewport — fixed-fit, content scrolls inside
        ui_box_begin(LAYOUT_FIXED, .size_w=ui_sz_grow(), .size_h=ui_sz_grow());
            canvas_draw_into_box();   // legacy draw call, uses box bounds
        ui_box_end();

        ui_box_begin(LAYOUT_COL, .size_w=ui_sz_fixed(240), .gap=4);
            ui_palette_row();
            ui_layer_panel();
            ui_phase4_sidebar();
        ui_box_end();
    ui_box_end();

    // Bottom — frame strip + status bar
    ui_box_begin(LAYOUT_ROW, .size_h=ui_sz_fixed(60), .gap=4);
        ui_frame_strip();
    ui_box_end();
ui_box_end();

ui_layout_end();
ui_render();   // flush queued commands via stbdraw primitives
```

Sizing helpers — encoded as 64-bit words so they pass by value:
```bpp
ui_sz_fit()       -> word     // size to children
ui_sz_grow()      -> word     // fill remaining; multiple grow siblings share
ui_sz_grow_max(N) -> word     // grow up to N
ui_sz_fixed(N)    -> word     // exact pixels
ui_sz_pct(P)      -> word     // percent of parent (0..100)
```

Encoding: high 8 bits = mode (1=FIT, 2=GROW, 3=FIXED, 4=PERCENT),
low 56 bits = value. Sentinel-free, fits in a word slot.

## Engine internals

Layout phases (run by `ui_layout_end`):

1. **First pass (top-down sizing)**: walk tree, resolve FIXED +
   PERCENT immediately. FIT containers come back during the
   bottom-up pass.
2. **Bottom-up FIT pass**: leaf-up, sum children's sizes plus
   padding + gap to set parent FIT dimensions.
3. **Top-down GROW pass**: distribute parent's remaining space
   to GROW children proportionally.
4. **Position pass**: top-down, compute child positions from
   parent origin + layout direction + alignment.
5. **Render command emit**: walk tree, emit one command per
   visual element (rect, text, image, custom-handler).

For B++ this is ~400-500 LOC of engine. Tree storage: flat node
arena (each node has parent_idx + first_child_idx + next_sibling_idx
+ computed_x/y/w/h + LayoutConfig). Reset per frame via
`ui_arena_reset`.

## Migration strategy

**Big-bang migration is wrong.** Old stbui has too many active
consumers to atomically swap. Instead: ship v2 ALONGSIDE v1, let
each consumer migrate when convenient. v1 stays until the last
consumer moves.

Concrete order:
1. **Session 1 — CLOSED 2026-05-17 (`c6d8ee8` + `dedfb04`)**: stbui
   v2 engine integrated INTO `stb/stbui.bsm` (NOT a parallel cartridge —
   user corrected the first draft). ~530 LOC: containers + sizing +
   four-pass layout + render-command output + widgets (`ui_text`,
   `ui_button`, `ui_image`, `ui_spacer`). Regression net at
   `tests/test_stbui_layout.bpp` covers FIT/GROW/FIXED/PERCENT
   combos (8 scenarios, 28 assertions). NOTE: `ui_image` later
   reverted by `dba3e2a` — zero consumers, paid the import-budget
   tax that broke pathfind via fbuf overflow. Re-add when a real
   consumer surfaces.
2. **Session 2 — CLOSED 2026-05-17 (`324c419`)**: Aseprite viewer
   migration. Smallest active consumer used as the ergonomics
   gate. Net LOC + clarity delta passed the gate criteria; cleared
   S3.
3. **Session 3 — CLOSED 2026-05-17 (`54ba80d`)**: Modulab editor
   migration. Canvas viewport became the only GROW slot; side
   panels (palette / frame strip / phase4 sidebar / layer panel)
   stay fixed-width and don't budge on zoom. Two visual fixes
   surfaced from user smoke: canvas centered inside its declared
   bbox (was shoved to top-left at low zoom), zoom clamped to the
   largest integer that fits (zoom 41 was overflowing into phase4).
   Mixed-sizing helpers added to stbui after S2 showed them as the
   most common asymmetric cases: `ui_col_fixed_w`, `ui_row_fixed_h`,
   `ui_col_fixed`.
4. **Session 4 — CLOSED 2026-05-17 (`b484a17`)**: level_editor
   migration. User's call: "migra tools standalone antes do bang9
   que engloba elas" — inner tools first, host last. Killed
   `picker_x = px + 700` hardcode + `avail_w = pw - 40 - 80`
   chrome-math; grid centered in canvas bbox; picker Y separated
   from canvas Y so the centered grid doesn't drag the right
   column vertically.
5. **Session 5 — CLOSED 2026-05-18**: fxlab migration. Shell
   reframed as `COL panel → ROW body { COL sidebar fixed_w(180)
   | COL main grow } + ROW helpbar fixed_h(24)`. Killed three
   magic-number offsets: `sx = px + _FXLAB_SIDEBAR_W + 16`,
   `sw = pw - _FXLAB_SIDEBAR_W - 32`, footer `py + ph - 18`.
   All now derive from declared bboxes. Preset list anchored
   to sidebar bbox; slider stack anchored to main bbox; footer
   anchored to helpbar bbox. The slider stack itself keeps the
   manual `slider_y += 36` accumulator — each slider row is two
   v1 widgets (`gui_label_c` + `gui_slider`) plus a value
   readout, doesn't pay enough to justify a `ui_row_fixed_h` per
   slider. Promote to per-row declared layout when a slider
   variant needs alignment v1 can't express. Bang 9 Effects tab
   continues to embed cleanly (same `fxlab_lib_frame(x, y, w, h)`
   signature — only positioning math changed).
6. **Session 6 — CLOSED 2026-05-18**: the_bug migration. Shell
   reframed as `COL panel → ROW topbar (fixed 36 or 60, depending
   on `_tb_run_started`) + ROW content (grow) + ROW statusbar
   (fixed 24)`. Killed four magic offsets: `_tb_ph = ph - 24`,
   `bar_y = py + 8`, `content_h = ph - (content_y - py) - 24`,
   `status_y = py + ph - 20`. Topbar height is computed before
   layout begins so the row dimensions stay static for the
   frame. Live panel split (only in STOPPED state) + scroll
   handling + placeholder centering all anchor to the content
   bbox instead of `(px, pw)` math. `_tb_ph` clip globals
   continue to feed `_tb_draw_line` / `_tb_draw_section` —
   computed from the content row's bottom edge now rather than
   a hardcoded `ph - 24`. Bang 9 Debug tab embed survives
   unchanged (same `the_bug_lib_frame(x, y, w, h)` signature).
7. **Session 7 — DEFERRED 2026-05-18 (Rule 28)**: mini_synth and
   sprite_viewer audit returned no killer-use-case. Both ship as
   standalone-only with fixed windows (320×180 and 640×480) and
   bespoke graphical layouts — a piano keyboard at `6 + col*24`
   key positions in mini_synth, a centered sprite + corner HUD in
   sprite_viewer. Neither has an `_lib.bsm` embed split today,
   neither has a panel-rect input, neither has a documented
   layout-misalignment bug. The v2 declarative API was designed
   for "tool that ships as Bang 9 panel + standalone with the
   same body" and pays for itself when chrome-math survives
   resize / embed transitions. Migrating these tools today would
   shuffle hardcoded offsets without delivering bug-class
   evidence — the exact restraint Rule 28's killer-use-case gate
   is designed to enforce. **Re-open the moment either tool
   gains a Bang 9 panel embed** (mini_synth already named as
   the Music tab in `stb++_lib.md` Cap 26's roadmap table —
   embed-contract refactor + v2 declarative migration land
   together when that arc opens).
8. **Session 8 — REVERTED 2026-05-18**: Bang 9 shell migration
   attempted, shipped, then reverted in the SAME session after
   user smoke caught the regression. The original migration
   declared the Bang 9 chrome as `COL win { ROW menu fixed(28)
   + ROW tabs fixed(32) + ROW panel grow + ROW status fixed(24) }`
   and replaced the two chrome-math lines (`panel_y =
   MENUBAR_H + TABSTRIP_H`, `panel_h = win_h - panel_y -
   STATUSBAR_H`) with bbox queries. Builds clean, but every
   embedded tool that itself calls `ui_layout_begin` (Modulab
   S3, level_editor S4, fxlab S5, the_bug S6) hit two
   regressions:

   1. **Node pool clobber.** `ui_layout_begin` resets
      `_ui2_node_count = 0`. The Bang 9 shell's
      `shell_draw_statusbar` reads `ui_node_y(status_idx)` AFTER
      `panels_draw` ran — by which time the embedded tool's own
      `ui_layout_begin` had wiped the pool and reused slot 0..N
      for its own tree. Statusbar rendered at random `(y, h)` —
      the visible "UI distorce" symptom.
   2. **Click buffer double-swap.** `ui_layout_begin` swaps
      `_ui2_curr_clicks` ↔ `_ui2_prev_clicks` and zeros the new
      curr. Calling it twice per frame (shell + embedded tool)
      double-swaps, so `prev_clicks` for the second call is the
      buffer that was just zeroed by the first. Modulab's
      aseprite viewer (uses `ui_button`) got dead buttons —
      `ui_button` reads `prev_clicks[seq]` which was zeroed by
      the shell's swap.

   Root cause: `ui_layout_begin` was designed under the
   "one-per-frame" invariant. Shell + embedded tool both calling
   it violates that invariant.

   Revert restores the manual chrome math
   (`MENUBAR_H + TABSTRIP_H` etc.) — same code that shipped pre-
   S8. Comment in `bang9.bpp` records the design intent and the
   bug class so the next visitor knows why the chrome arithmetic
   wasn't migrated.

9. **Session 8.1 — CLOSED 2026-05-18**: re-attempt of S8 after
   `ui_layout_begin` learned a nested-call discipline. The
   `ui_frame_begin()` + `ui_layout_begin()` split shipped: the
   click-buffer swap moved into a new `ui_frame_begin` that the
   host calls ONCE per frame, before any layout; the layout-begin
   now only resets node pool + sequencer + viewport (safe to
   call multiple times per frame). Same shape as the ImGui /
   Clay frame/layout boundary. Every consumer was updated to
   call `ui_frame_begin` at frame top:
   `bang9/bang9.bpp` + `tools/{modulab,level_editor,fxlab,
   the_bug}/{}.bpp` + `examples/stbui_layout_smoke.bpp`. Bang 9
   chrome migration re-applied: `COL win { menu fixed(28) + tabs
   fixed(32) + panel grow + status fixed(24) }` with shell
   bboxes cached locally BEFORE `panels_draw` runs (because the
   embedded tool's `ui_layout_begin` resets the node pool — same
   reasoning as before, just now the click buffer survives via
   `ui_frame_begin`'s one-per-frame contract).
9. **Session 9 — PARTIAL CLOSED 2026-05-18**: deletion of
   `examples/ui_demo.bpp` (the lone holdout consumer of the v1
   `lay_*` layout helpers + the styled widget family
   `gui_panel_s` / `gui_label_s` / `gui_number_c` / `gui_bar`
   + the `Style` struct). After this removal, the entire v1
   layout API (`lay_push` / `lay_pop` / `lay_x` / `lay_y` /
   `lay_w` / `LAY_DOWN` / `LAY_RIGHT` / `struct Layout` /
   `_lay_advance` / `_lay_stack` / `_lay_top`) and the styled
   widget set have zero source consumers across the whole
   project (verified via per-symbol grep of `tools/ bang9/
   games/ examples/ tests/`).

   **Followup S9.1 — CLOSED 2026-05-18 (same session as S8.1):**
   mechanical deletion of dead symbols from `stb/stbui.bsm`
   completed. ~280 LOC removed: the v1 layout API (`lay_push` /
   `lay_pop` / `_lay_advance` / `lay_x` / `lay_y` / `lay_w` /
   `struct Layout` / `LAY_DOWN` / `LAY_RIGHT` / `_lay_stack` /
   `_lay_top`), the styled-widget family (`gui_panel_s` /
   `gui_label_s` / `gui_number_c` / `gui_button_s` / `gui_bar` /
   `style_new` / `struct Style`), the three theme-shadow caches
   (`_stl_default` / `_stl_button` / `_stl_panel` + their
   `init_ui` malloc blocks), plus every `_lay_advance(...)` call
   and `lay_x(x)` / `lay_y(y)` indirection in surviving widget
   bodies (collapsed to `rx = x; ry = y;` via
   content-pattern sed). `gui_button` was refactored to read
   `_theme.button_bg` / `_theme.button_hover_bg` / `_theme.text`
   directly instead of going through `_stl_button` (since the
   Style cache no longer exists). Suite stayed 174/0/12 native
   + 138/0/48 C-emit; bootstrap byte-stable.

Each session ~3-4h, bisect-friendly per migration. v1 stays
green throughout.

## Critical files

| File | Action | Size |
|---|---|---|
| `stb/stbui.bsm` | EXTEND — declarative layout section appended to existing widgets | ~530 LOC added |
| `examples/stbui_layout_smoke.bpp` | NEW — 3-panel visible demo | ~90 LOC |
| `tests/test_stbui_layout.bpp` | NEW — unit tests for FIT/GROW/FIXED/PERCENT combos | ~210 LOC |
| `tools/modulab/aseprite_view.bsm` | MODIFY (S2) — declare layout via v2 instead of computing rects | -50 LOC net |
| `tools/modulab/modulab_lib.bsm` | MODIFY (S3) — same, replaces `_draw_status` + panel anchoring | -150 LOC net |
| `docs/manual/asset_formats.md` | minor: note v2 widgets in cross-references |
| `docs/manual/tonify_checklist.md` | Rule 23 anchor — stbui_v2 ships floor only; consumers opt-in to v1 OR v2 |

## API design decisions (closed before execute)

1. **Stack-based begin/end pairs over a single declarative
   call.** No macros in B++, but begin/end pairs read naturally
   in the consumer + map to the parser's existing scope rules.
2. **Sizing encoded as words, not structs.** Pass-by-value, no
   allocation, fits B++'s function-call style.
3. **Single arena per frame.** Reset at `ui_layout_begin`,
   never freed until app shutdown. ~3.5 MB cap (Clay's number)
   is overkill — start at 256 KB, bump if a real tool hits it.
4. **No keyboard event queue.** Consumers still call
   `key_pressed` / `mouse_down` directly. Layout engine only
   owns layout — input dispatch stays in the consumer (matches
   current B++ pattern).
5. **Render commands emitted as a flat array of structs**,
   consumed by a thin `ui_render()` helper that dispatches to
   stbdraw primitives. Custom commands (CUSTOM_DRAW with a
   `func() {}` pointer) cover canvas viewports + the Aseprite
   sheet display + any future special-cased rendering.
6. **Hit testing via element IDs**. Each box declares an
   optional `.id = "save_button"` for cross-frame queries
   (`ui_hovered("save_button")`). Auto-generated IDs (e.g.
   `"box_42"` from declaration order) cover the unnamed
   widgets so basic clicks work without explicit IDs.
7. **No theming / styling system in v1.** Colors / fonts /
   sizes are passed directly to each widget. Theme = a thin
   layer of helper functions later if needed.
8. **`ui_render()` flushes the command list immediately**, no
   retained-mode integration. Future: optional batching for GPU.

## Verification

```bash
# 1. Engine ships standalone
./bpp examples/stbui_v2_smoke.bpp -o /tmp/v2_smoke
/tmp/v2_smoke   # window opens, 3-panel demo, hover highlights, click logs

# 2. Layout math is correct
./bpp tests/test_stbui_v2_layout.bpp -o /tmp/v2_test
/tmp/v2_test    # exit 0; covers FIT/GROW/FIXED/PERCENT combos

# 3. v1 consumers still build
./bpp tools/modulab/modulab.bpp -o /tmp/m
./bpp tools/level_editor/level_editor.bpp -o /tmp/le
./bpp bang9/bang9.bpp -o /tmp/b9

# 4. (Session 2+) migrated consumer matches old behavior
./bpp tools/modulab/modulab.bpp -o /tmp/m
/tmp/m games/rts1/assets/sprites/wc1/peasant.json
# Visual: sheet/tag list/preview lay out as before but zoom +/-
# no longer reflows side panels.

# 5. Suite + bootstrap on every commit that touches stb/
bash tests/run_all.sh       # 173 ± new layout tests
bash tests/run_all_c.sh
./bpp src/bpp.bpp -o /tmp/g1 && /tmp/g1 src/bpp.bpp -o /tmp/g2 && cmp /tmp/g1 /tmp/g2
```

## Stop conditions

1. **Engine balloons past ~800 LOC** without a working smoke →
   stop, audit the layout passes. Probably the sizing algorithm
   needs a cleaner abstraction.
2. **First consumer migration (Aseprite viewer) takes > 2h** →
   stop, re-think the begin/end ergonomics. Aseprite viewer's
   3-panel layout is small enough that a 30-min port means the
   API works.
3. **Per-frame allocation creeps in** → arena reset isn't
   matching the cap. Profile + fix before adding more widgets.
4. **Modulab zoom +/- still reflows panels after v2 migration**
   → the new layout has the same bug; arena reset isn't
   isolating between calls. Probably a state leak.

## Out of scope (sidequests for later)

- **Scroll containers** with mouse-wheel + drag — defer until
  Modulab editor's main canvas needs panning on small windows.
- **Theme system** (color tokens, typography scale) — wait for
  a real theme need beyond "dark grey panels".
- **Animation easing** for transitions (button hover fade,
  panel slide) — Clay doesn't do this either; consumers can
  add their own.
- **Text input widget** with cursor + selection — current
  stbui's text_input is fine, just wrap it as a v2 widget.
- **Tooltip overlay** with z-index — wait for a real consumer
  asking.
- **Cross-tool sweep migration** — only migrate when a consumer
  needs a fix anyway. v1 stays maintained until natural
  attrition deprecates it.
- **Layout debug overlay** — toggle a key to draw box bounds +
  IDs over the live UI. Nice-to-have, ships in S2 if it falls
  out naturally.
- **Persistent element IDs across sessions** for retained-mode
  consumers (e.g. animated transitions). Not needed for v1.

## Decisões fechadas

1. **Naming + file layout.** "v2" is INTERNAL CLASSIFICATION only —
   used in this sidequest doc to talk about the work. The actual
   code lives in `stb/stbui.bsm` next to the legacy `gui_*` /
   `lay_*` helpers. New declarative primitives are namespaced
   `ui_*` so there's no symbol collision; consumers `import
   "stbui.bsm"` and reach for whichever family fits. Original
   draft proposed a parallel `stbui_v2.bsm`; user rejected
   (2026-05-17) — extra file confuses consumers + suggests a
   fork that doesn't exist. Done correctly: ONE cartridge that
   gradually picks up new API as old helpers are deprecated.

2. **Migration order.** Aseprite viewer first (~3 panels, 2h).
   Modulab second (biggest payoff). Bang 9 / level_editor /
   fxlab follow individually.

3. **v1 deprecation.** Natural attrition. With 43 raw `gui_button`
   call sites + 0 forcing function, atomic removal would break
   too much. Each consumer migrates on its own schedule. v1
   removed when last consumer moves OR explicit cleanup sidequest
   ships.

4. **Layout tests.** Ship `test_stbui_v2_layout.bpp` covering
   FIT / GROW / FIXED / PERCENT combos. ~150 LOC regression net.

5. **MVP widget palette LOCKED:** `ui_text`, `ui_button`, `ui_box`
   (container with row/col + padding + gap + alignment),
   `ui_image`, `ui_spacer`. NOTHING else without Rule 20 two-
   consumer demand. Specifically NOT shipping in v1:
   `ui_shadow`, `ui_gradient`, `ui_rounded_rect`, `ui_animated_*`,
   `ui_slider`, `ui_text_input`, `ui_palette_swatch`. Migration
   needs that widget? Add it WITH the migration commit + show
   the second consumer.

6. **S2 gate criteria (sharpened).** Aseprite viewer migration
   measured on TWO axes:
   - **LOC delta:** new layout call sites should sum to ≤ 70% of
     the old explicit-positioning math LOC.
   - **Clarity delta:** the new code must read as "row of frames
     | grow spacer | tag list column" — declarative structure
     visible at a glance. If a reviewer can't see the layout
     hierarchy in 30 seconds, the API failed.

   If EITHER axis is ambiguous after S2, **abort before S3**.
   Don't migrate Modulab on a v2 that didn't earn its keep.

## Lessons embedded

1. **Sintoma local frequentemente revela problema arquitetural.**
   Modulab zoom reflow → stbui positioning model não escala.
   User catched the depth, not the surface.

2. **Industry library inspiration > rolling our own.** Clay
   has solved this problem rigorously in C; copy the model,
   adapt to B++ idioms. Same pattern as adotando Aseprite JSON
   ao invés de inventar formato bespoke.

3. **Parallel-ship + gradual migration > big-bang refactor.**
   Five consumers depend on stbui today; atomic swap risks
   weeks of bugs. v2 alongside v1 = each migration verified
   in isolation, bisect-friendly.

4. **Stack-based immediate mode is the B++-native shape.**
   No macros, but begin/end pairs map naturally to the
   parser's scope rules. ImGui-style ergonomics without
   macro syntax.
