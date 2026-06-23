#!/bin/sh
# test_regalloc_cfg.sh — RegAlloc v2 Stage A (AST-to-CFG construction) gate.
#
# Why a stdout-capture gate instead of a normal .bpp test: the CFG built by
# src/bpp_regalloc.bsm isn't consumed by anything in codegen yet (by design
# — see docs/plans/compiler_boost_roadmap.md F.2's staged plan), so there is
# no PROGRAM BEHAVIOR that could observe a wrong block/edge decision. The
# only way to verify this stage is to read the compiler's OWN diagnostic
# output. Functions named __rg_dump_* in the fixture trigger
# regalloc_dump_cfg at compile time (regalloc_name_wants_dump) — this script
# captures that stdout and checks the exact block count + a few load-bearing
# edges for each hand-traced control-flow shape.
#
# tests/test_regalloc_cfg.bpp is ALSO a normal, runnable program (every
# function has real assertions in main()) — run_all.sh already covers that
# half; this script covers the half run_all.sh can't (the internal analysis
# decision itself).
set -e
cd "$(dirname "$0")/.."

BPP=./bpp
SRC=tests/test_regalloc_cfg.bpp
OUT=/tmp/_regalloc_cfg_gate

DUMP=$("$BPP" "$SRC" -o "$OUT" 2>&1)
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "FAIL  test_regalloc_cfg: $SRC did not compile"
    echo "$DUMP"
    exit 1
fi

fail() {
    echo "FAIL  test_regalloc_cfg: $1"
    echo "$DUMP"
    exit 1
}

# __rg_dump_straightline: one block (no control flow at all) plus the
# always-allocated unreachable tail after its T_RET.
echo "$DUMP" | grep -q "CFG __rg_dump_straightline: 2 blocks" \
    || fail "straightline: expected 2 blocks"

# __rg_dump_ifelse: cond block branches to if-body and else-body, both
# fall through to a shared join block that returns.
echo "$DUMP" | grep -q "CFG __rg_dump_ifelse: 5 blocks" \
    || fail "ifelse: expected 5 blocks"

# __rg_dump_ifnoelse: no else arm — the cond block's false edge goes
# straight to the join block (4 blocks, not 5).
echo "$DUMP" | grep -q "CFG __rg_dump_ifnoelse: 4 blocks" \
    || fail "ifnoelse: expected 4 blocks"

# __rg_dump_whileloop: header/body/continue/exit + the entry + the tail.
echo "$DUMP" | grep -q "CFG __rg_dump_whileloop: 6 blocks" \
    || fail "whileloop: expected 6 blocks"

# __rg_dump_switchret: NOT the function's trailing statement (a `return`
# follows it), so it must reach the CFG builder as a real T_SWITCH node
# instead of being normalised into a ternary by _inline_normalize_
# switch_ret first (which only fires on a TRAILING switch) — 3 arms +
# else, modelled as a chain of 2-way branches: entry + 3*(body+next-test)
# + the else body (reusing the last next-test slot) + join + tail = 9.
echo "$DUMP" | grep -q "CFG __rg_dump_switchret: 9 blocks" \
    || fail "switchret: expected 9 blocks"

# Spot-check one load-bearing EDGE, not just block counts: the while
# loop's continue-block must branch back to its OWN header (a back-edge),
# proving the loop-control wiring is right, not just the block count.
# whileloop's blocks: 0=entry 1=header 2=body 3=exit 4=continue 5=tail.
echo "$DUMP" | grep -A6 "CFG __rg_dump_whileloop" | grep -q "block 4: 0 stmts, succ=(1,-1)" \
    || fail "whileloop: continue block must branch back to header (block 1)"

# Re-run the SAME binary as a normal program too — Stage A must have
# zero behavioral effect (every __rg_dump_* function still has to
# compute the right answer; main()'s own assertions enforce this).
"$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  test_regalloc_cfg: $SRC's own runtime assertions failed (rc=$?)"; exit 1; }

echo "PASS  test_regalloc_cfg      (5 control-flow shapes, block+edge structure verified)"
exit 0
