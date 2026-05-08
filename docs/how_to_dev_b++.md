# How to Dev B++ — The Programmer's Book

**The K&R-style entry point for writing programs in B++.** The
language. The arsenal. The discipline. Everything you need from "I
have never seen B++" to "I am writing a non-trivial program in it."

This is one of four canonical books that ship with B++:

| Book | Audience | What's inside |
|---|---|---|
| **how_to_dev_b++.md** (this one) | someone writing a B++ program | language tour, stdlib essentials, idioms, compiler flags |
| `tonify_checklist.md` | anyone contributing code | the 20 style rules, applied from the first keystroke |
| `bootstrap_manual.md` | anyone hacking the compiler itself | bootstrap cycle, backend layout, portability tiers, adding builtins |
| `standard_b++_lib.md` | anyone using the game engine library | every `stb*.bsm` module in depth + Bang 9 + bundled tools |

If your question is about **writing a program**, you are in the
right book. If it is about **building or extending the compiler**,
go to `bootstrap_manual.md`. If it is about **a specific stb module**,
go to `standard_b++_lib.md`. If it is about **how to write code
expert-style**, go to `tonify_checklist.md`.

---

## Table of Contents

| Cap | Title | Part |
|-----|-------|------|
| 1 | Hello World | I — Começo |
| 2 | The Auto-Injected Prelude | I |
| 3 | Syntax | I |
| 4 | Types | I |
| 5 | Functions and Effects | I |
| 6 | Strings (bpp_str + strbuf) | II — Arsenal |
| 7 | Arrays (bpp_array + bpp_buf) | II |
| 8 | Hash tables (bpp_hash) | II |
| 9 | Buffers (bpp_buf) | II |
| 10 | Structs | II |
| 11 | Enums | II |
| 12 | I/O (bpp_io) | II |
| 13 | Runtime ABI (bpp_runtime) | II |
| 14 | Tonify — see `tonify_checklist.md` | III — Writing Pro |
| 15 | Disciplina QG (general vs battalion) | III |
| 16 | Idioms | III |
| 17 | Anti-patterns | III |
| — | Compiler dev — see `bootstrap_manual.md` | IV+V (REMOVED) |
| Apx | Standard Library Tour — see `standard_b++_lib.md` for depth | VI |
| 48 | Compiler Flags Reference | last |

Twelve language + arsenal chapters, four discipline chapters, one
compiler-flags reference, one one-paragraph-per-module library tour,
and pointers to the three companion books for everything else.

---

# Part I — Começo

Everything you need to write your first line of B++ and know it works.

---

## Cap 1 — Hello World

*Depends on: —*
*Source: new*
*Status: COMPLETE — 2026-04-21*

The minimum B++ program is three lines. No headers, no imports, no return statement:

```c
main() {
    put("Hello, World\n");
}
```

Compile and run:

```bash
bpp hello.bpp -o hello
./hello
# Hello, World
```

That is everything. `put(x)` dispatches at compile time by the type of `x` — string, integer, or float. `main` returns 0 implicitly when no explicit return. No `#include`, no `import`. How that works is Cap 2.

### Variations

Print an integer:

```c
main() {
    put(42);
    putchar('\n');
}
```

Interpolate:

```c
main() {
    auto score;
    score = 150;
    put("score: ");
    put(score);
    putchar('\n');
}
```

B++ has no `printf` format strings. You compose output with `put` calls — the compiler routes each call to `putstr`, `putnum`, or `putfloat` based on the argument type. For newlines, use `putchar('\n')` or `putmsg(s)` (which appends `\n` automatically). Build multi-part strings with `strbuf_*` (Cap 6).

Byte-level output when you need it:

```c
main() {
    putchar('H');
    putchar('i');
    putchar('\n');
}
```

`putchar(c)` writes one byte to stdout. It is the lowest-level output in the language — every other print helper builds on it.

### Where the three-line program lives in the repo

`examples/hello_world.bpp`. Open Bang 9 (or your editor), point at `examples/`, and this file plus a dozen siblings (`hello_window.bpp`, `snake_cpu.bpp`, `ui_demo.bpp`) are the learning on-ramp. Read the source, run the binary, modify, rebuild.

---

## Cap 2 — The Auto-Injected Prelude

*Depends on: Cap 1*
*Source: new (documents `src/bpp_import.bsm`:601-750 implicit behaviour)*
*Status: COMPLETE — 2026-04-21*

Every B++ program starts with nine modules already in scope. You do not need to `import` them. Their functions are globals. This is the "prelude" — the libraries the compiler auto-injects into every compilation unit.

### The prelude

| Module | What it gives you | Examples |
|--------|-------------------|----------|
| `_core.bsm` (per-OS: `_core_macos.bsm` / `_core_linux.bsm`) | Memory allocator + runtime base | `malloc`, `free`, `realloc`, `memcpy`, `memset` |
| `bpp_io.bsm` | stdout / stderr output, stdin input | `put`, `put_err`, `putchar`, `putchar_err`, `putstr`, `putnum`, `putfloat`, `putline`, `putmsg`, `getchar`, `getenv` |
| `bpp_str.bsm` | C-string operations + dynamic string builder | `str_len`, `str_eq`, `str_cpy`, `str_ends`, `str_starts`, `str_dup`, `strbuf_new`, `strbuf_cat`, `strbuf_ch`, `strbuf_num`, `strbuf_len`, `strbuf_free` |
| `bpp_array.bsm` | Dynamic arrays with 16-byte shadow header | `arr_new`, `arr_push`, `arr_pop`, `arr_get`, `arr_set`, `arr_len`, `arr_free`, `arr_clear`, `arr_last` |
| `bpp_buf.bsm` | Raw byte/word buffers + typed LE/BE read/write | `buf_byte`, `buf_word`, `buf_fill`, `buf_copy`, `read_u8/16/32/64`, `write_u8/16/32/64`, `read_u16be/u32be`, `write_u16be/u32be` |
| `bpp_hash.bsm` | hash tables (word keys + string keys) | `hash_new`, `hash_set`, `hash_get`, `hash_str_new`, `hash_str_set` |
| `bpp_math.bsm` | pure B++ sqrt, sin, cos | `sqrt`, `sin`, `cos`, `abs`, `min`, `max` |
| `bpp_file.bsm` | file I/O | `file_read_all`, `file_write_all` |
| `_stb_platform.bsm` (per-OS) | window, clock, input primitives | `_stb_init_window`, `_stb_get_time`, `_stb_poll_events`, `_stb_frame_wait`, `_stb_should_close` |

### Why this works

The compiler (`src/bpp_import.bsm`) auto-injects these nine modules at the top of every compilation unit. You see their names as if they were built-in — they are not. They are real B++ code in `src/bpp_*.bsm` and `src/backend/os/<os>/_core_<os>.bsm`. The compiler just makes them visible without requiring an explicit `import` statement.

Two groups of functions are NOT in this prelude and need `import`:

1. **`stb/*.bsm`** — game / tool libraries (`stbgame`, `stbdraw`, `stbui`, `stbaudio`, `stbtile`, ...). These are the game engine; import as needed.
2. **Compiler-internal** modules (`bpp_codegen`, `bpp_parser`, `bpp_lexer`, `bpp_internal`, `bpp_types`, ...). These are for people writing the compiler itself. User programs should not import them.

### Consequence for Hello World

The three-line `main() { put("Hello, World\n"); }` works because:
- `main` is the standard entry point the compiler looks for
- `put` is a function in `bpp_io.bsm`, which was auto-injected
- The return value defaults to 0 when you do not write `return`

That is the entire reason the minimum program is three lines instead of the seven or eight a non-prelude language would need. The prelude is B++'s productivity floor.

### Consequence for the tick loop

A windowed program also needs no `import`:

```c
main() {
    _stb_init_window(320, 180, "Tool", 60);
    while (_stb_should_close() == 0) {
        _stb_poll_events();
        _stb_frame_wait();
    }
}
```

`_stb_init_window`, `_stb_should_close`, `_stb_poll_events`, `_stb_frame_wait` all come from `_stb_platform.bsm`, auto-injected. The tick loop is written by YOUR `main()` — neither `stbgame` nor the platform layer runs a tick on your behalf. See Cap 26 for when to use `stbgame`'s 60-FPS wrappers instead of raw platform primitives.

---

## Cap 3 — Syntax

*Depends on: Cap 1*
*Source: legacy_docs/the_b++_programming_language.md §4-§7*
*Status: COMPLETE — 2026-04-23*

B++ syntax is deliberately small — C-family without the C footguns
that stopped mattering 40 years ago. Every construct you'll use in a
game is covered here.

### §3.1 — Control flow

**Conditionals:**

```c
if (x > 0) {
    y = x;
} else if (x == 0) {
    y = 1;
} else {
    y = 0 - x;   // no unary minus; use 0 - x
}

// Single-statement body may omit braces:
if (x < 0) x = 0 - x;
```

**Loops:**

```c
while (i < n) {
    if (i == 5) { break; }
    sum = sum + i;
    i = i + 1;
}

for (i = 0; i < 10; i = i + 1) {
    putchar('0' + i);
}
```

`for` desugars to `while` internally. `break` exits the innermost
loop; `continue` skips to the next iteration (re-running the step
expression in a `for`).

**Switch — two forms, no fallthrough, no `break`, no `case`:**

```c
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
```

The compiler emits **W021** if a value switch has no `else`.
Inherited from B (1969), corrected.

**Short-circuit `&&` and `||`:** the parser rewrites `a && b` to
`a ? b : 0` and `a || b` to `a ? 1 : b` before codegen, so every
backend honours C-style short-circuit semantics:

```c
if (p != 0 && p.field > 0) { ... }   // memory-safe everywhere
```

**Ternary:** `cond ? a : b` — only the selected branch evaluates.
Right-associative: `a ? b : c ? d : e` parses as
`a ? b : (c ? d : e)`.

**Compile-time folding:** integer-literal expressions fold at parse
time (`3 + 5` becomes `8` in the AST). `if (0) { ... }` drops its
body; `if (1) { x } else { y }` keeps only `x`. See also Cap 48
for the Phase D parser-level optimizations (strength reduction,
identity peephole, inline trivials) that layer on top of folding.

### §3.2 — Operators

**Arithmetic:**

```
*  /  %       multiply, divide, remainder
+  -          add, subtract
-x            unary negate (note: `-literal` must be `0 - literal`)
~x            bitwise NOT
!x            logical NOT (1 if x == 0, else 0)
```

**Shift:** `<<` and `>>`. Shift amount is taken modulo the word width.

**Relational and equality:** `<  >  <=  >=  ==  !=`. Signed on
integers, IEEE on floats. Comparisons return `0` or `1` as 64-bit
words.

**Bitwise and logical:**

```
&  |  ^       bitwise AND, OR, XOR (binary only)
&&            logical AND (short-circuit)
||            logical OR  (short-circuit)
```

Unary `&x` is address-of, not bitwise AND.

**Memory:**

```
*p            dereference: read/write the word (or slice) at p
&x            address-of: take address of a local / global / field
a[i]          indexed load: *(a + i * stride)
s.f           struct field access on a typed local
```

**Assignment:** `=` plain. `+=  -=  *=  /=  %=` compound arithmetic.
`&=  |=  ^=  <<=  >>=` compound bitwise. Compound assignments
evaluate the left operand once.

### §3.3 — Literals

**Integers:** decimal (`42`), hex (`0x1F`, `0xFF00AA55`), octal
(`0755` — leading zero), character (`'A'` = 65). All 64-bit words.

**Floats:** with fractional part (`1.0`, `3.14159`) or scientific
(`1.5e3`, `2.0e-7`, `2.5E+2`). 64-bit doubles.

The scientific form requires `digits.digits` before the optional
`e`/`E` exponent — both an integer and a fractional part are
mandatory. The exponent itself is one or more decimal digits with
an optional leading `+` or `-`. The acceptance table:

| Source | Lexed as | Why |
|---|---|---|
| `1.0e10` | float | digits.digits + e + digits |
| `1.0E10` | float | E uppercase equivalent |
| `1.0e+10` | float | explicit `+` sign |
| `1.0e-10` | float | explicit `-` sign |
| `2.5E2` | float | E without sign |
| `1.5E+3` | float | uppercase E + explicit sign |
| `1e10` | int `1` + identifier `e10` | no `.`, never enters float path |
| `1.e10` | int `1` + `.` + identifier `e10` | `.` requires a digit immediately after |
| `.5e10` | `.` + int `5` + identifier `e10` | leading `.` is punctuation, not a digit |
| `1.0eFoo` | float `1.0` + identifier `eFoo` | exponent rewinds when no digit follows |

The two reasons this form is restricted, smaller to bigger:

1. **Lexer stays single-pass with one-char lookahead.** After
   consuming the integer digits and seeing a `.`, the lexer needs
   to decide *now* whether the `.` continues a float or terminates
   the integer (separator before slice annotation `: byte`,
   parameter list, or future `struct.field` access). Requiring a
   digit immediately after the `.` answers the question with
   exactly one peek.
2. **Leaves `1.field` syntax open for future extensions.** Even
   though B++ does not have method dispatch on numeric literals
   today, reserving the `digit . non_digit` shape for future
   struct/method access avoids the C/JavaScript trap where
   `1.toFixed()` is ambiguous and the parser has to special-case
   it.

**Float warning:** float literals retain IEEE bits only when
assigned to a `: float`, `: half float`, or `: quarter float`
local. An un-annotated `auto x = 44100.0;` **silently truncates
to int** and produces diagnostic **E232** — always annotate float
locals.

**Strings:** null-terminated byte sequences. Escapes: `\n` `\r`
`\t` `\\` `\"` `\0` `\xHH`. String literals are immutable; the
compiler deduplicates identical literals into the same data-section
slot.

**Character literals:** `'A'` = 65, `'\n'` = 10, `'\t'` = 9,
`'\\'` = 92, `'\''` = 39.

### §3.4 — Comments

```c
// Line comment — extends to end of line.
/* Block comment — does NOT nest. */
```

Tonify Rule 9 (Cap 14) requires one blank comment line between a
function's intro comment and its first body statement, for clean
generated documentation.

### §3.5 — What this chapter does NOT cover

- **Type annotations** (`: word`, `: half`, etc.) — Cap 4
- **Function syntax** (signatures, effect annotations) — Cap 5
- **Struct / enum declarations** — Caps 10 / 11
- **Import system** — Cap 13

---

## Cap 4 — Types

*Depends on: Cap 3*
*Source: legacy_docs/the_b++_programming_language.md §8*
*Status: COMPLETE — 2026-04-23*

B++'s type system is a **hint layer** on top of a fundamentally
untyped 64-bit word machine. Every value is a word; annotations
(`: word`, `: half`, `: float`, `: Character`) tell the compiler
how to interpret that word for load/store/arithmetic. No runtime
type tags, no boxed values, no dynamic dispatch — the hints exist
only to drive sub-word instructions and ABI layout.

### §4.1 — Base types

| Type | Slice | Size | Notes |
|------|-------|------|-------|
| `word` | SL_WORD | 8 bytes | Default; an un-annotated `auto` is a word |
| `half` | SL_HALF | 4 bytes | 32-bit integer (sub-word load/store) |
| `quarter` | SL_QUARTER | 2 bytes | 16-bit integer |
| `byte` | SL_BYTE | 1 byte | 8-bit integer |
| `bit1..bit7` | SL_BIT..SL_BIT7 | 1-7 bits | For bit-packed struct fields |
| `float` | SL_WORD | 8 bytes | IEEE 754 double; lives in FP registers |
| `half float` | SL_HALF | 4 bytes | IEEE single-precision |
| `quarter float` | SL_QUARTER | 2 bytes | IEEE half-precision |
| `double` | SL_DOUBLE | 16 bytes | 128-bit SIMD vector |
| `<struct name>` | SL_STRUCT | as-declared | User-defined record type |

### §4.2 — Slice annotations

Attach `:` + type name to any local, field, or parameter:

```c
auto hp: word;              // 64-bit int (same as un-annotated)
auto tile: byte;            // 8-bit — compiler emits strb/ldrb
auto health_ratio: float;   // 64-bit double — FP register
auto c: Character;          // struct — sized to struct_size(Character)
```

For struct fields, slice annotations control packing:

```c
struct Entity {
    hp: half,              // offset 0, 4 bytes
    level: byte,           // offset 4, 1 byte
    flags: bit4,           // offset 5, 4 bits (packed with next)
    team: bit2,            // offset 5, 2 bits (same byte as flags)
    pos_x, pos_y,          // offset 8, 16 — plain words
    vel: double            // offset 24, 16-byte SIMD
}
```

### §4.3 — How hints drive codegen

The compiler emits the NARROWEST instruction that fits:

- `poke(buf + i)` byte-level write — always 1 byte
- `*(p + i) = val` with `p: byte` — `strb` on aarch64
- `*(p + i) = val` with `p: half` — `strh` on aarch64
- `auto f: float; f = 3.14;` — materializes in `d0` (FP reg)

Without annotations, everything defaults to word (8 bytes). Hints
are opt-in performance tuning, never auto-inferred — an un-hinted
`auto` NEVER becomes a sub-word type even if the assigned value
fits in a byte.

### §4.4 — Type propagation

The compiler tracks types through assignments + function calls in
one pass (see Cap 21). Forward-declared variables inherit the
hint from their first use:

```c
auto x;             // untyped (word)
x = pixel & 0xFF;   // result fits in byte, but x stays word
// To make x a byte, annotate:
auto x: byte;
x = pixel & 0xFF;   // compiler emits byte-sized arithmetic
```

**Sliced struct access** works via typed locals — see Cap 10 for
the critical rule that `*(ptr + 8)` does NOT work for sliced
struct fields (must use `auto n: Node; n.a` pattern).

### §4.5 — What this chapter does NOT cover

- **Type inference engine internals** — `src/bpp_types.bsm`, covered
  in Cap 21 as part of parser/analysis pipeline.
- **Struct / enum declaration syntax** — Caps 10 / 11.
- **Runtime type representation** — there isn't one. All values are
  just 64-bit words; hints live only in the compile-time symbol
  table.

---

## Cap 5 — Functions and Effects

*Depends on: Cap 3, Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §3 + §12*
*Status: COMPLETE — 2026-04-23*

Functions are the unit of scheduling in B++. Every function has
a signature, optional effect annotation, and a body. The effect
annotation is the lever that drives smart dispatch (Cap 22) and
the maestro concurrency pattern (Cap 24).

### §5.1 — Declaration syntax

```c
// Simplest form — returns a word:
add(a, b) { return a + b; }

// With void return — side effect only:
void print_greeting() {
    put("Hello\n");
}

// With typed parameters:
physics_tick(world, dt: float) { ... }

// With effect annotation (see §5.4):
pure_hash(s)@base { ... }
```

Functions return a word by default. The `void` keyword makes the
return type explicit as "no value" — typically used for
side-effect functions.

**Implicit return:** a function body that falls through the end
without `return` yields 0 for non-void functions. Cap 48 documents
the parser-level implicit-return-zero emission.

### §5.2 — Forward declarations and stubs

If function A needs to call function B that's defined later in the
file (or lives in another module yet to be parsed), declare a stub:

```c
stub B(x);   // forward declaration — B's body lives elsewhere

A() {
    B(42);   // legal because B is stubbed
}
```

`stub` without a body resolves at link time. A stub for a function
that has an annotation must repeat the annotation:

```c
stub compute(x)@base;   // stub must carry the effect signature
```

### §5.3 — Visibility

Top-level functions are visible across all modules that import the
defining file. Mark a function `static` to scope it to the current
module:

```c
static _private_helper(x) { return x * 2; }
public_api(x) { return _private_helper(x); }
```

Convention: prefix private helpers with an underscore. The compiler
does not enforce this, but every stdlib module follows it.

### §5.4 — Effect annotations (the five phases)

Functions carry one of five **phase annotations** that describe
what kind of work they do. The annotation is an `@` sigil glued to
the phase word with no space, placed between the parameter list
and the function body:

```c
@solo    // runs on main thread, drives presentation side effects
@base    // pure computation, parallelizable on worker threads
@io      // blocking I/O — stdin/stdout, files, sockets, syscalls
@gpu     // touches GPU resources (palette, texture, draw calls)
@time    // time-bounded path — audio callbacks, no malloc, no IO
```

The effects form a lattice — a function inherits the WORST effect
of any function it calls. A `@base` function that calls a `@io`
function is itself `@io`. The compiler tracks this automatically
and emits errors (W026) if annotations don't match the actual
call graph.

```
@time            (most strict — no malloc, no IO, no GPU)
   ↓
@io / @gpu       (sibling categories — can call base or own kind)
   ↓
@base            (pure, callable by everyone)
   ↓
@solo            (catch-all — most permissive, can call anything)
```

**Phase `@base` = the parallelizable phase.** Anything marked
`@base` can run on a worker thread during smart dispatch. The
compiler rejects side-effect-causing operations inside a `@base`
function (no writes to shared globals, no I/O, no allocation) —
the annotation is a contract the compiler enforces (W013).

**Phase `@time` = the realtime phase.** Audio callbacks, frame
deadlines, anything where a heap allocation would be a glitch.
The compiler forbids HEAP-tagged builtins (malloc, arr_push,
buf_byte, buf_word) inside a `@time` function.

```c
// Pure function — parallelizable:
static _dist(ax, ay, bx, by)@base {
    auto dx, dy;
    dx = bx - ax; dy = by - ay;
    return sqrt(dx * dx + dy * dy);
}

// Game loop — drives presentation, stays on main thread:
void tick(w, dt)@solo {
    update_positions(w, dt);    // inherits @solo through call
    render_sprites(w);
}

// Audio callback — no malloc, no IO:
void mixer_callback(buf, n)@time {
    auto i;
    for (i = 0; i < n; i++) { poke(buf + i, _next_sample()); }
}
```

`heap` is an internal compiler effect (PHASE_HEAP) the lattice
uses to track allocation; user code does not write `@heap`.
Allocation-free is implied by `@base` and `@time`; `@io` and
`@gpu` permit allocation because scaffolding (file readers,
texture uploaders) is allocation-heavy by nature.

### §5.5 — Function pointers

Functions have addresses. Take one with `fn_ptr(name)` and call
through it with `call(fp, args...)`:

```c
auto cb;
cb = fn_ptr(my_handler);
call(cb, 42, "hello");   // invokes my_handler(42, "hello")
```

Function pointers are 64-bit words stored in regular variables.
Pass them through arrays (event handlers, scene registries — see
Cap 34 `scene_register`), struct fields (ChipPrimitives — Cap 23),
or returned from builder functions.

### §5.6 — The Maestro pattern (introduction)

For games that want structured concurrency (main thread does I/O
+ render, worker threads do pure compute), `bpp_maestro.bsm`
provides a callback registration API:

```c
main() {
    maestro_register_init(fn_ptr(my_init));
    maestro_register_solo(fn_ptr(my_solo));      // every frame, main
    maestro_register_base(fn_ptr(my_physics));   // every frame, worker
    maestro_register_render(fn_ptr(my_render));
    maestro_run();
    return 0;
}
```

Full details in Cap 24. Effect annotations are the mechanism that
lets maestro dispatch to the right thread.

### §5.7 — What this chapter does NOT cover

- **Struct / enum parameters** — Caps 10 / 11.
- **Maestro scheduling internals** — Cap 24.
- **Type system interactions** (hint propagation through calls) —
  Cap 21.

---

# Part II — Arsenal

The stdlib. Every function listed in Cap 2's prelude table, expanded here with signature, semantics, and one example each.

---

## Cap 6 — Strings (bpp_str + strbuf_)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §4 + new content for §6.0 (three shapes rationale)*
*Status: COMPLETE — 2026-04-23*

### §6.0 — Three shapes of string in B++ (and why)

Before any API, understand the vocabulary: B++ represents strings in **three different shapes**, each for a different job. They are not interchangeable, and the choice of which to use is the first question every string-handling function answers.

#### The three shapes at a glance

| Shape | What it is | Where memory lives | API module | Who uses it |
|-------|-----------|-------------------|-----------|-------------|
| **Static C-string** | byte array terminated by `\0` | allocated by caller (literal, stack, or heap) | `bpp_str.str_*` | Every user program — the everyday string |
| **Dynamic builder** | `{length, capacity}` header + byte array that grows | heap, managed by builder, realloc on grow | `bpp_str.strbuf_*` | Programs that build strings from pieces |
| **Packed ref (slice)** | `(offset, length)` packed into 64 bits | no memory of its own — points INTO another buffer | `bpp_internal.buf_eq` / `packed_eq` (compiler-only) | The compiler's parser / codegen |

A physical analogy: a library.
- **Static C-string** = a book you bought. Has a cover (the NUL terminator), lives on your shelf, is yours.
- **Dynamic builder** = an empty notebook you fill in. Starts small, grows as you write. You throw it away when done.
- **Packed ref** = a bookmark saying "page 47, lines 3-15 of Dom Casmurro." You do not own the book; you only know where to find the passage.

#### Why three shapes

Three shapes exist because three use cases have genuinely different optimal representations:

1. **User programs handle existing strings.** You get a string (argv parameter, file content, config value) and you want to read it. The NUL terminator convention from C is the right shape — caller already has the memory, function just walks bytes until zero. `bpp_str` operates here.

2. **Programs build strings from pieces.** Logging, JSON serialization, formatted messages. You do not know the final size in advance; the string grows as you append. A builder with header `{len, cap}` is the right shape — it manages its own memory, reallocates on grow, and gives you amortized O(1) append. `strbuf_*` operates here.

3. **The compiler processes 10 MB of source and produces 100 000 tokens.** If every token allocated a C-string copy, the parser would burn 100 000 mallocs per compilation. A packed ref `(offset, length)` into the source buffer costs zero allocation — the token IS a pointer-and-length pair, not a string copy. `bpp_internal.buf_eq`, `packed_eq` operate here. User programs never see this shape.

#### This is not novelty

The packed ref concept is standard in modern systems languages:

| Language | Name |
|----------|------|
| C++ (C++17+) | `std::string_view` |
| Rust | `&str` / `&[u8]` |
| Go | slice (`[]byte`) |
| Zig | slice (`[]const u8`) |
| Swift | `Substring` / `StringProtocol` |
| **B++** | "packed ref" (internal name) |

B++'s only particularity is *packing* the (offset, length) into a single 64-bit word (32 + 32 bits) instead of two separate words — a minor storage win that limits strings to 4 GB (irrelevant for source code). The concept itself is identical.

#### A builder is also a C-string

Because a dynamic builder stores its bytes contiguously and terminates them with `\0` after the last byte, you can pass a builder to any `str_*` function for reading:

```c
auto msg;
msg = strbuf_new();
msg = strbuf_cat(msg, "hello");
put(str_len(msg));             // → 5, works
if (str_eq(msg, "hello")) {}   // → 1, works
strbuf_free(msg);
msg = 0;
```

What does NOT work: writing into a builder via `str_cpy` or `str_cat_raw`. Those functions ignore the builder's header and corrupt the `{len, cap}` bookkeeping. Always use `strbuf_cat` / `strbuf_ch` / `strbuf_num` to extend a builder.

#### What a function actually returns

An orthogonal clarification many programmers need. **Every B++ function returns ONE WORD (64 bits).** Not "a string", not "an array", not "a struct" — a word. What that word *means* is a contract between function and caller, documented in comments:

```c
// Returns the byte count — pure scalar, no pointer
str_len(s) { ... }              // → 5

// Returns a pointer to a freshly malloc'd NUL-terminated byte sequence.
// Caller is responsible for free(). This is a C-string shape.
str_dup(s) { ... }              // → 0x600000...

// Returns a pointer past the header of a builder. Caller uses strbuf_* only.
strbuf_new() { ... }            // → raw + 16

// Returns a packed ref (offset, length encoded into one word).
// Caller uses unpack_s / unpack_l to read. Compiler-internal only.
make_packed_tok(s, l) { ... }   // → 0x0000000A_00000014
```

A "string" is not a language-level type — it is a **layout convention**. The caller knows which API module to use based on which function produced the pointer. This is why the prelude lists the modules (Cap 2) separately from the shapes they manipulate (Cap 6, Cap 7, Cap 10, ...) — modules operate on shapes, not types.

The same rule applies to arrays (Cap 7): a pointer returned by `arr_new()` is shaped as `{len, cap}` + elements, and `arr_*` functions are the only correct way to read or modify it.

#### When to use which shape — the decision table

| You are doing | Use |
|---------------|-----|
| Checking a file extension or config key | `str_ends`, `str_starts`, `str_eq` |
| Measuring a string you received | `str_len` |
| Copying a string to a stable location | `str_dup` (allocates heap) |
| Writing into a fixed-size buffer you own | `str_cpy`, `str_cat_raw`, `str_from_int` |
| Building a message from pieces, size unknown | `strbuf_new` + `strbuf_cat` + `strbuf_num` + `strbuf_free` |
| Accumulating a log or serializing JSON | `strbuf_*` |
| Parser-internal work on source-buffer slices | packed refs (`buf_eq`, `packed_eq`) — compiler only |

One-sentence rule: **`str_*` reads or writes memory you already own. `strbuf_*` builds memory that grows. Packed refs view memory someone else owns.** Use the one whose story matches your job.

### §6.1 — `str_*` — Static C-string operations

Functions in `src/bpp_str.bsm` that operate on null-terminated C-strings you already own. No allocation: they read or write memory the caller provides.

| Function | Description |
|----------|-------------|
| `str_len(s)` | Return the number of bytes before the null terminator. |
| `str_eq(a, b)` | Return 1 if the two null-terminated strings are byte-for-byte equal, 0 otherwise. |
| `str_starts(s, prefix)` | Return 1 if `s` begins with `prefix`. |
| `str_ends(s, suffix)` | Return 1 if `s` ends with `suffix`. |
| `str_dup(s)` | Allocate a copy of `s` on the heap. The caller must `free` the result. |
| `str_cpy(dst, src)` | Copy `src` into `dst` (no bounds check — caller ensures room). |
| `str_cat(dst, src)` | Append `src` to the null-terminated content of `dst`. |
| `str_from_int(buf, n)` | Write the decimal representation of `n` into `buf`. `buf` must be at least 22 bytes for any 64-bit integer. |

Quick examples:

```c
// Testing extension
if (str_ends(path, ".bpp")) { put("B++ source\n"); }

// Safe copy into fixed buffer
auto name_buf;
name_buf = malloc(64);
str_cpy(name_buf, argv_get(1));

// Length
auto n;
n = str_len("hello");   // 5
```

### §6.2 — `strbuf_*` — Dynamic string builder

The `strbuf_*` family in `src/bpp_str.bsm` builds strings from pieces when you do not know the final length up front. The builder allocates a heap-backed buffer and doubles it as needed.

| Function | Description |
|----------|-------------|
| `strbuf_new()` | Allocate a fresh builder. Returns an opaque pointer. Must be freed with `strbuf_free`. |
| `strbuf_cat(b, s)` | Append the null-terminated string `s` to the builder. |
| `strbuf_ch(b, c)` | Append a single byte `c` to the builder. |
| `strbuf_num(b, n)` | Append the decimal representation of integer `n`. |
| `strbuf_free(b)` | Free the builder. Returns 0 — use the builder pointer directly before calling free. |

Lifecycle example — build a log line and print it:

```c
auto b;
b = strbuf_new();
strbuf_cat(b, "wave ");
strbuf_num(b, wave_num);
strbuf_cat(b, " closed: ");
strbuf_num(b, tests_passed);
strbuf_cat(b, " passed");
put(b);             // use b directly — it is already a null-terminated string
strbuf_free(b);     // returns 0, not the string
b = 0;
```

The builder pointer `b` is a valid null-terminated string at any point. Call `strbuf_free` when done; it frees the underlying allocation. If you need to keep the string after freeing the builder, copy it first with `str_dup(b)`.

### §6.3 — `str_peek` is a compiler builtin

`str_peek(s, i)` reads byte `i` of the string pointer `s`. It is NOT a function in `bpp_str.bsm` — it lives in `cg_builtin_dispatch` (`src/bpp_codegen.bsm`) and is intercepted before any function call is emitted.

#### Why a separate builtin and not just `peek(s + i)`?

`peek` is a raw memory primitive. When the compiler sees `peek(s + i)`, it pattern-matches the `+` expression at the call site and emits an indexed load:

```
peek(s + i)  →  LDRB w0, [x0, x1]   // ARM64 — only when + is visible at the call site
```

But if the address is pre-computed, the pattern is gone:

```
auto addr;
addr = s + i;
peek(addr)   →  LDRB w0, [x0]        // simple load — + already consumed
```

`str_peek` avoids this by accepting base and index as **separate parameters**. The compiler always has both values, so the indexed load is unconditional regardless of how `s` and `i` were derived:

```
str_peek(s, i)  →  LDRB w0, [x0, x1]   // ARM64 — always, without exception
```

On x86_64 both forms emit the same two instructions (`ADD rax, rcx` + `MOVZX`), so the distinction only matters on ARM64. Use `str_peek` when you need a guaranteed indexed load; use `peek(s + i)` when the `+` is always visible at the call site and the opportunistic optimization is enough.

Both are pure (`PHASE_BASE`) — the smart-dispatch system treats them identically for `job_parallel_for` eligibility.

```
// Walk a string one byte at a time
auto i, c;
i = 0;
while (1) {
    c = str_peek(s, i);
    if (c == 0) { break; }
    putchar(c);
    i = i + 1;
}
```

`str_peek` is always in scope — no import needed.

### §6.4 — Packed refs for compiler writers

Users never see these. They are used inside the B++ parser and lexer, which hold large slices of the source buffer without copying or null-terminating.

All three reside in `src/bpp_internal.bsm`. Import it only when hacking on the compiler itself.

| Function | Description |
|----------|-------------|
| `buf_eq(a, b, len)` | Return 1 if the `len` bytes at address `a` equal the `len` bytes at address `b`. Used for comparing source-buffer slices directly. |
| `packed_eq(a, b)` | Unpack both `a` and `b` as packed refs, compare the byte ranges they describe. Returns 1 if they refer to equal content. |
| `unpack_s(packed)` | Extract the start (byte offset) from a packed ref word. |
| `unpack_l(packed)` | Extract the length from a packed ref word. |

A "packed ref" is a single 64-bit word encoding `(offset << 32) | length`. The parser creates them with `make_packed_tok(s, l)`. They let the tokenizer identify thousands of tokens with zero heap allocation.

User programs import `bpp_str` for string work. The packed-ref API exists purely for the tokenizer's inner loop. Cap 21 describes where in the pipeline this API is consumed.

### §6.5 — Common patterns

Five idioms that cover the majority of real string handling in B++ programs:

**1. File-path extension test**

```c
if (str_ends(path, ".bpp") || str_ends(path, ".bsm")) {
    // handle source file
}
```

**2. Score interpolation (no format strings)**

```c
// Print "score: 1234" — put dispatches by type
put("score: ");
put(score);
putchar('\n');

// Or build it for display in a UI widget
auto b;
b = strbuf_new();
strbuf_cat(b, "score: ");
strbuf_num(b, score);
draw_text(b, hud_x, hud_y, 1, WHITE);  // use b directly
strbuf_free(b);
b = 0;
```

**3. Log accumulation**

```c
auto log;
log = strbuf_new();
strbuf_cat(log, "[init] ");
strbuf_num(log, step);
strbuf_cat(log, " done\n");
// ... more strbuf_cat calls ...
file_write_all("run.log", log, strbuf_len(log));  // use log directly
strbuf_free(log);
log = 0;
```

**4. Parse-a-line (tokenize by delimiter)**

```c
// Split a colon-separated key:value line
auto colon_pos, key_len, val_start;
colon_pos = 0;
while (str_peek(line, colon_pos) != ':' && str_peek(line, colon_pos) != 0) {
    colon_pos = colon_pos + 1;
}
key_len = colon_pos;
val_start = line + colon_pos + 1;
// key = line..line+key_len, value starts at val_start
```

**5. Safe copy into a fixed buffer**

```c
auto buf;
buf = malloc(256);
str_cpy(buf, source);      // source must fit in 255 bytes + null
// Use buf here, free when done
free(buf);
```

When the maximum length is unknown, use `str_dup` instead:

```c
auto copy;
copy = str_dup(source);    // allocates exactly str_len(source) + 1
// Use copy, then:
free(copy);

---

## Cap 7 — Arrays (bpp_array + bpp_buf)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §5*
*Status: COMPLETE — 2026-04-26*

B++ has two distinct array shapes:

**Dynamic arrays** (`arr_new` — TY_ARR): 16-byte shadow header holds length and capacity. Use `arr_push`/`arr_pop`/`arr_len`. Pointer returned points at element 0; header lives at `ptr - 16`. Free with `arr_free`.

```
auto a: array;
a = arr_new();
a = arr_push(a, 42);     // always reassign — may realloc
a = arr_push(a, 99);
if (arr_len(a) != 2) { ... }
arr_get(a, 0);            // → 42
a = arr_free(a);          // returns 0
```

**Raw buffers** (`buf_word`/`buf_byte` — TY_PTR): no header. Size is known at allocation. Access via `buf[i]` (word) or `poke`/`peek` (byte). Free with `free(buf)` directly, not `arr_free`.

```
auto w;
w = buf_word(256);    // 256 × 8 bytes, no header
w[0] = 42;            // word-indexed
free(w);

auto b;
b = buf_byte(1024);   // 1024 bytes, no header
poke(b + i, val);     // byte-indexed
free(b);
```

**Which to use:**
- Size unknown at allocation, grows dynamically → `arr_new`
- Size known at allocation (Huffman tables, pixel buffers, audio PCM, vertex buffers, game entity pools) → `buf_word` or `buf_byte`
- In practice: compiler internals use `arr_new`; every stb module and game uses `buf_word`/`buf_byte`

---

## Cap 8 — Hash Tables (bpp_hash)

*Depends on: Cap 7*
*Source: legacy_docs/the_b++_programming_language.md §5*
*Status: COMPLETE — 2026-04-27*

Two hash table families: word→word and string→word.

**Word hash** — integer keys mapped to word values:

```c
auto h;
h = hash_new(64);          // initial capacity (grows automatically)
hash_set(h, key, value);
hash_get(h, key);          // → value, or 0 if absent
hash_has(h, key);          // → 1 or 0
hash_del(h, key);
hash_clear(h);
hash_free(h);
```

**String hash** — byte-string keys (pointer + length) mapped to word values:

```c
auto h;
h = hash_str_new(64);
hash_str_set(h, ptr, len, value);
hash_str_get(h, ptr, len);   // → value, or 0 if absent
hash_str_clear(h);
hash_str_free(h);
```

**Iteration** — walk all live entries:

```c
auto i, cap, k, v;
cap = hash_cap(h);
for (i = 0; i < cap; i = i + 1) {
    if (hash_slot_live(h, i)) {
        k = hash_key_at(h, i);
        v = hash_val_at(h, i);
    }
}
```

The hash table is open-addressing with linear probing. Capacity doubles automatically when the load exceeds 75%. The zero value (key=0 or value=0) is the tombstone — do not use 0 as a meaningful key or value in a word hash. Use 1-based indexing or offset by 1 if you need to store zero.

For `hash_str_*`, the string bytes are hashed by content — two different pointers to the same bytes resolve to the same slot. Keys are NOT copied; the pointer must remain valid for the lifetime of the hash.

---

## Cap 9 — Buffers (bpp_buf)

*Depends on: —*
*Source: bpp_buf.bsm current state*
*Status: COMPLETE — 2026-04-27*

`bpp_buf.bsm` provides raw memory allocation and typed byte-level I/O. All functions operate on TY_PTR buffers — no header, no push/pop semantics.

**Allocation:**

```c
auto b, w;
b = buf_byte(1024);    // allocate 1024 bytes, access via poke/peek
w = buf_word(256);     // allocate 256 × 8-byte words, access via w[i]
free(b);               // free directly — no arr_free
free(w);
```

**Bulk operations:**

```c
buf_fill(buf, 0, n);          // set n bytes to 0 (memset equivalent)
buf_copy(dst, src, n);        // copy n bytes src→dst (memcpy, no overlap)
```

**Typed little-endian reads (binary file parsing, network, audio):**

```c
read_u8(buf, off)    // 1 byte unsigned
read_u16(buf, off)   // 2 bytes LE unsigned
read_u32(buf, off)   // 4 bytes LE unsigned
read_u64(buf, off)   // 8 bytes LE unsigned
```

**Typed little-endian writes:**

```c
write_u8(buf, off, val)
write_u16(buf, off, val)
write_u32(buf, off, val)
write_u64(buf, off, val)
```

**Big-endian variants (PNG, network byte order):**

```c
read_u16be(buf, off)   write_u16be(buf, off, val)
read_u32be(buf, off)   write_u32be(buf, off, val)
read_u64be(buf, off)   write_u64be(buf, off, val)
```

For single-byte access at any offset, use the inline builtins `peek(ptr)` / `poke(ptr, val)` directly — they compile to a single load/store instruction with no function call overhead.

---

## Cap 10 — Structs (bpp_struct)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §4.5 + new module bpp_struct.bsm*
*Status: PENDING — module bpp_struct.bsm does not exist yet; Fase 1.2 creates it*

---

## Cap 11 — Enums (bpp_enum)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §4.6 + new module bpp_enum.bsm*
*Status: PENDING — module bpp_enum.bsm does not exist yet; Fase 1.3 creates it*

---

## Cap 12 — I/O (bpp_io)

*Depends on: —*
*Source: bpp_io.bsm current state*
*Status: COMPLETE — 2026-04-27*

### Output

`put(x)` is the primary output function. The compiler rewrites it at compile time based on the inferred type of `x` — same philosophy as `auto x = 3.14`:

```c
put("hello\n")    // TY_PTR   → putstr("hello\n")
put(42)           // TY_WORD  → putnum(42)
put(3.14)         // TY_FLOAT → putfloat(3.14) — 4 decimal places
```

For stderr: `put_err(x)` with the same dispatch.

Full function family:

| Function | Effect |
|----------|--------|
| `put(x)` | stdout, type-dispatched |
| `put_err(x)` | stderr, type-dispatched |
| `putchar(c)` | one byte to stdout |
| `putchar_err(c)` | one byte to stderr |
| `putstr(s)` | null-terminated string to stdout, no newline |
| `putstr_err(s)` | null-terminated string to stderr, no newline |
| `putnum(n)` | decimal integer to stdout |
| `putnum_err(n)` | decimal integer to stderr |
| `putfloat(f)` | float to stdout, 4 decimal places |
| `putfloat_err(f)` | float to stderr, 4 decimal places |
| `putline()` | newline to stdout |
| `putline_err()` | newline to stderr |
| `putmsg(s)` | `putstr(s)` + newline to stdout |
| `puthex_err(n)` | pointer in `0x<hex>` form to stderr (debug) |

`putchar` is the primitive — every other helper builds on it. For debugging pointer values or suspected string corruption, `puthex_err(ptr)` prints the raw address.

### Input

```c
getchar()       // read one byte from stdin, returns 0-255 or -1 on EOF
getenv(name)    // look up environment variable by name, returns pointer or 0
```

### Pattern: composing output

```c
// Single value — use put
put(score);
putchar('\n');

// Multiple values on one line — chain put calls
put("wave "); put(wave); put(" passed\n");

// Multi-part string for storage/UI — use strbuf_*
auto b;
b = strbuf_new();
strbuf_cat(b, "score: ");
strbuf_num(b, score);
// use b directly — it is a valid null-terminated string
draw_text(b, x, y, 1, WHITE);
strbuf_free(b);
b = 0;
```

---

## Cap 13 — Runtime ABI (bpp_runtime)

*Depends on: Cap 9*
*Source: `src/bpp_runtime.bsm` (auto-injected)*

`bpp_runtime.bsm` is the contract between every B++ program and
the OS-specific core (`_core_macos.bsm` / `_core_linux.bsm`).
Auto-injected: no import needed. Three user-facing capabilities
ship in the module today, all built on top of the embedded
`__minisym` section every native binary carries.

### §13.1 — `panic(msg)` — abort with a stack trace

`panic(msg)` writes `panic: <msg>\n` to stderr, walks the FP
chain up to 32 frames printing `at <name>` per resolved frame,
and exits with code 134 (128 + SIGABRT). Use it when an
internal invariant has broken and the user has no recovery
path:

```bpp
if (cap < 0) {
    panic("buf_alloc: negative capacity");
}
```

Output:

```
panic: buf_alloc: negative capacity
  at buf_alloc
  at strbuf_grow
  at strbuf_cat
  at main
```

The classifier sees `panic` as `@io` (it writes to stderr), so
any `@time` audio callback that reaches `panic` lights up W026
at compile time. This catches realtime-IO violations before
production hits an audible glitch.

C-path fallback: `bpp --c` rewrites the call to
`(fprintf(stderr, "panic: %s\n", msg), abort(), 0LL)`. No stack
trace there (no minisym, no portable FP walker), but the exit
code stays consistent.

### §13.2 — `caller_pc(n)` / `caller_name(pc)` — introspection

`caller_pc(n)` returns the PC at depth `n` of the FP chain
counted from the calling function. `caller_pc(0)` is the
current function's own body PC; `caller_pc(1)` is the immediate
caller; and so on. Walking off the end returns 0 (the chip
primitive guards every iteration with a null-FP check).

`caller_name(pc)` resolves a PC to a packed name reference via
`_runtime_resolve_pc`. The result is `(strtab_off << 32) |
name_len`; unpack with the helpers from §13.3.

```bpp
// Where am I right now?
log_who(level) {
    auto packed, addr, len;
    packed = caller_name(caller_pc(1));   // 1 = log_who's caller
    if (packed != 0) {
        addr = _runtime_name_addr(packed);
        len  = _runtime_name_len(packed);
        sys_write(1, addr, len);
        putchar(' ');
    }
    putnum(level);
    putchar('\n');
}
```

Useful for ad-hoc tracing without breakpoints. For systematic
profiling, use the sampling profiler in §13.4 instead.

### §13.3 — Packed name unpack helpers

The minisym lookup returns names as packed `(strtab_offset,
length)` pairs so a single 8-byte slot carries both fields.
`_runtime_name_addr(packed)` resolves the offset to a real
byte pointer; `_runtime_name_len(packed)` returns the length.
Both return 0 when `packed` is 0 (the "PC not in any minisym
entry" sentinel).

```bpp
auto packed, addr, len;
packed = caller_name(some_pc);
if (packed != 0) {
    addr = _runtime_name_addr(packed);
    len  = _runtime_name_len(packed);
    sys_write(1, addr, len);
}
```

### §13.4 — `profile_start` / `profile_stop` / `profile_dump`

A sampling profiler. `profile_start(rate_hz, depth)` arms an
ITIMER_PROF at `rate_hz` (target — actual rate is bounded by
the dispatch rate of the workload) and a per-thread cooperative
hook in `bpp_job` and `bpp_maestro`. `depth` caps the captured
stack frames per sample — 8 is a good default; up to 32 max.

`profile_stop()` disarms the timer and freezes the rings for
the aggregator.

`profile_dump(out_buf, cap)` walks the rings, resolves each
sample's innermost PC via `_runtime_resolve_pc`, runs the
synth-name re-attribution, tallies, partial-sorts the top
`cap` entries by count descending, and writes them to
`out_buf`. Each entry is 16 bytes: 8 = packed name reference,
8 = sample count. Returns the number of entries written.

```bpp
import "bpp_job.bsm";

work_fn(idx)  { /* per-job work */ }

main() {
    auto buf, n, i, packed, addr, len;
    job_init(4);
    profile_start(1000, 8);

    auto k;
    for (k = 0; k < 1000; k++) {
        job_submit(fn_ptr(work_fn), k);
    }
    job_wait_all();

    profile_stop();

    buf = buf_word(16 * 2);
    n = profile_dump(buf, 16);
    for (i = 0; i < n; i++) {
        packed = *(buf + i * 16);
        putnum(*(buf + i * 16 + 8)); putchar(' ');
        addr = _runtime_name_addr(packed);
        len  = _runtime_name_len(packed);
        sys_write(1, addr, len);
        putchar('\n');
    }

    job_shutdown();
    return 0;
}
```

Sample output for a CPU-heavy worker pool:

```
272 work_fn
202 _job_worker_main
1 job_wait_all
```

The SIGPROF supplement (macOS) covers worker threads via
`pthread_kill` fan-out: when the timer fires on main, the
handler signals every worker so each captures its own
mcontext on the same tick. Without that, samples concentrate
on whichever thread the kernel schedules and miss the others
entirely.

### §13.5 — Recursive dispatch guard

`job_parallel_for` and `job_parallel_reduce` detect when they
are called from a worker thread (via `_thread_id()` matched
against `_job_main_tid` and the worker table) and fall back to
a serial run. Without the guard, an additive reduction inside
a function that runs on workers would deadlock the pool — the
worker submitting chunks back to the queue is the same worker
the queue needs to drain. Sprint 2b's auto-promote firing on
nested loops is the most common way to land on this path; the
guard makes it transparent.

### §13.6 — What this chapter does NOT cover

- Worker-thread pthread_kill fan-out for SIGPROF on Linux —
  blocked on ELF dynlink + thread parity. Cooperative path
  covers single-threaded Linux today.
- Folded-stacks export (`flamegraph.pl` format) — the
  aggregator already produces the data, but the export sugar
  is YAGNI until a consumer asks.
- `caller(n)` short-form — Phase 6.5 sugar over
  `caller_pc(n) + caller_name(...)`. Lands when a real user
  consumer wants it.

---

# Part III — Writing Pro B++

The rules, patterns, and anti-patterns that separate a first program from a shippable one.

---

## Cap 14 — Tonify (the style rules)

*Depends on: —*
*Source: see `docs/tonify_checklist.md`*
*Status: REFERENCE — content lives in tonify_checklist.md*

The tonification rules — storage classes, visibility, return types,
phase annotations, control flow, slice types, comments, struct
access, operator preferences — live in **`docs/tonify_checklist.md`**.

That file is canonical: every new function in the codebase is
written against its 20 rules from the first keystroke, not
post-hoc cleanup. Read it once before contributing.

A two-line preview of the rules:

- **Rule 1** — storage classes: `extrn` (write-once-after-init),
  `global` (worker-shared), `const` (compile-time), `static` (private).
- **Rule 4** — `@phase` annotations (`@base`, `@time`, `@io`, `@gpu`,
  `@solo`) on functions where intent is obvious; the classifier
  fills the rest.
- **Rule 25** — `@profile("name") { ... }` block annotation that
  pairs each instrumented region with a runtime aggregate visible in
  the profile HUD. Shipped 2026-05-07 (Phase 6.3 of the GPU pipeline
  roadmap).

Twenty-five rules total, ~1100 lines including examples and pitfalls.

---

## Cap 15 — Disciplina QG (general vs battalion)

*Depends on: Cap 2*
*Source: new*
*Status: COMPLETE — 2026-04-21*

### The single sentence

> **Control flow lives in the frontend / spine / stdlib. Specialization lives only in local-language translation. The general (HQ) issues one canonical order; the battalion (backend / tool / consumer) translates it into local execution.**

This is the constitution of B++. Every architectural decision consults it. Every commit that violates it owes either a justification in the commit message or a patch to bring the violation in line.

### Why "general" and "battalion"

B++ is a compiler. It takes high-level programs and produces machine-specific binaries. A compiler with multiple backends (x86_64, aarch64, C emitter, eventually RISC-V) has two failure modes:

1. **Each backend reinvents the logic** (bad). Drift. Bugs in one backend stay there. Three subtly different programs emerge from the same source.
2. **The frontend dictates the logic** (good). Backends only translate the final instruction. Fix a bug once, every backend inherits the fix.

The second mode is what B++ aspires to. The first is what it often falls into. This chapter names the discipline that keeps (2) from decaying into (1).

### The four tiers of HQ

The "general / battalion" mental model applies at FOUR layers of B++, each with its own canonical location. Three are user-facing; one is compiler-internal.

| Tier | General (HQ) | Battalion | Audience | Concrete example |
|------|--------------|-----------|----------|------------------|
| **Data primitives** | `src/bpp_*.bsm` auto-injected (strings, arrays, hash, buf) | Every program, tool, game, stb module | User | `arr_push` canonical in `bpp_array.bsm` — consumers call it, do not reimplement |
| **Runtime functions** | `src/bpp_runtime.bsm` declares the ABI contract; `src/backend/os/<os>/_core_<os>.bsm` implements it | Consumers call `malloc(n)` as a normal function | User + compiler | `malloc`, `free`, `realloc`, `memcpy`, `memset` |
| **Inline builtins (codegen dispatch)** | `src/bpp_codegen.bsm` `cg_builtin_dispatch` | `src/backend/chip/<chip>/<chip>_primitives.bsm` | User (transparently) | `peek`, `poke`, `str_peek`, `sys_write`, `argc_get` — dispatch in spine, final instruction per chip |
| **Compiler-internal libraries** | `src/bpp_internal.bsm`, `src/bpp_struct.bsm`, `src/bpp_enum.bsm` | Parser, validator, codegen, dispatch | Compiler only (not auto-injected) | `buf_eq(a, b, len)` operates on raw byte ranges; `packed_eq(a, b)` on source-buffer slices; `struct_register(name)` on the metadata table |

Four tiers, four canonical homes in `src/`. The fourth tier is where B++'s compiler-internal vocabulary lives — shapes and operations the compiler needs for its own work that user programs never see. Like the third tier (inline builtins), the fourth tier is HQ-controlled; unlike the third tier, it is not transparently available to user code.

**Why not fold the fourth tier into the first?** Because the data shapes are different. `bpp_str` operates on null-terminated C-strings (user programs hold strings that way). `bpp_internal.buf_eq` operates on raw address + length (the parser holds slices of the source buffer without allocating). Forcing one API to serve both audiences means either the user-facing API carries compiler internals (pollution) or the compiler pays allocation cost for packed refs (defeats the point). Cap 6 §6.0 walks through this with three-shapes-of-string as the canonical example.

### The three rules of the QG

**Rule 1 — Data manipulation is auto-injected and centralized.**
Strings, arrays, hash tables, buffers, structs, enums, and I/O all live in `src/bpp_*.bsm`, are auto-injected into every compilation unit, and are the ONLY canonical way to manipulate those data shapes. If a tool or game wants a string operation, it calls `bpp_str` — never reimplements. Exception: compiler internals (`bpp_internal.bsm`) may hold specialized variants (packed offset operations) that operate on shapes the user-facing API does not expose; those stay compiler-only.

**Rule 2 — Runtime functions have a contract and per-OS implementations.**
`src/bpp_runtime.bsm` declares what every OS runtime must provide (malloc, free, realloc, memcpy, memset). Each `src/backend/os/<os>/_core_<os>.bsm` implements those names. Consumers always call the name; the general (via `bpp_import.bsm`) chooses which implementation is auto-injected based on target OS. Backends do not reinvent memory management — they translate calls and let the runtime file handle the body.

**Rule 3 — Inline builtins dispatch in the spine, emit in the chip.**
Names like `peek`, `poke`, `str_peek`, `sys_write` are compiled inline (not through a function call). The dispatch logic (which builtin is this? how are its arguments arranged? which chip-level instruction closes the operation?) lives ONCE in `src/bpp_codegen.bsm` (`cg_builtin_dispatch`). Each chip backend exposes a minimal primitive IR (`emit_mem_read`, `emit_mem_write`, `emit_syscall`, `emit_shift_right`, etc.) and the spine calls those. A new chip port is ~400 lines of primitive IR translation, not 1200 lines of rebuilt dispatch logic.

### The audit

When reviewing any patch, ask:
- Does this touch `stb/`, `tools/`, or `games/` and introduce a function that manipulates strings, arrays, structs, or bytes? If yes: is it already in `bpp_*.bsm`? If yes, use that. If not, propose promoting it (need 2+ independent reimplementations to justify the promotion).
- Does this touch `src/backend/chip/<chip>/` and re-implement dispatch logic that another chip has a copy of? If yes: that logic belongs in `src/bpp_codegen.bsm` `cg_builtin_dispatch`. Move it.
- Does this add a new syscall-like builtin? If yes: the dispatch goes in the spine; each chip gets a new primitive IR function for just the final instruction.
- Does this add a runtime function? If yes: declare in `src/bpp_runtime.bsm`, implement per-OS in `src/backend/os/<os>/_core_<os>.bsm`.

If any answer is "I reimplemented the thing locally", the patch needs revision before merge.

### When divergence is allowed

Not every duplication is a violation. Legitimate reasons to keep specialized variants:

- **Performance shape**: `bpp_internal.bsm`'s `list_begin/push/end` (stack-based AST builder with arena alloc) is intentionally separate from `bpp_array.bsm`'s `arr_new/push` (heap-based growable). Different perf envelope. Document the reason in a header comment.
- **Data shape**: `bpp_internal.bsm`'s `buf_eq(a, b, len)` (raw address + length) is intentionally separate from `bpp_str.bsm`'s `str_eq(a, b)` (null-terminated C-strings). Different inputs. The compiler's internal representation does not null-terminate, so it needs a length-aware operation.
- **Emit-time vs call-time**: `malloc` (runtime function, called) and `peek` (inline builtin, emitted) are both HQ-controlled but via different mechanisms. Both are correct; they are not duplicates of each other.

Document each legitimate divergence with a comment in both modules saying "see the other; why we have both".

### The canonical example of legitimate divergence: three string shapes

B++ carries three string representations (Cap 6 §6.0): static C-strings (`bpp_str.str_*`), dynamic builders (`bpp_str.strbuf_*`), and packed refs (`bpp_internal.buf_eq` / `packed_eq`). These are **not** a discipline failure — they are three genuinely different data shapes for three genuinely different jobs:

- Static C-strings serve user programs that handle strings they already own (argv, file content, config).
- Dynamic builders serve programs that build strings from pieces without knowing final size.
- Packed refs serve the compiler's parser, which turns 10 MB of source into 100 000 tokens without 100 000 mallocs.

Each shape carries its own `_eq` / `_len` / etc. because the data they operate on is structurally different. Unifying them would cost either allocation (packed ref → C-string) or NUL terminator bookkeeping (C-string → packed ref) — taxes that defeat the purpose of having separate shapes.

**The discipline still applies within each shape**: `str_eq` is the ONE canonical C-string comparison; if something in the codebase reimplements it (like the old `cg_str_eq` did), that is a violation and gets merged. Divergence across shapes is allowed; divergence within a shape is not.

**Rule of thumb for reviewers**: when you see what looks like duplication, ask "are these operating on the same data shape?" If yes, merge. If no, ensure both carry a header comment cross-referencing the other.

### One rule, repeated

When in doubt:

- **Subtract before you add.** Is there already a primitive that does this? Use it. Only if nothing exists, propose adding — and only when you have at least two consumers who would use the new primitive.
- **Promote up, never down.** If three local helpers are doing the same thing, promote to the shared home (stdlib, spine, runtime contract) and delete the locals. Do not push canonical logic DOWN into consumers.
- **One general per tier.** Data primitives in `src/bpp_*.bsm`. Runtime contract in `src/bpp_runtime.bsm`. Inline dispatch in `src/bpp_codegen.bsm`. No fourth canonical home.

---

## Cap 16 — Idioms

*Depends on: Cap 14, Cap 15*
*Source: legacy_docs/how_to_dev_b++.md Part 4*
*Status: COMPLETE — 2026-04-27*

### stb module conventions

- File name: `stb<name>.bsm`. `.bsm` marks "B-Single-Module".
- Function prefix: every public function starts with the module's name. `path_new`, `hash_set`, `tile_collides`. Private helpers start with underscore: `_path_heap_swap`.
- Header comment: a paragraph describing what the module does, the intended use, and a code snippet showing the canonical pattern. Plan 9 / Bell Labs man pages, not marketing.
- Init function: `init_<name>()` that performs any one-time setup. Even if the body is `return 0;`, define it for symmetry.
- All comments in English, full sentences, user-manual style.

### Leaf vs coupled

A leaf module has zero `import` statements pointing at other stb files. It only depends on compiler builtins and auto-injected core modules. Leaf modules are testable in isolation.

Examples of leaf modules today: `stbcol`, `stbcolor`, `stbecs`, `stbinput`, `stbpath`, `stbpool`, `stbscene`, `stbsound`.

Couple a module only when the **purpose** demands it. `stbphys` imports `stbtile` because "platformer physics for tilemaps" is its purpose. `stbtile` imports `stbimage` because loading a tileset PNG needs a decoder. Those couplings are honest.

`stbpath` could have imported `stbtile` for a `path_load_from_tilemap` helper, but A* on a grid is not inherently a tilemap operation — it works on puzzles, AI planners, network topology, anything with cells. The bridge to a tilemap lives in the calling game as a 5-line loop, documented in `stbpath`'s header. The result: `stbpath` is a leaf, the compiler's own test suite can import it without GPU init.

**Acoplamento por propósito, não por conveniência.**

### Public structs and memory ownership

If the module exposes a struct, define it at the top of the file with `struct Name { field1, field2, field3 }`. Document each field on its own line. Callers create instances via `name_new(...)` returning a pointer. Field access uses dot notation; the compiler emits the right load/store size from the type hint.

State the memory contract in the header:

- "Returns a pointer the caller must free with `name_free`."
- "Copies the input bytes — the caller's buffer can be reused immediately."
- "Returns a pointer into a per-call scratch buffer; do not free."

Anything ending in `_new` returns owned memory and pairs with `_free`.

### Adding a new compiler module

1. Create `src/bpp_mymodule.bsm`.
2. Add explicit imports at the top for everything it uses:
   ```
   import "bpp_defs.bsm";
   import "bpp_array.bsm";
   import "bpp_internal.bsm";  // if you use buf_eq, packed_eq, etc.
   ```
3. Add `import "bpp_mymodule.bsm";` to `src/bpp.bpp`.
4. Add `init_mymodule();` to the init block in `main()`.
5. Wire the module's entry point into the pipeline.
6. Run the full bootstrap cycle.
7. Add a test in `tests/` if the module has behavior worth locking in.

### Available infrastructure

Before inventing a data structure, check whether one already exists:

| Need | Module | Functions |
|------|--------|-----------|
| Dynamic array | `bpp_array` | `arr_new`, `arr_push`, `arr_pop`, `arr_get`, `arr_set`, `arr_len`, `arr_clear`, `arr_free` |
| Hash map word→word | `bpp_hash` | `hash_new`, `hash_set`, `hash_get`, `hash_has`, `hash_del`, `hash_clear`, `hash_count`, `hash_free` |
| Hash map bytes→word | `bpp_hash` | `hash_str_new`, `hash_str_set`, `hash_str_get`, ... |
| Bump allocator (frame scope) | `bpp_arena` | `arena_new`, `arena_alloc`, `arena_reset`, `arena_free` |
| Fixed-size object pool | `stbpool` | `pool_new`, `pool_get`, `pool_put`, `pool_free` |
| Entity-component system | `stbecs` | `ecs_new`, `ecs_spawn`, `ecs_kill`, ... |
| Growable string builder | `bpp_str` | `strbuf_new`, `strbuf_cat`, `strbuf_num`, `strbuf_ch`, `strbuf_free` |
| Raw byte buffer | `bpp_buf` | `read_u8/u16/u32/u64`, `write_u8/u16/u32/u64` |
| File I/O | `bpp_file` | `file_read_all`, `file_write_all` |

The compiler itself uses several directly: `arr_*` for function/extern/global tables, `hash_str_*` for symbol-lookup hashes, `buf_eq` from `bpp_internal.bsm` for byte comparison.

### Symbol-table lookup strategies

When adding a new lookup site, pick one of three strategies depending on the access pattern:

**Eager rebuild.** Caller invokes `_rebuild_hash()` at the start of every phase that does lookups. Rebuild is O(n) once per phase. Lookups are pure O(1). Use when lookups are concentrated inside named phases with clear entry points. Failure mode: caller forgets to rebuild; lookups return wrong results silently. Example: `val_find_func` in `bpp_validate`.

**Lazy rebuild on stale.** The lookup function carries a `_hash_cnt` snapshot. Every lookup checks `if (_hash_cnt != arr_cnt) rebuild();` before querying. Use when the lookup function is the only entry point that matters and the array is mostly stable. Worst case O(n²) if inserts and lookups alternate. Example: `find_extern` in the C emitter (cold path).

**Incremental update at insertion sites.** Wherever the underlying array is pushed to, immediately call `register_in_hash(name, idx)`. Use when insertion sites are few and well-known. Failure mode: **silent and dangerous** — if you miss an insertion site, missing entries are invisible to lookups. Example: `find_struct` (parser and bo loader both call `register_struct_hash`).

When unsure, **eager is the safest**. It tolerates missed insertion sites because it rebuilds from the live state every phase. Lazy self-corrects. Only incremental can break silently.

---

## Cap 17 — Anti-patterns

*Depends on: Cap 14, Cap 15*
*Source: legacy_docs/how_to_dev_b++.md Part 5*
*Status: COMPLETE — 2026-04-27*

When tonifying existing code (or adding a rule that affects the whole codebase), process files in dependency order — leaves first, complex last. A broken leaf cascades; a broken root breaks in isolation and is easier to revert.

```
Batch 1 — Core utilities (leaf, no deps):
  src/bpp_io.bsm
  src/bpp_array.bsm
  src/bpp_buf.bsm
  src/bpp_str.bsm
  src/bpp_math.bsm
  src/bpp_file.bsm
  src/bpp_hash.bsm
  src/bpp_arena.bsm

Batch 2 — Runtime modules:
  src/bpp_beat.bsm
  src/bpp_job.bsm
  src/bpp_maestro.bsm
  src/bpp_mem.bsm

Batch 3 — Platform layers:
  src/backend/os/macos/_stb_core_macos.bsm
  src/backend/os/macos/_stb_platform_macos.bsm
  src/backend/os/macos/_stb_audio_macos.bsm
  src/backend/os/linux/_stb_core_linux.bsm
  src/backend/os/linux/_stb_platform_linux.bsm

Batch 4 — Game cartridges (stb/):
  stb/stbgame.bsm
  stb/stbrender.bsm
  stb/stbecs.bsm
  stb/stbphys.bsm
  stb/stbtile.bsm
  stb/stbsprite.bsm
  stb/stbfont.bsm
  stb/stbimage.bsm
  stb/stbinput.bsm
  stb/stbcolor.bsm
  stb/stbdraw.bsm
  stb/stbpath.bsm
  stb/stbcol.bsm
  stb/stbpool.bsm
  stb/stbui.bsm
  stb/stbaudio.bsm
  stb/stbmixer.bsm
  stb/stbsound.bsm
  stb/stbasset.bsm
  stb/stbscene.bsm

Batch 5 — Compiler internals:
  src/bpp_defs.bsm
  src/bpp_internal.bsm
  src/bpp_diag.bsm
  src/bpp_lexer.bsm
  src/bpp_parser.bsm
  src/bpp_types.bsm
  src/bpp_dispatch.bsm
  src/bpp_validate.bsm
  src/bpp_import.bsm
  src/bpp_bo.bsm
  src/bpp_bug.bsm

Batch 6 — Codegens:
  src/backend/chip/aarch64/a64_enc.bsm
  src/backend/chip/aarch64/a64_codegen.bsm
  src/backend/target/aarch64_macos/a64_macho.bsm
  src/backend/chip/x86_64/x64_enc.bsm
  src/backend/chip/x86_64/x64_codegen.bsm
  src/backend/target/x86_64_linux/x64_elf.bsm
  src/backend/c/bpp_emitter.bsm

Batch 7 — Entry points + games:
  src/bpp.bpp
  src/bug.bpp
  games/snake/snake_maestro.bpp
  games/snake/snake_particles.bsm
  games/snake/snake_gpu.bpp
  games/pathfind/pathfind.bpp
  games/platformer/platformer.bpp
  tools/mini_synth/mini_synth.bpp
```

### Recheck after each batch

Before bootstrapping, verify:

- Zero bare `auto` at file scope (should be `extrn`/`global`/`const`/`static auto`).
- Zero functions used only in their own file without `static` (grep every function, not just `_`-prefixed).
- Zero trailing `return 0;` in `void` functions.
- Zero side-effect functions missing `void`.
- Zero `import` in game files where `load` applies (batch 4 + 7 only).
- Zero override callbacks in engine modules without `stub` (batch 4 only).
- All relevant tests pass individually.
- Zero unexpected warnings during compilation.

### Bootstrap after each batch

```bash
./bpp src/bpp.bpp -o /tmp/bpp_verify
/tmp/bpp_verify src/bpp.bpp -o /tmp/bpp_verify2
shasum /tmp/bpp_verify /tmp/bpp_verify2   # must match
cp /tmp/bpp_verify2 ./bpp
./tests/run_all.sh                         # must be green
```

---

# Part IV — Building the Compiler — see `bootstrap_manual.md`

The chapters that used to live here (bootstrap procedure, canary
discipline, testing strategy, frontend + spine + battalions
architecture, runtime primitives, adding a new backend) now live in
**`docs/bootstrap_manual.md`** — the dedicated reference for
evolving the B++ compiler itself.

That manual covers:

- The Golden Rule + bootstrap cycle (`./bpp src/bpp.bpp -o /tmp/gen1`,
  the gen2/gen3 convergence check, byte-stable verification)
- macOS Gotchas (exit 137 / 138 / 139, codesign cache, sliced struct
  access)
- Source File Rules (English comments, Unicode, variables in blocks)
- Adding a New Compiler Module — explicit step-by-step
- Compilation Model (modular vs monolithic pipelines)
- Compiler Flags Reference (full table)
- Cross-Compiling and Running Games (--linux64, Docker, C emitter)
- Writing stb Modules — naming, layout, leaf vs coupled
- Available Infrastructure — stbarray, stbhash, stbarena, stbpool, ...
- Symbol-Table Lookup Strategies — eager / lazy / incremental
- Adding a New Builtin — the four-place rule (chip × OS × validate × C)
- Where to Implement a Feature — semantics in parser, instruction
  in chip, syscall in os
- Backend Layout — chip / os / target / c
- **Portability Tiers** — Tier 1 (builtin), Tier 2 (≤30-line
  attachment), Tier 3 (FFI-complex, separate file)
- Auto-Import System
- Tests Convention
- Recovering from a Broken bpp

Read `bootstrap_manual.md` before changing source under `src/`.
how_to_dev focuses on writing programs IN B++; bootstrap_manual is
the parallel volume for working ON B++.

---

# Part VI — Standard Library Tour (appendix)

Every `stb*.bsm` module in one paragraph each. **Full reference,
including API tables, decision points, and caveats, lives in
`standard_b++_lib.md`** — the deep-dive companion to this book.

The point of this appendix is to give a programmer the shape of
what's available without leaving how_to_dev. When the actual API
matters, jump to `standard_b++_lib.md` Cap N (numbering is
preserved across both books for cross-reference stability).

## Tools (Caps 26-27)

**Bang 9 IDE** — the editor + runner integrated environment.
Tabs for Project, Sprites (ModuLab), Levels (level_editor), Code,
Run. Built on top of `stbgame`/`stbdraw`/`stbui` like any other B++
program; the "embed pattern" lets each tool plug into a Bang 9 panel.

**Bundled tools** — `tools/modulab/` (pixel art editor),
`tools/level_editor/` (tilemap editor), `tools/mini_synth/`
(keyboard synth + WAV recorder), `tools/font_forge/` (TTF → bitmap
font), `tools/the_bug/` (bug debugger panel).

## Diagnostics (Cap 28)

Compiler warnings and errors catalogue. Detail in
`standard_b++_lib.md` Cap 28 + `docs/warning_error_log.md`. The latter
is the canonical registry — every diagnostic the compiler emits, where
it's defined, what triggers it, whether it shows source location.

## Audio (Caps 29-31)

**stbaudio** opens the audio device and runs a SPSC ring-buffered
realtime callback. Layer 0 of the audio stack — call `audio_open(cap)`
once, push frames via `audio_push_frame(L, R)` from your producer.

**stbmixer** is an 8-voice polyphonic mixer on top: tones (waveform
synthesis), samples (one-shot WAV playback), music (looping streaming).
Three buses (master / music / sfx) with independent gain. Used by
synthkey + every game that ships sound.

**stbsound** decodes WAV files (PCM 8/16/24/32 + IEEE float, mono /
stereo) and writes WAV from buffer. Format-agnostic — caller owns the
buffer, mixer routes via voice slots.

## Input + Window + Game loop (Caps 32-34)

**stbinput** — keyboard, mouse, text-input ring buffer. 77 keys
across 4 octaves, modifiers, action-rebinding via `action_define`.
Frame-edge "pressed" detection separate from "down" hold-state.

**stbwindow** — native dialogs (`window_save_dialog`, `window_alert`)
and basic window queries. macOS via NSPanel/NSAlert, Linux via X11.

**stbgame** — the game loop. `game_init` / `game_update(dt)` /
`game_render` / `game_quit` callbacks. Owns frame timing and the
quit flag. Built on `_stb_platform_<os>.bsm` underneath.

## Drawing + UI (Caps 35-37)

**stbdraw** — CPU framebuffer drawing primitives (rect, line,
circle, text, sprite, layer composition). Layer byte-grids for
tilemaps + level editing without re-malloc per frame.

**stbui** — widgets on top of stbdraw: button, slider, dropdown,
text_input, grid, palette swatch, scroll_view, tabs, tooltip.
Theme system with `theme_dark` / `theme_light` / `theme_retro`
presets.

**stbimage** — PNG decode (deflate + Paeth + tRNS) and PNG write
(uncompressed deflate). No FFI, no library — pure B++. Output
buffer is RGBA32.

## Sprite + Tile + Forge (Caps 38-40, 46)

**stbpal** — palettes (count + ARGB array). Seven catalogs
shipped (NES-54, GB-4, PICO-8, NKOTC-4, MCU-8, CB-32, DB-32),
plus cycling, LUT (flash + generic), lerp, .pal.json save/load.

**stbtile** — tilemap built on `Layer*` (byte grid). Load tileset
PNG, slice into N×M cells, paint into level. Used by platformer +
level_editor.

**stbforge** — animation runtime + character frames. Frame schedule,
phase-correct under big dt, loop / one-shot. testbed_play_animation
hook used by ModuLab + games that animate sprite atlases.

## Game systems (Caps 41-43)

**stbecs** — Entity-Component-System. `ecs_spawn` returns a handle;
components added per-entity via `ecs_set_*`. Custom components via
`ecs_component_new(world, sizeof(MyComponent))`.

**stbphys** — platformer physics. Collide-and-slide AABB against
Tilemap, gravity, jump, state machine (idle/run/jump/fall).

**stbpath** — A* on grid. `path_new(cols, rows)`, `path_set_cost(x, y, c)`,
`path_solve(sx, sy, gx, gy)`. Returns step list. ~80 LOC of A*, no
heap during solve (pre-allocated open + closed sets).

## Engine plumbing (Caps 43-45)

**stbpool** — fixed-size object pool. Allocate N×size up front,
`pool_get` returns a handle, `pool_put` releases. No fragmentation,
O(1) alloc/free.

**stbcol** — color math. RGBA pack/unpack, lerp, HSL conversion,
contrast helper.

**stbasset** — handle-based asset manager. `asset_load_sprite`,
`asset_load_sound`, `asset_load_music`, `asset_load_font` return 32-bit
handles (16-bit generation + 16-bit slot). HashStr dedup by path —
loading the same WAV twice returns the same handle. Generation bumps
on unload so stale handles fail safely.

## GPU (Cap 47)

**stbrender** — Metal-backed sprite + palette LUT rendering on
macOS. R8 sprite + 1×256 BGRA palette texture, indexed sprites,
GPU-side palette swap for damage flash / cycling. Vulkan/DX12 backends
land when those targets ship. Phase 1 of the Wolf3D arc added
`render_textured_strip` (column-wide UV-mapped blit with per-vertex
tint, used by raycasters for wall slices) and the marker-128 shader
fix that makes vertex tint multiplicative against the sampled texel.

## 2.5D engine + textures + profiler (Wolf3D Phase 1)

**stbtexture** — programmatic texture creation. Brick (running-bond),
stone (deterministic noise), wood (vertical planks), solid (flat
fill — replaces "do we keep a flat-shading raycast mode" with a
one-line texture choice). Each generator ships as both
`texture_X(w, h, ...)` (GPU factory) and `texture_X_to_buf(buf, w, h, ...)`
(pure-compute, headless-testable). The factory wraps the `_to_buf`
helper plus `_stb_gpu_create_texture` upload; the `_to_buf` form is
what the determinism test exercises and what CPU-only renderers
(future TUI / ANSI fallback) consume.

**stbraycast** — DDA + RayHit + per-column projection. The
cartridge stays content-blind on purpose: the caller dispatches
`hit.wall_type` to a texture handle, so a future game with
different wall types (DOOM clone, dungeon crawler) drops in
without forking the ray cast logic. Composes with stbtile for map
storage and stbrender for the per-column blit.

**stbprofile** — runtime profiler HUD overlay. Wraps the
auto-injected `bpp_runtime` profile primitives (`profile_start`,
`profile_stop`, `profile_dump`) with the UX every modern game-dev
profiler ships: hotkey toggle, REC indicator with elapsed timer,
FPS smoothed + frame-time avg / max, live top-N tally refreshed
every 500 ms (so the dump's internal allocations don't dominate
the hot list), final stderr dump on stop. Tier 2 (sparkline
normalised against `_PROFILE_SPARK_REF_US = 33000` so flat 60 FPS
shows visible jitter), Tier 3a (per-thread breakdown via
`profile_hud_cycle_thread`), Tier 3b (GPU timing readout from
`_stb_gpu_last_us`), and Tier 3c (scoped zones via the `@profile`
annotation, surfaced via `profile_zones_hud_draw`) all shipped
through 2026-05-07.

`stbphys` also gained a **Chapter FPS** (separate from the existing
PlatformerBody section): `FPSBody` struct + `fps_walk` /
`fps_turn` with per-axis collision-and-slide. The chapter promotion
followed the existing convention (one cartridge, multiple body
shapes; one chapter per shape).

## GPU pipeline arc (Phase 4–7, closed 2026-05-07)

Five cartridges layer on top of `stbrender` to give B++ a full
indie-grade GPU pipeline. None of them are required — every game
that doesn't import them pays nothing.

**stbshader** — custom vertex+fragment pipelines.
`gpu_pipeline_load(metal_path, vert, frag)` resolves the shader
through three fallbacks (cwd-relative → `/usr/local/lib/bpp/stb/
shaders/<basename>` → `path_asset(...)` walk-up), so games
running from any cwd find their shaders without code changes.
`gpu_target_create / gpu_target_bind / gpu_present_target` give
off-screen render-to-texture infrastructure; `gpu_uniform_set_*`
+ `gpu_bind_texture_*` cover the standard MTLBuffer / MTLTexture
binding surface.

**stbscene** — layered backgrounds with parallax. Three function
calls per layer for the side-scroller depth illusion:
`bg_layer_new(image, factor_x, factor_y)` registers, `bg_set_camera(cx, cy)`
updates the global camera, `bg_draw_all()` iterates registered
layers in order and dispatches one textured fullscreen quad per
visible layer. The `bg_layer.metal` shader handles the parallax
offset + normalised UV with NEAREST + repeat sampler for infinite
tiling.

**stbfx** — post-process effect chain. Owns two scratch GPU
targets internally and ping-pongs between them so any number of
effects can compose without per-frame allocation.
`fx_register(metal_path, vert, frag, uniform_size)` builds an
effect handle; `fx_chain_begin / fx_apply(handle) / fx_present()`
replaces the standard `game_render_end` for frames running
effects. Four typed factories ship out of the box:
`effect_crt(...)` (barrel + chromatic + scanlines),
`effect_scanlines(...)`, `effect_chromatic(...)`,
`effect_dither(...)` (4×4 Bayer). `effect_palette_quantize` is
the planned 5th — needs stbpal integration.

**`@profile("name") { ... }`** — Phase 6.3 compiler annotation
that lowers at parse time to a synthesized `T_BLOCK` carrying
`_prof_zone_enter("name")` / `_prof_zone_exit("name")` runtime
calls. Each completed enter/exit pair adds to a per-zone
aggregate (total_us + count) in stbprofile; render the panel via
`profile_zones_hud_draw(x, y, sz, color)`. Gates on the same
`_profile_hud_active` flag as the rest of the profile HUD so a
single P press surfaces both panels together. Tonify Rule 25
covers the surface + v1 caveats (early `return` inside a zone
leaves it open, nested zones aggregate flat, panic leaks).

**Pixel-perfect default in stbgame** — Phase 4.1.x reinterpreted
`game_init(virtual_w, virtual_h, title, fps)` as a virtual
canvas; the OS window auto-scales to ~80% of the monitor at
integer-multiple of the virtual size with BLACK letterbox.
`game_render_begin` / `game_render_end` give a one-line
orchestration replacement for the legacy `render_begin` /
`render_end` pair, taking care of the offscreen target +
NEAREST upscale + letterbox blit. CPU-only games that never
call `game_render_begin` keep their software framebuffer path
intact (the GPU isn't initialised until the first GPU helper
runs — `render_init` is lazy as of `c8c09e8`).

**`@profile` smoke**: `examples/profile_zones_smoke.bpp` — three
zones (heavy / medium / light) of measurable busy work; the
panel sorts them by total_us within ~1 second.

## Cap 48 — Compiler Flags Reference

*Depends on: Cap 18*
*Source: legacy_docs/how_to_dev_b++.md Part 8*
*Status: COMPLETE — 2026-04-27*

All flags are passed to `bpp` on the command line before the source file.

### Output targets

| Flag | Effect |
|------|--------|
| *(default)* | Native Mach-O ARM64 binary for the host machine |
| `--linux64` | Cross-compile to Linux x86_64 static ELF (no libc dependency) |
| `--c` | Emit C99 source to stdout (use `clang` or `gcc` to compile) |
| `--asm` | Emit ARM64 assembly to stdout (monolithic pipeline) |
| `--bug` | Emit `.bug` debug map alongside the binary (required for the `bug` debugger) |
| `-o <name>` | Output filename; defaults to `a.out` |

### Pipeline control

| Flag | Effect |
|------|--------|
| `--monolithic` | Force single-pass pipeline — all modules compiled at once, no caching |

Monolithic mode is used by `--asm` and `--c` automatically. For native builds, modular mode (default) is faster because unchanged modules load from `~/.bpp/cache/`.

### Inspection and debugging

| Flag | Effect |
|------|--------|
| `--show-deps` | Print the module dependency graph and exit |
| `--show-promotions` | Print `auto → extrn/global` promotion decisions |
| `--show-phases` | Print per-function phase classification (BASE/REALTIME/HEAP/IO/GPU/SOLO) |
| `--stats` | Print module/function/token counts to stderr |

These flags exit after printing — they do not produce a binary.

### Compatibility

| Flag | Effect |
|------|--------|
| `--clean-cache` | No-op; kept for backward compatibility with scripts that used the old cache |

The cache was removed entirely in 2026-04-13. This flag is accepted silently.

### Quick reference for common workflows

```bash
# Default: native ARM64 binary on macOS
bpp games/pathfind/pathfind.bpp -o /tmp/pathfind

# Cross-compile for Linux, test in Docker
bpp --linux64 games/pathfind/pathfind.bpp -o /tmp/pathfind_linux

# Debug build (required for bug debugger)
bpp --bug games/pathfind/pathfind.bpp -o /tmp/pathfind

# Inspect module graph
bpp --show-deps src/bpp.bpp

# Inspect phase annotations
bpp --show-phases src/bpp.bpp | grep REALTIME

# Emit C for portability analysis
bpp --c src/bpp.bpp > /tmp/bpp.c
```

---

# Meta — How this book is maintained

This book is living. Every change to the compiler, stdlib, or tooling updates its chapter in the same commit. Rules:

1. **A PR that ships new stdlib or backend logic MUST update its chapter.** Missing doc = CI should fail (today manual; eventually a gate).
2. **Depends on:** links are authoritative. If Cap 10 says "Depends on: Cap 4", and you change Cap 4 semantics, Cap 10 needs revisit. Grep `Depends on: Cap 4` to find them.
3. **Source: legacy_docs/X.md §Y** stays until the legacy section is marked ABSORBED in `legacy_docs/README.md`. Do not delete `legacy_docs/` until `grep -c PENDING\|DRAFT legacy_docs/README.md` returns 0.
4. **TOC at top of this file is the live index.** Status column drives the faxina progress. Run `grep -c PENDING docs/how_to_dev_b++.md` to see how many chapters remain.
5. **Code examples in every chapter should compile.** If they do not, fix the code OR the doc — never leave false examples.

When the book is complete: `legacy_docs/` is deletable, this file is the canonical reference, and any new chapter follows the same template.
