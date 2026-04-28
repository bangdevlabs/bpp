# bug — Visualization Architecture Plan

Inspired by Ryan Fleury's BSC 2025 talk "Cracking the Code: Realtime Debugger
Visualization Architecture" (RAD Debugger). Core thesis: visualization is the
part of the debugger that actually helps you understand the program. Process
control without visualization is archaeology.

---

## Audit: what already exists

Before planning new work, a grep audit of the source revealed that much of
Phase 1 infrastructure is already in place:

| Capability | Where | Status |
|-----------|-------|--------|
| Read memory from live process | `bug_gdb.bsm:457` `gdb_read_mem(addr, n)` | ✅ done |
| Read frame pointer / stack pointer | `bug_gdb.bsm:486-493` | ✅ done |
| Stack map with fp-relative offsets | `bug_sk_voffs` in `.bug` format | ✅ done |
| Print all locals of current frame | `bug_observe_macos.bsm:445` `_print_crash_locals` | ✅ done (crash only) |
| ASLR slide + text base resolution | `bug_observe_macos.bsm:547-551` | ✅ done |
| Lookup function from PC | `lookup_pc(pc, text_base)` | ✅ done |
| SIGTRAP handler (breakpoint hit) | `bug_observe_macos.bsm:622` | ✅ done |
| Global name + type + storage class | `bug_gl_name/type/class` in `.bug` | ✅ done |
| **Global variable address** (offset in __DATA) | not in `.bug` format | ❌ missing |
| `--watch` CLI flag | not in `bug.bpp` | ❌ missing |
| Type-aware display (struct tree, array, string) | none | ❌ missing |
| Expression evaluator | none | ❌ missing |
| Interactive TUI | none | ❌ missing |
| Graphical visualizers | none | ❌ missing |

**Key finding**: `_print_crash_locals` already reads fp-relative locals via
`gdb_read_mem`. Phase 1 is mostly connecting existing pieces. The only genuine
new infrastructure is: (a) global addresses in the `.bug` format, (b) the
`--watch` CLI, and (c) triggering the existing local dump at breakpoint time.

---

## Architecture model (RAD Debugger applied to B++)

```
 Process ──► Evaluation ──► Visualization ──► Display
              (read mem)      (type → kind)    (text / TUI / GUI)
```

**Evaluation** — given a name or expression, compute its address (stack-relative
for locals, data-section-relative for globals), read bytes via GDB `m` command,
return a typed value. B++ has a richer semantic layer than DWARF: arr_new header
layout, buf_word/buf_byte stride, bit-sliced fields, storage class — all in the
`.bug` format.

**Visualization** — map type → display kind. Not all words are integers. The
display kind decides how to format the value: scalar hex/decimal, IEEE float,
null-terminated string preview, struct field tree, array element list, raw hex
dump.

**Display** — render the result. Terminal text for all phases up to Phase 4
(which adds a REPL). Phase 5 adds a GPU window via stbgame/stbrender inside
Bang 9.

---

## B++-specific display kinds

| Type | Display kind | What to show |
|------|-------------|--------------|
| TY_WORD (untyped) | scalar | decimal + `0x` hex |
| TY_WORD (pointer) | pointer | hex address, peek target if non-null |
| TY_FLOAT | float | IEEE 754 decimal |
| TY_FLOAT_H | float32 | IEEE 754 decimal (32-bit) |
| struct | struct-tree | expandable: `name.field = value` per field |
| `arr_new()` array | array-list | header (len/cap) + first N elements |
| `buf_word(n)` | word-table | flat word list indexed 0..n-1 |
| `buf_byte(n)` | hex-dump | hex + ASCII sidebar |
| null-terminated string | string | text preview (up to 256 chars) |
| bit-sliced field (`: byte`, `: half`) | subword | decimal + masked hex |

This table lives in `bug_viz.bsm` and is fed by the type info already in `.bug`.

---

## Phase 1 — Watch list at breakpoints (~50 lines new code)

**What changes:**

**In `bug_observe_macos.bsm`**: The SIGTRAP handler already calls `lookup_pc`
and prints the function name. Add one call to `_print_crash_locals(fi, text_base)`
right after printing the function entry. Currently `_print_crash_locals` only runs
on fatal signals; it works identically on breakpoints.

**In `bug.bpp`**: Parse `--watch name[,name2,...]`. Store watched names in an
array. Pass it into `bug_run`. In the SIGTRAP handler, if the watch list is
non-empty, filter `_print_crash_locals` to only the named variables instead of
all locals.

**In `bpp_bug.bsm`** (compiler side): Emit per-global data section offsets.
Globals already have a fixed address relative to `__DATA` base — the compiler
assigns these during codegen. Extend the globals section of the `.bug` file with
a `u32 data_offset` per entry. In `bug_observe_macos.bsm`, compute
`data_base = text_base + text_size + ...` to resolve global addresses.

**Deliverable**: `bug --watch player,score ./platformer` prints current local
values every time execution stops. Globals (like `score`) readable by name.

**Files touched**: `bug.bpp` (~20 lines), `bug_observe_macos.bsm` (~10 lines),
`bpp_bug.bsm` (~15 lines for data offset emit), `bug_reader.bsm` (~5 lines).

**No new module needed.** The plan initially proposed `bug_eval.bsm` but the
existing `_print_crash_locals` infrastructure covers locals completely. `bug_eval.bsm`
enters when expression evaluation is needed (Phase 3).

---

## Phase 2 — Type-aware display (~150 lines)

**What changes:**

New module `bug_viz.bsm` implementing the display-kind table above. Called
from `_print_crash_locals` (renamed `_print_locals_watched`) instead of the
current raw `out_int(val)`.

Key additions:
- `viz_format(val, ty, depth)` — dispatch on type byte, format result as string.
- Struct expansion: read each field at `base_addr + field_offset`, recurse.
- `arr_new()` expansion: read header at `ptr - 16` (len at offset 0, cap at 8),
  iterate elements up to `min(len, 16)`.
- String detection: if type is TY_PTR and peeked first 4 bytes are printable
  ASCII, show as string preview alongside hex.
- Depth limit: max 2 levels of struct/array expansion to avoid runaway output.

**Deliverable**: `--watch tilemap` shows the full tilemap struct with field names
and values. `--watch cg_fn_name` shows the dynamic array with first 16 entries.
Pointer args labeled as `0x... → "string content"` when printable.

**Files**: new `bug_viz.bsm`, minor changes to `bug_observe_macos.bsm` and
`bug_observe_linux.bsm`.

---

## Phase 3 — Expression evaluator (~300 lines, new `bug_eval.bsm`)

**What changes:**

`bug_eval.bsm` — mini expression compiler:
- Lexer: tokenizes `player.vx`, `enc_pos - enc_buf`, `cg_fn_name[3]`.
- Parser: handles `name`, `expr.field`, `expr[i]`, `*expr`, binary `+/-/*/`.
- Type checker: resolves field names and array element types from `.bug` structs.
- Evaluator: computes address (not value) for lvalues, then reads via `gdb_read_mem`.
- No function calls. Constants from `.bug` constant table fold to their values.

`--watch "expr"` accepts any expression the evaluator handles. Quoted strings
passed as single watch items.

**Deliverable**: `bug --watch "enc_pos - enc_buf"` shows bytes emitted.
`bug --watch "player.vx"` drills into a struct field without expanding all fields.

---

## Phase 4 — Interactive TUI (~250 lines, new `bug_tui.bsm`)

**What changes:**

Instead of printing and resuming on each STOP, drop into a mini REPL:

```
-> update_player(dt=16, tilemap=0x100014a00, player=0x100014c00)
   score = 300
   player.vx = 60
   player.vy = -120
(bug) _
```

Commands:
- `w expr` — add expression to watch list
- `p expr` — one-shot print (not added to watch)
- `s` — single-step
- `c` — continue to next breakpoint
- `b funcname` — add breakpoint
- `q` — quit

Display: watch list re-rendered on each STOP — a terminal watch window.
Terminal: raw ANSI escape sequences, cursor positioning.

**Deliverable**: interactive debugging session. Equivalent to a basic IDE
debugger panel running in the terminal.

**Files**: new `bug_tui.bsm`, `bug.bpp` gains the REPL main loop.

---

## Phase 5 — Graphical visualizers in Bang 9 (~400 lines, new `bug_gui.bsm`)

**What changes:**

`bug --gui` opens a stbgame window alongside the watched process. Each watch
item renders according to its graphical kind:

- `@viz:rgba(w,h)` on a `buf_byte(w*h*4)` — live pixel buffer as image.
- `@viz:vec2` on a struct with `x,y` — dot on a 2D grid.
- `@viz:graph` on a float array — live bar/line chart.

Visualization hints come from source comments: `// @viz:rgba(320,180)`. The
compiler copies these into the `.bug` file as optional hint records. Zero-cost
when `bug` isn't in `--gui` mode.

Trigger: Wolf3D has a framebuffer `buf_byte(320*180*4)` — you can watch the
raycast result update in real time.

**Files**: new `bug_gui.bsm`, `.bug` format extended with viz hint section,
`bpp_bug.bsm` extended to capture `@viz:` comments.

---

## .bug format extensions

| Phase | Extension | Where |
|-------|-----------|-------|
| 1 | Per-global `u32 data_section_offset` | globals section |
| 2 | Array header layout tag (arr_new vs buf_word vs buf_byte) | type byte, new tag |
| 2 | Struct field slice widths (`: byte` / `: half` info) | struct section |
| 3 | Constant values (name → word) for expression folding | new constants section |
| 5 | Viz hint records (`@viz:rgba`, `@viz:vec2`, `@viz:graph`) | new hints section |

All extensions are backward-compatible: new section tags, old readers skip
unknown sections.

---

## Priority and sequencing

The user's instinct is correct: Wolf3D and RTS **without** a visualization-capable
debugger is an archaeology expedition. Complex scene graphs, allocation patterns,
raycast state — you need to see it, not infer it.

Revised priority (phases 1-4 before Wolf3D, Phase 5 during Wolf3D):

```
  ✅ Dev Loop 2 — bug --break, --dump-str, crash locals
     bug Phase 1 — watch list at breakpoints (~1 session)
     bug Phase 2 — type-aware display: struct/array/string (~1-2 sessions)
     bug Phase 3 — expression evaluator (~2 sessions)
     bug Phase 4 — interactive TUI: watch window REPL (~2 sessions)
  ──────────────────────────────────────────────────────────────
     Wolf3D Phase 1  — CPU raycaster (debugger will earn its keep here)
     bug Phase 5 — graphical visualizers (@viz: in Bang 9)
     Wolf3D Phase 2  — hybrid CPU + GPU
  ──────────────────────────────────────────────────────────────
     Dev Loop 3 — profiler (reuses bug Phase 1 infrastructure)
     Dev Loop 4 — hot reload watch mode
     RPG / RTS demos
```

Dev Loop 3 (profiler) moves after Phase 1 since the sampling profiler reuses the
stack-walk and variable-read infrastructure being built.

Total estimated effort for Phases 1-4: ~4-6 sessions.
Phase 5: 2-3 sessions, ideally during Wolf3D when there's a real framebuffer to visualize.
