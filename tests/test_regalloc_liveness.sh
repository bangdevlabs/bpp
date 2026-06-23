#!/bin/sh
# test_regalloc_liveness.sh — RegAlloc v2 Stage B (liveness analysis) gate.
#
# Same rationale as test_regalloc_cfg.sh: live_in/live_out isn't consumed
# by anything in codegen yet (Stage C, live intervals, is the first
# consumer), so there is no program behavior that could observe a wrong
# dataflow result. Functions named __rg_live_* in the fixture trigger
# regalloc_dump_liveness at compile time; this script captures that
# stdout and checks the exact use/def/live_in/live_out bitsets against
# values hand-traced in tests/test_regalloc_liveness.bpp's own comments.
#
# Bitsets are printed as decimal (bit vi = variable index vi, in
# cg_var_add's declaration order: params first, then each `auto` in
# source order).
set -e
cd "$(dirname "$0")/.."

BPP=./bpp
SRC=tests/test_regalloc_liveness.bpp
OUT=/tmp/_regalloc_liveness_gate

DUMP=$("$BPP" "$SRC" -o "$OUT" 2>&1)
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FAIL  test_regalloc_liveness: $SRC did not compile"
    echo "$DUMP"
    exit 1
fi

fail() {
    echo "FAIL  test_regalloc_liveness: $1"
    echo "$DUMP"
    exit 1
}

# __rg_live_straightline: vi a=0,b=1,x=2,y=3. x/y are both defined
# before any read in the block (kill-then-gen), so only a/b (read with
# no preceding def) enter `use`. No successor (T_RET) -> live_out=0.
echo "$DUMP" | grep -A2 "LIVE __rg_live_straightline" | grep -q "block 0: use=3 def=12 live_in=3 live_out=0" \
    || fail "straightline block 0: expected use=3 def=12 live_in=3 live_out=0"

# __rg_live_loop: vi n=0,i=1,sum=2. i and sum both carried across the
# whole loop -- header AND body must show live_in=live_out=7 (={n,i,sum}
# minus whatever each block's own def removes -- here nothing does,
# since n is never written and i/sum are read before being re-defined
# each iteration). The continue block (index 4, no step since this is a
# plain `while`) must branch back to the header (index 1) with the same
# live set flowing through unchanged.
echo "$DUMP" | grep -A7 "LIVE __rg_live_loop" | grep -q "block 1: use=3 def=0 live_in=7 live_out=7" \
    || fail "loop header (block 1): expected use=3 def=0 live_in=7 live_out=7"
echo "$DUMP" | grep -A7 "LIVE __rg_live_loop" | grep -q "block 2: use=6 def=6 live_in=7 live_out=7" \
    || fail "loop body (block 2): expected use=6 def=6 live_in=7 live_out=7"
echo "$DUMP" | grep -A7 "LIVE __rg_live_loop" | grep -q "block 4: use=0 def=0 live_in=7 live_out=7" \
    || fail "loop continue (block 4): expected use=0 def=0 live_in=7 live_out=7"

# __rg_live_nonoverlap: vi a=0,p=1,q=2. p is defined and consumed
# entirely within the if-arm's own block (def=2={p}); q entirely within
# the else-path's own block (def=4={q}) -- neither ever appears in any
# block's live_in/live_out (bit 1 or bit 2 set) anywhere in the
# function, confirming they never cross a block boundary alive, let
# alone coexist. Only `a` (bit 0) ever appears in a live set.
NONOVERLAP_BLOCK=$(echo "$DUMP" | grep -A5 "LIVE __rg_live_nonoverlap" | grep "^  block")
echo "$NONOVERLAP_BLOCK" | grep -vE "live_in=[01] live_out=[01]$" \
    && fail "nonoverlap: p or q leaked into a live_in/live_out set (every block must show live_in/live_out in {0,1} -- only bit 0 (a) ever crosses a block boundary)"
echo "$DUMP" | grep -A5 "LIVE __rg_live_nonoverlap" | grep -q "block 1: use=1 def=2 live_in=1 live_out=0" \
    || fail "nonoverlap if-arm (block 1): expected use=1 def=2 live_in=1 live_out=0"
echo "$DUMP" | grep -A5 "LIVE __rg_live_nonoverlap" | grep -q "block 3: use=1 def=4 live_in=1 live_out=0" \
    || fail "nonoverlap else-path (block 3): expected use=1 def=4 live_in=1 live_out=0"

# Re-run the SAME binary as a normal program too -- Stage B must have
# zero behavioral effect (main()'s own assertions enforce this).
"$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  test_regalloc_liveness: $SRC's own runtime assertions failed (rc=$?)"; exit 1; }

echo "PASS  test_regalloc_liveness (3 functions, use/def/live_in/live_out dataflow verified)"
exit 0
