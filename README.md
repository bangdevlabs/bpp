# B++

### B++ 0.23.x — Language Syntax Complete, Smart Dispatch, Mini Cooper Phase A + B0 — 14 April 2026

Self-hosting compiler with **three playable GPU games** (Snake, Pathfinder, Platformer with Kenney assets), **Snake running on Linux via raw X11 wire protocol** (no FFI, no libX11, no Xlib — just `sys_socket` over a Unix socket), and **parallel runtime** via the maestro pattern (worker pool + SPSC queues + jazz/rock solo-base-render phases). The language shape locked in for 1.0: 22 keywords + 6 annotations (`load` / `static` / `void` / `switch` / `continue` / `extrn` / `global`; `: base` / `: solo` / slice hints). **Smart dispatch engine shipped**: call-graph + fixpoint purity classifier + auto-promote of `auto` to `const` / `extrn` / `global`; only the codegen rewrite remains. **Mini Cooper Phase A + B0**: bitfields (`: bit` through `: bit7`), `malloc_aligned`, parser-level constant folding + DCE, `T_TERNARY` AST node, `&&` / `||` desugared to T_TERNARY so short-circuit is canonical. **Foundation refactor**: `.bo` cache removed (every compile from source at 0.27 s), repo split into `chip` / `os` / `target` / `c` backend layers, `bsys` syscall tables, `bmem` pure-B++ allocator, `brt0` runtime startup in B++ source. Suite at **59 passed / 0 failed / 11 skipped** with eight new canonical feature tests.

> **Version 0.23.x**: B++ ships **1.0** when a complete indie retro game is shipped, end to end, in pure B++. Until then we count fractional progress.

---

## Three Games, One Engine

B++ ships with three demo games in `games/`, all GPU-accelerated on Metal:

| Game | Folder | What it is |
|------|--------|-----------|
| **Snake** | `games/snake/` | Classic snake with ECS particle effects, high-score ranking, arena + pool allocators. Eat the apple, grow the tail, don't bite yourself. |
| **Pathfinder** | `games/pathfind/` | Rat vs cat chase. WASD movement, AI pursuit, collision, ECS particles on impact. Loads palette-indexed JSON sprites. |
| **Platformer** | `games/platformer/` | Side-scrolling platformer with Kenney Pixel Platformer assets (CC0). Real tilemap (stbtile), milli-pixel physics (stbphys), gravity, jumping, parallax, scrolling camera, coin collection, spikes, goal flag. Also ships a `platform_noasset.bpp` version with debug rectangles. |

---

## Back to the Future

In 1972, Ken Thompson's B language evolved into C. The rest is history — C conquered systems programming, operating systems, and eventually everything.

But what if a small group of rebels at Bell Labs had taken B in a different direction? What if instead of chasing general-purpose computing, they'd focused on one thing: **making games**?

B++ is that alternate timeline.

A language with the soul of B — every value is a word, no type declarations, no header files — but with 64-bit words, named struct fields, an orthogonal type system, and a compiler that produces native binaries directly. No assembler. No linker. No external tools.

But what if Sean Barret later joined tha group with his STB all written in B++?

And now we have a standard library that is, itself, a game engine and is ready to ship!

## Latest (14 April 2026 — version 0.23.x)

- **Mini Cooper Phase A + B0 shipped** — bitfields (`: bit` through `: bit7` with LSB-first packing, UBFX/BFI on ARM64 and SHR+AND on x86_64), `malloc_aligned(size, alignment)` built-in (uniform 16-byte bmem header, SIMD-ready), parser-level constant folding (`3 + 5` → `8`) and dead-code elimination (`if (0)` / `if (1)` / `while (0)` collapsed before the backends see them).
- **Ternary operator `?:` and short-circuit `&&` / `||`** — new `T_TERNARY` AST node, parser rewrites `a && b` → `T_TERNARY(a, b, 0)` and `a || b` → `T_TERNARY(a, 1, b)` so null-pointer guards like `if (p != 0 && p.field > 0)` are memory-safe on every backend. C emitter leans on C's native `?:`; both chip backends emit a single canonical handler.
- **Foundation refactor** — `.bo` module cache removed (~630 lines net, the #1 historical bug source), repo split into `src/backend/chip/<arch>/` + `os/<os>/` + `target/<arch>_<os>/` + `c/`. Syscalls collapsed into per-OS `BSYS_*` tables; `malloc` / `free` / `realloc` / `putchar` / `getchar` demoted from codegen builtins to real B++ functions in `bmem` / `bio`. Linux static ELF binaries now link literally nothing.
- **Language syntax complete for 1.0** — 22 keywords + 6 annotations. `load` (project-local imports), `static` (module-private with W020), `void` (implicit return 0), `switch` (value + condition dispatch, jump-table codegen for dense integer arms), `continue`, `extrn` (write-once-then-frozen storage class), `global` (worker-shared). Annotations `: base` / `: solo` bind to the smart dispatch engine.
- **Smart dispatch engine shipped (codegen rewrite pending)** — `call_graph_build` + `classify_all_functions` fixpoint + `promote_globals` auto-classification + `mark_reachable` BFS. `--show-promotions` prints every verdict. Lazy emit shrinks snake from 69 KB to 33 KB (52%).
- **Snake on Linux** via raw X11 wire protocol (zero FFI, zero libX11, zero libc on ELF). Cross-compile + Docker + XQuartz.
- **Tonify campaign** (batches 1-5 complete, ~350 `void` + ~200 `static` + ~50 `: base`), Node struct sliced (29% smaller AST), Rule 8 (typed struct access), Rule 9 (`var` vs `auto` struct decl), Rule 10 (portable built-ins), Rule 11 (ternary + short-circuit idioms) — all codified in `docs/tonify_checklist.md`.
- **23 stb modules + 2 platform layers** (in chip backends): draw, render, sprite, tile, phys, path, font, game, input, ui, math, col, color, array, hash, str, buf, file, io, image, arena, pool, ecs.

## The Language

```bpp
import "stbgame.bsm";

enum Dir { UP, DOWN, LEFT, RIGHT }

auto px, py, facing;

main() {
    game_init(320, 180, "B++ Game", 60);
    px = 160; py = 90; facing = RIGHT;

    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }

        if (key_down(KEY_UP))    { facing = UP; }
        if (key_down(KEY_DOWN))  { facing = DOWN; }
        if (key_down(KEY_LEFT))  { facing = LEFT; }
        if (key_down(KEY_RIGHT)) { facing = RIGHT; }

        if (facing == UP)    { py = py - 2; }
        if (facing == DOWN)  { py = py + 2; }
        if (facing == LEFT)  { px = px - 2; }
        if (facing == RIGHT) { px = px + 2; }

        clear(BLACK);
        draw_rect(px, py, 16, 16, GREEN);
        draw_text("arrows to move", 90, 4, 1, GRAY);
        draw_end();
    }
    return 0;
}
```

Compile and run:

```
bpp game.bpp -o game && ./game
```

No SDL. No raylib. No dependencies. One file in, one native binary out.

## What B++ Has

**Language:**
- **64-bit words** — every value is a 64-bit integer or double
- **Structs** — `struct Vec2 { x, y }` with heap or stack allocation, packed fields
- **Enums** — `enum State { MENU, PLAY, PAUSE, OVER }` resolved at compile time
- **Constants** — `const W = 320;` and `const abs(x) { ... }` with dead code elimination
- **Type hints** — `auto x: byte`, `auto f: half float` — 5 bases × 4 slices (11 real types)
- **Pointers** — `*(ptr)` dereference, `&var` address-of, `buf[i]` array indexing, `obj.field` struct access
- **Syntax sugar** — `buf[i]`, `for` loops, `else if`, string escapes (`\n`, `\t`, `\xHH`)
- **FFI** — call any C library: `import "SDL2" { void InitWindow(...); }`
- **Float math** — pure B++ `sqrt` (Newton-Raphson), IEEE 754 doubles, half/quarter float codegen
- **Memory** — `malloc`/`free`/`realloc` with Bell Labs style headers (size stored at ptr-8, real munmap)

**Compiler:**
- **Self-hosting** — the compiler compiles itself (fixed-point verified)
- **Native binaries** — ARM64 Mach-O + x86_64 ELF, built-in codesign, zero external tools
- **Cross-compilation** — `bpp --linux64 game.bpp -o game` from macOS
- **Modular compilation** — Go-style per-module codegen with content-addressed `.bo` cache
- **O(1) symbol tables** — six linear-scan lookups in the compiler refactored to hash queries via stbhash (eager / lazy / incremental strategies per call site)
- **Auto-injected platform layer** — the compiler pulls `_stb_platform_<target>.bsm` automatically when `stbgame` is in the import graph; library code never names the platform module
- **Diagnostics** — error codes (E001-E201), warnings (W001-W012), file:line:caret locations

**Debugger (`bug`):**
- **Zero-flag debugging** — `./bug ./program` just works, reads symbol table from binary
- **Function tracing** — call depth indentation, every function entry logged
- **Crash backtrace** — frame pointer chain walk shows full call stack
- **Local variable dump** — reads variable values from stack frame on crash (with `--bug`)
- **Cross-platform** — debugserver (macOS) + gdbserver (Linux) via GDB remote protocol

**Game engine (stb):**
- **GPU rendering** — Metal (macOS), Vulkan planned (Linux) — native API, no wrappers
- **GPU sprites** — any-size palette-indexed sprites: JSON load → RGBA → Metal texture → draw at scale
- **Tilemap engine** — grid + collision layer + PNG tileset loader + camera-aware GPU draw with type remap
- **Platformer physics** — milli-pixel Body, gravity, impulse jumping, separate-axis tile collision, ground probe
- **A\* pathfinding** — pure B++ A\* on a grid, binary min-heap with indexed decrease-key, leaf module, used by the cat AI in Pathfinder
- **Hash maps** — open addressing with linear probing, word keys (Knuth) and byte-sequence keys (djb2), tombstones, used by the compiler symbol tables
- **Software rendering** — CPU framebuffer with rects, circles, lines, sprites, text
- **Text rendering** — bitmap 8×8 fallback + pure B++ TrueType reader (cmap, glyf, Bézier, scanline AA)
- **Image loading** — pure B++ PNG loader with tRNS transparency (DEFLATE, Huffman, all filter types, zero FFI)
- **Game infrastructure** — arena allocator, object pool, entity-component system
- **Collision** — rect overlap, point-in-rect, distance² (squared, no sqrt needed)
- **23 modules** in `stb/` — draw, render, sprite, tile, phys, **path**, font, game, input, ui, math, col, color, array, **hash**, str, buf, file, io, image, arena, pool, ecs. Plus 2 platform layers that live alongside the per-chip backends in `src/aarch64/` and `src/x86_64/` (auto-injected by the compiler when `stbgame` is in the import graph).

## What B++ Doesn't Have

No classes. No templates. No exceptions. No garbage collector. No package manager. No build system. No 200MB toolchain download.

You write code. You compile it. You run it.

## The Standard B Library (stb)

The name **stb** is a tribute to [Sean Barrett's stb libraries](https://github.com/nothings/stb) — the single-header C libraries that defined a generation of small, focused, dependency-free building blocks for graphics, audio, fonts, and data. `stb_image.h`, `stb_truetype.h`, `stb_vorbis.c`, `stb_ds.h` and the rest are how a lot of indie game developers learned that "the right amount of library" is one file you can read in an afternoon. B++'s standard library is the same idea reframed as a pure-B++ collection: small modules, no headers (B++ has no headers anyway), no third-party dependencies, the minimum API surface for the maximum game.

stb is the game engine. It's not a wrapper around SDL or raylib — it **is** the engine, written entirely in B++.

**Rendering:**

| Module | What it does |
|--------|-------------|
| `stbdraw` | Software framebuffer rendering — rects, circles, lines, sprites, text |
| `stbrender` | GPU-accelerated 2D rendering — rects, circles, lines, outlines, text, sprites (Metal) |
| `stbsprite` | GPU sprite loading and rendering — any w×h, palette-indexed, JSON loader |
| `stbfont` | 8×8 bitmap font fallback + pure B++ TrueType reader (cmap, glyf, Bezier, scanline AA) |
| `stbcolor` | Color palette — `rgba()`, named constants (BLACK, WHITE, RED, BLUE, ...) |

**Game loop & input:**

| Module | What it does |
|--------|-------------|
| `stbgame` | Game loop — init, frame timing, quit |
| `stbinput` | Keyboard and mouse input from memory arrays |
| `stbui` | Immediate-mode UI widgets with per-frame arena |

**Math, physics & AI:**

| Module | What it does |
|--------|-------------|
| `stbmath` | Vec2, PRNG, sqrt (Newton-Raphson), abs, min, max, clamp, fixed-point trig |
| `stbcol` | Collision detection — rect overlap, point-in-rect, distance squared |
| `stbphys` | Platformer physics — Body struct, gravity, jump, tile collision, milli-pixel precision |
| `stbtile` | Tilemap engine — grid, collision layer, PNG tileset loader, camera-aware GPU draw, type remap |
| `stbpath` | A* pathfinding on a grid — binary min-heap with indexed decrease-key, Manhattan heuristic, leaf module |

**Data structures:**

| Module | What it does |
|--------|-------------|
| `stbarray` | Dynamic arrays with shadow header (stb_ds.h style) |
| `stbhash` | Hash maps — word keys (Knuth) and byte-sequence keys (djb2), open addressing, tombstones |
| `stbstr` | String operations and growable string builder |
| `stbbuf` | Raw buffer read/write (u8, u16, u32, u64) |

**Game infrastructure:**

| Module | What it does |
|--------|-------------|
| `stbarena` | Generic bump allocator — O(1) alloc, O(1) reset, 8-byte aligned |
| `stbpool` | Fixed-size object pool — O(1) get/put via embedded freelist |
| `stbecs` | Entity-component system — spawn/kill/recycle, parallel arrays, milli-unit physics |

**I/O & assets:**

| Module | What it does |
|--------|-------------|
| `stbfile` | File I/O — read and write entire files |
| `stbio` | Console I/O (print_int, print_msg, print_char) |
| `stbimage` | Pure B++ PNG loader — DEFLATE, Huffman, all filter types, zero FFI |

**Platform layers** (internal, selected automatically):

| Module | What it does |
|--------|-------------|
| `_stb_platform_macos` | Cocoa window, Metal GPU, texture upload, CoreGraphics software, keyboard/mouse |
| `_stb_platform_linux` | Terminal ANSI rendering, keyboard input |

Two rendering paths: `stbdraw` for CPU software rendering (framebuffer → CoreGraphics/ANSI), `stbrender` for GPU-accelerated rendering (Metal on macOS, Vulkan planned for Linux). Same game code — just swap `draw_*` for `render_*` and add `render_init()` after `game_init()`.

GPU rendering uses the platform's native API directly — Metal via `objc_msgSend`, Vulkan via `libvulkan.so`. No SDL. No OpenGL wrappers. The compiler talks to the hardware.

## Four Backends, Same Code

The game code never changes. Only the import:

```bpp
// Native macOS — zero dependencies:
import "stbgame.bsm";

// Linux terminal — ANSI rendering, zero dependencies:
import "stbgame.bsm";
// compile with: bpp --linux64 game.bpp -o game

// SDL2:
import "stbgame.bsm";
import "drv_sdl.bsm";

// Raylib:
import "stbgame.bsm";
import "drv_raylib.bsm";
```

## Getting Started

Install the compiler and standard library manually:

sudo cp bpp /usr/local/bin/                                                                
sudo mkdir -p /usr/local/lib/bpp/stb                                                       
sudo cp stb/*.bsm /usr/local/lib/bpp/stb/                                                  
      
### Install

```bash
# Bootstrap from C source (first time, or if bpp binary is missing)
clang bootstrap.c -o bpp

# Bootstrap + install globally
sh install.sh

# Or install without re-bootstrapping
sh install.sh --skip
```

The `bootstrap.c` file is the compiler emitted as C (~15K lines). It is the seed that builds everything — no external tools needed beyond a C compiler. **Note:** bootstrap.c is currently outdated and needs regeneration (see TODO).

### Play the Games

Each game lives in its own folder under `games/` with its own assets. Compile and run from inside the folder so that relative asset paths resolve:

```bash
# Snake with ECS particles, ranking, arena + pool
cd games/snake
bpp --clean-cache snake_gpu.bpp -o snake
./snake

# Pathfinder — rat vs cat chase
cd games/pathfind
bpp --clean-cache pathfind.bpp -o path
./path

# Platformer with Kenney Pixel Platformer assets (CC0)
cd games/platformer
bpp --clean-cache platformer.bpp -o plat_asset
./plat_asset

# Platformer with debug rectangles (no assets needed)
cd games/platformer
bpp --clean-cache platform_noasset.bpp -o plat_noasset
./plat_noasset
```

### Run on Linux (X11 via Docker + XQuartz)

You can cross-compile a B++ game to a Linux x86_64 ELF binary on macOS and run it inside an Ubuntu container, with the window appearing on your Mac via XQuartz. The Linux platform layer talks raw X11 wire protocol over a TCP socket — no `libX11`, no FFI, just `sys_socket` + `sys_connect` + `sys_write`.

**One-time setup on macOS:**

```bash
# 1. Install XQuartz (X11 server for macOS)
brew install --cask xquartz

# 2. Open XQuartz, then enable network connections:
#    XQuartz → Preferences → Security → "Allow connections from network clients"
#    Then quit XQuartz fully and reopen.

# 3. Persist the listen-on-TCP setting (XQuartz defaults to off)
defaults write org.xquartz.X11 nolisten_tcp 0
killall XQuartz 2>/dev/null
open -a XQuartz

# 4. Allow localhost connections to the X server
xhost +localhost

# 5. Install Docker Desktop for Mac if you do not already have it.
```

**Cross-compile and run a game:**

```bash
# Cross-compile snake to a Linux x86_64 ELF binary
bpp --linux64 examples/snake_cpu.bpp -o /tmp/snake_x11

# Run inside Ubuntu with the host XQuartz as the display
docker run --rm \
  --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 \
  -v /tmp:/tmp \
  ubuntu:22.04 \
  /tmp/snake_x11
```

A 320×180 window opens in XQuartz showing snake running on Linux. Arrow keys move the snake, the game speed and behavior are identical to the native macOS build. The window obeys the close button and the FocusOut handler clears stuck keys when you Alt-Tab away.

**The full X11 test suite that ships in `tests/`:**

These were used to verify each phase of the X11 wire-protocol implementation. They are also the easiest way to confirm a working XQuartz + Docker setup on a fresh machine.

```bash
# Phase 1 — dark blue window, ESC or close button to exit
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_window

# Phase 2 — animated rectangles + circle + text
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_draw

# Phase 3 — input test (arrows to move player, mouse to draw yellow ball, ESC to quit)
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_input

# Movement test — prints player position to terminal every 30 frames
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_movement

# Snake on Linux — arrow keys to move, eat the apple, grow the tail
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/snake_x11
```

If `docker` is not on your `PATH` after a Docker Desktop install on macOS, the binary lives at `/Applications/Docker.app/Contents/Resources/bin/docker` — substitute that for `docker` in any of the commands above.

**Caveats**:
- Currently only `draw_*` (CPU framebuffer) games work via X11. GPU games (`render_*`) need Vulkan, which is deferred (see TODO).
- macOS Docker has no GPU passthrough — even if Vulkan were implemented, real GPU rendering would require Linux hardware. The X11 path is software-only.
- You can also run without `DISPLAY`: the same binary falls back to ANSI terminal rendering.

### Run via the C Emitter (raylib path, portable C output)

The C emitter (`bpp --c`) translates B++ to portable C99. The right pattern is to write the game using `drv_raylib.bsm` instead of `stbgame.bsm`, so the generated C calls regular C functions (no Objective-C, no `objc_msgSend` calling-convention issues).

**Install raylib and gcc:**

```bash
# macOS
brew install raylib

# Ubuntu / Debian
sudo apt-get install libraylib-dev gcc
```

**Compile and run a game:**

```bash
# 1. Generate C from a raylib-flavored game
bpp --c examples/snake_raylib.bpp > /tmp/snake_raylib.c

# 2. Compile the C with gcc + raylib + objc (objc only on macOS)
gcc -Wno-implicit-function-declaration \
    -Wno-parentheses-equality \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    /tmp/snake_raylib.c -o /tmp/snake_raylib_c \
    -lraylib -lobjc

# 3. Run
/tmp/snake_raylib_c
```

On Linux, drop `-lobjc` and use `/usr/include` / `/usr/lib` paths. The same `.c` file should compile on Windows (with raylib for MSYS2/Visual Studio), BSDs, and Emscripten.

### Debug Any Game

```bash
./bug ./snake               # function tracing + crash report
./bug ./plat_assets         # works on any bpp binary, no flags needed
```

### Write Your Own

```bpp
// CPU rendering (software framebuffer):
import "stbgame.bsm";

main() {
    game_init(320, 180, "My Game", 60);
    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }
        clear(DARKGRAY);
        draw_circle(160, 90, 40, RED);
        draw_text("B++", 140, 82, 2, WHITE);
        draw_end();
    }
    return 0;
}
```

```bpp
// GPU rendering (Metal on macOS, Vulkan on Linux):
import "stbgame.bsm";
import "stbrender.bsm";

main() {
    game_init(320, 180, "My Game", 60);
    render_init();
    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }
        render_begin();
        render_clear(DARKGRAY);
        render_circle(160, 90, 40, RED);
        render_rect(60, 140, 200, 20, BLUE);
        render_end();
    }
    return 0;
}
```

```
bpp mygame.bpp -o mygame && ./mygame
```

## Project Structure

```
b++/
├── src/              — Compiler core (self-hosting, ~18 B++ modules)
│   ├── aarch64/      — ARM64 backend (encoder, codegen, Mach-O writer)
│   └── x86_64/       — x86_64 backend (encoder, codegen, ELF writer)
├── stb/              — Standard B Library (23 modules, pure B++, the game engine)
├── games/            — Complete playable games
│   ├── snake/        — Snake with ECS particles + ranking
│   ├── pathfind/     — Rat-and-cat chase with AI pursuit
│   └── platformer/   — Side-scrolling platformer with Kenney assets
├── examples/         — Small demos (hello, mouse, gpu_colours, raylib/sdl)
├── drivers/          — Backend drivers (SDL2, raylib — optional)
├── tests/            — Compiler and library tests
├── docs/             — Language manual, journal, TODO, plan reviews
├── ~/.bpp/cache/     — Global module cache (.bo files, content-addressed)
├── bpp               — The compiler binary
└── bug               — The debugger binary
```

## Platform Status

| Target | Status |
|--------|--------|
| macOS ARM64 (Apple Silicon) | **Working** — native Cocoa + Metal GPU, SDL2, raylib |
| Linux x86_64 | **Working** — static ELF, terminal rendering. GPU (Vulkan) in progress |
| Windows x86_64 | Planned — codegen + Win32 + DX12/Vulkan |
| WebAssembly | Future — codegen + WebGPU |

GPU access is native per platform — Metal on macOS, Vulkan on Linux, DirectX 12 on Windows. No middleware. The compiler talks to the hardware directly via FFI. stb modules are 100% platform-agnostic. Each target only needs a codegen backend + platform layer.

## Requirements

**macOS ARM64:**
- **No dependencies** for native games
- **SDL2** (`brew install sdl2`) for SDL examples
- **raylib** (`brew install raylib`) for raylib examples

**Linux x86_64:**
- Cross-compile from macOS: `bpp --linux64 game.bpp -o game`
- Run natively or in Docker — 45KB static binary, zero dependencies

## The Compiler

The B++ compiler is written in B++ and compiles itself. It produces
native ARM64 Mach-O binaries with built-in SHA-256 codesigning.

```
bpp source.bpp -o binary       # native ARM64 macOS (default)
bpp --linux64 src.bpp -o bin   # cross-compile to x86_64 Linux ELF
bpp --bug source.bpp -o bin    # emit .bug debug map (enhanced debugging)
bpp --c source.bpp              # emit C (for debugging)
bpp --asm source.bpp            # emit ARM64 assembly
bpp --show-deps source.bpp     # print module dependency graph
bpp --clean-cache              # delete all cached .bo files
```

16 compiler core modules + 5 files per chip backend (`src/aarch64/`, `src/x86_64/`) + 23 stb library modules, ~17,000 lines of B++. Self-hosting verified at every stage (`shasum bpp2 == bpp3`). Import search paths: `./`, `stb/`, `drivers/`, `src/`, `src/aarch64/`, `src/x86_64/`, `/usr/local/lib/bpp/` and its subfolders.

### The Debugger

```
./bug ./program                  # trace + crash report (reads symbol table)
./bug ./program                  # + locals if .bug file present
```

`bug` is a B++ program that uses the GDB remote protocol to communicate with Apple's debugserver (macOS) or gdbserver (Linux). No entitlements, no codesign, no special flags needed. Function names come from the Mach-O/ELF symbol table automatically.

## Contributing

B++ is open source and we want your help. The language is young and there's a lot to build:

**Platform ports** — Each new platform needs a codegen backend (instruction encoder + binary writer) and a platform layer (window, present, input, timing). ARM64 macOS and x86_64 Linux are done. Windows/Win32 and WebAssembly are open.

**Game tools** — Sprite editors, tilemap editors, level designers, particle systems — all built in B++ using stb. The language was made for this.

**stb modules** — Audio (`stbaudio`), tilemaps (`stbtile`), networking (`stbnet`), physics, pathfinding. Each is a pure B++ module with no platform dependencies.

**Games** — The best way to find what's missing in the language. Build a game, hit a wall, fix the compiler. That's how every feature in B++ was born.

**Documentation** — Tutorials, examples, guides. Especially for people coming from C, Lua, or Python game dev.

### How to start

1. Fork the repo
2. Pick something from the list above
3. Build and test (`bpp src/bpp.bpp -o bpp` to rebuild the compiler)
4. Open a PR

No bureaucracy. No code of conduct longer than the compiler. Just build cool stuff.

## License

MIT License

Copyright (c) 2026 Daniel Obino

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

*B++ was designed and implemented by Daniel Obino, 2026.*

*The compiler bootstrapped itself on March 20, 2026.*

*Zero-dependency native compilation on March 23, 2026.*

*Native game rendering without external libraries on March 24, 2026.*

*Compiler diagnostics and x86_64 Linux cross-compilation on March 25, 2026.*

*Modular Compilation, Dynamic Arrays, Type Hints, and Backend Reorganization on March 26, 2026.*

*Cache modular fix, memcpy/realloc builtins, per-variable hints, mouse events, install script on March 26, 2026.*

*float_ret Builtins, Go-Style Cache, Type System Fixes, Mouse Tracking on march 27, 2026*

*Mach-O Fix, Packed Structs, shr/assert, C Emitter, Input Lag on March 27, 2026*

*Type System Refactor: base × slice grid, f32/f16 support, orthogonal type composition on March 27, 2026.*

*.bo cache fix, compiler self-hash on March 30, 2026.*

*Optimized type encoding (bit-0 float flag), FLOAT_H/Q codegen, putchar_err, parser half-float fix on March 31, 2026.*

*GPU rendering via Metal, Go-style cache, string escape sequences on March 31, 2026.*

*Cache system overhaul (4 bugs), GPU render API (lines, circles, outlines), platform-agnostic trig on March 31, 2026.*

*Syntax Sugar + Codebase Cleanup on April 1, 2026.*

*Cache Fix, sys_fork, Bug Debugger, Structural Overhaul on April 1, 2026.*

*Game Infrastructure: Arena allocator, Object Pool, Entity-Component System, Syscall Builtins, Bug Debugger Fixes on April 2, 2026.*

*Bug Debugger v2: debugserver/gdbserver backend, GDB remote protocol, zero-flag debugging, crash backtrace + locals, sys_socket/sys_connect/sys_usleep builtins, Mach-O symbol table export on April 2, 2026.*

*GPU Text + TrueType + realloc: pure B++ TrueType reader, Metal glyph atlas, Bell Labs malloc/free/realloc, stbimage PNG loader, module reorganization on April 3, 2026.*

*GPU Sprites + Module Cache Fix: stbsprite.bsm (any w×h), pure B++ sqrt, 3 critical .bo cache bugs fixed (param types + return types + null terminators), pathfinder game ported on April 5, 2026.*

*B++ 0.2 — Tilemap, Physics, Address-of, Kenney assets: stbtile.bsm (tilemap engine), stbphys.bsm (platformer physics), address-of operator `&x` (9 compiler files + W012 diagnostic), PNG tRNS transparency, Metal alpha discard, platformer with Kenney Pixel Platformer assets (CC0), ECS particle alpha bug fixed on April 6, 2026.*

*B++ 0.21 — Linux X11, Validator Integration, C Emitter Modernized: X11 wire protocol Phases 1-3 (`_stb_platform_linux.bsm` 276 → 1160 lines, Unix socket + TCP, handshake, CreateWindow with FocusChangeMask, XPutImage, evdev/Apple keycode auto-detect, FocusOut stuck-key fix, WM_DELETE close, WM_NAME title), validator integrated into the incremental ARM64/x86_64 pipeline (was monolithic-only since written), E052 phantom check removed, `bpp_typeck.bsm` 507-line orphan deleted, C emitter modernized (8 builtins added, extern dedup with varargs, libc symbol skip), Snake running on Linux via Docker + XQuartz, raylib path verified end-to-end on April 6, 2026.*

*B++ 0.21 — stbhash, stbpath, O(1) Compiler Symbols, Auto-Platform: stbhash.bsm (word + byte-keyed hash maps, ~610 lines, used by the compiler), stbpath.bsm (A* with binary heap + indexed decrease-key, leaf module, ~440 lines), six compiler symbol lookups refactored from linear scan to O(1) hash (val_find_func/extern eager, find_func_idx eager, find_struct incremental at parser AND bo cache loader, is_extern/find_extern lazy), platform/observe files moved into chip folders (`src/aarch64/`, `src/x86_64/`) with chip+OS coupling READMEs, `_stb_platform.bsm` auto-injected by the compiler when stbgame is in the import graph (explicit import removed from stbgame.bsm), pathfind game refactored to milli-unit + tilemap + A* cat AI, test suite cleaned (71 → 52 tests, 100% passing), 23 stb modules + 2 platform layers in chip folders on April 6, 2026.*

*B++ 0.22 — Foundation Cleanup, Maestro Phase 1, Four Compiler Bug Fixes: eight modules promoted from `stb/` to `src/` as `bpp_*.bsm` (defs, array, hash, buf, str, io, math, file — compiler primitives and universal utilities no longer pretending to be standard library), `bpp_beat.bsm` + `bpp_job.bsm` + `bpp_maestro.bsm` shipping the first parallel B++ runtime (worker pool, SPSC queues, fixed-timestep frame loop, jazz/rock solo-base-render metaphor canonized), `mem_barrier` builtin (ARM64 DMB ISH + x86_64 MFENCE + C `__sync_synchronize`), pthread FFI via libSystem, snake_maestro.bpp as the first parallel game, `global` keyword as the foundation stone of smart dispatch, four latent compiler bugs fixed (pathbuf clobber on nested `process_file`, inode reuse poisoning macOS codesign cache, pthread-after-NSApp SIGTERM, install.sh ghost files from pre-0.21 directory moves) on April 8, 2026.*

*B++ 0.23 — Language Syntax Complete, Smart Dispatch Engine, Module Discipline: `load` / `static` / `void` keywords and `: base` / `: solo` annotations (the boundary between programmer-written and compiler-inferred locks in for 1.0), full smart dispatch implementation (`call_graph_build`, `classify_all_functions` double-buffered fixpoint on a height-1 lattice, `promote_globals` analysis, `mark_reachable` BFS, `--show-promotions` and `--stats` flags), function dedup + `stub` keyword + E105 (main in .bsm) + E221 (cross-module duplicate) + E222 (circular import), arena-backed AST via `bpp_arena.bsm` (8 MB arena, cache-local AST walks), Clang-style diagnostics with source line + caret + `Node.src_tok` position tracking, lazy emit shrinking snake from 69 KB to 33 KB (52%), bug debugger Level 1 with `[auto]` / `[global]` / `[extrn]` tags per variable, `continue` + `extrn` + `envp` / `getenv` on April 9, 2026.*

*B++ 0.23.1 — Cache Integrity, Diagnostic Accuracy, Tonify Infrastructure: two critical cache bugs fixed (auto-injected modules recording dep edges so cross-module invalidation works, `bo_read_export` populating `fn_static` / `fn_void` / `fn_phase_hint` to match `funcs[]` length), `docs/tonify_checklist.md` codified as the expert-mode reference for sweeping `.bsm` files, Rule 4 warning about `: base` on builtin-calling functions documented after the first W013 fire during batch 1, `bpp_io` / `bpp_array` / `bpp_buf` / `bpp_str` tonified as batch 1 canary on April 9, 2026.*

*Tonify Batches 1-5, Switch Statement, Multi-Error Diagnostics, Node Struct Slice, LDRSW Sign-Extension: ~350 `void` annotations + ~200 `static` markers + ~50 `: base` annotations + ~50 `while`-with-counter converted to `for` across 15 stb modules + 12 compiler internals + both platform layers, `switch` statement with value and condition dispatch forms (no fallthrough, jump-table codegen for dense integer arms, W021 exhaustiveness warning), `bare return;` in void functions, multi-error `diag_error` accumulation with capped output, Node struct sliced (`ntype: byte` / `dispatch: byte` / `itype: byte` shrinking the AST 29%), RECORD_SZ decoupled from NODE_SZ, ~200 raw `*(node + 8)` sites refactored to typed `n.a` access (sliced struct access rule born here), `: half` sign-extension fixed on both chip backends (ARM64 LDRSW, x86_64 MOVSXD), `_stb_core_linux.bsm` unblocking Linux cross-compile, maestro accumulator pattern for fixed timestep, `job_parallel_for` serial fallback for progressive enhancement, E230 (static const at file scope) diagnostic on April 11, 2026.*

*Foundation Refactor Mega-Session — Cache Removal, Repo Reorganization, bsys Tables, brt0 Runtime, bmem Allocator, Switch Jump Tables: the `.bo` module cache removed entirely (~630 lines net, the single largest bug source across the bootstrap's history, every compile now from source at 0.27 s), repo reorganized into `src/backend/chip/<arch>/` + `src/backend/os/<os>/` + `src/backend/target/<arch>_<os>/` + `src/backend/c/` to separate chip from OS from container from transpiler, bsys tables per OS replacing 18 hardcoded syscall numbers per codegen, `_brt0_<target>.bsm` moving `_bpp_argc` / `_bpp_argv` / `_bpp_envp` from codegen sentinels to real B++ globals, `putchar` / `putchar_err` / `getchar` demoted from codegen builtins to B++ functions in `bpp_io.bsm`, `bmem` allocator (`_bmem_<os>.bsm`) making `malloc` / `free` / `realloc` real B++ functions with `sys_mmap` / `sys_munmap` as the only builtins, switch jump tables on both backends + eval-once condition for sparse switches + C emitter switch (previously if/else-if chain), sliced struct access bug discovered and documented as Rule 8 in tonify on April 13-14, 2026.*

*Mini Cooper Phase A + B0, T_TERNARY Side Quest, Rule 11 Sweep: bitfields (`: bit`, `: bit2` through `: bit7`) with LSB-first packing, precomputed `(byte, bit)` per field, UBFX/BFI on ARM64 and SHR+AND/RMW on x86_64, `malloc_aligned(size, alignment)` built-in with uniform 16-byte header shared between regular and aligned allocations, constant folding and dead-code elimination lifted to the parser (single implementation all backends inherit, per-backend duplicates deleted), new `T_TERNARY` AST node and parser rewrite of `a && b` → `T_TERNARY(a, b, 0)` and `a || b` → `T_TERNARY(a, 1, b)` closing the short-circuit divergence between the C emitter and native backends, new `T_BLOCK` node for DCE statement groups, eight canonical tests added (bitfield, aligned_alloc, const_fold, realloc, signed_half, switch, ternary, short_circuit), Rule 11 idioms (ternary and short-circuit) swept across `bpp_types`, `bpp_parser`, `bpp_lexer`, `bpp_import`, `bpp_dispatch`, and `stb/stbimage` so the compiler source demonstrates the new patterns in the places programmers read first on April 14, 2026.*

*Designed and built by Daniel Obino. Compiler bootstrapped March 20, 2026.*
