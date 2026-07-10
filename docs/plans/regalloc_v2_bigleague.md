# Plan — RegAlloc v2 + caller-saved spilling (big-league general codegen)

**Opened 2026-07-08.** The goal is NOT faster hot loops — a measured study (below)
proved the performance-critical loops are already optimal. The goal is
**professional general-code register allocation**, matching the mature cousins
(gcc/clang/rustc/Jai/Zig) on the code that ISN'T a tight leaf loop.

## The measured driver (this is not speculation)

Instrumented the B3 selection to count, per function, hot int locals
(loop-weighted refs ≥ 1000) that overflow the promotion budget (a64: 10, x64: 5)
into memory. Compiled real workloads:

| workload | non-leaf functions with loop-hot overflow | worst |
|---|---|---|
| the compiler itself | 69 | synthesize_loop_fn (36), cg_emit_stmt (26) |
| fps_3d_cpu (a game) | 37 | asset-load, UI/text (render_text 17) |
| moog_demo (audio) | 19 | wav_decode, font/ttf loaders |

**Decisive finding:** the core hot loops — the 2.5D ray-cast per-column
(`stbraycast`), the DSP per-sample filter — are ABSENT from the overflow list.
They are leaf/tight and fit the budget. The overflow lives in **non-leaf general
code**: asset loading (runs once), UI/text rendering (per-frame but not the
bottleneck), and the compiler's own functions (build-time). `render_text` alone
has **117 frame-slot accesses out of 356 loads/stores** — spill-heavy, but it is
text rendering, not the pixel loop.

So: b++'s design is correctly focused for the hot path (NOT amateur there), and
measurably mediocre on general non-leaf code. RegAlloc v2 closes THAT gap. It is a
big-league general-quality investment, not a hot-path fix.

## What exists today

- **B3 promotion**: greedy, promotes the top-N hot int locals into callee-saved
  regs (survive calls). Budget = callee-saved pool.
- **Freelist**: caller-saved regs used ONLY for expression-scoped temporaries
  (never live across a call).
- **RegAlloc v2 (partial)**: `regalloc_linear_scan` + `regalloc_compare_vs_b3`
  exist (liveness → linear scan, shares one reg across non-overlapping live
  ranges, sees through inlined calls). a64 wires `regalloc_apply` /
  `regalloc_apply_float`; **x64 `_x64_regalloc_apply` is a `return 0` stub**.
- **Overflow → memory**: hot locals beyond the budget sit in the frame,
  loaded/stored each use.
- **`save_caller_saved_int` / `restore_caller_saved_int`**: empty stubs,
  reserved for exactly this arc (the caller-saved spill set around a call).

## The design (two mechanisms)

1. **Global allocation (linear scan) on both backends.** Activate the existing
   linear-scan comparison on x64 (the stub) so a register is shared across
   non-overlapping live ranges → more locals fit without widening the pool.
2. **Caller-saved spilling.** When a hot cross-call value can't get a
   callee-saved reg (pool exhausted), put it in a CALLER-saved reg and spill it
   around the calls it crosses (`save_caller_saved_int` before the call,
   `restore` after), instead of leaving it in memory. Wins when the value is used
   many times BETWEEN calls (register) and crosses FEW calls (few spills) — a
   cost model decides: `crossed_calls × spill_cost` vs `callee-saved prologue
   cost` vs `memory-access cost`.

## Increments

- **M1 — activate linear-scan on x64. ✅ DONE (2026-07-08, `a429a63`).** Filled
  `_x64_regalloc_apply` (mirror `_a64_regalloc_apply`) + `_x64_b3_reg_at` + a
  `regalloc_int_budget` chip primitive (the scan had run with a64's hardcoded
  10/14 budget; x64 needs 5). a64 byte-identical (same budget), x64 self-host
  stable. MEASURED on the x64 gen1: `emit_node` frame accesses 676 → 137 (−80%),
  `val_check_node` 258 → 92 (−64%); `cg_emit_stmt` declined by the gate (correct
  fallback). Register-sharing across non-overlapping live ranges is the win.
- **M2 — caller-saved spill set.** Concrete design (reverse-engineered from the
  machinery, 2026-07-08). The core distinction: a promoted CALLEE-saved reg is
  saved once in the prologue; a promoted CALLER-saved reg is the opposite — NOT
  prologue-saved, but spilled around EACH call. So M2 is a coupled atomic change:
  1. Budget → 14 for non-leaf too (was 10 non-leaf / 14 leaf); `_a64_b3_reg_at`
     returns x9..x12 for slots 10..13 regardless of leaf (was leaf-only). x64:
     the caller-saved GP are the arg regs (rsi/rdi/r8/r9) — feasible but the save
     must interleave with outgoing-arg setup, so a64 first.
  2. A NEW list `a64_caller_saved_promoted`, separate from `a64_promoted_regs`.
     The apply / b3_select routes an x9..x12 assignment there, NOT to
     a64_promoted_regs, AND still calls `_a64_b3_claim_freelist(reg)` (else it
     collides with the expression freelist that also lives in x9..x15).
  3. Frame layout reserves a slot for each `a64_caller_saved_promoted` reg, but
     the PROLOGUE does NOT save them (only a64_promoted_regs, the callee-saved).
  4. `_a64_emit_save_caller_saved_int` = str each caller-saved-promoted reg to its
     reserved frame slot; `_restore` = ldr each back. (Fixed frame slots, NOT
     push/pop, to avoid interleaving with the value-stack.)
  5. `cg_emit_call`: `call(p.emit_save_caller_saved_int)` before emit_call_full,
     `call(p.emit_restore_caller_saved_int)` after. Timing works because the args
     evaluate using x0..x7 + the remaining freelist (x13..x15), disjoint from the
     claimed x9..x12, and the promoted values stay in-register until the call
     clobbers them (the save to the frame is just the recovery copy).
  Gate on `a64_caller_saved_promoted` being non-empty (empty → save/restore are
  no-ops → byte-identical). Build + verify self-host after EACH sub-step; this is
  the coupled frame-layout + timing class of change that produces silent bugs
  when rushed (cf. the transcendental comparison bug).

  **STATUS 2026-07-08:** Step 1 (the mechanism — `a64_caller_saved_promoted`
  list, the save/restore primitives, the cg_emit_call hook) SHIPPED inactive +
  byte-identical (`6fa94ee`). Step 2 (the activation — budget 14, `_a64_b3_reg_at`
  x9..x12 always, `_a64_route_promoted` at the 3 push sites, the reserve_spill
  caller-saved band) was attempted and **broke CATASTROPHICALLY** — gen2 ≠ gen3
  and nearly the whole native suite failed, i.e. the miscompile hit almost every
  function, not just the ones that promote into a caller-saved reg. That breadth
  is the clue: it is NOT the spill-around-calls timing (which would only touch
  caller-saved-promoting non-leaf fns) but something in the always-on path — the
  budget-14 / `_a64_b3_reg_at`-x9..x12 change altering the linear-scan for EVERY
  non-leaf function, or the `_a64_route_promoted` replacement of all 3 push sites
  (site 797 may not be a local-promotion site — VERIFY what it is before routing
  it), or a claim_freelist interaction with x9..x12 now live across calls. Next
  attempt: activate ONE piece at a time (budget first, verify self-host; then
  reg_at; then routing; then frame) so the breaking sub-step is isolated.

  **2nd attempt (2026-07-08) — TWO bugs, first fixed, second open.** Bug A (lldb
  backtrace = stack overflow in `_a64_route_promoted`): the global string-replace
  of `a64_promoted_regs = arr_push(...)` → `_a64_route_promoted(reg)` ALSO rewrote
  the router's OWN else-branch → infinite recursion. Fixed. With A fixed the
  machinery is basically right: the minimal test (12 caller-saved locals live
  across a call) ran CORRECTLY (785 == Step-1), trivial worked. Bug B (STILL
  OPEN): `test_gpu_atlas_aseprite` crashed — `_png_unfilter → memcpy` with a wild
  pointer (a promoted pointer not preserved across a call). _png_unfilter has
  loops + nested calls the single-level test doesn't exercise. Likely (a) arg
  evaluation clobbering a CLAIMED x9..x12 ad-hoc (read-after-clobber during arg
  setup, the reload only comes after the bl); or (b) nested calls sharing the one
  spill-slot band. NEXT: minimal repro (promoted pointer used before AND after a
  call in a loop, plus a NESTED call in an arg), disasm the spill. Reverted to
  Step 1 (clean base).

  **Bug B refined (2026-07-08, 3rd pass).** Disassembled the M2-compiled
  `_png_unfilter` at its crashing `memcpy(out+dst, raw+src, stride)` call: the
  caller-saved SAVE/RESTORE (str/ldr x9..x12 to [x29,#0x130..0x148]) is balanced,
  the frame is 0x150 so the 0x148 slot fits exactly (NOT a frame overflow), and
  the memcpy ARGS are computed from CALLEE-saved regs (out=x20, dst=x25, raw=x19,
  src=x21, stride=x24) — NOT the caller-saved x9..x12 (which hold i/filter/etc.).
  Yet lldb shows memcpy's dst arg (out+dst) pointing into a READ-ONLY MAPPED region
  → so a callee-saved pointer (out or dst) is corrupted. So caller-saved promotion
  of SOME local (into x9..x12) corrupts a DIFFERENT var that lives in a callee-
  saved reg — pointing at a linear-scan assignment / live-range interaction the
  budget-14 change introduces, not the spill mechanics themselves (those look
  right). Two call sites matter: the case-0 memcpy AND the case-4 `_paeth` inside
  the inner loop. NEXT: a CORRECT minimal repro (the first attempt hung on its own
  logic — needs valid filter/stride data) copying _png_unfilter + _paeth verbatim,
  then lldb-break at ITS memcpy to read x20/x25 across scanlines and see which
  pointer drifts + correlate with the linear-scan slot assignment. Reverted to
  Step 1.

  **M2 SHIPPED 2026-07-09.** The activation is live on a64: budget 14 for
  every function, `_a64_b3_reg_at` maps slots 10..13 to x9..x12
  unconditionally, `_a64_route_promoted` sends caller-saved promotions to
  the per-call save list, and `_a64_fn_reserve_spill` reserves the
  caller-saved band at the end of the frame. Getting here took FOUR real
  bugs, in order of discovery: (1) a self-inflicted infinite recursion in
  the route helper (a careless global string-replace rewrote the helper's
  own else-branch); (2) `_a64_b3_select_const`'s taken-scan only checked
  `a64_promoted_regs`, so promoted constants collided with caller-saved
  locals — a PNG width variable read back as the IDAT case-label constant
  (found with `bug --watch`); (3) TWO latent hidden-x9 clobbers that
  predate M2 entirely — the T_SWITCH pop-discard popped into a hardcoded
  x9 and `_a64_emit_caller_pc`'s FP walk used x9 as scratch (`a03036f`;
  these also poisoned the bootstrap toolchain until laundered through);
  (4) the fixed 1 MB code buffer was 98.4% full and M2's code growth
  overran it, corrupting neighbouring heap (`3151c1a`, now growable).
  Verification: tga + native suite 240/0/12 + a64 gen2==gen3 + x64 Docker
  self-host gen1==gen2 + full benchmark catalog (biquad 0.87x / lcg 0.86x
  BEAT gcc -O2; xform 1.05x; conv at exact parity; manylive — the
  motivating shape — 1.44x -> 1.26x). Honest cost note: in call-dense
  functions the per-call wrap ADDS traffic (png_decode_to_buf 60 -> 342
  x29-touches across 34 call sites) — hot loops are call-free so the
  catalog holds, but this is exactly the promote-or-not decision **M3's
  cost model** must gate: weigh (loop-weighted ref savings) against
  (8 bytes of save/restore per call site crossed by the live range).

  ---- Historical diagnosis trail (kept for the method) ----

  **Bug B ROOT-CAUSE DIRECTION (2026-07-08, cracked with `bug --watch`, not lldb).**
  `bug --tui --break stb/stbpixels.bsm:565 --watch out,dst,src,stride,prior,j
  <bin>` on the M2 binary reads the b++ VARIABLES BY NAME at the memcpy (this is
  the tool lldb couldn't match — raw registers don't tell you which *variable*).
  Result: at the 1st memcpy `out = 0x300000010` and `stride = 0x125153910` are
  BOTH garbage while dst/src/j/prior are correct; at the crash `dst = 0x300000010`
  (the value `out` held earlier). Since the 1st memcpy did NOT crash, the real
  out/stride there are valid — so the `.bug` map (emitted by the SAME M2 codegen)
  records out/stride at the WRONG locations. That is the tell: under budget-14 the
  variable→register assignment is INCONSISTENT — `cg_var_promote` holds one reg for
  a variable while the value lives elsewhere in some range. Classic LIVE-RANGE
  SPLIT leaking past the `regalloc_has_any_split` refusal (or the split producing
  two assigns and the apply/`_a64_route_promoted` recording the wrong one). NEXT:
  dump the linear-scan assigns for `_png_unfilter` under budget-14, find the var
  with >1 assign (a split) or two vars mapped to the same slot/reg, and either
  tighten the split refusal or make the apply consistent. The spill mechanics are
  NOT the bug (confirmed earlier); the ASSIGNMENT is. Reverted to Step 1.
- **M3 — the cost model. SHIPPED 2026-07-09.** Function-level gate at the three
  caller-saved routing points: promote into x9..x12 (non-leaf) only when the
  loop-weighted references beat twice the loop-weighted call count
  (`_a64_caller_saved_worth`: refs > 2 * cg_fn_call_wt — one save + one restore
  around every call, both sides in the same 1000x currency; the spine counts
  real calls at loop weight in cg_b3_walk's T_CALL). In `_a64_regalloc_apply`
  the gate is PER SLOT (the wrap is paid per register, so a cold variable rides
  a slot a hot one already paid for). Measured: png_decode_to_buf x29-touches
  342 -> 78 (the M2 call-dense regression killed) while the catalog holds.
  Two findings recorded en route: (a) manylive showed a 14% swing purely from
  LOOP-HEAD ALIGNMENT (the M2 build's hot loop landed on a 64-byte boundary by
  luck, the M3 layout shifted it 4 bytes off — instruction-identical disasm) —
  a real future lever: gcc/LLVM align loop tops to 16/32B, b++ emits linearly;
  (b) the model is function-level, not live-range-level — the honest refinement
  (wrap only the regs LIVE at each call, cost only the calls a range CROSSES)
  needs interval-vs-callsite positions at emit time and belongs to M4's design.
  Known approximation: the linear scan may assign a hot var a caller-saved slot
  where B3 would have used callee-saved; the gate then refuses it entirely
  instead of re-slotting (safe fallback = memory, never wrong, occasionally
  conservative).
- **M4 — float. SHIPPED 2026-07-09** (`69f63b1` mechanism + `0cf5286`
  activation). Two new chip primitives (emit_save/restore_caller_saved_flt)
  wrap every call, nested inside the int pair. x64: `_x64_b3_select_float`
  promotes in NON-leaf functions too — each pick M3-gated
  (cg_caller_saved_worth, now a SPINE function shared by both chips'
  routing) and pushed to x64_caller_saved_promoted_flt; the band sits one
  word below the B3 int spills with the same -(i+1)*8 convention. a64: the
  machinery exists but the band stays EMPTY by design (callee-saved d8..d15
  already covers cross-call floats; no measured pressure driver). Proof:
  non-leaf float accumulator kernel bit-exact a64-native vs x64-Docker,
  disasm shows acc in xmm11 + movsd wrap; a64 byte-identical; bootstrap
  PASS; suite 240/0/12; x64 self-host gen1==gen2. THE former "architectural"
  x64 cross-call float gap is CLOSED. Remaining refinement (designed, not
  built): live-range-level wraps — save only the regs LIVE at a given call,
  charge only the calls a range crosses; needs interval-vs-callsite
  positions at emit time. Watch item: test_dsp_param_safety flaky segfault
  on x64/Rosetta, PRE-existing (same on pre-M4 compilers), separate hunt.
  [Both closed 2026-07-10 — the wraps refinement shipped as M5 below, and
  the "flaky segfault" watch item was a deterministic float-CIP gate
  off-by-one, fixed in `154f317`.]
- **M5 — live-range wrap masks. SHIPPED 2026-07-10** (`5f14566` mechanism
  byte-identical + `e3a5ae1` activation). The refinement's first half:
  regalloc_stamp_call_masks walks the CFG in the interval position
  numbering and stamps each T_CALL node with a bitmask of the band
  entries whose occupant is live at that statement; cg_emit_call passes
  the mask to the chip save/restore primitives, which skip dead entries.
  The spine mirrors band occupancy via cg_caller_band_note at the chips'
  routing sites (promoted constants note var -1 = always live); every
  unknown — unstamped call, splice clone, refused apply, occupant without
  an interval — defaults to the full wrap, so masking only ever removes
  saves liveness disproves. Measured: _png_unfilter 38→32 x29-touches;
  png_decode_to_buf honestly unchanged (its M3 survivors ARE live across
  its calls); sf_bench A/B alternating min-of-5 equal. NOT yet built (the
  second half): charging only the calls a range CROSSES in the M3 cost
  model itself — the promotion gate still weighs refs against the
  function-wide call count, so a variable dead around most calls pays the
  full-function price at promotion time even though emission no longer
  wraps it there. That refinement wants a per-interval crossed-call
  weight computed from the same stamp walk.
- **M6 — per-range promotion cost. STEP 0 MEASURED 2026-07-10, VERDICT:
  BUILD** (`f7a9d6b`, the probe). `BPP_M3_PROBE=1` reports every
  M3-refused candidate a per-crossed-call gate would admit: **151
  loop-hot refusals (refs >= 1000) across four workloads** — compiler 51
  (mo_resolve_relocations instr refs=12000 vs crossed cost 8000,
  topo_sort, regalloc_linear_scan), fps_3d_cpu 41 (_ttf_load_glyph —
  the asset-load class the pressure study named), pathfind 36,
  sound_fusion 23. Coherence argument: since M5, the EMISSION side
  already charges per-range (masked wraps); the promotion gate is the
  last place still paying the function-wide price. Design notes for the
  build: (a) stages A-C (CFG/liveness/RPO/intervals) read nothing from
  B3, so they can move ABOVE b3_select in cg_emit_func, making a
  cg_caller_saved_worth_range(vi) spine helper available to both the
  greedy path and the apply; (b) the greedy loop's early-exit
  monotonicity breaks under per-range costs (a colder var crossing
  fewer calls can win) — the selection must scan all candidates instead
  of returning at the first refusal; (c) the probe stays as the
  validation tool — after the build these hits should collapse to ~0.

## Gate + measurement per increment

Self-host byte-stable (gen2==gen3) both backends, suites both, catalog re-run,
zero warnings. THE measurement is the pressure study re-run: overflow-function
count and per-function overflow must DROP. Disasm a representative non-leaf
function (render_text, cg_emit_stmt) before/after and count the frame-slot
accesses eliminated. Hot-loop benchmarks (conv, raycast) must NOT regress (they
don't overflow, so they should be untouched — a guard against collateral damage).

## Honest scope note

This is a multi-session arc touching the allocator core. It does NOT speed up the
DSP/raster hot loops (already optimal). Its value is big-league general codegen
quality + (via M4) the one remaining float-parity gap on x64. Sequence it as a
deliberate arc, not a quick win.
