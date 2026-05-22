# B++ TODO — Shopping list

Pending-only. Detail lives in `docs/plans/*.md`. Shipped work lives
in `docs/journal.md` + commit history.

Last refresh: 2026-05-17.

---

## Active arcs (pick a thread to drive next session)

- **Content arc — decision point.** Engine no longer gates any of
  the four candidates: Wolf3D Phase 2 cont., Adventure Puzzle,
  RPG Dungeon, RTS demo. See `docs/plans/games_roadtrip.md`.
- **Multi-core Sprints A-G.** ~1010 LOC across 5-6 sessions,
  scattered between game features. Sprint E (Linux pthreads)
  blocked on ELF dynlink. See `docs/plans/multicore_state_report.md` §5.
- **bangscript runtime (DSL).** `.bang` parser + scheduler +
  verb system + cursor FSM. Foundation for Adventure Puzzle.
  See `docs/plans/bangscript_plan.md`.

---

## Engine sidequests (touch when in the area)

- **ELF dynamic linking** — Linux pthreads + Vulkan + ALSA + any
  FFI module unblock. ~2-3 days focused. See `docs/plans/elf_dynlink_plan.md`.
- **Linux Vulkan parity** — `stb/_stb_gpu_linux_vulkan.bsm`,
  ~1500 LOC. Depends on ELF dynlink.
- **Tier F compiler opts** — CSE / RegAlloc v2 / Auto-vectorization.
  Profile-gated, no real consumer yet. See `docs/plans/compiler_boost_roadmap.md`.
- **`bug` Phase 7 (game-debugger evolutions)** — trace mode +
  `name:line` breakpoints + conditional break + memory hexdump +
  snapshot/replay. ~280 LOC for items 1-4. Open when next
  game-debug session hits the same walls.
- **`effect_palette_quantize`** — 5th effect, needs stbpal
  integration. ~100 LOC. Ships when a game asks for indexed-
  palette post-process.
- **`fxlab` v2+** — auto-discover presets, live `.metal` editor,
  effect-chain reorder UI. Ships when v1 reveals real signal.
- **Linux x86_64 health restoration sidequest** — multi-item
  arc, widened 2026-05-21 after a cheap-discovery pass for S4
  cost-model inliner. ARM64 stays the reference target for
  every active perf sidequest until this closes.
    1. ✅ `pthread_kill` cross-compile fail — FIXED 2026-05-21
       (`d1f5457`), 14-LOC stub in `_core_linux.bsm`.
    2. ⚠️ `bpp_lin` runtime SIGSEGV on Linux x86_64 — newly
       discovered. Cross-compiled `bpp_lin` ELF runs cleanly
       for hello-world programs but segfaults at startup
       when self-hosting. strace shows the binary oscillating
       between "x32 mode" and "64 bit mode" before the
       fault — likely entry-point or runtime-init regression
       accumulated between 2026-05-02 and 2026-05-21. Bisect
       on commits in that window is the next step.
    3. ⏸️ Phase B1 (freelist register alloc) x86_64 emission
       bug — disabled since 2026-04-15 (`19da538`). Manifests
       under self-host on a fresh `_x64_has_call` recursion
       path. Untested since (items 1 + 2 block re-evaluation).
    4. ⏸️ Phase B2 (inline `: base` helpers) x86_64 path —
       disabled same commit, paired with B1.
  Items 3 + 4 cannot be re-evaluated until 1 + 2 are clean.
  S4 (cost-model inliner) inherits the ARM64-only posture for
  the same reason; see `docs/plans/sidequest_cost_model_inliner.md`
  for the explicit deferral note.
- **Host-aware compiler + install** — `bpp` auto-detects host
  OS+chip; defaults match host instead of always macOS arm64.
- **C-emitter `var T` parity** — pick uniform storage strategy
  (likely heap-backed) and sync native + C address rules.
  Defer until a real consumer needs `&local_struct`.
- **Excalibur Feature 4 (templates)** — `$T` generics + struct
  reflection. ~1500-2500 LOC, 5-8 sessions. Gated on
  Features 1-3 closed (DONE 2026-05-12) + 2 real consumers
  asking. See `legacy_docs/excalibur_arc.md`.

---

## Known open bugs

- **`&struct.field` returns wrong address.** Codegen short-circuits
  to base instead of adding field offset. Workaround: hand-compute
  offset or use typed `s.field = ...` access. Defer until a real
  consumer needs `&` on struct fields.
- **`call()` float args on x86_64.** `call(fp, args...)` drops
  float args (native function calls unaffected).
- **String dedup across modules.** Duplicate strings produce
  duplicate data-segment entries. Harmless but wasteful.
- **`bootstrap.c` is ARM64-only.** Regen with current C emitter
  when bootstrapping a non-supported platform.
- **`@profile` nested zones aggregate FLAT.** Parent total includes
  children's time. Hierarchical breakdown deferred to v3.

---

## Small / opportunistic (each <50 LOC, land when touching the file)

- `--stats` compiler flag — module / function / global / struct
  counts, peak tokens, wall-clock. ~40 LOC.
- Arena-backed AST in parser — swap per-node `malloc` for
  `arena_alloc(mod_arena, NODE_SZ)`. ~30-50 LOC.
- `mem_track` for games — per-tag allocation totals + `mem_report()`.
  ~50 LOC.
- `main()` in `.bsm` = fatal error (E223). ~5 LOC.
- `stub → real` dedup treats as always silent. ~30 LOC.
- `bug` headless detection — auto-fallback to `--dump` when
  stdin is not a TTY (`isatty(0)` / `sys_fstat(0)`). ~30 LOC.
- FFI auto-marshalling — compiler already knows FFI param types;
  eliminate the `(long long)` casts at call sites.
- Multi-return values — `return a, b;` + `x, y = foo();`.
  Eliminates the `float_ret` / `float_ret2` hack.
- Retina support — `contentsScale = 2.0` on CAMetalLayer.
- All games via C emitter — currently only `snake_gpu` and
  `snake_raylib` verified end-to-end through `bpp --c → gcc → run`.

---

## Deferred (waiting for trigger)

- **Tonify Rule 33 — composition heuristic** (distinct enum vs
  bit flag). Opens after the unsigned-annotations sidequest ships
  AND a retroactive audit sweep gives 4-5 concrete examples.
- **C emitter SIMD mapping** (`: double` + `vec_*`). Opens on
  two real consumers needing `vec_*` via the C-emit path
  (Rule 20). Today `vec_*` is native-only opt-in.
- **W032 cleanup sweep** — ~57 test sites with intentional integer
  `put(x)` diagnostics. Mechanical edit. Defer until a
  `--strict-warnings` CI gate or unrelated test touch.
- **`restrict` keyword** — when native codegen does aliasing-aware
  optimization. Post-1.0.
- **`offsetof` / field reflection** — P1 for bangscript runtime.
- **Prefetch builtins** (`__builtin_prefetch`) — when real
  profiling on RTS-sized workloads shows the win.
- **Full aliasing-aware optimizer** — post-1.0.

---

## P0 game modules (pure B++, ship when a consumer arc opens)

- `stbdialogue` / `stbinventory` / `stbmenu` / `stbsave` — RPG.
- `stbanim` — RTS sprite animation.
- `stbbangs` / `stbverb` / `stbcursor` — bangscript runtime.

---

## Post-1.0 (scaling capability — see `docs/journal.md` for context)

Parallel codegen per module · asset streaming (`stbasset.bsm`) ·
memory budget system · true live code reload · multi-error
recovery across modules · profiler GUI (timeline view) ·
networking (`stbnet.bsm`) · target-architecture refactor ·
Windows / WebAssembly backends · CLAP audio plugin export ·
EmacsOS distribution.
