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
   Export, the buttons live) + a mixer strip (8 faders + master). Read-only
   view of the model; Play renders the project once and streams it through the
   audio device while the playhead follows the cursor. `./tl` opens the window;
   `./tl --render` keeps the headless WAV path for CI. **Open follow-up:** the
   faders are visual only — interactive write-back to `track.volume` plus a
   mute toggle per channel (see the per-track mute note above) is not built
   yet.
3. **Clips + tools** — partial. Scissors (`tl_clip_split`, non-destructive cut)
   ✅ DONE. **Open:** WAV import (`tiny_lofi.bpp` still generates its test
   project with `gen_sine`/`gen_saw` scaffolding, not real audio files), draw
   clips as blocks, select, drag to move, Ctrl+C / Ctrl+V.
4. **Insert (single)** — ✅ engine-level DONE: every `Track` already carries
   one hardcoded Moog-filter slot (`tk.flt_on` / `tk.flt`,
   `tl_channel_process` calls `moon_process`) and `tl_track_set` already takes
   cutoff/reso/slope. **Open:** the channel-strip UI doesn't expose
   cutoff/resonance controls yet (`tl_gui.bsm`'s mixer strip is fader-only).
5. **Insert CHAIN + the `stbrotary` graduation (2026-06-22 addition).**
   `tl_channel.bsm`'s own header comment already flagged this: "adding a
   second insert plugin later means giving the channel a small chain instead
   of one `flt` slot" — the Leslie/rotary ask is exactly that second plugin
   arriving. Two parts, sequenced:
   - **5a — generalize `Track`'s insert.** Replace the single `flt_on`/`flt`
     pair with a small fixed-size ordered array of typed insert slots (kind +
     handle), so a channel can carry **Moog → Rotary** (or any order) instead
     of one hardcoded type. `tl_channel_process` walks the chain instead of
     calling `moon_process` directly. Existing single-Moog channels keep
     working — a chain of length 1 is today's behavior.
   - **5b — extract the rotary's core out of `stbmixer`'s single global
     instance into a per-instance unit.** Today `_mx_rotary_mode` /
     `_mx_rotary_lfo` / `_mx_rotary_cur` / `_mx_rotary_tgt` are module-private
     **globals** — exactly one rotary for the whole mixer
     (`stb/stbmixer.bsm:642-649`). `stblfo`'s `lfo_new`/`lfo_set_rate`/
     `lfo_tick` (which the global rotary already calls) is already
     instance-based, so the extraction is mechanical: wrap the existing
     mode/glide/tick/pan-law logic into a `Rotary` struct + `rotary_new()` /
     `rotary_set_mode()` / `rotary_tick(r, l, r) -> (float, float)`, the same
     shape as `moon_filter`'s `moon_new`/`moon_process`. This **is** the
     `stbrotary` graduation `docs/plans/audio_dsp_architecture.md` names —
     "graduate `stbrotary` (Tier 2) when `tiny_lofi` takes the rotary as a
     channel insert (the 2nd consumer)." `stbmixer`'s own global rotary can
     either stay as a thin wrapper over the new instanced core (one instance
     owned by the mixer) or be retired in favor of every caller owning its
     own — decide when wiring, not before; not a contract change either way.
   - Both halves are useful independently of live recording — they apply to
     every existing audio-clip channel today, no instrument-track work
     required first.
6. **Live record — open mini_synth inside a channel.** The mini-Cubase vision
   from Part 2 above, now sized into two real sub-steps instead of one big
   jump (mini_synth has no synth engine of its own — it is a keyboard UI +
   record loop around `mixer_note_on`/`mixer_fill`, so neither step duplicates
   DSP code):
   - **6a — import-first (cheap, ships now).** Record a take in mini_synth
     (already does this today, SPACE → `sound_save_wav`) → WAV-import (slice
     3) drops it onto a track as a normal `Clip` → slice 5's insert chain
     (Moog + Rotary) processes it like any other audio. Zero new engine work;
     unblocks "plug the rotary on a recorded performance" immediately once
     slices 3 and 5 land.
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
| `tl_channel.bsm` | the mixer channels (volume + Moog filter insert + slope) — owns the Track array |
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
