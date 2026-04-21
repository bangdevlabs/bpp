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
| 6 | Strings (bpp_str + sb_) | II — Arsenal | PENDING | legacy §4 | 4 |
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
    print_msg("score: ");
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
| `bpp_io.bsm` | stdout / stderr helpers | `print_msg`, `print_int`, `print_ln`, `putchar`, `putchar_err`, `getchar` |
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
*Source: legacy_docs/the_b++_programming_language.md §2*
*Status: PENDING*

---

## Cap 4 — Types

*Depends on: Cap 3*
*Source: legacy_docs/the_b++_programming_language.md §3*
*Status: PENDING — body to be absorbed from legacy §3 in Phase C*

Note on scope: this chapter documents TYPES AS LANGUAGE FEATURE —
the syntax `auto x: word`, `auto x: float`, `auto c: Character`, the
sub-word hints (`: half`, `: bit3`), and how they interact with the
effect lattice. The internal type-inference engine
(`src/bpp_types.bsm`) is compiler-only (fourth tier, see Cap 15) and
is documented in Cap 21 (Frontend) as part of the parser/analysis
pipeline. User programs interact with types via annotations; they
never import `bpp_types.bsm`.

---

## Cap 5 — Functions and Effects

*Depends on: Cap 3, Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §2.5*
*Status: PENDING*

---

# Part II — Arsenal

The stdlib. Every function listed in Cap 2's prelude table, expanded here with signature, semantics, and one example each.

---

## Cap 6 — Strings (bpp_str + sb_)

*Depends on: Cap 4*
*Source: legacy_docs/the_b++_programming_language.md §4 + new content for §6.0 (three shapes rationale)*
*Status: DRAFT — §6.0 complete; §6.1-§6.5 pending API expansion*

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

*PENDING — API reference with one example per function.*

### §6.2 — `sb_*` — Dynamic string builder

*PENDING — API reference with one example per function.*

### §6.3 — `str_peek` is a compiler builtin

*PENDING — document that `str_peek(s, i)` is emitted inline by codegen (a byte load at s+i), not a bpp_str function. Used interchangeably with `peek(s + i)` but carries pure-effect semantic for smart dispatch.*

### §6.4 — Packed refs for compiler writers

*PENDING — for developers hacking on the parser / codegen, document `buf_eq(a, b, len)`, `packed_eq(a, b)` in `bpp_internal.bsm`, and `unpack_s` / `unpack_l` to read the packed representation. User programs never import `bpp_internal` and never see this API.*

### §6.5 — Common patterns

*PENDING — snippets for the five most common string-handling idioms (file-path testing, score interpolation, log accumulation, parse-a-line, safe copy).*

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
