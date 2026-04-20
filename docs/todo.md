# B++ Roadmap

## Status — 2026-04-15

Version 0.23.x. Self-hosting, dual-backend (ARM64 native macOS + x86_64
cross-compile Linux), C emitter in parity. **Mini Cooper shipped** (all
seven phases: A + B0 + B1 + B2 + B3 + C + B4). 128-bit SIMD (`: double`
+ eleven `vec_*` builtins) landed in B4. Suite at **65 passed / 0 failed
/ 11 skipped**.

For the story of how we got here, see `docs/journal.md`. This file is
forward-looking only.

**Vision.** B++ makes everything that makes a game — the art, the sound,
AND the game itself. Version 1.0 ships when the Adventure Puzzle demo
runs end-to-end via bangscript, the five demo games are playable, the
full dev loop works (multi-error, debugger, profiler, hot reload,
metaprogramming), and a stranger can download and play without thinking
about the language it was written in.

---

## The 1.0 path

```
 ✅  0.22 — Foundation + multi-error (Dev Loop 1)
 ✅  0.23 — Language syntax complete, extrn + smart promotion,
           smart dispatch Phase 1+2, cache removed, backend reorg
 ✅  Mini Cooper — bitfields, aligned malloc, constfold + DCE,
           ternary, short-circuit, Sethi-Ullman freelist, inline
           : base helpers, hot-local RA, batch 6 tonify, : double SIMD
 ─────────────────────────────────────────────────────────────────────
     ↓ WE ARE HERE ↓
 ─────────────────────────────────────────────────────────────────────
  ✅ Dev Loop 2 — `bug --break`, `--dump-str`, observer fixes
  ✅ Type defense L1-L4 — W025/W026, effect lattice (HEAP/PANIC)
  ✅ stbaudio Day 1-3 — SPSC ring, mixer, stbsound WAV
  ✅ stbinput full keyboard (77 keys, macOS + Linux)
  ✅ synthkey — 4-octave polyphonic synth with WAV recording
 ─────────────────────────────────────────────────────────────────────
     ↓ WE ARE HERE ↓
 ─────────────────────────────────────────────────────────────────────
     stbwindow — lightweight window + native file dialogs (NSSavePanel)
     snake with audio sample (mixer one-shot + stbsound load)
     rhythm — Rhythm Teacher demo                      (first 1.0 game)
     Dev Loop 3 — profiler (bench + sampling)
     Wolf3D Phase 1  — CPU raycaster, 1 level
     Wolf3D Phase 2  — hybrid CPU + GPU
     Dev Loop 4 — hot reload watch mode
     RPG Dungeon demo
     Dev Loop 5 — metaprogramming (`$T` + reflection)
     RTS demo
     bangscript runtime (.bang format + engine)
     Adventure Puzzle demo  ────────────────────────────────────►  1.0
```

---

## Active — in commit order

### Shipped 2026-04-20 — moved to journal

- ✅ `stbpal` — Palette struct + 7 built-in catalogs (MCU-8, NKOTC-4,
  CB-32, DB-32, PICO-8, GB-4, NES-54). Cycling (phase-correct),
  LUT remap (+ flash convenience), palette lerp, `.pal.json` load/
  save. Three tests green.
- ✅ GPU indexed rendering path — Metal shader 4th branch (marker
  192), R8 sprite texture + 1×256 BGRA palette texture, palette
  re-upload ≈ 1 KB / frame per palette. `palette_gpu_upload` +
  `palette_gpu_update` + `sprite_create_indexed` +
  `sprite_draw_indexed` in stbrender.
- ✅ pathfind migrated to indexed path — damage flash via
  `palette_gpu_update(pal_gpu, pal_flash)` on hit (every sprite
  flashes white in a single 1 KB upload).
- ✅ `examples/gpu_palette_cycle` demo — 64×64 GB-4 bands flowing
  down at 150 ms/step.
- ✅ stbdraw Layer byte-grid ops — `layer_get/set/clear/fill`
  (flood 4-connected). level_editor migrated from raw malloc to
  `Layer*`; 40 LOC of duplicated grid logic deleted.
- ✅ stbforge testbed level loader — consumes level_editor's
  `.level.json`; ModuLab auto-loads `testbed.level.json` on Tab.
- ✅ stbforge animation runtime — `testbed_play_animation`,
  `testbed_find_animation`, `testbed_anim_tick`,
  `tb_current_frame_idx`. Loop + one-shot. ModuLab rebuilds an
  "all frames" animation at 8 fps on every Tab-into-testbed.
- ✅ macOS window-sizing fix — `_stb_init_window` now probes
  CGDisplayPixels[Wide/High] and picks the max scale that fits
  with menu-bar + title-bar reserve. Mouse divisor stays in sync
  via `_plt_scale`.
- ✅ UI button audit — `gui_button` centres labels, widths
  adjusted in ModuLab + level_editor for TTF-era 16 px glyphs.
- ✅ sprite_viewer default asset path routed through `path_asset`
  (works from any cwd) + last `sz=1` warning fixed.

### Tomorrow (2026-04-21) — dialog_editor

Third dev tool. Whatever it ends up being (branching dialog
state machine? animation timeline editor? scene composer?) the
motivation is to become the second/third client of something
currently modulab-local — that's what triggers the next
extraction. Scope decision at start of session.

Candidate directions:
- **Dialog / cutscene editor** — node graph of lines + choices,
  character portraits pulled from ModuLab sprite JSON, saves a
  `.dialog.json` loadable at runtime by games.
- **Animation editor** — explicit UI for building animations
  (right now ModuLab auto-derives "all frames at 8 fps"). Scrub
  timeline, per-anim fps, loop toggle, event markers.
- **Scene composer** — wire level_editor tilemaps + ModuLab
  sprites + character animations into a `.scene.json` the
  testbed consumes.

### Low-priority follow-ups

- Testbed controls (A/D/Space) not firing from ModuLab editor.
  Possibly upstream key interception. Investigate when dialog_editor
  surfaces similar issues, or sooner if it blocks playtest flow.
- Recorder `.` / `,` hotkeys bug (carried from earlier session —
  keys map correctly in stbinput but UI doesn't fire them).
- 3 flaky GPU tests (`test_gpu_circle/clear/shapes`) — exit 137
  SIGKILL on macOS codesign cache, oscillate run-to-run. Not
  regressions; the cache clears after a few runs.

### Shipped this sprint (2026-04-16/17) — moved to journal

- ✅ Bug #1 (W025 regression) + Bug #2 (16-seed) — root cause was
  page_count=1 in Mach-O chained fixups. Fixed.
- ✅ Bug #3 + #3b (diag line numbers off) — scan_comment not ticking
  cur_line. Fixed, .bug source map 1-for-1 with source.
- ✅ W025 — Rule 13 nudge shipped.
- ✅ Dev Loop 2 MVP — `bug --break`, `--dump-str`, observer fixes.
- ✅ Type defense L1-L4 — effect lattice with PHASE_HEAP + PHASE_PANIC.
  W025 + W026. 33 extern seeds. 25 diagnostics total.
- ✅ stbaudio Day 1-3 — CoreAudio + SPSC ring + bulk push.
- ✅ stbmixer — 8-voice polyphonic, 4-way waveform fader, dirt, volume.
- ✅ stbsound — WAV save/load.
- ✅ stbinput full keyboard — 77 keys, macOS + Linux.
- ✅ synthkey — 4-octave poly synth with WAV recording.

Full write-up in `docs/journal.md` entry 2026-04-16/17.

### 1. `stbwindow` — lightweight window + native file dialogs

### 2. Dev Loop 3: Profiler (minimal + sampling)

Two tiers:

- **`bench("label") { ... }`** builtin. Times a block via a high-res
  timer, prints ms/cycles. ~30 lines. Manual but immediate.
- **Sampling profiler.** Every N ms interrupt via `setitimer(ITIMER_PROF)`,
  capture the currently-running function via the `bug` stack walker,
  aggregate at exit. ~300 lines. Reuses the `bug` stack walker.

Ships before Wolf3D Phase 1.

### 3. `stbwindow` — lightweight window + native dialogs

Side quest born from synthkey: opening a window + getting key
events currently requires the full stbgame stack (GPU, framebuffer,
game loop). Audio tools, text editors, and utilities shouldn't need
a game engine just for a window.

**stbwindow** (new module) provides:
- Window open/close + event loop
- Key press/release events (same KEY_* enum from stbinput)
- **Native file dialogs**: NSSavePanel / NSOpenPanel on macOS,
  Zenity or kdialog on Linux. Platform-agnostic API:
  `window_save_dialog(title, default_name)` → returns path or 0.
  `window_open_dialog(title, filter)` → returns path or 0.

This is frontend-agnostic — stbgame can build on top of stbwindow
(adding GPU, framebuffer, game loop) while standalone tools use
stbwindow directly.

### 4. Snake with audio sample

Load a WAV recorded from synthkey via `sound_load_wav()`. Play it
as a one-shot when the snake eats an apple via a new mixer API:
`mixer_play_once(buf, n_frames)`. The "cobra comendo o próprio
rabo" demo — B++ tools producing content for B++ games.

### 5. Rhythm Teacher demo (`games/rhythm`)

**Game** (`docs/games_roadtrip.md` Game 1): 320×180 window, 4 lanes,
notes scroll down, timing graded PERFECT/GOOD/OK/MISS, 3 songs,
teacher mode (next key one bar ahead). Demo scope: 3 songs, 1
difficulty, core loop complete.

**Shipped capabilities (stbaudio Day 1-3 + stbmixer + stbsound):**
- ✅ WAV loader/saver — `stbsound.bsm`
- ✅ Mixer with 8 voice slots — `stbmixer.bsm`
- ✅ SPSC ring + CoreAudio AudioQueue — `stbaudio.bsm`
- ✅ Full keyboard input — `stbinput.bsm`

**Still needed:**
- Sample-accurate timing — bpp_beat integration at mixer level
- Streaming background music — looped voice slot
- Audio platform layer: CoreAudio AudioQueue on macOS; ALSA on Linux
  deferred until ELF dynamic linking (see P1).

Uses the maestro pattern: audio callback on an OS-owned thread, main
thread produces samples through an SPSC ring buffer with `mem_barrier()`.
Uses `malloc_aligned(size, 16)` for sample buffers and the B4 `vec_*`
builtins for the mixer's float lanes. ~1-2 weeks.

### 4. Wolf3D Phase 1 — CPU raycaster, 1 level

**Game** (`docs/games_roadtrip.md` Game 2): pure CPU raycaster, every
pixel computed and written by B++ code. Demo scope: 1 playable level,
1 enemy type, 1 weapon, core raycaster at 60 fps, playable start to
finish. If this hits 60 fps on Apple Silicon, B++ is proven for real
game work. Depends on Dev Loops 1-3. Assets: shareware Wolfenstein 3D
Episode 1 files. ~4-6 weeks.

### 5. Wolf3D Phase 2 — hybrid CPU + GPU

CPU does math (raycaster, projection, sort); GPU handles texture
sampling + blending via `_stb_gpu_vertex` quads. Same pattern as Doom 64.
No new modules, no shader changes — mechanical translation of the CPU
loop into GPU quad emission. ~2-3 weeks.

### 6. Dev Loop 4: Hot reload watch mode

`bpp --watch game.bpp`. Detect source change, recompile to a fresh
`/tmp` path, kill old process, restart. Not live code injection —
full restart automation. Ships before RPG. ~200 lines (shell + compiler
flag + process management).

### 7. RPG Dungeon demo

**Game** (`docs/games_roadtrip.md` Game 4): 1 dungeon room, 1 NPC with
dialogue, 1 enemy, 1 item, pause menu with save/load.

**New modules** (reused by RTS and Adventure):
- `stbdialogue.bsm` — dialogue boxes, word wrap, choices (~200 lines)
- `stbinventory.bsm` — item slots, icons (~150)
- `stbmenu.bsm` — nested menus, keyboard nav (~200)
- `stbsave.bsm` — save/load state (~150)

~2-3 weeks. Gameplay simple; the four UI modules are the value.

### 8. Dev Loop 5: Metaprogramming (`$T` + reflection)

Lands during RTS, not before. Two features:

- **`$T` generic parameters**: `swap($T, a, b) { auto tmp: T; ... }`.
  Compiler instantiates one AST copy per concrete type used. ~400 lines.
- **Compile-time struct reflection + code emission**:
  `const generate_serializers(T) { ... }` reads field metadata at compile
  time, emits `save_unit`, `save_building`, etc. Seed exists in
  `const FUNC`; the leap is exposing struct field metadata and
  `emit_function` that injects into the AST. ~1100-2000 lines.

Also the foundation for bangscript's `scene { }` block rewriting.

### 9. RTS demo

**Game** (`docs/games_roadtrip.md` Game 3): 1 map, 2 unit types, 1
building, combat + pathfinding + audio + UI + reactive AI. New
module: `stbanim.bsm` (sprite animation frames + direction tables,
~200 lines). Everything else reuses existing stb infrastructure.
~2-4 months for the vertical slice.

### 10. bangscript runtime (.bang format + engine)

Adventure game DSL. Full spec in `docs/bangscript_plan.md`. **Zero
compiler extensions.** `.bang` files are data parsed at runtime; the
engine is a pure B++ module.

Runtime modules (~980 lines):
- `stbbangs.bsm` (~700) — `.bang` parser + coroutine scheduler +
  command dispatcher + flag machine
- `stbverb.bsm` (~200) — SCUMM-style verb/interaction system
- `stbcursor.bsm` (~80) — cursor state machine

Could land as early as after RPG.

### 11. Adventure Puzzle demo → 1.0

**Game** (`docs/games_roadtrip.md` Game 5): 1 scene, 3-5 hotspots,
1 multi-step puzzle, verb system, cutscene. Stretch: episode zero
(3-4 scenes, 20-30 min). ~4-6 weeks for the prologue. When a player
downloads it, solves the puzzle, watches the cutscene and finishes
without thinking about the language — B++ ships as 1.0.

---

## Adjacent — opportunistic (not blocking 1.0)

### x86_64 perf parity on Linux — B1 freelist + B2 inline

ARM64 has the full Mini Cooper Phase B stack. x86_64 carries only B3
on Linux: B1 + B2 were reverted after a bisect-confirmed regression
that crashes `bpp` self-host on Linux. Narrower probes (freelist off,
inline hoist, `_x64_has_call` always-1) did not isolate the trigger.

Impact: Linux self-compile ~37% slower than macOS ARM64. No
correctness gap — emitted code is valid x86_64, just not as tight.

**Unblock path:**
1. Ship `bug` Linux/x86_64 support (gdbserver backend) — first.
2. Single-step through the self-host crash with the debugger.
3. Restore B1 x64 changes one hunk at a time with instruction-byte
   diffs against ARM64's emission.
4. Re-enable B2 inline.

### ELF dynamic linking

B++ x86_64 emits only static ELF today. Adding PLT/GOT, `DT_NEEDED`,
`PT_INTERP`, `dlopen`/`dlsym` builtins unlocks:
- Vulkan on Linux (libvulkan via dlopen)
- ALSA for stbaudio Linux backend (Rhythm Teacher on Linux)
- Any FFI module on Linux (libpng, sqlite, curl)

~2-3 days of focused work. Snake maestro and Rhythm Teacher on Linux
both wait on this.

### Host-aware compiler + install

Today `bpp` defaults to macOS arm64 and cross-compiles via `--linux64`.
Vision: detect host OS+chip at startup and default to that target. On
Linux x86_64, `bpp game.bpp` produces a Linux ELF with no flag.
`install.sh` mirrors it. Cross-compilation stays available via
`--target`. Transforms B++ from "macOS tool that cross-compiles" into
"native tool that adapts to the host."

### Refinements (each under 50 lines, land while touching the file)

- **`--stats` compiler flag** (~40 lines): module count (cached vs
  stale), function/global/struct counts, peak tokens, wall-clock.
- **Arena-backed AST in the parser** (~30-50): swap per-node `malloc`
  for `arena_alloc(mod_arena, NODE_SZ)`. `stbarena` already exists —
  wiring only.
- **`mem_track` for games** (~50): `mem_alloc(size, tag)` records
  (tag, size) in a static array; `mem_report()` prints per-tag totals.
- **Function dedup Fix 2 + Fix 3**:
  - Fix 2 (~5): `main()` in `.bsm` = fatal error (E223).
  - Fix 3 (~30): `stub` keyword marks intentional override targets;
    dedup treats `stub → real` as always silent.

### Small quality items

- **FFI auto-marshalling** — compiler knows FFI param types but
  ignores them; fixing eliminates `(long long)` casts at call sites.
- **Struct field sugar for compiler internals** — `*(rec + FN_NAME)`
  → `rec.name` for compiler-internal structs. ~20 locations.
- **Multi-return values** — `return a, b;` + `x, y = foo();`.
  Eliminates the `float_ret` / `float_ret2` hack.
- **Retina support** — `contentsScale = 2.0` on CAMetalLayer. Render
  at 2× physical pixels. Trivial once there's a reason.
- **Test all games via the C emitter** — only `snake_gpu` and
  `snake_raylib` have been verified end-to-end through `bpp --c → gcc
  → run`.
- **bootstrap.c regeneration** — currently ARM64-only, needs
  regen with the current C emitter. Low priority until someone wants
  to bootstrap on a platform B++ doesn't run on yet.

---

## Deferred — tracked, revisit when demand arises

- **C emitter SIMD mapping (`: double` + `vec_*`)** — native backends
  (NEON, SSE2) emit `vec_*` as instructions. The C transpile path
  needs `: double` locals typed as `__m128` and the eleven builtins
  mapped to `_mm_*` intrinsics. Today the emitter uses `long long`
  for every local unconditionally — invasive change. Zero demand
  from `bpp --c` today. Tracked; lands when the C backend sees real
  SIMD demand.
- **`restrict` keyword** — useful only when the native codegen does
  aliasing-aware optimization. Not part of Mini Cooper. For the C
  transpile path, a parser-level no-op pass-through would let it flow
  through to gcc. Revisit post-1.0.
- **`offsetof` / field reflection** — P1 for bangscript runtime.
  Exposes compile-time field offsets as runtime constants; enables
  auto-generated serializers. Separate task.
- **Prefetch builtins (`__builtin_prefetch`)** — add when real
  profiling on RTS-sized workloads shows the win.
- **Full aliasing-aware optimizer** — post-1.0. Not in scope.

---

## Game modules (pure B++, no platform deps)

| Module | Status | Notes |
|---|---|---|
| `stbtile.bsm` | ✅ | Tilemap engine |
| `stbphys.bsm` | ✅ | Physics with milli-units |
| `stbpath.bsm` | ✅ | A* pathfinding |
| `bpp_hash.bsm` | ✅ | Hash maps (used by compiler) |
| `stbaudio.bsm` | next | Rhythm Teacher |
| `stbdialogue.bsm` | P0 | RPG (dialogue boxes, choices) |
| `stbinventory.bsm` | P0 | RPG (item slots) |
| `stbmenu.bsm` | P0 | RPG (nested menus) |
| `stbsave.bsm` | P0 | RPG (save/load state) |
| `stbanim.bsm` | P0 | RTS (sprite animation) |
| `stbbangs.bsm` | P0 | bangscript (.bang parser + scheduler) |
| `stbverb.bsm` | P0 | bangscript (verb/interaction) |
| `stbcursor.bsm` | P0 | bangscript (cursor state machine) |
| `stbart.bsm` | post-1.0 | Pixel art primitives |
| `stbnet.bsm` | post-1.0 | TCP/UDP for multiplayer |

---

## Known open bugs

- **`call()` float args on x86_64** — `call(fp, args...)` ignores
  float arguments on x86_64. Doesn't affect native function calls,
  only the indirect `call()` builtin.
- **String dedup across modules** — duplicate strings across modules
  create duplicate entries in the data segment. Harmless but wasteful.
- **`bootstrap.c` is ARM64-only** — see the refinement above.

---

## Post-1.0 (scaling from demo to real game)

The 1.0 demos prove B++ delivers each game category. Scaling from
"1 demo level" to "full shipping game" requires the infrastructure
below. Each item is a 2-6 week project that adds capability without
changing the language.

- **Parallel codegen per module** — use the maestro job pool to make
  codegen multi-threaded. Becomes worthwhile at 50K+ lines.
- **Asset streaming (`stbasset.bsm`)** — load/unload on demand, atlas
  packing, manifest, background loading thread. ~500-1000 lines.
- **Memory budget system** — per-system allocation limits, warnings
  on overshoot, defrag for long sessions.
- **True live code reload** — preserve world state across recompile.
  Serialize → relink changed module → deserialize. Basically a
  second linker.
- **Multi-error recovery across modules** — current multi-error log
  is per-compilation. In a 200-module codebase, cross-module type
  errors need smarter recovery (Rust/TypeScript-level).
- **Profiler GUI (timeline view)** — Chrome-style timeline,
  per-frame breakdown, memory + GPU profilers. Likely a standalone
  `stbtool` that reads a binary trace file.
- **Networking (`stbnet.bsm`)** — TCP/UDP, state serialization,
  prediction/rollback. ~500-800 lines.
- **Target architecture refactor** — chip vs OS split. Trigger:
  first chip+OS combo that doesn't fit today's folders
  (Linux ARM, Intel macOS, Windows x86_64).
- **Vulkan GPU on Linux** — after ELF dynamic linking. libvulkan
  via dlopen, `VK_KHR_xlib_surface` bridge to the existing X11 wire
  protocol. Docker on macOS has no GPU passthrough — real testing
  needs Linux hardware.
- **Tools & editors** — sprite editor, tilemap editor, level
  designer, particle editor. Thin shells over stb modules.
- **Audio tools (CLAP plugin support)** — `--plugin` flag, `export`
  keyword, DSP primitives.
- **Windows / WebAssembly** — new codegen backends. Big work.
- **EmacsOS distribution** — custom Linux distro: Emacs (EXWM) as
  DE, B++ games in X11 windows. Vision project.
