# B++ Production Manual

How to evolve the B++ compiler and the standard library safely. This
is the manual for people who modify B++ itself — the compiler source,
the stb modules, the backends, the test suite. For language usage,
read `bpp_manual.md` first.

The compiler is self-hosting: `bpp` compiles itself. Every change to
the compiler source must go through the bootstrap cycle described
here. Every change to a stb module that the compiler imports must do
the same.

## The Golden Rule

**Never overwrite `./bpp` with an untested binary.** The repo binary is the
only tool that can build the compiler. If it gets corrupted, you must recover
it from git (`git show HEAD:bpp > bpp && chmod +x bpp`).

## Quick Reference

```bash
# 1. Build gen1 from current source (using the repo bpp)
./bpp src/bpp.bpp -o /tmp/bpp_gen1

# 2. Use gen1 to compile itself → gen2
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2

# 3. Verify stability: gen1 and gen2 must be identical
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2)
# (no output = stable bootstrap)

# 4. Install
cp /tmp/bpp_gen2 ./bpp
```

## Why Two Generations?

gen1 is compiled by the OLD compiler. If your source changes affect how code
is generated (new opcodes, different register allocation, etc.), gen1 may
produce slightly different machine code than what the old compiler would. gen2
is compiled by gen1 (the NEW compiler), so it reflects the new codegen fully.

If gen1 == gen2 (byte-identical), the bootstrap is stable. If they differ,
the codegen change is real — compile gen3 to confirm convergence:

```bash
/tmp/bpp_gen2 src/bpp.bpp -o /tmp/bpp_gen3
diff <(xxd /tmp/bpp_gen2) <(xxd /tmp/bpp_gen3)
# gen2 == gen3 means stable (1-cycle oscillation was expected); install gen3.
```

**1-cycle oscillation is normal** when you change how the compiler emits
code for a feature it uses internally. The old compiler emits the old
pattern into gen1; gen1 (the new compiler) re-emits its own source with
the new pattern, producing gen2; gen2 and gen3 then match because both
were compiled by a new-pattern compiler. Expected after any codegen
refactor.

If gen2 != gen3 but gen1 == gen3, you have a **2-cycle oscillation** —
the codegen output depends on which compiler binary compiled it, not
just the source. That's a real bug. Known historical cause: `static`
on file-scope variables in auto-injected modules changed the global
table between generations. Diagnose with `--show-deps` and by comparing
the byte layout between generations.

**There is no module cache to flag as a suspect.** The `.bo` cache was
removed in 0.23.x — B++ compiles every module from source on every
invocation. Full rebuilds take ~0.27s for the compiler, too fast to
benefit from caching. Any bootstrap divergence is a real codegen issue,
not staleness.

## macOS Gotchas

### Exit 137 (SIGKILL) — Code Signing Cache

macOS caches code signatures per filesystem path. If you overwrite a binary
at the same path and run it, the kernel may kill it based on the OLD cached
signature. **This is not a compiler bug.**

Fix: always compile to a FRESH path (use `/tmp/bpp_genN`), never overwrite
and immediately run from the same path.

### Exit 138 (SIGBUS) — Usually Alignment

On ARM64, SIGBUS typically means misaligned memory access. Common causes in
B++:
- Unicode characters in source files (see below)
- `auto` declarations inside loop bodies (move to function top)

### Exit 139 (SIGSEGV) — Null Pointer

Often a raw pointer dereference on memory that was not allocated, or
accessing a struct pointer that was never initialized. Always guard
pointer access at system boundaries.

### Raw offset access on sliced structs returns garbage

**New class of bug.** If you write `*(node + 8)` expecting to read
`node.a`, you will get junk bytes. B++ slicing packs struct fields
without alignment padding — the declaration-order offset is wrong.

Always use typed access:
```bpp
auto n: Node;
n = node_ptr;
n.a           // correct: compiler emits the real packed offset
```

See `docs/tonify_checklist.md` Rule 8 for the full rule. Manual
records (RECORD_SZ, tokens) are unsliced by design and may still use
named offset constants — only sliced structs need typed access.

## Source File Rules

### Unicode in Comments is OK

The B++ compiler handles UTF-8 in comments correctly. Unicode in
string literals is NOT tested.

### Variables Inside Blocks

Variables can be declared inside `if`, `for`, or `while` blocks. The codegen
pre-scans all blocks recursively (`a64_pre_reg_vars`, `x64_pre_reg_vars`) and
allocates stack space for every `auto` declaration regardless of nesting depth.

```bpp
// Both styles work correctly:
my_func() {
    auto i;
    for (i = 0; i < 10; i = i + 1) {
        auto tmp;          // stack space is pre-allocated
        tmp = something();
    }
}
```

### Comments in English

All code comments must be in English, written like a user manual.

## Testing Changes

### Simple Smoke Test

```bash
echo 'main() { putchar(72); putchar(10); return 0; }' > /tmp/test.bpp
./bpp /tmp/test.bpp -o /tmp/test_out
/tmp/test_out
# Should print: H
```

### Testing Both Pipelines

The compiler has two pipelines. Both must work:

```bash
# Modular (default for native binaries)
./bpp /tmp/test.bpp -o /tmp/test_mod

# Monolithic (forced)
./bpp --monolithic /tmp/test.bpp -o /tmp/test_mono
```

### Never Do

- Do NOT run `codesign` on bpp — it changes bytes and previously
  corrupted the module cache; cache is gone now but codesign still
  produces non-deterministic binaries, breaking bootstrap reproducibility.
- Do NOT test with binaries in `build/` — use `/tmp/` or project root.
- Do NOT leave `bpp` processes running in the background.
- Do NOT create intermediate binaries in the repo (bpp_new, bpp_check, etc.).

## Step-by-Step: Adding a New Compiler Module

1. Create `src/bpp_mymodule.bsm`.
2. Add explicit imports at the top of your module for everything it uses:
   ```
   import "bpp_defs.bsm";
   import "bpp_array.bsm";
   import "bpp_internal.bsm";  // if you use buf_eq, packed_eq, etc.
   ```
3. Add `import "bpp_mymodule.bsm";` to `src/bpp.bpp`.
4. Add `init_mymodule();` to the init block in `main()`.
5. Wire the module's entry point into the pipeline.
6. Run the full bootstrap cycle (gen1 → gen2 → diff).
7. Add a test file in `tests/` if the module has behavior worth locking in.

## Compilation Model

B++ uses modular compilation: each imported module is parsed,
type-checked, and codegen'd independently, in topological order.
**There is no persistent cache** — every invocation compiles from
source. Full compiler rebuild is ~0.27s; the C emitter path
(`bpp --c`) is the only place where compilation ever takes meaningful
time (gcc, not B++).

Modular pipeline:
1. Resolve all imports and auto-inject core utilities (bpp_array,
   bpp_hash, bpp_io, ..., _stb_core, bpp_beat, bpp_job, bpp_maestro,
   brt0, bpp_mem).
2. Topologically sort modules so dependencies emit before dependents.
3. For each module: lex → parse → type-infer → emit machine code
   directly into the encoder buffer.
4. After all modules, run dispatch analysis, validation, and resolve
   cross-module calls via `bo_resolve_calls_arm64/x64`.
5. Binary writer (Mach-O or ELF) consumes the encoder buffer and the
   global/string/float tables to produce the final binary.

## Compiler Flags Reference

| Flag            | Effect                                      |
|-----------------|---------------------------------------------|
| (default)       | Native Mach-O ARM64 binary, modular pipeline |
| `--monolithic`  | Force single-pass pipeline (all modules at once) |
| `--asm`         | Emit ARM64 assembly to stdout               |
| `--c`           | Emit C code to stdout                       |
| `--linux64`     | Cross-compile to Linux x86_64 ELF           |
| `--bug`         | Emit `.bug` debug map alongside the binary   |
| `--show-deps`   | Print module dependency graph and exit       |
| `--clean-cache` | **No-op**, kept for backward compat with scripts. |
| `--stats`       | Print module/function/token counts to stderr |
| `-o <name>`     | Output filename                              |

## Cross-Compiling and Running Games

### Native macOS / Linux

```bash
# Native ARM64 Mach-O (the default)
bpp games/snake/snake_gpu.bpp -o snake
./snake

# Cross-compile to Linux x86_64 ELF (static binary)
bpp --linux64 examples/snake_cpu.bpp -o /tmp/snake_linux
```

The `--linux64` output is a static ELF binary. It runs on any Linux x86_64 system without external dependencies. For graphical output it uses the X11 wire protocol implemented in `stb/_stb_platform_linux.bsm` (Phases 1-3: window, software framebuffer via XPutImage, keyboard + mouse). If `DISPLAY` is unset it falls back to ANSI terminal rendering.

### Running Linux Builds in Docker (with XQuartz on macOS)

```bash
# One-time XQuartz setup on macOS
brew install --cask xquartz
defaults write org.xquartz.X11 nolisten_tcp 0
killall XQuartz 2>/dev/null
open -a XQuartz
xhost +localhost

# Cross-compile and run
bpp --linux64 examples/snake_cpu.bpp -o /tmp/snake_linux
docker run --rm \
  --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 \
  -v /tmp:/tmp \
  ubuntu:22.04 \
  /tmp/snake_linux
```

The window appears in XQuartz on the macOS host. This works for any game using `draw_*` (CPU framebuffer). GPU games (`render_*`) require Vulkan, which is not yet implemented (see `docs/todo.md` P0 / P4).

### C Emitter (portable C output via raylib or SDL)

The C emitter (`--c`) translates B++ to C99 source code on stdout. It is the universal escape hatch for any platform B++ does not have a native backend for. The recommended pattern is to write the game using `drv_raylib.bsm` (or `drv_sdl.bsm`) instead of `stbgame.bsm`, so the generated C calls regular C functions and avoids the Objective-C calling-convention pitfalls of dlopen + objc_msgSend.

```bash
# 1. Write the game using the raylib driver
#    (see examples/snake_raylib.bpp)
#    import "stbgame.bsm";
#    import "drv_raylib.bsm";
#    import "stbio.bsm";

# 2. Generate C
bpp --c examples/snake_raylib.bpp > /tmp/snake_raylib.c

# 3. Compile with gcc + raylib
#    macOS:
gcc -Wno-implicit-function-declaration \
    -Wno-parentheses-equality \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    /tmp/snake_raylib.c -o /tmp/snake_raylib_c \
    -lraylib -lobjc

#    Linux:
gcc -Wno-implicit-function-declaration \
    -Wno-parentheses-equality \
    /tmp/snake_raylib.c -o /tmp/snake_raylib_c \
    -lraylib -lm

# 4. Run
/tmp/snake_raylib_c
```

The same `.c` file should compile on any platform with raylib installed: macOS, Linux, Windows (MSYS2 / MSVC), BSDs, Emscripten.

**Why not use the C emitter with `stbgame.bsm` directly?** `stbgame.bsm` ultimately calls Cocoa via `objc_msgSend`, which on arm64-darwin requires an exact calling convention per call site. The C emitter dedupes overloaded externs by emitting a single varargs declaration (`void* objc_msgSend(void*, void*, ...)`), which works as a forward declaration but fails at runtime because varargs ABI passes args on the stack, not in registers. For native Cocoa/Metal use the native backend (`bpp game.bpp -o game`); for portable C output use a raylib or SDL driver.

## Writing stb Modules

The standard library (`stb/`) is the contract between B++ programs and
the engine. Every module follows the same shape so callers know what
to expect and the compiler can include them in any build without
surprises.

### Naming and layout

- File name: `stb<name>.bsm`. The `.bsm` extension marks "B-Single-Module".
- Function prefix: every public function starts with the module's name,
  e.g. `path_new`, `hash_set`, `tile_collides`. Private helpers start
  with an underscore: `_path_heap_swap`, `_hash_resize`.
- Header comment: a paragraph describing what the module does, the
  intended use case, and a code snippet showing the canonical pattern.
  Like Plan 9 / Bell Labs man pages, not marketing copy.
- Init function: `init_<name>()` that performs any one-time setup.
  Even if the body is `return 0;`, define it for symmetry — it gets
  called from `game_init` and from any program that uses the module
  standalone.
- Comments: English, full sentences, written like a user manual. The
  global rule is in `~/.claude/CLAUDE.md`.

### Leaf vs coupled

A leaf module has zero `import` statements pointing at other stb files.
It only depends on compiler builtins (`malloc`, `peek`, `poke`, `free`,
`memcpy`, etc.). Leaf modules are testable in isolation and importable
from any program — including the compiler itself, including unit tests.

Examples of leaf modules: `stbarena`, `stbarray`, `stbbuf`, `stbcol`,
`stbcolor`, `stbecs`, `stbfile`, `stbhash`, `stbinput`, `stbio`,
`stbmath`, `stbpath`, `stbpool`, `stbstr`.

Couple a module to another only when the **purpose** demands it — not
when convenience tempts you. `stbphys` imports `stbtile` because
"platformer physics for tilemaps" is its purpose: physics needs a
tilemap to collide against. `stbtile` imports `stbimage` because
loading a tileset PNG needs a decoder. Those couplings are honest.

`stbpath` could have imported `stbtile` for a `path_load_from_tilemap`
helper, but A* on a grid is **not** inherently a tilemap operation —
it works on puzzles, AI planners, network topology, anything with
cells. The bridge to a Tilemap lives in the calling game as a 5-line
loop, documented in `stbpath`'s header. The result: `stbpath` is a
leaf, the compiler test suite can import it without GPU init.

The rule: **acoplamento por propósito, não por conveniência**.

### Public structs

If the module exposes a struct, define it at the top of the file with
`struct Name { field1, field2, field3 }`. Document each field on its
own line with a comment. Callers create instances via `name_new(...)`
which returns a pointer. Field access uses `.` notation
(`obj.field`); the compiler emits the right load/store size based on
the field's type hint.

### Memory ownership

State the contract in the header comment:
- "Returns a pointer the caller must free with `name_free`."
- "Copies the input bytes — the caller's buffer can be reused immediately."
- "Returns a pointer into a per-call scratch buffer; do not free."

If you allocate with `malloc`, document who frees. If you take a
pointer the caller owns, say so explicitly. The convention across stb
is that anything ending in `_new` returns owned memory and pairs with
`_free`.

## Available Infrastructure

When writing a new module — whether a stb library or a compiler pass —
you do not have to invent data structures. Use these:

| Need | Module | Functions |
|------|--------|-----------|
| Dynamic array (push/pop) | `stbarray` | `arr_new`, `arr_push`, `arr_pop`, `arr_get`, `arr_set`, `arr_len`, `arr_clear`, `arr_free` |
| Hash map word→word | `stbhash` | `hash_new`, `hash_set`, `hash_get`, `hash_has`, `hash_del`, `hash_clear`, `hash_count`, `hash_free` |
| Hash map bytes→word | `stbhash` | `hash_str_new`, `hash_str_set`, `hash_str_get`, `hash_str_has`, `hash_str_del`, `hash_str_clear`, `hash_str_count`, `hash_str_free` |
| Bump allocator (frame scope) | `stbarena` | `arena_new`, `arena_alloc`, `arena_reset`, `arena_free` |
| Fixed-size object pool | `stbpool` | `pool_new`, `pool_get`, `pool_put`, `pool_free` |
| Entity-component system | `stbecs` | `ecs_new`, `ecs_spawn`, `ecs_kill`, `ecs_set_pos`, `ecs_get_x`, `ecs_physics`, ... |
| Growable string builder | `stbstr` | `sb_new`, `sb_cat`, `sb_int`, `sb_ch`, `sb_free` |
| Raw byte buffer read/write | `stbbuf` | `read_u8/u16/u32/u64`, `write_u8/u16/u32/u64` |
| File I/O | `stbfile` | `file_read_all`, `file_write_all` |

The compiler itself uses several of these directly: `arr_*` for the
function/extern/global tables, `hash_str_*` for the symbol-lookup
hashes, `buf_eq` (from `bpp_internal.bsm`) for byte comparison.

When in doubt, search for an existing primitive before writing a new
one. Adding a new generic data structure is justified only when no
existing one fits the access pattern.

## Symbol-Table Lookup Strategies

This is the design conversation that came up when refactoring six
linear-scan symbol lookups in the compiler to O(1) hash queries. It is
worth understanding before adding a new lookup site of your own.

A "symbol table" here is any array (`funcs[]`, `externs[]`,
`sd_names`, `ex_name`) where you periodically need to find an entry by
name or key. The naive `for (i = 0; i < cnt; i++) if (eq(...)) return i;`
is O(n) per lookup, and tends to dominate compile time when the table
is large.

Three strategies for keeping a hash in sync with the underlying array:

### Eager rebuild
Caller invokes `_rebuild_hash()` at the start of every phase that does
lookups (e.g. `run_validate`, `infer_module`). The rebuild is O(n)
once per phase. Lookups are pure O(1) — no per-call branch, no stale
checks.

**When to use:** the lookups are concentrated inside named phases
with clear entry points, and the underlying array is stable through
the phase.

**Failure mode:** caller forgets to call rebuild. Lookups return wrong
results silently.

**Example:** `val_find_func` (validate.bsm), `find_func_idx` (types.bsm).

### Lazy rebuild on stale
The lookup function carries a `_hash_cnt` snapshot of the underlying
array's count from the last rebuild. Every lookup checks
`if (_hash_cnt != arr_cnt) rebuild();` before querying.

**When to use:** the lookup function is the only entry point that
matters and the array is mostly stable. Best for cold paths where
implementation simplicity outweighs the per-call branch.

**Failure mode:** if inserts and lookups alternate (worst case), the
hash gets rebuilt on every lookup → O(n²) total. In practice this
worst case rarely happens because parsing and lookup are usually in
different phases.

**Example:** `find_extern`, `is_extern` (emitter.bsm — C emitter is
cold path).

### Incremental update at insertion sites
Wherever the underlying array is appended to, immediately call
`register_in_hash(name, idx)`. The hash is always perfectly in sync;
lookups never trigger a rebuild and have zero per-call overhead.

**When to use:** the array's insertion sites are few and well-known.
You audited every site that calls `arr_push(arr, ...)` and you are
confident there is no other path.

**Failure mode:** **silent and dangerous.** If you miss an insertion
site, the missing entries are invisible to lookups. The bug only
appears when something specific happens (like a cached `.bo` file
loading a struct via the cache loader's inlined path instead of the
parser's `add_struct_def`). The day's session caught exactly this bug
in `find_struct` after the second compile of the platformer failed
with `E101: unknown type 'Body'`.

**Example:** `find_struct` (parser.bsm + bo.bsm — both insertion
sites must call `register_struct_hash`).

### How to choose

```
                  ┌─────────────────────────────────────┐
                  │ Are insertion sites few and known?  │
                  └────────────────┬────────────────────┘
                                   │
                  ┌────────────────┴────────────────┐
                Yes                                 No
                  │                                 │
                  ▼                                 ▼
       ┌──────────────────┐         ┌──────────────────────────────┐
       │ Use INCREMENTAL  │         │ Are lookups in clear phases? │
       │ (audit every     │         └────────────────┬─────────────┘
       │  insert site)    │                          │
       └──────────────────┘            ┌─────────────┴─────────────┐
                                     Yes                           No
                                       │                            │
                                       ▼                            ▼
                            ┌────────────────────┐    ┌──────────────────────┐
                            │ Use EAGER          │    │ Use LAZY             │
                            │ (rebuild at        │    │ (stale check inside  │
                            │  phase entry)      │    │  the lookup itself)  │
                            └────────────────────┘    └──────────────────────┘
```

When unsure, **eager is the safest**. It tolerates missed insertion
sites because it rebuilds from the live state every phase. Lazy is
also safe (it self-corrects on stale). Only incremental can break
silently — and only if you miss an insertion site.

## Adding a New Builtin

A "builtin" is a function name the compiler recognizes specially and
emits machine code for, instead of treating it as a regular function
call. `malloc`, `peek`, `sys_write`, `assert` etc. are all builtins.

Adding one requires touching **four** places. Miss any of them and
either the validator rejects your builtin as undefined or one of the
backends fails to lower it.

1. **`src/backend/chip/aarch64/a64_codegen.bsm`** — emit ARM64 machine
   code for the builtin's call site. Find the existing handler block
   (search for `"sys_write"` to land in the right area) and add your
   case. Use the existing `a64_str_eq(n.a, "name", len)` pattern. For
   syscalls, import the BSYS_* constant from `_bsys_macos.bsm`.

2. **`src/backend/chip/x86_64/x64_codegen.bsm`** — emit x86_64 machine
   code. Same structure, same naming pattern. Syscall numbers come
   from `_bsys_linux.bsm`.

3. **`src/bpp_validate.bsm`** — add the builtin to `val_is_builtin()`
   so the validator does not reject it as `E201: undefined function`.
   Always add to all three at the same time — this is the file that
   historically bit us when syscalls were missing from the validator
   but present in the codegens.

4. **`src/backend/c/bpp_emitter.bsm`** — if the builtin should also
   work through the C emitter (`bpp --c`), add a handler in the call
   emission code. Map it to the equivalent libc/POSIX function or
   inline a C expression. Remember to add any new header to
   `emit_runtime()` (e.g. `<sys/mman.h>` for mmap). If the builtin is
   native-only, skip this step but document why in the validator
   entry comment.

After all four edits, run a full bootstrap cycle:
```bash
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
shasum /tmp/bpp_gen1 /tmp/bpp_gen2   # must match
cp /tmp/bpp_gen1 ./bpp
```

A simple smoke test program that calls the new builtin verifies
end-to-end correctness:
```bash
echo 'main() { my_new_builtin(42); return 0; }' > /tmp/test.bpp
./bpp /tmp/test.bpp -o /tmp/test_out && /tmp/test_out
```

## Where to Implement a Feature

Before writing code, ask: **"Would a new backend need to reimplement
this?"** If yes, the feature is in the wrong layer. Push decisions as
early in the pipeline as possible. The earlier a decision is made, the
fewer backends carry the weight.

```
Source → Lexer → Parser → AST → Dispatch/Validate → Codegen → Binary
         ↑                 ↑            ↑                ↑
     token rules     SEMANTICS     analysis        EMISSION
     (rarely new)    (go here)    (classification)  (go here only
                                                     if chip/OS
                                                     specific)
```

### Decision tree

1. **Does the feature define WHAT the program does?** (language
   semantics: short-circuit, operator lowering, control flow sugar)
   → **Parser / frontend.** One implementation, all backends inherit.
   Example: `a && b` → `if (a) then b else 0`. The backend never
   sees `&&`. A future RISC-V or WASM backend gets short-circuit
   for free.

2. **Does the feature need a chip-specific instruction?** (SIMD,
   bitfield extract, atomic ops)
   → **`src/backend/chip/<arch>/`.** One implementation per chip.
   Example: UBFX on ARM64 for bitfield loads, SHR+AND on x86_64.

3. **Does the feature need an OS syscall or API?** (memory allocation,
   I/O, time, threads)
   → **`src/backend/os/<os>/`.** One implementation per OS.
   Example: `_bmem_linux.bsm` uses sys_mmap, `_bmem_macos.bsm`
   uses libSystem.

4. **Does the feature need chip+OS specific binary format?** (section
   layout, relocations, entry point)
   → **`src/backend/target/<chip_os>/`.** One per target combination.
   Example: Mach-O writer knows ARM64 relocations AND macOS loader.

### The portability test

When in doubt, imagine adding a new backend (say, RISC-V Linux).
Count how many files you would need to touch to support the feature:

- If the answer is **zero** (the feature is in the parser/frontend):
  the implementation is in the right place.
- If the answer is **one** (a new chip or OS file): acceptable.
- If the answer is **two or more existing files**: the feature is
  too spread out. Centralize before adding the new backend.

### Anti-pattern: duplicated semantics across backends

If you find the same logical decision (not the same instruction, but
the same *rule*) implemented in `a64_codegen.bsm` AND `x64_codegen.bsm`
AND `bpp_emitter.bsm`, that is a sign the decision belongs in the
frontend. The backends should only differ in HOW they emit, not in
WHAT they decide to emit.

Historical example: `if (0) { ... }` dead code elimination was
duplicated in both native codegens. Short-circuit `&&`/`||` was
heading the same way until caught. Both belong in the parser.

---

## Backend Layout

The native backends live under `src/backend/` with a three-way split:

```
src/backend/
  chip/              ← pure CPU — instruction encoding + AST dispatch
    aarch64/         ← a64_enc.bsm, a64_codegen.bsm
    x86_64/          ← x64_enc.bsm, x64_codegen.bsm
  os/                ← pure OS — platform APIs, syscalls, runtime
    macos/           ← _stb_core_macos, _stb_platform_macos,
                       bug_observe_macos, _bsys_macos, _brt0_macos,
                       _bmem_macos
    linux/           ← mirror for Linux
  target/            ← chip + OS specific binary format writers
    aarch64_macos/   ← a64_macho.bsm (Mach-O writer)
    x86_64_linux/    ← x64_elf.bsm (ELF writer)
  c/                 ← alternative backend: C source emitter
    bpp_emitter.bsm
```

Each layer has a clear responsibility:

- **chip/** files know about the CPU but nothing about the OS. The
  ARM64 codegen imports `_bsys_macos.bsm` directly because in the
  current ship list ARM64 always pairs with macOS — that coupling is
  documented in the import comment so a future Linux-ARM64 split is
  obvious.
- **os/** files know about the OS but nothing about the CPU. Syscall
  numbers live in `_bsys_<os>.bsm` constants. Runtime startup globals
  (`_bpp_argc`, `_bpp_argv`, `_bpp_envp`) are declared in
  `_brt0_<os>.bsm`. The allocator (`_bmem_<os>.bsm`) implements
  malloc/free/realloc/memcpy over sys_mmap.
- **target/** files combine chip + OS into the final binary format
  (Mach-O on macOS, ELF on Linux). These know BOTH the chip's
  encoding rules and the OS's loader conventions.
- **c/** is the portable escape hatch — emits C source that compiles
  via gcc/clang on any platform with a C toolchain.

When Linux-ARM64 or Intel-macOS arrives, a new `os/<os>/` or
`target/<chip>_<os>/` folder drops in without disturbing existing
backends. See `docs/todo.md` for the timeline.

## Auto-Import System

The compiler injects two kinds of imports automatically. Programs do
not need to write these imports manually.

### Target-suffix fallback (existing since cross-compile landed)

`bpp_import.bsm` carries a global `_imp_target` string set to `"macos"`
by default and to `"linux"` when `--linux64` is passed. When
`find_file()` cannot resolve an import name, it tries again with
`_<target>` injected before the `.bsm` extension.

So `import "_stb_platform.bsm"` resolves to `_stb_platform_macos.bsm`
on macOS or `_stb_platform_linux.bsm` on Linux, automatically.

Future: when the compiler gains host-OS detection at startup,
`_imp_target` will default to the host's OS string instead of being
hardcoded to `"macos"`.

### Platform-layer auto-injection (added 2026-04-06)

`process_file()` in `bpp_import.bsm` detects when the file being
processed is `stbgame.bsm` and pulls `_stb_platform.bsm` into the
import graph automatically. The detection is a simple suffix match
via `_path_is_stbgame`.

The result: `stbgame.bsm` no longer carries an explicit
`import "_stb_platform.bsm";` line. Programs that import `stbgame`
get the platform layer transparently. Programs that do not import
stbgame (`hello.bpp`, simple test files) stay lightweight — no
platform code linked in.

To add another auto-injected pairing in the future (say, "any program
that imports `stbnet` should also pull `_stb_net_<target>.bsm`"), copy
the `_path_is_stbgame` pattern and trigger another `process_file` call
for the dependency.

## Tests Convention

Every stb module that gets created or significantly changed should
have a `tests/test_<module>.bpp` file. This is standard production
practice for B++ — the tests are how you verify that the module's API
holds up across compiler refactors.

### File layout

```bpp
// test_<module>.bpp — Smoke test for stb<module>.bsm.
// One-paragraph description of what the test exercises.

import "stb<module>.bsm";
import "stbio.bsm";

main() {
    auto h, i;

    // ── Section 1: empty state ────
    h = something_new();
    if (something_count(h) != 0) { print_msg("FAIL section1: empty"); return 1; }
    ...

    // ── Section 2: basic operations ────
    something_set(h, 42);
    if (something_get(h, 42) != ...) { print_msg("FAIL section2: ..."); return 4; }
    ...

    something_free(h);
    print_msg("test_<module>: all PASS");
    print_ln();
    return 0;
}
```

### Rules

- **Return code per assertion is unique.** `return 1`, `return 2`, ...
  so a failed run can be traced to the exact assertion without printing
  anything until something breaks.
- **Print only on failure.** No noise on success. The final
  `"test_<module>: all PASS"` line is the only success output.
- **Cover the API surface.** Every public function should be exercised
  at least once. Edge cases (empty, full, resize, clear) get their own
  sections.
- **Run from `tests/`.** The test runner just walks `tests/test_*.bpp`
  and compiles each. Failed compile and non-zero exit both count as
  failures.

### Running the suite

```bash
for t in tests/test_*.bpp; do
    name=$(basename $t .bpp)
    ./bpp $t -o /tmp/$name 2>/dev/null || { echo "FAIL compile: $name"; continue; }
    /tmp/$name > /dev/null 2>&1 || echo "FAIL exit: $name"
    rm -f /tmp/$name
done
```

Target: 100% pass on every commit. If a test breaks, either fix the
test (API drift) or fix the module (regression). Do not let stale
tests accumulate — that is how the suite rotted from 71 tests to 20
broken ones before the 0.21 cleanup.

## Recovering from a Broken bpp

If `./bpp` is corrupted or produces broken binaries:

```bash
git show HEAD:bpp > ./bpp
chmod +x ./bpp
```

This restores the last committed working binary.
