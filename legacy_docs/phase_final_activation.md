# Final Backend Activation — Wave 9b + spine flip

**Status**: Execution pending. Handed off to the Emacs-side agent.
**Prerequisite**: Commit `914ebf6` (Phase 3.5 Waves 13/14/15
activation shipped).
**Goal**: Close the backend as Forth-portable. After this lands,
`cg_emit_func` lives in `bpp_codegen.bsm` and calls chip primitives
via fn_ptr dispatch; each chip's `emit_func` becomes a one-line
delegator. T_CALL activates through the 14 primitives the prior
scaffolding already wired. RISC-V port then becomes leaf work.

---

## Pre-flight

Verify the `914ebf6` baseline state:

```sh
git log -1 --oneline   # expect 914ebf6 Phase 3.5 activation
```

Canary hashes immutable across every wave of this session:

```
pathfind  50caa64bfa7f4476d0780c5857304db66176d852
rhythm    3d4f424b2ae7071110d8962750aaa700f2c57009
bang9     7a76c3b8f6d9cb7021cb4a221f5c9980accdee02
```

File inventory:

```
src/bpp_codegen.bsm                            885 lines
src/backend/chip/aarch64/a64_codegen.bsm      2624 lines
src/backend/chip/aarch64/a64_primitives.bsm    794 lines
src/backend/chip/x86_64/x64_codegen.bsm       1961 lines
src/backend/chip/x86_64/x64_primitives.bsm     414 lines
```

---

## What's shipped vs what remains

### Shipped (914ebf6)

Three waves of `emit_func` migration activated. Each chip's
`emit_func` already calls real primitives for:

- **Wave 13 prologue** — `_<chip>_emit_prologue(name_p, fn_lbl_id, frame_size)` (fat primitive: label + ret_lbl + frame alloc + B3 spill)
- **Wave 14 arg copy** — `_<chip>_emit_arg_copy_int(param_idx, frame_off)`
- **Wave 15 epilogue** — `_<chip>_emit_frame_teardown(frame_size)`

All three primitives have REAL bodies. Chip inline delegates to
them. Dual-mode (bin + asm) supported in a64. Byte-identity
verified on pathfind, rhythm, bang9.

### Remaining (this session's work)

1. **Wave 9b — T_CALL activation.** The 770-line beast in
   `a64_emit_node` / `x64_emit_node`. Currently falls through to
   chip inline because cg_emit_node doesn't handle T_CALL. The
   14 Wave 9 primitive slots are wired but bodies are partial
   (mostly real, 3 stubs).

2. **Spine flip — cg_emit_func.** Create the portable function
   emitter in `bpp_codegen.bsm` that orchestrates prologue +
   arg copy + body + epilogue via `call(cg_prim.X, ...)` instead
   of chip-inline calls. Then chip's `emit_func` becomes
   `return cg_emit_func(fi);`.

3. **Waves 16/17 — optional cleanup.** emit_module and emit_all.
   Low marginal portability value but shipping them closes the
   "all cases in spine" story. Stubs for these are wired but
   empty.

---

## Execution order

The plan executes three phases, each its own commit with
byte-identity gate:

```
Commit A: Wave 9b — T_CALL activation (chip-level helper refactor)
Commit B: Spine flip — cg_emit_func in bpp_codegen.bsm
Commit C: Waves 16/17 final polish (or skip if time tight)
```

Budget estimate: 4-5h. Wave 9b is the hard one (3h). Spine
flip is 1h. Waves 16/17 are 30-60min if attempted.

---

## COMMIT A — Wave 9b T_CALL activation

### Strategy: chip-local extraction first, then primitives

The safest path avoids surgery on the 770-line inline:

**Step A.1 — Extract T_CALL body to chip-local helper (byte-identical refactor)**

Move the ENTIRE `if (t == T_CALL) { ... }` body from
`a64_emit_node` into a new chip-local `a64_emit_call(n)`:

```
// In a64_codegen.bsm, replace inline T_CALL with:
if (t == T_CALL) { return a64_emit_call(n); }

// Add static a64_emit_call(n) somewhere in the file
// containing the full original body verbatim.
```

This is pure refactor — byte-identity MUST hold. If it doesn't,
you moved more than intended (e.g., captured a variable that
was previously local to emit_node). Roll back and investigate.

Same for `x64_emit_call`.

**Commit A intermediate**: "T_CALL extracted to chip-local helper
(pure refactor)".

### Step A.2 — Write missing Wave 9 primitive bodies

The scaffolding left these as stubs:

- `_a64_emit_call_extern(ex_idx)` — body deferred
- `_a64_emit_save_caller_saved_int()` — stub
- `_a64_emit_restore_caller_saved_int()` — stub
- Same three for x64

**Reverse-engineering caller-saved**: the inline T_CALL doesn't
have an explicit "save caller-saved" block. The register-file
model is: B3-promoted regs (x19-x24 on a64, r12-r15 on x64) are
callee-saved, saved in prologue, restored in epilogue. Everything
else is caller-saved but typically not live across the call
because:

- Result reg (x0 / rax): re-evaluated after the call
- Scratch regs: freelist-managed, no values live across the call

**Conclusion**: `_chip_emit_save_caller_saved_int` is probably a
no-op for current B++. Verify by implementing as `{ }` and
testing — if byte-identity holds with the primitive called from
cg_emit_call, the inline spill set was always empty.

If byte-identity breaks: the inline T_CALL DOES spill something.
Use `cmp -l` on the first differing offset in pathfind's output
to find the extra bytes. Narrow to which instruction they encode.
That reveals what was spilled.

**`_a64_emit_call_extern(ex_idx)` body** (copy from inline T_CALL's extern path):

```
void _a64_emit_call_extern(ex_idx) {
    auto name_p;
    name_p = arr_get(cg_ex_name, ex_idx);
    if (a64_bin_mode) {
        enc_add_reloc(enc_pos, name_p, 3);  // reloc type 3 = call
        enc_bl_placeholder();
    } else {
        out("  bl _"); a64_print_p(name_p); putchar('\n');
    }
}
```

Check inline T_CALL for the exact pattern and reloc type.

### Step A.3 — Write cg_emit_call in bpp_codegen.bsm

After primitives are real, write the portable spine:

```
cg_emit_call(n) {
    auto fi, exi, arr, cnt, i, is_extern, is_float_ret;
    auto fn_name: Node;
    auto int_slot, stack_slot, pad_bytes;

    fn_name = n.a;
    arr = n.b;
    cnt = n.c;

    fi = cg_find_fn(fn_name.a);
    is_extern = (fi < 0);

    // Builtin dispatch — peek/poke/sizeof/etc still chip-local.
    // If the name is a builtin, delegate to chip's emit_call_builtin.
    // (Add a primitive cg_prim.emit_call_builtin OR keep builtins
    //  as a chip-local check before calling cg_emit_call.)

    if (is_extern) {
        exi = cg_find_ext_by_argc(fn_name.a, cnt);
        if (exi < 0) {
            // Unknown — chip handles (builtin or error).
            return 0;
        }
    }

    // Count args that go on stack for pre-call alignment.
    auto stack_args;
    stack_args = 0;
    for (i = call(cg_prim.arg_reg_count_int); i < cnt; i = i + 1) {
        stack_args = stack_args + 1;
    }

    pad_bytes = call(cg_prim.emit_pre_call_align, stack_args);
    call(cg_prim.emit_save_caller_saved_int);

    // Evaluate each arg and route to ABI slot.
    int_slot = 0;
    stack_slot = 0;
    for (i = 0; i < cnt; i = i + 1) {
        cg_emit_node(arr[i]);  // result in x0/rax
        auto abi_slot;
        abi_slot = call(cg_prim.arg_reg_int, int_slot);
        if (abi_slot >= 0) {
            call(cg_prim.emit_push_arg_int, abi_slot, 0);
            int_slot = int_slot + 1;
        } else {
            call(cg_prim.emit_push_arg_int, stack_slot * 8, 1);
            stack_slot = stack_slot + 1;
        }
    }

    // Emit call.
    if (is_extern) {
        call(cg_prim.emit_call_extern, exi);
        is_float_ret = cg_ext_ret_is_float(exi);
    } else {
        call(cg_prim.emit_call_direct, arr_get(cg_fn_lbl, fi));
        // Internal function return type — fetch from parser's type table.
        is_float_ret = is_float_type(get_fn_ret_type(arr_get(cg_fn_fidx, fi)));
    }

    call(cg_prim.emit_post_call_unalign, pad_bytes);
    call(cg_prim.emit_restore_caller_saved_int);

    return is_float_ret;
}
```

### Step A.4 — Flip chip dispatch

In `a64_emit_node`, after confirming all T_CALL logic is covered
in cg_emit_call, replace:

```
if (t == T_CALL) { return a64_emit_call(n); }
```

with:

```
if (t == T_CALL) {
    // Handle builtin dispatch inline (peek/poke/sizeof/...)
    // that cg_emit_call doesn't cover.
    if (cg_str_eq(n.a, "peek", 4)) { ... }
    if (cg_str_eq(n.a, "poke", 4)) { ... }
    // ... other builtins ...

    // Non-builtin calls via portable spine.
    return cg_emit_call(n);
}
```

Builtins (~50 lines) stay chip-local. Regular calls go portable.
Same pattern for x64.

**Validate**: bootstrap + pathfind/rhythm/bang9 hashes match.

**If byte-identity breaks**: the most likely cause is arg evaluation
order or caller-saved scope. Isolate via a tiny test program with
a single 2-arg call and binary-diff with the baseline.

---

## COMMIT B — Spine flip: cg_emit_func

After T_CALL is activated, add `cg_emit_func` to `bpp_codegen.bsm`.

```
cg_emit_func(fi) {
    auto name_p, par_arr, par_cnt, body_arr, body_cnt, i, frame;
    auto total, sz, fty, abs_off;

    name_p = arr_get(cg_fn_name, fi);
    par_arr = arr_get(cg_fn_par, fi);
    par_cnt = arr_get(cg_fn_pcnt, fi);
    body_arr = arr_get(cg_fn_body, fi);
    body_cnt = arr_get(cg_fn_bcnt, fi);

    // Reset per-function state.
    arr_clear(cg_vars);
    arr_clear(cg_var_stack);
    arr_clear(cg_var_struct_idx);
    arr_clear(cg_var_off);
    arr_clear(cg_var_forced_ty);
    arr_clear(cg_var_refs);
    arr_clear(cg_var_addr_taken);
    arr_clear(cg_var_promote);

    cg_cur_fn_name = name_p;
    cg_cur_fn_idx = arr_get(cg_fn_fidx, fi);

    // Register params.
    cg_register_params(fi, par_arr, par_cnt);

    // Walk body to pre-register variables and collect stack structs.
    cg_pre_reg_vars(body_arr, body_cnt);

    // Compute frame layout. Chip-specific because alignment rules
    // and SIMD slot sizing differ per ABI. Primitive approach:
    //   frame_size = call(cg_prim.compute_frame_layout)
    // OR keep inline if the math is truly universal.
    // (Review both chips' existing math — a64 has : double
    //  SIMD 16-byte alignment, x64 has 24-byte scratch header.
    //  If the math diverges, make it a primitive.)
    frame = chip_compute_frame();   // pseudo — actual call pattern TBD

    // B3 reference walk + select.
    auto stmt_i: Node;
    for (i = 0; i < body_cnt; i = i + 1) {
        stmt_i = body_arr[i];
        if (stmt_i != 0) { cg_b3_walk(stmt_i); }
    }
    call(cg_prim.b3_select);   // chip picks its own reg range

    // Prologue.
    call(cg_prim.emit_prologue, name_p, arr_get(cg_fn_lbl, fi), frame);

    // Main-specific argc/argv/envp save.
    if (cg_str_eq(name_p, "main", 4)) {
        call(cg_prim.emit_save_startup_args);  // new primitive
    }

    // Arg copy.
    for (i = 0; i < par_cnt; i = i + 1) {
        call(cg_prim.emit_arg_copy_int, i, arr_get(cg_var_off, i));
    }

    // B3 zero non-parameter promoted locals, sub-width float narrow.
    // (These are small loops — could stay here or become primitives.)

    // Emit body.
    cg_depth = 0;
    cg_emit_body(body_arr, body_cnt);

    // Implicit return 0.
    call(cg_prim.emit_mov_zero);

    // Epilogue.
    call(cg_prim.emit_frame_teardown, frame);
}
```

Note the pseudo-code uses `chip_compute_frame()` — review whether
a64 and x64 frame math is close enough to unify. If yes, move
to spine. If no, make it a primitive.

**Flip chip dispatch**: replace both `a64_emit_func` and
`x64_emit_func` bodies with:

```
static void a64_emit_func(fi) { cg_emit_func(fi); }
static void x64_emit_func(fi) { cg_emit_func(fi); }
```

Or delete them entirely and rename cg_emit_func's callers
directly.

**Validate**: bootstrap + byte-identity on all 3 canaries.

---

## COMMIT C — Waves 16/17 (optional)

If Commits A+B shipped green and there's budget:

### Wave 16 — emit_module

Create `cg_emit_module(mi)` in spine. Delegates encoder reset
and label allocation to primitives.

### Wave 17 — emit_all

Create `cg_emit_all()` in spine. Delegates section directives
and global data emission to primitives.

Low marginal value (most of this is chip-specific encoder
plumbing), so skip if tired.

---

## Per-commit protocol (standard discipline)

1. Make changes.
2. Bootstrap: `./bpp src/bpp.bpp -o /tmp/bpp_<label>`
3. Byte-identity gate:
   ```sh
   /tmp/bpp_<label> games/pathfind/pathfind.bpp -o /tmp/pf_<label>
   /tmp/bpp_<label> games/rhythm/rhythm.bpp     -o /tmp/rh_<label>
   /tmp/bpp_<label> bang9/bang9.bpp             -o /tmp/b9_<label>
   shasum /tmp/{pf,rh,b9}_<label>
   ```
4. ALL THREE must match canary hashes. If any diff → git restore,
   investigate, retry.
5. Commit using the template below.

---

## Commit template

```
Phase 3.5 final activation: <one-line>

<Description of what this commit activates/refactors.>

Byte-identity verified:
  pathfind  50caa64bfa7f4476d0780c5857304db66176d852
  rhythm    3d4f424b2ae7071110d8962750aaa700f2c57009
  bang9     7a76c3b8f6d9cb7021cb4a221f5c9980accdee02

Line delta:
  a64_codegen.bsm     -<n>
  x64_codegen.bsm     -<n>
  bpp_codegen.bsm     +<n>
  a64_primitives.bsm  +<n>
  x64_primitives.bsm  +<n>

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

---

## Anticipated surprises

1. **Builtin dispatch leakage** — the inline T_CALL has ~10
   builtins (peek, poke, sizeof, float_ret, sys_*, etc). Those
   must stay chip-local (they often need specific reg ordering
   that doesn't fit the generic ABI dance). Budget 30 min to
   catalog and preserve them in chip's T_CALL dispatch before
   cg_emit_call runs.

2. **Frame math divergence** — a64 and x64 compute `frame`
   differently (a64 uses 40-byte scratch header, x64 uses 24,
   a64 has 16-byte SIMD alignment, x64 uses 8). Either:
   - Unify via a portable `cg_compute_frame()` with chip
     constants
   - Leave as `chip_compute_frame` primitive
   Inspect both before deciding.

3. **Caller-saved spill empty** — prediction: Wave 9's
   save/restore_caller_saved primitives can stay as no-ops.
   Verify empirically (bytes match) before celebrating.

4. **`emit_save_startup_args`** — main()'s argc/argv/envp save
   is ~10 lines of chip-specific reloc. Add as a new primitive
   slot if you don't want to inline chip-side.

5. **B3 select shared state** — `cg_promoted_regs` and
   `cg_b3_spill_base` are chip-written globals. The
   `cg_prim.b3_select` primitive writes into them. Verify chip
   primitives update these correctly before spine reads them.

---

## Verification end-of-session

```sh
# Must be reachable from 914ebf6:
git log --oneline 914ebf6..HEAD

# Expected: 2-3 commits (Wave 9b, spine flip, optional 16/17).

# Final byte-identity:
./bpp src/bpp.bpp -o /tmp/bpp_final
/tmp/bpp_final games/pathfind/pathfind.bpp -o /tmp/pf_final
/tmp/bpp_final games/rhythm/rhythm.bpp     -o /tmp/rh_final
/tmp/bpp_final bang9/bang9.bpp             -o /tmp/b9_final
shasum /tmp/{pf,rh,b9}_baseline /tmp/{pf,rh,b9}_final
# All 3 pairs must match.

# Final line counts (projected):
wc -l src/bpp_codegen.bsm \
      src/backend/chip/aarch64/{a64_codegen,a64_primitives}.bsm \
      src/backend/chip/x86_64/{x64_codegen,x64_primitives}.bsm

# Expected roughly:
#   bpp_codegen.bsm         ~1200 lines (+cg_emit_call +cg_emit_func)
#   a64_codegen.bsm          ~1200 lines (big drop — T_CALL + emit_func extracted)
#   a64_primitives.bsm        ~900 lines
#   x64_codegen.bsm          ~1100 lines
#   x64_primitives.bsm        ~500 lines
```

If all green: backend is Forth-portable. RISC-V port is the
next mountain but it's a LEAF task — writing one file of
primitives against a stable contract.

---

## Out of scope

- Phase 5 (RISC-V port) — separate multi-session effort.
- Phase 6 (install.sh chameleon) — separate session.
- Bang 9 / pathfind / modulab feature work.
- Compiler core changes (bpp.bpp, parser, types, lexer).
- Adding new AST node types.
- Re-architecting the ChipPrimitives struct beyond this scope.

---

## Session-close ritual

1. Report which commits shipped (hashes + descriptions).
2. Record end-of-session canary hashes.
3. Journal entry in `docs/journal.md`:
   - Commits shipped
   - Surprises encountered + how resolved
   - Final line counts
   - Status: "Backend Forth-portable" if all green, or
     "Backend closeout stage N" if partial.
4. Update `docs/todo.md` Phase 3.4 + 3.5 markers.

If backend fully closes: a celebratory journal entry is
warranted. This is the milestone that unlocks everything
downstream — RISC-V port, WebAssembly backend, install-time
chameleon, the full alien-parasite vision.

---

*Written 2026-04-20 evening, handing off from claude-code to
Emacs-side agent. Budget honest — 4-5h of focused work.
Byte-identity is the gate. No shortcuts.*
