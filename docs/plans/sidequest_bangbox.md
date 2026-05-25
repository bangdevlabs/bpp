# Sidequest — BangBox (bare-metal ARM64 demo cartridge)

**Status:** PROPOSED 2026-05-25, design-locked pending Q1-Q5
sign-off. Execution starts post-Wave-21 to avoid bandwidth
competition with the active compiler arc.

**Trigger:** Discussion sparked by [pac-ac/osakaOS](https://github.com/pac-ac/osakaOS)
deep-dive. The OS itself is not the goal — it is the most
brutal stress test we can build to validate B++ for its real
mission: games, tools, and graphic+audio computing. Every line
of code in BangBox must teach B++ something useful for the
broader project.

## The reframe — "OS not as goal, OS as forcing function"

Original framing considered: "Build a hobby OS in B++".

Refined framing (locked 2026-05-25): "Build the smallest
bare-metal cartridge that forces every B++ subsystem to prove
it works without OS conveniences — and ship as a vertically
integrated demo when done."

Outcomes measured at the project level, not at the OS level:

| Outcome | Measured how |
|---|---|
| stbdraw battle-tested at the metal | Pixel Lab runs at framebuffer rate without dropped frames |
| stbmixer / maestro interrupt-safety validated | Audio Lab plays without clicks across hours of runtime |
| stbimage asset pipeline portable | Asset Lab loads PNG + applies palette + blits cleanly |
| `@safe` doctrine pushed to limit | Audio callback @ 11025 Hz never violates W026 in production |
| Compiler `--freestanding` mode proven | Compiles without auto-injected hosted modules |
| `_stb_platform_baremetal` layer mature | New layer added under Rule 41 without touching stb/ |
| Tool stack portable beyond macOS/Linux | Modulab + Mini Synth + Bang 9 LITE run from same kernel |

Outcomes measured at the BangBox level (secondary):

| Outcome | Visible how |
|---|---|
| Boots in QEMU virt on Apple Silicon | `qemu-system-aarch64 -machine virt -kernel bangbox.bin` |
| Menu of Labs + Tools appears | Framebuffer-rendered text menu, arrow keys + enter |
| Snake runs as one of the apps | Standard B++ snake with stbinput + stbdraw |

---

## Decision lock — Path B (ARM64 QEMU virt)

After evaluating three paths (see "Path evaluation" section
below), the decision is **Path B: ARM64 QEMU virt** because:

1. **Zero new compiler work.** Existing `a64` backend services
   the project as-is. Compiler arc (Wave 21, hot-path opt)
   stays focused.
2. **Native KVM execution on Apple Silicon.** QEMU virt runs
   ARM64 guests natively on M-series hosts (`-accel hvf` or
   `-cpu host`). Iteration cycle = milliseconds.
3. **ARM64 bare-metal is technically simpler than x86.** No
   real-mode dance, GIC is more orderly than IDT, no GDT,
   no segment selectors.
4. **Compatible with Rule 41 additive portability.** A new OS
   layer (`_stb_platform_baremetal.bsm`) is the entire BangBox
   substrate from the compiler's view; stb stays untouched,
   chip stays untouched, programs (Snake, Modulab, Synth,
   Bang 9 LITE) stay untouched.

Rejected paths (recorded for posterity):

- **Path A: x86 32-bit + new `i386` backend.** OsakaOS-direct,
  but requires 4-6 weeks of new compiler work before any OS
  code lands. QEMU TCG is software-emulation slow on Apple
  Silicon. Adds permanent maintenance cost (third backend).

- **Path C: x86_64 long mode bare-metal.** Reuses existing
  backend but adds the real-mode → protected → long-mode
  boot transition complexity. Same TCG slowness as Path A.
  Worst-of-both-worlds.

### Apple Silicon bare-metal — explicitly NOT a target

Earlier discussion considered bare-metal on Apple Silicon
hardware directly (no QEMU). Current Asahi Linux reporting
(2026):

- M1/M2/M3: Asahi works via m1n1 + significant reverse
  engineering. ~3-6 months of extra work to bring B++ along.
- M4 and later: progress stalled per AppleInsider /
  Asahi blog. iBoot's raw boot object mechanism (the
  semi-official third-party boot path) is reportedly
  disabled on M4 and later.

Conclusion: Apple Silicon real-metal is a future arc gated on
Asahi progress, not part of BangBox.

---

## Architecture — Rule 41 in action

BangBox is a textbook application of Rule 41 (Additive
Portability). The diagram shows only the layer being ADDED;
everything above and to the sides is reused untouched.

```
┌──────────────────────────────────────────────────────────────┐
│ LAYER 5: PROGRAMS (target-agnostic, REUSED)                  │
│   snake.bpp, modulab.bpp, mini_synth.bpp, bang9.bpp          │
└──────────────────────────────────────────────────────────────┘
                            │
┌──────────────────────────────────────────────────────────────┐
│ LAYER 4: STB (target-agnostic, REUSED)                       │
│   stbgame, stbwindow, stbui, stbdraw, stbinput,              │
│   stbmixer, stbimage, stbecs, stbpath, stbpool, ...          │
└──────────────────────────────────────────────────────────────┘
                            │
                            ▼  resolved by --target=baremetal
┌──────────────────────────────────────────────────────────────┐
│ LAYER 3: OS PLATFORM (NEW — this is BangBox's contribution)  │
│   src/backend/os/baremetal/                                   │
│     _stb_platform_baremetal.bsm    (~1500 LOC)               │
│     _core_baremetal.bsm            (~300 LOC)                │
│     _bsys_baremetal.bsm            (~50 LOC)                 │
│     bug_observe_baremetal.bsm      (stubs, ~100 LOC)         │
└──────────────────────────────────────────────────────────────┘
                            │
┌──────────────────────────────────────────────────────────────┐
│ LAYER 2: TARGET FORMAT (NEW)                                  │
│   src/backend/target/aarch64_baremetal/                       │
│     baremetal_writer.bsm  — emit raw ELF kernel  (~400 LOC)  │
└──────────────────────────────────────────────────────────────┘
                            │
┌──────────────────────────────────────────────────────────────┐
│ LAYER 1: CHIP (REUSED — existing a64 backend)                │
│   src/backend/chip/aarch64/                                  │
└──────────────────────────────────────────────────────────────┘
```

**Rule 41 audit for the BangBox PR (each phase):**

1. Did we modify any `stb/` file?
   → No. The stb modules are target-agnostic and stay byte-identical.
   → If a stb module turns out to have hidden hosted assumptions
     (e.g., bpp_mem assuming mmap is available), the fix is to
     widen the abstraction in stb itself, not patch it per
     target.

2. Did we touch macOS or Linux backend directories?
   → No. macOS and Linux platform layers are siblings to the
     new baremetal layer.

3. Can a hello-world program be byte-identical at source level?
   → Yes. `import "stbwindow.bsm"; main() { ... }` must compile
     unchanged for `--target=macos`, `--target=linux64`, and
     `--target=baremetal`.

If any phase below violates one of these tests, stop and revisit
the abstraction.

---

## Compiler additions required

These are infra-level additions to the compiler that BangBox
needs. Each is small and reusable for future bare-metal targets
(future RISC-V baremetal, future embedded targets).

| Addition | What it does | LOC |
|---|---|---|
| `--target=baremetal` flag | Selects bare-metal OS layer + target format | ~30 in `bpp_args.bsm` |
| `--freestanding` flag | Suppresses auto-injection of hosted modules (bpp_io / bpp_file / etc. become opt-in) | ~150 in `bpp_import.bsm` |
| Inline asm builtin (`__asm("instr")` OR per-instruction `cli()`, `sti()`, `wfi()`, `dsb()`, `dmb()`) | Lets B++ emit ARM64 system instructions without full asm files | ~200 in `bpp_codegen.bsm` + chip primitives |
| `@interrupt` annotation | Marks functions as interrupt handlers (different prologue, no frame pointer) | ~100 in `bpp_dispatch.bsm` + a64 codegen |

**Compiler additions subtotal: ~480 LOC.**

The `--target=baremetal` and `--freestanding` flags are forever
useful for future targets — they unlock not just BangBox but
any embedded path (RPi pico, microcontrollers, etc.).

---

## Phase plan

Each phase is one commit, bootstrap byte-stable, suite green
(Tonify Rule 37). Risk peaks at P3 (boot stub) and P5 (audio
driver) — both are real hardware-talking code.

| Phase | What | LOC | Time | Risk |
|---|---|---|---|---|
| P0 | Design doc lockdown + Q1-Q5 decisions | — | 2 days | low |
| P1 | Compiler: `--target=baremetal` + `--freestanding` flags. Suppress auto-inject of bpp_io/bpp_file/bpp_args. Add new target format dispatch in `bpp.bpp` orchestrator. | ~200 | 5 days | low |
| P2 | Compiler: inline asm builtin (initial set: `wfi`, `dsb`, `dmb`, `isb`, `mrs`, `msr`). Per-instruction builtins in `cg_builtin_dispatch`. | ~200 | 5 days | low |
| P3 | Boot stub: `_start.s` (entry at 0x40080000), EL2 → EL1 drop if needed, vector table, branch to B++ `main()`. ARM64 conventions only. | ~150 | 1 week | **high** |
| P4 | `_stb_platform_baremetal.bsm` skeleton: UART (PL011 at 0x9000000) for early printf. Validates B++ runs on bare-metal at all. | ~250 | 5 days | medium |
| P5 | Framebuffer driver: virtio-gpu basic (no acceleration) OR PL110 simple framebuffer @ 320×240×8-bit indexed palette. Pixel push primitives. | ~350 | 1 week | medium |
| P6 | Keyboard + mouse via virtio-input. Event queue, modifier tracking, mouse hover/click dispatch. | ~250 | 1 week | medium |
| P7 | **Audio driver** (PL041 OR virtio-sound). Sample buffer feeding via timer interrupt. **@safe doctrine validated here** — audio callback must satisfy W026. | ~300 | 1.5 weeks | **high** |
| P8 | `_core_baremetal.bsm`: page allocator over fixed RAM region (no MMU initially), timer interrupt, hooks for bpp_arena to allocate from. | ~300 | 1 week | medium |
| P9 | `_bsys_baremetal.bsm`: stub syscalls (no real OS calls; bare-metal short-circuits). | ~80 | 2 days | low |
| P10 | Target format: `aarch64_baremetal/baremetal_writer.bsm`. Emit raw ELF kernel that QEMU `-kernel` accepts. Linker section layout. | ~400 | 1 week | medium |
| P11 | stbui v2 freestanding port. Existing stbui depends on hosted features; port the relevant subset (panels, buttons, text input) to baremetal. | ~500 | 1.5 weeks | medium |
| P12 | RAM disk infrastructure: baked-in assets at boot (PNG sprites, fonts, demo .bang scenes). Asset lookup by name. | ~150 | 3 days | low |
| P13 | **Optional: virtio-blk driver** for actual persistence. Simple block format (custom or FAT12). Lets edited assets survive reboot. | ~250 | 1 week | medium |
| P14 | **Pixel Lab** — stress test for stbdraw. Demonstrates clear, rect, line, circle, sprite blit, text rendering. FPS counter visible. | ~400 | 1 week | low |
| P15 | **Audio Lab** — stress test for stbmixer / maestro. Waveform generators, oscilloscope render, multi-voice mixing. | ~350 | 1 week | low |
| P16 | **Asset Lab** — stress test for stbimage. PNG load, palette cycling, sprite animation. | ~400 | 1.5 weeks | low |
| P17 | **Snake game** — standard game playable inside BangBox. | ~500 | 1 week | low |
| P18 | **Mini Modulab** — sprite editor. Pixel grid edit + Aseprite JSON parse + save to RAM disk. | ~800 | 2 weeks | medium |
| P19 | **Mini Synth** — audio tool. Knob widgets + waveform display + audio output. | ~600 | 1.5 weeks | medium |
| P20 | **Bang 9 LITE** — read+edit code, save to RAM disk. Run/Debug/HotReload tabs disabled with stub messages explaining "feature requires hosted environment". | ~250 | 1 week | medium |
| P21 | Studio launcher — menu showing Labs + Tools, navigation between them. | ~200 | 3 days | low |
| P22 | Polish + bench gates + docs + demo video | ~150 | 3 days | low |

**Total LOC: ~6630**

**Total time: ~16-18 weeks (~4-4.5 months) at focused pace**

The ~6630 LOC breakdown by Rule 41 layer:

| Layer addition | LOC |
|---|---|
| Compiler additions (Rule 41 doesn't bind, but bounded growth) | ~480 |
| OS platform layer (new under Rule 41) | ~1880 |
| Target binary format (new under Rule 41) | ~400 |
| stbui freestanding port (stb-internal, no Rule 41 violation since adding API surface) | ~500 |
| RAM disk + virtio-blk infra | ~400 |
| **Programs (no Rule 41 binding — these are user code)** | ~3000 |
| **Compiler + platform infra subtotal (Rule 41-relevant)** | **~3260** |

The ~3260 LOC of platform infra unlocks not just BangBox but
any future bare-metal target (RPi, embedded, custom hardware).

---

## What BangBox does NOT do (out of scope)

These are deferred explicitly to avoid scope creep:

- **GPU acceleration** — Vulkan port is a separate arc, see
  "GPU deferred" section below. BangBox is CPU-only.
- **Multitasking** — single-task only. The kernel hosts one
  app at a time. Switching apps = return to launcher.
- **Network stack** — out of scope completely.
- **Filesystem** — RAM disk only. Optional virtio-blk
  persistence in P13.
- **bpp self-host inside the OS** — Bang 9 LITE's Run/Debug
  tabs are stubbed. Self-host compiler in 16MB kernel space
  is a 3+ month separate arc.
- **Real PC speaker timing** — audio is sample-buffer based,
  not hardware-cycle based.
- **Asahi-compatible Apple Silicon real-metal** — gated on
  Asahi progress, separate arc.

---

## GPU deferred — Vulkan path

GPU acceleration in BangBox requires a Vulkan backend for
`stbrender` plus a virtio-gpu driver in B++. This is a
multi-month project on its own.

Architectural decision (per discussion 2026-05-25): **do Linux
Vulkan FIRST as a separate arc, then bare-metal Vulkan reuses
~85% of the work.**

```
[Now]    BangBox CPU-only — this sidequest    ~16-18 weeks
          B++ benefit: bare-metal portability validated,
          stb tool stack proven cross-platform.

[Then]   Linux Vulkan port for stbrender      ~3-4 months
          Separate arc. B++ benefit: GPU games + Bang 9 hot
          reload work on Linux (any ISA).

[Later]  Bare-metal Vulkan (BangBox upgrade)   ~2 months
          Reuses ~85% of Linux Vulkan code. B++ benefit:
          GPU games + Bang 9 hot reload work in BangBox.
```

Bang 9 LITE in the current BangBox MVP has stubbed Run/Debug
tabs and **no GPU hot reload** — same limitation as Linux today.
This is a known and accepted boundary.

---

## Open design questions (resolve before P0 closes)

### Q1 — Resolution / palette

| Option | Look | Effort |
|---|---|---|
| **320×240×8-bit indexed** (recommended) | Ultra-retro, chunky pixels | baseline |
| 640×480×8-bit | Medium retro | +50 LOC blit |
| 800×600×24-bit RGB | Modern | +100 LOC, loses retro charm |

Recommend: **320×240×8-bit indexed**.

### Q2 — Audio driver choice

| Option | Pro | Con |
|---|---|---|
| **PL041 (PrimeCell)** | Standard ARM audio, well-documented | ARM-specific |
| virtio-sound | Cross-platform virtio standard | Less mature in QEMU virt |

Recommend: **PL041 first, virtio-sound as fallback if PL041 driver work explodes**.

### Q3 — Persistence

| Option | Pro | Con |
|---|---|---|
| RAM disk only | Trivial, no driver | No persistence across reboots |
| **RAM disk + virtio-blk** | Real persistence for edits | +250 LOC driver, ~1 week extra |

Recommend: **start with RAM disk; add virtio-blk in P13 if MVP scope allows**.

### Q4 — Mouse support

| Option | Pro | Con |
|---|---|---|
| No mouse, keyboard-only | Simpler input layer | Modulab/Paint need mouse |
| **virtio-input mouse** | Tool ecosystem works | +100 LOC driver |

Recommend: **virtio-input mouse**.

### Q5 — Boot intro screen

| Option | Aesthetic |
|---|---|
| Boot direct to menu | Minimal |
| **Brief "BangBox v0.1" logo + loading bar** | Adds personality without faking GRUB |
| Fake GRUB menu | OS-feel maximized, but disingenuous |

Recommend: **brief logo + loading bar (~80 LOC, 2 days)**.

---

## Acceptance gates (per Tonify Rule 37, per phase)

```
1. Bootstrap byte-stable per phase: g1 → g2 → g3, cmp g2 g3
   (chip codegen changes may cause 1-cycle oscillation;
   g2 == g3 mandatory).

2. Native suite (macOS host): 180+/0/12 — no regression.

3. C-emit suite: 145+/0/47 — no regression.

4. BangBox bench: phase-specific
   - P14 (Pixel Lab): 60 fps minimum at 320×240
   - P15 (Audio Lab): zero dropouts over 10 minutes
   - P16 (Asset Lab): PNG load < 100 ms for 64×64 sprite
   - P22 (final): BangBox kernel size ≤ 256 KB

5. Rule 41 audit:
   - No modifications to `stb/` (verify with git diff)
   - No modifications to `src/backend/os/macos/` or
     `src/backend/os/linux/` (siblings, never touch)
   - Programs source-identical between targets (verify by
     diffing snake.bpp source used for macOS build vs
     baremetal build — must be byte-identical)
```

---

## Bench / measurement plan

BangBox lives or dies on whether it teaches B++ things. Each
Lab has a primary metric:

| Lab / Tool | Primary metric | Secondary metrics |
|---|---|---|
| Pixel Lab | Sustained FPS at 320×240 | Cache misses per frame, frame-time histogram |
| Audio Lab | Audio dropout count over 1 hour | Maestro callback latency p95, p99 |
| Asset Lab | PNG decode time per asset | RAM peak during decode |
| Mini Modulab | Time to save edited sprite (RAM disk vs virtio-blk) | Memory leaks across N edit cycles |
| Mini Synth | Audio callback consistency over knob drag | UI input lag |
| Bang 9 LITE | Save → re-open round-trip integrity | Editor input lag |
| Snake | Game loop time per frame | Memory leak (must be zero over 30 min) |

All metrics surface in a final **BangBox bench report** that
becomes a permanent regression target — future BangBox commits
must not degrade these numbers.

---

## Cross-references

- `docs/manual/tonify_checklist.md` Rule 41 — the Additive
  Portability doctrine that BangBox is the first major
  application of.
- `docs/manual/tonify_checklist.md` Rule 35 — "Games as infra
  stress test" — BangBox is the same doctrine applied at OS
  scale.
- `docs/manual/bootstrap_manual.md` "Architecture — The
  Six-Layer Cake" — layering context.
- `docs/plans/bangscript_plan.md` — bangscript runtime
  (separate arc; BangBox's RAM disk could host `.bang` scenes
  for a demo).
- [pac-ac/osakaOS](https://github.com/pac-ac/osakaOS) — the
  reference inspiration for the OS-as-stress-test framing
  (AyumuScript design also informed `bangscript_plan.md`).
- `docs/plans/todo.md` — Linux Vulkan port (P0/P4) is the
  prerequisite arc for BangBox GPU upgrade.

---

## Path evaluation (recorded for posterity)

Three paths were considered before locking Path B. Recorded
here for any future contributor revisiting the decision:

### Path A — x86 32-bit + new `i386` backend

- OsakaOS-direct reference
- 4-6 weeks new compiler work
- QEMU TCG slow on Apple Silicon
- Adds permanent third backend maintenance
- Total: ~7-9 weeks just for the MVP
- **Rejected:** Compiler work blocks OS work; emulation
  iteration cycle ~10x slower.

### Path B — ARM64 QEMU virt (CHOSEN)

- Reuses existing a64 backend
- QEMU virt runs native via HVF/KVM on Apple Silicon
- ARM64 bare-metal technically simpler than x86
- Total: ~16-18 weeks for full Studio
- **Chosen.**

### Path C — x86_64 long mode bare-metal

- Reuses existing x86_64 backend
- But adds complex real-mode → protected → long mode boot
- Same TCG slowness as Path A
- Less reference material (most hobby OS doc is 32-bit)
- **Rejected:** worst-of-both-worlds.

---

## Pre-flight checklist (before P0 closes)

- [ ] User signs off on Q1-Q5 design decisions above.
- [ ] Wave 21 (spine `cg_emit_node` takeover) is shipped or
      explicitly paused — BangBox should not compete for
      compiler-arc bandwidth.
- [ ] FPS visual verification (the open task from the
      2026-05-23 dispatch regression) is confirmed by user.
- [ ] Tonify Rule 41 is committed in `tonify_checklist.md`.
- [ ] This sidequest doc is committed and reviewed.

Once these are checked, P1 (compiler flags) can start.

---

## Success criteria — what does "BangBox done" mean

When BangBox ships, the following statements MUST be defensible:

1. **"B++ runs on bare metal."** Demonstrated by `qemu-system-aarch64
   -machine virt -kernel bangbox.bin` booting to the Lab menu.

2. **"B++ tool stack is target-portable."** Demonstrated by
   Modulab opening a sprite, editing it, saving it, all inside
   the bare-metal kernel.

3. **"B++ audio stack is metal-up correct."** Demonstrated by
   Audio Lab playing for an hour without clicks, with
   maestro's @safe gating verified.

4. **"Rule 41 holds in practice."** Demonstrated by `git diff`
   showing only additions under `src/backend/os/baremetal/` and
   `src/backend/target/aarch64_baremetal/`, plus the compiler
   flag additions in `bpp_args.bsm` and `bpp_import.bsm`. Zero
   modifications to existing target backends. Zero
   modifications to `stb/`.

5. **"A snake game compiles identically for macOS and
   bare-metal."** Demonstrated by byte-comparing the source
   of `games/snake/snake_maestro.bpp` used for both builds —
   they MUST be identical.

If any of these five statements becomes hand-wavy or
qualified, BangBox is not done. The bar is concrete.
