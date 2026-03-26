# B++

## A Language for Games

B++ is a small, low-level language for writing games. It has no type
declarations, no header files, and no standard C library dependency.
The compiler infers types automatically. You write simple code; the
compiler does the rest.

B++ descends from B, the language Ken Thompson wrote for early Unix.
Like B, every value is a machine word. Unlike B, the word is 64 bits,
structs have named fields, the compiler emits native ARM64 binaries
directly, and the whole system is self-hosting — the compiler compiles
itself.

## Getting Started

Install the compiler and standard library:

    sudo cp bpp /usr/local/bin/
    sudo mkdir -p /usr/local/lib/bpp/stb
    sudo cp stb/*.bsm /usr/local/lib/bpp/stb/

Compile and run a program:

    bpp hello.bpp -o hello
    ./hello

A game with no external dependencies:

    bpp examples/snake_native.bpp -o snake
    ./snake

The same game with SDL2:

    bpp examples/snake_sdl.bpp -o snake
    ./snake

The compiler produces a signed Mach-O ARM64 binary. No assembler, no
linker, no external tools.

## A First Program

    main() {
        putchar('H');
        putchar('i');
        putchar('\n');
        return 0;
    }

This prints `Hi` followed by a newline. `putchar` writes one byte to
standard output. There are no format strings and no `printf`. You build
what you need from bytes.

## Variables

All values are 64-bit integers. There are no booleans, no characters as
a distinct type. A variable is declared with `auto`:

    auto x, y, z;

Variables declared outside any function are global. Variables declared
inside a function are local to that function.

    auto counter;

    tick() {
        counter = counter + 1;
        return counter;
    }

There is no initialization syntax. Variables start at zero.

## Functions

A function takes zero or more parameters and returns a value.
Parameters and return values are all 64-bit integers.

    square(x) {
        return x * x;
    }

    max(a, b) {
        if (a > b) { return a; }
        return b;
    }

Every function must return a value. If you have nothing useful to
return, return zero.

Functions may be called before they are defined. A forward declaration
is a stub body that will be overwritten by the real definition later:

    process(x) { return 0; }

    // ... other code that calls process() ...

    process(x) {
        // real implementation
        return x * 2;
    }

The last definition wins.

## Function Pointers

Store a function's address and call it indirectly:

    add(a, b) { return a + b; }
    mul(a, b) { return a * b; }

    apply(fp, x, y) {
        return call(fp, x, y);
    }

    main() {
        auto r;
        r = apply(fn_ptr(add), 3, 4);    // r = 7
        r = apply(fn_ptr(mul), 3, 4);    // r = 12
        return 0;
    }

`fn_ptr(name)` returns the code address of a function. `call(fp, args...)`
calls the function at that address. Always pass the function pointer as the
first argument to `call`.

## Control Flow

There are three control structures: `if`, `while`, and `break`.

    if (x > 0) {
        y = x;
    } else {
        y = 0 - x;
    }

    while (i < n) {
        if (i == 5) { break; }
        sum = sum + i;
        i = i + 1;
    }

The braces are required when the body has more than one statement.
A single-statement body may omit them:

    if (x < 0) x = 0 - x;

`break` exits the innermost `while` loop. Nested loops are supported.

There is no `for`, no `switch`, no `do-while`. You will not miss them.

## Operators

Binary operators, highest precedence first:

    *  /  %        multiply, divide, remainder
    +  -           add, subtract
    <<  >>         left shift, right shift
    <  >  <=  >=   relational
    ==  !=         equality
    &              bitwise and
    ^              bitwise xor
    |              bitwise or
    &&             logical and
    ||             logical or

Unary operators:

    -              negate
    ~              bitwise not
    *              dereference (memory load)

Assignment:

    =              assign
    +=  -=  *=  /=  %=    compound assign

## Strings

String literals are written in double quotes:

    "hello, world"

A string literal is a pointer to a null-terminated byte sequence.
The function `str_peek(s, i)` reads byte `i` from string `s`.

## Character Literals

Single-character constants use single quotes:

    'A'        // 65
    '\n'       // 10 (newline)
    '\t'       // 9 (tab)
    '\\'       // 92 (backslash)
    '\''       // 39 (single quote)

## Numbers

Decimal and hexadecimal:

    42
    0xFF00
    16777216

All numbers are 64-bit signed integers.

## Floats

Floating-point literals contain a decimal point:

    3.14
    0.5
    100.0

The compiler infers float type and uses ARM64 double-precision registers.
Float and integer values convert automatically in mixed expressions.

## Type Hints

Variables and parameters can be annotated with a type hint for sub-word storage:

    auto x: byte;        // 8-bit (0-255)
    auto y: quarter;     // 16-bit (0-65535)
    auto z: half;        // 32-bit
    auto w: int;         // 64-bit (default, explicit)
    auto f: float;       // 64-bit double

    update(dt: float) {
        pos = pos + speed * dt;
        return 0;
    }

Type hints are opt-in performance tuning. The compiler uses narrower
load/store instructions (ldrb/strb on ARM64, movzx on x86_64) and the
C emitter maps to the corresponding C type. Without a hint, all values
are 64-bit.

Sub-word values truncate on store: `auto x: byte; x = 300;` gives 44
(300 & 0xFF). This is hardware truncation, not a runtime check.

## Comments

    // This is a line comment.

## Memory

`malloc(n)` allocates `n` bytes and returns a real pointer.
`free(ptr)` releases memory. `realloc(ptr, n)` resizes a block.

`peek(addr)` reads one byte at address `addr`.
`poke(addr, val)` writes one byte.

The dereference operator `*p` reads/writes 8 bytes (one word).
Offsets are in bytes. Each word is 8 bytes:

    auto buf;
    buf = malloc(80);
    *(buf + 0) = 100;     // store word at byte offset 0
    *(buf + 8) = 200;     // store word at byte offset 8 (second word)
    auto x;
    x = *(buf + 0);       // load: x is now 100

## Structs

A struct is a named group of 8-byte fields:

    struct Vec2 { x, y }

Heap-allocated structs use `auto` with a type annotation:

    auto p: Vec2;
    p = malloc(sizeof(Vec2));
    p.x = 100;
    p.y = 200;

Stack-allocated structs use `var` — no malloc needed:

    var v: Vec2;
    v.x = 100;
    v.y = 200;

`sizeof(Vec2)` is a compile-time constant (fields times 8).
Struct access is sugar for pointer arithmetic: `p.x` is `*(p + 0)`,
`p.y` is `*(p + 8)`.

## Enums

Named integer constants resolved at compile time:

    enum Dir { UP, DOWN, LEFT, RIGHT }
    enum Layer { GROUND = 1, PLAYER = 2, ENEMY = 4, BULLET = 8 }

    auto d;
    d = RIGHT;           // d = 3
    if (PLAYER & ENEMY) { ... }

Sequential values start at 0 by default. Explicit values with `= N`.
Enums are pure sugar — the parser replaces names with integer literals.

## Imports

### File imports

    import "stbgame.bsm";

This inlines the contents of the file at the import site. The compiler
searches relative to the importing file, then in the `stb/` directory.
Circular imports are detected and skipped. Module files use `.bsm`
(B-Single-Module) extension.

### FFI imports

    import "raylib" {
        void InitWindow(int, int, char*);
        void SetTargetFPS(int);
        int WindowShouldClose();
    }

This declares external functions from a dynamic library. In native mode,
the compiler creates GOT entries and links with the dylib at load time.
The library name maps to `/opt/homebrew/lib/lib{name}.dylib`.

## Built-in Functions

These names are recognized by the compiler and emit special code:

| Function | Description |
|----------|-------------|
| `malloc(n)` | Allocate n bytes, return pointer |
| `free(ptr)` | Release memory |
| `realloc(ptr, n)` | Resize memory block |
| `peek(addr)` | Read one byte |
| `poke(addr, val)` | Write one byte |
| `putchar(ch)` | Write one byte to stdout |
| `getchar()` | Read one byte from stdin, -1 on EOF |
| `str_peek(s, i)` | Read byte i from string s |
| `fn_ptr(name)` | Get code address of a function |
| `call(fp, args...)` | Indirect function call through pointer |
| `argc_get()` | Number of command-line arguments |
| `argv_get(i)` | Argument i as a pointer |
| `sys_write(fd, buf, len)` | Raw write syscall |
| `sys_read(fd, buf, len)` | Raw read syscall |
| `sys_open(path, flags)` | Raw open syscall |
| `sys_close(fd)` | Raw close syscall |
| `sys_fork()` | Fork process |
| `sys_execve(path, argv, envp)` | Replace process image |
| `sys_waitpid(pid)` | Wait for child process |
| `sys_exit(code)` | Terminate process |

I/O is implemented via raw syscalls (ARM64 `svc #0x80` on macOS, `syscall` on Linux x86_64). There is
no libc dependency.

## The Standard B Library

The `stb/` directory contains library modules. All are pure B++ with
zero external dependencies.

### stbarray — Dynamic Arrays

    import "stbarray.bsm";
    init_array();                  // required if using stbarray standalone

    auto a;
    a = arr_new();
    a = arr_push(a, 10);       // ALWAYS reassign (pointer may move)
    a = arr_push(a, 20);
    a = arr_push(a, 30);
    arr_get(a, 1);             // 20
    arr_len(a);                // 3
    arr_pop(a);                // 30
    a = arr_free(a);           // returns 0

Uses the shadow data technique (stb_ds.h): length and capacity are
hidden before the pointer. The user sees a plain pointer.

| Function | Description |
|----------|-------------|
| `arr_new()` | Create empty array |
| `arr_push(arr, val)` | Append, return (possibly moved) pointer |
| `arr_pop(arr)` | Remove and return last element |
| `arr_get(arr, i)` | Read element at index |
| `arr_set(arr, i, val)` | Write element at index |
| `arr_len(arr)` | Number of elements |
| `arr_last(arr)` | Last element |
| `arr_clear(arr)` | Reset length to zero |
| `arr_free(arr)` | Free memory, return 0 |

### stbstr — Strings

Static functions work on any null-terminated string:

| Function | Description |
|----------|-------------|
| `str_len(s)` | Length |
| `str_eq(a, b)` | 1 if equal |
| `str_cpy(dst, src)` | Copy |
| `str_chr(s, c)` | Find character, -1 if absent |
| `str_starts(s, prefix)` | 1 if prefix matches |
| `str_dup(s)` | Heap-allocated copy |
| `str_from_int(val, buf)` | Integer to string |

Dynamic string builder (growable, null-terminated):

    import "stbstr.bsm";
    init_str();                    // required if using stbstr standalone

    auto s;
    s = sb_new();
    s = sb_cat(s, "Score: ");
    s = sb_int(s, 42);
    print_msg(s);              // "Score: 42"
    s = sb_free(s);

| Function | Description |
|----------|-------------|
| `sb_new()` | Create empty string buffer |
| `sb_cat(sb, str)` | Append string, return pointer |
| `sb_ch(sb, char)` | Append character |
| `sb_int(sb, val)` | Append integer as text |
| `sb_len(sb)` | Length |
| `sb_clear(sb)` | Reset to empty |
| `sb_free(sb)` | Free, return 0 |

The string buffer IS a valid C string — pass it to any function
expecting `char*`.

### stbmath — Math

| Function | Description |
|----------|-------------|
| `vec2_new(x, y)` | Allocate Vec2 |
| `randi(limit)` | Random integer [0, limit) |
| `rng_seed(s)` | Seed the PRNG |
| `iabs(n)` | Absolute value |
| `min(a, b)`, `max(a, b)` | Minimum, maximum |
| `clamp(val, lo, hi)` | Clamp to range |
| `vec2_eq(a, b)` | 1 if equal |

### stbio — Console I/O

| Function | Description |
|----------|-------------|
| `print_char(c)` | Write one character |
| `print_int(n)` | Write decimal integer |
| `print_ln()` | Write newline |
| `print_msg(s)` | Write string + newline |

### stbdraw — Drawing

All drawing operates on a software framebuffer in memory. No external
graphics library needed.

| Function | Description |
|----------|-------------|
| `draw_rect(x, y, w, h, color)` | Filled rectangle |
| `draw_circle(cx, cy, r, color)` | Filled circle |
| `draw_line(x1, y1, x2, y2, color)` | Line (Bresenham) |
| `draw_text(text, x, y, sz, color)` | Text (8x8 bitmap font) |
| `draw_number(x, y, sz, color, val)` | Integer as text |
| `draw_sprite(data, x, y, w, h)` | RGBA sprite with transparency |
| `draw_sprite16(spr, pal, x, y)` | 16x16 palette-indexed sprite |
| `draw_bar(x, y, w, h, val, max, color)` | Progress bar |
| `measure_text(text, sz)` | Text width in pixels |
| `clear(color)` | Clear framebuffer |
| `put_px(x, y, color)` | Single pixel |
| `draw_end()` | Present framebuffer to screen |
| `rgba(r, g, b, a)` | Pack color components |

Colors: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `ORANGE`,
`PURPLE`, `GRAY`, `DARKGRAY`.

### stbinput — Keyboard and Mouse

All input reads from memory arrays. No external input library needed.

| Function | Description |
|----------|-------------|
| `key_down(k)` | 1 if key is held |
| `key_pressed(k)` | 1 if key was just pressed |
| `mouse_x()`, `mouse_y()` | Cursor position |
| `mouse_down(btn)` | 1 if button held |
| `point_in_rect(px,py,x,y,w,h)` | Hit test |

Keys (enum): `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_W`,
`KEY_A`, `KEY_S`, `KEY_D`, `KEY_SPACE`, `KEY_ENTER`, `KEY_ESC`, `KEY_TAB`.

### stbcol — Collision Detection

| Function | Description |
|----------|-------------|
| `rect_overlap(x1,y1,w1,h1,x2,y2,w2,h2)` | AABB rectangle overlap |
| `circle_overlap(x1,y1,r1,x2,y2,r2)` | Circle overlap (no sqrt) |
| `rect_contains(rx,ry,rw,rh,px,py)` | Point inside rectangle |
| `circle_contains(cx,cy,r,px,py)` | Point inside circle |

### stbbuf — Raw Buffer Access

| Function | Description |
|----------|-------------|
| `read_u8/u16/u32/u64(buf, off)` | Read N-bit value at byte offset |
| `write_u8/u16/u32/u64(buf, off, val)` | Write N-bit value at byte offset |

### stbfile — File I/O

| Function | Description |
|----------|-------------|
| `file_read_all(path, size_out)` | Read entire file, return buffer pointer |

### stbfont — 8x8 Bitmap Font

Provides glyph data for ASCII 32-127. Used internally by `draw_text`.

### stbgame — Game Loop

    import "stbgame.bsm";

    main() {
        game_init(320, 180, "My Game", 60);
        while (game_should_quit() == 0) {
            game_frame_begin();
            if (key_pressed(KEY_ESC)) { break; }
            clear(BLACK);
            draw_text("Hello", 100, 80, 2, WHITE);
            draw_end();
        }
        return 0;
    }

The game runs natively on macOS — no SDL, no raylib, no OpenGL, no
Metal. The compiler's platform layer opens a window via `objc_msgSend`
(Cocoa), renders the framebuffer via `CGBitmapContext` (CoreGraphics),
and reads keyboard input via `NSEvent`. This is fully implemented and
working as of March 24, 2026.

`game_init` calls all subsystem inits automatically (stbmath, stbinput,
stbdraw, stbfont, stbui). The dev never needs to call `init_array()`,
`init_str()`, etc. manually when using stbgame.

| Function | Description |
|----------|-------------|
| `game_init(w, h, title, fps)` | Create window, init all subsystems |
| `game_frame_begin()` | Poll input, calculate delta time |
| `game_dt()` | Delta time in seconds (float) |
| `game_dt_ms()` | Delta time in milliseconds (integer) |
| `game_should_quit()` | 1 if window closed |

### stbui — Immediate-Mode UI

| Function | Description |
|----------|-------------|
| `gui_label(x, y, text)` | Text label |
| `gui_number(x, y, val)` | Number display |
| `gui_panel(x, y, w, h, color)` | Background panel |
| `gui_button(x, y, w, h, label)` | Button (returns 1 on click) |
| `gui_bar(x, y, w, h, val, max, color)` | Progress bar |
| `style_new(bg, fg, font_sz, pad)` | Create style |

### Backend Drivers (Optional)

By default, stb runs natively on macOS using Cocoa/CoreGraphics. The
developer can override the native platform by importing a driver:

    import "stbgame.bsm";
    import "drv_sdl.bsm";      // use SDL2 instead of native
    // or
    import "drv_raylib.bsm";   // use raylib instead of native

The game code stays identical — only the import changes. Drivers live
in the `drivers/` directory and implement the `_stb_*` platform
functions using the external library's FFI.

| Driver | Library | Present Method |
|--------|---------|---------------|
| Native (default) | Cocoa + CoreGraphics | CGBitmapContext → NSImageView |
| `drv_sdl.bsm` | SDL2 | SDL_UpdateTexture |
| `drv_raylib.bsm` | raylib | DrawRectangle per pixel |

## The Compiler

The B++ compiler is written in B++ and compiles itself. It produces
native ARM64 Mach-O binaries directly, with built-in ad-hoc code
signing (SHA-256).

    bpp source.bpp -o binary     # native ARM64 binary (modular, default)
    bpp --linux64 source.bpp -o binary  # cross-compile to Linux x86_64 ELF
    bpp --c source.bpp > out.c   # emit C (monolithic, for bootstrap/debug)
    bpp --asm source.bpp > out.s # emit ARM64 assembly (monolithic)
    bpp --monolithic source.bpp -o out  # force single-pass compilation
    bpp --show-deps source.bpp          # print module dependency graph

Source lives in `src/`. The compiler has 18 modules:

| Module | Purpose |
|--------|---------|
| `bpp.bpp` | Main driver, argument parsing |
| `defs.bsm` | Constants, struct defs, pack/unpack, field offset names |
| `bpp_internal.bsm` | List builder, string helpers |
| `bpp_import.bsm` | File import resolver, module hashing, dependency graph |
| `bpp_lexer.bsm` | Tokenizer |
| `bpp_parser.bsm` | Parser (tokens → AST), module ownership tracking |
| `bpp_types.bsm` | Type inference (body-local, no cross-function propagation) |
| `bpp_dispatch.bsm` | Loop classification |
| `bpp_validate.bsm` | Semantic validation pass |
| `bpp_emitter.bsm` | C code emitter |
| `bpp_diag.bsm` | Diagnostic output to stderr |
| `bpp_bo.bsm` | .bo cache file I/O for modular compilation |
| `aarch64/a64_enc.bsm` | ARM64 instruction encoder |
| `aarch64/a64_codegen.bsm` | ARM64 code generator |
| `aarch64/a64_macho.bsm` | Mach-O writer + SHA-256 codesign |
| `x86_64/x64_enc.bsm` | x86_64 instruction encoder |
| `x86_64/x64_codegen.bsm` | x86_64 code generator |
| `x86_64/x64_elf.bsm` | ELF writer (Linux static binaries) |

### Self-Hosting

The fixed-point property holds: the compiler compiled by itself produces
identical output to the compiler compiled by the previous version.

To rebuild the compiler:

    bpp src/bpp.bpp -o bpp

### Modular Compilation

Native backends (ARM64 and x86_64) use per-module compilation by
default, inspired by Go's build model. Each module compiles to a `.bo`
(B++ Object) cache file containing export data and machine code. On
rebuild, unchanged modules load from cache.

    bpp src/bpp.bpp -o bpp

The compiler hashes each module's content and tracks dependencies. When
a module changes, it and all its dependents are recompiled. Unchanged
modules are loaded from `.bpp_cache/` in milliseconds.

The C emitter (`--c`) and assembly output (`--asm`) use the monolithic
pipeline (single-pass, no caching). Use `--monolithic` to force this
mode for native backends. Future backends (WASM) will also start
monolithic until their encoders support per-module output.

Cross-module function calls use type 4 relocations (BL/CALL placeholders)
resolved after all modules are loaded.

---

*B++ was designed and implemented by Daniel Obino, 2026.*
*The compiler bootstrapped itself on March 20, 2026.*
*Zero-dependency native compilation achieved on March 23, 2026.*
*Native game rendering (Cocoa, no SDL/raylib) achieved on March 24, 2026.*
*Compiler diagnostics (E001-E201, W001) completed on March 24, 2026.*
*Type propagation bug fixed, x86_64/ELF backend started on March 25, 2026.*
*Modular compilation (.bo cache, Go model) implemented on March 26, 2026.*
*Dynamic array migration and type hints completed on March 26, 2026.*
