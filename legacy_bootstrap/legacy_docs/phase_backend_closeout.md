# Backend Closeout — Phase 3.4 Wave 9 + Phase 3.5 + Phase 4

**Status**: Design complete, execution pending.
**Prerequisite**: Phase 3.4 at commit `122cdaf` (9/12 waves shipped,
35/60 slots wired, byte-identity held across 12 commits).
**Target**: Emacs-side agent, reading this doc cold.
**Estimated effort**: 3-4 focused sessions. Gate on byte-identity
every wave — no shortcuts.

---

## Why this doc exists

After the 2026-04-20 Phase 3.4 session, `cg_emit_node` in
`bpp_codegen.bsm` is authoritative for every AST node **except
T_CALL**. `emit_func`, `emit_module`, `emit_all` still live
per-chip. To close the backend and unlock RISC-V, three chunks
remain:

1. **Wave 9** — migrate T_CALL. The ABI beast. Its own session.
2. **Phase 3.5** — migrate `emit_func` (prologue / body call /
   epilogue). 3 waves, call them 13, 14, 15.
3. **Phase 4** — consolidate `emit_module` + `emit_all` spines.
   2 waves (16, 17).

After all three, each chip file is pure `_primitives.bsm` + a
thin wrapper in `_codegen.bsm` for init + encoder plumbing.

---

## Pre-flight (mandatory, once per session)

Verify the Phase 3.4 baseline. Hashes from session close:

```sh
./bpp src/bpp.bpp -o /tmp/bpp_baseline
shasum /tmp/bpp_baseline
# expect the Phase 3.4 commit 122cdaf's bpp hash

./bpp games/pathfind/pathfind.bpp -o /tmp/pathfind_baseline
./bpp games/rhythm/rhythm.bpp    -o /tmp/rhythm_baseline
./bpp bang9/bang9.bpp            -o /tmp/bang9_baseline
shasum /tmp/pathfind_baseline /tmp/rhythm_baseline /tmp/bang9_baseline
# expect:
#   pathfind  50caa64bfa7f4476d0780c5857304db66176d852
#   rhythm    3d4f424b2ae7071110d8962750aaa700f2c57009
#   bang9     7a76c3b8f6d9cb7021cb4a221f5c9980accdee02
```

If ANY hash mismatches: tree drifted. Stop, investigate.

**Also check file inventory at start:**

```sh
wc -l src/backend/chip/aarch64/a64_codegen.bsm \
      src/backend/chip/x86_64/x64_codegen.bsm \
      src/bpp_codegen.bsm \
      src/backend/chip/aarch64/a64_primitives.bsm \
      src/backend/chip/x86_64/x64_primitives.bsm

# expect (post Phase 3.4):
#   a64_codegen.bsm      2684
#   x64_codegen.bsm      1965
#   bpp_codegen.bsm       845
#   a64_primitives.bsm    558
#   x64_primitives.bsm    283
```

---

## SECTION A — Phase 3.4 Wave 9: T_CALL

### Why this is a whole wave to itself

T_CALL is ~770 lines of chip-specific code because it handles:

1. **Argument evaluation order** — left to right, results accumulate.
2. **Argument classification** — int vs float determines which
   ABI register family gets used.
3. **Register allocation across ABI slots** — AAPCS64 uses x0-x7
   for int, d0-d7 for float, independently counted. System V uses
   rdi/rsi/rdx/rcx/r8/r9 for int, xmm0-xmm7 for float.
4. **Stack spill** — args beyond register count push on stack with
   chip-specific alignment.
5. **Caller-saved spill** — x8-x18 on a64, rax/rcx/rdx/rdi/rsi/r8-r11
   on x64.
6. **Return value** — int in x0/rax, float in d0/xmm0.
7. **Stack re-alignment** before call (16-byte boundary).

### New primitives for Wave 9

Add to `ChipPrimitives` struct (reserved slots already in place):

```
// ── T_CALL support ──
// Each chip implements its ABI-aware arg slot resolution and
// actual call emission. Spine in cg_emit_node walks the args
// and asks the chip where each one goes.

arg_reg_int,    // (slot_idx) → physical GP reg, or -1 if spills
arg_reg_flt,    // (slot_idx) → physical FP reg, or -1 if spills
arg_reg_count_int, // () → 8 (a64) or 6 (x64)
arg_reg_count_flt, // () → 8 (a64) or 8 (x64)

emit_push_arg_int,  // (reg_dst_or_spill_off, is_spill)
                    // chip moves int arg from x0/rax into the slot
emit_push_arg_flt,  // (reg_dst_or_spill_off, is_spill)
                    // chip moves float arg from d0/xmm0 into slot

emit_pre_call_align,  // (stack_args_count) → chip aligns sp
                      // before call, returns bytes of padding added
emit_post_call_unalign, // (padding_bytes)
                        // chip restores sp after call

emit_call_direct,   // (target_label_id) → branch + link
emit_copy_ret_int,  // () → no-op on most chips (return already in x0/rax)
emit_copy_ret_flt,  // () → no-op on most chips (return already in d0/xmm0)

// Caller-saved spill/restore for live regs crossing the call.
// spine doesn't need to manage specific regs; chip primitive
// knows its caller-saved set.
emit_save_caller_saved_int,  // () → spill all live int callers to stack
emit_restore_caller_saved_int, // () → undo the above

// Extern (FFI) calls differ: extern may use C symbol name instead
// of internal function label, and may need relocation entry vs
// internal fixup.
emit_call_extern,   // (ex_idx) → chip resolves to FFI symbol + reloc
```

**13 new primitives.** Reserve slots in struct up front.

### The portable T_CALL in cg_emit_node

```
if (t == T_CALL) {
    auto arr, cnt, i, fi, exi;
    auto int_slot, flt_slot, stack_slot, pad_bytes;
    auto fn_name: Node;
    auto is_extern, is_float_ret;

    fn_name = n.a;       // function name node
    arr = n.b;           // arg nodes
    cnt = n.c;           // arg count

    // Resolve callee — is it a declared fn, an extern, or unknown?
    fi = cg_find_fn(fn_name.a);
    is_extern = (fi < 0);
    if (is_extern) {
        exi = cg_find_ext_by_argc(fn_name.a, cnt);
        if (exi < 0) {
            // Unknown symbol — emit a builtin dispatch (peek/poke/etc)
            // or error. Delegate to chip's emit_call_builtin primitive.
            cg_emit_call_builtin(fn_name.a, arr, cnt);
            return 0;
        }
    }

    // Count how many go on stack (used for pre-call alignment).
    auto int_in_regs, flt_in_regs, stack_int, stack_flt;
    int_in_regs = 0; flt_in_regs = 0;
    stack_int = 0; stack_flt = 0;
    for (i = 0; i < cnt; i = i + 1) {
        auto is_float_arg;
        if (is_extern) {
            is_float_arg = cg_ext_par_is_float(exi, i);
        } else {
            is_float_arg = cg_fn_par_is_float(fi, i);  // new helper
        }
        if (is_float_arg) {
            if (flt_in_regs < call(cg_prim.arg_reg_count_flt)) {
                flt_in_regs = flt_in_regs + 1;
            } else { stack_flt = stack_flt + 1; }
        } else {
            if (int_in_regs < call(cg_prim.arg_reg_count_int)) {
                int_in_regs = int_in_regs + 1;
            } else { stack_int = stack_int + 1; }
        }
    }

    // Pre-call stack alignment (chip decides how).
    pad_bytes = call(cg_prim.emit_pre_call_align, stack_int + stack_flt);

    // Save caller-saved regs that are live across the call.
    // The chip knows its caller-saved set.
    call(cg_prim.emit_save_caller_saved_int);

    // Evaluate each arg and route to its ABI slot or stack.
    int_slot = 0; flt_slot = 0; stack_slot = 0;
    for (i = 0; i < cnt; i = i + 1) {
        auto is_float_arg, arg_is_float_result;

        if (is_extern) {
            is_float_arg = cg_ext_par_is_float(exi, i);
        } else {
            is_float_arg = cg_fn_par_is_float(fi, i);
        }

        // Evaluate arg — result in x0 (int) or d0 (float).
        arg_is_float_result = cg_emit_node(arr[i]);

        // If arg expected float but produced int (or vice versa),
        // emit a cvt primitive. For now assume parser/typer
        // enforced matching — leave coercion for later pass.

        if (is_float_arg) {
            auto slot = call(cg_prim.arg_reg_flt, flt_slot);
            if (slot >= 0) {
                call(cg_prim.emit_push_arg_flt, slot, 0);
                flt_slot = flt_slot + 1;
            } else {
                call(cg_prim.emit_push_arg_flt, stack_slot * 8, 1);
                stack_slot = stack_slot + 1;
            }
        } else {
            auto slot = call(cg_prim.arg_reg_int, int_slot);
            if (slot >= 0) {
                call(cg_prim.emit_push_arg_int, slot, 0);
                int_slot = int_slot + 1;
            } else {
                call(cg_prim.emit_push_arg_int, stack_slot * 8, 1);
                stack_slot = stack_slot + 1;
            }
        }
    }

    // Emit the call.
    if (is_extern) {
        call(cg_prim.emit_call_extern, exi);
        is_float_ret = cg_ext_ret_is_float(exi);
    } else {
        call(cg_prim.emit_call_direct, arr_get(cg_fn_lbl, fi));
        is_float_ret = cg_fn_ret_is_float(fi);  // new helper
    }

    // Undo pre-call alignment.
    call(cg_prim.emit_post_call_unalign, pad_bytes);

    // Restore caller-saved.
    call(cg_prim.emit_restore_caller_saved_int);

    // Return value is already in x0 (int) or d0 (float) per ABI.
    return is_float_ret;
}
```

Note the **2 new portable helpers** that the spine needs:

- `cg_fn_par_is_float(fi, i)` — queries the parser's function
  records for the i-th param type
- `cg_fn_ret_is_float(fi)` — queries the function's return type

Both go in `bpp_codegen.bsm`, use existing parser state
(`get_fn_param_type(fi, i)`, `get_fn_ret_type(fi)`).

### Chip implementations of the new primitives (a64)

```
// a64_primitives.bsm — append

// AAPCS64: x0..x7 for int, d0..d7 for float, 8 each.
_a64_arg_reg_int(i) : base {
    if (i < 8) { return i; }
    return 0 - 1;
}
_a64_arg_reg_flt(i) : base {
    if (i < 8) { return i; }
    return 0 - 1;
}
_a64_arg_reg_count_int() : base { return 8; }
_a64_arg_reg_count_flt() : base { return 8; }

// Before arg eval, push x0 to stack; after we have the arg value,
// move it to the destination reg or stack slot.
void _a64_emit_push_arg_int(dst_or_off, is_spill) {
    if (is_spill != 0) {
        // Stack spill: str x0, [sp, #off]
        if (a64_bin_mode) { enc_str_uoff(0, 31, dst_or_off); }
        else { out("  str x0, [sp, #"); a64_print_dec(dst_or_off); out("]"); putchar('\n'); }
    } else {
        // Move x0 to xN.
        if (dst_or_off == 0) { return; }   // already in x0
        if (a64_bin_mode) { enc_mov_reg(dst_or_off, 0); }
        else { out("  mov x"); a64_print_dec(dst_or_off); out(", x0"); putchar('\n'); }
    }
}
void _a64_emit_push_arg_flt(dst_or_off, is_spill) {
    if (is_spill != 0) {
        if (a64_bin_mode) { enc_str_d_uoff(0, 31, dst_or_off); }
        else { out("  str d0, [sp, #"); a64_print_dec(dst_or_off); out("]"); putchar('\n'); }
    } else {
        if (dst_or_off == 0) { return; }
        if (a64_bin_mode) { enc_fmov_d_reg(dst_or_off, 0); }
        else { out("  fmov d"); a64_print_dec(dst_or_off); out(", d0"); putchar('\n'); }
    }
}

// Pre-call alignment. a64 requires SP 16-byte aligned at call.
// stack_args * 8 rounded up to 16.
_a64_emit_pre_call_align(stack_args) {
    auto pad;
    pad = stack_args * 8;
    if (pad % 16 != 0) { pad = pad + 8; }   // round up
    if (pad > 0) {
        if (a64_bin_mode) { enc_sub_imm(31, 31, pad); }
        else { out("  sub sp, sp, #"); a64_print_dec(pad); putchar('\n'); }
    }
    return pad;
}
void _a64_emit_post_call_unalign(pad_bytes) {
    if (pad_bytes > 0) {
        if (a64_bin_mode) { enc_add_imm(31, 31, pad_bytes); }
        else { out("  add sp, sp, #"); a64_print_dec(pad_bytes); putchar('\n'); }
    }
}

// Direct call: bl label.
void _a64_emit_call_direct(target_lbl) {
    if (a64_bin_mode) { enc_bl_label(target_lbl); }
    else { out("  bl "); a64_print_decorated_label(target_lbl); putchar('\n'); }
}

// Extern call: bl _<symname> with reloc.
void _a64_emit_call_extern(exi) {
    auto name_p;
    name_p = arr_get(cg_ex_name, exi);
    if (a64_bin_mode) {
        enc_add_reloc(enc_pos, name_p, 3);  // reloc type 3 = extern call
        enc_bl_placeholder();
    } else {
        out("  bl _"); a64_print_p(name_p); putchar('\n');
    }
}

// Return copy no-ops (ret value already in correct reg per ABI).
_a64_emit_copy_ret_int() { return 0; }
_a64_emit_copy_ret_flt() { return 0; }

// Caller-saved spill: x9..x15 are the scratch caller-saved set
// B++ currently uses. Spill all to frame scratch area.
// This uses the chip's own convention — B3 promoted regs (x19+)
// are callee-saved and don't need spill here.
void _a64_emit_save_caller_saved_int() {
    // Chip decides scope. Existing emit_node's T_CALL already
    // does this inline — extract to primitive preserving behavior.
    // PLACEHOLDER: inspect current T_CALL in a64_emit_node for
    // exact spill set and reproduce.
}
void _a64_emit_restore_caller_saved_int() {
    // mirror of save
}
```

### Chip implementations (x64, condensed)

System V ABI differs mainly in:
- Arg regs: rdi (7), rsi (6), rdx (2), rcx (1), r8 (8), r9 (9)
- 6 int regs, 8 FP regs
- Pre-call align: rsp must be 16 % 0 **before** CALL instruction
  (after CALL pushes ret addr, 16 % 8 inside callee)

```
_x64_arg_reg_int(i) : base {
    if (i == 0) { return 7; }   // rdi
    if (i == 1) { return 6; }   // rsi
    if (i == 2) { return 2; }   // rdx
    if (i == 3) { return 1; }   // rcx
    if (i == 4) { return 8; }   // r8
    if (i == 5) { return 9; }   // r9
    return 0 - 1;
}
_x64_arg_reg_int_count() : base { return 6; }

void _x64_emit_push_arg_int(dst_or_off, is_spill) {
    if (is_spill != 0) {
        x64_enc_mov_mem(dst_or_off, 0);   // mov [rsp+off], rax
    } else {
        if (dst_or_off == 0) { return; }   // already in rax
        x64_enc_mov_reg(dst_or_off, 0);
    }
}
// ...etc
```

### Wave 9 validation

Same byte-identity gate as all prior waves. Because T_CALL is
pervasive, ANY call in pathfind / rhythm / bang9 exercises this.
If ANY of the 3 hashes differ → roll back Wave 9, investigate.

Expected tests hit during verification:
- Integer-only calls (`rand()`, `malloc(1024)`)
- Float-arg calls (`draw_circle(x, y, r, color)` — x/y/r are int
  but color is often float-in-packed via rgba())
- Extern calls via objc_msgSend (Cocoa)
- Calls with > 8 int args (rare but possible in parser bridge)

### Expected line delta

```
a64_codegen.bsm:      -350 (T_CALL case removed)
x64_codegen.bsm:      -420 (T_CALL case + caller-saved wrappers)
bpp_codegen.bsm:      +130 (cg_emit_node T_CALL + helpers)
a64_primitives.bsm:   +200 (13 new primitives)
x64_primitives.bsm:   +200
```

Net: chip surface shrinks ~770 lines, shared + primitives grow ~530.

---

## SECTION B — Phase 3.5: emit_func (Waves 13, 14, 15)

### Overview

`emit_func` has three phases:

1. **Prologue** (Wave 13) — save fp/lr, allocate frame, save
   B3-promoted callee-saved regs, emit function label.
2. **Arg copy + body** (Wave 14) — copy incoming ABI-reg args
   into frame slots, then dispatch to `cg_emit_body` (already
   portable).
3. **Epilogue** (Wave 15) — restore callee-saved, deallocate
   frame, restore fp/lr, ret.

### Wave 13 — Prologue

**New primitives**:

```
emit_fn_entry_label,  // (name_p, fn_lbl_id)
                      // asm: `_name:` + .globl directive
                      // bin: bind label to current enc_pos
emit_frame_setup,     // (frame_size)
                      // stp x29, x30, [sp, #-N]!
                      // add x29, sp, #0
                      // sub sp, sp, #M (if frame > 504)
emit_save_callee_saved, // (regs_arr, count, base_offset)
                        // B3 promoted regs → frame slots
```

**Portable spine** (`cg_emit_func_prologue`):

```
cg_emit_func_prologue(fi) {
    auto name_p, frame_size, promoted_regs;
    name_p = arr_get(cg_fn_name, fi);
    cg_cur_fn_idx = arr_get(cg_fn_fidx, fi);
    cg_cur_fn_name = name_p;

    call(cg_prim.emit_fn_entry_label, name_p, arr_get(cg_fn_lbl, fi));

    // Pre-reg vars to count locals + structs.
    // (cg_pre_reg_vars already portable from Block B)
    arr_clear(cg_vars);
    arr_clear(cg_var_stack);
    ... (reset all var-table arrays)
    cg_pre_reg_vars(arr_get(cg_fn_body, fi), arr_get(cg_fn_bcnt, fi));

    // Run B3 pass to decide which locals get registers.
    for (auto i = 0; i < arr_len(cg_vars); i = i + 1) {
        arr_set(cg_var_refs, i, 0);
    }
    cg_b3_walk(arr_get(cg_fn_body, fi));
    call(cg_prim.b3_select);  // chip-specific reg selection

    // Compute frame size (portable — sum of local slot sizes).
    frame_size = cg_compute_frame_size(fi);

    // Emit frame setup.
    call(cg_prim.emit_frame_setup, frame_size);

    // Save B3-promoted callee-saved regs to frame spill area.
    call(cg_prim.emit_save_callee_saved,
         cg_promoted_regs_ptr(),
         cg_promoted_regs_count(),
         cg_b3_spill_base());
}
```

Complication: **`cg_promoted_regs_ptr()` and `cg_b3_spill_base()`
wrap chip-local arrays** (`a64_promoted_regs`, `a64_b3_spill_base`).
These are already chip-specific. Either:
- Expose via primitives that return the raw pointer + count
- Keep `b3_select` as a primitive that populates a shared
  `cg_promoted_regs` array

Second option cleaner. **New primitive**:

```
b3_select,  // () → runs B3 selection, populates cg_promoted_regs
            // and sets cg_b3_spill_base (both shared)
```

So declare in bpp_codegen.bsm:
```
auto cg_promoted_regs;  // shared, written by chip's b3_select
auto cg_b3_spill_base;  // shared, set by chip's b3_select
```

Chip's b3_select writes into cg_* and THEN the chip can also
populate its own a64_promoted_regs (legacy) if needed. Over time
the chip-local duplicates get deleted.

### Wave 14 — Arg copy + body dispatch

**New primitives**:

```
emit_arg_copy_int,  // (param_idx, frame_off)
                    // move xN → [x29, #off] for i-th int param
emit_arg_copy_flt,  // (param_idx, frame_off)
                    // move dN → [x29, #off] for i-th float param
```

**Spine**:

```
cg_emit_func_body(fi) {
    auto par_arr, par_cnt, i;
    par_arr = arr_get(cg_fn_par, fi);
    par_cnt = arr_get(cg_fn_pcnt, fi);

    // Copy each incoming ABI-reg arg into its frame slot.
    for (i = 0; i < par_cnt; i = i + 1) {
        auto par_name, vi, off, is_float;
        par_name = *(par_arr + i * 8);
        vi = cg_var_idx(par_name);
        if (vi < 0) { continue; }
        off = arr_get(cg_var_off, vi);
        is_float = cg_fn_par_is_float(fi, i);
        if (is_float) {
            call(cg_prim.emit_arg_copy_flt, i, off);
        } else {
            call(cg_prim.emit_arg_copy_int, i, off);
        }
    }

    // Dispatch body — cg_emit_body already portable.
    cg_depth = 0;
    cg_emit_body(arr_get(cg_fn_body, fi), arr_get(cg_fn_bcnt, fi));
}
```

### Wave 15 — Epilogue

**New primitive**:

```
emit_frame_teardown,  // (frame_size, promoted_regs, count, base_off)
                      // restore callee-saved, dealloc frame,
                      // restore fp/lr, ret
```

**Spine**:

```
cg_emit_func_epilogue(fi) {
    auto frame_size;
    frame_size = cg_compute_frame_size(fi);

    // Implicit return 0 for fns without explicit return.
    call(cg_prim.emit_mov_zero);

    // Emit return label (where `return` statements jump to).
    call(cg_prim.emit_label, cg_ret_lbl);

    // Restore + teardown.
    call(cg_prim.emit_frame_teardown,
         frame_size,
         cg_promoted_regs_ptr(),
         cg_promoted_regs_count(),
         cg_b3_spill_base);
}
```

### Putting Phase 3.5 together

After Waves 13-15, `emit_func` is fully migrated:

```
// bpp_codegen.bsm
cg_emit_func(fi) {
    cg_emit_func_prologue(fi);
    cg_emit_func_body(fi);
    cg_emit_func_epilogue(fi);
}

// a64_codegen.bsm — emit_func DELETED entirely
// x64_codegen.bsm — emit_func DELETED entirely
```

Expected line delta:
```
a64_codegen.bsm:   -340 (emit_func removed)
x64_codegen.bsm:   -200
bpp_codegen.bsm:   +120 (cg_emit_func + 3 sub-fns + helpers)
a64_primitives.bsm: +80 (5 new primitives)
x64_primitives.bsm: +60
```

---

## SECTION C — Phase 4: emit_module + emit_all

### Wave 16 — emit_module

`emit_module_arm64` and `emit_module_x86_64` both do:

1. Reset module-local state (fn labels, local vars).
2. Run cg_bridge_data (already portable).
3. Allocate a label for each function in the current module.
4. Emit each function (cg_emit_func — portable after Phase 3.5).
5. Resolve forward branches (chip-specific encoder call).

**New primitives**:

```
emit_module_setup,  // () → encoder reset, label array init
emit_module_finalize, // () → resolve_fixups, any post-module chip work
alloc_fn_labels,    // () → call enc_new_label() per cg_fn_name entry;
                    //      chip stores in its fn_lbl array
```

**Spine**:

```
cg_emit_module(mi) {
    auto fn_start, i;

    fn_start = arr_len(cg_fn_name);

    // Merge this module's functions/externs into the cg_* arrays.
    cg_bridge_module_append(mi);  // portable extension of bridge_data

    call(cg_prim.emit_module_setup);
    call(cg_prim.alloc_fn_labels);

    for (i = fn_start; i < arr_len(cg_fn_name); i = i + 1) {
        cg_emit_func(i);
    }

    call(cg_prim.emit_module_finalize);
}
```

### Wave 17 — emit_all

Top-level orchestrator. Complicated by a64's dual mode (asm and
binary). Cleanest split:

**Portable spine** (new: `cg_emit_all`):
```
cg_emit_all() {
    cg_bridge_data();
    cg_bind_startup_syms();

    call(cg_prim.emit_prelude);  // section directives, global
                                 // decls, etc. Chip-specific.

    // Emit all functions.
    auto i;
    for (i = 0; i < arr_len(cg_fn_name); i = i + 1) {
        cg_emit_func(i);
    }

    call(cg_prim.emit_global_data); // cg_gl_name → .quad 0s,
                                    // float tbl, string tbl
    call(cg_prim.emit_postlude);    // finalize fixups, close sections
}
```

**New primitives**:

```
emit_prelude,       // () → .text section, setup encoder state
emit_global_data,   // () → globals + flt_tbl + str_tbl emission
emit_postlude,      // () → finalize (fixup resolve, etc.)
```

For a64 specifically, `emit_prelude`/`emit_postlude` internally
branch on `a64_bin_mode`. x64 versions are binary-only.

### Phase 4 line delta

```
a64_codegen.bsm:     -180
x64_codegen.bsm:     -75
bpp_codegen.bsm:     +50
a64_primitives.bsm:  +200
x64_primitives.bsm:  +100
```

---

## Per-wave protocol (reminder)

Copy from Phase 3.4 execution protocol. Every wave:

1. Read source, design primitives, verify they're in the struct.
2. Implement primitives in both chips.
3. Add `cg_prim.X = fn_ptr(_chip_X)` lines to init_codegen_*.
4. Write portable spine / case, delete chip-local counterpart.
5. `./bpp src/bpp.bpp -o /tmp/bpp_waveN`
6. Shasum pathfind, rhythm, bang9 vs baseline — MUST match.
7. If diff: git restore modified files, investigate, retry.
8. If match: commit with the template.
9. Next wave.

## Commit template

```
Phase <N> Wave <W>: <one-line description>

Migrates <cases> from per-chip <function> into the portable
cg_emit_<name> spine. New primitives: <list>. cg_prim slots
populated in init_codegen_{arm64,x86_64}.

Byte-identity verified:
  pathfind  <hash>
  rhythm    <hash>
  bang9     <hash>

Line delta:
  a64_codegen.bsm  -<n>
  x64_codegen.bsm  -<n>
  bpp_codegen.bsm  +<n>
  a64_primitives.bsm  +<n>
  x64_primitives.bsm  +<n>
```

---

## Expected end state

After Wave 9 + Phase 3.5 + Phase 4:

```
src/bpp_codegen.bsm            ~1500 lines (all portable)
src/backend/chip/aarch64/
  a64_codegen.bsm              ~400 lines (init + wrapper)
  a64_primitives.bsm           ~900 lines (60 primitive bodies)
  a64_enc.bsm                  864 lines (unchanged — instruction encoders)
src/backend/chip/x86_64/
  x64_codegen.bsm              ~300 lines
  x64_primitives.bsm           ~700 lines
  x64_enc.bsm                  991 lines (unchanged)
```

**Total chip-specific code (non-encoder)** dropped from 5497 to
~2300 lines. 58% reduction.

Adding RISC-V = write `riscv_primitives.bsm` (≈ 900 lines
following a64's shape) + `riscv_enc.bsm` (≈ 500 lines) + populate
`init_codegen_riscv64()`. Portable spine is reused wholesale.

---

## Known risks

1. **Wave 9 caller-saved spill logic** is currently inline in T_CALL.
   Extracting preserving behavior is tricky. If you find the spill
   set differs by call context (live-range dependent), primitive
   may need a mask parameter.

2. **Extern calls with arg coercion** — if parser inserts cast
   nodes, cg_emit_node handles them transparently. If not, the
   spine may need per-arg coercion logic before the push_arg call.

3. **Wave 14 arg_copy** assumes all params flow through ABI regs
   or stack. Struct-by-value args (if B++ ever adds them) would
   need a new primitive.

4. **B3 promoted regs shared** requires the `cg_promoted_regs`
   global to be written by the chip's `b3_select` primitive. If
   the chip can't write to a shared global from a fn_ptr-dispatched
   function, use a pattern where b3_select RETURNS the array and
   spine stores it.

5. **emit_all prelude for asm mode** bundles section directives +
   global label declarations + placeholder literal tables. All
   chip-specific. Don't try to share that part.

---

## Session-close ritual (unchanged)

1. Report wave status (passed / deferred / blocked).
2. Record end-of-session hashes (bpp + 3 user programs).
3. Journal entry in `docs/journal.md`:
   - Waves shipped this session, commit hashes
   - Surprises resolved
   - Updated line counts
   - What remains (if anything)
4. Update `docs/todo.md`.

---

## Out of scope (intentional)

- Phase 5 (RISC-V port) — separate multi-session effort.
- Phase 6 (install.sh chameleon) — separate session.
- ModuLab / Wolf3D / pathfind game work.
- Compiler changes (`bpp.bpp`, parser, types, lexer, emitter).

---

*Written 2026-04-20 to close the backend refactor. After Wave 9
+ Phase 3.5 + Phase 4 ship, B++ is Forth-portable. RISC-V becomes
a leaf task. Auto-generation dream is one design doc away.*
