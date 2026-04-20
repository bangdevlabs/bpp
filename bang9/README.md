# Bang — BangDev IDE

**PRIVATE. Not part of the public B++ distribution.**

This folder is `.gitignore`d from the public B++ repository. Bang
is a proprietary product of BangDev — an integrated game dev
environment built in B++, targeting artists and designers who
want a turn-key tool to make B++ games without opening a terminal.

## What Bang is

- A single-binary IDE packaged as `.dmg` / `.exe` / `.AppImage`.
- Plan 9 / acme-inspired: minimalist, composable, mouse-friendly.
- Wraps the open `tools/*` (modulab, mini_synth, font_forge, etc)
  as integrated panels within a shared window.
- Provides a minimal code editor (acme-style) for pontual tweaks
  to game mechanics, physics, and values — not a full IDE, just
  enough for artists to adjust things without leaving the tool.
- Ships a project system (`bang.json` at the root) for organizing
  assets and code.
- Integrates build + run: `Ctrl+R` compiles the project with
  `./bpp` and launches the resulting game.

## What Bang is NOT

- Not a replacement for Emacs/VSCode/Vim for hardcore
  programmers. Write your engine code in your own editor; use
  Bang when you want the tools around you.
- Not a full IDE like Unity or Godot. Scope is deliberately
  narrow: art + audio + level + pontual code tweaks.
- Not free. When released, Bang will be a commercial product.

## Relationship to public B++

- Bang depends **only** on the public B++ compiler, stb, and
  tools. No private dependencies. Anyone with the public
  repository can build all the tools Bang wraps.
- Bang's value is the **integration, polish, and packaging** —
  not exclusive access to anything you couldn't do yourself.
- Any language feature, stb API, or tool API that Bang needs
  must be contributed to the public B++ first, following the
  governance process.

## Current structure

```
bang9/
├─ bang9.bpp       # entry point; wires the shell + global hotkeys
├─ shell.bsm       # window chrome: menu bar, tab strip, status bar
├─ panels.bsm      # panel dispatcher (Project / Sprites / Code / Run)
├─ dir.bsm         # directory listing via /bin/ls subprocess
├─ editor.bsm      # acme-style text editor with tag bar
├─ runner.bsm      # build/run subprocess integration
├─ build/          # compiled binaries (gitignored)
└─ README.md       # this file
```

## Hotkeys (current)

| Key | Action |
|-----|--------|
| **F2** | Save current file (Code panel) |
| **F5** | Refresh Project tree |
| **F7** | Build the project (runs `./bpp <entry> -o <out>`, logs to Run tab) |
| **F8** | Build + Run (launches the compiled game if build succeeded) |
| **Click tab** | Switch panel |
| **Click file in Project** | Open that file in the Code tab |

F-keys are transitional — once `stbaction` bindings land, they'll
be replaced by properly-named actions (`action_save`,
`action_build`, `action_run`) rebindable via the Tuning panel.

## Build convention (MVP)

For a project at path `<P>`, Bang 9 assumes:

- Entry source file: `<P>/<basename>.bpp` (e.g.
  `games/pathfind/pathfind.bpp`)
- Output binary: `<P>/build/<basename>` (e.g.
  `games/pathfind/build/pathfind`)

Projects not matching this convention need a future `bang.json`
at the project root with explicit `entry` and `output` fields.
For now the convention is enforced.

## Build Bang 9

```
bpp bang9/bang9.bpp -o bang9/build/bang9
./bang9/build/bang9              # opens window, empty project
./bang9/build/bang9 games/pathfind   # opens with a project
```

## Roadmap

### v0.1 — skeleton (where we are)

Window opens. Menu bar, tab strip, status bar. Tabs switch.
Placeholder panels.

### v0.2 — project panel

Left pane lists files/folders under the project root. Click a
file to see metadata (size, type). Double-click a .bsm/.bpp
opens the Code panel with that file loaded.

### v0.3 — acme-style code editor

Text buffer with cursor, selection, save. Minimal syntax
highlighting (comments, strings). `Ctrl+S` saves. `Ctrl+R`
runs the project.

### v0.4 — Sprites panel (modulab embedded)

Launch `tools/modulab` as a child process OR embed as a
compiled module with a panel-aware rendering mode. Decision
pending — whichever is cleaner in practice.

### v0.5 — Music, Level, Font panels

Same pattern as Sprites for mini_synth, level_editor (when
it exists), font_forge.

### v1.0 — packaging

Icon, splash screen, `.dmg` / `.exe` / `.AppImage` builders.
Distribution via BangDev site.

### v1.1+ — Windows port

Multiplier to reach artists on Windows. Requires new platform
layer + PE writer in B++. Strategic, planned for after MVP
stabilizes on Mac.
