#!/bin/sh
# install.sh — Install B++ compiler, standard library, and FFI bindings.
# Usage: ./install.sh          (bootstrap + install)
#        ./install.sh --skip   (install without bootstrap)
#
# Run this script directly from a terminal, not from Claude Code or other sandboxed environments.

set -e

PREFIX="/usr/local"
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/lib/bpp"
STB_DIR="$LIB_DIR/stb"
FFI_DIR="$LIB_DIR/ffi"

# Backend layout mirrors src/backend/ since the 0.23.x reorg:
#   chip/<arch>     — instruction encoders and per-arch codegen
#   os/<os>         — syscall tables and OS runtime modules (auto-injected)
#   target/<a>_<o>  — linker / object-file writer for each chip+OS pair
#   c               — C transpiler (used by `bpp --c`)
# The compiler's find_file helper consults these install-side paths
# whenever a program imports one of these modules, so the tree layout
# here must match bpp_import.bsm exactly.
BACKEND_DIR="$LIB_DIR/backend"
CHIP_ARM64_DIR="$BACKEND_DIR/chip/aarch64"
CHIP_X64_DIR="$BACKEND_DIR/chip/x86_64"
OS_MACOS_DIR="$BACKEND_DIR/os/macos"
OS_LINUX_DIR="$BACKEND_DIR/os/linux"
TARGET_MAC_DIR="$BACKEND_DIR/target/aarch64_macos"
TARGET_LIN_DIR="$BACKEND_DIR/target/x86_64_linux"
BACKEND_C_DIR="$BACKEND_DIR/c"

# Bootstrap unless --skip is passed.
if [ "$1" != "--skip" ]; then
    echo "==> Bootstrapping compiler..."
    ./bpp src/bpp.bpp -o /tmp/bpp_install
    echo "    Bootstrap complete."
else
    echo "==> Compiling fresh binary for install..."
    ./bpp src/bpp.bpp -o /tmp/bpp_install
fi

echo "==> Compiling debugger..."
./bpp tools/the_bug/the_bug.bpp -o /tmp/bug_install

echo "==> Installing to $PREFIX (requires sudo)..."

# Create directories.
sudo mkdir -p "$BIN_DIR" "$LIB_DIR" "$STB_DIR" "$FFI_DIR"
sudo mkdir -p "$CHIP_ARM64_DIR" "$CHIP_X64_DIR"
sudo mkdir -p "$OS_MACOS_DIR" "$OS_LINUX_DIR"
sudo mkdir -p "$TARGET_MAC_DIR" "$TARGET_LIN_DIR" "$BACKEND_C_DIR"

# Wipe pre-0.23.x install dirs. Before the backend reorg the per-chip
# modules lived at $LIB_DIR/aarch64 and $LIB_DIR/x86_64. Any stale .bsm
# left there would shadow the new backend/... locations via find_file's
# lookup order, so we clear them before copying fresh content below.
sudo rm -rf "$LIB_DIR/aarch64" "$LIB_DIR/x86_64"

# Install compiler binary. Remove old file first so macOS sees a fresh inode
# and validates the embedded code signature from scratch (no stale kernel cache).
sudo rm -f "$BIN_DIR/bpp"
sudo cp /tmp/bpp_install "$BIN_DIR/bpp"
sudo chmod 755 "$BIN_DIR/bpp"
rm -f /tmp/bpp_install

# Install debugger binary.
sudo rm -f "$BIN_DIR/bug"
sudo cp /tmp/bug_install "$BIN_DIR/bug"
sudo chmod 755 "$BIN_DIR/bug"
rm -f /tmp/bug_install

# Clear stale .bsm files from every install dir before copying. Without
# this, files that move between directories across releases (e.g. the
# platform layer migrating from stb/ to aarch64/ in 0.21, or the runtime
# modules migrating from stb/ to src/ in the smart-dispatch work) leave
# behind ghost copies that find_file can pick up depending on cwd. The
# install dirs are managed exclusively by install.sh, so wiping their
# .bsm files is safe — anything legitimate gets re-copied below.
sudo rm -f "$LIB_DIR"/*.bsm
sudo rm -f "$STB_DIR"/*.bsm
sudo rm -f "$FFI_DIR"/*.bsm
sudo rm -f "$CHIP_ARM64_DIR"/*.bsm
sudo rm -f "$CHIP_X64_DIR"/*.bsm
sudo rm -f "$OS_MACOS_DIR"/*.bsm
sudo rm -f "$OS_LINUX_DIR"/*.bsm
sudo rm -f "$TARGET_MAC_DIR"/*.bsm
sudo rm -f "$TARGET_LIN_DIR"/*.bsm
sudo rm -f "$BACKEND_C_DIR"/*.bsm

# Install the runtime modules that every user program either imports
# explicitly or receives via auto-injection. bpp_defs is the constants
# table injected at codegen time. The rest are high-level APIs:
#   bpp_array   — dynamic arrays with shadow header
#   bpp_buf     — raw buffer access for multi-byte reads/writes
#   bpp_str     — null-terminated and dynamic string utilities
#   bpp_hash    — open-addressing hash maps (Hash + HashStr flavors)
#   bpp_io      — putchar/getchar and basic I/O
#   bpp_math    — numeric helpers and math primitives
#   bpp_file    — file read/write shims over the OS syscalls
#   bpp_json    — JSON reader/writer (explicit import, not auto-injected)
#   bpp_beat    — universal monotonic clock
#   bpp_job     — N×SPSC worker pool
#   bpp_maestro — solo / base / render bandleader
#   bpp_arena   — bump allocator (auto-injected since 0.75)
#   bpp_path    — argv[0]-relative asset resolution (auto-injected since 0.75)
#   bpp_mem     — portable malloc/free/realloc/memcpy (auto-injected;
#                 split out of _core_<os>.bsm by the bpp_mem refactor)
#   bpp_time    — portable monotonic clock helpers (auto-injected)
#   bpp_thread  — portable worker-thread API (auto-injected)
# Auto-injected entries are pulled in by bpp_import.bsm for every user
# main() program — missing one breaks every compile with a confusing
# "malloc never defined" cascade. Explicit-import entries ship here so
# `import "bpp_json.bsm"` resolves from a standalone project directory.
# When a new user-facing src/ module ships, add it to this list at the
# same time.
sudo cp src/bpp_defs.bsm "$LIB_DIR/"
sudo cp src/bpp_array.bsm "$LIB_DIR/"
sudo cp src/bpp_buf.bsm "$LIB_DIR/"
sudo cp src/bpp_str.bsm "$LIB_DIR/"
sudo cp src/bpp_hash.bsm "$LIB_DIR/"
sudo cp src/bpp_io.bsm "$LIB_DIR/"
sudo cp src/bpp_math.bsm "$LIB_DIR/"
sudo cp src/bpp_file.bsm "$LIB_DIR/"
sudo cp src/bpp_json.bsm "$LIB_DIR/"
sudo cp src/bpp_beat.bsm "$LIB_DIR/"
sudo cp src/bpp_job.bsm "$LIB_DIR/"
sudo cp src/bpp_maestro.bsm "$LIB_DIR/"
sudo cp src/bpp_arena.bsm "$LIB_DIR/"
sudo cp src/bpp_path.bsm "$LIB_DIR/"
sudo cp src/bpp_mem.bsm "$LIB_DIR/"
sudo cp src/bpp_time.bsm "$LIB_DIR/"
sudo cp src/bpp_thread.bsm "$LIB_DIR/"
# bpp_runtime.bsm holds the runtime PC resolver (`_runtime_resolve_pc`)
# that reads the embedded minisym blob the binary writers ship in
# every native binary. Auto-injected by bpp_import.bsm into every
# user program — missing it here breaks compiles from a clean cwd
# with `error[E201]: function '_runtime_resolve_pc' called but never
# defined`.
sudo cp src/bpp_runtime.bsm "$LIB_DIR/"
# bpp_minisym.bsm is the COMPILE-TIME emitter (writes the MSYM blob
# into Mach-O __minisym / ELF BPPMINI). Imported by the binary
# writers in src/backend/target/<*>/. Shipped here so a fresh
# install can re-bootstrap bpp from source without the project tree.
sudo cp src/bpp_minisym.bsm "$LIB_DIR/"
sudo cp src/bpp_codegen.bsm "$LIB_DIR/"
sudo cp src/bug_reader.bsm "$LIB_DIR/"
# The remaining bug_*.bsm modules complete the standalone debugger
# build chain — without these, compiling tools/the_bug/the_bug.bpp
# from any cwd outside the project root fails with E002 because
# the local search path (./, stb/) does not see src/.
# bug_shared.bsm holds the cross-thread snapshot globals (status,
# pc, watch list, viz buffers) consumed by both the_bug.bpp and
# bug_runviz.bsm; it is imported explicitly by the_bug, so it must
# live next to the other bug_*.bsm modules in $LIB_DIR.
sudo cp src/bug_gdb.bsm "$LIB_DIR/"
sudo cp src/bug_viz.bsm "$LIB_DIR/"
sudo cp src/bug_eval.bsm "$LIB_DIR/"
sudo cp src/bug_brk.bsm "$LIB_DIR/"
sudo cp src/bug_runviz.bsm "$LIB_DIR/"
sudo cp src/bug_shared.bsm "$LIB_DIR/"
sudo cp src/bug_tui.bsm "$LIB_DIR/"

# Install standard library.
sudo cp stb/*.bsm "$STB_DIR/"

# Install FFI bindings (raylib, SDL2 backends — proof B++ FFI works).
sudo cp ffi/*.bsm "$FFI_DIR/"

# Install the four-layer backend. Matches the src/backend/ reorg from
# the 0.23.x cache-removal sprint and the find_file lookup list in
# bpp_import.bsm. On macOS the OS layer's _core, _stb_platform,
# _stb_audio, and bug_observe files are auto-injected whenever a
# user program pulls stbgame, so the OS subdirectories must ship
# even if the user never cross-compiles. The chip/ and target/
# trees are compiler internals — installed for completeness so an
# installed bpp can be re-bootstrapped from this tree without the
# source checkout.
sudo cp src/backend/chip/aarch64/*.bsm "$CHIP_ARM64_DIR/"
sudo cp src/backend/chip/x86_64/*.bsm "$CHIP_X64_DIR/"
sudo cp src/backend/os/macos/*.bsm "$OS_MACOS_DIR/"
sudo cp src/backend/os/linux/*.bsm "$OS_LINUX_DIR/"
sudo cp src/backend/target/aarch64_macos/*.bsm "$TARGET_MAC_DIR/"
sudo cp src/backend/target/x86_64_linux/*.bsm "$TARGET_LIN_DIR/"
sudo cp src/backend/c/*.bsm "$BACKEND_C_DIR/"

# Clear module cache so stale .bo files don't shadow the new modules.
echo "==> Clearing module cache..."
"$BIN_DIR/bpp" --clean-cache 2>/dev/null || rm -rf "$HOME/.bpp/cache"/*.bo 2>/dev/null || true

# Sanity check the install. Compiles and runs a tiny program that
# exercises malloc/free through bpp_array — the path that breaks first
# when an auto-injected module (bpp_mem, bpp_time, bpp_thread) was
# forgotten in the cp list above. Failing here is much friendlier than
# failing later when a user tries to compile their own program.
echo "==> Running install smoke test..."
SMOKE_SRC="/tmp/bpp_install_smoke.bpp"
SMOKE_BIN="/tmp/bpp_install_smoke"
cat > "$SMOKE_SRC" <<'EOF'
main() {
    auto a;
    a = arr_new();
    arr_push(a, 42);
    if (arr_get(a, 0) != 42) { return 1; }
    arr_free(a);
    put("install smoke: ok\n");
    return 0;
}
EOF
if "$BIN_DIR/bpp" "$SMOKE_SRC" -o "$SMOKE_BIN" >/dev/null 2>&1 && "$SMOKE_BIN" >/dev/null; then
    echo "    Smoke test passed."
else
    echo "    *** SMOKE TEST FAILED ***"
    echo "    Re-running with full output so the failing module is visible:"
    "$BIN_DIR/bpp" "$SMOKE_SRC" -o "$SMOKE_BIN" 2>&1 | head -20
    rm -f "$SMOKE_SRC" "$SMOKE_BIN"
    exit 1
fi
rm -f "$SMOKE_SRC" "$SMOKE_BIN"

# Second sanity check: every bug_*.bsm and every bpp_*.bsm that
# tools/the_bug/the_bug.bpp imports must be present in $LIB_DIR.
# This catches forgotten cp lines above before a user ever tries
# to recompile the debugger from a clean cwd. We list the files
# explicitly so a typo here is loud (rather than checking shell
# globs that could quietly match nothing).
echo "==> Verifying debugger module install set..."
BUG_REQUIRED="bug_reader.bsm bug_shared.bsm bug_viz.bsm bug_eval.bsm \
              bug_brk.bsm bug_runviz.bsm bug_tui.bsm bug_gdb.bsm"
BUG_MISSING=""
for m in $BUG_REQUIRED; do
    if [ ! -f "$LIB_DIR/$m" ]; then
        BUG_MISSING="$BUG_MISSING $m"
    fi
done
if [ -z "$BUG_MISSING" ]; then
    echo "    All debugger modules present."
else
    echo "    *** MISSING DEBUGGER MODULES:$BUG_MISSING ***"
    echo "    Add the corresponding 'sudo cp src/<name>' line above."
    exit 1
fi

# Update local binaries too (if not sandboxed). Remove first for fresh inode.
rm -f ./bpp 2>/dev/null; cp "$BIN_DIR/bpp" ./bpp 2>/dev/null || true
rm -f ./bug 2>/dev/null; cp "$BIN_DIR/bug" ./bug 2>/dev/null || true

echo "==> Installed:"
echo "    $BIN_DIR/bpp (compiler)"
echo "    $BIN_DIR/bug (debugger)"
echo "    $LIB_DIR/ ($(ls $LIB_DIR/*.bsm 2>/dev/null | wc -l | tr -d ' ') runtime modules)"
echo "    $STB_DIR/ ($(ls stb/*.bsm | wc -l | tr -d ' ') stb modules)"
echo "    $FFI_DIR/ ($(ls ffi/*.bsm | wc -l | tr -d ' ') FFI bindings)"
echo "    $CHIP_ARM64_DIR/ ($(ls src/backend/chip/aarch64/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $CHIP_X64_DIR/ ($(ls src/backend/chip/x86_64/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $OS_MACOS_DIR/ ($(ls src/backend/os/macos/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $OS_LINUX_DIR/ ($(ls src/backend/os/linux/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $TARGET_MAC_DIR/ ($(ls src/backend/target/aarch64_macos/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $TARGET_LIN_DIR/ ($(ls src/backend/target/x86_64_linux/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $BACKEND_C_DIR/ ($(ls src/backend/c/*.bsm | wc -l | tr -d ' ') modules)"
echo "==> Done."
