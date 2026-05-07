# Tier F — Heavy Compiler Optimization Passes (Roadmap)

## Context

B++'s compiler is a tree-walker with no IR. Waves 18-21 moved the
tree walker and statement dispatcher into the spine (`bpp_codegen.bsm`),
unblocking optimizations that previously had to be duplicated per chip
or weren't feasible at all.

Phases D-1 through D-4 (2026-04-22) landed the **single-node**
optimizations that fit the tree-walker model: strength reduction,
identity peephole, inline trivials for `buf_new` / `buf_new_w`, and
infrastructure cleanup (buf_fill / buf_copy / read_u*be across stb).

Tier F collects the **multi-node** optimizations — passes that require
analyzing relationships BETWEEN AST nodes rather than rewriting a
single binop at parse time. These are genuinely large projects; this
document scopes them honestly so the roadmap is visible without
pretending they fit into a single session.

## Why Tier F Is Deferred (not skipped)

The user feedback on the 2026-04-22 plan was explicit: "Tier F needs
profile data before commitment." Without a plugin showing scalar math
as its bottleneck, Tier F optimizations are speculation. Once a real
workload points at one specifically, its project opens.

The plan below is the roadmap for THAT moment, not for today.

## F.1 — CSE (Common Subexpression Elimination)

### What it does

`(a + b) * (a + b)` computes `a + b` once, reuses the result. Applies
to any expression tree where a subexpression appears more than once
and its value doesn't change between occurrences (pure expressions
of non-mutating variables).

### Why it's hard in B++

Tree-walker compilers normally walk each occurrence independently. To
CSE, we need:

1. **Structural hash of AST subtrees** — two T_BINOP nodes with same
   operator, same LHS, same RHS hash identically regardless of memory
   location. Need a stable hash that ignores node pointers, hashes
   over semantic content (operator, operand hashes, recursive).
2. **Value numbering per scope** — map from hash → first-occurrence
   offset. When a repeat is detected, emit a load from the cached
   slot instead of re-evaluating.
3. **Escape analysis** — a subexpression is CSE-safe only if none of
   its inputs are mutated between occurrences. For B++ that means:
   - No assignment to any variable in the subexpression
   - No call between occurrences that could mutate via aliasing
4. **Scratch slot management** — where does the cached value live?
   - Register if free (complicates register allocator)
   - Stack slot if no reg (requires frame-size bookkeeping)
   - Need to choose before emission of the FIRST occurrence

### Scope estimate

- Hash function + caching: 80 lines
- Scope tracking + value numbering: 150 lines
- Mutation detection: 200 lines (needs AST walker that tracks writes)
- Emission coordination (scratch slots, reg choice): 150 lines
- Testing + debugging edge cases: hard to estimate — CSE is notorious
  for correctness bugs in aliased structs, nested calls, etc.

**Total: 2-3 weeks.**

### Prerequisite

- Wave 21 real takeover (DONE — `cg_emit_node` owns the walker)
- AST node augmentation: each Node needs a field for "CSE slot index"
  (or a side table keyed by node pointer)

### When to unblock

- Profile of a real plugin shows scalar-arithmetic hot path
- OR: game code audit finds common patterns of
  `sqrt((x-cx)^2 + (y-cy)^2) ...` or similar repeat-heavy patterns

### Expected gain

- 5-15% on math-heavy code (physics, geometry)
- 0% on I/O- or allocation-bound code

## F.2 — Register Allocator v2 (SSA-lite + liveness)

### What it does

Replace the current B3 "count refs + promote hottest N" heuristic
with a **live-interval tracker** that assigns registers based on when
variables are alive across basic blocks, not just total reference count.

### Why it's hard in B++

B++ today has:
- Single-pass walker with no block structure explicit in AST
- B3 does a pre-pass to rank variables, then promotes up to N of them
- Spills happen ad-hoc when a call sequence needs specific registers

A v2 allocator needs:

1. **Explicit CFG** — basic blocks with edges. Currently B++ walks
   T_IF / T_WHILE / T_SWITCH recursively; converting to a flat CFG is
   a significant AST-to-CFG pass.
2. **Liveness analysis** — for each basic block, which variables are
   live-in, live-out. Classic dataflow algorithm (iterate until fixed
   point).
3. **Live intervals** — flatten per-variable liveness into time
   ranges over the linear emission order.
4. **Linear-scan allocation** (Brooks algorithm) — walk intervals,
   assign registers, spill when out of free regs.
5. **Coalescing** — avoid copy moves between variables that don't
   live simultaneously.

### Scope estimate

- AST-to-CFG conversion: 300 lines
- Liveness analysis: 150 lines
- Live-interval construction: 100 lines
- Linear-scan allocator: 200 lines
- Spill code emission rewrite: 150 lines
- Integration with existing chip primitives: 100 lines
- Testing against existing code (MUST produce identical semantics, not
  necessarily identical bytes): extensive

**Total: 3-4 weeks.**

### Prerequisite

- Wave 21 DONE
- F.1 CSE nice but not required
- AST node augmentation: variable references need stable IDs, not
  just name-packed strings

### When to unblock

- Profile shows stack traffic (spill/reload) as a significant fraction
  of hot-loop time
- OR: a specific workload (RTS with 1000 units, dense ECS) demands
  tighter register reuse

### Expected gain

- 10-25% on register-pressure-heavy code (physics, transforms)
- 5-10% on typical game code
- Negligible on I/O-bound or trivially-allocated code

## F.3 — Auto-Vectorization

### What it does

Detect loops whose body operates independently on sequential array
elements and emit SIMD operations (NEON 128-bit on aarch64, SSE2/AVX
on x86_64) that process 4 elements at once.

### Why it's hard (PhD-level)

1. **Dependency analysis** — a loop body may or may not have
   cross-iteration dependencies. `arr[i] = arr[i-1] + 1` can't be
   vectorized (each element depends on previous). `arr[i] = arr[i] *
   2` can. Detection requires SSA-style analysis or explicit
   dependency graph.
2. **Alignment reasoning** — SIMD loads prefer 16-byte-aligned
   addresses. B++ buffers aren't guaranteed aligned. Need either:
   - Proof that the buffer is aligned (analysis)
   - Runtime alignment prologue (process first few elements scalar
     until aligned)
   - Use unaligned SIMD loads (slower but always correct)
3. **Vectorizable operation set** — not every scalar op has a SIMD
   equivalent. Branches, function calls, type conversions need
   fallback to scalar.
4. **Epilogue for remainder** — if loop count isn't a multiple of 4,
   the last 0-3 elements need scalar handling.
5. **Chip-specific codegen** — NEON and SSE2 have different
   instruction shapes. Needs a mini-vectorization-IR that both chips
   can consume.

### Scope estimate

Honestly: **2-3 months full-time.**

### Prerequisite

- F.1 CSE + F.2 Reg allocator v2
- `vec_*` primitives already present (NEON + SSE2 families)
- AST loop detection working
- Profile data showing scalar inner loops as the bottleneck

### When to unblock

The only realistic trigger: a real-time DSP plugin (convolution reverb,
FFT-based EQ, physical modeling synth) where the user DEMONSTRATES
that scalar B++ code is 4× slower than a hand-SIMD'd equivalent. Even
then, the escape hatch of writing `vec_*` intrinsics by hand
(currently available) is usually more efficient per-developer-hour than
making the compiler auto-vectorize.

### Expected gain

- 2-4× on numerically-dense inner loops that fit the model
- 0× on control flow, allocation, I/O
- Negative (code bloat) if applied to loops that don't meet the
  criteria

## Recommended Gating

**Do not open Tier F until:**

1. A real plugin exists, runs, and is profiled.
2. The profile shows one of:
   - Common-subexpression waste (F.1 territory)
   - Register spill/reload time (F.2 territory)
   - Scalar-inner-loop dominance (F.3 territory)
3. Someone owns the resulting project end-to-end (design → impl →
   test → integration).

Until those three conditions exist, Tier F is speculation. The
Phase D work (buf_fill dogfood, D.4 inline trivials, D.1 strength
reduction) already captures the wins that don't need profiling to
justify. That's the correct place to stop without profile data.

## Related Work

- **Wave 22 candidate: D.4 expansion / E.3 non-trivial wrappers** — if
  Tier F is too big, the smaller next step is expanding D.4 to
  handle functions with locals (via alpha-renaming during AST
  substitution). Wraps `arr_get`, `str_len` style functions. Medium
  project (~2 weeks), big win on wrapper-heavy code.
- **Wave 23 candidate: tail-call optimization** — specialized case of
  no-arg re-call. Useful for recursive descent parsers and functional
  style, not games. Small project (~1 week). Parser first, games
  second.
- **Wave 24 candidate: profile-guided optimization** — instrument
  hot paths, emit feedback, re-compile with hints. Much further down
  the roadmap.

## Updated 2026-04-22

- Phase D-1 through D-4 shipped (bootstrap stable at `72e1b793`,
  suite 111/0 non-GPU).
- Tier F remains explicitly deferred per this document's own gating.
