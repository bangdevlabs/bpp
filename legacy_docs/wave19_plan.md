# Wave 19 Plan — Emit-Func Lift (orquestração)

**Status:** proposta. A ser executada DEPOIS do Wave 18 fechar (syscalls + fn_ptr +
vec). Este documento existe pra o próximo executor não começar do zero.

**Autor:** Claude Opus (sessão 2026-04-22, modo consultor durante execução
paralela do Wave 18 pelo agente emacs).

**Relação com plano 3.4 original:** O `legacy_docs/phase_3_4_chip_primitives.md`
(2026-04-20) descreveu **Waves 17-19** como "emit_func primitives" com primitivas
`emit_frame_setup`, `emit_frame_teardown`, `emit_save_callee_saved`, `emit_arg_copy`.
**As primitivas já foram implementadas** (o que o código rotula como Waves 13-15 é
equivalente): `_a64_emit_prologue`, `_a64_emit_arg_copy_int`, `_a64_emit_frame_teardown`
e seus parceiros x64. **O que falta é a ORQUESTRAÇÃO** — o spine assumir a
coordenação das primitivas em vez do chip's emit_func fazer tudo. Este
documento especifica essa orquestração faltante.

---

## Premissa

Após Wave 18 completar, o chip layer ainda vai conter `a64_emit_func` /
`x64_emit_func` — ~200 linhas de lógica de **emissão de função** (prólogo,
frame layout, arg copy, body walk, epílogo). **~80% desse código é portável**
mas está duplicado entre os dois chips.

Wave 19 extrai essa parte portável pro spine (`bpp_codegen.bsm`) e deixa no
chip apenas as primitivas que ainda precisam conhecer a ISA.

**Primitivas que JÁ EXISTEM (Waves 13-15, não redefinir):**
- `emit_prologue(name_p, fn_lbl, frame_size)` — prólogo completo
- `emit_arg_copy_int(param_idx, frame_off)` — copia arg GP
- `emit_frame_teardown(frame_size)` — epílogo completo
- `emit_arg_copy_flt(param_idx, frame_off)` — existe mas está vazia (a64 não
  tinha callers, x64 idem). Wave 19 NÃO ativa essa — deferred.

**Resultado esperado:**
- `a64_codegen.bsm` encolhe de ~1400 pra ~900 linhas
- `x64_codegen.bsm` encolhe de ~1100 pra ~700 linhas
- Uma única implementação de `cg_emit_func` vive no spine
- Frame layout math deixa de ter duas cópias quase idênticas

---

## Diagnóstico — o que tem dentro de emit_func hoje

Estrutura idêntica nos dois chips (marcações: **P**ortável / **C**hip-específico):

```
emit_func(fi):
  (P) 1. Extract metadata (name, params, body)
  (P) 2. Reset var tracking (arr_clear em ~10 arrays)
  (P) 3. Register params como vars + aplicar type hints
  (C) 4. Pre-register body vars (a64_pre_reg_vars / x64_pre_reg_vars)
  (C) 5. Compute per-var frame offsets
            a64: positivo, crescendo UP from fp, scratch=40
            x64: negativo, crescendo DOWN from rbp, scratch=24
  (P) 6. B3 ref counting walk (já usa cg_b3_walk genérico)
  (C) 7. B3 promotion select (_a64_b3_select / _x64_b3_select)
  (P) 8. Frame size: round up pra 16
  (C) 9. Prologue (já é primitiva — _a64_emit_prologue/_x64_emit_prologue) ✓
  (C) 10. main()-specific argc/argv/envp save to globals
  (C) 11. Incoming arg copy (já é primitiva — _a64/x64_emit_arg_copy_int) ✓
  (C) 12. Zero non-param promoted locals (reg-zero instruction)
  (C) 13. a64-only: narrow float sub-type params (SL_HALF / SL_QUARTER fcvt)
  (P) 14. Emit body (delegated — a64_emit_body / x64_emit_body)
  (C) 15. Implicit return 0 (mov x0, #0 / xor rax, rax)
  (C) 16. Epilogue (já é primitiva — _a64/x64_emit_frame_teardown) ✓
```

**Já em primitivas (de ondas anteriores):** prologue, arg_copy_int, epilogue.  
**Falta virar primitiva:** pre_reg_vars, b3_select, compute_var_offsets,
save_main_runtime_globals, zero_reg, narrow_float_params (a64 only),
implicit_ret_zero, emit_body.

---

## Primitivas novas do Wave 19 (8 novas)

Para `ChipPrimitives` em `bpp_codegen.bsm`:

```
struct ChipPrimitives {
    ...
    // ── Wave 19: function frame emission ──

    // Walk body pre-declaring variables. Chip owns this because
    // struct-slot sizes and SIMD alignment constraints affect the
    // pre-reg step (a64 and x64 have different 128-bit handling).
    pre_reg_vars,           // (body_arr, body_cnt)

    // Compute per-variable frame offsets and fill cg_var_off.
    // Returns `total_vars_bytes` — excluding the hidden scratch area.
    // Spine uses it to compute final frame size after B3 spills.
    compute_var_offsets,    // () → total_vars_bytes

    // Return the hidden scratch area size that sits between fp
    // and the first variable slot. On a64 = 40 (fp/lr + d0/d1
    // save slots + padding). On x64 = 24 (rbp + xmm0/xmm1 save).
    scratch_base,           // () → int

    // Chip's B3 register promotion selector. Walks cg_vars and
    // assigns promoted regs to cg_var_promote. Chip-specific
    // because each ISA has a different free-reg budget.
    b3_select,              // ()

    // Save argc / argv / envp from the current function's arg
    // registers into the runtime-global slots. Only emitted when
    // the function name is "main". a64 uses x0/x1/x2; x64 uses
    // rdi/rsi/rdx (already in arg-reg positions).
    save_main_runtime_globals, // ()

    // Zero a GP register. enc_mov_wide(reg, 0) on a64;
    // x64_enc_xor_zero(reg) on x64. Used to zero promoted locals
    // that aren't also parameters.
    emit_zero_reg,          // (reg)

    // a64-only: narrow float sub-type params. For each param
    // typed `: half` or `: quarter`, load d0 from frame, fcvt to
    // s0 / h0, store back. On x64 this is a no-op (x64 doesn't
    // encode sub-word float types at the param boundary yet).
    emit_narrow_float_params, // (par_cnt)

    // Emit `mov return-reg, 0` for the fallthrough implicit return.
    // a64: enc_mov_wide(0, 0). x64: x64_enc_xor_zero(0).
    emit_implicit_ret_zero, // ()

    // Walk the function body emitting each statement. Chip's
    // emit_stmt is still chip-local after Wave 19 (that's a later
    // wave). This primitive delegates to a64_emit_body /
    // x64_emit_body unchanged.
    emit_body,              // (body_arr, body_cnt)
}
```

---

## Spine implementation (pseudo-code)

```
// bpp_codegen.bsm

cg_emit_func(fi) {
    auto p: ChipPrimitives, name_p, par_arr, par_cnt, body_arr, body_cnt;
    auto frame, i, total;

    p = cg_prim;

    // 1. Extract metadata (portable)
    name_p = arr_get(cg_fn_name, fi);
    par_arr = arr_get(cg_fn_par, fi);
    par_cnt = arr_get(cg_fn_pcnt, fi);
    body_arr = arr_get(cg_fn_body, fi);
    body_cnt = arr_get(cg_fn_bcnt, fi);

    // 2. Reset var tracking (portable)
    cg_reset_var_tables();
    cg_cur_fn_name = name_p;
    cg_cur_fn_idx = arr_get(cg_fn_fidx, fi);

    // 3. Register params with hints (portable)
    cg_register_params(par_arr, par_cnt);

    // 4. Pre-register body vars (primitive — chip does SIMD stride)
    call(p.pre_reg_vars, body_arr, body_cnt);

    // 5. Compute offsets (primitive — each chip has own sign/scratch)
    total = call(p.compute_var_offsets);

    // 6. B3 ref walk (portable)
    auto stmt_i: Node;
    for (i = 0; i < body_cnt; i = i + 1) {
        stmt_i = body_arr[i];
        if (stmt_i != 0) { cg_b3_walk(stmt_i); }
    }

    // 7. B3 select (primitive)
    call(p.b3_select);

    // Spine records spill base, adds spill slots to total.
    cg_spill_base = call(p.scratch_base) + total;
    total = total + cg_promoted_reg_count() * 8;

    // 8. Frame size (portable)
    frame = call(p.scratch_base) + total;
    frame = (frame + 15) / 16 * 16;

    // 9. Prologue (existing primitive)
    call(p.emit_prologue, name_p, cg_fn_label(fi), frame);

    // 10. main() runtime globals (primitive)
    if (cg_str_eq(name_p, "main", 4)) {
        call(p.save_main_runtime_globals);
    }

    // 11. Arg copy (existing primitive loop)
    i = 0;
    while (i < par_cnt) {
        call(p.emit_arg_copy_int, i, arr_get(cg_var_off, i));
        i = i + 1;
    }

    // 12. Zero promoted non-param locals (primitive per reg)
    auto vi, n_vars, preg;
    n_vars = arr_len(cg_vars);
    for (vi = par_cnt; vi < n_vars; vi = vi + 1) {
        preg = arr_get(cg_var_promote, vi);
        if (preg >= 0) { call(p.emit_zero_reg, preg); }
    }

    // 13. Narrow float params (primitive — no-op on x64)
    call(p.emit_narrow_float_params, par_cnt);

    // 14. Body (primitive — still chip-local)
    cg_depth = 0;
    call(p.emit_body, body_arr, body_cnt);

    // 15. Implicit return 0 (primitive)
    call(p.emit_implicit_ret_zero);

    // 16. Epilogue (existing primitive)
    call(p.emit_frame_teardown, frame);
}
```

Note: `cg_reset_var_tables()` and `cg_register_params()` e `cg_b3_walk()` são
helpers portáveis que já vivem (ou deveriam viver) no spine — a maioria já existe,
é só consolidar.

---

## Migration strategy (batches pra não quebrar byte-identity)

Replicando a disciplina do Wave 18: cada passo é bootstrap + byte-identity +
testes antes do próximo.

### Batch 1 — Primitivas zeradas + body delegation

Adiciona slots na `ChipPrimitives` (novas 8), wire nos install, implementa apenas
as triviais:

- `emit_implicit_ret_zero`
- `emit_zero_reg`
- `emit_body` (delega pra a64_emit_body / x64_emit_body)
- `scratch_base` (returns 40 / 24)

**Gen1==gen2 check. Testes.** Nenhum callsite mudou ainda.

### Batch 2 — Lift implicit_ret_zero + emit_body no chip emit_func

Dentro de `a64_emit_func` e `x64_emit_func`, substituir as chamadas inline por
`call(p.emit_implicit_ret_zero)` e `call(p.emit_body, ...)`. Também trocar o zero
dos promoted locals por `call(p.emit_zero_reg, reg)`.

**Gen1==gen2 check. Testes.** Chip emit_func fica mais curto.

### Batch 3 — Lift b3_select + save_main_runtime_globals

Adiciona as duas primitivas (bodies reais). Spine ainda não existe — chip emit_func
chama as primitivas localmente.

**Gen1==gen2 check. Testes.**

### Batch 4 — Lift emit_narrow_float_params

Mesma estrutura — primitiva + chip emit_func passa a chamar. x64 é stub (return
empty).

**Gen1==gen2 check. Testes.**

### Batch 5 — Lift compute_var_offsets + pre_reg_vars

Aqui o cuidado é com `total` retornado + estado compartilhado (cg_spill_base).
A primitiva deve encapsular tudo e deixar spine só recebendo o inteiro.

**Gen1==gen2 check. Testes.**

### Batch 6 — Spine takes over emit_func

Define `cg_emit_func(fi)` no spine. Chip's `a64_emit_func` / `x64_emit_func` viram
wrappers de 1 linha: `return cg_emit_func(fi);`. Roda todos os testes. Se passa,
next batch.

**Gen1==gen2 check. Testes.** Este é o batch de maior risco — toda a orquestração
muda de dono.

### Batch 7 — Delete chip wrappers

Remove `a64_emit_func` / `x64_emit_func` completamente. Callers (bpp.bpp etc)
apontam direto pra `cg_emit_func`.

**Gen1==gen2 check. Testes finais.**

---

## Riscos e mitigações

### Risco 1: frame offset sign

a64 e x64 têm sinais inversos: a64 offsets são positivos (`+40, +48, +56, ...`
crescendo para cima do fp), x64 são negativos (`-24, -32, -40, ...` crescendo para
baixo do rbp). A primitiva `compute_var_offsets` encapsula isso inteiro — spine
não precisa saber.

**Mitigação:** primitivas manipulam cg_var_off, spine só lê. Nunca spine cria
offset numericamente.

### Risco 2: SIMD alignment dança

a64 precisa `abs_off` múltiplo de 16 pra STR Q; x64 precisa idem pra MOVUPS (ou
MOVAPS se alguém trocar). A aritmética é distinta:
- a64: `abs_off = 40 + total; if SIMD: round(abs_off, 16)`
- x64: `abs_mag = 24 + total; if SIMD: round(abs_mag, 16); off = -(abs_mag+16)`

**Mitigação:** fica 100% dentro de `compute_var_offsets` da chip. Spine nunca vê.

### Risco 3: B3 spill base compartilhada

`a64_b3_spill_base` e `x64_b3_spill_off` são variáveis globais chip-específicas
usadas pelo B3 spiller. Wave 19 precisa alinhar nomes ou expor via primitiva:

```
// chip primitive:
record_spill_base(total)  // chip faz a64_b3_spill_base = 40 + total
```

**Mitigação:** adicionar essa primitiva no Batch 5. Nome único global
`cg_spill_base_chip_local` setado pela primitiva.

### Risco 4: main()-specific logic

O save de argc/argv/envp só roda em main(). Spine pode ter `if (cg_str_eq(name_p,
"main", 4))` e chamar primitiva. Alternativamente, a primitiva pode se auto-skip
(verifica `cg_cur_fn_name`).

**Recomendação:** spine checa, primitiva só emite. Separação de responsabilidade
mais clara.

### Risco 5: flood do `call()` 2+ args

Wave 18 hit o limite do "call com mais de 2 args" (hipótese não provada mas
observada). Wave 19 usa primitivas com 1-2 args principalmente, exceto
`emit_prologue(name_p, fn_label, frame)` — **3 args** — que já funciona de ondas
anteriores.

**Mitigação:** se o bug do Wave 18 (call 4-args) ficar confirmado, Wave 19 evita
esse pattern. Já evita naturalmente: nenhuma primitiva nova tem 4+ args.

---

## Sucesso — como medir

Ao fim do Wave 19:

1. `src/backend/chip/aarch64/a64_codegen.bsm`: ~900 linhas (era ~1400)
2. `src/backend/chip/x86_64/x64_codegen.bsm`: ~700 linhas (era ~1100)
3. Nova função em `bpp_codegen.bsm`: `cg_emit_func` (~80 linhas)
4. Byte-identity: `gen1 == gen2` consistente
5. Regression gate: **112 passed / 0 failed** (não-GPU)
6. Duplicação de frame-math eliminada: agora tudo que é ABI-específico vive
   em uma primitiva com responsabilidade única

---

## Pós-Wave 19

**Wave 20:** emit_stmt orchestration + T_SWITCH/T_ASSIGN lifts. Documento
separado: `docs/wave20_plan.md`. Controles de fluxo (T_IF/T_WHILE/T_RET/etc) já
usam primitivas via Wave 7 — só falta lift orchestration + 3 casos residuais.

**Wave 21 (endgame do 3.4):** emit_node lift — o tree walker inteiro (~1000
linhas por chip) vira spine. É o Wave original do 3.4 "Waves 1-12". Quando isso
fechar, chip layer é **só** init + primitivas.

**Chip layer endgame projetado:** ~300 linhas de init + primitivas + raros
edge cases. "Forth-portable" como o 3.4 original prometeu.

---

## Comandos úteis para o executor

```bash
# Medida inicial (antes do Wave 19):
wc -l src/backend/chip/aarch64/a64_codegen.bsm src/backend/chip/x86_64/x64_codegen.bsm

# Identificar todas as chamadas diretas (não via primitiva) no chip:
grep -n "^\s*enc_\|^\s*x64_enc_" src/backend/chip/aarch64/a64_codegen.bsm | wc -l
grep -n "^\s*enc_\|^\s*x64_enc_" src/backend/chip/x86_64/x64_codegen.bsm | wc -l

# Self-host check (após cada batch):
./bpp src/bpp.bpp -o /tmp/bpp_gen1
cp /tmp/bpp_gen1 bpp
./bpp src/bpp.bpp -o /tmp/bpp_gen2
shasum bpp /tmp/bpp_gen2   # devem bater

# Regression gate:
sh tests/run_all.sh
```

---

## Referência cruzada

- **Wave 18 status:** em execução pelo agente emacs. Ver `docs/wave18_handoff.md`.
- **Primitivas existentes:** `bpp_codegen.bsm` struct `ChipPrimitives` (~45
  slots atualmente)
- **Fat-primitive precedente:** `_a64_emit_prologue` em `a64_primitives.bsm` é o
  modelo de referência — body completo dentro da primitiva, não é "thin wrapper".

Boa sorte. O chip layer agradece.
