# B++ Governance

This document describes how decisions are made in the B++ project.
The goal is transparency: contributors and users should know,
up front, who decides what and how that decision-making may evolve
over time.

## Current model: BDFL

B++ is currently governed by **Daniel Obino** ("BDFL" — Benevolent
Dictator For Life), who holds final authority on:

- **Language semantics and syntax** — what B++ programs mean,
  what compiles and what does not, what keywords exist.
- **Compiler internals** — backend architecture, codegen strategy,
  target platforms, optimization passes.
- **Standard library (stb/) scope** — which modules belong in
  stb, which APIs are public, what guarantees they make.
- **Tools under tools/** — which first-party tools are maintained
  as part of the ecosystem, their scope, and their design.
- **Acceptance of external contributions** — pull requests are
  reviewed and merged (or rejected) at the BDFL's discretion.
- **Release schedule** — when versions are tagged, what the
  version numbering convention means.

The BDFL model is **explicit and durable** — the current practice
and the foreseeable future. Contributors submitting patches to
B++ should understand they are contributing to a project whose
direction is set by one person.

## How decisions happen

For routine changes (bug fixes, small improvements, new stb
functions that fit existing conventions):

1. Open a pull request with the change.
2. BDFL reviews; merges or requests changes.
3. No RFC required for ordinary fixes.

For **significant changes** (new language features, breaking
changes, architectural shifts, new pillars in stb, strategic
redirections):

1. Open a GitHub issue tagged `rfc:` describing the proposal,
   motivation, alternatives considered, and expected impact.
2. Public discussion period — minimum **one week**, longer if
   the proposal is contentious.
3. BDFL posts a decision publicly on the issue: accept, reject,
   or request revision.
4. If accepted, a PR implements the change. It is reviewed and
   merged normally.
5. Decisions are recorded in `docs/decisions/` as short
   architectural decision records (ADRs).

## What contributors can expect

- **Transparency.** Rationale for significant decisions is
  written down publicly.
- **Acknowledgment.** Contributions are credited in commit
  messages and in the CHANGELOG when significant.
- **Responsiveness.** PRs get an initial response within a
  reasonable time (days, not weeks). Complex review may take
  longer but you will not be ignored.
- **Honesty.** If a contribution is not going to be accepted,
  you'll hear that clearly, with reasons, rather than silence.

## What contributors should **not** expect

- **Unanimity.** The BDFL may reject changes that a majority of
  discussion participants endorse, if the change is incompatible
  with the project's direction or values.
- **Backwards compatibility guarantees** beyond explicit release
  notes. B++ is still evolving.
- **Feature-request-as-commitment.** Filing an issue with a
  feature request does not create an obligation to implement it.

## Future evolution

As the project grows, the governance model may evolve:

### Stage 1 — BDFL (current)

One person decides. Simple, coherent, fast. Works for a project
of this size and maturity.

### Stage 2 — Core Team

Once the project has **three or more active, long-term
contributors** (say, each landing 20+ commits over 6 months),
a small core team may be formed. The BDFL remains as tiebreaker
but shares review and merge authority. Decisions on language
direction still flow through the RFC process, with the core team
as discussants of record.

### Stage 3 — Steering Committee / Foundation

If the ecosystem reaches significant scale — hundreds of regular
contributors, multiple commercial users, long-term sustainability
concerns — a formal steering committee with term limits and
voting procedures may be established. At that stage a non-profit
foundation (similar to the Rust Foundation, Python Software
Foundation, or Linux Foundation) could hold the trademarks,
manage project finances, and codify the governance in formal
bylaws.

These transitions are **aspirational**, not promises. The
current BDFL model is the operating mode and no timeline for
transition is committed.

## Relationship to Bang 9 and commercial products

This governance document covers **B++** — the language, the
compiler, and the open tools under `stb/`, `tools/`, `games/`,
and `docs/`.

**Bang 9** is a proprietary product of BangDev, governed
separately and without community input into its feature set
or direction. Changes to B++ itself, however, are bound by
this document and cannot be influenced by Bang commercial
priorities without following the RFC process.

This means:
- Bang 9 cannot force a feature into B++ by corporate fiat.
  Any such feature must go through the RFC process as a
  public B++ change, with the usual review period.
- Any B++ change proposed primarily to benefit Bang must
  disclose that motivation in the RFC.
- The community can trust that B++ as a language is not
  going to be silently warped to serve a commercial product
  without them knowing.

## Code of conduct

Contributors are expected to act with respect, patience,
and good faith. Harassment, discrimination, personal attacks,
spam, and bad-faith engagement are not tolerated. The BDFL
has final authority on moderation decisions.

This is a working project run by people. Be kind.

---

*Last updated: 2026-04-20*
