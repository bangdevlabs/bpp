# B++ (MAX_V2) 

## B++ 0.99

B++ tools producing content for B++ games, on a stack where every byte — from the PCM decoder to the bus volume to the note scheduler — compiles from pure B++ source.

> **Version 0.99x**: B++ — **produced entirely inside the B++ toolchain by way of [Bang 9](bang9/)**

**Latest achievements (May 27–29):**

- **Linux self-host shipped.** `bpp` compiles itself on Linux, gen2 == gen3 byte-identical in Docker. Two latent x86_64 operand-order bugs in the spine array builtins (`arr_len`, `arr_get`) found and fixed; the full test suite **passes on Linux for the first time (153/0/41)**.
- **`bug --disasm` covers x86-64 ELF + ARM64 Mach-O.** A from-scratch disassembler shipped inside the debugger — "objdump in bug" — necessary because Rosetta blocks gdb/strace/core on a translated process. Two compiler bugs in two days were diagnosed by aiming it at the project's own binaries.
- **Recycling allocator + parser overflow killed.** rts1's frame profile drove `bpp_mem` from mmap-per-malloc to a size-class free-list (**−37 % bootstrap**, 0.35 s → 0.22 s). Turning on full reuse exposed a fixed-buffer overflow class in the parser (per-decl / per-param / per-struct hint tables) that the old page-slack allocator had masked for the project's life — all three migrated to growable primitives; the "fixed buffer + cap + E-net" anti-pattern is gone.
- **`file_stat` builtin + Aseprite-aware hot-reload.** New `sys_stat` lowered through `_core_<os>.bsm` returning a portable `{ is_dir, size, mtime }`. Hot-reload now polls by mtime + watches the Aseprite `meta.image` sibling PNG — editing a level or sprite in **Bang 9 updates the running game live**, `file_read_all` gone from the frame.
- **Rule 42 closed via the disassembler.** A `: float` declarator was floating every neighbour in the same `auto` (pointer locals → doubles → SIGSEGV). Found in fps_wolf3d's bullet path via `scvtf` on a pointer op in `bug --disasm`; fixed by reading the parser's per-name hint array (`n.e[j]`) in a64 + x64 + C-emit. **Rule 43** (`auto (a, b, c): T;` grouped sugar) fell out the same arc once the per-name array was being consulted.
- **x86_64 reached full optimization parity with ARM64.** Inline (single + multi + void), B1 expression-register freelist, B3 local promotion, B4 `: double` SIMD, and smart-dispatch outlining all live on both backends; the only differences are System V's tighter register budget vs AAPCS64.
- **Bang 9 — the IDE — runs on Linux.** Three pre-existing bugs unlocked it: a Mach-O GOT page-straddle, an X11 `ConfigureNotify` resize gap, and a clip-rect / non-negative layout pass in stbui.

**Earlier this month (May 21–25): structural-work compiler.**
**Inline** (S4 cost-model inliner) splices small helpers at every call site; **outline** (smart-dispatch capture rewriter) turns natural `for` loops over `@safe` hosts into `job_parallel_for_data` dispatches; **compose** (outline × autovec) runs SIMD inside the synthesised worker — **6× measured** on `bench_compose.bpp`, ~32× theoretical ceiling. Both native backends collapsed their AST walkers into one shared spine (Wave 20 + 21, **–1300 LOC** of duplicated codegen). A latent silent float→int store bug fixed on both backends; HUD overhead killed (60 fps held flat).

---

## Back to the Future

In 1972, Ken Thompson's B language evolved into C. The rest is history — C conquered systems programming, operating systems, and eventually everything.

But what if a small group of rebels at Bell Labs had taken B in a different direction? What if instead of chasing general-purpose computing, they'd focused on one thing: **making games**?

B++ is that alternate timeline.

A language with the soul of B — every value is a word, no type declarations, no header files — but with 64-bit words, named struct fields, an orthogonal type system, and a compiler that produces native binaries directly. No assembler. No linker. No external tools.

But what if Sean Barrett later joined that group with his STB — all written in B++?

And now we have a standard library that is, itself, a game engine. With audio. And it just played its first chord.

## The Language

The minimum B++ program is three lines. No headers, no imports, no return statement:

## How to Dev B++ — The Programmer's Book

**The K&R-style entry point for writing programs in B++.** The
language. The arsenal. The discipline. Everything you need from "I
have never seen B++" to "I am writing a non-trivial program in it."

This is one of four canonical books that ship with B++:

| Book | Audience | What's inside |
|---|---|---|
| **how_to_dev_b++.md** (this one) | someone writing a B++ program | language tour, stdlib essentials, idioms, compiler flags |
| `tonify_checklist.md` | anyone contributing code | the 20 style rules, applied from the first keystroke |
| `bootstrap_manual.md` | anyone hacking the compiler itself | bootstrap cycle, backend layout, portability tiers, adding builtins |
| `standard_b++_lib.md` | anyone using the game engine library | every `stb*.bsm` module in depth + Bang 9 + bundled tools |

## The Auto-Injected Prelude

Every B++ program starts with modules already in scope. You do not need to `import` them. Their functions are globals. This is the "prelude" — the libraries the compiler auto-injects into every compilation unit.

## Hello World

```c
main() {
    put("Hello, World\n");
}
```

Compile and run:

```bash
bpp hello.bpp -o hello
./hello
# Hello, World
```

That is everything. `put(x)` dispatches at compile time by the type of `x` — string, integer, or float. `main` returns 0 implicitly when no explicit return. No `#include`, no `import`. How that works is Cap 2.

## Variations

Print an integer:

```c
main() {
    put(42);
    putchar('\n');
}
```

Interpolate:

```c
main() {
    auto score;
    score = 150;
    put("score: ");
    put(score);
    putchar('\n');
}
```

B++ has no `printf` format strings. You compose output with `put` calls — the compiler routes each call to `putstr`, `putnum`, or `putfloat` based on the argument type. For newlines, use `putchar('\n')` or `putmsg(s)` (which appends `\n` automatically). Build multi-part strings with `strbuf_*` (Cap 6).

Byte-level output when you need it:

```c
main() {
    putchar('H');
    putchar('i');
    putchar('\n');
}
```

`putchar(c)` writes one byte to stdout. It is the lowest-level output in the language — every other print helper builds on it.

## the three-line program lives in the repo

`examples/hello_world.bpp`. Open Bang 9 (or your editor), point at `examples/`, and this file plus a dozen siblings (`hello_window.bpp`, `snake_cpu.bpp`, `ui_demo.bpp`) are the learning on-ramp. Read the source, run the binary, modify, rebuild.

---

## The stbgame for game devs

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
}
```

Compile and run:

```
bpp game.bpp -o game && ./game
```

No SDL. No raylib. No dependencies. One file in, one native binary out.

## Six Game Prototypes, One Engine

B++ ships with six games in `games/`, all GPU-accelerated on Metal:

| Game | Folder | What it is |
|------|--------|-----------|
| **Snake** | `games/snake/` | Classic snake with ECS particle effects, high-score ranking, arena + pool allocators, **background music + in-code eat SFX**. Eat the apple, grow the tail, don't bite yourself. |
| **Pathfinder** | `games/pathfind/` | Rat vs cat chase. WASD movement, AI pursuit, collision, ECS particles on impact. Loads palette-indexed JSON sprites. Canonical hot-reload smoke (edit level in Bang 9 → game reflects in ~30 ms). |
| **Platformer** | `games/platformer/` | Side-scrolling platformer with Kenney Pixel Platformer assets (CC0). Real tilemap (stbtile), milli-pixel physics (stbphys), gravity, jumping, parallax, scrolling camera, coin collection, spikes, goal flag. Also ships a `platform_noasset.bpp` version with debug rectangles. |
| **Rhythm** | `games/rhythm/` | Rhythm-genre prototype. Menu → demo (auto-play) → transition countdown → play (snare on F or SPACE). Hit-windows: ±20 ms perfect, ±60 ms ok. Uses stbscene / stbasset / stbmixer music+SFX buses / beat_map text parser. |
| **FPS — Wolf3D** | `games/fps/` | Three builds in the same folder: `fps_3d.bpp` is the CPU raycaster baseline (Phase 1), `fps_3d_gpu.bpp` is the GPU port through CRT post-process + procedural wall textures (Phase 4-6 reference), `fps_wolf3d.bpp` is the Phase 2 build with enemies + billboard sprites with per-fragment depth + AI (`stbai` cartridge). WASD + ←/→ to move, P toggles the runtime profiler HUD, ESC quits. |
| **WC1 (RTS, mod)** | `games/rts1/` | Warcraft 1 mod on the original Blizzard asset bundle. Full 14-unit roster, melee + ranged combat, SC1-style damage matrix + caster energy. Hot-reload through Bang 9's Levels + Sprites tabs. Sessions 1-7 CLOSED; S8 (buildings) next. See `games/rts1/wc1_handoff.md`. |

---

## Bang 9 IDE/Engine

**Bang 9 is the integrated game-dev space of the B++ ecosystem.**
Not just an IDE. Not just an engine. One process where source
editing, asset authoring, runtime preview, and live tuning share a
window — because they share the same compiler, the same standard
library, and the same hot-reload backbone.

If you are building a new B++ game and you wonder "where do I edit
this?", the answer is always Bang 9. If the panel doesn't exist
yet, the path to building it is in this document.

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

## Compose-Multiplicatively: ~32× ceiling, 16× measured

A B++ program writes a plain natural-for loop. The compiler fans it
out across cores AND across SIMD lanes — same source, only `@safe`
on the host:

```bpp
void tick(dt: float, payload) @safe {
    auto i;
    for (i = 0; i < _cap; i++) {
        auto e: Entity;
        e = payload + i * sizeof(Entity);
        e.cooldown = e.cooldown - dt;
    }
}
```

**Outlining** wraps the caller-frame captures (`dt`, `_cap`) in a
synthesised struct and dispatches the body through
`job_parallel_for_data` (~6× on 8 cores). **Autovec** sees the
inner store as pattern P1 and emits 4-wide SIMD + scalar tail
(~4× on 4 lanes). Both fire on the same loop — the worker body
itself is SIMD'd:

```
bench_compose.bpp (1M cells × 100 rounds):
  SERIAL  303 ms
  COMPOSE  19 ms     → 16× measured  (bandwidth-bound; compute-
                                       bound workloads reach ~32×)
```

Multi-module game adoption: `wolf_ai_tick_cooldowns` in
`games/fps/ai.bsm` compiles to `bl _job_parallel_for_data` with
`__synth_4` as the worker. The `@safe` annotation is the only
user-side cost — same shape as `pure` in Haskell.

## Inline + Outline: the structural-work compiler

**S4 cost-model inliner** (May 21). Small helpers — single-return
accessors, integer arithmetic, struct field reads, multi-statement
bodies meeting the cost function (caller fan-out × callsite
hotness × const-arg savings) — splice their body at every call
site. Programmers write idiomatic `get_x(p)` accessors and a
7-statement xorshift64 RNG; the compiler emits the same code a
manual inline would. Per Tonify Rule 4, no `@no_inline` exists.

**Smart-dispatch outlining** (May 22). The transformation GCC
OpenMP / LLVM / Cilk / TBB / Burst / Apple GCD all ship (Allen +
Cocke 1972). Natural for-loops over `@safe` hosts get captures
synthesised into a struct, body rewritten as a worker, loop
replaced with `job_parallel_for_data`. The strict-`@safe` gate
(May 23) is the safety scaffolding — a loop only parallelises
when its host carries the annotation, catching dispatch from
signal-handler / interrupt frames before runtime.

## Spine Closeout: One Codegen, Two Chip Backends

The two native backends used to carry parallel AST walkers — every
statement and expression case implemented twice, register
allocation woven through the tree. The May 23 silent float→int
store bug (`fmov` vs `fcvtzs`) was discovered, fixed in `a64`,
then re-fixed in `x64` independently. Wave 20 (May 24) and Wave 21
(May 25) collapsed both walkers into the spine: `cg_emit_stmt` and
`cg_emit_node` in `src/bpp_codegen.bsm` own every AST case,
dispatching atomic ISA ops through a per-chip `ChipPrimitives`
table. ~1300 LOC of dead walker code deleted; adding a third
backend (RISC-V, WASM) is now one file of primitive bodies.

Side wins from the same window: cross-module global float types
recovered at validate time (no more `extrn X: float;` per
consumer); HUD per-frame malloc churn killed (60 fps held flat);
regression test `tests/test_memst_float_to_int.bpp` guards both
backends against the bug class.

**Full optimization parity (May 27).** Collapsing the walkers was
half the story; the parity push then brought every *optimization*
level to x86_64 too — B1 expression-register freelist, B2 / S4 / VI
inline (single-return, multi-statement, and void), B3 local
promotion, B4 `: double` SIMD, and smart-dispatch outlining — and
unified the inline-multi splicer into a single shared spine helper
(`290d77d`). Both backends now run the *identical* optimization set;
the only differences are ISA/ABI register-budget arithmetic (x64's
expression freelist is 1 register vs a64's 7, B3 is 5 vs 6 — System V
leaves x86_64 fewer free registers than AAPCS64). A third backend
inherits the whole optimization pipeline for free.

---

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
- **SIMD — explicit + autovec** — 11 `vec_*` builtins lower to NEON (ARM64) or SSE2 (x86_64) for the explicit path; the autovec pattern matcher recognises `arr[i] = arr[i] OP literal` shapes and emits a 4-wide SIMD loop + scalar tail automatically. `auto v: double` allocates from the SIMD register pool
- **Outline + compose (~32× ceiling, 6× measured)** — natural `for` loops over `@safe`-annotated hosts get their caller-frame captures wrapped in a synthesised struct, the body rewritten into a worker function, and the loop replaced with a `job_parallel_for_data` dispatch (smart-dispatch outlining, May 22). Autovec composes inside the synth body — cores × lanes both fire on the same source loop. The `@safe` annotation is the only user-facing cost (see Compose-Multiplicatively section above)
- **Cost-model inliner** — small `: base` helpers (single-statement trivial up to multi-statement bodies meeting the S4 cost model — caller fan-out × callsite hotness × const-arg savings) splice their body at every call site. Programmers write idiomatic `get_x(p)` accessors and the compiler emits the same code as a manual inline. Per Tonify Rule 4, no `@no_inline` annotation exists — inlining heuristics belong in the compiler
- **FFI** — `import "SDL2" { void InitWindow(...); }` calls any C library

**Compiler:**
- **Self-hosting** — the compiler compiles itself, fixed-point verified
- **Native binaries** — ARM64 Mach-O + x86_64 ELF, built-in SHA-256 codesign, zero external toolchain
- **Cross-compilation** — `bpp --linux64 game.bpp -o game` from macOS
- **Own runtime (libb)** — `bmem` allocator via `sys_mmap`, `brt0` startup code, `bsys` syscall tables. Native binaries have zero libc dependency
- **Module discipline** — `static` private to module, `load` vs `import` separates project from library, circular imports are errors (E222), cross-module duplicates are errors (E221)
- **Effect inference** — internal 4-state classifier (AUTO / BASE / SOLO / SAFE) tracks purity + side-effect surface across the call graph. The user-facing surface is binary (`@safe` annotation either holds or fails W026); internal granularity drives inlining + outlining + autovec eligibility decisions
- **V3 function-pointer type checking** — flow analysis + opt-in `func(...)` annotations + Estrita mode. Compiler proves `fn_ptr(target)` matches the resolved callee's shape; mismatches surface as W028 / E246 at the call site instead of register-bank corruption at runtime
- **Spine + primitive providers** — `cg_emit_module / func / stmt / node` all live in `src/bpp_codegen.bsm`. Each chip backend (`backend/chip/aarch64/`, `backend/chip/x86_64/`) exposes ~80 atomic ISA primitives via a function-pointer table. Adding a new chip (RISC-V, WASM) is one file of primitive bodies, no tree walker (Wave 20+21 closeout, May 24–25). Tonify Rule 41 (Additive Portability) codifies the doctrine: new targets ADD layers, never modify the shared spine
- **Four-backend split** — `chip/` (a64, x64), `os/` (macos, linux), `target/` (aarch64_macos, x86_64_linux), `c/` (universal C transpile escape hatch). Each axis composes independently
- **Diagnostics** — 39 codes registered (24 errors + 15 warnings) with `file:line:caret` locations, multi-error recovery, 20-error cap. Highest IDs: E264 (extrn no-def upgrade), W038 (int literal in `: ptr` parameter)
- **Self-compile speed** — bootstrap ~0.34s, small program ~0.05s, medium ~0.06s (Apple Silicon M-series, no cache)

**Debugger (`bug`):**
- **Three modes** — `bug --dump file.bug` (text dump), `bug --tui ./prog` (live REPL + watch), plain `bug` (GUI inspector with map browser + watch + viz panels)
- **GDB remote protocol** — debugserver (macOS) + gdbserver (Linux), no entitlements, no codesign dance
- **Always-on debug info** — every native compile writes a `.bug` map next to the binary; no `-g` flag, the compiler always emits it
- **Function tracing** — call depth indentation on every entry; `--break <fn>` for selective stops
- **Watch list + expression evaluator** — `--watch "player.vx,score,*head"` evaluates B++ expressions at every stop (struct fields, array indexing, dereference)
- **Type-aware display** — structs render as field trees, arrays as lists, pointers as addresses + target, floats with full precision
- **Runtime visualizers** — `viz_render_graph` / `viz_render_rgba` etc. for in-target panel rendering, GUI mirrors the same panels live
- **Phase 6 runtime symbolication** — `panic("msg")` writes a stack trace to stderr and exits 134; `profile_start(rate_hz, depth)` samples FP chains via SIGPROF + cooperative hooks across worker threads. Three aggregators: `profile_dump` (top-N by innermost frame), `profile_dump_thread` (per-worker breakdown), `profile_print_chains(name)` (full stack chains filtered by innermost — spike-bound diagnosis, May 25). No external `.bug` required at runtime — the binary embeds a minisym table

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

stb is the game engine. It's not a wrapper around SDL or raylib — it **is** the engine, written entirely in B++. 32 cartridges, pulled in opt-in per program — `import "stbgame.bsm"` brings only the floor (window + input + draw + font + color + strbuf); each cartridge above that is an explicit `import` line (Tonify Rule 23 — convenience couplings hide the dependency graph).

**Rendering:**

| Module | What it does |
|--------|-------------|
| `stbdraw` | Software framebuffer rendering — rects, circles, lines, sprites, text, named colors (`rgba()`, BLACK, WHITE, …), 8×8 bitmap font + pure-B++ TrueType reader (cmap, glyf, Bezier, scanline AA) |
| `stbrender` | GPU-accelerated 2D rendering — rects, circles, lines, outlines, text, sprites (Metal) |
| `stbshader` | Custom vertex+fragment pipelines — `gpu_pipeline_load(metal_path, vert, frag)` with cwd → install dir → walk-up resolve, `gpu_target_create / present_target` for off-screen rendering |
| `stbfx` | Post-process effect chain — `fx_register / fx_chain_begin / fx_apply / fx_present` ping-pong infrastructure + 4 typed factories (`effect_crt`, `effect_scanlines`, `effect_chromatic`, `effect_dither`) |
| `stbpal` | GPU palette pipeline — 7 built-in catalogs, phase-correct cycling, LUT remap, lerp |
| `stbtexture` | Procedural textures — `texture_brick / stone / wood / solid` + `_to_buf` headless variants |
| `stbraycast` | DDA + RayHit + per-column projection — content-blind cartridge for any 2.5D raycaster |
| `stbprofile` | Runtime profiler HUD — REC indicator, FPS smoothed, sparkline (fixed budget reference), live top-N tally, GPU timing readout, `@profile` zone aggregates |
| `stbforge` | Editor + playtest infrastructure — `.character.json` schema, mini platformer testbed runtime, debug overlay, deterministic input recorder. Used by ModuLab + level_editor |

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
| `stbgrid` | Generic 2D cell storage — bytes or words per cell, leaf primitive consumed by occupancy / fog-of-war / tile-class / BFS scratch (Tonify Rule 33 Tier 1) |
| `stbecs` | Entity-component system — spawn/kill/recycle, parallel arrays, milli-unit physics, custom components via `ecs_component_new` |
| `stbcharsheet` | Universal character sheet — stat + resource slots (HP / shields / energy / armor / damage / cooldown / etc.) consumed by RTS unit sheets, RPG party members, FPS enemies |
| `stbprojectile` | 2D / 3D projectile motion + lifecycle pool — pos + vel + gravity_z + lifetime + opaque payload. Built on `stbpool`; consumers wrap with targeting + collision + render policy |

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
| `stbpixels` | Layer-1 PNG codec — `pixels_load` / `pixels_save_png` + width/height/depth/channels accessors, DEFLATE + Huffman + every PNG filter type, zero FFI. Split from `stbimage` (2026-05-29) so tools that need only the codec (funnel's `vswap_sprites`, atlas packers) don't drag the whole Image cartridge |
| `stbimage` | Layer-2 Image struct — atlases, sprite/named lookup, mtime-based hot-reload with Aseprite `meta.image` sibling watching + cached sibling list, draw routines. Built on `stbpixels` |
 
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
| macOS ARM64 (Apple Silicon) | **Working** — native Cocoa + Metal GPU, SDL2, raylib. Self-hosting, suite 182/0/12. |
| Linux x86_64 | **Self-hosting** — `bpp` compiles itself on Linux (gen2 == gen3 byte-identical in Docker, 2026-05-27). Full test suite **passes 153/0/41** (2026-05-28). Static + dynamic ELF (FFI shared libs via PLT/GOT). X11 windowing (Bang 9 + games over XQuartz). Full optimization parity with ARM64 (inline / outline / B1 / B3 / B4 SIMD). GPU (Vulkan) and audio (ALSA) door open + Docker-verified at the link layer; driver call bodies stubbed until tested on x86 hardware. |
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
- Self-host on Linux: a Linux-built `bpp` defaults to ELF output (no `--linux64` flag needed). Self-hosting in Docker verified byte-stable across generations.
- Run natively or in Docker — 45KB static binary for the simple path, dynamic ELF when FFI is needed

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

B++ is ~70 days old. The following are the milestones, in order:

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
| **Apr 17** | **Polyphonic synthesizer** — 300-line `synthkey.bpp`, 4 octaves, 8 voices, waveform morph (sine → square), bitcrush + decimation dirt, WAV recording. |
| **Apr 17** | **The book + zero-warning clean compile** — language reference + dev guide merged into `docs/the_b++_programming_language.md`. Last two W026 false positives resolved. Every game compiles warning-free. |
| **Apr 18** | **Rhythm Teacher foundations — five-module ship**. `stbasset` (new, handle-based assets). `stbscene` (new, layered backgrounds + parallax). `stbmixer` extended (sample voices + 3 buses MASTER/MUSIC/SFX). `stbecs` + `stbimage` extended (component helpers + spritesheet frame sampling). Suite 68 → 74. |
| **Apr 18** | **`bpp_path` + auto-inject + the dog-food loop closed**. Assets resolve relative to `argv[0]`. `sound_load_wav` becomes a proper chunk scanner (PCM 8/16/24/32, IEEE f32, mono→stereo). Snake plays a WAV recorded in mini_synth + in-code SFX on apple-eat — **a cobra comeu o próprio rabo**. |
| **Apr 19** | **Tool infrastructure — ModuLab-ready bundle**. `stbwindow` (new, native dialogs). `stbinput` extended (printable-character ring). `src/bpp_json.bsm` (new, ~800 LOC reader + writer). `stbui` extensions (`gui_text_input`, `gui_grid`, `gui_palette`). Suite 75 → 78. |
| **Apr 20** | **GPU palette + animation** — `stbpal` (7 catalogs: MCU-8, PICO-8, GB-4, NES-54…), indexed R8 sprite shader + 1×256 palette texture (O(1) palette swap). `stbforge` animation runtime. ModuLab 1.0 shipped. Suite 108. |
| **Apr 20-22** | **Waves 18-21 — Portable compiler** — `cg_emit_func` / `cg_emit_stmt` / `cg_emit_node` / `cg_builtin_dispatch` lifted into `bpp_codegen.bsm` spine. Adding RISC-V is a folder task, not a rewrite. |
| **Apr 22** | **Phase D — STB faxina + parser opts** — 13 stb modules dogfooded to `bpp_*` APIs. Three parser passes: strength reduction, identity peephole, inline trivials. `arr_at` bounds-checked. Suite 108 → 111. |
| **Apr 23** | **The book** — `docs/manual/how_to_dev_b++.md` Caps 29-47 COMPLETE. All 47 chapters in the unified manual. |
| **Apr 24** | **Tablah benchmark** — external developer ports a Swift hashmap benchmark to B++ (1M key-gen + insert + filter). Drove `print_str` + `hash_*` iteration API additions. |
| **Apr 26-27** | **Stdlib design faxina** — `TY_ARR` reserved for `arr_new()`; raw buffers move to `bpp_buf`. `put(x)` smart dispatch by inferred type. `E245` on mixed-element arrays. xfail test convention (`// xfail: EYYY`). Suite 117. |
| **Apr 28** | **Operators + pointer primitives + Tonify v1+v2** — `++` / `--` / `+=` / `*=` / etc. desugared in parser. Ten `peek_q/h/w` + `peekfloat`/`pokefloat` width primitives. Full repo tonify (R0-R20) across compiler + all 22 stb + games + tools + Bang 9. Suite 120. |
| **May 4** | **Wolf3D Phase 1 — fps_3d.bpp walks at 60 FPS** — pure CPU raycaster (DDA + per-column textured wall blit). Four new stb cartridges (`stbtexture` / `stbraycast` / `stbprofile` + `stbphys` FPS chapter). Suite 132. |
| **May 5** | **V3 — function-pointer type checking** — flow analysis + opt-in `func(...)` annotations. `fn_ptr(target)` mismatches surface as W028 / E246 at the call site instead of register-bank corruption. AAPCS64 / SysV separate-bank ABI for `call(fp, args...)`. Suite 140. |
| **May 7** | **GPU pipeline arc CLOSED — all 7 phases shipped end-of-day**. Phase 4 pixel-perfect + 6 games migrated. Phase 5 `stbscene` (layered + parallax). Phase 6 `stbfx` (CRT / scanlines / chromatic / dither). Phase 6.3 `@profile("name") { ... }` scoped zones. Tonify Rule 25. 14 commits. |
| **May 8** | **Phase 6.3 v2 — scoped-cleanup epilogues for `@profile`** — v1 leaked open zones on early `return` + `panic`; v2 emits `_prof_save_drain` at every exit + a panic hook. Suite 141/0/12 + 114/0/39. |
| **May 9-10** | **fxlab arc CLOSED** — JSON-driven effects (`effect_from_json` + file_watch wiring). Standalone GUI tuner + Bang 9 `_panel_fx` between Levels and Code. Drag CRT intensity in Bang 9 → `fps_wolf3d` responds in ~30 ms. |
| **May 11** | **Phase annotation collapse — Multics → Unix** — 8 user-facing phases → 1 (`@safe`) + `@profile` + default. 700+ sites stripped across 95 files. E260 catches deprecated keywords. Tonify Rule 28 (killer-use-case gate) captures the meta-lesson. |
| **May 11** | **Wolf3D Phase 2 Session 0 — entity layer**. `level_editor` Tiles ↔ Objects toggle + kind picker + marker overlay. JSON schema v2 (`tiles[][]` + `entities[]`). `.level.json` double-suffix retired. |
| **May 12** | **Excalibur Arc Features 1-3** — polymorphic numeric literals (kills `const_float_demotes` bug class). Cast builtins (`floor_int` / `trunc_int` / `round_int`). Struct newtype (`struct WorldPos as Vec2` — World vs Grid mixing becomes a compile error). |
| **May 12** | **RTS Stress Arc — 5 sessions, ~1010 LOC** — ECS query + archetype storage + SIMD batching + flow-field pathfinding (`stbflow`) + system scheduler. Parallelism 57%; perf gate cleared by ~30×. |
| **May 13** | **WC1 Session 2 + builtin-to-source + `->` return-type syntax** — native map format via `tools/wc1_map_convert/`. Forest1 loads + hot-reloads through Bang 9 in ~30 ms. `arr_new` / `buf_word` / `argc_get` move from codegen builtins to plain B++ source. `fn(args) -> ret` syntax. |
| **May 14** | **Storage class shipped** — `global const` (cross-module .data slot) + `static const` (module-private). E263 on write-to-const; E264 upgrades `extrn` no-def diagnostic. Suite 167/0/12 + 130/0/45. |
| **May 16** | **Modulab Aseprite pivot CLOSED** — Modulab opens Aseprite JSON+PNG sheets, displays tag list + frame preview. Double-click hands a frame to Modulab's main canvas; Cmd+S writes back via `pixels_save_png`. `.atlas.json` double-suffix retired. |
| **May 17** | **stbui v2 declarative layout + parser fbuf overflow killed** — stack-based `ui_box / ui_end` pairs, four-pass FIT/GROW/FIXED/PERCENT solver. S1-S4 migrations (engine + Aseprite viewer + Modulab + level_editor). Parser fbuf overflow fixed by per-file `malloc(fsize+1)`. Suite 174/0/12 + 138/0/48. Docs reorganised into `docs/manual/` + `docs/plans/`. |
| **May 18** | **WC1 Sessions 6 + 7 CLOSED — full unit roster, damage matrix, projectile pipeline**. New Tier-1 `stbgrid` (cell storage). New Tier-2 `stbprojectile` (pool of in-flight projectiles, Doom mobj_t pattern). 14 unit kinds wired with SC1-flavored stats. Casters drain energy on cast + regen out of combat. Tonify Rule 33 promoted to explicit 3-tier model. Map render migrated to `tile_bind_image`. |
| **May 19-20** | **WC1 S8.0 pre-flight sidequests CLOSED** — macOS platform layer gains live `_plt_scale` update on resize-poll + `[NSEvent pressedMouseButtons]` sync at end of `_stb_poll_events` (self-heals the LeftMouseUp events Cocoa silently consumes during system gestures, killing the phantom-marquee-after-resize bug). Rts1 canvas bumped 320×240 → 480×270 (16:9, zero letterbox on 1080p/4K). Catapult attack JSON sidecars tuned to 400 ms/frame. Suite 176/0/12 + 140/0/48, bootstrap byte-stable. |
| **May 20** | **WC1 S8 CLOSED — Buildings + construction end-to-end**. 9 building kinds wired per faction (BuildingDef + ECS archetype + production tick + 34 atlas pipeline via new converter `--mode building`). Right-click foundation from peasant selection → walks + chops + completes. HP bars + amber progress bars + selection rings; bottom command card with authentic WC1 portrait_icons (27×19 strip frame-indexed per `war1gus icons.lua`) + icon_border + bottom_panel. Tile 76 (forest tree) marked blocked so peasants route around forests. Engine: `stbecs` ArchetypeRec gains `comp_bitmask` for mask-filtered queries. macOS platform gains `[NSEvent modifierFlags]` sync so Shift/Ctrl/Alt/Cmd actually fire. |
| **May 20** | **S8.5 HUD infra arc CLOSED — stbhud cartridge + bpp_arr prelude primitive**. New Tier-2 `stb/stbhud.bsm` layers GPU HUD widgets (`hud_image` / `hud_bar` / `hud_render`) on top of stbui's layout engine; games inside `game_render_begin/end` get the declarative layout they need without dragging stbimage into every tool that pulls stbui. New `src/bpp_arr.bsm` auto-injected prelude primitive (`arr_struct_new` / `_alloc` / `_at` / `_count` / `_reset` / `_free`) — growable struct-sized array sibling to `bpp_array` (word) for stbui pool growth + future stbecs / compiler refactors. Tonify gains **Rule 35** (games as infra stress test — pressure from real game arcs is the forcing function for engine fixes) + **Rule 36** (primitive promotion when two consumers are PLANNED, not just shipped). |
| **May 20** | **Compiler array migration arc CLOSED — 6 clusters to `arr_struct`**. `ty_vt` / `ty_gl` / `ft` / `diag_file` / `mod_bnd` / `fn_meta` (the codegen vt/cnts/ret/par cluster) migrated to AoS via `arr_struct<T>` + cross-file accessors; 3 hot clusters (`fn_phase` / `fn_effect` / lexer `toks`) audited and **preserved as SoA** with bench evidence (Rule 36b). Synth-fn safety bug caught + fixed by null-guarding `fn_meta_*` accessors — smart-dispatch's worker synthesizer grows `funcs[]` after `run_types` pre-fills `fn_metas`. New harness `tests/bench_compile.sh` (Rule 37 — bench citation) catches a fake "16% bootstrap regression" that turned out to be an old-bpp-erroring-fast-on-new-source measurement bug. Bootstrap 0.47s vs 0.5s target; suite 177/0/12 + 141/0/48 byte-stable. |
| **May 20** | **WC1 S9 CLOSED — Resources end-to-end + polish**. Gold mines pre-placed from the level (4 in forest1.json), right-click → peasant walks in, vanishes into the mine (canonical WC1 "into-the-mine" feedback), gather tick → comes out with the `peasant_with_gold` atlas swap, walks to nearest town hall, deposits → gold counter ticks up. Same loop for forests: right-click a tree (tile 76), peasant chops, tile flips to stump (tile 71) + pathfinder unblocks, peasant walks back with the `peasant_with_wood` carry sprite. Top resource bar HUD (gold + lumber icons + 3 counters) lands as the first piece of the HUD evolution roadmap (S8.6). Food cap recomputes per tick from town halls + farms; `wc1_resources_can_afford` / `_spend` shipped as the S10 production gate. Engine: `stbecs` `world.archetypes` migrated to `arr_struct<ArchetypeRec>` (per-archetype malloc → inline AoS) — rts1 now at 2 archetypes (units + buildings) is the consumer trigger Rule 36b queued for. Bootstrap byte-stable. |
| **May 20** | **Compiler hot-path opt CLOSED — bootstrap 0.51s → 0.37s (~27% faster)**. Four-stage stack of mechanical optimization, each profile-justified via `bench_compile.sh --sample` and shipped behind a sanity gate. S1: `unpack_l` SDIV → LSR (via `: u_word`, was bit-equivalent for packed refs but Phase D.1 skipped `/ 2^k` for signed semantics). S2: hash for `_dsp_find_func_idx` (the linear-scan-over-funcs-per-call-site was the silent quadratic that ate 17% of bootstrap CPU). S3a: CSE in `packed_eq` (hoist `unpack_s` out of byte-compare loop). S3b: lift `unpack_l` to `cg_builtin_dispatch` so call sites get 5-op inline (mirror of `shr` builtin with count hardcoded) — eliminated the function-call frame around the trivial body. Total ~85 LOC across 5 files; both suites unchanged at 177/0/12 + 141/0/48; bootstrap byte-stable (1-cycle oscillation expected after S3b changes self-host inlining). |
| **May 21** | **Compiler hot-path arc FULLY CLOSED — bootstrap 0.51s → 0.30s (~41% cumulative)**. Six more profile-driven stages on top of May 20's S1-S3b. S3e/S3f/S3h lift `arr_get` / `arr_len` / `u32` to `cg_builtin_dispatch` (trivial-body builtins eliminate call frames). S3i: `packed_eq` family compares 8 bytes/iter via `peek_w` chunking. S3j: first chip-level lift — `emit_ror32` chip primitive + `sha_rotr` builtin (cross-chip BINOP asymmetry caught in review, settled on `acc=value, scratch=count` convention). S3k: hash table for `cg_find_fn` (silent quadratic eliminated; asymptotic, not wall-time at this point). S3g reverted (composite `arr_struct_at` body lost 3-7% to dispatch-lane stack juggling) — established the rule of thumb that mechanical inlining via `cg_builtin_dispatch` wins for 1-op trivial bodies and loses for composites. Cost-model inliner S4 opened as the next sidequest. |
| **May 21** | **S4 cost-model inliner SHIPPED end-to-end (Phase B2 + multi-statement)**. ~700 LOC across 5 sessions. Cost function (P1) + per-callsite threshold (P2) + unified classifier (P3a) + multi-statement splice infrastructure (P3b-1) + activation guards (P3b-2) + smart-bind param strategy + B3 ref-count propagation (P3b-3). The classic xorshift64 RNG helper used to cost a call frame per use; post-S4 it splices into a 30-instruction inline sequence. Both single-statement (Phase B2) and multi-statement bodies are eligible; cost function uses caller fan-out + callsite hotness + const-arg savings instead of a hardcoded heuristic. Bootstrap byte-stable through every commit. |
| **May 22** | **Smart-dispatch outlining sidequest CLOSED — 6 phases**. The transformation GCC OpenMP / LLVM / Cilk / Intel TBB / Unity Burst / Apple GCD all ship (Allen + Cocke 1972, "procedure extraction for parallel-for") now lives in `bpp_dispatch.bsm`. P1 free-variable analysis → P2 capture struct synthesis → P3 AST rewriter → P4 Stage 3 publish → P5 `job_parallel_for_data` runtime variant + `_outline_cap_ptr` sidechannel → P6 gate-6 relax + wire-up. Two latent bugs caught mid-arc: reduction-with-captures rejected as out-of-scope; `ast_clone_subst` extended to recurse into T_IF / T_WHILE / T_BLOCK bodies (C-emit `test_mixer_sample` failure surfaced it). Programmer writes natural `for (i = 0; i < n; i++) { work(i, captured) }`; compiler dispatches N-core parallel via the same pattern as hand-written `job_parallel_for`. |
| **May 22** | **Autovec arc CLOSED + compose-multiplicatively measured**. Single-module `bench_compose.bpp` (1M cells × 100 mul-by-literal rounds): SERIAL 303ms → COMPOSE 19ms = **6x measured** (bandwidth-bound at this N; compute-bound workloads approach the ~32x theoretical ceiling). Autovec phases: P1 C-emitter SIMD path + 11 `vec_*_ps` wrapper macros → P2 pattern matcher (`arr[i] = arr[i] OP literal`) → P3 multi-pointer + multi-statement body classifier → P5 4-wide SIMD loop + scalar tail emission → P6 Phase 1 range-synth migration → P6 Phase 2 compose outlining + autovec inside synth body. FPS adoption attempted same day (`wolf_ai_tick_cooldowns`), blocked by a latent multi-module incremental-emit bug. |
| **May 22** | **Load/import distinction arc CLOSED — multi-module game adoption UNBLOCKED**. Seven-commit same-day arc (`0f68dfa`→`a6fa70a`). P1 `diag_file_compile_module[]` mapping → P2 `_process_file_inline_into` flag (load's contents attribute to parent's compile module, not own file) → P3+P4 combined: `lex_module` walks `mod_bnds` + `tok_mod` uses `diag_mod_idx` + orchestrator iterates topo order → P6 defer all per-module emit until AFTER the dispatch pipeline (so synth functions reach the binary instead of orphaning). Three latent bugs uncovered + fixed: BSYS_* cross-target const collision (renamed to `BSYS_MACOS_*` / `BSYS_LINUX_*`); mark_reachable miss-by-471-functions (worked around via lazy-filter suppression); outlining vs signal-handler safety (introduced `@safe` host gate). FPS `wolf_ai_tick_cooldowns` now compiles to `__synth_4` + `bl _job_parallel_for_data`. Bootstrap byte-stable + suite 179/0/12 + 144/0/47. |
| **May 23** | **stbecs gains `ecs_query_each_parallel`** — opt-in worker-pool variant of the query iterator, dispatches the per-archetype row walk across `_job_n_workers` worker threads. RTS adoption: 3 of 14 call sites (per-entity-independent workloads — movement / combat / regen) migrated in follow-up commits with `@safe` annotation on the callbacks. |
| **May 23** | **Strict `@safe` gate doctrine** — both capture-driven outlining AND pure MAP dispatch now require host `@safe`. The pre-strict gate (May 22 `a6fa70a`) only covered capture-driven outlining; the May 23 audit (`48d16e1`) extended to MAP after game adoption observed regressions in stb host functions whose loop shape the matcher accepted but whose function contract said "main-thread only". Five-host audit confirmed none of `profile_start` / `mixer_init` / `init_ui_layout` / `ui_frame_begin` / `init_game` deserves `@safe` — they're all init/per-frame main-thread paths. Tonify Rule 38 updated to capture the doctrine + the cautionary tale. |
| **May 23-24** | **T_MEMST float→int codegen bug FIXED on both backends**. `*(int_slot) = float_expr` silently reinterpreted IEEE 754 bits as integer (`fmov x0, d0` on a64; `movq rax, xmm0` on x64) instead of converting (`fcvtzs` / `cvttsd2si`). Surfaced when snake_maestro particles teleported to addresses outside the framebuffer after the load/import P6 commit made `ecs_physics` inline through this primitive (pre-P6 the call frame masked it). Fixed in `a64_codegen.bsm` (`2b0bbef`) and mirrored in `x64_codegen.bsm` (`e4bdf9a`) with regression test `tests/test_memst_float_to_int.bpp` guarding both backends. `ecs_physics` migrated to `dt: float` seconds API in the same window (no more milliseconds-int / seconds-float impedance mismatch). |
| **May 24** | **Wave 20 closeout — chip statement walkers retired**. The spine's `cg_emit_stmt` now owns the full 11-AST-case statement dispatcher (T_DECL / T_NOP / T_BLOCK / T_BREAK / T_CONTINUE / T_IF / T_WHILE / T_RET / T_ASSIGN / T_MEMST / T_SWITCH / expression-stmt). Chip-side `a64_emit_stmt` + `x64_emit_stmt` walkers + their fat `_full` wrappers + the `emit_stmt_full` primitive slot all deleted as dead code (-639 LOC across a64 + x64). The chip backends now contribute only ISA-atomic primitives to T_ASSIGN / T_MEMST / T_SWITCH; AST-level dispatch lives in one place. |
| **May 25** | **Wave 21 closeout — chip expression walkers retired**. `cg_emit_node` in `src/bpp_codegen.bsm` takes the eight expression cases (T_LIT / T_VAR / T_TERNARY / T_UNARY / T_CALL / T_MEMLD / T_ADDR / T_BINOP) including the T_BINOP left-save dance via `emit_binop_save_left` + `emit_binop_resolve` primitives, the T_VAR stack-struct address-of via `emit_addr_stack_struct`, and the `: u_word` unsigned-dispatch port (UDIV/UMOD/LSR). Three-commit ship per the bisect-friendly plan: pre-migration of chip-internal call sites (37 a64 + 46 x64 sites converted to indirect primitive calls) → wire flip (`p.emit_node = fn_ptr(cg_emit_node)`, validated via differential disasm probe `tests/codegen_parity_probe.bpp` — 0 bytes of divergence) → delete chip walker bodies (~700 LOC). Bootstrap convergent; native 180/0/12 + C-emit 145/0/47 + Linux/Docker headless 5/5. The chip backends are now pure primitive providers — adding a third backend is a single file of primitive bodies. |
| **May 25** | **Cross-module global float type recovery (E233/E240 gap fix)**. When a non-entry module references an entry-module `global X: float;` declaration, the type wasn't visible at infer time (parser processes non-entry modules in topological order BEFORE the entry module declares its globals). Result: the validator either missed E233 silently OR fired E240 ("int passed to float param") when the value WAS actually float. Workaround required `extrn X: float;` per consumer. Fix re-resolves T_VAR arg types at validator time (when `ty_gl[]` is fully populated) with a local-shadow guard via `fn_metas` vt_block to prevent the test_global_kw false positive. Surgical (+9% bootstrap) vs the rejected split-parse-infer alternative (+17%). |
| **May 25** | **HUD performance fix — 60→59 fps flicker eliminated**. Stack-chain profiling on `_mem_alloc_pages` (new diagnostic `profile_print_chains` in `bpp_runtime`) identified three HUD overlay sites mallocing scratch buffers per frame: `render_number` (10-20 calls/frame for the FPS counter + top-N list + zone numbers), `profile_zones_hud_draw` (selection-sort + used-mask scratch), `_profile_dump_inner` (sample aggregation scratch x2). Pre-allocating all three at init dropped `_mem_alloc_pages` samples 22 → 4 (95% reduction); live HUD steady at `ms:17 max:17` with variance zero. Tonify gains **Rule 40** (hot-path malloc is a frame-jitter trap) — pre-allocate scratch buffers at module init, file-scope static, reuse forever. |
| **May 27** | **The games stress-tested the compiler into a recycling allocator and a `file_stat` syscall.** rts1's profile showed ~98% of a frame in mmap, so the heap got a size-class free-list (mmap-per-malloc → pop/push, **−37% bootstrap**). Turning on full block reuse exposed a latent fixed-buffer overflow in the parser — per-declaration / per-parameter / per-struct hint tables (fixed `malloc(64)`/`buf_word`) overran into a neighbouring `Node`'s type byte; the old page-slack allocator had masked it for the life of the project. Fixed by migrating all three to growable `arr` / `arr_struct<FieldRec>`, retiring the "fixed buffer + cap + E-net" anti-pattern. The same profile then flagged per-frame asset re-reads, which drove a new **`sys_stat` / `file_stat`** builtin (`{ is_dir, size, mtime }`, industry-standard subset) so hot-reload polls by mtime instead of reading whole files. Caught a pre-existing gap on the way: Aseprite atlases keep pixels in a `meta.image` sibling PNG the watcher never checked — now it does, with a cached sibling list re-parsed only on manifest change. Editing a level or sprite in **Bang 9 updates the running game live**, `file_read_all` gone from the frame. A `_MEM_DEBUG` poison/UAF allocator mode (Zig-GPA-style) shipped alongside as standing memory-debug infra. |
| **May 27** | **Linux self-host — `bpp` compiles itself on Linux, byte-stable.** The long-standing "x64 large-binary startup crash" turned out to be two x86_64 codegen bugs in the hot-path array builtins: `arr_len` computed `16 - arr` (and `arr_get` computed `3 << i`) because their inlined operand dance assumed ARM64's register convention, and x86_64 pins the LHS to a different register — so the non-commutative subtract/shift came out reversed. Only the compiler itself ran these builtins on a live array early enough to hit it, which is why every small cross-compiled program ran fine. Cracked by dog-fooding the debugger into an objdump: new `bug --bytes` (hexdump a function by name via the `.bug` map) and `bug --disasm` (a from-scratch x86-64 disassembler, calibrated against objdump) made the reversed operands read straight off the screen — necessary because Rosetta blocks gdb/strace/core-dump introspection of a translated x64 process. Fixes rewrite both as commutative `arr + (-16)` / `i * 8`. A Linux-built `bpp` now defaults to ELF output (no `--linux64` needed), and bootstraps itself byte-identically across generations inside Docker. |
| **May 27** | **x86_64 brought level with ARM64 on every optimization.** With Linux unblocked, the long-trailing x64 backend reached full parity: inline (single-return + multi-statement + void splice), a B1 expression-register freelist (r11) for the T_BINOP left operand and T_MEMST store value, and a wider B3 local-promotion budget (5 callee-saved registers, r12/r13 added). Two of these — inline and B1 — had been disabled since April "paired with a regression"; that regression was never them, it was the arr_len/arr_get bugs fixed the same day, so both re-enabled cleanly and self-host byte-stable. The investigation also found two things already paired: outlining (the loop-to-worker transform runs in the shared dispatch pass, not per-chip) and `: double` SIMD (x64 already had SSE codegen). What's left is pure ABI arithmetic, not capability — System V leaves x86_64 fewer free registers than AAPCS64, so the freelist is 1 deep vs 7 and B3 is 5 vs 6. The whole arc was driven by dog-fooding the new `bug --disasm`: every step verified by disassembling the emitted code and re-running the Docker self-host. |
| **May 28** | **Bang 9 — the IDE — runs on Linux.** Tabs switch, panels render, the window resizes, and you can draw in ModuLab over X11/XQuartz. Three fixes unlocked it: a macOS Mach-O GOT page-straddle bug (a GOT spanning a 16KB boundary left spilled slots un-rebased), the X11 platform reporting `window_width/height` of 0 (collapsing every layout) plus a missing `ConfigureNotify` resize handler, and an stbui **graceful clamp** arc — a clip-rect stack + non-negative layout that holds content inside its panel instead of bleeding across tabs. |
| **May 28** | **ELF dynamic linking — the Linux portability engine — and the GPU/audio hardware door.** `write_elf_dyn` emits a real dynamic ELF (PT_INTERP/PT_DYNAMIC, .dynsym/.dynstr/.hash/.rela.plt/.plt/.got.plt, eager BIND_NOW); the **default** `--linux64` path routes FFI externs and runs them against libc in Docker — `strlen("hi")`→2, `abs(-5)`→5, `ldd` resolves libc.so.6 + ld-linux. The soname is resolved from the import-block tag (`elf_lib_soname`), so `import "Vulkan.B"` emits `DT_NEEDED libvulkan.so.1` (Docker-verified) with no engine change — the GPU/audio call bodies stay stubbed until a binary runs on real x86 hardware. Two paired-backend bugs cracked it: x64 conflated FFI externs with cross-module calls (the reloc was deleted before the writer saw it; fixed by mirroring a64's 3-way `T_CALL` + per-module extern refresh), and a stale global `elf_code_off`. Also: `bug` line-map v6 — crash reports, backtraces, `--dump` and `--disasm` now show `file:line`. |
| **May 28** | **First full test suite on Linux — and two real x64 bugs it caught.** The documented Linux gate only ever ran 10 X11 tests; the first whole-suite run (self-host bootstrapped inside the container, gen2==gen3, byte-identical to the macOS cross-compile) surfaced two genuine bugs. (1) The shared `file_write_all`/`bo_save_file` hardcoded macOS open flags (`0x601`) — missing `O_CREAT` on Linux, so **no B++ program could create a new file on Linux**; fixed with a per-OS `_sys_open_create_flags()`. (2) Every **float through memory** (struct fields, `pokefloat`/`peekfloat`) dropped its fraction on x64 (`1.5`→`1.0`) because three codegen spots treated the float slot as int — store, load (no `is_float_type` branch), and the `half float`/f32 path — all fixed by mirroring the a64 primitives. A third fix ported the E264 "undefined extrn" diagnostic from the Mach-O backend to ELF (it was a missing check, not a miscompile). Linux suite 142/26/26 → **153/0/41 — the full test suite passes on Linux for the first time**. Native 182/0/12 + C-emit 147/0/47 untouched. |

B++ went from "parser that parses itself" to "an IDE that authors content for the games it compiles" in two months. The philosophy that emerged along the way — **semantics in the frontend, emission in the backend; progressive disclosure everywhere; every dependency earned its place** — is written into [`docs/manual/how_to_dev_b++.md`](docs/manual/how_to_dev_b++.md), the unified manual that absorbs the previously fragmented programming-language reference, dev guide, and Bang 9 design docs. Adding a new chip, OS, or feature follows the same pattern the existing code does.

The version number is 0.99 because we believe b++ can already produce games with some tweaks. When the test count reaches the point where a complete indie retro game ships end-to-end in pure B++, that's 1.0.

---

*Designed and built by Daniel Obino. Compiler bootstrapped March 20, 2026. First sound April 16, 2026.*
