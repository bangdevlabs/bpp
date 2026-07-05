# Plan — the Inliner Arc (codegen Frontier 1)

**Status: CLOSED 2026-06-24** — Inc 1-8 shipped (leaf floats, arr_struct_at hot-only, control-flow bodies, S4 hotness propagation; `comp_process` bl 2→1; sf_bench 16.9→~9.8 ms across the arc with RegAlloc v2). Known future levers, deliberately NOT in this arc: trailing-return-EXPRESSION nested shapes (`sf_channel_process`/`moon_process`/`rotary_tick`) and re-running hot-constant promotion on spliced subtrees. Moved to legacy 2026-07-03.

**Goal:** close the **×3.7 inliner gap** the sound_fusion DAW exposed (16.6 ms
`sf_render` vs 4.5 ms hand-inlined vs 2.2 ms clang -O2; see benchmarks.md). Real
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
`sf_channel_process` (float + if + call), `arr_struct_at` (if + 2 returns).

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

## Stages (each gate: bootstrap → INSTALL → 192/0/12 + 155/0/49-on-the-installed-binary + `bpp --c` smoke + sf_bench + binary-size + audio md5)

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
> sf_bench ~15.4 ms (no regression; small win). Audio byte-identical to the trusted
> Inc 1 binary (corrected baseline `7ee452e7eb9237debafa7302b177d66c` — the old
> `8b8742ca…` predated a `sound_fusion.bpp` edit and was stale).

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
  read_u16/write_u16 now inline (sf_render 6 bl/iter → 3); **binary flat** (998386
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
    sf_bench flat (16.9 ms). Binary 998514.

  - **Inc 3b — guard-clause acceptance, `arr_struct_at` as first consumer. ✅ SHIPPED.**
    `classify_inlineable` recognises the guard-clause shape
    `if (C) { return A; } return B;` (2 stmts, T_IF whose body is a single
    T_RET, no else, followed by a final T_RET) and normalises it to the
    single-return ternary `return C ? A : B;` (semantically exact — ternary
    only evaluates the taken branch), then marks it tier 3. After normalisation
    the existing tier-2/3 binding path (Inc 2) emits it — **no new early-return
    splice machinery** (the new ternary/ret wrapper nodes need no `itype`; the
    ternary emit derives its type from the branches). `arr_struct_at` inlines
    only inside loops (`sf_render` per-clip lookup, `bl` count 3→2) and stays a
    normal call at its ~80 cold sites. **Result: sf_bench 16.9→15.3 ms (~10%, the
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
     `sf_channel_process` (float + if) are each blocked by a DIFFERENT gate too.
     Collapsing the chain needs float-leaf inlining + Inc 5 (multi-return) + more
     control-flow + the nested-inline architecture, together — not T_CALL in
     isolation.

  **Measured 2026-06-19 (throwaway env-gated probe `BPP_INLINE_PROBE`, reverted).**
  Counted functions blocked PURELY by the T_CALL gate whose calls are all to
  inlinable callees — the exact ceiling of a tier-1/T_CALL relaxation:
  - **sf_bench compile: 3 candidates** (`randi`, `sf_clip_count`, `sf_clip_at`) —
    ALL cold. `sf_render` calls `arr_struct_at` / `arr_struct_count` *directly*
    (already inlined by Inc 3b), not via the wrappers → **T_CALL relaxation gives
    ZERO sf_bench win.** And the chain functions never even reach the T_CALL gate
    (rejected earlier at float/multi-return/switch), so "1" alone wouldn't help
    the DAW either.
  - **self-compile: 33 candidates** (`diag_file_*`, `mod_bnd_*`, `fn_type_*`,
    `*_op_char`, …) — tiny compiler-internal accessors. At best a small
    *compile-time* micro-win, a different metric, and only after a 2-pass classify
    + accepting some bloat. Not worth it for the DAW goal.

  **Reprioritised → float-leaf inlining. ✅ SHIPPED.** The next real DAW lever was
  float-leaf, NOT T_CALL/nesting. `flt_onepole_tick(s,in,g)->float { return
  s+g*(in-s); }` is a pure matched-float leaf called 4×/sample by `moog_taps`.
  Three spine edits (agnostic, no chip changes): (1) `classify_inlineable` allows
  float return + float params, and forces any float-param function to tier 2
  (`has_float_param` — a float param must never take the fast substitute path,
  which can't convert the arg); (2) `_inline_register_callsite` types the mangled
  float-param slot (else the bind store writes a float through an int slot →
  FCVTZS truncation); (3) `cg_emit_inline_multi` force-binds float params (so the
  conversion happens at the typed-slot assignment, matching a real call). Collapses
  `moog_taps`'s 4 `bl` → 0 via the EXISTING Inc 2 binding path at one level
  (moog_taps is standalone — multi-return), so NO nesting architecture needed.
  **Result: sf_bench 15.3 → 13.1 ms (~14%; cumulative 16.9→13.1 = ~22%).**
  Compiler binary flat (no hot float leaves in the compiler), audio byte-identical
  (`7ee452e7…`), gen2==gen3, 193/0/12 + 156/0/49, `--c` clean. T_CALL/nesting
  (the "1" architecture) remains the LAST piece, after Inc 5 + control-flow.

- **Inc 5 — multi-value-return splice. ✅ SHIPPED 2026-06-22, mechanism only —
  zero measured sf_bench win, as predicted by item 2 above.** Lifted the
  blanket `fn_ret_arity > 1` reject in `classify_inlineable`
  (`bpp_dispatch.bsm:6226`) to a narrow shape check (trailing multi-return
  T_RET, arity matching the signature, forced tier 2); fixed the T_RET-multi
  blind spot in 4 AST helpers that previously read only `n.a` (the
  single-return field) — `_inline_count_nodes`, `_inline_has_tcall`,
  `_inline_param_refs`, and `ast_clone_subst` (the dangerous one: the
  blanket field copy left a cloned multi-return's expression array SHARED,
  unsubstituted, with the original — alpha-renaming would have silently not
  happened for any result); split `cg_emit_inline_multi` into a shared
  prelude (`_cg_inline_splice_prelude`) + two tails — the existing
  single-value/void tail, and a new `cg_emit_inline_multi_assign` that
  stores each cloned result directly into its caller-side target with NO
  bank push/pop (inlining erases the call boundary banking exists to cross).
  Added the caller-side dispatch check at the top of `T_ASSIGN`'s `n.d > 1`
  handler (`bpp_codegen.bsm:4385`) — **necessary**, not optional: without it,
  `n.b`'s generic `emit_node` dispatch was already routing a multi-return
  call through the OLD single-value `cg_emit_inline_multi`, which silently
  computed only the first result and left the rest reading garbage (caught
  by direct testing before the check existed — exactly the failure mode the
  arc's own "verify the bootstrapped compiler" lesson warns about, just one
  call layer deeper).
  **Verified mechanism correct** on a `moog_taps`-shaped synthetic leaf
  (`tests/test_multi_return_inline.bpp`, no internal calls): disasm shows
  the splice landing with zero `bl`s, B3-promoted float registers holding
  the intermediates, correct values across cold + hot call sites and a
  multi-use-param variant. 199/0/12 native, 160/0/51 C-emit (added
  `skip-c` to this new test + to `test_stbrotary.bpp`, which was ALSO
  hitting the pre-existing, unrelated C-backend multi-return-lowering gap
  and had simply never been run through `run_all_c.sh` before — confirmed
  via `git stash` that both failures reproduce identically on the
  pre-Inc-5 compiler). Audio export md5 unchanged
  (`f61fac72be5077a6e9ef9cae21dde2a1`). Binary +128 B.
  **But `moog_taps` itself still doesn't inline into `moog_slope`/
  `moog_tick`**: its SOURCE still literally calls `flt_onepole_tick` 4×
  (stbfilter.bsm:76-79) — those are real `T_CALL` nodes in the AST, and
  `_inline_has_tcall` walks the source AST, not the compiled output. That
  the 4 calls themselves compile down to 0 `bl` (float-leaf, already
  shipped) is irrelevant to THIS gate — it inspects what the callee's body
  *says*, not what it *compiles to*. Confirmed by disasm (`moog_slope` still
  shows a real `bl` + bank-pop sequence at the `moog_taps` call site) and by
  re-measuring sf_bench: 15.39 ms min-of-5, statistically unchanged from the
  15.29 ms pre-Inc-5 baseline. This is exactly item 2's prediction above,
  now confirmed by measurement rather than assumed: the filter chain is
  multi-gate-coupled, and Inc 5 alone — while a correct, necessary, tested
  piece of the puzzle — was never going to move sf_bench by itself. The
  remaining piece is the nested-inline architecture itemized in (1) above:
  clone each inlinable callee body, stamp fresh callsite ids on the clone,
  register the clone's slots in the OUTER frame, and let the splice consume
  the pre-stamped clone — so a call to an already-inlinable callee (like
  `flt_onepole_tick` inside `moog_taps`) stops disqualifying its caller.

- **Inc 6 — nested-inline architecture. Commit A + B ✅ SHIPPED 2026-06-22;
  Commit C (flip `moog_taps` for real) not started yet this session.**
  Staged per the plan at `docs/plans/silly-leaping-sunbeam.md` (the
  architecture writeup — pre-reg-time discovery, never mutating the
  shared callee body, `ast_clone_subst` resetting a cloned T_CALL's `.e`
  to 0 by default — lives there in full; this entry is the ship log).
  - **Commit A** (`abe3185`): the SAME T_RET-multi blind spot Inc 5 fixed
    in 4 helpers, missed in a 5th — `_inline_pre_reg_walk`
    (`bpp_dispatch.bsm:6053`) still read only `n.a`. Fixed identically.
    Byte-identical binary (this walker wasn't reachable from anything
    that mattered yet), zero behavior change, ships alone so nothing
    later is a suspect if something regresses.
  - **Commit B**: the mechanism itself. `classify_inlineable`'s single
    pass became a fixpoint (`while(changed && iter<8)`; B++ has no
    `do-while`, so it's a pre-seeded `while`) — needed because the
    relaxed gate (`_inline_has_disqualifying_tcall`, a sibling of
    `_inline_has_tcall` that exempts a call to an ALREADY-`fn_inlineable`
    target) reads OTHER functions' `fn_inlineable`, which a callee
    declared after its caller wouldn't have yet on one pass. New
    `_inline_register_callsite_slots`/`_inline_find_nested_calls`/
    `_inline_register_nested` register a nested call's own param/local
    slots into the SAME outer frame, read-only on the callee's shared
    body (never stamping `.e` there) — splice time
    (`_cg_inline_splice_prelude`) walks its OWN fresh clone with the
    same deterministic walker and stamps the pre-registered ids
    positionally via a cursor that persists across the whole
    per-statement loop (a first draft that reset the cursor per
    statement was a real bug, caught before shipping — every statement
    after the first nested call would have been stamped with the WRONG
    id). Two real bugs surfaced by REAL pre-existing code, not just the
    new synthetic test:
    1. `ast_clone_subst`'s blanket `cl.e = orig.e` copy was dead code
       before Inc 6 (an inlinable body was previously GUARANTEED
       call-free, so this line never ran on a T_CALL node) — once the
       gate relaxed, `stbmixer.bsm`'s `mixer_get_rotary() { return
       rotary_get_mode(_mx_rotary); }` (a single-statement tier-1 body
       whose ENTIRE statement IS a call to a multi-statement callee)
       became newly eligible, and its tier-1 fast path (unrelated,
       pre-existing code in `a64_codegen.bsm`) cloned that inner call
       node verbatim — carrying a callsite id meaningful only within
       `mixer_get_rotary`'s OWN compile into whatever frame
       `mixer_get_rotary` later got spliced into. `test_mixer_rotary`
       caught it (`extrn '_inl1_r' has no backing definition`). Fix:
       `ast_clone_subst`'s T_CALL case now resets `.e` to 0 explicitly;
       only an explicit re-stamp (this increment's own mechanism) sets
       it again.
    2. `test_audio_tone.bpp`'s cb_count window — unrelated to the
       inliner, surfaced only because it happened to run in the same
       suite pass; see that file's own header comment for the full
       investigation (ruled out a recurrence of the 2026-06-21 float-
       device-boundary bug; actual cause was a zero-margin fixed ceiling
       plus, after the first fix attempt, a second issue where deriving
       BOTH bounds from elapsed wall-clock time broke under
       `run_all_c.sh`'s 6-way parallel contention).
  - Verified on `tests/test_inline_nested.bpp` (two independent outer
    leaves calling the same nested callee, each with its own id
    sequence — the exact multi-caller-collision shape Inc 6 exists to
    make safe): disasm confirms zero `bl` in either outer. 200/0/12
    native, 160/0/52 C-emit, audio md5 unchanged, bootstrap byte-stable
    from gen2 (gen1≠gen2 is the documented 1-cycle oscillation — the
    compiler's OWN source has a qualifying nested-call shape now too,
    so gen1, built by a compiler without the mechanism yet, differs from
    gen2 onward, which is stable).
  - **Commit C ✅ SHIPPED 2026-06-22 — needed two more pieces, not zero**
    (the plan's "expect zero further source changes" prediction was
    wrong; corrected here by measurement, not papered over). Direct
    trace of `classify_inlineable` (temporary, reverted before
    committing) showed `moog_taps` still rejected at the node-count
    cap: `body_cost=84` against `INLINE_BODY_NODE_CAP=50`.
    1. **`perf(inliner): const-offset addressing` (`5cb1c32`)** —
       `_inline_count_nodes` counted a struct-field/fixed-index load's
       address (`T_BINOP('+', base, T_LIT)`, the canonical shape
       `parse_struct_field` always emits) as 3 nodes on top of the load
       itself, as if the `+` and the offset literal each became their
       own instruction. They don't — base+constant-offset folds into
       ONE load/store instruction's addressing mode on both chips, same
       as a plain `*ptr`. New `_inline_is_const_offset_addr` recognizes
       the shape; `_inline_count_nodes`'s T_MEMLD/T_MEMST cases count
       just the base. Drops `moog_taps`'s `body_cost` 84 → 62 — a real,
       general improvement to the cost model's accuracy (11 of
       moog_taps's struct-field accesses alone were over half the old
       total), not a moog_taps-specific carve-out. +48 B, fully green.
    2. **The scoped multi-return cap** — 62 is still over 50. Considered
       (user pushback, rightly) and rejected: just raising
       `INLINE_BODY_NODE_CAP` globally, since that widens blast radius
       for every single-return candidate too, none of which need it.
       Considered and rejected: a formula scaling the cap by
       `ret_arity`, since with exactly ONE real data point (arity 4 →
       62) any per-arity coefficient is equally unfalsifiable — curve-
       fitting through one point, dressed up as a formula instead of a
       constant, not more principled than the constant. Shipped: a
       SEPARATE `INLINE_MULTIRET_NODE_CAP = 70`, gated on `ret_arity > 1`
       only — single-return bodies keep exactly the original 50,
       untouched. The real, principled fix (LLVM-style: weight each AST
       node by its actual instruction cost instead of flat 1-each) is
       bigger than this evidence justifies building today; the struct-
       addressing fix above is a step in that direction, not the whole
       thing.
    Verified: `bug --disasm` on `moog_slope`/`moog_tick` shows **zero
    `bl`** at the `moog_taps` call site in both — full inlining, finally.
    Audio export md5 unchanged. 200/0/12 native, 160/0/52 C-emit,
    bootstrap byte-stable. **sf_bench: NO measurable movement** (~15.3-
    15.5 ms min-of-5, same noise band as before Commit C) — eliminating
    this one `bl` per channel per sample was not a measurable fraction
    of `sf_render`'s real cost. This is itself the answer to Part 2's
    decision rule below: `ours` did NOT converge toward `flat`, so the
    remaining gap is NOT primarily inliner-shaped at this point — see
    Part 2's first branch.

**Target:** ~16.6 → ~4.5 ms (the hand-inlined `sf_bench_flat` number, as it
stood pre-rotary); Inc 3b took the first step (16.9 → 15.3), float-leaf the
second (15.3 → 13.1 ms, *before* the rotary insert existed). The rotary
(slice 5, 2026-06-22) then added its own per-sample `bl` back, and re-syncing
`sf_bench`/`sf_bench_flat` to the real post-rotary, post-stereo project
re-based the table at **15.29 ms (ours) / 5.06 ms (flat) / 3.26 ms (oracle)**
— Inc 5 shipped the multi-return mechanism against THIS baseline and
measured no movement (15.29 → ~15.4 ms, noise). Inc 6 (Commits A-C) then
shipped the nested-inline architecture AND got `moog_taps` to fully
collapse into `moog_tick`/`moog_slope` (zero `bl`, confirmed by disasm) —
and STILL measured no movement (~15.3-15.5 ms min-of-5). Two increments in
a row closing real, verified inliner gaps with zero `sf_bench` effect is
itself the signal: the remaining `ours`-vs-`flat` gap is concentrated
somewhere call-overhead elimination doesn't reach — most likely
`moog_slope`'s own `switch` dispatch, `rotary_tick`'s if/else, or
`sf_channel_process`'s surrounding loop structure, none of which the
inliner can touch without first extending past the existing control-flow
gate (a materially bigger, separately-scoped project, not a natural next
increment of this one). Per the plan's own decision rule: **re-disassemble
`sf_bench`'s actual hot loop before opening anything else** — find out
empirically where the remaining cycles go rather than assuming it's "more
inlining" just because that's the tool already in hand. The flat/oracle
gap (~1.5-1.6x) is unchanged by any of this (neither `sf_bench_flat` nor
the oracle calls `moog_taps`) and remains Frontier 2's territory (RegAlloc
v2 / liveness, roadmap F.2, `docs/plans/compiler_boost_roadmap.md`) —
unopened, ungated change from before.

## Discipline

Same as the codegen journey: one increment per commit; **the compiler is the
test harness** — `sf_bench` (perf) + `sound_fusion --render` md5 (correctness) +
the two suites + byte-stable bootstrap, every step. Use `the_bug` (`--disasm` /
`--break`, see `docs/manual/debug_with_bug.md`) when a step regresses
byte-stability. Measure, don't believe.

## Addendum (2026-06-22/23) — the decision rule's answer

Followed the plan's own closing instruction: re-disassembled `rotary_tick`
(stb/stbrotary.bsm) instead of opening another inliner increment. Found
the real bottleneck was NOT call overhead at all — it was struct-field
addressing and F.2.c's compute-in-place gate never covering struct-field
leaves or comparison operators. Four commits, in order:

1. `e9fbb8d`/`7149082`/`851b8e1` — const-offset addressing: fold a
   struct-field load/store's `base + literal` address into the
   instruction's own offset, and skip copying an already-promoted base
   into the accumulator when the address can read/write through it
   directly. `sf_bench` 15.4 → 13.76 ms.
2. `2ad0685` — extend F.2.c's float compute-in-place
   (`cg_float_tree_need`/`cg_emit_float_into`) to `T_MEMLD` (struct-field)
   leaves, mirroring the proven integer-CIP precedent. `13.76 → 13.08 ms`.
3. `60a59cb` — extend compute-in-place to float comparisons (`< > <= >=
   == !=`), which had no precedent on either side: `_a64_emit_cmp_flt`'s
   `fcmp d1, d0` is a fixed-register convention, not a hardware
   requirement, and the save/resolve dance upstream of it exists only to
   satisfy that convention. `13.08 → 13.05 ms` (within noise — this
   benchmark's hot path leans on struct-field arithmetic more than
   comparisons, reported honestly rather than assumed).

`rotary_tick`'s own disassembly after commit 3: 35 → 25 `fmov`. The
remaining shuffle was almost entirely on expressions with a float LITERAL
operand (`r.cur + 0.0001`, `x > 1.5`, threshold checks against named
constants) — `cg_float_tree_need` had no `T_LIT` leaf case on either the
arithmetic or comparison path, a third, distinct gap from the two closed
above.

4. `b81bd23` — extend compute-in-place to float literal leaves. A float
   literal is always cheap to materialize (no register contention, just
   a 3-instruction const-pool load); new `emit_fconst_into(dreg, flt_id)`
   parameterizes the existing `emit_load_float_const`/`cg_emit_lit` const-
   pool load over the destination instead of the fixed accumulator. Slots
   into `cg_float_tree_need` generically, so both the arithmetic CIP
   (commit 2) and the comparison CIP (commit 3) picked it up with zero
   further changes to either — confirmed by reading both call sites.
   `13.05 → 12.07 ms` — the single biggest commit of the whole
   investigation, confirming the disasm-driven prediction that this was
   the dominant remaining lever.

**`rotary_tick`'s own disassembly: 35 → 5 `fmov` (86% reduction).
`sf_bench`: 15.4 → 12.07 ms min-of-5 (~22% across the four float-CIP
commits).** No further float-CIP leaf gap is known; the struct-field/
comparison/literal trio is, as far as re-disassembly shows, complete.

5. `6cdff89` — float comparison branch fusion. `cg_emit_cmp_to_branch`
   (the shared T_IF/T_WHILE condition-fusion gate) already fused integer
   comparisons into `cmp + b.cc` with no boolean materialization; it
   explicitly bailed out for floats, falling back to `fcmp + cset + cbz`.
   Extended it by reusing the float-CIP machinery (`cg_try_float_cmp` for
   eligibility, a new shared `cg_float_cmp_operands_into` helper, a new
   `emit_cmp_flt_branch` primitive mirroring `emit_cmp_branch_rr`).
   `rotary_tick`: 6 of the remaining `fcmp`/`cset`/`cbz` triplets fused to
   `fcmp`/`b.cc` (175 → 169 instructions). **`sf_bench`: 12.07 → 12.11 ms
   — no measurable movement, reported honestly.** A `cset` feeding an
   immediately-following `cbz` sits off the critical dependency path on
   an out-of-order core (the branch predictor speculates past it
   regardless), so removing one non-blocking instruction per comparison
   doesn't show up in wall-clock time — the same lesson this manual
   already documents for integer loop control on the biquad kernel
   (`docs/manual/bootstrap_manual.md`, "Codegen Quality and the
   Register-Allocation Frontier"). The struct-field/comparison/literal
   trio's gains were real because they removed work ON the critical path
   (extra loads, extra `fmov` copies feeding the next dependent op); this
   one doesn't. Verified correct regardless (boundary `<=`/`>=` cases
   tested explicitly, both `if` and `while` call sites) — a real,
   shipped, disasm-confirmed change, just not a measured speedup on this
   benchmark.

**This closes the branch-fusion lever named at the end of the float-CIP
trio.** No further named, unstarted lever from this investigation
remained at that point — a fresh re-disassembly was needed to find the
next one, which the user explicitly asked for (after first re-checking
`examples/bench_codegen.bpp` against the `--c` + `gcc -O2` oracle to
confirm the float-CIP trio hadn't regressed the canonical kernels — it
hadn't: lcg 0.99x, xform 1.08x, biquad 1.01x, all within the documented
parity band).

6. `832e75b` — typed push/pop for call arguments. Re-disassembling
   `sf_channel_process` (one call deep from `sf_render`, not
   `rotary_tick` this time) found a different, much bigger-in-scope
   pattern: every call argument was pushed through the stack as raw
   integer bits (a float bit-reinterpreted via `fmov` first), popped
   back positionally, then a separate rearrange pass routed each one
   into its real AAPCS64/SysV bank — paying 2 extra `fmov`s per float
   argument plus a `mov` for any int argument whose bank position
   differed from its push position. The fix already existed, proven, on
   both chips, for the `call()` builtin (FFI by raw function pointer):
   record each argument's type into a small `types_buf`, push through
   the type-matched primitive, pop in reverse order routing directly
   into the final ABI register via the spine's `cg_count_tags`. Ported
   that exact pattern to `a64_emit_call`/`x64_emit_call`'s "Regular B++
   function call" path; `cg_emit_call_arg_rearrange` and its two
   primitives are now dead and deleted.

   Verification surfaced a real, **pre-existing** bug (confirmed via a
   direct disasm diff against a pre-change build — same wrong final
   register state in both, not introduced today): `cg_find_ext_by_argc`
   matches an FFI extern by name + arg count only, and
   `_stb_platform_macos.bsm` declares `objc_msgSend` with two different
   6-arg overloads (`ptr,ptr,double,double,double,double` vs
   `ptr,ptr,int,ptr,ptr,int`) — the lookup always picks the first one in
   source order, silently routing a real `NSDate*` argument into a float
   register for the Cocoa event loop's `nextEventMatchingMask:...` call.
   This had been surviving by coincidence (whatever garbage was left in
   the wrongly-unused integer register happened to be harmless); the
   call-argument rewrite changed nearby register timing enough that it
   stopped being harmless, crashing every test that opens a real window
   (`test_gpu_clear`/`shapes`/`circle`/`rect`/`atlas_aseprite`,
   `test_stbgame_native`) with `-[__NSCFNumber timeIntervalSinceNow]:
   unrecognized selector`. Fixed by no longer consulting the (possibly
   ambiguous) declared param type for FFI argument ROUTING at all —
   route by the argument's own evaluated type instead, since every real
   call site already passes explicitly-typed literals matching the
   intended param. `cg_ext_par_is_float` is now unused, removed.

   `sf_channel_process`: the `fmov` bounce around its `rotary_tick` call
   is gone, confirmed via disasm. **`sf_bench`: 12.07 → 11.39 ms (~5.6%)**
   — unlike branch-fusion, this removes work that WAS on the critical
   path (the extra `fmov`s feed directly into the call). 206/0/12 native
   (every GPU/window test green again), 166/0/52 C-emit, audio md5
   unchanged, bench_codegen vs gcc -O2 unchanged.

**Running total at this point: `sf_bench` 15.4 → 11.39 ms (~26%).**

7. `1d68b21` — fixed a separate, genuinely pre-existing bug found while
   chasing the next lever via fresh re-disassembly of `moog_tick` (one
   level of nested inlining deeper than `rotary_tick`/`sf_channel_process`
   — the user explicitly asked to fix it once found, even though it
   predates today: "mesmo sendo pre-existente vamos consertar").
   `ast_clone_subst`'s `T_ASSIGN` case never got the same per-element
   clone the `T_RET` case already has for its own multi-value array —
   the blanket `cl.c = orig.c` copy left a multi-return assignment's
   TARGETS (`a, b, c, d = f()`) shared, unrenamed, with the original.
   A function whose body does exactly this (`moog_tick`'s own
   `a,b,c,d = moog_taps(m,in); return d;`) returns garbage once IT
   ITSELF gets inlined one level further out: the renamed clone's
   untouched target names no longer resolve via `cg_var_idx` in the
   outer frame, so the float/int unpack decision defaults to integer
   (`fcvtzs` truncating the result). Confirmed live, not theoretical:
   `examples/moog_demo.bpp` — the worked example for this exact filter —
   had been rendering complete silence (peak 0/32767) for as long as
   `moog_tick` existed, since nothing in the test suite exercises this
   nested shape. Fixed by mirroring `T_RET`'s own fix exactly. After:
   peak 17561/32767, audible. `sf_bench`/audio md5 unaffected (`moog_tick`
   itself isn't on sound_fusion's render path — `moog_slope` is — so the bug
   was dormant for the DAW specifically).

8. `99e504e` — the lever the re-disassembly was actually chasing: with
   commit 7 fixed first (needed to even verify this one), relaxed
   `_cg_inline_splice_prelude`'s per-param substitution check. It had
   two blanket exclusions beyond what correctness requires: a FLOAT
   param ALWAYS bound regardless of the argument's own type (even when
   already matching, no conversion to skip), and only `T_VAR`/`T_LIT`
   arguments were eligible at all (a struct-field `T_MEMLD` read always
   bound too, even though it's exactly as idempotent when its address is
   the canonical `var + const-offset` shape — `cg_try_const_offset`,
   already proven elsewhere this session). Found on the exact same
   `moog_taps` re-disassembly: every struct-field (`mm.s1`..`mm.s4`) and
   every local (`g`, the running pole result) bound unconditionally,
   spending a stack round-trip per pole for values already sitting in
   clean registers or a one-instruction struct load. A second, smaller
   gap surfaced fixing this one: a freshly-renamed param/local's mangled
   substitute node needs its real type copied from `cg_var_forced_ty`,
   or a FURTHER nested splice sees `TY_UNK` and the new type-match check
   always treats it as a mismatch. `moog_tick`: 119 → 86 instructions
   (zero stack spills around any of the four poles, confirmed via
   disasm). **`sf_bench`: 11.39 → 10.16 ms (~10.6%) — the single biggest
   commit of the whole investigation.**

**Running total at this point: `sf_bench` 15.4 → 10.16 ms (~34%).**

9. `8bbb905` — closed the doc's own predicted "needs control-flow +
   nested-inline" gap for the remaining chain link. Re-disassembling
   `moon_process`/`moog_slope` (one hop further down the chain than the
   `moog_taps` work above) found `moog_slope` rejected outright by
   `classify_inlineable`'s very first gate — its body ends in a `switch`
   (one arm per pole count), not a `T_RET`. New
   `_inline_normalize_switch_ret` rewrites a trailing switch where every
   arm is exactly one `return` into a single nested-ternary return — the
   switch's twin of Inc 3b's existing guard-clause normaliser. Surfaced a
   second bug while computing the now-reachable body's cost:
   `_inline_count_nodes`'s `T_ASSIGN` case never got the multi-TARGET
   sibling of the multi-RESULT fix Inc 5/6 already applied to `T_RET` —
   fixed the one call site that matters today, left the three siblings
   (`_inline_has_tcall`/`_inline_param_refs`/`_inline_writes_param`)
   documented-but-untouched since every current candidate's multi-target
   is a plain local (provably inert, not fixed speculatively).

   Even with `moog_slope` eligible, `moon_process`'s call into it still
   didn't nest: `moon_process`'s entire body is `pp=p; return
   moog_slope(...);` — the nested call lives INSIDE the trailing return,
   and Inc 6's nested-call discovery/consumption both deliberately
   excluded the trailing return ("`moog_taps`'s own `return a,b,c,d;`
   has no calls in it, matching this limit exactly" — true then, not
   anymore). Extended both sides symmetrically (discovery: one more call
   to the already-correct `_inline_find_nested_calls`, now also applied
   to the trailing statement; consumption: a new shared
   `_inline_stamp_nested` helper used by both splice tails, continuing
   the same id sequence the prelude's per-statement loop started).

   Verified via `bug --disasm` on the REAL target, not just the new
   synthetic test: `sf_channel_process`'s only remaining `bl` is
   `rotary_tick` (an unrelated plugin) — `moon_process → moog_slope →
   moog_taps → flt_onepole_tick` is now fully spliced, zero `bl`,
   cold and hot call sites alike.

   **`sf_bench`: controlled same-session A/B (git stash, rebuild,
   measure both back to back) — 10906µs → 10774µs min-of-10, a real but
   modest ~1.2%.** Smaller than the `bl`-count collapse alone would
   suggest — the same lesson as the branch-fusion commit above: call/
   return overhead on this target is cheaper than the node-count metric
   implies. Shipped anyway because it's a correct, real capability (the
   nested-inline mechanism now reaches one more genuine shape), not
   because the number demanded it.

**Running total across the whole "redisassemble instead of guess" arc:
`sf_bench` 15.4 → ~10.77 ms (~30%, noise band ±~1-2% at this point — see
commit 9's controlled A/B for the tightest recent measurement).** No
further named lever remains; the next one needs another fresh
re-disassembly. Per the user's own explicit direction (2026-06-23):
**RegAlloc v2 (real liveness-based register allocation) opens next, as
its own dedicated arc — not another inliner increment.**

## A second `bl` returns — the next named lever, found 2026-06-24

RegAlloc v2 (above) shipped and closed in the same session it opened
(see its own arc). The same day, the channel-strip work (sound_fusion
→ zener_comp, the TG12345-style compressor) reintroduced exactly the
shape this arc's own opening paragraph described: `sf_channel_process`
disassembles with `rotary_tick` AND `zener_process` as its only two
remaining `bl`s. Traced precisely (not guessed): `comp_process` calls
`amp_to_db_f`/`db_to_amp_f` (real `bl`s), which themselves call
`log_f`/`exp_f` (real `bl`s) — a clean two-level chain of un-inlined
calls, the same `moon_process → moog_slope → moog_taps →
flt_onepole_tick` shape this arc already closed once.

The wall is NOT a size overflow — `exp_f` is a true leaf (zero calls in its
own body) yet still doesn't qualify. `_inline_count_nodes_body`
(bpp_dispatch.bsm ~line 7339) returns a deliberate sentinel **99** for any
body containing `T_WHILE`/`T_IF`/`T_SWITCH`/nested `T_BLOCK` — "control-flow
inlining is out of scope per Q1," by design, not by accident.

## Increment 7 — control-flow leaf bodies, shipped same day, two real bugs found

Actioned the same day it was found, per the user's own explicit push
("a gente cria os programas pra estressar a infraestrutura... foi assim
que chegamos aonde chegamos em 3 meses"). `_inline_count_nodes` (and its
five siblings — `_inline_has_disqualifying_tcall`, `_inline_param_refs`,
`_inline_memst_count`, `_inline_indirect_call_count_node`,
`_inline_has_inst_builtin`, `_inline_find_nested_calls`) gained explicit
`T_IF`/`T_WHILE`/`T_BLOCK` recursion instead of the blanket 99 sentinel.
`ast_clone_subst` and `cg_emit_stmt` already handled both node types
correctly (proved by the outlining arc + this splice's own per-statement
loop) — the ONLY missing piece, in principle, was these counters refusing
to look inside.

In principle. Re-disassembling the SELF-HOSTED COMPILER (not a synthetic
test) surfaced two real, dangerous, previously-latent bugs the moment
control-flow bodies actually became eligible at this codebase's real
scale:

1. **Infinite recursion via a cyclic nested-inline graph.**
   `_inline_register_callsite_slots` <-> `_inline_register_nested`
   recurse into every nested inlinable call's OWN nested calls, with a
   comment that explicitly assumed an ACYCLIC call graph ("bounded by
   the actual, acyclic call graph, no artificial depth cap needed") —
   true while only pure single/multi-statement leaves could ever
   qualify (no two of those in this codebase happened to call each
   other), false the instant a loop-bodied PAIR does. The self-hosted
   compiler hung rebuilding ITSELF (gen2 → gen3) — not on a contrived
   test, on its own 20K-line source. Fixed with `_inline_registering`,
   an in-progress set checked before each recursive descent — a nested
   call back to a function already mid-registration breaks the cycle
   (that one occurrence's OWN nested calls don't get walked again; the
   call site itself still gets a valid id and splices normally — the
   same "falls through to a real `bl`, safe but not maximally optimal"
   degradation already documented on `.e`'s reset elsewhere).

2. **Early returns inside a spliced if/while corrupt control flow.**
   `cg_emit_stmt`'s `T_RET` handler is unconditional — "evaluate return
   expression... jump to the epilogue" — with no notion of "currently
   splicing an inlined body." A trailing return is fine (the splice
   pulls its EXPRESSION out directly, never walks it through
   `cg_emit_stmt`); a return buried inside a non-trailing if/while
   (`is_alpha`/`is_digit`-shaped: `if (cond) { return X; } ... return
   Y;`, used by the LEXER on every character) is not — once spliced,
   that `ret` jumps to the WRONG function's epilogue entirely. This
   one didn't hang at compile time; it produced a gen2 binary that hung
   on EVEN a trivial 4-line "hello world" input, because lexing anything
   calls `is_alpha` immediately. Found by bisecting with a minimal
   standalone repro (NOT by staring at the 20K-line compiler) once the
   self-hosted hang pointed at "something this fundamental." Fixed with
   `_inline_body_has_nested_ret`, an explicit recursive check added to
   every `T_IF`/`T_WHILE`/`T_BLOCK` case in `_inline_count_nodes` —
   disqualify, don't try to teach the splice to rewrite early returns
   into structured control flow (a real future increment, not a
   one-line fix).

   Verified by hand: `repro1.bpp` (continue inside an inlined loop,
   nested inside the CALLER's own loop, to make sure break/continue
   target resolution survives splicing too) settles to the
   hand-computed value exactly. Break/continue were never the bug —
   only the unconditional early-return-as-real-jump was.

   A THIRD, quieter gap surfaced by the same stress test: a local
   declared INSIDE a loop body (not at the function's own top level —
   the autovec SIMD-synthesis targets' `auto c: Cell;` inside their own
   `for`, `tests/test_autovec_compose.bpp`) was never collected for
   alpha-renaming, so its references fell through as an unresolved bare
   name (E264, "extrn has no backing definition"). Fixed the same way —
   `_inline_collect_locals`/`_inline_register_callee_locals` gained the
   identical `T_IF`/`T_WHILE`/`T_BLOCK` recursion.

   **The capability genuinely works now** — dozens of small control-flow
   leaves throughout the compiler's own source (`is_int_type`, `clamp`,
   `arr_pop`, `floor_f`, `sqrt`, `struct_field_name`, `cg_b3_eligible`,
   and many more) now actually splice at their real call sites, verified
   via the same re-disassembly discipline this whole arc runs on.

   **`exp_f`/`log_f` specifically still don't make the cut — measured,
   not guessed:** `exp_f`'s body costs **78** nodes (cap is 50 — its
   3-while range-reduce-then-series shape is genuinely bigger than any
   existing single-return candidate, not a bug); `log_f` costs 99 — its
   own `if (x <= 0.0) { return 0.0; }` domain guard is EXACTLY the
   early-return shape increment 7 disqualifies, correctly. `amp_to_db_f`
   (cost 12) and `db_to_amp_f` (cost 5) are individually tiny and
   control-flow-clean, but each calls a callee that doesn't qualify
   (`log_f`, `exp_f`), so Inc 6's composability rule correctly declines
   to wave them through — a call to a non-inlinable callee still
   disqualifies, exactly as it always has. `sf_channel_process`'s `bl`
   count is unchanged at 2 (`rotary_tick`, `zener_process`) — the
   capability shipped; this ONE named example just doesn't clear the
   existing bars. Raising `INLINE_BODY_NODE_CAP` specifically to admit
   `exp_f` was considered and deliberately NOT done — one measured
   candidate is not the two-consumer bar this codebase's own promotion
   discipline asks for (Tonify Rule 20's spirit applied to a compiler
   constant, not just a stb module), and a global cap raise widens
   every OTHER existing candidate's blast radius for a problem only one
   function has. Revisit with its own measurement if a second
   real candidate needs the room.

Bootstrap byte-stable (gen2==gen3, confirmed AFTER both bug fixes — the
hang and the corruption were both pre-fix-only), suite 219/0/12, C-emit
176/0/55, all regalloc/autovec gates green, real games + sound_fusion +
zener_comp compile clean, sound_fusion's audio md5 unchanged.

## Increment 8 — wire up S4 + call-graph hotness propagation, same day

The user pushed back on Inc 7's "measured, not a bug, didn't raise the
shared cap" conclusion: a hand-picked node-count ceiling is the wrong
shape of answer when nobody knows what a real b++ program will look
like. Pointed at the dormant **S4 cost model** (`_inline_cost`,
`_inline_threshold` — designed, never wired to a consumer, sitting in
this same file since before this session) as evidence the codebase
already half-agrees. Confirmed by hand that wiring S4 AS DESIGNED still
would not have admitted `exp_f`: its only hotness signals are SHALLOW
(does THIS call site sit in a textual loop; does the caller have 10+
direct callers) — neither one reaches `exp_f`, five calls below the
actual hot loop (`sf_render`'s per-sample mix -> `sf_channel_process` ->
`zener_process` -> `comp_process` -> `db_to_amp_f` -> `exp_f`). User's
call: build BOTH — S4 wiring AND a transitive hotness signal that
reaches the real loop.

**Shipped same day.** Three pieces:

1. **`fn_max_call_loop_depth` + `fn_hot_transitive`** (new, in
   `call_graph_build`/`_cgb_walk`) — the max parser-stamped loop depth
   (`T_CALL.d`, already on every call node, S4 P0a) among every DIRECT
   call site into a function, then propagated transitively over
   `_fn_callers` (a fixpoint, same shape/cap as `classify_inlineable`'s
   own): if G is hot and G calls F, F is hot too. Built for free inside
   `_cgb_walk`'s existing T_CALL discovery — no new walk.

2. **Tier 4 — "oversized, cost-gated"** (`classify_inlineable`). A body
   over the existing free cap (50/70) no longer means outright
   rejection: under a new, generous backstop (150/210 — round, not
   measured; nothing needs it tuned yet) it becomes tier 4, admitted
   ONLY per-callsite via `_inline_cost(...) <= _inline_threshold(...)`.
   Tiers 1-3 are completely unchanged — tier 4 only ever ADDS candidates
   that were previously rejected outright. `_inline_threshold` gained a
   third multiplier, `×3` when the caller is `fn_hot_transitive` — bigger
   than the existing `×2` (fanout) / `×1.5` (textual loop) because
   "provably reachable from a real loop, at any depth" is strictly more
   information than either shallow signal.

3. **Threaded `owner_fidx` through the whole nested-call-discovery path**
   (`_inline_find_nested_calls`, `_inline_register_nested`,
   `_inline_stamp_nested`, `_cg_inline_splice_prelude`) — tier 4's cost
   check has to fire identically at registration time AND splice time, or
   the positional `nested_ids[nk]` stamping silently reads the wrong id
   for the wrong call. This is the part that actually matters for
   `exp_f`: it is never called directly from a hot top-level site, only
   NESTED inside `db_to_amp_f`'s own body — without this threading, the
   tier-4 check would only ever run at the (irrelevant) top level.

**Two bugs caught before this shipped, both via `bug` per
`docs/manual/debug_with_bug.md` rather than ad-hoc prints once pointed
at the right tool:**

- **A genuine SENTINEL bug, not a logic error.** `_inline_count_nodes`
  returns **99** as a "fully disqualified" sentinel (T_SWITCH, an early
  return inside control flow — Inc 7's own fix). 99 was always smaller
  than the OLD caps (50/70), so a plain `> CAP` check rejected it for
  free. The new HARD backstop (150/210) is BIGGER than 99 — so the same
  comparison silently let a disqualified body through as "merely
  oversized." A T_SWITCH-bodied function got promoted to tier 4 and
  crashed compiling `sound_fusion.bpp` (segfault, `_a64_emit_switch_jtbl`
  — jump-table emission has no splice support, was never supposed to be
  reached this way). `bug --tui --break-all` caught it in one run — the
  backtrace landed exactly on the crash frame. Fixed with an explicit
  `if (body_cost >= 99) { continue; }` ahead of the backstop comparison,
  independent of its exact value.
- Lesson generalizes: any time an existing magic number doubles as both
  "a real measurement" and "a sentinel," a SECOND magic number added
  nearby (here, a backstop meant to be generous) can silently change
  which one wins. Check sentinels explicitly; never let them ride on
  "happens to be smaller than the old threshold."

**Measured result**: `comp_process`'s `bl` count dropped from 2 to 1.
`db_to_amp_f` (cost 5) — once `fn_hot_transitive` propagates hot from
`sf_render`'s per-sample loop, through `sf_channel_process` ->
`zener_process` -> `comp_process`, all the way down to `db_to_amp_f` —
now inlines into `comp_process` directly, AND its own nested call to
`exp_f` (cost 78, tier 4, cleared via the SAME propagated hotness)
inlines too: the disassembly shows `exp_f`'s full range-reduce-and-series
body spliced in, zero `bl` anywhere in that chain. `amp_to_db_f` still
doesn't inline — correctly: it calls `log_f`, still disqualified by its
own early-return domain guard (Inc 7), and Inc 6's composability rule
correctly declines to wave through a call to a non-inlinable callee
regardless of tier or hotness. Binary size: byte-identical on
`sound_fusion`'s total size (content hash differs — net code-size delta
from this one swap is effectively zero, call overhead removed roughly
offsetting the duplicated body).

Bootstrap byte-stable (gen2==gen3, confirmed after the sentinel fix),
suite 219/0/12, C-emit 176/0/55, all gates green, real games +
sound_fusion + zener_comp compile clean, audio md5 unchanged, the
pre-existing moog-filter-chain inlining (tiers 1-3) verified unchanged
by spot-checking `sf_channel_process`'s own `bl` count (still exactly 2:
`rotary_tick`, `zener_process` — unaffected by tier 4's introduction).

## A real ~2x regression, found re-running the benchmark catalog, same day

Per the user's own next request — "vamos ler benchmark.md... e rodar todos os
testes de novo pra ter um parametro" — re-ran `examples/bench_codegen.bpp`
against the day's final binary. `xform` (the integer-throughput kernel)
measured **~13.6-15.5ms, min-of-5**, against a historical/parity record of
~6.6-7.8ms — not noise (consistent across 5 runs, ~2x). Bisected with disposable
`git worktree`s at each of the day's 7 commits (the binaries are git-committed,
so no rebuild needed per commit — just `git checkout <sha> -- bpp` and run):
clean through `dc1e6c1`/`346efa1`, regressed starting exactly at `d9c25e6`
(Inc 7, control-flow leaf bodies).

**Root cause.** `transform` (`examples/bench_codegen.bpp`'s xform kernel) is
`static void`, body is a single `for` loop with no return — exactly the
VOID-inlineable (VI-2) shape, and exactly the shape Inc 7 newly allowed past
the old blanket T_WHILE rejection. Once eligible, VI-2 splices it
UNCONDITIONALLY at its one call site (inside `main`'s own `for (r=0;
r<TREP; r++)` loop). The disassembly showed why this hurts: `transform`'s
two 64-bit LCG-style constants (`6364136223846793005`,
`1442695040888963407`) were being REMATERIALISED via their full 4-instruction
`movz`+`movk`×3 chain on EVERY one of the inner loop's 65536 iterations,
320 times over — `examples/bench_codegen.bpp`'s own header comment on
`transform` even names this exact hazard ("the two loop-invariant 64-bit
constants... rematerialised as pure per-element overhead").

Why: "Stage 2a" hot-constant promotion (`cg_const_val`/`cg_const_wt` in
`bpp_codegen.bsm`) is a per-function pass that walks the CALLER's own body
ONCE, before codegen, hoisting loop-weighted constants to dedicated
registers materialised after the prologue. It runs BEFORE any inline splice
happens, so it is completely blind to constants arriving later via a
spliced callee's body — they get emitted on the default "rematerialise
every reference" path instead. The same general shape as RegAlloc v2's
"mangled slot invisible to linear-scan" bug from an earlier session
(`benchmarks.md`'s Stage-E-float entry): an analysis pass that runs on the
caller before splicing can't see what the splice adds.

**Fix shipped, not a structural rewrite.** Re-running hot-constant
promotion on a spliced subtree (the "real" fix) is a project on RegAlloc
v2's own "sees through inlined calls" scale — not today's scope. Instead,
disqualified the narrow, precisely-measured shape: `_inline_has_wide_lit`
(new) recursively checks a loop body for an integer literal needing more
than one ARM64 `mov`-immediate instruction (more than one nonzero 16-bit
chunk) — wired into `_inline_count_nodes`'s `T_WHILE` case, returning the
disqualifying 99 sentinel exactly like the early-return check does. Float
literals are explicitly exempt (filtered via the existing `is_int_lit_node`
helper) — they are always memory-loaded via a single `ldr` from the
literal pool regardless of hoisting, so `exp_f`/`log_f`'s own loops (full
of float constants like `0.6931471805599453`) are correctly unaffected.
Verified explicitly: `comp_process`'s `bl` count stayed at 1 (the
`db_to_amp_f`/`exp_f` inlining from Inc 8 survived this fix untouched).

**Lesson for whoever extends this arc next**: every time a new shape of
body becomes inline-eligible, re-run the FULL benchmark catalog
(`benchmarks.md`), not just the one named example that motivated the
change — `transform` was never the target of Inc 7/8's work, it just
happened to be sitting in the standard benchmark file with the exact
shape that exposes a gap the target functions don't have. A synthetic
correctness test would never have caught this; only a real timing
regression did.

Re-verified after the fix: `xform` back to ~6.8-7.1ms (min-of-5, matching
history), bootstrap byte-stable, suite 219/0/12, C-emit 176/0/55, all
gates green, audio md5 unchanged, `comp_process`'s 1-`bl` count (Inc 8's
win) confirmed intact.

## Post-close addendum — Lever-1 (2026-07-04)

Reopened for one increment (see benchmarks.md same date): builtin-using
bodies 11..40 nodes admit as TIER 4 (per-callsite cost+hotness, never
unconditional) and `_inline_has_disqualifying_tcall` composes through
VOID-inlineable targets. Spring hot loop 59.9 → 36.2 ms (1.65×), md5
unchanged, +6% binary accepted. Named next: VI doesn't fire on statement
calls inside CLONE-SUBSTITUTED bodies (4 `delay_push` bls survive in
spring_process), and the gcc -O2 spring oracle sits at 8.3 ms (4.3×).
