# x86_64 — x86_64 target bundle

This folder is the **complete target backend** for x86_64 + Linux.

Today the relationship is one-to-one: B++ supports a single x86_64 OS
(Linux), so the chip folder doubles as the Linux target folder. The
files inside are not pure chip code — they're a chip+OS bundle.

## Contents

| File | Layer | Purpose |
|------|-------|---------|
| `x64_enc.bsm` | chip | x86_64 instruction encoder (pure encoding tables) |
| `x64_elf.bsm` | OS | ELF binary writer for static Linux executables |
| `x64_codegen.bsm` | chip+OS | AST → x86_64 machine code, uses Linux syscall numbers and ELF sections |
| `_stb_platform_linux.bsm` | OS | X11 wire-protocol window + ANSI terminal fallback + Linux keyboard/mouse |
| `bug_observe_linux.bsm` | OS | Process observation via gdbserver (Linux GDB remote backend) |

## Future split

When B++ adds Windows x86_64 or macOS Intel support, this folder will
be refactored into three pieces:

```
src/
├── arch/x86_64/         encoder.bsm, registers.bsm   (chip pure)
├── os/linux/            elf.bsm, syscalls.bsm,
│                        _stb_platform.bsm, bug_observe.bsm
├── targets/
│   ├── x64_linux.bsm    (~30 lines, glues arch/x86_64 + os/linux)
│   └── x64_windows.bsm  (~30 lines, glues arch/x86_64 + os/windows)
```

The `targets/<chip>_<os>.bsm` file will be a thin combinator that
imports the chip encoder and the OS layer for a specific pairing.

Until that refactor happens, **do not add new files to this folder
that are pure chip code with no OS dependency** — they'll just need to
be moved out later. Same goes for new pure-OS files: keep them
co-located with the existing Linux files here for now, but mark them
clearly with the OS name in the filename (e.g. `_linux`, `bug_*_linux`).

## See also

- `src/aarch64/README.md` — equivalent target bundle for aarch64+macOS
- `docs/todo.md` — "Target architecture refactor" entry tracking the split
