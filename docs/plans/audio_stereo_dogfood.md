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

## Part B0 — How b++ transmits data, top-down (the holistic view)

The reason multi-return is a real edge isn't "everything is a word" — it's subtler
and cleaner. b++ has **exactly two base premises: word and float.** Everything else
is a *slice* (a narrower width) of one of those two. This is literally one bit.

### 1. The type code (the foundation) — `src/bpp_defs.bsm:212-275`
A type is a packed byte: **`(slice << 4) | base`**.
- `ty_base(ty) = ty & 0xF` — base family (low nibble).
- `ty_slice(ty) = (ty >> 4) & 0xF` — width (high nibble).
- **`is_float_type(ty) = ty & 1`** — **bit 0 IS the premise**: `0` = word, `1` = float.

Base families: `BASE_WORD 0x02`, `BASE_FLOAT 0x03`, `BASE_UWORD 0x0E`, `BASE_PTR
0x04`, `BASE_ARR 0x06`, `BASE_STRUCT 0x08`, `BASE_FN 0x0A`. **Only `BASE_FLOAT` has
bit 0 set.** Pointers, arrays, structs, fn-pointers, unsigned — all bit 0 = 0, i.e.
the **word family** (they're word-sized handles/addresses). So at the machine level
there are exactly **two kinds of value: float, and everything-else (word).**

Slices (high nibble): `SL_FULL 0`(64) / `SL_HALF 1`(32) / `SL_QUARTER 2`(16) /
`SL_BYTE 3`(8) / `SL_BIT..SL_BIT7 4-10`(1-7) / `SL_DOUBLE 11`(128 SIMD). Bit-slices
exist on **word only** ("float bitfields are not a thing"). So:
- **word** premise → full/half/quarter/byte/bit1-7 (integer widths) + uword + ptr/arr/struct/fn handles.
- **float** premise → full(f64)/half(f32)/quarter(f16)/double(128 SIMD).

A slice is the SAME premise at a narrower width — `: byte` is a word loaded/stored 8
bits at a time; `: half float` is a float loaded/stored as f32. The premise (bit 0)
never changes; only the load/store width does.

### 2. Transmission: two premises → two register banks → 8-in / 8-out
Because there are two premises, there are exactly **two register banks**: integer
(`x0..x7` a64 / SysV int regs x64) and float (`d0..d7` / `xmm`). Every value crossing
a call boundary is routed by `is_float_type` — the *one bit* — into its bank. The
argument classifier `cg_emit_call_arg_rearrange` (`bpp_codegen.bsm:732`) does exactly
this: walk each arg, `is_float_type(get_fn_par_type(...))` → float bank (`flt_idx++`)
or int bank (`int_idx++`). Slices ride their premise's bank (a `: byte` arg is in an
x-register, narrowed). **Returns mirror arguments**: up to **8 per bank**, classified
the same way — the "register-window transformer" (`multi_value_return_plan.md`):
a function reads ≤8 word + ≤8 float inputs from registers and hands back ≤8 word +
≤8 float outputs in registers, **never touching memory in between.**

### 3. Where this BEATS C++ (the answer to the original question)
The C ABIs cap *register* return hard:
- **x86-64 System V:** a returned aggregate is classified into eightbytes; **≤2
  eightbytes (≤16 bytes)** go in `RAX:RDX` / `XMM0:XMM1`, **anything bigger → MEMORY**
  (a hidden `sret` pointer, caller-allocated, callee writes through it). For floats:
  2 eightbytes = **4 floats** max in registers; 5+ → memory.
- **AArch64 AAPCS64:** a homogeneous float aggregate (HFA) of **≤4 members** returns
  in `V0-V3`; **>4, or any non-HFA >16 bytes → MEMORY** (`x8` indirect result).

So **both mainstream C ABIs cap float register-return at 4.** b++ banks **8** per
bank (a custom register window, explicitly beyond SysV's 2 — `backend_parity.md`).
**The win zone is 5-8 returned values:** in b++ they stay in registers; in C++ they
spill to memory (sret) — a store, a pointer hop, a reload, and a lost chance to keep
the chain register-resident.

This unlocks a **functional / stateless DSP style** that is clean AND register-
resident in b++ but memory-bound in C++:
```
// A pure Moog step: takes the input + 4 ladder states, returns the output + the
// 4 NEW states — no hidden mutable struct, no aliasing, trivially testable.
moog_step(in, s1, s2, s3, s4) -> (out, s1', s2', s3', s4')   // 5 floats out
```
5 float returns → b++ `d0..d4` (registers); C++ → MEMORY (sret) on BOTH ABIs. The
**Moog bass you want is the canonical case**: a stateless ladder whose new state
rides home in registers. This is the concrete "we beat C++ because of multi-return."

*(ABI facts from the SysV AMD64 psABI + AAPCS64 — well-established; I can validate a
specific case on godbolt if you want hard proof against a real compiler.)*

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

### Mono is a first-class path — a flag on the SAME structure (not a fork)
Mono is not legacy to be removed; it's a real signal path (a **Moog bass is mono**,
and mixes routinely mix mono + stereo channels). The rule: **one structure, gated by
a flag — never a parallel mono codepath.** A `Track` carries `stereo: bit`:
- **mono track** (`stereo = 0`): the insert/filter runs **once** (one filter state),
  producing a single `wet`; the channel then places it into the frame at its pan
  position → `(L, R) = pan(wet, θ)`. Cheaper (one Moog state, not two) — exactly what
  a mono bass wants.
- **stereo track** (`stereo = 1`): the insert runs **twice** (independent L/R state)
  → `(L_wet, R_wet)`, then per-channel gain/pan.

Both return the **same `(float, float)`** from the same `tl_channel_process`; the flag
only gates *how many filter passes* run inside. So `tl_render`, the `Track` struct,
the multi-return ABI path, and the mixer sum are **identical** for mono and stereo —
the flag is the only difference. (A mono *bus/submix* that should stay mono until the
master is the same idea one level up: a `stereo: bit` on the bus.) This keeps the
multi-return consumer real for both, and lets a Moog-bass mono channel be the cheap
common case without a second codepath to maintain.

### Phasing (each shippable + dogfoodable; the audio md5 baseline MOVES — re-baseline)
- **S1 — model: `tl_channel_process -> (float, float)` + pan, `tl_render` stereo
  sum. ✅ SHIPPED 2026-06-19.** `Track` gained `pan`/`stereo`/precomputed
  `gain_l`/`gain_r` (constant-power gains computed once in `_tl_apply_pan`, NOT
  per frame — pan is constant). `tl_channel_process` is the **first real product
  multi-return consumer** (`return l, r` → `d0/d1`; `tl_render` does `chl, chr =
  tl_channel_process(...)` and sums each side). The mono path is the gated common
  case (one filter pass + pan); the stereo-source two-pass path is reserved for
  S2 (no stereo source yet). Verified: compiles, **new stereo baseline md5
  `3e258737c9f0ef83193793b63eb7eed8`** (bass centre → L==R, lead pan 0.5 → R>L,
  both confirmed), tl_bench ~14.6 ms (~205× realtime). (`mini_synth` is still mono
  — it rides `stbmixer`, which goes stereo in S2.)
- **S2 — `stbmixer` per-voice pan + stereo `mixer_fill`. ✅ CORE SHIPPED 2026-06-19.**
  Per-voice pan gains (`_mx_voice_pan_l/r`, fixed-point /1024, **unity-centre =
  1024/1024 = identity** so existing consumers are byte-unchanged), a
  `mixer_set_voice_pan(slot, pan)` API (unity-centre balance law, computed once,
  no float in the hot loop), pan applied in all three voice paths (tone / sample /
  music), and — the structural change — **the mono collapse `(left+right)/2` is
  gone**: master gain + dirt (per-channel, shared decim counter) + clamp + write
  now run on L and R independently. So a **stereo sample WAV now plays in stereo**
  (it used to be averaged to mono) and tone voices are pannable. Verified:
  `test_mixer_sample` + `test_mixer_stream` pass (centre byte-identical); pan probe
  confirms centre → L==R (6400/6400), hard-right → L=0 / R live. `tiny_lofi`'s
  render is **unaffected** (it uses stbfilter/`tl_render`, not stbmixer — md5
  stays `3e258737…`).

  **S2b (pending) — the consumer wiring:**
  - `mini_synth` "gains real stereo" is now a **UX choice** (the capability is
    there): pan-by-key (keyboard tracking — can be disorienting), a master/pan
    control, or stay centre. Interactive, so it needs an ear-check, not a headless
    md5. *Decide the UX before wiring.*
  - `tl_gui` pan knobs (GUI — needs the window to verify).
  - the stereo-source two-pass branch in `tl_channel_process` — deferred until a
    stereo *source* (stereo clip import) exists; no consumer yet.
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
