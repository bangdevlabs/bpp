# Plan — float to the device boundary (stbaudio's ASBD + SPSC ring)

**Status:** ✅ SHIPPED 2026-06-21 — all three stages (S1-S3) landed same day as
proposed, plus the `mini_synth` recording-path consequence found and fixed
along the way (Part E). Verified: full suite 195/0/12 unchanged,
`bench_compile.sh` unchanged (0.24s bootstrap, no compiler involvement —
`git diff --stat -- src/` shows only the one platform file), `test_audio_tone`
and `test_mixer_stream` both pass against the **real CoreAudio device** with
the new float ASBD (the strongest signal short of listening — a malformed
format flag fails queue creation outright rather than misplaying silently).
Part 2 of the float-core architecture —
`docs/plans/audio_float_core_migration.md` is Part 1 (closed same day: the
`stbmixer` internal DSP path, s16 only at the sample-read and ring-write
boundaries). Part 1 deliberately left the ring-write boundary alone; this
part studies that boundary itself — `stbaudio`'s `AudioStreamBasicDescription`
+ SPSC ring + the two CoreAudio callbacks in `_stb_audio_macos.bsm` — and
designs the abstract `stb/stbaudio.bsm` contract so a future Linux backend
inherits a float-native wire for free instead of an s16 one it would have to
redesign away from later.

## Part A — Why (researched, not assumed)

Part 1's research established "float internal, fixed-point only at the
wire" as the plugin/DAW standard. The open question this part answers: is
`stbaudio`'s s16 ring *the* wire (the genuine hardware/interchange boundary,
same category as a WAV file on disk), or just *a* boundary the project drew
before checking what the OS's own audio stack actually wants?

Checked against the actual platform docs and the actual code, not memory:

- **macOS Core Audio's canonical format is Float32**, non-interleaved linear
  PCM — for AudioUnits and the AUHAL that talks to the real device. The HAL
  processes internally in float regardless of what format an app hands it.
  `_stb_audio_macos.bsm`'s `_aud_fill_desc` (line 176) requests
  `kAudioFormatFlagIsSignedInteger` — so today's code asks for s16, the HAL
  re-floats it for any internal processing, then converts again for the
  actual DAC format. That round trip buys nothing; it spends a quantization
  step Core Audio's own canonical path does not have.
- **iOS uses Int16** — for a reason that does not transfer: battery/CPU cost
  on ARM silicon without a fast FPU. `mini_synth` / `sound_fusion` target
  desktop macOS, where that tradeoff is moot.
- **WASAPI's shared-mode engine always mixes in float32** internally
  (`IAudioClient::GetMixFormat` reports the engine's native format) —
  submitting int forces the same int→float→int round trip Core Audio does.
- **PortAudio's own design notes** describe why native APIs historically took
  only integer (old hardware/driver constraint) and say plainly: "more
  recently some native APIs accept floats and use floats internally... in
  such cases it may be desirable to pass the floats through unaltered rather
  than converting to an integer format" — exactly the situation Core Audio
  is in today.
- **JACK's sample type is `float` system-wide**, by design, specifically to
  avoid quantization at every routing hop.

Verdict: the s16 ring is inherited from `_stb_audio_macos.bsm`'s own
"Iteration 3... simplest possible signal" scope (the file's own header says
so), not a researched architecture decision. There is a real, citable
reason to go float here — same category of reason as Part 1, one layer
further down, not "preguiça disguised as a boundary."

## Part B — What's touched

| Subsystem | Today | After |
|---|---|---|
| `AudioStreamBasicDescription` (`_aud_fill_desc`) | `mFormatFlags = kAudioFormatFlagIsSignedInteger\|IsPacked` (0x0C), `mBitsPerChannel = 16`, `mBytesPerFrame`/`mBytesPerPacket = 4` | `mFormatFlags = kAudioFormatFlagIsFloat\|IsPacked` (0x09), `mBitsPerChannel = 32`, `mBytesPerFrame`/`mBytesPerPacket = 8` — Core Audio's own canonical shape |
| SPSC ring (`_aud_ring_data`) | 4 bytes/frame (2× s16, raw `poke`/`peek`) | 8 bytes/frame (2× float32, via the existing `pokefloat_h`/`peekfloat_h` builtins — no new compiler primitive needed) |
| `_aud_square_cb` (tone-test callback) | writes s16 samples | writes float32 samples; `_aud_amplitude`'s public contract stays s16-style (0..32767) — converted to a float gain at the one point it's applied, same boundary-conversion discipline Part 1 used for `mixer_set_voice_pan` |
| `_aud_stream_cb` (ring-drain callback, the one `mixer_stream` actually drives) | copies s16 bytes straight through | copies float32 bytes straight through — still a pure memcpy, only the element width changes |
| `stb/stbaudio.bsm` public contract (`audio_push`, `audio_push_frames`) | "push one stereo **s16** frame" | "push one stereo **float32** frame" — the only real contract change in this part; the only existing caller (`stbmixer.mixer_fill`'s final write) already holds a float sample right up to that call |
| `stbmixer.bsm`'s final write (`mixer_fill`) | `float → *32767.0 → truncate → poke 2× s16` | two `pokefloat_h` calls — **deletes** a conversion step Part 1 had to add, instead of adding a new one |
| WAV files on disk (`stbsound.bsm`) | s16 PCM | **unchanged** — disk WAV is its own interchange-format boundary, same reasoning as Part 1's sample/music read path; out of scope here |
| `_aud_amplitude` / `audio_set_amplitude` public API | raw s16 peak 0..32767 | **unchanged contract** — still 0..32767, converted to a float fraction only at the point it's multiplied |

Side-finding while reading the actual callback code (not just the docs):
`stb++_lib.md` Cap 30.8 documents `_aud_amplitude` as a final multiply stage
inside the stbaudio callback (`× _aud_amplitude / 32767 → DAC`), but
`_aud_stream_cb` — the callback `mixer_stream` actually drives — never reads
`_aud_amplitude`; it is a pure ring-to-buffer copy. Only the disused
tone-test callback applies it. Pre-existing doc/code mismatch, unrelated to
this plan — noted so it is not mistaken for something this part must
preserve.

## Part C — Staged rollout

Same discipline as Part 1: each stage independently shippable, tested
headless where possible, and the *behavioral* contract (callback fires,
ring drains, `cb_count`/`consumed` counters advance, tone audible at the
chosen amplitude) must not move.

- **S1 — ASBD + ring width. ✅ SHIPPED.** Flipped `mFormatFlags`
  (`kAudioFormatFlagIsFloat|IsPacked` = `0x09`) / `mBitsPerChannel` (32) /
  `mBytesPerFrame` / `mBytesPerPacket` (8) in `_aud_fill_desc`; widened
  `_aud_ring_data` to 8 bytes/frame. Both callbacks updated together.
  Verified: `test_audio_tone` passes against the real device (CoreAudio
  accepted `AudioQueueNewOutput` with the new descriptor — a wrong flag
  combination would have failed queue creation outright, so this is a real
  acceptance proof, not just "didn't crash"); `test_mixer_stream`'s
  `consumed`/`cb_count` assertions pass against the real ring + callback.
- **S2 — `stbaudio.bsm` contract. ✅ SHIPPED (same commit as S1).**
  `audio_push`/`audio_push_frames` now typed `: float`; doc comments
  updated to describe float32 stereo frames.
- **S3 — `stbmixer.bsm`'s final write. ✅ SHIPPED.** Replaced the
  `*32767.0` + clamp-by-truncation + 2×s16-poke tail in `mixer_fill` with
  two `pokefloat_h` calls — `left`/`right` are already the exact float
  values the wire wants, so this is a straight write with zero conversion
  math left. Verified: `test_mixer_sample`, `test_mixer_rotary`,
  `test_mixer_stream` all pass; their read-back helper swapped
  `peek_q` + sign-extend for `peekfloat_h` (grepped every test that reads
  `mixer_fill`'s output buffer, not just the one already in mind — Tonify's
  verify-the-variants discipline). One buffer-sizing bug caught in the same
  pass: `test_mixer_sample.bpp`'s scratch `out` buffer was already
  under-allocated for its largest `mixer_fill` call before this change (16
  bytes for a call that writes up to 12 frames); fixed to size off the
  actual largest call instead of the first one, independent of this plan
  but found while touching the same lines.

## Part D — Linux readiness (why now, not later)

`docs/plans/elf_dynlink_plan.md` Phase 2 already names the Linux audio
door: the dynlink *engine* is built and Docker-verified (`ALSA.B` →
`libasound.so.2` resolves today), but the actual driver calls are stubbed,
gated on real x86_64 Linux hardware with a driver present — not blocked on
this plan, and this plan does not unblock that hardware gate.

What this plan *does* do: ALSA's native PCM format
(`SND_PCM_FORMAT_FLOAT_LE`) is float — the same shape this plan gives the
ring. If the abstract `stbaudio` contract goes float now, the eventual
Linux platform file slots into the *same* contract with no redesign. (Minor
correction noted in passing: `elf_dynlink_plan.md` calls that future file
`_audio_linux.bsm`; the established convention — already shipped as
`_stb_audio_macos.bsm`, see `bootstrap_manual.md`'s Portability Tiers —
is `_stb_audio_linux.bsm`. Fixed in that doc alongside registering this
plan.) Per the manual's own consolidate-vs-separate doctrine, the abstract
API gets designed "at natural transition points — when the second backend
appears." Doing it now, with exactly one backend shipped, is cheaper than
discovering mid-Linux-activation that the contract needs to change shape
under two consumers at once.

Scope check against the compiler architecture (`bootstrap_manual.md`
Backend Layout + spine sections): this part touches `os/`
(`_stb_audio_macos.bsm`) and the portable `stb/` layer (`stbaudio.bsm`,
`stbmixer.bsm`) only. The spine (`bpp_codegen.bsm`) owns AST-shape codegen
decisions and needs no new ones here — `peekfloat_h`/`pokefloat_h` (32-bit
float load/store, already a chip-level builtin on both a64 and x64 per
`bpp_buf.bsm`) and the existing dlopen/FFI mechanism are sufficient. No
compiler change is in scope.

## Part E — Risks / open questions (resolved)

- **`kAudioFormatFlagIsFloat`/`IsPacked` = `0x09` confirmed empirically, not
  just from memory.** `test_audio_tone` (which calls `AudioQueueNewOutput`
  with the new descriptor against the real device) passes — a wrong flag
  combination would have failed queue creation outright, so this is a real
  acceptance proof.
- The hardcoded sample-rate bit pattern
  (`_aud_poke_u64(desc + 0, 0x40E5888000000000)`) was left untouched as
  planned (offset 0, unrelated field) — no regression.
- **Found and fixed**: `mini_synth`'s recording path was *not* safe by
  default. `_sk_fill_buf` (mixer_fill's output) and the recording copy loop
  share the same buffer — the loop byte-copied it verbatim into
  `_sk_rec_buf` before `sound_save_wav`. Once the wire turned float, that
  copy would have written float32 bytes into a WAV file whose header still
  claims s16 PCM, silently corrupting every recording. Fixed by converting
  sample-by-sample (`peekfloat_h` → `*32767.0` → s16 bytes) at the copy
  point in `tools/mini_synth/mini_synth.bpp`, keeping "WAV stays s16 on
  disk" intact end to end. This is exactly the grep-the-variants risk Part E
  flagged before implementation — confirms the discipline, not just the
  caution.
- **Found and fixed (later, via `mini_synth`'s routing readout):** the
  per-buffer `AudioQueueAllocateBuffer` size in `_stb_audio_tone_start` /
  `_stb_audio_open` stayed hardcoded at `4096` (bytes), which used to equal
  1024 frames at 4 bytes/frame (s16) but silently became 512 frames at the
  new 8 bytes/frame (float32) — the callback fired ~2x as often as
  intended (observed 85 Hz, not the documented ~43 Hz) on half the
  buffering headroom (~11.6ms instead of ~23.2ms). Fixed: both sites now
  request `8192`. `tests/test_audio_tone.bpp`'s `cb_count` check was floor-
  only (`>= 8`) and would not have caught a doubled rate; added a ceiling
  (`<= 16`) so this class of regression fails loud next time.
- **Found and fixed (same session):** `tests/test_audio_pipe.bpp`
  hand-rolls its own wire buffer to test `stbaudio` below `stbmixer` —
  exactly the kind of direct `audio_push_frames` caller the grep-the-
  variants sweep above should have caught, and didn't, because it doesn't
  call `mixer_fill` at all so it never matched that grep. It kept writing
  s16 bytes at a 4-byte stride into a buffer `audio_push_frames` now reads
  at an 8-byte float32 stride — silently corrupted (s16 integer bit
  patterns reinterpreted as IEEE-754 float32 decode to huge/NaN
  magnitudes, audible as harsh broadband noise once unclamped raw bytes
  reach the DAC) until it finally read past the end of its own buffer and
  segfaulted. The test also had no pass/fail assertions at all — pure
  diagnostic prints that always returned 0 — which is *why* a real crash
  was the first signal anyone got. Fixed the synthesis to float32 and
  added the assertions the file's own header already promised
  ("`total_pushed` should equal the frame count", "`cb_count` should be
  > 3", "`consumed` should be > 0").
- **Found and fixed (follow-up, same day): `_aud_amplitude` was a silent
  no-op for every `stbmixer`-based program.** Writing up Cap 30.8's gain
  chain in `stb++_lib.md` for this plan surfaced it: `_aud_stream_cb` (the
  callback `mixer_stream` drives) never read `_aud_amplitude`, only the
  disused tone-test callback did. Fixed by applying the same float gain in
  `_aud_stream_cb`, with `_stb_audio_open` seeding `_aud_amplitude` to 32700
  (0 dB / unity gain, the professional-audio convention for "untouched" —
  not the s16 ceiling 32767, not an arbitrary percent constant) only when
  unset, so existing programs that never call `audio_set_*` are unaffected.
  See `docs/journal.md` 2026-06-21 for the full reasoning.

## Cross-references
- `docs/plans/audio_float_core_migration.md` — Part 1 (mixer-internal
  float), closed the day this part was proposed.
- `docs/plans/elf_dynlink_plan.md` — the Linux dynlink engine + the
  GPU/audio "door" (Phase 2), gated on hardware, not on this plan.
- `docs/manual/bootstrap_manual.md` — Backend Layout, the spine, and the
  Portability Tiers (Tier 3 — why Audio gets its own per-OS file).
- `docs/manual/stb++_lib.md` Cap 29/30 — `stbaudio` / `stbmixer` docs (need
  a follow-up pass once this part ships; they still describe the s16 wire
  and the `_aud_amplitude` gain-chain mismatch noted in Part B).
