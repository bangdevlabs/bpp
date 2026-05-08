# Handoff briefing — B++ since 0.68, and the ModuLab port

*For the Emacs-side agent, 2026-04-20 morning.*

## Where we are

**B++ 0.78.** 78 passing tests, zero failures. Bootstrap sha stable.
ARM64 macOS native + x86_64 Linux cross-compile. All four shipped
games (snake, pathfind, platformer, rhythm) compile with zero
warnings. Snake now plays music recorded inside mini_synth — the
"cobra comeu o próprio rabo" dog-food loop closed on Apr 18.

Three commits since `0.68` (`26254e6`):

- `eed1223` — The book + zero-warning clean compile (Apr 17)
- `e2f4d56` — Infra week day 1: Rhythm Teacher foundations (Apr 18)
- `1068412` — Tool infra bundle: stbwindow, bpp_json, stbui widgets,
  text input (Apr 19)

## What shipped, by commit

### `eed1223` — Docs consolidation + W026 cleanup

- `docs/bpp_manual.md` + `bpp_language_reference.md` merged into a
  single K&R-style book: `docs/the_b++_programming_language.md`
  (16 chapters, 1318 lines).
- `docs/bootprod_manual.md` + `docs/tonify_checklist.md` merged
  into `docs/how_to_dev_b++.md` (9 parts, 830 lines). This is now
  the canonical "how to dev" guide.
- Stale plan docs (`dev_loop_2_plan.md`, `libb_design.md`,
  `maestro_plan.md`, `module_discipline_report.md`,
  `type_defense_plan.md`) deleted — they served their purpose,
  code shipped, history lives in `journal.md`.
- Two W026 false positives fixed: `_stb_gpu_init` and `render_init`
  were annotated `: gpu` but honestly reached IO on shader-compile
  error paths. The effect lattice was right; the annotations
  weren't. Now every game compiles zero-warning.
- `tools/audio/synth/` renamed to `tools/audio/mini_synth/`;
  `synthkey.bpp` renamed to `mini_synth.bpp`;
  `games/snake/snake_particles.bsm` renamed to `particles.bsm`.

### `e2f4d56` — Rhythm Teacher foundations (big one)

Five new or extended stb modules + one new stb module + one new
`src/` module + Rhythm Teacher game:

- **`stbasset.bsm` (new, ~370 lines)** — handle-based asset
  manager. 32-bit handles split `[16-bit generation | 16-bit slot]`.
  O(1) dedup by path via HashStr, stale-handle detection via
  generation check, one table for sprite/sound/music/font. Slot 0
  reserved as null handle. Public API: `asset_load_sprite`,
  `asset_load_sound`, `asset_load_music`, `asset_load_font`,
  `asset_get`, `asset_width/height`, `asset_kind`,
  `asset_unload[_all]`, `asset_count`. Caller always frees the
  returned path strings from stbasset-adjacent APIs.

- **`stbscene.bsm` (new, ~165 lines)** — scene manager with
  deferred switch, register/update/draw/unload lifecycle. Four
  function pointers per scene, 16-slot table. Public API:
  `init_scene`, `scene_register(id, load, update, draw, unload)`,
  `scene_switch(id)` (deferred), `scene_tick(dt_ms)`, `scene_draw`,
  `scene_shutdown`, `scene_current`.

- **`src/bpp_path.bsm` (new, ~206 lines)** — argv[0]-relative
  asset resolution with upward filesystem probe. `path_exe_dir()`
  computes the binary's own directory from `argv[0]`. `path_asset(
  relpath)` joins dir + relpath and probes up to 4 levels to
  accommodate `games/<game>/build/<bin>` layouts where the binary
  sits in a subdir relative to assets. Caches the winning prefix
  on first call. **Auto-injected since 0.75** — every user program
  gets `path_asset` for free, alongside `arena_new` / etc.
  (`bpp_arena` was also promoted at the same time.)

- **`stbmixer.bsm` (extended, +277 lines)** — voice table grew
  from 5 parallel arrays to 11. New voice kinds:
  - `MX_KIND_TONE` — the existing synthkey-style tone synthesis
  - `MX_KIND_SAMPLE` — one-shot stereo s16 playback
  - `MX_KIND_MUSIC` — looping BGM on dedicated slot 8
  Voice pool grew 8 → 10 slots. Three volume buses
  (`MX_BUS_MASTER`, `MX_BUS_MUSIC`, `MX_BUS_SFX`) with 0..100 gain.
  New public API: `mixer_play_sample(buf, n_frames)`,
  `mixer_play_music(buf, n_frames, loop)`, `mixer_stop_music()`,
  `mixer_music_frames()`, `mixer_music_ms()`,
  `mixer_set_bus_volume`, `mixer_get_bus_volume`.

- **`stbsound.bsm` (rewritten, +286 lines)** — `sound_load_wav`
  is now a proper chunk scanner. Supports PCM 8/16/24/32-bit +
  IEEE Float 32-bit, mono (auto-expanded to stereo) or stereo,
  any sample rate (plays at mixer's rate; off-rate = wrong pitch
  but no failure). Skips LIST/INFO/fact chunks between fmt and
  data. The old auto-free-previous-buffer behavior removed —
  caller now owns the buffer. Prints specific stderr diagnostics
  on every rejection path (format code, bit depth, missing chunks,
  channel count). Saved `sound_save_wav` unchanged.

- **`stbecs.bsm` (extended)** — `ecs_component_new(w,
  element_size)` + `ecs_component_at(base, id, element_size)`
  helpers for custom parallel components (Rhythm Teacher's
  RhythmNote doesn't fit the core pos/vel/flags).

- **`stbsprite.bsm` (extended)** — `sprite_draw_frame(tex, x, y,
  fw, fh, sheet_w, sheet_h, row, col, scale)` for spritesheet
  row/col sampling. `anim_frame(elapsed_ms, fps, frame_count,
  loop)` for time-based frame selection. Platform layer picks up
  `_stb_gpu_sprite_uv(tex, x, y, w, h, u0, v0, u1, v1)` for UV
  sub-rectangles.

- **`stbfont.bsm` / `stbimage.bsm`** — stderr diagnostics on load
  failure (file missing, invalid chunk layout, no 'head' table,
  etc).

- **`games/rhythm/` (new)** — full rhythm-genre prototype. 5
  files: `rhythm.bpp` (entry + scene registry), `beat_map.bsm`
  (text-file chart parser), `menu_scene.bsm`, `play_scene.bsm`
  (3 internal phases: DEMO / TRANSITION / PLAY), `results_scene.bsm`.
  Teacher character drawn from primitives (head circle, eye dots,
  mouth rect) with state machine (idle / hit / miss / celebrate).
  Hit windows: ±20 ms perfect, ±60 ms ok, else miss. Both F and
  SPACE fire the snare (either trigger feedback + window match).
  Classic "boom-chick" 4-bar rock groove in `assets/beat_map.txt`.
  Samples copied from ironplate's recorded WAVs.

- **`games/snake/snake_maestro.bpp` (extended)** — loads
  `snake_loop.wav` (recorded in mini_synth) as background music
  via `mixer_play_music(..., loop=1)`. Eat SFX generated in code
  at init via `_gen_shot_sample()` (60 ms 880 Hz square-wave burst
  with decay envelope) for instant attack; the recorded
  `snake_shot.wav` had too-slow onset to line up with the apple-eat
  frame.

### `1068412` — Tool infra for the ModuLab port

- **`stbwindow.bsm` (new)** — native window-level utilities:
  - `window_save_dialog(title, default_name)` — NSSavePanel,
    returns malloc'd path or 0 on cancel
  - `window_open_dialog(title)` — NSOpenPanel, same contract
  - `window_alert(title, message)` — NSAlert info + OK
  - `window_confirm(title, message)` — NSAlert warning + OK/Cancel,
    returns 1/0
  - `window_error(title, message)` — NSAlert critical + OK

  All block `-[runModal]` until dismissed. Backend in
  `_stb_platform_macos.bsm` via `_stb_dialog_save` /
  `_stb_dialog_open` / `_stb_alert_*`. `_gpu_nsstr` dropped its
  `static` guard so dialogs share the same NSString helper.

- **`src/bpp_json.bsm` (new, ~920 lines)** — full reader + writer,
  no external dependencies. Supports objects, arrays, strings with
  complete escape handling (including `\uXXXX` → UTF-8 encoding),
  ints, bools, null, floats (integer portion preserved; fractional
  + exponent consumed but discarded in MVP).

  Two bugs found & fixed during test bring-up:
  - Null-terminator overwrite in the string intern pool:
    `_json_intern` committed `strings_len = off + len`, no slot
    for the \0 → next intern overwrote it → `json_object_get
    ("name")` failed because the comparison read 's' from "snake".
    Fix: reserve the terminator in `strings_len`.
  - Non-contiguous children layout: parser assumed container
    children were stored at `first_child + i` in the node table.
    Recursive descent interleaves nested descendants between
    siblings. Fix: each container allocates its own word array of
    child-node indices; `json_child` dereferences that.
    `json_free` walks types and frees every container's child list.

  Public API:
  - Parse: `json_parse(src, len)` → doc or 0 on error
    (diagnostic on stderr). `json_free(doc)`.
  - Navigate: `json_root(doc)` → idx 0. `json_type(doc, idx)`.
    `json_length(doc, idx)` for containers. `json_child(doc,
    parent, i)` returns child idx. `json_object_get(doc, obj,
    key)` returns child idx or -1.
  - Read: `json_int(doc, idx)`, `json_bool(doc, idx)`,
    `json_string(doc, idx)` (null-terminated pointer into the
    doc's string pool; valid until `json_free`).
  - Typed convenience with defaults: `json_object_get_int(doc,
    obj, key, fallback)`, `json_object_get_string(doc, obj, key)`
    (returns 0 on missing), `json_object_get_bool(doc, obj, key,
    fallback)`.
  - Write: `json_writer_begin(sb)`, `json_write_{object,array}_
    begin/end`, `json_write_{string,int,bool,null}` (for array
    elements), `json_write_{string,int,bool,null}_field` (for
    object members), `json_write_{object,array}_field_begin`
    (for nested-container members). Builder maintains comma
    discipline via a depth-indexed bitmap; output buffer is an
    sb_* string builder the caller supplies.

- **`stbinput` text input stream** — per-frame printable-char ring
  (64 bytes, cleared by `_input_save_prev`, populated from
  `-[NSEvent characters]` by the platform event pump). Public:
  `input_text_len()` + `input_text_char(i)`. Printable range
  0x20..0x7E; control keys (arrows, ESC, backspace) stay in the
  `key_pressed` lane.

- **`stbui` widgets** — three new:
  - `gui_text_input(ti, x, y, w, h)` — editable text field with
    click-to-focus, backspace, Enter commit, caret indicator.
    Caller owns a `TextInput` struct + `maxlen + 1` byte buffer.
    Lifecycle: `text_input_new(ti, buf, maxlen)`,
    `text_input_set(ti, s)`, `text_input_clear(ti)`. Returns 1
    on the frame Enter is pressed while focused.
  - `gui_grid(x, y, cols, rows, cell_size, border_color)` — empty
    cell grid with hover highlight. Returns clicked cell index
    (0..cols×rows-1) or -1. Foundation for ModuLab's pixel canvas,
    tilemap editors, any grid-based UI.
  - `gui_palette(x, y, colors, n, per_row, cell_size, selected)` —
    color swatch selector. Draws the n colors in a wrap-around
    grid, highlights the selected one with a cyan accent border.
    Returns clicked idx or -1.

- **Platform layer `_stb_platform_macos.bsm` (extended)**:
  - `-[NSWindow center]` added before `makeKeyAndOrderFront` — all
    games / tools now spawn centered instead of bottom-left.
  - Dialog + alert helpers via `objc_msgSend` against NSSavePanel
    / NSOpenPanel / NSAlert.
  - NSEvKeyDown now also reads `-[NSEvent characters]` and feeds
    printable ASCII into stbinput's ring via `_input_push_char`.

- **`docs/how_to_dev_b++.md`** — new rebuild-scope rule table. Only
  bootstrap when compiler changes; only full suite when stb/ or
  runtime changes. Game/tool-only changes compile just that
  artifact.

## API surface the Emacs agent can call

If you're building emacs-side tooling that drives B++ or reads its
output, these are the hooks that now exist:

### Asset / path resolution (auto-injected, every program)

```
path_asset(relpath)      — caller-owned malloc'd path; walks up 4
                           levels from the binary to find assets.
path_exe_dir()           — cached directory of the binary.
```

### Native file dialogs (stbwindow, explicit import)

```
window_save_dialog(title, default_name)   — blocking, returns path
window_open_dialog(title)                 — blocking, returns path
window_alert / window_confirm / window_error
```

### JSON (bpp_json.bsm, explicit import)

Full spec above. Round-trip tested with object-of-scalars,
arrays-of-arrays, and mixed docs.

### Text input (stbinput + stbui, comes with stbgame)

```
input_text_len() / input_text_char(i)     — per-frame char stream
gui_text_input(ti, x, y, w, h)            — editable field widget
```

### Asset manager (stbasset, explicit import)

```
asset_load_sprite(path)   — PNG → GPU texture
asset_load_sound(path)    — WAV → s16 stereo (any source format)
asset_load_music(path)    — same as sound but tagged for mixer streaming
asset_load_font(path, px) — TTF at target pixel height
asset_get(h)              — handle → internal pointer
asset_unload(h) / asset_unload_all
```

## ModuLab port plan — starting today (2026-04-20)

User decided: ModuLab port over Wolf3D Phase 1. Deep dog-food path
— B++ compiles itself, plays its own music, paints its own art.

### Reference

Original JS lives at `/Users/Codes/bangdev-monorepo/apps/modulab/`
(original version with extensions) and `/Users/Codes/modulab/src/`
(the cleaner 4-file version that's the porting target). 4 files,
1415 lines total:

- `modulab-core.js` (160 lines) — state, palettes, UNIVERSES
  (NKOTC/NES 8×8, CB/LUCAS 32×32, MCU/BangDev 16×16), dither
  matrices.
- `modulab-canvas.js` (414 lines) — canvas, tools (brush, eraser,
  fill, line, rect, oval, eyedrop, select, stamp), selection,
  undo/redo.
- `modulab-app.js` (606 lines) — UI topbar, palette, patterns,
  timeline, actions, playback, wiring.
- `modulab-io.js` (235 lines) — export PNG / JSON / spritesheet /
  atlas / MCU Sprite16, import PNG/JSON.

### Target layout in B++

```
tools/modulab/
  modulab.bpp           — entry: game_init, audio pump skipped,
                          scene registry, main loop
  core.bsm              — state struct, UNIVERSES, palettes,
                          frames array, undo stack
  canvas.bsm            — tools (brush, eraser, fill via flood,
                          line Bresenham, rect, oval, select,
                          stamp), undo/redo snapshot
  ui.bsm                — topbar with universe selector, palette
                          row (gui_palette), tool sidebar, frame
                          list, timeline, playback
  io.bsm                — export JSON + spritesheet PNG grid;
                          import JSON via window_open_dialog
```

Estimated 5 days of focused work. Every piece of infra already
shipped this week: palette row = `gui_palette`, canvas = `gui_grid`,
filename input = `gui_text_input`, Save As / Open = `window_save_
dialog` / `window_open_dialog`, JSON = `bpp_json`, PNG read
already existed via `stbimage`.

### Key design choice: output format

ModuLab exports palette-indexed JSON:

```
{ "data": [[0, 1, 1, 0],
           [1, 2, 2, 1],
           [1, 2, 2, 1],
           [0, 1, 1, 0]] }
```

Which is **exactly** the format `stbsprite.bsm`'s `load_sprite`
already reads. Zero impedance mismatch — paint in ModuLab, save,
reload in the game. The dog-food loop extends: B++ tools producing
B++ engine assets with a shared native format.

### Plan for today (Apr 20)

Day 1 goal: `tools/modulab/core.bsm` + `canvas.bsm` with core
tools (brush + eraser + fill) working end-to-end. A pixel-paintable
window that exports JSON the engine can reload, even if the UI is
stubby. Then day 2-3: rest of tools + undo/redo + full UI.

Day 2 goal: tools (line / rect / oval / select / stamp) +
undo/redo snapshot stack + frame management.

Day 3 goal: full UI (topbar, palette row, tool sidebar, frame list
+ timeline) + I/O (JSON export + spritesheet grid PNG export +
JSON import via file dialog).

Day 4: universes (NKOTC 8×8 NES palette, CB 32×32 LUCAS, MCU 16×16
BangDev) + dither matrices + stencil + color cycling.

Day 5: integration — paint a sprite in ModuLab, save JSON, reload
it in platformer or pathfind to verify the full loop.

### Open question the Emacs agent should keep in mind

If you do any Emacs-side integration with ModuLab (e.g. a save-
assets-to-project buffer, a watcher that reloads games when art
changes), the canonical asset location is the game's own
`assets/` directory next to its `.bpp` entry. `path_asset` resolves
from the binary dir upward, so a binary at `games/<game>/build/
<bin>` finds `games/<game>/assets/...` transparently. The stbwindow
dialogs return malloc'd paths the caller must free; they're
UTF-8 null-terminated C strings.

## What this means for Emacs-side tooling

Concrete things the Emacs agent can do now that weren't possible
before this week:

1. **Parse `.bpp` project files' `assets/` directories** —
   `bpp_json` can read Aseprite JSON spritesheet metadata,
   Tiled `.tmj` tilemaps, ModuLab palette-indexed sprites — all
   via the same parser.
2. **Generate ModuLab-compatible JSON** for in-editor drafts that
   a game then picks up at runtime.
3. **Build watch-and-reload pipelines** — a B++ game is 50-200 KB
   native binary; rebuild is <1 s; asset hot-reload via
   `asset_hot_reload` (stubbed; mtime-based impl ships when a
   tool needs it).
4. **Compose cross-tool audio** — pattern-record in mini_synth,
   save WAV, embed in any game via `stbasset`.

No API contract changes expected this week. ModuLab port will add
a `tools/modulab/` directory but won't change any shared stb or
runtime API. If that changes (e.g. a `bpp_json` extension becomes
required for floats), it'll land as a separate commit with a
migration note.

---

*Joe / B++ main branch @ 1068412.  Push pending — user decides
when to sync to origin.*
