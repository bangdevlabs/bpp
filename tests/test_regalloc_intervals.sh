#!/bin/sh
# test_regalloc_intervals.sh — RegAlloc v2 Stage C (live intervals) gate.
#
# Same rationale as the other regalloc gates: intervals aren't consumed
# by anything in codegen yet (Stage D, linear-scan, is the first
# consumer), so there's no program behavior that could observe a wrong
# interval. Functions named __rg_ivl_* in the fixture trigger
# regalloc_dump_intervals at compile time; this script checks the exact
# [start_pos, end_pos] per variable against values derived in
# tests/test_regalloc_intervals.bpp's own comments from the already-
# verified Stage B liveness + Stage C RPO sidequest results.
set -e
cd "$(dirname "$0")/.."

BPP=./bpp
SRC=tests/test_regalloc_intervals.bpp
OUT=/tmp/_regalloc_intervals_gate

DUMP=$("$BPP" "$SRC" -o "$OUT" 2>&1)
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FAIL  test_regalloc_intervals: $SRC did not compile"
    echo "$DUMP"
    exit 1
fi

fail() {
    echo "FAIL  test_regalloc_intervals: $1"
    echo "$DUMP"
    exit 1
}

# __rg_ivl_straightline: every variable touched only at RPO position 0.
echo "$DUMP" | grep -q "IVL __rg_ivl_straightline: 4 intervals" \
    || fail "straightline: expected 4 intervals"
echo "$DUMP" | grep -A4 "IVL __rg_ivl_straightline" | grep -q "var 0: \[0,0\]" \
    || fail "straightline var 0: expected [0,0]"
echo "$DUMP" | grep -A4 "IVL __rg_ivl_straightline" | grep -q "var 3: \[0,0\]" \
    || fail "straightline var 3: expected [0,0]"

# __rg_ivl_loop: n, i, sum all span the whole loop -> [0,4] each.
echo "$DUMP" | grep -q "IVL __rg_ivl_loop: 3 intervals" \
    || fail "loop: expected 3 intervals"
echo "$DUMP" | grep -A3 "IVL __rg_ivl_loop" | grep -q "var 0: \[0,4\]" \
    || fail "loop var 0 (n): expected [0,4]"
echo "$DUMP" | grep -A3 "IVL __rg_ivl_loop" | grep -q "var 2: \[0,4\]" \
    || fail "loop var 2 (sum): expected [0,4]"

# __rg_ivl_nonoverlap: the load-bearing check -- p and q's intervals
# must NOT overlap (the whole arc's motivating property).
echo "$DUMP" | grep -q "IVL __rg_ivl_nonoverlap: 3 intervals" \
    || fail "nonoverlap: expected 3 intervals"
echo "$DUMP" | grep -A3 "IVL __rg_ivl_nonoverlap" | grep -q "var 1: \[2,2\]" \
    || fail "nonoverlap var 1 (p): expected [2,2]"
echo "$DUMP" | grep -A3 "IVL __rg_ivl_nonoverlap" | grep -q "var 2: \[1,1\]" \
    || fail "nonoverlap var 2 (q): expected [1,1]"

# Re-run the SAME binary as a normal program too -- Stage C must have
# zero behavioral effect.
"$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  test_regalloc_intervals: $SRC's own runtime assertions failed (rc=$?)"; exit 1; }

echo "PASS  test_regalloc_intervals (3 functions, [start,end] intervals verified, non-overlap confirmed)"
exit 0
