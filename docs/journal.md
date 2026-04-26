# B++ Bootstrap Journal

## 2026-04-24 — 🧪 TABLAH: EXTERNAL BENCHMARK + HASH ITERATION API

First external stress-test of B++. A software engineer ported **tablah** — a
Swift hashmap benchmark — to B++ to validate the language as a general-purpose
systems tool. The benchmark exercises four phases in sequence: parallel
generation of 1M U64 keys (Xorshift64, 8 workers via `job_parallel_for`),
parallel generation of 1M U32 values, hashmap creation (1M inserts at 25% load
factor), and a filter pass (first 6-bit char index == 'E' or 'J' and value ≥
1 billion).

Two variants shipped. **`examples/tablah.bpp`** uses the clean public API
throughout. **`examples/tablah_opt.bpp`** is hand-optimized: `xorshift64`
inlined at every call site and the 9-iteration inner loop fully unrolled —
submitted as a companion to show the gap between clean-code and zero-overhead
B++.

Two stdlib additions were needed to support the port:

**`print_str` added to `bpp_io.bsm`** — `sys_write`-backed string printer with
no trailing newline. Required by `tablah`'s `print_timing` helper, which
composes label + integer + unit on one line. Now auto-injected alongside
`print_msg`.

**Hash iteration API added to `bpp_hash.bsm`** — four new functions
(`hash_cap`, `hash_slot_live`, `hash_key_at`, `hash_val_at`) let callers
iterate all live entries without touching `Hash` struct internals. `tablah.bpp`
filter phase uses these; `tablah_opt.bpp` deliberately bypasses them (direct
struct access) to shave per-slot indirection and demonstrate the trade-off.

Minor: `games/pathfind/pathfind.bpp` updated to load
`cat_sprite.modulab.json` (ModuLab-exported asset path).

---

## 2026-04-23 — 📚 DOC BACKFILL: every chapter shipped

Short doc-maintenance session. Three files updated:

**`docs/todo.md`** — status date updated to 2026-04-23, version to
0.111, suite to 111/0 (non-GPU). The 1.0 path gained four new ✅
milestones (GPU palette + ModuLab 1.0, Waves 18-21 portable backend,
Phase D). The stale wave-by-wave Active section replaced with a
clean post-D summary + next targets (Phase 5 RISC-V, Phase 6
install chameleon). `stbaudio` row in the game modules table
updated from "next" to ✅.

**`README.md`** — insignia bumped to B++ 0.111. Module count 21 →
22 (stbpal). Test count 78 → 111. Timeline table gained four
entries: Apr 20 GPU palette, Apr 20-22 Waves 18-21, Apr 22
Phase D, Apr 23 this session. Closing paragraph updated.

**`docs/journal.md`** — this entry.

---

## 2026-04-22 — 🧹 PHASE D: STB FAXINA + CAPS 35-47 + PARSER OPTS

**Stb library fully dogfooded**. Every hand-rolled pattern that
duplicated an auto-injected `bpp_*` tool migrated to the canonical
API. Thirteen modules touched: stbgame (scene manager), stbmixer
(11 voice arrays), stbasset (slot init), stbforge (character frames
+ testbed world), stbsound (byte readers/writers), stbimage (BE
stream readers), stbpal (palette alloc + clone + LUT flash),
stbdraw (font atlas + layers + rasterizer), stbrender (text
measure), stbecs (ecs_new), stbpath (path_new), stbtile (tile_new),
stbui (arena migrated to bpp_arena).

The grep-audit missed-item sweep at session end caught five
residuals that the first pass skipped: stbdraw lines 525 + 672
(font atlas + baked bitmap zero loops), stbdraw `_ttf_u16` /
`_ttf_u32` (BE readers now delegate to bpp_buf's `read_u*be`),
stbforge line 1189 (sprite argb clear), stbinput line 87
(keys_prev snapshot copy).

**Caps 35-47 shipped** — thirteen book chapters, one per stb
module in `docs/how_to_dev_b++.md`. Pattern matches the earlier
Caps 29-34: intent / scope / API / decision points / caveats /
verification / "what this chapter does NOT cover". Book grew
from ~2800 lines to 3276 lines.

**`arr_at` added to bpp_array** — bounds-checked + stderr-logged
variant for boundary-layer callers (PNG chunk readers, WAV header
parsers, asset ID lookups from untrusted input). `arr_get` stays
unchecked (status-quo for every current caller inside `for (i=0;
i<arr_len; i++)` loops — bounds check would be dead code in hot
paths). The split captures the design intent: `arr_get` for
already-validated indices, `arr_at` when the index came from
outside.

**Phase D compiler optimizations** — three passes landed in
`bpp_parser.bsm`, all parser-level (no spine / chip changes):

- **D.1 strength reduction** — `x * 2^k` → `x << k`, `x % 2^k` →
  `x & (2^k - 1)`. Pattern-matched on the post-fold T_BINOP branch.
  Signed division deliberately NOT reduced (signed ASR vs integer
  divide differ on negatives; B++ ints are signed).
- **D.2 identity peephole** — `x + 0`, `x * 1`, `x | 0`, `x ^ 0`,
  `x & -1` → `x`; `x * 0`, `x & 0` → `0`. Both left-literal and
  right-literal sides where commutative. Emitted when exactly one
  operand is a T_LIT (both-lit is handled by the existing constant
  folder above).
- **D.4 inline trivials (narrow whitelist)** — `buf_new(n)` →
  `malloc(n)`, `buf_new_w(n)` → `malloc(n << 3)`. Pure T_CALL
  rename (and BINOP insertion for buf_new_w). Eliminates the
  bl/ret pair in 32+ hot-path sites across stbmixer (16),
  stbecs (8), stbpath (8). Explicit whitelist was the user's
  gating recommendation — expand case-by-case when profile
  data justifies each addition, don't auto-detect everything.

Skipped with honest reasons (per user calibration):
- **D.3 builtin const-fold** — cosmetic; `len("foo") → 3` matters
  for generated code, not real game code.
- **E.1 tail-call** — benefits parser recursion, zero game impact.
- **E.2 hot loop unroll (N≤8)** — game loops have N=64..1920
  (audio fill, pixel blit). Won't fire.
- **E.3 inline non-trivial wrappers** — needs alpha-renaming
  infrastructure for functions with locals; genuine 1-2 week
  project.

**Tier F design doc** at `docs/tier_f_roadmap.md` — scopes CSE
(2-3 weeks), register allocator v2 (3-4 weeks), auto-vectorization
(2-3 months). Gated behind profile data from a real plugin.

**bpp** = `72e1b793e28b0a6af8b15bc492a946ce1f8e62cf`. **Suite** = 111
passed / 3 GPU-sandbox-flakes / 11 skipped, gen2 == gen3 across
the whole chain. Zero non-GPU regressions.

Commits planned for the next consolidation push: Phase D work +
Caps 35-47 + new chapters (filled today on the 23rd) + parser
optimizations + tier F roadmap + stb dogfood fixes all land
together once the doc backfill completes.

---



**Every compilation entry point is now a spine function.** `cg_emit_func`,
`cg_emit_stmt`, `cg_emit_node`, and `cg_builtin_dispatch` all live in
`bpp_codegen.bsm` and own their dispatchers end to end; the chip walkers
(a64_emit_func / a64_emit_stmt / a64_emit_node and x64 siblings) are
dead code awaiting a cosmetic deletion pass.

**Wave 18** — syscall lift (5 commits). CG_SYS_* portable enum in the
spine. Each chip maps it to BSYS_* through its own `sys_num` helper,
so the spine never imports both OS header sets (the Plan A collision
that killed the original attempt). 18 of 23 sys_* built-ins + fn_ptr
lifted into `cg_builtin_dispatch`. The 5 holdouts have non-standard
ABI shapes (sys_fork's macOS x1-child-zero, sys_execve's arg swap,
sys_waitpid's hardcoded zeros, sys_lseek / sys_getdents's reversed
x64 eval order that would break byte-identity). sys_usleep / sys_ioctl
/ sys_nanosleep / sys_clock_gettime are Linux-only or macOS-only in
the current tree. vec_* (9 SIMD builtins) explicitly out of scope per
the Wave 22 SIMD plan — NEON vs SSE2 shapes don't factor cleanly
through uniform primitives.

**Wave 19** — emit_func orchestrated by spine (6 commits). Twelve new
ChipPrimitives slots carry the function-frame recipe. Frame math
(positive offsets up from fp on aarch64, negative from rbp on x86_64,
SIMD 16-byte alignment rounding) is entirely inside the chip's
`fn_compute_offsets`. Spine only sees `total` as an integer. The
spine-takeover commit (batch 6) preserved byte-identity on first try
— the primitive decomposition was deep enough that gen1 == gen2 held
even though aarch64 and x86_64 now share the same orchestrator.

**Wave 20** — emit_stmt orchestrated by spine (3 commits). First pass
was a fat-delegator fake through `emit_stmt_full` that the user
caught. Redone honestly in four stages: spine first absorbed the
eight simple cases (T_DECL / T_NOP / T_BLOCK / T_BREAK / T_CONTINUE
/ T_IF / T_WHILE / T_RET) that route through existing primitives.
T_ASSIGN lifted with a new `emit_store_var_typed` primitive plus
the portable `cg_has_call` walker (replacing byte-identical
`_a64_has_call` / `_x64_has_call` chip copies). T_MEMST split into
three sibling primitives (memst_float_simd / memst_float_scalar /
memst_int) that own their eval-push-eval-pop-store dance. T_SWITCH
lifted with switch_jtbl (wrapping each chip's jump-table emitter),
pop_discard (drop cond word between arms), and branch_eq / branch_ne
(filled the slots reserved since Wave 7). Spine dispatcher for each
arm body recurses via `cg_emit_stmt`, so all statement emission now
re-enters the spine instead of looping back through chip code.

**Wave 21** — emit_node orchestrated by spine (2 commits, endgame).
Stage 1 lifted the seven cases that flow through portable helpers
(T_LIT → cg_emit_lit, T_VAR non-stack → cg_emit_var, T_TERNARY /
T_UNARY / T_MEMLD / T_ADDR via existing ChipPrimitives slots,
T_CALL → cg_emit_call). Stage 2 tackled T_BINOP — the tangled case
with the left-save freelist dance on aarch64 vs always-stack on
x86_64. Two fat primitives encapsulate the decision: `save_left`
returns a portable save_token (-1 float stack, -2 GP stack, >=0
freelist reg), `resolve` post-eval handles coercion + retrieval and
returns the left_reg_int the spine hands to the arithmetic /
comparison primitives. T_VAR stack-struct activated through the
scaffolded `emit_addr_stack_struct` primitive.

**Closure** — every AST case the compiler emits is now dispatched by a
spine function with ChipPrimitives calls below it. New chip ports
implement the primitive surface (~90 slots now) and inherit every
orchestration detail for free. Byte-identity preserved at every stage
except Wave 18 batch 4 (expected: the compiler itself uses sys_wait4
/ sys_ptrace internally, so gen1 vs gen2 diverge once on the first
lift to the new path — gen2 == gen3 holds, confirming determinism of
the new compiler).

**bpp** = `6b4ea072e94cc0c79579cb57d936ada17dc921ed`. **Suite** = 114
passed / 0 failed / 11 skipped on clean display; GPU tests are
display-contention flakes (same behaviour before and after this
session, reproducible against earlier commits).

**Docker Linux validation blocked**: a parallel agent has
`stb/stbdraw.bsm` mid-refactor calling new GPU functions
(`_stb_gpu_sprite_uv`, `_stb_gpu_create_texture`, `_stb_gpu_sprite`,
`_input_save_prev`) that don't exist in `_stb_platform_linux` yet.
`bpp --linux64` fails on any target program because the platform
file auto-injects stbdraw. Not this session's scope; tracked for the
parallel work's completion. Global snapshot commit `9e1f556` folded
the in-flight stb changes + this session's Wave plans into a single
point so the worktree doesn't diverge while the two agents work on
different layers.

Commits this session, oldest first:
`e033564 bd8696e bcd5bb9 d0db5f5 8ab31a0`  — Wave 18
`c18e1f7 7915efa 0a1c06e 6e9d18e f436505 3bd7cd4` — Wave 19
`6fe2ec4 af1a8e9 674ccae` — Wave 20
`9e1f556` — global snapshot with parallel-agent work
`a42aab8 78dc7ea` — Wave 21

**Lesson**: first attempt at Waves 20 + 21 shipped as fat-primitive
scaffolding with a commit message that claimed "closed" when it was
actually "scaffolded". User caught it. Redid as real lifts. Honest
commit messages matter more than volume of commits.

---

## 2026-04-20 — 🏁 BACKEND FORTH-PORTABLE: Wave 9b + Commit B SHIPPED

**B++ has a portable-spine backend.**

19 commits into the Phase 3.4 arc, `cg_emit_node` and
`cg_emit_func` both live in `bpp_codegen.bsm` and dispatch
through `cg_prim` fn_ptr tables to chip-local implementations.
Adding a new architecture (RISC-V, WebAssembly, anything) is
now **one file of primitives**, not a fork of the tree walker.

**This final-activation session's commits**:

```
be275f8  Commit B — cg_emit_func spine flip (fat-primitive)
46a2702  Wave 9b A.3+A.4 — cg_emit_call spine + T_CALL dispatch flip
258b5e5  Wave 9b A.2 — real call_extern primitive bodies
fda6dac  Wave 9b A.1 — T_CALL extracted to chip-local helper
```

**Production path now**:

```
bpp_codegen.bsm    →  cg_prim (fn_ptr table)  →  <chip>_codegen.bsm
cg_emit_node                                     a64_emit_call, x64_emit_call
  → cg_emit_call                                 a64_emit_func, x64_emit_func
  → cg_emit_func                                 (+ 80+ fine-grained primitives
                                                   from Waves 1-15)
```

**Design call: fat-primitive for Wave 9b + Commit B**

The doc's pseudo-code for the fine-grained cg_emit_call (walking
args, calling arg_reg_int + push_arg_int + call_direct per arg)
assumes a different scheduling than the chip's inline:

- Inline has B2 `: base` helper splicing (AST clone + recurse)
- Inline pushes all args to stack then pops to x0..x(N-1)
- Inline does FFI int/float separation AFTER the pop-to-regs

The doc's spine writes each arg directly into its ABI register.
Rewriting spine per the doc + flipping dispatch would break
byte-identity because the alloc order differs. The finer-grained
primitives stay RESERVED in the `ChipPrimitives` struct (arg_reg_*,
emit_push_arg_*, emit_call_direct, etc.) for a future decomposition
session where the spine owns ABI scheduling and each chip
reimplements its allocator to match.

The fat-primitive (emit_call_full + emit_func_full) wraps the
chip's extracted helper verbatim. Spine owns dispatch; chip owns
implementation. Zero byte-identity risk. Same design as Wave 8's
emit_memld_load (chip handles the sub-word + bit-packed dispatch
matrix internally).

**What's not shipped (explicit)**

- **Commit C (Waves 16/17)**: cg_emit_module + cg_emit_all. The
  callers live in bpp.bpp (frozen per the pitfall list). Fat-
  primitive wrap without caller-flip is decorative; doc itself
  flags these as "low marginal portability value — skip if
  time tight." Shipping the full emit_module spine requires
  either unfreezing bpp.bpp's dispatch or restructuring
  emit_module to call cg_emit_func internally (which it
  already does, since Commit B's caller flip).

- **Fine-grained T_CALL primitives activation**: the 14 Wave 9
  slots (arg_reg_*, push_arg_*, pre/post_call_align,
  call_direct/extern, copy_ret_*, save/restore_caller_saved)
  stay wired but inactive. Activating them requires rewriting
  chip emit_call to match spine's ABI scheduler — a session
  that should profile the current pattern, design the new
  schedule, and migrate in one pass without byte-identity
  assumptions.

**The alien-parasite vision unblocked**

With `cg_emit_node` + `cg_emit_func` spine-driven, adding a chip
becomes:

1. `<chip>_enc.bsm` — instruction byte encoders (~500 lines)
2. `<chip>_primitives.bsm` — 80+ primitives mapping each slot
   to the chip's ISA (~900 lines)
3. `<chip>_codegen.bsm` — thin: init_codegen_<chip>, emit_call,
   emit_func, emit_module, emit_all (~800 lines today; shrinks
   further once Wave 9 decomposition ships)

RISC-V port = mechanical translation, one session. WebAssembly
backend = another. Install.sh chameleon (auto-detects host chip,
cross-compiles) = half session.

**End-of-session canary hashes (now stable through 19 commits)**

```
HEAD                   be275f8  Commit B (cg_emit_func)
bpp_codegen.bsm          940   (+55 this session — cg_emit_call +
                                 cg_emit_func + 2 fat-primitive
                                 slots + docs)
a64_codegen.bsm         2642   (+18 — extracted emit_call + slot
                                 wirings + T_CALL/emit_func dispatch
                                 flips)
a64_primitives.bsm       807   (+13 — real call_extern body)
x64_codegen.bsm         1977   (+16)
x64_primitives.bsm       423   (+9)

Canary hashes — pinned for 19 consecutive commits:
  pathfind  50caa64bfa7f4476d0780c5857304db66176d852
  rhythm    3d4f424b2ae7071110d8962750aaa700f2c57009
  bang9     7a76c3b8f6d9cb7021cb4a221f5c9980accdee02
```

**Phase 3.4 arc complete. 🏁**

Next mountain: RISC-V port (Phase 5), install chameleon (Phase 6).
Then the ecosystem (ModuLab, Wolf3D, Bang 9 IDE, bangscript
runtime) continues on top of a truly portable foundation.

## 2026-04-20 — Wave 9b steps A.1 + A.2 (T_CALL extraction + extern bodies)

Third session of the 2026-04-20 backend push. After the jedi's
Phase 3.5 activation at 914ebf6, this session picked up the
final activation doc (phase_final_activation.md) and shipped
the two lowest-risk steps of Wave 9b's 4-step plan.

**Commits**

```
258b5e5  Wave 9b step A.2 — real call_extern primitive bodies
fda6dac  Wave 9b step A.1 — T_CALL extracted to chip-local helper
```

**A.1 — T_CALL extracted to chip-local helper** (pure refactor).
The 770-line T_CALL body in a64_emit_node and the 615-line
equivalent in x64_emit_node were moved into new static
functions `a64_emit_call(n)` and `x64_emit_call(n)`. The
emit_node case became a one-line delegate. Byte-identity
trivially preserved — pure function extraction, no logic
change. Done via `sed` surgery (extract body lines + assemble
new file with forward decl + function block) to avoid manually
moving 770+615 lines through Read/Edit turns.

**A.2 — Real bodies for call_extern primitives.** Filled the
stubbed `_a64_emit_call_extern` + `_x64_emit_call_extern`
with the exact patterns from the extracted helpers. a64 uses
GOT-anchored FFI via adrp+ldr+blr with reloc type 3; x64 uses
call-rel32 with reloc type 4. Both save xmm0/xmm1 to rbp-
relative scratch slots so float_ret()/float_ret2() builtins
can recover FFI float returns.

The save_caller_saved_int + restore_caller_saved_int
primitives stay as empty stubs. The doc's risk #3 predicts
these are no-ops in the current B++ register model (B3-
promoted regs are callee-saved via prologue/epilogue;
freelist manages scratch; nothing is live across a call
that needs explicit save). Next session's Step A.3 will
verify empirically when cg_emit_call activates the primitives.

**What remains for the final activation session**

Steps A.3, A.4, and Commit B:

- **A.3** — Write `cg_emit_call(n)` in `bpp_codegen.bsm`.
  The doc has a 50-line pseudo-code in §COMMIT A Step A.3.
  Walks args, dispatches through `cg_prim.arg_reg_int` +
  `emit_push_arg_int` + `emit_call_direct/extern` + pre/post
  alignment + save/restore_caller_saved.

- **A.4** — Flip chip dispatch. Currently each chip's T_CALL
  case delegates to the extracted `<chip>_emit_call(n)` helper.
  Need to split that helper into two paths: builtins
  (~10 names: peek, poke, sizeof, float_ret[2], shr, assert,
  str_peek, sys_write, sys_read, possibly more) stay chip-
  local; everything else routes through `cg_emit_call(n)` in
  the spine. Budget the builtin catalog ~30m per doc's
  surprise #1.

- **Commit B** — Spine flip `cg_emit_func`. After T_CALL is
  activated, mirror the pattern for emit_func: portable
  cg_emit_func in spine + one-line delegate in chip. Frame
  math divergence (surprise #2) is the decision point.

- **Commit C** — Optional Waves 16/17 cleanup.

**Why stop here**

A.1 + A.2 are byte-identity-trivial. A.3 + A.4 require
careful integration (builtin catalog preserving semantics,
arg-eval-order edge cases, caller-saved empirical check).
Attempting those in remaining session context risks a bad
ship. Stopping at a proven-stable checkpoint lets the next
session pick up the extracted helper functions (which are
now chip-local + well-typed) and write the spine + flip
dispatch in one focused pass.

**End-of-session state**

```
HEAD                    258b5e5  Wave 9b step A.2
bpp_codegen.bsm            885   (unchanged — A.3 will add cg_emit_call)
a64_codegen.bsm           2635   (+11: T_CALL body moved to chip helper)
a64_primitives.bsm         807   (+13: call_extern real body)
x64_codegen.bsm           1972   (+11: T_CALL body moved)
x64_primitives.bsm         423   (+9: call_extern real body)

Canary hashes (immutable for 16 commits now):
  pathfind  50caa64bfa7f4476d0780c5857304db66176d852
  rhythm    3d4f424b2ae7071110d8962750aaa700f2c57009
  bang9     7a76c3b8f6d9cb7021cb4a221f5c9980accdee02
```

## 2026-04-20 — Backend closeout scaffolding: Wave 9 + Phase 3.5/4 contract surface

Continuation of the same day. After the 12-wave Phase 3.4
shipped, the user pointed me at `docs/phase_backend_closeout.md`
(810 lines, the jedi's spec for Wave 9 + Phase 3.5 + Phase 4).
Reading the doc made clear that the remaining 6 waves split
into "scaffolding" (declare slots + add stubs + wire fn_ptrs,
zero output change) and "activation" (write real bodies, flip
spine dispatch, delete chip-side inline). This session did the
scaffolding for ALL 6 waves in two commits.

**Commits**

```
315b896  Phase 3.5+4 scaffolding (13 primitives, bodies stubbed)
9ccf082  Wave 9 scaffolding (14 primitives, partial bodies)
```

**What this means concretely**

- Contract is COMPLETE: 87 slots in `struct ChipPrimitives`
  cover every primitive the doc anticipates for the Forth-portable
  backend.
- Both chips' `_primitives.bsm` files have a function for every
  slot — some real (Wave 9 trivial primitives like arg_reg_*),
  some stubs (Phase 3.5/4 + Wave 9 caller-saved + extern call).
- `cg_install_<chip>_primitives()` populates every slot.
- Spine `cg_emit_node` T_CALL stays inactive; `cg_emit_func`
  / `cg_emit_module` / `cg_emit_all` don't exist yet.
- Production path is unchanged — chip's existing inline
  T_CALL + emit_func/module/all keep producing identical
  output. Byte-identity verified across all 4 commits today
  (Phase 3.4's 9 + this session's 2).

**Why scaffolding-only this session**

Wave 9 alone is doc-budgeted at 3-4h with 5 documented risks
(caller-saved scope, b3_select shared globals, asm-mode
bundling, etc.). Trying to ship full Wave 9 + 13 + 14 + 15 +
16 + 17 in one session was risk-ratio bad: 6 chances for a
byte-identity break, each rolling back hours of work. The
scaffolding strategy locks in the contract surface across one
session with zero risk, then a followup session writes bodies
and flips dispatches in a focused pass.

**Followup session work (Wave 9b + activation)**

Per scaffolding contract, the followup session:
1. Writes real bodies for the 13 stubbed primitives.
2. Writes `cg_emit_func`, `cg_emit_module`, `cg_emit_all`
   in spine.
3. Flips dispatch: chip's `emit_func{_arm64,_x86_64}` becomes
   `return cg_emit_func(fi);` (or gets deleted entirely);
   same for emit_module/emit_all.
4. Reverse-engineers caller-saved spill set from inline T_CALL
   for `_<chip>_emit_save_caller_saved_int`.
5. Activates spine T_CALL by replacing chip's inline body
   with `return cg_emit_call(n);`.
6. Validates byte-identity at each step.

After the followup ships, the closeout doc's expected end state
realizes:
```
bpp_codegen.bsm        ~1500 lines (all portable)
a64_codegen.bsm        ~400 lines (init + wrapper)
a64_primitives.bsm     ~900 lines (full bodies)
x64_codegen.bsm        ~300 lines
x64_primitives.bsm     ~700 lines
```

**Re-install pattern reminder for next agent**: the per-emit_module
re-install of `cg_install_<chip>_primitives()` (introduced as
Wave 2's bug fix in commit 365b896) MUST move with emit_module
when Wave 16 activates. Either the chip's emit_module wrapper
stays as a thin shim that calls install + cg_emit_module, or
cg_emit_module gets target-aware install. Plan calls out the
second as cleaner.

**End-of-session state (use as next-session baseline)**

```
HEAD                 315b896  (Phase 3.5+4 scaffolding)
pathfind             50caa64bfa7f4476d0780c5857304db66176d852
rhythm               3d4f424b2ae7071110d8962750aaa700f2c57009
bang9                7a76c3b8f6d9cb7021cb4a221f5c9980accdee02

Total wired slots in cg_prim: 49 (Phase 3.4) + 14 (Wave 9) +
13 (Phase 3.5/4) = 76 active slots, contract has 87.
Waves shipped substantively: 8 (Phase 3.4 cumulative).
Waves shipped as scaffolding: 6 (Wave 9 + 13 + 14 + 15 + 16 + 17).
```

## 2026-04-20 — Phase 3.4 closeout: Waves 1-12 of chip_primitives migration

12 waves walked through to completion — 8 with substantive
migration, 1 carried forward (Wave 9 / T_CALL — too coupled
with chip-side scheduling for wave-style call-site
replacement), 1 N/A (Wave 11 — no T_SUBSCRIPT/T_FIELD AST
node types; subscript+field both lower to T_MEMLD), 2
collateral (Wave 12's emit_stmt cases were migrated
alongside Wave 7's emit_node cases because the inline bodies
were identical).

**Final tally**: 35 of 60 contract slots wired, ~250 LOC
shrunk in chip codegens cumulative, 9 commits in this
session, all byte-identical against the jedi handoff
canaries.

**Wave 9 deferred** — see commit 0d38cb0 message. T_CALL
needs its own session; the wave-style call-site replacement
applied to Waves 1-8/10 doesn't fit the 770-line interleaved
ABI dispatch.

## 2026-04-20 — Phase 3.4 first half: Waves 1-7 of chip_primitives migration

Seven waves shipped, all byte-identical against the jedi handoff
hashes (pathfind 50caa64b, rhythm 3d4f424b, bang9 7a76c3b8). The
Forth-style portable spine + per-chip `_primitives.bsm` pattern
is fully bootstrapped — every commit kept the suite of three
canary programs hash-stable.

**Commits (newest first)**

```
80e3355 Wave 7  — T_IF / T_WHILE / T_TERNARY / T_RET / T_BREAK / T_CONTINUE (8 primitives)
0de1d29 Wave 6  — T_BINOP comparisons (cmp_int / cmp_flt with portable cond_id)
e388d07 Wave 5  — T_BINOP arith + bitwise + shift (14 primitives)
70f3a45 Wave 4  — T_UNARY ~ / int -- / float --
d4b4b17 Wave 3  — T_VAR local + global
365b896 Wave 2  — T_LIT int / string / float
b7495f3 Wave 1  — nd == 0 dispatch
698fc61 Foundation — Phases 1-3.3 baseline (jedi pit stop)
```

**Wave 7 highlight: first real chip-code shrink**

Net **−198 LOC** in chip codegens (vs. previous waves which
were near-flat because each `enc_X` → `call(p.X, ...)`
substitution was 1:1 in line count). Wave 7's control-flow
cases were dense `if (a64_bin_mode) { enc_*_label(...); } else
{ out("..."); a64_print_dec(lbl); ... }` blocks — collapsing
each into a `call(p.emit_jump, lbl)` reclaims the bin/asm
duplication. The same shrink pattern will repeat at Waves
9 (T_CALL) + 12 (emit_stmt) + 13-19 (emit_func), where
inline branches dominate.

**Chip_primitives table state**

`struct ChipPrimitives` declared with all 49 contract slots up
front (44 originally + 5 added across waves). `cg_prim` is a
single global pointer allocated lazily. Wired this session:

```
arith       emit_add  emit_sub  emit_mul  emit_div  emit_mod
            emit_neg
            emit_fadd emit_fsub emit_fmul emit_fdiv
logical     emit_and  emit_or   emit_xor  emit_shl  emit_shr
            emit_not
move/cmp    emit_mov_zero  emit_mov_imm
            emit_cmp_int   emit_cmp_flt
memory      emit_load_local  emit_load_global  emit_load_str_addr
            emit_load_float_const
float-extra emit_fneg
```

23 of 49 slots in use after Wave 6; Wave 7 added 8 more
(emit_new_label, emit_label, emit_jump, emit_branch_if_zero,
emit_fcond_to_int_truth, emit_promote/demote, emit_jump_to_epilogue).
Total wired: 31 of 56 slots. Waves 8-12 will fill emit_load /
store, emit_call + ABI metadata (arg_reg, return_reg, etc.),
emit_push/pop, plus T_ASSIGN dispatch.

**Init-order bug found and fixed (Wave 2 surface)**

Critical infra bug uncovered when Wave 2 tested byte-identity:
`init_codegen_arm64()` and `init_codegen_x86_64()` both run
unconditionally at startup, last-init-wins on the shared
`cg_prim` table. With x86_64 init at line 66 of `bpp.bpp`
running after arm64's at line 63, the table always carried x64
fn_ptrs — arm64 emission then called x64 primitives that wrote
into `x64_enc_buf` (unused by arm64 mode), silently dropping
the instruction from arm64 output. Wave 1 masked the bug
because `nd == 0` is rare; Wave 2 (every literal) surfaced it
as a 33 KB pathfind size delta.

**Fix**: extracted `cg_install_arm64_primitives()` and
`cg_install_x86_64_primitives()` helpers, called both from
`init_codegen_<chip>` AND from the top of `emit_module_<chip>`.
Per-module re-install costs a handful of word stores and
guarantees the active chip owns `cg_prim` during emission.

**T_SIZEOF doesn't exist**

Plan listed `T_SIZEOF` for Wave 1 but `bpp_defs.bsm` has no
such constant — parser lowers `sizeof(X)` to a `T_LIT` with
the size baked in. Wave 1 collapsed to the `nd == 0` case
alone. Documented in commit message.

**B3 promoted-locals interaction (Wave 3) honoured**

Per the plan's Anticipated Surprise #8, the B3 promoted-register
short-circuit lives in the chip's existing `<chip>_emit_load_var`
function. Wave 3's `emit_load_local` primitive is a thin wrapper
that delegates — the spine never sees the promotion table. Same
for the SL_DOUBLE / SL_HALF / SL_QUARTER sub-word dispatch
matrix.

**Visibility changes**

Several previously-`static` chip helpers became public so the
primitives in `_primitives.bsm` can delegate to the existing
implementations: `a64_print_dec`, `a64_print_p`, `a64_emit_mov`,
`a64_emit_load_var`, `x64_emit_load_var`. None of them changed
behaviour.

**Line counts (end of session)**

```
src/bpp_codegen.bsm                              823  (+225 since handoff)
src/backend/chip/aarch64/a64_codegen.bsm        2854  (+2 — net wash, primitives extracted offset by spine wiring)
src/backend/chip/aarch64/a64_primitives.bsm      288  (new, Wave 1)
src/backend/chip/x86_64/x64_codegen.bsm         2010  (+6)
src/backend/chip/x86_64/x64_primitives.bsm       181  (new, Wave 1)
```

The chip codegens haven't shrunk yet because Waves 1-6 mostly
added one-line `call(p.X, ...)` replacements where the original
was a multi-line `if (a64_bin_mode) { enc_X(...); } else { out("..."); }`.
Net per-case savings show up as those calls collapse multi-line
bodies into single-line dispatches. Bigger shrink expected at
Wave 12 when `emit_node` itself becomes a thin tail-call to
`cg_emit_node`.

**Open for next session**

Waves 8-12 in order:

```
Wave 8   T_MEMLD / T_MEMST / T_ADDR    (sub-word + bit-packed dispatch — needs chip-side helper extraction first)
Wave 9   T_CALL                        (HARD — ABI divergence; budget 2-3 h)
Wave 10  T_ASSIGN
Wave 11  T_SUBSCRIPT / T_FIELD
Wave 12  emit_stmt migration
```

Then Waves 13-19 for emit_func + epilogues. Then Phase 4 dedup,
Phase 5 RISC-V validation, Phase 6 install.sh chameleon. The
chip_primitives infrastructure is fully proven by Wave 7 (the
control-flow primitives went green on first try) — remaining
waves have no architectural risk, only volume.

**Why stop at Wave 7 today**: Wave 8 cases (T_MEMLD bit-packed
fields, T_MEMST struct copies, T_ADDR global addressing) carry
4-way dispatch matrices INLINE that need chip-side helper
extraction before they can be wrapped as primitives. That's
the same pre-work the doc's own pitfall #5 implies for the
remaining cases. Mid-session attempt at Wave 8 with diminishing
context risks introducing a byte-identity break that forces a
rollback. Better to ship 7 clean than 8.5 with a bug.

End-of-session canary hashes (use as next-session baseline):

```
bpp        regenerated each wave — re-bootstrap from src first
pathfind   50caa64bfa7f4476d0780c5857304db66176d852
rhythm     3d4f424b2ae7071110d8962750aaa700f2c57009
bang9      7a76c3b8f6d9cb7021cb4a221f5c9980accdee02
```

The three canaries — the actual byte-identity contract — are
pinned to the original handoff values. Any future wave that
breaks one of these three hashes must roll back.

## 2026-03-18 — Stage 2 Complete: Self-Hosting Parser

### Milestone

The B++ bootstrap compiler is now at **Stage 2 of 3** toward self-hosting.
The full pipeline works end-to-end:

```
source.bpp  -->  bpp_lexer (B++)  -->  token stream  -->  bpp_parser (B++)  -->  JSON IR
```

Both the lexer and parser are written in B++ itself, compiled through the
Python compiler, and produce output identical to the Python compiler's IR.

### What was built

| Component | File | Status |
|-----------|------|--------|
| Python compiler (lexer + parser + IR) | `bpp_compiler.py` | Working — reference implementation |
| Python emitter (raylib demo) | `test_suite2.py` | Working — raylib cube demo runs |
| B++ lexer (Stage 1) | `bootstrap/lexer.bpp` | Working — tokenizes all .bpp files |
| B++ lexer build script | `bootstrap/build_lexer.py` | Working |
| B++ parser (Stage 2) | `bootstrap/parser.bpp` | Working — parses all .bpp files |
| B++ parser build script | `bootstrap/build_parser.py` | Working |

### Key technical decisions

- **Memory model**: `uint8_t mem_base[1MB]` with bump allocator. All B++ values
  are `long long`. Byte access via `peek()`/`poke()` built-ins.
- **Pack/unpack**: String references stored as `(start, len)` pair in one
  `long long` using `s + l * 0x100000000`.
- **C helper injection**: JSON emitter uses C functions (`jot`, `jkp`, `jopen`)
  injected by the build script to avoid `\"` escape sequences, which B++ string
  literals do not support.
- **Forward declarations**: B++ stubs (`parse_statement() { return 0; }`) are
  overwritten by later definitions since the Python compiler stores functions in
  a dict (last definition wins). C forward declarations generated automatically.
- **Buffer sizing**: Token buffer needs 262KB+ for large files (~5000+ tokens
  at 24 bytes each). Fixed after overflow was found when parser parsed itself.

### Bugs fixed during bootstrap

1. **`*` missing from tokenizer regex** — multiplication and dereference silently
   dropped. Fixed in `bpp_compiler.py`.
2. **No operator precedence** — flat left-to-right parsing. Fixed with
   precedence climbing in both Python compiler and B++ parser.
3. **Emitter double-wrapping memory expressions** — string heuristic caused
   nested `mem_base + (mem_base + ...)`. Fixed with proper `mem_load`/`mem_store`
   IR nodes.
4. **String escaping in B++** — B++ tokenizer terminates strings at first `"`,
   no escape support. Fixed by using C helper functions for JSON output.
5. **Token buffer overflow** — 65KB too small for ~5000 tokens. Increased to 262KB.

### Validation

- `lexer.bpp`: 19/19 functions match Python compiler IR (normalized).
- `parser.bpp`: Successfully parses itself (41 functions, 38 globals, 5 externs).
- `raylib_demo2.bpp`: Parses correctly (1 function, 9 globals).

---

---

## 2026-03-19 — Type Inference + Dispatch Analysis

### Milestone

B++ now has a type inference engine and a dispatch analyzer — the two core
modules that make B++ more than "C without types". The compiler can infer
the smallest type for every variable and classify loops for parallel/GPU
execution, all without the programmer writing a single type annotation.

### Architecture decisions

- **No header files** — `import "file.bpp";` reads, parses, and merges
  another B++ file's IR. `import "lib" { ... }` declares FFI functions.
  Circular import protection included. Inspired by Go modules and C++20
  modules, but simpler (B++ has no legacy to support).
- **Optional type annotations** — the programmer writes `auto x = 10;`
  (compiler infers byte) or `word x = 10;` (programmer overrides). The
  escape hatch exists but is rarely needed.
- **Unified compiler** — no JSON between stages in the final product.
  Pipeline is lex → parse → type → dispatch → codegen in one process.
- **Separate type and dispatch modules** — types.bpp answers "what is this
  data?", dispatch.bpp answers "where should this code run?". Both feed
  into codegen but evolve independently.

### What was built

| Component | File | Status |
|-----------|------|--------|
| Module import system | `bpp_compiler.py` | Working — file import + FFI import |
| Type inference (5 types) | `bpp_compiler.py` | Working — byte/half/word/long/ptr |
| Dispatch analysis | `bpp_compiler.py` | Working — seq/parallel/gpu/split |
| Shared constants | `bootstrap/defs.bpp` | Working — 30 globals, 1 function |
| Type inference (B++) | `bootstrap/types.bpp` | Parses — 11 functions, 36 globals |
| Dispatch analysis (B++) | `bootstrap/dispatch.bpp` | Parses — 14 functions, 40 globals |

### Type system

Five primitive types inferred from context:

| Type | Size | Inferred when |
|------|------|---------------|
| byte | 8-bit | Literal 0-255, `peek()` return |
| half | 16-bit | Literal 256-65535 |
| word | 32-bit | Literal 65536-4G, `int` FFI return |
| long | 64-bit | Literal > 4G, `mem_load` (dereference) |
| ptr | 64-bit | `malloc()` return, string literals, pointer arithmetic |

Rules: binary ops promote to larger type. Ptr wins everything. Variables
take the largest type seen across all assignments in the function.

### Dispatch analysis

Four dispatch classifications for while-loops:

| Code | Meaning | Trigger |
|------|---------|---------|
| sequential | Must run in order | Side effects (extern calls, global writes) |
| parallel | CPU parallelizable | No side effects, independent iterations |
| gpu_candidate | Can run on GPU | Parallel + strided memory access |
| split_candidate | Mixed — split update/draw | Strided access but some side effects |

Tested on `raylib_demo2.bpp` (10-cube bouncing demo):
- Init loop: **gpu_candidate** (strided `OBJS + i * 40`, no side effects)
- Game loop: **sequential** (BeginDrawing, ClearBackground, EndDrawing)
- Inner object loop: **split_candidate** (7 pure stmts + 1 DrawRectangle)

### Bugs fixed

6. **Type keyword / variable name ambiguity** — `ptr` is both a type keyword
   and a valid variable name. `ptr = expr;` was parsed as type declaration.
   Fixed by checking for `=` after type keywords to disambiguate.

### Future type system expansion

```
Layer 1 (now):  byte, half, word, long, ptr
Layer 2 (next): vec2, vec4, matrix, fixed (game math)
Layer 3 (future): handle (coroutine/entity), buffer (GPU memory)
```

---

## Next Steps — Stage 3: Code Generation

### Option A: C Emitter in B++ (simplest path to self-hosting)

Write a B++ program (`bootstrap/emitter.bpp`) that reads JSON IR from stdin and
outputs C code to stdout. This would complete the self-hosting chain:

```
source.bpp | bpp_lexer | bpp_parser | bpp_emitter > output.c
clang output.c -o output
```

The emitter is the simplest stage — it just walks the IR JSON and prints C code.
The Python `build_lexer.py` / `build_parser.py` already contain the emitter
logic in `translate_node()` and `generate_c()`.

### Option B: ARM64 Native Code Generator

Replace the C backend with direct ARM64 (AArch64) machine code emission:

```
source.bpp | bpp_lexer | bpp_parser | bpp_codegen > output.o
```

This eliminates the dependency on clang entirely. Key considerations:

- **Calling convention**: ARM64 uses x0-x7 for arguments, x0 for return value,
  x19-x28 are callee-saved. Stack alignment must be 16 bytes.
- **Memory model**: Can keep the `mem_base` array approach, or transition to
  real pointers. Starting with mem_base is safer.
- **Output format**: Mach-O object file on macOS, or ELF on Linux. Could also
  emit assembly text and use `as` to assemble.
- **Simplest start**: Emit ARM64 assembly text (`.s` file), assemble with `as`,
  link with `ld`. Avoids dealing with binary object file formats initially.
- **Registers**: Since B++ has no types (everything is `long long`), all values
  fit in 64-bit registers. Use a simple register allocator or stack-based approach.

### Recommended order

1. **Stage 3A**: C Emitter in B++ (complete self-hosting via C backend)
2. **Stage 3B**: ARM64 codegen (eliminate clang dependency)
3. **Stage 4**: Bootstrap — compile the compiler with itself

Stage 3A is faster and proves the full pipeline works. Stage 3B can then replace
the C backend without changing the frontend.

---

## 2026-03-19 — Stage 3: C Emitter in B++

### Milestone

The B++ bootstrap compiler now has a C code emitter written in B++
(`bootstrap/emitter.bpp`). The full self-hosting pipeline works end-to-end:

```
source.bpp  -->  bpp_lexer  -->  bpp_parser  -->  bpp_emitter  -->  output.c
```

Tested on `lexer.bpp`: the emitter output compiles with clang and produces a
working lexer binary identical in behavior to the original.

### What was built

| Component | File | Status |
|-----------|------|--------|
| C emitter (B++) | `bootstrap/emitter.bpp` | Working — ~620 lines |
| Emitter build script | `bootstrap/build_emitter.py` | Working — builds native `bpp_emitter` |

### How the emitter works

1. **JSON reader**: Reads IR from stdin character by character. Handles strings
   with escape sequences, nested objects/arrays, null values.
2. **IR parser**: Builds in-memory AST nodes (64-byte fixed-size, same layout
   as parser). Handles all 13 node types. Forward declaration dedup (keeps last
   definition).
3. **Used extern scanner**: Walks all function bodies to find which extern
   functions are actually called (for `#include` generation).
4. **C emitter**: Walks AST and outputs C with proper indentation. Handles FFI
   type casts, pointer conversion, built-in functions.

### Key technical decisions

- **JSON parsing in B++**: No general-purpose JSON library — the parser knows
  the exact IR structure and reads fields in order. Uses `jnext_key()` helper
  that handles comma + key + colon in one call. `jskip_str()` avoids wasting
  string buffer on discarded key names.
- **Forward declaration dedup**: B++ uses stub-then-real pattern
  (`parse_statement() { return 0; }` followed by real body). The emitter's
  `find_func()` detects duplicates and overwrites the stub.
- **FFI pointer conversion**: Pointer-type args (`char*`, `void*`, `ptr`,
  `word`) get `(const char*)(mem_base + offset)`. String literal args detected
  by checking if AST node is `T_LIT` starting with `"` — these get
  `(const char*)` instead.
- **Built-in mapping**: `malloc` → `bpp_alloc`, `peek` → byte read from
  `mem_base`, `poke` → byte write, `|` → `||` in C.
- **C output helpers**: `out()` and `print_buf()` injected by
  `build_emitter.py` — avoids escape sequence issues in B++ string literals.

### Known limitations

- **File imports**: `import "defs.bpp";` is resolved by the Python compiler
  only. The `bpp_parser` doesn't read files. Files with file imports need:
  `python3 bpp_compiler.py file.bpp | bpp_emitter > out.c`
- **Injected helpers**: `parser.bpp` depends on `jot`/`jkp`/`jopen` from its
  build script — can't go through the generic emitter pipeline.
- **Cosmetic differences**: Emitter output differs from Python emitter (no
  comments, different global order) but is functionally equivalent.

### Bugs fixed

7. **Duplicate function definitions** — Forward declaration stubs emitted
   alongside real bodies caused C redefinition errors. Fixed with `find_func()`
   dedup in `parse_one_func()`.

### Validation

- `lexer.bpp`: Full pipeline produces C that compiles and runs correctly.
- `emitter.bpp`: Python compiler IR → `bpp_emitter` → C output compiles.
- Build system: `build_emitter.py` compiles `emitter.bpp` to native
  `bpp_emitter` via clang.

---

---

## 2026-03-19 — Fat Structs

### Milestone

B++ now supports structs with named field access, `sizeof`, and nested structs.
This is purely syntactic sugar — the parser lowers struct operations to
`mem_load`/`mem_store` with computed offsets before types or dispatch see them.

### Syntax

```
struct Vec2 { x, y }
struct Rect { pos: Vec2, w, h }

auto p: Vec2;
p = MemAlloc(sizeof(Vec2));
p.x = 100;
p.y = 200;

auto r: Rect;
r.pos.x = 10;   // nested access chains offsets
```

### How it works

- `p.x` → `mem_load(ptr: binop(p + 0))`, `p.y` → `mem_load(ptr: binop(p + 8))`
- `sizeof(Vec2)` → compile-time constant `16` (2 fields × 8 bytes)
- Nested structs: `r.pos.x` → offset 0 (pos offset 0 + x offset 0)
- Type annotations on `auto`, function params, and globals
- Struct info exported in IR as `ir["structs"]` for downstream tools
- Zero changes needed in types, dispatch, or emitter — they see plain
  `mem_load`/`mem_store`

### Validated by

- `snake_v2.bpp`: rewrote `snake_demo.bpp` using `Vec2` struct — same
  semantics, cleaner code (`head.x` instead of `*(head + 0)`)

---

---

## 2026-03-19 — Standard B (stb) Library + Snake v3

### Milestone

B++ now has a standard library called **Standard B (stb)** in `stb/`. Snake v3
is a full game using the complete stb stack — the first real program built
entirely on B++'s own libraries.

### Modules

| Module | Purpose | External dep |
|--------|---------|-------------|
| `stbmath.bpp` | Vec2, random, abs/min/max/clamp | raylib (MemAlloc, GetRandomValue) |
| `stbinput.bpp` | Keyboard + mouse input | raylib (IsKey*, GetMouse*) |
| `stbdraw.bpp` | Drawing, colors, text, numbers | raylib (Draw*, Clear*, etc.) |
| `stbui.bpp` | Immediate-mode GUI (label, button, panel, bar) | none — pure B++ |
| `stbstr.bpp` | String ops (len, eq, cpy, cat, chr, dup) | none — pure B++ (peek/poke) |
| `stbio.bpp` | Console + file I/O | **libc — BLOCKED** |
| `stbgame.bpp` | Game bootstrap (chains all inits) | raylib (InitWindow, etc.) |

### Key decisions

- **No libc in game libs**: Replaced `malloc` → `MemAlloc` and `rand` →
  `GetRandomValue` from raylib in all game modules.
- **Compiler auto-discovery**: `_STB_DIRS = ["stb"]` in the compiler resolves
  `import "stbmath.bpp"` to `stb/stbmath.bpp` automatically.
- **Import dedup**: Compiler tracks `_imported` set — no double-processing.
- **draw_number**: Converts int to string internally using peek/poke, hidden
  from the API. User just calls `draw_number(x, y, sz, color, val)`.

### Snake v3 features

- Full game loop with stbgame
- Directional input via named constants (KEY_UP, KEY_RIGHT, etc.)
- In-memory top-5 ranking (sorted insertion)
- On-screen game-over panel with ranking display (stbui + stbdraw)
- Console ranking printout (stbio)
- Restart with SPACE

### Shim fixes (test_suite_snake_v3.py)

- `g_` prefix for all B++ globals (avoids raylib macro collisions)
- `bpp_` prefix for all B++ functions (avoids C stdlib name collisions)
- `BPP_STR` macro: runtime heuristic to distinguish real pointers from
  mem_base offsets — solves the dual-pointer problem when string literals
  pass through B++ function parameters
- String literal cast: `(long long)"text"` for passing strings through
  B++ functions typed as `long long`

### Bootstrap lexer updates

- Added `.` (46) and `:` (58) to `is_punct`
- Added `struct`, `sizeof`, `ptr`, `byte`, `half`, `long` to keyword list

---

---

## 2026-03-19 — I/O Design Decision: Syscalls, Not libc

### Problem

`stbio.bpp` is the last module with a libc dependency (`putchar`, `puts`,
`fopen`, `fread`, `fwrite`, `fclose`). Two approaches were rejected:

1. **Keep `import "libc"`** — exposes C dependency in B++ source code.
2. **Hide libc behind compiler built-ins** — compiler emits `stdio.h` calls
   invisibly. Still depends on C runtime.

### Decision

**I/O must be a fundamental B++ capability implemented via raw syscalls.**
No libc, no raylib, no hidden C.

> "IO é basico pra qualquer linguagem e tem que ser raiz do bpp"

### Design

- macOS ARM64 syscalls: `write` (4), `read` (3), `open` (5), `close` (6)
- `putchar`/`getchar` become thin wrappers over `write`/`read` on fd 0/1
- stbio.bpp builds `print_int`, `print_msg`, etc. in pure B++ on top of
  these primitives
- During C-emit stage: inline asm (`svc #0x80`) or linked `.s` file
- After ARM64 codegen: native syscall instructions directly

### Status

Design decided. Implementation pending. This is on the critical path to
making snake_v3 run without any libc dependency and toward full bootstrap.

---

---


## 2026-03-20 — Self-Hosting Achieved

### Milestone

The B++ compiler compiles itself. Fixed point verified: emitter_v2 compiles
emitter.bpp producing emitter_v3.c, which is identical to emitter_v2.c.

All three bootstrap stages (lexer, parser, emitter) are now libc-free.
Generated C uses only `#include <stdint.h>` and inline ARM64 syscall stubs.

### What was done

- Removed all libc from build_*.py shims (stdio.h, string.h, strncmp)
- Added syscall runtime to all build scripts (bpp_sys_write, bpp_sys_read)
- Added built-in handlers: putchar, getchar, str_peek in all build_*.py
- Rewrote parser.bpp: jot/jkp/jopen as pure B++ (were injected C helpers)
- Rewrote emitter.bpp: fully self-contained (~1200 lines), zero imports
  - out(), buf_eq(), print_buf() as native B++ functions
  - emit_runtime() emits syscall stubs in pure B++
  - emit_q()/emit_reg() helpers for inline asm emission
  - Global deduplication in emit_globals()
- Added file I/O built-ins: sys_open, sys_read, sys_close, sys_write
- Added argv access: argc_get(), argv_get(i)
- Created ./bpp compiler driver (shell script)
- Wrote bootstrap/import.bpp — Stage 0 import resolver

---

---

## 2026-03-20 — Snake v3 Compiles via B++ Pipeline

### Milestone

Snake v3 compiles and runs through the full B++ pipeline, no Python involved:

```
./bpp snake_v3.bpp -o snake_v3 [clang flags]
./snake_v3
```

This is the first real multi-file game compiled end-to-end by the B++ compiler.
The 4-stage pipeline works: `bpp_import → bpp_lexer → bpp_parser → bpp_emitter`.

### Bugs fixed

8. **macOS syscall carry flag** — On macOS ARM64, `svc #0x80` sets the carry
   flag on error and returns positive errno in x0. `if (fd < 0)` never triggered
   because errno 2 (ENOENT) looks like fd 2 (stderr). Fixed by adding
   `csneg x0, x0, x0, cc` after `svc #0x80` in all syscall stubs. This negates
   x0 when carry is set, giving Linux-style negative errno. Applied in:
   - `emitter.bpp` emit_runtime() (4 stubs: write, read, open, close)
   - `build_emitter.py`, `build_lexer.py`, `build_parser.py` (2 stubs each)

9. **Import resolver recursion** — `fbuf` was a single global buffer overwritten
   by recursive imports (e.g., snake_v3 → stbgame → stbinput). Fixed with
   stack-based allocation: `fbuf_top` tracks the current position, each call
   saves/restores it. Path also copied into the fbuf stack so it survives
   child calls clobbering `pathbuf`.

10. **Raylib name collisions** — `#include <raylib.h>` defines `KEY_UP`, `BLACK`,
    etc. as enums/macros that collide with B++ globals. Fixed by having the
    emitter forward-declare extern functions directly instead of including
    library headers:
    ```c
    // Before (broken):
    #include <raylib.h>
    long long KEY_UP = 0;  // ERROR: redefines enum

    // After (works):
    int IsKeyDown(int);
    void DrawRectangle(int, int, int, int, int);
    long long KEY_UP = 0;  // No conflict
    ```

11. **Parser splits `char*` into two tokens** — The lexer tokenizes `char*` as
    `char` + `*`, and the parser pushed them as separate FFI argument types.
    `DrawText` got args `[char, *, int, int, int, int]` instead of
    `[char*, int, int, int, int]`. Fixed in `parse_import()`: after reading a
    type token, if the next token is `*`, merge them into a pointer type by
    copying both into vbuf.

12. **Extern return type cast** — `MemAlloc` returns `void*` but B++ variables
    are `long long`. The emitter now wraps extern calls that return pointer
    types in `(long long)` cast.

13. **MemAlloc vs malloc** — Raylib's `MemAlloc` returns a real heap pointer,
    but B++ uses everything as mem_base offsets. `mem_base + real_pointer` =
    segfault. Replaced `MemAlloc` with `malloc` (mapped to `bpp_alloc`) in all
    stb modules and snake_v3.bpp.

14. **BPP_STR for extern args** — String values passed through B++ function
    parameters to extern calls were wrapped in `(const char*)(mem_base + v)`.
    But if the value is a real pointer (from a string literal cast to
    `long long`), this overflows. Fixed: non-literal pointer args now use
    `BPP_STR(v)` which auto-detects real pointers vs mem_base offsets.

15. **Color byte order** — Colors were stored as `0xRRGGBBAA` (human-readable)
    but raylib's `Color` struct on little-endian ARM64 needs `0xAABBGGRR`.
    `BLACK = 0x000000FF` was interpreted as `{r=255, g=0, b=0, a=0}` (red with
    zero alpha). Fixed with `rgba(r, g, b, a)` helper that computes the correct
    byte packing via `r + g*256 + b*65536 + a*16777216`.

### Key technical decisions

- **Forward declarations over includes** — The emitter now forward-declares
  extern functions with their FFI-specified types. This is more portable (no
  header dependency) and avoids all namespace collisions.
- **BPP_STR everywhere** — The dual-pointer problem (string literal pointer vs
  mem_base offset) is resolved uniformly via the BPP_STR macro for all extern
  pointer arguments.
- **malloc is a built-in** — B++ `malloc` maps to `bpp_alloc` (bump allocator
  in mem_base). External allocators like `MemAlloc` return incompatible pointers.

### Validation

- `snake_v3.bpp`: Compiles and runs via `./bpp snake_v3.bpp`
- Self-hosting: Fixed point still holds (v2 == v3) after all changes
- Import resolver: Handles recursive imports (snake_v3 → stbgame → stbinput → ...)

### Next steps

1. ARM64 native codegen (eliminate clang dependency)
2. UI Phase 2 (closures, declarative blocks)
3. Backend x86_64

---

---


## 2026-03-22 — Bitwise Operators + Dispatch Hints

### Milestone

B++ now has native bitwise operators and programmer dispatch hints.
The language is no longer limited to arithmetic-only bit manipulation.

### Bitwise operators added

Six new operators follow C precedence (highest to lowest):

| Operator | Precedence | Meaning |
|----------|-----------|---------|
| `<<` `>>` | 8 | Left/right shift |
| `&` | 5 | Bitwise AND |
| `^` | 4 | Bitwise XOR |
| `|` | 3 | Bitwise OR |
| `&&` | 2 | Logical AND (now exposed in parser) |
| `||` | 1 | Logical OR (unchanged) |

**Breaking change**: `|` is now bitwise OR, not logical OR. Use `||` for
logical OR. Existing code is safe because all prior uses of `|` were between
boolean expressions (0 or 1), where bitwise OR gives the same result.

### Dispatch hints

Programmers can now override the automatic dispatch analysis with a hint:

```
@gpu while (i < n) { ... }
@par while (i < n) { ... }
@seq while (i < n) { ... }
```

The hint is stored in the while node's `d` field and flows through the
JSON IR as `"dispatch_hint": "gpu"`. The dispatch engine checks it before
running the automatic analysis. When no hint is present, behavior is
unchanged.

### Files changed

| File | Change |
|------|--------|
| `bootstrap/lexer.bpp` | Added `^` (94) to `is_op`, `@` (64) to `is_punct`, `>>` and `<<` to `scan_op` |
| `bootstrap/parser.bpp` | New precedence table with all operators, `@hint` before `while`, `dispatch_hint` in JSON |
| `bootstrap/emitter.bpp` | Removed `\|` → `\|\|` conversion, skip `dispatch_hint` in while node |
| `bootstrap/dispatch.bpp` | Check hint code before automatic analysis |
| `bootstrap/defs.bpp` | Updated WHILE node layout docs |
| `bpp_compiler.py` | OP regex, PREC table, `\|` → `\|\|` removed, dispatch hint parsing |
| `build_lexer.py` | Removed `\|` → `\|\|` |
| `build_parser.py` | Removed `\|` → `\|\|` |
| `build_emitter.py` | Removed `\|` → `\|\|` |
| `docs/bpp.md` | Updated Operators section, added Dispatch Hints section |

### Validation

- `snake_v3.bpp`: Compiles through Python compiler (981 lines C, no errors)
- `lexer.bpp`: Compiles (515 lines C), `^` operator recognized
- `parser.bpp`: Compiles (1546 lines C), dispatch hint code emitted
- `emitter.bpp`: Compiles (1733 lines C), zero `||` in output (all `|` now bitwise)
- Bitwise test: `x = 0xFF; x = x >> 4; return x | 3;` → emits `(x >> 4)` and `(x | 3)`
- Dispatch hint test: `@gpu while(...)` → IR contains `"dispatch_hint": "gpu"`

---

## 2026-03-22 — Real Pointers + Float Inference

### Real pointers

Replaced the simulated memory model (`mem_base[1MB]` + bump allocator) with
real pointer operations across the entire compiler. `malloc` now returns real
addresses, `peek`/`poke` dereference directly, `mem_load`/`mem_store` use
`uintptr_t` casts. Removed `mem_base[]`, `bpp_alloc()`, `BPP_STR` macro.

Files changed: `bpp_compiler.py`, `bootstrap/emitter.bpp`,
`bootstrap/codegen_arm64.bpp`, all `build_*.py` scripts.

This is a simplification — fewer instructions per operation, no more
dual-pointer detection in `str_peek`, and B++ can now work with real system
memory and FFI pointers naturally.

### ARM64 codegen

- Built `build_codegen.py` with comprehensive test (strings, variables, loops,
  functions, arithmetic, memory)
- Added string literal emission (`.asciz` in `__TEXT,__cstring`, loaded via
  `adrp`/`add`)
- Fixed array argument offsets (`*(arr + 8)` not `*(arr + 1)` — values are
  8-byte words)
- Real pointers: `peek` → `ldrb w0, [x0]`, `poke` → `strb w1, [x0]`,
  `mem_load` → `ldr x0, [x0]`, `malloc` → `bl _malloc`
- Removed `_mem_base` and `__bpp_heap` BSS/data sections

### Float inference (Python compiler)

Added native float support with type inference to `bpp_compiler.py`:

- **Lexer**: `FLOAT` token for `\d+\.\d+` patterns
- **Parser**: `TK_FLOAT` literal type
- **Type system**: `TY_FLOAT = 6`
- **Inference engine** (`_infer_all_floats`): fixed-point iteration with:
  - Local variable inference (assigned from float literal/expression → double)
  - Cross-function return propagation (calling float-returning function → double)
  - Call-site parameter propagation (float arg → callee param becomes double)
  - Integer-op guard: params used with `%`, `&`, `|`, `^`, `>>`, `<<` stay
    `long long` even if called with float args (prevents `double % int` errors)
- **FFI**: `float` and `double` types added to FFI maps

Example — this now compiles and runs correctly:
```
lerp(a, b, t) { return a + (b - a) * t; }
main() { auto w; w = lerp(0.0, 100.0, 0.75); print_int(w); }
// Output: 75
```

The inference correctly determines: `lerp` params are `double` (float args at
call site, no integer-only ops), returns `double`, and `w` in main is `double`
(receives float return value).

### Float — what's left

| Component | Status | Notes |
|-----------|--------|-------|
| Lexer (Python + B++) | DONE | FLOAT token |
| Parser (Python + B++) | DONE | TK_FLOAT literal |
| Inference (Python) | DONE | Cross-function, int-op guard |
| emitter.bpp | TODO | Emit `double` declarations in generated C |
| codegen_arm64.bpp | TODO | d0-d7 registers, fadd/fsub/fmul/fdiv, scvtf/fcvtzs |
| build_*.py | Low priority | Bootstrap code doesn't use float |

### Architecture note

The ARM64 codegen is entirely integer-based — everything goes through x0, no
type metadata. Float support will be **additive** (parallel d0 path), not a
refactor of existing integer code. The JSON IR already carries float literals
as `"val": "3.14"` which the codegen can detect.

---

---

## 2026-03-22 — Phase 1 Complete + Phase 2 In Progress: Unified Compiler

### Phase 1 complete

- Renamed all stb modules from `.bpp` to `.bsm` (B-Single-Module)
- Added char literal support (`'A'` → 65, `'\n'` → 10) in lexer, parser, and Python compiler
- Updated all import strings across the codebase

### Phase 2: Unified compiler — architecture

Eliminated JSON glue between pipeline stages. The unified compiler (`bpp.bpp`)
is a thin orchestrator importing 7 `.bsm` modules:

```
bpp.bpp (orchestrator, ~50 lines)
  ├── defs.bpp           — shared constants, Node struct
  ├── bpp_internal.bsm   — shared utilities (buf_eq, make_node, list ops, out)
  ├── bpp_import.bsm     — import resolver (output to memory buffer, not stdout)
  ├── bpp_lexer.bsm      — lexer (reads buffer, writes token arrays)
  ├── bpp_parser.bsm     — parser (no JSON output, builds AST in memory)
  ├── bpp_types.bsm      — type inference (walks AST, annotates itype fields)
  ├── bpp_dispatch.bsm   — dispatch analysis (walks AST, annotates dispatch fields)
  └── bpp_emitter.bsm    — C emitter (reads AST directly, no JSON input)
```

Pipeline in memory: `source → import resolver → outbuf → lexer → toks/vbuf →
parser → AST (funcs/externs/globals) → types → dispatch → emitter → stdout`.

### What was built

| Component | File | Lines | Status |
|-----------|------|-------|--------|
| Orchestrator | `bootstrap/bpp.bpp` | ~50 | Done |
| Shared utils | `bootstrap/bpp_internal.bsm` | ~90 | Done |
| Import resolver | `bootstrap/bpp_import.bsm` | ~280 | Done — emit_ch/emit_str to outbuf |
| Lexer | `bootstrap/bpp_lexer.bsm` | ~340 | Done — add_token() to toks array |
| Parser | `bootstrap/bpp_parser.bsm` | ~800 | Done — no JSON, no read_tokens |
| Type inference | `bootstrap/bpp_types.bsm` | ~330 | Done — prefixed: ty_set_var_type etc. |
| Dispatch analysis | `bootstrap/bpp_dispatch.bsm` | ~430 | Done — prefixed: dsp_add_local etc. |
| C emitter | `bootstrap/bpp_emitter.bsm` | ~600 | Done — bridge_data + emit_all |

### What was deleted

- ~250 lines of JSON output from parser (emit_jp, jot, jkp, jopen, emit_node, etc.)
- ~300 lines of JSON input from emitter (jnext, jskip, jexpect, jread_str, parse_node, etc.)
- Duplicate definitions across stages (struct Node, constants, pack/unpack, buf_eq, etc.)

### Name collision resolution

Functions duplicated across stages were consolidated:
- `buf_eq`, `print_str`, `out`, `make_node`, `list_begin/push/end` → `bpp_internal.bsm`
- `set_var_type` (parser) vs `set_var_type` (types) → types prefixed with `ty_`
- `local_vars` (dispatch) → prefixed with `dsp_`
- Emitter reads parser's data via `bridge_data()` which copies 64-byte records
  into emitter's parallel arrays

### Python compiler fixes for unified compilation

1. **Struct propagation to sub-parsers** — `.bsm` modules parsed independently
   didn't inherit struct definitions. `auto n: Node; n.ntype` caused parser sync
   loss. Fixed: `parent_structs` parameter in `BPlusPlusParser.__init__()`.

2. **Missing built-in functions** — `sys_open`, `sys_close`, `sys_read`,
   `sys_write`, `argv_get`, `argc_get` only existed in bootstrap emitter.
   Added to Python compiler's `_emit_node` + C runtime section.

3. **macOS syscall carry flag** — Added `csneg x0, x0, x0, cc` to all syscall
   wrappers in Python compiler's runtime output.

### Current blocker

The generated C (3827 lines) has 20 errors: string literals passed to
`long long` parameters without cast. Example:

```c
// Generated:
if (buf_eq((src + start), "auto", 4)) {
// Needed:
if (buf_eq((src + start), (long long)"auto", 4)) {
```

This affects `buf_eq`, `val_is`, `emit_str` calls throughout the compiler
modules. The fix is in `bpp_compiler.py`'s `_emit_node` — the `t == "lit"`
branch needs `(long long)` cast for string literals in all expression contexts.

### What remains after the fix

1. Compile: `python3 bpp_compiler.py --emit bootstrap/bpp.bpp > /tmp/bpp_unified.c`
2. Build: `clang /tmp/bpp_unified.c -o /tmp/bpp_unified`
3. Test on simple programs
4. Compare output vs old 4-stage pipeline
5. Fixed-point self-hosting: `./bpp_unified bootstrap/bpp.bpp > bpp_v2.c`

---

---


## 2026-03-23 — B++ Is Born: Zero-Dependency Native Compiler

### Milestone

B++ compiles itself to native ARM64 Mach-O executables with zero external
tools. No assembler, no linker, no C compiler, no codesign. One command:

```
bpp source.bpp -o game
./game
```

The compiler is installed at `/usr/local/bin/bpp`. Self-hosting verified
at every stage: C backend fixed-point, native backend fixed-point (3
generations bit-identical).

### What was built

| Feature | Description |
|---------|-------------|
| String literal relocation fix | Strings now written to __DATA with per-string offsets |
| stb library decoupled from raylib | All 7 modules pure B++, driver pattern (`_drv_*`) |
| `drv_raylib.bsm` | Raylib backend (17 functions), imported by the game |
| Function pointers | `fn_ptr(name)` + `call(fp, args...)`, BLR instruction |
| FFI (Foreign Function Interface) | GOT + chained fixups, calls any dylib function natively |
| `-o` output flag | `bpp source.bpp -o binary` (like `cc -o`) |
| Default mode = native binary | No `--bin` needed, `--c` for C output |
| Native ad-hoc codesign | SHA-256 in pure B++, SuperBlob + CodeDirectory |
| `~` bitwise NOT operator | Lexer + parser + codegen (MVN instruction) |
| `free` / `realloc` built-ins | Memory management for dynamic data structures |
| `sys_fork/execve/waitpid/exit` | Process control syscalls |
| `stbarray.bsm` | Dynamic arrays with shadow header (stb_ds.h technique) |
| `stbstr.bsm` string builder | `sb_new/sb_cat/sb_ch/sb_int/sb_free` |
| Compiler uses stbarray | `globals`, `externs`, `funcs` migrated to `arr_push` |
| Repo reorganization | `src/`, `tests/`, `build/`, `stb/`, `examples/` |
| `bpp-mode.el` | Emacs mode: syntax highlight, indentation, compile commands |
| Treemacs icons | Custom icons for `.bpp` and `.bsm` files |
| Char literal refactor | Magic numbers → char literals in lexer and parser |

### Bugs fixed

| # | Bug | Cause | Fix |
|---|-----|-------|-----|
| 21 | String literal output garbled | Strings never written to __DATA, all relocs same addr | Added `mo_sdata` buffer with per-string offsets |
| 22 | argc/argv point to same slot | `mo_packed_eq(-1, -2)` returned true (both length=0) | Early return for negative sentinel values |
| 23 | `~` operator missing | Not in lexer's `is_op`, not in parser's `parse_expr` | Added to lexer, parser, codegen (MVN) |
| 24 | Native compiler crash (600 funcs) | Codegen buffers 4096 bytes = 512 entries, overflowed | Increased to 65536 bytes |

### Architecture

The compiler is now a single pipeline with 11 modules:

```
source.bpp → import → lex → parse → types → dispatch
  → codegen_arm64 → enc_arm64 → macho (+ SHA-256 codesign) → binary
```

Native binaries link with dylibs via GOT (chained fixups). The stb
library is pure B++. Games choose their backend by importing a driver
module (e.g., `drv_raylib.bsm`).

**Update (2026-03-24):** The driver pattern (`_drv_*`) was replaced by
the platform layer architecture. stb is now a complete game engine — all
drawing is done on a software framebuffer in pure B++, and the platform
layer handles presenting it to the screen. See the 2026-03-24 entry for
details. The snake game now compiles to native (Cocoa), SDL2, and raylib
backends with identical game code.

### Test results

- Self-hosting: C fixed-point ✓, native fixed-point ✓
- snake_v3: compiles and runs natively with raylib ✓
- snake_native: runs natively with Cocoa, zero dependencies ✓ (2026-03-24)
- snake_sdl: runs with SDL2 backend ✓ (2026-03-24)
- snake_raylib: runs with raylib backend (new stb API) ✓ (2026-03-24)
- stbarray: 15 tests pass ✓
- stbstr: 17 tests pass ✓
- SHA-256: matches OpenSSL output for "hello" ✓
- FFI: Cocoa (objc_msgSend) + CoreGraphics (dlsym) + SDL2 + raylib ✓

---

---

## 2026-03-23 — Phase 5: Native Mach-O ARM64 Binary (Almost Born)

### Milestone

B++ can now emit native Mach-O ARM64 executables directly — no assembler, no
linker, no clang. The `--bin` flag triggers the full native pipeline:

```
./bpp --bin source.bpp
```

This produces `a.out`, a signed Mach-O executable that runs on macOS ARM64.
All test programs pass. The compiler itself compiles and runs — but its C
output is garbled due to one remaining bug in string literal relocation.

### Architecture

Three new modules complete the native pipeline:

| Module | Purpose | Lines |
|--------|---------|-------|
| `bpp_codegen_arm64.bsm` | ARM64 code generator (from AST, not JSON) | ~700 |
| `bpp_enc_arm64.bsm` | Binary instruction encoder (4-byte ARM64 opcodes) | ~400 |
| `bpp_macho.bsm` | Mach-O writer (headers, segments, codesign) | ~700 |

Pipeline: `source → import → lex → parse → types → dispatch → codegen_arm64 →
enc_arm64 (code buffer + relocations) → macho (Mach-O binary + codesign)`.

### What was built

**Binary instruction encoding** (`bpp_enc_arm64.bsm`):
- Every ARM64 instruction encoded as a 4-byte value via `enc_emit32()`
- Relocation table for forward references: ADRP/ADD pairs patched after layout
- Three relocation types: 0 = global variable, 1 = string literal, 2 = float literal
- Label system with backpatching for branches (B, CBZ, CBNZ, B.cond)
- Full floating-point encoding: FADD, FSUB, FMUL, FDIV, FCMP, SCVTF, FCVTZS, FMOV

**Mach-O writer** (`bpp_macho.bsm`):
- Complete Mach-O header with magic, CPU type, file type
- Load commands: LC_SEGMENT_64 (__PAGEZERO, __TEXT, __DATA, __LINKEDIT), LC_MAIN
- Page-aligned segments (4096-byte boundaries)
- __TEXT,__text: code bytes from encoder
- __DATA: globals (8 bytes each), float literals (8 bytes each), string data
- Relocation resolution: patches ADRP/ADD instruction pairs with correct page offsets
- Ad-hoc code signing via external `codesign -s -` (called via `sys_exec`)
- IEEE 754 float encoding (`mo_atof`) implemented as pure integer arithmetic
- `mo_packed_eq` for comparing packed string references by content

**Codegen integration** (`bpp_codegen_arm64.bsm`):
- Adapted from standalone `codegen_arm64.bpp` (~1400 lines → ~700 lines)
- Removed JSON reader, list builder, type analysis (all handled by other modules)
- Added `a64_fn_fidx` array mapping emitter function index → parser function index
- Type queries use `get_fn_var_type`, `get_fn_ret_type`, `get_fn_par_type` from bpp_types.bsm
- Global variable deduplication in `bridge_data_arm64()`
- argc/argv implemented via hidden globals (-1, -2 sentinel packed values)
- main() prologue saves dyld's x0/x1 (argc/argv) to hidden globals

**Bump allocator** (emitted as runtime in every native binary):
- `mmap` syscall (197) allocates 16MB heap
- `malloc(size)` = bump pointer, 8-byte aligned
- No `free()` — sufficient for compiler and most B++ programs

### Bugs fixed

16. **Type system index mismatch** — Both emitters (C and ARM64) deduplicate
    functions by name, creating `fn_cnt < func_cnt`. But type queries
    (`get_fn_ret_type`, `get_fn_par_type`) use the parser's raw index. Fixed
    by adding `fn_fidx` / `a64_fn_fidx` mapping arrays that translate emitter
    index → parser index before every type query.

17. **IEEE 754 float encoding** — The C backend's `*(long long*)(ptr) = bits`
    where `bits` is a `double` truncates instead of bit-preserving (2.0 → 2
    instead of 0x4000000000000000). Rewrote `mo_atof` as pure integer
    computation: parse integer + fractional digits as rational, find exponent
    via successive halving/doubling, extract 52 mantissa bits, assemble
    sign|exponent|mantissa. No float arithmetic anywhere.

18. **argc/argv in native mode** — Native binaries couldn't read command-line
    arguments. macOS ARM64 with LC_MAIN receives argc in x0 and argv in x1
    from dyld. Fixed by adding hidden globals (`a64_argc_sym`, `a64_argv_sym`
    with sentinel packed values -1, -2) and saving x0/x1 in main()'s prologue.
    `argc_get()` and `argv_get(i)` load from these globals via ADRP+LDR.

19. **Global variable deduplication** — The parser adds the same global name
    multiple times from different imported modules, each with a different
    packed value (different vbuf offset). In binary mode, each gets a separate
    __DATA slot. `init_import()` writes to one slot, `main()` reads from
    another → NULL pathbuf. Fixed by deduplicating globals in
    `bridge_data_arm64()` using string-content comparison, and using
    `mo_packed_eq` in the relocation resolver.

20. **defs.bpp → defs.bsm** — `defs.bpp` has no `main()` function (it's a
    module of constants and struct definitions). Renamed to `.bsm` for
    consistency with the module system.

### Test results (all passing)

| Test | Expected | Result |
|------|----------|--------|
| test_exit42 | exit=42 | exit=42 |
| test_arith | exit=42 | exit=42 |
| test_hello | "Hello, B++!" | "Hello, B++!" |
| test_global | exit=42 | exit=42 |
| test_fib | exit=55 | exit=55 |
| test_malloc | exit=42 | exit=42 |
| test_float | "6" | "6" |
| test_float2 | "314" | "314" |
| test_float3 | "13 7 33 3 75" | "13 7 33 3 75" |
| test_argv | "hello" exit=2 | "hello" exit=2 |
| test_native_debug | all inits | "123456789ABC" |

### Blocking bug — string literal output garbled (**RESOLVED**)

The native compiler's C output was garbled — type-1 relocations (string
literals) resolved to wrong addresses in `mo_resolve_relocations`.

**Fixed** in the same session by adding `mo_sdata` buffer with per-string
offsets. See bug #21 in the 2026-03-23 entry above.

### What remained for B++ to be born (**ALL RESOLVED**)

1. ~~Fix string literal relocation~~ — ✓ Fixed (bug #21)
2. ~~Self-compilation~~ — ✓ Fixed-point verified (2026-03-23)
3. ~~Fixed-point verification~~ — ✓ Native compiler compiles itself (2026-03-23)
4. ~~Cleanup~~ — ✓ Done
5. ~~Ad-hoc codesign in B++~~ — ✓ Implemented in pure B++ SHA-256 (2026-03-23)

---


## 2026-03-24 — stb Native: Games Without External Libraries

### Milestone

B++ games run natively on macOS without SDL, raylib, or any external
game library. The stb standard library is the game engine — all drawing,
input, and timing is pure B++. The compiler's platform layer handles OS
communication via Cocoa/CoreGraphics.

Three backends work with identical game code:
- **Native** (Cocoa) — zero dependencies
- **SDL2** — texture upload via SDL_UpdateTexture
- **Raylib** — DrawRectangle per pixel

### What was built

| Feature | Description |
|---------|-------------|
| `var` keyword | Stack-allocated structs: `var v: Vec2;` (no malloc) |
| `enum` | Named constants: `enum Dir { UP, DOWN, LEFT, RIGHT }` |
| `break` | Exit innermost while loop, nested support |
| `stbbuf.bsm` | Raw buffer access: read/write u8/u16/u32/u64 |
| `stbfile.bsm` | File I/O: `file_read_all(path, size_out)` |
| `stbfont.bsm` | 8x8 bitmap font (A-Z, a-z, 0-9, punctuation) |
| `stbdraw.bsm` rewrite | Software framebuffer rendering (pure B++) |
| `stbinput.bsm` rewrite | Memory-based input with Key enum |
| `stbgame.bsm` rewrite | Game loop via platform built-ins |
| `_stb_platform_macos.bsm` | Cocoa native: dlopen AppKit, objc_msgSend, CGBitmapContext |
| `drv_sdl.bsm` | SDL2 backend: texture upload, letterbox scaling |
| `drv_raylib.bsm` | Raylib backend: rlgl texture or DrawRectangle |
| FFI ARM64 ABI fix | Separate int/float register counters |
| FFI overload by argc | Multiple declarations of same extern name |
| FFI float return types | Type system aware of extern "double" returns |
| Global float variables | Load/store globals as float across functions |
| Target import resolver | `foo.bsm` → auto-tries `foo_<target>.bsm` |
| Import path: `drivers/` | Drivers resolved from drivers/ directory |
| Import path: global | `/usr/local/lib/bpp/stb/` for installed modules |
| MCU game port | Full game (rat vs cat) running on all 3 backends |

### Examples

| Example | Backend | Description |
|---------|---------|-------------|
| `hello.bpp` | raylib FFI | Simple colored window |
| `hello_native.bpp` | Native | Hello world with stb |
| `snake_native.bpp` | Native | Snake game, zero dependencies |
| `snake_sdl.bpp` | SDL2 | Same snake, SDL backend |
| `snake_raylib.bpp` | Raylib | Same snake, raylib backend |
| `sdl_demo.bpp` | SDL2 | Drawing, input, text demo |
| `raylib_demo.bpp` | raylib FFI | Bouncing cubes |
| `raylib_demo2.bpp` | raylib FFI | Multi-cube physics |

### Architecture

```
Game code (dev writes)
    │
    ▼
stb (pure B++: stbdraw, stbinput, stbgame, stbfont)
    │
    ▼
Platform layer (_stb_init_window, _stb_present, _stb_poll_events)
    │
    ▼
Backend (auto-selected by target or overridden by driver import)
    │
    ├── Native: Cocoa + CoreGraphics (macOS, zero deps)
    ├── SDL2: drv_sdl.bsm (texture upload)
    └── Raylib: drv_raylib.bsm (DrawRectangle)
```

### Bugs fixed

| # | Bug | Cause | Fix |
|---|-----|-------|-----|
| 25 | TY_STRUCT crashes binary | Global not declared with `auto` in defs.bsm | Add `auto TY_STRUCT` declaration |
| 26 | FFI float args in wrong regs | Single counter for int+float | Separate int_idx/flt_idx counters |
| 27 | FFI mixed arg positions | Int args not compacted after float removal | Compact ints: `mov x[int_idx], x[i]` |
| 28 | Global float lost across functions | Global load/store always used integer regs | Track global types, use ldr d0/str d0 |
| 29 | stbio + SDL crash | String literal conflict with many externals | Workaround: avoid stbio before SDL calls |
| 30 | mach_absolute_time wrong | Missing timebase conversion | mach_timebase_info numer/denom |
| 31 | CoreGraphics colors wrong | bitmapInfo 8198 ignores alpha byte | Changed to 8194 (AlphaPremultipliedFirst) |

### Codebase size

| Component | Lines of B++ |
|-----------|-------------|
| Compiler (12 modules) | ~8,000 |
| stb library (11 modules) | ~1,800 |
| Platform layer (macOS) | ~200 |
| Drivers (SDL2 + raylib) | ~200 |
| **Total** | **~10,000** |

A self-hosting compiler + game engine + native platform layer in 10K lines of a minimalist language.

### Pending (RESOLVED 2026-03-24)

- ~~**stbcol.bsm** — Not yet tested or integrated~~ → Tested (24/24) and integrated into MCU. See 2026-03-24 entry.

---

## 2026-03-24 — Compiler Diagnostics + x86_64 Backend + stbcol Integration

### stbcol.bsm — Tested and Integrated

- `tests/test_col.bpp`: 24 tests covering rect_overlap, circle_overlap, rect_contains, circle_contains. All pass.
- `tests/test_col_float.bpp`: Float argument test — rect_overlap works with double params via type inference.
- MCU game (`/Users/Codes/mcu-bpp/src/main.bpp`): integrated stbcol, `check_overlap()` now calls `rect_overlap()`.
- **MCU crash root cause**: missing `import "_stb_platform.bsm"`. The platform functions (`_stb_init_window`, `_stb_present`, etc.) were called but never defined — the compiler silently generated broken GOT relocations. Fixed by adding the import.

### Compiler Diagnostics — Teaching Error Messages

New architecture: semantic errors live in `bpp_validate.bsm`, platform-agnostic. Codegen only has internal panics for "should never happen after validate" cases.

```
import → lex → parse → types → dispatch → VALIDATE → codegen → macho
                                              │
                                     semantic errors here
                                     platform-agnostic
```

**New modules:**
- `src/bpp_diag.bsm` — Diagnostic output infrastructure. All messages go to stderr (fd=2). Functions: `diag_fatal(code)`, `diag_warn(code)`, `diag_str()`, `diag_int()`, `diag_packed()`, `diag_loc()`, `diag_summary()`.
- `src/bpp_validate.bsm` — Semantic validation pass. Walks all function bodies after type inference, checks every T_CALL has a target (in funcs[], externs[], or builtins). Reports E201 with function name and suggestion.

**Error catalog implemented:**

| Code | Stage | Message |
|------|-------|---------|
| E001 | import | `cannot open file '{path}'` |
| E002 | import | `import '{name}' not found (searched: ./, stb/, /usr/local/lib/bpp/stb/)` |
| E101 | parser | `unknown type '{name}' -- define it with 'struct'` |
| E102 | parser | `struct has no field '{field}'` |
| E103 | parser | `sizeof applied to unknown type '{name}'` |
| E104 | parser | `unexpected token in expression` |
| E201 | validate | `function '{name}' called but never defined -- missing import?` |
| W001 | lexer | `unterminated string literal at line N` |

**Line number tracking**: lexer counts newlines in `skip_ws()`, stores line per token in `tok_lines` array. File boundary tracking in import resolver maps tokens to source files.

**Validate immediately proved its value**: caught the `print_msg` bug in `bpp_codegen_arm64.bsm` — a function called but never defined (from stbio.bsm, which the compiler doesn't import). Fixed by replacing with `diag_str` + `sys_exit(2)`.

### ARM64 Encoder — Dynamic Arrays (Buffer Overflow Fix)

**Root cause of compiler hang**: `enc_lbl_off` was a fixed 32KB buffer (4096 labels max). With 456+ functions (after adding x86 modules), label IDs exceeded 4096, silently corrupting heap memory. The compiler hung or produced broken binaries.

**Fix**: Migrated all 7 encoder arrays from `malloc` fixed buffers to `arr_push`/`arr_get`/`arr_set` dynamic arrays (stbarray):

| Array | Before | After |
|-------|--------|-------|
| `enc_lbl_off` | `malloc(32768)` — 4096 labels max | `arr_new()` — unlimited |
| `enc_fix_pos/lbl/ty` | `malloc(65536)` — 8192 fixups max | `arr_new()` — unlimited |
| `enc_rel_pos/sym/ty` | `malloc(32768)` — 4096 relocs max | `arr_new()` — unlimited |

Added `arr_clear()` calls at codegen start to reset arrays between compilations.

### T_BREAK Declaration Fix

`T_BREAK` was assigned (`T_BREAK = 13` in `init_defs()`) but never declared with `auto` at module scope. This meant it was a local variable inside `init_defs`, invisible to other modules. The emitter/codegen checked `if (t == T_BREAK)` against an uninitialized global (always 0).

Fixed: added `T_BREAK` to the `auto` declaration in `defs.bsm`. The Mach-O internal panic immediately caught this: `internal error: global 'T_BREAK' not found in data section`.

### x86_64 Cross-Compilation Backend (Linux ELF)

Three new modules for cross-compiling B++ to Linux x86_64 static ELF binaries:

| File | Lines | Purpose |
|------|-------|---------|
| `src/bpp_enc_x86_64.bsm` | ~856 | x86_64 instruction encoder (prefix `x64_enc_`) |
| `src/bpp_codegen_x86_64.bsm` | ~1220 | x86_64 code generator (Linux syscalls) |
| `src/bpp_elf.bsm` | ~389 | ELF writer (static binary, 2 PT_LOAD segments) |

New flag: `bpp --linux64 source.bpp -o out` (mode=3).

**Status**: Code written, not yet tested on actual Linux. Docker container `bpp-linux` (Ubuntu 24.04) prepared for testing. Blocked by the encoder buffer overflow (now fixed with dynamic arrays).

### Bugs Fixed

| # | Bug | Cause | Fix |
|---|-----|-------|-----|
| 32 | MCU segfault | Missing `import "_stb_platform.bsm"` — GOT relocations broken | Added import to MCU game |
| 33 | Compiler hang with 456+ functions | `enc_lbl_off` buffer overflow (4096 labels max) | Dynamic arrays via stbarray |
| 34 | `T_BREAK` invisible to other modules | Missing `auto` declaration in defs.bsm | Added to auto line |
| 35 | `print_msg` in codegen undefined | Called stbio function the compiler doesn't import | Replaced with `diag_str` + `sys_exit(2)` |
| 36 | `diag_buf(ptr, len)` — `ptr` is a keyword | B++ treats `ptr` as type keyword, not parameter | Renamed to `diag_buf(addr, len)` |

### macOS Quarantine Issue (UNRESOLVED)

macOS Sequoia applies `com.apple.provenance` extended attribute to all files written by sandboxed processes (Claude Code runs sandboxed). Native binaries with this attribute get SIGKILL when executed from the repo directory.

**Does NOT work**: `xattr -d`, `DevToolsSecurity -enable`, `dangerouslyDisableSandbox` flag.
**Works**: `xattr -cr ./bpp && codesign -f -s - ./bpp` from user's terminal, or build to `/tmp` and copy.
**Pending**: `claude config set sandbox.enabled false` + reboot.

### Codebase Size

| Component | Lines of B++ |
|-----------|-------------|
| Compiler (15 modules) | ~10,900 |
| stb library (13 modules + platform) | ~2,000 |
| Drivers (SDL2 + raylib) | ~200 |
| **Total** | **~13,100** |

### Architecture After This Session

```
bpp.bpp (orchestrator)
  ├── defs.bsm            — constants, Node struct
  ├── stbarray.bsm        — dynamic arrays (compiler dependency)
  ├── bpp_internal.bsm    — shared utilities
  ├── bpp_diag.bsm        — NEW: diagnostic output (stderr)
  ├── bpp_import.bsm      — import resolver + E001/E002
  ├── bpp_parser.bsm      — parser + E101-E104
  ├── bpp_lexer.bsm       — lexer + line tracking + W001
  ├── bpp_types.bsm       — type inference
  ├── bpp_dispatch.bsm    — loop analysis
  ├── bpp_validate.bsm    — NEW: semantic validation (E201)
  ├── bpp_emitter.bsm     — C backend
  ├── bpp_enc_arm64.bsm   — ARM64 encoder (dynamic arrays)
  ├── bpp_codegen_arm64.bsm — ARM64 codegen
  ├── bpp_macho.bsm       — Mach-O writer (internal panics)
  ├── bpp_enc_x86_64.bsm  — NEW: x86_64 encoder
  ├── bpp_codegen_x86_64.bsm — NEW: x86_64 codegen
  └── bpp_elf.bsm         — NEW: ELF writer
```

---


## 2026-03-25 — Linux x86_64 Port: First Binary to Snake Game

### x86_64 Cross-Compilation Working

The B++ compiler now cross-compiles from macOS ARM64 to Linux x86_64 ELF binaries. The full pipeline works: `./bpp --linux64 source.bpp -o binary` produces a static ELF that runs in a Docker Ubuntu x86_64 container.

### Bugs Found and Fixed During Port

**Float addition emitting 3x addsd**: Draft code was left in `bpp_codegen_x86_64.bsm` — three `addsd` instructions where one was needed. Every float add was tripled (1.0+48 gave 51+1+1=53 instead of 49). Removed the draft lines.

**elf_packed_eq missing sentinel comparison**: The argc/argv globals use sentinel values (-1, -2) as packed names. The Mach-O writer's `mo_packed_eq` had `if (a == b) return 1` for direct comparison, but the ELF writer's `elf_packed_eq` didn't — causing it to unpack -1 as a string with length 0xFFFFFFFF. Added the same guards.

**Type propagation poisoning callee params**: `propagate_call_params()` permanently mutated callee parameter types. When `print_int(whole)` was called with a float, it marked `print_int`'s `n` as TY_FLOAT globally. Removed propagation entirely (Phase A fix — see separate entry below).

**x86_64 call-site type awareness**: The ARM64 codegen consulted `get_fn_par_type()` at call sites to convert float↔int correctly. The x86_64 codegen was blind — just did `MOVQ` for all floats. Replicated the ARM64 logic: check callee's expected type, emit CVTTSD2SI or CVTSI2SD+MOVQ as needed.

### Platform Layer Auto-Routing

Added automatic platform selection via `--linux64` flag: sets `_imp_target = "linux"`, so `import "_stb_platform.bsm"` resolves to `_stb_platform_linux.bsm` automatically. Same mechanism already works for macOS.

Created `stb/_stb_platform_linux.bsm` — terminal-based platform layer:
- Renders framebuffer as ANSI 256-color half-block characters
- Raw terminal mode via `sys_ioctl` (TCGETS/TCSETS)
- Non-blocking keyboard input (arrow keys + WASD)
- Timing via `sys_clock_gettime(CLOCK_MONOTONIC)`
- Frame rate cap via `sys_nanosleep`

### New Syscalls

Added to x86_64 codegen + validator: `sys_ioctl` (16), `sys_nanosleep` (35), `sys_clock_gettime` (228).

### Tests Passing

| Test | Result |
|------|--------|
| exit(42) | 42 |
| hello world (putchar) | Hello, Linux x86_64! |
| full suite (strings, recursion, malloc, loops, if/else, function calls) | All pass |
| floats (arithmetic, comparison, truncation) | 3.00, 6.00, OK |
| file I/O (open, write, read, close) | wrote 13, read back OK |
| argc/argv | argc=4, all args correct |
| **Snake game** | Compiles (45KB static ELF), renders frames in terminal |

### Design Decision: Two Calling Conventions

Internal B++ calls use GPR-uniform convention (all args in RDI/RSI/etc., floats bit-transferred via MOVQ). External FFI calls (future) will use System V ABI with floats in XMM registers. Cost: 1 MOVQ per float arg per call (~free on modern CPUs).

---

## 2026-03-25 — Type Propagation Bug Fix + Evolution Roadmap

### Bug Fix: Type Propagation

`propagate_call_params()` in `bpp_types.bsm` was permanently mutating callee parameter types based on one caller's argument types. When `print_int(whole)` was called with a float `whole`, the type system marked `print_int`'s parameter `n` as TY_FLOAT globally — poisoning all other call sites. Crashed on ARM64, wrong output on x86_64.

**Root cause**: Cross-function type propagation was one-directional and permanent. The guard (`uses_int_ops_list`) only caught `%`, `&`, `|`, `^`, `<<`, `>>` as int-only evidence — operations like `/` and `>` didn't prevent propagation.

**Fix**: Removed `propagate_call_params()` from the fixed-point loop. Types are now determined solely by body inference. The codegen handles float↔int conversion at each call site independently (ARM64 already did this; x86_64 port replicates the same logic). Bootstrap verified — identical 280994-byte binaries.

**Design decision**: A full "context-based specialization" system (compiler generates `add$FF`/`add$LL` variants automatically) was designed but **paused** — no current B++ code needs it, and it touches the emission loop which is high-risk for bootstrap.

### Evolution Roadmap

Established the three-pillar vision for B++:

1. **ART** — `stbart.bsm` (pixel art primitives, ported from ModuLab JS editor) + sprite/tilemap/level/particle editors
2. **SOUND** — `stbdsp.bsm` (DSP primitives) + `stbplugin.bsm` (CLAP audio plugins, native + FFI driver like SDL)
3. **GAMES** — stbgame + stbtile + stbphys + stbpath

Asset pipeline: `.bspr` → `.btm` → `.blvl` → game binary. Full plan in `docs/todo.md`.

Key blockers identified: mouse events not polled (B0), `mouse_pressed()` undefined (B1), `file_write_all()` missing (B2), SDL key gaps (B3).

x86_64 calling convention decided: two conventions — internal B++ = all GPR (MOVQ for floats), external FFI = System V ABI.

---


## 2026-03-26 — Modular Compilation, Dynamic Arrays, Type Hints, and Backend Reorganization

Two sessions worth of work. The compiler gained dynamic arrays, sub-word type hints, named field offsets, and a full modular compilation pipeline with .bo object files.

### Dynamic Array Migration

87 arrays across 9 compiler modules migrated from fixed-size malloc with manual counters to `arr_new()`/`arr_push()`/`arr_get()`/`arr_set()` (from `stbarray.bsm`). Eliminates all hard-coded limits on structs, enums, variables, functions, and similar entities. Affected modules: `bpp_parser`, `bpp_types`, `bpp_codegen_arm64`, `bpp_codegen_x86_64`, `bpp_enc_arm64`, `bpp_enc_x86_64`, `bpp_macho`, `bpp_elf`, `bpp_dispatch`.

### Type Hints — Sub-Word Types Across All Backends

Real sub-word type support added to all three backends (ARM64, x86_64, C emitter). Syntax: `auto x: byte;`, `fn(x: float)`. Supported types: `byte` (8-bit), `quarter` (16-bit), `half` (32-bit), `int` (64-bit), `float` (64-bit double). ARM64 uses `ldrb`/`strb`/`ldrh`/`strh`/`ldr w`/`str w`. x86_64 mirrors the same narrowing. C emitter maps to `uint8_t`/`uint16_t`/`uint32_t`. Truncation is performed by hardware. Design: sub-word hints are opt-in performance tuning, never auto-inferred.

### Phase 0 — Module Tracking

Added `tok_src_off` in the lexer to track source file boundaries. Parser stores per-entity module ownership via `func_mods[]`/`glob_mods[]`. `fname_buf` provides permanent filename storage. `diag_loc()` uses binary search over source offsets to resolve the correct file and line for diagnostic messages.

### Phase 1 — Hash + Dependency Graph

FNV-1a content hashing per module. Dependency graph via `dep_from[]`/`dep_to[]` arrays. `topo_sort()` implements Kahn's algorithm in reverse order. Hash persistence in `.bpp_cache/hashes`. `propagate_stale()` marks dependents dirty when a source file changes. `--show-deps` flag for inspecting the graph. Also fixed `hex_emit()` for signed values.

### Named Field Offsets

Added named constants for function record fields (`FN_TYPE`/`FN_NAME`/`FN_PARS`/`FN_PCNT`/`FN_BODY`/`FN_BCNT`/`FN_HINTS`) and extern record fields (`EX_TYPE`/`EX_NAME`/`EX_LIB`/`EX_RET`/`EX_ARGS`/`EX_ACNT`) in `defs.bsm`. Refactored ~110 raw numeric offset accesses across 8 files. This eliminates a class of bugs like the n.c/n.d confusion described below.

### Bug Fix: var Struct Flag Collision

The parser set the stack struct flag for `var v: Vec2` in field `n.c` (offset 24), but type hints also stored their value in `n.c`. Since `TY_BYTE = 1` coincided with the struct flag value, every variable declaration was silently treated as byte-typed. All `test_var_*` tests were segfaulting. Fix: moved the struct flag to `n.d` (offset 32).

### Modular Compilation (.bo Files — Go Model)

Full incremental compilation pipeline, modeled after Go's package compilation. Implemented in 8 steps:

- **bpp_bo.bsm**: New module with I/O primitives — `read`/`write` for `u8`/`u16`/`u32`/`u64`, packed string serialization with vbuf re-packing.
- **Export data serialization**: Functions (return type + parameter types + hints), structs, enums, globals, and externs are serialized into `.bo` files.
- **Per-module codegen**: `emit_module_arm64`/`emit_module_x86_64` with accumulative `enc_buf`. Cross-module `BL`/`CALL` instructions use type 4 relocations.
- **Cross-module call resolution**: `bo_resolve_calls` patches `BL`/`CALL` placeholders after all modules are compiled, then removes type 4 entries from the relocation list.
- **Incremental driver**: `--incremental` flag triggers per-module lex/parse/infer/codegen. `.bo` cache stored in `.bpp_cache/`.
- **String/float index remapping**: Resolved issue where string and float constant indices collided across modules. Sentinel handling preserved for argc/argv globals.
- **Extern refresh**: FFI argument rearrangement was empty in the modular path because externs were only loaded once. Fixed by refreshing externs per-module before codegen.

### Module Ownership Tracking

Added `struct_mods[]`/`enum_mods[]`/`extern_mods[]` alongside existing `func_mods[]`/`glob_mods[]`. All entity types now track which source module defined them.

### find_func_idx Backward Scan

Changed `find_func_idx` to scan backward for "last wins" semantics matching codegen behavior. Later reverted — backward scan caused a segfault in gen3 bootstrap due to interaction with type inference. Deferred for future investigation.

### Backend Reorganization

Moved platform-specific files into subdirectories:
- `src/aarch64/`: `a64_enc.bsm`, `a64_codegen.bsm`, `a64_macho.bsm`
- `src/x86_64/`: `x64_enc.bsm`, `x64_codegen.bsm`, `x64_elf.bsm`

All import paths updated throughout the compiler source.

### x86_64 _start Stub

Extracted `x64_emit_start_stub()` so the per-module pipeline can emit `_start` after all modules have been compiled rather than at the start of codegen.

### stb Changes

- `stbdraw`: `draw_number()` changed from global `_num_buf` to local malloc+free.
- `stbui`: Added bounds checks in `ui_alloc()` and `lay_push()`.

### Bugs Found

| Bug | Status |
|-----|--------|
| var struct flag collision (`n.c = 1 = TY_BYTE`) | FIXED — moved flag to `n.d` |
| Extern FFI rearrangement empty in modular path | FIXED — refresh externs per-module |
| Param float inference regression (params in float expressions stay `TY_LONG`, `fcvtzs` destroys values) | WORKAROUND — `: float` annotation. Root cause: `propagate_call_params()` disabled |
| `find_func_idx` backward scan segfault in gen3 bootstrap | REVERTED |
| `ptr` and `len` are reserved words in B++ | Discovered when writing `bpp_bo.bsm` |

### Verification

| Test | Result |
|------|--------|
| Bootstrap (monolithic) | Passes |
| Bootstrap (per-module `--incremental`) | Self-consistent |
| All test programs | Pass (ARM64 + Linux x86_64) |
| Snake game | Compiles and runs on all backends |
| MCU game | Compiles and runs (with `: float` parameter annotations) |

---



## 2026-03-26 — Cache Fix, Code Signature, memcpy/realloc, Import Fixes, Per-Variable Hints, stb Infra

Second session of the day. Recovered from a git push that lost changes. Rebuilt everything from backup.

### Cache Modular Fix (Definitive)

Manifest hash embedded in every `.bo` file header. Three-layer validation: (1) manifest hash — ensures same program/module list, (2) source hash — module content unchanged, (3) dependency hashes — no transitive changes. Version bumped to 2; old v1 `.bo` files auto-rejected. Fast-path in `.bpp_cache/hashes` file skips loading `.bo` files entirely when the program changes. `cache_manifest_hash()` computes FNV-1a of module count + all filenames. Eliminates the cross-program cache corruption that caused segfaults when compiling different programs in sequence.

### Code Signature Fix

Mach-O code signature now matches the system `codesign` tool output: `CS_ADHOC` flag (0x2) without `CS_LINKER_SIGNED`, version 0x20100, 2 special slots (info plist + requirements, zero-filled). Page hashes use SHA-256. Binaries no longer need external `codesign` after compilation.

### memcpy + realloc Builtins

`memcpy(dst, src, len)` implemented in all three backends. ARM64 uses optimized post-index byte loop (cbz skip + ldrb_post/strb_post/subs/cbnz). x86_64 uses test+jz skip + loadb/storeb/sub/jnz loop. C emitter calls libc `memcpy()`. New encoder instructions: `enc_ldrb_post`, `enc_strb_post`, `enc_subs_imm`.

`realloc` now supports both 2-arg `realloc(ptr, size)` (backward compat, just malloc) and 3-arg `realloc(ptr, old_size, new_size)` (allocate + copy). ARM64 3-arg: mmap + byte copy loop. x86_64 3-arg: mremap first, fallback to mmap + copy. C emitter: uses libc realloc (ignores old_size).

### Import System Improvements

Added `src/`, `/usr/local/lib/bpp/`, and `/usr/local/lib/bpp/drivers/` to the import search path. Updated 13 test files with current module names (`defs.bpp` → `defs.bsm`, `bpp_enc_arm64.bsm` → `aarch64/a64_enc.bsm`, etc.) and added missing imports for incomplete tests.

### C Emitter Fixes

Stack struct copy (`b = a;`) now emits `memcpy()` instead of illegal lvalue cast. Added `#include <string.h>` to the C header.

### Per-Variable Type Hints

`auto x: byte, y: quarter;` now correctly assigns individual hints per variable. Parser allocates a hints array stored in T_DECL node field `e` (offset 40). Type inference reads per-variable hints when present, falls back to single hint for backward compat.

### word Keyword Removed

`word` removed from the lexer keyword list — it's now a free identifier. `auto x: word;` still works via `try_type_annotation()`. `byte`, `half`, `quarter` work as standalone declaration keywords (`byte hp;`).

### stb Infrastructure

- `file_write_all(path, buf, len)` in stbfile.bsm
- `blend_px(x, y, src_color)` in stbdraw.bsm — alpha blending with ARGB format
- `mouse_released(btn)` + fixed `mouse_pressed(btn)` with real edge detection via `_stb_mouse_btn_prev`
- Mouse button events (left/right down/up) in Cocoa event loop (_stb_platform_macos.bsm)

### Install Script

`install.sh` — compiles fresh binary to `/tmp`, then `sudo cp` to `/usr/local/bin/bpp` + stb + drivers + defs. Usage: `sh install.sh --skip`.

### Param Float Inference (FAILED — Deferred)

Naive T_BINOP promotion (promote all vars to float when binop result is float) breaks Cocoa apps silently. Variables used in both integer and float contexts (e.g. `head_x`) get wrongly promoted. Reverted. Workaround: explicit `: float` annotation. Needs conservative approach (params-only, check integer usage).

### Verification

| Suite | Result |
|-------|--------|
| Native | 15/15 |
| C Emitter | 15/15 |
| Linux | 6/6 |
| Encoder | 13/13 |
| Examples | 6/6 |
| Snake | Runs |

### Lessons Learned

- NEVER use `git stash`/`checkout`/`restore` on the `bpp` binary — it replaces the bootstrapped compiler with the git fossil
- Always test `snake_native` after type system changes
- `install.sh` must compile to `/tmp` first to bypass sandbox restrictions
- `git checkout` on source files reverts ALL changes, not just targeted ones — don't use for selective reverting

## 2026-03-27 — float_ret Builtins, Go-Style Cache, Type System Fixes, Mouse Tracking

The P0 blocker (mouse stuck at 0,0) is resolved. Three independent bugs were found and fixed, plus a long-standing cache invalidation issue that had been causing inconsistent builds since modular compilation was introduced.

### float_ret() / float_ret2() — Design B

Both ARM64 and x86_64 backends now save float return registers (d0/d1 on ARM64, xmm0/xmm1 on x86_64) to hidden stack slots immediately after every extern function call. Two new builtins read from those slots:

- `float_ret()` — first float return value
- `float_ret2()` — second float return value

Frame layout changed: ARM64 variables start at offset 40 (was 24), x86_64 at -(24+total) (was -(8+total)). The extra 16 bytes per frame hold the saved float registers.

Design B was chosen over Design A (trampoline alias) because it's architecture-agnostic — each backend saves its own registers, no platform-specific function names needed. The pattern works identically across ARM64, x86_64, and future backends (WASM, cc65).

**Pattern:**
```
objc_msgSend(ev, sel_registerName("locationInWindow"));
lx = float_ret();     // d0 saved from last extern call
ly = float_ret2();    // d1 saved from last extern call
```

**Root cause of mouse 0,0:** `objc_msgSend` was declared as returning `ptr`, so the codegen emitted `scvtf d0, x0` (converting garbage integer to float) instead of reading d0 directly. And d1 was volatile between statements. float_ret/float_ret2 solve both problems.

### Go-Style Cache Hash Chain

Replaced `propagate_stale()` with `hash_with_deps()`. Each module's hash now includes the hashes of all its dependencies, computed in topological order. Transitive invalidation is automatic — if C changes, B's combined hash changes, A's combined hash changes. No separate propagation step.

**Before:** `propagate_stale()` walked the dependency graph and manually marked dependents. This had bugs — modules could be missed, producing inconsistent binaries (1958 bytes different from a fresh compile).

**After:** `hash_with_deps()` — 15 lines, correct by construction. Proven: incremental compile produces byte-identical binary to fresh compile.

This was the bug that caused 3 days of cache-related build failures since modular compilation was introduced.

### Type System: Forced Hints Override Inference

`a64_var_is_float()` and `x64_var_is_float()` now check the forced type annotation (`: int`, `: byte`, etc.) before consulting type inference. If the programmer annotated a variable, the annotation wins.

**Before:** `auto x: int; x = 3.14;` would store the raw double bit pattern because inference promoted `x` to TY_FLOAT, ignoring the `: int` hint.

**After:** The `: int` hint forces `fcvtzs` (float→int conversion). The programmer's intent is respected.

### Type System: Global Float Poisoning Removed

Removed automatic promotion of global variables to TY_FLOAT (line 270 of bpp_types.bsm). Previously, `_stb_mouse_x = lx / 3.0` would permanently mark `_stb_mouse_x` as a float global, causing all subsequent stores to write raw double bit patterns (19-digit numbers starting with 46).

Now globals are only float if explicitly annotated with `: float`. No globals in the codebase use `: float`, so this change has zero impact on existing code.

### Window Close Detection

Added `isVisible` check at the end of `_stb_poll_events()` (once per frame, after all events are processed). When the user clicks the red X button, `_plt_quit_flag` is set to 1 and `game_should_quit()` returns true.

Also added `game_end()` / `_stb_shutdown()` to stbgame.bsm, but `return 0` from main already terminates the process correctly — `game_end()` is optional for resource cleanup.

### bootstrap.c Status

bootstrap.c was generated on 2026-03-26 but never tested. It cannot compile current B++ source — the lexer/parser are outdated. Marked as P0 in the TODO. Without a working bootstrap.c, B++ cannot be distributed to platforms where a pre-built binary isn't available.

### Files Changed

| File | Change |
|------|--------|
| `src/aarch64/a64_codegen.bsm` | float_ret/float_ret2, frame +16 bytes, d0/d1 saves, var_is_float respects hints |
| `src/x86_64/x64_codegen.bsm` | Same changes for x86_64 |
| `src/bpp_import.bsm` | `hash_with_deps()` replaces `propagate_stale()` |
| `src/bpp.bpp` | Calls `hash_with_deps` instead of `propagate_stale` |
| `src/bpp_types.bsm` | Removed global float auto-promotion |
| `src/bpp_validate.bsm` | `float_ret` registered as builtin |
| `stb/_stb_platform_macos.bsm` | Mouse via float_ret, `: int` conversion, window close |
| `stb/stbgame.bsm` | `game_end()` / `_stb_shutdown()` |
| `bootstrap.c` | Mirrored changes (non-functional — TODO) |
| `README.md`, `docs/` | Updated |

### Verification

| Suite | Result |
|-------|--------|
| Core tests | 17/17 |
| Self-hosting | Pass (gen2 compiles test_hello) |
| Cache test | Incremental == Fresh (byte-identical) |
| Mouse tracking | Square follows mouse, X/Y in HUD |
| Window close | ESC + red X both terminate cleanly |

### Lessons Learned

- Cache invalidation bugs manifest as non-deterministic builds — the same source produces different binaries depending on cache state
- Go's hash-of-hashes approach is simple and correct — propagation-based schemes have edge cases
- Type inference should never override explicit programmer annotations
- Global type promotion is dangerous — it silently changes storage format across the entire program
- `isVisible` in the event loop (once per frame) is cheap; in `should_close` (per check) causes slowdown
- `return 0` from main terminates macOS Cocoa processes cleanly — no need for `NSApp terminate:`
- Always recompile the compiler before testing platform changes — stb modules are compiled by bpp

## 2026-03-27 (session 2) — Mach-O Fix, Packed Structs, shr/assert, C Emitter, Input Lag

Second session of the day. Fixed the Mach-O encoder, added packed structs, new builtins, fixed the C emitter, and eliminated input lag on both platforms.

### Mach-O Encoder Fix

The binary encoder had three bugs that caused SIGKILL on certain code sizes:

1. **ULEB128 hardcoded to 2 bytes**: The exports trie encoded `_main`'s offset with a fixed 2-byte ULEB128 and hardcoded `terminal_size=3`. When `_main` offset exceeded 16384 (large binaries like the compiler itself), it needed 3+ bytes. Now uses `mo_write_uleb128()` with dynamic `terminal_size`.

2. **4-byte alignment on LINKEDIT sub-offsets**: dyld requires `sizeof(void*)` alignment (8 bytes on ARM64) for chained fixups, exports trie, symtab, and strtab offsets. The encoder only aligned chained fixups to 4 bytes. Now all LINKEDIT content is 8-byte aligned.

3. **No explicit padding between LINKEDIT sections**: The layout computed offsets upfront, but writers could produce different sizes than estimated. Now explicit `mo_pad(mo_et_off - mo_pos)` calls between each section guarantee that file positions match load command offsets.

Also discovered: `cp` on macOS can invalidate the kernel's code signature cache (same inode, different content). Fix: `rm -f ./bpp` before `cp` ensures a fresh inode. Updated `install.sh`.

### shr() and assert() Builtins

`shr(value, shift)` — logical right shift (unsigned, fills with zeros). Unlike `>>` which is arithmetic (sign-extending), `shr` uses LSRV on ARM64 and SHR CL on x86_64. Critical for bit manipulation on unsigned values.

`assert(cond)` — traps if condition is zero. ARM64: CBNZ over BRK #0. x86_64: TEST+JNE over INT3. Zero overhead when condition is true (single branch, predicted taken).

### Packed Structs

Struct fields can now have type hints that determine their byte size:

```
struct Pixel { r: byte, g: byte, b: byte, a: byte }  // 4 bytes total
struct Fat   { r, g, b, a }                            // 32 bytes (8 per field, unchanged)
```

Implementation:
- Parser: `sd_fhints` parallel array stores per-field type hints. `add_struct_field` computes cumulative offsets. `get_field_offset` sums actual field sizes instead of `i * 8`.
- Codegen: `T_MEMLD` and `T_MEMST` nodes carry the field hint in `n.b`/`n.c`. ARM64 emits LDRB/STRB for byte, LDRH/STRH for quarter, LDR W/STR W for half.
- `sizeof(Packed)` correctly returns 4, not 32.

Backward compatible: fields without hints remain 8 bytes.

### C Emitter Fix

The C emitter's syscall wrappers used inline assembly (`svc #0x80`) which broke under `-O2` — the compiler reordered stores to `_bpp_scratch` before the asm fence, corrupting output. Fixed by replacing all inline asm syscall wrappers with libc calls (`write()`, `read()`, `open()`, `close()`). The C emitter already links libc for malloc/free/fork.

Added `#include <fcntl.h>` for `open()`.

### Global Float Type Hints

`auto x: float` at global scope now correctly registers the variable as TY_FLOAT via `ty_set_global_type()` in `parse_global()`. Previously, the `: float` annotation was parsed but the `_prim_hint` was never applied to globals — only to locals and struct fields.

### Input Lag Elimination (macOS + Linux)

Moved input polling from `game_frame_begin()` to the end of `_stb_present()`, after the frame sleep. This ensures input is read as close as possible to when it's used, eliminating the 1-frame delay caused by polling before sleep.

Also replaced fixed-duration sleep with smart frame timing: `remaining = budget - elapsed`. Only sleeps the time left in the frame budget, not a full frame duration regardless of render time.

### Key Event Beep Fix

macOS Cocoa plays an alert beep when key events are forwarded via `sendEvent:` without a responder. Fixed by filtering out `NSEvKeyDown` and `NSEvKeyUp` before calling `sendEvent:` — B++ consumes key events directly from the event stream.

### Files Changed

| File | Change |
|------|--------|
| `src/aarch64/a64_macho.bsm` | ULEB128 dynamic, 8-byte alignment, explicit LINKEDIT pads |
| `src/aarch64/a64_codegen.bsm` | shr(), assert(), packed T_MEMLD/T_MEMST |
| `src/x86_64/x64_codegen.bsm` | shr(), assert(), packed T_MEMLD/T_MEMST |
| `src/bpp_parser.bsm` | sd_fhints, packed field offsets, global `: float` registration |
| `src/bpp_validate.bsm` | shr, assert registered as builtins |
| `src/bpp_emitter.bsm` | libc wrappers replace inline asm, fcntl.h |
| `stb/_stb_platform_macos.bsm` | Smart frame timing, post-sleep poll, key filter |
| `stb/_stb_platform_linux.bsm` | Smart frame timing, post-sleep poll |
| `stb/stbgame.bsm` | game_frame_begin simplified, first-frame poll |
| `install.sh` | rm before cp for fresh inode |

### Verification

| Suite | Result |
|-------|--------|
| ARM64 | 19/19 (including test_packed, test_builtins) |
| Linux | 6/6 compile, 5/6 run in Docker |
| C emitter | PASS with -O2 |
| Self-host | gen2 compiles and runs |
| MCU game | Cat chases rat, smooth movement with `: float` globals |
| Mouse test | Cursor tracking at 60fps, no lag, no beep |

### Lessons Learned

- Mach-O LINKEDIT requires 8-byte aligned offsets — Apple's dyld silently rejects misaligned binaries
- ULEB128 sizes must be computed dynamically, never hardcoded — binary size determines encoding width
- `cp` on macOS preserves content but can invalidate kernel code signature cache — always `rm` first
- C compiler optimizations (-O2) can reorder stores past inline asm even with "memory" clobber — use libc wrappers
- Global `: float` annotations must be explicitly registered — the parser reads the hint but must apply it
- Input polling belongs after the frame sleep, not before — eliminates 1 frame of lag
- Fixed-time sleep wastes frame budget — subtract elapsed time for responsive input
- Packed structs are backward compatible: no hint = 8 bytes per field, same as before
- MCU game is the best integration test — catches type system regressions immediately

---

## 2026-03-27 — Type System Refactor: Base × Slice

### The Change

Replaced the flat type enum (TY_BYTE=1, TY_QUART=2, TY_HALF=3, TY_WORD=4, TY_PTR=5, TY_FLOAT=6, TY_STRUCT=7) with an orthogonal base × slice system packed in a single byte.

5 base types (lower nibble): UNKNOWN(0), WORD(1), FLOAT(2), PTR(3), STRUCT(4)
5 slices (upper nibble): FULL(0), HALF(1), QUARTER(2), BYTE(3), DOUBLE(4)

Encoding: `base | (slice × 16)`. 25 composed type constants in a 5×5 grid. Every base can combine with every slice — the type system is a LEGO set.

```
              BYTE(8)  QUARTER(16)  HALF(32)  FULL(64)  DOUBLE(128)
UNKNOWN       0x30      0x20        0x10      0x00       0x40
WORD          0x31      0x21        0x11      0x01       0x41
FLOAT         0x32      0x22        0x12      0x02       0x42
PTR           0x33      0x23        0x13      0x03       0x43
STRUCT        0x34      0x24        0x14      0x04       0x44
```

### What Changed

- **defs.bsm**: 25 type constants, 10 building blocks (5 bases + 5 slices), 6 helper functions (ty_base, ty_slice, ty_make, is_float_type, is_int_type, slice_width)
- **bpp_types.bsm**: promote() rewritten with base+slice logic — float+wider-int widens float to avoid precision loss. ty_set_var_type() uses promote() instead of fragile `>` comparison. type_of_lit() returns TY_WORD for all integer literals (programmer chooses slice).
- **bpp_parser.bsm**: try_type_annotation() supports compound "half float" → TY_FLOAT_H and "quarter float" → TY_FLOAT_Q. field_hint_size() uses slice_width(). Global float registration uses is_float_type().
- **bpp_emitter.bsm**: emit_type_for() dispatches by base then slice. T_DECL grouping uses ty_slice().
- **bpp_validate.bsm**: W002 uses is_float_type().
- **a64_codegen.bsm + x64_codegen.bsm**: All `== TY_FLOAT` checks use is_float_type(). All sub-word checks use ty_slice() == SL_BYTE/SL_QUARTER/SL_HALF.

### Why

The flat enum mixed "what it is" (word vs float vs ptr) with "how wide" (byte vs half vs full) in one axis. This:
- Blocked f32/f16 (needed for audio/GPU)
- Made promote() depend on fragile numeric ordering
- Required manual additions to 15+ places for every new type
- Conflated literal magnitude with variable storage width

The new system separates concerns into two orthogonal axes. Adding a new width or base type is additive, not multiplicative.

### Test Results

| Suite | Result |
|-------|--------|
| Native (17 tests) | PASS |
| Bootstrap verify | Byte-identical |
| C emitter | PASS |
| Snake native | Compiles and runs |
| MCU game | Runs correctly |
| Mouse tester | Cursor tracking at 60fps |

### Lessons Learned

- B++ functions return one value — this made two-field types impractical. Packed encoding (base | slice << 4) is the right fit for a single-return language.
- type_of_lit() returning TY_BYTE for small literals was a design mistake — literal magnitude should not determine variable storage width. Slices are programmer opt-in.
- The codegen already implemented the base+slice decision in two steps ("is it float?" then "which sub-word?"). The refactor just named what was already there.
- promote() must widen float to at least the integer operand's width: promote(f32, i64) = f64, not f32. Returning the narrower float loses 41 bits of precision.
- SL_DOUBLE (128-bit) slot is reserved for future SIMD/NEON even though no codegen supports it yet — costs zero to include.

## 2026-03-30 — .bo Cache Fix + Compiler Self-Hash

### The Bug

Modular compilation with `.bo` caching had a critical bug: `bo_read_export()` did not push to `sd_fhints` when loading structs from cache. When `add_struct_field()` subsequently called `arr_get(sd_fhints, idx)`, it read beyond the array bounds, got NULL, and crashed with `*(NULL + offset) = value` (SIGSEGV).

This meant the second bootstrap stage always crashed when the cache was populated — `bpp` compiled `bpp2` (populating cache), but `bpp2` crashed compiling `bpp3` (loading corrupt cache). The bug was masked by manually clearing `.bpp_cache/` before each bootstrap.

### Root Cause

In `src/bpp_bo.bsm`, `bo_read_export()` for structs was missing one line compared to `add_struct_def()` in the parser:

```
// Parser (add_struct_def) — correct:
sd_fhints = arr_push(sd_fhints, malloc(64 * 8));

// bo_read_export — was missing this line entirely
```

Additionally, `add_struct_field()` was called with 2 arguments instead of 3 (missing the `hint` parameter).

### Fix

Two lines added to `bo_read_export()`:
1. `sd_fhints = arr_push(sd_fhints, malloc(64 * 8));` — initialize hints array for cached structs
2. `add_struct_field(idx, bo_read_str(), 0);` — pass explicit hint=0

### Compiler Self-Hash

Added `compiler_self_hash()` to `bpp_import.bsm`. The cache manifest hash now includes the FNV-1a hash of the first 8KB of the compiler binary (`argv[0]`). When the compiler changes, the entire cache is invalidated automatically — no manual clearing needed.

Uses a private `malloc(8192)` buffer to avoid corrupting `_cache_buf` which is shared by `cache_load()` and `cache_save()`.

### .gitignore Update

`.bpp_cache/` directory is now tracked in git (via `.gitkeep`) so it always exists after clone. Only the contents (`.bo` files and `hashes`) are gitignored.

### Verification

| Test | Result |
|------|--------|
| Bootstrap (bpp == bpp2 == bpp3 with cache) | PASS |
| 13 native tests (macOS ARM64) | PASS |
| 13 cross-compiled tests (Linux x86_64 via Docker) | PASS |
| Snake native (Cocoa) | Compiles and runs |
| MCU game | Compiles and runs |
| Linux ELF generation | Valid ELF64 binary |

### Files Changed

- `src/bpp_bo.bsm` — 2 lines added (sd_fhints init + hint arg)
- `src/bpp_import.bsm` — compiler_self_hash() + manifest hash integration
- `.gitignore` — track .bpp_cache/ directory, ignore contents
- `.bpp_cache/.gitkeep` — ensure directory exists in repo

## 2026-03-31 — Optimized Type Encoding + FLOAT_H/Q Codegen

### Milestone

Type system encoding redesigned: bit 0 = float flag, 14 phantom types removed, `is_float_type(ty) = ty & 1` (1 ARM64 instruction). All float sub-types now have working codegen on ARM64 and x86_64 cross-compilation.

### Type Encoding Change

Old encoding (5x5 grid): `base | (slice * 16)` with 25 constants, 12 unused.

New encoding (bit-0 float flag):
```
bit 0     = float flag (1 = float, 0 = not float)
bits 0-3  = base (WORD=0x02, FLOAT=0x03, PTR=0x04, STRUCT=0x08, UNK=0x0C)
bits 4-5  = slice (FULL=0, HALF=1, QUARTER=2, BYTE=3)
bits 6-7  = free (future use)
```

11 real types. `is_float_type()` reduced from AND+CMP+BRANCH (3 insns) to single TBNZ (1 insn). `is_int_type()` and `SL_DOUBLE` removed (zero callers). Only `defs.bsm` changed — all other files use named constants.

### FLOAT_H / FLOAT_Q Codegen

ARM64 (a64_codegen.bsm + a64_enc.bsm):
- `a64_emit_load_var`: FLOAT_H loads s-register + FCVT d,s; FLOAT_Q loads h-register + FCVT d,h
- `a64_emit_store_var`: FCVT s,d + STR s (FLOAT_H); FCVT h,d + STR h (FLOAT_Q)
- T_MEMLD/T_MEMST: float field types in packed structs (load/store + widen/narrow)
- Param narrowing in a64_emit_func prologue
- 15 new encoder functions: single-precision arithmetic, conversion, load/store, half-precision conversion and load/store

x86_64 (x64_enc.bsm):
- MOVSS load/store, CVTSS2SD, CVTSD2SS encoder functions
- Codegen updates deferred (multi-statement if-block bug in x64 self-compile)

### Parser Fix: half float / quarter float

The compound type annotation `auto x: half float` triggered an infinite loop due to the multi-statement if-block codegen bug. Fix: extracted `_parse_half_hint()` and `_parse_quarter_hint()` helper functions with flat guard patterns (no nested if-blocks).

### putchar_err Builtin

Write 1 byte to stderr (fd=2). Same as putchar but for diagnostic output.

### ELF Writer Bug Found

`sys_open(filename, 0x601)` uses macOS O_CREAT/O_TRUNC flags. Linux uses different values (O_CREAT=0x40, not 0x200). Fix: changed to 0x241 for Linux ELF writer. Files were only created when pre-existing (O_TRUNC without O_CREAT).

### macOS Code Signing Cache Discovery

macOS kernel caches code signing decisions per filesystem path. When a binary at a path is replaced (e.g., `bpp2` overwritten by compilation), the kernel may kill the new binary with SIGKILL (exit 137) based on the cached decision from the old binary. Workaround: copy binary to a fresh path in `/tmp/` before executing.

### Verification

| Test | Result |
|------|--------|
| ARM64 bootstrap convergence | PASS (SHA 19beb13d) |
| x86_64 cross-compile programs | PASS |
| FLOAT_H arithmetic (add, sub, mul, div) | PASS |
| FLOAT_Q (16-bit half float) | PASS |
| WORD_H (32-bit int) | PASS |
| putchar_err | PASS |
| half float parser | PASS |
| quarter float parser | PASS |

### Files Changed

- `src/defs.bsm` — Encoding values, helper functions, remove 14 phantoms
- `src/aarch64/a64_enc.bsm` — 15 new encoder functions (single + half precision)
- `src/aarch64/a64_codegen.bsm` — FLOAT_H/Q in load/store/memld/memst/params, putchar_err
- `src/bpp_parser.bsm` — _parse_half_hint, _parse_quarter_hint helpers
- `src/x86_64/x64_enc.bsm` — MOVSS/CVTSS2SD/CVTSD2SS encoder functions
- `src/x86_64/x64_elf.bsm` — sys_open flags fix (0x601 → 0x241)

---

## 2026-03-31b — GPU Rendering via Metal + Go-Style Cache

### GPU Rendering (stbrender)

First GPU rendering in B++. Metal backend embedded in `_stb_platform_macos.bsm` (one file per platform). Public API in `stb/stbrender.bsm`.

**Working API:**
- `render_init()` — creates Metal device, command queue, CAMetalLayer, compiles shader, builds pipeline
- `render_begin()` — gets drawable, opens render pass with clear color
- `render_clear(color)` — sets clear color (ARGB packed, same as stbdraw)
- `render_rect(x, y, w, h, color)` — batched colored rectangle (6 vertices per quad)
- `render_end()` — flushes batch, presents, frame timing, input polling

**Vertex format:** 8 bytes per vertex (int16 x,y + uint8 r,g,b,a). All integer — no float writes from B++. Shader converts int→float on GPU.

**Shader:** Embedded as string literal, compiled at runtime via `[MTLDevice newLibraryWithSource:options:error:]`. Shader source built with `sb_cat()` + `\n` escape sequences.

**Architecture decision:** GPU code lives IN the platform file. `stbrender.bsm` is platform-agnostic. `_stb_gpu_*` functions are platform implementations.

### String Escape Sequences

`scan_string()` in lexer now processes escape sequences, matching `scan_char()` behavior:
- `\n` (10), `\t` (9), `\r` (13), `\0` (0)
- `\\` (92), `\"` (34)
- `\xHH` (hex byte)

Processed bytes written directly to vbuf; token length reflects decoded output.

### Platform Architecture Refactor

**stbgame imports platform internally.** Game files no longer need `import "_stb_platform.bsm"`. Removed from 4 files. Driver override still works (last-definition-wins).

**Frame timing and input polling extracted from platform files.** Platform only provides raw operations:
- `_stb_present()` — blit framebuffer (macOS: CoreGraphics, Linux: ANSI)
- `_stb_gpu_present()` — flush batch + Metal commit
- `_stb_frame_wait()` — sleep remaining frame budget (macOS: usleep, Linux: nanosleep)

Orchestration moved to agnostic modules:
- `draw_end()` = `_stb_present()` + `_stb_frame_wait()` + `_input_save_prev()` + `_stb_poll_events()`
- `render_end()` = `_stb_gpu_present()` + `_stb_frame_wait()` + `_input_save_prev()` + `_stb_poll_events()`

### Go-Style Build Cache

Migrated from per-project `.bpp_cache/` to global `~/.bpp/cache/`. Files named by content hash.

**Old system:**
- `.bpp_cache/stb_stbgame.bo` (filename = module path)
- `.bpp_cache/hashes` (text file with manifest + per-module hashes)
- 3-layer validation (manifest hash + source hash + dependency hashes)

**New system:**
- `~/.bpp/cache/b7a1df7edb63d9cd.bo` (filename IS the hash)
- No hashes file needed
- Validation = file exists (hash encodes source + deps + compiler version)
- Compiler hash mixed into every module hash (upgrading bpp invalidates all)
- Shared across projects (stbdraw compiles once, any project reuses)

Added `_getenv("HOME")` (pure B++, reads environ from argv memory), `cache_init()` (creates `~/.bpp/` and `~/.bpp/cache/`), `cache_init_stale()`.

Removed `cache_load()`, `cache_save()`, `cache_manifest_hash()`, `_cache_buf`.

### Other Changes

- `sys_mkdir(path, mode)` builtin: ARM64 (syscall 136), x86_64 (syscall 83), C emitter. Not yet in validator.
- `init_str()` called by `game_init()` so `sb_new()`/`sb_cat()` work for GPU shader builder.
- macOS number key mapping: KEY_0–KEY_9 added to `_plt_map_vkey()`.
- Parser workarounds removed: `_parse_half_hint`, `_parse_quarter_hint`, `_check_next_float` inlined back into `try_type_annotation`. Multi-statement if-blocks confirmed working.
- `.bo` header version bumped to 3 (no hash data in header, simplified to magic + version + target).

### RESOLVED: "Mach-O Writer Bug" Was Cache Bug

The SIGSEGV when adding functions was NOT a Mach-O writer bug. It was the .bo cache serving stale object files. Root cause: `compiler_self_hash()` only read 8KB of the compiler binary — insufficient to detect code changes past the first few modules.

---

## 2026-03-31c — Cache Fixes + GPU Primitives + Architecture

### Cache Bug Fixes (3 separate bugs)

**Bug 1: compiler_self_hash() read only 8KB.** The compiler binary is ~347KB. Changes to modules compiled after the first 8KB were invisible to the cache. Fix: read entire binary in 8KB chunks, accumulate hash.

**Bug 2: Module 0 (top-level .bpp) not isolated in outbuf.** The modular pipeline used `lex_module(0)` which got the wrong source range because module 0's content is at the END of outbuf (after all imports), not at position 0 where `diag_file_starts[0]` pointed. Fix: track `mod0_real_start` (set after last import), compile module 0 separately after the cache loop using the correct range.

**Bug 3: mod_idx() misattributed module 0 tokens.** The binary search in `mod_idx()` returned the last stb module for tokens belonging to module 0 (because module 0's content is past all other modules in outbuf). This caused the last stb module's .bo to contain module 0's functions and strings. Fix: special case in `mod_idx()` — if `src_off >= mod0_real_start`, return 0.

**Bug 4: lex_module for last module included module 0 content.** `lex_module(mk-1)` lexed from `starts[mk-1]` to `src_len`, which included module 0's code at the end. Fix: stop at `mod0_real_start` instead of `src_len`.

### Validator Updates

Registered `sys_mkdir` and `putchar_err` as builtins in `bpp_validate.bsm`. C backend compilation no longer fails on these.

### C Emitter Fixes

- `emit_c_str()`: re-escapes `\n`, `\t`, `\r`, `\0`, `\\`, `\"` in string literals. The lexer now processes escape sequences into raw bytes, so the C emitter must re-escape them for valid C output.
- Added `#include <sys/stat.h>` for `mkdir()` in the C runtime.

### GPU Render API Expansion

Added to `stbrender.bsm` (platform-agnostic, uses `_stb_gpu_vertex` + `_stb_gpu_rect`):
- `render_line(x0, y0, x1, y1, color)` — axis-aligned as 1px rect, diagonal as thin quad
- `render_circle(cx, cy, radius, color)` — triangle fan, 32 segments
- `render_circle_outline(cx, cy, radius, color)` — line segments
- `render_rect_outline(x, y, w, h, color)` — 4 lines

### Trigonometry Moved to stbmath.bsm

Fixed-point cos/sin table (32 segments, ×1024) moved from `_stb_platform_macos.bsm` to `stbmath.bsm`. Functions `math_cos(i)` and `math_sin(i)` are platform-agnostic. Geometry functions in stbrender use these instead of per-backend trig tables. `math_trig_init()` called by `game_init()`.

Removed `_stb_gpu_line`, `_stb_gpu_circle`, `_stb_gpu_circle_outline` from macOS backend. Backend now only provides raw primitives: `_stb_gpu_vertex`, `_stb_gpu_rect`, `_stb_gpu_clear`, `_stb_gpu_begin`, `_stb_gpu_present`.

### Architecture Decision: GPU Philosophy

GPU rendering is native per platform via codegen/FFI. No third-party APIs (no SDL, no GLFW).
- macOS ARM64: Metal via objc_msgSend (DONE)
- Linux x86_64: Vulkan via libvulkan.so FFI (PLANNED)
- Windows x86_64: DirectX 12 or Vulkan (FUTURE)

`stbrender.bsm` is identical on all platforms. Backend primitives (`_stb_gpu_*`) are platform-specific.

### Verification

| Test | Result |
|------|--------|
| Bootstrap convergence (gen2=gen3) | PASS |
| Cache: different programs get correct strings | PASS (rect→circle→rect) |
| Cache: no manual clearing needed | PASS |
| test_hello (no imports) | PASS |
| test_gpu_circle (filled circle) | PASS |
| test_gpu_shapes (lines, circles, outlines, rects) | PASS |
| test_gpu_rect (WASD movement) | PASS |

### Files Changed

- `src/bpp_import.bsm` — compiler_self_hash reads full binary, mod0_real_start tracking, mod_idx special case for module 0
- `src/bpp_lexer.bsm` — lex_module stops at mod0_real_start for last module
- `src/bpp_validate.bsm` — sys_mkdir + putchar_err registered
- `src/bpp_emitter.bsm` — emit_c_str, sys/stat.h include
- `src/bpp.bpp` — module 0 compiled separately after cache loop
- `stb/stbmath.bsm` — math_cos/math_sin, fixed-point trig table
- `stb/stbgame.bsm` — math_trig_init() in game_init
- `stb/stbrender.bsm` — render_line, render_circle, render_circle_outline, render_rect_outline
- `stb/_stb_platform_macos.bsm` — removed geometry functions (line/circle/outline)
- `tests/test_gpu_shapes.bpp` — NEW: visual test for all render primitives
- `tests/test_gpu_circle.bpp` — NEW: minimal circle test
- `tests/test_syntax.bpp` — NEW: tests for else if, buf[i], for loop

---

## 2026-03-31d — Syntax Sugar + Codebase Cleanup

### New Syntax (parser-only, zero codegen changes)

**`buf[i]`** — Array indexing desugars to `*(buf + i * 8)` for reads, `*(buf + i * 8) = val` for writes. Added in `parse_primary()` after variable resolution. When parser sees `[`, it builds T_BINOP(+, base, T_BINOP(*, index, 8)) wrapped in T_MEMLD. Assignment to `buf[i]` works automatically because the existing `T_MEMLD = val` → `T_MEMST` conversion handles it.

**`for (init; cond; step) { body }`** — Desugars to `init; while (cond) { body; step; }`. Implemented via `_for_init` global: `parse_for()` stashes the init statement, returns T_WHILE with step appended to body. `parse_body()` and `parse_function()` check `_for_init` and insert before the while node. Added `for` to lexer keywords.

**`else if`** — In `parse_if()`, when `else` is followed by `if`, parse inner `parse_if()` directly as single-element else body. No extra braces needed.

**`print_int` availability** — `stbgame.bsm` now imports `stbio.bsm`. Any program using stbgame gets `print_int`, `print_msg`, `print_ln` automatically.

### Codebase Cleanup (~230 conversions)

Applied the new syntax to the compiler source, stb library, and examples:
- ~110 `*(arr + i * 8)` → `arr[i]` conversions across 8 compiler files
- ~120 `while` → `for` loop conversions across 11 files
- Examples and tests updated with new syntax

### Files Changed

- `src/bpp_lexer.bsm` — `for` keyword added to check_kw
- `src/bpp_parser.bsm` — parse_for(), else if in parse_if(), buf[i] in parse_primary(), _for_init handling in parse_body() and parse_function()
- `src/bpp_emitter.bsm` — ~95 syntax conversions (buf[i] + for loops)
- `src/bpp_types.bsm` — ~50 syntax conversions
- `src/bpp_dispatch.bsm` — ~29 syntax conversions
- `src/bpp_validate.bsm` — ~9 syntax conversions
- `src/bpp_import.bsm` — ~33 syntax conversions
- `src/bpp_bo.bsm` — ~35 syntax conversions
- `src/bpp_parser.bsm` — ~15 syntax conversions
- `src/bpp_diag.bsm` — ~7 syntax conversions
- `src/bpp.bpp` — ~16 syntax conversions
- `stb/stbgame.bsm` — imports stbio.bsm, math_trig_init() call
- `stb/stbmath.bsm` — for loop conversions
- `examples/*.bpp` — updated with new syntax
- `tests/test_syntax.bpp` — NEW: validates all three syntax sugars

### Verification

| Test | Result |
|------|--------|
| ARM64 bootstrap convergence | PASS (SHA 88a5a9da) |
| Parser workaround removal | PASS (half float, quarter float) |
| String escape sequences | PASS (\n, \t, \xHH, \\, \") |
| GPU clear (Metal) | PASS (color switching 1-6) |
| GPU rectangles (Metal) | PASS (5 rects + WASD movement) |
| snake_native (software) | PASS |
| SDL/raylib examples | Compile OK |
| Linux cross-compile (6 tests) | PASS |
| C emitter output | PASS |
| Go-style cache location | PASS (~/.bpp/cache/) |
| Cache invalidation | PASS (hash changes on source edit) |
| 13 unit tests | PASS |

### Files Changed

- `src/bpp_lexer.bsm` — escape sequences in scan_string, _hex_val helper
- `src/bpp_parser.bsm` — removed workaround helpers, inlined half/quarter parsing
- `src/bpp_import.bsm` — _getenv, cache_init, cache_init_stale, compiler hash in deps, removed cache_load/cache_save/cache_manifest_hash
- `src/bpp_bo.bsm` — hash-based bo_make_path, v3 header, _bo_init_hex
- `src/bpp_emitter.bsm` — sys_mkdir builtin + runtime
- `src/bpp_validate.bsm` — (needs sys_mkdir registration, TODO)
- `src/aarch64/a64_codegen.bsm` — sys_mkdir builtin
- `src/x86_64/x64_codegen.bsm` — sys_mkdir builtin
- `src/bpp.bpp` — cache_init, _bo_init_hex, removed cache_save
- `stb/stbgame.bsm` — imports _stb_platform + stbstr, init_str in game_init
- `stb/stbdraw.bsm` — draw_end orchestrates present+wait+poll
- `stb/stbrender.bsm` — NEW: GPU rendering public API
- `stb/_stb_platform_macos.bsm` — Metal GPU backend, _stb_frame_wait, number keys, removeFromSuperview fix
- `stb/_stb_platform_linux.bsm` — _stb_frame_wait extracted
- `tests/test_gpu_clear.bpp` — NEW: GPU clear with color switching
- `tests/test_gpu_rect.bpp` — NEW: GPU rectangles with movement
- `tests/test_escape.bpp` — NEW: escape sequence validation

---

## 2026-04-01 — Cache Fix, sys_fork, Bug Debugger, Structural Overhaul

### Milestone

Three structural bugs that plagued B++ for days were found and fixed in a single session. The modular cache now works like Go's build cache. The `sys_fork` syscall works correctly. The `bug` native debugger is integrated into the compiler.

### Root Causes Found

**"BUG-1" was never a codegen bug.** The "multi-statement if-block crash" was actually three separate cache bugs:

1. **Missing dependency edges**: All 16 compiler .bsm modules imported everything through bpp.bpp (flat imports). The dependency graph had zero inter-module edges. `hash_with_deps()` couldn't propagate changes because there were no edges to propagate on. Fix: added explicit `import` statements to every module.

2. **sys_fork label mismatch**: macOS fork returns PID in x0 for both parent and child, uses x1 to distinguish. The fix needed `enc_cbz_label(1, lbl)` but was coded with `a64_new_lbl()` (text label counter) instead of `enc_new_label()` (binary encoder label). Fix: one word change.

3. **Cache cross-contamination**: All programs shared the same .bo files in `~/.bpp/cache/`. Fix: mix the main file hash (module 0) into every module's combined hash, isolating each program's cache.

### New Syscall Builtins (both backends)

| Builtin | macOS (ARM64) | Linux (x86_64) |
|---------|---------------|----------------|
| `sys_ptrace(req, pid, addr, data)` | syscall 26 | syscall 101 |
| `sys_wait4(pid, status, opts, rusage)` | syscall 7 | syscall 61 |
| `sys_getdents(fd, buf, size)` | getdirentries64 (344) | getdents64 (217) |
| `sys_unlink(path)` | syscall 10 | syscall 87 |

`sys_fork` fixed on ARM64: `cbz x1, lbl; mov x0, #0` after `svc #0x80`.

### Bug Debugger Integration

- `--bug` flag generates `.bug` debug map alongside binary
- `bug` program compiles standalone, reads `.bug`, launches target
- Dump mode: functions, structs, globals, address map, stack map
- Run mode: fork+exec, ptrace observe, crash reports with function names
- ASLR slide calculated at runtime
- Breakpoint infrastructure complete but blocked by macOS W^X (BUG-5)

### Cache System (Go Model)

- Explicit imports in all 16 modules create real dependency graph
- `hash_with_deps()` propagates transitively (leaf-first topological order)
- Main file hash mixed into all modules (per-program isolation)
- `--clean-cache` deletes `~/.bpp/cache/` via native sys_fork + /bin/rm

### Validate + Typeck Merged

Single module, single AST walk:
- Layer 1 (always on): E201 undefined function, W002/W003/W005
- Layer 2 (hint-activated): E050/E052/E053, W010/W011
- Per-function hint table for Layer 2

### Files Changed (22 files, +595 lines)

- `src/bpp.bpp` — --bug flag, --clean-cache, init_bug/init_validate, bug_save
- `src/bpp_import.bsm` — explicit imports, cache_clean, main_hash in hash_with_deps
- `src/bpp_validate.bsm` — complete rewrite: merged typeck
- `src/bpp_diag.bsm`, `bpp_internal.bsm`, `bpp_lexer.bsm`, `bpp_parser.bsm`, `bpp_types.bsm`, `bpp_dispatch.bsm`, `bpp_emitter.bsm`, `bpp_bo.bsm` — explicit imports
- `src/aarch64/a64_codegen.bsm` — sys_fork fix, sys_ptrace, sys_wait4, sys_getdents, sys_unlink
- `src/aarch64/a64_enc.bsm`, `a64_macho.bsm` — explicit imports
- `src/x86_64/x64_codegen.bsm` — sys_ptrace, sys_wait4
- `src/x86_64/x64_enc.bsm`, `x64_elf.bsm` — explicit imports
- `src/bpp_emitter.bsm` — sys_ptrace/sys_wait4/sys_getdents/sys_unlink C wrappers
- `src/bug_observe_macos.bsm` — native syscalls, breakpoint infra, ASLR slide
- `src/bug.bpp` — 64-bit out_hex fix
- `src/defs.bsm` — FFI type classification helpers

## 2026-04-02 — Game Infrastructure + Syscall Builtins + Bug Debugger Fixes

### Milestone

Three new stb modules for game development: arena allocator, object pool, and entity-component system. Four new syscall builtins added to both backends. Bug debugger now reads PC correctly via Mach APIs with entitlements.

### New stb Modules

| Module | Lines | What it does |
|--------|-------|-------------|
| `stbarena.bsm` | ~50 | Bump allocator. O(1) alloc, O(1) reset. 8-byte aligned, overflow guard. |
| `stbpool.bsm` | ~70 | Fixed-size object pool. O(1) get/put via embedded freelist. Min 8-byte objects. |
| `stbecs.bsm` | ~120 | Entity-component system. Spawn/kill/recycle IDs, parallel arrays (pos, vel, flags), milli-unit physics. Games add custom component arrays indexed by same ID. |

### New Syscall Builtins (both backends)

| Builtin | macOS (ARM64) | Linux (x86_64) |
|---------|---------------|----------------|
| `sys_lseek(fd, off, whence)` | syscall 199 | syscall 8 |
| `sys_fchmod(fd, mode)` | syscall 124 | syscall 91 |
| `sys_unlink(path)` | syscall 10 (was missing) | syscall 87 (existed) |
| `sys_getdents(fd, buf, sz)` | syscall 344 (was missing) | syscall 217 (existed) |

All four also added to C emitter (`bpp_emitter.bsm`) and validator (`bpp_validate.bsm`).

### Bug Debugger Fixes

1. **Entitlements**: Created `bug.entitlements` with `com.apple.security.cs.debugger` + `get-task-allow`. Signing bug binary with entitlements makes `task_for_pid` work, enabling PC reading via Mach APIs.
2. **ASLR slide**: Now calculated correctly from first-stop PC. Was returning 0 (negative) before entitlements fix.
3. **lookup_pc**: Fixed to include `_aslr_slide` in address comparison. Was always returning `main` because slide offset dominated the distance calculation.
4. **Breakpoint writing**: Replaced ptrace `PT_WRITE_D` (fails with EINVAL on macOS) with `mach_vm_protect` + `mach_vm_write`. Breakpoints verified as written correctly via `mach_vm_read_overwrite`.
5. **I-cache issue (OPEN)**: ARM64 Apple Silicon I-cache is not coherent with remote `mach_vm_write`. Breakpoints verified in data cache but CPU fetches stale I-cache. Disk patching strategy designed but not yet tested.

### README Updated

- stb table reorganized by category (17 modules now)
- New builtins listed
- Compiler flags section updated (--bug, --clean-cache, --show-deps)

### Bootstrap

Triple bootstrap verified stable after each codegen change. Two rounds: ARM64 syscalls, then x86_64 syscalls. Linux x86_64 tested via Docker.

### Files Changed

- `stb/stbarena.bsm` — new: arena allocator
- `stb/stbpool.bsm` — new: object pool
- `stb/stbecs.bsm` — new: ECS
- `tests/test_arena.bpp`, `test_pool.bpp`, `test_ecs.bpp`, `test_gameinfra.bpp` — new tests
- `src/aarch64/a64_codegen.bsm` — sys_lseek, sys_fchmod, sys_unlink, sys_getdents
- `src/x86_64/x64_codegen.bsm` — sys_lseek, sys_fchmod
- `src/bpp_emitter.bsm` — sys_lseek, sys_fchmod C wrappers
- `src/bpp_validate.bsm` — sys_lseek, sys_fchmod registered
- `src/bug_observe_macos.bsm` — entitlements, mach_vm_protect writes, lookup_pc fix, disk patching (WIP)
- `bug.entitlements` — new: debug entitlements for task_for_pid
- `README.md` — updated features, stb table, compiler flags
- `docs/todo.md` — updated status and done list

## 2026-04-02b — Bug Debugger v2: debugserver Backend + Full Debug Features

### Architecture rewrite

Replaced the entire bug debugger backend. Instead of using ptrace/Mach APIs directly (which couldn't write breakpoints due to I-cache issues), bug now delegates to Apple's `debugserver` (macOS) and `gdbserver` (Linux) via the GDB remote protocol over TCP.

```
bug (B++ program, no entitlements)
  → spawns debugserver/gdbserver (has kernel-level debug privileges)
  → communicates via GDB remote protocol over TCP socket
  → debugserver handles I-cache coherence for software breakpoints
```

### Zero-flag debugging

The debugger now works with zero special flags:

```bash
./bpp examples/snake_full.bpp -o build/snake_full
./bug build/snake_full
```

Function names are read directly from the Mach-O/ELF symbol table — no `--bug` flag needed for basic function tracing. The `--bug` flag remains available for enhanced debugging (local variables, types) via the `.bug` metadata file.

### New features (all automatic, no flags)

1. **Call depth indentation** — nested calls are visually indented
2. **Crash backtrace** — FP chain walk shows the full call stack on crash
3. **Crash locals** — when `.bug` file is present, reads local variables from the target's stack frame via GDB `m` command
4. **Smart filtering** — with `.bug` file: traces only user functions. Without: traces everything from symbol table.

### Example output: crash report

```
-> main()
  -> foo()
    -> bar()

=== CRASH ===
signal: SIGSEGV
pc: 0x0000000104cb8330
in: bar()
  locals:
    x = 45 (0x0000002d)
    y = 10 (0x0000000a)
    p = 0  (0x00000000)
  backtrace:
    foo()
    main()
=============
```

### New builtins (all 3 backends: C emitter + ARM64 + x86_64)

| Builtin | macOS | Linux | Purpose |
|---------|-------|-------|---------|
| `sys_socket(domain, type, proto)` | syscall 97 | syscall 41 | Create TCP/UDP socket |
| `sys_connect(fd, addr, len)` | syscall 98 | syscall 42 | Connect socket to address |
| `sys_usleep(microseconds)` | select() timeout | poll() timeout | Sleep for N microseconds |

### Mach-O symbol table

The ARM64 codegen now emits nlist entries for ALL user functions in the Mach-O symbol table. Previously only `_main` and `__mh_execute_header` were exported. Now `nm binary` shows every function, enabling the debugger to work without the `.bug` file.

### New files

- `src/bug_gdb.bsm` — GDB remote protocol client (TCP socket, packet I/O, breakpoints, registers, memory reads, ASLR slide query)
- `src/bug_observe_macos.bsm` — rewritten: spawns debugserver, Mach-O symbol table parsing, all debug features
- `src/bug_observe_linux.bsm` — rewritten: spawns gdbserver, ELF symbol table parsing, same debug features

### What was removed

- All ptrace/Mach API code from the observer (task_for_pid, thread_get_state, mach_vm_write, etc.)
- BRK embedding in codegen (`--bug` no longer modifies the binary)
- Codesign requirement for the bug binary
- FFI imports in bug_gdb.bsm (replaced with builtins: sys_socket, sys_read, sys_write)

### BUG-5 resolved

The I-cache coherence issue is permanently solved. Apple's debugserver has kernel-level privileges (`com.apple.private.cs.debugger`) that provide I-cache coherence for software breakpoints. We delegate to it instead of trying to work around the limitation ourselves.

### Bootstrap

gen2==gen3 verified after: sys_socket/sys_connect/sys_usleep builtins, BRK removal, symbol table emission, Mach-O layout changes.

### Const (language feature, same session)

- `const NAME = expr;` — compile-time evaluated constants with full arithmetic
- `const fn(args) { body }` — compile-time functions with if/else, recursion, composition
- Negative values supported via T_UNARY wrapping in make_int_lit
- Dead code elimination: `if (0) { ... }` skipped in ARM64 + x86_64 codegens
- `const` keyword added to lexer, parser dispatches in parse_program/parse_module
- const_eval_node: handles T_LIT, T_BINOP, T_UNARY, T_VAR (enum/param lookup), T_CALL
- const_eval_call: separate function to avoid stack frame overflow (23 vars in one function)
- const_eval_body: walks T_RET + T_IF for const function evaluation
- find_enum_idx: returns index (not value) to support negative const values
- arr_truncate added to stbarray.bsm for const function scope cleanup

### W012 Warning (added then removed)

- Added W012 "auto inside block may corrupt stack" — detected 60+ warnings in compiler
- Investigation revealed: `a64_pre_reg_vars`/`x64_pre_reg_vars` ALREADY scan T_IF/T_WHILE recursively
- auto inside blocks was NEVER broken — the pre-scan allocates stack correctly
- W012 removed as false positive. Bootstrap manual updated to document block-scoped auto as supported.
- Retracted myth documented in memory alongside the unicode false correlation.

### Snake ECS (`examples/snake_full.bpp`)

- Complete rewrite using const, stbecs, stbarena, stbpool
- Particles as ECS entities with velocity + lifetime in flags
- WASD + arrow key input
- Score ranking system
- CPU rendering (stbdraw) — GPU version planned (needs render_text via glyph atlas)

### Files Changed (additional)

- `src/bpp_lexer.bsm` — `const` keyword
- `src/bpp_parser.bsm` — const_eval_node, const_eval_call, const_eval_body, parse_const, const_find_fn, find_enum_idx, make_int_lit negative support, _block_depth (added then removed)
- `src/aarch64/a64_codegen.bsm` — DCE for T_IF with literal condition
- `src/x86_64/x64_codegen.bsm` — DCE for T_IF with literal condition
- `src/bpp_diag.bsm` — negative line number guard (show ? instead)
- `src/bpp_import.bsm` — moved auto nbuf to function top
- `stb/stbarray.bsm` — arr_truncate
- `docs/bootstrap_manual.md` — corrected: auto inside blocks is supported
- `examples/snake_full.bpp` — new: snake with all game infrastructure

## 2026-04-03 — GPU Text + TrueType + realloc + Module Reorganization

### GPU Text Rendering
- Expanded vertex format from 8 to 16 bytes (added UV coords + use_tex flag)
- Updated Metal shader: vertex reads UV at offset 8-11, use_tex at offset 12; fragment samples texture when use_tex > 0.5, else solid color
- Built 128x64 glyph atlas from 8x8 bitmap font data at GPU init
- Created MTLTexture (R8Unorm), upload via replaceRegion with MTLRegion by-reference
- Texture bound per frame via setFragmentTexture:atIndex:
- Added `render_text(text, x, y, sz, color)` and `render_number(x, y, sz, color, val)` to stbrender.bsm
- Atlas size passed as uniforms (sc[2], sc[3]) for dynamic atlas support

### Pure B++ TrueType Font Reader (~500 lines in stbfont.bsm)
- TrueType table directory parser (head, hhea, maxp, hmtx, loca, glyf, cmap, kern)
- Cmap Format 4 decoder (Unicode codepoint → glyph index)
- Glyf outline loader (flag-compressed points, delta-encoded coords, compound glyphs, implicit on-curve midpoints)
- Quadratic Bézier flattener (recursive subdivision)
- Scanline rasterizer with 4x vertical oversampling for antialiasing
- Atlas builder: pre-renders ASCII 32-127, row-based packing
- `font_load(path, pixel_height)` — loads any .ttf, rasterizes atlas
- `render_font(path, height)` — loads TTF + uploads atlas to GPU
- `render_text()` auto-dispatches: TrueType metrics when loaded, 8x8 bitmap fallback otherwise
- Tested with Arial.ttf (773KB, 3381 glyphs): correct rendering on GPU

### realloc with Header (Bell Labs Style)
- malloc now allocates size+8, stores requested size in 8-byte header, returns ptr+8
- free reads size from header at ptr-8, does real munmap (was no-op before)
- realloc(ptr, new_size) — standard 2-arg API (was broken 3-arg). Reads old_size from header, allocates new block, copies min(old, new) bytes
- All 3 backends updated: ARM64 (mmap+munmap), x86_64 (mremap with mmap fallback), C emitter (native libc)
- Bootstrapped gen2 + gen3 successfully
- Cleaned up file_read_all workaround (now uses realloc natively)

### Module Reorganization
- Created `stbcolor.bsm` — color palette (rgba(), BLACK, WHITE, RED...) as independent module
- Moved `_stb_font` global from stbdraw.bsm to stbfont.bsm
- stbrender.bsm no longer imports stbdraw.bsm (GPU and CPU renderers fully independent)
- stbdraw.bsm imports stbcolor.bsm + stbmath.bsm (fixed missing Vec2 import)
- stbgame.bsm imports stbcolor.bsm, calls init_color()

### stbimage.bsm DEFLATE Fix
- Fixed Huffman table naming inconsistencies in _inflate_huffman, _inflate_fixed_tables, _inflate_dynamic_tables
- Replaced broken manual save/restore with _zh_save_lit()/_zh_save_dst()/_zh_use_lit()/_zh_use_dst() helpers
- PNG loader fully working: tested 4x4 red PNG → W=4, H=4, R=255, G=0, B=0, A=255

### Bug Fixes
- file_read_all: realloc didn't copy data (B++ realloc was 3-arg but called with 2). Fixed by realloc header.
- _ttf_scale declared as `: float` but used as fixed-point integer — produced garbage glyph metrics. Removed float hint.
- stbdraw.bsm missing `import "stbmath.bsm"` (Vec2 undefined error)

### Files Changed
- `stb/_stb_platform_macos.bsm` — vertex 8→16, shader UV+texture, atlas creation+upload, texture bind, _stb_gpu_upload_atlas()
- `stb/stbrender.bsm` — render_text (TrueType+bitmap), render_number, render_measure, render_font, all vertex callers updated
- `stb/stbfont.bsm` — TrueType reader: table parser, cmap, glyf loader, rasterizer, atlas builder, font_load(), font_loaded()
- `stb/stbcolor.bsm` — NEW: rgba() + color palette constants
- `stb/stbdraw.bsm` — removed color/rgba (moved to stbcolor), removed _stb_font (moved to stbfont), added imports
- `stb/stbgame.bsm` — added stbcolor import + init_color()
- `stb/stbfile.bsm` — file_read_all realloc fix, then cleaned to use native realloc
- `stb/stbimage.bsm` — DEFLATE Huffman naming fix
- `src/aarch64/a64_codegen.bsm` — malloc header, free munmap, realloc 2-arg with copy
- `src/x86_64/x64_codegen.bsm` — malloc header, free munmap, realloc 2-arg with mremap+fallback
- `src/bpp_emitter.bsm` — realloc 2-arg (C emitter)

## 2026-04-06 — B++ 2.0: Tilemap, Physics, Address-of, Kenney Assets

### Milestone
B++ 2.0. Three new stb modules, a new compiler feature, PNG transparency, and a platformer running with real pixel art assets on Metal GPU.

### stbtile.bsm — Tilemap Engine (NEW — module #22)
- `tile_new(w, h, tw, th)` — create tilemap with any grid and tile dimensions.
- `tile_get/tile_set` — byte-level tile access, bounds-checked, returns 0 for OOB.
- `tile_solid(tm, type)` / `tile_is_solid` — collision layer via 256-entry mask.
- `tile_collides(tm, px, py, pw, ph)` — AABB vs solid tiles. Lifted from platformer's inline code, made generic.
- `tile_at(tm, px, py)` — tile type at pixel position (auto grid conversion).
- `tile_load_set(path, tw, th, &ts_out)` — loads PNG tileset via stbimage, cuts into individual GPU textures (1 per tile). No atlas UV — each tile is its own texture. RGBA buffers intentionally leaked (Apple Silicon async pattern).
- `tile_bind_set(tm, textures, count)` — bind tileset to tilemap.
- `tile_map_type(tm, game_type, tileset_index)` — remap game tile types (T_GROUND=1) to tileset indices (63). Decouples game logic from tileset layout. Default: identity mapping.
- `tile_draw(tm, cam_x, cam_y, scr_w, scr_h)` — GPU render with camera-aware culling and remap. Falls back to colored debug rects if no tileset bound.
- ~200 lines. Depends on stbimage + stbfile.

### stbphys.bsm — Platformer Physics (NEW — module #23)
- `struct Body { x, y, vx, vy, w, h, on_ground, gravity, jump_vel, move_spd }`.
- Milli-pixel convention: positions in px×1000, velocities in px/sec. `pos += vel * dt_ms` works directly.
- `phys_body(w, h, gravity, jump_vel, move_spd)` — create body with movement parameters.
- `phys_update(b, dt, tm)` — full step: gravity → move_x → move_y (separate axis collision resolution).
- `phys_jump(b)` — impulse if on_ground.
- **Bug fix #1 — snap formula**: original used `new_py / th` (top of body) to find ground tile. Correct: `(new_py + body.h) / th` (bottom of body). Old formula snapped 1 tile too high, causing constant mini-bouncing.
- **Bug fix #2 — ground probe**: `on_ground` lasted exactly 1 frame (set on collision, cleared next frame). Added 1px-below probe: `tile_collides(tm, px, new_py + 1, w, h)` keeps on_ground=1 while standing on solid ground. Without this, jump input had <2% chance of catching the on_ground frame.
- ~180 lines. Depends on stbtile + stbmath.

### Address-of Operator `&` (COMPILER FEATURE)
B++ now supports `&variable` to get the memory address of a local or global variable. Eliminates the `malloc(8)` out-parameter workaround that was used throughout the codebase.

Before: `buf = malloc(8); func(buf); val = *(buf); free(buf);` (4 lines, memory leak risk)
After: `func(&val);` (1 line, zero allocation)

**Implementation:**
- New AST node: `T_ADDR = 14` in defs.bsm.
- Parser: added `&` to both `parse_unary()` (for `&x` as operand) and `parse_expr()` (for `&x` at expression start — same dispatch pattern as `-` and `~`). Missing the `parse_expr` dispatch was the bug that took 2 attempts to find.
- x86-64 codegen: `LEA RAX, [RBP + offset]` for locals, `LEA RAX, [RIP + reloc]` for globals.
- aarch64 codegen: `ADD X0, FP, #offset` for locals, `ADRP + ADD` for globals.
- All analysis passes updated: validate, typeck, types (3 locations), emitter, dispatch (2 locations). 9 files total.
- Bootstrap verified: `shasum bpp2 == bpp3`.

**Known limitation (W012):** `&` does not work as an argument to extern FFI functions or `call()` (function pointer calls). The stack address gets clobbered by the calling convention. Workaround: use `malloc(8)` for FFI out-parameters. Compiler emits W012 warning when this pattern is detected.

**Codebase cleanup:** 6 files converted from `malloc(8)` to `&` — stbsprite, stbfont, stbimage (×2), bug_observe_macos (×2), bug_observe_linux (×2). ~30 lines of workaround eliminated.

### stbimage.bsm — tRNS Chunk Support
- PNG palette-indexed images (8-bit colormap) can have per-entry transparency via the tRNS chunk. stbimage did not read this chunk — all palette pixels got alpha=255.
- Added tRNS chunk handler: reads alpha byte array (up to 256 entries), applies during RGBA expansion in `_png_to_rgba`. Palette entries beyond tRNS length default to 255 (opaque).
- Kenney Pixel Platformer sprites now render with correct transparency.

### Metal Shader — Alpha Discard
- Fragment shader's sprite path (`use_tex > 0.25`) now has `if (t.a < 0.01) discard_fragment()`.
- Without this, pixels with alpha=0 (from tRNS) were still composited as black rectangles.
- Combined with tRNS support, character sprites render cleanly on any background.

### Platformer with Kenney Assets
- `games/platformer/platform_noasset.bpp` — GPU rendering with debug rectangles, stbtile + stbphys. 16x16 tiles.
- `games/platformer/platform_assets.bpp` — Kenney Pixel Platformer (CC0) tileset. 18x18 tiles, 20×9 grid (180 tiles). Character sprites 24x24 (9×3 grid, 27 sprites).
- Tile remap: game types (T_GROUND=1, T_DIRT=2...) mapped to tileset indices (63, 123...) via `tile_map_type`.
- Kenney assets downloaded to `games/platformer/assets/` (tilemap_packed.png, tilemap-characters_packed.png).
- Parallax background, scrolling camera, HUD with score + coins, death/restart.

### W012 Diagnostic
- New compiler warning: `'&' used as argument to extern/call() — may crash at runtime`.
- Fires when T_ADDR node appears as argument to an extern function or `call()` built-in.
- Added to bpp_validate.bsm in the T_CALL validation section.
- Discovered after `mach_timebase_info(&tb)` and `call(..., &err)` caused segfaults in the Metal platform init. The stack address computed by `&` is correct for B++ function calls, but the codegen path for `call()` and extern FFI does not preserve it across the calling convention.
- Workaround: use `malloc(8)` for FFI out-parameters. Native B++ functions (`file_read_all(path, &size)`, etc.) work correctly with `&`.
- Future fix: make the codegen path for extern/call() arguments save-and-restore registers before evaluating argument expressions, so LEA-produced stack addresses survive across nested evaluations.

### Snake ECS Particle Bug (found while testing on GPU)
Symptom: snake_full.bpp had working yellow particles on apple eat using software rendering. After migrating to GPU (snake_gpu.bpp in games/snake/), particles became invisible.

Root cause: two bit-width bugs in `update_particles`. The flags field packs `color << 8 | life` — but color is 32-bit ARGB (alpha in top byte), and the code used 24-bit masks that lost the alpha:

```
ecs_set_flags(world, i, (flags & 0xFFFFFF00) + life - 1);  // BUG: 24-bit mask drops alpha
color = (flags >> 8) & 0xFFFFFF;                            // BUG: same mask drops alpha
```

Worked by accident on CPU: `draw_rect` software path didn't use alpha for blending, so RGB was visible regardless. On GPU, Metal multiplies by alpha and discards pixels with alpha < 0.01 (the fragment shader change we did today for Kenney sprite transparency) — particles with alpha=0 became completely invisible.

Fix: decrement the life byte directly with `flags - 1` (safe because life > 0 in the branch), and extract color with `flags >> 8` (no mask needed — life is the only thing below).

Key lesson: "it worked before" is a lie when changing backends. Implicit assumptions about alpha handling (or any other blend state) leak through.

### Files Changed
- `src/defs.bsm` — T_ADDR = 14
- `src/bpp_parser.bsm` — `&` in parse_unary + parse_expr
- `src/x86_64/x64_codegen.bsm` — T_ADDR codegen (LEA)
- `src/aarch64/a64_codegen.bsm` — T_ADDR codegen (ADD)
- `src/bpp_validate.bsm` — T_ADDR validation + W012
- `src/bpp_typeck.bsm` — T_ADDR type check
- `src/bpp_types.bsm` — T_ADDR type inference (×3 locations)
- `src/bpp_emitter.bsm` — T_ADDR C emitter + scan_calls
- `src/bpp_dispatch.bsm` — T_ADDR contains_var + has_side_effects
- `stb/stbtile.bsm` — NEW: tilemap engine
- `stb/stbphys.bsm` — NEW: platformer physics
- `stb/stbimage.bsm` — tRNS chunk + `&` cleanup
- `stb/stbsprite.bsm` — `&` cleanup
- `stb/stbfont.bsm` — `&` cleanup
- `stb/_stb_platform_macos.bsm` — alpha discard in shader
- `src/bug_observe_macos.bsm` — `&` cleanup
- `src/bug_observe_linux.bsm` — `&` cleanup
- `games/platformer/platform_noasset.bpp` — NEW: platformer with stbtile+stbphys
- `games/platformer/platform_assets.bpp` — NEW: platformer with Kenney assets
- `docs/todo.md` — updated roadmap, X11 as priority
- `docs/x11_plan_review.md` — NEW: review notes for X11 agent

---

## 2026-04-05 — GPU Sprites, stbsprite Module, Module Cache Fix, Pure B++ sqrt

### GPU Sprite UV Fix
- Sprite textures were rendering all-red or wrong colors due to Metal uniform buffer timing: CPU writes to a shared MTLBuffer are read by GPU at execution time (after commit), not encode time. Per-draw-call uniform switching via `setVertexBytes` caused SIGKILL.
- Fix: pre-normalize all UVs to 0–65535 range at vertex-emit time. Shader divides by `65535.0` (fixed constant). No runtime per-batch uniform switching needed.
- `_stb_gpu_sprite()` now emits UV `(0, 0, 65535, 65535)` always (full texture).
- Text rendering (bitmap + TrueType) updated: UV coordinates pre-normalized to 0–65535 at `render_text` time.

### stbsprite.bsm — Generic Sprite Module (NEW)
- `load_sprite(path, out, w, h)` — parses JSON sprite files (`{ "data": [[...], ...] }`) into w×h byte buffer of palette indices. Pure B++ JSON parser.
- `sprite_create(spr, pal, w, h)` — converts palette-indexed data to RGBA, uploads to GPU via `_stb_gpu_create_texture`. Returns texture handle. Works with any dimensions (not just 16×16).
- `sprite_draw(tex, x, y, w, h, sz)` — draws sprite at integer scale factor. Calls `_stb_gpu_sprite` with pre-normalized UVs.
- `stbrender.bsm` keeps `render_create_sprite16`/`render_sprite` as backward-compatible 16×16 wrappers.
- 21 stb modules total.

### Pure B++ sqrt in stbmath.bsm
- Added `sqrt(x: float)` using Newton-Raphson iteration (8 iterations, ~15 digits precision).
- Zero FFI — no `System.B` import. Pure B++, consistent with stb zero-dependency philosophy.
- Games no longer need `import "System.B" { double sqrt(double); }`.

### MCU Game Ported to games/pathfinder/
- Ported `mcu-bpp/src/mcu_gpu.bpp` into `games/pathfinder/pathfinder.bpp`.
- Rat-and-cat chase game: WASD movement, diagonal normalization, AI pursuit, collision, particles (ECS), HUD (health bar + score), game-over screen.
- Uses `load_sprite` + `sprite_create` from stbsprite.bsm instead of inline loader.
- Assets: `games/pathfinder/assets/sprites/rat_sprite.json`, `cat_sprite.json`.
- Runs at 60fps on Metal GPU with sprite textures, text rendering, and particle effects.

### Module Cache Bugs Fixed (3 bugs in bpp_bo.bsm)
Critical cache correctness issue discovered: compiling the same program twice produced different binaries. Second compilation (using cached `.bo` files) generated wrong code. Root cause: `bo_read_export()` failed to replicate the full type state that fresh compilation produces.

**Bug 1: Parameter types not registered (`fn_par_types`)**
- `bo_read_export()` read param types from `.bo` but discarded them (`bo_read_u8()` with no assignment).
- `get_fn_par_type()` returned `TY_WORD` for all cached function params, even float params.
- Effect: `sqrt(x: float)` called with integer calling convention → movement code completely broken.
- Fix: store param types in `fn_par_types` array via `arr_set`.

**Bug 2: Return types not registered (`fn_ret_types`)**
- `bo_read_export()` called `set_func_type(name_p, rty)` (name-based table) but not `arr_set(fn_ret_types, idx, rty)` (index-based table).
- `get_fn_ret_type(func_idx)` returned 0 for cached functions.
- Effect: float return values treated as integer in cross-module calls.
- Fix: `arr_set(fn_ret_types, func_cnt - 1, rty)` after growing per-function arrays.

**Bug 3: Missing null terminators in `bo_read_str`**
- `bo_read_str()` copied string bytes into `vbuf` consecutively without null terminators.
- `mo_atof()` (float string → IEEE 754 parser) scans character-by-character until non-digit. Without null terminator, it reads into the next string: "100.0" followed by "3.0" → parsed as "100.03" → wrong float constant.
- Effect: float constant pool had corrupted values. Binary produced different hashes even with identical float table strings.
- Fix: `poke(vbuf + vbuf_pos, 0); vbuf_pos = vbuf_pos + 1;` after each string copy.

**Discovery process:** Bootstrap test (`bpp2 == bpp3`) always passed because different compiler binaries have different `compiler_self_hash`, causing all cache keys to change — the cache was never actually exercised during bootstrap. Game compilation (`bpp game.bpp` twice) exposed the bug because the same `bpp` binary reuses cached `.bo` files.

### Files Changed
- `stb/stbsprite.bsm` — NEW: generic sprite loading, creation, and drawing
- `stb/stbmath.bsm` — added pure B++ `sqrt()` (Newton-Raphson)
- `stb/stbrender.bsm` — UV pre-normalization (text), sprite wrappers delegate to stbsprite
- `stb/_stb_platform_macos.bsm` — shader UV fix (`/65535.0`), `_stb_gpu_sprite()` simplified
- `src/bpp_bo.bsm` — 3 cache bugs fixed: param types, return types, null terminators
- `games/pathfinder/pathfinder.bpp` — NEW: ported MCU game with GPU sprites
- `games/pathfinder/assets/sprites/` — rat and cat sprite JSON files

## 2026-04-06 — B++ 0.21: Linux X11, Validator Integration, C Emitter Modernized

### X11 Wire Protocol on Linux (Phases 1-3)
The Linux platform layer (`stb/_stb_platform_linux.bsm`) grew from 276 to 1160 lines, gaining a full X11 wire-protocol client. Zero FFI, zero shared libraries — raw `sys_socket`/`sys_connect`/`sys_read`/`sys_write` over a Unix domain socket (or TCP for Docker+XQuartz). Mode is auto-selected at init time: if `DISPLAY` is set and an X server is reachable, the platform switches into windowed mode; otherwise it falls back to the existing terminal ANSI rendering — no regression.

**Phase 1** — Connection + window:
- DISPLAY parser reading `/proc/self/environ` (no getenv syscall needed)
- `_x11_resolve_hosts` walks `/etc/hosts` so Docker's `host.docker.internal` mapping works without DNS
- Manual byte-swap of `sin_port` because B++'s `write_u16` is little-endian and TCP expects big-endian
- 12-byte connection setup, vendor-string parsing, pixel format validation
- CreateWindow with `event_mask = 2261071` (FocusChangeMask = 2097152 included so FocusOut actually fires)
- InternAtom + ChangeProperty to register WM_DELETE_WINDOW
- WM_NAME via the predefined STRING atom (39) so window managers display a title

**Phase 2** — Software rendering:
- CreateGC + XPutImage (opcode 72)
- Critical insight: stbdraw's framebuffer is already in BGRA byte order on a little-endian host (because `write_u32(fb, off, ARGB)` lands as `[B, G, R, A]` in memory). Direct copy, no swizzle — first attempt was double-swapping R↔B and produced inverted colors.

**Phase 3** — Input:
- Event drain via `sys_ioctl(fd, FIONREAD, &avail)` using the new `&` operator (no malloc scratch)
- Full event dispatcher: error (type 0), KeyPress, KeyRelease, ButtonPress, ButtonRelease, MotionNotify, FocusOut (clears `_stb_keys` to fix stuck keys after Alt-Tab), Expose, ClientMessage (WM_DELETE_WINDOW)
- **evdev/Apple keycode auto-detect**: Xorg uses evdev codes (arrows = 111/116/113/114), XQuartz forwards Apple ADB codes (arrows = 134/133/131/132). Both servers report `vendor = "The X.Org Foundation"`, so vendor-string detection is impossible. Solution: try evdev first, fall back to Apple, lock into Apple mode the first time an Apple-only code is recognized. Verified working with Xvfb (evdev path) via xdotool injection.
- Helpers `_x11_err_str`/`_x11_err_dec` write to stderr via `putchar_err` for protocol error logging.

**Snake on Linux**: `bpp --linux64 examples/snake_cpu.bpp -o /tmp/snake_x11`, then `docker run --rm --add-host host.docker.internal:host-gateway -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp ubuntu:22.04 /tmp/snake_x11` opens the game window in XQuartz with full input — first B++ game running on Linux outside of the terminal.

### Validator Integrated into Incremental Pipeline
`run_validate()` had been monolithic-only since the validator was written, meaning the ARM64 and x86_64 native backends never ran the strict semantic checks (E050 ptr mismatch, E201 undefined function, W002/W003 arg count, etc.). Only the `--c` and `--asm` paths caught these. Snake bug reports stayed buried.

Fix: `run_validate()` now runs at the end of the incremental pipeline, after every module (including module 0) has been parsed and inferred, just before the final binary linking. A per-module variant `validate_module(mi)` is also exported for future use, but the loop iterates module indices in raw order — not topological — so per-module validation would fire before the dependencies of the module had been parsed. The end-of-flow approach side-steps this.

### E052 Removed
The validator's `float-to-word` check (E052) rejected the canonical pattern:
```bpp
auto lx: float, ix: int;
ix = lx / 3.0;        // E052 fired here
```
…even though the explicit `: int` hint **is** the cast. Native backends already emit `fcvtzs` implicitly for this case, and the C emitter relies on C's implicit `double → long long` truncation. The check was a phantom — it only ever fired in monolithic mode, and only against legitimate explicit conversions. Removed entirely.

### `bpp_typeck.bsm` Deleted
A 507-line orphan file from a never-finished plan. No imports, no callers, dead since it was created. Removed.

### C Emitter Modernized
Eight builtins were missing from `bpp_emitter.bsm` after they had been added to the native backends:
- `putchar_err` → `bpp_sys_write(2, ...)`
- `assert` → `(cond) ? 0 : (abort(), 0)`
- `shr` → `((uint64_t)(value)) >> shift`
- `float_ret` / `float_ret2` → reads global `_bpp_float_ret_1/2`
- `sys_ioctl`, `sys_nanosleep`, `sys_clock_gettime` → libc wrappers

`#include <sys/ioctl.h>` and `<time.h>` added to the C preamble. Two static `double _bpp_float_ret_1/2` slots added to the memory model.

**Extern dedup**: B++ source can declare the same FFI name multiple times with different signatures (notably `objc_msgSend` on macOS). C does not allow overloaded forward declarations. Fix: `emit_includes()` walks the extern table once per unique name, and when a name has more than one signature it appends `, ...` varargs so the single declaration accepts any call shape. **Caveat**: varargs ABI on arm64-darwin pushes args to the stack instead of registers, which breaks `objc_msgSend` if you actually call it through the C output. The right path for Cocoa via the C emitter is function pointer casts at the call site (deferred). For portable C output via raylib/SDL drivers, the dedup is fine because those drivers do not call objc_msgSend.

**libc symbol skip**: `is_libc_symbol(name_p)` returns 1 for POSIX names already declared by the headers we include (`usleep`, `sleep`, `nanosleep`, `clock_gettime`, `ioctl`, `read`, `write`, `open`, `close`, `lseek`, `fchmod`, `mkdir`, `unlink`, `fork`, `execve`, `waitpid`, `wait4`, `ptrace`, `_exit`, `socket`, `connect`, `mmap`, `munmap`, `malloc`, `free`, `realloc`, `abort`). These extern declarations are skipped to avoid conflicts with the real prototypes (which differ — `usleep` takes `useconds_t` on macOS, not `int`).

### C Emitter Verified End-to-End
- `bpp --c games/snake/snake_gpu.bpp > snake_gpu.c; gcc -lobjc snake_gpu.c -o snake_gpu_c` produces a 96 KB Mach-O. Runs but crashes inside `_stb_init_window` because of the varargs ABI mismatch on `objc_msgSend` — known limitation, deferred to future work.
- `bpp --c examples/snake_raylib.bpp > snake_raylib.c; gcc -I/opt/homebrew/include -L/opt/homebrew/lib -lraylib -lobjc snake_raylib.c -o snake_raylib_c` produces a 94 KB Mach-O that **runs the game correctly via raylib**. This is the canonical "portable C output" path: write the game with `drv_raylib.bsm` instead of `stbgame.bsm`, emit C, link against raylib, run anywhere raylib runs (macOS, Linux, Windows, BSDs).

### Architectural Rule Confirmed
Two clean paths emerge:
- **Native binary** (`bpp foo.bpp`): import `stbgame.bsm`, talks to Cocoa/Metal via dlopen + objc_msgSend. Native ABI is correct because the backends know the exact calling convention per call site. Zero external tools.
- **Portable C output** (`bpp --c foo.bpp`): import `drv_raylib.bsm` or `drv_sdl.bsm`. Generated C calls regular C functions with fixed signatures — no objc_msgSend, no varargs hell. Compile with gcc + raylib/SDL on the target platform.

Mixing them (`stbgame.bsm` through `--c`) is the path that hits the objc_msgSend varargs problem and is the one not currently usable.

### Files Changed
- `stb/_stb_platform_linux.bsm` — 276 → 1160 lines, X11 wire protocol added
- `tests/test_x11_window.bpp` — NEW (Phase 1 verification)
- `tests/test_x11_draw.bpp` — NEW (Phase 2 verification)
- `tests/test_x11_input.bpp` — NEW (Phase 3 verification)
- `tests/test_x11_movement.bpp` — NEW (automated input test via xdotool)
- `tests/test_x11_snake_min.bpp` — NEW (input isolation)
- `docs/x11_session_notes.md` — NEW (implementation log)
- `src/bpp_emitter.bsm` — 8 builtins added, `is_libc_symbol`, extern dedup with varargs
- `src/bpp_validate.bsm` — `validate_module(mi)` added, E052 removed, builtin recognition extended
- `src/bpp.bpp` — `run_validate()` now runs after module 0 in incremental mode
- `src/bpp_typeck.bsm` — DELETED (507-line orphan)
- `docs/todo.md` — version 0.21, ELF dynamic linking promoted to P0, P4 Vulkan revised

---

## 2026-04-06b — B++ 0.21 (continued): stbhash, stbpath, O(1) Compiler Symbols, Auto-Platform

### Milestone
The afternoon and evening of 2026-04-06 turned the morning's tilemap+physics+X11 release into a more complete 0.21: two new pure-B++ data structures (a hash map and an A* pathfinder), six O(n) lookups in the compiler replaced with O(1) hash queries, the platform-layer indirection removed from `stbgame.bsm` and turned into a compiler-level auto-injection, all platform/observe sources moved into the appropriate chip folders, the pathfinder game refactored end-to-end onto the milli-unit convention, and the test suite cleaned of legacy drift (71 → 52 tests, 100% passing).

### sys_socket / sys_connect / sys_usleep validator fix
First problem of the day. `install.sh` was failing in the "compile debugger" step:
```
error[E201]: function 'sys_socket' called but never defined -- missing import?
```
Discovery story: the validator's `val_is_builtin()` enumerates every B++ builtin by name. Three names — `sys_socket`, `sys_connect`, `sys_usleep` — were present in `src/x86_64/x64_codegen.bsm` and `src/aarch64/a64_codegen.bsm` but **never added to the validator list**. The bug had existed since these syscalls were introduced, but stayed hidden because the `.bo` cache held cached versions of `bug_observe_*` and validation was skipped for cached functions. Cache cleanup (or fresh checkout) exposed it.

Fix: 4 lines in `bpp_validate.bsm`. Bootstrap-verified, promoted.

Lesson: codegen and validator are two parallel lists that must stay in sync. A cross-grep test would catch this — added to the cleanup ideas.

### stbhash.bsm — Generic hash maps (NEW — module #25)
Pure B++ hash map. Two flavors share the file:
- **`Hash`** (word keys → word values): Knuth multiplicative hash, open addressing, linear probing, tombstone-aware delete, 75%-load resize.
- **`HashStr`** (byte-sequence keys → word values): djb2 hash over the key bytes, copies keys on insert (caller's source buffer can be freed immediately).

API for both: `*_new`, `*_set`, `*_get`, `*_has`, `*_del`, `*_clear`, `*_count`, `*_free`. ~610 lines, zero dependencies, leaf module.

Designed primarily because the compiler needed it (see the symbol-table refactor below), but the API is generic enough for any program: entity ID lookup, asset cache, frequency counting, sparse arrays, string interning.

`tests/test_hash.bpp` exercises both flavors with 49 assertions including a 100-key resize stress.

### stbpath.bsm — A* pathfinding (NEW — module #26)
Pure B++ A* on a grid. Three design choices that took deliberate discussion:

**1. Leaf module, no stbtile dependency.** First draft imported `stbtile.bsm` for a `path_load_from_tilemap` convenience function. That dragged the entire image-loading and GPU-texture stack into anyone who wanted to use A* on a grid — including unit tests. Fixed by deleting the bridge function and documenting the 5-line tilemap → PathFinder loop as an inline snippet in the header comment. stbpath is now a true leaf, importable from puzzle solvers, AI planners, and the compiler test suite.

**2. Binary min-heap with indexed `heap_pos[]` for true decrease-key.** Initial draft used a linear-scan open list — O(n) per pop, O(n²) per query, breaks down at ~100×100 grids. Replaced with a binary heap (parent at `(i-1)/2`, children at `2i+1`/`2i+2`). Each cell's position in the heap is tracked in `heap_pos[]`, so when a relaxation finds a better g-score for a cell already in the heap, we sift it up in O(log n) — no lazy deletion, no stale pops. Total search complexity: O((w·h) · log(w·h)).

**3. Half-word scores rejected.** Considered storing `g_score`/`f_score` as half-word (16-bit) for 50% memory savings, but the max value of 65535 limits the implementation to grids around 256×256 before silent overflow. The footgun outweighs the savings. Scores stay word-sized; the future answer for huge grids is a separate `stbpath_sparse.bsm` using `stbhash` for the visited set (P2 todo).

~440 lines. `tests/test_path.bpp` covers seven scenarios: open-grid straight line, trivial start==goal, U-shaped wall detour, unreachable goal, blocked endpoints, out-of-bounds endpoints, scratch reuse across queries.

### Compiler symbol-table refactor: 6 lookups → O(1)
Audited the compiler for the `for (i = 0; i < *_cnt; ...)` symbol-lookup pattern and found six call sites:

| Function | File | Strategy | Reason |
|---|---|---|---|
| `val_find_func` | bpp_validate.bsm | Eager rebuild at `run_validate`/`validate_module` entry | funcs[] stable during validation pass |
| `val_find_extern` | bpp_validate.bsm | Eager rebuild | Same reason |
| `find_func_idx` | bpp_types.bsm | Eager rebuild at `run_types`/`infer_module` entry | Called from BOTH type inference AND codegen, eager works for both |
| `find_struct` | bpp_parser.bsm + bpp_bo.bsm | **Incremental update** at insertion sites | Structs added DURING parsing, lookups happen DURING parsing — interleaved, lazy would be O(n²) |
| `is_extern` | bpp_emitter.bsm | Lazy rebuild on stale | C emitter is a cold path, simplicity wins |
| `find_extern` | bpp_emitter.bsm | Lazy rebuild (shares hash with `is_extern`) | Same reason |

The choice between **eager**, **lazy**, and **incremental** hash sync was the most interesting design conversation of the session. Each strategy has a real trade-off:

- **Eager** (rebuild at phase entry): predictable O(n) once per pass, no per-lookup overhead, but the caller must remember to rebuild at the right places. Works when phase boundaries are clear.
- **Lazy** (rebuild on stale check inside the lookup): self-contained, no caller cooperation needed, but adds a branch on every lookup and has a pathological worst case if inserts and lookups are interleaved. Best when the lookup function is the only entry point and the table is mostly stable.
- **Incremental** (update hash at every insertion site): zero rebuild cost ever, lookups are pure O(1), but **requires a complete audit of every insertion site** — miss one and lookups fail silently for the missed entries. Best when the insertion sites are few and well-known.

The `find_struct` choice almost broke production. The incremental approach requires updating the hash at every place that calls `arr_push(sd_names, ...)`. There are TWO such places: `add_struct_def` in `bpp_parser.bsm` (the fresh-parse path) AND `bpp_bo.bsm:712` (the cached `.bo` loader, which inlines the struct registration to avoid calling `tok_mod()` during cache load). The first commit only updated the parser path. **Second compilation of the platformer game then failed** with `E101: unknown type 'Body'` because `Body` came from `stbphys.bsm` which was loaded from cache, where the bo loader had pushed it into `sd_names` directly without touching the hash.

Fix: extracted a public helper `register_struct_hash(name_p, idx)` in `bpp_parser.bsm` and called it from BOTH insertion sites. The incremental approach now works, and the helper makes any future insertion site obvious.

Note that `find_func_idx` had the SAME structural risk (`bpp_bo.bsm:696` also pushes to `funcs[]` directly) — but escaped because its strategy is **eager rebuild**, which reads `funcs[]` at the start of each pass and doesn't care who put things there. This is exactly why eager is more forgiving than incremental: it tolerates missed insertion sites by re-deriving from the live state.

Each refactor was bootstrap-verified independently before promotion (`shasum bpp_new == shasum bpp_verify`). Six fixed-point cycles, all passing.

Functional speedup: hard to measure exactly without a benchmark harness, but a fresh bootstrap of `src/bpp.bpp` (~1500 functions, ~10000 call sites in the AST) replaces O(n²) symbol resolution work with O(n) — back-of-envelope ~500x fewer comparisons in the validator and codegen passes.

### Platform/observe files moved into chip folders
Four files moved out of confusing locations:
- `stb/_stb_platform_macos.bsm` → `src/aarch64/_stb_platform_macos.bsm`
- `stb/_stb_platform_linux.bsm` → `src/x86_64/_stb_platform_linux.bsm`
- `src/bug_observe_macos.bsm` → `src/aarch64/bug_observe_macos.bsm`
- `src/bug_observe_linux.bsm` → `src/x86_64/bug_observe_linux.bsm`

`find_file()` in `bpp_import.bsm` extended with four new search paths: `src/aarch64/`, `src/x86_64/`, `/usr/local/lib/bpp/aarch64/`, `/usr/local/lib/bpp/x86_64/`. `install.sh` updated to copy from `src/aarch64/*.bsm` and `src/x86_64/*.bsm` into the new install directories.

**Architectural caveat**: chip and OS aren't the same thing. macOS is OS, aarch64 is chip. They happen to be 1:1 paired in B++ today (Apple Silicon = aarch64+macOS, B++ has no Linux ARM yet) but will diverge when a new combination lands. Each chip folder now has a `README.md` explaining the current chip+OS coupling and documenting the future `arch/` + `os/` + `targets/` split (P3 in `docs/todo.md`).

The codegen files (`a64_codegen.bsm`, `x64_codegen.bsm`) were ALREADY chip+OS bundles — the encoder logic is chip-pure but the binary writer (Mach-O for macOS, ELF for Linux) and syscall numbers are OS-specific. So the platform layer joining the codegen in the chip folder reflects reality: each chip folder is a complete target backend.

### Auto-injection of `_stb_platform.bsm`
Before: `stb/stbgame.bsm` had `import "_stb_platform.bsm";` on line 14, and the import resolver's target-suffix fallback (`_imp_target` = "macos" or "linux") rewrote that to the per-OS file. The indirection worked but exposed an ugly import to anyone reading stbgame.

After: the explicit import is gone from stbgame. `process_file()` in `bpp_import.bsm` detects when the file being processed is `stbgame.bsm` (suffix match via a small `_path_is_stbgame` helper) and auto-pulls `_stb_platform.bsm` into the import graph BEFORE stbgame's own line scan runs. The platform module appears in the topology like any other dependency, the existing target-suffix fallback resolves it to the per-OS file, and stbgame's source no longer mentions the platform layer at all.

The trigger is "stbgame is being imported" rather than "the program references `_stb_*` symbols" — both produce identical results today (every `_stb_*` consumer goes through stbgame), and the import-graph trigger is far cheaper to implement than a symbol-table scan. Future option: switch to a full symbol scan if a non-stbgame consumer ever appears.

`hello.bpp` and other minimal programs that don't import stbgame stay lightweight: no platform layer pulled in, no Cocoa/Metal overhead. The auto-injection costs nothing for programs that don't need it.

### pathfind.bpp refactor: milli-unit + tilemap + A*
The morning's pathfinder game was running fine but emitted four W002 warnings on the `rect_overlap` call:
```bpp
if (rect_overlap(rat_x, rat_y, rat_size * 1.0, rat_size * 1.0, ...))
```
The `* 1.0` round-trip was promoting integer `rat_size` to float just to be truncated back to int inside `rect_overlap`. Dead weight, plus the warning was the compiler nudging at a deeper issue: `rat_x` and `rat_y` were `auto rat_x: float`, **the old game-state convention from before stbphys.bsm existed**.

Rewrote the game end-to-end to match the new milli-unit convention from stbphys/stbecs:
- All positions in `int * 1000` (milli-pixels)
- All velocities in pixels/sec (int)
- `pos = pos + vel * dt_ms` works directly in integer math, no float in game state at all
- Pixel position recovered with `pos / 1000` only at draw time and tile lookup time

Then layered the new modules on top:
- `stbtile.bsm` provides the arena: a 20×11 tilemap with two vertical walls, a horizontal stripe, and a couple of obstacles. Walls are tile type 1, marked solid via `tile_solid`.
- `stbpath.bsm` provides the cat's brain: every frame, run A* from the cat's tile to the rat's tile, walk one step toward the next waypoint along the returned path. The cat now navigates around walls instead of just pointing at the rat.
- The `* 1.0` round-trips in `rect_overlap` are gone (rat/cat sizes are passed as plain ints).
- The leftover `rat row0:` debug print at line 199 is gone.

The game compiles with **zero warnings** and runs at 60 fps on Metal GPU. The cat actually pathfinds.

### Test suite cleanup: 71 → 52, 100% passing
Audited every test file in `tests/`. Found 20 failing — none of them regressions from the day's work. All were legacy drift from earlier B++ eras:
- Seven `test_io*` tests using `print_str`, `pstr`, `io_print_str`, `eprint_msg` — all functions removed long ago. Path was also wrong (`import "stb/stbio.bsm"` instead of `import "stbio.bsm"`).
- `test_types.bpp` with `byte lives;` (pre-base×slice syntax), `test_init2.bpp` and `test_import.bpp` calling the removed `init_defs()`, `test_q.bpp` with the never-shipped quoteless `import stbio;` syntax.
- `test_enc_arm64.bpp` importing `aarch64/a64_enc.bsm` (wrong path), `test_getdents.bpp` hardcoding `/Users/obino/.bpp/cache`.
- `test_debug_bin.bpp` and `test_native_debug2/3/4.bpp` importing many internal compiler modules — scratch dev programs, not regression tests.
- `test_stbdraw.bpp` redefining `_stb_present` as a no-op, which collided with the auto-imported platform layer.
- `test_linux_exit.bpp` returning `42` intentionally as a cross-compile smoke test — not a real regression test.

All 19 deleted (one file kept and rewritten: `tests/test_io.bpp` is now a clean exercise of the current `print_msg`/`print_int`/`print_char`/`print_ln` API). Re-ran the suite: **52/52 passing**.

### Files Changed
- `src/bpp_validate.bsm` — 3 syscalls added to builtin list, hash refactor of val_find_func/val_find_extern, eager rebuild in run_validate/validate_module
- `src/bpp_types.bsm` — hash refactor of find_func_idx, eager rebuild in run_types/infer_module
- `src/bpp_parser.bsm` — incremental hash for find_struct, register_struct_hash helper exposed
- `src/bpp_bo.bsm` — register_struct_hash call added at the cached struct insertion site
- `src/bpp_emitter.bsm` — lazy hash for is_extern/find_extern
- `src/bpp_import.bsm` — find_file gains src/aarch64, src/x86_64 + global install paths; process_file auto-injects _stb_platform when stbgame is imported
- `stb/stbgame.bsm` — `import "_stb_platform.bsm"` removed (now compiler-injected)
- `stb/stbhash.bsm` — NEW: word + byte-keyed hash maps, ~610 lines
- `stb/stbpath.bsm` — NEW: A* with binary min-heap + indexed decrease-key, ~440 lines
- `src/aarch64/_stb_platform_macos.bsm` — moved from stb/
- `src/aarch64/bug_observe_macos.bsm` — moved from src/
- `src/aarch64/README.md` — NEW: chip+OS coupling note
- `src/x86_64/_stb_platform_linux.bsm` — moved from stb/
- `src/x86_64/bug_observe_linux.bsm` — moved from src/
- `src/x86_64/README.md` — NEW: chip+OS coupling note
- `install.sh` — copies from src/aarch64 and src/x86_64 into per-target install dirs
- `games/pathfind/pathfind.bpp` — full rewrite: milli-unit positions, tilemap arena, A* cat AI, removed `* 1.0` round-trips and debug print
- `tests/test_hash.bpp` — NEW: 49 assertions, both Hash and HashStr
- `tests/test_path.bpp` — NEW: 7 A* scenarios
- `tests/test_io.bpp` — rewritten for current stbio API
- `tests/test_io2.bpp`, `test_io4.bpp`, `test_io6.bpp`, `test_io7.bpp`, `test_io8.bpp`, `test_io_simple.bpp`, `test_q.bpp`, `test_stbio_q.bpp`, `test_types.bpp`, `test_init2.bpp`, `test_import.bpp`, `test_enc_arm64.bpp`, `test_getdents.bpp`, `test_debug_bin.bpp`, `test_native_debug2.bpp`, `test_native_debug3.bpp`, `test_native_debug4.bpp`, `test_stbdraw.bpp`, `test_linux_exit.bpp` — DELETED (legacy drift)
- `docs/todo.md` — Done section reorganized by category, P0 stbaudio + host-aware compiler, P2 stbpath/stbhash done, P3 target architecture refactor

---

## 2026-04-07 / 2026-04-08 — Foundation Cleanup + Maestro Plan Phase 1 + 4 Compiler Bug Fixes (0.22)

A long, deliberate session that started with planning the maestro pattern and ended with B++ having a substantially cleaner foundation than it had at the start of 0.21. The headline is the **8 module renames** that resolved the long-standing ambiguity about what is a compiler tool versus what is a user library, but the most important outcomes were structural: **4 latent compiler bugs** that we discovered and fixed while working on the maestro plan, the **`global` keyword** as the first foundation stone of smart dispatch, and `bpp_dispatch.bsm` finally getting wired into the modular pipeline (Constraint 1 of the plan).

The session was unusually philosophical for a coding day. Several decisions that look trivial in the diff (renaming `defs.bsm` to `bpp_defs.bsm`, picking which storage class keywords to add) actually went through extended design discussions before the user committed to a direction. Those discussions are worth recording because they explain WHY the foundation looks the way it does now.

### The maestro plan came together first

The day opened with finalizing `docs/maestro_plan.md` (~1000 lines), the canonical reference for the parallel infrastructure work. The plan's key framing — borrowed from the user — is the **jazz/rock ensemble metaphor**, which turned out to be more honest than the orchestra metaphor I'd originally proposed:

- **Base** (the rhythm section): pure functions that run in parallel on worker threads. Bass, drums, harmony — the deterministic foundation.
- **Solo** (the improvisers): mutable functions that run sequentially on the main thread. They react to player input, mutate state, decide branches in runtime.
- **Beat** (the clock): a shared monotonic clock that both phases read independently. Coordinates without locking.
- **Maestro** (the bandleader): cues base in, cues solo in, manages tempo. Doesn't play an instrument — coordinates.

The orchestra metaphor failed because in a real orchestra everyone follows the score; nobody improvises. Jazz and rock are honest about the rhythm-section-vs-soloist split.

The plan ships in three phases. Phase 1 is manual classification (the user explicitly registers `solo`, `base`, `render` callbacks via API). Phase 1.5 is the slice-type sweep across the existing stb library. Phase 2 (now refactored, see below) is real smart dispatch with compile-time analysis. The plan also identifies six structural constraints that the implementation has to respect — the most important being Constraint 1 (`bpp_dispatch.bsm` is currently SKIPPED in the modular pipeline) and Constraint 6 (Linux pthread is blocked on ELF dynamic linking, which is its own multi-day milestone deferred outside this work).

A peer review during planning produced seven concrete changes (A through G) that all got folded in: pthread via FFI instead of raw syscall wrappers, N×SPSC queues instead of SPMC + CAS, calling-convention bridge test before stbjob, `.bo` cache extension for `fn_phase[]`, `tests/run_all.sh` as commit 0, `tests/test_slice_struct.bpp` before the slice sweep, and dropping the runtime `func_purity()` builtin in favor of compile-time validation. These are all in the plan now.

### Maestro Plan Phase 1 — six commits, all green

The implementation followed the plan's commit ordering exactly:

1. **`tests/run_all.sh` (commit 0)** — 130-line POSIX shell runner that compiles and runs every `tests/test_*.bpp` file with platform-aware skip rules and a 10-second wall-clock guard per test. Wired into `install.sh` (later reverted from install.sh per the user's "you shouldn't have touched install" feedback — kept the script but install.sh stays clean of test invocation). Skip categories: not-linux (X11/linux_*), not-macos (macho_*), no-display (GPU + window tests when DISPLAY unset), interactive (debugger smoke tests).

2. **`stb/stbbeat.bsm` (commit 1)** — universal monotonic clock. `init_beat`, `beat_now_ms/us/ns`, `beat_now_at_rate(hz)`, `beat_now_frames(fps)`, `beat_now_samples(rate)`, `beat_now_in_bpm(bpm)`, `beat_reset`. Initially had no compiler change, but discovered immediately that the platform layer can't be imported standalone (it has hidden coupling to stbgame globals like `_stb_fb`), so stbbeat had to import stbgame. This coupling stayed as a "temporary" workaround through the rest of Phase 1 — the user later called it out as architecturally wrong, see "Batch 2" at the end of this entry.

3. **`mem_barrier()` builtin (commit 2)** — the only new compiler builtin in Phase 1. Emits ARM64 DMB ISH (`0xD5033BBF`) and x86_64 MFENCE (`0F AE F0`). Full bidirectional fence used by stbjob's SPSC queue ordering. C emitter equivalent: `__sync_synchronize()`. Bootstrap shasum changed from `63a96850...` to `3f5068e43...`.

4. **pthread FFI in macOS platform layer (commit 3)** — declared `pthread_create`/`pthread_join` via libSystem (already linked, no flag needed), added `_stb_thread_create`/`_stb_thread_join` wrappers that present a clean `long long(long long)` entry-point interface. Validated by `tests/test_thread.bpp` — Constraint 5 of the plan. **The B++ `long long(long long)` ABI is bit-compatible with pthread's `void*(void*)` ABI on Apple Silicon**, as predicted; no trampoline needed.

5. **`stb/stbjob.bsm` (commit 4)** — N×SPSC worker pool with round-robin submit. `job_init` spawns N pthreads (each on its own private SPSC queue), `job_submit` round-robins onto a worker queue, `job_wait_all` polls done counters per worker, `job_parallel_for` is the workhorse, `job_shutdown` sentinel-and-joins. **No CAS** — each queue has exactly one producer (main) and one consumer (the dedicated worker), so memory ordering via `mem_barrier()` is sufficient. `tests/test_job.bpp` validated correctness with 100 + 10000 jobs through a 4-worker pool. **First parallel B++ workload running on real OS threads.**

6. **`stb/stbmaestro.bsm` (commit 5)** — solo/base/render bandleader. `maestro_configure(w, h, title, fps)`, `maestro_register_init/solo/base/render/quit(fn_ptr)`, `maestro_run()`. The implementation owns the frame timing, the quit flag, and the scheduling of base work onto workers. `tests/test_maestro.bpp` validated 5 ticks of solo→base→render with parallel base phase dispatching `job_parallel_for(8, fn)`.

7. **`games/snake/snake_maestro.bpp` (commit 6)** — port of `snake_gpu.bpp` reorganized into solo/base/render phases. Particle physics dispatched to a worker thread; render reads results after maestro auto-calls `job_wait_all`. `snake_gpu.bpp` stays untouched alongside as the side-by-side baseline for visual validation.

### Bug 1: pathbuf clobber in bpp_import.bsm — latent since 0.21

The first compiler bug surfaced when the test runner sequence `test_job → test_maestro` started failing with `error[E201]: function 'job_parallel_for' called but never defined`. Standalone `test_maestro` worked. Standalone `test_job` worked. Only the sequence broke.

The discovery story took most of an hour. First hypothesis: cache hash collision. Compared `--show-deps` output for both compile contexts and found something genuinely strange:

```
[STALE] tests/test_maestro.bpp
  -> tests/test_maestro.bpp        ← self-import??
  -> stb/stbjob.bsm
[clean] stb/stbmaestro.bsm
  -> tests/test_maestro.bpp        ← stbmaestro depends on test_maestro??
  -> stb/stbgame.bsm
```

Backwards edges. Self-loops. Modules pointing at module 0 (the entry file) that they had no business depending on. Three of stbmaestro's four dependencies were spuriously rewritten to point at `tests/test_maestro.bpp`.

Following the corrupted dep_to values back through `bpp_import.bsm`, the bug was in the module-index lookup that records the dep edge after recursing into the imported module. The code:

```bpp
plen = find_file(fbuf + my_path, path_len, linebuf, fname_len);  // sets pathbuf
if (plen > 0) {
    process_file(pathbuf, plen);                                    // RECURSES, may overwrite pathbuf
    dep_from = arr_push(dep_from, my_mod_idx);
    dep_to = arr_push(dep_to, mod_idx_by_hash(pathbuf, plen));      // pathbuf is now stale!
```

`pathbuf` is shared scratch — `find_file()` reuses it on every nested import resolution. By the time `mod_idx_by_hash(pathbuf, plen)` ran post-recursion, `pathbuf` contained whatever path the deepest nested `find_file()` had written last. The hash didn't match anything in `imp_hashes`, so `mod_idx_by_hash` returned its default-fail value of `0` — which happens to be the entry-file index. Every affected dep edge spuriously pointed at module 0.

The blast radius was severe and disguised the symptom completely:

1. Corrupted dep edges → wrong topological sort
2. Wrong topo sort → wrong dep-hash-chain in `hash_with_deps()`
3. Wrong dep hashes → wrong `.bo` cache filename hashes
4. Filename hash collisions across two different modules in two different programs → one program's empty stub `.bo` would satisfy another program's "is module X cached?" check
5. Loading an empty `.bo` for a real module → "function not found" errors with no obvious connection to the import system

The fix is small (~10 lines): copy `pathbuf` to the `fbuf` stack before recursing, look up by the saved copy, restore `fbuf_top` after.

```bpp
auto saved_path, _spi;
saved_path = fbuf_top;
for (_spi = 0; _spi < plen; _spi = _spi + 1) {
    poke(fbuf + saved_path + _spi, peek(pathbuf + _spi));
}
fbuf_top = fbuf_top + plen;

process_file(pathbuf, plen);

dep_to = arr_push(dep_to, mod_idx_by_hash(fbuf + saved_path, plen));

fbuf_top = saved_path;
```

This bug had been **latent since the modular pipeline landed in 0.21**. It was never reproduced before because shallow import trees rarely clobbered `pathbuf` deeply enough to break the lookup. `stbmaestro → stbgame → (10+ stb modules including the platform layer)` was the first import chain in the project deep enough to reliably destroy `pathbuf` before the post-recursion lookup. The maestro plan didn't introduce the bug — it just made it observable.

Saved as `feedback_pathbuf_scratch.md` in agent memory because the lesson generalizes: any code that calls `process_file()` and then uses any shared scratch buffer (`pathbuf`, `linebuf`, etc.) has to copy first. The next refactor in this area should consider renaming the variables to `_scratch_pathbuf` to make the danger visible.

### Bug 2: inode reuse in macho_writer + elf_writer

Different bug, same day. After the maestro plan implementation was working in `/tmp/snake_maestro`, the user reported it didn't work when compiled inside `games/snake/`. Same source, same compiler, different output behavior. After half an hour of confusion, the user did the right thing — confirmed the binary was being killed by `zsh: terminated`, then ran `bug ./snake_maestro` to trace and saw the process getting SIGKILL'd at the very first `_stb_init_window` call.

The clue was: compiling to a fresh filename (`snake_fresh_38144`) worked, recompiling to the existing path (`snake_maestro`) didn't. **macOS code signing cache by inode**. When `bpp -o snake_maestro` truncates and rewrites an existing file via `sys_open(0x601)`, the inode is reused. The kernel still has the OLD ad-hoc code signature cached for that inode and validates the new bytes against the old signature, which always mismatches, and the kernel kills the process at exec.

`install.sh` already worked around this by `sudo rm -f` before `cp`. The compiler now does the same: `sys_unlink` before `sys_open` in both `src/aarch64/a64_macho.bsm:1355` and `src/x86_64/x64_elf.bsm:392`. ~3 lines per writer, plus comments explaining why. **This closes the entire class of "binary mysteriously killed when recompiled" bugs on macOS**, which has been a recurring footgun documented in agent memory as `feedback_macos_sigkill.md`.

Bootstrap-verified, suite green, snake_maestro now compiles cleanly to any path on any compile.

### Bug 3: pthread after NSApplication

The third bug surfaced after fixing bugs 1 and 2. snake_maestro compiled fine but the window opened and froze. `top` showed the process alive at 100% CPU. The `bug` debugger trace stopped at `job_init()` with sig 15 (SIGTERM). Not SIGSEGV, not SIGKILL — SIGTERM. Something was actively killing the process with a graceful termination signal.

The stbjob test (`test_thread.bpp`, `test_job.bpp`, `test_maestro.bpp`) all passed in isolation. The difference between them and snake_maestro: snake_maestro opens a real Cocoa window via `game_init` BEFORE creating pthread workers. The test programs either don't open a window (test_thread, test_job) or open one and exit after 5 ticks before the window can become unresponsive (test_maestro).

Hypothesis: macOS Cocoa has a documented "single-thread → multi-thread transition" requirement. NSApplication needs to be informed that the app has gone multi-threaded BEFORE pthread state interferes with Cocoa. If pthread_create runs after NSApplication is already up, certain Cocoa subsystems (notification center, run loop) can get into a bad state and the OS sends SIGTERM as a graceful "shut down please" signal.

Fix: in `bpp_maestro.bsm`, swap the order in `maestro_run()`. Spawn the worker pool **before** `game_init` opens the window. The workers park in their spin loop and don't touch any user state until a job arrives, so the reorder is safe. Verified by sampling the main thread of a running snake_maestro and confirming it sits in `usleep → __semwait_signal` (the normal frame budget wait) instead of being killed.

### Bug 4: install.sh ghost files

The fourth bug surfaced when the user reported that even after the snake_maestro fix, compiling from games/snake/ still produced a different (broken) binary than compiling from the repo root. Same source, same bpp shasum, different output.

`bpp src/snake_maestro.bpp --show-deps` from games/snake/ showed:

```
[clean] /usr/local/lib/bpp/stb/_stb_platform_macos.bsm
  -> /usr/local/lib/bpp/stb/stbbuf.bsm
  -> /usr/local/lib/bpp/stb/stbstr.bsm
```

`/usr/local/lib/bpp/stb/_stb_platform_macos.bsm`. **Ghost file from before the 0.21 platform-layer-move-to-aarch64**. The new platform layer in `/usr/local/lib/bpp/aarch64/` has the pthread FFI declarations; the old one in `/usr/local/lib/bpp/stb/` does not. `find_file` searches `stb/` BEFORE `aarch64/`, so it preferred the stale file, and the resulting binary was missing `_stb_thread_create` (replaced with garbage that crashed `job_init`).

Fix: `install.sh` gained a pre-clean step that wipes `*.bsm` from every install dir before re-copying. Without this, files that move between directories across releases leave behind ghost copies that find_file picks up depending on cwd.

```sh
sudo rm -f "$LIB_DIR"/*.bsm
sudo rm -f "$STB_DIR"/*.bsm
sudo rm -f "$DRV_DIR"/*.bsm
sudo rm -f "$ARM64_DIR"/*.bsm
sudo rm -f "$X64_DIR"/*.bsm
```

Caught a real bug AND prevents the entire class going forward. The user agreed this was the right fix and confirmed it solved the "compile from games/snake/" issue immediately.

### dispatch.bsm wired into the modular pipeline (Constraint 1 fix)

While debugging the per-function classification work, hit Constraint 1 from the maestro plan: `bpp_dispatch.bsm` was previously SKIPPED in the modular pipeline (only ran in the old monolithic path). The fix is one line in `bpp.bpp` — call `run_dispatch();` after `infer_module(0)` and before `run_validate();`, matching the monolithic pipeline order (types → dispatch → validate). Per-loop dispatch analysis now runs for modular compiles.

But adding `run_dispatch()` immediately surfaced a second-order bug: cached functions loaded from `.bo` files have `body_arr = 0` (the AST is not reconstructed at load time, only the export signature). `run_dispatch()` tried to walk the AST of every function and crashed dereferencing 0 when it hit a cached function. Fix: skip cached functions in `run_dispatch()` — their per-loop dispatch decisions were already applied when the .bo was first written, so the cached machine code already reflects them. Only fresh-parsed functions need re-classification.

### The `global` keyword — design discussion

The user proposed adding a `global` keyword to declare top-level variables that are shared with worker threads. The motivation: smart dispatch needs to know which globals are "safe to access from workers" (the programmer authorized) vs which are "main-thread-only" (legacy default). Without an explicit marker, the compiler has to assume the worst and refuse to dispatch any loop that touches a global.

Three positions emerged in the discussion:

**Position A** — Add `global x;` at file scope; `auto x;` at file scope is the legacy/conservative default. Backwards compat preserved. The new `global` keyword is the explicit "this is shared" marker. Parser error if used inside functions ("global declarations are file-scope only").

**Position B** — Force `extrn x;` or `global x;` at file scope, deprecate `auto` at top-level. Breaking change.

**Position C** — Add `extrn` as alias for `auto` at top-level. Three forms for two semantics. Confusing.

The user picked Position A after some back-and-forth. The decision was driven by two principles:

1. **The `auto`-as-automatic mental model is elegant**. `auto` already means "the language figures out where it goes" — local in functions, serial global at file scope. Renaming top-level `auto` to something else would break that elegance. Leaving `auto` alone and adding `global` as a NEW keyword for the multi-thread case is the smaller, more conservative change.
2. **The distinction matters more in multi-threaded code**. In single-thread, local-vs-global is just about scope/lifetime. In multi-thread, it's also about thread safety: globals are shared mutable state that needs synchronization, locals are inherently per-thread by stack ownership. `global` makes that distinction visible at the declaration site.

Implementation: ~30 lines across `bpp_lexer.bsm` (add "global" to `check_kw`), `bpp_parser.bsm` (add `glob_shared` array parallel to `globals[]`, `parse_global` takes `is_shared` parameter, top-level dispatchers handle `global` as well as `auto`), and `bpp_bo.bsm` (cache reads default to `is_shared=0` until the format learns to round-trip the flag). `tests/test_global_kw.bpp` validates the new keyword parses, the legacy `auto` still works at top-level, and float type hints still apply. Bootstrap shasum: `7bf2f4459...`.

Saved as `feedback_base_solo_lexicon.md` and the larger discussion about three storage classes (`auto`/`global`/`extrn`) made it into the todo as a future task.

### The `extrn` keyword discussion (designed but not yet implemented)

The user pushed further on the storage-class design: should there be a third keyword for "write-once-after-init" globals? The use case is universal in games — assets, config, lookup tables, level data — load once at init, then read forever. The user pointed out that B/BCPL had `extrn` as the historical name for external storage, and B++ already has `extrn` as a reserved (but unimplemented) keyword in the lexer.

The three-way categorization emerged:

| Form | Mutable when | Safe for workers? |
|---|---|---|
| `const NAME = lit` | never (compile-time literal, no storage) | trivially yes — no storage |
| `auto x;` top-level | always (legacy/conservative) | NO (smart dispatch refuses) |
| `extrn x;` top-level | only before `job_init()` (write-once-after-init) | YES (read-only after freeze) |
| `global x;` top-level | always (worker-shared) | YES with stride analysis |

The freeze point for `extrn` would be `job_init()` (or `maestro_run()` which calls it internally). Static enforcement: the compiler tracks "is `job_init` reachable from this point in the call graph?" and rejects writes to `extrn` variables along any path that has crossed the freeze. A reachability pass — not yet implemented but designed and recorded in todo as a P0.

The decision was not to implement `extrn` in this session because the implementation requires a reachability analysis that hasn't been built yet. The design discussion is recorded so the work can land in a future session with full context.

### "src/ é o videogame, stb/ é o cartucho" — the cleanup philosophy

The biggest discussion of the day was about **what belongs in `src/` versus `stb/`**. The historical state was ambiguous: some "stb" modules were used by the compiler (stbarray, stbhash) and never by any game; others were used only by games (stbecs, stbphys); a few were used by both (stbio, stbmath, stbfile). There was no clean rule.

The user articulated the right framing with two analogies:

**Videogame analogy**: `src/` is the console — forged, complete, comes with the hardware. `stb/` is the cartridges — optional, plug in what you need. The compiler is what makes the console work; the cartridges are the games.

**Percussion analogy**: `src/` is the rack and the stands — the structure that holds everything. `stb/` is the percussion instruments fastened to the rack — the things you actually play. Together, the percussionist makes their art.

By those analogies, the question becomes: **what's part of the "base console" that comes with B++?**

The user's filter: "if the compiler needs it to construct itself, programmers will need it too". The compiler is the canary — it's the most complex B++ program possible, and the structures/utilities it needs to exist are the same ones any non-trivial B++ program will eventually need. arr_push, hash_set, byte buffer ops, string ops, print, math, file I/O — all universal. None of the "stb" modules in this category have any direct game importer; they're all infrastructure.

A second filter that the user added later: **"would this appear naturally as a builtin/stdlib in any modern language?"** The 7 modules pass that test (Python has list/dict/str/bytes/print/math/open, JS has Array/Map/String/Uint8Array/console/Math/fs.read, Rust has Vec/HashMap/String/Vec<u8>/println!/f64::*/std::fs, Go has []T/map/string/[]byte/fmt/math/os.Read*). The other 16 stb modules don't (no language has built-in ECS, physics, tilemap, sprite rendering — those are domain-specific cartridges).

The decision: **8 module renames** (counting `defs.bsm` → `bpp_defs.bsm`, the only file in `src/` that didn't follow the bpp_ convention).

```
src/defs.bsm           → src/bpp_defs.bsm           (compiler constants)
stb/stbarray.bsm       → src/bpp_array.bsm          (dynamic arrays)
stb/stbhash.bsm        → src/bpp_hash.bsm           (hash maps)
stb/stbbuf.bsm         → src/bpp_buf.bsm            (byte buffers)
stb/stbstr.bsm         → src/bpp_str.bsm            (string ops)
stb/stbio.bsm          → src/bpp_io.bsm             (print/getchar/putchar)
stb/stbmath.bsm        → src/bpp_math.bsm           (sin/cos/sqrt/randi)
stb/stbfile.bsm        → src/bpp_file.bsm           (file I/O wrappers)
```

After the renames, the categories are clean:

- **`src/` (22 files)**: 11 compiler stages + 7 core utilities + 4 runtime modules (bpp_beat, bpp_job, bpp_maestro, bpp_defs)
- **`src/aarch64/` and `src/x86_64/`**: chip+OS bundles (codegen, encoder, binary writer, platform layer, bug observer)
- **`stb/` (16 files)**: pure cartridges — game/render/ecs/phys/tile/sprite/path/font/image/input/color/draw/ui/cole/arena/pool. None of these are imported by the compiler; all are explicit-import for user programs.

The mechanics took ~50 import sites updated across the codebase. install.sh extended to ship the new src/bpp_*.bsm modules to `$LIB_DIR` alongside the compiler internals. All using `perl -i -pe` for the batch find/replace, which is the right tool for the job; the user pushed back briefly on the use of perl but accepted it after seeing the alternative was 30+ separate Edit operations.

### The fixpoint vs SCC discussion (and why fixpoint won)

While planning the per-function purity classification (still pending in batch 2), the question came up: should `classify_all_functions()` use Tarjan's SCC algorithm or a simple double-buffered fixpoint iteration?

I initially leaned toward SCC because it's "the elegant solution" — single pass, mutual recursion handled by construction, asymptotically optimal. The user pushed back hard on this and won the argument decisively.

The user's argument:

1. **The lattice has height 1**. Purity is binary: PHASE_BASE → PHASE_SOLO is the only transition, no upgrade. For a height-1 lattice, fixpoint converges to the EXACT result in O(call-chain-depth) passes. Not approximate. The "fixpoint is approximate" intuition comes from value-set analysis where the lattice has many heights and convergence can produce conservative results — that doesn't apply here.

2. **Both produce identical results for binary purity**. SCC only wins on a constant factor (passes over the array), and even that constant doesn't translate to real time because each fixpoint pass is microseconds in B++ scale.

3. **Tarjan has silent bug classes that fixpoint doesn't have**. Wrong low-link update, wrong stack pop boundary, wrong sentinel handling — all classify functions incorrectly without crashing. Debugging silent classification errors when "smart dispatch is emitting parallel_for for the wrong function" is brutal. Fixpoint bugs are loud (loop forever, or test fails obviously).

4. **YAGNI**. SCC is the right tool when the lattice grows beyond binary (PHASE_GLOBAL_RW, PHASE_FFI_PURE, etc.). When that day comes, swap fixpoint for SCC; the rest of the code stays the same. Doing SCC now is "B-tree for an array of 50 elements" — building scalability we don't need yet.

5. **~25 lines vs ~60 lines, in a step that is less critical than the codegen synthesis**. Save the time and attention budget for where it actually matters.

I conceded. The plan is fixpoint with double-buffering, deferred to batch 2 when classify_function actually gets implemented. The full discussion is worth recording because it shows the user's instinct: "doing it right the first time" doesn't mean "the most sophisticated algorithm available", it means "the simplest tool that produces the correct result for the actual problem you have". Tarjan is a tool, not a virtue.

u### What we did NOT do (and why)

A few things came up in design discussion but were explicitly deferred:

- **`extrn` keyword + reachability analysis** — designed, in todo, but the static enforcement of "no write after `job_init` reachable" is its own pass that needs the call graph from the smart dispatch work to land first.

- **Auto-injection of the 7 utilities for module 0** — the user wants `arr_new()`, `hash_set()`, `print_int()`, `sin()`, `file_read_all()` to "come for free" without imports. The mechanism is straightforward (extend `process_file` to fire an injection block when `my_mod_idx == 0`). Deferred to batch 2 alongside the platform layer split, because we want to bootstrap-verify the rename + reorganization first as a clean checkpoint.

- **Refactor of `bpp_beat`/`bpp_job`/`bpp_maestro` to be standalone** — currently they import stbgame because the platform layer has hidden coupling to stbgame globals. The right fix is to split `_stb_platform_<os>.bsm` into `_stb_core_<os>.bsm` (time + threads, universal) and `_stb_platform_<os>.bsm` (window + GPU, game-specific). Deferred to batch 2.

- **Smart dispatch implementation** — the call graph + classify_function + codegen synthesis is the bulk of the smart dispatch work. Deferred to its own session after batch 2 lands.

- **SCC** — see above. Deferred indefinitely until the lattice grows beyond binary purity.

### Counts at session end

- `src/`: 13 → 22 .bsm files (8 renames + 3 new runtime modules)
- `stb/`: 23 → 16 cartridges
- Tests: 52 → 57 (added 6, removed 1 stale)
- Suite: **46 passed / 0 failed / 11 skipped**
- bpp shasum at commit: **`f8f78682753dd519db966863e04ea9803e047f32`**
- Files changed in commit: 73 (+2902 / -107 lines)

### Files Changed (selected)

**New runtime in src/**:
- `src/bpp_beat.bsm` — universal monotonic clock
- `src/bpp_job.bsm` — N×SPSC worker pool
- `src/bpp_maestro.bsm` — solo/base/render bandleader

**Renames** (preserving git history via similarity detection):
- `src/defs.bsm` → `src/bpp_defs.bsm`
- `stb/stbarray.bsm` → `src/bpp_array.bsm`
- `stb/stbhash.bsm` → `src/bpp_hash.bsm`
- `stb/stbbuf.bsm` → `src/bpp_buf.bsm`
- `stb/stbstr.bsm` → `src/bpp_str.bsm`
- `stb/stbio.bsm` → `src/bpp_io.bsm`
- `stb/stbmath.bsm` → `src/bpp_math.bsm`
- `stb/stbfile.bsm` → `src/bpp_file.bsm`

**Compiler changes**:
- `src/aarch64/a64_codegen.bsm` — `mem_barrier()` builtin emit (DMB ISH)
- `src/aarch64/a64_macho.bsm` — `sys_unlink` before `sys_open` (inode fix)
- `src/aarch64/_stb_platform_macos.bsm` — pthread FFI + `_stb_thread_create`/`_stb_thread_join` wrappers
- `src/x86_64/x64_codegen.bsm` — `mem_barrier()` builtin emit (MFENCE)
- `src/x86_64/x64_elf.bsm` — `sys_unlink` before `sys_open` (parity with macho)
- `src/bpp_validate.bsm` — `mem_barrier` added to `val_is_builtin`
- `src/bpp_emitter.bsm` — `mem_barrier` C emitter equivalent (`__sync_synchronize`)
- `src/bpp_lexer.bsm` — `global` recognized as keyword
- `src/bpp_parser.bsm` — `parse_global` takes `is_shared` parameter, `glob_shared` array
- `src/bpp_bo.bsm` — cached globals default to `is_shared=0`
- `src/bpp_dispatch.bsm` — skip cached functions (`body_arr == 0`) in `run_dispatch`
- `src/bpp_import.bsm` — pathbuf clobber fix (save before recurse, restore after)
- `src/bpp.bpp` — wire `run_dispatch()` into modular pipeline before `run_validate()`

**New tests**:
- `tests/run_all.sh` (130 lines, runner)
- `tests/test_barrier.bpp` (mem_barrier round-trip)
- `tests/test_beat.bpp` (clock monotonicity)
- `tests/test_thread.bpp` (calling-convention bridge)
- `tests/test_job.bpp` (100 + 10000 jobs through 4 workers)
- `tests/test_maestro.bpp` (5-tick maestro_run)
- `tests/test_global_kw.bpp` (global vs auto vs const)

**Modified tests**:
- `tests/test_stbgame_native.bpp`, `tests/test_gpu_rect.bpp` — added 30-frame auto-exit
- ~25 test files — import path updates for the renamed modules

**Deleted**:
- `tests/test_io9.bpp` — broken legacy stub from `.bo Cache Fix` commit, importing a never-existing `stb/stbio_new.bsm` and calling `pstr()` which doesn't exist anywhere

**New game**:
- `games/snake/snake_maestro.bpp` — port of snake_gpu using the maestro pattern, side-by-side with the original

**Documentation**:
- `docs/maestro_plan.md` — NEW, ~1000 lines
- `docs/games_roadtrip.md` — maestro phase notes
- `docs/todo.md` — 0.22 cycle done section, P0 reordered
- `README.md` — typo fixes

**Build infrastructure**:
- `install.sh` — pre-clean step + new bpp_*.bsm modules to $LIB_DIR
- `bpp` — recompiled, shasum `f8f78682...`

### What's next (batch 2)

The session's commit (`318ef25`) is a checkpoint. The next session continues with **batch 2**: split the platform layer into `_stb_core_<os>.bsm` + `_stb_platform_<os>.bsm`, refactor `bpp_beat`/`bpp_job`/`bpp_maestro` to be standalone (drop the stbgame import), add `maestro_quit()`, add `game_run()` wrapper to stbgame, auto-inject the 7 utilities + 3 runtime modules + `_stb_core` for every user `.bpp` program. After that, the actual smart dispatch work (call_graph_build, classify_all_functions, codegen synthesis) becomes its own focused session.

The cleanup is the hard work that pays off forever. Foundation arrumada, casa limpa, B++ tá ficando forte.

---

## 2026-04-09 — B++ 0.23: Language Syntax Complete + Smart Dispatch + Module Discipline

The language shape locked in for 1.0. Three keywords and two annotations landed simultaneously: `load`, `static`, `void` on the keyword side; `: base` and `: solo` on the annotation side. Together they draw the final boundary between what the programmer writes and what the compiler infers — and the boundary is small enough to memorize.

`load` is the project-local twin of `import`. `load "file.bsm"` only searches the entry `.bpp`'s own directory and follows strict module rules. Games use it for their own multi-file split; `import` stays reserved for stb cartridges and compiler modules on the search path.

`static` makes a function private to its defining module. Anything without `static` is assumed public API. W020 fires when another module reaches across the boundary — the `_` prefix is convention, but `static` is the rule.

`void` is how a function declares that its return value is meaningless. The body can omit the trailing `return;`, and the compiler inserts an implicit `return 0;` at the machine-code level.

`: base` and `: solo` are the programmer's interface to the smart dispatch engine. `: base` asserts a function is pure and worker-safe; the compiler verifies by building a call graph and running fixpoint classification. If the assertion is wrong (a hidden global write, a call to `malloc`, anything marked impure transitively) W013 fires with the exact reason. `: solo` is the override when the programmer knows the function must stay on the main thread despite being pure on paper — typically because it touches an NSApp or an XCB handle whose APIs assume serial access.

Behind the keywords: `call_graph_build()` walks every function body collecting direct callees, address-taken (`fn_ptr(x)`) marks, and local impurity. `classify_all_functions()` runs a double-buffered fixpoint over the graph — a height-1 lattice that converges in `O(call-chain-depth)` passes. `promote_globals()` classifies every `auto x;` at file scope into `const` / `extrn` / `global` based on where writes appear; `--show-promotions` prints the verdict for each. `mark_reachable()` BFSes from `main` to compute `fn_reachable[]`; the monolithic pipeline's bridge then skips dead functions during emission. Snake shrinks from 69 KB to 33 KB — 52% reduction — just from lazy emit.

The module discipline side landed in the same commit. Function dedup (Fix 1) — every name has a single entry in `funcs[]`. The `stub` keyword — explicit override placeholder, silent replacement by the real definition, E105 if `main()` appears in a `.bsm`. Cross-module duplicate names (not marked `stub`) are E221; circular imports are E222.

Arena-backed AST: `make_node` and `list_end` now pull from an 8 MB arena instead of `malloc(NODE_SZ)`. `stbarena.bsm` got promoted to `src/bpp_arena.bsm` and is auto-injected. Allocations stopped fragmenting, and AST walks got measurably faster from the cache locality.

Diagnostics upgraded to Clang style: every warning now displays `file:line`, the source line, and a caret pointing at the token. `Node.src_tok` carries the birth token position through the entire tree. `_val_show_context_at()` can render location for any node — not just the ones validate originally annotated.

`.bug` file format gained a `glob_class` byte per global so `bug program.bug` now shows `[auto]` / `[global]` / `[extrn]` next to each variable. Level 1 of the bug debugger's long-term vision checked off.

Bootstrap: `6422f800...`. Suite: 50 passed / 0 failed / 11 skipped. 43 files changed (3425 insertions, 915 deletions).

---

## 2026-04-09 — B++ 0.23.1: Cache Integrity + Tonify Infrastructure

Two critical cache bugs found during the tonify sweep, and the tonify checklist itself codified as repeatable documentation.

First bug: auto-injected modules weren't recording their `dep_from` / `dep_to` edges, so editing `bpp_str.bsm` (auto-injected) did NOT invalidate consumers' `.bo` caches. Next build: 105 spurious W020 warnings because cached functions were carrying over stale metadata. Fix: auto-injected modules now participate in the dep graph like any other import.

Second bug: `bo_read_export` wasn't populating `fn_static[]`, `fn_void[]`, `fn_phase_hint[]` for cached functions. The arrays ran shorter than `funcs[]`, so `arr_get(fn_static, i)` for a cached function read garbage. "Is static" flags were false-positive on cached functions across module boundaries. Fix: default to 0 when reading cached exports and populate all three arrays to match `funcs[]` length.

Tonify infrastructure: `docs/tonify_checklist.md` added as the 7-rule (at the time) expert checklist for sweeping `.bsm` and `.bpp` files. Rules cover storage classes, visibility, return types, phase annotations, control flow, slice types, and comment style. The key learned rule: never mark `: base` on a function that calls a builtin (`malloc`, `putchar`, `peek`, `sys_*`, etc.) — the classifier treats all builtins as impure, and W013 will catch the mistake. Pure pointer-arithmetic readers (`arr_get`, `arr_len`, `unpack_s`) are the only safe candidates.

Batch 1 tonified: `bpp_io`, `bpp_array`, `bpp_buf`, `bpp_str`. Bootstrap: `6b532917...`. Suite: 50 / 0 / 11.

---

## 2026-04-11 — Tonify Batches 1-5 + Switch + Multi-Error + Node Slice + LDRSW

The tonify campaign reached its first major milestone: 5 of 7 planned batches done. In numbers: ~350 `void` annotations added where functions had trailing `return 0;`, ~200 `static` markers for private helpers, ~50 `: base` annotations for confirmed pure functions, ~50 `while`-with-manual-counter converted to `for` loops. 15 stb modules + 12 compiler internals + both platform layers passed through the sweep. The compiler self-compiles at byte-identical output across every intermediate checkpoint.

The tonify work is deliberately cosmetic on the surface but structural underneath — every `void` declares intent, every `static` enforces a boundary, every `: base` invites the classifier to verify a purity claim. What was implicit becomes provable.

Alongside tonify, the language gained `switch`. Two forms: value dispatch (`switch (x) { X, Y { ... } Z { ... } else { } }`) and condition dispatch (`switch { a > b { ... } c == d { ... } else { } }`). No fallthrough — each arm is its own block. Value dispatch with integer-literal arms and at least 3 cases triggers codegen jump tables; sparse or non-integer arms fall back to an if-chain but the condition is evaluated ONCE and kept on `[sp]` across comparisons. Exhaustiveness warning W021 fires when the switch has no `else`.

`bare return;` landed in `void` functions — equivalent to falling off the end, but explicit and readable at early exits.

**Multi-error diagnostics**: `diag_error` now accumulates into a list and parsing continues with best-effort recovery. All errors render at the end of the compile with full Clang-style context. Cap at ~20 errors so the output stays readable. `diag_fatal` sites that represented genuinely unrecoverable state got audited; everything else migrated to `diag_error`. E230 (static const blocked at file scope) added as a new diagnostic after the pitfall surfaced during batch 2.

**Node struct sliced**: `ntype: byte`, `dispatch: byte`, `itype: byte` replace their word-sized equivalents. AST memory footprint dropped 29%. `RECORD_SZ` (function/extern records) decoupled from `NODE_SZ` so future Node slice changes don't ripple into record layout. ~200 sites of the raw `*(node + 8)` pattern refactored to typed `n.a` access — the sliced layout packs fields without alignment padding, so hardcoded offsets don't work anymore. This refactor is what surfaced the sliced-struct access rule (later Rule 8 in tonify).

**LDRSW sign-extension fix**: `: half` (signed 32-bit) struct fields were loading as zero-extended unsigned values. ARM64 needs `LDRSW` not `LDR W`; x86_64 needs `MOVSXD` not `MOV`. Both backends fixed. `dsp_is_local` had a `packed_eq` bug producing W013 false positives on cross-module references to local names — also fixed.

Runtime: `_stb_core_linux.bsm` created so Linux cross-compile no longer depends on a ghost macOS file. Maestro's accumulator pattern (fixed timestep) landed so fps-independent physics works correctly under load. `job_parallel_for` grew a serial fallback: if `job_init` was never called, the for loop runs inline. Progressive enhancement means smart dispatch can rewrite loops without risking silent correctness loss if the worker pool isn't set up.

Suite: 62 / 0 / 11. Bootstrap stable.

---

## 2026-04-13/14 — Foundation Refactor Mega-Session + Mini Cooper Plan

Marathon session that landed five major structural changes back-to-back and closed with a fresh plan for the next ambitious push. Bootstrap verified after every step; `51 passed / 0 failed / 11 skipped` throughout.

**1. Cache removal (commit `0d3f282`).** The `.bo` module cache was the #1 bug source (stale cache, phantom errors, bootstrap oscillation across multiple sessions). Measured full-rebuild time at 0.27s; cache saved 0.17s at the cost of hundreds of lines of complexity. Removed entirely: ~630 lines net across `bpp.bpp`, `bpp_bo.bsm`, `bpp_import.bsm`. Every compilation is now from source. Go 1.20 and Jai both proved this model works when the compiler is fast enough.

**2. Repo reorganization (same commit).** Old `src/aarch64/` and `src/x86_64/` collapsed chip + OS concerns. New layout:

```
src/backend/
  chip/aarch64/ | chip/x86_64/      ← pure CPU encoding/codegen
  os/macos/ | os/linux/              ← platform + syscall + runtime
  target/aarch64_macos/              ← Mach-O writer
  target/x86_64_linux/               ← ELF writer
  c/bpp_emitter.bsm                  ← C source backend
```

Added a `_try_dir()` helper in `find_file()` that replaced ~90 lines of per-path `poke()` chains with ~20 lines of clean calls. Adding a new target is now creating new folders — zero compiler changes.

**3. bsys/brt0 runtime extraction (commits `0d3f282` and `f5ce95a`).** The 18 hard-coded syscall numbers in each codegen collapsed into `BSYS_*` const tables per OS (`_bsys_macos.bsm`, `_bsys_linux.bsm`). Startup globals `_bpp_argc` / `_bpp_argv` / `_bpp_envp` moved from codegen sentinels (-1/-2/-3) to real B++ globals declared in `_brt0_<target>.bsm` — the codegen looks them up via `a64_bind_startup_syms` / `x64_bind_startup_syms` after brt0 is parsed. `add_hidden_globals()` helpers removed from both codegens.

**4. putchar/getchar as B++ functions (commit `d0bba09`).** Phase 3 closes: `putchar`, `putchar_err`, `getchar` left the codegen builtin list and became actual B++ functions in `bpp_io.bsm` using `sys_write(1, &buf, 1)` via the little-endian-trick local-var pattern. ~80 lines of inline machine code per backend eliminated. C emitter keeps its hard-coded `bpp_sys_write` wrappers — by design, the C path uses libc.

**5. bmem allocator (commit `9eca4de`).** B++ now has its own `malloc` / `free` / `realloc` written in B++ itself. `sys_mmap` and `sys_munmap` got promoted to proper builtins (6-arg and 2-arg handlers in both codegens + C emitter `mmap()` / `munmap()` wrappers). `_bmem_linux.bsm` and `_bmem_macos.bsm` implement a minimal mmap-per-allocation allocator with an 8-byte size header. Linux static ELF binaries no longer link anything at all — true libc-free. `bpp_mem.bsm` is the public API shim, auto-injected.

**6. Jump tables + eval-once switch (commit `7b2b578`).** Form-1 value-dispatch switches with dense integer arms now emit a branch table on both backends: ARM64 uses `adr + add + br` into a `b arm_lbl` table; x86_64 uses `shl rax, 3 + lea + add + jmp rcx` into 8-byte slots (`jmp rel32` + 3 nops). Fallback (non-dense switches) now evaluates the switch condition ONCE and peeks it from `[sp]` per comparison — the old pattern re-emitted the condition per arm-value, turning switch-on-function-call into N function calls. C emitter upgraded to emit real C `switch` statements (previously if/else-if chain) so gcc can apply its own jump-table optimization.

**7. Sliced struct access bug discovered and documented.** The root cause of a jump-table bug was a B++-ism: `*(node + 8)` did NOT read `.a` because Node's sliced layout packs fields without alignment padding. `.a` actually lives at byte offset 1 (not 8) because `ntype: byte` consumes 1 byte with no padding after it. Same bug was latent in the T_IF dead-code-elimination path since slicing landed — `*(cond_node + 0) == T_LIT` never matched because junk bytes from `.a` bled into the 8-byte read. Both call sites fixed to use typed access (`auto n: Node; n.a`). `docs/tonify_checklist.md` Rule 8 added. `feedback_sliced_struct_access.md` saved to agent memory.

**8. Mini Cooper plan approved.** End of session produced a fresh multi-session plan at `~/.claude/plans/compressed-soaring-turing.md`: native performance ladder (B0 const folding, B1 expression register allocation, B2 inline `: base` functions, B3 local RA, B4 `: double` slice + SIMD builtins) plus language gaps (A1 bitfields, A2 aligned malloc). Target: Wolfenstein 3D at 60 fps and RTS with 1000+ units on native B++ without `bpp --c | gcc -O2`. Honest speedup expectation: 30-40% of gcc -O2 in general code, 60-80% on hot paths with manual SIMD. Enough on modern hardware (Apple M4 has ~100× the compute of the 386 that shipped Wolf3D in 1992).

**Counts at session end**
- Commits landed: `0d3f282`, `f5ce95a`, `d0bba09`, `9eca4de`, `7b2b578`, `4b2ccef`.
- Suite: 51 passed / 0 failed / 11 skipped.
- Compile time: 0.27s baseline, unchanged after all refactors.
- Compiler self-hash: stable gen2 == gen3 at every checkpoint.
- Linux cross-compile validated via Docker: hello + switch stress test produce identical output (31711) on both platforms.

---

## 2026-04-14 — Mini Cooper Phase A + B0, T_TERNARY Side Quest, Rule 11 Sweep

The Mini Cooper plan's opening moves all landed in a single continuation session. Phase A delivered the two language-feature gaps (bitfields and aligned malloc), Phase B0 delivered constant folding and dead-code elimination, and an unplanned side quest delivered the ternary operator and short-circuit `&&` / `||` along the way. Bootstrap stable at every checkpoint; suite grew from 51 → 59 as eight canonical tests were added to lock in the new features.

**Phase A1 — Bitfields.** `: bit`, `: bit2` through `: bit7` joined the slice ladder, encoded as slice ordinals 4-10 under the expanded 4-bit slice field (grown from 2 bits, with `SL_DOUBLE` reserved at 11 for Phase B4). The parser's struct layout pass precomputes `(byte_offset, bit_offset)` per field, LSB-first packing, overflow starts a fresh byte, and any non-bit field flushes the in-flight packing. Field hints carry the bit offset in bits 8-10 of the hint word so `T_MEMLD` / `T_MEMST` codegen knows how to position the UBFX / BFI (ARM64) or SHR + AND / read-modify-write (x86_64) instruction. C emitter uses bitwise expressions. Full round-trip across all three backends validated with `tests/test_bitfield.bpp`.

During A1 integration a pre-existing bug surfaced: stack structs with total size not a multiple of 8 corrupted the next variable's slot because the 8-byte chunked copy wrote past the struct. Rounded up the allocation and copy counts across both chip codegens and the C emitter.

**Phase A2 — `malloc_aligned(size, alignment)`.** New built-in following the textbook over-allocation pattern. The allocator header expanded from 8 bytes (size only) to 16 bytes (raw mmap pointer + total mmap size) for both regular `malloc` and `malloc_aligned`. `free` now reads the header uniformly. No external code depended on the old 8-byte layout (audited — `bpp_array.bsm` and `bpp_str.bsm` have their own 16-byte headers above the user pointer, independent of bmem). A free side effect: `malloc` now returns 16-byte aligned pointers, a gift for later SIMD users.

**Phase B0 — Const fold and DCE, moved to the parser.** Started per-backend (B0.1 for ARM64, B0.2 for x86_64) then refactored to a single parser-level implementation covering the canonical `3 + 5` → `8` case across all integer ops and shifts. Division / modulo by zero is explicitly NOT folded — the runtime trap still fires.

DCE got the same treatment: `if (0)` / `if (1)` / `while (0)` are collapsed at the parser. A new AST node `T_BLOCK` wraps the taken branch so backends don't need separate "skip the condition" logic. The previous per-backend DCE handlers (three places: ARM64, x86_64, and the C emitter's comment-only acknowledgment) all got deleted. The C emitter inherits everything for free because literal conditions disappear before the C output is generated.

**T_TERNARY + short-circuit side quest.** A user prompt surfaced a quieter bug: `&&` and `||` in B++ were not short-circuit. Every integer `&&` emitted both sides' cset-and pattern; the float path did the same with fcmp. The C emitter used C's native `&&` (which IS short-circuit), so `bpp --c game.bpp` and `bpp --native game.bpp` produced different behaviour on the same source — silent divergence that would break null-pointer guards like `if (p != 0 && peek(p) > 0)` on native but not on C.

The clean fix was to lift short-circuit to the AST: a new `T_TERNARY` node for `cond ? then : else`, and the parser rewrites `a && b` to `T_TERNARY(a, b, 0)` and `a || b` to `T_TERNARY(a, 1, b)`. Backends get a single canonical handler each; the C emitter passes through to native C's `?:`. The per-backend `&&` / `||` handlers in `a64_codegen.bsm` and `x64_codegen.bsm` were deleted (four branches in each). The ternary operator is now first-class — programmers can write `max = a > b ? a : b;` without boilerplate.

The decision to put this in the frontend rather than in each chip codegen — twice in one session — forced a new canonical rule. The rule now lives in `feedback_feature_placement_canonical.md`: semantic decisions (what the program means) go in the parser; emission decisions (how the chip encodes it) stay in the backend. The litmus test: would a new backend need to reimplement this? If yes, wrong layer.

**Batch 5.5 — Rule 11 sweep.** The new ternary + `&&` short-circuit idioms got applied across the high-traffic compiler and stb files: `bpp_types.bsm` (2 sites collapsing 14-line `if/else` chains to 4 lines), `bpp_parser.bsm` (nested `else if` collapsed via `&&`), `bpp_lexer.bsm` (the `add_token(kw == 2 ? 1 : kw == 1 ? 0 : 2, ...)` ternary), `bpp_import.bsm` (two `diag_str(is_load ? "load" : "import")` simplifications), `bpp_dispatch.bsm` (three sites including the `wbucket = frozen ? glob_writes_post : glob_writes_pre` consolidation), and `stb/stbimage.bsm` (PNG depth-16 stride unified as `step = depth == 16 ? 2 : 1` across all 5 color modes). 11 sites, ~76 lines saved. The compiler source now demonstrates the new idioms in the places programmers read first when learning B++.

**Canonical tests added**: `test_bitfield`, `test_aligned_alloc`, `test_const_fold`, `test_realloc`, `test_signed_half`, `test_switch`, `test_ternary`, `test_short_circuit`. Each guards one feature, each runs on both macOS ARM64 and Linux Docker x86. The suite moved from 51 → 59 passing.

**Memory and rules codified**: four new agent-memory files (`feedback_tonify_first`, `feedback_backend_layers`, `feedback_canonical_tests_and_docker`, `feedback_feature_placement_canonical`) plus three new rules in `docs/tonify_checklist.md` (Rule 9 on `var` vs `auto` struct declaration, Rule 10 on portable built-ins, Rule 11 on ternary and short-circuit idioms).

**What's next.** Phase B1 is the big one — expression register allocation, four sub-steps, 400 lines, two-pool freelist (GP + SIMD), 74+ push/pop sites to rewrite. The parser improvements from this session make B1's code analysis cleaner (short-circuit means `&&` is no longer a binop the RA has to handle; const fold means literal sub-trees are already collapsed). Smart dispatch (originally blocked on cache format extension) also cleared: the cache doesn't exist anymore, so that dependency evaporated.

## 2026-04-15 — Mini Cooper Phase B4 — `: double` SIMD — **Mini Cooper done**

The last Mini Cooper performance phase landed: 128-bit SIMD is now a first-class slice in the language. Eleven `vec_*` builtins (`vec_load4`, `vec_store4`, `vec_add4`, `vec_sub4`, `vec_mul4`, `vec_div4`, `vec_min4`, `vec_max4`, `vec_splat4`, `vec_dot4`, `vec_get4`) lower to NEON on ARM64 and SSE2 on x86_64, both natively emitted — no library, no runtime dispatch, no intrinsic shims. With Phases A, B0, B1, B2, B3, C, and now B4 complete, the Mini Cooper perf ladder is done and the next move is stbaudio + the Rhythm Teacher demo.

**Type plumbing.** `TY_FLOAT_D = 0xB3` joined the type table (BASE_FLOAT 0x03 | SL_DOUBLE << 4). The `SL_DOUBLE = 11` slice constant and `slice_width(SL_DOUBLE) → 128` had been reserved since Phase B1 specifically to keep B4 additive. The parser gained a single-branch `: double` hint that flows through to locals and struct fields; `auto v: double;` now reserves a 16-byte stack slot and `struct Particle { pos: double, vel: double, mass: float, id }` computes 128-bit field offsets correctly. `SL_DOUBLE` piggybacks on the same T_MEMLD / T_MEMST field-offset path that bitfields use, so struct field SIMD is free.

**`emit_node` return convention extended.** Expressions used to return `0` for int (x0 / rax) and `1` for float (d0 / xmm0). B4 adds `2` for SIMD (v0 / xmm0 as a 128-bit value). Every existing caller keeps working because SIMD values only feed into other SIMD ops — there is no mixed-type expression with a SIMD operand outside `vec_*` and the store-var path. The store path gained an **E231 diagnostic**: assigning a SIMD RHS (`rty == 2`) to a non-`: double` local is caught at compile time instead of silently corrupting a scalar slot. This caught the canonical bug mid-bring-up: `auto va; va = vec_splat4(3.0);` used to be garbage; now it's a clean error.

**ARM64 NEON encoders** live in `a64_enc.bsm`. Thirteen new encoders: load/store (`enc_ldr_q_uoff`, `enc_str_q_uoff`, plus pre-index `enc_str_q_pre` / post-index `enc_ldr_q_post` for SIMD push/pop), the `.4S` arithmetic and min/max family (`FADD/FSUB/FMUL/FDIV/FMIN/FMAX V.4S` funneled through a single `enc_neon_vvv(opcode, rd, rn, rm)` helper), two DUP variants (from GPR for splat of int, from scalar-S for splat of float), `FADDP V.4S` + `FADDP S` (the horizontal-sum pair that LLVM uses for `vaddvq_f32` — `FADDV` faulted on every variant I tried on Apple Silicon), and `UMOV W, V.S[lane]` for lane extract. The SIMD freelist scaffolding (`a64_simd_alloc` / `a64_simd_free`) sat dark in the codebase since B1; it's still reset-per-function and not yet consumed, reserved for a future pass that spills v-registers across calls.

**x86_64 SSE2 encoders** live in `x64_enc.bsm`. Packed load/store as MOVUPS (the unaligned form — MOVAPS would fault on non-16-byte buffers), and the rest of the family — ADDPS/SUBPS/MULPS/DIVPS/MINPS/MAXPS — routed through `x64_enc_sse_packed(opcode, dst, src)`. Plus MOVHLPS, MOVAPS reg-reg (needed to preserve the left operand of non-commutative ops through the right-operand evaluation), SHUFPS, PSHUFD (for splat and lane extract), ADDSS (the scalar tail of `vec_dot4`), MOVD in both directions (for `vec_splat4(int)` and the final scalar readback of `vec_get4`). **SSE2 only** — no DPPS, no SSE4.1 path. `vec_dot4` lowers to the classic five-instruction horizontal sum: `MULPS; MOVHLPS; ADDPS; PSHUFD; ADDSS`. Runtime CPU-feature dispatch is not in scope and will not be inside B4; the emitted binary runs on any x86_64 chip at the baseline ISA level.

**Commutative vs non-commutative spill dance.** SIMD binops evaluate left, push Q / xmm to stack, evaluate right into v0 / xmm0, then reload the saved left into v1 / xmm1 before the op. On ARM64 the FADD/FMUL/FMIN/FMAX encoders accept operand order; on x86_64 the SSE2 packed ops are destination-source and need a MOVAPS reg-reg to shuffle operands into place for non-commutative SUB/DIV. Both backends wrap this in a `_a64_vec_binop` / `_x64_vec_binop` helper so the eleven builtins collapse to eleven short call sites instead of eleven copies of the same eight-line spill/reload sequence — tonify Rule 2 (DRY) and Rule 3 (low cyclomatic complexity) applied aggressively after the user caught the first draft's `if { if { ... } }` repetition.

**Stack alignment for `: double` locals.** The frame builder walks locals and assigns stack offsets; SIMD slots need to land on a 16-byte boundary. The lesson was painful: rounding the relative `total` to 16 is not enough — the absolute offset (`40 + total` on ARM64, `24 + total` on x86_64) is what matters. A preceding 8-byte scalar left an otherwise-correctly-rounded SIMD slot at an 8-mod-16 absolute position. `test_vec_align_stack.bpp` pins this: `auto a, c; auto v: double; auto b;` and an assertion `(&v & 15) == 0`. B3 register promotion was already incapable of promoting SIMD locals (eligibility rejects `ty_slice != SL_FULL`), so there's no interaction.

**Four new canonical tests**, each covering a distinct blast radius:
- `test_vec_simd` — every one of the eleven builtins with known inputs, read back via `vec_get4` lane extract.
- `test_vec_align` — same ops against `malloc_aligned(16, 16)` (aligned) and `malloc(16)` (unaligned); result must match, proving MOVUPS/LDR-Q are safe on unaligned heap.
- `test_vec_align_stack` — the frame-alignment assertion above.
- `test_vec_struct` — the Jedi test. Struct with mixed `: double`, `: double`, `: half`, and plain fields. Writes every field, reads every field back, verifies no overlap. Guards against the B3-era `spill_base` family of bugs where a 16-byte field's offset is computed as 8 and aliases its neighbour silently.

Suite 61 → 65 passing, 0 failed, 11 skipped.

**C emitter — deferred.** The native backends emit vec_* as instructions; the C transpile path needs `: double` locals typed as `__m128` and the eleven builtins mapped to `_mm_*` intrinsics. The current C emitter uses `long long` for every local unconditionally — changing that is invasive work with zero demand from B++ programs that target the C backend today. Punted with a TODO comment in `bpp_emitter.bsm` and an entry in `docs/todo.md`. `bpp --c` on a program that uses `vec_*` builtins will fail cleanly at the extern-lookup stage, which is the correct behaviour — wrong-output would be worse than no-output.

**Cross-compile verified.** All four vec tests pass on macOS ARM64 natively and on x86_64 under docker/alpine (`bpp --linux64` → static ELF → `alpine` container run). Bootstrap converges at gen2 (`shasum` matches gen1). The Linux self-host workaround for B1/B2 (documented in a prior journal entry) was untouched and remains in force; SIMD bypasses the freelist entirely because `vec_*` results always land in v0 / xmm0 directly.

**Plan + review discipline held.** Plan mode → Plan agent review → Jedi-master sanity check → implementation, all logged. The Jedi review caught the `vec_get4` tuple (store+reload via stack spill + scalar single-precision reload + CVTSS2SD was the only SSE2-friendly path) before the first encoder was written, and flagged the need for the E231 diagnostic before the first wrong program could be compiled.

**Mini Cooper done marker.** Phases A (bitfields + aligned malloc) + B0 (parser constfold/DCE) + B1 (Sethi-Ullman freelist) + B2 (inline `: base` helpers) + B3 (hot locals in callee-saves) + C (batch 6 tonify) + B4 (`: double` SIMD) have all shipped. The native perf ladder the plan promised is delivered. `stbaudio` and the Rhythm Teacher demo are the next move — they were the whole reason B4 had a specific shape: a 4-wide float mixer with aligned buffers is the shortest path from here to audible output.

## 2026-04-16/17 — Mega session: page_count fix + effect lattice + stbaudio Day 2-3 + synthkey

The longest single session in B++ history. Started debugging a 3-day-old bootstrap regression and ended with a working polyphonic synthesizer with WAV recording.

**Mach-O chained fixups page_count (the bug that blocked everything).** `a64_macho.bsm` hardcoded `page_count = 1` in the chained-fixups header. When `__DATA` grew past 16 KB (tipped by the 16th dispatch seed), dyld rebased only page 0; GOT pointers on page 1 kept their on-disk values and string literal ADRP+ADD reads returned ASLR-shifted garbage. The `dyld[...]: fixup chain entry off end of page` warning had been printing on every build for weeks — nobody read it. `bug --dump-str` pinpointed the bytes in one run. Fix: dynamic page_count + per-page start offsets. Also fixed: `imports_off` was hardcoded `+24` and broke when the header grew.

**W025 shipped (Rule 13 compiler nudge).** The "W025 bootstrap regression" that shelved it 3 days earlier was the page_count bug in disguise. Re-implemented: `_val_is_unhinted_param` + T_BINOP check + `diag_help`. First catch: `_stb_init_window(w, h, title)` using `w * 3.0` — annotated `w: word, h: word` across all 4 backends.

**Bugs #3 + #3b closed.** Lexer's `scan_comment()` skipped newlines without ticking `cur_line`. Every `//` comment line caused all subsequent tokens to report N lines early. `malloc` at source:34 of `_bmem_macos.bsm` came back as line 10 (24 comment lines before it — matched exactly). Fix: tick `cur_line` in both `//` and `/* */` branches. `.bug` source map now 1-for-1 with source files.

**Level 4 effect lattice (sub-steps B + C + PHASE_HEAP + PHASE_PANIC).** Four lattice refinements in one session:

- **L4-B**: `fn_effect[]` populated via extern seeds + call-graph fixpoint. `_dsp_eff_join` implements the lattice join.
- **L4-C**: W026 fires when `fn_effect` disagrees with `: realtime` / `: io` / `: gpu` annotations. Gradual model — unknown externs default to PHASE_AUTO so only KNOWN violations fire.
- **PHASE_HEAP**: malloc/free/realloc/memcpy reclassified from SOLO to HEAP. HEAP is subsumed by IO/GPU in the join, so `: io` audio setup that allocates scaffolding stays honestly annotated. `: realtime` still rejects HEAP (the killer check for audio callbacks). The plan's "heap (currently SOLO; eventually distinct)" TODO is now shipped.
- **PHASE_PANIC**: sys_exit = never-returns = lattice bottom (⊥). `PANIC ∨ X = X`. Functions with error-path `sys_exit` no longer contaminate their effect. Formally identical to Rust's `!` / Swift's `Never`. `_stb_shutdown` (io + sys_exit) resolved cleanly; `_stb_gpu_init` has 1 remaining true-positive (error diagnostics print via sys_write before sys_exit).

Lattice order: `AUTO < BASE < REALTIME < HEAP < {IO, GPU} < SOLO`, PANIC = ⊥ (absorbed by everything).

**stbinput full keyboard.** 40 new Key enum slots (total 77). All 26 letters, punctuation (`- = [ ] ; ' \ , / ``), F1-F12 complete, Ctrl/Alt/Meta, Delete/Home/End/PgUp/PgDn. macOS VKey enum + `_plt_map_vkey` extended. Linux evdev + XQuartz mapping tables extended in parallel. Needed for synthkey's 4-octave layout; benefits every future keyboard-heavy app.

**stbaudio Day 2 — SPSC ring buffer.** `_stb_audio_open(cap_frames)` allocates a power-of-two ring, spins up the CoreAudio queue, starts the consumer callback (`_aud_stream_cb`). Producer feeds via `_stb_audio_push_frame(left, right)` or bulk `_stb_audio_push_frames(buf, n)`. When producer falls behind, callback pads with silence. `mem_barrier` sandwich on both sides. `test_audio_stream.bpp` added (silent, headless).

**stbmixer.bsm — polyphonic 8-voice mixer.** Parallel-array voice state (active / key / hp / amp / phase). Phase increments +1 per sample — identical to the tone-test callback, zero jitter. API: `mixer_init`, `mixer_note_on(key_id, freq)`, `mixer_note_off(key_id)`, `mixer_fill(buf, n)`, `mixer_stream(n)`. 4-way waveform fader (sine → triangle → sawtooth → square) via crossfade blend. Dirt control (bitcrush + decimation/sample-and-hold). Master volume.

**stbsound.bsm — audio file formats.** `sound_save_wav(path, buf, n_frames)` writes RIFF PCM WAV (44100 stereo s16). `sound_load_wav(path)` reads and returns frame count + buffer. Pure I/O + format, no device dependency.

**tools/audio/synth/synthkey.bpp — the demo.** 4-octave polyphonic keyboard synth. ZXCV/ASDF/QWERTY/numrow = MIDI 48-95. Press to sustain, release to stop. LEFT/RIGHT = waveform shape (sine → tri → saw → square). UP/DOWN = dirt (clean → bitcrush + decimation). SPACE = record/stop WAV. Note names + octave numbers drawn inside each key. Piano-style UI with white/black key layout and highlight on press. Two visual faders (WAVE + DIRT).

**Audio quality investigation.** The WAV recording sounded clean while real-time playback had "fritação" (digital crackling). Root cause: the fill loop capped at 512 samples/frame but the device consumed 735/frame (44100÷60). Deficit of 223 samples/frame drained the ring in ~5 seconds → callback hit silence gaps → clicks. Also: `malloc` + `free` every frame for the scratch buffer = kernel call per frame = timing jitter. Fix: pre-allocated 1024-frame scratch buffer, fill loop runs in batches until ring is topped up. Real-time playback now matches the WAV quality.

**New stb modules:** stbmixer (16th module), stbsound (17th module).

Suite 66 → 68 passing (test_audio_stream + test_mixer_stream). Bootstrap sha stable throughout. 25 active diagnostics (W025 + W026 added).

## 2026-04-17b — The Book + zero-warning clean compile

Short cleanup day after the synthkey marathon. Two outcomes: the
docs got their long-overdue consolidation, and the last two W026
false positives were resolved — every game in the repo now compiles
with zero warnings.

**The book.** `docs/bpp_manual.md` (1049 lines, tutorial) and
`docs/bpp_language_reference.md` (568 lines, reference) merged into
a single K&R-style volume: `docs/the_b++_programming_language.md`.
16 chapters totalling 1318 lines, covering Preface → Tutorial →
Variables/Storage → Functions → Control Flow → Operators → Literals
→ Comments → Type System → Memory → Composite Types → Modules →
Concurrency → Builtins → Standard Library → Compiler → Debugger +
Design Principles appendix. Updates since the old manuals: `switch`
documented (the manual still claimed "there is no switch"), full
effect lattice with PANIC/HEAP/REALTIME/IO/GPU, SIMD entire chapter,
`load` vs `import` distinction, `void`/`stub`/`: serial`, `&`
address-of as a first-class section, E232 note on the
`auto x = 44100.0` silent-int-conversion trap. The two source files
are gone from `docs/`; `.md~` backups remain.

**The production guide.** Earlier the same day,
`docs/bootprod_manual.md` (760 lines) and `docs/tonify_checklist.md`
(538 lines) were consolidated into `docs/how_to_dev_b++.md`
(830 lines, 9 parts): Daily Loop, Tonify Expert Mode, Architectural
Discipline, Writing Modules, File Order / Sweep Discipline, Testing,
Cross-Compile and Deploy, Compiler Flags Reference, Recovery. The
`how_to_dev` and `the_book` are the two canonical references now;
`bootprod_manual.md` and `tonify_checklist.md` become historical.

**Language reference filled in.** Before the merge, the standalone
reference got expanded: full phase annotations table (all five
writable phases + heap/panic notes), operators section (arithmetic,
comparison, bitwise, logical, memory, assignment, ternary),
address-of as its own section with syscall examples, literals
(integer/float/string/char with escape table), comments, core
builtins table with effect codes for each. Went 343 → 568 lines.

**install.sh cross-checked.** Every auto-injected module in
`bpp_import.bsm` (`bpp_mem`, `bpp_array`, `bpp_hash`, `bpp_buf`,
`bpp_str`, `bpp_io`, `bpp_math`, `bpp_file`, `bpp_beat`, `bpp_job`,
`bpp_maestro`) is in the install list, plus `bpp_defs` (codegen-time)
and `bpp_arena` (explicit-import). `stb/*.bsm` wildcard picks up all
18 modules including the three new audio ones. `src/backend/` layers
all copied by wildcard. Nothing stale, nothing missing.

**W026 cleanup — `: gpu` on init was wrong.** After the
`: realtime` audio path landed last session, two warnings kept
firing on every game compile:

```
warning[W026]: '_stb_gpu_init' annotated ': gpu' but reaches IO
warning[W026]: 'render_init' annotated ': gpu' but reaches IO
```

The classifier was right. `_stb_gpu_init` prints to stderr and
calls `sys_exit(1)` when Metal shader compilation fails. The
`bpp_dispatch.bsm` comment already spelled out why this isn't
absorbed by PHASE_PANIC: "they print before dying, so the IO is
real." Join(GPU, IO) = SOLO in the lattice — init is genuinely
SOLO, not GPU. Fix: drop the `: gpu` annotation from both (the
compiler infers SOLO from the body, which is what we want).
Per-frame helpers (`render_begin`, `render_clear`, `render_rect`,
`render_end`, `_stb_gpu_vertex`, `_stb_gpu_present`, etc.) stay
`: gpu` — those never touch syscalls. Comments added at both
sites explaining why future re-annotation will re-trigger W026.

Result: `bpp games/snake/snake_maestro.bpp`,
`bpp games/platformer/platform.bpp`, and every other game in the
repo now emit **zero warnings**. Bootstrap hash stable
(c7b5d8de...).

**Takeaway on the lattice.** The 2026-04-16 PHASE_PANIC refinement
made `sys_exit` the bottom element — `PANIC ∨ X = X`. It does NOT
make "the whole function that eventually calls sys_exit" bottom.
IO that executes before sys_exit is still observable (the process
is still alive during the write). This distinction was already in
the dispatch comment; the fix landed on the call sites rather than
on the classifier. Same policy whenever an effect annotation
disagrees with a function's real behaviour: change the annotation
first, change the lattice last.

## 2026-04-18 — Infra week day 1: Rhythm Teacher foundations

User opened: "a nossa próxima missão é rythm teacher e depois
wolf3d". Then revealed the ironplate prototype — a complete
Rhythm Teacher already built in C/raylib, 1540 lines of game
code on top of a 1300-line engine. Reading it answered every
open question about what a B++ port needs.

Key finding: the ironplate engine already uses **handles**. A
morning's research rabbit-hole on reference counting vs handles
(Kevin Lawler, Unreal TSharedPtr, Giordano's resource post, Bevy
Handle<T>, SFML sf::Sound/SoundBuffer, CoreAudio retained types)
had landed at the same conclusion independently: handles beat
refcount for game asset management. Validated by encountering
the same pattern in a live C engine.

User's framing: "é a nossa última semana de max, não quero
deixar pra depois essas coisas que posso fazer agora de infra".
Translation: build all the infra this week so next week is
pure game work. Five gaps identified:

  1. Handle-based asset manager (bridge to rhythm teacher's
     load_sprite/load_sound/load_music/load_font)
  2. Scene manager (MENU → PLAY → RESULTS without a main-loop rewrite)
  3. Audio buses + music streaming (game needs MASTER/MUSIC/SFX + BGM)
  4. Generic ECS components (rhythm teacher's RhythmNote doesn't fit
     the pos/vel/flags core)
  5. Spritesheet frame picker (teacher animation is 4 rows × 5 frames)

All five shipped clean in one pass. Details below.

**stbasset.bsm (new, 364 lines).** Handle layout is 32-bit:
[16-bit generation | 16-bit slot index]. Slot 0 reserved as
null handle so `auto h;` (= 0) is always a safe "no asset"
default. `HashStr` dedupes by path so loading the same WAV
twice returns the same handle. `asset_unload` bumps generation;
all outstanding handles fail the check on next `asset_get` —
ABA-safe without atomics (single-threaded asset ops).
Four loaders: `asset_load_sprite` (PNG → GPU texture),
`asset_load_sound` / `asset_load_music` (WAV → PCM, differs
only in kind tag for mixer routing), `asset_load_font` (TTF
through stbfont's single-slot state).

**stbscene.bsm (new, 155 lines).** Scene = 4 function pointers
(load, update, draw, unload). Switches are deferred to the top
of the next `scene_tick` so any handler can safely call
`scene_switch` without corrupting mid-frame state. 16 scene
slots; ironplate got by with 3 so plenty of headroom. Uses
B++ `fn_ptr(name)` + `call(fp, ...)` — no callback struct, no
vtable boilerplate.

**stbmixer.bsm (extended, +170 lines).** Voice table grew from
5 parallel arrays to 11. New fields: `kind`, `buf`, `pos`,
`frames`, `bus`, `loop`. Voice kinds:

  - `MX_KIND_TONE`   — synthkey's existing waveform synthesis
  - `MX_KIND_SAMPLE` — one-shot stereo s16 playback from buffer
  - `MX_KIND_MUSIC`  — looping streaming with position tracking

Voice pool grew 8 → 10: slots 0-7 for tones/SFX, slot 8
dedicated to music (can't be evicted by SFX flood), slot 9
spare. Three buses (`MX_BUS_MASTER` / `MUSIC` / `SFX`) with
independent 0..100 gain. The mixer_fill inner loop now
branches on `kind` — a modest cost since it's still one
read-write per voice per frame, way below the 44100×10 = 441K
ops/sec budget.

New public API: `mixer_play_sample(buf, n)`, `mixer_play_music(
buf, n, loop)`, `mixer_stop_music()`, `mixer_music_frames()`,
`mixer_music_ms()`, `mixer_set_bus_volume(bus, vol)`,
`mixer_get_bus_volume(bus)`. The `mixer_music_ms()` is the
critical glue for rhythm teacher — the note chart schedules
against it to stay sync-locked to BGM.

**stbecs.bsm (extended, +25 lines).** Added
`ecs_component_new(w, element_size)` and `ecs_component_at(base,
id, element_size)` for custom components. They're thin wrappers
over `malloc(capacity * size)` and pointer arithmetic, but they
document the pattern and keep game code from reinventing it.
Game-defined structs like `RhythmNote` work through typed
locals: `auto n: RhythmNote; n = ecs_component_at(notes, id,
sizeof(RhythmNote)); n.beat_time = 1.0;`.

**stbsprite.bsm + _stb_platform_macos.bsm (extended).**
Platform: added `_stb_gpu_sprite_uv(tex, x, y, w, h, u0, v0,
u1, v1)` — same 6-vertex quad as `_stb_gpu_sprite` but with
caller-supplied UVs in 0..65535 fixed point. stbsprite: added
`sprite_draw_frame(tex, x, y, fw, fh, sheet_w, sheet_h, row,
col, scale)` that computes UVs from sheet grid coords, and
`anim_frame(elapsed_ms, fps, frame_count, loop)` for
time-based frame picking. These make the rhythm teacher
character animation (4 rows × N frames per state) a one-liner
per draw.

**Tests (6 new).** test_asset (handle semantics), test_asset_wav
(WAV load + dedup + unload cycle with real file on disk),
test_scene (register + switch + tick + shutdown lifecycle),
test_mixer_sample (sample voices + music slot + bus clamp +
loop vs one-shot), test_ecs_component (custom components with
struct layout round-trip), test_anim_frame (time → frame math).
Suite: 68 → 74 passing, zero failures.

**Discoveries on the way.**

- `: heap` is not a user-facing annotation. The lattice has
  `PHASE_HEAP` but the parser doesn't accept it as an input —
  the five writable phases are still `base`, `solo`, `realtime`,
  `io`, `gpu`. Tried `: heap` on ecs_component_new, compiler
  caught it with E104. Kept ecs_component_new un-annotated;
  compiler infers PHASE_HEAP from the malloc.

- `ptr` is a reserved keyword. Test file used `auto ptr` and
  got E104. Renamed to `buf`. Should log this to `feedback_
  keyword_collisions.md`.

- Linux cross-compile of snake_maestro still fails (GPU-only,
  no Vulkan on Linux yet). Pre-existing issue, not a
  regression. `snake_cpu.bpp` Linux cross still clean.

Bootstrap hash stable (`c7b5d8de...`). All four games
(snake_maestro, pathfind, platform, plat_noasset) plus
mini_synth compile zero-warning. Module count 18 → 20 stb.
Version bumped 0.68 → 0.74 (test count is the version).

**Next up**: port `games/rhythm/` itself. beat_map loader,
timing windows, scoring, teacher sprite animation, menu +
results scenes. With the infra done, the port is 1500 lines
of game logic — tomorrow's work.

## 2026-04-18b — Rhythm prototype ships + snake closes the dog-food loop

Second half of the same day. Two games shipped, `bpp_path`
promoted to auto-inject, `sound_load_wav` grew into a real chunk
scanner, asset-load failures now print specific stderr lines
instead of silently returning 0, and snake started playing a
groove recorded live inside mini_synth. "A cobra comeu o próprio
rabo" is the canonical phrasing for the moment — B++ producing
the audio content that B++ games play.

**games/rhythm ships.** Structured the port faithful to the
ironplate C prototype: three scenes (`menu_scene`, `play_scene`,
`results_scene`) via the new `stbscene` module, `beat_map.bsm`
parses a text-file chart with BPM + offset + note lines, play
scene runs the three-phase DEMO → TRANSITION ("YOUR TURN!" 3.5 s
countdown) → PLAY pattern, teacher character drawn from
primitives (head circle, eye dots, mouth rect) with state
machines (idle / hit / miss / celebrate). Hit windows: ±20 ms
perfect (green), ±60 ms ok (yellow), else miss. Both F and SPACE
fire the snare — matches ironplate's "any tap plays the snare"
design, so the player gets instant audible feedback regardless
of whether a note is in the window. A classic "boom-chick"
4-bar rock groove ships as `assets/beat_map.txt` (49 notes:
kick + hi-hat on 1 and 3, snare + hi-hat on 2 and 4, hi-hat
on every eighth, snare fill on the last half-bar).

**bpp_path.bsm (new).** Games that load assets with hard-coded
relative paths only work when the user's shell happens to be in
the right directory. `bpp_path` fixes that: `path_exe_dir()`
computes the binary's own directory from `argv[0]`, `path_asset(
relpath)` joins it with the supplied relative path and —
critically — probes the filesystem walking up to 4 levels
(`<dir>/relpath`, `<dir>/../relpath`, ..., `<dir>/../../../../relpath`)
caching the first prefix that resolves. So `./build/rhythm`
inside `games/rhythm/` AND `games/rhythm/build/rhythm` from
the repo root BOTH find `assets/beat_map.txt` transparently.

**Auto-injection: bpp_arena + bpp_path.** Both now auto-injected
alongside bpp_mem / bpp_array / bpp_hash / etc. Every user
program gets them for free. Bootstrap cycle converged in gen2 ==
gen3 (1-cycle oscillation as expected for auto-inject change).
Sha `d08a9e99...`.

**sound_load_wav rewrite — the bug that blocked Rhythm audio.**
Initial port to stbasset loaded 3 drum samples and produced
silence. Investigation found THREE compounding bugs:

1. Ironplate's samples weren't PCM-16 stereo 44100. HI-HAT.wav
   was IEEE Float stereo. KICK.wav and SNARE.wav were PCM-16
   MONO. The old `sound_load_wav` rejected all three silently
   — returned `-1`, asset_load_sound returned 0, mixer
   never played.
2. The old `sound_load_wav` auto-freed `_snd_last_buf` on every
   call. When stbasset loaded three sounds in sequence, each
   load freed the previous buffer while stbasset still held the
   pointer → dangling + silence on the earlier handles.
3. `_aud_ring` got over-fed and under-fed in different
   experiments as we tried to shave latency; the wrong middle
   produced time-stretch stuttering.

Fixes:
- Removed the auto-free from sound_load_wav; caller owns the
  buffer.
- Rewrote sound_load_wav into a chunk scanner (handles LIST /
  INFO / fact chunks between fmt and data), supporting PCM
  8/16/24/32-bit + IEEE Float 32-bit + mono (auto-expanded to
  stereo) + stereo. Rejects unsupported formats cleanly with a
  specific stderr diagnostic, not silent.
- Audio pump matches the mini_synth proven pattern: ring size
  4096 for rhythm (fill-until-near-full, chunks of 1024), ring
  size 8192 for snake (FPS=10 means the pump only fires every
  100 ms; the larger ring survives the drain cycle without
  under-production).

**Asset-load error logging (the user asked for this).**
Previously failures were silent — asset_load_sprite returned 0,
asset_load_sound returned 0, and the programmer had a silent
transparent sprite / silent game with zero information. Now:
- `stbsound`: prints `stbsound: '<path>': <reason>` for each
  reject case. `<reason>` is one of: `file not found or
  unreadable`, `truncated (under 12 bytes)`, `not a RIFF/WAVE
  file`, `missing fmt chunk`, `missing data chunk`,
  `unsupported PCM bit depth <N>`, `IEEE float must be 32-bit,
  got <N>-bit`, `format code <N> not supported`, `<N> channels
  not supported`.
- `stbimage`: `stbimage: '<path>': file not found or
  unreadable` / `not a valid PNG or unsupported chunk layout`.
- `stbfont`: `stbfont: '<path>': file not found or unreadable`
  / `no 'head' table (not a valid TTF)`.

Programmers finally see what went wrong without breaking out
`bug`.

**Snake closes the dog-food loop.** `snake_maestro.bpp`:
- `stbasset` loads `assets/samples/snake_loop.wav` (recorded
  in mini_synth).
- `mixer_play_music(..., loop=1)` on the dedicated music slot.
- Music bus drops to 60% so the eat SFX stays punchy.
- Eat SFX generated in code — `_gen_shot_sample()` builds a 60
  ms 880 Hz square wave with a linear decay envelope on the
  tail 40%. First sample at full amplitude means instant
  attack (the recorded snake_shot.wav had a slow onset that
  lagged behind the apple-eat frame). Called via
  `mixer_play_sample(shot_buf, shot_frames)`.
- Audio pump at the tail of solo_phase (10 Hz = once per
  snake tick); 8192-frame ring survives the 100 ms drain
  between ticks.

Every sound audible while snake runs now came from B++ code:
the instrument, the recording, the decoder, the mixer, the
ring buffer, the FFI, the bus gain, the eat-SFX generator.
**A cobra comeu o próprio rabo.**

**how_to_dev scope rule.** User called out a waste pattern
(running bootstrap + full suite after every game-file touch).
Added a table to Part 1 mapping what-you-changed → minimum
verification. Game/tool edits only compile that artifact;
stb/ needs full suite; src/ compiler needs bootstrap + suite;
docs need nothing.

**Counts.** 20 stb modules (unchanged), 24 compiler+runtime
modules in src/ (bpp_path new). 75 tests passing (rhythm
adds test_beat_map). Bootstrap sha stable
(`d08a9e99...`). Zero warnings across snake_gpu /
snake_maestro / pathfind / platform / plat_noasset /
rhythm / mini_synth.

**Next up (branch point).** Infra week budget remains. Two
paths diverge:

- **(A) ModuLab port** — port the user's JS pixel art editor
  (4 files, 1415 lines) to B++ as tools/modulab/. Estimated
  4-5 days including the last infra pieces (bpp_json, stbui
  widgets, optional stbwindow file dialogs). Delivers a
  complete native tool for producing game art in the B++
  ecosystem.
- **(B) FPS prototype (Wolf3D)** — jump straight to a first-
  person CPU raycaster + WL1 shareware loader. The last
  "genre of game" the fleet still doesn't have. 2-3 weeks
  for the vertical slice. Art comes from the shareware files,
  not ModuLab.

Both are valid closes-of-the-week. ModuLab is deeper
dog-fooding; Wolf3D is broader genre coverage. User to
decide.

## 2026-04-19 — ModuLab sprint starts: the tool infra bundle

User picked ModuLab. Before touching the port itself, today built
the four pieces of infra every pixel-editor-shaped tool needs.
The port proper starts tomorrow with every dependency already
shipping.

**Window centering fix.** Games were spawning bottom-left because
`_stb_init_window` left the initial origin at `(100, 100)` in
Cocoa coordinates (origin = bottom-left corner). Added
`-[NSWindow center]` before `makeKeyAndOrderFront`. All games
now open centered on the active screen — snake, rhythm,
platformer, pathfind, synth.

**stbwindow.bsm (new).** Public API for native dialogs:
`window_save_dialog`, `window_open_dialog`, `window_alert`,
`window_confirm`, `window_error`. Backed by objc_msgSend wrappers
in the platform layer hitting NSSavePanel / NSOpenPanel / NSAlert.
All `-[runModal]` calls block the main thread until dismissed —
the expected UX for save flows. Dialog returns malloc'd paths the
caller frees; alerts return void except `confirm` which returns
1/0 for OK/Cancel. `_gpu_nsstr` dropped its `static` guard so the
platform-side dialog functions can reuse the same NSString builder
the GPU code uses.

**stbinput text-input stream.** Added `_stb_text_buf` (64-byte
ring), `_stb_text_len`, cleared at the top of every frame by
`_input_save_prev`. Platform's NSEvKeyDown handler grabs
`-[NSEvent characters]` and feeds printable ASCII (0x20..0x7E)
into the ring via `_input_push_char`. Public API: `input_text_len()`
+ `input_text_char(i)` for widgets to iterate. Control keys
(arrows, ESC, backspace, etc.) stay in the key_pressed lane so
text widgets can still handle editing commands explicitly.

**bpp_json.bsm (new, ~800 lines).** Full reader + writer, no
external dependencies. Reader uses a flat node table plus a
string pool plus per-container child-index lists; writer uses
an sb_* string builder with depth-tracked comma emission. Types
supported: objects, arrays, strings with full escape set
(including `\uXXXX` transcoded to UTF-8), ints, bools, null,
floats (integer portion preserved; fractional + exponent
consumed but discarded in MVP).

Two bugs caught during test bring-up:

- **Null-terminator overwrite.** `_json_intern` committed
  `strings_len = off + len` (no terminator). Next intern
  overwrote the terminator → `json_object_get("name")` failed
  because the comparison loop read `'s'` from "snake" where
  `'\0'` was expected. Fix: `strings_len = off + len + 1`.

- **Non-contiguous children.** Parser assumed a container's
  children were stored at `first_child + i` in the node table.
  Recursive descent interleaves nested-container descendants
  between siblings of the outer container, so that assumption
  broke once the test had a nested array. Fix: each container
  node allocates its own word array of child-node indices;
  `json_child` dereferences that list. `json_free` walks the
  node table and frees every container's child list.

The test exercises object with string / int / bool / null /
array of strings / array of arrays of ints — every round-trip
survives. Typed convenience getters (`_int`, `_string`, `_bool`
with defaults) work. 77th passing test.

**stbui widgets.** Three new widgets for the tool-shaped apps:

- `gui_text_input(ti, x, y, w, h)` — editable text field with
  click-to-focus, backspace, Enter-commit. Caller owns a
  `TextInput` struct + backing buffer via `text_input_new`;
  `text_input_set` / `text_input_clear` for programmatic edits.
  Draws a focused border when active, caret after the last
  character, consumes chars from stbinput's per-frame stream.
- `gui_grid(x, y, cols, rows, cell_size, border)` — empty-cell
  grid with hover highlight, returns the clicked cell index
  (0..cols×rows-1) or -1 for no click. Foundation for the
  tilemap editor and ModuLab's pixel canvas.
- `gui_palette(x, y, colors, n, per_row, cell_size, selected)` —
  swatch array with a selected-border accent. Direct use in
  ModuLab's palette row + any theme picker.

78th passing test.

**Rebuild-scope rule documented.** User flagged the waste of
running full bootstrap + suite after every touch. Added a table
to `docs/how_to_dev_b++.md` mapping what-you-changed →
minimum verification. Game/tool edits compile that one artifact;
stb/ needs the full suite; src/ compiler needs bootstrap + suite;
docs need nothing. Kills a big source of unnecessary rebuild
cycles in a typical session.

**Counts.** 25 bpp_* runtime + compiler core in src/, 21 stb
modules (stbwindow added), 78 passing tests / 11 skipped, zero
failures. Version bumped 0.75 → 0.78.

**Tomorrow.** ModuLab port proper. Start with
`tools/modulab/core.bsm` (universes, paletas, state struct) and
`canvas.bsm` (brush, eraser, fill, line, rect, oval, select,
undo/redo snapshots). Day 2: I/O (export JSON, spritesheet PNG
grid, import JSON). Day 3: UI wiring (palette row, frame list,
tools sidebar) + integration test (paint a sprite in ModuLab,
reload in a game).

## 2026-04-20 — GPU palette pipeline + animation runtime

Full retro graphics stack day. Shipped the palette-indexed GPU
rendering path (Amiga / SNES-style palette cycling + damage flash
+ colour ramps), extended the Layer primitive for byte-grid ops
so level_editor stops carrying a duplicate grid, wired animation
playback into the testbed so painted frames animate on Tab-in,
and migrated pathfind as the first game consumer of the indexed
pipeline.

**Window sizing fix (macOS).** `_stb_init_window` hard-coded a
3x logical-to-screen scale, which blew past the MacBook's visible
frame for any game asking for ≥640-wide windows — the top of the
window ended up behind the menu bar. `CGMainDisplayID +
CGDisplayPixelsWide/High` now probe the active display at startup
and pick the largest `scale ∈ {1,2,3}` that fits with 80 pt
vertical reserve for menu bar + title bar. `_plt_scale` is stashed
as a module global so `_stb_poll_events` divides
`locationInWindow` by the same factor (not the old 3.0 literal)
when mapping to the framebuffer.

**UI button audit.** `UI_FONT_SZ` had flipped from multiplier
(2 → 16 px) to pixel height (sz=16 → 16 px) during the TTF
transition, but button widths in ModuLab + level_editor were
still sized for 8 px/char. `gui_button` now centres its label
(falling back to a 4-px left margin if the label overflows) and
offending call sites (`Save/Open/Import/Sheet` in ModuLab topbar,
`Dup`/`Del` in the frame strip, tool sidebar's `Eyedrop`, level
editor's `Save`/`Open`) were widened to fit 16-px glyphs. Canvas
origin in ModuLab shifted from x=120 to x=144 so the wider tool
sidebar doesn't overlap.

**stbpal.bsm (new, ~600 LOC).** Palette extracted as a first-
class pillar because both `stbdraw` (CPU layer compositor) and
`stbrender` (new GPU indexed path) consume it — burying it in
`stbdraw` would have made `stbrender` import it sideways through
its own dependency. `struct Palette { count, data }` + 7 built-
in catalogs: MCU-8, NKOTC-4, CB-32, DB-32, PICO-8, GB-4, NES-54.
Cycling (rotate a slot range every period_ms, phase-correct under
big dt), LUT (`palette_lut_flash` + generic `palette_lut_apply`),
linear lerp between two palettes, and .pal.json save/load round-
trip are all pure CPU. Tests: `test_stbpal_builtin`,
`test_stbpal_cycle`, `test_stbpal_lerp_lut` — all green.

**GPU indexed rendering path.** Three layers extended:

- **Metal shader** (`_stb_platform_macos.bsm`). Added a second
  sampler (`smp_n`, nearest) and a second fragment texture slot
  for the palette. Third-bucket use_tex marker (192) routes
  between font (255), indexed (192), RGBA sprite (128), and
  solid (0). Indexed path: sample R8 → index → sample 1×256
  BGRA palette texture at `(idx + 0.5) / 256.0`. Discard on
  index 0 to preserve transparency convention.
- **Platform helpers.** `_stb_gpu_create_texture_r8` for sprite
  indices, `_stb_gpu_create_palette_texture` (1×256 BGRA,
  densifying palette.data's 8-byte-stride words into tight 4-
  byte pixels), `_stb_gpu_update_palette_texture` for per-frame
  refresh, `_stb_gpu_bind_palette` (slot 1), `_stb_gpu_sprite_indexed`.
- **stbrender API.** `palette_gpu_upload(pal)` →
  handle, `palette_gpu_update(handle, pal)` (1 KB re-upload),
  `sprite_create_indexed(bytes, w, h)`, `sprite_draw_indexed(sprite_tex,
  pal_tex, x, y, w, h, sz)`. Every sprite sharing a palette
  handle re-skins in one upload — cycling, flashing, day/night
  all scale O(1) in sprite count.

**pathfind migration.** Ported first as proof: `PAL` raw word
array → `pal_normal` + `pal_flash` Palette structs, sprites
switched to `sprite_create_indexed`, draws via
`sprite_draw_indexed`. On rat hit, `palette_gpu_update(pal_gpu,
pal_flash)` swaps every visible sprite to pure white for 80 ms —
the classic NES hit sparkle — with a timer back to `pal_normal`.
One upload, every sprite flashes; at 500 sprites the cost would
be identical.

**gpu_palette_cycle example.** Minimal viable demo: 64×64
sprite of 4-pixel horizontal bands using GB-4 indices 1..4,
`palette_cycle_begin(pal, 1, 4, 150)` rotates the four greens
every 150 ms, `palette_cycle_tick + palette_gpu_update` per
frame — bands flow downward without the sprite texture ever
re-uploading. User confirmed the visual works.

**stbdraw Layer ops.** `Layer` was the compositor's unit with
`{fb, w, h, alpha, blend, visible}`; level_editor was the second
client wanting the same byte grid. Rather than extract
`stbcanvas` (extraction-for-purity, cost > value on a shared
surface of ~60 LOC), added `layer_get / set / clear / fill` to
the existing Layer section. `layer_fill` is a 4-connected BFS
flood fill. level_editor migrated: `_lvl_tiles` (raw malloc)
→ `Layer*` via `layer_new`, paint/erase/fill delegate through
layer helpers, undo ring still copies `l.fb` bytes via the struct
slice. 40 LOC deleted. One new test: `test_stbdraw_layer_ops`.

**Testbed level loader.** `testbed_load_world(path)` in stbforge
consumes the level_editor `.level.json` schema (any palette index
> 0 = solid). Dimensions resize the world grid in place; spawn
recentres. ModuLab's `_cycle_mode` probes
`path_asset("testbed.level.json")` on Tab-into-testbed — present
loads, absent leaves the default hand-drawn platformer world.

**Character format + testbed recorder** (earlier-session leftover
from the consolidated stbforge pillar — testbed runtime, character
load/save, recorder ring, inspector — all listed here so the
status is explicit): `.character.json` schema round-trips cleanly,
recorder captures inputs + snapshot checkpoints, testbed_update
threads input through the recorder so playback replays physics
deterministically.

**Animation playback runtime** (stbforge). `testbed_play_animation(i)`
/ `testbed_find_animation(name)` / `testbed_anim_tick(dt)` /
`tb_current_frame_idx` / `tb_anim_idx` / `tb_anim_frame`. Phase-
correct under big dt, looping + one-shot (clamps at last frame).
`testbed_update` auto-ticks; `testbed_draw` resolves source
frame through `tb_current_frame_idx` so animation-off flows stay
backwards-compatible. ModuLab's `_cycle_mode` now calls a fresh
`_rebuild_testbed_animation` helper on every Tab — clears prior
animations, registers a single "all" animation covering every
painted frame at 8 fps looping, auto-plays anim 0 if multi-frame.
Single-frame projects keep the legacy static rendering. New
helper in stbforge: `character_clear_animations` frees every
Animation + its frames and resets anim_count. Test:
`test_modulab_anim_playback` — step, sub-period, loop
wraparound, stop, non-loop clamp.

**sprite_viewer lint.** `draw_text("+ / -  zoom   ESC  quit",
..., 1, ...)` was the last site triggering the transitional
`draw_text: sz<8` warning. Bumped sz to 8.

**Name collisions from stbpal extraction.** ModuLab core had a
zero-arg `palette_count()` returning `g_pal_n`; conflicted with
stbpal's `palette_count(pal)`. Renamed ModuLab's to
`g_palette_count`. pathfind's public `init_palette` collided
with stbpal's — made pathfind's static + renamed
`_pathfind_init_palette`.

**Counts.** 108 passing / 3 failed / 11 skipped (the 3 failures
are flaky GPU tests — exit 137 SIGKILL from macOS codesign cache,
oscillate between pass/fail run-to-run, not regressions). 22 stb
modules (`stbpal` added). New tests: `test_modulab_testbed_level`,
`test_modulab_anim_playback`, `test_stbdraw_layer_ops`,
`test_stbpal_builtin`, `test_stbpal_cycle`, `test_stbpal_lerp_lut`.

**Open items for tomorrow.**
- `dialog_editor` as third dev tool — probably the extraction
  trigger for something else (animation editor UI? dialog
  state machine? TBD once scope is clear).
- Testbed controls not firing from ModuLab's editor (A/D/Space
  possibly intercepted upstream). Low priority, but user flagged
  it when animation playback was confirmed working.
- Testbed world defaults to hand-drawn platformer layout —
  documented in response but worth documenting here: drop a
  `testbed.level.json` next to the binary to override.
