# The B++ Programming Language

*Daniel Obino — 2026*

---

## Preface

B++ is a small, low-level language for writing games. There are no
header files, no dependency on the standard C library, and no
external tool chain. The compiler is written in B++, compiles
itself, and emits native machine code directly — no assembler, no
linker.

B++ descends from Ken Thompson's B, the language that preceded C on
early Unix. Every value is a machine word, now 64 bits wide.
Structs carry named fields. Optional slice annotations let a word
narrow to a single bit or widen to a 128-bit SIMD vector. Nothing
else is required to ship a game.

The system is small enough to read in an afternoon and large enough
to ship a game. This book is the full language reference and
tutorial in one volume.

---

## 1. A Tutorial Introduction

### 1.1 Getting Started

The shipped repository contains the compiler binary `bpp` already
built. To install it system-wide along with the standard library:

    sh install.sh

`install.sh` bootstraps the compiler from source, then copies `bpp`
and `bug` into `/usr/local/bin/` and the runtime modules into
`/usr/local/lib/bpp/`. The bootstrap step verifies the compiler can
build itself before installing — see `docs/how_to_dev_b++.md` for
the full production discipline.

If `bpp` is missing or unusable, recover it from the C source seed:

    clang bootstrap.c -o bpp

`bootstrap.c` is the compiler emitted as C99. Regenerate it any time
with `bpp --c src/bpp.bpp > bootstrap.c`.

### 1.2 Hello World

    main() {
        putchar('H');
        putchar('i');
        putchar('\n');
        return 0;
    }

Compile and run:

    bpp hello.bpp -o hello
    ./hello

This prints `Hi` followed by a newline. `putchar` writes one byte
to standard output. There are no format strings and no `printf` —
you build what you need from bytes.

### 1.3 A First Game

    import "stbgame.bsm";

    main() {
        game_init(320, 180, "Hi", 60);
        while (game_should_quit() == 0) {
            game_frame_begin();
            if (key_pressed(KEY_ESC)) { break; }
            clear(BLACK);
            draw_text("Hello", 100, 80, 2, WHITE);
            draw_end();
        }
        return 0;
    }

Compile as usual. On macOS the platform layer opens a Cocoa window
and presents pixels via CoreGraphics or Metal. On Linux it speaks
the X11 wire protocol directly over a Unix socket. No shared
libraries, no GLFW, no SDL.

---

## 2. Variables and Storage

All values are 64-bit integers by default. There are no booleans,
no characters as a distinct type. A variable is declared with
`auto`:

    auto x, y, z;

Variables declared outside any function are top-level. Variables
declared inside a function are local.

    auto counter;

    tick() {
        counter = counter + 1;
        return counter;
    }

There is no initialization syntax at declaration. Variables start
at zero.

### 2.1 Storage classes (top-level)

| Keyword | What it does |
|---------|-------------|
| `auto`   | Default. Smart-promoted by the compiler to the best storage class based on usage. |
| `global` | Worker-shared mutable. Readable and writable from worker threads (base phase). |
| `extrn`  | Frozen after init. Mutable before `maestro_run()` / `job_init()`, immutable after. |
| `const`  | Compile-time literal: `const N = 10;` inlines the value at every use site. |

#### Smart promotion (the B++ way)

Start with `auto` for everything. As the project grows and the
maestro pattern is introduced, the compiler promotes automatically:

```bpp
// Level 1: just write code
auto world;
auto assets;
auto counter;

// Level 2: ask the compiler what it did
// $ bpp --show-promotions game.bpp
//   assets -> extrn    (written once in main before maestro_run)
//   world  -> global   (read by base-phase worker function)
//   counter: auto      (no promotion — mutated in loop)

// Level 3: be explicit when you know better
extrn assets;            // "I know this is frozen"
global world;            // "I know workers touch this"
auto counter: serial;    // "don't promote, I want serial"
```

The `: serial` annotation is an escape hatch that prevents
promotion even when the compiler could prove the variable
eligible.

---

## 3. Functions

A function takes zero or more parameters and returns a value.
Parameters and return values are all 64-bit words by default.

    square(x) { return x * x; }

    max(a, b) {
        if (a > b) { return a; }
        return b;
    }

### 3.1 Implicit return

If a function body doesn't end with `return`, the compiler inserts
`return 0;` automatically. Side-effect functions don't need
boilerplate:

    draw_hud() {
        render_text("SCORE", 4, 4, 1, WHITE);
    }

### 3.2 Void functions

Mark a function `void` to say "no useful return value." The
compiler warns (W017) if a `void` function's return is used:

    void draw_hud() { ... }
    auto x;
    x = draw_hud();           // warning[W017]: 'draw_hud' is void

### 3.3 Forward declarations and `stub`

Functions may be called before they are defined. A plain empty body
is a forward declaration that the real definition overrides later;
the last definition wins.

For library modules that provide *default* implementations meant to
be overridden by the user's entry file, use `stub`. The override
is silent — no duplicate warning:

```bpp
// stbgame.bsm — engine stubs
stub game_init() { return 0; }
stub game_update(dt) { return 0; }

// mygame.bpp — user overrides silently
game_init() {
    world = ecs_new(200);
}
```

### 3.4 Visibility

| Keyword | What it does |
|---------|-------------|
| *(nothing)* | **Public.** Any module can call the function. Default. |
| `static`    | **Private.** Only code in the same `.bsm` can reach it. |

Cross-module access to a `static` symbol is a compile-time error.

```bpp
// wolf3d_ray.bsm
cast(x, y, angle) { ... }          // public
static _intersect(wall) { ... }    // private to ray.bsm
```

### 3.5 Function pointers

    add(a, b) { return a + b; }

    apply(fp, x, y) { return call(fp, x, y); }

    main() {
        auto r;
        r = apply(fn_ptr(add), 3, 4);    // r = 7
        return 0;
    }

`fn_ptr(name)` returns the code address of a function.
`call(fp, args...)` invokes the function at that address. Always
pass the pointer as the first argument to `call`.

---

## 4. Control Flow

### 4.1 Conditionals

    if (x > 0) {
        y = x;
    } else if (x == 0) {
        y = 1;
    } else {
        y = 0 - x;
    }

A single-statement body may omit braces:

    if (x < 0) x = 0 - x;

### 4.2 Loops

    while (i < n) {
        if (i == 5) { break; }
        sum = sum + i;
        i = i + 1;
    }

    for (i = 0; i < 10; i = i + 1) {
        putchar('0' + i);
    }

`for` desugars to `while` internally. `break` exits the innermost
loop; `continue` skips to the next iteration (re-running the step
expression in a `for`).

### 4.3 Switch

Two forms, no fallthrough, no `break`, no `case`:

    // Value dispatch
    switch (dir) {
        UP    { dy = 0 - 1; }
        DOWN  { dy = 1; }
        LEFT, RIGHT { dx = 0; }   // comma for multi-value
        else  { /* default */ }
    }

    // Condition dispatch
    switch {
        hp <= 0        { die(); }
        hp < hp_max/4  { flash_red(); }
        else           { /* healthy */ }
    }

The compiler emits **W021** if a value switch has no `else`.
Inherited from B (1969), corrected.

### 4.4 Short-circuit `&&` and `||`

The parser rewrites `a && b` to `a ? b : 0` and `a || b` to
`a ? 1 : b` before codegen, so every backend honours C-style
short-circuit semantics:

    if (p != 0 && p.field > 0) { ... }   // memory-safe everywhere

### 4.5 Ternary

    cond ? a : b

Only the selected branch evaluates. Right-associative:
`a ? b : c ? d : e` parses as `a ? b : (c ? d : e)`.

### 4.6 Compile-time folding

Integer-literal expressions fold at parse time: `3 + 5` becomes
`8` in the AST. `if (0) { ... }` and `while (0) { ... }` drop
their bodies; `if (1) { x } else { y }` keeps only `x`.

---

## 5. Operators

### Arithmetic

    *  /  %       multiply, divide, remainder
    +  -          add, subtract
    -x            unary negate
    ~x            bitwise NOT
    !x            logical NOT (1 if x == 0, else 0)

### Shift

    <<  >>        logical left / right shift

The shift amount is taken modulo the word width.

### Relational and equality

    <  >  <=  >=  signed on integers, IEEE on floats
    ==  !=        equality

Comparisons return `0` (false) or `1` (true) as a 64-bit word.

### Bitwise and logical

    &  |  ^       bitwise AND, OR, XOR (binary only)
    &&            logical AND (short-circuit)
    ||            logical OR  (short-circuit)

Unary `&x` is address-of, not bitwise AND.

### Memory

    *p            dereference: read/write the word (or slice) at p
    &x            address-of: take the address of a local / global / field
    a[i]          indexed load: *(a + i * stride)
    s.f           struct field access on a typed local

### Assignment

    =             plain assignment
    +=  -=  *=  /=  %=             compound arithmetic
    &=  |=  ^=  <<=  >>=           compound bitwise

Compound assignments evaluate the left operand once.

### Ternary

    c ? a : b     conditional expression

---

## 6. Literals

### 6.1 Integers

- Decimal: `42`, `-17`, `1000000`
- Hexadecimal: `0x1F`, `0xFF00AA55`
- Octal: `0755` (leading zero)
- Character: `'A'` (ASCII code, 65)

All integer literals are 64-bit words.

### 6.2 Floats

- With fractional part: `1.0`, `3.14159`, `0.5`
- Scientific: `1.5e3`, `2.0e-7`

Float literals are 64-bit doubles. They retain IEEE bits only when
assigned to a `: float`, `: half float`, or `: quarter float`
local. An un-annotated `auto x = 44100.0;` **silently truncates to
int** and produces diagnostic **E232** — always annotate float
locals.

### 6.3 Strings

    "hello"          // null-terminated byte sequence
    "line\n"         // escape sequences: \n \r \t \\ \" \0 \xHH

String literals are immutable; the compiler deduplicates identical
literals into the same data-section slot.

### 6.4 Character literals

    'A'        // 65
    '\n'       // 10
    '\t'       // 9
    '\\'       // 92
    '\''       // 39

---

## 7. Comments

    // Line comment — extends to end of line.
    /* Block comment — does NOT nest. */

Tonify Rule 9 requires one blank comment line between a function's
intro comment and its first body statement, for clean generated
documentation.

---

## 8. The Type System

B++ uses an orthogonal type system with two axes: **base type**
(what it is) and **slice** (how wide). The compiler infers the
base type; the programmer optionally specifies the slice for
packed layout or performance tuning.

### 8.1 Base types

    word       64-bit integer (default — every value starts as a word)
    float      64-bit double (inferred from '.' or float math)
    ptr        64-bit pointer (inferred from string literals, malloc, etc.)
    struct     composite type (defined with the struct keyword)

### 8.2 Slice annotations

The slice ladder runs from 1 bit to 128 bits. Apply to struct
fields (packed layout) or local variables (storage hints).

| Annotation | Width | Status |
|------------|-------|--------|
| `: bit`  | 1 bit (boolean flag) | Phase A1 |
| `: bit2` | 2 bits (0-3)   | Phase A1 |
| `: bit3` | 3 bits (0-7)   | Phase A1 |
| `: bit4` | 4 bits (0-15)  | Phase A1 |
| `: bit5` | 5 bits (0-31)  | Phase A1 |
| `: bit6` | 6 bits (0-63)  | Phase A1 |
| `: bit7` | 7 bits (0-127) | Phase A1 |
| `: byte`    | 8-bit unsigned integer (0-255) | ship |
| `: quarter` | 16-bit integer                 | ship |
| `: half`    | 32-bit integer                 | ship |
| *(default)* | 64-bit word                    | ship |
| `: quarter float` | 16-bit float (ARM64 FEAT_FP16) | ship |
| `: half float`    | 32-bit float                   | ship |
| `: float`         | 64-bit double                  | ship |
| `: double`        | 128-bit SIMD (4× float32)      | ship |
| `: StructName`    | Typed struct access (dot syntax) | ship |

```bpp
auto hp: byte;              // 8-bit health points
auto velocity: half float;  // 32-bit float velocity
auto player: Player;        // typed struct with dot access
auto flags: bit4;           // 4 flags packed (Phase A1)
auto lanes: double;         // 128-bit SIMD vector
```

Sub-word types also work as standalone declaration keywords:

    byte hp;
    half score;
    quarter tile_id;

Per-variable hints in one declaration:

    auto r: byte, g: byte, b: byte;

### 8.3 How it works

Each type packs into a single byte:

- Bit 0    = float flag (1 = float, 0 = not float)
- Bits 0-3 = base (WORD=0x02, FLOAT=0x03, PTR=0x04, STRUCT=0x08)
- Bits 4-5 = slice (FULL=0, HALF=1, QUARTER=2, BYTE=3)

The compiler uses narrower load/store instructions for sub-word
types: LDRB/STRB for byte, LDRH/STRH for quarter, LDR w/STR w for
half (integer), LDR s/STR s + FCVT for half float. The C emitter
maps to `uint8_t`, `uint16_t`, `uint32_t`, `float`, `double`.

Without a hint, all values are 64-bit. Sub-word values truncate on
store: `auto x: byte; x = 300;` gives 44 (hardware truncation, no
runtime check).

### 8.4 Promotion rules

When types mix in an expression, the compiler widens:

- `PTR` wins everything (pointer arithmetic).
- `FLOAT` wins integers (int converts to float).
- Float widens to at least the integer's width: f32 + i64 = f64.
- Same base: wider slice wins (byte + half = half).

---

## 9. Memory

    malloc(n)          allocate n bytes, return pointer
    malloc_aligned(n,a) allocate with explicit alignment
    realloc(p, n)      resize a block
    free(p)            release memory
    memcpy(d, s, n)    copy n bytes
    peek(addr)         read one byte
    poke(addr, val)    write one byte

The dereference operator `*p` reads/writes 8 bytes (one word).
Offsets are in bytes:

    auto buf;
    buf = malloc(80);
    *(buf + 0) = 100;
    *(buf + 8) = 200;
    auto x;
    x = *(buf + 0);       // 100

### 9.1 Address-of (`&`)

`&x` returns the memory address of a local, global, or struct
field. This is the inverse of dereference: `*(&x) == x`.

```bpp
auto avail;
sys_ioctl(fd, 0x541B, &avail);    // FIONREAD writes into avail

auto tv;
sys_clock_gettime(0, &tv);         // kernel fills the timespec

set_value(dest, val) { *(dest) = val; return 0; }
auto x;
x = 0;
set_value(&x, 42);                 // x is now 42
```

`&` replaces the older `malloc(8)` scratch pattern. It does not
work with extern FFI functions or `call()`; the compiler emits
**W012** for those cases — fall back to `malloc(8)` there.

### 9.2 Array indexing

`a[i]` is sugar for `*(a + i * stride)` — where `stride` is 8 for
plain words or the slice size for typed arrays.

    auto buf;
    buf = malloc(40);
    buf[0] = 10;          // *(buf + 0) = 10
    buf[1] = 20;          // *(buf + 8) = 20

    for (i = 0; i < 5; i = i + 1) {
        buf[i] = i * 100;
    }

### 9.3 String access

A string literal is a pointer to a null-terminated byte sequence.
`str_peek(s, i)` reads byte `i`:

    auto s;
    s = "hello";
    putchar(str_peek(s, 0));   // 'h'

---

## 10. Composite Types

### 10.1 Structs

A struct is a named group of fields. By default each field is 8
bytes:

    struct Vec2 { x, y }                  // 16 bytes

Fields can have slice hints for packed layout:

    struct Pixel { r: byte, g: byte, b: byte, a: byte }   //  4 bytes
    struct Tile  { id: quarter, flags: byte }              //  3 bytes
    struct Particle {
        pos: double,              // 16 bytes (SIMD)
        vel: double,              // 16 bytes (SIMD)
        mass: half float,         //  4 bytes
        id                        //  8 bytes
    }

Heap-allocated structs use `auto` with a type annotation:

    auto p: Pixel;
    p = malloc(sizeof(Pixel));   // 4 bytes
    p.r = 255;                   // STRB — 1-byte write
    p.g = 128;

Stack-allocated structs use `var` — no malloc:

    var v: Vec2;
    v.x = 100;
    v.y = 200;

`sizeof(Pixel)` is a compile-time constant (sum of field sizes).
`p.r` on a `: byte` field emits LDRB/STRB (1 byte), not LDR/STR.

**Rule 8 (critical).** When a pointer targets a sliced struct,
always use typed access (`auto p: Struct; p.field`). Raw offset
`*(ptr + N)` does NOT respect the packed layout and will read
wrong memory. See `docs/tonify_checklist.md`.

### 10.2 Enums

Named integer constants, resolved at compile time:

    enum Dir   { UP, DOWN, LEFT, RIGHT }
    enum Layer { GROUND = 1, PLAYER = 2, ENEMY = 4, BULLET = 8 }

    auto d;
    d = RIGHT;                   // d = 3
    if (PLAYER & ENEMY) { ... }

Sequential values start at 0 by default. The parser replaces names
with integer literals — enums are pure sugar.

### 10.3 SIMD (`: double` and `vec_*`)

128-bit SIMD lives in the language as the widest rung of the slice
ladder. A local declared `: double` reserves a 16-byte,
16-byte-aligned stack slot holding four 32-bit floats. Eleven
`vec_*` builtins operate on those slots and lower to NEON on
ARM64 and SSE2 on x86_64 — no library call, no runtime feature
check.

```bpp
auto v: double;
v = vec_splat4(2.0);         // every lane = 2.0
```

| Builtin | Meaning |
|---------|---------|
| `vec_load4(ptr)`    | Load 4 float32 lanes (unaligned-safe). |
| `vec_store4(ptr, v)` | Store 4 float32 lanes (unaligned-safe). |
| `vec_splat4(x)`     | Broadcast scalar `x` to every lane. |
| `vec_add4(a, b)`    | Lane-wise addition. |
| `vec_sub4(a, b)`    | Lane-wise subtraction. |
| `vec_mul4(a, b)`    | Lane-wise multiplication. |
| `vec_div4(a, b)`    | Lane-wise division. |
| `vec_min4(a, b)`    | Lane-wise minimum. |
| `vec_max4(a, b)`    | Lane-wise maximum. |
| `vec_dot4(a, b)`    | Horizontal sum of `a * b`. Scalar float. |
| `vec_get4(v, i)`    | Extract lane `i` (0..3) as a scalar float. |

Assigning a SIMD result to a non-`: double` local is error **E231**.

**Alignment.** Stack slots for `: double` are always 16-byte
aligned. Heap buffers passed to `vec_load4`/`vec_store4` do NOT
need to be aligned — the native backends always emit the unaligned
form (LDR Q on ARM64, MOVUPS on x86_64).

**Baseline ISA.** ARMv8 base (FADD/FMUL/FDIV/FMIN/FMAX `.4S`,
DUP, FADDP, LDR Q / STR Q). SSE2 only on x86_64 (ADDPS, MULPS,
MOVUPS, SHUFPS, MOVHLPS) — part of the x86_64 baseline, no
runtime feature detection. The C transpile path does not currently
map `: double` or `vec_*`.

---

## 11. Modules

### 11.1 `import` vs `load`

| Keyword | What it does |
|---------|-------------|
| `import` | **Engine/library module.** Searches `stb/`, `src/`, installed paths, then `/usr/local/lib/bpp/`. Visibility rules (`static`) are not enforced across imported modules. |
| `load`   | **Project file.** Searches only the entry file's directory. Visibility rules ARE enforced between loaded modules. If the file is not found locally, it's a fatal error — no fallback to engine paths. |

```bpp
// wolf3d.bpp
load "wolf3d_ray.bsm";     // my project file — strict rules
load "wolf3d_enemy.bsm";
import "stbgame.bsm";       // engine — permissive rules
import "stbecs.bsm";
```

### 11.2 FFI imports

    import "raylib" {
        void InitWindow(int, int, char*);
        void SetTargetFPS(int);
        int WindowShouldClose();
    }

This declares external functions from a dynamic library. In native
mode, the compiler creates GOT entries and links with the dylib at
load time. The library name maps to
`/opt/homebrew/lib/lib{name}.dylib` on macOS.

### 11.3 Auto-injection

Every user `.bpp` entry file gets a standard set of runtime modules
injected at compile time — the user never `import`s them by hand:

    bpp_mem     malloc/free/realloc/memcpy
    bpp_array   dynamic arrays
    bpp_hash    hash maps
    bpp_buf     raw buffer readers/writers
    bpp_str     string utilities
    bpp_io      putchar/getchar
    bpp_math    numeric helpers
    bpp_file    file I/O
    bpp_beat    universal monotonic clock
    bpp_job     N×SPSC worker pool
    bpp_maestro solo / base / render bandleader

On top of these, the compiler injects `bpp_defs.bsm` at codegen
time and the per-OS runtime (`_bmem`, `_brt0`, `_bsys`) plus the
platform layer whenever `stbgame` is in the import graph. See
`docs/how_to_dev_b++.md` for the full injection list.

---

## 12. Concurrency — the Maestro Pattern

B++ has a six-level effect lattice that classifies every function:

```
PANIC  ≤  BASE  ≤  { IO, GPU, HEAP, REALTIME }  ≤  SOLO
```

Five names are writable as annotations; `HEAP` and `PANIC` exist in
the lattice but are inferred, never annotated.

### 12.1 Phase annotations

| Annotation | Code | Meaning |
|------------|------|---------|
| `: base`     | 1 | Pure — no global writes, no impure calls. Worker-safe. |
| `: solo`     | 2 | Any side effect (catch-all). Main thread only. |
| `: realtime` | 3 | Audio-callback safe: no malloc, no IO, bounded time. |
| `: io`       | 4 | Touches files / network / stdout / stderr. |
| `: gpu`      | 5 | Touches GPU resources (render thread only). |

```bpp
helper(x) { return x * 2; }              // inferred PHASE_BASE

particle_update(i): base {                // W013 if body not pure
    ecs_set_pos(world, i, new_x);
}

critical_section(): solo {                // no auto-dispatch
    update_global_state();
}

mixer_fill(buf, n): realtime {            // W013 if body mallocs
    synth_render(buf, n);
}
```

The compiler verifies every annotation. If the body's inferred
effect is stricter than the annotation, the hint is redundant. If
it's more permissive, the compiler emits **W013** ("declared
`: base` but calls `malloc`") or **W026** (effect escalation).

### 12.2 Maestro

The maestro pattern is B++'s concurrency front-end. The entry file
calls `maestro_run()`; the compiler classifies every function as
solo, base, or render and dispatches base functions to a worker
pool automatically. No threads to manage, no locks to write — the
compiler proves safety from the phase lattice.

---

## 13. Core Builtins

Every B++ program gets these names without any import. They are
resolved in the frontend, emitted inline by the codegen, and
classified in the effect lattice.

### Memory

| Builtin | Effect | Meaning |
|---------|--------|---------|
| `peek(p)`               | base | Read one byte at address `p`. |
| `poke(p, v)`            | solo | Write byte `v` at address `p`. |
| `str_peek(p, i)`        | base | Read byte `i` of a string pointer. |
| `malloc(n)`             | heap | Allocate `n` bytes (16-byte aligned). |
| `malloc_aligned(n, a)`  | heap | Allocate with explicit alignment. |
| `realloc(p, n)`         | heap | Resize; may move. |
| `realloc(p, old, new)`  | heap | Resize with copy of `old` bytes. |
| `free(p)`               | heap | Release. |
| `memcpy(d, s, n)`       | base | Copy `n` bytes, return `d`. |
| `mem_barrier()`         | base | Compiler-level memory barrier. |
| `fn_ptr(name)`          | base | Function pointer from a symbol name. |
| `call(fp, args...)`     | *varies* | Indirect call through a function pointer. |

### I/O

| Builtin | Effect | Meaning |
|---------|--------|---------|
| `putchar(c)`     | io | Write byte to stdout. |
| `putchar_err(c)` | io | Write byte to stderr. |
| `getchar()`      | io | Read one byte from stdin; `-1` on EOF. |

### Command line

| Builtin | Meaning |
|---------|---------|
| `argc_get()` | Number of command-line arguments. |
| `argv_get(i)` | Argument `i` as a null-terminated pointer. |

### Raw syscalls

Same name on both macOS (arm64) and Linux (x86_64); the backend
resolves the syscall number:

```
sys_open    sys_close    sys_read    sys_write
sys_lseek   sys_mmap     sys_munmap  sys_ioctl
sys_mkdir   sys_unlink   sys_fchmod  sys_getdents
sys_fork    sys_execve   sys_wait4   sys_waitpid
sys_exit    sys_ptrace   sys_socket  sys_connect
sys_clock_gettime  sys_nanosleep  sys_usleep
```

### Float returns and helpers

| Builtin | Meaning |
|---------|---------|
| `float_ret()`  | Read saved first float return register (d0/xmm0) from last extern call. |
| `float_ret2()` | Read saved second float return register (d1/xmm1). |
| `shr(v, n)`    | Logical right shift (unsigned, fills with zeros). Unlike `>>` which sign-extends. |
| `assert(cond)` | Trap (BRK/INT3) if `cond` is zero. Zero overhead when true. |

### SIMD

`vec_splat4`, `vec_load4`, `vec_store4`, `vec_add4`, `vec_sub4`,
`vec_mul4`, `vec_div4`, `vec_min4`, `vec_max4`, `vec_dot4`,
`vec_get4` — see §10.3.

I/O is implemented via raw syscalls (ARM64 `svc #0x80` on macOS,
`syscall` on Linux x86_64). There is no libc dependency.

---

## 14. The Standard Library

The `stb/` directory contains library modules. All are pure B++
with zero external dependencies. Modules promoted to universal
runtime (`bpp_*` in `src/`) are auto-injected; everything in
`stb/` requires an explicit `import`.

### 14.1 stbgame — Game Loop

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

`game_init` calls every subsystem init (math, input, draw, font,
ui) automatically. The platform layer is auto-injected whenever
`stbgame` is in the import graph — `stbgame.bsm` itself never
mentions a platform module by name.

| Function | Description |
|----------|-------------|
| `game_init(w, h, title, fps)` | Create window, init all subsystems |
| `game_frame_begin()`          | Poll input, calculate delta time |
| `game_dt()`                   | Delta in seconds (float) |
| `game_dt_ms()`                | Delta in milliseconds (integer) |
| `game_should_quit()`          | 1 if window closed |

### 14.2 stbdraw — CPU Drawing

Software framebuffer drawing. Same colour constants as `stbrender`.

| Function | Description |
|----------|-------------|
| `draw_rect(x, y, w, h, color)`       | Filled rectangle |
| `draw_circle(cx, cy, r, color)`      | Filled circle |
| `draw_line(x1, y1, x2, y2, color)`   | Bresenham line |
| `draw_text(s, x, y, sz, color)`      | Text (8×8 bitmap) |
| `draw_number(x, y, sz, color, val)`  | Integer as text |
| `draw_sprite(data, x, y, w, h)`      | RGBA sprite with transparency |
| `draw_sprite16(spr, pal, x, y)`      | 16×16 palette-indexed sprite |
| `draw_bar(x, y, w, h, val, max, c)`  | Progress bar |
| `measure_text(s, sz)`                | Text width in pixels |
| `clear(color)`                       | Clear framebuffer |
| `put_px(x, y, color)`                | Single pixel |
| `blend_px(x, y, color)`              | Alpha-blended pixel |
| `draw_end()`                         | Present to screen |
| `rgba(r, g, b, a)`                   | Pack ARGB word |

Colours: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`,
`ORANGE`, `PURPLE`, `GRAY`, `DARKGRAY`.

### 14.3 stbrender — GPU Drawing

Metal on macOS, Vulkan on Linux (planned). Same colours as
`stbdraw`.

| Function | Description |
|----------|-------------|
| `render_init()`                          | Initialize GPU (after `game_init`) |
| `render_begin()`                         | Start GPU frame |
| `render_clear(color)`                    | Set clear colour |
| `render_rect(x, y, w, h, color)`         | Filled rect |
| `render_line(x0, y0, x1, y1, color)`     | Line |
| `render_circle(cx, cy, r, color)`        | Filled circle (32 segments) |
| `render_circle_outline(cx, cy, r, c)`    | Circle outline |
| `render_rect_outline(x, y, w, h, color)` | Rectangle outline |
| `render_text(s, x, y, sz, color)`        | GPU text |
| `render_end()`                           | Flush, present, poll input |

The GPU backend batches vertices (8 bytes each: int16 x,y +
uint8 r,g,b,a) and draws as triangles. Coordinates go integer
from B++; the shader converts to clip space.

### 14.4 stbinput — Keyboard and Mouse

| Function | Description |
|----------|-------------|
| `key_down(k)`                    | 1 if key held |
| `key_pressed(k)`                 | 1 if just pressed (edge) |
| `mouse_x()` / `mouse_y()`        | Cursor position |
| `mouse_down(btn)`                | 1 if button held |
| `mouse_pressed(btn)`             | 1 if just pressed |
| `mouse_released(btn)`            | 1 if just released |
| `point_in_rect(px,py,x,y,w,h)`   | Hit test |

Keys: `KEY_UP`/`DOWN`/`LEFT`/`RIGHT`, `KEY_W`/`A`/`S`/`D`,
`KEY_SPACE`, `KEY_ENTER`, `KEY_ESC`, `KEY_TAB`, `KEY_SHIFT`,
`KEY_E`/`R`/`Z`/`X`/`G`/`C`/`N`/`T`, `KEY_0`..`KEY_9`,
`KEY_PERIOD`, `KEY_BACKSPACE`.

Mouse buttons: `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`.

### 14.5 stbaudio — CoreAudio Output

Low-level audio stack. AudioQueue on macOS with an SPSC ring
buffer feeding the callback.

| Function | Description |
|----------|-------------|
| `audio_init(sample_rate, channels)` | Start AudioQueue |
| `audio_write(buf, frames)`          | Push frames into the ring |
| `audio_shutdown()`                  | Stop and free |

### 14.6 stbmixer — Polyphonic Mixer

10-voice mixer that handles three voice kinds: synthesized tones
(via note_on), one-shot samples (SFX), and looping streams (BGM
music). Tones and SFX share the first 8 slots; music lives on the
dedicated slot 8 so BGM can't be knocked out by a flood of effects.

Three independent volume buses — `MX_BUS_MASTER`, `MX_BUS_MUSIC`,
`MX_BUS_SFX` — route voice output with per-bus 0..100 gain.

| Function | Description |
|----------|-------------|
| `mixer_init()`                       | Allocate voice and bus tables |
| `mixer_note_on(key_id, freq_hz)`     | Start a tone voice |
| `mixer_note_off(key_id)`             | Release tones matching key |
| `mixer_play_sample(buf, n_frames)`   | One-shot stereo s16 sample |
| `mixer_play_music(buf, n_frames, loop)` | Stream BGM on slot 8 |
| `mixer_stop_music()`                 | Halt the music voice |
| `mixer_music_frames()`               | Playback position (frames) |
| `mixer_music_ms()`                   | Playback position (ms) |
| `mixer_set_bus_volume(bus, vol)`     | Set a bus's 0..100 gain |
| `mixer_get_bus_volume(bus)`          | Read a bus's gain |
| `mixer_set_fader(val)`               | Tone waveform morph 0..768 |
| `mixer_set_dirt(val)`                | Bitcrush + decimation 0..256 |
| `mixer_fill(buf, n): realtime`       | Render mono-summed stereo |
| `mixer_stream(n)`                    | Render + push to audio ring |

### 14.7 stbsound — WAV I/O

WAV read/write (RIFF PCM, 44100 stereo s16). Pure codec — no
device dependency.

| Function | Description |
|----------|-------------|
| `sound_load_wav(path)`          | Decode a WAV; buffer via `sound_buf()` |
| `sound_save_wav(path, buf, n)`  | Write a WAV to disk |
| `sound_buf()` / `sound_frames()` | Last-loaded buffer + length |

### 14.8 stbtile — Tilemap Engine

Grid of tile types with a solid mask and an optional textured
tileset.

| Function | Description |
|----------|-------------|
| `tile_new(w, h, tw, th)`              | Allocate a tilemap |
| `tile_get/tile_set(tm, gx, gy, val)`  | Read / write a tile |
| `tile_solid(tm, type)`                | Mark a tile type solid |
| `tile_collides(tm, px, py, pw, ph)`   | AABB hit-test |
| `tile_load_set(path, tw, th, &out)`   | Load PNG tileset |
| `tile_map_type(tm, game_type, idx)`   | Remap logical → tileset |
| `tile_draw(tm, cx, cy, sw, sh)`       | GPU render with culling |

### 14.9 stbphys — Platformer Physics

Milli-pixel positions and pixel/sec velocities. `pos += vel * dt_ms`
works directly in integer math.

| Function | Description |
|----------|-------------|
| `phys_body(w, h, gravity, jump_vel, move_spd)` | Allocate a body |
| `phys_set_pos(b, px, py)`                      | Set pixel-coord position |
| `phys_jump(b)`                                 | Impulse if grounded |
| `phys_update(b, dt_ms, tm)`                    | Step physics |
| `phys_px_x(b)` / `phys_px_y(b)`                | Pixel position for drawing |

### 14.10 stbpath — A* Pathfinding

Grid-based A* with a binary min-heap and indexed decrease-key. Pure
algorithm, no graphics dependency.

| Function | Description |
|----------|-------------|
| `path_new(w, h)`                        | Allocate a PathFinder |
| `path_set_blocked(pf, gx, gy, flag)`    | Mark a cell |
| `path_find(pf, sx, sy, gx, gy, out, n)` | Run A*, return waypoint count |

### 14.11 stbecs — Entity-Component System

Parallel-array ECS. Positions and velocities in milli-units.
Custom game components live in caller-owned parallel arrays keyed
by the same entity ID — `ecs_component_new` allocates a
capacity-sized array, `ecs_component_at` returns a typed pointer
to the id-th element.

| Function | Description |
|----------|-------------|
| `ecs_new(capacity)`                       | Allocate a world |
| `ecs_spawn(w)`                            | Allocate an entity |
| `ecs_kill(w, id)` / `ecs_alive(w, id)`    | Lifecycle |
| `ecs_set_pos/vel/flags(w, id, ...)`       | Write core components |
| `ecs_get_x/y/flags(w, id)`                | Read core components |
| `ecs_physics(w, dt_ms)`                   | Step by `vel * dt_ms` |
| `ecs_component_new(w, element_size)`      | Allocate a custom parallel array |
| `ecs_component_at(base, id, element_size)`| Typed pointer to id-th element |

### 14.12 stbsprite — GPU Sprites

Palette-indexed sprite loader + sheet animation helpers.

| Function | Description |
|----------|-------------|
| `load_sprite(path, out, w, h)`         | Parse a JSON palette sprite |
| `sprite_create(spr, pal, w, h)`        | Upload as GPU texture |
| `sprite_draw(tex, x, y, w, h, scale)`  | Draw at integer scale |
| `sprite_draw_frame(tex, x, y, fw, fh, sheet_w, sheet_h, row, col, scale)` | Draw one cell of a spritesheet |
| `anim_frame(elapsed_ms, fps, frame_count, loop)` | Pick current frame from a time accumulator |

### 14.13 stbimage — PNG Loader

Pure B++ PNG decoder. DEFLATE, all five filter types,
palette-indexed images, tRNS for palette transparency.

| Function | Description |
|----------|-------------|
| `img_load(path)`      | Decode PNG, return RGBA buffer |
| `img_w()` / `img_h()` | Dimensions of last load |
| `img_free(buf)`       | Release |

### 14.14 stbfont — Text Rendering

8×8 bitmap font for ASCII 32-127, plus a pure-B++ TrueType reader
(`cmap`, `glyf`, Bézier rasterization, scanline antialiasing).

### 14.15 stbui — Immediate-Mode UI

| Function | Description |
|----------|-------------|
| `gui_label(x, y, text)`                  | Text label |
| `gui_number(x, y, val)`                  | Number display |
| `gui_panel(x, y, w, h, color)`           | Background panel |
| `gui_button(x, y, w, h, label)`          | Button (1 on click) |
| `gui_bar(x, y, w, h, val, max, color)`   | Progress bar |
| `style_new(bg, fg, font_sz, pad)`        | Create style |

### 14.16 stbcol — Collision Detection

| Function | Description |
|----------|-------------|
| `rect_overlap(...)`             | AABB overlap |
| `circle_overlap(...)`           | Circle overlap (no sqrt) |
| `rect_contains(...)`            | Point inside rect |
| `circle_contains(...)`          | Point inside circle |

### 14.17 stbpool — Object Pool

Fixed-size object pool with embedded freelist for O(1) get/put.

| Function | Description |
|----------|-------------|
| `pool_new(obj_size, capacity)` | Allocate |
| `pool_get(p)`                  | Take an object |
| `pool_put(p, obj)`             | Return to freelist |

### 14.18 stbasset — Handle-Based Asset Manager

Loads sprites, sounds, music, and fonts once and hands out
`uint32` handles (16-bit generation + 16-bit slot). Stale
handles fail generation check — no use-after-free. Repeated
loads of the same path return the same handle (dedup).

| Function | Description |
|----------|-------------|
| `init_asset()`                             | Allocate the slot table |
| `asset_load_sprite(path)` → `SpriteHandle` | Decode PNG + upload to GPU |
| `asset_load_sound(path)` → `SoundHandle`   | Decode WAV into PCM buffer |
| `asset_load_music(path)` → `MusicHandle`   | Decode WAV for looped BGM |
| `asset_load_font(path, pixel_size)` → `FontHandle` | Load a TTF |
| `asset_get(h)`                             | Resolve to raw pointer, 0 if stale |
| `asset_width(h)` / `asset_height(h)`       | Natural dimensions |
| `asset_kind(h)`                            | One of `ASSET_SPRITE` etc. |
| `asset_unload(h)`                          | Free + bump generation |
| `asset_unload_all()`                       | Drop every loaded asset |
| `asset_count()`                            | Live-asset count (leak probe) |

Handle layout: `[16-bit generation | 16-bit slot index]`. Slot 0
is reserved as the null handle, so `auto h;` initialised to 0 is
always a safe "no asset" sentinel.

### 14.19 stbscene — Scene Manager

Register scenes (menu, play, results) by numeric id and switch
between them from any callback. Actual switches are deferred to
the next tick — safe to call from inside update or draw.

| Function | Description |
|----------|-------------|
| `init_scene()`                                       | Allocate scene table |
| `scene_register(id, load_fn, update_fn, draw_fn, unload_fn)` | Register a scene |
| `scene_switch(id)`                                   | Queue switch to scene |
| `scene_tick(dt_ms)`                                  | Run deferred switch + update |
| `scene_draw()`                                       | Draw current scene |
| `scene_shutdown()`                                   | Unload active scene + clear |
| `scene_current()`                                    | Active id, -1 if none |

### 14.20 Universal runtime (auto-injected)

These live in `src/bpp_*.bsm` and are injected into every user
program — documented here for completeness.

- **bpp_mem**     — `malloc`/`free`/`realloc`/`memcpy` wrappers
- **bpp_array**   — dynamic arrays with shadow header (`arr_new`,
  `arr_push`, `arr_get`, `arr_len`, `arr_free`, ...)
- **bpp_hash**    — open-addressing hash maps: `Hash` (word keys)
  and `HashStr` (byte-sequence keys)
- **bpp_buf**     — `read_u8`/`u16`/`u32`/`u64` and `write_*`
- **bpp_str**     — `str_len`, `str_eq`, `str_cpy`, `str_chr`,
  `str_starts`, `str_dup`, plus the `sb_*` dynamic string builder
- **bpp_io**      — `print_char`, `print_int`, `print_ln`, `print_msg`
- **bpp_math**    — `randi`, `rng_seed`, `iabs`, `min`, `max`,
  `clamp`, `math_cos`, `math_sin`, `vec2_*`
- **bpp_file**    — `file_read_all`, `file_write_all`
- **bpp_beat**    — universal monotonic clock
- **bpp_job**     — N×SPSC worker pool
- **bpp_maestro** — solo / base / render bandleader
- **bpp_arena**   — bump allocator (explicit import only, not
  auto-injected)

### 14.21 Backend Drivers

By default, `stbgame` runs natively on macOS using Cocoa/Metal
and on Linux using X11. To override with a portable driver:

    import "stbgame.bsm";
    import "drv_sdl.bsm";      // SDL2
    // or
    import "drv_raylib.bsm";   // raylib

The game code stays identical — only the import changes. Drivers
live in `drivers/` and implement the `_stb_*` platform functions
via external FFI. Mixing `stbgame` + driver is useful for the C
transpile path (`bpp --c`) where native `objc_msgSend` calling
convention is not safe to reach through C.

---

## 15. The Compiler

The B++ compiler is written in B++ and compiles itself. It produces
native ARM64 Mach-O binaries with built-in ad-hoc code signing, or
static x86_64 ELF binaries for Linux.

    bpp source.bpp -o binary              # native host binary (default)
    bpp --linux64 source.bpp -o binary    # cross-compile to Linux x86_64 ELF
    bpp --c source.bpp > out.c            # emit portable C99
    bpp --asm source.bpp > out.s          # emit assembly (monolithic)
    bpp --monolithic source.bpp -o out    # force single-pass native
    bpp --show-deps source.bpp            # print dependency graph
    bpp --show-promotions source.bpp      # print smart-promotion decisions
    bpp --clean-cache                     # wipe ~/.bpp/cache/*.bo

### 15.1 Source layout

- `src/` — core compiler modules
  - `bpp.bpp` (driver)
  - `bpp_import.bsm`, `bpp_lexer.bsm`, `bpp_parser.bsm`,
    `bpp_types.bsm`, `bpp_dispatch.bsm`, `bpp_validate.bsm`,
    `bpp_diag.bsm`, `bpp_bo.bsm`, `bpp_bug.bsm`, `bpp_internal.bsm`,
    `bpp_defs.bsm`
  - The universal runtime modules (`bpp_mem.bsm`, `bpp_array.bsm`,
    `bpp_hash.bsm`, `bpp_buf.bsm`, `bpp_str.bsm`, `bpp_io.bsm`,
    `bpp_math.bsm`, `bpp_file.bsm`, `bpp_beat.bsm`, `bpp_job.bsm`,
    `bpp_maestro.bsm`, `bpp_arena.bsm`)
- `src/backend/` — split four ways
  - `chip/<arch>` — instruction encoders and per-arch codegen
  - `os/<os>` — syscall tables and OS runtime (`_bmem`, `_brt0`,
    `_bsys`, platform layer, bug observer)
  - `target/<arch>_<os>` — linker / object-file writer for each
    chip+OS pair
  - `c` — C transpiler (used by `bpp --c`)
- `stb/` — optional engine modules (see §14)
- `drivers/` — portable driver modules

### 15.2 Modular compilation

Native backends use per-module compilation by default, inspired by
Go's build model. Each module compiles to a `.bo` (B++ Object) cache
file containing export data and machine code. On rebuild, unchanged
modules load from cache.

The compiler hashes each module's content and tracks dependencies.
When a module changes, it and all its dependents recompile;
unchanged modules load from `~/.bpp/cache/` in milliseconds.

Cross-module function calls use type-4 relocations (BL/CALL
placeholders) resolved after all modules are loaded. The C emitter
(`--c`) and assembly output (`--asm`) use the monolithic pipeline —
single-pass, no caching.

### 15.3 Self-hosting

The fixed-point property holds: the compiler compiled by itself
produces identical output to the compiler compiled by the previous
version. To rebuild:

    bpp src/bpp.bpp -o bpp

See `docs/how_to_dev_b++.md` for the full bootstrap discipline.

### 15.4 Cross-compile to Linux

    bpp --linux64 examples/snake_cpu.bpp -o snake_linux

The Linux build uses the X11 wire protocol (Phases 1-3: window,
software framebuffer via XPutImage, keyboard + mouse) when `DISPLAY`
is set, falling back to ANSI terminal rendering when it isn't. To
run the binary inside Docker with the window on a macOS host:

    # XQuartz setup
    brew install --cask xquartz
    defaults write org.xquartz.X11 nolisten_tcp 0
    killall XQuartz 2>/dev/null
    open -a XQuartz
    xhost +localhost

    docker run --rm \
      --add-host host.docker.internal:host-gateway \
      -e DISPLAY=host.docker.internal:0 \
      -v /tmp:/tmp \
      ubuntu:22.04 \
      /tmp/snake_linux

Currently only `draw_*` (CPU framebuffer) games work via X11. GPU
(`render_*`) on Linux needs Vulkan, which is deferred (see
`docs/todo.md`).

### 15.5 Portable C output

The C emitter translates B++ to portable C99. The right pattern is
to write the game with `drv_raylib.bsm` (or `drv_sdl.bsm`) instead
of `stbgame.bsm`, so the generated C calls regular C functions:

    bpp --c examples/snake_raylib.bpp > snake.c
    gcc -I/opt/homebrew/include -L/opt/homebrew/lib \
        snake.c -o snake -lraylib -lobjc
    ./snake

The same `snake.c` should compile on any platform with raylib
installed (macOS, Linux, Windows, BSDs, Emscripten).

---

## 16. The Debugger

B++ includes `bug`, a native debugger built in B++ that speaks the
GDB remote serial protocol directly to `debugserver` (macOS) or
`gdbserver` (Linux). No GDB, no LLDB, no DWARF — it reads a
B++-defined `.bug` debug map the compiler writes alongside every
binary.

### 16.1 Two modes

    bug hello.bug              # dump mode — parse the .bug map
    bug ./hello                # observe mode — run under trace
    bug --break main ./hello   # observe with a selective breakpoint

Every compile writes a `.bug` file next to the binary. It is always
on — no `-g` switch. Keep the `.bug` alongside the binary; observe
mode reads it by appending `.bug` to the target path.

### 16.2 Dump mode

`bug file.bug` parses a `.bug` file and prints every section the
compiler emitted: functions (with parameter types), structs,
globals, externs, source map, address map, stack map.

### 16.3 Observe mode

`bug ./program [args...]` launches the target under `debugserver` or
`gdbserver` and connects via the GDB remote protocol. By default
every function entry is traced, indented to show the call tree:

    main
      setup_world
        spawn_enemy
      run_frame
        update_enemy
        render

Target stdout flows through unchanged.

### 16.4 Selective breakpoints

    bug --break update_enemy ./game
    bug --break func_a --break func_b ./target

Each `--break` adds one function to a 32-slot array. Only the named
functions register with the remote stub; everything else is
invisible. Combine with `--break-all` to re-enable the full trace:

    bug --break update_enemy --break-all ./game

### 16.5 Pointer-argument introspection

    bug --break my_fn --dump-str ./program

For each of `x0`/`x1` (ARM64) or `rdi`/`rsi` (x86_64), `bug` reads
up to 32 bytes from the register's pointer and prints the result
as a C string if the bytes are printable ASCII. The single most
valuable debugger flag — see `docs/debug_with_bug.md` for the
Apr 2026 case study where it caught a Mach-O writer bug in one hit.

---

## Appendix: Design Principles

**Public by default.** Write code, it works. Add `static` when the
project grows and you need module boundaries.

**Implicit return.** No boilerplate `return 0;`. Add `void` when
you want the compiler to catch "used void as value" bugs.

**Auto everything.** Auto storage class, auto phase classification,
auto type inference. Add explicit keywords (`extrn`, `global`,
`: base`, `: solo`) when you know better or want compiler
verification.

**Simple for simple things.** A 50-line snake game and a 10K-line
RTS use the same language, just different levels of annotation.
The language scales with the project — you never have to learn
everything upfront.

```
Level 1 (beginner):  auto x;        foo() { code; }
Level 2 (growing):   extrn assets;  void draw() { code; }
Level 3 (expert):    global world;  update(): base { code; }
```

---

*B++ was designed and implemented by Daniel Obino, 2026.*
*The compiler bootstrapped itself on March 20, 2026.*

*See `docs/journal.md` for the full development chronology and
`docs/how_to_dev_b++.md` for the production discipline.*
