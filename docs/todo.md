# B++ Evolution Roadmap — Game Engine + Art Tools + Audio Plugins

## Status: 2026-03-27

Type system refactored from flat enum to orthogonal base × slice grid (5 bases × 5 slices = 25 types, packed in 1 byte). Compiler self-hosts with Go-model modular compilation. float_ret()/float_ret2() builtins. Diagnostics (E001-E201, W001-W005). Packed structs. memcpy/realloc builtins. `install.sh` for global installation. Parser supports compound type annotations: `half float` → 32-bit float, `quarter float` → 16-bit float.

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself.

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

TOOLS (compiler + infra)          ← compiles everything
  bpp (self-hosting, ARM64 + x86_64)
```

**Asset pipeline**: `.bspr` (sprite editor) → `.btm` (tilemap editor) → `.blvl` (level designer) → game

---

## P0 — Developer Experience (immediate priorities)

### 1. Native FLOAT_H / FLOAT_Q codegen (Fase 1B)
Type system supports TY_FLOAT_H (32-bit float) and TY_FLOAT_Q (16-bit float) but codegen still uses 64-bit float instructions. Need:
- ARM64: s-register load/store/arithmetic (enc_ldr_s_uoff, enc_fadd_s, etc.)
- x86_64: SS instructions (MOVSS, ADDSS, SUBSS, etc.)
- Codegen dispatch by ty_slice() within the float path
- emit_node return value extended: 0=int, 1=FLOAT, 2=FLOAT_H
~150 lines across encoder + codegen files.

### 2. Debug printing builtins
`print_int(x)` and `print_float(x)` that write to stderr/terminal. Every debugging session needs this. Also `print_str(s)` without manual str_peek loops. ~20 lines each in stbio.bsm, no compiler changes needed.

### 3. Array syntax sugar: buf[i]
`buf[i]` as sugar for `*(buf + i * 8)` and `buf[i] = val` for `*(buf + i * 8) = val`. Parser-only change — desugars to existing T_MEMLD/T_MEMST. Massive readability improvement for game code.

### 4. Compiler warnings (analysis pass)
Add warnings for common mistakes without adding new types:
- W006: variable used before assignment
- W007: function called with wrong argument count (already E-level, add W for close matches)
- W008: comparison of pointer with integer literal other than 0
- W009: unreachable code after return/break

### 5. DWARF debug info → lldb support
Add `__DWARF` section to Mach-O/ELF with line tables so lldb can:
- Show source file:line on breakpoints and crashes
- Step through B++ code line by line
- Print variable values at break

Implementation:
- `--debug` flag enables DWARF `.debug_line` emission
- Map each emitted instruction back to source file:line (already tracked by diag system)
- Write DWARF line number program into `__DWARF,__debug_line` section
- Add `LC_UUID` load command for debugger identification
- `assert(cond)` enhanced to embed file:line in trap metadata

### 6. Sprite/tilemap loader in stb
`load_sprite16(path)` and `load_tilemap(path)` — every game needs this, shouldn't be 50 lines of manual JSON parsing per project. Part of stbart.bsm.

### 7. Sound
Basic audio output for game feedback. Evaluate: CoreAudio (macOS native), ALSA (Linux), or minimal beep/sample playback via platform layer.

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
