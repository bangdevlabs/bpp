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
| Compile-time literal (inline, no slot) | `const X = value;` | `const CELL = 10;` |
| Cross-module immutable, **needs a slot** | `global const X = value;` | `global const SCREEN_W = 320;` |
| Module-private immutable, **needs a slot** | `static const X = value;` | `static const _MX_MAX_VOICES = 10;` |
| Intentionally serial, don't promote | `auto x: serial;` | `auto _temp: serial;` |
| Mutable per-frame state | `auto x;` (keep) | `auto head_x;` |

**`const` vs `global const` vs `static const` — when does each apply.**

`const X = value;` (no qualifier) is **compile-time literal substitution**.
The parser inlines the value at every use site; no symbol is emitted into
the data section, no `extrn X;` from another module can resolve to it.
This is the cheapest form and the right choice for *most* constants.

`global const X = value;` emits a real **read-only slot** into `.data`
with a global symbol. Other modules can `extrn X;` it, and the compiler
rejects writes (E263). Use when a constant has to be addressable from a
different module — typically because the consumer is upstream of the
declaration in the call graph and cannot inline.

`static const X = value;` is the same shape as `global const` but the
symbol stays module-private. Use when a constant needs an addressable
slot but should not leak across modules.

The trap that motivated the dedicated slot variants (sidequest 2026-05-14):
`stb/stbrender.bsm` declares `extrn SCREEN_W;` and dereferences it inside
`render_init()`. A consumer game writing `const SCREEN_W = 320;` does NOT
satisfy that extrn — the const is parser-level only. The fix is `global
const SCREEN_W = 320;` (or, more commonly, importing `stbgame` /
`stbwindow` which already define the slot via their own write-once init).
The compiler now points at this exact distinction via E264 when the
extrn fails to resolve.

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

## Rule 4: Phase annotations — `@safe` + `@profile` + default

> **SHIPPED 2026-05-11** — collapse from 8-phase lattice to 2-state
> user-facing model. Commits: `842212f` (introduce `@safe`),
> `60b1d8b` (stb sweep), `66e6da1` (compiler internals), `a17eff9`
> (apps), `7e528d4` (parser strict-mode + docs rewrite), `e1b16a1`
> (cleanup + E260 diagnostic). Net codebase change: 700+ annotation
> sites removed across 95 files; ~80 lines of dead enforcement
> stripped from `bpp_dispatch.bsm`; new E260 directs muscle-memory
> users at the new model.


Two user-facing annotations earn their keep; everything else is
default (no annotation = full power, programmer's responsibility).

| Annotation | Use case | Compiler enforces |
|-----------|----------|-------------------|
| `@safe` | Audio callbacks, worker entry points, any bounded path that MUST stay bounded | YES — W026 fires if call graph reaches malloc / IO / GPU / syscall / blocking |
| `@profile("name")` | Scoped instrumentation (Phase 6.3) | NO — metadata only, drives `_prof_save_enter` / `_prof_save_drain` codegen |
| (none) | Default — full power, programmer responsibility | NO |

The annotation is an `@` sigil glued to the keyword with no space,
placed between the parameter list and the function body:

```bpp
// Audio callback: bounded time, no malloc, no IO, no syscall.
// W026 fires if any of those leak in via the call graph.
static _aud_square_cb(user, queue, buffer)@safe {
    // ...
}

// Worker thread entry point: same enforcement, same use case.
worker_step(state)@safe {
    // ...
}
```

The killer use case: an audio callback annotated `@safe` is proven by
the compiler to never call `malloc`, `putchar`, or any other side-
effecting builtin through its transitive call graph. Eliminates an
entire class of audible glitches by proof rather than testing.

### When to annotate

Annotate `@safe` ONLY when the function MUST satisfy the safety
contract (audio callbacks, multi-core worker entries, hard real-time
paths). Don't annotate "for documentation" — comments are for
documentation. Per Rule 28: annotations earn their keep ONLY when
they catch a specific bug class via compiler verification.

For everything else — pure helpers, syscall wrappers, GPU calls,
game logic, orchestrators — leave unannotated. The compiler's
internal `fn_effect` array still classifies each function for its
own optimization decisions (inline candidates, parallel candidates);
it just doesn't emit a warning if you accidentally do something
side-effecting in a function you never claimed was pure.

### Migration history (2026-05-11)

Pre-collapse, B++ shipped an 8-state phase lattice
(`@base / @solo / @time / @io / @gpu` + internal `@heap / @panic /
@realtime`). The audit found that the compiler internal already only
consulted BASE / SOLO + TIME for real decisions; the other 5 phases
accumulated as inert documentation that propagated through the
lattice without driving enforcement. 700+ redundant annotations were
stripped in a one-day migration sweep. The post-mortem lives in
`journal.md` (2026-05-11 "Multics-drift") and the meta-lesson lives
in Rule 28.

The deprecated keywords are no longer accepted by the parser — a
program using `@base`/`@time`/`@io`/`@gpu`/`@solo`/`@heap`/`@panic`/
`@realtime` produces E260 with a clear migration hint pointing back
at this rule. The `phase_lattice.bpp` and `phase_lattice_bad.bpp`
examples were rewritten as `@safe` showcases.

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
| 0-3 (2 bits) | `: bit2` | `tier: bit2` (Phase A1) |
| 0-7 (small enum, 3 bits) | `: bit3` | `direction: bit3` (Phase A1) |
| 0-15 (larger enum, 4 bits) | `: bit4` | `level: bit4` (Phase A1) |
| 0-31 (5 bits) | `: bit5` | `quality: bit5` (Phase A1) |
| 0-63 (6 bits) | `: bit6` | `slot: bit6` (Phase A1) |
| 0-127 (7 bits) | `: bit7` | `score: bit7` (Phase A1) |
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

## Rule 9: Local struct allocation — `var` vs `auto x: Type`

Two ways to declare a struct-typed local exist; they look similar but
allocate different things. Picking the wrong one is silent — the program
compiles and then segfaults at the first field write.

| Pattern | What it allocates | When to use |
|---------|-------------------|-------------|
| `var x: T;` | `sizeof(T)` bytes on the stack, in place. `x` IS the struct. | Short-lived struct used only inside the function. No pointer to `x` escapes. |
| `auto x: T; x = malloc(sizeof(T));` | 1 word slot, then heap allocation. `x` holds the pointer. | Struct outlives the function, is returned, or is stored elsewhere. |
| `auto x: T;` alone | 1 word slot, **uninitialized (== 0)**. `x` is a null pointer. | Only when `x` will be assigned a real pointer from elsewhere (function return, array element, etc.) before any field access. |

**Pitfall.** `auto x: T; x.field = v;` looks like a stack-struct declaration
but is silently broken — `x` is null until you assign a pointer to it,
so the field write goes to address 0 and SIGBUSes. The compiler does
not warn because the type hint is legal; only the missing initialization
is the bug.

**Rule of thumb.** If the struct lives entirely within the function and
you do not need a pointer to it elsewhere, use `var`. If it crosses
function boundaries or feeds an array of structs, use `auto x: T;` plus
`malloc`.

| Bad | Good |
|-----|------|
| `auto e: Entity; e.hp = 100;` | `var e: Entity; e.hp = 100;` |
| `auto e: Entity; e.hp = 100;` (when e escapes) | `auto e: Entity; e = malloc(sizeof(Entity)); e.hp = 100;` |

## Rule 10: Portable built-ins (cross-platform first)

When adding functionality that user programs will call from any platform
(macOS, Linux, C transpilation), expose it as a B++ built-in. Don't push
the platform conditionals onto user code. The 2026-04-13 backend reorg
introduced this methodology specifically so B++ stays agnostic and easy
to port — the user should never write `if (macos) { ... } else { ... }`.

**The pattern** (worked examples: `malloc`, `malloc_aligned`, `memcpy`,
`putchar`, `getchar`):

| Layer | File | Purpose |
|-------|------|---------|
| Public API | `src/bpp_<area>.bsm` | Declares the function; imports the per-OS implementation file. |
| OS implementation | `src/backend/os/<os>/_<area>_<os>.bsm` | Uses `sys_*` syscalls and OS-specific constants. One file per supported OS. |
| C transpiler mapping | `src/backend/c/bpp_emitter.bsm` | Maps the call to a libc equivalent so `bpp --c` produces working C. |

**Decision rule.** If a user program would need to write
`if (platform == macos) { ... } else { ... }` to do this thing on
multiple platforms, the thing is a missing built-in. Add it.

**Don't expose as user-facing API:**

| Primitive | Why it stays internal |
|-----------|----------------------|
| `sys_mmap`, `sys_open`, `sys_lseek`, ...   | Syscall numbers and signatures differ across OSs |
| `MAP_PRIVATE` / `MAP_ANON` numeric values | Different on macOS vs Linux |
| File mode bits, flag constants | Platform-specific encodings |
| Anything in `src/backend/os/<os>/` directly | Layer is OS implementation, not user API |

These primitives exist so the built-ins above can use them. User code
should never see them.

**When in doubt**, ask: "would it be embarrassing if a user had to
write this by hand on every new platform B++ ports to?" If yes, it
needs to be a built-in. The whole point of the backend split is that
porting B++ to a new OS = adding `src/backend/os/<newos>/_*.bsm` files
without touching user programs or the frontend.

**Cross-reference**: `~/.claude/projects/-Users-Codes-b--/memory/feedback_backend_layers.md`
for the full layer map (chip / os / target / c).

## Rule 11: Ternary and short-circuit idioms

Two language features landed together as a side quest: `?:` as a real
expression and `&&` / `||` with C-style short-circuit semantics. Use
them where they fit instead of the longer alternatives.

### Use ternary for value-selecting `if/else`

| Before | After |
|--------|-------|
| `if (cond) { x = a; } else { x = b; }` | `x = cond ? a : b;` |
| `auto val; if (hp < 20) { val = 99; } else { val = 0; }` | `auto val = hp < 20 ? 99 : 0;` |
| Dispatch to one of two function calls | `(cond ? f : g)(args)` — wait, no function pointers yet; use `cond ? f(args) : g(args)` |

Ternary is right-associative: `a ? b : c ? d : e` parses as
`a ? b : (c ? d : e)`. Use that to flatten cascades.

### Use `&&` for null-pointer guards and combined conditions

`&&` is now short-circuit. The right operand only runs when the left
is truthy. That makes the `if (ptr != 0 && ptr.field > 0)` pattern
memory-safe — the dereference never executes when the pointer is null.

| Before | After |
|--------|-------|
| `if (p != 0) { if (p.field > 0) { ... } }` | `if (p != 0 && p.field > 0) { ... }` |
| `if (a) { if (b) { if (c) { ... } } }` | `if (a && b && c) { ... }` |
| Side-effect-bearing right operand inside `if (0 && ...)` | Just delete the call — it's dead code now |

### When NOT to use

- If the else branch is multiple statements: keep `if/else` — ternary is for value selection, not statement blocks.
- If ternary nesting would exceed two levels: use explicit control flow.
- If the right operand of `&&` or `||` has observable side effects that you want run unconditionally: split into two statements.

Both forms desugar to `T_TERNARY` in the AST, so the backends see a
single canonical node. Adding the idiom costs zero runtime overhead
versus the longer form.

## Rule 12: Float bits across an `auto` slot need `: float`

Storing a float-typed value into a bare `auto` local converts it to
int via FCVTZS — the IEEE 754 bit pattern is gone. The compiler now
catches this with E232 (assignment) and E233 (call site), so the
practical rule is: when a local needs to hold a float, annotate it.

| Pattern | Action |
|---------|--------|
| `auto sr; sr = 44100.0;` | Annotate: `auto sr: float;` |
| `auto pi; pi = 3.14159;` | Annotate: `auto pi: float;` |
| `auto x; x = some_float_func();` | Annotate `x: float`, OR use the return as a float-typed expression directly |
| Truncation IS the intent | Drop the decimal point on the literal: `auto x; x = 3;` |

The compiler emits the explicit conversion path (`FCVTZS` on ARM64,
`CVTTSD2SI` on x86_64) for explicitly-int destinations annotated
`: word`, `: byte`, `: half`, `: quarter`. The `: int` synonym was
removed in 0.23.x — `: word` is the canonical name.

### Polymorphic literals at call sites (Excalibur Arc 1.B)

Excalibur Arc Feature 1 Session 1.B (shipped 2026-05-11) relaxed
ONE specific case: a **decimal integer literal** passed directly to
a `: float` parameter silently widens to float at the call boundary.

```bpp
void foo(speed: float) { ... }

foo(42);        // OK — `42` widens to 42.0 (lossless)
foo(0xFF);      // E240 — hex literals preserve bit-pattern intent
foo(my_int);    // E240 — typed int local could have demoted upstream
foo(0 - 7);     // E240 — `0 - 7` is T_BINOP, not a bare T_LIT
foo(3.14);      // OK — float literal already matches
```

The widening is in-place: validate appends `.0` to the literal's
source bytes so codegen sees a normal float literal. Bit-pattern
intent (hex), demotion concerns (typed int local), and compound
expressions remain explicit by design.

### `const` demotes float literals just like bare `auto`

`const NAME = 0.001;` stores a parsed-int 0, NOT the IEEE bits of
0.001. The const declaration has no type annotation slot in v0.23.x,
so a float literal at the right-hand side is silently demoted at
parse time. Subsequent uses see zero, not the intended fraction.

**Status post-Excalibur 1.B**: still broken. Session 1.B fixed only
the call-site widening for decimal int literals. The const-storage
demotion is a separate language gap that needs `const` to learn float
typing at the slot level — tracked for a future Excalibur session
or independent compiler sidequest. Until then, the `auto local: float`
workaround below is the safe pattern.

Discovered 2026-05-09 while writing `tests/test_json_float.bpp` —
`const _JSON_FLOAT_TOL = 0.000000001;` was 0 at every comparison
site, making `diff > tol` always true and the test always fail.
Replacement: declare a local instead, with the annotation:

```b++
auto tol_floor: float;
tol_floor = 0.000000001;
```

| Pattern | Action |
|---------|--------|
| `const TOL = 1e-9;` (float literal at RHS) | Use `auto tol: float;` local with the same value, OR move the literal to the only call site that needs it |
| `const PI = 3.14159;` ditto | Same — local annotated `: float` |
| `const N = 8;` (integer) | Fine, `const` works for ints |
| `const NAME = "label";` (string) | Fine, `const` works for string-pool refs |

W027 (the diagnostic that catches `auto` demotion) does NOT fire on
`const`, so the pitfall is silent — easy to miss in code review.

### `const` demotes float literals just like bare `auto`

`const NAME = 0.001;` stores a parsed-int 0, NOT the IEEE bits of
0.001. The const declaration has no type annotation slot in v0.23.x,
so a float literal at the right-hand side is silently demoted at
parse time. Subsequent uses see zero, not the intended fraction.

Discovered 2026-05-09 while writing `tests/test_json_float.bpp` —
`const _JSON_FLOAT_TOL = 0.000000001;` was 0 at every comparison
site, making `diff > tol` always true and the test always fail.
Replacement: declare a local instead, with the annotation:

```b++
auto tol_floor: float;
tol_floor = 0.000000001;
```

| Pattern | Action |
|---------|--------|
| `const TOL = 1e-9;` (float literal at RHS) | Use `auto tol: float;` local with the same value, OR move the literal to the only call site that needs it |
| `const PI = 3.14159;` ditto | Same — local annotated `: float` |
| `const N = 8;` (integer) | Fine, `const` works for ints |
| `const NAME = "label";` (string) | Fine, `const` works for string-pool refs |

This is a real B++ language gap, not a Tonify-only stylistic rule —
the const machinery should learn float typing the same way `auto`
did. Tracked as a future compiler sidequest; until it ships, the
`auto` workaround is the safe pattern. W027 (the diagnostic that
catches `auto` demotion) does NOT fire on `const`, so the pitfall
is silent — easy to miss in code review.

Cross-reference: `~/.claude/projects/-Users-Codes-b--/memory/feedback_auto_float_silent_int.md`
for the discovery story (CoreAudio sample-rate field corruption).

## Rule 13: Public API parameters use explicit hints when non-word

Functions declared without `static` form the module's public API.
Parameters that expect non-word types (float, byte, struct pointer,
small enum) should carry an explicit hint in the signature. Static
functions are implementation detail — they are free to experiment with
inferred types. Public functions should communicate their type
expectations to every caller.

| Pattern | Action |
|---------|--------|
| `audio_play(rate, buf, len)` where `rate` is float | Add: `audio_play(rate: float, buf, len: word)` |
| `static helper(tmp)` where `tmp` is internal | Keep as-is — `static` is implementation |
| `pos_set(x, y)` where both are word | Keep as-is — word is the default |
| `set_color(r, g, b, a)` where all are float | Add: `set_color(r: float, g: float, b: float, a: float)` |
| `set_pixel(x: byte, y: byte, c: byte)` already follows the rule | No change |
| `sound_load_wav(path)` where `path` is a C-string pointer | Add: `sound_load_wav(path: ptr)` |
| `image_load(path)` where `path` is a string pointer | Add: `image_load(path: ptr)` |

**The `: ptr` annotation (shipped 2026-05-12).** Same shape as
`: float` / `: word` / `: byte` — declares the parameter is a pointer.
The motivating bug: `put_err(path)` inside an unannotated-param body
silently dispatches to `putnum_err` (TY_WORD default) and prints the
pointer as a decimal integer instead of dereferencing the string.
Annotating the parameter `: ptr` routes the smart dispatch through
`putstr_err` and the path prints correctly. See `docs/journal.md`
2026-05-12 for the codebase-wide sweep that introduced this pattern.

**Why.** API is contract. Without explicit hints on public parameters,
two callers using the same function disagree about its type intent.
One caller passes `1.0` (float), the next passes `1` (int) — both
"compile" but they exercise different codegen paths and one of them is
the bug. Explicit hints turn the function signature into the single
source of truth.

**W025 (shipped 2026-04-16):** the compiler nudge for Rule 13. Fires
when a non-`static` function uses an un-hinted parameter in float
arithmetic — the exact shape where a caller cannot tell from the
signature whether the param wants an int or a float. The fix is
usually `param: float` (promote at call site) or `param: word`
(document integer intent, the body promotes internally). First-cut
implementation hit a bootstrap regression in an earlier sprint that
turned out to be a Mach-O chained-fixups page_count bug unrelated to
W025 itself; fixing page_count unblocked the nudge. The first
violation it caught was `_stb_init_window(w, h, title)` using
`w * 3.0` / `h * 3.0` inside a Cocoa CGRect setup — annotated to
`w: word, h: word` since pixel counts are integers at the call site.

**W032 (shipped 2026-05-12):** the compiler nudge for the
ambiguous `put` / `put_err` call. Fires when smart dispatch picks
`putnum` / `putnum_err` because the argument is a TY_WORD variable
(parameter default OR untyped local) — but the call might have
been intended as a string print whose pointer is being formatted as
a decimal integer. The fix is either `param: ptr` on the declaration
(smart dispatch then routes to `putstr` / `putstr_err`) or calling
`putnum` / `putnum_err` directly to acknowledge the integer intent.
Literals like `put(42)` never warn — they are obviously intentional.

**Return-type annotation (`->` form, canonical since 2026-05-13):**
function return type uses `->` (NOT `:`), placed between the
parameter list and the body opening brace. Different glyph from
the variable / param / field annotations: `:` reads "X has type T"
(it's a property of X), `->` reads "function returns T" (it's the
output side of a function arrow). Same convention as Rust /
Python / Swift / C++11+ trailing-return / TypeScript.

```bpp
// Public API: declares the function returns a pointer. Callers
// can write `auto local: ptr = path_asset("foo");` without E053,
// and `put(path_asset(...))` routes through putstr (no W032).
path_asset(relpath: ptr) -> ptr {
    ...
    return out;             // out is a malloc'd byte buffer
}

// stub form: same annotation on the declaration.
stub path_asset(relpath: ptr) -> ptr;

// Higher-order: outer fn-def uses `->`, inner fn-type also uses `->`.
make_handler() -> func(float) -> word {
    auto inner: func(float) -> word;
    return inner;
}
```

The two glyphs at a glance:

| Position | Glyph | Reads as |
|----------|-------|----------|
| `auto x: ptr` | `:` | "x has type ptr" |
| `(p: ptr)` | `:` | "param p has type ptr" |
| `field: byte` | `:` | "field has type byte" |
| `f(args) -> ptr {` | `->` | "f returns ptr" |
| `func(args) -> ptr` (type expr) | `->` | "type of fn returning ptr" |

`:` is reserved for "X has type T" annotations on storage; `->`
is reserved for "function returns T" — never the same glyph for
both concepts. Mixing them is a parser error since the
2026-05-13 cleanup (commit pending).

**When to annotate the return type:**

| Pattern | Action |
|---------|--------|
| Public function returning a malloc'd pointer (`str_dup`, `path_asset`, `image_load`, `strbuf_new`) | Annotate `-> ptr` so callers can chain without workarounds |
| Public function returning a float (`game_dt`, `sin_f`, `cos_f`) | Annotate `-> float` (or `-> half float` for 32-bit precision) |
| Public function returning a sub-word int by intent (rarely needed) | Annotate the matching slice type; usually unnecessary |
| Static helpers used only in this file | Skip — flow analysis covers the call site |
| Functions returning a plain word (counts, indices, IDs) | Skip — TY_WORD is the default, no annotation needed |

Sweep is not bulk — only annotate when a consumer actually
needs the return type for downstream smart dispatch. Same
opt-in shape as `: ptr` on params. Two-consumer rule (Rule 20)
applies before promoting "we should annotate ALL pointer
returns."

**Migration history (2026-05-13):** B++ briefly accepted both
`fn(): ret {` and `fn() -> ret {` for function definitions
between the V3 ship (2026-05-05) and the cleanup (2026-05-13).
The `:` form was a wart — same glyph for "var has type" and
"function returns" obscured the distinction. With only 1
production consumer (path_asset, added the same week), the
unification cost was 1 line of source + 1 line of test +
3 docs sites. Future programmers see one canonical form.

### Builtin return-type registry — exception only for true builtins

`src/bpp_types.bsm:_builtin_ret_type` is a hardcoded registry
that tags a function's return type by string match. After the
2026-05-13 cleanup it contains ONLY entries for **true builtins**
— functions implemented directly in codegen with no B++ source
file (`peekfloat` emits a single LDR D + FCVT instruction;
`peekfloat_h` emits LDR S + FCVT). They cannot be source
functions because the inline instruction is the point — wrapping
them in a function call defeats the purpose.

**Rule for new code:** any function that has (or could have) a
`.bsm` source MUST declare its return type via `-> ret` on the
signature directly. Adding entries to `_builtin_ret_type` for
source-having functions is forbidden — it splits the canonical
form across two locations and forces future readers to grep the
compiler internals to learn an API's shape.

**Migration history:** 2026-05-13 retired the registry tagging
of `arr_new` / `buf_word` / `buf_byte` (moved to source `->
array` / `-> ptr`) and demoted `argc_get` / `argv_get` /
`envp_get` from codegen builtin to plain source functions in
`src/bpp_args.bsm`. The registry now contains only `peekfloat`
and `peekfloat_h`. See `docs/builtin_to_source_plan.md` for the
full rationale + execution log.

## Rule 14: Increment and compound assignment operators

`++`, `--`, `+=`, `-=`, `*=`, `/=`, `%=` are now first-class syntax.
Replace manual accumulator patterns everywhere — especially for loop
increments, counters, and array walkers.

### Mapping table

| Before | After | Notes |
|--------|-------|-------|
| `i = i + 1;` | `i++;` | Loop counter, position |
| `i = i - 1;` | `i--;` | Reverse walk |
| `++i;` / `i++;` | Both work | No semantic difference as statements |
| `x = x + n;` | `x += n;` | Any n |
| `x = x - n;` | `x -= n;` | Any n |
| `x = x / n;` | `x /= n;` | Safe when RHS is simple literal/var |
| `x = x * n;` | `x *= n;` | **See pitfall below** |
| `for (i = 0; i < n; i = i + 1)` | `for (i = 0; i < n; i++)` | All for-loop increments |

### CRITICAL pitfall: `*=` and `/=` change precedence when RHS has `+`/`-`

`x = x * 10 + y` means `(x * 10) + y`.  
`x *= 10 + y` means `x = x * (10 + y)`.  **Different result.**

**Rule**: only apply `*=`/`/=`/`%=` when the ENTIRE right-hand side is a
single literal or identifier. If the original expression has `+` or `-`
after the multiplied value, keep the explicit form.

| Pattern | Action |
|---------|--------|
| `val = val * 10 + (ch - '0');` | **Keep as-is** — digit accumulator, `*=` changes semantics |
| `h = h * 33 + peek(p + i);` | **Keep as-is** — hash function, same issue |
| `cap = cap * 2;` | Use `cap *= 2;` — RHS is pure literal ✓ |
| `pos = pos / 10;` | Use `pos /= 10;` — safe ✓ |
| `len = len * stride;` | Use `len *= stride;` — RHS is pure identifier ✓ |

### `+=`/`-=` are always safe

Addition and subtraction are associative for integers, so
`x = x + a + b` and `x += a + b` produce the same value.

---

## Rule 15: Raw memory — prefer peek/poke width builtins

The multi-byte peek/poke family emits a single typed load/store
instruction (LDRH / LDR W / LDR X / LDR S / LDR D on ARM64). Replace
manual byte-assembly patterns with the canonical builtin.

### Integer widths

| Before | After | Instruction |
|--------|-------|-------------|
| `peek(p) \| (peek(p+1) << 8)` | `peek_q(p)` | LDRH — 16-bit LE, zero-extend |
| `poke(p, v); poke(p+1, v>>8);` | `poke_q(p, v)` | STRH — low 16 bits |
| `peek(p) \| (peek(p+1)<<8) \| (peek(p+2)<<16) \| (peek(p+3)<<24)` | `peek_h(p)` | LDR W — 32-bit LE, zero-extend |
| four-byte poke dance | `poke_h(p, v)` | STR W — low 32 bits |
| eight-byte peek_w | `peek_w(p)` | LDR X — full 64-bit |
| eight-byte poke dance | `poke_w(p, v)` | STR X |

Naming mnemonic: `_q` = quarter (16-bit), `_h` = half (32-bit), `_w` = word (64-bit).
Same as the slice type system (`TY_FLOAT_H` = 32-bit).

### Float widths

| Before | After | Instruction |
|--------|-------|-------------|
| 4× poke/peek byte dance to reinterpret f32 | `peekfloat_h(addr)` | LDR S + FCVT D |
| same for write | `pokefloat_h(addr, v: half float)` | FCVT + STR S |
| 8× poke/peek for f64 | `peekfloat(addr)` | LDR D |
| same for write | `pokefloat(addr, v: float)` | STR D |

### `bpp_buf` read/write wrappers (buffered offset access)

When the address comes from `buf + offset`, prefer the `bpp_buf` wrappers
which take `(buf, offset)` separately — cleaner call sites in parsers and
serializers.

| Builtin | `bpp_buf` wrapper | When to use |
|---------|-------------------|-------------|
| `peek_q(buf + off)` | `read_u16(buf, off)` | Format parsers (PNG, WAV chunk headers) |
| `peek_h(buf + off)` | `read_u32(buf, off)` | |
| `peek_w(buf + off)` | `read_u64(buf, off)` | |
| `peekfloat_h(buf+off)` | `read_f32(buf, off)` | IEEE float WAV, GLB |
| `peekfloat(buf + off)` | `read_f64(buf, off)` | |
| Corresponding writes | `write_u16/u32/u64/f32/f64` | |
| BE variants | `read_u16be/u32be/u64be` | PNG, network byte order — byte-by-byte, intentional |

**Note:** BE variants use byte-by-byte assembly. Do NOT replace them with
`peek_q`/`peek_h` — those emit native-endian LE loads.

---

## Rule 16: `bpp_buf` bulk operations replace manual loops

For byte-level bulk operations, prefer the `bpp_buf` API over hand-rolled
loops. The intent is clearer, and the implementations are free to upgrade
to `memset`/`memmove` syscall wrappers as the runtime grows
(`bpp_runtime.bsm` shipped in Phase 6.1).

| Before | After | Notes |
|--------|-------|-------|
| `for (i=0;i<n;i++) { poke(b+i, 0); }` | `buf_fill(b, 0, n)` | Zero-fill n bytes |
| byte-by-byte copy loop | `buf_copy(dst, src, n)` | No overlap guarantee (wraps memcpy) |
| byte-by-byte copy with overlap check | `buf_move(dst, src, n)` | Overlap-safe (memmove semantics) |
| `for (i=0;i<n;i++) { if (peek(a+i) != peek(b+i)) { return 0; } }` | `buf_cmp(a, b, n) == 0` | Returns 0 equal, -1 a<b, 1 a>b |
| `malloc(n)` for raw byte buffer | `buf_byte(n)` | No header, no length metadata |
| `malloc(n * 8)` for word buffer | `buf_word(n)` | Allocates n × 8 bytes |

**`buf_cmp` return value**: 0 = equal, -1 = a < b at first difference, 1 = a > b.
Matches `memcmp` sign convention.

---

## Rule 17: `put()` / `put_err()` for ad-hoc output

`put(x)` is a smart-dispatch builtin: the compiler rewrites it to
`putstr`, `putnum`, or `putfloat` based on the inferred type of `x`.
Use it for quick debug prints and simple output instead of choosing
the typed variant manually.

| Before | After | Notes |
|--------|-------|-------|
| `putnum(i);` | `put(i);` | int/word |
| `putstr(s);` | `put(s);` | pointer → treated as C-string |
| `putfloat(f);` | `put(f);` | float |
| `putnum_err(n);` | `put_err(n);` | stderr version |
| `putstr_err(s);` | `put_err(s);` | stderr version |

**When to keep the typed variant**: when you need a specific format
(`puthex_err` for hex dumps) or when the smart dispatch might infer the
wrong type. The typed variants are always available as the explicit
fallback.

---

## Rule 18: `bpp_array` — dynamic arrays

`bpp_array` is auto-injected. No import needed.

### Core API

| Function | Signature | Notes |
|----------|-----------|-------|
| `arr_new()` | `→ arr` | Allocate empty dynamic array |
| `arr_push(arr, val)` | `→ arr` | Append val; returns same arr (may realloc) |
| `arr_pop(arr)` | `→ val` | Remove and return last element |
| `arr_get(arr, i)` | `→ val` | Get element at index i — **unchecked, fast** |
| `arr_at(arr, i)` | `→ val` `@io` | Bounds-checked get — logs OOB to stderr, returns 0 |
| `arr_set(arr, i, val)` | `void` | Set element at index i |
| `arr_len(arr)` | `→ int` `@base` | Current element count |
| `arr_cap(arr)` | `→ int` `@base` | Current allocated capacity |
| `arr_last(arr)` | `→ val` `@base` | Peek last element without removing |
| `arr_truncate(arr, n)` | `void` | Set length to n (no deallocation) |
| `arr_clear(arr)` | `void` | Reset length to 0 |
| `arr_free(arr)` | `void` | Free the array and its header |

Subscript syntax `arr[i]` desugars to `arr_get(arr, i)` for reads and
`arr_set(arr, i, val)` for writes — use it in idiomatic code.

### `arr_get` vs `arr_at` — choose by call site, not by taste

| Site | Use | Why |
|------|-----|-----|
| Hot inner loops (`for (i=0; i<arr_len(arr); i++)`) | `arr_get` / `arr[i]` | Bounds are provably safe; branch + I/O in `arr_at` kills real-time budget |
| Boundary-layer callers: PNG/WAV parsers, JSON deserializers, asset loaders | `arr_at` | Index comes from external/untrusted input; fail loudly with context |
| Code you control, inside a safe range | `arr_get` / `arr[i]` | Trust internal code |

**Common anti-pattern**: using `arr_at` everywhere "for safety". `arr_at`
tags the function `@io` (it writes to stderr), which contaminates the
call graph. Use it only at the actual trust boundary.

### Gotcha: `arr_push` may invalidate pointers

`arr_push` can reallocate the internal buffer. Any raw pointer into the
array body (e.g. `auto p = arr + offset`) is invalidated after a push.
Re-derive from `arr` after any push if you need pointer stability.

---

## Rule 19: `bpp_str` — string utilities + `strbuf` dynamic builder

`bpp_str` is auto-injected. No import needed. All string functions work
on null-terminated C-strings.

### Predicate and query

| Function | Signature | Notes |
|----------|-----------|-------|
| `str_len(s)` | `→ int` | strlen — walk to null byte |
| `str_eq(a, b)` | `→ 0/1` | strcmp: 1 if equal, 0 if different |
| `str_neq(a, b, n)` | `→ 0/1` | strncmp: 1 if first n bytes equal |
| `str_starts(s, prefix)` | `→ 0/1` | Prefix match |
| `str_ends(s, suffix)` | `→ 0/1` | Suffix match — canonical; don't write `_ends_with` private helpers |
| `str_chr(s, c)` | `→ index or -1` | First occurrence of byte c; -1 = not found |

### Mutation and copy

| Function | Signature | Notes |
|----------|-----------|-------|
| `str_cpy(dst, src)` | `→ len` | strcpy into dst; returns bytes written |
| `str_cat_raw(dst, src)` | `→ len` | strcat into dst (dst must have space) |
| `str_dup(s)` | `→ new_ptr` | malloc + copy; caller frees |
| `str_from_int(val, buf)` | `→ len` | itoa into buf; returns number of digits written |

### `strbuf` — dynamic string builder

Use `strbuf` to build strings incrementally without manual length tracking.

| Function | Signature | Notes |
|----------|-----------|-------|
| `strbuf_new()` | `→ sb` | Allocate empty builder (initial cap 64) |
| `strbuf_cat(sb, src)` | `→ sb` | Append null-terminated string |
| `strbuf_ch(sb, c)` | `→ sb` | Append single char |
| `strbuf_num(sb, val)` | `→ sb` | Append decimal integer |
| `strbuf_len(sb)` | `→ int` `@base` | Current content length (not capacity) |
| `strbuf_clear(sb)` | `void` | Reset length to 0 (retains allocation) |
| `strbuf_free(sb)` | `void` | Free |

The builder's content is a null-terminated C-string at `*(sb + _SB_HDR)`.
To pass the result to a function expecting `char*`, read that pointer.

### Common anti-patterns to replace

| Before | After |
|--------|-------|
| Private `_ends_with` helper in module | `str_ends(s, suffix)` |
| Manual `str_len` + memcmp pair | `str_neq(a, b, n)` |
| `malloc(256); str_cpy(buf, s1); str_cat_raw(buf, s2)` | `strbuf_new()` + `strbuf_cat` × 2 |
| `while (peek(s+i) != 0) { i++; }` counting loop | `str_len(s)` |

---

## Rule 20: Two-consumer rule — promote on the second consumer

**The moment a second caller appears for a private helper, promote
it to its canonical home.** Do not wait for the third.

The trigger is the *second* consumer because:

- One consumer is local optimisation. The function lives where it
  was written, and that is the right call.
- Two consumers is **shared infrastructure pretending to be
  private**. Every additional caller after the second pays the cost
  of duplicated logic, divergent fixes, and the eventual "which
  copy is canonical?" archaeology. By the third consumer, the
  divergence is already there.
- The second consumer is also when the *interface* gets its first
  honest stress test — one caller can rationalise any signature; a
  second caller forces the API to be more general than convenient.

**Where to promote:**

| Helper kind | Canonical home |
|---|---|
| Pure compute (`sin_f`, `floor_f`, hashing, bit-tricks) | `src/bpp_math.bsm` (Layer 3, auto-injected) |
| Data structure / collection | `src/bpp_<shape>.bsm` (auto-injected) |
| Game-class generic (procedural generators, pathfinding, sprites) | `stb/<feature>.bsm` (Layer 2, opt-in import) |
| Compiler-internal (parser/codegen helpers) | `src/bpp_internal.bsm` |
| OS/platform primitive | `src/backend/os/<os>/_*_<os>.bsm` (per OS) |

**Worked example (2026-05-04):** `_rc_sin` / `_rc_cos` / `_rc_abs_f`
/ `_rc_floor_f` lived as private helpers in `stb/stbraycast.bsm`
through Wolf3D Session 1. When Session 3's `fps_walk` /
`fps_turn` in `stbphys.bsm` needed the same trig, the rule fired:
two consumers, promote up. They moved to `src/bpp_math.bsm` as
public `sin_f` / `cos_f` / `abs_f` / `floor_f` (auto-injected) in
the same commit that wired `fps_walk`. Bootstrap stayed
byte-stable; both consumers call the public API; future trig
needs in any program use the same names.

**The discipline that fails this rule:**

- "I'll keep my private copy and we'll consolidate later." Later
  never comes; the third consumer fork-bombs the duplication.
- "Promotion needs a refactor session of its own." It does not.
  Promote in the same commit as the second use; the diff is one
  delete + one move + N call-site renames.
- "The interface might still change — let's wait." If the
  interface needs to change, the second consumer is the one
  forcing the change. Wait, and the first consumer hardens around
  a wrong interface.

**The discipline that follows the rule:** when adding a new
function, check whether *any* existing helper does the same job.
If yes, use it. If no, write the new function in its canonical
home from the first keystroke — even if you only have one caller
today. The cost of "lives in the right place since day 1" is zero;
the cost of late promotion compounds.

---

## Rule 21: `func`-types are opt-in (V3 — shipped 2026-05-05)

`func(args) -> ret` is a first-class type. Like `:float` / `:half` /
`:byte`, the annotation is opt-in: untyped function-pointer code keeps
compiling. Annotate when the type matters for correctness — registries
where the wrong shape silently corrupts arg routing through register
banks.

| Pattern | Action |
|---------|--------|
| Public registry param: `maestro_register_solo(fn)` | Annotate: `maestro_register_solo(fn: func(float) -> void)` |
| Callback param inside event/action handler | Annotate the param with the expected signature |
| Local fn-pointer used in `call(fp, ...)` | Annotate when the type pins down the contract; else flow analysis handles it |
| Untyped public callback (no annotation) | Leave it — flow analysis (W028) catches mismatches at the call site |

**Without annotation**, V3 still catches mismatches:

- **W028 (intra-function flow analysis, Session 2):** when the
  fn-pointer's origin can be resolved within the current function
  (`auto fp = fn_ptr(target); call(fp, args)`), the validator
  checks each arg against the resolved target's declared params.
- **W028 (cross-function via registration helper, Session 4):**
  when a helper function stores a param into a global (the
  registration pattern), V3 walks every caller of that helper and
  records the registered targets. Subsequent `call(global, args)`
  sites validate against ALL registered targets — Estrita conflict
  resolution: any origin mismatching the call site's args fires
  W028.
- **E246 (annotation contract, Session 3):** when both sides of an
  assignment carry explicit `func(...)` annotations, mismatched
  shapes are a hard error. The annotation is the contract.

**Estrita conflict resolution**: a registry that receives callbacks
of multiple incompatible signatures is a bug, not a feature. B++
follows data-oriented design conventions (Acton/Blow/Muratori): use
typed overloads (`maestro_register_solo_int(...)` +
`maestro_register_solo_float(...)`), tagged unions, or context-struct
patterns instead of polymorphic registries. If a future
plugin/scripting system genuinely needs polymorphic dispatch, an
opt-in `@poly` annotation will silence W028 — not yet implemented.

Pitfall 6 below is now diagnostic-shipped: the pattern that
historically corrupted `dt` between `maestro_register_solo` and
`call(_ms_cb_solo, dt)` fires W028 today, regardless of annotations.

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

Rfechaunning `codesign -s - ./bpp` changes the binary's bytes, which changes
the `compiler_hash` used in cache keys. All existing `.bo` files become
keyed to the old hash, causing silent stale-cache bugs that manifest as
2-cycle bootstrap oscillation. See `bootprod_manual.md` for full details.

**Rule**: NEVER run `codesign` on the `./bpp` binary.

### Pitfall 4: `return 0 - 1` instead of `return -1`

B++ has supported unary minus on literals since the parser gained the
`T_UNARY('-')` node. The `0 - 1` pattern is a historical artifact from
before that landed. It compiles correctly but is misleading — readers
expect `-1`, not a subtraction expression.

**Fix**: replace `0 - 1` with `-1` everywhere in `return` statements and
assignments.

```
// Before                         After
return 0 - 1;                  → return -1;
hovered_idx = 0 - 1;           → hovered_idx = -1;
```

**Script anchor**: replace `\b0 - 1\b` with `-1`. The regex is safe
because no codebase pattern writes `something * 0 - 1` (that would be
an unusual expression). Verify with grep before bulk replace.

**Do NOT replace**: `0 - x` (variable), `arr[len - 1]`, `n - 1` (not
zero-based). The pattern to target is literally `0 - 1` (zero minus one).

### Pitfall 5: `: float` parameters on functions called via fn_ptr `call()` — RESOLVED 2026-05-04

**Status: HISTORICAL.** The compiler now routes float args through
the proper FP register bank under both AAPCS64 (aarch64) and System V
x86_64. Direct calls AND indirect `call(fp, args...)` dispatch emit
the bit-cast / movq / fmov dance automatically based on the
compile-time return type of each arg expression. Callbacks declared
with `: float` parameters work as expected.

What the historical workaround required (drop the `: float`
annotation, convert internally via `dt_s = dt / 1000.0`) was a hack
around the gap; the gap is closed. Any code still using that
workaround can be cleaned up — `: float` on the parameter is the
canonical form again.

**Convention shift recorded:** maestro_run now passes `dt` as
**float seconds** (industry standard — Unity `Time.deltaTime`,
Unreal `FApp::GetDeltaTime`, Bevy `Res<Time>.delta_seconds`,
Godot `_process(delta)`). `pos += vel * dt` reads as the physics
formula directly; the older int-millisecond convention forced
every callsite to write `pos += vel * dt / 1000` and dropped sub-ms
precision (1000/60 truncated to 16ms produced visible 4% drift).

**Cross-reference**: `warning_error_log.md` "FFI float-param trap"
(W027) is marked RESOLVED. The new gotcha — Pitfall 6 below —
covers the inverse direction.

---

### Pitfall 6: `call(fp, ...)` arg types must match the callee's param types — DIAGNOSTIC SHIPPED 2026-05-05

**Status: COVERED.** V3 ships W028 (flow analysis, intra- and
cross-function via registration pattern) and E246 (annotation
contract). The trapping behavior below is now caught by the compiler
in every shape that B++ programs encounter today — see Rule 21 above
for the diagnostic ladder.

After the AAPCS64 / System V refactor, the `call()` builtin routes
each arg into the correct register bank based on its **compile-time
expression type**:

- arg returns `int` (`ety == 0`) → pushed via int stack, popped into
  `x_<int_seq>` / `rdi rsi rdx rcx r8 r9` (System V).
- arg returns `float` (`ety == 1`) → pushed via FP stack, popped into
  `d_<flt_seq>` / `xmm<flt_seq>`.

The callee then reads each parameter from the matching ABI bank
(via the prologue's `emit_arg_copy_int` or `emit_arg_copy_flt`).
The two banks have independent sequence counters: `(int, float, int)`
lands at `(x0, d0, x1)`, NOT `(x0, d1, x2)`.

**The new wrong way** is to declare a param with one type and pass
an arg of the other type through `call()`:

```bpp
// Callee:
void my_solo(dt: float) { ... }       // reads d0

// Caller (dt as int):
auto dt;
dt = 16;                                // int
fp = fn_ptr(my_solo);
call(fp, dt);                           // BUG: arg goes to x0, callee reads d0 → garbage
```

The fix is symmetric to the Pitfall 5 historical case but inverted:
**make the caller's arg type match the callee's param type.** Either
declare the local as `: float` and assign a float value, or change
the callee to take int.

| Pattern | Action |
|---------|--------|
| Callee declares `dt: float`, caller computes int `dt` | Annotate caller local: `auto dt: float;` — assignment from int converts via SCVTF. |
| Callee declares `dt` (int), caller has float value | Either annotate callee `dt: float` (preferred) or cast at caller via `: word` truncation. |
| Direct call `name(arg)` (not via `call()`) | Safe — direct call sites are already type-aware: compiler emits `scvtf` / `fcvtzs` per declared param type. |

**Diagnostic candidate.** Warn at `call(fp, args...)` when `fp` can
be statically traced back to a `fn_ptr(name)` call AND any arg's
compile-time type doesn't match the corresponding param type of
`name`. Requires light flow analysis (track fn_ptr origin through
local assignments). Not yet implemented — sidequest opportunity.

**Direct calls are exempt.** When the caller writes `my_solo(dt)`
naming the function directly (not through a `fn_ptr` indirection),
the compiler knows the callee's declared types and emits the
appropriate conversion: `scvtf` (int → float bits via SCVTF) for
int-arg-to-float-param, `fcvtzs` (float-truncate-to-int) for the
inverse. The pitfall fires only on the indirect `call()` path.

**Cross-reference**: `warning_error_log.md` "Known compiler
diagnostic gaps" section.

### Pitfall 7: Multi-pass GPU rendering with shared `_gpu_vbuf` — RESOLVED 2026-05-07

When a frame consists of multiple `render_begin / render_end`
cycles (e.g., offscreen render-to-target + window blit, the
Phase 4.1.2 contract), the shared `_gpu_vbuf` needs offset
accumulation across passes. Resetting `_gpu_flush_off = 0` at
every `_stb_gpu_begin` — which the original single-pass design
did — produces a visible-only race.

**The race scenario**:

```bpp
// Pass 1
gpu_target_bind(target);        // offscreen
render_begin();                  // _gpu_flush_off = 0 (reset)
render_clear(BLUE);              // CPU writes bytes 0..96 of _gpu_vbuf
render_end();                    // commit cmdbuf — GPU starts reading 0..96 async

// Pass 2
gpu_target_bind(0);              // window
render_begin();                  // _gpu_flush_off = 0 (reset — BUG)
render_clear(MAGENTA);           // CPU writes bytes 0..96 — overwrites Pass 1's reads
render_end();                    // GPU reads 0..96 — sees MAGENTA bytes for Pass 1, garbage for Pass 2
```

Single-pass games never hit this because `nextDrawable` blocks
long enough between frames for the previous GPU read to complete.
Multi-pass within a frame has no such serialisation point —
Pass 1's commit returns immediately and Pass 2 starts CPU writes
before Pass 1's GPU read is done.

**Fix (live in `_stb_platform_macos.bsm` since 2026-05-07)**:

`_gpu_flush_off` accumulates within a frame. Each pass owns a
non-overlapping offset region:

```
Pass 1 → offsets [0..N1]
Pass 2 → offsets [N1..N2]
Pass 3 → offsets [N2..N3]
...
```

Reset to 0 happens only at the window-pass present (frame
boundary). By the time the CPU writes those offsets next frame,
the previous frame's GPU reads are reliably done — no
`waitUntilCompleted`, no lost async perf, just cooperative
buffer-region ownership across passes.

**When this pitfall applies**:

- Custom multi-pass rendering pipelines: yes, this is the contract.
- Effect chains (Phase 6 `stbfx`): yes.
- Single-pass games (snake, pathfind, fps_3d, etc.): no — only
  one pass per frame, no race possible. The fix is a no-op for
  them.
- Multi-attachment within a single render pass: no — same
  encoder, same submission, no concurrent CPU writes.

**Cross-platform contract**:

Linux GPU backend (when implemented via Vulkan / X11 hardware
path) MUST honour the same offset-accumulation contract or
implement an alternative race-free strategy (per-pass buffers,
ring buffer, MTLEvent-equivalent fences). Failing this breaks
multi-pass rendering silently — visible only as wrong colours
or garbage at runtime.

**Diagnostic methodology that caught this** (recorded for
posterity):

1. Suite + bootstrap stayed green — race conditions in async
   GPU work cannot be caught by either.
2. Visual eyeball check on `render_target_smoke.bpp` showed
   "magenta flickers then blue overwrites" — the symptom.
3. `bug --tui --break _stb_gpu_clear_inline` confirmed
   smart-dispatch routing was correct (magenta arg arrived).
4. `put_err` instrumentation in the platform layer confirmed
   correct dimensions and colors at queue time.
5. Isolation tests (comment Pass 1, comment blit) narrowed
   the failure surface.
6. `waitUntilCompleted` probe after Pass 1 commit fixed the
   visual → race confirmed → designed proper fix without sync.

The combo of "visual + isolation + sync probe" is the canonical
recipe for narrowing async GPU race conditions that suite and
bootstrap cannot catch.

---

## Rule 22: stb cartridges do NOT import bpp_* runtime modules

The auto-inject pass in `src/bpp_import.bsm` injects every
`bpp_*` runtime module (`bpp_array`, `bpp_str`, `bpp_io`,
`bpp_buf`, `bpp_hash`, `bpp_file`, `bpp_math`, `bpp_arena`,
`bpp_maestro`, `bpp_beat`, `bpp_job`, `bpp_json`) into every
program before user code is compiled. By B++ design, no `stb*.bsm`
cartridge ever needs an explicit `import "bpp_*.bsm"` line —
auto-inject covers them.

| Pattern | Action |
|---------|--------|
| `import "bpp_array.bsm";` in any stb file | Delete |
| `import "bpp_math.bsm";` in any stb file | Delete |
| `import "bpp_json.bsm";` in any stb file | Delete (auto-injected since 2026-05-06) |
| Any other `import "bpp_*.bsm";` in stb | Verify it's auto-injected, then delete |

**Why.** Explicit imports for auto-injected modules are noise.
They suggest a dependency graph that the compiler doesn't actually
respect (auto-inject runs BEFORE the dep edge from the explicit
import is recorded). Worse, they tempt readers into thinking a
cartridge depends on `bpp_str` more than another stb cartridge —
in fact every stb file gets the entire bpp runtime for free.

**Game files** (`*.bpp`) and **tools** (`tools/`) are different —
they may keep explicit `import` lines for clarity even when the
target is auto-injected. Auto-inject covers them too, but the
extra import is harmless and signals intent to the reader.

**Status (Phase 3.5.5):** all 11 stb cartridges swept; the only
remaining `import "bpp_*"` lines anywhere are in tests + game
files. New stb cartridges should never carry one.

---

## Rule 23: Cartridge minimalism — `stbgame` and `stbwindow` ship the floor only

The two entry-point cartridges that open a window split by use case
and ship only the foundational subsystems every windowed program
needs. Everything else is opt-in via explicit `import` from the
caller — no convenience couplings.

| Cartridge | Used by | Loop | Resolution policy |
|---|---|---|---|
| `stbgame.bsm` | games | 60-fps loop + maestro | pixel-perfect default (Phase 4.1+) |
| `stbwindow.bsm` | tools | manual loop, no fps cap | native resolution |

**Both** include the same foundational subsystems and nothing
beyond them:

- `init_math` + `math_trig_init` — sqrt / sin / cos
- `init_input` — keyboard + mouse + action map
- `init_color` — palette + named colour constants
- `init_draw` — CPU framebuffer primitives (rect, line, sprite, blit)
- `init_font` — bitmap text
- `init_str` — strbuf pool

**Neither** pulls in:

- `stbui` — widgets, theme, ui-arena. Opt in with
  `import "stbui.bsm"; init_ui(); ... ui_arena_reset();` per frame.
- `stbimage` — PNG decode + atlas pack + hot-reload. Opt in with
  `import "stbimage.bsm"; image_load(...);` and, if you want live
  reload, `image_hot_reload_enable(img); image_hot_reload_tick_throttled();` per frame.
- `stbaudio` / `stbmixer` — audio stack. Opt in only when needed.
- `stbecs` / `stbphys` / `stbtile` / `stbpath` — game systems.
- `stbrender` — Metal-backed GPU pipeline. Required for sprite atlas
  flushes; CPU-only games skip it.

**Why.** Convenience couplings hide the dependency graph. A test
that imports `stbgame` to use the loop should not magically also
get widget-arena tracking — it inflates compile time, drags
unused code into every binary, and tempts the cartridges to grow
sideways instead of staying focused. Cartridges live at Layer 2
and must respect the top-down dependency rule (a program imports
what it uses; a cartridge does not import what its callers might
want).

**Tools that draw widgets** (Bang 9, ModuLab, Level Editor,
the_bug) opt in like this:

```bpp
import "stbwindow.bsm";
import "stbui.bsm";

main() {
    window_init_full(800, 600, "Tool");
    init_ui();                              // once at boot
    while (window_should_close() == 0) {
        window_frame_step();
        ui_arena_reset();                   // per frame
        // ... render ...
        draw_end();
    }
    window_close();
    return 0;
}
```

**Games that hot-reload assets** (pathfind today; any future game
that wants live edits) opt in like this:

```bpp
import "stbgame.bsm";
import "stbimage.bsm";

main() {
    game_init(W, H, "Game", 60);
    auto atlas = image_load("assets/sprites.json");
    image_hot_reload_enable(atlas);

    while (game_should_quit() == 0) {
        game_frame_begin();
        image_hot_reload_tick_throttled(); // per frame
        // ... update + render ...
    }
    return 0;
}
```

**The discipline that fails this rule:**

- "Add `init_ui()` to `stbgame` so callers don't have to remember."
  Then every CPU game pays the ui-arena cost. Every test that uses
  `stbgame` for the loop pulls stbui into its binary.
- "Tick `image_hot_reload` from `stbgame` because some games want
  it." Then every game that doesn't use atlases polls a watch list
  every frame. The poll cost is small but the principle is what
  matters: cartridges should not act on behalf of callers.
- **2026-05-17 caught in real life:** stbui v2 engine commit
  `dedfb04` added `import "stbimage.bsm"` so a new `ui_image`
  widget could call `image_draw_size`. Same anti-pattern one
  layer deeper — stbui is a Layer 2 cartridge that now forced
  every consumer of stbui to transitively load stbimage. Cost
  surfaced days later: stbgame → stbui → stbimage chain summed
  past the parser's `fbuf` cap (286 KB > 256 KB) and silently
  corrupted the heap, SIGSEGV'ing every game compile. Even after
  the parser bug was fixed at the root (`cd6d00e`), the
  speculative import was reverted (`dba3e2a`) because the
  widget had zero consumers across games / tools / examples /
  tests. The rule applies whether the cartridge being inflated
  is `stbgame` or `stbui` itself: no convenience import without
  a verified consumer.

**The discipline that follows the rule:** the cartridge ships the
floor. The caller imports what it needs. The dependency graph
matches what the binary actually links.

**Cross-reference**: Rule 22 (stb !import bpp_*) and Rule 12
(:float opt-in) — same minimalism principle applied to different
layers. Auto-inject covers what every program *must* have; opt-in
covers what programs *might* want. The two should not blur.

---

## Rule 24: `render_clear` is context-smart (single API, dual mechanism)

`render_clear(color)` is one function with two implementations.
The runtime picks the optimal mechanism based on context — the
programmer always writes `render_clear(color)` and gets pixels of
that color, regardless of whether they call it before or inside a
render pass.

| Context | Implementation | Performance |
|---|---|---|
| OUTSIDE pass (before `render_begin`) | State-set `_gpu_clear_*` for the next render_begin's `loadAction = .clear` | Free (tile-memory clear on Apple Silicon) |
| INSIDE pass (between `render_begin` / `render_end`) | Emit fullscreen solid-color quad through default pipeline | ~µs on M4 |

The flag `_gpu_in_pass` (set by `_stb_gpu_begin`, cleared by
`_stb_gpu_present`) is the runtime signal the dispatch reads.

### Single-pass game (typical)

```bpp
while (!quit) {
    game_frame_begin();
    render_clear(BG);             // outside → free tile-memory clear
    render_begin();
    render_rect(...);
    render_end();
}
```

Identical to how every existing game (snake, pathfind, fps_3d,
rhythm, platformer) is written today. Zero source change.

### Multi-pass with intermediate clears

```bpp
while (!quit) {
    // Offscreen pass — render the world.
    gpu_target_bind(scene_target);
    render_begin();
    render_clear(SKY_BLUE);       // inside → quad-clear NOW
    draw_world();
    render_end();

    // Window pass — letterbox + blit.
    gpu_target_bind(0);
    render_begin();
    render_clear(LETTERBOX_BG);   // inside → quad-clear NOW
    gpu_present_target(scene_target, ...);
    render_end();
}
```

The same call works in either context. Pass-state is implicit; the
programmer writes intent ("pixels turn this color"), the runtime
picks the optimal Metal verb.

### Why single API (not OpenGL-style dual)

OpenGL / SDL / Vulkan separate `glClearColor` (state) from
`glClear` (action) because their runtimes don't track pass state at
the API surface — the programmer has to disambiguate. B++ tracks
`_gpu_in_pass` internally, so the programmer doesn't have to.
Dispatch is decided where the information lives.

This pattern follows existing B++ conventions for smart-default +
opt-in to explicit:

| Capability | Smart default | Explicit opt-in |
|---|---|---|
| Type assignment | `auto x = 1.5` (inferred) | `auto x: float` (precision) |
| fn-pointer typing | `auto fp = fn_ptr(name)` | `auto fp: Fn(...) -> ret` |
| Pixel-perfect | `game_init(W, H, ...)` (default ON) | `game_set_pixel_perfect(off)` |
| **Render clear** | **`render_clear(color)` (smart context)** | **— (no opt-in needed yet)** |

If a future caller needs explicit state-setting *while inside an
active pass* (a niche case for delayed clears), an opt-in
`render_set_clear_state(color)` can be added without breaking the
smart default. None has surfaced yet — Rule 24 ships as
single-API.

### Edge case: custom pipeline active

The inline-clear path uses the default pipeline's solid-fill
emission. If the caller has switched to a custom pipeline
(`gpu_pipeline_use(custom)`) and calls `render_clear` before
restoring the default, the fullscreen quad goes through the
custom pipeline — usually wrong (custom pipelines rarely render
solid color). Rule of thumb: call `gpu_pipeline_use_default()`
before `render_clear` if you've switched pipelines, OR move the
clear outside the pass entirely.

### Worked migration history

Phase 4.1.2 introduced this rule. The earlier Phase 4.1.2 attempt
shipped a workaround ("call render_clear BEFORE render_begin") to
patch a multi-pass bug where clear color and pass binding got out
of sync. The smart-dispatch redesign in this rule eliminates the
trap structurally — both call orders produce correct output, and
the 38 existing single-pass call sites need zero edits because
their behavior under the smart dispatch matches the original
state-setting semantics exactly.

---

## Rule 25: `@profile("name") { ... }` annotation for scoped zones

Phase 6.3 of the GPU pipeline roadmap (shipped 2026-05-07) added
a compiler-level zone annotation that pairs each instrumented
block with a runtime aggregate visible in the profile HUD. Drop
it around any expensive stretch of code where you want a
finer-than-function-level breakdown.

**Surface:**

```bpp
@profile("ray_cast") {
    gpu_pipeline_use(ray_pipeline);
    gpu_uniform_set_frag(0, cam_buf, CAM_BUF_SZ);
    gpu_draw_full();
}
```

The parser lowers this to a synthesised `T_BLOCK` with prologue
`_prof_zone_enter("ray_cast")` and epilogue
`_prof_zone_exit("ray_cast")`. Aggregates land in
`stb/stbprofile.bsm`'s zone table; render the panel via
`profile_zones_hud_draw(x, y, sz, color)` — gates on the same
`_profile_hud_active` flag the rest of the profile HUD reads,
so a single P press surfaces both panels together.

**When to reach for it:**

- Multi-stage render phases (ray_cast / hud_overlay /
  crt_effect — exactly the fps_3d_gpu integration that
  motivated the feature).
- Frame-budget triage: "the function takes 1.9 ms, but where
  inside?".
- Cross-cutting work that doesn't decompose cleanly into
  helper functions.

**When to skip it:**

- One-statement blocks; the function-level dump already covers
  these.
- Hot loops where the enter/exit pair would dominate the cost
  it claims to measure (each call is ~50 ns including the
  beat_now_us read; loops at >10 M iter/s would feel it).

**v2 closed early-return + panic leaks (2026-05-08).** A
function that contains any `@profile` block now pays a
prologue / epilogue pair: the compiler injects
`_prof_save_enter()` right after the prologue and
`_prof_save_drain()` at the epilogue convergence point. Drain
pops every open zone above the depth recorded at entry and
credits each one's `total_us` / `count`. The chip primitive
preserves the function's int + float return value (x0 / d0 on
aarch64, rax / xmm0 on x86_64) across the call so the cleanup
is invisible to the caller. The native backends ship the
epilogue drain; the C emitter keeps v1 semantics (best-effort
leak on early return) — tests opting into v2 properties carry
`// skip-c:` accordingly.

`panic()` is wired through a function-pointer hook
(`_prof_drain_all_fp`). `init_profile` registers
`_prof_drain_all` there so a panic that fires inside a zone
closes every open zone before the trace prints. Programs that
never load stbprofile keep the hook null and the call is
skipped.

**Remaining v2 caveats:**

- Nested zones still aggregate FLAT — a parent's `total_us`
  includes its children's time. Hierarchical breakdown is a
  separate v3 feature.
- The C-emitter backend still ships v1 leak-on-early-return.
  Use `// skip-c:` when a test asserts the v2 invariant.
- Recursion deeper than `_PROF_SAVE_MAX` (32 frames) silently
  no-ops the save_enter; the corresponding drain is also a
  no-op. Same overflow contract as the v1 zone stack.

**Naming:** the original spec called it `@profile_zone(...)`
but `_zone` was redundant — the annotation IS the zone. Other
`@`-annotations in the language read as adjectives on the
following construct (`@base func`, `@gpu func`, `@seq while`);
`@profile { ... }` reads as "profile this".

---

## Rule 27: Define functions leaves-first within a file (V3 inference)

V3 return-type inference is single-pass top-to-bottom. When function A
calls function B, A's return type is inferred correctly only if B's
return type is already in `fn_ret_types` at the moment A's body is
processed — i.e. B must be defined ABOVE A in the same file.

If A is processed first, `func_type("B")` returns the TY_WORD default;
A's return type becomes TY_WORD; every downstream consumer that
expects A to return float trips E240.

```bpp
// WRONG ordering — vec2_distance returns TY_WORD silently:
vec2_distance(a, b)@base {
    ...
    return sqrt(dx * dx + dy * dy);     // sqrt undefined yet → TY_WORD
}

sqrt(x: float)@base { ... return ...; }  // defined below

// caller:
auto r: float;
r = vec2_distance(a, b);                 // E240: int → float
```

```bpp
// RIGHT ordering — sqrt resolved before vec2_distance reads it:
sqrt(x: float)@base { ... return ...; }

vec2_distance(a, b)@base {
    ...
    return sqrt(dx * dx + dy * dy);     // sqrt → TY_FLOAT, propagates
}
```

**The rule applies WITHIN a file.** Across files, the import order
handles dependency ordering — a cartridge that imports another sees
the imported cartridge's functions already typed.

**Workaround when reordering is impossible** (mutual recursion, etc):
the typed-local thunk pattern.

```bpp
my_helper(x)@base {
    auto r: float;
    r = some_forward_ref(x);            // returns TY_WORD here
    return r;                            // r is : float — return type becomes TY_FLOAT
}
```

The thunk costs one line and zero runtime (compiles to identical
assembly). The cost is real only when the forward ref is rare and
isolated. When you find yourself adding the thunk to many helpers in
the same file, reorder the file instead — that's the actual fix.

**Discovered 2026-05-11** during the Vec2 promotion arc when 3 of
the 4 scalar Vec2 helpers (vec2_length, vec2_distance,
vec2_normalize — all calling sqrt) tripped E240 at every consumer
site. Initial diagnosis blamed `add_type` for not recursing
through struct-field expressions; the actual cause was the
forward reference to sqrt, which lived BELOW the Vec2 helpers in
bpp_math.bsm at the time. Reorder + thunk-removal fixed both
symptoms cleanly.

---

## Rule 26: Audit existing cartridges before creating a new one

Before writing `stb/stb<X>.bsm` (or `src/bpp_<X>.bsm`, or any
new module), spend two minutes verifying nothing already covers
the shape:

```sh
ls stb/ | grep -i <area>          # name match (entity, pool, ai, …)
grep -rln "<symbol>\|<concept>" stb/ src/ | head
```

The check is cheap; the cost of skipping it is real. Two fresh
examples from the project's own history:

- **stbentity duplicate (2026-05-11).** Wolf3D Phase 2 Session 2
  shipped a 200-LOC `stb/stbentity.bsm` with `ent_pool_new` /
  `ent_spawn` / `ent_kill` / `ent_alive` / `ent_count` /
  `ent_at` — a near-1:1 duplicate of the existing `stb/stbecs.bsm`
  (`ecs_new` / `ecs_spawn` / etc.). The HANDOFF that recommended
  the new cartridge was itself stale. Caught only when the user
  asked "a gente não tem um stbecs?" Deleted, used stbecs with
  a custom `WolfEntPayload` component instead.
- **stbpool / stbecs split (pre-existing).** Both ship today,
  serving different niches (raw recycling vs ID-stable entity
  storage). The split is justified, but no new "Pool"-named
  cartridge should be added without first checking whether either
  already does what's needed.

The two-consumer rule (Rule 20) decides whether to *promote* a
helper into a cartridge. Rule 26 decides whether the cartridge
already exists. Rule 26 fires first — if the answer is yes, the
question of promotion is moot.

**When to skip Rule 26:** never. Even when "I'm sure nothing else
does this," the audit is two minutes; reading 200 lines back out
of git is twenty.

**Companion stale-doc check.** A HANDOFF or planning doc
recommending a "new cartridge X" is itself a hypothesis until
verified against `ls stb/`. Treat HANDOFFs as decaying — the
moment they were written, the repo started drifting away from
them. When a doc says "ship `stb<X>`," verify `stb<X>` doesn't
exist before opening the editor; if a sibling cartridge is close
enough, update the HANDOFF too so the next reader doesn't repeat
the audit.

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
  ./bpp src/bpp.bpp -o /tmp/bpp_gen1
  /tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
  cmp /tmp/bpp_gen1 /tmp/bpp_gen2            # must be identical (gen1==gen2)
  cp /tmp/bpp_gen1 ./bpp
  bash tests/run_all.sh                      # must be 120/0/11
```

Note: `--clean-cache` is a no-op (cache was removed in 2026-04-13). B++
compiles everything from source on every run. The bootstrap check is
`gen1 == gen2` (idempotent self-compilation), not a shasum pair.

---

## Rule 28: Killer use case gate — restraint over completion

Discovered 2026-05-11 during the phase annotation over-engineering
audit. The lesson is meta — it applies to every system-design
decision in the language, not just one feature.

### The pattern that creates over-engineering

Each addition to a system feels locally rational:

- "We need `PHASE_HEAP` so `@realtime` can reject malloc but
  `@io` audio setup can still allocate." → justified by ONE
  immediate need.
- "We need `PHASE_PANIC` so `sys_exit` doesn't contaminate
  effect propagation." → justified by ONE specific bug.
- "We need `@io` and `@gpu` distinct for symmetry." → "completes
  the lattice."

Cumulatively, B++'s phase system grew from 2 useful phases
(BASE/SOLO + TIME for audio safety) to 8 phases over a single
long session in 2026-04-16. Most of the resulting expansion was
inert metadata — propagating through the lattice without driving
enforcement that any consumer used. ~500 redundant annotations
accumulated in the codebase. Five months later (2026-05-11) we
audited and decided to collapse back.

### The rule — three checks before adding a feature

Before adding a new annotation, type slice, builtin, phase,
diagnostic, or any other surface-area addition to the language:

**1. Killer use case test.** "What specific bug class does this
catch that CANNOT be caught inside existing features?" If the
answer is "completes the model" or "Rust/Swift/Zig has it" — red
flag. Add the bug class to the answer, then re-evaluate. If you
cannot name 2 specific real bugs this catches, the feature is
speculative.

**2. Smaller-tool test.** "Could a special-case rule, sentinel
handling, or attribute on a builtin solve this without adding a
new dimension to the system?" PHASE_HEAP could have been a rule
inside `@realtime` ("realtime rejects malloc"), not a new phase.
PHASE_PANIC could have been special-case handling of `sys_exit`
in effect propagation, not a lattice bottom element. Prefer the
smaller tool.

**3. Restraint-bias convention test.** "Does the rule we're
about to write bias toward adoption or restraint?" The original
Rule 4 said "annotate when intent is obvious" — encouraged
adoption, led to ~500 annotation sites. The restraint phrasing
would be: "annotate ONLY when compiler verification is needed;
comments are for documentation."

### The post-mortem heuristics

Specific patterns to watch for in future feature proposals:

- **"Completes the model" justification** → demand specific
  bug class, otherwise reject.
- **Symmetry-driven additions** (we have IO, so we need GPU)
  → ask if the asymmetry was actually causing problems.
- **Long-session feature stacking** → at the end of a
  ~6+ hour session, attention is depleted; the "let's also add
  X while we're here" move suppresses the step-back audit.
  Defer system-design additions to a fresh session.
- **Documentation-via-annotation** → if the value is
  communication-to-humans, use a comment. If it's
  communication-to-the-compiler-for-proof, then the annotation
  earns its keep.
- **Match-Rust-vibe** → B++'s stated philosophy is "back to the
  future, programmer freedom, less Rust verbosity." Rust-style
  effect tracking (`unsafe`, `async`, `const fn`) is dimensional
  pinning; we already explicitly chose NOT to follow it.

### Quarterly system audit checklist

Every ~3 months, step back from active work and audit:

- Storage classes — still 3 (auto/static/extrn)? Should it grow?
- Phase annotations — how many phases? Each earning its keep?
- Tonify rules — how many? Reading the latest 5 added, do they
  bias toward restraint or accretion?
- Builtins — how many? Each used in real consumer code?
- Type slices — how many? Real bugs caught?
- Diagnostics — how many warnings? Each fires in real code?

If any number GREW since last audit without a corresponding
"killer use case" doc entry per addition: audit deeper before
adding more.

### Examples of the rule applied (project history)

- **fxlab plan 2026-05-09**: agent proposed 1100-LOC plan with
  codegen + MSL parser + .fx.meta + import keyword surgery.
  Killer-use-case test rejected it. Collapsed to ~400 LOC.
- **Vec2 promotion 2026-05-10**: agent wanted to wait for
  two-consumer rule. User correctly identified Vec2 as
  primitive-tier (always justified). Pre-existing rule needed
  refinement.
- **Phase annotation collapse 2026-05-11**: this rule.
  System being collapsed precisely because earlier additions
  failed the killer-use-case test retrospectively.
- **`ui_image` widget revert 2026-05-17 (commit `dba3e2a`)**: the
  stbui v2 engine commit `dedfb04` had added a `ui_image(img,
  frame_idx)` widget speculatively, on the assumption that
  declarative layouts would want to compose atlas frames inline.
  Killer-use-case test would have rejected at write time — there
  was no consumer asking for it. The cost surfaced two ways: (a)
  the supporting `import "stbimage.bsm"` pushed the
  stbgame → stbui → stbimage import chain past the parser's fbuf
  cap, SIGSEGV'ing every game compile (root-fixed by `cd6d00e`);
  (b) the widget itself had zero call sites across all games /
  tools / examples / tests at revert time. Both Rules 23 and 28
  pointed the same direction: revert until a real consumer needs
  the API + can shape it for what they actually want.

The rule does not say "never add features." It says "demand
specific bug-class evidence per addition, and prefer the smaller
tool when the bug can be caught without expanding the system."

## Rule 29: `struct X as Base` — domain typing without runtime cost

Discovered 2026-05-12 alongside Excalibur Feature 3 shipping. The
rule is when to reach for `struct WorldPos as Vec2;` and when to
keep using the base struct directly.

### When to declare a newtype

Use `struct X as Base;` when two values share the same byte layout
but represent DIFFERENT DOMAINS, and mixing them up has caused (or
would cause) a real bug:

| Domain pair                 | Newtype instead of bare base                    |
|-----------------------------|-------------------------------------------------|
| World coordinates / grid coords  | `struct WorldPos as Vec2; struct GridPos as Vec2;` |
| Damage points / health points    | `struct Damage as float; struct Health as float;` |
| Screen pixels / world pixels     | `struct ScreenPx as Vec2; struct WorldPx as Vec2;` |
| Input event ID / output channel ID | `struct InputId as word; struct ChannelId as word;` |

The rule is the same as Rule 28: name a specific bug class.
"Programmer passed grid coords to a world-coords function" is a
bug class with concrete examples (Wolf3D Phase 2 enemy AI sees
fractional grid positions, draw code interprets world coords as
grid). Newtype declares the distinction at the schema level so
the future programmer cannot mix them by accident.

### When NOT to declare a newtype

- The base type is already domain-specific (e.g. there is only one
  flavor of "score" — just use `: word`).
- The conversion is dynamic and unpredictable (e.g. `: word`
  parameters to syscalls — over-typing makes the FFI surface
  noisier without catching anything real).
- You are tempted to declare it "for documentation" rather than
  for real disambiguation. Documentation lives in comments.

### Mechanics

```bpp
struct Vec2 {
    x: float,
    y: float
}

// Newtype declaration. Layout is COPIED from Vec2, so a
// WorldPos value is byte-identical to a Vec2 value.
struct WorldPos as Vec2;
struct GridPos  as Vec2;

main() {
    auto wp: WorldPos;
    wp = malloc(sizeof(WorldPos));    // sizeof(WorldPos) == sizeof(Vec2)
    wp.x = 1.5;                       // dot access works through inherited layout
    wp.y = 2.5;
    free(wp);
    return 0;
}
```

`sizeof(WorldPos) == sizeof(Vec2)`, dot access reaches the same
byte offsets, and the codegen emits identical instructions to the
plain-Vec2 version. The identity exists ONLY in the parser's
struct registry — there is zero runtime cost.

### Enforcement (E262)

The 2026-05-12 ship covers full enforcement: `auto wp: WorldPos;
auto gp: GridPos; wp = gp;` fires E262 at validate time with both
struct names + a help line pointing at the conversion-helper
pattern. Implementation uses the parser's existing
`get_var_type_in_fn` (per-function struct-idx tracker) instead of
threading struct identity through the type system — sidesteps the
TY_STRUCT-is-flat limitation while delivering the rejection where
it matters (typed-local assignment).

When the conversion is genuinely intentional (e.g. you have a
function `world_to_grid(w: WorldPos): GridPos` that performs the
actual coordinate math), the helper makes the boundary explicit
and the assignment becomes `gp = world_to_grid(wp)` — no diagnostic
fires because the LHS receives the function's return value, not
another struct-typed local.

### Limitation: parameter passing across struct identities

E262 is currently scoped to T_ASSIGN. Calling `take_world(gp)`
where `take_world(w: WorldPos)` does NOT yet fire — function
parameter hints store `TY_STRUCT` flat without struct id, so the
validator cannot tell which struct the parameter expected. That
gap closes when the type system grows full struct-identity
threading. Until then, the assignment-site rejection covers the
common case where domain-mixing bugs surface.

## Rule 30: ECS layout — SoA flat vs AoSoA chunked, never AoS

`stb/stbecs.bsm` ships two storage layouts that coexist
deliberately. Pick by workload, not by aesthetic — the wrong
default hides the right perf surface.

### The three layouts (and why one is forbidden)

| Layout | Shape | Status in B++ |
|--------|-------|---------------|
| **SoA flat** | `pos_x[], pos_y[], vel_x[], vel_y[]` — one contiguous array per field, all entities live in `[0, capacity)` | Default since stbecs landed; what `ecs_new` + `ecs_set_pos` give you |
| **AoSoA chunked** | Archetypes group entities by component set; each chunk is SoA internally; archetypes form an AoS at the world level | Added 2026-05-12 in RTS Stress Arc Session 2; opt-in via `ecs_component_register` + `ecs_archetype` + `ecs_spawn_at` |
| **AoS** (Array of Structs) | `struct Entity { x, y, vx, vy }; entities[N]` — each entity is a contiguous fat struct | **DO NOT USE** as primary ECS storage |

### Why NOT AoS for the hot loop

It's the 1990s C++ pattern. Modern AAA engines (Bevy, Unity DOTS,
Flecs) all moved AWAY from AoS toward SoA / AoSoA. Three load-bearing
reasons:

1. **Access pattern mismatch.** ECS hot loops are typically
   single-component (`for each entity: update position`). AoS forces
   you to stride past every other field's bytes per entity, wasting
   cache bandwidth.

2. **SIMD-hostile.** `vec_4f_load(positions + i*4)` needs four
   adjacent floats — that's what SoA gives. AoS interleaves other
   fields between them, so SIMD needs gather/scatter (much slower).

3. **Cache pressure.** Streaming `pos_x[]` brings in only x's for
   the next 8 entities per cache line. AoS brings 4-entity strides
   with 75% of the line being unwanted fields. At 50K+ entities the
   prefetcher starts losing ground.

AoS is fine for **cold path, single-entity access** — e.g. "render
the inventory item the player is hovering over" — but never the
default for the per-frame iteration loop.

### When to use SoA flat (legacy `ecs_new` path)

Reach for `ecs_new(cap) + ecs_set_pos / ecs_get_x / ecs_alive` when:

- Every entity has the **same** component set (homogeneous world)
- Component count fits the World struct's existing slots (pos, vel,
  flags + custom via `ecs_component_new`)
- Workload is single- or dual-component touch in tight loops
- Entity count is bounded and known up front
- Iteration is **dense** (most entities are live and touched per
  frame)

This is the right default for pathfind, snake, fps_wolf3d. Don't
move them.

### When to use AoSoA chunked (archetype path)

Reach for `ecs_component_register + ecs_archetype + ecs_spawn_at`
when:

- Different entities carry **different** component sets (heterogeneous
  world — `Tile` has only Pos; `Combatant` has Pos+Vel+Hp+Faction)
- Queries are **sparse** — "iterate entities with Vel" filters out
  most of the world
- Component count grows beyond the legacy World struct's slots
- SIMD batching is on the roadmap (Session 3 of RTS Stress Arc) —
  contiguous component arrays inside chunks are SIMD-natural

The AoSoA win is **selectivity scaling**, not single-loop cache
locality. B++ legacy SoA flat is already cache-friendly because
`buf_word` arrays are linear; chunking on top adds indirection
that doesn't pay back for dense workloads.

### Empirical findings (2026-05-12)

The original RTS Stress Arc Session 2 spec claimed "3-5x cache
locality speedup" for archetype storage. That number was imported
from the Bevy/DOTS pitch, where the baseline is AoS — B++'s baseline
is already SoA via parallel `buf_word` arrays, so the comparison
doesn't apply. Two benches anchor the corrected claim.

**Dense iteration** (`tests/bench_ecs_iter.bpp`): single/dual-field
walk over 50K entities. Legacy SoA flat wins ~1.85x because per-entity
arithmetic (chunk pointer + component offset + slot stride) carries
more work than legacy's flat `buf_word[k]` indexing, and there is
no cache pattern legacy hadn't already been hitting. This is
informational, not a perf gate.

**Sparse selectivity** (`tests/bench_ecs_sparse_query.bpp`): the
workload where archetype storage genuinely wins, even without SIMD:

| Selectivity | Legacy | Archetype | Ratio |
|-------------|--------|-----------|-------|
| 10% (5K of 50K) | 88 ms | 31 ms | **2.80x** |
| 2%  (1K of 50K) | 90 ms | 8.6 ms | **10.48x** |

The ratio scales with 1/sparsity because archetype walks
`O(matching_entities)` while legacy walks `O(total_entities)`
checking each via bitmap. Asymptotically the gap widens as the
matching fraction shrinks. This is the load-bearing win for
RTS-scale heterogeneous worlds.

Where archetype storage wins (beyond sparse selectivity):

- **SIMD batching** (RTS Session 3): vec_* primitives over the
  chunk's contiguous component array. Orthogonal to selectivity —
  stacks on top.
- **Component flexibility**: registering custom components without
  growing the World struct (qualitative, not perf).

### The decision rule

Default to **SoA flat**. Switch to **AoSoA chunked** when one of
these triggers fires:

- Game grows past 5 component types per entity
- Hot query is sparse (sparsity > 5x — most entities filtered out)
- Profile shows the scan cost dominating
- SIMD batching becomes the gating perf concern

Never reach for AoS as the primary store. If you find yourself
writing `struct Entity { x, y, vx, vy }; entities[N]` for the hot
loop, stop — use the legacy ECS or register an archetype.

The two paths coexist forever. `ecs_query_each` walks both
transparently. Migration between them is per-game, optional, never
forced.

## Rule 31: Asset infra — games consume Bang 9 / Modulab formats

The B++ stack ships an **authoring surface** (Bang 9 IDE,
Modulab sprite editor, level_editor) and a **runtime surface**
(the game binary). A new game that authors its own asset
editor splits the investment two ways: every WC1-only sprite
editor is hours not spent making the existing tools better,
AND it doubles the surface area of "tool that knows about
this asset shape."

**The rule: new games consume the formats Bang 9 / Modulab
already author, unless they can name a specific bug class
those formats cause.**

### Canonical asset → tool → format → loader

| Asset kind | Authoring tool | Format | Game loader |
|---|---|---|---|
| Levels (tile grids) | Bang 9 Levels tab + `level_editor` | JSON: `{width, height, tiles[][], spawns?, resources?}` | `bpp_json` (auto-injected) + `tile_set` per cell |
| Sprites (no animation) | Modulab + Bang 9 Sprite tab | Atlas pack: `*.json` (manifest + per-sprite Modulab files) | `image_load` + `image_named_id` + `image_draw_size` |
| Sprites (with animation) | Aseprite (heavy work) + Modulab (in-context tweaks) | Aseprite-compatible sprite sheet: PNG + `*.json` sidecar (Array shape + frameTags) | `image_load` + `image_anim_id` + `image_anim_frame` + `image_draw_size_flipped` |
| Audio (one-shots) | external editor / war1tool | `.wav` | `sound_load_wav` |
| Music (BGM) | external sequencer / war1tool | `.mid` (Format 0/1, no SMPTE) | `midi_play_file` |
| Hot-reload signal | filesystem mtime | n/a | `file_watch_register` (level edits + atlas re-saves both ride this) |

### When new code touches assets

| Pattern | Action |
|---|---|
| New game wants a level format | Use level_editor JSON. Need new fields (spawn points, resource deposits, victory triggers)? **Extend the schema, don't fork.** |
| New game wants sprite tiles | Use Modulab atlas-pack. Author in Modulab; consume via `image_load`. Don't write a per-game PNG-slicing helper. |
| Asset arrives in a foreign syntax (Lua-ish, INI, XML) | Write a one-shot offline converter under `tools/<game>_<asset>_convert/` that emits the canonical Bang 9 format. The **game NEVER reads the foreign format at runtime.** |
| Hot-reload during dev | `file_watch_register` from day 1. Pathfind ↔ level_editor is the proven rail; the same code shape works for any new game. |

### Why

Tools investment compounds. Every game that opts into Bang 9
+ Modulab pays for the next game's bug fixes in those tools.
Every game that forks pays maintenance on its own editor
forever, AND the IDE tabs the team lives in (Bang 9) stays
ignorant of that game's asset shape — no in-context preview,
no round-trip edits, no shared hot-reload.

The killer use case for the convention exists today: pathfind
authors `level1.json` in Bang 9's Levels tab; the running game
picks up edits in ~30ms via `file_watch_register`. The WC1 mod
arc (Session 2 onwards) opens its `forest1.json` in the SAME
tab with the SAME shape; the SAME hot-reload path works
without one new line in level_editor. **Two consumers, one
authoring surface — the asset toolchain pays for itself
twice.**

### When to fork (Rule 28 escape hatch)

If a new game's asset shape is structurally different and
Bang 9's UI cannot represent it cleanly (a 3D voxel game where
the level_editor's 2D-tile abstraction doesn't apply, an
audio-only game with no visual asset surface, etc.), the right
answer is **a new tab in Bang 9** — not a parallel external
editor. Discuss before forking; restraint-bias per Rule 28.

### Cross-references

- **`docs/manual/asset_formats.md`** — **canonical spec** for every JSON /
  atlas / palette / tileset shape the table above mentions. When a
  new asset shape lands (e.g. animation graph, particle preset),
  document the schema there + extend the table here. THIS is where
  to look for "what fields does a level JSON carry" / "how do I
  emit a tileset-mode map."
- `how_to_dev_b++.md` Cap 16 — "Asset infra" sub-section,
  one-paragraph version of this rule pointing back at this rule +
  `asset_formats.md`.
- Modulab atlas-pack reference: `tools/modulab/` (authoring),
  `pathfind.json` (canonical consumer example),
  `stb/stbimage.bsm` `image_load` / `image_hot_reload_enable`.
- level_editor JSON reference: `tools/level_editor/level_editor_lib.bsm`
  (consumer/authoring tool — palette + tileset modes), `bang9/panels.bsm`
  (embedded host), `games/pathfind/assets/levels/level1.json`
  (palette-mode example), `games/rts1/assets/converted/maps/forest1.json`
  (tileset-mode example), `bpp_json` (loader).
- Per-project meta entries (asset choices specific to one
  game): drop a memory entry like
  `project_<game>_bang9_editable_meta.md` so future sessions
  inherit the format choice without re-deciding.

## Rule 32: `: u_word` family — unsigned semantics for `/`, `%`, `>>`

The `: u_word` / `: u_half` / `: u_quarter` / `: u_byte`
storage-class annotations declare the variable as **unsigned**.
The bit pattern stored in memory is unchanged — these annotations
only affect the codegen for three operators where signed and
unsigned semantics diverge:

| Operator | Signed (default) | With `: u_*` LHS                |
|----------|------------------|---------------------------------|
| `/`      | SDIV / IDIV      | UDIV / DIV (zero-extend RDX)    |
| `%`      | signed remainder | unsigned remainder              |
| `>>`     | ASR / SAR (sign) | LSR / SHR (zero-fill)           |

Every other operator (`+`, `-`, `*`, `&`, `|`, `^`, `<<`, `==`,
`!=`, `<`, `>`, `<=`, `>=`) is **bit-identical** in two's-complement
and stays on the existing signed path — no annotation needed.

### Dispatch is LHS-only

The smart dispatch reads `n.b.itype` (left-hand operand). The
right-hand side's annotation is ignored. This mirrors the float
dispatch convention: `auto x: u_word; auto y; z = x / y;` picks
the unsigned path. `auto x; auto y: u_word; z = x / y;` does NOT.

Rationale: the LHS carries the dividend / shifted-value semantics.
A "mixed" expression has to pick one — picking LHS keeps the
contract local to the variable the programmer annotated.

### When to use

Reach for `: u_word` when the high bit carries data, not sign:

- **Bitfields packed into the top bits of a word** — e.g. tagged
  pointers, packed array lengths, hash tables that use the high
  byte for a generation counter. Without the annotation,
  `>> N` sign-extends and corrupts the read.
- **Hash buckets, modulo addressing into power-of-two tables** —
  `idx % cap` where `idx` is a 64-bit hash. Signed `%` returns
  negative remainders for hashes with the top bit set, which
  blows the array bounds check.
- **Reading byte streams as 64-bit words** — file decoders that
  pack 8 bytes into a `word` and shift them out.

Stay on the default signed path otherwise. Most B++ code treats
`word` as a generic integer where the sign just works.

### Example

```bpp
auto h: u_word, idx;
h = hash64(key);
idx = h % cap;        // unsigned mod — never negative
```

Versus the signed default:

```bpp
auto h, idx;
h = hash64(key);
idx = h % cap;        // signed mod — may be negative when h's top bit is set
```

### Don't write the workaround

Pre-Rule-32 code worked around this with explicit masking:

```bpp
idx = (h >> 1) & 0x7FFFFFFFFFFFFFFF;   // strip sign bit, then divide
idx = idx % cap;
```

That pattern compiles and ran for years. The annotation
replaces it with one source-level character. After landing
this rule, audit for `>> 1` followed by an `& 0x7FFF...`
mask — those are candidates to migrate to `: u_word` and
delete the mask.

### Cross-references

- `src/bpp_codegen.bsm` — type-helper definitions (`is_unsigned_type`,
  `is_int_type`).
- `src/backend/chip/aarch64/a64_codegen.bsm` T_BINOP — dispatch site
  (a64 path).
- `src/backend/chip/x86_64/x64_codegen.bsm` T_BINOP — dispatch site
  (x64 path).
- `src/backend/c/bpp_emitter.bsm` T_BINOP — `(uint64_t)` cast wrap
  for the C transpiler.
- Smoke tests: `tests/test_unsigned_div.bpp`, `_mod.bpp`, `_shr.bpp`,
  `_arith_shared.bpp`.

## Rule 33: Cartridge generality tiers — fully-generic vs genre-generic

Every `stb*` cartridge in the library sits at one of two tiers:

| Tier | Audience | Examples |
|------|----------|----------|
| **Fully-generic** | Any game or tool, regardless of genre | `stbcol` (geometry), `stbgrid` (cell storage), `bpp_buf` (raw buffers), `stbstr` (strings) |
| **Genre-generic** | Programs in a specific genre lane | `stbflow` (RTS / TD crowd pathing), `stbphys` (platformer physics), `stbtile` (tilemap renderer), `stbecs` (entity simulation) |

A program-specific module (`wc1_*.bsm`, `fps_*.bsm`, `snake_*.bsm`) is
neither — it lives **inside** the game's source folder, not `stb/`.

### Why the distinction matters

It changes who is allowed to import the cartridge and what kind of
features may live in it.

- **Fully-generic cartridges** are leaf primitives. They store data
  or compute pure functions on it. They must not embed genre
  semantics. Any tier above can import them.
- **Genre-generic cartridges** ship complete subsystems for one
  domain. They may import fully-generic primitives freely and may
  carry genre-shaped APIs (`flow_compute(goal_x, goal_y)`,
  `phys_apply_gravity(body)`). They must not embed *game-specific*
  semantics (player factions, unit names, level layouts).

The rule "genre-generic cartridges may consume fully-generic
primitives" is what made `stbgrid` happen — `stbflow` was carrying
three ad-hoc grids that were really storage, not crowd pathing.
Lifting them into `stbgrid` made `stbflow` smaller, gave non-RTS
games (roguelikes, Sokoban puzzles, tower defense placement
preview) a real handle to use.

### Decision: which tier does a new cartridge belong in?

Ask the two questions in order.

1. **Could a non-`<genre>` game pull this cartridge and get value?**
   Roguelike, snake, sokoban, tetris, top-down adventure, side-
   scroller, puzzle, anything outside the genre that surfaced the
   need. If yes → fully-generic. If no → genre-generic.
2. **Does the cartridge store data without naming what the data
   means?** A grid of words, a buffer of bytes, a geometry test,
   a string. If yes → almost always fully-generic. Anything that
   names the semantics ("occupancy", "tile class", "unit class",
   "physics body") is genre-generic at minimum.

WC1's tile-occupancy work surfaced this distinction the hard way.
The first instinct was "put it in `stbcol`" — but `stbcol` is
fully-generic geometry primitives, not stateful storage. The
second instinct was "put it in `stbflow`" — but `stbflow` is RTS
crowd pathing, and occupancy is useful to puzzle games + roguelikes
that never touch crowd movement. The answer was a new fully-
generic cartridge (`stbgrid`) plus a `wc1_collision_*` wrapper
that gave the cells occupancy semantics in-game.

### Cross-cartridge import rules

```
games/<g>/<g>_*.bsm       → may import fully-generic + genre-generic
stb/<genre>*.bsm          → may import fully-generic; not other genre cartridges
stb/<fully-generic>.bsm   → must not import any other cartridge
```

The fully-generic floor is a leaf set with zero outgoing
dependencies into the rest of `stb/`. This is what keeps the
import graph DAG-shaped (no cycles, no surprise transitive pulls
— see Rule 23's `dedfb04` lesson where `stbui` reached down into
`stbimage` and broke the rule).

### When a genre-generic cartridge wants a fully-generic primitive

That is the healthy graduation pattern (`stbflow` → `stbgrid`). The
sequence is:

1. The genre cartridge already carries an ad-hoc version of the
   primitive (or a game is about to ask for the same shape).
2. A second consumer exists or is concrete enough that Rule 20
   (two-consumer rule) fires.
3. Lift the primitive into a new fully-generic cartridge, with
   the anti-speculation guard in the header (Rule 28).
4. Refactor the genre cartridge to consume the new primitive.
   This becomes the second-consumer test that validates the API.

The graduation is single-commit, mechanical, and bisect-friendly.
If it turns into a sprawling redesign, the primitive was not
fully-generic enough — back out and rethink.

### When NOT to graduate

- One consumer, no second on the horizon. Stay in the consumer's
  source folder (e.g. `wc1_collision.bsm` keeping the helpers
  there) — Rule 20 + Rule 28.
- The "primitive" naturally embeds genre vocabulary. If you
  can't write the header docstring without saying "unit",
  "player", "level", or "enemy", it is not fully-generic.
- The graduation would force a cycle in the import DAG (e.g. a
  fully-generic cartridge wanting to import a genre cartridge to
  work). The shape is wrong — keep it in the game module.

### Cross-references

- Rule 20 — two-consumer promotion timing.
- Rule 23 — cartridge minimalism (the floor-only rule for entry-
  point cartridges).
- Rule 26 — audit before creating; covers the "is there already a
  cartridge that does this?" step.
- Rule 28 — killer use case gate.
- `docs/plans/sidequest_stbgrid.md` — worked example of a fully-
  generic graduation that lifted stbflow's ad-hoc grids out into
  `stbgrid`, with `wc1_movement.bsm` consuming both.
