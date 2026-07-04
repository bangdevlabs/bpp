# Plan — T2 inc 2: the TY_STR / TY_PTR split (str-aware promotion)

**Status:** Increments A-C SHIPPED 2026-07-04. D (flip bare-PTR put
dispatch) + E (param str inference from call sites — the utopia's next
step, kills the `path: ptr` annotation class) remain gated.

**Design decisions locked during landing:**
- TY_STR = TY_PTR | 0x100 (flag bit 8, NOT a new base) — the base nibble
  has no free non-float values (`is_float_type` is `ty & 1`), and riding
  BASE_PTR gives width/arith/is_ptr_type/promote for free; only dispatch
  reads the flag.
- **String-purity guard**: STR promotion requires every store to be str
  (VtEntry.nonstr) — `s = 0;`, the universal release idiom, is a word
  store that must never turn a compiler-written hint into E053. Float
  never needed this (int→float CONVERTS; word→str CONFLICTS). Found the
  hard way: the bug binary's own sources stopped compiling until the
  guard landed.
- Two-stage bootstrap (capability first, prelude use second) because the
  installed compiler must parse `-> str` before bpp_str.bsm may use it.
- The doctrine (user's framing): annotations exist as POWER, the culture
  uses them only at meaning boundaries — str enters once at a producer's
  signature or a literal and FLOWS; locals never annotate.

## Why (the design finding that reopened inc 2)

Inc 2 ("promote `: ptr` locals") was gated because promotion would have
changed living programs — and the probe showed the deeper truth: **`: ptr`
conflates two meanings.** In practice it means "C string" (paths, labels —
put dispatches putstr), but malloc/buf results are opaque pointers whose
useful print is the ADDRESS. Probed reality (2026-07-04):

- string literal → bare local → put: putstr already (literal carries the type);
- `str_dup(...)` → bare local → put: prints the ADDRESS (str-ness lost — the
  producer's return is unannotated);
- `malloc(...)` → bare local → put: address (correct!), plus W032 noise —
  and this is safe TODAY only because malloc infers WORD; annotating it
  `-> ptr` would flip put to reading heap bytes as a string.

The fix is a real type: **TY_STR (BASE_STR = 0x05)** — "a pointer whose
bytes are a NUL-terminated string." `: ptr` stays for opaque pointers.

## Increments

- **A — the type exists, behavior-neutral:** BASE_STR/TY_STR in defs;
  string LITERALS type TY_STR; put/put_err dispatch putstr for STR (PTR
  branch unchanged — nothing breaks); promote() makes STR sticky through
  arithmetic (s+1 is still a string); is_ptr_type accepts BASE_STR (a str
  IS a pointer mechanically); val_type_name knows "str".
- **B — the annotation + producers:** `: str` / `-> str` parse like `ptr`;
  the canonical string producers gain `-> str` (str_dup, path_asset —
  wider adoption grows with consumers, Rule 20).
- **C — promotion:** the T2 sweep also writes TY_STR hints back (same
  guards). `auto d; d = str_dup(x); put(d);` → putstr, no W032.
- **D — the breaking cleanup (GATED):** flip bare TY_PTR's put dispatch
  from putstr to the address formatter once the repo's string-meaning
  `: ptr` annotations have migrated to `: str`. Needs its own migration
  sweep + Rule 28 review; NOT part of this arc's first landing.

## Verification

Per increment: bootstrap chain, native + C-emit suites, render md5
unchanged, probes for the three shapes above, new tests/test_str_type.bpp.
Zero-regression argument for A-C: STR is additive — every site that used
to see TY_PTR still sees TY_PTR (literals move to STR but every literal
consumer gains an STR branch in the same commit; put keeps putstr for both).
