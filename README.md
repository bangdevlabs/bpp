# B++

### Rock Solid B++ — 27 March 2026

Self-hosting compiler with modular compilation, type hints, dynamic arrays, two native backends (ARM64 + x86_64), and a monolithic fallback. The compiler compiles itself, caches per-module object files, and produces signed native binaries with zero external tools. Let's rock!

---

## Back to the Future

In 1972, Ken Thompson's B language evolved into C. The rest is history — C conquered systems programming, operating systems, and eventually everything.

But what if a small group of rebels at Bell Labs had taken B in a different direction? What if instead of chasing general-purpose computing, they'd focused on one thing: **making games**?

B++ is that alternate timeline.

A language with the soul of B — every value is a word, no type declarations, no header files — but with 64-bit words, named struct fields, and a compiler that produces native binaries directly. ARM64 macOS and x86_64 Linux. No assembler. No linker. No external tools.

And a standard library that is, itself, a game engine.

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

- **64-bit words** — every value is a 64-bit integer or double
- **Structs** — `struct Vec2 { x, y }` with heap (`auto`) or stack (`var`) allocation
- **Enums** — `enum State { MENU, PLAY, PAUSE, OVER }` resolved at compile time
- **Float math** — `sqrt`, `floor`, `fabs` via system FFI, IEEE 754 doubles
- **FFI** — call any C library: `import "SDL2" { void InitWindow(...); }`
- **Self-hosting** — the compiler compiles itself
- **Native binaries** — ARM64 Mach-O + x86_64 ELF, built-in codesign, zero external tools
- **Compiler diagnostics** — error codes (E001-E201), warnings (W001-W005), file:line locations
- **Cross-compilation** — compile Linux binaries from macOS (`--linux64`)
- **Type hints** — `auto x: byte, y: quarter` — per-variable sub-word types for performance tuning
- **Builtins** — `memcpy`, `realloc`, `float_ret()`/`float_ret2()` for struct returns from extern calls
- **Modular compilation** — Go-model per-module codegen with .bo cache and Go-style hash chain invalidation
- **Monolithic fallback** — single-pass pipeline for C emitter, ASM output, and future backends (WASM)
- **Module dependency tracking** — content + dependency hashing, topological sort (`--show-deps`)
- **Install script** — `sh install.sh` bootstraps and installs compiler + stb + drivers globally

## What B++ Doesn't Have

No classes. No templates. No exceptions. No garbage collector. No package manager. No build system. No 200MB toolchain download.

You write code. You compile it. You run it.

## The Standard B Library (stb)

stb is the game engine. It's not a wrapper around SDL or raylib — it **is** the engine, written entirely in B++.

| Module | What it does |
|--------|-------------|
| `stbdraw` | Software framebuffer rendering — rects, circles, lines, sprites, text |
| `stbinput` | Keyboard and mouse input from memory arrays |
| `stbgame` | Game loop — init, frame timing, quit |
| `stbfont` | 8×8 bitmap font for text rendering |
| `stbmath` | Vec2, PRNG, abs, min, max, clamp |
| `stbarray` | Dynamic arrays with shadow header |
| `stbstr` | String operations and growable string builder |
| `stbbuf` | Raw buffer read/write (u8, u16, u32, u64) |
| `stbfile` | File I/O — read and write entire files |
| `stbui` | Immediate-mode UI widgets |
| `stbcol` | Collision detection (AABB, circles) |
| `stbio` | Console I/O (print_int, print_msg) |

New in stb: mouse tracking on macOS (float_ret for NSPoint), window close detection, `blend_px()` for alpha blending, `file_write_all()` for saving files, `mouse_pressed()`/`mouse_released()` with edge detection.

Every pixel is written to a memory buffer. The compiler's platform layer puts those pixels on screen. On macOS, that's Cocoa + CoreGraphics — no SDL, no OpenGL, no Metal. Just `objc_msgSend` and a `CGBitmapContext`.

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
# Snake — native (no dependencies)
bpp examples/snake_native.bpp -o snake && ./snake

# Snake — SDL2 (requires SDL2 installed)
bpp examples/snake_sdl.bpp -o snake && ./snake

# Snake — raylib (requires raylib installed)
bpp examples/snake_raylib.bpp -o snake && ./snake

# Interactive demo — SDL2
bpp examples/sdl_demo.bpp -o demo && ./demo

# Bouncing cubes — raylib direct FFI
bpp examples/raylib_demo.bpp -o cubes && ./cubes
```

### Write Your Own

```bpp
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

```
bpp mygame.bpp -o mygame && ./mygame
```

## Project Structure

```
b++/
├── src/              — Compiler core (12 B++ modules, self-hosting)
│   ├── aarch64/      — ARM64 backend (encoder, codegen, Mach-O writer)
│   └── x86_64/       — x86_64 backend (encoder, codegen, ELF writer)
├── stb/              — Standard B Library (pure B++, the game engine)
├── drivers/          — Backend drivers (SDL2, raylib — optional)
├── examples/         — Working game examples
├── tests/            — Compiler and library tests
├── docs/             — Language manual, journal, and evolution roadmap
├── .bpp_cache/       — Module cache (.bo files, auto-generated)
└── bpp               — The compiler binary
```

## Platform Status

| Target | Status |
|--------|--------|
| macOS ARM64 (Apple Silicon) | **Working** — native Cocoa, SDL2, raylib |
| Linux x86_64 | **Working** — static ELF binaries, ANSI terminal rendering |
| Windows x86_64 | Planned — codegen + Win32 platform layer |
| WebAssembly | Future — codegen + Canvas API |

stb is 100% pure B++ (no platform code). Each target only needs a codegen backend + platform layer. The game code and stb library stay identical across all platforms.

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
bpp --c source.bpp              # emit C (for debugging)
bpp --asm source.bpp            # emit ARM64 assembly
```

17 modules, ~11,000 lines of B++. Self-hosting verified at every stage. Import search paths: `./`, `stb/`, `drivers/`, `src/`, `/usr/local/lib/bpp/`.

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

*Designed and built by Daniel Obino. Compiler bootstrapped March 20, 2026.*
