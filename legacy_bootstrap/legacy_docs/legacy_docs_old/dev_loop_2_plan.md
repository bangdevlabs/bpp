# Dev Loop 2 MVP — `bug --break` runtime debugger

## Context

Ship a runtime debugger that unblocks three stalled bugs from the
2026-04-16 type-defense sprint (W025 regression, init_dispatch
16-seed threshold, diagnostic line-number off by 365). All three need
single-step observation of gen1 during its self-compile; today the
team has no tool to do that and has been debugging by printf.

Strategic pivot approved on 2026-04-16: pause Level 4 B+C, ship this
debugger first, return to Level 4 with the right tool.

**Scope goal (one sentence):** `bug /tmp/bpp_broken --break _val_w025_check src/bpp.bpp -o /tmp/x`
stops at the entry of `_val_w025_check`, prints the calling function's
args, continues, and reports the normal call-tree for the rest of the
program.

## Surprise finding from the gap audit

The `bug` "observe mode" is **already implemented** despite the help
text saying "TODO". Lines 410-594 of `src/backend/os/macos/bug_observe_macos.bsm`
are ~185 LOC of mature code: spawns `debugserver`, handshakes GDB
protocol, parses the Mach-O symbol table, queries ASLR slide, sets
breakpoints at every function entry, handles hits with indented
call-tree printing + first-4-args dump, reads stack on crash, walks
frame-pointer chain. The GDB protocol client (`src/bug_gdb.bsm`, 489
LOC) is production-ready — Z0/z0/c/s/registers/memory all work.

The MVP is additive: **filter which breakpoints to set** instead of
setting all. Everything downstream of "breakpoint hit" is already
shipped.

## What the MVP must deliver

| Capability | Behavior |
|---|---|
| `bug <prog> [args…]` | Unchanged: traces every function (current behavior). |
| `bug --break <fn> <prog> [args…]` | Stops only at `<fn>` entry. Can repeat `--break` N times. |
| `bug --break <fn>:<line> <prog>` | **Scope cut for MVP:** resolves if the line matches the function's entry line (from source_pos), else errors "per-line breakpoints require full line map (post-MVP)". Rationale below. |
| `--break-all` | Explicit opt-in for today's trace-every-function mode when mixed with `--break`. |

**Why `name:line` is scope-cut.** A true per-line breakpoint needs a
full line → PC table in the .bug file. Today only ONE PC per function
is recorded (the entry). Wiring the full table is ~150 extra LOC in
`bpp_bug.bsm` + the compiler's codegen pass, not a free add. For the
three bugs we need to debug, `--break <fn>` at entry is enough. The
`<fn>:<line>` form lands in Dev Loop 2.5 (post-MVP).

## Architecture

Four sub-steps, each independently bootstrappable. Each commits on its
own so the history tells the story.

### Sub-step 1 — populate `source_pos` in the `.bug` file (~15 LOC)

**File:** `src/bpp_bug.bsm` around line 148 (the
`bo_write_u32(0)` that hardcodes every function's source position).

Replace with a token-to-line lookup that mirrors what `diag_loc` does
in `bpp_diag.bsm:131-153`:

```bpp
auto tok_idx, line_abs, mi, base_line, rel_line;
tok_idx   = *(funcs[i] + FN_SRC_TOK);
line_abs  = *(tok_lines + tok_idx * 8);
mi        = mod_idx(*(tok_src_off + tok_idx * 8));
base_line = arr_get(diag_file_line_starts, mi);
rel_line  = line_abs - base_line;
bo_write_u32(rel_line);
```

`source_pos` semantics: **line number within the declaring module**
(1-based). For module 0 (the entry `.bpp`), `mod_idx()` already
accounts for `mod0_real_start` via binary search over
`diag_file_starts`. No special case needed.

**Reuse, do NOT reinvent:** the entire tok→line path already exists.
The error log uses it daily.

**Verification:** compile any program with `--bug`, dump with `bug
<file.bug>`, confirm source_pos values are real line numbers (not 0)
and match the actual source positions.

### Sub-step 2 — CLI `--break` parser in `bug.bpp` (~45 LOC)

**File:** `src/bug.bpp`, `main()` argv loop.

Iterate argv from index 1. For each arg:
- Starts with `--break ` (next arg is the spec): push onto `bp_specs[]` array.
- `--break-all`: set `flag_break_all = 1`.
- Anything else (no `--` prefix): the target program path. Stop
  parsing flags, remaining args feed the target.

Spec format: `<name>` or `<name>:<line>`. Parse the `:` split, store
name + line (0 if no colon). Cap the number of breakpoints at 32 or
so — practical, avoids dynamic alloc for MVP.

Pass `bp_specs` array and count into `bug_run(target_path, start_idx,
argc, bp_specs, bp_count, flag_break_all)`.

**Legacy behavior:** `bug <prog>` with no `--break` flags keeps
today's behavior (trace all functions). `--break-all` is only needed
when the user wants both — e.g. `--break foo --break-all` to highlight
`foo` inside the full trace. MVP can skip this nuance; treat "no
`--break` given" as "trace all" exactly as today.

### Sub-step 3 — breakpoint spec resolver in bug_observe_macos (~50 LOC)

**File:** `src/backend/os/macos/bug_observe_macos.bsm`, new helper.

```bpp
static resolve_break_spec(spec_name, spec_line, text_base) {
    // Returns address, or 0 on lookup failure.
    auto i, fn_idx;
    fn_idx = -1;
    for (i = 0; i < bug_am_count; i = i + 1) {
        if (packed_eq(bug_am_name[i], spec_name)) { fn_idx = i; }
    }
    if (fn_idx < 0) { return 0; }
    if (spec_line > 0) {
        // Scope-cut for MVP: only accept if spec_line matches the
        // function's entry line. Any other line rejects with a helpful
        // message pointing to Dev Loop 2.5 for full line-map support.
        auto declared_line;
        declared_line = lookup_source_pos(fn_idx);
        if (declared_line != spec_line) {
            print_msg("bug: --break ");
            print_packed(spec_name);
            putchar(':'); print_int(spec_line);
            print_msg(" — only function-entry line ");
            print_int(declared_line);
            print_msg(" is supported in the MVP (full line map: post-MVP)\n");
            return 0;
        }
    }
    return text_base + bug_am_off[fn_idx];
}
```

**Edge case:** static vs cross-module name collisions. Address map
stores exported symbols only, so static function names appear once.
Good enough for MVP.

**Ambiguity resolved:** `main:42` means **line 42 of the source file
where `main` is declared** (i.e. relative to the module — which for
the entry `.bpp` means relative to line 1 after imports resolve).
This matches what the user types when reading the source.

### Sub-step 4 — selective breakpoint setting in bug_run (~30 LOC)

**File:** `src/backend/os/macos/bug_observe_macos.bsm`, lines 458-466
(the current "loop over all functions" block).

Replace with:

```bpp
if (bp_count > 0) {
    // User specified --break flags: set only those.
    for (i = 0; i < bp_count; i = i + 1) {
        auto addr;
        addr = resolve_break_spec(bp_specs[i].name, bp_specs[i].line, text_base);
        if (addr != 0) {
            gdb_set_bp(addr);
            print_msg("bug: break set at ");
            print_packed(bp_specs[i].name);
            if (bp_specs[i].line > 0) { putchar(':'); print_int(bp_specs[i].line); }
            putchar('\n');
        }
    }
} else {
    // Legacy: trace every function (existing code path).
    for (i = 0; i < bug_am_count; i = i + 1) {
        gdb_set_bp(text_base + bug_am_off[i]);
    }
}
```

**Breakpoint-hit handler (lines 490-534) stays unchanged** — it
reads PC, looks up function, prints indented call. Whether only one
breakpoint fires or all of them, the handler is the same.

## Files touched

| File | Change | LOC |
|---|---|---|
| `src/bpp_bug.bsm` | populate source_pos | ~15 |
| `src/bug.bpp` | argv parse for `--break` | ~45 |
| `src/backend/os/macos/bug_observe_macos.bsm` | resolver + selective BP | ~80 |
| `tests/test_bug_break.bpp` (NEW) | sample target for manual tests | ~30 |
| `docs/warning_error_log.md` | note on `--break` error codes | ~5 |
| **Total** | | **~175** |

Slightly over the agent's 160 LOC estimate because the resolver
needs to inspect source_pos (~20 extra LOC for the sub-step 3 MVP
error path). Still well under the original 300-400 estimate.

## Bootstrap + verification gates

Bootstrap only needed for sub-step 1 (touches `bpp_bug.bsm` which is
compiler source). Sub-steps 2-4 touch `bug.bpp` / `bug_observe_macos.bsm`
which are the debugger itself — rebuild the `bug` binary, no
bootstrap cycle.

After each sub-step, verify:

**Sub-step 1:**
```
./bpp src/bpp.bpp -o /tmp/bpp_verify
./bpp --bug tests/test_vec_simd.bpp -o /tmp/t
./bug /tmp/t.bug | grep source_pos   # non-zero values
```

**Sub-step 2:**
```
./bpp src/bug.bpp -o /tmp/bug_new
/tmp/bug_new                         # prints updated usage
/tmp/bug_new --break main /tmp/vec   # doesn't error, even if no behavior yet
```

**Sub-step 3+4 (MVP done):**
```
/tmp/bug_new --break _val_w025_check /tmp/bpp_broken src/bpp.bpp -o /tmp/x
# Expected: debugserver attaches, breakpoint fires at _val_w025_check
#          entry, prints args, continues. Bug #1 becomes debuggable.
```

## What lights up after MVP ships

Immediate follow-on work — the three bugs we pivoted for:

1. **Bug #1 (W025 regression).** Git checkout the W025 branch,
   rebuild gen1 with `--bug`, run:
   ```
   bug --break _val_w025_check gen1 src/bpp.bpp -o /tmp/x
   ```
   Step through the compilation, observe where "break" gets mis-tokenized.
2. **Bug #2 (init_dispatch 16-seed).** Add the 16th `add_extern_effect`
   back, rebuild gen1 with `--bug`, run with breakpoints on
   `scan_token` (lexer) and `is_keyword_len5`. See the exact moment
   `break` gets TK_ID instead of TK_KW.
3. **Bug #3 (line numbers off by 365).** Breakpoint on `diag_loc` and
   inspect how `line` vs `base_line` resolve. Likely a missing
   `mod0_real_start` subtraction somewhere.

All three should fall in a single session with the debugger in hand.

## Deferred (Dev Loop 2.5)

- Per-line breakpoints (`--break fn:line` for any line, not just
  entry). Needs a PC-to-line table in the `.bug` file populated at
  codegen time. ~150 LOC across `bpp_bug.bsm` + both chip codegens.
- Interactive REPL (step, continue, watch, print-var).
- Conditional breakpoints.
- Linux gdbserver backend (today observe is macOS-only via
  debugserver; Linux has the `bug_observe_linux.bsm` stub only).

## Risk register

| Risk | Mitigation |
|---|---|
| `source_pos` values collide with existing 0-valued dump-mode display | Dump reader ignores value semantics; printing real numbers is strictly more informative. No breakage. |
| `--break` name spec hits a `static` function from another module | Address map lists every function; collisions unlikely. If we hit one, post-MVP adds `file.fn:line` qualified form. |
| Debugserver spawn fails on macOS 15+ | Already handled by existing code path (lines 75-134 in bug_observe_macos). If it breaks on a new OS release, that's a separate bug. |
| MVP's line-form rejection annoys the user | Error message points explicitly to Dev Loop 2.5 so they know the scope cut is deliberate. |

## Success criterion

A single command reproduces a breakpoint hit with indented call tree:

```
$ bug --break _aud_square_cb tests/test_audio_tone
bug: break set at _aud_square_cb
[spawn debugserver, attach, run]
→ main
  → audio_tone_test(440, 2000)
    → _stb_audio_tone_start(440)
      ... (no breakpoints hit here — callback not invoked yet)
    → _aud_square_cb(<buf>, <queue>, <buffer>)    [BREAK HIT]
                                                    args: 0x0 0x... 0x...
[continue, runs normally]
exit 0
```

If that output lands, MVP ships and the three blocker bugs become
directly debuggable.
