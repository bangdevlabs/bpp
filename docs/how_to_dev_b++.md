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
| 10 | Structs (bpp_struct) | II | PENDING | legacy §4.5 | 4 |
| 11 | Enums (bpp_enum) | II | PENDING | legacy §4.6 | 4 |
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
*Status: PENDING*

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

### The three tiers of HQ

The "general / battalion" mental model applies at THREE layers of B++, each with its own canonical location:

| Tier | General (HQ) | Battalion | Concrete example |
|------|--------------|-----------|------------------|
| **Data primitives** | `src/bpp_*.bsm` auto-injected (strings, arrays, hash, buf, struct, enum) | Every program, tool, game, stb module | `arr_push` canonical in `bpp_array.bsm` — consumers call it, do not reimplement |
| **Runtime functions** | `src/bpp_runtime.bsm` declares the ABI contract; `src/backend/os/<os>/_core_<os>.bsm` implements it | Consumers call `malloc(n)` as a normal function | `malloc`, `free`, `realloc`, `memcpy`, `memset` |
| **Inline builtins (codegen dispatch)** | `src/bpp_codegen.bsm` `cg_builtin_dispatch` | `src/backend/chip/<chip>/<chip>_primitives.bsm` | `peek`, `poke`, `str_peek`, `sys_write`, `argc_get` — dispatch in spine, final instruction per chip |

Three tiers, three canonical homes in `src/`. Every consumer (games, tools, stb, backends) downstream is a battalion.

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
*Source: legacy_docs/phase_backend_closeout.md*
*Status: PENDING*

---

## Cap 23 — Battalions (chip backends)

*Depends on: Cap 22*
*Source: legacy_docs/max_plan_codegen_split.md*
*Status: PENDING*

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
