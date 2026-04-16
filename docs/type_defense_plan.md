# Type Defense Ladder — Plan from the Jedi Master

> Authored in the Claude.ai web app and handed to the Claude Code (Emacs)
> agent for execution. This file is the executor's reference — the plan
> survives between agent sessions even if context is lost.

## Context

B++ is word-based by design. Strength: simplicity (everything is a
64-bit slot). Weakness: the compiler does not catch certain bugs that
type systems catch. This plan adds three defense layers without
abandoning the word-based philosophy:

1. **Level 1 — Aggressive diagnostics.** More warnings/errors that
   catch type bugs without requiring a full type system.
2. **Level 2 — Tonify Rule 13.** Convention for hints on public APIs,
   with optional warning.
3. **Level 4 (revisited) — Effect annotations.** Extend the smart
   dispatch from 2 values (`: base` / `: solo`) to 5 values (adds
   `: realtime`, `: io`, `: gpu`).

Level 4 extends what already exists, not greenfield. Smart dispatch
already classifies functions by effect (pure vs impure). This phase
adds granularity.

**Philosophy:** every level is opt-in. Existing code keeps compiling.
A programmer who wants more protection annotates more.

## Pre-requisites (already shipped before this plan was authored)

- E232 (silent float→int at assignment) — fatal error
- E233 (silent float→int at call site) — fatal error, promoted from W002
- `: base` / `: solo` annotations working (W013 mismatch detection)
- Type-lock semantics (`ty_lock_var`) protecting explicit hints from inference
- `: int` synonym removed from parser (one canonical name: `: word`)
- `smart dispatch` with `classify_all_functions()` running
- Bootstrap stable, suite at 66/0/11

---

## Execution order

Each level is an independent sprint. Bootstrap between each sub-step.
Recommendation: Level 1 → Level 2 → Level 4, because 1 and 2 are cheap
and immediately useful, and 4 benefits from running AFTER stbaudio
exists (real `: realtime` use case).

Within Level 1, ship in this order (cheapest first):
**E242 → E244 → E243 → E240 → E241**. Each diagnostic in its own
commit. Bootstrap between each. If any false positive fires in stb/ or
src/, investigate before shipping.

---

## Level 1 — Aggressive diagnostics (~80 lines code)

### E240 — untyped local passed to float param

**Trigger:** function declares parameter as `: float` and the caller
passes a variable WITHOUT `: float` hint.

**Example:**
```bpp
audio_play(rate: float, buf, len) { ... }

main() {
    auto sr;            // no hint
    sr = 44100.0;       // already E232, but in case of through-flow:
    audio_play(sr, ...) // E240: sr has no float hint
}
```

**Where:** `src/bpp_validate.bsm`, T_CALL validation path, after arg
count check. Sibling of E233 (which handles the inverse: float arg to
int param).

### E241 — silent truncation when storing float return

**Trigger:** function returns float, caller stores in non-float var.

**Example:**
```bpp
compute_distance(): float { return 3.14; }

main() {
    auto x;                      // no hint
    x = compute_distance();      // E241: float return → int slot
}
```

**Complication:** B++ has no return type annotations broadly (one
exists in `test_float_h.bpp` for `: half float` return, but it is a
parser feature with one user). Mitigation: infer return type from the
function body — if every return statement returns a float-typed
expression, the function returns float.

**Where:** `src/bpp_validate.bsm`. Extend `classify_all_functions()` to
infer return type alongside phase. The inferred return type already
exists as `ty_cur_ret` in `bpp_types.bsm` — wire it into the validator.

### E242 — shift count out of range

**Trigger:** `T_BINOP` with `<<` or `>>` and RHS is a `T_LIT` value
outside 0..63.

**Where:** `src/bpp_parser.bsm` or `src/bpp_validate.bsm` — parser is
preferred (catches at AST construction time before any later pass sees
the bad node).

### E243 — pointer compared to non-zero literal

**Trigger:** `T_BINOP` with `==` or `!=`, one operand is a pointer
(result of malloc, `fn_ptr`, string literal), other operand is a
`T_LIT` with non-zero value.

**Idiomatic exception:** `if (ptr == 0)` is a null check — never
fires.

**Where:** `src/bpp_validate.bsm`, T_BINOP validation. Need to track
ptr-ness through the AST (already partially present via `BASE_PTR` in
`bpp_types.bsm`).

### E244 — float literal in integer context

**Trigger:** float literal used where the language requires int:
- `arr[3.14]` (array index)
- `x << 2.5` (shift count)
- `x & 0xFF.0` (bitmask — though `0xFF.0` is unlikely to parse as float anyway)

**Where:** `src/bpp_validate.bsm`. Simple check at the relevant AST
sites.

### Files touched (Level 1)

| File | Change | Lines |
|---|---|---|
| `src/bpp_validate.bsm` | E240, E241, E243, E244 logic | ~60 |
| `src/bpp_parser.bsm` | E242 shift range | ~10 |
| `docs/warning_error_log.md` | Document E240-E244 | ~30 |
| `tests/test_diag_types.bpp` | Coverage for all 5 | ~80 |

### Bootstrap gate (Level 1)

Each diagnostic in its own commit. Bootstrap between each. Existing
codebase must compile with **zero false positives**. If anything fires
in stb/ or src/, investigate before shipping.

---

## Level 2 — Tonify Rule 13 + W025 (~40 lines)

### Rule 13 — Public API parameters use explicit hints when non-word

Add to `docs/tonify_checklist.md`:

> Functions declared without `static` form the module's public API.
> Parameters that expect non-word types (float, byte, struct pointer,
> etc.) should have explicit hints in the signature. Static functions
> are implementation — free to experiment. Public functions communicate
> their type expectations to callers.

| Pattern | Action |
|---|---|
| `audio_play(rate, buf, len)` where rate is float | Add: `audio_play(rate: float, buf, len: word)` |
| `static helper(tmp)` where tmp is internal | Keep as-is |
| `pos_set(x, y)` where both are word | Keep as-is (default) |

Convention-driven. W025 (the compiler nudge) was attempted in this
sprint and triggered a bootstrap regression — see the W025 status
note below; for now Rule 13 is enforced by code review.

### W025 — public function with ambiguous param type — STATUS: deferred

**Trigger:**
- Function called is non-`static`
- Function uses param in a context that suggests a specific type (e.g.
  arithmetic with a `: float` operand)
- Parameter has no hint
- Function is called from more than 2 different sites

**Where:** `src/bpp_validate.bsm`, function-level pass over non-static
functions.

**Status (2026-04-16):** A first-cut implementation introduced a
parameter-name-tracking helper (`_val_params` array, `_val_remember_param`,
`_val_is_param`, `_val_w025_check`) and a check inside the T_BINOP
handler. The helpers themselves caused a 1-cycle bootstrap oscillation
and, when the diag_help message was added, the gen2 binary failed with
"internal error: global 'break' not found in data section" — a codegen
issue that surfaces only when the new validator code is compiled by
the previous-generation `bpp`. Bisection narrowed the trigger to the
specific combination of helpers + T_BINOP wiring + a long help string,
but root cause is not pinned. Shelved for a dedicated debugging
session. Rule 13 still ships as documented convention; W025 is
tracked here.

### Files touched (Level 2)

| File | Change | Lines |
|---|---|---|
| `src/bpp_validate.bsm` | W025 logic | ~30 |
| `docs/tonify_checklist.md` | Rule 13 section | ~30 |
| `docs/warning_error_log.md` | Document W025 | ~10 |
| `tests/test_rule13.bpp` | Test coverage | ~40 |

### Bootstrap gate (Level 2)

Run on stb/. Expect 5-10 W025 firing on existing public APIs. Fix them
or suppress the ones that aren't real improvements. Goal: stb/
compiles clean with W025 enabled.

### Risk

Heuristic can fire false positives. Mitigation: tune threshold (call
site count, usage specificity) based on what stb/ shows.

---

## Level 4 (revisited) — Phase annotations expanded from 2 to 5

### Effect taxonomy

```
PHASE_BASE       pure computation, worker-safe
PHASE_REALTIME   no malloc, no IO, no blocking, bounded time (audio callback safe)
PHASE_IO         does I/O (files, network, stdout)
PHASE_GPU        touches GPU resources (render thread only)
PHASE_SOLO       catch-all: other side effects
```

New annotations:
- `: base` (already exists)
- `: solo` (already exists)
- `: realtime` (new)
- `: io` (new)
- `: gpu` (new)

### Effect propagation rules

Fixpoint on the call graph already exists. Composition rules:

| Caller context | Can call | Cannot call |
|---|---|---|
| `: base` | base | realtime, io, gpu, solo |
| `: realtime` | base, realtime | io, gpu, solo |
| `: io` | base, io | realtime (if annotated) |
| `: gpu` | base, gpu | realtime (if annotated) |
| `: solo` | any | (catch-all, permissive) |

Golden rule: `: realtime` is the strictest. `: solo` is the most
permissive. `: base` is a sublimit of all.

### Builtin effect table

Each builtin needs an effect tag. Extend `classify_all_functions()`:

| Builtin | Effect |
|---|---|
| `malloc`, `free`, `realloc`, `memcpy` | heap (currently SOLO; eventually distinct) |
| `sys_write`, `sys_read`, `sys_open`, `sys_close` | io |
| `putchar`, `putchar_err`, `getchar` | io |
| `_stb_gpu_present`, `_stb_gpu_vertex`, etc. | gpu |
| `sys_mmap`, `sys_munmap` | heap |
| `peek`, `poke`, `str_peek`, `shr` | base (pure) |
| `fn_ptr`, `call` | base (pure; callee determines effect) |
| `mem_barrier` | base (compiler barrier, no effect) |
| `sys_exit`, `sys_abort` | solo (terminal) |
| `sys_clock_gettime`, `sys_usleep` | io (kernel call, timing) |

Table lives in `src/bpp_dispatch.bsm` as a const array.

### Special case: `call(fp, ...)` into unknown

Effect is unknown at compile time. Two options:
- **MVP:** assume `: solo` (worst case, always safe).
- **Advanced:** `: calls(T)` annotation says "fp has effect T". Defer
  to post-1.0.

### Files touched (Level 4)

| File | Change | Lines |
|---|---|---|
| `src/bpp_defs.bsm` | PHASE_REALTIME, PHASE_IO, PHASE_GPU constants | ~5 |
| `src/bpp_parser.bsm` | Parse new annotations | ~15 |
| `src/bpp_dispatch.bsm` | Builtin effect table + propagation | ~80 |
| `src/bpp_validate.bsm` | Effect compatibility checks + W013 extensions | ~30 |
| `stb/stbaudio.bsm` | Annotate audio callback `: realtime` | ~5 |
| `stb/stbgame.bsm` | Annotate GPU funcs `: gpu` | ~10 |
| `docs/bpp_language_reference.md` | New "Effect annotations" section | ~80 |
| `docs/tonify_checklist.md` | Extend Rule 4 | ~20 |
| `tests/test_effects.bpp` | Comprehensive effect test | ~100 |

### Bootstrap gate (Level 4) — three sub-steps

**Sub-step A — infrastructure (~50 lines).** Constants, lexer, parser
accepts new annotations. Zero behavioral change. Bootstrap must match
pre-A sha exactly.

**Sub-step B — classification + propagation (~60 lines).** Builtin
effect table. `classify_all_functions()` extended to propagate
effects. Still no enforcement. Bootstrap verifies classification by
dumping with `--show-phases`.

**Sub-step C — enforcement (~40 lines).** Validator checks effect
compatibility at call sites. W013 extended. Existing `: base` /
`: solo` code keeps working. New annotations active. Bootstrap must
pass with existing code unchanged.

### Integration with stbaudio

When stbaudio ships, the audio callback annotated `: realtime`. The
compiler verifies. If `malloc` is accidentally called from the
callback path, the compiler rejects it.

This is the killer demo: "your audio code can't have silent glitches
from realtime violations because the compiler proves it."

### Risk

Effect propagation via fixpoint can produce unexpected
classifications. Mitigation: `--show-phases` diagnostic shows every
function's classification.

False positive: `sys_clock_gettime` marked `io` but safe in realtime.
Mitigation: `: suppress(io)` escape hatch for known-safe cases.
Post-MVP feature.

---

## Total summary

| Level | Code | Test | Docs | Value |
|---|---|---|---|---|
| 1 — Diagnostics | ~80 | ~80 | ~30 | High (immediate bug catching) |
| 2 — Rule 13 + W025 | ~30 | ~40 | ~40 | Medium (API discipline) |
| 4 — Effects expanded | ~145 | ~100 | ~100 | High (real-time safety) |
| **Total** | **~255** | **~220** | **~170** | |

Three sprints, each independently bootstrappable. Can be done in
sequence or parallel (different files touched, minimal overlap).

---

## Post-plan sweep (mandatory)

After each level ships, apply the new feature across `src/` and `stb/`.
Sweeps are not "extra work" — they are the integral closing of each
level. Without the sweep, features stay decorative: language gets the
tool but the codebase doesn't use it, and a newcomer reading
canonical code does not learn the new pattern.

| Level shipped | Sweep | What it does |
|---|---|---|
| Level 1 → | **Sweep 2a** | Fix every E240-E244 site that fires across `src/` + `stb/` + `games/`. Each fire is either a latent bug to fix or a place that needs an explicit hint. |
| Level 2 → | **Sweep 2b** | Apply Rule 13 to every public API in `stb/`. Annotate parameters with their real types where the type is not word. Modules: stbaudio, stbfont, stbrender, stbecs, stbphys, stbmath, etc. |
| Level 4 → | **Sweep 2c** | Annotate `: realtime` / `: io` / `: gpu` across `src/` + `stb/`. The audio callback in `stbaudio` becomes `: realtime` (this is the killer demo: compiler proves no malloc inside callback). Render-path functions in stbgame/stbrender become `: gpu`. I/O wrappers become `: io`. |

### Why this matters

Adding `: realtime` does not catch bugs if nobody annotates. The audio
callback without `: realtime` keeps accepting anything. Value only
appears once `stbaudio.audio_callback` carries the annotation and the
compiler proves `malloc` does not reach it.

Sweep is where the loop closes between feature and practice. It also
calibrates the feature itself: if a sweep finds many latent bugs, the
diagnostic was worth shipping. If it finds nothing, the diagnostic was
either too conservative (relax it) or covers a problem that does not
exist in this codebase (consider removing).

### Estimated cost

- **Sweep 2a:** 30-80 sites depending on how aggressive E240-E244 are
  in practice. 1-2 sessions.
- **Sweep 2b:** ~15 stb modules with public APIs, ~5-15 functions each
  worth a hint. ~50-100 hints added. 1 session.
- **Sweep 2c:** ~30-50 functions across `src/` + `stb/` that earn an
  effect annotation. 1-2 sessions.

**Total post-plan sweep:** 4-5 sessions of mechanical-but-criterious
work (each site evaluated individually).

### Discipline

Sweep ships in the same PR as the level it sweeps for. Don't merge
"Level 2 + Rule 13 docs" without merging "Sweep 2b adds the hints" in
the same window. Otherwise the canonical code drifts from the
canonical rule and the language teaches the wrong pattern by example.
