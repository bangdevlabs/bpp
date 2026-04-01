# B++ Evolution Roadmap — Game Engine + Art Tools + Audio Plugins

## Status: 2026-03-31

**GPU rendering working on macOS ARM64 via Metal.** Lines, circles, rects, outlines — all platform-agnostic in stbrender.bsm. Trig in stbmath.bsm. Backend only needs `_stb_gpu_vertex` + `_stb_gpu_rect`.

**Cache system fixed.** 4 bugs resolved: compiler_self_hash read full binary, module 0 isolation, mod_idx attribution, lex_module boundary. Modular compilation works without manual cache clearing.

**Next: ELF dynamic linking → Linux X11 + Vulkan GPU.** The x86_64 ELF writer needs PLT/GOT for shared library calls. This unblocks Vulkan on Linux and any future FFI. Then sprites/textures on both backends simultaneously.

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself. GPU access is native per platform (Metal/Vulkan/DX12), no third-party APIs.

```

ART (pixel art, animation)        ← creates visual assets
  stbart.bsm + sprite editor
  tilemap editor, particle editor

SOUND (music, SFX)                ← creates audio assets
  stbdsp.bsm + stbplugin.bsm
  CLAP plugins (native + FFI)

GAMES (engine + logic)            ← assembles everything
  stbgame + stbdraw + stbinput
  stbtile + stbphys + stbpath
, eu re
TOOLS (compiler + infra)          ← compiles everything
  bpp (self-hosting, ARM64 + x86_64)
```

**Asset pipeline**: `.bspr` (sprite editor) → `.btm` (tilemap editor) → `.blvl` (level designer) → game

---

## P0 — Immediate Priorities

### 1. ELF Dynamic Linking (BLOCKS Linux GPU) ← NEXT
The x86_64 ELF writer produces fully static binaries. Vulkan (and any FFI on Linux) requires dynamic linking to shared libraries. Must add PLT/GOT infrastructure to `x64_elf.bsm`, mirroring the Mach-O writer's chained fixups.

**What's needed (~500 lines in x64_elf.bsm):**
- PT_INTERP program header (`/lib64/ld-linux-x86-64.so.2`)
- `.dynamic` section (DT_NEEDED, DT_SYMTAB, DT_STRTAB, etc.)
- `.dynsym` + `.dynstr` (dynamic symbol/string tables)
- `.rela.plt` (relocation entries for PLT stubs)
- `.plt` + `.got.plt` (PLT stubs → GOT → lazy resolution by ld.so)
- Relocation type 3 in x64_codegen for extern calls (instead of type 4)
- Wire up `elf_add_got()` in bpp.bpp (mirror macOS lines 396-402)

**Test:** Cross-compile a program that calls `printf` from libc.

### 2. Linux Syscall Builtins (~170 lines in x64_codegen.bsm)
Add missing syscalls for X11/Vulkan:
- `sys_socket(domain, type, protocol)` — syscall 41
- `sys_connect(fd, addr, addrlen)` — syscall 42
- `sys_mmap(addr, len, prot, flags, fd, offset)` — syscall 9
- `sys_munmap(addr, len)` — syscall 11
- `sys_poll(fds, nfds, timeout)` — syscall 7
- `sys_sendmsg(fd, msg, flags)` — syscall 46
- `sys_recvmsg(fd, msg, flags)` — syscall 47

### 3. X11 Window + Vulkan GPU Backend (~900 lines in _stb_platform_linux.bsm)
Replace terminal-only Linux backend with native windowed GPU rendering.

**X11 (via libX11.so FFI):** `XOpenDisplay`, `XCreateSimpleWindow`, `XMapWindow`, event polling, keycode mapping. Uses same FFI mechanism as Vulkan.

**Vulkan (via libvulkan.so FFI):** instance, surface (VK_KHR_xlib_surface), device, swapchain, render pass, pipeline, SPIR-V shaders (embedded byte arrays), vertex batching. Same vertex format as Metal (8 bytes: int16 x,y + uint8 r,g,b,a).

**Test:** Cross-compile test_gpu_shapes.bpp to Linux ELF, run in Docker with lavapipe (software Vulkan) or UTM VM with virtio-gpu.

### 4. GPU Sprites + Textures (Metal + Vulkan together)
Add texture support to both backends simultaneously to avoid refactoring vertex format twice.
- `_stb_gpu_texture_create(w, h, pixels)` — upload RGBA texture
- `_stb_gpu_texture_bind(tex_id)` — bind for drawing
- Vertex format expands: add UV coordinates (int16 u, v → 12 bytes per vertex)
- Update shaders (Metal + SPIR-V) for texture sampling
- `render_sprite(tex_id, x, y, w, h)` in stbrender.bsm (agnostic)

### ~~5. Debug printing builtins~~ — **DONE 2026-03-31**
`print_int` already in stbio.bsm, now auto-imported by stbgame.

### ~~6. Array syntax sugar: buf[i]~~ — **DONE 2026-03-31**
`buf[i]` desugars to `*(buf + i * 8)`. Parser-only. ~230 conversions applied to compiler + stb + examples.

### ~~6b. for loop + else if~~ — **DONE 2026-03-31**
`for (init; cond; step) { body }` desugars to init + while. `else if` chains without extra braces. ~120 for-loop conversions applied.

### 7. Struct field sugar: rec.name
Formalize internal compiler structs (FuncRecord, ExternRecord, Node) so `*(rec + FN_NAME)` becomes `rec.name`. The struct system already supports this for user types — need to define compiler-internal structs and annotate variables. ~20 locations to convert. Eliminates magic offset constants.

### 8. DWARF debug info → lldb/gdb support
Add `__DWARF`/`.debug_line` section to Mach-O/ELF with line tables for debugger stepping.

## P1 — Infrastructure

### bootstrap.c — Must be functional for distribution
bootstrap.c was generated but never tested. It cannot compile current B++ source (lexer/parser outdated). Without a working bootstrap, B++ cannot be distributed via GitHub — the bpp binary is platform-specific and should not be committed.

**Fix:** Use `./bpp --c src/bpp.bpp` to regenerate bootstrap.c, then verify: `cc bootstrap.c -o bpp_seed && ./bpp_seed src/bpp.bpp -o bpp`.

### Multi-return values (Fase 2)
`return a, b;` and `x, y = foo();`. Second return value in x1/d1 (ARM64) or rdx/xmm1 (x86_64). Eliminates float_ret()/float_ret2() hack. Enables Go-style (value, error) returns. ~190 lines. Prerequisite for Fase 3 (unpack types to two clean fields).

---

## Phase 1 — New stb Modules (pure B++, no platform deps)

### 1e: `stb/stbart.bsm` — Pixel Art Primitives (~350 lines)

Ported from ModuLab (JS pixel art editor). The art module holds editing logic so tools stay thin — same pattern as stbdraw for rendering.

```
struct ArtCanvas  { pixels, w, h, pal, pal_count }
struct ArtHistory { snapshots, count, pos, max_snaps }
struct ArtSelection { buf, x, y, w, h, active }
```

API:
- **Canvas**: `art_new`, `art_get`, `art_set`, `art_clear`, `art_resize`
- **Tools**: `art_brush` (diamond radius), `art_erase`, `art_fill` (BFS flood), `art_fill_pattern` (8x8 tiled), `art_fill_dither`, `art_line` (Bresenham), `art_rect`, `art_oval`, `art_eyedrop`
- **Selection**: `art_select`, `art_stamp` (paste at offset), `art_sel_free`
- **Transform**: `art_mirror_h`, `art_mirror_v`, `art_rot90`, `art_rot180`, `art_rot270`
- **History**: `art_history_new`, `art_save` (push snapshot), `art_undo`, `art_redo`
- **Animation**: `art_frame_new`, `art_frame_copy`, `art_onion` (blend prev frame via `blend_px`)
- **I/O**: `art_save_bspr`, `art_load_bspr`, `art_save_bmp` (universal export), `art_export_sheet` (spritesheet)

Design notes:
- Pixels stored as palette indices (1 byte each), -1 = transparent
- Palette is separate array (up to 256 RGBA entries)
- History uses full snapshots (not diffs) — simple, like ModuLab
- BMP export for universal compatibility (54-byte header + raw pixels). No PNG dependency
- `.bspr` for internal pipeline (sprite editor → tilemap editor → game)

### 1a: `stb/stbtile.bsm` — Tilemap (~200 lines)

```
struct Tileset { img, tile_w, tile_h, cols, count }
struct Tilemap { data, w, h, tile_w, tile_h, scroll_x, scroll_y }
```

- `tm_new`, `tm_set`, `tm_get`, `tm_fill`, `ts_new`, `tm_draw`, `tm_scroll`, `tm_collide`
- `tm_px_to_grid`, `tm_grid_to_px`
- Flat byte array, 1 byte per tile, max 255 types

### 1b: `stb/stbphys.bsm` — Physics (~180 lines)

```
struct Body { x, y, vx, vy, ax, ay, w, h, grounded, mass }
```

- Milli-units (×1000) for sub-pixel precision — deterministic across CPUs
- `body_new`, `body_update`, `body_impulse`, `body_vs_rect`, `body_vs_tilemap`
- `body_screen_x`, `body_screen_y` — convert milli→pixel

### 1c: `stb/stbpath.bsm` — A* Pathfinding (~250 lines)

```
struct PathGrid { blocked, w, h, g_cost, parent, open_set, open_len }
```

- `path_grid_new`, `path_set_blocked`, `path_from_tilemap`, `path_find`
- `path_x(packed)`, `path_y(packed)` — unpack coordinates
- Flat arrays for open/closed sets, grid up to ~128x128

### 1d: `stb/stbaudio.bsm` — Game Audio Generation (~300 lines)

```
struct Voice { waveform, freq, volume, phase, attack, decay, sustain, release, env_phase, env_time }
struct Mixer { voices, count, sample_rate, buffer, buf_len }
```

- `mix_new`, `mix_voice`, `mix_adsr`, `mix_note_on/off`, `mix_render`
- Waveforms: square, saw, noise, pulse (chiptune/game SFX focus)

---

## Phase 2 — Binary File Formats

| Format | Magic | Layout | Used by |
|--------|-------|--------|---------|
| `.bspr` (sprite) | `BSP\0` | u16 w + u16 h + u8 pal_count + palette + pixels | sprite editor → tilemap editor → game |
| `.btm` (tilemap) | `BTM\0` | u16 w + u16 h + u8 tw + u8 th + tile indices | tilemap editor → level designer → game |
| `.blvl` (level) | `BLV\0` | u16 obj_count + objects + u16 zone_count + zones | level designer → game |

BMP export for universal image compatibility (no PNG dependency needed).

All use `stbbuf` read/write helpers + `stbfile` read_all/write_all.

---

## Phase 3 — Game & Art Tools (`examples/`)

### 3a: Sprite Editor (`examples/sprite_editor.bpp`, ~300 lines)

Thin UI shell over stbart.bsm. Inspired by ModuLab (our JS pixel art editor).

Deps: stbgame, stbart, stbinput, stbdraw, stbui, stbfile, stbbuf

Layout: palette sidebar (left) + zoomed canvas (center) + timeline (right)

Features:
- All stbart tools: brush, eraser, fill, pattern fill, dither fill, line, rect, oval, eyedropper
- Selection + stamp (copy/paste regions)
- 16-color palette, number key shortcuts, zoom 8-16x
- Frame timeline with thumbnails, add/duplicate/delete frames
- Onion skin (previous frame blended via `blend_px`)
- Color cycling (palette rotation with settable timing)
- Mirror/rotate transforms
- Undo/redo (stbart history)
- Save/load `.bspr` + BMP export
- Hotkeys: B/E/F/L/R/O/I (tools), [ ] (brush size), Ctrl+Z/Y (undo/redo), G (grid)

### 3b: Tilemap Editor (`examples/tilemap_editor.bpp`, ~500 lines)

Deps: stbgame, stbtile, stbart, stbinput, stbdraw, stbui, stbfile, stbbuf

Layout: tile picker (left) + scrollable map canvas (center) + toolbar (top)

Features:
- Click to place, right-click to erase, arrow key scroll
- Tile picker from loaded tileset (`.bspr` sprite sheet from sprite editor)
- 4 layers (bg, mid, fg, collision)
- Save/load `.btm`

### 3c: Level Designer (`examples/level_designer.bpp`, ~600 lines)

Deps: stbgame, stbtile, stbphys, stbart, stbinput, stbdraw, stbui, stbfile, stbbuf

Features:
- Tilemap backdrop (loaded from `.btm` from tilemap editor)
- Drag-to-place entities (spawn points, enemies, items, triggers)
- Zone definition (rectangular regions with type tags)
- Property panel for selected object
- Export `.blvl`

### 3d: Particle Editor (`examples/particle_editor.bpp`, ~350 lines)

Deps: stbgame, stbinput, stbdraw, stbui, stbmath

Features:
- Live emitter preview, parameter adjustment, export config

---

## Phase 4 — Audio Platform Layer

- [ ] `_stb_audio_init(sample_rate, buf_size)` / `_stb_audio_submit(buf, count)`
- [ ] macOS: AudioQueue via FFI
- [ ] SDL: `SDL_OpenAudioDevice` + `SDL_QueueAudio`

## Phase 5 — stbnet (future, needs compiler work)

- [ ] New builtins: `sys_socket`, `sys_bind`, `sys_connect`, `sys_send`, `sys_recv`
- [ ] `stb/stbnet.bsm` — TCP/UDP helpers, pure B++
- [ ] LegoBox-style collaboration server (like ModuLab's team backend)

---

## Phase 6 — Audio Plugins (CLAP native + FFI driver)

B++ compiles directly to `.clap` plugins. Same pattern as games: stb abstracts the platform, dev writes pure B++ logic. Two paths to the same goal — native stb orchestration OR FFI driver (like SDL/raylib for video).

### Architecture (mirrors stbgame)

```
GAMES                              PLUGINS
─────                              ───────
stbgame.bsm    (orchestration)     stbplugin.bsm   (lifecycle)
stbdraw.bsm    (primitives)        stbdsp.bsm      (DSP primitives)
stbinput.bsm   (input)             stbparam.bsm    (parameters)
_stb_platform_macos.bsm            _stb_clap.bsm   (CLAP binding)
drv_sdl.bsm    (alt backend)       drv_clap.bsm    (FFI driver, like SDL)
```

### Two paths (like native Cocoa vs SDL for video)

**Path A — Native stb**: `stbplugin.bsm` + `_stb_clap.bsm` handle everything internally. Pure B++. Requires compiler `--plugin` mode (MH_BUNDLE/ET_DYN, export keyword, PIC).

**Path B — FFI driver**: `drv_clap.bsm` wraps CLAP C API via `import "clap" { ... }`. Same as `drv_sdl.bsm` wraps SDL2. Works with current compiler if host provides the .dylib bridge. Simpler, works sooner.

Both paths use the same `stbdsp.bsm` and `stbparam.bsm`. Dev code is identical — only the import changes.

### Plugin code example

```
import "stbplugin.bsm";
import "stbdsp.bsm";

auto filt: Biquad;

plugin_info() {
    plug_name("BangFilter");
    plug_vendor("BangDev");
    plug_add_param("cutoff", 20.0, 20000.0, 1000.0);
    plug_add_param("resonance", 0.1, 10.0, 0.707);
    return 0;
}

plugin_activate(sample_rate) {
    bq_lowpass(filt, plug_param(0), plug_param(1), sample_rate);
    return 0;
}

plugin_process(input, output, frames) {
    auto i; i = 0;
    while (i < frames) {
        auto s;
        s = dsp_read(input, i);
        s = bq_tick(filt, s);
        dsp_write(output, i, s);
        i = i + 1;
    }
    return 0;
}
```

### Compiler changes needed

- [ ] **A1** `export` keyword — parser flag, propagate to codegen export table
- [ ] **A2** `dsp_read`/`dsp_write` builtins — f32↔f64 conversion (CVTSS2SD / FCVT)
- [ ] **A5** `--plugin` flag — MH_BUNDLE (macOS) / ET_DYN (Linux) output mode
- [ ] **A5b** PIC verification — ARM64 already near-PIC (ADRP+ADD), x86_64 uses RIP-relative
- [ ] **A5c** Export trie expansion — export all `export`-marked functions, not just `_main`

### stb modules

- [ ] **A3** `stb/stbdsp.bsm` — DSP primitives (~400 lines)
  - `struct Biquad { b0, b1, b2, a1, a2, z1, z2 }` — bq_lowpass/highpass/bandpass/notch/peak/shelf, bq_tick
  - `struct Delay { buf, len, pos, feedback }` — delay_new, delay_tick, delay_tap, delay_tap_interp
  - `struct Osc { table, size, phase, freq, sr }` — osc_sine/saw/square/triangle, osc_tick
  - `struct Env { attack, decay, sustain, release, phase, level, rate }` — env_set, env_gate_on/off, env_tick
  - Utilities: db_to_lin, lin_to_db, dsp_mix, dsp_clamp, dsp_lerp, midi_to_freq
  - FFI math: sin, cos, tanh, exp, log, sqrt, pow from libm

- [ ] **A4** `stb/stbparam.bsm` — Parameter system (~100 lines)
  - `struct Param { id, name, min_val, max_val, default_val, value }`
  - plug_add_param, plug_param, plug_param_count, plug_param_name, plug_param_range

- [ ] **A6** `stb/_stb_clap.bsm` — CLAP binding (~150 lines, native path)
  - Generates clap_entry, clap_plugin_factory, clap_plugin structs
  - Maps plugin_info/activate/process to CLAP callbacks via fn_ptr

- [ ] **A6b** `drivers/drv_clap.bsm` — CLAP FFI driver (~150 lines, FFI path)
  - `import "clap" { ... }` wrapping CLAP C API (like drv_sdl wraps SDL2)
  - Same stbplugin.bsm interface, different backend

- [ ] **A7** `stb/stbplugin.bsm` — Plugin orchestration (~100 lines)
  - plug_name, plug_vendor, plug_url
  - Lifecycle: plugin_info, plugin_activate, plugin_deactivate, plugin_process
  - Imports _stb_clap.bsm or drv_clap.bsm based on target/import

### Real-time constraints (B++ natural advantages)

- Zero malloc in audio thread — arena pre-allocated in plugin_activate
- No GC, no exceptions, no vtables — deterministic execution
- Flat arrays + peek/poke — cache-friendly DSP buffers
- Milli-unit physics pattern reusable for fixed-point DSP if needed

### Execution order

| Step | What | Blocks |
|------|------|--------|
| **A3** | stbdsp.bsm (can start NOW, test as standalone) | Plugin audio |
| **A4** | stbparam.bsm | Plugin UI in DAW |
| **A2** | dsp_read/dsp_write builtins (f32↔f64) | Real audio buffers |
| **A1** | `export` keyword | All plugin output |
| **A5** | `--plugin` mode (MH_BUNDLE/ET_DYN + PIC + exports) | Loadable plugins |
| **A6** | _stb_clap.bsm (native) + drv_clap.bsm (FFI) | CLAP format |
| **A7** | stbplugin.bsm orchestration | Dev-facing API |
| **A9** | First plugin: BangFilter.clap | Proof of concept |

### ~~P1: Per-variable type hints~~ — **DONE 2026-03-26**
Parser stores per-variable hints array in T_DECL node field `e` (offset 40). Type inference reads individual hints. `word` removed from keyword list. `byte`/`half`/`quarter` work as standalone declarations.

## Known Bugs

### BUG-1: Multi-statement if-block codegen corruption
Adding multiple function calls inside certain if-block bodies causes the compiled binary to crash (SIGSEGV/SIGILL) or hang. Affects both ARM64 and x86_64 backends. Workaround: use separated if-statements (`if (c) { a(); } if (c) { b(); }` instead of `if (c) { a(); b(); }`). Root cause unknown — likely in how the parser or codegen handles multi-statement if-block bodies at certain nesting depths or function sizes.

### BUG-2: macOS code signing cache (SIGKILL 137)
macOS kernel caches code signing decisions per filesystem path. Replacing a binary at the same path (e.g., compiling bpp2 over an old bpp2) may cause SIGKILL on the new binary. Workaround: copy to a fresh `/tmp/` path before executing. Not a compiler bug — macOS kernel behavior.

### BUG-3: x86_64 self-compile crash with new encoder functions
Adding new functions to x64_enc.bsm causes the cross-compiled x86_64 binary to crash during self-compilation (SIGFAULT/SIGILL). Simple programs cross-compile and run correctly. Likely related to BUG-1 (multi-statement if-block) in the x64 codegen when the binary grows beyond a certain size. x86_64 FLOAT_H codegen changes deferred until BUG-1 is fixed.

### BUG-4: bootstrap.c is ARM64-only
`bootstrap.c` uses ARM64 inline assembly (x0, x16, svc). Cannot compile on x86_64. The C emitter generates portable code (libc wrappers), but `bootstrap.c` was generated from an older version. Needs regeneration. C emitter now has `emit_c_str()` and `sys/stat.h` include (fixed 2026-03-31c).

### P2: String dedup across modules
Duplicate strings in modular compilation create duplicate entries in data segment. Add dedup in the writer (hash string content).

### P2: `call()` builtin float args
`call(fp, args...)` ignores float arguments entirely on x86_64. Needs type-aware argument passing.

### Future: Context-Based Specialization (PAUSED)
Automatic function specialization per call-site type signature. `add$FF` (float) vs `add$LL` (int). Resume when real code needs `add(1.5, 2.5) = 4.0`.

---

## Global Execution Order

| Step | What | Size | Blocks |
|------|------|------|--------||
| **1e** | **stbart** (pixel art primitives) | ~350 lines | Sprite editor, all art tools |
| **1a** | stbtile | ~200 lines | Tilemap editor, Level designer |
| **1b** | stbphys | ~180 lines | Level designer |
| **3a** | **Sprite editor** (thin shell over stbart) | ~300 lines | Tileset creation for pipeline |
| **1c** | stbpath | ~250 lines | Enemy AI in games |
| **3b** | Tilemap editor | ~500 lines | Level designer |
| **3d** | Particle editor | ~350 lines | Visual effects |
| **1d** | stbaudio (game audio) | ~300 lines | Sound in games |
| **3c** | Level designer | ~600 lines | Full pipeline |
| **4** | Audio platform layer | ~200 lines | Game audio playback |
| **A3** | stbdsp (DSP primitives) | ~400 lines | Audio plugins |
| **A1-A5** | Compiler: export, --plugin, PIC | ~300 lines | Plugin output |
| **A6-A7** | CLAP binding + stbplugin | ~400 lines | CLAP format |
| **5** | stbnet | Large | Multiplayer + collaboration |

**Minimal viable demo**: Phase 0 + stbart + Sprite Editor = ~770 lines.

## Verification Protocol

After each phase:
1. `bpp <file> -o /tmp/test && /tmp/test`
2. Bootstrap: `bpp src/bpp.bpp -o /tmp/bpp && /tmp/bpp src/bpp.bpp -o /tmp/bpp2`
3. Phase 0: `test_mouse.bpp` — visual mouse tracking
4. Phase 1: small test programs per module
5. Phase 3: each tool opens, accepts input, saves/loads correctly
6. Phase 6: plugin loads in Bitwig/Reaper, processes audio
