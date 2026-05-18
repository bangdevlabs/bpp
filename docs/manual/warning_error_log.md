# B++ Warning & Error Reference

Every diagnostic the compiler can emit, where it's defined, what triggers it,
and whether it currently shows source location (file:line + source + caret).

**Rule: every new warning or error MUST show source location.** No exceptions.
If you can't get the exact line, at minimum show the function name and module.
Update this document and `tests/test_diagnostics.bpp` when adding new codes.

## Status Summary

```
Total diagnostics:  25 active + 2 reserved
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
| E245 | array element type conflict | bpp_types.bsm | ty_set_elem | ✅ | Two different element types inferred for the same TY_ARR variable — arrays are always homogeneous |
| E246 | incompatible func-types | bpp_validate.bsm | 416/590 | ✅ | Annotation contract violated: assignment between two `func(...)` types with mismatched shape, or `call(fp, args)` where fp's annotated signature disagrees with the call site arg types (V3 Session 3 — shipped 2026-05-05) |
| E247 | func-type cannot exceed 8 args | bpp_parser.bsm | parse_fn_type | ✅ | A `func(arg1, ..., arg9)` declaration exceeds the AAPCS64/SysV register-pass ceiling (V3 Session 1 — shipped 2026-05-04) |
| E248 | func-type nesting exceeds maximum depth | bpp_parser.bsm | parse_fn_type | ✅ | Higher-order `func` types deeper than 16 levels — almost always a typo (V3 Session 1) |
| E249 | malformed func-type | bpp_parser.bsm | parse_fn_type | ✅ | Missing `(` after `func` or missing `->` after argument list (V3 Session 1) |
| E260 | deprecated phase keyword | bpp_parser.bsm | 1556 | ✅ | Programmer wrote one of the legacy phase keywords (`@base`, `@solo`, `@time`, `@io`, `@gpu`, `@heap`, `@panic`, `@realtime`) on a function signature. All eight were retired in the 2026-05-11 phase annotation collapse. The diagnostic emits source location plus three help lines pointing at `@safe` (the post-collapse replacement), the omit-it option, and `docs/manual/tonify_checklist.md` Rule 4 for the spec. Replaces the generic E104 "unexpected token '{'" that fired before the dedicated diagnostic landed. |
| E261 | float literal stored into typed-int local | bpp_validate.bsm | 698 | ✅ | `auto x: word; x = 0.5;` (or any other int slice on the left, float literal on the right). Excalibur Arc Feature 1 closure (gap A): the LITERAL case is bug-shaped because the literal exists only to carry fractional bits and the destination cannot hold them. The variable case (`x: word; x = some_float_var;`) stays silent because float→int truncation through a variable is intentional for screen-coordinate / pixel work. Help lines suggest the Excalibur Feature 2 cast builtins (`floor_int` / `trunc_int` / `round_int`) for explicit conversion intent, or annotating `: float` to keep the bits. |
| E262 | cross-struct assignment between typed locals | bpp_validate.bsm | 700 | ✅ | `auto wp: WorldPos; auto gp: GridPos; wp = gp;` — both lhs and rhs are typed-struct locals with DIFFERENT struct identities. Excalibur Feature 3 closure: structs are distinct types in B++; an assignment between two struct identities is almost always a domain-mixing bug. The diagnostic prints both struct names and points the programmer at the conversion-helper pattern (`world_to_grid(w: WorldPos): GridPos { ... }`) plus Rule 29. Uses parser-side `get_var_type_in_fn` to look up struct idx per typed local, scoped to the current function (avoids cross-function vt_names bleed). |
| E263 | cannot assign to const slot | bpp_validate.bsm | T_ASSIGN handler | ✅ | `global const X = 7; main() { X = 99; }` — writing to a slot declared with `global const` or `static const` violates the read-only contract those storage classes establish. Storage class sidequest 2026-05-14: the slot lives in `.data` with the init value the parser captured, and the compiler enforces it as immutable. Help line points at `auto` / `global` for the mutable equivalents. |
| E264 | extrn has no backing definition in the link graph | a64_macho.bsm + x64_elf.bsm | mo_resolve_relocations | ✅ | `extrn SCREEN_W;` (declared inside e.g. `stbrender.bsm`) without any module providing the slot. Storage class sidequest 2026-05-14 — replaces the cryptic "internal error: global 'X' not found in data section" that fired when a reloc walker couldn't locate the symbol. Help block lists the three fix paths (`global const X = N;` slot, `auto X;` write-once init, or import a module that already defines it) plus the gotcha that `const X = N;` is parser-level literal substitution and emits no symbol. |

## Runtime asset-load diagnostics (stderr, not compile-time)

Not diagnostics from the compiler — these are printed by stb
library loaders at runtime when an asset fails to load.
Programmer-facing messages so games don't have "silent missing
sprite / silent muted sample" bugs. Added 2026-04-18 after the
ironplate WAV import surfaced several silent-format-reject
cases.

All follow the same shape: `<module>: '<path>': <reason>`.

**stbsound** (`sound_load_wav`):
- `file not found or unreadable`
- `truncated (under 12 bytes)`
- `not a RIFF/WAVE file`
- `missing fmt chunk`
- `missing data chunk`
- `unsupported PCM bit depth <N> (need 8, 16, 24, or 32)`
- `IEEE float must be 32-bit, got <N>-bit`
- `format code <N> not supported (accepts PCM=1 or IEEE float=3)`
- `<N> channels not supported (need mono or stereo)`

**stbimage** (`img_load`):
- `file not found or unreadable`
- `not a valid PNG or unsupported chunk layout`

**stbfont** (`font_load`):
- `file not found or unreadable`
- `no 'head' table (not a valid TTF)`

Convention: the loader prints + returns 0; callers in
`stbasset.bsm` propagate the null handle, never wrap or
re-log.

## Warnings (compilation continues)

| Code | Message | File | Line | Has loc? | Trigger |
|------|---------|------|------|----------|---------|
| W002 | implicit float-to-int | bpp_validate.bsm | n/a | n/a | RETIRED in 0.23.x — promoted to E233 (silent float→int at call site is now a fatal error, not a warning) |
| W003 | wrong argument count | bpp_validate.bsm | 407 | ❌ | Call with mismatched arg count |
| W005 | unreachable code | bpp_validate.bsm | 299 | ✅ | Code after return statement |
| W010 | narrowing conversion | bpp_validate.bsm | 270 | ❌ | Float narrowed to half/quarter |
| W011 | precision loss | bpp_validate.bsm | 254 | ❌ | Word to half float |
| W012 | & in FFI argument | bpp_validate.bsm | 451 | ✅ | Address-of passed to extern/call |
| W013 | @base mismatch | bpp_dispatch.bsm | n/a | n/a | REMOVED 2026-05-11 (phase annotation collapse). The `@base` annotation was retired with the rest of the 8-phase lattice; `@safe` (W026) is now the only user-facing phase enforcement. The classifier code that drove this warning is dead. Historical: fired when a function annotated `@base` was found to be SOLO by the BASE/SOLO classifier. |
| W020 | static cross-module | bpp_validate.bsm | 397 | ✅ | Calling static fn from other module |
| W021 | switch not exhaustive | bpp_parser.bsm | TBD | ✅ | switch without else arm |
| W025 | public param without hint used in float arithmetic | bpp_validate.bsm | 681 | ✅ | Non-static fn uses un-hinted param in a T_BINOP where the other side is float-typed — annotate `: float` to preserve IEEE bits or `: word` to document integer intent (Rule 13) |
| W026 | @safe contract violated | bpp_dispatch.bsm | 1796 | ✅ | Function annotated `@safe` reaches HEAP / IO / GPU / SOLO through its transitive call graph. The annotation is a contract: bounded time, no malloc, no IO, no GPU, no syscall. Killer use case: audio callbacks (e.g. `_aud_square_cb` in `_stb_audio_macos.bsm`) and worker thread entry points. Verify with `examples/phase_lattice_bad.bpp` (`bad_safe_heap`, `bad_safe_io`). The legacy `@time`/`@io`/`@gpu` enforcement was REMOVED 2026-05-11 in the phase annotation collapse — those annotations are no longer accepted by the parser. |
| W028 | call() arg-type mismatch via flow analysis | bpp_validate.bsm | 165 | ✅ | `call(fp, args)` where fp's resolved target (intra-function origin tracking, Session 2; or cross-function via registration helper, Session 4) declares a parameter whose float/int polarity does not match the supplied arg. Estrita conflict resolution for multi-origin: any registered target with a mismatching signature fires. Diagnostic-shipped 2026-05-05 — closes Pitfall 6 from `tonify_checklist.md` |
| W031 | callback registered without `@safe` | bpp_dispatch.bsm | 1226 | ✅ | A `fn_ptr(target)` literal flowed into a registration sink (`maestro_register_base` for worker threads, `job_submit` for worker tasks) and `target` was not annotated `@safe`. Without `@safe`, the compiler cannot verify the bounded-time / no-malloc / no-IO contract — a worker that grows a malloc later races against itself silently. The suggestion engine is the positive companion of W026 (W026 verifies the contract once you claim it; W031 nudges you to claim it where it makes sense). Diagnostic emits source location of the TARGET function (where to add `@safe`), plus three help lines. See Task 4 of phase annotation collapse arc in `journal.md`. |

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
| W030 | scoped zone inside `@safe` region | Reserved — GPU pipeline roadmap Phase 6.3 (scoped zones compiler feature). Originally reserved for `@gpu`; renamed during the 2026-05-11 phase annotation collapse. |
| E250 | shader source compile failure | Reserved — GPU pipeline roadmap Phase 2.1 (`.metal` → `MTLLibrary` failure surfaced as compiler diagnostic when build-time validated; runtime path remains stderr + exit) |
| E251 | pipeline state creation failure | Reserved — GPU pipeline roadmap Phase 2.1 (`MTLRenderPipelineState` construction failure with named vertex/fragment functions) |
| E252 | atlas grid metadata mismatch | Reserved — GPU pipeline roadmap Phase 3.1 (atlas declared NxM but PNG dimensions disagree) |
| E253 | render target size mismatch | Reserved — GPU pipeline roadmap Phase 4.1 (off-screen `MTLTexture` target dimensions inconsistent with declared virtual resolution) |
| E254 | effect chain target undefined | Reserved — GPU pipeline roadmap Phase 6.1 (effect referenced before its target was created) |
| E255 | scoped zone unmatched enter/exit | Reserved — GPU pipeline roadmap Phase 6.3 (zone prologue without corresponding epilogue at codegen time, e.g. early return without zone cleanup) |

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
  `docs/manual/debug_with_bug.md` for the full case study.
- **Fix.** Dynamic `page_count = ceil(mo_data_size / 16 KB)` +
  per-page start offsets in the header; dynamic `imports_off`
  that tracks the now-variable segment size.
- **Lesson recorded.** When a runtime symbol lookup fails against
  a literal the compiler definitely emitted, the first check is
  always "do the runtime bytes match the file bytes." Everything
  else is plumbing around that one question.

---

## Known compiler diagnostic gaps

Real foot-guns that have caused bugs in shipped code. No W-code
fires for them today — each is a candidate sidequest with the
right shape (small, locks a contract permanently).

### FFI float-param trap (W027 — RESOLVED 2026-05-04 by AAPCS64 refactor)

**Status: closed.** The underlying gap — `call(fp, args...)` passing
all args through GP registers regardless of type — is fixed. Both
chip backends now do compile-time type-aware arg routing:

- `a64_codegen.bsm`'s `call()` builtin emits `fpush` for float args
  and routes float args to `d_<flt_seq>` per AAPCS64 (independent
  counter from `x_<int_seq>` int args).
- `x64_codegen.bsm`'s `call()` does the same with `fpush` plus a
  post-pop rearrange that moves float bits via `MOVQ xmm, gpr` into
  the SysV xmm register bank.
- The callee prologue (`cg_emit_func` in the spine + chip
  `_emit_arg_copy_int` / `_emit_arg_copy_flt` primitives) reads
  float params from the FP bank instead of bit-casting through
  memory from a GP slot.
- The shared spine helper `cg_emit_call_arg_rearrange` extends the
  same logic to direct B++ function calls so the AAPCS64 separate-
  bank convention applies uniformly to indirect (`call()`),
  external (`cg_find_ext_by_argc`), and internal (`fidx_c >= 0`)
  call paths.

W027's diagnostic helper in `bpp_validate.bsm::_val_check_fn_ptr`
is now a no-op stub; the W-code is reserved (do not reuse) and
documented here for archaeology only.

**Trigger (historical).** A function declared with a `: float`
parameter was invoked through `call(fn_ptr_var, args...)` instead
of a direct call. The dispatch passed args via GP registers only;
the callee read the float param from FP register `d0` which
contained stale data, not the caller's value.

**Symptom.** Silent zero (or garbage) for the affected param.
Usually presents as "this code path runs but does nothing" — the
function executes, but every multiplication-by-dt collapses to
zero. Wolf3D Phase 1 Session 3 hit this when the maestro solo
callback declared `dt: float` and movement multiplied by `dt`
always produced 0 — player rotated (turn doesn't depend on dt
arithmetic in a way that exposes zero) but never walked.

**Diagnostic candidate.** Warn at `fn_ptr(name)` when `name`'s
parameter list contains any `: float` annotation. `call()` on the
resulting pointer is the trap site, but the simpler check fires
at the construction site. A more thorough version would also
flag the `call(fp, arg)` site when arg's static type is integer
and the fp's source declared float; that needs tracking the
fn_ptr's origin through the AST (more flow analysis), so start
with the construction-site warning.

**Workaround until the warning ships.** Drop `: float` from
parameters of functions registered as fn_ptr callbacks. Convert
internally:

```bpp
void my_solo(dt) {
    auto dt_s: float;
    dt_s = dt;
    dt_s = dt_s / 1000.0;
    // ... use dt_s ...
}
```

**Cross-reference.** `tonify_checklist.md` Pitfall 5 documents
the same trap from the contributor angle.

### `call(fp, ...)` arg-type mismatch with callee param type (NEW GAP after AAPCS64 refactor)

**Status: open — diagnostic candidate.**

**Trigger.** After the AAPCS64 / System V refactor that closed
W027, the `call()` builtin routes args by their compile-time
expression type into the matching ABI register bank (int → x/r
regs, float → d/xmm regs). The callee reads each param from the
bank dictated by its DECLARED type. When the two disagree —
arg's static type ≠ param's declared type — the callee reads
garbage from a stale ABI register.

**Symptom shape.** Same "stuck-looking, silent-zero" presentation
as historical W027, just inverted: callee declared `dt` (int)
receiving a float arg (caller's value lands in `d0`, callee reads
`x0`), or callee declared `dt: float` receiving an int arg
(caller's int lands in `x0`, callee reads `d0`).

**Diagnostic candidate.** Warn at the `call(fp, args...)` site
when `fp` can be statically traced back to a specific
`fn_ptr(name)` call AND any arg's compile-time type doesn't match
the corresponding declared param type of `name`. Requires light
flow analysis (track fn_ptr origin through local assignments and
extrn / global writes). Not yet implemented — assign W028 when
shipped.

**Workaround until the warning ships.** Make the caller's arg
type match the callee's declared param type:

```bpp
// Callee:
void my_solo(dt: float) { ... }

// Caller — declare local as float so SCVTF promotes int → float
// during assignment:
auto dt: float;
dt = 16;
call(fp, dt);
```

Direct calls (`my_solo(dt)` — not through `call(fp, ...)`) are
already type-aware via the existing per-call-site arg-conversion
emit in `a64_emit_node` / `x64_emit_node` (look for
`is_float_type(get_fn_par_type(fidx_c, i))` branches). The gap
exists only on the indirect `call()` path because the builtin
dispatch can't know the callee's signature without flow analysis.

**Cross-reference.** `tonify_checklist.md` Pitfall 6.

### T_BLOCK skipped by type-inference traversals (FIXED 2026-05-04)

**Trigger.** Parser-level dead-code elimination wraps the surviving
branch of `if (CONST_TRUTHY) { body }` (and `if (CONST_FALSEY) {} else { body }`)
in a `T_BLOCK` node. Three traversals in `bpp_types.bsm`
(`add_type`, `propagate_in_node`, `uses_int_ops_node`) and one
in `bpp_validate.bsm` (`val_check_node`) had no T_BLOCK case —
they fell through silently and never recursed into the body.

**Symptom.** Type inference never ran on calls inside the const-
folded block, so `arg.itype` stayed `TY_UNK` and any smart-
dispatch rewrite (notably `put` / `put_err`) routed wrong. With
a string literal arg, `put_err("...")` ended up calling
`putnum_err(addr_of_literal)`, printing the pointer address
instead of the string content.

**Fix.** Added T_BLOCK cases to all four traversals — each
recurses via `add_type_list` / `propagate_in_body` /
`uses_int_ops_list` / `val_check_list`. Bootstrap stayed
byte-stable. Locked by `tests/test_const_fold_block_dispatch.bpp`.

**Lesson recorded.** When the parser introduces a new AST node
shape (T_BLOCK was added during the if-fold work), every
recursive traversal in the compiler needs a case for it — not
just the codegen passes. The fact that codegen handled T_BLOCK
correctly hid the gap in type inference for months.

### SIGPROF firing during profile_dump (175-sample noise)

**Trigger.** After `profile_stop()` disarms ITIMER_PROF, the
subsequent `profile_dump` walk attributes a chunk of samples
(observed: ~175 in a 600-frame Wolf3D run) to whichever function
the dump itself was running in. Real signal:noise ratio in the
output drops correspondingly.

**Symptom.** A function that ran once at end-of-run shows up
high in the top-N table. In Wolf3D Phase 1 Session 5's profile,
`_wolf3d_dump_profile` ranked third with 175 samples despite
executing exactly once.

**Hypothesis.** PC resolution uses "largest-not-exceeding"
matching against the minisym table; if the timer has any
in-flight signal at disarm, OR if some samples in the ring
resolve to functions adjacent in the binary layout to
dump_profile, those samples land in the wrong bucket.
Investigation needed.

**Diagnostic candidate.** Either hardening of `profile_stop`
(sync on outstanding signals before returning), or filtering of
samples whose PC falls within `_wolf3d_dump_profile` /
`profile_dump` / `_runtime_resolve_pc` themselves.

---

## Apple-side noise (known, ignore)

External warnings that look like our bugs but originate inside
Apple frameworks. Listed here so future devs don't burn hours
hunting causes in our backend.

### `The class 'NSOpenPanel' overrides the method identifier`

Full message shape:

```
<timestamp> <binary>[pid:tid] The class 'NSOpenPanel' overrides the method identifier.
                              This method is implemented by class 'NSWindow'
```

**When it fires.** Once per process, the first time NSOpenPanel
is touched — i.e. the first call to `window_open_dialog` /
`window_save_dialog` / `window_open_folder_dialog`. ObjC runtime
realizes classes lazily; the warning is emitted by `_objc_inform`
when it notices that NSOpenPanel and NSWindow both declare
`-identifier` (via the `NSUserInterfaceItemIdentification`
protocol).

Empirically: standalone `level_editor` does NOT warn until the
user clicks Open. Bang 9 does NOT warn until Open is clicked
in the Levels tab. modulab / fxlab / games never warn (no
NSOpenPanel touch in any code path).

**Why it is NOT ours.** Verified by triangulation:

1. Zero dynamic class registration in our backend.
   `grep -rn "objc_allocateClassPair\|class_addMethod\|class_replaceMethod"`
   in `src/backend/os/macos/` returns only the source-side
   diagnostic comment at `_stb_platform_macos.bsm:741-757`.
2. Zero `identifier` selector usage anywhere in the tree.
3. Our NSOpenPanel call is vanilla:
   `objc_msgSend(objc_getClass("NSOpenPanel"), sel_registerName("openPanel"))`
   — identical pattern to any Swift / ObjC app that opens a
   file dialog.
4. Empirically reproduced in `level_editor` standalone with no
   Bang 9 involvement — same single warning line on first
   Open click.
5. **macOS 15 (Sequoia) regression**, not a long-standing
   bug. Surfaced September 2024 across the third-party app
   ecosystem that uses NSOpenPanel directly (instead of
   SwiftUI's `fileImporter` modifier which abstracts over
   it). Same warning reported in unrelated codebases —
   Deep-Live-Cam (Python+PyObjC), NoMachine native client,
   anylabeling (Python+Qt), Activity Browser, Docling,
   Scilab. Pattern: any app touching raw NSOpenPanel on
   macOS 15+ hits this regardless of language / framework.

**Why no fix.** ObjC dispatch chooses the most-specific method
via vtable normally — zero behavioural impact. Pre-warming
NSOpenPanel during startup would only relocate the warning to
a "less suspicious" moment; it adds boot cost for cosmetic gain
and would mask the message if Apple ever changes the trigger.
Suppressing via `objc_setLogFunction` is brittle and risks
hiding real warnings.

**Source-side comment.** `src/backend/os/macos/_stb_platform_macos.bsm:741-757`
holds the on-site explanation for anyone reading the dialog
wrappers and wondering.

**Triage template** for future `overrides the method X` warnings
on AppKit classes we touch: grep our backend for class
registration calls (`objc_allocateClassPair`, `class_addMethod`,
`class_replaceMethod`, `method_exchangeImplementations`). If
clean, the warning is Apple's — log it here, do not pre-warm,
do not suppress.

**Revisit when**: macOS 16 lands, or when any of the third-party
apps tracking this regression confirm Apple shipped a fix in a
macOS 15.X.Y point release. If Apple corrects the regression,
the warning vanishes on its own.
