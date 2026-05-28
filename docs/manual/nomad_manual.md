# The B++ Nomad Manual — Portability Doctrine

How B++ travels. This is the playbook for reaching a new target — a new chip, OS,
binary format, or GPU/audio backend — without the cost curve exploding, and the
field-tested techniques learned building B++ itself. It is the *target-agnostic*
companion to `bootstrap_manual.md` (which is the production/bootstrap reference)
and the place specific port plans point back to. When you start a new port, read
this first; when you finish a hard lesson, write it here.

The one-line thesis: **a new target is the sum of the layers it adds, never a
modification of the layers it shares.** Adding the 5th target is as cheap as the
2nd because the shared layers carry zero per-target weight.

---

## 1. Additive layers (Rule 41) — add, never modify

Every target combination is built by writing **only the missing layers**. The
compiler frontend, the stb modules, the programs, and the other targets' backends
stay untouched.

```
stb/ + programs            ← target-agnostic, NEVER modified
  ├── new CHIP    → src/backend/chip/<isa>/   (codegen + primitives + enc)
  ├── new OS      → src/backend/os/<os>/      (platform + core + bsys + bug_observe)
  ├── new FORMAT  → src/backend/target/<fmt>/ (Mach-O / ELF / PE / WASM writer)
  └── new GPU     → stb/<gpu>_render_backend.bsm (+ per-OS loader/surface)
```

**The three audit tests for any portability PR** (from Rule 41):
1. Did you modify an `stb/` file? If yes, the abstraction leaked — widen it in
   *one* place rather than special-case per target.
2. Did you touch another target's backend? Targets are siblings, not parents. A
   macOS-file change for a Linux feature is a violation.
3. Could a `import "stbwindow.bsm"; main(){…}` program be byte-identical source
   across targets? No `#if TARGET`, no per-target runtime branches. If your
   target needs a program-level escape hatch, the leak is in the platform layer.

Precedent: macOS→Linux added only `os/linux/` + `target/x86_64_linux/` (~2300
LOC), zero stb/program/chip changes; a64→x64 added only `chip/x86_64/` (~3200
LOC). The "zero change in user code" is the feature, not a side effect.

---

## 2. Push the decision early — the spine

The earlier in the pipeline a decision is made, the fewer backends carry its
weight.

```
Source → Lexer → Parser → AST → Dispatch/Validate → Codegen → Binary
                  ↑ semantics        ↑ analysis        ↑ emission (chip/OS only)
```

- **Shape-shared codegen lives in the spine** (`bpp_codegen.bsm`): the AST
  walkers (`cg_emit_stmt`, `cg_emit_node`), builtin dispatch, the inline splicer.
  Chips contribute only **ISA-atomic primitives** through a function-pointer
  table (`ChipPrimitives`). When you add a chip, you implement primitives, not
  walkers.
- **The test:** "does the `bpp` *binary* need both variants linked?" If yes →
  spine. If it's compile-time-target-selected (one platform per binary) → a
  per-target file. Runtime concerns (GPU state, syscalls) are per-target; codegen
  *shape* is spine.
- **Anti-pattern:** the same logical *decision* (not the same instruction)
  duplicated across `a64_codegen` and `x64_codegen` and `bpp_emitter` means it
  belongs in the frontend. Short-circuit `&&`, dead-code `if (0)`, operator
  lowering — the backend should differ only in *how* it emits, never in *what* it
  decides to emit.

---

## 3. Build the engine, stub the hardware — couple on arrival

The pattern for any capability you can't fully exercise yet (no hardware, no
driver, no second OS): **build the engine, verify what's testable, stub the rest
behind a wired door, and leave an activation checklist.** This is Discipline #2
(stub the parity floor) scaled up to whole subsystems.

1. **Find the payload-agnostic engine** and build it fully. Example: ELF dynamic
   linking — the PLT/GOT machinery is identical whether you link `libc`,
   `libvulkan`, or `libasound`. Build it once.
2. **Verify it against whatever the environment *does* have.** No GPU in Docker?
   Verify the dynlink engine by linking `libc` and calling a symbol — same
   machinery, fully exercised, payload swapped for one that exists.
3. **Stub the hardware-gated payload** in its own per-OS file, each call marked
   `// ships when <hardware> present`, returning sane "unavailable" defaults so
   programs compile and run (degrading gracefully) on a box without the hardware.
4. **Wire the door** — the abstract API is connected to the proven engine, so
   coupling the real driver is a small, known step.
5. **Write the activation checklist** in the plan: the exact steps to flip stubs
   → real when the hardware arrives. The engine is already proven; activation is
   mechanical plus any one pre-identified risk.

The win: the motor is mounted and running on everything you can test; the entry
door is linked; you bolt on the driver when the hardware shows up — no rework,
because the engine never had to guess.

---

## 4. Always test more than one output

A single backend hides bugs. B++ keeps **independent codegen paths** precisely
because each catches a different bug class. When you add a compiler feature, run
it through more than one:

- **Native vs C-emit.** The native backend silently tolerates what C rejects:
  integer-division precedence in `*=`, `extrn`/function namespace collisions,
  missing builtin handlers. A test that fails only under `--c` is still a real
  bug — usually in the source the native path merely tolerated. (`run_all.sh` +
  `run_all_c.sh` are separate harnesses for this reason.)
- **Dog-food the cousins.** Before trusting your own binary-format assumptions,
  compile the equivalent with gcc/clang and read it with `readelf`/`objdump`.
  The ELF-dynlink reconciliation did exactly this: a gcc pthread binary in Docker
  revealed that pthread needs the full C runtime (`__libc_start_main`/TLS) — a
  fact that would have cost days of crash-debugging if assumed away. The cousins
  are a free reference implementation; check against them.
- **Run in the real target, not just the host.** Cross-compiling cleanly is not
  running cleanly. Bang 9 *built* for Linux long before it *ran* — running it in
  Docker/XQuartz surfaced two latent bugs the build never could (window
  dimensions never set → zero-height layout; no `ConfigureNotify` → no resize).
- **When a "fix" hides a symptom, suspect it's masking, not fixing.** The stbui
  clip "fixed" overlapping panel text on Linux by *clipping it away* — which also
  hid the real content. The actual bug was upstream (`window_width()` returned 0).
  A clip that makes content vanish is a clue, not a cure. Bisect: disable the
  suspect layer and see what the symptom *really* is.

---

## 5. Format-agnostic model, format-specific emission

When two targets need "the same thing" in different file formats, split it:

- **The model is shared** — push it into the spine. *Which* externs a program
  calls, their GOT-slot assignment, the relocation marker: that is identical
  across formats and lives in `cg_ex_*` + reloc type 4 in `bpp_codegen.bsm`.
- **The emission is per-format** — each target writer consumes the shared model
  and lowers it to its own structures. Mach-O builds chained fixups + a GOT; ELF
  builds `.rela.plt` + `.got.plt`. **Do not try to spine the emission** — the
  formats have no common ground at the point that matters.

The tell that you've split it wrong: a bug that exists in *only one* format.
The Mach-O GOT-straddle bug (per-page chained fixups) had no ELF analogue
(per-entry relocations) — so there was nothing to share in the *fix*, only in the
*policy* ("lay the GOT out so the loader resolves it"). Spining the format-specific
fix would have made ELF carry dead weight for a problem it can't have. Graduate
the shared model at the **second consumer** (Rule 20/36); keep the rest per-target.

---

## 6. Byte-stability is the floor

Any change to compiler/runtime/backend source must keep the bootstrap
**gen1 == gen2** byte-identical (a 1-cycle oscillation gen1≠gen2==gen3 is normal
after a codegen-self change; a 2-cycle gen2≠gen3 is a real bug). Pin
`BPP_BUILD_ID=0…0` when comparing — an unpinned build embeds a varying id and
will "differ" for a reason that isn't codegen. Comment-only and doc-only edits are
inert by definition (the lexer strips comments) and need no rebuild; everything
else bootstraps. A Layer-4 change "breaks every binary" — the discipline is
non-negotiable there.

---

## 7. Verify reality, not the header

Documents and assumptions go stale; git and the binary do not.

- A plan's status header can lie (the 2026-05-28 cleanup found shipped arcs still
  headed "PROPOSED"). Verify against git, then fix the header. See the Plan
  Lifecycle section of `bootstrap_manual.md`.
- A perf claim must cite `bench_compile.sh`, not memory — "was 0.41s" without the
  harness once compared an error-out to a compile and invented a regression.
- A successful cross-compile run can still be the *fallback* path (Linux X11
  silently drops to ANSI when the server is unreachable, and exits 0). Confirm the
  thing you think happened actually happened — look at the artifact.

---

## Worked example — the 2026-05-28 Linux sprint

A compact illustration of the whole playbook:

- **Bang 9 on Linux** — built fine, but ran wrong. Running it in Docker/XQuartz
  (§4) surfaced a window-dimension gap: the X11 platform never set
  `_stb_win_w/_stb_win_h`, so `window_width()/height()` returned 0 and every
  `ui_layout` GROW row collapsed to height 0. The "graceful clamp" we'd shipped
  was *masking* it (§4: a fix that hides content). Bisecting (disable the clip
  layer) revealed the real bug; the fix was purely additive in the Linux platform
  layer (§1). Then `ConfigureNotify` made resize reflow + clamp work — additive
  again, mirroring the macOS detection.
- **The Mach-O GOT-straddle bug** — a per-page-fixup problem with no ELF analogue
  (§5). Fixed in the Mach-O target writer alone; the shared model stayed in the
  spine; the policy got written down so the ELF engine inherits it.
- **The ELF dynlink engine** (its plan, in `docs/plans/` while active) — the
  engine/door pattern (§3): build the payload-agnostic engine, verify it in
  Docker against `libc` (§4, dog-fooding the cousins to settle libc-init), stub
  the GPU/audio payload behind a wired door, leave the activation checklist for
  when x86 hardware arrives.

Each move was additive, dog-fooded against reality, byte-stable, and pushed the
decision to the right layer. That is the nomad way: travel light, add only what
the new ground requires, and never make the home you came from carry it.
