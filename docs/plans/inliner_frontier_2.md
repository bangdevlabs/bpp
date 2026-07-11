# Study — the Inliner Frontier, second pass (2026-07-10)

**Status: STUDY — measured driver for reopening the inliner arc.**
The first arc closed 2026-06-24 (`docs/plans/legacy/inliner_arc.md`,
Inc 1-8 + two post-close addenda). This study re-derives the frontier
empirically after RegAlloc v2 completed, per the standing
"re-disassemble instead of guess" discipline.

## Why now — the gap composition flipped

The 2026-07-10 catalog (benchmarks.md) decomposes sf_bench (stereo,
quiet machine, min-of-5):

| build | µs / 3s render | multiplier |
|---|---|---|
| ours (call-heavy) | 12476 | — |
| hand-inlined flat | 5044 | **inliner gap ×2.47** |
| gcc -O2 oracle | 4406 | **flat-codegen gap ×1.14** |

At the arc's close the split was ×3.1 / ×1.8 (mono) and ×2.5 / ×1.39
(07-09). **RegAlloc v2 closed most of the flat-codegen half (1.8 →
1.14); the inliner half is now essentially THE whole remaining gap on
call-heavy audio code.**

## The bl census (bug --disasm, current compiler, sf_bench binary)

Per-sample call tree today (bl → callee):

```
sf_render → sf_render_block → sf_channel_process   [1+1 per sample/ch]
  ├─ blondie_process → amp_process                 [new insert since the arc]
  │     └─ oversample_up, _amp_stage_chain ×2,
  │        oversample_down, conv_tick              [5 bl/sample when active]
  ├─ spring_process                                [0 bl — nested-VI win HOLDS]
  ├─ zener_process → comp_process
  │     └─ amp_to_db_f → log_f, db_to_amp_f → exp_f  [4 bl/sample when active]
  └─ rotary_tick
        └─ lfo_set_rate → _lfo_safe_inc,
           lfo_tick → lfo_value                    [4 bl/sample when active]
```

The moog chain (`moon_process → moog_slope → moog_taps →
flt_onepole_tick`) remains fully spliced — zero bl. The bench demo
activates ONLY the moon filter + rotary, so sf_bench's ×2.47 is carried
by the channel dispatch + the rotary/lfo chain; the blondie and
compressor chains cost nothing in the bench but are the real per-sample
price in any project that uses those inserts.

## THE finding — the 2026-07-03 totality guards silently re-armed the walls

Inc 8 (2026-06-24) had `db_to_amp_f` + `exp_f` (tier 4, transitive
hotness) fully spliced into `comp_process` — "zero bl anywhere in that
chain", measured. Today `comp_process` has 2 bls again. Root cause:
the audio-grade totality hardening (`43e2872`, 2026-07-03) prepended
early-return guard clauses to the transcendental family —

```
exp_f:  if (x != x) { return x; }  if (x < -745.0) { return 0.0; }
sin_f/cos_f: magnitude guards (same shape)
log_f:  if (x <= 0.0) { return 0.0; }   (was already there)
```

— and an early return ahead of control flow is EXACTLY the shape Inc 7
disqualifies (`_inline_body_has_nested_ret`), correctly. Composability
then propagates the block upward: `db_to_amp_f`/`amp_to_db_f` call a
non-inlinable callee → refused; `lfo_value`'s LFO_SIN arm calls `sin_f`
→ refused (its trailing switch-of-returns is otherwise exactly what
`_inline_normalize_switch_ret` handles); `lfo_tick`/`lfo_set_rate` →
refused; `rotary_tick` keeps its 2 bls. One hardening commit quietly
re-opened two whole chains (compressor dB math, rotary LFO), and no
gate caught it because the bench demo exercises neither.

**Watch rule going forward:** adding a guard clause to a hot leaf is a
potential inliner regression — the bl-census (this study's script) is
the check, not sf_bench alone.

## The ranked levers

1. **Leading-guard normalisation (the one that pays everywhere).**
   Generalize Inc 3b's guard-clause→ternary normaliser to a PREFIX of
   `if (cond) { return X; }` guards ahead of an otherwise-clean body:
   rewrite into nested ternaries feeding the trailing return (pure
   expression, no early return left). Re-opens exp_f (tier-4 hotness
   already reaches it — Inc 8 proved it), sin_f, log_f, _lfo_safe_inc →
   collapses BOTH re-opened chains and unlocks amp_to_db_f for the
   first time. The alternative (teaching the splice to rewrite early
   returns into structured control flow) is the heavier "real future
   increment" the arc named; the prefix-guard normaliser is the cheap
   80%.
2. **The insert dispatch layer** (`sf_channel_process`'s 4 bls +
   `sf_render_block`): multi-return, control-flow-gated, big bodies —
   the honest answer may be "leave them" (one bl per insert per sample
   is the plugin boundary); measure after lever 1.
3. **blondie's amp chain** (`oversample_up/down`, `_amp_stage_chain`):
   new since the arc, 5 bl/sample when active. conv_tick's cost is its
   own loop (inlining buys ~nothing); the oversample pair are
   small-bodied candidates worth a census after lever 1.
4. **Re-run hot-constant promotion on spliced subtrees** — the arc's
   other named leftover (wide literals lose hoisting when spliced;
   the xform-regression class).

## Method note

The census script (address-map + `bl` targets per function) lives in
this study's journal entry; it is the acceptance metric for any
reopened increment — bl/sample per chain, then sf_bench + a
compressor-active + a blondie-active bench variant so the inactive-in-
demo chains stop hiding.

## Execution record — 2026-07-10, levers 1-4

**Lever 1 SHIPPED** (`2fd8077` mechanism byte-identical, `2c2178b`
normaliser arity fix + test, `2f0975b` flip). Structured early returns:
splice frames carry an end label; T_RET inside a splice emits value
(coerced to the callee's declared type / discarded / stored to
multi-assign targets) + jump. Three latent bugs fell during landing:
(1) the guard/switch normalisers silently DROPPED results 2..N of
multi-value returns (compiler segfault, pre-existing, found by the new
test); (2) the a64 encoder corrupted frames past 4095 bytes — the 13th
immediate bit landed on SH and a 5392-byte frame became 5 MB
(enc_add/sub_imm_any now split); (3) the 99 cost sentinel collided with
real early-ret body costs (INLINE_DISQ = 99999). Census: compressor
chain 6 bl → 3 (zener + both dB wrappers spliced), lfo_set_rate
spliced. Remaining: exp_f / log_f / sin_f / lfo_value — tier-4 cap
question, measure before raising.

**Lever 2 verdict — leave the insert dispatch layer.** The 4 surviving
sf_channel_process bls (blondie, comp, spring, rotary) are big
multi-return control-flow bodies over every cap; the xform lesson
below says merging register-hungry bodies into one budget is how
regressions happen. One bl per active insert per sample is the honest
plugin-boundary price today.

**Lever 3 verdict — deferred, low priority.** blondie's amp_process
(5 bl/sample when active: oversample pair, stage_chain ×2, conv_tick)
is inactive in the bench and the DAW holds 122×+ realtime; conv_tick
is loop-bound (inlining buys nothing). Revisit with a blondie-active
bench when the plugin ships in a project.

**Lever 4 SHIPPED with its target renamed** (`6b39179`).
cg_b3_desc_consts makes spliced callees' constants visible to
hot-constant promotion (loop-depth mirrored — without it the constants
recorded at weight 1). But the wide-lit refusal STAYS: with it off,
xform ran ~11.4ms vs its 6.6-7.8 band even with constants visible —
bug --disasm showed movz+movk chains re-materializing IN-loop because
the merged caller's B3 budget was exhausted. The refusal is a proxy
for "don't splice a register-hungry loop body into a
budget-constrained caller"; true retirement = budget-aware splice
gating + mangled-slot ref counting (the named future increment,
together with the tier-4 cap question above).

## Addendum — the tier-4 cap question, answered (2026-07-10 night, `ce685ca`)

Instruments first: **BPP_INLINE_PROBE=1** prints every classification
(cost/arity) and every tier-4 callsite ADMIT/DENY with cost-vs-threshold;
**sf_bench_inserts.bpp** is the all-inserts acceptance bench (baseline
102.9 ms / 29× / ×2.3 vs oracle — levers 2 and 3's real numbers live
here now: the blondie chain's 90 ms is dominated by the 1024-tap conv,
loop-bound, inlining-neutral).

The measurement: exp_f 98 / cos_f 101 / log_f 104 / sin_f 106 vs the
90 hot ceiling — denied by ≤ 16. (log_f is a true leaf today; the
exp_f-bootstrap note above is stale.) The obvious fix — ×2 threshold
for true-leaf callees — was tried and **REFUSED by the bench: +27%**
(30 exp_f sites admitted; ~100-node bodies merged into comp_process /
sf_channel_process; register budgets diluted). Together with the
wide-lit finding, that is the SAME mechanism measured twice in one
night, and it settles the frontier's shape:

**The next inliner increment is budget-aware splice admission** — a
caller-side pressure account (projected live locals of already-admitted
splices vs the register budget) consulted at tier-4 admission time,
replacing both the wide-lit proxy and any flat threshold tuning. Its
two measured drivers: xform 6.8→11.4 ms (wide-lit off), inserts bench
102.9→130.5 ms (leaf bonus on). Until it exists, the transcendental
family stays as honest bl's — cheap ones: the chain they'd collapse is
worth ~2 bl/sample against a 90 ms conv-dominated render.
