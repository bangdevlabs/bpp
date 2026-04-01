# B++ Bootstrap Journal

## 2026-03-18 — Stage 2 Complete: Self-Hosting Parser

### Milestone

The B++ bootstrap compiler is now at **Stage 2 of 3** toward self-hosting.
The full pipeline works end-to-end:

```
source.bpp  -->  bpp_lexer (B++)  -->  token stream  -->  bpp_parser (B++)  -->  JSON IR
```

Both the lexer and parser are written in B++ itself, compiled through the
Python compiler, and produce output identical to the Python compiler's IR.

### What was built

| Component | File | Status |
|-----------|------|--------|
| Python compiler (lexer + parser + IR) | `bpp_compiler.py` | Working — reference implementation |
| Python emitter (raylib demo) | `test_suite2.py` | Working — raylib cube demo runs |
| B++ lexer (Stage 1) | `bootstrap/lexer.bpp` | Working — tokenizes all .bpp files |
| B++ lexer build script | `bootstrap/build_lexer.py` | Working |
| B++ parser (Stage 2) | `bootstrap/parser.bpp` | Working — parses all .bpp files |
| B++ parser build script | `bootstrap/build_parser.py` | Working |

### Key technical decisions

- **Memory model**: `uint8_t mem_base[1MB]` with bump allocator. All B++ values
  are `long long`. Byte access via `peek()`/`poke()` built-ins.
- **Pack/unpack**: String references stored as `(start, len)` pair in one
  `long long` using `s + l * 0x100000000`.
- **C helper injection**: JSON emitter uses C functions (`jot`, `jkp`, `jopen`)
  injected by the build script to avoid `\"` escape sequences, which B++ string
  literals do not support.
- **Forward declarations**: B++ stubs (`parse_statement() { return 0; }`) are
  overwritten by later definitions since the Python compiler stores functions in
  a dict (last definition wins). C forward declarations generated automatically.
- **Buffer sizing**: Token buffer needs 262KB+ for large files (~5000+ tokens
  at 24 bytes each). Fixed after overflow was found when parser parsed itself.

### Bugs fixed during bootstrap

1. **`*` missing from tokenizer regex** — multiplication and dereference silently
   dropped. Fixed in `bpp_compiler.py`.
2. **No operator precedence** — flat left-to-right parsing. Fixed with
   precedence climbing in both Python compiler and B++ parser.
3. **Emitter double-wrapping memory expressions** — string heuristic caused
   nested `mem_base + (mem_base + ...)`. Fixed with proper `mem_load`/`mem_store`
   IR nodes.
4. **String escaping in B++** — B++ tokenizer terminates strings at first `"`,
   no escape support. Fixed by using C helper functions for JSON output.
5. **Token buffer overflow** — 65KB too small for ~5000 tokens. Increased to 262KB.

### Validation

- `lexer.bpp`: 19/19 functions match Python compiler IR (normalized).
- `parser.bpp`: Successfully parses itself (41 functions, 38 globals, 5 externs).
- `raylib_demo2.bpp`: Parses correctly (1 function, 9 globals).

---

---

## 2026-03-19 — Type Inference + Dispatch Analysis

### Milestone

B++ now has a type inference engine and a dispatch analyzer — the two core
modules that make B++ more than "C without types". The compiler can infer
the smallest type for every variable and classify loops for parallel/GPU
execution, all without the programmer writing a single type annotation.

### Architecture decisions

- **No header files** — `import "file.bpp";` reads, parses, and merges
  another B++ file's IR. `import "lib" { ... }` declares FFI functions.
  Circular import protection included. Inspired by Go modules and C++20
  modules, but simpler (B++ has no legacy to support).
- **Optional type annotations** — the programmer writes `auto x = 10;`
  (compiler infers byte) or `word x = 10;` (programmer overrides). The
  escape hatch exists but is rarely needed.
- **Unified compiler** — no JSON between stages in the final product.
  Pipeline is lex → parse → type → dispatch → codegen in one process.
- **Separate type and dispatch modules** — types.bpp answers "what is this
  data?", dispatch.bpp answers "where should this code run?". Both feed
  into codegen but evolve independently.

### What was built

| Component | File | Status |
|-----------|------|--------|
| Module import system | `bpp_compiler.py` | Working — file import + FFI import |
| Type inference (5 types) | `bpp_compiler.py` | Working — byte/half/word/long/ptr |
| Dispatch analysis | `bpp_compiler.py` | Working — seq/parallel/gpu/split |
| Shared constants | `bootstrap/defs.bpp` | Working — 30 globals, 1 function |
| Type inference (B++) | `bootstrap/types.bpp` | Parses — 11 functions, 36 globals |
| Dispatch analysis (B++) | `bootstrap/dispatch.bpp` | Parses — 14 functions, 40 globals |

### Type system

Five primitive types inferred from context:

| Type | Size | Inferred when |
|------|------|---------------|
| byte | 8-bit | Literal 0-255, `peek()` return |
| half | 16-bit | Literal 256-65535 |
| word | 32-bit | Literal 65536-4G, `int` FFI return |
| long | 64-bit | Literal > 4G, `mem_load` (dereference) |
| ptr | 64-bit | `malloc()` return, string literals, pointer arithmetic |

Rules: binary ops promote to larger type. Ptr wins everything. Variables
take the largest type seen across all assignments in the function.

### Dispatch analysis

Four dispatch classifications for while-loops:

| Code | Meaning | Trigger |
|------|---------|---------|
| sequential | Must run in order | Side effects (extern calls, global writes) |
| parallel | CPU parallelizable | No side effects, independent iterations |
| gpu_candidate | Can run on GPU | Parallel + strided memory access |
| split_candidate | Mixed — split update/draw | Strided access but some side effects |

Tested on `raylib_demo2.bpp` (10-cube bouncing demo):
- Init loop: **gpu_candidate** (strided `OBJS + i * 40`, no side effects)
- Game loop: **sequential** (BeginDrawing, ClearBackground, EndDrawing)
- Inner object loop: **split_candidate** (7 pure stmts + 1 DrawRectangle)

### Bugs fixed

6. **Type keyword / variable name ambiguity** — `ptr` is both a type keyword
   and a valid variable name. `ptr = expr;` was parsed as type declaration.
   Fixed by checking for `=` after type keywords to disambiguate.

### Future type system expansion

```
Layer 1 (now):  byte, half, word, long, ptr
Layer 2 (next): vec2, vec4, matrix, fixed (game math)
Layer 3 (future): handle (coroutine/entity), buffer (GPU memory)
```

---

## Next Steps — Stage 3: Code Generation

### Option A: C Emitter in B++ (simplest path to self-hosting)

Write a B++ program (`bootstrap/emitter.bpp`) that reads JSON IR from stdin and
outputs C code to stdout. This would complete the self-hosting chain:

```
source.bpp | bpp_lexer | bpp_parser | bpp_emitter > output.c
clang output.c -o output
```

The emitter is the simplest stage — it just walks the IR JSON and prints C code.
The Python `build_lexer.py` / `build_parser.py` already contain the emitter
logic in `translate_node()` and `generate_c()`.

### Option B: ARM64 Native Code Generator

Replace the C backend with direct ARM64 (AArch64) machine code emission:

```
source.bpp | bpp_lexer | bpp_parser | bpp_codegen > output.o
```

This eliminates the dependency on clang entirely. Key considerations:

- **Calling convention**: ARM64 uses x0-x7 for arguments, x0 for return value,
  x19-x28 are callee-saved. Stack alignment must be 16 bytes.
- **Memory model**: Can keep the `mem_base` array approach, or transition to
  real pointers. Starting with mem_base is safer.
- **Output format**: Mach-O object file on macOS, or ELF on Linux. Could also
  emit assembly text and use `as` to assemble.
- **Simplest start**: Emit ARM64 assembly text (`.s` file), assemble with `as`,
  link with `ld`. Avoids dealing with binary object file formats initially.
- **Registers**: Since B++ has no types (everything is `long long`), all values
  fit in 64-bit registers. Use a simple register allocator or stack-based approach.

### Recommended order

1. **Stage 3A**: C Emitter in B++ (complete self-hosting via C backend)
2. **Stage 3B**: ARM64 codegen (eliminate clang dependency)
3. **Stage 4**: Bootstrap — compile the compiler with itself

Stage 3A is faster and proves the full pipeline works. Stage 3B can then replace
the C backend without changing the frontend.

---

## 2026-03-19 — Stage 3: C Emitter in B++

### Milestone

The B++ bootstrap compiler now has a C code emitter written in B++
(`bootstrap/emitter.bpp`). The full self-hosting pipeline works end-to-end:

```
source.bpp  -->  bpp_lexer  -->  bpp_parser  -->  bpp_emitter  -->  output.c
```

Tested on `lexer.bpp`: the emitter output compiles with clang and produces a
working lexer binary identical in behavior to the original.

### What was built

| Component | File | Status |
|-----------|------|--------|
| C emitter (B++) | `bootstrap/emitter.bpp` | Working — ~620 lines |
| Emitter build script | `bootstrap/build_emitter.py` | Working — builds native `bpp_emitter` |

### How the emitter works

1. **JSON reader**: Reads IR from stdin character by character. Handles strings
   with escape sequences, nested objects/arrays, null values.
2. **IR parser**: Builds in-memory AST nodes (64-byte fixed-size, same layout
   as parser). Handles all 13 node types. Forward declaration dedup (keeps last
   definition).
3. **Used extern scanner**: Walks all function bodies to find which extern
   functions are actually called (for `#include` generation).
4. **C emitter**: Walks AST and outputs C with proper indentation. Handles FFI
   type casts, pointer conversion, built-in functions.

### Key technical decisions

- **JSON parsing in B++**: No general-purpose JSON library — the parser knows
  the exact IR structure and reads fields in order. Uses `jnext_key()` helper
  that handles comma + key + colon in one call. `jskip_str()` avoids wasting
  string buffer on discarded key names.
- **Forward declaration dedup**: B++ uses stub-then-real pattern
  (`parse_statement() { return 0; }` followed by real body). The emitter's
  `find_func()` detects duplicates and overwrites the stub.
- **FFI pointer conversion**: Pointer-type args (`char*`, `void*`, `ptr`,
  `word`) get `(const char*)(mem_base + offset)`. String literal args detected
  by checking if AST node is `T_LIT` starting with `"` — these get
  `(const char*)` instead.
- **Built-in mapping**: `malloc` → `bpp_alloc`, `peek` → byte read from
  `mem_base`, `poke` → byte write, `|` → `||` in C.
- **C output helpers**: `out()` and `print_buf()` injected by
  `build_emitter.py` — avoids escape sequence issues in B++ string literals.

### Known limitations

- **File imports**: `import "defs.bpp";` is resolved by the Python compiler
  only. The `bpp_parser` doesn't read files. Files with file imports need:
  `python3 bpp_compiler.py file.bpp | bpp_emitter > out.c`
- **Injected helpers**: `parser.bpp` depends on `jot`/`jkp`/`jopen` from its
  build script — can't go through the generic emitter pipeline.
- **Cosmetic differences**: Emitter output differs from Python emitter (no
  comments, different global order) but is functionally equivalent.

### Bugs fixed

7. **Duplicate function definitions** — Forward declaration stubs emitted
   alongside real bodies caused C redefinition errors. Fixed with `find_func()`
   dedup in `parse_one_func()`.

### Validation

- `lexer.bpp`: Full pipeline produces C that compiles and runs correctly.
- `emitter.bpp`: Python compiler IR → `bpp_emitter` → C output compiles.
- Build system: `build_emitter.py` compiles `emitter.bpp` to native
  `bpp_emitter` via clang.

---

---

## 2026-03-19 — Fat Structs

### Milestone

B++ now supports structs with named field access, `sizeof`, and nested structs.
This is purely syntactic sugar — the parser lowers struct operations to
`mem_load`/`mem_store` with computed offsets before types or dispatch see them.

### Syntax

```
struct Vec2 { x, y }
struct Rect { pos: Vec2, w, h }

auto p: Vec2;
p = MemAlloc(sizeof(Vec2));
p.x = 100;
p.y = 200;

auto r: Rect;
r.pos.x = 10;   // nested access chains offsets
```

### How it works

- `p.x` → `mem_load(ptr: binop(p + 0))`, `p.y` → `mem_load(ptr: binop(p + 8))`
- `sizeof(Vec2)` → compile-time constant `16` (2 fields × 8 bytes)
- Nested structs: `r.pos.x` → offset 0 (pos offset 0 + x offset 0)
- Type annotations on `auto`, function params, and globals
- Struct info exported in IR as `ir["structs"]` for downstream tools
- Zero changes needed in types, dispatch, or emitter — they see plain
  `mem_load`/`mem_store`

### Validated by

- `snake_v2.bpp`: rewrote `snake_demo.bpp` using `Vec2` struct — same
  semantics, cleaner code (`head.x` instead of `*(head + 0)`)

---

---

## 2026-03-19 — Standard B (stb) Library + Snake v3

### Milestone

B++ now has a standard library called **Standard B (stb)** in `stb/`. Snake v3
is a full game using the complete stb stack — the first real program built
entirely on B++'s own libraries.

### Modules

| Module | Purpose | External dep |
|--------|---------|-------------|
| `stbmath.bpp` | Vec2, random, abs/min/max/clamp | raylib (MemAlloc, GetRandomValue) |
| `stbinput.bpp` | Keyboard + mouse input | raylib (IsKey*, GetMouse*) |
| `stbdraw.bpp` | Drawing, colors, text, numbers | raylib (Draw*, Clear*, etc.) |
| `stbui.bpp` | Immediate-mode GUI (label, button, panel, bar) | none — pure B++ |
| `stbstr.bpp` | String ops (len, eq, cpy, cat, chr, dup) | none — pure B++ (peek/poke) |
| `stbio.bpp` | Console + file I/O | **libc — BLOCKED** |
| `stbgame.bpp` | Game bootstrap (chains all inits) | raylib (InitWindow, etc.) |

### Key decisions

- **No libc in game libs**: Replaced `malloc` → `MemAlloc` and `rand` →
  `GetRandomValue` from raylib in all game modules.
- **Compiler auto-discovery**: `_STB_DIRS = ["stb"]` in the compiler resolves
  `import "stbmath.bpp"` to `stb/stbmath.bpp` automatically.
- **Import dedup**: Compiler tracks `_imported` set — no double-processing.
- **draw_number**: Converts int to string internally using peek/poke, hidden
  from the API. User just calls `draw_number(x, y, sz, color, val)`.

### Snake v3 features

- Full game loop with stbgame
- Directional input via named constants (KEY_UP, KEY_RIGHT, etc.)
- In-memory top-5 ranking (sorted insertion)
- On-screen game-over panel with ranking display (stbui + stbdraw)
- Console ranking printout (stbio)
- Restart with SPACE

### Shim fixes (test_suite_snake_v3.py)

- `g_` prefix for all B++ globals (avoids raylib macro collisions)
- `bpp_` prefix for all B++ functions (avoids C stdlib name collisions)
- `BPP_STR` macro: runtime heuristic to distinguish real pointers from
  mem_base offsets — solves the dual-pointer problem when string literals
  pass through B++ function parameters
- String literal cast: `(long long)"text"` for passing strings through
  B++ functions typed as `long long`

### Bootstrap lexer updates

- Added `.` (46) and `:` (58) to `is_punct`
- Added `struct`, `sizeof`, `ptr`, `byte`, `half`, `long` to keyword list

---

---

## 2026-03-19 — I/O Design Decision: Syscalls, Not libc

### Problem

`stbio.bpp` is the last module with a libc dependency (`putchar`, `puts`,
`fopen`, `fread`, `fwrite`, `fclose`). Two approaches were rejected:

1. **Keep `import "libc"`** — exposes C dependency in B++ source code.
2. **Hide libc behind compiler built-ins** — compiler emits `stdio.h` calls
   invisibly. Still depends on C runtime.

### Decision

**I/O must be a fundamental B++ capability implemented via raw syscalls.**
No libc, no raylib, no hidden C.

> "IO é basico pra qualquer linguagem e tem que ser raiz do bpp"

### Design

- macOS ARM64 syscalls: `write` (4), `read` (3), `open` (5), `close` (6)
- `putchar`/`getchar` become thin wrappers over `write`/`read` on fd 0/1
- stbio.bpp builds `print_int`, `print_msg`, etc. in pure B++ on top of
  these primitives
- During C-emit stage: inline asm (`svc #0x80`) or linked `.s` file
- After ARM64 codegen: native syscall instructions directly

### Status

Design decided. Implementation pending. This is on the critical path to
making snake_v3 run without any libc dependency and toward full bootstrap.

---

---


## 2026-03-20 — Self-Hosting Achieved

### Milestone

The B++ compiler compiles itself. Fixed point verified: emitter_v2 compiles
emitter.bpp producing emitter_v3.c, which is identical to emitter_v2.c.

All three bootstrap stages (lexer, parser, emitter) are now libc-free.
Generated C uses only `#include <stdint.h>` and inline ARM64 syscall stubs.

### What was done

- Removed all libc from build_*.py shims (stdio.h, string.h, strncmp)
- Added syscall runtime to all build scripts (bpp_sys_write, bpp_sys_read)
- Added built-in handlers: putchar, getchar, str_peek in all build_*.py
- Rewrote parser.bpp: jot/jkp/jopen as pure B++ (were injected C helpers)
- Rewrote emitter.bpp: fully self-contained (~1200 lines), zero imports
  - out(), buf_eq(), print_buf() as native B++ functions
  - emit_runtime() emits syscall stubs in pure B++
  - emit_q()/emit_reg() helpers for inline asm emission
  - Global deduplication in emit_globals()
- Added file I/O built-ins: sys_open, sys_read, sys_close, sys_write
- Added argv access: argc_get(), argv_get(i)
- Created ./bpp compiler driver (shell script)
- Wrote bootstrap/import.bpp — Stage 0 import resolver

---

---

## 2026-03-20 — Snake v3 Compiles via B++ Pipeline

### Milestone

Snake v3 compiles and runs through the full B++ pipeline, no Python involved:

```
./bpp snake_v3.bpp -o snake_v3 [clang flags]
./snake_v3
```

This is the first real multi-file game compiled end-to-end by the B++ compiler.
The 4-stage pipeline works: `bpp_import → bpp_lexer → bpp_parser → bpp_emitter`.

### Bugs fixed

8. **macOS syscall carry flag** — On macOS ARM64, `svc #0x80` sets the carry
   flag on error and returns positive errno in x0. `if (fd < 0)` never triggered
   because errno 2 (ENOENT) looks like fd 2 (stderr). Fixed by adding
   `csneg x0, x0, x0, cc` after `svc #0x80` in all syscall stubs. This negates
   x0 when carry is set, giving Linux-style negative errno. Applied in:
   - `emitter.bpp` emit_runtime() (4 stubs: write, read, open, close)
   - `build_emitter.py`, `build_lexer.py`, `build_parser.py` (2 stubs each)

9. **Import resolver recursion** — `fbuf` was a single global buffer overwritten
   by recursive imports (e.g., snake_v3 → stbgame → stbinput). Fixed with
   stack-based allocation: `fbuf_top` tracks the current position, each call
   saves/restores it. Path also copied into the fbuf stack so it survives
   child calls clobbering `pathbuf`.

10. **Raylib name collisions** — `#include <raylib.h>` defines `KEY_UP`, `BLACK`,
    etc. as enums/macros that collide with B++ globals. Fixed by having the
    emitter forward-declare extern functions directly instead of including
    library headers:
    ```c
    // Before (broken):
    #include <raylib.h>
    long long KEY_UP = 0;  // ERROR: redefines enum

    // After (works):
    int IsKeyDown(int);
    void DrawRectangle(int, int, int, int, int);
    long long KEY_UP = 0;  // No conflict
    ```

11. **Parser splits `char*` into two tokens** — The lexer tokenizes `char*` as
    `char` + `*`, and the parser pushed them as separate FFI argument types.
    `DrawText` got args `[char, *, int, int, int, int]` instead of
    `[char*, int, int, int, int]`. Fixed in `parse_import()`: after reading a
    type token, if the next token is `*`, merge them into a pointer type by
    copying both into vbuf.

12. **Extern return type cast** — `MemAlloc` returns `void*` but B++ variables
    are `long long`. The emitter now wraps extern calls that return pointer
    types in `(long long)` cast.

13. **MemAlloc vs malloc** — Raylib's `MemAlloc` returns a real heap pointer,
    but B++ uses everything as mem_base offsets. `mem_base + real_pointer` =
    segfault. Replaced `MemAlloc` with `malloc` (mapped to `bpp_alloc`) in all
    stb modules and snake_v3.bpp.

14. **BPP_STR for extern args** — String values passed through B++ function
    parameters to extern calls were wrapped in `(const char*)(mem_base + v)`.
    But if the value is a real pointer (from a string literal cast to
    `long long`), this overflows. Fixed: non-literal pointer args now use
    `BPP_STR(v)` which auto-detects real pointers vs mem_base offsets.

15. **Color byte order** — Colors were stored as `0xRRGGBBAA` (human-readable)
    but raylib's `Color` struct on little-endian ARM64 needs `0xAABBGGRR`.
    `BLACK = 0x000000FF` was interpreted as `{r=255, g=0, b=0, a=0}` (red with
    zero alpha). Fixed with `rgba(r, g, b, a)` helper that computes the correct
    byte packing via `r + g*256 + b*65536 + a*16777216`.

### Key technical decisions

- **Forward declarations over includes** — The emitter now forward-declares
  extern functions with their FFI-specified types. This is more portable (no
  header dependency) and avoids all namespace collisions.
- **BPP_STR everywhere** — The dual-pointer problem (string literal pointer vs
  mem_base offset) is resolved uniformly via the BPP_STR macro for all extern
  pointer arguments.
- **malloc is a built-in** — B++ `malloc` maps to `bpp_alloc` (bump allocator
  in mem_base). External allocators like `MemAlloc` return incompatible pointers.

### Validation

- `snake_v3.bpp`: Compiles and runs via `./bpp snake_v3.bpp`
- Self-hosting: Fixed point still holds (v2 == v3) after all changes
- Import resolver: Handles recursive imports (snake_v3 → stbgame → stbinput → ...)

### Next steps

1. ARM64 native codegen (eliminate clang dependency)
2. UI Phase 2 (closures, declarative blocks)
3. Backend x86_64

---

---


## 2026-03-22 — Bitwise Operators + Dispatch Hints

### Milestone

B++ now has native bitwise operators and programmer dispatch hints.
The language is no longer limited to arithmetic-only bit manipulation.

### Bitwise operators added

Six new operators follow C precedence (highest to lowest):

| Operator | Precedence | Meaning |
|----------|-----------|---------|
| `<<` `>>` | 8 | Left/right shift |
| `&` | 5 | Bitwise AND |
| `^` | 4 | Bitwise XOR |
| `|` | 3 | Bitwise OR |
| `&&` | 2 | Logical AND (now exposed in parser) |
| `||` | 1 | Logical OR (unchanged) |

**Breaking change**: `|` is now bitwise OR, not logical OR. Use `||` for
logical OR. Existing code is safe because all prior uses of `|` were between
boolean expressions (0 or 1), where bitwise OR gives the same result.

### Dispatch hints

Programmers can now override the automatic dispatch analysis with a hint:

```
@gpu while (i < n) { ... }
@par while (i < n) { ... }
@seq while (i < n) { ... }
```

The hint is stored in the while node's `d` field and flows through the
JSON IR as `"dispatch_hint": "gpu"`. The dispatch engine checks it before
running the automatic analysis. When no hint is present, behavior is
unchanged.

### Files changed

| File | Change |
|------|--------|
| `bootstrap/lexer.bpp` | Added `^` (94) to `is_op`, `@` (64) to `is_punct`, `>>` and `<<` to `scan_op` |
| `bootstrap/parser.bpp` | New precedence table with all operators, `@hint` before `while`, `dispatch_hint` in JSON |
| `bootstrap/emitter.bpp` | Removed `\|` → `\|\|` conversion, skip `dispatch_hint` in while node |
| `bootstrap/dispatch.bpp` | Check hint code before automatic analysis |
| `bootstrap/defs.bpp` | Updated WHILE node layout docs |
| `bpp_compiler.py` | OP regex, PREC table, `\|` → `\|\|` removed, dispatch hint parsing |
| `build_lexer.py` | Removed `\|` → `\|\|` |
| `build_parser.py` | Removed `\|` → `\|\|` |
| `build_emitter.py` | Removed `\|` → `\|\|` |
| `docs/bpp.md` | Updated Operators section, added Dispatch Hints section |

### Validation

- `snake_v3.bpp`: Compiles through Python compiler (981 lines C, no errors)
- `lexer.bpp`: Compiles (515 lines C), `^` operator recognized
- `parser.bpp`: Compiles (1546 lines C), dispatch hint code emitted
- `emitter.bpp`: Compiles (1733 lines C), zero `||` in output (all `|` now bitwise)
- Bitwise test: `x = 0xFF; x = x >> 4; return x | 3;` → emits `(x >> 4)` and `(x | 3)`
- Dispatch hint test: `@gpu while(...)` → IR contains `"dispatch_hint": "gpu"`

---

## 2026-03-22 — Real Pointers + Float Inference

### Real pointers

Replaced the simulated memory model (`mem_base[1MB]` + bump allocator) with
real pointer operations across the entire compiler. `malloc` now returns real
addresses, `peek`/`poke` dereference directly, `mem_load`/`mem_store` use
`uintptr_t` casts. Removed `mem_base[]`, `bpp_alloc()`, `BPP_STR` macro.

Files changed: `bpp_compiler.py`, `bootstrap/emitter.bpp`,
`bootstrap/codegen_arm64.bpp`, all `build_*.py` scripts.

This is a simplification — fewer instructions per operation, no more
dual-pointer detection in `str_peek`, and B++ can now work with real system
memory and FFI pointers naturally.

### ARM64 codegen

- Built `build_codegen.py` with comprehensive test (strings, variables, loops,
  functions, arithmetic, memory)
- Added string literal emission (`.asciz` in `__TEXT,__cstring`, loaded via
  `adrp`/`add`)
- Fixed array argument offsets (`*(arr + 8)` not `*(arr + 1)` — values are
  8-byte words)
- Real pointers: `peek` → `ldrb w0, [x0]`, `poke` → `strb w1, [x0]`,
  `mem_load` → `ldr x0, [x0]`, `malloc` → `bl _malloc`
- Removed `_mem_base` and `__bpp_heap` BSS/data sections

### Float inference (Python compiler)

Added native float support with type inference to `bpp_compiler.py`:

- **Lexer**: `FLOAT` token for `\d+\.\d+` patterns
- **Parser**: `TK_FLOAT` literal type
- **Type system**: `TY_FLOAT = 6`
- **Inference engine** (`_infer_all_floats`): fixed-point iteration with:
  - Local variable inference (assigned from float literal/expression → double)
  - Cross-function return propagation (calling float-returning function → double)
  - Call-site parameter propagation (float arg → callee param becomes double)
  - Integer-op guard: params used with `%`, `&`, `|`, `^`, `>>`, `<<` stay
    `long long` even if called with float args (prevents `double % int` errors)
- **FFI**: `float` and `double` types added to FFI maps

Example — this now compiles and runs correctly:
```
lerp(a, b, t) { return a + (b - a) * t; }
main() { auto w; w = lerp(0.0, 100.0, 0.75); print_int(w); }
// Output: 75
```

The inference correctly determines: `lerp` params are `double` (float args at
call site, no integer-only ops), returns `double`, and `w` in main is `double`
(receives float return value).

### Float — what's left

| Component | Status | Notes |
|-----------|--------|-------|
| Lexer (Python + B++) | DONE | FLOAT token |
| Parser (Python + B++) | DONE | TK_FLOAT literal |
| Inference (Python) | DONE | Cross-function, int-op guard |
| emitter.bpp | TODO | Emit `double` declarations in generated C |
| codegen_arm64.bpp | TODO | d0-d7 registers, fadd/fsub/fmul/fdiv, scvtf/fcvtzs |
| build_*.py | Low priority | Bootstrap code doesn't use float |

### Architecture note

The ARM64 codegen is entirely integer-based — everything goes through x0, no
type metadata. Float support will be **additive** (parallel d0 path), not a
refactor of existing integer code. The JSON IR already carries float literals
as `"val": "3.14"` which the codegen can detect.

---

---

## 2026-03-22 — Phase 1 Complete + Phase 2 In Progress: Unified Compiler

### Phase 1 complete

- Renamed all stb modules from `.bpp` to `.bsm` (B-Single-Module)
- Added char literal support (`'A'` → 65, `'\n'` → 10) in lexer, parser, and Python compiler
- Updated all import strings across the codebase

### Phase 2: Unified compiler — architecture

Eliminated JSON glue between pipeline stages. The unified compiler (`bpp.bpp`)
is a thin orchestrator importing 7 `.bsm` modules:

```
bpp.bpp (orchestrator, ~50 lines)
  ├── defs.bpp           — shared constants, Node struct
  ├── bpp_internal.bsm   — shared utilities (buf_eq, make_node, list ops, out)
  ├── bpp_import.bsm     — import resolver (output to memory buffer, not stdout)
  ├── bpp_lexer.bsm      — lexer (reads buffer, writes token arrays)
  ├── bpp_parser.bsm     — parser (no JSON output, builds AST in memory)
  ├── bpp_types.bsm      — type inference (walks AST, annotates itype fields)
  ├── bpp_dispatch.bsm   — dispatch analysis (walks AST, annotates dispatch fields)
  └── bpp_emitter.bsm    — C emitter (reads AST directly, no JSON input)
```

Pipeline in memory: `source → import resolver → outbuf → lexer → toks/vbuf →
parser → AST (funcs/externs/globals) → types → dispatch → emitter → stdout`.

### What was built

| Component | File | Lines | Status |
|-----------|------|-------|--------|
| Orchestrator | `bootstrap/bpp.bpp` | ~50 | Done |
| Shared utils | `bootstrap/bpp_internal.bsm` | ~90 | Done |
| Import resolver | `bootstrap/bpp_import.bsm` | ~280 | Done — emit_ch/emit_str to outbuf |
| Lexer | `bootstrap/bpp_lexer.bsm` | ~340 | Done — add_token() to toks array |
| Parser | `bootstrap/bpp_parser.bsm` | ~800 | Done — no JSON, no read_tokens |
| Type inference | `bootstrap/bpp_types.bsm` | ~330 | Done — prefixed: ty_set_var_type etc. |
| Dispatch analysis | `bootstrap/bpp_dispatch.bsm` | ~430 | Done — prefixed: dsp_add_local etc. |
| C emitter | `bootstrap/bpp_emitter.bsm` | ~600 | Done — bridge_data + emit_all |

### What was deleted

- ~250 lines of JSON output from parser (emit_jp, jot, jkp, jopen, emit_node, etc.)
- ~300 lines of JSON input from emitter (jnext, jskip, jexpect, jread_str, parse_node, etc.)
- Duplicate definitions across stages (struct Node, constants, pack/unpack, buf_eq, etc.)

### Name collision resolution

Functions duplicated across stages were consolidated:
- `buf_eq`, `print_str`, `out`, `make_node`, `list_begin/push/end` → `bpp_internal.bsm`
- `set_var_type` (parser) vs `set_var_type` (types) → types prefixed with `ty_`
- `local_vars` (dispatch) → prefixed with `dsp_`
- Emitter reads parser's data via `bridge_data()` which copies 64-byte records
  into emitter's parallel arrays

### Python compiler fixes for unified compilation

1. **Struct propagation to sub-parsers** — `.bsm` modules parsed independently
   didn't inherit struct definitions. `auto n: Node; n.ntype` caused parser sync
   loss. Fixed: `parent_structs` parameter in `BPlusPlusParser.__init__()`.

2. **Missing built-in functions** — `sys_open`, `sys_close`, `sys_read`,
   `sys_write`, `argv_get`, `argc_get` only existed in bootstrap emitter.
   Added to Python compiler's `_emit_node` + C runtime section.

3. **macOS syscall carry flag** — Added `csneg x0, x0, x0, cc` to all syscall
   wrappers in Python compiler's runtime output.

### Current blocker

The generated C (3827 lines) has 20 errors: string literals passed to
`long long` parameters without cast. Example:

```c
// Generated:
if (buf_eq((src + start), "auto", 4)) {
// Needed:
if (buf_eq((src + start), (long long)"auto", 4)) {
```

This affects `buf_eq`, `val_is`, `emit_str` calls throughout the compiler
modules. The fix is in `bpp_compiler.py`'s `_emit_node` — the `t == "lit"`
branch needs `(long long)` cast for string literals in all expression contexts.

### What remains after the fix

1. Compile: `python3 bpp_compiler.py --emit bootstrap/bpp.bpp > /tmp/bpp_unified.c`
2. Build: `clang /tmp/bpp_unified.c -o /tmp/bpp_unified`
3. Test on simple programs
4. Compare output vs old 4-stage pipeline
5. Fixed-point self-hosting: `./bpp_unified bootstrap/bpp.bpp > bpp_v2.c`

---

---


## 2026-03-23 — B++ Is Born: Zero-Dependency Native Compiler

### Milestone

B++ compiles itself to native ARM64 Mach-O executables with zero external
tools. No assembler, no linker, no C compiler, no codesign. One command:

```
bpp source.bpp -o game
./game
```

The compiler is installed at `/usr/local/bin/bpp`. Self-hosting verified
at every stage: C backend fixed-point, native backend fixed-point (3
generations bit-identical).

### What was built

| Feature | Description |
|---------|-------------|
| String literal relocation fix | Strings now written to __DATA with per-string offsets |
| stb library decoupled from raylib | All 7 modules pure B++, driver pattern (`_drv_*`) |
| `drv_raylib.bsm` | Raylib backend (17 functions), imported by the game |
| Function pointers | `fn_ptr(name)` + `call(fp, args...)`, BLR instruction |
| FFI (Foreign Function Interface) | GOT + chained fixups, calls any dylib function natively |
| `-o` output flag | `bpp source.bpp -o binary` (like `cc -o`) |
| Default mode = native binary | No `--bin` needed, `--c` for C output |
| Native ad-hoc codesign | SHA-256 in pure B++, SuperBlob + CodeDirectory |
| `~` bitwise NOT operator | Lexer + parser + codegen (MVN instruction) |
| `free` / `realloc` built-ins | Memory management for dynamic data structures |
| `sys_fork/execve/waitpid/exit` | Process control syscalls |
| `stbarray.bsm` | Dynamic arrays with shadow header (stb_ds.h technique) |
| `stbstr.bsm` string builder | `sb_new/sb_cat/sb_ch/sb_int/sb_free` |
| Compiler uses stbarray | `globals`, `externs`, `funcs` migrated to `arr_push` |
| Repo reorganization | `src/`, `tests/`, `build/`, `stb/`, `examples/` |
| `bpp-mode.el` | Emacs mode: syntax highlight, indentation, compile commands |
| Treemacs icons | Custom icons for `.bpp` and `.bsm` files |
| Char literal refactor | Magic numbers → char literals in lexer and parser |

### Bugs fixed

| # | Bug | Cause | Fix |
|---|-----|-------|-----|
| 21 | String literal output garbled | Strings never written to __DATA, all relocs same addr | Added `mo_sdata` buffer with per-string offsets |
| 22 | argc/argv point to same slot | `mo_packed_eq(-1, -2)` returned true (both length=0) | Early return for negative sentinel values |
| 23 | `~` operator missing | Not in lexer's `is_op`, not in parser's `parse_expr` | Added to lexer, parser, codegen (MVN) |
| 24 | Native compiler crash (600 funcs) | Codegen buffers 4096 bytes = 512 entries, overflowed | Increased to 65536 bytes |

### Architecture

The compiler is now a single pipeline with 11 modules:

```
source.bpp → import → lex → parse → types → dispatch
  → codegen_arm64 → enc_arm64 → macho (+ SHA-256 codesign) → binary
```

Native binaries link with dylibs via GOT (chained fixups). The stb
library is pure B++. Games choose their backend by importing a driver
module (e.g., `drv_raylib.bsm`).

**Update (2026-03-24):** The driver pattern (`_drv_*`) was replaced by
the platform layer architecture. stb is now a complete game engine — all
drawing is done on a software framebuffer in pure B++, and the platform
layer handles presenting it to the screen. See the 2026-03-24 entry for
details. The snake game now compiles to native (Cocoa), SDL2, and raylib
backends with identical game code.

### Test results

- Self-hosting: C fixed-point ✓, native fixed-point ✓
- snake_v3: compiles and runs natively with raylib ✓
- snake_native: runs natively with Cocoa, zero dependencies ✓ (2026-03-24)
- snake_sdl: runs with SDL2 backend ✓ (2026-03-24)
- snake_raylib: runs with raylib backend (new stb API) ✓ (2026-03-24)
- stbarray: 15 tests pass ✓
- stbstr: 17 tests pass ✓
- SHA-256: matches OpenSSL output for "hello" ✓
- FFI: Cocoa (objc_msgSend) + CoreGraphics (dlsym) + SDL2 + raylib ✓

---

---

## 2026-03-23 — Phase 5: Native Mach-O ARM64 Binary (Almost Born)

### Milestone

B++ can now emit native Mach-O ARM64 executables directly — no assembler, no
linker, no clang. The `--bin` flag triggers the full native pipeline:

```
./bpp --bin source.bpp
```

This produces `a.out`, a signed Mach-O executable that runs on macOS ARM64.
All test programs pass. The compiler itself compiles and runs — but its C
output is garbled due to one remaining bug in string literal relocation.

### Architecture

Three new modules complete the native pipeline:

| Module | Purpose | Lines |
|--------|---------|-------|
| `bpp_codegen_arm64.bsm` | ARM64 code generator (from AST, not JSON) | ~700 |
| `bpp_enc_arm64.bsm` | Binary instruction encoder (4-byte ARM64 opcodes) | ~400 |
| `bpp_macho.bsm` | Mach-O writer (headers, segments, codesign) | ~700 |

Pipeline: `source → import → lex → parse → types → dispatch → codegen_arm64 →
enc_arm64 (code buffer + relocations) → macho (Mach-O binary + codesign)`.

### What was built

**Binary instruction encoding** (`bpp_enc_arm64.bsm`):
- Every ARM64 instruction encoded as a 4-byte value via `enc_emit32()`
- Relocation table for forward references: ADRP/ADD pairs patched after layout
- Three relocation types: 0 = global variable, 1 = string literal, 2 = float literal
- Label system with backpatching for branches (B, CBZ, CBNZ, B.cond)
- Full floating-point encoding: FADD, FSUB, FMUL, FDIV, FCMP, SCVTF, FCVTZS, FMOV

**Mach-O writer** (`bpp_macho.bsm`):
- Complete Mach-O header with magic, CPU type, file type
- Load commands: LC_SEGMENT_64 (__PAGEZERO, __TEXT, __DATA, __LINKEDIT), LC_MAIN
- Page-aligned segments (4096-byte boundaries)
- __TEXT,__text: code bytes from encoder
- __DATA: globals (8 bytes each), float literals (8 bytes each), string data
- Relocation resolution: patches ADRP/ADD instruction pairs with correct page offsets
- Ad-hoc code signing via external `codesign -s -` (called via `sys_exec`)
- IEEE 754 float encoding (`mo_atof`) implemented as pure integer arithmetic
- `mo_packed_eq` for comparing packed string references by content

**Codegen integration** (`bpp_codegen_arm64.bsm`):
- Adapted from standalone `codegen_arm64.bpp` (~1400 lines → ~700 lines)
- Removed JSON reader, list builder, type analysis (all handled by other modules)
- Added `a64_fn_fidx` array mapping emitter function index → parser function index
- Type queries use `get_fn_var_type`, `get_fn_ret_type`, `get_fn_par_type` from bpp_types.bsm
- Global variable deduplication in `bridge_data_arm64()`
- argc/argv implemented via hidden globals (-1, -2 sentinel packed values)
- main() prologue saves dyld's x0/x1 (argc/argv) to hidden globals

**Bump allocator** (emitted as runtime in every native binary):
- `mmap` syscall (197) allocates 16MB heap
- `malloc(size)` = bump pointer, 8-byte aligned
- No `free()` — sufficient for compiler and most B++ programs

### Bugs fixed

16. **Type system index mismatch** — Both emitters (C and ARM64) deduplicate
    functions by name, creating `fn_cnt < func_cnt`. But type queries
    (`get_fn_ret_type`, `get_fn_par_type`) use the parser's raw index. Fixed
    by adding `fn_fidx` / `a64_fn_fidx` mapping arrays that translate emitter
    index → parser index before every type query.

17. **IEEE 754 float encoding** — The C backend's `*(long long*)(ptr) = bits`
    where `bits` is a `double` truncates instead of bit-preserving (2.0 → 2
    instead of 0x4000000000000000). Rewrote `mo_atof` as pure integer
    computation: parse integer + fractional digits as rational, find exponent
    via successive halving/doubling, extract 52 mantissa bits, assemble
    sign|exponent|mantissa. No float arithmetic anywhere.

18. **argc/argv in native mode** — Native binaries couldn't read command-line
    arguments. macOS ARM64 with LC_MAIN receives argc in x0 and argv in x1
    from dyld. Fixed by adding hidden globals (`a64_argc_sym`, `a64_argv_sym`
    with sentinel packed values -1, -2) and saving x0/x1 in main()'s prologue.
    `argc_get()` and `argv_get(i)` load from these globals via ADRP+LDR.

19. **Global variable deduplication** — The parser adds the same global name
    multiple times from different imported modules, each with a different
    packed value (different vbuf offset). In binary mode, each gets a separate
    __DATA slot. `init_import()` writes to one slot, `main()` reads from
    another → NULL pathbuf. Fixed by deduplicating globals in
    `bridge_data_arm64()` using string-content comparison, and using
    `mo_packed_eq` in the relocation resolver.

20. **defs.bpp → defs.bsm** — `defs.bpp` has no `main()` function (it's a
    module of constants and struct definitions). Renamed to `.bsm` for
    consistency with the module system.

### Test results (all passing)

| Test | Expected | Result |
|------|----------|--------|
| test_exit42 | exit=42 | exit=42 |
| test_arith | exit=42 | exit=42 |
| test_hello | "Hello, B++!" | "Hello, B++!" |
| test_global | exit=42 | exit=42 |
| test_fib | exit=55 | exit=55 |
| test_malloc | exit=42 | exit=42 |
| test_float | "6" | "6" |
| test_float2 | "314" | "314" |
| test_float3 | "13 7 33 3 75" | "13 7 33 3 75" |
| test_argv | "hello" exit=2 | "hello" exit=2 |
| test_native_debug | all inits | "123456789ABC" |

### Blocking bug — string literal output garbled (**RESOLVED**)

The native compiler's C output was garbled — type-1 relocations (string
literals) resolved to wrong addresses in `mo_resolve_relocations`.

**Fixed** in the same session by adding `mo_sdata` buffer with per-string
offsets. See bug #21 in the 2026-03-23 entry above.

### What remained for B++ to be born (**ALL RESOLVED**)

1. ~~Fix string literal relocation~~ — ✓ Fixed (bug #21)
2. ~~Self-compilation~~ — ✓ Fixed-point verified (2026-03-23)
3. ~~Fixed-point verification~~ — ✓ Native compiler compiles itself (2026-03-23)
4. ~~Cleanup~~ — ✓ Done
5. ~~Ad-hoc codesign in B++~~ — ✓ Implemented in pure B++ SHA-256 (2026-03-23)

---


## 2026-03-24 — stb Native: Games Without External Libraries

### Milestone

B++ games run natively on macOS without SDL, raylib, or any external
game library. The stb standard library is the game engine — all drawing,
input, and timing is pure B++. The compiler's platform layer handles OS
communication via Cocoa/CoreGraphics.

Three backends work with identical game code:
- **Native** (Cocoa) — zero dependencies
- **SDL2** — texture upload via SDL_UpdateTexture
- **Raylib** — DrawRectangle per pixel

### What was built

| Feature | Description |
|---------|-------------|
| `var` keyword | Stack-allocated structs: `var v: Vec2;` (no malloc) |
| `enum` | Named constants: `enum Dir { UP, DOWN, LEFT, RIGHT }` |
| `break` | Exit innermost while loop, nested support |
| `stbbuf.bsm` | Raw buffer access: read/write u8/u16/u32/u64 |
| `stbfile.bsm` | File I/O: `file_read_all(path, size_out)` |
| `stbfont.bsm` | 8x8 bitmap font (A-Z, a-z, 0-9, punctuation) |
| `stbdraw.bsm` rewrite | Software framebuffer rendering (pure B++) |
| `stbinput.bsm` rewrite | Memory-based input with Key enum |
| `stbgame.bsm` rewrite | Game loop via platform built-ins |
| `_stb_platform_macos.bsm` | Cocoa native: dlopen AppKit, objc_msgSend, CGBitmapContext |
| `drv_sdl.bsm` | SDL2 backend: texture upload, letterbox scaling |
| `drv_raylib.bsm` | Raylib backend: rlgl texture or DrawRectangle |
| FFI ARM64 ABI fix | Separate int/float register counters |
| FFI overload by argc | Multiple declarations of same extern name |
| FFI float return types | Type system aware of extern "double" returns |
| Global float variables | Load/store globals as float across functions |
| Target import resolver | `foo.bsm` → auto-tries `foo_<target>.bsm` |
| Import path: `drivers/` | Drivers resolved from drivers/ directory |
| Import path: global | `/usr/local/lib/bpp/stb/` for installed modules |
| MCU game port | Full game (rat vs cat) running on all 3 backends |

### Examples

| Example | Backend | Description |
|---------|---------|-------------|
| `hello.bpp` | raylib FFI | Simple colored window |
| `hello_native.bpp` | Native | Hello world with stb |
| `snake_native.bpp` | Native | Snake game, zero dependencies |
| `snake_sdl.bpp` | SDL2 | Same snake, SDL backend |
| `snake_raylib.bpp` | Raylib | Same snake, raylib backend |
| `sdl_demo.bpp` | SDL2 | Drawing, input, text demo |
| `raylib_demo.bpp` | raylib FFI | Bouncing cubes |
| `raylib_demo2.bpp` | raylib FFI | Multi-cube physics |

### Architecture

```
Game code (dev writes)
    │
    ▼
stb (pure B++: stbdraw, stbinput, stbgame, stbfont)
    │
    ▼
Platform layer (_stb_init_window, _stb_present, _stb_poll_events)
    │
    ▼
Backend (auto-selected by target or overridden by driver import)
    │
    ├── Native: Cocoa + CoreGraphics (macOS, zero deps)
    ├── SDL2: drv_sdl.bsm (texture upload)
    └── Raylib: drv_raylib.bsm (DrawRectangle)
```

### Bugs fixed

| # | Bug | Cause | Fix |
|---|-----|-------|-----|
| 25 | TY_STRUCT crashes binary | Global not declared with `auto` in defs.bsm | Add `auto TY_STRUCT` declaration |
| 26 | FFI float args in wrong regs | Single counter for int+float | Separate int_idx/flt_idx counters |
| 27 | FFI mixed arg positions | Int args not compacted after float removal | Compact ints: `mov x[int_idx], x[i]` |
| 28 | Global float lost across functions | Global load/store always used integer regs | Track global types, use ldr d0/str d0 |
| 29 | stbio + SDL crash | String literal conflict with many externals | Workaround: avoid stbio before SDL calls |
| 30 | mach_absolute_time wrong | Missing timebase conversion | mach_timebase_info numer/denom |
| 31 | CoreGraphics colors wrong | bitmapInfo 8198 ignores alpha byte | Changed to 8194 (AlphaPremultipliedFirst) |

### Codebase size

| Component | Lines of B++ |
|-----------|-------------|
| Compiler (12 modules) | ~8,000 |
| stb library (11 modules) | ~1,800 |
| Platform layer (macOS) | ~200 |
| Drivers (SDL2 + raylib) | ~200 |
| **Total** | **~10,000** |

A self-hosting compiler + game engine + native platform layer in 10K lines of a minimalist language.

### Pending (RESOLVED 2026-03-24)

- ~~**stbcol.bsm** — Not yet tested or integrated~~ → Tested (24/24) and integrated into MCU. See 2026-03-24 entry.

---

## 2026-03-24 — Compiler Diagnostics + x86_64 Backend + stbcol Integration

### stbcol.bsm — Tested and Integrated

- `tests/test_col.bpp`: 24 tests covering rect_overlap, circle_overlap, rect_contains, circle_contains. All pass.
- `tests/test_col_float.bpp`: Float argument test — rect_overlap works with double params via type inference.
- MCU game (`/Users/Codes/mcu-bpp/src/main.bpp`): integrated stbcol, `check_overlap()` now calls `rect_overlap()`.
- **MCU crash root cause**: missing `import "_stb_platform.bsm"`. The platform functions (`_stb_init_window`, `_stb_present`, etc.) were called but never defined — the compiler silently generated broken GOT relocations. Fixed by adding the import.

### Compiler Diagnostics — Teaching Error Messages

New architecture: semantic errors live in `bpp_validate.bsm`, platform-agnostic. Codegen only has internal panics for "should never happen after validate" cases.

```
import → lex → parse → types → dispatch → VALIDATE → codegen → macho
                                              │
                                     semantic errors here
                                     platform-agnostic
```

**New modules:**
- `src/bpp_diag.bsm` — Diagnostic output infrastructure. All messages go to stderr (fd=2). Functions: `diag_fatal(code)`, `diag_warn(code)`, `diag_str()`, `diag_int()`, `diag_packed()`, `diag_loc()`, `diag_summary()`.
- `src/bpp_validate.bsm` — Semantic validation pass. Walks all function bodies after type inference, checks every T_CALL has a target (in funcs[], externs[], or builtins). Reports E201 with function name and suggestion.

**Error catalog implemented:**

| Code | Stage | Message |
|------|-------|---------|
| E001 | import | `cannot open file '{path}'` |
| E002 | import | `import '{name}' not found (searched: ./, stb/, /usr/local/lib/bpp/stb/)` |
| E101 | parser | `unknown type '{name}' -- define it with 'struct'` |
| E102 | parser | `struct has no field '{field}'` |
| E103 | parser | `sizeof applied to unknown type '{name}'` |
| E104 | parser | `unexpected token in expression` |
| E201 | validate | `function '{name}' called but never defined -- missing import?` |
| W001 | lexer | `unterminated string literal at line N` |

**Line number tracking**: lexer counts newlines in `skip_ws()`, stores line per token in `tok_lines` array. File boundary tracking in import resolver maps tokens to source files.

**Validate immediately proved its value**: caught the `print_msg` bug in `bpp_codegen_arm64.bsm` — a function called but never defined (from stbio.bsm, which the compiler doesn't import). Fixed by replacing with `diag_str` + `sys_exit(2)`.

### ARM64 Encoder — Dynamic Arrays (Buffer Overflow Fix)

**Root cause of compiler hang**: `enc_lbl_off` was a fixed 32KB buffer (4096 labels max). With 456+ functions (after adding x86 modules), label IDs exceeded 4096, silently corrupting heap memory. The compiler hung or produced broken binaries.

**Fix**: Migrated all 7 encoder arrays from `malloc` fixed buffers to `arr_push`/`arr_get`/`arr_set` dynamic arrays (stbarray):

| Array | Before | After |
|-------|--------|-------|
| `enc_lbl_off` | `malloc(32768)` — 4096 labels max | `arr_new()` — unlimited |
| `enc_fix_pos/lbl/ty` | `malloc(65536)` — 8192 fixups max | `arr_new()` — unlimited |
| `enc_rel_pos/sym/ty` | `malloc(32768)` — 4096 relocs max | `arr_new()` — unlimited |

Added `arr_clear()` calls at codegen start to reset arrays between compilations.

### T_BREAK Declaration Fix

`T_BREAK` was assigned (`T_BREAK = 13` in `init_defs()`) but never declared with `auto` at module scope. This meant it was a local variable inside `init_defs`, invisible to other modules. The emitter/codegen checked `if (t == T_BREAK)` against an uninitialized global (always 0).

Fixed: added `T_BREAK` to the `auto` declaration in `defs.bsm`. The Mach-O internal panic immediately caught this: `internal error: global 'T_BREAK' not found in data section`.

### x86_64 Cross-Compilation Backend (Linux ELF)

Three new modules for cross-compiling B++ to Linux x86_64 static ELF binaries:

| File | Lines | Purpose |
|------|-------|---------|
| `src/bpp_enc_x86_64.bsm` | ~856 | x86_64 instruction encoder (prefix `x64_enc_`) |
| `src/bpp_codegen_x86_64.bsm` | ~1220 | x86_64 code generator (Linux syscalls) |
| `src/bpp_elf.bsm` | ~389 | ELF writer (static binary, 2 PT_LOAD segments) |

New flag: `bpp --linux64 source.bpp -o out` (mode=3).

**Status**: Code written, not yet tested on actual Linux. Docker container `bpp-linux` (Ubuntu 24.04) prepared for testing. Blocked by the encoder buffer overflow (now fixed with dynamic arrays).

### Bugs Fixed

| # | Bug | Cause | Fix |
|---|-----|-------|-----|
| 32 | MCU segfault | Missing `import "_stb_platform.bsm"` — GOT relocations broken | Added import to MCU game |
| 33 | Compiler hang with 456+ functions | `enc_lbl_off` buffer overflow (4096 labels max) | Dynamic arrays via stbarray |
| 34 | `T_BREAK` invisible to other modules | Missing `auto` declaration in defs.bsm | Added to auto line |
| 35 | `print_msg` in codegen undefined | Called stbio function the compiler doesn't import | Replaced with `diag_str` + `sys_exit(2)` |
| 36 | `diag_buf(ptr, len)` — `ptr` is a keyword | B++ treats `ptr` as type keyword, not parameter | Renamed to `diag_buf(addr, len)` |

### macOS Quarantine Issue (UNRESOLVED)

macOS Sequoia applies `com.apple.provenance` extended attribute to all files written by sandboxed processes (Claude Code runs sandboxed). Native binaries with this attribute get SIGKILL when executed from the repo directory.

**Does NOT work**: `xattr -d`, `DevToolsSecurity -enable`, `dangerouslyDisableSandbox` flag.
**Works**: `xattr -cr ./bpp && codesign -f -s - ./bpp` from user's terminal, or build to `/tmp` and copy.
**Pending**: `claude config set sandbox.enabled false` + reboot.

### Codebase Size

| Component | Lines of B++ |
|-----------|-------------|
| Compiler (15 modules) | ~10,900 |
| stb library (13 modules + platform) | ~2,000 |
| Drivers (SDL2 + raylib) | ~200 |
| **Total** | **~13,100** |

### Architecture After This Session

```
bpp.bpp (orchestrator)
  ├── defs.bsm            — constants, Node struct
  ├── stbarray.bsm        — dynamic arrays (compiler dependency)
  ├── bpp_internal.bsm    — shared utilities
  ├── bpp_diag.bsm        — NEW: diagnostic output (stderr)
  ├── bpp_import.bsm      — import resolver + E001/E002
  ├── bpp_parser.bsm      — parser + E101-E104
  ├── bpp_lexer.bsm       — lexer + line tracking + W001
  ├── bpp_types.bsm       — type inference
  ├── bpp_dispatch.bsm    — loop analysis
  ├── bpp_validate.bsm    — NEW: semantic validation (E201)
  ├── bpp_emitter.bsm     — C backend
  ├── bpp_enc_arm64.bsm   — ARM64 encoder (dynamic arrays)
  ├── bpp_codegen_arm64.bsm — ARM64 codegen
  ├── bpp_macho.bsm       — Mach-O writer (internal panics)
  ├── bpp_enc_x86_64.bsm  — NEW: x86_64 encoder
  ├── bpp_codegen_x86_64.bsm — NEW: x86_64 codegen
  └── bpp_elf.bsm         — NEW: ELF writer
```

---


## 2026-03-25 — Linux x86_64 Port: First Binary to Snake Game

### x86_64 Cross-Compilation Working

The B++ compiler now cross-compiles from macOS ARM64 to Linux x86_64 ELF binaries. The full pipeline works: `./bpp --linux64 source.bpp -o binary` produces a static ELF that runs in a Docker Ubuntu x86_64 container.

### Bugs Found and Fixed During Port

**Float addition emitting 3x addsd**: Draft code was left in `bpp_codegen_x86_64.bsm` — three `addsd` instructions where one was needed. Every float add was tripled (1.0+48 gave 51+1+1=53 instead of 49). Removed the draft lines.

**elf_packed_eq missing sentinel comparison**: The argc/argv globals use sentinel values (-1, -2) as packed names. The Mach-O writer's `mo_packed_eq` had `if (a == b) return 1` for direct comparison, but the ELF writer's `elf_packed_eq` didn't — causing it to unpack -1 as a string with length 0xFFFFFFFF. Added the same guards.

**Type propagation poisoning callee params**: `propagate_call_params()` permanently mutated callee parameter types. When `print_int(whole)` was called with a float, it marked `print_int`'s `n` as TY_FLOAT globally. Removed propagation entirely (Phase A fix — see separate entry below).

**x86_64 call-site type awareness**: The ARM64 codegen consulted `get_fn_par_type()` at call sites to convert float↔int correctly. The x86_64 codegen was blind — just did `MOVQ` for all floats. Replicated the ARM64 logic: check callee's expected type, emit CVTTSD2SI or CVTSI2SD+MOVQ as needed.

### Platform Layer Auto-Routing

Added automatic platform selection via `--linux64` flag: sets `_imp_target = "linux"`, so `import "_stb_platform.bsm"` resolves to `_stb_platform_linux.bsm` automatically. Same mechanism already works for macOS.

Created `stb/_stb_platform_linux.bsm` — terminal-based platform layer:
- Renders framebuffer as ANSI 256-color half-block characters
- Raw terminal mode via `sys_ioctl` (TCGETS/TCSETS)
- Non-blocking keyboard input (arrow keys + WASD)
- Timing via `sys_clock_gettime(CLOCK_MONOTONIC)`
- Frame rate cap via `sys_nanosleep`

### New Syscalls

Added to x86_64 codegen + validator: `sys_ioctl` (16), `sys_nanosleep` (35), `sys_clock_gettime` (228).

### Tests Passing

| Test | Result |
|------|--------|
| exit(42) | 42 |
| hello world (putchar) | Hello, Linux x86_64! |
| full suite (strings, recursion, malloc, loops, if/else, function calls) | All pass |
| floats (arithmetic, comparison, truncation) | 3.00, 6.00, OK |
| file I/O (open, write, read, close) | wrote 13, read back OK |
| argc/argv | argc=4, all args correct |
| **Snake game** | Compiles (45KB static ELF), renders frames in terminal |

### Design Decision: Two Calling Conventions

Internal B++ calls use GPR-uniform convention (all args in RDI/RSI/etc., floats bit-transferred via MOVQ). External FFI calls (future) will use System V ABI with floats in XMM registers. Cost: 1 MOVQ per float arg per call (~free on modern CPUs).

---

## 2026-03-25 — Type Propagation Bug Fix + Evolution Roadmap

### Bug Fix: Type Propagation

`propagate_call_params()` in `bpp_types.bsm` was permanently mutating callee parameter types based on one caller's argument types. When `print_int(whole)` was called with a float `whole`, the type system marked `print_int`'s parameter `n` as TY_FLOAT globally — poisoning all other call sites. Crashed on ARM64, wrong output on x86_64.

**Root cause**: Cross-function type propagation was one-directional and permanent. The guard (`uses_int_ops_list`) only caught `%`, `&`, `|`, `^`, `<<`, `>>` as int-only evidence — operations like `/` and `>` didn't prevent propagation.

**Fix**: Removed `propagate_call_params()` from the fixed-point loop. Types are now determined solely by body inference. The codegen handles float↔int conversion at each call site independently (ARM64 already did this; x86_64 port replicates the same logic). Bootstrap verified — identical 280994-byte binaries.

**Design decision**: A full "context-based specialization" system (compiler generates `add$FF`/`add$LL` variants automatically) was designed but **paused** — no current B++ code needs it, and it touches the emission loop which is high-risk for bootstrap.

### Evolution Roadmap

Established the three-pillar vision for B++:

1. **ART** — `stbart.bsm` (pixel art primitives, ported from ModuLab JS editor) + sprite/tilemap/level/particle editors
2. **SOUND** — `stbdsp.bsm` (DSP primitives) + `stbplugin.bsm` (CLAP audio plugins, native + FFI driver like SDL)
3. **GAMES** — stbgame + stbtile + stbphys + stbpath

Asset pipeline: `.bspr` → `.btm` → `.blvl` → game binary. Full plan in `docs/todo.md`.

Key blockers identified: mouse events not polled (B0), `mouse_pressed()` undefined (B1), `file_write_all()` missing (B2), SDL key gaps (B3).

x86_64 calling convention decided: two conventions — internal B++ = all GPR (MOVQ for floats), external FFI = System V ABI.

---


## 2026-03-26 — Modular Compilation, Dynamic Arrays, Type Hints, and Backend Reorganization

Two sessions worth of work. The compiler gained dynamic arrays, sub-word type hints, named field offsets, and a full modular compilation pipeline with .bo object files.

### Dynamic Array Migration

87 arrays across 9 compiler modules migrated from fixed-size malloc with manual counters to `arr_new()`/`arr_push()`/`arr_get()`/`arr_set()` (from `stbarray.bsm`). Eliminates all hard-coded limits on structs, enums, variables, functions, and similar entities. Affected modules: `bpp_parser`, `bpp_types`, `bpp_codegen_arm64`, `bpp_codegen_x86_64`, `bpp_enc_arm64`, `bpp_enc_x86_64`, `bpp_macho`, `bpp_elf`, `bpp_dispatch`.

### Type Hints — Sub-Word Types Across All Backends

Real sub-word type support added to all three backends (ARM64, x86_64, C emitter). Syntax: `auto x: byte;`, `fn(x: float)`. Supported types: `byte` (8-bit), `quarter` (16-bit), `half` (32-bit), `int` (64-bit), `float` (64-bit double). ARM64 uses `ldrb`/`strb`/`ldrh`/`strh`/`ldr w`/`str w`. x86_64 mirrors the same narrowing. C emitter maps to `uint8_t`/`uint16_t`/`uint32_t`. Truncation is performed by hardware. Design: sub-word hints are opt-in performance tuning, never auto-inferred.

### Phase 0 — Module Tracking

Added `tok_src_off` in the lexer to track source file boundaries. Parser stores per-entity module ownership via `func_mods[]`/`glob_mods[]`. `fname_buf` provides permanent filename storage. `diag_loc()` uses binary search over source offsets to resolve the correct file and line for diagnostic messages.

### Phase 1 — Hash + Dependency Graph

FNV-1a content hashing per module. Dependency graph via `dep_from[]`/`dep_to[]` arrays. `topo_sort()` implements Kahn's algorithm in reverse order. Hash persistence in `.bpp_cache/hashes`. `propagate_stale()` marks dependents dirty when a source file changes. `--show-deps` flag for inspecting the graph. Also fixed `hex_emit()` for signed values.

### Named Field Offsets

Added named constants for function record fields (`FN_TYPE`/`FN_NAME`/`FN_PARS`/`FN_PCNT`/`FN_BODY`/`FN_BCNT`/`FN_HINTS`) and extern record fields (`EX_TYPE`/`EX_NAME`/`EX_LIB`/`EX_RET`/`EX_ARGS`/`EX_ACNT`) in `defs.bsm`. Refactored ~110 raw numeric offset accesses across 8 files. This eliminates a class of bugs like the n.c/n.d confusion described below.

### Bug Fix: var Struct Flag Collision

The parser set the stack struct flag for `var v: Vec2` in field `n.c` (offset 24), but type hints also stored their value in `n.c`. Since `TY_BYTE = 1` coincided with the struct flag value, every variable declaration was silently treated as byte-typed. All `test_var_*` tests were segfaulting. Fix: moved the struct flag to `n.d` (offset 32).

### Modular Compilation (.bo Files — Go Model)

Full incremental compilation pipeline, modeled after Go's package compilation. Implemented in 8 steps:

- **bpp_bo.bsm**: New module with I/O primitives — `read`/`write` for `u8`/`u16`/`u32`/`u64`, packed string serialization with vbuf re-packing.
- **Export data serialization**: Functions (return type + parameter types + hints), structs, enums, globals, and externs are serialized into `.bo` files.
- **Per-module codegen**: `emit_module_arm64`/`emit_module_x86_64` with accumulative `enc_buf`. Cross-module `BL`/`CALL` instructions use type 4 relocations.
- **Cross-module call resolution**: `bo_resolve_calls` patches `BL`/`CALL` placeholders after all modules are compiled, then removes type 4 entries from the relocation list.
- **Incremental driver**: `--incremental` flag triggers per-module lex/parse/infer/codegen. `.bo` cache stored in `.bpp_cache/`.
- **String/float index remapping**: Resolved issue where string and float constant indices collided across modules. Sentinel handling preserved for argc/argv globals.
- **Extern refresh**: FFI argument rearrangement was empty in the modular path because externs were only loaded once. Fixed by refreshing externs per-module before codegen.

### Module Ownership Tracking

Added `struct_mods[]`/`enum_mods[]`/`extern_mods[]` alongside existing `func_mods[]`/`glob_mods[]`. All entity types now track which source module defined them.

### find_func_idx Backward Scan

Changed `find_func_idx` to scan backward for "last wins" semantics matching codegen behavior. Later reverted — backward scan caused a segfault in gen3 bootstrap due to interaction with type inference. Deferred for future investigation.

### Backend Reorganization

Moved platform-specific files into subdirectories:
- `src/aarch64/`: `a64_enc.bsm`, `a64_codegen.bsm`, `a64_macho.bsm`
- `src/x86_64/`: `x64_enc.bsm`, `x64_codegen.bsm`, `x64_elf.bsm`

All import paths updated throughout the compiler source.

### x86_64 _start Stub

Extracted `x64_emit_start_stub()` so the per-module pipeline can emit `_start` after all modules have been compiled rather than at the start of codegen.

### stb Changes

- `stbdraw`: `draw_number()` changed from global `_num_buf` to local malloc+free.
- `stbui`: Added bounds checks in `ui_alloc()` and `lay_push()`.

### Bugs Found

| Bug | Status |
|-----|--------|
| var struct flag collision (`n.c = 1 = TY_BYTE`) | FIXED — moved flag to `n.d` |
| Extern FFI rearrangement empty in modular path | FIXED — refresh externs per-module |
| Param float inference regression (params in float expressions stay `TY_LONG`, `fcvtzs` destroys values) | WORKAROUND — `: float` annotation. Root cause: `propagate_call_params()` disabled |
| `find_func_idx` backward scan segfault in gen3 bootstrap | REVERTED |
| `ptr` and `len` are reserved words in B++ | Discovered when writing `bpp_bo.bsm` |

### Verification

| Test | Result |
|------|--------|
| Bootstrap (monolithic) | Passes |
| Bootstrap (per-module `--incremental`) | Self-consistent |
| All test programs | Pass (ARM64 + Linux x86_64) |
| Snake game | Compiles and runs on all backends |
| MCU game | Compiles and runs (with `: float` parameter annotations) |

---



## 2026-03-26 — Cache Fix, Code Signature, memcpy/realloc, Import Fixes, Per-Variable Hints, stb Infra

Second session of the day. Recovered from a git push that lost changes. Rebuilt everything from backup.

### Cache Modular Fix (Definitive)

Manifest hash embedded in every `.bo` file header. Three-layer validation: (1) manifest hash — ensures same program/module list, (2) source hash — module content unchanged, (3) dependency hashes — no transitive changes. Version bumped to 2; old v1 `.bo` files auto-rejected. Fast-path in `.bpp_cache/hashes` file skips loading `.bo` files entirely when the program changes. `cache_manifest_hash()` computes FNV-1a of module count + all filenames. Eliminates the cross-program cache corruption that caused segfaults when compiling different programs in sequence.

### Code Signature Fix

Mach-O code signature now matches the system `codesign` tool output: `CS_ADHOC` flag (0x2) without `CS_LINKER_SIGNED`, version 0x20100, 2 special slots (info plist + requirements, zero-filled). Page hashes use SHA-256. Binaries no longer need external `codesign` after compilation.

### memcpy + realloc Builtins

`memcpy(dst, src, len)` implemented in all three backends. ARM64 uses optimized post-index byte loop (cbz skip + ldrb_post/strb_post/subs/cbnz). x86_64 uses test+jz skip + loadb/storeb/sub/jnz loop. C emitter calls libc `memcpy()`. New encoder instructions: `enc_ldrb_post`, `enc_strb_post`, `enc_subs_imm`.

`realloc` now supports both 2-arg `realloc(ptr, size)` (backward compat, just malloc) and 3-arg `realloc(ptr, old_size, new_size)` (allocate + copy). ARM64 3-arg: mmap + byte copy loop. x86_64 3-arg: mremap first, fallback to mmap + copy. C emitter: uses libc realloc (ignores old_size).

### Import System Improvements

Added `src/`, `/usr/local/lib/bpp/`, and `/usr/local/lib/bpp/drivers/` to the import search path. Updated 13 test files with current module names (`defs.bpp` → `defs.bsm`, `bpp_enc_arm64.bsm` → `aarch64/a64_enc.bsm`, etc.) and added missing imports for incomplete tests.

### C Emitter Fixes

Stack struct copy (`b = a;`) now emits `memcpy()` instead of illegal lvalue cast. Added `#include <string.h>` to the C header.

### Per-Variable Type Hints

`auto x: byte, y: quarter;` now correctly assigns individual hints per variable. Parser allocates a hints array stored in T_DECL node field `e` (offset 40). Type inference reads per-variable hints when present, falls back to single hint for backward compat.

### word Keyword Removed

`word` removed from the lexer keyword list — it's now a free identifier. `auto x: word;` still works via `try_type_annotation()`. `byte`, `half`, `quarter` work as standalone declaration keywords (`byte hp;`).

### stb Infrastructure

- `file_write_all(path, buf, len)` in stbfile.bsm
- `blend_px(x, y, src_color)` in stbdraw.bsm — alpha blending with ARGB format
- `mouse_released(btn)` + fixed `mouse_pressed(btn)` with real edge detection via `_stb_mouse_btn_prev`
- Mouse button events (left/right down/up) in Cocoa event loop (_stb_platform_macos.bsm)

### Install Script

`install.sh` — compiles fresh binary to `/tmp`, then `sudo cp` to `/usr/local/bin/bpp` + stb + drivers + defs. Usage: `sh install.sh --skip`.

### Param Float Inference (FAILED — Deferred)

Naive T_BINOP promotion (promote all vars to float when binop result is float) breaks Cocoa apps silently. Variables used in both integer and float contexts (e.g. `head_x`) get wrongly promoted. Reverted. Workaround: explicit `: float` annotation. Needs conservative approach (params-only, check integer usage).

### Verification

| Suite | Result |
|-------|--------|
| Native | 15/15 |
| C Emitter | 15/15 |
| Linux | 6/6 |
| Encoder | 13/13 |
| Examples | 6/6 |
| Snake | Runs |

### Lessons Learned

- NEVER use `git stash`/`checkout`/`restore` on the `bpp` binary — it replaces the bootstrapped compiler with the git fossil
- Always test `snake_native` after type system changes
- `install.sh` must compile to `/tmp` first to bypass sandbox restrictions
- `git checkout` on source files reverts ALL changes, not just targeted ones — don't use for selective reverting

## 2026-03-27 — float_ret Builtins, Go-Style Cache, Type System Fixes, Mouse Tracking

The P0 blocker (mouse stuck at 0,0) is resolved. Three independent bugs were found and fixed, plus a long-standing cache invalidation issue that had been causing inconsistent builds since modular compilation was introduced.

### float_ret() / float_ret2() — Design B

Both ARM64 and x86_64 backends now save float return registers (d0/d1 on ARM64, xmm0/xmm1 on x86_64) to hidden stack slots immediately after every extern function call. Two new builtins read from those slots:

- `float_ret()` — first float return value
- `float_ret2()` — second float return value

Frame layout changed: ARM64 variables start at offset 40 (was 24), x86_64 at -(24+total) (was -(8+total)). The extra 16 bytes per frame hold the saved float registers.

Design B was chosen over Design A (trampoline alias) because it's architecture-agnostic — each backend saves its own registers, no platform-specific function names needed. The pattern works identically across ARM64, x86_64, and future backends (WASM, cc65).

**Pattern:**
```
objc_msgSend(ev, sel_registerName("locationInWindow"));
lx = float_ret();     // d0 saved from last extern call
ly = float_ret2();    // d1 saved from last extern call
```

**Root cause of mouse 0,0:** `objc_msgSend` was declared as returning `ptr`, so the codegen emitted `scvtf d0, x0` (converting garbage integer to float) instead of reading d0 directly. And d1 was volatile between statements. float_ret/float_ret2 solve both problems.

### Go-Style Cache Hash Chain

Replaced `propagate_stale()` with `hash_with_deps()`. Each module's hash now includes the hashes of all its dependencies, computed in topological order. Transitive invalidation is automatic — if C changes, B's combined hash changes, A's combined hash changes. No separate propagation step.

**Before:** `propagate_stale()` walked the dependency graph and manually marked dependents. This had bugs — modules could be missed, producing inconsistent binaries (1958 bytes different from a fresh compile).

**After:** `hash_with_deps()` — 15 lines, correct by construction. Proven: incremental compile produces byte-identical binary to fresh compile.

This was the bug that caused 3 days of cache-related build failures since modular compilation was introduced.

### Type System: Forced Hints Override Inference

`a64_var_is_float()` and `x64_var_is_float()` now check the forced type annotation (`: int`, `: byte`, etc.) before consulting type inference. If the programmer annotated a variable, the annotation wins.

**Before:** `auto x: int; x = 3.14;` would store the raw double bit pattern because inference promoted `x` to TY_FLOAT, ignoring the `: int` hint.

**After:** The `: int` hint forces `fcvtzs` (float→int conversion). The programmer's intent is respected.

### Type System: Global Float Poisoning Removed

Removed automatic promotion of global variables to TY_FLOAT (line 270 of bpp_types.bsm). Previously, `_stb_mouse_x = lx / 3.0` would permanently mark `_stb_mouse_x` as a float global, causing all subsequent stores to write raw double bit patterns (19-digit numbers starting with 46).

Now globals are only float if explicitly annotated with `: float`. No globals in the codebase use `: float`, so this change has zero impact on existing code.

### Window Close Detection

Added `isVisible` check at the end of `_stb_poll_events()` (once per frame, after all events are processed). When the user clicks the red X button, `_plt_quit_flag` is set to 1 and `game_should_quit()` returns true.

Also added `game_end()` / `_stb_shutdown()` to stbgame.bsm, but `return 0` from main already terminates the process correctly — `game_end()` is optional for resource cleanup.

### bootstrap.c Status

bootstrap.c was generated on 2026-03-26 but never tested. It cannot compile current B++ source — the lexer/parser are outdated. Marked as P0 in the TODO. Without a working bootstrap.c, B++ cannot be distributed to platforms where a pre-built binary isn't available.

### Files Changed

| File | Change |
|------|--------|
| `src/aarch64/a64_codegen.bsm` | float_ret/float_ret2, frame +16 bytes, d0/d1 saves, var_is_float respects hints |
| `src/x86_64/x64_codegen.bsm` | Same changes for x86_64 |
| `src/bpp_import.bsm` | `hash_with_deps()` replaces `propagate_stale()` |
| `src/bpp.bpp` | Calls `hash_with_deps` instead of `propagate_stale` |
| `src/bpp_types.bsm` | Removed global float auto-promotion |
| `src/bpp_validate.bsm` | `float_ret` registered as builtin |
| `stb/_stb_platform_macos.bsm` | Mouse via float_ret, `: int` conversion, window close |
| `stb/stbgame.bsm` | `game_end()` / `_stb_shutdown()` |
| `bootstrap.c` | Mirrored changes (non-functional — TODO) |
| `README.md`, `docs/` | Updated |

### Verification

| Suite | Result |
|-------|--------|
| Core tests | 17/17 |
| Self-hosting | Pass (gen2 compiles test_hello) |
| Cache test | Incremental == Fresh (byte-identical) |
| Mouse tracking | Square follows mouse, X/Y in HUD |
| Window close | ESC + red X both terminate cleanly |

### Lessons Learned

- Cache invalidation bugs manifest as non-deterministic builds — the same source produces different binaries depending on cache state
- Go's hash-of-hashes approach is simple and correct — propagation-based schemes have edge cases
- Type inference should never override explicit programmer annotations
- Global type promotion is dangerous — it silently changes storage format across the entire program
- `isVisible` in the event loop (once per frame) is cheap; in `should_close` (per check) causes slowdown
- `return 0` from main terminates macOS Cocoa processes cleanly — no need for `NSApp terminate:`
- Always recompile the compiler before testing platform changes — stb modules are compiled by bpp

## 2026-03-27 (session 2) — Mach-O Fix, Packed Structs, shr/assert, C Emitter, Input Lag

Second session of the day. Fixed the Mach-O encoder, added packed structs, new builtins, fixed the C emitter, and eliminated input lag on both platforms.

### Mach-O Encoder Fix

The binary encoder had three bugs that caused SIGKILL on certain code sizes:

1. **ULEB128 hardcoded to 2 bytes**: The exports trie encoded `_main`'s offset with a fixed 2-byte ULEB128 and hardcoded `terminal_size=3`. When `_main` offset exceeded 16384 (large binaries like the compiler itself), it needed 3+ bytes. Now uses `mo_write_uleb128()` with dynamic `terminal_size`.

2. **4-byte alignment on LINKEDIT sub-offsets**: dyld requires `sizeof(void*)` alignment (8 bytes on ARM64) for chained fixups, exports trie, symtab, and strtab offsets. The encoder only aligned chained fixups to 4 bytes. Now all LINKEDIT content is 8-byte aligned.

3. **No explicit padding between LINKEDIT sections**: The layout computed offsets upfront, but writers could produce different sizes than estimated. Now explicit `mo_pad(mo_et_off - mo_pos)` calls between each section guarantee that file positions match load command offsets.

Also discovered: `cp` on macOS can invalidate the kernel's code signature cache (same inode, different content). Fix: `rm -f ./bpp` before `cp` ensures a fresh inode. Updated `install.sh`.

### shr() and assert() Builtins

`shr(value, shift)` — logical right shift (unsigned, fills with zeros). Unlike `>>` which is arithmetic (sign-extending), `shr` uses LSRV on ARM64 and SHR CL on x86_64. Critical for bit manipulation on unsigned values.

`assert(cond)` — traps if condition is zero. ARM64: CBNZ over BRK #0. x86_64: TEST+JNE over INT3. Zero overhead when condition is true (single branch, predicted taken).

### Packed Structs

Struct fields can now have type hints that determine their byte size:

```
struct Pixel { r: byte, g: byte, b: byte, a: byte }  // 4 bytes total
struct Fat   { r, g, b, a }                            // 32 bytes (8 per field, unchanged)
```

Implementation:
- Parser: `sd_fhints` parallel array stores per-field type hints. `add_struct_field` computes cumulative offsets. `get_field_offset` sums actual field sizes instead of `i * 8`.
- Codegen: `T_MEMLD` and `T_MEMST` nodes carry the field hint in `n.b`/`n.c`. ARM64 emits LDRB/STRB for byte, LDRH/STRH for quarter, LDR W/STR W for half.
- `sizeof(Packed)` correctly returns 4, not 32.

Backward compatible: fields without hints remain 8 bytes.

### C Emitter Fix

The C emitter's syscall wrappers used inline assembly (`svc #0x80`) which broke under `-O2` — the compiler reordered stores to `_bpp_scratch` before the asm fence, corrupting output. Fixed by replacing all inline asm syscall wrappers with libc calls (`write()`, `read()`, `open()`, `close()`). The C emitter already links libc for malloc/free/fork.

Added `#include <fcntl.h>` for `open()`.

### Global Float Type Hints

`auto x: float` at global scope now correctly registers the variable as TY_FLOAT via `ty_set_global_type()` in `parse_global()`. Previously, the `: float` annotation was parsed but the `_prim_hint` was never applied to globals — only to locals and struct fields.

### Input Lag Elimination (macOS + Linux)

Moved input polling from `game_frame_begin()` to the end of `_stb_present()`, after the frame sleep. This ensures input is read as close as possible to when it's used, eliminating the 1-frame delay caused by polling before sleep.

Also replaced fixed-duration sleep with smart frame timing: `remaining = budget - elapsed`. Only sleeps the time left in the frame budget, not a full frame duration regardless of render time.

### Key Event Beep Fix

macOS Cocoa plays an alert beep when key events are forwarded via `sendEvent:` without a responder. Fixed by filtering out `NSEvKeyDown` and `NSEvKeyUp` before calling `sendEvent:` — B++ consumes key events directly from the event stream.

### Files Changed

| File | Change |
|------|--------|
| `src/aarch64/a64_macho.bsm` | ULEB128 dynamic, 8-byte alignment, explicit LINKEDIT pads |
| `src/aarch64/a64_codegen.bsm` | shr(), assert(), packed T_MEMLD/T_MEMST |
| `src/x86_64/x64_codegen.bsm` | shr(), assert(), packed T_MEMLD/T_MEMST |
| `src/bpp_parser.bsm` | sd_fhints, packed field offsets, global `: float` registration |
| `src/bpp_validate.bsm` | shr, assert registered as builtins |
| `src/bpp_emitter.bsm` | libc wrappers replace inline asm, fcntl.h |
| `stb/_stb_platform_macos.bsm` | Smart frame timing, post-sleep poll, key filter |
| `stb/_stb_platform_linux.bsm` | Smart frame timing, post-sleep poll |
| `stb/stbgame.bsm` | game_frame_begin simplified, first-frame poll |
| `install.sh` | rm before cp for fresh inode |

### Verification

| Suite | Result |
|-------|--------|
| ARM64 | 19/19 (including test_packed, test_builtins) |
| Linux | 6/6 compile, 5/6 run in Docker |
| C emitter | PASS with -O2 |
| Self-host | gen2 compiles and runs |
| MCU game | Cat chases rat, smooth movement with `: float` globals |
| Mouse test | Cursor tracking at 60fps, no lag, no beep |

### Lessons Learned

- Mach-O LINKEDIT requires 8-byte aligned offsets — Apple's dyld silently rejects misaligned binaries
- ULEB128 sizes must be computed dynamically, never hardcoded — binary size determines encoding width
- `cp` on macOS preserves content but can invalidate kernel code signature cache — always `rm` first
- C compiler optimizations (-O2) can reorder stores past inline asm even with "memory" clobber — use libc wrappers
- Global `: float` annotations must be explicitly registered — the parser reads the hint but must apply it
- Input polling belongs after the frame sleep, not before — eliminates 1 frame of lag
- Fixed-time sleep wastes frame budget — subtract elapsed time for responsive input
- Packed structs are backward compatible: no hint = 8 bytes per field, same as before
- MCU game is the best integration test — catches type system regressions immediately

---

## 2026-03-27 — Type System Refactor: Base × Slice

### The Change

Replaced the flat type enum (TY_BYTE=1, TY_QUART=2, TY_HALF=3, TY_WORD=4, TY_PTR=5, TY_FLOAT=6, TY_STRUCT=7) with an orthogonal base × slice system packed in a single byte.

5 base types (lower nibble): UNKNOWN(0), WORD(1), FLOAT(2), PTR(3), STRUCT(4)
5 slices (upper nibble): FULL(0), HALF(1), QUARTER(2), BYTE(3), DOUBLE(4)

Encoding: `base | (slice × 16)`. 25 composed type constants in a 5×5 grid. Every base can combine with every slice — the type system is a LEGO set.

```
              BYTE(8)  QUARTER(16)  HALF(32)  FULL(64)  DOUBLE(128)
UNKNOWN       0x30      0x20        0x10      0x00       0x40
WORD          0x31      0x21        0x11      0x01       0x41
FLOAT         0x32      0x22        0x12      0x02       0x42
PTR           0x33      0x23        0x13      0x03       0x43
STRUCT        0x34      0x24        0x14      0x04       0x44
```

### What Changed

- **defs.bsm**: 25 type constants, 10 building blocks (5 bases + 5 slices), 6 helper functions (ty_base, ty_slice, ty_make, is_float_type, is_int_type, slice_width)
- **bpp_types.bsm**: promote() rewritten with base+slice logic — float+wider-int widens float to avoid precision loss. ty_set_var_type() uses promote() instead of fragile `>` comparison. type_of_lit() returns TY_WORD for all integer literals (programmer chooses slice).
- **bpp_parser.bsm**: try_type_annotation() supports compound "half float" → TY_FLOAT_H and "quarter float" → TY_FLOAT_Q. field_hint_size() uses slice_width(). Global float registration uses is_float_type().
- **bpp_emitter.bsm**: emit_type_for() dispatches by base then slice. T_DECL grouping uses ty_slice().
- **bpp_validate.bsm**: W002 uses is_float_type().
- **a64_codegen.bsm + x64_codegen.bsm**: All `== TY_FLOAT` checks use is_float_type(). All sub-word checks use ty_slice() == SL_BYTE/SL_QUARTER/SL_HALF.

### Why

The flat enum mixed "what it is" (word vs float vs ptr) with "how wide" (byte vs half vs full) in one axis. This:
- Blocked f32/f16 (needed for audio/GPU)
- Made promote() depend on fragile numeric ordering
- Required manual additions to 15+ places for every new type
- Conflated literal magnitude with variable storage width

The new system separates concerns into two orthogonal axes. Adding a new width or base type is additive, not multiplicative.

### Test Results

| Suite | Result |
|-------|--------|
| Native (17 tests) | PASS |
| Bootstrap verify | Byte-identical |
| C emitter | PASS |
| Snake native | Compiles and runs |
| MCU game | Runs correctly |
| Mouse tester | Cursor tracking at 60fps |

### Lessons Learned

- B++ functions return one value — this made two-field types impractical. Packed encoding (base | slice << 4) is the right fit for a single-return language.
- type_of_lit() returning TY_BYTE for small literals was a design mistake — literal magnitude should not determine variable storage width. Slices are programmer opt-in.
- The codegen already implemented the base+slice decision in two steps ("is it float?" then "which sub-word?"). The refactor just named what was already there.
- promote() must widen float to at least the integer operand's width: promote(f32, i64) = f64, not f32. Returning the narrower float loses 41 bits of precision.
- SL_DOUBLE (128-bit) slot is reserved for future SIMD/NEON even though no codegen supports it yet — costs zero to include.

## 2026-03-30 — .bo Cache Fix + Compiler Self-Hash

### The Bug

Modular compilation with `.bo` caching had a critical bug: `bo_read_export()` did not push to `sd_fhints` when loading structs from cache. When `add_struct_field()` subsequently called `arr_get(sd_fhints, idx)`, it read beyond the array bounds, got NULL, and crashed with `*(NULL + offset) = value` (SIGSEGV).

This meant the second bootstrap stage always crashed when the cache was populated — `bpp` compiled `bpp2` (populating cache), but `bpp2` crashed compiling `bpp3` (loading corrupt cache). The bug was masked by manually clearing `.bpp_cache/` before each bootstrap.

### Root Cause

In `src/bpp_bo.bsm`, `bo_read_export()` for structs was missing one line compared to `add_struct_def()` in the parser:

```
// Parser (add_struct_def) — correct:
sd_fhints = arr_push(sd_fhints, malloc(64 * 8));

// bo_read_export — was missing this line entirely
```

Additionally, `add_struct_field()` was called with 2 arguments instead of 3 (missing the `hint` parameter).

### Fix

Two lines added to `bo_read_export()`:
1. `sd_fhints = arr_push(sd_fhints, malloc(64 * 8));` — initialize hints array for cached structs
2. `add_struct_field(idx, bo_read_str(), 0);` — pass explicit hint=0

### Compiler Self-Hash

Added `compiler_self_hash()` to `bpp_import.bsm`. The cache manifest hash now includes the FNV-1a hash of the first 8KB of the compiler binary (`argv[0]`). When the compiler changes, the entire cache is invalidated automatically — no manual clearing needed.

Uses a private `malloc(8192)` buffer to avoid corrupting `_cache_buf` which is shared by `cache_load()` and `cache_save()`.

### .gitignore Update

`.bpp_cache/` directory is now tracked in git (via `.gitkeep`) so it always exists after clone. Only the contents (`.bo` files and `hashes`) are gitignored.

### Verification

| Test | Result |
|------|--------|
| Bootstrap (bpp == bpp2 == bpp3 with cache) | PASS |
| 13 native tests (macOS ARM64) | PASS |
| 13 cross-compiled tests (Linux x86_64 via Docker) | PASS |
| Snake native (Cocoa) | Compiles and runs |
| MCU game | Compiles and runs |
| Linux ELF generation | Valid ELF64 binary |

### Files Changed

- `src/bpp_bo.bsm` — 2 lines added (sd_fhints init + hint arg)
- `src/bpp_import.bsm` — compiler_self_hash() + manifest hash integration
- `.gitignore` — track .bpp_cache/ directory, ignore contents
- `.bpp_cache/.gitkeep` — ensure directory exists in repo

## 2026-03-31 — Optimized Type Encoding + FLOAT_H/Q Codegen

### Milestone

Type system encoding redesigned: bit 0 = float flag, 14 phantom types removed, `is_float_type(ty) = ty & 1` (1 ARM64 instruction). All float sub-types now have working codegen on ARM64 and x86_64 cross-compilation.

### Type Encoding Change

Old encoding (5x5 grid): `base | (slice * 16)` with 25 constants, 12 unused.

New encoding (bit-0 float flag):
```
bit 0     = float flag (1 = float, 0 = not float)
bits 0-3  = base (WORD=0x02, FLOAT=0x03, PTR=0x04, STRUCT=0x08, UNK=0x0C)
bits 4-5  = slice (FULL=0, HALF=1, QUARTER=2, BYTE=3)
bits 6-7  = free (future use)
```

11 real types. `is_float_type()` reduced from AND+CMP+BRANCH (3 insns) to single TBNZ (1 insn). `is_int_type()` and `SL_DOUBLE` removed (zero callers). Only `defs.bsm` changed — all other files use named constants.

### FLOAT_H / FLOAT_Q Codegen

ARM64 (a64_codegen.bsm + a64_enc.bsm):
- `a64_emit_load_var`: FLOAT_H loads s-register + FCVT d,s; FLOAT_Q loads h-register + FCVT d,h
- `a64_emit_store_var`: FCVT s,d + STR s (FLOAT_H); FCVT h,d + STR h (FLOAT_Q)
- T_MEMLD/T_MEMST: float field types in packed structs (load/store + widen/narrow)
- Param narrowing in a64_emit_func prologue
- 15 new encoder functions: single-precision arithmetic, conversion, load/store, half-precision conversion and load/store

x86_64 (x64_enc.bsm):
- MOVSS load/store, CVTSS2SD, CVTSD2SS encoder functions
- Codegen updates deferred (multi-statement if-block bug in x64 self-compile)

### Parser Fix: half float / quarter float

The compound type annotation `auto x: half float` triggered an infinite loop due to the multi-statement if-block codegen bug. Fix: extracted `_parse_half_hint()` and `_parse_quarter_hint()` helper functions with flat guard patterns (no nested if-blocks).

### putchar_err Builtin

Write 1 byte to stderr (fd=2). Same as putchar but for diagnostic output.

### ELF Writer Bug Found

`sys_open(filename, 0x601)` uses macOS O_CREAT/O_TRUNC flags. Linux uses different values (O_CREAT=0x40, not 0x200). Fix: changed to 0x241 for Linux ELF writer. Files were only created when pre-existing (O_TRUNC without O_CREAT).

### macOS Code Signing Cache Discovery

macOS kernel caches code signing decisions per filesystem path. When a binary at a path is replaced (e.g., `bpp2` overwritten by compilation), the kernel may kill the new binary with SIGKILL (exit 137) based on the cached decision from the old binary. Workaround: copy binary to a fresh path in `/tmp/` before executing.

### Verification

| Test | Result |
|------|--------|
| ARM64 bootstrap convergence | PASS (SHA 19beb13d) |
| x86_64 cross-compile programs | PASS |
| FLOAT_H arithmetic (add, sub, mul, div) | PASS |
| FLOAT_Q (16-bit half float) | PASS |
| WORD_H (32-bit int) | PASS |
| putchar_err | PASS |
| half float parser | PASS |
| quarter float parser | PASS |

### Files Changed

- `src/defs.bsm` — Encoding values, helper functions, remove 14 phantoms
- `src/aarch64/a64_enc.bsm` — 15 new encoder functions (single + half precision)
- `src/aarch64/a64_codegen.bsm` — FLOAT_H/Q in load/store/memld/memst/params, putchar_err
- `src/bpp_parser.bsm` — _parse_half_hint, _parse_quarter_hint helpers
- `src/x86_64/x64_enc.bsm` — MOVSS/CVTSS2SD/CVTSD2SS encoder functions
- `src/x86_64/x64_elf.bsm` — sys_open flags fix (0x601 → 0x241)

---

## 2026-03-31b — GPU Rendering via Metal + Go-Style Cache

### GPU Rendering (stbrender)

First GPU rendering in B++. Metal backend embedded in `_stb_platform_macos.bsm` (one file per platform). Public API in `stb/stbrender.bsm`.

**Working API:**
- `render_init()` — creates Metal device, command queue, CAMetalLayer, compiles shader, builds pipeline
- `render_begin()` — gets drawable, opens render pass with clear color
- `render_clear(color)` — sets clear color (ARGB packed, same as stbdraw)
- `render_rect(x, y, w, h, color)` — batched colored rectangle (6 vertices per quad)
- `render_end()` — flushes batch, presents, frame timing, input polling

**Vertex format:** 8 bytes per vertex (int16 x,y + uint8 r,g,b,a). All integer — no float writes from B++. Shader converts int→float on GPU.

**Shader:** Embedded as string literal, compiled at runtime via `[MTLDevice newLibraryWithSource:options:error:]`. Shader source built with `sb_cat()` + `\n` escape sequences.

**Architecture decision:** GPU code lives IN the platform file. `stbrender.bsm` is platform-agnostic. `_stb_gpu_*` functions are platform implementations.

### String Escape Sequences

`scan_string()` in lexer now processes escape sequences, matching `scan_char()` behavior:
- `\n` (10), `\t` (9), `\r` (13), `\0` (0)
- `\\` (92), `\"` (34)
- `\xHH` (hex byte)

Processed bytes written directly to vbuf; token length reflects decoded output.

### Platform Architecture Refactor

**stbgame imports platform internally.** Game files no longer need `import "_stb_platform.bsm"`. Removed from 4 files. Driver override still works (last-definition-wins).

**Frame timing and input polling extracted from platform files.** Platform only provides raw operations:
- `_stb_present()` — blit framebuffer (macOS: CoreGraphics, Linux: ANSI)
- `_stb_gpu_present()` — flush batch + Metal commit
- `_stb_frame_wait()` — sleep remaining frame budget (macOS: usleep, Linux: nanosleep)

Orchestration moved to agnostic modules:
- `draw_end()` = `_stb_present()` + `_stb_frame_wait()` + `_input_save_prev()` + `_stb_poll_events()`
- `render_end()` = `_stb_gpu_present()` + `_stb_frame_wait()` + `_input_save_prev()` + `_stb_poll_events()`

### Go-Style Build Cache

Migrated from per-project `.bpp_cache/` to global `~/.bpp/cache/`. Files named by content hash.

**Old system:**
- `.bpp_cache/stb_stbgame.bo` (filename = module path)
- `.bpp_cache/hashes` (text file with manifest + per-module hashes)
- 3-layer validation (manifest hash + source hash + dependency hashes)

**New system:**
- `~/.bpp/cache/b7a1df7edb63d9cd.bo` (filename IS the hash)
- No hashes file needed
- Validation = file exists (hash encodes source + deps + compiler version)
- Compiler hash mixed into every module hash (upgrading bpp invalidates all)
- Shared across projects (stbdraw compiles once, any project reuses)

Added `_getenv("HOME")` (pure B++, reads environ from argv memory), `cache_init()` (creates `~/.bpp/` and `~/.bpp/cache/`), `cache_init_stale()`.

Removed `cache_load()`, `cache_save()`, `cache_manifest_hash()`, `_cache_buf`.

### Other Changes

- `sys_mkdir(path, mode)` builtin: ARM64 (syscall 136), x86_64 (syscall 83), C emitter. Not yet in validator.
- `init_str()` called by `game_init()` so `sb_new()`/`sb_cat()` work for GPU shader builder.
- macOS number key mapping: KEY_0–KEY_9 added to `_plt_map_vkey()`.
- Parser workarounds removed: `_parse_half_hint`, `_parse_quarter_hint`, `_check_next_float` inlined back into `try_type_annotation`. Multi-statement if-blocks confirmed working.
- `.bo` header version bumped to 3 (no hash data in header, simplified to magic + version + target).

### RESOLVED: "Mach-O Writer Bug" Was Cache Bug

The SIGSEGV when adding functions was NOT a Mach-O writer bug. It was the .bo cache serving stale object files. Root cause: `compiler_self_hash()` only read 8KB of the compiler binary — insufficient to detect code changes past the first few modules.

---

## 2026-03-31c — Cache Fixes + GPU Primitives + Architecture

### Cache Bug Fixes (3 separate bugs)

**Bug 1: compiler_self_hash() read only 8KB.** The compiler binary is ~347KB. Changes to modules compiled after the first 8KB were invisible to the cache. Fix: read entire binary in 8KB chunks, accumulate hash.

**Bug 2: Module 0 (top-level .bpp) not isolated in outbuf.** The modular pipeline used `lex_module(0)` which got the wrong source range because module 0's content is at the END of outbuf (after all imports), not at position 0 where `diag_file_starts[0]` pointed. Fix: track `mod0_real_start` (set after last import), compile module 0 separately after the cache loop using the correct range.

**Bug 3: mod_idx() misattributed module 0 tokens.** The binary search in `mod_idx()` returned the last stb module for tokens belonging to module 0 (because module 0's content is past all other modules in outbuf). This caused the last stb module's .bo to contain module 0's functions and strings. Fix: special case in `mod_idx()` — if `src_off >= mod0_real_start`, return 0.

**Bug 4: lex_module for last module included module 0 content.** `lex_module(mk-1)` lexed from `starts[mk-1]` to `src_len`, which included module 0's code at the end. Fix: stop at `mod0_real_start` instead of `src_len`.

### Validator Updates

Registered `sys_mkdir` and `putchar_err` as builtins in `bpp_validate.bsm`. C backend compilation no longer fails on these.

### C Emitter Fixes

- `emit_c_str()`: re-escapes `\n`, `\t`, `\r`, `\0`, `\\`, `\"` in string literals. The lexer now processes escape sequences into raw bytes, so the C emitter must re-escape them for valid C output.
- Added `#include <sys/stat.h>` for `mkdir()` in the C runtime.

### GPU Render API Expansion

Added to `stbrender.bsm` (platform-agnostic, uses `_stb_gpu_vertex` + `_stb_gpu_rect`):
- `render_line(x0, y0, x1, y1, color)` — axis-aligned as 1px rect, diagonal as thin quad
- `render_circle(cx, cy, radius, color)` — triangle fan, 32 segments
- `render_circle_outline(cx, cy, radius, color)` — line segments
- `render_rect_outline(x, y, w, h, color)` — 4 lines

### Trigonometry Moved to stbmath.bsm

Fixed-point cos/sin table (32 segments, ×1024) moved from `_stb_platform_macos.bsm` to `stbmath.bsm`. Functions `math_cos(i)` and `math_sin(i)` are platform-agnostic. Geometry functions in stbrender use these instead of per-backend trig tables. `math_trig_init()` called by `game_init()`.

Removed `_stb_gpu_line`, `_stb_gpu_circle`, `_stb_gpu_circle_outline` from macOS backend. Backend now only provides raw primitives: `_stb_gpu_vertex`, `_stb_gpu_rect`, `_stb_gpu_clear`, `_stb_gpu_begin`, `_stb_gpu_present`.

### Architecture Decision: GPU Philosophy

GPU rendering is native per platform via codegen/FFI. No third-party APIs (no SDL, no GLFW).
- macOS ARM64: Metal via objc_msgSend (DONE)
- Linux x86_64: Vulkan via libvulkan.so FFI (PLANNED)
- Windows x86_64: DirectX 12 or Vulkan (FUTURE)

`stbrender.bsm` is identical on all platforms. Backend primitives (`_stb_gpu_*`) are platform-specific.

### Verification

| Test | Result |
|------|--------|
| Bootstrap convergence (gen2=gen3) | PASS |
| Cache: different programs get correct strings | PASS (rect→circle→rect) |
| Cache: no manual clearing needed | PASS |
| test_hello (no imports) | PASS |
| test_gpu_circle (filled circle) | PASS |
| test_gpu_shapes (lines, circles, outlines, rects) | PASS |
| test_gpu_rect (WASD movement) | PASS |

### Files Changed

- `src/bpp_import.bsm` — compiler_self_hash reads full binary, mod0_real_start tracking, mod_idx special case for module 0
- `src/bpp_lexer.bsm` — lex_module stops at mod0_real_start for last module
- `src/bpp_validate.bsm` — sys_mkdir + putchar_err registered
- `src/bpp_emitter.bsm` — emit_c_str, sys/stat.h include
- `src/bpp.bpp` — module 0 compiled separately after cache loop
- `stb/stbmath.bsm` — math_cos/math_sin, fixed-point trig table
- `stb/stbgame.bsm` — math_trig_init() in game_init
- `stb/stbrender.bsm` — render_line, render_circle, render_circle_outline, render_rect_outline
- `stb/_stb_platform_macos.bsm` — removed geometry functions (line/circle/outline)
- `tests/test_gpu_shapes.bpp` — NEW: visual test for all render primitives
- `tests/test_gpu_circle.bpp` — NEW: minimal circle test
- `tests/test_syntax.bpp` — NEW: tests for else if, buf[i], for loop

---

## 2026-03-31d — Syntax Sugar + Codebase Cleanup

### New Syntax (parser-only, zero codegen changes)

**`buf[i]`** — Array indexing desugars to `*(buf + i * 8)` for reads, `*(buf + i * 8) = val` for writes. Added in `parse_primary()` after variable resolution. When parser sees `[`, it builds T_BINOP(+, base, T_BINOP(*, index, 8)) wrapped in T_MEMLD. Assignment to `buf[i]` works automatically because the existing `T_MEMLD = val` → `T_MEMST` conversion handles it.

**`for (init; cond; step) { body }`** — Desugars to `init; while (cond) { body; step; }`. Implemented via `_for_init` global: `parse_for()` stashes the init statement, returns T_WHILE with step appended to body. `parse_body()` and `parse_function()` check `_for_init` and insert before the while node. Added `for` to lexer keywords.

**`else if`** — In `parse_if()`, when `else` is followed by `if`, parse inner `parse_if()` directly as single-element else body. No extra braces needed.

**`print_int` availability** — `stbgame.bsm` now imports `stbio.bsm`. Any program using stbgame gets `print_int`, `print_msg`, `print_ln` automatically.

### Codebase Cleanup (~230 conversions)

Applied the new syntax to the compiler source, stb library, and examples:
- ~110 `*(arr + i * 8)` → `arr[i]` conversions across 8 compiler files
- ~120 `while` → `for` loop conversions across 11 files
- Examples and tests updated with new syntax

### Files Changed

- `src/bpp_lexer.bsm` — `for` keyword added to check_kw
- `src/bpp_parser.bsm` — parse_for(), else if in parse_if(), buf[i] in parse_primary(), _for_init handling in parse_body() and parse_function()
- `src/bpp_emitter.bsm` — ~95 syntax conversions (buf[i] + for loops)
- `src/bpp_types.bsm` — ~50 syntax conversions
- `src/bpp_dispatch.bsm` — ~29 syntax conversions
- `src/bpp_validate.bsm` — ~9 syntax conversions
- `src/bpp_import.bsm` — ~33 syntax conversions
- `src/bpp_bo.bsm` — ~35 syntax conversions
- `src/bpp_parser.bsm` — ~15 syntax conversions
- `src/bpp_diag.bsm` — ~7 syntax conversions
- `src/bpp.bpp` — ~16 syntax conversions
- `stb/stbgame.bsm` — imports stbio.bsm, math_trig_init() call
- `stb/stbmath.bsm` — for loop conversions
- `examples/*.bpp` — updated with new syntax
- `tests/test_syntax.bpp` — NEW: validates all three syntax sugars

### Verification

| Test | Result |
|------|--------|
| ARM64 bootstrap convergence | PASS (SHA 88a5a9da) |
| Parser workaround removal | PASS (half float, quarter float) |
| String escape sequences | PASS (\n, \t, \xHH, \\, \") |
| GPU clear (Metal) | PASS (color switching 1-6) |
| GPU rectangles (Metal) | PASS (5 rects + WASD movement) |
| snake_native (software) | PASS |
| SDL/raylib examples | Compile OK |
| Linux cross-compile (6 tests) | PASS |
| C emitter output | PASS |
| Go-style cache location | PASS (~/.bpp/cache/) |
| Cache invalidation | PASS (hash changes on source edit) |
| 13 unit tests | PASS |

### Files Changed

- `src/bpp_lexer.bsm` — escape sequences in scan_string, _hex_val helper
- `src/bpp_parser.bsm` — removed workaround helpers, inlined half/quarter parsing
- `src/bpp_import.bsm` — _getenv, cache_init, cache_init_stale, compiler hash in deps, removed cache_load/cache_save/cache_manifest_hash
- `src/bpp_bo.bsm` — hash-based bo_make_path, v3 header, _bo_init_hex
- `src/bpp_emitter.bsm` — sys_mkdir builtin + runtime
- `src/bpp_validate.bsm` — (needs sys_mkdir registration, TODO)
- `src/aarch64/a64_codegen.bsm` — sys_mkdir builtin
- `src/x86_64/x64_codegen.bsm` — sys_mkdir builtin
- `src/bpp.bpp` — cache_init, _bo_init_hex, removed cache_save
- `stb/stbgame.bsm` — imports _stb_platform + stbstr, init_str in game_init
- `stb/stbdraw.bsm` — draw_end orchestrates present+wait+poll
- `stb/stbrender.bsm` — NEW: GPU rendering public API
- `stb/_stb_platform_macos.bsm` — Metal GPU backend, _stb_frame_wait, number keys, removeFromSuperview fix
- `stb/_stb_platform_linux.bsm` — _stb_frame_wait extracted
- `tests/test_gpu_clear.bpp` — NEW: GPU clear with color switching
- `tests/test_gpu_rect.bpp` — NEW: GPU rectangles with movement
- `tests/test_escape.bpp` — NEW: escape sequence validation
