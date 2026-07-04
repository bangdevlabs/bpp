# Plan — T2: local float smart-promotion (typing evolution, stage 2)

**Status:** Increments 1 + 3 SHIPPED 2026-07-04. Increment 2 (`: ptr`) stays
gated — see the note in its bullet. Verified: gen1==gen2 byte-IDENTICAL (the zero-regression
property held at the strongest level), native 228/0/12, C-emit 189/0/51,
probe green on x64-in-Docker, bench_compile 0.30/0.31s (unchanged band),
audio md5 unchanged.

## Goal

`auto sr; sr = 44100.0;` should just work — inferred `: float`, no E232, no
annotation. Stage 2 of the typing doctrine ("infer the obvious, annotate the
ambiguous, refuse the unknown", journal 2026-07-03): the FIRST type-system
change whose effect is REMOVING annotations from user code. The boçalidade
gate: annotations-per-KLOC in the conformance corpus must never rise; T2
lowers it.

## The mechanism (why this is cheap)

The compiler already has both halves; they were never wired together:

- **Types half:** `add_type`'s T_ASSIGN already calls
  `ty_set_var_type(lhs, rhs_ty)` — the vt table learns a bare local is float
  at the first store. But that channel only feeds smart dispatch/validate.
- **Codegen half:** slot layout reads the DECL's per-name hint array
  (`n.e[j]` via `cg_decl_var_hint`, the Rule 42 channel). Validate's E232
  reads the same array (`_val_get_hint` from the T_DECL scan).

**T2 = at end-of-function inference (save_fn_types), write the inferred
TY_FLOAT back into the decl's hint slot.** The poke IS the annotation:
E232 stops firing (hint_ty > 0), native codegen allocates a d-register
float slot, the C emitter declares `double`, W032/put dispatch float — all
downstream behaves as if the user typed `: float`. No validate change, no
new syntax, no new diagnostic.

## Safety properties

1. **Zero regression surface by construction:** today ANY float store into a
   bare local is E232-fatal, so code that compiles has no promotion
   candidates — T2 only legalises previously-rejected programs. Compiler
   self-compile must stay byte-identical (the strongest gate).
2. **Conservative disqualifiers (v1):** explicit annotation (locked), the
   local's address taken (`&x` — promotion must not silently change what
   the bytes at &x mean), struct-typed locals (parser struct table wins),
   entries with no decl hint slot (params, pre-decl assignments), vt type
   anything but exact TY_FLOAT (slices stay explicit).
3. Mixed stores (`x = 3.14; x = 5;`) promote to float — identical to the
   behaviour of an explicit `: float` local receiving an int (scvtf).

## Increments

1. **Float locals** (this increment): parser always attaches the per-name
   hint array (`node.e = dh_arr` unconditionally); VtEntry grows
   `hslot`/`ataken`; T_DECL records the hint-slot address; T_ADDR marks
   address-taken; save_fn_types runs the promotion sweep. ← SHIPPED
2. **`: ptr` from `-> ptr` flows** — GATED, and now with the concrete
   reason: unlike float (where E232 made bare candidates impossible),
   `auto p; p = malloc(64);` is LEGAL today, so ptr promotion would change
   put()/W032 dispatch in living programs — it breaks the zero-regression
   property increments 1+3 were built on. Needs its own arc (likely a
   W-diagnostic first). Rule 28 evidence required.
3. **Multi-assign targets** — SHIPPED 2026-07-04, same commit family as
   inc 1. add_type's T_ASSIGN multi path types EVERY target from the
   callee's DECLARED ret-slot type (get_fn_ret_type_at), then the same
   save_fn_types sweep promotes the float ones. Before: targets 2..N of a
   bare multi-assign silently read the WRONG register bank. gen1==gen2
   byte-identical again (the compiler has no bare float multi-assigns).

## Verification per increment

Bootstrap byte-stable (expected byte-IDENTICAL gen for inc 1 — no candidates
in the compiler), native + C-emit suites, audio md5 unchanged, new
tests/test_local_float_promotion.bpp (positive: bare local promotes, put
dispatches float, C-emit declares double; negative: addr-taken still E232,
struct-typed still E232, param unaffected, `: word` consent-truncation
unaffected).
