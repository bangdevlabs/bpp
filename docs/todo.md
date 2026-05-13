# B++ Roadmap — Pending Only

Forward-looking. What shipped lives in `docs/journal.md`. The full
back-catalogue of completed phases (Wolf3D Phase 1, V3 fn-type
checking, GPU pipeline arc Phases 4–7) is mirrored in commit
history.

## Status — 2026-05-09

Version 0.23x. Self-hosting, dual-backend (ARM64 native macOS +
x86_64 cross-compile Linux), C emitter in parity. Suite at
**142 passed / 0 failed / 12 skipped** (native) +
**115 passed / 0 failed / 39 skipped** (C). Bootstrap byte-stable
from either repo-root or `tests/` cwd (runner pinned to REPO_ROOT
since `c8c09e8`).

**Recent landings:**
- 2026-05-07 — GPU pipeline arc CLOSED (Phases 4-7): pixel-perfect
  default, parallax, 4 post-process effects, `@profile` scoped zones.
- 2026-05-08 — Phase 6.3 v2: scoped-cleanup epilogues for `@profile`
  (early return + panic no longer leak open zones).
- 2026-05-09 — fxlab arc: Sessão 1 (substrate) + Sessão 2 (GUI) shipped.
  JSON-driven effects + file_watch hot-reload + standalone tuner with
  preset list + auto-generated sliders + save-on-release JSON write-back.
- 2026-05-10 — fxlab Sessão 3 + polish: Bang 9 `_panel_fx` + 4 bug
  fixes (runner entry resolution, project-sacred read-only resolver,
  dual-watch on fallback, fps_wolf3d game_frame_begin + drop per-frame
  effect_set). Hot-reload visually validated end-to-end: drag slider
  in Bang 9 → fps_wolf3d responds in ~30ms. **Arc CLOSED.**
- 2026-05-10 — Layout cleanup: `stb/effects/` → `effects/` (single
  root). `stb/` is engine-internal only (`.bsm` + `.metal`). `effects/`
  is the user namespace, mirrored across engine source / install /
  user project.
- 2026-05-11 — Wolf3D Phase 2 Session 0 (Caminho A): level_editor
  ganha entity layer (Tiles ↔ Objects toggle, kind picker, marker
  overlay), JSON schema v2 (`tiles[][]` + `entities[]`), pathfind
  level migrado in-place, fps_wolf3d carrega level via JSON loader
  (replaces hardcoded ASCII maze), `bang9_space_manual.md` canoniza
  Bang 9 como engine/IDE da bangdev. Drop `.level.json` double-suffix
  (now `.json` matching sprites/effects). bang9 artifacts cleaned
  (`level1.level.json` deleted, `modulab_prefs.json` `.gitignore`d).

**In flight:** content arc decision point — Wolf3D Phase 2 / Adventure /
RPG / RTS. Engine no longer the bottleneck.

**Sidequest open:** custom shader override (`<project>/shaders/*.metal`
with project-first / install-fallback resolution in
`gpu_pipeline_load`, ~10 LOC stbshader). Ships when a user game wants
a non-stb shader. Today `stb/shaders/` stays engine-internal.

---

## The 1.0 path (what's left)

```
 ✅  0.22 — Foundation + multi-error (Dev Loop 1)
 ✅  0.23 — Language syntax complete, smart dispatch, Tonify v1+v2
 ✅  Wolf3D Phase 1 — fps_3d.bpp walks at 60 FPS (CPU raycaster)
 ✅  V3 — function-pointer type checking
 ✅  GPU pipeline arc — Phases 4-7 (pixel-perfect → effects → @profile)
 →  Content arc — pick one: Wolf3D Phase 2 / Adventure / RPG / RTS
 →  Dev Loop 4 — hot reload watch mode
 →  Dev Loop 5 — metaprogramming ($T + reflection)
 →  bangscript runtime — .bang format + engine
 →  Adventure Puzzle demo → 1.0
```

Order of the next four content milestones is the player's call. The
engine doesn't gate any of them.

---

## Active — content arc decision point

Engine is no longer the bottleneck. Pick one to drive the next
sprint:

### A. Wolf3D Phase 2 (~7 sessions)

Sprites + enemies + audio + doors + level files. Uses Phase 3
sprite batching + Phase 6 CRT. Procedural wall textures from
Phase 6.4 stay in place; PNG-loaded artwork can swap in via
`image_load(...).tex` without touching the shader.

Reference: `games/fps/HANDOFF.md` (last updated for Phase 1
close-out), `docs/games_roadtrip.md` Game 2.

### B. Adventure Puzzle demo (~10 sessions)

Thimbleweed-flavored. Uses Phase 5 parallax + Phase 6 effects
(watercolor, vignette). Bangscript runtime (`stbbangs.bsm`) is
the foundation — see Section "bangscript runtime" below.

Reference: `docs/bangscript_plan.md`, `docs/games_roadtrip.md`
Game 5.

### C. RPG Dungeon (~2-3 weeks)

Four new modules reusable by RTS and Adventure: `stbdialogue`,
`stbinventory`, `stbmenu`, `stbsave`. The four UI modules are
the value; gameplay is intentionally simple.

Reference: `docs/games_roadtrip.md` Game 4.

### D. RTS demo (~2-4 months for vertical slice)

Sprite batching + zoom pixel-perfect + reactive AI. New module:
`stbanim` (~200 LOC). Everything else reuses existing
infrastructure.

Reference: `docs/games_roadtrip.md` Game 3.

---

## Active — Excalibur Arc (strategic language polish)

**Opened 2026-05-11.** Multi-session arc that lifts B++ into the
modern game-dev language tier (Jai / Zig / Odin convergence) while
preserving B++'s identity: storage class system, Tonify discipline,
retro feel. Runs in parallel with the content arc; can be paused
at any session boundary for Wolf3D / RPG / etc. work.

**Canonical spec**: [`docs/excalibur_arc.md`](excalibur_arc.md) —
read this first before any session here.

Four features in sequence:

1. **Polymorphic numeric literals** — literals carry shape until
   slot context fixes the type. Kills `feedback_const_float_demotes`
   bug class. ~150 LOC compiler + 80 tests across 3 sessions.

2. **Cast builtins** — `floor_int` / `trunc_int` / `round_int`
   replace typed-local thunks and ad-hoc `(long long)` casts.
   ~50 LOC + 30 tests + -100 cleanup across 1 session.

3. **Struct newtype** — opt-in `struct WorldPos as Vec2` with
   compiler-enforced identity. World vs Grid mixing becomes
   compile error. ~250 LOC + 80 tests across 3 sessions.

4. **Templates / metaprogramming** — generic procedures with
   compile-time substitution. Absorbs the "Metaprogramming"
   entry below into the Excalibur frame. ~1500-2500 LOC across
   5-8 sessions. Triggers when Features 1-3 close + 2 real
   consumers ask (RTS metaprog + Adventure DSL + Excalibur 3
   newtype operations).

**FFI compatibility**: verified — none of the 4 features break
extern surface. The orthogonal "FFI auto-marshalling" sidequest
remains independent.

**Current state**: Session 1.A (literal shape in lexer/parser)
is next. ~50 LOC, pause-safe, no runtime behavior change.

**Pause discipline**: each session ends with bootstrap byte-stable
+ suite green + Status section in `docs/excalibur_arc.md` updated
with `next: <session ID>` marker. Wolf3D / other work can resume
between any two sessions without conflict.

---

## Queued — RTS Stress Arc (runtime scaling)

**Opened 2026-05-12 (planning).** Five-session arc that scales
B++'s existing ECS / profile / multicore / SIMD infrastructure
from demo scope (2 unit types per `games_roadtrip.md` Game 3) to
**Red Alert scale stress test** (30+ unit types, 1000+ live
units, 60fps target). User named the target "B++ cantando" —
the proof that B++ carries a real RTS workload, not just a
vertical-slice demo.

**Canonical spec**: [`docs/rts_stress_arc.md`](rts_stress_arc.md)
— read this first.

Five sessions:

1. **ECS query + iterator** (~120 LOC) — replace manual
   `for i, if alive[i]` with `ecs_query_each(query, fn_ptr)`.
   Foundation for sessions 2-5.

2. **Archetype storage** (~400 LOC) — entities with same
   component set live in 16KB contiguous chunks. Cache locality
   3-5x speedup at 1000+ entities. Major refactor with backward-
   compat shim for existing parallel-array consumers.

3. **SIMD batching inside systems** (~120 LOC, up to ~250 if
   `vec_*` builtins need extension) — `ecs_physics` and similar
   use `vec_*` on 4-entity batches. Another 2-4x on top of
   Session 2.

4. **Flow fields pathfinding** (~250 LOC) — new
   `stb/stbflow.bsm`. One compute per goal, all units sample.
   Replaces per-unit A* for many-units-to-same-objective.

5. **System scheduler** (~120 LOC) — register systems,
   auto-parallelize independent ones via maestro pool. Ties
   query + archetype + multicore together.

**Optional Session 6**: stress demo (`games/rts_stress/`),
~300 LOC game code proving 1000 units / 60fps. Ships separately
after infra arc closes.

**Total**: ~1010 LOC infra + ~300 LOC optional demo. Five-six
sessions. Net deliverable: B++ at AAA RTS scale.

**Anti-features**: templates (graduated out of Excalibur per
Rule 28), GPU compute for unit simulation (industry uses
CPU for game logic; GPU for rendering stays as-is), lockstep
fixed-point determinism (post-1.0 with multiplayer arc).

**Industry context audited 2026-05-12**: C&C Red Alert /
Warcraft / StarCraft all scaled to AAA via OOP+vtables (no
templates). Modern AAA (Unity DOTS, Bevy) use archetype ECS.
GPU compute used in industry for particles/cloth/fluid, NOT
for unit AI / strategy logic (branching + sync overhead beats
parallelism win).

**Triggers — when to open**:
- Hard prerequisite: Excalibur Features 1+2+3 closed
- Soft prerequisite (one of):
  - Wolf3D Phase 2 demo level shipping or near-shipping
  - User explicit "let's start stress test"

**Pause discipline**: each session ends with bootstrap byte-stable
+ suite green + `docs/rts_stress_arc.md` Status section updated
with `next: <session ID>` marker. Excalibur / Wolf3D / other
work can resume between any two sessions.

**Current state**: doc landed. No code yet. Blocked on
Excalibur 1+2+3.

---

## Engine-side sidequests (not blocking content arc)

These tighten the engine but don't gate any content. Pick up
opportunistically.

### Phase annotation collapse — Multics → Unix simplification — CLOSED 2026-05-11

**SHIPPED 2026-05-11 — same day as the open.** Six commits across
the arc:

| Commit    | Scope                                                  |
|-----------|--------------------------------------------------------|
| `842212f` | Step 1: introduce `@safe` + W026 enforcement clause    |
| `60b1d8b` | Step 2a: stb cartridges (242 sites stripped, 23 files) |
| `66e6da1` | Step 2b: compiler internals + backend (302 sites, 33)  |
| `a17eff9` | Step 2c+2d: games / tools / examples / tests (130, 31) |
| `7e528d4` | Step 3+4: parser strict-mode + docs rewrite (27 sites) |
| `e1b16a1` | Cleanup: dead enforcement strip, E260, lattice rewrite |

**Net result:** 700+ redundant annotation sites removed across
95 files. ~80 lines of dead enforcement stripped from
`bpp_dispatch.bsm`. Single user-facing annotation `@safe` (plus
`@profile` for scoped instrumentation, plus default no-annotation
for everything else). E260 fires with a focused migration hint when
any of the eight deprecated keywords appears in source. Bootstrap
byte-stable in every commit. Native suite 148/0/12, C suite
121/0/39 throughout.

Spec + post-mortem now in `tonify_checklist.md` Rule 4 (current
model) + Rule 28 (meta-lesson) + `journal.md` (2026-05-11
"Multics-drift post-mortem" + arc closure summary).

**Original briefing kept below for historical reference.**

---

### Original briefing (2026-05-11, pre-execution)

Strategic simplification of the 8-phase annotation system to a
binary user-facing model. Honors B++'s "tudo é word" philosophy
(Unix-style one abstraction does many uses, unlike Multics-style
specialized abstractions).

**The diagnosis.** The current system has 8 phase tags
(`@base`, `@solo`, `@time`, `@io`, `@gpu`, `@heap`, `@panic`,
`@realtime`) in a flat namespace, mixing 5 orthogonal dimensions
(time, space/resource, purity, control flow, instrumentation).
Compiler internal logic already only consults BASE/SOLO + TIME
for actual decisions — the rest is decoration that propagates
through the lattice without driving enforcement. Per the
diagnostic comment at `src/bpp_dispatch.bsm:156`: phase hints
beyond BASE/SOLO are "inert tags ignored by W013 and dispatch
logic". Codebase has accumulated ~500 redundant phase
annotations as cultural drift.

**The proposed model.** Two user-facing annotations replace the
8-phase lattice:

- **`@safe`** — bounded time + no malloc + no IO/GPU/syscall +
  worker-thread-safe. Covers the killer use cases (audio
  callback verification, multi-core worker safety). Compiler
  enforces. Replaces `@time` semantically and pins what `@base`
  was used for in worker contexts.
- **`@profile("name")`** — scoped instrumentation (Phase 6.3).
  Different category from effects; kept intact.
- **default** (no annotation) — full power, no compiler
  verification. Programmer is on their own.

Internal `fn_effect` array (already in `bpp_dispatch.bsm`) keeps
fine-grained effect tracking for optimization decisions
(inlining, parallelization candidates). The user-facing namespace
shrinks; compiler internal capability unchanged.

**Tradeoffs audited (2026-05-11)**:
- ✅ Zero performance loss — effect inference stays internal
- ✅ Zero safety capability loss — audio + worker safety preserved
- ✅ Zero optimization opportunity loss — `fn_effect` keeps tracking
- 🟡 Reporting granularity reduces (errors say "unsafe context"
  instead of "expected @io got @gpu") — cosmetic
- 🟡 Annotation-as-documentation lost — mitigate with code comments

**Net ship**:
- Compiler: add `@safe` annotation + back-compat aliases for old
  phases that map to it (`@time` → `@safe`, `@base` →
  inferred). ~30 LOC compiler.
- Tonify Rule 4: rewrite to reflect 2-state model. ~50 LOC docs.
- Migration: grep + replace across codebase, remove redundant
  annotations. -500 chars verbosity. ~2-3 hours focused work.
- Tests: confirm audio callback verification still fires on
  malloc, worker-safety inference still rejects shared-mutation
  paths. ~30 LOC tests.

**Total: ~3-4 hours focused work + bootstrap byte-stable in each
phase. Net codebase change: subtraction.**

**Step 0 — audit before commit**: 30 min grep audit in
`src/bpp_dispatch.bsm` and `src/bpp_codegen.bsm` to confirm
codegen doesn't depend on @io/@gpu/@heap distinctions for
decisions beyond error reporting. If audit reveals a real
codegen consumer of those phases, revise scope. Otherwise
proceed with full collapse.

**Trigger**: after Excalibur Feature 1 closes. Can interleave
between Excalibur sessions if user wants the simplification
faster (no codegen conflict with literal shape work).

**Reference**: conversation 2026-05-11 with user — established
the Multics-vs-Unix framing. Honors `feedback_cartridge_minimalism.md`
spirit ("ship floor only, complexity is opt-in").

### Positive `@safe` suggestion engine — CLOSED 2026-05-11 (commit 4f95703)

**SHIPPED** end-to-end same-day as the open. W031 fires when
`fn_ptr(target)` flows into `maestro_register_base` or `job_submit`
without `target@safe`. Diagnostic points at target signature with
3 help lines + `see: docs/tonify_checklist.md Rule 4` anchor.
Pipeline validated by annotating `particle_base_job` in
`games/snake/particles.bsm` — W031 silenced, W026 stayed silent
(contract genuinely satisfied). 5 other production sites surfaced
by W031, kept un-annotated as nudges for the next-touch agent
(restraint-bias per Rule 28). Suite 149/0/12 + 122/0/39, bootstrap
byte-stable. Briefing kept below for historical reference.

---

**Opened 2026-05-11** as the Task 4 followup to the Phase annotation
collapse arc.

**The gap.** The collapse made `@safe` the only user-facing phase
annotation, and E260 catches programmers who write a deprecated
keyword. But there is no positive diagnostic: a programmer who
SHOULD annotate `@safe` (because they're registering a worker
thread or an audio callback) gets no compiler nudge if they forget.
The annotation becomes "the discipline you have to remember" —
exactly the failure mode Rule 28 warns about.

**Killer use case (Rule 28 gate)** — two specific bug classes that
nothing today catches:

1. Worker thread registered via `maestro_register_base(fn_ptr(name))`
   without `name` being annotated `@safe` → race condition latent
   until the worker hits an unprotected global write under load.
2. Audio callback registered via `AudioQueueNewOutput(..., fn_ptr(cb),
   ...)` (or the Linux ALSA equivalent) without `cb` being annotated
   `@safe` → audible glitch latent until the callback hits malloc /
   syscall under buffer pressure.

Both bugs are invisible at compile-time today and surface only at
runtime under stress. The compiler already knows the context: it
sees the `fn_ptr(name)` literal flow into the registration builtin.
A diagnostic at the registration site closes the loop.

**Smaller-tool test** — could docs alone solve this? No. Programmers
forget. Compiler suggestion is the right tooling here.

**Restraint-bias test** — emit as a WARNING (not error). Programmer
can ignore for one-shot scripts or experiments. Match B++'s
"programmer freedom" stance.

Passes Rule 28 gates: specific bug classes, smallest tool that
catches them, restraint-biased emit (warning not error).

**Scope.** ~1-2 hours. Instrument `add_extern_effect` (or build a
dedicated table) marking specific builtin names as
"callback-registration sinks". When the call-graph walker sees
`fn_ptr(name)` flowing into one of those sinks, look up
`fn_phase_hint[name]` — if not `PHASE_SAFE`, emit W-new with text:

```
warning[Wxxx]: 'name' registered as <kind> callback without @safe
   --> file.bpp:N
   N | maestro_register_base(fn_ptr(name));
     |                       ^^^^^^^^^^^^
   = note: <kind> callbacks must satisfy bounded-time + no-malloc
           contract; @safe gives the compiler a chance to verify
   = help: annotate `name(...)@safe { ... }` to enable W026
   = see:  docs/tonify_checklist.md Rule 4
```

Initial sink list: `maestro_register_base`, `maestro_register_solo`
(the latter to suggest dropping the registration if the function
is pure-enough), `job_register`, plus the audio FFI builtins via
the existing `add_extern_effect` slot.

**Trigger**: first time a real bug from this class hits us in
production, OR opportunistically before that to close the
discipline-as-tooling gap.

**Verification**: bootstrap byte-stable; 2 new tests
(`test_safe_suggestion_worker.bpp`, `test_safe_suggestion_audio.bpp`)
pin the warning fires for each killer use case; existing suite
green.

**Closes**: the open question raised in `e1b16a1` commit message
("falta diagnóstico positivo").

### Multi-core completion — Sprints A-G

Full spec: [`docs/multicore_state_report.md`](multicore_state_report.md).

Sprint 1 + Sprint 2a/2b (`+`) shipped 2026-04-30; ~1010 LOC
remain to close the 8 gaps end-to-end. In priority order:

- **Sprint A** — stride / start rewrite (~40 LOC). Today the
  scanner detects `stride != 1` and `start != 0` but the
  rewriter bails out to serial. A chunked `job_parallel_for`
  variant + matching dispatch path closes both at once.
  Unblocks SIMD-aware pixel loops and partial-iteration
  parallel maps.
- **Sprint B** — non-additive reductions (~30 LOC). Identity-
  element table for `*`, `min`, `max`, `&`, `|`, `^`. Scanner
  + rewrite already exist for `+`; this just extends the
  identity / merge tables.
- **Sprint C** — channels (~80 LOC). `chan_send / recv /
  try_send / try_recv / close` on top of the SPSC ring already
  proven in `bpp_job`. Unblocks audio→main thread events,
  worker progress reporting.
- **Sprint D** — atomic primitives minimum (~50 LOC).
  `atomic_load`, `atomic_store`, `atomic_fetch_add`. Per-chip
  primitive (a64 + x64), exposed as builtin. Unblocks
  Naughty-Dog-style job dependency counters.
- **Sprint E** — Linux thread parity (~500 LOC). Depends on
  ELF dynamic linking (separately tracked above). Single
  unblock = 8-32× parallelism on Linux hosts. **Highest
  capability priority** — until this lands, every Linux
  program is single-threaded regardless of dispatch.
- **Sprint F** — CPU feature detection (~300 LOC). `cpu_has`
  builtin + per-OS detection (sysctl on macOS, /proc/cpuinfo
  on Linux). Enables AVX2 / SVE fast-path dispatch.
- **Sprint G** — map / reduce sugar APIs (~50 LOC). Pure
  ergonomic wrappers (`arr_map(in, n, fn, out)`,
  `arr_reduce(in, n, op, init)`) — capability already exists
  via Sprints A+B.

Recommended ordering: A → B → D → C → (ELF dynlink) → E → F →
G. Sprints A-D land as foundation work between game features;
E waits on the dynlink prerequisite; F+G land last.

### Tier F compiler optimizations — gating still applies

Full spec: [`docs/compiler_boost_roadmap.md`](compiler_boost_roadmap.md).

Three multi-node optimization passes (CSE, RegAlloc v2, Auto-
vectorization) that are intentionally DEFERRED until profile
data justifies them. Doc gating:

> **Do not open Tier F until:**
> 1. A real plugin exists, runs, and is profiled.
> 2. The profile shows one of: CSE waste, register spill /
>    reload time, scalar-inner-loop dominance.
> 3. Someone owns the resulting project end-to-end.

Status today (2026-05-07): none of the three conditions have
fired. Single-node opts (Phase D-style strength reduction,
inline trivials) continue to land opportunistically and capture
the wins that don't need profile evidence. Tier F sits as a
shovel-ready 2-3-week project per pass when a real consumer
flags the bottleneck.

### Linux Vulkan parity for the GPU pipeline

`stb/_stb_gpu_linux_vulkan.bsm` (new). Mirrors the macOS Metal
layer: device + command queue + render pipeline + texture upload
+ vertex buffer. ~1500 LOC of dedicated work. Unlocks GPU games
on Linux end-to-end. Doesn't block any phase — Phase 4–6 ship on
macOS today, Linux remains "cross-compiles but doesn't run GPU"
until parity arrives.

### `effect_palette_quantize` — the 5th effect

Deferred from Phase 6.2. Needs real stbpal integration (palette
uploaded as a 1D texture or constant array, per-pixel
nearest-colour search in the fragment shader). ~100 LOC. Ships
when an actual game asks for indexed-palette post-process.

### `fxlab` tool (`tools/fxlab/`) — ARC CLOSED 2026-05-10

Renamed from the original `fx_playground` proposal. Same intent:
preset tuner that loads effect manifests, exposes uniform sliders,
hot-reloads to running games via file_watch. V1 shipped JSON
manifests + slider GUI standalone + Bang 9 panel; live `.metal`
editor (Modelo 4 of the original design discussion) deferred
until real signal emerges from the V1 tuner in use.

**Driver:** Wolf3D Phase 2 needs to calibrate 5+ effects × 3-4
params each. Manual edit-build-run loop is insane at that volume.

**Plan:** `~/.claude/plans/groovy-munching-seahorse.md`. 3 sessions,
all shipped:

- Sessão 1 — substrate. JSON manifests +
  `effect_from_json` + `effect_set` + file_watch wiring; 4 effects
  migrated; visually validated end-to-end via `fps_3d_gpu` and
  `fx_library_smoke`.
- Sessão 2 — fxlab GUI standalone (~470 LOC `fxlab_lib.bsm` + ~30
  LOC entry). Preset sidebar + auto-generated sliders per param +
  save-on-release JSON write-back. **Pure JSON editor — does not
  load shaders itself**, the running game in another process
  holds the FxEffect handle and reacts via file_watch_tick.
- Sessão 3 — Bang 9 `_panel_fx` between Levels and Code (~25 LOC
  panel + tab strip update + embed contract refinement: lifted
  `init_ui` / `theme_dark` out of `fxlab_lib_init` to match the
  modulab / level_editor convention so the host owns UI subsystem
  boot + palette).

V2+ candidates if signal emerges from Wolf3D tuning:
- Auto-discover presets from `stb/effects/*.json` (V1 hardcodes
  the 4 in `fxlab_lib_init`; needs an OS-portable `dir_list` that
  doesn't shell out — graduate from `bang9/dir.bsm`).
- Live `.metal` editor (Modelo 4 from original design).
- Effect-chain ordering UI (drag-reorder which effect runs first).

### Modulab prefs path migration to user config dir — SHIPPED 2026-05-11

Closed in the same Wolf3D Phase 2 Session 0 batch where it
surfaced. `_stb_user_config_dir(app_name)` lives in both OS
backends now (`_core_macos.bsm` → `~/Library/Application Support/bpp/`,
`_core_linux.bsm` → `$XDG_CONFIG_HOME/bpp/` with `~/.config/bpp/`
fallback). Modulab's `_prefs_path()` calls it; binary can now live
anywhere on disk and the pref file follows the user, not the
install. Falls back to legacy `<exe_dir>` behavior only if HOME is
unset (extremely unusual).

### x86_64 perf parity on Linux — B1 freelist + B2 inline

ARM64 has the full Mini Cooper Phase B stack. x86_64 carries
only B3 on Linux: B1 + B2 were reverted after a bisect-confirmed
regression.

**Unblock path:**
1. Ship `bug` Linux/x86_64 support (gdbserver backend) — first.
2. Single-step through the self-host crash with the debugger.
3. Restore B1 x64 changes one hunk at a time with
   instruction-byte diffs against ARM64's emission.
4. Re-enable B2 inline.

### C-emitter `var T` parity

Native and C disagree on the storage location of `var s: Pt`
locals. Native allocates a stack slot and `&s` is FP-relative.
C lowers `var s` to `long long s[N]` (array decay-as-pointer),
and `&s` had to be special-cased in the T_ADDR emitter.

Forward path (when a real consumer hits this — currently no
game needs it):

1. Pick one storage strategy for `var T` and document it across
   both backends. Most likely "always heap-backed" — uniform,
   predictable, and `var p: Pt` becomes literal sugar for
   `auto p: Pt; p = malloc(sizeof(Pt));` with a matching
   `free` at scope exit.
2. Update native `_a64_emit_addr_local` and the C T_VAR/T_ADDR
   emit paths to share the same address calculation rule.
3. Add a regression test that takes `&local_struct`, passes it
   to another function, and reads a field both directions.

### `&struct.field` returns wrong address (compiler bug)

Confirmed 2026-05-11 with this 5-LOC repro:

```bpp
struct V { a, b, c }
main() {
    auto p: V, pa, pc;
    p = malloc(24);
    pa = &p.a;
    pc = &p.c;
    putstr("&p.a="); putnum(pa);
    putstr(" &p.c="); putnum(pc);
    putstr(" diff="); putnum(pc - pa);
    putline();
}
```

Expected: `diff = 16` (a at offset 0, c at offset 16). Actual:
both `&p.a` and `&p.c` return the struct base pointer
(`diff = 0`). With float-typed fields the same operator returns
a non-zero offset that ALSO doesn't match the field's actual
position — both behaviours are wrong, just differently wrong.

Fix: codegen for `&` on a struct field expression must add the
field's byte offset (already known via `get_field_offset`) to
the base pointer, NOT short-circuit to the base or to a
constant.

Same family as `feedback_sliced_struct_access.md` (raw offset
arithmetic vs typed access). Caught during the Vec2 promotion
audit when investigating whether `&_ai_scratch.vel_x` could
substitute for offset arithmetic in stbai callers — it cannot
today because of this bug.

Defer until: a real consumer needs `&` on struct fields. Today
the workaround is "compute the offset by hand" or "use typed
access (`s.field = ...`) instead of `&s.field`". Both are
ergonomic enough that the bug doesn't block any active arc.

### `bug` headless detection

`bug file.bug` (no `--dump`) routes to the GUI viewer. In a
headless environment (CI, piped invocation, no DISPLAY/
WindowServer), Cocoa opens the window and waits for the event
loop with no human to drive it. Today the only signal is a
one-line stderr hint at GUI launch. When stdin/stdout is
verifiably not a TTY, auto-fallback to `--dump` instead. Needs
`isatty(0)` — `sys_ioctl(0, TIOCGETA, ...)` or
`sys_fstat(0)` checking `S_IFCHR`. ~30 LOC.

Defer until: someone files an actual issue or runs `bug` in CI.

### `bug` Phase 7 — game-debugger evolutions

Captured 2026-05-11 during Wolf3D Phase 2 Session 2 debugging.
Each item is one capability that would have cut the sprite-pack
investigation from hours to minutes. Pick them up after content
arcs hit the same friction; none block Phase 2.

- **Trace mode (no-pause log)** — `--trace <fn>` prints entry +
  args without stopping the target. Critical for 60 Hz games:
  function-entry breakpoints fire so often that stop-the-world
  starves the maestro and the window never opens. The user can
  observe AI/render call counts alongside the running game. The
  observe-mode `O`-packet decoder already handles tracee stdout;
  this just adds a "continue immediately after logging" path on
  the host side. ~100 LOC.

- **`name:line` breakpoints** — break at a specific source line,
  not function entry. Source-position scaffolding exists in
  `.bug` (the per-function source-line map). Failing today on a
  source-map index off-by-N. Without it, every break hits at
  function ENTRY where locals are uninitialised stack garbage.
  ~80 LOC.

- **Conditional break** — `--break <fn> if <expr>`. Skip 999
  hits of the same callsite when you only care about the one
  where a local crossed a threshold. Reuses the existing
  expression evaluator. ~60 LOC.

- **Memory hexdump** — `d <addr-or-name> <bytes>` in the REPL.
  Inspect a GPU uniform buffer, an ECS payload slot, or any
  raw region at the moment of a break. ~40 LOC.

- **Snapshot / replay** — capture full process state at a break
  and replay deterministically. "What did the world look like
  10 frames ago when the enemy got stuck?" Largest item, only
  build once physics / multi-frame bugs become routine. Defer.

Estimated total for items 1-4: ~280 LOC + REPL plumbing.
Open this as a dedicated arc when the next game-debug session
hits the same walls.

### ELF dynamic linking

B++ x86_64 emits only static ELF today. Adding PLT/GOT,
`DT_NEEDED`, `PT_INTERP`, `dlopen`/`dlsym` builtins unlocks:
- Vulkan on Linux (libvulkan via dlopen)
- ALSA for stbaudio Linux backend
- Any FFI module on Linux (libpng, sqlite, curl)

~2-3 days of focused work. Snake maestro and Rhythm Teacher on
Linux both wait on this.

### Host-aware compiler + install

Today `bpp` defaults to macOS arm64 and cross-compiles via
`--linux64`. Vision: detect host OS+chip at startup and default
to that target. On Linux x86_64, `bpp game.bpp` produces a
Linux ELF with no flag. `install.sh` mirrors it.
Cross-compilation stays available via `--target`.

### Refinements (each under 50 lines, land while touching the file)

- **`--stats` compiler flag** (~40 lines): module count, function/
  global/struct counts, peak tokens, wall-clock.
- **Arena-backed AST in the parser** (~30-50): swap per-node
  `malloc` for `arena_alloc(mod_arena, NODE_SZ)`. `stbarena`
  already exists — wiring only.
- **`mem_track` for games** (~50): `mem_alloc(size, tag)` records
  (tag, size) in a static array; `mem_report()` prints per-tag
  totals.
- **Function dedup polish**:
  - `main()` in `.bsm` = fatal error (E223). ~5 LOC.
  - `stub` keyword marks intentional override targets; dedup
    treats `stub → real` as always silent. ~30 LOC.

### Small quality items

- **FFI auto-marshalling** — compiler knows FFI param types but
  ignores them; fixing eliminates `(long long)` casts at call
  sites.
- **Struct field sugar for compiler internals** —
  `*(rec + FN_NAME)` → `rec.name` for compiler-internal structs.
  ~20 locations.
- **Multi-return values** — `return a, b;` + `x, y = foo();`.
  Eliminates the `float_ret` / `float_ret2` hack.
- **Retina support** — `contentsScale = 2.0` on CAMetalLayer.
  Render at 2× physical pixels. Trivial once there's a reason.
- **Test all games via the C emitter** — only `snake_gpu` and
  `snake_raylib` have been verified end-to-end through
  `bpp --c → gcc → run`.
- **bootstrap.c regeneration** — currently ARM64-only, needs
  regen with the current C emitter. Low priority until someone
  wants to bootstrap on a platform B++ doesn't run on yet.

---

## bangscript runtime (.bang format + engine)

Adventure game DSL. Full spec in `docs/bangscript_plan.md`.
**Zero compiler extensions.** `.bang` files are data parsed at
runtime; the engine is a pure B++ module.

Runtime modules (~980 lines):
- `stbbangs.bsm` (~700) — `.bang` parser + coroutine scheduler +
  command dispatcher + flag machine
- `stbverb.bsm` (~200) — SCUMM-style verb/interaction system
- `stbcursor.bsm` (~80) — cursor state machine

Could land as early as the Adventure Puzzle demo prep.

---

## Metaprogramming (Dev Loop 4) — ABSORBED INTO EXCALIBUR ARC

**Status 2026-05-11**: this work is now Feature 4 of the
Excalibur Arc — see [`docs/excalibur_arc.md`](excalibur_arc.md).
Same scope, same triggers (lands during RTS), now anchored
to the strategic language arc instead of standalone Dev Loop.

Original notes retained here for reference until Feature 4
opens dedicated planning:

- **`$T` generic parameters**: `swap($T, a, b) { auto tmp: T;
  ... }`. Compiler instantiates one AST copy per concrete type
  used. ~400 lines.
- **Compile-time struct reflection + code emission**:
  `const generate_serializers(T) { ... }` reads field metadata
  at compile time, emits `save_unit`, `save_building`, etc.
  Seed exists in `const FUNC`; the leap is exposing struct field
  metadata and `emit_function` that injects into the AST.
  ~1100-2000 lines.

Also the foundation for bangscript's `scene { }` block
rewriting.

When Feature 4 of the Excalibur Arc opens, this section
becomes the seed for the dedicated session decomposition.

---

## Deferred — tracked, revisit when demand arises

- **C emitter SIMD mapping (`: double` + `vec_*`)** — native
  backends (NEON, SSE2) emit `vec_*` as instructions. The C
  transpile path needs `: double` locals typed as `__m128` and
  the eleven builtins mapped to `_mm_*` intrinsics. Today the
  emitter uses `long long` for every local unconditionally —
  invasive change (~200-300 LOC). Zero demand from `bpp --c`
  today.
  - **Trigger to open the sidequest** (Rule 20 two-consumer):
    two real consumers must demand SIMD via the C-emit path. A
    bench file or single-purpose library does not count — needs
    a shipped game / lib + a portability target where only the
    C path is available. Until then, `vec_*` stays native-only
    opt-in and the stb cartridges that import `vec_*` are
    quarantined to native (the convention is documented in the
    `stb/stbecs.bsm` header — portability-tier note). C is
    already a two-tier language itself (SSE/AVX intrinsics are
    non-standard `<immintrin.h>` extensions), so B++ mirroring
    that split is alignment with industry practice, not a
    portability hole.
  - **Confirmed 2026-05-12 during RTS Stress Arc Session 3**:
    initial attempt put `vec_axpy_f32` inside stbecs.bsm, broke
    four pre-existing C-emit tests transitively. Refactor moved
    the helper out of the cartridge into the bench file that
    consumes it; cartridge restored to C-emit-clean. See
    `tests/bench_ecs_physics_simd.bpp` for the canonical pattern
    consumers should follow until this sidequest opens.
- **Retire `emit_load_runtime_global` chip primitive** — after the
  2026-05-13 builtin → source migration (see
  `docs/builtin_to_source_plan.md`), `_a64_emit_load_runtime_global`
  and `_x64_emit_load_runtime_global` have zero call sites. The
  dispatch-table entries (`p.emit_load_runtime_global = fn_ptr(...)`)
  and the two primitive bodies (~10 LOC across both chip backends)
  are dead code. Delete in a focused cleanup commit; needs
  bootstrap + suite gate but no functional change. Trivial.

- **Return-type annotations** — RESOLVED 2026-05-13. End-to-end
  flow (parser → V3 inference via `fn_ret_hint` → validate +
  codegen) was always shipped, only the surface syntax had a
  wart: the original `fn(args): ret {` form used `:` for both
  "var has type" (storage) and "function returns" (output). The
  2026-05-13 cleanup migrated to `fn(args) -> ret {` (single
  parser change + 1 production consumer + 1 test). The two
  glyphs are now cleanly split: `:` = "X has type T" (storage),
  `->` = "function returns T" (output). Same convention as Rust
  / Python / Swift / C++11+ / TypeScript. First consumer:
  `path_asset(relpath: ptr) -> ptr`. Sweep is opt-in per call-
  site (Rule 13 + Rule 20). See Tonify Rule 13's "Return-type
  annotation" section for the canonical pattern + how_to_dev
  Cap 5.1 for the two-glyph convention.
- **W032 cleanup sweep across remaining tests** — ~57 sites still
  emit W032 because of intentional integer `put(x)` calls in test
  diagnostics (e.g. `put("count="); put(n);`). Each site swaps to
  `putnum(n)` per the explicit-intent rule. Mechanical edit; defer
  until either someone adds a `--strict-warnings` CI gate or the
  remaining tests get touched for unrelated reasons. The stb/ stb-
  cartridge sites + stbmidi tests are already swept clean.
- **`restrict` keyword** — useful only when the native codegen
  does aliasing-aware optimization. Not part of Mini Cooper.
  Revisit post-1.0.
- **`offsetof` / field reflection** — P1 for bangscript runtime.
  Exposes compile-time field offsets as runtime constants;
  enables auto-generated serializers. Separate task.
- **Prefetch builtins (`__builtin_prefetch`)** — add when real
  profiling on RTS-sized workloads shows the win.
- **Full aliasing-aware optimizer** — post-1.0. Not in scope.

---

## Game modules (pure B++, no platform deps)

| Module | Status | Notes |
|---|---|---|
| `stbtile.bsm` | ✅ | Tilemap engine |
| `stbphys.bsm` | ✅ | Physics with milli-units + FPSBody chapter |
| `stbpath.bsm` | ✅ | A* pathfinding |
| `stbtexture.bsm` | ✅ | Procedural textures (brick / stone / wood / solid) |
| `stbraycast.bsm` | ✅ | DDA + RayHit + per-column projection |
| `stbprofile.bsm` | ✅ | Runtime profiler HUD + `@profile` zone aggregates |
| `stbscene.bsm` | ✅ | Layered backgrounds with parallax (Phase 5) |
| `stbshader.bsm` | ✅ | Custom pipelines + `gpu_target_*` (Phase 4.1.x) |
| `stbfx.bsm` | ✅ | Effect chain + 4 typed effects (Phase 6.1+6.2) |
| `bpp_hash.bsm` | ✅ | Hash maps (used by compiler) |
| `stbaudio.bsm` | ✅ | CoreAudio + SPSC ring |
| `stbdialogue.bsm` | P0 | RPG (dialogue boxes, choices) |
| `stbinventory.bsm` | P0 | RPG (item slots) |
| `stbmenu.bsm` | P0 | RPG (nested menus) |
| `stbsave.bsm` | P0 | RPG (save/load state) |
| `stbanim.bsm` | P0 | RTS (sprite animation) |
| `stbbangs.bsm` | P0 | bangscript (.bang parser + scheduler) |
| `stbverb.bsm` | P0 | bangscript (verb/interaction) |
| `stbcursor.bsm` | P0 | bangscript (cursor state machine) |
| `stbart.bsm` | post-1.0 | Pixel art primitives |
| `stbnet.bsm` | post-1.0 | TCP/UDP for multiplayer |

---

## Known open bugs

- **`call()` float args on x86_64** — `call(fp, args...)` ignores
  float arguments on x86_64. Doesn't affect native function
  calls, only the indirect `call()` builtin.
- **String dedup across modules** — duplicate strings across
  modules create duplicate entries in the data segment. Harmless
  but wasteful.
- **`bootstrap.c` is ARM64-only** — see refinements above.
- **`@profile` nested zones aggregate FLAT** — parent total_us
  includes children's time. Hierarchical breakdown deferred to
  v3 when a real consumer needs it. (v1 leak on early-return /
  panic CLOSED 2026-05-08 — scoped-cleanup epilogues shipped.)

---

## Post-1.0 (scaling from demo to real game)

The 1.0 demos prove B++ delivers each game category. Scaling
from "1 demo level" to "full shipping game" requires the
infrastructure below. Each item is a 2-6 week project that adds
capability without changing the language.

- **Parallel codegen per module** — use the maestro job pool to
  make codegen multi-threaded. Becomes worthwhile at 50K+ lines.
- **Asset streaming (`stbasset.bsm`)** — load/unload on demand,
  atlas packing, manifest, background loading thread. ~500-1000
  lines.
- **Memory budget system** — per-system allocation limits,
  warnings on overshoot, defrag for long sessions.
- **True live code reload** — preserve world state across
  recompile. Serialize → relink changed module → deserialize.
  Basically a second linker.
- **Multi-error recovery across modules** — current multi-error
  log is per-compilation. In a 200-module codebase, cross-module
  type errors need smarter recovery.
- **Profiler GUI (timeline view)** — Chrome-style timeline,
  per-frame breakdown, memory + GPU profilers. Likely a
  standalone `stbtool` that reads a binary trace file.
- **Networking (`stbnet.bsm`)** — TCP/UDP, state serialization,
  prediction/rollback. ~500-800 lines.
- **Target architecture refactor** — chip vs OS split. Trigger:
  first chip+OS combo that doesn't fit today's folders
  (Linux ARM, Intel macOS, Windows x86_64).
- **Vulkan GPU on Linux** — see "Linux Vulkan parity" sidequest
  above. Same scope.
- **Tools & editors** — sprite editor (ModuLab ✅), tilemap
  editor, level designer (level_editor ✅), particle editor.
  Thin shells over stb modules.
- **Audio tools (CLAP plugin support)** — `--plugin` flag,
  `export` keyword, DSP primitives.
- **Windows / WebAssembly** — new codegen backends. Big work.
- **EmacsOS distribution** — custom Linux distro: Emacs (EXWM)
  as DE, B++ games in X11 windows. Vision project.
