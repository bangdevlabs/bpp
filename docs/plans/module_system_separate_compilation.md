# Plan — module system / separate compilation (big-league table stakes)

**Opened 2026-07-08.** Every mature systems language the b++ project measures
itself against — C/C++, Rust, Zig, Odin, Jai — has **separate compilation**:
compile each unit to an object, then link. b++ today does **whole-program**
compilation (all imported modules → one executable in one pass). This arc adds
separate compilation so large b++ projects get **incremental builds** (recompile
only the changed module).

## Honest driver

There is NO measured build-time pain today: b++ self-compiles (a ~20K-line
compiler) in seconds. The consumer is CONCRETE but FUTURE: large b++ codebases /
games where a one-file change shouldn't retrigger a whole-program recompile. This
is a "play in the big league long-term" investment (the user's explicit call),
not an urgent fix. Build it deliberately, measure the incremental-build win on a
real multi-module project (a game).

## What exists today

- **Whole-program**: `cg_emit` walks all modules, emits one ELF/Mach-O
  executable. Multi-module SUPPORT exists (import/load, the load/import arc,
  per-module externs), but everything compiles together.
- **The Wave 16/17 scaffolding** (reserved for exactly this arc): the
  ChipPrimitives slots `emit_module_setup`, `emit_module_finalize`,
  `alloc_fn_labels`, `emit_prelude`, `emit_global_data`, `emit_postlude` — all
  0-call empty stubs today, wired but inactive.
- **Object emitters**: the ELF (`a64_elf` / `write_elf_dyn`) and Mach-O emitters
  produce EXECUTABLES + already do relocations (FFI, GOT/PLT, dynlink). Producing
  RELOCATABLE OBJECTS (.o) is the missing output mode.
- **The re-install note** (journal): the per-`emit_module`
  `cg_install_<chip>_primitives()` must move with `emit_module` when Wave 16
  activates — either the chip's `emit_module` stays a thin shim (install +
  `cg_emit_module`), or `cg_emit_module` gets target-aware install (the plan
  calls the second cleaner).

## The design

1. **Object output mode.** Extend the ELF/Mach-O emitters to write a
   RELOCATABLE object (`.o`): a symbol table (defined + undefined), relocations
   for cross-module references, sections (.text/.data/.rodata/.bss). Most of the
   relocation machinery exists (dynlink); this is a new output KIND, not new
   relocation logic.
2. **Per-module emission.** `emit_module_setup` opens a module's sections;
   `cg_emit_module` emits its functions + data; `emit_module_finalize` closes it
   into a `.o`. Cross-module calls/globals become undefined symbols + relocations.
3. **Linking.** Combine the `.o`s. Option A: emit a final link step that resolves
   the relocations and produces the executable (b++'s own mini-linker — the
   reloc code is already there). Option B: emit standard `.o`s and shell out to
   the system `ld`. Option A keeps b++ self-contained (no toolchain dep, matches
   the "b++ is the whole toolchain" ethos); Option B is less code. **Lean A** for
   the executable path, allow B for interop.
4. **Incremental driver.** A build front-end: hash each module's source; recompile
   only modules whose hash (or a dependency's) changed; relink. This is where the
   actual win lands.

## Increments

- **M1 — object output for ONE module.** Add a `.o` output mode to the ELF
  emitter: symbol table + relocations + sections for a single module. Verify: a
  trivial 2-module program compiled as 2 `.o`s + linked (via A or `ld`) runs
  identically to the whole-program build.
- **M2 — activate the Wave 16 primitives.** Route function/data emission through
  `emit_module_setup` / `cg_emit_module` / `emit_module_finalize`; move the
  per-module primitive re-install as the journal notes. Whole-program build stays
  byte-identical (the primitives just wrap the existing path when there's one
  module).
- **M3 — the mini-linker (Option A).** Resolve cross-module relocations, combine
  sections, emit the executable. Reuse the dynlink reloc code.
- **M4 — incremental front-end.** Source hashing + dependency tracking +
  recompile-changed-only. Measure the incremental-build win on a multi-module
  game (change one file, time the rebuild vs whole-program).

## Gate + measurement

Whole-program build stays byte-identical through M1-M2 (separate compilation is an
alternative path, not a replacement, until proven). Self-host stable both
backends. THE measurement (M4): incremental rebuild time on a real multi-module
project vs the whole-program time — the win must be real (change 1 of N modules →
~1/N the build time).

## Honest scope note

This is the largest of the three scaffolding arcs — a real feature (object
format, linking, incremental driver), multi-session. It has a concrete future
consumer (large-codebase incremental builds) but no measured pain today. Sequence
it as the long-term big-league investment it is; do NOT rush it. The Wave 16
scaffolding is correctly KEPT (documented reserve) until this arc executes.
