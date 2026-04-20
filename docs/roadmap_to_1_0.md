# Roadmap to B++ 1.0

**Target: `games/pathfind/` shipped as a complete indie game, authored
entirely within the B++ ecosystem via Bang 9.**

This document expands the 1.0 commitment in `README.md` and
`docs/bang9_vision.md` into a concrete, stepped roadmap. Each
step has a deliverable you can verify. Estimates are omitted
deliberately — the project pace has consistently outrun prior
estimates.

Each step runs `sh tests/run_all.sh` green before proceeding, and
rebuilds all seven shipping apps (snake_maestro, snake_gpu, rhythm,
pathfind, platformer, mini_synth, modulab) to confirm no regression.

---

## Stage 0 — ModuLab closure + shared infra (parallel track)

Work executed by the Emacs-side agent, orthogonal to Bang 9
construction. These close the long-running ModuLab Fase A and lay
down infra every Bang 9 stage benefits from. Runs in parallel with
Stages A–D below.

### 0.1 `bench()` runtime helper

New `src/bpp_bench.bsm`. Three functions on top of `beat_now_us()`:

```
bench_begin()                 // start timestamp in µs
bench_end(label, start)       // prints "bench[label] = 1234 us"
bench_section(label, start)   // like bench_end but returns elapsed
```

No compiler changes, no block-syntax sugar (`bench("x") { ... }` is
deferred — needs parser work). ~40 LOC. Test exercises a known-bounded
math loop and asserts `elapsed > 0` + label prints.

**Deliverable**: any perf test from this point on uses `bench_section`
instead of ad-hoc `beat_now_us()` subtraction.

### 0.2 `docs/todo.md` cleanup

Remove the duplicate `### 1. stbwindow` block (shipped earlier, listed
twice). Mark all Fase A items ✅ with today's date. Move the "WE ARE
HERE" pointer past the closure items. Sampling profiler moves to a new
"On-demand infrastructure" section so no future agent treats it as a
blocker.

### 0.3 Dirty-flag on Layer + canvas composite

**Unconditional architectural change, not measurement-gated.** Big
canvases arrive in weeks (Modo 3 playtest, level_editor canvas,
Fase B character animation preview); retrofit in three months costs
more than greenfield today.

**Design — three tracks:**

- **Content dirty bit (per-layer)**: `dirty` byte added to
  `struct Layer` in `stb/stbdraw.bsm`. Flipped by
  `layer_set / layer_clear / layer_fill` and any `pix_set`-style
  write. Also flipped by `layer_set_visible / alpha / blend` since
  those change composite output without touching fb.
- **Structural version counter (canvas-level)**:
  `g_canvas_structure_version` in `tools/modulab/canvas.bsm`, bumped
  by `layer_add / layer_delete / layer_move`. Canvas tracks
  `g_canvas_composite_last_struct_version` alongside
  `g_canvas_composite_last_frame`. Skip check becomes three-way:
  `same frame AND same struct version AND all layers clean`.
- **Compositor decides, primitive doesn't**:
  `layer_composite_indexed` in stbdraw never skips — it's a
  primitive. The caller (canvas.bsm) owns the skip decision. This
  keeps the primitive reusable for tools that want forced
  recomposition (level_editor, test harnesses, GPU composite).

**Risk B measurement, bracketed**:

- Baseline at `n=256` before the change (switch
  `test_modulab_composite_perf.bpp` from `state_init(128)` to
  `state_init(256)`). Record worst-case µs. Expected 12-16 ms.
- Implement dirty-flag. Re-measure same test.
- Add idle sub-test: 100 iterations without mutating any layer
  between composites. Expected worst < 100 µs (skip path).
- Both numbers recorded in `docs/journal.md` entry.

**Test gates** (from measured baseline, not aspirational):

- Dirty path gate = `baseline_worst_us * 1.2` (20 % headroom for
  future regression detection, honest about actual composite cost).
- Idle path gate = 500 µs (skip path does only bit-reads +
  structure-version compare).
- `BASELINE_US` hard-coded as literal in the test file with a
  pointer to the journal entry that produced it — auditable
  provenance.

**Caveats**:

- Dirty bookkeeping must cover every mutation path; direct
  `poke(layer.fb + ..., idx)` bypass would stale the composite.
  Mitigation: skeptical comment over `struct Layer` invariant,
  code review. Existing codebase has ~3 mutation sites, all
  routed through layer API.
- Visibility / alpha / blend toggles correctly flip dirty even
  though they don't touch fb bytes.

### 0.4 `test_stbdraw_ttf.bpp` + committed TTF fixture

New test and new repo asset:
- `assets/fonts/b++.ttf` — Go Mono (Bigelow & Holmes, BSD-3-Clause)
  stripped to ASCII 32-127, ~30-40 KB. Committed as fixture so the
  test ALWAYS runs; silent skip would let TTF regressions sneak in.
- `assets/fonts/LICENSE` — Go font license text preserved.
- `tests/test_stbdraw_ttf.bpp` — loads the font at 16 px, draws an
  "A" and an "O", asserts non-zero pixel coverage and disjoint
  glyph footprints.

**Rationale for Go Mono**: BSD-3-Clause is simpler to bundle than
OFL's reserved-name clause. Same designers as Lucida (Bell Labs /
Unix programming font heritage). Monospace matches B++'s programmer
identity. A later follow-up may add Go Regular (non-mono) at full
character set for text-heavy tool UI — font-lineage decision is
recorded now so no future agent commits yet another font.

### 0.5 TTF caller audit proof

Run:
```
rg 'draw_(text|number)\([^,]+(,[^,]+){3},\s*[1-7]\s*[,)]' \
   --type-add 'bpp:*.bpp' --type bpp \
   --type-add 'bsm:*.bsm' --type bsm
```

Document zero hits in a new `docs/journal.md` block titled "TTF
caller audit cleared". If hits exist, fix inline: `sz = 1/2/3`
becomes `sz = 8/16/24` (pixel-height semantics under the new
TTF-aware `draw_text`).

### 0.6 Looping BGM in stbmixer

`stb/stbmixer.bsm` gains `loop_mode` per voice + wraparound in
the voice step function. API adds:

```
mixer_play_loop(slot, buf, n_frames)   // play + loop on exhaust
mixer_set_loop(slot, on)                // toggle on active voice
mixer_stop(slot)                        // existing; clears loop
```

Implementation: when `cursor >= n_frames && loop_mode != 0`, set
`cursor = 0` instead of stopping the voice. ~120 LOC. Test plays
a 100-sample buf looped, steps 350 frames, asserts voice still
active at step 350.

**Deliverable**: pathfind Stage C2 (intro scene music) can now
use `mixer_play_loop` for ambient BGM.

**Out of scope**: crossfade at loop boundary (polish for
post-1.0).

### Stage 0 order

```
1. bench helpers + test                   ~1h
2. docs/todo.md cleanup                   ~15m
3. Dirty-flag (+ Risk B measurement)      ~1-2h
4. TTF test + caller audit                ~45m
──────── ModuLab Fase A closed ────
5. Looping BGM in stbmixer                ~3h
──────── Stage 0 done, ModuLab graduated ────
```

If any step uncovers more than trivial drift (mutation path
bypassing layer_set, font-load bug, mixer race), the work
stops and gets its own follow-up plan. No half-finished merges.

---

## Stage A — Bang 9 shell becomes usable

### A1. Bang 9 Project panel (file tree)

Read `g_proj_root` from the filesystem, list files and folders in
the panel. Click a .bsm/.bpp sets `g_selected_file` (stored, not
yet acted on). Highlight active selection.

**Deliverable**: `./bang9/build/bang9 games/pathfind` opens,
Project tab shows the real directory tree of pathfind. Click a
file, the selection persists visually.

### A2. Refactor `tools/modulab/` → library mode

Extract modulab's lifecycle (init, update, draw, input, shutdown)
into `tools/modulab/modulab.bsm` as callable functions. Keep a
thin `tools/modulab/modulab.bpp` wrapper that owns the window
and calls into the module — preserves standalone mode.

**Deliverable**: `./tools/modulab/build/modulab` runs standalone
exactly as before, zero regression. `modulab.bsm` can be imported
by other apps without bringing a window with it.

### A3. Bang 9 Sprites panel embeds modulab

Wire `panel_sprites(x, y, w, h)` in `bang9/panels.bsm` to call
`modulab_init / update / draw`. Input events from Bang 9 are
translated to panel-relative coordinates before forwarding.

**Deliverable**: Bang 9 Sprites tab renders modulab's canvas in
the panel area. Mouse works. Save writes to `g_proj_root/assets/`.

### A4. Bang 9 Code panel — acme-style editor

Native text editor in Bang 9 (not a wrapped tool). Text buffer,
cursor, single-line selection, save (Ctrl+S), open file from
Project panel selection. No syntax highlighting yet.

**Deliverable**: Click .bsm in Project panel, it opens in Code
panel. Edit text, Ctrl+S saves. Text file on disk reflects the
edit.

### A5. Bang 9 Build/Run integration

`Ctrl+B` invokes `bpp g_proj_root/<entry>.bpp -o g_proj_root/build/<name>`
via subprocess, captures stderr into the Run panel's log. `Ctrl+R`
builds then executes. Errors in the log render with the source
location; clicking jumps to Code panel at that line.

**Deliverable**: Open pathfind in Bang 9, Ctrl+R — it compiles,
launches, you play the game in its own window. Close it, adjust
code, Ctrl+R again — new version runs.

---

## Stage B — Creative tools embedded

### B1. `tools/level_editor/` standalone MVP

New tool. Grid-based tilemap editor:
- 2D canvas showing tilemap
- Tile palette on the side (reads from project's tileset PNG)
- Paint tiles with mouse drag
- Mark spawn point, goal point, obstacles
- Save/Load `.level.json` matching the format pathfind already
  consumes via stbtile

**Deliverable**: `./tools/level_editor/build/level_editor games/pathfind/levels/level_01.json`
opens, you paint a level, save, pathfind reads it and plays that
level.

### B2. Refactor `tools/mini_synth/` → library mode

Same extraction pattern as A2 applied to mini_synth. Keep
standalone `.bpp` wrapper.

### B3. Refactor `tools/level_editor/` → library mode

Same extraction pattern applied to the tool born in B1.

### B4. Bang 9 Music panel embeds mini_synth

Same pattern as A3. `panel_music` calls
`mini_synth_init/update/draw`.

**Deliverable**: Bang 9 Music tab lets you play keys, record a WAV,
save to `g_proj_root/assets/sounds/`.

### B5. Bang 9 Levels panel embeds level_editor

Same pattern. `panel_levels` calls
`level_editor_init/update/draw`.

**Deliverable**: Bang 9 Levels tab shows the active level,
editable in place, saves to `g_proj_root/levels/`.

### B6. `stbconfig.bsm` — JSON config library

New stb module. Wraps `bpp_json` with a named-path API so
games can keep gameplay values in a `config.json` alongside
code:

```
stbconfig API (in stb/stbconfig.bsm):
  config_load(path)              // loads + caches JSON
  config_float(key, default)     // "physics.gravity"
  config_int(key, default)       // "player.max_hp"
  config_str(key, default)       // "audio.bgm_file"
  config_reload_if_changed()     // call per frame; reloads
                                 //   when mtime changes
```

Under the hood: reads the JSON at init, keeps it parsed in
memory, watches mtime. When the file changes on disk (i.e.
Bang 9 Tuning panel saves), the cached values update on the
next `config_reload_if_changed()` call. Zero rebuild needed.

**Deliverable**: Pathfind refactors its hardcoded constants
(grid size, hero speed, cat speed, level timer) into
`games/pathfind/config.json`. Game behaves identically but
values are now editable at runtime.

### B7. Bang 9 Tuning panel — live JSON editor

Seventh tab in the Bang 9 strip. Reads the project's
`config.json`, renders a tree view with appropriate editors
per type:

- Numbers: slider + number input (range inferred from
  `{"_min": X, "_max": Y}` metadata or autodetected)
- Strings: text field
- Booleans: checkbox
- Nested objects: collapsible groups

Edits save to the file immediately (debounced by ~200 ms).
Game running in another window picks up changes via
`config_reload_if_changed()` — no restart, no recompile.

This is the **visible proof** that the ecosystem is serious:
designer opens Tuning panel, drags a slider while the game
runs, sees the behavior change in real time. Unity's
ScriptableObject workflow, Godot's Resources, Unreal's
DataAssets — same category, self-hosted in pure B++.

**Deliverable**: Bang 9 Tuning tab loads `config.json`, shows
editable values. Drag a "gravity" slider while pathfind is
running: hero's fall speed changes live. Save quits cleanly,
config.json on disk matches the final values.

---

## Stage C — Pathfind content production

### C1. Looping BGM in stbmixer

Extend stbmixer voice state with a `loop_mode` flag. Voice that
finishes a sample wraps back to start if loop_mode is on.
Ship `mixer_play_loop`, `mixer_set_loop`.

**Deliverable**: `tests/test_stbmixer_loop.bpp` passes — voice
keeps playing after sample ends. Used by pathfind for background
music.

### C2. Pathfind intro scene

New stbscene registered as `SCENE_INTRO`. Title screen with
"PATHFIND" text, sprite of hero + cat as decoration, "Press
ENTER to start" prompt. Music fades in during intro. ENTER
transitions to SCENE_PLAY.

**Deliverable**: Launch pathfind, see title screen, press ENTER,
game starts.

### C3. Pathfind 3 levels designed in Bang 9

Using the Levels panel in Bang 9 (via level_editor), design:
- `level_01.json` — introduction (small grid, 1-2 obstacles)
- `level_02.json` — medium (more obstacles, longer path)
- `level_03.json` — hard (narrow corridors, requires planning)

Load sequence hardcoded in pathfind: beat level_01 → load
level_02, etc. After level_03 → SCENE_END.

**Deliverable**: Playing pathfind walks through three distinct
levels, each visually distinct, each fairer or harder than the
last. All three files were authored in Bang 9, not hand-coded.

### C4. Pathfind score system

Track time per level, moves used, deaths. HUD shows current run
stats. End-of-level summary: "Level 1 cleared — 45 s, 127 moves".
End-of-game summary aggregates.

**Deliverable**: Score persists across levels within a run. End
screen shows totals.

### C5. Pathfind end cutscene

SCENE_END registered. Sequenced frame-by-frame sprite animation
(rat reaching safety, cat giving up, "THE END" text), music
swells then fades. Any key returns to SCENE_INTRO.

**Deliverable**: Completing level 3 triggers the cutscene.
Watchable end-to-end in ~30 seconds.

### C6. Pathfind menu (start / continue / quit)

SCENE_MENU as the first scene on launch. Options:
- **New game** — start at level 1
- **Continue** — resume last run if saved state exists
- **Quit** — exits to OS

Save state persisted to `g_proj_root/save.json` between
sessions.

**Deliverable**: Pathfind launches to menu. Can start fresh,
resume, or quit. Save file round-trips properly.

---

## Stage D — Polish + ship

### D1. Asset hot-reload hookup in pathfind

Pathfind calls `asset_hot_reload()` once per frame in its main
loop. Changes saved from Bang 9 Sprites panel appear live in a
running pathfind.

**Deliverable**: Run pathfind, edit hero sprite in Bang 9, save,
pathfind's hero visual updates on the next frame. Zero restart.

### D2. Intro music produced in Bang 9

Open mini_synth in Bang 9's Music panel. Compose a 30-60 second
intro loop. Save as `games/pathfind/assets/music/intro.wav`.
Pathfind loads and plays it during SCENE_INTRO.

**Deliverable**: The music in the shipped pathfind was composed
in Bang 9. Proof artifact: the `.wav` file committed along with
the music's mini_synth project source.

### D3. Sprite polish in Bang 9

Open each sprite in pathfind (hero, cat, walls, goal, effects)
in the Bang 9 Sprites panel. Refine them visually. Save back
to `games/pathfind/assets/sprites/`.

**Deliverable**: Every sprite in shipped pathfind was last-touched
in Bang 9. Git commit history shows the touch trail.

### D4. Font choice finalized

Pick between the current 8×8 bitmap, the hand-drawn 8×16 font
we started, or an external TTF (Departure Mono / m5x7). Commit
the choice, bake it into stbdraw as default. Pathfind's HUD
and scene text use it.

**Deliverable**: One canonical default font across the ecosystem.

### D5. Release build

- Strip debug symbols from the shipping binary.
- Create `install_pathfind.sh` that copies the built game + assets
  to `~/Applications/` or equivalent.
- Write `games/pathfind/README.md` — short player-facing doc
  with how-to-play + credits.
- Git tag `pathfind-v1.0` and `b++-v1.0`.

**Deliverable**: A user downloads B++, runs `install_pathfind.sh`,
plays pathfind 1.0 from start to end without opening a terminal
ever again.

### D6. Announce

Write a short post for:
- Project README (update the "What B++ Has" section with the
  1.0 achievement)
- Journal (`docs/journal.md` — the 1.0 entry)
- Optionally: Hacker News, Itch.io page, personal blog

Narrative: "B++ 1.0 ships today. Pathfind — a complete indie
game — was produced entirely inside the B++ ecosystem via
Bang 9. Every pixel, every sound, every level authored in
first-party tools. Language, compiler, IDE, creative tools,
debugger — all self-hosted."

---

## Post-1.0: Wolf3D as 1.x prestige

After 1.0 ships, the ecosystem has proven itself. The next
flagship is the Wolf3D-inspired game described in
`docs/bang9_vision.md` — our own modern game in retro
aesthetic, built entirely in Bang 9. Scope and timeline to be
planned in a separate roadmap document at that time.

Other things that will queue up post-1.0:
- Windows port (critical for artist audience)
- dialog_editor tool
- cutscene_editor tool
- font_forge polish + maybe shipping a B++-canonical font
- Full file watcher / live reload
- Project templates
- Code panel syntax highlighting + LSP-light features

None of these are gate items for 1.0. They're 1.x+ work.

---

## How to use this document

- If you're a contributor: find the next unchecked step in the
  current stage, implement it, open a PR.
- If you're watching progress: the stage letters (A/B/C/D) are
  the macro phases; the numeric steps within each are sequential
  enough that stage A.5 tells you roughly where we are.
- If you're an AI agent continuing this work: read the Stage
  headers to know what depends on what, and verify each step's
  deliverable is met before marking it done in your todo list.

---

*Document version: 0.1 — 2026-04-20*
