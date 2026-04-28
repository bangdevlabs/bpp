import subprocess, os, sys, json

# Settings
COMPILER_SCRIPT = "bpp_compiler.py"
TEST_FILE = "snake_v2.bpp"
OUTPUT_IR = "snake_v2.json"
SHIM_C = "bpp_shim_v2.c"

# ANSI Colors
GREEN, RED, CYAN, RESET = "\033[92m", "\033[91m", "\033[96m", "\033[0m"

def translate_node(node):
    if not node or not isinstance(node, dict): return str(node)
    t = node.get("type")

    if t == "lit": return str(node.get("val", "0"))
    if t == "var": return str(node.get("name", ""))
    if t == "nop": return ""

    # Dereference read: *(ptr) becomes a read from mem_base at the given offset.
    if t == "mem_load":
        ptr = translate_node(node.get("ptr"))
        return f"(*(long long*)(mem_base + ({ptr})))"

    # Dereference write: *(ptr) = val becomes a store into mem_base.
    if t == "mem_store":
        ptr = translate_node(node.get("ptr"))
        val = translate_node(node.get("val"))
        return f"*(long long*)(mem_base + ({ptr})) = {val}"

    # Unary operator (e.g., negation).
    if t == "unary_op":
        return f"({node['op']}{translate_node(node['right'])})"

    # Binary operator: pure arithmetic, no memory wrapping.
    if t == "binop":
        left = translate_node(node['left'])
        right = translate_node(node['right'])
        op = "||" if node["op"] == "|" else node["op"]
        return f"({left} {op} {right})"

    # Plain variable assignment (no dereference on the left side).
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

        # Map B++ malloc to our custom allocator in the simulated memory space.
        if name == "malloc":
            return f"bpp_alloc({', '.join(args)})"

        if name in ["ClearBackground", "DrawRectangle"] and args:
            # Raylib expects a Color struct; GetColor converts a uint32 hex to Color.
            args[-1] = f"GetColor((uint32_t){args[-1]})"
        return f"{name}({', '.join(args)})"
    return ""

def generate_c_shim(ir):
    with open(SHIM_C, "w") as f:
        f.write("#include <raylib.h>\n#include <stdlib.h>\n#include <stdint.h>\n#include <stdio.h>\n\n")
        f.write("uint8_t mem_base[1024*1024];\n")
        f.write("long long bpp_alloc(int size) { static int ptr=16; int r=ptr; ptr+=size; return (long long)r; }\n\n")

        for g in ir.get("globals", []):
            f.write(f"long long {g} = 0;\n")

        for name, data in ir["functions"].items():
            fname = "main_bpp" if name == "main" else name
            params = ", ".join([f"long long {p}" for p in data["params"]])
            f.write(f"\nlong long {fname}({params}) {{\n")

            for stmt in data["body"]:
                if stmt.get("type") == "decl":
                    f.write(f"  long long {', '.join(stmt.get('vars', []))};\n")
                else:
                    code = translate_node(stmt)
                    if code and code != "0":
                        f.write(f"  {code};\n")
            f.write("  return 0;\n}\n")

        f.write("\nint main() {\n  main_bpp(); \n  return 0;\n}\n")

def run_test():
    print(f"{CYAN}B++ Snake v2 - Fat Structs Edition{RESET}")

    # 1. GENERATE IR
    result = subprocess.run([sys.executable, COMPILER_SCRIPT, TEST_FILE], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"{RED}Compiler Error:\n{result.stderr}{RESET}")
        return

    ir_data = json.loads(result.stdout)
    with open(OUTPUT_IR, "w") as f: json.dump(ir_data, f, indent=4)

    # 2. GENERATE AND COMPILE SHIM
    generate_c_shim(ir_data)

    compile_cmd = [
        "clang", SHIM_C,
        "-I/opt/homebrew/include", "-L/opt/homebrew/lib", "-lraylib",
        "-framework", "CoreVideo", "-framework", "IOKit",
        "-framework", "Cocoa", "-framework", "OpenGL",
        "-o", "snake_v2"
    ]

    if subprocess.run(compile_cmd).returncode == 0:
        print(f"{GREEN}Compiled! Running Snake v2...{RESET}")
        subprocess.run(["./snake_v2"])
    else:
        print(f"{RED}Clang failed to compile the shim.{RESET}")

if __name__ == "__main__":
    run_test()
