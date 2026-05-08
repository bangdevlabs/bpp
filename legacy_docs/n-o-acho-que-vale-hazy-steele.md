# Annotation System Redesign

## Context

O sistema de anotações do B++ misturava dois conceitos distintos sob a mesma sintaxe `: `,
causando confusão visual e conceitual:

- **Phase annotations** em funções: `: base`, `: io`, `: gpu`, `: time`, `: solo`
- **Type hints** em variáveis/parâmetros: `: float`, `: half`, `: byte`, `: bool`

A solução é separar com `@phase` colado à função. `:` continua exclusivo para tipos.

---

## Findings — o que a auditoria revelou

### Phase system: real e funcional, não documentação

| Annotation | Ocorrências | Enforcement |
|---|---|---|
| `: base` | 292 | W013 — dispara se a função tem side effects |
| `: io`   | 109 | W026 — dispara se alcança GPU ou SOLO |
| `: gpu`  |  53 | W026 — dispara se alcança IO ou SOLO |
| `: time` |   5 (2 reais) | W026 — garante que audio callbacks não chamam malloc/IO |
| `: solo` |   0 usos reais | 0 enforcement — existe como override, nunca usado |

W013 e W026 fazem enforcement real via call-graph analysis em `bpp_dispatch.bsm:1154-1268`.
O sistema é útil — só a sintaxe era ruim.

### Slice hints em variáveis: só float funciona na prática

`:float` é o único hint essencial em variáveis (muda banco de registradores).
Integer slices (`:byte`, `:quarter`, `:half`) em variáveis: zero usos em produção.
Struct fields continuam usando slices normalmente (23 ocorrências reais).

### Por que `@` e não `[phase]`

`[phase]` colide com possível sintaxe futura de generics `func[T](x: T)`.
`@` não é usado em lugar nenhum do B++ hoje (verificar com grep antes de implementar).
Lê como "anotado com" — familiar de Python/Java/Rust.
Parser ainda mais simples: apenas `if (val_is_ch('@'))`.

```bpp
// Antes (horrível — : colide visualmente com type hints)
arr_len(arr) : base { return *(arr - 8); }
audio_open(sr: float) : io { ... }
_aud_stream_cb(user, q, buf) : time { ... }

// Depois (@ colado, visualmente distinto)
arr_len(arr)@base { return *(arr - 8); }
audio_open(sr: float)@io { ... }
_aud_stream_cb(user, q, buf)@time { ... }
```

---

## Mudanças planejadas

### Mudança 1 — `@phase` syntax

**Parser** (`src/bpp_parser.bsm:1243-1288`):
- Substituir `if (val_is_ch(':'))` + peek-ahead por `if (val_is_ch('@'))`
- Consumir `@`, ler o phase name, nada mais
- Remove toda a lógica de peek-ahead (~20 linhas → ~10 linhas)
- `@` nunca é type hint, sem ambiguidade

**Sequência de migração (2 passos — obrigatório para evitar chicken-and-egg):**

O compilador usa `: phase` em seus próprios fontes. Se o parser parar de aceitar
a sintaxe antiga antes do sweep, o bootstrap quebra.

```
Passo A — aceitar ambos:
  a1. Parser aceita AMBOS `: phase` E `@phase`
  a2. Bootstrap (old syntax ainda funciona, gen1==gen2)

Passo B — migrar e fechar:
  a3. Sweep repo: `: phase` → `@phase` (461 ocorrências, ~40 arquivos)
  a4. Bootstrap (new syntax funciona, gen1==gen2)
  a5. Remover branch `: phase` do parser
  a6. Bootstrap final (gen1==gen2 — confirma que old branch foi removido)
```

**Repo sweep** — padrões a substituir:
```
") : base {"    → ")@base {"
") : io {"      → ")@io {"
") : gpu {"     → ")@gpu {"
") : time {"    → ")@time {"
") : solo {"    → ")@solo {"
") : base;"     → ")@base;"     (stubs/extrn)
") : io;"       → ")@io;"
```
Arquivos: src/, stb/, bang9/, tools/, games/ (~40 .bsm files).
Excluir: docs/ e tests/ que podem ter exemplos didáticos da sintaxe antiga
(atualizar separadamente após o sweep).

**Tonify checklist** (`docs/tonify_checklist.md` Rule 4):
- Atualizar tabela e exemplos para `@phase`
- Remover `:solo` da tabela de anotações recomendadas (0 usos, é escape hatch)

### Mudança 2 — `:solo` deprecação

`:solo` tem 0 usos reais. Mantém no parser (escape hatch futuro), remove da documentação.

- Parser: adicionar comentário "escape hatch, não use sem motivo forte"
- Tonify checklist Rule 4: remover da tabela recomendada

### Não entra nesta sessão

**`:bool`** — adiado. `:bit` já tem 0 usos em variáveis; `:bool` provavelmente teria o mesmo
destino. Sem caso de uso concreto, 150 LOC + W027 + 2 chips não têm ROI hoje.

### Não muda

- Os 5 tipos de phase (base/io/gpu/time/solo) — todos têm propósito
- W013/W026 enforcement — funcionando
- Type hint system (`:float`, `:half float`) — funcionando
- Slice em struct fields — 23 usos reais, funcionando

---

## Arquivos críticos

| Arquivo | Mudança |
|---|---|
| `src/bpp_parser.bsm:1243-1288` | Fase A: aceitar ambos. Fase B: só `@phase` |
| `src/bpp_lexer.bsm` | Verificar se `@` já tem tratamento especial |
| `docs/tonify_checklist.md` Rule 4 | Atualizar sintaxe, remover `:solo` |
| ~40 .bsm files | Sweep `: phase` → `@phase` |

Paths reais dos codegens (verificar antes de qualquer edição futura):
- `src/backend/chip/aarch64/a64_codegen.bsm`
- `src/backend/chip/x86_64/x64_codegen.bsm`

---

## Nota futura

`@phase` não colide com `a[i]` (indexação) nem com possível `func[T]` (generics).
Se B++ um dia adicionar generics com `[T]`, a sintaxe permanece limpa.
Se `@` for reutilizado para outra coisa (macros, annotations de outra natureza),
o phase pode migrar para um símbolo diferente — mas `@` é o melhor candidato hoje.

---

## Verificação

```bash
# Pré-condição: @ não está em uso no lexer/parser hoje
grep -n "@" src/bpp_lexer.bsm src/bpp_parser.bsm

# Após passo a2 (parser aceita ambos):
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
cmp /tmp/bpp_gen1 /tmp/bpp_gen2   # gen1==gen2

# Após passo a3 (sweep completo):
grep -rn ") : base\|) : io\|) : gpu\|) : time\|) : solo" src/ stb/ bang9/ games/
# deve retornar zero matches

# Após passo a4 (bootstrap com nova sintaxe):
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
cmp /tmp/bpp_gen1 /tmp/bpp_gen2   # gen1==gen2

# Após passo a6 (bootstrap final sem branch legado):
bash tests/run_all.sh   # suite completa, esperar 120/0/11
```
