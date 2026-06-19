# Plan — `bpp --tags`: a compiler-emitted symbol index (ctags / etags)

**Status:** design / proposed 2026-06-19. A "pseudo-LSP": editor go-to-definition
+ symbol outline for people writing b++, driven by the one component that already
knows every symbol and where it lives — **the compiler**. Sparked by the Tsoding
*CTags vs ETags* stream (extending `rluba/jai-ctags` to the Emacs format) and
Jonathan Blow's "the compiler is the source of truth for tooling" point. Same
thread b++ already pulls for `bug` (a debugger with no DWARF/GDB, reading the
compiler's own `.bug` map).

## Goal + the real consumer

A `tags` / `TAGS` index so an editor can jump to a definition (`arr_struct_at`,
`put`, `mixer_note_on`, a user's own function) and list a file's symbols. The
consumer is **real and present**: the user is starting to write b++ and wants
navigation while writing (this is what clears the Rule 28 gate — not speculative).
The eventual best home is **Bang 9** reading the same index natively for in-IDE
go-to-def / outline; the tags file is the interop bridge for external editors
(vim, Emacs, VS Code) in the meantime.

## What it is — and the honest ceiling

A tags index gives the ~80% of "LSP" people actually use:

- **go-to-definition** (`Ctrl-]` in vim, `M-.` in Emacs)
- **symbol outline / "jump to symbol in file"**
- **dumb name completion** (editors that complete from the tags set)

It does **not** give: find-all-references, hover types, rename, diagnostics,
context-aware completion. Those need either a richer compiler-emitted symbol DB
or a real LSP server (JSON-RPC + incremental reparse) — a much bigger surface,
deferred to "Phase 2+ if a real need shows up" (see below). MVP stays at tags;
that is the cheapest lever with the highest day-one payoff for *writing* code.

## Architecture — `bpp --tags`, parse-driven, reuse the bug resolver

Emit during a normal parse, as a sibling output mode to `--c` / `--asm` /
`--bug` / `--show-deps`. Rationale: the compiler has full fidelity (signatures,
kinds, scoping) and needs no `--bug` prerequisite, and the load-bearing
machinery already exists.

```
bpp --tags  main.bpp            # writes ./tags  (ctags format)  — default
bpp --etags main.bpp            # writes ./TAGS  (Emacs format)
bpp --tags=- main.bpp           # to stdout
```

Indexing scope = every symbol the compile pulls in: the user's program **plus
the auto-injected prelude plus any `import`ed stb** — i.e. exactly the symbols
reachable while writing that program. Indexing the whole compiler is then just
`bpp --tags src/bpp.bpp` (dogfood: navigate b++ in b++).

### What already exists (verified 2026-06-19) vs what to add

| Piece | State |
|---|---|
| token → (file, line, **byte offset**) resolver (`tok_lines` / `tok_src_off` / `tok_mod` / `diag_file_line_start`) in `bpp_bug.bsm` | ✅ exists — the `.bug` Source Map already uses it; `tok_src_off` is the etags byte offset |
| Functions carry a def token (`FN_SRC_TOK`) → line/offset/file resolvable | ✅ exists |
| Function signature (`FN_PARS`/`FN_PCNT`, return type) for the ctags `signature:` field | ✅ exists |
| Structs / globals / externs / consts / enums: names present (`sd_names`, `enum_*`, globals tables) | ⚠️ names yes, **per-symbol def token missing** — small parser additions mirroring `FN_SRC_TOK` (capture `tok_pos` at the definition site) |
| The tags emitter itself + `--tags`/`--etags` flag | ❌ new — `src/bpp_tags.bsm` (or fold into `bpp_bug.bsm`, which owns the resolver) + a flag in `bpp.bpp` |

So **functions-only ctags is nearly free** (the resolver + `FN_SRC_TOK` both
exist); each further symbol class costs one small "capture the def token" parser
hook.

## Output formats

**ctags** (`tags`, sorted for the editor's binary search):
```
!_TAG_FILE_FORMAT	2
!_TAG_FILE_SORTED	1
arr_struct_at	src/bpp_arr.bsm	/^arr_struct_at(a: Arr, idx) {$/;"	f	signature:(a: Arr, idx)
Arr	src/bpp_arr.bsm	/^struct Arr {$/;"	s
```
- `name <TAB> file <TAB> /^line text$/;" <TAB> kind [<TAB> ext:fields]`
- A **search pattern** address (`/^…$/`) survives edits better than a raw line
  number; build it from the resolved line via the source bytes (`tok_src_off`).
  Line-number addresses are the trivial fallback for S1.
- `kind`: `f` function, `s` struct, `m` struct member, `v` global/extern,
  `d` const, `g` enum, `e` enum member.

**etags** (`TAGS`, per-file sections, carries byte offsets):
```
\x0c
src/bpp_arr.bsm,<size>
arr_struct_at(a: Arr, idx) {\x7farr_struct_at\x01127,4521
```
- `pattern \x7f tagname \x01 line , byteoffset` — `byteoffset` comes straight
  from `tok_src_off[FN_SRC_TOK]`.

## Phasing (each step shippable + dogfoodable, gated like any change)

- **S0 — spike via `.bug` (optional, fastest proof).** A throwaway
  `bug --tags ./prog` that reformats the existing `.bug` Source Map → ctags,
  just to validate the format + editor wiring with ~zero new code. Throw away
  once S1 lands (or keep if a `.bug`-based path proves useful).
- **S1 — `bpp --tags`, functions only, ctags, search-pattern address.** Uses
  `FN_SRC_TOK` + the bug resolver. The first genuinely useful build — jump to
  any function definition (yours, prelude, stb). Verify on `src/bpp.bpp` (the
  compiler indexes itself) + a small user program.
- **S2 — structs + struct members + globals/externs/consts/enums.** Each needs
  the small "capture def token" parser hook. Now the whole symbol surface is
  navigable.
- **S3 — etags format** (`--etags`), byte offsets via `tok_src_off`.
- **S4 — ctags extension fields** (`signature:`, `kind`, `typeref:`) for editors
  that show them.
- **S5 — Bang 9 native consumer.** Bang 9 reads the same index (or calls the
  resolver directly) for in-IDE go-to-def + a symbol outline panel — the
  b++-native "pseudo-LSP", zero-dep, the `bug`-style model.
- **Phase 2+ (only if a real need shows up) — beyond tags.** find-references /
  hover would need a richer compiler-emitted symbol DB; a true LSP server is a
  separate, much larger project. Both are deferred behind a concrete consumer.

## Editor wiring (so it's usable while writing)

- **vim / neovim:** `:set tags=./tags` then `Ctrl-]` / `:tag name` / `:tselect`.
- **Emacs:** `M-x visit-tags-table` on `TAGS`, then `M-.`.
- **VS Code:** a ctags extension reads `tags`.
- **Helix / Zed:** these want an LSP, not tags — so for those users the Phase 2
  LSP path is what matters; note it honestly rather than pretend tags covers them.
- Generation: a `tests/`-style script or a `make tags` that runs `bpp --tags`
  over the project entry; re-run on save (or a file-watch, mirroring the
  hot-reload rail) keeps it fresh.

## Scope discipline

- **Rule 45 (extend before mint):** the resolver + `FN_SRC_TOK` already live in
  the compiler/`bug` layer — `--tags` is a new *view* of existing data, not a new
  subsystem. Prefer folding the emitter next to the `.bug` writer in
  `bpp_bug.bsm` (shared resolver) over a brand-new module unless it grows.
- **Rule 28 (killer use case):** the consumer is the user writing b++. Ship the
  smallest useful slice first (S1 functions-only) and grow per real friction —
  don't build the LSP server until tags is proven insufficient.
- **Rule 41 (additive / agnostic):** `--tags` is a pure output projection of the
  parse — no codegen, no backend, no bootstrap-byte-stability risk to the
  compiled binary (it short-circuits before codegen, like `--show-deps`).

## Cross-references

- `docs/manual/debug_with_bug.md` — the `.bug` map + `bug --dump` (the existing
  symbol/line projection this builds beside).
- `bpp_bug.bsm` — the token → (file, line, offset) resolver to reuse.
- `src/bpp_parser.bsm` — `FN_SRC_TOK` (done); `sd_names` / `enum_*` / globals
  (where the def-token capture hooks go).
- `docs/manual/how_to_dev_b++.md` Cap 48 — the compiler-flags family `--tags`
  joins.
- Reference: `github.com/rluba/jai-ctags` (the Jai equivalent); ctags format
  `ctags.sourceforge.net/FORMAT`; Emacs `TAGS` format.
