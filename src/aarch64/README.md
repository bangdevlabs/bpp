# aarch64 — ARM64 target bundle

This folder is the **complete target backend** for ARM64 + macOS.

Today the relationship is one-to-one: B++ supports a single ARM64 OS
(macOS), so the chip folder doubles as the macOS target folder. The
files inside are not pure chip code — they're a chip+OS bundle.

## Contents

| File | Layer | Purpose |
|------|-------|---------|
| `a64_enc.bsm` | chip | ARM64 instruction encoder (pure encoding tables) |
| `a64_macho.bsm` | OS | Mach-O binary writer + macOS code signing |
| `a64_codegen.bsm` | chip+OS | AST → ARM64 machine code, uses macOS syscall numbers and Mach-O sections |
| `_stb_platform_macos.bsm` | OS | Cocoa window + Metal GPU + CoreGraphics + macOS keyboard/mouse |
| `bug_observe_macos.bsm` | OS | Process observation via debugserver (macOS GDB remote backend) |

## Future split

When B++ adds Linux ARM64 support, this folder will be refactored
into three pieces:

```
src/
├── arch/aarch64/        encoder.bsm, registers.bsm   (chip pure)
├── os/macos/            machof.bsm, syscalls.bsm, codesign.bsm,
│                        _stb_platform.bsm, bug_observe.bsm
├── targets/
│   ├── arm64_macos.bsm  (~30 lines, glues arch/aarch64 + os/macos)
│   └── arm64_linux.bsm  (~30 lines, glues arch/aarch64 + os/linux)
```

The `targets/<chip>_<os>.bsm` file will be a thin combinator that
imports the chip encoder and the OS layer for a specific pairing.

Until that refactor happens, **do not add new files to this folder
that are pure chip code with no OS dependency** — they'll just need to
be moved out later. Same goes for new pure-OS files: keep them
co-located with the existing macOS files here for now, but mark them
clearly with the OS name in the filename (e.g. `_macos`, `bug_*_macos`).

## See also

- `src/x86_64/README.md` — equivalent target bundle for x86_64+Linux
- `docs/todo.md` — "Target architecture refactor" entry tracking the split
