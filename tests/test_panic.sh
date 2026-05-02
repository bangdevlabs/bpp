#!/bin/sh
# Phase 6.3 regression — locks in:
#   1. panic("msg") writes "panic: msg\n" to stderr.
#   2. The FP-chain walker prints one `at <name>` line per frame,
#      including every link of the deep_a → deep_b → deep_c chain.
#   3. The process exits with rc=134 (128 + SIGABRT convention).
#
# Guards against the "wild FP segfault on chain end" class of bug
# the Phase 6.3 patches to caller_pc primitives prevent. If the
# null-FP guard regresses, this test catches a SIGSEGV (rc=139)
# instead of the expected 134, with the stack trace cut short.

set -e

export BPP_BUILD_ID=00000000000000000000000000000000

cd "$(dirname "$0")/.."

BPP=./bpp
SRC=tests/test_panic.bpp

if [ ! -x "$BPP" ]; then
    echo "FAIL: $BPP not built"
    exit 1
fi

TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

"$BPP" "$SRC" -o "$TMP/tp" >/dev/null

# Run the binary, capture stderr, and pin the exit code. `set -e` is
# disabled around the run because rc != 0 is the expected outcome.
set +e
ERR=$("$TMP/tp" 2>&1)
RC=$?
set -e

if [ "$RC" -ne 134 ]; then
    echo "FAIL: expected rc=134, got rc=$RC"
    echo "$ERR"
    exit 1
fi

echo "$ERR" | grep -q "^panic: oops from deep_c$" \
    || { echo "FAIL: panic message missing"; echo "$ERR"; exit 1; }
echo "$ERR" | grep -q "  at deep_c"  \
    || { echo "FAIL: deep_c frame missing"; echo "$ERR"; exit 1; }
echo "$ERR" | grep -q "  at deep_b"  \
    || { echo "FAIL: deep_b frame missing"; echo "$ERR"; exit 1; }
echo "$ERR" | grep -q "  at deep_a"  \
    || { echo "FAIL: deep_a frame missing"; echo "$ERR"; exit 1; }
echo "$ERR" | grep -q "  at main"    \
    || { echo "FAIL: main frame missing"; echo "$ERR"; exit 1; }

echo "PASS  test_panic"
