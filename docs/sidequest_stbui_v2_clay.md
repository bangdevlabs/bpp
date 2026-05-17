# Sidequest — stbui v2: Clay-inspired layout for B++

**Status:** GO — design locked 2026-05-17, ready to execute S1.
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
1. **Session 1**: ship stbui v2 engine (containers + sizing +
   layout passes + render-command output) + first widgets
   (`ui_text`, `ui_button`, `ui_spacer`, `ui_image`). One test
   consumer: `examples/stbui_v2_smoke.bpp` that lays out a
   3-panel demo + dumps the render commands.
2. **Session 2**: migrate Aseprite viewer (smallest active
   consumer, 4 panels). Validates the model on a real UI with
   nested boxes + grow + fixed sizing.
3. **Session 3**: migrate Modulab editor — biggest payoff.
   Status line / palette / phase4 / layer / frame strip /
   canvas viewport all flow naturally. Zoom changes only the
   canvas's inner pixel scale, never the surrounding layout.
4. **Session 4**: migrate Bang 9 tabs OR level_editor OR fxlab.
   Whichever the user wants exercised next.
5. **Session 5 (cleanup)**: with three migrations live, deprecate
   v1 widgets that have no consumers left. Document the
   `gui_button(x, y, w, h, label)` rosetta for any holdouts.

Each session ~3-4h, bisect-friendly per migration. v1 stays
green throughout.

## Critical files

| File | Action | Size |
|---|---|---|
| `stb/stbui_v2.bsm` | NEW — engine + widgets | ~600 LOC |
| `stb/stbui_v2_types.bsm` | NEW — `UiBox`, `UiSize`, `UiRenderCmd` structs | ~60 LOC |
| `examples/stbui_v2_smoke.bpp` | NEW — 3-panel demo, asserts layout math | ~80 LOC |
| `tests/test_stbui_v2_layout.bpp` | NEW — unit tests for sizing passes (FIT/GROW/PERCENT combos) | ~150 LOC |
| `tools/modulab/aseprite_view.bsm` | MODIFY (S2) — declare layout via v2 instead of computing rects | -50 LOC net |
| `tools/modulab/modulab_lib.bsm` | MODIFY (S3) — same, replaces `_draw_status` + panel anchoring | -150 LOC net |
| `docs/asset_formats.md` | minor: note v2 widgets in cross-references |
| `docs/tonify_checklist.md` | Rule 23 anchor — stbui_v2 ships floor only; consumers opt-in to v1 OR v2 |

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

1. **Naming.** `stbui_v2.bsm` parallel to `stbui.bsm`. Consumers
   reference either explicitly; v1 stays until natural attrition
   migrates last consumer.

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
