# Bang 9 ↔ running game — integration study (future)

**What this is.** A *study plan* (not an implementation plan) for a question we
will face when Bang 9's game-integration becomes a priority: **how should the
IDE let you see, tune, and debug the running game?** B++ runs the game in a
**separate process** from Bang 9 (hot-reload safety, signal-handler isolation),
so the answer is not obvious. Before committing, study the three established
approaches and how each maps to our architecture.

Parked for now — zoomer and the Quake port come first. This records the map so
the study is fast when we pick it up. The screen-capture primitive just built
(`stbwindow` `screen_capture`, `_stb_platform_macos.bsm`) is the seed of
approach C.

## Context (our constraints)

- **Separate process by design.** The game runs apart from Bang 9 so a game
  crash / signal handler can't take the IDE down, and hot-reload can swap code
  under the game. This deliberately rules out the "same process" approach (A).
- **We already have a hot-reload loop.** fxlab: drag a slider in Bang 9 → the
  running game (other process) responds in ~30 ms. The feedback loop exists; the
  missing piece is *seeing* the game inside the IDE.
- **We already have a remote-debug foundation.** The `bug` debugger drives a
  remote stub over the GDB remote protocol (`bug --tui`, debugserver-style). A
  data link between IDE and a separate process is not new to us — it exists.

## The three approaches to study

### A. Play-In-Editor — same-process embedded viewport
**Who:** Unity (Game view), Unreal (PIE / Simulate). The game runs *inside* an
editor panel, same process; the editor owns the render target; you edit
properties live while it plays.
**Study:** how the editor owns/【shares the render target; runtime property
editing; the coupling cost (one process for game + tools).
**Mapping to b++:** **conflicts with our separate-process model** — adopting it
means merging game + IDE into one process, losing hot-reload isolation and
signal-handler safety. Study it mainly to understand the tradeoff we are
*declining*, and what we give up by declining it (tightest possible loop,
in-viewport picking/gizmos).

### B. Remote debug protocol — separate process + data link
**Who:** Godot (editor ↔ running game over a debugger protocol: remote scene
tree, live property edit, remote inspector). The game's *visual* is its own
window; the editor manipulates live state over the wire.
**Study:** Godot's editor-debugger protocol (message format, remote scene tree,
how live edits are applied, how it stays decoupled). Then: **how to extend our
existing `bug` remote stub** (GDB-remote foundation) toward game/scene state —
inspect/modify the running game's data from Bang 9 over the link we already
have.
**Mapping to b++:** **closest fit** — it respects the separate-process model and
builds on infrastructure we already own (the bug remote stub + fxlab's
process-to-process push). This is the **high-value study**.

### C. Screen-capture mirror — decoupled visual
**Who:** boomer/OBS/zoomer category, accessibility magnifiers. Capture the game
window's pixels and show them; no protocol, no coupling, works for any process.
**Study:** window-track capture (capture a specific window, even occluded — on
macOS `CGWindowListCreateImage`), live frames vs one-shot, latency, the
input-forwarding question (synthesize events to the game window if interaction
is wanted — and the re-coupling cost of doing so).
**Mapping to b++:** the **lightweight visual complement** — a view-only mirror
for the observe/inspect cases (breakpoint frame, passive/visual tuning, tuning
against a demo/replay), built on the capture primitive we already have. Not a
substitute for playing in the real window + hot-reload for interactive tests.

## Hypothesis to validate (the likely answer)

b++ probably wants **B + C, not A**:
- **B (remote protocol on the `bug` foundation)** for *data*: inspect/modify the
  running game's state, scene, and tunables from Bang 9 — the tight loop without
  merging processes.
- **C (capture mirror)** for *visual*: a quick, decoupled view of the game
  inside the IDE, strongest next to the debugger (see the frame at the
  breakpoint) and with demo/replay (watch real gameplay run while you tune).
- **Not A**, because it costs the separate-process isolation that hot-reload and
  crash-safety depend on.

The study's job is to confirm or refute this, and to scope what extending the
`bug` stub to game state actually takes.

## Study resources

- **Godot** — editor debugger / remote inspector docs + source (the closest
  architectural sibling: separate process + protocol).
- **Unreal PIE / Unity editor architecture** — to understand the same-process
  approach we are declining, and what its tight loop buys.
- **Our `bug` remote stub** — `src/bug.bpp` + the GDB-remote plumbing
  (`bug_observe_<os>.bsm`); the foundation approach B would extend.
- **fxlab hot-reload loop** — the existing process-to-process push (the data
  half of the loop already works).
- **RenderDoc** — frame-capture / GPU-state inspection, a reference for the
  "inspect a frame" side (related to C, and to a future GPU debugger).

## When to revisit

When Bang 9's game-integration becomes a priority (after the zoomer tool proves
the capture path and after the Quake port gives a real game to integrate with).
Not now. The capture primitive is the only piece we build today; approaches B
and A stay studied-not-built until the need is concrete (Rule 28).
