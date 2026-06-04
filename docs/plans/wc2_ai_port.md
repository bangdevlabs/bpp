# WC2 AI Port — Stratagus engine → rts2 (B++)

**What this is.** A faithful port plan for the Warcraft II computer-player AI.
The wargus *scripts* (the build-order tapes in `land_attack.lua` etc.) sit on
top of the **Stratagus AI engine** (~7 000 LOC of C++ in
`stratagus/src/ai/`), which implements the primitives the scripts call
(`AiNeed`, `AiForce`, `AiAttackWithForce`, …) plus the per-second subsystems
that do the real work (resource manager, force manager, building placement,
attack planner). This document **enumerates everything that engine does**,
then maps each behavior to our rts2 status so we can reimplement it point by
point in B++.

Technique: *enumerate the source behavior exhaustively → map to the target →
port each discrete piece*. Same funnel discipline we use for assets (Rule 31),
applied to logic.

Related: [`bangscript_plan.md`](bangscript_plan.md) — the eventual declarative
DSL will *generate* exactly the script tape in Part A; this port gives it a
runtime worth targeting. Source: `stratagus/src/ai/{ai,ai_resource,ai_force,
ai_building,ai_plan,ai_magic}.cpp`, `ai_local.h`, `script_ai.cpp`.

**Status legend:** ✅ ported · 🟡 partial · ❌ missing.

---

## Part A — Current rts2 AI (`rts2_ai.bsm`, commit 214992e)

We already adopted the *shape*: a build-order TAPE executed one step per
strategic tick (5 Hz), with the economy + production + defence running every
tick underneath to fulfil the tape's targets.

**Opcodes** (`_ai_script` = flat `{op,a,b,c}` array, `_ai_pc` = program counter):

| Op | Effect |
|----|--------|
| `AI_WORKERS n` | set `_ai_worker_target` (economy trains peons toward it) |
| `AI_BUILD kind` | start one of `kind`; block until it exists |
| `AI_WAIT kind` | block until one is completed |
| `AI_FORCE m,r,c` | set army composition target (melee/ranged/cavalry) |
| `AI_ATTACK` | block until massed, then launch the wave |
| `AI_LOOP i` | jump to step `i` (endless escalating waves) |

**The tape:** 4 peons → farm → barracks → 6 peons → harass with 1 → attack
with 4 → 8 peons → stable → 6 melee + 2 ranged → combined arms + cavalry →
loop the two big waves.

**Subsystems we have:** per-kind building scan + army role classification
(`_ai_scan_*`), worker→mine assignment with a min-choppers floor
(`_ai_manage_workers`), supply farms on demand, peon + army training toward
targets (`_ai_try_train`, `_ai_train_army`), ring-search placement
(`_ai_build`, r=3..9 around the town hall via `wc1_placement_valid`), single
massed attack (`_ai_launch_attack` → focus enemy near their hall, fall back to
razing the hall), home defence preempt (`_ai_defend`).

**Single force, 5 Hz.** Stratagus runs at 1 Hz with up to 50 forces; we run a
single implicit army at 5 Hz. That's the biggest structural simplification.

---

## Part B — Stratagus AI engine enumeration (the spec)

> Architecture: a Lua coroutine-style script (a table of step-functions)
> advances one step per second; each step returns `true` (block / re-run) or
> `false` (advance). Primitives only **enqueue requests** and **define
> forces**; the C++ subsystems do the work each second. Global `AiPlayer`
> points at the current player's `PlayerAi`; all subsystem calls read/write
> through it.

### 1. Lifecycle & dispatch (`ai.cpp`)
- **AiInit(player)** — alloc `PlayerAi`; pick a `CAiType` by race+name match
  (fallback `AiTypes[0]`); copy its `Script`; seed `Collect[Gold]=50,
  Wood=50, Oil=0`.
- **AiEachCycle** — every game cycle: only sets `AiPlayer = player.Ai` (no
  per-frame work).
- **AiEachSecond** — the heavy tick (1 Hz): (1) set `AiPlayer`; (2)
  **AiExecuteScript** (advance one step); (3) **AiCheckUnits** (reconcile
  request queues vs owned units); (4) **AiResourceManager**; (5)
  **AiForceManager**; (6) **AiCheckMagic**; (7) **AiSendExplorers** at most
  once per 5 s.
- **AiCheckUnits()** — for each `UnitTypeRequests {Type,Count}`: owned =
  unitcount + equivalents; if `Count - owned - alreadyQueued > 0` →
  `AiAddUnitTypeRequest`. Then `Force.CheckUnits` tops up force demand; then
  `UpgradeToRequests` and `ResearchRequests` similarly.

### 2. Data model (`ai_local.h`)
- **PlayerAi** — `Force` (manager); resource arrays `Reserve/Used/Needed/
  Collect[MaxCosts]` + `NeededMask` (resources currently short) + `NeedSupply`
  flag; exploration request list; the three **script request queues**
  (`UnitTypeRequests`, `UpgradeToRequests`, `ResearchRequests`); and
  **`UnitTypeBuilt`** = the actual work-order list (`vector<AiBuildQueue>`);
  `LastRepairBuilding` round-robin cursor.
- **AiBuildQueue** — `{Want, Made, Type, Wait (retry gate), Pos (build-near,
  -1=anywhere)}`.
- **AiForce** — flags `Completed/Defending/Attacking`, `Role`
  (Attack/Defend); `UnitTypes` (composition demand `{Want,Type}`), `Units`
  (members); attack state `{FormerForce, State (Free/Waiting/Boarding/
  GoingToRallyPoint/AttackingWithTransporter/Attacking), GoalPos, HomePos,
  WaitOnRallyPoint=60}`.
- **AiForceManager** — fixed `forces[AI_MAX_FORCES=50]`; the upper half
  (≥`AI_MAX_FORCE_INTERNAL=25`) is reserved for engine-spawned attack copies;
  `script[]` maps script force-id → real index lazily.
- **AiHelper** — static capability DB (`DefineAiHelper`): per unit-type lists
  of who can Train/Build/Upgrade/Research/Repair, per resource the Refineries
  + Depots. **UnitTypeEquivs[]** collapses equivalent types to a canonical
  slot — *nearly every "do we have / can we build X" check expands through
  equivalence and prefers higher-PRIORITY variants (paladin over knight).*

### 3. Resource management (`ai_resource.cpp`)
Tunables: collect interval 4 s, depot/repair range 15, crowded depot 15 refs,
build-place retry 150 then 450 cycles.
- **AiCheckCosts(costs)** — recompute `Used[]` from in-progress Build orders;
  return bitmask of resources where `have+stored-used < cost-reserve`.
- **AiCheckSupply(type)** — sum food from supply buildings under construction +
  current `Supply-Demand-type.demand` − demand of everything training; false
  the moment it goes negative.
- **AiResourceManager()** — (1) **AiCheckingWork**; (2) speculative
  **AiRequestSupply** if `Supply==Demand`; (3) **AiCollectResources** every 4 s
  staggered by `PlayerIndex%4`; (4) **AiCheckRepair**; (5) clear `NeededMask`.
- **AiCheckingWork()** — supply first; then walk `UnitTypeBuilt`: skip if over
  limit; if costs missing → OR into `NeededMask` and continue (let cheaper
  items proceed); else if `Want>Made && Wait<=GameCycle` → **AiMakeUnit**; on
  success `++Made`; on building-place failure set backoff `Wait=+150` then
  `+450`.
- **AiMakeUnit(type)** — expand to available equivalents (sorted by PRIORITY
  desc); find builders/trainers via `AiHelper`; for each owned source →
  **AiBuildBuilding** / **AiTrainUnit**; return on first success.
- **AiBuildBuilding** — gather idle workers of the builder type, pick a random
  one, **AiFindBuildingPlace**, `CommandBuildBuilding`; retry with other
  workers if the first fails.
- **AiRequestSupply()** — cache supply-providing types; if one already queued,
  abort; cost-per-supply normalize, cheapest first; if affordable build it and
  **insert at front** of `UnitTypeBuilt`.
- **AiNewDepotRequest(worker)** — when a harvester's depot is too far: if a
  deposit already exists within 15 of the harvest spot abort; else queue the
  cheapest allowed depot type with `Pos=harvestLocation` (build near the
  resource).
- **AiGetSuitableDepot(worker,old)** — among storage buildings for the
  resource (need ≥2), nearest to worker that isn't the old one, has `Refs≤15`,
  no enemies within 15, and a reachable resource nearby.
- **AiCollectResources()** — the load balancer (4 s, staggered): survey
  harvesters into `assigned[res]`; idle-carrying → `CommandReturnGoods`;
  idle-empty → `unassigned[res]`. Desired distribution = `Collect[c]`
  **doubled if c ∈ NeededMask** → `wanted[c]`. Loop neediest-first: satisfy by
  taking a free worker or **stealing** from a lower-priority, less-needy
  resource (never one returning goods); decrement + re-sort until stable.
- **AiAssignHarvester** — terrain (forest) → `FindTerrainType` within 1000 →
  `CommandResourceLoc`; unit (mine) → nearest depot then nearest resource →
  `CommandResource`; on failure `AiExplore` with a move-mask.
- **AiCheckRepair()** — round-robin from `LastRepairBuilding`: damaged
  repairable building, not under construction, not attacked in last 5 s → skip
  if enemies in sight, skip if any needed resource `<99` stored, else
  **AiRepairUnit** (find ready worker within 15, `CommandRepair`). Also repairs
  an unworked under-construction building when resources are flush.
- **AiAddUpgradeToRequest / AiAddResearchRequest** — abort if costs missing
  (set `NeededMask`) or over limit; find source via `AiHelper.Upgrade/
  Research`; first idle owner → `CommandUpgradeTo` / `CommandResearch`.
- **AiEnemyUnitsInDistance(player,type,pos,range)** — box-select enemies near
  pos (optionally only counter-attackers); used everywhere (placement, rally,
  repair).

### 4. Building placement (`ai_building.cpp`)
All finders flood-fill (`TerrainTraversal`) from the worker / nearPos with a
move-mask = worker movement − occupancy bits.
- **AiFindBuildingPlace(worker,type,nearPos)** — dispatcher: depot →
  **HallPlaceFinder** (near mine) or **LumberMillPlaceFinder** (near terrain
  resource); mine → **AiFindMiningPlace** (must overlap the node); else →
  **BuildingPlaceFinder** with full surround check.
- **BuildingPlaceFinder** — per reachable tile: `CanBuildUnitType` AND no
  enemies within 8 tiles → **AiCheckSurrounding**; fully open → accept; else
  remember as `backupok` fallback.
- **AiCheckSurrounding** — walk the perimeter ring at `AiAdjacentRange=1`
  counting free→blocked transitions; `obstacleCount==0` is ideal,
  `backupok=<5` (normal) / `<3` (shore). **This is the spacing rule that keeps
  the AI from boxing in its own paths.**
- **HallPlaceFinder** — flood to a `ResourceOnMap` mine that **IsAUsableMine**
  (no enemy near, no depot already adjacent, `<2` other buildings clustering
  it), place near the mine.

### 5. Force management (`ai_force.cpp`)
Tunables: rally/near-goal dist 5, defender scan box 15, send stagger `i/5`,
`maxPathing=2` searches/tick.
- **Assign(unit,force)** — if ungrouped: into the given force iff
  **IsBelongsTo**, else first non-attacking force whose composition wants this
  type; set `GroupId`, refcount++.
- **IsBelongsTo(type)** — recompute owned-by-type (collapsed via equivs); set
  `Completed`; for each wanted `{Type,Want}` short by the candidate type flag
  belongs; short by >1 → not Completed. Drives recruitment + the ready-flag.
- **CheckUnits(counter)** — for each non-attacking force + wanted type, queue
  the shortfall vs `owned+equiv+counter−attacking` as build requests. *This is
  how a force composition becomes build orders.*
- **AiForceManager()** (1 Hz) — `Force.Update()` then
  `AiAssignFreeUnitsToForce()`.
- **Update()** — budget 2 pathing searches/tick.
  - *Defending forces*: dead-prune; empty → Waiting; goal off-map →
    ReturnToHome; reached goal + no enemies in range → ReturnToHome; allies
    near goal → order idle members to attack/move there (staggered).
  - *Attacking forces*: dead-prune → **AiForce::Update()**.
- **AiForce::Update()** (the attack ride) — no aggressive units → give up
  (`Reset(true)`). State machine: **Boarding** (load transports) →
  **AttackingWithTransporter** (move + unload) → **GoingToRallyPoint** (wait at
  a safe staging tile until the force gathers, then pick a target and switch to
  **Attacking**) → **Attacking** (all idle → re-find enemy; none → give up;
  else new rally point). Idle members re-issued orders toward the leader
  (staggered `i/5`).
- **AiForce::Attack(pos)** — record `HomePos`; if pos invalid search the whole
  map (**AiForceEnemyFinder**: Aggressive/AllMap/Building per force type); no
  land target / transporter involved → **PlanAttack** (sea invasion); else
  compute **NewRallyPoint** and GoingToRallyPoint (or Attacking). Send each
  unit: aggressive → `CommandAttack`, escort → `CommandDefend(leader)`, else
  `CommandMove`, staggered.
- **NewRallyPoint(start)** — flood from leader for a reachable tile with no
  enemies within 15 and `|dist−15|` small — a safe staging point toward the
  target. Resets `WaitOnRallyPoint=60`.
- **AiAttackWithForce(force)** — if not Defending, **clone** the script force
  into a fresh internal force (≥25): move units + UnitTypes, record
  `FormerForce`, reset the original (so the script force rebuilds). Then
  `Attack(invalid)`.
- **AiAttackWithForces(list)** — **merge** all listed forces into one internal
  force and `Attack` (units cooperate).

### 6. Attack planning & exploration (`ai_plan.cpp`)
- **PlanAttack()** — sea-invasion planner: mark transporter-reachable terrain,
  flood from a land unit treating that water as passable to find a reachable
  enemy, pull in transporters until board capacity suffices, set Boarding.
- **AiFindWall(force)** — flood ≤1000 for a `WallOnMap` tile to siege enemy
  walls (dormant `#if 0`).
- **AiSendExplorers()** — up to 5 tries: pick a random exploration request,
  **GetBestExplorer** (closest idle mover matching the terrain mask,
  **preferring flyers**, toward a random unexplored tile near the request) →
  `CommandMove`; then clear all requests.

### 7. Event handlers (`ai.cpp` callbacks)
- **AiHelpMe(attacker,defender)** — under-attack response: ignore
  friendly-fire / scouts / summons. Brothers-in-arms in the defender's force
  that can target the attacker and aren't mid-attack switch targets
  (`CommandAttack`, saving prior order). Then every non-attacking Defend force
  (or non-Waiting Attack force) sets `Defending=true` and `Attack(attacker.pos)`
  — the home guard responds.
- **AiUnitKilled(unit)** — remove from force; if the force is now empty clear
  Attacking and (if a committed attack force) `Reset(true)`.
- **AiWorkComplete / AiTrainingComplete** — building/unit finished →
  **AiRemoveFromBuilt** (decrement `Made`/`Want`, erase entry when `Want==0`);
  a trained unit is auto-enrolled into a force that wants it (`Force.Assign`).
- **AiCanNotBuild / AiCanNotReach** — failed build → **AiReduceMadeInBuilt**
  (decrement `Made` only, keep `Want` → retries later).
- **AiCanNotMove(unit)** — goal reachable but blocked by a unit →
  **AiMoveUnitInTheWay**: throttled 10 cycles; shove one random allied
  same-movetype idle blocker within `TileWidth+1` to a free adjacent tile.
- **AiNeedMoreSupply** — set `NeedSupply=true`.

### 8. Magic (`ai_magic.cpp`)
- **AiCheckMagic()** — for each idle spellcaster, enable autocast on the first
  available `AICast`-flagged spell it can cast.

### 9. Script primitives (`script_ai.cpp`) — effect on the data model
- **AiNeed(t)** — append `{t,1}` to `UnitTypeRequests` (non-blocking).
- **AiSet(t,n)** — set/insert `{t,n}` in `UnitTypeRequests`.
- **AiWait(t)** — block until owned (+equiv) ≥ requested Count.
- **AiForce(id,{t,n,…},reset?)** — define/update force `id` composition
  (normalize through equivs), State=Waiting, then
  `AiAssignFreeUnitsToForce(id)`.
- **AiForceRole(id,role)** / **AiWaitForce(id)** (block until `Completed`) /
  **AiWaitForces({ids})** / **AiCheckForce(id)** (query).
- **AiAttackWithForce(id)** / **AiAttackWithForces({ids})** / **AiReleaseForce(id)**.
- **AiResearch(u)** / **AiUpgradeTo(t)** — append to research/upgrade queues.
- **AiSleep(n)** — block n cycles (also gates speculative supply).
- **AiPendingBuildCount(t)** — queued `Want` for type.

### 10. Key tunables
`AI_MAX_FORCES=50`, `AI_MAX_FORCE_INTERNAL=25`, `AI_WAIT_ON_RALLY_POINT=60`,
collect interval 4 s (stagger `PlayerIndex%4`), explorer cadence 5 s,
move-in-way throttle 10 cycles, depot/repair range 15, crowded depot 15 refs,
placement no-enemy radius 8, surround obstacle limit `<5`/`<3` shore, build
retry 150→450 cycles, rally/defend distances 5/15, send stagger `i/5`,
`maxPathing=2`, terrain/depot search 1000, NeededMask doubles collect %,
explorer ray (start 3, ×1.5, 8 tries), `AiSendExplorers` 5 retries.

---

## Part C — Port mapping (subsystem → rts2 status)

| # | Subsystem | Status | Gap / notes |
|---|-----------|:--:|-------------|
| 1 | Lifecycle / dispatch | ✅ | `_ai_tick` 5 Hz ≈ AiEachSecond. We have no per-cycle/per-second split (don't need it). |
| 2 | Data model | 🟡 | We have the tape + per-kind scan + role counts. **Missing:** request queues, the `UnitTypeBuilt` work-order list with retry `Wait`, the **equivalence map** (paladin>knight preference). |
| 3a | Worker→mine assignment | ✅ | `_ai_manage_workers` + min-choppers. |
| 3b | Wood/forest balance + NeededMask ×2 boost | 🟡 | We hold ≥1 chopper; no demand-driven rebalance, no "double the needy resource". |
| 3c | Depot requests (new hall near far mines) | ❌ | `AiNewDepotRequest`/`AiGetSuitableDepot` — expand toward distant mines; **high value on big maps**. |
| 3d | Supply | ✅ | Farms on demand (food within 2 of cap). |
| 3e | Repair | ❌ | `AiCheckRepair` — repair damaged buildings with spare workers. |
| 4a | Placement search | 🟡 | Ring search r=3..9. **Missing the surround/spacing check** → can box itself in or fail to place (suspected cause of the *AI-stuck* bug). |
| 4b | Placement enemy-rejection | ❌ | Don't build within 8 tiles of enemies. |
| 5a | Force composition → build orders | ✅ | `_ai_train_army` trains toward melee/ranged/cav. |
| 5b | Multiple forces | ❌ | We run one implicit army; Stratagus juggles 50 (attack + defend + internal). |
| 5c | Force state machine (rally → attack) | ❌ | No rally points / gathering state — we launch the instant we're massed. |
| 5d | Clone/merge-to-internal attack | ❌ | Lets the script keep rebuilding while a wave is out. Our LOOP re-issues instead. |
| 6a | Attack target find | 🟡 | `_ai_launch_attack` = nearest enemy near their hall, fallback raze hall. No pathing/rally. |
| 6b | Exploration | ❌ | No fog/scouting model yet. |
| 7a | Defence (AiHelpMe) | 🟡 | `_ai_defend` turns the whole army on a base threat. Missing: per-unit brothers-in-arms target switch; defend-force role. |
| 7b | AiUnitKilled force cleanup | ✅ | Implicit (scan recomputes counts each tick). |
| 7c | AiCanNotBuild retry | 🟡 | The BUILD step re-tries each tick; no backoff `Wait`. |
| 7d | AiCanNotMove unblock | ❌ | `AiMoveUnitInTheWay` — shove idle blockers. **Likely fixes "units stuck"**; pairs with our tile-claim collision. |
| 8 | Magic / autocast | ❌ | Casters in roster, no spell AI. |
| 9 | Script primitives | 🟡 | Have WORKERS/BUILD/WAIT/FORCE/ATTACK/LOOP. **Missing:** RESEARCH/UPGRADE (no tech tree yet), SLEEP, FORCE_ROLE, multi-force, AiSet-vs-AiNeed. |

---

## Part D — Recommended port order (point by point)

Ordered by *gameplay impact per LOC*, smallest robust wins first.

1. **Placement surround check (4a/4b).** Port `AiCheckSurrounding` (perimeter
   obstacle count ≤ limit) + enemy-within-8 rejection into `_ai_build`.
   Suspected fix for the AI getting stuck unable to place a building.
2. **AiCanNotMove unblock (7d).** Port `AiMoveUnitInTheWay` (throttled shove of
   one idle blocker) into the movement tick. Suspected fix for units freezing
   — pairs with our existing tile-claim collision.
3. **Depot requests (3c).** When a mine is far from the hall, queue a new town
   hall near it (`AiNewDepotRequest` + usable-mine check). Big-map economy.
4. **Repair (3e).** Idle-worker repair of damaged buildings when resources are
   flush.
5. **Worker rebalance + NeededMask ×2 (3b).** Demand-driven gold/wood split;
   double the workers on a resource the build queue is starved of.
6. **Rally-point force state (5c) + better target find (6a).** Gather at a safe
   staging tile, then commit — stops the dribble-attack feeding.
7. **Research / Upgrade primitives (9).** Needs the tech-tree first (separate
   arc); then `AI_RESEARCH` / `AI_UPGRADE` opcodes + the upgrade execution.
8. **Multiple forces + clone/merge (5b/5d).** The big structural step:
   promote our single army to an `arr_struct<AiForce>` with roles. Do last —
   most of the value lands in 1–6 with the single force.
9. **Equivalence map (2).** Only matters once upgrades exist (knight→paladin).
10. **Magic autocast (8)** and **exploration (6b)** — lowest priority for a
    no-fog skirmish.

Keep the **tape data-driven** throughout (`_ai_script_init`) — each step above
adds a subsystem the tape *targets*, never tape-specific code. This is the
runtime [`bangscript_plan.md`](bangscript_plan.md) will emit into.

---

## Part E — Known issues (observed 2026-06-04 playtest)

- **AI got stuck ("travado").** Enemy stopped progressing. Prime suspects, in
  order: placement failure with no surround fallback (item 1) leaving a `BUILD`
  step blocked forever; or the economy stalling because peons froze (item 2).
  Diagnose with `RTS_DEBUG=1 ./rts2` and read the heartbeat `pc=` (which tape
  step is stuck) + `peon=` / `gold=`.
- **Crude death animation** when enemy peons die — separate rendering issue
  (death anim frames / timing), not AI. Track under unit-render polish.

---

## Part F — stb graduation map (Tonify Rule 33 / Rule 20)

**Principle.** `stbai`'s own header deliberately EXCLUDES the RTS brain:
*"Combat / target acquisition logic (… RTS routes orders — too different)"*.
The RTS AI is Tier-3 (`rts2_ai.bsm`) by design. Only one RTS exists
(`games_roadtrip.md` Game 3), so per Rule 20 there is **no second consumer** —
the brain stays in rts2. A few port items, however, are **fully-generic grid
algorithms** (not RTS-specific) with a natural Tier-1/2 home; write *those*
as pure functions over a grid + a predicate so graduating them later is a
move, not a rewrite.

| Port item | Nature | Home now | Graduation target | Gate |
|---|---|---|---|---|
| Placement surround/open check (item 1) | fully-generic grid algo (count free→blocked transitions around a footprint) | rts2 — **pure** `(gx,gy,w,h,range)` over a cell-free predicate | `stbgrid` (perimeter scan taking a `fn_ptr` predicate) | a 2nd grid-placement consumer |
| Unblock stuck unit (item 2) | movement-coupled | `rts2_movement` | `stbflow` / `stbgrid` | a 2nd RTS |
| Nearest-enemy flood / target acquisition (6a) | combat AI | rts2 | — `stbai` **excludes** this by its own header | — |
| Worker balancer (3b), depot (3c), repair (3e) | RTS economy brain | rts2 | — Tier-3 | a 2nd RTS |
| Force state machine / rally (5, 6) | RTS brain | rts2 | — Tier-3 | a 2nd RTS |
| Build-order tape VM (Part A) | RTS-shaped behavior VM | rts2 | **bangscript** (the DSL emits the tape) | the bangscript arc |

**Convention for this port:** write the generic-algorithm rows (placement,
unblock) as PURE helpers — a grid/footprint in, an obstacle count out, no
`_ai_*` globals — so graduation is a file move + rename. Everything else is RTS
brain and correctly lives in `rts2_ai` (Tier-3); it graduates only when a
second RTS appears (then revisit a possible `stbrts` Tier-2 cartridge). Do
**not** widen `stbai` for the RTS brain — its header already drew that line.

---

## Constants we'll reuse (rts2 units, 32 px tiles)

Stratagus distances are in tiles; multiply by `WC1_TILE_PX = 32` for pixel
thresholds. Our `_ai_defend` range is `(12 tiles × 32)² = 147456`. Placement
no-enemy radius 8 tiles, rally 5, defend box 15 — all × 32 when used in pixels.
