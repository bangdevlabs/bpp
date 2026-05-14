# Sidequest — `: u_word` / `: u_half` / `: u_quarter` / `: u_byte` annotations

**Status:** SUPERSEDED — see `sidequest_unsigned_annotations_retry.md`
for the canonical plan. Original encoding choices (TY_UNSIGNED_BIT
= 0x40 then bit 3 = 0x08 then kind 0x07) all collided with existing
type tags. The retry uses kind 0x0E (BASE_UWORD) and shipped
2026-05-14.
**Predecessor:** storage class sidequest (`global const` / `static
const`) shipped `c1430b9`. Same arc; surfaced the signed-int bit
split bug whose proper fix needs unsigned arithmetic.
**Position:** opt-in feature, zero impacto em código existente.

## Pre-execution audit (verified)

- Zero código existente usa `u_word`/`u_half`/`u_quarter`/`u_byte`/
  `TY_UNSIGNED_BIT`/`udiv`/`umod`/`lshr` — clean slate.
- Chip primitive table (`src/bpp_codegen.bsm`): slots 4/5/11 are
  signed (`emit_div` / `emit_mod` / `emit_shr` = SDIV / SMOD / ASR).
  3 new slots needed (`emit_udiv` / `emit_umod` / `emit_lshr`).
- ARM64 backend: `_a64_emit_div` uses SDIV, `_a64_emit_shr` uses
  ASRV. Need UDIV / LSRV variants. Encoders likely exist in
  `a64_enc.bsm`; verify `a64_enc_udiv` / `a64_enc_lsrv`.
- x86_64 backend: `_x64_emit_div` uses CQO+IDIV (signed). Need
  XOR RDX,RDX + DIV variant. SHR opcode `48 D3 E8` for unsigned
  shift. Encoders to verify in `x64_enc.bsm`.
- Type encoding (`src/bpp_defs.bsm:170-180`): low nibble = kind
  (0x2 word, 0x3 float, 0x4 ptr), high nibble = width
  (0=full, 1=half, 2=quarter, 3=byte), bit 0 = float flag. **Bit
  6 (0x40) is FREE.** Reserve as `TY_UNSIGNED_BIT`.
  - `TY_WORD | TY_UNSIGNED_BIT = 0x42` — u64
  - `TY_WORD_H | TY_UNSIGNED_BIT = 0x52` — u32
  - `TY_WORD_Q | TY_UNSIGNED_BIT = 0x62` — u16
  - `TY_WORD_B | TY_UNSIGNED_BIT = 0x72` — u8
  - `is_float_type` stays correct because bit 0 = 0 in all 4
    variants.

## Why this sidequest (Rule 28)

Killer use cases documented:
1. **Bit splitting of full word** — storage class sidequest surfaced
   `(val / 0x100000000) & 0xFFFFFFFF` rounding-toward-zero for
   negatives. Annotation eliminates the trap.
2. **Hash modulo** — `hash % buckets` produces negative when hash
   bit 63 is set. Manual `& 0x7FFF...) % buckets` workaround.
   Annotation cleans it up.
3. **Color channels, MIDI bytes, network protocol fields** — domain
   literal "unsigned bit pattern". Annotation documents intent.

Rule 28 verdict: 2+ real use cases. Not speculation.

## Design lock-in

### Naming: `u_` prefix family

| Annotation | Internal encoding | Equivale a C |
|------------|-------------------|--------------|
| `: word` (implicit) | `0x02` (TY_WORD) | int64_t |
| `: half` | `0x12` (TY_WORD_H) | int32_t |
| `: quarter` | `0x22` (TY_WORD_Q) | int16_t |
| `: byte` | `0x32` (TY_WORD_B) | int8_t |
| `: u_word` | `0x42` | uint64_t |
| `: u_half` | `0x52` | uint32_t |
| `: u_quarter` | `0x62` | uint16_t |
| `: u_byte` | `0x72` | uint8_t |

**No `: u` shorthand** — always explicit width. C/Rust/Zig industry
pattern adapted to B++'s width-named family.

### Semantics affected

| Operator | Signed default | Unsigned variant |
|----------|----------------|------------------|
| `x / y` | SDIV (rounds toward zero) | UDIV (bit-correct) |
| `x % y` | sign of dividend | always positive |
| `x >> n` | ASR (sign-extend) | LSR (zero-fill) |

**Unchanged:** `+`, `-`, `*` (overflow wraps either way), `<<`
(left shift zero-fills), `&`, `|`, `^`, `~` (bit ops), `<`, `>`
(stay signed), `==`, `!=`.

**Implicit signed↔unsigned conversion: proibida.** Error if
`auto x: word; x = y` where `y: u_word`. Force explicit cast or
matching annotation.

## Implementation plan (8 blocos sequenciais)

Cada bloco verificável isoladamente. Bootstrap byte-stable como gate.

### Bloco 1 — Type system: TY_UNSIGNED_BIT flag (~15 LOC)

Files:
- `src/bpp_defs.bsm` — add constant
- `src/bpp_internal.bsm` — add `is_unsigned_type(ty)` + `ty_width(ty)` helpers

```bpp
// In bpp_defs.bsm init_defs:
TY_UNSIGNED_BIT = 0x40;
```

```bpp
// In bpp_internal.bsm:
is_unsigned_type(ty) {
    return (ty & TY_UNSIGNED_BIT) != 0;
}
ty_width(ty) {
    return ty & 0x3F;
}
```

Gate: bootstrap byte-stable + suite 167/0/12 + 130/0/45.

### Bloco 2 — Parser annotation handling (~15 LOC)

Files:
- `src/bpp_parser.bsm:try_type_annotation` — 4 new keyword branches

```bpp
if (buf_eq(ts, "u_word", 6))    { _prim_hint = TY_WORD   | TY_UNSIGNED_BIT; consume(); return; }
if (buf_eq(ts, "u_half", 6))    { _prim_hint = TY_WORD_H | TY_UNSIGNED_BIT; consume(); return; }
if (buf_eq(ts, "u_quarter", 9)) { _prim_hint = TY_WORD_Q | TY_UNSIGNED_BIT; consume(); return; }
if (buf_eq(ts, "u_byte", 6))    { _prim_hint = TY_WORD_B | TY_UNSIGNED_BIT; consume(); return; }
```

Gate: bootstrap + suite + smoke `auto x: u_word;` parses without error.

### Bloco 3 — Spine smart dispatch + AST node tags (~50 LOC)

Files:
- `src/bpp_defs.bsm` — add `T_UDIV`, `T_UMOD`, `T_LSHR` AST tags
- `src/bpp_types.bsm` — extend binop walk with unsigned dispatch

```bpp
// LHS-only rule: when LHS carries TY_UNSIGNED_BIT, rewrite op tag.
// Mixed-operand (signed LHS + unsigned RHS or vice versa) is the
// E0XX path — defer enforcement until first real case.
if (is_float_type(lhs_ty)) {
    // existing float dispatch path
} else if (is_unsigned_type(lhs_ty)) {
    if (binop.op == T_DIV) { binop.op = T_UDIV; }
    if (binop.op == T_MOD) { binop.op = T_UMOD; }
    if (binop.op == T_SHR) { binop.op = T_LSHR; }
}
```

Gate: bootstrap + suite + smoke shows `T_UDIV` not `T_DIV` in AST.

### Bloco 4 — Chip primitives (~80 LOC across 2 backends)

**Anti-pattern warning:** during this bloco the agent will write
`_a64_emit_udiv` and `_x64_emit_udiv` side-by-side. There is **no
shared structure** to factor — ARM does UDIV directly (1 instr),
x64 does XOR RDX,RDX + DIV (2 instr). Don't abstract. Each chip's
3 functions are 1-4 LOC each, totally different.

Files:
- `src/bpp_codegen.bsm` — 3 new primitive slots in `p` struct
- `src/backend/chip/aarch64/a64_primitives.bsm` — 3 emit functions
- `src/backend/chip/aarch64/a64_codegen.bsm` — register primitives
- `src/backend/chip/aarch64/a64_enc.bsm` — add UDIV / LSRV encoders if missing
- `src/backend/chip/x86_64/x64_primitives.bsm` — 3 emit functions
- `src/backend/chip/x86_64/x64_codegen.bsm` — register primitives
- `src/backend/chip/x86_64/x64_enc.bsm` — add XOR_reg / DIV (unsigned) / SHR encoders if missing

**a64:**
```bpp
void _a64_emit_udiv(left_reg) {
    a64_enc_udiv(0, left_reg, 1);
}
void _a64_emit_umod(left_reg) {
    a64_enc_udiv(2, left_reg, 1);
    a64_enc_msub(0, 2, 1, left_reg);
}
void _a64_emit_lshr(left_reg) {
    a64_enc_lsrv(0, left_reg, 1);
}
```

**x64:**
```bpp
void _x64_emit_udiv(left_reg) {
    x64_enc_xor_reg(2, 2);
    x64_enc_div(1);
}
void _x64_emit_umod(left_reg) {
    x64_enc_xor_reg(2, 2);
    x64_enc_div(1);
    x64_enc_mov_reg(0, 2);
}
void _x64_emit_lshr(left_reg) {
    x64_enc_shr_reg_cl();
}
```

**Codegen tree walk:** in `src/bpp_codegen.bsm`, T_DIV / T_MOD / T_SHR
dispatch site gains T_UDIV / T_UMOD / T_LSHR branches calling
`p.emit_udiv` etc.

Gate: bootstrap + suite + native smoke `auto x: u_word; x = -1947;
auto hi; hi = x / 0x100000000;` → `hi == 0xFFFFFFFF`.

### Bloco 5 — C-emit path (~30 LOC)

File: `src/backend/c/bpp_emitter.bsm`

```c
// Signed (today):
//   (int64_t)x / (int64_t)y       for T_DIV
// Unsigned (new):
if (op == T_UDIV) {
    cw_w("((uint64_t)("); emit_expr(lhs); cw_w(") / (uint64_t)("); emit_expr(rhs); cw_w("))");
}
```

Gate: bootstrap + C-emit suite + `--c` smoke compiles + runs returning 0.

### Bloco 6 — W032 diagnostic (~30 LOC)

File: `src/bpp_validate.bsm`

Detect `T_DIV` where RHS is power-of-2 literal AND LHS is NOT
unsigned. Emit W032 with helpful suggestion.

```
warning[W032]: division by power-of-two literal on signed value
  ... mo_w32((_gd_val / 0x100000000) & 0xFFFFFFFF);
  = help: signed division rounds toward zero — for negatives smaller
         in magnitude than the divisor, the quotient is 0 (loses high bits)
  = help: if you intend bit-pattern extraction, use `(x >> 32) & 0xFFFFFFFF`
  = help: if you intend unsigned arithmetic, annotate `auto x: u_word;`
  = help: see Tonify Rule 32 (unsigned annotations)
```

Register in `docs/warning_error_log.md`.

Gate: bootstrap + suite + xfail test fires W032.

### Bloco 7 — Audit sweep tracker (~doc only)

Add entry to `docs/todo.md` listing greps + candidate cartridges
(stbpal, stbmidi, stbimage, bpp_hash, bpp_minisym, etc). NÃO MIGRAR
— per-cartridge audit + Rule 28 + commit later.

### Bloco 8 — Tonify Rule 32 + doc updates (~doc only)

Files:
- `docs/tonify_checklist.md` — add Rule 32 (decision matrix for `: u_*`)
- `docs/tonify_checklist.md` Rule 13 — add unsigned param annotation line
- `docs/how_to_dev_b++.md` Cap 4 — add §4.7 Unsigned annotations
- `docs/warning_error_log.md` — register W032 row

## Test plan (5 new tests)

1. `tests/test_u_word_basic.bpp` — 4 widths parse + store roundtrip
2. `tests/test_u_word_div_unsigned.bpp` — UDIV proves bit-correct
3. `tests/test_u_word_mod_unsigned.bpp` — UMOD always positive
4. `tests/test_u_word_lshr.bpp` — LSR zero-fills (vs ASR sign-extends)
5. `tests/test_W032_signed_div_pow2.bpp` — xfail W032

## Out of scope (Rule 28)

- `: u` shorthand
- Implicit signed↔unsigned conversion error E0XX (defer until real case)
- Unsigned comparisons `<`, `>` (defer until real case)
- `: u_float` (IEEE has signed bit)
- Migration sweep (separate per-cartridge commits)
- Function-call RHS in `const` initializer

## Estimated effort

| Bloco | LOC | Tempo |
|-------|-----|-------|
| 1 — TY_UNSIGNED_BIT | ~15 | 30min |
| 2 — Parser keywords | ~15 | 30min |
| 3 — Smart dispatch | ~50 | 2h |
| 4 — Chip primitives | ~80 | 3h |
| 5 — C-emit | ~30 | 1h |
| 6 — W032 | ~30 | 1h |
| 7 — Audit tracker | doc | 30min |
| 8 — Rule 32 + docs | doc | 1h |
| **Total** | **~220 LOC** | **9-11h** |

## Spine portability — explicit

Same DNA as storage class sidequest's `glob_slot_init_value`
refactor:

| Layer | Responsibility | This sidequest |
|-------|----------------|----------------|
| Spine | DECIDE which variant | Detects `is_unsigned_type(lhs)`, rewrites T_DIV → T_UDIV |
| Chip | EMIT chip-specific bytes | UDIV vs SDIV vs DIV+XOR — chip-different |
| Target | ZERO IMPACT | Operators don't touch file format |

When RISC-V / WASM / Vulkan SPIR-V arrives: implement 3 new chip
primitives (~30 LOC). Spine + parser + types + C-emit + W032 stay
untouched. Single-touch-point per new backend.

**During Bloco 4: do NOT abstract the 3 a64 / 3 x64 functions.** No
shared structure exists — each is a different opcode pattern.
"Abstracting" creates a fake layer that future backends would also
need to touch.

## Commit shape (single commit)

```
B++: add :u_word / :u_half / :u_quarter / :u_byte annotations with
unsigned smart-dispatch ops + W032 diagnostic

Adds opt-in type hints that route /, %, >> through unsigned variants
without programmer needing to write udiv/umod/lshr manually. Storage
stays signed word internally (no new type hierarchy); single bit flag
TY_UNSIGNED_BIT (0x40) OR'd into existing width type code.

Triggered by signed-vs-unsigned bug surfaced during storage class
sidequest (commit c1430b9). Killer use cases per Rule 28: bit
splitting + hash modulo. Future MIDI bytes, color channels, network
protocol fields reuse the annotation.

[full body per spec]
```
