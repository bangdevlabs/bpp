#!/bin/sh
# test_regalloc_linscan.sh — RegAlloc v2 Stage D (linear-scan, SHADOW
# MODE) gate.
#
# Same rationale as the other regalloc gates: the allocator's decisions
# aren't wired into cg_var_promote yet (that's Stage E, a separate,
# carefully-gated future increment — see regalloc_linear_scan's own
# header comment in bpp_regalloc.bsm), so there's no program behavior
# that could observe a wrong assignment. Functions named __rg_asn_* in
# the fixture trigger regalloc_dump_assignments at compile time; this
# script checks the exact slot (or "spill") per variable against values
# hand-traced in tests/test_regalloc_linscan.bpp's own comments.
set -e
cd "$(dirname "$0")/.."

BPP=./bpp
SRC=tests/test_regalloc_linscan.bpp
OUT=/tmp/_regalloc_linscan_gate

DUMP=$("$BPP" "$SRC" -o "$OUT" 2>&1)
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FAIL  test_regalloc_linscan: $SRC did not compile"
    echo "$DUMP"
    exit 1
fi

fail() {
    echo "FAIL  test_regalloc_linscan: $1"
    echo "$DUMP"
    exit 1
}

echo "$DUMP" | grep -A4 "ASN __rg_asn_straightline" | grep -q "var 0: slot 0" || fail "straightline var 0: expected slot 0"
echo "$DUMP" | grep -A4 "ASN __rg_asn_straightline" | grep -q "var 3: slot 3" || fail "straightline var 3: expected slot 3"

echo "$DUMP" | grep -A3 "ASN __rg_asn_loop" | grep -q "var 2: slot 2" || fail "loop var 2 (sum): expected slot 2"

# The load-bearing check: p (var 1) and q (var 2) must land on the
# SAME slot, since their intervals never overlap -- the property this
# whole arc exists to exploit, which today's B3 cannot see at all.
P_SLOT=$(echo "$DUMP" | grep -A3 "ASN __rg_asn_nonoverlap" | grep "var 1:" | grep -oE 'slot [0-9]+')
Q_SLOT=$(echo "$DUMP" | grep -A3 "ASN __rg_asn_nonoverlap" | grep "var 2:" | grep -oE 'slot [0-9]+')
if [ -z "$P_SLOT" ] || [ -z "$Q_SLOT" ]; then
    fail "nonoverlap: p or q was spilled instead of getting a slot (got p=$P_SLOT q=$Q_SLOT)"
fi
if [ "$P_SLOT" != "$Q_SLOT" ]; then
    fail "nonoverlap: p ($P_SLOT) and q ($Q_SLOT) should SHARE a slot -- their intervals never overlap"
fi

# Spill path: 11 vars all live simultaneously against budget=10 -- the
# 11th (var 10, arriving last with every slot full and no active
# interval strictly worse than itself) must be the one that spills;
# every earlier one (0..9) must hold a real slot.
SPILL_DUMP=$(echo "$DUMP" | grep -A12 "ASN __rg_asn_spill")
echo "$SPILL_DUMP" | grep -q "var 10: spill" || fail "spill: expected var 10 to spill, got: $SPILL_DUMP"
echo "$SPILL_DUMP" | grep "^  var" | grep -v "var 10:" | grep -q "spill" \
    && fail "spill: only var 10 should spill, found another spilled var: $SPILL_DUMP"

"$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  test_regalloc_linscan: $SRC's own runtime assertions failed (rc=$?)"; exit 1; }

echo "PASS  test_regalloc_linscan  (assign-or-spill verified, p/q slot-sharing confirmed, spill heuristic verified)"
exit 0
