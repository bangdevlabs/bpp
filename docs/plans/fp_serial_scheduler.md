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
stays bit-exact, a b++ scheduler can match it WITHOUT the FP-non-associativity
`@fast` opt-in — the unroll + mul/add separation alone recovers most of the 3.46×.
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
  `-ffast-math`. So Phase 2 must be **opt-in** (a `@fast` / `: fast` annotation
  on the function or expression, or a compiler flag), never the default, and the
  checksums for a reassociated kernel will legitimately differ in the last bits.
  This is the one place "byte-identical to the scalar path" cannot be the gate;
  use an epsilon comparison + a documented tolerance.

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
