#!/usr/bin/env python3
# build_game.py — Build any B++ game that uses the stb library and raylib.
# Usage: python3 build_game.py <source.bpp> [output_name]

import subprocess, os, sys, json

COMPILER_SCRIPT = "bpp_compiler.py"

GRN, RED_A, CYAN, RESET = "\033[92m", "\033[91m", "\033[96m", "\033[0m"

# Track which names are globals, locals, functions, and externs for the current scope.
_globals = set()
_locals = set()
_functions = set()
_extern_funcs = {}

def _g(name):
    """Prefix a variable with g_ if it is a B++ global (avoids raylib name collisions)."""
    if name in _locals:
        return name
    return f"g_{name}" if name in _globals else name

def translate_node(node):
    if not node or not isinstance(node, dict): return str(node)
    t = node.get("type")

    if t == "lit":
        val = str(node.get("val", "0"))
        if val.startswith('"'):
            return f"(long long){val}"
        return val
    if t == "var": return _g(str(node.get("name", "")))
    if t == "nop": return ""

    if t == "mem_load":
        ptr = translate_node(node.get("ptr"))
        return f"(*(long long*)(mem_base + ({ptr})))"

    if t == "mem_store":
        ptr = translate_node(node.get("ptr"))
        val = translate_node(node.get("val"))
        return f"*(long long*)(mem_base + ({ptr})) = {val}"

    if t == "unary_op":
        return f"({node['op']}{translate_node(node['right'])})"

    if t == "binop":
        left = translate_node(node['left'])
        right = translate_node(node['right'])
        op = "||" if node["op"] == "|" else node["op"]
        return f"({left} {op} {right})"

    if t == "assign":
        left = translate_node(node.get("left", {}))
        right = translate_node(node.get("right"))
        return f"{left} = {right}"

    if t == "return":
        return f"return {translate_node(node.get('val'))}"

    if t == "if":
        cond = translate_node(node.get("cond"))
        body = "\n    ".join([f"{translate_node(s)};" for s in node.get("body", [])])
        result = f"if ({cond}) {{\n    {body}\n  }}"
        if node.get("else_body"):
            eb = "\n    ".join([f"{translate_node(s)};" for s in node["else_body"]])
            result += f" else {{\n    {eb}\n  }}"
        return result

    if t == "while":
        cond = translate_node(node.get("cond"))
        body = "\n    ".join([f"{translate_node(s)};" for s in node.get("body", [])])
        return f"while ({cond}) {{\n    {body}\n  }}"

    if t == "call":
        name = node.get("name")
        args = [translate_node(a) for a in node.get("args", [])]

        if name in ("malloc", "MemAlloc"):
            return f"bpp_alloc({', '.join(args)})"

        # Built-in memory operations.
        if name == "peek" and args:
            return f"((long long)mem_base[({args[0]})])"
        if name == "poke" and len(args) >= 2:
            return f"(mem_base[({args[0]})] = (uint8_t)({args[1]}))"

        # Built-in I/O: putchar writes 1 byte to stdout via raw syscall.
        if name == "putchar" and args:
            return f"(mem_base[0] = (uint8_t)({args[0]}), bpp_sys_write(1, mem_base, 1))"

        # Built-in I/O: getchar reads 1 byte from stdin via raw syscall.
        if name == "getchar":
            return "(bpp_sys_read(0, mem_base + 1, 1) > 0 ? (long long)mem_base[1] : -1LL)"

        # Built-in: str_peek reads a byte from a string, handling both pointer types.
        if name == "str_peek" and len(args) >= 2:
            s, i = args[0], args[1]
            return f"(({s}) >= (1024*1024) ? (long long)((uint8_t*)((uintptr_t)({s})))[({i})] : (long long)mem_base[({s}) + ({i})])"

        # Apply type casts for extern functions based on their declared signatures.
        extern_info = _extern_funcs.get(name)
        if extern_info:
            raw_args = node.get("args", [])
            sig_args = extern_info.get("args", [])
            for i, atype in enumerate(sig_args):
                if i < len(args):
                    if atype == "char*":
                        args[i] = f"BPP_STR({args[i]})"
                    elif atype == "ptr":
                        args[i] = f"(void*){args[i]}"

            if extern_info.get("lib") == "raylib" and name in (
                "ClearBackground", "DrawRectangle", "DrawCircle", "DrawLine", "DrawText"
            ) and args:
                args[-1] = f"GetColor((uint32_t){args[-1]})"

            ret = extern_info.get("ret", "int")
            if ret == "ptr":
                return f"(long long){name}({', '.join(args)})"

            return f"{name}({', '.join(args)})"

        if name in _functions:
            cname = "main_bpp" if name == "main" else f"bpp_{name}"
            return f"{cname}({', '.join(args)})"

        return f"{name}({', '.join(args)})"
    return ""

def generate_c(ir, out_c):
    global _globals, _extern_funcs, _functions
    _globals = set(ir.get("globals", []))
    _extern_funcs = ir.get("extern_functions", {})
    _functions = set(ir.get("functions", {}).keys())

    with open(out_c, "w") as f:
        f.write("#include <raylib.h>\n#include <stdlib.h>\n#include <stdint.h>\n\n")
        f.write("uint8_t mem_base[1024*1024];\n")
        f.write("long long bpp_alloc(int size) { static int ptr=16; int r=ptr; ptr+=size; return (long long)r; }\n")
        f.write("#define BPP_STR(v) ((v) >= (1024*1024) ? (const char*)(uintptr_t)(v) : (const char*)(mem_base + (v)))\n\n")

        # Built-in I/O runtime — raw ARM64 syscalls.
        f.write("// B++ I/O runtime — raw syscalls.\n")
        f.write("static long long bpp_sys_write(long long fd, const void* buf, long long len) {\n")
        f.write('  register long long x0 __asm__("x0") = fd;\n')
        f.write('  register long long x1 __asm__("x1") = (long long)buf;\n')
        f.write('  register long long x2 __asm__("x2") = len;\n')
        f.write('  register long long x16 __asm__("x16") = 4;\n')
        f.write('  __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");\n')
        f.write("  return x0;\n}\n")
        f.write("static long long bpp_sys_read(long long fd, void* buf, long long len) {\n")
        f.write('  register long long x0 __asm__("x0") = fd;\n')
        f.write('  register long long x1 __asm__("x1") = (long long)buf;\n')
        f.write('  register long long x2 __asm__("x2") = len;\n')
        f.write('  register long long x16 __asm__("x16") = 3;\n')
        f.write('  __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16) : "memory", "cc");\n')
        f.write("  return x0;\n}\n\n")

        for g in ir.get("globals", []):
            f.write(f"long long g_{g} = 0;\n")

        for name, data in ir["functions"].items():
            global _locals
            fname = "main_bpp" if name == "main" else f"bpp_{name}"
            params = data["params"]
            _locals = set(params)

            for stmt in data["body"]:
                if stmt.get("type") == "decl":
                    _locals.update(stmt.get("vars", []))

            param_str = ", ".join([f"long long {p}" for p in params])
            f.write(f"\nlong long {fname}({param_str}) {{\n")

            for stmt in data["body"]:
                if stmt.get("type") == "decl":
                    f.write(f"  long long {', '.join(stmt.get('vars', []))};\n")
                else:
                    code = translate_node(stmt)
                    if code and code != "0":
                        f.write(f"  {code};\n")
            f.write("  return 0;\n}\n")

        f.write("\nint main() {\n  main_bpp(); \n  return 0;\n}\n")

def build(source, output=None):
    base = os.path.splitext(os.path.basename(source))[0]
    if not output:
        output = base
    out_c = f"{base}_shim.c"

    print(f"{CYAN}Building {source} → {output}{RESET}")

    result = subprocess.run([sys.executable, COMPILER_SCRIPT, source], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"{RED_A}Compiler Error:\n{result.stderr}{RESET}")
        return False

    ir_data = json.loads(result.stdout)
    generate_c(ir_data, out_c)

    compile_cmd = [
        "clang", out_c,
        "-I/opt/homebrew/include", "-L/opt/homebrew/lib", "-lraylib",
        "-framework", "CoreVideo", "-framework", "IOKit",
        "-framework", "Cocoa", "-framework", "OpenGL",
        "-w", "-o", output
    ]

    if subprocess.run(compile_cmd).returncode == 0:
        print(f"{GRN}Compiled! Running {output}...{RESET}")
        subprocess.run([f"./{output}"])
        return True
    else:
        print(f"{RED_A}Clang failed to compile.{RESET}")
        return False

if __name__ == "__main__":
    source = sys.argv[1] if len(sys.argv) > 1 else "ui_demo.bpp"
    output = sys.argv[2] if len(sys.argv) > 2 else None
    build(source, output)
