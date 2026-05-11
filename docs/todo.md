# B++ Roadmap — Pending Only

Forward-looking. What shipped lives in `docs/journal.md`. The full
back-catalogue of completed phases (Wolf3D Phase 1, V3 fn-type
checking, GPU pipeline arc Phases 4–7) is mirrored in commit
history.

## Status — 2026-05-09

Version 0.23x. Self-hosting, dual-backend (ARM64 native macOS +
x86_64 cross-compile Linux), C emitter in parity. Suite at
**142 passed / 0 failed / 12 skipped** (native) +
**115 passed / 0 failed / 39 skipped** (C). Bootstrap byte-stable
from either repo-root or `tests/` cwd (runner pinned to REPO_ROOT
since `c8c09e8`).

**Recent landings:**
- 2026-05-07 — GPU pipeline arc CLOSED (Phases 4-7): pixel-perfect
  default, parallax, 4 post-process effects, `@profile` scoped zones.
- 2026-05-08 — Phase 6.3 v2: scoped-cleanup epilogues for `@profile`
  (early return + panic no longer leak open zones).
- 2026-05-09 — fxlab arc: Sessão 1 (substrate) + Sessão 2 (GUI) shipped.
  JSON-driven effects + file_watch hot-reload + standalone tuner with
  preset list + auto-generated sliders + save-on-release JSON write-back.
- 2026-05-10 — fxlab Sessão 3 + polish: Bang 9 `_panel_fx` + 4 bug
  fixes (runner entry resolution, project-sacred read-only resolver,
  dual-watch on fallback, fps_wolf3d game_frame_begin + drop per-frame
  effect_set). Hot-reload visually validated end-to-end: drag slider
  in Bang 9 → fps_wolf3d responds in ~30ms. **Arc CLOSED.**
- 2026-05-10 — Layout cleanup: `stb/effects/` → `effects/` (single
  root). `stb/` is engine-internal only (`.bsm` + `.metal`). `effects/`
  is the user namespace, mirrored across engine source / install /
  user project.
- 2026-05-11 — Wolf3D Phase 2 Session 0 (Caminho A): level_editor
  ganha entity layer (Tiles ↔ Objects toggle, kind picker, marker
  overlay), JSON schema v2 (`tiles[][]` + `entities[]`), pathfind
  level migrado in-place, fps_wolf3d carrega level via JSON loader
  (replaces hardcoded ASCII maze), `bang9_space_manual.md` canoniza
  Bang 9 como engine/IDE da bangdev. Drop `.level.json` double-suffix
  (now `.json` matching sprites/effects). bang9 artifacts cleaned
  (`level1.level.json` deleted, `modulab_prefs.json` `.gitignore`d).

**In flight:** content arc decision point — Wolf3D Phase 2 / Adventure /
RPG / RTS. Engine no longer the bottleneck.

**Sidequest open:** custom shader override (`<project>/shaders/*.metal`
with project-first / install-fallback resolution in
`gpu_pipeline_load`, ~10 LOC stbshader). Ships when a user game wants
a non-stb shader. Today `stb/shaders/` stays engine-internal.

---

## The 1.0 path (what's left)

```
 ✅  0.22 — Foundation + multi-error (Dev Loop 1)
 ✅  0.23 — Language syntax complete, smart dispatch, Tonify v1+v2
 ✅  Wolf3D Phase 1 — fps_3d.bpp walks at 60 FPS (CPU raycaster)
 ✅  V3 — function-pointer type checking
 ✅  GPU pipeline arc — Phases 4-7 (pixel-perfect → effects → @profile)
 →  Content arc — pick one: Wolf3D Phase 2 / Adventure / RPG / RTS
 →  Dev Loop 4 — hot reload watch mode
 →  Dev Loop 5 — metaprogramming ($T + reflection)
 →  bangscript runtime — .bang format + engine
 →  Adventure Puzzle demo → 1.0
```

Order of the next four content milestones is the player's call. The
engine doesn't gate any of them.

---

## Active — content arc decision point

Engine is no longer the bottleneck. Pick one to drive the next
sprint:

### A. Wolf3D Phase 2 (~7 sessions)

Sprites + enemies + audio + doors + level files. Uses Phase 3
sprite batching + Phase 6 CRT. Procedural wall textures from
Phase 6.4 stay in place; PNG-loaded artwork can swap in via
`image_load(...).tex` without touching the shader.

Reference: `games/fps/HANDOFF.md` (last updated for Phase 1
close-out), `docs/games_roadtrip.md` Game 2.

### B. Adventure Puzzle demo (~10 sessions)

Thimbleweed-flavored. Uses Phase 5 parallax + Phase 6 effects
(watercolor, vignette). Bangscript runtime (`stbbangs.bsm`) is
the foundation — see Section "bangscript runtime" below.

Reference: `docs/bangscript_plan.md`, `docs/games_roadtrip.md`
Game 5.

### C. RPG Dungeon (~2-3 weeks)

Four new modules reusable by RTS and Adventure: `stbdialogue`,
`stbinventory`, `stbmenu`, `stbsave`. The four UI modules are
the value; gameplay is intentionally simple.

Reference: `docs/games_roadtrip.md` Game 4.

### D. RTS demo (~2-4 months for vertical slice)

Sprite batching + zoom pixel-perfect + reactive AI. New module:
`stbanim` (~200 LOC). Everything else reuses existing
infrastructure.

Reference: `docs/games_roadtrip.md` Game 3.

---

## Engine-side sidequests (not blocking content arc)

These tighten the engine but don't gate any content. Pick up
opportunistically.

### Multi-core completion — Sprints A-G

Full spec: [`docs/multicore_state_report.md`](multicore_state_report.md).

Sprint 1 + Sprint 2a/2b (`+`) shipped 2026-04-30; ~1010 LOC
remain to close the 8 gaps end-to-end. In priority order:

- **Sprint A** — stride / start rewrite (~40 LOC). Today the
  scanner detects `stride != 1` and `start != 0` but the
  rewriter bails out to serial. A chunked `job_parallel_for`
  variant + matching dispatch path closes both at once.
  Unblocks SIMD-aware pixel loops and partial-iteration
  parallel maps.
- **Sprint B** — non-additive reductions (~30 LOC). Identity-
  element table for `*`, `min`, `max`, `&`, `|`, `^`. Scanner
  + rewrite already exist for `+`; this just extends the
  identity / merge tables.
- **Sprint C** — channels (~80 LOC). `chan_send / recv /
  try_send / try_recv / close` on top of the SPSC ring already
  proven in `bpp_job`. Unblocks audio→main thread events,
  worker progress reporting.
- **Sprint D** — atomic primitives minimum (~50 LOC).
  `atomic_load`, `atomic_store`, `atomic_fetch_add`. Per-chip
  primitive (a64 + x64), exposed as builtin. Unblocks
  Naughty-Dog-style job dependency counters.
- **Sprint E** — Linux thread parity (~500 LOC). Depends on
  ELF dynamic linking (separately tracked above). Single
  unblock = 8-32× parallelism on Linux hosts. **Highest
  capability priority** — until this lands, every Linux
  program is single-threaded regardless of dispatch.
- **Sprint F** — CPU feature detection (~300 LOC). `cpu_has`
  builtin + per-OS detection (sysctl on macOS, /proc/cpuinfo
  on Linux). Enables AVX2 / SVE fast-path dispatch.
- **Sprint G** — map / reduce sugar APIs (~50 LOC). Pure
  ergonomic wrappers (`arr_map(in, n, fn, out)`,
  `arr_reduce(in, n, op, init)`) — capability already exists
  via Sprints A+B.

Recommended ordering: A → B → D → C → (ELF dynlink) → E → F →
G. Sprints A-D land as foundation work between game features;
E waits on the dynlink prerequisite; F+G land last.

### Tier F compiler optimizations — gating still applies

Full spec: [`docs/compiler_boost_roadmap.md`](compiler_boost_roadmap.md).

Three multi-node optimization passes (CSE, RegAlloc v2, Auto-
vectorization) that are intentionally DEFERRED until profile
data justifies them. Doc gating:

> **Do not open Tier F until:**
> 1. A real plugin exists, runs, and is profiled.
> 2. The profile shows one of: CSE waste, register spill /
>    reload time, scalar-inner-loop dominance.
> 3. Someone owns the resulting project end-to-end.

Status today (2026-05-07): none of the three conditions have
fired. Single-node opts (Phase D-style strength reduction,
inline trivials) continue to land opportunistically and capture
the wins that don't need profile evidence. Tier F sits as a
shovel-ready 2-3-week project per pass when a real consumer
flags the bottleneck.

### Linux Vulkan parity for the GPU pipeline

`stb/_stb_gpu_linux_vulkan.bsm` (new). Mirrors the macOS Metal
layer: device + command queue + render pipeline + texture upload
+ vertex buffer. ~1500 LOC of dedicated work. Unlocks GPU games
on Linux end-to-end. Doesn't block any phase — Phase 4–6 ship on
macOS today, Linux remains "cross-compiles but doesn't run GPU"
until parity arrives.

### `effect_palette_quantize` — the 5th effect

Deferred from Phase 6.2. Needs real stbpal integration (palette
uploaded as a 1D texture or constant array, per-pixel
nearest-colour search in the fragment shader). ~100 LOC. Ships
when an actual game asks for indexed-palette post-process.

### `fxlab` tool (`tools/fxlab/`) — ARC CLOSED 2026-05-10

Renamed from the original `fx_playground` proposal. Same intent:
preset tuner that loads effect manifests, exposes uniform sliders,
hot-reloads to running games via file_watch. V1 shipped JSON
manifests + slider GUI standalone + Bang 9 panel; live `.metal`
editor (Modelo 4 of the original design discussion) deferred
until real signal emerges from the V1 tuner in use.

**Driver:** Wolf3D Phase 2 needs to calibrate 5+ effects × 3-4
params each. Manual edit-build-run loop is insane at that volume.

**Plan:** `~/.claude/plans/groovy-munching-seahorse.md`. 3 sessions,
all shipped:

- Sessão 1 — substrate. JSON manifests +
  `effect_from_json` + `effect_set` + file_watch wiring; 4 effects
  migrated; visually validated end-to-end via `fps_3d_gpu` and
  `fx_library_smoke`.
- Sessão 2 — fxlab GUI standalone (~470 LOC `fxlab_lib.bsm` + ~30
  LOC entry). Preset sidebar + auto-generated sliders per param +
  save-on-release JSON write-back. **Pure JSON editor — does not
  load shaders itself**, the running game in another process
  holds the FxEffect handle and reacts via file_watch_tick.
- Sessão 3 — Bang 9 `_panel_fx` between Levels and Code (~25 LOC
  panel + tab strip update + embed contract refinement: lifted
  `init_ui` / `theme_dark` out of `fxlab_lib_init` to match the
  modulab / level_editor convention so the host owns UI subsystem
  boot + palette).

V2+ candidates if signal emerges from Wolf3D tuning:
- Auto-discover presets from `stb/effects/*.json` (V1 hardcodes
  the 4 in `fxlab_lib_init`; needs an OS-portable `dir_list` that
  doesn't shell out — graduate from `bang9/dir.bsm`).
- Live `.metal` editor (Modelo 4 from original design).
- Effect-chain ordering UI (drag-reorder which effect runs first).

### Modulab prefs path migration to user config dir

`tools/modulab/prefs.bsm` writes to `<exe_dir>/modulab_prefs.json`.
Same CWD/install path bug class fxlab arc fixed in May 2026 — prefs
end up next to the binary instead of in user space. When Bang 9
ships installed, this pollutes `/usr/local/bin/`.

Migration: write to `~/.config/bpp/modulab_prefs.json` (XDG on
Linux, `~/Library/Application Support/bpp/` on macOS). ~30 LOC in
prefs.bsm: add `_user_config_path()` helper, swap `<exe_dir>` for
it. The file is currently `.gitignore`d so accidental commits don't
re-leak the bug.

Defer until: someone hits the bug shipping Bang 9 to a non-dev
machine.

### x86_64 perf parity on Linux — B1 freelist + B2 inline

ARM64 has the full Mini Cooper Phase B stack. x86_64 carries
only B3 on Linux: B1 + B2 were reverted after a bisect-confirmed
regression.

**Unblock path:**
1. Ship `bug` Linux/x86_64 support (gdbserver backend) — first.
2. Single-step through the self-host crash with the debugger.
3. Restore B1 x64 changes one hunk at a time with
   instruction-byte diffs against ARM64's emission.
4. Re-enable B2 inline.

### C-emitter `var T` parity

Native and C disagree on the storage location of `var s: Pt`
locals. Native allocates a stack slot and `&s` is FP-relative.
C lowers `var s` to `long long s[N]` (array decay-as-pointer),
and `&s` had to be special-cased in the T_ADDR emitter.

Forward path (when a real consumer hits this — currently no
game needs it):

1. Pick one storage strategy for `var T` and document it across
   both backends. Most likely "always heap-backed" — uniform,
   predictable, and `var p: Pt` becomes literal sugar for
   `auto p: Pt; p = malloc(sizeof(Pt));` with a matching
   `free` at scope exit.
2. Update native `_a64_emit_addr_local` and the C T_VAR/T_ADDR
   emit paths to share the same address calculation rule.
3. Add a regression test that takes `&local_struct`, passes it
   to another function, and reads a field both directions.

### `bug` headless detection

`bug file.bug` (no `--dump`) routes to the GUI viewer. In a
headless environment (CI, piped invocation, no DISPLAY/
WindowServer), Cocoa opens the window and waits for the event
loop with no human to drive it. Today the only signal is a
one-line stderr hint at GUI launch. When stdin/stdout is
verifiably not a TTY, auto-fallback to `--dump` instead. Needs
`isatty(0)` — `sys_ioctl(0, TIOCGETA, ...)` or
`sys_fstat(0)` checking `S_IFCHR`. ~30 LOC.

Defer until: someone files an actual issue or runs `bug` in CI.

### ELF dynamic linking

B++ x86_64 emits only static ELF today. Adding PLT/GOT,
`DT_NEEDED`, `PT_INTERP`, `dlopen`/`dlsym` builtins unlocks:
- Vulkan on Linux (libvulkan via dlopen)
- ALSA for stbaudio Linux backend
- Any FFI module on Linux (libpng, sqlite, curl)

~2-3 days of focused work. Snake maestro and Rhythm Teacher on
Linux both wait on this.

### Host-aware compiler + install

Today `bpp` defaults to macOS arm64 and cross-compiles via
`--linux64`. Vision: detect host OS+chip at startup and default
to that target. On Linux x86_64, `bpp game.bpp` produces a
Linux ELF with no flag. `install.sh` mirrors it.
Cross-compilation stays available via `--target`.

### Refinements (each under 50 lines, land while touching the file)

- **`--stats` compiler flag** (~40 lines): module count, function/
  global/struct counts, peak tokens, wall-clock.
- **Arena-backed AST in the parser** (~30-50): swap per-node
  `malloc` for `arena_alloc(mod_arena, NODE_SZ)`. `stbarena`
  already exists — wiring only.
- **`mem_track` for games** (~50): `mem_alloc(size, tag)` records
  (tag, size) in a static array; `mem_report()` prints per-tag
  totals.
- **Function dedup polish**:
  - `main()` in `.bsm` = fatal error (E223). ~5 LOC.
  - `stub` keyword marks intentional override targets; dedup
    treats `stub → real` as always silent. ~30 LOC.

### Small quality items

- **FFI auto-marshalling** — compiler knows FFI param types but
  ignores them; fixing eliminates `(long long)` casts at call
  sites.
- **Struct field sugar for compiler internals** —
  `*(rec + FN_NAME)` → `rec.name` for compiler-internal structs.
  ~20 locations.
- **Multi-return values** — `return a, b;` + `x, y = foo();`.
  Eliminates the `float_ret` / `float_ret2` hack.
- **Retina support** — `contentsScale = 2.0` on CAMetalLayer.
  Render at 2× physical pixels. Trivial once there's a reason.
- **Test all games via the C emitter** — only `snake_gpu` and
  `snake_raylib` have been verified end-to-end through
  `bpp --c → gcc → run`.
- **bootstrap.c regeneration** — currently ARM64-only, needs
  regen with the current C emitter. Low priority until someone
  wants to bootstrap on a platform B++ doesn't run on yet.

---

## bangscript runtime (.bang format + engine)

Adventure game DSL. Full spec in `docs/bangscript_plan.md`.
**Zero compiler extensions.** `.bang` files are data parsed at
runtime; the engine is a pure B++ module.

Runtime modules (~980 lines):
- `stbbangs.bsm` (~700) — `.bang` parser + coroutine scheduler +
  command dispatcher + flag machine
- `stbverb.bsm` (~200) — SCUMM-style verb/interaction system
- `stbcursor.bsm` (~80) — cursor state machine

Could land as early as the Adventure Puzzle demo prep.

---

## Hot reload watch mode (Dev Loop 4)

`bpp --watch game.bpp`. Detect source change, recompile to a
fresh `/tmp` path, kill old process, restart. Not live code
injection — full restart automation. ~200 lines (shell + compiler
flag + process management). Ships before RPG.

---

## Metaprogramming (Dev Loop 5)

Lands during RTS, not before. Two features:

- **`$T` generic parameters**: `swap($T, a, b) { auto tmp: T;
  ... }`. Compiler instantiates one AST copy per concrete type
  used. ~400 lines.
- **Compile-time struct reflection + code emission**:
  `const generate_serializers(T) { ... }` reads field metadata
  at compile time, emits `save_unit`, `save_building`, etc.
  Seed exists in `const FUNC`; the leap is exposing struct field
  metadata and `emit_function` that injects into the AST.
  ~1100-2000 lines.

Also the foundation for bangscript's `scene { }` block
rewriting.

---

## Deferred — tracked, revisit when demand arises

- **C emitter SIMD mapping (`: double` + `vec_*`)** — native
  backends (NEON, SSE2) emit `vec_*` as instructions. The C
  transpile path needs `: double` locals typed as `__m128` and
  the eleven builtins mapped to `_mm_*` intrinsics. Today the
  emitter uses `long long` for every local unconditionally —
  invasive change. Zero demand from `bpp --c` today.
- **`restrict` keyword** — useful only when the native codegen
  does aliasing-aware optimization. Not part of Mini Cooper.
  Revisit post-1.0.
- **`offsetof` / field reflection** — P1 for bangscript runtime.
  Exposes compile-time field offsets as runtime constants;
  enables auto-generated serializers. Separate task.
- **Prefetch builtins (`__builtin_prefetch`)** — add when real
  profiling on RTS-sized workloads shows the win.
- **Full aliasing-aware optimizer** — post-1.0. Not in scope.

---

## Game modules (pure B++, no platform deps)

| Module | Status | Notes |
|---|---|---|
| `stbtile.bsm` | ✅ | Tilemap engine |
| `stbphys.bsm` | ✅ | Physics with milli-units + FPSBody chapter |
| `stbpath.bsm` | ✅ | A* pathfinding |
| `stbtexture.bsm` | ✅ | Procedural textures (brick / stone / wood / solid) |
| `stbraycast.bsm` | ✅ | DDA + RayHit + per-column projection |
| `stbprofile.bsm` | ✅ | Runtime profiler HUD + `@profile` zone aggregates |
| `stbscene.bsm` | ✅ | Layered backgrounds with parallax (Phase 5) |
| `stbshader.bsm` | ✅ | Custom pipelines + `gpu_target_*` (Phase 4.1.x) |
| `stbfx.bsm` | ✅ | Effect chain + 4 typed effects (Phase 6.1+6.2) |
| `bpp_hash.bsm` | ✅ | Hash maps (used by compiler) |
| `stbaudio.bsm` | ✅ | CoreAudio + SPSC ring |
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
  float arguments on x86_64. Doesn't affect native function
  calls, only the indirect `call()` builtin.
- **String dedup across modules** — duplicate strings across
  modules create duplicate entries in the data segment. Harmless
  but wasteful.
- **`bootstrap.c` is ARM64-only** — see refinements above.
- **`@profile` nested zones aggregate FLAT** — parent total_us
  includes children's time. Hierarchical breakdown deferred to
  v3 when a real consumer needs it. (v1 leak on early-return /
  panic CLOSED 2026-05-08 — scoped-cleanup epilogues shipped.)

---

## Post-1.0 (scaling from demo to real game)

The 1.0 demos prove B++ delivers each game category. Scaling
from "1 demo level" to "full shipping game" requires the
infrastructure below. Each item is a 2-6 week project that adds
capability without changing the language.

- **Parallel codegen per module** — use the maestro job pool to
  make codegen multi-threaded. Becomes worthwhile at 50K+ lines.
- **Asset streaming (`stbasset.bsm`)** — load/unload on demand,
  atlas packing, manifest, background loading thread. ~500-1000
  lines.
- **Memory budget system** — per-system allocation limits,
  warnings on overshoot, defrag for long sessions.
- **True live code reload** — preserve world state across
  recompile. Serialize → relink changed module → deserialize.
  Basically a second linker.
- **Multi-error recovery across modules** — current multi-error
  log is per-compilation. In a 200-module codebase, cross-module
  type errors need smarter recovery.
- **Profiler GUI (timeline view)** — Chrome-style timeline,
  per-frame breakdown, memory + GPU profilers. Likely a
  standalone `stbtool` that reads a binary trace file.
- **Networking (`stbnet.bsm`)** — TCP/UDP, state serialization,
  prediction/rollback. ~500-800 lines.
- **Target architecture refactor** — chip vs OS split. Trigger:
  first chip+OS combo that doesn't fit today's folders
  (Linux ARM, Intel macOS, Windows x86_64).
- **Vulkan GPU on Linux** — see "Linux Vulkan parity" sidequest
  above. Same scope.
- **Tools & editors** — sprite editor (ModuLab ✅), tilemap
  editor, level designer (level_editor ✅), particle editor.
  Thin shells over stb modules.
- **Audio tools (CLAP plugin support)** — `--plugin` flag,
  `export` keyword, DSP primitives.
- **Windows / WebAssembly** — new codegen backends. Big work.
- **EmacsOS distribution** — custom Linux distro: Emacs (EXWM)
  as DE, B++ games in X11 windows. Vision project.
