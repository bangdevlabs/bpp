# Sidequest — bug: fine-grained source-line map + source-line stepping

**Status:** scoped 2026-05-28. Not started. **Prioritized BEFORE
`elf_dynlink_plan.md`** — the dynlink engine (and especially its eventual
debugging on real x86 hardware) should ride on a `bug` that shows source lines
and steps by line, not just functions and instructions.

**Goal:** bring `bug` to DWARF-line-table + lldb-stepping *fidelity*, keeping the
bespoke `.bug` format. Two phases, #2 builds on #1:

1. **Line map** — `--disasm` / backtrace / crash reports show the exact
   `foo.bpp:42`, not just the enclosing function.
2. **Source-line stepping** — `n` (step to next source line), step-over (don't
   descend into calls), `finish` (run to frame return), on top of the existing
   instruction single-step.

We *learn* DWARF's line-table technique and lldb's stepping UX; we do **not**
emit DWARF (bespoke `.bug` stays the format, per the studio philosophy). The one
scenario that might revisit that — wanting gdb/lldb on real x86 hardware where
Rosetta blocks bug's own ptrace — is a separate call, noted in
`nomad_manual.md`, not this plan.

## What bug has today (the starting line)

`bug` is already well past the "primitive" stereotype — this is a refinement,
not a foundation:
- **Per-function source map** (`bpp_bug.bsm` SOURCE MAP section): each function →
  `(module, relative declaration line)`, computed `tok_lines[tidx] -
  diag_file_line_start(mod)`. So bug knows *"`foo` starts at foo.bpp:N"*, not
  *"PC X is at foo.bpp:M"*.
- **Register-aware locals** (`bug_eval`): reads B3-promoted locals via
  `gdb_read_reg`, stack locals via `fp + off` (from `cg_var_promote`/`cg_var_off`).
- **Instruction single-step** (`bug_tui` `s` / `TUI_STEP`), continue, break,
  watch, backtrace, and `bug_disasm` (the objdump-in-bug).

The infra that makes this feasible already exists:
- **`tok_lines[tidx]` + `tok_mod(tidx)` + `diag_file_line_start(mod)`** — the
  token→relative-line conversion the per-function map already uses.
- **`cur_text_pos` chip primitive + `_bug_inl_record(s, e, fidx)`** in the spine
  (`bpp_codegen.bsm`) — inline-range recording that captures `(code PC, …)`
  chip-agnostically during codegen. The line map mirrors this exactly.

## Phase 1 — the line map (PC → `.bpp` line)

### 1a. Record per-statement `(PC, module, line)` during codegen (spine)

In `cg_emit_stmt` (the spine's statement dispatcher), at the *start* of each
statement, record `(call(pp.cur_text_pos), mod, line)` where `line` comes from
the statement node's source token via the existing
`tok_lines[tok] - diag_file_line_start(mod)` path. This is **chip-agnostic**
(uses the `cur_text_pos` primitive, exactly like `_bug_inl_record`), so both
backends inherit it — add a `_bug_line_record(pc, mod, line)` sibling to
`_bug_inl_record`, accumulating into a growable array.

**Gating (byte-stability):** like the inline-range recording, this fires only in
`--bug` mode — `cur_text_pos` returns the sentinel when not emitting a map, so a
normal build records nothing and the emitted code is byte-identical. `bpp` is
built without `--bug`, so the bootstrap stays **gen1 == gen2**.

**Granularity:** per-statement is the target (matches DWARF "is_stmt" rows).
Each statement node already has a source token; expressions inherit their
statement's line. Inline-spliced bodies already have their own range section —
keep them; the line map covers the non-inlined statement stream.

### 1b. `.bug` LINE TABLE section (format v5 → v6)

Add a new section after SOURCE MAP: `u32 count`, then `count` rows of
`(u32 pc_offset, u16 module, u32 line)`, **sorted by `pc_offset`** so the reader
binary-searches a PC to its row (the row with the greatest `pc ≤ query`). Bump
the `.bug` format version to v6; `bug_reader` parses the new section and exposes
`bug_line_for_pc(pc) → (module, line)`. Older v5 maps (no section) degrade to
the per-function line, exactly as today.

### 1c. Consume it (the payoff)

- **`bug_disasm`** — print the source line above each PC range whose line
  changed (`; foo.bpp:42` interleaved with the asm, objdump `-l` style).
- **Backtrace** — each frame shows `fn() at foo.bpp:42`, not just `fn()`.
- **Crash report** — "crashed at foo.bpp:42" instead of only the function.

Phase-1 win condition: compile a known program `--bug`, and `bug --dump` /
`--disasm` / a crash backtrace report the correct `file:line` for a hand-checked
statement.

## Phase 2 — source-line stepping (lldb `n` / step-over / `finish`)

All three compose from pieces that already exist (instruction single-step + the
Phase-1 line table + `bug_disasm` for call detection + temp breakpoints):

- **`n` — step to next source line.** Single-step instructions; after each, look
  up `bug_line_for_pc(pc)`; stop when the line differs from the start line (and
  the frame hasn't grown — don't stop inside a callee).
- **step-over.** When the next instruction is a call (detect via `bug_disasm`),
  set a temporary breakpoint at the return address (PC after the call) and
  continue, instead of single-stepping into the callee.
- **`finish`.** Set a temporary breakpoint at the caller's return address (from
  the backtrace / stackmap bug already computes) and continue.

Wire these as `bug_tui` commands (`n`, `so`/step-over, `fin`) and the matching
`bug_observe` control flow. Reuse the existing `--break` temp-breakpoint
machinery; no new gdb-remote primitives needed.

Phase-2 win condition: under `bug --tui`, `n` advances one source line, step-over
runs a call without descending, `finish` returns to the caller — verified on a
small multi-function program (and, where ptrace is available, in Docker on a
Linux build).

## Verification gates

- **Bootstrap gen1 == gen2 byte-stable** — the line recording is gated on
  `--bug`, dormant in the normal compiler build (same discipline as the existing
  inline-range recording). This is the load-bearing check, since Phase 1 touches
  the spine's `cg_emit_stmt`.
- Native a64 suite green + C-emit suite unaffected (the line map is native-`.bug`
  only; `--c` doesn't emit it).
- A `tests/test_bug_linemap.bpp`-style smoke: build `--bug`, assert a known
  statement's PC resolves to its source line.
- `.bug` v5 maps still load (the new section is additive; absence → per-function
  fallback).

## Doctrine compliance

- **Spine for the recording** — per-statement `(PC, line)` capture is
  chip-agnostic and lives in `bpp_codegen.bsm`'s `cg_emit_stmt`, calling the
  `cur_text_pos` primitive (mirrors `_bug_inl_record`). Both backends inherit it;
  no per-chip duplication. (`how_to_dev` decision tree: shape-shared → spine.)
- **Bespoke format** — extend `.bug` (v6), do not adopt DWARF (studio philosophy:
  learn the technique, keep the format). `nomad_manual.md` §4 "dog-food the
  cousins" is the lens: DWARF's line program and lldb's stepping are the
  reference, not the implementation.
- **Rule 41 N/A** — not a new target; this is an engine refinement shared by all
  targets (the `.bug` map is target-agnostic; the disasm consumer is per-arch but
  already exists for both).

## Estimated effort

- Phase 1a recording (spine, mirror `_bug_inl_record`): ~40 lines.
- Phase 1b `.bug` v6 section + `bug_reader` parse + `bug_line_for_pc`: ~60 lines.
- Phase 1c disasm/backtrace/crash display: ~50 lines.
- Phase 2 stepping (`n`/step-over/`finish` in `bug_tui` + `bug_observe`): ~120
  lines, the bulk being step-over's call-detection + temp-breakpoint dance.

Phase 1 is a focused session; Phase 2 a second. Both land before the ELF dynlink
engine so its debugging rides on source-line `bug`.

## Relationship to `elf_dynlink_plan.md`

Sequenced first. The dynlink engine is byte-level ELF work whose verification is
`readelf`/`ldd` + crash-debugging under `bug`; doing it with a `bug` that reports
`foo.bpp:42` and steps by source line is materially faster than one that reports
only functions and single instructions — exactly the kind of bug-hunt the recent
Mach-O/Linux work was (where source-line context would have helped). Land this,
then build the dynlink engine.
