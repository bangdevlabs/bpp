# Bang 9 Space Manual

**Bang 9 is the integrated game-dev space of the B++ ecosystem.**
Not just an IDE. Not just an engine. One process where source
editing, asset authoring, runtime preview, and live tuning share a
window — because they share the same compiler, the same standard
library, and the same hot-reload backbone.

This document is the canonical reference for what Bang 9 is, what
ships inside it today, what conventions hold across every tool that
plugs into it, and how to add new tools without breaking the model.

If you are building a new B++ game and you wonder "where do I edit
this?", the answer is always Bang 9. If the panel doesn't exist
yet, the path to building it is in this document.

---

## The premise

Other engines split the workspace: Unity has the editor and the
build target as different binaries; VSCode hosts the source editor
and you tab to a separate game window; Godot ships its editor and
runtime in one binary but never lets a tool author embed third-party
panels with full subsystem access.

Bang 9 collapses the layers. The same `bang9` binary that hosts
your code editor also runs the sprite painter, the tilemap editor,
the effect tuner, the debugger, and the build/run loop — all
sharing one window, one theme, one input event stream, one
file-watch tick. Tools are plain B++ programs that opted into the
embed contract; the engine plumbing that makes them work as panels
is the same plumbing they use as standalone executables.

This means three things in practice:

1. Any standalone B++ tool can become a Bang 9 panel with a small
   refactor (init/frame/shutdown contract).
2. Any change to the engine (a new shader feature, a new physics
   primitive, a new asset format) gets a corresponding tool the
   same week.
3. Game devs never leave Bang 9. Their entire workflow — from
   spawning a project to shipping a binary — happens in one place.

---

## What ships in Bang 9 today

Each tab is a tool. Each tool exists as both a standalone binary
in `tools/` and as a panel inside Bang 9.

| Tab | Tool | Edits | Standalone | Status |
|---|---|---|---|---|
| Project | (built-in) | Project tree, file open | n/a | Always-on |
| Sprites | modulab | Pixel sprites + atlases | `tools/modulab/` | Shipped |
| Levels | level_editor | Tilemap layers + (planned) entity layers | `tools/level_editor/` | Shipped (single-layer); multi-layer in flight |
| Effects | fxlab | Post-process shader uniforms via JSON manifests | `tools/fxlab/` | Shipped 2026-05-10 |
| Code | (built-in) | Source files (`.bpp` / `.bsm`) | n/a | Acme-style editor |
| Run | (built-in) | Build + run + log | n/a | F7 build / F8 run |
| Debug | the_bug | `.bug` debug map viewer + watch + viz | `tools/the_bug/` | Shipped |

Future tabs (no fixed order, ship when a real consumer needs them):
- **Sound** — sample browser + envelope editor for `stbsound` / `stbmixer`
- **Animation** — timeline for sprite animation + entity FSMs
- **UI** — visual builder for `stbui` panels
- **Shaders** — `.metal` editor with live recompile (Modelo 4 from fxlab arc)

The pattern: when you find yourself editing the same kind of asset
in three different games, the editor has earned its tab.

---

## The hot-reload backbone

Every panel that edits an asset can write the change to disk and
have a running game pick it up within ~30 ms — without rebuilding,
without re-launching, without re-opening the file. Same wire,
across every tool:

```
[ panel writes JSON / asset ] → file system
                                      ↓
[ running game's file_watch_tick polls mtime ] → reload callback
                                      ↓
[ in-memory state updated, next frame uses new value ]
```

This is what makes Bang 9 feel like a real workspace and not a
batch compiler. The file_watch poll runs every ~30 frames (~0.5s
at 60 FPS) by default, with a burst mode that drops to 1 frame
right after a hit so iteration feels instant.

For a panel to participate in this loop, it just needs to write
the file; the game side already polls. The only requirement is that
both processes refer to the **same file path** — covered by the
project layout convention below.

---

## Project layout convention

Bang 9 expects a project to look like this:

```
<project_root>/
├── <name>.bpp                ← entry source file
├── <name>.bsm                ← additional sources (optional)
├── assets/
│   ├── sprites/              ← edited by modulab
│   │   ├── hero.json
│   │   ├── hero.modulab.json (full editor state, optional)
│   │   └── tileset.json
│   ├── levels/               ← edited by level_editor
│   │   └── level1.level.json
│   ├── fonts/                ← TTF or bitmap fonts
│   ├── audio/                ← .wav, .aiff (when sound editor lands)
│   └── ...
├── effects/                  ← edited by fxlab
│   ├── crt.json
│   ├── scanlines.json
│   └── ...
├── shaders/                  ← project-specific .metal (V1.1+)
│   └── my_glow.metal
└── build/                    ← compiler output (auto-created, gitignore-able)
    └── <name>
```

Two namespaces, one rule each:

- **`assets/`** — game content the user creates. Every editor tool
  writes here. `assets/sprites/`, `assets/levels/`, `assets/audio/`
  are the canonical subdirs; new asset types add new subdirs.

- **`effects/`** — engine-shipped configurables (post-process
  shaders today, possibly more later). Project-sacred /
  install-seed pattern: engine ships templates at
  `<install>/effects/`, lazy-copied to `<project>/effects/` on
  first user customization via fxlab.

`stb/` and `src/` are **engine namespaces, never appearing in user
projects**. If a Bang 9 panel ever wants to write a `stb/` directory
under a user project, that is a bug.

---

## The embed contract

Every tool that wants to become a Bang 9 panel ships a `_lib.bsm`
exposing three functions:

```bpp
void <tool>_lib_init(proj_root);                     // one-time setup
<tool>_lib_frame(px, py, pw, ph);                    // per-frame draw + input
void <tool>_lib_shutdown();                          // teardown
```

Rules the contract enforces:

1. **The host owns the UI subsystem boot.** `init_ui()` and
   `theme_dark()` (or whichever theme) are called once by the host
   before the lib's init runs. The lib must NOT call them — doing
   so would clobber the host's palette.

2. **The host owns the window.** The lib paints into a panel rect
   the host gives it (`px, py, pw, ph`). It must NOT call
   `window_init_full`, `window_present`, or any window primitive.

3. **The host owns the project root.** The lib receives `proj_root`
   as init parameter and reads/writes asset files at
   `<proj_root>/<sub>/<file>`. It must NOT use CWD-relative paths.

4. **The lib owns its state.** Module-level statics are fine. The
   lib's init allocates, its shutdown frees. Bang 9 will call them
   in pairs.

5. **The lib's frame returns 0 on continue, 1 on quit.** The host
   ignores the return when it owns global quit (Bang 9 does), but
   honors it for embeds inside richer hosts (e.g. a custom debugger
   shell embedding the_bug in a sub-panel).

The standalone entry binary is then ~30 LOC:

```bpp
import "stbwindow.bsm"; import "stbdraw.bsm";
import "stbui.bsm";     import "stbinput.bsm";
load "<tool>_lib.bsm";

main(argc, argv) {
    auto proj_root;
    if (argc >= 2) { proj_root = argv[1]; }
    else           { proj_root = "."; }

    window_init_full(800, 600, "<tool>");
    init_ui();
    theme_dark();
    <tool>_lib_init(proj_root);

    while (window_should_close() == 0) {
        window_frame_step();
        ui_arena_reset();
        clear(rgba(20, 22, 28, 255));
        if (<tool>_lib_frame(0, 0, 800, 600) != 0) { break; }
        draw_end();
    }

    <tool>_lib_shutdown();
    window_close();
    return 0;
}
```

Same file, two roles: standalone for power users / scripts, panel
for the integrated workflow.

---

## How a new tool becomes a Bang 9 tab

When you've shipped a standalone tool and used it across two games,
promote it to a panel:

1. **Refactor to the embed contract** if not already there. Lift
   `init_ui` / `theme_dark` out of `_lib_init`. Confirm
   `_lib_init(proj_root)` signature; the standalone entry passes
   `argv[1]`.

2. **Add the load + state in `bang9/panels.bsm`:**

   ```bpp
   load "../tools/<your_tool>/<your_tool>_lib.bsm";

   static auto _<your_tool>_initialized;

   static void _panel_<your_tool>(x, y, w, h) {
       if (w < <min_w> || h < <min_h>) {
           _placeholder(x, y, w, h, "<Tab Label>",
                        "Resize Bang 9 window to at least <hint>");
           return;
       }
       if (_<your_tool>_initialized == 0) {
           <your_tool>_lib_init(g_proj_root);
           _<your_tool>_initialized = 1;
       }
       <your_tool>_lib_frame(x, y, w, h);
   }
   ```

3. **Wire the dispatcher branch in `panels_draw`:**

   ```bpp
   } else if (g_active_tab == <new_index>) {
       _panel_<your_tool>(x, y, w, h);
   }
   ```

4. **Add the tab label in `bang9/bang9.bpp`** at the chosen index,
   bump `g_tab_count`, and **shift every hardcoded tab index** in
   `bang9/` that comes after the new one. (Sessão 3 of the fxlab arc
   inserted Effects between Levels and Code — every later index
   shifted by 1. Same shape applies here.)

5. **Document the new tab in this manual's table.** A tool is not
   shipped until the user knows it exists.

Recipe is mechanical. The hard work is the tool itself; integration
is ~50 LOC.

---

## Build + Run + Debug integration

The Run tab is the build/run dashboard. Behind the scenes:

- **Entry resolution**: if a `.bpp` file is open in the Code panel,
  Bang 9 builds that file. Otherwise it scans `<proj_root>` for
  `*.bpp` files (alphabetical) and builds the first match. The old
  `<segment>/<segment>.bpp` convention is fallback only.

- **Build directory**: `<proj_root>/build/<basename>` is created on
  first build (`mkdir -p`). Binary lands there; `.bug` debug map
  goes to `/tmp/bang9_<basename>.bug` to keep the project tree
  clean.

- **Run command**: `cd <proj_root> && build/<basename>` — child
  process inherits `proj_root` as CWD so relative paths in
  `effect_from_json("effects/...")` and `image_load("assets/...")`
  resolve against the project, not Bang 9's launch dir.

- **Debug**: every Bang 9 build emits a `.bug` map. The Debug tab
  lazy-loads it on first activation; subsequent rebuilds invalidate
  the cache via the `_debug_needs_reload` flag.

The whole loop — edit code → F8 → see output → tab to debug — is
~5 keystrokes. No leaving Bang 9.

---

## The "project sacred, install seed" pattern

When a tool ships engine-default values (effect manifests today,
maybe more tomorrow), the dual-location pattern is:

- **`<install>/<asset>/`** — read-only templates shipped via
  `install.sh`. The engine reads here as fallback.
- **`<project>/<asset>/`** — user-owned customization. Created
  lazily by the **editor tool** on first save (NOT by the runtime
  on first read — that would silently pollute every project).

The runtime's resolver reads project-first, install-fallback,
never writes. The editor reads project-first, install-template-
fallback for slider population, but writes only to the project.
Dual-watch from the runtime side covers the case where
customization appears after initial load.

This is the model used by Unity (`Library/` cache vs `Assets/`
sacred), Cargo (`~/.cargo/registry/` vs `target/`). Worth following
for any future asset type Bang 9 supports.

---

## Tools as their own consumers

Every tool ships as both panel and standalone. This is not just
convenience — it's a forcing function:

- A tool that only works as a panel can hide host-coupling bugs
  (assumes a specific theme, a specific window size, a specific
  tab order). Running it standalone catches these.

- A tool that only works as standalone can never integrate
  smoothly. Promoting forces the embed contract early.

The convention makes the contract testable in two configurations
without writing test scaffolding.

---

## Cross-tool workflows

Bang 9's strength is that tools share a runtime. Live workflow
examples that exist today:

- **Tune effect → see in game**: F8 a game with an effect chain.
  Switch to Effects tab. Drag CRT intensity. Game window updates
  in ~30 ms. (Wolf3D Phase 2 calibration loop.)

- **Edit sprite → see in game**: F8 a game that loads
  `assets/sprites/hero.json`. Switch to Sprites tab. Paint pixels.
  Save. Game's `file_watch` reloads the sprite on next tick.
  (Pathfind + modulab loop.)

- **Edit level → see in game**: F8 a game that loads
  `assets/levels/level1.level.json`. Switch to Levels tab. Paint
  walls. Save. Game reloads tilemap. (Pathfind + level_editor;
  Wolf3D Phase 2 will inherit this.)

The cross-process file_watch is what makes these feel like one
program even though Bang 9 + the game are separate OS processes.
The boundary is invisible to the user.

---

## Roadmap signals

What gets built next is driven by which of these workflows hit
friction:

| Signal | Triggered tool |
|---|---|
| "I want enemies and items in my Wolf3D level, not just walls" | Levels gets entity/object layer (in flight, Caminho A — see Wolf3D Phase 2 notes) |
| "I'm tuning sound effects by hand-editing JSON" | Sound tab (sample browser + envelope editor) |
| "I'm hand-coding sprite animation timing in the game source" | Animation tab (timeline + FSM editor) |
| "My game's MSL shader needs constant recompile" | Shaders tab (Modelo 4 from fxlab arc) |
| "I keep building the same UI panels by hand" | UI tab (visual builder for stbui) |

No tool is built ahead of demand. Two-consumer rule fires.

---

## Tonify + bootstrap discipline inside Bang 9 work

Bang 9 panels are stb-tier code: they ship inside the engine
binary. Apply tonify checklist on every change. Specifically:

- **Storage class + visibility**: panel state is `static auto` at
  module scope; helpers are `static`.
- **Phase annotations**: `@base` only for PURE functions. Helpers
  that `malloc` / `str_dup` must NOT be `@base` — leave
  unannotated, the inferencer assigns HEAP. W013 catches the
  violation.
- **Module discipline**: `import` for stb modules, `load` for
  project-local files (sibling `.bsm` in `bang9/`).
- **Comment style**: panel comments read like user manual
  (per global rule). Explain WHY this panel exists, what it edits,
  what convention it follows.

Bootstrap discipline: changes inside `bang9/` and `tools/` are NOT
compiler/runtime/backend changes — no full bootstrap cycle needed.
Compile the affected program, smoke it manually, ship. Bootstrap is
mandatory for `src/`, `src/backend/`, and stb modules the compiler
imports (`bpp_*.bsm`).

---

## File index (where things live)

| Concern | File |
|---|---|
| Tab strip + main loop | `bang9/bang9.bpp` |
| Panel dispatch + each `_panel_*` | `bang9/panels.bsm` |
| Build / run / .bug path resolution | `bang9/runner.bsm` |
| Code editor | `bang9/editor.bsm` |
| Shell menu + dialogs | `bang9/shell.bsm` |
| Project tree directory listing | `bang9/dir.bsm` |
| Embed lib for each tool | `tools/<name>/<name>_lib.bsm` |
| Standalone entry for each tool | `tools/<name>/<name>.bpp` |

When in doubt about the canonical location, search for the closest
analogue (e.g. "how does fxlab do this?") and follow the pattern.
The fxlab arc + this manual together establish the canonical shapes.

---

## What this manual is NOT

- Not a tutorial. For "how do I open my first project", read
  `docs/bpp_manual.md`.
- Not a roadmap. Future tools ship when triggered, not on schedule.
- Not exhaustive. Each tool has its own header comments + standalone
  README where domain detail lives. This manual is the integration
  contract, not the depth.

For the engine vision in the broader B++ context, see
`docs/games_roadtrip.md` (the 1.0 path) and
`docs/how_to_dev_b++.md` (the developer journey).
