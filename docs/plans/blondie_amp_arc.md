# Plan — blondie_amp: a Fender Bassman model (preamp + convolution cab)

**Status:** OPEN (2026-07-03). Increments 1 (SVF) + 2 (stbdrive) + 3 (stboversample) + 4 (FMV tone stack) + 5 (stbamp topology) + 6 (blondie_amp plugin + sound_fusion insert) DONE. Next: 7 (stbconv + real speaker IR — the Stage-F consumer).

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

## Reference + licence (RESOLVED 2026-07-03: clean-room)

`github.com/flubber2077/Open-Source-Bassman-Preamp` (JUCE/C++) is **GPL-3.0**.
b++ is **Apache-2.0**. GPL-3.0 code cannot be pulled into an Apache-2.0 project
(it would force the combined work to GPL). So we do **NOT** port their code — not
their `clip()` formula, not any line. The repo is used only to confirm
*architecture* (facts, not copyrightable): the signal chain, that a Bassman uses
3 tube stages + an FMV tone stack + 2× oversampling + antiderivative anti-alias.

Every DSP piece is implemented **independently from public / textbook sources**:
the Fender circuit is public hardware; tube saturation = soft-clipping (tanh) is
decades-old standard DSP; the FMV tone-stack transfer function derives from the
published schematic's passive network; the ADAA anti-alias is an academic paper
(Parker/Zavalishin/Le Bivic 2016) implemented from the paper's math. No GPL text
is read into the implementation (only the `.h` interface names were ever seen).

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
2. **`stbdrive`** (new Tier-1): the tube-saturation waveshaper. ← DONE. Modeled
   independently (clean-room): `tanh_f` added to `bpp_math` (soft-clip built on
   `exp_f`); `drive_soft` = symmetric tanh clip (odd harmonics), `drive_tube` =
   asymmetric valve curve (even harmonics, DC-free). Stateless pure functions;
   `test_stbdrive` pins exact tanh values + the asymmetry, green on both backends.
3. **`stboversample`** (new): 2x up/down with an anti-alias half-band FIR —
   mandatory so the tube nonlinearity's harmonics above Nyquist don't alias.
   (Its half-band filter is itself a small FIR — a secondary Stage-F touch.)
   ← DONE. 11-tap Hamming-windowed half-band, unity DC gain, clean-room from
   the textbook sinc formula. `oversample_up(os, x) -> (a, b)` (one sample →
   two 2x-rate, via the multi-value return Inc 1 exposed) + `oversample_down(os,
   a, b) -> x`. The FIR is an explicit symmetric sum-of-products (the even taps
   are structurally zero and simply absent) — the intended small Stage-F
   consumer. `test_stboversample` pins the defining properties: a constant
   passes as a clean (c, c) pair (unity gain, no zero-stuff dip), a slow sine
   round-trips to a 4.5-sample-delayed copy (~0.7% passband ripple), the filter
   is linear, and an oversampled-Nyquist ±1 stream is rejected to ~0 (the
   anti-alias stopband null, hand-derived as h0 - 2*(h1+h3+h5) = 0). Green on
   both backends; sound_fusion md5 unchanged (no consumer yet); no bootstrap
   (Layer-2, wildcard-installed).
4. **FMV tone stack** (`stbtonestack`, new Tier-1): the Fender Bass/Mid/Treble
   passive network. ← DONE. Clean-room from the PUBLIC academic result — Yeh &
   Smith, "Discretization of the '59 Fender Bassman Tone Stack" (DAFx-06): the
   3rd-order analog H(s) coefficients (multi-term polynomials in the three pot
   positions, with the m²/l·m/t·m cross terms that make the controls interact)
   transcribed verbatim from the paper, then the paper's own bilinear transform
   (s = c·(1−z⁻¹)/(1+z⁻¹), c = 2·fs) to a digital 3rd-order IIR. NOT the GPL
   reference code (only the paper's equations, a mathematical fact). '59 Bassman
   5F6-A values baked in (250k/1M/25k/56k, 250pF/.02µF/.02µF), swappable for
   Marshall/Vox. `tonestack_set(ts, treble, mid, bass, fs)` recomputes the seven
   coefficients; `tonestack_tick(ts, x)` runs the difference equation.
   `test_stbtonestack` pins: exact DC block (numerator coeffs sum to 0 — the
   series input cap), passive (never amplifies), and each control moves its own
   band. Verified against the reference BEHAVIOUR — the flat-setting response
   shows the textbook Fender mid-scoop (bass 0.83 → scoop bottom 0.24 @ 640 Hz →
   treble 0.60). Green both backends (233/0/12 + 194/0/51); md5 unchanged;
   Layer-1, no bootstrap.
5. **`stbamp`** (new Tier-2): the topology — 3 tube stages + tone stack + the
   cheap 2-filter cab — composed from the primitives above. Named after the
   technique (amp modelling). ← DONE. Signal chain: `in*in_gain -> up2x ->
   [stage1][stage2] -> tonestack -> [stage3] -> down2x -> cab -> out*level`,
   where a stage = one-pole input LP (Miller ~10kHz) -> `drive_tube` asymmetric
   valve curve -> one-pole coupling HP (~10Hz, DC block); the tone stack sits
   between stages 2 and 3 at the oversampled rate; cab = LP ~4kHz + HP ~85Hz at
   base rate. `amp_set_gain(0..1)` escalates the three stage drives; `amp_set_
   tone(t,m,b)`. Composes all four Tier-1 primitives (stbfilter/stbdrive/
   stboversample/stbtonestack) — NO new DSP math, pure wiring. `test_stbamp`
   pins: silence->silence, bounded/no-NaN at full drive, saturation increases
   with gain, tone knob changes the highs. Reference-behaviour proof: at low
   gain a 10x input grows 9.6x (near-linear); at high gain it grows 1.0x (full
   tube saturation — level-independent, the cranked-amp sustain). Both backends
   234/0/12 + 195/0/51; md5 unchanged; Layer-2, no bootstrap.
6. **`blondie_amp`** plugin (`tools/blondie_amp/`) + wire into sound_fusion as a
   5th channel insert (mono stage, before the pan — an amp is a mono device, same
   as the spring). First playable Bassman tone. ← DONE. `blondie_amp.bsm` is the
   thin plugin wrapper over stbamp (new/set/process/reset/free, homage name like
   morricone_spring). Wired as the FIRST insert in `sf_channel_process` (the amp
   is the sound source; filter/comp/spring/rotary are post-amp effects) with its
   own Track fields (amp_on + amp handle + gain/treble/mid/bass cache), setter
   `sf_track_set_amp`, reset, and five GUI accessors. The GUI grew a 5th lane-
   header toggle (AMP, first; all five now 17px to fit the 150px header) and an
   AMP rack block (gain + treble/mid/bass drag-bars); GUI_H 600→680 for the room.
   Off by default → sound_fusion render md5 UNCHANGED (`f61fac72…`). Headless
   probe confirmed the amp is live in the render path (channel energy 902 off →
   2151 on, no NaN). Native suite 234/0/12; zero warnings; tools-only, no
   bootstrap. NOTE: the demo project doesn't engage it — this is a playtest ship
   (toggle AMP on a channel and drive it).
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
