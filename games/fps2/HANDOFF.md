# fps2 — a Quake-class true-3D engine in B++ (handoff)

**What this is.** The next rung of the FPS arc: from the Wolfenstein-class 2.5D
raycaster (`examples/fps_3d.bpp` + its GPU sibling) up to a **true-3D,
BSP-based engine in the spirit of Quake (id Software, 1996)**. This file records
the feasibility verdict and the two decisions taken before the port plan is
written, so the plan starts from settled ground.

Status: **pre-plan.** No engine code yet. Next deliverable is the full port plan
(see "Next" below).

---

## Feasibility verdict

**As a language, B++ can do it.** Quake was written in C; B++ is a C cousin and
expresses everything the engine needs — floats (f16/f32/f64), SIMD/autovec,
pointers, structs, manual memory, FFI, GPU access. Nothing in Quake's
architecture requires a language feature B++ lacks. The work is **effort + new
stb cartridges**, not language capability.

**The enabler.** Quake's *engine* source is open (id GPL'd it in 1999) and the
software renderer is documented in depth (Abrash, *Graphics Programming Black
Book*). So this is the same **enumerate the source → map to the target → port
point by point** funnel used for the Stratagus AI in rts2 and the Bell Labs
malloc. The spec exists and is studyable.

**Source location.** The GPL engine is cloned to **`/Users/Codes/Quake`** (a
sibling dir, the same convention as `/Users/Codes/Warcraft/stratagus`). Layout:
- `WinQuake/` — the **software renderer** (the port's main target). Key files:
  `mathlib.c` (3D math → `stbmath3d`); `model.c` (BSP loader) + `world.c`
  (collision/trace → `stbbsp` + `stbphys`); `r_*.c` + `d_*.c` (the software
  rasterizer); `r_alias.c`/`r_sprite.c` (MDL models); `snd_*.c` (sound);
  `vid_*.c` (per-OS video output — replaced by `stbwindow`/GPU).
- `WinQuake/gl_*.c` — **GLQuake**, the reference for the GPU path.
- `qw-qc/` — the **QuakeC** game logic (reimplement directly in B++).
- `QW/` — QuakeWorld (netcode); low priority for single-player.

**De-risking finding.** The `.s` files are hand-written x86 asm that is *optional
acceleration* — the readme states the engine builds "with only C code" behind a
`#define` (the software path loses ~half its speed; GL barely changes). So
**there is no assembly to port** — we target the C, and the `.s` files are
reference-only for how id optimized the hot loops. The earlier "inline asm" gap
is moot.

Assets (maps/textures/models/sounds) are NOT in this repo (they are id's
commercial property); they come later, ripped from an owned copy and converted
through the funnel into `games/fps2/`.

**Legal.** The *engine code* is GPL — study and port freely, with license
awareness. The *assets* (maps, textures, models, sounds) remain id's property;
owning the game (e.g. GOG) covers personal/learning use — do not redistribute.
Ripping owned assets through the funnel into Bang 9 / B++ formats is the right
application of that.

---

## Decision 1 — CPU software rasterizer vs GPU

**Key framing: most of the engine is CPU either way.** BSP, PVS, culling, 3D
math, collision, and entity logic all run on the CPU in both paths. **Only the
final triangle rasterization is the CPU-vs-GPU choice** — a swappable renderer
backend, exactly like the existing CPU/GPU FPS siblings.

- **Performance:** a CPU software rasterizer is **slower than the GPU, always**
  — the GPU is purpose-built silicon. CPU rendering is *not* a performance play.
  It is, however, perfectly **viable** at Quake resolutions (Quake ran 320×240
  on a Pentium; a modern CPU does that trivially) and **zero-dependency** (no
  Metal/Vulkan), which fits the B++ ethos.
- **Why do it anyway:** the CPU rasterizer is the **lesson** — it teaches
  exactly what the GPU hides (rasterization rules, perspective-correct
  interpolation, depth testing, clipping). Writing it makes the later GPU work
  *better understood*, not redundant.

**Decision:** phased hybrid. The CPU software rasterizer is a deliberate
**learning module** (understand the pipeline); the GPU is the **production /
shipping** path. Build them as siblings over the same CPU-side engine (BSP/PVS/
math/collision), the way `fps_3d.bpp` (CPU) and `fps_3d_gpu.bpp` (GPU) already
share input + map + body. Modern world ships on the GPU; the CPU path is how we
learn why.

---

## Decision 2 — the compiler needs no pre-arc

Quake was **C** (not C++), plus hand-written x86 **asm** for the hot inner loops
and **QuakeC** (a custom bytecode VM for *game logic*, not the engine). What
C-as-used-in-Quake has that B++ does not — and why none of it blocks the port:

| C/Quake feature | B++ | Port approach |
|---|---|---|
| `union` (float↔int punning; QuakeC `eval_t`) | absent | explicit reinterpret — `read_f32`/`write_f32`/`read_u32`/`peekfloat` (already in `bpp_buf`) |
| `typedef` (`vec3_t = float[3]`, …) | absent | named structs directly (`struct Vec3 {x,y,z}`) or buffers |
| preprocessor (`#define`/`#include`) | absent | modules (`import`) + `const` |
| `goto` (error cleanup) | absent | restructure with flags / early returns |
| `switch` fallthrough | differs | B++ switch has no fallthrough (value/condition dispatch) — translate |
| varargs (`Con_Printf(...)`) | absent | replace printf-style logging with `put`-style |
| inline asm (hot loops) | absent | **not needed** — SIMD/autovec + modern hardware |

B++ **has** the rest Quake leans on: `enum`, `switch`, `struct`, `sizeof`,
`const`/`static`, floats (f16/f32/f64 via `: float`/`: half`/`: double`),
bitfields (`: bit`), function pointers (`fn_ptr`), SIMD/autovec.

**Conclusion:** none of the gaps are capability blockers — they are
translation/ergonomic differences the funnel handles. The most recurring one,
`union`, is already covered by explicit reinterpret. **So there is no compiler
pre-arc; the work is entirely in stb cartridges + engine code.**

### Important — the gap table is NOT a "features to add to B++" TODO

"C has it and B++ does not" ≠ "B++ is missing it." The table splits three ways:

- **Deliberate absences — keep absent (B++ is simpler/safer on purpose):**
  preprocessor (replaced by modules + `const`), `goto` (structured control),
  `switch` fallthrough (B++'s no-fallthrough + W021 exhaustiveness is *safer*).
  Adding these back would be identity regression — the same Multics-drift the
  8-phase lattice taught (see `feedback_phase_overengineering_lesson`).
- **Already covered by a better B++ mechanism:** `union` → explicit reinterpret
  (`read_f32`/`write_f32`); varargs → `put` smart-dispatch; inline asm → SIMD.
- **Minor ergonomic only:** `typedef` (named structs already cover it).

So porting Quake should **not** chase C feature-parity. The real stack growth
from this arc is **domain-shaped**, earned by use and gated by Rule 20
(two consumers) / Rule 28 (restraint):

- **Generics** (Excalibur Feature 4, deferred) for vec/mat + typed vertex
  arrays — a 3D engine is the canonical consumer, and may be the *second named
  consumer* that finally opens that arc.
- Possibly **vector/SIMD ergonomics** (e.g. `a + b` on a vec3) that the
  rasterizer's hot math would stress.
- And above all the **domain cartridges** (`stbmath3d`, `stbbsp`, the 3D
  renderer, BSP collision) — that is the stack that grows for B++ gamedevs.

Let the engine *surface* a need; add only what a real idiom demands twice. The
harder game is the right driver — of domain features, not C-parity. The
prioritized language-evolution arc this drives — built-in vector/matrix math
(learn from Odin), generics via monomorphization (Zig comptime = Excalibur F4),
compile-time execution (Jai `#run`), and SOA/AOS (Jai) — is laid out in
[`docs/plans/bpp_3d_evolution.md`](../../docs/plans/bpp_3d_evolution.md).

---

## Missing stb cartridges (the actual work, dependency order)

| Cartridge | What it is | Notes |
|---|---|---|
| **`stbmath3d`** | vec3/vec4, mat4, quaternion, plane, dot/cross/normalize, MVP, frustum | foundational; current raycaster only uses 2D trig |
| **renderer (CPU + GPU)** | textured-triangle rasterization | CPU: spans + z-buffer + perspective-correct + surface caching. GPU: extend Metal/Vulkan pipeline to 3D (MVP + depth + textured tris + lightmap) |
| **`stbbsp`** | load/traverse BSP tree, decompress + walk PVS | the heart of Quake; pure CPU data-structure work (like the pathfinder/ECS) |
| **collision** (extend `stbphys`) | BSP-trace (sweep an AABB through the planes), movement physics | — |
| **models (MDL)** | vertex-morph animation, textured triangle meshes | monsters/weapons |
| **lightmaps** | baked static lighting (texture × lightmap) | trivial on GPU |
| **asset loaders** (funnel) | PAK archive, BSP map, MDL model, WAD/textures, palette, sound | rip-from-owned-game → Bang 9 / B++ formats |

Existing building blocks to reuse: `stbcamera`, `stbraycast`, `stbtexture`,
`stbpixels`, `stbsound`, `stbmixer`, the GPU door (Metal 2D + Vulkan FFI), the
runtime profiler, and the funnel conversor.

---

## Phasing sketch (à la the Stratagus port)

1. **Math + static world** — `stbmath3d` + BSP loader; render one static map
   (GPU path first, for a tangible "walk around a Quake level" milestone).
2. **Visibility** — PVS decompression + leaf culling.
3. **Movement + collision** — BSP-trace, player physics.
4. **Models + animation** — MDL load + vertex-morph render.
5. **Sound + entities/gameplay** — and, as the deep CS lesson, the **CPU
   software rasterizer + perspective-correct mapping** as a sibling backend.

Scope is honest: this is a **large** arc — bigger than rts2, months of work —
and the natural escalation of the FPS Frankenstein vision (Wolf3D → Doom →
**Quake** → original game). It is also the most CS-rich thing to build: BSP,
PVS, rasterization, 3D math, collision — the canon of 90s 3D.

---

## Next

The full **port plan** is written: [`docs/plans/quake_port.md`](../../docs/plans/quake_port.md)
— Quake's subsystems enumerated from the GPL source (`/Users/Codes/Quake/
WinQuake/`), each mapped to B++/stb status, ordered into seven phased milestones
that interleave the language-evolution track. The two decisions above (CPU/GPU
hybrid; no compiler pre-arc) are its starting premises.

**Entry point: Phase 0** — `stbmath3d` (ported from `mathlib.c`) + the built-in
vector/matrix language feature it drives. Self-contained, well-tested, and the
single most impactful first rung. See the plan's Part E.
