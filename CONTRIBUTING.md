# Contributing to B++

Thanks for considering a contribution. A few things to know
before you start.

## What this project is

B++ is a self-hosting compiled language with a game-dev focused
standard library and toolset. The design values are:

- **Zero external dependencies** — no FFI, no package manager,
  no runtime libraries beyond what ships in `src/`.
- **Self-hosted** — the compiler is written in B++ itself.
- **Minimalism** — fewer, bigger pillars beat many small
  fragmented modules. Changes that multiply file count or API
  surface need strong justification.
- **Hackable at every level** — source is readable, conventions
  are documented, no hidden state.
- **Game-dev first, general-purpose allowed** — stb is tuned for
  games but the language itself is general.

Contributions that align with these values have a much better
chance of being accepted.

## Governance

Decisions on language direction are made by the BDFL
(see `GOVERNANCE.md`). For significant changes, follow the RFC
process described there. Small fixes go straight to PR.

## Before you start significant work

**Open an issue first.** If your change is non-trivial — new
language feature, breaking API change, new stb module, large
refactor — discuss the direction before writing code. This
saves both sides from rejected-PR frustration.

For small fixes (bug in existing code, typo, doc tweak) go
straight to PR.

## How to submit a PR

1. Fork the repository.
2. Create a branch with a descriptive name (`fix-parser-E204`,
   `stb-new-particle-system`).
3. Make your change. Keep it focused — one concern per PR.
4. **Run the full test suite** (`sh tests/run_all.sh`). All
   tests must pass before the PR is reviewed.
5. **Bootstrap the compiler** if you touched `src/` — see the
   workflow in `docs/how_to_dev_b++.md`. The two rebuild
   shasums must match.
6. Commit with a clear message. First line describes the
   change; body explains why if non-obvious.
7. Push and open a PR. Fill in the PR template.

## Coding conventions

All new code follows the tonification rules in
`docs/how_to_dev_b++.md`. Briefly:

- **Comments in English**, complete sentences, user-manual
  style. Explain **why**, not just **what**.
- **Function naming**: public stb functions are prefixed by
  the module (`path_new`, `hash_set`). Private helpers start
  with underscore (`_path_heap_swap`).
- **No `!` operator** — use `== 0` instead.
- **Reserved keywords**: `byte`, `bit`, `half`, `word`,
  `ptr`, `base` cannot be used as variable names. See the
  full list in the language reference.
- **Type hints are opt-in performance tuning**, not default.
  Use `: base` for pure functions, `: gpu` for GPU-only
  paths, etc. See `docs/how_to_dev_b++.md` Part 2, Rule 4.

## Tests

- New stb module or feature → **write a test**
  (`tests/test_*.bpp`). No exception.
- Bug fix → **write a test that fails before your fix**
  (and passes after). Regression prevention is not optional.
- Breaking change → update all affected tests in the same PR.

## Platforms

B++ targets macOS (native) and Linux (X11 or terminal
fallback). Both must compile clean. Linux testing runs
through a Docker flow — see `docs/how_to_dev_b++.md` Part 7.
If you only have one platform available, open the PR with a
note; we'll test the other before merge.

## License of contributions

By submitting a pull request, you agree that your contribution
will be licensed under Apache 2.0 (the project's license) and
that you have the right to make the submission.

For substantial contributions, a Developer Certificate of
Origin (DCO) sign-off may be requested — equivalent to
appending `Signed-off-by: Name <email>` to your commit
messages. This is a lightweight alternative to a full CLA.

## Communication

- **Bug reports / feature requests** → GitHub issues
- **Design discussions / RFCs** → GitHub issues tagged
  `rfc:`
- **Security issues** → email the BDFL privately first
  (contact in project README). Do not open a public issue for
  vulnerabilities.

## One-line summary

Make it small, test it, explain why, be kind. Welcome aboard.

---

*Last updated: 2026-04-20*
