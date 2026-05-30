# Sidequest brief: BW-style army / horde movement for rts1

**For:** the external research agent. **Pattern:** same as the free-list,
file_stat, and allocator studies — study how RTS engines actually move
armies (StarCraft: Brood War is the user's gold standard), come back with a
comparative table + a recommendation for rts1 + **where it lives** (game
code vs an stb cartridge), phased (α/β/γ), with sources.

## The goal (what the user wants)

A **player-vs-AI skirmish that PLAYS**: the user commands one army, the AI
commands the other, both **march across an open field and clash**. The
reference is **StarCraft: Brood War** — "enviable movement, hard but
generates good gameplay." The user explicitly values that the *imperfection
is the depth* (clumping, micro, masterable jank), NOT a frictionless
SC2-style flow.

## What the 200v200 stress test actually revealed (the real findings)

`RTS_STRESS=N ./rts` spawns an open-field 200-vs-200 arena (see `rts.bpp`).
Running it proved:

1. **The engine RENDERS + TICKS 400 units at 62 fps** (ms:16, max:17). Perf
   is in the industry ballpark — NOT the problem. (This is the payoff of the
   allocator + file_stat + the spatial auto-acquisition: the auto-target
   scan went from O(N²) — 98% of the frame, `_acq_consider`+`ecs_get` — to
   O(window) via `wc1_collision_unit_at`, ~16 samples.)
2. **But armies cannot MARCH.** The player's dense block can't cross the
   field to the enemy; it gridlocks / extrudes ~one column per tick. The
   AI's army sits. So **no skirmish happens** — the stress exercises asset
   loading + render + the idle tick, but not units *in action*. This is the
   actual limit the stress surfaced, and it's the thing to fix.
3. **Movement is janky even at 2–3 selected units** — so this is NOT only a
   crowd-scaling problem; the base per-unit movement (A* + tile-claim) is
   rough at any scale.

## Current rts1 movement model (so you don't re-derive it)

In `games/rts1/rts1_movement.bsm`, built on `stbpath` + `stbflow` + `stbgrid`:

- **Cell-locked:** each unit claims ONE tile in a shared stbgrid occupancy
  field (`_occupied`, via `wc1_collision_register/release/is_blocked_for`).
  Strictly one unit per tile.
- **Direction:** per-unit A* (`stbpath`) for groups `< FLOW_THRESHOLD` (6);
  a shared flow-field (`stbflow`: `flow_compute` from the goal, `flow_step`
  samples the gradient) for groups `>= 6`.
- **Collision (`_step_unit`):** when the next tile is occupied, the unit
  either runs `_try_headon_swap` (ONLY for head-on/convergent crossings —
  the gather-route deadlock) or `_idle_anim` (WAITS that frame). There is
  **no general give-way / push / yield.**
- **Result:** a dense block gridlocks — every next-tile is held by a
  friendly also trying to move, so all wait; only the front edge advances,
  and entity-order processing (not flow-order) makes even that ripple
  slowly. Small groups are janky from the A* + wait-on-block.
- **Diagnosis:** this is the *worst hybrid* — cell-locked rigidity (WC1 /
  C&C) WITHOUT BW's pushing/shuffling, so it FREEZES instead of FLOWING.

## The spectrum (my analysis — validate or refute)

- **Brood War:** sub-cell positions + collision **pushing** (units shove
  each other apart, never freeze) + region-based pathfinding + per-unit
  local behavior. The army forms a clumpy "ball" and flows; the
  clumping/jank is *masterable* — the depth the user wants. NOT rigid
  formation, NOT a flow-field.
- **SC2 / Supreme Commander / PA:** flow-field + boids steering
  (separation / cohesion / alignment), continuous positions → smooth,
  "perfect" group flow. Easier, less movement-micro depth (the "soulless"
  critique).
- **OpenRA (C&C remake, cell-based, open-source) — the MOST relevant
  reference, since rts1 is cell-based:** cell-locked + a **give-way /
  unit-yield** system + hierarchical pathfinding + deadlock resolution.
  Closest to rts1's model and shows how to move armies on a grid without
  gridlock.

## Research questions

1. **Brood War internals:** the collision/push model (sub-cell? collision
   radius? how the shove resolves), the region-based pathfinder, and *why*
   the jank is masterable. What specifically makes BW army movement feel
   good despite being imperfect?
2. **OpenRA cell-based give-way:** how does it march armies on a cell grid
   without gridlock — the unit-yield / "step aside" / re-path system + the
   hierarchical pathfinder? (Likely the most directly portable model.)
3. **SC2 / SupCom flow + steering:** for contrast — what does continuous
   position + separation steering buy, and what's lost (determinism, the
   micro "soul")?
4. **Cell + push hybrid:** can rts1 KEEP a deterministic tile grid but add
   push / give-way so dense blocks shuffle (BW-ish) without going fully
   continuous? The minimal change to `_step_unit`'s "blocked → wait".
5. **Per-unit quality (the "janky at 2–3"):** what makes a SINGLE unit's
   path smooth — path smoothing, look-ahead, reroute-vs-wait? rts1 is rough
   even at small N.
6. **Processing order:** does processing moving units in flow/path order
   (front-first) vs entity-id order materially improve cohesion + speed?

## Where does it live? (the user's stb instinct — Rule 33 / 35)

The user expected this work to "touch the compiler or an stb," not just game
code — and they're right that crowd movement is **generic** (FPS swarms,
RPG mobs, future RTS). So the study must recommend WHERE the BW-style
movement lives:
- evolve **`stbflow`** (flow-field) + **`stbgrid`** (occupancy) with a
  give-way / push step, OR
- a new **`stbcrowd` / `stbsteer`** Tier-2 cartridge.

Name the future consumers (Rule 36b). The movement, done right, is **infra**
that graduates out of `rts1_movement.bsm` — that is the "touch an stb"
angle. (Compiler: out of scope — movement is runtime, not a language
feature; the only compiler tie-in is autovec for steering math, which
already exists.)

## Constraints any recommendation must respect

- rts1 is **tile-based** (16 px tiles, stbgrid `_occupied`). The user LIKES
  the deterministic cell model + the BW "hardness" — do NOT recommend
  SC2-smooth-and-easy unless the case is overwhelming.
- **62 fps @ 400 units** is the perf bar. Movement must scale: the
  auto-acquisition fix showed the pattern — spatial / O(window), never
  O(N²).
- Rule 33 (cartridge tiers / where it lives) + Rule 41 (if it graduates to
  stb, the policy is portable, per-OS primitives untouched).
- Determinism preferred (replay / bootstrap-friendliness).

## Deliverable shape (mirror the file_stat / allocator studies)

A comparative table (BW / SC2 / OpenRA / SupCom: cell vs continuous, push
vs flow vs formation, pathfinder, determinism, "feel") + a recommendation
for rts1 (my prior: cell-based + give-way/push toward the BW feel, likely
via OpenRA's approach) + WHERE it lives (game vs `stbflow`/`stbgrid`
evolution vs a new `stbcrowd`) + a phased plan (α = quick win in
`rts1_movement`; β = the stb graduation; γ = anything heavier). If the
honest answer is "go continuous (SC2)," show why it's worth losing the cell
determinism + the BW feel the user asked for.

## Cross-references

- `games/rts1/rts1_movement.bsm` — the current model (cell-claim + flow +
  head-on-swap, no give-way).
- `stb/stbflow.bsm`, `stb/stbgrid.bsm`, `stb/stbpath.bsm` — the movement
  primitives that would host the graduated logic.
- `games/rts1/rts1_combat.bsm` — the spatial auto-acquisition (the pattern
  for scaling: O(window) over the occupancy grid, not O(N)).
- `docs/plans/sidequest_allocator_final_push.md` — the prior study format.
