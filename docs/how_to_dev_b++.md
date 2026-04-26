# How to Dev B++ — The Unified Book

**The single entry point for everything about B++.** Writing programs. Writing the compiler. Writing backends. Adding tools. The language reference, the stdlib, the discipline, the architecture. One book. Twenty-eight chapters. Six parts.

If this book does not answer your question, the question is either:
1. not yet absorbed from `legacy_docs/` — check there, then file an issue against this book
2. genuinely novel — open a section, propose it, integrate

No other canonical document. Everything that used to live in separate markdown files in `docs/` now lives here as a chapter.

---

## Table of Contents

| Cap | Title | Part | Status | Source | Deps |
|-----|-------|------|--------|--------|------|
| 1 | Hello World | I — Começo | COMPLETE | new | — |
| 2 | The Auto-Injected Prelude | I | COMPLETE | new | 1 |
| 3 | Syntax | I | PENDING | legacy_docs/the_b++_programming_language.md §2 | 1 |
| 4 | Types | I | PENDING | legacy §3 | 3 |
| 5 | Functions and Effects | I | PENDING | legacy §2.5 | 3, 4 |
tun| 6 | Strings (bpp_str + sb_) | II — Arsenal | PENDING | legacy §4 | 4 |
| 7 | Arrays (bpp_array) | II | PENDING | legacy §5 | 4 |
| 8 | Hash tables (bpp_hash) | II | PENDING | legacy §5 | 7 |
| 9 | Buffers (bpp_buf) | II | PENDING | legacy §5 | — |
| 10 | Structs (language feature) | II | PENDING | legacy §4.5 | 4 |
| 11 | Enums (language feature) | II | PENDING | legacy §4.6 | 4 |
| 12 | I/O (bpp_io) | II | PENDING | legacy §4.1 | — |
| 13 | Runtime ABI (bpp_runtime) | II | PENDING | new | 9 |
| 14 | Tonify (the style rules) | III — Writing Pro | PENDING | legacy_docs/how_to_dev_b++.md Part 2 | — |
| 15 | Disciplina QG (general vs battalion) | III | COMPLETE | new | 2 |
| 16 | Idioms | III | PENDING | legacy how_to_dev Part 4 | 14, 15 |
| 17 | Anti-patterns | III | PENDING | legacy how_to_dev Part 5 | 14, 15 |
| 18 | Bootstrap procedure | IV — Building the Compiler | PENDING | legacy how_to_dev Part 1 | — |
| 19 | Canary discipline | IV | PENDING | legacy how_to_dev Part 5 | 18 |
| 20 | Testing | IV | PENDING | legacy how_to_dev Part 6 | 18 |
| 21 | Frontend (parser, lexer) | V — Compiler Architecture | PENDING | legacy the_b++ §7 | — |
| 22 | Spine (bpp_codegen, dispatch) | V | PENDING | legacy phase_backend_closeout | 21 |
| 23 | Battalions (chip backends) | V | PENDING | legacy max_plan_codegen_split | 22 |
| 24 | Runtime primitives (malloc, syscalls) | V | PENDING | legacy the_b++ §7 | 13, 23 |
| 25 | Adding a new backend | V | PENDING | legacy how_to_dev Part 7 | 22, 23, 24 |
| 26 | Bang 9 IDE | VI — Ecosystem | PENDING | legacy_docs/bang9_vision.md | — |
| 27 | Tools (modulab, level_editor, mini_synth) | VI | PENDING | legacy_docs/bang9_*embed/factory*.md | 26 |
| 28 | Roadmap and Current Status | VI | PENDING | legacy_docs/roadmap_to_1_0.md | — |
| 29 | Audio output (stbaudio) | VI | COMPLETE | new | 2, 15 |
| 30 | Audio mixer (stbmixer) | VI | COMPLETE | new | 29 |
| 31 | Sound files (stbsound) | VI | COMPLETE | new | 30 |
| 32 | Input (stbinput) | VI | COMPLETE | new | 2 |
| 33 | Window (stbwindow) | VI | COMPLETE | new | 2 |
| 34 | Game loop (stbgame) | VI | COMPLETE | new | 32, 33 |
| 35 | Drawing (stbdraw) | VI | COMPLETE | new | 34 |
| 36 | UI widgets (stbui) | VI | COMPLETE | new | 35 |
| 37 | Image decode (stbimage) | VI | COMPLETE | new | 2 |
| 38 | Palette (stbpal) | VI | COMPLETE | new | 35 |
| 39 | Tilemap (stbtile) | VI | COMPLETE | new | 38, 42 |
| 40 | ECS (stbecs) | VI | COMPLETE | new | 2 |
| 41 | Physics (stbphys) | VI | COMPLETE | new | 2 |
| 42 | Pathfinding (stbpath) | VI | COMPLETE | new | 2 |
| 43 | Pool allocator (stbpool) | VI | COMPLETE | new | 2 |
| 44 | Color math (stbcol) | VI | COMPLETE | new | 2 |
| 45 | Asset hub (stbasset) | VI | COMPLETE | new | 37, 31, 38 |
| 46 | Level forge (stbforge) | VI | COMPLETE | new | 45 |
| 47 | GPU render (stbrender) | VI | COMPLETE | new | 38 |

Each chapter header repeats `Source:` / `Status:` / `Depends on:` so grep-based audits work. Status legend: `PENDING` (scaffolded, no content) / `DRAFT` (partial content migrated, needs review) / `COMPLETE` (content fully here, legacy source marked ABSORBED in `legacy_docs/README.md`).

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
    print_msg("Hello, World");
}
```

Compile and run:

```bash
bpp hello.bpp -o hello
./hello
# Hello, World
```

That is everything. `print_msg` writes a string to stdout followed by a newline. `main` returns 0 implicitly when no explicit return. No `#include`, no `import`. How that works is Cap 2.

### Variations

Print an integer:

```c
main() {
    print_int(42);
    print_ln();
}
```

Interpolate:

```c
main() {
    auto score;
    score = 150;
    print_str("score: ");
    print_int(score);
    print_ln();
}
```

B++ has no `printf` format strings. Composition is explicit — chain `print_*` calls or build a string with `sb_*` (Cap 6).

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
| `bpp_io.bsm` | stdout / stderr helpers | `print_msg`, `print_str`, `print_int`, `print_ln`, `putchar`, `putchar_err`, `getchar` |
| `bpp_str.bsm` | strings, both static and builder | `str_len`, `str_eq`, `str_starts`, `str_ends`, `str_dup`, `sb_new`, `sb_cat`, `sb_int` |
| `bpp_array.bsm` | dynamic arrays with shadow header | `arr_new`, `arr_push`, `arr_get`, `arr_set`, `arr_len`, `arr_free` |
| `bpp_hash.bsm` | hash tables (word keys + string keys) | `hash_new`, `hash_set`, `hash_get`, `hash_str_new`, `hash_str_set` |
| `bpp_buf.bsm` | byte buffers + LE/BE multi-byte read/write | `read_u8/16/32/64`, `write_u8/16/32/64`, `read_u16be`, `write_u16be` |
| `bpp_math.bsm` | pure B++ sqrt, sin, cos | `sqrt`, `sin`, `cos`, `abs`, `min`, `max` |
| `bpp_file.bsm` | file I/O | `file_read_all`, `file_write_all` |
| `_stb_platform.bsm` (per-OS) | window, clock, input primitives | `_stb_init_window`, `_stb_get_time`, `_stb_poll_events`, `_stb_frame_wait`, `_stb_should_close` |

### Why this works

The compiler (`src/bpp_import.bsm`) auto-injects these nine modules at the top of every compilation unit. You see their names as if they were built-in — they are not. They are real B++ code in `src/bpp_*.bsm` and `src/backend/os/<os>/_core_<os>.bsm`. The compiler just makes them visible without requiring an explicit `import` statement.

Two groups of functions are NOT in this prelude and need `import`:

1. **`stb/*.bsm`** — game / tool libraries (`stbgame`, `stbdraw`, `stbui`, `stbaudio`, `stbtile`, ...). These are the game engine; import as needed.
2. **Compiler-internal** modules (`bpp_codegen`, `bpp_parser`, `bpp_lexer`, `bpp_internal`, `bpp_types`, ...). These are for people writing the compiler itself. User programs should not import them.

### Consequence for Hello World

The three-line `main() { print_msg("Hello, World"); }` works because:
- `main` is the standard entry point the compiler looks for
- `print_msg` is a function in `bpp_io.bsm`, which was auto-injected
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
(`1.5e3`, `2.0e-7`). 64-bit doubles.

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
    print_msg("Hello\n");
}

// With typed parameters:
physics_tick(world, dt: float) { ... }

// With effect annotation (see §5.3):
pure_hash(s) : base { ... }
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
stub compute(x) : base;   // stub must carry the effect signature
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

### §5.4 — Effect annotations (the four phases)

Functions carry one of four **phase annotations** that describe
what kind of work they do:

```c
solo     // runs on main thread, drives real-time side effects
base     // pure computation, parallelizable on worker threads
io       // blocking I/O — stdin/stdout, files, sockets, syscalls
heap     // allocates memory (malloc, arr_push, buf_new)
```

The effects form a lattice — a function inherits the WORST effect
of any function it calls. A `: base` function that calls a `: io`
function is itself `: io`. The compiler tracks this automatically
and emits errors if annotations don't match the actual call graph.

**Phase :base = the parallelizable phase.** Anything marked
`: base` can run on a worker thread during smart dispatch. The
compiler rejects side-effect-causing operations inside a `: base`
function (no writes to shared globals, no I/O, no allocation) —
the annotation is a contract the compiler enforces.

```c
// Pure function — parallelizable:
static _dist(ax, ay, bx, by) : base {
    auto dx, dy;
    dx = bx - ax; dy = by - ay;
    return sqrt(dx * dx + dy * dy);
}

// Game loop — drives presentation, stays on main thread:
void tick(w, dt) : solo {
    update_positions(w, dt);    // inherits :solo through call
    render_sprites(w);
}
```

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

## Cap 6 — Strings (bpp_str + sb_)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §4 + new content for §6.0 (three shapes rationale)*
*Status: COMPLETE — 2026-04-23*

### §6.0 — Three shapes of string in B++ (and why)

Before any API, understand the vocabulary: B++ represents strings in **three different shapes**, each for a different job. They are not interchangeable, and the choice of which to use is the first question every string-handling function answers.

#### The three shapes at a glance

| Shape | What it is | Where memory lives | API module | Who uses it |
|-------|-----------|-------------------|-----------|-------------|
| **Static C-string** | byte array terminated by `\0` | allocated by caller (literal, stack, or heap) | `bpp_str.str_*` | Every user program — the everyday string |
| **Dynamic builder** | `{length, capacity}` header + byte array that grows | heap, managed by builder, realloc on grow | `bpp_str.sb_*` | Programs that build strings from pieces |
| **Packed ref (slice)** | `(offset, length)` packed into 64 bits | no memory of its own — points INTO another buffer | `bpp_internal.buf_eq` / `packed_eq` (compiler-only) | The compiler's parser / codegen |

A physical analogy: a library.
- **Static C-string** = a book you bought. Has a cover (the NUL terminator), lives on your shelf, is yours.
- **Dynamic builder** = an empty notebook you fill in. Starts small, grows as you write. You throw it away when done.
- **Packed ref** = a bookmark saying "page 47, lines 3-15 of Dom Casmurro." You do not own the book; you only know where to find the passage.

#### Why three shapes

Three shapes exist because three use cases have genuinely different optimal representations:

1. **User programs handle existing strings.** You get a string (argv parameter, file content, config value) and you want to read it. The NUL terminator convention from C is the right shape — caller already has the memory, function just walks bytes until zero. `bpp_str` operates here.

2. **Programs build strings from pieces.** Logging, JSON serialization, formatted messages. You do not know the final size in advance; the string grows as you append. A builder with header `{len, cap}` is the right shape — it manages its own memory, reallocates on grow, and gives you amortized O(1) append. `sb_*` operates here.

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
msg = sb_new();
msg = sb_cat(msg, "hello");
print_int(str_len(msg));       // → 5, works
if (str_eq(msg, "hello")) {}   // → 1, works
msg = sb_free(msg);
```

What does NOT work: writing into a builder via `str_cpy` or `str_cat_raw`. Those functions ignore the builder's header and corrupt the `{len, cap}` bookkeeping. Always use `sb_cat` / `sb_ch` / `sb_int` to extend a builder.

#### What a function actually returns

An orthogonal clarification many programmers need. **Every B++ function returns ONE WORD (64 bits).** Not "a string", not "an array", not "a struct" — a word. What that word *means* is a contract between function and caller, documented in comments:

```c
// Returns the byte count — pure scalar, no pointer
str_len(s) { ... }              // → 5

// Returns a pointer to a freshly malloc'd NUL-terminated byte sequence.
// Caller is responsible for free(). This is a C-string shape.
str_dup(s) { ... }              // → 0x600000...

// Returns a pointer past the header of a builder. Caller uses sb_* only.
sb_new() { ... }                // → raw + 16

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
| Building a message from pieces, size unknown | `sb_new` + `sb_cat` + `sb_int` + `sb_free` |
| Accumulating a log or serializing JSON | `sb_*` |
| Parser-internal work on source-buffer slices | packed refs (`buf_eq`, `packed_eq`) — compiler only |

One-sentence rule: **`str_*` reads or writes memory you already own. `sb_*` builds memory that grows. Packed refs view memory someone else owns.** Use the one whose story matches your job.

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
if (str_ends(path, ".bpp")) { print_msg("B++ source"); }

// Safe copy into fixed buffer
auto name_buf;
name_buf = malloc(64);
str_cpy(name_buf, argv_get(1));

// Length
auto n;
n = str_len("hello");   // 5
```

### §6.2 — `sb_*` — Dynamic string builder

The `sb_*` family in `src/bpp_str.bsm` builds strings from pieces when you do not know the final length up front. The builder allocates a heap-backed buffer and doubles it as needed.

| Function | Description |
|----------|-------------|
| `sb_new()` | Allocate a fresh builder. Returns an opaque pointer. Must be freed with `sb_free`. |
| `sb_cat(b, s)` | Append the null-terminated string `s` to the builder. |
| `sb_ch(b, c)` | Append a single byte `c` to the builder. |
| `sb_int(b, n)` | Append the decimal representation of integer `n`. |
| `sb_float(b, v)` | Append a float value formatted as a decimal string. |
| `sb_free(b)` | Release the builder and return the finished null-terminated string as a heap allocation. The caller owns the returned pointer and must `free` it. |

Lifecycle example — build a log line, print it, free it:

```c
auto b, result;
b = sb_new();
sb_cat(b, "wave ");
sb_int(b, wave_num);
sb_cat(b, " closed: ");
sb_int(b, tests_passed);
sb_cat(b, " passed");
result = sb_free(b);    // b is now gone; result is a fresh heap string
print_msg(result);
free(result);
```

The builder is not reusable after `sb_free`. If you need a second string, call `sb_new` again.

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
// Print "score: 1234" using explicit chaining
print_str("score: ");
print_int(score);
print_ln();

// Or build it for display in a UI widget
auto b, label;
b = sb_new();
sb_cat(b, "score: ");
sb_int(b, score);
label = sb_free(b);
draw_text(label, hud_x, hud_y, 1, WHITE);
free(label);
```

**3. Log accumulation**

```c
auto log;
log = sb_new();
sb_cat(log, "[init] ");
sb_int(log, step);
sb_cat(log, " done\n");
// ... more sb_cat calls ...
auto text;
text = sb_free(log);
file_write_all("run.log", text, str_len(text));
free(text);
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

## Cap 7 — Arrays (bpp_array)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §5*
*Status: PENDING*

---

## Cap 8 — Hash Tables (bpp_hash)

*Depends on: Cap 7*
*Source: legacy_docs/the_b++_programming_language.md §5*
*Status: PENDING*

---

## Cap 9 — Buffers (bpp_buf)

*Depends on: —*
*Source: legacy_docs/the_b++_programming_language.md §5*
*Status: PENDING*

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
*Source: legacy_docs/the_b++_programming_language.md §4.1*
*Status: PENDING*

---

## Cap 13 — Runtime ABI (bpp_runtime)

*Depends on: Cap 9*
*Source: new module bpp_runtime.bsm*
*Status: PENDING — module bpp_runtime.bsm does not exist yet; Fase 1.4 creates it*

---

# Part III — Writing Pro B++

The rules, patterns, and anti-patterns that separate a first program from a shippable one.

---

## Cap 14 — Tonify (the style rules)

*Depends on: —*
*Source: legacy_docs/how_to_dev_b++.md Part 2*
*Status: PENDING*

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

B++ carries three string representations (Cap 6 §6.0): static C-strings (`bpp_str.str_*`), dynamic builders (`bpp_str.sb_*`), and packed refs (`bpp_internal.buf_eq` / `packed_eq`). These are **not** a discipline failure — they are three genuinely different data shapes for three genuinely different jobs:

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
*Status: PENDING*

---

## Cap 17 — Anti-patterns

*Depends on: Cap 14, Cap 15*
*Source: legacy_docs/how_to_dev_b++.md Part 5*
*Status: PENDING*

---

# Part IV — Building the Compiler

Bootstrap, canary, testing. The mechanics of changing B++ itself.

---

## Cap 18 — Bootstrap procedure

*Depends on: —*
*Source: legacy_docs/how_to_dev_b++.md Part 1*
*Status: PENDING*

---

## Cap 19 — Canary discipline

*Depends on: Cap 18*
*Source: legacy_docs/how_to_dev_b++.md Part 5*
*Status: PENDING*

---

## Cap 20 — Testing

*Depends on: Cap 18*
*Source: legacy_docs/how_to_dev_b++.md Part 6*
*Status: PENDING*

---

# Part V — Compiler Architecture

Frontend, spine, battalions, runtime. How `src/` fits together.

---

## Cap 21 — Frontend (parser, lexer)

*Depends on: —*
*Source: legacy_docs/the_b++_programming_language.md §7*
*Status: PENDING*

---

## Cap 22 — Spine (bpp_codegen, dispatch)

*Depends on: Cap 21*
*Source: legacy_docs/phase_backend_closeout.md + new architectural content*
*Status: DRAFT — conceptual body complete, recipe section pending*

### The spine in one sentence

**`src/bpp_codegen.bsm` is the portable tree-walker that turns the AST into a sequence of primitive operations. It decides WHAT needs to happen. Backends (Cap 23) decide HOW to encode it for a specific chip.**

### Why a spine exists

A naive compiler has one codegen file per target (x86, ARM, RISC-V, C emitter). Each one re-implements the complete recipe of how to walk an AST: visit literals, visit binary operators, visit calls, handle control flow, emit stack frames, evaluate arguments in the right order, push and pop, coerce types.

The recipe is **95% identical across targets**. Only the final instruction at each leaf changes. Duplicating the recipe three or four times (as B++ did for most of its first year) means:

- Every bug fix is triplicated — and two of the three copies drift out of sync over time.
- Every new language feature (a new AST node, a new effect lattice edge) must be added in every backend.
- Porting to a new chip is 2000+ lines of "rebuild the recipe."

A spine consolidates the recipe. The spine in `src/bpp_codegen.bsm` walks the AST, builds up the argument-evaluation sequence, handles push/pop bookkeeping, tracks register state, and then calls a **chip primitive** for the single final instruction. Each chip implements ~50 primitives (add, sub, load, store, compare, branch, syscall, ...) and nothing more. Port RISC-V = implement 50 primitives. Not 2000 lines of rewritten recipe.

### What the spine currently owns

After Phase 3.4 (closed 2026-04-20), the spine owns every AST node type:

- **Literals** — `T_LIT` (integer, string, float) dispatches through `cg_prim.emit_lit_int` / `emit_lit_str` / `emit_lit_float`.
- **Variables** — `T_VAR`, `T_AUTO`, `T_STATIC`, `T_EXTRN`, `T_GLOBAL`. Dispatches through `cg_prim.emit_var_load` / `emit_var_store` / `emit_var_addr`.
- **Operators** — `T_BINOP` and `T_UNARY`. Spine evaluates operands in canonical order (second first, push, first, pop), chip emits the final add/sub/shl/etc.
- **Memory** — `T_MEMLD`, `T_MEMST`, `T_ADDR`. Typed loads and stores, chip emits the final ldr/str of the right width.
- **Control flow** — `T_IF`, `T_WHILE`, `T_FOR`, `T_TERNARY`, `T_BREAK`, `T_CONTINUE`, `T_RET`. Spine emits the label plumbing; chip emits the branch instruction.
- **Function calls** — `T_CALL`. Spine evaluates arguments, arranges them in ABI slots, calls the chip's `emit_call_full` to land the actual call instruction.
- **Function definitions** — `cg_emit_func`. Spine emits prologue/arg-copy/body/teardown through chip primitives.

### What the spine does NOT yet own (and needs to)

**Inline builtin dispatch** — today, when the spine processes `T_CALL(peek, [addr])`, it passes the whole node to `emit_call_full` in each chip. Each chip then re-detects "oh, this is peek" by name, and emits the inline instruction instead of a real call. The detection logic is triplicated (Cap 23 shows the three copies).

The correct architecture: the spine detects the builtin name, orchestrates argument evaluation, and then calls a chip primitive for just the final instruction. This is **Wave 18**, described in detail in Cap 23.

### The full dispatch path for a simple call (pre-Wave 18)

```
parser: "peek(0x1000)" is parsed as T_CALL node with .a = "peek", .b = [lit_0x1000]
          │
          ▼
spine cg_emit_node sees T_CALL
          │
          ▼
delegates to chip's emit_call_full(node)
          │
          ▼
chip's emit_call_full checks: "is n.a == 'peek'? 'poke'? 'sys_write'? ..."
          │           (43 branches — one per builtin)
          ▼
chip emits inline load/store/syscall instruction
```

Three chips, three copies of that 43-branch ladder. Each branch duplicates "evaluate arg, push, evaluate next, pop, emit final instruction."

### The Wave 18 architecture (target)

```
parser: same T_CALL node
          │
          ▼
spine cg_emit_node sees T_CALL
          │
          ▼
spine's cg_builtin_dispatch(n) checks name against central table (43 entries)
          │      If match: spine evaluates args through cg_emit_node,
          │      push/pops via cg_prim, calls primitive for final instruction
          ▼
chip's emit_mem_read(width=1) emits ONE instruction: ldrb w0, [x0]   (or x86's mov al, [rax], or C's *(uint8_t*)...)
```

One dispatch table. ~10 chip primitives. Bug fixes, new builtins, and new backends all consolidated.

### The contract

Every chip backend exposes a struct of function pointers (`ChipPrimitives` in `bpp_codegen.bsm`). The spine calls through the struct; the chip provides the implementations. Adding a new chip = fill in the struct. Removing a chip = delete the file. The spine never changes.

---

## Cap 23 — Battalions (chip backends)

*Depends on: Cap 22*
*Source: legacy_docs/max_plan_codegen_split.md + new architectural content (Wave 18 recipe)*
*Status: DRAFT — body complete through Wave 18 recipe, legacy historical context pending absorption*

### Battalions in one sentence

**Each chip backend (`src/backend/chip/<chip>/`) translates the spine's abstract primitive operations into the actual byte sequences the chip's ISA understands. They do ONE thing: encode instructions.**

### The current state (pre-Wave 18)

Three battalions exist:

| Battalion | Files | Targets |
|-----------|-------|---------|
| `aarch64` | `a64_codegen.bsm`, `a64_primitives.bsm`, `a64_enc.bsm` | ARM64 native (macOS M-series, Linux ARM servers) |
| `x86_64`  | `x64_codegen.bsm`, `x64_primitives.bsm`, `x64_enc.bsm` | x86_64 Linux |
| `c`       | `bpp_emitter.bsm` | Emits C source (compiled by clang/gcc for any target the host C compiler supports) |

### The inline builtin duplication problem

Every backend has a ladder inside its `emit_call_full` (or equivalent) that looks like this:

```c
// Copy 1 of 3 (aarch64) — src/backend/chip/aarch64/a64_codegen.bsm:832
if (cg_str_eq(n.a, "peek", 4)) {
    arr = n.b;
    ety = a64_emit_node(*(arr + 0));
    if (ety == 1) { enc_fcvtzs(0, 0); }
    enc_ldrb_uoff(0, 0, 0);               // ← THE ONLY CHIP-SPECIFIC BIT
    return 0;
}

// Copy 2 of 3 (x86_64) — src/backend/chip/x86_64/x64_codegen.bsm:566
if (cg_str_eq(n.a, "peek", 4)) {
    arr = n.b;
    ety = x64_emit_node(*(arr + 0));
    if (ety == 1) { x64_enc_cvttsd2si(0, 0); }
    x64_enc_loadb(0, 0, 0);               // ← THE ONLY CHIP-SPECIFIC BIT
    return 0;
}

// Copy 3 of 3 (C emitter) — src/backend/c/bpp_emitter.bsm:675
if (emit_name_eq(name_p, "peek", 4)) {
    out("((long long)(*(uint8_t*)((uintptr_t)(");
    if (cnt > 0) { emit_node(arr[0]); }
    out("))))");                          // ← THE ONLY C-SPECIFIC BIT
    return;
}
```

The detection logic ("is this peek?"), argument evaluation, float-to-int coercion, push/pop, and stack management are **structurally identical across all three**. Only the final line (the `enc_ldrb`, `x64_enc_loadb`, or `out("((uint8_t*)...)")`) actually differs. 43 builtins × 3 backends = **~130 near-duplicate code blocks**.

This is the single largest architectural wart in B++'s current compiler. Every bug fix or new builtin has to touch three files. New backend port costs 2000+ lines of rebuilt recipe.

### Wave 18 — the lift

The fix is conceptually simple: move the detection + argument orchestration into the spine (Cap 22), leave only the chip-specific instruction at each leaf.

**Before Wave 18** — `emit_call_full` is a 43-branch ladder, triplicated:

```c
// a64_codegen.bsm::emit_call_full
if (is_peek) { eval args, push/pop, enc_ldrb; return; }
if (is_poke) { eval args, push/pop, enc_strb; return; }
if (is_sys_write) { eval x3, enc_svc; return; }
// ... 40 more ...
// real function call
enc_bl(...);
```

**After Wave 18** — `emit_call_full` is a 2-line router, each chip has a ~10-function primitive IR:

```c
// spine bpp_codegen.bsm::cg_emit_call
cg_emit_call(n) {
    if (cg_builtin_dispatch(n)) { return; }
    return cg_prim.emit_call_full(n);     // real call path
}

// spine bpp_codegen.bsm::cg_builtin_dispatch
cg_builtin_dispatch(n) {
    if (cg_str_eq(n.a, "peek", 4)) {
        cg_emit_node(n.b[0]);
        cg_coerce_float_to_int();
        cg_prim.emit_mem_read(1);         // chip emits ldrb / loadb / *uint8_t*
        return 1;
    }
    if (cg_str_eq(n.a, "poke", 4)) {
        cg_emit_node(n.b[1]);             // val
        cg_coerce_float_to_int();
        cg_emit_push();
        cg_emit_node(n.b[0]);             // addr
        cg_coerce_float_to_int();
        cg_emit_pop_scratch();
        cg_prim.emit_mem_write(1);        // chip emits strb / storeb / *uint8_t*= 
        return 1;
    }
    // ... 41 more names, each one handler ...
    return 0;
}

// a64_primitives.bsm — only the ISA-specific instructions
void a64_emit_mem_read(width) {
    if (width == 1) { enc_ldrb_uoff(0, 0, 0); }
    if (width == 2) { enc_ldrh_uoff(0, 0, 0); }
    if (width == 4) { enc_ldr_w_uoff(0, 0, 0); }
    if (width == 8) { enc_ldr_uoff(0, 0, 0); }
}
// (and 9 more primitives — emit_mem_write, emit_syscall, emit_shift_right, ...)
```

### The 10-primitive surface every battalion exposes

Wave 18's proof set identifies the minimum primitive IR that covers the 43 builtins:

| Primitive | Purpose | Example instructions |
|-----------|---------|---------------------|
| `emit_load_imm(slot, imm)` | Load an integer literal into a register slot | `mov rax, imm` / `movz x0, imm` |
| `emit_push_acc()` | Push accumulator (slot 0) onto stack | `push rax` / `str x0, [sp, -16]!` |
| `emit_pop_scratch()` | Pop top of stack into scratch (slot 1) | `pop rcx` / `ldr x1, [sp], 16` |
| `emit_mem_read(width)` | Load `width` bytes from `[acc]` into acc | `mov al, [rax]` (w=1) / `ldrb w0, [x0]` |
| `emit_mem_write(width)` | Store `width` bytes from scratch into `[acc]` | `mov [rax], cl` (w=1) / `strb w1, [x0]` |
| `emit_shift_right()` | `acc = acc >> scratch` | `shr rax, cl` / `lsr x0, x0, x1` |
| `emit_barrier()` | Memory barrier | `mfence` / `dmb ish` |
| `emit_syscall(argc)` | Issue syscall with argc args | `syscall` / `svc 0x80` |
| `emit_assert(kind)` | Crash with diagnostic | `int3` / `brk 0` |
| `emit_fn_ptr(name_packed)` | Load function address into acc | `lea rax, [rip + sym]` / `adrp x0, sym` |
| `emit_argv_slot(which)` | Load argc/argv/envp from saved slot | `mov rax, [rbp - N]` / `ldr x0, [x29, N]` |

Most builtins map to 1 or 2 of these primitives. `peek` is `emit_mem_read(1)` after arg eval. `poke` is `emit_mem_write(1)` after eval+push+eval+pop. `sys_write` is three arg evals + `emit_syscall(3)`.

### Recipe: how to lift a builtin (`peek` as worked example)

Every builtin lift follows the same 8 steps. First time through takes ~30 minutes; subsequent builtins ~10 minutes. All intermediate steps must preserve byte-identity of the compiled `bpp` binary.

**Step 1 — Extract the chip-specific piece as a named primitive.**
In `a64_primitives.bsm`, add `void a64_emit_mem_read(width) { if (width == 1) { enc_ldrb_uoff(0, 0, 0); } ... }`. Still unused. Bootstrap. `gen1 == gen2` expected (pure addition, no caller). ✓

**Step 2 — Same for x64.**
In `x64_primitives.bsm`, add `void x64_emit_mem_read(width) { if (width == 1) { x64_enc_loadb(0, 0, 0); } ... }`. Bootstrap + Docker (x86 backend change). ✓

**Step 3 — Same for C emitter.**
In `bpp_emitter.bsm`, add `void c_emit_mem_read(width) { if (width == 1) { out("((long long)(*(uint8_t*)((uintptr_t)("); } ... }`. Bootstrap. ✓

**Step 4 — Wire the primitive into ChipPrimitives struct.**
Add `emit_mem_read` field to `ChipPrimitives` in `bpp_codegen.bsm`. Each chip binds it in `cg_bind_chip_primitives`. Bootstrap. `gen1 == gen2` (still unused). ✓

**Step 5 — Add `cg_builtin_dispatch` to spine with empty table.**
In `bpp_codegen.bsm`, add the function. Call it from `cg_emit_call` before the chip's `emit_call_full`. Empty table — every call still falls through to the chip's own ladder. Bootstrap. `gen1 == gen2`. ✓

**Step 6 — Add `peek` entry to the spine dispatch table.**
Spine now handles `peek` itself. The chip's old `peek` branch becomes dead code (unreachable because spine caught the name first). Bootstrap. `gen1 == gen2` expected — the EMITTED bytes are identical; only which codepath produced them changed. ✓

**Step 7 — Delete the chip-specific `peek` branches.**
In `a64_codegen.bsm`, `x64_codegen.bsm`, `bpp_emitter.bsm` — remove the `if (cg_str_eq(n.a, "peek", 4)) { ... }` block. Three deletions in three commits, one bootstrap each. `gen1 == gen2`. ✓

**Step 8 — Write the test.**
`tests/test_builtin_peek_lifted.bpp` — a program that exercises `peek` across types (int, float-coerced, nested expressions) and asserts expected bytes. This becomes the regression guard for the lifted dispatch.

### Abort conditions specific to Wave 18

- **`gen1 != gen2` at any lift step** — the dispatch reordering accidentally changed emitted bytes. Bug in the spine's push/pop ordering or float coercion. Revert, investigate with `bug --dump-str` on both binaries to locate where the byte streams diverge.
- **Docker x86_64 diverges from macOS ARM** — primitive IR is inconsistent between chips (one chip expects acc-in-slot-0, another expects stack-top). Stop all lifts, reconcile the primitive contract first.
- **A builtin does not fit the 10-primitive surface** — some syscalls need more arguments than `emit_syscall(argc)` handles cleanly. Allow chip-local dispatch for that specific builtin; document it as a "divergent" builtin in this chapter with justification. Not every lift has to succeed.

### Why this chapter matters more than any other

The whole book describes B++'s architecture. This chapter describes **the piece that was built ad-hoc first and rearchitected last**. Every other part of the compiler landed reasonably clean from the start. The builtin dispatch accumulated 43 near-duplicate code paths across three backends before anyone had the vocabulary to name why that was a problem.

"Make it work, make it right" — this chapter is the "make it right" arc for the most-reviewed and most-touched code in the entire compiler. If B++ ships 1.0 with the dispatch still triplicated, that wart outlives the language. Wave 18 closes it.

---

## Cap 24 — Runtime primitives (malloc, syscalls)

*Depends on: Cap 13, Cap 23*
*Source: legacy_docs/the_b++_programming_language.md §7*
*Status: PENDING*

---

## Cap 25 — Adding a new backend

*Depends on: Cap 22, Cap 23, Cap 24*
*Source: legacy_docs/how_to_dev_b++.md Part 7*
*Status: PENDING*

---

# Part VI — Ecosystem

Bang 9, tools, game scripting, roadmap.

---

## Cap 26 — Bang 9 IDE

*Depends on: —*
*Source: legacy_docs/bang9_vision.md*
*Status: PENDING*

---

## Cap 27 — Tools (modulab, level_editor, mini_synth) — embed pattern

*Depends on: Cap 26*
*Source: legacy_docs/bang9_tool_embed.md + legacy_docs/bang9_factory.md + legacy_docs/bang9_live_preview.md*
*Status: PENDING*

---

## Cap 28 — Roadmap and Current Status

*Depends on: —*
*Source: legacy_docs/roadmap_to_1_0.md*
*Status: PENDING*

---

## Cap 29 — Audio output (stbaudio)

*Depends on: Cap 2 (auto-injected prelude), Cap 15 (QG discipline)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbaudio.bsm` is the bottom of the audio stack — it opens the device,
runs the realtime callback, and exposes one session-level volume knob.
Polyphonic mixing, WAV loading, and multi-bus routing live one level up
in `stbmixer.bsm` (Cap 30) and `stbsound.bsm`.

B++'s audio stack is the same split-layer design as every other stb module:
a public portable API (`stb/stbaudio.bsm`) sits on a platform implementation
(`src/backend/os/<os>/_stb_audio_<os>.bsm`). The portable file never touches
CoreAudio / ALSA / WASAPI directly — it calls the platform primitives via
target-suffix resolution.

On macOS the platform layer drives CoreAudio's AudioQueue with dlopen'd
function pointers. Linux ALSA lands when ELF dynamic linking ships. Both
backends expose the same public contract, so user code is zero-touch when
you cross-compile.

### §29.0 — Three shapes of volume (and why)

Volume is a **session property**, not a per-call property. You set it once,
and every subsequent tone / push / stream respects the choice. Three APIs
sit on the same underlying amplitude knob:

#### The three shapes at a glance

| Shape | API | Use case |
|-------|-----|----------|
| raw amplitude | `audio_set_amplitude(amp)` | Fine-grained control over the s16 peak (0..32767). Used when you need exact bit-level precision. |
| linear percent | `audio_set_volume(pct)` | UI sliders (0..100). Matches iTunes / Spotify / macOS volume HUD — every major consumer app uses linear-on-amplitude. |
| decibels | `audio_set_volume_db(db)` | Plugin UIs, mixing consoles. Matches VST / AU / CLAP parameter ranges (typically 0..-60 dB). |

All three APIs read from the same `_aud_amplitude` word. You can mix them
freely — `audio_set_volume(50)` followed by `audio_get_volume_db()` returns
approximately -6 dB.

#### Decision table

| Situation | API |
|-----------|-----|
| UI slider (0..100 knob) | `audio_set_volume(pct)` |
| Mixer fader / plugin param | `audio_set_volume_db(db)` |
| Exact bit-level amplitude | `audio_set_amplitude(raw)` |
| Toggle mute | `audio_mute()` / `audio_unmute()` |
| "Loudness in the test is too loud" | `audio_set_volume_db(0 - 24)` |

#### Why three shapes

Same reason B++ has three string shapes: **different callers think in
different units**. A game dev setting HUD volume thinks in percent. A
mixing engineer thinks in dB. A codec test that must hit an exact sample
value thinks in raw amplitude.

This is not novelty — every serious audio framework ships at least two
(raw + dB). CoreAudio's AudioQueueSetParameter takes raw; CoreAudio's
AVAudioUnit takes dB. PortAudio ships both. SDL_Mix ships linear 0-128.
You get to pick the one that matches the domain instead of translating
units yourself.

### §29.1 — `audio_set_amplitude` / `audio_get_amplitude`

```c
void audio_set_amplitude(amp) : io;   // clamps to 0..32767
audio_get_amplitude() : io;           // returns current peak
```

`amp` is a **signed-16 peak value**: 0 is silent, 32767 is the s16 maximum
(hard clip on most DACs). The default used by `audio_tone_test` is 8000
(≈24% of peak, ≈-12 dB). Values outside the range are clamped, not
wrapped — negative becomes 0, anything over 32767 becomes 32767.

```c
audio_set_amplitude(16384);   // exactly -6 dB
audio_set_amplitude(0);       // silence (use audio_mute for toggleable)
audio_set_amplitude(99999);   // clamped to 32767
```

### §29.2 — `audio_set_volume` / `audio_get_volume`

```c
void audio_set_volume(pct) : io;   // 0..100, clamped
audio_get_volume() : io;           // returns 0..100
```

Linear percent-of-peak knob. 100 maps to amplitude 32700 (deliberately
0.2% below s16 max so a full-scale setting has a sliver of headroom for
any downstream gain stage). 0 maps to 0 (silent).

The scale is **linear on amplitude, not on perceived loudness** — because
that's what every consumer audio slider does. For perceptually-linear
volume, use the dB API instead.

```c
audio_set_volume(50);    // ~16350 amp, about -6 dB
audio_set_volume(25);    // ~8175 amp — basically the default 8000
audio_set_volume(10);    // ~3270 amp, background level
```

**Watch out:** because the scale is linear, the top half of the slider
barely changes perceived loudness (100% vs 50% sounds like "loud vs
slightly quieter"). That's a UX artefact every audio app in the last 20
years has shipped with — users are used to it, so don't try to "fix" it
without good reason.

### §29.3 — `audio_set_volume_db` / `audio_get_volume_db` (plugin-oriented)

```c
void audio_set_volume_db(db) : io;   // typical range 0..-60
audio_get_volume_db() : io;          // returns 0..-96
```

Decibels map logarithmically to amplitude: each -6 dB halves the
amplitude exactly. This is the unit real audio professionals think in —
every plugin parameter, every mixer fader. Use this if your code is
going to sit next to other plugin-style software, or if users are going
to type numbers into a UI.

```c
audio_set_volume_db(0);         // full scale — 32700 amp
audio_set_volume_db(0 - 6);     // half    — 16350 amp
audio_set_volume_db(0 - 12);    // quarter — 8192 amp (≈ old default)
audio_set_volume_db(0 - 24);    // 1/16   — 2050 amp, comfortable
audio_set_volume_db(0 - 36);    // 1/64   — 510  amp, quiet background
audio_set_volume_db(0 - 60);    // 1/1024 — 32 amp, nearly silent
audio_set_volume_db(0 - 96);    // silence (below s16 resolution)
```

**Why a negative literal needs `0 - 36` instead of `-36`:** unary minus
on integer literals is parsed as a subtraction expression in the current
grammar. The workaround is either `0 - 36` or storing the constant in a
variable first (`auto dim = 0 - 36; audio_set_volume_db(dim);`). This
will be fixed when the lexer learns the unary-minus-on-literal shortcut.

#### Under the hood: faking `pow()` without a math runtime

B++'s runtime has no `pow()` yet, so `audio_set_volume_db` does the
dB→amplitude conversion with integer-only arithmetic:

1. Separate the dB value into full-halvings (`db / 6`) and remainder
   (`db % 6`). Each -6 dB is exactly one bit-shift right of the
   amplitude, so full-halvings lowers for free.
2. For the 0..-5 dB remainder, look up a 6-entry table of ratios-out-of-
   1000 (`1000, 891, 794, 708, 631, 562`) — these are `10^(-n/20) * 1000`
   rounded to nearest integer.
3. Combine: `amp = (32700 >> halvings) * remainder_ratio / 1000`.

Error is < 0.3% on amplitude across the whole 0..-96 dB range. The
human just-noticeable-difference for loudness is about 1 dB (≈12% amp);
0.3% is inaudible. When the runtime ships `pow()` the helper can swap
to direct math without changing the API.

### §29.4 — Mute (`audio_mute` / `audio_unmute` / `audio_is_muted`)

```c
void audio_mute() : io;
void audio_unmute() : io;
audio_is_muted() : io;           // 0 or 1
```

Mute remembers the previous amplitude and restores it on unmute. Safe to
call repeatedly — mute on an already-muted session is a no-op. Typical
pattern:

```c
if (key_pressed(KEY_M)) {
    if (audio_is_muted()) { audio_unmute(); }
    else                  { audio_mute(); }
}
```

The underlying implementation uses two words (`_aud_is_muted` flag + saved
amplitude) so the sentinel is unambiguous — user can legitimately set
amplitude to 0 without the mute toggle getting confused about state.

### §29.5 — Default amplitude and init

When a program first calls `audio_tone_test` (or any other signal
generator) without having configured volume, the platform layer seeds
`_aud_amplitude = 8000` (≈-12 dB). This default is applied **only if the
user has not set a volume beforehand** — the seeder checks for zero and
bails out otherwise, so a pre-session `audio_set_volume_db(0 - 24)` call
sticks through tone_start.

```c
main() {
    audio_set_volume_db(0 - 24);   // applies first
    audio_tone_test(440, 2000);    // respects -24 dB, does NOT reset to 8000
    return 0;
}
```

### §29.6 — Plugin pattern: cross-session volume persistence

For plugins that need volume to survive module reloads (typical in a
DAW context), store the dB value in whatever your preferences system is
and restore on init:

```c
// At plugin init:
auto saved_db;
saved_db = prefs_get_int("volume_db", 0 - 12);   // default -12 dB
audio_set_volume_db(saved_db);

// When the user drags the volume slider:
audio_set_volume_db(new_db);
prefs_set_int("volume_db", new_db);
```

Because the set/get pair round-trips exactly for integer dB values, the
plugin UI stays consistent with the audio engine without a separate
"shadow value" in the UI state.

### §29.7 — What this chapter does NOT cover

- **Mixer fan-out** (per-channel volume). That's `stbmixer.bsm`'s
  territory — `mixer_set_voice_gain(voice_id, amp)` per slot. The
  `audio_*` APIs here set the master level that applies AFTER the
  mixer sum.
- **Fade curves** (smooth volume transitions over N ms). Not shipped
  yet — would be `audio_fade_to(target_amp, duration_ms)` when added.
  Today users step the value themselves in their render loop.
- **Stream I/O** (push-pull ring buffer). `audio_open` / `audio_push`
  / `audio_push_frames` / `audio_ring_space` are separate from volume;
  they control the data path, not the gain.

### §29.8 — Verification

The volume API has a proof-of-life test:

```
tests/test_audio_tone.bpp   — calls audio_set_volume_db before the tone
```

Compile and run:

```bash
./bpp tests/test_audio_tone.bpp -o /tmp/t
/tmp/t
```

A clean run plays ~2 seconds of a 440 Hz square wave at whatever volume
the test selected, prints seven non-zero FFI probe values, and reports
~86 callback invocations (2 seconds × 43 callbacks/sec at 1024 frames
per buffer). Exit code 0.

If the tone sounds at the default (-12 dB, loud), volume setter was not
called before `audio_tone_test`. If the tone is silent but callbacks are
non-zero, either the dB value was ≤-96 or an amplitude was set to 0.
Check the specific `audio_set_*` call before the tone.

---

## Cap 30 — Audio mixer (stbmixer)

*Depends on: Cap 29 (stbaudio)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbmixer.bsm` sits between sound sources and the single audio device.
It owns the voice pool, runs the per-sample synthesis loop, routes
voices to named buses (master / music / sfx), and pushes the mixed
output into stbaudio's SPSC ring.

The division of labor:
- `stbaudio` — one stream in, one device out. One master volume.
- `stbmixer` — N voices in, one stream out. Per-voice + per-bus +
  master volume, all composable.
- `stbsound` — decodes WAV files into buffers the mixer can play.

Game code almost always touches `stbmixer` (call `mixer_note_on` /
`mixer_play_sample` / `mixer_stream`); it rarely needs `stbaudio` beyond
`audio_open` / `audio_close`. If the mixer is not enough for your use
case, you can bypass it and push raw frames with `audio_push_frames` —
but that forfeits polyphony.

### §30.0 — The voice pool

The mixer maintains a fixed pool of 10 voice slots:
- **Slots 0-7** — the "SFX / tone pool". Any `note_on` or
  `play_sample` call lands here. When all 8 are busy, new notes are
  dropped on the floor (return -1).
- **Slot 8** — reserved for music. `play_music` always uses this
  slot; a second `play_music` replaces the first track rather than
  competing with SFX.
- **Slot 9** — spare for future streaming voices.

Each slot has 12 parallel arrays (kind, key, amp, phase, buffer, pos,
frames, bus, loop, ...) totaling 8 × 10 = 80 bytes per array, well
under a cache line family. No struct-of-voices layout — voices are
iterated in the per-sample loop, and parallel arrays are cache-
friendlier than struct-of-arrays for that pattern.

Voice kinds (stored in `_mx_voice_kind[slot]`):
- `MX_KIND_TONE` — synthesised square/sine/saw/triangle at `freq_hz`
- `MX_KIND_SAMPLE` — one-shot PCM buffer playback
- `MX_KIND_MUSIC` — looping (or not) PCM buffer playback
- `MX_KIND_NONE` — slot is free

### §30.1 — The four-level gain chain

Every sample passes through four multiplicative gains before it hits
the DAC. Knowing the order matters when you tune volumes:

```
voice sample
  × _mx_voice_amp[slot]           ← per-voice amplitude
  × _mx_bus_vol[bus] / 100        ← bus gain (MUSIC / SFX)
  × _mx_master_vol / 100          ← master gain (legacy synthkey knob)
  × _mx_bus_vol[MASTER] / 100     ← master bus slider
  → clipped to s16 → DAC
```

Conceptually, the four levels serve different roles:

| Level | Who sets it | When | Unit |
|-------|-------------|------|------|
| voice | `mixer_note_on` defaults to `_MX_VOICE_AMP` (8000). Caller may override via `mixer_set_voice_volume[_db]`. | Per-note | raw s16 or dB |
| bus   | App code balances MUSIC vs SFX. | Per-session or runtime | percent or dB |
| master | Legacy synthkey knob — often left at default 80. | Session | percent or dB |
| master bus | The final global volume slider in the UI. | User | percent or dB |

Collapsing: if your app only has one slider, set `_mx_bus_vol[MASTER]`
and leave the rest at defaults. Plugins and games with separate music
vs effects sliders use the bus layer. DAW-style per-voice ducking
uses the voice layer.

### §30.2 — Bus operations

Three named buses are defined at init: `MX_BUS_MASTER`, `MX_BUS_MUSIC`,
`MX_BUS_SFX`. Every voice is routed to one of them (default SFX for
tone / sample voices, MUSIC for the music slot).

```c
// Percent API (0..100)
void mixer_set_bus_volume(bus, vol);
mixer_get_bus_volume(bus);

// dB API (typical range 0..-60)
void mixer_set_bus_volume_db(bus, db);
mixer_get_bus_volume_db(bus);

// Mute toggles — remembers pre-mute volume
void mixer_mute_bus(bus);
void mixer_unmute_bus(bus);
mixer_bus_is_muted(bus);
```

Typical game scene:

```c
// Cinematic: dip the SFX while dialogue plays
mixer_set_bus_volume_db(MX_BUS_SFX, 0 - 18);   // -18 dB (about 1/8)
dialogue_play("level_up");
mixer_wait_until_done();
mixer_set_bus_volume_db(MX_BUS_SFX, 0);        // restore
```

Pause menu pattern:

```c
// User opens menu — duck music, full SFX for UI beeps
mixer_set_bus_volume_db(MX_BUS_MUSIC, 0 - 12);

// User picks "exit to title" — kill music immediately
mixer_mute_bus(MX_BUS_MUSIC);

// Back to game
mixer_unmute_bus(MX_BUS_MUSIC);
```

### §30.3 — Master operations

```c
void mixer_set_master_volume(vol);       // 0..100 (legacy synthkey)
mixer_get_master_volume();
void mixer_set_master_volume_db(db);
mixer_get_master_volume_db();

void mixer_mute_master();
void mixer_unmute_master();
mixer_master_is_muted();
```

`mixer_mute_master()` is the big red button — kills every bus output
without altering per-bus balance. Unmute restores the exact prior
state. Use for a game-wide "pause everything" or the classic "user
pressed M" shortcut.

The confusing bit: there are TWO master knobs — `_mx_master_vol`
(legacy synthkey-era) and `_mx_bus_vol[MX_BUS_MASTER]` (the named
bus). Both multiply the final sum. For most use cases, pick one and
leave the other at 100. Kept both for backward compatibility; a
future stbmixer v2 may collapse them.

### §30.4 — Per-voice operations

```c
void mixer_set_voice_volume(slot, amp);       // raw s16 peak, 0..32767
mixer_get_voice_volume(slot);
void mixer_set_voice_volume_db(slot, db);     // dB from 0 dB = 32700
mixer_get_voice_volume_db(slot);
```

Voice volume is the first multiplier in the gain chain. Setting it
to 0 silences that specific voice without affecting other voices on
the same bus. Useful for:

- **Fade-in** on note attack:
  ```c
  auto slot, frame;
  slot = mixer_note_on(key, 440);
  for (frame = 0; frame < 30; frame = frame + 1) {
      mixer_set_voice_volume(slot, 8000 * frame / 30);
      mixer_stream(128);
  }
  ```
- **Velocity-sensitive** MIDI-style triggers: scale `amp` by the
  velocity byte at note_on.
- **Ducking** a specific voice temporarily: save its current amp,
  drop it, restore later. Don't use `mixer_mute_bus` for a single
  voice — that kills every voice on the bus.

Voice amplitude persists until `note_off` or the next `note_on` on
that slot. It does NOT reset to `_MX_VOICE_AMP` (8000) between notes
from different keys — if you set a voice to 2000 and play a new note
at the same slot, the next note inherits 2000. Call
`mixer_set_voice_volume(slot, 8000)` after `note_off` to reset, or
set it explicitly on every `note_on`.

### §30.5 — Waveform fader and dirt (non-volume tone shaping)

These existed before the volume API and still belong under mixer
control:

```c
void mixer_set_fader(val);      // 0=sine 256=triangle 512=saw 768=square
mixer_get_fader();

void mixer_set_dirt(val);       // 0=clean, 256=max bitcrush+decimation
mixer_get_dirt();
```

Fader cross-blends four waveforms. Dirt stacks bitcrush + sample-and-
hold decimation onto the final mix. Both are orthogonal to the gain
chain — they affect the tone timbre, not the loudness. A chiptune
test that wants 8-bit-era sound turns both cranked:

```c
mixer_set_fader(768);     // pure square
mixer_set_dirt(180);      // moderate crunch
```

### §30.6 — Streaming the mix

```c
mixer_stream(n);           // fill and push n frames to the audio ring
mixer_fill(buf, n);        // fill a user buffer without pushing (for analysis)
```

Typical game loop:

```c
auto space;
space = audio_ring_space();
if (space > 0) {
    if (space > 512) { space = 512; }   // scratch buf cap
    mixer_stream(space);
}
```

The mixer's scratch buffer is 512 frames (`_MX_FILL_BUF_FRAMES`).
Larger pushes split into multiple calls. For the stream to stay
uninterrupted, feed at least as many frames per second as the
device consumes (44100 for stbaudio's default rate).

### §30.7 — Precision caveat for dB APIs

Percent storage inside the mixer is integer 0..100. Going through
`_db → pct` loses precision for values below about -40 dB (where
percent would round to 0). For plugin-grade precision, you would
need to refactor the internal storage to raw amplitude (0..32767)
— which is a breaking change and not yet scheduled.

Practical effect:
- `mixer_set_bus_volume_db(bus, 0 - 48)` sets percent to 0 — silent.
- Readback then returns -60 dB (the floor), not -48 dB.
- Voices with dB values above -40 round-trip exactly for integer dB.

If a future DAW or plugin host needs sub-dB precision, budget a
refactor that promotes `_mx_bus_vol` and friends to `long long`
amplitude units (0..32767) with the percent API as a wrapper.

### §30.8 — Relationship with stbaudio's volume

stbaudio has its own `audio_set_amplitude / audio_set_volume /
audio_set_volume_db` (Cap 29). These knobs operate on the audio
stream AFTER the mixer pushed its samples. The full gain chain
from a mixer voice to the DAC is:

```
voice sample
  × voice_amp                                  (mixer)
  × bus_vol / 100                              (mixer)
  × _mx_master_vol / 100                       (mixer)
  × master_bus_vol / 100                       (mixer)
  → push to audio ring
  × _aud_amplitude / 32767                     (stbaudio callback)
  → DAC
```

Two distinct master layers. **Rule of thumb:** use the mixer's
master when adjusting game internals (music ducking, fade-outs).
Use stbaudio's master for session-level volume persistence (user's
global volume slider that survives game restarts). Don't use both
at the same time unless you enjoy debugging gain chains.

### §30.9 — Verification

```
tests/test_mixer_sample.bpp   — one-shot sample playback
tests/test_mixer_stream.bpp   — continuous streaming of mixer output
```

Both exercise the post-volume-API mixer end to end: open device,
init mixer, set volumes, play samples / notes, confirm playback via
ring drain counters. A passing run prints `OK` and exits 0. Run
them after any change to stbmixer:

```bash
./bpp tests/test_mixer_sample.bpp -o /tmp/t && /tmp/t
./bpp tests/test_mixer_stream.bpp -o /tmp/t && /tmp/t
```

### §30.10 — What this chapter does NOT cover

- **Solo** (mute all voices except the selected one). DAW feature,
  not shipped. Workaround: save each voice's amp, drop unselected
  ones to 0, restore on un-solo.
- **Fade curves** over N ms. Not shipped. Step the voice amp in
  your render loop — same pattern as the fade-in example above.
- **Per-voice panning** (L/R balance). The mixer outputs mono-on-
  both-channels today. Per-voice pan is a future voice-state field.
- **Send effects** (reverb / delay routed through a dedicated bus).
  Would require a full FX graph, not just a bus routing; out of
  scope for the current mixer.

---

## Cap 31 — Sound files (stbsound)

*Depends on: Cap 30 (stbmixer)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbsound.bsm` is a **format-only** module — it reads and writes audio
files on disk. No device handle, no callback, no real-time behaviour.
A program that just wants to inspect a WAV (dump RMS, compute length,
slice regions) can import stbsound alone without opening the audio
device.

The split from stbmixer is deliberate: playback and decoding are
different lifetimes. A game loads 5 WAVs at boot, then plays them
repeatedly via the mixer. Decoding happens once; playback happens
every frame. Keeping them in separate modules lets tools (level
editors, sample ripper, audio debugger) use one without the other.

### §31.1 — Format scope

Today stbsound speaks exactly one file format: **RIFF WAV, 44100 Hz,
16-bit signed, stereo interleaved PCM**. Each frame is 4 bytes:
`left_lo, left_hi, right_lo, right_hi`. This is the same format
stbaudio's SPSC ring uses, so a loaded buffer plugs directly into
`mixer_play_sample` without conversion.

Mono files or other sample rates are not supported — the WAV reader
rejects them with a clean error. Future formats (ogg / mp3 / flac)
would land as sibling modules (`stbogg`, `stbmp3`) rather than
extending stbsound, keeping each decoder focused.

### §31.2 — Load a WAV

```c
sound_load_wav(path) : io;   // returns 0 on success, -1 on failure
sound_buf() : base;          // pointer to the loaded sample buffer
sound_frames() : base;       // number of stereo frames in the buffer
```

The load pattern is stateful — there's one "last loaded" slot shared
across calls. This is deliberate: most programs load a handful of
WAVs at boot and track their handles separately, so the state is just
a convenience for the common "load then immediately play" pattern:

```c
// Load an SFX.
if (sound_load_wav("assets/apple_crunch.wav") != 0) {
    print_msg("failed to load apple_crunch\n");
    return 1;
}
auto apple_buf, apple_frames;
apple_buf    = sound_buf();
apple_frames = sound_frames();

// Load a music track — this overwrites the "last loaded" slot,
// but apple_buf still points at the first load (malloc'd).
if (sound_load_wav("assets/bgm.wav") != 0) { return 1; }
auto bgm_buf, bgm_frames;
bgm_buf    = sound_buf();
bgm_frames = sound_frames();

// Play them via the mixer.
mixer_play_sample(apple_buf, apple_frames);
mixer_play_music(bgm_buf, bgm_frames, 1);   // 1 = loop
```

The buffer is owned by the caller after the load — stbsound allocates
it with `malloc` and forgets the pointer once `sound_buf` has been
read. It's your responsibility to `free(apple_buf)` when the sample
is no longer needed. The mixer does NOT take ownership either; as
long as the sample is on an active voice, the buffer must stay alive.

### §31.3 — Save a WAV

```c
sound_save_wav(path, buf, n_frames) : io;   // returns 0 on success
```

Stateless — takes the buffer + frame count explicitly, writes the
file. Header construction happens inside the call: RIFF / WAVE / fmt
chunk (PCM s16 44.1k stereo) / data chunk. Total header is 44 bytes
plus the sample data.

Typical use case: save a render of generated audio so downstream
tools (Audacity, a plugin UI) can inspect:

```c
auto buf;
buf = malloc(60 * 44100 * 4);   // 60 seconds
mixer_fill(buf, 60 * 44100);    // synthesize
sound_save_wav("/tmp/render.wav", buf, 60 * 44100);
free(buf);
```

The write path uses `bpp_io.bsm`'s file API (`fopen` / `fwrite` /
`fclose` via the `: io` effect annotation). Partial writes return
-1 — filesystem full, permission denied, invalid path. The
incomplete file is left on disk; callers that want atomic writes
should save to a temp path and rename.

### §31.4 — Verification

```
tests/test_sound_roundtrip.bpp   — save then load + byte equality check
```

The round-trip test generates 1 second of silence, saves it, loads
it back, and asserts every byte matches. If this fails, the header
construction is wrong or the byte-order helpers (`_snd_w16` /
`_snd_w32`) dropped a bit somewhere.

### §31.5 — Relationship with the rest of the audio stack

```
stbsound  — disk ↔ buffer  (WAV read/write)
   ↓ buffer
stbmixer  — N buffers → 1 stream  (voices + buses + volume)
   ↓ stream
stbaudio  — 1 stream → DAC  (CoreAudio / ALSA / ...)
   ↓ speakers
```

Each layer is usable alone. A command-line tool that just dumps WAV
stats imports stbsound only. A synthesizer that generates tones
on the fly imports stbmixer + stbaudio (no stbsound). A game imports
all three.

### §31.6 — What this chapter does NOT cover

- **Streaming decoding** (load-as-you-play). `sound_load_wav` reads
  the whole file. For BGM files too big to keep in RAM, you'd need
  a streaming reader that feeds the mixer's ring in chunks.
- **Format conversion** (resample 48kHz → 44.1, mono → stereo). Not
  shipped. Future `sound_convert(src_buf, src_frames, src_rate,
  src_channels) → (dst_buf, dst_frames)` would live here.
- **Metadata** (RIFF LIST INFO tags, cue points). Skipped today —
  the reader treats everything after the `data` chunk header as
  PCM bytes.
- **Compressed WAV** (ADPCM, μ-law). PCM-only. Compressed variants
  are rare in games and land as a separate decoder if ever needed.

---

## Cap 32 — Input (stbinput)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbinput.bsm` is the keyboard + mouse polling layer. Every frame the
platform event pump fills the input arrays; game code reads them with
simple `key_down(KEY_X)` / `mouse_x()` style calls. No event queue to
drain, no listener registration, no callback plumbing — just state
you can query.

The module is platform-agnostic at the API level (same `key_down`
call whether you're on macOS Cocoa or Linux X11). Per-platform
scancode mapping lives in `_stb_platform_<os>.bsm` and normalizes
everything to the same 77-key enum.

### §32.1 — Three input shapes

| Shape | Use case | API |
|-------|----------|-----|
| **Held state** | "Is the key currently down?" Player movement while a key is held. | `key_down(k)` / `mouse_down(btn)` |
| **Edge trigger** | "Did the key start being pressed this frame?" Actions that should fire once per press (jump, shoot). | `key_pressed(k)` / `mouse_pressed(btn)` / `mouse_released(btn)` |
| **Text entry** | "What printable characters were typed this frame?" UI text fields. | `input_text_len()` / `input_text_char(i)` |

**Rule of thumb:**
- Movement → `key_down` (continuous polling)
- Actions → `key_pressed` (once per press, fires on the down-edge)
- Chat / console → `input_text_*` (respects keyboard layout, handles
  Shift for capitals)

### §32.2 — The KEY_* enum (77 keys)

Keys are normalized integer IDs. Grouped by category:

```
Navigation:  KEY_UP  KEY_DOWN  KEY_LEFT  KEY_RIGHT
WASD:        KEY_W  KEY_A  KEY_S  KEY_D
Chords:      KEY_SPACE  KEY_ENTER  KEY_ESC  KEY_TAB  KEY_BACKSPACE
Letters:     KEY_A..KEY_Z (all 26)
Digits:      KEY_0..KEY_9
Function:    KEY_F1..KEY_F12
Modifiers:   KEY_SHIFT  KEY_CTRL  KEY_ALT  KEY_META
Punctuation: KEY_PERIOD  KEY_COMMA  KEY_SLASH  KEY_MINUS  KEY_EQUAL
             KEY_LBRACKET  KEY_RBRACKET  KEY_SEMICOLON  KEY_QUOTE
             KEY_BACKSLASH  KEY_GRAVE
Editing:     KEY_DELETE  KEY_HOME  KEY_END  KEY_PGUP  KEY_PGDOWN
```

Each is an integer in the range 1..256 — directly indexes into the
underlying `_stb_keys` byte array. Never hardcode integers; always
use the enum name.

### §32.3 — Keyboard state

```c
key_down(k) : base;       // 1 if currently held, 0 if not
key_pressed(k) : base;    // 1 if just pressed this frame, 0 otherwise
```

`key_pressed` is `(current && !prev)` — fires exactly once per
physical press. Example: a pause toggle.

```c
if (key_pressed(KEY_P)) {
    paused = paused == 0 ? 1 : 0;
}
```

Holding P doesn't re-toggle every frame — only the initial press
fires. For continuous input, use `key_down`:

```c
if (key_down(KEY_W)) { player_y = player_y - speed; }
if (key_down(KEY_S)) { player_y = player_y + speed; }
```

The prev-state snapshot happens inside `_input_save_prev` called
before each `_stb_poll_events`. This runs automatically from
`game_frame_begin` (Cap 34) — you don't call it by hand.

### §32.4 — Mouse state

```c
mouse_x();                   // x in window pixels (0..window_w)
mouse_y();                   // y in window pixels (0..window_h)
mouse_down(btn);             // 1 if button held
mouse_pressed(btn);          // 1 if button pressed this frame
mouse_released(btn);         // 1 if button released this frame
```

Button IDs: `MOUSE_LEFT` (1), `MOUSE_RIGHT` (2), `MOUSE_MIDDLE` (3).

```c
if (mouse_pressed(MOUSE_LEFT)) {
    if (point_in_rect(mouse_x(), mouse_y(), btn_x, btn_y, btn_w, btn_h)) {
        fire_button();
    }
}
```

`point_in_rect(px, py, x, y, w, h)` is a convenience: returns 1 if
the point lies inside the rectangle (inclusive of left/top edges,
exclusive of right/bottom). Pure integer compare, no float math.

### §32.5 — Text input

Typed characters accumulate into a per-frame ring. Read them with
`input_text_len` + `input_text_char`:

```c
auto i, ch;
for (i = 0; i < input_text_len(); i = i + 1) {
    ch = input_text_char(i);
    if (ch == '\b')      { my_string_pop(); }        // backspace
    else if (ch == '\r') { submit(); }               // enter
    else                 { my_string_append(ch); }    // printable
}
```

The ring is 64 bytes per frame — way more than any human can type.
Overflow silently drops characters from the end. The buffer clears
at the start of every frame (`_input_save_prev` empties it).

**Shift handling is automatic** — the platform layer respects the
keyboard layout, so a Shift+A press lands as `'A'` (65) in the text
ring while `KEY_A` remains `key_down`. Don't try to reconstruct
case from `key_down(KEY_SHIFT)` — use the text ring.

### §32.6 — Actions (high-level binding)

For games that want the "configurable keybindings" pattern, stbinput
includes a named-action layer:

```c
void action_define(name, key, mod);   // register an action
void action_rebind(name, key, mod);   // change an existing binding
action_pressed(name);                 // edge trigger
action_down(name);                    // held state
```

Example:

```c
// At game init:
action_define("jump",   KEY_SPACE, 0);
action_define("shoot",  KEY_Z,     0);
action_define("save",   KEY_S,     ACTION_MOD_CTRL);

// In the game loop:
if (action_pressed("jump"))  { player_jump(); }
if (action_down("shoot"))    { fire_bullet(); }
if (action_pressed("save"))  { save_game(); }
```

Modifier flags: `ACTION_MOD_CTRL`, `ACTION_MOD_SHIFT`, `ACTION_MOD_ALT`,
`ACTION_MOD_META`. OR them together for multi-modifier actions.

Iteration (for a rebinding UI):

```c
auto n, i;
n = action_count();
for (i = 0; i < n; i = i + 1) {
    render_action_row(
        action_name_at(i),
        action_key_at(i),
        action_mod_at(i)
    );
}
```

### §32.7 — Frame lifecycle

Input state is refreshed by the game loop in this sequence:

```
frame N:
  1. _input_save_prev()     ← snapshot current → prev
  2. _stb_poll_events()     ← platform fills current + text ring
  3. [user code]            ← reads key_down/key_pressed/mouse_*/etc
  4. render + present
  5. sleep for frame budget
frame N+1:
  ...
```

`stbgame` (Cap 34) calls steps 1-2 inside `game_frame_begin()`. If
you build your own loop without stbgame, call them in the same order
before reading any input.

### §32.8 — What this chapter does NOT cover

- **Gamepad** (Xbox / PlayStation / Switch Pro). Not shipped. Would
  live in a separate module (`stbpad`).
- **Raw mouse motion** (delta-X / delta-Y between frames). You can
  compute this yourself — save `mouse_x()` from last frame and
  subtract. No rare-input support (mouse wheel, high-DPI deltas).
- **IME / CJK input composition**. The text ring emits one byte
  per codepoint, ASCII-only. Japanese / Chinese / Korean input
  would need a composition layer on top.
- **Touch** (iOS / Android). Out of scope — B++ targets desktop
  platforms today.

---

## Cap 33 — Window (stbwindow)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbwindow.bsm` covers two distinct concerns that happen to both
involve the native window system: **native dialogs** (save/open/
alert/confirm) and **live window geometry** (tracking resizes). It
sits on top of the platform layer's NSApp / NSWindow wrappers —
programs using stbwindow also need `import "stbgame.bsm"` because
the platform file owns the window lifecycle.

### §33.1 — Platform coverage

| Feature | macOS | Linux (X11) |
|---------|-------|-------------|
| Save dialog | ✓ NSSavePanel | ✗ returns 0 (cancelled) |
| Open dialog | ✓ NSOpenPanel | ✗ returns 0 |
| Folder dialog | ✓ NSOpenPanel folder mode | ✗ returns 0 |
| Alerts (info/confirm/error) | ✓ NSAlert | ✗ stderr warning |
| Live geometry | ✓ Cocoa KVC poll | ✓ X11 ConfigureNotify |
| Framebuffer resize | ✓ NSImageView | ✓ XPutImage |

The Linux dialog no-op is deliberate — desktop Linux has no stable
native file picker (Zenity / kdialog / portal spec move independently).
Callers that need cross-platform pickers should branch on `return
== 0` and fall back to a CLI prompt or a hardcoded path.

### §33.2 — File dialogs

```c
window_save_dialog(title, default_name) : io;    // → path or 0
window_open_dialog(title) : io;                  // → path or 0
window_open_folder_dialog(title) : io;           // → path or 0
```

All three return a **malloc'd null-terminated C-string** on success,
or `0` if the user cancelled. **Caller owns the string and must
`free` it.**

```c
auto path;
path = window_save_dialog("Save level", "untitled.level.json");
if (path == 0) { return; }            // user cancelled
file_write_all(path, buf, len);
free(path);
```

Dialogs **block the main thread** until dismissed. Audio rings
drain while the modal is up — the mixer will push silence, which is
usually imperceptible for a save flow. If you're running a real-time
audio demo and can't tolerate the pause, don't open a dialog there.

### §33.3 — Alerts

```c
void window_alert(title, message) : io;           // info + OK
window_confirm(title, message) : io;              // OK/Cancel → 1 or 0
void window_error(title, message) : io;           // red icon + OK
```

Info and error alerts are blocking acknowledgements — the user has
to click OK to proceed. Confirm is the standard "Are you sure?"
pattern:

```c
if (has_unsaved_changes) {
    if (!window_confirm("Discard?", "You have unsaved changes.")) {
        return;   // user picked Cancel
    }
}
close_document();
```

Choose the right shape:
- **alert** — completion acknowledgement ("Saved!")
- **confirm** — gated destructive action ("Delete selected layer?")
- **error** — red for real failures ("Failed to load /tmp/x: no
  such file")

### §33.4 — Live window geometry

For apps that want to react to user-driven window resizes (IDEs,
level editors, laid-out tools):

```c
window_width() : base;       // current content width in px
window_height() : base;      // current content height in px
window_resized();            // edge-triggered: 1 if changed since last call
void window_fb_resize();     // reallocate _stb_fb to match window
```

Typical resize-aware game loop:

```c
while (game_should_quit() == 0) {
    game_frame_begin();

    if (window_resized()) {
        window_fb_resize();          // match framebuffer to new window
        layout_recompute();          // re-lay-out your UI panels
    }

    clear(BLACK);
    draw_scene();
    draw_end();
}
```

`window_resized()` is **edge-triggered** — it returns 1 exactly once
per actual resize. Calling it twice per frame means the second call
returns 0 even if the window just resized. **Only ONE caller per
frame** should consume the flag (your main loop, not individual
panels).

### §33.5 — Framebuffer vs window size

These are different concepts:

- `window_width()` / `window_height()` — the native window's current
  content area in pixels. Grows/shrinks as the user drags the corner.
- `_stb_w` / `_stb_h` — the software framebuffer size. Fixed until
  you call `window_fb_resize()`.

By default, the platform **stretches** the old framebuffer into the
new window size (`NSImageView.imageScaling = .axesIndependently` on
macOS). Pixelated but keeps legacy apps working without opting in.

If you want crisp 1:1 pixels at the new size (typical for IDEs and
editors), call `window_fb_resize()` after `window_resized()` returns
1. The function frees the old framebuffer and reallocates
`_stb_w * _stb_h * 4` bytes at the new dimensions — **contents are
discarded**, so redraw the whole frame on the next frame.

Apps that don't care about resize (most games — they run at fixed
resolution) can ignore the whole live-geometry API. The platform
handles the stretch automatically.

### §33.6 — What this chapter does NOT cover

- **Multiple windows**. B++ supports exactly one window per process
  today. Modal dialogs piggyback on that window's event pump.
- **Window icon / title bar customization**. The title is set once
  via `game_init` (Cap 34) and never changes.
- **Custom cursors**. The platform uses the OS default cursor; no
  API to swap for a crosshair or drag-hand cursor yet.
- **Drag-and-drop**. Not shipped. macOS NSDraggingDestination would
  plug in here but hasn't been wired.
- **Clipboard**. Not shipped. Future `window_clipboard_read()` /
  `window_clipboard_write(s)` would sit next to the dialog APIs.

---

## Cap 34 — Game loop (stbgame)

*Depends on: Cap 32 (stbinput), Cap 33 (stbwindow)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbgame.bsm` is the **one-call game bootstrapping** module. Call
`game_init(w, h, title, fps)` and the whole stack wakes up: math,
input, draw, font, UI, str — all initialized, window opened, frame
timing armed. Everything after that point is a game loop reading
input, drawing, and calling `game_frame_begin()` + `draw_end()`.

stbgame is also the home of the **scene manager** — a tiny
state-machine layer that lets a game flip between MENU / PLAY /
RESULTS scenes without the main loop caring about any of them.

### §34.1 — Init + frame cycle

The canonical game loop:

```c
main() {
    game_init(640, 360, "My Game", 60);
    while (game_should_quit() == 0) {
        game_frame_begin();
        // ... update world using game_dt_ms() or game_dt() ...
        clear(BLACK);
        // ... draw ...
        draw_end();
    }
    game_end();
    return 0;
}
```

| Call | What it does |
|------|--------------|
| `game_init(w, h, title, fps)` | Init math/input/draw/font/ui/str, allocate framebuffer, open window, arm timing. |
| `game_frame_begin()` | Snapshot prev input, compute `dt_ms`, reset UI arena. |
| `game_dt_ms()` | Integer ms since last frame (clamped to 1..50). |
| `game_dt()` | Float seconds (dt_ms * 0.001). |
| `game_should_quit()` | 1 if user closed window or set quit flag. |
| `game_end()` | Close window, free platform resources. |

`fps <= 0` defaults to 60 FPS (16 ms frame time). The frame budget
is enforced by `draw_end()` which sleeps to pad out the remainder —
you do NOT need to call `usleep` manually.

### §34.2 — Delta time discipline

Always scale per-frame motion by `dt_ms` (or `dt`) so the game runs
the same speed regardless of frame rate:

```c
// WRONG — speed depends on FPS
player_x = player_x + 3;

// RIGHT — 120 pixels per second at any FPS
player_x = player_x + 120 * game_dt_ms() / 1000;
```

`game_dt_ms()` is clamped to 1..50 ms so:
- A stalled frame (debugger pause, heavy GC) doesn't teleport the
  player across the screen.
- A too-fast frame (paused → resumed) doesn't freeze motion.

For physics simulations needing finer time steps, divide dt into
sub-steps yourself inside your update loop.

### §34.3 — `game_run` — the maestro wrapper

For programs that use the `bpp_maestro` pattern (init / solo / base
/ render callbacks — see Cap 22 / 24), `game_run` bundles
`game_init` + `maestro_run` + `game_end`:

```c
main() {
    maestro_register_init(fn_ptr(my_init));
    maestro_register_solo(fn_ptr(my_solo));
    maestro_register_base(fn_ptr(my_base));
    maestro_register_render(fn_ptr(my_render));
    maestro_register_quit(fn_ptr(my_quit));
    game_run(640, 360, "My Game", 60);
    return 0;
}

my_solo(dt) {
    game_frame_begin();
    if (game_should_quit()) { maestro_quit(); }
    // ... per-frame solo-phase work ...
}
```

Use `game_run` when you want maestro's phase lattice (solo / base
workers / render on main). Use the plain `game_init` + manual loop
when you don't.

### §34.4 — Scene manager

The scene manager ships in stbgame (absorbed from the older
`stbscene.bsm`) because most games want scenes AND the game loop
together.

A scene is four function pointers:

```
load()     — called once when entered. Allocate resources.
update(dt) — per-frame logic. dt in milliseconds.
draw()     — per-frame render. Reads world state.
unload()   — called once when left. Free what load allocated.
```

Any of the four may be `0` (null) if the scene has nothing to do
at that phase. A simple MENU scene typically passes 0 for load /
unload — it has no heap state to manage.

**Lifecycle:**

```c
#define SCENE_MENU 0
#define SCENE_GAME 1

main() {
    game_init(640, 360, "My Game", 60);
    init_scene();

    scene_register(SCENE_MENU, 0,
                   fn_ptr(menu_update), fn_ptr(menu_draw), 0);
    scene_register(SCENE_GAME,
                   fn_ptr(game_load), fn_ptr(game_update),
                   fn_ptr(game_draw), fn_ptr(game_unload));
    scene_switch(SCENE_MENU);

    while (game_should_quit() == 0) {
        game_frame_begin();
        scene_tick(game_dt_ms());
        scene_draw();
        draw_end();
    }
    scene_shutdown();
    game_end();
    return 0;
}
```

To transition, call `scene_switch(new_id)` from anywhere — even
from inside an update or draw handler. The switch is **deferred to
the next `scene_tick`**, so mid-frame state isn't corrupted:

```c
menu_update(dt_ms) {
    if (key_pressed(KEY_ENTER)) {
        scene_switch(SCENE_GAME);   // safe — fires next frame
    }
}
```

The scene pool is capped at 16 entries (`_SCENE_MAX`). Bigger RPGs
can bump it — adjust the constant and rebuild.

### §34.5 — Frame lifecycle, annotated

```
game_frame_begin()
  ├ On first frame: _stb_poll_events() to seed input.
  │   (Every subsequent frame, input was polled at the end of
  │    the PREVIOUS draw_end(), so it's as fresh as possible.)
  ├ Compute dt_ms = now - _stb_last_time, clamp to 1..50.
  ├ Reset UI arena — all gui_* widgets from last frame freed.
  └ [user code runs here — update world, handle input]

draw_end()
  ├ _stb_present() — blit framebuffer to window.
  ├ _stb_frame_wait() — sleep to pad out _stb_frame_ms budget.
  ├ _stb_poll_events() — catch up on input BEFORE next frame.
  └ [next frame begins]
```

The key insight: input polling happens **between** frames, not at
the top of each frame. This gives the game code the most recent
possible input state when it reads. You don't need to think about
it — just know that `game_frame_begin` + `draw_end` wrap the loop
correctly.

### §34.6 — What this chapter does NOT cover

- **Multi-window games**. One window per process today (Cap 33).
- **Background music mixing**. That's `stbmixer` (Cap 30) — stbgame
  doesn't auto-open the audio device. Call `audio_open` explicitly
  if you want sound.
- **Save/load checkpoint**. Out of scope. `bpp_file.bsm` +
  `bpp_json.bsm` are the building blocks for persistence.
- **Multiplayer networking**. No networking in stbgame. The
  sockets syscalls exist (Cap 18 / Wave 18), but a game-ready
  networking module would be its own `stbnet`.

---

## Cap 35 — Drawing (stbdraw)

*Depends on: Cap 34 (stbgame)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbdraw.bsm` is the 2D software rasterizer — every `draw_*`, `clear`,
`layer_*`, and text rendering call lives here. It writes into the
framebuffer `_stb_fb` (RGBA, `_stb_w * _stb_h` pixels), which the
platform layer blits to the window each frame via `_stb_present`.

### §35.1 — Color and framebuffer

Colors are 32-bit packed ARGB integers (alpha high, blue low). Every
`draw_*` call takes one color arg.

```c
rgba(r, g, b, a);                // build a color from 0..255 channels
clear(color);                    // fill the whole framebuffer
```

Constants available: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`,
`CYAN`, `MAGENTA`, `GRAY`, `PURPLE`, `ORANGE`, plus a full 16-color
palette declared in `init_color()` (called by `game_init`).

### §35.2 — Shapes

```c
void draw_rect(x, y, w, h, color);
void draw_rect_v(pos, w, h, color);   // pos as packed (x<<32)|y (future)
void draw_circle(cx, cy, r, color);
void draw_line(x1, y1, x2, y2, color);
void draw_border(x, y, w, h, color, thickness);
void draw_rounded_rect(x, y, w, h, r, color);
void draw_gradient(x, y, w, h, c_top, c_bot);
void draw_shadow(x, y, w, h, intensity);
void draw_shadow_rounded(x, y, w, h, r, intensity);
void draw_bar(x, y, w, h, val, max_val, color);   // HP bar style
```

Bounds are clipped — writes outside `_stb_w / _stb_h` are silently
dropped, so games don't have to guard every call.

### §35.3 — Sprites

```c
void draw_sprite(data, x, y, w, h);          // raw ARGB buffer
void draw_sprite16(spr, pal, x0, y0);        // 16-color indexed sprite
```

`draw_sprite` blits an RGBA buffer; `draw_sprite16` takes a palette
(see Cap 38) and an indexed sprite — each byte is a 4-bit palette
index. Transparent pixels (index 0) are preserved.

### §35.4 — Text

```c
void draw_text(text, x, y, sz, color);
void draw_number(x, y, sz, color, val);
```

Uses an 8x8 bitmap font baked into `_font_table`. `sz` is a pixel
multiplier (1 = 8x8 per glyph, 2 = 16x16, etc.). Text clips to
framebuffer bounds. Each character is rendered as a scaled box of
set/clear pixels.

`draw_text` uses `render_measure(text, sz)` (Cap 47) to pre-compute
widths when needed.

### §35.5 — Draw lifecycle

```c
void draw_begin();   // currently a no-op (reserved for future batching)
void draw_end();     // present + sleep for frame budget + poll input
```

`draw_end` is the loop terminator — it presents the framebuffer to
the window, waits out the frame budget (`_stb_frame_ms`), and polls
the next frame's input. Don't call `_stb_poll_events` manually —
`draw_end` handles it.

### §35.6 — Layers (indexed byte grids)

For palette-indexed games (Captain Buddy, pathfind), layers hold
8-bit indices that compose via a final palette. Faster than RGBA
for NES-era visuals.

```c
layer_new(w, h, blend);                       // BLEND_NORMAL or BLEND_MULTIPLY
void layer_free(layer);
void layer_set_alpha(layer, a);               // 0..255
void layer_set_visible(layer, v);
void layer_set_blend(layer, mode);
layer_get(layer, x, y);                       // byte at (x,y) or 0 OOB
void layer_set(layer, x, y, idx);             // write byte, OOB-safe
void layer_clear(layer, idx);                 // fill with one index
void layer_fill(layer, x, y, idx);            // 4-connected flood fill
void layer_composite_indexed(dst, dw, dh,
                             layers, n,
                             palette);        // composite N layers
```

Typical use: game entities draw into one layer, background into
another, compose both into the main RGBA framebuffer via
`layer_composite_indexed` once per frame.

### §35.7 — TrueType rasterizer

For custom fonts beyond the baked 8x8 table, stbdraw includes a
runtime TTF rasterizer. Load a .ttf buffer, build a glyph atlas at a
target pixel size, blit glyphs by codepoint.

The rasterizer is ~400 lines of scanline-based edge rasterization
with 4x vertical oversampling — matches quality that game fonts
expect, much slower than bitmap fonts but run once at atlas-build
time. Game loop reads the atlas, not the rasterizer.

### §35.8 — What this chapter does NOT cover

- **GPU rendering** — `draw_*` is CPU-only. GPU primitives live in
  `stbrender.bsm` (Cap 47).
- **Image loading** — PNGs/sprites come from `stbimage.bsm` (Cap 37)
  or `stbasset.bsm` (Cap 45).
- **Animation** — `stbforge` owns animation playback (Cap 46).

---

## Cap 36 — UI widgets (stbui)

*Depends on: Cap 35 (stbdraw)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbui.bsm` is an immediate-mode UI toolkit — widgets compute
geometry, dispatch input, and draw themselves in a single function
call. No retained tree, no register/deregister, no IDs beyond what
your layout provides.

### §36.1 — Immediate-mode philosophy

Every frame, you declare the UI you want:

```c
game_frame_begin();
// ... game logic ...
if (gui_button(100, 50, 80, 24, "Save")) { save_game(); }
if (gui_button(100, 80, 80, 24, "Quit")) { quit = 1; }
gui_label(10, 10, "Score: ", score);
draw_end();
```

No state persists between frames unless you store it. The widget
returns 1 the frame it's clicked, 0 otherwise. This means your game
logic flows top-to-bottom every frame, free of callbacks.

The UI arena (32 KiB per-frame, wraps `bpp_arena`) holds transient
allocations — style overrides, layout frames, hover-fade states —
and resets every frame. A leaking widget only leaks for one frame.

### §36.2 — Themes

```c
void theme_dark();        // default — text light, bg dark
void theme_light();       // text dark, bg light
void theme_retro();       // NES-palette-inspired
void theme_set(theme);    // custom theme pointer
```

A theme is a `Theme` struct with fields for background, surface,
text, accent, button_bg, and corner radii. Widgets reference the
active theme — changing themes mid-frame updates every subsequent
widget.

### §36.3 — Basic widgets

```c
gui_button(x, y, w, h, label);        // → 1 on click, 0 otherwise
gui_label(x, y, text, ...);           // static text
gui_panel(x, y, w, h);                // framed card backdrop
gui_line(x1, y1, x2, y2);             // decorative rule
gui_progress(x, y, w, h, val, max);
gui_checkbox(x, y, label, checked);   // → new checked state
gui_slider(x, y, w, val, min, max);   // → new val
gui_dropdown(x, y, w, labels, n, selected);  // → new index
gui_text_input(x, y, w, buf, max);   // → new length
```

All widgets are stateless from the UI's perspective — you pass in
the current state and get the new state back. The widget internals
handle hover animations, click detection, and drawing.

### §36.4 — Layout helpers

Absolute positioning gets tedious for multi-row forms. The layout
stack provides a minimal flex-style system:

```c
layout_begin(x, y, w, h, direction);   // LAYOUT_ROW or LAYOUT_COL
  layout_push(width);                   // allocate space for next widget
  gui_button(0, 0, 0, 0, "A");         // uses current layout slot
  layout_push(width);
  gui_button(0, 0, 0, 0, "B");
layout_end();
```

Stack depth is 8 (fixed). Nested row-inside-column compositions
handle most game HUDs. For complex layouts (popups with arbitrary
positioning), fall back to absolute coordinates.

### §36.5 — Hover and press state

Cross-frame state is keyed by a `(x << 16) | y` hash of the widget's
top-left corner. The same button at the same position is recognized
between frames without explicit IDs. Collision is possible but rare
(two widgets at identical coordinates would share state).

The hover fade takes ~120 ms to complete — if the mouse leaves and
returns within the fade window, the animation resumes smoothly
instead of snapping.

### §36.6 — What this chapter does NOT cover

- **Custom widget authoring** — the public API covers the shipped
  widgets. Adding a new one requires understanding the theme +
  layout internals (source reference: lines 700+).
- **Accessibility** (screen readers, keyboard-only nav). Minimal
  keyboard support via `gui_text_input`; no broader a11y framework.
- **Retained-mode UI** — this is strictly immediate. If you want a
  retained tree, build your own on top.

---

## Cap 37 — Image decode (stbimage)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbimage.bsm` decodes image files to RGBA pixel buffers. Today:
**PNG only** (RIFF-style chunked format, zlib DEFLATE compressed).
Other formats (JPG, BMP, TGA) would land as sibling modules
(`stbjpg`, `stbbmp`) rather than extending stbimage.

### §37.1 — Public API

```c
img_load(path) : io;   // returns pixel buffer or 0
img_w();               // width of last loaded image
img_h();               // height of last loaded image
void img_free(pixels); // release a buffer from img_load
```

State is singleton: the last loaded image's dimensions live in
`img_w()` / `img_h()`. Typical pattern:

```c
auto pixels, w, h;
pixels = img_load("assets/player.png");
if (pixels == 0) { return 1; }   // load failed
w = img_w();
h = img_h();
// ... use pixels (w*h*4 bytes of RGBA) ...
img_free(pixels);
```

### §37.2 — What's supported

- **Color types**: RGB (3-byte), RGBA (4-byte), grayscale (1-byte
  widened to RGB), grayscale+alpha, indexed (via PLTE + tRNS).
- **Bit depths**: 8 per channel (16-bit PNG is clipped to 8).
- **Filters**: all five PNG filters (None / Sub / Up / Average / Paeth).
- **Interlacing**: NOT supported. Adam7-interlaced PNGs return 0 with a
  stderr warning. Typical asset pipelines (Aseprite, Figma export) don't
  interlace — it's a feature for progressive-download web images.
- **tRNS chunk**: palette alpha is honored; RGB-key transparency is
  discarded (fully-opaque output).

### §37.3 — Internal structure

Three layered decoders:
1. **Bit reader** (`_zr_read`) — DEFLATE bit-level unpacking, 1 bit
   at a time into a 32-bit window.
2. **Huffman decoder** (`_zh_build`, `_inf_decode`) — canonical
   Huffman tables reconstructed per deflate block.
3. **PNG chunk scanner** (`img_load` body) — walks IHDR, PLTE, IDAT,
   tRNS, IEND. Non-critical chunks (tEXt, gAMA, sRGB) are skipped.

The zlib / DEFLATE layer is a faithful port of `stb_image.h`'s
implementation. ~400 lines, no compromises on correctness — passes
every shipped test PNG including 3-byte RGB, 4-byte RGBA, indexed
palette, and grayscale+alpha variants.

### §37.4 — Performance notes

PNG decode is NOT hot path. Games call `img_load` once per asset
at boot, the decoded pixels live in memory for the session. Decode
time for a 256×256 sprite: ~5-10 ms on Apple Silicon, mostly
Huffman + filter work. If the pipeline ever needs faster decode,
candidates are (a) SIMD filters for large images, (b) single-pass
Huffman for tiny textures.

### §37.5 — What this chapter does NOT cover

- **Writing PNGs**. `stbimage_write.bsm` (if ever shipped) would live
  next door. Today, use `sound_save_wav` for audio and raw RGBA buffer
  writes for pixel captures.
- **Animated formats** (APNG, GIF). Out of scope — use per-frame PNGs.
- **Streaming decode** (for textures > RAM). Out of scope — assume
  images fit.

---

## Cap 38 — Palette (stbpal)

*Depends on: Cap 35 (stbdraw)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbpal.bsm` is the palette type plus a catalog of named built-in
palettes. Games that draw indexed sprites (like Captain Buddy, the
NES-era mode) pick a palette at boot and reference colors by index
thereafter — 4-bit or 8-bit indices replace 32-bit ARGB everywhere.

### §38.1 — The Palette type

```c
struct Palette { count, data }   // count entries, each 8-byte ARGB
```

Each entry is a full 32-bit ARGB color. Index 0 is conventionally
transparent (alpha = 0).

```c
palette_new(n);                       // allocate zero-filled palette
void palette_free(pal);
palette_clone(pal);                   // deep copy
palette_count(pal);                   // number of entries
palette_get(pal, i);                  // read color at index i (checked)
void palette_set(pal, i, argb);       // write color at index i
palette_data(pal);                    // raw data pointer (hot loops)
```

### §38.2 — Built-in catalog

Eight presets, accessed by name:

| Name | Size | Purpose |
|------|------|---------|
| `"mcu-8"` | 8 | ModuLab default — vivid Nintendo-era spread |
| `"nkotc-4"` | 4 | NES minimalist — 3 colors + transparent |
| `"cb-32"` | 32 | Captain Buddy — 4-row grid (gray/red/green/blue) |
| `"gameboy-4"` | 4 | Original Game Boy shades |
| `"pico8-16"` | 16 | PICO-8 fantasy console |
| `"c64-16"` | 16 | Commodore 64 spread |
| `"ega-16"` | 16 | EGA PC palette |
| `"cga-4"` | 4 | CGA magenta/cyan/white/black |

```c
palette_builtin(name);      // → Palette pointer (clone it if mutating)
void init_palette();        // seeds the catalog (called by game_init)
```

The catalog is shared — `palette_builtin("mcu-8")` returns the same
pointer across calls. To mutate one slot without affecting other
callers, `palette_clone` first.

### §38.3 — Palette cycling (animated shimmer)

Classic NES-era effect: rotate a subset of palette entries to
create water, fire, lava animations without re-rendering sprites.

```c
palette_cycle_begin(pal, start, count, period_ms);
void palette_cycle_stop(pal, start);
void palette_cycle_tick(pal, dt_ms);   // call once per frame
```

A cycle shifts `count` entries starting at `start` by one slot every
`period_ms / count` frames. Looks like flowing water at 400 ms
period with 4 entries. Multiple cycles can run simultaneously on
different slot ranges.

### §38.4 — Palette LUTs (global swap)

A `PaletteLUT` remaps indices — useful for damage flashes (every
pixel → white for 2 frames), day/night fades, or character-specific
color shifts.

```c
palette_lut_new(count);                     // identity map
void palette_lut_free(lut);
void palette_lut_set(lut, from_idx, to_idx);
palette_lut_flash(count, to_idx);           // all → to_idx
void palette_lut_apply(src, lut, dst);      // dst[i] = src[lut[i]]
```

### §38.5 — Palette interpolation

```c
palette_lerp(a, b, t, out);   // out[i] = lerp(a[i], b[i], t), t=0..255
```

Per-channel linear interpolation. Useful for smooth palette
transitions (level changes, time of day).

### §38.6 — Save / load

```c
palette_save(pal, path, name) : io;   // JSON format, human-editable
palette_load(path) : io;              // → Palette pointer or 0
```

The JSON format stores each entry as a hex string (`"#FF2020"`). Uses
`bpp_json` writer + parser underneath (Phase D dogfood — no hand-
rolled JSON structure anywhere in stb).

### §38.7 — What this chapter does NOT cover

- **GPU-side palette upload** — that's `stbrender` (Cap 47) territory.
  The `palette_data` pointer lands directly in a uniform buffer.
- **Dithering** between palettes. Out of scope.
- **Automatic palette reduction** (color quantization from RGBA →
  indexed). Pre-process sprites offline.

---

## Cap 39 — Tilemap (stbtile)

*Depends on: Cap 38 (stbpal), Cap 42 (stbpath)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbtile.bsm` is the indexed-byte tilemap — each cell stores a
single-byte tile index (0..255), which looks up into a tileset
(sprite atlas) and optional solid-mask table. The backing buffer
is dense (1 byte per cell), so a 128×128 tilemap is 16 KiB.

### §39.1 — The Tilemap type

```c
struct Tilemap { w, h, tw, th, data, solid_mask, tileset, tile_count, remap }
```

`w × h` cells of `tw × th` pixels each. `data` is the tile index
grid. `solid_mask` is a 256-entry byte array marking which tile
indices are walls (for physics queries). `tileset` is the decoded
sprite atlas (from `stbimage`). `remap` translates game-logic tile
types into tileset sprite indices — decouples code from artwork.

### §39.2 — Creating and populating

```c
tile_new(w, h, tw, th);             // empty map, all cells = 0
void tile_free(tm);
void tile_set(tm, x, y, idx);       // write a cell
tile_get(tm, x, y);                 // read a cell (0 OOB)
void tile_load_tileset(tm, path) : io;   // load PNG + auto-slice
void tile_set_solid(tm, idx, is_solid);
tile_is_solid(tm, idx);
```

`tile_load_tileset` takes a PNG arranged in a `tw × th` grid (e.g.,
a 16×16-sprite atlas of 8×8 cells = 128×128 PNG). It slices into
64 sprite frames automatically.

### §39.3 — Rendering

```c
void tile_draw(tm, cam_x, cam_y);   // blit visible cells
```

`tile_draw` computes which cells overlap the viewport (given
camera offset), then draws each via indexed sprite blit. Only
visible cells are rendered — a 1024×1024 map at 8×8 per cell
with a 320×180 viewport draws ~1000 cells, not a million.

### §39.4 — Physics integration

The `solid_mask` array feeds `stbphys` (Cap 41) for collision.

```c
if (tile_is_solid(tm, tile_get(tm, gx, gy))) { block_movement(); }
```

A tile index of 3 could be "grass" (not solid) while 4 is "wall"
(solid). Solid mask flips independently of the rendering — the same
tileset art can be recycled with different collision semantics.

### §39.5 — Save / load

```c
void tile_save(tm, path) : io;       // JSON
tile_load(path) : io;                 // → Tilemap
```

JSON format stores dimensions, tile size, and the raw byte grid
(base64-ish hex). Small enough for git to diff cleanly. Level
editors (ModuLab) read/write this.

### §39.6 — What this chapter does NOT cover

- **Infinite worlds / chunks** — tilemaps are bounded. For
  streaming terrain, split into multiple Tilemaps and load on
  demand.
- **Multi-layer tilemaps** (separate foreground / background /
  decoration) — compose by creating N tilemaps and drawing them
  in order. The module doesn't wrap that for you.
- **Auto-tiling** (bitmask-based tile selection from neighbors).
  Tilemap editors handle this at authoring time; no runtime support.

---

## Cap 40 — ECS (stbecs)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbecs.bsm` is a minimal entity-component system — fixed-capacity,
structure-of-arrays (SoA), parallel-array layout. No queries, no
iteration kernels, no bitmask masks per component. Just flat arrays
indexed by entity ID.

### §40.1 — The World type

```c
struct World {
    capacity, count,          // max entities, current live count
    alive,                    // 1 byte per ID: 0 = dead, 1 = live
    free_ids, free_count,     // reusable ID stack
    pos_x, pos_y,             // position components (all entities have these)
    vel_x, vel_y,             // velocity
    flags                     // per-entity bit flags
}
```

All arrays are `capacity`-sized. Dead slots stay allocated, just
flagged `alive[id] = 0`.

### §40.2 — Lifecycle

```c
ecs_new(capacity);            // create world with N entity slots
void ecs_free(w);
ecs_spawn(w);                 // → new entity ID, or -1 if full
void ecs_despawn(w, id);      // mark dead, recycle ID
ecs_alive(w, id);             // 1 or 0
```

ID recycling uses a LIFO stack — `despawn` pushes the freed ID back,
`spawn` pops the most recent. Gives good cache locality when many
entities come and go.

### §40.3 — Component access

No getters/setters — just index the arrays:

```c
w.pos_x[id] = 100;
w.vel_y[id] = -5;
// flags use bit ops
w.flags[id] = w.flags[id] | FLAG_JUMPING;
```

Raw array access is FAST — single load/store, no function call,
no bounds check in release mode. The tradeoff: you must validate
IDs yourself where they come from untrusted sources (save games,
network messages). Inside the game loop, loops over `0..count`
with alive checks are safe by construction.

### §40.4 — Typical game loop usage

```c
// Physics pass: update position from velocity.
auto id;
for (id = 0; id < w.count; id = id + 1) {
    if (peek(w.alive + id) == 0) { continue; }
    w.pos_x[id] = w.pos_x[id] + w.vel_x[id];
    w.pos_y[id] = w.pos_y[id] + w.vel_y[id];
}

// Render pass: draw each live entity.
for (id = 0; id < w.count; id = id + 1) {
    if (peek(w.alive + id) == 0) { continue; }
    draw_sprite(...);
}
```

The SoA layout is cache-friendly: the position loop touches only
pos_x + pos_y arrays, never pulling velocity/flags into cache. At
10k entities, this is measurably faster than AoS struct-per-entity.

### §40.5 — Adding components

No dynamic component registration. To add a new field (say, `hp`),
edit the `World` struct + allocation + init pattern. This is
deliberately static — matches B++'s "no reflection, no dynamic
layout" philosophy.

For variable-per-entity data (inventory, dialogue choice history),
store an index into a side table: `w.inventory_idx[id]` points into
a global `inventory_pool`.

### §40.6 — What this chapter does NOT cover

- **Queries / filters** (give me all entities with FLAG_ENEMY and
  health < 10). Loop manually with the flag check.
- **Parallel iteration** across components (SIMD over pos_x array).
  Use `vec_*` intrinsics (Cap 23 / Wave 18) if your loop is
  profile-hot.
- **Archetypes** (grouping entities by component set). Not shipped —
  flat arrays for everyone.

---

## Cap 41 — Physics (stbphys)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbphys.bsm` is a minimalist AABB physics integrator. Point-on-tile
collision via `tile_is_solid`, gravity as a constant, velocity +
acceleration integrated per frame. Think Celeste-style platformer
physics, not Box2D.

### §41.1 — The Body type

```c
struct Body {
    x, y,           // position (center, integer pixels)
    vx, vy,         // velocity (pixels per second, integer)
    w, h,           // collision box (width, height)
    grounded,       // 1 if standing on solid this frame
    facing          // -1 or 1, last non-zero horizontal input
}
```

### §41.2 — API

```c
body_new(x, y, w, h);         // allocate a Body at (x,y) with bbox
void body_free(body);
void body_step(body, tm, dt_ms);   // one physics tick
```

`body_step` does:
1. Apply gravity: `vy += gravity * dt`
2. Apply drag: `vx *= drag_factor` (integer approximation)
3. Integrate position: `x += vx * dt`, `y += vy * dt`
4. Resolve collision against the tilemap (checks 4 corners + edges)
5. Update `grounded` flag based on downward collision

### §41.3 — Tuning constants

Globals set by `init_phys`:
- `_phys_gravity` — 800 (px/s²)
- `_phys_max_speed` — 600 (px/s horizontal cap)
- `_phys_jump_velocity` — -350 (initial vy on jump)
- `_phys_drag_ground` — 220/256
- `_phys_drag_air` — 245/256

Override before `init_phys` or tweak directly for per-character feel.

### §41.4 — Typical platformer loop

```c
body_step(player, tilemap, dt);
if (key_down(KEY_LEFT))  { player.vx = -200; }
if (key_down(KEY_RIGHT)) { player.vx = 200; }
if (key_pressed(KEY_SPACE) && player.grounded) {
    player.vy = _phys_jump_velocity;
}
```

### §41.5 — What this chapter does NOT cover

- **Rotation** (bodies are axis-aligned rectangles only).
- **Joints / constraints** (pin, spring, etc.). Not shipped — static
  physics.
- **Dynamic vs kinematic split** — every body is dynamic.
- **Continuous collision** (tunneling prevention for high-velocity
  small objects). Use `body_step_sub(n)` to divide into sub-steps
  manually if needed.

---

## Cap 42 — Pathfinding (stbpath)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbpath.bsm` is an A* pathfinder on grid maps. 4-connected
neighborhood (up/down/left/right), Manhattan heuristic, unit edge
costs. Optimized for small-to-medium grids (up to ~256×256);
beyond that, flow fields or HPA* would beat it.

### §42.1 — The PathFinder type

```c
struct PathFinder {
    w, h,                     // grid dimensions
    cells,                    // byte grid: 0 = walkable, 1+ = blocked
    g_score, f_score,         // A* scores per cell
    came_from,                // parent cell for path reconstruction
    heap, heap_pos, heap_cnt, // open-set binary heap
    closed                    // closed-set bitmask
}
```

One PathFinder per search context — reuse across queries (same
dimensions) to avoid re-allocation.

### §42.2 — API

```c
path_new(w, h);                               // create finder
void path_free(pf);
void path_set_blocked(pf, gx, gy, blocked);
path_is_blocked(pf, gx, gy);
path_find(pf, sx, sy, tx, ty, out_xs, out_ys, max_len);   // → path length
```

`path_find` returns the path length (number of cells) or 0 if no
path. The cells are written to `out_xs[0..len-1]` and
`out_ys[0..len-1]`. Order: start → ... → target.

`max_len` is the caller-supplied output buffer size. Paths longer
than `max_len` truncate — cheap safeguard against infinite
backtracking bugs.

### §42.3 — Heuristic and constants

```c
const PATH_INF = 2000000000;   // represents unreachable
```

Manhattan distance: `|dx| + |dy|`. Admissible + consistent for
4-connected unit-cost grids.

### §42.4 — Performance

A 100×100 grid with ~50% obstacles computes a full cross-map path
in ~2-5 ms on Apple Silicon. Dominated by heap operations. The
heap is a classic binary-heap priority queue with decrease-key
via an inverse map (`heap_pos`).

For very large maps or real-time reactive pathing, consider:
- **Reusing scores** across queries when obstacles don't change
- **Jump-point search** (JPS) variant — 8-connected only, not
  shipped
- **Flow fields** for many-to-one targeting (hordes chasing player)

### §42.5 — What this chapter does NOT cover

- **8-connected movement / diagonals** — 4-connected only. For
  diagonals, add 4 neighbor offsets + adjust heuristic to
  Chebyshev distance.
- **Variable costs** (sand costs 2, water costs 5). Expand
  `g_score` delta per cell type.
- **Multi-agent pathing** (cooperative, traffic-avoidant). Out of
  scope — compute per-agent serially.

---

## Cap 43 — Pool allocator (stbpool)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbpool.bsm` is a fixed-element-size pool allocator. Pre-allocate
N slots of size B, hand out pointers from a free list, return
pointers by adding back to the free list. O(1) alloc/free, zero
fragmentation, trivial to reset.

### §43.1 — API

```c
pool_new(elem_size, capacity);
void pool_free(pool);
pool_alloc(pool);                  // → pointer to a slot, or 0 if full
void pool_release(pool, ptr);      // return a slot to the free list
pool_reset(pool);                  // return all slots
```

### §43.2 — Typical use

Bullet pools, particle systems, damage-number pop-ups. Anything
where:
- Element size is uniform
- Max concurrent count is known (or a reasonable cap exists)
- Allocation speed matters more than memory overhead

```c
auto pool;
pool = pool_new(sizeof(Bullet), 256);

// Shoot
auto b: Bullet;
b = pool_alloc(pool);
if (b != 0) { b.x = 0; b.y = 0; b.vx = 10; /* ... */ }

// Despawn (off-screen, expired)
pool_release(pool, b);
```

### §43.3 — Memory layout

A single malloc block holds `elem_size * capacity` bytes. The free
list is a pointer chain through the first 8 bytes of each free
slot — no separate index table, no header per slot.

### §43.4 — What this chapter does NOT cover

- **Variable-size allocations** — use `bpp_arena` for those.
- **Thread safety** — single-threaded. For SPSC, pair with a ring.
- **Generation counters** — no ABA protection on free-list. If a
  caller holds a stale pointer across a release + realloc, undefined
  behavior. Pair with `stbasset`-style generation handles if that
  matters.

---

## Cap 44 — Color math (stbcol)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbcol.bsm` is pure color math — no allocations, no state, just
functions. RGBA packing / unpacking, blending, HSV conversion.

### §44.1 — Packing

```c
rgba_of(r, g, b, a);       // pack 4 bytes into a single ARGB word
rgba_r(color);             // extract channel
rgba_g(color);
rgba_b(color);
rgba_a(color);
```

### §44.2 — Blending

```c
rgba_blend(dst, src);                 // standard over-compositing
rgba_lerp(a, b, t);                   // 0..255 interpolation
rgba_multiply(a, b);                  // modulation
rgba_mix(a, b, ratio_of_256);
```

`rgba_blend` implements the Porter-Duff `over` operator using the
src's alpha: `out = src.rgb + dst.rgb * (1 - src.a)`. Integer
arithmetic throughout.

### §44.3 — HSV

```c
hsv_to_rgba(h, s, v, a);        // hue 0..360, sat/val 0..255
rgba_to_hsv(color, out_h, out_s, out_v);
```

Useful for color pickers, hue shifting, rainbow effects.

### §44.4 — Predefined constants

```c
BLACK, WHITE, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, GRAY
PURPLE, ORANGE, PINK, LIME, NAVY, TEAL, MAROON
TRANSPARENT            // = 0
```

Declared in `init_color()` (called by `game_init`).

### §44.5 — What this chapter does NOT cover

- **sRGB gamma correction** — all math is linear in integer space.
- **Color space conversions** (LAB, XYZ, OKLCH). Out of scope.
- **HDR / wide-gamut** colors. 32-bit integer ARGB only.

---

## Cap 45 — Asset hub (stbasset)

*Depends on: Cap 37 (stbimage), Cap 31 (stbsound), Cap 38 (stbpal)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbasset.bsm` is the game-wide asset registry — sprites, sounds,
music, fonts are loaded once, assigned a handle, and looked up by
path. Duplicates are de-duped automatically; reloads are cheap.

### §45.1 — The AssetSlot type and handle shape

```c
struct AssetSlot { kind, generation, ptr, w, h, mtime }
```

Handles are packed 32-bit integers: `(generation << 16) | slot`.
The generation counter increments on reuse, so stale handles
from a replaced asset return 0 from lookups instead of aliasing.

```c
ASSET_NONE = 0;           // empty slot
ASSET_SPRITE = 1;         // GPU texture handle
ASSET_SOUND = 2;          // stereo s16 PCM buffer
ASSET_MUSIC = 3;          // looping BGM buffer
ASSET_FONT = 4;           // font atlas + metrics
```

### §45.2 — Public API

```c
void init_asset();                           // called by game_init
asset_load_sprite(path) : io;                // → handle or 0
asset_load_sound(path) : io;                 // → handle
asset_load_music(path) : io;                 // → handle
asset_load_font(path, size) : io;            // → handle
void asset_release(handle);                  // explicit cleanup

asset_sprite(handle);                        // → GPU texture
asset_sound_buf(handle);                     // → PCM buffer
asset_sound_frames(handle);                  // → frame count
asset_music_buf(handle);
asset_music_frames(handle);
asset_font(handle);
asset_w(handle); asset_h(handle);
```

Path dedup: `asset_load_sprite("assets/player.png")` called twice
returns the same handle — the internal `hash_str_new(512)` table
keeps path → slot mapping.

### §45.3 — Hot reload

```c
void asset_check_reload();   // call once per frame (dev builds)
```

Walks every loaded asset, compares file mtime against the recorded
mtime. Changed files get re-loaded in place — the sprite's GPU
texture gets re-uploaded, the sound's buffer swapped, etc. Handles
stay valid across reload (same slot, same generation).

This is the backbone of ModuLab's "edit sprite → see in game" loop.
Expect ~5-10 ms cost per frame with 256 assets — trivial for dev
builds, skip the call in release.

### §45.4 — What this chapter does NOT cover

- **Async loading** — every load blocks. For giant asset packs,
  split into initial + streaming tiers manually.
- **Compression / archive formats** (ZIP, custom bundles). Assets
  are loose files on disk.
- **Content hashing / integrity**. No checksum verification — a
  corrupt PNG trips `stbimage`'s decode error, other formats may
  misbehave silently.

---

## Cap 46 — Level forge (stbforge)

*Depends on: Cap 45 (stbasset)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbforge.bsm` is the level-editor and character-editor substrate
that powers ModuLab's testbed. Holds the data model for characters
(sprites + pivots + hitbox + animations + controls), records
gameplay for replay, and renders the testbed world.

### §46.1 — The Character type

```c
struct Character {
    sprite_path,              // PNG asset path
    pivot_x, pivot_y,
    hit_x, hit_y, hit_w, hit_h,
    gravity, max_speed, jump_velocity,
    ctrl_left, ctrl_right, ctrl_jump,   // KEY_* enum IDs
    animations, anim_count              // Animation array + length
}

struct Animation { name_ptr, frames, frame_count, fps, loop }
```

One Character per editable in-game actor. Animations each hold a
sequence of sprite frame indices + playback parameters.

### §46.2 — Public API

```c
character_new();
void character_free(cp);
void character_set_sprite(cp, path);
void character_set_pivot(cp, x, y);
void character_set_hitbox(cp, x, y, w, h);
void character_set_physics(cp, gravity, max_speed, jump_vel);
void character_set_control(cp, ctrl_left, ctrl_right, ctrl_jump);

character_add_animation(cp, name);          // → animation pointer
void character_set_frames(ap, frames, n);   // copy frames into animation
character_animation_count(cp);
character_animation_at(cp, idx);
```

### §46.3 — Save / load (JSON)

```c
character_save(cp, path) : io;     // → 0 on success, -1 on fail
character_load(path) : io;         // → Character or 0
```

JSON format via `bpp_json` writer (Phase D — no hand-rolled
serialization). Deterministic field order, line-oriented, git-
diffable.

### §46.4 — Testbed runtime

The testbed is the "play the character you just edited" loop.
Fixed 32×18 tile world with a ground plus three ledges — lets the
user verify gravity, jump height, collision in a repeatable scene.

```c
void testbed_init(character_ptr);
void testbed_tick(dt_ms);
void testbed_draw();
void testbed_end();
```

### §46.5 — Recorder / replay

For Rhythm Teacher and deterministic-testing flows, stbforge
records per-frame input + position so a level can be replayed
byte-identically.

```c
void rec_start();
void rec_tick(...);
void rec_stop();
rec_save(path);
rec_load(path);
```

### §46.6 — What this chapter does NOT cover

- **Animation preview UI** — stbforge holds the data, ModuLab does
  the editor UI (Cap 27 — Tools).
- **Tween / easing curves**. Frame-indexed animation only, no
  inter-frame interpolation.

---

## Cap 47 — GPU render (stbrender)

*Depends on: Cap 38 (stbpal)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbrender.bsm` is the GPU-side rendering layer — where supported
by the platform (Metal on macOS today; Vulkan on Linux deferred).
Enables indexed-sprite GPU upload, 3D-coordinate quads, and
per-frame vertex streaming for particle / UI systems that want to
skip the CPU rasterizer.

### §47.1 — Public API

```c
void render_init();
void render_begin();
void render_end();
void render_clear(color);
void render_rect(x, y, w, h, color);
void render_circle(cx, cy, r, color);
void render_line(x1, y1, x2, y2, color);
void render_circle_outline(cx, cy, r, color);
void render_rect_outline(x, y, w, h, color);
void render_sprite(handle, x, y, w, h);
void render_sprite_indexed(handle, pal, x, y, w, h);
void render_text(text, x, y, sz, color);
render_measure(text, sz);    // measure width in pixels
```

Parallel to `stbdraw`'s CPU API, but dispatched to GPU when
available. Fall back: if no GPU backend is installed,
`render_init` is a no-op and every `render_*` call routes to the
matching `draw_*` equivalent (so games compile and run either way).

### §47.2 — Indexed sprites on the GPU

The indexed rendering path uploads the palette once as a small
texture (256×1 ARGB), the sprite as an 8-bit texture, and the
shader samples `palette[sprite[uv]]` per fragment. This matches
how Doom64 / SNES emulators draw: one 8-bit fetch + one palette
lookup per pixel, zero CPU cost.

### §47.3 — What this chapter does NOT cover

- **Custom shaders / post-processing**. The current pipeline is
  fixed: textured quads with palette LUT. Post effects would be
  an extension.
- **3D rendering**. The coordinate system is 2D-only today.
- **Compute shaders**. Out of scope.
- **Batch rendering of many small sprites** (particle system). For
  < 1000 sprites per frame the per-call overhead is fine; beyond
  that, a proper instanced-quad pipeline would help.

---

# Appendices

## Appendix A — Compiler Flags Reference

*Source: legacy_docs/how_to_dev_b++.md Part 8*
*Status: PENDING*

## Appendix B — Recovery

*Source: legacy_docs/how_to_dev_b++.md Part 9*
*Status: PENDING*

---

# Meta — How this book is maintained

This book is living. Every change to the compiler, stdlib, or tooling updates its chapter in the same commit. Rules:

1. **A PR that ships new stdlib or backend logic MUST update its chapter.** Missing doc = CI should fail (today manual; eventually a gate).
2. **Depends on:** links are authoritative. If Cap 10 says "Depends on: Cap 4", and you change Cap 4 semantics, Cap 10 needs revisit. Grep `Depends on: Cap 4` to find them.
3. **Source: legacy_docs/X.md §Y** stays until the legacy section is marked ABSORBED in `legacy_docs/README.md`. Do not delete `legacy_docs/` until `grep -c PENDING\|DRAFT legacy_docs/README.md` returns 0.
4. **TOC at top of this file is the live index.** Status column drives the faxina progress. Run `grep -c PENDING docs/how_to_dev_b++.md` to see how many chapters remain.
5. **Code examples in every chapter should compile.** If they do not, fix the code OR the doc — never leave false examples.

When the book is complete: `legacy_docs/` is deletable, this file is the canonical reference, and any new chapter follows the same template.
