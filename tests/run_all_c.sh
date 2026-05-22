#!/bin/sh
# tests/run_all_c.sh — Compile and run every tests/test_*.bpp through the
# C backend, then report a summary.
#
# Usage (from the repo root):
#   sh tests/run_all_c.sh
#
# Exit codes:
#   0  every test went B++ -> C -> binary -> ran with exit 0
#   1  one or more tests failed at any of those three steps
#
# Each test is independent, so the runner farms them out across six
# worker processes via xargs -P. With a typical macOS machine that drops
# total wall time from ~38 s (sequential) to ~6-8 s. Workers spawn the
# same script in --worker mode, do all three steps for one test, and
# append a single result line to a shared file the parent reads at the
# end.
#
# Skips:
#   * Tests that are not relevant on this host (linux_*/x11_* on macOS,
#     macho_*/codegen_macho on Linux).
#   * Tests that exercise capabilities the C emitter cannot reach yet:
#       - Cocoa/Metal via objc_msgSend varargs (gpu_*, stbgame_native,
#         gameinfra) — known limitation, use drv_raylib for portable C.
#       - SIMD vec_* builtins — deferred, no _mm_* intrinsic mapping yet.
#       - Threads/jobs/maestro — pthread FFI is macOS-only Phase 1.
#       - Backend-specific tests (test_macho_*, test_codegen_macho,
#         test_enc_macho) — they assert on Mach-O bytes, irrelevant via C.

set -u

# .bug emission is skipped for the --c path (no native offsets yet),
# but pinning the build_id keeps any future shared infrastructure
# byte-stable across runs.
export BPP_BUILD_ID=00000000000000000000000000000000

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BPP="$REPO_ROOT/bpp"
TESTS_DIR="$REPO_ROOT/tests"
BUILD_DIR="/tmp/bpp_run_all_c"
RESULTS="$BUILD_DIR/.results"
SELF="$REPO_ROOT/tests/run_all_c.sh"

# Pin cwd to the repo root so:
#   1. find_file() picks up `src/`, `stb/`, and `ffi/` relative to the
#      project (matters before `install.sh` has placed prelude modules
#      under `/usr/local/lib/bpp/`).
#   2. tests that load fixtures via relative paths
#      (e.g. `games/rhythm/assets/beat_map.txt`) resolve the same way as
#      under run_all.sh, which already pins cwd.
# Without this, running `tests/run_all_c.sh` from inside `tests/`
# silently failed asset_wav / beat_map / modulab_handdiff at the run
# stage and test_bpp_arr at the emit stage.
cd "$REPO_ROOT"

# Per-step time budgets in seconds. The cap exists to protect the host
# CPU when the C emitter or clang or the resulting binary hangs — without
# this, a single bad test pins a core for the whole run.
EMIT_BUDGET=30
GCC_BUDGET=30
RUN_BUDGET=5

# gcc/clang flags shared by every test compile.
#   -w                                       silence the wall of warnings
#                                            the C emitter naturally generates
#   -Wno-error=implicit-function-declaration Apple clang 15+ promoted this
#                                            warning to a hard error; the C
#                                            emitter occasionally references
#                                            libc helpers without a forward
#                                            decl, so demote it back to a
#                                            warning that -w then silences
#   -lobjc                                   the macOS prelude pulls in
#                                            _stb_platform_macos.bsm which
#                                            references objc_msgSend
CC_FLAGS="-w -Wno-error=implicit-function-declaration"
# -lm pulled in for the Excalibur Feature 2 cast builtins (floor /
# nearbyint). macOS links libm automatically via libSystem; Linux
# requires the explicit flag, so we always pass it for parity.
LD_FLAGS="-lobjc -lm"

if [ ! -x "$BPP" ]; then
    echo "FATAL: $BPP not found or not executable. Run install.sh first."
    exit 2
fi

mkdir -p "$BUILD_DIR"

OS="$(uname -s)"

# Run a command with a timeout. Returns the command's exit code, or 124
# if the timeout fired (mirrors GNU coreutils `timeout`). Used because
# macOS does not ship `timeout` by default and a hung child process here
# would otherwise pin a CPU core indefinitely.
run_timeout() {
    secs="$1"
    shift
    "$@" &
    cmd_pid=$!
    ( sleep "$secs"; kill -TERM "$cmd_pid" 2>/dev/null
      sleep 1;        kill -KILL "$cmd_pid" 2>/dev/null ) &
    watchdog=$!
    wait "$cmd_pid" 2>/dev/null
    rc=$?
    # The command finished first — cancel the watchdog so it does not
    # linger and kill an unrelated process that recycles the same PID.
    kill -KILL "$watchdog" 2>/dev/null
    wait "$watchdog" 2>/dev/null
    return $rc
}

# Worker mode: process a single test by name and exit. Spawned by xargs
# from the parent for each non-skipped test. Writes one result line to
# the shared results file: "PASS <name>" or "FAIL <name> <stage>".
if [ "${1:-}" = "--worker" ]; then
    name="$2"
    src="$TESTS_DIR/$name.bpp"
    cfile="$BUILD_DIR/$name.c"
    bin="$BUILD_DIR/$name"

    if ! run_timeout "$EMIT_BUDGET" "$BPP" --c "$src" > "$cfile" 2>/dev/null; then
        echo "FAIL $name emit" >> "$RESULTS"
        exit 0
    fi
    if ! run_timeout "$GCC_BUDGET" cc $CC_FLAGS "$cfile" -o "$bin" $LD_FLAGS 2>/dev/null; then
        echo "FAIL $name gcc" >> "$RESULTS"
        exit 0
    fi
    if ! run_timeout "$RUN_BUDGET" "$bin" > /dev/null 2>&1; then
        echo "FAIL $name run" >> "$RESULTS"
        exit 0
    fi
    echo "PASS $name" >> "$RESULTS"
    exit 0
fi

# --- parent mode below ---

# Returns "skip:<reason>" if the test should not run on this host or
# under the C backend, empty otherwise.
should_skip() {
    name="$1"
    case "$name" in
        test_linux_*|test_x11_*)
            [ "$OS" != "Linux" ] && echo "skip:not-linux" && return
            ;;
        test_macho_*|test_codegen_macho|test_enc_macho)
            echo "skip:backend-specific" && return
            ;;
        test_gpu_*|test_stbgame_native|test_gameinfra)
            echo "skip:cocoa-varargs" && return
            ;;
        test_native_debug)
            echo "skip:interactive" && return
            ;;
        test_panic)
            # Phase 6.3 panic — expected rc=134, not 0. The wrapper
            # tests/test_panic.sh covers it; skip from the rc=0 loop.
            echo "skip:shell-wrapped" && return
            ;;
        test_vec_align_stack)
            echo "skip:stack-alignment-checks-native-frame-layout" && return
            ;;
        test_thread|test_job|test_maestro)
            echo "skip:phase1-macos-only" && return
            ;;
    esac
    echo ""
}

# Reset the results file. Each worker appends one line during its run.
: > "$RESULTS"

# Two passes: the first classifies, prints SKIPs, and writes the queue
# of non-skipped test names to a file. The second feeds that file to
# xargs. Doing it in two passes keeps the SKIP printf lines from
# leaking into xargs's stdin and being treated as fake test names.
QUEUE="$BUILD_DIR/.queue"
: > "$QUEUE"

SKIP=0
for src in "$TESTS_DIR"/test_*.bpp; do
    name=$(basename "$src" .bpp)

    # xfail: tests that begin with `// xfail: <code>` are negative tests.
    # Their job is to verify the front end (parser, type checker, validator)
    # rejects invalid code with a specific diagnostic. The front end runs
    # the same regardless of the chosen backend, so the native runner
    # already covers these. Skip in the C runner — there is no C output
    # to compile when emit fails on purpose.
    if head -3 "$src" | grep -q '^// xfail'; then
        printf "  SKIP  %-40s (%s)\n" "$name" "xfail"
        SKIP=$((SKIP + 1))
        continue
    fi

    # skip-c: tests whose first three lines carry `// skip-c: <reason>`
    # are intentionally exempt from the C backend. Used when the test
    # asserts native-only behaviour (e.g. caller_pc relies on the
    # minisym section the C path does not emit, so the test would
    # always return non-zero in the C suite by design).
    if head -3 "$src" | grep -q '^// skip-c:'; then
        reason=$(head -3 "$src" | sed -n 's|^// skip-c: \(.*\)|\1|p' | head -1)
        printf "  SKIP  %-40s (%s)\n" "$name" "$reason"
        SKIP=$((SKIP + 1))
        continue
    fi

    skip=$(should_skip "$name")
    if [ -n "$skip" ]; then
        printf "  SKIP  %-40s (%s)\n" "$name" "${skip#skip:}"
        SKIP=$((SKIP + 1))
        continue
    fi
    echo "$name" >> "$QUEUE"
done

xargs -n 1 -P 6 sh "$SELF" --worker < "$QUEUE"

# Print PASS/FAIL lines in alphabetical order. Workers write in
# completion order, so we sort by the test-name column before display.
sort -k 2 "$RESULTS" | while IFS=' ' read -r status name rest; do
    case "$status" in
        PASS) printf "  PASS  %-40s\n" "$name" ;;
        FAIL) printf "  FAIL  %-40s (%s)\n" "$name" "$rest" ;;
    esac
done

PASS=$(grep -c '^PASS ' "$RESULTS")
FAIL=$(grep -c '^FAIL ' "$RESULTS")

echo ""
echo "Summary: $PASS passed, $FAIL failed, $SKIP skipped"

[ "$FAIL" -gt 0 ] && exit 1
exit 0
