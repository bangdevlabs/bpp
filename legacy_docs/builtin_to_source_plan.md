## Sidequest plan — registry → source migration

**Status:** PLAN ONLY (saved 2026-05-13 evening). User sleeps; we
execute together with fresh eyes.

**Companion:** WC1 mod arc Session 2 in `games/rts_1.0/wc1_handoff.md`.
Session 2 paused mid-stream (converter scanner hangs in an unrelated
bug); resumes after this sidequest closes.

---

## Why this sidequest

Today's `->` return-type cleanup (commit `67a28d5`) split B++'s
type-annotation glyphs cleanly: `:` for "X has type T" (storage),
`->` for "function returns T" (output). One canonical way per
concept.

That cleanup exposed an inconsistency one layer down: the compiler
itself uses TWO ways to declare a function's return type.

**Way 1 — source-level `->` annotation** (Tonify Rule 13, the
canonical form):

```bpp
path_asset(relpath: ptr) -> ptr { ... }
```

**Way 2 — hardcoded compiler registry** in `src/bpp_types.bsm`:

```c
if (... "arr_new"  ...) { set_func_type(fn_np, TY_ARR); }
if (... "buf_word" ...) { set_func_type(fn_np, TY_PTR); }
if (... "buf_byte" ...) { set_func_type(fn_np, TY_PTR); }
if (... "peekfloat"   ...) { return TY_FLOAT;   }
if (... "peekfloat_h" ...) { return TY_FLOAT_H; }
if (... "argv_get"    ...) { return TY_PTR;     }
if (... "envp_get"    ...) { return TY_PTR;     }
```

Way 2 is a wart for two distinct reasons that need different fixes
(the "two migrations" the audit caught):

- **Migration 1 — `arr_new` / `buf_word` / `buf_byte`** are normal
  B++ functions in `bpp_array.bsm` / `bpp_buf.bsm`. They have
  source. The registry only exists because the `->` syntax wasn't
  adopted when they were written. Pure cleanup, zero runtime
  trade-off.

- **Migration 2 — `argc_get` / `argv_get` / `envp_get`** are codegen
  builtins (no source). But they **don't need to be** — they only
  read from globals (`_bpp_argc`, `_bpp_argv`, `_bpp_envp`) that
  the chip-specific startup glue already populates. A B++ source
  function can read those globals just as well, with the only cost
  being function-call overhead (~3-5 instructions per call,
  amortised over ~10 startup-time calls = ~30 ns total). In return,
  every new backend touches 1 file (startup glue) instead of 3
  (codegen dispatch + return-type registry + per-arg machinery).

Both migrations move the codebase toward "single canonical way" and
simplify future backend ports. Together they retire ~60 LOC of
compiler-internal special-case code.

---

## What we already know (from this evening's recon)

Recon results that shape the plan — verify these still hold tomorrow
before executing:

1. **Globals already exist as plain B++ slots:**
   - `src/backend/os/macos/_core_macos.bsm:71-73` — `global _bpp_argc; global _bpp_argv; global _bpp_envp;`
   - `src/backend/os/linux/_core_linux.bsm:28-30` — same three globals
   - C emitter `bpp_emitter.bsm:1937-1939` — `_bpp_argc = (long long)argc; _bpp_argv = (long long)(uintptr_t)argv; _bpp_envp = ...`

2. **Codegen already saves arguments to those globals at main()
   prologue** — comment in `_core_macos.bsm:62-64`: "macOS uses
   LC_MAIN, so dyld calls main(argc, argv, envp) with standard
   calling convention. The codegen emits a save sequence at the
   top of main() that stores x0/x1/x2 to these globals."

3. **The current builtin path** uses dedicated machinery
   (`cg_argc_sym` / `cg_argv_sym` / `cg_envp_sym` symbols +
   `emit_load_runtime_global` per chip backend) to load these
   globals. After migration this machinery may have no remaining
   consumers and can be retired.

4. **Annotation syntax confirmed:**
   - `: ptr` → TY_PTR (parser line 989-993)
   - `: array` → TY_ARR (parser line 1046)
   - `-> ptr {` → return-type form (parser line 1656-1672, today's
     migration)
   - **NOTE:** `: arr` is NOT a valid annotation — the keyword is
     `array`. The hint table uses `TY_ARR` internally but the
     source-level glyph is `array`. Migration 1 must use
     `arr_new() -> array {` not `arr_new() -> arr {`.

5. **Registry hits to remove (full list):**
   - `bpp_types.bsm:1059, 1062, 1063` — first set_func_type ladder
     (arr_new, buf_word, buf_byte)
   - `bpp_types.bsm:1123, 1126, 1127` — second set_func_type ladder
     (same three names — duplicate code path, both ladders fire on
     the same conditions)
   - `bpp_types.bsm:248, 249` — `_builtin_ret_type` entries for
     argv_get / envp_get (added today)
   - `bpp_codegen.bsm:1619-1622, 1627-1636, 1641-1650` — the three
     builtin dispatch blocks for argc_get / argv_get / envp_get

6. **Backend-specific code that STAYS** (chip primitives that
   only the codegen can express):
   - `peek` / `poke` / `peek_w` / `poke_w` / `peekfloat` /
     `peekfloat_h` — single-instruction memory access
   - `sys_*` syscalls — trap instruction
   - The startup glue that saves x0/x1/x2 (a64) or rdi/rsi/rdx
     (x64) to `_bpp_argc/argv/envp` at main() prologue
   - `peekfloat` / `peekfloat_h` registry entries in
     `_builtin_ret_type` (lines 240-241) — these are TRUE builtins
     that emit a single LDR S/D + FCVT and have no B++ source

---

## Migration 1 plan — `arr_new` / `buf_word` / `buf_byte`

**Risk profile:** zero trade-off. Pure cleanup.

**LOC change estimate:** +3 lines source annotation, -6 lines
registry deletions = -3 net.

### Step 1.1 — Annotate `arr_new` source (~30 sec)

File: `src/bpp_array.bsm` line 40.

```diff
- arr_new() {
+ arr_new() -> array {
```

### Step 1.2 — Annotate `buf_word` / `buf_byte` source (~30 sec)

File: `src/bpp_buf.bsm` lines 203, 209.

```diff
- buf_byte(n) {
+ buf_byte(n) -> ptr {
   ...
- buf_word(n) {
+ buf_word(n) -> ptr {
```

### Step 1.3 — Delete registry entries (~1 min)

File: `src/bpp_types.bsm`. Two ladders (lines 1059-1063 and
1123-1127). Delete the three lines per ladder mentioning arr_new
/ buf_word / buf_byte. Leave any other `set_func_type` calls
intact.

**Note:** the ladders contain other entries we're NOT touching
(probably handling extern declarations from `import "X.B" {...}`
blocks). Delete only the three arr/buf lines per ladder. If after
deletion the ladder body is empty, leave the surrounding
`if (something)` block alone — it might gate other code paths.

### Step 1.4 — Bootstrap + verify

```sh
./bpp src/bpp.bpp -o /tmp/bpp_g1
/tmp/bpp_g1 src/bpp.bpp -o /tmp/bpp_g2
cmp /tmp/bpp_g1 /tmp/bpp_g2 && echo BYTE-STABLE
cp /tmp/bpp_g1 ./bpp
bash tests/run_all.sh        # expect 163/0/12
bash tests/run_all_c.sh      # expect 130/0/45
```

### Failure modes to watch for

- **Some user code passes `arr_new()` to a context expecting
  TY_PTR** (not TY_ARR). With the registry tag, the type was
  promoted via the `set_func_type` call. With `-> array`
  annotation, V3 inference might trip if downstream code does
  weird things with the result.
  - **Mitigation:** `TY_ARR` is documented as "semantic alias of
    TY_PTR for codegen purposes" (per bpp_types.bsm:270 comment).
    Should compile clean. If not: revert and investigate before
    moving forward.

- **C-emit suite regression** because the C emitter's tagging
  path differs from the native one.
  - **Mitigation:** run `tests/run_all_c.sh` immediately after
    bootstrap. If failures appear, investigate the specific
    failure before proceeding to Migration 2.

---

## Migration 2 plan — `argc_get` / `argv_get` / `envp_get`

**Risk profile:** real but bounded. Touches startup glue interface
(read side, not write side). 3 backends affected (a64 macOS, x64
Linux, C emit) but glue WRITE side stays put — only the READ side
moves from codegen to source.

**LOC change estimate:** +20 lines new source (3 source funcs in
new file or in `_core_<os>.bsm`), -33 lines codegen dispatch + 2
lines registry = ~-15 net.

### Step 2.1 — Find a home for the source functions

**Option A (simpler — picks up tomorrow):** add the three functions
to BOTH `_core_macos.bsm` AND `_core_linux.bsm` (next to the
already-defined globals). Each per-OS core file owns its own
"reader of these globals." Reads from the globals next to the
write side; cohesive.

**Option B (more general):** create `src/bpp_args.bsm` as a new
auto-injected module shared across all OSes. Single canonical
home, both macOS and Linux versions go away.

**Recommendation: Option B** — it's the principled choice (one
canonical home, not duplicated per OS), and it matches the
prelude convention (Cap 2 lists modules by purpose, not by OS).
The B++ globals are written by chip-specific glue but the READ
function is portable.

**Path:** `src/bpp_args.bsm`, ~30 LOC including header. Add to the
auto-inject prelude in `src/bpp_import.bsm` next to `bpp_io`,
`bpp_array`, etc.

### Step 2.2 — Author `bpp_args.bsm`

```bpp
// bpp_args.bsm — Process startup arguments + environment access.
//
// Reads the runtime globals (_bpp_argc / _bpp_argv / _bpp_envp)
// that the per-OS startup glue populates at program entry. The
// glue (in src/backend/os/<os>/_core_<os>.bsm and the C emitter's
// main() shim) saves the three slots; this file's three functions
// are the user-facing readers.
//
// Migrated from cg_builtin_dispatch on 2026-05-13 — see
// docs/builtin_to_source_plan.md for the rationale (one canonical
// way to declare a function's return type, fewer touch points
// per new backend port).

extrn _bpp_argc;
extrn _bpp_argv;
extrn _bpp_envp;

// Number of command-line arguments. argv[0] is the program name
// per the standard convention, so argc_get() >= 1 in any well-
// formed startup.
argc_get() -> word {
    return _bpp_argc;
}

// Pointer to the i-th argument string (null-terminated C-string),
// or 0 when i is out of range. argv_get(0) is the program name;
// argv_get(1)..argv_get(argc_get()-1) are the user-supplied
// arguments.
argv_get(i) -> ptr {
    if (i < 0)             { return 0; }
    if (i >= _bpp_argc)    { return 0; }
    return peek_w(_bpp_argv + i * 8);
}

// Pointer to the i-th environment string in the form "NAME=value",
// or 0 when i walks past the NULL terminator that ends the envp
// array. The OS-supplied envp is itself NULL-terminated so we
// don't need an explicit count to bound the walk.
envp_get(i) -> ptr {
    auto p;
    if (i < 0) { return 0; }
    p = peek_w(_bpp_envp + i * 8);
    return p;       // 0 when caller indexes past the NULL terminator
}
```

### Step 2.3 — Add to auto-inject prelude

File: `src/bpp_import.bsm`. Find the auto-inject list (the place
that pushes `bpp_io`, `bpp_array`, etc.). Add `bpp_args.bsm` to
that list.

**Look for pattern:** comment like "auto-inject these modules into
every program" or the loop that pushes the prelude module names.
Should be obvious; if not, search for `bpp_io.bsm` literal — the
new entry sits next to it.

### Step 2.4 — Delete the codegen dispatch entries

File: `src/bpp_codegen.bsm` lines 1616-1650 (the three blocks for
argc_get, argv_get, envp_get). Delete them entirely. The
surrounding code (sys_exit, sys_close, etc.) stays.

After deletion, `cg_argc_sym` / `cg_argv_sym` / `cg_envp_sym`
might have no remaining consumers. Check:

```sh
grep -n "cg_argc_sym\|cg_argv_sym\|cg_envp_sym" src/bpp_codegen.bsm
```

If only definition + initialisation remain (no reads), delete
those too. If something else still reads them (e.g. a debugging
helper), leave them alone — that's a separate cleanup.

Also check whether `emit_load_runtime_global` per-chip primitive
has other consumers. If not, it can be retired in a follow-on.
Don't bundle that with this sidequest unless trivial.

### Step 2.5 — Delete the C emitter dispatch entries

File: `src/backend/c/bpp_emitter.bsm` lines 1324-1344. Delete the
three blocks for argc_get / argv_get / envp_get. Same shape as
the codegen dispatch deletion.

After deletion, `_bpp_argc` / `_bpp_argv` / `_bpp_envp` get
emitted as plain global reads when the new source functions
compile through the C emitter. Verify the C emitter declares
these globals correctly (the comment at line 1785 says
"_bpp_argc/_bpp_argv/_bpp_envp are now declared in brt0.bsm as
B++ globals" — should still work).

### Step 2.6 — Delete the registry entries

File: `src/bpp_types.bsm` lines 248-249. Delete the two `argv_get`
/ `envp_get` lines I added today. The source functions now declare
their return type via `-> ptr` — registry tagging is redundant.

### Step 2.7 — Bootstrap (3-cycle minimum)

```sh
# Cycle 1: old bpp (knows builtins) compiles new bpp source (which
# uses the source funcs in bpp_args.bsm and has the dispatch
# entries deleted from its own codegen module). The old bpp
# emits builtins for any argc_get/argv_get/envp_get IT sees in
# bpp source — but bpp_args.bsm now has source defs that LOOK
# like calls but actually become real fn-bodies. Likely OK because
# old bpp parses the source funcs correctly and the call sites
# inside bpp_args.bsm functions (returns, peek_w) have no
# argc_get/argv_get calls in them.
./bpp src/bpp.bpp -o /tmp/bpp_g1

# Cycle 2: new bpp (no builtin dispatch, uses bpp_args.bsm)
# compiles itself.
/tmp/bpp_g1 src/bpp.bpp -o /tmp/bpp_g2
cmp /tmp/bpp_g1 /tmp/bpp_g2 && echo BYTE-STABLE   # gen1 == gen2 ideally

# If gen1 != gen2 but gen2 == gen3, that's the standard 1-cycle
# oscillation — install gen2 and verify gen2 == gen3:
/tmp/bpp_g2 src/bpp.bpp -o /tmp/bpp_g3
cmp /tmp/bpp_g2 /tmp/bpp_g3 && echo STABLE-ON-CYCLE-3

cp /tmp/bpp_g2 ./bpp
```

### Step 2.8 — Suite check

```sh
bash tests/run_all.sh           # expect 163/0/12
bash tests/run_all_c.sh         # expect 130/0/45
```

### Failure modes to watch for

- **Bootstrap breaks because the SAVE side of the startup glue
  doesn't write to globals the source funcs can read.**
  - **Diagnosis:** `tests/test_argv_*.bpp` (if exists) or write a
    tiny `argv_smoke.bpp` that prints `argv_get(0)`. Run it. If
    output is null/garbage, the save side never ran or wrote to
    a different location.
  - **Mitigation:** revert codegen dispatch deletion, investigate
    where the save actually lands, fix the global linkage.

- **C-emit suite regression** because the C emitter's old dispatch
  emitted slightly different type coercions
  (`((long long)((char**)(uintptr_t)_bpp_argv)[(int)(...)]) ` vs
  what `peek_w(_bpp_argv + i * 8)` becomes in C).
  - **Mitigation:** if a specific test fails, look at the C
    output. The new path should produce equivalent semantics
    via `peek_w` (which is itself a builtin that emits the right
    pointer-deref in C). If not, may need to keep the C-emit
    dispatch entry as a special case while removing the native
    one.

- **`peek_w` behavior on the `_bpp_argv` global.** The global
  itself is a pointer-sized word that holds the address of the
  argv array. So `_bpp_argv + i * 8` walks the argv table by 8
  bytes per entry, and `peek_w(...)` deref'd loads the i-th
  pointer entry. Verify this matches what the codegen builtin
  was doing.
  - **Sanity check:** look at the assembly the OLD builtin
    emitted vs the NEW source func. They should land at the
    same machine code modulo the function-call wrapper.

---

## Combined execution order (recommendation)

1. **Migration 1 first** (lower risk, validates the `-> array`
   syntax works, builds confidence).
2. **Bootstrap + suite** between Migration 1 and Migration 2 —
   never mix two changes in one bootstrap.
3. **Migration 2 second** with the per-step gates above.
4. **Final bootstrap + suite check.**
5. **Commit as ONE commit** ("compiler: retire builtin/registry
   special-casing for argc/argv/envp + arr_new + buf_word/byte —
   single canonical `-> ret` form per Tonify Rule 13") — both
   migrations share the principle, both close the same wart.

**Estimated time:** 1.5-2.5 hours total with you driving the
verification, accounting for the bootstrap 3-cycle on Migration 2
and one round of "investigate something unexpected" per migration.

---

## Verification gates (apply at every step)

- **Bootstrap byte-stable** — `cmp /tmp/bpp_gN /tmp/bpp_gN+1`.
  1-cycle oscillation tolerated; 2-cycle (gen2 != gen3) is a real
  bug, revert.
- **Native suite 163/0/12** — currently the floor. Any regression
  is a real failure.
- **C-emit suite 130/0/45** — same floor.
- **Smoke** — `./bpp games/rts_1.0/rts.bpp -o /tmp/rts && /tmp/rts`
  should still launch and render the WC1 forest grass + tree
  cluster (Session 1 baseline).

---

## Rollback strategy

This sidequest doesn't touch user-facing files (no game / stb /
test changes). If anything breaks badly:

```sh
git diff src/ > /tmp/sidequest.patch    # capture current state
git checkout HEAD -- src/                # revert all src/ changes
./bpp src/bpp.bpp -o /tmp/bpp_g1         # rebuild from clean source
cp /tmp/bpp_g1 ./bpp                     # restore working bpp
```

The patch in `/tmp/sidequest.patch` preserves the work for
re-investigation in a fresh session.

---

## Open questions to resolve together tomorrow

1. **`-> array` vs `-> ptr` for `arr_new`** — does V3 inference
   accept `-> array` or does the parser need `-> ptr`? Test by
   modifying just `arr_new` first and seeing if it bootstraps. If
   not, fall back to `-> ptr` (TY_ARR is a semantic alias of
   TY_PTR per the comment at bpp_types.bsm:270; nobody should
   notice the difference functionally).

2. **Option A vs Option B for `bpp_args.bsm` location** — single
   shared file (Option B, recommended) vs per-OS file (Option A).
   You may have a preference based on the import-graph shape.

3. **`emit_load_runtime_global` retirement** — separate cleanup or
   bundle? Recommend separate; this sidequest's scope is the
   builtin → source migration, not chip-primitive consolidation.

4. **Tonify Rule 13 needs an addendum** — clarify the registry-vs-
   source distinction. After Migration 2 closes, the ONLY surviving
   `_builtin_ret_type` entries are `peekfloat` / `peekfloat_h` —
   true single-instruction primitives that genuinely can't be
   source funcs. Add a note to Rule 13 (or Rule 31's neighborhood)
   distinguishing "source funcs use `-> ret`" from "true builtins
   that emit a single instruction live in the registry."

5. **how_to_dev Cap 15 (QG general/battalion)** — the four-tier
   table currently lists "inline builtins" as Tier 3. After
   Migration 2, `argc_get` / `argv_get` / `envp_get` move from
   Tier 3 to Tier 1 (data primitives, auto-injected). The Tier 3
   entry should clarify "true single-instruction builtins ONLY
   (peek, poke, syscalls, peekfloat) — readers of pre-populated
   globals are Tier 1, not Tier 3." Two-line edit, frees the
   pattern for future reviewers.

---

## After this sidequest closes

Resume **WC1 Session 2** in `games/rts_1.0/wc1_handoff.md`:

- Open bug: `tools/wc1_map_convert/wc1_map_convert.bpp` scanner
  exits silently with no JSON output. Suspect: `_skip_unrecognised_call`
  or `_handle_set_tile` loop that doesn't advance `_off`. Reproduce
  with a tiny synthetic .sms (3 SetTile calls + 1 unrecognised
  Player call) to isolate. If still hung, add `putstr` progress
  prints inside the scan loop.
- After the scanner runs cleanly, the Session 2 ship is:
  `wc1_map_load(path: ptr) -> WC1Map` reading the JSON via
  `bpp_json` + a hot-reload registration via `file_watch_register`.

The WC1 prototype exists to stress B++ end-to-end. This sidequest
is the kind of design improvement that prototypes are MEANT to
surface.
