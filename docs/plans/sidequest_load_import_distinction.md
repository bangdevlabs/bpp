# Sidequest — `load` vs `import` proper distinction

**Status:** DESIGN-LOCKED 2026-05-22 (followup section at the end
of this doc supersedes the original "Tier A vs Tier B" framing
and the "Next-session scope (revised)" section).

**Trigger:** Autovec P6 Phase 2 (compose outlining + autovec) shipped
2026-05-22 (commit `2384b46`). Single-module test programs measure
6x compose vs serial. BUT game adoption blocked by a latent
incremental-emit bug surfaced when `wolf_ai_tick_cooldowns` was
added to FPS (`f09e5ad`):

  * Smart-dispatch correctly identifies the candidate
  * `synthesize_loop_fn` creates the synth function
  * `_outline_rewrite_with_captures` mutates the host's T_WHILE
    into a T_BLOCK with publish + `job_parallel_for_data` call
  * BUT modules 1..N in incremental mode emit BEFORE the
    dispatch pipeline runs — host's binary code retains its
    original scalar form, synth never emitted

Diagnosed via `bug --dump /tmp/fps_w.bug`: 12 synth functions
registered in debug map, ZERO in binary `nm` output.

**The "always was" nature:** smart-dispatch outlining shipped
2026-05-22 (`7da49fc`-`ab8e0aa`). Pre-`f09e5ad`, no real game
code triggered outlining (game hot loops used explicit
`ecs_query_each(fn_ptr(...))` patterns that fail at gate 7).
The bug existed but was dormant. Game adoption exposes it.

## Two concepts currently conflated

| Concept | Used for | Per file today? |
|---|---|---|
| **Source file** | `.bug` debug map, source-line attribution, error messages | YES |
| **Compile module** | parse/emit pipeline boundary, incremental compile | YES (same entry as source file) |

The conflation means: `load` and `import` both create a new
*compile module* AND a new *source file* entry. They differ only
in path resolution (entry-relative vs full-search).

## The proposed distinction

Per user clarification 2026-05-22: "all games use `load` for
internal modules, `import` for stb cartridges. They don't
touch src/ by principle."

| Semantic | `load` | `import` |
|---|---|---|
| Path resolution | entry-relative | full search (stb/ + installed) |
| **Source file entry** | **YES (preserve debug)** | YES |
| **Compile module** | **REUSES parent** | NEW module |
| Dispatch pipeline visibility | merged into parent | separate |
| Synth function attribution | parent's module | own module |

### Why this distinction matters

  * **`load` = project-local helpers.** Game devs split internal
    code across files for organization. They expect this code
    to behave as if inlined — no module boundary semantics.
    Currently the implicit boundary blocks autovec/outlining
    when hot loops live in `load`-ed files.
  * **`import` = engine cartridges.** stb cartridges are
    cross-project, isolated API surfaces. Module boundaries
    are real (separate compile units, separate emit timing).
  * **Source-line attribution preserved.** bug debugger still
    knows `ai.bsm:42` even though that file's *compile module*
    is now module 0. Necessary for game-dev debugging.

## Scope decision — Tier A vs Tier B

**Tier A — `load` inline (this sidequest)**

Make `load`-ed files share parent's compile module while
keeping their own source-file entry. Game adoption (FPS, RTS,
all current games) becomes possible because hot loops in
`load`-ed files go through the same dispatch + emit cycle as
the entry file.

  * **Pro:** Direct fix for the use case that exists today.
    All games immediately benefit.
  * **Pro:** Doesn't touch incremental-emit architecture for
    `import` modules — they keep their fast per-module emit.
  * **Pro:** Source-line attribution preserved (compile module
    is internal; source file is the user-visible concept).
  * **Con:** Adds an asymmetry — `load` vs `import` semantics
    diverge for compile module. But the semantics are
    documented and match user mental model ("load = inline,
    import = boundary").

**Tier B — `import` incremental-emit fix (future arc)**

Re-emit `import`-ed modules after dispatch when they had
candidates rewritten. Enables autovec/outlining inside stb
cartridges (future opportunity).

  * **Pro:** Uniform fix — both load and import benefit.
  * **Con:** Substantial architectural work (~200-300 LOC).
    Re-emit requires either patching in-place code or
    rewinding outbuf, both invasive.
  * **Pro long-term:** unlocks compose-multiplicatively in
    engine code (stbmixer, stbphys, future cartridges).
  * **Out of scope for THIS sidequest.** Opens as separate arc
    when first stb cartridge develops outlining-eligible
    patterns. Today every stb hot loop has function-call /
    multi-stmt-body shape that gate-7/8/9 reject.

## Design decisions (resolve before P1)

### Q1 — Module entry creation for `load`

Two implementations possible:

**(a) Skip diag_files entry for inlined `load`**: the loaded
file content streams into the parent's diag_files entry.
- Pro: Simplest. ~30 LOC.
- Con: bug debugger doesn't know "this line came from ai.bsm".
  Source-line attribution lost — debugger reports parent
  source line for inlined code.
- Verdict: REJECTED. Source-line attribution is a quality
  regression too steep.

**(b) Create diag_files entry but mark as inline**: source-file
entry preserved (bug debugger keeps attribution), but the
file's functions get the PARENT's compile module index.
- Pro: Best of both worlds.
- Con: Decouples source-file from compile-module, needing
  new tracking arrays.
- Verdict: ACCEPTED. The right semantic.

### Q2 — Compile module tracking

New per-source-file metadata: `diag_file_compile_module[i]`.
Array indexed by diag_file index, value is the compile
module index (which can be shared by multiple source files).

Helper: `compile_mod_of(diag_file_idx)` → returns the compile
module.

Codegen + dispatch consumers that read `func_mods[i]` get
COMPILE module (already-correct for non-inline). New consumers
that need SOURCE file: query diag_files directly.

### Q3 — process_file inline-mode flag

Static `_process_file_inline_into` — set by caller before
recursing for a `load`. process_file reads it, consumes it
(sets to 0), and decides whether to create new compile module
or reuse current top.

```c
static auto _process_file_inline_into;  // 0 = new module, 1 = reuse parent

// Call site for load:
if (_import_is_load) {
    _process_file_inline_into = 1;
}
process_file(pathbuf, plen);

// Inside process_file:
auto inline_mode = _process_file_inline_into;
_process_file_inline_into = 0;  // consume

if (inline_mode) {
    my_compile_mod = parent's compile mod  // current top
} else {
    my_compile_mod = arr_struct_count(diag_files) - 1  // new
}
```

### Q4 — Dependency tracking

`dep_from` / `dep_to` arrays track import dependencies. For
`load`-inline, the dependency is "this file content is in
the same module as parent". The dep edge becomes redundant
when the file is part of the same compile module.

Decision: don't push to dep_from/dep_to for inline loads. The
parent's deps cover the loaded file's deps transitively (each
import in the loaded file is a real dep of the parent).

### Q5 — Boundary records

`_record_bnd(mod_idx, outbuf_line_offset)` marks where a
module's content starts in the merged outbuf. For inline loads,
we DON'T record a new boundary (compile module is unchanged).
BUT we DO update the source-file's line_start so the bug
debugger can map outbuf lines back to source file lines.

### Q6 — Cross-platform compatibility

`bug --dump` output displays "function X in module Y". With
inline loads, X's source file (ai.bsm) is preserved but
compile module is 0 (entry). Display priority:
- Source file: shows ai.bsm in source line attribution
- Compile module: shown only in the per-module emit metadata

The user-facing bug output should prioritize source file.

### Q7 — Tests

Tests required:
1. `tests/test_load_inline.bpp` — entry .bpp + `load "test_load_inline_helper.bsm"`. Helper has a function `outline_eligible_helper()` with caller-frame refs. Verify:
   - Compiles clean
   - Synth function appears in `nm` output
   - Helper's source-line attribution preserved in `.bug` map

2. `tests/test_load_vs_import.bpp` — distinguishes the two
   forms. Import a cartridge, load a helper. Verify import's
   module is separate, load's is inlined.

3. FPS rebuild — `wolf_ai_tick_cooldowns` synth appears in
   binary, host body rewritten to call `job_parallel_for_data`.
   Empirical AV_P1 fire confirmed.

### Q8 — Acceptance gates

Per Tonify Rule 37:
1. Bootstrap byte-stable across phases.
2. Native suite 179/0/12 + C-emit suite 144/0/47.
3. FPS adoption empirical: `nm /tmp/fps_w` shows `__synth_K`
   functions, `objdump` shows host has `bl job_parallel_for_data`.
4. RTS adoption can begin (separate sidequest).

## Phase plan (~150 LOC, 1-2 sessions)

| Phase | What | LOC | Risk |
|---|---|---|---|
| P0 | Design lock (this doc) | — | low |
| P1 | `diag_file_compile_module[]` array + `compile_mod_of()` helper. Initial state: each diag_files[i] maps to compile module i (1:1, no change). | ~30 | low |
| P2 | `_process_file_inline_into` flag in `bpp_import.bsm`. Load call site sets it. process_file reads + consumes + branches. | ~50 | medium |
| P3 | Update all `func_mods` / module-index queries that should use compile module. Audit codegen + dispatch + emit consumers. | ~30 | medium |
| P4 | Bug debugger `.bug` map: prioritize source file in line-attribution display. | ~20 | low |
| P5 | Tests + FPS rebuild validation. | ~50 | low |

**Total: ~180 LOC. 1-2 sessions estimate.**

Risk peak at P2 (process_file behavior change) and P3 (audit
all consumers — easy to miss one).

## Out of scope (this sidequest)

  * **Tier B (`import` re-emit)** — separate arc. Open when
    first stb cartridge surfaces outlining-eligible pattern.
  * **Strict module isolation** — B++ doesn't have namespaces;
    function calls are globally resolvable. This sidequest
    preserves the status quo. Adding isolation is a separate
    design discussion (probably never; it's not B++'s style).
  * **`load` chains** — `load A; A loads B; B loads C`. The
    fix is recursive: every load goes inline into the
    inline-mode's parent compile module. For game code this
    is module 0 (entry .bpp).

## Session 2026-05-22 — investigation findings

Attempted P2 (load inline flag) + P3 (parser uses
`compile_mod_of`) implementation discovered two architectural
issues deeper than the original scope assumed:

**1. `tok_mod` / `mod_idx` known-broken for nested imports.**
Existing comment in `bpp_parser.bsm:45`: *"without relying on
tok_mod() which returns incorrect module attribution for
module 0 (its content sits at mod0_real_start, past all
auto-injected modules)"*. The same issue applies to any
module with nested imports — `mod_idx` returns the diag_files
idx whose `.start <= src_off`, but when a module imports
others recursively, its content gets split into chunks. The
later chunks have `.start` ≥ next file's start, so binary
search returns the wrong (nested) module.

Empirical confirmation: compiling FPS, `tok_mod` for
`wolf_ai_tick_cooldowns` returns 39 (some nested import's
idx), not 37 (ai.bsm's actual idx). `compile_mod_of(39)`
returns 39 (identity, since P2 only set the entry for ai.bsm
at idx 37).

**2. `lex_module(i)` partitions by a single contiguous range
`[diag_files[i].start, diag_files[i+1].start)`** — doesn't
account for the file's content RESUMING after nested imports
return. The later content gets lexed under whichever module
happens to own that outbuf range, not the original file's
module.

The CORRECT semantic primitive is `diag_mod_idx` (uses
`mod_bnds` boundary records which DO track interleaved-import
correctly). But switching parsers + per-module emit
attribution to use `mod_bnds`-based lookup is substantially
more work than the original scope.

**Deferred-emit approach attempted and reverted:** changed
the orchestrator to defer modules 1..N emit until after
dispatch. Bootstrap broke (gen1 produced an incomplete binary
missing `_main`). The reason: `emit_module_arm64` has
ordering-sensitive interactions with `cg_bridge_data` +
`cg_fn_name` accumulation + label allocation that need
detailed sequencing. Not safe to defer without redesigning
those interactions.

## Next-session scope (revised)

The architectural fix is bigger than this sidequest's
original ~180 LOC estimate. Real scope:

  * Either: switch parser + emit-attribution to use
    `diag_mod_idx` (mod_bnds-based) instead of `tok_mod` /
    `mod_idx`. Then the inline-load mapping can apply
    cleanly. ~150-200 LOC, touches parser + several callers.
  * OR: redesign incremental-emit ordering. Decouple parse
    from emit. Track which modules need re-emit after
    dispatch. ~200-300 LOC, touches orchestrator + module
    metadata.

Both require careful bootstrap-verify per phase. Better to
treat as a 2-3 session arc with proper design checkpoints
than a quick fix.

**Current state preserved:**
  * `0f68dfa` — P1 array infrastructure (committed, harmless
    identity mapping until consumers wired up).
  * `38fd075` — Original design doc.
  * This update — findings honest record.

## Cross-references

  * `docs/plans/sidequest_autovec_b4_completion.md` — sidequest
    that surfaced this bug
  * `docs/manual/tonify_checklist.md` Rule 4 — annotation
    doctrine (separate concern; this sidequest doesn't change)
  * Commits `f09e5ad` (FPS adoption), `a2d5fdf` (P6 Phase 1),
    `2384b46` (P6 Phase 2), `4b98e7c` (sidequest doc + bug
    diagnosis) — the trail that led here
  * `src/bpp_import.bsm` — primary file modified
  * `src/bpp.bpp` — orchestrator; verify both incremental and
    monolithic paths still work

---

## Design lock (2026-05-22 followup — supersedes Tier A vs Tier B)

After a second deep-read of the source (sessions late 2026-05-22),
the original "two-tier" framing turned out to be a remediation,
not a root-cause solution. The new design is grounded in the
actual call chain that produces the bug.

### The bug as a cascade, not a point

```
process_file recurses on nested imports
  ↓ outbuf becomes interleaved (parent | child | parent | grandchild | parent ...)
lex_module(mi) partitions by the range [diag_file_start(mi), diag_file_start(mi+1))
  ↓ parent's resumed content (after child) lands in the next module's range
parse_function reads my_mod = tok_mod(tok_pos) → mod_idx(tok_pos)
  ↓ wrong module attribution
func_mods[i] = wrong module
emit_module_arm64(mi) filters by func_mods[i] == mi
  ↓ function emits in wrong module, OR never emits, OR emits in a module that ran before dispatch
run_dispatch rewrites the AST of an already-emitted function
  ↓ rewrite silently lost — synth functions in .bug map, zero in binary
```

The root is **step 2 (lex_module's contiguous-range assumption)**.
All downstream consequences chain from there.

### The two emit APIs and why the agent's deferred-emit attempt failed

The ARM64 backend has two emit paths today:

| API | Population strategy | Today's use |
|---|---|---|
| `emit_all_arm64()` in `a64_codegen.bsm:2357` | `cg_bridge_data()` clears + rebuilds `cg_fn_name` from `func_cnt`; allocates labels for all functions; emits each via `cg_emit_func(i)` | `--monolithic`, `--c`, `--asm` (proven byte-stable) |
| `emit_module_arm64(mi)` in `a64_codegen.bsm:2269` | Appends to existing `cg_fn_name` filtering by `func_mods[i] == mi`; allocates labels only for new functions; emits this module's subset | Default native binary (incremental path) |

The agent's earlier "defer modules 1..N emit" attempt kept the
**append** strategy but reordered:

```
parse 1..N → parse 0 → dispatch → emit_module(0) → emit_module(1) → ... → emit_module(N)
```

This broke because `cg_fn_name` got synth functions (emitted via
module 0 first) prepended before the original modules' functions
got appended. Label IDs shifted vs the previous order, so
`bo_resolve_calls_arm64()` bound calls to the wrong targets —
gen1 produced a binary missing `_main`.

The fix is to switch the per-module path from **append** to
**rebuild-and-slice**: call `cg_bridge_data()` once after
dispatch to populate `cg_fn_name` with all functions (synths
included, in `func_cnt` order — the same order monolithic mode
already uses byte-stably), then have each `emit_module_arm64(i)`
WALK the existing table and emit the subset where
`func_mods[i] == i`.

### What this preserves vs what changes

| Aspect | Preserved | Changed |
|---|---|---|
| Per-module emit organization (`emit_module_arm64(i)` per `i`) | ✓ | — |
| `.bug` map per-module function attribution | ✓ | — |
| `func_mods[]` as the filter primitive | ✓ | — |
| Source-file granularity in error reporting | ✓ | — |
| `cg_fn_name` population strategy | — | **append → rebuild-once** |
| Phase ordering of emit vs dispatch | — | **emit-before-dispatch → emit-after-dispatch** |
| `lex_module` range source | — | **`diag_file_start` → `mod_bnds`** (interleaved-import aware) |
| `tok_mod` / `mod_idx` semantics | — | **converges with `diag_mod_idx`** |
| `flag_incr` / `flag_mono` dual paths in `bpp.bpp:265-414` | — | **removed** (single linear pipeline) |
| `--monolithic`, `--clean-cache` flags | — | **removed** (no longer meaningful) |

**We are NOT regressing to monolithic mode.** The per-module
emit structure stays. Only the *function-table population
strategy* changes (rebuild vs append), and the *phase ordering*
of emit vs dispatch becomes consistent (always emit-after).

### Phase plan (8 phases, ~320 LOC total, 2-3 sessions)

Each phase is one commit, bootstrap byte-stable, suite green
(Tonify Rule 37). Risk peaks at P6 — but the upstream phases
P3+P4+P5 make P6 tractable because `func_mods[]` accuracy is
fixed first.

| Phase | What | Anchor in source | LOC | Risk |
|---|---|---|---|---|
| P1 ✓ | `diag_file_compile_module[]` array + `compile_mod_of()` helper. Initial identity mapping. | shipped `0f68dfa` | done | — |
| P2 | `_process_file_inline_into` flag in `bpp_import.bsm`. Load call site sets it before recursing. `process_file` reads + consumes it + decides whether to set `diag_file_compile_module[my_mod_idx] = parent_compile_mod` or `= my_mod_idx`. | `bpp_import.bsm:594, 687, 1273-1300` | ~50 | medium |
| P3 | Switch `lex_module(mi)` from `diag_file_start` range to `mod_bnds`-based partitioning. New shape: walk `mod_bnds` entries, lex tokens where `b.mi == mi` for each boundary range. Resolves the cascade root. | `bpp_lexer.bsm:679-703` | ~40 | medium |
| P4 | Switch `tok_mod` / `mod_idx` to use `diag_mod_idx` (or alias them). The two converge once `lex_module` is mod_bnds-aware. | `bpp_import.bsm:204-217` (`mod_idx`) + callers | ~20 | low |
| P5 | Audit consumers of `tok_mod(tok_pos)` and `func_mods[i]`. Verify nested-import functions now get correct `func_mods` value. Key site: `parse_function:1789`. Other sites via grep. | `bpp_parser.bsm:1789` + `grep -rn "tok_mod\|func_mods" src/` | ~30 | medium |
| P6 | **Tier B core, the risky one.** Reorder `bpp.bpp:265-414` so `emit_module_arm64(i)` for `i in 0..N` runs AFTER `run_dispatch()`. Refactor `emit_module_arm64`: instead of appending to `cg_fn_name`, READ from it (populated once by a single `cg_bridge_data()` call after dispatch). Per-module emit becomes "walk cg_fn_name, emit where func_mods matches". Same change in `x64_codegen.bsm`. | `bpp.bpp:265-414`, `a64_codegen.bsm:2269`, `x64_codegen.bsm` equivalent | ~80 | **high** |
| P7 | Cleanup: remove `flag_incr` / `flag_mono` dual paths from `bpp.bpp`. Drop `--monolithic` and `--clean-cache` flags. Update `docs/manual/bootstrap_manual.md` "Compiler Flags Reference" + `docs/manual/how_to_dev_b++.md` Cap 48. | `bpp.bpp`, `bpp_args.bsm`, manual chapters | ~50 | low |
| P8 | Tests + game adoption + bench. `tests/test_load_inline.bpp` + `tests/test_load_vs_import.bpp`. FPS rebuild verifies `__synth_K` in `nm` + `bl job_parallel_for_data` in `objdump`. RTS rebuild smoke. `bench_compile.sh` 5-run delta. | `tests/`, FPS+RTS rebuild | ~50 | low |

### Session 2026-05-22 (later) — P3 scope revision

Empirical finding while attempting P3: **the mod_bnds-aware
lex_module change cannot ship without simultaneously switching
the orchestrator parse loop from mi-ascending to topological
order.**

Why: the orchestrator's `for i in 1..mk-1` (`bpp.bpp:272`)
iterates modules in diag_files CREATION order (depth-first by
process_file). Parents get LOWER mi than their imports (mi is
assigned at process_file entry). With OLD lex_module's
single-range partitioning (`[diag_file_start(mi), diag_file_start(mi+1))`),
each module's lex range silently includes content of its
adjacent sibling/child files in outbuf — an OVER-LEX side
effect that covered the parser's "parent references child's
types before child is parsed" gap.

Switch to mod_bnds-aware lex (correct per-mi partitioning) +
keep mi-asc parse order → bootstrap fails with `unknown type
'ChipPrimitives'`-shape errors at the FIRST module that
references a struct from its own import.

Switch to topological order alone (with OLD lex) → also
breaks because OLD lex over-includes sibling content; iterating
in topo order makes those over-lexed tokens land in the wrong
parse_module pass.

The two changes must land together. Single-commit P3
deliverable shipped here covers ONLY the infrastructure
(abs_line in ModBnd + public accessors). The combined
lex+orchestrator change moves to a follow-up commit ("P3
combined") with both pieces landing atomically.

### Why P3+P4+P5 must land BEFORE P6

`emit_module_arm64(mi)` filters by `func_mods[i] == mi`. If
`func_mods[]` is still wrong (the bug today), the filter picks
the wrong functions even after rebuild-via-cg_bridge_data.
P3+P4+P5 fix `func_mods[]` accuracy. After that, P6's "rebuild
once + per-module emit reads the table" works as intended.

Reverse order would be: rebuild-once works correctly but
filtered-emit still misses functions because their `func_mods`
is wrong. The bug would reproduce at a later stage.

### Acceptance gates (per Tonify Rule 37, per phase)

```
1. Bootstrap byte-stable: g1 → g2 → g3, cmp g2 g3 (g1 may differ
   in P3/P4/P5/P6 due to mod attribution changes — 1-cycle
   oscillation acceptable; g2 == g3 mandatory).
2. Native suite: 179/0/12 (no regression in pass/fail count).
3. C-emit suite: 144/0/47 (no regression).
4. bench_compile.sh: bootstrap delta within ±5%. Larger budget
   than usual because P6 reorders the entire emit phase.
5. (P8 only) FPS empirical: `nm /tmp/fps_w | grep __synth` shows
   the synth functions, `objdump -d /tmp/fps_w | grep "bl.*job_parallel_for_data"`
   shows the host body calling the worker dispatcher.
```

### What this sidequest is NOT

- **Not threading the compiler.** Single-thread stays. Multi-thread
  is its own arc, behind significant bench-justified pressure.
- **Not adding generics / monomorphization.** Future arc.
- **Not bringing back the `.bo` cache.** Future arc; this design
  prepares for it cleanly (per-module emit + post-dispatch
  invariant = right shape for cache key by `(content_hash, dispatch_input_hash)`).
- **Not changing namespace semantics.** Globally-visible
  functions stay globally visible.

### Commit message style (for the implementing agent)

Mirror the S4 / outlining / autovec arcs:

```
load/import P2: process_file inline-mode flag

P2 of the load/import distinction sidequest (see
docs/plans/sidequest_load_import_distinction.md "Design lock"
section). Adds _process_file_inline_into static flag to
bpp_import.bsm. When `load` keyword is detected at the
check_file_import site, the flag is set before process_file
recurses. process_file reads + consumes the flag; if set, the
new diag_files entry's compile_module index points to parent's
compile_module (= reuses parent), instead of pointing at itself
(= new module).

Bootstrap byte-stable: gen1 == gen2 == gen3.
Suite: 179/0/12 native + 144/0/47 C-emit, no regression.
No game behavior change yet (P3+ unlock it).
```

