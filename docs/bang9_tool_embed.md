# Bang 9 Tool Embed — modulab / level_editor / mini_synth as panels

**Status**: Design complete. Execution pending.
**Prerequisite**: Current Bang 9 state (commit `0c8d80e`+ later, Sprites tab is placeholder).
**Goal**: End-to-end game production inside Bang 9 window. Artists never leave the project host — no alt-tab between tools, no context loss, no window management friction. This is the Bang 9 vision for B++ 1.0.
**Estimated effort**: 2 sessions (one per phase below).

---

## Phase 1 — modulab embed (priority 1, biggest value for pathfind)

### Current state

- `tools/modulab/modulab.bpp` (787 lines): main() + UI helpers (`_draw_status`, `_draw_help`, `_handle_keys`, `_handle_mouse`, `_handle_topbar`, `_cycle_mode`, etc).
- `tools/modulab/{core,canvas,io,ui,select,prefs}.bsm` (3259 lines total): modular state + widgets. Loaded via `load` (not `import`) from modulab.bpp.
- Hardcoded layout positions in modulab.bpp:
  - `_cvs_x = 144`, `_cvs_y = 60` (canvas origin)
  - `ui_tool_sidebar(12, 64)`, `ui_frame_strip(12, 500)` (left + bottom)
  - `ui_phase4_sidebar(620, 64)`, `ui_layer_panel(620, 290)` (right sidebars)
  - `mx < _cvs_x || mx > 610` (click suppression)
  - `SCREEN_W - 96` (status "[unsaved]"), `SCREEN_H - 18` (help bar)
  - `SCREEN_W - 60`, `SCREEN_H - 18` (testbed REC indicator)
- Expects `game_init(800, 600, "ModuLab", 60)` to have opened an 800x600 window.

### Target architecture

Create `tools/modulab/modulab_lib.bsm` that exports three public functions:

```
// One-time setup. Host calls after its own game_init().
// initial_name: default file to load ("hero" if no argv[1]).
// canvas_n: canvas cells per side (16 is the default).
void modulab_lib_init(initial_name, canvas_n);

// One frame render. Host calls during its game loop after
// game_frame_begin() and clear(). Draws within the given panel
// rect (px, py are window-absolute; pw, ph are dimensions).
// Return value: 1 if user requested quit (ESC pressed), 0 else.
modulab_lib_frame(px, py, pw, ph);

// Cleanup on host shutdown.
void modulab_lib_shutdown();
```

### Layout adaptation strategy

Modulab's layout currently assumes `origin = (0, 0)` and `extent = (800, 600)`. For panel embed, it must accept an arbitrary `(origin, extent)`.

Add panel-rect globals to `modulab_lib.bsm`:

```
static extrn _ml_ox, _ml_oy, _ml_pw, _ml_ph;
```

`modulab_lib_frame` sets these from its arguments at frame top. Then every hardcoded layout coord in modulab.bpp's frame body gets rewritten:

| Before | After |
|---|---|
| `_cvs_x = 144` (init) | `_cvs_x = _ml_ox + 144` |
| `_cvs_y = 60` (init) | `_cvs_y = _ml_oy + 60` |
| `ui_tool_sidebar(12, 64)` | `ui_tool_sidebar(_ml_ox + 12, _ml_oy + 64)` |
| `ui_frame_strip(12, 500)` | `ui_frame_strip(_ml_ox + 12, _ml_oy + 500)` |
| `ui_phase4_sidebar(620, 64)` | `ui_phase4_sidebar(_ml_ox + 620, _ml_oy + 64)` |
| `ui_layer_panel(620, 290)` | `ui_layer_panel(_ml_ox + 620, _ml_oy + 290)` |
| `mx > 610` | `mx > _ml_ox + 610` |
| `SCREEN_W - 96` | `_ml_ox + _ml_pw - 96` |
| `SCREEN_H - 18` | `_ml_oy + _ml_ph - 18` |
| `SCREEN_W - 60` | `_ml_ox + _ml_pw - 60` |
| `SCREEN_W - 54` | `_ml_ox + _ml_pw - 54` |

Mouse handling **does not need translation** because:
- `mouse_x()` / `mouse_y()` return window-absolute coords
- `_cvs_x` / `_cvs_y` are now also window-absolute (after shift)
- Comparison `mx >= _cvs_x && mx < _cvs_x + canvas_pixel_width` works natively

The `_handle_keys` function needs no changes — key input is panel-agnostic.

### ui.bsm / canvas.bsm / other modules

These receive `(x, y, ...)` coordinates as parameters from modulab.bpp's frame body. They don't have hardcoded positions of their own (verify with `grep -nE "^\s*(draw_text|draw_rect) *\(" tools/modulab/*.bsm` — expected: all use parameter-passed coords). If any file has a literal position, note it and shift by `_ml_ox`/`_ml_oy`.

### modulab.bpp main() becomes thin

```
main() {
    auto initial_name;
    if (argc_get() >= 2) { initial_name = argv_get(1); }
    else                 { initial_name = "hero"; }

    game_init(800, 600, "ModuLab", 60);
    modulab_lib_init(initial_name, 16);

    while (game_should_quit() == 0) {
        game_frame_begin();
        clear(rgba(14, 16, 22, 255));
        if (modulab_lib_frame(0, 0, 800, 600) != 0) { break; }
        draw_end();
    }

    modulab_lib_shutdown();
    game_end();
    return 0;
}
```

### Bang 9 Sprites panel

In `bang9/panels.bsm`, replace `_panel_sprites` placeholder:

```
import "modulab_lib.bsm";

static extrn _sprites_initialized;

static void _panel_sprites(x, y, w, h) {
    if (w < 800 || h < 600) {
        // Panel too small for modulab's layout.
        _placeholder(x, y, w, h,
                     "Sprites (ModuLab)",
                     "Resize Bang 9 to at least 1024x768");
        return;
    }
    if (_sprites_initialized == 0) {
        // Auto-detect initial sprite from project convention.
        auto initial;
        initial = _find_first_sprite_in_project();  // scan assets/sprites/
        if (initial == 0) { initial = "hero"; }
        modulab_lib_init(initial, 16);
        _sprites_initialized = 1;
    }
    modulab_lib_frame(x, y, w, h);
}
```

Bang 9's main loop already wraps panels_draw in `game_frame_begin` / `draw_end`, so modulab's frame just composes into Bang 9's framebuffer.

### Risks

1. **ui.bsm widgets with internal hardcoded positions.** Grep confirms 95% of modulab takes x/y as params, but verify. If found, shift by _ml_ox/_ml_oy in the receiving fn.

2. **Screen size mismatch.** Modulab expects >= 800x600. Bang 9 opens at 1024x768 — fits with ~224 pixels horizontal slack (no visible damage; canvas centered or left-aligned). If user resizes Bang 9 below 800x600, show the placeholder.

3. **Prefs file location.** modulab saves prefs to `~/.modulab_prefs.json` or similar. Bang 9 embed should NOT clobber this — same file works for both modes.

4. **Quit handling.** `modulab_lib_frame` returns 1 on ESC. In standalone, this breaks the loop. In Bang 9, IGNORE the return value (ESC closes Bang 9 window, not just the Sprites panel — or we remap to "clear selection" when embedded).

5. **Testbed mode (Tab key).** Modulab's Mode 2 (testbed playtest) takes over full screen. In embed, either:
   - Allow testbed to use the full Bang 9 window (overriding chrome while active) — complex
   - Disable testbed mode in embed (modulab_lib_init could take an `allow_testbed` flag) — simpler
   - Render testbed within panel rect like editor mode (simplest, may look cramped)
   Recommend option 3 for first ship.

### Validation plan

1. After refactor, `./bpp tools/modulab/modulab.bpp -o /tmp/modulab` compiles cleanly.
2. `/tmp/modulab hero` opens standalone, behaves identical to pre-refactor (pixel-push test — draw some pixels, verify saved .json matches).
3. `./bpp bang9/bang9.bpp -o bang9/build/bang9` compiles.
4. `./bang9/build/bang9 games/pathfind` opens Bang 9. Click Sprites tab. Modulab's canvas + palette + sidebar should render within the panel area. Click to paint. Verify pixels appear.
5. Switch to Project tab, back to Sprites — state preserved, no crash.
6. Save a sprite via Put button. File appears in `games/pathfind/`. Verify with `ls -la`.

### Commit template

```
Bang 9 Phase 1: modulab embedded in Sprites panel

Extracts modulab to library mode (modulab_lib.bsm) with
panel-aware layout. Standalone modulab.bpp becomes thin wrapper.
Bang 9's Sprites tab now renders modulab directly in the panel
rect — no subprocess, no window switch.

Layout: all absolute positions in modulab.bpp shifted by panel
origin (_ml_ox, _ml_oy); SCREEN_W/H references replaced with
_ml_pw/_ml_ph. Mouse coords stay window-absolute (comparison
works naturally because both modulab state and mouse are in
the same coord space).

Validated: modulab standalone still paints, saves, loads
identically (pixel-push + .json diff test). Bang 9 Sprites tab
renders modulab canvas + tools within panel. No regression.
```

---

## Phase 2 — level_editor embed (priority 2)

### Current state

`tools/level_editor/level_editor.bpp` (456 lines monolithic).

### Refactor pattern

Same as Phase 1:

1. Create `tools/level_editor/level_editor_lib.bsm`
2. Extract init + frame body from main()
3. Add `_le_ox`, `_le_oy`, `_le_pw`, `_le_ph` panel-rect globals
4. Shift hardcoded positions by panel origin
5. Thin `level_editor.bpp` main()

### Bang 9 integration

Add "Levels" tab to `g_tab_names` in `bang9.bpp`:

```
g_tab_count = 5;  // was 4
g_tab_names = malloc(g_tab_count * 8);
*(g_tab_names + 0 * 8) = "Project";
*(g_tab_names + 1 * 8) = "Sprites";
*(g_tab_names + 2 * 8) = "Levels";   // new
*(g_tab_names + 3 * 8) = "Code";
*(g_tab_names + 4 * 8) = "Run";
```

Add `_panel_level` case in `panels.bsm:panels_draw`:

```
if (g_active_tab == 0) { _panel_project(x, y, w, h); }
if (g_active_tab == 1) { _panel_sprites(x, y, w, h); }
if (g_active_tab == 2) { _panel_level(x, y, w, h); }
if (g_active_tab == 3) { _panel_code(x, y, w, h); }
if (g_active_tab == 4) { _panel_run(x, y, w, h); }
```

With `_panel_level` mirroring `_panel_sprites` but calling `level_editor_lib_frame`.

### Risks + validation

Same pattern as Phase 1 — test level_editor standalone unchanged, test Levels tab inside Bang 9.

---

## Phase 3 — mini_synth embed (priority 3, lowest blocking for pathfind)

### Why lower priority

mini_synth is a **musical instrument**, not an asset editor. pathfind's music is expected to be WAV files (loaded via `stbsound`), not generated live. Mini_synth integration is cosmetic for 1.0 — nice to have a "Music" tab but doesn't block game production.

### Refactor pattern

Same as Phase 1 / 2 for the extraction. Mini_synth is 306 lines — smallest, simplest.

### Bang 9 integration

Add "Music" tab (6 tabs total now). `_panel_music` calls `mini_synth_lib_frame`.

### Special consideration: audio

mini_synth opens `stbaudio` streams on launch. Embedded in Bang 9, audio should:
- Open on first entry to Music tab (lazy init)
- Continue playing even when switching to other tabs? Or pause? User expects the latter — recommend pause + resume on tab activation.

Implement `mini_synth_lib_pause()` / `mini_synth_lib_resume()` called by `_panel_music` based on active tab.

---

## Cross-phase considerations

### Bang 9 imports

After all three phases, `bang9.bpp` imports:

```
import "stbgame.bsm";
import "stbui.bsm";
import "stbpath.bsm";
import "stbwindow.bsm";

import "shell.bsm";
import "panels.bsm";
import "runner.bsm";
import "editor.bsm";

// NEW: tool libraries
import "../tools/modulab/modulab_lib.bsm";
import "../tools/level_editor/level_editor_lib.bsm";
import "../tools/mini_synth/mini_synth_lib.bsm";
```

Verify `import` resolves relative paths correctly, or use absolute paths from project root.

### Shared state concerns

Modulab, level_editor, and mini_synth each have their own global state in .bsm files (`g_canvas_n`, level globals, synth voices). When all three are loaded into Bang 9, their globals coexist. No conflict expected IF they use different names (verify with `grep -h '^auto\|^extrn' tools/{modulab,level_editor,mini_synth}/*.bsm | sort | uniq -d`).

If collision found: rename the smaller tool's globals with tool prefix.

### Build system

`bang9/build/bang9` compilation pulls in all three tool libraries transitively. Build time ~2-3x current (modulab alone is 3000+ lines). Still under 5 seconds on M1.

### Init order

Each tool lib tracks its own `_initialized` flag. Bang 9 calls `*_init` lazily on first panel activation. No cross-tool dependencies.

---

## Out of scope

- Shared asset manager (Bang 9 + tools pointing at same `assets/` folder) — nice but not needed for first ship.
- Auto-refresh: when modulab saves a sprite, level_editor picks up new tile sheet. Requires event bus or mtime poll. Deferred.
- Cross-tool drag/drop (drag sprite from Sprites tab into Levels tab as tile). Deferred.
- Undo/redo unified across tools. Each tool keeps its own history today. Unifying is future UX work.
- Tool preferences persistence (save tab position on quit, restore on open). Deferred.
- Remove standalone `modulab.bpp` / `level_editor.bpp` / `mini_synth.bpp` entry points after embed works. Keep as parallel mode for the 1-3 artists who prefer standalone windows. Can remove later.

---

## Acceptance criteria (for 1.0 ship readiness)

After all three phases:

- [ ] Bang 9 opens with 5-6 tabs: Project, Sprites, Levels, Code, Run, (Music)
- [ ] Click Sprites → modulab's full UI renders within panel, functional
- [ ] Paint in Sprites, Put → saves to `games/<project>/assets/sprites/<name>.json`
- [ ] Switch to Code, back to Sprites — state preserved
- [ ] Click Levels → level_editor renders, functional
- [ ] Click Music → mini_synth renders, can play notes (optional for 1.0)
- [ ] Press Build (F7 or tag bar) → `./bpp games/<project>/<project>.bpp -o <...>/build/<project>`
- [ ] Press Run → launches the compiled game in its own window (existing behavior)
- [ ] Existing standalone `modulab`, `level_editor`, `mini_synth` binaries still work identically
- [ ] All 78+ existing tests pass (no regression in compiler or stb)

When all checked: **Bang 9 is the artist workbench for B++ 1.0.** Ship.

---

## Session plan

**Session A (modulab embed, Phase 1):**
- 2-3h focused
- Output: 1-2 commits (refactor + integration)
- Acceptance: checklist items 2-4 above

**Session B (level_editor + mini_synth, Phases 2+3):**
- 2-3h focused
- Output: 2 commits
- Acceptance: remaining checklist items

Total: 4-6h across two sessions. Ship in a day if you're focused, or a weekend at relaxed pace.

---

## Pitfalls

1. **Don't skip standalone validation.** After each phase, rebuild and smoke-test the standalone tool. Regressions are easy to miss when focused on the embed.

2. **Don't try to "fix" modulab's layout during refactor.** If positions look funny at weird aspect ratios — ship first, polish later. The refactor's scope is "make panel-aware," not "redesign UX."

3. **Don't merge all tool globals into one namespace.** Each tool's state stays in its own .bsm files. Bang 9 just calls their public APIs.

4. **Don't assume Bang 9 panel is always >= 800x600.** If smaller, show a "resize window" placeholder like the code snippet above shows. Don't try to scale modulab's UI to fit — that's layout hell.

5. **Don't forget Run tab integration.** After Bang 9 builds a game and the user presses Run, the game opens in its own window (via `runner_run`). This is the CURRENT behavior and should be preserved — the game isn't embedded in Bang 9, only the tools are. Games get their own window (game_init opens a fresh one).

---

*Written 2026-04-20 late evening. Claude-code session handoff to
Emacs-side agent. After this doc ships, Bang 9 closes as "the
B++ artist workbench" and pathfind production starts end-to-end
inside it.*
