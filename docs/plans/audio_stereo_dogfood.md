# Plan — stereo audio + dogfooding b++ features in the audio stack

**Status:** proposed 2026-06-19. Two coupled goals the user surfaced: (1) the audio
tools barely use b++'s distinctive features, so the language isn't being
stress-tested and codegen wins on those features (e.g. multi-return) optimise
something almost nobody uses; (2) the DAW is fake-mono and needs real stereo.
Stereo is the fix for BOTH — its natural shape is multi-return `(float, float)`,
giving that feature a real product consumer while making the DAW genuinely stereo.

---

## Part A — The audit (measured 2026-06-19)

Feature usage across `tools/tiny_lofi`, `tools/mini_synth`, `tools/moon_filter`,
`stb/stbfilter`, `stb/stbmixer`, `stb/stbsound`, `stb/stbaudio`:

| b++ feature | hits | note |
|---|---|---|
| `struct` | 4 | Moog / Track / Clip / … — used well |
| `: float` slices | 43 | used well (DSP is float throughout) |
| **multi-return `-> (…)`** | **1** | ONLY `moog_taps` (internal 4-tap) — no product use |
| **`@safe`** | **0** | realtime audio is `@safe`'s killer use case — and it's unused |
| **SIMD / `: double` / `vec_*` / autovec** | **0** | the mix loop is a textbook data-parallel candidate, all scalar |
| newtype (`struct X as`) | 0 | no `WorldPx`-style domain typing (e.g. `Sample as float`) |
| `u_word` family | 0 | — |
| enum | 0 | voice kinds / bus ids are bare ints |
| func-type (`func(...) ->`) | 0 | callback registries untyped |

**Implications (Rule 35 / 20 / 28):**
- **multi-return has one consumer** → fails Rule 20's "earns its keep" bar. Its real
  killer use case (stereo `(L,R)` in registers) was never built; the feature is
  *orphaned*, not wrong.
- **`@safe` absent from realtime paths** → the one place the project sells `@safe`
  (no malloc/IO in the audio callback) isn't using it. (`tiny_lofi` is offline-
  render so it's exempt, but `stbmixer`'s fill path + `mini_synth`'s realtime loop
  are exactly the contract.)
- **scalar mix loop** → the autovectoriser (a whole shipped arc) has no audio
  consumer, despite the per-frame channel sum being the canonical map-reduce.

**Dogfood backlog (prioritised):**
1. **Stereo via multi-return `(float, float)`** — Part C. Headline; fixes the
   measurement gap + ships a real feature.
2. **`@safe` on the realtime mix path** (`mixer_fill` / `mini_synth` loop) — proves
   the no-alloc contract on the exact code it was built for.
3. **autovec the per-frame channel sum** (or a block-planar fast path) — gives the
   autovectoriser its first audio consumer (see Part B).
4. (later) enum for voice-kind/bus, newtype `Sample`/`Gain`, as they earn it.

---

## Part B — Study: how professional audio does stereo (C++), and can we do better?

### What pro plugins actually do
- **Block (buffer) processing, not per-sample.** Hosts (VST3 / AU / CLAP) call
  `processBlock(buffer, nFrames)` with a block of samples. The per-sample work is
  a loop *inside* one call — never a function call per sample. This amortises call
  overhead and, crucially, **enables vectorisation**.
- **Planar channels, not interleaved.** Buffers are `float** channels` — a separate
  contiguous array per channel (`left[]`, `right[]`). JUCE `AudioBuffer<float>`,
  VST3, CLAP all expose planar `getWritePointer(ch)`. Planar is what lets the inner
  loop run SIMD-wide over one channel at a time. Interleaved (`L,R,L,R`) is only the
  *I/O* format (the DAC); DSP converts to planar.
- **SIMD across the axis that's independent.** An IIR/ladder filter (our Moog) has a
  serial feedback dependency *within a channel across samples* — you can't SIMD
  successive samples. So pros SIMD across the **independent** axis instead: the two
  channels together (2-wide L|R), or many **voices** together (N-wide), or process
  blocks of a non-recursive stage.
- **Per-sample `(L,R)` tuples are the teaching model**, not the perf model. Clear to
  read; you never ship it in a hot inner loop in C++ because the call overhead +
  lack of vectorisation costs an order of magnitude.

### The b++ angle — where multi-return genuinely wins, and where it doesn't
- **API ergonomics: b++ multi-return `(float,float)` is *cleaner* than typical C++.**
  C++ per-sample stereo is usually out-params (`void tick(float& l, float& r)`) or a
  `std::pair`/struct that may or may not be register-returned depending on ABI. b++
  returns `(L,R)` in two registers (`d0,d1`) — no struct, no memory, no out-param
  aliasing. So at the *clarity* axis we are already better than the common C++ shape.
- **Raw perf: the crown is still block-planar + SIMD**, which b++ can ALSO do — the
  autovectoriser + `: double` (128-bit, 2×f64 / 4×f32) already exist. The honest
  picture:
  - per-sample `(L,R)` multi-return = clean + dogfoods the feature, but *serial*
    (one frame at a time). Perfect when perf isn't the bottleneck.
  - block-planar + autovec = the pro-audio perf path; the model function stays
    readable, a fast path vectorises the non-recursive stages.
- **A genuinely novel "do better" idea (later, speculative):** a stereo frame is two
  floats — exactly a `: double` lane pair. A stereo filter could process L|R as one
  2-wide SIMD op (the two channels are independent), so `(L,R)` isn't just ergonomic
  but *one instruction*. That would make b++ stereo both the cleanest API AND
  SIMD — a real edge. Needs the codegen to fuse a `(float,float)` return into a
  packed lane pair; flagged as a research item, not MVP.

### Verdict for our stack
The DAW runs at ~229× realtime — **perf is not the bottleneck**, *clarity and
feature coverage are*. So: adopt **per-sample `(float,float)` multi-return** for
stereo now (cleanest API, dogfoods the feature, beats the common C++ shape on
ergonomics). Keep block-planar + autovec as the documented perf path for when a
real workload (many tracks / live monitoring) needs it. Revisit the `: double`
lane-pair fusion as a research spike if/when we want the "cleanest AND fastest"
claim.

*(If you want this part sourced harder, I can deep-research a real open-source
plugin's DSP — Surge, Vital, or a JUCE dsp module — to validate the block-planar +
per-channel-SIMD claims against actual shipping code.)*

---

## Part C — The stereo design

Per-channel pan, true stereo sum, multi-return `(float, float)` for the per-frame
path. The layers, bottom-up:

```
stbmixer   per-voice pan → mixer_fill writes a real stereo sum (today: L==R mono)
   ↑
stbfilter  (mono filters stay mono; a stereo insert processes L,R — two states)
   ↑
tl_channel tl_channel_process(tr, dryL, dryR) -> (float, float)   // (L,R)
   ↑
tl_timeline tl_render sums (L,R) per channel into the stereo out_buf
```

- **`tl_channel_process(tr, dry_l, dry_r) -> (float, float)`** — the headline
  multi-return consumer. Applies the insert + per-channel `vol` and `pan` (constant-
  power pan: `l = in * cos(θ)`, `r = in * sin(θ)`, θ from pan ∈ [-1,1]). Returns
  `(L, R)`.
- **`tl_render`** — accumulate `(mL, mR)` per frame across channels; write distinct
  L/R to `out_buf` (kills the `// R (mono)` line).
- **`Track`** gains a `pan: float` field; the mixer strip (`tl_gui`) gets a pan knob.
- **`stbmixer`** — add per-voice pan so `mixer_fill` produces a genuine stereo sum
  (today line 562: "Left and right carry the same mono sum"); `mini_synth` then
  plays in stereo for free.
- **Source signal** stays mono per clip/voice (a mono sample panned into the stereo
  field) — standard; true stereo *sources* (a stereo WAV clip) are a later step.

### Phasing (each shippable + dogfoodable; the audio md5 baseline MOVES — re-baseline)
- **S1 — model: `tl_channel_process -> (float, float)` + pan, `tl_render` stereo
  sum.** The offline render becomes real stereo. **New audio baseline** (the mono
  `7ee452e7…` md5 is intentionally superseded). Verify by ear + the new md5, and —
  per the user's note — render via **both `tiny_lofi --render` AND `mini_synth`** to
  measure audio across tools, not one path.
- **S2 — `stbmixer` per-voice pan + stereo `mixer_fill`.** `mini_synth` gains real
  stereo; `tl_gui` pan knobs.
- **S3 — `@safe` on the realtime mix path** (mixer fill / mini_synth loop) — dogfood
  backlog #2; proves no-alloc on the realtime path.
- **S4 — (optional perf) autovec / block-planar mix**, or the `: double` lane-pair
  stereo spike — only if a real workload needs it (measure first).
- **THEN** the inliner arc's Inc 5 (multi-value-return splice) finally has a *real,
  hot, product* multi-return target (`tl_channel_process`) to inline + measure —
  instead of the orphaned `moog_taps`.

---

## Why this reorders the inliner arc

The inliner's remaining lever (multi-return inlining + the nested-inline
architecture) was about to optimise `moog_taps` — a feature with one contrived
consumer. Building stereo first means: (a) multi-return earns its keep with a real
consumer, (b) `tl_bench` becomes a representative stereo workload, (c) Inc 5 then
optimises code the product actually runs. Dogfood, then optimise — not optimise an
orphan.

## Cross-references
- `docs/manual/stb++_lib.md` Cap 29–31 (audio stack), Cap 39/ autovec notes.
- `docs/plans/inliner_arc.md` — Inc 5 (multi-return splice) waits on a real consumer.
- `docs/plans/tiny_lofi.md` — the DAW build order (stereo slots into the mixer slice).
- Rule 35 (games/tools as infra stress test), Rule 20/28 (a feature earns its keep
  via a real consumer), Rule 39 (explicit vs implicit SIMD — the block-planar path).
