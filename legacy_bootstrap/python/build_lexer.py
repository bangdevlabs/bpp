#!/usr/bin/env python3
"""Build and test the B++ bootstrap lexer."""

import subprocess, sys, json, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
COMPILER = os.path.join(ROOT_DIR, "bpp_compiler.py")
SOURCE = os.path.join(SCRIPT_DIR, "lexer.bpp")
SHIM_C = os.path.join(SCRIPT_DIR, "lexer_shim.c")
OUTPUT = os.path.join(SCRIPT_DIR, "bpp_lexer")

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

    if t == "mem_load":
        ptr = translate_node(node.get("ptr"))
        return f"(*(long long*)((uintptr_t)({ptr})))"

    if t == "mem_store":
        ptr = translate_node(node.get("ptr"))
        val = translate_node(node.get("val"))
        return f"*(long long*)((uintptr_t)({ptr})) = {val}"

    # Unary operator (e.g., negation).
    if t == "unary_op":
        return f"({node['op']}{translate_node(node['right'])})"

    # Binary operator: pure arithmetic, no memory wrapping.
    if t == "binop":
        left = translate_node(node['left'])
        right = translate_node(node['right'])
        op = node["op"]
        return f"({left} {op} {right})"

    # Plain variable assignment.
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
            return f"(*(uint8_t*)((uintptr_t)({args[0]})) = (uint8_t)({args[1]}))"

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
    """Generate a C source file from the B++ IR."""
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

        # Forward-declare all B++ functions so call order doesn't matter.
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
    """Compile lexer.bpp to a native executable."""
    print(f"{CYAN}🔧 B++ Bootstrap Lexer Builder{RESET}")

    result = subprocess.run(
        [sys.executable, COMPILER, SOURCE],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(f"{RED}Compiler Error:\n{result.stderr}{RESET}")
        return False

    ir = json.loads(result.stdout)
    generate_c(ir)

    cmd = ["clang", SHIM_C, "-o", OUTPUT, "-lm", "-w"]
    if subprocess.run(cmd).returncode != 0:
        print(f"{RED}Clang compilation failed.{RESET}")
        return False

    print(f"{GREEN}Lexer built: {OUTPUT}{RESET}")
    return True


def test(input_file=None):
    """Run the lexer on a test input and display the tokens."""
    if not os.path.exists(OUTPUT):
        if not build():
            return

    if input_file:
        print(f"\n{CYAN}Tokenizing: {input_file}{RESET}\n")
        with open(input_file) as f:
            result = subprocess.run(
                [OUTPUT], stdin=f, capture_output=True, text=True)
    else:
        test_code = (
            '// test\n'
            'auto x;\n'
            'main() {\n'
            '    x = 42;\n'
            '    if (x > 0) {\n'
            '        *(x + 8) = 0xFF;\n'
            '    } else {\n'
            '        x = 0;\n'
            '    }\n'
            '}\n'
        )
        print(f"\n{CYAN}Tokenizing inline test:{RESET}\n{test_code}")
        result = subprocess.run(
            [OUTPUT], input=test_code, capture_output=True, text=True)

    print(result.stdout)
    if result.stderr:
        print(f"{RED}{result.stderr}{RESET}")


if __name__ == "__main__":
    if build():
        input_file = sys.argv[1] if len(sys.argv) > 1 else None
        test(input_file)
