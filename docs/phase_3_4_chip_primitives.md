# Phase 3.4 — chip_primitives Contract & Execution Plan

**Status**: Design complete, execution pending.
**Prerequisite**: Phases 1, 2, 3.1, 3.3 (done this session).
**Estimated effort**: 3-5 focused sessions.
**Target executor**: Emacs-side agent, reading this doc cold.

---

## Problem statement

After Phases 1-3.3, `a64_codegen.bsm` and `x64_codegen.bsm` still
contain the big tree-walkers that do instruction emission inline:

| Function | a64 lines | x64 lines | Shared structure? |
|---|---|---|---|
| `emit_node` | 1100+ | 894 | YES — same AST dispatch |
| `emit_stmt` | 450 | 275 | YES — same AST dispatch |
| `emit_func` | 298 | 158 | YES — prologue/epilogue pattern |
| `emit_all` | 103 | 31 | partial |
| `emit_module` | 83 | 45 | partial |

Each function is a `switch` on AST node type, and inside each case
there's `if (a64_bin_mode) { enc_XXX; } else { out("..."); }` that
emits chip-specific bytes or asm text.

**The portable dispatch should live once in `bpp_codegen.bsm`.
The emission lives once per chip as `chip_emit_*` primitives.**

After Phase 3.4:
- `bpp_codegen.bsm`: ~1500 lines (spine + helpers, shared by all chips)
- `a64_primitives.bsm`: ~400 lines (chip-specific emission only)
- `x64_primitives.bsm`: ~400 lines (chip-specific emission only)
- Porting to RISC-V: write `riscv_primitives.bsm` ≈ 400 lines. Done.

---

## The link-conflict problem and chosen solution

`bpp.bpp` imports BOTH `a64_codegen.bsm` and `x64_codegen.bsm`
(the compiler cross-compiles). They can't both define
`chip_emit_mov_imm()` — the linker would fail with duplicate symbol.

Three options considered:

### Option A: fn_ptr dispatch (CHOSEN)

Each chip defines its primitives with a chip-prefixed name:
- `_a64_emit_mov_imm()` in `a64_primitives.bsm`
- `_x64_emit_mov_imm()` in `x64_primitives.bsm`

`bpp_codegen.bsm` owns a struct of function pointers:

```
struct ChipPrimitives {
    emit_mov_zero,     // fn_ptr
    emit_mov_imm,      // fn_ptr
    emit_load_var,     // fn_ptr
    // ... 40 more
}

extrn cg_prim: ChipPrimitives;
```

Codegen init picks the right chip and populates `cg_prim`:

```
// In init_codegen_arm64():
cg_prim.emit_mov_zero = fn_ptr(_a64_emit_mov_zero);
cg_prim.emit_mov_imm  = fn_ptr(_a64_emit_mov_imm);
...

// In init_codegen_x86_64():
cg_prim.emit_mov_zero = fn_ptr(_x64_emit_mov_zero);
cg_prim.emit_mov_imm  = fn_ptr(_x64_emit_mov_imm);
...
```

The portable spine calls through the table:

```
cg_emit_node(nd) {
    if (nd == 0) { call(cg_prim.emit_mov_zero); return 0; }
    if (t == T_LIT) {
        ...
        call(cg_prim.emit_mov_imm, cg_parse_int(n.a));
        ...
    }
    ...
}
```

**Pros**:
- Zero link conflict (chip prefix keeps symbols distinct)
- Clean spine code (no branches)
- Natural "chip plug-in" model — new chip just fills the struct
- Cross-compile supported: switch `cg_prim` between calls

**Cons**:
- 1 pointer indirection per primitive call (negligible perf)
- Init code sets ~40 fn_ptrs (~40 lines of boilerplate per chip)

### Option B: runtime switch

```
cg_emit_node(nd) {
    if (nd == 0) {
        if (cg_target == TGT_A64) { _a64_emit_mov_zero(); }
        else { _x64_emit_mov_zero(); }
        return 0;
    }
    ...
}
```

Rejected: branch on every primitive call, spine grows, adds an
`if` at 200+ call sites.

### Option C: build-time selection

Modify `bpp_import.bsm` to conditionally include only the target
chip's primitives. Rejected: breaks cross-compile (bpp can only
target one chip at a time), requires compiler changes.

**Decision: Option A wins.** fn_ptr dispatch is clean, flexible,
and imposes zero cost on the common case.

---

## The chip_primitives contract

**45 primitives organized in 8 categories.** Every chip must
implement all of them. Adding a primitive is a breaking change
to the contract — bump a version counter and update all chips.

```
// ChipPrimitives struct, declared in bpp_codegen.bsm:

struct ChipPrimitives {
    // ── Arithmetic (8) ──
    emit_add,        // (rd, rs1, rs2, size)        integer add
    emit_sub,        // (rd, rs1, rs2, size)        integer sub
    emit_mul,        // (rd, rs1, rs2, size)        integer mul
    emit_div,        // (rd, rs1, rs2, size)        signed div
    emit_mod,        // (rd, rs1, rs2, size)        signed mod
    emit_neg,        // (rd, rs, size)              integer negate
    emit_fadd,       // (rd, rs1, rs2)              float add (double)
    emit_fsub,       // (rd, rs1, rs2)              float sub

    // ── Logical (6) ──
    emit_and,        // (rd, rs1, rs2)              bitwise and
    emit_or,         // (rd, rs1, rs2)              bitwise or
    emit_xor,        // (rd, rs1, rs2)              bitwise xor
    emit_shl,        // (rd, rs1, rs2)              shift left
    emit_shr,        // (rd, rs1, rs2)              shift right arithmetic
    emit_not,        // (rd, rs)                    bitwise not

    // ── Move / compare (5) ──
    emit_mov_zero,   // ()                          set result reg to 0
    emit_mov_imm,    // (imm)                       set result reg to imm
    emit_mov_reg,    // (rd, rs)                    move between regs
    emit_cmp,        // (rs1, rs2, size)            flags-setting compare
    emit_cmp_imm,    // (rs, imm, size)             compare with immediate

    // ── Memory (5) ──
    emit_load,       // (rd, rbase, off, size, signed)  load N bytes from [rbase+off]
    emit_store,      // (rsrc, rbase, off, size)        store N bytes to [rbase+off]
    emit_load_global,// (name_p, is_float)              load global var value
    emit_store_global,// (name_p, is_float)             store to global var
    emit_load_str_addr, // (str_id)                     load address of str literal

    // ── Control flow (7) ──
    emit_branch_eq,  // (label_id)                  branch if prev cmp was eq
    emit_branch_ne,  // (label_id)                  branch if ne
    emit_branch_lt,  // (label_id)                  branch if signed less
    emit_branch_ge,  // (label_id)                  branch if signed greater-equal
    emit_jump,       // (label_id)                  unconditional jump
    emit_label,      // (label_id)                  fix label at current position
    emit_call,       // (target_label)              call function by label

    // ── Stack / ABI (6) ──
    emit_push,       // (reg)                       push GP reg to stack
    emit_pop,        // (reg)                       pop GP reg from stack
    emit_fpush,      // (reg)                       push FP reg (double)
    emit_fpop,       // (reg)                       pop FP reg
    emit_frame_setup,// (frame_size)                prologue (save fp/lr, alloc frame)
    emit_frame_teardown,// (frame_size)             epilogue (restore fp/lr, dealloc)

    // ── Syscall (1) ──
    emit_syscall,    // (num_args)                  syscall using chip's ABI

    // ── Float literal (2) ──
    emit_load_float_const, // (flt_id)              load _flt_<id> address + value to d0
    emit_fcmp,       // (rs1, rs2)                  float compare (sets flags)

    // ── Register file metadata (5 — scalar returns, no fn_ptr) ──
    arg_reg,         // (i)        → register for i-th GP argument
    farg_reg,        // (i)        → register for i-th FP argument
    return_reg,      // ()         → GP return register
    freturn_reg,     // ()         → FP return register
    word_size,       // ()         → 4 or 8
}
```

**Metadata primitives** are implemented as regular functions
(`_a64_arg_reg(i)` etc.) called via fn_ptr dispatch too, since
they return scalars that vary per chip.

---

## Migration order

Case-by-case extraction of `emit_node` in 12 waves, ordered
simplest-first so early wins build confidence and debug infra.

Each wave:
1. Pick N cases from the list below.
2. For each case, identify which primitives are called.
3. Make sure those primitives exist in both `a64_primitives.bsm`
   and `x64_primitives.bsm`.
4. Rewrite the case body in `cg_emit_node` using primitive calls.
5. Delete the case from `a64_emit_node` and `x64_emit_node`.
6. Bootstrap: `./bpp src/bpp.bpp -o /tmp/bpp_waveN`
7. Byte-identity check: pathfind, rhythm, bang9 must all match
   baseline.

### Wave 1 — NULL + metadata (trivial)

- `nd == 0` → `emit_mov_zero`
- `T_SIZEOF` → `emit_mov_imm(size)`

### Wave 2 — literals

- `T_LIT` integer → `emit_mov_imm`
- `T_LIT` string → `emit_load_str_addr`
- `T_LIT` float → `emit_load_float_const`

### Wave 3 — simple var access

- `T_VAR` local (via emit_load_var primitive)
- `T_VAR` global int → `emit_load_global(name_p, 0)`
- `T_VAR` global float → `emit_load_global(name_p, 1)`

### Wave 4 — unary ops

- `T_UNARY` negate → `emit_neg`
- `T_UNARY` bitwise not → `emit_not`
- `T_UNARY` logical not → `cmp + branch` portable sequence

### Wave 5 — binary arithmetic

- `T_BINOP` `+`, `-`, `*`, `/`, `%` → `emit_add/sub/mul/div/mod`
- `T_BINOP` `&`, `|`, `^`, `<<`, `>>` → `emit_and/or/xor/shl/shr`

### Wave 6 — comparisons

- `T_BINOP` `==`, `!=`, `<`, `<=`, `>`, `>=` → `emit_cmp + branch`
  Uses a portable lowering: eval lhs, push, eval rhs, pop lhs, cmp,
  branch to set-true or fall-through-false.

### Wave 7 — control flow

- `T_IF` → `emit_branch + emit_label` pattern
- `T_WHILE` → loop header + body + back-branch
- `T_TERNARY` → shares T_IF lowering
- `T_RET` → eval expr + jump to epilogue label

### Wave 8 — memory access

- `T_MEMLD` byte/word/long → `emit_load`
- `T_MEMST` byte/word/long → `emit_store`
- `T_ADDR` → various addr computations (frame-relative, global)

### Wave 9 — function calls

Most complex. Each chip's ABI differs in detail (AAPCS64 vs
System V AMD64 vs future RISC-V). The primitive set needs:
- `emit_push_arg(val_reg, slot, is_float)` — put arg in ABI slot
- `emit_call_direct(target_label)` — emit the call
- `emit_load_ret(rd)` — copy return value from ABI-specified reg

### Wave 10 — assignments

- `T_ASSIGN` to local, global, deref pointer, struct field

### Wave 11 — struct/array

- `T_SUBSCRIPT` → scaled index + load
- `T_FIELD` → offset + load

### Wave 12 — emit_stmt migration

Apply the same pattern to `emit_stmt`, which dispatches on
statement-level AST nodes. Reuses all primitives from waves 1-11.

---

## Worked example: Wave 1 (the trivial case)

### Before

**`a64_codegen.bsm`:**

```
static a64_emit_node(nd) {
    auto n: Node;
    auto t, ...;
    if (nd == 0) {
        if (a64_bin_mode) { enc_movz(0, 0, 0); }
        else { out("  mov x0, #0"); putchar('\n'); }
        return 0;
    }
    n = nd; t = n.ntype;
    ...
}
```

**`x64_codegen.bsm`:**

```
static x64_emit_node(nd) {
    auto n: Node;
    auto t, ...;
    if (nd == 0) {
        x64_enc_xor_rax_rax();   // xor rax, rax — fastest zero
        return 0;
    }
    n = nd; t = n.ntype;
    ...
}
```

### After

**`bpp_codegen.bsm`:**

```
cg_emit_node(nd) {
    auto n: Node;
    auto t, ...;
    if (nd == 0) {
        call(cg_prim.emit_mov_zero);
        return 0;
    }
    n = nd; t = n.ntype;
    ...
}
```

**`a64_primitives.bsm`:**

```
// Set result reg (x0) to zero.
void _a64_emit_mov_zero() {
    if (a64_bin_mode) { enc_movz(0, 0, 0); }
    else { out("  mov x0, #0"); putchar('\n'); }
}
```

**`x64_primitives.bsm`:**

```
// Set result reg (rax) to zero using xor-self (shortest encoding).
void _x64_emit_mov_zero() {
    x64_enc_xor_rax_rax();
}
```

**Init setup in `init_codegen_arm64()`:**

```
cg_prim.emit_mov_zero = fn_ptr(_a64_emit_mov_zero);
// ... other primitives ...
```

**Init setup in `init_codegen_x86_64()`:**

```
cg_prim.emit_mov_zero = fn_ptr(_x64_emit_mov_zero);
// ... other primitives ...
```

### Byte-identity validation after Wave 1

```sh
./bpp src/bpp.bpp -o /tmp/bpp_wave1
for g in pathfind rhythm; do
    ./bpp games/$g/$g.bpp -o /tmp/${g}_baseline
    /tmp/bpp_wave1 games/$g/$g.bpp -o /tmp/${g}_wave1
    shasum /tmp/${g}_baseline /tmp/${g}_wave1
done
/tmp/bpp_wave1 bang9/bang9.bpp -o /tmp/b9_wave1
./bpp bang9/bang9.bpp -o /tmp/b9_baseline
shasum /tmp/b9_baseline /tmp/b9_wave1

# All 3 pairs must shasum-match. If any diff, roll back this wave.
```

---

## Worked example: Wave 9 (the hard case — T_CALL)

T_CALL is where the ABI bite happens. ARM64 (AAPCS64) and x86_64
(System V) both have the "first N args in registers, rest on
stack" pattern, but the specific registers differ:

| ABI | GP arg regs | FP arg regs | Stack slot size |
|---|---|---|---|
| AAPCS64 | x0..x7 | d0..d7 | 8 bytes (aligned 16) |
| System V | rdi, rsi, rdx, rcx, r8, r9 | xmm0..xmm7 | 8 bytes (aligned 16) |

**Primitives needed** (chip provides):

```
_a64_arg_reg(i) : base {           // AAPCS64 GP: x0..x7
    return i;
}
_x64_arg_reg(i) : base {           // System V: rdi, rsi, rdx, rcx, r8, r9
    if (i == 0) { return 7; }      // rdi
    if (i == 1) { return 6; }      // rsi
    if (i == 2) { return 2; }      // rdx
    if (i == 3) { return 1; }      // rcx
    if (i == 4) { return 8; }      // r8
    if (i == 5) { return 9; }      // r9
    return 0 - 1;                  // goes on stack
}
```

**Portable T_CALL in `cg_emit_node`:**

```
if (t == T_CALL) {
    auto arr, cnt, i, target_lbl, ret_is_float;
    arr = n.b; cnt = n.c;

    // 1. Evaluate args left-to-right into temp slots.
    // 2. Move each arg into its ABI register or stack slot.
    // 3. Emit call instruction.
    // 4. Copy return value to x0/rax (already there by ABI).

    for (i = 0; i < cnt; i = i + 1) {
        cg_emit_node(arr[i]);              // result in chip's ret_reg
        call(cg_prim.emit_push_arg, i, /* is_float= */ 0);
    }

    // Resolve callee.
    target_lbl = cg_find_fn_label(n.a);
    call(cg_prim.emit_call_direct, target_lbl);

    // Return is already in the ABI's return reg.
    ret_is_float = cg_is_fn_ret_float(n.a);
    return ret_is_float;
}
```

The `emit_push_arg` primitive handles the actual register move or
stack push per chip.

---

## Execution checklist (for the Emacs agent)

Before starting:
- [ ] Read this whole doc end to end
- [ ] Verify baseline: `./bpp src/bpp.bpp -o /tmp/bpp_baseline`
- [ ] Capture baseline hashes of pathfind, rhythm, bang9
- [ ] Confirm current state of `bpp_codegen.bsm`, `a64_codegen.bsm`,
      `x64_codegen.bsm` matches Phase 3.3 checkpoint
      (a64 ≈ 2852 lines, x64 ≈ 2004 lines, bpp_codegen ≈ 598 lines)

Per wave:
- [ ] Create/extend `src/backend/chip/aarch64/a64_primitives.bsm`
      with the wave's primitives
- [ ] Create/extend `src/backend/chip/x86_64/x64_primitives.bsm`
      with parallel primitives
- [ ] Add fn_ptr assignments to `init_codegen_arm64()` and
      `init_codegen_x86_64()`
- [ ] Rewrite the wave's cases in `cg_emit_node` (in
      `bpp_codegen.bsm`) using `call(cg_prim.X, args)`
- [ ] Delete those cases from `a64_emit_node` and `x64_emit_node`
- [ ] Bootstrap: `./bpp src/bpp.bpp -o /tmp/bpp_waveN`
- [ ] Byte-identity check: all 3 programs must match baseline
- [ ] If match: commit (or move to next wave if batch-committing)
- [ ] If mismatch: revert, investigate, retry

Completion:
- [ ] `cg_emit_node` is the authoritative tree walker; both
      `a64_emit_node` and `x64_emit_node` are gone
- [ ] `a64_primitives.bsm` + `x64_primitives.bsm` hold all
      chip-specific emission
- [ ] Byte-identity validated on 3+ non-trivial programs
- [ ] Line counts:
      - `a64_codegen.bsm` down to ~1000 (from 2852)
      - `x64_codegen.bsm` down to ~700 (from 2004)
      - `bpp_codegen.bsm` up to ~1600 (from 598)
      - `a64_primitives.bsm` ≈ 400
      - `x64_primitives.bsm` ≈ 400

---

## Apply same pattern to emit_stmt / emit_func

After Wave 12 (emit_node complete), repeat the process for:

### emit_stmt (Wave 13-16)

Statement-level dispatch. Reuses primitives from emit_node plus
a few new ones:
- `emit_label_name(name_p)` — emit named label for asm mode
- `emit_data_section` / `emit_text_section` — section directives

### emit_func (Wave 17-19)

Function prologue/epilogue. Main new primitives:
- `emit_frame_setup(frame_size)` — save fp/lr, alloc stack
- `emit_frame_teardown(frame_size)` — restore, dealloc
- `emit_save_callee_saved(regs)` — B3-promoted reg spills
- `emit_arg_copy(i)` — copy incoming arg from ABI reg to frame slot

---

## After Phase 3.4 — what it unlocks

With chip_primitives in place, adding a new chip becomes:

1. Write `riscv64_primitives.bsm` (~400 lines): one function per
   primitive, using the chip's instruction encodings.
2. Write `riscv64_enc.bsm` (~500 lines): the raw instruction
   byte emitters (this already exists for a64 and x64, equivalent
   effort for RISC-V).
3. Add fn_ptr assignments to a new `init_codegen_riscv64()`.
4. Add a target triple in the compiler dispatch.

**No changes to `bpp_codegen.bsm`.** The portable logic reuses.

Then Phase 5 (RISC-V validation) is ~1 week of careful porting
and QEMU testing. Phase 6 (install.sh chameleon) is ~1 day.

The "infecção parasita inteligente" vision unlocks the moment
chip_primitives is firmly in place.

---

## Pitfalls & warnings

1. **Do not touch `bpp.bpp`, `bpp_parser.bsm`, `bpp_types.bsm`,
   or `bpp_lexer.bsm`** during this refactor. They're frozen.

2. **Byte-identity is the hard invariant.** After every wave,
   validate. Don't batch waves hoping byte-identity "mostly"
   holds — one missed bug cascades.

3. **x64 has no asm mode.** Every a64 primitive has an
   `if (a64_bin_mode) { enc... } else { out... }` form; x64's
   parallel only has the binary path. This asymmetry is real
   and correct.

4. **Register numbering is chip-specific.** In a64, "reg 0" means
   x0. In x64, "reg 0" means rax. In RISC-V, "reg 10" means a0.
   Primitives take abstract reg numbers; each chip's impl maps
   to physical regs.

5. **Don't extract `a64_emit_mov` from `a64_codegen.bsm` — it's
   already a primitive wrapper, just call it from
   `_a64_emit_mov_imm`.**

6. **Float arg handling in `T_CALL`** needs care: each chip has
   separate FP arg registers from GP, and the codegen must track
   which arg is float to route correctly.

7. **The `a64_bin_mode` flag** stays chip-local (only a64 cares).
   Don't try to generalise it to a shared toggle.

---

## Contract version: v1

If any primitive is added, renamed, or removed: bump to v2 and
update `bpp_codegen.bsm`, `a64_primitives.bsm`, `x64_primitives.bsm`,
and any other chip implementations in lockstep. The `cg_prim`
struct is a hard interface.

---

*Written 2026-04-20 at the close of the claude-code MAX plan
session. Handed off to the Emacs-side agent for execution.*
