# B++

### Bug Debugger v2 + Game Infrastructure — 2 April 2026

Self-hosting compiler with GPU rendering (Metal on macOS), Go-style modular cache, **native debugger** (`bug` — zero-flag function tracing, crash backtrace, local variable dump via debugserver/gdbserver), game infrastructure (arena, pool, ECS), type hints with Base×Slice system, two native backends (ARM64 + x86_64), and cross-compilation. The compiler compiles itself and produces signed native binaries with zero external tools.

---

## Back to the Future

In 1972, Ken Thompson's B language evolved into C. The rest is history — C conquered systems programming, operating systems, and eventually everything.

But what if a small group of rebels at Bell Labs had taken B in a different direction? What if instead of chasing general-purpose computing, they'd focused on one thing: **making games**?

B++ is that alternate timeline.

A language with the soul of B — every value is a word, no type declarations, no header files — but with 64-bit words, named struct fields, an orthogonal type system, and a compiler that produces native binaries directly. ARM64 macOS and x86_64 Linux. No assembler. No linker. No external tools.

And a standard library that is, itself, a game engine.

## Latest (2 April 2026)

- **Bug debugger v2**: `./bug ./program` — zero flags, just works. Function tracing with call depth indentation, crash backtrace via FP chain walk, local variable dump on crash. Uses Apple's debugserver (macOS) / gdbserver (Linux) via GDB remote protocol. Reads function names directly from Mach-O/ELF symbol table.
- **Game infrastructure**: `stbarena` (bump allocator), `stbpool` (O(1) freelist), `stbecs` (entity-component system with milli-unit physics).
- **New builtins**: `sys_socket`, `sys_connect`, `sys_usleep`, `sys_ptrace`, `sys_wait4`, `sys_getdents`, `sys_unlink` — all three backends.
- **Cache fixed for real**: Go-style modular cache with per-program isolation and cascading invalidation.

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
- **Syntax sugar** — `buf[i]`, `for` loops, `else if`, string escapes (`\n`, `\t`, `\xHH`)
- **FFI** — call any C library: `import "SDL2" { void InitWindow(...); }`
- **Float math** — `sqrt`, `floor`, `fabs` via FFI, IEEE 754 doubles, half/quarter float codegen

**Compiler:**
- **Self-hosting** — the compiler compiles itself (fixed-point verified)
- **Native binaries** — ARM64 Mach-O + x86_64 ELF, built-in codesign, zero external tools
- **Cross-compilation** — `bpp --linux64 game.bpp -o game` from macOS
- **Modular compilation** — Go-style per-module codegen with content-addressed `.bo` cache
- **Diagnostics** — error codes (E001-E201), warnings (W001-W011), file:line:caret locations

**Debugger (`bug`):**
- **Zero-flag debugging** — `./bug ./program` just works, reads symbol table from binary
- **Function tracing** — call depth indentation, every function entry logged
- **Crash backtrace** — frame pointer chain walk shows full call stack
- **Local variable dump** — reads variable values from stack frame on crash (with `--bug`)
- **Cross-platform** — debugserver (macOS) + gdbserver (Linux) via GDB remote protocol

**Game engine (stb):**
- **GPU rendering** — Metal (macOS), Vulkan planned (Linux) — native API, no wrappers
- **Software rendering** — CPU framebuffer with rects, circles, lines, sprites, text
- **Game infrastructure** — arena allocator, object pool, entity-component system
- **17 modules** — draw, render, font, game, input, ui, math, col, array, str, buf, file, io, image, arena, pool, ecs

## What B++ Doesn't Have

No classes. No templates. No exceptions. No garbage collector. No package manager. No build system. No 200MB toolchain download.

You write code. You compile it. You run it.

## The Standard B Library (stb)

stb is the game engine. It's not a wrapper around SDL or raylib — it **is** the engine, written entirely in B++.

**Rendering:**

| Module | What it does |
|--------|-------------|
| `stbdraw` | Software framebuffer rendering — rects, circles, lines, sprites, text |
| `stbrender` | GPU-accelerated 2D rendering — rects, circles, lines, outlines (Metal) |
| `stbfont` | 8×8 bitmap font for text rendering |

**Game loop & input:**

| Module | What it does |
|--------|-------------|
| `stbgame` | Game loop — init, frame timing, quit |
| `stbinput` | Keyboard and mouse input from memory arrays |
| `stbui` | Immediate-mode UI widgets with per-frame arena |

**Math & physics:**

| Module | What it does |
|--------|-------------|
| `stbmath` | Vec2, PRNG, abs, min, max, clamp, fixed-point trig (cos/sin) |
| `stbcol` | Collision detection (AABB, circles) |

**Data structures:**

| Module | What it does |
|--------|-------------|
| `stbarray` | Dynamic arrays with shadow header |
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
| `stbio` | Console I/O (print_int, print_msg) |
| `stbimage` | Image loading (PNG, JPEG, BMP) via stb_image FFI |

**Platform layers** (internal, selected automatically):

| Module | What it does |
|--------|-------------|
| `_stb_platform_macos` | Cocoa window, Metal GPU, CoreGraphics software, keyboard/mouse |
| `_stb_platform_linux` | Terminal ANSI rendering, keyboard input |

Two rendering paths: `stbdraw` for CPU software rendering (framebuffer → CoreGraphics/ANSI), `stbrender` for GPU-accelerated rendering (Metal on macOS, Vulkan on Linux). Same game code — just swap `draw_end()` for `render_end()`.

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

### Run Examples

```bash
# Snake with ECS + arena + pool (native, no dependencies)
bpp examples/snake_full.bpp -o snake && ./snake

# Debug snake with function tracing
./bug ./snake
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
├── src/              — Compiler core (16 B++ modules, self-hosting)
│   ├── aarch64/      — ARM64 backend (encoder, codegen, Mach-O writer)
│   └── x86_64/       — x86_64 backend (encoder, codegen, ELF writer)
├── stb/              — Standard B Library (pure B++, the game engine)
├── drivers/          — Backend drivers (SDL2, raylib — optional)
├── examples/         — Working game examples
├── tests/            — Compiler and library tests
├── docs/             — Language manual, journal, and evolution roadmap
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

16 compiler modules + 17 stb library modules, ~13,000 lines of B++. Self-hosting verified at every stage. Import search paths: `./`, `stb/`, `drivers/`, `src/`, `/usr/local/lib/bpp/`.

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

*Designed and built by Daniel Obino. Compiler bootstrapped March 20, 2026.*
