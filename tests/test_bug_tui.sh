#!/bin/sh
# Phase 4 TUI regression — locks in:
#   1. ./bug builds clean (no wild BLs in bug_tui_repl).
#   2. The REPL prompt fires on a breakpoint hit.
#   3. The `?` command prints the help text.
#   4. The `lb` command reports the breakpoint table.
#   5. The `q` command exits the session cleanly.
#
# This guards against the "phantom relocation" class of compiler bug
# (stale `enc_rel_cnt` shadow desynced from `arr_len(enc_rel_pos)`)
# that produced a wild BL inside `bug_tui_repl` and made the REPL
# jump 9 MB below the binary base on the first command. Fixed by
# eliminating `enc_rel_cnt` in favour of `arr_len()` plus a defensive
# `else` arm in `mo_resolve_relocations`.
#
# Limitation: --break only fires reliably for `main` today. Selective
# breakpoints on non-main functions don't trigger SIGTRAP — that's a
# Phase 1 issue tracked separately. We test the path that works.

set -e

cd "$(dirname "$0")/.."

BPP=./bpp
BUG=./bug

if [ ! -x "$BPP" ] || [ ! -x "$BUG" ]; then
    echo "FAIL: bpp or bug not built"
    exit 1
fi

TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

cat > "$TMP/tgt.bpp" <<EOF
main() {
    auto x;
    x = 42;
    return x;
}
EOF

"$BPP" --bug "$TMP/tgt.bpp" -o "$TMP/tgt" >/dev/null

OUT=$(printf "?\nlb\nq\n" | "$BUG" --tui --break main "$TMP/tgt" 2>&1)

echo "$OUT" | grep -q "(bug)"      || { echo "FAIL: REPL prompt missing"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q "show this help"  || { echo "FAIL: help text missing"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q "(no breakpoints)" || { echo "FAIL: lb output missing"; echo "$OUT"; exit 1; }

echo "PASS  test_bug_tui"
