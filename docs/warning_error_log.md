# B++ Warning & Error Reference

Every diagnostic the compiler can emit, where it's defined, what triggers it,
and whether it currently shows source location (file:line + source + caret).

**Rule: every new warning or error MUST show source location.** No exceptions.
If you can't get the exact line, at minimum show the function name and module.
Update this document and `tests/test_diagnostics.bpp` when adding new codes.

## Status Summary

```
Total diagnostics:  23 active + 2 reserved
With source location: 19  ✅  (file:line + source + caret)
With filename only:    3  ⚠️  (import resolver — runs before lexer, no tok_pos)
  E001 (cannot open file) — shows filename
  E002 (import not found) — shows filename + search paths
  E222 (circular import)  — shows unresolved module names
Reserved:              2  (W017, W014 — see Reserved section)
```

## Errors (fatal — compilation stops)

| Code | Message | File | Line | Has loc? | Trigger |
|------|---------|------|------|----------|---------|
| E001 | recursive import | bpp_import.bsm | 756 | ❌ | Module imports itself |
| E002 | import/load not found | bpp_import.bsm | 977 | ❌ | File not found in search paths |
| E050 | struct field type mismatch | bpp_validate.bsm | 218 | ❌ | Struct field with wrong type |
| E053 | type annotation error | bpp_validate.bsm | 232 | ❌ | Invalid type annotation |
| E101 | unknown type | bpp_parser.bsm | 569 | ✅ | `: UnknownType` after variable |
| E102 | unexpected token in primary | bpp_parser.bsm | 1609 | ✅ | Invalid start of expression |
| E103 | unexpected keyword | bpp_parser.bsm | 1712 | ✅ | Keyword used as expression |
| E104 | unexpected token | bpp_parser.bsm | 1788 | ❌ | Parser fallthrough error |
| E105 | main() in module file | bpp_parser.bsm | 1127 | ❌ | main() defined in .bsm not .bpp |
| E120 | parse error (generic) | bpp_parser.bsm | 826 | ✅ | General parse failure |
| E201 | undefined function | bpp_validate.bsm | 368 | ✅ | Call to undeclared function |
| E221 | duplicate definition | bpp_parser.bsm | 1085 | ✅ | Same function in two modules |
| E222 | circular dependency | bpp_import.bsm | 198 | ❌ | Module import cycle |
| E230 | static const at file scope | bpp_parser.bsm | TBD | ✅ | `static const X = N;` silently produces 0 at runtime |
| E232 | silent float→int at assignment | bpp_validate.bsm | 396 | ✅ | `auto x; x = 3.14;` drops IEEE 754 bits; annotate `: float` or write int literal |
| E233 | silent float→int at call site | bpp_validate.bsm | 495 | ✅ | float arg passed to int param (promoted from W002) |
| E240 | int passed to float param | bpp_validate.bsm | 519 | ✅ | callee expects float, caller passed int — annotate source `: float` or use float literal |
| E242 | shift count out of range | bpp_validate.bsm | 558 | ✅ | shift count > 63 — hardware masks to 6 bits, value silently wrong |
| E243 | pointer compared to non-zero literal | bpp_validate.bsm | 600 | ✅ | `if (ptr == 42)` is almost always a bug; only 0 is the canonical null check |
| E244 | float literal in int context | bpp_parser.bsm + bpp_validate.bsm | 2122 + 583 | ✅ | array index or shift count cannot be a float literal |

## Warnings (compilation continues)

| Code | Message | File | Line | Has loc? | Trigger |
|------|---------|------|------|----------|---------|
| W002 | implicit float-to-int | bpp_validate.bsm | n/a | n/a | RETIRED in 0.23.x — promoted to E233 (silent float→int at call site is now a fatal error, not a warning) |
| W003 | wrong argument count | bpp_validate.bsm | 407 | ❌ | Call with mismatched arg count |
| W005 | unreachable code | bpp_validate.bsm | 299 | ✅ | Code after return statement |
| W010 | narrowing conversion | bpp_validate.bsm | 270 | ❌ | Float narrowed to half/quarter |
| W011 | precision loss | bpp_validate.bsm | 254 | ❌ | Word to half float |
| W012 | & in FFI argument | bpp_validate.bsm | 451 | ✅ | Address-of passed to extern/call |
| W013 | : base mismatch | bpp_dispatch.bsm | 939 | ✅ | Function annotated base but impure |
| W020 | static cross-module | bpp_validate.bsm | 397 | ✅ | Calling static fn from other module |
| W021 | switch not exhaustive | bpp_parser.bsm | TBD | ✅ | switch without else arm |

## What "has location" means

**✅ Shows source location:**
```
warning[W003]: 'add' expects 2 argument(s), got 1
  --> game.bpp:42
  42 | x = add(10);
     |            ^
```

**❌ Missing source location:**
```
warning[W003]: 'add' expects 2 argument(s), got 1
```

The ❌ ones need `diag_loc(n.src_tok)` + `diag_show_line(n.src_tok)` added.
AST nodes now carry `src_tok` (the token position at creation time) so the
information is available — it just needs to be wired through.

## How to add source location to a warning

In the validator (val_check_node), after the warning message:
```bpp
diag_warn(N);
diag_str("message...");
diag_newline();
_val_show_context_at(n.src_tok);    // ← add this line
```

In the dispatch (classify_all_functions), use the function record's stored position:
```bpp
diag_warn(N);
diag_str("message...");
diag_newline();
diag_loc(*(funcs[i] + 64));         // ← tok_pos stored at offset 64
diag_show_line(*(funcs[i] + 64));
```

In the import resolver, use the current line position:
```bpp
diag_fatal(N);
diag_str("message...");
diag_newline();
// Import errors don't have tok_pos — show the filename instead.
```

## Line number accuracy

The line number shown is file-relative (not absolute in the merged source).
`diag_loc` converts absolute → relative using `diag_file_line_starts[module]`.

Known issue: module 0 (entry file) line numbers may be off if `cur_line` is
not reset before lexing. The fix: set `cur_line` to the newline count up to
`mod0_real_start` before module 0's lex loop in bpp.bpp.

## Reserved / Planned codes

Codes reserved for planned features. Do NOT reuse these numbers for other diagnostics.

| Code | Planned for | Status |
|------|-------------|--------|
| W017 | void used as value (`x = void_func()`) | Reserved — ships with void keyword validation |
| W014 | extrn written after freeze point | Reserved — needs investigation (crashed self-host) |

## Process: adding a new diagnostic

1. Choose a code: errors use E-series, warnings use W-series
2. Check the Reserved table above — do not reuse reserved codes
3. Add `diag_fatal(N)` or `diag_warn(N)` with clear message
4. Add `diag_loc(tok_pos or n.src_tok)` + `diag_show_line(...)` 
5. Update this document with the new code
6. Add a test case to `tests/test_diagnostics.bpp`
7. Run `tests/run_all.sh` to verify no regression

## Backend / linker bugs (historical, non-diagnostic)

Bugs that were NOT compiler diagnostics — they surfaced at runtime
(dyld, the kernel loader, or miscompiled code) and were silent for
long stretches before something tipped them over a threshold. They
live here because the lesson — "what signal did we miss, what tool
finally caught it" — belongs next to the diagnostics we do have.

### `chained_fixups_page_count` — hardcoded page_count = 1 (2026-04-16, fixed)

- **Where it lived.** `src/backend/target/aarch64_macos/a64_macho.bsm`
  — `mo_write_chained_fixups_real`. Emitted the chained-fixups
  header with a literal `mo_w8(1); mo_w8(0);` for `page_count`.
- **Why it worked for months.** As long as `__DATA` fit in one
  16 KB page, a single rebasable page was always the correct
  count.
- **What tipped it.** The 16th dispatch seed in `init_dispatch()`
  (`sys_lseek`, four bytes of literal plus the 15 prior seed
  names that each carried a four-byte-or-longer literal) pushed
  `__DATA` past 16 KB. dyld rebased page 0 only; GOT pointers on
  page 1 kept their on-disk values, and ASLR-relative string
  literal reads returned garbage.
- **Runtime error shape.** Compiler refused to self-compile with
  `global 'break' not found in data section`. The 'break' string
  literal the lexer was looking up had been silently corrupted
  because its GOT pointer sat on page 1.
- **Signal we had been ignoring.** `DYLD_PRINT_WARNINGS=1` had
  been printing `dyld[...]: fixup chain entry off end of page` on
  every build for weeks. Nobody read the warning.
- **Tool that caught it.** `bug --break buf_eq --dump-str` showed
  the literal argument's pointer contents as gibberish at call
  time, which disproved every hypothesis about the *lookup* logic
  and forced attention onto the *bytes themselves*. See
  `docs/debug_with_bug.md` for the full case study.
- **Fix.** Dynamic `page_count = ceil(mo_data_size / 16 KB)` +
  per-page start offsets in the header; dynamic `imports_off`
  that tracks the now-variable segment size.
- **Lesson recorded.** When a runtime symbol lookup fails against
  a literal the compiler definitely emitted, the first check is
  always "do the runtime bytes match the file bytes." Everything
  else is plumbing around that one question.
