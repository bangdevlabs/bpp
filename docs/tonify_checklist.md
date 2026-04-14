# Tonify Checklist — Expert Mode

Apply this checklist to EVERY `.bsm` and `.bpp` file during the tonification
sweep. Each rule references `bpp_language_reference.md`. Go top to bottom.

## Per-file procedure

1. Read the entire file
2. Apply each rule below in order
3. Compile: `./bpp src/bpp.bpp -o /tmp/bpp_verify`
4. If bootstrap breaks, revert last change and investigate
5. Move to next file

---

## Rule 0: Module discipline (`load` vs `import` + `stub`)

For EVERY `import` in a game file (`.bpp` or game `.bsm`):

| Pattern | Action |
|---------|--------|
| Imported file lives in the SAME directory as the entry `.bpp` | Change to `load` |
| Imported file is from `stb/`, `src/`, or an installed path | Keep `import` |

For EVERY callback function in engine modules (e.g. stbgame.bsm):

| Pattern | Action |
|---------|--------|
| Function exists to be overridden by the entry `.bpp` | Add `stub`: `stub game_init() { }` |
| Function is real implementation, not meant to be overridden | Keep as-is |

**Applies to batch 4 (stbgame callbacks) and batch 7 (game files).** Batches 1-3 and
5-6 use `import` exclusively — compiler internals never use `load`.

## Rule 1: Storage classes (top-level variables)

For EVERY `auto x;` at file scope, decide:

| Pattern | Change to | Example |
|---------|-----------|---------|
| Set once in init, never written again | `extrn x;` | `extrn _ARR_HDR;` |
| Read/written by worker functions | `global x;` | `global _last_dt;` |
| Compile-time literal | `const X = value;` | `const CELL = 10;` |
| Intentionally serial, don't promote | `auto x: serial;` | `auto _temp: serial;` |
| Mutable per-frame state | `auto x;` (keep) | `auto head_x;` |

## Rule 2: Visibility (`static`)

For EVERY function and variable in the file — not just `_` prefixed ones:

| Pattern | Action |
|---------|--------|
| Function called only within this file | Add `static` (and `_` prefix if missing) |
| Function called cross-module | Keep public — `static` breaks it |
| Function not called today but is public API by design | Keep public — it exists for user programs |
| Variable used only in this file | Add `static` |
| Variable used cross-module | Keep public |

**The `_` prefix is a convention, not the rule.** A function named `helper()` that
is only called inside its own file is just as private as `_helper()`. Conversely,
`_stb_get_time()` is called cross-module despite the prefix — grep decides, not naming.

**Check EVERY function via grep:** `grep -rn "function_name" src/ stb/ games/ tests/`
to verify whether any cross-module caller exists. Do not skip non-`_` names.

## Rule 3: Return type (`void` + implicit return + bare `return;`)

For EVERY function:

| Pattern | Action |
|---------|--------|
| Returns meaningful value (`return x + y;`) | Keep as-is |
| Only `return 0;` at the end (side-effect function) | Remove `return 0;` + add `void` |
| `void` function with early-exit `return 0;` in the middle | Change to bare `return;` |
| Mixed: some paths return value, some don't | Keep explicit returns, no `void` |
| `return 0;` is the ONLY statement | Remove it (implicit return handles it) |

Bare `return;` (no expression) is supported in B++ and produces an implicit
`return 0`. Use it in `void` functions for early-exit guards instead of
the misleading `return 0;`.

## Rule 4: Phase annotations (`: base` / `: solo`)

For functions where the intent is clear:

| Pattern | Action |
|---------|--------|
| Pure function: reads args, computes, returns value. No global writes. | Add `: base` |
| Side-effect function: writes globals, calls impure functions | Add `: solo` (optional — compiler infers) |
| Unclear / mixed | Leave unannotated (compiler auto-classifies) |

**Only annotate when the intent is OBVIOUS.** Don't guess. The compiler classifies automatically for unannotated functions.

**WARNING: do NOT mark `: base` on functions that call builtins** (malloc, free, putchar, str_peek, envp_get, sys_*, etc.). The classifier treats ALL builtins as impure. Only pure pointer-arithmetic readers qualify (arr_get, arr_len, etc.). W013 will catch mistakes — trust it.

## Rule 5: Control flow (`continue` + `for` + `switch`)

### `switch` (value dispatch + condition dispatch)

For if-chains that test ONE variable against MULTIPLE constants:

| Pattern | Action |
|---------|--------|
| 3+ sequential `if (t == X) { ... } if (t == Y) { ... }` | Convert to `switch (t) { X { ... } Y { ... } else { } }` |
| 3+ sequential `if (cond1) { } else if (cond2) { }` with DIFFERENT conditions | Convert to `switch { cond1 { } cond2 { } else { } }` |
| `if/else` with only 2 branches | Keep `if/else` — switch is overkill |

Two forms:
```bpp
// Form 1: value dispatch — tests expr against constants
switch (state) {
    IDLE, PATROL { move(); }
    ATTACK       { fire(); }
    else         { }
}

// Form 2: condition dispatch — first truthy arm wins
switch {
    can_attack(e)  { attack(e); }
    can_see(p)     { chase(p); }
    else           { idle(); }
}
```

W021 warning if no `else` arm (exhaustiveness hint).

### Loop cleanup

| Pattern | Action |
|---------|--------|
| `while` with manual counter (`i = 0; while (i < n) { ... i = i + 1; }`) | Convert to `for` |
| `if (skip_condition) { i = i + 1; } else { ... real work ... }` | Use `continue`: `if (skip) { continue; } ... real work ...` |
| Deeply nested if/else that could be flattened with early-continue | Refactor with `continue` |

## Rule 6: Slice types (struct fields)

For struct definitions with fields that are smaller than a word:

| Field range | Annotation | Example |
|-------------|-----------|---------|
| 0-1 (single flag) | `: bit` | `alive: bit` (Phase A1) |
| 0-7 (small enum, 3 bits) | `: bit3` | `direction: bit3` (Phase A1) |
| 0-15 (larger enum, 4 bits) | `: bit4` | `level: bit4` (Phase A1) |
| 0-255 (boolean, flags, small count) | `: byte` | `on_ground: byte` |
| 0-65535 (moderate count, dimensions) | `: quarter` | `w: quarter, h: quarter` |
| Signed 32-bit (physics values, accelerations) | `: half` | `gravity: half` |
| Full 64-bit (positions, pointers) | (none) | `x, y` |
| 128-bit SIMD register (4× float32) | `: double` | `angles: double` (Phase B4) |

Bit-sliced fields pack LSB-first into the current byte. When a bit
field overflows the current byte, a new byte starts. Non-bit fields
terminate bit-packing (remaining bits in the last byte are padding).

## Rule 7: Comments

| Pattern | Action |
|---------|--------|
| Comment says `return 0` after removal | Delete the comment too |
| Comment mentions old name (stbarena, stbio) | Update to new name |
| Comment is accurate | Keep |

## Rule 8: Typed struct access for sliced types

Raw offset access `*(ptr + N)` does NOT respect sliced struct layouts.
B++ packs sliced fields without alignment padding, so the offset you
would guess from declaration order is wrong. When reading or writing a
struct field, declare the pointer with its struct type and use dot
notation — the compiler emits the real packed offset.

| Pattern | Action |
|---------|--------|
| `*(node + 8)` with hardcoded offset on Node (sliced) | Convert to typed: `auto n: Node; n = node; n.a` |
| `auto p;` where p points to a struct | Annotate: `auto p: StructName;` |
| Stale offset constants from pre-slicing era (ND_ITYPE, etc.) | Delete if unused |

**Why.** Raw `*(ptr + N)` emits a blind 8-byte load at `ptr + N`.
Typed access calls `get_field_offset` at codegen time to compute the
actual packed offset. Example: in the Node struct, `.a` lives at byte
offset 1 (not 8) because `ntype: byte` consumes 1 byte with no
padding after it. Any code that hardcodes `*(node + 8)` reads bytes
straddling the end of `.a` and the start of `.b` — garbage.

**Escape hatch.** Manual records (RECORD_SZ, token slots, function
records) are unsliced by design and use named offset constants like
`FN_NAME = 8`. Those are word-aligned layouts independent of the
sliced-struct system. Rule 8 applies ONLY to sliced structs.

See `~/.claude/projects/-Users-Codes-b--/memory/feedback_sliced_struct_access.md`
for the full incident history (DCE and jump-table bugs caused by this
exact pattern).

---

## Known pitfalls (discovered during batch 2)

These are compiler bugs or limitations that affect tonification. They are
documented here so nobody hits them again. Each one has a corresponding
diagnostic code in `warning_error_log.md`.

### Pitfall 1: `static const` does not work at file scope

`static const X = 16;` compiles without error but the value is **0 at
runtime**. The `const` inlining does not fire when combined with `static`.
This causes silent bugs: variables that should be 16 are 0, leading to
division by zero, null pointer dereference, etc.

**Workaround**: use `auto X;` with assignment in the init function.
**Diagnostic**: E230 (fatal error if `static const` is used at file scope).
**Fix needed**: the parser/codegen must handle `static` + `const` together.

### Pitfall 2: 1-cycle bootstrap oscillation is normal after codegen changes

If gen1 != gen2 but gen2 == gen3, the bootstrap has a **1-cycle
oscillation** — expected after any codegen refactor. The old compiler
emits the old code pattern into gen1; gen1 (with new codegen logic)
re-emits its own source using the new pattern, producing gen2; gen2
and gen3 both run new-pattern codegen so they match. Install gen3 and
move on.

If gen2 != gen3 (the new compiler does NOT produce the same output
when compiling itself), that is a real codegen bug. Diagnose with
`--show-deps` and by byte-diffing gen2 vs gen3 around specific
functions. The module cache was removed in 0.23.x, so stale-cache is
no longer a suspect — every compilation is from source.

**Fix**: `./bpp --clean-cache`, then re-run the bootstrap. If gen1 == gen2
after clean-cache, the oscillation was stale cache — not a real issue.
See `bootprod_manual.md` for the full debugging procedure.

`static auto` on file-scope variables in auto-injected modules (beat,
job, maestro) is safe — batch 1 modules (array, str, math) already use
it without issues.

### Pitfall 3: `codesign` on `./bpp` corrupts the cache

Running `codesign -s - ./bpp` changes the binary's bytes, which changes
the `compiler_hash` used in cache keys. All existing `.bo` files become
keyed to the old hash, causing silent stale-cache bugs that manifest as
2-cycle bootstrap oscillation. See `bootprod_manual.md` for full details.

**Rule**: NEVER run `codesign` on the `./bpp` binary.

---

## File order (leaves first, complex last)

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

Batch 3 — Platform layers:
  src/aarch64/_stb_core_macos.bsm
  src/aarch64/_stb_platform_macos.bsm

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

Batch 5 — Compiler internals:
  src/bpp_defs.bsm
  src/bpp_internal.bsm
  src/bpp_diag.bsm
  src/bpp_lexer.bsm
  src/bpp_parser.bsm
  src/bpp_types.bsm
  src/bpp_dispatch.bsm
  src/bpp_validate.bsm
  src/bpp_emitter.bsm
  src/bpp_import.bsm
  src/bpp_bo.bsm
  src/bpp_bug.bsm

Batch 6 — Codegens:
  src/aarch64/a64_enc.bsm
  src/aarch64/a64_codegen.bsm
  src/aarch64/a64_macho.bsm
  src/x86_64/x64_enc.bsm
  src/x86_64/x64_codegen.bsm
  src/x86_64/x64_elf.bsm

Batch 7 — Entry points + games:
  src/bpp.bpp
  src/bug.bpp
  games/snake/snake_maestro.bpp
  games/snake/snake_particles.bsm
  games/snake/snake_gpu.bpp
  games/pathfind/pathfind.bpp

RECHECK after each batch (before bootstrap):
  - Zero bare `auto` at file scope (should be extrn/global/const/static auto)
  - Zero functions used only in their own file without `static` (grep every function, not just `_` prefixed)
  - Zero trailing `return 0;` in `void` functions
  - Zero side-effect-only functions missing `void` (grep for functions where the only return is `return 0;` at the end)
  - Zero `import` in game files where `load` applies (batch 4+7 only)
  - Zero override callbacks in engine modules without `stub` (batch 4 only)
  - All relevant tests pass individually
  - Zero warnings during compilation

Bootstrap after EACH BATCH:
  ./bpp --clean-cache
  ./bpp src/bpp.bpp -o /tmp/bpp_verify
  /tmp/bpp_verify --clean-cache
  /tmp/bpp_verify src/bpp.bpp -o /tmp/bpp_verify2
  shasum /tmp/bpp_verify /tmp/bpp_verify2    # must match
  cp /tmp/bpp_verify2 ./bpp
  ./tests/run_all.sh                         # must be 50/0/11
```
