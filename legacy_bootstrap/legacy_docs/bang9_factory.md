# Bang 9 as BangDev's Game Factory

**Audience**: Anyone trying to understand what Bang 9 IS and how games plug into it.
**Status**: Vision doc. Current implementation hits ~60% of the target.

---

## The mental model in one line

Bang 9 is an **IDE + asset pipeline** that reads and writes a single, conventional
project layout. Games are just B++ programs that read the same layout at runtime.
The same files power both.

---

## The convention (where files live)

```
games/<game_name>/
├── <game_name>.bpp              ← game entry point, thin
├── <game_name>_lib.bsm          ← (future) game logic as a library
├── assets/
│   ├── sprites/
│   │   ├── hero.modulab.json    ← modulab canonical (editor-only)
│   │   ├── hero.json            ← stbsprite format (game loads this)
│   │   ├── rat_sprite.json
│   │   └── cat_sprite.json
│   ├── levels/
│   │   ├── level1.level.json    ← level_editor format
│   │   └── level2.level.json
│   └── sounds/                  ← (future) mini_synth output
│       └── coin.synth.json
└── build/
    └── <game_name>              ← binary, produced by Bang 9 Build
```

Two rules, both enforced by convention (not the compiler):

1. **Sprites live in `assets/sprites/*.json`** (stbsprite format).
   The game reads them via `load_sprite(path, buf, w, h)` from stbdraw.
   Modulab writes `<name>.modulab.json` (canonical, for round-tripping) AND
   `<name>.json` (stbsprite, for the game) on every Save via `mlab_save_both`.

2. **Levels live in `assets/levels/*.level.json`** (level_editor format).
   The game reads them via a lib helper (to be written) that calls
   `file_read_all` + `json_parse`. Today most games hardcode level data;
   refactoring to file-backed levels is the unlock for "edit in Bang 9".

---

## What's working today (2026-04-21)

| Piece | Status | Notes |
|-------|--------|-------|
| Bang 9 IDE chrome | ✅ | Menu, tabs, status bar, folder picker |
| Project panel | ✅ | Browse / descend / ascend folders |
| Sprites panel (modulab embed) | ✅ | Edits `.modulab.json` AND imports `.json` (NEW) + `.png` |
| Levels panel (level_editor embed) | ⚠️  | Works, but hardcoded 40×22 tile size — doesn't match pathfind's 20×11 |
| Code panel | ✅ | Read-only viewer |
| Run panel | ✅ | Build / Build+Run / Run buttons |
| Subprocess launch | ✅ | cd's into project root so relative asset paths work (FIXED TODAY) |
| Asset auto-load on tab open | ⚠️  | Sprites tab auto-loads `.modulab.json` → falls back to `.json` |
| Live in-window game preview | ❌ | Phase 4 (see docs/bang9_live_preview.md) |
| Game-reads-level-from-disk | ❌ | Blocker for Levels tab to be useful |
| Audio editor (mini_synth) | ❌ | Phase 3 of embed plan |

---

## What BREAKS the universal factory today

### 1. Games hardcode their levels

Pathfind's `build_arena()` has `tile_set(tm, 5, 2, T_WALL)` sprinkled inline.
Level_editor can't touch that — there's no file to edit. User asked:

> "será que a gente tem que fazer o level hardcoded?"

**No**. The fix is a ~40-line pathfind refactor:

```c
// NEW: games/pathfind/level_io.bsm
load_pathfind_level(path) {
    // parse .level.json, fill `tm` via tile_set
    // validate size matches GRID_W × GRID_H
}

// games/pathfind/pathfind.bpp in init_game():
- build_arena();
+ load_pathfind_level("assets/levels/level1.level.json");
```

Plus one-time: save the current hardcoded arena as `level1.level.json`
(via a throwaway dumper or hand-written JSON).

### 2. Level editor has a fixed 40×22 grid

Most games are smaller (pathfind is 20×11). Level_editor won't let you edit
a 20×11 file — it rejects as "size mismatch".

Fix options:
- **A.** Make `LVL_W` / `LVL_H` runtime-configurable. Init takes `(name, w, h)`.
  About 200 lines of de-constantification. Unblocks every future game.
- **B.** Each game ships its own level_editor fork with matching dimensions.
  Cheap but non-universal. Not the BangDev way.

Recommendation: **A**, next session.

### 3. Games aren't library-shaped yet

`pathfind.bpp` has `main()` + window + loop all in one file. To preview live in
Bang 9 (Phase 4), pathfind needs `pathfind_lib_init / _frame / _shutdown` — same
pattern modulab and level_editor now have.

Not a blocker for subprocess Run (which works today). Required for live preview.

---

## The minimum viable "universal engine"

You can ship a Bang-9-native game today without any of the Phase 4 work, as long
as you follow the convention:

1. Put sprites in `assets/sprites/*.json`.
2. Put level data in `assets/levels/*.level.json` (size = your game's tile grid).
3. Read both at runtime via `load_sprite` + a 20-line `load_level` helper.
4. `bang9 games/<game>` → edit + subprocess run works end-to-end.

The artist never leaves Bang 9 except to see the running game (separate window).
That's not ideal but it's already 10× better than "edit in vim, run in terminal".

---

## The target (Phase 4 and beyond)

Once every game is library-shaped AND reads assets from files:

```
┌──────────────────────────────────────────────┐
│ File  Edit  View  Build  Help                │
├──────────────────────────────────────────────┤
│ Project│Sprites│Levels│Code│Run│ Play ●     │
├──────────────────────────────────────────────┤
│                                              │
│   [ game rendering live here ]               │
│                                              │
│   Edit rat sprite in Sprites tab             │
│   → tab to Play                              │
│   → rat has new look, immediately            │
│                                              │
└──────────────────────────────────────────────┘
```

This is the BangDev vision. One window, whole workflow.

---

## Session-by-session roadmap

Each session is ~1 day of work. Order matters — later steps depend on earlier ones.

### Next session

1. **Make level_editor size-configurable** (~200 lines, level_editor_lib.bsm)
2. **Refactor pathfind to load level from `assets/levels/level1.level.json`**
   (~40 lines in pathfind.bpp, one-time dump of current arena as JSON)
3. **Fix error spam** from auto-load probes (try_load should parse-check
   before calling the real loader, not after)

After this session:
- Levels tab works for pathfind ✅
- Edit level1 → Build+Run → see new arena ✅

### Session +1

4. **Library-extract pathfind** (`pathfind_lib.bsm`)
   — same pattern as modulab_lib.bsm extraction

5. **Add Play tab to Bang 9**
   — renders pathfind_lib_frame into the panel rect

After this session:
- Live in-window preview ✅
- Sprite/level saves trigger `pathfind_lib_reload_assets()` ✅

### Session +2

6. **Extract mini_synth_lib** + Music tab (Phase 3 of embed plan)
7. **Apply the library-extract pattern to rhythm** (second demo game)

### Session +3+

8. **Asset dirty propagation** — Bang 9 watches for saves, reloads
9. **Build system** — a `game.bang.json` manifest per game describing entry
   point, output, asset conventions (replaces the current basename-equals-dir
   heuristic in runner.bsm)

---

## Answers to the user's specific questions (2026-04-21)

> "como que eu monto o level, edito o sprite do personagem e vejo real time em
> ação, ta faltando esse link"

**Today**: Edit sprite → Save → click Build+Run → see result in separate window.
**Next session**: Refactor pathfind to load level from file so Levels tab is
meaningful. Then the loop is: edit anything → Save → Build+Run → see it.
**Phase 4**: Replace "Build+Run → separate window" with "tab to Play → live".

> "como que a gente faz ele ser uma engine pra gente criar, editar e aprimorar
> os jogos de forma universal pra bangdev?"

Three things make Bang 9 universal:

1. **Convention over config**: the `assets/sprites/*.json`, `assets/levels/*.level.json`
   layout is the contract. Games that follow it get everything. Games that don't
   stay on the CLI.

2. **Embed-ready tools**: modulab and level_editor already follow the `_lib_init /
   _frame / _shutdown` pattern. Every future asset editor (mini_synth, particle
   designer, animation timeline) follows the same shape.

3. **Games as libraries (Phase 4)**: when the game itself exports `_lib_init /
   _frame / _shutdown / _reload_assets`, Bang 9 can preview it live. One-line
   cost for the game, huge unlock for the artist.

> "eu to meio perdido em como amarrar todo o processo nessa fabrica que é bang9
> dentro do ecossistema b++"

The factory has four stations (Sprites, Levels, Code, Play). Each station is
JUST a panel in Bang 9, and every panel reads/writes the same `assets/`
directory. The game is the consumer at the end of the conveyor belt. Click
Build and the factory ships.

The missing pieces are: (a) make every game CONSUME from the standard directory
(pathfind's level refactor); (b) make level_editor flexible on grid size;
(c) make games library-shaped for live preview. Each of those is one session.
We're ~60% of the way there today.

---

## Cross-references

- `docs/bang9_tool_embed.md` — the original embed plan (Phases 1/2/3)
- `docs/bang9_live_preview.md` — Phase 4 (games as library panels)
- `tools/modulab/modulab_lib.bsm` — reference implementation of the embed contract
- `tools/level_editor/level_editor_lib.bsm` — second reference
- `bang9/runner.bsm` — subprocess launch (shell-wrapped with `cd` for asset paths)
- `bang9/panels.bsm` — auto-load wiring for Sprites + Levels tabs

---

## One sentence

Bang 9 becomes BangDev's universal game factory when every game reads its
assets from `assets/sprites/*.json` and `assets/levels/*.level.json`, every
game is shaped as a `_lib.bsm` with `init/frame/shutdown/reload_assets`, and
every tool in Bang 9's tab strip reads and writes the same conventional paths
— which it already does for sprites, almost does for levels, and will do for
everything else as each station gets wired.
