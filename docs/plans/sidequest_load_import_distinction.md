# Sidequest — `load` vs `import` proper distinction

**Status:** PROPOSED 2026-05-22, design-phase open.

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
