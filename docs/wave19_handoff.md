# Wave 19 Handoff — 2026-04-22 (agente emacs, sessão pós-Wave-18)

## Estado atual

- **bpp:** `f05af48296c9c5abc66c8c160700a5b5bbbf3bd0`, commit `c18e1f7` (Wave 19 batch 1).
- **Wave 18:** completa (30 builtins liftados, 13 intencionalmente chip-local).
- **Wave 19 batch 1:** scaffolding em produção. 8 slots novos em `ChipPrimitives`; 4 primitivas wired com body real; 4 primitivas restantes declaradas mas unbound.
- **Suite:** 114 passed / 0 failed / 11 skipped. `test_gpu_circle/clear/shapes` podem flakar ambientalmente se display apertar; rerodar se acontecer.

## O que já tá pronto (batch 1)

### Slots novos em `ChipPrimitives` (bpp_codegen.bsm)

```
fn_pre_reg_vars,          // (body_arr, body_cnt)
fn_compute_offsets,       // () → total_vars_bytes (fills cg_var_off)
fn_scratch_base,          // () → hidden-scratch size
save_main_runtime_globals,// () — argc/argv/envp save
emit_zero_reg,            // (reg) — reg ← 0
emit_narrow_float_params, // (par_cnt) — a64 half/quarter fcvt
emit_implicit_ret_zero,   // () — return-reg ← 0
emit_body,                // (body_arr, body_cnt) — chip body walker
```

### Primitivas com corpo real (ambos chips)

- `_a64_fn_scratch_base` → 40, `_x64_fn_scratch_base` → 24.
- `_a64_emit_zero_reg(reg)`, `_x64_emit_zero_reg(reg)`.
- `_a64_emit_implicit_ret_zero()`, `_x64_emit_implicit_ret_zero()`.
- `_a64_emit_body_prim(body, cnt)` / `_x64_emit_body_prim(body, cnt)` (delegators).

Primitivas já bindadas em `cg_install_<chip>_primitives` (slots `fn_scratch_base`, `emit_zero_reg`, `emit_implicit_ret_zero`, `emit_body`).

### Ainda dead code

Nenhum call site usa ainda — `a64_emit_func` e `x64_emit_func` continuam com o corpo original inline.

## O que falta (batches 2-6)

Seguir `docs/wave19_plan.md` literalmente.

### Batch 2 — Substituir chamadas inline por primitives

Dentro de `a64_emit_func` (linhas ~1843-2090 em `a64_codegen.bsm`) e `x64_emit_func`:

- Linha `enc_mov_wide(_z_preg, 0)` (bin mode) e `out("  mov x", dec, ", #0")` (asm mode) → `call(p.emit_zero_reg, _z_preg)`.
- Linha `enc_mov_wide(0, 0); out("  mov x0, #0")` (implicit return) → `call(p.emit_implicit_ret_zero)`.
- Chamada `a64_emit_body(body_arr, body_cnt)` → `call(p.emit_body, body_arr, body_cnt)`.

Mesma troca no x64 (`x64_enc_xor_zero(preg)` → primitive, etc).

**Checkpoint:** bootstrap, gen1==gen2, suite 114/0. Byte-identity obrigatória (pura refactoração — bytes idênticos).

### Batch 3 — Implementar b3_select + save_main_runtime_globals primitives

`_a64_b3_select_prim()` e `_x64_b3_select_prim()` estão com body `{ }` (linhas ~665-667 a64 e ~348 x64). Preencher:

```
void _a64_b3_select_prim() { _a64_b3_select(); }
void _x64_b3_select_prim() { _x64_b3_select(); }
```

Adicionar `_a64_save_main_runtime_globals()` / `_x64_save_main_runtime_globals()` extraindo o bloco `if (cg_str_eq(name_p, "main", 4))` de `emit_func` (ambos bin+asm modes pro a64). Bind nos installers.

Substituir inline no `emit_func` por `call(p.b3_select)` e `call(p.save_main_runtime_globals)` (só quando name == "main").

**Checkpoint:** gen1==gen2, suite 114/0.

### Batch 4 — emit_narrow_float_params primitive

Extract bloco `while (i < par_cnt) { ... SL_HALF / SL_QUARTER ... }` do `a64_emit_func` (linhas ~1956-1969 bin mode e ~2018-2032 asm mode) pra `_a64_emit_narrow_float_params(par_cnt)`.

x64 stub vazio:

```
void _x64_emit_narrow_float_params(par_cnt) { }
```

Bind em ambos installers. Substituir inline por `call(p.emit_narrow_float_params, par_cnt)`.

**Checkpoint:** gen1==gen2, suite 114/0.

### Batch 5 — pre_reg_vars + compute_var_offsets primitives

Wrappers pras funções `static` existentes:

```
void _a64_fn_pre_reg_vars(arr, cnt) { a64_pre_reg_vars(arr, cnt); }
void _x64_fn_pre_reg_vars(arr, cnt) { x64_pre_reg_vars(arr, cnt); }
```

Para `fn_compute_offsets`: extrair o bloco `total = 0; for (i = 0; i < arr_len(cg_vars); ...) { abs_off = 40 + total; ... }` de `a64_emit_func` (linhas ~1890-1907) pra primitive que retorna `total`. Também expõe spill_base setando `cg_spill_base_chip_local` global (novo).

Mesmo pra x64, com sua aritmética inversa (offsets negativos) — ver wave19_plan.md §"Risco 1 — frame offset sign".

Spine exposes novo global:
```
// bpp_codegen.bsm:
auto cg_spill_base_chip_local;  // set by fn_compute_offsets, read by spine
```

Bind ambos installers. `emit_func` substitui inline:
```
call(p.fn_pre_reg_vars, body_arr, body_cnt);
total = call(p.fn_compute_offsets);
```

**Checkpoint:** gen1==gen2, suite 114/0. ALTO RISCO — offset math tem que bater bit-a-bit.

### Batch 6 — Spine takeover

Criar `cg_emit_func(fi)` em `bpp_codegen.bsm` seguindo o pseudo-code do plano §138-224. Chip's `a64_emit_func` / `x64_emit_func` viram wrappers de 1 linha:

```
void a64_emit_func(fi) { cg_emit_func(fi); }
```

Também precisa portar helpers portáveis (`cg_reset_var_tables`, `cg_register_params`) pro spine se ainda não existem.

**Checkpoint:** gen1==gen2 obrigatória. Suite 114/0. ESTE É O PONTO DE MAIOR RISCO. Se diverge, reverte e bisecta.

Commit de Wave 19 fecha aqui.

### Wave 20 Batch 0 — cleanup Wave 19

Deletar wrappers `a64_emit_func` / `x64_emit_func`. Caller deve apontar direto pro `cg_emit_func` em `bpp.bpp` e em qualquer outro lugar.

## Riscos conhecidos (da revisão do consultor)

- **Frame offset sign inverso** — a64 positivo, x64 negativo. A primitive encapsula, spine só lê.
- **SIMD alignment** — a64 rounds abs_off pra 16, x64 rounds mag. Dentro da primitive.
- **B3 spill base** — variável global chip-specific. Expose via `cg_spill_base_chip_local` setado pela primitive.
- **main() check** — spine faz `cg_str_eq(name_p, "main", 4)` e chama primitive se match.
- **Batch 6 spine takeover** — maior risco. Se gen2 mute-crash ou diverge, reverte imediatamente.

## Referências rápidas

- Plano detalhado: `docs/wave19_plan.md`.
- Wave 18 closed commits: `e033564` (B1), `bd8696e` (B2), `bcd5bb9` (B3), `d0db5f5` (B4), `8ab31a0` (B5+close).
- Wave 19 batch 1: `c18e1f7`.
- Schedule realista: Wave 19 completa ~3-4h; Waves 20-21 mais ~5-7h.

## Checklist por batch

1. Editar arquivos.
2. `./bpp src/bpp.bpp -o /tmp/bpp_gen1`.
3. `/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2`.
4. `shasum /tmp/bpp_gen1 /tmp/bpp_gen2` — bater.
5. `cp /tmp/bpp_gen1 bpp`.
6. `sh tests/run_all.sh` — 114/0 (ou com 3 GPU flake, rerodar).
7. `git commit` com mensagem padrão `Wave 19 batch N: <descrição>`.

Se qualquer passo diverge: `git checkout` nos arquivos afetados e reabrir.

## Por que parei aqui

Sessão já vinha longa (Wave 18 completa + Batch 1). Waves 19/20/21 exigem byte-identity ESTRITA nos takeovers — pressa quebra isso fácil. Natural boundary 1 atingida (Wave 18 complete) + pre-aquecimento Wave 19 (Batch 1). Próxima sessão arranca do Batch 2 com cabeça fresca.

Boa bola.
