#!/bin/sh
# tests/bench_mixed_auto.sh — benchmark + cross-backend correctness for
# mixed-type `auto` declarations (the per-declarator type-hint fix, 2026-05-29).
#
# Builds tests/bench_mixed_auto.bpp with BOTH backends — native and `--c`
# (emit C, compile with cc) — runs each, and asserts the checksums match. A
# divergence (or an infinite loop on --c) means a backend mis-typed a mixed
# local, e.g. flooded `: byte` onto the loop counter. Reports each backend's
# wall-clock time so the generated code can be compared (B++ native codegen
# vs clang -O2 from the same source).
#
# Usage (from the repo root):
#   sh tests/bench_mixed_auto.sh
set -e

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BPP="$REPO/bpp"
SRC="$REPO/tests/bench_mixed_auto.bpp"
TMP="$(mktemp -d)"
export BPP_BUILD_ID=00000000000000000000000000000000

# cc flags mirror tests/run_all_c.sh so the emitted C compiles cleanly.
CC_FLAGS="-w -Wno-error=implicit-function-declaration"
LD_FLAGS="-lobjc -lm"

trap 'rm -rf "$TMP"' EXIT

# Time a binary's wall clock (seconds) over `time -p`.
time_run() {
    { /usr/bin/time -p "$1" >/dev/null; } 2>&1 | awk '/real/{print $2}'
}

echo "== native backend =="
"$BPP" "$SRC" -o "$TMP/bench_native"
nat_out=$("$TMP/bench_native")
nat_t=$(time_run "$TMP/bench_native")
echo "  $nat_out   (${nat_t}s)"

echo "== --c backend (emit C -> cc -O2) =="
"$BPP" --c "$SRC" > "$TMP/bench.c"
cc -O2 $CC_FLAGS "$TMP/bench.c" -o "$TMP/bench_c" $LD_FLAGS
c_out=$("$TMP/bench_c")
c_t=$(time_run "$TMP/bench_c")
echo "  $c_out   (${c_t}s)"

echo "== result =="
nat_sum=$(printf '%s' "$nat_out" | sed -E 's/.*checksum=//')
c_sum=$(printf '%s' "$c_out" | sed -E 's/.*checksum=//')
if [ "$nat_sum" = "$c_sum" ]; then
    echo "PASS  mixed-type auto checksum=$nat_sum matches across native + --c"
    exit 0
else
    echo "FAIL  native=$nat_sum  --c=$c_sum  (a backend mis-typed a mixed local)"
    exit 1
fi
