# MAX Plan — Codegen Core Split

**Goal.** Extract all portable code from
`src/backend/chip/aarch64/a64_codegen.bsm` (3189 lines) and
`src/backend/chip/x86_64/x64_codegen.bsm` (2308 lines) into a
single shared `src/backend/bpp_codegen.bsm`. Each chip backend
shrinks to a thin `chip_primitives` layer (~300 lines), making
B++ "Forth-portable": port to a new ISA by writing ~30 primitive
emit functions.

**Invariant (revised).** The `bpp` binary's OWN shasum shifts
harmlessly across the refactor — moving globals between source
files changes the data-section layout of the self-hosted
compiler, even when the emitted code is semantically identical.
The stronger and more useful invariant is:

> **Compiling any user program with the refactored `bpp` produces
> byte-identical output to the baseline `bpp`.**

This is what users actually care about: their games, their tools,
their IDE all compile to the exact same bytes before and after
the refactor. Any divergence here means a semantic change slipped
in — roll back immediately.

**Validation method** — after any Phase 1+ change:

```sh
# Baseline (captured before the refactor started):
./bpp games/pathfind/pathfind.bpp -o /tmp/pathfind_baseline
./bpp games/rhythm/rhythm.bpp    -o /tmp/rhythm_baseline
./bpp bang9/bang9.bpp            -o /tmp/bang9_baseline

# After the change, recompile bpp from source:
./bpp src/bpp.bpp -o /tmp/bpp_refactor

# Compile the same programs with the refactored bpp:
/tmp/bpp_refactor games/pathfind/pathfind.bpp -o /tmp/pathfind_refactor
/tmp/bpp_refactor games/rhythm/rhythm.bpp    -o /tmp/rhythm_refactor
/tmp/bpp_refactor bang9/bang9.bpp            -o /tmp/bang9_refactor

# All three pairs must hash-match:
shasum /tmp/pathfind_{baseline,refactor}
shasum /tmp/rhythm_{baseline,refactor}
shasum /tmp/bang9_{baseline,refactor}
```

**Phase 1 sub-step 1 validation** (switch globals `cg_sw_min/max/total`):

```
✓ games/pathfind/pathfind.bpp  d5124354cd40cc7673379a5772a7087940bf1671
✓ games/rhythm/rhythm.bpp      585e6de9643e8a3291f35e340514a6611d1d864d
✓ bang9/bang9.bpp              6703fc7cb5ff3fc7e826f2d8e386fdbfda25a194
```

All three byte-identical. The mechanism works.

---

## Current state (Phase 0: audit complete)

| File | Lines | Functions |
|------|-------|-----------|
| `chip/aarch64/a64_codegen.bsm` | 3189 | 54 |
| `chip/aarch64/a64_enc.bsm`     |  864 | encoder (chip-specific — stays) |
| `chip/x86_64/x64_codegen.bsm`  | 2308 | 40 |
| `chip/x86_64/x64_enc.bsm`      |  991 | encoder (chip-specific — stays) |
| `bpp_codegen.bsm`             |   ~20 | empty scaffold (ready) |

---

## Classification: what moves, what stays

Each function in the two `_codegen.bsm` files classifies as one
of three kinds:

- **[P] Portable** — no chip-specific emission. Moves to
  `bpp_codegen.bsm` with the prefix dropped.
- **[C] Chip-specific** — emits instruction bytes or asm text.
  Stays in the chip file, renamed to `chip_emit_*`.
- **[M] Mixed** — mostly portable but calls chip emitters mid-way.
  Splits: the portable spine moves; the emit sites become calls
  into `chip_primitives`.

### A64 audit

| Function | Kind | Target | Notes |
|---|---|---|---|
| `init_codegen_arm64` | M | split | state init (P), register mask seeds (C) |
| `a64_freelist_reset` | P | core | register allocator state only |
| `a64_gp_alloc` / `a64_gp_free` | P | core | GP register allocator |
| `a64_simd_alloc` / `a64_simd_free` | P | core | SIMD register allocator |
| `_a64_has_call` | P | core | AST walker |
| `a64_bind_startup_syms` | M | split | symbol binding (P), entry stub (C) |
| `bridge_data_arm64` | M | split | AST import (P), reloc setup (C) |
| `a64_find_ext` | P | core | extern lookup |
| `a64_find_ext_by_argc` | P | core | extern lookup |
| `a64_ext_par_is_float` | P | core | extern type query |
| `a64_ext_ret_is_float` | P | core | extern type query |
| `a64_str_eq` | P | core | string compare against sbuf |
| `a64_str_eq_packed` | P | core | packed string compare |
| `a64_print_p` | C | chip | asm-only (a64 uses `out()`) |
| `a64_print_dec` | C | chip | asm-only |
| `a64_var_add` | P | core | variable table append |
| `a64_var_idx` | P | core | variable table lookup by name |
| `_a64_b3_eligible` | P | core | B3 optimization analysis |
| `_a64_b3_walk` | P | core | B3 tree walk |
| `_a64_b3_select` | P | core | B3 selection pass |
| `a64_var_find` | P | core | |
| `a64_pre_reg_vars` | M | split | register-binding heuristic (P), reg names (C) |
| `a64_parse_int` | P | core | integer literal parser |
| `a64_is_float_lit` | P | core | float literal detection |
| `a64_var_type` | P | core | variable type query |
| `a64_var_is_float` | P | core | |
| `a64_emit_load_var` | C | chip | instruction emission |
| `a64_emit_store_var` | C | chip | |
| `a64_find_fn` | P | core | function table lookup |
| `a64_new_lbl` | P | core | label counter |
| `a64_emit_mov` | C | chip | ARM64 MOV encoding |
| `a64_switch_is_dense` | P | core | pure analysis |
| `a64_emit_switch_jtbl` | M | split | analysis (P), jump-table bytes (C) |
| `a64_emit_push/pop/fpush/fpop/vpush/vpop` | C | chip | stack ops |
| `_a64_vec_binop` | C | chip | ARM64 NEON encoding |
| `a64_emit_node` | M | split | tree dispatch (P), each emission site (C) |
| `a64_emit_body` | P | core | sequential body dispatcher |
| `a64_emit_stmt` | M | split | statement dispatch (P), emission (C) |
| `a64_emit_func` | M | split | prologue/epilogue logic split |
| `a64_mod_init` | M | split | global arrays setup (P), reg init (C) |
| `emit_module_arm64` | M | split | module dispatcher (P), per-node calls (C) |
| `emit_all_arm64` | M | split | top-level entry (P), per-module (C) |

### X64 audit

Parallel structure — same logical functions with `x64_` prefix,
but **no asm output path** (x64 is binary-only). That difference
means `a64_print_p` / `a64_print_dec` have no x64 parallel and
don't deduplicate during extraction.

| Function | Kind | Target | Notes |
|---|---|---|---|
| `init_codegen_x86_64` | M | split | same as a64 |
| `bridge_data_x86_64` | M | split | |
| `x64_bind_startup_syms` | M | split | |
| `x64_str_eq` | P | core | |
| `x64_str_eq_packed` | P | core | |
| `x64_var_add` | P | core | |
| `x64_var_idx` | P | core | |
| `x64_var_find` | P | core | |
| `_x64_b3_eligible` / `_walk` / `_select` | P | core | |
| `x64_pre_reg_vars` | M | split | |
| `x64_parse_int` | P | core | |
| `x64_is_float_lit` | P | core | |
| `x64_var_is_float` | P | core | |
| `x64_emit_load_var` / `_store_var` | C | chip | |
| `x64_find_fn` | P | core | |
| `x64_find_ext` / `_ext_by_argc` | P | core | |
| `x64_arg_reg` | C | chip | x86_64 System V / Win64 ABI dispatch |
| `x64_switch_is_dense` | P | core | |
| `x64_emit_switch_jtbl` | M | split | |
| `x64_emit_push/pop/vpush/vpop/fpush/fpop` | C | chip | |
| `_x64_vec_binop` | C | chip | SSE/AVX encoding |
| `x64_align_call` / `_unalign_call` | C | chip | x86_64 stack alignment |
| `x64_emit_node` | M | split | |
| `x64_emit_body` | P | core | |
| `x64_emit_stmt` | M | split | |
| `x64_emit_func` | M | split | |
| `x64_mod_init` | M | split | |
| `emit_module_x86_64` | M | split | |
| `x64_emit_start_stub` | C | chip | Linux-specific _start |
| `emit_all_x86_64` | M | split | |

### Summary counts

| Kind | a64 | x64 | Total | Disposition |
|------|-----|-----|-------|-------------|
| [P] Portable | 23 | 18 | 41 (deduplicates to ~22) | moves to `bpp_codegen.bsm` |
| [C] Chip-only | 17 | 16 | 33 | renamed `chip_emit_*`, stays |
| [M] Mixed | 14 | 6 | 20 | split — spine to core, emit to chip |

After the split:
- `bpp_codegen.bsm`: ~2000 lines (20 P functions + spines of 20 M functions)
- `a64_primitives.bsm`: ~400 lines (17 C + chip halves of 14 M)
- `x64_primitives.bsm`: ~350 lines (16 C + chip halves of 6 M)
- Total backend reduction: ~2000 lines of duplicated logic deleted.

---

## Shared globals that need to move

These are declared as `auto` in each `_codegen.bsm` but are
structurally identical — moving them to `bpp_codegen.bsm` with
a single name is the other half of the refactor.

| Current names | Shared name | Owner |
|---|---|---|
| `a64_sbuf` / `x64_sbuf` | `cg_sbuf` | bpp_codegen |
| `a64_vars` / `x64_vars` | `cg_vars` | |
| `a64_var_stack` / `x64_var_stack` | `cg_var_stack` | |
| `a64_var_struct_idx` / `x64_var_struct_idx` | `cg_var_struct_idx` | |
| `a64_var_off` / `x64_var_off` | `cg_var_off` | |
| `a64_var_forced_ty` / `x64_var_forced_ty` | `cg_var_forced_ty` | |
| `a64_var_refs` / `x64_var_refs` | `cg_var_refs` | |
| `a64_var_addr_taken` / `x64_var_addr_taken` | `cg_var_addr_taken` | |
| `a64_var_promote` / `x64_var_promote` | `cg_var_promote` | |
| `a64_promoted_regs` / `x64_promoted_regs` | `cg_promoted_regs` | |
| `a64_fn_name/par/pcnt/body/bcnt/fidx` | `cg_fn_*` | |
| `a64_gl_name` / `x64_gl_name` | `cg_gl_name` | |
| `a64_ex_name/ret/args/acnt` | `cg_ex_*` | |
| `a64_flt_tbl` / `x64_flt_tbl` | `cg_flt_tbl` | |
| `a64_str_tbl` / `x64_str_tbl` | `cg_str_tbl` | |
| `a64_lbl_cnt` / `x64_lbl_cnt` | `cg_lbl_cnt` | |
| `a64_depth` / `x64_depth` | `cg_depth` | |
| `a64_break_stack` / `x64_break_stack` | `cg_break_stack` | |
| `a64_continue_stack` / `x64_continue_stack` | `cg_continue_stack` | |
| `a64_cur_fn_name/idx` | `cg_cur_fn_*` | |
| `a64_bin_mode` (x64 is always bin) | `cg_bin_mode` (a64 only writes; x64 ignores) | |
| `a64_fn_lbl` / `x64_fn_lbl` | `cg_fn_lbl` | |
| `a64_ret_lbl` / `x64_ret_lbl` | `cg_ret_lbl` | |
| `a64_sw_min/max/total` / `x64_sw_min/max/total` | `cg_sw_min/max/total` | |

~40 globals. Deduplicated to ~40 (same count, but single name
now). Memory footprint halves — currently two sets of arrays are
allocated even though only one chip is active at a time.

**A64-only globals** (stay in a64 file, not shared):
- `a64_gp_free_mask` / `a64_simd_free_mask` — ARM register masks
- `a64_b3_spill_base` — ARM-specific B3 spill offset base

**X64-only globals** (stay in x64 file):
- `x64_argc_sym/argv_sym/envp_sym` — Linux-specific stub
- `x64_b3_spill_off` — x86-specific spill offset

---

## Phase plan

### Phase 0 — Audit (this doc) ✅

Done. This document captures what moves, what stays, and the
shared-global rename table.

### Phase 1.A / 1.B / 2 / 3.1 / 3.3 — COMPLETE (this session)

Done in sequence, byte-identity validated after each sub-step:

**Phase 1.A** — `os/` consolidation: `_core_<os>.bsm` replaces
the libc-style split (`_bsys`, `_brt0`, `_bmem`, `_stb_core`).
macOS: 7 → 5 files. Linux: 6 → 4 files.

**Phase 2 Blocks A-F** — Portable utilities extracted to
`bpp_codegen.bsm`:
- Block A: `cg_sbuf` + string helpers (str_eq, str_eq_packed,
  parse_int, is_float_lit)
- Block B: variable table (8 globals + var_add/idx/find)
- Block C: function/extern tables (11 globals + 5 helpers)
- Block D: switch_is_dense + cg_sw_* globals
- Block E: B3 optimisation (b3_eligible, b3_walk)
- Block F: counters/loop state + var_type, var_is_float, new_lbl

**Phase 3.1** — `cg_bridge_data` (parser → codegen state).
**Phase 3.3** — `cg_bind_startup_syms` + cg_argc/argv/envp_sym.

Line counts at end of session:

| file | start | after 3.3 | delta |
|------|-------|-----------|-------|
| a64_codegen.bsm | 3189 | 2852 | **−337** |
| x64_codegen.bsm | 2308 | 2004 | **−304** |
| bpp_codegen.bsm | 0 | 598 | +598 |

**Phase 3.4 — DESIGN COMPLETE, EXECUTION PENDING.**
See `docs/phase_3_4_chip_primitives.md` for the full contract
and 12-wave migration plan. Handed off to the Emacs-side agent.

---

### Phase 1 — Extract globals (in progress)

Strategy: rename all `a64_*` globals in `a64_codegen.bsm` to
`cg_*`, move their declarations to `src/bpp_codegen.bsm`, import
`bpp_codegen.bsm` from `a64_codegen.bsm`. Same for x64.

Each global rename is mechanical:
1. Add `auto cg_X;` in `bpp_codegen.bsm`
2. Replace `a64_X` with `cg_X` in `a64_codegen.bsm` (globals only,
   not function names)
3. Replace `x64_X` with `cg_X` in `x64_codegen.bsm`
4. Delete the `auto a64_X;` and `auto x64_X;` declarations
5. Bootstrap + verify user-program byte-identity

**Phase 1 sub-step 1 complete.** Globals `cg_sw_min`, `cg_sw_max`,
`cg_sw_total` (switch-analysis state) moved to bpp_codegen.
a64's 23 references renamed in one pass. Byte-identity confirmed
for pathfind, rhythm, bang9.

Remaining subgroups (each is one sub-step with its own validation):
- `cg_sbuf` — the AST string buffer (both chips reference)
- `cg_vars`, `cg_var_*` — variable table (10 parallel arrays)
- `cg_fn_name/par/pcnt/body/bcnt/fidx` — function table
- `cg_gl_name` — global variable list
- `cg_ex_name/ret/args/acnt` — extern table
- `cg_flt_tbl`, `cg_str_tbl` — literal pools
- `cg_lbl_cnt`, `cg_depth` — counters
- `cg_break_stack`, `cg_continue_stack` — loop stacks
- `cg_cur_fn_name/idx` — current function context
- `cg_bin_mode` — a64 only (x64 always binary)
- `cg_fn_lbl`, `cg_ret_lbl` — function label ids
- `cg_promoted_regs` — B3 promotion (different per chip?)

Expected effort: ~1 session for remaining globals. Each sub-step
bootstraps and validates before moving to the next.

### Phase 2 — Move [P] functions (next sessions)

For each portable function (in priority order, smallest first):
1. Copy function body to `bpp_codegen.bsm` with prefix dropped
2. In `a64_codegen.bsm` + `x64_codegen.bsm`, replace the body
   with a single forward call to the shared version
3. Bootstrap, check shasum
4. Once both chips compile against it, **delete** the chip-local
   forwarders. Body now lives only in bpp_codegen.

Candidate order (smallest + lowest-dependency first):
1. `str_eq_packed` (~10 lines, no deps)
2. `str_eq` (~5 lines)
3. `parse_int` (~20 lines)
4. `is_float_lit` (~10 lines)
5. `var_is_float` (~10 lines)
6. `var_add` / `var_idx` / `var_find` (table ops, ~30 lines)
7. `find_fn` / `find_ext` / `find_ext_by_argc` (~30 lines)
8. `_b3_eligible` / `_b3_walk` / `_b3_select` (~80 lines — the
   B3 optimisation pass, biggest portable block)
9. `switch_is_dense` (~50 lines)
10. `new_lbl` (~3 lines — trivial but good closure)

Expected effort: ~2 sessions for all 22 portable functions.

### Phase 3 — Split [M] functions (most delicate)

The mixed functions include the beasts: `emit_node` (~1100 lines
in a64), `emit_stmt` (~450), `emit_func` (~300). These need
careful extraction of the tree-walking spine from the emission
sites.

Strategy: go through each chip-specific `if (t == T_KIND)` block
and replace the emission code with a call to `chip_emit_*`. The
`chip_emit_*` functions go into `a64_primitives.bsm` /
`x64_primitives.bsm` (new files).

The chip_primitives contract (see header of `bpp_codegen.bsm`
for the 39-function list) is locked in Phase 3 — adding a new
primitive forces updating all chips. Design discipline matters.

Expected effort: ~5 sessions (the heavy lifting).

### Phase 4 — Deduplicate [M] spines

After Phase 3, `a64_codegen.bsm` has `emit_node` with the same
tree-walk dispatch as `x64_codegen.bsm`'s `emit_node`. Move the
shared spine to `bpp_codegen.bsm`, leaving only
`chip_primitives` in the chip files.

Expected effort: ~2 sessions.

### Phase 5 — Validation: port RISC-V

Write `src/backend/chip/riscv64/` from scratch using only the
`chip_primitives` contract. If it compiles + runs a small
program on QEMU-riscv64, the contract is proven sound. If not,
the interface needs refinement — go back and adjust.

Expected effort: ~5-10 sessions.

### Phase 6 — `install.sh` chameleon

Detect `uname -m` + `uname -s`. If known backend exists, use it.
If unknown chip but C transpiler exists, emit a warning and fall
back to `--c` mode. Install-time graceful degradation.

Expected effort: ~1 session.

---

## Anti-goals

- **No semantic changes.** Every shasum check confirms this.
- **No new optimisations.** DCE, CSE, peephole — leave as-is.
- **No API changes** for user programs. `bpp foo.bpp -o bar`
  works identically before and after.
- **No cleanup of encoding quirks.** If `a64_enc.bsm` has weird
  encoding of some instruction, that stays. Different battle.
- **No compiler extensions.** Parser, types, dispatch — all
  frozen during the refactor.

---

## Open questions

1. **Asm mode in x64.** a64 has `out("...")` asm mode; x64 is
   binary-only. Does bpp_codegen carry an `IF bin_mode` fork
   throughout, or is asm mode a chip-specific concern that
   doesn't even make it into core? Recommendation: **keep asm
   mode chip-local to a64**. Core emits via abstract primitives;
   a64's primitives internally decide asm-vs-bin. Zero state
   leaks to core.

2. **Ordering of `stack` vs `struct_idx` vs `off` arrays.** These
   are parallel arrays indexed by `cg_var`. During rename, verify
   no code accesses them by raw pointer arithmetic (it shouldn't —
   all access is via `arr_get` / `arr_push` — but grep-confirm
   before the rename).

3. **B3 optimization pass.** `_a64_b3_spill_base` is an a64-local
   offset; `_x64_b3_spill_off` is an x64-local offset. They have
   the same role but different semantics (base vs direct offset).
   Keep both as chip-local; the B3 walker functions themselves
   (eligibility + selection) are portable.

---

*Started 2026-04-20. Phase 0 complete. Byte-identity baseline:
`8c272d4a01e751ee92d98c19285ef89981684c6a`.*
