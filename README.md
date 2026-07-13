# B++

**The B language, if it had gone to make games instead of C.**

A systems language with the soul of Ken Thompson's B — every value is a word, no headers, no type declarations — but with 64-bit words, named struct fields, and a compiler that emits native **ARM64 and x86-64 binaries directly**: no assembler, no linker, no external tools. Its standard library is a game engine. It has its own debugger, its own IDE, and its own audio stack. All of it, down to the PCM decoder, compiles from pure B++ source.

> **v0.99** — B++ is produced entirely inside its own toolchain, by way of [Bang 9](bang9/), its IDE. It stays 0.99 until a complete indie retro game ships end-to-end in pure B++. Then it's 1.0.

## Back to the future

In 1972 Ken Thompson's B became C, and C went on to conquer systems programming, operating systems, and eventually everything. B++ is the other timeline — the one where B stayed small and word-oriented and went to make games. Sean Barrett's STB joined the lab too, so the standard library *is* the engine. And it just played its first chord.

## Hello, world

```c
main() {
    put("Hello, World\n");
}
```

```bash
bpp hello.bpp -o hello && ./hello
```

A game is not much longer:

```c
import "stbgame.bsm";

main() {
    game_init(320, 180, "My Game", 60);
    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }
        clear(DARKGRAY);
        draw_circle(160, 90, 40, RED);
        draw_end();
    }
}
```

## Quickstart

```bash
# Seed the compiler from its C bootstrap (first time only)
clang bootstrap.c -o bpp

# Build + install the toolchain
sh install.sh

# Run a game
cd games/snake && bpp snake_maestro.bpp -o build/snake && ./build/snake

# Debug any binary — function trace + crash report, no flags
./bug ./build/snake
```

`bootstrap.c` is the compiler emitted as C (~15K lines) — the only seed, and the only C in the project. After that, B++ compiles itself in ~0.3s.

## What's in the box

- **The compiler** — self-hosting: lexer, parser, type inference, optimizer, and two native backends in one ~0.3s pass. Reaches `gcc -O2` parity on hot inner loops via register promotion, compute-in-place, inlining, strength reduction, and SIMD. (How it's wired: [`compiler_internals.md`](docs/manual/compiler_internals.md).)
- **Two CPUs, one source** — ARM64/macOS (Mach-O) and x86-64/Linux (ELF), plus a C-emitter escape hatch for everywhere else.
- **The engine (`stb`)** — rendering, input, the game loop, ECS, physics, pathfinding, tilemaps, sprites, UI, audio — one coherent auto-injected standard library, not a pile of dependencies.
- **`bug`** — a from-scratch debugger with its own ARM64 + x86-64 disassembler ("objdump in bug"), variable-by-name watches, and crash reports mapped to `file:line`.
- **[Bang 9](bang9/)** — the IDE/engine space where B++ authors content for the games it compiles.
- **The audio stack** — PCM decode, mixer, synth, effects (a spring reverb, a playable Fender Bassman) — every byte from the decoder to the bus volume in pure B++.

## Recent

The full milestone-by-milestone record is in [`docs/journal.md`](docs/journal.md). A few recent headlines:

- **Jul 12** — Small-constant multiply strength reduction (`x*3 → x + x<<1`, both backends); the `manylive` kernel 1.35× → 1.18× vs `gcc -O2`.
- **Jul 8–11** — RegAlloc v2 reaches x86-64 parity (liveness-based linear-scan sharing, caller-saved spilling, cross-call float); the inliner arc closes on a register-pressure admission account.
- **Jun 17** — All three codegen kernels — integer serial, integer throughput, float serial — hit `gcc -O2` parity, from 3.71× a week earlier.
- **May 27–28** — Linux self-host: `bpp` compiles itself byte-stable in Docker; the full suite passes on Linux for the first time.

## Docs

Four books ship with the language:

| Book | For |
|---|---|
| [`how_to_dev_b++.md`](docs/manual/how_to_dev_b++.md) | writing B++ programs — the K&R-style tour |
| [`compiler_internals.md`](docs/manual/compiler_internals.md) | how the compiler is wired, end to end |
| [`bootstrap_manual.md`](docs/manual/bootstrap_manual.md) | hacking the compiler itself |
| [`stb++_lib.md`](docs/manual/stb++_lib.md) | the engine library + Bang 9 + tools |

## Status

Honest about the edges: GPU rendering is Metal on macOS today (Vulkan on Linux is deferred); `bootstrap.c` needs a regen; and it's 0.99 on purpose. But what works, works end-to-end — the toolchain builds itself, its own IDE, and playable games.

---

*Designed and built by Daniel Obino. Compiler bootstrapped March 20, 2026. First sound April 16, 2026.*
