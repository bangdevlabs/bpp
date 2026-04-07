# The Snake Report

> Every system needs a canary. B++ has a snake.

**First written:** 2026-03-19
**Last updated:** 2026-04-06

## Why Snake

Snake is the simplest game that is still a game. There is a head, a
tail, an apple, four directions, and a death condition. You can hold
the rules in one hand. There is nowhere for cleverness to hide.

That is exactly why B++ was built around it.

When the compiler was still a Python prototype, snake was the test
that something useful could be expressed at all. When self-hosting
landed, snake was the first program through the new pipeline. When
the GPU backend came online, snake was the first game on Metal. When
the X11 platform layer was finished, snake was the first game on
Linux. Every time the language gained a new capability, snake was
how we knew the capability worked end-to-end.

The fleet has three games now — Snake, Pathfinder, and Platformer —
and snake is the one in front. It has been the longest. It is the
smallest. It carries the most history per byte of source.

## What Snake Proved Before B++ Could Compile Itself

The first version, `snake_demo.bpp`, was written on **2026-03-19**,
the day before the bootstrap. At that point B++ was still being
compiled by a Python tool that emitted C, which clang then compiled
against a raylib shim. Three layers of indirection between the source
and the binary.

And yet the source itself was already short. Already readable. The
Python compiler was rough and the C shim was a hack, but the **B++
language** behind both was already producing game code that looked
like game code. No type ceremony, no header hell, no boilerplate. The
move-direction state machine was eight lines. The collision check was
six. The whole game fit in less than two hundred lines including the
HUD.

This was the moment we knew the language was right even though the
compiler wasn't. The proof was on the screen: a snake, eating an
apple, growing a tail, written in code you could read in three
minutes. If a Python prototype could already deliver that, the
self-hosted compiler — when it arrived — would carry the same shape
forward.

The next day, **2026-03-20**, B++ bootstrapped. `snake_v3.bpp` was
the first non-trivial program compiled by `bpp` compiling itself.
The output ran. The compilation pipeline collapsed from three layers
to one. The source did not need to change.

That is what "the language reduces complexity" means in practice:
the same program survives every collapse of its toolchain.

## Snake Firsts

These are the moments where snake was the canary for a new piece of
B++. In every case the next two games in the fleet followed within
days, but snake was always first through the door.

| Date | First | What it proved |
|------|-------|----------------|
| 2026-03-19 | First B++ game (via Python compiler) | The language shape was right before the toolchain was |
| 2026-03-20 | First program compiled by self-hosted bpp | Bootstrap is real, the IR survives the rewrite |
| 2026-03-23 | First zero-dependency native binary | No SDL, no raylib, no libc — direct Mach-O from the compiler |
| 2026-03-24 | First multi-backend game (native + SDL + raylib) | One source compiles to three different runtime stacks via driver imports |
| 2026-03-31 | First game on the GPU | Metal works through `objc_msgSend`, render API is sound |
| 2026-04-02 | First game with ECS particles | Arena + pool + ECS infrastructure pays back in real game code |
| 2026-04-06 | First B++ game on Linux | X11 wire protocol works end-to-end, raw `sys_socket` can drive a window |
| 2026-04-06 | First B++ game in Docker | `--linux64` cross-compile + XQuartz + Ubuntu container all align |

Every other game in `games/` is younger than snake. Pathfinder ported
the GPU sprite path. Platformer ported the tilemap and the milli-pixel
physics. Both inherited snake's loop structure, snake's HUD pattern,
snake's death-and-restart flow. The fleet is shaped like snake.

## What Snake Taught

A few things came out of snake that we did not see coming.

**1. The "alpha bug" was hiding in plain sight.** When snake migrated
from CPU rendering (`draw_*`) to the GPU path (`render_*`), the
particle effects vanished. The cause was a 24-bit bitmask in the ECS
flags field that quietly dropped the alpha byte. Software rendering
ignored alpha, so the bug was invisible for weeks. The Metal fragment
shader's `discard_fragment()` for `alpha < 0.01` exposed it
instantly. The fix was three characters: change `0xFFFFFF00` to a
shift. The lesson: "it worked before" is a lie when you change
backends, because backends quietly assume different invariants.

**2. The simplest game is the cruelest test of caching.** When the
modular `.bo` cache landed, snake was the program that broke first
and broke worst. Three separate cache bugs surfaced from compiling
the same snake.bpp twice in a row: param types not registered, return
types not registered, null terminators missing in the string pool.
None of them showed up under bootstrap (because every bootstrap
recompiles the compiler, which generates a new compiler hash, which
invalidates the cache). It took a small game compiled twice to find
what the entire compiler couldn't.

**3. Snake's death loop is the perfect ECS stress test.** Eat an
apple, spawn N particles, score++. The particle system has to handle
spawn, decay, render, and free, every frame, with the same game
loop running the snake itself. If the ECS leaks, snake leaks visibly.
If the ECS is slow, snake slows visibly. If the ECS gets the alpha
bug, the particles vanish visibly. Snake is the dial that tells you
the game-infra modules are healthy.

**4. Cross-compile is real because snake said so.** The Linux X11
plan was built and tested with five small files in `tests/`, one for
each phase. But none of them counted as "real" until snake itself,
the same snake that had been running on macOS since March, opened a
window inside an Ubuntu container with the host XQuartz drawing the
pixels. That moment closed the loop: the same source, the same engine,
the same gameplay, on a second operating system, through a connection
the compiler authored from scratch.

## Where Snake Stands Today

```bpp
// games/snake/snake_gpu.bpp — the canonical snake of B++ 0.21.
// Compiles to a single Mach-O binary on macOS or a static ELF on Linux.
import "stbgame.bsm";
import "stbrender.bsm";
import "stbecs.bsm";
import "stbarena.bsm";
import "stbpool.bsm";
```

**Native macOS:** `bpp games/snake/snake_gpu.bpp -o snake; ./snake`.
Cocoa window, Metal GPU, ECS particles on apple-eat, animated head
direction, persistent high-score ranking written through `stbfile`,
death and restart with arena reset between runs.

**Native Linux (cross-compiled):**
`bpp --linux64 examples/snake_cpu.bpp -o /tmp/snake_x11`. Static ELF
binary, ~45 KB, runs anywhere x86_64 Linux runs. Uses the X11 wire
protocol for windowed mode and falls back to terminal ANSI rendering
when `DISPLAY` is unset.

**Inside Docker on macOS:**
```bash
docker run --rm \
  --add-host host.docker.internal:host-gateway \
  -e DISPLAY=host.docker.internal:0 \
  -v /tmp:/tmp \
  ubuntu:22.04 \
  /tmp/snake_x11
```
Window opens in XQuartz on the host. Same gameplay as native. The
keyboard works, the close button works, the FocusOut handler clears
stuck keys when you Alt-Tab. This is the same pixels, the same
collision checks, the same particle system that runs on the Mac —
just transported through three thousand lines of platform code that
B++ wrote itself.

## Snake at the Front of the Fleet

```
                         ┌──────┐
                         │ snake│  ← canary, pioneer, standard-bearer
                         └──┬───┘
                            │
              ┌─────────────┴─────────────┐
              │                           │
        ┌─────▼────┐                ┌─────▼─────┐
        │pathfinder│                │ platformer│
        └──────────┘                └───────────┘
```

Pathfinder learned GPU sprite loading from snake's Metal path.
Platformer learned the milli-pixel convention from the snake-shaped
loop pattern. When stbpath landed on 2026-04-06, the first place we
tried it was the cat in pathfinder — but the test that proved the
A* heap was correct ran inside a tiny test program shaped exactly
like snake's main loop.

Three games in the fleet today. Snake is the one that has been at
the front the whole way. Every regression test, every backend port,
every new piece of compiler infrastructure is, in the end, "does
snake still run?" The day snake stops running is the day B++ stops
being a thing.

So far the answer has always been yes.

## What's Next

- **Snake on Vulkan** when ELF dynamic linking lands — same source,
  same game, talking to the Linux GPU instead of the Linux X11
  framebuffer.
- **Snake with audio** when stbaudio lands — apple-eat sound, death
  jingle, restart cue. The smallest game becomes the smallest
  audio test.
- **Snake on Windows** when the Windows backend lands — same source,
  same game, on the third operating system. The fleet sails on.

---

*Snake first ran on 2026-03-19. It has not stopped running.*
