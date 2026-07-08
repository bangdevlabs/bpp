# Backend parity — aarch64 vs x86_64

> **Spine/backend doc trio** — three sibling docs, kept separate on purpose
> (different jobs, different change rates — don't merge them). **You are reading
> the *scoreboard*** (exactly where a64 and x64 diverge **today** — the one that
> changes on every codegen commit; see "When to update this doc" at the bottom).
> The others: [nomad_manual.md](nomad_manual.md) — the *doctrine* (how to reach a
> new target, the principles this matrix is an instance of);
> [spine_analysis.md](spine_analysis.md) — the *design rationale* (is the spine a
> C--, should it evolve, + b++'s typing model).

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
| Int local promotion (B3) | ✅ x19..x28 (10) | ✅ r12..r15/rbx | parity (count differs) |
| Loop-weighted B3 — promote loop-invariant params (Stage 2b) | ✅ | ✅ | parity (shared spine) |
| `: double` 128-bit SIMD + 4×f32 `vec_*` (B4) | ✅ NEON | ✅ SSE2 | parity |
| Byte-wide SIMD (`vec_load16b` … `vec_movemask`) | ✅ NEON | ✅ SSE2 (`PMOVMSKB`) | parity |
| Multi-value return banking | ✅ x0..x7/d0..d7 | ✅ rax,rdx,rcx,rsi,rdi,r8-r10/xmm0-7 | parity (custom window) |
| IEEE NaN float-compare semantics (`NaN != NaN` true, orderings false) | ✅ FCMP (by ISA design) | ✅ fixed 2026-07-03 (UCOMISD unordered: LT/LE swap+ABOVE, EQ/NE fold PF; was silently wrong for EQ/NE/LT/LE — `tests/test_float_nan_cmp.bpp` is the gate) | parity |
| Autovec + outline (loop→worker, SIMD synth) | ✅ | ✅ | parity (shared spine) |
| Dynamic linking / FFI externs | ✅ Mach-O | ✅ ELF PLT/GOT | parity |
| Immediate-form shift (`<<`/`>>` by a constant) | ✅ SBFM/UBFM | ✅ `C1 /4,/5,/7` | parity (shared spine peephole) |
| Int expression freelist (B1) | ✅ x9..x15 (7 deep) | ⚠️ r11 (1) / r11+r8+r9 (3) in leaf fns | narrower (SysV pressure); leaf-widened 2026-07-08 |
| Float compute-in-place (F.2.c) + float-B3 | ✅ | ✅ (2026-07-07, leaf fns) | parity in leaf fns (caller-saved xmm) |
| Float `fmadd` (fused) | ✅ (baseline) | ✅ under `--fma` (FMA3/VEX); mulsd+addsd otherwise | parity via opt-in (FMA3 not baseline SSE2) |
| **Integer compute-in-place (Stage 1)** | ✅ | ✅ (2026-07-07) | parity (2-operand emit; 1-deep freelist) |
| ↳ CIP memory-load leaf — `a[i]` subscripts | ✅ | ✅ (2026-07-07) | rides Int-CIP |
| ↳ CIP strength reduction `x*2^k` → shift (Stage C) | ✅ | ✅ (2026-07-07) | rides Int-CIP |
| ↳ CIP integer `madd`/`msub` fusion (Stage E) | ✅ | ❌ (no int MAC → imul+add) | **architectural** |
| Constant promotion (Stage 2a) | ✅ x19..x28 | ✅ rbx/r12..r15 | parity (`emit_mov_phys`) |
| Wider B3 budget — caller-saved promotion in leaf fns | ✅ x9..x12 (10→14) | ❌ (keeps 5) | **a64 only** |
| Induction-variable pointer walk (`base[i]` → walking reg) | ✅ | ❌ `iv_supported`=0 (no post-index) | **a64 only** |
| RegAlloc v2 (liveness → linear-scan; shares one reg across non-overlapping live ranges + sees through inlined call sites) | ✅ int + float | ✅ int (2026-07-08, M1); float declined | int: parity (M1 — measured emit_node 676→137 frame accesses); float: **architectural** (no callee-saved XMM band). Caller-saved spilling (M2+) = big-league general quality, see `docs/plans/regalloc_v2_bigleague.md` |
| `fmov d, #imm` for AArch64-encodable float constants (1 insn) | ✅ | n/a | a64 closing its OWN 3-insn `adrp+add+ldr` cost for encodable consts; x64 already loads ANY float const in one `movsd [rip]`, so x64 is *ahead* on non-encodable constants |

## Bottom line for a port

For the question "how far behind is x64 when we port?": measured on `log_f`
(a float-heavy transcendental) the x64 codegen is **~2.7× larger** than a64
(~1008 vs ~368 bytes). That whole gap is the **float family** — float-CIP,
float-B3, `fmadd` — and it is **architectural, not debt**: SysV makes every
XMM caller-saved, so the d8..d15 promotion the a64 float wins depend on has no
x64 target. It will not close, and re-attempting it is the one thing this doc
exists to stop. For **integer / control-flow / SIMD / structural** code x64 is
at or near parity (inline, B3, const-promotion, multi-return, autovec, dynlink,
and — since 2026-07-07 — integer-CIP all real); the only genuine remaining x64
debt is the **integer** half of RegAlloc v2 (implementable). Integer-CIP on x64
is capped in SCOPE by the 1-deep GP freelist (a throughput ceiling, not a
correctness one) and stays low-value since integer serial code has ample
headroom. Practical reading: porting the audio/DSP engine to x64 will run
**correctly** but meaningfully slower per float sample — and since a64 has
~290× realtime headroom on the DAW render, even ~2.7× slower leaves comfortable
realtime margin. The slowdown is inherent to x86-64, not a backlog to burn down.

## Architectural gaps (permanent — do NOT "fix")

These are forced by the System V AMD64 ABI / baseline x86-64 ISA, not by
missing work. Re-deriving them wastes time; the answer is recorded here.

- **No callee-saved XMM → no CROSS-CALL float-B3, and no `fmadd`.** SysV makes
  ALL `xmm0..15` caller-saved (Win64 keeps `xmm6..15`; this is the divergence).
  **CORRECTED 2026-07-07 — the "no float CIP / float-B3" claim was too strong.**
  The a64-STYLE win (promoting floats into *callee-saved* d8..d15 so they survive
  calls) has no x64 target, true. But that only blocks promotion across CALLS. In
  a LEAF function (no calls) caller-saved xmm are never clobbered, so the float
  CIP (temps in xmm8..11) and float-B3 (promote the accumulator into xmm12..14)
  both work there — shipped, x64 self-host stable. Non-leaf functions still fall
  back to the value-stack. What stays genuinely architectural is **fused
  `fmadd`**: it needs FMA3 (Haswell+, x86-64-v3), not baseline SSE2 (would SIGILL
  on older CPUs), so the CIP decomposes to `mulsd + addsd` via the `has_float_fma`
  predicate. Net: x64 float is at parity for the common leaf DSP kernel, one
  rounding-per-op behind a64 (mul+add rounds twice, fmadd once — a legitimate
  numeric divergence, well below audio's threshold).
- **Shallow int freelist (1 vs 7).** SysV leaves far fewer caller-saved GP
  registers free than AAPCS64, so the B1 expression freelist is `r11` only
  (vs `x9..x15`). Not a bug — a register-budget fact. It's also *why* the
  integer-CIP gap below is low-value on x64.

## Deferred gaps (implementable — the spine + stubs are ready)

The expensive part (the spine machinery) is done and backend-agnostic; only
the chip-specific emission remains. Filling these in is a contained task.

### Integer compute-in-place (Stage 1) — ✅ SHIPPED on x64 (2026-07-07)

a64 emits `+ - * & | ^ /` integer trees destination-driven (no x0 shuttle); x64
now does too. The four emit stubs are filled (`_x64_emit_iop_into` /
`iload_var_into` / `iload_mem_into` / `ishl_imm_into`), and `_x64_int_temp_count`
returns the real freelist count (r11, 1 deep). Proven by the **x64 self-host**
(gen1 == gen2, the CIP compiler compiles itself byte-identically) + the bench
checksums + a64 byte-identical throughout.

**The three bugs it took to get there — all worth keeping, because two are about
the 2-operand ISA and one is a genuine SPINE latent bug the a64 3-operand emit
had been masking:**

1. **Destination-aliases-right-operand (the spine bug, and the broad one).** The
   integer assign-into-destination lever fires without a `cg_node_uses_var`
   guard — safe on a64 because `add dreg, s1, s2` reads BOTH sources then writes
   dreg atomically. On x64's 2-operand ISA `x = a + x` becomes
   `mov x, a; add x, x` = 2a. Accumulators / counters (`total = delta + total`)
   are everywhere, so enabling the x64 CIP broke the compiler's own parser
   broadly (a spurious `E101 unknown type` from miscompiled symbol resolution).
   Fixed in `_x64_emit_iop_into`: when `dreg == s2`, apply s1 in place for the
   commutative ops (`s2 OP s1 == s1 OP s2`) and `neg dreg; add dreg, s1` for
   subtraction. (The spine's own gate could ALSO grow a `cg_node_uses_var`
   check, but the chip-level fix keeps the win — `x = a + x` still computes in
   place — and is correct.)
2. **Integer madd fusion needs 2 temps and is useless on x64.** The spine fuses
   `(a*b)+c` → `emit_imadd` (a single a64 `madd`), computing both multiplicands
   into temps; on the 1-deep freelist the second `int_temp_alloc` returned −1.
   x86 has no integer FMA anyway, so a `has_int_madd` chip predicate (a64=1,
   x64=0) gates the fusion off and the CIP decomposes to imul+add.
3. **idiv/imadd must not blind-restore rax.** idiv leaves the quotient in rax;
   `dreg` CAN be rax (the return register), so save/restore rax/rdx only when
   they are NOT the destination.

The 1-deep freelist (r11) genuinely caps the *scope* (deeper trees fall back via
the `need <= count` gate) but is NOT a correctness blocker — the earlier
"needs a wider pool first" verdict was wrong; the wider pool is a pure
throughput optimisation, still **low-value** (integer serial code has ample
headroom). Docker caught every bug; the root cause came from reasoning about the
2-operand emit, and `bug --disasm` (which handles x86-64) would have found the
miscompiled function faster than objdump.

### Constant promotion (Stage 2a) — ✅ SHIPPED on x64 (no longer deferred)

Closed since this section was written: `_x64_b3_select_prim()` now chains
`_x64_b3_select_const()` (verified — `x64_primitives.bsm`), so x64 promotes hot
loop-invariant constants into spare callee-saved GP registers exactly like a64.
This is the parity row in the matrix above (`emit_mov_phys`); the old
"to implement" steps are kept out of the doc per the closing rule below. Left
here only as a marker that it was once deferred and is now done.

## When to update this doc

Any change that lands a codegen optimization on one backend but not the other
MUST add a row here (and stub the other backend per "Three Disciplines #2" in
the bootstrap manual). A change that closes a deferred gap moves its row to
parity and deletes its "to implement" section.
