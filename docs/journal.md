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

### Pending

- **stbcol.bsm** — Collision detection module created (rect_overlap, circle_overlap, rect_contains, circle_contains). Not yet tested or integrated into any game. MCU has inline collision that should be refactored to use stbcol.

---

