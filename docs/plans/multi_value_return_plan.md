# Multi-Value Return — the arity-8 "register-window transformer"

Status: **planned, gate lifted 2026-06-14.** The performance gate
(register-resident returns) was F.2.c — float compute-in-place + B3
promotion — which shipped in `c099438` (1.27x on the biquad). This plan
turns the `bootstrap_manual.md` "Multi-value return (the ABI side)" note
into a phased implementation.

## The vision (why arity 8, not C's 2)

Because b++ is typeless and every scalar is a 64-bit word, a function can
take up to 8 arguments in registers (x0..x7 int, d0..d7 float) AND return
up to 8 values the same way — a symmetric **8-in / 8-out** register ABI.
C cannot do this cleanly (it returns one scalar, or a struct via a hidden
pointer + memory). b++ functions calling b++ functions are free to use a
richer convention: **functions as register-window transformers** — a hot
DSP/graphics kernel reads its inputs from registers, computes, and hands
back several results in registers, never touching memory in between.

This is the differentiated play: not "copy C's two-register return," but
a register-window ABI that fits a word-oriented, register-allocated
backend. The audio motivation is concrete:

```
left, right          = osc_filter_pan(t);     // 2 floats out
r, g, b, a           = shade(u, v);           // 4 floats out
x, y, z, nx, ny, nz  = transform(vtx);        // 6 floats out
```

With F.2.c, when the consumers (`left`, `right`, …) are hot enough to be
B3-promoted, the returned d0..d7 values are `fmov`'d straight into their
callee-saved registers — the chain stays register-resident.

## The ABI (mirror of the argument path)

Returns mirror the EXISTING per-scalar 2-bank argument classifier
(`cg_emit_call_arg_rearrange` + the prologue int_seq/flt_seq loop). Each
returned scalar is classified by its static type:

- integer-typed results fill `x0, x1, x2, …` (int bank),
- float-typed results fill `d0, d1, d2, …` (float bank),

independently, exactly as arguments fill x0..x7 / d0..d7. No System-V
recursive aggregate classifier is needed — b++ has no nested aggregates
at the ABI boundary, only a flat list of word-scalars. Budget: 8 per bank
(matches the argument-register count). Overflow (>8 of one bank) is a
compile error for now; spilling can come later if a real consumer needs
it (logged, never silent — Rule: no silent caps).

## Terrain map (from the 2026-06-14 Explore)

| Piece | Where | Note |
|---|---|---|
| `return expr;` codegen | `bpp_codegen.bsm:3394` (T_RET) eval→coerce→jump | extend to a list |
| value lands in | x0 / d0 (a64), rax/xmm0 (x64) | add x1.. / d1.. |
| arg-bank classifier (MIRROR THIS) | `cg_emit_call_arg_rearrange` `bpp_codegen.bsm:674` | per-arg int/flt split |
| arg-copy chip prims | `a64_primitives.bsm:1097` (copy_int / copy_flt / int_to_flt) | add return-side twins |
| assignment parse | `bpp_parser.bsm:3145` (T_ASSIGN) | add comma-LHS |
| statement dispatch | `bpp_parser.bsm:2609` | fallthrough to parse_expr |
| function signature | `bpp_parser.bsm:1857` (`-> Type`) | extend to `-> t1, t2, …` |
| FnMeta | `bpp_types.bsm:61` (single `ret_type`) | add `ret_count` + `ret_types[]` |
| return-type query | `get_fn_ret_type` `bpp_types.bsm:831` | add per-index variant |
| closest precedent | `: double` 128-bit return via q0 (`a64_codegen.bsm:728,1493`) | tuple-of-words is simpler |

Key finding: the `emit_copy_ret_int / emit_copy_ret_flt` primitive slots
already exist in the contract; the return path is a clean extension, not
a rewrite.

## Phase A — syntax + banked return (compiles & runs)

Goal: `a, b = f()` parses, type-checks, codegens, and runs correctly, for
up to 8 banked return values. Correctness only; no perf claim yet.

1. **Parser — return signature.** Extend the `-> Type` parse to a
   comma-list `-> t1, t2, …` (reuse the param comma-list precedent).
2. **FnMeta.** Add `ret_count` + a `ret_types` block (parallel to
   `par_block`). Default `ret_count = 1` for every existing function (so
   all current code is unchanged).
3. **Parser — return statement.** `return e1, e2, …;` → a T_RET carrying
   a list of expressions (arity must match the signature).
4. **Parser — multi-assign.** `a, b, … = f();` → a new node (or extended
   T_ASSIGN) with a target list and a single call RHS. Statement-position
   only (b++ has no assignment-as-expression). Arity must match the
   callee's `ret_count`.
5. **Codegen — return.** Evaluate each return expr and place it in its
   banked register (x-bank / d-bank), mirroring arg rearrange, then jump
   to epilogue. New chip prims as needed (return twins of arg-copy).
6. **Codegen — call site.** After the `bl`, unpack the banked return
   registers into the assignment targets (store each, type-aware — reuse
   `emit_store_var_typed`, which already handles promoted locals).
7. **Tests.** `tests/test_multi_return.bpp`: int-only, float-only, mixed,
   arity 2..8, in a loop. Plus the negative cases (arity mismatch →
   diagnostic).

Gate each step: bootstrap gen2==gen3==gen4 byte-stable, native 187/0/12,
C-emit 152/0/47. Commit Phase A as its own milestone.

## Phase B — register-resident chain (measured, over F.2.c)

Goal: prove the perf payoff the sidequest was for. Build a DSP chain
(`osc → filter → pan`) that returns multiple floats whose consumers are
B3-promoted, and MEASURE that the values stay in d8..d15 across the chain
(disasm: no frame round-trip between the return and the next call's
args). Compare against the out-parameter version. Report the honest
number — and, per the session's hard lesson, isolate the variable
(same kernel, only the return convention changes) so the measurement is
a real control, not a hand-written conflation.

## Out of scope (for now)

- C-emit / x86_64 multi-return: a64 first (where F.2.c lives); the C
  backend can keep single-return or lower a tuple to a struct later.
- >8 of one bank (spill to stack): error for now, logged.
- Returning a `: double` SIMD value as one of the slots: later.
