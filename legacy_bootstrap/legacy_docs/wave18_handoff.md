# Wave 18 Handoff — 2026-04-22

Quem pega a bola: **agente do emacs**. Quem entrega: agente Claude Code (sessão
do dia 21-22/04 a partir do commit `f727514`).

## Estado do repo no momento do hand-off

- **bpp atual:** não commitado. Hash `9c64099abc6a0fd6740db5378d3048091a54d9bb`.
- **Último commit:** `f727514` (Wave 18: assert lift).
- **Working tree sujo:** bpp + 5 arquivos com a lift de **batch 1** (argc_get /
  argv_get / envp_get) + primitivas Wave 18 novas. Tudo bate self-host gen2==gen3.

Testes: **109 passed / 3 failed / 11 skipped.** Os 3 fails são `test_gpu_clear /
test_gpu_circle / test_gpu_shapes` — exclusivamente flakiness do sandbox sem
display (provado rodando o mesmo HEAD 3x no commit anterior sem Wave 18 e dando
os mesmos 3 fails). No ambiente do usuário com display de verdade: 112/0.

## O que foi concluído nesta sessão (4 commits em cima de f727514)

| Commit | Conteúdo |
|--------|----------|
| `9eee36c` | Wave 18: float_ret + float_ret2 lift + parser B.1/B.2 sweep |
| `868673b` | Fix 3 regressions: `_stb_get_time_us()` + tests sem `bpp_mem.bsm` |
| `f727514` | Wave 18: assert lift (8º builtin) |
| (uncommited) | **Wave 18: argc_get / argv_get / envp_get lift (batch 1)** — 3 builtins a mais, 3 novas primitivas (`emit_load_runtime_global`, `emit_index_word`, mais `emit_syscall` / `emit_syscall_norm_err` stubbed), sys_exit de volta como inline. |

**Placar Wave 18: 11 builtins lifted / ~34 restantes.**

Lifted: `peek`, `poke`, `str_peek`, `mem_barrier`, `shr`, `float_ret`,
`float_ret2`, `assert`, `argc_get`, `argv_get`, `envp_get`.

Pendentes: todos os `sys_*` (21), `fn_ptr`, `call`, `vec_*` (~9).

## O bug que me travou

### Resumo em uma linha

Importar **os dois** `_bsys_<os>.bsm` no spine (`bpp_codegen.bsm`) colide em
todo nome compartilhado — `BSYS_WRITE = 4` no macOS e `= 1` no Linux. A segunda
definição sobrescreve a primeira no escopo do spine, então o compilador
macOS gera `mov x16, #1 ; svc #0x80` para sys_write, que invoca
`sys_exit` no macOS. Binário resultante crasha imediatamente (SIGSEGV 139 ou
sai com lixo).

### Tentativa que falhou

Minha primeira arquitetura foi:
```
// bpp_codegen.bsm
import "_bsys_macos.bsm";
import "_bsys_linux.bsm";   // sobrescreve o macos ⚠

if (cg_str_eq(n.a, "sys_write", 9)) {
    cg_syscall_std(p, n, BSYS_WRITE, 3, -1, 0, 1);  // BSYS_WRITE=1 aqui, ERRADO pra macOS
    return 1;
}
```

### Tentativa que **deveria** funcionar mas crashou (e eu não isolei a causa)

Depois tentei passar o nome do syscall como packed-ref via `n.a` e deixar
o chip mapear pro próprio BSYS:

```
// Spine:
call(p.emit_syscall, n.a, argc, extra_slot, extra_val);

// a64_primitives.bsm:
static _a64_sys_num(name_p) : base {
    if (cg_str_eq(name_p, "sys_write", 9)) { return BSYS_WRITE; }   // corretamente 4
    ...
}
void _a64_emit_syscall(name_p, arg_count, extra_slot, extra_val) {
    num = _a64_sys_num(name_p);
    enc_mov_wide(16, num);
    enc_svc(0x80);
}
```

O compilador buildou `gen1`, bootstrap gen1→gen2 foi feito, mas gen2 crashou
na primeira tentativa de escrever em stdout (`bpp` sem argumentos deveria
imprimir help, ficou mudo e saía com código 0 sem produzir output). O binário
não produzia arquivo de saída mesmo em `echo 'main(){return 0;}' > /tmp/t.bpp ; bpp /tmp/t.bpp -o /tmp/t`.

### Hipóteses não-verificadas do bug

1. **O mais provável:** passagem de `n.a` (packed ref, 64 bits codificando
   offset+length) através de `call(fn_ptr, arg, ...)` não preserva o valor
   como eu assumia. Teste mínimo: compilar um helper que pega `name_p` e
   só faz `cg_str_eq(name_p, "sys_write", 9)` e imprimir o resultado.
2. **Menos provável mas possível:** ordem de inicialização das primitivas.
   `p.emit_syscall = fn_ptr(_a64_emit_syscall)` é feito em
   `cg_install_arm64_primitives()` que roda no primeiro chamada init. Se
   algo chama `emit_syscall` antes disso, pega slot zero e crasha.
3. **Também possível:** `_a64_sys_num` tem anotação `: base` igual aos
   outros helpers, mas talvez a passagem de `name_p` por valor entre
   funções anotadas seja diferente. Verificar se preservar a annotation
   através de calls é garantido.

### Como eu reproduziria se pegasse o bastão de volta

```bash
# Ponto de partida: retornar ao bpp 9c64099a (estado atual),
# reaplicar a arquitetura name-based:
# 1) Adicionar primitivas emit_syscall/emit_syscall_norm_err com
#    signature (name_p, argc, extra_slot, extra_val) nos dois chips.
# 2) Adicionar ChipPrimitives slots.
# 3) Adicionar _a64_sys_num / _x64_sys_num helpers.
# 4) No spine, um único builtin (começar com sys_close que é
#    1 arg, normalize=1, nenhum extra_slot) dispatched via
#    cg_syscall_std.
# 5) Bootstrap.
# 6) Se gen2 crasha: rodar gen1 sobre um programa mínimo que só chama
#    sys_close e nada mais. Dump do assembly emitido (bpp --emit-asm
#    se existir, ou compilar pra .o + objdump) pra ver se mov x16
#    pegou o número certo.
# 7) Se o número está certo no asm mas ainda crasha: bug tá em
#    outro lugar (inicialização de primitiva? ordenação?). Se o
#    número está errado: bug em _a64_sys_num / passagem de name_p.
```

## Abordagem alternativa (menos invasiva)

Se o bug acima for impossível de isolar, existe um **plano B**: definir um
enum portável de syscall-IDs no spine e cada chip mapeia internamente.

```
// bpp_codegen.bsm
const CG_SYS_WRITE = 0;
const CG_SYS_READ = 1;
const CG_SYS_OPEN = 2;
...

// spine dispatch:
if (cg_str_eq(n.a, "sys_write", 9)) {
    cg_syscall_std(p, n, CG_SYS_WRITE, 3, -1, 0, 1);
    return 1;
}

// a64_primitives.bsm:
static _a64_sys_num(id) : base {
    if (id == CG_SYS_WRITE) return BSYS_WRITE;  // = 4 no macOS
    if (id == CG_SYS_READ)  return BSYS_READ;
    ...
}
```

A vantagem: o `id` é um int, sem risco de passagem através de `call`
perder informação. Desvantagem: duplica enumeração em 2 lugares (spine
+ cada chip), mas o spine já tem ~45 builtins então é manageable.

## Primitivas Wave 18 que **já existem** no trunk

Em `ChipPrimitives` (bpp_codegen.bsm):
- `emit_node(node) → ety` — tree walker chip
- `emit_mem_read(width)`
- `emit_mem_read_indexed(width)`
- `emit_mem_write(val_reg, width)`
- `emit_push_acc()`
- `emit_pop_scratch()`
- `emit_coerce_float_to_int()`
- `emit_mem_barrier()`
- `emit_shift_right_logical()`
- `emit_load_float_ret(slot)` — 0 ou 1
- `emit_trap_if_zero()`
- **`emit_load_runtime_global(sym_id)`** (novo nesta sessão)
- **`emit_index_word()`** (novo)
- **`emit_syscall(name_p, argc, extra_slot, extra_val)`** (novo, **stubbed** — não chama nada; spine não dispatcha syscalls)
- **`emit_syscall_norm_err()`** (novo, stubbed)

Os dois últimos stubbed estão prontos pra receber a lógica real
no plano A ou plano B acima. A spine tem `cg_argc_sym` / `cg_argv_sym`
/ `cg_envp_sym` como globais (já estavam antes, agora consumidas pelo
spine dispatch).

## Outros itens pendentes (fora do Wave 18)

- **D2.3 — Bang 9 inspector panel lê meta.json sidecar.** Todo o
  pipeline produtor (`--emit-meta`) já tá commitado (fff066a, c972832);
  falta o lado consumidor na UI do Bang 9.
- **Phase D — stb malloc audit.** Sessão dedicada, não tocado aqui.
- **Phase C — Absorver legacy_docs nos capítulos novos.** Idem.

## Coisa boa que caiu por tabela

- `_stb_get_time_us()` agora é primitiva de plataforma em `_core_macos`
  e `_core_linux` (mach_absolute_time / CLOCK_MONOTONIC com resolução
  de microssegundo real). `bpp_beat.bsm` usa de verdade no
  `beat_now_us` / `beat_now_ns` — antes era `_stb_get_time() * 1000`
  (mentira com precisão de ms). Isso destravou test_bpp_bench que
  passou a medir 134 µs num loop de 100k iter (antes 0).
- Parser B.1/B.2 sweep completado — 5 callsites migrados pra API
  canônica `struct_find` / `struct_size` / `enum_register` /
  `enum_find` / `enum_value`.

## Comandos úteis

```bash
# Self-host check
./bpp src/bpp.bpp -o /tmp/bpp_gen1
cp /tmp/bpp_gen1 bpp
./bpp src/bpp.bpp -o /tmp/bpp_gen2
shasum bpp /tmp/bpp_gen2   # devem bater

# Regression gate
sh tests/run_all.sh

# Ver o estado dos builtins no chip vs spine
grep 'cg_str_eq(n.a, "' src/bpp_codegen.bsm | wc -l   # spine
grep 'cg_str_eq(n.a, "' src/backend/chip/aarch64/a64_codegen.bsm | wc -l
grep 'cg_str_eq(n.a, "' src/backend/chip/x86_64/x64_codegen.bsm | wc -l
```

Boa trincheira. Cansei.

---

## Addendum — análise read-only (sessão Claude Code 22/04 tarde)

Peguei o handoff, inspecionei o código sem tocar em nada. Três achados que mudam as prioridades.

### Achado 1 — O tree atual está seguro, a crash NÃO é reproduzível do HEAD

`_a64_emit_syscall` e `_x64_emit_syscall` estão com body `return;` no commit atual (`src/backend/chip/aarch64/a64_primitives.bsm:924`, `src/backend/chip/x86_64/x64_primitives.bsm:518`). O spine em `bpp_codegen.bsm` **não dispatcha syscall pelo slot novo** — syscalls continuam na ladder chip-local.

Implicação: o próximo agente não reproduz a crash rodando `bpp` atual. A crash só volta se ele re-implementar plano A do zero. A base atual é boa pra branch.

### Achado 2 — Hipótese 1 precisa ser reformulada

A hipótese no handoff era "passagem de `n.a` (packed ref) através de `call(fn_ptr, arg, ...)` não preserva o valor". Evidência que **fura** isso:

- `bpp_codegen.bsm:393` faz `call(p.emit_load_global, packed, 1)` — passa packed ref via fn_ptr, **funciona hoje**.
- `bpp_codegen.bsm:396` idem, `call(p.emit_load_global, packed, 0)`.

Então packed refs **sobrevivem** o mecanismo fn_ptr+call. Mas:

- **Zero** sites de `call(p.X, a, b, c, d)` com 4 args no codebase inteiro (grep confirmou). O maior uso hoje é 3 args.
- A implementação de `call` em `a64_codegen.bsm:1240-1264` trata arg count arbitrário (push todos, pop em ordem reversa pros regs x0..x(cnt-2), fn_ptr em x9). Teoricamente suporta 4 args mas **nunca foi exercitado em produção**.

Hipótese 1 reformulada: **o path de 4-arg fn_ptr indirect call é mecanicamente correto no fonte mas nunca foi auto-compilado em self-host**. Se tiver bug de codegen latente aí, `emit_syscall(name_p, argc, extra_slot, extra_val)` é o primeiro caller a acender a luz.

### Achado 3 — `cg_syscall_std` não existe no tree

O handoff menciona a spine chamando `cg_syscall_std(p, n, BSYS_WRITE, 3, -1, 0, 1)` como helper. Grep em `src/` não acha essa função. Implicação: foi removida no revert. Se o bug do gen2 mute-crash estava dentro dela (por exemplo, arg-order swap entre BSYS_NUMBER e `argc`), ele se foi com o código. Debugar pela hipótese 1 sem reimplementar cg_syscall_std na mesma forma antiga vai ser chute no escuro.

### Recomendação concreta pro próximo agente

**Vai direto pro plano B (enum portável).** Razões:

1. **Custo**: 21 syscalls × 2 chips = 42 linhas de mapping trivial. Dá pra splitar em 3-4 commits pequenos.
2. **Risco baixo**: int passa por fn_ptr sem questionamento (evidência: `emit_load_float_ret(slot)`, `emit_mem_read(width)`, `emit_mem_write(val_reg, width)` todos usam int args).
3. **Plano A só ganha "uma fonte de verdade"** (o nome) mas isso é cosmético — o mapping explícito em enum torna a intenção MAIS clara, não menos.
4. **O debugging do mute-crash do plano A é caro**: sem o `cg_syscall_std` original, reconstruir pra reproduzir é ~1h, isolar é 1-2h, corrigir é incerto.

### Se plano A for obrigatório por algum motivo arquitetural

Antes de re-implementar syscall dispatch inteiro, criar um **teste de canário** pro 4-arg fn_ptr call, isolado de syscall:

```
// Primitiva throwaway em ambos os chips
void _a64_test_4arg(a, b, c, d) {
    // imprime os 4 args em stderr
    // (implementar com sys_write no mode bin)
}
// Spine:
call(p.test_4arg, packed_value, 0x1111, 0x2222, 0x3333);
```

Se os 4 args chegam íntegros → mecânica está OK, bug estava no `cg_syscall_std` deletado. Se não → bug de codegen real em 4-arg indirect call, precisa fix no backend antes de qualquer lift de syscall.

Esse canário custa ~30min, elimina a variável "mecânica de call" de uma vez, e qualquer lift futuro que precise de 4+ args (call, fn_ptr complexas, vec_*) se beneficia da clareza.

### Ponto de partida para a próxima sessão

- **Commit de base**: HEAD atual (working tree uncommitted do handoff, bpp `9c64099a`).
- **Primeira ação**: executar canário de 4-arg call (Achado 3). Se passa, plano B com confiança. Se falha, o bug backend veio à luz.
- **Segunda ação**: plano B, começar por `sys_exit` (0 extra args, retornos simples) antes dos `sys_write`/`sys_read` que têm `extra_slot`.
- **Byte-identity**: esperar gen1 ≠ ./bpp (mudança de source, normal) mas gen1 == gen2 (self-reproduce deterministic). Se gen2 diverge, reverter.

### Achados colaterais (úteis mas não bloqueantes)

- `emit_load_runtime_global(sym_id)` recebe int — sem surpresa, funcionou porque é path já testado.
- `emit_index_word()` sem args — mesmo motivo.
- Os primitives stubbed são portão seguro. Podem ficar como estão indefinidamente se plano B nunca entrar.

Passo a bola afiada. Sorte no próximo commit.

---

## Addendum 2 — canário executado (sessão Claude Code 22/04 fim de tarde)

Peguei a bola, consultei o Claude Code anterior em modo consultor, e rodei o canário de 4-arg antes de qualquer lift real. Resultado: **mecânica está OK**.

### Setup do canário

- Slot dedicado `emit_canary(a, b, c, d)` adicionado em `ChipPrimitives`.
- Primitives `_a64_emit_canary` e `_x64_emit_canary` — corpo idêntico: 8 chars literais ("canary: ") + 4 `print_err_hex` pros 4 args, separados por espaço.
- Bind em ambos `cg_install_<chip>_primitives`.
- Spine: handler `_canary_4arg` em `cg_builtin_dispatch` que dispara `call(p.emit_canary, 0xAA, 0xBB, 0xCC, 0xDD)` e retorna `1` (handled).
- Validator: entrada `_canary_4arg` em `val_is_builtin` pra não ser tratado como extern missing.

### Execução

```
./bpp src/bpp.bpp -o /tmp/bpp_gen1          # gen1: hash d2662017 com canário
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2  # gen2: hash d2662017 idêntico
/tmp/bpp_gen1 tests/test_canary_4arg.bpp -o /tmp/canary_out 2>/tmp/canary.err
# test: main() { _canary_4arg(); return 0; }
cat /tmp/canary.err
# → canary: 0x00000000000000aa 0x00000000000000bb 0x00000000000000cc 0x00000000000000dd
```

Os 4 markers chegam intactos e na ordem correta através do `call(p.emit_canary, a, b, c, d)` passando por struct-slot indirection. aarch64 validado. x86_64 bindado mas não testado em Docker nessa sessão — o Plano B precisa validar lá quando chegar no primeiro sys_* lift.

### Conclusão

**Hipótese 1 original está morta.** Packed refs e ints sobrevivem o mecanismo `call(fn_ptr, a, b, c, d)` com 4 argumentos através de um slot de `ChipPrimitives`. Não existe bug latente no codegen de 4-arg indirect call no aarch64 que precise fix antes do Plano B.

Implicação direta: quando o Plano A original crashou (sessão anterior), a causa **não era** o mecanismo de 4-arg call. Suspeita mais forte agora é:

1. **A colisão `BSYS_WRITE=4 (macOS) vs =1 (Linux)` do import duplo no spine** — se `cg_syscall_std` antigo referenciava `BSYS_WRITE` depois do segundo import ter sobrescrito o valor, `sys_write` emitia `mov x16, #1`, que no macOS é `sys_exit`, explicando perfeitamente o "mute crash exit 0" observado.
2. **`cg_syscall_std` mal escrito** — arg-order swap entre `BSYS_NUMBER` e `argc` cairia no mesmo sintoma.

Ambas são resolvidas pelo Plano B (enum portável CG_SYS_*), que evita o import duplo no spine e dá mapping explícito chip-local.

### Rollback executado

Canário era diagnóstico one-shot, sem valor de regressão. Removido inteiro pós-validação:

- Slot `emit_canary` — removido de `ChipPrimitives`.
- `_a64_emit_canary`, `_x64_emit_canary` — removidos de ambos `*_primitives.bsm`.
- Bindings — removidos de ambos `*_codegen.bsm` installers.
- Handler `_canary_4arg` — removido de `cg_builtin_dispatch`.
- Entrada `_canary_4arg` — removida de `val_is_builtin`.
- `tests/test_canary_4arg.bpp` — deletado.

Bootstrap pós-rollback: `bpp == gen1 == gen2 == 9c64099a` (byte-idêntico ao estado pré-canário do handoff original).

### Suite pós-rollback

109 passed / 3 failed / 11 skipped. Os 3 fails são `test_gpu_{circle,clear,shapes}` — **confirmado como flake ambiental macOS**: cada um passa com exit 0 rodado individualmente (runners isolados, timeout 8s cada). A suite completa abre 30+ janelas GPU em rápida sucessão e o window-manager do macOS eventualmente perde focus entre testes. Não é regressão de código.

### Recomendação pro próximo agente (ainda mais afiada)

**Plano B, começando por `sys_exit`** — o mais simples: 1 arg, sem extra_slot, sem normalize_err. Mapping:

```
// bpp_codegen.bsm — adicionar const CG_SYS_EXIT = 0 (ou enum se existir)
// cg_builtin_dispatch:
if (cg_str_eq(n.a, "sys_exit", 8)) {
    call(p.emit_node, *(arr + 0));  // arg 0: exit code em x0
    call(p.emit_syscall, CG_SYS_EXIT, 1, -1, 0);  // id, argc, extra_slot=-1 (none), extra_val ignored
    return 1;
}

// a64_primitives.bsm — _a64_emit_syscall agora faz:
void _a64_emit_syscall(sys_id, argc, extra_slot, extra_val) {
    auto num;
    if (sys_id == CG_SYS_EXIT)  { num = BSYS_EXIT; }
    if (sys_id == CG_SYS_WRITE) { num = BSYS_WRITE; }
    // ... etc
    enc_mov_wide(16, num);
    enc_svc(0x80);
}
```

**Ordem sugerida dos lifts**: `sys_exit` → `sys_close` → `sys_read` (2 args, ainda sem extra) → `sys_write` (3 args) → então o resto. Deixar `sys_mmap`/`sys_munmap`/`sys_ioctl` por último (mais args, mais superfície).

**Checklist a cada lift**:
1. `gen1 == gen2` (determinismo — byte-identity).
2. `sh tests/run_all.sh` deve ficar 109/3 ou 112/0 (os 3 gpu fails são ruído conhecido — reroda e se os MESMOS 3 aparecem, não é regressão).
3. Linux side: Docker test antes de commitar qualquer mudança em `_x64_*` ou em `_bsys_linux.bsm`.

Bola rolando. A mecânica não é mais suspeita.
