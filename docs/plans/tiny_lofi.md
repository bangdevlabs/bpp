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
| `stb/stbfilter.bsm` | the first plugin — Moog ladder LP (SHIPPED). `moog_taps` returns all four ladder taps (-6/-12/-18/-24 dB/oct) in one b++ multi-value return; `moog_slope` picks one |

tiny_lofi is the application that arranges these into a timeline + mixer.

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
      s = sample from the track's active clip at this frame (0 if none)
      s = track.insert.filter(s)        // the Moog plugin, per track
      s = s * track.volume
      sum += s
    out[frame] = sum * master_volume    // soft-clamp to s16
```

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
2. **Recording source.**
   - *Import-first* (recommended): mini_synth already records to WAV; tiny_lofi
     imports those as clips and arranges/edits them. The editor + mixer + export
     is the core; the synth stays a separate instrument.
   - *Live-record*: capture the mini_synth voice into a track at the playhead in
     realtime. A follow-up slice once the timeline exists.

## Build order (vertical slices, each shippable + dogfoodable)

1. **Engine** — ✅ DONE (2026-06-18). The data model (Track[8] / Clip) +
   `tl_render()` + `tl_export()`. Validated by a hardcoded project: a 220 Hz
   sine on channel 0 (scissors-cut at 1.0 s, lossless) + a 330 Hz saw through
   the Moog at a 2-pole slope on channel 1 → a mixed WAV. The slope-4 variant
   renders byte-identical to a pre-refactor baseline, so the mix is proven.
   *This is the slice that tests the audio system — it does.*
2. **GUI shell** — window + the timeline grid (8 lanes, a time ruler, a
   playhead) + a transport bar (play/stop/export) + a mixer strip (8 faders +
   master). Read-only view of the model.
3. **Clips + tools** — import a WAV as a clip; draw clips as blocks; select;
   drag to move; scissors to cut at the playhead; Ctrl+C / Ctrl+V.
4. **Insert** — the Moog filter per track, cutoff/resonance in the channel
   strip; render runs each track through its insert.
5. **Live record** — capture the mini_synth voice into the armed track at the
   playhead (needs realtime or a record-then-place flow).
6. **Realtime playback** — the `@safe` callback streams the live mix; scrubbing
   and play-while-edit.

Slices 1–4 give a usable offline DAW: load sounds, arrange on 8 tracks, cut /
copy / paste, set volumes, filter, export a WAV. Slices 5–6 add live recording
and realtime feel.

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
