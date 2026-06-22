# Plan — stbmixer float-core migration (int s16 → float DSP, s16 only at the wire)

**Status:** ✅ SHIPPED 2026-06-21 — all four stages (S1-S4) landed same-day. Surfaced
while wiring the Leslie
rotary (`mixer_set_rotary`): the rotary's float LFO tick had to be converted into a
fixed-point `/1024 word` gain just to multiply into `stbmixer`'s entirely-integer
per-sample loop — the exact same conversion `mixer_set_voice_pan` already does for
per-voice pan. The user asked "audio shouldn't this all be float?" — real doubt, not
rhetorical — so we researched the market before deciding. Verdict: yes, and it's
worth doing now rather than paying the float→fixed→int tax on every future DSP
feature (`stbenv`/noise for `kong_beat`, future sends, the eventual `stbrotary`
graduation).

---

## Part A — Why now (the market research)

- **Industry standard is float internal, fixed-point only at the wire.** VST3/AU/AAX
  process in 32-bit float (VST3 mixing mode and mastering plugins go 64-bit). Modern
  DAW summing engines are float: Ableton Live 32-bit internal + 64-bit summing,
  Logic Pro 64-bit summing engine. Even Pro Tools, historically fixed-point (48-bit
  fixed summing on HD/MIX), migrated to 64-bit float on HDX. Float wins because
  headroom across a processing chain is lossless until the final clip — fixed-point
  accumulates quantization error at every stage.
- **Vintage fixed-point gear (Linn LM-1, E-mu SP-1200, Akai MPC60) was fixed-point
  because the chips had no FPU — not an aesthetic choice.** The "lo-fi crunch" people
  now associate with that era is a side effect of those hardware limits, later turned
  into a *deliberate* effect (decimators/bitcrushers process a clean float signal and
  quantize on purpose, rather than baking reduced precision into the whole engine).
  `stbmixer`'s `_mx_dirt` already wants to be that kind of effect, but today it
  crunches a signal that was already quantized to s16 — crunch-on-crunch.
- **b++ already has the seam.** `stbfilter` (the Moog ladder) and `stblfo` are
  float-only by design ("Everything runs in float; convert to/from s16 at the audio
  boundary" — `stbfilter.bsm` header). `tl_timeline.bsm` (tiny_lofi) already uses the
  float convention `sample / 32768.0` in, `* 32767.0` out. `stbmixer` is the one
  cartridge still running the old all-integer engine. Migrating it *unifies* the
  stack instead of introducing a third convention.
- **Perf is not a blocker.** The float-serial codegen gap closed in the register-
  lever arc — `biquad` (FP serial) is at gcc -O2 parity per `bench_codegen.bpp`
  (`docs/manual/benchmarks.md`). "int is faster" stopped being true for b++ once that
  shipped; there's no performance argument left for keeping the mixer integer.

## Part B — What's touched (before → after)

| Subsystem | Today | After |
|---|---|---|
| Per-voice oscillator (square/sine/tri/saw + fader crossfade) | int, amp baked into the waveform math | float waveform in **[-1, 1]**, gain applied as a separate float multiply |
| Per-voice pan (`_mx_voice_pan_l/r`) | fixed-point `/1024 word` | float gain `0.0..1.0`, multiplied directly — no fixed-point step |
| Bus volume / master volume (0..100) | int percent, `* vol / 100` | **external API unchanged** (still 0..100 / dB) — converted to a float fraction once per `mixer_fill` call, multiplied directly |
| Dirt (bitcrush + decimation) | the *only* precision available | an explicit quantize→requantize-to-float effect inserted into the float chain — same audible crunch, now structurally an effect, not a precision ceiling |
| Sample/Music WAV playback | s16 read, mixed as int | WAV stays s16 **on disk** (interchange format, unaffected) — converted to float once per sample read (`peek_q` + sign-extend + `/32768.0`), mixed in float |
| Rotary (just shipped) | float LFO tick → `/1024 word` gain → int multiply | float LFO tick → float gain → direct float multiply (drops the `rgl`/`rgr` fixed-point dance) |
| The wire boundary | `mixer_fill`'s final `poke` writes s16 | **unchanged in kind** — still the s16 conversion point, just sourced from float (`* 32767.0`, clamp, truncate) instead of int. The ring buffer and CoreAudio queue were always s16 and stay s16. |
| External API (`mixer_set_voice_volume(slot, amp: word 0..32767)`, `audio_set_amplitude`, bus/master percent + dB setters) | s16 raw / percent / dB | **unchanged contract** — callers (`mini_synth`, `rhythm`, `snake`, `tiny_lofi`) need zero edits; the int↔float conversion happens only inside `stbmixer.bsm` at the boundary functions |

The float convention is **-1.0..+1.0 full scale**, matching `tl_timeline.bsm`
(`sf / 32768.0` in, `* 32767.0` out) and `stbfilter` — not a new convention.

## Part C — Staged rollout

Each stage is independently shippable and tested headless via `mixer_fill` (no
audio device needed — the existing `test_mixer_*` pattern). A stage is allowed to
move the audio-correctness baseline (same discipline as the stereo S1/S2 arc); what
must NOT move is the *behavioral* contract the tests check: voice-activity counts,
L==R when centred/rotary-off, L≠R when panned/rotary-on, clamps holding.

- **S1 — master-stage float. ✅ SHIPPED 2026-06-21.** Converted the per-frame
  accumulator from the end of the voice loop onward: rotary, master/bus gain, clip,
  dirt, final s16 write. Voices still hand back int samples (oscillators/pan/
  sample-read are unchanged — S2/S3 below). `master_gain_f` is derived once per
  `mixer_fill` call (hoisted, same discipline as `fader`/`master_bus_vol`); the
  rotary code from this session simplified in the same pass (dropped `rgl`/`rgr`
  fixed-point, now a direct float gain). **S4 absorbed into S1**: dirt's bitcrush
  already lived in the master stage being converted, so the quantize-to-s16-then-
  back-to-float framing (the deliberate-effect shape) landed here instead of as a
  separate stage — the crush/decimate bit-math itself is byte-identical, just
  bracketed by the float boundary now. Verified: `test_mixer_sample`,
  `test_mixer_stream`, `test_mixer_rotary` all pass; a dirt probe (`mixer_set_dirt`
  + amplitude scan) confirms sane non-zero, non-overflowing s16 output; full suite
  195/0/12; `mini_synth`/`rhythm`/`snake_maestro` (the three consumers) compile
  clean with zero warnings.
- **S2 — per-voice oscillator float. ✅ SHIPPED 2026-06-21.** Square/sine/tri/saw +
  fader crossfade computed as a float waveform in [-1,1] (`sn_f`/`tri_f`/`saw_f`/
  `sq_f`/`sample_f`); `amp` converted once to a float gain (`amp_f = amp/32768.0`)
  and applied as a separate multiply instead of being baked into the waveform math.
  `math_sin`'s int table (×1024) stays — it's a cheap per-sample lookup, no reason
  to switch to `sin_f`'s Taylor series — just normalized to float on read.
- **S3 — pan in float directly. ✅ SHIPPED 2026-06-21 (same commit as S2).** Dropped
  `_mx_voice_pan_l/r`'s fixed-point `/1024` everywhere (oscillator, sample, music
  paths). The arrays keep their existing `buf_word`-backed 8-byte slots (no
  allocation change) but now hold IEEE float bits, read/written via
  `peekfloat`/`pokefloat` at the computed byte offset — same idiom the WAV sample
  reads already use (`peek_q(sbuf + spos*4)`), not a new convention.
  `mixer_set_voice_pan`'s `pan: float` parameter no longer needs the `* 1024.0`
  conversion; the stored gain IS the multiplier. Landed together with S2 because
  both touch the same per-voice loop body, and finishing pan let the per-frame
  `left`/`right` accumulator itself become float from the top of the frame loop —
  retiring S1's separate `left_f`/`right_f` boundary conversion (there's only one
  representation through the whole frame now). Verified: `test_mixer_sample`,
  `test_mixer_stream`, `test_mixer_rotary` pass; a pan probe (centre → L==R,
  hard-left/hard-right → the far channel drops to exactly 0) confirms the float
  gain path; the dirt probe's peak amplitude is bit-for-bit unchanged (6399);
  full suite 195/0/12; all three consumers compile clean.
- **S4 — sample/music WAV playback in float. ✅ SHIPPED 2026-06-21 (closed the same
  day as S2/S3).** Turned out to be already 90% done as a forced consequence of
  S3: once pan became a float multiply, the SAMPLE/MUSIC branches HAD to normalize
  their s16 read (`left_s`/`right_s`) to float (`ls_f`/`rs_f`) before the pan
  multiply, to feed the now-float `left`/`right` accumulator — there was no
  remaining int leg to convert. What's left genuinely int is the wire-format read
  itself (`peek_q` + sign-extend on the raw s16 bytes) — structurally identical to
  why the final write is s16: both are interchange-format boundaries, not part of
  the DSP signal path. Closed with a permanent regression test:
  `test_mixer_sample.bpp` case (9) plays a non-silent SAMPLE voice hard-panned
  right and confirms L is exactly silent while R stays live. **All four stages
  shipped — the float-core migration is complete.** `_mx_voice_pan_l/r`'s last
  `/1024`/`*1024` trace is gone (only the unrelated `math_sin` lookup table, which
  is internal-only and was never part of the migration, still says ×1024).
  Human-verified: the user installed the rebuilt `mini_synth` lib and confirmed
  it plays normally — this is the first float-core checkpoint validated by ear,
  not just by checksum.

The mixer is now float end-to-end except at its two wire-format boundaries:
reading an s16 sample/music buffer in, and writing s16 to `buf` (the ring/device)
out. Both boundaries are unavoidable — they're interchange formats, not part of
the DSP path. `mini_synth`/`rhythm`/`snake_maestro` all compile clean against the
new engine; full suite 195/0/12 throughout every stage.

## Part D — Risks

- Many call sites touch `_mx_voice_amp` / `_mx_voice_pan_l/r` directly inside
  `mixer_fill`'s three voice-kind branches — each stage needs the full grep, not a
  spot edit.
- Existing tests read raw s16 bytes back from `mixer_fill`'s output buffer
  (`peek_q` + sign-extend) — that contract is the wire format and does NOT change,
  so those tests stay valid through every stage.
- dB↔percent helpers (`_mx_db_to_pct` / `_mx_pct_to_db`) are pure percent math,
  untouched by this migration — they feed the float fraction conversion, not the
  other way around.

## Cross-references
- `docs/plans/audio_dsp_architecture.md` — the rotary/Leslie packaging decision this
  builds on.
- `docs/plans/audio_stereo_dogfood.md` — where the float/int seam first became
  visible (`_mx_voice_pan_l/r` shipped as fixed-point specifically because the mixer
  was int-only at the time).
- `stb/stbfilter.bsm`, `stb/stblfo.bsm`, `tools/tiny_lofi/tl_timeline.bsm` — the
  float convention this migration aligns `stbmixer` with.
