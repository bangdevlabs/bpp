# Debugging B++ with `bug`

`bug` is the native B++ debugger. It reads the `.bug` debug map the
compiler writes alongside every binary and drives the target through
the platform's remote stub (`debugserver` on macOS, `gdbserver` on
Linux). There is no GDB, no LLDB, no DWARF — it is a B++ program
reading a B++-defined format and speaking the GDB remote serial
protocol directly.

This document covers every feature `bug` exposes today, the workflow
behind each, and a closing case study of the Apr 16 investigation
that turned `bug` from a toy tracer into a tool strong enough to
find a Mach-O writer bug in under an hour.

---

## Two modes

`bug` dispatches on the first non-flag argument. If it ends in
`.bug`, the program enters **dump mode**. Otherwise it enters
**observe mode** and treats the argument as the target executable.

```
bug hello.bug                  # dump mode
bug ./hello                    # observe mode — runs ./hello and traces it
bug --break main ./hello       # observe mode with a selective breakpoint
```

### Building a target with debug info

Every compile writes a `.bug` file next to the binary. It is always
on — no `-g` switch. Keep the `.bug` alongside the binary; observe
mode reads it by appending `.bug` to the target path.

```
bpp hello.bpp -o hello         # produces ./hello and ./hello.bug
bug ./hello                    # finds ./hello.bug automatically
```

---

## Dump mode — inspecting the debug map

`bug file.bug` parses a `.bug` file and prints every section the
compiler emitted. Useful for confirming that functions, structs,
globals, externs, and source positions landed where you expected
before touching the program at all.

```
bug hello.bug
```

Output sections, in order:

- **Functions** — name, parameter list with types, return type,
  local variables with types.
- **Structs** — struct layout with field offsets, sizes, types.
- **Globals** — name, type, storage class (`[auto]`, `[global]`,
  `[extrn]`).
- **Externs** — name, library, argument list.
- **Source Map** — per-function `(module index, line number)` pair.
  Populated by `bpp_bug.bsm` using the `FN_SRC_TOK` captured at
  function-name entry. A `line=0` row means the function came from
  a module whose token state was dropped before the bug writer ran.
- **Address Map** — function name → byte offset in the text
  section. This is the two-hop input used by observe mode to turn
  function names into breakpoint addresses.
- **Stack Map** — per-function frame size and local variable offsets.
  Needed for future REPL-style variable inspection; already emitted.

When a local or field shows `ty:<number>`, the debug writer saw a
type it doesn't know how to print. Common offenders are new slice
constants that the dump formatter (`out_type` in `bug.bpp`) hasn't
been taught yet — adding a one-liner there is a routine patch.

---

## Observe mode — watching a program run

`bug ./program [args...]` launches the target under `debugserver`
(macOS) or `gdbserver` (Linux) and connects via the GDB remote
serial protocol. Every function entry the program executes is
traced by default, indented to show the call tree:

```
main
  setup_world
    spawn_enemy
    spawn_enemy
  run_frame
    update_enemy
    render
  teardown
```

Target stdout flows through unchanged — target `printf`-equivalents
print exactly where they would if you ran the program directly.
This is possible because `bug_gdb.bsm` now decodes `O`-packets (the
GDB protocol's "target output" framing) and writes the bytes to
fd 1. Earlier revisions swallowed them.

### Arguments to the target

Every argv token after the target path is forwarded to the program.
macOS `debugserver` parses its own flags greedily, so observe mode
inserts a literal `--` before the forwarded args to mark the
boundary:

```
bug ./game --level 3
  → debugserver gdb-remote 127.0.0.1:1234 ./game -- --level 3
```

Without the `--`, `debugserver` would swallow `--level` as one of
its own options and you would spend twenty minutes wondering why
your game has no arguments.

### Target stdin / stdout

Both are passed through. Earlier observe-mode revisions redirected
the target's stdio to `/dev/null` along with `debugserver`'s — this
is fixed. If your program reads from stdin, run it under `bug` the
same way you'd run it directly.

---

## Selective breakpoints — `--break <fn>`

By default observe mode traces every function in the program. That
is an excellent default for "what is this binary doing" surveys and
a terrible default for "I need to know exactly when `foo` runs."
`--break <fn>` narrows the trace to specific entry points.

```
bug --break update_enemy ./game
bug --break func_a --break func_b ./target
```

Each `--break` adds one function name to a 32-slot array. Only the
named functions are registered with the remote stub; every other
function entry is invisible. When you hit a breakpoint, you get
the full register dump and can ask the target to continue.

`name:line` breakpoints are **not yet implemented**. The scaffolding
exists (per-function source position is in the `.bug` file), but
resolving `source.bpp:42 → function index → byte offset` hits an
off-by-N in the source map we haven't pinned yet. Passing a
colon-bearing spec returns a clean error today instead of
resolving wrong. See `docs/todo.md` bug #3b.

### Combining selective with full trace — `--break-all`

If you want selective breakpoints *and* the legacy trace for
everything else, add `--break-all`:

```
bug --break update_enemy --break-all ./game
```

Without `--break-all`, any `--break` flag switches observe mode
into "only the named functions" behaviour. The distinction matters
when your breakpoint lives deep in a call chain you also want to
see.

---

## Pointer-argument introspection — `--dump-str`

Every breakpoint hit prints the stopped function name and the
general-purpose registers. `--dump-str` adds one extra piece: for
each of `x0` and `x1` (ARM64) / `rdi` and `rsi` (x86_64), `bug`
reads up to 32 bytes from the register's pointer value and prints
the result as a C string if the bytes are printable ASCII.

```
bug --break my_fn --dump-str ./program
```

Output shape at each hit:

```
BP hit: my_fn  x0=0x100004000  x1=0x100004020
  x0="hello world"
  x1="look up this literal"
```

This is the single most valuable flag in the debugger. Most bugs
that manifest as "wrong behaviour" boil down to "the function
received a different pointer than I expected." `--dump-str` shows
you the pointer's target at the exact moment the callee sees it —
not at the call site, not before the callee shuffles registers,
but at function entry with the final argument state.

When a pointer doesn't point at a printable string, `--dump-str`
shows garbled bytes or NULs. Useful signal: if `x1="""".g.."` where
you expected `x1="break"`, you have a literal-dedup or
string-relocation bug, not a logic bug.

---

## Case study — the page_count bug (2026-04-16)

**Symptom.** Adding a 16th entry to the dispatch-seed list in
`init_dispatch()` broke bootstrap with a cryptic compile-time
error: `global 'break' not found in data section`. The 15th entry
was fine; any 16th name whose string was four bytes or longer
flipped the compiler into a wrong-output state.

**First instinct.** Token table overflow. `tok_lines` and
`tok_src_off` had been 1 MB since the self-host; they were
saturating. Bumped both to 8 MB, bumped `outbuf` to 8 MB for good
measure. No change. "aumentar os canais não foi o suficiente, é
algo na contagem que desloca" — the user's insight. The trigger
was tied to *count*, not volume.

**Why `bug` got stronger first.** Before the actual debugging
session, `bug` needed four fixes to be viable against a
self-compile:

1. `debugserver` was receiving its own `-o` flag as a target
   argument. Added the `--` separator.
2. Target stdout was redirected to `/dev/null` along with
   `debugserver`'s stderr. Removed the redirect.
3. `O`-packets (the GDB protocol's target-output frames) were being
   discarded. Added a hex decoder that re-emits them on fd 1.
4. `argv[start_idx]` was being passed twice to the target.
   Off-by-one in the forward loop.

With those in place, `bug` could actually run the compiler under
trace.

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
address were *wrong bytes*. The pointer resolved via ADRP+ADD was
pointing to unrelated memory.

**The real bug.** ARM64 addresses literals via `ADRP x0, page +
ADD x0, offset`. The ADRP+ADD pair is set at link time by reading
a chained fixup from the Mach-O. The chained fixup header has a
`page_count` field. In `a64_macho.bsm` it was hardcoded:

```
mo_w8(1); mo_w8(0);  // page_count = 1 as uint16
```

As long as `__DATA` fit in one 16KB page, the hardcoded `1`
worked. The 16th dispatch seed (`sys_lseek`, the four-byte literal
on top of 15 prior four-byte-or-longer literals) pushed `__DATA`
past 16 KB. dyld only rebased page 0. GOT pointers on page 1 kept
their on-disk values — valid at compile time, garbage after ASLR
slid the binary. String reads from page 1 returned whatever
happened to be in the process's address space at the rebased
offset.

**Confirming signal that had been there all along.** Running
`./bpp` with `DYLD_PRINT_WARNINGS=1` surfaced
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
_hdr_size = 22 + _pc * 2;
if ((_hdr_size & 3) != 0) {
    _hdr_size = _hdr_size + (4 - (_hdr_size & 3));
}
mo_w32(_hdr_size);
// ... emit page_count and per-page start offsets ...
mo_w8(_pc & 0xFF); mo_w8((_pc >> 8) & 0xFF);
_pg = 0;
while (_pg < _pc) {
    if (_pg == got_page) {
        mo_w8(got_page_off & 0xFF);
        mo_w8((got_page_off >> 8) & 0xFF);
    } else {
        mo_w8(0xFF); mo_w8(0xFF);  // no fixups on this page
    }
    _pg = _pg + 1;
}
```

The `imports_off` computation that immediately followed had the
same hardcoded assumption (`+ 24`); replaced it with the dynamic
`_seg_size` matching the writer.

**What `bug` earned.** Every feature listed in this document —
`--dump-str`, the `--` separator, pass-through stdout, the
`O`-packet decoder — came out of this one investigation. The
debugger paid for itself in the session it was extended.

**Lesson.** When a compile-time error's message mentions a string
literal that the compiler should have emitted, the first question
is not "where is the lookup failing" but "does the runtime see the
bytes the file says are there." `--dump-str` answers that question
in under a second. Everything else is plumbing around it.

---

## Appendix — what `bug` cannot do yet

- No interactive REPL. You get traced call tree + optional
  `--dump-str`; you cannot step, inspect locals, or evaluate
  expressions. Planned for Dev Loop 2 Day 2+.
- No `name:line` breakpoints. Function-entry only. Tracked as
  bug #3b in `docs/todo.md`.
- No watchpoints. Memory writes are not observed.
- Linux port is partial. `bug_observe_linux.bsm` speaks the GDB
  protocol but has less polish than the macOS path; the `--`
  separator fix applies on Linux too but the O-packet decoder was
  tested under `debugserver`.
- Attach-to-running-pid is not wired. `bug` always fork-exec's its
  target.
