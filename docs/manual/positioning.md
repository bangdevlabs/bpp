# Where B++ stands — a compiler self-assessment (2026-06-17)

An honest read of what B++ has become as a compiler over the last few months,
and how it sits next to the systems languages used for games — C/C++, Jai,
Odin, Zig. Grounded in the measured numbers (`benchmarks.md`), not aspiration.

## What B++ is, in one paragraph

A self-hosted, single-binary compiler with its **own backends** (AArch64
Mach-O + x86-64 ELF, no LLVM, no external assembler/linker) that **compiles
itself in 0.25 s**, plus an integrated game stack — GPU, audio, ECS, structured
concurrency (Maestro), autovectoriser, the `stb*` cartridge library, the Bang 9
editor, and the `bug` debugger. It is a language *and* an engine *and* a
toolchain, written in itself.

## The compiler's evolution (the last ~2 months)

- **Self-hosting + speed.** Compiles its own ~30k-line source in **0.25 s**
  (down from 0.51 s after the hot-path arc). No `.o` cache — every build is
  from source, the Jai/Go "fast enough that you don't cache" model.
- **Codegen quality** went from a back-of-the-envelope ~2–3.7× off `gcc -O2`
  to, today:

  | kernel | ratio to `gcc -O2` |
  |---|---|
  | integer serial (lcg) | **1.02× (parity)** |
  | integer throughput (xform) | **1.10×** |
  | float serial (biquad) | **1.02× (parity)** |
  | register-heavy leaf | **1.17×** |

  It got there by building, in order: a call-aware register freelist; B3 local
  promotion (now widened into caller-saved regs for leaf functions);
  compute-in-place arithmetic; instruction selection (immediate shift, strength
  reduction, integer `madd`/`fmadd`); the induction-variable pointer walk; and
  loop control (compare-and-branch fusion, in-place self-update). On
  data-parallel work the autovectoriser + outliner give **2–4×** (compose 4×,
  SIMD 2–3×, flow-field 4×).
- **Two real backends at parity** — every codegen feature lands on a64 and x64,
  both self-host byte-stable, verified in Docker.

## How it sits next to the peers

| | backend | self-host compile | codegen vs C | engine in the box |
|---|---|---|---|---|
| **C/C++** (gcc/clang) | own, 40 yr | slow (seconds–minutes, LTO/PGO) | the oracle | no |
| **Jai** | LLVM | fast goal, LLVM cost | ≈ C (LLVM) | partial |
| **Odin** | LLVM | fast | ≈ C (LLVM) | no (vendor libs) |
| **Zig** | LLVM + own (WIP) | fast (own backend) | ≈ C (LLVM); own backend maturing | no |
| **B++** | own (no LLVM) | **0.25 s** | **~1.1–1.4×** | **yes** |

The key axis is **backend strategy**. Jai, Odin, and release-mode Zig lean on
**LLVM**, so they inherit C-level codegen "for free" — at the cost of a large
dependency and slower builds. B++, like Zig's in-progress self-hosted backend
(and like QBE / C-- / cranelift in spirit), **hand-writes its backend**: that
buys the 0.25 s self-host and zero dependencies, and costs the ~1.1–1.4× gap
that mature optimizers still hold.

So the honest placement:

- **On raw scalar codegen**, the LLVM-backed languages win on the long tail
  (aggressive auto-vectorisation, whole-program scheduling) — but on the scalar
  kernels b++ is now **at `gcc -O2` parity across the board**: integer serial
  (1.02×), integer throughput (1.10×), and float serial (1.02×). It was 2–3.7×
  a month ago. The remaining edge the LLVM-backed compilers hold is the heavy
  SIMD auto-vectoriser and cross-iteration scheduling, not basic scalar codegen.
- **On compile speed and footprint**, B++ is in the Jai/Zig "fast iteration"
  camp and arguably ahead — a 0.25 s self-host with no LLVM, no system linker.
- **On what ships in the box**, B++ is alone: the others are languages (plus a
  standard library); the renderer, audio mixer, ECS, job system, sprite/tile
  pipeline, and editor are the user's problem. B++ ships all of them.
- **On maturity and ecosystem**, B++ is the youngest by an order of magnitude —
  months versus C/C++'s decades and Jai/Odin/Zig's ~7–8 years and real
  communities. No package ecosystem, one-and-a-bit developers, a small surface.

## What this means for game performance specifically

Game hot paths are usually **memory-, cache-, and algorithm-bound**, not
arithmetic-bound. At ~1.1–1.2× `gcc -O2` on integer kernels and 2–4× from
autovec on data-parallel passes, **B++ codegen is rarely the bottleneck for
actual game logic** — the ECS iteration, the pathfinding, the per-entity
update. The place the gap bites is **DSP/audio** (float-serial dependency
chains), where the 1.44× shows; that is also where the next codegen lever (FP
instruction scheduling) would pay off.

## The honest verdict

B++ is a genuinely fast, self-contained, self-hosted compiler that has reached
**`gcc -O2` parity on integer code with its own from-scratch backend, in
months**, while also shipping a complete game engine and editor.
It does not beat C/C++ or the LLVM-backed languages on raw codegen, and it
does not pretend to — the remaining gap (float scheduling, more aggressive
auto-vectorisation) is named and measurable. What it offers instead is a point
in the design space none of the peers occupy: **own backend + sub-second
self-host + integrated engine + a typeless-B-to-smart-hints language**, with
the codegen gap small enough that it is, for game work, mostly a non-issue —
and shrinking. The story of the last two months is exactly that gap closing.
