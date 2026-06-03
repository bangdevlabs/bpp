# Funnel — WC2 asset lane

Adiciona Warcraft II ao funil de conversão, terceira fonte depois de
WC1 (rts1) e Wolfenstein 3D (fps). Mesmo contrato Tonify Rule 31:
formato estrangeiro entra OFFLINE → JSON canônico Bang 9 sai → o jogo
NUNCA lê o formato estrangeiro em runtime (zero dep não-B++).

Precedente direto: o lane WC1 consumiu a saída-PNG do `war1tool`; o lane
Wolf3D decodificou o container binário VSWAP do zero com a disciplina
"S0 dump-first". O WC2 fica entre os dois e a decisão de qual caminho
seguir é o S0.

## RECON — 2026-06-02 (fonte DOS confirmada + container decodificado)

A fonte que resolveu tudo NÃO foi o CD da Battle.net Edition (1999): o
`INSTALL.EXE` da BNE empacota os dados num CAB InstallShield proprietário
que 7z / cabextract / unshield não abrem sem rodar o instalador (exigiria
Windows/Wine/VM). Em vez disso, a **versão DOS Tides of Darkness (1995)**
— uma pasta JÁ instalada (`WarcrafD/`, roda em DOSBox) — traz os `.WAR`
soltos, exatamente como o `DATA.WAR` do WC1. **Sem instalador, sem
Windows.** Localização na máquina:

    /Users/Codes/Warcraft/Warcraft_II_-_Tides_of_Darkness_1995/WarcrafD/

Arquivos de dados (em `DATA/`):

| Arquivo        | Tamanho | Conteúdo (provável)                  |
|----------------|---------|--------------------------------------|
| `MAINDAT.WAR`  | 9.8 MB  | gráficos, tilesets, units, sprites   |
| `REZDAT.WAR`   | 2.8 MB  | interface / recursos                 |
| `STRDAT.WAR`   | 77 KB   | strings / textos                     |
| `SFXDAT.SUD`   | 5.6 MB  | efeitos sonoros (formato SUD)        |

Mais **77 `.PUD`** (mapas single/multi) soltos em `WarcrafD/` + `PUDS/`.

### Formato do container `.WAR` — DECODIFICADO

O `.WAR` do WC2 é a **mesma família do WC1**, só muda o magic. Header
little-endian:

    u32 magic            0x18 = WC1   |   0x19 = WC2
    u16 nEntries         nº de chunks (MAINDAT.WAR = 528)
    u16 id/flags         MAINDAT.WAR = 1000 (0x03e8)
    u32 offset[nEntries] tabela de offsets absolutos dos chunks

**Prova de que lemos certo:** `offset[0]` do MAINDAT = 2120, que bate com
`8 (header) + 528×4 (tabela) = 2120`. No WC1 o mesmo cálculo fecha em 2340
(`8 + 583×4`). Conferido via Python no recon.

Larguras de leitura no idioma B++ (deduzidas do `vswap_dump`, working code):
**`peek_q` = 2 bytes (u16)**, **`peek_h` = 4 bytes (u32)**, `peek` = 1 byte,
`peek_w`/`*p` = 8 bytes.

### Formato do GRAPHIC (sprite) — DECODIFICADO (2026-06-02)

Os chunks são comprimidos (flag `0x20` no high byte do `u32` inicial) com
**LZSS, ring buffer de 4096 bytes** (control byte LSB-first: bit 1 = literal,
bit 0 = back-ref `u16 = (len-3)<<12 | offset`). Algoritmo idêntico WC1↔WC2.

Header do sprite (após descomprimir) **difere do WC1**:

| | WC1 (magic 0x18) | WC2 (magic 0x19) |
|---|---|---|
| nframes | u16 @0 | u16 @0 |
| width   | **u8 @2** | **u16 @2** |
| height  | **u8 @3** | **u16 @4** |
| 1º descritor | @4 | **@6** |

Descritor de frame (8 bytes, igual nos dois): `u8 xoff, yoff, width, height`
+ `u32 offset` (high bit → `width += 256`). Frames arrumados 5-por-linha
quando `nframes%5==0` (convenção de 5 direções).

**Pixels do frame — RLE por linha (diferença crítica WC1↔WC2):**
- **WC1:** linhas raw (`width × height` bytes de índice direto).
- **WC2:** sprite "compilado". Em `chunk+offset`: `u16 rowoffset[height]`
  (offset de cada linha relativo ao início do frame), depois linhas RLE
  estilo GRP. Cada linha são pacotes até completar `width`:
    - `b & 0x80` → run transparente de `(b & 0x7F)` (pula, deixa transparente)
    - `b & 0x40` → run-length: `(b & 0x3F)` cópias do **próximo** byte
    - senão      → literal: copia `b` bytes
- **Paleta = índice 2** (paleta principal, confirmada no `wartool.cpp` do
  Wargus). VGA `RGB 0-63 << 2`. **Transparência: índices 0 E 255** (o wartool
  marca 255 transparente e remapeia 0→255).

**`war2_gfx.bpp` escrito e VALIDADO visualmente (2026-06-02):** com o RLE +
paleta 2 + transparência 0/255, decodificou unidades humanas reais e
coloridas corretamente — chunk 56 (mago/clérigo de robe azul, 5 dir × 13
frames) e chunk 33 (unidade maior 56×56 com animação de morte). LZSS + header
+ frames + RLE + sheet + cor: **tudo correto**. Uso:
`war2_gfx <ARCHIVE.WAR> <pal_idx> <gfx_idx> <out.png>`, ramifica por magic
(0x18 raw / 0x19 RLE).

### Catálogo de chunks do MAINDAT.WAR (2026-06-02)

Classificador valida sprite real por estrutura (header plausível + frame0 com
`rowoffset[0] == 2*height`). **262 sprites válidos.** Faixas identificadas a
olho (frame-0, paleta 2) num contact-sheet:

| Faixa de índice | Conteúdo |
|---|---|
| **33–70, 120–127, 182–183, 526–527** | **Unidades** (5 direções; navios, voadores, casters, catapultas, ogros) |
| **80–191, 473–525** | **Prédios** (maioria 2 frames 96×96/128×128 = normal+danificado; 2 facções) |
| **324–358, 470–471** | **Efeitos / projéteis / ícones** (16×16, 32×32) |
| **2,10,18,300,363–368,438** | Paletas (768 bytes) |

IDs confirmados a olho: 35=Gryphon Rider, 36=Dragão, 49/50=catapulta/ballista,
52=ogro, 56=mago azul, 39/40/41/59-62=navios.

✅ **Mapa índice→asset RESOLVIDO** (`tools/funnel/war2_maindat_index.md`): peguei
a tabela `Todo[]` do `wartool.h` do Wargus — os índices batem com nosso arquivo
DOS. Cada entrada dá faction + categoria + **paleta** + archive index. Pareamento
de paleta confirmado: unidades=2, winter=18, wasteland=10, swamp=438.
**Peasant=47, Peon=48** (confirmado pelos `%4_with_wood`=122/`%4_with_gold`=124
e validado a olho: walk+trabalho+morte em 5 direções, cor correta).

**Resolvido:**
- ✅ **Sidecar Aseprite** — `war2_gfx` já emite `<out>.json` (Aseprite "Array":
  `frames[]` com bbox por célula + `meta.image/size`), carregável pelo
  `image_load`. frameTags nomeados (walk/attack/die) = passo seguinte, dependem
  das contagens de anim por unidade (def. do wargus, como o `walk_n/atk_n/die_n`
  do `wc1_sprite_convert`).
- ✅ **Roster extraído** — `tools/funnel/war2_extract_rts2.sh` (driver batch,
  tabela idx→paleta→nome é a fonte da verdade; re-rodar regenera consistente)
  gerou **41 sprites** em `games/rts2/assets/sprites/wc2/`. Nomeados (identificados
  a olho @frame 5, pares humano/orc): footman/grunt, peasant/peon (+ carry
  wood/gold), ballista/catapult, knight/ogre, archer/axethrower, mage/
  death_knight, gryphon_rider, dragon, transports, corpses, critters. 15
  `unit_NNN` de fallback (navios/voadores/especiais) — renomear editando a
  tabela do driver conforme confirmar.

## EXTRAÇÃO COMPLETA (2026-06-02)

Um comando — `sh tools/funnel/war2_extract_rts2.sh` — gera tudo em
`games/rts2/assets/`:

| Asset | Qtd | Tool | Formato |
|---|---|---|---|
| Sprites (unidades + prédios summer) | 81 png+json | `war2_gfx` | Aseprite "Array" |
| Tilesets (summer/winter/wasteland/swamp) | 4 png+json | `war2_tiles` | atlas_grid 16×16 |
| Mapas (todos os PUDs) | 77 json | `war2_pud` | level_editor |

Tools do funnel (todos standalone `.bpp`, leitura de dados, nada executa os
binários do jogo):
- `war2_dump.bpp` — inspeciona o container `.WAR`.
- `war2_gfx.bpp` — sprite/prédio: LZSS + RLE WC2 + paleta + sheet 5-dir + sidecar.
- `war2_tiles.bpp` — tileset: megatile (idx-1)/minitile (idx)/paleta; offset de
  minitile = `(ref & 0xFFFC) << 4` (WC2, vs `<<1` do WC1); flip x/y.
- `war2_pud.bpp` — PUD → level JSON. Seções tagged (tag+u32len): lê ERA
  (tileset), DIM (w,h), MTXM (tiles). **MTXM indexa o sheet de megatiles
  diretamente** (verificado renderizando ALAMO — mapa coerente).
- `war2_extract_rts2.sh` — driver único (tabela idx→paleta→nome = fonte da
  verdade) que constrói os 3 tools e extrai sprites + tilesets + mapas.

**Pendências (refino, não bloqueiam):**
- Nomes semânticos dos 15 `unit_NNN` (navios/voadores) + dos prédios
  (`bldg_*_NNN` → town_hall/farm/...): editar a tabela do driver e re-rodar.
- Tags de animação nomeadas no sidecar (walk/attack/die — contagens por unidade).
- Tiles MTXM > 1488 (megatiles especiais raros) clampam pra 0 hoje; o set de
  megatiles cobre o grosso do mapa.
- **Scaffolding rts2**: limpar a pasta (`*.bsm~`, subpasta `rts1/`, `build/`),
  rename `rts1_`→`rts2_`, repontar os paths de asset pros `assets/*/wc2/`.

### Consequência pro plano

O caminho **B (decoder próprio em B++)** deixou de ser "caro" e virou o
recomendado: como o container é quase idêntico ao WC1, o `war2_dump.bpp`
(S0) é praticamente um fork do leitor do WC1. **Não precisamos do `wartool`
externo** — dá pra ficar 100% zero-dep não-B++. O `wartool` do Wargus fica
como referência opcional (índices de chunk + os `scripts/*.lua` com os
números de units/buildings).

## Pré-requisito (RESOLVIDO)

Os `.WAR` já estão na máquina (ver RECON acima). O único caminho externo que
NÃO seguimos: rodar o instalador da BNE em Windows/Wine/VM — desnecessário
agora que temos a fonte DOS. O `wartool` do repo **Wargus**
(github.com/Wargus/wargus, NÃO o war1gus que é o irmão WC1) continua útil
só como referência cruzada de índices + scripts.

## Decisão de arquitetura (resolver no S0)

| Caminho | O que é | Custo | Quando escolher |
|---|---|---|---|
| **A — consumir saída do wartool** | funnel lê os PNG/WAV/MID que o wartool já gerou | baixo (= caso WC1) | default; sprites, sons, música |
| **B — decoder .WAR/PUD próprio em B++** | funnel abre o container binário direto | alto (= caso Wolf3D) | só se quisermos pureza zero-dep total, ou se o wartool não cobrir mapas |

Recomendação inicial: **híbrido** — caminho A pra sprites/áudio (já
saem PNG/WAV/MID), e pra mapas depende do que o wartool entrega (S0
decide entre reusar `wc1_map_convert` ou escrever um parser PUD).

## Source material

| Fonte | Onde | Papel |
|---|---|---|
| Archives WC2 | CD/install (`*.WAR`) | a constraint real — PNG/WAV/MID/PUD que viram nossos assets |
| `wartool` | repo Wargus | extrator + conversor pra PNG/Lua (= war1tool) |
| `wargus/scripts/*.lua` | repo Wargus | números de partida (units/buildings/upgrades) — copiar o bom, mudar o resto |
| Formato PUD | doc Wargus / pud.txt | layout binário dos mapas WC2 (se formos pelo caminho B) |

## Sessões

### S0 — Recon + dump do container (PARCIAL — 2026-06-02)
- [x] Fonte localizada (DOS Tides of Darkness, `.WAR` soltos — ver RECON).
- [x] Container `.WAR` decodificado (magic/nEntries/id/offset[]) e validado.
- [x] **`tools/funnel/war2_dump.bpp`** — fork do `vswap_dump`: lê magic +
      nEntries + id + tabela de offsets, reporta nº de chunks + spot-check,
      valida `offset[0] == 8 + nEntries*4`. **VALIDADO 2026-06-02** contra
      MAINDAT (528 chunks, id 1000), REZDAT (104, id 3000), STRDAT (127,
      id 4000) — todos PASS — e cross-game no DATA.WAR do WC1 (583, magic
      0x18). Larguras confirmadas: `peek_q`=2 bytes (u16), `peek_h`=4 (u32).
- [ ] **Mapas são PUD cru** (binário) — não há SMS aqui, os 77 `.PUD` estão
      soltos. Próximo S0 separado: `war2_pud_dump.bpp` no mesmo espírito
      (ler section table TYPE/VER/DIM/ERA/MTXM/UNIT…, validar contra um
      `.PUD` real pequeno antes de decodificar tiles).
- [ ] WC2 tem 4 tilesets (forest / winter / wasteland / swamp) vs 1 do WC1 —
      mapear tile-id por tileset (cruzar com `wargus` se preciso).
- **Entrega:** `war2_dump` rodando + nº de chunks de cada `.WAR` confirmado.

### S1 — Um tileset end-to-end (forest, o mais próximo do WC1)
- `war2_map_convert.bpp` (ou wc1_map_convert estendido) emite UM mapa
  forest → JSON `{type,version,width,height,tiles[][]}`.
- Carrega no rts1 via o loader existente + abre na aba Levels do Bang 9 +
  hot-reload via file_watch (round-trip de graça, igual WC1).
- **Entrega visível:** um mapa WC2 renderizado e editável no Bang 9.

### S2 — Um sprite de unidade end-to-end
- `war2_sprite_convert.bpp`: PNG sheet do wartool → sidecar Aseprite com
  frameTags nomeados (idle/walk/attack/die por direção).
- Tratar as diferenças WC2 vs WC1: framing, contagem de direções, e o
  fato de WC2 ter unidades navais/aéreas (schema de anim diferente —
  cribar de `wargus/scripts/anim.lua`, igual fizemos com war1gus).
- Pega o equivalente do peasant (footman? peon?) → anima no rts1 via
  image_load.
- **Entrega visível:** uma unidade WC2 andando/atacando no jogo.

### S3 — Largura + unificação do funnel
- Batch: 4 tilesets, roster completo, naval/aéreo, buildings, ícones de
  upgrade.
- Dobrar `war2_*` no dispatcher único `funnel <source> <kind>` + atlas
  packer/emitters compartilhados (a unificação S3 que o journal já
  marcou como pendente pro funil inteiro).

### S4 — Áudio/música (opcional, barato)
- WAV/MID do wartool já são canônicos — só fiar no stbmixer/stbmidi.

## Caveats / questões abertas
- **Packaging do remaster 2025** pode não ser lido pelo wartool clássico
  (resolver no S0).
- **Onde vai a saída?** Decisão de produto: WC2 aumenta o rts1 atual, ou
  vira um `games/rts2/`? Default deste handoff: dir paralelo
  `assets/converted_wc2/`, decisão de jogo fica fora do lane de conversão.
- **Decoder .WAR em B++** (pureza zero-dep total) fica como follow-on,
  não bloqueia nada — o wartool externo offline é aceito (já usamos
  unblzzrd + war1tool assim no WC1).
