#!/bin/sh
# bench_autovec_gate.sh — deterministic autovectorisation gate.
#
# Why a static gate instead of a runtime benchmark: `examples/bench_compose.bpp`
# is memory-BANDWIDTH-bound (1M cells), so the 4-wide SIMD the autovectoriser
# emits is hidden behind the memory wall — its runtime is the same whether the
# vectorisation fires or silently regresses to scalar. A runtime benchmark
# therefore can NOT tell you whether the SIMD is alive. This gate reads the
# EMITTED MACHINE CODE instead: it compiles the canonical autovec pattern with
# `--bug`, finds the outlined worker, and checks for 4-wide `.4s` ops (and the
# 128-bit `ldr q` / `str q` they bracket). PASS iff the synth body is
# vectorised; FAIL (scalar synth) is the regression, deterministically.
#
# History note: this gate first read FAIL not because the autovec was broken
# but because the `bug` disassembler could not decode NEON — it printed the
# 128-bit `ldr q` as a scalar `ldr s` and the `.4s` arithmetic as `.word`, so
# the grep below found nothing. The fix was to teach `bug --disasm` NEON (see
# src/bug_disasm.bsm); the autovec itself was correct all along. Lesson: a gate
# is only as honest as the tool it reads through — cross-check against `otool`
# / `objdump` when a disassembly-based gate disagrees with expectation.
#
# Uses the `bug` debugger's static sub-commands (`--dump`, `--disasm`) — no
# ptrace, so it works in CI / Docker.
set -e
cd "$(dirname "$0")/.."
unset BPP_BUILD_ID

SRC=examples/bench_compose.bpp
OUT=/tmp/_autovec_gate

./bpp --bug "$SRC" -o "$OUT" >/dev/null 2>&1 \
    || { echo "FAIL  autovec gate: $SRC did not compile"; exit 1; }

# The outlined loop body becomes a __synth_<n> worker. Find it in the map.
SYNTH=$(bug --dump "$OUT.bug" 2>/dev/null | grep -oE '__synth_[0-9]+' | head -1)
if [ -z "$SYNTH" ]; then
    echo "FAIL  autovec gate: no __synth worker emitted (outlining did not fire)"
    exit 1
fi

# A vectorised worker contains 4-wide arithmetic (.4s) and/or 128-bit q-reg
# load/stores; a scalar fallback uses s-registers only.
VEC=$(bug --disasm "$OUT" "$SYNTH" 2>/dev/null | grep -cE '\.4s|ldr[[:space:]]+q|str[[:space:]]+q' || true)

if [ "$VEC" -gt 0 ]; then
    echo "PASS  bench_autovec_gate     ($SYNTH: $VEC vector ops — 4-wide SIMD fired)"
    exit 0
fi
echo "FAIL  bench_autovec_gate     ($SYNTH is scalar — autovec did not vectorise; regression)"
exit 1
