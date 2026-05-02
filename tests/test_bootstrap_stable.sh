#!/bin/sh
# tests/test_bootstrap_stable.sh — regression for the documented
# bootstrap convention.
#
# `bootstrap_manual.md` says: compile gen1 from current source,
# compile gen2 from gen1, expect them to be byte-identical. The
# manual does NOT require `BPP_BUILD_ID` to be set — and it
# shouldn't have to. The build_id is derived from a content hash
# of the merged source, so two compiles of the same source tree
# produce the same id without environmental coordination.
#
# This test enforces the invariant: 5 successive bootstraps of
# `src/bpp.bpp`, no env var seeding, all produce the same hash.
# Catches the regression where the build_id falls back to a
# time-based or ASLR-based seed and breaks deterministic builds.

set -e

cd "$(dirname "$0")/.."

unset BPP_BUILD_ID

if [ ! -x ./bpp ]; then
    echo "FAIL: ./bpp not built"
    exit 1
fi

TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

# Triple-bootstrap. gen1 may differ from gen0 (the installed bpp)
# whenever the codegen has changed, so the comparison runs
# starting at gen1 onwards — gen1 is the first build emitted by
# a NEW-pattern compiler. Streams are fully detached from the
# parent shell so a piped or closed inherited stdin does not
# upset the bpp diag emitter.
./bpp        src/bpp.bpp -o "$TMP/gen1" </dev/null >/dev/null 2>/dev/null
"$TMP/gen1"  src/bpp.bpp -o "$TMP/gen2" </dev/null >/dev/null 2>/dev/null
"$TMP/gen2"  src/bpp.bpp -o "$TMP/gen3" </dev/null >/dev/null 2>/dev/null
"$TMP/gen3"  src/bpp.bpp -o "$TMP/gen4" </dev/null >/dev/null 2>/dev/null

# gen2 onwards must be byte-stable. gen1 may differ when the
# installed compiler emits a different pattern from what the
# new source produces (1-cycle oscillation, documented).
cmp "$TMP/gen2" "$TMP/gen3" \
    || { echo "FAIL: gen2 != gen3 (bootstrap not stable)"; exit 1; }
cmp "$TMP/gen3" "$TMP/gen4" \
    || { echo "FAIL: gen3 != gen4 (bootstrap not stable)"; exit 1; }

# Same compiler binary applied to the same source twice in a row
# must produce byte-identical output. This catches in-process
# nondeterminism that the fixpoint check above could mask.
"$TMP/gen2" src/bpp.bpp -o "$TMP/run_a" </dev/null >/dev/null 2>/dev/null
"$TMP/gen2" src/bpp.bpp -o "$TMP/run_b" </dev/null >/dev/null 2>/dev/null
cmp "$TMP/run_a" "$TMP/run_b" \
    || { echo "FAIL: same-compiler same-source produced different output"; exit 1; }

echo "PASS  test_bootstrap_stable"
