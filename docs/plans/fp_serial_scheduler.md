# Plan — closing the FP-serial gap (biquad 1.49× → parity)

**UPDATE (2026-07-06): Phase 2 (the FP scheduler) is now JUSTIFIED by a measured
gap. The blondie_amp arc's convolution cab (`stb/stbconv.bsm`, Inc 7) is the
"real audio DSP workload" the 2026-06-17 note below said to wait for. Measured
on the 1024-tap FIR (`examples/bench_conv.bpp`): ours 284 ms vs gcc -O2 82 ms =
3.46×, checksums bit-identical. Disassembling gcc's kernel (house discipline —
disasm the reference before naming the gap) is decisive: it is NOT vectorized
(0 packed ops); it is scalar, unrolled 4× with independent `fmul`s that fill the
FP units and shorten the critical path, then a serial `fadd` chain in the SAME
order (no reassociation). So the gap is instruction SCHEDULING, and because gcc
stays bit-exact, a b++ scheduler can match it WITHOUT any FP-reassociation
opt-in (no such mechanism exists in b++ anyway — see the correction note in
Phase 2) — the unroll + mul/add separation alone recovers most of the 3.46×.
This is the measured, scheduling-bound audio gap that moves Phase 2 from
"deferred/speculative" to "the next compiler arc." See
`docs/manual/benchmarks.md` (the 2026-07-06 FIR-convolution entry) for the full
disasm + numbers.**

**UPDATE (2026-06-17): Phase 1 SHIPPED (`befad75`) and it CLOSED the biquad gap —
1.49× → 1.02× (parity). Phase 2 was DEFERRED at the time: no measured gap to
chase. The 2026-07-06 update above is exactly the "real workload demonstrates a
scheduling-bound gap" trigger that note asked for. The diagnosis below was
correct: the bulk of the biquad gap was the float copy funnel, not scheduling —
the convolution gap is the OTHER thing, genuinely scheduling.**

**Status:** Phase 1 DONE, Phase 2 deferred. The integer kernels reached `gcc -O2` parity
(lcg 1.02×, xform 1.10×) after the loop-control + the two register levers. The
one standing codegen gap is the **float serial kernel** — the biquad IIR filter
at **1.49× gcc -O2**. Audio/DSP is a first-class target, so this is worth
finishing. The plan has a cheap first phase and a harder second phase; do them
in order and measure between.

## Diagnosis (measured, not guessed)

`gcc -O2` biquad inner loop — 7 instructions, post-increment addressing, a
near-serial fmadd chain seeded by one independent `fmul`:

```
ldr   d5, [x0], #8
fmul  d18, d17, d1          ; b1*x1  (independent, starts the chain early)
fmadd d18, d0, d5, d18      ; + b0*xi
fmadd d7,  d2, d7, d18      ; + b2*x2
fmadd d7,  d3, d6, d7       ; - a1*y1
fmadd d18, d4, d16, d7      ; - a2*y2
str   d18, [x1], #8
```

b++'s biquad inner loop is **dominated by `fmov` register shuffles**:

```
fmul  d0, d15, d8           ; the compute is fine (fmul + fmadd chain)
fmadd d0, d2, d9, d0
...
fmov  d11, d0               ; save y
fmov  d0, d9 ; fmov d13, d0 ; x2 = x1   ← 2 fmovs, routed through d0
fmov  d0, d8 ; fmov d9,  d0 ; x1 = x    ← 2 fmovs
fmov  d0, d10; fmov d14, d0 ; y2 = y1   ← 2 fmovs
fmov  d0, d11; ...          ; y1 = y    ← 2 fmovs
```

The state updates `x2=x1; x1=x; y2=y1; y1=y; acc=acc+y` — five float var-to-var
copies between **promoted d-registers** — are each emitted as `fmov d0, src;
fmov dst, d0`, routing through `d0` exactly like the integer x0 funnel that
Stage 1 / the assign-into-destination lever already removed for integers. **This
is the bulk of the gap, and it is cheap to fix.** The genuine FP-scheduling
difference (how the independent taps are interleaved) is the smaller, second
residual.

## Phase 1 — float assignment direct move (cheap; the float twin of lever 1)

A `floatVar = floatVar` (or any float value already in a promoted d-register
assigned to another promoted float local) should emit **one** `fmov dDst, dSrc`,
not a round-trip through `d0`.

- **Where:** the T_ASSIGN statement path, the float branch. Mirror the integer
  self-update / assign-into-dest: when LHS is a promoted `: float` local and the
  RHS is a promoted float leaf (or a float CIP expression that the float CIP
  could emit into the destination d-register), emit straight into the
  destination register.
- **Primitive:** `emit_fmov_reg` already exists (used by multi-value return);
  reuse it for the var-to-var case so `x2 = x1` is `fmov d13, d9`.
- **Also:** extend the float compute-in-place (`cg_emit_float_into`) to accept
  the assignment's destination d-register, exactly as `cg_emit_int_into` now
  does for integers (the `y = b0*x + …` expression should land in `y`'s register
  with `fmadd` writing it directly, no `fmov d11, d0` save).
- **Expected:** removes ~5 `fmov` pairs per iteration → biquad ~1.49× → ~1.15–1.2×.
- **Risk:** low. Same shape as the integer levers, same gate. a64 has the
  callee-saved d8–d15 pool; x64 has zero callee-saved xmm (the float-B3 opt-out
  already documented in `backend_parity.md`), so this is a64-only — gate on the
  float-CIP being active, like the integer path gates on `int_temp_count >= 0`.

## Phase 2 — FP instruction scheduling (the real lever)

After Phase 1 the compute is a left-deep serial `fmul → fmadd → fmadd → …`
chain. gcc seeds the chain with an independent `fmul` and orders the taps so the
FP pipeline stays busy. The biquad's feedforward taps (`b0*x, b1*x1, b2*x2`) are
mutually independent; only the feedback (`a1*y1, a2*y2`) is loop-carried.

- **Lever:** a small **list scheduler** over the float expression DAG that
  (a) computes the independent sub-products into **separate** d-registers (the
  d8–d15 pool gives room), (b) issues them back-to-back to fill the FP units,
  (c) sums them at the end. I.e. tree-balance the accumulator instead of a
  strict left spine, and reorder so independent ops don't wait on each other.
- **Scope tightly:** start with *within-iteration* reassociation of a
  sum-of-products (`a*b + c*d + e*f + …`) into a balanced tree using a handful
  of accumulator registers — this is the pattern every DSP filter and dot
  product has. Do NOT start with software pipelining across iterations (the
  hardest form); leave that as a later phase if a dedicated audio bench still
  shows a gap.
- **Correctness caveat:** floating-point addition is **not associative** —
  reassociating `a + b + c` changes rounding. gcc only does this under
  `-ffast-math`. So Phase 2 must be **opt-in** (a hypothetical `@fast` annotation
  on the function or expression, or a compiler flag), never the default, and the
  checksums for a reassociated kernel will legitimately differ in the last bits.
  This is the one place "byte-identical to the scalar path" cannot be the gate;
  use an epsilon comparison + a documented tolerance.
  **(CORRECTION 2026-07-07: `@fast` was only ever THIS plan's sketch for a
  mechanism that would have to be designed — no such annotation exists in b++.
  The language's real annotation surface is `@safe` (the only function
  annotation, bpp_parser.bsm ~2039), statement-level `@profile("zone")`, and
  the `@seq`/`@par`/`@gpu` dispatch hints before a `while`; the old phase
  keywords are E260-deprecated. Later docs repeated "`@fast` opt-in" as if it
  were an established mechanism — it is not. Per the 2026-07-07 addendum below,
  the conv arc needs NO reassociation at all, so no such mechanism needs to be
  designed for it; if a multiple-accumulator S4 is ever justified, designing
  the opt-in is part of that increment's work, and `@safe`'s parser path is the
  pattern to follow.)**

## Gate + measurement

- `examples/bench_codegen.bpp` biquad is the standing FP-serial gate.
- Add a dedicated **audio/DSP bench** (a real filter bank or FFT butterfly) so
  the win is measured on the workload that motivates it, not just the synthetic
  biquad.
- Phase 1: exact checksums must still match (it is pure copy elimination).
- Phase 2: epsilon checksum + a stated tolerance, behind the opt-in annotation.

## Why this order

Phase 1 is the integer levers' float twin — cheap, low-risk, and it removes the
*majority* of the measured gap (the `fmov` funnel), likely landing biquad near
~1.2×. Phase 2 is the genuine "PhD" scheduling lever and carries the FP-
associativity correctness question, so it earns the opt-in gate and a real audio
benchmark before it ships. Measure after Phase 1 before deciding how far Phase 2
needs to go.

---

## ADDENDUM (2026-07-07) — the pre-arc disasm decomposes the 3.46×, and it is NOT (mostly) scheduling

Before writing any scheduler, the house discipline (disasm OUR OWN kernel the
way the CIP arc disasm'd gcc's) was applied to `conv_tick`'s inner loop as the
Stage-F compiler emits it today. The finding restructures the arc: the bulk of
the 3.46× is the **float value-stack**, not instruction order. Per tap we emit
~16 instructions including **two full stack round-trips**:

```
fmov d0, d8               ; acc (promoted d8!) funneled into d0
str  d0, [sp, #-0x10]!    ; PUSH acc to the fpush value-stack (memory!)
ldr  d0, [x21]            ; load ir[k]           (ip promoted x21 — good)
str  d0, [sp, #-0x10]!    ; PUSH it (memory again)
ldr  d0, [x22]            ; load hist            (bp promoted x22 — good)
ldr  d1, [sp], #0x10      ; POP the ir value back
fmul d0, d1, d0
ldr  d1, [sp], #0x10      ; POP acc back
fadd d0, d1, d0           ; no fmadd fusion
fmov d8, d0               ; funnel back out to acc
add  x21, x21, #8 ; sub x22, x22, #8 ; add x23, x23, #1
b    (top-tested while: cmp+b.ge at top, unconditional b at bottom)
```

RegAlloc already did its job (acc→d8, ip/bp/k→x21-x23). What failed is the
**float compute-in-place**: `cg_float_tree_need` accepts T_VAR/T_LIT/T_MEMLD
leaves but NOT `peekfloat(addr)` — which is exactly how a pointer-walking FIR
reads its operands — so `acc = acc + peekfloat(ip)*peekfloat(bp)` falls off the
CIP path entirely, losing the fmadd fusion (which the CIP already has,
line ~3716) and paying the generic accumulator stack. This is the float twin of
the integer CIP memory-leaf bug (2026-06-17: "checked n.b!=8 but subscripts
carry hint 0" — the lever existed, the leaf test starved it).

**Corrected attack order** (cheapest first, measure between, all bit-exact
until S3):

- **S1 — peekfloat as a float-CIP leaf.** Extend `cg_float_tree_need` /
  `cg_emit_float_into` to accept `peekfloat(addrTree)` where the address tree
  fits the INTEGER temp budget (the same cross-pool validation the T_MEMLD case
  already does). Expected per-tap: `ldr, ldr, fmadd d8,dA,dB,d8` + loop control
  — kills both stack round-trips, the d0 funnel, and un-starves the existing
  fmadd fusion in one move. Bit-exact (pure copy/traffic elimination).
- **S2 — while-loop bottom-test fusion**, if the disasm after S1 still shows
  `cmp+b.ge` at top plus unconditional `b` at bottom (the for-loop fusion lever
  may not cover `while`). Two branches per tap → one. Bit-exact.
- **S3 — unroll ×4 + mul/add separation** (the genuine scheduling residual,
  gcc's shape: batched independent `fmul`s + the `fadd` chain in SOURCE order).
  NOTE: gcc's own kernel proves this form is bit-exact — the accumulator adds
  stay serially ordered; only the loads/muls are hoisted. NO reassociation
  opt-in is needed for this step; one would only be needed for
  multiple-accumulator splitting, a possible S4 that current numbers may never
  justify — and no such opt-in mechanism exists in b++ today (see the
  correction note in Phase 2: `@fast` was a sketch, never a language feature).

Estimate: S1 alone should cut the 284 ms hot loop to near the fmadd-serial
bound; measure `bench_conv` after each stage and stop when the residual vs
82 ms stops paying (per the "when to STOP" doctrine that closed the integer
arc).
