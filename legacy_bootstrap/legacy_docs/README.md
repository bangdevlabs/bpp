# legacy_docs — Source Material Being Absorbed

These documents are the **source material** for the unified `docs/how_to_dev_b++.md` book. They remain read-only during the consolidation faxina. Nothing here is deleted until every section is marked **ABSORBED** below.

Why a legacy folder: previously 15 fragmented docs existed, each holding a piece of the truth. Readers had to know which one to consult. The faxina unifies everything into a single book (28 chapters, 6 parts). This folder is the construction ramp — references live here while the book assembles.

## How to use this folder

- **Before deleting any file here**: every section in the absorption map below must be marked ABSORBED.
- **When writing a chapter** in `docs/how_to_dev_b++.md`: open the legacy source noted in the chapter's `Source:` header, read alongside, write the chapter body, mark the source section ABSORBED in this README.
- **When `docs/how_to_dev_b++.md` is COMPLETE on every chapter**: this folder can be deleted from the repo (git history preserves it forever).

## Absorption map

Each legacy doc below lists the chapter(s) in the new book that absorb its content. Status per section: `PENDING` / `DRAFT` / `ABSORBED`.

### the_b++_programming_language.md (50 KB — the language reference)

| Source section | New book destination | Status |
|----------------|----------------------|--------|
| §1 Introduction | Cap 1 Hello World | PENDING |
| §2 Syntax basics | Cap 3 Syntax | PENDING |
| §3 Types | Cap 4 Types | PENDING |
| §4 Strings | Cap 6 Strings | PENDING |
| §5 Arrays + data structures | Cap 7 Arrays, Cap 10 Structs, Cap 11 Enums | PENDING |
| §6 stb modules | Cap 6-13 (Arsenal) | PENDING |
| §7 Compiler overview | Cap 21-25 (Compiler architecture) | PENDING |

### how_to_dev_b++.md (42 KB — the dev guide)

| Source section | New book destination | Status |
|----------------|----------------------|--------|
| Part 1 The Daily Loop (bootstrap) | Cap 18 Bootstrap + Cap 19 Canary | PENDING |
| Part 2 Tonify Expert Mode | Cap 14 Tonify | PENDING |
| Part 3 Architectural Discipline | Cap 15 Disciplina QG (with new QG framing) | PENDING |
| Part 4 Writing Modules | Cap 16 Idioms | PENDING |
| Part 5 File Order and Sweep | Cap 19 Canary Discipline | PENDING |
| Part 6 Testing | Cap 20 Testing | PENDING |
| Part 7 Cross-Compile and Deploy | Cap 25 Adding a new backend | PENDING |
| Part 8 Compiler Flags Reference | Appendix A | PENDING |
| Part 9 Recovery | Appendix B | PENDING |

### Bang 9 cluster

| Legacy doc | New book destination | Status |
|------------|----------------------|--------|
| bang9_vision.md | Cap 26 Bang 9 IDE | PENDING |
| bang9_tool_embed.md | Cap 27 Tools embed pattern | PENDING |
| bang9_factory.md | Cap 27 Tools embed pattern | PENDING |
| bang9_live_preview.md | Cap 27 Phase 4 future | PENDING |

### Plans + history

| Legacy doc | New book destination | Status |
|------------|----------------------|--------|
| roadmap_to_1_0.md | Cap 28 Roadmap (summary), rest archived | PENDING |
| games_roadtrip.md | Cap 28 Roadmap (excerpt) | PENDING |
| max_plan_codegen_split.md | Cap 23 Battalions (historical context) | PENDING |
| phase_backend_closeout.md | Cap 22-23 (historical context) | PENDING |
| phase_final_activation.md | Cap 22-23 (historical context) | PENDING |
| bangscript_plan.md | ARCHIVE (superseded vision) | PENDING |
| debug_with_bug.md | Cap 24 Runtime primitives (bug debugger subsection) | PENDING |
| snake_report.md | ARCHIVE (one-off debugging session) | PENDING |
| warning_error_log.md | ARCHIVE (working notes) | PENDING |

## Status legend

- **PENDING** — not yet read for absorption, content still exclusively in legacy doc
- **DRAFT** — chapter scaffolded in new book, partial content copied, pending review
- **ABSORBED** — content fully captured in new book; legacy section is now informational-only

## When to delete this folder

All rows above must be ABSORBED. Run:

```
grep -c "PENDING\|DRAFT" legacy_docs/README.md
```

Must return `0`. Then the folder can be deleted with `git rm -r legacy_docs/`.
