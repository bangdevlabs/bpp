# B++ Roadmap

## Status: 2026-04-06 — version 0.21

B++ 0.21 milestone. Compiler: address-of `&`, validator integrated into incremental pipeline, **stbhash-backed symbol tables (6 linear scans → O(1) lookups)**, C emitter modernized (8 builtins added, extern dedup, libc skip), **auto-injection of platform layer when stbgame is in the import graph**. Linux X11 wire protocol Phases 1-3 (window + software rendering + input). Game engine: **stbpath (A* with decrease-key heap)**, **stbhash (word + byte-keyed maps)**, stbtile, stbphys — **26 stb modules**. Three games: snake (Linux-native via X11), pathfinder (refactored to milli-unit + tilemap + A* AI), platformer. **Test suite cleaned: 52/52 passing.**

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself. **Version 1.0 ships when a complete indie retro game is shipped in pure B++.**

---

## Done (this cycle)

### Compiler internals
- **`sys_socket`/`sys_connect`/`sys_usleep` added to validator builtin list** (`bpp_validate.bsm`) — validator was rejecting them as undefined; bug only surfaced when cache was clean (cached `.bo` masked it). The fix is 4 lines but the discovery story is documented in the journal.
- **stbhash-backed symbol tables**: 6 linear-scan lookups in the compiler replaced with O(1) hash queries — `val_find_func`/`val_find_extern` (eager rebuild, validate.bsm), `find_func_idx` (eager rebuild, types.bsm), `find_struct` (incremental update, parser.bsm + bo.bsm), `find_extern`/`is_extern` (lazy rebuild, emitter.bsm). Strategy chosen per call site based on insertion-vs-lookup pattern.
- **`_stb_platform.bsm` auto-injection**: compiler detects stbgame in the import graph and pulls the per-target platform layer (`process_file` in `bpp_import.bsm`). The explicit `import "_stb_platform.bsm"` line was removed from `stbgame.bsm` — no library code mentions the platform indirection anymore.
- **Platform/observe files moved into chip folders**: `_stb_platform_macos.bsm` and `bug_observe_macos.bsm` now live in `src/aarch64/`; `_stb_platform_linux.bsm` and `bug_observe_linux.bsm` in `src/x86_64/`. `find_file()` extended with `src/aarch64/`, `src/x86_64/`, `/usr/local/lib/bpp/aarch64/`, `/usr/local/lib/bpp/x86_64/` search paths. Each chip folder has a `README.md` documenting the chip+OS coupling and the future arch/os/targets split.
- **`install.sh` updated** to copy from the new chip folders into per-target install directories.

### New stb modules
- **stbpath.bsm**: pure-B++ A* pathfinding. Binary min-heap with **indexed positions** (`heap_pos[]`) for true O(log n) decrease-key — no lazy deletion, no stale pops. Manhattan heuristic, 4-connected grid, zero dependencies (intentionally not coupled to stbtile — the bridge is documented as an inline 8-line snippet).
- **stbhash.bsm**: pure-B++ hash map. Two flavors in one file: word-keyed `Hash` (Knuth multiplicative) and byte-keyed `HashStr` (djb2). Open addressing, linear probing, tombstone-aware delete, 75%-load resize. Used by the compiler symbol-table refactor and ready for any program that needs a dictionary.

### Game refactors
- **pathfind.bpp rewritten**: positions now in **milli-pixel int** (no more `float` for game state), tilemap-based arena with walls, **cat AI uses A\* via stbpath** to navigate around obstacles toward the rat, ECS particle damage, separate-axis tile collision for the rat. Removed dead `* 1.0` round-trips and the leftover `rat row0:` debug print.

### Test suite cleanup
- **71 tests → 52 tests, all passing.** 19 stale tests deleted (legacy `print_str`/`pstr`/`io_print_str` API, `byte lives;` pre-base×slice syntax, `init_defs()` removed function, hardcoded `/Users/obino/.bpp/cache` path, scratch debug imports of internal compiler modules, `_stb_present` redefinition no longer compatible with auto-imported platform layer). 1 new consolidated `test_io.bpp` exercises the current stbio API. Test files kept now reflect a real maintained surface, not historical sediment.
- **`tests/test_hash.bpp`**: 49 assertions covering both Hash and HashStr (insert, lookup, update, delete, resize stress, clear, length disambiguation).
- **`tests/test_path.bpp`**: 7 A* scenarios (open grid straight line, trivial start==goal, U-shaped wall detour, unreachable, blocked endpoints, OOB, scratch reuse).

### Earlier in the cycle (X11 + validator + C emitter)
- **stbtile.bsm**: tilemap engine — tile_new, tile_get/set, tile_solid, tile_collides, tile_load_set (PNG tileset), tile_map_type (remap), tile_draw (GPU + culling)
- **stbphys.bsm**: platformer physics — Body struct, milli-pixel convention, gravity/jump/collision, ground probe
- **Address-of `&`**: compiler feature — T_ADDR node, x64+aarch64 codegen, LEA/ADD for locals, RIP-relative for globals
- **stbimage tRNS**: palette-indexed PNG transparency via tRNS chunk
- **Alpha discard**: Metal fragment shader `discard_fragment()` for transparent sprites
- **Platformer GPU**: games/platformer/ with Kenney assets, stbtile+stbphys, GPU rendering
- **Physics bugs fixed**: snap formula (bottom of body, not top), ground probe for reliable on_ground
- **X11 wire protocol Phases 1-3**: `_stb_platform_linux.bsm` extended (276→1160 lines), Unix socket + TCP, handshake, CreateWindow, XPutImage, keyboard+mouse events, evdev/Apple keycode auto-detect, FocusOut stuck-key cleanup, WM_DELETE close, WM_NAME title. Snake runs on Linux X11 in Docker via XQuartz
- **Validator integrated into incremental pipeline**: `run_validate()` now runs in ARM64/x86_64 builds, not just `--c` and `--asm` (was a 4-month bug). E052 (float→int reject) removed because it rejected legitimate explicit `: int` hints
- **`bpp_typeck.bsm` deleted**: 507-line orphan from a never-finished plan
- **C emitter modernized**: 8 missing builtins added (`putchar_err`, `assert`, `shr`, `float_ret`, `float_ret2`, `sys_ioctl`, `sys_nanosleep`, `sys_clock_gettime`), extern dedup with varargs for collisions (objc_msgSend), `is_libc_symbol()` skip to avoid header conflicts (`usleep`, `sleep`, etc.)
- **Snake on Linux**: `bpp --linux64 examples/snake_cpu.bpp` cross-compiles to ELF, runs in Ubuntu Docker container with `host.docker.internal:0` DISPLAY pointing at host XQuartz
- **C emitter raylib path**: `bpp --c examples/snake_raylib.bpp > game.c; gcc game.c -lraylib -lobjc -o game` produces a Mach-O 94KB binary that runs the game via raylib's OpenGL — portable C output for any platform with raylib

---

## P0 — Next Up (Priority Order)

### stbaudio.bsm + rhythm_teacher demo port
Build the audio module from scratch and port the existing C `rhythm_teacher`
demo to pure B++ as the validation target. stbaudio needs:
- Platform layer audio backends: CoreAudio (macOS) and ALSA (Linux)
- DSP primitives: oscillator (sine/square/saw/triangle), ADSR envelope,
  biquad filter, simple delay/reverb, mixer with channel routing
- Sample buffer management (mono/stereo, int16/float)
- MIDI-style note triggering API (`audio_note_on(freq, velocity)`)
- A simple `audio_init(sample_rate, channels)` / `audio_frame()` loop
- Decision: pure-B++ DSP only, no FFI to system DSP libs

The rhythm_teacher port is the acceptance test: if we can port a real
interactive audio demo from C, the API is good enough. Estimated scope:
~600 lines stbaudio + ~400 lines for the port + per-OS platform
backends.

### ELF Dynamic Linking (Linux GPU + libpng/sqlite/curl path)
B++'s x86_64 backend produces only static ELF binaries. To use any
shared library on Linux (raylib, libpng, libsqlite, **libvulkan**), the
ELF writer needs PLT/GOT, LC_LOAD_DYLIB analog, dlopen/dlsym builtins.
This unlocks Vulkan GPU on Linux (Phase 4 of the X11 plan), but is
valuable on its own — any future stb FFI module on Linux needs it.
Also unlocks ALSA dynamic linking for stbaudio Linux backend.
Estimate: 2-3 days. See P4 for the full Vulkan rollout that follows.

### Test all games via the C emitter (raylib path)
Only `snake_gpu.bpp` and `snake_raylib.bpp` have been verified end-to-end
through `bpp --c → gcc → run`. The other games (`pathfind.bpp`,
`platformer/platformer.bpp`, `snake_cpu.bpp` with X11) need:
- Cross-compile via `--c` and via `--linux64`
- gcc compile + link with the right libs (`-lraylib -lobjc` on macOS,
  `-lraylib -lm` on Linux)
- Smoke test (window opens, input responds, no crashes)
- Document any games that need source tweaks to work via the C path

### Host-aware compiler + install
Today the compiler defaults to macOS arm64 and `install.sh` only sets
up that target — `--linux64` is the cross-compile flag. The vision:
the compiler detects the host OS and chip at startup and defaults to
that target, so `bpp game.bpp` on Linux x86_64 produces a Linux ELF
without any flag. `install.sh` should mirror this: detect host, run
the right bootstrap path, install the right binary layout.

Cross-compilation stays available via explicit `--target` flag, but
becomes the exception, not the rule. This transforms B++ from "macOS
tool that cross-compiles to Linux" into "native tool that adapts to
the host." Requires:
- Compile-time or runtime host detection in `init_target()`
- `install.sh` host-detection branch (uname -s + uname -m)
- A `--target=<chip>_<os>` flag to override the default
- `--linux64` becomes a backward-compat alias

### Retina support
contentsScale = 2.0 on CAMetalLayer. Render at 2x physical pixels.

### FFI auto-marshalling
The compiler knows FFI param types but ignores them in codegen. Needs codegen changes.

---

## P1 — Compiler Quality

### Struct field sugar
`*(rec + FN_NAME)` → `rec.name` for compiler-internal structs. ~20 locations.

### Multi-return values
`return a, b;` and `x, y = foo();`. Eliminates float_ret/float_ret2 hack.

### bootstrap.c
Needs regeneration with current C emitter. Required for GitHub distribution.

---

## P2 — Game Modules (pure B++, no platform deps)

| Module | Status | Purpose |
|--------|--------|---------|
| stbtile.bsm | **DONE** | Tilemap engine (load, scroll, collide, remap) |
| stbphys.bsm | **DONE** | Physics with milli-units (gravity, impulse, collision) |
| stbpath.bsm | **DONE** | A* pathfinding (binary heap, decrease-key, Manhattan, leaf module) |
| stbhash.bsm | **DONE** | Hash maps (word + byte keys, open addressing, used by compiler) |
| stbpath_sparse.bsm | PENDING | A* variant with stbhash-backed cells for huge grids (>1500×1500) — only build when an actual game demands it |
| stbart.bsm | PENDING | Pixel art primitives (brush, fill, line, history, animation) |
| stbaudio.bsm | PENDING (P0) | Game audio generation (waveforms, ADSR, mixer) — see P0 for the rhythm_teacher port acceptance test |
| stbnet.bsm | PENDING | TCP/UDP socket abstraction for multiplayer |

---

## P3 — Target architecture refactor (chip vs OS split)

Today the chip folders (`src/aarch64/`, `src/x86_64/`) actually hold a
**chip+OS bundle** — encoder, binary writer, syscalls, platform layer,
and bug observer all live together because B++ supports exactly one
OS per chip right now (aarch64 → macOS, x86_64 → Linux). When the
second OS for an existing chip lands (Linux ARM, Intel macOS,
Windows x86_64), this layout has to split:

```
src/
├── arch/
│   ├── aarch64/   encoder.bsm, registers.bsm        (chip pure)
│   └── x86_64/    encoder.bsm, registers.bsm        (chip pure)
├── os/
│   ├── macos/     machof.bsm, syscalls.bsm, codesign.bsm,
│   │              _stb_platform.bsm, bug_observe.bsm
│   ├── linux/     elf.bsm, syscalls.bsm,
│   │              _stb_platform.bsm, bug_observe.bsm
│   └── windows/   pe.bsm, syscalls.bsm, ...
├── targets/
│   ├── arm64_macos.bsm    (~30 lines, glues arch + os)
│   ├── x64_linux.bsm
│   ├── arm64_linux.bsm
│   └── x64_windows.bsm
```

The trigger for doing this work is "when we need a new chip+OS combo
that doesn't fit the existing folders." Until then, the chip folders
double as target bundles and the README in each folder explains the
coupling.

---

## P3.5 — Tools & Editors

- Sprite editor (~300 lines, thin shell over stbart)
- Tilemap editor (~500 lines)
- Level designer (~600 lines)
- Particle editor (~350 lines)

---

## P4 — Linux GPU (Vulkan)

Depends on **ELF Dynamic Linking** (now in P0). Once dlopen works:

1. `libvulkan.so` loaded at runtime via dlopen
2. Function pointers resolved via dlsym (vkCreateInstance etc.)
3. `VK_KHR_xlib_surface` to bridge Vulkan with the X11 wire protocol
   already implemented
4. Minimal pipeline: instance, physical device, swapchain, vertex/fragment
   shaders (SPIR-V embedded as data)

**Testing reality**: Docker on macOS has no GPU passthrough. Real GPU
testing requires (a) actual Linux hardware, (b) a Linux VM with PCI
passthrough, or (c) software fallback via LavaPipe (Mesa). LavaPipe
works in Docker but is CPU rendering, defeating the purpose. The
Vulkan path is "code-complete and ready" until real hardware is
available.

---

## P5 — Audio

- Audio platform layer (CoreAudio macOS, ALSA Linux)
- stbdsp.bsm (DSP primitives: biquad, delay, oscillator, envelope)
- CLAP plugin support (`--plugin` flag, export keyword)

---

## P6 — Distribution (EmacsOS)

- Custom Linux distro: Emacs (EXWM) as DE, B++ games in X11 windows
- stbnet.bsm (TCP/UDP, multiplayer)
- Package manager

---

## Known Bugs

### BUG-2: macOS code signing cache (SIGKILL 137)
Replacing a binary at the same path may cause SIGKILL. Workaround: use fresh `/tmp/` paths.

### BUG-4: bootstrap.c is ARM64-only
Needs regeneration with current C emitter.

### P2-level: String dedup across modules
Duplicate strings in modular compilation create duplicate entries in data segment.

### P2-level: `call()` float args on x86_64
`call(fp, args...)` ignores float arguments on x86_64.

---

## Done (historical)

- Self-hosting compiler (2026-03-20)
- Native Mach-O ARM64 binary (2026-03-23)
- stb game engine: 24 modules
- x86_64 Linux cross-compilation (2026-03-25)
- Type hints: byte/quarter/half/int/float (2026-03-26)
- Modular compilation with .bo caching (2026-03-26)
- Base×Slice type system: 5 bases × 4 slices (2026-03-27)
- Optimized encoding: bit 0 = float flag (2026-03-31)
- FLOAT_H/Q codegen ARM64 (2026-03-31)
- GPU rendering via Metal (2026-03-31)
- `buf[i]` array syntax + `for` loop + `else if` syntax sugar (2026-03-31)
- Cache: explicit imports, Go-style hash cascading, per-program mixing (2026-04-01)
- Bug debugger v2: debugserver backend, GDB remote protocol (2026-04-02)
- const keyword, ECS, arena, pool, collision (2026-04-02)
- GPU sprites: any size, UV fix, sprite_create/sprite_draw (2026-04-05)
- stbsprite.bsm: palette-indexed sprite loading from JSON (2026-04-05)
- Module cache fix: 3 critical bugs (param types, return types, null terminators) (2026-04-05)
- Pathfinder game: rat-and-cat chase, ECS particles, GPU sprites (2026-04-05)
- stbtile.bsm + stbphys.bsm: tilemap engine + platformer physics (2026-04-06)
- Address-of `&` operator: compiler feature, 9 files, bootstrap verified (2026-04-06)
- stbimage tRNS: palette PNG transparency (2026-04-06)
- Alpha discard: Metal fragment shader discard_fragment() (2026-04-06)
- Platformer with Kenney assets: real PNG tileset, character sprites (2026-04-06)
- stbpath.bsm: A* with binary heap + indexed decrease-key, leaf module (2026-04-06)
- stbhash.bsm: word + byte-keyed hash maps, open addressing, used by compiler (2026-04-06)
- pathfind.bpp refactor: milli-unit positions, tilemap walls, A* AI for cat (2026-04-06)
- Compiler symbol-table refactor: 6 linear scans → O(1) hash, eager+lazy+incremental strategies (2026-04-06)
- Platform/observe files moved into chip folders + auto-import via compiler (2026-04-06)
- Test suite cleanup: 71 → 52 tests, 100% passing, legacy API drift removed (2026-04-06)
