# Quake Port — WinQuake software engine → fps2 (B++)

**What this is.** A faithful port plan for the Quake engine (id Software, 1996),
reimplemented in B++. The GPL source is the spec, cloned at
`/Users/Codes/Quake/WinQuake/` (~99k LOC; the renderer alone is ~9k and is the
most complex subsystem). This document **enumerates what the engine does**, maps
each subsystem to its B++/stb status, and orders the port into shippable
milestones — interleaving the **language-evolution track**
([`bpp_3d_evolution.md`](bpp_3d_evolution.md)) that the engine earns as it goes.

Technique: *enumerate the source exhaustively → map to the target → port each
piece*, the same funnel used for the Stratagus AI in rts2 (`wc2_ai_port.md`).
The detailed source enumeration lives alongside this plan; this is the
actionable roadmap.

### The organizing principle — the product is the stdlib, not the game

Quake was built *on top of* C's standard library: a generic libc already
existed, and id wrote a Quake-specific engine in the application above it. The 3D
power lived in the game. **B++ has a choice id never had:** the 3D stdlib does
not exist yet, so as we port the engine, *every generic capability is routed
either into the stdlib (stb) or left stuck in the game (`games/fps2/`)*. If we do
not consciously push the generic 3D capability into stb, it congeals in the game
folder — amazing world-building tools trapped in one corner instead of
**pulverized across B++'s standard library, where every B++ gamedev inherits
them for free and standardized.**

So the **primary deliverable of this port is B++'s standardized 3D stdlib** —
`stbmath3d`, `stbbsp`, `stbrender3d`, `stbmodel`, `stbphys` (extended), spatial
audio — and **fps2 is the proving ground**: the first, demanding consumer that
drives and validates each cartridge. The layering:

```
language  (built-in vector/matrix, generics)   — the most generic, deepest
   ↑
stdlib (stb 3D stack)                           — THE PRODUCT
   ↑
games/fps2 (Quake's weapons/monsters/levels)    — a thin game on top
```

**The operational rule: Quake is the *implementation* spec, not the *API* shape.**
For each subsystem ask "what is the generic capability any 3D game wants here?"
and expose *that*, implemented with Quake's proven approach. `stbbsp` exposes
"spatial query + visibility", not `R_RecursiveWorldNode`; `stbrender3d` exposes
"submit meshes + camera → pixels" with swappable CPU/GPU backends, not Quake's
edge list. The boundary test for every line of code: *would another B++ 3D game
want this?* → yes = stdlib; "this is how Quake's Shambler attacks" = fps2.

The anti-speculation discipline still holds: **fps2 drives each API** (we only
expose what the game actually needs), but shaped for the domain, not for Quake.

Companion: [`games/fps2/HANDOFF.md`](../../games/fps2/HANDOFF.md) (decisions:
CPU/GPU hybrid; no compiler pre-arc; no asm to port — id ships a pure-C path).

**Status legend:** ✅ have it · 🟡 partial · ❌ missing.

---

## Part A — Current B++/stb status (what we build on)

| Capability | Status | Notes |
|---|---|---|
| Window / input / platform | ✅ | `stbwindow`, per-OS platform layer (macOS/Linux) |
| 2D GPU pipeline | 🟡 | Metal sprites + textured quads + shaders (`stbfx`/`stbshader`); Vulkan FFI door. **No 3D** (no MVP/depth/perspective triangles) |
| CPU raster | 🟡 | pixel blit (`stbpixels`, `put_px`) + raycast textured walls (`stbraycast`, `fps_3d.bpp`). **No triangle rasterizer** |
| 3D math (vec3/mat/plane) | ❌ | raycaster uses 2D trig; `stbcamera` has a yaw/pitch camera but no shared vec3/matrix library |
| Textures / images | ✅ | `stbtexture`, `stbimage` |
| Sound | 🟡 | `stbsound` + `stbmixer` mix audio; need Quake's channel model + 3D spatialization + WAV/sfxcache |
| Entities (ECS) | ✅ | `stbecs` — the home for Quake's edicts/entities |
| Pathfinding / AI | ✅ | `stbpath` (A*), `stbai` — for monster AI later |
| Math base | ✅ | `bpp_math` (sqrt via Newton, platform-agnostic trig) |
| Assets / files | ✅ | funnel conversor + file I/O; need Quake-format loaders |
| Profiler | ✅ | `stbprofile` — the CPU-vs-GPU readout we already use in the FPS siblings |

**Reading:** the platform, texture, sound-mixing, ECS, AI, and asset
infrastructure exist. The gap is the entire **true-3D stack** — math, BSP, the
renderer (both backends), models, BSP collision — plus the Quake-format loaders.

---

## Part B — WinQuake enumeration (the spec)

The engine, by subsystem. File references are into `/Users/Codes/Quake/WinQuake/`.

### 1. 3D math (`mathlib.c/.h`, ~675 LOC)
`vec_t` = float, `vec3_t` = vec_t[3]; no matrix struct — transforms are bare
`float[3][3]` (rotation) and `float[3][4]` (rotation+translation). Macros for the
hot ops (`DotProduct`, `VectorAdd/Subtract/Copy/MA`), functions for the rest
(`VectorNormalize` returns length, `CrossProduct`, `VectorScale`,
`AngleVectors` = euler→forward/right/up, `R_ConcatRotations`/`R_ConcatTransforms`
= matrix compose, `BoxOnPlaneSide` = AABB-vs-plane). `mplane_t` = {normal, dist,
type, signbits} drives BSP traversal and clipping. Fixed-point types
(`fixed8/16_t`) feed the rasterizer's integer inner loops.

### 2. BSP world model (`bspfile.h`, `model.c/.h`, ~2.6k LOC)
The on-disk `.bsp` is a header + 15 lumps. The geometry tree: `dplane_t`,
`dnode_t` (split plane + 2 children; negative child = leaf), `dleaf_t` (contents
+ a `visofs` into the PVS + a marksurface range), `dface_t` (plane, edge range,
texinfo, 4 lightstyles, lightmap offset), `dvertex_t`, `dedge_t`, `texinfo_t`
(s/t projection vectors), `miptex_t` (4 mip levels). Collision lives in a
parallel `dclipnode_t` tree (per-hull).

Loading (`Mod_LoadBrushModel` → the `Mod_Load*` lump readers) expands these into
in-memory `mnode_t`/`mleaf_t`/`msurface_t`/`texture_t`/`hull_t`/`model_t`.
`Mod_MakeHull0` builds collision hull 0 from the render tree.
`CalcSurfaceExtents` computes lightmap bounds per face.

**PVS** (the visibility system that made Quake run on a Pentium): a per-leaf
RLE-compressed visibility bitfield in the `visdata` lump. `DecompressVis` expands
it (run-length: a 0x00 byte means "skip N zero bytes", encoded in the next
byte). `Mod_LeafPVS` returns the decompressed set for a leaf.

### 3. The render pipeline (software) (`r_*.c` + `d_*.c`, ~9.1k LOC — the heart)
Per-frame flow (call order is the plan's backbone):
```
R_RenderView
  R_SetupFrame        — viewport, view transform, frustum planes
  R_MarkLeaves        — PVS: mark visframe on visible leaves
  R_EdgeDrawing
    R_RenderWorld
      R_RecursiveWorldNode   — walk BSP front-to-back; emit each leaf's
                               surfaces; R_RenderFace → R_EmitEdge (clip + add
                               face edges to the global edge list)
    R_ScanEdges        — per scanline: insert/remove/step active edges, derive
                         spans from adjacent edge pairs (R_GenerateSpans)
    R_DrawBEntitiesOnList  — brush-model (door/lift) surfaces, z-tested
    R_DrawEntitiesOnList   — alias models (see §4)
    R_DrawViewModel / R_DrawParticles
  D_DrawSurfaces       — for each span: build the surfcache, then the span drawer
```
**Edge list / spans** (`r_edge.c`, `r_draw.c`): the world is drawn span-by-span
from a sorted active-edge list (fixed16 u stepping). This is the perf-critical
core. **Surface caching** (`d_surf.c`): a surface's texture×lightmap is composed
and mipped once into a `surfcache_t`, then reused across frames (rover/LRU
eviction) — the trick that made textured+lit surfaces affordable. **Span drawers**
(`d_scan.c`): `D_DrawSpans8` (paletted + lightmap), `D_DrawZSpans` (z only),
`D_DrawTurbulent8Span` (water warp), sky. **Lightmaps**: `R_BuildLightMap`
composes 4 baked styles × `d_lightstylevalue[]` + `R_AddDynamicLights`; no GI,
only baked shadows + dlights. Texture mapping is **perspective-correct** (1/z per
span, affine within).

### 4. Alias models / MDL (`r_alias.c`, `d_polyse.c`, ~1.9k LOC)
Animated models are vertex-morph keyframes (`maliasframedesc_t`,
`mtriangle_t`, `aliashdr_t`). `R_AliasSetUpTransform` builds the world→view
matrix; `R_AliasPreparePoints` transforms verts; `R_AliasTransformAndProject…`
projects to screen; `R_AliasSetupLighting` shades per view angle;
`R_AliasSetupFrame` picks keyframe + lerp. `d_polyse.c` is a second rasterizer:
recursive triangle subdivision (`D_PolysetRecursiveTriangle`) for affine-but-
close-enough mapping, gradient calc, scanline fill (`D_PolysetDrawSpans8`).

### 5. Collision / movement (`world.c`, `sv_move.c`, `sv_phys.c`, ~2.4k LOC)
Collision traces the per-hull `dclipnode_t` tree, not the render tree.
`SV_HullPointContents` / `SV_PointContents` classify a point (EMPTY/SOLID/WATER/
SLIME/LAVA/SKY/CLIP). The trace sweeps a point/box through the hull recursively
(plane splits). Hull index is chosen by bbox size (point / player / large /
head). Entity proximity uses an `areanode_t` spatial grid (`SV_LinkEdict`,
`SV_TouchLinks`). Physics (`sv_phys.c`) does gravity, sliding, water.

### 6. Sound (`snd_dma.c`, `snd_mix.c`, `snd_mem.c`, ~1.8k LOC engine-side)
`channel_t` = {sfx, leftvol/rightvol, pos, looping, origin, attenuation}.
`S_StartSound` queues; `SND_Spatialize` pans L/R from the 3D origin vs the
listener; `S_PaintChannels` mixes channels into a 16-bit paintbuffer
(`SND_PaintChannelFrom8/16`); `S_TransferStereo16` copies to the output ring.
`sfxcache_t` holds decoded WAV samples (`GetWavinfo`).

### 7. QuakeC VM (`pr_exec.c`, `pr_edict.c`, `pr_cmds.c`, ~3.7k LOC)
A small bytecode VM runs the **game logic** (not the engine). `dstatement_t` =
{op, a, b, c} (operands are indices into `pr_globals`). `PR_ExecuteProgram` is
the opcode loop (STORE/LOAD/ADD/CALL/RETURN/IF/GOTO/STATE…). `edict_t` =
entities; `entvars_t` is the progs-defined field block; `eval_t` is the runtime
value union (float/vec3/string/func/edict). ~70 builtins (`pr_cmds.c`:
spawn/setmodel/setorigin/traceline/sound/walkmove…).

### 8. Master loop (`host.c`, `quakedef.h`, `server.h`, `render.h`)
`Host_Frame` → filter time → input → console → **server frame** (physics + game
tick) → read from server → **`SCR_UpdateScreen` → `V_RenderView` → `R_RenderView`**
→ **`S_Update`** (audio). `refdef_t` is the render definition (viewport, vieworg,
viewangles, fov). `entity_t` is a renderable; `edict_t` is a game entity. There's
a client/server split even in single-player (loopback).

> **Out of scope for the port:** networking (`net_*.c`, ~7k), the per-OS
> platform/video/input (`vid_*.c`/`sys_*.c`/`in_*.c`, ~10k — replaced by
> `stbwindow`/GPU), and the original menus/console (rebuild minimally). The
> `.s` assembly files are optional accelerations — we port the C.

---

## Part C — Port mapping (subsystem → fps2 status)

| # | Subsystem | Status | Gap → cartridge / work |
|---|-----------|:--:|---|
| 1 | 3D math | ❌ | **`stbmath3d`** (vec3/vec4, mat3x4/mat4, plane, AABB, AngleVectors, concat, normalize, BoxOnPlaneSide) — drives the built-in vector/matrix language feature |
| 2 | BSP load + PVS | ❌ | **`stbbsp`** (lump parser → in-memory tree; `DecompressVis`; PointInLeaf/LeafPVS). Funnel loader for `.bsp` |
| 3a | World render — GPU | 🟡→ | **`stbrender3d`** (GPU backend): MVP + depth buffer + perspective-correct textured triangles + lightmap multiply. Build meshes from faces |
| 3b | World render — software | ❌ | **`stbrender3d`** (CPU backend, the 90s lesson): edge list + span gen + surface cache + perspective-correct mapper (`r_edge`/`r_draw`/`r_surf`/`d_scan`) |
| 4 | PVS culling + BSP walk | ❌ | `R_MarkLeaves` + `R_RecursiveWorldNode` front-to-back → part of **`stbbsp`** (both backends consume it) |
| 5 | Collision (primitive) | 🟡 | trace/sweep/contents (`SV_*Contents`, box sweep) + generic helpers (gravity integrate, slide-along-normal, step-up) → extend **`stbphys`**; `areanode` grid → reuse `stbgrid` |
| 5b | Movement *feel* (mechanic) | — | Quake's accel constants, air-control, bunny-hop, jump tuning → **`games/fps2/`** (game mechanic, not engine) |
| 6 | Alias models (MDL) | ❌ | **`stbmodel`** loader + vertex-morph anim + animated triangle render (GPU first) |
| 7 | Lightmaps | ❌ | bake into surfaces (BSP lump) → texture×lightmap multiply (trivial on GPU; surfcache on CPU) |
| 8 | Sound | 🟡 | Quake channel model + `SND_Spatialize` (3D pan) + sfxcache/WAV → extend **`stbsound`/`stbmixer`** |
| 9 | Game logic | 🟡 | **reimplement QuakeC directly in B++** over `stbecs` (entities) — items/doors/combat/monsters. (A QuakeC VM is optional, later) |
| 10 | Master loop / refdef | 🟡 | `stbgame`/maestro already give the frame loop; add `refdef`-shaped camera state |
| 11 | Asset loaders (funnel) | ❌ | PAK archive, `.bsp`, `.mdl`, WAD/textures, palette, WAV → Bang 9 / B++ formats |

---

## Part D — Recommended port order (milestones)

Ordered for *gameplay-visible progress per phase*, GPU path first (fastest to a
tangible level), software renderer as the parallel deep-dive. **Each phase ships
a stdlib cartridge — that is the product; the fps2 milestone is the consumer that
validates it.** Each phase also pairs with the **language feature it earns** (the
`bpp_3d_evolution.md` track) — feature lands when the phase's idiom appears. Read
each milestone as "deliver stb cartridge X, proven by fps2 doing Y".

**Phase 0 — Math foundation.**
`stbmath3d` (vec3/mat3x4/plane/AABB + `AngleVectors`/concat/normalize). Port
`mathlib.c` point by point with tests against known values.
→ **Language: built-in vector/matrix types** (Odin-style component-wise + swizzle)
— `stbmath3d` is the immediate consumer; biggest ergonomic unlock, everything 3D
depends on it.

**Phase 1 — Walk a static level (GPU).**
`stbbsp` loads a `.bsp` (funnel); build per-face triangle meshes; extend the GPU
pipeline to 3D (MVP + depth + textured tris). Free-fly camera, no collision.
→ Milestone: *"you can fly through a Quake map in pure B++."*
→ **Language: generics** start to bite here (typed vertex/index arrays) — opens
Excalibur F4 with fps2 as the second named consumer.

**Phase 2 — Visibility.**
`DecompressVis` + `R_MarkLeaves` + `R_RecursiveWorldNode` front-to-back; cull to
the PVS. → Milestone: correct visibility + the perf win.
→ **Language: compile-time execution** (bake PVS/lookup tables) when it pays.

**Phase 3 — Move and collide.**
Hull trace (`SV_HullPointContents` + box sweep) + generic movement helpers
(gravity integrate, slide-along-normal, step-up) → `stbphys`; `areanode` grid →
`stbgrid`. The **Quake-specific movement feel** (accel constants, air-control,
bunny-hop, jump tuning) lives in `games/fps2/`, not in the cartridge — it is a
game mechanic. → Milestone: walk/jump/collide with Quake's feel.

**Phase 4 — Alias models + animation.**
`stbmodel` (MDL load + vertex-morph keyframes + lerp); render animated triangle
meshes (GPU). → Milestone: animated monsters/weapons on screen.
→ **Language: SOA/AOS** for vertex buffers when the hot data demands it.

**Phase 5 — Sound.**
Quake channel model + `SND_Spatialize` + sfxcache/WAV → `stbsound`/`stbmixer`.
→ Milestone: positional audio.

**Phase 6 — Gameplay.**
Reimplement the QuakeC game logic in B++ over `stbecs` — items, doors, triggers,
combat, monster AI (`stbai`/`stbpath`). → Milestone: a playable level.

**Phase 7 — The software renderer (the 90s lesson, parallel track).**
Edge list + span generation + surface caching + perspective-correct mapping +
`d_polyse` triangle rasterizer — as a **sibling backend** over the same CPU-side
BSP/PVS/math, exactly like `fps_3d.bpp` (CPU) sits beside `fps_3d_gpu.bpp` (GPU).
→ Milestone: software-rendered Quake — the Abrash masterclass. This is where the
deepest CS lives (fixed-point edge stepping, 1/z interpolation, cache eviction).

**Strategy (RTS-proven):** build each phase with good tests; then, once fps2 runs
naturally, let *production* surface the real bugs — the way rts2 playtests caught
the gather-contention, repair, and movement-thrash bugs that unit tests didn't.

---

## Part E — First concrete step (Phase 0 detail)

Start where everything depends: `stbmath3d`, ported from `mathlib.c`.

1. **Types:** `Vec3` (and `Vec4`), a `Mat3x4` (rotation+translation) and `Mat4`
   (projection). Decide the layout (struct `{x,y,z}` now; the built-in vector
   feature replaces it once the language gains it — write consumers so the swap
   is a rename).
2. **Ops:** dot, cross, add/sub/scale, length, normalize (return length, like
   Quake), `VectorMA`. Test each against hand-computed values (the `assert`
   pattern the book exercises use).
3. **Transforms:** `AngleVectors` (euler→fwd/right/up), `ConcatRotations`/
   `ConcatTransforms`, `BoxOnPlaneSide`. These are the BSP-walk + camera
   primitives Phases 1–2 need.
4. **The language feature, in parallel:** prototype built-in vector arithmetic
   (`a + b`, `v.xyz`) on `Vec3`/`Vec4`, validated by `stbmath3d`'s own tests.
   Keep it *built-in types*, not user operator overloading (see
   `bpp_3d_evolution.md`). Ship it byte-stable + suite-green like any compiler
   arc.

This phase alone is a clean, self-contained, well-tested cartridge + the single
most impactful language feature — the right first rung.

---

## Part F — The 3D stdlib layer (stb), and the fps2 boundary

This is **stdlib-first by design**: the cartridges below are the *product*, built
in `stb/` from the start — not game code that "graduates later when a second
consumer appears". That conservative gate is the trap that traps tools in a game
folder. The graduation is sanctioned by the project's own refined rule
(`feedback_consumers_in_view_graduation`: a Tier-2 cartridge may graduate with
**1 current consumer + 2+ concretely-named future ones**) — here that is fps2
(current) + every 3D game on the b++ roadmap (named). The anti-speculation
discipline is preserved differently: **fps2 drives each API** (we only build what
the game needs), shaped for the *domain*, not for Quake.

**The standardized 3D stack every b++ gamedev inherits:**

| Cartridge | Generic capability (the API shape) | Quake source = impl spec |
|---|---|---|
| `stbmath3d` | vec/mat/plane/AABB algebra | `mathlib.c` |
| `stbbsp` | load a BSP; spatial query (point→leaf); visibility (PVS) | `model.c`, `bspfile.h` |
| `stbrender3d` | submit meshes + camera → pixels; **swappable CPU/GPU backends**; depth, textured tris, lightmaps | `r_*`/`d_*` (CPU), `gl_*` (GPU) |
| `stbmodel` | load + play animated models (vertex-morph), render | `r_alias.c`, MDL format |
| `stbphys` (extend) | trace/sweep a box through a spatial structure; contents query | `world.c`, `sv_move.c` |
| `stbsound` (extend) | 3D spatial mixing (pan/attenuate from listener) | `snd_mix.c`, `snd_dma.c` |
| `stbcamera` (extend) | 3D camera / view definition (the `refdef` shape) | `r_main.c`, `view.c` |
| asset loaders | generic format readers: PAK, `.bsp`, `.mdl`, WAD, palette, WAV | (funnel) |

Below the stdlib, the **language** layer (`bpp_3d_evolution.md`): built-in
vector/matrix, generics, comptime, SOA — the most universal capabilities, pushed
deepest so the whole stdlib (and every gamedev) builds on them.

**What stays in `games/fps2/` (Tier-3, Quake-specific):** the game rules and
content — weapon/monster/item behaviour, level scripting, the specific asset
bindings, the tuning. The QuakeC game logic is *adapted* into B++ over `stbecs`;
the engine primitives it calls are all stdlib. The boundary test, applied to
every line: *would another B++ 3D game want this?* yes → stb; "this is how
Quake's Shambler attacks" → fps2.

Write the stdlib cartridges as **pure functions / clean handles over plain
data**, game-agnostic from line one — not Quake-shaped behind a generic name.
That is the difference between a 3D stdlib and a Quake engine wearing a stb
prefix.

---

## Part G — Scope, legality, constants

- **Scale honesty:** this is the largest arc the project has taken — the
  renderer alone is ~9k LOC of C (minus asm/platform). Phased, each milestone
  ships; do not attempt it monolithically (that is the Multics-drift trap).
- **Legality:** the engine code is GPL (study/port freely, license-aware). The
  assets (maps/textures/models/sounds) are id's — owning the game (GOG) covers
  personal/learning use; do not redistribute. Rip owned assets through the
  funnel into `games/fps2/`, the same as rts1/rts2/fps1.
- **Units:** Quake is right-handed Z-up, world units ≈ 1 unit. Keep the BSP's
  native scale; the camera/projection handles the rest.

---

## Constants we'll reuse

`MAX_MAP_*` lump limits, `CONTENTS_*` (EMPTY=-1, SOLID=-2, WATER=-3, …),
`SURF_*` flags (DRAWSKY/DRAWTURB/DRAWTILED), hull indices (0 point / 1 player /
2 large / 3 head), `MAXLIGHTMAPS=4` styles per face, the PVS RLE format. Pull the
exact values from `bspfile.h` / `quakedef.h` when each loader is written.
