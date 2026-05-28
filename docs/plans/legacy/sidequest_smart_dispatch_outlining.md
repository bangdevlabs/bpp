# Sidequest — Smart-dispatch outlining + captured values

**Status:** PROPOSED 2026-05-22, design-phase open.

**Trigger:** Gate 6 of `_fdc_subtree_safe` (`src/bpp_dispatch.bsm:2268`)
rejects loop bodies that reference function parameters or
outer function-locals. A naive relax (commit `4a9d3e7`+wip,
2026-05-22, reverted) accepted such references as long as
they weren't written in body — but the synth worker function
has its own frame and CANNOT see the caller's stack. Tests
crashed (SIGSEGV in `test_reduce_runtime`, 141/141 C-emit
failures cascading). The relax is fundamentally wrong without
**outlining + captured values**.

**Industry name for what we need:** *procedure outlining for
parallel-for*. Established compiler transformation since the
1970s (Allen + Cocke), production-grade implementations in
GCC OpenMP, LLVM `llvm.openmp.*`, Cilk Plus spawn frames,
Intel TBB lambda closures, Unity Burst `IJobParallelFor`
structs, Apple GCD blocks, Rust rayon. Same shape every time:
captured free variables packaged into a struct, outlined
function reads from struct.

**Benchmark anchor:** real game-update pattern that's
impossible to auto-paralelize today —

```bpp
void my_physics(world, dt) {
    auto i;
    for (i = 0; i < world.count; i++) {
        update_unit(i, world, dt);   // refs world, dt = parent-frame
    }
}
```

This loop is exactly what Burst's `IJobParallelFor` captures
into a struct, then dispatches per-iteration. B++ today
requires the programmer to refactor the loop into explicit
`job_parallel_for(N, fn_ptr(__worker))` AND extract
`__worker` as a separate function that uses globals or struct
args. The outlining transformation does the refactor
automatically.

---

## Why this is its own sidequest

The simple "relax gate 6 to accept names not in write-set"
approach (one afternoon's experiment, reverted) breaks
because the synth worker function lives in a **separate
frame** and a **worker thread**. Caller-frame names are
inaccessible — synth body emits loads from non-existent
slots, producing garbage or segfaults at runtime.

Making the relax safe requires three new pieces:

1. **Free-variable analysis** — identify every name in the
   loop body that is NOT a loop-local, NOT a loop var, and
   NOT a global. Those are the "captures".
2. **Capture publication** — caller-side code that copies
   captured values into a known location workers can read.
3. **Synth function rewrite** — outlined body reads captures
   from the known location instead of expecting them in its
   own frame.

This is the canonical outlining transformation. Skipping any
of the three pieces breaks correctness.

---

## Industry research — outlining patterns

Five reference points surveyed before locking the design.

### GCC OpenMP — `omp_data_t` + outlined function

GCC lowers `#pragma omp parallel for` into:

```c
// User code:
#pragma omp parallel for shared(world) firstprivate(dt)
for (int i = 0; i < n; i++) update(i, world, dt);

// GCC outlined IR:
struct .omp_data_t { Array *world; float dt; };
void .omp_fn(void *_d) {
    struct .omp_data_t *d = _d;
    /* per-thread iteration range computed from omp runtime */
    for (i = ...) update(i, d->world, d->dt);
}
GOMP_parallel(.omp_fn, &data, ...);
```

`firstprivate` = capture by value, copied per-thread.
`shared` = capture by reference (pointer to caller's slot).

Source: GCC source `gcc/omp-low.c` (~6 k LOC), GCC
documentation "OpenMP Implementation" chapter, Diego Novillo
papers on GCC OpenMP lowering.

### LLVM OpenMP — `llvm.openmp.parallel` outliner

Similar shape: outliner pass extracts the loop body into a
new function, captures via struct. Polly extends this for
polyhedral transformations.

Source: LLVM `llvm/lib/Transforms/IPO/OpenMPOpt.cpp`, LLVM
OpenMP runtime documentation.

### Cilk Plus — `cilk_for` spawn frames

`cilk_for (int i = 0; i < n; i++) { update(i, dt); }`
generates an outlined worker function plus a "spawn frame"
struct holding captured values. Runtime distributes work via
work-stealing scheduler.

Source: Frigo, Halpern, Leiserson, Lewin-Berlin, "Reducers
and other Cilk++ hyperobjects" (2009); Intel Cilk Plus
runtime documentation.

### Unity Burst — `IJobParallelFor` explicit struct

User writes the capture struct themselves:

```csharp
struct UpdatePositionsJob : IJobParallelFor {
    public float dt;
    public NativeArray<float3> positions;
    public void Execute(int i) {
        positions[i] += velocities[i] * dt;
    }
}
new UpdatePositionsJob { dt = Time.dt, positions = pos }.Schedule(n, 64);
```

Burst-compiles the struct + Execute method into worker code.
No magic — programmer is the outliner.

Source: Unity Burst documentation, "Burst Compiler User
Manual" 2024.

### Apple GCD — Blocks with `__block` capture

```objc
__block int captured = 42;
dispatch_apply(n, queue, ^(size_t i) { use(captured); });
```

Clang's blocks runtime synthesises a struct holding captured
references, the block invocation reads from the struct.

Source: Apple Developer documentation "Blocks Programming
Topics", clang `lib/CodeGen/CGBlocks.cpp`.

### Universal shape

| Component | Every system |
|---|---|
| Identify free variables in body | ✓ |
| Generate capture struct | ✓ |
| Outline body into separate function | ✓ |
| Pass captures to outlined function | ✓ via struct ptr or implicit closure object |
| Per-iteration index passed separately | ✓ |
| Runtime distributes work | ✓ |

B++ smart-dispatch already has the LAST piece (`job_parallel_for`
distributes work). Missing: 1, 2, 3, 4. This sidequest adds
them in B++-idiomatic form.

---

## Design decisions (to resolve before P0)

### Q1 — Capture storage location

Options:
- **Per-callsite thread-local slot** — runtime sets a TLS
  pointer to the capture struct; worker reads via TLS.
  Pros: zero stack-frame overhead, clean. Cons: TLS not yet
  standard infrastructure in B++ runtime.
- **Per-callsite global slot** — one global per outlining
  site, populated by caller before `job_parallel_for` call.
  Pros: simplest. Cons: not re-entrant (nested parallel
  dispatch would clobber).
- **Pass struct pointer as argument** — `__synth_K(idx, captures)`.
  Pros: re-entrant, clean, matches GCC OpenMP exactly. Cons:
  `job_parallel_for` runtime today only passes `idx`.
  Requires `job_parallel_for_data(n, fn, data)` or similar
  variant.

Recommend: option (c). Mirror GCC OpenMP exactly. Re-entrant
by construction. The runtime extension is bounded
(`job_parallel_for_data` helper, ~30 LOC).

### Q2 — Capture mode (by-value vs by-reference)

- Scalars (`dt`, `n`, ...) → by-value (copy into struct).
  Single read per iteration, no aliasing concerns.
- Pointers / arrays / struct ptrs → by-reference (copy the
  pointer; the caller's underlying memory is the "captured"
  thing). The pointer value is stable; the memory it points
  to is the data workers iterate over.

Mirror of OpenMP's `firstprivate` vs `shared`. The decision
is automatic based on the variable's storage class /
declared type.

### Q3 — Free-variable identification

Walk loop body collecting every `T_VAR(name)` where `name`
is:
- NOT the loop variable
- NOT in `_fdc_loop_locals` (declared inside body)
- NOT a known global (GLOB_*)

Those are the free variables — must be captured.

Storage of the result: parallel arrays
`_fdc_capture_names[]` + `_fdc_capture_modes[]` populated
during the existing safety walk, rebuilt per candidate.

### Q4 — Write-set still required

The earlier (reverted) gate 6 relax used a write-set to
prove read-only. Outlining REUSES that machinery — captures
must be read-only in the body (write-through-capture would
cross worker boundaries with unclear semantics). Burst
similarly distinguishes `[ReadOnly]` (capture by value with
read-only access) from `[WriteOnly]` (per-worker output
buffer with disjoint write index).

For phase 1, accept only read-only captures. Write-through-
capture deferred to phase 2 of THIS sidequest (if needed).

### Q5 — Synth function signature change

Today:
```
static void __synth_K(idx) { /* loop body */ }
```

Post-outlining:
```
static void __synth_K(idx, captures: _capture_K) {
    /* loop body, with caller-frame names rewritten as captures->name */
}
```

Or capture struct as plain pointer:
```
static void __synth_K(idx, _cap_ptr) {
    auto _cap: _capture_K;
    _cap = _cap_ptr;
    /* body uses _cap.world, _cap.dt, etc. */
}
```

The second form fits B++'s existing struct/pointer idioms
better and avoids changing the synth signature's parameter-
type machinery.

### Q6 — Rewriter changes (Stage 3)

Today the rewriter replaces the for-loop with:
```
job_parallel_for(N, fn_ptr(__synth_K));
```

Post-outlining:
```
auto _cap_struct: _capture_K;
_cap_struct.world = world;
_cap_struct.dt = dt;
job_parallel_for_data(N, fn_ptr(__synth_K), &_cap_struct);
```

The struct allocation happens on the CALLER's stack (lives
until `job_wait_all` returns, which `job_parallel_for_data`
blocks on).

### Q7 — AST rewriting inside synth body

Currently `synthesize_loop_fn` builds the synth body by
sharing the loop's body pointer verbatim (the MAP path).
Post-outlining, the body needs to be CLONED with each
captured name's T_VAR rewritten to point at `_cap.name`
instead of the original.

Reuses `ast_clone_subst` machinery from S4. The substitution
map is `{ caller_name -> T_MEMLD(_cap + offset) }` for each
capture. Builds on existing infrastructure.

### Q8 — Bench acceptance gates

Per Tonify Rule 37:
1. Bootstrap byte-stable across the arc.
2. Suite 178/0/12 native + 141/0/49 C-emit at every phase.
3. New gate: every newly-synth-dispatched game loop must
   produce IDENTICAL output to the explicit
   `job_parallel_for` equivalent, byte-for-byte if
   deterministic.
4. `examples/tablah.bpp`-style anchor: a worker function
   originally written with `job_parallel_for` should also
   work when written as natural for-loop after outlining.

---

## Phase plan (proposed, ~6 phases, ~550 LOC)

| Phase | What | LOC est. | Risk |
|---|---|---|---|
| P0 | Design lock (this doc) + revert gate 6 (DONE) | ~50 | low |
| P1 | Free-variable analysis (`_fdc_collect_captures`) — walk body, populate `_fdc_capture_names[]` + `_fdc_capture_modes[]` parallel arrays | ~100 | low |
| P2 | Capture struct synthesis at classify time — generate `_capture_K` struct type with fields for each free var | ~80 | medium (touches type system) |
| P3 | Synth function rewrite — extend `synthesize_loop_fn` to inject the capture-struct param + AST-clone the body rewriting free-var refs to struct field accesses | ~120 | medium |
| P4 | Stage 3 rewriter extension — emit struct allocation + field assignment statements before the `job_parallel_for_data` call | ~100 | medium |
| P5 | Runtime: `job_parallel_for_data(N, fn, data)` variant in `bpp_job.bsm` + worker reads the data pointer | ~60 | low |
| P6 | Gate 6 relax (proper this time): accept caller-frame names in `_fdc_subtree_safe` because outlining now captures them | ~30 | low |
| Pn | Bench + iterate + games migration if appropriate | ~80 | depends |

**Total: ~620 LOC** across 3-4 sessions estimate.

Ship gate per phase: bootstrap byte-stable + suite green +
no regression on `bench_compile.sh`. Each phase a bisect-
friendly commit.

Risk peak at P3 (AST cloning with free-var substitution —
similar shape to S4 P3a-P3b alpha-rename machinery, so the
playbook is established).

---

## Next arc after this one — autovec / Phase B4 completion

Sequence locked 2026-05-22: outlining ships first, then
autovec is the next big-perf arc. The two compose
multiplicatively — outlining gives N-cores parallelism,
autovec gives M-lanes SIMD-per-iteration. Together they
approach the perf ceiling that Burst / Unity DOTS deliver
in C# (and that hand-tuned C++ with SSE/AVX intrinsics
delivers in production engines).

Rough estimate of the second arc:

| Feature gap | Status today | Impact pos-arc 2 |
|---|---|---|
| `: double` slice → NEON | a64 working, x64 incomplete | both chips full SIMD |
| Autovec pass (loop → SIMD inference) | None | implicit SIMD for safe loops |
| Aligned memory hints | Partial via `malloc_aligned` | first-class type slice |
| SoA-friendly iteration | Manual via `: u_word` array shapes | compiler-assisted |

Likely 2-3x the outlining sidequest in LOC. Multi-session.
Touches chip primitives, type system, codegen splice point.
Separate sidequest doc when this arc closes.

Composition target (theoretical hot-loop ceiling):

```
Baseline scalar single-thread       1x
+ S4 inline (shipped 2026-05-21)   ~1.1x
+ Outlining (THIS sidequest)        ~8x (cores)
+ Autovec / Phase B4 (next arc)     ~4x (SIMD lanes)
                                  ─────────
Total combined                     ~35x potential
```

## Out of scope explicitly

- **Write-through-capture** (Burst's `[WriteOnly]` /
  OpenMP's `lastprivate`). Phase 1 of this sidequest only
  handles read-only captures. Write-through can be added
  later as Phase 2 / a follow-on sidequest if a real game
  use case surfaces.
- **Nested parallel dispatch** — outlined function calling
  another outlined function. Out of scope; smart-dispatch
  scanner today rejects loops with T_CALL inside body's
  inline path, so this can't happen by construction.
- **Capture by closure** (Haskell-style currying). B++ has
  no closures; captures are explicit struct fields.
- **Cross-module outlining** — synth function and its
  capture-struct type live in the same module as the loop.
- **GPU dispatch via `@gpu`** — separate path entirely;
  shaders have their own capture mechanism (uniforms,
  vertex attribs). Out of scope.

---

## Acceptance gates (summary)

1. Bootstrap byte-stable: gen1 == gen2 == gen3 at every
   phase commit.
2. Native suite 178+/0/12 + C-emit suite 141+/0/49 green
   throughout.
3. New test: `tests/test_outlining_smoke.bpp` exercises a
   loop that needs caller-frame capture; runs correctly
   under auto-dispatch.
4. Existing game / example programs must produce
   byte-identical output to their pre-outlining build (since
   their explicit `job_parallel_for` use is unaffected).
5. No bootstrap regression > 5 % on `bench_compile.sh`.

---

## Open questions to ask at design phase kickoff

- Is there value in supporting the `job_parallel_for_data`
  variant separately from auto-outlining? (Programmers may
  want to write outlined-style code by hand for clarity even
  in cases the scanner doesn't catch.)
- What happens if a captured name shadows a global? Today
  the scanner doesn't have shadowing (function locals can't
  shadow globals at parse time). But outlining might surface
  the case if a programmer tries.
- How does the debugger (`bug`) display captures? The
  `_capture_K` struct is synth-generated and not in
  programmer source. Bug-watch on `world` inside the loop
  should still resolve to the captured pointer's target —
  needs source-name mapping similar to S4 P3b's mangled-
  local handling (Q8 of the cost-model inliner doc).

---

## Sources (industry research, 2026-05-22)

GCC OpenMP:
- "GCC OpenMP Implementation" — GCC internal documentation
- Diego Novillo, "OpenMP and automatic parallelization in
  GCC" — GCC Developers Summit 2006
- GCC source `gcc/omp-low.c`

LLVM OpenMP:
- LLVM source `llvm/lib/Transforms/IPO/OpenMPOpt.cpp`
- LLVM OpenMP runtime documentation

Cilk:
- Frigo, Halpern, Leiserson, Lewin-Berlin, "Reducers and
  other Cilk++ hyperobjects" (2009)
- "The Implementation of the Cilk-5 Multithreaded Language"
  (1995)

Burst:
- Unity Burst Compiler User Manual 2024
- "Unity DOTS Best Practices" — Unite 2022

Apple GCD:
- Apple Developer documentation "Blocks Programming Topics"
- clang `lib/CodeGen/CGBlocks.cpp`

Foundational:
- Frances Allen + John Cocke, "A Catalogue of Optimizing
  Transformations" (1972) — early discussion of procedure
  outlining
- Aho, Lam, Sethi, Ullman, "Compilers: Principles,
  Techniques, and Tools" (Dragon Book) chapter on parallel
  loop transformations

B++ anchor files (foundation this sidequest extends):
- `src/bpp_dispatch.bsm:2268` — `_fdc_name_is_safe` (gate 6)
- `src/bpp_dispatch.bsm:3223` — `synthesize_loop_fn`
- `src/bpp_dispatch.bsm:3342` — Stage 3 rewriter (loop-to-
  call transformation)
- `src/bpp_job.bsm:303` — `job_parallel_for` runtime
- `docs/manual/tonify_checklist.md` Rule 38 — explicit vs
  implicit parallel dispatch (the doctrine this sidequest
  amplifies)
