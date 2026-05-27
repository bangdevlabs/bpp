# RTS Engine Robustness Arc — study + adapt (not port)

**Status:** proposed 2026-05-26, pending reviewer alignment.
**Why now:** the S11 enemy AI is the first system to drive *autonomous
multi-unit economy*, and it stress-exposed robustness gaps that manual
human play hid. The gaps freeze the **human's** economy too — they are
the real playability blocker, not the AI's brain.

## Premise

The mature reference is **Stratagus** (the engine under war1gus = WC1 and
wargus = WC2). Its robustness lives in the C++ engine — which is **not in
the local tree** (we only have WC1 data + war1gus Lua scripts + tools)
and is ~100K+ LOC. So:

- **Do NOT port the engine.** Infeasible (not local, too large, wrong
  language/architecture for our stb cartridges).
- **Study the specific ALGORITHMS** Stratagus uses for the three weak
  subsystems and **adapt the idea** into our stb engine. Keep our
  cartridges (`stbpath` A*, `stbflow` BFS field, `stbgrid` occupancy,
  `stbecs`) — augment them, don't replace.
- The war1gus **Lua scripts stay as DATA reference** (unit costs in
  `balancing.lua`, build orders + force comps in `ai.lua`) — same way
  we already used `icons.lua` / `buttons.lua`.

**Engine source is now local:** `git clone --depth 1 Wargus/stratagus`
into `/Users/Codes/Warcraft1/stratagus` (8.8 M, full `src/`). The key
files to study: `src/action/action_resource.cpp` (harvest — STUDIED,
see Adaptation 1), `src/pathfinder/pathfinder.cpp` + `src/action/
action_move.cpp` (movement/pathing — TODO), `src/ai/ai_resource.cpp`
(AI resource mgmt). The war1gus Lua (units/AI/balancing) is the DATA
layer on top.

## The three weak subsystems + current gaps

| Subsystem | Current (rts1) | Gap exposed |
|---|---|---|
| **Gather** | Worker walks to a *perimeter tile* of the resource and stands there harvesting | Multiple workers contend for the same tile → one parks, others freeze. Reachable-resource finding is ad-hoc (interior trees unreachable). |
| **Movement** | `_step_unit` waits when the next tile is occupied; head-on swap added | 3+ unit jams, a parked unit blocking a needed tile, and convoy cycles still deadlock. No give-way / re-path. |
| **Pathfinding** | `stbpath` A* on terrain only (ignores units); `stbflow` for groups | A path never re-routes around a *unit* blocker (units aren't in the A* grid), so a blocked unit can only wait. |

## Adaptation 1 — Gather: the "enter-resource" model (CENTERPIECE)

**This is the highest-value change — it removes perimeter contention AND
the gather deadlock at the root, and it's the authentic WC1 behaviour.**

**Status in tree (2026-05-26): IMPLEMENTED, pending visual smoke.** The
gold-mine enter-model is complete and compiles clean (0 warnings):

- "Remove from map" half: on enter the worker releases its perimeter
  tile (`wc1_movement_release_unit_tile`) and the render hides the
  `GATHER_GOLD` worker — a harvesting worker holds no tile, so the
  perimeter pile-up that froze gatherers is gone.
- Capacity model: `Building.active` counts gatherers inside a mine,
  gated by `_WC1_MINE_MAX_ON_BOARD = 5` (war1gus `buildings.lua:286`).
  A worker arriving when the mine is full idles at its perimeter tile
  and re-tests next tick — a poll-based equivalent of Stratagus's
  explicit wake-the-waiter drain. `active++` on enter (`_gather_visit`),
  `active--` on leave (harvest-done) and on cancel / reassign
  (`_reset_anim_if_gathering`, the Stratagus :244 abort-release).

All game-local (Tier 3) — nothing went into `stbpath` / `stbgrid` here,
because a per-mine concurrent-gatherer cap is WC1 semantics, not a
generic cell-storage or pathfinding primitive (Rule 33). The cartridge
work is Adaptation 3 (`path_find_avoiding` → `stbpath`).

**Open validation:** visual smoke — gold income paces correctly, no
perimeter freeze with 6+ workers commanded onto one mine. **KNOWN
EDGE:** a worker killed WHILE inside a mine leaks one slot (combat sets
`DYING` without routing through the leave / cancel paths). Rare (the
worker is hidden inside the mine), noted for a follow-on `active--`
guard in `wc1_combat`'s death path, or a per-tick recount if it bites.

**Grounded in `action_resource.cpp`** (studied 2026-05-26, line refs
below). Stratagus's harvest is a state machine
(`COrder_Resource::Execute`, dispatch at :1309–1413):
`SUB_START_RESOURCE → SUB_MOVE_TO_RESOURCE → SUB_START_GATHERING →
SUB_GATHER_RESOURCE → SUB_STOP_GATHERING → SUB_MOVE_TO_DEPOT →
SUB_RETURN_RESOURCE → loop`. The robustness comes from a **capacity
model**: each resource has `MaxOnBoard` (max concurrent gatherers) and
`Resource.Active` (current count). Mechanics confirmed from source:

- **Gate** (`StartGathering`, :569): if `Active >= MaxOnBoard` the
  worker `unit.Wait = 10; return 0` — **waits cheaply, no re-path, no
  crowding** (source comment: "CPU usage is really low"). This is what
  turns a mob into an orderly queue.
- **Enter** (:605): `Resource.Active++`. For the gold mine
  (`HarvestFromOutside` unset → false) the worker is `unit.Remove(goal)`d
  off the map into the resource (:585) — the "vanish into the mine"
  visual; zero tile occupancy while inside.
- **Leave** (`StopGathering`, :871): `Resource.Active--`, then walk
  `AssignedWorkers`, find the longest-waiting `IsGatheringWaiting()`
  worker and set `next->Wait = 0` (:886–919) — the **queue-drain** that
  unfreezes exactly one waiter. The leaver `DropOutNearest`s onto a free
  tile toward the depot (:939/:965).
- **Cancel** (:244–246): `Resource.Active--` so an aborted gather frees
  its slot.

Wood is a separate path (`TerrainHarvester`, :523–543): the worker chops
*adjacent* to the tree, never enters, no capacity. So WC1 already splits
mines (capacity/enter) from trees (chop-adjacent) — matching the split
proposed below.

Today a harvesting worker is a unit standing on a perimeter tile. Replace
with the Stratagus/WC1 model:

1. **Approach:** worker paths to a walkable tile adjacent to the resource
   (already have `wc1_movement_find_walkable_perimeter`).
2. **Enter:** on arrival, the worker is *removed from the world* (hidden,
   no Pos/occupancy) and a harvest timer starts. The resource tracks how
   many workers are inside (a small capacity, e.g. WC1's per-mine limit).
3. **Exit:** when the timer elapses, the worker re-appears at a free
   adjacent tile carrying cargo, then paths to the deposit.
4. **Deposit + loop:** credits the resource, walks back, re-enters.

Why it fixes everything:
- **No perimeter contention** — workers aren't standing on tiles; they're
  *inside*. N workers share one mine with zero tile conflict.
- **No gather deadlock** — the only movement is approach + return, on
  short routes, and the head-on swap handles those crossings.
- **Authentic** — this is exactly how WC1 looks (peasants vanish into the
  mine, re-emerge with gold).

Wood/trees: the same model, except a felled tree clears to grass (already
done) and the worker re-targets the nearest *edge* tree (already done).
Trees have no "inside", so wood keeps the stand-adjacent-and-chop model —
but with the felled-clears-to-grass + nearest-edge-tree logic, wood no
longer piles (only one worker per tree; the tree is consumed).

Scope: rewrite the gather state machine's mine path (`rts1_gather.bsm`)
around enter/exit. ~150-250 LOC. Touches the Anim/Gather state + the
production tick's render skip (already skips workers "inside" the mine).

## Adaptation 2 — Movement: give-way + re-path on block

Beyond the head-on swap (done), add Stratagus-style block resolution:

- **Stall counter** in `Path` (new field). Increment each frame the
  next tile is blocked by a unit.
- On a short stall (e.g. 4 frames): **re-request the A* path** — but
  with the blocker's tile *temporarily marked blocked* so A* routes
  around it. (Our A* ignores units today; this is the targeted bridge.)
- On a longer stall with no alternate: the **lower-priority unit gives
  way** — steps to any free adjacent tile, then re-paths. Priority by a
  cheap rule (e.g. carrying/attacking > idle, else higher eid yields).

This handles the cases the head-on swap doesn't: 3+ jams, a parked
blocker, convoy cycles. ~60-100 LOC in `rts1_movement.bsm` + 1 `Path`
field.

## Adaptation 3 — Pathfinding: dynamic re-route

The give-way above leans on "A* around a temporarily-blocked tile." Make
that a first-class capability: `path_find_avoiding(pf, src, dst, blocked
list, buf)` — A* that treats a small set of extra tiles as blocked for
this one query (the blocker tiles). Keeps `stbpath` terrain-only by
default; the avoid-list is per-call. ~40 LOC in `stbpath` (Tier-1
cartridge — genre-agnostic, clears the 2-consumer gate via any
crowd-pathing game).

## Prioritization + sequencing

1. **Gather enter-model** (Adaptation 1) — biggest win, fixes the core
   economy freeze for human + AI. Do FIRST.
2. **Movement give-way + re-path** (Adaptation 2) — fixes residual jams.
3. **`path_find_avoiding`** (Adaptation 3) — enables 2; do alongside it.

Only AFTER these does the AI brain (the reviewer's manager pattern, or
the current FSM) sit on a solid engine — because then "assign worker →
gather" and "march army" actually execute reliably.

## What this is NOT

- Not a Stratagus port. Not new cartridges beyond augmenting `stbpath`.
- Not the AI manager framework (that's the layer ON TOP, after this).
- Not a rewrite of `stbecs`/`stbflow`/`stbgrid` — they stay; we augment
  gather (game-local) + movement (game-local) + `stbpath` (one helper).

## Open questions for the reviewer

1. ~~Does the enter-model apply to wood?~~ **ANSWERED** by the source:
   no — mines use the `MaxOnBoard`/`Active` capacity model; wood
   (`TerrainHarvester`) chops adjacent. Our split is correct.
2. ~~Pick our mine `MaxOnBoard`~~ **ANSWERED** by the war1gus data:
   `buildings.lua:286` sets `unit-gold-mine MaxOnBoard = 5` (campaign);
   `balancing.lua:85` rebalances to `3` (multiplayer). NOT 1. The gold
   mine is a *remove-from-map* harvester (`HarvestFromOutside` unset →
   the source `unit.Remove`s the worker, action_resource.cpp:585). We
   adopt **MaxOnBoard = 5** + the remove-from-map model; in B++ that is
   "release tile + render-hide while inside" (already done) — no ECS
   despawn needed, which also dodges the slot-recycling guard we already
   maintain in `_gather_visit` / `_render_unit`.
3. Movement give-way priority + re-path: study `action_move.cpp` +
   `pathfinder.cpp` next (engine is now local) before speccing
   Adaptation 2/3 in detail.
