# B++

## A Language for Games

B++ is a small, low-level language for writing games. There are no type
declarations, no header files, and no dependency on the standard C
library. The compiler infers types and emits native machine code
directly — no assembler, no linker, no external tools.

B++ descends from B, the language Ken Thompson wrote for early Unix.
Every value is a machine word, just as it was in B. The word is now
64 bits. Structs carry named fields. The compiler is written in B++
and compiles itself.

The system is small enough to read in an afternoon and large enough
to ship a game.

## Getting Started

The shipped repo contains the compiler binary `bpp` already built. To
install it system-wide along with the standard library:

    sh install.sh

`install.sh` bootstraps the compiler from source, then copies `bpp`
and `bug` into `/usr/local/bin/` and the `stb/` library into
`/usr/local/lib/bpp/`. The bootstrap step verifies the compiler can
build itself before installing — see `bootprod_manual.md` for the
full bootstrap discipline.

If `bpp` is missing or unusable, recover it from the C source seed:

    clang bootstrap.c -o bpp

`bootstrap.c` is the compiler emitted as C99. Regenerate it any time
with `bpp --c src/bpp.bpp > bootstrap.c`.

Compile and run a program:

    bpp hello.bpp -o hello
    ./hello

A game with no external dependencies:

    bpp games/snake/snake_gpu.bpp -o snake
    ./snake

The compiler produces a signed Mach-O ARM64 binary on macOS or a
static x86_64 ELF binary on Linux (via `--linux64` from macOS, native
on Linux once host detection lands).

### Cross-Compiling to Linux

Pass `--linux64` to produce a static x86_64 ELF binary:

    bpp --linux64 examples/snake_cpu.bpp -o snake_linux

The Linux build uses the X11 wire protocol (Phases 1-3 implemented:
window, software framebuffer via XPutImage, keyboard + mouse) when
`DISPLAY` is set, and falls back to ANSI terminal rendering when it
is not. To run the binary inside a Docker container with the window
showing on a macOS host:

    # One-time XQuartz setup on the macOS host
    brew install --cask xquartz
    defaults write org.xquartz.X11 nolisten_tcp 0
    killall XQuartz 2>/dev/null
    open -a XQuartz
    xhost +localhost

    # Run the cross-compiled binary
    docker run --rm \
      --add-host host.docker.internal:host-gateway \
      -e DISPLAY=host.docker.internal:0 \
      -v /tmp:/tmp \
      ubuntu:22.04 \
      /tmp/snake_linux

Currently only `draw_*` (CPU framebuffer) games work via X11. GPU games
(`render_*`) need Vulkan, which is deferred (see `docs/todo.md`).

### Portable C Output (raylib / SDL drivers)

The C emitter (`--c`) translates B++ to portable C99 source. The right
pattern is to write the game with `drv_raylib.bsm` (or `drv_sdl.bsm`)
instead of `stbgame.bsm`, so the generated C calls regular C functions:

    bpp --c examples/snake_raylib.bpp > snake.c
    gcc -I/opt/homebrew/include -L/opt/homebrew/lib \
        snake.c -o snake -lraylib -lobjc
    ./snake

The same `snake.c` should compile on any platform with raylib installed
(macOS, Linux, Windows, BSDs, Emscripten). Avoid mixing `stbgame.bsm`
with `--c`: `stbgame` calls Cocoa via `objc_msgSend`, which has a
calling-convention pitfall when reached through C — see `bootstrap_manual.md`.

The driver model is the bridge between the B++ engine API and external
C libraries. Each driver implements the same `_stb_*` interface that the
native platform layer provides:

| Driver         | Library | Use case |
|----------------|---------|----------|
| `drv_raylib.bsm` | raylib  | Portable C output via the C emitter |
| `drv_sdl.bsm`    | SDL2    | Same, when SDL is preferred over raylib |

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

Four control structures: `if`, `else if`, `while`, `for`, and `break`.

    if (x > 0) {
        y = x;
    } else if (x == 0) {
        y = 1;
    } else {
        y = 0 - x;
    }

    while (i < n) {
        if (i == 5) { break; }
        sum = sum + i;
        i = i + 1;
    }

    for (i = 0; i < 10; i = i + 1) {
        putchar('0' + i);
    }

The braces are required when the body has more than one statement.
A single-statement body may omit them:

    if (x < 0) x = 0 - x;

`else if` chains without extra braces. `for` desugars to `while` internally.
`break` exits the innermost loop. There is no `switch`, no `do-while`.

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
    &              address-of (get memory address of a variable)

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

## Type System

B++ uses an orthogonal type system with two axes: **base type** (what it is)
and **slice** (how wide). The compiler infers the base type. The programmer
optionally specifies the slice for performance tuning.

### Base types

    word       64-bit integer (the default — every value starts as a word)
    float      64-bit double (inferred when a value contains '.' or comes from float math)
    ptr        64-bit pointer (inferred from string literals, malloc, etc.)
    struct     composite type (defined with the struct keyword)

### Slices

    (none)     full width — 64 bits (default)
    half       half width — 32 bits
    quarter    quarter width — 16 bits
    byte       eighth width — 8 bits

### Combining base and slice

The programmer writes the slice. The compiler figures out the base:

    auto x;                  // word, full (64-bit int) — inferred
    auto x: byte;            // word, byte (8-bit int)
    auto x: half;            // word, half (32-bit int)
    auto x: float;           // float, full (64-bit double)
    auto x: half float;      // float, half (32-bit float)
    auto x: quarter float;   // float, quarter (16-bit float)

Per-variable hints in a single declaration:

    auto r: byte, g: byte, b: byte;

Parameters:

    update(dt: float) {
        pos = pos + speed * dt;
        return 0;
    }

Sub-word types also work as standalone declaration keywords:

    byte hp;
    half score;
    quarter tile_id;

### How it works

Internally, each type is packed into a single byte with an optimized encoding:
- Bit 0 = float flag (1 = float, 0 = not float)
- Bits 0-3 = base type (WORD=0x02, FLOAT=0x03, PTR=0x04, STRUCT=0x08, UNK=0x0C)
- Bits 4-5 = slice (FULL=0, HALF=1, QUARTER=2, BYTE=3)

`is_float_type(ty)` reduces to `ty & 1` — a single instruction on ARM64 (TBNZ).
`ty_base()`, `ty_slice()`, and `ty_make()` compose and decompose types.

The compiler uses narrower load/store instructions for sub-word types:
`ldrb`/`strb` for byte, `ldrh`/`strh` for quarter, `ldr w`/`str w` for half (integer),
`ldr s`/`str s` + FCVT for half float (single-precision),
`ldr h`/`str h` + FCVT for quarter float (half-precision, ARM64 FEAT_FP16).
The C emitter maps to `uint8_t`, `uint16_t`, `uint32_t`, `float`, `double`.

Without a hint, all values are 64-bit. Sub-word values truncate on store:
`auto x: byte; x = 300;` gives 44 (300 & 0xFF). This is hardware truncation,
not a runtime check.

### Promotion rules

When mixing types in expressions, the compiler promotes to the wider type:
- PTR wins everything (pointer arithmetic)
- FLOAT wins integers (int converts to float)
- Float widens to at least the integer's width: f32 + i64 = f64 (no precision loss)
- Same base: wider slice wins (byte + half = half)

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

### Address-of

`&x` returns the memory address of variable `x` on the stack (or in the
data section for globals). This is the inverse of dereference: `*(&x) == x`.

Use it to pass out-parameters to functions without allocating memory:

    set_value(dest, val) { *(dest) = val; return 0; }

    main() {
        auto x;
        x = 0;
        set_value(&x, 42);  // x is now 42
        return x;
    }

**Note:** `&` does not work with extern FFI functions or `call()` (function
pointer calls). The compiler emits warning W012 for this case. Use
`malloc(8)` as a workaround for FFI out-parameters.

### Array indexing

`buf[i]` is sugar for `*(buf + i * 8)` — read or write the i-th word:

    auto buf;
    buf = malloc(40);
    buf[0] = 10;          // same as *(buf + 0) = 10
    buf[1] = 20;          // same as *(buf + 8) = 20
    x = buf[0] + buf[1];  // x is 30

    for (i = 0; i < 5; i = i + 1) {
        buf[i] = i * 100;
    }

This works with any pointer: `malloc` buffers, `arr_push` arrays,
function parameter arrays, struct field arrays. The index is always
in words (8 bytes), not bytes.

## Structs

A struct is a named group of fields. By default each field is 8 bytes:

    struct Vec2 { x, y }           // 16 bytes (2 x 8)

Fields can have type hints for packed layout:

    struct Pixel { r: byte, g: byte, b: byte, a: byte }   // 4 bytes (4 x 1)
    struct Tile  { id: quarter, flags: byte }              // 3 bytes (2 + 1)

Supported field hints: `byte` (1 byte), `quarter` (2 bytes), `half` (4 bytes),
`half float` (4 bytes, f32), `quarter float` (2 bytes, f16). No hint = 8 bytes.

Heap-allocated structs use `auto` with a type annotation:

    auto p: Pixel;
    p = malloc(sizeof(Pixel));     // 4 bytes
    p.r = 255;                     // STRB — 1 byte write
    p.g = 128;

Stack-allocated structs use `var` — no malloc needed:

    var v: Vec2;
    v.x = 100;
    v.y = 200;

`sizeof(Pixel)` is a compile-time constant (sum of field sizes).
Struct access is sugar for pointer arithmetic with the correct load/store size:
`p.r` on a `: byte` field emits LDRB/STRB (1 byte), not LDR/STR (8 bytes).

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
| `realloc(ptr, n)` | Resize memory block (2-arg: just allocate) |
| `realloc(ptr, old, new)` | Resize with copy (3-arg: allocate + copy old bytes) |
| `memcpy(dst, src, len)` | Copy len bytes from src to dst, return dst |
| `peek(addr)` | Read one byte |
| `poke(addr, val)` | Write one byte |
| `putchar(ch)` | Write one byte to stdout |
| `putchar_err(ch)` | Write one byte to stderr |
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
| `sys_fork()` | Fork process (macOS: returns 0 in child) |
| `sys_execve(path, argv, envp)` | Replace process image |
| `sys_waitpid(pid)` | Wait for child process (1 arg, status=NULL) |
| `sys_wait4(pid, status, opts, rusage)` | Wait with full control (4 args) |
| `sys_exit(code)` | Terminate process |
| `sys_ioctl(fd, req, arg)` | Device control |
| `sys_ptrace(req, pid, addr, data)` | Process trace (debug) |
| `sys_getdents(fd, buf, size)` | Read directory entries |
| `sys_unlink(path)` | Delete a file |
| `sys_mkdir(path, mode)` | Create a directory |
| `sys_socket(domain, type, proto)` | Create a TCP/UDP socket |
| `sys_connect(fd, addr, len)` | Connect a socket to an address |
| `sys_usleep(microseconds)` | Sleep for N microseconds |
| `sys_nanosleep(req, rem)` | Sleep (Linux) |
| `sys_clock_gettime(id, tp)` | Clock read (Linux) |
| `float_ret()` | Read saved first float return register (d0/xmm0) from last extern call |
| `float_ret2()` | Read saved second float return register (d1/xmm1) from last extern call |
| `shr(value, shift)` | Logical right shift (unsigned, fills with zeros). Unlike `>>` which sign-extends |
| `assert(cond)` | Trap (BRK/INT3) if condition is zero. Zero overhead when true |

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
| `math_cos(i)` | cos(i * 11.25) * 1024 (fixed-point, i=0..32) |
| `math_sin(i)` | sin(i * 11.25) * 1024 (fixed-point, i=0..32) |

### stbio — Console I/O

| Function | Description |
|----------|-------------|
| `print_char(c)` | Write one character |
| `print_int(n)` | Write decimal integer |
| `print_ln()` | Write newline |
| `print_msg(s)` | Write string + newline |

### stbdraw — CPU Drawing

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
| `blend_px(x, y, color)` | Alpha-blended pixel (ARGB, 255=opaque) |
| `draw_end()` | Present framebuffer to screen |
| `rgba(r, g, b, a)` | Pack color components |

Colors: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `ORANGE`,
`PURPLE`, `GRAY`, `DARKGRAY`.

### stbrender — GPU Drawing

GPU-accelerated 2D rendering. Uses Metal on macOS, Vulkan on Linux
(planned). Same color constants as stbdraw. Import `stbrender.bsm`
alongside `stbgame.bsm`.

| Function | Description |
|----------|-------------|
| `render_init()` | Initialize GPU (call once after game_init) |
| `render_begin()` | Start GPU frame |
| `render_clear(color)` | Set background clear color |
| `render_rect(x, y, w, h, color)` | Filled rectangle |
| `render_line(x0, y0, x1, y1, color)` | Line |
| `render_circle(cx, cy, r, color)` | Filled circle (32 segments) |
| `render_circle_outline(cx, cy, r, color)` | Circle outline |
| `render_rect_outline(x, y, w, h, color)` | Rectangle outline |
| `render_end()` | Flush, present, poll input |

The GPU backend batches vertices (8 bytes each: int16 x,y + uint8
r,g,b,a) and draws them as triangles. The shader converts integer
coordinates to clip space on the GPU. No float math needed from B++.

### stbinput — Keyboard and Mouse

All input reads from memory arrays. No external input library needed.

| Function | Description |
|----------|-------------|
| `key_down(k)` | 1 if key is held |
| `key_pressed(k)` | 1 if key was just pressed (edge-triggered) |
| `mouse_x()`, `mouse_y()` | Cursor position in framebuffer coordinates |
| `mouse_down(btn)` | 1 if button held |
| `mouse_pressed(btn)` | 1 if button just pressed (edge-triggered) |
| `mouse_released(btn)` | 1 if button just released (edge-triggered) |
| `point_in_rect(px,py,x,y,w,h)` | Hit test |

Keys (enum): `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_W`,
`KEY_A`, `KEY_S`, `KEY_D`, `KEY_SPACE`, `KEY_ENTER`, `KEY_ESC`, `KEY_TAB`,
`KEY_SHIFT`, `KEY_E`, `KEY_R`, `KEY_Z`, `KEY_X`, `KEY_G`, `KEY_C`,
`KEY_N`, `KEY_T`, `KEY_0`-`KEY_9`, `KEY_PERIOD`, `KEY_BACKSPACE`.

Mouse buttons (enum): `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`.

### stbcol — Collision Detection

| Function | Description |
|----------|-------------|
| `rect_overlap(x1,y1,w1,h1,x2,y2,w2,h2)` | AABB rectangle overlap |
| `circle_overlap(x1,y1,r1,x2,y2,r2)` | Circle overlap (no sqrt) |
| `rect_contains(rx,ry,rw,rh,px,py)` | Point inside rectangle |
| `circle_contains(cx,cy,r,px,py)` | Point inside circle |

### stbcolor — Color Constants

| Function / Constant | Description |
|----------------------|-------------|
| `rgba(r, g, b, a)` | Pack four bytes into an ARGB word |
| `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `ORANGE`, `PURPLE`, `GRAY`, `DARKGRAY` | Pre-built colors |

### stbtile — Tilemap Engine

A grid of tile types with a separate solid mask, an optional textured
tileset, and a remap from game-defined types to tileset indices.

| Function | Description |
|----------|-------------|
| `tile_new(w, h, tw, th)` | Allocate a tilemap |
| `tile_get/tile_set(tm, gx, gy, val)` | Read/write a single tile |
| `tile_solid(tm, type)` | Mark a tile type as solid |
| `tile_collides(tm, px, py, pw, ph)` | AABB hit-test against solid tiles |
| `tile_load_set(path, tw, th, &out)` | Load a PNG tileset, one GPU texture per tile |
| `tile_map_type(tm, game_type, ts_idx)` | Remap a logical tile to a tileset index |
| `tile_draw(tm, cx, cy, sw, sh)` | GPU render with camera culling |

### stbphys — Platformer Physics

A `Body` struct with milli-pixel positions and pixel/sec velocities.
`pos += vel * dt_ms` works directly in integer math.

| Function | Description |
|----------|-------------|
| `phys_body(w, h, gravity, jump_vel, move_spd)` | Allocate a body |
| `phys_set_pos(b, px, py)` | Set position from pixel coordinates |
| `phys_jump(b)` | Apply jump impulse if grounded |
| `phys_update(b, dt_ms, tm)` | Gravity, X move, Y move with tile collision |
| `phys_px_x(b)` / `phys_px_y(b)` | Pixel-coordinate position for drawing |

### stbpath — A* Pathfinding

Grid-based A* with a binary min-heap and indexed decrease-key. Pure
algorithm, no graphics dependency.

| Function | Description |
|----------|-------------|
| `path_new(w, h)` | Allocate a PathFinder |
| `path_set_blocked(pf, gx, gy, blocked)` | Mark a cell |
| `path_find(pf, sx, sy, gx, gy, out, max)` | Run A*, write waypoints, return count |

The grid is dependency-free — you fill it with `path_set_blocked` from
any source. To bridge from a Tilemap, inline an 8-line loop calling
`tile_is_solid` (the snippet is documented in `stbpath.bsm`).

### stbhash — Hash Maps

Two flavors share the file: word keys and byte-sequence keys. Both use
open addressing with linear probing, tombstones, and 75%-load resize.

| Word-keyed | Byte-keyed |
|------------|------------|
| `hash_new(cap)` | `hash_str_new(cap)` |
| `hash_set(h, key, val)` | `hash_str_set(h, buf, len, val)` |
| `hash_get(h, key)` | `hash_str_get(h, buf, len)` |
| `hash_has(h, key)` | `hash_str_has(h, buf, len)` |
| `hash_del(h, key)` | `hash_str_del(h, buf, len)` |
| `hash_clear`/`hash_count`/`hash_free` | `hash_str_clear`/`hash_str_count`/`hash_str_free` |

The byte-keyed flavor copies the key bytes on insert, so the caller's
source buffer can be freed immediately. The compiler itself uses
`HashStr` for symbol-table lookups.

### stbarena — Bump Allocator

Frame-scoped allocation with O(1) reset.

| Function | Description |
|----------|-------------|
| `arena_new(size)` | Allocate an arena of `size` bytes |
| `arena_alloc(a, n)` | Carve `n` bytes off the bump pointer |
| `arena_reset(a)` | Reset the bump pointer to zero |
| `arena_free(a)` | Release the arena |

### stbpool — Object Pool

Fixed-size object pool with embedded freelist for O(1) get/put.

| Function | Description |
|----------|-------------|
| `pool_new(obj_size, capacity)` | Allocate a pool |
| `pool_get(p)` | Take a free object, returns pointer |
| `pool_put(p, obj)` | Return an object to the freelist |

### stbecs — Entity-Component System

Parallel-array ECS for game entities. Positions and velocities live in
milli-units (px×1000) for sub-pixel precision without floats.

| Function | Description |
|----------|-------------|
| `ecs_new(capacity)` | Allocate a world |
| `ecs_spawn(w)` | Allocate an entity, returns id |
| `ecs_kill(w, id)` / `ecs_alive(w, id)` | Lifecycle |
| `ecs_set_pos/vel/flags(w, id, ...)` | Write components |
| `ecs_get_x/y/flags(w, id)` | Read components |
| `ecs_physics(w, dt_ms)` | Step every live entity by `vel * dt_ms` |
| `ecs_capacity(w)` | Total slot count |

### stbsprite — GPU Sprites

Palette-indexed sprite loading and drawing on the GPU.

| Function | Description |
|----------|-------------|
| `load_sprite(path, out, w, h)` | Parse a JSON sprite into a byte buffer of palette indices |
| `sprite_create(spr, pal, w, h)` | Convert to RGBA and upload as a GPU texture |
| `sprite_draw(tex, x, y, w, h, scale)` | Draw at integer scale |

### stbimage — PNG Loader

Pure B++ PNG decoder. Supports DEFLATE, all five filter types,
palette-indexed images, and the tRNS chunk for palette transparency.

| Function | Description |
|----------|-------------|
| `img_load(path)` | Decode a PNG, returns RGBA pixel buffer |
| `img_w()` / `img_h()` | Width and height of the last load |
| `img_free(buf)` | Release pixel buffer |

### stbfont — Text Rendering

8×8 bitmap font for ASCII 32-127, plus a pure-B++ TrueType reader
(`cmap`, `glyf`, Bézier rasterization, scanline antialiasing). Used
internally by `draw_text` and `render_text`.

### stbbuf — Raw Buffer Access

| Function | Description |
|----------|-------------|
| `read_u8/u16/u32/u64(buf, off)` | Read N-bit value at byte offset |
| `write_u8/u16/u32/u64(buf, off, val)` | Write N-bit value at byte offset |

### stbfile — File I/O

| Function | Description |
|----------|-------------|
| `file_read_all(path, size_out)` | Read entire file, return buffer pointer |
| `file_write_all(path, buf, len)` | Write buffer to file, return 0 or -1 |

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

The game runs natively on the host. On macOS the platform layer opens
a window through `objc_msgSend` (Cocoa) and presents pixels via
`CGBitmapContext` (CoreGraphics) for `draw_*`, or via Metal for
`render_*`. On Linux the layer speaks the X11 wire protocol directly
over a Unix socket — no Xlib, no shared libraries.

`game_init` calls all subsystem inits automatically (stbmath, stbinput,
stbdraw, stbfont, stbui). The platform layer itself is injected by the
compiler when it sees that `stbgame` is in the import graph, so
`stbgame.bsm` no longer mentions any platform module by name.

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

Source lives in `src/`. The compiler is roughly 16 core modules plus
a per-target backend bundle in each chip folder.

**Core (`src/`):**

| Module | Purpose |
|--------|---------|
| `bpp.bpp` | Main driver, argument parsing |
| `defs.bsm` | Constants, struct field offsets, pack/unpack helpers |
| `bpp_internal.bsm` | List builder, string helpers (`buf_eq`, `packed_eq`) |
| `bpp_import.bsm` | Import resolver, module hashing, dependency graph, target-suffix fallback, platform auto-injection |
| `bpp_lexer.bsm` | Tokenizer |
| `bpp_parser.bsm` | Parser (tokens → AST), struct registry with hashed lookup |
| `bpp_types.bsm` | Type inference, function-table hash, cross-module propagation |
| `bpp_dispatch.bsm` | Loop classification |
| `bpp_validate.bsm` | Semantic validation, builtin recognition, symbol-table hashes |
| `bpp_emitter.bsm` | C code emitter, extern dedup, libc symbol skip |
| `bpp_diag.bsm` | Diagnostic output to stderr |
| `bpp_bo.bsm` | `.bo` cache file I/O for modular compilation |
| `bpp_bug.bsm` | `.bug` debug map writer |

**Per-target backend (`src/aarch64/`, `src/x86_64/`):**

Each chip folder is a complete target bundle: encoder + binary writer
+ syscall numbers + platform layer + bug observer for the OS currently
paired with that chip. See the `README.md` in each folder for the
chip+OS coupling note and the future split plan.

| File | Purpose |
|------|---------|
| `a64_enc.bsm`, `x64_enc.bsm` | Instruction encoder (chip pure) |
| `a64_macho.bsm`, `x64_elf.bsm` | Binary format writer (OS) |
| `a64_codegen.bsm`, `x64_codegen.bsm` | AST → machine code (chip + OS syscalls) |
| `_stb_platform_macos.bsm`, `_stb_platform_linux.bsm` | OS platform layer (window, GPU/X11, input) |
| `bug_observe_macos.bsm`, `bug_observe_linux.bsm` | OS process observation for the debugger |

## The Debugger

B++ includes `bug`, a native debugger that runs alongside the compiler.
It uses the GDB remote protocol to communicate with Apple's debugserver
(macOS) or gdbserver (Linux). No special flags or compilation steps needed.

### Basic usage

    bpp source.bpp -o program      # compile normally
    ./bug ./program                 # trace + crash report

### What it does automatically

- **Function tracing**: prints every function call with indented call depth
- **Crash report**: on SIGSEGV/SIGBUS/SIGABRT, shows signal + PC + function name
- **Backtrace**: walks the frame pointer chain to show the full call stack
- **Symbol table**: reads function names directly from the Mach-O/ELF binary

### Enhanced mode (with .bug file)

    bpp --bug source.bpp -o program   # compile with debug metadata
    ./bug ./program                    # trace + crash report + locals

When a `.bug` file is present, the debugger also:

- **Reads local variables** on crash (names + values from the stack frame)
- **Filters trace** to only user-defined functions (skips stb library internals)

### Architecture

    bug (TCP client) → debugserver/gdbserver (OS debug server) → target process

The bug binary needs no entitlements or codesign. The OS debug server handles
all privileged operations (ptrace, breakpoints, I-cache coherence).

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

*See `docs/journal.md` for the full development chronology.*
