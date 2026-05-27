# Sidequest brief — void / statement-context inlining (S4 follow-on "VI")

**For:** the external review agent. **Ask:** evaluate this plan before we
implement it — is the design sound, is the safety reasoning complete, is the
value worth the risk, is there a better approach? This is a **compiler
codegen** change (Layer 3 + a64 Layer 4), so the bar is bootstrap
byte-stability + the three-runner tripod green. Same review pattern as the
allocator / file_stat / horde-movement studies.

> **Status of S4 itself:** SHIPPED / CLOSED 2026-05-21 (commits `6b9f719`
> `6a7dcd5` `2f9a36a` P3b-1/2/3, closeout `3076e60`; journal "S4 inline
> (shipped 2026-05-21) ~1.1x"). The `docs/plans/sidequest_cost_model_inliner.md`
> header was stale ("IN PROGRESS / P3b next") and has been corrected. **This
> brief is a NEW increment, not a re-opening of S4.** Do not touch shipped S4
> code paths except to extend them as described.

## Motivation (verified, not hypothesised)

A 200v200 RTS stress profile (`games/rts1`, software path) is dominated by
A* pathfinding. The hot helper `_path_heap_swap` (`stb/stbpath.bsm`) shows up
as its own symbol (~15 frame samples) — a 6-statement `void` body called in a
tight loop from `_path_heap_sift_up` / `_path_heap_sift_down`. It is NOT being
inlined by the shipped S4 inliner.

**First hypothesis (WRONG, recorded so it isn't re-tried):** the S4
`param_refs ≤ 1` gate excludes it because `_path_heap_swap(pf,...)` references
`pf` many times. — Refuted by reading `classify_inlineable`
(`src/bpp_dispatch.bsm:5844`): that gate is enforced **only for
`body_cnt == 1`** (lines 5901-5922); the P3b-3 multi-statement splice binds
args to `_inl<N>_<param>` slots once, so param reuse is explicitly fine for
multi-statement bodies.

**Verified real reason — criterion 3.** Running `_path_heap_swap` through every
gate in `classify_inlineable`:

| Criterion | `_path_heap_swap` |
|---|---|
| 1. phase ∈ {BASE, AUTO, SOLO} | ✅ (writes via a pointer param, no calls) |
| 2. body_arr ≠ 0, body_cnt ≥ 1 | ✅ (7 statements) |
| **3. last statement is `T_RET`** | ❌ **FAILS** |
| 4. param_cnt ≤ 3 | ✅ (pf, i, j) |
| 5. no floats | ✅ |
| 6. node_count ≤ `INLINE_BODY_NODE_CAP` (50) | ✅ |
| 7. no `T_CALL` in body | ✅ |
| 8. param_refs ≤ 1 | n/a (only `body_cnt == 1`) |

`_path_heap_swap` is `void`; its body ends in an assignment
(`p.heap_pos[cj] = i;`), not a `return`. Confirmed in the parser
(`src/bpp_parser.bsm:1928-1936`) that **no synthetic trailing `T_RET` is
appended** — a void function's implicit return is a *codegen epilogue*
(`emit_jump_to_epilogue`), never an AST node. So criterion 3 rejects it.

**The design boundary this reveals:** the S4 splice handles **value-returning
calls in expression context** (it needs the trailing `T_RET`'s expression as
the call's value). **`void` helpers called as statements are out of scope** —
they have no value to thread. That is a deliberate boundary, not a bug. This
brief proposes extending the inliner across it.

## Proposed change — surface (decision in spine, emission per-chip)

Per the bootstrap manual's Discipline #3 + the "backends differ in HOW, not
WHAT" rule, the *decision* goes in the chip-neutral spine, the *emission* in
the a64 chip backend only (exactly as Phase B2 / S4 already are a64-only).

| Where | Change |
|---|---|
| `src/bpp_dispatch.bsm` `classify_inlineable` (5844) | Accept a `void` function (`fn_void[i] == 1`, global from `bpp_parser.bsm:53`) whose body contains **zero `T_RET` nodes** (straight-line, falls off the end). Reuse criteria 1, 2, 4, 5, 6, 7 unchanged. |
| `src/bpp_dispatch.bsm` pre-reg walker (`_inline_register_callsite` 5658 + the AST walker 5705) | Currently stamps a `callsite_id` + registers `_inl<N>_<param>` slots only for **multi-statement** inlineable callees ("Single-statement inlineable callees are left alone", 5695-5696). Extend it to also stamp **void** inlineable calls (any body_cnt), so the splice has its mangled slots. |
| `src/backend/chip/aarch64/a64_codegen.bsm` splice site (1397) | Route void-inlineable calls to `_a64_emit_inline_multi`. Keep the `inl_bcnt == 1 && body[0] is T_RET` fast path for value calls; everything else (multi-stmt value, OR void any-bcnt) goes to the multi helper when `n.e > 0`. |
| `src/backend/chip/aarch64/a64_codegen.bsm` `_a64_emit_inline_multi` (486) | Add **void mode**: detect it via "last statement is not a `T_RET`". In void mode, emit **all** `inl_bcnt` statements via `cg_emit_stmt` (the value loop today is `while si < inl_bcnt - 1`, then it emits the trailing `T_RET`'s expr as the value at 603-606 — void mode runs `si < inl_bcnt` and skips the final-value step), and return a dummy expr-type (the call's value is discarded in statement position). |
| C emitter (`backend/c/bpp_emitter.bsm`) + x64 (`x64_codegen.bsm`) | **No change.** The C emitter does not splice (`fn_inlineable` absent — emits plain calls, lets `-O2` inline); x64's Phase B2 is disabled. Both keep emitting calls — semantically identical, since inlining preserves behaviour. The requirement on them is only that they do **not choke** on a function the shared classifier now marks inlineable (they must still emit a correct call). This needs verifying, not coding. |

## Safety reasoning

- **Early-return mis-splice.** If a void body contained a `return;` that is not
  the last statement, splicing all statements would emit a jump to the
  *caller's* epilogue mid-body — a serious bug. **Guard:** a void candidate is
  rejected if **any** statement is a `T_RET`. (Criterion 6 already rejects
  control flow via the 99-node trick, so an eligible void body is straight-line;
  the extra T_RET scan covers a trailing/dead `return;`.) `_path_heap_swap` has
  no return → qualifies.
- **A trailing explicit `return;` in a void function** already works today via
  the existing value path (last stmt IS `T_RET`, clones `make_int_lit(0)` as a
  harmless discarded value). So the new path targets only fall-off-the-end void
  functions.
- **Statement vs expression context.** A `void` call can only appear as a
  statement (using it in an expression is a type error), so "no value to
  thread" is always correct for the void case.

## Phasing (each step bootstrap-stable + tripod-green, mirrors P3b cadence)

- **VI-1 — infrastructure, inert.** `classify_inlineable` marks void candidates,
  but the a64 splice site guards void → falls through to a normal call (no
  splice yet). Must produce **`gen1 == gen2` exact** and an identical suite
  (zero behaviour change). Mirrors "P3b-1 infrastructure, dead code".
- **VI-2 — activate.** Extend the pre-reg walker to stamp void calls + add the
  void mode to `_a64_emit_inline_multi` + flip the splice-site guard to route
  void → multi helper. Void inlining goes LIVE. **Expect a 1-cycle bootstrap
  oscillation** (the compiler now inlines its own `void` helpers, e.g. the many
  `emit_*` functions, so its codegen of itself changes once) → confirm
  `gen2 == gen3` per the manual. Full tripod green.
- **VI-3 — measure.** Rebuild rts1, re-profile `RTS_STRESS` (expect
  `_path_heap_swap` folded into the sift functions, gone from the symbol
  profile). Run `tests/bench_compile.sh` (the compiler inlining its own void
  helpers may also shave bootstrap compile time, the way S1-S3k did).

## Verification (the tripod — already baselined on the current tree)

| Leg | Baseline (pre-change) | Command |
|---|---|---|
| Native ARM64 | **180 / 0 / 12** | `sh tests/run_all.sh` |
| C backend | **145 / 0 / 47** | `sh tests/run_all_c.sh` |
| Linux x86_64 (Docker/XQuartz) | **11 / 11** (5 headless + 5 X11 + `test_path`) | manual loop, `bootstrap_manual.md` §"Running the suite" |
| Bootstrap byte-id | **`e5a96d06904a3af73f5617b608076b39c8bcf369`** | gen1 == gen2 |

The stbpath lazy-clear that already shipped in this branch is validated on all
three codegen paths (`test_path` passes native, C, and cross-compiled x86_64).
Each VI step must hold every row green (VI-2 may move bootstrap to a new stable
hash via the expected 1-cycle, confirmed by `gen2 == gen3`).

## Risk

Medium (codegen). Mitigations: inert-first phasing; the early-return guard; the
expected-and-confirmed 1-cycle oscillation; the tripod as the net; nothing
committed without the full verification. Lower bound on blast radius: a bug
here breaks every a64 binary (Layer 4), so the bootstrap + tripod gate is
non-negotiable.

## The honest value case

The immediate target is small (~15 samples in one game). But void
statement-context inlining is a **general capability**: every `void` helper
called in a hot loop benefits — across stb, games, and the **compiler itself**
(which is full of `void emit_*` helpers). The realistic upside is a
bootstrap-compile-time win (à la S1-S3k) **plus** the game-side win, not just
the 15 samples. The alternative — source-side manual inlining of just
`_path_heap_swap` into the two sift functions — is zero-compiler-risk and buys
the 15 samples, but buys nothing general.

## Questions we want the external agent to pressure-test

1. Is "emit all statements, no final value" the correct and complete void
   splice semantics? Any interaction with the binding prologue (arg eval order,
   the BIND-vs-SUBSTITUTE per-param strategy) that breaks for a void body?
2. Is the "reject any `T_RET` in a void candidate" guard sufficient, or is there
   a sharper rule (e.g. allow a single trailing `return;`)?
3. Is the 1-cycle (not 2-cycle) oscillation prediction correct for "the compiler
   begins inlining its own void helpers"? Any reason it could be 2-cycle (a real
   bug) instead?
4. Portability: are we certain the shared classifier marking void inlineable
   cannot perturb the x64 (B2-disabled) or C-emitter paths beyond "still emits a
   call"? Where exactly would each read `fn_inlineable` and could either act on
   it unexpectedly?
5. Value vs risk on the last engineering day: ship the general compiler
   capability (VI-1→VI-3), or take the zero-risk source-side manual inline and
   stop? What would you do?

## Cross-references

- `src/bpp_dispatch.bsm` — `classify_inlineable` (5844), `_inline_register_callsite`
  (5658), `_inline_pre_reg_walk` (5705), `INLINE_BODY_NODE_CAP` (5809).
- `src/backend/chip/aarch64/a64_codegen.bsm` — `_a64_emit_inline_multi` (486),
  T_CALL splice site (1397).
- `src/bpp_parser.bsm` — `fn_void` (53), body build with no synthetic T_RET (1928).
- `docs/plans/sidequest_cost_model_inliner.md` — the shipped S4 design (header now
  corrected to SHIPPED).
- `docs/manual/bootstrap_manual.md` — Disciplines, the three-runner tripod
  (§"Running the suite"), the Docker/XQuartz Linux leg.

---

# External agent review (2026-05-27, session 2)

Pressure-test of the 5 questions + verdict on the proposed sequence.

## Q1 — "Emit all statements, no final value" is complete void splice semantics?

**Yes, complete.** The binding prologue (`_inl<N>_<param>` slots + arg→slot stores)
runs *before* the body and is independent of what the body does with values. The
value-vs-void difference is exclusively post-body:
- Value mode: loop emits `inl_bcnt - 1` statements, then extracts the trailing
  `T_RET` expr as the call's value via `cg_emit_node`.
- Void mode: loop emits `inl_bcnt` statements, returns a dummy expr-type.

BIND-vs-SUBSTITUTE (P3b-3) is a per-param decision based on `param_refs` in the
body — orthogonal to body shape. Arg eval order is fixed by the prologue regardless.

**Single safety bolt to add**: assert in the void splicer that the caller is in
statement context. Today this is true by typing rules (`void` calls can't be in
expressions), but a future reroute could violate it silently. Cheap defensive
assert in `_a64_emit_inline_multi`'s void branch.

## Q2 — Is the "reject any T_RET" guard sufficient?

**Yes, and it's the *correct boundary*. Don't try to sharpen it.**

The three cases partition cleanly:
- void with trailing `T_RET` → **already handled** by the existing S4 value path
  (clones `make_int_lit(0)` as discarded value).
- void with no `T_RET` (falls off the end) → **VI proposed path**.
- void with mid-body `T_RET` → **REJECT** (early-return mis-splice).

A sharper rule (e.g. "allow exactly one trailing T_RET in VI") would just duplicate
the value path's territory. Each path should own exactly one case; overlap is where
latent bugs nest (reuse-bug lesson).

**Edge to verify**: B++ distinguishes `return;` (no expr) from `return expr;`? If
both parse to `T_RET` nodes, the scan catches both. Should be a 1-line grep on the
parser to confirm.

## Q3 — Is 1-cycle (not 2-cycle) the correct oscillation prediction?

**1-cycle is correct, conditional on a property worth verifying explicitly.**

Property: `classify_inlineable` reads only the *parsed AST* of the callee
(`body_arr`/`body_cnt`/`param_cnt`/node count), and inlining mutates only the *emit*
not the AST. If true, classify decisions are deterministic across generations →
gen2 == gen3 byte-identical.

**2-cycle would indicate a real bug**, most likely:
1. Feedback loop — classify reads post-inline state (e.g., node count computed
   after splicing).
2. AST mutation — splice modifies `body_arr` instead of just emit.
3. Hidden dependency — classify consults state populated during codegen.

**Verification before VI-2**:
```bash
grep -n "fn_inlineable\|classify_inlineable\|INLINE_BODY_NODE_CAP" \
  src/bpp_dispatch.bsm src/backend/chip/aarch64/*.bsm
```
Confirm classify reads only AST inputs. If yes, 1-cycle is mathematically guaranteed.

**Stop-the-line signal**: if gen3 != gen2 during VI-2 bootstrap, STOP. Don't force
`gen4 == gen3` as "good enough" — it would hide a real feedback loop.

## Q4 — Portability: classifier marking void inlineable can't perturb x64/C beyond "still emits a call"?

**VI-1 is safe by design. VI-2 has a real slot-leakage risk that needs concrete verification.**

### VI-1 (inert)
Only `fn_inlineable[i]` is set. The a64 splice site guards void → plain call (VI-1
promise). x64 has B2 disabled; C emitter doesn't read `fn_inlineable`. Zero
perturbation. Verifiable: `gen1 == gen2` exact is the mandatory gate.

### VI-2 (active) — the risk

The pre-reg walker in VI-2 stamps `callsite_id` + registers `_inl<N>_<param>` slots
in the **var table**. The var table is read by *every* backend for local declarations.

Scenario: classify marks `_path_heap_swap` inlineable → walker adds `_inl0_pf`,
`_inl0_i`, `_inl0_j` to the caller's var table → **x64 emits stack stores for slots
never read** (B2 disabled doesn't consume them) → **C emitter emits
`long long _inl0_pf;` declarations** never referenced.

This is dead code, not a correctness bug — but it IS perturbation beyond "still emits
a call". If the C suite byte-compares emitted source against a baseline, it could
fail.

**Mandatory verification between VI-1 commit and VI-2 implementation**:
```bash
# After VI-1 commits:
sh tests/run_all_c.sh 2>&1 | tail -3   # expect 145/0/47
# (VI-1 walker NOT yet extended to void → no slot leakage yet)

# After VI-2 implementation but BEFORE flipping the splice guard:
# Build with walker extended but a64 splice still guarded → x64/C should still be 145/0/47.
# If they regress here, the walker's slot-registration is leaking and needs gating.
```

**If VI-2 perturbs x64/C** — the cleaner fix is **lazy slot registration at emit
time** in a64 (not in the cross-cutting walker). The walker stamps only
`callsite_id` (opaque to non-splicing backends); a64 emit declares slots locally.
Half-day refactor, no catastrophe.

## Q5 — Value vs risk on the last engineering day?

**Do BOTH in order, but STOP before VI-2.**

### Recommended sequence (refines the agent's proposal)

1. **Manual source-side inline of `_path_heap_swap` into `_path_heap_sift_up` /
   `_path_heap_sift_down`** (~5 min, zero compiler risk).
   - Hedge: locks the 15-sample rts1 win regardless of what happens with VI.
   - Commit: `perf(rts1/pathfinding): manual inline of _path_heap_swap into sift loops`.

2. **VI-1 inert classifier flag** (~1 h).
   - gen1 == gen2 exact required.
   - Suite + tripod green.
   - Commit: `feat(dispatch): mark void functions as inlineable candidates (inert)`.

3. **STOP today. Handoff for next session.**
   - VI-2 + VI-3 deferred. Reasons:
     - Q4 verification (slot leakage in x64/C) requires VI-1 to be committed and
       run against baseline, *then* VI-2 implemented and re-verified. Two
       verification cycles needed.
     - Codegen bugs broke us once already (reuse-bug). "Last engineering day" +
       "codegen change" + "tight timeline" = the exact combination that produces
       bugs landed with no recovery window.
     - VI-2 blast radius is every a64 binary. Bar is high.

### Why both source-side AND VI-1 (not just one)

- Source-side is the immediate, guaranteed win — locks the 15 samples.
- VI-1 plants the marker for the general capability; zero risk to commit.
- Doing both today leaves *two* secure commits + clean handoff for VI-2.
  Better than four commits with one falling over at the end.

## Verdict

✅ Q1, Q2, Q3 — design technically sound.
⚠️ Q4 — real slot-leakage risk in VI-2. Concrete verification between phases is
non-negotiable. Mitigation (lazy slot registration) available if needed.
✅ Q5 — agent's proposed sequence is correct, refined with the source-side inline
as a parallel hedge.

The next session can pick up VI-2 with the verification gate explicit. If gen3 !=
gen2 OR C suite regresses, the lazy-slot refactor is the fallback path before
proceeding.

---

# Progress + portability contract (2026-05-27, session 3)

## Shipped this session
- Source-side: `_path_heap_swap` hand-inlined into both sift functions +
  stbpath lazy-clear — `06ae76e` (locks the 15 samples independent of VI).
- **VI-1** (inert classifier flag `fn_void_inlineable`) — `4beaec3`. gen1==gen2
  exact (`2f951f0d`); tripod identical (native 180/0/12, C 145/0/47, Linux 11/11).
- **VI-2** (walker stamps void callsites + a64 splice void mode + defensive
  assert) — this commit. Bootstrap converged as predicted: gen1 `522422a2` !=
  gen2 `e6d6148a` == gen3 (the 1-cycle is the compiler now inlining its own void
  helpers). Tripod: native 180/0/12, **C 145/0/47 (the mandatory Q4 check)**,
  Linux Docker 11/11.

## Q4 risk RESOLVED (not just mitigated)
The slot-leakage worry: the walker stamps `_inl<N>_<param>` into the var table;
x64/C read it → dead-code/regression. Investigation found the walker
(`_inline_pre_reg_walk_body`) is invoked from **a64 only** (`a64_codegen.bsm:689`
is the sole call site — not x64, and the C emitter never runs pre_reg nor reads
`cg_vars`). So void slots are registered only on the a64 path, where the splice
consumes them; x64/C never see them. `run_all_c` stayed 145/0/47, confirming it
empirically. The lazy-slot fallback was not needed.

## Portability contract (Disciplina #2 — behavioral parity, not optimization parity)
Inline (S4/B2 value + VI void) is an **a64-only optimization**:
- **a64** inlines → correct + optimized.
- **x64** emits a plain call (B2 disabled) → correct, not optimized.
- **C emitter** emits a plain C call and delegates inlining to the C compiler
  (`-O2`) → correct, optimized at the C level. Intentional + permanent.

All three produce behaviorally-equivalent programs, so portability parity holds.
Documented at the `x64_codegen.bsm` call-emit site and here.

## The x64 self-host bug (fuxicada finding) + Linux bootstrap status
Confirmed empirically this session: the x64-cross-compiled bpp (`bpp_lin`)
**SIGSEGVs at startup, deterministically** (6/6 rc=139, even no-args). Small x64
programs cross-compile + run fine (tripod 11/11), so it is a **scale-dependent**
codegen/ELF bug that only hits the large bpp binary (consistent with the old
"many call sites" note). Narrowing showed it crashes before any real work (even
`--show-deps`), i.e. at startup/load of the large binary.

**Answer to "can we bootstrap on Linux via Docker?":** in principle yes (Docker
is a real Linux x64 environment); in practice **no, not yet** — a Linux-native
bootstrap (run bpp on Linux to compile src/bpp.bpp → gen2_lin, check byte-identity)
is blocked by this exact crash, because bpp can't run on Linux to self-host. The
macOS-ARM64 self-host bootstrap works; the Linux-x64 self-host is the open
frontier. Fixing the startup SIGSEGV (the "Linux x86_64 health restoration" arc —
approach: build `bpp_lin` with `--bug`, core dump + gdb in the container, or real
x64 hardware) would unblock BOTH x64 inline AND a Linux Docker bootstrap. Good
candidate for a "last exercise".
