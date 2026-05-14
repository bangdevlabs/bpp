# Sidequest — `: u_word` / `: u_half` / `: u_quarter` / `: u_byte` (RETRY)

**Status:** SHIPPED 2026-05-14 — kind 0x0E (BASE_UWORD), 4 smoke tests
landed (`test_unsigned_div`, `_mod`, `_shr`, `_arith_shared`),
Tonify Rule 32 + how_to_dev §4.6 added, suite 171/0/12 native +
136/0/47 C, bootstrap byte-stable.
**Predecessor:** `docs/sidequest_unsigned_annotations.md` (plan original).
**Diferença chave:** o slot livre no encoding é **kind 0x0E**, NÃO bit 3
(0x08) nem bit 6 (0x40) nem kind 0x07. As duas tentativas anteriores
escolheram o slot errado.

---

## Por que a primeira tentativa abortou (post-mortem honesto)

### Erro 1 (plan original, bit 6 = 0x40)
Colide com TY_WORD_BIT family que vive no **slice nibble** (high), não
no kind nibble:
- TY_WORD_BIT = 0x42 (slice=0x4, base=0x2)
- TY_WORD_BIT7 = 0xA2 (slice=0xA, base=0x2)

Reservar bit 6 do byte inteiro = bit 2 do slice nibble → conflita com
SL_BIT (slice 4 = `0100`), SL_BIT2 (5 = `0101`), SL_BIT4 (7 = `0111`),
SL_BIT6 (9 = `1001`), SL_BIT7 (10 = `1010`). Não dá.

### Erro 2 (plan revisado, bit 3 = 0x08 no low nibble)
Bit 3 do low nibble está em uso pelos kinds:
- BASE_STRUCT = 0x08 (bit 3 set)
- BASE_FN     = 0x0A (bit 3 set)
- BASE_UNKNOWN = 0x0C (bit 3 set)

`TY_WORD | 0x08 = 0x0A` colide com BASE_FN. Não dá.

### Erro 3 (commit 1 da execução, relocar bit fields pra kind 0x7)
Kind 0x7 = `0111` tem bit 0 = 1. `is_float_type(ty) = ty & 1`
retorna 1 pra qualquer TY_BIT* relocado → todos viram "float" pro
compilador. test_bitfield falha em runtime.

---

## Verdade do encoding (auditada 2026-05-14)

`src/bpp_defs.bsm:184-244` é a fonte. Reproduzido literalmente:

```b++
BASE_WORD    = 0x02    // bit 0 = 0 (int)
BASE_FLOAT   = 0x03    // bit 0 = 1 (float)
BASE_PTR     = 0x04
BASE_ARR     = 0x06
BASE_STRUCT  = 0x08
BASE_FN      = 0x0A
BASE_UNKNOWN = 0x0C

is_float_type(ty) { return ty & 1; }           // KEY: bit 0
ty_base(ty)       { return ty & 0xF; }         // low nibble = kind
ty_slice(ty)      { return (ty / 16) & 0xF; }  // high nibble = width
slice_width(sl)   { ... 64/32/16/8/128 ... }   // base-AGNOSTIC
```

Tabela completa do low nibble:

| Kind | Status | Float-safe (bit 0=0)? |
|---|---|---|
| 0x00 | sentinel "no base" | ✓ even |
| 0x01 | livre, mas odd | ✗ colide is_float |
| 0x02 | BASE_WORD | ✓ |
| 0x03 | BASE_FLOAT | (intencional) |
| 0x04 | BASE_PTR | ✓ |
| 0x05 | livre, mas odd | ✗ |
| 0x06 | BASE_ARR | ✓ |
| 0x07 | livre, mas odd | ✗ ← **commit 1 quebrou aqui** |
| 0x08 | BASE_STRUCT | ✓ |
| 0x09 | livre, mas odd | ✗ |
| 0x0A | BASE_FN | ✓ |
| 0x0B | livre, mas odd | ✗ |
| 0x0C | BASE_UNKNOWN | ✓ |
| 0x0D | livre, mas odd | ✗ |
| **0x0E** | **LIVRE, even** | **✓ ← ÚNICO slot viável** |
| 0x0F | livre, mas odd | ✗ |

**Único kind par livre: `0x0E`.** Bit 0 = 0 (float-safe), kind nibble
livre (não conflita com nenhum BASE_*).

---

## Plano corrigido — 1 commit (não mais 3)

A versão original tinha 3 commits (relocate TY_WORD_BIT → relocate
TY_FLOAT_D → add unsigned). Os 2 primeiros eram cleanup do drift, mas
o agente anterior provou que mexer com a localização das famílias
existentes quebra `is_float_type` e cascateia. **Deixa as famílias
existentes em paz.** Só adiciona o novo kind no slot livre.

### Single commit: `feat: : u_word / u_half / u_quarter / u_byte annotations`

#### Mudanças em `src/bpp_defs.bsm`

**1. Adicionar BASE_UWORD ao bloco de constantes (linha ~191):**

```b++
BASE_FN = 0x0A;
// BASE_UWORD — unsigned int family. Kind 0x0E é o único kind par
// livre (bit 0 = 0 → is_float_type safe; bits 1-3 não colidem com
// nenhum BASE existente). Mesma posição do nibble que BASE_WORD;
// slice_width funciona inalterado porque depende só do slice nibble.
BASE_UWORD = 0x0E;
```

**2. Adicionar família TY_UWORD após TY_WORD_B (linha ~205):**

```b++
TY_WORD   = 0x02; TY_WORD_H  = 0x12; TY_WORD_Q  = 0x22; TY_WORD_B  = 0x32;
TY_UWORD  = 0x0E; TY_UWORD_H = 0x1E; TY_UWORD_Q = 0x2E; TY_UWORD_B = 0x3E;
```

**3. Adicionar helper `is_unsigned_type` após `is_float_type` (linha ~237):**

```b++
is_float_type(ty) { return ty & 1; }

// Return 1 if the type is any unsigned-int variant. Bit 0 = 0 so
// it never collides with the float flag; the kind nibble (0xE)
// uniquely identifies the unsigned family.
is_unsigned_type(ty) { if ((ty & 0xF) == BASE_UWORD) { return 1; } return 0; }

// Return 1 if the type is any int variant (signed OR unsigned).
// Used by sites that need "is this a non-float integer" rather than
// the stricter BASE_WORD-only check.
is_int_type(ty) {
    auto b;
    b = ty & 0xF;
    if (b == BASE_WORD) { return 1; }
    if (b == BASE_UWORD) { return 1; }
    return 0;
}
```

**4. Declarar `extrn` no topo de `bpp_defs.bsm` (linha ~75-80):**

```b++
extrn TY_WORD,  TY_WORD_H,  TY_WORD_Q,  TY_WORD_B;
extrn TY_UWORD, TY_UWORD_H, TY_UWORD_Q, TY_UWORD_B;
extrn BASE_UWORD;
extrn is_unsigned_type, is_int_type;
```

#### Mudanças em `src/bpp_parser.bsm`

**5. Parser sites (procura por `tl == 4` e `tl == 6` e `tl == 7`):**

Bloco já existe pros tamanhos de token. Add por tl:

```b++
// tl == 6 — junto com "double"
if (tl == 6 && buf_eq(ts, "u_word", 6)) { _prim_hint = TY_UWORD; consume(); return; }
if (tl == 6 && buf_eq(ts, "u_half", 6)) { _prim_hint = TY_UWORD_H; consume(); return; }
if (tl == 6 && buf_eq(ts, "u_byte", 6)) { _prim_hint = TY_UWORD_B; consume(); return; }

// tl == 9 — novo bloco se ainda não existe
if (tl == 9 && buf_eq(ts, "u_quarter", 9)) { _prim_hint = TY_UWORD_Q; consume(); return; }
```

Localização exata: depois do bloco "tl == 7 quarter" (~linha 1108).

#### Mudanças em `src/bpp_validate.bsm`

**6. Audit dos 2 sites que checam `BASE_WORD` literal:**

- **Linha 592** (`if (vb == BASE_WORD)`): W011 narrowing-precision diagnostic. Decisão:
  - Estender pra `if (vb == BASE_WORD || vb == BASE_UWORD)` para que W011 dispare também em unsigned narrow.
  - Razão: precision loss é igual signed/unsigned.

- **Linha 794** (`if (ty_base(hint_ty) == BASE_WORD)`): E261 float-lit → int-hint diagnostic. Decisão:
  - Estender pra `if (ty_base(hint_ty) == BASE_WORD || ty_base(hint_ty) == BASE_UWORD)`.
  - Razão: storing float literal numa variável unsigned é mesmo erro.

#### Mudanças em `src/bpp_codegen.bsm` — smart dispatch no T_BINOP

**7. No bloco T_BINOP do `cg_emit_node` (linha ~2370-2431):**

Insira ANTES do `if (c0 == '/' && op_l == 1) { call(p.emit_div, ...)`:

```b++
// Smart dispatch: unsigned semantics route to unsigned primitives.
// LHS type decides (B++ smart-dispatch convention — same as
// existing float-vs-int routing via use_flt above).
if (is_unsigned_type(lty)) {
    if (c0 == '/' && op_l == 1) { call(p.emit_udiv, left_reg_int); return 0; }
    if (c0 == '%' && op_l == 1) { call(p.emit_umod, left_reg_int); return 0; }
    if (c0 == '>' && c1 == '>') { call(p.emit_lshr, left_reg_int); return 0; }
}
```

Os outros operadores (+, -, *, &, |, ^, <<, ==, !=, <, <=, >, >=,
cmp_int) **continuam usando o caminho signed** — bit-level idênticos
em two's-complement (add/sub/mul/and/or/xor/shl não importa o sinal;
cmp signed vs unsigned é decisão separada que NÃO incluímos nessa
sidequest por enquanto — `<` em unsigned ainda usa signed cmp).

#### Mudanças em `src/bpp_codegen.bsm` — ChipPrimitives table

**8. Adicionar 3 slots ao ChipPrimitives struct (~linha 220):**

Localizar onde estão `emit_div`, `emit_mod`, `emit_shr` (Wave 5
arithmetic). Adicionar 3 vizinhos:

```b++
emit_udiv,              // ()  unsigned divide (UDIV / DIV)
emit_umod,              // ()  unsigned modulo
emit_lshr,              // ()  logical shift right (LSR / SHR)
```

#### Mudanças em `src/backend/chip/aarch64/a64_primitives.bsm`

**9. Adicionar 3 implementações dual-mode (binary + text) após `_a64_emit_shr`:**

```b++
void _a64_emit_udiv(left_reg) {
    if (a64_bin_mode) { enc_udiv(0, left_reg, 0); }
    else { out("  udiv x0, x"); a64_print_dec(left_reg); out(", x0"); putchar('\n'); }
}
void _a64_emit_umod(left_reg) {
    // umod = left - (left/right)*right. UDIV+MSUB pattern.
    if (a64_bin_mode) {
        enc_udiv(1, left_reg, 0);  // x1 = left/right
        enc_msub(0, 1, 0, left_reg); // x0 = left - x1*x0
    } else {
        out("  udiv x1, x"); a64_print_dec(left_reg); out(", x0"); putchar('\n');
        out("  msub x0, x1, x0, x"); a64_print_dec(left_reg); putchar('\n');
    }
}
void _a64_emit_lshr(left_reg) {
    if (a64_bin_mode) { enc_lsrv(0, left_reg, 0); }
    else { out("  lsr x0, x"); a64_print_dec(left_reg); out(", x0"); putchar('\n'); }
}
```

**Verificar antes:** `enc_udiv`, `enc_msub`, `enc_lsrv` já existem em
`src/backend/chip/aarch64/a64_enc.bsm` (linhas ~225-250). Se não
existirem, escrever (são opcodes próximos dos signed; ~10 LOC cada).

**10. Wire na ChipPrimitives instance (procurar `_a64_emit_div` na
struct e adicionar os 3 vizinhos):**

```b++
p.emit_udiv = fn_ptr(_a64_emit_udiv);
p.emit_umod = fn_ptr(_a64_emit_umod);
p.emit_lshr = fn_ptr(_a64_emit_lshr);
```

#### Mudanças em `src/backend/chip/x86_64/x64_primitives.bsm`

**11. Adicionar 3 implementações:**

```b++
void _x64_emit_udiv(left_reg) {
    // RAX = left (já está aí), zera RDX, DIV reg → quociente em RAX.
    x64_enc_xor_reg(2, 2);  // xor rdx, rdx
    x64_enc_div(1);          // div rcx (reg encoding 1)
}
void _x64_emit_umod(left_reg) {
    x64_enc_xor_reg(2, 2);
    x64_enc_div(1);
    x64_enc_mov_reg(0, 2);   // mov rax, rdx (resto em RAX)
}
void _x64_emit_lshr(left_reg) {
    x64_enc_shr_cl(0);       // já é o opcode unsigned shift right
}
```

**Verificar antes:** `x64_enc_div` (unsigned DIV opcode `F7 F1`) pode
não existir — `x64_enc_idiv` (signed) existe. Se faltar, ~3 LOC pra
adicionar em `x64_enc.bsm`. `x64_enc_xor_reg`, `x64_enc_shr_cl`,
`x64_enc_mov_reg` já existem confirmado.

**12. Wire na ChipPrimitives instance:**

```b++
p.emit_udiv = fn_ptr(_x64_emit_udiv);
p.emit_umod = fn_ptr(_x64_emit_umod);
p.emit_lshr = fn_ptr(_x64_emit_lshr);
```

#### Mudanças em `src/backend/c/c_emit.bsm` (se C emitter precisa parity)

**13. Auditar se C emitter usa primitive table ou re-implementa T_BINOP:**

```bash
grep -n "T_BINOP\|emit_div\|emit_mod\|emit_shr" /Users/Codes/b++/src/backend/c/c_emit.bsm
```

Se ele re-implementa (provável), adicionar:
- `u_word` mapeia pra `uint64_t`, etc
- `/` em unsigned vira `(uint64_t)x / (uint64_t)y` (cast no emit)
- Similar pra `%` e `>>`

Se C emitter atrasar — `// skip-c:` no smoke test, atualiza ticket
separado pra fechar parity depois.

---

## Smoke test

`tests/test_u_word_smoke.bpp`:

```b++
import "bpp_io.bsm";

main() {
    auto x: u_word, y: u_word, z: u_word, w: u_word;
    x = -1;             // 0xFFFFFFFFFFFFFFFF em u_word
    y = 4;
    z = x / y;          // unsigned: 0x3FFFFFFFFFFFFFFF (não -1 ou 0)
    w = x >> 2;         // unsigned: 0x3FFFFFFFFFFFFFFF (LSR, não ASR)
    print_int(z); print_ln();
    print_int(w); print_ln();
    // signed comparison:
    auto sx: word;
    sx = -1;
    print_int(sx / 4);  // -1 (signed: rounds toward 0)
    print_ln();
    print_int(sx >> 2); // -1 (ASR: bit 63 fills)
    print_ln();
    return 0;
}
```

Output esperado:
```
4611686018427387903
4611686018427387903
-1
-1
```

Confirma:
- u_word `/` 4 = `0x3FFF...FFFF` (UDIV, não SDIV)
- u_word `>>` 2 = `0x3FFF...FFFF` (LSR, não ASR)
- word `/` 4 = -1 (SDIV preservado)
- word `>>` 2 = -1 (ASR preservado)

---

## Verification protocol

```bash
# 1. Build
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2) | head  # SHOULD be empty (byte-stable)

# 2. Suite native
cp /tmp/bpp_gen1 /Users/Codes/b++/bpp
bash tests/run_all.sh
# Expected: 167 passed, 0 failed, 12 skipped (mesmo baseline pre-sidequest)

# 3. Suite C
bash tests/run_all_c.sh
# Expected: same baseline OR skip-c smoke test for new feature

# 4. Smoke
./bpp tests/test_u_word_smoke.bpp -o /tmp/u_word_smoke
/tmp/u_word_smoke
# Match the 4 expected lines above
```

---

## Roteiro de commit

UM commit:
```
feat: : u_word / u_half / u_quarter / u_byte annotations

Reserve kind 0x0E (the only free even kind, float-safe via bit 0=0)
for the unsigned int family. Smart-dispatch T_BINOP routes /, %, >>
through new emit_udiv / emit_umod / emit_lshr primitives when LHS
is unsigned. Other operators (+, -, *, &, |, ^, <<, cmp) keep
signed primitives — bit-identical in two's-complement.

Annotations:
  : u_word     → TY_UWORD   = 0x0E  (u64)
  : u_half     → TY_UWORD_H = 0x1E  (u32)
  : u_quarter  → TY_UWORD_Q = 0x2E  (u16)
  : u_byte     → TY_UWORD_B = 0x3E  (u8)

Helpers: is_unsigned_type, is_int_type (signed OR unsigned).

Pre-existing BASE_WORD-specific sites in validate.bsm (W011 narrow,
E261 float-lit→int-hint) extended to cover unsigned identically.

Smoke: tests/test_u_word_smoke.bpp exercises u_word vs word for
divide and shift-right, confirming bit-level semantics.
```

---

## Stop conditions (se algo der errado)

1. **Bootstrap não fecha byte-stable** → reverter, investigar qual
   site novo introduziu não-determinismo. Provavelmente é o C emitter
   se foi tocado.

2. **Suite regride** → identificar test específico. Se for
   bitfield-relacionado, double-check que TY_WORD_BIT family
   permanece intocada (slice nibble 4-10, base WORD = 0x2). Se for
   float-relacionado, double-check `is_float_type` ainda retorna 0
   pra TY_UWORD* (bit 0 = 0).

3. **`enc_udiv`/`enc_lsrv`/`enc_msub` não existem em a64_enc.bsm**
   → escrever. ARM64 opcodes:
   - UDIV: `9AC0_0800` base, registers em bits 0-4 (Rd), 5-9 (Rn), 16-20 (Rm)
   - LSRV: `9AC0_2400` base
   - MSUB: `9B00_8000` base
   Mesma família dos signed encoders já presentes.

4. **`x64_enc_div` não existe** → escrever. x64 opcode:
   - DIV reg: `REX.W + F7 /6` (ModR/M byte `0xF0 | reg`)
   - 3 LOC similares aos outros enc_* helpers.

5. **C emitter quebra** → adicionar `// skip-c:` no smoke test, abrir
   ticket separado `docs/sidequest_unsigned_c_parity.md`. Não bloqueia
   o commit principal.

---

## O que NÃO está nesse sidequest (escopo explícito)

- Comparação unsigned (`<` em u_word usar CMP+B.LO em vez de B.LT) —
  deferred. Hoje funciona o suficiente para todos os killer use cases
  (`/`, `%`, `>>` são onde aparece o bug com negativos).
- Implicit signed↔unsigned conversion warnings — deferred.
- Relocação de TY_FLOAT_D ou TY_WORD_BIT (do plan original 3-commit)
  — DROPPED. Deixa as famílias existentes onde estão. Esse retry foca
  apenas em adicionar o novo kind, sem mexer no que funciona.
- W032 diagnostic (signed division by power-of-2 literal sugerindo
  unsigned) — engatilhado, abre como sidequest separado depois do
  unsigned shipped.
- Tonify Rule 32 — vira documento DEPOIS do feature shipado, não no
  mesmo commit. Doc-as-you-ship.

---

## Resumo do que mudou desse plano vs o original

| Aspecto | Plan original | Plan retry |
|---|---|---|
| Slot escolhido | Bit 6 = 0x40 (slice nibble conflict) → depois bit 3 = 0x08 (kind nibble conflict) | **Kind 0x0E** (único par livre) |
| Número de commits | 3 (relocate WORD_BIT + relocate FLOAT_D + add unsigned) | 1 (só add unsigned) |
| Mexe em famílias existentes? | Sim (TY_WORD_BIT, TY_FLOAT_D) | Não — deixa intocadas |
| Risk de quebra de `is_float_type` | Alto (Commit 1 do agente anterior quebrou) | Zero — 0x0E tem bit 0 = 0 |
| Validate.bsm sites a tocar | ~5 (relocações + checks) | 2 (W011 + E261 estendem signed→unsigned-também) |

Encoding consolidation cleanup (relocate TY_WORD_BIT, TY_FLOAT_D) é
**arc separado** se algum dia justificar. Hoje não é gating do
unsigned annotation.
