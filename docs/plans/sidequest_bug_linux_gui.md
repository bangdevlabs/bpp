# Sidequest — Bang 9 + the Debug panel on Linux (async-observe + X11 dialogs parity)

**Status:** scoped 2026-05-27. Not started.

**Goal:** make Bang 9 (and its embedded `the_bug` Debug panel) cross-compile to
Linux x86_64 and open in Docker/XQuartz. This is the GUI half of the Linux
unblock — the toolchain (`bpp`) and the CLI debugger (`src/bug.bpp`) already
self-host / run on Linux; the IDE does not yet, because two function families
exist only in the macOS backend.

This is the *bug equivalent of the codegen parity arc* (May 27): the Linux
backend lags the macOS backend in two well-bounded layers, and pairing them
realizes the project premise — **programs (and tools) open on any backend.**

## The gap (empirical)

`./bpp --linux64 bang9/bang9.bpp` fails with `E201` for ten functions, in two
families:

1. **Async-observe API** (the debugger half) — used by `the_bug_lib.bsm`'s
   frame-driven Debug panel:
   - `bug_run_async_start(target, start_idx, argc, bp_names, bp_count, flag_break_all)`
   - `bug_run_poll()` — non-blocking; drains GDB events, returns 1 on stop.
   - `bug_run_resume()`
   - `bug_run_async_stop()`
   Defined in `src/backend/os/macos/bug_observe_macos.bsm` (~1130–1280);
   **absent** from `src/backend/os/linux/bug_observe_linux.bsm`, which has only
   the synchronous `bug_run` (for `--tui`).

2. **Native dialogs / alerts** (the shell half) — used by the Bang 9 shell:
   - `_stb_dialog_open(...)`, `_stb_dialog_save(...)`, `_stb_dialog_open_folder(...)`
   - `_stb_alert_info(title, msg)`, `_stb_alert_error(...)`, `_stb_alert_confirm(...)`
   Defined in `src/backend/os/macos/_stb_platform_macos.bsm` (Cocoa
   `NSAlert` / `NSOpenPanel` / `NSSavePanel`); **absent** from the Linux X11
   layer `src/backend/os/linux/_stb_platform_linux.bsm`.

Everything else already works on Linux: the X11 window + framebuffer
(`_stb_init_window`, `_stb_present` via `XPutImage`), input (`_stb_poll_events`),
and Bang 9 is CPU-rendered (Rule 23 — `put_px` blit, not the GPU path), so it
does **not** need Vulkan. The two families above are the whole gap.

## Sub-arc A — Async-observe parity (LOW–MEDIUM)

A pure reshape, not new protocol work. The GDB-remote client primitives
(`gdb_connect`, `gdb_handshake`, `gdb_query_stop`, `gdb_query_slide`,
`gdb_try_recv_stop`, `gdb_continue_send`) all live in the **shared**
`src/bug_gdb.bsm`, and the Linux observer already has `_spawn_gdbserver` +
`parse_elf_header` + the synchronous `bug_run` that drives them.

Port the four async functions into `bug_observe_linux.bsm`, mirroring the macOS
shapes, swapping:
- `_spawn_debugserver` → `_spawn_gdbserver` (exists)
- `parse_macho_header` → `parse_elf_header` (exists; sets `_elf_text_addr`)
- Mach-O base addrs (`_mo_text_addr`, `_mo_data_seg_vmaddr`, slide) → ELF
  equivalents. For a static ELF there is no ASLR slide at this layer, so
  `_async_text_base` = the ELF text vaddr and `_async_data_base` = the data
  segment vaddr directly (verify against the `.bug` address-map coordinate).
- `bug_shared_*` snapshot wiring is shared — reuse verbatim.

~130 LoC. The control flow (`start` = spawn+connect+handshake+set-bp+continue;
`poll` = non-blocking `gdb_try_recv_stop`; `resume` = continue; `stop` = kill)
copies macOS directly.

**Caveat (important):** `gdbserver` ptraces the target. Under Docker/Rosetta on
Apple Silicon, ptrace of a translated x86_64 process is blocked (the same wall
the bug hunt hit). So **live observe via the Debug panel is validated on real
x86 Linux hardware**; under Docker the window + map browser + `.bug` load work,
but stepping a running tracee may not. That's a Rosetta limitation, not a code
gap — and exactly why we want real x86 hardware eventually.

## Sub-arc B — X11 dialogs / alerts parity (MEDIUM–HIGH)

macOS uses Cocoa; X11 has no native file picker or alert. Two approaches:

- **(i) In-window widgets** — draw modals + a file-browser into the existing
  X11 framebuffer. Robust, zero external deps, works in a bare Docker
  `ubuntu:22.04`. More code (the file-list browser is the bulk).
- **(ii) Shell out** to `zenity` / `kdialog` / `xmessage`. Quick, but fragile —
  not present in bare Docker, adds a runtime dependency.

**Recommendation: (i) in-window.** It matches "open on any backend" with no
deps and works where the X11 window already works. Split by difficulty:
- **Alerts** (`_stb_alert_info` / `_error` / `_confirm`) — EASY/MEDIUM. A modal
  box drawn into the framebuffer: dim background, message text, `OK` (+ `Cancel`
  for confirm), a small event loop until a button or key. Returns confirm's
  bool.
- **File dialogs** (`_stb_dialog_open` / `_save` / `_open_folder`) — MEDIUM/HIGH.
  An in-window directory browser: list entries (`sys_*` dir read), navigate
  up/down, type-to-filter or a text field for save names, select. The biggest
  single unknown of the arc.

**Decision needed:** confirm in-window vs zenity (or a hybrid: zenity if
`$PATH` has it, else in-window). I lean in-window for portability.

## Phasing (Discipline #2 — stub the Linux parity first)

- **Phase 0 — Stubs (parity floor).** Add all ten functions to the Linux
  backend as minimal stubs (async returns sane defaults / IDLE; dialogs return
  cancel/empty; alerts → `putstr_err`). Bang 9 cross-compiles (E201s gone).
  **High value on its own:** it's the first time we can test whether Bang 9's
  *core* (X11 window + draw + input) even runs on Linux/Docker today — the real
  test of "the IDE opens on any backend." A window appearing in XQuartz, even
  with stub dialogs, is the milestone.
- **Phase 1 — Async-observe real** (sub-arc A). Debug panel loads + (on real
  x86 Linux) observes.
- **Phase 2 — Alerts real** (in-window modals).
- **Phase 3 — File dialogs real** (in-window browser).
- **Phase 4 — End-to-end** — cross-compile Bang 9, run in Docker/XQuartz per
  `bootstrap_manual.md`'s flow; verify window opens, Debug tab loads a `.bug`,
  alerts/dialogs function. Edit→build→debug loop on the backend we unblocked.

## Verification gates (every phase)

- Native suite 180/0/12 + bootstrap byte-stable (these are Linux-backend +
  X11-layer changes; they must not perturb a64 codegen output).
- C-emit suite 145/0/47 unaffected.
- `./bpp --linux64 bang9/bang9.bpp -o /tmp/bang9_lin` → rc 0, no E201.
- Phase 4: window opens in XQuartz from Docker.

## Risks

1. **Rosetta ptrace** (sub-arc A live-observe) — documented above; window +
   map-load are unaffected, live stepping needs real x86 hardware.
2. **The file-browser widget** (sub-arc B Phase 3) is the largest unknown — it's
   UI code with no existing analogue in the codebase.
3. **Latent Linux-X11 gaps** — the platform layer "waits to grow more" per
   `debug_with_bug.md`; once Bang 9 actually runs, other small gaps (font
   metrics, clipboard, cursor) may surface. Phase 0 flushes these early.

## Relationship to the `./bug` entry-point decision

Independent but adjacent: `install.sh` builds `./bug` from the GUI
`tools/the_bug/the_bug.bpp`, while the analysis features (`--bytes`/`--disasm`)
live in the CLI `src/bug.bpp`. Recommended (separate, smaller change): make
`./bug` the CLI build; `the_bug_lib` stays as the Bang 9 Debug panel (untouched
by this arc). `docs/manual/debug_with_bug.md` is stale and should be rewritten
to match (CLI canonical + Bang 9 Debug panel; not the retired "merged into
the_bug" story).
