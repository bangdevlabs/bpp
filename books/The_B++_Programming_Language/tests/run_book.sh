#!/bin/sh
# run_book.sh — conformance gate for "The B++ Programming Language".
#
# The book is self-contained under books/The_B++_Programming_Language/.
# This compiles and runs every in-text example (../examples/chNN/*.bpp) and
# every exercise solution (./chNN/*.bpp). A listing that fails to compile,
# emits a warning, or exits non-zero (an exercise's assert tripping) fails the
# gate. The book is only as trustworthy as the code that compiles from it.
#
# Usage (from anywhere):
#   sh books/The_B++_Programming_Language/tests/run_book.sh
#
# Exit codes: 0 = every listing compiled clean and ran to a clean exit.
#             1 = at least one listing failed.

set -u

export BPP_BUILD_ID=00000000000000000000000000000000

# Locate ourselves: the script lives in <book>/tests, the book root is one up,
# and the compiler `bpp` sits at the repo root, three levels above this script.
TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
BOOK_DIR="$(cd "$TESTS_DIR/.." && pwd)"
REPO_ROOT="$(cd "$TESTS_DIR/../../.." && pwd)"
BPP="$REPO_ROOT/bpp"
BUILD_DIR="/tmp/bpp_book_run"

if [ ! -x "$BPP" ]; then
    echo "FATAL: $BPP not found or not executable. Run install.sh first."
    exit 2
fi
mkdir -p "$BUILD_DIR"

# A line of representative input for the stdin-reading listings. Non-stdin
# programs simply ignore it.
SAMPLE="$BUILD_DIR/sample.txt"
printf 'hello world\nthe quick brown fox\n123 45 6\na longer line than all the others over here\n' > "$SAMPLE"

pass=0
fail=0

# run_one <label> <source.bpp>
run_one() {
    label="$1"
    src="$2"
    out="$BUILD_DIR/$(echo "$label" | tr '/' '_')"

    warn="$("$BPP" "$src" -o "$out" 2>&1)"
    if [ $? -ne 0 ]; then
        echo "FAIL  $label   (compile error)"
        echo "$warn" | sed 's/^/        /' | head -6
        fail=$((fail + 1))
        return
    fi
    if echo "$warn" | grep -q "warning"; then
        echo "FAIL  $label   (warning emitted)"
        echo "$warn" | sed 's/^/        /' | head -6
        fail=$((fail + 1))
        return
    fi

    "$out" < "$SAMPLE" > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "FAIL  $label   (non-zero exit / assert tripped)"
        fail=$((fail + 1))
        return
    fi

    echo "ok    $label"
    pass=$((pass + 1))
}

echo "── examples ───────────────────────────────────────────"
for f in "$BOOK_DIR"/examples/ch*/*.bpp; do
    [ -e "$f" ] || continue
    run_one "example/$(basename "$(dirname "$f")")/$(basename "$f")" "$f"
done

echo "── exercises ──────────────────────────────────────────"
for f in "$TESTS_DIR"/ch*/*.bpp; do
    [ -e "$f" ] || continue
    run_one "exercise/$(basename "$(dirname "$f")")/$(basename "$f")" "$f"
done

echo "───────────────────────────────────────────────────────"
echo "book: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
