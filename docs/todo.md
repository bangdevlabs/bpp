# B++ Roadmap

## Status: 2026-04-06

B++ 2.0 milestone. Compiler: address-of operator `&`, tRNS PNG transparency, alpha discard shader. Game engine: stbtile (tilemap), stbphys (physics), 24 stb modules total. Platformer running with Kenney Pixel Platformer assets (CC0) on Metal GPU. Three games: snake, pathfinder, platformer.

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself.

---

## Done (this cycle)

- **stbtile.bsm**: tilemap engine — tile_new, tile_get/set, tile_solid, tile_collides, tile_load_set (PNG tileset), tile_map_type (remap), tile_draw (GPU + culling)
- **stbphys.bsm**: platformer physics — Body struct, milli-pixel convention, gravity/jump/collision, ground probe
- **Address-of `&`**: compiler feature — T_ADDR node, x64+aarch64 codegen, LEA/ADD for locals, RIP-relative for globals
- **stbimage tRNS**: palette-indexed PNG transparency via tRNS chunk
- **Alpha discard**: Metal fragment shader `discard_fragment()` for transparent sprites
- **Platformer GPU**: games/platformer/ with Kenney assets, stbtile+stbphys, GPU rendering
- **Physics bugs fixed**: snap formula (bottom of body, not top), ground probe for reliable on_ground

---

## P0 — Next Up (Priority Order)

### X11 Wire Protocol (Linux native windowing) — PRIORITY
Raw X11 protocol over Unix socket. No FFI, no shared libraries.
Phase 1: Connection + window creation (~200 lines)
Phase 2: Software rendering via XPutImage (~80 lines)
Phase 3: Input handling — keyboard + mouse (~120 lines)
See: `docs/x11_plan_review.md` for implementation notes.

### stbpath.bsm — A* pathfinding
Grid-based A*, Manhattan heuristic, path_from_tilemap bridge.
~250 lines. Upgrades pathfinder game AI from pure pursuit to real pathfinding.

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
| stbpath.bsm | PENDING | A* pathfinding |
| stbart.bsm | PENDING | Pixel art primitives (brush, fill, line, history, animation) |
| stbaudio.bsm | PENDING | Game audio generation (waveforms, ADSR, mixer) |

---

## P3 — Tools & Editors

- Sprite editor (~300 lines, thin shell over stbart)
- Tilemap editor (~500 lines)
- Level designer (~600 lines)
- Particle editor (~350 lines)

---

## P4 — Linux GPU (Vulkan)

1. ELF dynamic linking (PLT/GOT for shared libraries)
2. Vulkan GPU backend via libvulkan.so
3. Test: cross-compile GPU game, run in Docker/VM

Note: X11 windowing (P0) uses wire protocol, no ELF linking needed.
Vulkan requires dynamic linking and is a separate, deferred effort.

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
