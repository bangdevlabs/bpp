#!/usr/bin/env python3
"""Build and test the B++ bootstrap C emitter (Stage 3)."""

import subprocess, sys, json, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
COMPILER = os.path.join(ROOT_DIR, "bpp_compiler.py")
SOURCE = os.path.join(SCRIPT_DIR, "emitter.bpp")
SHIM_C = os.path.join(SCRIPT_DIR, "emitter_shim.c")
OUTPUT = os.path.join(SCRIPT_DIR, "bpp_emitter")
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
    """Generate C source file from the emitter's IR."""
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
    """Compile emitter.bpp to a native executable."""
    print(f"{CYAN}B++ Bootstrap Emitter Builder{RESET}")

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

    print(f"{GREEN}Emitter built: {OUTPUT}{RESET}")
    return True


def test(input_file=None):
    """Test the emitter by running the full bootstrap pipeline."""
    if not os.path.exists(LEXER):
        print(f"{RED}Lexer not found. Run build_lexer.py first.{RESET}")
        return
    if not os.path.exists(PARSER):
        print(f"{RED}Parser not found. Run build_parser.py first.{RESET}")
        return
    if not os.path.exists(OUTPUT):
        if not build():
            return

    target = input_file or os.path.join(SCRIPT_DIR, "lexer.bpp")
    print(f"\n{CYAN}Testing emitter on: {target}{RESET}")

    # Full pipeline: source | lexer | parser | emitter > C code
    with open(target) as f:
        lex = subprocess.run([LEXER], stdin=f, capture_output=True)
    if lex.returncode != 0:
        print(f"{RED}Lexer failed.{RESET}")
        return

    parse = subprocess.run([PARSER], input=lex.stdout, capture_output=True)
    if parse.returncode != 0:
        print(f"{RED}Parser failed.{RESET}")
        return

    emit = subprocess.run([OUTPUT], input=parse.stdout, capture_output=True)
    if emit.returncode != 0:
        print(f"{RED}Emitter failed.{RESET}")
        return

    c_code = emit.stdout.decode("utf-8", errors="replace")
    print(f"{GREEN}Emitter produced {len(c_code)} bytes of C code.{RESET}")

    # Compare with Python emitter output.
    ref = subprocess.run(
        [sys.executable, COMPILER, "--emit", target],
        capture_output=True, text=True)
    if ref.returncode != 0:
        print(f"{RED}Python emitter failed:\n{ref.stderr}{RESET}")
        print("\n--- B++ emitter output (first 80 lines) ---")
        for line in c_code.split("\n")[:80]:
            print(line)
        return

    ref_code = ref.stdout

    if c_code.strip() == ref_code.strip():
        print(f"{GREEN}MATCH: B++ emitter output matches Python emitter.{RESET}")
    else:
        # Show first difference.
        c_lines = c_code.split("\n")
        r_lines = ref_code.split("\n")
        for i, (a, b) in enumerate(zip(c_lines, r_lines)):
            if a != b:
                print(f"{RED}DIFF at line {i+1}:{RESET}")
                print(f"  B++:    {a}")
                print(f"  Python: {b}")
                break
        else:
            shorter = "B++" if len(c_lines) < len(r_lines) else "Python"
            print(f"{RED}Output lengths differ ({shorter} is shorter).{RESET}")
        print(f"\n--- B++ emitter output (first 40 lines) ---")
        for line in c_lines[:40]:
            print(line)
        print(f"\n--- Python emitter output (first 40 lines) ---")
        for line in r_lines[:40]:
            print(line)


if __name__ == "__main__":
    if build():
        input_file = sys.argv[1] if len(sys.argv) > 1 else None
        test(input_file)
