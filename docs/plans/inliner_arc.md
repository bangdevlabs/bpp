# Plan — the Inliner Arc (codegen Frontier 1)

**Goal:** close the **×3.7 inliner gap** the tiny_lofi DAW exposed (16.6 ms
`tl_render` vs 4.5 ms hand-inlined vs 2.2 ms clang -O2; see benchmarks.md). Real
call-heavy programs pay a per-call tax that the tight microbenchmark kernels hid.
Frontier 2 (RegAlloc v2, the residual ×2) follows this arc.

## The starting point (2026-06-18)

The **active** inliner (Phase B2, `classify_inlineable` in `bpp_dispatch.bsm`)
only inlines **leaf single-return** functions and rejects, in stacked criteria:
float params/returns (6081/6084), multi-value-return (6056), any `T_CALL` in the
body (6093), control flow (node-count 99), >3 params. A **cost-model inliner
(S4)** is built but **dormant** (`_inline_cost`/threshold exist; the consumer
`classify_inlineable_v2` was never wired).

Our DAW hot path is blocked everywhere: `flt_onepole_tick` (float),
`moog_taps` (float + multi-return), `moog_slope` (float + switch + call),
`tl_channel_process` (float + if + call), `arr_struct_at` (if + 2 returns).

## Lesson from the first attempt (2026-06-18) — gate before you relax

The first Inc 1 *relaxed eligibility* (let peek/poke wrappers inline) WITHOUT a
hotness gate. It was correct + byte-stable, but a measurement of the general
benchmark (the user's question) caught the trap: the active inliner inlines an
eligible function at **every** call site, and peek/poke wrappers are ubiquitous,
so the **compiler binary grew +132 KB (+13%)** with **no compile-time win**
(0.26 → 0.29 s, flat) — bloating hundreds of cold sites to speed up a few hot
loops (+3% DAW only). **Reverted.** Eligibility relaxation only pays at HOT call
sites; without the gate it's a net loss. So the gate comes first.

## Lesson from the second attempt (2026-06-18) — verify the BOOTSTRAPPED compiler

The size-gated Inc 1 (commit `a314e36`, since reverted) avoided the bloat and
passed native 192/0/12 + C-emit 155/0/49 + byte-stable bootstrap — but it
**broke `--c`**: a compiler *bootstrapped from* the Inc 1 source crashes on
`bpp --c` (SIGSEGV in the C-emit path — the builtin-wrapper inline mis-compiles
a function `bpp_emitter.bsm` uses). The native suite never exercises `--c`, and
**the regression was MISSED because `run_all_c.sh` ran with the OLD `./bpp`, not
the freshly-bootstrapped Inc 1 compiler** — so it tested the previous codegen's
`--c`, which was fine. The bug only surfaces when the NEW compiler emits C.

**The verification gap (now a hard rule for this arc):** an inliner change must
be verified by **bootstrapping → INSTALLING (fresh inode, never codesign) → then
running `run_all_c.sh`**. The C-emit suite only catches a self-miscompile when
`./bpp` IS the compiler under test. `test_bootstrap_stable.sh` checks byte-
stability but NOT `--c` correctness, and a stable fixpoint can still be a
miscompile. Add a `bpp --c <smoke>.bpp` check on the bootstrapped binary to the
per-step gate, alongside binary size.

(The whole detour also cost hours to the **Golden Rule** violation — overwriting
`./bpp` with an untested binary, which AMFI then SIGKILLed (137), contaminating
every downstream `/tmp` compiler. Recover with `git show HEAD:bpp > bpp` via a
FRESH inode, and build from the stable installed `/usr/local/bin/bpp`, never the
just-overwritten `./bpp`.)

## Stages (each gate: bootstrap → INSTALL → 192/0/12 + 155/0/49-on-the-installed-binary + `bpp --c` smoke + tl_bench + binary-size + audio md5)

> **Status: Inc 1 SHIPPED (re-landed + fixed, 2026-06-18).** Two earlier attempts
> reverted (bloat; then a `--c` self-miscompile). The third landed: same size-gated
> approach, plus the real root-cause fix — `classify_inlineable` was comparing
> callee names with `cg_str_eq`, which reads the *codegen-time* `cg_sbuf` (0 during
> the analysis pass in the `--c` pipeline → SIGSEGV, confirmed via `bug`: cg_sbuf==0).
> New `_inline_name_eq` compares against the *parse-time* `vbuf` instead. Verified
> the proper way (bootstrap → INSTALL → suites + `--c` smoke on the installed binary).
>
> **Status: Inc 2 SHIPPED (2026-06-18) — the binding-path foundation.** Reframed
> from the original "float leaf" plan: the real first brick is letting a
> *single-return body with a multi-use param* inline at all. Such bodies were
> rejected at criterion 8 because the fast `ast_clone_subst` path shares arg nodes
> → a side-effecting arg would be re-evaluated per use. Inc 2 marks them
> `fn_inlineable = 2` (instead of rejecting) and routes them through the
> multi-statement binding splice `cg_emit_inline_multi`, which binds each arg to a
> temp ONCE. Shared infra — the whole filter chain AND `arr_struct_at` have
> multi-use params. `fn_inlineable` is read only truthy / `== 2`, so the new value
> is safe. **Binary flat (998514, identical to Inc 1 — no bloat), as predicted for
> pure foundation.** gen2==gen3 byte-stable + `--c` smoke clean (incl. the compiler
> self-emit that crashed Inc 1) + 193/0/12 + 156/0/49 on the installed binary.
> tl_bench ~15.4 ms (no regression; small win). Audio byte-identical to the trusted
> Inc 1 binary (corrected baseline `7ee452e7eb9237debafa7302b177d66c` — the old
> `8b8742ca…` predated a `tiny_lofi.bpp` edit and was stale).

Two gating mechanisms, used where each fits: a **size gate** (a thin body that's
size-neutral to inline can go everywhere) and a **hotness gate** (a bigger or
widely-used body inlines only at hot — in-loop — call sites). The size gate is
cheap and lands first; the hotness gate (per-call-site loop-depth) is built when
the first bigger/widely-used target needs it.

- **Inc 1 — size-gated inline-instruction builtins. ✅ SHIPPED (re-landed + fixed).**
  `_inline_is_inst_builtin` (peek/poke family) + a relaxed `_inline_has_tcall`
  (builtin calls don't disqualify a wrapper) + a tight `INLINE_BUILTIN_NODE_CAP`
  (10) so ONLY thin single-instruction wrappers qualify. **The fix that made it
  real:** name comparison in the analysis pass must use `_inline_name_eq` (over
  `vbuf`), NOT `cg_str_eq` (over the codegen-time `cg_sbuf`, which is 0 in `--c`).
  read_u16/write_u16 now inline (tl_render 6 bl/iter → 3); **binary flat** (998386
  → 998514, +128 B, no bloat). Audio byte-identical; native 192/0/12 + C-emit
  155/0/49 **on the installed binary** + `bpp --c` smoke + byte-stable bootstrap.

- **Inc 2 — binding path for multi-use-param single-return bodies. ✅ SHIPPED.**
  Mark such bodies `fn_inlineable = 2`; both backends skip the re-evaluating fast
  substitute path and fall to `cg_emit_inline_multi` (bind each arg to a temp
  once). The `_inline_pre_reg_walk` registers their callsites (`callee_bcnt > 1 ||
  fn_inlineable == 2`) so the binding path gets its per-callsite slot block. Pure
  foundation: enables the chain but needs the later bricks to collapse it.
  (Float-leaf inlining — the originally-planned Inc 2 — is still gated separately
  by the float-param rejection and folds into a later increment.)

- **Inc 3 — control flow (guard-clause) inlining + the HOTNESS GATE.** Unblocks
  `arr_struct_at` — the **biggest single call source** (~3.17M/render) but a
  *widely-used* accessor (83 call sites), so inlining it everywhere would bloat.
  Split into two verified bricks:

  - **Inc 3a — hotness-gate infrastructure. ✅ SHIPPED (inert).** New tier
    `fn_inlineable = 3` (= "hot-only"): the binding splice, but only at IN-LOOP
    call sites. The whole gate lives in the **spine** (`bpp_dispatch.bsm`): the
    pre-reg walk tracks `cg_inline_loop_depth` (reset per fn, bumped around
    T_WHILE) and registers a tier-3 callsite **only when depth > 0** — a cold
    site keeps `n.e == 0` and the codegen already falls through to a normal `bl`.
    The only per-backend edits are two mirrored one-liners: the chip fast-path
    gate is now `fn_inlineable == 1` (tiers 2 + 3 fall to binding). **Fully
    backend-agnostic** — `_inline_pre_reg_walk_body` runs for both a64 and x64,
    so no gap/stub. Nothing is tier 3 yet → inert: gen2==gen3 byte-stable,
    `--c` clean (incl. self-emit), 193/0/12 + 156/0/49 on the installed binary,
    tl_bench flat (16.9 ms). Binary 998514.

  - **Inc 3b — guard-clause acceptance, `arr_struct_at` as first consumer. ✅ SHIPPED.**
    `classify_inlineable` recognises the guard-clause shape
    `if (C) { return A; } return B;` (2 stmts, T_IF whose body is a single
    T_RET, no else, followed by a final T_RET) and normalises it to the
    single-return ternary `return C ? A : B;` (semantically exact — ternary
    only evaluates the taken branch), then marks it tier 3. After normalisation
    the existing tier-2/3 binding path (Inc 2) emits it — **no new early-return
    splice machinery** (the new ternary/ret wrapper nodes need no `itype`; the
    ternary emit derives its type from the branches). `arr_struct_at` inlines
    only inside loops (`tl_render` per-clip lookup, `bl` count 3→2) and stays a
    normal call at its ~80 cold sites. **Result: tl_bench 16.9→15.3 ms (~10%, the
    arc's first real win); cost +1.65% compiler binary, self-compile flat
    (0.24s).** Verified: gen2==gen3 + 193/0/12 + 156/0/49 on the installed binary
    + `--c` clean (incl. self-emit) + audio md5 `7ee452e7…`. The mutate-in-place
    is sound because `classify_inlineable` runs once per compilation (the two
    `bpp.bpp` call sites are the mutually-exclusive modular / monolithic arms).

- **Inc 4 — bottom-up + relax the T_CALL gate** (reverse-topo order, recursion
  guard) so a call to an already-inlinable function stops disqualifying its
  caller. Hotness-gated. **⚠ Investigation 2026-06-19 — two blockers found,
  re-scope before building:**

  1. **Nested binding-path inlining is an architectural gap, not a one-liner.**
     The callsite-id scheme stamps a global id on the *caller's own* call node
     (`cn.e`) and registers `_inl<id>_<param>` slots in the *caller's* frame; the
     splice clones the callee body (`ast_clone_subst` copies `.e`). For a NESTED
     inlinable call this breaks: the callee body AST is **shared** (used standalone
     + by every inliner), so its nested call's `.e` and its mangled slots belong to
     the *callee's* frame, not the outer caller's → the tier-2/3 binding path would
     read wrong-frame slots → **miscompile**. (Safe today only because the T_CALL
     gate blocks every body-with-a-call from inlining at all — which is exactly
     what Inc 4 relaxes; Inc 3b is unaffected because arr_struct_at is a leaf.)
     Tier-1 (fast-substitute, no slots) nested calls compose for free, so the
     *minimal* Inc 4 = relax T_CALL to "calls to TIER-1 callees only". Full
     tier-2/3 nesting needs the inline tree flattened at pre-reg: clone each
     inlinable callee body, stamp FRESH ids on the clone, register the clone's
     slots in the outer frame, and have the splice consume the pre-stamped clone.
  2. **The filter chain is multi-gate-coupled — T_CALL alone won't move it.**
     `flt_onepole_tick` (float), `moog_taps` (multi-return), `moog_slope` (switch),
     `tl_channel_process` (float + if) are each blocked by a DIFFERENT gate too.
     Collapsing the chain needs float-leaf inlining + Inc 5 (multi-return) + more
     control-flow + the nested-inline architecture, together — not T_CALL in
     isolation. Don't ship a T_CALL relaxation expecting a chain win; measure
     which (if any) real hot call sites a tier-1-only relaxation actually helps
     first (measure-don't-believe).

- **Inc 5 — multi-value-return splice.** Inline `moog_taps` (4 banked returns →
  4 result locals). Part of the coordinated filter-chain push (see Inc 4 note 2).

**Target:** ~16.6 → ~4.5 ms (the hand-inlined `tl_bench_flat` number); Inc 3b
took the first step (16.9 → 15.3). The chain collapse is the next big lever but
needs the coordinated push above. The remaining 4.5 → 2.2 ms is Frontier 2
(RegAlloc v2 / liveness, roadmap F.2).

## Discipline

Same as the codegen journey: one increment per commit; **the compiler is the
test harness** — `tl_bench` (perf) + `tiny_lofi --render` md5 (correctness) +
the two suites + byte-stable bootstrap, every step. Use `the_bug` (`--disasm` /
`--break`, see `docs/manual/debug_with_bug.md`) when a step regresses
byte-stability. Measure, don't believe.
