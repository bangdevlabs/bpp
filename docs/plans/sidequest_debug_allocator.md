# Sidequest: Debug allocator (`_MEM_DEBUG`) + `bug` watchpoints

**Status:** opened 2026-05-27. **Phase A0 SHIPPED** — `_MEM_DEBUG` toggle +
poison-on-free + skip-zero-on-pop live in `bpp_mem.bsm`, default off,
bootstrap byte-stable, suite 180/0/12. Phases A1 (double-free), A2
(alloc/free-PC history + leak report), B0 (`bug` watchpoints) remain open.
Opened because the free-list reuse arc
(`sidequest_freelist_reuse_corruption.md`) is "shooting in the dark" — we
keep bisecting by hand because we have **no memory-safety tooling**. Every
cousin language pairs a recycling allocator with companion debug tooling;
we don't, and it's costing us. This sidequest builds that tooling.

## Using A0 (manual procedure — it is not a suite test)

`_MEM_DEBUG` is a compile-time `const` in the auto-injected `bpp_mem.bsm`,
so the test suite (which compiles with whatever is committed = off) cannot
toggle it per-test — exactly like its sibling `_MEM_REUSE`. Exercising it
is a deliberate, manual procedure:

```bash
# 1. Enable poison + reuse in src/bpp_mem.bsm:
#      const _MEM_REUSE = 1;
#      const _MEM_DEBUG = 1;
# 2. Build a compiler that carries the debug allocator:
./bpp src/bpp.bpp -o /tmp/bpp_dbg
# 3. Use it to compile the failing program; the debug allocator now runs
#    while /tmp/bpp_dbg builds the AST, so a use-after-free on a recycled
#    node surfaces the poison byte (0xBE) instead of valid-looking data.
/tmp/bpp_dbg tests/test_thread.bpp -o /tmp/t
# 4. Read the corrupted field (e.g. via the validate instrumentation in
#    sidequest_freelist_reuse_corruption.md): itype == 0xBE confirms
#    recycled memory; itype != 0xBE points at a write-spill.
# 5. Restore the toggles to 0 before committing anything.
```

Poison value, layout, and gating live alongside `_MEM_REUSE` so the two
diagnostics compose. Never commit with either toggle set to 1.

## Why now — the cousins co-evolve allocator + debug tooling

From the free-list research (C/C++, Zig, Go, Jai/Odin):

| Cousin | Recycling allocator | Companion debug tool |
|---|---|---|
| C/C++ | ptmalloc/jemalloc (bins, reuse) | **ASan / Valgrind** — poison, red-zones, UAF + double-free + alloc/free stacks |
| Zig | std heap | **`GeneralPurposeAllocator`** — built-in double-free / UAF / leak detection with stack traces; "never reuses addresses in safety mode" |
| Go | span allocator (mcache/mcentral/mheap) | **race detector + `GODEBUG=allocfreetrace`** |
| Jai/Odin | context + temp allocators | debug allocators that track alloc/free |

**The lesson:** a recycling allocator without companion debug tooling is a
minefield. Our free-list arc surfaced THREE latent compiler bugs (struct-
field overflow [fixed], hex bit-63 SIGSEGV, the current `Node.itype`
corruption) — and each took manual byte-level bisection because `bug` has
**no watchpoints** (documented limitation in `debug_with_bug.md`) and
`bpp_mem` has **no debug mode**. This is Rule 35 applied to *tooling*: the
stress test revealed a gap in our debugging infra, not just in the engine.

## Two complementary pieces

### Piece A — `_MEM_DEBUG` mode in `bpp_mem.bsm` (the practical tool)

A compile-time toggle (`const _MEM_DEBUG = 0/1`, same shape as `_MEM_REUSE`)
that turns the free-list into a Zig-GPA-style safety allocator. Tiers, in
order of value-per-LOC:

**Tier 1 — poison-on-free + no-zero-on-pop (~10 LOC). Cracks the CURRENT bug.**
- `free(small)`: fill the block (past the intrusive link word) with a
  distinctive poison byte, e.g. `0xBE`, before pushing to the free-list.
- `malloc(small)` pop: in debug mode, do NOT zero the popped block — hand
  it out still carrying poison in any field the new owner doesn't write.
- Effect: a use-after-free (stale read of a freed-then-reused block's
  unwritten field) reads `0xBE…` — a distinctive signature, not zero.
- **Immediate use for the `Node.itype` bug:** re-instrument the E233 check
  to print the corrupt `itype`. If it reads the poison byte → the node sits
  in recycled free-list memory (a UAF / some nodes are NOT arena). If it
  reads a non-poison value → it's a write-spill in infer/parse, not reuse of
  freed memory. **Discriminates the two open hypotheses in one run** — the
  thing we've been guessing at by hand.

**Tier 2 — double-free detection (~10 LOC).**
- The small-block marker uses the disjoint gap `[_MEM_NUM_CLASSES, 4111]`
  (currently unused). On `free(small)`, write a `_MEM_FREED_MAGIC` (a value
  in that gap) into the marker; if the marker is ALREADY the freed-magic →
  `panic("double free")` (stack trace via minisym). On pop, restore the
  class marker. Catches the double-free bug class (one of the three this
  arc could still be hiding).

**Tier 3 — alloc/free-site history + leak report (~30 LOC).**
- In debug mode use an expanded header (e.g. 32 bytes: raw, marker,
  alloc_pc, free_pc) recorded via `caller_pc`. On a poison-confirmed UAF,
  print "block last freed at <free_pc>, allocated at <alloc_pc>" — the ASan
  experience. At exit, walk live blocks → leak report. (Header-size change
  is debug-only; release mode keeps the 16-byte header untouched.)

**Gating:** `_MEM_DEBUG = 0` in release → zero change, bootstrap byte-stable,
suite green. Debug is strictly opt-in (rebuild with the toggle), never the
default. Composes with `_MEM_REUSE` (debug mode implies reuse, since the
whole point is catching reuse bugs).

### Piece B — watchpoints in `bug` (the canonical "who writes X")

`debug_with_bug.md` lists "No watchpoints" as a known gap. The GDB remote
protocol (which `bug` already speaks to debugserver/gdbserver) has the
`Z2,addr,len` / `z2,addr,len` packets for hardware **write watchpoints**;
ARM64 has 4 HW watchpoint registers. Add:
- `bug --tui --watch-mem <hexaddr>:<len> ./prog` (CLI) + `wm <addr>` (REPL).
- On the watchpoint trap, report the writing instruction's PC + the FP-walk
  stack (reuse the existing observe-mode stack machinery).

This is the surgical "who corrupts this byte" tool. **Caveat:** it needs the
target address, which for a dynamic AST node is one of thousands — so it
pairs with Piece A (the debug allocator can print a block's address) or a
phase-time `caller_pc`/address probe. Powerful for "I know address X is
getting clobbered, catch the writer."

## How this cracks the current `Node.itype` bug

1. Build with `_MEM_DEBUG=1` (Tier 1: poison + no-zero) + `_MEM_REUSE=1`.
2. Re-run the failing compile with the validate instrumentation; read the
   corrupt `itype` value:
   - `== 0xBE` (poison) → the node is in recycled free-list memory → it's a
     UAF / some nodes aren't arena. Use Tier 3 (free-site PC) or a Piece-B
     watchpoint on the block to find the freer + the stale reader.
   - `!= poison` (e.g. still 0xC0/0xA9) → it's a **write-spill** in
     infer/parse (a wide store of an adjacent field clobbering `itype`),
     NOT reuse of freed memory. Then grep `add_type` / parser node-init for
     a byte-field store emitting `STR` instead of `STRB`, and confirm with a
     Piece-B watchpoint on the node's `itype` address.

Either branch ends the guessing.

## Phasing

| Phase | Scope | LOC | Gate |
|---|---|---|---|
| A0 ✅ | `_MEM_DEBUG` toggle + Tier 1 poison/no-zero | ~10 | release (`=0`) bootstrap byte-stable + suite 180/0/12 — DONE 2026-05-27 |
| A1 | Tier 2 double-free (freed-magic) | ~10 | smoke test: double-free → panic |
| A2 | Tier 3 alloc/free-PC history + leak report | ~30 | smoke test: known leak reported |
| B0 | `bug` `Z2` watchpoint packets + `--watch-mem` / `wm` | ~60 | smoke: watch a global, catch the writer's PC |

A0 alone cracks the current bug (the discriminator) and is the minimum to
unblock the free-list reuse ship. A1–A2 + B0 are durable infra for the next
memory bug (and there will be one — every recycling-allocator language keeps
this tooling permanently for exactly this reason).

## Acceptance (per Rule 37 + bootstrap discipline)

- `_MEM_DEBUG=0` (release): bootstrap gen1==gen2 byte-stable, suite 180/0/12,
  `bench_compile.sh` unchanged (debug code is dead-code-eliminated when off).
- `_MEM_DEBUG=1`: a deliberate double-free / UAF / leak smoke test fires the
  expected panic / report.
- The current `Node.itype` bug's hypothesis is discriminated (poison vs
  write-spill) — recorded back into `sidequest_freelist_reuse_corruption.md`.

## Cross-references

- `docs/plans/sidequest_freelist_reuse_corruption.md` — the immediate
  consumer; A0 discriminates its two open hypotheses.
- `docs/manual/debug_with_bug.md` — "No watchpoints" is the gap Piece B fills.
- `src/bpp_mem.bsm` — `_MEM_REUSE` toggle is the pattern `_MEM_DEBUG` mirrors.
- `src/bpp_runtime.bsm` — `caller_pc` / `panic` (minisym stack traces) are
  the primitives Tiers 2–3 lean on.
- Free-list cousins study (session notes, 2026-05-27): Zig GPA, ASan,
  Go `allocfreetrace` — the prior art this sidequest adapts to B++.
