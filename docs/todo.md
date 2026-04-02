# B++ Roadmap

## Status: 2026-04-01

GPU rendering working (Metal macOS). Go-style modular cache with per-program isolation. Native debugger (`bug`) integrated. Type system: Base×Slice with FLOAT_H/Q codegen. Two backends (ARM64 + x86_64). `buf[i]` and `for` loop syntax sugar.

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself.

---

## P0 — Next Up

### Bug debugger: breakpoint write (BUG-5)
macOS W^X blocks `mach_vm_write` on code pages. Infrastructure ready (ASLR slide, address map, breakpoint arrays). Two solutions:
1. Compile BRK inline via `--bug` flag (emit BRK at function entry in codegen)
2. Use `posix_spawn` with `_POSIX_SPAWN_DISABLE_ASLR` + Mach exception ports

### FFI auto-marshalling
The compiler knows FFI param types (`int*`, `float`, `double`) but ignores them in codegen. All args passed as 64-bit words. Helpers ready in defs.bsm (`ffi_is_ptr`, `ffi_is_float`, etc.). Needs codegen changes in all 3 backends to route float args to XMM/d-registers and handle int* out-params.

### ELF dynamic linking (blocks Linux GPU)
x86_64 ELF writer produces static binaries only. Vulkan/X11 needs PLT/GOT for shared libraries. ~500 lines in `x64_elf.bsm`.

### GPU sprites + textures
Texture upload + sampling on Metal (and Vulkan when ready). Vertex format needs UV coords. Shader update. `render_sprite(tex, x, y, w, h)` in stbrender.bsm.

### Platformer test game
`games/platformer/game.bpp` exists with placeholder graphics. Needs: stb_image FFI for PNG loading, real Kenney assets, enemies, scrolling camera. Stress-tests all stb modules.

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

1. Syscalls: sys_socket, sys_connect, sys_mmap, sys_munmap, sys_poll
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

### BUG-5: macOS W^X blocks debugger breakpoints
`mach_vm_write` and `ptrace(PT_WRITE_D)` fail on code pages. Bug debugger can observe/crash-report but cannot set software breakpoints yet.

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
