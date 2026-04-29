# B++ Standard Library — Deep Reference

The library that ships with B++. This is the deep-dive companion to
`how_to_dev_b++.md` — there the language is introduced and the arsenal
modules (`bpp_*.bsm`) are described as part of the prelude; here every
`stb*.bsm` module is documented in full, alongside the bundled tools
(Bang 9 IDE, ModuLab, level_editor, mini_synth) and the diagnostic
catalog.

This book is consulted when:

- you are writing a game / tool that uses an stb module and need the
  full API reference, the intent-and-scope, and the caveats
- you are debugging a behaviour from one of the stb modules and want
  the design notes that explain "why is it this way"
- you are an agent picking up the codebase and need full context
  beyond the K&R tour in `how_to_dev_b++.md`

**The library is intentionally Plan 9 / man-page in style**: every
module gets a paragraph of intent, an API table, decision points, and
a "what this chapter does NOT cover" note. No marketing copy, no
buzzword-heavy abstractions — the same flavour as the rest of B++.

## Layout

| Part | Contents |
|------|----------|
| I — Tools | Bang 9 IDE, embedded tools (ModuLab, level_editor, mini_synth) |
| II — Diagnostics | Compiler warnings + errors catalogue |
| III — Audio | stbaudio (device), stbmixer (8-voice polyphonic), stbsound (file formats) |
| IV — Input + Window | stbinput (keyboard/mouse), stbwindow (native dialogs), stbgame (loop) |
| V — Drawing | stbdraw (CPU framebuffer), stbui (widgets), stbimage (PNG decode) |
| VI — Sprite + Tile | stbpal (palettes), stbtile (tilemaps), stbforge (animations) |
| VII — Game systems | stbecs (entities), stbphys (physics), stbpath (A*) |
| VIII — Engine plumbing | stbpool (pool allocator), stbcol (color math), stbasset (handle-based assets) |
| IX — GPU | stbrender (textures + sprites + palette LUT) |

The chapters keep their original numbering from `how_to_dev_b++.md`'s
Part VI so cross-references in the rest of the codebase remain valid.
The original `how_to_dev_b++.md` keeps a one-paragraph appendix per
module pointing back here.

---

# Part VI — Ecosystem

Bang 9, tools, game scripting, roadmap.

---

## Cap 26 — Bang 9 IDE

*Depends on: —*
*Source: legacy_docs/bang9_vision.md*
*Status: COMPLETE — 2026-04-27*

Bang 9 is a DAW for game development — an integrated environment built in B++ that hosts creative tools as panels inside a single project window, the same way Ableton Live hosts synthesizer plugins.

### The mental model

```
Ableton Live (the host)             Bang 9 (the host)
├─ project + timeline               ├─ project + file tree
├─ MIDI editor (native)             ├─ code editor (native, acme-style)
├─ transport: play/stop/record      ├─ transport: build/run/debug
└─ hosts VST/AU plugins             └─ hosts tools/ as panels

Serum, Kontakt (the plugins)        modulab, mini_synth, level_editor (tools)
├─ runs STANDALONE (own window)     ├─ runs STANDALONE (own window)
└─ runs INSIDE the DAW (as plugin)  └─ runs INSIDE Bang 9 (as panel)
```

This is not decoration — it is the architectural commitment. Every tool in `tools/` must satisfy the dual-mode contract: run standalone, embed cleanly in Bang 9.

### Window layout

```
┌──────────────────────────────────────────────────────┐
│ File  Edit  View  Build  Run  Help                   │  ← Menu bar
├──────────────────────────────────────────────────────┤
│ [Project] [Sprites] [Music] [Levels] [Code] [Run]    │  ← Tab strip
├──────────────────────────────────────────────────────┤
│                                                       │
│                   ACTIVE PANEL                        │  ← Panel body
│                                                       │
├──────────────────────────────────────────────────────┤
│ project: games/pathfind       Bang 9 v0.3            │  ← Status bar
└──────────────────────────────────────────────────────┘
```

| Tab | Role | Source |
|-----|------|--------|
| Project | File tree + asset overview | native to Bang 9 |
| Sprites | Pixel art editing | embeds `tools/modulab/` |
| Music | Audio synthesis | embeds `tools/mini_synth/` |
| Levels | Tilemap editing | embeds `tools/level_editor/` |
| Code | Acme-style text editor | native to Bang 9 |
| Run | Build output + running game | native to Bang 9 |

### Who Bang 9 is for

**Primary — artists and designers.** The target user never opens a terminal. They download Bang 9 as a single installer, edit sprites in the Sprites panel, compose music in the Music panel, paint levels in Levels, and hit Run. No compiler invocation, no test suite, no dependency graph.

**Secondary — programmers working with artists.** They use `bpp` directly from their own editors (Emacs, VSCode, Vim) and commit to the same repo the artist's Bang 9 writes to. Bang 9 is optional for them — useful for inspecting the visual state of the project.

### Relationship to B++ (open vs. proprietary)

B++ (the compiler, stdlib, tools, debugger, docs) is open source, Apache 2.0. Bang 9 is a proprietary commercial product built on top. This mirrors the audio world: Audio Unit / VST are open standards; Ableton and Logic are proprietary products that implement those standards.

Three non-negotiable guardrails:
1. **Standalone tools stay viable forever.** An artist who runs `./modulab` directly gets a fully functional sprite editor. Bang 9's value is integration, not exclusivity.
2. **B++ is not a hostage.** No language feature is introduced solely for Bang 9. All changes go through the public process in `GOVERNANCE.md`.
3. **Transparency.** Users who download B++ see that Bang 9 exists and is commercial. No dark patterns.

### Build + run

When the user hits **Build** (Ctrl+B), Bang 9 invokes `bpp` as a subprocess, captures stdout/stderr, and renders output in the Run panel. Compiler errors are clickable — they jump to the Code panel at the offending line (using the E/W source location system from Cap 28).

When the user hits **Run** (Ctrl+R), Bang 9 builds if needed, then executes the resulting binary. The game runs in a separate window. **Ctrl+F5** launches under `bug` (the debugger, Cap 49).

### Project layout convention

```
games/<game_name>/
├── <game_name>.bpp              ← game entry point
├── assets/
│   ├── sprites/
│   │   ├── hero.modulab.json    ← modulab canonical (editor round-trip)
│   │   └── hero.json            ← stbsprite format (game loads this)
│   ├── levels/
│   │   └── level1.level.json    ← level_editor format
│   └── sounds/
│       └── coin.synth.json
└── build/
    └── <game_name>              ← binary output
```

Bang 9 reads and writes this layout; games read it at runtime through the stb modules. The same files power both.

### The 1.0 milestone

Bang 9's first major milestone is a **complete, shippable game produced entirely within the ecosystem** — the `games/pathfind/` project, with intro scene, 3 levels authored in level_editor, background music from mini_synth, and sprites polished in modulab. Every step of production inside Bang 9, zero external tools.

### What this chapter does NOT cover

The embed protocol (how tools expose their three-function API so Bang 9 can host them) is Cap 27. The diagnostic display system that powers clickable compiler errors is Cap 28.

---

## Cap 27 — Tools (modulab, level_editor, mini_synth) — embed pattern

*Depends on: Cap 26*
*Source: legacy_docs/bang9_tool_embed.md + legacy_docs/bang9_factory.md + legacy_docs/bang9_live_preview.md*
*Status: COMPLETE — 2026-04-27*

Every tool in `tools/` satisfies a three-function contract that lets it run both standalone (its own window) and embedded inside Bang 9 (as a panel drawing into a sub-rect).

### The three-function contract

```c
// One-time setup. Called once when the panel is mounted.
void <tool>_lib_init(initial_arg, canvas_n);

// One frame. Called by the host every frame after game_frame_begin().
// px/py/pw/ph is the panel rect in window-absolute coordinates.
// Returns 1 if the tool requested quit (e.g. ESC), 0 otherwise.
<tool>_lib_frame(px, py, pw, ph);

// Cleanup. Called when the panel is unmounted or Bang 9 exits.
void <tool>_lib_shutdown();
```

Standalone wrappers call the same three functions from their own `game_init` / game loop, passing `(0, 0, SCREEN_W, SCREEN_H)` as the rect.

### Layout adaptation

Every tool has hardcoded layout coordinates that assume `origin = (0, 0)`. The embed refactor adds four panel-rect globals to the `_lib.bsm` file:

```c
static extrn _ml_ox, _ml_oy, _ml_pw, _ml_ph;
```

`*_lib_frame` sets these from its arguments at frame top. Every hardcoded position in the frame body shifts:

| Before | After |
|--------|-------|
| `draw_rect(12, 64, ...)` | `draw_rect(_ml_ox + 12, _ml_oy + 64, ...)` |
| `SCREEN_W - 96` | `_ml_ox + _ml_pw - 96` |
| `SCREEN_H - 18` | `_ml_oy + _ml_ph - 18` |

Mouse coordinates **do not need translation** — `mouse_x()` / `mouse_y()` return window-absolute values, and after the shift the tool's internal comparison coordinates are also window-absolute.

### Panel size guard

Each panel function checks whether the host rect is large enough for the tool's layout. If not, it renders a placeholder:

```c
static void _panel_sprites(x, y, w, h) {
    if (w < 800 || h < 600) {
        _placeholder(x, y, w, h, "Sprites (ModuLab)",
                     "Resize Bang 9 to at least 1024×768");
        return;
    }
    if (_sprites_initialized == 0) {
        modulab_lib_init(_find_first_sprite_in_project(), 16);
        _sprites_initialized = 1;
    }
    modulab_lib_frame(x, y, w, h);
}
```

### Lazy init + lazy audio

Each tool tracks its own `_initialized` flag. The host calls `*_lib_init` on first panel activation, not at Bang 9 startup. This keeps Bang 9's cold-start fast even when multiple heavy tools are registered.

For mini_synth (the audio tool): `stbaudio` streams open on first activation of the Music tab. The tool exposes `mini_synth_lib_pause()` / `mini_synth_lib_resume()` so the host can mute it when switching to another tab.

### Bang 9 import chain

After all three tools are embedded, `bang9.bpp` imports:

```c
import "../tools/modulab/modulab_lib.bsm";
import "../tools/level_editor/level_editor_lib.bsm";
import "../tools/mini_synth/mini_synth_lib.bsm";
```

Each tool's global state lives in its own `.bsm` file — no shared namespace. Collision check: `grep -h '^auto\|^extrn' tools/{modulab,level_editor,mini_synth}/*.bsm | sort | uniq -d` should return nothing.

### The project convention (game factory)

Bang 9 is also a **game factory** — it enforces a consistent project layout so that tools, the IDE, and the game runtime all refer to the same files:

- Sprites: `assets/sprites/<name>.json` — modulab writes this on Save via `mlab_save_both` (writes both `.modulab.json` for round-tripping and `.json` for the game).
- Levels: `assets/levels/<name>.level.json` — level_editor format, read at runtime via `json_parse`.
- Sounds: `assets/sounds/<name>.synth.json` — mini_synth output (future).

The game code loads these paths at runtime through the stb modules, never hardcodes asset data. This is what makes the edit-compile-run loop work inside Bang 9 without the game having any knowledge of the IDE.

### Phase 4 — live preview (design, not yet shipped)

The final UX piece is seeing the game running **inside** Bang 9 while editing — edit a sprite, tab to preview, see it move. The design: Bang 9 embeds the game as a subprocess, the game renders to a shared memory region, Bang 9 blits that region into the Run panel every frame. Edits to asset files trigger an incremental reload (the asset is already a file; the game's next `load_sprite` call picks it up). Design doc at `legacy_docs/bang9_live_preview.md`; implementation gated on Phase 1-3 stability.

### What this chapter does NOT cover

The `bug` debugger's embed in a future Debug tab is a separate design. This chapter covers the three creative tools only.

---

## Cap 28 — Diagnostics (warnings + errors)

*Depends on: —*
*Source: docs/warning_error_log.md*
*Status: COMPLETE — 2026-04-27*

The compiler emits two kinds of diagnostics: **errors** (fatal — compilation stops) and **warnings** (non-fatal — compilation continues with a caution). All codes are numeric: `EXXX` for errors, `WXXX` for warnings. The canonical reference lives at `docs/warning_error_log.md`; this chapter explains the system design and the most important codes.

### How diagnostics are emitted

Every diagnostic goes through `bpp_diag.bsm`, which formats the message with source location: file path, line number, the source line itself, and a caret under the offending token. The rule is: **every new error or warning must show source location**. No naked message strings.

```
src/game.bpp:42:15: E232 — silent float→int at assignment
    auto x;
    x = 3.14;
              ^
  Hint: annotate `: float` or use an integer literal.
```

Three pre-lexer errors (`E001`, `E002`, `E222`) can only show a filename because the lexer has not yet run; all other diagnostics carry a full `tok_pos`.

The active diagnostic count and location status:

```
Total diagnostics:  25 active + 2 reserved
With source location: 19  ✅  (file:line + source + caret)
With filename only:    3  ⚠️  (import resolver — pre-lexer)
Reserved:              2  (W017, W014)
```

### Errors (fatal)

| Code | Trigger | Notes |
|------|---------|-------|
| E001 | Module imports itself | Recursive import detected |
| E002 | File not found in search paths | Import or load path missing |
| E050 | Struct field type mismatch | Field assigned wrong type |
| E053 | Invalid type annotation | Bad `: Type` suffix |
| E101 | Unknown type in annotation | `: UnknownType` |
| E102 | Invalid start of expression | Parser primary fallthrough |
| E103 | Keyword used as expression | e.g. `auto` as value |
| E104 | Unexpected token (generic) | Parser fallthrough |
| E105 | `main()` defined in `.bsm` | Entry point must be in `.bpp` |
| E120 | General parse failure | Syntax error with location |
| E201 | Call to undeclared function | Forward reference missing |
| E221 | Duplicate function definition | Same name in two modules |
| E222 | Circular import dependency | Import cycle detected |
| E230 | `static const` at file scope | Produces silent 0 at runtime |
| E232 | Silent float→int at assignment | `auto x; x = 3.14;` drops IEEE bits; annotate `: float` |
| E233 | Float arg passed to int param | Promoted from W002 in 0.23 |
| E240 | Int passed to float param | Annotate source `: float` or use float literal |
| E242 | Shift count out of range | Count > 63; hardware masks to 6 bits |
| E243 | Pointer compared to non-zero literal | `if (ptr == 42)` is almost always a bug |
| E244 | Float literal in int context | Array index or shift count cannot be a float |
| E245 | Array element type conflict | Two different inferred elem types for same `TY_ARR` |

### Warnings (non-fatal)

| Code | Trigger | Notes |
|------|---------|-------|
| W002 | ~~Implicit float-to-int~~ | **RETIRED** — promoted to E233 in 0.23 |
| W003 | Wrong argument count | Call with mismatched arg count |
| W005 | Unreachable code | Code after `return` statement |
| W010 | Narrowing conversion | Float narrowed to half/quarter |
| W011 | Precision loss | Word to half float |
| W012 | `&` in FFI argument | Address-of passed to extern/call |
| W013 | `@base` mismatch | Function annotated base but impure |
| W020 | Static cross-module call | Calling `: static` function from another module |
| W021 | Switch not exhaustive | Value switch without `else` arm |
| W025 | Public param without hint in float arithmetic | Annotate `: float` or `: word` — see Rule 13 |
| W026 | Phase annotation violated | Function annotated `@time` reaches a HEAP call, etc. |

### W026 — the effect lattice guard

W026 fires when a function's explicit phase annotation is violated by its transitive call graph. The lattice is:

```
BASE < REALTIME < HEAP < {IO, GPU} < SOLO
```

A function annotated `@time` must not call anything that reaches HEAP (i.e., no malloc). A function annotated `@io` permits HEAP (allocation is ubiquitous in I/O scaffolding) but must not be called from a `@time` context.

Unknown externs default to `PHASE_AUTO`, so only **known-to-be-incompatible** paths fire W026. This makes the check gradual: new extern calls are safe by default and opt into classification over time.

### Runtime asset-load diagnostics

These are **not compiler diagnostics** — they are printed by stb loaders at runtime when an asset fails to load. They follow the format `<module>: '<path>': <reason>` so missing or malformed assets are visible immediately.

```
stbsound: 'assets/sounds/coin.wav': unsupported PCM bit depth 24 (need 8, 16, 24, or 32)
stbimage: 'assets/sprites/hero.png': not a valid PNG or unsupported chunk layout
stbfont:  'assets/fonts/ui.ttf': no 'head' table (not a valid TTF)
```

The loader prints to stderr and returns 0 (null handle). Callers in `stbasset.bsm` propagate the null handle; the stb modules never wrap or re-log.

### Adding a new diagnostic

1. Pick the next available code in `docs/warning_error_log.md`.
2. Add the emission call in the appropriate pass file (`bpp_parser.bsm`, `bpp_validate.bsm`, or `bpp_dispatch.bsm`).
3. Use `diag_error(tok_pos, EXXX, message)` or `diag_warn(tok_pos, WXXX, message)` — never bare `print_msg`.
4. Add an xfail test: `// xfail: EXXX` on line 1 of a new `tests/test_EXXX.bpp`.
5. Update `docs/warning_error_log.md` with the new row.

The rule is firm: **no new diagnostic ships without a source location and an xfail test**.

### What this chapter does NOT cover

The diagnostic formatting internals (column calculation, caret alignment, ANSI color codes) live in `bpp_diag.bsm` and are not user-facing. The xfail test mechanism is explained in Cap 20 (Testing).

---

## Cap 29 — Audio output (stbaudio)

*Depends on: Cap 2 (auto-injected prelude), Cap 15 (QG discipline)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbaudio.bsm` is the bottom of the audio stack — it opens the device,
runs the `@time` callback, and exposes one session-level volume knob.
Polyphonic mixing, WAV loading, and multi-bus routing live one level up
in `stbmixer.bsm` (Cap 30) and `stbsound.bsm`.

B++'s audio stack is the same split-layer design as every other stb module:
a public portable API (`stb/stbaudio.bsm`) sits on a platform implementation
(`src/backend/os/<os>/_stb_audio_<os>.bsm`). The portable file never touches
CoreAudio / ALSA / WASAPI directly — it calls the platform primitives via
target-suffix resolution.

On macOS the platform layer drives CoreAudio's AudioQueue with dlopen'd
function pointers. Linux ALSA lands when ELF dynamic linking ships. Both
backends expose the same public contract, so user code is zero-touch when
you cross-compile.

### §29.0 — Three shapes of volume (and why)

Volume is a **session property**, not a per-call property. You set it once,
and every subsequent tone / push / stream respects the choice. Three APIs
sit on the same underlying amplitude knob:

#### The three shapes at a glance

| Shape | API | Use case |
|-------|-----|----------|
| raw amplitude | `audio_set_amplitude(amp)` | Fine-grained control over the s16 peak (0..32767). Used when you need exact bit-level precision. |
| linear percent | `audio_set_volume(pct)` | UI sliders (0..100). Matches iTunes / Spotify / macOS volume HUD — every major consumer app uses linear-on-amplitude. |
| decibels | `audio_set_volume_db(db)` | Plugin UIs, mixing consoles. Matches VST / AU / CLAP parameter ranges (typically 0..-60 dB). |

All three APIs read from the same `_aud_amplitude` word. You can mix them
freely — `audio_set_volume(50)` followed by `audio_get_volume_db()` returns
approximately -6 dB.

#### Decision table

| Situation | API |
|-----------|-----|
| UI slider (0..100 knob) | `audio_set_volume(pct)` |
| Mixer fader / plugin param | `audio_set_volume_db(db)` |
| Exact bit-level amplitude | `audio_set_amplitude(raw)` |
| Toggle mute | `audio_mute()` / `audio_unmute()` |
| "Loudness in the test is too loud" | `audio_set_volume_db(0 - 24)` |

#### Why three shapes

Same reason B++ has three string shapes: **different callers think in
different units**. A game dev setting HUD volume thinks in percent. A
mixing engineer thinks in dB. A codec test that must hit an exact sample
value thinks in raw amplitude.

This is not novelty — every serious audio framework ships at least two
(raw + dB). CoreAudio's AudioQueueSetParameter takes raw; CoreAudio's
AVAudioUnit takes dB. PortAudio ships both. SDL_Mix ships linear 0-128.
You get to pick the one that matches the domain instead of translating
units yourself.

### §29.1 — `audio_set_amplitude` / `audio_get_amplitude`

```c
void audio_set_amplitude(amp)@io;   // clamps to 0..32767
audio_get_amplitude()@io;           // returns current peak
```

`amp` is a **signed-16 peak value**: 0 is silent, 32767 is the s16 maximum
(hard clip on most DACs). The default used by `audio_tone_test` is 8000
(≈24% of peak, ≈-12 dB). Values outside the range are clamped, not
wrapped — negative becomes 0, anything over 32767 becomes 32767.

```c
audio_set_amplitude(16384);   // exactly -6 dB
audio_set_amplitude(0);       // silence (use audio_mute for toggleable)
audio_set_amplitude(99999);   // clamped to 32767
```

### §29.2 — `audio_set_volume` / `audio_get_volume`

```c
void audio_set_volume(pct)@io;   // 0..100, clamped
audio_get_volume()@io;           // returns 0..100
```

Linear percent-of-peak knob. 100 maps to amplitude 32700 (deliberately
0.2% below s16 max so a full-scale setting has a sliver of headroom for
any downstream gain stage). 0 maps to 0 (silent).

The scale is **linear on amplitude, not on perceived loudness** — because
that's what every consumer audio slider does. For perceptually-linear
volume, use the dB API instead.

```c
audio_set_volume(50);    // ~16350 amp, about -6 dB
audio_set_volume(25);    // ~8175 amp — basically the default 8000
audio_set_volume(10);    // ~3270 amp, background level
```

**Watch out:** because the scale is linear, the top half of the slider
barely changes perceived loudness (100% vs 50% sounds like "loud vs
slightly quieter"). That's a UX artefact every audio app in the last 20
years has shipped with — users are used to it, so don't try to "fix" it
without good reason.

### §29.3 — `audio_set_volume_db` / `audio_get_volume_db` (plugin-oriented)

```c
void audio_set_volume_db(db)@io;   // typical range 0..-60
audio_get_volume_db()@io;          // returns 0..-96
```

Decibels map logarithmically to amplitude: each -6 dB halves the
amplitude exactly. This is the unit real audio professionals think in —
every plugin parameter, every mixer fader. Use this if your code is
going to sit next to other plugin-style software, or if users are going
to type numbers into a UI.

```c
audio_set_volume_db(0);         // full scale — 32700 amp
audio_set_volume_db(0 - 6);     // half    — 16350 amp
audio_set_volume_db(0 - 12);    // quarter — 8192 amp (≈ old default)
audio_set_volume_db(0 - 24);    // 1/16   — 2050 amp, comfortable
audio_set_volume_db(0 - 36);    // 1/64   — 510  amp, quiet background
audio_set_volume_db(0 - 60);    // 1/1024 — 32 amp, nearly silent
audio_set_volume_db(0 - 96);    // silence (below s16 resolution)
```

**Why a negative literal needs `0 - 36` instead of `-36`:** unary minus
on integer literals is parsed as a subtraction expression in the current
grammar. The workaround is either `0 - 36` or storing the constant in a
variable first (`auto dim = 0 - 36; audio_set_volume_db(dim);`). This
will be fixed when the lexer learns the unary-minus-on-literal shortcut.

#### Under the hood: faking `pow()` without a math runtime

B++'s runtime has no `pow()` yet, so `audio_set_volume_db` does the
dB→amplitude conversion with integer-only arithmetic:

1. Separate the dB value into full-halvings (`db / 6`) and remainder
   (`db % 6`). Each -6 dB is exactly one bit-shift right of the
   amplitude, so full-halvings lowers for free.
2. For the 0..-5 dB remainder, look up a 6-entry table of ratios-out-of-
   1000 (`1000, 891, 794, 708, 631, 562`) — these are `10^(-n/20) * 1000`
   rounded to nearest integer.
3. Combine: `amp = (32700 >> halvings) * remainder_ratio / 1000`.

Error is < 0.3% on amplitude across the whole 0..-96 dB range. The
human just-noticeable-difference for loudness is about 1 dB (≈12% amp);
0.3% is inaudible. When the runtime ships `pow()` the helper can swap
to direct math without changing the API.

### §29.4 — Mute (`audio_mute` / `audio_unmute` / `audio_is_muted`)

```c
void audio_mute()@io;
void audio_unmute()@io;
audio_is_muted()@io;           // 0 or 1
```

Mute remembers the previous amplitude and restores it on unmute. Safe to
call repeatedly — mute on an already-muted session is a no-op. Typical
pattern:

```c
if (key_pressed(KEY_M)) {
    if (audio_is_muted()) { audio_unmute(); }
    else                  { audio_mute(); }
}
```

The underlying implementation uses two words (`_aud_is_muted` flag + saved
amplitude) so the sentinel is unambiguous — user can legitimately set
amplitude to 0 without the mute toggle getting confused about state.

### §29.5 — Default amplitude and init

When a program first calls `audio_tone_test` (or any other signal
generator) without having configured volume, the platform layer seeds
`_aud_amplitude = 8000` (≈-12 dB). This default is applied **only if the
user has not set a volume beforehand** — the seeder checks for zero and
bails out otherwise, so a pre-session `audio_set_volume_db(0 - 24)` call
sticks through tone_start.

```c
main() {
    audio_set_volume_db(0 - 24);   // applies first
    audio_tone_test(440, 2000);    // respects -24 dB, does NOT reset to 8000
    return 0;
}
```

### §29.6 — Plugin pattern: cross-session volume persistence

For plugins that need volume to survive module reloads (typical in a
DAW context), store the dB value in whatever your preferences system is
and restore on init:

```c
// At plugin init:
auto saved_db;
saved_db = prefs_get_int("volume_db", 0 - 12);   // default -12 dB
audio_set_volume_db(saved_db);

// When the user drags the volume slider:
audio_set_volume_db(new_db);
prefs_set_int("volume_db", new_db);
```

Because the set/get pair round-trips exactly for integer dB values, the
plugin UI stays consistent with the audio engine without a separate
"shadow value" in the UI state.

### §29.7 — What this chapter does NOT cover

- **Mixer fan-out** (per-channel volume). That's `stbmixer.bsm`'s
  territory — `mixer_set_voice_gain(voice_id, amp)` per slot. The
  `audio_*` APIs here set the master level that applies AFTER the
  mixer sum.
- **Fade curves** (smooth volume transitions over N ms). Not shipped
  yet — would be `audio_fade_to(target_amp, duration_ms)` when added.
  Today users step the value themselves in their render loop.
- **Stream I/O** (push-pull ring buffer). `audio_open` / `audio_push`
  / `audio_push_frames` / `audio_ring_space` are separate from volume;
  they control the data path, not the gain.

### §29.8 — Verification

The volume API has a proof-of-life test:

```
tests/test_audio_tone.bpp   — calls audio_set_volume_db before the tone
```

Compile and run:

```bash
./bpp tests/test_audio_tone.bpp -o /tmp/t
/tmp/t
```

A clean run plays ~2 seconds of a 440 Hz square wave at whatever volume
the test selected, prints seven non-zero FFI probe values, and reports
~86 callback invocations (2 seconds × 43 callbacks/sec at 1024 frames
per buffer). Exit code 0.

If the tone sounds at the default (-12 dB, loud), volume setter was not
called before `audio_tone_test`. If the tone is silent but callbacks are
non-zero, either the dB value was ≤-96 or an amplitude was set to 0.
Check the specific `audio_set_*` call before the tone.

---

## Cap 30 — Audio mixer (stbmixer)

*Depends on: Cap 29 (stbaudio)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbmixer.bsm` sits between sound sources and the single audio device.
It owns the voice pool, runs the per-sample synthesis loop, routes
voices to named buses (master / music / sfx), and pushes the mixed
output into stbaudio's SPSC ring.

The division of labor:
- `stbaudio` — one stream in, one device out. One master volume.
- `stbmixer` — N voices in, one stream out. Per-voice + per-bus +
  master volume, all composable.
- `stbsound` — decodes WAV files into buffers the mixer can play.

Game code almost always touches `stbmixer` (call `mixer_note_on` /
`mixer_play_sample` / `mixer_stream`); it rarely needs `stbaudio` beyond
`audio_open` / `audio_close`. If the mixer is not enough for your use
case, you can bypass it and push raw frames with `audio_push_frames` —
but that forfeits polyphony.

### §30.0 — The voice pool

The mixer maintains a fixed pool of 10 voice slots:
- **Slots 0-7** — the "SFX / tone pool". Any `note_on` or
  `play_sample` call lands here. When all 8 are busy, new notes are
  dropped on the floor (return -1).
- **Slot 8** — reserved for music. `play_music` always uses this
  slot; a second `play_music` replaces the first track rather than
  competing with SFX.
- **Slot 9** — spare for future streaming voices.

Each slot has 12 parallel arrays (kind, key, amp, phase, buffer, pos,
frames, bus, loop, ...) totaling 8 × 10 = 80 bytes per array, well
under a cache line family. No struct-of-voices layout — voices are
iterated in the per-sample loop, and parallel arrays are cache-
friendlier than struct-of-arrays for that pattern.

Voice kinds (stored in `_mx_voice_kind[slot]`):
- `MX_KIND_TONE` — synthesised square/sine/saw/triangle at `freq_hz`
- `MX_KIND_SAMPLE` — one-shot PCM buffer playback
- `MX_KIND_MUSIC` — looping (or not) PCM buffer playback
- `MX_KIND_NONE` — slot is free

### §30.1 — The four-level gain chain

Every sample passes through four multiplicative gains before it hits
the DAC. Knowing the order matters when you tune volumes:

```
voice sample
  × _mx_voice_amp[slot]           ← per-voice amplitude
  × _mx_bus_vol[bus] / 100        ← bus gain (MUSIC / SFX)
  × _mx_master_vol / 100          ← master gain (legacy synthkey knob)
  × _mx_bus_vol[MASTER] / 100     ← master bus slider
  → clipped to s16 → DAC
```

Conceptually, the four levels serve different roles:

| Level | Who sets it | When | Unit |
|-------|-------------|------|------|
| voice | `mixer_note_on` defaults to `_MX_VOICE_AMP` (8000). Caller may override via `mixer_set_voice_volume[_db]`. | Per-note | raw s16 or dB |
| bus   | App code balances MUSIC vs SFX. | Per-session or runtime | percent or dB |
| master | Legacy synthkey knob — often left at default 80. | Session | percent or dB |
| master bus | The final global volume slider in the UI. | User | percent or dB |

Collapsing: if your app only has one slider, set `_mx_bus_vol[MASTER]`
and leave the rest at defaults. Plugins and games with separate music
vs effects sliders use the bus layer. DAW-style per-voice ducking
uses the voice layer.

### §30.2 — Bus operations

Three named buses are defined at init: `MX_BUS_MASTER`, `MX_BUS_MUSIC`,
`MX_BUS_SFX`. Every voice is routed to one of them (default SFX for
tone / sample voices, MUSIC for the music slot).

```c
// Percent API (0..100)
void mixer_set_bus_volume(bus, vol);
mixer_get_bus_volume(bus);

// dB API (typical range 0..-60)
void mixer_set_bus_volume_db(bus, db);
mixer_get_bus_volume_db(bus);

// Mute toggles — remembers pre-mute volume
void mixer_mute_bus(bus);
void mixer_unmute_bus(bus);
mixer_bus_is_muted(bus);
```

Typical game scene:

```c
// Cinematic: dip the SFX while dialogue plays
mixer_set_bus_volume_db(MX_BUS_SFX, 0 - 18);   // -18 dB (about 1/8)
dialogue_play("level_up");
mixer_wait_until_done();
mixer_set_bus_volume_db(MX_BUS_SFX, 0);        // restore
```

Pause menu pattern:

```c
// User opens menu — duck music, full SFX for UI beeps
mixer_set_bus_volume_db(MX_BUS_MUSIC, 0 - 12);

// User picks "exit to title" — kill music immediately
mixer_mute_bus(MX_BUS_MUSIC);

// Back to game
mixer_unmute_bus(MX_BUS_MUSIC);
```

### §30.3 — Master operations

```c
void mixer_set_master_volume(vol);       // 0..100 (legacy synthkey)
mixer_get_master_volume();
void mixer_set_master_volume_db(db);
mixer_get_master_volume_db();

void mixer_mute_master();
void mixer_unmute_master();
mixer_master_is_muted();
```

`mixer_mute_master()` is the big red button — kills every bus output
without altering per-bus balance. Unmute restores the exact prior
state. Use for a game-wide "pause everything" or the classic "user
pressed M" shortcut.

The confusing bit: there are TWO master knobs — `_mx_master_vol`
(legacy synthkey-era) and `_mx_bus_vol[MX_BUS_MASTER]` (the named
bus). Both multiply the final sum. For most use cases, pick one and
leave the other at 100. Kept both for backward compatibility; a
future stbmixer v2 may collapse them.

### §30.4 — Per-voice operations

```c
void mixer_set_voice_volume(slot, amp);       // raw s16 peak, 0..32767
mixer_get_voice_volume(slot);
void mixer_set_voice_volume_db(slot, db);     // dB from 0 dB = 32700
mixer_get_voice_volume_db(slot);
```

Voice volume is the first multiplier in the gain chain. Setting it
to 0 silences that specific voice without affecting other voices on
the same bus. Useful for:

- **Fade-in** on note attack:
  ```c
  auto slot, frame;
  slot = mixer_note_on(key, 440);
  for (frame = 0; frame < 30; frame = frame + 1) {
      mixer_set_voice_volume(slot, 8000 * frame / 30);
      mixer_stream(128);
  }
  ```
- **Velocity-sensitive** MIDI-style triggers: scale `amp` by the
  velocity byte at note_on.
- **Ducking** a specific voice temporarily: save its current amp,
  drop it, restore later. Don't use `mixer_mute_bus` for a single
  voice — that kills every voice on the bus.

Voice amplitude persists until `note_off` or the next `note_on` on
that slot. It does NOT reset to `_MX_VOICE_AMP` (8000) between notes
from different keys — if you set a voice to 2000 and play a new note
at the same slot, the next note inherits 2000. Call
`mixer_set_voice_volume(slot, 8000)` after `note_off` to reset, or
set it explicitly on every `note_on`.

### §30.5 — Waveform fader and dirt (non-volume tone shaping)

These existed before the volume API and still belong under mixer
control:

```c
void mixer_set_fader(val);      // 0=sine 256=triangle 512=saw 768=square
mixer_get_fader();

void mixer_set_dirt(val);       // 0=clean, 256=max bitcrush+decimation
mixer_get_dirt();
```

Fader cross-blends four waveforms. Dirt stacks bitcrush + sample-and-
hold decimation onto the final mix. Both are orthogonal to the gain
chain — they affect the tone timbre, not the loudness. A chiptune
test that wants 8-bit-era sound turns both cranked:

```c
mixer_set_fader(768);     // pure square
mixer_set_dirt(180);      // moderate crunch
```

### §30.6 — Streaming the mix

```c
mixer_stream(n);           // fill and push n frames to the audio ring
mixer_fill(buf, n);        // fill a user buffer without pushing (for analysis)
```

Typical game loop:

```c
auto space;
space = audio_ring_space();
if (space > 0) {
    if (space > 512) { space = 512; }   // scratch buf cap
    mixer_stream(space);
}
```

The mixer's scratch buffer is 512 frames (`_MX_FILL_BUF_FRAMES`).
Larger pushes split into multiple calls. For the stream to stay
uninterrupted, feed at least as many frames per second as the
device consumes (44100 for stbaudio's default rate).

### §30.7 — Precision caveat for dB APIs

Percent storage inside the mixer is integer 0..100. Going through
`_db → pct` loses precision for values below about -40 dB (where
percent would round to 0). For plugin-grade precision, you would
need to refactor the internal storage to raw amplitude (0..32767)
— which is a breaking change and not yet scheduled.

Practical effect:
- `mixer_set_bus_volume_db(bus, 0 - 48)` sets percent to 0 — silent.
- Readback then returns -60 dB (the floor), not -48 dB.
- Voices with dB values above -40 round-trip exactly for integer dB.

If a future DAW or plugin host needs sub-dB precision, budget a
refactor that promotes `_mx_bus_vol` and friends to `long long`
amplitude units (0..32767) with the percent API as a wrapper.

### §30.8 — Relationship with stbaudio's volume

stbaudio has its own `audio_set_amplitude / audio_set_volume /
audio_set_volume_db` (Cap 29). These knobs operate on the audio
stream AFTER the mixer pushed its samples. The full gain chain
from a mixer voice to the DAC is:

```
voice sample
  × voice_amp                                  (mixer)
  × bus_vol / 100                              (mixer)
  × _mx_master_vol / 100                       (mixer)
  × master_bus_vol / 100                       (mixer)
  → push to audio ring
  × _aud_amplitude / 32767                     (stbaudio callback)
  → DAC
```

Two distinct master layers. **Rule of thumb:** use the mixer's
master when adjusting game internals (music ducking, fade-outs).
Use stbaudio's master for session-level volume persistence (user's
global volume slider that survives game restarts). Don't use both
at the same time unless you enjoy debugging gain chains.

### §30.9 — Verification

```
tests/test_mixer_sample.bpp   — one-shot sample playback
tests/test_mixer_stream.bpp   — continuous streaming of mixer output
```

Both exercise the post-volume-API mixer end to end: open device,
init mixer, set volumes, play samples / notes, confirm playback via
ring drain counters. A passing run prints `OK` and exits 0. Run
them after any change to stbmixer:

```bash
./bpp tests/test_mixer_sample.bpp -o /tmp/t && /tmp/t
./bpp tests/test_mixer_stream.bpp -o /tmp/t && /tmp/t
```

### §30.10 — What this chapter does NOT cover

- **Solo** (mute all voices except the selected one). DAW feature,
  not shipped. Workaround: save each voice's amp, drop unselected
  ones to 0, restore on un-solo.
- **Fade curves** over N ms. Not shipped. Step the voice amp in
  your render loop — same pattern as the fade-in example above.
- **Per-voice panning** (L/R balance). The mixer outputs mono-on-
  both-channels today. Per-voice pan is a future voice-state field.
- **Send effects** (reverb / delay routed through a dedicated bus).
  Would require a full FX graph, not just a bus routing; out of
  scope for the current mixer.

---

## Cap 31 — Sound files (stbsound)

*Depends on: Cap 30 (stbmixer)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbsound.bsm` is a **format-only** module — it reads and writes audio
files on disk. No device handle, no callback, no real-time behaviour.
A program that just wants to inspect a WAV (dump RMS, compute length,
slice regions) can import stbsound alone without opening the audio
device.

The split from stbmixer is deliberate: playback and decoding are
different lifetimes. A game loads 5 WAVs at boot, then plays them
repeatedly via the mixer. Decoding happens once; playback happens
every frame. Keeping them in separate modules lets tools (level
editors, sample ripper, audio debugger) use one without the other.

### §31.1 — Format scope

Today stbsound speaks exactly one file format: **RIFF WAV, 44100 Hz,
16-bit signed, stereo interleaved PCM**. Each frame is 4 bytes:
`left_lo, left_hi, right_lo, right_hi`. This is the same format
stbaudio's SPSC ring uses, so a loaded buffer plugs directly into
`mixer_play_sample` without conversion.

Mono files or other sample rates are not supported — the WAV reader
rejects them with a clean error. Future formats (ogg / mp3 / flac)
would land as sibling modules (`stbogg`, `stbmp3`) rather than
extending stbsound, keeping each decoder focused.

### §31.2 — Load a WAV

```c
sound_load_wav(path)@io;   // returns 0 on success, -1 on failure
sound_buf()@base;          // pointer to the loaded sample buffer
sound_frames()@base;       // number of stereo frames in the buffer
```

The load pattern is stateful — there's one "last loaded" slot shared
across calls. This is deliberate: most programs load a handful of
WAVs at boot and track their handles separately, so the state is just
a convenience for the common "load then immediately play" pattern:

```c
// Load an SFX.
if (sound_load_wav("assets/apple_crunch.wav") != 0) {
    put("failed to load apple_crunch\n");
    return 1;
}
auto apple_buf, apple_frames;
apple_buf    = sound_buf();
apple_frames = sound_frames();

// Load a music track — this overwrites the "last loaded" slot,
// but apple_buf still points at the first load (malloc'd).
if (sound_load_wav("assets/bgm.wav") != 0) { return 1; }
auto bgm_buf, bgm_frames;
bgm_buf    = sound_buf();
bgm_frames = sound_frames();

// Play them via the mixer.
mixer_play_sample(apple_buf, apple_frames);
mixer_play_music(bgm_buf, bgm_frames, 1);   // 1 = loop
```

The buffer is owned by the caller after the load — stbsound allocates
it with `malloc` and forgets the pointer once `sound_buf` has been
read. It's your responsibility to `free(apple_buf)` when the sample
is no longer needed. The mixer does NOT take ownership either; as
long as the sample is on an active voice, the buffer must stay alive.

### §31.3 — Save a WAV

```c
sound_save_wav(path, buf, n_frames)@io;   // returns 0 on success
```

Stateless — takes the buffer + frame count explicitly, writes the
file. Header construction happens inside the call: RIFF / WAVE / fmt
chunk (PCM s16 44.1k stereo) / data chunk. Total header is 44 bytes
plus the sample data.

Typical use case: save a render of generated audio so downstream
tools (Audacity, a plugin UI) can inspect:

```c
auto buf;
buf = malloc(60 * 44100 * 4);   // 60 seconds
mixer_fill(buf, 60 * 44100);    // synthesize
sound_save_wav("/tmp/render.wav", buf, 60 * 44100);
free(buf);
```

The write path uses `bpp_io.bsm`'s file API (`fopen` / `fwrite` /
`fclose` via the `@io` effect annotation). Partial writes return
-1 — filesystem full, permission denied, invalid path. The
incomplete file is left on disk; callers that want atomic writes
should save to a temp path and rename.

### §31.4 — Verification

```
tests/test_sound_roundtrip.bpp   — save then load + byte equality check
```

The round-trip test generates 1 second of silence, saves it, loads
it back, and asserts every byte matches. If this fails, the header
construction is wrong or the byte-order helpers (`_snd_w16` /
`_snd_w32`) dropped a bit somewhere.

### §31.5 — Relationship with the rest of the audio stack

```
stbsound  — disk ↔ buffer  (WAV read/write)
   ↓ buffer
stbmixer  — N buffers → 1 stream  (voices + buses + volume)
   ↓ stream
stbaudio  — 1 stream → DAC  (CoreAudio / ALSA / ...)
   ↓ speakers
```

Each layer is usable alone. A command-line tool that just dumps WAV
stats imports stbsound only. A synthesizer that generates tones
on the fly imports stbmixer + stbaudio (no stbsound). A game imports
all three.

### §31.6 — What this chapter does NOT cover

- **Streaming decoding** (load-as-you-play). `sound_load_wav` reads
  the whole file. For BGM files too big to keep in RAM, you'd need
  a streaming reader that feeds the mixer's ring in chunks.
- **Format conversion** (resample 48kHz → 44.1, mono → stereo). Not
  shipped. Future `sound_convert(src_buf, src_frames, src_rate,
  src_channels) → (dst_buf, dst_frames)` would live here.
- **Metadata** (RIFF LIST INFO tags, cue points). Skipped today —
  the reader treats everything after the `data` chunk header as
  PCM bytes.
- **Compressed WAV** (ADPCM, μ-law). PCM-only. Compressed variants
  are rare in games and land as a separate decoder if ever needed.

---

## Cap 32 — Input (stbinput)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbinput.bsm` is the keyboard + mouse polling layer. Every frame the
platform event pump fills the input arrays; game code reads them with
simple `key_down(KEY_X)` / `mouse_x()` style calls. No event queue to
drain, no listener registration, no callback plumbing — just state
you can query.

The module is platform-agnostic at the API level (same `key_down`
call whether you're on macOS Cocoa or Linux X11). Per-platform
scancode mapping lives in `_stb_platform_<os>.bsm` and normalizes
everything to the same 77-key enum.

### §32.1 — Three input shapes

| Shape | Use case | API |
|-------|----------|-----|
| **Held state** | "Is the key currently down?" Player movement while a key is held. | `key_down(k)` / `mouse_down(btn)` |
| **Edge trigger** | "Did the key start being pressed this frame?" Actions that should fire once per press (jump, shoot). | `key_pressed(k)` / `mouse_pressed(btn)` / `mouse_released(btn)` |
| **Text entry** | "What printable characters were typed this frame?" UI text fields. | `input_text_len()` / `input_text_char(i)` |

**Rule of thumb:**
- Movement → `key_down` (continuous polling)
- Actions → `key_pressed` (once per press, fires on the down-edge)
- Chat / console → `input_text_*` (respects keyboard layout, handles
  Shift for capitals)

### §32.2 — The KEY_* enum (77 keys)

Keys are normalized integer IDs. Grouped by category:

```
Navigation:  KEY_UP  KEY_DOWN  KEY_LEFT  KEY_RIGHT
WASD:        KEY_W  KEY_A  KEY_S  KEY_D
Chords:      KEY_SPACE  KEY_ENTER  KEY_ESC  KEY_TAB  KEY_BACKSPACE
Letters:     KEY_A..KEY_Z (all 26)
Digits:      KEY_0..KEY_9
Function:    KEY_F1..KEY_F12
Modifiers:   KEY_SHIFT  KEY_CTRL  KEY_ALT  KEY_META
Punctuation: KEY_PERIOD  KEY_COMMA  KEY_SLASH  KEY_MINUS  KEY_EQUAL
             KEY_LBRACKET  KEY_RBRACKET  KEY_SEMICOLON  KEY_QUOTE
             KEY_BACKSLASH  KEY_GRAVE
Editing:     KEY_DELETE  KEY_HOME  KEY_END  KEY_PGUP  KEY_PGDOWN
```

Each is an integer in the range 1..256 — directly indexes into the
underlying `_stb_keys` byte array. Never hardcode integers; always
use the enum name.

### §32.3 — Keyboard state

```c
key_down(k)@base;       // 1 if currently held, 0 if not
key_pressed(k)@base;    // 1 if just pressed this frame, 0 otherwise
```

`key_pressed` is `(current && !prev)` — fires exactly once per
physical press. Example: a pause toggle.

```c
if (key_pressed(KEY_P)) {
    paused = paused == 0 ? 1 : 0;
}
```

Holding P doesn't re-toggle every frame — only the initial press
fires. For continuous input, use `key_down`:

```c
if (key_down(KEY_W)) { player_y = player_y - speed; }
if (key_down(KEY_S)) { player_y = player_y + speed; }
```

The prev-state snapshot happens inside `_input_save_prev` called
before each `_stb_poll_events`. This runs automatically from
`game_frame_begin` (Cap 34) — you don't call it by hand.

### §32.4 — Mouse state

```c
mouse_x();                   // x in window pixels (0..window_w)
mouse_y();                   // y in window pixels (0..window_h)
mouse_down(btn);             // 1 if button held
mouse_pressed(btn);          // 1 if button pressed this frame
mouse_released(btn);         // 1 if button released this frame
```

Button IDs: `MOUSE_LEFT` (1), `MOUSE_RIGHT` (2), `MOUSE_MIDDLE` (3).

```c
if (mouse_pressed(MOUSE_LEFT)) {
    if (point_in_rect(mouse_x(), mouse_y(), btn_x, btn_y, btn_w, btn_h)) {
        fire_button();
    }
}
```

`point_in_rect(px, py, x, y, w, h)` is a convenience: returns 1 if
the point lies inside the rectangle (inclusive of left/top edges,
exclusive of right/bottom). Pure integer compare, no float math.

### §32.5 — Text input

Typed characters accumulate into a per-frame ring. Read them with
`input_text_len` + `input_text_char`:

```c
auto i, ch;
for (i = 0; i < input_text_len(); i = i + 1) {
    ch = input_text_char(i);
    if (ch == '\b')      { my_string_pop(); }        // backspace
    else if (ch == '\r') { submit(); }               // enter
    else                 { my_string_append(ch); }    // printable
}
```

The ring is 64 bytes per frame — way more than any human can type.
Overflow silently drops characters from the end. The buffer clears
at the start of every frame (`_input_save_prev` empties it).

**Shift handling is automatic** — the platform layer respects the
keyboard layout, so a Shift+A press lands as `'A'` (65) in the text
ring while `KEY_A` remains `key_down`. Don't try to reconstruct
case from `key_down(KEY_SHIFT)` — use the text ring.

### §32.6 — Actions (high-level binding)

For games that want the "configurable keybindings" pattern, stbinput
includes a named-action layer:

```c
void action_define(name, key, mod);   // register an action
void action_rebind(name, key, mod);   // change an existing binding
action_pressed(name);                 // edge trigger
action_down(name);                    // held state
```

Example:

```c
// At game init:
action_define("jump",   KEY_SPACE, 0);
action_define("shoot",  KEY_Z,     0);
action_define("save",   KEY_S,     ACTION_MOD_CTRL);

// In the game loop:
if (action_pressed("jump"))  { player_jump(); }
if (action_down("shoot"))    { fire_bullet(); }
if (action_pressed("save"))  { save_game(); }
```

Modifier flags: `ACTION_MOD_CTRL`, `ACTION_MOD_SHIFT`, `ACTION_MOD_ALT`,
`ACTION_MOD_META`. OR them together for multi-modifier actions.

Iteration (for a rebinding UI):

```c
auto n, i;
n = action_count();
for (i = 0; i < n; i = i + 1) {
    render_action_row(
        action_name_at(i),
        action_key_at(i),
        action_mod_at(i)
    );
}
```

### §32.7 — Frame lifecycle

Input state is refreshed by the game loop in this sequence:

```
frame N:
  1. _input_save_prev()     ← snapshot current → prev
  2. _stb_poll_events()     ← platform fills current + text ring
  3. [user code]            ← reads key_down/key_pressed/mouse_*/etc
  4. render + present
  5. sleep for frame budget
frame N+1:
  ...
```

`stbgame` (Cap 34) calls steps 1-2 inside `game_frame_begin()`. If
you build your own loop without stbgame, call them in the same order
before reading any input.

### §32.8 — What this chapter does NOT cover

- **Gamepad** (Xbox / PlayStation / Switch Pro). Not shipped. Would
  live in a separate module (`stbpad`).
- **Raw mouse motion** (delta-X / delta-Y between frames). You can
  compute this yourself — save `mouse_x()` from last frame and
  subtract. No rare-input support (mouse wheel, high-DPI deltas).
- **IME / CJK input composition**. The text ring emits one byte
  per codepoint, ASCII-only. Japanese / Chinese / Korean input
  would need a composition layer on top.
- **Touch** (iOS / Android). Out of scope — B++ targets desktop
  platforms today.

---

## Cap 33 — Window (stbwindow)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbwindow.bsm` covers two distinct concerns that happen to both
involve the native window system: **native dialogs** (save/open/
alert/confirm) and **live window geometry** (tracking resizes). It
sits on top of the platform layer's NSApp / NSWindow wrappers —
programs using stbwindow also need `import "stbgame.bsm"` because
the platform file owns the window lifecycle.

### §33.1 — Platform coverage

| Feature | macOS | Linux (X11) |
|---------|-------|-------------|
| Save dialog | ✓ NSSavePanel | ✗ returns 0 (cancelled) |
| Open dialog | ✓ NSOpenPanel | ✗ returns 0 |
| Folder dialog | ✓ NSOpenPanel folder mode | ✗ returns 0 |
| Alerts (info/confirm/error) | ✓ NSAlert | ✗ stderr warning |
| Live geometry | ✓ Cocoa KVC poll | ✓ X11 ConfigureNotify |
| Framebuffer resize | ✓ NSImageView | ✓ XPutImage |

The Linux dialog no-op is deliberate — desktop Linux has no stable
native file picker (Zenity / kdialog / portal spec move independently).
Callers that need cross-platform pickers should branch on `return
== 0` and fall back to a CLI prompt or a hardcoded path.

### §33.2 — File dialogs

```c
window_save_dialog(title, default_name)@io;    // → path or 0
window_open_dialog(title)@io;                  // → path or 0
window_open_folder_dialog(title)@io;           // → path or 0
```

All three return a **malloc'd null-terminated C-string** on success,
or `0` if the user cancelled. **Caller owns the string and must
`free` it.**

```c
auto path;
path = window_save_dialog("Save level", "untitled.level.json");
if (path == 0) { return; }            // user cancelled
file_write_all(path, buf, len);
free(path);
```

Dialogs **block the main thread** until dismissed. Audio rings
drain while the modal is up — the mixer will push silence, which is
usually imperceptible for a save flow. If you're running a real-time
audio demo and can't tolerate the pause, don't open a dialog there.

### §33.3 — Alerts

```c
void window_alert(title, message)@io;           // info + OK
window_confirm(title, message)@io;              // OK/Cancel → 1 or 0
void window_error(title, message)@io;           // red icon + OK
```

Info and error alerts are blocking acknowledgements — the user has
to click OK to proceed. Confirm is the standard "Are you sure?"
pattern:

```c
if (has_unsaved_changes) {
    if (!window_confirm("Discard?", "You have unsaved changes.")) {
        return;   // user picked Cancel
    }
}
close_document();
```

Choose the right shape:
- **alert** — completion acknowledgement ("Saved!")
- **confirm** — gated destructive action ("Delete selected layer?")
- **error** — red for real failures ("Failed to load /tmp/x: no
  such file")

### §33.4 — Live window geometry

For apps that want to react to user-driven window resizes (IDEs,
level editors, laid-out tools):

```c
window_width()@base;       // current content width in px
window_height()@base;      // current content height in px
window_resized();            // edge-triggered: 1 if changed since last call
void window_fb_resize();     // reallocate _stb_fb to match window
```

Typical resize-aware game loop:

```c
while (game_should_quit() == 0) {
    game_frame_begin();

    if (window_resized()) {
        window_fb_resize();          // match framebuffer to new window
        layout_recompute();          // re-lay-out your UI panels
    }

    clear(BLACK);
    draw_scene();
    draw_end();
}
```

`window_resized()` is **edge-triggered** — it returns 1 exactly once
per actual resize. Calling it twice per frame means the second call
returns 0 even if the window just resized. **Only ONE caller per
frame** should consume the flag (your main loop, not individual
panels).

### §33.5 — Framebuffer vs window size

These are different concepts:

- `window_width()` / `window_height()` — the native window's current
  content area in pixels. Grows/shrinks as the user drags the corner.
- `_stb_w` / `_stb_h` — the software framebuffer size. Fixed until
  you call `window_fb_resize()`.

By default, the platform **stretches** the old framebuffer into the
new window size (`NSImageView.imageScaling = .axesIndependently` on
macOS). Pixelated but keeps legacy apps working without opting in.

If you want crisp 1:1 pixels at the new size (typical for IDEs and
editors), call `window_fb_resize()` after `window_resized()` returns
1. The function frees the old framebuffer and reallocates
`_stb_w * _stb_h * 4` bytes at the new dimensions — **contents are
discarded**, so redraw the whole frame on the next frame.

Apps that don't care about resize (most games — they run at fixed
resolution) can ignore the whole live-geometry API. The platform
handles the stretch automatically.

### §33.6 — What this chapter does NOT cover

- **Multiple windows**. B++ supports exactly one window per process
  today. Modal dialogs piggyback on that window's event pump.
- **Window icon / title bar customization**. The title is set once
  via `game_init` (Cap 34) and never changes.
- **Custom cursors**. The platform uses the OS default cursor; no
  API to swap for a crosshair or drag-hand cursor yet.
- **Drag-and-drop**. Not shipped. macOS NSDraggingDestination would
  plug in here but hasn't been wired.
- **Clipboard**. Not shipped. Future `window_clipboard_read()` /
  `window_clipboard_write(s)` would sit next to the dialog APIs.

---

## Cap 34 — Game loop (stbgame)

*Depends on: Cap 32 (stbinput), Cap 33 (stbwindow)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbgame.bsm` is the **one-call game bootstrapping** module. Call
`game_init(w, h, title, fps)` and the whole stack wakes up: math,
input, draw, font, UI, str — all initialized, window opened, frame
timing armed. Everything after that point is a game loop reading
input, drawing, and calling `game_frame_begin()` + `draw_end()`.

stbgame is also the home of the **scene manager** — a tiny
state-machine layer that lets a game flip between MENU / PLAY /
RESULTS scenes without the main loop caring about any of them.

### §34.1 — Init + frame cycle

The canonical game loop:

```c
main() {
    game_init(640, 360, "My Game", 60);
    while (game_should_quit() == 0) {
        game_frame_begin();
        // ... update world using game_dt_ms() or game_dt() ...
        clear(BLACK);
        // ... draw ...
        draw_end();
    }
    game_end();
    return 0;
}
```

| Call | What it does |
|------|--------------|
| `game_init(w, h, title, fps)` | Init math/input/draw/font/ui/str, allocate framebuffer, open window, arm timing. |
| `game_frame_begin()` | Snapshot prev input, compute `dt_ms`, reset UI arena. |
| `game_dt_ms()` | Integer ms since last frame (clamped to 1..50). |
| `game_dt()` | Float seconds (dt_ms * 0.001). |
| `game_should_quit()` | 1 if user closed window or set quit flag. |
| `game_end()` | Close window, free platform resources. |

`fps <= 0` defaults to 60 FPS (16 ms frame time). The frame budget
is enforced by `draw_end()` which sleeps to pad out the remainder —
you do NOT need to call `usleep` manually.

### §34.2 — Delta time discipline

Always scale per-frame motion by `dt_ms` (or `dt`) so the game runs
the same speed regardless of frame rate:

```c
// WRONG — speed depends on FPS
player_x = player_x + 3;

// RIGHT — 120 pixels per second at any FPS
player_x = player_x + 120 * game_dt_ms() / 1000;
```

`game_dt_ms()` is clamped to 1..50 ms so:
- A stalled frame (debugger pause, heavy GC) doesn't teleport the
  player across the screen.
- A too-fast frame (paused → resumed) doesn't freeze motion.

For physics simulations needing finer time steps, divide dt into
sub-steps yourself inside your update loop.

### §34.3 — `game_run` — the maestro wrapper

For programs that use the `bpp_maestro` pattern (init / solo / base
/ render callbacks — see Cap 22 / 24), `game_run` bundles
`game_init` + `maestro_run` + `game_end`:

```c
main() {
    maestro_register_init(fn_ptr(my_init));
    maestro_register_solo(fn_ptr(my_solo));
    maestro_register_base(fn_ptr(my_base));
    maestro_register_render(fn_ptr(my_render));
    maestro_register_quit(fn_ptr(my_quit));
    game_run(640, 360, "My Game", 60);
    return 0;
}

my_solo(dt) {
    game_frame_begin();
    if (game_should_quit()) { maestro_quit(); }
    // ... per-frame solo-phase work ...
}
```

Use `game_run` when you want maestro's phase lattice (solo / base
workers / render on main). Use the plain `game_init` + manual loop
when you don't.

### §34.4 — Scene manager

The scene manager ships in stbgame (absorbed from the older
`stbscene.bsm`) because most games want scenes AND the game loop
together.

A scene is four function pointers:

```
load()     — called once when entered. Allocate resources.
update(dt) — per-frame logic. dt in milliseconds.
draw()     — per-frame render. Reads world state.
unload()   — called once when left. Free what load allocated.
```

Any of the four may be `0` (null) if the scene has nothing to do
at that phase. A simple MENU scene typically passes 0 for load /
unload — it has no heap state to manage.

**Lifecycle:**

```c
#define SCENE_MENU 0
#define SCENE_GAME 1

main() {
    game_init(640, 360, "My Game", 60);
    init_scene();

    scene_register(SCENE_MENU, 0,
                   fn_ptr(menu_update), fn_ptr(menu_draw), 0);
    scene_register(SCENE_GAME,
                   fn_ptr(game_load), fn_ptr(game_update),
                   fn_ptr(game_draw), fn_ptr(game_unload));
    scene_switch(SCENE_MENU);

    while (game_should_quit() == 0) {
        game_frame_begin();
        scene_tick(game_dt_ms());
        scene_draw();
        draw_end();
    }
    scene_shutdown();
    game_end();
    return 0;
}
```

To transition, call `scene_switch(new_id)` from anywhere — even
from inside an update or draw handler. The switch is **deferred to
the next `scene_tick`**, so mid-frame state isn't corrupted:

```c
menu_update(dt_ms) {
    if (key_pressed(KEY_ENTER)) {
        scene_switch(SCENE_GAME);   // safe — fires next frame
    }
}
```

The scene pool is capped at 16 entries (`_SCENE_MAX`). Bigger RPGs
can bump it — adjust the constant and rebuild.

### §34.5 — Frame lifecycle, annotated

```
game_frame_begin()
  ├ On first frame: _stb_poll_events() to seed input.
  │   (Every subsequent frame, input was polled at the end of
  │    the PREVIOUS draw_end(), so it's as fresh as possible.)
  ├ Compute dt_ms = now - _stb_last_time, clamp to 1..50.
  ├ Reset UI arena — all gui_* widgets from last frame freed.
  └ [user code runs here — update world, handle input]

draw_end()
  ├ _stb_present() — blit framebuffer to window.
  ├ _stb_frame_wait() — sleep to pad out _stb_frame_ms budget.
  ├ _stb_poll_events() — catch up on input BEFORE next frame.
  └ [next frame begins]
```

The key insight: input polling happens **between** frames, not at
the top of each frame. This gives the game code the most recent
possible input state when it reads. You don't need to think about
it — just know that `game_frame_begin` + `draw_end` wrap the loop
correctly.

### §34.6 — What this chapter does NOT cover

- **Multi-window games**. One window per process today (Cap 33).
- **Background music mixing**. That's `stbmixer` (Cap 30) — stbgame
  doesn't auto-open the audio device. Call `audio_open` explicitly
  if you want sound.
- **Save/load checkpoint**. Out of scope. `bpp_file.bsm` +
  `bpp_json.bsm` are the building blocks for persistence.
- **Multiplayer networking**. No networking in stbgame. The
  sockets syscalls exist (Cap 18 / Wave 18), but a game-ready
  networking module would be its own `stbnet`.

---

## Cap 35 — Drawing (stbdraw)

*Depends on: Cap 34 (stbgame)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbdraw.bsm` is the 2D software rasterizer — every `draw_*`, `clear`,
`layer_*`, and text rendering call lives here. It writes into the
framebuffer `_stb_fb` (RGBA, `_stb_w * _stb_h` pixels), which the
platform layer blits to the window each frame via `_stb_present`.

### §35.1 — Color and framebuffer

Colors are 32-bit packed ARGB integers (alpha high, blue low). Every
`draw_*` call takes one color arg.

```c
rgba(r, g, b, a);                // build a color from 0..255 channels
clear(color);                    // fill the whole framebuffer
```

Constants available: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`,
`CYAN`, `MAGENTA`, `GRAY`, `PURPLE`, `ORANGE`, plus a full 16-color
palette declared in `init_color()` (called by `game_init`).

### §35.2 — Shapes

```c
void draw_rect(x, y, w, h, color);
void draw_rect_v(pos, w, h, color);   // pos as packed (x<<32)|y (future)
void draw_circle(cx, cy, r, color);
void draw_line(x1, y1, x2, y2, color);
void draw_border(x, y, w, h, color, thickness);
void draw_rounded_rect(x, y, w, h, r, color);
void draw_gradient(x, y, w, h, c_top, c_bot);
void draw_shadow(x, y, w, h, intensity);
void draw_shadow_rounded(x, y, w, h, r, intensity);
void draw_bar(x, y, w, h, val, max_val, color);   // HP bar style
```

Bounds are clipped — writes outside `_stb_w / _stb_h` are silently
dropped, so games don't have to guard every call.

### §35.3 — Sprites

```c
void draw_sprite(data, x, y, w, h);          // raw ARGB buffer
void draw_sprite16(spr, pal, x0, y0);        // 16-color indexed sprite
```

`draw_sprite` blits an RGBA buffer; `draw_sprite16` takes a palette
(see Cap 38) and an indexed sprite — each byte is a 4-bit palette
index. Transparent pixels (index 0) are preserved.

### §35.4 — Text

```c
void draw_text(text, x, y, sz, color);
void draw_number(x, y, sz, color, val);
```

Uses an 8x8 bitmap font baked into `_font_table`. `sz` is a pixel
multiplier (1 = 8x8 per glyph, 2 = 16x16, etc.). Text clips to
framebuffer bounds. Each character is rendered as a scaled box of
set/clear pixels.

`draw_text` uses `render_measure(text, sz)` (Cap 47) to pre-compute
widths when needed.

### §35.5 — Draw lifecycle

```c
void draw_begin();   // currently a no-op (reserved for future batching)
void draw_end();     // present + sleep for frame budget + poll input
```

`draw_end` is the loop terminator — it presents the framebuffer to
the window, waits out the frame budget (`_stb_frame_ms`), and polls
the next frame's input. Don't call `_stb_poll_events` manually —
`draw_end` handles it.

### §35.6 — Layers (indexed byte grids)

For palette-indexed games (Captain Buddy, pathfind), layers hold
8-bit indices that compose via a final palette. Faster than RGBA
for NES-era visuals.

```c
layer_new(w, h, blend);                       // BLEND_NORMAL or BLEND_MULTIPLY
void layer_free(layer);
void layer_set_alpha(layer, a);               // 0..255
void layer_set_visible(layer, v);
void layer_set_blend(layer, mode);
layer_get(layer, x, y);                       // byte at (x,y) or 0 OOB
void layer_set(layer, x, y, idx);             // write byte, OOB-safe
void layer_clear(layer, idx);                 // fill with one index
void layer_fill(layer, x, y, idx);            // 4-connected flood fill
void layer_composite_indexed(dst, dw, dh,
                             layers, n,
                             palette);        // composite N layers
```

Typical use: game entities draw into one layer, background into
another, compose both into the main RGBA framebuffer via
`layer_composite_indexed` once per frame.

### §35.7 — TrueType rasterizer

For custom fonts beyond the baked 8x8 table, stbdraw includes a
runtime TTF rasterizer. Load a .ttf buffer, build a glyph atlas at a
target pixel size, blit glyphs by codepoint.

The rasterizer is ~400 lines of scanline-based edge rasterization
with 4x vertical oversampling — matches quality that game fonts
expect, much slower than bitmap fonts but run once at atlas-build
time. Game loop reads the atlas, not the rasterizer.

### §35.8 — What this chapter does NOT cover

- **GPU rendering** — `draw_*` is CPU-only. GPU primitives live in
  `stbrender.bsm` (Cap 47).
- **Image loading** — PNGs/sprites come from `stbimage.bsm` (Cap 37)
  or `stbasset.bsm` (Cap 45).
- **Animation** — `stbforge` owns animation playback (Cap 46).

---

## Cap 36 — UI widgets (stbui)

*Depends on: Cap 35 (stbdraw)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbui.bsm` is an immediate-mode UI toolkit — widgets compute
geometry, dispatch input, and draw themselves in a single function
call. No retained tree, no register/deregister, no IDs beyond what
your layout provides.

### §36.1 — Immediate-mode philosophy

Every frame, you declare the UI you want:

```c
game_frame_begin();
// ... game logic ...
if (gui_button(100, 50, 80, 24, "Save")) { save_game(); }
if (gui_button(100, 80, 80, 24, "Quit")) { quit = 1; }
gui_label(10, 10, "Score: ", score);
draw_end();
```

No state persists between frames unless you store it. The widget
returns 1 the frame it's clicked, 0 otherwise. This means your game
logic flows top-to-bottom every frame, free of callbacks.

The UI arena (32 KiB per-frame, wraps `bpp_arena`) holds transient
allocations — style overrides, layout frames, hover-fade states —
and resets every frame. A leaking widget only leaks for one frame.

### §36.2 — Themes

```c
void theme_dark();        // default — text light, bg dark
void theme_light();       // text dark, bg light
void theme_retro();       // NES-palette-inspired
void theme_set(theme);    // custom theme pointer
```

A theme is a `Theme` struct with fields for background, surface,
text, accent, button_bg, and corner radii. Widgets reference the
active theme — changing themes mid-frame updates every subsequent
widget.

### §36.3 — Basic widgets

```c
gui_button(x, y, w, h, label);        // → 1 on click, 0 otherwise
gui_label(x, y, text, ...);           // static text
gui_panel(x, y, w, h);                // framed card backdrop
gui_line(x1, y1, x2, y2);             // decorative rule
gui_progress(x, y, w, h, val, max);
gui_checkbox(x, y, label, checked);   // → new checked state
gui_slider(x, y, w, val, min, max);   // → new val
gui_dropdown(x, y, w, labels, n, selected);  // → new index
gui_text_input(x, y, w, buf, max);   // → new length
```

All widgets are stateless from the UI's perspective — you pass in
the current state and get the new state back. The widget internals
handle hover animations, click detection, and drawing.

### §36.4 — Layout helpers

Absolute positioning gets tedious for multi-row forms. The layout
stack provides a minimal flex-style system:

```c
layout_begin(x, y, w, h, direction);   // LAYOUT_ROW or LAYOUT_COL
  layout_push(width);                   // allocate space for next widget
  gui_button(0, 0, 0, 0, "A");         // uses current layout slot
  layout_push(width);
  gui_button(0, 0, 0, 0, "B");
layout_end();
```

Stack depth is 8 (fixed). Nested row-inside-column compositions
handle most game HUDs. For complex layouts (popups with arbitrary
positioning), fall back to absolute coordinates.

### §36.5 — Hover and press state

Cross-frame state is keyed by a `(x << 16) | y` hash of the widget's
top-left corner. The same button at the same position is recognized
between frames without explicit IDs. Collision is possible but rare
(two widgets at identical coordinates would share state).

The hover fade takes ~120 ms to complete — if the mouse leaves and
returns within the fade window, the animation resumes smoothly
instead of snapping.

### §36.6 — What this chapter does NOT cover

- **Custom widget authoring** — the public API covers the shipped
  widgets. Adding a new one requires understanding the theme +
  layout internals (source reference: lines 700+).
- **Accessibility** (screen readers, keyboard-only nav). Minimal
  keyboard support via `gui_text_input`; no broader a11y framework.
- **Retained-mode UI** — this is strictly immediate. If you want a
  retained tree, build your own on top.

---

## Cap 37 — Image decode (stbimage)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbimage.bsm` decodes image files to RGBA pixel buffers. Today:
**PNG only** (RIFF-style chunked format, zlib DEFLATE compressed).
Other formats (JPG, BMP, TGA) would land as sibling modules
(`stbjpg`, `stbbmp`) rather than extending stbimage.

### §37.1 — Public API

```c
img_load(path)@io;   // returns pixel buffer or 0
img_w();               // width of last loaded image
img_h();               // height of last loaded image
void img_free(pixels); // release a buffer from img_load
```

State is singleton: the last loaded image's dimensions live in
`img_w()` / `img_h()`. Typical pattern:

```c
auto pixels, w, h;
pixels = img_load("assets/player.png");
if (pixels == 0) { return 1; }   // load failed
w = img_w();
h = img_h();
// ... use pixels (w*h*4 bytes of RGBA) ...
img_free(pixels);
```

### §37.2 — What's supported

- **Color types**: RGB (3-byte), RGBA (4-byte), grayscale (1-byte
  widened to RGB), grayscale+alpha, indexed (via PLTE + tRNS).
- **Bit depths**: 8 per channel (16-bit PNG is clipped to 8).
- **Filters**: all five PNG filters (None / Sub / Up / Average / Paeth).
- **Interlacing**: NOT supported. Adam7-interlaced PNGs return 0 with a
  stderr warning. Typical asset pipelines (Aseprite, Figma export) don't
  interlace — it's a feature for progressive-download web images.
- **tRNS chunk**: palette alpha is honored; RGB-key transparency is
  discarded (fully-opaque output).

### §37.3 — Internal structure

Three layered decoders:
1. **Bit reader** (`_zr_read`) — DEFLATE bit-level unpacking, 1 bit
   at a time into a 32-bit window.
2. **Huffman decoder** (`_zh_build`, `_inf_decode`) — canonical
   Huffman tables reconstructed per deflate block.
3. **PNG chunk scanner** (`img_load` body) — walks IHDR, PLTE, IDAT,
   tRNS, IEND. Non-critical chunks (tEXt, gAMA, sRGB) are skipped.

The zlib / DEFLATE layer is a faithful port of `stb_image.h`'s
implementation. ~400 lines, no compromises on correctness — passes
every shipped test PNG including 3-byte RGB, 4-byte RGBA, indexed
palette, and grayscale+alpha variants.

### §37.4 — Performance notes

PNG decode is NOT hot path. Games call `img_load` once per asset
at boot, the decoded pixels live in memory for the session. Decode
time for a 256×256 sprite: ~5-10 ms on Apple Silicon, mostly
Huffman + filter work. If the pipeline ever needs faster decode,
candidates are (a) SIMD filters for large images, (b) single-pass
Huffman for tiny textures.

### §37.5 — What this chapter does NOT cover

- **Writing PNGs**. `stbimage_write.bsm` (if ever shipped) would live
  next door. Today, use `sound_save_wav` for audio and raw RGBA buffer
  writes for pixel captures.
- **Animated formats** (APNG, GIF). Out of scope — use per-frame PNGs.
- **Streaming decode** (for textures > RAM). Out of scope — assume
  images fit.

---

## Cap 38 — Palette (stbpal)

*Depends on: Cap 35 (stbdraw)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbpal.bsm` is the palette type plus a catalog of named built-in
palettes. Games that draw indexed sprites (like Captain Buddy, the
NES-era mode) pick a palette at boot and reference colors by index
thereafter — 4-bit or 8-bit indices replace 32-bit ARGB everywhere.

### §38.1 — The Palette type

```c
struct Palette { count, data }   // count entries, each 8-byte ARGB
```

Each entry is a full 32-bit ARGB color. Index 0 is conventionally
transparent (alpha = 0).

```c
palette_new(n);                       // allocate zero-filled palette
void palette_free(pal);
palette_clone(pal);                   // deep copy
palette_count(pal);                   // number of entries
palette_get(pal, i);                  // read color at index i (checked)
void palette_set(pal, i, argb);       // write color at index i
palette_data(pal);                    // raw data pointer (hot loops)
```

### §38.2 — Built-in catalog

Eight presets, accessed by name:

| Name | Size | Purpose |
|------|------|---------|
| `"mcu-8"` | 8 | ModuLab default — vivid Nintendo-era spread |
| `"nkotc-4"` | 4 | NES minimalist — 3 colors + transparent |
| `"cb-32"` | 32 | Captain Buddy — 4-row grid (gray/red/green/blue) |
| `"gameboy-4"` | 4 | Original Game Boy shades |
| `"pico8-16"` | 16 | PICO-8 fantasy console |
| `"c64-16"` | 16 | Commodore 64 spread |
| `"ega-16"` | 16 | EGA PC palette |
| `"cga-4"` | 4 | CGA magenta/cyan/white/black |

```c
palette_builtin(name);      // → Palette pointer (clone it if mutating)
void init_palette();        // seeds the catalog (called by game_init)
```

The catalog is shared — `palette_builtin("mcu-8")` returns the same
pointer across calls. To mutate one slot without affecting other
callers, `palette_clone` first.

### §38.3 — Palette cycling (animated shimmer)

Classic NES-era effect: rotate a subset of palette entries to
create water, fire, lava animations without re-rendering sprites.

```c
palette_cycle_begin(pal, start, count, period_ms);
void palette_cycle_stop(pal, start);
void palette_cycle_tick(pal, dt_ms);   // call once per frame
```

A cycle shifts `count` entries starting at `start` by one slot every
`period_ms / count` frames. Looks like flowing water at 400 ms
period with 4 entries. Multiple cycles can run simultaneously on
different slot ranges.

### §38.4 — Palette LUTs (global swap)

A `PaletteLUT` remaps indices — useful for damage flashes (every
pixel → white for 2 frames), day/night fades, or character-specific
color shifts.

```c
palette_lut_new(count);                     // identity map
void palette_lut_free(lut);
void palette_lut_set(lut, from_idx, to_idx);
palette_lut_flash(count, to_idx);           // all → to_idx
void palette_lut_apply(src, lut, dst);      // dst[i] = src[lut[i]]
```

### §38.5 — Palette interpolation

```c
palette_lerp(a, b, t, out);   // out[i] = lerp(a[i], b[i], t), t=0..255
```

Per-channel linear interpolation. Useful for smooth palette
transitions (level changes, time of day).

### §38.6 — Save / load

```c
palette_save(pal, path, name)@io;   // JSON format, human-editable
palette_load(path)@io;              // → Palette pointer or 0
```

The JSON format stores each entry as a hex string (`"#FF2020"`). Uses
`bpp_json` writer + parser underneath (Phase D dogfood — no hand-
rolled JSON structure anywhere in stb).

### §38.7 — What this chapter does NOT cover

- **GPU-side palette upload** — that's `stbrender` (Cap 47) territory.
  The `palette_data` pointer lands directly in a uniform buffer.
- **Dithering** between palettes. Out of scope.
- **Automatic palette reduction** (color quantization from RGBA →
  indexed). Pre-process sprites offline.

---

## Cap 39 — Tilemap (stbtile)

*Depends on: Cap 38 (stbpal), Cap 42 (stbpath)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbtile.bsm` is the indexed-byte tilemap — each cell stores a
single-byte tile index (0..255), which looks up into a tileset
(sprite atlas) and optional solid-mask table. The backing buffer
is dense (1 byte per cell), so a 128×128 tilemap is 16 KiB.

### §39.1 — The Tilemap type

```c
struct Tilemap { w, h, tw, th, data, solid_mask, tileset, tile_count, remap }
```

`w × h` cells of `tw × th` pixels each. `data` is the tile index
grid. `solid_mask` is a 256-entry byte array marking which tile
indices are walls (for physics queries). `tileset` is the decoded
sprite atlas (from `stbimage`). `remap` translates game-logic tile
types into tileset sprite indices — decouples code from artwork.

### §39.2 — Creating and populating

```c
tile_new(w, h, tw, th);             // empty map, all cells = 0
void tile_free(tm);
void tile_set(tm, x, y, idx);       // write a cell
tile_get(tm, x, y);                 // read a cell (0 OOB)
void tile_load_tileset(tm, path)@io;   // load PNG + auto-slice
void tile_set_solid(tm, idx, is_solid);
tile_is_solid(tm, idx);
```

`tile_load_tileset` takes a PNG arranged in a `tw × th` grid (e.g.,
a 16×16-sprite atlas of 8×8 cells = 128×128 PNG). It slices into
64 sprite frames automatically.

### §39.3 — Rendering

```c
void tile_draw(tm, cam_x, cam_y);   // blit visible cells
```

`tile_draw` computes which cells overlap the viewport (given
camera offset), then draws each via indexed sprite blit. Only
visible cells are rendered — a 1024×1024 map at 8×8 per cell
with a 320×180 viewport draws ~1000 cells, not a million.

### §39.4 — Physics integration

The `solid_mask` array feeds `stbphys` (Cap 41) for collision.

```c
if (tile_is_solid(tm, tile_get(tm, gx, gy))) { block_movement(); }
```

A tile index of 3 could be "grass" (not solid) while 4 is "wall"
(solid). Solid mask flips independently of the rendering — the same
tileset art can be recycled with different collision semantics.

### §39.5 — Save / load

```c
void tile_save(tm, path)@io;       // JSON
tile_load(path)@io;                 // → Tilemap
```

JSON format stores dimensions, tile size, and the raw byte grid
(base64-ish hex). Small enough for git to diff cleanly. Level
editors (ModuLab) read/write this.

### §39.6 — What this chapter does NOT cover

- **Infinite worlds / chunks** — tilemaps are bounded. For
  streaming terrain, split into multiple Tilemaps and load on
  demand.
- **Multi-layer tilemaps** (separate foreground / background /
  decoration) — compose by creating N tilemaps and drawing them
  in order. The module doesn't wrap that for you.
- **Auto-tiling** (bitmask-based tile selection from neighbors).
  Tilemap editors handle this at authoring time; no runtime support.

---

## Cap 40 — ECS (stbecs)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbecs.bsm` is a minimal entity-component system — fixed-capacity,
structure-of-arrays (SoA), parallel-array layout. No queries, no
iteration kernels, no bitmask masks per component. Just flat arrays
indexed by entity ID.

### §40.1 — The World type

```c
struct World {
    capacity, count,          // max entities, current live count
    alive,                    // 1 byte per ID: 0 = dead, 1 = live
    free_ids, free_count,     // reusable ID stack
    pos_x, pos_y,             // position components (all entities have these)
    vel_x, vel_y,             // velocity
    flags                     // per-entity bit flags
}
```

All arrays are `capacity`-sized. Dead slots stay allocated, just
flagged `alive[id] = 0`.

### §40.2 — Lifecycle

```c
ecs_new(capacity);            // create world with N entity slots
void ecs_free(w);
ecs_spawn(w);                 // → new entity ID, or -1 if full
void ecs_despawn(w, id);      // mark dead, recycle ID
ecs_alive(w, id);             // 1 or 0
```

ID recycling uses a LIFO stack — `despawn` pushes the freed ID back,
`spawn` pops the most recent. Gives good cache locality when many
entities come and go.

### §40.3 — Component access

No getters/setters — just index the arrays:

```c
w.pos_x[id] = 100;
w.vel_y[id] = -5;
// flags use bit ops
w.flags[id] = w.flags[id] | FLAG_JUMPING;
```

Raw array access is FAST — single load/store, no function call,
no bounds check in release mode. The tradeoff: you must validate
IDs yourself where they come from untrusted sources (save games,
network messages). Inside the game loop, loops over `0..count`
with alive checks are safe by construction.

### §40.4 — Typical game loop usage

```c
// Physics pass: update position from velocity.
auto id;
for (id = 0; id < w.count; id = id + 1) {
    if (peek(w.alive + id) == 0) { continue; }
    w.pos_x[id] = w.pos_x[id] + w.vel_x[id];
    w.pos_y[id] = w.pos_y[id] + w.vel_y[id];
}

// Render pass: draw each live entity.
for (id = 0; id < w.count; id = id + 1) {
    if (peek(w.alive + id) == 0) { continue; }
    draw_sprite(...);
}
```

The SoA layout is cache-friendly: the position loop touches only
pos_x + pos_y arrays, never pulling velocity/flags into cache. At
10k entities, this is measurably faster than AoS struct-per-entity.

### §40.5 — Adding components

No dynamic component registration. To add a new field (say, `hp`),
edit the `World` struct + allocation + init pattern. This is
deliberately static — matches B++'s "no reflection, no dynamic
layout" philosophy.

For variable-per-entity data (inventory, dialogue choice history),
store an index into a side table: `w.inventory_idx[id]` points into
a global `inventory_pool`.

### §40.6 — What this chapter does NOT cover

- **Queries / filters** (give me all entities with FLAG_ENEMY and
  health < 10). Loop manually with the flag check.
- **Parallel iteration** across components (SIMD over pos_x array).
  Use `vec_*` intrinsics (Cap 23 / Wave 18) if your loop is
  profile-hot.
- **Archetypes** (grouping entities by component set). Not shipped —
  flat arrays for everyone.

---

## Cap 41 — Physics (stbphys)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbphys.bsm` is a minimalist AABB physics integrator. Point-on-tile
collision via `tile_is_solid`, gravity as a constant, velocity +
acceleration integrated per frame. Think Celeste-style platformer
physics, not Box2D.

### §41.1 — The Body type

```c
struct Body {
    x, y,           // position (center, integer pixels)
    vx, vy,         // velocity (pixels per second, integer)
    w, h,           // collision box (width, height)
    grounded,       // 1 if standing on solid this frame
    facing          // -1 or 1, last non-zero horizontal input
}
```

### §41.2 — API

```c
body_new(x, y, w, h);         // allocate a Body at (x,y) with bbox
void body_free(body);
void body_step(body, tm, dt_ms);   // one physics tick
```

`body_step` does:
1. Apply gravity: `vy += gravity * dt`
2. Apply drag: `vx *= drag_factor` (integer approximation)
3. Integrate position: `x += vx * dt`, `y += vy * dt`
4. Resolve collision against the tilemap (checks 4 corners + edges)
5. Update `grounded` flag based on downward collision

### §41.3 — Tuning constants

Globals set by `init_phys`:
- `_phys_gravity` — 800 (px/s²)
- `_phys_max_speed` — 600 (px/s horizontal cap)
- `_phys_jump_velocity` — -350 (initial vy on jump)
- `_phys_drag_ground` — 220/256
- `_phys_drag_air` — 245/256

Override before `init_phys` or tweak directly for per-character feel.

### §41.4 — Typical platformer loop

```c
body_step(player, tilemap, dt);
if (key_down(KEY_LEFT))  { player.vx = -200; }
if (key_down(KEY_RIGHT)) { player.vx = 200; }
if (key_pressed(KEY_SPACE) && player.grounded) {
    player.vy = _phys_jump_velocity;
}
```

### §41.5 — What this chapter does NOT cover

- **Rotation** (bodies are axis-aligned rectangles only).
- **Joints / constraints** (pin, spring, etc.). Not shipped — static
  physics.
- **Dynamic vs kinematic split** — every body is dynamic.
- **Continuous collision** (tunneling prevention for high-velocity
  small objects). Use `body_step_sub(n)` to divide into sub-steps
  manually if needed.

---

## Cap 42 — Pathfinding (stbpath)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbpath.bsm` is an A* pathfinder on grid maps. 4-connected
neighborhood (up/down/left/right), Manhattan heuristic, unit edge
costs. Optimized for small-to-medium grids (up to ~256×256);
beyond that, flow fields or HPA* would beat it.

### §42.1 — The PathFinder type

```c
struct PathFinder {
    w, h,                     // grid dimensions
    cells,                    // byte grid: 0 = walkable, 1+ = blocked
    g_score, f_score,         // A* scores per cell
    came_from,                // parent cell for path reconstruction
    heap, heap_pos, heap_cnt, // open-set binary heap
    closed                    // closed-set bitmask
}
```

One PathFinder per search context — reuse across queries (same
dimensions) to avoid re-allocation.

### §42.2 — API

```c
path_new(w, h);                               // create finder
void path_free(pf);
void path_set_blocked(pf, gx, gy, blocked);
path_is_blocked(pf, gx, gy);
path_find(pf, sx, sy, tx, ty, out_xs, out_ys, max_len);   // → path length
```

`path_find` returns the path length (number of cells) or 0 if no
path. The cells are written to `out_xs[0..len-1]` and
`out_ys[0..len-1]`. Order: start → ... → target.

`max_len` is the caller-supplied output buffer size. Paths longer
than `max_len` truncate — cheap safeguard against infinite
backtracking bugs.

### §42.3 — Heuristic and constants

```c
const PATH_INF = 2000000000;   // represents unreachable
```

Manhattan distance: `|dx| + |dy|`. Admissible + consistent for
4-connected unit-cost grids.

### §42.4 — Performance

A 100×100 grid with ~50% obstacles computes a full cross-map path
in ~2-5 ms on Apple Silicon. Dominated by heap operations. The
heap is a classic binary-heap priority queue with decrease-key
via an inverse map (`heap_pos`).

For very large maps or real-time reactive pathing, consider:
- **Reusing scores** across queries when obstacles don't change
- **Jump-point search** (JPS) variant — 8-connected only, not
  shipped
- **Flow fields** for many-to-one targeting (hordes chasing player)

### §42.5 — What this chapter does NOT cover

- **8-connected movement / diagonals** — 4-connected only. For
  diagonals, add 4 neighbor offsets + adjust heuristic to
  Chebyshev distance.
- **Variable costs** (sand costs 2, water costs 5). Expand
  `g_score` delta per cell type.
- **Multi-agent pathing** (cooperative, traffic-avoidant). Out of
  scope — compute per-agent serially.

---

## Cap 43 — Pool allocator (stbpool)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbpool.bsm` is a fixed-element-size pool allocator. Pre-allocate
N slots of size B, hand out pointers from a free list, return
pointers by adding back to the free list. O(1) alloc/free, zero
fragmentation, trivial to reset.

### §43.1 — API

```c
pool_new(elem_size, capacity);
void pool_free(pool);
pool_alloc(pool);                  // → pointer to a slot, or 0 if full
void pool_release(pool, ptr);      // return a slot to the free list
pool_reset(pool);                  // return all slots
```

### §43.2 — Typical use

Bullet pools, particle systems, damage-number pop-ups. Anything
where:
- Element size is uniform
- Max concurrent count is known (or a reasonable cap exists)
- Allocation speed matters more than memory overhead

```c
auto pool;
pool = pool_new(sizeof(Bullet), 256);

// Shoot
auto b: Bullet;
b = pool_alloc(pool);
if (b != 0) { b.x = 0; b.y = 0; b.vx = 10; /* ... */ }

// Despawn (off-screen, expired)
pool_release(pool, b);
```

### §43.3 — Memory layout

A single malloc block holds `elem_size * capacity` bytes. The free
list is a pointer chain through the first 8 bytes of each free
slot — no separate index table, no header per slot.

### §43.4 — What this chapter does NOT cover

- **Variable-size allocations** — use `bpp_arena` for those.
- **Thread safety** — single-threaded. For SPSC, pair with a ring.
- **Generation counters** — no ABA protection on free-list. If a
  caller holds a stale pointer across a release + realloc, undefined
  behavior. Pair with `stbasset`-style generation handles if that
  matters.

---

## Cap 44 — Color math (stbcol)

*Depends on: Cap 2 (auto-injected prelude)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbcol.bsm` is pure color math — no allocations, no state, just
functions. RGBA packing / unpacking, blending, HSV conversion.

### §44.1 — Packing

```c
rgba_of(r, g, b, a);       // pack 4 bytes into a single ARGB word
rgba_r(color);             // extract channel
rgba_g(color);
rgba_b(color);
rgba_a(color);
```

### §44.2 — Blending

```c
rgba_blend(dst, src);                 // standard over-compositing
rgba_lerp(a, b, t);                   // 0..255 interpolation
rgba_multiply(a, b);                  // modulation
rgba_mix(a, b, ratio_of_256);
```

`rgba_blend` implements the Porter-Duff `over` operator using the
src's alpha: `out = src.rgb + dst.rgb * (1 - src.a)`. Integer
arithmetic throughout.

### §44.3 — HSV

```c
hsv_to_rgba(h, s, v, a);        // hue 0..360, sat/val 0..255
rgba_to_hsv(color, out_h, out_s, out_v);
```

Useful for color pickers, hue shifting, rainbow effects.

### §44.4 — Predefined constants

```c
BLACK, WHITE, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, GRAY
PURPLE, ORANGE, PINK, LIME, NAVY, TEAL, MAROON
TRANSPARENT            // = 0
```

Declared in `init_color()` (called by `game_init`).

### §44.5 — What this chapter does NOT cover

- **sRGB gamma correction** — all math is linear in integer space.
- **Color space conversions** (LAB, XYZ, OKLCH). Out of scope.
- **HDR / wide-gamut** colors. 32-bit integer ARGB only.

---

## Cap 45 — Asset hub (stbasset)

*Depends on: Cap 37 (stbimage), Cap 31 (stbsound), Cap 38 (stbpal)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbasset.bsm` is the game-wide asset registry — sprites, sounds,
music, fonts are loaded once, assigned a handle, and looked up by
path. Duplicates are de-duped automatically; reloads are cheap.

### §45.1 — The AssetSlot type and handle shape

```c
struct AssetSlot { kind, generation, ptr, w, h, mtime }
```

Handles are packed 32-bit integers: `(generation << 16) | slot`.
The generation counter increments on reuse, so stale handles
from a replaced asset return 0 from lookups instead of aliasing.

```c
ASSET_NONE = 0;           // empty slot
ASSET_SPRITE = 1;         // GPU texture handle
ASSET_SOUND = 2;          // stereo s16 PCM buffer
ASSET_MUSIC = 3;          // looping BGM buffer
ASSET_FONT = 4;           // font atlas + metrics
```

### §45.2 — Public API

```c
void init_asset();                           // called by game_init
asset_load_sprite(path)@io;                // → handle or 0
asset_load_sound(path)@io;                 // → handle
asset_load_music(path)@io;                 // → handle
asset_load_font(path, size)@io;            // → handle
void asset_release(handle);                  // explicit cleanup

asset_sprite(handle);                        // → GPU texture
asset_sound_buf(handle);                     // → PCM buffer
asset_sound_frames(handle);                  // → frame count
asset_music_buf(handle);
asset_music_frames(handle);
asset_font(handle);
asset_w(handle); asset_h(handle);
```

Path dedup: `asset_load_sprite("assets/player.png")` called twice
returns the same handle — the internal `hash_str_new(512)` table
keeps path → slot mapping.

### §45.3 — Hot reload

```c
void asset_check_reload();   // call once per frame (dev builds)
```

Walks every loaded asset, compares file mtime against the recorded
mtime. Changed files get re-loaded in place — the sprite's GPU
texture gets re-uploaded, the sound's buffer swapped, etc. Handles
stay valid across reload (same slot, same generation).

This is the backbone of ModuLab's "edit sprite → see in game" loop.
Expect ~5-10 ms cost per frame with 256 assets — trivial for dev
builds, skip the call in release.

### §45.4 — What this chapter does NOT cover

- **Async loading** — every load blocks. For giant asset packs,
  split into initial + streaming tiers manually.
- **Compression / archive formats** (ZIP, custom bundles). Assets
  are loose files on disk.
- **Content hashing / integrity**. No checksum verification — a
  corrupt PNG trips `stbimage`'s decode error, other formats may
  misbehave silently.

---

## Cap 46 — Level forge (stbforge)

*Depends on: Cap 45 (stbasset)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbforge.bsm` is the level-editor and character-editor substrate
that powers ModuLab's testbed. Holds the data model for characters
(sprites + pivots + hitbox + animations + controls), records
gameplay for replay, and renders the testbed world.

### §46.1 — The Character type

```c
struct Character {
    sprite_path,              // PNG asset path
    pivot_x, pivot_y,
    hit_x, hit_y, hit_w, hit_h,
    gravity, max_speed, jump_velocity,
    ctrl_left, ctrl_right, ctrl_jump,   // KEY_* enum IDs
    animations, anim_count              // Animation array + length
}

struct Animation { name_ptr, frames, frame_count, fps, loop }
```

One Character per editable in-game actor. Animations each hold a
sequence of sprite frame indices + playback parameters.

### §46.2 — Public API

```c
character_new();
void character_free(cp);
void character_set_sprite(cp, path);
void character_set_pivot(cp, x, y);
void character_set_hitbox(cp, x, y, w, h);
void character_set_physics(cp, gravity, max_speed, jump_vel);
void character_set_control(cp, ctrl_left, ctrl_right, ctrl_jump);

character_add_animation(cp, name);          // → animation pointer
void character_set_frames(ap, frames, n);   // copy frames into animation
character_animation_count(cp);
character_animation_at(cp, idx);
```

### §46.3 — Save / load (JSON)

```c
character_save(cp, path)@io;     // → 0 on success, -1 on fail
character_load(path)@io;         // → Character or 0
```

JSON format via `bpp_json` writer (Phase D — no hand-rolled
serialization). Deterministic field order, line-oriented, git-
diffable.

### §46.4 — Testbed runtime

The testbed is the "play the character you just edited" loop.
Fixed 32×18 tile world with a ground plus three ledges — lets the
user verify gravity, jump height, collision in a repeatable scene.

```c
void testbed_init(character_ptr);
void testbed_tick(dt_ms);
void testbed_draw();
void testbed_end();
```

### §46.5 — Recorder / replay

For Rhythm Teacher and deterministic-testing flows, stbforge
records per-frame input + position so a level can be replayed
byte-identically.

```c
void rec_start();
void rec_tick(...);
void rec_stop();
rec_save(path);
rec_load(path);
```

### §46.6 — What this chapter does NOT cover

- **Animation preview UI** — stbforge holds the data, ModuLab does
  the editor UI (Cap 27 — Tools).
- **Tween / easing curves**. Frame-indexed animation only, no
  inter-frame interpolation.

---

## Cap 47 — GPU render (stbrender)

*Depends on: Cap 38 (stbpal)*
*Source: new*
*Status: COMPLETE — 2026-04-22*

`stbrender.bsm` is the GPU-side rendering layer — where supported
by the platform (Metal on macOS today; Vulkan on Linux deferred).
Enables indexed-sprite GPU upload, 3D-coordinate quads, and
per-frame vertex streaming for particle / UI systems that want to
skip the CPU rasterizer.

### §47.1 — Public API

```c
void render_init();
void render_begin();
void render_end();
void render_clear(color);
void render_rect(x, y, w, h, color);
void render_circle(cx, cy, r, color);
void render_line(x1, y1, x2, y2, color);
void render_circle_outline(cx, cy, r, color);
void render_rect_outline(x, y, w, h, color);
void render_sprite(handle, x, y, w, h);
void render_sprite_indexed(handle, pal, x, y, w, h);
void render_text(text, x, y, sz, color);
render_measure(text, sz);    // measure width in pixels
```

Parallel to `stbdraw`'s CPU API, but dispatched to GPU when
available. Fall back: if no GPU backend is installed,
`render_init` is a no-op and every `render_*` call routes to the
matching `draw_*` equivalent (so games compile and run either way).

### §47.2 — Indexed sprites on the GPU

The indexed rendering path uploads the palette once as a small
texture (256×1 ARGB), the sprite as an 8-bit texture, and the
shader samples `palette[sprite[uv]]` per fragment. This matches
how Doom64 / SNES emulators draw: one 8-bit fetch + one palette
lookup per pixel, zero CPU cost.

### §47.3 — What this chapter does NOT cover

- **Custom shaders / post-processing**. The current pipeline is
  fixed: textured quads with palette LUT. Post effects would be
  an extension.
- **3D rendering**. The coordinate system is 2D-only today.
- **Compute shaders**. Out of scope.
- **Batch rendering of many small sprites** (particle system). For
  < 1000 sprites per frame the per-call overhead is fine; beyond
  that, a proper instanced-quad pipeline would help.

---

# Appendices

## Appendix A — Compiler Flags Reference

*Source: legacy_docs/how_to_dev_b++.md Part 8*
*Status: COMPLETE — 2026-04-27*

| Flag | Effect |
|------|--------|
| (default) | Native Mach-O ARM64 binary, modular pipeline |
| `--linux64` | Cross-compile to Linux x86_64 ELF |
| `--c` | Emit C code to stdout |
| `--asm` | Emit ARM64 assembly to stdout |
| `--bug` | Emit `.bug` debug map alongside the binary |
| `--monolithic` | Force single-pass pipeline (all modules at once) |
| `--show-deps` | Print module dependency graph and exit |
| `--show-promotions` | Print `auto → extrn/global` promotions |
| `--show-phases` | Print per-function phase classification |
| `--stats` | Print module/function/token counts to stderr |
| `--clean-cache` | No-op, kept for backward compat with scripts |
| `-o <name>` | Output filename |

## Appendix B — xfail test convention

*Status: COMPLETE — 2026-04-27*

Place `// xfail: EYYY` on the **first line** of a test file to declare that the test expects the compiler to emit error code `EYYY` rather than producing a successful binary.

`run_all.sh` treats the test as **passing** when the compiler emits exactly that error code, and **failing** if the compiler succeeds or emits a different error.

```bpp
// xfail: E201
// Verifies that calling an undefined function is rejected.
main() {
    undefined_function();
}
```

Use xfail tests to lock in rejection behavior: they catch regressions where a previously-invalid program starts compiling silently.

---

