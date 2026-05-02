# Debugging B++ with `bug`

`bug` is the native B++ debugger. It reads the `.bug` debug map
the compiler writes alongside every binary and drives the target
through the platform's remote stub (`debugserver` on macOS,
`gdbserver` on Linux). There is no GDB, no LLDB, no DWARF — it is
a B++ program reading a B++-defined format and speaking the GDB
remote serial protocol directly.

The debugger ships in two binaries:

- **`bug`** — the legacy CLI in `src/bug.bpp`. Pure terminal
  experience. Best for "I'm in the shell, run this under bug".
- **`the_bug`** in `tools/the_bug/` — same engine wrapped with a
  GUI. Open `.bug` files, browse the debug map visually, run the
  target under live observation with watch + viz panels updating
  on every breakpoint. Best for "I want to see what's happening
  inside the program as it runs."

Both link the same observer module (`bug_observe_<os>.bsm`) and
debug-map reader, so any feature documented here works in both.
The GUI panels are layered on top.

This document covers every feature the debugger exposes today.

---

## Three modes

The CLI dispatches on the first non-flag argument:

```
bug hello.bug                       # dump mode (legacy `bug --dump`)
bug --tui ./hello                   # TUI live observation + REPL
the_bug                             # GUI: file picker, browse, run
the_bug hello.bug                   # GUI: open with .bug pre-loaded
```

| Mode  | Binary    | What you get |
|-------|-----------|--------------|
| Dump  | `bug` / `the_bug --dump file.bug` | Static text dump of the `.bug` map |
| TUI   | `bug --tui ./prog` | Live trace + REPL in the terminal |
| GUI   | `the_bug [file.bug]` | Window with map browser + live panels |

### Building a target with debug info

Every native compile writes a `.bug` file next to the binary,
automatically. There is no `-g` switch — it is always on, like
Microsoft's `.pdb` files or Apple's `.dSYM` bundles. Keep the
`.bug` alongside the binary; observe mode reads it by appending
`.bug` to the target path.

```
bpp hello.bpp -o hello              # produces ./hello and ./hello.bug
bug --tui ./hello                   # finds ./hello.bug automatically
```

**Exception:** `bpp --c source.bpp` (C emitter) does not emit a
`.bug` because the binary's instruction offsets do not exist yet —
they are decided by gcc/clang downstream. Native (`bpp source.bpp`)
and cross-compile (`bpp --linux64 source.bpp`) both emit it.

**Build ID identity check.** Every build embeds a 16-byte
build_id into the binary (Mach-O `LC_UUID` on macOS, ELF
`PT_NOTE NT_GNU_BUILD_ID` on Linux) and the same value into the
`.bug` file's header. When `bug` loads a `.bug`, it verifies the
binary's build_id matches before doing anything. Mismatch fails
loud:

```
bug: stale .bug for ./hello: expected build_id <hex32>,
     binary has <hex32>. Recompile to regenerate.
```

This catches the staleness class of bug — `.bug` describing a
binary that was rebuilt later — at load time, not at "wrong
analysis 30 minutes in".

**Reproducible builds.** For bootstrap byte-stability and CI, set
`BPP_BUILD_ID=00000000000000000000000000000000` to force the
build_id to zero. The test suite (`tests/run_all.sh`) does this
automatically.

**`--bug` flag.** Historically `bpp --bug ...` was required to
emit the `.bug` file. The flag is now deprecated — any compile
emits `.bug` automatically. Passing `--bug` still works but
prints a deprecation warning to stderr. Will be removed in a
future release.

---

## Dump mode — inspecting the debug map

`bug file.bug` (or `the_bug --dump file.bug`) parses a `.bug`
file and prints every section the compiler emitted. Useful for
confirming functions, structs, globals, externs, and source
positions landed where you expected before touching the program
at all.

Output sections, in order:

- **Functions** — name, parameter list with types, return type,
  local variables with types.
- **Structs** — struct layout with field offsets, sizes, types.
- **Globals** — name, type, storage class (`[auto]`, `[global]`,
  `[extrn]`), and `__DATA` offset (used by the watch list to
  resolve global names at runtime).
- **Externs** — name, library, argument list.
- **Source Map** — per-function `(module index, line number)`.
- **Address Map** — function name → byte offset in the text
  section. Two-hop input that lets observe mode turn function
  names into breakpoint addresses.
- **Stack Map** — per-function frame size + per-local variable
  offsets, location kinds (register vs stack slot), and details.
- **Viz Hints** — any `// @viz` annotations the lexer captured
  during compile (replayed automatically when the target runs).

When a local or field shows `ty:<number>`, the dump formatter
hasn't been taught that slice constant yet — adding a one-liner
to `out_type` is a routine patch.

The GUI dump mode (`the_bug` opens `.bug` in the file picker)
shows the same sections, scrollable with mouse wheel / Page
Up/Down, with section headers in accent colour.

---

## Observe mode — watching a program run

`bug --tui ./program [args...]` launches the target under
`debugserver` (macOS) or `gdbserver` (Linux) and connects via the
GDB remote serial protocol. Every function entry the program
executes is traced by default, indented to show the call tree:

```
-> main()
  -> setup_world(world=0x100014a00)
    -> spawn_enemy()
    -> spawn_enemy()
  -> run_frame(dt=16)
    -> update_enemy()
    -> render()
  -> teardown()
```

Target stdout flows through unchanged — target `printf`-equivalents
print exactly where they would if you ran the program directly.

### Arguments to the target

Every argv token after the target path is forwarded to the
program. macOS `debugserver` parses its own flags greedily, so
observe mode inserts a literal `--` before forwarded args to mark
the boundary:

```
bug --tui ./game --level 3
  → debugserver gdb-remote 127.0.0.1:1234 ./game -- --level 3
```

Without the `--`, `debugserver` would swallow `--level` as one of
its own options.

### Target stdin / stdout

Both are passed through. If your program reads from stdin, run it
under `bug` the same way you'd run it directly.

---

## Selective breakpoints — `--break <fn>`

By default observe mode traces every function in the program.
That is excellent for "what is this binary doing" surveys and
terrible for "I need to know exactly when `foo` runs."
`--break <fn>` narrows the trace.

```
bug --tui --break update_enemy ./game
bug --tui --break func_a --break func_b ./target
```

Each `--break` adds one function name to a 32-slot array. Only
the named functions are registered with the remote stub; every
other function entry is invisible.

`name:line` breakpoints are **not yet implemented** —
scaffolding exists (per-function source position is in `.bug`)
but resolving `source.bpp:42 → function index → byte offset`
hits an off-by-N in the source map. Passing a colon-bearing spec
returns a clean error today instead of resolving wrong.

### Combining selective with full trace — `--break-all`

Selective breakpoints *plus* the legacy trace for everything
else:

```
bug --tui --break update_enemy --break-all ./game
```

Without `--break-all`, any `--break` flag switches observe mode
into "only the named functions" behaviour.

---

## Pointer-argument introspection — `--dump-str`

Every breakpoint hit prints the stopped function name and the
general-purpose registers. `--dump-str` adds one extra piece: for
each of `x0` and `x1` (ARM64) / `rdi` and `rsi` (x86_64), `bug`
reads up to 32 bytes from the register's pointer value and prints
the result as a C string if the bytes are printable ASCII.

```
bug --tui --break my_fn --dump-str ./program
```

Output shape at each hit:

```
-> my_fn(x0=0x100004000, x1=0x100004020)
   x0="hello world" x1="look up this literal"
```

This is the single most valuable flag for tracing string
arguments. Most "wrong behaviour" bugs boil down to "the function
received a different pointer than I expected." `--dump-str` shows
the pointer's target at the exact moment the callee sees it.

When a pointer doesn't point at a printable string, `--dump-str`
shows garbled bytes or NULs. Useful signal: if `x1="""".g.."`
where you expected `x1="break"`, you have a literal-dedup or
string-relocation bug, not a logic bug.

---

## Watch list — `--watch <names>`

Filter the trace and surface specific values at every stop:

```
bug --tui --watch player,score,enc_pos ./game
bug --tui --watch "player.vx,score" ./game
```

Each comma-separated entry is either a plain name or an
expression. Plain names match locals (in the stopped function's
frame) or globals (read from `__DATA` via the offset captured at
compile time). Expressions go through the eval module and can
include `.field`, `[index]`, `*deref`, `&addr`, and arithmetic.

Output at every stop, after the call-tree line:

```
-> update_enemy(e=0x100014c00, dt=16)
  locals:
    e = 0x100014c00 → struct Enemy { hp = 100, vx = 60, ... }
  globals:
    score = 300 (0x12c)
  expressions:
    player.vx = 60 (0x3c)
    enc_pos - enc_buf = 4096 (0x1000)
```

The watch list combines with `--break` so you only see updates
at the breakpoints you care about.

---

## Type-aware display

Watch values render differently based on the type the compiler
recorded in `.bug`:

- **Float** types print the IEEE-754 decimal alongside the bit
  pattern: `42.500 (ieee 0x4045400000000000)`.
- **Pointers** print the hex address and, when the target bytes
  look like printable ASCII, append a string preview:
  `0x100004000 → "hello world"`.
- **Structs** with a `: TypeName` annotation expand fields:
  `0x100014c00`, then per-field lines underneath.
- **Sliced fields** (`: byte` / `: half` / `: quarter`) show the
  decimal alongside the masked hex.
- **Untyped** falls back to signed decimal + companion hex.

Same dispatch table feeds the TUI text path and the GUI panel
path (the panel variant skips the string-preview probe to avoid
extra GDB round-trips at frame rate).

---

## Expression evaluator

`--watch` accepts B++-style expressions, evaluated at every stop
against the current frame:

```
bug --tui --watch "player.vx" ./game
bug --tui --watch "tokens[5]" ./compiler
bug --tui --watch "*head" ./linkedlist
bug --tui --watch "enc_pos - enc_buf" ./bpp
```

Supported syntax: identifiers (locals, globals, struct field
access via `.`, array index `[]`, dereference `*`, address-of
`&`), and binary `+ - * /`. No function calls. Constants from the
`.bug` constant table fold to their values.

Useful for derived metrics that don't have a name in the source —
"bytes emitted into encoder so far", "particle count alive", etc.

---

## Interactive TUI REPL

When `--tui` is on, every breakpoint hit drops you into a small
REPL instead of auto-continuing:

```
-> update_enemy(e=0x100014c00, dt=16)
  locals:
    e.hp = 95
    e.vx = 60
(bug)
```

Commands:

| Command | Effect |
|---------|--------|
| `c`           | Continue to next breakpoint |
| `s`           | Single-step one instruction |
| `q`           | Quit (kill tracee) |
| `w expr`      | Add expression to watch list |
| `p expr`      | One-shot print (don't add to watches) |
| `b funcname`  | Add a function-entry breakpoint at runtime |
| `lw`          | List active watches |
| `uw <idx>`    | Remove watch at index |
| `v expr graph <N>` | Add graph visualizer (N consecutive doubles) |
| `v expr vec2`      | Add 2D vector visualizer |
| `v expr rgba <w> <h>` | Add RGBA bitmap visualizer |
| `lv`          | List active visualizers |
| `uv <idx>`    | Remove visualizer |

Visualizers persist across breakpoints — once added, they
re-render on every stop with the latest tracee state.

---

## Runtime visualizers

`v <expr> <kind> [params]` adds a multi-line ASCII picture to the
watch panel. Each kind reads `peek` bytes from the address `expr`
resolves to and renders accordingly:

- **graph** `v buf graph 32` — reads 32 consecutive 8-byte
  doubles, renders as a single-row Unicode bar chart
  (`▁▂▃▄▅▆▇█`) auto-scaled to the data's min/max, with the
  numeric range printed below.
- **vec2** `v pos vec2` — reads two doubles, plots a `*` on a
  21×9 ASCII grid with crosshairs at the origin.
- **rgba** `v framebuffer rgba 320 180` — reads `w*h*4` bytes,
  downsamples to 48×24 cells, paints with ANSI 24-bit colour
  (true-colour terminal required: iTerm2, kitty, alacritty,
  modern xterm, modern Terminal.app).

The `v` command stays in the REPL session. To bake them into the
source so they auto-replay on every run:

```c
// @viz framebuffer rgba 320 180
auto framebuffer;
framebuffer = malloc(320 * 180 * 4);
```

The lexer captures `@viz` annotations during compile; the
debugger replays them at startup as if the user had typed each
one at the REPL. Same parser handles both paths, so the grammar
stays consistent.

---

## GUI live debugger (the_bug)

The GUI wraps the same engine with a window. Layout:

```
┌──────────────────────────────────────────────┐
│ The Bug — Debug Map Viewer                   │
│  [Load .bug] [Run target] [Continue] [Stop]  │
│  status: stopped at update_enemy()           │
├──────────────────────────────────────────────┤
│ ┌─[live panels — visible when STOPPED]────┐ │
│ │ watches                                  │ │
│ │   e.hp = 95 (0x5f)                       │ │
│ │   e.vx = 60 (0x3c)                       │ │
│ │ visualizers                              │ │
│ │   [framebuffer panel — RGBA preview]     │ │
│ └──────────────────────────────────────────┘ │
│                                              │
│ static .bug map (scrollable)                 │
│   Functions (343)                            │
│     main(args, env) -> word                  │
│     ...                                      │
└──────────────────────────────────────────────┘
```

Workflow:

1. Click **Load .bug** → file picker → static map fills the
   content area.
2. Click **Run target** → spawns the binary derived from the
   `.bug` path (drops the `.bug` suffix) under debugserver, sets
   breakpoints on every function entry, sends initial continue.
3. Each frame the GUI polls the observer (non-blocking via
   `select(timeout=0)` on the GDB socket). When a SIGTRAP
   arrives, status flips to STOPPED, the live panel area
   appears with watch + viz snapshots populated by the observer.
4. Click **Continue** → step past the BP and resume.
5. Click **Stop** → kill tracee + tear observer down.
6. ESC quits the panel.

The live panel uses the same `viz_format_panel` that the watch
list calls — type-aware decimal + hex + IEEE float + struct
address rendering, no tracee reads at draw time (snapshots are
captured once per stop).

The GUI is macOS-only at the moment. Linux observer
(`bug_observe_linux.bsm`) speaks the GDB protocol but the GUI
wrapper uses the macOS Cocoa platform layer — Linux X11 GUI
debugger waits for the platform layer to grow more.

---

## Case study — the page_count bug (2026-04-16)

**Symptom.** Adding a 16th entry to the dispatch-seed list in
`init_dispatch()` broke bootstrap with a cryptic compile-time
error: `global 'break' not found in data section`. The 15th
entry was fine; any 16th name whose string was four bytes or
longer flipped the compiler into a wrong-output state.

**First instinct.** Token table overflow. `tok_lines` and
`tok_src_off` had been 1 MB since the self-host; they were
saturating. Bumped both to 8 MB, bumped `outbuf` to 8 MB for
good measure. No change. "aumentar os canais não foi o
suficiente, é algo na contagem que desloca" — the user's
insight. The trigger was tied to *count*, not volume.

**The investigation loop.** The failing message came from
`diag_fatal` at the `global 'break' not found` site. The message
string is a literal. The lexer had just scanned the word `break`
and was looking it up. The literal address should point at the
string `"break"`.

`--dump-str` told the truth in one hit:

```
bug --break buf_eq --dump-str ./bpp_broken
  BP hit: buf_eq  x0=0x1004000a0  x1=0x1004020b0
    x0="keywd: b"
    x1=""""g."
```

`x1` was supposed to be `"break"`. It wasn't. The bytes at that
address were *wrong bytes*. The pointer resolved via ADRP+ADD
was pointing to unrelated memory.

**The real bug.** ARM64 addresses literals via `ADRP x0, page +
ADD x0, offset`. The ADRP+ADD pair is set at link time by
reading a chained fixup from the Mach-O. The chained fixup
header has a `page_count` field. In `a64_macho.bsm` it was
hardcoded:

```
mo_w8(1); mo_w8(0);  // page_count = 1 as uint16
```

As long as `__DATA` fit in one 16KB page, the hardcoded `1`
worked. The 16th dispatch seed (`sys_lseek`, the four-byte
literal on top of 15 prior four-byte-or-longer literals) pushed
`__DATA` past 16 KB. dyld only rebased page 0. GOT pointers on
page 1 kept their on-disk values — valid at compile time,
garbage after ASLR slid the binary.

**Confirming signal.** Running `./bpp` with
`DYLD_PRINT_WARNINGS=1` surfaced
`dyld[...]: fixup chain entry off end of page` on every build —
for weeks. As long as `__DATA` stayed under 16 KB, the warning
was cosmetic because there was nothing past page 0 to corrupt.
The 16th seed tipped it.

**The fix.** Replace the hardcoded byte with a dynamic
computation:

```bpp
auto _pc, _hdr_size, _pg;
_pc = (mo_data_size + MO_PAGE_SIZE - 1) / MO_PAGE_SIZE;
if (_pc < 1) { _pc = 1; }
// ... emit page_count and per-page start offsets ...
```

**Lesson.** When a compile-time error's message mentions a
string literal that the compiler should have emitted, the first
question is not "where is the lookup failing" but "does the
runtime see the bytes the file says are there." `--dump-str`
answers that question in under a second.

This is also where most of the debugger's basics earned their
keep — the `--` separator, pass-through stdout, the `O`-packet
decoder, even `--dump-str` itself all shipped during this single
investigation.

---

## Phase history (what shipped when)

| Phase | What it added | Status |
|-------|---------------|--------|
| Dev Loop 1 | `bug` CLI, dump + observe modes, `--break`, `--dump-str` | shipped |
| Phase 1    | `--watch`, `_print_crash_locals` for stack frames | shipped |
| Phase 2    | Type-aware display (struct / array / pointer / float) | shipped |
| Phase 3    | Expression evaluator (`--watch "expr"`) | shipped |
| Phase 4    | Interactive TUI REPL | shipped |
| Phase 5.1  | `viz_render_graph`, `viz_render_vec2` (TUI ANSI) | shipped |
| Phase 5.2  | `viz_render_rgba` (TUI 24-bit colour) | shipped |
| Phase 5.4  | `@viz` source annotations | shipped |
| Phase 5.3  | GUI live debugger (`the_bug` Run / Continue / Stop, watch + viz panels) | shipped |
| Phase 6    | Profiler + runtime symbolication + `panic()` + `caller(n)` | planned |

Phase 6 detail: see `docs/bug_phase6_plan.md`.

---

## Appendix — what `bug` cannot do yet

- **No `name:line` breakpoints.** Function-entry only. Tracked
  as `docs/todo.md` bug #3b.
- **No watchpoints.** Memory writes are not observed.
- **No attach-to-pid.** `bug` always fork-execs its target.
- **Linux GUI not wired.** `bug_observe_linux.bsm` speaks the
  GDB protocol but the GUI wrapper depends on the macOS Cocoa
  platform layer. Waits for Linux X11 GUI maturity.
- **No `panic()` builtin.** Programs can't deliberately abort
  with a stack trace yet — Phase 6.3 lands that.
- **No sampling profiler.** No `sys_profile_*` API yet —
  Phase 6.4 lands that, with cooperative sampling at
  `bpp_job` / `maestro` boundaries plus a signal-based
  supplement for audio / main thread.
- **No runtime caller introspection.** `caller(n)` lives in
  Phase 6.5 once minisym + `_runtime_resolve_pc` ship in
  Phase 6.1+6.2.

For the broader roadmap (Wolf3D, multicore Sprints, etc.), see
`docs/bug_viz_plan.md` and `docs/multicore_state_report.md`.
