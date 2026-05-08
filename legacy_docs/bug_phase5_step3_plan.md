# Bug — Phase 5 Step 3: Engine unificada GUI+TUI

## Context

A faxinada (2026-04-30) deixou `the_bug` com a casa arrumada:
- `.bug` sempre-ligado, build_id 16 bytes (Mach-O LC_UUID + ELF PT_NOTE)
- `main()` refatorado em três entries (`_run_gui`, `_run_dump`, `_run_tui`)
- Reader checa identidade na carga, falha alto em mismatch

Mas a GUI e a TUI ainda têm pipelines paralelos diferentes:

```
TUI (./bug --tui ./prog):
   ./bug → fork → debugserver → SIGTRAP handler → viz_render_*
                                       → out_str (ANSI no terminal)

GUI (./bug ou ./bug foo.bug):
   ./bug → stbwindow → painel mostra .bug estático
                  → botão "Run target" → fork+execve solto
                  → (sem observação, sem viz, sem watch)
```

Phase 5 step 3 unifica: o botão Run da GUI passa a usar o MESMO pipeline
`bug_run` que o `--tui`, com watch e viz dispatch pintando em painéis
via `stbdraw` em vez de ANSI no terminal. Esta é a equiparação com RAD
Debugger — visualização live durante execução do programa.

## Goal

Um único `bug_run` que serve TUI e GUI:

- TUI: pipe de saída ANSI continua (zero regressão; testes shell
  ainda passam)
- GUI: cada SIGTRAP popula uma estrutura compartilhada (watch values,
  viz buffer snapshots) que o frame loop da GUI lê e pinta em painéis
- Mesma dispatch table de visualizers — `viz_render_<kind>` ganha um
  par `viz_render_<kind>_panel(rect, addr, ...)` que usa `stbdraw` em
  vez de `out_*`

## Architecture

### Observer becomes non-blocking

Hoje `bug_run` faz fork + debugserver + loop blocking de SIGTRAP. Pra
GUI funcionar, o frame loop não pode bloquear na espera do SIGTRAP do
tracee. Duas opções:

**Opção A — polling no tick:**
- `bug_run_async_start(target_path, ...)` faz fork + setup, retorna
  imediatamente
- `bug_run_poll()` é chamado a cada frame; checa se há evento novo
  (BP hit, exit, output) sem bloquear
- Quando há evento: popula shared state, retorna 1; senão retorna 0

**Opção B — observer thread:**
- Background thread roda o loop blocking original
- Main thread (GUI) só lê shared state quando renderiza
- Threading no B++ ainda é Phase 1 macOS-only — frágil, evita

**Recomendação: A (polling).** Mais simples, sem dependência de
threading, GUI mantém 60fps. Polling no GDB protocol é leve (read
não-bloqueante no socket TCP). TUI continua usando o caminho
blocking original.

### Shared state structure

Novo módulo `src/bug_shared.bsm` (agnostic) define:

```
// Latest watch snapshot, populated on every SIGTRAP / BP hit.
global bug_shared_watch_count;          // arr_len(values)
global bug_shared_watch_names;          // arr<packed>
global bug_shared_watch_values;         // arr<u64>
global bug_shared_watch_types;          // arr<u8>

// Latest viz buffer snapshots. Each entry has a kind + reader-copied
// memory bytes (graph: N doubles; vec2: 2 doubles; rgba: w*h*4 bytes).
global bug_shared_viz_count;
global bug_shared_viz_kind;             // arr<u8> — VIZ_GRAPH / VIZ_VEC2 / VIZ_RGBA
global bug_shared_viz_addr;             // arr<u64> — original tracee address (for label)
global bug_shared_viz_w, bug_shared_viz_h;  // for rgba
global bug_shared_viz_data;             // arr<ptr> — heap-allocated copy of bytes
global bug_shared_viz_data_len;         // arr<int>

// Tracee status.
global bug_shared_status;               // 0=running, 1=stopped (BP), 2=exited, 3=crashed
global bug_shared_pc;                   // PC at last stop
global bug_shared_func_name;            // packed name of last function
```

### Panel renderers

Cada `viz_render_<kind>` em `bug_runviz.bsm` ganha um par:

```
viz_render_graph(addr, n)               // existing — TUI ANSI
viz_render_graph_panel(px, py, pw, ph, addr, n)   // new — stbdraw

viz_render_vec2(addr)
viz_render_vec2_panel(px, py, pw, ph, addr)

viz_render_rgba(addr, w, h)
viz_render_rgba_panel(px, py, pw, ph, addr, w, h)
```

A leitura de memória (`gdb_read_mem`) acontece UMA vez por SIGTRAP no
observer. O resultado fica em `bug_shared_viz_data`. As funções
`_panel` leem desse buffer compartilhado, não tocam o tracee.

### the_bug_lib integration

`tools/the_bug/the_bug_lib.bsm` ganha:

- `_tb_observe_active` — 1 quando há tracee rodando sob bug
- Botão "Run target" → chama `_run_tui_in_gui_mode()` que invoca
  `bug_run_async_start`
- Frame loop adiciona `bug_run_poll()` antes do render
- Layout do painel ganha 3 áreas:
  - Topo: trace de funções (existente, expandido com call tree)
  - Centro: watch values (lista live)
  - Inferior: viz canvases (chamadas das `viz_render_*_panel`)

### Renderização rgba via stbsprite

Pro rgba especificamente:
- Cria uma textura `stbsprite_new(w, h)` UMA vez por viz registrado
- A cada SIGTRAP: copia bytes do tracee pro buffer da textura via
  `stbsprite_upload(spr, data)`
- `viz_render_rgba_panel` desenha a textura escalada pra o rect do
  painel via `sprite_draw_scaled(spr, x, y, w, h)`

Performance: upload de 320×180×4 = 230 KB por frame. Em 60fps = 13.8
MB/s. Trivial pro PCIe.

## Implementation order

### Stage 1 — Async observer (substancial)

**Files:**
- `src/backend/os/macos/bug_observe_macos.bsm` — adicionar
  `bug_run_async_start`, `bug_run_poll`, `bug_run_async_stop`
- `src/bug_shared.bsm` (novo) — globals e accessors

**Risco:** maior da fase. GDB protocol parsing precisa ser
não-blocking. Setup de timeout em `sys_read` de socket. Reuse de
`bug_gdb.bsm:gdb_send_recv` precisa virar `gdb_send` + `gdb_try_recv`
separados.

**Verification:**
- TUI ainda passa `tests/test_bug_tui.sh` (zero regressão no path
  legado)
- `bug_run_async_start` retorna em <100ms
- `bug_run_poll` retorna em <5ms quando tracee está rodando
- Eventos (BP hit, exit) aparecem em até 1 frame (16ms @ 60fps)

### Stage 2 — Panel renderers (moderado)

**Files:**
- `src/bug_runviz.bsm` — adicionar `viz_render_<kind>_panel`
  paralelos aos existentes

**Risco:** baixo. Reuso de `stbdraw` (`draw_rect`, `draw_text`,
`draw_line`) e `stbsprite` para rgba. Cada renderer ~30-50 LOC.

**Verification:**
- Smoke test: programa que escreve num framebuffer fixo + `@viz
  buffer rgba 32 24` no source. Painel deve mostrar o conteúdo
  esperado.

### Stage 3 — Wire-up GUI (pequeno)

**Files:**
- `tools/the_bug/the_bug_lib.bsm` — botão Run usa async observer,
  frame loop chama poll + renderiza viz panels

**Risco:** baixo se Stages 1-2 limpos. Layout de painéis é o que
toma tempo (UX, não funcionalidade).

**Verification:**
- `./bug pathfind.bug` → click Run → trace + watch + viz aparecem
  live no painel
- Comparação visual com a referência RAD Debugger
  (https://www.youtube.com/watch?v=_9_bK_WjuYY&t=1260s)

## Critical files

| Arquivo | Mudança | Tamanho |
|---|---|---|
| `src/backend/os/macos/bug_observe_macos.bsm` | Async observer (start/poll/stop) | ~150 LOC novas |
| `src/bug_shared.bsm` | Globals + accessors pra shared state | ~80 LOC novas |
| `src/bug_runviz.bsm` | `_panel` variantes dos renderers | ~120 LOC novas |
| `src/bug_gdb.bsm` | Quebrar `gdb_send_recv` em send + try_recv | ~30 LOC mudança |
| `tools/the_bug/the_bug_lib.bsm` | Wire async observer, layout painéis | ~200 LOC mudança |

Total estimado: ~580 LOC novas + 230 LOC modificadas. **2-3 sessões**.

## Existing infra reused

- `bug_run` em `bug_observe_macos.bsm` — modelo do path blocking que
  mantemos pra TUI
- `gdb_read_mem`, `gdb_send_recv` em `bug_gdb.bsm` — primitivas GDB
  protocol
- `viz_render_graph/vec2/rgba` em `bug_runviz.bsm` — dispatch table
  e leitura de memória já existem
- `_tui_handle_v` — parser de comandos `v <expr> <kind>`, mesmo
  caminho usado pra `@viz` source hints
- `stbdraw` API: `draw_rect`, `draw_text`, `draw_line`, `rgba()`
- `stbsprite` API: `stbsprite_new`, `stbsprite_upload`,
  `sprite_draw_scaled`
- `_debug_needs_reload` em `runner.bsm` + `panels.bsm` — Bang 9 já
  recarrega `.bug` após build, vai funcionar pro fluxo "compila →
  debug" sem mudança

## Decisões pré-acordadas

1. **Polling, não threading.** B++ thread support é Phase 1
   macOS-only; threading complica sem ganho real. Polling no tick é
   suficiente pra GUI a 60fps.
2. **TUI mantém path blocking.** Zero regressão no `--tui`. Testes
   shell continuam passando. Async é só pra GUI.
3. **rgba via stbsprite.** Upload por frame é trivial. Não inventar
   GPU streaming engine.
4. **Shared state em globals, não actor model.** B++ é pequena, não
   precisa de message queue. Globais com convenção "observer
   escreve, GUI lê".

## Risk principal

Stage 1 (async observer) é a parte que pode dar trabalho. GDB
protocol assume blocking reads em vários pontos. Quebrar isso requer:

1. Socket em modo não-blocking (`fcntl(F_SETFL, O_NONBLOCK)` —
   verificar se o builtin existe ou se precisa ser adicionado)
2. Buffer de leitura parcial (recv pode retornar metade de um
   pacote)
3. State machine pra parsing incremental do protocol

**Mitigação:** começar com timeout-based polling em vez de
non-blocking puro — `select(socket, timeout=0)` antes de cada read,
aceitar que leitura pode bloquear até 1ms. Mais simples que
non-blocking puro, suficiente pra 60fps.

## Out of scope

- **Phase 6 (profiler + Caminho C)** — meta de longo prazo. Não
  toca esta fase.
- **Bang 9 panel rendering avançado** — viz dentro do Bang 9 (não
  só ./bug standalone) só depois que essa fase ship-ar e ./bug
  servir como reference UX.
- **`name:line` breakpoints** — issue separado (`docs/todo.md`
  bug #3b), bloqueado por off-by-N no source map, não relacionado
  a esta fase.
- **Linux observer GUI** — `bug_observe_linux.bsm` continua só TUI
  por enquanto. Phase 5 step 3 é macOS-only no primeiro corte;
  Linux GUI vem quando o platform layer ELF + GUI estabilizar.

## Activation

Este ticket é o próximo natural depois da faxinada. Pode ser
aberto na próxima sessão sem fricção — todos os arquivos estão
estáveis, suite verde 121/0/11, gen-stable.
