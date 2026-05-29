# Linux full-suite gaps (x86_64 ELF)

**Status:** open — surfaced 2026-05-28 by the **first-ever full `run_all.sh` on
Linux**. The documented Linux verification (`bootstrap_manual.md` leg 3) only
ran `test_linux_*`/`test_x11_*` (10 tests). Running the whole suite (~194) under
the cross-compiled Linux `bpp` in Docker (`ubuntu:22.04`, glibc 2.35, x86_64
under Rosetta) gave **142 passed / 26 failed / 26 skipped**.

Context that frames every gap below:
- **Self-host is solid.** The Linux `bpp` bootstraps byte-stable inside the
  container (gen2 == gen3) and is byte-identical to the macOS cross-compile
  (gen1 == gen2, same sha256 `0db7599…`, 899 959 bytes) — codegen is
  deterministic and host-independent.
- **None of the failures are from the dynlink arc.** `test_extern_strlen`
  (the dynlink path) passes; the failing tests use the static `write_elf` path
  (0 FFI externs) and `E264` lives only in `a64_macho.bsm`. See
  `elf_dynlink_plan.md`.
- **15 of the 26 are now skip-listed** as unwired-platform (audio/MIDI = the
  ALSA door, 10; worker-pool/SIGPROF = threads via `clone(2)`, 5), mirroring the
  existing `test_thread/job/maestro` skips. `outline_smoke` is among the 15
  skipped but its crash is noted below.
- **10 of the 26 were root-caused and FIXED this session** — `test_modulab_prefs`
  (open-flags portability) + the 9-test x64 float-through-memory codegen bug
  (see "Resolved" below).
- **1 remains open**: `test_extrn_no_def_diag` — a missing diagnostic, below.

Post-fix the Linux suite is **152 passed / 1 failed / 41 skipped**.

## Open — E264 is not emitted on the ELF backend (1 test)

`test_extrn_no_def_diag`. **Not a miscompile — a missing diagnostic.** The "extrn
has no backing definition in the link graph" check (E264) is emitted only in
`a64_macho.bsm:1083`; the ELF target (`x64_elf.bsm`) never grew it, so an
undefined `extrn` global compiles cleanly on Linux instead of being refused.
Port the link-graph check + E264 emission into the ELF reloc-resolution path
(`elf_resolve_relocations`).

## Resolved this session

- **`test_modulab_prefs` — FIXED.** Root-caused live, not a `:ro`-mount artifact:
  the config dir (`~/.config/bpp/`) was created and writable, yet `prefs_save`'s
  `file_write_all` returned -1. The shared writers `file_write_all`
  (`bpp_file.bsm`) and `bo_save_file` (`bpp_bo.bsm`) hardcoded
  `sys_open(path, 0x601)` — the **macOS** `O_WRONLY|O_CREAT|O_TRUNC`. On Linux
  `0x601` decodes to `O_WRONLY|O_TRUNC|O_APPEND` (Linux `O_CREAT` is `0x40`), so
  **creating any new file failed**; only pre-existing files worked. The same
  flag bug had been fixed long ago for the ELF *output* writer (`x64_elf.bsm` →
  `0x241`, journal 2026-05-12) but never mirrored to the shared file I/O —
  exactly the "fixed for the macOS target, Linux deferred while the backend was
  blocked" pattern. Fix: a per-OS `_sys_open_create_flags()` in `_core_<os>.bsm`
  (0x601 macOS / 0x241 Linux, same auto-injected mechanism as
  `_stb_user_config_dir`), used by both shared writers. This was a real
  portability hole — *no* B++ program could create a new file via `file_write_all`
  on Linux. Verified in Docker (`modulab_prefs` → OK, file written); Linux suite
  143/10/41. Commit `a6d57b7`.

- **x64 float-through-memory — FIXED (9 tests).** `test_json_float`,
  `test_level_v2`, `test_struct_field_float_propagation`, `test_vec2_arithmetic`,
  `test_vec2_float_roundtrip`, `test_newtype`, `test_ecs_archetype`,
  `test_ecs_component`, `test_peek_widths` all failed identically on x64-ELF: a
  float that round-trips through memory **lost its fraction** (`1.5` read back as
  `1.0`, `peekfloat` returned the bits as an int). Scalar/in-register floats were
  fine; only struct-field / `pokefloat`-`peekfloat` memory broke. Three x64-only
  spots treated the float slot as int — **not a Rosetta artifact**, a real
  codegen type/width bug (the systematic `1.5 → 1.0`, with integer memory ops
  intact, proved it): (1) `_x64_emit_memst_float_scalar` delegated to the
  truncating int path (`cvttsd2si`); (2) `_x64_emit_memld_load` had no
  `is_float_type` branch so a `: float` field loaded via an 8-byte integer load;
  (3) `_x64_emit_float_store_scalar` stored the low 32 bits of the double for
  `SL_HALF` without `cvtsd2ss`. All three now mirror the a64 paths. Verified in
  Docker; Linux suite 143/10/41 → **152/1/41**. Commit `4193fb1`.

## Skipped-but-noted: outline_smoke crashes instead of degrading

`test_outline_smoke` is skip-listed under the concurrency-infra class (it
dispatches via `job_parallel_for_data`, which needs the worker pool). The note:
on Linux it **SIGSEGVs (139)** rather than degrading to a serial run — the
doctrine is "outlining is serial-correct on Linux until ELF threads ship", so
when threads land, the serial fallback for the outlined path must be made to
**run, not crash**. Fix alongside the thread/`clone(2)` work.

## How to reproduce

```bash
# cross-compile the seed Linux bpp on macOS
BPP_BUILD_ID=00000000000000000000000000000000 ./bpp --linux64 src/bpp.bpp -o /tmp/bpp_lx
# bootstrap + full suite inside the container (repo read-only, ./bpp overlaid)
docker run --rm --platform linux/amd64 \
  -v "$PWD":"$PWD":ro -v /tmp/bpp_lx:"$PWD"/bpp:ro -v /tmp:/tmp -w "$PWD" \
  ubuntu:22.04 sh tests/run_all.sh
```

Resolution gate (Rule-style): a single failing native test is a real bug; the
same is true here. Do not let these rot — close them when the x86 hardware /
thread work happens, and delete this file when the list reaches zero.
