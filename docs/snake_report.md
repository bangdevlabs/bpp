# Snake Report — From Prototype to Multi-Backend

**Original date:** 2026-03-19
**Updated:** 2026-03-24

## History

The snake game has been the proving ground for B++ at every stage:

| Date | Version | Pipeline |
|------|---------|----------|
| 2026-03-19 | snake_demo.bpp | Python compiler → C shim → clang + raylib |
| 2026-03-23 | snake_v3.bpp | Self-hosting B++ compiler → native ARM64 + raylib driver |
| 2026-03-24 | snake_native.bpp | Same compiler → native ARM64, **zero external dependencies** |
| 2026-03-24 | snake_sdl.bpp | Same compiler → native ARM64 + SDL2 driver |
| 2026-03-24 | snake_raylib.bpp | Same compiler → native ARM64 + raylib driver (new API) |

## What the Snake Proves Now

The same game code compiles to three different backends:

```bpp
// snake_native.bpp — zero dependencies:
import "stbgame.bsm";
import "_stb_platform.bsm";

// snake_sdl.bpp — SDL2:
import "stbgame.bsm";
import "drv_sdl.bsm";

// snake_raylib.bpp — raylib:
import "stbgame.bsm";
import "drv_raylib.bsm";
```

The game logic, drawing, input, and text rendering are identical across
all three. Only the import line changes. This proves that stb is a
complete, backend-agnostic game library.

## What Each Backend Does

| Backend | Window | Present | Input | Timing |
|---------|--------|---------|-------|--------|
| Native (Cocoa) | NSWindow via objc_msgSend | CGBitmapContext → NSImage → NSImageView | NSEvent keyCode | mach_absolute_time |
| SDL2 | SDL_CreateWindow | SDL_UpdateTexture (fast) | SDL_PollEvent | SDL_GetPerformanceCounter |
| Raylib | InitWindow (GLFW) | DrawRectangle per pixel | IsKeyDown polling | SetTargetFPS |

## stb Architecture

All drawing happens on a software framebuffer in memory:

```
stbdraw writes pixels → _stb_fb (malloc'd buffer)
                            ↓
_stb_present() copies to screen (backend-specific)
```

The stb modules (stbdraw, stbinput, stbgame, stbfont) are 100% pure B++.
They never import SDL, raylib, Cocoa, or any external library. The platform
layer handles OS communication.

## Language Features Used

The snake demonstrates these B++ features:

| Feature | Usage |
|---------|-------|
| `enum` | `enum Key { KEY_UP = 1, KEY_DOWN, ... }` |
| `break` | Exit game loop on ESC |
| `auto` / heap structs | Body segments as pointer array |
| String literals | Window title, game over text |
| `draw_text` | HUD, game over overlay, ranking |
| `draw_rect` | Snake body, apple, head |
| `key_down` / `key_pressed` | Movement, restart |
| `randi()` | Apple placement |
| `print_msg` / `print_int` | Console ranking output |

## Original Fat Struct Design (2026-03-19)

The original report proposed fat structs with named fields. This has been
implemented as two features:

- **`auto p: Vec2; p = malloc(sizeof(Vec2)); p.x = 100;`** — heap structs (done)
- **`var v: Vec2; v.x = 100;`** — stack structs, no malloc (done)

Both work in the current compiler. The snake uses simple integer globals
for positions (no struct needed for this scale) but the MCU game port uses
structs extensively.

## What's Next

- **stbtile.bsm** — Tilemap rendering for RPGs, platformers
- **stbaudio.bsm** — Sound effects and music
- **More backends** — Linux (X11), WebAssembly
- **Texture upload for raylib** — Replace per-pixel DrawRectangle with rlgl
