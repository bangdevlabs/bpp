# Plan: stbbeat + stbmaestro + Worker Pool + Stb Slice Sweep + Phase Annotations

## Context

B++ is a self-hosting language for games. The user is converging on an
architecture borrowed from the jazz/rock ensemble model (more honest than the
orchestra metaphor — in a real orchestra everyone follows the score):

- **Base** (the rhythm section): functions with no side effects that run in
  parallel on worker threads. They're the deterministic foundation that follows
  the chart, predictable, repeatable. Bass + drums + harmony of the program.
- **Solo** (the improvisers): mutable functions that run sequentially on the
  main thread. They react to player input, mutate state, decide branches in
  runtime — improvisational within the form.
- **Beat** (the clock counting time): shared monotonic clock that both phases
  read independently. Coordinates without locking.
- **Maestro** (the bandleader): cues base in, cues solo in, manages tempo.
  Doesn't play an instrument — coordinates.

The user-facing lexicon is exactly **base** and **solo**, NOT "pure" —
annotations are `: base` and `: solo`, the runtime API is
`maestro_register_base()` / `maestro_register_solo()`, and constants are
`PHASE_BASE` / `PHASE_SOLO` / `PHASE_AUTO`. The word "pure" only appears in
references to existing `dispatch.bsm` code (`is_pure_extern`, `pure_ext`) that
was named years ago for a different concept (FFI whitelist) and is left
untouched.

Audio plugins and games share the same model — B++ becomes the language for
real-time audiovisual systems.

This plan implements that vision in three phases. Phase 1 ships a working
maestro pattern with workers and manual classification. Phase 1.5 sweeps the
existing stb library to apply slice types in the hot structs (Body, World,
Hash/HashStr) — both for genuine cache wins where they exist and to give
programmers canonical worked examples of typed-under-the-hood B++. Phase 2
adds compile-time phase classification using existing `dispatch.bsm` logic,
with `:` syntax override for cases the compiler can't prove.

The user explicitly wants to reuse what B++ already has instead of adding new
layers:

- `:` syntax (already used for type hints) gets a new use: `func(...): base` /
  `func(...): solo`
- `dispatch.bsm` already has `has_side_effects()` and `dsp_is_local()` — extend
  per-function instead of inventing a new pass
- `add_pure_extern()` already exists for FFI override (the `pure_ext` name is
  historical, this is just the FFI whitelist)
- `const FUNC(args) { body }` continues unchanged for compile-time pure
  functions
- The slice operators (`: byte`, `: quarter`, `: half`, `: half float`,
  `: quarter float`) were planted years ago waiting for an infrastructure
  consumer — Phase 1.5 finally activates them, in the same spirit as D's "you
  can use the GC, or you can opt out and manage memory yourself". B++ is
  typeless on top, typed underneath, and typing is a tool the programmer can
  reach for when they want either optimization or clarity.

Snake is the test bed because snake has been the canary for every B++ feature
since the bootstrap. Continuing the tradition.

## Code review applied

This plan was reviewed by a peer agent already deep in the B++ codebase. Seven
concrete changes from that review are folded in below, plus one platform
constraint (J) caught during the user's final pass:

- **A** — pthread via FFI instead of raw `bsdthread_create`/`clone` syscall
  wrappers (drops ~150 lines of compiler builtin code)
- **B** — N independent SPSC queues (one per worker, round-robin submit)
  instead of a single SPMC queue, removing the need for CAS in Phase 1
- **C** — `tests/test_thread.bpp` runs BEFORE `stbjob.bsm` to validate the
  calling-convention bridge (B++ `long long(long long)` ↔ pthread
  `void*(void*)`) before betting any infrastructure on it
- **D** — `.bo` cache format extended to serialize/deserialize `fn_phase[]` so
  that cached functions don't silently lose their phase classification on
  cache hit
- **E** — `tests/run_all.sh` is commit 0, blocking, before any other commit
  lands
- **F** — `tests/test_slice_struct.bpp` runs before Phase 1.5 to validate
  sliced struct field load/store codegen (the first production use of this
  codegen path)
- **G** — drop the runtime `func_purity()` builtin entirely; Phase 2 stays
  manual at the `register_*` API level and the compiler only validates that
  annotations agree with analysis (W013)
- **J** — Phase 1 ships macOS-only. Linux pthread requires ELF dynamic linking
  (P0 in `docs/todo.md`, multi-day milestone) which is intentionally NOT
  bundled into this plan. `snake_maestro` inherits Linux support automatically
  when ELF dynlink ships independently. See Constraint 6 for the full story.

---

## Architecture summary

```
USER PROGRAM (.bpp)             ← writes natural code, no thread keywords
        │
        ▼
stbmaestro (the bandleader)     ← knows the beat, cues solo → base → render phases
        │
        ├─→ stbbeat (the clock) ← T=0 at init, monotonic, agnostic of unit
        ├─→ stbjob (worker pool) ← N workers, N independent SPSC queues, round-robin submit
        └─→ stbgame (window/io)
        │
        ▼
PLATFORM LAYER                  ← _stb_get_time() + pthread_create/pthread_join (via FFI)
```

User registers callbacks for init, solo, base, render, quit. Maestro calls
them in order each tick: solo runs first (input + state mutation on main
thread), then base runs (pure parallel work on workers), then render submits
to GPU/framebuffer. Base phase dispatches pure work to worker pool via
`stbjob`. Workers communicate with main thread via lock-free ring buffer
protected by `mem_barrier()`.

Phase order rationale: solo first because it reads input and updates state.
base second because it consumes the just-updated state to compute derived
values in parallel. render last because it presents the result.

---

## Phase 1: Minimum viable maestro with manual classification

**Goal**: ship a working `snake_maestro.bpp` that runs in parallel on worker
threads, side-by-side with the existing `snakefull_gpu.bpp`.

### Phase 1 platform caveat — macOS-first, Linux deferred

`snake_maestro.bpp` targets macOS as its primary platform for Phase 1. The
reason is structural, not bureaucratic: pthread on macOS works immediately
because `libSystem.B.dylib` is already linked dynamically by the Mach-O writer
(`LC_LOAD_DYLIB`), and `pthread_create` is just a function inside that
already-loaded dylib. The macOS path only needs FFI declarations and it works.

Linux is a different story. `pthread_create` lives in `libpthread.so.0` (or in
modern glibc, just `libc.so.6`), which is a shared object. B++'s
`src/x86_64/x64_elf.bsm` writes static ELF — no PLT, no GOT, no `DT_NEEDED`,
no `PT_INTERP` for the dynamic linker. A statically-linked ELF cannot call
into a `.so`. Until ELF dynamic linking ships (it's the P0 item from
`docs/todo.md` post-0.21, an independent multi-day project), Linux cannot do
FFI to anything that isn't a raw syscall, and therefore cannot do pthread.

Resolution for Phase 1:

- `snake_maestro.bpp` builds and runs on macOS only.
- `snakefull_gpu.bpp` (the existing Linux X11 game) continues to work on Linux
  unchanged, because it uses pure Linux syscalls (X11 wire protocol, no shared
  library calls). Phase 1 introduces zero regression for current Linux users.
- The Linux platform row in the file table below describes the work that will
  happen once ELF dynlink lands — but commit 3 only ships the macOS half.
- When ELF dynamic linking eventually ships as its own milestone (independent
  of this plan), Linux maestro support comes for free: `stbjob` and
  `stbmaestro` have zero platform-specific logic, so the moment FFI to
  `libpthread` becomes possible, `snake_maestro` just compiles for
  `--linux64` with no library changes.

This is the same posture B++ takes elsewhere: feature ships on the platform
where it can ship cleanly, the other platform inherits when its blocker is
removed. Whether the Linux dynlink work can even be tested via Docker is
itself an open question — that uncertainty is exactly why bundling it into
this plan would be a mistake.

### Files to create

**`tests/run_all.sh`** — ~30 lines. Shell loop that compiles + runs every
`tests/test_*.bpp` and reports pass/fail counts. Blocking precondition for
every commit that follows. Wired into `install.sh`.

**`stb/stbbeat.bsm`** — ~80 lines. Universal monotonic clock. Reads
`_stb_get_time()` once at `init_beat()`, exposes `beat_now_ms/us/ns`,
`beat_now_at_rate(hz)`, `beat_now_frames(fps)`, `beat_now_samples(rate)`,
`beat_now_in_bpm(bpm)`, `beat_reset()`. Zero deps. Leaf module.

**`stb/stbjob.bsm`** — ~150 lines. Worker pool, N independent SPSC queues,
one per worker, round-robin submit. `job_init(n_workers)` calls
`pthread_create` N times (via the FFI declarations added below).
`job_submit(fn_ptr, arg)` increments a round-robin counter and pushes onto
the chosen worker's queue. `job_wait_all()` polls all queues for drain.
`job_parallel_for(n, fn_ptr)` is the workhorse. No CAS needed — each queue
has exactly one producer (main) and one consumer (its dedicated worker).
`mem_barrier()` provides ordering between producer and consumer.
Implementation note (change H from the review): include a comment in the
source noting that `mem_barrier()` emits a full bidirectional fence (`DMB
ISH` on ARM64, `MFENCE` on x86_64), which is overkill compared to release-only
on the producer + acquire-only on the consumer, but the per-frame cost is
negligible and adding separate acquire/release builtins would be premature
complexity until a real audio hot loop proves it matters. Work-stealing is
explicit future work, NOT in Phase 1.

**`stb/stbmaestro.bsm`** — ~180 lines. Bandleader.
`maestro_configure(title, w, h, fps)`,
`maestro_register_init/solo/base/render/quit(fn_ptr)`, `maestro_run()`.
Internally calls `game_init`, `init_beat`, `job_init`, then runs the loop
alternating solo → base → render until `game_should_quit()`. Internal state
uses sub-word slices (see "Slice usage in infrastructure" section). Maestro
is the timing authority — owns dt computation and passes it to user
callbacks (see Constraint 3).

**`games/snake/snake_maestro.bpp`** — ~250 lines. Port of `snakefull_gpu.bpp`
reorganized into solo/base/render phases. Same gameplay, same sprites, same
sounds. Side-by-side with original.

### Files to modify (compiler builtins)

| File | Lines added | Purpose |
|---|---|---|
| `src/aarch64/a64_codegen.bsm` | 15 | Emit `mem_barrier()` (`DMB ISH`). No `sys_thread_*` builtins — threads come from pthread via FFI. |
| `src/x86_64/x64_codegen.bsm` | 15 | Emit `mem_barrier()` (`MFENCE`). No thread builtins. |
| `src/bpp_validate.bsm` | 4 | Add `mem_barrier` to `val_is_builtin()`. |
| `src/bpp_emitter.bsm` | 8 | C emitter equivalent: `mem_barrier` → `__sync_synchronize`. |
| `src/aarch64/_stb_platform_macos.bsm` | 12 | FFI declarations for `pthread_create`, `pthread_join`, `pthread_self`. macOS already links `libSystem.B.dylib` which provides pthread; no new link flag needed. **This is the only platform that actually ships in Phase 1.** |
| `src/x86_64/_stb_platform_linux.bsm` | 0 in Phase 1 | Deferred until ELF dynamic linking lands (P0 item from `docs/todo.md`, independent multi-day project). The same ~12 lines of pthread FFI declarations will be added at that time, plus the dynlink machinery itself (PLT/GOT, `DT_NEEDED`, `PT_INTERP`, relocations). Until then, Linux cannot FFI to `libpthread` because the ELF writer produces static binaries. See "Phase 1 platform caveat" above. |

**Total Phase 1**: ~770 lines (compiler ~42, library ~440, game ~250,
tests/script ~38). Significantly lighter than the original draft because
pthread FFI replaces ~150 lines of syscall wrapper code.

### Implementation order (Phase 1)

**Commit 0**: `tests/run_all.sh`. Pure shell script (~30 lines). For loop
over `tests/test_*.bpp`, compile each, run each, count pass/fail, exit
non-zero on any failure. Wired into `install.sh` so any future contributor
runs it. This is blocking for every commit that follows — every commit below
ends with "run `tests/run_all.sh`, must be 100% pass". No compiler change,
lowest possible risk, but it gates everything else. Without it the feedback
loop on miscompiles is "implement → bootstrap → run snake by hand → cross
fingers", and that's how regressions sneak in.

**Commit 1**: `stb/stbbeat.bsm` only. No compiler change. Smoke test with a
30-line `tests/test_beat.bpp` that prints `beat_now_ms()` in a loop. Run
`tests/run_all.sh`.

**Commit 2**: `mem_barrier()` builtin only (the ONLY compiler change in
Phase 1). Bootstrap-verify shasum match. Smoke test with
`tests/test_barrier.bpp` that does a simple SPSC ring buffer in a single
thread (validates the instruction emits but doesn't actually need threads
yet). Run `tests/run_all.sh`.

**Commit 3**: pthread FFI declarations in
`src/aarch64/_stb_platform_macos.bsm` only. **macOS-only commit** — Linux
does not get pthread FFI in Phase 1 (see "Phase 1 platform caveat" above;
blocked on ELF dynamic linking). No compiler builtin change, no codegen
change. Smoke test with `tests/test_thread.bpp` on macOS — this is the
calling-convention bridge test flagged in change C: create one thread, have
it write to a global, join, main reads global and asserts. If the global is
wrong after join, the B++ `long long(long long)` ↔ pthread `void*(void*)`
ABI assumption is broken and the fix is to declare the entry point via FFI
with the matching signature and let the linker resolve it. **Do NOT proceed
to commit 4 until `test_thread.bpp` passes on macOS.** On Linux,
`tests/test_thread.bpp` is skipped via a `#if mac` guard or equivalent (mark
it as a known-skip in `tests/run_all.sh`). Run `tests/run_all.sh`.

**Commit 4**: `stb/stbjob.bsm` with N independent SPSC queues, one per
worker, round-robin submit. No CAS. Pure library, no compiler change. Smoke
test with `tests/test_job.bpp` that submits 100 trivial jobs to a 4-worker
pool and verifies all completed. Stress test: submit 10000 jobs, join, verify
count. Run `tests/run_all.sh`.

**Commit 5**: `stb/stbmaestro.bsm`. Pure library. No compiler change. Smoke
test with `tests/test_maestro.bpp` that registers 3 callbacks (solo prints
`S`, base prints `B`, render prints `R`) and runs for 60 ticks. Run
`tests/run_all.sh`.

**Commit 6**: `games/snake/snake_maestro.bpp`. Port of existing snake.
Original `snakefull_gpu.bpp` stays untouched. Validation: side-by-side
compile + run, gameplay identical. Run `tests/run_all.sh`.

### Manual classification API for Phase 1

```bpp
// In snake_maestro.bpp
import "stbmaestro.bsm";

main() {
    maestro_configure(SCREEN_W, SCREEN_H, "Snake Maestro", 10);
    maestro_register_init(fn_ptr(init_world));
    maestro_register_solo(fn_ptr(snake_solo_phase));        // explicit: improvisational, mutable
    maestro_register_base(fn_ptr(particles_base_phase));    // explicit: rhythm section, parallel-safe
    maestro_register_render(fn_ptr(snake_render_phase));
    maestro_run();
    return 0;
}
```

User responsibility: functions registered as base MUST have no side effects
(no global writes, no impure FFI calls). No compiler verification in Phase 1
— that's what Phase 2 W013 will provide. Bug in user code in Phase 1 =
silent race condition. This is the same trade-off Unity Job System makes
before they added Burst safety checks.

---

## Phase 1.5: Slice sweep across the existing stb library

**Goal**: apply sub-word slice types in the existing stb hot structs as the
FIRST real-world consumer of the slice operators that were planted years ago.
Two reasons, both load-bearing:

1. **Cache hygiene where it actually pays off** — Body instances saved 23
   bytes each, World saves 12, Hash saves 16. None of these alone are
   dramatic (most stb structs are single instances per game, not arrays of
   hundreds), but they add up and they cost almost nothing to apply.
2. **Pedagogy** — every game that imports `stbphys` reads `Body`. Every game
   that uses ECS sees `World`. These structs become the canonical worked
   examples of how a B++ programmer reaches for typing when they want either
   optimization or clarity. The slice operators stop being "a feature in the
   manual nobody uses" and become "the way the standard library is written".

### Audit conclusion (read first to set expectations)

I audited every stb module looking for hot structs. The honest finding: most
stb structs are single instances per game (one `Tilemap` per level, one
`World` per game, a handful of `Hash`es), so slicing them does NOT reduce
cache pressure during hot loops the way slicing per-particle component
arrays would. The real array-of-hundreds case lives in game-specific structs
(particles, bullets, enemies), which are programmer-owned and out of scope
for stb sweep. The win in stb is hygiene + pedagogy, not raw throughput. We
do this anyway because the canonical examples matter — and because the new
infra (`stbjob`, `stbmaestro`) already lands sliced.

Modules with no real opportunity that we explicitly skip: `stbgame`,
`stbdraw`, `stbsprite`, `stbfont`, `stbimage`, `stbinput`, `stbmath`,
`stbpool`, `stbui` (all single instances or precision-critical), and
`stbpath` (touching A* indices is risky against the existing 65535
silent-failure rationale already documented in
`feedback_lookup_strategies.md` — leave it alone).

### Structs to refactor

**`stb/stbphys.bsm` Body** — pedagogically the strongest example because
every platformer-style game extends `Body`. Six fields slice naturally:

```bpp
struct Body {
    x, y,                       // word — sub-pixel position precision (must stay word)
    vx, vy,                     // word — velocity precision (must stay word)
    w: quarter, h: quarter,     // hitbox dimensions, 8-256 pixels typical
    on_ground: byte,            // boolean
    gravity: half,              // acceleration, 0-2000 typical
    jump_vel: half,             // impulse, -1000 to 0 typical
    move_spd: half              // horizontal speed, 0-500 typical
}
// Saves 23 bytes per Body instance (48 → 25 bytes useful, padded to alignment).
```

**`stb/stbecs.bsm` World** — slices the count/capacity bookkeeping. The real
ECS hot path (the `alive[]` byte array) is already sliced and has been since
the module was written, which is the proof that the design intent for slices
was always infra-first.

```bpp
struct World {
    capacity: quarter, count: quarter,  // entity counts, max 65535 (a generous game)
    alive,                              // pointer to existing byte array
    free_ids, free_count: quarter,
    pos_x, pos_y, vel_x, vel_y, flags   // pointers to component arrays (word)
}
```

**`stb/stbhash.bsm` Hash and HashStr** — capacities are always power-of-2,
well-bounded, completely safe to slice:

```bpp
struct Hash {
    cap: quarter, cnt: quarter,  // power-of-2 capacities, 16-65536 typical
    keys, vals, used             // pointers (word)
}
struct HashStr {
    cap: quarter, cnt: quarter,
    key_buf, key_off, vals, used
}
```

### Files to modify

| File | Lines changed | Purpose |
|---|---|---|
| `stb/stbphys.bsm` | ~12 (Body struct + sites that read/write the sliced fields) | Slice Body's small enum-like fields |
| `stb/stbecs.bsm` | ~6 (World struct + counter increments) | Slice capacity/count |
| `stb/stbhash.bsm` | ~10 (both struct headers + their resize logic) | Slice cap/cnt |

**Total Phase 1.5**: ~30 lines changed across 3 modules. No new files. No
compiler changes.

### Implementation order (Phase 1.5)

The order is safest first so we shake out slice-related codegen surprises
(if any exist) on the simplest case before touching the most-used struct.
Phase 1.5 starts with a focused regression test (change F from the code
review) because struct field slices are about to become a production codegen
path and we want to know NOW if there's a latent bug.

**Commit 6-pre**: `tests/test_slice_struct.bpp`. Pure test, no library/code
change. Validates that sliced struct fields load and store correctly across
all four widths (byte, quarter, half, word). Concrete test:

```bpp
struct T {
    a: byte,        // 0-255
    b: half,        // i32 range
    c: quarter,     // 0-65535
    d,              // word
}
main() {
    auto t: T;
    t = malloc(32);
    t.a = 200; t.b = 0 - 100000; t.c = 50000; t.d = 9999999;
    if (t.a != 200) { return 1; }
    if (t.b != 0 - 100000) { return 2; }
    if (t.c != 50000) { return 3; }
    if (t.d != 9999999) { return 4; }
    return 0;
}
```

If this passes, Phase 1.5 is safe to proceed. If this fails, STOP — there's
a latent bug in struct-field-store codegen that exists today, and that's a
discovery worth knowing about before any stb module starts using sliced
fields. The bug surfaces here cleanly with no library coupling. Run
`tests/run_all.sh`.

**Commit 6a**: `stb/stbhash.bsm` slice sweep. Recompile bpp + run all tests
+ recompile every game in `games/`. Hash is used by the compiler itself, so
this is also a self-host stress test for sliced struct fields. Bootstrap-
verify shasum of bpp. Run `tests/run_all.sh`.

**Commit 6b**: `stb/stbecs.bsm` slice sweep. Recompile + test + bootstrap-
verify. `snake_maestro` and any other ECS-using game must run identically.
Run `tests/run_all.sh`.

**Commit 6c**: `stb/stbphys.bsm` slice sweep. Recompile platformer +
pathfinder (any game using physics). Visual gameplay must be identical —
same jump height, same fall speed, same collisions. This is the highest-
touch commit because `Body` fields are read/written in many sites. Run
`tests/run_all.sh`.

### Verification (Phase 1.5)

After each of 6a / 6b / 6c:

1. `./bpp --clean-cache && ./bpp src/bpp.bpp -o /tmp/bpp_p15 && /tmp/bpp_p15 --clean-cache && /tmp/bpp_p15 src/bpp.bpp -o /tmp/bpp_p15b && shasum /tmp/bpp_p15 /tmp/bpp_p15b`
   — bootstrap fixed-point still holds.
2. All 52 + Phase-1 tests pass.
3. Recompile every game in `games/` and run each one for 30 seconds.
   Visual identity check.
4. Re-run the original `snakefull_gpu.bpp` AND the new `snake_maestro.bpp`
   side by side — both must match Phase 1 baseline.

### What this commits to (and what it does NOT)

Commits to: applying slices to 3 stb structs as worked examples of the
typed-under-the-hood pattern, validating that sliced struct fields work
correctly through the entire stb → game → bpp self-host loop, and giving
programmers canonical references they can copy when optimizing their own
hot structs.

Does NOT commit to: extending `stbecs` with a new sub-word component-array
API (still belongs in a separate plan), refactoring user game structs
(programmer's choice), or chasing marginal wins in
`stbtile`/`stbui`/`stbpool` where the structs aren't hot.

---

## Phase 2: Static phase classification with `:` annotation override

**Goal**: compiler analyzes every function for side effects and stores its
phase; programmer can annotate `: base` or `: solo` to document intent or
override; validator warns if annotation disagrees with analysis. No runtime
builtin — `maestro_register_base()` / `maestro_register_solo()` stay manual
at the call site exactly like Phase 1, the upgrade is purely "the compiler
now tells you when you got it wrong" (change G from the code review).

### Files to modify

**`src/bpp_dispatch.bsm`** — ~95 lines added. Extend with
`classify_function(func_idx)` that calls `has_side_effects()` over each
statement of the function body, ORs the results, then does transitive
closure across `T_CALL` to other functions. Stores in new `fn_phase[]` array
(parallel to `fn_ret_types[]`). Eager rebuild strategy — runs as a pass at
dispatch entry, walks `funcs[]` from live state. Reuses existing
`dsp_is_local()` and `is_pure_extern()` machinery — no new analysis logic
invented.

**`src/bpp_parser.bsm`** — ~25 lines added. Parse optional `:` between
params closing `)` and body `{`. Detect `base` / `solo` keywords. Store hint
in new `fn_phase_hint[func_idx]` array. Logic goes between lines 954-955 of
`parse_function()`.

**`src/defs.bsm`** — ~5 lines added. Add constants: `PHASE_AUTO=0`,
`PHASE_BASE=1`, `PHASE_SOLO=2`. (Three values, not four — anything that has
any side effect or transitively calls something with side effects is
conservatively `PHASE_SOLO`.) Reserve `base` and `solo` as lexer keywords to
avoid ambiguity with `: base` parsed as a type hint.

**`src/bpp_types.bsm`** — ~5 lines added. Initialize `fn_phase[]` and
`fn_phase_hint[]` parallel arrays alongside existing `fn_par_types[]`.

**`src/bpp_bo.bsm`** — ~20 lines added. Serialize `fn_phase[func_idx]` per
exported function during cache write; deserialize during `bo_read_export()`.
Without this, cached functions load with `PHASE_AUTO` and the validator
can't check them (Constraint 4).

**`src/bpp_validate.bsm`** — ~20 lines added. After dispatch runs, validate
that `fn_phase_hint` and `fn_phase` agree. If user annotated `: base` but
dispatch found side effects, emit warning W013.

**Total Phase 2**: ~170 lines added on top of Phase 1. (Was ~220 in the
original draft; dropping the runtime `func_purity()` builtin removed ~50
lines of codegen + maestro changes.)

### The annotation syntax (Phase 2)

```bpp
// Default: no annotation, dispatch infers
update_particle(idx, dt) {
    auto p: Particle;
    p = particles[idx];
    p.x = p.x + p.vx * dt;
    return 0;
}
// → dispatch sees no global writes, no impure calls, fn_phase = PHASE_BASE automatically

// Explicit annotation: declares intent that the compiler verifies
my_thread_safe_ffi_wrapper(arg): base {
    return call(some_known_safe_extern, arg);
}
// → fn_phase_hint = PHASE_BASE, dispatch analyzes, warns W013 if it disagrees

// Explicit override: force solo even if the function happens to be pure
critical_section(state): solo {
    // This function looks pure but the user wants it sequential for ordering reasons
    return process(state);
}
// → fn_phase_hint = PHASE_SOLO, no warning regardless of analysis (override is always allowed)

// Annotation alongside type hints on parameters works:
mix_audio(a: byte, b: byte): base { return a + b; }
// → param type hints go inside the parens, : base goes after the closing paren
```

The runtime API does NOT change between Phase 1 and Phase 2.
`maestro_register_base(fn_ptr(foo))` is still how the user wires a function
into the base phase. The win in Phase 2 is that if `foo` is annotated
`: base` AND has a hidden side effect, the compiler warns at compile time
instead of letting it become a race at runtime. Programmer is still
responsible for matching `register_*` calls to the right phase.

### Implementation order (Phase 2)

After Phase 1 (commits 0-6) and Phase 1.5 (commits 6-pre, 6a-6c) are landed
and verified:

**Commit 7-pre**: Two preconditions in one commit (small enough to bundle).
- (a) Wire `bpp_dispatch.bsm` into the modular pipeline as a real pass
  between typecheck and codegen (Constraint 1). Verify by re-running every
  game in `games/` and confirming dispatch's existing loop-level
  `has_side_effects()` analysis fires (add a debug print temporarily, then
  remove).
- (b) Extend the `.bo` cache format to serialize/deserialize a per-function
  phase tag, even though the field is unused so far — a default `PHASE_AUTO`
  slot is enough. This wires up the on-disk format in advance so commit 8
  doesn't have to also bump the `.bo` version (Constraint 4 / change D).

Bootstrap-verify shasum match. Run `tests/run_all.sh`.

**Commit 7**: `:` syntax parsing in `bpp_parser.bsm`. Reserve `base` and
`solo` as lexer keywords. Bootstrap-verify. Smoke test
(`tests/test_phase_annot.bpp`) with the three parser collision cases from
change I:

```bpp
func1(a: byte, b: byte): base { return a + b; }   // type hints + phase
func2(): solo { return 0; }                        // no params + phase
func3() { auto x: byte; x = 0; return x; }         // : byte locals, no phase
```

Verify `fn_phase_hint[]` is populated correctly for `func1` and `func2` and
stays `PHASE_AUTO` for `func3`. Run `tests/run_all.sh`.

**Commit 8**: `classify_function()` extension in `bpp_dispatch.bsm`, using
eager rebuild strategy (Constraint 2 — same model as `find_func_idx`). Now
that the `.bo` format already has the slot (commit 7-pre), commit 8 can
populate it during codegen. Smoke test that a known-pure function gets
`PHASE_BASE` and a known-mutable function gets `PHASE_SOLO`. Critical
regression test: compile a game that mixes a parsed module and a cached
`.bo` module, then verify functions from BOTH sources got classified
correctly (this is the test that would have caught the `find_struct` bug if
it had existed for phase classification). Bootstrap-verify. Run
`tests/run_all.sh`.

**Commit 9**: Validation pass in `bpp_validate.bsm` — emit W013 when
`fn_phase_hint != PHASE_AUTO` and `fn_phase_hint != fn_phase`. Test: a
function with `: base` annotation but a global write triggers W013. A
function with `: solo` annotation never triggers W013 (override is always
allowed). Bootstrap-verify. Run `tests/run_all.sh`.

(The original commit 9 — runtime `func_purity()` builtin and auto-dispatch
via maestro — is dropped. Change G: keeping `register_base`/`register_solo`
manual is genuinely fine for Phase 2 because the API doesn't change between
Phase 1 and Phase 2 and there's no runtime address → func_idx lookup table
to maintain. If a future game needs auto-dispatch, that's a follow-up plan.)

---

## Critical files reference

### Existing code being reused (no modification needed)

- `src/bpp_dispatch.bsm:182-267` — `has_side_effects()` is the seed for
  per-function purity
- `src/bpp_dispatch.bsm:38-50` — `add_pure_extern()` / `is_pure_extern()`
  for FFI override (already exposed)
- `src/bpp_parser.bsm:849-892` — `parse_const()` and const function
  machinery (orthogonal, kept as-is)
- `src/aarch64/_stb_platform_macos.bsm:247-254` — `_stb_get_time()` returns
  ms since init
- `src/x86_64/_stb_platform_linux.bsm:117-125` — same on Linux
- `src/bpp_import.bsm:666-772` — auto-injection pattern (mirror for
  `stbmaestro` if desired)
- `stb/stbgame.bsm` — `game_init`/`frame_begin`/`should_quit`/`dt` that
  maestro wraps

### Existing code being extended

- `src/bpp_dispatch.bsm` — add `classify_function()`, `fn_phase[]` storage,
  integrate into modular pipeline
- `src/bpp_parser.bsm:928-981` — `parse_function()` gains `:` annotation
  parsing for `base`/`solo` keywords
- `src/bpp_bo.bsm` — extend cache format to serialize `fn_phase[func_idx]`
- `src/aarch64/a64_codegen.bsm` + `src/x86_64/x64_codegen.bsm` — single new
  builtin (`mem_barrier`)
- `src/aarch64/_stb_platform_macos.bsm` +
  `src/x86_64/_stb_platform_linux.bsm` — pthread FFI declarations
- `src/bpp_validate.bsm` — `mem_barrier` builtin recognition + W013
  phase-mismatch check
- `src/bpp_emitter.bsm` — `mem_barrier` C emitter equivalent

---

## Verification

### Phase 1 verification (after commit 6)

**On macOS** (the primary target for Phase 1):

1. `./bpp --clean-cache` then `./bpp src/bpp.bpp -o /tmp/bpp_p1`. Use
   `bpp_p1` to compile itself again. shasum must match (fixed-point
   bootstrap).
2. `cd games/snake && ../../bpp snakefull_gpu.bpp -o snake_orig` — original
   still compiles and runs.
3. `cd games/snake && ../../bpp snake_maestro.bpp -o snake_new` — new
   version compiles.
4. `./snake_orig` and `./snake_new` side by side. Same gameplay, same speed,
   same particles, same scoring. If they diverge, the bug is in `stbmaestro`
   or the port.
5. `tests/run_all.sh` is 100% pass. Target: 52 existing tests + new test
   files (`test_beat`, `test_barrier`, `test_thread`, `test_job`,
   `test_maestro`) all green.
6. CPU usage check: `top -pid $(pgrep snake_new)` should show >100% CPU when
   base phase has work (multiple cores active). For snake the base workload
   is small so this may not be visible — that's OK, the validation is
   "doesn't crash, gameplay matches".

**On Linux** (regression-only, no new feature expected):

1. Bootstrap fixed-point still holds (`mem_barrier` builtin and the
   unrelated commits don't break the Linux build).
2. `cd games/snake && ../../bpp snakefull_gpu.bpp -o snake_orig` — original
   still compiles and runs unchanged. Phase 1 must not regress existing
   Linux games.
3. `tests/run_all.sh` passes with `test_thread`, `test_job`, `test_maestro`
   marked as known-skip on Linux (Constraint 6). Every other test passes
   normally.
4. `snake_maestro.bpp` is expected to fail to compile on Linux with a clear
   "platform layer does not provide pthread on this target" error. This is
   the documented behavior for Phase 1, not a bug.

### Phase 1.5 verification (after commit 6c)

1. `tests/test_slice_struct.bpp` passed before any stb module was touched
   (commit 6-pre).
2. Bootstrap fixed-point holds after each of 6a/6b/6c.
3. All 52 + Phase-1 tests + new test files still pass via
   `tests/run_all.sh`.
4. Every game in `games/` recompiles and runs for 30 seconds. Visual
   identity check vs the pre-Phase-1.5 baseline.
5. `snakefull_gpu.bpp` AND `snake_maestro.bpp` both still match Phase 1
   baseline (slicing the library shouldn't change game-visible behavior).

### Phase 2 verification (after commit 9)

1. Bootstrap-verify after each compiler change (commits 7-pre, 7, 8, 9).
2. `tests/test_phase_annot.bpp` parses all three collision cases correctly
   (change I).
3. Cached + parsed module mix: compile a game whose dependency is in `.bo`
   cache, verify functions from the cached module got their `fn_phase`
   correctly populated from the `.bo` file (Constraint 4 / commit 7-pre
   regression).
4. Test annotations: write a function with `: base` that has a global write
   — validator should emit W013 warning.
5. Test override: write a function that has no side effects but is
   annotated `: solo` — validator should NOT warn (override is always
   allowed).
6. The runtime API (`maestro_register_base`/`maestro_register_solo`) is
   unchanged from Phase 1 — `snake_maestro.bpp` must compile and run without
   modification.

### Bootstrap discipline (every compiler commit)

```bash
./bpp --clean-cache
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 --clean-cache
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
shasum /tmp/bpp_gen1 /tmp/bpp_gen2     # MUST match
cp /tmp/bpp_gen1 ./bpp
```

---

## Risk assessment

**Low risk:**

- `tests/run_all.sh` (commit 0) — pure shell script, no compiler interaction
- `stbbeat.bsm` (commit 1) — leaf module, no compiler change
- pthread FFI declarations (commit 3) — uses existing FFI mechanism, no
  codegen change. Risk is calling-convention bridge (Constraint 5),
  mitigated by `test_thread.bpp` running before commit 4.
- `stbmaestro.bsm` (commit 5) — pure library
- `snake_maestro.bpp` (commit 6) — original snake unchanged, side-by-side
- Phase 1.5 commit 6-pre — pure test file, shake-out for sliced struct
  field codegen

**Medium risk:**

- `mem_barrier()` builtin (commit 2) — single instruction emit, but
  bootstrap must verify exactly. Smallest possible compiler change.
- N×SPSC queue in `stbjob` (commit 4) — concurrent data structure, but the
  N×SPSC choice (one producer, one consumer per queue) is the simplest
  correct lock-free design. No CAS, no shared mutable indices across
  workers. Mitigation: stress test with 10000 jobs / 4 workers in
  `test_job.bpp`.
- `.bo` cache format extension (commit 7-pre b) — bumping cache layout
  always carries clean-cache risk. Mitigation: the field starts as a
  default `PHASE_AUTO` placeholder (commit 7-pre) so a stale cache loaded
  by a Phase-2 binary still parses correctly.

**Higher risk:**

- `classify_function()` transitive closure (commit 8) — fixed-point
  algorithm over the call graph. Risk of infinite loop on recursive
  functions. Mitigation: cap iterations, mark recursive cycles as
  `PHASE_SOLO` (conservative).
- `:` syntax parser change (commit 7) — small change but in a hot path of
  the parser. Reserve `base` and `solo` as keywords up front to avoid
  ambiguity with `: base` parsed as a type hint. Bootstrap must verify
  carefully and the three parser collision cases from change I must all
  parse correctly.
- Phase 1.5 `stbphys.bsm` Body slice (commit 6c) — `Body` is read/written
  from many sites, and the compiler's struct-field codegen has to honor
  sliced loads/stores correctly. If there's any latent bug in slice
  load/store emission, this commit is where it surfaces in production.
  Mitigation: 6-pre (`test_slice_struct.bpp`) shakes out the codegen path
  before any stb module is touched. 6a (`stbhash`) and 6b (`stbecs`) run
  before 6c as smaller-blast-radius shake-out. Each game must be visually
  validated, not just compiled.
- Phase 1.5 self-host risk (commit 6a) — `Hash` is used by the compiler
  itself (the O(1) symbol-table refactor from 0.21). Slicing cap/cnt means
  the compiler is now reading sliced fields during its own bootstrap. If
  slice loads emit wrong code, the compiler self-hosts incorrectly and may
  produce a binary that miscompiles itself. Mitigation: bootstrap-verify
  with shasum; if the second-gen shasum diverges from first-gen, revert
  immediately. `test_slice_struct.bpp` from 6-pre should already catch this
  before 6a even starts.

**Out of scope for this plan:**

- Audio plugins (`stbaudio.bsm`, VST3 export) — separate future plan
- GPU compute via Vulkan — separate future plan
- Extending `stbecs` to expose sub-word slice components in user-facing
  component arrays — separate ECS optimization plan. Phase 1.5 only touches
  the `World` struct's bookkeeping fields, not the component array storage
  layout.
- Refactoring user game structs to use slices — programmer's choice. The
  Phase 1.5 stb examples become the reference.
- ELF dynamic linking on Linux — this is a P0 item in `docs/todo.md` but it
  is its own multi-day milestone, not part of this plan. Constraint 6
  explains the consequence: `snake_maestro` is macOS-only in Phase 1, and
  Linux inherits maestro support automatically when ELF dynlink ships
  independently.

**Known limitations (not risks, just facts):**

- `snake_maestro.bpp` does not run on Linux until ELF dynamic linking
  lands. `snakefull_gpu.bpp` continues to run on Linux unchanged (zero
  regression for existing Linux users).
- Phase 1.5 and Phase 2 are platform-agnostic (slice sweep is a library
  refactor, phase classification is a compiler analysis), so those phases
  reach Linux normally even though Phase 1's `snake_maestro` doesn't.

---

## Constraints surfaced during research

These are not scope additions — they are facts about the existing codebase
that the plan has to respect or it will hit a wall during implementation.
Captured here so they aren't rediscovered the hard way.

### Constraint 1 — `dispatch.bsm` is currently SKIPPED in the modular pipeline

`bpp_dispatch.bsm` only runs in the monolithic compile path; the modular
pipeline (the path that produces `.bo` cache files and that every game now
uses) skips it entirely. This means Phase 2 cannot start by extending
`classify_function()` and expecting it to run — the dispatch pass first has
to be wired into the modular pipeline as a real phase between typecheck and
codegen. This is a precondition for commits 8 and 9, not a "nice-to-have".
Handled as commit 7-pre (a) in the Phase 2 ordering.

### Constraint 2 — `fn_phase[]` MUST use eager rebuild, not incremental

This is the same lesson as `find_struct` from
`feedback_lookup_strategies.md`. Functions get registered into `funcs[]` from
two sites: `bpp_parser.bsm` (when source is parsed) AND `bpp_bo.bsm` (when a
`.bo` cache file is loaded, around line 712, the same site that bypassed
`add_struct_def`). If Phase 2 uses an incremental "register phase at
function-creation time" strategy, it will silently miss every function that
comes from the cache loader, and the bug only surfaces when a cached
module's function is the first thing dispatched to a worker.

Required strategy: eager rebuild. `classify_function()` runs as a pass that
walks `funcs[]` from start to end at the entry to the dispatch phase,
populating `fn_phase[]` from the live state. Same model as `find_func_idx`
from the 0.21 refactor — eager tolerates missed insertion sites because it
re-derives from whatever is in `funcs[]` at the moment the pass runs. This
is non-negotiable; the `find_struct` incident from 0.21 is exactly the
precedent.

### Constraint 3 — single source of truth for time (`stbbeat` vs `stbgame`'s existing `_stb_last_time`)

`stbgame.bsm` already maintains `_stb_dt_ms`, `_stb_last_time`, and
`_stb_frame_ms` for its own frame loop. `stbbeat.bsm` will independently
read `_stb_get_time()` from the platform layer. Both reading from the same
platform clock is fine, but the maestro must NOT compute its own dt while
also calling `game_frame_begin()` which computes its own dt. The two will
drift by one platform-clock read per frame and produce subtly different
frame times.

Resolution: `stbmaestro` is the timing authority during a `maestro_run()`
session. It owns the call to `_stb_get_time()` via `stbbeat`, computes dt
once per tick, and passes that dt down to the user callbacks
(solo/base/render get dt as their argument, not from `stbgame` globals).
`stbgame`'s existing dt machinery continues to work for non-maestro programs
(the existing `snakefull_gpu.bpp` still runs, unchanged), but maestro
programs read dt from the maestro context, not from `stbgame` globals. This
is a one-line decision in `stbmaestro`'s loop but it has to be written down
because forgetting it means `snake_maestro` will tick at slightly the wrong
rate and the side-by-side comparison won't quite match.

### Constraint 4 — cached functions need their phase serialized in `.bo`

Cached functions loaded from `.bo` files have `body_arr = 0` because the AST
isn't reconstructed. `has_side_effects()` cannot run on them (no body to
walk). This means `classify_function()` simply cannot classify cached
functions on the fly — their phase has to be serialized into the `.bo` file
at codegen time and deserialized at load time.

Without this, every game compilation after a cache hit would have cached
functions defaulting to `PHASE_AUTO`, and the validator (commit 9) would
either spuriously warn about every cached function or silently miss real
bugs. ~20 lines in `src/bpp_bo.bsm` to add the field to the export record.
Handled as commit 7-pre (b) in the Phase 2 ordering — the `.bo` format slot
is added BEFORE `classify_function()` is wired up, so commit 8 doesn't have
to bump the cache version mid-stream.

### Constraint 5 — pthread calling convention bridge needs an explicit smoke test

`pthread_create` expects an entry function with signature
`void* (*)(void*)`. B++ functions have signature `long long (long long)`. On
Apple Silicon and x86_64 SysV, `RDI`/`X0` carries the first argument and
`RAX`/`X0` carries the return — the two ABIs are bit-compatible for
one-arg/one-return functions, so this probably "just works". But "probably"
is not "verified", and finding out it doesn't work after `stbjob` is
half-built is the worst possible time.

Resolution: commit 3 (pthread FFI declarations) ships with
`tests/test_thread.bpp` that creates one thread, has it write to a global,
joins, and asserts the global. If the global is wrong, the ABI assumption is
broken and the fix is to declare the entry point via FFI with explicit
signature so the linker resolves it. Do not start commit 4 (`stbjob`) until
`test_thread.bpp` passes. This is change C from the code review.

### Constraint 6 — Linux pthread is blocked on ELF dynamic linking

This is the constraint that the original draft missed and that tries to bite
at commit 3 if you're not paying attention. `pthread_create` lives in
`libpthread.so.0` on Linux (or in `libc.so.6` on modern glibc). B++'s
`src/x86_64/x64_elf.bsm` writes static ELF with no PLT, no GOT, no
`DT_NEEDED`, no `PT_INTERP`. A statically-linked ELF cannot call into a
`.so`. There is no `-lpthread` flag that helps because there is no dynamic
linker to resolve it.

Resolution: Phase 1 ships macOS-only. Linux maestro is deferred until ELF
dynamic linking lands as its own milestone (P0 in `docs/todo.md`, independent
of this plan, multi-day project that touches PLT/GOT generation, dynamic
relocations, program header layout, and the `PT_INTERP` interpreter path).
Once dynlink ships, Linux maestro inherits automatically because
`stbjob`/`stbmaestro` have zero platform-specific logic — only the platform
layer's pthread FFI declarations need to land.

**Why this is not Option 2** (block Phase 1 on dynlink): bundling ELF
dynlink into Phase 1 would inflate the plan from ~770 lines to ~1500+ lines,
double the timeline, and delay Rhythm Teacher (Game 1 in
`docs/games_roadtrip.md`, which itself needs threads for audio). It would
also force a major piece of compiler work into a plan that's supposed to be
about the maestro pattern. The honest decoupling is: maestro pattern ships
now on macOS, dynlink ships independently, Linux gets both when both exist.

**Why this is not a regression**: every existing Linux game
(`snakefull_gpu.bpp`, the X11 Wolfenstein-style raycaster prep, etc.) uses
pure syscalls (X11 wire protocol, no shared library calls). They continue to
work on Linux unchanged. Phase 1 only adds a new path (`snake_maestro.bpp`)
that happens to require pthread; the old paths are untouched.

**Open question worth flagging**: it isn't even certain that ELF dynamic
linking can be cleanly tested via Docker (different glibc versions,
different `ld-linux-x86-64.so.2` paths, alpine vs ubuntu vs debian
variations). That uncertainty by itself is a strong argument for keeping
dynlink in its own scoped milestone where it gets the testing attention it
needs, instead of sneaking it in under another plan.

---

## Slice usage in infrastructure (the design principle)

The slice types (`: byte`, `: half`, `: quarter`, `: half float`) were
planted by the language designer years ago expecting they'd be consumed by
INFRASTRUCTURE (dispatch, workers, ECS internals) — NOT by user game code.
The user clarified during planning: "the consumer was always meant to be
dispatch / maestro / workers, not the programmer manually packing structs."

This plan explicitly uses sub-word slices in the new infrastructure to
optimize cache usage and memory bandwidth at the root. Worker threads
processing arrays benefit massively from cache-friendly data layouts, and
2-8x improvements are realistic when the right fields are packed.

### Where slice types are used in this plan

**`stbjob.bsm` job queue entry** — pack metadata in bytes/halves so more
entries fit per cache line:

```bpp
struct JobEntry {
    fn_ptr,                  // word: function address (must be 64-bit)
    arg,                     // word: argument
    worker_id: byte,         // 0-N workers, byte fits
    status: byte,            // pending(0)/running(1)/done(2)
    priority: byte,          // 0-255 scheduling priority
    flags: byte,             // bit flags: cancel, retry, persist
}
// Total: 20 bytes vs 48 bytes if all word-sized.
// On a 64-byte cache line, that's 3 entries instead of 1.
```

**`stbmaestro.bsm` internal state** — small fixed-rate counters fit in half:

```bpp
struct MaestroState {
    tick_count: half,        // 32-bit good for 49 days @ 60fps
    phase: byte,             // 0=solo, 1=base, 2=render, 3=quit
    worker_count: byte,      // 0-255 workers
    pad_lo: half,            // alignment
    beat_zero,               // word: ms since process start
    callbacks_arr,           // word: pointer to callback table
}
```

**`stbecs.bsm` World struct** (Phase 1.5, commit 6b) — slices the
bookkeeping fields (capacity, count, free_count to `: quarter`) as part of
the stb sweep. The big-payoff future extension — exposing sub-word slice
types to component arrays themselves — is a separate plan, but the
per-entity savings would look like this when it lands:

| Component | Today | With slice (future) | Saving |
|---|---|---|---|
| `pos_x[id]`, `pos_y[id]` | word | word (need precision) | 0 |
| `vel_x[id]`, `vel_y[id]` | word | word (need precision) | 0 |
| `alive[id]` | byte (already!) | byte | — |
| `life[id]` | word | byte (0-255 frames) | 8x |
| `color[id]` | word | half (32-bit RGBA) | 2x |
| `entity_type[id]` | word | byte (0-255 types) | 8x |
| `flags[id]` | word | byte (8 bit flags) | 8x |

Per-entity: 56 bytes today → 24 bytes with full slicing. More than 2x
reduction in memory bandwidth when a base-phase worker iterates over
particles. The fact that `alive[]` was sliced as byte from day one is the
proof that slices were always meant for this — Phase 1.5 finishes activating
that intent for the bookkeeping, and a follow-up plan finishes it for the
component arrays.

### How this validates the slice type audit

Earlier in this planning session I (assistant) audited slice type usage and
found "zero consumers outside tests". I framed this as dead weight. That was
wrong. The user clarified: the slice types were planted for THIS moment —
when B++ would have parallel infrastructure that benefits from
cache-friendly layouts. They were waiting for a consumer that didn't exist
yet.

Phase 1 introduces the infra consumer (`stbjob`/`stbmaestro`). Phase 1.5
takes the next step the user explicitly demanded: apply slices in the
existing stb library too, both to capture the modest cache wins and (more
importantly) to give programmers canonical worked examples in modules they
already read every day. The slice types graduate from "feature documented in
the manual nobody uses" to "the way the standard library is written, and the
model the programmer copies when they want to optimize their own structs".
B++ is typeless on top, typed underneath, in the same spirit as D's optional
GC.

### Why this plan respects "use what exists"

| User concern | Resolution |
|---|---|
| "Don't add new layers" | `:` annotation syntax reuses existing colon-hint pattern. Two new lexer keywords (`base`, `solo`) but no new syntactic forms. |
| "Reuse what dispatch already does" | `classify_function()` calls existing `has_side_effects()` per-statement. No new analysis logic. |
| "FFI override" | Already exists via `add_pure_extern()`. The `pure_ext` whitelist name is historical and stays as-is. No change needed. |
| "Compile-time pure functions" | `const FUNC(args)` already exists, kept untouched. Orthogonal to runtime phase classification. |
| "Variables sem auto serem global" | Verified: B++ requires `auto`. Parser tracks locals via `var_stack_names`. Dispatch already queries via `dsp_is_local`. No change. |
| "Don't write a new threading layer" | pthread via FFI on both platforms. No new compiler builtins for thread creation, no syscall wrappers. macOS already links libSystem; Linux adds `-lpthread` (once dynlink lands). |
| "Don't add CAS for one use case" | N×SPSC queues, one per worker, round-robin submit. Each queue has exactly one producer and one consumer, so memory ordering via `mem_barrier()` is sufficient. CAS is reserved for "future work when a real game proves it's needed". |

### Snake as the test bed

Snake has been the canary for every major B++ feature: first program
self-compiled, first GPU game, first Linux X11 game, first Docker game.
Continuing the tradition: snake is the first game on `stbmaestro`. This is
recorded narratively in `docs/snake_report.md` and reinforced by keeping
`snakefull_gpu.bpp` untouched alongside the new `snake_maestro.bpp`.
