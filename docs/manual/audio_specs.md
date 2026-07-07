# Audio Specs — Translating Real-World Audio Physics into B++

A catalogue of how the engine turns audio-engineering reality (decibels,
envelope timing, gain-reduction curves, filter corner frequencies) into
b++ code, and a worked checklist for doing it again the next time a real
piece of hardware needs reproducing. Plan 9 / man-page style, like the
rest of the manuals — read this before writing a new DSP plugin, not
instead of reading the actual cartridge source.

This is a younger sibling of `bootstrap_manual.md` (compiler internals)
and `stb++_lib.md` (module reference): those two tell you HOW the
compiler and the library are built; this one tells you how to take a
number from a hardware datasheet or a textbook formula and land it
correctly in a `.bsm` file.

## Cap 1 — The transcendental-math floor: what b++ has, and why it's hand-rolled

b++ ships no libm. Every transcendental function is hand-rolled, pure
b++, no FFI — `src/bpp_math.bsm` is a complete, closed inventory:

| Function | Method | Precision target | Added |
|---|---|---|---|
| `sqrt` | 8 Newton-Raphson iterations | ~15 digits | original |
| `sin_f` / `cos_f` | range-reduce to [-π,π], 5-term Taylor series | ~1e-9 near 0, ~1e-3 near ±π | original |
| `exp_f` | range-reduce to `\|r\| <= ln2/2`, 12-term Taylor series | ~15 digits | 2026-06-24 |
| `log_f` | range-reduce to `m` in [1,2), 8 Newton iterations bootstrapped by `exp_f` | ~15 digits, hand-verified 4-decimal convergence by the 3rd Newton step | 2026-06-24 |
| `amp_to_db_f` / `db_to_amp_f` | `8.6858896*log_f(x)` / `exp_f(x*0.11512925)` | exact given log_f/exp_f | 2026-06-24 |

**Why hand-rolled and not a libm FFI call:** consistency and
self-containment. Every other primitive in the runtime (the heap, the
threads, the syscalls) is reached through the OS-specific backend layer
on purpose (`bootstrap_manual.md`'s portability tiers) — math is the one
domain where the implementation is genuinely the same on every platform,
so writing it once in b++ itself means zero FFI surface, zero ABI
mismatch risk, and the C emitter (`bpp --c`) produces the identical
algorithm instead of silently swapping in glibc's `exp()` and getting a
different rounding behaviour on a different platform.

**The recurring shape**: range-reduce the input into a small interval
where a short series or a few Newton iterations converges fast, then
undo the reduction with a cheap multiply/add. `sin_f`/`cos_f` reduce by
repeated ±2π subtraction; `exp_f` reduces by repeated halving toward
`ln2/2`; `log_f` reduces by repeated doubling/halving toward `[1,2)`.
None of them use bit-level tricks (no `frexp`-style exponent extraction)
— matching the existing precedent of simple, auditable loops over
clever, hard-to-verify bit manipulation. When you need a new
transcendental, look for the SAME shape before inventing a new one;
`log_f` itself was derived by bootstrapping Newton's method off of
`exp_f` rather than writing a second from-scratch series — reuse the
inverse function when one already exists.

**When to reach for a NEW one.** Per Tonify Rule 28 (measure, don't
speculate): add a transcendental only when a real consumer needs it.
`exp_f` was added because the TG12345 compressor's envelope-follower
time-constant formula (`coefficient = 1 - exp(-1/(tau*sr))`) has no
exp-free substitute that stays faithful to the real circuit's timing.
`log_f` followed immediately because dB math needs it and dB math was
the user's own explicit, named requirement for compressor fidelity —
not because "audio code might want it someday." If you find yourself
wanting `pow_f` for a fractional exponent, check whether the formula can
be rewritten in terms of `exp_f`/`log_f` first (`x^y = exp(y*log(x))`)
before adding a third series from scratch — the soft-knee compressor
formula below needed only a literal square (`x*x`), not a real `pow`,
specifically because squaring an exponent is free in any language.

## Cap 2 — Decibels

### §2.1 — The two conversions

```bpp
amp_to_db_f(amp: float) -> float    // 20*log10(amp), floored at -100dB
db_to_amp_f(db: float)  -> float    // 10^(db/20)
```

`amp_to_db_f` floors its input at `0.00001` linear (-100dB) before taking
the log — true silence (`amp == 0`) has no real logarithm, and `log_f`'s
own domain guard (`x <= 0` returns `0.0`) would otherwise read back as
**0dB**, exactly backwards for silence. Always reason about the floor
when you see a metering readout pinned at a suspiciously round number.

### §2.2 — Reference points worth memorising

| Linear amplitude | dB | Mnemonic |
|---|---|---|
| 1.0 | 0 dB | unity gain — "this control was never touched," not the floor |
| 0.5 | -6.02 dB | half amplitude ≈ "half as loud" rule of thumb |
| 0.1 | -20 dB | a tenth |
| 2.0 | +6.02 dB | double amplitude |
| 0.00001 (the floor) | -100 dB | `amp_to_db_f`'s own silence floor |

`stbaudio.bsm` independently documents the SAME unity-gain convention
(`_aud_amplitude` seeds to 32700, "0 dB / unity gain... the professional-
audio convention for 'this control was never touched'" — Pro Tools and
every other DAW default every fader to 0dB, not to the floor). When you
add a new gain control, default it to unity (0dB / 1.0 linear), never to
silence — a control that does nothing until touched is the expected
behaviour; a control that's silent until touched looks like a bug.

### §2.3 — Where dB math already lived before `log_f`/`exp_f` existed

`stbaudio.bsm`'s `audio_set_volume_db` and `stbmixer.bsm`'s
`mixer_set_*_volume_db` family predate `log_f`/`exp_f` by about a month.
Their own header comment is explicit about why: *"B++'s runtime has no
`pow()` yet, so `audio_set_volume_db` does the dB→amplitude conversion
with integer-only arithmetic"* — a 6-entry lookup table of ratios-per-dB
plus bit-shifting for whole-6dB halvings, with the comment *"When the
runtime ships `pow()` the helper can swap to direct math without
changing the API."* That promise is now kept — `audio_set_volume_db` /
`audio_get_volume_db` / `_mx_db_to_pct` / `_mx_pct_to_db` all call
`db_to_amp_f`/`amp_to_db_f` directly today (`tests/test_db_swap.bpp`
locks in the result). The swap was also a genuine, measured
IMPROVEMENT, not just a cleanup: an exhaustive scan found the OLD table
held an exact integer-dB round-trip down to -63dB before the table's
own ~0.3% approximation error compounded with the integer-amplitude
quantization; the new exact math holds the same exact round-trip 11dB
further, down to -73dB (the remaining limit past that point is the
INTEGER AMPLITUDE STORAGE itself, not the math — see the long comment on
`audio_get_volume_db`). The lesson for future readers: an honest "we
faked it, here's exactly how" comment next to a workaround is what makes
the workaround findable and replaceable later. Write that comment every
time you reach for an integer approximation instead of real math; it is
the seed of the next sidequest, not a confession to hide.

## Cap 3 — Envelope followers (time constant → one-pole coefficient)

Every dynamics processor needs to know "how loud is the signal RIGHT
NOW," smoothed over some number of milliseconds so it doesn't react to
every single sample's instantaneous wiggle. The standard tool is a
one-pole exponential smoother:

```
g = 1 - exp(-1 / (tau_seconds * sample_rate))
env[n] = env[n-1] + g * (input[n] - env[n-1])
```

`tau_seconds` is the time constant — informally, "how long until the
follower has covered ~63% of the distance to a sudden new level."
`stb/stbdynamics.bsm`'s `_comp_coeff_from_ms` is the canonical b++
implementation:

```bpp
static _comp_coeff_from_ms(ms: float, sample_rate: float) -> float {
    auto tau_s: float;
    tau_s = ms / 1000.0;
    return 1.0 - exp_f(0.0 - 1.0 / (tau_s * sample_rate));
}
```

**Asymmetric attack/release** is the standard compressor topology: a
SHORT time constant when the level is rising (the follower must catch a
transient quickly) and a LONGER one when it's falling (so the gain
reduction releases smoothly instead of pumping):

```bpp
if (lvl > env) { g = attack_g; } else { g = release_g; }
env = env + g * (lvl - env);
```

Compute `attack_g`/`release_g` ONCE when the time constant changes (at
`_new`/`_set` time), never per sample — the formula above needs
`exp_f`, and paying that cost every sample instead of once per
parameter change is the kind of waste the "redisassemble instead of
guess" discipline exists to catch. `comp_process` itself never calls
`exp_f` directly; only `_comp_coeff_from_ms` does.

`stb/stbenv.bsm`'s ADSR envelope is a different, RELATED tool — it
shapes a synthesized note's amplitude over a triggered attack/decay/
sustain/release lifecycle, not a continuous follower reacting to an
arbitrary input signal. Reach for `stbenv` when you're shaping a voice's
OWN amplitude on note-on/note-off; reach for the one-pole follower
pattern above when you're DETECTING the level of an arbitrary incoming
signal (a compressor, a noise gate, a VU meter).

## Cap 4 — Soft-knee gain reduction (the compressor curve)

The textbook feedforward soft-knee compressor (Reiss & McPherson, *Audio
Effects: Theory, Implementation and Application* — the formula nearly
every software compressor implements, including `stb/stbdynamics.bsm`):

```
over = level_db - threshold_db

if (2*over <= -knee_db):       gr_db = 0
elif (2*|over| <= knee_db):    gr_db = (1/ratio - 1) * (over + knee_db/2)^2 / (2*knee_db)
else:                          gr_db = (1/ratio - 1) * over

gain_linear = db_to_amp_f(gr_db)   // gr_db <= 0, so gain_linear <= 1.0
output = input * gain_linear
```

The middle branch is a quadratic blend across a knee of width `knee_db`
straddling the threshold — instead of snapping hard at the threshold,
the gain reduction ramps in smoothly. This is the dB-domain reason the
formula only ever needs a literal square (`x*x` — see `comp_process`'s
`knee_term * knee_term`), never a fractional `pow`: the knee's
interpolation is quadratic by construction, not exponential.

**Reproducing a real hardware compressor from its datasheet — the
checklist**, worked through for the EMI TG12345 (`stb/stbdynamics.bsm`,
2026-06-24; full primary-source research banked in this session's
project memory, `project_sound_fusion_tg12345_research.md`):

1. **Find the primary source, not the marketing copy.** Modern boutique
   recreations (Chandler Limited, Waves) often expand or round the
   original's parameter set for sale. The EMI TG12345 Mk.II factory
   Handbook itself (`manualslib.com/manual/769744`) gave exact values
   that disagreed with secondary retrospectives in places — always
   prefer the original document when you can find it.
2. **Identify the FIXED vs the SWITCHABLE controls.** A real piece of
   hardware from this era rarely had continuously-variable everything —
   the TG12345 had exactly a mode switch and a 6-position release
   switch, fixed attack. Encode the fixed values as named constants
   (`_COMP_ATTACK_MS`, `_COMP_KNEE_DB`), not as setter parameters — a
   setter you never expose is a control the original didn't have either.
3. **Map ratios to enum constants, not raw floats at call sites.**
   `COMP_COMPRESS` / `COMP_LIMIT` read at the call site; `2.0` / `8.0`
   buried in `comp_set_mode` do not. Document which classic unit each
   mode was built to emulate (RS124 / Fairchild 660) — that context is
   what makes the next reader trust the ratio number is real, not a
   guess.
4. **Implement the detector/timing math with the REAL formula**, not an
   approximation, once the math primitives exist to support it (Cap 1/3
   above) — this is where `exp_f` earned its keep.
5. **Verify by hand BEFORE trusting the test.** Compute one settled
   output value by hand from the formula (a sustained input, a known
   threshold/ratio, the algebra worked on paper) and check the running
   program against that number before writing the locked-in test case.
   Every numeric example in `tests/test_stbdynamics.bpp` was derived
   this way, not reverse-engineered from whatever the first run printed.
6. **Split the cartridge from the plugin.** The DSP math (`stbdynamics`)
   stays genre-agnostic and Tier-1; the named product (`zener_comp`,
   `tools/zener_comp/`) wraps it with the new/set/process/reset/free
   plugin contract (Cap 5) and carries the historical naming. Never
   name a Tier-1 cartridge after the commercial product it evokes —
   that naming discipline belongs at the plugin layer.

## Cap 5 — The plugin-wraps-cartridge pattern

Every DSP effect in the engine has exactly two layers, and the boundary
between them is load-bearing:

```
stb/<name>.bsm           the CARTRIDGE — reusable math, no identity,
                         Tier-1 (no imports) or Tier-2, named after
                         the TECHNIQUE (stbfilter, stbdynamics, stbrotary)

tools/<plugin>/<plugin>.bsm   the PLUGIN — a named product with its own
                              state + lifecycle, named after a real-world
                              reference WITHOUT copying a trademark
                              (moon_filter ~= Moog; zener_comp ~= the
                              Zener-diode circuit, not "TG12345" or
                              "Abbey Road" or "Zener Limiter" — those
                              are Chandler/Waves/EMI's own product names)
```

Both layers always expose the same five-function shape:
`<x>_new(sample_rate) -> ptr`, `<x>_set_*(...)`, `<x>_process(p, in) ->
out`, `<x>_reset(p)`, `<x>_free(p)`. A plugin's `_process` is usually a
one-line forward to the cartridge's own process call (`zener_process`
just calls `comp_process`) — the plugin layer's job is naming and
lifecycle, not new math. The HOST (`sound_fusion`'s channel strip) talks
ONLY to the plugin layer, never reaches into a cartridge directly — the
same discipline `moon_filter`/`stbfilter` established first; `zener_comp`/
`stbdynamics` is the second consumer that proves it's a pattern, not a
one-off.

## Cap 6 — Filter corner frequencies (cross-reference)

One-pole filter design (the Moog ladder's building block, shelving EQ
stages) lives in `stb/stbfilter.bsm` and is NOT duplicated here — see
that file's own header + `flt_onepole_g`'s comment for the `g = x/(1+x)`
exp-free coefficient derivation (a DIFFERENT, older technique than
`exp_f`'s range-reduce-then-series: `flt_onepole_g` maps a cutoff
frequency to a coefficient directly via a rational approximation, no
exponential at all, because a one-pole LOWPASS's coefficient doesn't
need the same precision a millisecond-accurate envelope-follower time
constant does). When the EQ slice of the channel-strip arc opens
(bass shelf + presence bell, both still unbuilt as of this writing —
`docs/plans/sound_fusion.md` slice 8), it extends `stbfilter.bsm` using
that existing technique, not `exp_f` — picking the RIGHT existing
primitive for the job is itself part of this manual's point.

## Cap 7 — Oversampling a nonlinearity (the anti-alias half-band FIR)

A memoryless nonlinearity — a tube waveshaper, a hard clip — generates
harmonics the input did not have. Any of those harmonics above the Nyquist
frequency **fold back** (alias) into the audible band as inharmonic,
metallic grit that no filter downstream can remove, because after aliasing
the garbage sits at legitimate low frequencies. The fix is not a better
clipper; it is running the clipper at a HIGHER sample rate where there is
headroom for the new harmonics, then band-limiting and decimating back:

```
(a, b) = oversample_up(os, x);   // 1 sample -> 2 at 2x rate
a = drive_tube(a, ...);          // shape BOTH at the doubled rate
b = drive_tube(b, ...);
y = oversample_down(os, a, b);   // filter out the folded content, decimate
```

`stb/stboversample.bsm` is the b++ implementation. The band-limiting filter
is a **half-band FIR**: cutoff at exactly a quarter of the oversampled rate
(= half the base Nyquist), and its defining property is that every EVEN tap
except the centre is ZERO — `h(n) = 0` for `n = ±2, ±4, …`. That halves the
multiplies AND makes the upsampler's job cheap: a zero-stuffed input passes
straight through the centre tap on even output phases, and only the
interpolated (odd) samples cost a full FIR. The taps come from the textbook
ideal-sinc-times-a-window recipe (11-tap Hamming), normalised to unity DC
gain so a constant passes unchanged — clean-room, no third-party source.

Three testable properties nail the correctness (`tests/test_stboversample.bpp`):
1. **Constant → clean (c, c) pair.** Upsampling a constant must yield the
   constant on BOTH phases (unity gain, no zero-stuff dip). This is the
   half-band signature and the strongest single check.
2. **Slow sine round-trips delayed.** A passband sine comes back undistorted
   with a constant group delay — **4.5 base samples** for a 2× half-band
   (non-integer: each linear-phase filter contributes a constant and the 2×
   resampling makes the total a half-integer). Comparing against an
   INTEGER-delayed copy misreads the half-sample as ~5% distortion — measure
   the delay empirically (or use the fractional value) before believing a
   "distortion" number.
3. **Stopband null.** A ±1 stream at the oversampled Nyquist (the highest
   frequency the doubled rate carries) must be rejected to ~0. By the
   half-band design the odd taps sum to 0.5, so the response there is
   `h(0) − 2·(h1+h3+h5) ≈ 0.5 − 0.5 = 0`, hand-derivable.

The half-band's summed-products form is also the intended small consumer for
the deferred FP instruction scheduler (`fp_serial_scheduler.md`), the same
"a real FIR to schedule" role the convolution cab fills at full scale.

## Cap 8 — The FMV tone stack (an interacting passive network, from a paper)

The Fender/Marshall/Vox Treble-Middle-Bass tone stack is the classic guitar
tone control, and its character is that the three knobs are **not
orthogonal**: they share one three-capacitor network, so turning one shifts
what the others do, and the famous "mid scoop" at noon is the emergent shape
of that coupling. Three independent shelving filters can never reproduce it;
the real interacting network can, which is why `stb/stbtonestack.bsm` is a
single 3rd-order IIR whose seven coefficients are recomputed from the three
pot positions together, not three separate filters.

**The clean-room path when the reference is GPL.** The obvious reference
implementation is GPL-3.0 and Apache-2.0 b++ cannot pull it in — so the DSP
math had to come from a PUBLIC, non-copyrightable source: the *equations*,
not the code. The authoritative one is Yeh & Smith, "Discretization of the
'59 Fender Bassman Tone Stack" (DAFx-06, `ccrma.stanford.edu/~dtyeh/papers/
yeh06_dafx.pdf`). It gives the analog transfer function
`H(s) = (b1·s + b2·s² + b3·s³)/(a0 + a1·s + a2·s² + a3·s³)` with the seven
coefficients as multi-term polynomials in the pot positions (t, m, l) and
the component values — the m²/l·m/t·m cross terms ARE the control
interaction — and the bilinear transform (`s = c·(1−z⁻¹)/(1+z⁻¹)`, `c =
2·fs`) to a digital 3rd-order filter. Transcribe the equations, implement,
test; do NOT read the GPL code.

**A method note worth keeping.** The paper's coefficient formulas are only
in the PDF (its Figure 1 component values are an image). When the box has no
`pdftotext`/`poppler`, the text is still recoverable by zlib-decompressing
the PDF's own FlateDecode streams (a ~20-line python script) — the formulas
came out verbatim. `'59 Bassman 5F6-A` values: R1/R2/R3/R4 =
250k/1M/25k/56k, C1/C2/C3 = 250pF/.02µF/.02µF — swap them for a Marshall or
Vox, same topology.

The tests (`tests/test_stbtonestack.bpp`) pin the properties, not a
reference implementation: **exact DC block** (the digital numerator
coefficients sum to 0 — the series input cap gives `H(0)=0`, exact up to
rounding), **passivity** (a unit sine never comes out larger), and each
control moving its own band. The reference-behaviour proof is the flat-
setting frequency response — it must be the textbook Fender mid-scoop (bass
~0.83 → scoop bottom ~0.24 @ 640 Hz → treble ~0.60), which no coefficient
typo would reproduce by accident.

## Cap 9 — Amp topology (composing primitives is its own increment)

A guitar-amp model is not one algorithm; it is a WIRING of the primitives
above in the order the hardware uses them. `stb/stbamp.bsm` (Tier-2) owns no
new DSP — it composes stboversample + three tube stages + the tone stack +
a cab, in the '59 Bassman signal-chain order (a public hardware fact):

```
in·gain → up2x → [stage1][stage2] → tonestack → [stage3] → down2x → cab → out·level
```

A tube stage = one-pole input lowpass (the tube's Miller capacitance rolls
off the top) → `drive_tube` asymmetric valve curve (the saturation) →
one-pole coupling highpass (the coupling cap blocks the DC the asymmetric
clip introduces before the next stage). Three cascade with escalating drive;
the tone stack sits between stages 2 and 3 at the OVERSAMPLED rate (where the
Bassman places its passive network); the cheap cab is one lowpass (~4 kHz
speaker rolloff) + one highpass (~85 Hz cabinet limit) at base rate, the
stepping stone a real impulse-response convolution (`stbconv`, later)
replaces.

The value test (`tests/test_stbamp.bpp`) verifies the composed behaviour, not
the pieces (each has its own test): silence→silence, bounded/no-NaN at full
drive, and — the defining nonlinear signature — **saturation that rises with
gain**. The reference-behaviour proof measured how much the output grows when
the input grows 10×: at low gain **9.6×** (near-linear), at high gain
**1.0×** — full tube saturation, the level-independent "singing sustain" of a
cranked amp. That single number is the difference between a working amp model
and a fancy EQ.

**The playtest lesson (the manual's real point here).** When the amp was
first auditioned, its tone controls sounded flat and indistinct — and the
temptation was to declare the DSP broken and start changing coefficients.
The discipline (Tonify Rule 28 / measure-don't-believe, applied to one's OWN
work) said measure first: a low-drive frequency sweep showed the controls DO
move distinct, correct bands, and a level probe showed the amp is not quiet.
The real cause was the **test signal** — a sine carries energy at one
frequency, so no broadband EQ can sound distinct on it, and a passive Fender
stack is authentically subtle. Nothing was broken; nothing was changed. The
right test signal (Cap 10) revealed the controls working. Reach for the
correct measurement before "fixing" a DSP block that a bad test made look
wrong.

## Cap 10 — Test signals: pink noise for a broadband EQ

To hear (or measure) what a tone control does across the spectrum, the input
must HAVE energy across the spectrum. A sine has it at one frequency only, so
an EQ on a sine moves almost nothing — the wrong tool for auditioning a tone
stack. **Pink noise** (`stb/stbnoise.bsm`, `noise_pink`) carries equal energy
per OCTAVE (a −3 dB/octave tilt), which is what sounds "even" to the ear and
excites every band of an EQ at once, so a knob's effect is immediately
audible. White noise (`noise_white`, xorshift, flat spectrum) is the source
the pink filter shapes; pink comes from the standard Paul Kellet economy
filter (three one-pole lowpasses summed over the white — public-domain DSP).

The time-domain signature that tests the tilt without a spectrum analyser:
pink noise is **smoother** than white (its successive samples change less,
because the low-pass integration correlates them). `tests/test_stbnoise.bpp`
checks both are bounded and zero-mean and that pink's mean per-sample step is
well below white's — the tilt captured directly. `tools/pink_noise/
gen_pink_noise.bpp` writes a normalised pink-noise WAV; load it into
sound_fusion (`--import`) to audition the Blondie amp on real broadband
material.

## Cap 11 — The convolution cab (a measured cabinet is just an FIR)

The two-filter cab (Cap 9) is a caricature — a speaker cabinet has a complex,
resonant frequency response no lowpass + highpass captures. The exact way to
reproduce a specific cabinet is to record its **impulse response** (IR — its
output when fed a single click) and CONVOLVE the amp's output with it:

```
y[n] = sum_{k=0}^{M-1} h[k] * x[n-k]      // h = the M-tap cabinet IR
```

That is `stb/stbconv.bsm`: the output is the running dot product of the IR
with the last M inputs. A guitar-cab IR is short (a few hundred to a few
thousand taps), so DIRECT convolution — literally the sum above per sample —
is fine; only when an IR runs to tens of thousands of taps (a reverb) does
partitioned FFT convolution earn its complexity. `amp_set_cab_ir` swaps
stbamp's cheap cab for a real one; the IR is the caller's data (measured, or
license-clean — the cartridge ships none).

**The implementation choice that matters, and why it's the compiler's
business.** A convolution's hot loop is a PURE sum-of-products, so how it is
written decides how fast it runs. stbconv keeps the last M inputs in a
DOUBLE-LENGTH ring (each sample written to both `ring[pos]` and
`ring[pos+M]`), so the convolution window is always a contiguous span and the
inner loop is a branchless two-pointer multiply-accumulate — no modulo, no
wraparound test per tap. That is the exact shape an instruction scheduler
wants. Measured against gcc -O2 (`examples/bench_conv.bpp`, 1024 taps), b++ is
**3.46× slower**, and disassembling gcc shows why: it is NOT vectorised — it
unrolls the loop 4× and issues independent multiplies that fill the FP units,
shortening the critical path, while b++ emits a strict serial fmadd chain.
This is the concrete audio workload that justified building the FP instruction
scheduler (`docs/plans/fp_serial_scheduler.md`); the DSP and the compiler meet
here, which is the whole reason the amp arc ends on a convolution. The full
disasm + numbers are in `docs/manual/benchmarks.md` (2026-07-06 FIR entry).

## Cross-references

- `docs/manual/bootstrap_manual.md` — the portability tiers, the
  three-discipline contract every change goes through, the spine/chip
  split for compiler-internal codegen (not relevant to writing a DSP
  plugin, but explains why math primitives are hand-rolled per Cap 1).
- `docs/manual/stb++_lib.md` Cap 29-31 — the full stbaudio/stbmixer/
  stbsound API reference (the gain-chain layers above where a plugin's
  output eventually lands).
- `docs/manual/tonify_checklist.md` Rule 20 (two-consumer promotion),
  Rule 28 (measure, don't speculate), Rule 33 (Tier 1/2/3 cartridge
  taxonomy) — the three rules this manual's Cap 4-5 lean on hardest.
- `docs/plans/sound_fusion.md` slice 8 — the live build-order doc for
  the channel-strip arc this manual's worked example came from.
- `docs/plans/inliner_arc.md` — the 2026-06-24 addendum records a real
  compiler-frontier finding (`exp_f`/`log_f`'s loop bodies are out of
  the current inliner's scope) discovered while building the compressor
  this manual documents — read it if you're about to write another
  loop-bodied leaf function and wonder whether it will inline.
