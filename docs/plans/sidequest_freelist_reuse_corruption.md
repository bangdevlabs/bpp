# Free-list `_MEM_REUSE=1` — Node.itype Byte Corruption Sidequest

**Status**: Open. Diagnosed but not fixed. Carve-only (`_MEM_REUSE=0`) ships clean with
+37% bootstrap speedup; this sidequest is the followup to enable full recycling for
the per-frame allocator win on rts1.

## Symptom

With `_MEM_REUSE=1` in `src/bpp_mem.bsm`:

- Bootstrap byte-stable (gen1 == gen2) — the compiler self-compiles.
- Suite breaks **180→146/34** with E233 cascade ("argument N is float, parameter is int").
- Failures cluster in tests that exercise high-arg-count call sites — `_stb_gpu_sprite_uv_tint` (13 args), `_stb_gpu_vertex` (9 args).

## Precise Diagnostic (2026-05-27)

Instrumented `_val_is_function_local` and the E233 check site in `bpp_validate.bsm`
to log every T_VAR-arg type-check during validation. Found:

```
[arg] fi=203 ai=3  callee_fi=187 name=aa arg_ty=2   par_ty=2   ← OK (TY_WORD)
[arg] fi=361 ai=0  callee_fi=360 name=aa arg_ty=2   par_ty=2   ← OK
[arg] fi=662 ai=12 callee_fi=451 name=aa arg_ty=192 par_ty=2   ← CORRUPTED
[arg] fi=663 ai=12 callee_fi=451 name=aa arg_ty=169 par_ty=2   ← CORRUPTED
```

- fi=662 is `image_draw_size_tinted`, fi=663 is `image_draw_tinted` (both in
  `stb/stbimage.bsm`), both call `_stb_gpu_sprite_uv_tint` with `aa` as arg 13.
- Same local name `aa` types correctly (TY_WORD = 0x02) in 2 other functions, but
  comes back as **byte-level garbage** (0xC0, 0xA9) in these two callers.
- The garbage value DIFFERS per call site (192 vs 169) → confirms the corruption is
  positional, not a single shared clobber.

## Root Cause Class

**Heap corruption** of the `itype: byte` field on specific T_VAR AST Nodes. Not a
logic bug in inference — `aa = (tint >> 24) & 0xFF` is pure integer arithmetic that
the type checker correctly infers as TY_WORD across all four call sites. The
corruption fires AFTER inference completes (validate sees the garbage), so somewhere
between `run_types` and `run_validate` a writer mis-targets the itype byte slot.

Phases between inference and validate (per `src/bpp.bpp` lines 419-435):
1. `run_types()` — sets correct itype on every Node
2. `find_dispatch_candidates()`
3. `synthesize_loop_fn()`
4. `rewrite_dispatch_loops()`
5. `annotate_temps()`
6. `run_dispatch()` — writes `Node.dispatch: byte`
7. `run_validate()` — reads garbage itype

## Why Reuse-Only

Carve-only `_MEM_REUSE=0` makes every malloc a fresh unique address — no buffer is
ever handed out twice. So a stale writer pointing at a stale buffer writes into
unreached memory (silently lost). With reuse enabled, the same buffer comes back
into circulation, the stale write lands on something live, and the corruption is
observable.

The bug is therefore **a latent use-after-free in the compiler**, masked for the
lifetime of the project by the simple-allocator era. Free-list reuse is a
bisection tool that surfaces the bug — the fix has to address the use-after-free,
not the allocator.

## Excluded Hypotheses (Already Ruled Out)

- `bpp_types.bsm` arr_struct usage — audited 21 `arr_struct_at(ty_vt, ...)` + 12
  `arr_struct_at(fn_metas, ...)` call sites. Every alloc-then-at pattern refetches
  `e` after the alloc. No cached pointer crosses a grow.
- `_val_is_function_local` is fine — instrumentation showed `aa` IS found in the
  local table, recovery code (validate.bsm:983) doesn't fire.
- The args array (`n.b`) is arena-allocated (see `list_end` in `bpp_internal.bsm`),
  arena memory is never freed during compile.
- Node memory itself is arena-allocated; the Node address is stable.
- `propagate_set_par_type` is gated off (`changed = 0` hard-disable in `run_types`).

## Suspect List (Rank-Ordered)

1. **`run_dispatch` byte-store width bug** — writes `Node.dispatch: byte` but a
   codegen mistake (STR instead of STRB, or wider mem write) spills into the
   adjacent `itype: byte` byte. Layout per `bpp_defs.bsm:35`:
   `{ ntype: byte, a, b, c, d, e, dispatch: byte, itype: byte, src_tok }`. Adjacency
   is suspicious. Targeted check: grep `.dispatch =` writes in `bpp_dispatch.bsm`
   and trace one through codegen — verify it emits STRB not STR.
2. **`rewrite_dispatch_loops` / `synthesize_loop_fn` AST surgery** — these rewrite
   nodes in place; if a clone-and-patch path drops the itype field or copies stale
   bytes, the corrupted byte appears here.
3. **Hash resize collateral** — `_ty_func_hash` resize frees 2048 + 256 byte
   buffers; if any cross-module accessor (`bpp_codegen.bsm`,
   `bpp_dispatch.bsm`) reads through one of those AFTER its lifetime ends, the
   write-back path can corrupt unrelated memory.

## Next Step (For Resume Session)

Single targeted experiment to discriminate suspect #1 from #2-3:

```bash
# In src/bpp.bpp around line 433, COMMENT OUT run_dispatch():
#       run_dispatch();
# Rebuild, bootstrap, set _MEM_REUSE=1, re-run suite.
# - If suite goes back to 180/0/12 → run_dispatch is the culprit
#   (drill into byte-store codegen for Node.dispatch).
# - If suite still 146/34 → one of the earlier phases is at fault.
```

If #1 confirmed, the fix is in the chip backend's emit for byte-field store —
verify `cg_emit_byte_store` (or whatever the spine names it) lowers to
`strb w, [base, #off]` not `str x, [base, #off]`.

## Bisect Update (2026-05-27, session 2) — dispatch pipeline RULED OUT

Ran the prescribed experiment AND extended it. With `_MEM_REUSE=1`:

- Comment `run_dispatch()` (modular, `bpp.bpp:331`) → still **146/34**. So
  run_dispatch (suspect #1) is NOT the culprit.
- Comment the WHOLE dispatch pipeline — `find_dispatch_candidates`,
  `synthesize_loop_fn`, `rewrite_dispatch_loops`, `annotate_temps`,
  `run_dispatch` (modular `bpp.bpp:326-331`) → **still 146/34, identical
  failures.**

**Conclusion: the corruption does NOT originate in the dispatch pipeline.**
The phases at `bpp.bpp:320-324` (call_graph_build / classify_* /
mark_reachable / promote_globals) run AFTER `infer_module(0)` and do not
write `Node.itype`. So the garbage `itype` **originates in `infer_module(0)`
(inference itself) or in PARSE** — upstream of where this doc originally
pointed. The "run_types sets correct itype, corruption is downstream"
assumption was wrong.

### Sharper hypothesis (rules out the simple stale-read)

The current free-list **zeroes every block on pop** (reuse). So a stale read
of a recycled block would yield `itype = 0`, NOT the observed `0xC0` / `0xA9`.
The garbage is **non-zero and positional** (differs per call site). Therefore
it is **a WRITE of a specific (reuse-dependent) value into the `itype` byte**
during inference/parse — most likely a **byte-store-width spill**: a wider
store (`STR`, 8 bytes) of an adjacent field whose value is reuse-dependent,
landing on `itype`. The field immediately BEFORE `itype` is `dispatch`; a
wide store of `dispatch` would put its 2nd byte into `itype`. But the
dispatch writers (run_dispatch, annotate_temps) are now ruled out — so look
for a wide store of a field near `itype` in the PARSER's node init or in
`add_type`'s node writes, OR a node whose memory is NOT arena (freelist-
allocated + reused) so its bytes are recycled content.

### Next experiment (discriminator)

Instrument the E233 arg-check in `bpp_validate.bsm` (where `arg_ty` reads
192/169) to ALSO dump the node's RAW bytes: `ntype`, `dispatch`, `itype`,
and the full word at the node + `src_tok`. 
- If ONLY `itype` is garbage (ntype = T_VAR correct) → a targeted itype
  write/spill in infer/parse.
- If MULTIPLE adjacent bytes are garbage (ntype/dispatch/src_tok too) → the
  whole node's memory is recycled content → some nodes are freelist-allocated
  (not arena) and get reused. Then find who frees a node.

Alternative tool (per the cousins study, Zig GPA): a debug-allocator mode in
`bpp_mem` that poisons freed blocks with a DISTINCTIVE byte (e.g. 0xAB) and
does NOT zero on pop — if the corrupted `itype` then reads `0xAB`, it confirms
the node sits in recycled freelist memory (contradicting "node is arena").

## Reproducer

Minimal: `tests/test_thread.bpp` fails with E233 on `_stb_gpu_sprite_uv_tint`
arg 13. Smaller repros likely available — any program that calls a 9+ arg
function with a locally-derived integer in the last slot.

## Files Touched (Reverted in Closing Commit)

- `src/bpp_validate.bsm` — instrumentation in `_val_is_function_local` + the T_CALL
  arg-check loop. REVERTED in the closing commit; this doc preserves the data.
- `src/bpp_mem.bsm` — `_MEM_REUSE = 0` (kept). Toggle to 1 to reproduce.

## Disposition

Ship carve-only NOW (37% bootstrap win, rts1 frame-jitter gone for blocks ≤4KB).
Open this sidequest as a P1 followup — fixing it unlocks the rts1 per-frame churn
case (long-running game allocates and frees small blocks every tick, full reuse
prevents the per-frame mmap storm).
