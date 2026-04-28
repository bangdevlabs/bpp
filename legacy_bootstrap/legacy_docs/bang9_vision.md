# Bang 9 — vision

**A DAW for game development, built in B++, shipping to artists.**

This document is the strategic direction for Bang 9 — BangDev's
proprietary IDE built on top of the open B++ language. It is the
north star every session — agent, contributor, future self — should
orient to.

---

## One-sentence pitch

Bang 9 is an integrated game development environment for B++
projects, modeled after music DAWs (Ableton, Logic, FL Studio):
a single host window that loads creative tools as panels, with
code, assets, music, and levels all living in one project surface.

---

## The mental model: Bang 9 is the DAW, tools are the plugins

Everyone who has ever opened Ableton Live understands the shape
of Bang 9 in a glance. Swap "synth plugin" for "pixel editor",
swap "MIDI clip" for "level tilemap", and the architecture
carries over wholesale.

```
Ableton Live (the host)                 Bang 9 (the host)
├─ project + timeline                   ├─ project + file tree
├─ MIDI editor (native)                 ├─ code editor (native, acme-style)
├─ transport: play/stop/record          ├─ transport: build/run/debug
├─ output meters + automation           ├─ output console + build status
└─ hosts VST/AU plugins as windows      └─ hosts tools/ as panels

Serum, Kontakt, Ozone (the plugins)     modulab, mini_synth, level_editor,
                                          font_forge (the tools)
├─ each does one thing                  ├─ each does one thing
├─ runs STANDALONE (own window)         ├─ runs STANDALONE (own window)
├─ runs INSIDE the DAW (as plugin)      ├─ runs INSIDE Bang 9 (as panel)
└─ same UX in both modes                └─ same UX in both modes
```

This analogy is not decoration — it is the architectural
commitment. Every tool in `tools/` must satisfy the dual-mode
contract: run standalone, embed cleanly in Bang 9.

---

## Who Bang 9 is for

### Primary: artists and designers

The target user never opens a terminal. They download Bang 9 as
a single installer (.dmg on Mac, .exe on Windows when the port
lands, .AppImage on Linux), double-click, and see a window with
panels for every aspect of the game they're working on. They:

- Edit sprites in the Sprites panel (which is modulab under the
  hood).
- Compose music in the Music panel (mini_synth under the hood).
- Paint levels in the Levels panel (level_editor).
- Tweak small code values in the Code panel (acme-style text
  editor — enough to change `BULLET_SPEED = 120` to `150`, not
  enough to write a module from scratch).
- Hit **Run** to compile and play the game.
- Hit **Export** to ship it (distant feature).

They never see a compiler invocation, a test suite, a dependency
graph, or a build file. Bang 9 is their entire world.

### Secondary: programmers working with artists

Hardcore programmers have their own editors (Emacs, VSCode, Vim)
and use `bpp` directly. They commit to the same Git repo the
artist's Bang 9 writes to. Bang 9 is **optional** for them. They
open Bang 9 when they want to see the current state of the
project visually, or when they need to tweak something
non-programmatically.

### Tertiary: curious hobbyists

People who download B++ to learn the language, see the tools in
`tools/`, and want a more polished integrated experience will
discover Bang 9. For them, Bang 9 may or may not be worth
paying for — depends on the price point and the polish.

---

## Relationship to B++ (the language)

**B++ is open source (Apache 2.0). Bang 9 is proprietary.**

This split is deliberate and defensible:

- **B++ as language** is infrastructure — the compiler (bpp),
  the standard library (stb), the tools (tools/), the debugger
  (bug), the documentation (docs/). All Apache 2.0.
  Redistributable, forkable, commercially usable.
- **Bang 9 as product** is a commercial offering built on top
  of the open infrastructure. BangDev (the maker) owns it,
  sells it (someday), and controls its direction.

This is how it works in music:

- **Audio Unit / VST / LV2** — open standards.
- **Ableton Live / Logic Pro** — proprietary products that
  implement those standards.
- Plugins made for one DAW work in others because the standards
  are open.
- The DAWs compete on UX, workflow, integration — not on
  whether they can play a WAV file.

Bang 9 competes on **UX, packaging, and integration quality**
— not on exclusive access to anything. Every tool Bang 9 wraps
is also available standalone, openly, for anyone who wants to
use it without Bang 9.

### Ethical guardrails

These three are non-negotiable:

1. **Standalone tools stay viable forever.** Bang 9 must never
   cripple the standalone versions of its panels to force
   upgrades. An artist who chooses to run `./modulab` directly
   (without paying for Bang 9) gets a fully functional sprite
   editor. Bang 9's value is integration, not exclusivity.
2. **B++ is not a hostage.** No language feature is introduced
   solely to benefit Bang 9. All B++ changes go through the
   public RFC process documented in `GOVERNANCE.md`. Community
   contributions are welcome and reviewed on merit.
3. **Transparency of commercial intent.** Users who download
   B++ should see, in the README and other entry points, that
   Bang 9 exists and is commercial. No dark patterns.

---

## The 1.0 milestone: pathfind complete

Bang 9's first major milestone — and by extension B++ 1.0 — is
not a Wolf3D-style engine showcase. It is a **complete,
shippable game produced entirely within the ecosystem**.

**Target: `games/pathfind/` shipped as v1.0, with:**

- Intro scene (title screen, music fade-in)
- 3 levels, each designed in level_editor within Bang 9
- Score system with HUD
- Looping background music produced in mini_synth
- Final cutscene when the player completes all three levels
- Sprites polished in modulab
- All assets authored through Bang 9
- No step of the production required a non-B++ tool

This is what we ship as evidence the ecosystem works. The
narrative is: "We built a language, a compiler, a debugger,
an IDE, a suite of creative tools, and then used all of them
together to produce a game. Every pixel, every sound, every
level was made inside the ecosystem."

### Why pathfind and not Wolf3D

Wolf3D was originally slated as the flagship game. It is a
**technical stunt** — a raycaster in B++ proves the engine is
capable. But a raycaster alone does not validate the
**production pipeline**. Levels, sprites, and music would still
be authored externally or hand-coded.

Pathfind is simpler: a tile-based puzzle game. Levels are
tilemaps. Sprites are small. Music is looping ambient. Every
content type maps cleanly to a Bang 9 panel. If Bang 9 cannot
produce a complete pathfind v1.0, it cannot produce anything
larger either. Pathfind is the **minimum viable test** of the
whole ecosystem.

Wolf3D returns as a post-1.0 prestige project (see below).

---

## Post-1.0 prestige: Wolf3D as mod

After B++ 1.0 ships (pathfind complete), the ecosystem has
proven itself. The next flagship is an ambitious expression of
what the ecosystem can produce:

**A Wolf3D-style FPS mod / spiritual successor**, but:

- A **modern** game with **retro visual aesthetic** — not a
  literal clone, a continuation in the same spirit.
- **Our own story, our own characters, our own episodes**.
- Every level, sprite, sound, and line of dialog authored
  in Bang 9.
- The engine is ours (B++ raycaster + stbdraw + stbrender).
- The aesthetic choice is deliberate: retro because retro
  reads well and the raycaster naturally hits that look,
  not because we couldn't afford better.

This is the "Tarantino move" — modern filmmaking paying
deliberate homage to an earlier era, with modern production
quality. The story: "We built an ecosystem, shipped a
complete game in 1.0, and then used our own tools to build
a genre-defining indie FPS that stands alongside the classics."

Target for this milestone: **B++ 1.x**, likely 1-3 months
after 1.0, depending on story scope.

---

## Architecture

### Window layout

```
┌──────────────────────────────────────────────────────┐
│ File  Edit  View  Build  Run  Help                   │  ← Menu bar
├──────────────────────────────────────────────────────┤
│ [Project] [Sprites] [Music] [Levels] [Code] [Run]    │  ← Tab strip
├──────────────────────────────────────────────────────┤
│                                                       │
│                                                       │
│                 ACTIVE PANEL                          │  ← Panel body
│                                                       │
│                                                       │
├──────────────────────────────────────────────────────┤
│ project: games/pathfind       Bang 9 v0.3            │  ← Status bar
└──────────────────────────────────────────────────────┘
```

Fixed 1024×768 window in early versions. Resizable in v0.5+.

### Panels

| Tab | Role | Source |
|-----|------|--------|
| Project | File tree + asset overview | native to Bang 9 |
| Sprites | Pixel art editing | embeds `tools/modulab/` |
| Music | Audio synthesis / composition | embeds `tools/mini_synth/` |
| Levels | Tilemap / scene editing | embeds `tools/level_editor/` (to build) |
| Code | Acme-style text editor | native to Bang 9 |
| Run | Build output + running game view | native to Bang 9 |

Future panels (as tools mature): Font (embeds `tools/font_forge/`),
Dialog (embeds `tools/dialog_editor/`), Cutscene (embeds
`tools/cutscene_editor/`), Debug (embeds `bug`).

### The plugin contract

Every tool in `tools/` must expose a standardized API so it
can run both standalone and as a Bang 9 panel:

```b++
// Init — called once when the panel is mounted.
void <tool>_init(proj_root, w, h);

// Update — called every frame with delta time.
void <tool>_update(dt_ms);

// Draw — called every frame to render into a (x,y,w,h) rect.
// The tool does NOT own the window; it draws into a region.
void <tool>_draw(x, y, w, h);

// Input event — called when the panel is active and user
// interacts. Returns 1 if the event was consumed.
<tool>_input(evt_type, evt_data);

// Shutdown — called when the panel is unmounted or Bang 9
// exits.
void <tool>_shutdown();
```

Each tool also ships a thin `<tool>.bpp` wrapper for standalone
mode that:

- Calls `game_init()` to own a window.
- Calls the same init/update/draw/input functions from the
  game loop.
- Translates absolute mouse coordinates to panel-relative.

This mirror-API is the entire architectural burden: one
refactor per tool, once. From then on, the tool is a first-class
citizen of both worlds.

### Project system

A Bang 9 project is a folder with a `bang.json` at the root (or
falls back to using the folder name and contents if the JSON is
absent). The JSON describes:

- Project name, version, description
- Entry point (`games/pathfind/pathfind.bpp`)
- Asset directories (sprites, music, levels, fonts)
- Build configuration (optimization flags, target platform)
- Last-used panel state (which tab was open, cursor position,
  etc.)

Projects can be opened via File → Open or by passing the path
on the command line:

```
./bang9/build/bang9 games/pathfind
```

Or eventually via a launcher UI that remembers recent projects.

### Build + run

When the user hits **Build** (Ctrl+B), Bang 9 invokes `bpp` as
a subprocess, captures stdout/stderr, and renders the output
in the Run panel. Compilation errors are clickable — clicking
jumps to the Code panel with the offending file open at the
right line.

When the user hits **Run** (Ctrl+R), Bang 9 builds if needed,
then executes the resulting binary. The game runs in a
separate window (initially) or in an embedded render target
(future optimization).

**Ctrl+F5** launches under the `bug` debugger, attaching the
Debug panel (future work).

---

## Platforms

| Platform | Status | Priority |
|----------|--------|----------|
| macOS (Apple Silicon) | Target for MVP | Now |
| macOS (Intel) | Should work, untested | Medium |
| Linux x86_64 | Works via X11 | Before 1.0 |
| Linux aarch64 | Should work, untested | Low |
| Windows x86_64 | **Not yet — major work** | Strategic requirement for artist audience |
| Linux ARM (Raspberry Pi) | Possible | Low |

Windows is the critical strategic platform because the artists
Bang 9 is ultimately for predominantly use Windows. Shipping
Bang 9 without Windows support limits the addressable audience
to roughly 20% of the target market.

Windows support requires:

- PE binary writer (replacing Mach-O / ELF writers) — ~1500 LOC
- Win32 platform layer (window, input, audio, file I/O) — ~2000 LOC
- DirectX backend or software renderer fallback — ~1500 LOC
- Cross-compilation from Mac/Linux — build system work

Rough estimate: 6-10 weeks of focused work. Scheduled for
post-1.0, possibly as 1.1.

---

## Business model

**Deferred. Decision made when Bang 9 v1.0 exists and has real users.**

Current options being considered:

- (A) All free + sponsorship/consulting revenue (Godot model)
- (B) B++ free + Bang 9 one-time purchase €29-79 (Aseprite /
  PureBasic model)
- (C) Open core (free base, paid premium features — team
  collab, cloud sync, etc.)

The decision is **not** urgent. Making it now — before Bang 9
exists as a product — would be optimizing imaginary revenue
against imaginary users. The pragmatic path: ship free beta
initially, gather real usage data, decide based on empirical
signal 6-12 months later.

Licensing and architectural decisions made now are chosen to
**preserve optionality**. We do not paint ourselves into a
corner on license terms, repo structure, or distribution
channels. Everything can pivot when the time comes.

---

## Non-goals

- **Not Unity.** Bang 9 does not aspire to 3D authoring, cloud
  services, asset marketplaces, or team collaboration features.
- **Not a general-purpose IDE.** Programmers who want deep
  refactoring, full LSP support, and multi-language editing
  should use their existing tools. Bang 9's code editor is
  Plan 9 acme-style — enough for pontual tweaks, not a
  Vim/Emacs replacement.
- **Not a web platform.** Bang 9 is native desktop only. No
  browser version, no cloud editing.
- **Not cross-engine.** Bang 9 produces B++ games. It does not
  export to Unity, Godot, or other engines. If that need
  arises, a separate export tool is the answer, not a Bang 9
  extension.

These non-goals keep the scope tractable and the product focused.

---

## How this document evolves

This vision is durable but not frozen. Significant shifts —
changes to target audience, pivots in the 1.0 scope, major
architectural rewrites — should be recorded here with a date
and rationale. Minor details (panel names, specific API
signatures, rough timelines) are allowed to drift in
implementation without requiring document updates.

A reader opening this document a year from now should be able
to answer three questions from it alone:

1. What is Bang 9 trying to be?
2. Who is it for?
3. Why is it architected this way?

If any of those answers change, this document is the place
that change is recorded.

---

*Document version: 0.1 — 2026-04-20*
*Author: Daniel Obino / BangDev*
*Status: strategic north star, active*
