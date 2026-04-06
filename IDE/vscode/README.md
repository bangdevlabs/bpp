# B++ for VS Code

Syntax highlighting for `.bpp` and `.bsm` files in Visual Studio Code.

## What it highlights

- Keywords: `if`, `else`, `while`, `for`, `return`, `break`, `continue`, `auto`, `struct`, `enum`, `const`, `extrn`, `sizeof`
- Built-ins: `malloc`, `free`, `realloc`, `peek`, `poke`, `memcpy`, `putchar`, `getchar`, `fn_ptr`, `call`, `assert`, `shr`, all `sys_*` syscalls
- Type hints: `: byte`, `: half`, `: word`, `: float`, `: ptr`, `: long`, `: half float`, etc.
- Constants: compiler internals (`T_*`, `TK_*`, `TY_*`), input (`KEY_*`, `MOUSE_*`), colors (`BLACK`, `RED`, ...)
- Strings, char literals, numbers (decimal, hex, float)
- Line comments (`//`)
- Function definitions and calls
- Operators

## Install (local, no marketplace)

Copy the whole `vscode` folder into your VS Code extensions directory:

```bash
# macOS / Linux
cp -r IDE/vscode ~/.vscode/extensions/bpp-0.1.0

# Then restart VS Code
```

After restart, open any `.bpp` or `.bsm` file and you should see highlighting. If not, check the bottom-right language selector — should say "B++".

## Files

- `package.json` — extension manifest, language registration
- `language-configuration.json` — comment style, brackets, auto-close
- `syntaxes/bpp.tmLanguage.json` — TextMate grammar (regex rules for colors)

## Editing the grammar

The grammar is a TextMate JSON file. To add new keywords/builtins, edit
`syntaxes/bpp.tmLanguage.json` and reload VS Code (`Cmd+Shift+P` →
"Developer: Reload Window").

Scope names follow TextMate conventions — see the VS Code theme colors
your editor maps each scope to.
