#!/bin/sh
# test_regalloc_intervals.sh — RegAlloc v2 Stage C (live intervals) gate.
#
# Same rationale as the other regalloc gates: intervals aren't consumed
# by anything in codegen yet (Stage D, linear-scan, is the first
# consumer), so there's no program behavior that could observe a wrong
# interval. Functions named __rg_ivl_* in the fixture trigger
# regalloc_dump_intervals at compile time; this script checks the exact
# [start_pos, end_pos] per variable (now STATEMENT-level positions, and
# possibly several intervals per variable via live-range splitting —
# see tests/test_regalloc_intervals.bpp's own comments for the
# hand-derivation) against values derived there.
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

# __rg_ivl_straightline: single block, no gaps -- one interval each.
echo "$DUMP" | grep -q "IVL __rg_ivl_straightline: 4 intervals" \
    || fail "straightline: expected 4 intervals"
echo "$DUMP" | grep -A4 "IVL __rg_ivl_straightline" | grep -q "var 0: \[0,1\]" \
    || fail "straightline var 0 (a): expected [0,1]"
echo "$DUMP" | grep -A4 "IVL __rg_ivl_straightline" | grep -q "var 3: \[2,3\]" \
    || fail "straightline var 3 (y): expected [2,3]"

# __rg_ivl_loop: n and i both split around the exit block (a mutually-
# exclusive sibling of the body in this RPO order, sitting between
# them) -> 2 intervals each; sum stays contiguous -> 1 interval. Total
# 5 LiveInterval entries.
echo "$DUMP" | grep -q "IVL __rg_ivl_loop: 5 intervals" \
    || fail "loop: expected 5 intervals (split entries)"
LOOP_BLOCK=$(echo "$DUMP" | grep -A5 "IVL __rg_ivl_loop")
echo "$LOOP_BLOCK" | grep -q "var 0: \[0,3\]" || fail "loop var 0 (n) first interval: expected [0,3]"
echo "$LOOP_BLOCK" | grep -q "var 0: \[5,6\]" || fail "loop var 0 (n) second interval: expected [5,6]"
echo "$LOOP_BLOCK" | grep -q "var 2: \[2,6\]" || fail "loop var 2 (sum): expected one contiguous [2,6]"

# __rg_ivl_nonoverlap: the load-bearing check -- p and q's intervals
# must NOT overlap (the whole arc's motivating property). `a` itself
# splits around the if-arm's mutually-exclusive sibling.
echo "$DUMP" | grep -q "IVL __rg_ivl_nonoverlap: 4 intervals" \
    || fail "nonoverlap: expected 4 intervals (a splits in two)"
NONOVERLAP_BLOCK=$(echo "$DUMP" | grep -A4 "IVL __rg_ivl_nonoverlap")
echo "$NONOVERLAP_BLOCK" | grep -q "var 1: \[4,5\]" || fail "nonoverlap var 1 (p): expected [4,5]"
echo "$NONOVERLAP_BLOCK" | grep -q "var 2: \[2,3\]" || fail "nonoverlap var 2 (q): expected [2,3]"

# Re-run the SAME binary as a normal program too -- Stage C must have
# zero behavioral effect.
"$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  test_regalloc_intervals: $SRC's own runtime assertions failed (rc=$?)"; exit 1; }

echo "PASS  test_regalloc_intervals (3 functions, statement-level intervals + live-range splitting verified)"
exit 0
