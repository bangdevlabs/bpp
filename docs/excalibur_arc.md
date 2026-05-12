# Excalibur Arc — B++ numeric type system v2

**Strategic language polish that positions B++ alongside the new
generation of game-dev languages (Jai, Zig, Odin) while preserving
B++'s own identity: storage class system, Tonify discipline, retro
feel.**

This document is the canonical anchor for a multi-session arc that
will run alongside the Wolf3D Phase 2 content arc. Either the user
or the Emacs agent should read this before any session here to
understand sequencing, dependencies, and pause/resume contracts.

The arc was named "Excalibur" by the user during the planning
discussion — the sword for the final boss. Naming aside, this is
the language-level polish that makes the rest of the roadmap
(Wolf3D Phase 2 polish, RPG, RTS, Adventure) substantially less
painful to author.

---

## Premise

B++ today (post-V3 fn-type checking, 2026-05-05) has:

- Storage class system (`auto` / `static` / `extrn`) with type hints
  via `: float` annotation
- V3 strict checking that emits E240 on float→int narrowing at
  call sites
- Default `auto` is TY_WORD (int slot); float requires explicit
  annotation
- Numeric literals are typed at parse time: `0.5` → TY_FLOAT,
  `42` → TY_WORD
- No newtype / branded types — domain separation lives in convention
  (Vec2 for world, future Vec2i for grid)
- No templates / generics — pending in the metaprogramming roadmap

This arc closes four gaps that, together, lift B++ into the
"modern game-dev language" tier:

1. **Polymorphic numeric literals** — literals carry shape (decimal
   vs integer) until binding context fixes the type. Kills the
   "const NAME = 0.6 demotes to int 0" bug class.

2. **Cast builtins** — `floor_int` / `trunc_int` / `round_int`
   replace typed-local thunks and ad-hoc `(long long)` casts at FFI
   boundaries. Lossy conversions become explicit and grep-able.

3. **Struct newtype** — opt-in `struct WorldPos as Vec2` style
   declarations that give domain types compiler-enforced identity.
   World vs Grid coordinate mixing becomes a compile error, not a
   convention violation.

4. **Templates / metaprogramming** — generic procedures with
   compile-time type substitution. Already in `docs/todo.md` as
   "Metaprogramming"; this arc absorbs the planning and execution
   into the same strategic frame.

When all four ship, B++ has the numeric system Jai/Zig/Odin
converged on, with B++'s own sotaque (storage class system, Tonify
discipline, optional annotations).

---

## Industry context

Three modern game-dev / systems languages converged on similar
choices for numeric handling:

| Language | Implicit narrowing | Implicit widening | Literal polymorphism | Cast syntax |
|----------|--------------------|--------------------|----------------------|-------------|
| C        | silent             | silent             | suffix (`0.5f`)      | `(int)v` silent |
| C++ modern | warn/silent (depends on flags) | silent | suffix | `static_cast<int>(v)` verbose |
| Rust     | ❌ error           | ❌ verbose `as`    | suffix (`0.5_f32`)   | `v as i32` |
| Zig      | ❌ error           | ✅ safe only       | ✅ comptime int/float | `@as(i32, v)` + `@intFromFloat` etc. |
| Jai      | ❌ error           | safe only          | ✅                    | `xx v` short |
| Odin     | ❌ error           | ✅ constants only  | ✅ untyped const     | `i32(v)` |
| **B++ today** | ❌ V3 E240    | ✅ TY_WORD → TY_FLOAT | ❌ typed at parse | typed-local thunk |
| **B++ post-Excalibur** | ❌ E240 | ✅ safe widening | ✅ shape-driven | `floor_int(v)` etc. |

The convergence is **strict-but-ergonomic**: no silent
conversions, but short cast syntax and polymorphic literals to
avoid Rust-grade verbosity.

B++ today is closer to Rust on strictness (V3 catches narrowing)
but lacks Rust's escape hatches. Post-Excalibur, B++ joins the
new generation explicitly.

---

## Anti-features (explicit non-goals)

To keep the arc focused, here is what we explicitly will NOT
build:

- **Implicit narrowing (C-style)** — silent float→int truncation.
  V3 E240 stays as ERROR, not warning. The Mars Climate Orbiter
  was caused by this; never bringing it back.

- **Rust-grade verbose type annotations on every signature** — B++
  keeps annotations optional. Annotation appears only when the
  programmer wants to pin a type for clarity or performance.

- **Function overloading** — same name, different signatures.
  C++-style. Conflicts with B++'s smart dispatch (`put()` is the
  only allowed runtime-dispatch case, compile-time rewrite). Skip
  unless a real consumer demands.

- **Dimensional analysis (F# / Frink units)** — full unit tracking
  (Meters × Seconds = Meter-Seconds). Heavy machinery, marginal
  payoff for game dev. Skip.

- **Generic type variance / lifetime annotations (Rust borrow
  checker)** — out of scope. B++ memory is malloc + arena.

---

## Features in detail

### Feature 1 — Polymorphic numeric literals

**Why first**: foundational. Every other feature gets cleaner
because literals adapt to context. Mata classe inteira de bugs
(`feedback_const_float_demotes.md`, parts of
`feedback_auto_float_silent_int.md`).

**Design**:

- Numeric literals carry `INT_SHAPE` or `DECIMAL_SHAPE` at lex
  time, not concrete `TY_WORD` / `TY_FLOAT`.
- At type-resolution time, the slot context determines the type.
- `auto x: float = 0.5` → `0.5` binds as TY_FLOAT.
- `auto x: int = 0.5` → ERROR (lossy bind, decimal can't fit int
  without loss).
- `auto x: float = 42` → `42` binds as TY_FLOAT (int → float
  widening, safe).
- `const NAME = 0.6` → NAME carries DECIMAL_SHAPE; concrete type
  fixed at each use site.
- Bare `auto x = 0.5` (no annotation) → preserves current default
  (TY_WORD by default) — backward compat unless we choose to
  promote.

**Decision points**:

- Should bare `auto x = 0.5` warn / error / preserve default?
  - **Recommendation**: preserve default with diagnostic
    (analogous to today's `feedback_const_float_demotes` warning
    pattern). Force programmer to annotate when intent is float.

#### Session 1.A — Literal shape in lexer/parser

- **Files**: `src/bpp_lexer.bsm`, `src/bpp_parser.bsm`,
  `src/bpp_defs.bsm`
- **Scope**: tokens for numeric literals carry shape annotation.
  AST T_NUM nodes preserve shape into types pass.
- **LOC**: ~50
- **Pause-safety**: literals fall back to defaults at codegen.
  Functionally identical to before. Only the AST gets richer.
- **Verification**: parser tests confirm shape annotation appears
  on T_NUM. Bootstrap byte-stable. Suite 145/0/12.

#### Session 1.B — Resolution via slot context

- **Files**: `src/bpp_types.bsm` (resolution path),
  `src/bpp_codegen.bsm` (literal emit chooses right opcode
  per concrete type)
- **Scope**: `add_type` reads slot context, materializes literal
  to slot's type if lossless. Errors on lossy
  (`auto x: int = 0.5`).
- **LOC**: ~80
- **Pause-safety**: behind feature flag during dev
  (`BPP_POLYMORPHIC_LITERALS=1`). Default off → identical to
  pre-arc behavior.
- **Verification**: 8 shapes of binding (typed slots × literal
  types × directions). Both flag states bootstrap byte-stable.

#### Session 1.C — Migration + cleanup

- **Files**: codebase string updates (locate and remove
  workarounds), `docs/tonify_checklist.md` (Rule 12 simplification),
  `MEMORY.md` index (graduate `feedback_const_float_demotes` to
  Resolved/Historical).
- **Scope**: feature flag default on. Search and remove existing
  `auto local: float; local = literal; ...` thunks that exist
  only because of the demotion bug.
- **LOC**: -50 (net subtraction)
- **Pause-safety**: feature on by default but isolated as one
  commit. Easy revert if needed.
- **Verification**: all games + smokes work visually. Bootstrap
  byte-stable. Suite 145/0/12.

**Feature 1 close marker**: commit with "Excalibur Feature 1
SHIPPED" message. Update this doc's Status section. Eligible to
pause for Wolf3D session here.

---

### Feature 2 — Cast builtins

**Why second**: ergonomic win + net subtraction in codebase. Low
risk, high cleanup payoff.

**Design**:

Three builtins added to `src/backend/chip/aarch64/a64_builtin.bsm`
and `src/backend/chip/x86_64/x64_builtin.bsm`:

```bpp
floor_int(x: float): int    // floor toward -inf, then int cast
trunc_int(x: float): int    // truncate decimal (toward 0)
round_int(x: float): int    // round to nearest even
```

Chip primitives (ARM64):
- `floor_int`: `FRINTM` (round toward -inf) + `FCVTZS`
- `trunc_int`: `FCVTZS` direct (truncation is default)
- `round_int`: `FRINTN` (round nearest even) + `FCVTZS`

x86_64 equivalent ops via SSE2 (cvttsd2si / cvtsd2si with rounding
control).

#### Session 2.A — Builtins + migration

- **Files**: `src/backend/chip/aarch64/a64_builtin.bsm`,
  `src/backend/chip/x86_64/x64_builtin.bsm`,
  `src/bpp_codegen.bsm` (dispatch),
  `src/bpp_types.bsm` (return type registration),
  `tests/test_floor_int.bpp`, `tests/test_trunc_int.bpp`,
  `tests/test_round_int.bpp`.
- **Scope**: builtins added, return type registered as TY_WORD,
  codegen lowering per chip. Migration: replace existing
  `auto i: int; ... = floor_f(...)` thunks with direct
  `floor_int(...)` calls.
- **LOC**: ~50 compiler + 30 tests + -100 migration subtraction
- **Pause-safety**: builtins are additive. Pre-migration code
  continues working with old patterns.
- **Verification**: edge cases (-0.5, 0.0, 0.5, NaN, large
  values). Both chip backends produce identical results for
  identical input. Bootstrap byte-stable.

**Feature 2 close marker**: commit with "Excalibur Feature 2
SHIPPED" message. Update Status section.

---

### Feature 3 — Struct newtype

**Why third**: needs Feature 1 (polymorphic literals bind to
newtypes naturally). Higher implementation complexity than 1 or 2.
Real consumer signal expected during Wolf3D Phase 2 (Vec2i for
grid) or later (RPG / RTS domain mixing).

**Design**:

Syntax (decision in Session 3.A):

```bpp
struct WorldPos as Vec2     // or another keyword form
struct GridPos  as Vec2i
struct Damage   as float    // newtype over primitive
```

Semantics:
- WorldPos and Vec2 are distinct types at the type-checker level
- Operations on newtypes delegate to base type
  (`worldpos1 + worldpos2` works, returns WorldPos)
- Cross-newtype operations error
  (`worldpos + gridpos` → ERROR, even though both wrap vec2)
- Conversion is explicit via user-written helper procs
  (`world_to_grid(w: WorldPos): GridPos { ... }`)
- FFI rule: newtypes auto-unwrap to base type at extern call
  boundaries (decision in Session 3.B)
- Codegen: zero-cost. Newtypes emit identical bytes to base type;
  identity exists only in the type checker.

#### Session 3.A — Parser + AST

- **Files**: `src/bpp_lexer.bsm` (keyword decision),
  `src/bpp_parser.bsm`, `src/bpp_defs.bsm` (new AST node type).
- **Scope**: declarations parse. AST carries identity. Type
  system still treats as base type (rigor lands in 3.B).
- **LOC**: ~80
- **Pause-safety**: declarations work but provide no enforcement.
  Codebase using newtypes still compiles as if base type.
- **Verification**: parser tests round-trip various newtype
  declarations. Bootstrap byte-stable.

#### Session 3.B — Type identity + dispatch + FFI rule

- **Files**: `src/bpp_types.bsm` (identity tracking,
  cross-newtype rejection), `src/bpp_codegen.bsm` (FFI unwrap),
  `tests/test_newtype_identity.bpp`,
  `tests/test_newtype_ffi.bpp`.
- **Scope**: type system rejects cross-newtype ops. Same-newtype
  ops delegate to base. FFI calls auto-unwrap newtypes to their
  base type.
- **LOC**: ~120
- **Pause-safety**: behind feature flag during dev.
- **Verification**: tests cover mixing rejection, delegation,
  conversion-helper pattern, FFI marshalling.

#### Session 3.C — Codegen zero-cost validation + docs

- **Files**: `tests/test_newtype_zerocost.bpp` (bytecode
  comparison), `docs/how_to_dev_b++.md` (newtype section),
  `docs/tonify_checklist.md` (new rule on newtype use).
- **Scope**: validate zero-cost claim. Newtype version of Vec2
  emits identical bytes to plain Vec2 version. Document the
  pattern and when to use it (domain typing: world vs grid,
  damage vs health, etc.).
- **LOC**: ~50 docs + tests
- **Verification**: bytecode diff between newtype and base type
  versions is empty. Bootstrap byte-stable.

**Feature 3 close marker**: commit with "Excalibur Feature 3
SHIPPED". Update Status section.

---

### Feature 4 — Templates / metaprogramming

**Why last**: highest risk. Foundational compiler change.
Benefits most from Features 1-3 already shipped (polymorphic
literals + cast builtins + newtypes all play together with
generics).

**Status**: already embryonically planned in `docs/todo.md` →
section "Metaprogramming (Dev Loop 5)". This arc absorbs that
planning into the strategic frame.

**Trigger**: at least 2 real consumers in the roadmap requesting
generics. Candidates:
- RTS metaprog (Dev Loop 5 per `docs/games_roadtrip.md`)
- Adventure DSL bangscript (per `docs/bangscript_plan.md`)
- Excalibur Feature 3 newtypes (if they grow demand for
  generic operations across newtypes)

When 2 of those are concrete (i.e. Wolf3D Phase 2 closed, RPG
arc starting, RTS in planning), open the dedicated planning
session to decompose Feature 4 into its sessions.

**Preliminary estimate** (not committed):
- 5-8 sessions
- ~1500-2500 LOC compiler
- Sessions cover: generic syntax `$T` parser, type substitution
  mechanism, per-call instantiation, codegen (one body, many
  concrete emissions), mutual recursion / forward refs, tests +
  macro pattern cleanup.

**Cross-reference — `bpp_defs` exposure to user code**: Feature 4
needs the compiler's vocabulary (AST node kinds T_*, type codes
TY_*/BASE_*/SL_*, the Excalibur SHAPE_* literal annotations,
struct layout queries) reachable from user programs at
compile-evaluation time. Bundle this exposure into Feature 4's
planning rather than fragmenting per-constant. Note discovered
2026-05-11 during Session 1.A test design — a user-side test
that tried `extrn SHAPE_INT` failed because bpp_defs is
compiler-internal today.

When Feature 4 opens, this section gets fully detailed.

**Feature 4 close marker**: TBD.

---

## Sequencing

**Hard dependencies**:
- Feature 2 has zero deps on Features 1/3/4
- Feature 3 strongly benefits from Feature 1 (polymorphic literals
  bind newtypes cleanly)
- Feature 4 benefits from all of 1/2/3

**Recommended order**:
```
1.A → 1.B → 1.C  ─── Feature 1 closed
2.A              ─── Feature 2 closed
3.A → 3.B → 3.C  ─── Feature 3 closed
[planning gate]
4.A → 4.B → ...  ─── Feature 4 (open when triggers fire)
```

**Parallel allowed**:
- Feature 2.A can ship between 1.B and 1.C without conflict
- Wolf3D Phase 2 sessions can run between any Excalibur sessions
- Bug Phase 7 (trace + REPL hexdump) can run between any Excalibur
  sessions

**Pause discipline**:
- Each Excalibur session ends with: commit, bootstrap byte-stable,
  suite green
- Status section in this doc updated with "next session" marker
- Working tree clean (no half-implemented features)
- Wolf3D / other work can resume / continue freely after pause

---

## FFI compatibility

**Verified by audit (2026-05-11)**: none of the 4 features break
existing FFI surface. Specifically:

| Feature | FFI impact |
|---------|------------|
| 1. Polymorphic literals | Zero. Literals at FFI call sites bind to declared C param types as today. |
| 2. Cast builtins | Improves. Replaces `(long long)x` cast tax with `floor_int(x)` etc. |
| 3. Struct newtype | Adds auto-unwrap rule at FFI boundaries (decision in Session 3.B). Convention. |
| 4. Templates | Zero. FFI signatures are concrete, not generic. |

The orthogonal "FFI auto-marshalling" sidequest in `docs/todo.md`
remains independent — eliminates the cast tax even further but is
not gated by Excalibur.

---

## Pause / resume convention

When the user signals "pausing Excalibur to do Wolf3D / other
work":

1. Current Excalibur session finishes cleanly (no half-done).
2. Bootstrap byte-stable + suite green confirmed.
3. Commit the session.
4. Update Status section below with `next: <session ID>` marker.
5. Optionally note any context the next agent needs (decisions
   pending, design questions, etc.).

When resuming:

1. Read this doc's Status section.
2. Find `next: ...` marker.
3. Read prior session's commit message + journal entry for
   context.
4. Proceed with the marked session.

The doc carries the state. No external tracking needed.

---

## Tonify rules emerging from this arc

Anticipated new rules (added at each Feature close):

- After Feature 1: Rule 28 (suggested) — "Numeric literals are
  shape-polymorphic; bind via slot context. Use annotation when
  intent matters, not as workaround." Graduates the
  `feedback_const_float_demotes` lesson to canonical rule.

- After Feature 2: Possibly Rule 29 — "Lossy numeric conversions
  use named builtins, never silent casts. Intent is grep-able."

- After Feature 3: Rule 30 — "Domain types via newtype when
  mixing causes real bugs. World vs Grid is the canonical case.
  Skip when single domain dominates."

- After Feature 4: Rule 31 — "Generics for primitive math
  helpers. Game code stays concrete."

Numbering will adjust based on what rules exist at the time
(today highest is Rule 27).

---

## Status

**Arc opened**: 2026-05-11

**Current state**: Features 1 + 2 + 3 SHIPPED 2026-05-12 (Feature
3 v1 — declarations + zero-cost layout; cross-newtype enforcement
deferred to a follow-up sidequest). Excalibur arc closed for now;
RTS Stress Arc (`docs/rts_stress_arc.md`) is the next thread.
Feature 1 closed in two passes: Sessions 1.A/1.B/1.C on
2026-05-11 covering literal shape + call-site widening, then
gaps A (E261 typed-local lossy assign) + B (`const NAME = 0.6`
preserves float bits via `ev_lit_src`) on 2026-05-12. Feature 2
shipped same-day as gap closure: floor_int / trunc_int /
round_int with chip lowering on ARM64 + x86_64 + C emitter, plus
migration of 18 floor_f sites across stb/games/tools. Feature 3
shipped v1 same-day: declarations parse, layout copies from base
(zero-cost), helper API exposed; cross-newtype enforcement
deferred to a follow-up sidequest.

**Next thread**: RTS Stress Arc (`docs/rts_stress_arc.md`).
Per the user directive, Phase 4 (templates) stays deferred — the
RTS arc moves engine-side first, templates re-evaluated when /if
specific consumers raise the trigger.

**Feature 1 — SHIPPED**:
- Session 1.A — SHIPPED 2026-05-11. T_LIT nodes carry
  `SHAPE_INT` / `SHAPE_DECIMAL` in `n.b`. Purely additive;
  no runtime behavior change.
- Session 1.B — SHIPPED 2026-05-11. Decimal int literals
  (`SHAPE_INT`, NOT hex) passed to `: float` parameters
  silently widen at validate time via in-place source rewrite
  (".0" appended to literal bytes in vbuf). Codegen sees a
  normal float literal and uses the existing path. Hex
  literals + typed int locals continue to error E240.
- Session 1.C — SHIPPED 2026-05-11 (docs-only). Tonify Rule 12
  extended to document the new polymorphic-literal rule and
  the residual gap (const-storage demote still needs a
  separate fix). `feedback_const_float_demotes` memory updated
  to note the partial fix. No code-side workarounds were
  removable in active source: the typed-float locals that
  exist (e.g. `_enemy_step` tunings in `games/fps/ai.bsm`)
  are workarounds for the unfixed const-storage demote, NOT
  for the call-site widening 1.B closed.

**Feature 1 final tally**: ~50 LOC parser annotations + ~70
LOC validate promotion + ~40 LOC const-float storage + E261
detection + tests + ~120 LOC docs. Suite 145/0/12 → 151/0/12
(+4 tests). Bootstrap byte-stable at every commit. Closing
commits: `714f20e` (1.A), `a2d5011` (1.B), `6a3072e` (1.C),
`4054a04` (gaps A + B).

**Feature 2 — SHIPPED**:
- ARM64: enc_frintm + enc_frintn encoders, _a64_emit_floor_to_int
  + _a64_emit_round_to_int primitives.
- x86_64: x64_enc_cvtsd2si + x64_enc_roundsd encoders (SSE4.1),
  _x64_emit_floor_to_int + _x64_emit_round_to_int primitives.
- ChipPrimitives gains emit_floor_to_int + emit_round_to_int slots.
- cg_builtin_dispatch handles "floor_int" / "trunc_int" / "round_int".
- bpp_validate.bsm `val_is_builtin` accepts the 3 names; bpp_dispatch
  registers them as pure_extern (safe to call from `@safe` contexts).
- C emitter lowers via <math.h>: floor() / cast / nearbyint(). LD_FLAGS
  in run_all_c.sh gains -lm.
- Migration: 18 floor_f sites across stbai/stbphys/stbraycast/stbdraw/
  games_fps_ai/fxlab_lib swept to direct floor_int calls. floor_f
  itself stays in bpp_math.bsm for the rare case where the float
  result is wanted. Suite 152/0/12 + 125/0/39, bootstrap byte-stable.

**Feature 3 — SHIPPED v1 (declarations + zero-cost layout)**:
- `struct WorldPos as Vec2;` syntax parses. Parser uses a new
  `as` keyword path inside `parse_struct`; the layout (fields,
  hints, byte/bit offsets, size, field count) is COPIED from the
  base into the newtype's struct registry entry so every existing
  layout-walker stays agnostic.
- `sd_newtype_base[idx]` parallel array tracks the base struct
  index (-1 for normal structs). `is_newtype(idx)` and
  `newtype_base(idx)` helpers are exposed for downstream
  consumers.
- `sizeof(newtype) == sizeof(base)`, dot access through inherited
  offsets, FFI auto-unwrap is automatic since the byte layout is
  identical (no wrapper code needed).
- `tests/test_newtype.bpp` pins size parity, field-offset
  matching, cross-type byte-aliasing.
- `tonify_checklist.md` Rule 29 documents when to reach for the
  pattern (domain typing without runtime cost) and when to keep
  the bare base.

**Feature 3 — DEFERRED to a follow-up sidequest**: cross-newtype
assignment rejection at `val_check_assign_compat`. Threading
struct identity through the type system (today TY_STRUCT is a
flat code without struct-id preservation) is a substantial type-
system surgery — the right time to do it is when the first real
cross-newtype bug surfaces in active code. Until then, the
declaration alone is high enough value to ship: programmers
reading the source see the distinction (WorldPos vs GridPos)
even if the compiler doesn't reject mixing.

**Feature 4**: deferred until triggers fire

**Pause history**: (will fill as Wolf3D sessions interleave)

---

## Why this is Excalibur

The arc was named by the user during planning — the sword for the
final boss. The final boss is not a single game; it's the cumulative
authoring weight of Wolf3D Phase 2 polish, RPG, RTS, and Adventure
shipping with confidence.

Each feature is a sharpening of the blade:

- Feature 1 kills silent numeric bugs at the literal level.
- Feature 2 makes lossy conversion intent visible in source.
- Feature 3 prevents domain-mixing bugs structurally.
- Feature 4 lets one helper serve many types without copy-paste.

When all four ship, every subsequent game built in B++ is authored
with sharper tools. The investment compounds across the rest of
the roadmap.

---

## Cross-references

- `docs/games_roadtrip.md` — overall 1.0 path; this arc supports
  every game past Wolf3D Phase 1.
- `docs/todo.md` — has FFI auto-marshalling (orthogonal) and
  Metaprogramming (absorbed as Feature 4).
- `docs/tonify_checklist.md` — receives new rules as features
  close.
- `docs/journal.md` — receives entry per session.
- `docs/how_to_dev_b++.md` — receives sections on new constructs
  as they land.
- `MEMORY.md` — graduates resolved feedback memories
  (`feedback_const_float_demotes` after Feature 1.C).
