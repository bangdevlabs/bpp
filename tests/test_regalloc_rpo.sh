#!/bin/sh
# test_regalloc_rpo.sh — RegAlloc v2 Stage C prerequisite: RPO gate.
#
# Same rationale as the other regalloc gates: RPO isn't consumed by
# anything yet (Stage C's interval construction is the first consumer),
# so there's no program behavior that could observe a wrong order.
# Functions named __rg_rpo_* trigger regalloc_dump_rpo at compile time;
# this script checks the exact RPO sequence against the structure
# hand-traced in tests/test_regalloc_rpo.bpp's own comments, in
# particular the one edge (10->4) that goes from a HIGHER allocation
# index to a LOWER one with no loop/back-edge involved — the exact case
# that breaks "use the block's allocation index as its position."
set -e
cd "$(dirname "$0")/.."

BPP=./bpp
SRC=tests/test_regalloc_rpo.bpp
OUT=/tmp/_regalloc_rpo_gate

DUMP=$("$BPP" "$SRC" -o "$OUT" 2>&1)
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FAIL  test_regalloc_rpo: $SRC did not compile"
    echo "$DUMP"
    exit 1
fi

fail() {
    echo "FAIL  test_regalloc_rpo: $1"
    echo "$DUMP"
    exit 1
}

echo "$DUMP" | grep -q "RPO __rg_rpo_nested: \[0,1,3,2,5,6,8,10,4,7,9\]" \
    || fail "expected RPO [0,1,3,2,5,6,8,10,4,7,9] -- got: $(echo "$DUMP" | grep 'RPO __rg_rpo_nested')"

"$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  test_regalloc_rpo: $SRC's own runtime assertions failed (rc=$?)"; exit 1; }

echo "PASS  test_regalloc_rpo      (allocation-index-breaking nested while/if/while verified)"
exit 0
