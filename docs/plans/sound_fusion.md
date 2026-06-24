# Plan — tiny_lofi: an 8/16-bit mini DAW in b++

**Goal:** a small DAW to produce 8/16-bit (chiptune/lofi) tracks — and, just as
importantly, to dogfood the whole b++ audio system end to end (synth → record →
edit → mix → plugin → export). Started 2026-06-18 by the user (a music
producer), after the codegen reached gcc -O2 parity.

## The blocks that already exist

| block | role |
|---|---|
| `stbwindow` | window + manual frame loop (native) |
| `stbui` | immediate-mode UI widgets (buttons, sliders, panels) |
| `stbmixer` | voice table, integer s16 synthesis, buses |
| `stbsound` | `sound_save_wav` / `sound_load_wav` (RIFF, 44100 / s16 / stereo) |
| `tools/mini_synth/` | the instrument — keyboard synth, records to WAV |
| `stb/stbfilter.bsm` | the DSP filter **cartridge** — reusable primitives (`flt_onepole_*`) + filters built on them (Moog ladder; EQ next). `moog_taps` returns all four ladder taps (-6/-12/-18/-24 dB/oct) in one b++ multi-value return |
| `tools/moon_filter/` | the first **plugin** — "moon_filter", a named effect (`moon_new`/`set`/`process`/`reset`/`free`) wrapping the cartridge's Moog. Hung on a tiny_lofi channel |

tiny_lofi is the application that arranges these into a timeline + mixer.

### The audio architecture — three layers

```
stb/stbfilter.bsm   DSP cartridge   reusable primitives + filters (no identity)
        ↓ import
tools/moon_filter/  named plugin    moon_filter: state + params + process (a product)
        ↓ load "../moon_filter/"
tools/tiny_lofi/    the host (DAW)   hangs plugins on channels, arranges + mixes
```

A *cartridge* is a category of reusable math (filters) and lives in `stb/`; a
*plugin* is a named product with its own identity + lifecycle and lives in
`tools/`. The DAW hosts plugins. New plugins (EQ, bit-crush, drive) follow the
same shape: cartridge primitive(s) in stbfilter, named wrapper in `tools/`.

## The MVP feature set (from the user)

A timeline to record/cut/edit; **8 instrument channels** to mix; a small tool
belt — scissors (cut), Ctrl+C / Ctrl+V (copy/paste), record, drag clips; a
plugin **insert** (the filter); a **mixer** for per-channel volume; and
**export**. The minimum to record the mini_synth and arrange it into a track.

## Architecture — model + render, GUI on top

The core is a pure **data model** and a pure **render** function. The GUI only
edits the model; the render only reads it. Keeping them separate means the audio
pipeline is testable offline (render a hardcoded project → a WAV) before any
pixel is drawn — which is exactly how we prove the audio system works.

```
Project
  bpm, sample_rate, length
  Track[8]
    name, volume (0..1), muted, the insert (filter on/off + cutoff/reso)
    Clip[]
      audio buffer (s16) + start (samples on the timeline) + len + offset-in-buffer
  master_volume

render(project) -> s16 stereo buffer:
  for each frame:
    sum = 0
    for each track:
      if (track.muted) { continue; }    // skip entirely — not yet wired, see below
      s = sample from the track's active clip at this frame (0 if none)
      s = track.insert.filter(s)        // the Moog plugin, per track
      s = s * track.volume
      sum += s
    out[frame] = sum * master_volume    // soft-clamp to s16
```

**Per-track mute — deferred, not yet implemented** (user request 2026-06-21,
parked for when the mixer-strip slice below gets interactive write-back).
`Track.muted` is in the schema but nothing reads it yet — `grep -rn "muted"
tools/tiny_lofi/` is empty today. This is a *different* mechanism from the
`stbaudio` session-level mute fixed the same day
(`docs/plans/legacy/audio_float_device_boundary.md` — `_aud_amplitude`, one global
knob applied after everything is mixed): a tiny_lofi track mute is per-track,
applied *inside* `tl_render`'s mixing loop, the same shape as `track.volume`
above — `continue`/skip the track's contribution entirely (cheaper and more
correct than multiplying by 0, since a muted track's insert/filter state
doesn't need to keep ticking). Worth mirroring `stbaudio`'s
save-on-mute/restore-on-unmute pattern (`_aud_is_muted` + `_aud_saved_amp` in
`_stb_audio_macos.bsm`) for `Track.muted` + `Track.volume` too, so toggling
mute doesn't lose the fader's prior position.

Clips reference loaded s16 buffers (a mini_synth WAV export, or a recorded
take). Cut splits a clip into two; copy/paste duplicates a clip's
(buffer, offset, len); drag changes its `start`.

## Two decisions that shape everything

1. **Playback: offline-render-then-play, OR realtime streaming.**
   - *Offline* (recommended for the MVP): "play" renders the visible range to a
     buffer and plays that buffer; "export" renders the whole project to a WAV.
     Simple, no realtime constraint, gets a working editor fast.
   - *Realtime*: the `@safe` audio callback streams the live mix straight from
     the model as you edit. The real DAW feel, but the realtime/no-malloc
     constraint + per-block mixing is a later slice.
2. **Recording source — the mini-Cubase vision (set by the user 2026-06-18).**
   The target feel: open the mini_synth *inside a tiny_lofi track*, record a take
   there, then hang plugins on top (moon_filter first). The key realization is
   that the synth engine is **stbmixer**, not locked inside mini_synth —
   mini_synth is just a keyboard UI + a record loop around `mixer_note_on` /
   `mixer_fill`. So tiny_lofi drives the *same engine* directly; no synth code is
   duplicated.
   - *Import-first* (the near step): record in mini_synth → WAV → import as a
     clip → the channel insert (moon_filter) filters it. Needs only WAV-import.
   - *Live capture* (the real thing): tiny_lofi imports stbmixer, plays notes,
     captures `mixer_fill` into the armed channel at the playhead → moon_filter
     on top. Same engine, a dedicated slice (arm + playhead + record buffer).

## Build order (vertical slices, each shippable + dogfoodable)

1. **Engine** — ✅ DONE (2026-06-18). The data model (Track[8] / Clip) +
   `tl_render()` + `tl_export()`. Validated by a hardcoded project: a 220 Hz
   sine on channel 0 (scissors-cut at 1.0 s, lossless) + a 330 Hz saw through
   the Moog at a 2-pole slope on channel 1 → a mixed WAV. The slope-4 variant
   renders byte-identical to a pre-refactor baseline, so the mix is proven.
   *This is the slice that tests the audio system — it does.*
2. **GUI shell** — ✅ DONE (2026-06-18, `tl_gui.bsm`). Window + the timeline
   grid (8 lanes, a time ruler, a playhead) + a transport bar (Play / Stop /
   Export, the buttons live) + a mixer strip (8 faders + master). Play
   renders the project once and streams it through the audio device while
   the playhead follows the cursor. `./tl` opens the window; `./tl --render`
   keeps the headless WAV path for CI. **Faders are now interactive ✅
   SHIPPED 2026-06-22** (closes this slice's open follow-up): the lane
   header's horizontal volume bar, the mixer strip's 8 vertical faders, and
   the master bar all read mouse position while dragged and write back live
   — `tl_channel.bsm` gained `tl_track_set_volume(tr, vol)` as a focused
   setter (changes only `.vol`, leaving the insert chain and pan untouched —
   routing this through the kitchen-sink `tl_track_set` would have forced
   re-supplying flt_on/cutoff/reso/slope/pan on every drag frame, since the
   GUI doesn't cache them). A per-channel mute toggle is still open, parked
   with the per-track mute note above (same underlying gap: `Track.muted`
   has no write path yet).

   **Found and fixed 2026-06-22 — `tl_render` was feeding s16 bytes to a
   float32 device wire.** Reported by the user after the slice-3 editing
   fixes landed: "o audio que veio foi algo muito tosco... lago surreal,
   doeu o ouvido" the moment Play was pressed. `tl_render` wrote stereo s16
   (`write_u16`) into `_g_play_buf`, which `tl_gui_run`'s Play path then
   handed straight to `audio_push_frames` — but that function has read
   stereo **float32** (8 bytes/frame) since
   `docs/plans/legacy/audio_float_device_boundary.md` shipped on 2026-06-21.
   Reinterpreting s16 integer bit patterns as IEEE-754 float32 decodes to
   huge/NaN magnitudes — exactly the "horrible modem noise" mechanism that
   plan's own "Found and fixed" list already named twice
   (`tests/test_audio_pipe.bpp`, and a silent `_aud_amplitude` no-op). This
   was a **third** instance of the same bug shape: `tl_gui_run` was a third
   hand-rolled `audio_push_frames` caller (bypassing `stbmixer`, like
   `test_audio_pipe.bpp` did) that the original migration's grep-the-callers
   sweep never matched, because `tiny_lofi` didn't exist yet when that sweep
   ran. It went unnoticed for a full day because the headless `--render` /
   `tl_export` path never touched `audio_push_frames` at all — only the
   interactive Play button did, and apparently nobody had pressed it since
   the float32 migration until right after slice 3 made the editor usable
   enough to want to.

   Fixed by making `tl_render` itself float32-native (`pokefloat_h`, 8
   bytes/frame, clamped to [-1,1]) — the same wire format `stbmixer.
   mixer_fill` already targets — and moving the s16-on-disk conversion into
   `tl_export` (a dedicated pass, `peekfloat_h` → `*32767.0` → `write_u16`,
   the same boundary-conversion shape `mini_synth`'s recording loop already
   uses). **Verified, not assumed:** a sample-by-sample diff between the
   pre-fix and post-fix EXPORT path (264,600 samples) found a max difference
   of **1 LSB** — pure float32-precision rounding noise from the
   `pokefloat_h`/`peekfloat_h` round-trip, confirming the conversion is
   mathematically equivalent and the export path was never the broken one.
   A dedicated probe pushed `tl_render`'s real output through the actual
   CoreAudio device end-to-end: zero NaN/out-of-range samples, callback
   ran, ring drained real frames. Bootstrap stable, suite 198/0/12.
3. **Clips + tools — ✅ CLOSED 2026-06-22.** Scissors (`tl_clip_split`,
   non-destructive cut) ✅ DONE. WAV import ✅ SHIPPED (`tl_io.bsm`'s
   `tl_import_wav`/`tl_import_clip` — see slice 6a below for the detail; the
   `tiny_lofi.bpp` demo still ALSO keeps the `gen_sine`/`gen_saw` generators
   for its two synthetic tracks, not because import didn't land but because
   they're a convenient zero-dependency way to seed a deterministic demo
   project — no change needed there). Select / drag / copy-paste / delete
   ✅ SHIPPED, in two passes (the first shipped select+drag+copy/paste; the
   user then live-tested it and reported three real gaps, fixed in the
   second pass):
   - **Select + drag** (`_g_draw_lanes`): click a clip to select it (a thick
     yellow 4-edge outline — the first version only drew 2 thin edges and
     was too easy to miss, per the user's own report after testing) and
     start a drag; the clip follows the mouse along the timeline (`start`)
     and across lanes (`track`) until the button releases. Clicking empty
     lane space deselects.
   - **Copy/paste** (`_g_handle_clip_keys`): accepts EITHER Ctrl or Cmd/META
     — the first version checked only `KEY_CTRL`, which silently did
     nothing for a Mac user's muscle-memory ⌘C/⌘V (caught by the same
     live-test). Paste places a new clip (sharing the source's buffer, the
     same aliasing convention `tl_clip_split` already established) at the
     current playhead, on the source's own track.
   - **Delete** (same function, Delete or Backspace): removes the selected
     clip. Needed a new prelude primitive — `bpp_arr.bsm` had push/at/count/
     reset/free but no individual removal at all. Added
     `arr_struct_remove(a, idx)` (swap-remove: the last live element moves
     into the removed slot, O(1), order not preserved — right for an
     unordered collection where only "is X still here" matters).
     `tl_timeline.bsm` wraps it as `tl_remove_clip(i)`. Since `bpp_arr.bsm`
     is in the COMPILER's own import graph (`bpp.bpp` imports it), this
     change needed the full bootstrap cycle, not just a suite run — ran
     clean (gen2==gen3==gen4).
   - **Click-to-seek** (`_g_draw_ruler` + the empty-lane-click case in
     `_g_draw_lanes`): clicking the ruler or empty timeline space moves the
     playhead there — the third gap the live-test caught ("clico na
     timeline e o marcador não sai do tempo zero"). This was never built in
     the first pass, not a regression.
   - **Verified:** new `tests/test_bpp_arr.bpp` Phase 7 (swap-remove from
     the middle, from the end, and an out-of-range no-op) + a one-off engine
     probe (`tl_remove_clip` swap behavior, `tl_track_set_volume` clamping)
     run clean. Bootstrap stable, suite 198/0/12. The interactive-fader
     work above shipped in the same session, caught by the same live-test
     pass ("os botões de volume do canal e do mixer tbm não funcionam").
   - **Still open:** drawing clip CONTENT (waveform/notes) inside the block
     — noted under slice 6c below, not built.
4. **Insert (single)** — ✅ engine-level DONE: every `Track` already carries
   one hardcoded Moog-filter slot (`tk.flt_on` / `tk.flt`,
   `tl_channel_process` calls `moon_process`) and `tl_track_set` already takes
   cutoff/reso/slope. **Open:** the channel-strip UI doesn't expose
   cutoff/resonance controls yet (`tl_gui.bsm`'s mixer strip is fader-only).
5. **Insert CHAIN + the `stbrotary` graduation — ✅ SHIPPED 2026-06-22.**
   `tl_channel.bsm`'s own header comment had already flagged this: "adding a
   second insert plugin later means giving the channel a small chain instead
   of one `flt` slot."
   - **5a — generalize `Track`'s insert — shipped as a fixed TWO-slot chain,
     not a generic N-deep one.** `Track` gained `rot_on`/`rot` alongside the
     existing `flt_on`/`flt`; `tl_channel_process` runs Moog (pre-pan) then
     Rotary (post-pan, on the now-stereo signal — the same position it
     occupies in `stbmixer`'s own master stage) in that fixed order.
     Deliberately NOT a generic kind-dispatched array: there are exactly two
     concrete plugin kinds today and a fixed tone-then-spin order is the
     natural signal-chain shape (a guitar amp or organ rig always drives
     filter/drive into modulation, not the other way round) — Rule 20's
     two-consumer bar applies to "how many slots get generalized", and two
     named fields clears it more honestly than an abstraction with one real
     shape. A third insert kind is the trigger to revisit, not now.
     `tl_track_set_rotary(tr, rot_on, mode)` is the new setter (kept separate
     from `tl_track_set` rather than growing its already-7-arg list further).
   - **5b — extracted the rotary's core out of `stbmixer`'s single global
     instance into `stb/stbrotary.bsm`, a per-instance Tier-2 effect.** This
     **is** the graduation `docs/plans/audio_dsp_architecture.md` named —
     "graduate `stbrotary` (Tier 2) when `tiny_lofi` takes the rotary as a
     channel insert (the 2nd consumer)." `stbmixer`'s public API
     (`mixer_set_rotary`/`mixer_get_rotary`/`MX_ROTARY_*`) is byte-for-byte
     unchanged — it now wraps one `Rotary` instance (`_mx_rotary`) instead of
     four bare globals; `mixer_fill`'s inline glide/LFO/pan-law block
     collapsed to one `rotary_tick` call. `tl_channel.bsm` owns a SEPARATE
     instance per track (8 total), proving the instances don't share state.
   - **Verified, not assumed:** new `tests/test_stbrotary.bpp` (4 cases,
     including two independent instances not sharing state) + the existing
     `tests/test_mixer_rotary.bpp` both pass unchanged against the refactor.
     Rendered `tiny_lofi`'s hardcoded demo project with the lead track's
     rotary OFF → md5 `3e258737c9f0ef83193793b63eb7eed8`, an EXACT match for
     the documented pre-existing stereo baseline (`audio_stereo_dogfood.md`)
     — proves the refactor changed zero bytes of existing behavior. With the
     rotary ON, the render differs starting at the exact frame the lead clip
     begins (frame ~22050, 0.5 s) — proves the new insert is doing real,
     measurable work, not a no-op. Bootstrap stable, suite 198/0/12 (the new
     test counted). `mini_synth`/`rhythm`/`snake_maestro` recompile clean.
   - Both halves apply to every existing audio-clip channel today —
     useful independently of live recording, no instrument-track work
     required first. `tiny_lofi.bpp`'s own hardcoded demo project now
     dogfoods the rotary on its lead track, the same way it already
     dogfoods scissors and pan.
6. **Live record — open mini_synth inside a channel.** The mini-Cubase vision
   from Part 2 above, now sized into two real sub-steps instead of one big
   jump (mini_synth has no synth engine of its own — it is a keyboard UI +
   record loop around `mixer_note_on`/`mixer_fill`, so neither step duplicates
   DSP code):
   - **6a — import-first — ✅ SHIPPED 2026-06-22.** `tl_io.bsm` gained
     `tl_import_wav(path) -> (ptr, word)` (loads via `sound_load_wav`,
     down-mixes stbsound's always-stereo decode to the mono s16 shape every
     `Clip` already uses — L+R averaged; this matches the engine's existing
     "mono source panned into the field" convention, not a new compromise)
     and `tl_import_clip(track, path, start)` (the one-call "drop a file on
     a track" action: import + `tl_add_clip` together). **Verified, not
     assumed:** a synthetic WAV with opposite-sign L/R (100, -100, 200,
     -200, …) downmixes to exactly 0 every frame; an equal-L/R WAV (12345,
     12345) downmixes losslessly back to 12345 — both adversarial enough to
     catch a sign or averaging bug, not just "looked plausible." The
     `tiny_lofi.bpp` demo now dogfoods the real round-trip: channel 2 saves
     channel 0's dry bass to a real WAV file and imports it straight back as
     a fresh clip, hard-panned left so it's audibly distinct — proving a
     take recorded elsewhere (mini_synth's own SPACE → `sound_save_wav`, or
     any external sample) becomes a normal `Clip` with zero special-casing,
     and slice 5's insert chain (Moog + Rotary) processes it exactly like
     any other audio with no extra wiring. Bootstrap stable, suite 198/0/12.
     mini_synth's actual recordings remain stereo on disk — down-mixing
     happens only at tiny_lofi's import boundary, not at mini_synth's
     recording boundary.
   - **6b — live capture (the real thing).** `tiny_lofi` imports `stbmixer`
     directly, drives `mixer_note_on`/`mixer_note_off` from the keyboard the
     same way `mini_synth` does, and captures `mixer_fill`'s output into the
     armed track's clip buffer at the playhead. A dedicated slice: arm state,
     playhead-synced capture, record buffer lifecycle. This is the point
     where a track stops being strictly "pre-rendered audio" and starts
     behaving like an instrument track — but it still produces a normal
     `Clip` at the end (recorded, not live-streamed), so it does NOT require
     `tl_render` to become realtime. A true always-live instrument track
     (re-synthesized from a stored note sequence on every render, never
     baked to a clip) is a further-out idea, not scoped here — 6b already
     delivers the "open mini_synth inside a track" feel.
   - **6c — draw the clip's CONTENT in its timeline block (noted 2026-06-22,
     not built).** Every shipped DAW draws *what's inside* a block, not just
     a solid rectangle: an audio clip shows its waveform (min/max envelope
     per pixel column — nobody renders every sample, the block is too
     narrow), a MIDI/instrument clip shows its notes (a tiny piano-roll
     silhouette, pitch → vertical position, duration → width). `tl_gui.bsm`
     today draws clips as plain rectangles. This matters more once 6b lands
     (an instrument-track block needs ITS OWN visual language, distinct from
     an audio block, so a user can tell at a glance which track is which
     kind) — flagged here so the need doesn't get lost, not scheduled. Likely
     shape when it's time: precompute a per-clip min/max envelope at
     import/record time (cheap, one pass, stored alongside the `Clip`) rather
     than recomputing it from raw samples every redraw; a MIDI/note-block
     needs the NoteEvent sequence 6b's design implies to exist first.
7. **Realtime playback** — the `@safe` callback streams the live mix; scrubbing
   and play-while-edit. The big architectural jump (offline-render-then-play →
   true realtime); do this last, once everything above already works offline.

Slices 1–4 give a usable offline DAW: load sounds, arrange on 8 tracks, cut /
copy / paste, set volumes, filter, export a WAV. Slice 5 makes inserts
stackable and lands the rotary. Slice 6 is the live-instrument feel. Slice 7
is the realtime jump.

### A separate, decoupled `@safe` step — ✅ SHIPPED 2026-06-22

The dogfood backlog's `@safe` item (`docs/plans/audio_stereo_dogfood.md` Part
A #2) targeted `stbmixer.mixer_fill` — the producer-side fill path mini_synth's
main loop calls every ~23 ms to keep the SPSC ring fed for the real CoreAudio
callback (`_aud_stream_cb`, already `@safe`) to drain. Annotated
`mixer_fill(buf, n: word)@safe` directly (`stb/stbmixer.bsm`); the compiler's
W026 call-graph walk found **zero violations on the first try** — `math_sin`
(bpp_math), `env_tick`/`env_is_active` (stbenv), `lfo_set_rate`/`lfo_tick`
(stblfo) are all pure-arithmetic Tier-1 leaves by design, confirming the
inspection-based read was right, not just lucky. Verified: bootstrap stable,
suite 197/0/12, and all three real consumers (`mini_synth`, `games/rhythm`,
`games/snake/snake_maestro`) recompile clean with zero warnings — rebuilt and
committed alongside. `mini_synth`'s RSS stayed flat (~68 MB, unrelated to the
unbounded-RAM fix shipped the same day — this was a compiler-verification
annotation, not a runtime change).

mini_synth's own per-frame loop (`window_frame_step`/`draw_*`/`audio_push_frames`)
stays unannotated on purpose — it is the main-thread orchestrator (IO, drawing,
the ring producer), exactly the shape Tonify Rule 4 says should NOT carry
`@safe`. The contract that matters lives one level down, in the function that
actually generates samples — `mixer_fill` — which is now compiler-proven
bounded. This was worth doing *before* slice 7 (the realtime jump): had W026
found a violation, that would have been a latent bug worth fixing before
building a live callback on top of the same code path.

## Where it lives

`tools/tiny_lofi/` — the engine is **split into modules** (so it is easy to
maintain and grow, and so a Bang 9 tab can later drive it through `tl_lib.bsm`):

| File | Owns |
|---|---|
| `tiny_lofi.bpp` | standalone entry — `load`s the lib, runs the hardcoded test project |
| `tl_lib.bsm` | aggregator — `import`s stbsound + stbfilter, `load`s the modules, `tl_init` |
| `tl_channel.bsm` | the mixer channels (volume + a fixed Moog-then-rotary two-slot insert chain) — owns the Track array |
| `tl_timeline.bsm` | clips arranged over time + the offline mix/render — owns the Clip store |
| `tl_io.bsm` | export to WAV (import lands with the clips slice) |
| `tl_tools.bsm` | non-destructive edits — scissors (`tl_clip_split`) today; copy/paste/drag next |

The Moog filter graduated to `stb/stbfilter.bsm` (a reusable DSP cartridge,
installed via the `stb/*.bsm` glob in install.sh) with two consumers:
`examples/moog_demo.bpp` (the small read-it-in-one-sitting demo) and tiny_lofi.
Built and authored in the BangDev studio stack (Bang 9) per the studio
philosophy.

## Cross-references
- `docs/plans/audio_dsp_architecture.md` — the rotary/LFO/envelope layer map;
  names the exact "tiny_lofi takes the rotary as a channel insert" moment
  slice 5 above implements.
- `docs/plans/audio_stereo_dogfood.md` — the `@safe` dogfood backlog item.
- `docs/plans/legacy/audio_float_core_migration.md` /
  `docs/plans/legacy/audio_float_device_boundary.md` — why `stbmixer`'s
  `mixer_fill` and the device wire are float32 today (closed plans; this is
  the engine slice 6b would drive directly).
