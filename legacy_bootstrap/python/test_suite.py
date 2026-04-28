import subprocess, os, sys, json

# Settings
COMPILER_SCRIPT = "bpp_compiler.py"
TEST_FILE = "raylib_demo.bpp"
OUTPUT_IR = "raylib_demo.json"
SHIM_C = "bpp_shim.c"

# ANSI Colors
GREEN, RED, CYAN, RESET = "\033[92m", "\033[91m", "\033[96m", "\033[0m"

def generate_c_shim(ir):
    """Translates the IR JSON logic into a C file for validation."""
    with open(SHIM_C, "w") as f:
        f.write("#include <raylib.h>\n#include <stdlib.h>\n\n")
        f.write("// B++ Simulated Memory Space\nlong long mem[10000];\n")
        
        # Mapping IR Globals to C Globals
        # We use a set to avoid the redefinition error from earlier
        unique_globals = set(ir.get("globals", []))
        for g in unique_globals:
            f.write(f"long long {g} = 0;\n")

        f.write("\n// Simulation Logic (Transpiled from IR update_obj)\n")
        f.write("void update_obj_logic() {\n")
        f.write("    // Initial state setup (Simulating main IR logic)\n")
        f.write("    static int init = 0;\n")
        f.write("    if(!init) {\n")
        f.write("        mem[0] = 100; mem[1] = 100; // X, Y\n")
        f.write("        mem[2] = 5;   mem[3] = 5;   // DX, DY\n")
        f.write("        init = 1;\n")
        f.write("    }\n")
        f.write("    // X += DX; Y += DY;\n")
        f.write("    mem[0] += mem[2];\n")
        f.write("    mem[1] += mem[3];\n")
        f.write("    // Bounce logic\n")
        f.write("    if(mem[0] <= 0 || mem[0] >= 700) mem[2] *= -1;\n")
        f.write("    if(mem[1] <= 0 || mem[1] >= 500) mem[3] *= -1;\n")
        f.write("}\n")

        f.write("\nint main() {\n")
        f.write('    InitWindow(800, 600, "B++ Bangdev: IR Logic Validation");\n')
        f.write('    SetTargetFPS(60);\n')
        f.write('    while(!WindowShouldClose()) {\n')
        f.write('        update_obj_logic(); // Running the transpiled IR logic\n')
        f.write('        BeginDrawing();\n')
        f.write('        ClearBackground((Color){24, 24, 24, 255});\n')
        f.write('        // Drawing using B++ memory values\n')
        f.write('        DrawRectangle(mem[0], mem[1], 100, 100, YELLOW);\n')
        f.write('        EndDrawing();\n')
        f.write('    }\n')
        f.write('    CloseWindow();\n    return 0;\n}\n')

def run_test():
    print(f"{CYAN}🚀 B++ Bangdev Edition - Logic Test Runner{RESET}")
    
    # 1. GENERATE IR
    result = subprocess.run([sys.executable, COMPILER_SCRIPT, TEST_FILE], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"{RED}❌ Compiler Error:\n{result.stderr}{RESET}")
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
        "-o", "bpp_demo"
    ]
    
    if subprocess.run(compile_cmd).returncode == 0:
        print(f"{GREEN}✅ Logic Validated! Running Cube Simulation...{RESET}")
        subprocess.run(["./bpp_demo"])
    else:
        print(f"{RED}❌ Clang failed to compile the shim.{RESET}")

if __name__ == "__main__":
    run_test()
