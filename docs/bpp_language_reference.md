# B++ Language Reference — Keywords & Annotations

## Storage (top-level variables)

| Keyword | What it does |
|---------|-------------|
| `auto` | Default — serial, or smart-promoted to the best storage class by the compiler. If the compiler can prove the variable is written once before `maestro_run` and never after, it promotes to `extrn`. If workers touch it, promotes to `global`. Otherwise stays serial. The programmer doesn't need to think about this. |
| `global` | Worker-shared mutable. The variable can be read and written from worker threads (base phase). The programmer takes responsibility for stride safety. Use for game state that the maestro pattern dispatches over. |
| `extrn` | Frozen after init. Mutable before `maestro_run()` / `job_init()`, immutable forever after. Reads from any worker are safe because the memory never changes. Use for asset tables, config data, lookup tables loaded once at startup. |
| `const` | Compile-time literal. `const N = 10;` inlines the value at every use site. No runtime storage allocated. |

### Smart promotion (the B++ way)

The programmer can start with `auto` for everything. As the project grows and the maestro pattern is introduced, the compiler automatically promotes variables to the right class:

```bpp
// Level 1: just write code
auto world;
auto assets;
auto counter;

// Level 2: the compiler tells you what it did
// $ bpp --show-promotions game.bpp
//   assets -> extrn    (written once in main before maestro_run)
//   world  -> global   (read by base-phase worker function)
//   counter: auto      (no promotion — mutated in loop)

// Level 3: be explicit when you know better
extrn assets;          // "I know this is frozen"
global world;          // "I know workers touch this"
auto counter: serial;  // "don't promote, I want serial"
```

---

## Visibility (default: public)

| Keyword | What it does |
|---------|-------------|
| *(nothing)* | **Public** — any module can call this function or access this variable. This is the default. No keyword needed. |
| `static` | **Private to this module.** Only code in the same `.bsm` file can call a `static` function or access a `static` variable. Cross-module access is a compile-time error. |

```bpp
// wolf3d_ray.bsm
cast(x, y, angle) { ... }          // public — any module calls this
static _intersect(wall) { ... }    // private — only ray.bsm calls this

static extrn _tables;              // private + frozen
global ray_result;                  // public + worker-shared
```

---

## Return (default: implicit return 0)

| Keyword | What it does |
|---------|-------------|
| *(omit return)* | **Implicit return 0.** If the function body doesn't end with a `return` statement, the compiler inserts `return 0;` automatically. No boilerplate needed for side-effect functions like `init_game()`, `draw_hud()`, `apply_damage()`. |
| `void` | **Explicit "no useful return."** Marks a function as never returning a useful value. The compiler warns (W017) if a `void` function's return value is used: `x = draw_hud();` |

```bpp
// Level 1: just omit return — compiler handles it
draw_hud() {
    render_text("SCORE", 4, 4, 1, WHITE);
}

// Level 2: be explicit — compiler catches mistakes
void draw_hud() {
    render_text("SCORE", 4, 4, 1, WHITE);
}
auto x;
x = draw_hud();    // warning[W017]: 'draw_hud' is void

// Functions that return values: use return normally
add(a, b) {
    return a + b;   // explicit return — value is meaningful
}
```

---

## Override

| Keyword | What it does |
|---------|-------------|
| `stub` | **Placeholder function.** Declares a function that is meant to be overridden by the entry file (.bpp). The override is always silent — no duplicate warning. Used by engine modules (stbgame callbacks) and will be used by bangscript for generated scene functions. |

```bpp
// stbgame.bsm — engine provides stubs
stub game_init() { return 0; }
stub game_update(dt) { return 0; }

// mygame.bpp — programmer provides real implementations
game_init() {
    world = ecs_new(200);    // overrides stub silently
}
```

---

## Module

| Keyword | What it does |
|---------|-------------|
| `import` | **Engine/library module.** Searches `stb/`, `src/`, installed paths, then `/usr/local/lib/bpp/`. Use for standard library and engine modules. Visibility rules (static) are NOT enforced across imported modules — engine internals remain accessible. |
| `load` | **Project file.** Searches ONLY the entry file's directory. Use for your game's own modules. Visibility rules (static) ARE enforced between loaded modules. If the file is not found locally, it's a fatal error — no fallback to engine paths. |

```bpp
// wolf3d.bpp
load "wolf3d_ray.bsm";     // my project file — strict rules
load "wolf3d_enemy.bsm";   // my project file — strict rules
import "stbgame.bsm";       // engine — permissive rules
import "stbecs.bsm";        // engine — permissive rules
```

---

## Type

| Keyword | What it does |
|---------|-------------|
| `auto` | Default type is **word** (64-bit integer). B++ is typeless on top — all values are 64-bit words unless annotated with a slice hint. |
| `struct` | Define a named struct with named fields. Access fields with dot syntax: `player.x`. |
| `enum` | Define named constants: `enum { RED, GREEN, BLUE }`. |

---

## Control Flow

| Keyword | What it does |
|---------|-------------|
| `if` / `else` | Conditional execution. |
| `for` | C-style for loop: `for (init; cond; step) { body }`. |
| `while` | While loop: `while (cond) { body }`. |
| `break` | Exit the innermost loop immediately. |
| `continue` | Skip to the next iteration. In `for` loops, runs the step expression before re-testing the condition (C semantics). |
| `return` | Return a value from the current function. |

---

## Annotations (`:` syntax)

Annotations use the existing `:` syntax after a variable name or function parameters. They are NOT keywords — they are hints that give the compiler more information or override automatic decisions.

### Type/size annotations (after variable name)

| Annotation | What it does |
|------------|-------------|
| `: byte` | 8-bit unsigned integer (0-255) |
| `: half` | 32-bit integer |
| `: quarter` | 16-bit integer |
| `: float` | 64-bit double |
| `: half float` | 32-bit float |
| `: quarter float` | 16-bit float |
| `: StructName` | Typed struct access (enables dot syntax) |

```bpp
auto hp: byte;              // 8-bit health points
auto velocity: half float;  // 32-bit float velocity
auto player: Player;        // typed struct with dot access
```

### Storage annotation (after top-level variable name)

| Annotation | What it does |
|------------|-------------|
| `: serial` | Prevent auto promotion. The variable stays `auto` (serial) even if the compiler thinks it could be promoted to `extrn` or `global`. Escape hatch for intentionally sequential variables. |

```bpp
auto debug_counter: serial;  // "I know what I'm doing, don't promote"
```

### Phase annotations (after function parameters)

| Annotation | What it does |
|------------|-------------|
| `: base` | Assert that this function is pure (no side effects). The compiler verifies against its own analysis — if the function actually has side effects, it emits warning W013. Worker-safe: can be dispatched to a worker thread. |
| `: solo` | Assert that this function must run sequentially on the main thread. The compiler respects this even if the function looks pure. Programmer override for ordering-sensitive code. |

```bpp
// Compiler auto-classifies (no annotation needed):
helper(x) { return x * 2; }         // auto-classified as base (pure)

// Programmer asserts and compiler verifies:
particle_update(i): base {           // W013 if not actually pure
    ecs_set_pos(world, i, new_x);
}

// Programmer forces sequential:
critical_section(): solo {           // always sequential, no auto-dispatch
    update_global_state();
}
```

---

## Design Principles

**Public by default.** Write code, it works. Add `static` when the project grows and you need module boundaries.

**Implicit return.** No boilerplate `return 0;`. Add `void` when you want the compiler to catch "used void as value" bugs.

**Auto everything.** Auto storage class, auto phase classification, auto type inference. Add explicit keywords (`extrn`, `global`, `: base`, `: solo`) when you know better or want compiler verification.

**Simple for simple things.** A 50-line snake game and a 10K-line RTS use the same language, just different levels of annotation. The language scales with the project — you never have to learn everything upfront.

```
Level 1 (beginner):      auto x;           foo() { code; }
Level 2 (growing):       extrn assets;     void draw() { code; }
Level 3 (expert):        global world;     update(): base { code; }
```
