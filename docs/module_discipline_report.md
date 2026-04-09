# B++ Language Finalization — Implementation Report

**Priority:** P0 foundation. Lands BEFORE any game goes multi-file.
**Scope:** ~120 lines across 5 files.
**Goal:** Lock down the syntax and semantics of B++ so every line of
code written from this point forward follows the final language rules.

## Context

B++ is about to grow: multi-file games (Wolf3D: 8 modules, RTS: 10),
bangscript (.bang files), and the dev loop tooling. The language syntax
and semantics must be finalized NOW so that all future code is written
in the final form. Changing the rules after thousands of lines exist
is painful. Changing them now costs ~120 lines.

This report covers 7 changes that together define how B++ looks and
feels in its final form.

## Prerequisite: Function Dedup (DONE)

Fix 1 (mod0_real_start) and Fix 1b (dedup refinement) are applied and
bootstrapped. gen1==gen2, SHA `87af9f22`. Binary at `/tmp/bpp_gen1`.

**First step: `cp /tmp/bpp_gen1 ./bpp`** to install the dedup fixes.

---

## The B++ Design Language — Final Form

```bpp
// wolf3d_ray.bsm — the language in its complete form

load "wolf3d_shared.bsm";       // project file (local only)
import "stbgame.bsm";            // engine module (stb/ or installed)

static extrn _tables;             // private + frozen after init
global ray_result;                // public + worker-shared
const MAX_DEPTH = 64;             // public + compile-time

cast(x, y, angle) {              // public, implicit return 0
    _build_tables();
    ray_result = _trace(x, y, angle);
}

static _build_tables() {          // private to this module
    _tables = malloc(256);
}

static _trace(x, y, angle) {     // private to this module
    return _tables[0] * x;        // explicit return (has value)
}

void draw_debug() {               // public, explicitly returns nothing
    render_text("DEBUG", 4, 4, 1, WHITE);
}                                  // no return needed

stub game_init() { return 0; }   // placeholder, overridden by .bpp
```

### The progressive disclosure pattern

```
Level 1 (beginner)          Level 2 (project grows)       Level 3 (expert)
─────────────────           ────────────────────           ──────────────
foo() { code; }             void foo() { code; }          foo(): base { }
auto x;                     extrn assets;                 global world;
load "mine.bsm";            static helper() { }          stub on_hit() { }
import "stb.bsm";
```

The programmer starts simple. As the project grows, the language
offers more precision. No feature is mandatory. Every feature is
there when you need it.

---

## Change 1: `load` keyword for project files (~30 lines)

### What

`load "file.bsm"` resolves ONLY in the entry file's directory. It
signals "this file is part of my project." `import` continues to
resolve in stb/ and installed paths, signaling "this is an engine
or library module."

```bpp
load "wolf3d_ray.bsm";      // project: ONLY in entry file's dir
import "stbgame.bsm";        // engine: stb/ or /usr/local/lib/bpp/
```

### Why

1. The programmer sees `load` and knows it is a project file.
2. The compiler can apply stricter rules to loaded files.
3. `load "stbgame.bsm"` fails if stbgame.bsm is not in the game dir
   — prevents accidentally treating engine code as project code.
4. Mirrors Jai's `#load` vs `#import` distinction.

### Resolution rules

| Keyword | Search path | Failure |
|---------|-------------|---------|
| `load`  | Entry file directory ONLY | Fatal error if not found |
| `import` | Same dir → `stb/` → installed | Fatal error if not found (current behavior) |

### Implementation

**File: `src/bpp_import.bsm`**

1. In `check_file_import()` (~line 600): recognize `load` alongside
   `import`. The detection is a string match on the first word:
   `load ` (5 chars) vs `import ` (7 chars). Return a flag.

2. In the line-processing loop (~line 878): when keyword is `load`,
   call `find_file` with ONLY the entry file's directory. Skip stb/
   and installed paths. If not found, fatal error.

3. New array: `auto mod_is_local;` in `init_import()`, pushed
   alongside `diag_file_starts`. Value 1 = loaded, 0 = imported.

**Estimated: ~30 lines.**

### Backwards compatibility

Existing code uses only `import`. It continues to work. `load` is
purely additive.

---

## Change 2: `static` keyword for module-private visibility (~35 lines)

### What

`static` before a function or top-level variable declaration makes it
private to the module. Cross-module access is a compile-time error.

```bpp
static _helper() { ... }         // private — only this .bsm calls it
cast(x, y) { ... }               // public (default)

static extrn tables;              // private + frozen
global ray_result;                // public + worker-shared
```

### Why

1. Familiar from C. `static` at file scope = private to the file.
2. Explicit — a code reviewer sees `static` and knows the intent.
3. Unlike `_` prefix, cannot be forgotten or overlooked.
4. Applies uniformly to functions AND variables.

### Error message

```
error[E220]: 'helper' is private to wolf3d_ray.bsm (declared static)
  --> wolf3d_hud.bsm:42
```

### Implementation

**File: `src/bpp_lexer.bsm`** (~3 lines)

Recognize `static` as a keyword (TK_KW). Same mechanism as `auto`,
`global`, `extrn`, `const`.

**File: `src/bpp_parser.bsm`** (~15 lines)

1. In `parse_function()`: if the current token is `static`, consume
   it and set a flag on the FuncRecord. New field: `FN_STATIC` in
   the node layout (or reuse a bit in `FN_TYPE`).

2. In `parse_global()` / `parse_global_simple()`: if preceded by
   `static`, set a flag on the global entry. New array:
   `glob_static[]` parallel to `glob_names[]`.

3. In `parse_program()` / `parse_module()`: when the token is
   `static`, peek ahead: if followed by identifier + `(`, it is a
   static function. If followed by `auto`/`global`/`extrn`, it is a
   static variable. Dispatch accordingly.

**File: `src/bpp_validate.bsm`** (~15 lines)

In the call validation pass: if the callee has FN_STATIC set and
`func_mods[callee] != func_mods[caller]`, emit E220.

In the global access pass: same check for globals with static flag.

**File: `src/bpp_defs.bsm`** (~2 lines)

Add `FN_STATIC` offset to the FuncRecord layout.

**Estimated: ~35 lines total.**

### Constraint: `static` + `stub` is an error

`stub` exists to be overridden from outside the module. `static`
prevents outside access. Together they contradict.

```
error: 'static' and 'stub' cannot be combined
```

~2 lines in the parser.

---

## Change 3: `void` keyword for functions (~20 lines)

### What

`void` before a function name declares that the function does not
return a useful value. The compiler enforces this in two ways:

1. A `void` function does not need `return`. If it has one, it must
   be bare `return;` or `return 0;` — not `return expr;` with a
   non-zero expression.
2. Assigning the result of a `void` function to a variable is a
   warning.

```bpp
void draw_hud() {
    render_text("SCORE", 4, 4, 1, WHITE);
}                                  // no return needed — compiler is happy

auto x;
x = draw_hud();                   // warning[W017]: 'draw_hud' is void
```

### Why

1. Finalizes the function syntax: `name()`, `void name()`, and
   `stub name()` cover all cases.
2. Communicates intent: the programmer says "this returns nothing"
   and the compiler verifies it.
3. Progressive disclosure: beginners skip it, experienced programmers
   use it for safety.
4. Enables future W016 analysis: functions with mixed return paths
   (some return values, some don't) can be flagged.

### Implementation

**File: `src/bpp_lexer.bsm`** (~2 lines)

Recognize `void` as a keyword.

**File: `src/bpp_parser.bsm`** (~10 lines)

1. In `parse_function()`: if the current token is `void`, consume it
   and set `FN_VOID` flag on the FuncRecord.
2. `void` + `static` can combine: `static void draw_debug() { }`.
3. `void` + `stub` can combine: `void stub game_draw() { return 0; }`.

**File: `src/bpp_validate.bsm`** (~8 lines)

1. If a void function has `return expr;` where expr is not 0 or
   absent, emit warning.
2. If any call site does `x = void_func();`, emit W017.

**File: `src/bpp_defs.bsm`** (~1 line)

Add `FN_VOID` offset.

**Estimated: ~20 lines total.**

---

## Change 4: Implicit return 0 (~15 lines)

### What

If a function body does not end with a `return` statement, the
compiler inserts `return 0;` automatically. This applies to ALL
functions (void and non-void).

```bpp
// Before (today):
init_game() {
    world = ecs_new(200);
    return 0;                      // boilerplate
}

// After:
init_game() {
    world = ecs_new(200);
}                                  // compiler inserts return 0
```

### Why

Every game function (init, update, draw, callbacks) ends with
`return 0;` today. It is pure ceremony. Implicit return eliminates
it without breaking any existing code (explicit `return 0;` still
works).

### Implementation

**File: `src/aarch64/a64_codegen.bsm`** (~5 lines)

After emitting the function body, check if the last emitted statement
was a return instruction. If not, emit `mov x0, #0` + `ret`.

**File: `src/x86_64/x64_codegen.bsm`** (~5 lines)

Same: emit `xor eax, eax` + `ret` if no return was emitted.

**File: `src/bpp_emitter.bsm`** (~5 lines)

C emitter: emit `return 0;` before the closing `}` if the last
statement is not a return.

**Estimated: ~15 lines total.**

### Backwards compatibility

100% compatible. Existing `return 0;` statements continue to work.
The implicit return only activates when there is no explicit return
at the end.

---

## Change 5: Cross-module duplicate = fatal error (~5 lines)

### What

Two different non-entry modules defining the same function with real
bodies is a fatal error, not a warning.

```
error[E221]: duplicate definition of 'init'
  --> wolf3d_enemy.bsm:5 and wolf3d_ray.bsm:3
  | rename to enemy_init / ray_init
```

### Prerequisite

The 2 existing W015 warnings (`packed_eq`, `str_eq`) must be resolved
first. Investigate which modules define them, deduplicate, then make
the check fatal.

### Implementation

**File: `src/bpp_parser.bsm`** (~5 lines)

In the dedup code, change the cross-module non-entry case from
`diag_warn(15)` to `diag_fatal(21)` + `sys_exit(1)`.

**Exceptions remain:**
- Entry file (module 0) overrides any module: silent (callback pattern).
- Same-module replacement: silent (forward-declaration pattern).

---

## Change 6: Circular import = fatal error (~20 lines)

### What

If the module dependency graph has a cycle, fatal error.

```
error[E222]: circular dependency between modules
  wolf3d_enemy.bsm imports wolf3d_player.bsm
  wolf3d_player.bsm imports wolf3d_enemy.bsm
  | extract shared functions to a third module
```

### Implementation

**File: `src/bpp_import.bsm`** (~20 lines)

After `topo_sort()` completes, check if all modules were visited.
If `arr_len(mod_topo) < arr_len(diag_file_starts)`, there is a cycle.
Print the unvisited modules and exit.

---

## Change 7: `main()` in `.bsm` = fatal error (~8 lines)

### What

Only the entry file (.bpp, module 0) can define `main()`.

```
error[E223]: main() is not allowed in module files (.bsm)
  --> wolf3d_ray.bsm:1
```

### Implementation

**File: `src/bpp_parser.bsm`** (~8 lines)

At the end of `parse_function()`, if the function name is `main` and
`my_mod != 0`, emit E223 and exit.

---

## What's ALREADY DONE (from the current session)

These items from the original report were implemented and verified during the
0.22 smart-dispatch session. They do NOT need to be re-done:

| Item | Status | How |
|------|--------|-----|
| Prerequisite: function dedup | ✅ DONE | Fix 1 + Fix 1b + stub keyword |
| packed_eq / str_eq collisions | ✅ DONE | Renamed str_eq → emit_name_eq, deleted duplicate packed_eq. 0 W015 warnings. |
| main in .bsm = error (Change 7) | ✅ DONE | E105 via `_parser_is_entry` flag. Works in both pipelines. |

The agent implementing the remaining changes should start from bootstrap
`c95203cfe459d09bd57c38eb761bb6b3b1e501a0` (50 passed, 0 failed, 11 skipped).

---

## Summary Table

| # | Change | Lines | Keywords | Error codes | Status |
|---|--------|-------|----------|-------------|--------|
| 1 | `load` keyword | ~30 | `load` | — | ⬜ TODO |
| 2 | `static` keyword | ~35 | `static` | E220 | ⬜ TODO |
| 3 | `void` keyword | ~20 | `void` | W017 | ⬜ TODO |
| 4 | Implicit return 0 | ~15 | — | — | ⬜ TODO |
| 5 | Cross-module dup = error | ~5 | — | E221 | ⬜ TODO (upgrade W015→fatal) |
| 6 | Circular import = error | ~20 | — | E222 | ⬜ TODO |
| 7 | main in .bsm = error | ~8 | — | E223/E105 | ✅ DONE |
| 8 | Arg count check | ~20 | — | W018 | ⬜ TODO |
| 9 | `: base` / `: solo` annotations | ~40 | — | W013 | ⬜ TODO |
| **Total remaining** | | **~185** | **3 new** | **5 new** | |

---

## Change 8: Argument count check (W018)

### What

When a function is called with a different number of arguments than its
definition specifies, emit a warning. The compiler has this information
from `FN_PCNT` in the function record and `n.c` (arg count) in the
T_CALL AST node.

```
warning[W018]: 'add' expects 2 arguments, got 1
  --> game.bpp:5
```

### Why

B++ inherits from B/C where `foo()` meant "unknown parameters." B++ does
whole-program compilation, so `funcs[]` always has the complete definition.
The information exists — we just need to compare at call sites.

Without this check, `add(10)` compiles silently and `b` gets garbage from
whatever was in register x1. This is the same class of bug that C's
`(void)` was designed to prevent.

### Implementation

**File: `src/bpp_validate.bsm`** (~20 lines)

In the T_CALL validation path, after verifying the function exists:

```bpp
// Compare call-site arg count vs definition param count.
auto call_argc, def_argc;
call_argc = n.c;            // arg count from the T_CALL node
def_argc = *(funcs[fi] + FN_PCNT);
if (call_argc != def_argc) {
    diag_warn(18);
    diag_str("'");
    diag_packed(n.a);
    diag_str("' expects ");
    diag_int(def_argc);
    diag_str(" arguments, got ");
    diag_int(call_argc);
    diag_newline();
    diag_loc(tok_pos);
}
```

**Exception:** Variadic builtins (putchar, sys_write, etc.) don't have
entries in `funcs[]` — they're handled by `val_is_builtin()` and should
be skipped. The check only fires for user-defined functions found via
`val_find_func()`.

**Estimated: ~20 lines.**

---

## Change 9: Phase annotations `: base` / `: solo` (W013)

### What

Functions can be annotated with `: base` (pure, worker-safe) or `: solo`
(sequential, main-thread) between the closing `)` and the opening `{`.
The compiler verifies the annotation against `fn_phase[]` (already
computed by `classify_all_functions()` which runs on every compilation).

```bpp
// Programmer asserts: this function is pure
particle_update(i): base {
    ecs_set_pos(world, i, new_x, new_y);
}

// Programmer asserts: this must run sequentially
handle_input(dt): solo {
    if (key_down(KEY_UP)) { facing = DIR_UP; }
}

// No annotation: compiler classifies automatically (current behavior)
helper(x) {
    return x * 2;
}
```

### The B++ pattern

This follows the same "simple by default, explicit when needed" philosophy:

| Level | What the programmer writes | What happens |
|-------|---------------------------|--------------|
| Beginner | `foo() { }` | Compiler auto-classifies via fn_phase[] |
| Intermediate | `foo(): base { }` | Compiler verifies — W013 if wrong |
| Expert | `foo(): solo { }` | Compiler respects — forces sequential |

Same pattern as `auto x;` (inferred) → `extrn x;` (explicit).

### Implementation

**File: `src/bpp_parser.bsm`** (~25 lines)

In `parse_function()`, between consuming `)` and consuming `{`, check
for optional `: base` or `: solo` annotation:

```bpp
// After consuming ')':
auto phase_hint;
phase_hint = 0;  // 0 = auto, 1 = base, 2 = solo
if (val_is_ch(':')) {
    consume();
    if (val_is("base", 4)) { phase_hint = 1; consume(); }
    if (val_is("solo", 4)) { phase_hint = 2; consume(); }
}
consume();  // consume '{'
```

Store `phase_hint` in a new parallel array `fn_phase_hint[]` (alongside
`fn_phase[]`).

**File: `src/bpp_validate.bsm`** (~15 lines)

After `classify_all_functions()` runs, compare `fn_phase_hint[i]` vs
`fn_phase[i]`:

```bpp
if (fn_phase_hint[i] == PHASE_BASE) {
    if (fn_phase[i] == PHASE_SOLO) {
        diag_warn(13);
        diag_str("function annotated as ':base' but has side effects");
        // ...
    }
}
// : solo is always valid — it's a programmer override
```

**Estimated: ~40 lines.**

### Interaction with smart dispatch codegen (future)

When the smart dispatch codegen eventually rewrites parallel-safe loops
to `job_parallel_for`, it will only auto-dispatch loops whose body calls
PHASE_BASE functions. The `: base` annotation lets the programmer
explicitly declare intent, which gives the codegen confidence to dispatch
without needing to prove purity transitively.

---

## The Complete B++ Keyword Table (after all changes)

| Keyword | Category | Meaning |
|---------|----------|---------|
| `auto` | storage | Serial variable (default, or smart-promoted) |
| `global` | storage | Worker-shared variable |
| `extrn` | storage | Frozen after init |
| `const` | storage | Compile-time constant |
| `static` | visibility | Private to this module |
| `void` | return | Function returns nothing useful |
| `stub` | override | Placeholder, meant to be replaced by entry file |
| `load` | import | Project file (local directory only) |
| `import` | import | Engine/library module (stb/ or installed) |
| `struct` | type | Struct definition |
| `enum` | type | Enum definition |
| `if` / `else` | control | Conditional |
| `for` / `while` | control | Loop |
| `break` | control | Exit loop |
| `continue` | control | Skip to next iteration |
| `return` | control | Return from function |
| `byte` / `half` / `quarter` | type hint | Sub-word slice annotation |

Annotations (not keywords, use existing `:` syntax):
| Annotation | Where | Meaning |
|------------|-------|---------|
| `: base` | after function params | Pure, worker-safe (compiler verifies) |
| `: solo` | after function params | Sequential, main-thread (programmer override) |
| `: serial` | after global name | Prevent auto promotion (escape hatch) |
| `: byte` / `: float` etc | after variable name | Type/size hint |

3 new keywords (`static`, `void`, `load`). 22 keywords + 6 annotations total.
The language syntax is COMPLETE for 1.0.

## Implementation Order (updated)

```
ALREADY DONE:
  ✅ Function dedup (Fix 1 + stub keyword)
  ✅ packed_eq / str_eq collision fixes
  ✅ main in .bsm = error (E105 via _parser_is_entry)

REMAINING (start from bootstrap c95203cf):
  Step 1:  Change 4 — implicit return 0         (~15 lines, safest first)
  Step 2:  Change 8 — arg count check W018       (~20 lines, safety)
  Step 3:  Change 1 — load keyword               (~30 lines, new feature)
  Step 4:  Change 2 — static keyword             (~35 lines, new feature)
  Step 5:  Change 3 — void keyword               (~20 lines, new feature)
  Step 6:  Change 9 — : base / : solo            (~40 lines, annotations)
  Step 7:  Change 5 — cross-module dup = fatal    (~5 lines, upgrade W015)
  Step 8:  Change 6 — circular import = fatal     (~20 lines, safety)
  Step 9:  Bootstrap gen1 → gen2 → verify
  Step 10: Full test suite
  Step 11: Update bootprod_manual.md + docs
```

**ONE CHANGE AT A TIME.** Bootstrap after each:
```bash
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2)   # must be empty
cp /tmp/bpp_gen1 ./bpp
```

## Testing

After all changes:

```bash
# 1. Existing test suite must pass.
BPP=./bpp bash tests/run_all.sh

# 2. Test load keyword.
mkdir -p /tmp/test_mod
echo 'helper() { return 42; }' > /tmp/test_mod/helper.bsm
echo 'load "helper.bsm"; main() { return helper(); }' > /tmp/test_mod/game.bpp
./bpp /tmp/test_mod/game.bpp -o /tmp/test_mod/game
/tmp/test_mod/game; echo $?   # should print 42

# 3. Test load fails for non-local file.
echo 'load "stbgame.bsm"; main() { return 0; }' > /tmp/test_badload.bpp
./bpp /tmp/test_badload.bpp 2>&1   # should error: not found

# 4. Test static enforcement.
echo 'static secret() { return 1; }' > /tmp/test_mod/helper.bsm
echo 'load "helper.bsm"; main() { return secret(); }' > /tmp/test_mod/game.bpp
./bpp /tmp/test_mod/game.bpp 2>&1   # should error: E220 private

# 5. Test void keyword.
echo 'void noop() { } main() { auto x; x = noop(); return x; }' > /tmp/test_void.bpp
./bpp /tmp/test_void.bpp 2>&1   # should warn: W017 void used as value

# 6. Test implicit return.
echo 'main() { putchar(72); putchar(10); }' > /tmp/test_impl.bpp
./bpp /tmp/test_impl.bpp -o /tmp/test_impl
/tmp/test_impl   # should print H

# 7. Test cross-module dup.
echo 'init() { return 1; }' > /tmp/test_mod/a.bsm
echo 'init() { return 2; }' > /tmp/test_mod/b.bsm
cat > /tmp/test_mod/game.bpp << 'EOF'
load "a.bsm";
load "b.bsm";
main() { return init(); }
EOF
./bpp /tmp/test_mod/game.bpp 2>&1   # should error: E221

# 8. Test main in .bsm.
echo 'main() { return 0; }' > /tmp/test_mod/helper.bsm
echo 'load "helper.bsm"; main() { return 0; }' > /tmp/test_mod/game.bpp
./bpp /tmp/test_mod/game.bpp 2>&1   # should error: E223

# 9. Test static + stub rejected.
echo 'static stub foo() { return 0; }' > /tmp/test_mod/helper.bsm
echo 'load "helper.bsm"; main() { return 0; }' > /tmp/test_mod/game.bpp
./bpp /tmp/test_mod/game.bpp 2>&1   # should error: static + stub
```

## Files Touched

| File | Changes |
|------|---------|
| `src/bpp_lexer.bsm` | `static`, `void` as keywords (~5 lines) |
| `src/bpp_parser.bsm` | `static`/`void`/`load` parsing, cross-dup fatal, main check (~45 lines) |
| `src/bpp_validate.bsm` | `static` cross-module check, `void` return check (~25 lines) |
| `src/bpp_import.bsm` | `load` resolution, `mod_is_local`, circular check (~50 lines) |
| `src/bpp_defs.bsm` | `FN_STATIC`, `FN_VOID` field offsets (~3 lines) |
| `src/aarch64/a64_codegen.bsm` | Implicit return 0 (~5 lines) |
| `src/x86_64/x64_codegen.bsm` | Implicit return 0 (~5 lines) |
| `src/bpp_emitter.bsm` | Implicit return 0 in C output (~5 lines) |

## Future (Post-1.0, documented but NOT implemented now)

### W016: Mixed return path analysis

When the smart dispatch call graph is running, analyze functions:
- Has SOME `return X` where X != 0? → "value function"
- Has code paths without explicit return? → W016 warning

```
warning[W016]: 'get_damage' has return values [50, 10] but
  also reaches end of function without explicit return
  --> enemy.bsm:15
  | add a default return or mark as void
```

Lands alongside smart dispatch (P0 step 5). ~40 lines in dispatch.

### `: base` / `: solo` phase annotations on functions

Expert-level maestro phase control. A function annotated `: base`
is guaranteed pure (worker-safe). `: solo` is serial-only. This is
the Maestro Plan Phase 2 annotation system.

```bpp
update_physics(): base {     // compiler verifies purity
    // only reads extrn, writes global with stride
}
```

Lands during Maestro Phase 2 (P0 step 5).
