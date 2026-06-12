# B++ language evolution for 3D — the fps2-driven compiler arc

**What this is.** The Quake-class engine (`games/fps2/`) is the next
higher-complexity game, and — exactly as the RTS arc grew the compiler
(type hints, the `arr_struct` AoS migration, smart-dispatch) — it should drive a
deliberate, *domain-shaped* evolution of the B++ language. This doc separates
the wheat from the chaff: which features a 3D engine genuinely earns, the modern
language to learn each from, the existing B++ foothold each builds on, and the
sequence.

## The principle (and what it is NOT)

A harder game is the right driver of compiler evolution. But the growth is
**domain features earned by a real consumer**, gated by Rule 20 (two consumers)
and Rule 28 (restraint) — **not** C feature-parity. Two distinct things that are
easy to conflate:

- **Do NOT chase the "C has it, B++ doesn't" list.** `goto`, the preprocessor,
  and `switch` fallthrough are deliberate absences where B++ is simpler/safer;
  `union`/varargs/asm are already covered better (explicit reinterpret /
  smart-dispatch / SIMD). Adding these back is the Multics-drift the 8-phase
  lattice taught (`feedback_phase_overengineering_lesson`). See
  `games/fps2/HANDOFF.md` Decision 2.
- **DO pursue the modern domain features below.** They are not on the C-gap
  list — they come from games/graphics/data-oriented design, and the modern
  systems languages (Jai, Zig, Odin) converge on them precisely because they
  target this domain. Each lands when a Quake idiom demands it *twice*.

## The features, prioritized

### HIGH — transform 3D code

**1. Built-in vector / matrix math.** `vec3 + vec3`, component-wise ops, swizzle
(`v.xyz`). The single biggest ergonomic win for graphics code.
- **Learn from: Odin** — `[3]f32` with component-wise arithmetic + swizzles is
  the tasteful gold standard (also GLSL/HLSL).
- **Design discipline:** *built-in* vector/matrix types with component-wise
  arithmetic — **NOT** general user-defined operator overloading (the C++ road).
  Built-in types keep B++'s simplicity and weak/optional-typing identity while
  delivering the graphics ergonomics.
- **B++ foothold:** SIMD `vec_*` / autovec already exist underneath; this is the
  surface syntax over them.
- **Consumer:** `stbmath3d` — the first cartridge the engine needs. Start here.

**2. Generics via monomorphization (= Excalibur Feature 4, deferred).**
`vec<T>`, `mat<T>`, typed vertex/index arrays.
- **Learn from: Zig `comptime`** — the cleanest model: a generic is a function
  that takes *types* as compile-time parameters and monomorphizes. Do **not**
  copy C++ templates (their complexity is the cautionary tale).
- **B++ foothold:** Excalibur F4 is already designed (`$T` syntax + per-call
  monomorphization). Quake is plausibly the **second named consumer** (with the
  vector-math containers) that opens the arc per Rule 20.

### MEDIUM — real enablers

**3. Compile-time execution.** Bake lookup tables, lightmaps, BSP preprocessing
at build time instead of at runtime.
- **Learn from: Jai `#run` + Zig `comptime`.**
- **B++ foothold:** `const`-fold + `const_eval_node` already do limited
  compile-time evaluation — the seed of a fuller comptime.

**4. SOA / AOS as a language concern.** Struct-of-arrays layout for vertex,
particle, and SIMD-hot data.
- **Learn from: Jai (built-in SOA) / Zig MultiArrayList.**
- **B++ foothold:** the `arr_struct` **AoS** migration is already done; SOA is
  the natural complement, and a rasterizer / particle system is the canonical
  SOA consumer.

### LOW — cheap, minor

**5. `typedef` / type-alias.** Parser-level name binding to a type.
- Honest assessment: least impactful of the set. Named structs (`struct Vec3
  {x,y,z}`) already give named types; once (1) lands, the alias adds little.
  Add it if readability genuinely bites — it is trivial — but it does not move
  the needle the way (1)/(2) do.

## Sequence

Engine and compiler co-evolve, each feature pulled by a concrete Quake idiom —
not all at once (that would be the drift):

1. **Built-in vector/matrix (1)** — `stbmath3d` is the immediate consumer; the
   biggest ergonomic unlock, and every downstream 3D system depends on it.
2. **Generics (2)** — when typed containers (vertices, matrices) genuinely hurt;
   opens Excalibur with Quake as the second consumer.
3. **Comptime (3) + SOA (4)** — when lightmap/BSP baking and the hot vertex
   buffers arrive.
4. **typedef (5)** — anytime, if it itches.

Each earns its place by an idiom appearing **twice** in the engine, not by "Zig
has it". The discipline is unchanged; the direction is to walk — toward this
domain wheat, not C-parity.

## What we are NOT taking from the modern languages

To keep the lesson honest: borrow the *technique*, not the whole language.
- From **C++**: operator overloading for math is tempting but its general,
  user-definable form is the complexity trap — take built-in vector types
  instead. Templates → take Zig's comptime model, not C++'s.
- From **Jai/Zig/Odin**: their allocator/context systems, error-handling models,
  and build systems are out of scope — B++ already has manual/arena memory and
  its own toolchain. Take comptime, SOA, and vector math; leave the rest.

## Relationship to fps2

This is the **compiler/language track** that runs alongside the `games/fps2/`
**engine track**. The engine surfaces the need; the language feature lands; the
engine adopts it — the same way the RTS and its cartridges co-evolved. The fps2
port plan (to be written) references this doc for the language work each phase
assumes.
