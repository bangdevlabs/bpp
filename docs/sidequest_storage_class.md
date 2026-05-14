# Sidequest — global const + static const storage classes

**Status:** PLAN (escrito 2026-05-14, post-WC1-Session-3 Bloco 3.1
smoke trap).
**Triggered by:** smoke test failure during Session 3 Bloco 3.1
where `const SCREEN_W = 1;` did not satisfy `extrn SCREEN_W;`
declared in `stb/stbrender.bsm`. Surfaced fundamental gap in B++
storage class composition.
**Predecessor docs:** `games/rts1/wc1_handoff.md` Session 3 closure
notes, conversation thread 2026-05-13 evening.

## Sobre a observação do user

> "a gente conseguiu se livrar de ficar declarando variavel o tempo
> todo em b++ mas fluxo de analise e compilação é muito importante
> pra quem programa jogos eu sinto, não tem muito como fugir teria
> que ter um smart dispatch no compilador muito bom pra eliminar
> isso"

Observação correta e load-bearing. B++ eliminou declarações
redundantes em 3 áreas via smart dispatch:

1. `auto x;` defaulta pra TY_WORD; promote auto pra `: half`/`: float`
   via uso interno
2. `put_err(x)` smart-dispatch pra putnum/putstr/putfloat baseado em
   `arg.itype`
3. Func annotations opt-in (V3 — flow analysis pega mismatches sem
   annotation explicit)

**Storage class é a 4ª área onde smart dispatch ATÉ AGORA não chegou,
e essa sidequest argumenta que NÃO É o caso pra adicionar** — porque
storage class afeta link graph (cross-module), e Zig-style lazy
materialization quebra B++ separate compilation. Em vez disso,
adicionamos composicional explicit qualifiers (`global const`,
`static const`) que ALINHAM com o resto do design (auto/global/extrn
qualificados por visibility, agora qualificáveis por mutability via
const).

Esse é o trade-off honesto. B++ é "smart by default + escape hatch
explícito quando structural". Storage class é structural. Mantém-se
explicit.

## Context

Hoje B++ tem cinco caminhos de storage class:

```bpp
const X = 42;       // parser-level literal substitution, NO data slot
auto X;             // mutable slot, file-scope visibility (~ C tentative def)
global X;           // mutable slot, worker-shared (B++-only concept, Rule 30)
extrn X;            // reference to slot defined elsewhere
static <auto|global|extrn> X;   // module-private variants
```

Faltam 2 combinações naturais:

```bpp
global const X = 42;    // IMMUTABLE slot, cross-module shared
static const X = 42;    // IMMUTABLE slot, module-private
```

Hoje:
- `static const` é HARD-REJECTED em `src/bpp_parser.bsm:1926-1932` (E230)
- `global const` é parseado como `global X = 42` que não compõe (global
  espera só nomes)

**Motivação concreta:** o smoke do Session 3 quebrou exatamente nesse
gap. Programmer escreveu `const SCREEN_W = 1;` esperando satisfazer
`extrn SCREEN_W;`. Inconsistência real porque B++ é hybrid:
- `extrn`/`auto`/`static` seguem modelo C
- `const` segue modelo Rust/Go (literal substitution)
- Quem vem de C/C++ tem 75% intuição correta + 25% trip silencioso

Esse sidequest adiciona as 2 composições que faltam + diagnostic
upgrade que catch a confusão.

## Design lock-in (não revisitar durante execução)

### Semântica das duas novas combinações

| Sintaxe | Visibility | Mutability | Cria slot? | Aceita `extrn` em outro módulo? |
|---------|------------|------------|------------|----------------------------------|
| `const X = 42;` (hoje, inalterado) | parser-only | inline literal | ✗ | ✗ (não é símbolo) |
| `global const X = 42;` | cross-module | imutável após init | ✓ (.data ou .rodata) | ✓ |
| `static const X = 42;` | module-private | imutável após init | ✓ (.data ou .rodata, module-scope) | ✗ (static = scoped) |

### Mutability enforcement

**Decisão:** read-only após emit. Compilador rejeita writes pós-
declaração com `error[EXXX]: cannot assign to const slot 'X'`.

Diferença vs `extrn` (Rule 1 write-once convention):
- `extrn X` + write é convention (programmer disciplina). Compiler
  não impede write em runtime.
- `global const X = 42` é compiler-enforced. Slot é read-only no link,
  write é EXXX.

Implementação: durante o validate pass, flag globals com
`GLOB_GLOBAL_CONST` ou `GLOB_STATIC_CONST` como read-only. Qualquer
T_ASSIGN que tenha LHS apontando pra esse global → emit error.

### Expression on the RHS

Slot const aceita:
- Literais (int, float, string) ← caso primário
- Outros consts compile-time (`global const Y = X + 1;` onde X já é const)
- `const_eval_node` (engine existente) decide se a expressão é
  compile-time evaluable

Rejeita:
- Function calls (`global const Z = compute_default();` → EXXX:
  needs compile-time expression)
- References a auto/global/extrn variables
- Dynamic strings (`global const PATH = path_join(...)` → EXXX)

**Justificativa:** slot const tem que ter valor conhecido em compile
time pra ir pra .rodata. Mesma constraint que C++ `constexpr` no
global scope.

### Cross-module conflict

Se dois módulos declaram `global const X = 42;` com mesmo nome:
- Mesmo valor: ainda assim ERROR (duplicate definition, like C
  `int X = 42` em 2 .c files)
- Valores diferentes: ERROR (obviamente)

Não tem detecção de "valores iguais → OK silently". Programmer escolhe
UM módulo pra ser o canonical home + outros usam `extrn X;`.

`static const` não conflita cross-module porque é module-private.

### Type inference

`global const X = 42;` infere `X: word`.
`global const PI = 3.14;` infere `PI: float` (via SHAPE_DECIMAL fast
path em parse_const).
`global const NAME = "build";` infere `NAME: ptr` (string literal
points into rodata).

Hint explícito permitido:
```bpp
global const X: half = 16;     // 16-bit slot em vez de 64-bit
global const FREQ: float = 440.0;
```

## Implementation plan

Quatro blocos. Cada bloco verificável isoladamente. Bootstrap byte-
stable como gate em cada um.

### Bloco 1 — Storage class enum + parser path (~80 LOC)

**Files:**
- `src/bpp_parser.bsm` — adicionar `GLOB_GLOBAL_CONST` +
  `GLOB_STATIC_CONST` constants near `GLOB_AUTO/GLOB_GLOBAL/GLOB_EXTRN`
  (linhas ~895)
- `src/bpp_parser.bsm:1898-1932` — adicionar branches `global const` +
  `static const`
- `src/bpp_parser.bsm:1180` `parse_global()` — aceitar novo parameter
  `is_const` (boolean), parsear `= expr` quando true

**Mudanças concretas:**

Top-level dispatch (`bpp_parser.bsm:1894-1908`):
```bpp
} else if (val_is("global", 6)) {
    consume();
    if (peek_type() == TK_KW && val_is("const", 5)) {
        consume();
        parse_global_const(GLOB_GLOBAL_CONST);
    } else {
        parse_global(GLOB_GLOBAL);
    }
} else if (val_is("extrn", 5)) {
    parse_global(GLOB_EXTRN);
}
```

Static branch (`bpp_parser.bsm:1914-1934`):
```bpp
} else if (val_is("static", 6)) {
    consume();
    if (peek_type() == TK_KW) {
        if (val_is("auto", 4)) { parse_global(GLOB_AUTO); }
        else if (val_is("global", 6)) {
            consume();
            if (peek_type() == TK_KW && val_is("const", 5)) {
                consume();
                parse_global_const(GLOB_STATIC_CONST);
            } else {
                parse_global(GLOB_GLOBAL);
            }
        }
        else if (val_is("extrn", 5)) { parse_global(GLOB_EXTRN); }
        else if (val_is("const", 5)) {
            consume();
            parse_global_const(GLOB_STATIC_CONST);
            // Replaces the E230 rejection that was here.
        }
        else if (val_is("void", 4)) { ... }
    }
}
```

New `parse_global_const(gclass)` function (~40 LOC):
```bpp
static void parse_global_const(gclass) {
    auto var_p, expr_node, val;
    var_p = cur_packed();
    consume();   // consume name
    try_type_annotation(var_p);
    consume();   // consume '='
    expr_node = parse_expr();
    val = const_eval_node(expr_node);   // already exists, reused
    add_global(var_p, gclass);
    glob_init_val = arr_push(glob_init_val, val);
    glob_init_class = arr_push(glob_init_class, gclass);
    if (val_is_ch(';')) { consume(); }
}
```

Mutability check em validate pass:
```bpp
if (lhs.ntype == T_VAR) {
    auto gclass;
    gclass = glob_class_of(lhs.a);
    if (gclass == GLOB_GLOBAL_CONST || gclass == GLOB_STATIC_CONST) {
        diag_fatal(EXXX);
        diag_str("cannot assign to const slot '");
        diag_named(lhs.a);
        diag_str("'");
        diag_newline();
        diag_loc(lhs.src_tok);
        sys_exit(1);
    }
}
```

**Verify gate:**
- Bootstrap byte-stable (mexe em parser mas não em existing tokens flow)
- `tests/run_all.sh` mantém 163/0/12
- `tests/run_all_c.sh` mantém 130/0/45

### Bloco 2 — Codegen emit_global_data extension (~60 LOC)

**Files:**
- `src/backend/chip/aarch64/a64_primitives.bsm` —
  `_a64_emit_global_data()` currently no-op
- `src/backend/chip/x64/x64_primitives.bsm` — similar
- `src/backend/c/bpp_emitter.bsm` — C emitter side

**Mudanças concretas:**

Walk `globals[]`, for each `GLOB_GLOBAL_CONST` or `GLOB_STATIC_CONST`:
- Find matching entry in `glob_init_val` (parallel array)
- Emit ASM data directive with the value
- Label: global symbol (`GLOB_GLOBAL_CONST`) or local
  (`GLOB_STATIC_CONST`)

Pseudo-assembly per backend:
```asm
# aarch64:
.section __DATA,__data           # ou __DATA,__const para true read-only
.globl _X                        # se GLOBAL_CONST
_X:
    .quad 42                     # word value
```

Type-aware emit:
- `TY_WORD` → `.quad`
- `TY_WORD_H` → `.long`
- `TY_FLOAT` → `.double` (8 bytes)
- `TY_FLOAT_H` → `.float` (4 bytes)
- `TY_PTR` (string) → label pointing to string literal in `__cstring`
  section

C emitter:
```c
const int64_t X = 42;            /* GLOB_GLOBAL_CONST */
static const int64_t Y = 100;    /* GLOB_STATIC_CONST */
```

**Verify gate:**
- Bootstrap byte-stable (changes emit but only for new storage class,
  existing code untouched)
- Suite green
- Write smoke test: `tests/test_global_const.bpp` that declares + reads
- C-emit smoke: same test under `--c` flag works

### Bloco 3 — Diagnostic E0XX upgrade (~30 LOC)

**Files:**
- `src/bpp_validate.bsm` or wherever extrn resolution happens
- `docs/warning_error_log.md` — register new error code

**Mudanças concretas:**

When extrn fails to resolve, emit:
```
error[E0XX]: extrn 'SCREEN_W' has no backing definition in the link graph.
  required by: stb/stbrender.bsm:48
  To define a slot for it, ONE of:
  - `global const SCREEN_W = N;`  (cross-module immutable, .rodata slot)
  - `auto SCREEN_W;` + write-once init (Rule 1 discipline, mutable slot)
  - Import a module that defines it (stbgame.bsm or stbwindow.bsm both do)
  Note: `const SCREEN_W = N;` is compile-time literal substitution.
  It does NOT create a slot — see Tonify Rule 1 (storage classes).
```

Replace cryptic `internal error: global 'X' not found in data section`
with this.

### Bloco 4 — Doc updates (~doc only)

**Files:**

`docs/tonify_checklist.md` Rule 1 — extend storage class table:

Add rows:
```
| Constant, cross-module shared, read-only | `global const X = 42;` | `global const SCREEN_W = 320;` |
| Constant, module-private, read-only      | `static const X = 42;` | `static const SINE_TABLE_SIZE = 256;` |
```

Add paragraph "When to use which":
- `const X = 42;` (no qualifier) — inline literal substitution. Zero
  runtime cost. Most cases.
- `global const X = 42;` — needs slot AND cross-module accessible
  (extrn target). E.g. shared build config.
- `static const X = 42;` — needs slot but only inside this module.
  E.g. lookup table size.

`docs/how_to_dev_b++.md` Cap 4 (Types) — add §4.6 — Constants and
storage.

`docs/how_to_dev_b++.md` Cap 16 (Idioms) — add "Use `const` for
inline, `global const` for shared, `static const` for private-with-
slot".

NEW section em `docs/asset_formats.md` Cross-references — pointer pra
Rule 1 updated table.

`docs/warning_error_log.md` — adicionar E0XX (extrn no def) + EXXX
(assign to const slot).

### Bloco 5 — Migration evaluation (~tracking only, NO migration)

File: `docs/todo.md` — adicionar entry "Migrate existing extrn-with-
write-once pattern → global const where applicable"

**NÃO MIGRAR durante esse sidequest.** Listar candidatos pra futura
sweep:
- `stbmixer.bsm:35-40` — `static extrn _MX_MAX_VOICES;` poderia ser
  `global const _MX_MAX_VOICES = 10;`
- `stbgame.bsm` — `SCREEN_W/SCREEN_H` (mas estes mudam via
  `game_init`, então NÃO são const; ficam auto)
- `stbecs.bsm` — `_ECS_CHUNK_PAYLOAD = 16384`, `_ECS_SYS_MAX = 32` —
  já são const (parser-level OK)

A maioria dos atuais static extrn + write-once em
stbmixer/stbecs/etc são constantes de runtime config (config decisions
made by init code). Esses NÃO migram pra global const — global const
exige valor compile-time.

Os que migram são os build-time constants verdadeiros (raros no
codebase atual). Migration é optional sweep, não obrigatória.

## Test plan

Tests novos a adicionar:

1. `tests/test_global_const.bpp` — smoke positivo:
```bpp
global const ANSWER = 42;
global const PI: float = 3.14;
global const NAME = "build-2026-05-13";
main() {
    if (ANSWER != 42)        { return 1; }
    auto f: float; f = PI;
    if (f < 3.14) { if (f > 3.15) { return 2; } }
    if (str_len(NAME) != 14) { return 3; }
    return 0;
}
```

2. `tests/test_static_const.bpp` — module-private behavior. Cria
   `tests/helpers/static_const_lib.bsm` com `static const HIDDEN = 99;`.
   Main importa lib, tenta `extrn HIDDEN;` → expect link error.

3. `tests/test_global_const_xmod.bpp` — cross-module. Lib:
   `global const SHARED = 7;`. Main: `extrn SHARED;` +
   `main() { return SHARED - 7; }`. Expected: returns 0.

4. `tests/test_assign_const_fails.bpp` — xfail EXXX:
   `global const X = 1; main() { X = 2; return 0; }`. Expected: compile
   error EXXX.

5. `tests/test_const_inline_still_works.bpp` — regression:
   `const Y = 10; main() { return Y; }` — works como antes, NÃO emit slot.

6. `tests/test_extrn_no_def_diag.bpp` — xfail E0XX. File com
   `extrn ORPHAN;` and nothing defines it. Expected: clean diagnostic
   with the 3 fix suggestions.

7. `tests/test_global_const_c_emit.bpp` — C-emit smoke (no `// skip-c:`).
   Equivalent of test #1 but under `--c` flag. Verifies C emitter
   produces `const int64_t ANSWER = 42;`.

## Verification gates per bloco

1. Bootstrap byte-stable (`./bpp src/bpp.bpp -o /tmp/g1 && /tmp/g1
   src/bpp.bpp -o /tmp/g2 && cmp /tmp/g1 /tmp/g2`)
2. Native suite: 163/0/12 → 170/0/12 (após os 7 novos tests)
3. C-emit suite: 130/0/45 → 136/0/45 (1 novo C-emit test + 5 que
   rodam via skip-c=false)
4. Game smoke: pathfind / fps_wolf3d / rts compile clean
5. Doc lint (manual sweep): tonify Rule 1 table extended, how_to_dev
   §4.6, warning_error_log E0XX/EXXX rows

## Out of scope (intentional)

Per Rule 28:
- Zig-style lazy slot materialization — quebra separate compilation.
  Adia indefinidamente.
- Whole-program analysis pass — over-engineer pra escala atual.
- Sistema de módulos Jai/Rust-style com namespace — refactor enorme,
  fora de escopo.
- `static auto X = 42;` init-with-value sugar — separate sidequest,
  low priority. Hoje funciona via `static auto X; init() { X = 42; }`.
- Migration sweep dos `static extrn` existentes — optional follow-up,
  NÃO blocker.
- Function call no RHS de const — requires constant function eval,
  big feature. Adia.

## Reuse (não reinventar)

- `add_global` (`bpp_parser.bsm:901`) — registry existente
- `const_eval_node` (`bpp_parser.bsm` + `bpp_internal.bsm`) — compile-
  time eval already works for int + decimal
- `enum_register` / `add_enum_val_float` — pattern pra registrar
  nome-valor parser-level (reference, not direct reuse)
- `emit_global_data` — backend hook já declarado, hoje no-op em a64,
  vamos preencher
- `glob_class` array — extend com 2 valores novos
- `try_type_annotation` — reusable pra `: type` em const declarations

## Estimated effort

| Bloco | LOC | Tempo |
|-------|-----|-------|
| 1 — Parser + storage class | ~80 | 2-3h |
| 2 — Codegen emit (3 backends: a64, x64, C-emit) | ~60 | 2-3h |
| 3 — Diagnostic E0XX upgrade | ~30 | 1h |
| 4 — Doc updates | ~doc | 1-2h |
| 5 — Test plan + execution | ~150 (tests) | 1-2h |
| **Total** | **~320 LOC + tests + docs** | **7-11h** |

## Open questions resolver junto com o agente executor

1. **`static global const` triple combo** — aceitar e tratar como
   static const? Ou rejeitar? Recommendation: rejeitar com diagnostic.
2. **`extrn const X;` declaration syntax** — permitir como "extrn que
   promete ser const slot" (vs `extrn X;` generic)? Útil pra
   documentation. Recommendation: NÃO adicionar agora.
3. **String literal slot lifetime** — `global const NAME = "build";` —
   onde fica o string literal? Atualmente strings vão pra `__cstring`.
   Slot global pointing to it. Cleanup: ninguém free o string literal
   (lives forever, OK). Verify no edge case com `extrn NAME;` em outro
   módulo reading pointer.
4. **Initializer scope** — pode `global const Y = X + 1;` quando X é
   outro global const? Sim, via const_eval_node recursion. Testar.
5. **Annotation cascade** — quando `global const Y = some_const`, o Y
   herda type de some_const? Decisão: infer from RHS (matches const
   current behavior). Annotation override opcional.

## Acknowledgement do design philosophy

O sidequest NÃO adiciona smart dispatch pra storage class. Decisão
consciente:
- Smart dispatch funciona quando decisão é local (call-site type,
  function-level inference)
- Storage class decisão é structural (afeta link graph, cross-module
  symbol table)
- Tornar storage class implicit (Zig-style lazy) quebra separate
  compilation OU exige whole-program pass

Mantém-se a filosofia "smart by default em decisões locais, explicit em
decisões structural" que B++ já segue. Programmer paga 1 keyword
(`global const`) em vez de programmer pagar 5+ keyword combinations +
compiler paying whole-program complexity. Trade-off favorece B++
existing design.

Esse é o limite até onde "smart compiler elimina declaração" vai em
B++. A user's observation está correta: storage class é a parede contra
essa filosofia, e está OK pousar nela.
