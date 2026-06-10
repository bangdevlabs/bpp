# The B++ Programming Language — a learner's book in the K&R tradition

**Phase 0 — the mapping doc.** Before any prose is written, this enumerates
the real b++ language surface and maps Kernighan & Ritchie's *The C Programming
Language* (K&R) chapter-by-chapter onto b++: what transposes cleanly, what must
be rewritten, what gets replaced, and what is omitted. Same "enumerate the
source → map to the target → port point by point" funnel we use for the
Stratagus AI port (`wc2_ai_port.md`) and asset conversion (Tonify Rule 31),
applied to a textbook.

> **What this book is — and is not.** A *learner's* book for people who will
> write b++, in the spirit, structure, and exercise-driven pedagogy of K&R. It
> is an **original work in the K&R tradition**, not a copy: we emulate the
> progression and style, but every line of prose and every exercise is written
> from scratch for b++. K&R's own text and exercise wording are copyrighted and
> are never reproduced. We refer to K&R's chapter topics only to map them.

---

## Part A — Why this book, and the lineage hook

**The gap it fills.** The existing `docs/manual/` is *contributor* docs (how to
build the compiler, the Tonify rules, the cartridge map). There is no "learn
b++ from zero" book aimed at someone who wants to *program in the language*.
This is the onboarding rail that does not exist yet.

**Three payoffs, not one.**
1. **Onboarding** — the missing learner-facing book.
2. **A living conformance corpus** — every example and every exercise solution
   is a real `.bpp` that must compile and run, wired into `tests/`. K&R-scale
   that is dozens of examples plus a large exercise set: a regression suite that
   grows with the book.
3. **A design driver** — wherever a K&R idiom has no clean b++ equivalent, we
   have located a rough edge. The book surfaces language warts the way the AI
   port surfaced the gather-contention bug.

**The lineage hook (the book's identity).** B (Ken Thompson, 1969, from BCPL)
was *typeless*: the only data type was the machine word; pointers and integers
were interchangeable. K&R's C is the language Ritchie built by *adding types to
B*. b++'s default model — `auto x` is a 64-bit machine word, with optional
*smart* type hints layered on — is essentially **B's word model revived**, with
modern inference on top. So a "K&R for b++" teaches a language that sits
historically *between B and C*: it keeps B's typeless substrate and adds C-like
(but optional, weak) typing. Working title in the tradition: **The B++
Programming Language**.

---

## Part B — The real b++ language surface (verified against source)

The facts the mapping rests on, read out of the compiler (not assumed):

**Control flow** (`bpp_parser.bsm`): `if` / `else` / `else if`; `for`; `while`;
`switch`; `break`; `continue`; `return`. **No** `goto`, **no** `do…while`.
- `switch` is *not* C's switch: no fall-through, no `case`/`default`/`break`
  keywords. Two forms — **value dispatch** `switch (x) { 1,2 { … } else { … } }`
  (comma-separated values per arm, `else` is the default) and **condition
  dispatch** `switch { cond { … } else { … } }` (cond/guard chain). The
  compiler emits **W021** if there is no `else` arm (exhaustiveness hint). This
  is a genuine b++-is-safer divergence worth a whole teaching beat.

**Type / declaration keywords** (`bpp_lexer.bsm`): `auto`, `int`, `char`,
`byte`, `half`, `long`, `void`, `ptr`, `var`, `bit`, `enum`, `struct`,
`sizeof`. **No** `union`, **no** `typedef`. Primitive width/kind is expressed as
a per-declarator hint (`: float`, `: bit`, `: ptr`, …); default is the word.

**Storage class / linkage** (`bpp_parser.bsm`, `GLOB_*`): `static`, `global`,
`const`, `extrn` (+ `global const` / `static const`). Enforced at compile time —
**E263** (write to const slot), **E264** (extrn with no backing definition).

**I/O**: `put("…")` with smart dispatch (`bpp_io.bsm` auto-injected) rewrites
`put(x)` → `putstr`/`putnum`/`putfloat` by inferred type. Files via
`file_read_all` / `file_write_all` / `file_stat`; errors via `putstr_err`. No
`printf` format strings.

**Hello world** (`examples/hello_world.bpp`):
```
main() {
    put("Hello, World\n");
}
```

**Command-line args** (`bpp_args.bsm`): `argc_get()` / argv accessors over
`_bpp_argc` / `_bpp_argv` — the K&R `argv` exercises have a real target.

**Memory / collections**: `malloc`/`realloc`/`free`, `buf_byte`/`buf_word`
(raw), `arr_*` (8-byte word arrays, shadow header), `arr_struct_*` (growable
fixed-size struct arrays via `elem_size` erasure). `sizeof` exists.

**Modules**: `import` / load (no C preprocessor — `#include`/`#define` have no
direct analog; `const X = N;` is parser-level literal substitution).

**Functions / annotations**: `func`, `stub`, return-type hint `-> word`,
`fn_ptr(name)` for function values; `@safe` / `@base` / `@profile` annotations.

---

## Part C — Chapter-by-chapter mapping

Legend: ✅ transposes · 🔄 rewrite (concept differs) · 🔁 replace (b++ has its
own mechanism) · 💪 b++ goes *deeper* than K&R · ⛔ omit (no analog).

| K&R chapter (topic) | b++ status | Plan |
|---|---|---|
| **1. A Tutorial Introduction** | ✅ | hello via `put`; variables `auto`; loops `for`/`while`; symbolic constants `const`; char input/arrays; functions & arguments; scope. The "feel" transposes; swap `printf` → `put`. |
| **2. Types, Operators, Expressions** | 🔄 | The biggest rewrite and the most distinctive chapter. Teach the **word default + optional hints** (`: int`/`: char`/`: float`/`: half`/`: bit`/`: ptr`), the smart-vs-weak model, and the B→C→b++ hook here. C's promotion/cast rules become "what the word model does + where a hint changes codegen." |
| **3. Control Flow** | ✅ + 🔄 | `if`/`else`/`for`/`while`/`break`/`continue` transpose. `switch` is rewritten: **no fall-through, value- and condition-dispatch forms, `else` default, W021**. `goto` and `do…while` → ⛔ omit-with-note (show the loop refactor instead). |
| **4. Functions and Program Structure** | 💪 | Storage classes are a *strength*: K&R has `static`/`extern`; b++ has `static`/`extrn`/`global`/`const` + a real link graph (E263/E264). Go deeper than K&R. **The preprocessor section → 🔁 replace** with modules (`import`/load) + `const` literal substitution. |
| **5. Pointers and Arrays** | 🔄 + 💪 | Pointers and pointer arithmetic transpose. Arrays diverge: teach **both** levels — raw pointers / `buf_*` / `malloc` *and* the growable containers (`arr_*`, `arr_struct_*`). `argv` exercises map via `argc_get`. Multi-dim/pointer-arrays → adapt to b++ idiom. |
| **6. Structures** | ✅ + 💪 | `struct` + per-field hints transpose. **Bitfields (K&R §6.9) map to `: bit` hints** — a clean win. Self-referential structs (lists/trees) transpose (cf. `arr_struct` of records). `typedef` → ⛔ omit (structs are named directly); `union` → ⛔ omit-with-note. |
| **7. Input and Output** | 🔁 | Replace stdio: `put`/smart dispatch instead of `printf`/`scanf` format strings; `file_read_all`/`file_write_all` for files; `putstr_err` for diagnostics. Teach the b++ way (typed dispatch) rather than format-string parsing. |
| **8. The System Interface** | 🔁 + 💪 | Replace/expand the UNIX-interface chapter: `sys_*` syscalls, `file_*`, `file_stat`. K&R's iconic "write your own storage allocator" maps directly onto **b++'s own arena/free-list allocator** — a distinctive parallel. Cross-target (a64/x64, macOS/Linux) makes this *richer* than the original. |
| **Appendix A — Reference Manual** | partial | Already exists in pieces: `docs/manual/` + `warning_error_log.md` (E###/W###). The book points to / consolidates these. |
| **Appendix B — Standard Library** | partial | Maps to the cartridge map (`stb++_lib.md`) + the prelude (`bpp_arr`, `bpp_array`, `bpp_io`, `bpp_args`, …). |

**Reading of the map.** Nothing is a dead end. Chapters 1 and 3 transpose;
**4 and 6 are places b++ goes deeper than K&R**; 2 and 7 diverge and get
rewritten; 5 and 8 map with b++-specific richness. The divergences are the most
valuable content — they teach what makes b++ b++.

---

## Part D — The recurring pedagogical device

A standard call-out box, used wherever b++ departs from C, keeps the
transposition honest and turns every divergence into a lesson:

> **C → b++.** *The C way (K&R):* `printf("%d\n", n);` — *The b++ way:*
> `put(n); put("\n");` (smart dispatch picks `putnum`). *Why it differs:* b++
> has no format-string mini-language; the compiler dispatches on the inferred
> type of the argument.

These boxes are the book's spine. They are also a checklist of the language's
edges (the divergence ledger below).

---

## Part E — Divergence ledger (the edges to teach, and to watch)

Each is a place the transposition is *not* 1:1 — a teaching beat, and sometimes
a design-feedback signal:

1. **Typeless default.** `auto x` is a word; types are optional hints. (Ch 2 —
   the central rewrite, and the B-lineage hook.)
2. **`switch` without fall-through**, with value- and condition-dispatch forms,
   `else` default, and W021 exhaustiveness. (Ch 3 — safer than C.)
3. **No preprocessor.** Modules (`import`/load) + `const` literal substitution
   replace `#include`/`#define`. (Ch 4.)
4. **Arrays are containers.** `arr_*` / `arr_struct_*` alongside raw pointers,
   not C's array-decays-to-pointer story. (Ch 5.)
5. **No `typedef`, no `union`.** Structs named directly; bitfields via `: bit`.
   (Ch 6.)
6. **`put` / smart dispatch, not `printf`.** No format strings. (Ch 7.)
7. **Own allocator + cross-target syscalls.** b++'s arena/free-list as the
   "write your own malloc" parallel. (Ch 8.)
8. **No `goto`, no `do…while`.** Show the structured-loop refactor. (Ch 3.)

If any of these turns out to *block* a natural teaching example (not just
differ), that is a design-feedback item for the language, logged here.

**Findings surfaced by writing Chapter 1 (2026-06-10) — the payoff in action:**

- **Assigning to a parameter disturbed the caller's argument — FIXED
  2026-06-10.** `f(x){ x = x+100; }` called as `f(n)` left the caller's `n`
  changed; even `f(n+0)` leaked. **Root cause:** the inliner
  (`cg_emit_inline_multi`, `bpp_codegen.bsm`) classified a parameter whose
  argument is a simple `T_VAR`/`T_LIT` as SIMPLE — substituting *every*
  reference to the parameter with the caller's argument node. Correct for a
  *read* parameter, but for a *written* one (`x = x+100`) the substitution
  rewrote the store into the caller's variable (`n = n+100`). `f(n+0)` folded to
  `T_VAR n` before inlining, so it leaked too; a genuinely non-trivial arg
  (`f(n*2)` → `T_BINOP` → BIND) was always safe — the tell that pinned it.
  **Fix:** new `_inline_writes_param` scanner (mirrors `ast_clone_subst`'s
  traversal) forces the BIND strategy (own per-callsite local) for any parameter
  written in the body, keeping the fast SIMPLE path for read-only parameters.
  Bootstrap byte-stable (gen1==gen2), suite 183/0/12, zero warnings. §1.8 now
  teaches genuine call-by-value (you may modify a parameter; it is a private
  copy). Reading params was always fine, which is why the whole compiler
  bootstrapped despite the bug.
- **`put("literal")` mis-dispatched inside a `switch` arm — FIXED 2026-06-10.**
  Root cause: the type-inference pass (`bpp_types.bsm`) had three statement
  walkers (`add_type`, `propagate_in_node`, `uses_int_ops_node`) that descended
  into `T_IF`/`T_WHILE`/`T_BLOCK` but **not** `T_SWITCH` — so a `put` inside a
  switch arm was never typed, lost its smart-dispatch rewrite, and stayed the
  default `put` = `putnum` (printing the string pointer as an int). Same class
  as the historical "ast_clone_subst missed T_IF/T_WHILE/T_BLOCK" gap. Fix:
  added a `T_SWITCH` case to all three walkers, traversing the subject, each
  arm's case-label values (`n.b`) and body statements (`n.src_tok`), and the
  else body — the canonical arm iteration used by codegen. `put` now works
  inside switch arms; float-param propagation and int-op detection reach arm
  bodies too. Bootstrap byte-stable (gen1==gen2), suite 183/0/12, zero warnings.
  The book still uses explicit printers as a style choice (W032-clean), not a
  workaround.

---

## Part F — Repository layout

```
book/
  README.md              # TOC + how to read / build the examples
  ch01_tutorial.md
  ch02_types.md
  …
examples/book/ch01/      # every in-text example as a runnable .bpp
  hello.bpp
  …
tests/book/ch01/         # exercise reference solutions, CI'd by run_all
  ex01_xx.bpp
  …
```

Convention: every code listing in the prose is a real file under
`examples/book/`; every exercise ships a reference solution under `tests/book/`
so the suite compiles and runs the entire book. A broken example fails CI.

---

## Part G — Phasing

- **Phase 0 (this doc).** Surface enumerated, mapping fixed. ✅
- **Phase 1 — Chapter 1 as a vertical slice.** ✅ **DONE 2026-06-10.**
  `book/README.md` (front matter + "C → b++" box convention) + `book/
  ch01_tutorial.md` (10 sections: hello → variables → for → const → char I/O →
  arrays → functions → parameters → char arrays → scope). 13 runnable examples
  (`examples/book/ch01/`) + 7 self-checking exercises (`tests/book/ch01/`) +
  the conformance gate `tests/book/run_book.sh`. **20/20 green, zero warnings.**
  The gate compiles AND runs every listing; an assert trip fails it. Proves the
  format and the call-out device end-to-end; awaiting user review of tone.
- **Phases 2–8.** One chapter per arc, its own commit; examples and exercise
  solutions land as a growing conformance corpus.

Keep scope disciplined — one chapter at a time, each shippable and CI-green,
the same way the AI port and the compiler arcs ship verified steps as
individual commits. This is documentation with a concrete killer use case (no
learner doc exists), so it clears the restraint gate; the only real risk is
over-scoping, which the phasing contains.

---

## Part H — Open questions (resolved + deferred)

- **Prose language:** English. *(decided 2026-06-10)*
- **First deliverable:** Phase 0 mapping doc, then Chapter 1 slice.
  *(decided 2026-06-10)*
- **Where the prelude / auto-injection is taught:** three tiers *(decided
  2026-06-10)*. (1) A short forward-reference **box in Chapter 1** — just enough
  that the reader knows where `put` & friends come from (already written). (2)
  The full didactic treatment in **Chapter 4 (Functions & Program Structure)** —
  the natural home, since that is where K&R covers `#include`/`#define`/program
  structure, which we replace with modules (`import`/load) + the prelude:
  compilation units, what the prelude provides and why, importing your own
  modules, scope across files. (3) The prelude **reference list** (the ~21
  modules and what each offers) in **Appendix B (Standard Library)**, mapping to
  `stb++_lib.md` + the prelude modules. The *injection mechanism* itself
  (`bpp_import.bsm`, dependency-order injection) stays a **light sidebar** in
  Ch4 — a learner needs "these tools are always here, they are real B++
  libraries, here's how to add your own", not the compiler internals (those live
  in `docs/manual/`).
- **Deferred:** exact exercise difficulty curve per chapter; whether to ship a
  `bpp run book/...` convenience runner; whether Appendices A/B are written
  fresh or are curated links into existing `docs/manual/`. Decide at Phase 1
  review.
