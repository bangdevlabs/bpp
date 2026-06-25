# Plan — audio DSP + instrument/plugin architecture (where things live)

**Status:** roadmap / proposed 2026-06-19. Answers a concrete question — "where
should the Leslie/rotary live? `stbrotary`, `stblfo`, `stbpan`, or `stbmixer`?" —
by first mapping the audio DSP + instrument layer against its **named** consumers,
so cartridge decomposition is principled (Rule 33 tiers, Rule 20 two-consumer)
instead of guessed. Grounded in real + planned products: `mini_synth` (keyboard
synth, exists), **`kong_beat` (an 8/16-bit drum machine, planned)**, `moon_filter`
(Moog plugin, exists), the Leslie/rotary effect (wanted now), `sound_fusion` (the DAW
host), and future synths/effects.

## What exists today (measured 2026-06-19)
- `stbaudio` (device + ring), `stbmixer` (voice table, **oscillators inline**:
  sin/saw/tri/square blended by the fader; buses; `dirt` = bitcrush/decimation;
  **now per-voice pan + stereo**, S2), `stbsound` (WAV), `stbfilter` (Moog ladder
  + one-pole), `stbmidi`, `moon_filter` (Moog plugin), `mini_synth` (instrument).
- **Gaps (nothing has these):** no **LFO**, no **envelope/ADSR** (the mixer note
  is on/off — `stbmixer.bsm:21` itself flags the missing ADSR; note-off clicks),
  no **noise** generator (needed for drums).

## The layer map (primitives → effects → instruments → hosts)

| Tier | Kind | Have | Need (named consumers) |
|---|---|---|---|
| **1 — primitive** | oscillator | mixer (inline) | `kong_beat` toms/kick (pitched osc) |
| | **noise** | — | `kong_beat` snare/hat ← NEW |
| | **envelope (ADSR)** | — | `kong_beat` (every drum hit), `mini_synth` (de-click), future synths ← NEW |
| | **LFO** | — | Leslie (rotation), `mini_synth` (vibrato/tremolo/PWM), `kong_beat` (swing, tom pitch-mod), filter sweeps ← NEW |
| | filter | `stbfilter` | ✓ |
| | pan | mixer + `sf_channel` (inline) | ✓ (stays small) |
| **2 — effect** | Moog plugin | `moon_filter` | ✓ |
| | **rotary/Leslie** | — | `mini_synth` (now), `sound_fusion` channel insert (planned) |
| | delay / reverb / drive | — | future |
| **3 — instrument/tool** | keyboard synth | `mini_synth` | ✓ |
| | **drum machine** | — | `kong_beat` (planned) |
| **host** | DAW / mixer | `sound_fusion` / `stbmixer` | ✓ |

## The decision (per Rule 20/33)

**The LFO is the real primitive to extract; the Leslie is thin glue on top.** A
Leslie = an LFO driving pan (+ AM, + Doppler). The LFO is reused by *four* named
consumers (Leslie, vibrato, kong_beat swing, filter sweeps) — that clears the
two-consumer bar decisively. So:

1. **`stblfo` — extract NOW (Tier 1).** A modulation source: phase + rate +
   waveform (sin/tri/saw/square/S&H), free-run, retrig, depth/offset. Fully
   generic (any audio program), zero genre semantics → Tier 1. This is the answer
   to "stblfo vs stbrotary": **stblfo**, because the LFO is the part with many
   consumers; the rotary is one of them.
2. **The Leslie/rotary = a thin composite on `stblfo` + the mixer's pan.** Its
   *packaging* follows the second consumer:
   - 1 consumer today (`mini_synth`) → start it small: a `mixer_set_rotary(speed)`
     mode in `stbmixer` that advances an `stblfo` per sample and modulates pan
     (the unity-centre balance law already turns a pan sweep into the antiphase
     L/R swell = rotary + tremolo in one).
   - When `sound_fusion` adopts it as a **channel insert** (the concrete 2nd
     consumer, like `moon_filter`) → graduate to **`stbrotary`** (Tier 2 effect:
     stblfo + pan + AM + Doppler), consumed by both. That's the honest
     `stbrotary` moment — not before.
3. **`stbenv` (ADSR) — extract WITH `kong_beat` (Tier 1).** Every drum hit is an
   envelope on osc/noise; it also de-clicks `mini_synth` note-off. Two+ named
   consumers → justified; build it as the kong_beat foundation (or just before).
4. **`stbpan` — do NOT mint.** Pan is a few lines, already inline in the mixer
   (`mixer_set_voice_pan`) and `sf_channel` (constant-power). No independent
   consumer wants pan *alone*. Revisit only if surround/multichannel shows up.
5. **noise — a small generator** (white/pink), lands with `kong_beat`; likely a
   helper in `stbmixer` (a NOISE voice kind) or a tiny `stbosc`, decided when
   kong_beat's voices are built.

## `kong_beat` sketch (the forcing function for env + noise)
An 8/16-bit drum machine = a **step sequencer** over **synthesised drum voices**:
- kick = sine + downward **pitch-env** + amp-**env**; snare = **noise** + tone +
  env; hat = filtered **noise** + short env; clap/tom variants. → each drum is
  `osc/noise + stbenv (+ stbfilter)`.
- sequencer: 16-step grid, per-step on/velocity/**pan** (dogfoods the S2 pan!),
  **swing** (timing or an `stblfo`), pattern chain.
- consumes: `stbenv` (heavy), noise (new), `stbfilter`, `stbmixer` voices,
  optionally `stblfo`. So kong_beat validates env + noise the way the Leslie
  validates the LFO.

## Recommended order
1. **`stblfo`** (Tier-1 primitive) — ✅ SHIPPED 2026-06-21.
2. **Leslie/rotary** on `stblfo` — start as a `stbmixer` rotary mode (1 consumer);
   `mini_synth` toggles chorale/tremolo. MVP = rotary-pan + speed ramp
   (headless-testable via `mixer_fill`); AM + Doppler are step 2.
   **Mixer-side plumbing ✅ SHIPPED 2026-06-21** (`mixer_set_rotary` +
   `tests/test_mixer_rotary.bpp`, proven headless) — **but `mini_synth` does
   not call it yet**, so the feature has no live consumer/keybinding today.
   Wiring that is the open part of this item. Triggered the float-core
   sidequest (`docs/plans/legacy/audio_float_core_migration.md` +
   `docs/plans/legacy/audio_float_device_boundary.md`, both shipped the same day) —
   the rotary's float LFO tick was the friction that surfaced "the mixer
   should be float internally", which then surfaced "so should the device
   wire underneath it."
3. **`stbenv` + noise** — with `kong_beat` (the next instrument), which also
   de-clicks `mini_synth`. **`stbenv` ✅ SHIPPED 2026-06-21** (ADSR, Tier-1,
   `tests/test_stbenv.bpp`) **and the de-click consumer is live**:
   `mixer_note_off` now releases through a short envelope instead of cutting
   instantly (`stb++_lib.md` §30.1c). **Noise is NOT shipped** — still parked
   for when `kong_beat`'s voices are actually built, per the original call
   below; `kong_beat` itself (the step sequencer + drum voices) has not
   started.
4. Graduate **`stbrotary`** (Tier 2) when `sound_fusion` takes the rotary as a
   channel insert (the 2nd consumer).

## Cross-references
- `docs/plans/audio_stereo_dogfood.md` — the stereo/pan work this builds on (S2
  shipped the per-voice pan the rotary modulates).
- `docs/plans/sound_fusion.md` — the DAW host (rotary as a future channel insert).
- Rule 33 (tier triage), Rule 20/28 (a cartridge earns its keep via real
  consumers), Rule 35 (instruments/games stress the engine → surface the gaps).
