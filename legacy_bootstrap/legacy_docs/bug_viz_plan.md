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
| `--watch` CLI flag | landed in `tools/the_bug/the_bug.bpp` | ✅ done |
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

**In `tools/the_bug/the_bug.bpp`**: Parse `--watch name[,name2,...]`. Store watched names in an
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

**Files touched**: `tools/the_bug/the_bug.bpp` (~20 lines), `bug_observe_macos.bsm` (~10 lines),
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

**Files**: new `bug_tui.bsm`, `tools/the_bug/the_bug.bpp` gains the REPL main loop.

---

## Phase 5 — Graphical visualizers (status real)

Originalmente desenhada como uma fase única de ~400 LOC no
`bug_gui.bsm`. Na execução, virou quatro sub-passos. Status real
em 2026-04-30:

### Step 1 — graph + vec2 renderers (TUI) — ✅ SHIPPED 2026-04-29

- `viz_render_graph(addr, n)`: lê N doubles consecutivos, pinta uma
  linha Unicode `▁▂▃▄▅▆▇█` com range numérico embaixo.
- `viz_render_vec2(addr)`: lê dois doubles, plota `*` num grid 21×9
  ASCII com crosshairs.
- Comando runtime `v <expr> graph <N>` / `v <expr> vec2` no TUI;
  visualizadores persistem entre breakpoints.
- Files: novo `src/bug_runviz.bsm` (~280 LOC), comando `v / lv / uv`
  em `src/bug_tui.bsm`.

### Step 2 — rgba renderer (TUI) — ✅ SHIPPED 2026-04-29

- `viz_render_rgba(addr, w, h)`: lê `w*h*4` bytes, downsampla pra
  preview 48×24 cells, pinta com ANSI 24-bit colour
  (`\x1B[48;2;R;G;Bm  \x1B[0m`).
- Wire format: `v <expr> rgba <w> <h>` no prompt.
- Custo O(w*h) por frame (downsample por block-average), aguenta
  até ~720p.

### Step 3 — Engine unificada GUI+TUI — ✅ SHIPPED 2026-04-30

**Three stages** delivered the engine convergence:

**Stage 1** — Async observer (`bug_run_async_start` /
`bug_run_poll` / `bug_run_resume` / `bug_run_async_stop`) in
`src/backend/os/macos/bug_observe_macos.bsm`. Non-blocking GDB
primitives (`gdb_socket_has_data` via `sys_select`,
`gdb_continue_send`, `gdb_try_recv_stop`) in `src/bug_gdb.bsm`.
Shared snapshot module `src/bug_shared.bsm` (status / watch
arrays / viz arrays) populated on every stop. New portable
builtin `sys_select` (CG_SYS_SELECT enum + a64/x64/macos/linux
wiring + validator + C emitter).

**Stage 2** — `gdb_read_mem_chunk(addr, len, dst)` in
`src/bug_gdb.bsm` (4 KB chunks, multi-digit hex size). Observer
populates RGBA snapshots via the chunked reader. Panel renderers
in `src/bug_runviz.bsm`: `viz_render_graph_panel` /
`viz_render_vec2_panel` / `viz_render_rgba_panel` /
`viz_render_all_panel`, all reading from `bug_shared_viz_*` and
drawing through `stbdraw` (CPU framebuffer). RGBA uses nearest-
neighbour scaling with aspect-fit, no GPU sprite needed.

**Stage 3** — GUI wire-up in `tools/the_bug/the_bug_lib.bsm`.
Replaced fork+execve in Run button with `bug_run_async_start`,
frame loop calls `bug_run_poll` instead of `sys_wait4`. Added
Continue / Stop buttons gated on `bug_shared_status`. Live panel
area appears below the button bar when STOPPED, splits content
into watch list (top) + viz panels (bottom) with a fixed 240 px
height (or half the content area). Static `.bug` map continues
below the live area. Status indicator under the buttons reads
"running... / stopped at <fn>() / exited rc=N / crashed sig=N".
`viz_format_panel` in `src/bug_viz.bsm` provides type-aware
strbuf formatting (decimal + IEEE float with 3-digit fraction +
hex pointer + "@0x<addr>" struct) so the watch panel mirrors
TUI's `viz_format` without touching the tracee.

Decisions ratified:
- Polling-based observer, not threading. `select(timeout=0)`
  before each read keeps the GUI at 60 fps.
- Shared state in plain globals (single producer, single
  consumer, same thread).
- TUI path untouched — `bug_run` in observer kept the original
  blocking flow; zero regression on `tests/test_bug_tui.sh`.
- macOS-only first cut. Linux observer GUI waits for ELF
  dynlink + GUI stabilisation.

Ship state: gen-stable, suite 121/0/11, C 103/0/29, bug TUI
PASS, the_bug builds clean.

### Step 4 — `@viz` source annotation — ✅ SHIPPED 2026-04-29

- Source `// @viz framebuffer rgba 320 180` → captura no lexer
  (`scan_comment` chama `_capture_viz_hint` em `src/bpp_lexer.bsm`).
- `.bug` carrega seção VIZ_HINTS após SOURCE MAP (formato: u16
  count + per-entry u16 length + raw bytes).
- `viz_replay_hints()` em `src/bug_runviz.bsm` walka `bug_vh_texts`
  e alimenta cada um pelo `_tui_handle_v` (mesmo parser do `v`
  runtime).
- User não digita nada — visualizers ficam ativos no primeiro BP
  hit.

---

## Phase 6 — Profiler + Runtime Symbolication

**Status: shipped.** Stages 6.1, 6.2, 6.3, 6.4.1, 6.4.2 all
landed (panic with stack trace, cooperative + SIGPROF
profiler, runtime PC resolution via embedded minisym). User-
facing API is documented in `docs/debug_with_bug.md` under
"Phase 6 user-facing API". The planning doc lives at
`legacy_bootstrap/legacy_docs/bug_phase6_plan.md` for
historical reference.

Stage 6.5 (`caller(n)` sugar wrapper) is the only piece
deferred — YAGNI until a real consumer in user code asks for
it. The compose `caller_name(caller_pc(n))` already works
today.

Resumo: minisym subset embutido no binário (Mach-O `__minisym`
section / ELF PT_NOTE `BPPMINI` record) permite o runtime
resolver PCs sem depender do `.bug` separado.

testProfiler é **multi-thread aware desde v1** — programa B++
típico já roda múltiplas threads (audio callback + bpp_job
workers). Single-thread profiler vê só ~20% do programa.
Solução: cooperative sampling em bpp_job/maestro (zero
async-signal-safety risk) + signal-based supplement pra audio
callback e main thread quando granularidade fina importa.

5 stages, ~700 LOC, 4-5 sessões. Foundation (6.1 + 6.2) ship
juntas; depois 6.3 (panic) → 6.4 cooperative → 6.4 signal →
6.5 caller sugar.

Cross-version safety: build_id v4 (já shipado em Phase 5)
permite profiler refusar `.bug` stale.

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

## Priority and sequencing (atualizado 2026-04-30)

```
  ✅ Dev Loop 2 — bug --break, --dump-str, crash locals
  ✅ bug Phase 1 — watch list at breakpoints
  ✅ bug Phase 2 — type-aware display: struct/array/string
  ✅ bug Phase 3 — expression evaluator
  ✅ bug Phase 4 — interactive TUI: watch window REPL
  ✅ bug Phase 5 step 1 — graph + vec2 renderers (TUI)
  ✅ bug Phase 5 step 2 — rgba renderer (TUI, ANSI 24-bit)
  ✅ bug Phase 5 step 4 — @viz source annotation
  ✅ Faxinada the_bug — main() refactor + .bug sempre-ligado +
     build_id (16 bytes Mach-O LC_UUID / ELF PT_NOTE GNU)
  ✅ bug Phase 5 step 3 — Engine unificada GUI+TUI
  ──────────────────────────────────────────────────────────────
     ESTAMOS AQUI. Phase 5 fully shipped.
  ──────────────────────────────────────────────────────────────
     Wolf3D Phase 1 — CPU raycaster
     Wolf3D Phase 2 — hybrid CPU + GPU
     Multicore Sprints 1-2 (start_idx / stride / reduction)
  ──────────────────────────────────────────────────────────────
     bug Phase 6 — Profiler + Caminho C (subset embutido)
     Dev Loop 4 — hot reload watch mode
     RPG / RTS demos
```

Dev Loop 3 (profiler) virou Phase 6 desta linha — depende do
caminho hybrid (.bug separado + minisym embutido) que casa com a
arquitetura já desenhada.

Total entregue: **Phases 1-4 + Phase 5 (steps 1, 2, 3, 4)**.
Próximas opções: Wolf3D, multicore Sprints, bug Phase 6.
