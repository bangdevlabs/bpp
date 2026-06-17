# Backend parity — aarch64 vs x86_64

B++ is backend-agnostic by construction: the spine (`bpp_codegen.bsm`) owns
the portable codegen logic and defers instruction emission to a chip's
`ChipPrimitives` table (a64 = `aarch64/`, x64 = `x86_64/`). This document is
the single source of truth for **where the two backends diverge** and why —
so a claim like "full parity" never goes stale unnoticed, and so anyone
picking up an x64 gap knows exactly which stubs to fill.

**Testing caveat (load-bearing).** a64 is the native dev target (macOS); x64
is exercised only by cross-compiling (`--linux64`) and running in Docker
(`ubuntu:22.04`, `--platform linux/amd64`). The full Linux suite + self-host
byte-stability run there, but Docker can't pair 100% with native hardware
(no GPU, Rosetta-translated CPU). Treat x64 perf numbers as directional and
always confirm a codegen change with the Docker self-host (`gen1 == gen2`).

## Feature matrix

| Capability | a64 | x64 | Class |
|---|---|---|---|
| Inline (B2 single-return / S4 multi-stmt / VI void) | ✅ | ✅ | parity |
| Int local promotion (B3) | ✅ x19..x24 (6) | ✅ r12..r15/rbx | parity (count differs) |
| Loop-weighted B3 — promote loop-invariant params (Stage 2b) | ✅ | ✅ | parity (shared spine) |
| `: double` 128-bit SIMD + 4×f32 `vec_*` (B4) | ✅ NEON | ✅ SSE2 | parity |
| Byte-wide SIMD (`vec_load16b` … `vec_movemask`) | ✅ NEON | ✅ SSE2 (`PMOVMSKB`) | parity |
| Multi-value return banking | ✅ x0..x7/d0..d7 | ✅ rax,rdx,rcx,rsi,rdi,r8-r10/xmm0-7 | parity (custom window) |
| Autovec + outline (loop→worker, SIMD synth) | ✅ | ✅ | parity (shared spine) |
| Dynamic linking / FFI externs | ✅ Mach-O | ✅ ELF PLT/GOT | parity |
| Immediate-form shift (`<<`/`>>` by a constant) | ✅ SBFM/UBFM | ✅ `C1 /4,/5,/7` | parity (shared spine peephole) |
| Int expression freelist (B1) | ✅ x9..x15 (7 deep) | ⚠️ r11 (1 deep) | **architectural** |
| Float compute-in-place (F.2.c) + float-B3 + `fmadd` | ✅ | ❌ | **architectural** |
| **Integer compute-in-place (Stage 1)** | ✅ | ❌ stubs ready | **deferred** |
| ↳ CIP memory-load leaf — `a[i]` subscripts (Jun 17 fix) | ✅ | ❌ (CIP off) | rides Int-CIP |
| ↳ CIP strength reduction `x*2^k` → shift (Stage C) | ✅ | ❌ stub | rides Int-CIP |
| ↳ CIP integer `madd`/`msub` fusion (Stage E) | ✅ | ❌ stub (no int MAC) | rides Int-CIP |
| **Constant promotion (Stage 2a)** | ✅ | ❌ no stub yet | **deferred** |

## Architectural gaps (permanent — do NOT "fix")

These are forced by the System V AMD64 ABI / baseline x86-64 ISA, not by
missing work. Re-deriving them wastes time; the answer is recorded here.

- **No callee-saved XMM → no float CIP / float-B3 / fmadd.** SysV makes ALL
  `xmm0..15` caller-saved (Win64 keeps `xmm6..15`; this is the divergence).
  The float F.2.c win came from promoting hot floats into callee-saved
  `d8..d15`; x64 has no equivalent band, so the winning stage has no target.
  `fmadd`/`fmsub` additionally need FMA3 (Haswell+, x86-64-v3), not the
  baseline SSE2 the ELF target assumes — emitting it would SIGILL on older
  CPUs. x64 opts out via `_x64_simd_temp_count() = 0`. See
  `x64_primitives.bsm` (the F.2.c hooks) — the comment there is the canonical
  explanation.
- **Shallow int freelist (1 vs 7).** SysV leaves far fewer caller-saved GP
  registers free than AAPCS64, so the B1 expression freelist is `r11` only
  (vs `x9..x15`). Not a bug — a register-budget fact. It's also *why* the
  integer-CIP gap below is low-value on x64.

## Deferred gaps (implementable — the spine + stubs are ready)

The expensive part (the spine machinery) is done and backend-agnostic; only
the chip-specific emission remains. Filling these in is a contained task.

### Integer compute-in-place (Stage 1)

a64 emits `+ - * & | ^` integer trees destination-driven (no x0 shuttle); x64
keeps the accumulator path. **Spine is complete** — `cg_emit_int_into` /
`cg_int_tree_need` / `cg_int_leaf_kind` and the six struct slots
(`emit_iop_into` etc.) exist and run on both backends. **x64 chip stubs
exist** in `x64_primitives.bsm` (`_x64_emit_iop_into` etc., currently `{ }`)
and `_x64_int_temp_count()` returns `-1`, so the spine gate
`cg_int_tree_need <= int_temp_count` never fires and the accumulator path is
used untouched.

To implement:
1. Fill the four emit stubs. `emit_iop_into(op, dreg, s1, s2)` must handle the
   **2-operand x86 ISA**: if `dreg != s1` emit `mov dreg, s1` first, then the
   in-place `OP dreg, s2` (`add`/`sub`/`imul`/`and`/`or`/`xor` reg,reg). In
   the CIP `dreg != s2` always holds, so no operand-aliasing fixup is needed.
   `emit_iconst_into` = `mov dreg, imm64`; `emit_iload_var_into` = load from
   the rbp-relative frame slot; `emit_iload_mem_into` = `mov dreg,[dreg]`.
2. Give `_x64_int_temp_count()` a real free count + a temp pool. With only
   `r11` free it fires for ≤1-temp trees (still the very common `var OP const`
   / `var OP var`). A wider pool (e.g. r10 + reclaimed arg regs in call-free
   regions) is more work for diminishing return — **low priority**: the SysV
   register pressure caps the win.

### Constant promotion (Stage 2a)

a64 promotes hot loop-invariant constants into spare callee-saved registers;
x64 does not. **Spine is complete and backend-agnostic** — the loop-weighted
constant table (`cg_const_val/wt/reg`), the prologue materialisation (step
12b in `emit_func`), and all three use-sites read the table on any backend.
The ONLY a64-specific piece is `_a64_b3_select_const()`, chained into
`_a64_b3_select_prim`. x64's `_x64_b3_select_prim` simply doesn't call a
const-select, so `cg_const_reg` stays `-1` and every use-site/materialise is
a no-op.

To implement: add `_x64_b3_select_const()` (mirror the a64 one — rank
`cg_const_wt`, assign x64's free callee-saved regs r12..r15/rbx not taken by
local B3, push them onto `x64_promoted_regs` so the prologue saves them, set
`cg_const_reg`) and append it to `_x64_b3_select_prim`. The accumulator
use-site (`cg_emit_lit`) and the prologue materialise then light up
automatically. Higher value than x64 integer-CIP because it works on the
accumulator path (no freelist pressure) and SysV has enough callee-saved GP
registers.

## When to update this doc

Any change that lands a codegen optimization on one backend but not the other
MUST add a row here (and stub the other backend per "Three Disciplines #2" in
the bootstrap manual). A change that closes a deferred gap moves its row to
parity and deletes its "to implement" section.
