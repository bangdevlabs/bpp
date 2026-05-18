# B++

> **Read the book**: [`docs/manual/how_to_dev_b++.md`](docs/manual/how_to_dev_b++.md) — single unified manual, 28 chapters, 6 parts.
> Language basics → stdlib reference → writing pro B++ → building the compiler → architecture → ecosystem.
> Everything you need in one file. No other canonical doc.

### B++ 0.24x — Editor toolkit + content infrastructure complete — stbui v2 ships, parser overflow killer — 17 May 2026

A ten-day arc closed the gap between "compiler can render" and "you can author content for it." Five intertwined threads landed: Bang 9 IDE matured (Levels gained an entity layer, Effects gained fxlab's live slider tuner, every embedded tool inherits a clean panel contract), the language picked up storage classes + polymorphic numeric literals + cast builtins + struct newtypes (the Excalibur Arc), the runtime scaled to "AAA RTS scale" via ECS archetype + SIMD batching + flow-field pathfinding (the RTS Stress Arc), Wolf3D Phase 2 added enemies + billboard sprites + AI on top of the GPU pipeline, and the WC1 mod walks an end-to-end content loop with hot-reload between Bang 9's tabs and the running game.

**stbui v2 — Clay-inspired declarative layout (today's headline).** The original immediate-mode `gui_*` widget family forced every consumer to hardcode pixel positions; ~500 call sites across Bang 9, Modulab, level_editor, fxlab, and the Aseprite viewer all fed the same friction. v2 ships as an additive layer in the same cartridge — declare a tree of `ui_box` / `ui_row` / `ui_col` containers with FIT / GROW / FIXED / PERCENT sizing rules and a four-pass solver computes positions for you. The level_editor, Modulab, and Aseprite viewer all migrated today. Side-panel layouts now survive zoom, host resize, and embed-into-Bang-9 transitions without manual reflow code.

**Parser fbuf overflow killed.** Routine work surfaced a SIGSEGV compiling pathfind in Bang 9 — bisected to import-chain content exceeding the parser's 256 KB fixed buffer, silently corrupting heap regions used by `arr_push`. Fix: `process_file` now mallocs a content buffer sized exactly to the file (`sys_lseek(SEEK_END)` → `malloc(fsize+1)` → single read), frees on return. Mirrors gcc/clang/rustc per-translation-unit allocation. Bug-class killed at the root; no future import chain can repeat it. ~17 LOC, triple-generation byte-stable bootstrap, suite 174/0/12 native + 138/0/48 C.

**The catalog of what shipped in this 10-day window** (full narratives in [`docs/journal.md`](docs/journal.md); closed sidequests archived to `legacy_docs/`):

- **Phase 6.3 v2 — scoped-cleanup epilogues for `@profile`** (2026-05-08): early return + panic no longer leak open zones.
- **fxlab arc — JSON-driven effect tuner with live hot-reload** (2026-05-09 → 05-10): substrate + standalone GUI tuner + Bang 9 `_panel_fx` between Levels and Code. Slider drag in Bang 9 → `fps_wolf3d` responds in ~30 ms. **Arc CLOSED.**
- **Phase annotation collapse — Multics → Unix** (2026-05-11): 8 user-facing phases → 1 (`@safe`) + `@profile` + default. 700+ sites stripped across 95 files. Tonify Rule 28 captured the meta-lesson (killer-use-case gate).
- **Wolf3D Phase 2 — Sessions 0-2** (2026-05-11): entity layer in level_editor, billboard sprites with per-fragment depth, stbai cartridge for enemy AI.
- **Excalibur Arc Features 1-3** (2026-05-11 → 05-12): polymorphic numeric literals (kills `feedback_const_float_demotes`), cast builtins (`floor_int` / `trunc_int` / `round_int`), struct newtype (`struct WorldPos as Vec2` with compiler-enforced identity).
- **RTS Stress Arc — 5 sessions, ~1010 LOC** (2026-05-12): ECS query iterator + archetype storage + SIMD batching + flow-field pathfinding + system scheduler. Parallelism proven at 57%, perf gate cleared by ~30×.
- **WC1 mod — Sessions 1-5** (2026-05-12 → 05-16): tile renderer + native map format + offline SMS→JSON converter + unit sprite rendering + ECS spawn + camera/input + movement with A* + 8-direction animation via Aseprite pipeline.
- **Storage class shipped** (2026-05-14): `global const` / `static const` slots + E263 (write reject) + E264 (extrn no-def upgrade).
- **Modulab Aseprite pivot** (2026-05-16): Modulab opens Aseprite JSON+PNG sheets, displays tag list + frame preview, double-click hands the frame off to Modulab's main canvas for pixel editing; Cmd+S writes back via `pixels_save_png`. Aseprite-format `.atlas.json` double-suffix retired (single `.json` convention now uniform).
- **stbui v2 — Sessions 1-4** (2026-05-17, today): engine + Aseprite viewer + Modulab editor + level_editor migrations. Parser overflow killer alongside.

**Docs reorganised** (2026-05-17): `docs/` now splits into `docs/manual/` (production references — `how_to_dev_b++.md`, `tonify_checklist.md`, `bang9_space_manual.md`, `asset_formats.md`, `bootstrap_manual.md`, `debug_with_bug.md`, `stb++_lib.md`, `warning_error_log.md`) and `docs/plans/` (active or pending work — `bangscript_plan.md`, `compiler_boost_roadmap.md`, `elf_dynlink_plan.md`, `games_roadtrip.md`, `multicore_state_report.md`, `sidequest_stbui_v2_clay.md`). The journal, the TODO shopping list, and the snake report stay at `docs/` root. Shipped sidequests live in `legacy_docs/` for archival.

**Snake remains the audio showcase** — drum loop recorded inside mini_synth (the polyphonic synthesizer in the same language) plays during gameplay, with an 880 Hz apple-eat SFX. The rhythm-teacher prototype ships alongside: four drum lanes, demo/play phases, tight hit-window scoring, and a text-file beat-map format. Phase 6's runtime profiler + minisym + panic / stack-trace / `caller_pc` infrastructure powers the Phase 1 HUD.

B++ tools producing content for B++ games, on a stack where every byte — from the PCM decoder to the bus volume to the note scheduler — compiles from pure B++ source.

> **Version 0.24x**: B++ — **produced entirely inside the B++ toolchain by way of [Bang 9](bang9/)** (the acme-inspired IDE that hosts the open tools as panels, itself open-source under Apache 2.0 + trademark).

---

## The Synthesizer

`tools/mini_synth/mini_synth.bpp` — 300 lines, polyphonic, 4 octaves (C3–B6), WAV recording:

```
bpp synthkey.bpp -o synth && ./synth
```

| Control | What it does |
|---------|-------------|
| **Keyboard rows** (ZXCV / ASDF / QWERTY / 1234) | 46 chromatic keys across 4 octaves. Press to sustain, release to stop. 8 simultaneous voices. |
| **LEFT / RIGHT** | Waveform morph: sine (clean) → triangle → sawtooth → square (harsh) |
| **UP / DOWN** | Dirt: clean signal → bitcrush + sample-and-hold decimation |
| **SPACE** | Toggle WAV recording (saves to disk on stop) |
| **ESC** | Quit |

The same audio stack powers the game engine. Snake can play a WAV sample recorded in synthkey. Rhythm Teacher (next) will use the mixer for sample-accurate note timing.

---

## Six Games, One Engine

B++ ships with six games in `games/`, all GPU-accelerated on Metal:

| Game | Folder | What it is |
|------|--------|-----------|
| **Snake** | `games/snake/` | Classic snake with ECS particle effects, high-score ranking, arena + pool allocators, **background music + in-code eat SFX**. Eat the apple, grow the tail, don't bite yourself. |
| **Pathfinder** | `games/pathfind/` | Rat vs cat chase. WASD movement, AI pursuit, collision, ECS particles on impact. Loads palette-indexed JSON sprites. Canonical hot-reload smoke (edit level in Bang 9 → game reflects in ~30 ms). |
| **Platformer** | `games/platformer/` | Side-scrolling platformer with Kenney Pixel Platformer assets (CC0). Real tilemap (stbtile), milli-pixel physics (stbphys), gravity, jumping, parallax, scrolling camera, coin collection, spikes, goal flag. Also ships a `platform_noasset.bpp` version with debug rectangles. |
| **Rhythm** | `games/rhythm/` | Rhythm-genre prototype. Menu → demo (auto-play) → transition countdown → play (snare on F or SPACE). Hit-windows: ±20 ms perfect, ±60 ms ok. Uses stbscene / stbasset / stbmixer music+SFX buses / beat_map text parser. |
| **FPS — Wolf3D** | `games/fps/` | Three builds in the same folder: `fps_3d.bpp` is the CPU raycaster baseline (Phase 1), `fps_3d_gpu.bpp` is the GPU port through CRT post-process + procedural wall textures (Phase 4-6 reference), `fps_wolf3d.bpp` is the Phase 2 build with enemies + billboard sprites with per-fragment depth + AI (`stbai` cartridge). WASD + ←/→ to move, P toggles the runtime profiler HUD, ESC quits. |
| **WC1 (RTS, mod)** | `games/rts1/` | Warcraft 1 mod running on the original Blizzard asset bundle — converted PNG / WAV / MID / map files via `tools/wc1_map_convert/` + `tools/wc1_sprite_convert/`. Peasant walks the forest map with 8-direction A* path animation; hot-reload through Bang 9's Levels + Sprites tabs. Sessions 1-5 shipped (rendering, maps, units, camera, movement); combat / buildings / AI in flight. See `games/rts1/wc1_handoff.md` for the multi-session plan. |

---

## Back to the Future

In 1972, Ken Thompson's B language evolved into C. The rest is history — C conquered systems programming, operating systems, and eventually everything.

But what if a small group of rebels at Bell Labs had taken B in a different direction? What if instead of chasing general-purpose computing, they'd focused on one thing: **making games**?

B++ is that alternate timeline.

A language with the soul of B — every value is a word, no type declarations, no header files — but with 64-bit words, named struct fields, an orthogonal type system, and a compiler that produces native binaries directly. No assembler. No linker. No external tools.

But what if Sean Barrett later joined that group with his STB — all written in B++?

And now we have a standard library that is, itself, a game engine. With audio. And it just played its first chord.

## From Silence to Music

Two weeks ago B++ could draw pixels but couldn't make a sound. The compiler worked, the games ran, but the speakers were silent.

Then we cleaned the house. Eight modules moved from `stb/` into `src/` — array, hash, buf, str, io, math, file, arena — because they weren't game library code, they were foundation. Things every program needs. They became auto-injected: no import line required, just call the function and it's there. On top of them we built `bpp_beat` (a monotonic clock that thinks in milliseconds, microseconds, samples, BPM, and frames), `bpp_job` (a worker pool with lock-free SPSC queues and `mem_barrier` fences), and `bpp_maestro` (a game loop that splits work into solo, base, and render phases — jazz band, not orchestra).

The repo got reorganized. `src/backend/chip/` for instruction encoding, `os/` for syscalls and platform code, `target/` for binary formats, `c/` for the transpiler. The `.bo` module cache — the #1 historical source of bugs — got deleted entirely. Every compile runs from source now. Takes 0.27 seconds. Nobody misses the cache.

The language syntax locked in. `switch` with jump tables. `?:` ternary. `&&`/`||` short-circuit. `static` for module privacy. `void` for functions that return nothing. `extrn` for write-once state. `global` for shared mutable state. Bitfields (`: bit` through `: bit7`). 128-bit SIMD (`: double`, eleven `vec_*` builtins on NEON and SSE2). Every keyword earned its place because a game or tool needed it.

Then came the tonify sweep — a week of reading every line of B++ code written so far and asking "does this say what it means?" 350 functions got `void`. 200 got `static`. 50 got `: base` (verified pure by the compiler). The Node struct that holds the AST shrank 29% by slicing fields to byte-width. The smart dispatch engine — call-graph analysis + fixpoint purity classifier — shrank Snake from 69 KB to 33 KB just by not emitting dead code.

And then we turned the sound on.

`stbaudio` opens the audio device via CoreAudio FFI. A lock-free ring buffer sits between the main thread (producer) and CoreAudio's realtime callback (consumer). `stbmixer` runs eight voices simultaneously — sine, triangle, sawtooth, square — blended by a continuous fader with bitcrush and decimation for that digital dirt. `stbsound` reads and writes WAV files.

We didn't plan to build a synthesizer. The plan said "audio stack, then Rhythm Teacher game." But the infrastructure was strong enough that a 300-line polyphonic synth fell out naturally. 46 chromatic keys across 4 octaves, 8 simultaneous voices, waveform morphing, dirt control, WAV recording. Someone sat down and played chords.

The compiler now understands effects. Every function carries a classification — pure, heap, I/O, GPU, realtime, or solo — propagated through the call graph by a fixpoint lattice. The audio callback is annotated `: realtime`. If someone accidentally adds a `malloc` or a `putchar` inside the callback path, W026 fires at compile time. The click never reaches the speakers.

The debugger found the hardest bug of the sprint. A Mach-O header had `page_count = 1` hardcoded — when the data section grew past 16 KB, string literals silently corrupted. `bug --dump-str` showed the wrong bytes at the call site in one run. Three days of blind archaeology replaced by one command.

41 compiler modules in `src/`. 29 library cartridges in `stb/`. 3 platform layers. 52 diagnostics. 77 keyboard inputs. 174 native + 138 C-emitter tests, zero failures.

The version number is the test count.

## The Language

```bpp
import "stbgame.bsm";

enum Dir { UP, DOWN, LEFT, RIGHT }

auto px, py, facing;

main() {
    game_init(320, 180, "B++ Game", 60);
    px = 160; py = 90; facing = RIGHT;

    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }

        if (key_down(KEY_UP))    { facing = UP; }
        if (key_down(KEY_DOWN))  { facing = DOWN; }
        if (key_down(KEY_LEFT))  { facing = LEFT; }
        if (key_down(KEY_RIGHT)) { facing = RIGHT; }

        if (facing == UP)    { py = py - 2; }
        if (facing == DOWN)  { py = py + 2; }
        if (facing == LEFT)  { px = px - 2; }
        if (facing == RIGHT) { px = px + 2; }

        clear(BLACK);
        draw_rect(px, py, 16, 16, GREEN);
        draw_text("arrows to move", 90, 4, 1, GRAY);
        draw_end();
    }
    return 0;
}
```

Compile and run:

```
bpp game.bpp -o game && ./game
```

No SDL. No raylib. No dependencies. One file in, one native binary out.

## What B++ Has

**Language core:**
- **Typeless words** — every value is a 64-bit slot that can hold an integer, a pointer, or a packed value
- **Optional slices** — `auto x: byte`, `: quarter`, `: half`, `: float`, `: double` (128-bit SIMD), `: bit3` for packed struct fields. One unified ladder from 1 bit to 128 bits
- **Structs with dot access** — `struct Vec2 { x, y }`, `p.x = 10`, field-packing with bit slices
- **Enums** — `enum State { MENU, PLAY, OVER }` resolved at compile time
- **Constants and compile-time functions** — `const W = 320;` and `const abs(x) { ... }` with dead-code elimination lifted to the parser
- **Pointers** — `*(ptr)` dereference, `&var` address-of, `buf[i]` array, `obj.field` struct access. Width-typed reads/writes: `peek_q/h/w` (16/32/64-bit LE), `peekfloat`/`peekfloat_h` (double/float), matching `poke_*` variants
- **Compound operators** — `x++`, `x--`, `x += e`, `x -= e`, `x *= e`, `x /= e`, `x %= e` — desugared in the parser, inherited by all backends
- **Storage classes** — `auto` (default), `static` (module-private), `extrn` (frozen post-init), `global` (worker-shared), `const` (compile-time), `global const` / `static const` (immutable cross-module / module-private slots, shipped 2026-05-14)
- **Effect annotations** — `@safe` (bounded time + no malloc / IO / GPU / syscall — the only user-facing annotation since the 2026-05-11 phase collapse) + `@profile("name") { ... }` (scoped instrumentation, lowers to runtime aggregate). Compiler tracks effects internally for optimization; the user-facing surface is binary
- **Polymorphic numeric literals** — literals carry SHAPE until slot context fixes the type. `const TOL = 0.0001` now preserves float bits cleanly. Kills the `feedback_const_float_demotes` bug class
- **Newtype structs** — `struct WorldPos as Vec2` produces a distinct type with compiler-enforced identity. World vs Grid mixing becomes a compile error, with zero runtime cost
- **Ternary and short-circuit** — `x = a ? b : c;`, `if (p != 0 && p.field > 0)` both lowered in the parser (one implementation, every backend inherits)
- **SIMD** — 11 `vec_*` builtins lowering to NEON (ARM64) or SSE2 (x86_64). `auto v: double` allocates from the SIMD register pool
- **Smart dispatch** — compiler analyzes purity across the call graph and auto-parallelizes worker-safe loops into `job_parallel_for`
- **FFI** — `import "SDL2" { void InitWindow(...); }` calls any C library

**Compiler:**
- **Self-hosting** — the compiler compiles itself, fixed-point verified
- **Native binaries** — ARM64 Mach-O + x86_64 ELF, built-in SHA-256 codesign, zero external toolchain
- **Cross-compilation** — `bpp --linux64 game.bpp -o game` from macOS
- **Own runtime (libb)** — `bmem` allocator via `sys_mmap`, `brt0` startup code, `bsys` syscall tables. Native binaries have zero libc dependency
- **Module discipline** — `static` private to module, `load` vs `import` separates project from library, circular imports are errors (E222), cross-module duplicates are errors (E221)
- **Effect inference** — internal fixpoint classifier tracks purity / IO / GPU / heap / syscall / panic across the call graph. The user-facing surface is binary (`@safe` annotation either holds or fails W026); internal granularity drives optimization decisions (inlining, parallelization candidates)
- **V3 function-pointer type checking** — flow analysis + opt-in `func(...)` annotations + Estrita mode. Compiler proves `fn_ptr(target)` matches the resolved callee's shape; mismatches surface as W028 / E246 at the call site instead of register-bank corruption at runtime
- **Three-backend split** — `chip/`, `os/`, `target/`. Adding Linux ARM64 or Windows is a folder, not a rewrite
- **Diagnostics** — 25 error/warning codes with `file:line:caret` locations, multi-error recovery, 20-error cap
- **Self-compile speed** — ~0.1 seconds from scratch, no cache

**Debugger (`bug`):**
- **Three modes** — `bug --dump file.bug` (text dump), `bug --tui ./prog` (live REPL + watch), plain `bug` (GUI inspector with map browser + watch + viz panels)
- **GDB remote protocol** — debugserver (macOS) + gdbserver (Linux), no entitlements, no codesign dance
- **Always-on debug info** — every native compile writes a `.bug` map next to the binary; no `-g` flag, the compiler always emits it
- **Function tracing** — call depth indentation on every entry; `--break <fn>` for selective stops
- **Watch list + expression evaluator** — `--watch "player.vx,score,*head"` evaluates B++ expressions at every stop (struct fields, array indexing, dereference)
- **Type-aware display** — structs render as field trees, arrays as lists, pointers as addresses + target, floats with full precision
- **Runtime visualizers** — `viz_render_graph` / `viz_render_rgba` etc. for in-target panel rendering, GUI mirrors the same panels live
- **Phase 6 runtime symbolication** — `panic("msg")` writes a stack trace to stderr and exits 134; `profile_start(rate_hz, depth)` samples FP chains via SIGPROF + cooperative hooks across worker threads, `profile_dump` returns the top-N hot frames. No external `.bug` required at runtime — the binary embeds a minisym table

**Audio stack:**
- **stbaudio** — CoreAudio AudioQueue FFI, SPSC ring buffer, realtime callback annotated `: realtime`
- **stbmixer** — polyphonic 8-voice mixer, 4 waveforms (sine/tri/saw/square) with continuous morph, bitcrush + decimation distortion, master volume
- **stbsound** — WAV read/write, RIFF PCM 44100 stereo s16

The first B++ audio program was a 440 Hz sine tone. The second was a 300-line polyphonic synthesizer with WAV recording, 46 keys mapped to 4 chromatic octaves, waveform and dirt controls, zero glitches under load. See `tools/mini_synth/mini_synth.bpp`.

## What B++ Doesn't Have

No classes. No templates. No exceptions. No garbage collector. No package manager. No build system. No 200MB toolchain download.

You write code. You compile it. You run it.

## The Standard B Library (stb)

The name **stb** is a tribute to [Sean Barrett's stb libraries](https://github.com/nothings/stb) — the single-header C libraries that defined a generation of small, focused, dependency-free building blocks for graphics, audio, fonts, and data. `stb_image.h`, `stb_truetype.h`, `stb_vorbis.c`, `stb_ds.h` and the rest are how a lot of indie game developers learned that "the right amount of library" is one file you can read in an afternoon. B++'s standard library is the same idea reframed as a pure-B++ collection: small modules, no headers (B++ has no headers anyway), no third-party dependencies, the minimum API surface for the maximum game.

stb is the game engine. It's not a wrapper around SDL or raylib — it **is** the engine, written entirely in B++. 29 cartridges, pulled in opt-in per program — `import "stbgame.bsm"` brings only the floor (window + input + draw + font + color + strbuf); each cartridge above that is an explicit `import` line (Tonify Rule 23 — convenience couplings hide the dependency graph).

**Rendering:**

| Module | What it does |
|--------|-------------|
| `stbdraw` | Software framebuffer rendering — rects, circles, lines, sprites, text |
| `stbrender` | GPU-accelerated 2D rendering — rects, circles, lines, outlines, text, sprites (Metal) |
| `stbsprite` | GPU sprite loading and rendering — any w×h, palette-indexed, JSON loader |
| `stbshader` | Custom vertex+fragment pipelines — `gpu_pipeline_load(metal_path, vert, frag)` with cwd → install dir → walk-up resolve, `gpu_target_create / present_target` for off-screen rendering |
| `stbfx` | Post-process effect chain — `fx_register / fx_chain_begin / fx_apply / fx_present` ping-pong infrastructure + 4 typed factories (`effect_crt`, `effect_scanlines`, `effect_chromatic`, `effect_dither`) |
| `stbpal` | GPU palette pipeline — 7 built-in catalogs, phase-correct cycling, LUT remap, lerp |
| `stbtexture` | Procedural textures — `texture_brick / stone / wood / solid` + `_to_buf` headless variants |
| `stbraycast` | DDA + RayHit + per-column projection — content-blind cartridge for any 2.5D raycaster |
| `stbprofile` | Runtime profiler HUD — REC indicator, FPS smoothed, sparkline (fixed budget reference), live top-N tally, GPU timing readout, `@profile` zone aggregates |
| `stbfont` | 8×8 bitmap font fallback + pure B++ TrueType reader (cmap, glyf, Bezier, scanline AA) |
| `stbcolor` | Color palette — `rgba()`, named constants (BLACK, WHITE, RED, BLUE, ...) |

**Game loop & input:**

| Module | What it does |
|--------|-------------|
| `stbgame` | Game loop — init, frame timing, quit, virtual-canvas + auto-scaled window, `game_render_begin / end` pixel-perfect helpers |
| `stbwindow` | Standalone window for tools (no game loop, no GPU init) — used by ModuLab / mini_synth / the_bug |
| `stbinput` | Keyboard and mouse input from memory arrays |
| `stbui` | Immediate-mode UI widgets with per-frame arena |
| `stbscene` | Layered backgrounds with parallax — `bg_layer_new(image, factor_x, factor_y)` + `bg_set_camera` + `bg_draw_all` |

**Physics, collision, pathing, AI, camera:**

| Module | What it does |
|--------|-------------|
| `stbcol` | Collision detection — rect overlap, point-in-rect, distance squared |
| `stbphys` | Platformer physics — Body struct, gravity, jump, tile collision, milli-pixel precision + `FPSBody` chapter for first-person motion |
| `stbtile` | Tilemap engine — grid, collision layer, PNG tileset loader, camera-aware GPU draw, type remap |
| `stbpath` | A* pathfinding on a grid — binary min-heap with indexed decrease-key, Manhattan heuristic |
| `stbflow` | Flow-field pathfinding — one BFS compute per goal, all units sample. Drops N×A* to a constant when many units chase the same target (RTS Stress Arc S4) |
| `stbai` | Enemy AI building blocks — wander / chase / attack state machines, line-of-sight checks, shipped with Wolf3D Phase 2 |
| `stbcamera` | 2D camera state + clamp — factored out of per-game `_camera_clamp` after rts1 + platformer hit the same pattern |

**Game infrastructure:**

| Module | What it does |
|--------|-------------|
| `stbpool` | Fixed-size object pool — O(1) get/put via embedded freelist |
| `stbecs` | Entity-component system — spawn/kill/recycle, parallel arrays, milli-unit physics, custom components via `ecs_component_new` |
| `stbscene` | Scene manager — register/switch/load/update/draw/unload with deferred switches |

**Audio:**

| Module | What it does |
|--------|-------------|
| `stbaudio` | Audio device I/O — CoreAudio AudioQueue, SPSC ring buffer, realtime callback |
| `stbmixer` | 10-voice mixer — tones, one-shot samples, looping music, three independent buses (MASTER/MUSIC/SFX), bitcrush + decimation dirt |
| `stbsound` | Audio file formats — WAV read/write (RIFF PCM 44100 stereo s16) |
| `stbmidi` | MIDI playback — XMI/MIDI parser + per-channel tone scheduler routed through `stbmixer`. Drives WC1's converted soundtrack and any future MIDI-driven music |

**Assets:**

| Module | What it does |
|--------|-------------|
| `stbasset` | Handle-based asset manager — `uint32` handles with 16-bit generation, dedup by path, ABA-safe stale detection, one table for sprites/sounds/music/fonts |
| `stbimage` | Pure B++ PNG loader — DEFLATE, Huffman, all filter types, zero FFI |

### AND MANY MORE TO COME!!!!!

### Universal runtime (promoted from stb to src)

Eight modules that started as `stb*` but turned out to be things every B++ program needs — not just games. They migrated from `stb/` to `src/` as `bpp_*.bsm` and became auto-injected for every compile, without requiring an `import` line. They are the compiler's own primitives as much as the game engine's.

| Module | Formerly | What it does |
|--------|----------|-------------|
| `bpp_array` | `stbarray` | Dynamic arrays with shadow header (stb_ds.h style) |
| `bpp_hash` | `stbhash` | Hash maps — word keys (Knuth) and byte-sequence keys (djb2), open addressing, tombstones |
| `bpp_buf` | `stbbuf` | Raw buffer read/write (u8, u16, u32, u64) |
| `bpp_str` | `stbstr` | String operations and growable string builder |
| `bpp_math` | `stbmath` | Vec2, PRNG, sqrt (Newton-Raphson), abs, min, max, clamp, fixed-point trig |
| `bpp_io` | `stbio` | Console I/O (`print_int`, `print_msg`, `print_char`, `putchar`, `getchar`) |
| `bpp_file` | `stbfile` | File I/O — read and write entire files |
| `bpp_arena` | `stbarena` | Generic bump allocator — O(1) alloc, O(1) reset, 8-byte aligned |

Three more `bpp_*` modules ship as core runtime alongside these, with no prior stb life:

| Module | What it does |
|--------|-------------|
| `bpp_beat` | Monotonic clock — milliseconds, microseconds, samples, BPM, frames, from one source of truth |
| `bpp_job` | Worker pool with lock-free SPSC queues and `mem_barrier` fences |
| `bpp_maestro` | Game loop — solo / base / render phase split, fixed-timestep accumulator |
| `bpp_mem` | `malloc`/`free`/`realloc` via `sys_mmap` — the libb allocator that replaces libc on the native path |

**Platform layers** (internal, selected automatically):

| Module | What it does |
|--------|-------------|
| `_stb_platform_macos` | Cocoa window, Metal GPU, texture upload, CoreGraphics software, keyboard/mouse |
| `_stb_platform_linux` | X11 wire protocol (raw socket), terminal ANSI fallback, keyboard input |
| `_stb_audio_macos` | CoreAudio AudioQueue FFI, realtime callback, SPSC ring consumer |

Two rendering paths: `stbdraw` for CPU software rendering (framebuffer → CoreGraphics/ANSI), `stbrender` for GPU-accelerated rendering (Metal on macOS, Vulkan planned for Linux). Same game code — just swap `draw_*` for `render_*` and add `render_init()` after `game_init()`.

GPU rendering uses the platform's native API directly — Metal via `objc_msgSend`, Vulkan via `libvulkan.so`. No SDL. No OpenGL wrappers. The compiler talks to the hardware.

## Four Backends, Same Code

The game code never changes. Only the import:

```bpp
// Native macOS — zero dependencies:
import "stbgame.bsm";

// Linux terminal — ANSI rendering, zero dependencies:
import "stbgame.bsm";
// compile with: bpp --linux64 game.bpp -o game

// SDL2:
import "stbgame.bsm";
import "drv_sdl.bsm";

// Raylib:
import "stbgame.bsm";
import "drv_raylib.bsm";
```

## Getting Started

Install the compiler and standard library manually:

sudo cp bpp /usr/local/bin/                                                                
sudo mkdir -p /usr/local/lib/bpp/stb                                                       
sudo cp stb/*.bsm /usr/local/lib/bpp/stb/                                                  
      
### Install

```bash
# Bootstrap from C source (first time, or if bpp binary is missing)
clang bootstrap.c -o bpp

# Bootstrap + install globally
sh install.sh

# Or install without re-bootstrapping
sh install.sh --skip
```

The `bootstrap.c` file is the compiler emitted as C (~15K lines). It is the seed that builds everything — no external tools needed beyond a C compiler. **Note:** bootstrap.c is currently outdated and needs regeneration (see TODO).

### Play the Games

Each game lives in its own folder under `games/`. Since 0.75, asset paths are resolved relative to the **binary** (via `bpp_path` auto-injected) — you can `cd` into the game folder *or* run the binary from the repo root, both work.

```bash
# Snake with ECS particles, ranking, arena + pool, music loop, eat SFX
cd games/snake
bpp snake_maestro.bpp -o build/snake
./build/snake

# Pathfinder — rat vs cat chase
cd games/pathfind
bpp pathfind.bpp -o build/pathfind
./build/pathfind

# Platformer with Kenney Pixel Platformer assets (CC0)
cd games/platformer
bpp platform.bpp -o build/plat_asset
./build/plat_asset

# Platformer with debug rectangles (no assets needed)
cd games/platformer
bpp plat_noasset.bpp -o build/plat_noasset
./build/plat_noasset

# Rhythm — rhythm-genre prototype
cd games/rhythm
bpp rhythm.bpp -o build/rhythm
./build/rhythm
#  ENTER          start
#  F or SPACE     hit snare (during PLAY phase)
#  ESC            back to menu
```

### Run on Linux (X11 via Docker + XQuartz)

You can cross-compile a B++ game to a Linux x86_64 ELF binary on macOS and run it inside an Ubuntu container, with the window appearing on your Mac via XQuartz. The Linux platform layer talks raw X11 wire protocol over a TCP socket — no `libX11`, no FFI, just `sys_socket` + `sys_connect` + `sys_write`.

**One-time setup on macOS:**

```bash
# 1. Install XQuartz (X11 server for macOS)
brew install --cask xquartz

# 2. Open XQuartz, then enable network connections:
#    XQuartz → Preferences → Security → "Allow connections from network clients"
#    Then quit XQuartz fully and reopen.

# 3. Persist the listen-on-TCP setting (XQuartz defaults to off)
defaults write org.xquartz.X11 nolisten_tcp 0
killall XQuartz 2>/dev/null
open -a XQuartz

# 4. Allow localhost connections to the X server
xhost +localhost

# 5. Install Docker Desktop for Mac if you do not already have it.
```

**Cross-compile and run a game:**

```bash
# Cross-compile snake to a Linux x86_64 ELF binary
bpp --linux64 examples/snake_cpu.bpp -o /tmp/snake_x11

# Run inside Ubuntu with the host XQuartz as the display
docker run --rm \
  --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 \
  -v /tmp:/tmp \
  ubuntu:22.04 \
  /tmp/snake_x11
```

A 320×180 window opens in XQuartz showing snake running on Linux. Arrow keys move the snake, the game speed and behavior are identical to the native macOS build. The window obeys the close button and the FocusOut handler clears stuck keys when you Alt-Tab away.

**The full X11 test suite that ships in `tests/`:**

These were used to verify each phase of the X11 wire-protocol implementation. They are also the easiest way to confirm a working XQuartz + Docker setup on a fresh machine.

```bash
# Phase 1 — dark blue window, ESC or close button to exit
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_window

# Phase 2 — animated rectangles + circle + text
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_draw

# Phase 3 — input test (arrows to move player, mouse to draw yellow ball, ESC to quit)
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_input

# Movement test — prints player position to terminal every 30 frames
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/test_x11_movement

# Snake on Linux — arrow keys to move, eat the apple, grow the tail
docker run --rm --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 -v /tmp:/tmp \
  ubuntu:22.04 /tmp/snake_x11
```

If `docker` is not on your `PATH` after a Docker Desktop install on macOS, the binary lives at `/Applications/Docker.app/Contents/Resources/bin/docker` — substitute that for `docker` in any of the commands above.

**Caveats**:
- Currently only `draw_*` (CPU framebuffer) games work via X11. GPU games (`render_*`) need Vulkan, which is deferred (see TODO).
- macOS Docker has no GPU passthrough — even if Vulkan were implemented, real GPU rendering would require Linux hardware. The X11 path is software-only.
- You can also run without `DISPLAY`: the same binary falls back to ANSI terminal rendering.

### Run via the C Emitter (raylib path, portable C output)

The C emitter (`bpp --c`) translates B++ to portable C99. The right pattern is to write the game using `drv_raylib.bsm` instead of `stbgame.bsm`, so the generated C calls regular C functions (no Objective-C, no `objc_msgSend` calling-convention issues).

**Install raylib and gcc:**

```bash
# macOS
brew install raylib

# Ubuntu / Debian
sudo apt-get install libraylib-dev gcc
```

**Compile and run a game:**

```bash
# 1. Generate C from a raylib-flavored game
bpp --c examples/snake_raylib.bpp > /tmp/snake_raylib.c

# 2. Compile the C with gcc + raylib + objc (objc only on macOS)
gcc -Wno-implicit-function-declaration \
    -Wno-parentheses-equality \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    /tmp/snake_raylib.c -o /tmp/snake_raylib_c \
    -lraylib -lobjc

# 3. Run
/tmp/snake_raylib_c
```

On Linux, drop `-lobjc` and use `/usr/include` / `/usr/lib` paths. The same `.c` file should compile on Windows (with raylib for MSYS2/Visual Studio), BSDs, and Emscripten.

### Debug Any Game

```bash
./bug ./snake               # function tracing + crash report
./bug ./plat_assets         # works on any bpp binary, no flags needed
```

### Write Your Own

```bpp
// CPU rendering (software framebuffer):
import "stbgame.bsm";

main() {
    game_init(320, 180, "My Game", 60);
    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }
        clear(DARKGRAY);
        draw_circle(160, 90, 40, RED);
        draw_text("B++", 140, 82, 2, WHITE);
        draw_end();
    }
    return 0;
}
```

```bpp
// GPU rendering (Metal on macOS, Vulkan on Linux):
import "stbgame.bsm";
import "stbrender.bsm";

main() {
    game_init(320, 180, "My Game", 60);
    render_init();
    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }
        render_begin();
        render_clear(DARKGRAY);
        render_circle(160, 90, 40, RED);
        render_rect(60, 140, 200, 20, BLUE);
        render_end();
    }
    return 0;
}
```

```
bpp mygame.bpp -o mygame && ./mygame
```

## Project Structure

```
b++/
├── src/                        — Compiler core + universal runtime (39 B++ modules)
│   ├── bpp_*.bsm               — Core utilities (array, hash, buf, str, io, math, file, arena)
│   ├── bpp_beat/job/maestro    — Clock, worker pool, game loop orchestrator
│   ├── bpp_runtime.bsm         — panic + sampling profiler + runtime PC resolution (Phase 6)
│   ├── bug_*.bsm               — Debugger engine (reader, gdb, observe, viz, tui, eval, brk, runviz)
│   └── backend/
│       ├── chip/aarch64/       — ARM64 encoder + codegen
│       ├── chip/x86_64/        — x86_64 encoder + codegen
│       ├── os/macos/           — libSystem FFI, platform layer, audio
│       ├── os/linux/           — X11 wire protocol, syscalls
│       ├── target/aarch64_macos/ — Mach-O writer (LC_UUID + __minisym section)
│       ├── target/x86_64_linux/  — ELF writer (PT_NOTE GNU build_id + BPPMINI)
│       └── c/                  — C transpiler (portable escape hatch)
├── stb/                        — Standard B Library (29 cartridges, game engine)
├── stb/shaders/                — Metal shaders shipped with stb (pp_blit, fx_*, fps_raycast, bg_layer)
├── tools/mini_synth/           — Polyphonic synthesizer (300 lines)
├── tools/the_bug/              — `bug` debugger (CLI + GUI, single binary)
├── tools/modulab/              — Pixel sprite editor (Save / Save As / Open + frame strip + palette)
├── games/                      — Complete playable games
│   ├── snake/                  — Snake + ECS particles + ranking + music + SFX
│   ├── pathfind/               — Rat-and-cat chase with AI pursuit
│   ├── platformer/             — Side-scrolling platformer with Kenney assets
│   ├── rhythm/                 — Rhythm-genre prototype (menu → demo → play)
│   └── fps/                    — Wolf3D scaffold (Phase 1 in-flight, see HANDOFF.md)
├── bang9/                      — Acme-inspired IDE that hosts the open tools as panels
├── examples/                   — Small demos (hello, mouse, gpu_colours, raylib/sdl)
├── drivers/                    — Backend drivers (SDL2, raylib — optional)
├── tests/                      — Compiler and library tests (174 native + 138 C, 0 failures)
├── docs/                       — The unified book (how_to_dev_b++.md, 28 chapters) + journal + TODO
├── legacy_bootstrap/           — Earlier compiler bootstrap + archived planning docs. Read-only.
├── bpp                         — The compiler binary
└── bug                         — The debugger binary
```

## Platform Status

| Target | Status |
|--------|--------|
| macOS ARM64 (Apple Silicon) | **Working** — native Cocoa + Metal GPU, SDL2, raylib |
| Linux x86_64 | **Working** — static ELF, terminal rendering. GPU (Vulkan) in progress |
| Windows x86_64 | Planned — codegen + Win32 + DX12/Vulkan |
| WebAssembly | Future — codegen + WebGPU |

GPU access is native per platform — Metal on macOS, Vulkan on Linux, DirectX 12 on Windows. No middleware. The compiler talks to the hardware directly via FFI. stb modules are 100% platform-agnostic. Each target only needs a codegen backend + platform layer.

## Requirements

**macOS ARM64:**
- **No dependencies** for native games
- **SDL2** (`brew install sdl2`) for SDL examples
- **raylib** (`brew install raylib`) for raylib examples

**Linux x86_64:**
- Cross-compile from macOS: `bpp --linux64 game.bpp -o game`
- Run natively or in Docker — 45KB static binary, zero dependencies

## The Compiler

The B++ compiler is written in B++ and compiles itself. It produces
native ARM64 Mach-O binaries with built-in SHA-256 codesigning.

```
bpp source.bpp -o binary       # native ARM64 macOS (default; .bug map is always written)
bpp --linux64 src.bpp -o bin   # cross-compile to x86_64 Linux ELF
bpp --c source.bpp              # emit portable C (no .bug — instruction layout decided downstream)
bpp --asm source.bpp            # emit ARM64 assembly
bpp --show-deps source.bpp     # print module dependency graph
```

39 compiler core modules in `src/` + backend split under `src/backend/chip/<arch>/` + `os/<os>/` + `target/<arch>_<os>/` + 25 stb library modules. Self-hosting verified at every commit (`shasum gen1 == gen2`). The `.bo` module cache was retired in 0.23.x — every compile runs from source, ~0.27 s for the whole compiler. Import search paths: `./`, `stb/`, `drivers/`, `src/`, `src/backend/chip/<arch>/`, `src/backend/os/<os>/`, `src/backend/target/<arch>_<os>/`, `/usr/local/lib/bpp/` and its subfolders.

### The Debugger

```
bug                              # GUI: pop a file picker, browse a .bug map
bug file.bug                     # GUI: open with .bug pre-loaded
bug --dump file.bug              # text dump of every section the .bug carries
bug --tui ./program              # live REPL on every breakpoint (watch + step + locals)
bug --tui --break update_enemy ./program     # selective breakpoints
bug --tui --watch "player.vx,score" ./game   # live expression watch on every stop
```

`bug` is a B++ program that uses the GDB remote protocol to communicate with Apple's debugserver (macOS) or gdbserver (Linux). No entitlements, no codesign, no special flags needed. Function names come from the Mach-O/ELF symbol table automatically; the `.bug` map adds locals, struct layouts, source positions, viz hints, and the embedded minisym table the runtime uses for `panic`/`profile_*`. See `docs/manual/debug_with_bug.md` for the full feature tour.

## Contributing

B++ is open source and we want your help. The language is young and there's a lot to build:

**Platform ports** — Each new platform needs a codegen backend (instruction encoder + binary writer) and a platform layer (window, present, input, timing). ARM64 macOS and x86_64 Linux are done. Windows/Win32 and WebAssembly are open.

**Game tools** — Sprite editors, tilemap editors, level designers, particle systems — all built in B++ using stb. The language was made for this.

**stb modules** — Audio (`stbaudio`), tilemaps (`stbtile`), networking (`stbnet`), physics, pathfinding. Each is a pure B++ module with no platform dependencies.

**Games** — The best way to find what's missing in the language. Build a game, hit a wall, fix the compiler. That's how every feature in B++ was born.

**Documentation** — Tutorials, examples, guides. Especially for people coming from C, Lua, or Python game dev.

### How to start

1. Fork the repo
2. Pick something from the list above
3. Build and test (`bpp src/bpp.bpp -o bpp` to rebuild the compiler)
4. Open a PR

No bureaucracy. No code of conduct longer than the compiler. Just build cool stuff.

## License & Governance

B++ is distributed under the **Apache License, Version 2.0** — see
[`LICENSE`](LICENSE) for the full text.

Three additional documents cover the full legal and organizational
picture:

- [`LICENSE`](LICENSE) — Apache 2.0. You may use, modify, and
  redistribute B++ freely, including for commercial purposes.
  Patent grant and attribution requirements apply.
- [`TRADEMARK.md`](TRADEMARK.md) — the names **B++**, **Bang**,
  and **BangDev** are trademarks. Code is open; names are
  protected. Fair use allowed (write tutorials, say "Built with
  B++"), but you can't name your fork "B++".
- [`GOVERNANCE.md`](GOVERNANCE.md) — project direction is set
  by the BDFL (Daniel Obino). Contributions welcome via the RFC
  process. The model may evolve toward a committee as the project
  grows, but that is aspirational, not scheduled.
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — how to submit patches,
  coding conventions, testing discipline.

Copyright 2026 Daniel Obino / BangDev.

### Note on Bang 9

**Bang 9** is BangDev's acme-inspired IDE for B++ — an editor +
project host that embeds the open tools (ModuLab, level editor,
mini_synth, font forge) as plugin panels so artists and designers
can ship a complete B++ game without leaving a single window.
Lives at [`bang9/`](bang9/) in this repo and is distributed under
the same Apache 2.0 + trademark umbrella as the rest of the
project. **Code is open. The name "Bang 9" is reserved for the
canonical distribution** — forks welcome, just rename your fork.

---

## Timeline

B++ is ~50 days old. The following are the milestones, in order:

| Date | Milestone |
|------|-----------|
| **Mar 18** | First parser written in B++. Bootstrap stage 2 reached. |
| **Mar 20** | **Self-hosting** — compiler compiles itself, fixed-point verified. |
| **Mar 23** | Zero-dependency native compilation (ARM64 Mach-O, built-in codesign). |
| **Mar 24** | Native game rendering with no external libraries. |
| **Mar 25** | x86_64 Linux cross-compilation. Clang-style diagnostics with source locations. |
| **Mar 26-27** | Modular compilation + dynamic arrays + type hints + packed structs + mouse input. |
| **Mar 27** | **Type system final** — base × slice grid, orthogonal composition, f16/f32/f64 float codegen. |
| **Mar 31** | **GPU rendering via Metal** — native 2D pipeline, platform-agnostic trig. |
| **Apr 1** | Syntax sugar — `buf[i]`, `for`, `else if`, string escapes. |
| **Apr 2** | **Bug debugger v2** — debugserver/gdbserver via GDB remote protocol, zero flags, crash backtrace, local variables. |
| **Apr 3** | Pure B++ TrueType reader. Metal glyph atlas. Bell Labs malloc/free/realloc. |
| **Apr 5** | GPU sprites any size. Pure B++ `sqrt` via Newton-Raphson. Pathfinder game ported. |
| **Apr 6** | **B++ 0.2** — Tilemap engine, platformer physics, address-of operator, PNG tRNS transparency, Kenney Pixel Platformer (CC0). |
| **Apr 6** | **B++ 0.21** — Linux X11 (wire protocol, Unix + TCP socket, 1160 lines). Snake runs on Linux via Docker + XQuartz. |
| **Apr 6** | stbhash + stbpath + O(1) compiler symbol tables. Auto-injected platform layer. |
| **Apr 8** | **B++ 0.22** — Maestro Phase 1 (worker pool, SPSC queues, jazz metaphor). `global` keyword. Four structural compiler bugs fixed. |
| **Apr 9** | **B++ 0.23** — Language syntax locked in. `load`/`static`/`void`/`:base`/`:solo`. Smart dispatch engine (fixpoint classifier + auto-promotion). 8 MB arena-backed AST. Lazy emit shrinks Snake 52%. |
| **Apr 11** | Tonify batches 1-5. `switch` with jump tables. Multi-error recovery. Node struct sliced 29% smaller. Maestro accumulator pattern. |
| **Apr 13-14** | **Foundation refactor** — `.bo` cache deleted (#1 bug source across history). Repo reorganized into chip/os/target/c. `bsys`/`brt0`/`bmem` libb replaces libc on the native path. Switch jump tables. |
| **Apr 14** | **Mini Cooper Phase A + B0** — Bitfields (`: bit3`). `malloc_aligned`. `T_TERNARY` side quest closes short-circuit divergence. Const fold + DCE lifted to parser. |
| **Apr 15-16** | **Mini Cooper B1-B4 complete** — Sethi-Ullman register allocation. Inline of small `: base` helpers. Local register allocation across callee-saved regs. `: double` 128-bit SIMD with 11 `vec_*` builtins (NEON + SSE2). Tonify sweep across every codegen file. |
| **Apr 16** | **Smart dispatch Phase 2** — compiler auto-parallelizes worker-safe loops into `job_parallel_for` with zero programmer intervention. |
| **Apr 16** | **First sound** — `stbaudio` opens CoreAudio device. 440 Hz sine tone plays through the speakers from B++ code. |
| **Apr 17** | **Polyphonic synthesizer** — 300-line `synthkey.bpp` with 4 octaves, 8 voices, 4 waveforms with continuous morph, bitcrush + decimation dirt, WAV recording. A musical instrument written in B++, compiled by B++, producing audio through B++'s own standard library. |
| **Apr 17** | **The book + zero-warning clean compile** — `docs/bpp_manual.md` and `docs/bpp_language_reference.md` merged into `docs/the_b++_programming_language.md` (K&R-style, 16 chapters). `docs/manual/how_to_dev_b++.md` consolidates production discipline + tonify expert mode. Last two W026 false positives resolved (`_stb_gpu_init` + `render_init` were annotated `: gpu` but honestly reach IO on shader-compile error paths — the lattice was right, the annotations weren't). Every game in the repo now compiles with zero warnings. |
| **Apr 18** | **Infra week, day 1 — Rhythm Teacher foundations**. Five new/extended modules land in a single pass, all tested and clean-compiling. **`stbasset`** (new) — handle-based asset manager. 32-bit handles `[generation\|slot]` with O(1) dedup-by-path, stale-handle detection, kind-tagged slots for sprite/sound/music/font. **`stbscene`** (new) — scene manager with deferred switch, register/update/draw/unload lifecycle. **`stbmixer`** (extended) — sample voices + dedicated music slot + three buses (MASTER/MUSIC/SFX) with independent gain; `mixer_play_sample`, `mixer_play_music`, `mixer_music_ms` for note-chart sync. **`stbecs`** (extended) — `ecs_component_new` / `ecs_component_at` helpers for custom parallel components. **`stbsprite`** (extended) — `sprite_draw_frame` for spritesheet row/col sampling, `anim_frame` for time-based frame selection. Platform layer picks up `_stb_gpu_sprite_uv` for UV sub-rectangles. Suite 68 → 74 passing. Bootstrap sha stable. Zero warnings. |
| **Apr 18** | **`bpp_path` + auto-inject promotion + the dog-food loop closed**. New `src/bpp_path.bsm` resolves asset paths relative to `argv[0]` with upward filesystem probe — games run from any directory without breaking relative paths. `bpp_arena` + `bpp_path` promoted to auto-inject (every user program gets them for free). `sound_load_wav` rewritten into a proper chunk scanner: PCM 8/16/24/32, IEEE float 32, mono-to-stereo auto-expand, skips LIST/INFO chunks. `stbsound`/`stbimage`/`stbfont` now emit stderr diagnostics on asset load failure (format code, bit depth, missing chunks, not-a-PNG) — programmer no longer cegos quando um asset some. `games/rhythm/` ships as a real rhythm-genre prototype: beat_map text loader, menu + play (demo/transition/play phases) + results scenes, teacher character drawn in primitives, hit windows ±20ms perfect / ±60ms ok. `games/snake/snake_maestro.bpp` loads `snake_loop.wav` (recorded in mini_synth) as background music and fires an in-code 880 Hz square-wave burst on apple-eat — **a cobra comeu o próprio rabo**: B++ producing content for B++ games, end to end. |
| **Apr 19** | **Tool infrastructure — ModuLab-ready bundle**. Four modules land in one day as pre-work for the ModuLab pixel-editor port: **`stbwindow`** (new, 150 lines) — native `NSSavePanel` / `NSOpenPanel` save/open dialogs + `NSAlert` info/confirm/error dialogs, all blocking `runModal` calls wrapped as `window_*` public API. **`stbinput`** extension — per-frame printable-character ring (64-byte capacity, cleared each frame) populated from `-[NSEvent characters]` and consumed via `input_text_len()` / `input_text_char(i)`. **`src/bpp_json.bsm`** (new, ~800 lines) — full JSON reader + writer with chunk-scanning parser, objects, arrays, strings (with escape sequences + \uXXXX → UTF-8), ints, bools, null, typed accessor convenience (`json_object_get_int/string/bool` with defaults), explicit per-container child-index lists (no contiguous-node assumption). Two internal bugs caught along the way: null-terminator accounting in the string intern (`"name"` lookup failed because next intern overwrote the terminator) and non-contiguous children layout (recursive-descent interleaves descendants between siblings, so containers need explicit child-index arrays). **`stbui`** extensions — `gui_text_input` with click-to-focus + backspace + Enter commit, `gui_grid` cell picker with hover highlight + click dispatch, `gui_palette` swatch selector. New `TextInput` struct with `text_input_new` / `text_input_set` / `text_input_clear` lifecycle. Window positioning bug fixed (games spawned bottom-left; added `-[NSWindow center]` before show). Suite 75 → 78 passing. |
| **Apr 20** | **GPU palette + animation** — `stbpal`: 7 built-in catalogs (MCU-8, PICO-8, GB-4, NES-54…), phase-correct cycling, LUT remap, lerp. Metal shader extended to indexed R8 sprites + 1×256 palette texture — O(1) palette swap (damage flash, day/night) at any sprite count. stbdraw Layer byte-grid ops (`layer_get/set/clear/fill`). stbforge animation runtime. ModuLab 1.0 shipped (pixel editor, 5 phases). Suite 108 passing. |
| **Apr 20-22** | **Waves 18-21 — Portable compiler** — `cg_emit_func`, `cg_emit_stmt`, `cg_emit_node`, and `cg_builtin_dispatch` lifted into `bpp_codegen.bsm` spine. Both chip backends delegate through `cg_prim` dispatch. Adding RISC-V is now a folder task, not a rewrite. |
| **Apr 22** | **Phase D — STB faxina + parser opts** — 13 stb modules dogfooded to `bpp_*` APIs. Three parser passes: strength reduction (`x * 2^k → x << k`), identity peephole (`x + 0 → x`), inline trivials (`buf_new → malloc`). `arr_at` bounds-checked accessor. Tier F roadmap (`docs/tier_f_roadmap.md`) scopes CSE / RA v2 / auto-vec — gated on profile data. Suite 108 → 111 passing. |
| **Apr 23** | **The book** — `docs/manual/how_to_dev_b++.md` Caps 29-47 COMPLETE. All 47 chapters in the unified manual. Roadmap and todo refreshed to post-Phase-D baseline. |
| **Apr 24** | **Tablah benchmark** — Swift hashmap benchmark ported to B++ by an external software engineer. 1M parallel key/value generation (8 workers), 1M hashmap inserts, filter pass (char + value threshold). Two variants: clean public-API (`tablah.bpp`) and hand-optimized with inlined xorshift + unrolled 9-step loop (`tablah_opt.bpp`). Drove two stdlib additions: `print_str` in `bpp_io` and the hash iteration API (`hash_cap` / `hash_slot_live` / `hash_key_at` / `hash_val_at`) in `bpp_hash`. |
| **Apr 26-27** | **Stdlib design faxina** — Major array/string/IO design resolution. `TY_ARR` = only `arr_new()` (dynamic, 16-byte header); `buf_byte`/`buf_word` moved to `bpp_buf` as raw TY_PTR buffers — honest separation confirmed by grep: `arr_new` lives only in compiler src, never in stb/games/tools. `put(x)` smart dispatch added: compiler rewrites `put(x)` → `putstr`/`putnum`/`putfloat` by inferred type — same philosophy as `auto x = 3.14`. Fase 3 array element inference: float writes → `elem TY_FLOAT` → `DSP_GPU`; `E245` conflict error when same array receives mixed types. xfail test convention (`// xfail: EYYY`). 47 manual-loop sites replaced with canonical `str_len`/`str_cpy`/`buf_copy`/`buf_fill` calls across 11 files. Suite 117 passing. |
| **Apr 28** | **Operators + pointer primitives + Tonify v1+v2** — `++`/`--`/`+=`/`-=`/`*=`/`/=`/`%=` desugared in parser (no new AST nodes, inherited by both backends). Ten `peek_q/h/w`/`poke_q/h/w`/`peekfloat`/`pokefloat` pointer-width primitives. Full repo tonify: R0–R20 applied across compiler src, all 22 stb modules, games, tools, Bang 9. `static auto`/`global`/`extrn`/`const` storage classes now correct on every file-scope declaration in the repo. All byte-copy loops replaced with `buf_copy`. All multi-peek LE reads replaced with `peek_h`/`peek_q`.Bang9 fix. Suite 120 passing. |
| **May 4** | **Wolf3D Phase 1 — fps_3d.bpp walks at 60 FPS** — pure CPU raycaster (DDA + per-column textured wall blit), runtime profiler HUD with hotkey toggle, four new stb cartridges (`stbtexture` / `stbraycast` / `stbprofile` + `stbphys` Chapter FPS), nine compiler wins as side-effects (lexer scientific-notation literals, T_BLOCK type-inference recursion fix, W027 FFI float-param diagnostic, minisym PC resolver bound check, µs-precision maestro hybrid sleep + busy-wait, `sin_f`/`cos_f`/`abs_f`/`floor_f` promoted to `bpp_math` under Tonify Rule 20). Suite 132 passing. |
| **May 5** | **V3 — function-pointer type checking** — flow analysis + opt-in `func(...)` annotations + Estrita mode. Compiler proves `fn_ptr(target)` matches the resolved callee's shape; mismatches surface as W028 / E246 at the call site instead of register-bank corruption at runtime. AAPCS64 / SysV separate-bank ABI for the `call(fp, args...)` builtin. 8 of 10 sidequests landed alongside (profile sparkline, per-thread backend, PNG decode, SIGPROF filter, FPS HUD only, palette fills public, Tier 3a HUD wire-up, WAV decode split). Suite 140 passing. |
| **May 7** | **GPU pipeline arc CLOSED — all 7 phases shipped end-of-day**. Phase 4.1.x pixel-perfect (offscreen targets + smart-dispatch `render_clear` + `_gpu_flush_off` race fix + virtual canvas + auto-orchestration). Phase 4.2 (6 games migrated). Phase 5 stbscene (layered + parallax). Phase 6 stbfx (effect chain + 4 typed factories: CRT / scanlines / chromatic / dither). Phase 6.4 (fps_3d_gpu adopts CRT + procedural wall textures, paridade visual com fps_3d CPU). Phase 6.3 scoped zones (`@profile("name") { ... }` parser annotation lowers to runtime aggregate). Sidequests in same arc: shader install pipeline (`stb/shaders/` ships with compiler + 3-tier resolve in `gpu_pipeline_load`), sparkline normalised against fixed budget, env-fix bundle (`bpp_internal` + `bpp_bench` added to install.sh — was breaking external developers; suite cd-pinned to REPO_ROOT; `game_init` no longer auto-calls `render_init` — was destroying CPU-only games' NSImageView; SIGPROF disarmed before `job_shutdown`; A/D strafe inversion in `fps_walk`). Compiler gap fixed: `pre_reg_vars` recurses into `T_BLOCK` in both backends. Tonify Rule 25 (`@profile`). 14 commits. Suite 140 passing. |
| **May 8** | **Phase 6.3 v2 — scoped-cleanup epilogues for `@profile`**. The v1 lowering left zones open on early `return` and `panic`; v2 emits matching `_prof_save_drain` at every function exit + a panic hook drain. Suite 141/0/12 native + 114/0/39 C. |
| **May 9-10** | **fxlab arc CLOSED**. Sessão 1 substrate: JSON-driven effects + `effect_from_json` + `effect_set` + file_watch wiring; 4 effects migrated to manifests. Sessão 2 GUI standalone: ~470 LOC `fxlab_lib.bsm` with preset sidebar + auto-generated sliders per param + save-on-release JSON write-back (pure JSON editor — the running game in another process holds the FxEffect handle and reacts via `file_watch_tick`). Sessão 3: Bang 9 `_panel_fx` between Levels and Code. Drag CRT intensity in Bang 9 → `fps_wolf3d` responds in ~30 ms. Embed contract canonized: lib `_init` does NOT call `init_ui` / `theme_*` (host owns UI subsystem). |
| **May 11** | **Phase annotation collapse — Multics → Unix**. 8 user-facing phases (`@base`, `@solo`, `@time`, `@io`, `@gpu`, `@heap`, `@panic`, `@realtime`) → 1 (`@safe`) + `@profile` + default. 700+ redundant annotation sites stripped from 95 files across compiler + stb + games + tools + tests. Parser strict-mode rejects deprecated keywords with E260 (focused migration hint). 6 commits, bootstrap byte-stable each, suite 148/0/12 + 121/0/39 throughout. Tonify Rule 28 (killer-use-case gate) added as meta-lesson. |
| **May 11** | **Wolf3D Phase 2 Session 0 — entity layer**. `level_editor` gains Tiles ↔ Objects toggle, kind picker, marker overlay. JSON schema v2 (`tiles[][]` + `entities[]`). pathfind level migrated in place. `fps_wolf3d` loads level via JSON loader (replaces hardcoded ASCII maze). `bang9_space_manual.md` canonizes Bang 9 as engine/IDE. `.level.json` double-suffix retired in favor of `.json` (folder context `assets/levels/` disambiguates type). |
| **May 12** | **Excalibur Arc Features 1-3**. Polymorphic numeric literals (kills `feedback_const_float_demotes` bug class — literals carry shape until slot context fixes the type). Cast builtins (`floor_int` / `trunc_int` / `round_int` replace typed-local thunks and ad-hoc `(long long)` casts). Struct newtype (`struct WorldPos as Vec2` with compiler-enforced identity — World vs Grid mixing becomes a compile error). |
| **May 12** | **RTS Stress Arc — 5 sessions, ~1010 LOC**. ECS query + iterator (`ecs_query_each(query, fn_ptr)`). Archetype storage (entities with same component set live in 16 KB contiguous chunks). SIMD batching inside systems (`vec_*` on 4-entity batches). Flow-field pathfinding (`stb/stbflow.bsm`: one compute per goal, all units sample). System scheduler (auto-parallelize independent systems via maestro pool). Parallelism proven at 57%; perf gate cleared by ~30×. Infrastructure arc CLOSED. |
| **May 13** | **WC1 mod — Session 2 + builtin-to-source migration + `->` return-type syntax**. WC1 Session 2: native map format + offline SMS→JSON converter (`tools/wc1_map_convert/`) + `wc1_map_load` reads JSON via `bpp_json` + hot-reload via `file_watch_register`. Forest1 map loads + renders end-to-end. Hot-reload validated: edit `forest1.json` in Bang 9's Levels tab → running `rts.bpp` reflects new tile in ~30 ms. Builtin migration: `arr_new` / `buf_word` / `buf_byte` + `argc_get` / `argv_get` / `envp_get` move from `cg_builtin_dispatch` to plain B++ source. `fn(args) -> ret` return-type syntax (clean split: `:` = storage, `->` = output). |
| **May 14** | **Storage class shipped** (`c1430b9`). `global const SCREEN_W = 320;` (cross-module immutable, .data slot) + `static const _MX_MAX_VOICES = 10;` (module-private immutable). E263 fires on write-to-const; E264 promotes `extrn` with no def to a clearer diagnostic. Spine `glob_slot_init_value` helper; `>> 32` canonical fix for negative-int data-section emit. Suite 167/0/12 + 130/0/45. |
| **May 16** | **Modulab Aseprite pivot CLOSED**. Modulab opens Aseprite JSON+PNG sheets, displays sheet thumbnails + tag list + frame preview (Aseprite-viewer mode). Double-click a frame hands it off to Modulab's main canvas with the full pixel-editing tool surface (brush / fill / line / rect / oval / palette / layers / undo). Cmd+S writes the edited frame back to the source PNG via `pixels_save_png`. `image_path_sibling` promoted to public `stbimage` API. `.atlas.json` double-suffix retired (single `.json` convention now uniform across sprites + levels + effects). |
| **May 17** | **stbui v2 declarative layout + parser fbuf overflow killed**. v2 ships in the same `stbui.bsm` cartridge as the v1 widget family — stack-based `ui_box / ui_end` pairs, four-pass FIT/GROW/FIXED/PERCENT solver, arena memory, hit testing via per-frame click latching. S1 engine + S2 Aseprite viewer + S3 Modulab editor + S4 level_editor migrations all today. Parser fbuf overflow caught alongside: `process_file`'s 256 KB shared stack overflowed when stbui's transitive imports grew past it; fix swaps fbuf-for-content with `malloc(fsize+1)` per file (mirrors gcc / clang / rustc per-translation-unit allocation). Triple-gen byte-stable. Suite 174/0/12 + 138/0/48. Docs reorganised into `docs/manual/` (production refs) + `docs/plans/` (active work). |

B++ went from "parser that parses itself" to "an IDE that authors content for the games it compiles" in two months. The philosophy that emerged along the way — **semantics in the frontend, emission in the backend; progressive disclosure everywhere; every dependency earned its place** — is written into [`docs/manual/how_to_dev_b++.md`](docs/manual/how_to_dev_b++.md), the unified manual that absorbs the previously fragmented programming-language reference, dev guide, and Bang 9 design docs. Adding a new chip, OS, or feature follows the same pattern the existing code does.

The version number is the test count. When the test count reaches the point where a complete indie retro game ships end-to-end in pure B++, that's 1.0.

---

*Designed and built by Daniel Obino. Compiler bootstrapped March 20, 2026. First sound April 16, 2026.*
