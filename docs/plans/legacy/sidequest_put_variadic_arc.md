# Sidequest: extend `put` to accept N args via smart-dispatch concat

**Status: CLOSED / SHIPPED** (`236d9ff`, variadic put/put_err via parser-level smart-dispatch concat; doctrine recorded as Tonify Rule 45). Moved to legacy 2026-07-03.

> **Goal**: `put("count: ", n, " of ", total, "\n");` works.
> Each arg is independent, goes through smart-dispatch in order.
> No format string. No placeholders. No new function names. The
> rule is "`put(x, y, z, ...)` prints each arg, picking the right
> printer per type."

## The one-line ask

Extend `put` and `put_err` in the parser so a call with **2 or more
args** expands at parse time into a sequence of typed sub-calls
(`putstr` / `putnum` / `putfloat` / `putchar` etc.) — one per arg,
chosen by the existing smart-dispatch by inferred type. The single-arg
form `put(x)` is **unchanged**.

## Why this shape, and why NOT format strings

A short design-history note so the next agent does not re-litigate
this. Three options were considered:

| Option | Example | Verdict |
|---|---|---|
| New names (`println`, `printf`) | `println("count: {}", n);` | Rejected — adds names instead of generalising the existing one |
| Zig-style `{}` format string on `put` | `put("count: {}\n", n);` | Rejected — adds syntax (`{}`, `{{`, `}}`) + diagnostics (E270/E271/E272) + parser slicing for no extra value over concat |
| Concat — args in order, no placeholder | `put("count: ", n, "\n");` | **Picked** — uses smart-dispatch's existing per-arg type selection, zero new syntax |

Smart-dispatch already chooses the right printer per type for a single
arg. Generalising to N args is "do the same thing per arg in order."
No placeholder is needed because the args are already in the right
position — between the literal pieces, exactly where the comma is.

## The rule, in one sentence

> **`put(a, b, c, ...)` prints each argument in order. Each argument
> goes through `put(x)` smart-dispatch — string → `putstr`, number →
> `putnum`, float → `putfloat`. `put_err` is the stderr mirror. No
> trailing newline is added; include `\n` in an argument if you want
> one.**

## Semantics by example

```b++
put("hello\n");                       // unchanged — putstr("hello\n")
put(n);                               // unchanged — putnum(n)
put("count: ", n, "\n");              // new — putstr + putnum + putchar
put("a=", a, " b=", b, " c=", c);    // new — no newline
put(a, b, c);                         // new — concatenated, no separator
put_err("error[E", code, "]: ", msg, "\n");   // stderr mirror
```

What each example desugars to at parse time:

```b++
// put("count: ", n, "\n");
//   →
{
    putstr("count: ");
    put(n);                // dispatched by n's type — putnum if word
    putstr("\n");
}
```

Each arg is independently type-inferred, so `put(arg)` selects the
right printer via the same `cg_builtin_dispatch` logic that runs today
for single-arg `put`.

## What stays the same

- All existing `put*` primitives (`putchar`, `putstr`, `putnum`,
  `putfloat`, `putmsg`, `putline`, the `_err` mirrors) keep working,
  unchanged. The new behaviour is a **parser-level expansion** on top
  of them.
- Single-arg `put(x)` behaviour is byte-identical to today. Smart
  dispatch picks `putstr` / `putnum` / `putfloat` exactly as before.
- `put("hi {world}")` with one arg keeps printing the literal string
  with the braces. **No format-string parsing exists**, so `{` and `}`
  carry no special meaning anywhere.
- `putmsg(s)` stays as the "string + newline" shortcut. Could be
  retired in a later cleanup, but not in this arc.

## Implementation phases

Each step is bootstrap byte-stable + suite green + a unit test.
Estimated total: **1-2 days**.

### P0 — Test fixtures + design lock (~half day)

- Write `tests/test_put_variadic.bpp` exercising:
  - `put("hello\n")` — single literal (unchanged)
  - `put(n)` — single number (unchanged)
  - `put("count: ", n, "\n")` — two args, literal + number + literal
  - `put(a, b, c)` — three args, no literal pieces
  - `put("x=", x, " y=", y, " z=", z, "\n")` — mixed
  - `put_err(...)` mirror cases
  - Float + string + word mixed: `put("at ", x, ", ", y, ": ", err)`
- Write `tests/test_put_variadic_safe.bpp` confirming a function
  using only `put(...)` with literals and `auto`-typed numbers
  passes existing diagnostics (W032 still fires per arg when TY_WORD
  is ambiguous).
- No negative tests in this phase — the syntax is permissive (every
  list of args is valid).

### P1 — Parser recognises multi-arg `put` / `put_err` (~half day)

- In `bpp_parser.bsm` where `put` / `put_err` are parsed (currently
  treated as ordinary T_CALL with smart-dispatch hint), check the
  arg count.
- If `args == 1`, fall through to today's code path. Zero behaviour
  change.
- If `args >= 2`, route to `parse_put_variadic` helper. The helper:
  1. Wraps the sequence into a T_BLOCK so the call site stays
     statement-position.
  2. For each arg in order, builds a T_CALL of `put` (or `put_err`)
     with that single arg. Each gets its own type-inferred dispatch
     at the next pass.
  3. The block has no value (already statement context).
- No new lexer rules. No string slicing. No format parsing.

### P2 — Validator + smart-dispatch (~0 days, free)

- Because P1 expands into single-arg `put(x)` calls that already
  exist, the validator and codegen need no changes. Each sub-call
  flows through `add_type`, smart-dispatch in `cg_builtin_dispatch`,
  and codegen identically to a hand-written version.
- Verify with `bug --disasm` on a couple of test programs that the
  emitted machine code matches what you would get from writing the
  sub-calls by hand.

### P3 — Backward-compat sweep (~half day)

- Grep the repo for `put(` and `put_err(` calls that currently pass
  multiple args. Today these are rejected by the parser (W003 — too
  many args). After this arc, they become legal. Walk every hit and
  confirm the new behaviour matches author intent:
  ```sh
  grep -rn 'put(' src/ stb/ games/ tools/ examples/ tests/ \
      | grep -vE 'put\(([^,()]|\([^)]*\))*\)' \
      | head
  ```
- Pay attention to test fixtures that intentionally trigger W003 —
  those become silent under the new behaviour. Convert them to a
  different W003 trigger (e.g. an actual user function with 1 declared
  param called with 2 args).

### P4 — Docs + book update (~half day)

- Update `bpp_io.bsm` header comments: note that `put` accepts N args
  and concats with smart-dispatch.
- Update `docs/manual/how_to_dev_b++.md` Cap 12 (I/O) — replace the
  current single-arg description of `put` with the N-arg rule and
  a table of "what each arg becomes."
- Update `docs/manual/tonify_checklist.md` if Rule 13 (smart-dispatch
  on `put`) needs adjustment to mention the multi-arg form.
- Update the book Chapter 1:
  - `06_count_chars.bpp` example: change last line to
    `put(count, "\n");` so the book example shows the new ergonomic
    form.
  - In §1.5, after the existing sidebars, add a new short sidebar:
    "Sidebar — `put` accepts as many args as you want. Each one
    is printed in order, with the right format for its type. There
    is no separator between args (no automatic space, no automatic
    newline); include literal text or `\n` where you want them."
  - Update §1.2 callout if it implies `put` is single-arg.

### P5 — Smoke test + close-out (~half day)

- Bootstrap byte-stable (gen1 == gen2).
- Tripod green (native 180+, C-emit, Linux self-host).
- `bench_compile.sh` — bootstrap stays at current speed.
- Journal entry + README headline.

## Risks + mitigations

- **Backward compat: silently changed behaviour of `put(a, b)`.**
  Today this fires W003 ("expects 1 argument, got 2"). After the arc,
  it compiles + emits both. Risk: a program calling `put(x, ignored)`
  by accident used to warn and now silently does something the author
  did not intend. Mitigation: P3 sweep + journal note.
- **Parser ambiguity inside `put(...)` argument list.** None expected
  — comma is already the arg separator at the call site. The expansion
  happens AFTER the call's argument list is parsed.
- **`put` recursion in the expansion.** Each sub-call is `put(arg_i)`
  with one arg. The parser must route the single-arg case to the
  existing single-arg path, never back into the multi-arg expansion,
  or we infinitely loop. Mitigation: the helper builds T_CALL nodes
  with one arg AND a flag that says "do not re-expand" — or simpler,
  the recursion guard is the arg-count check (`if args >= 2`).
- **`@safe` interaction.** The expansion is into `put` / `putstr` /
  `putnum` / `putchar` which all call `sys_write`. None of them are
  `@safe`-clean. Same status as the single-arg `put` today —
  documented, no new restriction.
- **C-emit backend.** The expansion happens in the parser, before any
  backend sees the call. C-emit sees the expanded sub-calls and emits
  them as normal. No backend change required.

## Open questions for the external reviewer

When this gets reviewed (same pattern as the allocator + file_stat +
hedged-reads arcs):

1. **Trigger by arg count, or by something else?** Recommended: only
   `args >= 2` triggers the multi-arg path. Single-arg stays exactly
   as today. Alternative: always allow the expansion (`put()` with
   zero args is a no-op). The zero-arg case is cosmetic — does it
   matter? Probably no, but worth deciding.
2. **Separator default.** Concat means no separator. Go / Python
   `Println(a, b)` puts a SPACE between args automatically. Should
   B++ follow that? Recommended: **no**. The user can write
   `put(a, " ", b)` if they want spaces — explicit and uniform.
   Adding auto-separators makes the rule less mechanical and breaks
   "smart-dispatch per arg" symmetry.
3. **Auto-newline at end?** Same answer: **no**. Add `"\n"` to the
   last arg when you want one. Symmetric with how `putnum` /
   `putstr` work today.
4. **Reserve the right to add format specifiers later as helper
   functions?** Recommended: yes — `hex(n)`, `pad(s, w)`,
   `precision(f, 4)` etc. would be plain functions returning strings.
   No syntax extension; they compose with `put` naturally:
   `put("addr=", hex(addr), "\n");`. **DEFER** — wait for a real
   need.
5. **C-emit verification.** Confirm the C backend handles the
   expanded sub-calls identically to native. The expansion is in the
   parser so C-emit should be untouched, but worth running
   `tests/run_all_c.sh` after the arc and noting the result.

## Constraints any recommendation must respect

- **Rule 41 (additive portability)**: parser change in
  `bpp_parser.bsm` (cross-platform). No chip-level changes. No per-OS
  changes.
- **Bootstrap byte-stability**: gen1 == gen2 must hold across every
  commit.
- **No runtime variadic**: no boxed args, no runtime type table, no
  format-string parser. The expansion is **entirely at parse time**,
  exactly like `for` desugaring to `while`.
- **Backward compatibility**: single-arg `put(x)` byte-identical to
  today. `put("hi {world}")` keeps printing literal braces — no
  format-string syntax was added.
- **Smart-dispatch reuse**: each sub-arg flows through the existing
  smart-dispatch in `cg_builtin_dispatch`. No new dispatch logic.
- **`@safe` interaction**: `put` family stays not-`@safe` (calls
  `sys_write`). Same as today.

## Cross-references

- `src/bpp_io.bsm` — `put` / `put_err` primitives. Stays unchanged.
- `src/bpp_codegen.bsm` — `cg_builtin_dispatch` + `put(x)` smart
  dispatch. Reused without modification.
- `src/bpp_parser.bsm` — where the new arg-count check + expansion
  helper lives. The only file with substantive changes.
- `docs/manual/warning_error_log.md` — W003 ("too many args")
  semantics shift for `put` specifically. Either gain a footnote or
  add a new diagnostic ID for the case where W003 used to fire but
  no longer does.
- `docs/manual/how_to_dev_b++.md` Cap 12 — updated description of
  `put` family.
- `books/The_B++_Programming_Language/ch01_tutorial.md` §1.5 — new
  sidebar + `06_count_chars` example update.
- `docs/plans/sidequest_allocator_final_push.md` — template this
  doc follows.
