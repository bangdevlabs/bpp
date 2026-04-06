# B++ Roadmap

## Status: 2026-04-03

GPU text rendering working (bitmap 8x8 + TrueType with antialiasing). Pure B++ PNG loader (stbimage.bsm). Pure B++ TrueType reader (stbfont.bsm). realloc with header (Bell Labs style) — malloc stores size, free does real munmap, realloc copies automatically. Module reorganization: stbcolor (palette), stbfont (font data + TTF), stbrender (GPU), stbdraw (CPU) — fully independent. 18 stb modules. Go-style modular cache. Native debugger (bug v2). Two backends (ARM64 + x86_64).

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself.

---

## Done (this cycle)

- **GPU text**: render_text + render_number on Metal, 16-byte vertex with UV+texture, glyph atlas
- **TrueType**: pure B++ reader (~500 lines), cmap, glyf, rasterizer with AA, atlas builder, font_load()
- **stbimage.bsm**: pure B++ PNG loader (876 lines), DEFLATE fixed
- **realloc**: header-based (3 backends), free does real munmap, 2-arg API
- **stbcolor.bsm**: color palette as independent module
- **Module reorg**: stbrender independent of stbdraw

---

## P0 — Next Up

### Retina support
contentsScale = 2.0 on CAMetalLayer. Render at 2x physical pixels. Needs drawable size query for shader uniforms.

### GPU sprites + textures
Texture upload infra done (glyph atlas uses it). Needs: `render_sprite(tex, x, y, w, h)` with arbitrary textures. Load PNG via stbimage.bsm → upload as MTLTexture → render with UV quads.

### Snake GPU migration
Replace draw_* → render_* in examples/snake_full.bpp. ~15 lines. Blocked on nothing — all infra ready.

### Platformer test game
`games/platformer/game.bpp` exists with placeholder graphics. Needs: real Kenney assets via stbimage, enemies, scrolling camera. Stress-tests all stb modules.

### FFI auto-marshalling
The compiler knows FFI param types but ignores them in codegen. Helpers ready in defs.bsm. Needs codegen changes in all 3 backends.

### ELF dynamic linking (blocks Linux GPU)
x86_64 ELF writer produces static binaries only. Vulkan/X11 needs PLT/GOT for shared libraries.

---

## P1 — Compiler Quality

### Struct field sugar
`*(rec + FN_NAME)` → `rec.name` for compiler-internal structs. ~20 locations.

### Multi-return values
`return a, b;` and `x, y = foo();`. Eliminates float_ret/float_ret2 hack. ~190 lines.

### bootstrap.c
Needs regeneration with current C emitter. Required for GitHub distribution.

### Module system (future)
Real module identity, versioning, namespaces, package manager. Needed when B++ has community packages. Design doc in memory/project_module_system.md.

---

## P2 — Game Modules (pure B++, no platform deps)

| Module | Lines | Purpose |
|--------|-------|---------|
| stbart.bsm | ~350 | Pixel art primitives (brush, fill, line, history, animation) |
| stbtile.bsm | ~200 | Tilemap engine (load, scroll, collide) |
| stbphys.bsm | ~180 | Physics with milli-units (gravity, impulse, collision) |
| stbpath.bsm | ~250 | A* pathfinding |
| stbaudio.bsm | ~300 | Game audio generation (waveforms, ADSR, mixer) |

---

## P3 — Tools & Editors

- Sprite editor (~300 lines, thin shell over stbart)
- Tilemap editor (~500 lines)
- Level designer (~600 lines)
- Particle editor (~350 lines)

---

## P4 — Linux Native (X11 + Vulkan)

1. Syscalls: sys_mmap, sys_munmap, sys_poll (sys_socket + sys_connect done)
2. X11 window via libX11.so FFI
3. Vulkan GPU backend via libvulkan.so FFI
4. Test: cross-compile GPU game, run in Docker/VM

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

### ~~BUG-5: macOS W^X blocks debugger breakpoints~~ RESOLVED
Solved by delegating to Apple's debugserver via GDB remote protocol. No more direct ptrace/Mach API usage.

### P2-level: String dedup across modules
Duplicate strings in modular compilation create duplicate entries in data segment.

### P2-level: `call()` float args on x86_64
`call(fp, args...)` ignores float arguments on x86_64.

---

## Done (historical)

- Self-hosting compiler (2026-03-20)
- Native Mach-O ARM64 binary (2026-03-23)
- stb game engine: 14 modules (draw, input, game, col, math, str, array, buf, file, font, io, ui, platform×2)
- x86_64 Linux cross-compilation (2026-03-25)
- Type hints: byte/quarter/half/int/float (2026-03-26)
- Modular compilation with .bo caching (2026-03-26)
- Base×Slice type system: 5 bases × 4 slices (2026-03-27)
- Optimized encoding: bit 0 = float flag (2026-03-31)
- FLOAT_H/Q codegen ARM64 (2026-03-31)
- GPU rendering via Metal (2026-03-31)
- `buf[i]` array syntax sugar (2026-03-31)
- `for` loop + `else if` syntax sugar (2026-03-31)
- Cache fix: explicit imports, Go-style hash cascading (2026-04-01)
- Cache isolation: per-program main_hash mixing (2026-04-01)
- sys_fork ARM64 fix: enc_new_label (2026-04-01)
- sys_ptrace + sys_wait4 + sys_getdents + sys_unlink builtins (2026-04-01)
- Bug debugger: --bug flag, .bug format, dump mode, run mode (2026-04-01)
- Validate + typeck merged (2026-04-01)
- `--clean-cache` command (2026-04-01)
- Per-variable type hints (2026-03-26)
- Compiler diagnostics: E001-E201, W001-W011, Clang-style source+caret (2026-03-24+)
- putchar_err builtin (2026-03-31)
- Packed struct fields with sub-word load/store (2026-03-27)
- stbarena: generic bump allocator, 8-byte aligned, overflow guard (2026-04-02)
- stbpool: fixed-size object pool, O(1) freelist get/put (2026-04-02)
- stbecs: entity-component system, milli-unit physics, ID recycling (2026-04-02)
- sys_lseek + sys_fchmod builtins, both backends + C emitter (2026-04-02)
- sys_unlink + sys_getdents added to ARM64 (were x86_64-only) (2026-04-02)
- Bug debugger: entitlements fix, ASLR slide fix, lookup_pc fix (2026-04-02)
- Bug debugger v2: debugserver/gdbserver backend, GDB remote protocol, zero-flag debugging (2026-04-02)
- sys_socket, sys_connect, sys_usleep builtins — all 3 backends (2026-04-02)
- Mach-O symbol table: all functions exported (enables debugger without --bug) (2026-04-02)
- Crash backtrace + crash locals + call depth indentation in bug (2026-04-02)
- const keyword: compile-time values + functions + DCE (2026-04-02)
- arr_truncate in stbarray (2026-04-02)
- Block-scoped auto confirmed working (false myth retracted) (2026-04-02)
- snake_full.bpp: snake with const + ECS + arena + pool (2026-04-02)
