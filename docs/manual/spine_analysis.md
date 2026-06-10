# Spine vs. C-- — is the b++ codegen spine a "portable assembly"?

**What this is.** A design analysis triggered by a question: in the Simon
Peyton Jones interview (*Co-Creator of Haskell*, youtu.be/xcB_LF3cdqw) he
describes **C-- / Cmm** — the portable assembly language GHC uses to lower
lambda calculus onto many machine backends. Is the b++ codegen *spine*
(`src/bpp_codegen.bsm`) the same thing? And *should* it become a real C--, or
does being an imperative language exempt us?

Conclusion up front: the spine fills the **same architectural role** C-- fills
(the narrow waist between one front-end and N machine backends), but it is a
*higher-level* IR than C--, and the half of C-- that was its research
contribution — a portable **runtime interface** for garbage-collected / lazy
functional languages — does not apply to us. "Becoming a real C--" decomposes
into three independent pieces with three different answers.

---

## Part A — What the spine actually is (the evidence)

`src/bpp_codegen.bsm` self-describes as:

> *"portable codegen logic shared across chip backends … pure AST-level /
> register-allocation / ABI-layout / tree-walking code — no chip-specific
> instruction emission. Each chip backend (a64 / x64 / future riscv / wasm)
> imports this file and implements a small set of `chip_emit_*` primitives."*

Key facts established by reading the source:

- **A fixed 30-slot `chip_primitives` contract.** A Forth-style minimal
  instruction table (add/sub/mul/div, and/or/xor/shift, load/store, branch,
  call/ret, frame setup/teardown, syscall, plus ABI helpers like
  `chip_arg_reg`, `chip_return_reg`, `chip_sp_reg`, `chip_fp_reg`). The spine
  runs the portable passes and calls the active chip through this interface.
  Concrete backends: `backend/chip/aarch64/`, `backend/chip/x86_64/`, and the
  C emitter `backend/c/bpp_emitter.bsm`; `riscv` / `wasm` reserved.
- **Tree-walking, accumulator register model.** The header for the BINOP path
  reads *"result→reg 0, RHS already in reg 0; LHS in chip's left register"* —
  classic expression-tree evaluation with a small fixed register discipline
  and a per-frame **spill** area (`fn_reserve_spill`, `arg_reg_int/-flt → -1 if
  spills`). There is **no** basic-block graph, **no** CFG, **no** SSA, **no**
  liveness/interference graph anywhere in the spine (grep: zero hits).

So the spine is a **portable tree-walking code generator** with a minimal
per-chip instruction interface. It is decidedly *not* an optimizing middle-end
in the Cmm / LLVM-IR sense.

---

## Part B — Where the C-- analogy holds (and it genuinely does)

| Concept | C-- / Cmm (GHC) | b++ spine |
|---|---|---|
| Portable middle layer | C-- | `bpp_codegen.bsm` |
| Front-end retargets once, fans out to N backends | native NCG, LLVM, unregisterised C | a64, x64, C emitter (+ riscv/wasm reserved) |
| Machine detail pushed *below* the waist | instruction selection / ABI per target | the 30 `chip_emit_*` primitives |
| "Implement the machine interface, get the passes for free" | C--'s core proposition | "spine dispatches, chip implements" |

This is the same engineering instinct SPJ describes: don't rewrite codegen per
machine — lower to one portable target and have each backend implement a small
vocabulary. The 30-primitive minimal table echoes C--'s minimalist
abstract-machine design.

---

## Part C — Where the analogy breaks

1. **The spine is a *higher-level* IR than C--.** The spine tree-walks the b++
   AST straight to chip primitives. Cmm sits *below* the AST: GHC has already
   lowered AST → Core → STG → **Cmm**, so Cmm is a flat control-flow graph of
   basic blocks, gotos, and abstract-machine registers — not a tree. b++ skips
   the flat-IR / CFG stage entirely. In IR-level terms the spine is closer to a
   *portable macro-assembler* than to Cmm or LLVM IR.

2. **C--'s runtime-interface raison d'être does not apply.** C--'s research
   contribution was making the *runtime system* portable: GC stack maps,
   exception unwinding, tail calls, custom calling conventions — the expensive
   part of compiling a lazy, garbage-collected functional language. b++ is
   imperative with manual / arena memory: no GC, no closures, no thunks, no
   laziness, errors via `E###`/`diag` codes (no unwinding). That entire half of
   C-- is inert for us.

3. **No lambda calculus.** GHC's heavy lifting (laziness → graph reduction,
   thunks, the STG machine) happens *above* Cmm. b++ has no such pipeline; it
   compiles imperative code directly. The "port lambda calculus to many
   backends" framing is exactly what b++ does **not** need to do.

**One-line positioning:** the spine is *C-- minus the hard part* — a
retargetable waist at a higher IR level, without the functional-runtime
contract that justified C--'s existence. Of SPJ's two axes — (a) retargetable
backend and (b) portable runtime for lambda calculus — the spine covers (a) and
ignores (b), because b++ is not a functional language.

---

## Part D — "Becoming a real C--" decomposes into three pieces

C-- bundles three separable things. They have *different* answers for an
imperative b++.

### (A) A flat, optimizable IR — basic blocks, virtual registers, ideally SSA

The substrate for serious global optimization. **What it would take:**

1. A normalized IR data structure (basic blocks + instructions + virtual regs)
   — a new layer between AST and chip.
2. An AST → IR lowering pass.
3. SSA construction (phi nodes) — prerequisite for the good optimizations.
4. A dataflow framework + the passes (DCE, global GVN/CSE, sparse conditional
   const-prop, dead-store elimination).
5. A **graph-coloring register allocator** over the IR, *replacing* today's
   accumulator-plus-spill scheme.
6. IR → chip lowering (the 30 primitives mostly survive underneath).

Multi-thousand-LOC, multi-month — comparable to or larger than the Excalibur
generics arc.

**Verdict: possible, but poor ROI for this project's workload.**
- b++'s performance wins came from *algorithmic* fixes (hashing out quadratic
  scans — the largest single wins) and *targeted lifts* (autovec,
  smart-dispatch outlining, builtin lifts), **not** from classical
  optimization. The hot-path arc took ~41% off the bootstrap with none of this.
- The workload (self-hosted toolchain + games) prioritizes **compile speed**
  and **byte-stable bootstrap**. More passes cut against both, and each new
  pass is surface area to break gen2==gen3.
- This is precisely the "locally rational, cumulatively inert" trap the
  Excalibur / Multics-drift post-mortem (Tonify Rule 28, killer-use-case gate)
  warns against.
- **Reopen signal:** when you hit an optimization the tree-walk genuinely
  cannot express — the canonical examples are *global (cross-block) register
  allocation* (impossible under the accumulator model, which reallocates per
  node) or *cross-block GVN*. Until then, the flat IR is speculative.

### (B) The runtime interface — GC stack maps, unwinding, tail calls

**Verdict: genuinely unnecessary. Being imperative exempts us — this is the
clean "we don't need it."**
- No GC (manual arenas + free-lists) → no stack maps to maintain.
- No exceptions (`E###`/`diag`, `putstr_err`, -1/0 returns) → no unwinding.
- Loop-based, not deep-recursion control flow → proper tail calls are a
  nice-to-have, not load-bearing.

This is the *defining* half of C-- and it evaporates for b++. It would only
reopen if b++ ever adopted a GC, which the studio philosophy (manual memory,
bespoke formats) says it will not.

### (C) A stable / serializable / shareable form — the BangDev-specific payoff

C-- aspired to be a *shared target multiple front-ends emit*. This is the one
axis with a plausible, project-specific motivation.

- There is a concrete candidate second front-end: **bangscript**
  (`docs/plans/bangscript_plan.md`), the declarative DSL that will *generate*
  the RTS AI tape. If BangDev grows more small DSLs, a **stable, dumpable**
  spine becomes the narrow waist that lets them all share the a64/x64/C/wasm
  backends — exactly C--'s use case, and aligned with the studio's
  "tools that build tools" identity.
- **Cost is far lower than (A):** the 30-slot contract is already half of it.
  Missing pieces are (i) a textual / serializable form of the spine for
  inspection, and (ii) a stable *input* API for a non-b++ front-end to emit.
  No SSA required. A dumpable spine would also slot naturally into the
  `bug --disasm` culture as a debug level between AST and disassembly.
- **The gate is Tonify Rule 20 (two consumers).** Today there is exactly one
  front-end (b++'s own). Do **not** build the shared form speculatively — open
  it when bangscript (or a second *named* front-end) concretely wants to emit
  spine. Same gate as generics and Tier-2 graduations.
- **Technical nuance for this axis:** wasm (a reserved target) is a *stack
  machine*. The spine's tree-walking / accumulator shape maps onto wasm *more*
  naturally than a register-CFG IR would. So if (C) happens, **do not
  reflexively flatten to a register IR** — the tree shape is an asset for wasm,
  not a defect. This is a further reason to pursue (C) without (A).

---

## Part E — Recommendation

- **(B) runtime interface:** skip. "Because we're imperative we don't need it"
  is correct — this half is literally what C-- exists to solve for
  functional / GC'd languages, and it is inert for b++.
- **(A) optimizing SSA middle-end:** defer. It *could* make the spine a real
  C-- / LLVM-class IR, but the ROI is poor for a compile-speed- and
  byte-stability-sensitive toolchain, and our gains have come from algorithmic
  and targeted work the tree-walk already expresses. Reopen only on a concrete
  optimization the AST passes cannot express (global reg-alloc / cross-block
  GVN).
- **(C) stable, shareable, inspectable spine:** the only axis with a
  BangDev-specific payoff (multi-front-end over shared backends; fits `bug`).
  Cheap relative to (A) because the 30-slot contract is half-done. **Gate on
  Rule 20** — a second named front-end (bangscript+) wanting to emit. If
  pursued, keep the tree shape (good for wasm), do not become a register-CFG.

**In one sentence:** the spine does not *need* to become a real C-- — the half
that defines C-- (a runtime for lambda calculus) is inert for an imperative
language, and the optimization half has weak ROI here; the piece that may one
day be worth it is the least obvious one — making the spine a **shareable,
inspectable** waist for BangDev's own DSLs — and that opens on a real second
consumer, not on the ambition of C-- parity.

---

## Appendix — terminology note: is b++ statically typed?

Relevant because it underpins why (B) does not apply. b++ is **statically typed
in the weak, optional sense**: type is a compile-time-only concept (no runtime
type tags — that is what puts it on the static, not dynamic, side), and
annotations (`: float`, `: ArchetypeRec`, `: fn`) drive codegen (int vs FP
registers, struct layout, member access). But the default type is the 64-bit
**machine word** (`auto x;`), the system is **unsound** (a `: Type` hint on a
pointer from `arr_struct_at` is an unchecked assertion over type-erased
storage — you can lie), and typing is **optional**. Net: weaker than C, in the
BCPL/Forth family of "typeless substrate with static hints", not the ML/Rust
family. See `memory/` notes on the `arr_struct` elem-size-erasure +
per-declarator-hint pattern for how genericity is achieved without parametric
polymorphism (which b++ also lacks — planned as Excalibur Feature 4, deferred).

### The five axes of "static" — keywords vs. type discipline

A common confusion: the `static` / `extrn` / `const` keywords use "static" in a
*different* sense than "static typing". In PL theory "static" means only
"determined at compile time", and that applies to several **independent** axes:

| "static" axis | applies to… | b++ keyword |
|---|---|---|
| static **typing** | the *type* of a value | `: float`, `: struct`, `void`, `auto` + inference |
| static **storage duration** | the *lifetime* of a variable | `static`, `global` |
| static **linkage / visibility** | the *visibility* of a symbol | `static` (internal), `extrn` (external) |
| static **mutability** | whether a slot is *writable* | `const`, `static const`, `global const` |
| static **dispatch** | which function a call resolves to | smart dispatch (builtins) |

Each can be compile-time ("static") or run-time ("dynamic") **independently**:
assembly has static linkage and zero typing; Python has neither these storage
keywords nor static typing; C has both. So `static`/`extrn`/`const` classify
b++ on the **storage / linkage / mutability** axes — *not* on the typing axis.
They are the C meaning of `static`/`extern`, about *how data is filed and
addressed*, orthogonal to *how its bits are interpreted* (the type).

- `static` — internal-linkage / file-local storage class (`bpp_parser.bsm:28`,
  `GLOB_*`). Where a symbol lives and who sees it; says nothing about its type.
- `extrn` — external-linkage declaration ("defined in another module/lib").
  Resolved in the link graph; an `extrn` with no backing definition raises
  **E264** (`mo_resolve_relocations`).
- `const` — immutability. Writing a const slot raises **E263**
  (`bpp_validate.bsm:747`, in the `T_ASSIGN` handler). Two flavors:
  `const X = N;` is **parser-level literal substitution** (emits no symbol — a
  `#define`/constexpr), while `global const`/`static const` is an **immutable
  `.data` slot** with real linkage.

**Why `const` is not "static typing".** In C, `const` is a *type qualifier*
(part of the type, flows through expressions: `const int*` ≠ `int*`). In b++ it
is checked as a **storage-class property of the slot** (read-only in `.data`),
not a type that propagates — E263 is a write-to-immutable check at the point of
assignment, not a type-flow analysis. So it is static *analysis*, not static
*typing*.

### Smart ≠ strong — intelligence vs. soundness is yet another axis

b++ has sophisticated compile-time type machinery — **smart dispatch**
(`bpp_io.bsm:134`, `bpp_dispatch.bsm`: rewrites generic `put(x)` →
`putstr/putnum/putfloat` by the inferred type of `x`; also worker/loop
synthesis), **smart promotion** (`bpp_dispatch.bsm:2004`: infers `auto` globals
into the right storage class via use-def tracking, `glob_pinned` / `: serial`
opting out), and the `_prim_hint` type-inference in the parser. This is real,
non-trivial type *awareness*.

But "smart" is about **inference and resolution** (figuring out what the
programmer meant, picking the right implementation), which is a *different axis*
from **soundness / enforcement** (rejecting ill-typed programs). Strong-vs-weak
typing is about enforcement, not intelligence. A language can be:

- smart + weak — **b++** (great inference, no guardrails),
- smart + strong — ML / Rust (great inference *and* enforcement),
- dumb + strong — early Java (explicit everything, still enforced),
- dumb + weak — assembly.

So the smarts make b++ more **ergonomic and expressive** without making it
**safer**. "Weak typing" remains the correct technical label for the
enforcement axis even though the inference axis is strong. This is a
*deliberate* design point, not an unfinished type system: maximize what the
programmer can express with minimal ceremony, trust the programmer, omit the
guardrails — the same "powerful and dangerous" philosophy as C / BCPL / Forth,
with modern inference layered on top. The freedom and the danger are the same
coin; that is the language's stated intent (a freer tool for the programmer to
build with).
