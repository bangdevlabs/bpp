# How to Dev B++

The canonical guide for working on B++ itself — the compiler, the standard library, the backends, the tests. If you are writing *a program in B++*, read `bpp_language_reference.md` first. This document is for people who change the compiler source and need the production pattern in one place.

B++ is self-hosting. Every change to the compiler or to a module it imports has to pass through the bootstrap cycle. Every line of code in `src/` and `stb/` follows the same expert-mode conventions. This guide is how you work inside those constraints without breaking anything.

---

## Part 1: The Daily Loop

### The Golden Rule

**Never overwrite `./bpp` with an untested binary.** The repo binary is the only tool that can build the compiler. If it gets corrupted, you must recover it from git:

```bash
git show HEAD:bpp > ./bpp && chmod +x ./bpp
```

### What earns a bootstrap / full suite?

Token-cost awareness: don't bootstrap + run all tests after every
touch. Rebuild only what you actually affected.

| What you changed | Minimum verification |
|------------------|----------------------|
| A game file (`games/<game>/*.bpp` or `.bsm`) | Compile just that game: `./bpp games/<game>/<game>.bpp -o /tmp/out` |
| A tool (`tools/<tool>/*.bpp` / `.bsm`)       | Compile just that tool |
| An `stb/*.bsm` module                         | Full suite (`./tests/run_all.sh`) — tests import stb |
| A `src/bpp_*.bsm` runtime module              | Full suite — every program auto-imports them |
| The compiler (`src/bpp.bpp`, parser, lexer, codegen, validate, dispatch, import, types, bug, bo, internal, defs) | Full **bootstrap cycle** + full suite |
| Platform layer (`src/backend/os/<os>/*`)      | Compile at least one game that uses it (stbgame) |
| Chip / target backend (`src/backend/chip/...`, `target/...`) | Bootstrap + full suite (the compiler itself re-emits through them) |
| Docs (`docs/*.md`, `README.md`)               | Nothing to verify — read-only |

The most common mistake is running `./bpp src/bpp.bpp -o /tmp/bpp_bt` + `./tests/run_all.sh` after touching a game. That's a 2-second rebuild of the entire compiler + 70 tests just to verify the game compiles. Compile the game alone instead.

### Bootstrap cycle

Every change to the compiler source runs through this:

```bash
# 1. Build gen1 with the current repo compiler
./bpp src/bpp.bpp -o /tmp/bpp_gen1

# 2. Use gen1 to recompile the source
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2

# 3. Verify the two binaries are byte-identical
shasum /tmp/bpp_gen1 /tmp/bpp_gen2
# or: diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2)

# 4. Run the suite against the new compiler
cp /tmp/bpp_gen2 ./bpp
./tests/run_all.sh

# 5. Install globally if you want other projects to see the change
sh install.sh --skip
```

If `./tests/run_all.sh` shows regressions, `./bpp` is still the old one — you have not installed gen2 yet. Revert your source change, recover the repo binary from git if needed, and investigate.

### When gen1 differs from gen2

Changing how the compiler emits code for a feature the compiler itself uses (register allocation, calling convention, etc.) produces a one-cycle oscillation: gen1 has old codegen + new source, so it emits the *old* pattern; gen1 recompiling itself produces gen2 with the *new* pattern; gen2 and gen3 then match because both were compiled by a new-pattern compiler.

```bash
# If gen1 != gen2, check convergence with gen3
/tmp/bpp_gen2 src/bpp.bpp -o /tmp/bpp_gen3
shasum /tmp/bpp_gen2 /tmp/bpp_gen3
# gen2 == gen3 means stable (1-cycle oscillation was expected); install gen3
```

If **gen2 != gen3**, the compiler output depends on which binary compiled it — that is a real codegen bug, not oscillation. Diagnose with `bug --dump-str` on both binaries to find the divergence, or with `--show-deps` and `--stats` to see the module graph and symbol counts.

The `.bo` cache was removed in 0.23.x, so stale-cache is no longer a suspect. Every compile rebuilds from source in ~0.1 seconds.

### Debugging with `bug`

The native B++ debugger is always available, no flags required. Every compile already writes a `.bug` file alongside the binary:

```bash
bpp hello.bpp -o hello         # produces ./hello and ./hello.bug

# Dump mode — inspect the debug map
bug hello.bug                  # prints functions, structs, globals, externs

# Observe mode — run and trace
bug ./hello                    # call tree, crash backtrace, local variables
bug --break main ./hello       # stop at function entry, print frame, continue
```

`bug` reads the `.bug` format directly and drives the target through `debugserver` (macOS) or `gdbserver` (Linux) via the GDB remote serial protocol. No DWARF, no LLDB, no codesign dance. See `docs/debug_with_bug.md` for the complete feature list.

**`bug --dump-str` is the fast path for string-corruption bugs.** It shows exactly what bytes the data section contains at each string literal site. When a Mach-O writer bug corrupted `"break"` into nonsense during the Apr 16 session, `--dump-str` located the bad page in under a minute, versus three days of blind archaeology.

### Smoke tests

```bash
# Quickest possible check the compiler still works
echo 'main() { putchar(72); putchar(10); return 0; }' > /tmp/test.bpp
./bpp /tmp/test.bpp -o /tmp/test_out && /tmp/test_out
# Should print: H

# Test both pipelines
./bpp /tmp/test.bpp -o /tmp/test_mod               # modular (default)
./bpp --monolithic /tmp/test.bpp -o /tmp/test_mono # monolithic (forced)

# Full test suite
./tests/run_all.sh
```

### Platform gotchas

**Exit 137 (SIGKILL) on macOS.** The kernel's code signature cache kills a binary based on the previous binary at the same path. Always compile to a fresh path (`/tmp/bpp_genN`), never overwrite and re-run in place.

**Exit 138 (SIGBUS) on ARM64.** Usually misaligned memory access. Check for unicode in string literals (not supported), or pointer arithmetic that lands on an unaligned address.

**Exit 139 (SIGSEGV).** Null pointer dereference — often a struct pointer that was never initialized. See Rule 9 below for the `auto x: T` pitfall that causes this silently.

**Raw offset access on sliced structs returns garbage.** `*(node + 8)` does not respect slice packing. Always use typed access: `auto n: Node; n = node_ptr; n.a`. See Rule 8 below.

### What to never do

- Do NOT run `codesign` on `./bpp` — it changes bytes and breaks bootstrap reproducibility.
- Do NOT leave `bpp` processes running in the background.
- Do NOT create intermediate binaries in the repo (`bpp_new`, `bpp_check`, etc.) — use `/tmp/`.
- Do NOT test with binaries from `build/` or any stale path.
- Do NOT write code comments in Portuguese. English, user-manual style.

---

## Part 2: Tonify Expert Mode

Tonify is the name of the production code style. Every `.bsm` and `.bpp` file in `src/`, `stb/`, `games/`, and `tests/` follows the same thirteen rules. "Tonification" means sweeping a file against the checklist and bringing it up to standard. When you write new code, write it tonified from the start.

The philosophy: **simple by default, explicit when needed.** A beginner can write `auto x;` and the compiler figures it out. An expert writes `extrn x: float;` and the compiler verifies it. The same progression runs through every dimension of the language.

### Per-file procedure

1. Read the entire file before touching anything.
2. Apply each rule below in order (they compose cleanly top to bottom).
3. Bootstrap: `./bpp src/bpp.bpp -o /tmp/bpp_verify`.
4. If the bootstrap breaks, revert the last change and investigate before continuing.
5. Move to the next file.

### Rule 0 — Module discipline (`load` vs `import`, `stub`)

For every import in a game file (`.bpp` or game-local `.bsm`):

| Pattern | Action |
|---------|--------|
| Imported file lives in the SAME directory as the entry `.bpp` | Change to `load` |
| Imported file is from `stb/`, `src/`, or an installed path | Keep `import` |

For every callback function in engine modules (e.g. `stbgame.bsm`):

| Pattern | Action |
|---------|--------|
| Function exists to be overridden by the entry `.bpp` | Add `stub`: `stub game_init() { }` |
| Function is a real implementation | Keep as-is |

Applies to game files (batches 4 and 7 below). Compiler internals always use `import`.

### Rule 1 — Storage classes

For every `auto x;` at file scope:

| Pattern | Change to | Example |
|---------|-----------|---------|
| Set once in init, never written again | `extrn x;` | `extrn _ARR_HDR;` |
| Read/written by worker functions | `global x;` | `global _last_dt;` |
| Compile-time literal | `const X = value;` | `const CELL = 10;` |
| Intentionally serial, don't promote | `auto x: serial;` | `auto _temp: serial;` |
| Mutable per-frame state | `auto x;` (keep) | `auto head_x;` |

### Rule 2 — Visibility (`static`)

For every function and variable — not just `_`-prefixed ones:

| Pattern | Action |
|---------|--------|
| Function called only within this file | Add `static` |
| Function called cross-module | Keep public |
| Function is public API by design (even if currently uncalled) | Keep public |
| Variable used only in this file | Add `static` |
| Variable used cross-module | Keep public |

The `_` prefix is a convention, not the rule. A function named `helper()` that is only called inside its own file is just as private as `_helper()`. Conversely, `_stb_get_time()` is called cross-module despite the prefix — grep decides, not naming.

Check every function via grep: `grep -rn "function_name" src/ stb/ games/ tests/` to verify whether any cross-module caller exists. Do not skip non-`_` names.

### Rule 3 — Return type (`void` + implicit return + bare `return;`)

For every function:

| Pattern | Action |
|---------|--------|
| Returns meaningful value (`return x + y;`) | Keep as-is |
| Only `return 0;` at the end (side-effect function) | Remove `return 0;` + add `void` |
| `void` function with early-exit `return 0;` in the middle | Change to bare `return;` |
| Mixed: some paths return value, some don't | Keep explicit returns, no `void` |
| `return 0;` is the ONLY statement | Remove it (implicit return handles it) |

Bare `return;` (no expression) is supported and produces an implicit `return 0`. Use it in `void` functions for early-exit guards.

### Rule 4 — Phase annotations

For functions where the intent is clear:

| Pattern | Action |
|---------|--------|
| Pure function: reads args, computes, returns value. No global writes. | Add `: base` |
| Side-effect function: writes globals, calls impure functions | `: solo` (optional — inferred) |
| Audio callback / realtime path: no malloc, no IO, bounded time | Add `: realtime` |
| Touches files / network / stdout / syscalls | Add `: io` |
| Touches GPU resources (calls `_stb_gpu_*`) | Add `: gpu` |
| Unclear / mixed | Leave unannotated (compiler auto-classifies) |

**Only annotate when the intent is OBVIOUS.** Don't guess. The compiler classifies automatically for unannotated functions.

**Do NOT mark `: base` on functions that call builtins** (malloc, free, putchar, sys_*). The classifier treats every builtin as impure. Only pure pointer-arithmetic readers qualify (arr_get, arr_len). W013 catches mistakes.

Effect annotations form a strict-to-loose ladder:

```
realtime  (most strict — can only call base or other realtime)
   ↓
io / gpu  (sibling categories — can call base or own kind)
   ↓
base      (pure, callable by everyone)
   ↓
solo      (catch-all — most permissive)
```

The killer use case: an audio callback annotated `: realtime` is verified by the compiler never to call `malloc`, `putchar`, or anything `: io` / `: gpu` — eliminating an entire class of audible glitches by proof rather than testing.

### Rule 5 — Control flow (`switch`, `for`, `continue`)

For if-chains that test one variable against multiple constants:

| Pattern | Action |
|---------|--------|
| 3+ sequential `if (t == X)`  against the same variable | `switch (t) { X { } Y { } else { } }` |
| 3+ `if (cond1)` `else if (cond2)` with different conditions | `switch { cond1 { } cond2 { } else { } }` |
| 2-branch `if/else` | Keep `if/else` — switch is overkill |

Two switch forms:

```bpp
// Value dispatch — tests expr against constants
switch (state) {
    IDLE, PATROL { move(); }
    ATTACK       { fire(); }
    else         { }
}

// Condition dispatch — first truthy arm wins
switch {
    can_attack(e)  { attack(e); }
    can_see(p)     { chase(p); }
    else           { idle(); }
}
```

W021 fires if no `else` arm (exhaustiveness hint).

Loop cleanup:

| Pattern | Action |
|---------|--------|
| `while` with manual counter (`i = 0; while (i < n) { ... i = i + 1; }`) | Convert to `for` |
| `if (skip) { i = i + 1; } else { ... }` | `if (skip) { continue; } ...` |
| Deeply nested if/else that could be flattened | Refactor with `continue` |

### Rule 6 — Slice types on struct fields

| Field range | Annotation | Example |
|-------------|-----------|---------|
| 0-1 | `: bit` | `alive: bit` |
| 0-3 | `: bit2` | `tier: bit2` |
| 0-7 | `: bit3` | `direction: bit3` |
| 0-15 | `: bit4` | `level: bit4` |
| 0-31 | `: bit5` | `quality: bit5` |
| 0-63 | `: bit6` | `slot: bit6` |
| 0-127 | `: bit7` | `score: bit7` |
| 0-255 | `: byte` | `on_ground: byte` |
| 0-65535 | `: quarter` | `w: quarter, h: quarter` |
| Signed 32-bit | `: half` | `gravity: half` |
| Full 64-bit | (none) | `x, y` |
| 128-bit SIMD (4× float32) | `: double` | `velocity: double` |

Bit-sliced fields pack LSB-first into the current byte. When a bit field would overflow the current byte, a new byte starts. Non-bit fields terminate bit-packing (remaining bits in the last byte are padding).

### Rule 7 — Comments

| Pattern | Action |
|---------|--------|
| Comment says `return 0` after removal | Delete the comment too |
| Comment mentions a renamed module (stbarena, stbio) | Update to new name (bpp_arena, bpp_io) |
| Comment is accurate | Keep |

### Rule 8 — Typed struct access for sliced types

Raw offset access `*(ptr + N)` does NOT respect sliced struct layouts. B++ packs sliced fields without alignment padding, so the offset you would guess from declaration order is wrong. Use typed access:

| Pattern | Action |
|---------|--------|
| `*(node + 8)` with hardcoded offset on sliced Node | `auto n: Node; n = node; n.a` |
| `auto p;` where p points to a struct | Annotate: `auto p: StructName;` |
| Stale offset constants from pre-slicing era | Delete if unused |

Why: raw `*(ptr + N)` emits a blind 8-byte load at `ptr + N`. Typed access calls `get_field_offset` at codegen time to compute the real packed offset. Example: in Node, `.a` lives at byte offset 1 (not 8) because `ntype: byte` consumes 1 byte with no padding after. Code that hardcodes `*(node + 8)` reads bytes straddling `.a` and `.b` — garbage.

**Escape hatch.** Manual records (`RECORD_SZ`, token slots, function records) are unsliced by design and use named offset constants like `FN_NAME = 8`. Those are word-aligned layouts independent of the sliced-struct system. Rule 8 applies only to sliced structs.

### Rule 9 — Local struct allocation (`var` vs `auto x: T`)

Two ways to declare a struct-typed local look similar but allocate different things. Picking the wrong one is silent — the program compiles and then segfaults at the first field write.

| Pattern | Allocates | Use when |
|---------|-----------|----------|
| `var x: T;` | `sizeof(T)` bytes on the stack, in place. `x` IS the struct. | Short-lived struct used only inside the function. No pointer to `x` escapes. |
| `auto x: T; x = malloc(sizeof(T));` | 1 word slot, then heap allocation. `x` holds the pointer. | Struct outlives the function, is returned, or stored elsewhere. |
| `auto x: T;` alone | 1 word slot, **uninitialized (== 0)**. `x` is null. | Only when `x` will be assigned a real pointer before any field access. |

**The pitfall.** `auto x: T; x.field = v;` looks like a stack-struct declaration but is silently broken — `x` is null until you assign a pointer to it, so the field write goes to address 0 and SIGBUSes. The compiler does not warn because the type hint is legal; only the missing initialization is the bug.

Rule of thumb: if the struct lives entirely within the function and you do not need a pointer to it elsewhere, use `var`. If it crosses function boundaries, use `auto x: T;` plus `malloc`.

### Rule 10 — Portable builtins (cross-platform first)

When adding functionality user programs will call from any platform (macOS, Linux, C transpilation), expose it as a B++ builtin. Don't push platform conditionals onto user code. Users should never write `if (platform == macos) { } else { }`.

The pattern (worked examples: `malloc`, `malloc_aligned`, `memcpy`, `putchar`, `getchar`):

| Layer | File | Purpose |
|-------|------|---------|
| Public API | `src/bpp_<area>.bsm` | Declares the function; imports the per-OS implementation. |
| OS implementation | `src/backend/os/<os>/_<area>_<os>.bsm` | Uses `sys_*` syscalls and OS-specific constants. One file per OS. |
| C transpiler mapping | `src/backend/c/bpp_emitter.bsm` | Maps the call to a libc equivalent so `bpp --c` produces working C. |

**Decision rule.** If a user program would need to write `if (platform == macos) { ... } else { ... }` to do this on multiple platforms, the thing is a missing builtin. Add it.

**Don't expose as user-facing API:** `sys_mmap`, `sys_open`, `sys_lseek`, `MAP_PRIVATE`, file mode bits, anything in `src/backend/os/<os>/` directly. These exist so the builtins above can use them.

### Rule 11 — Ternary and short-circuit

Use ternary for value-selecting `if/else`:

| Before | After |
|--------|-------|
| `if (cond) { x = a; } else { x = b; }` | `x = cond ? a : b;` |
| `auto val; if (hp < 20) { val = 99; } else { val = 0; }` | `auto val = hp < 20 ? 99 : 0;` |

Use `&&` / `||` for null guards and combined conditions. B++ short-circuits: the right operand only runs if the left is truthy, so `if (ptr != 0 && ptr.field > 0)` is memory-safe — the dereference never executes on a null pointer.

| Before | After |
|--------|-------|
| `if (p != 0) { if (p.field > 0) { ... } }` | `if (p != 0 && p.field > 0) { ... }` |
| `if (a) { if (b) { if (c) { ... } } }` | `if (a && b && c) { ... }` |

**When not to use.** Multi-statement else branches → keep `if/else`. Ternary nesting beyond two levels → use explicit control flow. Right operand of `&&` with observable side effects you want unconditionally → split.

Both forms desugar to `T_TERNARY` in the AST. Adding the idiom costs zero runtime overhead.

### Rule 12 — Float bits need `: float`

Storing a float value into a bare `auto` local converts it to int via FCVTZS — the IEEE 754 bit pattern is gone. E232 and E233 catch this at compile time. Practical rule: when a local needs to hold a float, annotate it.

| Pattern | Action |
|---------|--------|
| `auto sr; sr = 44100.0;` | `auto sr: float;` |
| `auto pi; pi = 3.14159;` | `auto pi: float;` |
| `auto x; x = some_float_func();` | Annotate `x: float` |
| Truncation IS the intent | Drop the decimal: `auto x; x = 3;` |

The explicit conversion path (`FCVTZS` on ARM64, `CVTTSD2SI` on x86_64) fires for int-annotated destinations (`: word`, `: byte`, `: half`, `: quarter`). `: int` synonym was removed in 0.23.x — `: word` is the canonical name.

### Rule 13 — Public API parameters use explicit hints

Functions declared without `static` form the module's public API. Parameters that expect non-word types (float, byte, struct pointer, small enum) should carry an explicit hint.

| Pattern | Action |
|---------|--------|
| `audio_play(rate, buf, len)` where `rate` is float | `audio_play(rate: float, buf, len: word)` |
| `static helper(tmp)` where `tmp` is internal | Keep as-is — `static` is implementation detail |
| `pos_set(x, y)` where both are word | Keep as-is — word is the default |
| `set_color(r, g, b, a)` where all are float | `set_color(r: float, g: float, b: float, a: float)` |

W025 nudges when a non-`static` function uses an un-hinted parameter in float arithmetic — the exact shape where a caller cannot tell from the signature whether the param wants an int or a float.

---

## Part 3: Architectural Discipline

### Where to implement a feature

Before writing code, ask: **"Would a new backend need to reimplement this?"** If yes, the feature is in the wrong layer. Push decisions as early in the pipeline as possible.

```
Source → Lexer → Parser → AST → Dispatch/Validate → Codegen → Binary
         ↑                 ↑            ↑                ↑
     token rules     SEMANTICS     analysis        EMISSION
     (rarely new)    (go here)    (classification)  (chip/OS specific only)
```

Decision tree:

1. **Defines what the program does** (language semantics: short-circuit, operator lowering, control flow sugar)
   → **Parser / frontend.** One implementation, all backends inherit. Example: `a && b` → `T_TERNARY(a, b, 0)`. Backend never sees `&&`. A future RISC-V or WASM backend gets short-circuit for free.

2. **Needs a chip-specific instruction** (SIMD, bitfield extract, atomic ops)
   → `src/backend/chip/<arch>/`. Example: UBFX on ARM64 for bitfield loads, SHR+AND on x86_64.

3. **Needs an OS syscall or API** (memory allocation, I/O, time, threads)
   → `src/backend/os/<os>/`. Example: `_bmem_linux.bsm` uses `sys_mmap`, `_bmem_macos.bsm` wraps libSystem.

4. **Needs chip+OS binary format** (section layout, relocations, entry point)
   → `src/backend/target/<chip>_<os>/`. Example: Mach-O writer knows ARM64 relocations AND macOS loader.

**Portability test.** Imagine adding a new backend (say, RISC-V Linux). Count how many files you would need to touch to support the feature:

- Zero (feature is in parser/frontend): right place.
- One (new chip or OS file): acceptable.
- Two or more existing files: too spread out. Centralize before adding the new backend.

**Anti-pattern.** If the same *rule* (not the same instruction) is implemented in `a64_codegen.bsm` AND `x64_codegen.bsm` AND `bpp_emitter.bsm`, the decision belongs in the frontend. Backends should differ in HOW they emit, not WHAT they decide to emit. Historical example: `if (0) { ... }` DCE was duplicated in both codegens. Short-circuit was heading the same way until caught. Both belong in the parser.

### Backend layout

```
src/backend/
  chip/                     — pure CPU: instruction encoding + AST dispatch
    aarch64/                  a64_enc.bsm, a64_codegen.bsm
    x86_64/                   x64_enc.bsm, x64_codegen.bsm
  os/                       — pure OS: platform APIs, syscalls, runtime
    macos/                    _stb_core_macos, _stb_platform_macos,
                              _bsys_macos, _brt0_macos, _bmem_macos,
                              _stb_audio_macos, bug_observe_macos
    linux/                    mirror for Linux
  target/                   — chip + OS: binary format writers
    aarch64_macos/            a64_macho.bsm (Mach-O writer)
    x86_64_linux/             x64_elf.bsm (ELF writer)
  c/                        — alternative backend: C source emitter
    bpp_emitter.bsm
```

**chip/** knows about the CPU but nothing about the OS. **os/** knows about the OS but nothing about the CPU. Syscall numbers live in `_bsys_<os>.bsm`; startup globals in `_brt0_<os>.bsm`; the allocator in `_bmem_<os>.bsm`. **target/** combines chip + OS into the final binary format. **c/** is the portable escape hatch.

When a new chip/OS combo arrives (Linux ARM64, Intel macOS, Windows x86_64), a new `os/<os>/` or `target/<chip>_<os>/` folder drops in without disturbing existing backends.

### Auto-import system

The compiler injects imports automatically so user programs don't write boilerplate:

**Target-suffix fallback.** `bpp_import.bsm` carries a global `_imp_target` set to `"macos"` by default, `"linux"` with `--linux64`. When `find_file()` cannot resolve a name, it retries with `_<target>` injected before `.bsm`. So `import "_stb_platform.bsm"` resolves to `_stb_platform_macos.bsm` or `_stb_platform_linux.bsm` automatically.

**Universal core.** Every program gets `bpp_array`, `bpp_hash`, `bpp_buf`, `bpp_str`, `bpp_io`, `bpp_math`, `bpp_file`, `bpp_arena`, `bpp_beat`, `bpp_job`, `bpp_maestro`, `bpp_mem`, and `_stb_core_<target>` injected without an `import` line. These are compiler primitives and universal utilities, not game library.

**Game engine.** `process_file()` detects when the file being processed is `stbgame.bsm` and pulls `_stb_platform_<target>.bsm` into the import graph. The result: programs that import `stbgame` get the platform layer transparently. Programs that don't stay lightweight — no window code linked in.

To add another auto-injected pairing in the future ("any program that imports `stbnet` should also pull `_stb_net_<target>.bsm`"), copy the `_path_is_stbgame` pattern.

### Adding a new builtin

A builtin is a function name the compiler recognizes specially and emits machine code for, instead of treating it as a regular function call. `malloc`, `peek`, `sys_write`, `mem_barrier` are all builtins.

Four places to touch:

1. **`src/backend/chip/aarch64/a64_codegen.bsm`** — emit ARM64 machine code. Find the handler block (search for `"sys_write"`) and add your case using the `a64_str_eq(n.a, "name", len)` pattern. For syscalls, import the BSYS_* constant from `_bsys_macos.bsm`.

2. **`src/backend/chip/x86_64/x64_codegen.bsm`** — emit x86_64 machine code. Same structure. Syscall numbers come from `_bsys_linux.bsm`.

3. **`src/bpp_validate.bsm`** — add to `val_is_builtin()` so the validator doesn't reject it as `E201: undefined function`. Always add to all three at the same time.

4. **`src/backend/c/bpp_emitter.bsm`** — if the builtin should work through `bpp --c`, add a handler in the call emission code. Map to the libc/POSIX equivalent or inline a C expression. Add any new header to `emit_runtime()`. If the builtin is native-only, skip this step but document why in the validator entry.

After all four edits, bootstrap.

---

## Part 4: Writing Modules

### stb module conventions

- File name: `stb<name>.bsm`. `.bsm` marks "B-Single-Module".
- Function prefix: every public function starts with the module's name. `path_new`, `hash_set`, `tile_collides`. Private helpers start with underscore: `_path_heap_swap`.
- Header comment: a paragraph describing what the module does, the intended use, and a code snippet showing the canonical pattern. Plan 9 / Bell Labs man pages, not marketing.
- Init function: `init_<name>()` that performs any one-time setup. Even if the body is `return 0;`, define it for symmetry.
- All comments in English, full sentences, user-manual style.

### Leaf vs coupled

A leaf module has zero `import` statements pointing at other stb files. It only depends on compiler builtins and auto-injected core modules. Leaf modules are testable in isolation.

Examples of leaf modules today: `stbcol`, `stbcolor`, `stbecs`, `stbinput`, `stbpath`, `stbpool`, `stbscene`, `stbsound`.

Couple a module only when the **purpose** demands it. `stbphys` imports `stbtile` because "platformer physics for tilemaps" is its purpose. `stbtile` imports `stbimage` because loading a tileset PNG needs a decoder. Those couplings are honest.

`stbpath` could have imported `stbtile` for a `path_load_from_tilemap` helper, but A* on a grid is not inherently a tilemap operation — it works on puzzles, AI planners, network topology, anything with cells. The bridge to a tilemap lives in the calling game as a 5-line loop, documented in `stbpath`'s header. The result: `stbpath` is a leaf, the compiler's own test suite can import it without GPU init.

**Acoplamento por propósito, não por conveniência.**

### Public structs and memory ownership

If the module exposes a struct, define it at the top of the file with `struct Name { field1, field2, field3 }`. Document each field on its own line. Callers create instances via `name_new(...)` returning a pointer. Field access uses dot notation; the compiler emits the right load/store size from the type hint.

State the memory contract in the header:

- "Returns a pointer the caller must free with `name_free`."
- "Copies the input bytes — the caller's buffer can be reused immediately."
- "Returns a pointer into a per-call scratch buffer; do not free."

Anything ending in `_new` returns owned memory and pairs with `_free`.

### Adding a new compiler module

1. Create `src/bpp_mymodule.bsm`.
2. Add explicit imports at the top for everything it uses:
   ```
   import "bpp_defs.bsm";
   import "bpp_array.bsm";
   import "bpp_internal.bsm";  // if you use buf_eq, packed_eq, etc.
   ```
3. Add `import "bpp_mymodule.bsm";` to `src/bpp.bpp`.
4. Add `init_mymodule();` to the init block in `main()`.
5. Wire the module's entry point into the pipeline.
6. Run the full bootstrap cycle.
7. Add a test in `tests/` if the module has behavior worth locking in.

### Available infrastructure

Before inventing a data structure, check whether one already exists:

| Need | Module | Functions |
|------|--------|-----------|
| Dynamic array | `bpp_array` | `arr_new`, `arr_push`, `arr_pop`, `arr_get`, `arr_set`, `arr_len`, `arr_clear`, `arr_free` |
| Hash map word→word | `bpp_hash` | `hash_new`, `hash_set`, `hash_get`, `hash_has`, `hash_del`, `hash_clear`, `hash_count`, `hash_free` |
| Hash map bytes→word | `bpp_hash` | `hash_str_new`, `hash_str_set`, `hash_str_get`, ... |
| Bump allocator (frame scope) | `bpp_arena` | `arena_new`, `arena_alloc`, `arena_reset`, `arena_free` |
| Fixed-size object pool | `stbpool` | `pool_new`, `pool_get`, `pool_put`, `pool_free` |
| Entity-component system | `stbecs` | `ecs_new`, `ecs_spawn`, `ecs_kill`, ... |
| Growable string builder | `bpp_str` | `sb_new`, `sb_cat`, `sb_int`, `sb_ch`, `sb_free` |
| Raw byte buffer | `bpp_buf` | `read_u8/u16/u32/u64`, `write_u8/u16/u32/u64` |
| File I/O | `bpp_file` | `file_read_all`, `file_write_all` |

The compiler itself uses several directly: `arr_*` for function/extern/global tables, `hash_str_*` for symbol-lookup hashes, `buf_eq` from `bpp_internal.bsm` for byte comparison.

### Symbol-table lookup strategies

When adding a new lookup site, pick one of three strategies depending on the access pattern:

**Eager rebuild.** Caller invokes `_rebuild_hash()` at the start of every phase that does lookups. Rebuild is O(n) once per phase. Lookups are pure O(1). Use when lookups are concentrated inside named phases with clear entry points. Failure mode: caller forgets to rebuild; lookups return wrong results silently. Example: `val_find_func` in `bpp_validate`.

**Lazy rebuild on stale.** The lookup function carries a `_hash_cnt` snapshot. Every lookup checks `if (_hash_cnt != arr_cnt) rebuild();` before querying. Use when the lookup function is the only entry point that matters and the array is mostly stable. Worst case O(n²) if inserts and lookups alternate. Example: `find_extern` in the C emitter (cold path).

**Incremental update at insertion sites.** Wherever the underlying array is pushed to, immediately call `register_in_hash(name, idx)`. Use when insertion sites are few and well-known. Failure mode: **silent and dangerous** — if you miss an insertion site, missing entries are invisible to lookups. Example: `find_struct` (parser and bo loader both call `register_struct_hash`).

When unsure, **eager is the safest**. It tolerates missed insertion sites because it rebuilds from the live state every phase. Lazy self-corrects. Only incremental can break silently.

---

## Part 5: File Order and Sweep Discipline

When tonifying existing code (or adding a rule that affects the whole codebase), process files in dependency order — leaves first, complex last. A broken leaf cascades; a broken root breaks in isolation and is easier to revert.

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
  src/bpp_mem.bsm

Batch 3 — Platform layers:
  src/backend/os/macos/_stb_core_macos.bsm
  src/backend/os/macos/_stb_platform_macos.bsm
  src/backend/os/macos/_stb_audio_macos.bsm
  src/backend/os/linux/_stb_core_linux.bsm
  src/backend/os/linux/_stb_platform_linux.bsm

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
  stb/stbaudio.bsm
  stb/stbmixer.bsm
  stb/stbsound.bsm
  stb/stbasset.bsm
  stb/stbscene.bsm

Batch 5 — Compiler internals:
  src/bpp_defs.bsm
  src/bpp_internal.bsm
  src/bpp_diag.bsm
  src/bpp_lexer.bsm
  src/bpp_parser.bsm
  src/bpp_types.bsm
  src/bpp_dispatch.bsm
  src/bpp_validate.bsm
  src/bpp_import.bsm
  src/bpp_bo.bsm
  src/bpp_bug.bsm

Batch 6 — Codegens:
  src/backend/chip/aarch64/a64_enc.bsm
  src/backend/chip/aarch64/a64_codegen.bsm
  src/backend/target/aarch64_macos/a64_macho.bsm
  src/backend/chip/x86_64/x64_enc.bsm
  src/backend/chip/x86_64/x64_codegen.bsm
  src/backend/target/x86_64_linux/x64_elf.bsm
  src/backend/c/bpp_emitter.bsm

Batch 7 — Entry points + games:
  src/bpp.bpp
  src/bug.bpp
  games/snake/snake_maestro.bpp
  games/snake/snake_particles.bsm
  games/snake/snake_gpu.bpp
  games/pathfind/pathfind.bpp
  games/platformer/platformer.bpp
  tools/mini_synth/mini_synth.bpp
```

### Recheck after each batch

Before bootstrapping, verify:

- Zero bare `auto` at file scope (should be `extrn`/`global`/`const`/`static auto`).
- Zero functions used only in their own file without `static` (grep every function, not just `_`-prefixed).
- Zero trailing `return 0;` in `void` functions.
- Zero side-effect functions missing `void`.
- Zero `import` in game files where `load` applies (batch 4 + 7 only).
- Zero override callbacks in engine modules without `stub` (batch 4 only).
- All relevant tests pass individually.
- Zero unexpected warnings during compilation.

### Bootstrap after each batch

```bash
./bpp src/bpp.bpp -o /tmp/bpp_verify
/tmp/bpp_verify src/bpp.bpp -o /tmp/bpp_verify2
shasum /tmp/bpp_verify /tmp/bpp_verify2   # must match
cp /tmp/bpp_verify2 ./bpp
./tests/run_all.sh                         # must be green
```

---

## Part 6: Testing

Every stb module or compiler pass that gets created or significantly changed should have a `tests/test_<module>.bpp` file. Tests are how you lock in behavior across compiler refactors.

### Test file layout

```bpp
// test_<module>.bpp — Smoke test for <module>.
// One-paragraph description of what the test exercises.

import "<module>.bsm";
import "bpp_io.bsm";

main() {
    auto h;

    // ── Section 1: empty state ────
    h = something_new();
    if (something_count(h) != 0) { print_msg("FAIL section1: empty"); return 1; }

    // ── Section 2: basic operations ────
    something_set(h, 42);
    if (something_get(h, 42) != ...) { print_msg("FAIL section2: basic"); return 4; }

    something_free(h);
    print_msg("test_<module>: all PASS");
    print_ln();
    return 0;
}
```

### Rules

- **Unique return code per assertion.** `return 1`, `return 2`, ... so a failed run traces to the exact assertion without printing noise.
- **Print only on failure.** No output on success. The final `"test_<module>: all PASS"` line is the only success output.
- **Cover the API surface.** Every public function exercised at least once. Edge cases (empty, full, resize, clear) get their own sections.
- **Run from `tests/`.** The test runner walks `tests/test_*.bpp` and compiles each. Failed compile and non-zero exit both count as failures.

Target: 100% pass on every commit. If a test breaks, fix the test (API drift) or fix the module (regression). Do not let stale tests accumulate — that is how the suite rotted from 71 tests to 20 broken ones before the 0.21 cleanup.

---

## Part 7: Cross-Compile and Deploy

### Native Linux from macOS

```bash
# Static ELF, no dependencies
bpp --linux64 games/snake/snake_gpu.bpp -o /tmp/snake_linux
```

If `DISPLAY` is set, the game opens an X11 window. Without it, falls back to ANSI terminal rendering.

### Linux via Docker + XQuartz (for testing on macOS)

One-time setup on macOS:

```bash
brew install --cask xquartz
defaults write org.xquartz.X11 nolisten_tcp 0
killall XQuartz 2>/dev/null
open -a XQuartz
xhost +localhost
```

Then cross-compile and run:

```bash
bpp --linux64 games/snake/snake_gpu.bpp -o /tmp/snake_linux
docker run --rm \
  --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 \
  -v /tmp:/tmp \
  ubuntu:22.04 \
  /tmp/snake_linux
```

The window appears on the macOS host via XQuartz. Works for any game using `draw_*` (CPU framebuffer). GPU games (`render_*`) need Vulkan, which is not yet implemented.

### C emitter (universal escape hatch)

The C emitter (`bpp --c`) translates B++ to portable C99 on stdout. It is the fallback for any platform B++ does not have a native backend for. Recommended pattern: use `drv_raylib.bsm` or `drv_sdl.bsm` instead of `stbgame.bsm` so the generated C calls regular C functions and avoids the Objective-C calling-convention pitfalls of `objc_msgSend`.

```bash
bpp --c examples/snake_raylib.bpp > /tmp/snake_raylib.c

# macOS
gcc -Wno-implicit-function-declaration -Wno-parentheses-equality \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    /tmp/snake_raylib.c -o /tmp/snake_raylib_c \
    -lraylib -lobjc

# Linux
gcc -Wno-implicit-function-declaration -Wno-parentheses-equality \
    /tmp/snake_raylib.c -o /tmp/snake_raylib_c \
    -lraylib -lm
```

The same `.c` compiles on any platform with raylib: macOS, Linux, Windows (MSYS2 / MSVC), BSDs, Emscripten.

**Why not use the C emitter with `stbgame.bsm` directly?** `stbgame.bsm` ultimately calls Cocoa via `objc_msgSend`, which on arm64-darwin requires an exact calling convention per call site. The C emitter dedupes overloaded externs by emitting a single varargs declaration (`void* objc_msgSend(void*, void*, ...)`), which works as a forward declaration but fails at runtime because the varargs ABI passes args on the stack, not in registers. For native Cocoa/Metal use the native backend; for portable C use a raylib or SDL driver.

---

## Part 8: Compiler Flags Reference

| Flag | Effect |
|------|--------|
| (default) | Native Mach-O ARM64 binary, modular pipeline |
| `--linux64` | Cross-compile to Linux x86_64 ELF |
| `--c` | Emit C code to stdout |
| `--asm` | Emit ARM64 assembly to stdout |
| `--bug` | Emit `.bug` debug map alongside the binary |
| `--monolithic` | Force single-pass pipeline (all modules at once) |
| `--show-deps` | Print module dependency graph and exit |
| `--show-promotions` | Print `auto → extrn/global` promotions |
| `--show-phases` | Print per-function phase classification |
| `--stats` | Print module/function/token counts to stderr |
| `--clean-cache` | No-op, kept for backward compat with scripts |
| `-o <name>` | Output filename |

---

## Part 9: Recovery

### Recovering from a broken `./bpp`

```bash
git show HEAD:bpp > ./bpp
chmod +x ./bpp
```

This restores the last committed working binary.

### Recovering from bootstrap divergence

If gen1 != gen2 after a source change, follow the decision tree:

1. Run gen3: `/tmp/bpp_gen2 src/bpp.bpp -o /tmp/bpp_gen3`.
2. If gen2 == gen3, it was a 1-cycle oscillation (normal after codegen changes). Install gen3.
3. If gen2 != gen3, the codegen output depends on the binary that compiled it — real bug. Diagnose:
   - `bug --dump-str /tmp/bpp_gen2` and `bug --dump-str /tmp/bpp_gen3` to compare data sections.
   - `./bpp --show-deps src/bpp.bpp` to see module graph.
   - `./bpp --stats src/bpp.bpp` to see symbol counts.
4. Revert the source change. Debug in isolation with a smaller test case.

Historical cause of 2-cycle oscillation: `static` on file-scope variables in auto-injected modules changed the global table between generations. The fix was to be consistent about storage class on auto-injected modules — sparse inconsistencies in batch 1 vs batch 2 produced compilations that differed between the old and new compiler.

### Recovering from a failing test suite

If `./tests/run_all.sh` shows failures that were not there before your change:

1. Check `shasum ./bpp /tmp/bpp_gen2` — are you still running the old binary? Copy gen2 into `./bpp`.
2. Run the failing test in isolation: `./bpp tests/test_<name>.bpp -o /tmp/t && /tmp/t`.
3. If it compiles but fails at runtime, use `bug`: `bug /tmp/t`.
4. If it fails to compile, check whether the test depends on a builtin or module that your change affected.

### The panic button

When nothing else works:

```bash
git stash
git show HEAD:bpp > ./bpp && chmod +x ./bpp
./tests/run_all.sh      # confirm the committed state is green
git stash pop           # bring your changes back
```

If the committed state is not green, something broke between commits — `git bisect` is the next move.

---

## References

- **Tonify source of truth**: `docs/tonify_checklist.md` — same rules as Part 2 above with deeper rationale and historical notes per rule.
- **Debugger**: `docs/debug_with_bug.md` — complete `bug` feature list with the Apr 16 Mach-O case study.
- **Diagnostics**: `docs/warning_error_log.md` — every W and E code the compiler emits.
- **Language spec**: `docs/bpp_language_reference.md` — grammar, type system, keyword table.
- **Roadmap**: `docs/todo.md` — what ships next and why.
- **Journal**: `docs/journal.md` — chronological record of design decisions.
