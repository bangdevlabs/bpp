# RTS Stress Arc — pushing B++ to Red Alert scale

**Strategic runtime engineering arc that scales B++'s existing
ECS / profile / multicore / SIMD infrastructure from "demo scope"
(2 unit types per `games_roadtrip.md`) to "Red Alert scale stress
test" (30+ unit types, 1000+ live units, 60fps).**

This document is the canonical anchor for a 5-6 session arc that
runs after the Excalibur Arc closes (or interleaved if the user
chooses). Either the user or the Emacs agent should read this
before any session here to understand sequencing, dependencies,
and pause/resume contracts.

The user named the target "B++ cantando" during planning — the
ambition is to prove B++ can carry a real RTS workload, not just
a vertical-slice demo. The arc is what unlocks that proof.

---

## Premise

B++ today (2026-05-12) has the primitive pieces for game-scale
simulation:

- **`stb/stbecs.bsm` (177 LOC)** — parallel-array ECS with
  spawn/kill/recycle, fixed components (pos/vel/flags) plus
  custom components via `ecs_component_new(world, element_size)`.
- **`stb/stbprofile.bsm` (801 LOC)** — Tier 1/2/3 HUD, `@profile`
  scoped zones, per-thread breakdown, GPU timing. Mature.
- **`src/bpp_job.bsm` (490 LOC) + `src/bpp_maestro.bsm` (299 LOC)**
  — worker pool, SPSC ring buffers, `job_parallel_for`. Sprint
  1+2a shipped; sprints A-G partially open (see `docs/todo.md`).
- **`stb/stbpath.bsm` (426 LOC)** — A* via `path_find(pf, sx, sy,
  gx, gy, out_buf, max_steps)`. Per-unit, no flow fields.
- **`vec_*` SIMD builtins** — dispatch logic exists in
  `bpp_codegen.bsm`; specific ops vary by chip.

Real consumers of stbecs today:
- `games/pathfind/pathfind.bpp`
- `games/snake/particles.bsm`, `games/snake/snake_maestro.bpp`
- `games/fps/fps_wolf3d.bpp`, `games/fps/ai.bsm`
- `examples/snake_cpu.bpp`, `examples/snake_gpu.bpp`

The dominant usage pattern is `for i in 0..capacity { if alive[i] {
... } }` — manual iteration over parallel arrays. Works at
demo scale; cache-unfriendly and serial at AAA scale.

This arc closes five concrete gaps between the current shape and
the Red Alert / Warcraft scale target.

---

## Anti-features (explicit non-goals)

To keep the arc focused, the following are NOT in scope:

- **Templates / metaprogramming** — Excalibur Feature 4
  graduated out 2026-05-12 after the audit confirmed C&C Red
  Alert and Warcraft scaled to AAA without templates (pure OOP
  with virtual methods). ECS-based polymorphism over data
  covers the same ground with better cache locality.

- **GPU compute shaders for unit simulation** — Industry research
  (2026-05-12) confirms GPU compute is mainstream for **specific
  workloads only** (particles via Unreal Niagara, cloth via
  PhysX, fluid via NVIDIA Flex). RTS unit AI / strategy logic
  stays CPU across the industry because branching, state
  divergence, and CPU↔GPU sync overhead beat the SIMD win.
  GPU rendering (existing Phase 4-7 pipeline) continues to be
  used for sprite/quad rendering exactly as today.

- **Particle-effects-via-GPU-compute** — Possible post-1.0
  specialization if a specific game (e.g. RTS with massive
  destructible terrain) demands it. Separate sidequest, not
  part of this arc.

- **Network multiplayer / lockstep determinism** — `stbnet`
  is post-1.0 work (`docs/games_roadtrip.md`). This arc is
  single-player stress test.

- **Fixed-point arithmetic** — used in lockstep multiplayer
  RTS (StarCraft, AoE) for cross-platform determinism. Out of
  scope until multiplayer arc opens.

- **AI behavior trees / GOAP** — Phase 2 enemy AI in Wolf3D
  is hand-rolled FSM per `games/fps/HANDOFF.md`. Stress arc
  inherits that pattern at scale; advanced AI substrate is
  separate.

---

## Industry context — how AAA RTS scaled

| Pattern | Era | Examples | Why it shipped |
|---------|-----|----------|----------------|
| OOP virtual methods | 1995-2003 | C&C Red Alert, Warcraft, StarCraft | C++ pre-template-maturity; vtables = compact dispatch |
| Component-based ECS | 2003-2015 | Sins of a Solar Empire, custom engines | Cache locality wins at 1000+ entities |
| Archetype ECS + jobs | 2018-present | Unity DOTS, Bevy, modern AAA | Data parallelism + multicore + SIMD |
| Templates / metaprog | (rarely) | Some C++ AAA (declined post-2003) | Avoided — see Excalibur Feature 4 graduation |

B++ today is at "basic ECS without archetypes." The stress arc
brings it to "archetype ECS + jobs + SIMD" — the modern
mainstream tier.

---

## Inventory — what we already have

### `stb/stbecs.bsm` — minimal but real

API surface:
```bpp
ecs_new(capacity)                      // creates World with parallel arrays
ecs_spawn(w) → id                      // returns entity ID 0..capacity-1
ecs_kill(w, id)                        // recycles ID to free pool
ecs_alive(w, id) → 0|1                 // alive bitmap check
ecs_count(w) → int                     // live entity count
ecs_set_pos/vel/flags(w, id, x, y)     // built-in components
ecs_component_new(w, element_size)     // custom component allocator
ecs_component_at(comp, id, size) → ptr // typed access for struct components
ecs_physics(w, dt)                     // canned position += velocity system
```

Strengths: simple, works, 5+ real consumers in the codebase.

Gaps (mapped to sessions below):
- No query system — manual `for i, if alive[i]` everywhere
- No archetype chunks — cache misses at scale
- No SIMD usage inside systems — scalar update loops
- No system scheduler — programmer wires call order by hand
- No multicore integration — `ecs_physics` is sequential

### `stb/stbprofile.bsm` — mature, ready

801 LOC. Tier 1/2/3 HUD, `@profile` scoped zones, per-thread
breakdown, GPU timing. **Zero work needed for this arc.** Stress
arc will use it heavily to find bottlenecks.

### Multicore — `bpp_job` + `bpp_maestro`

`job_parallel_for(start, end, fn_ptr)` proven. Workers + SPSC
rings shipped. Sprints A-G partially complete (see todo.md).

Integration gap: stbecs doesn't call into `job_parallel_for`
today. Systems iterate serially.

### `stb/stbpath.bsm` — A* only

`path_find(pf, sx, sy, gx, gy, out_buf, max_steps)` works for
N≤10 simultaneous pathing. Above that, per-unit A* explodes
quadratically.

Gap: no flow fields. For RTS scale (50+ units moving to same
objective), flow field is the canonical solution.

### `vec_*` SIMD builtins

Compiler has SIMD dispatch logic (`bpp_codegen.bsm:344, 2565`).
Specific ops vary by chip. Need targeted use inside ECS systems.

---

## Sessions in detail

### Session 1 — ECS query + iterator (~120 LOC)

**Goal**: replace `for i in 0..capacity, if alive[i]` boilerplate
with `ecs_query_each(query, fn_ptr)`. Skip-dead iteration via
alive bitmap scan. Foundation for sessions 2-5.

**Files**: `stb/stbecs.bsm` (extend), `tests/test_ecs_query.bpp`
(new), migration of one existing consumer (pathfind preferred).

**API additions**:
```bpp
ecs_query(world, comp_mask) → query_handle       // selector
ecs_query_each(query, fn_ptr)                    // callback per live entity
ecs_query_count(query) → int                     // live count matching
```

**Scope**:
- `comp_mask` is opaque word; for now just "alive" (mask=1).
  Real component mask waits for archetype storage (Session 2).
- `fn_ptr` is called with `(world, id)` per live entity.
- Loop hot path: `for i in 0..capacity { if alive[i] { call(fn, w, i); } }`
  in the iterator core; consumers don't write it.

**LOC**: ~80 in stbecs + ~40 in tests.

**Pause-safety**: legacy `for i, if alive[i]` patterns continue
working. New API is additive.

**Verification**:
- Bootstrap byte-stable
- Suite green (mantém baseline)
- `tests/test_ecs_query.bpp` confirms query iterates only live entities
- Migrate `games/pathfind/pathfind.bpp` to use `ecs_query_each` for
  one of its loops — smoke test still works

---

### Session 2 — Archetype storage (~400 LOC)

**Goal**: entities sharing the same component set live in
contiguous chunks. Iteration becomes linear stream → cache hit
rate goes from ~60% to ~95%. The big single session of the arc.

**Files**: `stb/stbecs.bsm` (major refactor), `tests/test_ecs_archetype.bpp`,
benchmark `tests/bench_ecs_iter.bpp`.

**Design**:
- World owns a list of archetypes
- Each archetype = (component_set, chunk_list)
- Each chunk = 16KB block, holds N entities of identical component shape
- `ecs_spawn(world, comp_mask)` finds-or-creates archetype, allocates
  in chunk
- `ecs_query_each` walks matching archetypes, iterates chunks linearly

**Trade-off**:
- Old API (`ecs_set_pos`, parallel arrays) MUST stay backward
  compat — 5+ consumers depend on it. Internal: legacy entities
  live in a "default archetype" with the original layout.
- New API: `ecs_spawn_archetype(world, components_descriptor)` returns
  entity in chunk storage.

**LOC**: ~400 (refactor + new API + backcompat shims).

**Pause-safety**: behind feature flag during dev
(`BPP_ECS_ARCHETYPES=1`). Default off → identical to today.
Migration of consumers happens session-by-session post-Session 2.

**Verification**:
- Bootstrap byte-stable
- Suite green with flag off (zero impact on existing code)
- Suite green with flag on (new tests pass)
- `tests/bench_ecs_iter.bpp` shows 3-5x speedup for 1000-entity
  iteration vs legacy layout (concrete measurement, not theoretical)

---

### Session 3 — SIMD batching inside systems (~120 LOC)

**Goal**: refactor `ecs_physics` to use `vec_*` builtins on
4-entity chunks. Reusable pattern for other systems (damage,
collision broad-phase).

**Files**: `stb/stbecs.bsm` (`ecs_physics` rewrite), example pattern
in `docs/how_to_dev_b++.md` for game-side systems, benchmark.

**Pattern**:
```bpp
// Before (Session 1+2):
ecs_query_each(query_pos_vel, fn_ptr(update_one));

// After (Session 3):
// stbecs internally calls SIMD batch processor for chunks of 4
// when system function has @safe + vec compatible signature.
```

**Real implementation**: depends on what `vec_*` actually supports
in current B++. Session begins with 20-min audit of available
ops. If `vec_add4` / `vec_mul4_scalar` exist → straightforward.
If not → session expands to ~200 LOC adding necessary builtins
to chip backends.

**LOC**: ~120 in stbecs + ~30 doc examples. Could grow to ~250
if vec builtin additions are needed.

**Pause-safety**: SIMD path is fast-path optimization. Scalar
fallback always works. Bench validates.

**Verification**:
- Bootstrap byte-stable
- Suite green
- Bench: ecs_physics over 1000 entities — 2-4x speedup vs Session 2
  scalar version (concrete measurement)
- Visual smoke: snake / pathfind / fps_wolf3d still behave correctly

---

### Session 4 — Flow fields pathfinding (~250 LOC)

**Goal**: new module `stb/stbflow.bsm` for one-to-many pathfinding.
Compute one flow field per goal; all units sample. RTS scale unlocked.

**Files**: `stb/stbflow.bsm` (new), `tests/test_stbflow.bpp`,
example consumer in `games/pathfind/` (waypoint-style demo).

**API**:
```bpp
flow_field_new(grid_w, grid_h) → field
flow_field_compute(field, goal_x, goal_y, blocked_mask)
flow_field_sample(field, x, y) → (dx, dy)
flow_field_free(field)
```

**Implementation**: Dijkstra from goal, BFS-style. Backward
direction (gradient toward goal) stored per cell. O(w*h) compute,
O(1) sample. Reuses `stbpath` blocked-cell helpers for terrain.

**LOC**: ~250 (compute + sample + tests + integration).

**Pause-safety**: new module, doesn't touch existing stbpath.
A* stays for single-unit pathing.

**Verification**:
- Bootstrap byte-stable
- Suite green
- `tests/test_stbflow.bpp` confirms 100 units paths to same goal
  in O(field_compute + 100*sample) instead of O(100*A*)
- Bench: 100 units → goal, measure ms total. Target: <2ms total.

---

### Session 5 — System scheduler (~120 LOC)

**Goal**: register systems, scheduler ticks them in order with
auto-parallelism via maestro pool for independent systems.

**Files**: `stb/stbecs.bsm` (scheduler addition), `tests/test_ecs_scheduler.bpp`.

**API**:
```bpp
ecs_system_register(world, fn_ptr, system_flags) → handle
ecs_systems_tick(world, dt)
```

**Flags**:
- `SYS_PARALLEL` — can run in parallel with other parallel systems
  (compiler/user asserts no shared-state mutation)
- `SYS_SERIAL` — must run on main thread (rendering, input)

**Implementation**: registry of (fn_ptr, flags). Tick groups
adjacent `SYS_PARALLEL` systems and dispatches via
`job_parallel_for`-style fan-out. `SYS_SERIAL` runs main thread.

**LOC**: ~120.

**Pause-safety**: callers can opt in by registering systems. Old
code that calls systems by hand keeps working.

**Verification**:
- Bootstrap byte-stable
- Suite green
- `tests/test_ecs_scheduler.bpp`: 2 independent parallel systems
  run concurrently on 2-core machine, measured by per-thread profile
- Migration of one game (snake_maestro recommended) to scheduler
  pattern. Validates real consumer.

---

### Session 6 — RTS stress demo (~300 LOC) [optional capstone]

**Goal**: `games/rts_stress/` — proof that everything composes.
30 unit types via ECS components, 1000+ units on map, 60fps target.

**Not required for arc close** — sessions 1-5 ship the
infrastructure. Session 6 is the empirical proof. May ship as
separate work after the infra arc closes.

**Scope**:
- 30 unit-type recipes (component combinations) — not 30 hardcoded structs
- 1000 units spawned at start, behaviors: idle/move/attack
- 1 map with terrain
- Profile HUD always on
- Frame budget: 16ms. Profile reports per-system time.

**LOC**: ~300 game code.

**Verification**:
- 1000 units, 60fps maintained for 60-second run on dev machine
- Profile HUD shows: render <8ms, physics+AI <6ms, headroom for spikes
- Visual: army moves and engages correctly

---

## Sequencing

**Hard dependencies**:
- Session 1 (query) → Session 2 (archetype) — query API is the
  consumer interface to archetypes
- Session 2 (archetype) → Session 3 (SIMD) — SIMD needs cache-
  contiguous data
- Sessions 4 + 5 are independent of 2/3 — can land in any order
- Session 6 (stress demo) requires 1-3 minimum, ideally 1-5

**Recommended order**:
```
1.A (query)       ─── foundation
2.A (archetype)   ─── major refactor with flag
3.A (SIMD)        ─── perf win on archetype layout
[pause point]
4.A (flow fields) ─── independent
5.A (scheduler)   ─── ties it together
6.A (stress demo) ─── empirical proof [optional]
```

**Parallel allowed**:
- Session 4 (flow fields) can ship between any two other sessions
- Wolf3D Phase 2 sessions can run between Stress Arc sessions
- Excalibur sessions can interleave (different files; no merge conflict)

---

## Pause / resume convention

Same discipline as Excalibur Arc:

1. Current session finishes cleanly (no half-done state)
2. Bootstrap byte-stable + suite green confirmed
3. Commit the session
4. Update Status section below with `next: <session ID>` marker
5. Wolf3D / Excalibur / other work can resume

---

## Triggers — when this arc opens

**Hard prerequisite**: Excalibur Features 1+2+3 closed. Reasons:
- Polymorphic literals (Excalibur 1) → cleaner ECS math expressions
- Cast builtins (Excalibur 2) → cleaner integer/float boundaries in
  scaled milli-units → pixel rendering
- Struct newtype (Excalibur 3) → opt-in safety for World vs Grid
  coordinates in pathing/ECS

**Soft prerequisites**:
- Wolf3D Phase 2 demo level shipping or near-shipping (proves the
  basic stack on a real game before stressing it)
- User signal: explicit "let's start the stress test" go-ahead

**Anti-trigger**: don't open the arc just because Excalibur closed.
Wait for one of the soft prerequisites too.

---

## Status

**Arc opened**: 2026-05-12. Excalibur Features 1+2+3 closed
same-day; Session 1 shipped immediately after per the
continuous-execute directive.

**Current state**: Session 1 SHIPPED 2026-05-12.

**Next session**: Session 2 — Archetype storage (~400 LOC, the
big single session of the arc).

**Session 1 — SHIPPED**:
- `EcsQuery` struct + `ecs_query(w, comp_mask)` /
  `ecs_query_each(q, fn_ptr)` / `ecs_query_count(q)` /
  `ecs_query_free(q)` API in `stb/stbecs.bsm`. comp_mask is
  future-proofing: today only `1` (any live entity) is
  meaningful; richer masks unlock once archetype storage lands
  in Session 2.
- `tests/test_ecs_query.bpp` covers count, each, kill/respawn
  churn, callback-receives-correct-id-set.
- One pathfind consumer migrated: `update_particles` in
  `games/pathfind/pathfind.bpp` now dispatches via
  `ecs_query_each(_, fn_ptr(_pf_step_particle))` instead of the
  inline `for i, ecs_alive(world, i)` loop. Functionally
  identical — same skip-dead pattern, same body, same per-frame
  alloc/free.
- Bootstrap byte-stable, suite 154/0/12 + 127/0/39.

**Session 2**: not started
**Session 3**: not started
**Session 4**: not started
**Session 5**: not started
**Session 6**: optional capstone, not started

---

## Tonify rules emerging from this arc

Anticipated new rules (added at session close):

- After Session 2: Rule 29 (suggested) — "ECS scale: archetype
  storage for cache locality, parallel arrays for ad-hoc components.
  Pick by entity count: <100 = parallel arrays fine, >1000 = archetype."

- After Session 5: Rule 30 — "System scheduler over hand-wired ticks
  when N systems >= 4. Below that, plain function calls are clearer."

Numbering adjusts based on what's added during Excalibur in parallel.

---

## Cross-references

- `docs/excalibur_arc.md` — language polish arc (parallel)
- `docs/games_roadtrip.md` Game 3 (RTS) — demo scope vs stress scope ladder
- `docs/todo.md` Multi-core completion (Sprints A-G) — adjacent work
- `docs/tonify_checklist.md` Rule 28 — killer-use-case gate that
  justified Templates graduation out of Excalibur
- `stb/stbecs.bsm` — current state (177 LOC, basic shape)
- `stb/stbprofile.bsm` — mature, used heavily during this arc
- `src/bpp_job.bsm` + `src/bpp_maestro.bsm` — multicore primitives

---

## Why this is "B++ cantando"

The arc was named by the user during planning as the proof point
that B++ is a serious game-dev language, not just a clean toy.

Each session is a step toward that proof:

- Session 1 makes ECS ergonomic — devs stop writing manual
  `for i, if alive[i]` loops
- Session 2 makes ECS fast — 3-5x iteration speedup via cache
  locality
- Session 3 makes ECS SIMD-fast — 2-4x more on top
- Session 4 makes pathing scale — 100 units pathing without
  quadratic cost
- Session 5 makes systems parallel — multi-core utilization
- Session 6 proves it visually — 1000 units, 60fps, on screen

When all four infra sessions ship, the gap between B++ and a
modern ECS engine (Unity DOTS, Bevy) closes substantially. Then
shipping an RTS demo at Red Alert scale becomes "write the game
code," not "fight the engine."

That's the cantar.
