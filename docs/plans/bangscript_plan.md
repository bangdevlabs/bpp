# bangscript — Adventure Game DSL for B++

bangscript is a declarative scene description format (`.bang` files)
paired with a B++ runtime engine (`stbbangs.bsm`). Scene files are
**data, not code** — they are parsed at runtime like JSON, not compiled
by the B++ compiler. The result is a clean separation between the
engine (B++ code) and the content (`.bang` scripts), where an artist
or writer can edit scenes without touching a single line of B++.

bangscript exists because adventure games are the reason B++ exists.
SCUMM defined LucasArts. SCI defined Sierra. bangscript defines B++.

## Design Principles

1. **Data, not code.** `.bang` files are declarative scene descriptions,
   loaded at runtime by the B++ engine. The B++ compiler is never
   modified. Zero compiler extensions, zero metaprog dependency.

2. **Separate files, separate concerns.** `.bpp` files contain engine
   code and game logic. `.bang` files contain scenes, hotspots,
   cutscenes, and dialogue. An artist edits `.bang`; a programmer
   edits `.bpp`. They never collide.

3. **Native engine, interpreted scenes.** The engine (stbbangs.bsm)
   is compiled B++ — fast, debuggable, profiled. The scene data is
   walked at runtime via a simple command dispatcher (switch over
   node types). Scene logic is not performance-critical; rendering
   and audio are.

4. **Callbacks for complex logic.** When a scene needs custom puzzle
   logic (item combinations, conditional state machines), the `.bang`
   file calls a B++ function by name via `call`. The boundary is
   explicit and clean.

5. **Instant hot reload.** Changing a `.bang` file does not require
   recompilation. The engine re-parses the file and the scene updates
   in-place. Iteration time for scene content: under 1 second.

## Historical References

| Engine | Studio | Era | Format | Key insight |
|--------|--------|-----|--------|-------------|
| **AGI** | Sierra | 1984-1989 | LOGIC scripts + resource files | Separated logic from resources (PIC, VIEW, SOUND). First engine to let non-programmers build rooms. |
| **SCI** | Sierra | 1988-1996 | Smalltalk-based scripts | Evolved AGI with OOP. Scripts compiled to bytecode, run by interpreter. King's Quest V, Gabriel Knight. |
| **SCUMM** | LucasArts | 1987-1998 | `.scm` script files | Cooperative multitasking in 1988. Parallel actor threads. Monkey Island, Indiana Jones. |
| **Ren'Py** | Tom Rothamel | 2004-present | `.rpy` files | Line-oriented, indent-based. Millions of games made. Most successful modern adventure DSL. |
| **Ink** | Inkle Studios | 2016-present | `.ink` files | Pure narrative format. Branching, conditions, diversions. 80 Days, Heaven's Vault. |
| **Yarn Spinner** | Secret Lab | 2015-present | `.yarn` files | Unity integration. Line-oriented, node-based dialogue. |
| **AyumuScript** | pac-ac (OsakaOS) | 2024-present | embedded OS shell | Line-based parser inside a hobby OS. **Special variables (`$KEY_CHAR`, `$MOUSE_X`)** expose runtime state as the engine-script boundary. `ex` (sync) vs `ext` (async-task) execution modes. ~150 LOC interpreter handles loops, conditions, functions, lists. See [pac-ac/osakaOS](https://github.com/pac-ac/osakaOS). |

bangscript takes from each: AGI/SCI's resource separation, SCUMM's
parallel actor model, Ren'Py's line-oriented readability, Ink's
narrative purity, and **AyumuScript's special-variable boundary
between engine state and script logic** (added 2026-05-25 after the
OsakaOS deep-dive — see "Script Expressivity" section below). The
`.bang` format is closest to Ren'Py in syntax but closer to SCUMM in
execution model (coroutine scheduler, not linear script), with
AyumuScript's `$var` convention adopted for runtime-state reads.

---

## Scope — Adventure Demo

The first bangscript game is the **Adventure Puzzle demo**: the cherry
on top of B++ 1.0. It ships last in the roadtrip, after all other
games and dev loop items have proven the infrastructure.

**Prologue scope (1.0 target):** 1 scene, 3-5 hotspots, 1 multi-step
puzzle, ~5-10 minutes of gameplay. Proves bangscript end to end.

**Episode zero (stretch goal):** 3-4 scenes, multiple puzzles,
inventory, 1 cinematic cutscene, ~20-30 minutes of gameplay. The demo
people will remember.

The artist is confirmed — original assets, no copyright issues.

---

## The `.bang` Format

### Design rules

- One instruction per line.
- Indentation (2 spaces) defines hierarchy.
- Strings in double quotes.
- Numbers are integer literals.
- Suffix `s` = seconds, `ms` = milliseconds, `f` = frames.
- `#` starts a comment.
- `set` / `check` for boolean flags (binary persistent state).
- `var` for script-local variables (scene-scoped numeric/string state).
- `$name` reads a value — either a script-defined `var` or a
  special variable published by the engine (`$mouse_x`, `$time_sec`,
  etc. — see "Special Variables" below).
- Expressions in conditions: `check $score >= 100`, `check $hp > 0`.
- `call` invokes a B++ function (the bridge to custom logic).
- `par` executes children in parallel (waits for all to finish).
- `spawn` starts a child block in the background and continues
  immediately — fire-and-forget, used for NPC behaviors that
  outlive the parent block.
- `loop` / `pool` repeats a block while a condition holds (or a
  fixed count). Different from `par` — `loop` is sequential
  iteration.
- `choice` / `label` / `goto` for branching.

### Scene definition

```bang
# scenes/office.bang — Indy's office

scene indys_office
  bg "indys_office.png"
  music "office_ambient.wav"

  actor indy at 200 300

  hotspot desk at 100 200 150 50
    on look
      say indy "My desk. Always cluttered."
    on use key
      walk indy desk 2s
      say indy "The key fits..."
      wait 1s
      sound "unlock.wav"
      set desk_unlocked
      goto hallway
    on use screwdriver
      say indy "That won't work here."

  hotspot bookshelf at 300 100 80 200
    on look
      say indy "Old textbooks. Nothing useful."
    on pick
      say indy "Too heavy."

  exit door at 500 100 50 200
    goto hallway
```

### Cutscene definition

```bang
# scenes/intro.bang — Opening cutscene

cutscene intro
  fade_in 2s
  music "theme.wav"
  par
    walk indy 400 300 3s
    cam_pan 0 0 500 0 3s
  say indy "I hate snakes."
  wait 1s
  fade_out 1s
```

### Dialogue tree

```bang
# scenes/guard_dialogue.bang

dialogue guard_encounter
  say guard "Halt! Who goes there?"
  choice
    "I'm a professor." -> friendly
    "None of your business." -> hostile
  label friendly
    say guard "Ah, Dr. Jones! Go right ahead."
    set guard_friendly
  label hostile
    say guard "I'm calling security."
    goto game_over
```

### Special variables (engine → script boundary)

Inspired by **AyumuScript** in OsakaOS, the engine publishes runtime
state as **read-only special variables** that scripts read with the
`$` prefix. This replaces dozens of small B++ callbacks with direct
script reads.

Engine-published variables (read-only from script):

| Variable | Type | Source |
|---|---|---|
| `$mouse_x`, `$mouse_y` | int | cursor position in screen coords |
| `$mouse_click` | int (0/1) | 1 if mouse pressed this frame |
| `$key_last` | int | last raw key code received |
| `$key_pressed` | int (0/1) | 1 if any key down this frame |
| `$time_sec`, `$time_ms` | int | elapsed time since `bangs_init` |
| `$frame_count` | int | total frames rendered |
| `$player_x`, `$player_y` | int | player actor position |
| `$current_scene` | string | id of the active scene |
| `$active_actor` | string | actor currently in focus (last spoke) |
| `$random_0_100` | int | new random integer 0-99 per read |
| `$dt_ms` | int | frame delta in milliseconds (read by AI behaviors) |

Mutations always flow one way: **script reads via `$`, writes via
`call` to a B++ function**. The engine never lets script write
runtime state directly — that boundary stays clean.

Example — branching on live state:

```bang
hotspot ambient_check at 0 0 800 600
  on enter
    if $time_sec > 60
      say indy "I've been here too long."
    fi
    if $mouse_x > 400
      face indy right
    fi
```

This eliminates the "create a flag for every comparison" antipattern.

### Script-local variables (`var`)

Scenes can declare local variables that exist only for the scene's
lifetime — they reset on `bangs_enter`. Useful for counters, temps,
and accumulators that don't need to persist as flags.

```bang
scene office
  var visit_count 0
  var last_item_picked "none"

  on enter
    var visit_count $visit_count + 1
    if $visit_count > 3
      say indy "I keep coming back here."
    fi

  hotspot desk at 100 200 50 50
    on pick
      var last_item_picked "key"
      say indy "I picked up the key."
```

`var` accepts expressions on the RHS (numeric: `+ - * / %`,
comparisons: `< <= > >= == !=`, strings: literal only). The
expression evaluator is shared with `check`.

### General loops (`loop` / `pool`)

`par` runs children concurrently and waits for all. `loop` repeats
sequentially while a condition holds (or for a fixed count).

```bang
# Loop while condition is true:
behavior guard_patrol
  loop $guard_alert == 0
    walk guard waypoint_a 3s
    wait 1s
    walk guard waypoint_b 3s
    wait 1s
  pool
  say guard "I see you!"

# Loop a fixed number of times:
cutscene knock_at_door
  loop 3
    sound "knock.wav"
    wait 500ms
  pool
  say indy "Someone's here."
```

### Async background tasks (`spawn`)

`par` blocks the parent until all children complete. `spawn` starts
a child block in the background and the parent continues
immediately. The spawned task runs as a separate coroutine — useful
for NPC behaviors that should outlive the cutscene that started
them.

```bang
cutscene intro
  fade_in 2s
  spawn guard_patrol      # guard starts walking, doesn't block
  say indy "Time to leave."
  walk indy door 3s
  # guard_patrol is still running here, will continue after cutscene
```

Maps to AyumuScript's `ex` (sync, like `par`) vs `ext` (async, like
`spawn`) distinction.

### Function definitions (post-MVP Phase 1)

Reusable named blocks for DRY patterns. Stored alongside scenes in
the scene graph; invoked by name with positional args.

```bang
function greet(actor_id, greeting)
  walk $actor_id indy 2s
  face $actor_id indy
  say $actor_id $greeting
end

scene tavern
  on enter
    greet bartender "Welcome traveler!"
    greet patron_1 "What's the news?"
    greet patron_2 "Have a drink with me."
```

Function args use `$arg_name` to read inside the body. Return values
via `$R` (mirrors AyumuScript). **Out of MVP scope** — adds ~100 LOC
for call stack + arg binding. Tracked for Phase 1 after the MVP
ships.

### Callbacks to B++ for complex logic

```bang
  hotspot chest at 400 200 50 50
    on use lockpick
      call try_open_chest
```

```bpp
// game.bpp — the B++ side
try_open_chest() {
    if (bangs_flag_get(FLAG_HAS_LOCKPICK)) {
        bangs_flag_set(FLAG_CHEST_OPEN, 1);
        bangs_say(ACTOR_INDY, "Got it!");
        bangs_sound("chest_open.wav");
        inventory_add(ITEM_GOLD);
    } else {
        bangs_say(ACTOR_INDY, "I need something to open this.");
    }
}
```

---

## Architecture

### How it works at runtime

```
game.bpp (B++)                    .bang files (data)
─────────────────                 ──────────────────
main() {                          scene indys_office
  bangs_init();                     bg "indys_office.png"
  bangs_load("scenes/");            hotspot desk at 100 200 ...
  bangs_enter("indys_office");        on look
  game_run();  // maestro loop          say indy "My desk..."
}                                     on use key
                                        walk indy desk 2s
                                        ...
```

1. `bangs_init()` — allocates the scheduler arena and command arrays.
2. `bangs_load(dir)` — reads every `.bang` file in the directory,
   parses each into an in-memory scene graph (array of nodes).
3. `bangs_enter(scene_id)` — activates a scene, loads its background,
   registers its hotspots.
4. Each frame, `bangs_tick(dt)` — advances active coroutines (walk,
   say, wait, fade, par) and checks hotspot interactions.
5. Verb clicks dispatch to the `on` handler for the matched
   verb+target pair. The handler is a sequence of commands executed
   as a coroutine (one command per frame or blocking until done).
6. `call` commands look up a B++ function by name in a callback
   registry and invoke it.

### The `.bang` parser (~300 lines)

A line-oriented parser in pure B++. No dependency on the B++ compiler
internals. The parser:

1. Reads a `.bang` file line by line.
2. Counts leading spaces to determine indent level.
3. Splits each line into command + arguments.
4. Builds a tree of BangNode structs in arena memory:
   ```
   BangNode { type, indent, str_arg, int_args[4], children, next }
   ```
5. Command types are integer constants:
   `CMD_SCENE`, `CMD_BG`, `CMD_ACTOR`, `CMD_HOTSPOT`, `CMD_ON`,
   `CMD_SAY`, `CMD_WALK`, `CMD_WAIT`, `CMD_SOUND`, `CMD_MUSIC`,
   `CMD_SET`, `CMD_CHECK`, `CMD_GOTO`, `CMD_CALL`, `CMD_PAR`,
   `CMD_FADE_IN`, `CMD_FADE_OUT`, `CMD_CAM_PAN`, `CMD_CHOICE`,
   `CMD_LABEL`, `CMD_EXIT`.

~20 command types. Each is 1-2 lines of parsing code. The parser is
~300 lines total and is a leaf module (no imports beyond bpp_array,
bpp_str, bpp_io).

### The command dispatcher (~200 lines)

A switch statement inside the coroutine tick loop:

```bpp
bangs_exec_cmd(node) {
    auto cmd;
    cmd = *(node + BANG_TYPE);
    if (cmd == CMD_SAY) { _bangs_do_say(node); }
    else if (cmd == CMD_WALK) { _bangs_do_walk(node); }
    else if (cmd == CMD_WAIT) { _bangs_do_wait(node); }
    else if (cmd == CMD_SOUND) { _bangs_do_sound(node); }
    else if (cmd == CMD_CALL) { _bangs_do_call(node); }
    // ... ~20 cases
}
```

Each `_bangs_do_*` handler is 5-15 lines. The total dispatcher is
~200 lines including all handlers.

---

## Prerequisites (Already on the 1.0 Path)

Everything below must land BEFORE bangscript work begins. With the
`.bang` format, the prerequisite list is SHORTER than the embedded
approach — notably, **metaprogramming is no longer a prerequisite**.

| #  | Item | Why bangscript depends on it | Status |
|----|------|------------------------------|--------|
| 1  | Maestro batch 2 | Coroutine scheduler uses maestro for par blocks | in progress |
| 2  | `.bo` transitive hash | Ensures stbbangs.bsm cache is sound | pending |
| 3  | stbaudio (Rhythm Teacher) | Music + SFX + voice during cutscenes | pending |
| 4  | stbmath trig (Wolf3D) | `walk` needs sin/cos for angular paths | pending |
| 5  | Hot reload | Re-parse `.bang` on file change (trivial with `.bang`) | pending |
| 6  | stbdialogue + stbinventory + stbmenu + stbsave (RPG) | UI and persistence | pending |

**No longer prerequisites** (compared to the embedded approach):
- ~~Metaprogramming ($T + reflection)~~ — `.bang` is parsed at runtime, not rewritten at compile time.
- ~~Multi-error log~~ — `.bang` parse errors are reported by the runtime, not the compiler.
- ~~Debugger breakpoints~~ — Scene debugging is via `say` and flag inspection, not step-through.
- ~~Profiler~~ — Scene logic is not performance-critical.
- ~~extrn + auto smart~~ — Scenes are data, not compiled globals.
- ~~Smart dispatch codegen~~ — No compile-time par block rewriting.

This means **bangscript can land earlier in the roadtrip** — potentially
right after the RPG Dungeon demo, before metaprog and the RTS.

---

## Runtime Modules (NEW — ~2230 Lines Total)

### `stbbangs.bsm` (~1500 lines) — The Complete bangscript Engine

Combines the `.bang` parser, coroutine scheduler, command dispatcher,
flag machine, special-variable publisher, and expression evaluator in
a single module. This is the heart of bangscript.

**LOC estimate revised 2026-05-26** based on AyumuScript study (~1200
LOC C++ machine code in `pac-ac/osakaOS/src/script.cc`). The original
~950 LOC estimate underestimated the expression evaluator + call
stack + special-variable resolver. Honest estimate after studying a
working reference implementation lands at ~1500 LOC. See "AyumuScript
as design reference" section below.

```
// Initialization and loading.
bangs_init()                     — Allocate scheduler arena, command arrays, var table.
bangs_load(dir_path)             — Parse all .bang files in a directory.
bangs_enter(scene_id)            — Activate a scene, load bg, register hotspots, reset scene-local vars.

// Game loop integration.
bangs_tick(dt, mx, my, click)    — Publish special vars, advance coroutines, check hotspots, dispatch verbs.
bangs_draw()                     — Render active scene (bg, actors, speech bubbles, verb bar).

// Callback registry (bridge to B++ game logic).
bangs_register(name, fn_ptr)     — Register a B++ function as a .bang callback.

// Special variable publishing (engine → script boundary).
bangs_publish(name, val)         — Publish a special variable for scripts to read via $name.
                                   Called every tick for $mouse_x/$mouse_y/$time_sec/etc.,
                                   and on demand for $player_x/$active_actor.

// State flags (persistent binary puzzle state).
bangs_flag_set(id, val)          — Set a flag.
bangs_flag_get(id)               — Read a flag.
bangs_save(path)                 — Serialize all flags + vars + current scene to file.
bangs_load_save(path)            — Restore saved state.

// Cutscene primitives (called from B++ callbacks).
bangs_say(actor, text)           — Show speech bubble.
bangs_walk(actor, x, y, speed)   — Move actor.
bangs_sound(path)                — Play sound effect.
bangs_music(path)                — Start music loop.

// Live REPL (dev-time iteration, AyumuScript-inspired).
bangs_repl_eval(line)            — Parse + execute one .bang line against the current scene.
                                   Used by Bang 9 / level_editor for interactive scene authoring.
```

Internally, the ~1500 LOC split across three architectural layers:

**Layer 2 — Interpreter machine (~680 LOC; directly portable from
AyumuScript reference):**
- Line parser state machine: ~80 LOC (char-by-char accumulation,
  newline trigger)
- Keyword detection + dispatch: ~100 LOC
- Argument tokenizer (`argparse` / `argcount` / `argshift`): ~60 LOC
- Var table lookup (uses `bpp_hash`): ~40 LOC
- Loop stack (fixed 16-level bounded): ~30 LOC
- Call stack (uses `arr_struct<CallStackUnit>`): ~80 LOC
- Expression evaluator (operator precedence, `+ - * / % == < > etc.`): ~150 LOC
- `numOrVar` / `strOrVar` type resolver: ~80 LOC
- Special vars dispatch (`$key_last`, `$mouse_x`, etc.): ~60 LOC

**Layer 3 — Bangscript domain commands (~320 LOC; designed fresh):**
- Scene commands (`scene`, `bg`, `actor`, `hotspot`, `exit`): ~70 LOC
- Cutscene commands (`cutscene`, `fade_in/out`, `cam_pan`, `wait`): ~60 LOC
- Actor commands (`say`, `walk`, `face`, `animate`): ~50 LOC
- Audio commands (`sound`, `music`): ~30 LOC
- State commands (`set`, `check`, `var`, `goto`, `label`): ~50 LOC
- Dialogue commands (`dialogue`, `choice`): ~30 LOC
- Behavior commands (`behavior`, `state`, `random` — Phase 1 vision): ~30 LOC

**New layers (~470 LOC; fresh design specific to bangscript):**
- Coroutine scheduler (fixed 64 slots, supports `par` / `spawn` /
  `loop`, frame-aware): ~150 LOC
- Scene graph + hotspot dispatch (mouse click → verb match → handler): ~240 LOC
- Special-variable publish API (engine → script boundary): ~50 LOC
- REPL eval helper (parse one line, dispatch): ~20 LOC
- Flag machine (binary persistent state): ~50 LOC

Total breakdown: 680 (Layer 2 machine) + 320 (Layer 3 commands) +
470 (coroutine + scene + special vars + REPL + flags) = **~1470 LOC
core**, rounded up to ~1500 with edge cases.

### `stbverb.bsm` (~200 lines) — Verb / Interaction System

SCUMM-style verb system. The player selects a verb (LOOK, USE, TALK,
PICK UP), then clicks a hotspot. The verb+target pair dispatches to
the scene's `on` handler.

```
verb_init()                      — Set up verb bar at bottom of screen.
verb_set_active(verb_id)         — Highlight a verb.
verb_tick(mx, my, click)         — Process input, return matched (verb, target) pair.
verb_draw()                      — Render verb bar + current selection text.
```

### `stbcursor.bsm` (~80 lines) — Cursor State Machine

Cursor mode management: walk, look, use, pick, talk. Each mode has a
different cursor sprite.

```
cursor_init(sprite_sheet)        — Load cursor sprites.
cursor_set_mode(mode_id)         — Change cursor appearance.
cursor_tick(mx, my)              — Update position.
cursor_draw()                    — Render at mouse position.
cursor_over_hotspot()            — Returns hotspot_id or -1.
```

### AyumuScript as design reference — Hybrid implementation strategy

bangscript's interpreter machine (Layer 2 above) is implemented via
**Hybrid Strategy** — locked 2026-05-26 after studying AyumuScript
(`pac-ac/osakaOS/src/script.cc`, ~1200 LOC C++):

- **Lift mechanical patterns from AyumuScript** where the implementation
  is universal (parser state machine, expression evaluator with operator
  precedence, special-variable resolver convention, loop stack
  management, call stack with arg passing).
- **Redesign substrate via B++ primitives** where AyumuScript's C++
  approach is unidiomatic. B++ has `bpp_hash` (replaces AyumuScript's
  hand-rolled hash table), `arr_struct<T>` (replaces linked-list `Node`
  containers), `bpp_str` packed refs (replaces `char*` + `mm->malloc`
  per string), `bpp_arena` for scope-bound allocation.

The resulting B++ code is shorter than literal C++ translation because
the prelude does the heavy lifting that AyumuScript reinvented inline.

**Specific patterns lifted from AyumuScript:**

| AyumuScript C++ | bangscript B++ port |
|---|---|
| `while (indexF < size) switch (file[indexF])` line accumulator | Direct port (~80 LOC) |
| `argparse` / `argcount` / `argshift` token helpers | Direct port (~60 LOC) |
| Loop stack `nestedLoop[16]` + `nestedLoopCondition[16]` | Direct port (~30 LOC) |
| `evalExpr()` operator-precedence walker | Direct port (~150 LOC) |
| `numOrVar()` / `strOrVar()` type resolver with `$` prefix | Direct port (~80 LOC) |
| `$0`–`$9`, `$F0`–`$F9`, `$R`, `$input` convention | Adopted (bangscript uses same `$` prefix with own var names) |

**Specific patterns redesigned for B++ idiom:**

| AyumuScript C++ | bangscript B++ rewrite |
|---|---|
| `Type*` union with manual tag (`type` field) + four-way switch | B++ tagged struct (Node-like AST node) |
| Custom hash table `varTable[VAR_TABLE_SIZE]` | `bpp_hash` (already proven, packed-name keys) |
| `List* lists` linked list of `Node*` | `arr_struct<T>` (cache-friendly, growable) |
| `cli->mm->malloc / free` per object | `bpp_arena` scope-bound (zero-cost reset on scene exit) |
| `char*` string buffers + manual length tracking | `bpp_str` packed refs (offset + length in one word) |
| Singleton `CommandLine* cli` global state | Per-scene state struct (cleaner ownership) |

**Patterns from AyumuScript NOT adopted in bangscript:**

- **`varList` family** (`insertList`, `removeList`, `readList`, `writeList`,
  `printList`, `destroyList`): list operations don't fit adventure-game
  use case. Defer to Phase 2+ if a future consumer surfaces real need.
- **`dvar` (delete variable)**: bangscript uses scene-local var lifetime
  (auto-zero on `bangs_enter`); explicit delete is unnecessary.
- **`@X` ASCII shortcut** (`@A` → 65): bangscript uses character literals
  in strings; ASCII access via `$key_last` integer is sufficient.
- **`inputStr` keyboard buffer**: bangscript handles input via special vars
  (`$key_last`, `$key_pressed`) updated per-tick by engine.

This Hybrid Strategy gives bangscript a **proven design oracle** for the
hardest parts (expression evaluator, parser state machine, variable
resolution) while using B++ primitives where they outperform C++'s
hand-rolled substrate. Net effect: ~30% smaller code than literal
translation, with same semantics.

### Historical validation — bangscript is the third generation

The strategic/tactical split that bangscript embodies (declarative data
for high-level behavior + engine code for low-level mechanics) is the
third iteration of a pattern Blizzard established in 1998 and BWAPI
researchers consolidated in the 2010s:

| Generation | Strategic layer | Tactical layer | Designer ergonomics |
|---|---|---|---|
| **Blizzard SC1 (1998)** | `aiscript.bin` (compiled bytecode), proprietary internal tooling | Hardcoded in game engine | Required Blizzard build environment, slow iteration |
| **BWAPI bots (2010+)** | Text config files (build orders, strategy selection) | Algorithm-based managers (UAlbertaBot, CommandCenter) | Edit config + recompile bot |
| **bangscript (2026)** | `.bang` text files with full expression evaluator + state machines | stbai managers + B++ game engine | Edit `.bang` + hot reload at runtime |

The historical validation matters because it confirms two architectural
decisions:

1. **The split is correct.** Strategic = data, tactical = engine has been
   validated for 28 years across at least two industry generations.
2. **Modern toolchain pays off.** Text format + hot reload + expression
   evaluator + multi-consumer runtime is what each generation gained
   over the previous one. bangscript is on the right side of that
   trajectory.

See `docs/plans/sidequest_bangbox.md` for the analogous architectural
pattern at the OS layer (Rule 41 additive portability).

### Multi-consumer validation — three distinct use cases

The bangscript runtime is **single design, multiple consumers**. The
parser + dispatcher + coroutine scheduler doesn't know which use case
it's running:

| Consumer | Granularity | Tick rate | Lifetime | Vocabulary |
|---|---|---|---|---|
| **Cutscenes** | Sequence of events (linear narrative) | Per-frame (60Hz) | Scene lifetime | `say`, `walk`, `fade_in`, `cam_pan`, `par` |
| **NPC behaviors** (Phase 1 vision) | FSM per-entity (micro-comportamento) | Per-frame (60Hz) | Entity lifetime | `state`, `walk`, `check`, `random`, `goto` |
| **RTS strategies** (Phase 1.5 — game-local AI brain consumer) | FSM per-player (macro-comportamento) | Strategic tick (5Hz) | Game lifetime | `state`, `train`, `construct`, `attack`, `scout`, `check` |

Three distinct consumers, one runtime. This is **Rule 36 (primitive
promotion when two consumers planned)** passing emphatically — not 2
hypothetical consumers, but 3 concrete consumers with overlapping
requirements that share machinery.

### Phase 1.5 — Strategic AI via `.bang` (~50-100 LOC game-local migration)

**Layer boundary clarification** (important; documented in the existing
`stb/stbai.bsm` header as of commit `52e2dab`):

- `stbai` cartridge = **genre-agnostic primitives only** (line-of-sight
  raycast, steering helpers, move-with-collision). The cartridge
  header explicitly excludes "state machines, per-frame dispatch,
  combat/target acquisition logic" — those stay in calling games.
- `rts1_ai.bsm` (game-local module) = **the RTS strategic brain**
  (build orders, economy FSM, attack waves, scouting state).
  Consumes `stbai` primitives where useful (LoS for scout vision,
  steering for unit movement) but owns all game-genre logic.

So the migration path is for the **game-local AI brain module**, not
for the stbai cartridge:

Discovered via rts1 S11 AI design (2026-05-26): build orders are
state-machine sequences naturally expressed in `.bang` syntax. **rts1_ai
V1** ships with hardcoded struct-array build orders (because bangscript
runtime is post-RPG); **rts1_ai V2** migrates to `.bang` files when
bangscript runtime is available. The same migration pattern would apply
to any future game-local AI brain module (rpg2_ai.bsm, tactics_ai.bsm,
etc.) that consumes the bangscript runtime.

Example `.bang` strategic AI (consumed by rts1_ai.bsm, not by stbai):

```bang
# games/rts1/ai/peon_rush_orc.bang
ai_strategy peon_rush_orc
  state opener
    on supply 4: train peon
    on supply 5: train peon
    on supply 6: construct barracks
    on supply 8: train peon
    on supply 9: train grunt
    check enemy_scouted -> response
    check barracks_done -> midgame

  state midgame
    train grunt
    train grunt
    check army_count >= 5 -> attack
    check enemy_attacking -> defend

  state attack
    rally_at enemy_hq
    attack
    check army_count < 3 -> defend
```

The strategic command set (`train`, `construct`, `attack`, `scout`,
`rally_at`, `defend`) is registered by `rts1_ai.bsm` as bangscript
callbacks via `bangs_register("train", fn_ptr(_rts1_ai_cmd_train))`.
Each handler lives in `rts1_ai.bsm` because the semantics are RTS-
genre-specific (a `train` command means something only inside an RTS).
`stbai` primitives (LoS, steering) are reused as helpers from within
those handlers.

See `games/rts1/rts1_handoff.md` Session 11 for the rts1_ai design.

### Shared Modules (Land Earlier with RPG / RTS)

| Module | Built during | Purpose for bangscript |
|--------|-------------|----------------------|
| `stbdialogue.bsm` | RPG | Dialogue box rendering, word wrap, choice selection |
| `stbinventory.bsm` | RPG | Item list, slot management, item icons |
| `stbmenu.bsm` | RPG | Nested menus, selection cursor |
| `stbsave.bsm` | RPG / RTS | Serialize flags + inventory + scene state |
| `stbaudio.bsm` | Rhythm Teacher | Music, SFX, voice during cutscenes |

---

## Bill of Materials

| Category | Lines | When |
|----------|-------|------|
| `stbbangs.bsm` (Layer 2 machine + Layer 3 commands + coroutine + scene + special vars + REPL) | ~1500 | After RPG ships |
| `stbverb.bsm` | ~200 | Together with stbbangs |
| `stbcursor.bsm` | ~80 | Together |
| Adventure demo game code (`.bpp` + `.bang` files) | ~500-800 | After all modules ship |
| **Total bangscript-specific** | **~2280-2580** | **~5-7 weeks** |

LOC trajectory across estimates:

| Date | Estimate | Source |
|---|---|---|
| Initial plan | ~700 stbbangs (~1500-1800 total) | Intuition |
| Post-AyumuScript additions (special vars, expressions, var/loop/spawn, REPL) | ~950 stbbangs (~1730-2030 total) | Plan revision 2026-05-25 |
| Post-AyumuScript study (Hybrid Strategy lock) | **~1500 stbbangs (~2280-2580 total)** | Reference impl validation 2026-05-26 |

The growth is not feature creep — it is **estimate honesty**. The first
two numbers underestimated the interpreter machine (expression evaluator,
call stack, special-variable resolver). Studying AyumuScript's ~1200 LOC
C++ implementation grounds the bangscript estimate against a working
reference. ~1500 LOC for the same machine + bangscript-specific layers
(coroutine, scene graph, hotspot dispatch) is honest.

Compare to the embedded approach: **~2300-2600 lines, ~4-6 weeks**.
The `.bang` format still saves the metaprog dependency entirely and
arrives at similar LOC with **dramatically better designer ergonomics**
(text format, hot reload, three-consumer runtime). The win is qualitative,
not quantitative.

---

## Dependency Diagram

```
[stbaudio, stbdialogue, stbinventory, stbmenu, stbsave]
    │          (all land before bangscript)
    ▼
┌────────────────────────────────────┐
│  stbbangs.bsm                      │
│  (.bang parser + coroutine sched   │
│   + command dispatcher + flags)    │
│  ~700 lines                        │
├────────────────────────────────────┤
│  stbverb.bsm + stbcursor.bsm     │
│  ~280 lines                        │
└────────┬───────────────────────────┘
         │
         ▼
┌────────────────────────────────────┐
│  Adventure Puzzle demo             │
│  game.bpp (~200 lines B++)         │
│  + scenes/*.bang (~300-600 lines)  │
└────────────────────────────────────┘
```

**No compiler dependency.** bangscript is pure runtime.

---

## Ordering in the 1.0 Roadtrip

With `.bang` decoupled from the compiler, bangscript can land earlier.
The new natural position is after RPG (which delivers the UI modules
bangscript needs) and before or alongside the RTS:

```
0.22 (current)
  ├─► Maestro batch 2
  ├─► Transitive .bo hash
  ├─► extrn + auto smart
  ├─► Dogfood sweep
  ├─► Smart dispatch codegen
  ├─► Dev loop 1-3 (multi-error, debugger, profiler)
  │
  ├─► Rhythm Teacher              ← stbaudio
  ├─► Wolf3D Phase 1 + Phase 2   ← CPU + GPU rendering
  │
  ├─► Dev loop 4: hot reload
  ├─► RPG Dungeon                 ← stbdialogue, stbinventory, stbmenu, stbsave
  │
  ├─► Dev loop 5: metaprog ($T + reflection)
  ├─► RTS demo                    ← scale + concurrency
  │
  ├─► bangscript (stbbangs + stbverb + stbcursor)
  └─► Adventure Puzzle ──────────► B++ 1.0   (the cherry on top)
```

Note: bangscript could also land BEFORE the RTS (right after RPG)
since it no longer depends on metaprog. The ordering is flexible.
Adventure still goes last as the cherry.

---

## Compiler Foundation: Function Dedup

(Relevant for the B++ game code, not for `.bang` files.)

### Fix 1 — `mod0_real_start` after auto-injection (DONE)

Set `mod0_real_start = outbuf_len` after the auto-injection block in
`bpp_import.bsm`. Fixed 115 spurious W015 warnings caused by module 0
re-lexing the entire outbuf.

### Fix 1b — Dedup refinement (DONE)

Same-module replacement: silent. Entry file overrides module: silent.
Cross-module both non-entry: W015.

### Fix 2 — `main()` in `.bsm` is an error (PENDING, ~5 lines)

### Fix 3 — `stub` keyword (PENDING, ~30 lines, before bangscript)

See previous revision of this document for full details on Fix 2-3.

---

## Critical Path — bangscript MVP

1. `stbbangs.bsm` with `.bang` parser + minimal scheduler (~400 lines)
2. One test scene: `tests/test_bangscript.bang` (10 lines)
3. One test program: `tests/test_bangscript.bpp` (~30 lines: init,
   load, enter, run loop)

This proves the full pipeline: `.bang` parsed → scene loaded →
hotspot clicked → `say` command executed → speech bubble rendered.
~430 lines total, testable without ANY other prerequisite.

---

## Open Questions

1. **Coroutine context allocation.** Arena per scene (reset on scene
   exit) or global arena with manual free? Arena per scene is simpler
   and prevents leaks.

2. **Maximum active coroutines.** Fixed 64 slots or dynamic array?
   64 is generous for adventure games (typical: 3-5 simultaneous
   actors in a cutscene). Fixed is simpler.

3. **Save format for flags.** Binary blob (fast, fragile) or
   key-value text (slow, debuggable)? Recommend binary for shipping,
   text for development (toggled by a flag).

4. **Verb bar layout.** Fixed 9-verb SCUMM layout or configurable?
   Start with fixed (GIVE, OPEN, CLOSE, PICK UP, LOOK AT, TALK TO,
   USE, PUSH, PULL), make configurable later if needed.

5. **Scene transition effects.** Fade-to-black is mandatory. Others
   (wipe, dissolve, iris) are nice-to-have for episode zero scope.

6. **Callback lookup.** By name string (hash lookup, ~O(1)) or by
   integer ID (array index, O(1))? Name string is more readable in
   `.bang` files. Hash lookup via `bpp_hash` is already available.

7. **`.bang` file discovery.** Load all `.bang` files in a directory
   (`bangs_load("scenes/")`) or explicit manifest? Directory scan is
   simpler and matches the "drop a file, it works" workflow.

8. **`var` scoping rules** (AyumuScript addition). Three options:
   - **Scene-local** (default): vars reset on `bangs_enter`. Simple,
     prevents leaks across scenes. Limits cross-scene state to flags.
   - **Function-local** (when functions ship in Phase 1): vars
     declared inside `function ... end` are scope-bound, separate
     from scene vars. Mirrors AyumuScript's call stack.
   - **Global** (opt-in via `global $name` syntax): a small set of
     truly persistent vars (`$money`, `$party_size`). Persists with
     flags in saves.

   Recommend: scene-local default, function-local for Phase 1,
   `global` opt-in via `set` extension (`set $money 100` to declare
   a persistent numeric — flags can already do this if we generalise
   them to int values).

9. **Special variable refresh cadence**. Every-tick vars (`$mouse_x`,
   `$time_sec`) cost ~30 ns/tick of write traffic per var (~10 vars
   = ~300 ns/tick total — negligible). On-demand vars
   (`$current_scene`, `$active_actor`) only re-publish on change.
   Recommend: publish-every-tick for input + time, publish-on-change
   for scene state, both via the same `bangs_publish(name, val)` API.

10. **REPL eval scope**. The Bang 9 / level_editor REPL evaluates one
    `.bang` line at a time against the current scene. Open: does the
    REPL share `var` state with the running scene, or have its own
    isolated scratch scope? Recommend shared — feels like a debugger
    where you can poke at scene state, not a sandbox.

---

## Post-1.0 Vision: `.bang` as General Behavior Authoring

The coroutine scheduler does not know whether it is running a cutscene
or an AI behavior. Both are sequences of commands executed frame by
frame, with conditions, states, and transitions. This means `.bang`
is not an adventure game format — it is a **behavior authoring format**
that starts with adventure games and grows.

### Phase 1 — AI behaviors via `.bang` (~250 lines engine work)

The `.bang` format gains three additions:

- `behavior` — root block for NPC state machines (like `scene` for rooms).
- `state` — named sub-block with commands and transitions.
- `random` — weighted probabilistic choice.
- `check condition -> state` — conditional transition (extends existing `goto`).

Example:

```bang
# ai/guard.bang

behavior guard
  state idle
    wait 2s
    check player_nearby -> alert
    random
      70 -> patrol
      30 -> idle

  state patrol
    walk guard waypoint_next 3s
    check player_nearby -> alert
    check patrol_done -> idle

  state alert
    say guard "Who's there?"
    face guard player
    wait 500ms
    check player_visible -> attack
    goto search

  state search
    walk guard last_known_pos 3s
    wait 2s
    say guard "Must have been my imagination."
    goto patrol

  state attack
    call guard_combat
```

The parser adds ~100 lines (new block types). The dispatcher adds
~150 lines (state machine tick logic). Each behavior runs as a
coroutine in the same scheduler that runs cutscenes — a guard on
patrol and Indy in a cutscene share the same 64-slot pool.

An NPC in the RPG dungeon or Adventure demo can reference a `.bang`
behavior instead of having its AI hardcoded in B++. The designer
tweaks numbers in the `.bang` file; the programmer only touches B++
when custom logic is needed (via `call`).

### Phase 2 — `stbai.bsm` (~400-600 lines)

A dedicated AI module that reads `.bang` behaviors and adds:

- **Utility scoring.** Each action gets a desirability score based on
  the NPC's current state. The highest-scoring action wins. Replaces
  the simple `random` / `check` branching with something that feels
  alive.

- **Per-NPC memory.** Each NPC remembers: last player position, who
  they talked to, what items they saw. Memory decays over time.
  Enables "the guard remembers you broke in yesterday" behavior.

- **Personality traits.** Defined in `.bang` data files:

  ```bang
  # data/personalities.bang

  trait brave
    fight_threshold 0.3
    flee_threshold 0.1

  trait coward
    fight_threshold 0.8
    flee_threshold 0.5

  trait loyal
    defend_ally_weight 90
    betray_weight 1
  ```

  Traits affect decision weights in the utility scorer. A `brave`
  guard attacks at 30% health; a `coward` guard flees at 50%. The
  writer defines personalities; the engine does the math.

- **Group behavior.** NPCs aware of each other: "if ally is in
  combat, help" or "if leader flees, followers flee". Uses the
  existing ECS (`stbecs.bsm`) for spatial queries.

### Phase 3 — Procedural content via `.bang` templates

Inspired by Dwarf Fortress raws and Warsim's kingdom generation.
`.bang` files define templates with variation slots:

```bang
# templates/tavern_quest.bang

template tavern_quest
  npc innkeeper
    personality friendly helpful
    say innkeeper "There's trouble in {location}..."
  quest clear_location
    objective kill {enemy_type} {count}
    reward gold {reward_amount}
  variables
    location: basement, cellar, mine, forest_cave
    enemy_type: rat, spider, goblin
    count: 3, 5, 8
    reward_amount: 30, 50, 100
```

```bang
# templates/creature.bang

creature rat
  health 3
  speed 2
  behavior wander
  on_death
    sound "squeak.wav"
    drop item_tail 30

creature guard
  health 20
  speed 4
  behavior guard
  personality brave loyal
  on_death
    say guard "For the king..."
    call on_guard_death
```

The engine instantiates from templates with weighted random selection.
This is the Dwarf Fortress model: the richness comes from
**combinatorial explosion of simple rules**, not from hand-authored
complexity.

### Phase 4 — Neural / learned behaviors (speculative)

The furthest horizon. Instead of hand-authored state machines, NPCs
learn behaviors through simulation:

- Train offline: run thousands of simulated encounters with reward
  signals (survive, complete objective, maintain loyalty).
- Export learned weights to a `.bang`-compatible format (a decision
  table with scores per state×action pair).
- The runtime reads the table and makes decisions — still just a
  switch statement, but the weights came from training, not design.

This is NOT a neural network running at runtime. It is a
**pre-trained decision table** baked into data that the existing
`.bang` dispatcher reads. The runtime cost is one table lookup per
NPC per frame — the same as a hand-authored state machine.

Whether this is worth building depends on whether the games B++ makes
are complex enough to need it. A 1-level adventure demo does not. A
Dwarf Fortress-scale simulation does. The path exists.

### Why `.bang` scales to all of this

The key insight: `.bang` is not an adventure game format. It is a
**declarative behavior description** that a B++ runtime interprets.
Scene definitions, cutscenes, AI behaviors, creature stats, personality
profiles, quest templates, world generation rules — all are the same
thing: structured data with optional command sequences.

The difference between Monkey Island and Dwarf Fortress is not the
data format — it is the complexity of the engine that interprets it.
The 1.0 engine handles scenes and cutscenes. Post-1.0, the engine
grows to handle behaviors, AI, and procedural generation. But the
format is the same, the parser is the same, and the scheduler is the
same.

This is exactly what Sierra did with SCI: started with King's Quest
(simple adventure), evolved to Gabriel Knight (complex adventure with
cinematic AI), all on the same engine, all in the same script format.
The format grew with the engine.
