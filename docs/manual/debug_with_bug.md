# Debugging B++ with `bug`

`bug` is the native B++ debugger. It reads the `.bug` debug map
the compiler writes alongside every binary and drives the target
through the platform's remote stub (`debugserver` on macOS,
`gdbserver` on Linux). There is no GDB, no LLDB, no DWARF — it is
a B++ program reading a B++-defined format and speaking the GDB
remote serial protocol directly.

The debugger ships as **two binaries over one shared engine**:

- **`bug`** — the headless CLI, built from `src/bug.bpp`. It imports
  no GUI modules (no Cocoa), so it cross-compiles to Linux x86_64 and
  runs in Docker. Reach for it to dump a map, observe a program under
  the remote stub, or read machine code (`--bytes` / `--disasm`).
- **`the_bug`** — the windowed inspector, built from
  `tools/the_bug/the_bug.bpp` (macOS) — the same panels Bang 9 embeds.

Both share the observer (`bug_observe_<os>.bsm`), the debug-map reader,
and the dump engine, so a feature behaves the same regardless of which
front-end launched it. (A 0.23.x-era refactor briefly fused the two
into one `bug`; the split was reinstated on 2026-05-28 precisely so the
CLI could build for Linux without dragging in the macOS window layer.)

This document covers every feature the debugger exposes today.

---

## Modes

`bug` (the CLI) dispatches on its flags and **never opens a window** —
the windowed inspector is the separate `the_bug` binary.

```
bug --dump hello.bug                # dump the .bug map to stdout (text)
bug --tui ./hello                   # live observation + REPL in the terminal
bug --bytes ./hello fn              # hexdump one function's machine code
bug --disasm ./hello fn             # disassemble one function (objdump-in-bug)

the_bug                             # GUI: file picker, browse a .bug
the_bug hello.bug                   # GUI: open with .bug pre-loaded
```

| Tool / mode    | Invocation                  | What you get |
|----------------|-----------------------------|--------------|
| `bug` dump     | `bug --dump file.bug`       | Static text dump of the `.bug` map (~2 k lines for the compiler) |
| `bug` tui      | `bug --tui ./prog`          | Live trace + REPL in the terminal (drives the remote stub) |
| `bug` bytes    | `bug --bytes ./prog fn`     | Hexdump of one function's machine code — static, no ptrace |
| `bug` disasm   | `bug --disasm ./prog fn`    | Disassembly of one function — static, no ptrace |
| `the_bug` GUI  | `the_bug` / `the_bug f.bug` | Window with map browser + live panels |

**`bug` is headless — safe for CI / Docker.** With no arguments it
prints usage; it never blocks on an event loop. The two static
sub-commands (`--bytes`, `--disasm`) need only the binary + its `.bug`
map and no ptrace, so they work cross-target — a macOS `bug` can read a
Linux x86_64 ELF, and vice-versa. For the windowed inspector, run
`the_bug` on macOS.

### Building a target with debug info

`.bug` emission is opt-in via the `--bug` flag — same model as
`gcc -g`. The default compile produces only the binary; pass
`--bug` when you want the debug map written alongside.

```
bpp hello.bpp -o hello              # produces ./hello only
bpp --bug hello.bpp -o hello        # produces ./hello + ./hello.bug
bug --tui ./hello                   # finds ./hello.bug automatically
```

For tooling that wants the `.bug` file off to the side (so it does
not pollute the user's project tree), pass `--bug=<path>` and the
compiler writes the map at the given location instead of next to
the binary. Bang 9 uses this to keep the file under `/tmp`:

```
bpp --bug=/tmp/bang9_pathfind.bug pathfind.bpp -o build/pathfind
```

**Exception:** `bpp --c source.bpp` (C emitter) does not emit a
`.bug` because the binary's instruction offsets do not exist yet —
they are decided by gcc/clang downstream. Native (`bpp source.bpp`)
and cross-compile (`bpp --linux64 source.bpp`) both emit it when
`--bug` is passed.

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

**`--bug` flag.** `.bug` emission is opt-in. Pass `--bug` to write
`<output>.bug`, or `--bug=<path>` to write at a custom location.
Without the flag, only the binary is produced. Bang 9 always passes
`--bug=/tmp/bang9_<basename>.bug` so the debugger panel works while
the project tree stays clean; the file is unlinked on Bang 9
shutdown.

---

## Dump mode — inspecting the debug map

`bug --dump file.bug` parses a `.bug` file and prints every
section the compiler emitted to stdout. Useful for confirming
functions, structs, globals, externs, and source positions
landed where you expected before touching the program at all.

For the same data with a click-to-explore surface, open the file in
the GUI: `the_bug file.bug`. Pick by context — `bug --dump` for
grep-friendly text on stdout, `the_bug` for the windowed browser.

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

## Reading machine code — `--bytes` and `--disasm` (objdump-in-bug)

Two static sub-commands read a function's emitted machine code straight
from the binary — no ptrace, no running the program. Both take a binary
and a function name, find the function in the `.bug` address map, map
its code-relative offset to a file position by parsing the container,
and then either hexdump or disassemble it:

```
bug --bytes  ./game _proj_hit_check     # raw bytes, 16 per line
bug --disasm ./game _proj_hit_check     # decoded instructions
```

Because they never attach to a process, they work where a live debugger
cannot: in Docker, on a cross-compiled binary, or on a Rosetta-translated
x86_64 process that `gdb` / `lldb` refuse to trace. This is the path that
caught the x86_64 self-host bugs during the Linux bring-up — an operand
order emitted as `3 << i` instead of `i << 3` reads straight off the
`--disasm` output. As of 2026-05-29 it has full parity on both backends.

**Both containers, both ISAs.** `--bytes`/`--disasm` detect the binary
format from its first byte (`0x7F` = ELF, `0xCF` = Mach-O 64) to find
where the code section begins:

- **ELF**: code starts where the `PT_NOTE` program header ends (the B++
  layout is `[ELF header][program headers][PT_NOTE][code]` with the exec
  `PT_LOAD` at `p_offset 0`).
- **Mach-O**: code starts at the `__TEXT/__text` section's file `offset`
  (the Mach header + load commands sit before it).

`--disasm` then picks the decoder from the **target byte in the `.bug`
map** (`0` = ARM64, `1` = x86_64), so a macOS `bug` disassembles a Linux
ELF as x86-64 and a Linux `bug` disassembles a Mach-O as arm64 — the
decoder follows the file, not the host.

**Coordinates.** Offsets in the left column are *code-relative* — the
same coordinate the `bug --dump` Address Map uses — so a `+0x664b8`
label, a `cbz x0, 0x66754` branch target, and a `bl 0x339d0` call target
all line up with the map. (They are not absolute VM addresses; that keeps
them stable across PIE load slides.)

`--disasm` disassembles the **whole function** (its length is the gap to
the next address-map entry) and interleaves the source line above the
instructions it covers (`; file:line`, from the v6 line table) —
objdump-`-l` style, entirely static.

**Coverage and the `.byte` / `.word` escape hatch.** Each decoder knows
the SUBSET its backend emits, which is enough to read codegen and spot
operand-order or wild-displacement bugs. Anything outside that vocabulary
prints as `.byte 0xNN` (x86, variable length) or `.word 0x........`
(arm64, fixed 4-byte, so the stream always stays aligned). On arm64 the
scalar coverage is ~99.998%; the only gap is NEON/SIMD, which appears
solely in auto-vectorized hot loops and is intentionally left raw.

Both need the `.bug` map present — build with `bpp --bug ...` (see
*Building a target with debug info* above). The map carries the function
names, the target byte, and the line table.

---

## Observe mode — watching a program run

`bug --tui ./program [args...]` launches the target under
`debugserver` (macOS) or `gdbserver` (Linux) and connects via the
GDB remote serial protocol. Every function entry the program
executes is traced by default, indented to show the call tree:

### Locating the remote stub

On macOS, the observer searches for `debugserver` in this order:

1. `BUG_DEBUGSERVER` env var (if set and the file exists).
2. `/Library/Developer/CommandLineTools/Library/PrivateFrameworks/LLDB.framework/Versions/A/Resources/debugserver`
3. `/Library/Developer/CommandLineTools/Library/PrivateFrameworks/LLDB.framework/Resources/debugserver`
4. `/Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Versions/A/Resources/debugserver`
5. `/Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Resources/debugserver`

The first hit wins. The CommandLineTools paths come first because
`xcode-select --install` is the smallest install that ships
`debugserver`; Xcode is the larger optional add-on. Two layouts
per toolchain (`Versions/A/Resources/` vs bare `Resources/`) cover
the framework symlink layouts that varied across macOS releases.

If none is found, `bug` prints every path it tried and the
`xcode-select --install` hint, plus the env-var override path:

```
export BUG_DEBUGSERVER=/opt/homebrew/Cellar/llvm/.../debugserver
```

On Linux, the observer searches for `gdbserver` in this order:

1. `BUG_GDBSERVER` env var (if set and the file exists).
2. `/usr/bin/gdbserver`
3. `/usr/local/bin/gdbserver`

Distro-specific install hints (`apt`, `dnf`, `pacman`, `apk`) are
printed when nothing is found.

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
| Phase 6.1  | Minisym section emission (Mach-O `__minisym`, ELF PT_NOTE `BPPMINI`) | shipped |
| Phase 6.2  | Runtime PC resolution (`_runtime_resolve_pc`) + `caller_pc` / `caller_name` builtins | shipped |
| Phase 6.3  | `panic(msg)` with stack trace via FP walk + `_runtime_resolve_pc` | shipped |
| Phase 6.4.1 | Cooperative sampling profiler (boundaries in `bpp_job` workers + maestro phases) | shipped |
| Phase 6.4.2 | SIGPROF supplement (macOS): per-thread mcontext capture + pthread_kill fan-out | shipped |
| Phase 6.5  | `caller(n)` sugar wrapper over `caller_pc` + `caller_name` | deferred (YAGNI — no consumer yet) |
| CLI / disasm | `bug` re-split as the Linux-capable CLI (`src/bug.bpp`); `--bytes` + `--disasm` objdump-in-bug; v6 line table source interleaving; ELF/x86-64 **and** Mach-O/arm64 parity (2026-05-28 → 29) | shipped |

Phase 6 detail: see the planning doc archived at
`legacy_bootstrap/legacy_docs/bug_phase6_plan.md` (kept for
historical context — every stage shipped except 6.5, deferred
YAGNI).

---

## Phase 6 user-facing API

A program built with `bpp` automatically links the runtime
profiler and panic primitives — no import required (the
`bpp_runtime.bsm` module is auto-injected, same path as
`bpp_io.bsm`).

```bpp
// Crash with a stack trace and exit 134 (SIGABRT convention).
// The trace lists every frame from the panic site up to main.
panic("invariant broken: cap < 0");

// Sampling profiler. profile_start arms a 1 kHz ITIMER_PROF
// + cooperative boundary hooks; profile_stop disarms; dump
// returns top-N (packed_name, count) pairs sorted by count.
profile_start(1000, 8);   // rate_hz, max stack depth
... user workload ...
profile_stop();

auto buf;
buf = buf_word(16 * 2);             // 16 entries × 2 words
auto n;
n = profile_dump(buf, 16);
auto i, packed, name_addr, name_len;
for (i = 0; i < n; i++) {
    packed = *(buf + i * 16);
    putnum(*(buf + i * 16 + 8)); putchar(' ');
    name_addr = _runtime_name_addr(packed);
    name_len  = _runtime_name_len(packed);
    sys_write(1, name_addr, name_len);
    putchar('\n');
}

// Caller introspection. caller_pc(n) walks n frames up the FP
// chain; caller_name(pc) resolves via the embedded minisym.
caller_pc(0)               // current function's body PC
caller_pc(1)               // caller's PC
caller_name(caller_pc(0))  // packed name of the current function
```

The profiler captures samples from every worker thread when
SIGPROF fires on main: the handler reads its own
`ucontext.uc_mcontext.__ss.__pc` and then `pthread_kill`s each
worker so each captures its own state on the same tick. The
synth-name re-attribution (`__synth_K` → caller frame) keeps
auto-dispatched loop bodies from appearing as anonymous workers
in the profile output.

---

## Appendix — what `bug` cannot do yet

- **No `name:line` breakpoints.** Function-entry only. Tracked
  as `docs/plans/todo.md` bug #3b.
- **No watchpoints.** Memory writes are not observed.
- **No attach-to-pid.** `bug` always fork-execs its target.
- **Linux GUI not wired.** `bug_observe_linux.bsm` speaks the
  GDB protocol but the GUI wrapper depends on the macOS Cocoa
  platform layer. Waits for Linux X11 GUI maturity.
- **Linux SIGPROF profiling.** Stubbed in `_core_linux.bsm`
  until ELF dynlink + thread parity ship — Linux runs single-
  threaded today, so the cooperative path covers it without
  the supplement.
- **No flamegraph export.** `profile_dump` returns the top-N
  table; folded-stacks export for `flamegraph.pl` lands when a
  consumer asks (YAGNI).

For the broader roadmap (Wolf3D, multicore Sprints, etc.), see
`docs/plans/multicore_state_report.md`. The original bug visualization
plan that this manual implements is archived at
`legacy_docs/bug_viz_plan.md` (Phases 1–5 all shipped — see the
status table above).
