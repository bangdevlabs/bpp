# B++ Language Reference — Keywords & Annotations

## Storage (top-level variables)

| Keyword | What it does |
|---------|-------------|
| `auto` | Default — serial, or smart-promoted to the best storage class by the compiler. If the compiler can prove the variable is written once before `maestro_run` and never after, it promotes to `extrn`. If workers touch it, promotes to `global`. Otherwise stays serial. The programmer doesn't need to think about this. |
| `global` | Worker-shared mutable. The variable can be read and written from worker threads (base phase). The programmer takes responsibility for stride safety. Use for game state that the maestro pattern dispatches over. |
| `extrn` | Frozen after init. Mutable before `maestro_run()` / `job_init()`, immutable forever after. Reads from any worker are safe because the memory never changes. Use for asset tables, config data, lookup tables loaded once at startup. |
| `const` | Compile-time literal. `const N = 10;` inlines the value at every use site. No runtime storage allocated. |

### Smart promotion (the B++ way)

The programmer can start with `auto` for everything. As the project grows and the maestro pattern is introduced, the compiler automatically promotes variables to the right class:

```bpp
// Level 1: just write code
auto world;
auto assets;
auto counter;

// Level 2: the compiler tells you what it did
// $ bpp --show-promotions game.bpp
//   assets -> extrn    (written once in main before maestro_run)
//   world  -> global   (read by base-phase worker function)
//   counter: auto      (no promotion — mutated in loop)

// Level 3: be explicit when you know better
extrn assets;          // "I know this is frozen"
global world;          // "I know workers touch this"
auto counter: serial;  // "don't promote, I want serial"
```

---

## Visibility (default: public)

| Keyword | What it does |
|---------|-------------|
| *(nothing)* | **Public** — any module can call this function or access this variable. This is the default. No keyword needed. |
| `static` | **Private to this module.** Only code in the same `.bsm` file can call a `static` function or access a `static` variable. Cross-module access is a compile-time error. |

```bpp
// wolf3d_ray.bsm
cast(x, y, angle) { ... }          // public — any module calls this
static _intersect(wall) { ... }    // private — only ray.bsm calls this

static extrn _tables;              // private + frozen
global ray_result;                  // public + worker-shared
```

---

## Return (default: implicit return 0)

| Keyword | What it does |
|---------|-------------|
| *(omit return)* | **Implicit return 0.** If the function body doesn't end with a `return` statement, the compiler inserts `return 0;` automatically. No boilerplate needed for side-effect functions like `init_game()`, `draw_hud()`, `apply_damage()`. |
| `void` | **Explicit "no useful return."** Marks a function as never returning a useful value. The compiler warns (W017) if a `void` function's return value is used: `x = draw_hud();` |

```bpp
// Level 1: just omit return — compiler handles it
draw_hud() {
    render_text("SCORE", 4, 4, 1, WHITE);
}

// Level 2: be explicit — compiler catches mistakes
void draw_hud() {
    render_text("SCORE", 4, 4, 1, WHITE);
}
auto x;
x = draw_hud();    // warning[W017]: 'draw_hud' is void

// Functions that return values: use return normally
add(a, b) {
    return a + b;   // explicit return — value is meaningful
}
```

---

## Override

| Keyword | What it does |
|---------|-------------|
| `stub` | **Placeholder function.** Declares a function that is meant to be overridden by the entry file (.bpp). The override is always silent — no duplicate warning. Used by engine modules (stbgame callbacks) and will be used by bangscript for generated scene functions. |

```bpp
// stbgame.bsm — engine provides stubs
stub game_init() { return 0; }
stub game_update(dt) { return 0; }

// mygame.bpp — programmer provides real implementations
game_init() {
    world = ecs_new(200);    // overrides stub silently
}
```

---

## Module

| Keyword | What it does |
|---------|-------------|
| `import` | **Engine/library module.** Searches `stb/`, `src/`, installed paths, then `/usr/local/lib/bpp/`. Use for standard library and engine modules. Visibility rules (static) are NOT enforced across imported modules — engine internals remain accessible. |
| `load` | **Project file.** Searches ONLY the entry file's directory. Use for your game's own modules. Visibility rules (static) ARE enforced between loaded modules. If the file is not found locally, it's a fatal error — no fallback to engine paths. |

```bpp
// wolf3d.bpp
load "wolf3d_ray.bsm";     // my project file — strict rules
load "wolf3d_enemy.bsm";   // my project file — strict rules
import "stbgame.bsm";       // engine — permissive rules
import "stbecs.bsm";        // engine — permissive rules
```

---

## Type

| Keyword | What it does |
|---------|-------------|
| `auto` | Default type is **word** (64-bit integer). B++ is typeless on top — all values are 64-bit words unless annotated with a slice hint. |
| `struct` | Define a named struct with named fields. Access fields with dot syntax: `player.x`. |
| `enum` | Define named constants: `enum { RED, GREEN, BLUE }`. |

---

## Control Flow

| Keyword | What it does |
|---------|-------------|
| `if` / `else` | Conditional execution. Binary decisions (2 branches). |
| `for` | C-style for loop: `for (init; cond; step) { body }`. |
| `while` | While loop: `while (cond) { body }`. |
| `switch` | Multi-way dispatch. Two forms: value dispatch `switch (expr) { V { } }` and condition dispatch `switch { cond { } }`. No fallthrough, no break, no case. Comma for multi-value: `A, B { }`. `else` for default. W021 if no `else`. Inherited from B (1969), corrected. |
| `break` | Exit the innermost loop immediately. |
| `continue` | Skip to the next iteration. In `for` loops, runs the step expression before re-testing the condition (C semantics). |
| `return` | Return a value from the current function. Bare `return;` supported in void functions. |
| `cond ? a : b` | Ternary conditional expression. Only the selected branch evaluates. Right-associative: `a ? b : c ? d : e` parses as `a ? b : (c ? d : e)`. |

**Short-circuit `&&` and `||`.** Both operators respect C-style short-circuit
semantics: the right operand evaluates ONLY when the left cannot determine
the result. `0 && expr` never runs `expr`; `1 || expr` never runs `expr`.
This is a language guarantee — all backends honour it because the parser
desugars `a && b` and `a || b` to a ternary node before codegen. Null-pointer
guards like `if (p != 0 && p.field > 0)` are memory-safe on every backend.

**Compile-time evaluation.** Integer-literal expressions fold at parse time:
`3 + 5` becomes `8` in the AST; `if (0) { ... }` and `while (0) { ... }` drop
their bodies; `if (1) { x } else { y }` keeps only `x`. These happen in the
frontend, so every backend sees already-simplified trees.

---

## Annotations (`:` syntax)

Annotations use the existing `:` syntax after a variable name or function parameters. They are NOT keywords — they are hints that give the compiler more information or override automatic decisions.

### Type/size annotations (after variable name)

The slice ladder runs from 1 bit to 128 bits, one unified system for
both integer and float types. Apply to struct fields (for packed
layout) or to local variables (for storage class hints).

| Annotation | What it does | Phase |
|------------|-------------|-------|
| `: bit` | 1 bit (boolean flag) | A1 |
| `: bit2` | 2 bits (0-3) | A1 |
| `: bit3` | 3 bits (0-7) | A1 |
| `: bit4` | 4 bits (0-15) | A1 |
| `: bit5` | 5 bits (0-31) | A1 |
| `: bit6` | 6 bits (0-63) | A1 |
| `: bit7` | 7 bits (0-127) | A1 |
| `: byte` | 8-bit unsigned integer (0-255) | (ship) |
| `: quarter` | 16-bit integer | (ship) |
| `: half` | 32-bit integer | (ship) |
| (default) | 64-bit word | (ship) |
| `: quarter float` | 16-bit float | (ship) |
| `: half float` | 32-bit float | (ship) |
| `: float` | 64-bit double | (ship) |
| `: double` | 128-bit SIMD (4× float32) | B4 |
| `: StructName` | Typed struct access (enables dot syntax) | (ship) |

```bpp
auto hp: byte;              // 8-bit health points
auto velocity: half float;  // 32-bit float velocity
auto player: Player;        // typed struct with dot access
auto flags: bit4;           // 4 flags packed in 4 bits (Phase A1)
auto lanes: double;         // 128-bit SIMD vector (Phase B4)
```

**Important — Rule 8:** When a pointer targets a sliced struct, always
use typed access (`auto p: Struct; p.field`). Raw offset `*(ptr + N)`
does NOT respect the packed layout and will read wrong memory. See
`docs/tonify_checklist.md` Rule 8.

### Storage annotation (after top-level variable name)

| Annotation | What it does |
|------------|-------------|
| `: serial` | Prevent auto promotion. The variable stays `auto` (serial) even if the compiler thinks it could be promoted to `extrn` or `global`. Escape hatch for intentionally sequential variables. |

```bpp
auto debug_counter: serial;  // "I know what I'm doing, don't promote"
```

### Phase annotations (after function parameters)

B++ has a six-level effect lattice. Five names are writable as
annotations (`base`, `solo`, `realtime`, `io`, `gpu`); `heap` and
`panic` exist in the lattice but are inferred, never annotated.

```
PANIC  ≤  BASE  ≤  { IO, GPU, HEAP, REALTIME }  ≤  SOLO
```

The join of a function's body is what the compiler records. Calling
a higher-phase function lifts the caller unless the caller already
has a stricter annotation, in which case the compiler warns.

| Annotation | Code | Meaning |
|------------|------|---------|
| `: base`     | 1 | Pure — no global writes, no impure calls. Worker-safe. |
| `: solo`     | 2 | Any side effect (catch-all). Must run sequentially. |
| `: realtime` | 3 | Audio-callback safe: no malloc, no IO, bounded time. |
| `: io`       | 4 | Touches files / network / stdout / stderr. |
| `: gpu`      | 5 | Touches GPU resources (render thread only). |
| *(no annotation)* | — | Auto-classified by propagation from the body. |

The compiler verifies every annotation. If the function body's
inferred effect is stricter than the annotation, the hint is
redundant (and may be removed). If it's more permissive, the
compiler emits **W013** ("declared `: base` but calls `malloc`"),
**W026** (effect escalation), or similar.

`PHASE_HEAP` (transient allocation) and `PHASE_PANIC` (`sys_exit`,
never-returns) exist below the user-facing phases and let the
compiler reason about abort paths and allocation-only leaves
without forcing the programmer to annotate them.

```bpp
// Compiler auto-classifies (no annotation needed):
helper(x) { return x * 2; }         // inferred PHASE_BASE

// Programmer asserts and compiler verifies:
particle_update(i): base {           // W013 if body not pure
    ecs_set_pos(world, i, new_x);
}

// Programmer forces sequential:
critical_section(): solo {           // no auto-dispatch to workers
    update_global_state();
}

// Audio callback constraint:
mixer_fill(buf, n): realtime {       // W013 if body mallocs or syscalls
    synth_render(buf, n);
}
```

---

## SIMD (`: double` and `vec_*` builtins)

128-bit SIMD lives in the language as the widest rung of the slice
ladder. A local declared `: double` reserves a 16-byte, 16-byte-aligned
stack slot holding four 32-bit floats. Eleven `vec_*` builtins operate
on those slots and lower to NEON on ARM64 and SSE2 on x86_64 — no
library call, no runtime feature check.

### Declaring a SIMD local

```bpp
auto v: double;           // four float32 lanes, 16-byte aligned
v = vec_splat4(2.0);      // every lane = 2.0
```

`: double` also works as a struct field. Field offsets are computed
in 16-byte steps, so a mixed struct lays out cleanly:

```bpp
struct Particle {
    pos: double,          // offset  0, 16 bytes
    vel: double,          // offset 16, 16 bytes
    mass: half float,     // offset 32,  4 bytes
    id,                   // offset 40,  8 bytes
}
```

Access through a typed local (`auto p: Particle; p.pos = ...`)
respects the packed layout. Raw `*(ptr + N)` does NOT — see
Rule 8 in `tonify_checklist.md`.

### Builtins

All eleven return or consume values in a SIMD register (v0 on ARM64,
xmm0 on x86_64). Assigning a SIMD result to a non-`: double` local is
error E231.

| Builtin | Meaning |
|---------|---------|
| `vec_load4(ptr)` | Load 4 float32 lanes from memory (unaligned-safe). |
| `vec_store4(ptr, v)` | Store 4 float32 lanes to memory (unaligned-safe). |
| `vec_splat4(x)` | Broadcast scalar `x` to every lane. Int or float scalar; float is converted to single precision. |
| `vec_add4(a, b)` | Lane-wise addition. |
| `vec_sub4(a, b)` | Lane-wise subtraction. |
| `vec_mul4(a, b)` | Lane-wise multiplication. |
| `vec_div4(a, b)` | Lane-wise division. |
| `vec_min4(a, b)` | Lane-wise minimum. |
| `vec_max4(a, b)` | Lane-wise maximum. |
| `vec_dot4(a, b)` | Horizontal sum of `a * b`. Returns scalar float. |
| `vec_get4(v, i)` | Extract lane `i` (0..3) as a scalar float. |

```bpp
auto a: double;
auto b: double;
a = vec_splat4(2.0);
b = vec_splat4(3.0);
a = vec_add4(a, b);                        // [5, 5, 5, 5]
auto lane;
lane = vec_get4(a, 2);                     // 5.0
auto dot;
dot = vec_dot4(a, vec_splat4(1.0));        // 20.0 (sum of four 5s)
```

### Alignment

- **Stack slots** (`auto v: double;`) are always 16-byte aligned.
  The compiler rounds the absolute frame offset, not just the
  relative one — verified by `test_vec_align_stack.bpp`.
- **Heap buffers** passed to `vec_load4` / `vec_store4` do NOT need
  to be 16-byte aligned. The native backends always emit the
  unaligned form (LDR Q on ARM64, MOVUPS on x86_64), so both
  `malloc_aligned(size, 16)` and plain `malloc(size)` work.
- For consumers that care about perf (audio DSP, SIMD-heavy inner
  loops), prefer `malloc_aligned(size, 16)`. The 16-byte alignment
  is also the default `malloc` alignment since Phase A2, so in
  practice unaligned access only happens when pointer arithmetic
  offsets into the middle of a buffer.

### Baseline ISA

- **ARM64**: ARMv8 base. FADD/FMUL/FDIV/FMIN/FMAX `.4S`, DUP,
  FADDP, LDR Q / STR Q. No ARMv8.1+ extensions required.
- **x86_64**: SSE2 only (part of the x86_64 baseline). ADDPS,
  MULPS, MOVUPS, SHUFPS, MOVHLPS. No SSE4.1 DPPS, no AVX, no
  runtime feature detection. Any x86_64 chip runs the binary.

### C backend

The C transpile path (`bpp --c`) does NOT currently map `: double`
or `vec_*` — it requires typing every local that holds a SIMD value
as `__m128`, which is an invasive change to the emitter. Programs
that use SIMD and go through the C backend will fail at the
extern-lookup stage, which is the correct signal. Native `bpp`
(ARM64 / x86_64) is the supported SIMD target; C-backend support
is tracked in `docs/todo.md`.

---

## Operators

### Arithmetic

| Operator | Meaning |
|----------|---------|
| `+` `-` `*` `/` `%` | Standard binary arithmetic. Integer or float depending on operand types. Integer division truncates toward zero. |
| `-x` | Unary negation. |
| `~x` | Bitwise NOT. |
| `!x` | Logical NOT: `1` if `x == 0`, else `0`. |

### Comparison

| Operator | Meaning |
|----------|---------|
| `==` `!=` | Equality / inequality. |
| `<` `<=` `>` `>=` | Ordered comparison. Signed on integers, IEEE on floats. |

All comparisons return `0` (false) or `1` (true) as a 64-bit word.

### Bitwise and shift

| Operator | Meaning |
|----------|---------|
| `&` `\|` `^` | Bitwise AND, OR, XOR. Binary form only. Unary `&x` is address-of. |
| `<<` `>>` | Logical left / right shift. Shift amount is taken modulo the word width. |

### Logical (short-circuit)

| Operator | Meaning |
|----------|---------|
| `&&` | Logical AND. Right side evaluates only when left is non-zero. |
| `\|\|` | Logical OR. Right side evaluates only when left is zero. |

The parser rewrites `a && b` to `a ? b : 0` and `a || b` to `a ? 1 : b`
before codegen, so every backend honours short-circuit semantics.

### Memory

| Operator | Meaning |
|----------|---------|
| `*p` | Dereference: read the 64-bit word (or sized slice) at address `p`. |
| `&x` | Address-of: take the address of a local, global, or struct field. The result is a pointer (64-bit word). |
| `a[i]` | Indexed load. Equivalent to `*(a + i * stride)` where `stride` is 8 for plain words or the slice size for typed arrays. |
| `s.f` | Struct field access on a typed local (`auto s: T;`). Respects the packed layout. |

### Assignment

| Operator | Meaning |
|----------|---------|
| `=` | Plain assignment. |
| `+=` `-=` `*=` `/=` `%=` | Compound arithmetic assignment. |
| `&=` `\|=` `^=` `<<=` `>>=` | Compound bitwise assignment. |

Compound assignments evaluate the left operand once.

### Ternary

| Operator | Meaning |
|----------|---------|
| `c ? a : b` | Conditional expression. Only the selected branch evaluates. Right-associative. |

---

## Address-of (`&`)

The unary `&` operator takes the address of a local, a global, or a
struct field. The result is a 64-bit word — a plain pointer. Used
anywhere a syscall or builtin needs an out-parameter that writes
into an existing variable.

```bpp
auto avail;
sys_ioctl(fd, 0x541B, &avail);       // FIONREAD writes into avail

auto tv;
sys_clock_gettime(0, &tv);            // kernel fills the timespec

auto p: Particle;
init_particle(&p);                    // callee writes through the pointer
```

`&x` replaces the older `malloc(8)` scratch pattern — prefer it in
every new syscall wrapper and out-parameter API.

---

## Literals

### Integer

- Decimal: `42`, `-17`, `1000000`
- Hexadecimal: `0x1F`, `0xFF00AA55`
- Octal: `0755` (leading zero)
- Character: `'A'` (resolves to the ASCII code, 65)

All integer literals are 64-bit words.

### Float

- Decimal with fractional part: `1.0`, `3.14159`, `0.5`
- Scientific notation: `1.5e3`, `2.0e-7`

Float literals default to 64-bit double. They retain IEEE bits only
when assigned to a `: float`, `: half float`, or `: quarter float`
local. An `auto x = 44100.0;` **silently truncates to int** and
produces diagnostic **E232** — always annotate float locals.

### String

- Double-quoted: `"hello"` — produces a pointer to a null-terminated
  byte sequence in the data section.
- Escape sequences: `\n`, `\r`, `\t`, `\\`, `\"`, `\0`, `\xHH`.

```bpp
putchar_err_str("boot failed: ");
auto msg;
msg = "hello\n";
```

Strings are immutable. The compiler deduplicates identical literals
into the same data-section slot.

---

## Comments

| Form | What it does |
|------|-------------|
| `// …` | Line comment. Extends to end of line. |
| `/* … */` | Block comment. Does **not** nest. |

Tonify Rule 9 requires one blank comment line between a function's
intro comment and its first body statement, for readability in
generated documentation.

---

## Core builtins

Every B++ program gets these built-in names without any import.
They are resolved in the frontend, emitted inline by the codegen,
and classified in the effect lattice.

### Memory

| Builtin | Effect | Meaning |
|---------|--------|---------|
| `peek(p)` | base | Read one byte at address `p` (0..255). |
| `poke(p, v)` | solo | Write byte `v` at address `p`. Lives in bpp_defs but effectively base-safe on the write. |
| `str_peek(p, i)` | base | Read byte `i` of a string pointer. |
| `malloc(n)` | heap | Allocate `n` bytes; returns a pointer or 0. 16-byte aligned since Phase A2. |
| `malloc_aligned(n, a)` | heap | Allocate with explicit alignment. |
| `realloc(p, n)` | heap | Resize an allocation; may move. |
| `free(p)` | heap | Release an allocation. |
| `memcpy(dst, src, n)` | base | Copy `n` bytes. |
| `mem_barrier()` | base | Compiler-level memory barrier (no CPU fence). |
| `fn_ptr(name)` | base | Produce a function pointer from a symbol name. |

### I/O

| Builtin | Effect | Meaning |
|---------|--------|---------|
| `putchar(c)` | io | Write byte to stdout. |
| `putchar_err(c)` | io | Write byte to stderr. |
| `getchar()` | io | Read one byte from stdin; `-1` on EOF. |

### Raw syscalls

Available directly to avoid stdlib wrappers when bootstrapping or
writing platform code. All are `: io` or `: solo` depending on
side effect. Available on both macOS (arm64) and Linux (x86_64):

```
sys_open    sys_close   sys_read    sys_write
sys_lseek   sys_mmap    sys_munmap  sys_ioctl
sys_mkdir   sys_unlink  sys_fchmod  sys_getdents
sys_fork    sys_execve  sys_wait4   sys_waitpid
sys_exit    sys_ptrace
sys_clock_gettime  sys_nanosleep  sys_usleep
```

Syscall numbers are resolved per-OS by the backend — the name is
the same portable surface everywhere.

### SIMD

`vec_splat4`, `vec_load4`, `vec_store4`, `vec_add4`, `vec_sub4`,
`vec_mul4`, `vec_div4`, `vec_min4`, `vec_max4`, `vec_dot4`,
`vec_get4` — see the SIMD section above.

---

## Design Principles

**Public by default.** Write code, it works. Add `static` when the project grows and you need module boundaries.

**Implicit return.** No boilerplate `return 0;`. Add `void` when you want the compiler to catch "used void as value" bugs.

**Auto everything.** Auto storage class, auto phase classification, auto type inference. Add explicit keywords (`extrn`, `global`, `: base`, `: solo`) when you know better or want compiler verification.

**Simple for simple things.** A 50-line snake game and a 10K-line RTS use the same language, just different levels of annotation. The language scales with the project — you never have to learn everything upfront.

```
Level 1 (beginner):      auto x;           foo() { code; }
Level 2 (growing):       extrn assets;     void draw() { code; }
Level 3 (expert):        global world;     update(): base { code; }
```
