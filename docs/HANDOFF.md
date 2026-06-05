# Handoff — read before you start producing

Last updated: **2026-06-05** (after rts2 repair + the wc1→wc2 rename).

Welcome. Before writing any code in this repo, read these `docs/manual/` files —
they encode the conventions that keep the self-hosting compiler byte-stable and
the stb library clean. Skipping them is how regressions get in.

## Read first, in this order

1. **`docs/manual/how_to_dev_b++.md`** — the core dev loop: how to build (`./bpp
   prog.bpp -o out`), run the test suites, the bootstrap (gen2==gen3
   byte-stability) contract, and the project layout. Start here.
2. **`docs/manual/tonify_checklist.md`** — the standing rules. Especially **Rule
   33** (Tier 1/2/3: prelude vs stb cartridge vs game module — what graduates and
   what stays in the game), **Rule 20** (two-consumer rule for graduation), Rule
   37 (cite `tests/bench_compile.sh` for any perf claim), and the `@safe` /
   warning contracts (Rules 4, 28). Check warnings every commit (W031 has ridden
   through before).
3. **`docs/manual/stb++_lib.md`** — the cartridge library map (stbecs, stbgrid,
   stbflow, stbpath, stbui, stbhud, stbprojectile, …). Know what already exists
   before you write a new system; most "new" RTS needs are a consumer of an
   existing cartridge.
4. **`docs/manual/debug_with_bug.md`** — the `bug` debugger (`--bytes`,
   `--disasm`, objdump-in-bug). This is how compiler/codegen bugs get diagnosed
   here — read the disassembly, don't guess.
5. **`docs/manual/funnel_conversor.md`** + **`docs/manual/warning_error_log.md`**
   — only if you touch asset conversion (`tools/funnel/` WC2 sprites/tiles) or
   need the E###/W### code reference.

`bang9_space_manual.md`, `bootstrap_manual.md`, `nomad_manual.md` are deeper
references — read on demand, not up front.

## Where rts2 (the Warcraft II mod) is right now

- **Game:** `games/rts2/` — WC2 (Tides of Darkness) port. Native 640×480 FOV
  (Blizzard-remaster letterbox technique), 32px tiles, foot-anchored unit draw,
  WC2 icon-strip portraits/command card. All code is `wc2_`-prefixed now;
  file headers match `rts2_*.bsm`.
- **AI port plan:** `docs/plans/wc2_ai_port.md` — the worked example of the
  "enumerate the source engine, then port point-by-point" discipline. We hold the
  Stratagus + Wargus source (`/Users/Codes/Warcraft/stratagus|wargus/`) — **when
  reimplementing a Stratagus system, READ ITS SOURCE, don't guess from
  screenshots** (this saved the whole gather-contention pass). Items 1–4 ✅ done.
- **Next task: item 5** — worker rebalance + `NeededMask ×2` (demand-driven
  gold/wood split; double the workers on a resource the build queue is starved
  of). See Part D of the plan.
- **Untested:** the repair feature (item 4, `d763b8c`) compiles + boots but
  hasn't been playtested. The user verifies each feature visually before moving
  on — ask them to damage a building and right-click it with a peasant (player
  path) and watch the AI mend its own (AI path) before counting it done.

## House rules (quick)

- Commits go to `main`. Commit-message footer:
  `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`.
- Comments in **English**, manual-style (what + why).
- Unit/building stats stay **hardcoded** (no JSON crutch — user preference).
- There is pre-existing uncommitted work in the tree that is **not** the rts2
  arc (`tools/forge/`, `tools/font_forge/` deletions, `sidequest_hedged_reads.md`,
  `games/rts1/rts`). Leave it alone unless the user asks — it's a separate
  sidequest.
