#!/bin/sh
# install.sh — Install B++ compiler, standard library, and drivers.
# Usage: ./install.sh          (bootstrap + install)
#        ./install.sh --skip   (install without bootstrap)
#
# Run this script directly from a terminal, not from Claude Code or other sandboxed environments.

set -e

PREFIX="/usr/local"
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/lib/bpp"
STB_DIR="$LIB_DIR/stb"
DRV_DIR="$LIB_DIR/drivers"
ARM64_DIR="$LIB_DIR/aarch64"
X64_DIR="$LIB_DIR/x86_64"

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
./bpp src/bug.bpp -o /tmp/bug_install

echo "==> Installing to $PREFIX (requires sudo)..."

# Create directories.
sudo mkdir -p "$BIN_DIR" "$LIB_DIR" "$STB_DIR" "$DRV_DIR" "$ARM64_DIR" "$X64_DIR"

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
sudo rm -f "$DRV_DIR"/*.bsm
sudo rm -f "$ARM64_DIR"/*.bsm
sudo rm -f "$X64_DIR"/*.bsm

# Install compiler internal modules and runtime modules. bpp_defs.bsm is
# the constant table the compiler injects into every program. The other
# bpp_*.bsm modules in src/ are core runtime / generic utilities that
# live alongside the compiler internals because they are part of the
# language, not optional libraries:
#   bpp_array  — dynamic arrays with shadow header (used by compiler + games)
#   bpp_buf    — raw buffer access for multi-byte reads/writes
#   bpp_str    — null-terminated and dynamic string utilities
#   bpp_hash   — open-addressing hash maps (Hash + HashStr flavors)
#   bpp_beat   — universal monotonic clock
#   bpp_job    — N×SPSC worker pool
#   bpp_maestro — solo / base / render bandleader
# bpp_beat / bpp_job / bpp_maestro are auto-injected when stbgame is
# brought in. The others are imported explicitly by user programs that
# want generic utilities.
sudo cp src/bpp_defs.bsm "$LIB_DIR/"
sudo cp src/bpp_array.bsm "$LIB_DIR/"
sudo cp src/bpp_buf.bsm "$LIB_DIR/"
sudo cp src/bpp_str.bsm "$LIB_DIR/"
sudo cp src/bpp_hash.bsm "$LIB_DIR/"
sudo cp src/bpp_io.bsm "$LIB_DIR/"
sudo cp src/bpp_math.bsm "$LIB_DIR/"
sudo cp src/bpp_file.bsm "$LIB_DIR/"
sudo cp src/bpp_beat.bsm "$LIB_DIR/"
sudo cp src/bpp_job.bsm "$LIB_DIR/"
sudo cp src/bpp_maestro.bsm "$LIB_DIR/"

# Install standard library.
sudo cp stb/*.bsm "$STB_DIR/"

# Install drivers.
sudo cp drivers/*.bsm "$DRV_DIR/"

# Install per-target bundles. Each chip folder currently holds the
# codegen + platform layer + bug observer for the OS bound to that chip
# (aarch64 = macOS today, x86_64 = Linux today). When B++ supports a
# new chip+OS combination this layout will be revisited (see todo.md).
sudo cp src/aarch64/*.bsm "$ARM64_DIR/"
sudo cp src/x86_64/*.bsm "$X64_DIR/"

# Clear module cache so stale .bo files don't shadow the new modules.
echo "==> Clearing module cache..."
"$BIN_DIR/bpp" --clean-cache 2>/dev/null || rm -rf "$HOME/.bpp/cache"/*.bo 2>/dev/null || true

# Update local binaries too (if not sandboxed). Remove first for fresh inode.
rm -f ./bpp 2>/dev/null; cp "$BIN_DIR/bpp" ./bpp 2>/dev/null || true
rm -f ./bug 2>/dev/null; cp "$BIN_DIR/bug" ./bug 2>/dev/null || true

echo "==> Installed:"
echo "    $BIN_DIR/bpp (compiler)"
echo "    $BIN_DIR/bug (debugger)"
echo "    $LIB_DIR/defs.bsm"
echo "    $STB_DIR/ ($(ls stb/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $DRV_DIR/ ($(ls drivers/*.bsm | wc -l | tr -d ' ') drivers)"
echo "    $ARM64_DIR/ ($(ls src/aarch64/*.bsm | wc -l | tr -d ' ') modules)"
echo "    $X64_DIR/ ($(ls src/x86_64/*.bsm | wc -l | tr -d ' ') modules)"
echo "==> Done."
