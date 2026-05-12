# stbmidi.bsm — design plan

Saved 2026-05-12 as the canonical design anchor for the
`stb/stbmidi.bsm` cartridge. If the implementing session runs out of
context mid-flight, the next agent picks up from here.

## Goal

A modern, market-standard MIDI cartridge for B++ covering three
concrete consumer classes:

1. **Game music playback** — `games/rts_1.0/` ships 45 MIDI tracks
   (converted from XMI by war1tool). Future retro games re-use the
   same path.
2. **mini_synth recording playback** — `tools/mini_synth/` already
   synthesises live; recording-to-MIDI plus loading-back will lean on
   the same event representation.
3. **Live MIDI hardware input** (future) — a CoreMIDI / ALSA driver
   pushes events into the same synth dispatcher. Zero special-cased
   "live mode."

The cartridge mirrors how JUCE / RtMidi / libsmf carve the problem
space: parser + sequencer + synth-dispatch live as three independent
layers behind a unified event representation. No layer locks the
implementation of another, so each can evolve without forcing a
client refactor.

## API surface

```b++
// ─── Constants ────────────────────────────────────────────────────
const MIDI_NOTE_OFF           = 0x80;
const MIDI_NOTE_ON            = 0x90;
const MIDI_POLY_AFTERTOUCH    = 0xA0;
const MIDI_CONTROLLER         = 0xB0;
const MIDI_PROGRAM            = 0xC0;
const MIDI_CHANNEL_AFTERTOUCH = 0xD0;
const MIDI_PITCH_BEND         = 0xE0;
const MIDI_META               = 0xFF;

// Meta sub-types (data1 holds the sub-type for type == MIDI_META)
const MIDI_META_TEXT          = 0x01;
const MIDI_META_TRACK_NAME    = 0x03;
const MIDI_META_INST_NAME     = 0x04;
const MIDI_META_TEMPO         = 0x51;
const MIDI_META_TIME_SIG      = 0x58;
const MIDI_META_EOT           = 0x2F;

// ─── Structures ───────────────────────────────────────────────────
// Event view passed to the synth callback. Allocated once per
// sequencer (reused across dispatches — no per-event malloc).
struct MidiEvent {
    tick_samples,   // absolute sample offset from file start
    type,           // high nibble of status byte (NOTE_ON, etc) or MIDI_META
    channel,        // 0..15 (0 for meta events)
    data1,          // note / program / controller / meta sub-type
    data2           // velocity / value / tempo us-per-quarter / etc
}

// Parsed file. Events are stored as parallel arrays (SoA),
// mirroring the 11-voice parallel-array pattern in stbmixer
// (`stb/stbmixer.bsm:141-151`). The sequencer's hot loop in
// `midi_seq_advance_samples` reads e_tick on every iteration for
// the dispatch-window comparison and only reaches into e_type /
// e_data1 / e_data2 for events that actually fire — SoA keeps the
// comparison column cache-hot. (Tonify Rule 30 is ECS-specific; the
// precedent here is the stbmixer voice-state layout, not Rule 30.)
struct MidiFile {
    format,                  // 0 or 1 (Format 2 rejected at load)
    n_tracks,
    division,                // ticks per quarter note (positive only)
    initial_us_per_quarter,  // tempo at tick 0 (500000 = 120 BPM default)
    n_events,
    e_tick,                  // buf_word: absolute MIDI tick per event
    e_type,                  // buf_word
    e_channel,               // buf_word
    e_data1,                 // buf_word
    e_data2                  // buf_word
}

// Sequencer state. Carries the playback cursor plus the
// anchor-tick / anchor-sample / samples-per-tick triplet that
// makes mid-stream tempo changes drift-free.
//
// `owns_file` decides whether `midi_seq_free` also releases the
// underlying MidiFile. `midi_play_file` sets it to 1 so callers can
// free everything with a single call; direct `midi_seq_new` users
// keep ownership of the file and `owns_file` stays 0.
//
// `scratch_evt` is a single pre-allocated MidiEvent reused across
// every dispatch. The callback receives a pointer to this scratch
// instance — its fields are valid ONLY for the duration of the
// callback call. Callbacks that need to retain an event must copy
// the fields out before returning. No per-event malloc in the hot
// loop.
struct MidiSequencer {
    file,             // MidiFile pointer
    owns_file,        // 1 if midi_seq_free should also free `file`
    cursor,           // next event index in file.e_*
    sample_pos,       // current sample cursor (continuous, never reset)
    anchor_tick,      // most-recent tempo-change anchor (resets on loop)
    anchor_sample,    // sample position at anchor_tick
    samples_per_tick, // samples per MIDI tick from anchor forward
    sample_rate,
    callback,         // fn_ptr called once per dispatched event
    scratch_evt,      // pre-allocated MidiEvent reused for callbacks
    loop,
    playing
}

// ─── Parser ───────────────────────────────────────────────────────
// Loads an SMF Format 0 or 1 file. Format 2 returns NULL with a
// `stbmidi: format 2 not supported` diagnostic. SMPTE division
// (negative division header) returns NULL with a `stbmidi: SMPTE
// division not supported` diagnostic.
midi_file_load(path) -> MidiFile

void midi_file_free(file)

// ─── Sequencer ────────────────────────────────────────────────────
midi_seq_new(file, callback, loop, sample_rate) -> MidiSequencer

// Primary advance: sample-accurate, sample-batched. Call from the
// same site as `mixer_stream(n)` with `n_samples = n`.
void midi_seq_advance_samples(seq, n_samples)

// Convenience wrapper for wall-clock callers (game loops, tests).
// Converts `dt_us` to samples via cached `sample_rate` then
// dispatches identically.
void midi_seq_advance_us(seq, dt_us)

// pause / resume: the cursor stays where it is; resume continues
// from the exact event the dispatcher last visited.
// stop: also resets cursor to 0 AND restores the initial tempo
// anchor (sample_pos goes back to 0). pause = "freeze in place,"
// stop = "rewind and freeze." A subsequent resume after stop plays
// the track from the beginning.
void midi_seq_pause(seq)
void midi_seq_resume(seq)
void midi_seq_stop(seq)
void midi_seq_free(seq)
   // If `owns_file` was set (the midi_play_file path) the file
   // is freed along with the sequencer. Otherwise the caller still
   // owns `seq.file` and must call midi_file_free separately.

// ─── Default synth — routes events to stbmixer ────────────────────
// Event-only signature so live-input drivers can reuse it without
// inventing a fake sequencer handle. `@safe` because the body is
// bounded — it touches the mixer and nothing else.
void midi_synth_default(event) @safe

// ─── High-level helper for games ──────────────────────────────────
midi_play_file(path, loop) -> MidiSequencer
   // Returns 0 on load failure (the parser already printed a
   // diagnostic via put_err). Callers MUST check before passing to
   // any other API; passing 0 to midi_seq_advance_samples crashes.
   // The returned sequencer owns the underlying MidiFile — a single
   // midi_seq_free call releases everything.
```

## Timebase — sample-accurate from day 1

Events carry the absolute MIDI tick from the file. The sequencer's
advance function converts ticks to samples on the fly using:

    sample_of(event) = anchor_sample + (event.tick - anchor_tick) * samples_per_tick

When a tempo meta event (`FF 51 03`) crosses the dispatch boundary,
the sequencer re-anchors:

    anchor_sample   = sample_of(tempo_event)
    anchor_tick     = tempo_event.tick
    samples_per_tick = new_us_per_quarter / division * sample_rate / 1_000_000

This avoids drift across tempo changes because every tempo segment
is anchored at the exact sample its boundary tick maps to. No
accumulated rounding.

`midi_seq_advance_samples(n)` walks events whose computed sample
position falls in `[sample_pos, sample_pos + n)` and fires the
callback in tick order. After the loop, `sample_pos += n`.

**Sample-pos authority.** The sequencer is the sole authority on
its own `sample_pos`. The advance API increments it strictly by the
`n_samples` the caller passes. The sequencer does NOT consult
`beat_now_samples` (which is millisecond-resolution under the hood —
see `src/bpp_beat.bsm`) during the per-event dispatch loop. This
decouples MIDI clock from wall-clock drift and keeps tempo-change
re-anchoring deterministic. `beat_now_samples` is still useful to
seed an anchor at sequencer creation and to cross-check long-running
drift in tests, but it never appears in the hot path.

**Tempo re-anchor — implementation discipline.** When an
`advance_samples` call spans a tempo change, events BEFORE the
tempo event use the OLD `samples_per_tick`, the tempo event itself
triggers the re-anchor, and events AFTER it use the NEW
`samples_per_tick`. The dispatch loop MUST recompute each event's
sample position using the CURRENT anchor at the moment of dispatch.
Pre-computing all events' sample positions upfront breaks the
invariant: any event after a tempo change would carry a stale
offset baked at parse time. Always: read tick, read tempo state,
multiply, dispatch — never cache the product.

**Loop continuity.** When `loop != 0` and the cursor walks past the
last event, the sequencer restarts the cursor at 0 BUT keeps
`sample_pos` continuous (it is not reset to 0). The tick anchor
resets — file ticks restart at 0 — but `anchor_sample` picks up the
current `sample_pos` value so forward tick→sample math stays
consistent. External callers tracking elapsed playback time
("how long has the music been running?") observe a monotonic
`sample_pos` across loop boundaries instead of an abrupt rewind.

## Threading — main thread only

B++'s audio model puts user code exclusively on the main thread.
`stbmixer.mixer_stream(n)` generates samples on the main thread and
pushes them into an SPSC ring; the platform-native audio realtime
thread drains the ring opaquely. No b++ code runs on the realtime
thread.

`stbmidi` inherits this discipline trivially:
- Parser load runs on main thread.
- Sequencer advance runs on main thread (alongside or just before
  `mixer_stream`).
- Synth callback runs on main thread (called by sequencer).
- Pause / play / stop are plain main-thread function calls.

No atomic flags, no lock-free queues, no cross-thread state. Future
upgrade path (sample-driven advance from a real audio callback)
stays compatible: the same `midi_seq_advance_samples` becomes the
audio-callback entry, with the sample-accurate timebase already in
place.

## Format support

- **Format 0** — single MTrk merged across channels. WC1 corpus is
  100% Format 0. Trivial case (n_tracks = 1 inside the parser).
- **Format 1** — N MTrks, each independent, merged into one tick-
  sorted event list at load time. DAW exports + any wild .mid use
  this. ~20–40 LOC delta over Format 0 only; included from day 1.
- **Format 2** — explicitly rejected. Multiple independent sequences
  in one file is rare, weird, and has zero killer use case in the
  B++ horizon.

SMPTE division (header word with the sign bit set, encoding fps +
ticks-per-frame instead of ticks-per-quarter) is rejected with a
clear diagnostic. Music MIDIs use ticks-per-quarter; SMPTE division
is a film / video sync convention.

## LOC budget

| Layer | LOC |
|---|---|
| Constants + structs | ~40 |
| SMF parser (VLQ + header + tracks + Format 1 merge) | ~300 |
| Sequencer (advance + tempo re-anchor) | ~150 |
| Default synth | ~80 |
| midi_play_file helper | ~30 |
| **Cartridge total** | **~600** |
| Correctness tests | ~180 |
| Optional bench | ~60 |

Estimated session: 6–8 hours of focused implementation.

## Implementation order

Bottom-up. Each step verifiable before moving forward.

1. **Constants + structs + skeleton allocator (`midi_file_load` /
   `midi_file_free` shells).** Compile clean, free runs as no-op.
2. **VLQ decode helper + SMF header parse.** Test against the WC1
   `00.mid` header (verifies division = 245, format = 0, n_tracks = 1).
3. **Track parse — single MTrk.** Walk events, decode running
   status, handle meta events (focus on tempo + EOT). Produces a
   parallel-array event stream sorted by tick (single track = already
   sorted).
4. **Format 1 multi-track merge.** Parse N tracks, concatenate, sort
   by tick. Reject Format 2 + SMPTE up front.
5. **Sequencer scaffold — `midi_seq_new` + `midi_seq_advance_samples`
   without tempo changes (constant samples_per_tick from
   initial_us_per_quarter).** Synthetic test verifies events fire in
   the right sample windows.
6. **Tempo change re-anchor logic.** Mid-stream tempo events
   recompute samples_per_tick from their anchor. Synthetic test pins
   drift-free behaviour across a tempo change.
7. **`midi_seq_pause` / `_resume` / `_stop` / `_free`.** Plain state
   toggles + cleanup.
8. **Default synth — route note_on/off to mixer.** Build a 128-entry
   MIDI-key → freq table (12-TET A440). Map other event types to
   no-op for now (program-change / controllers ignored in v1; the
   API contract is still complete).
9. **`midi_play_file` helper.** Load + new + sets callback to
   `midi_synth_default`.
10. **Smoke test against a real WC1 .mid.** Load `00.mid`, run
    `midi_seq_advance_samples` for the equivalent of 10 seconds, count
    events dispatched. Confirms parser + sequencer work end-to-end on
    real data.

## Test gates

- Synthetic Format 0 round-trip: write a known event stream to a
  byte buffer in SMF wire format, parse it back, dispatch through
  the sequencer, confirm callback receives every event with correct
  tick/type/channel/data.
- Format 1 multi-track merge: two tracks with overlapping ticks,
  confirm merged stream is tick-ordered globally.
- Tempo change re-anchor: a stream with `tempo_120bpm @ tick 0` →
  `note_on @ tick 100` → `tempo_240bpm @ tick 200` → `note_on @ tick
  300`. Sample positions of the four events must match closed-form
  predictions.
- Format 2 / SMPTE rejection: pass malformed headers, confirm load
  returns NULL with the right `put_err` message.
- WC1 smoke: load `games/rts_1.0/assets/converted/music/00.mid`,
  advance for 10 seconds of samples, assert > 100 events dispatched
  without crash.

## What is intentionally NOT in v1

Per Rule 28 — deferred until concrete consumer demand surfaces:

- Full General MIDI instrument set (sample-based synthesis). Default
  synth uses the existing `mixer_note_on(key_id, freq)` + the fader-
  driven waveform. stbmixer's tone-voice pool is 8 slots (slots 0–7
  in `_MX_MAX_VOICES = 10`; slot 8 is dedicated to streaming music
  via `mixer_play_music`, slot 9 is reserved). A single MIDI track
  fits comfortably in 8 simultaneous voices for early-90s GM music;
  denser modern arrangements clip at the eighth concurrent note-on.
- Per-channel program change → instrument selection. Default synth
  ignores program-change events; future synth-callback consumers can
  intercept them.
- ADSR envelopes. Need a richer voice in stbmixer first.
- Velocity → loudness mapping. Same dependency.
- Pitch bend, modulation wheel, sustain pedal. Events parse cleanly
  and reach the callback; default synth ignores them.
- Live MIDI hardware I/O. Separate cartridge when the platform
  driver exists (CoreMIDI / ALSA / WinMM).
- SMF file writing. mini_synth can save recordings once the read
  path is stable.
- Multi-sequencer voice arbitration. v1 assumes at most one
  MidiSequencer is dispatching note-on events to the mixer at a time.
  Two sequencers running concurrently share the same 8-voice tone
  pool; if both attempt note-on on the same MIDI key, the second
  call returns the existing voice (stbmixer's "already playing this
  key" short-circuit) instead of starting a new voice with a
  different timbre. Cross-sequencer voice allocation is a separate
  design when a real use case demands two MIDI streams audible at
  once — game music + ambient sting, for instance — and depends on
  the mixer growing per-voice timbre selection first.

## Cross-references

- `docs/rts_stress_arc.md` — infrastructure foundation that
  unblocks the RTS demo (Sessions 1–5 closed 2026-05-12).
- `games/rts_1.0/assets/converted/music/` — 45 real MIDIs the
  cartridge feeds.
- `stb/stbmixer.bsm` — synth dispatch target.
- `tools/mini_synth/mini_synth.bpp` — future second consumer
  (recording playback).

## Status

**Plan saved 2026-05-12.** Implementation pending — slot in when
ready to attack.
