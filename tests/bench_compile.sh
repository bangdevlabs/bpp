#!/bin/sh
# tests/bench_compile.sh — Reproducible compile-time bench harness.
#
# What it measures:
#   1. Self-host bootstrap (`bpp src/bpp.bpp`). The largest input the
#      compiler routinely processes; the canonical "is bootstrap still
#      fast" number cited in `docs/journal.md` entries.
#   2. Small program (`tests/test_array.bpp`, ~55 lines). The cost
#      floor — startup + auto-inject prelude + a few user functions.
#   3. Medium program (`tests/test_stbmidi.bpp`, ~240 lines + stbmidi
#      cartridge). The user-program scale that catches per-AST-node
#      regressions without amplifying them through compiler internals.
#
# Why three sizes: a perf change can move bootstrap by 10% without
# touching small or medium programs (regression sits in compiler-
# internal accessors), and vice-versa (regression sits in a hot
# builtin every program hits). The triple-shot makes the location of
# the cost obvious from the numbers alone.
#
# Usage (from the repo root):
#   sh tests/bench_compile.sh
#   sh tests/bench_compile.sh --runs 10                 # more runs
#   sh tests/bench_compile.sh --bpp /tmp/bpp_candidate  # benchmark a
#                                                       # specific binary
#   sh tests/bench_compile.sh --sample                  # also collect
#                                                       # `sample(1)`
#                                                       # hot-list for
#                                                       # the bootstrap
#                                                       # run (macOS only)
#
# Output shape:
#
#   bench_compile.sh
#     bpp: /Users/.../bpp  (924434 bytes, mtime 2026-05-20 16:27)
#     runs: 5
#
#                                wall (seconds, real)
#     case                    min    median    max
#     ───────────────────  ─────  ────────  ─────
#     bootstrap             0.47    0.48     0.48
#     small (test_array)    0.06    0.06     0.07
#     medium (test_stbmidi) 0.07    0.08     0.09
#
# Citation rule (Tonify Rule 37 — bench citation on perf claims):
#   Any commit message, journal entry, or PR description that claims a
#   compile-time improvement / regression MUST cite a fresh
#   `bench_compile.sh` run. Past tense ("bootstrap was 0.41s") with no
#   citation is not enough — the baseline could have been measured
#   against an errored compile (the OLD-bpp-on-NEW-source trap that
#   surfaced 2026-05-20). Quote the harness output verbatim or summarize
#   the table with min/median per case + total wall.
#
# Exit code: 0 on success, 1 if any single compile fails. Per-run
# wall times go to stdout; failures dump stderr to stderr.

set -u

# ── Argument parsing ─────────────────────────────────────────
RUNS=5
BPP_OVERRIDE=""
SAMPLE_HOTLIST=0
while [ $# -gt 0 ]; do
    case "$1" in
        --runs)        RUNS="$2"; shift 2 ;;
        --bpp)         BPP_OVERRIDE="$2"; shift 2 ;;
        --sample)      SAMPLE_HOTLIST=1; shift ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *)
            echo "unknown flag: $1" >&2
            exit 2
            ;;
    esac
done

# ── Repo + binary discovery ──────────────────────────────────
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

if [ -n "$BPP_OVERRIDE" ]; then
    BPP="$BPP_OVERRIDE"
else
    BPP="$REPO_ROOT/bpp"
fi

if [ ! -x "$BPP" ]; then
    echo "FATAL: $BPP not found or not executable" >&2
    exit 2
fi

# Reproducibility: pin the build_id so any incidental difference in
# the embedded UUID does not leak into wall-time variance.
export BPP_BUILD_ID=00000000000000000000000000000000

# ── Case table ────────────────────────────────────────────────
# Each entry: <label>|<source path>|<output path>
# Output paths live in /tmp so the repo tree stays clean across runs.
CASES="
bootstrap|src/bpp.bpp|/tmp/bench_bootstrap
small (test_array)|tests/test_array.bpp|/tmp/bench_test_array
medium (test_stbmidi)|tests/test_stbmidi.bpp|/tmp/bench_test_stbmidi
"

# Header
binsize=$(stat -f %z "$BPP" 2>/dev/null || stat -c %s "$BPP" 2>/dev/null)
binmtime=$(stat -f "%Sm" -t "%Y-%m-%d %H:%M" "$BPP" 2>/dev/null || \
           stat -c "%y" "$BPP" 2>/dev/null | cut -c1-16)
echo "bench_compile.sh"
echo "  bpp:  $BPP  ($binsize bytes, mtime $binmtime)"
echo "  runs: $RUNS"
echo ""
printf "  %-22s  %-6s  %-6s  %-6s\n" "case" "min" "median" "max"
printf "  %-22s  %-6s  %-6s  %-6s\n" "──────────────────────" "──────" "──────" "──────"

ANY_FAIL=0
RESULTS_TXT=""

# ── Per-case loop ────────────────────────────────────────────
# `IFS=|` splits the table line into label|src|out. Each run records
# the wall-clock seconds reported by `time -p` and the sorted list
# yields min / median / max for the report.
echo "$CASES" | while IFS='|' read -r label src out; do
    [ -z "$label" ] && continue

    times_file=$(mktemp)
    fail=0

    i=1
    while [ "$i" -le "$RUNS" ]; do
        # time -p prints "real X.XX" on stderr; capture stderr only.
        if ! out_real=$( { /usr/bin/time -p "$BPP" "$src" -o "$out" >/dev/null; } 2>&1 ); then
            echo "FAIL  $label: $BPP $src returned non-zero" >&2
            echo "$out_real" >&2
            fail=1
            break
        fi
        real=$(echo "$out_real" | awk '/^real /{print $2}')
        echo "$real" >> "$times_file"
        i=$((i + 1))
    done

    if [ "$fail" -eq 1 ]; then
        printf "  %-22s  FAILED\n" "$label"
        rm -f "$times_file"
        continue
    fi

    # Sort numerically; pick min (head), median (middle row), max (tail).
    sort -n "$times_file" > "$times_file.sorted"
    min=$(head -1 "$times_file.sorted")
    max=$(tail -1 "$times_file.sorted")
    mid_row=$(( (RUNS + 1) / 2 ))
    median=$(sed -n "${mid_row}p" "$times_file.sorted")

    printf "  %-22s  %-6s  %-6s  %-6s\n" "$label" "$min" "$median" "$max"

    rm -f "$times_file" "$times_file.sorted"
done

# ── Optional `sample` hot-list ───────────────────────────────
# macOS-only. Launches one extra bootstrap compile in the background,
# attaches `sample(1)` for ~1s, and prints the top-of-stack list.
if [ "$SAMPLE_HOTLIST" -eq 1 ]; then
    if ! command -v sample >/dev/null 2>&1; then
        echo "" >&2
        echo "  --sample requested but /usr/bin/sample not found (macOS only)" >&2
    else
        echo ""
        echo "  sample(1) hot-list (top-of-stack, >=5 samples) for one bootstrap run:"
        sample_out=$(mktemp)
        "$BPP" src/bpp.bpp -o /tmp/bench_bootstrap_sampled >/dev/null 2>&1 &
        bpp_pid=$!
        sample "$bpp_pid" 1 -mayDie -file "$sample_out" >/dev/null 2>&1 &
        sample_pid=$!
        wait "$bpp_pid"
        wait "$sample_pid" 2>/dev/null || true
        # Print only the "Sort by top of stack" tail — the easy-to-read
        # summary line that names the actual hot functions.
        awk '/^Sort by top of stack/{flag=1; next} flag && /^$/{flag=0} flag' "$sample_out" \
            | sed 's/^/    /'
        rm -f "$sample_out"
    fi
fi

exit $ANY_FAIL
