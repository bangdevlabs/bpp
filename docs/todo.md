# B++ Roadmap

## Status: 2026-04-11 — version 0.22

B++ 0.22 continues. **Tonify sweep** (batches 1-5 complete, 6-7 pending): ~350 void, ~200 static, ~50 :base, ~50 while→for, Node struct sliced (29% smaller AST), raw ptr→dot syntax refactor. **New language features**: `switch` statement (value + condition dispatch, inherited from B, no fallthrough), `bare return;`, multi-error diagnostics, E230 guard. **Codegen fixes**: `:half` sign-extension (LDRSW/MOVSXD both backends), `_stb_core_linux.bsm` created (Linux cross-compile unblocked), `dsp_is_local` packed_eq fix. **Progressive enhancement architecture**: `job_parallel_for` falls back to serial when no workers — smart dispatch auto-rewrite never silently drops work. Suite at **62 passed / 0 failed**.

**Vision**: B++ makes everything that makes a game — the art, the sound, AND the game itself.

**Version 1.0 ships when the Adventure Puzzle demo runs end-to-end via bangscript, all five demo games are playable, the full dev loop works (multi-error, debugger, profiler, hot reload, metaprogramming), and a stranger can download and play without thinking about the language it was written in.**

See `docs/games_roadtrip.md` for the full path from 0.22 to 1.0.
See `docs/bangscript_plan.md` for the adventure game DSL specification.

---

## The Road to 1.0

Everything below is organized around one question: what has to ship to get to 1.0? The answer is: the maestro pattern, the five dev loop capabilities, the five games (each as a vertical slice, not a full clone), bangscript (the adventure game DSL), and the modules those games need.

Anything NOT on the path to 1.0 is in "Post-1.0" at the bottom. When in doubt, move it down.

### 1.0 path — in commit order

```
0.22  ─┬─► [Module discipline: load, private, dup-error, circular-error]  ← FOUNDATION
       │
       ├─► [Maestro batch 2]
       │
       ├─► [Transitive .bo content hash fix]
       │
       ├─► [extrn keyword + auto smart promotion]
       │
       ├─► [Dogfood sweep: auto → extrn/global]
       │
       ├─► [Smart dispatch codegen (Maestro Phase 2)]
       │
       ├─► [Dev Loop 1: Multi-error + warning log]
       ├─► [Dev Loop 2: Debugger breakpoints]
       ├─► [Dev Loop 3: Profiler minimal + sampling]
       │
       ├─► [stbaudio.bsm + Rhythm Teacher demo]
       │
       ├─► [Wolf3D Phase 1: 1 level CPU raycaster]
       ├─► [Wolf3D Phase 2: hybrid CPU+GPU]
       │
       ├─► [Dev Loop 4: Hot reload watch mode]
       │
       ├─► [RPG Dungeon: dialogue, inventory, menu, save]
       │
       ├─► [Dev Loop 5: Metaprogramming ($T + reflection)]
       │
       ├─► [RTS demo: 1 map, 2 unit types]
       │
       ├─► [bangscript: compiler extensions + runtime]
       │
       └─► [Adventure Puzzle demo]  ──────────────► 1.0
```

Each step is detailed below.

---

## P0 — Path to 1.0 (in commit order)

### 0. Language finalization — `static`, `void`, `load`, implicit return, module rules

**Foundation change.** Lands before everything else. Finalizes the B++
syntax and semantics so every line of code written from this point
forward uses the final language form. See `docs/module_discipline_report.md`
for full spec with implementation details and test cases.

Seven changes (~133 lines total, 3 new keywords):

1. **`load` keyword** (~30 lines): `load "file.bsm"` resolves ONLY in
   the entry file's directory. `import` resolves in stb/ and installed
   paths. Separates "project file" from "engine module" at the language
   level. Inspired by Jai's `#load` vs `#import`.

2. **`static` keyword** (~35 lines): `static foo() { }` makes a
   function private to its module. Cross-module access is E220. Applies
   to functions and top-level variables. Familiar from C.

3. **`void` keyword** (~20 lines): `void foo() { }` declares a
   function that returns nothing. `x = void_func()` is W017. Completes
   the function syntax alongside `stub`.

4. **Implicit return 0** (~15 lines): Functions without explicit return
   get `return 0` inserted by the compiler. Eliminates boilerplate
   `return 0;` in every game_init/update/draw.

5. **Cross-module duplicate = fatal error** (~5 lines): E221.
   Forces prefix discipline (`ray_init`, `enemy_init`).

6. **Circular import = fatal error** (~20 lines): E222.
   Forces clean dependency structure.

7. **`main()` in `.bsm` = error** (~8 lines): E223.
   Enforces `.bpp` vs `.bsm` contract.

### 1. Maestro Plan 0.22 Batch 2 — runtime modules go standalone

In progress. Phase 1 of the maestro plan landed in 0.22 batch 1 with the runtime modules (`bpp_beat`, `bpp_job`, `bpp_maestro`) coupled to `stbgame` because their implementations needed `_stb_get_time` and `_stb_thread_create` which live in the platform layer, and the platform layer is auto-injected via stbgame. **This coupling is wrong** — beat/job/maestro are universal runtime, useful for CLI tools, audio plugins, network servers, batch processors, anything that has a `main()` and wants time + workers + a tick loop.

Batch 2 refactors:

1. **Split `_stb_platform_<os>.bsm` into `_stb_core_<os>.bsm` + `_stb_platform_<os>.bsm`**:
   - `_stb_core_<os>.bsm` (NEW): time + threads + basic syscalls. Universal. Auto-injected for any user `main()`. ~80 lines per OS.
   - `_stb_platform_<os>.bsm` (existing, slimmer): window + input + GPU + draw. Game-specific. Auto-injected only when stbgame is in the import graph. Imports `_stb_core_<os>.bsm` internally.

2. **Refactor `bpp_beat` / `bpp_job` / `bpp_maestro`** to drop their `import "stbgame.bsm";` lines. Each becomes standalone: bpp_beat lazy-inits, bpp_job uses `_stb_thread_create` from core, bpp_maestro owns its own loop and dt. stbgame gains a `game_run()` wrapper that combines `game_init` + `maestro_run` + `game_end`. snake_maestro migrates to use it.

3. **Auto-injection rules**: the 7 core utilities + 3 runtime modules + `_stb_core_<target>` auto-inject for every program. `_stb_platform_<target>` continues to auto-inject only via stbgame trigger.

After batch 2, beat/job/maestro are true core runtime, and any B++ program with a `main()` can use them without importing or thinking about stbgame.

### 2. `.bo` cache transitive content hash (FOUNDATION FIX)

The module cache is the single biggest source of friction in B++ development right now and the bug we hit twice in 0.22 (once silent, once loud) is the same one: the cache hashes each `.bsm` file individually and tags its `.bo` with that hash, but the dependency edge stops at `import` declarations. When a module USES a global declared in `bpp_defs.bsm` (like `T_BREAK`), the cache for the consumer module isn't invalidated when defs.bsm changes — even though the consumer's compiled code depends on the global's binding.

**Symptoms we hit:**
- 0.21 cycle: phantom W013 check masked because the consumer's `.bo` was loaded with stale validator state.
- 0.22 cycle: adding `T_CONTINUE` to `bpp_defs.bsm` produced `error[E104]: unexpected token '.'` in unrelated `src/x86_64/x64_codegen.bsm` until `--clean-cache` was run.
- Every smart-dispatch debug session today involved manually clearing the cache between iterations because cached `.bo`s lied about the previous source state.

**The proper fix is a transitive content hash**: each `.bo` carries not just its own source hash but a hash of all its imported modules' .bo content hashes. Cache hit only when the entire transitive closure matches. ~30 lines in `bpp_bo.bsm`. After computing a module's own source hash, walk its import edges, look up each import's `.bo` content hash, combine: `transitive_hash = sha(source_hash || sorted(import_bo_hashes))`. Tag the `.bo` with the transitive hash. Any change to ANY transitive dependency invalidates this and every consumer.

**Why this MUST land before everything below it:**
- `extrn` keyword adds a new global storage class that codegen reads. Touching `bpp_defs.bsm` with the cache lying underneath is painful.
- The dogfood sweep edits dozens of files migrating `auto` → `extrn`/`global`. Without the fix, every edit requires manual `--clean-cache`.
- Smart dispatch codegen produces different output depending on call-graph state. Stale cache then becomes a *correctness* liability — Build A and Build B with the same source produce different binaries, and the user can't tell which is right.

**Risk**: bumping the cache layout always carries clean-cache risk. The fix ITSELF will require one final `--clean-cache` to land. After that, the workaround disappears for good. Test plan: bootstrap fixed-point, edit `bpp_defs.bsm`, recompile any module without `--clean-cache`, verify the edit takes effect.

### 3. `extrn` keyword + `auto` smart promotion

Third storage class beyond `auto` and `global`. `extrn` declares a top-level variable that is mutable BEFORE `job_init()` runs (or `maestro_run()` which calls it internally), then becomes read-only forever. Compiler statically enforces: writes to `extrn` after the freeze point are errors. Reads from `extrn` are unconditionally safe from any thread because the memory is literally immutable.

The use case is universal in games: load assets / config / level data once at init, process them in parallel during the run. With `extrn`, the smart dispatch pass treats reads from `extrn` as zero-side-effect, and loops that only read `extrn` can be auto-parallelized without any stride analysis.

```bpp
extrn assets;       // declared, write-once-after-init
extrn config;
global world;       // shared mutable, programmer cuida do stride
auto debug_count;   // legacy serial

main() {
    assets = load_asset_file("levels.bin");  // OK, before job_init
    config = parse_config("game.json");      // OK, before job_init
    job_init(4);                              // freeze point
    // assets = something;                    // ERROR after this line
    maestro_run();
}
```

**Auto smart promotion** (paired with extrn): `auto x;` at file scope runs through a compile-time analysis pass that promotes it to `const`/`extrn`/`global` automatically when the compiler can prove 100% which class fits. Fallback is legacy serialized `auto`. Escape hatch via existing `:` hint syntax: `auto x: serial;` forces serialized and disables promotion.

The analysis reuses `fn_phase[]` (already built by classify_all_functions) plus write-site tracking. Promotion to extrn uses a conservative rule (variable written ONLY inside `main()` before the first `maestro_run` / `job_init` call) — not full reachability analysis, which is a separate project. `--show-promotions` flag prints every decision for debuggability.

Estimated: 150-250 lines across lexer, parser, validate, dispatch. See the session notes on ordering and the three analysis caveats (reachability vs purity, stride analysis scope, dev tooling).

### 4. Dogfood sweep — `auto` → `extrn` / `global` migration

After `extrn` lands, sweep every `.bsm` and `.bpp` file in `src/` and `stb/` (and `games/`) and migrate top-level `auto` declarations to the right storage class:

- **`auto x;` → `extrn x;`** when `x` is set once at init and never written again (asset tables, lookup tables, font metrics, frequency tables, parsed config)
- **`auto x;` → `global x;`** when `x` is shared mutable state that workers legitimately read/write (entity arrays, particle pools, world state)
- **`auto x;` stays `auto`** when `x` is genuinely main-thread-only and conservative

B++'s own standard library demonstrates the correct usage patterns. Programmers reading `stbecs.bsm`, `stbphys.bsm`, `stbtile.bsm` see `extrn`/`global` declarations in real code and learn the storage class system by example. The migration also validates the design under real use — every ambiguous case is signal.

Estimated scope: ~50-100 globals across ~30 files. Mechanical sweep, one file at a time, with `git diff` review per file. snake_maestro is the canary — it should auto-dispatch the particle update loop after the migration without any other source change.

### 5. Maestro Plan Phase 2 — smart dispatch codegen

Already partially shipped (call graph + fixpoint classifier are DONE in 0.22). Remaining work:

- **`.bo` cache format extension**: serialize `fn_phase[]` per exported function AND `glob_class[]` per exported global. Same BO_VERSION bump. Depends on step 2 (transitive content hash) so cache invalidation is sound.

- **Codegen reads `w.dispatch`** + consults `glob_class[]` for the loop classifier. When a `for` loop has only induction-var-or-global-or-loop-local reads/writes AND only calls to `PHASE_BASE` functions AND the touched globals are `global` (not `auto` legacy), the codegen rewrites the loop body into a synthesized function and emits `job_parallel_for(N, fn_ptr(synthesized_fn))` instead of the serial loop.

- **Function synthesis from loop body**: extract loop body to a new function that takes the induction variable as its single argument. Restriction A (induction var + globals only): no closure capture needed. Loops that reference outer locals stay serial. Snake's particle update fits restriction A exactly.

- **Migrate `snake_maestro.bpp`** to declare its globals with `global`, remove the explicit `register_base` calls, verify the compiler auto-dispatches particle physics without intervention.

End state: B++ programs write natural code, the compiler analyzes purity, the codegen rewrites parallel-safe loops to use the worker pool transparently, and snake_maestro's `main()` shrinks to a single `game_run()` call.

### 6. Dev Loop Item 1: Multi-error + warning log

The compiler today exits on first `diag_fatal` via `sys_exit(1)`. Real code has multiple errors per compile; fix-one-recompile-fix-next is painful at scale. Also: fixing the diagnostic system first makes the other dev loop items much less painful to build (debugging the debugger, the profiler, etc.).

Work: convert every `diag_fatal` site to `diag_error` that accumulates into a list and continues parsing/typechecking with best-effort recovery. Display all errors at end. Cap at ~20 errors to avoid flood. ~200-300 lines across `bpp_diag.bsm`, `bpp_parser.bsm`, `bpp_validate.bsm`.

Ships before Wolf3D Phase 1 because Wolf3D dev loop is where it first matters.

### 7. Dev Loop Item 2: `bug` with breakpoints

Wolf3D has state-machine enemy AI and collision logic. Without step-through, a "why did the enemy walk through the wall at frame 523?" bug takes hours instead of minutes.

B++ already has `bug` v2 with debugserver/gdbserver backend. Adding breakpoints is an API on top: `bug foo --break main:42 --break update_enemy`, translating source:line to byte addresses via the `.bug` symbol map, setting breakpoints via the existing GDB remote protocol client. Variable watch comes for free once breakpoints work.

~400-600 lines in `src/bug.bpp` and `src/bug_gdb.bsm`. Ships before Wolf3D Phase 1 starts.

### 8. Dev Loop Item 3: Profiler (minimal + sampling)

Wolf3D CPU at 60 fps is the honest benchmark. If it drops to 30 fps, without a profiler the developer is Sherlock with printf.

Two tiers:

- **Minimal `bench`**: a builtin `bench("label") { ... }` that times a code block via a high-resolution timer and prints ms/cycles. ~30 lines. Manual but immediate.
- **Sampling profiler**: every N ms, interrupt the program via `setitimer(ITIMER_PROF)` and capture which function is currently running (via the `bug` stack walker). Aggregate at end: "600/1000 samples in `raycast_column`, 270 in `draw_wall_strip`". ~300 lines, reuses the `bug` stack walker.

Ships before Wolf3D Phase 1 runs. Minimal lands first, sampling lands as soon as bench is proven.

### 9. `stbaudio.bsm` + Rhythm Teacher demo

**Rhythm Teacher**: 320×180 window, notes scroll down 4 lanes (A/S/D/F), timing graded PERFECT/GOOD/OK/MISS, 3 songs, "teacher mode" shows next key one bar ahead. Demo scope: 3 songs, 1 difficulty, core loop complete. See `docs/games_roadtrip.md` for the full design.

Capabilities this game forces:
- WAV file loader (PCM 16-bit) → `stbaudio.bsm` `audio_load(path)`
- Mixer with N voice slots → `stbaudio.bsm` `audio_play(voice, sample)`
- Sample-accurate timing → `stbaudio.bsm` frame counter from device
- Streaming background music → looped voice slot
- Audio platform layer → `_stb_platform_macos.bsm` CoreAudio AudioQueue, `_stb_platform_linux.bsm` ALSA SND_PCM (Linux deferred until ELF dynlink)

stbaudio uses the maestro pattern: audio callback runs on an OS-owned thread, main thread produces samples via an SPSC ring buffer using `mem_barrier()`. The maestro infrastructure makes this the standard pattern, not special-case threading.

Estimate: 1-2 weeks. Small game, high impact. Audio is a foundational module — once it ships, every following game gets it for free.

### 10. Wolf3D Phase 1 — CPU raycaster, 1 level demo

**Goal**: prove B++ can carry a real CPU rendering workload. Pure CPU raycaster, every pixel computed and written by B++ code. If this runs at 60 fps on Apple Silicon, the language is proven for real game work.

Demo scope: 1 playable level, 1 enemy type, 1 weapon, core raycaster at 60 fps, playable start to finish. See `docs/games_roadtrip.md` for the architecture and milestone breakdown.

Assets: shareware Wolfenstein 3D Episode 1 files (freely distributable since 1992). `.WL1` loader in `stbimage.bsm` style.

Estimate: 4-6 weeks for the demo level. Depends on dev loop items 1-3 being in place (multi-error log, breakpoints, profiler).

### 11. Wolf3D Phase 2 — hybrid CPU+GPU

After Phase 1 ships and proves B++ can render in pure software, the engine upgrades to use the existing GPU pipeline. The CPU still does all the math (raycaster, projection, sort) but the GPU handles texture sampling and blending via `_stb_gpu_vertex` quads.

This is exactly how Doom 64 worked: software raycasting, hardware texture mapping. Proves that B++'s 2D GPU pipeline composes into a 3D-feeling renderer with no shader changes.

Estimate: 2-3 weeks. Mechanical translation of the CPU rendering loop into GPU quad emission. No new modules required.

### 12. Dev Loop Item 4: Hot reload watch mode

`bpp --watch game.bpp` detects source changes, recompiles to a fresh `/tmp` path, kills the old process, restarts. Not true live code injection (hard); just full restart automation. Cuts 80% of the iteration friction for a fraction of the cost.

Becomes critical when tuning RPG dialogue pacing, enemy speed, level geometry — where the dev needs to see the effect in under 5 seconds.

Ships before the RPG demo starts because RPG content iteration is where the pain is highest. ~200 lines shell + compiler flag + process management.

### 13. RPG Dungeon demo

**Demo scope**: 1 dungeon room, 1 NPC with dialogue, 1 enemy, 1 item, pause menu with save/load. See `docs/games_roadtrip.md` Game 4.

New modules it forces (reused by RTS and Adventure):
- `stbdialogue.bsm`: dialogue boxes, word wrap, choices (~200 lines)
- `stbinventory.bsm`: item slots, icons (~150 lines)
- `stbmenu.bsm`: nested menus, keyboard nav (~200 lines)
- `stbsave.bsm`: save/load state (~150 lines)

Estimate: 2-3 weeks. The gameplay is simple; the value is the four UI modules.

### 14. Dev Loop Item 5: Metaprogramming (`$T` + compile-time reflection)

The RTS is where boilerplate hurts most: N unit types × (save / load / update / render / debug) = NxM hand-written functions. Jai is the reference.

Two features:

- **`$T` generic parameters**: `swap($T, a, b) { auto tmp: T; tmp = *a; *a = *b; *b = tmp; }`. The compiler instantiates a copy of the AST per concrete type used. ~400 lines.
- **Compile-time struct reflection + code emission**: `const generate_serializers(T) { ... }` reads struct field metadata at compile time and emits `save_unit`, `save_building` etc. The seed exists in `const FUNC` already; the leap is exposing struct field metadata and `emit_function` that injects into the AST. ~1100-2000 lines. Biggest scope of the 5 dev loop items.

Lands during RTS development, not before. Also the foundation for bangscript's `scene { }` block rewriting.

### 15. RTS demo

**Demo scope**: 1 map, 2 unit types, 1 building, core combat + pathfinding + audio + UI + simple reactive AI opponent. Not a full RTS. See `docs/games_roadtrip.md` Game 3.

New modules:
- `stbanim.bsm`: sprite animation frames + direction tables (~200 lines)

Everything else reuses existing infrastructure (stbtile, stbpath, stbecs, stbsprite, stbui, stbaudio, stbimage, stbrender, stbdialogue, stbmenu, stbsave from RPG).

Estimate: 2-4 months for the demo vertical slice.

### 16. bangscript runtime (`.bang` format + engine)

The adventure game DSL. See `docs/bangscript_plan.md` for the full specification.

**Zero compiler extensions.** `.bang` files are data parsed at runtime,
not compiled by the B++ compiler. The engine is a pure B++ module.

Runtime modules (~980 lines):
- `stbbangs.bsm` (~700): `.bang` parser + coroutine scheduler + command dispatcher + flag machine
- `stbverb.bsm` (~200): SCUMM-style verb/interaction system
- `stbcursor.bsm` (~80): cursor state machine

No longer depends on metaprogramming — could land as early as after
the RPG Dungeon demo.

### 17. Adventure Puzzle demo → B++ 1.0

**Demo scope**: 1 scene, 3-5 hotspots, 1 multi-step puzzle, verb system, cutscene. See `docs/games_roadtrip.md` Game 5.

Stretch: episode zero (3-4 scenes, 20-30 min gameplay).

Estimate: 4-6 weeks for the prologue. When a player downloads it, solves the puzzle, watches the cutscene, and the demo is complete from intro to credits without ever thinking about the language it was written in, B++ ships as 1.0.

---

## P0.5 — Refinements (land opportunistically alongside the roadtrip)

Small, high-value improvements that make the compiler and runtime more
elegant. None is a side quest — each is under 50 lines and lands
whenever someone is already touching the relevant file.

### `--stats` compiler flag (~40 lines)

Print module count (cached vs stale), function/global/struct counts,
peak token count, and wall-clock time. Answers "where is build time
going?" before it becomes a problem. Lands in `bpp.bpp` main, reads
counters that already exist (`func_cnt`, `arr_len(diag_file_starts)`,
`mod_stale`).

### Arena-backed AST in the parser (~30-50 lines)

Replace per-node `malloc(NODE_SZ)` with `arena_alloc(mod_arena, NODE_SZ)`.
One arena per module, reset after codegen. Eliminates parser memory
leaks, improves cache locality for AST walks. `stbarena` already
exists — this is wiring, not invention.

### `mem_track` for games (~50 lines)

Lightweight allocation tracking: `mem_alloc(size, tag)` wraps malloc
and records (tag, size) in a static array. `mem_report()` prints
per-tag totals. Answers "where is memory going?" during game
development. Leaf module (`stbmem.bsm` or `bpp_mem.bsm`), zero
performance impact beyond one arr_push per alloc.

### Function dedup: Fix 2 + Fix 3

- **Fix 2 (~5 lines):** `main()` in `.bsm` is a fatal error. Enforces
  the `.bpp` vs `.bsm` contract at the compiler level.
- **Fix 3 (~30 lines):** `stub` keyword marks intentional override
  targets. The dedup treats stub → real as always silent. Lands before
  bangscript (metaprog rewriter will generate stubs).

Fix 1 (mod0_real_start) and Fix 1b (dedup refinement) are already done.

---

## P1 — Adjacent quality work (not blocking 1.0, but close)

These items are important but don't sit on the direct path to 1.0. They land opportunistically or in a cleanup pass after 1.0 ships.

### ELF Dynamic Linking

B++'s x86_64 backend produces only static ELF binaries. To use any shared library on Linux (raylib, libpng, libsqlite, **libvulkan**, **libasound for ALSA**), the ELF writer needs PLT/GOT, `DT_NEEDED`, `PT_INTERP`, dlopen/dlsym builtins. This unlocks:
- Vulkan GPU on Linux (the P4 Vulkan rollout below)
- ALSA dynamic linking for stbaudio Linux backend (Rhythm Teacher on Linux)
- Any FFI module on Linux (libpng, sqlite, curl)

Estimate: 2-3 days of focused work. Snake maestro / Rhythm Teacher on Linux both wait on this.

### Host-aware compiler + install

Today the compiler defaults to macOS arm64 and `install.sh` only sets up that target — `--linux64` is the cross-compile flag. Vision: the compiler detects the host OS and chip at startup and defaults to that target, so `bpp game.bpp` on Linux x86_64 produces a Linux ELF without any flag. `install.sh` mirrors this.

Cross-compilation stays available via explicit `--target`. Transforms B++ from "macOS tool that cross-compiles to Linux" into "native tool that adapts to the host."

### Maestro Plan Phase 1.5 — slice sweep across stb hot structs

Per `docs/maestro_plan.md`. Apply slice types (`: byte`, `: quarter`, `: half`) to the bookkeeping fields of stb hot structs (Body in stbphys, World in stbecs, Hash/HashStr in bpp_hash). Both for cache hygiene and for canonical worked examples programmers will read when learning to optimize their own structs. Revisits after smart dispatch ships because some slice opportunities depend on knowing which globals are worker-shared.

### Retina support

`contentsScale = 2.0` on CAMetalLayer. Render at 2x physical pixels. Trivial once there's a reason to care.

### FFI auto-marshalling

The compiler knows FFI param types but ignores them in codegen. Fix would eliminate all the `(long long)` casts at call sites and make FFI declarations feel first-class.

### Struct field sugar for compiler internals

`*(rec + FN_NAME)` → `rec.name` for compiler-internal structs. ~20 locations.

### Multi-return values

`return a, b;` and `x, y = foo();`. Eliminates the `float_ret`/`float_ret2` hack. Would be nice but not blocking.

### bootstrap.c regeneration

Needs regeneration with the current C emitter. Required for GitHub distribution. Low priority until someone wants to bootstrap on a platform B++ doesn't run on yet.

### Test all games via the C emitter (raylib path)

Only `snake_gpu.bpp` and `snake_raylib.bpp` have been verified end-to-end through `bpp --c → gcc → run`. The other games need the same smoke test.

---

## P2 — Game modules (pure B++, no platform deps)

| Module | Status | Notes |
|---|---|---|
| `stbtile.bsm` | DONE | Tilemap engine |
| `stbphys.bsm` | DONE | Physics with milli-units |
| `stbpath.bsm` | DONE | A* pathfinding |
| `bpp_hash.bsm` | DONE | Hash maps (used by compiler) |
| `stbaudio.bsm` | P0 | Built during Rhythm Teacher |
| `stbdialogue.bsm` | P0 | Built during RPG (dialogue boxes, choices) |
| `stbinventory.bsm` | P0 | Built during RPG (item slots) |
| `stbmenu.bsm` | P0 | Built during RPG (nested menus) |
| `stbsave.bsm` | P0 | Built during RPG (save/load state) |
| `stbanim.bsm` | P0 | Built during RTS (sprite animation) |
| `stbbangs.bsm` | P0 | Built during bangscript (.bang parser + scheduler + dispatcher) |
| `stbverb.bsm` | P0 | Built during bangscript (verb/interaction system) |
| `stbcursor.bsm` | P0 | Built during bangscript (cursor state machine) |
| `stbart.bsm` | post-1.0 | Pixel art primitives |
| `stbnet.bsm` | post-1.0 | TCP/UDP for multiplayer |

---

## P3 — Post-1.0 work (scaling from demo to real game)

Everything below is explicitly post-1.0. The 1.0 demos prove B++
delivers each game category. Scaling from "1 demo level" to "full
shipping game" requires the infrastructure below. Each item is a
project of 2-6 weeks that adds infrastructure without changing the
language.

### Parallel codegen per module

The compiler is single-threaded. The modular pipeline and .bo cache
handle most of the build time, but codegen for stale modules is
serial. When the codebase grows to 50K+ lines, parallel codegen using
the maestro job pool cuts wall-clock time proportionally to core count.
Requires threading the codegen (which reads/writes global state today).

### Asset streaming (`stbasset.bsm`)

Demos load all assets at init and fit in memory. A full game has
hundreds of textures, sprites, audio files, and maps. Needs:
load/unload on demand, atlas packing, asset manifest, background
loading thread. ~500-1000 lines.

### Memory budget system

The `mem_track` refinement (P0.5) gives visibility. A full budget
system adds: per-system allocation limits, warnings on budget
exceeded, memory defragmentation for long-running games. Overkill
for demos, essential for a 2-hour game session.

### True live code reload (no process restart)

The 1.0 hot reload is kill+restart — loses all game state. True live
reload preserves world state: serialize state, recompile changed
module, re-link, deserialize. Requires automatic state serialization
(builds on metaprogramming) and partial re-linking. Significant
complexity — basically a second linker.

### Multi-error recovery across modules

The 1.0 multi-error log accumulates ~20 errors within a single
compilation. In a 50K-line codebase with 200 modules, error recovery
needs to handle cross-module type errors gracefully (like Rust or
TypeScript). Requires a more sophisticated error recovery strategy in
the parser and type inference.

### Profiler GUI (timeline view)

The 1.0 profiler is text-based (bench + sampling). A full profiler
needs: Chrome-style timeline view, per-frame breakdown, memory
profiler, GPU profiler. Likely ships as a standalone `stbtool` that
reads a binary trace file. The ring-buffer design from P0.5 feeds
directly into this.

### Networking (`stbnet.bsm`)

TCP/UDP sockets, state serialization, prediction/rollback for
multiplayer. None of the 5 demos has multiplayer. A full RTS or
co-op adventure needs this. ~500-800 lines.

### Target architecture refactor (chip vs OS split)

Today the chip folders (`src/aarch64/`, `src/x86_64/`) actually hold a chip+OS bundle. When a second OS for an existing chip lands (Linux ARM, Intel macOS, Windows x86_64), split into `arch/` + `os/` + `targets/` directories. Trigger: first time we need a chip+OS combo that doesn't fit existing folders.

### Vulkan GPU on Linux

Depends on ELF dynamic linking (P1). Once dlopen works: libvulkan via dlopen, `VK_KHR_xlib_surface` to bridge with the existing X11 wire protocol, minimal pipeline. **Docker on macOS has no GPU passthrough** — real GPU testing requires actual Linux hardware.

### Tools & editors

Sprite editor, tilemap editor, level designer, particle editor. All thin shells over stb modules. Landmine: hot reload + metaprogramming make these much easier to build, which is why they're post-1.0.

### Audio tools (CLAP plugin support)

`--plugin` flag, `export` keyword, DSP primitives (biquad, delay, oscillator, envelope). Post-1.0 once the audio path is mature.

### Windows / WebAssembly

New codegen backends. Big work, post-1.0.

### Distribution (EmacsOS)

Custom Linux distro: Emacs (EXWM) as DE, B++ games in X11 windows. Vision project.

---

## Known bugs

### BUG-2: macOS code signing cache (SIGKILL 137) — FIXED in 0.22

Fixed in 0.22 by `sys_unlink` before `sys_open` in a64_macho.bsm + x64_elf.bsm. Kept here as documentation of the fix.

### BUG-4: bootstrap.c is ARM64-only

Needs regeneration with current C emitter. Low priority.

### String dedup across modules

Duplicate strings in modular compilation create duplicate entries in data segment. Harmless but wasteful.

### `call()` float args on x86_64

`call(fp, args...)` ignores float arguments on x86_64. Doesn't affect native function calls, only the indirect `call()` builtin.

---

## Done (0.22 cycle)

### Foundation cleanup — `src/` vs `stb/` ambiguity resolved
- **8 module renames** from `stb/stb*.bsm` to `src/bpp_*.bsm`. Affected modules:
  - `src/defs.bsm` → `src/bpp_defs.bsm`
  - `stb/stbarray.bsm` → `src/bpp_array.bsm`
  - `stb/stbhash.bsm` → `src/bpp_hash.bsm`
  - `stb/stbbuf.bsm` → `src/bpp_buf.bsm`
  - `stb/stbstr.bsm` → `src/bpp_str.bsm`
  - `stb/stbio.bsm` → `src/bpp_io.bsm`
  - `stb/stbmath.bsm` → `src/bpp_math.bsm`
  - `stb/stbfile.bsm` → `src/bpp_file.bsm`
- ~50 import sites updated across compiler, stb modules, tests, and games
- `install.sh` extended to ship the new `src/bpp_*.bsm` modules
- `stb/` count: 23 → 16 cartridges; `src/` count: 13 → 22 .bsm files

### Maestro Plan Phase 1 — first parallel B++ runtime (macOS-only)
- `src/bpp_beat.bsm` (NEW, ~120 lines): universal monotonic clock
- `src/bpp_job.bsm` (NEW, ~250 lines): N×SPSC worker pool, round-robin submit, no CAS
- `src/bpp_maestro.bsm` (NEW, ~180 lines): solo/base/render bandleader
- `mem_barrier()` builtin in both codegens (DMB ISH / MFENCE)
- pthread FFI in `_stb_platform_macos.bsm` via libSystem
- `games/snake/snake_maestro.bpp` (NEW, ~290 lines): side-by-side with snake_gpu
- `docs/maestro_plan.md` (NEW, ~1000 lines)

### `global` keyword
- Top-level globals can be declared `global x;` instead of `auto x;`. Both produce identical runtime layout — the difference is a static intent flag (`glob_shared` array) that smart dispatch consults.
- `auto` is the legacy conservative default; `global` is the explicit "shared with workers" marker.
- Inside functions, `global` is a parser error.

### `bpp_dispatch.bsm` wired into modular pipeline
Constraint 1 of `maestro_plan.md`: dispatch was previously skipped in the modular pipeline. Wired in at the same logical position as the monolithic path.

### `continue` keyword + smart-dispatch call graph + fixpoint classification
- `continue` statement added: T_CONTINUE node, keyword in lexer, parser handler, codegen in both backends (jump-to-step label), C emitter. For-loop step expression now lives in `node.e`.
- `call_graph_build()` in `bpp_dispatch.bsm`: walks each function, populates `fn_calls[i]`, `fn_address_taken[i]`, `fn_locally_impure[i]`.
- `classify_all_functions()` with double-buffered fixpoint. Lattice has height 1 so fixpoint is exact. No SCC needed.
- 2 silent compiler bugs found and fixed in the way: stale cache after defs.bsm edit (workaround in `feedback_cache_bug.md`, real fix is P0 item 2), and `a64_emit_node` vs `a64_emit_stmt` silent silence (documented in `feedback_emit_stmt_vs_node.md`).

### 4 structural compiler bug fixes
1. `pathbuf` clobber in `bpp_import.bsm:794-820` — latent since 0.21 modular pipeline
2. inode reuse in `a64_macho.bsm:1355` + `x64_elf.bsm:392` — `sys_unlink` before `sys_open` closes the SIGKILL 137 bug class on macOS
3. pthread after NSApplication in stbmaestro — workers spawn before `game_init`
4. `install.sh` ghost files — pre-clean step for stale `.bsm` files in install dirs

### Test infrastructure: `tests/run_all.sh`
130-line POSIX shell runner with platform-aware skip rules and 10-second wall-clock guard. Suite at **46 pass / 0 fail / 11 skip** out of 57. New tests: `test_barrier`, `test_beat`, `test_thread`, `test_job`, `test_maestro`, `test_global_kw`.

### Documentation
- `docs/maestro_plan.md` (NEW, ~1000 lines)
- `docs/games_roadtrip.md` reframed around demo-level scope + dev loop integration
- `docs/todo.md` reorganized around path-to-1.0 vs post-1.0

---

## Done (0.21 cycle)

### Compiler internals
- `sys_socket`/`sys_connect`/`sys_usleep` added to validator builtin list
- stbhash-backed symbol tables: 6 linear-scan lookups → O(1) hash queries
- `_stb_platform.bsm` auto-injection via stbgame in import graph
- Platform/observe files moved into chip folders
- `install.sh` updated to copy from new chip folders

### New stb modules
- `stbpath.bsm`: pure-B++ A* with binary min-heap + indexed decrease-key
- `stbhash.bsm`: hash maps (word + byte keys, used by compiler)

### Game refactors
- `pathfind.bpp` rewritten: milli-pixel positions, tilemap arena, A* cat AI

### Test suite cleanup
- 71 → 52 tests, all passing. 19 stale tests deleted.
- `tests/test_hash.bpp` (49 assertions), `tests/test_path.bpp` (7 scenarios)

### Earlier 0.21 work (X11 + validator + C emitter)
- `stbtile.bsm` + `stbphys.bsm`: tilemap + platformer physics
- Address-of `&` operator (compiler feature, 9 files)
- `stbimage` tRNS: palette-indexed PNG transparency
- Alpha discard: Metal fragment shader `discard_fragment()`
- Platformer GPU with Kenney assets
- X11 wire protocol Phases 1-3 in `_stb_platform_linux.bsm` (276→1160 lines)
- Validator integrated into incremental pipeline (4-month bug)
- `bpp_typeck.bsm` deleted (507-line orphan)
- C emitter modernized (8 missing builtins, extern dedup, libc symbol skip)
- Snake on Linux via X11 + Docker + XQuartz
- C emitter raylib path verified end-to-end

---

## Done (historical)

- Self-hosting compiler (2026-03-20)
- Native Mach-O ARM64 binary (2026-03-23)
- x86_64 Linux cross-compilation (2026-03-25)
- Type hints: byte/quarter/half/int/float (2026-03-26)
- Modular compilation with .bo caching (2026-03-26)
- Base×Slice type system: 5 bases × 4 slices (2026-03-27)
- Optimized encoding: bit 0 = float flag (2026-03-31)
- FLOAT_H/Q codegen ARM64 (2026-03-31)
- GPU rendering via Metal (2026-03-31)
- `buf[i]` array syntax + `for` loop + `else if` syntax sugar (2026-03-31)
- Cache: explicit imports, Go-style hash cascading, per-program mixing (2026-04-01)
- Bug debugger v2: debugserver backend, GDB remote protocol (2026-04-02)
- `const` keyword, ECS, arena, pool, collision (2026-04-02)
- GPU sprites: any size, UV fix, sprite_create/sprite_draw (2026-04-05)
- `stbsprite.bsm`: palette-indexed sprite loading from JSON (2026-04-05)
- Module cache fix: 3 critical bugs (2026-04-05)
- Pathfinder game: rat-and-cat chase, ECS particles, GPU sprites (2026-04-05)
