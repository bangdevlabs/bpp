# Wave 21 Plan — Emit-Node Lift (endgame do 3.4)

**Status:** proposta. A ser executada DEPOIS do Wave 20 (emit_stmt orquestração).

**Autor:** Claude Opus (sessão 2026-04-22, modo consultor).

**Relação com plano 3.4 original:** O `legacy_docs/phase_3_4_chip_primitives.md`
(2026-04-20) reservou **Waves 1-12** pra `emit_node`. Surpresa feliz: **as Waves
1-10 já foram executadas** (primitivas emit_mov_zero / emit_mov_imm /
emit_load_str_addr / emit_load_float_const / emit_add-sub-mul-div / emit_cmp / etc.
estão todas wired, e T_LIT/T_VAR/T_UNARY/T_CALL/T_MEMLD/T_ADDR já dispatchadas
via spine helpers). **Wave 21 fecha os ~10% restantes.**

---

## Estado real do emit_node (não o que eu achei antes)

Ao inspecionar `a64_emit_node` e `x64_emit_node`, descobri que o tree walker
**já está majoritariamente lifted**. Abaixo o status real por caso:

| Caso       | Status                           | O que sobra chip-local        |
|------------|----------------------------------|-------------------------------|
| `nd == 0`  | ✓ `call(p.emit_mov_zero)`        | Nada                          |
| `T_LIT`    | ✓ `cg_emit_lit(n.a)` (spine)     | Nada                          |
| `T_VAR`    | ✓ `cg_emit_var(n.a)` (spine)     | Stack struct addr-of inline   |
| `T_TERNARY`| ✓ primitivas de control flow     | Recursão em `a64_emit_node`   |
| `T_BINOP`  | ✓ arithmetic/comparison via primitivas | **Left-save + int↔float convs inline** |
| `T_UNARY`  | ✓ `cg_emit_unary(n.a, uty)`      | Recursão em `a64_emit_node`   |
| `T_CALL`   | ✓ `cg_emit_call(n)` (Wave 18)    | Recursão interna              |
| `T_MEMLD`  | ✓ `call(p.emit_memld_load, fh)`  | Recursão em `a64_emit_node`   |
| `T_ADDR`   | ✓ `call(p.emit_addr_local/global)` | Nada                        |

**Resumo:** o tree walker é **~90% spine**. O que sobra:
1. Outer dispatcher shell duplicado (`static a64_emit_node(nd) { if (t == X) ... }`)
2. Inline `enc_scvtf` / `enc_fcvtzs` / `x64_enc_cvtsi2sd` / `x64_enc_cvttsd2si`
   em T_BINOP (conversões int↔float no left-save dance)
3. Inline `enc_add_imm(0, 29, off)` / `x64_enc_lea(0, 5, off)` em T_VAR stack
   struct addr-of
4. Inline `a64_gp_alloc()` / `enc_mov_reg` no freelist path de T_BINOP (a64 only)
5. Recursão em `a64_emit_node` / `x64_emit_node` — uma vez que `cg_emit_node`
   existir no spine, vira `call(p.emit_node, nd)` (primitiva JÁ existe —
   `p.emit_node` foi wired no Wave 18)

---

## O que Wave 21 faz

**Objetivo:** migrar o dispatcher outer de emit_node pro spine e cleanar os
últimos vestígios de inline asm.

**Não-objetivo:** tocar em primitivas já lifted. Waves 1-10 funcionam. Não
mexe.

### Primitivas novas do Wave 21 (3 novas)

```
struct ChipPrimitives {
    ...
    // ── Wave 21: emit_node residuals ──

    // Stage left operand in a "save slot" for later use in T_BINOP.
    // Returns an opaque handle that emit_binop_retrieve_left consumes.
    // On a64 the save is either a freelist reg (returning its index)
    // or a stack push (returning -1). On x64 always stack push
    // (returns -1). The spine calls this BEFORE evaluating the right
    // operand.
    //
    // Parameters:
    //   lty        — left type (0 int, 1 float)
    //   right_has_call — 1 if the right-side expression contains a
    //                    function call (tells a64 to skip the
    //                    freelist because caller-saved regs clobber)
    emit_binop_save_left,   // (lty, right_has_call) → save_handle

    // After right operand is in acc/xmm0, retrieve the left operand
    // into scratch-int (x1/rcx) or scratch-float (d1/xmm1) per
    // use_flt. Handles the int↔float conversion + promotion dance.
    //
    // Parameters:
    //   save_handle — from emit_binop_save_left
    //   lty         — left type (0 int, 1 float)
    //   rty         — right type (0 int, 1 float)
    //   use_flt     — 1 if the BINOP uses float path
    emit_binop_retrieve_left, // (save_handle, lty, rty, use_flt)

    // Address-of a stack-allocated struct variable (`struct T foo;`
    // where foo takes multiple frame slots). Emits the `add rd, fp,
    // #off` form specific to the chip. T_VAR's edge case.
    //
    // Already has a sibling primitive `emit_addr_local` but that's
    // for scalar locals; stack structs need the same form but the
    // chip file currently inlines it. Separating is defensive —
    // future refactors may add struct-specific decoration.
    emit_addr_stack_struct,   // (offset)
}
```

Nota: `p.emit_node` **já existe** (wired no Wave 18 scaffold). Não é primitiva
nova — é o que faz a recursão continuar no chip até o outer dispatcher lift
estar pronto no Batch 4.

---

## Spine implementation

```
// bpp_codegen.bsm

cg_emit_node(nd) {
    auto p: ChipPrimitives, n: Node, t;
    auto op_s, op_l, c0, c1, vi, lty, rty, use_flt, is_io, uty, ety, off;

    if (nd == 0) {
        p = cg_prim;
        call(p.emit_mov_zero);
        return 0;
    }

    n = nd; t = n.ntype;
    p = cg_prim;

    if (t == T_LIT) { return cg_emit_lit(n.a); }

    if (t == T_VAR) {
        vi = cg_var_idx(n.a);
        if (vi >= 0 && arr_get(cg_var_stack, vi)) {
            call(p.emit_addr_stack_struct, arr_get(cg_var_off, vi));
            return 0;
        }
        return cg_emit_var(n.a);
    }

    if (t == T_TERNARY) { return cg_emit_ternary(n); }
    if (t == T_BINOP)   { return cg_emit_binop(n); }
    if (t == T_UNARY)   {
        uty = call(p.emit_node, n.b);
        return cg_emit_unary(n.a, uty);
    }
    if (t == T_CALL)    { return cg_emit_call(n); }
    if (t == T_MEMLD)   {
        ety = call(p.emit_node, n.a);
        if (ety == 1) { call(p.emit_demote_flt_to_int); }
        return call(p.emit_memld_load, n.b);
    }
    if (t == T_ADDR)    { return cg_emit_addr(n); }

    // Unknown node type — shouldn't happen if parser is clean.
    return 0;
}

static cg_emit_ternary(n: Node) {
    auto p: ChipPrimitives, lbl_else, lbl_done, ety, ety_then;
    p = cg_prim;
    ety = call(p.emit_node, n.a);
    if (ety == 1) { call(p.emit_fcond_to_int_truth); }
    lbl_else = call(p.emit_new_label);
    lbl_done = call(p.emit_new_label);
    call(p.emit_branch_if_zero, lbl_else);
    ety_then = call(p.emit_node, n.b);
    call(p.emit_jump, lbl_done);
    call(p.emit_label, lbl_else);
    call(p.emit_node, n.c);
    call(p.emit_label, lbl_done);
    return ety_then;
}

static cg_emit_binop(n: Node) {
    auto p: ChipPrimitives;
    auto op_s, op_l, c0, c1, is_io, lty, rty, use_flt;
    auto save_handle;
    p = cg_prim;

    op_s = unpack_s(n.a); op_l = unpack_l(n.a);
    c0 = peek(cg_sbuf + op_s);
    c1 = 0; if (op_l > 1) { c1 = peek(cg_sbuf + op_s + 1); }

    is_io = 0;
    if (c0 == '%' || c0 == '^') { is_io = 1; }
    if ((c0 == '&' || c0 == '|') && op_l == 1) { is_io = 1; }
    if ((c0 == '<' && c1 == '<') || (c0 == '>' && c1 == '>')) { is_io = 1; }

    // Evaluate left, save for retrieval.
    lty = call(p.emit_node, n.b);
    save_handle = call(p.emit_binop_save_left, lty, cg_has_call(n.c));

    // Evaluate right into acc/xmm0.
    rty = call(p.emit_node, n.c);

    use_flt = is_io == 0 && (lty == 1 || rty == 1) ? 1 : 0;

    // Retrieve left (with int↔float conversion if needed).
    call(p.emit_binop_retrieve_left, save_handle, lty, rty, use_flt);

    // Dispatch to the primitive — left_reg=1 is the canonical choice
    // (a64 freelist case resolves to acc's scratch, x64 always uses
    // rcx). cg_emit_binop_op dispatches by operator+type.
    return cg_emit_binop_op(c0, c1, op_l, use_flt);
}

static cg_emit_binop_op(c0, c1, op_l, use_flt) {
    auto p: ChipPrimitives;
    p = cg_prim;

    if (use_flt) {
        if (c0 == '+' && op_l == 1) { call(p.emit_fadd); return 1; }
        if (c0 == '-' && op_l == 1) { call(p.emit_fsub); return 1; }
        if (c0 == '*' && op_l == 1) { call(p.emit_fmul); return 1; }
        if (c0 == '/' && op_l == 1) { call(p.emit_fdiv); return 1; }
        if (c0 == '=' && c1 == '=') { call(p.emit_cmp_flt, CG_CC_EQ); return 0; }
        if (c0 == '!' && c1 == '=') { call(p.emit_cmp_flt, CG_CC_NE); return 0; }
        if (c0 == '<') {
            if (c1 == '=') { call(p.emit_cmp_flt, CG_CC_LE); return 0; }
            if (op_l == 1) { call(p.emit_cmp_flt, CG_CC_LT); return 0; }
        }
        if (c0 == '>') {
            if (c1 == '=') { call(p.emit_cmp_flt, CG_CC_GE); return 0; }
            if (op_l == 1) { call(p.emit_cmp_flt, CG_CC_GT); return 0; }
        }
        return 1;
    }
    // Integer path — left_reg hint is 1 (x64 ignores; a64 uses 1
    // after retrieve-left lands the value in x1 in fallback paths).
    if (c0 == '+' && op_l == 1) { call(p.emit_add, 1); return 0; }
    if (c0 == '-' && op_l == 1) { call(p.emit_sub, 1); return 0; }
    if (c0 == '*' && op_l == 1) { call(p.emit_mul, 1); return 0; }
    if (c0 == '/' && op_l == 1) { call(p.emit_div, 1); return 0; }
    if (c0 == '%' && op_l == 1) { call(p.emit_mod, 1); return 0; }
    if (c0 == '&' && op_l == 1) { call(p.emit_and, 1); return 0; }
    if (c0 == '|' && op_l == 1) { call(p.emit_or,  1); return 0; }
    if (c0 == '^' && op_l == 1) { call(p.emit_xor, 1); return 0; }
    if (c0 == '>' && c1 == '>') { call(p.emit_shr, 1); return 0; }
    if (c0 == '<' && c1 == '<') { call(p.emit_shl, 1); return 0; }
    if (c0 == '=' && c1 == '=') { call(p.emit_cmp_int, CG_CC_EQ, 1); return 0; }
    if (c0 == '!' && c1 == '=') { call(p.emit_cmp_int, CG_CC_NE, 1); return 0; }
    if (c0 == '<') {
        if (c1 == '=') { call(p.emit_cmp_int, CG_CC_LE, 1); return 0; }
        if (op_l == 1) { call(p.emit_cmp_int, CG_CC_LT, 1); return 0; }
    }
    if (c0 == '>') {
        if (c1 == '=') { call(p.emit_cmp_int, CG_CC_GE, 1); return 0; }
        if (op_l == 1) { call(p.emit_cmp_int, CG_CC_GT, 1); return 0; }
    }
    return 0;
}

static cg_emit_addr(n: Node) {
    auto p: ChipPrimitives, child: Node, vi;
    p = cg_prim;
    child = n.a;
    if (child != 0 && child.ntype == T_VAR) {
        vi = cg_var_idx(child.a);
        if (vi >= 0) {
            call(p.emit_addr_local, arr_get(cg_var_off, vi));
            return 0;
        }
        call(p.emit_addr_global, child.a);
        return 0;
    }
    return 0;
}
```

---

## Primitivas chip-side

### `_a64_emit_binop_save_left(lty, right_has_call)`

```
_a64_emit_binop_save_left(lty, right_has_call) {
    auto alloc_reg;
    if (lty == 1) {
        a64_emit_fpush();
        return 0 - 1;  // float path: no freelist, handle = -1
    }
    if (right_has_call) {
        // Caller-saved clobber risk. Use stack.
        a64_emit_push();
        return 0 - 1;
    }
    alloc_reg = a64_gp_alloc();
    if (alloc_reg >= 0) {
        if (a64_bin_mode) { enc_mov_reg(alloc_reg, 0); }
        else { out("  mov x"); a64_print_dec(alloc_reg); out(", x0"); putchar('\n'); }
        return alloc_reg;
    }
    // Freelist exhausted — fall back to stack.
    a64_emit_push();
    return 0 - 1;
}
```

### `_a64_emit_binop_retrieve_left(save_handle, lty, rty, use_flt)`

```
_a64_emit_binop_retrieve_left(save_handle, lty, rty, use_flt) {
    if (use_flt) {
        // Right may need int→float conversion.
        if (rty == 0) {
            if (a64_bin_mode) { enc_scvtf(0, 0); }
            else { out("  scvtf d0, x0"); putchar('\n'); }
        }
        if (lty == 1) {
            a64_emit_fpop(1);
        } else if (save_handle >= 0) {
            // Left was in freelist reg — convert to float.
            if (a64_bin_mode) { enc_scvtf(1, save_handle); }
            else { out("  scvtf d1, x"); a64_print_dec(save_handle); putchar('\n'); }
            a64_gp_free(save_handle);
        } else {
            a64_emit_pop(1);
            if (a64_bin_mode) { enc_scvtf(1, 1); }
            else { out("  scvtf d1, x1"); putchar('\n'); }
        }
        return;
    }
    // Integer path.
    if (rty == 1) {
        if (a64_bin_mode) { enc_fcvtzs(0, 0); }
        else { out("  fcvtzs x0, d0"); putchar('\n'); }
    }
    if (lty == 1) {
        a64_emit_fpop(0);
        if (a64_bin_mode) { enc_fcvtzs(1, 0); }
        else { out("  fcvtzs x1, d0"); putchar('\n'); }
    } else if (save_handle >= 0) {
        // Keep left in its freelist reg — return it. Spine dispatch
        // uses left_reg=1 as convention; if save_handle != 1,
        // a mov x1, x{save_handle} is emitted to match convention.
        if (save_handle != 1) {
            if (a64_bin_mode) { enc_mov_reg(1, save_handle); }
            else { out("  mov x1, x"); a64_print_dec(save_handle); putchar('\n'); }
        }
        a64_gp_free(save_handle);
    } else {
        a64_emit_pop(1);
    }
}
```

### `_a64_emit_addr_stack_struct(offset)`

```
_a64_emit_addr_stack_struct(offset) {
    if (a64_bin_mode) { enc_add_imm(0, 29, offset); }
    else { out("  add x0, x29, #"); a64_print_dec(offset); putchar('\n'); }
}
```

### x64 counterparts

Muito mais simples (x64 não tem freelist):

```
_x64_emit_binop_save_left(lty, right_has_call) {
    if (lty == 1) { x64_emit_fpush(); }
    else { x64_emit_push(); }
    return 0 - 1;   // always -1 on x64
}

_x64_emit_binop_retrieve_left(save_handle, lty, rty, use_flt) {
    if (use_flt) {
        if (rty == 0) { x64_enc_cvtsi2sd(0, 0); }
        if (lty == 1) { x64_emit_fpop(1); }
        else {
            x64_emit_pop(0);
            x64_enc_cvtsi2sd(1, 0);
        }
        return;
    }
    if (rty == 1) { x64_enc_cvttsd2si(0, 0); }
    if (lty == 1) {
        x64_enc_mov_reg(1, 0);
        x64_emit_fpop(0);
        x64_enc_cvttsd2si(0, 0);
    } else {
        x64_enc_mov_reg(1, 0);
        x64_emit_pop(0);
    }
}

_x64_emit_addr_stack_struct(offset) {
    x64_enc_lea(0, 5, offset);
}
```

---

## Helper portável no spine

```
// cg_has_call(nd) — walks a subtree looking for T_CALL. Cached
// per call because the walk is O(n). Used by T_BINOP to decide
// whether the left operand can live in a freelist reg or needs
// to hit the stack (caller-saved clobber).
//
// Replaces chip's _a64_has_call (x64 doesn't have the freelist
// distinction so it had no such helper — but having one on both
// chips via the spine keeps the contract uniform).
cg_has_call(nd) {
    auto n: Node, t;
    if (nd == 0) { return 0; }
    n = nd; t = n.ntype;
    if (t == T_CALL) { return 1; }
    // Recursively check children. Node structure: a/b/c/d are
    // Node pointers; e is an array length scalar.
    if (cg_has_call(n.a)) { return 1; }
    if (cg_has_call(n.b)) { return 1; }
    if (cg_has_call(n.c)) { return 1; }
    if (cg_has_call(n.d)) { return 1; }
    return 0;
}
```

---

## Migration strategy (4 batches)

Mesmo padrão: cada batch é bootstrap + gen1==gen2 + `sh tests/run_all.sh`.

### Batch 1 — Add primitives (zero activation)

Adiciona slots na `ChipPrimitives`:
- `emit_binop_save_left`
- `emit_binop_retrieve_left`
- `emit_addr_stack_struct`

Implementa nos dois chips, wire nos init. Adiciona `cg_has_call` no spine.
Nenhum callsite mudou. Bin maior (código morto) mas byte-identity preservada.

**Gen1==gen2 + tests.**

### Batch 2 — Lift T_VAR stack struct addr-of

Pequena mudança cirúrgica: no chip's emit_node dentro do T_VAR, substituir
`enc_add_imm(0, 29, off)` / `x64_enc_lea(0, 5, off)` por
`call(p.emit_addr_stack_struct, off)`.

**Gen1==gen2 + tests.**

### Batch 3 — Lift T_BINOP left-save dance

Dentro de a64_emit_node e x64_emit_node, o T_BINOP inteiro troca:
- Antes: `lty = emit_node(n.b); if/else push/alloc/...` + int↔float conv inline
- Depois: `lty = emit_node(n.b); save_handle = call(p.emit_binop_save_left, lty, cg_has_call(n.c)); rty = emit_node(n.c); call(p.emit_binop_retrieve_left, save_handle, lty, rty, use_flt);`

**Gen1==gen2 + tests.** Esse é o batch onde mais código é removido dos chips.

### Batch 4 — Spine takes over emit_node orchestration

Define `cg_emit_node(nd)` no spine com toda a lógica de dispatch acima. Extrai
`cg_emit_ternary`, `cg_emit_binop`, `cg_emit_binop_op`, `cg_emit_addr` como
helpers internos.

`a64_emit_node(nd)` / `x64_emit_node(nd)` viram wrappers de uma linha:
```
static a64_emit_node(nd) { return cg_emit_node(nd); }
```

Callers externos (inline em emit_stmt, etc) continuam chamando
`a64_emit_node` e o redirect leva pro spine.

**Gen1==gen2 + tests — batch de maior risco. Todo o tree walker muda de dono.**

### Batch 5 — Delete chip wrappers

Todos os call sites de `a64_emit_node` / `x64_emit_node` (inside chip files —
`emit_stmt`, `emit_body`, switch dispatcher, etc) passam a chamar `cg_emit_node`
direto ou `call(p.emit_node, nd)`. Delete os wrappers.

**Gen1==gen2 + tests finais. FIM da faxina do 3.4.**

---

## Riscos e mitigações

### Risco 1: `cg_has_call` walker

Antes só a64 precisava (por causa da freelist). Agora ambos chips chamam via
spine. Overhead mínimo porque a64 já fazia o walk — x64 chama também mas o
resultado é sempre ignorado pelo x64 primitive (save_handle sempre -1). No
futuro x64 poderia adotar uma freelist-like e usar a info, então não custa
nada ter pronto.

**Mitigação:** nenhuma necessária — helper é puro-função de Node tree.

### Risco 2: save_handle semântica

a64: pode ser -1 (stack) ou 0..N (freelist reg).
x64: sempre -1.

Spine passa opaque pro retrieve. Se algum chip futuro introduzir novos valores
(e.g. 2 = stashed in hidden slot), o contract suporta — spine não inspeciona
o handle, só encaminha.

**Mitigação:** documentar que save_handle é opaque. Adicionar assert se algum
chip retornar um handle que o retrieve não reconhece.

### Risco 3: left_reg convention (a64)

Antes do lift, a64 passava `left_reg_int` (o reg onde left caiu) como
parâmetro pras primitivas de arithmetic/comparison. Isso porque a64 tem 3-reg
ISA e o primitiva pode usar x{left_reg} direto como source.

Após retrieve-left, o left sempre cai em `x1` (por convenção do retrieve-left
nos casos fallback). Spine passa `left_reg=1` unconditionally. A primitiva a64
emite `add x0, x1, x0` — idêntico ao que gerava antes na maior parte dos
casos.

**Risco específico:** quando o freelist alocava um reg != 1 (tipo x9 ou x10),
antes a primitiva gerava `add x0, x9, x0`. Depois do lift, retrieve-left
emite um `mov x1, x9` antes do operator, e o operator gera `add x0, x1, x0`.
**Mesma semântica, bytes diferentes.**

**Mitigação:** isso **quebra byte-identity**. Precisamos dos freelist hits
continuarem emitindo o mesmo código. Duas opções:

**Opção A — manter convention flexível:** `retrieve-left` retorna o reg onde
left ficou; spine passa esse valor em `left_reg`. Preserva a forma original.
A primitiva signature `emit_add(left_reg)` já aceita esse parâmetro.

**Opção B — aceitar mudança:** o mov x1, x9 adiciona 4 bytes por binop no
freelist path; é correto mas não byte-identical. Desde que `gen2 == gen3`
(self-host estável) e tests passem, aceitamos.

**Recomendação:** Opção A. `emit_binop_retrieve_left` retorna o left_reg
index (int). Spine passa pro operator. Preserva byte-identity nos freelist
hits. Assinatura final:

```
emit_binop_retrieve_left(save_handle, lty, rty, use_flt) → left_reg_int
```

Para use_flt path: sempre retorna 1 (d1). Para int path: retorna o reg
onde left ficou (freelist ou 1).

### Risco 4: T_UNARY / T_MEMLD recursão

Antes: `a64_emit_node(n.b)` — chip dispatcher loop.
Depois (Batch 4): `call(p.emit_node, nd)` — primitiva que delega de volta
pro chip's tree walker até ser redirecionado pro spine.

Se `p.emit_node` aponta pra `a64_emit_node` que agora é wrapper pra
`cg_emit_node`, a recursão flui correta. Zero mudança na semântica.

**Mitigação:** Batch 4 ordem importa — `cg_emit_node` precisa estar definido
ANTES de redirecionar `a64_emit_node` pra chamar ele (o que acontece na
mesma edit). Compilar + rodar testes.

### Risco 5: static functions no spine

Helpers `cg_emit_binop_op`, `cg_emit_ternary`, etc. são static-module.
Callers só dentro de bpp_codegen.bsm. Isso evita collision com nomes de
outros módulos.

**Mitigação:** usar `static` keyword nas definições.

---

## Sucesso — como medir

Ao fim do Wave 21:

1. `src/backend/chip/aarch64/a64_codegen.bsm`: ~300 linhas (era ~700 pós-W20)
2. `src/backend/chip/x86_64/x64_codegen.bsm`: ~250 linhas (era ~550 pós-W20)
3. Nova função no spine: `cg_emit_node` (~100 linhas) + helpers (~150 linhas)
4. Byte-identity: `gen1 == gen2` consistente
5. Regression gate: **112 passed / 0 failed** (não-GPU)
6. `a64_emit_node` / `x64_emit_node` não existem mais como implementações —
   são ou wrappers de 1 linha ou deletados.

---

## Pós-Wave 21 — a faxina ESTÁ feita

Depois do Wave 21, o endgame arquitetural descrito no 3.4 original é alcançado:

```
src/backend/chip/<arch>/
├── <arch>_enc.bsm         ~900 linhas — encoder cru, intocável
├── <arch>_primitives.bsm  ~1000-1200 linhas — o "verbo físico"
└── <arch>_codegen.bsm     ~250-300 linhas — init + register allocator +
                                              module-level (mod_init /
                                              emit_module / emit_all) +
                                              SIMD vec primitivas
```

**O que sobra no codegen não é refatorável:**
- `init_codegen_<chip>` + `cg_install_<chip>_primitives` — instala fn_ptrs
- Register allocator state (gp/simd freelists, promoted regs) — chip-específico
- `mod_init`, `emit_module`, `emit_all` wrappers — parcialmente portáveis
- `_vec_binop` — NEON vs SSE, não portabiliza sem perder performance
- Helpers ABI-específicos irredutíveis

Total line reduction hoje → pós-W21: **~2000 linhas de código duplicado
deletadas** (todas viveram no spine desde o início mas eram copiadas entre
chips).

Adicionar RISC-V como terceiro chip após Wave 21:
- `riscv64_enc.bsm` (~500 linhas) — raw encoder
- `riscv64_primitives.bsm` (~900 linhas) — implementa o contract
- `riscv64_codegen.bsm` (~200 linhas) — init + register allocator

Total: ~1600 linhas pra um chip novo inteiro. **Zero duplicação de tree
walker**, zero duplicação de dispatch, zero re-implementação de controle de
fluxo. O sonho do "Forth-portable".

---

## Commands úteis

```bash
# Medida do estado (pós-Wave 20 vs pós-Wave 21):
wc -l src/backend/chip/aarch64/a64_codegen.bsm src/backend/chip/x86_64/x64_codegen.bsm

# Contar enc_* inline remanescente no tree walker:
awk '/^static a64_emit_node/,/^}/ {print}' src/backend/chip/aarch64/a64_codegen.bsm | grep -cE "enc_"

# Self-host + regression após cada batch:
./bpp src/bpp.bpp -o /tmp/bpp_gen1
cp /tmp/bpp_gen1 bpp
./bpp src/bpp.bpp -o /tmp/bpp_gen2
shasum bpp /tmp/bpp_gen2
sh tests/run_all.sh

# Teste específico do freelist path (a64):
echo 'main() { auto a, b, c; a = 1; b = 2; c = a + b * 3; return c; }' > /tmp/freelist.bpp
./bpp /tmp/freelist.bpp -o /tmp/freelist_old
cp /tmp/bpp_gen1 bpp
./bpp /tmp/freelist.bpp -o /tmp/freelist_new
shasum /tmp/freelist_{old,new}   # must match se Opção A (flexible convention)
```

---

## Referência cruzada

- **Plano original:** `_legacy_bootstrap/legacy_docs/phase_3_4_chip_primitives.md`
  — "Waves 1-12: emit_node".
- **Wave 18 handoff:** `docs/wave18_handoff.md`.
- **Wave 19 plan:** `docs/wave19_plan.md` (emit_func orchestration).
- **Wave 20 plan:** `docs/wave20_plan.md` (emit_stmt orchestration).
- **Contract final:** ChipPrimitives com ~52 slots (45 originais + 4 de
  Wave 18 + 4 de Wave 20 + 3 de Wave 21).

---

## Postscript — o que vem DEPOIS do endgame?

Nada que seja "faxina". O chip layer está consolidado. A partir daqui:

1. **Adicionar ISAs novas** (RISC-V é o candidato óbvio — design já previu)
2. **Tunar primitivas** pra performance (peephole, combined ops)
3. **Audit do encoder** — linhas de `<chip>_enc.bsm` que podem ser mais curtas
   com encoding tricks modernos (BFC em vez de AND+MOV em a64, etc.)
4. **Documentar o contract** como API estável (v1 → v1.0 frozen, v2 pra
   extensões)

Mas isso é evolução, não faxina. A casa tá limpa.

Se tiver energia pós-Wave 21 nesse dia, Bang 9 polish (inspector panel D2.3)
é a próxima prioridade da lista antiga.
