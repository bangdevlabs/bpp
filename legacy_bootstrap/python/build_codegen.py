#!/usr/bin/env python3
"""Build and test the B++ ARM64 code generator (Stage 4)."""

import subprocess, sys, json, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
COMPILER = os.path.join(ROOT_DIR, "bpp_compiler.py")
SOURCE = os.path.join(SCRIPT_DIR, "codegen_arm64.bpp")
SHIM_C = os.path.join(SCRIPT_DIR, "codegen_shim.c")
OUTPUT = os.path.join(SCRIPT_DIR, "bpp_codegen")
LEXER = os.path.join(SCRIPT_DIR, "bpp_lexer")
PARSER = os.path.join(SCRIPT_DIR, "bpp_parser")

GREEN, RED, CYAN, RESET = "\033[92m", "\033[91m", "\033[96m", "\033[0m"


def translate_node(node):
    """Translate a single IR node to C code."""
    if not node or not isinstance(node, dict):
        return str(node)
    t = node.get("type")

    if t == "lit":
        val = str(node.get("val", "0"))
        if val.startswith('"'):
            return f"(long long){val}"
        return val
    if t == "var":
        return str(node.get("name", ""))
    if t == "nop":
        return ""
    if t == "decl":
        return f"long long {', '.join(node.get('vars', []))}"
    if t == "mem_load":
        ptr = translate_node(node.get("ptr"))
        return f"(*(long long*)((uintptr_t)({ptr})))"
    if t == "mem_store":
        ptr = translate_node(node.get("ptr"))
        val = translate_node(node.get("val"))
        return f"*(long long*)((uintptr_t)({ptr})) = {val}"
    if t == "unary_op":
        return f"({node['op']}{translate_node(node['right'])})"
    if t == "binop":
        left = translate_node(node['left'])
        right = translate_node(node['right'])
        op = node["op"]
        return f"({left} {op} {right})"
    if t == "assign":
        left = translate_node(node.get("left", {}))
        right = translate_node(node.get("right"))
        return f"{left} = {right}"
    if t == "return":
        return f"return {translate_node(node.get('val'))}"

    if t == "if":
        cond = translate_node(node.get("cond"))
        body = "\n    ".join(
            [f"{translate_node(s)};" for s in node.get("body", [])])
        result = f"if ({cond}) {{\n    {body}\n  }}"
        if node.get("else_body"):
            eb = "\n    ".join(
                [f"{translate_node(s)};" for s in node["else_body"]])
            result += f" else {{\n    {eb}\n  }}"
        return result

    if t == "while":
        cond = translate_node(node.get("cond"))
        body = "\n    ".join(
            [f"{translate_node(s)};" for s in node.get("body", [])])
        return f"while ({cond}) {{\n    {body}\n  }}"

    if t == "call":
        name = node.get("name")
        args = [translate_node(a) for a in node.get("args", [])]
        if name == "malloc":
            return f"(long long)malloc({', '.join(args)})"
        if name == "peek":
            return f"((long long)(*(uint8_t*)((uintptr_t)({args[0]}))))"
        if name == "poke":
            return (f"(*(uint8_t*)((uintptr_t)({args[0]}))"
                    f" = (uint8_t)({args[1]}))")
        if name == "putchar" and args:
            return f"(_bpp_scratch[0] = (uint8_t)({args[0]}), bpp_sys_write(1, _bpp_scratch, 1))"
        if name == "getchar":
            return "(_bpp_scratch[0] = 0, bpp_sys_read(0, _bpp_scratch, 1) > 0 ? (long long)_bpp_scratch[0] : -1LL)"
        if name == "str_peek" and len(args) >= 2:
            s, i = args[0], args[1]
            return f"(long long)((uint8_t*)((uintptr_t)({s})))[({i})]"
        # Everything else: pass through.
        return f"{name}({', '.join(args)})"

    return ""


def generate_c(ir):
    """Generate C source file from the codegen's IR."""
    with open(SHIM_C, "w") as f:
        f.write("#include <stdint.h>\n")
        f.write("#include <stdlib.h>\n\n")
        f.write("static uint8_t _bpp_scratch[16];\n\n")

        # Raw syscall runtime for I/O (ARM64 macOS).
        f.write("// B++ I/O runtime — raw ARM64 syscalls.\n")
        f.write("static long long bpp_sys_write(long long fd,"
                " const void* buf, long long len) {\n")
        f.write('  register long long x0 __asm__("x0") = fd;\n')
        f.write('  register long long x1 __asm__("x1") = (long long)buf;\n')
        f.write('  register long long x2 __asm__("x2") = len;\n')
        f.write('  register long long x16 __asm__("x16") = 4;\n')
        f.write('  __asm__ volatile("svc #0x80\\n\\tcsneg x0, x0, x0, cc"'
                ' : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)'
                ' : "memory", "cc");\n')
        f.write("  return x0;\n}\n")
        f.write("static long long bpp_sys_read(long long fd,"
                " void* buf, long long len) {\n")
        f.write('  register long long x0 __asm__("x0") = fd;\n')
        f.write('  register long long x1 __asm__("x1") = (long long)buf;\n')
        f.write('  register long long x2 __asm__("x2") = len;\n')
        f.write('  register long long x16 __asm__("x16") = 3;\n')
        f.write('  __asm__ volatile("svc #0x80\\n\\tcsneg x0, x0, x0, cc"'
                ' : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)'
                ' : "memory", "cc");\n')
        f.write("  return x0;\n}\n\n")

        for g in ir.get("globals", []):
            f.write(f"long long {g} = 0;\n")

        # Forward-declare all B++ functions.
        for name, data in ir["functions"].items():
            fname = "main_bpp" if name == "main" else name
            params = ", ".join([f"long long {p}" for p in data["params"]])
            f.write(f"long long {fname}({params});\n")
        f.write("\n")

        for name, data in ir["functions"].items():
            fname = "main_bpp" if name == "main" else name
            params = ", ".join([f"long long {p}" for p in data["params"]])
            f.write(f"long long {fname}({params}) {{\n")
            for stmt in data["body"]:
                if stmt.get("type") == "decl":
                    f.write(
                        f"  long long {', '.join(stmt.get('vars', []))};\n")
                else:
                    code = translate_node(stmt)
                    if code and code != "0":
                        f.write(f"  {code};\n")
            f.write("  return 0;\n}\n")

        f.write("\nint main() {\n  main_bpp();\n  return 0;\n}\n")


def build():
    """Compile codegen.bpp to a native executable."""
    print(f"{CYAN}B++ Bootstrap ARM64 Codegen Builder{RESET}")

    result = subprocess.run(
        [sys.executable, COMPILER, SOURCE],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(f"{RED}Compiler Error:\n{result.stderr}{RESET}")
        return False

    ir = json.loads(result.stdout)
    generate_c(ir)

    cmd = ["clang", SHIM_C, "-o", OUTPUT, "-lm",
           "-w", "-Wno-int-conversion"]
    if subprocess.run(cmd).returncode != 0:
        print(f"{RED}Clang compilation failed.{RESET}")
        return False

    print(f"{GREEN}Codegen built: {OUTPUT}{RESET}")
    return True


def test():
    """Test the codegen by compiling a hello world program."""
    if not os.path.exists(LEXER):
        print(f"{RED}Lexer not found. Run build_lexer.py first.{RESET}")
        return
    if not os.path.exists(PARSER):
        print(f"{RED}Parser not found. Run build_parser.py first.{RESET}")
        return
    if not os.path.exists(OUTPUT):
        if not build():
            return

    # Comprehensive test: variables, loops, functions, strings, arithmetic, memory.
    test_bpp = os.path.join(SCRIPT_DIR, "test_hello.bpp")
    with open(test_bpp, "w") as f:
        f.write("""auto gl;

print_str(s) {
    auto i, ch;
    i = 0; ch = str_peek(s, i);
    while (ch != 0) { putchar(ch); i = i + 1; ch = str_peek(s, i); }
    return 0;
}

add(a, b) { return a + b; }

main() {
    auto x, y, i;
    print_str("Hello B++");
    putchar(10);
    x = 3; y = 4;
    gl = add(x, y);
    i = 0;
    while (i < gl) { putchar(48 + i); i = i + 1; }
    putchar(10);
    if (gl == 7) { putchar(89); } else { putchar(78); }
    putchar(10);
    auto buf;
    buf = malloc(16);
    poke(buf, 65);
    poke(buf + 1, 66);
    putchar(peek(buf));
    putchar(peek(buf + 1));
    putchar(10);
    *(buf + 0) = 9999;
    if (*(buf + 0) == 9999) { putchar(89); } else { putchar(78); }
    putchar(10);
    return 0;
}
""")

    print(f"\n{CYAN}Testing codegen on: {test_bpp}{RESET}")

    # Pipeline: source | lexer | parser | codegen > assembly
    with open(test_bpp) as f:
        lex = subprocess.run([LEXER], stdin=f, capture_output=True)
    if lex.returncode != 0:
        print(f"{RED}Lexer failed.{RESET}")
        return

    parse = subprocess.run([PARSER], input=lex.stdout, capture_output=True)
    if parse.returncode != 0:
        print(f"{RED}Parser failed.{RESET}")
        return

    codegen = subprocess.run([OUTPUT], input=parse.stdout, capture_output=True)
    if codegen.returncode != 0:
        print(f"{RED}Codegen failed (exit {codegen.returncode}).{RESET}")
        if codegen.stderr:
            print(codegen.stderr.decode("utf-8", errors="replace"))
        return

    asm_code = codegen.stdout.decode("utf-8", errors="replace")
    print(f"{GREEN}Codegen produced {len(asm_code)} bytes of ARM64 assembly.{RESET}")

    # Write assembly to file.
    asm_file = os.path.join(SCRIPT_DIR, "test_hello.s")
    with open(asm_file, "w") as f:
        f.write(asm_code)
    print(f"Assembly written to: {asm_file}")

    # Show the assembly output for inspection.
    print(f"\n--- ARM64 Assembly ---")
    print(asm_code)

    # Try to assemble and link.
    test_bin = os.path.join(SCRIPT_DIR, "test_hello")
    cmd = ["clang", "-arch", "arm64", asm_file, "-o", test_bin,
           "-nostdlib", "-lSystem"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"{RED}Assembly/link failed:{RESET}")
        print(result.stderr)
        return

    print(f"{GREEN}Binary built: {test_bin}{RESET}")

    # Run the test binary.
    result = subprocess.run([test_bin], capture_output=True, text=True, timeout=5)
    print(f"Exit code: {result.returncode}")
    if result.stdout:
        print(f"Output: {repr(result.stdout)}")
    expected = "Hello B++\n0123456\nY\nAB\nY\n"
    if result.stdout == expected:
        print(f"{GREEN}SUCCESS: All tests passed!{RESET}")
    else:
        print(f"{RED}UNEXPECTED OUTPUT:{RESET}")
        print(f"  expected: {repr(expected)}")
        print(f"  got:      {repr(result.stdout)}")


if __name__ == "__main__":
    if build():
        test()
