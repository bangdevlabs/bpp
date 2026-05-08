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

## Rule 4: Phase annotations (`@base` / `@solo` / `@time` / `@io` / `@gpu`)

For functions where the intent is clear:

| Pattern | Action |
|---------|--------|
| Pure function: reads args, computes, returns value. No global writes. | Add `@base` |
| Side-effect function: writes globals, calls impure functions | Add `@solo` (optional — compiler infers) |
| Audio callback / time-bounded path: no malloc, no IO, bounded time | Add `@time` |
| Touches files / network / stdout / syscalls | Add `@io` |
| Touches GPU resources (calls `_stb_gpu_*`) | Add `@gpu` |
| Unclear / mixed | Leave unannotated (compiler auto-classifies) |

The annotation is an `@` sigil glued to the phase word, with no space, placed
between the parameter list and the function body:

```bpp
static _dist(ax, ay, bx, by)@base {
    return sqrt((bx-ax)*(bx-ax) + (by-ay)*(by-ay));
}

void mixer_callback(buf, n)@time {
    // no malloc, no io, no gpu
}
```

**Only annotate when the intent is OBVIOUS.** Don't guess. The compiler classifies automatically for unannotated functions.

**WARNING: do NOT mark `@base` on functions that call builtins** (malloc, free, putchar, str_peek, envp_get, sys_*, etc.). The classifier treats ALL builtins as impure. Only pure pointer-arithmetic readers qualify (arr_get, arr_len, etc.). W013 will catch mistakes — trust it.

**Effect annotations form a strict-to-loose ladder:**

```
@time     (most strict — can only call base or other time)
   ↓
@io / @gpu  (sibling categories — can call base or own kind)
   ↓
@base     (pure, callable by everyone)
   ↓
@solo     (catch-all — most permissive, can call anything)
```

The killer use case: an audio callback annotated `@time` is verified
by the compiler to never call `malloc`, `putchar`, or anything `@io` /
`@gpu` — eliminating an entire class of audible glitches by proof
rather than testing.

**Status (2026-04-28):** the `@phase` sigil syntax is the canonical
form across all 55+ stb/games/tools/bang9 files. The old `: phase`
colon form was retired in the 2026-04-28 migration. `realtime` was
renamed to `time` in the same sweep.

| Module category | Suggested effect annotation |
|---|---|
| `_stb_gpu_*` primitives, `render_*`, `sprite_*`, `tile_draw` | `@gpu` |
| `*_load`, `audio_*`, `_stb_audio_tone_*`, `_stb_init_window` | `@io` |
| Audio callbacks (e.g. `_aud_square_cb`) | `@time` |
| Pure helpers (math, string, array, hash) | `@base` |
| Game `init` / `update` / orchestrator `main` | leave unannotated (`@solo` inferred) |

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
    auto atlas = image_load("assets/sprites.atlas.json");
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
