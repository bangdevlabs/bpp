# Plan — blondie_amp: a Fender Bassman model (preamp + convolution cab)

**Status:** OPEN (2026-07-03). Increment 1 (SVF) DONE. Next: Increment 2 (stbdrive).

**Increment 1 note — a compiler bug fell out of it.** The SVF's guard test (an
explicit `x != x` NaN check) exposed that the **C emitter never implemented
multi-value return**: `return a,b,c` emitted only `return a`, and `a,b,c = f()`
only assigned `a` — every value past the first came back garbage on `--c`, for
int and float, at all arities. Latent because the audio stack runs native and
loose `>`-threshold checks let NaN slip through (NaN compares false). Fixed both
sides in `bpp_emitter.bsm` to bank values 1..N-1 into `_bpp_{float,int}_ret_p`
globals (mirroring the native x0..x7 / d0..d7 banking). Also implemented `tan_f`
in `bpp_math` (the bilinear prewarp, real consumers imminent). Bootstrap
byte-stable; native 222/0/12, C-emit 179/0/55; `test_svf` now green on BOTH
backends; sound_fusion render md5 unchanged.

## Goal

A full-fidelity Fender Bassman guitar-amp model as a sound_fusion plugin:
`blondie_amp` (the preamp — tube stages + FMV tone stack) plus a **convolution
cab** (a real speaker impulse response, not a filter approximation). The name is
an homage: "Blondie" is Clint Eastwood's character in *The Good, the Bad and the
Ugly* — scored by Morricone — so `blondie_amp` + `morricone_spring` form one
spaghetti-western guitar rig.

**Two goals, both real:**
1. The authentic Bassman tone (the user's musical target).
2. **Stress the engine.** The convolution cab is a genuine FIR (sum of
   independent products) — the first real consumer of the deferred **Stage-F**
   FP-instruction-scheduler frontier (`fp_serial_scheduler.md` Phase 2). The
   preamp itself is nonlinear + IIR (NOT a Stage-F consumer); the cab is where
   the guitar feature and the compiler frontier finally coincide.

## Reference (read the source, don't guess)

`github.com/flubber2077/Open-Source-Bassman-Preamp` (JUCE/C++). We **port the DSP
model** to b++ — we do NOT copy the C++ text (different language anyway), and the
Bassman circuit itself is public hardware (a schematic is not copyrightable). The
tone-stack transfer function and the tube-saturation curve are textbook DSP.
**TODO before shipping stbdrive/tonestack: check the repo's licence and credit it
in the plugin header.** The SVF (Increment 1) is a textbook filter, no IP tie.

Signal chain the source revealed (2x oversampled):
`up2x -> [tubeStage x3, with the FMV tone stack before stage 3] -> down2x -> cab`
where each tube stage = `input filter -> saturation waveshaper -> coupling
filter`, and that repo's "cab" is a cheap 2-filter approximation (LP ~4 kHz +
HP ~85 Hz) — we take that as the stepping-stone cab, then replace it with a real
IR convolution.

## Increments (each ships with a hand-derived value test; all Layer 1-2, no bootstrap)

1. **SVF in `stbfilter`** (state-variable filter, TPT/Cytomic form). The
   workhorse resonant LP/HP/BP/notch. Highest leverage — it unblocks the preamp's
   coupling/Miller filters, the cheap cab, AND the deferred **TG12345 2-band EQ
   slice** (`sound_fusion.md`). Three planned consumers (Rule 36). ← DONE
2. **`stbdrive`** (new Tier-1): the tube-saturation waveshaper. Port the exact
   curve from `Saturation.cpp`. `exp_f` gives tanh if needed, or a cheaper
   polynomial/rational soft clip.
3. **`stboversample`** (new): 2x up/down with an anti-alias half-band FIR —
   mandatory so the tube nonlinearity's harmonics above Nyquist don't alias.
   (Its half-band filter is itself a small FIR — a secondary Stage-F touch.)
4. **FMV tone stack** (`stbfilter` or `stbamp`): port the Fender Bass/Mid/Treble
   passive network coefficients from `FMVTonestack.cpp`.
5. **`stbamp`** (new Tier-2): the topology — 3 tube stages + tone stack + the
   cheap 2-filter cab — composed from the primitives above. Named after the
   technique (amp modelling).
6. **`blondie_amp`** plugin (`tools/blondie_amp/`) + wire into sound_fusion as a
   5th channel insert (mono stage, before the pan — an amp is a mono device, same
   as the spring). First playable Bassman tone.
7. **`stbconv` + a real speaker IR** (the endgame): direct short-FIR convolution
   (reuses `stbdelay`'s ring buffer) for a real Bassman cab. This is the Stage-F
   consumer — measure the FP scheduler here; reassociation of the FIR dot-product
   is `@fast`-opt-in (FP add is not associative). If the IR is long, partitioned
   FFT convolution is the follow-on (a further FFT/Stage-F consumer).

## Discipline

- Cheap cab (Increment 5) first as a stepping stone, then the real IR (Increment
  7) — the user's call: prototype the tone fast, upgrade fidelity after.
- Each primitive: a value test pinning hand-derived numbers or defining
  properties (like `test_stbdelay`/`test_stbreverb`).
- No compiler bytes change → no bootstrap; `stb/*.bsm` is wildcard-installed;
  the plugin lives in `tools/` (loaded by relative path, like moon_filter).
- Verify sound_fusion's `--render` md5 stays unchanged after each stb addition
  (the inserts are off by default → byte-identical baseline).
