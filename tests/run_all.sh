#!/bin/sh
# tests/run_all.sh — Compile and run every tests/test_*.bpp and report a summary.
#
# Usage (from the repo root):
#   sh tests/run_all.sh
#
# Exit codes:
#   0  every test compiled and exited 0
#   1  one or more tests failed
#
# This is the regression gate for every commit that touches src/. Run it
# after each compiler change. If it ever turns red, do not push the change —
# diagnose the regression first.
#
# Tests are skipped (not failed) when they are not relevant to the current
# host: linux_*/x11_* on macOS, macho_*/codegen_macho on Linux, and any GPU
# or X11 test when there is no display available. Skips are reported in the
# summary so they remain visible.

set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BPP="$REPO_ROOT/bpp"
TESTS_DIR="$REPO_ROOT/tests"
BUILD_DIR="/tmp/bpp_run_all"

if [ ! -x "$BPP" ]; then
    echo "FATAL: $BPP not found or not executable. Run install.sh first."
    exit 2
fi

mkdir -p "$BUILD_DIR"

OS="$(uname -s)"
HAS_DISPLAY=0
if [ "$OS" = "Darwin" ]; then
    # macOS always has a window server available to a logged-in user.
    HAS_DISPLAY=1
elif [ -n "${DISPLAY:-}" ]; then
    HAS_DISPLAY=1
fi

# Returns "skip:<reason>" if the test should not run on this host, empty otherwise.
should_skip() {
    name="$1"
    case "$name" in
        test_linux_*|test_x11_*)
            [ "$OS" != "Linux" ] && echo "skip:not-linux" && return
            [ "$HAS_DISPLAY" -eq 0 ] && echo "skip:no-display" && return
            ;;
        test_macho_*|test_codegen_macho|test_enc_macho)
            [ "$OS" != "Darwin" ] && echo "skip:not-macos" && return
            ;;
        test_thread|test_job|test_maestro)
            # pthread FFI is macOS-only in Phase 1 of the maestro plan.
            # Linux inherits these tests when ELF dynamic linking lands.
            [ "$OS" != "Darwin" ] && echo "skip:phase1-macos-only" && return
            ;;
        test_gpu_*|test_stbgame_native|test_gameinfra)
            # GPU + window smoke tests. They open a real window and run
            # for a fixed number of frames before auto-exiting (30 frames
            # ≈ 0.5 s). On a host with no display the runner skips them.
            [ "$HAS_DISPLAY" -eq 0 ] && echo "skip:no-display" && return
            ;;
        test_native_debug)
            # Debugger smoke test wants to attach to a child process.
            # Treat as skip in batch mode to avoid interactive hangs.
            echo "skip:interactive"
            return
            ;;
    esac
    echo ""
}

PASS=0
FAIL=0
SKIP=0
FAIL_NAMES=""

for src in "$TESTS_DIR"/test_*.bpp; do
    [ -f "$src" ] || continue
    base="$(basename "$src" .bpp)"
    out="$BUILD_DIR/$base"

    skip="$(should_skip "$base")"
    if [ -n "$skip" ]; then
        printf "  SKIP  %-32s (%s)\n" "$base" "${skip#skip:}"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Compile. Bury normal output, surface compile failures only.
    if ! "$BPP" "$src" -o "$out" >/dev/null 2>"$BUILD_DIR/$base.compile.err"; then
        printf "  FAIL  %-32s (compile)\n" "$base"
        sed 's/^/        /' "$BUILD_DIR/$base.compile.err"
        FAIL=$((FAIL + 1))
        FAIL_NAMES="$FAIL_NAMES $base"
        continue
    fi

    # Run with a wall-clock guard. Tests should be quick; anything that
    # hangs more than 10 seconds is a failure.
    if [ "$OS" = "Darwin" ]; then
        # macOS has no GNU timeout. Use a background+kill pattern.
        ( "$out" >/dev/null 2>"$BUILD_DIR/$base.run.err" ) &
        pid=$!
        ( sleep 10 && kill -9 $pid 2>/dev/null ) &
        watcher=$!
        wait $pid 2>/dev/null
        rc=$?
        kill -9 $watcher 2>/dev/null
        wait $watcher 2>/dev/null
    else
        timeout 10 "$out" >/dev/null 2>"$BUILD_DIR/$base.run.err"
        rc=$?
    fi

    if [ "$rc" -eq 0 ]; then
        printf "  PASS  %s\n" "$base"
        PASS=$((PASS + 1))
    else
        printf "  FAIL  %-32s (exit %d)\n" "$base" "$rc"
        sed 's/^/        /' "$BUILD_DIR/$base.run.err"
        FAIL=$((FAIL + 1))
        FAIL_NAMES="$FAIL_NAMES $base"
    fi
done

TOTAL=$((PASS + FAIL))
echo ""
echo "Summary: $PASS passed, $FAIL failed, $SKIP skipped (out of $((TOTAL + SKIP)) total)"

if [ "$FAIL" -gt 0 ]; then
    echo "Failed tests:$FAIL_NAMES"
    exit 1
fi
exit 0
