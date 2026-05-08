# Wave 20 Plan — Emit-Stmt Lift (statement dispatcher + T_SWITCH + T_ASSIGN struct copy)

**Status:** proposta. A ser executada DEPOIS do Wave 19 (emit_func orchestration).

**Autor:** Claude Opus (sessão 2026-04-22, modo consultor).

**Relação com plano 3.4 original:** O `legacy_docs/phase_3_4_chip_primitives.md`
(2026-04-20) mencionou **Waves 13-16** como "emit_stmt" — **esse plano é a
continuação explícita disso**. A numeração real divergiu porque Waves 13-15
foram usadas para primitivas de `emit_func` (prologue/arg_copy/teardown). Wave 7
já lifted a maior parte dos primitivas de control flow (emit_new_label,
emit_branch_if_zero, emit_jump, emit_label, emit_fcond_to_int_truth,
emit_jump_to_epilogue) — o que falta é **ORQUESTRAÇÃO** + 3 casos residuais
ainda com `enc_*` inline.

---

## Premissa

Após Wave 19, `a64_emit_stmt` e `x64_emit_stmt` ficam com ~450 / ~275 linhas
dispatchando 11 tipos de AST:

```
T_DECL      — no-op (pre-reg)
T_ASSIGN    — variable/global assign + STACK STRUCT COPY (inline!)
T_MEMST     — pointer deref store + sub-word dispatch (inline!)
T_IF        — ✓ Wave 7 (usa primitivas)
T_BREAK     — ✓ Wave 7
T_CONTINUE  — ✓ Wave 7
T_WHILE     — ✓ Wave 7
T_RET       — ✓ Wave 7
T_SWITCH    — parcialmente Wave 11 (dense jtbl), mas compare-chain INLINE
T_NOP       — no-op
T_BLOCK     — delega pra emit_body
<default>   — expression statement, delega pra emit_node
```

**Ganho potencial:** ~300 linhas duplicadas caem. Spine fica com UM emit_stmt.

---

## O que ainda tem `enc_*` inline em emit_stmt

### T_ASSIGN stack struct copy (a64: ~12 linhas, x64: ~10 linhas)

```
// a64 (inline em T_ASSIGN):
fc = (get_struct_size(sidx) + 7) / 8;
j = 0;
while (j < fc) {
    if (a64_bin_mode) {
        enc_ldr_uoff(8, 0, j * 8);
        enc_str_uoff(8, 29, off + j * 8);
    } else {
        out("  ldr x8, [x0, #"); ...
        out("  str x8, [x29, #"); ...
    }
    j = j + 1;
}
```

Portável: loop + offsets. Chip-específico: load/store instruction pair.

**Primitiva nova:** `emit_struct_copy_word(src_off, dst_off, word_idx)` — lê
word em [src_base + word_idx*8] e escreve em [fp + dst_off + word_idx*8].

### T_MEMST sub-word dispatch

Já usa primitiva `emit_memst_store(field_hint)` em versões recentes
(Wave 8). Verificar se está 100% lifted — se sim, nada a fazer.

### T_SWITCH compare-chain (a64: ~40 linhas, x64: ~30 linhas)

Form 1 (value dispatch, sparse) tem:
```
// inline em T_SWITCH:
for (sk = 0; sk < sv_cnt; sk = sk + 1) {
    a64_emit_node(sv_arr[sk]);
    enc_ldr_uoff(1, 31, 0);         // peek saved cond from [sp]
    enc_cmp_reg(1, 0);              // compare to arm value
    enc_b_cond_label(0, smatch_lbl); // b.eq smatch_lbl
}
```

Primitivas novas:
- `emit_peek_into_scratch()` — load from [sp] into scratch reg (x1/rcx)
- `emit_cmp_scratch_acc()` — compare scratch vs acc
- `emit_branch_if_eq(label)` — branch to label on equal (already there as
  emit_branch_if_zero + invert?)

Form 2 (condition dispatch) tem:
```
if (a64_bin_mode) { enc_cbnz_label(0, smatch_lbl); }
else { out("  cbnz x0, .L"); ... }
```

Primitiva nova (ou reuse existente):
- `emit_branch_if_nonzero(label)` — complement of emit_branch_if_zero

### Default case (expression statement)

```
a64_emit_node(nd);
```

Já usa chip's emit_node. Quando emit_node for lifted (Wave 21), isto vira
`call(p.emit_node, nd)`.

---

## Primitivas novas do Wave 20 (4 novas)

```
struct ChipPrimitives {
    ...
    // ── Wave 20: statement orchestration ──

    // Copy one 64-bit word from [src_base + offset] (src_base in
    // acc / x0 / rax) into frame slot [fp + dst_off]. Used by
    // T_ASSIGN stack struct copy. Loop counter managed by spine.
    emit_struct_copy_word,  // (src_word_off, dst_word_off)

    // Peek the saved condition word at [sp + 0] into scratch (x1/rcx).
    // Used by T_SWITCH form 1 (value dispatch).
    emit_peek_cond,         // ()

    // Compare scratch vs acc; set flags for emit_branch_if_eq.
    // On a64 this is cmp x1, x0. On x64 it's cmp rcx, rax.
    emit_cmp_scratch_acc,   // ()

    // Branch to label if acc != 0 (complement of emit_branch_if_zero).
    // On a64: cbnz. On x64: test + jne.
    emit_branch_if_nonzero, // (label_id)
}
```

Plus recycle existing ones: `emit_branch_if_zero`, `emit_label`, `emit_jump`,
`emit_new_label`, `emit_push_acc`, `emit_pop_scratch`, etc.

---

## Spine implementation (pseudo-code)

```
// bpp_codegen.bsm

cg_emit_stmt(nd) {
    auto p: ChipPrimitives, n: Node, t;
    auto off, lbl, lbl2, arr, cnt, i, rty, vi, ety;

    if (nd == 0) { return 0; }
    p = cg_prim;
    n = nd; t = n.ntype;

    if (t == T_DECL) { return 0; }   // pre-registered

    if (t == T_ASSIGN) { cg_emit_assign(p, n); return 0; }
    if (t == T_MEMST)  { cg_emit_memst(p, n);  return 0; }

    // Control flow — Wave 7 primitives already in place, just
    // lift the orchestration.
    if (t == T_IF)       { cg_emit_if(p, n);       return 0; }
    if (t == T_BREAK)    { cg_emit_break(p);       return 0; }
    if (t == T_CONTINUE) { cg_emit_continue(p);    return 0; }
    if (t == T_WHILE)    { cg_emit_while(p, n);    return 0; }
    if (t == T_RET)      { cg_emit_ret(p, n);      return 0; }
    if (t == T_SWITCH)   { cg_emit_switch(p, n);   return 0; }

    if (t == T_NOP) { return 0; }
    if (t == T_BLOCK) { call(p.emit_body, n.b, n.c); return 0; }

    // Default: expression statement.
    call(p.emit_node, nd);
    return 0;
}

// Each cg_emit_* below is a portable helper — no enc_* calls,
// only primitives. They're static-local to bpp_codegen.bsm.

static cg_emit_if(p, n) {
    auto lbl, lbl2, ety;
    lbl  = call(p.emit_new_label);
    lbl2 = call(p.emit_new_label);
    ety = call(p.emit_node, n.a);
    if (ety == 1) { call(p.emit_fcond_to_int_truth); }
    call(p.emit_branch_if_zero, lbl);
    call(p.emit_body, n.b, n.c);
    call(p.emit_jump, lbl2);
    call(p.emit_label, lbl);
    if (n.e > 0) { call(p.emit_body, n.d, n.e); }
    call(p.emit_label, lbl2);
}

static cg_emit_switch(p, n) {
    // ~60 linhas no total — value dispatch form 1 + condition
    // dispatch form 2 + dense-jtbl early return.
    // Usa: emit_node, emit_push_acc, emit_pop_scratch,
    //      emit_peek_cond, emit_cmp_scratch_acc,
    //      emit_branch_if_eq (== branch_if_zero of (cmp-result)),
    //      emit_branch_if_nonzero, emit_new_label, emit_label,
    //      emit_jump.
    // Jump-table dense path stays chip-local via
    // call(p.emit_switch_jtbl, n, end_lbl) — that's a single
    // primitive, chip owns the table emission.
}

static cg_emit_assign(p, n) {
    auto lhs, vi, off, rty, i, fc, sidx;
    lhs = n.a;

    // Stack struct copy.
    if (lhs.ntype == T_VAR) {
        vi = cg_var_idx(lhs.a);
        if (vi >= 0 && arr_get(cg_var_stack, vi)) {
            call(p.emit_node, n.b);   // src ptr in acc
            off = arr_get(cg_var_off, vi);
            sidx = arr_get(cg_var_struct_idx, vi);
            fc = (struct_size(sidx) + 7) / 8;
            for (i = 0; i < fc; i = i + 1) {
                call(p.emit_struct_copy_word, i * 8, off + i * 8);
            }
            return;
        }
    }

    // Normal assignment path — already portable via primitives.
    rty = call(p.emit_node, n.b);
    if (lhs.ntype == T_VAR) {
        vi = cg_var_idx(lhs.a);
        if (vi >= 0) {
            off = arr_get(cg_var_off, vi);
            call(p.emit_store_local, vi, off, rty);
        } else {
            cg_emit_global_store(p, lhs.a, rty);
        }
    }
}
```

---

## Migration strategy (6 batches)

Mesmo approach do Wave 18/19: primitivas → lift → validate.

### Batch 1 — Add primitives (zero activation)

Adiciona os 4 slots em `ChipPrimitives`, implementa nos dois chips, wire nos
install. Nenhum callsite mudou ainda.

Primitivas:
- `emit_struct_copy_word(src_off, dst_off)`
- `emit_peek_cond()`
- `emit_cmp_scratch_acc()`
- `emit_branch_if_nonzero(label)`

**Gen1==gen2 + sh tests/run_all.sh.** Binário fica maior (código morto) mas
comportamento não muda.

### Batch 2 — Lift T_ASSIGN struct copy no chip emit_stmt

Dentro de `a64_emit_stmt` e `x64_emit_stmt`, substituir o loop de
ldr_uoff/str_uoff por `call(p.emit_struct_copy_word, ...)`. Chip ainda orquestra
o statement dispatch. **Gen1==gen2 + tests.**

### Batch 3 — Lift T_SWITCH compare chain

Dentro do T_SWITCH, substituir as chamadas diretas de `enc_ldr_uoff` /
`enc_cmp_reg` / `enc_b_cond_label` / `enc_cbnz_label` por `call(p.emit_peek_cond)`,
`call(p.emit_cmp_scratch_acc)`, `call(p.emit_branch_if_*)`. Dense-jtbl continua
via primitiva existente `emit_switch_jtbl`. **Gen1==gen2 + tests.**

### Batch 4 — Extract cg_emit_if / cg_emit_while / cg_emit_break / etc

Helpers portáveis no spine. Chip emit_stmt ainda chama eles via
`cg_emit_if(p, n)` etc. Helpers só usam primitivas. **Gen1==gen2 + tests.**

### Batch 5 — Spine takes over emit_stmt orchestration

Define `cg_emit_stmt(nd)` no spine. `a64_emit_stmt` / `x64_emit_stmt` viram
wrappers de 1 linha: `return cg_emit_stmt(nd);`. **Gen1==gen2 + tests — batch
de maior risco.**

### Batch 6 — Delete chip wrappers

Callers do a64_emit_stmt / x64_emit_stmt apontam direto pra cg_emit_stmt.
Remove a wrapper. **Gen1==gen2 + tests finais.**

---

## Riscos e mitigações

### Risco 1: T_SWITCH tem state compartilhado (cg_break_stack)

O break-stack é array global portável — já tá em `bpp_codegen.bsm`. Spine
manipula livremente. **Sem risco.**

### Risco 2: T_SWITCH dense jtbl stays chip-local

A emissão de jump-table (bytes da tabela + adr_label) é ISA-específica. Wave 11
criou `emit_switch_jtbl` como primitiva. Spine só chama. **Sem risco.**

### Risco 3: T_ASSIGN fallthrough to global store

Quando a variável não é local, cai em `cg_prim.emit_store_global`. Essa
primitiva já existe. **Sem risco.**

### Risco 4: T_MEMST já foi lifted?

Verificar em Wave 8/10 handoffs. Se `emit_memst_store` já faz todo o
sub-word dispatch, T_MEMST já está 100% via primitiva — Wave 20 só precisa
remover o wrapper chip-local e deixar spine dispatchar direto.

**Mitigação:** batch 1 inclui audit de T_MEMST. Se já lifted, skip.

### Risco 5: Expression statement chama emit_node chip-local

Default case do emit_stmt cai em `a64_emit_node(nd)` / `x64_emit_node(nd)`. Esses
ainda são chip-local (Wave 21). Spine pode chamar `call(p.emit_node, nd)` —
primitiva já existe (`p.emit_node` foi adicionado no Wave 18 scaffold).

**Sem risco, mas confirma que p.emit_node delega de volta pro chip's tree
walker até Wave 21 lifted.**

---

## Sucesso — como medir

Ao fim do Wave 20:

1. `src/backend/chip/aarch64/a64_codegen.bsm`: ~700 linhas (era ~900 pós-W19)
2. `src/backend/chip/x86_64/x64_codegen.bsm`: ~550 linhas (era ~700 pós-W19)
3. Nova função em `bpp_codegen.bsm`: `cg_emit_stmt` (~150 linhas)
4. Byte-identity: `gen1 == gen2` consistente
5. Regression gate: **112 passed / 0 failed** (não-GPU)
6. T_SWITCH / T_ASSIGN / T_IF duplicados eliminados

---

## Pós-Wave 20 — próximo é Wave 21 (emit_node)

O maior remaining — **1000+ linhas de tree walker** em cada chip. `emit_node`
dispatcha T_LIT, T_VAR, T_TERNARY, T_BINOP, T_UNARY, T_CALL (já lifted),
T_MEMLD, T_ADDR. Cada um tem shape-específico-por-tipo que já quase todo
usa primitivas de Waves 4-6. Falta orquestração.

Wave 21 é o ENDGAME do 3.4 original. Depois dele:
- `a64_codegen.bsm` ≈ 300 linhas (init + wrappers + raros edge cases)
- `x64_codegen.bsm` ≈ 250 linhas
- `bpp_codegen.bsm` ≈ 2500 linhas (todo o tree walker portável)

Chip layer vira **só** init + primitivas — o "Forth-portable" que o plano
original prometeu.

---

## Commands úteis

```bash
# Medida do estado atual:
wc -l src/backend/chip/aarch64/a64_codegen.bsm src/backend/chip/x86_64/x64_codegen.bsm

# Identificar chamadas inline restantes em emit_stmt:
awk '/^static void a64_emit_stmt/,/^}/ {print}' src/backend/chip/aarch64/a64_codegen.bsm | grep -cE "enc_|out\\("

# Self-host e regression (após cada batch):
./bpp src/bpp.bpp -o /tmp/bpp_gen1
cp /tmp/bpp_gen1 bpp
./bpp src/bpp.bpp -o /tmp/bpp_gen2
shasum bpp /tmp/bpp_gen2
sh tests/run_all.sh
```

---

## Referência cruzada

- **Plano original:** `_legacy_bootstrap/legacy_docs/phase_3_4_chip_primitives.md`
  — "Apply same pattern to emit_stmt" seção.
- **Wave 19 plan:** `docs/wave19_plan.md` (emit_func orchestration).
- **Wave 18 handoff:** `docs/wave18_handoff.md` (builtin dispatch, em execução).
- **Primitivas existentes:** `bpp_codegen.bsm::ChipPrimitives` (~45 slots;
  Wave 20 adiciona mais 4 totalizando ~49).

Tá pronto pra executar depois do Wave 19 fechar.
