# Bang 9 Live Game Preview — Phase 4 of Tool Embed

**Status**: Design. Not implemented.
**Prerequisites**: Phase 1 (modulab embed) ✅, Phase 2 (level_editor embed) ✅, folder picker ✅, asset auto-load ✅, Play buttons ✅ (all shipped as of 2026-04-21).
**Value**: The final 20% that makes Bang 9 feel like a real game engine to the artist. Edit a sprite, tab to the preview, see it move. Edit a tile, see the level update while the game runs.

---

## The user's ask (verbatim, 2026-04-21)

> "como que eu monto o level, edito o sprite do personagem e vejo real time em ação, ta faltando esse link"

Translation: "how do I build the level, edit the character sprite, and see it in real-time in action — this link is missing."

That's the design constraint. Not "how do I run the game" (that's the Play button, shipped). The ask is: **see the game running inside Bang 9 while I'm editing it**.

---

## Current state (what exists after 2026-04-21 fixes)

```
 ┌─────────────────────────────────────────┐
 │ File  Edit  View  Build  Help           │
 ├─────────────────────────────────────────┤
 │ Project │ Sprites │ Levels │ Code │ Run │
 ├─────────────────────────────────────────┤
 │                                         │
 │  • Project: browse + navigate folders   │
 │  • Sprites: modulab embedded            │
 │            auto-loads first .modulab    │
 │            .json from assets/sprites/   │
 │  • Levels : level_editor embedded       │
 │            auto-loads first .level.json │
 │            from assets/levels/          │
 │  • Code   : read-only source viewer     │
 │  • Run    : [Build] [Build+Run] [Run]   │
 │            launches game as subprocess  │
 │            (separate window)            │
 │                                         │
 └─────────────────────────────────────────┘
```

The chain **works** end-to-end through subprocess. What's **missing** is the "and see it live without alt-tabbing" piece.

---

## Why subprocess isn't the final answer

The Run button today does `sys_fork() + sys_execve(built_binary)`. The game opens its own window. That's fine for smoke-testing but breaks the flow the user wants:

1. Artist edits `rat_sprite.modulab.json` in Sprites tab.
2. Clicks Run. Pathfind opens in a separate window.
3. Alt-tab to Bang 9 to edit the sprite again.
4. Alt-tab to pathfind to see the change — **doesn't update**. Game loaded the sprite at init.
5. Kill pathfind. Click Run again. New window. Alt-tab…

Every edit cycle costs 3+ window switches and a full game restart. The artist's flow is broken.

---

## Target experience (Phase 4)

```
 ┌─────────────────────────────────────────┐
 │ File  Edit  View  Build  Help           │
 ├─────────────────────────────────────────┤
 │ Project│Sprites│Levels│Code│Run│ Play ● │  ← new tab, live dot = running
 ├─────────────────────────────────────────┤
 │                                         │
 │    ┌───────────────────────────────┐    │
 │    │                               │    │
 │    │   [ PATHFIND GAME HERE ]      │    │  ← rendered INTO the panel
 │    │                               │    │    rect, not a separate window
 │    │   Rat chases cheese. Cat      │    │
 │    │   stalks rat. Live.           │    │
 │    │                               │    │
 │    └───────────────────────────────┘    │
 │                                         │
 │    [Reset] [Pause] [Reload Assets]      │
 │                                         │
 └─────────────────────────────────────────┘
```

Artist edits `rat_sprite.modulab.json` → clicks Play tab → sees rat with new sprite **immediately**. No alt-tab. No restart. No shell.

---

## Three viable architectures

### A. Library embed (same pattern as modulab / level_editor)

Extract pathfind into `games/pathfind/pathfind_lib.bsm` with:

```
pathfind_lib_init()
pathfind_lib_frame(px, py, pw, ph)   // one game tick, drawn into panel rect
pathfind_lib_shutdown()
pathfind_lib_reload_assets()         // re-read sprites / level from disk
```

**Pros**: Same pattern we already know works. No IPC. Zero extra dependencies.
**Cons**: Every game Bang 9 wants to host must be written as a library. Forces the
  author to adopt the `_lib_frame` convention. Couples Bang 9 build to every game's
  code. Scales poorly past pathfind.

**Effort**: ~1 day per game. Pathfind specifically is ~600 lines in one .bpp —
manageable. Harder games with more state will hurt.

### B. Headless game + framebuffer IPC

Game runs as a child process but writes its framebuffer to a shared memory segment
(`shm_open` + `mmap`). Bang 9 reads the segment each frame and blits to the Play
panel. Input flows the other direction (Bang 9 → pipe → game).

**Pros**: Games stay unmodified (beyond a tiny shim that writes to `/dev/shm/...`
  instead of calling `_stb_present`). Any B++ game works.
**Cons**: Requires `sys_shm_open`, `sys_mmap` with MAP_SHARED. Neither exists in
  B++ today. Adding them hits the compiler bootstrap ritual. Also platform-
  specific: macOS shm has warts, Linux is cleaner.

**Effort**: ~2 days backend (sys_shm + sys_mmap MAP_SHARED variants, input pipe
  protocol, shim header), ~1 day integration.

### C. In-process module reload

Bang 9 dlopens the game's compiled module, calls its `pathfind_frame()` exported
symbol every tick, re-dlopens when the file changes.

**Pros**: Zero IPC. Truly live (asset changes = instant reload).
**Cons**: B++ has no dynamic linking yet. Planning this is Phase 6+ territory
  (modulab shipped without it). ELF/Mach-O parser + symbol resolver = big lift.

**Effort**: 2+ weeks. Deferred until B++ has a dynamic linker.

---

## Recommendation: A (library embed) for pathfind specifically

Pathfind is the flagship game and the one the user is demoing. A library-embed
refactor is ~1 day and matches the pattern the user already understands (because
they watched us do it for modulab + level_editor).

The follow-on games (rhythm, synth experiments) get subprocess-only for now.
Upgrade them to library-embed one at a time as they become important.

Option B (shm IPC) becomes the right move once there are ≥3 games we want to
preview live and writing `_lib` wrappers for each hurts.

---

## Phase 4 execution plan (when ready)

### Step 1: Extract `pathfind_lib.bsm` (~4 hours)

Mirror the modulab extraction:

```
games/pathfind/pathfind.bpp          # thin wrapper, main() + game loop
games/pathfind/pathfind_lib.bsm      # init / frame / shutdown / reload_assets
```

Move to `_lib.bsm`:
- Global state (rat_x, rat_y, cat_x, cat_y, invuln_ms, game_over, score, …)
- `init_game()` → `pathfind_lib_init()`
- `update_rat()` + `update_cat()` + `draw_game()` → called from `pathfind_lib_frame()`
- Asset loading (tilemap, sprites) → factored so `pathfind_lib_reload_assets()`
  can re-read from disk at runtime

Keep `pathfind.bpp` as a 40-line standalone wrapper that opens a window and
calls `pathfind_lib_frame(0, 0, 640, 400)` in a loop.

### Step 2: Panel origin shift (~1 hour)

Replace hardcoded `SCREEN_W` / `SCREEN_H` references with `_pf_ox + _pf_pw` and
`_pf_oy + _pf_ph` (same convention as `_ml_ox` in modulab_lib). Mouse coords
already come from `mouse_x()` / `mouse_y()` which are window-absolute, so the
cursor math stays correct.

Specific knobs:
- Tilemap origin: `_pf_ox + 0, _pf_oy + 0` (top-left of panel)
- HUD elements: shift by `_pf_ox`, `_pf_oy`
- Collision bounds: already pixel-relative, no shift needed

### Step 3: Bang 9 "Play" tab (~1 hour)

Add a sixth tab `Play`:

```c
g_tab_count = 6;
*(g_tab_names + 5 * 8) = "Play";
```

`_panel_play(x, y, w, h)`:
1. Lazy-init pathfind_lib on first activation.
2. If Sprites or Levels tab dirty flag is set, call `pathfind_lib_reload_assets()`.
3. Call `pathfind_lib_frame(x, y, w, h)`.
4. Draw control strip below (Reset, Pause, Reload).

### Step 4: Reload hooks (~1 hour)

Modulab and level_editor already own "dirty on unsaved changes" flags. When
the user clicks Save in those tabs, set a global `_g_assets_changed = 1`. The
Play panel checks this flag, calls `pathfind_lib_reload_assets()`, clears the
flag. Near-instant preview update.

### Step 5: Input routing (~30 min)

Key input to the Play panel: all WASD / arrow keys go through Bang 9's
`key_down()` / `key_pressed()` as normal. Pathfind_lib reads them from the
same source modulab does. No extra routing needed.

Gotcha: Bang 9's F1/F2/F5/F7/F8 shortcuts must NOT leak into the game. Either
check `g_active_tab != 5` before dispatching hotkeys, or have pathfind_lib
ignore those specific keycodes.

---

## What to NOT do in Phase 4

1. **Do NOT add new syscalls** for shared memory. Option B is off the table
   until there's ≥3 games to preview.
2. **Do NOT refactor every game to `_lib` form** preemptively. Only pathfind
   needs it for the demo. Others stay subprocess.
3. **Do NOT add dynamic linking.** Option C is Phase 6+.
4. **Do NOT embed the game's window chrome** (title bar, X button). The Play
   panel IS the window — it's inside Bang 9.
5. **Do NOT share the game's framebuffer with Bang 9's panel area by blitting.**
   The game draws directly into Bang 9's framebuffer via the shifted
   `_stb_fb + _pf_oy * pitch + _pf_ox * 4` math. Same trick modulab uses.

---

## Acceptance criteria

Phase 4 is done when this flow works without the artist touching any window
other than Bang 9:

1. Open Bang 9 with `./bang9 games/pathfind`.
2. Sprites tab auto-loads rat_sprite. Draw a hat on the rat. Click Save.
3. Levels tab auto-loads level1. Add a few wall tiles. Click Save.
4. Click Play tab. Rat with hat appears on updated level. Use arrow keys to
   move it. Cat chases. Score ticks up.
5. Tab back to Sprites. Change hat color. Click Save.
6. Tab forward to Play. Hat color updated without game restart.

If any step requires alt-tab or a command line, Phase 4 is incomplete.

---

## Open questions for a future session

- **Audio lifecycle when tabbing away from Play?** Keep audio thread running
  (like modulab's testbed does) or pause on tab-out? Probably pause — saves
  CPU and stops background music bleeding through while editing.
- **Game quits itself?** Pathfind has a game_over state. Does Play tab auto-
  reset or show a "dead — click Reset" overlay? Latter is more respectful of
  the artist's debug session.
- **Frame rate**. Pathfind is 60 fps. Bang 9 is also 60 fps. Double-rate or
  interleave? Shared loop at 60 fps is the clean answer.
- **Panel size vs game native size**. Pathfind expects 640x400. What if the
  Play panel is smaller? Letterbox, scale, or clip? Letterbox with a
  border is the safest, matches what modulab does.

---

## Cross-references

- `docs/bang9_tool_embed.md` — Phase 1/2/3 embed design (modulab, level_editor,
  mini_synth). Same panel-origin shift pattern applies.
- `tools/modulab/modulab_lib.bsm` — reference implementation of the lib
  contract. The `(_ml_ox, _ml_oy, _ml_pw, _ml_ph)` convention copies over.
- `tools/level_editor/level_editor_lib.bsm` — second reference. Shorter
  lib, simpler state, good for deciding how much to factor.

---

## One-sentence summary

Phase 4 is: extract pathfind into a `_lib.bsm` the same way we did modulab and
level_editor, add a Play tab, wire asset-dirty flags to `reload_assets()`.
~1 day, no new syscalls, no compiler changes, unlocks the "edit-live" flow
the artist is asking for.
