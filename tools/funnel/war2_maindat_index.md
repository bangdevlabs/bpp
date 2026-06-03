# WC2 MAINDAT.WAR — mapa de índices (graphics)

Fonte: `Todo[]` em `wartool.h` do Wargus (github.com/Wargus/wargus). Os
índices de archive batem com o nosso `DATA/MAINDAT.WAR` (DOS Tides of
Darkness, 528 entradas) — verificado pelo `war2_dump` + decode.

Formato da entrada original: `{G, arg, "caminho", PALETTE_IDX, ARCHIVE_IDX}`.
O `%NN` no caminho é o id de tipo de unidade/prédio interno do WC2 (o nome
legível sai da tabela de unidades; ver inferências abaixo).

## Regra de paleta (4º campo)

| Contexto | Paleta |
|---|---|
| Unidades + prédios summer (default) | **2** |
| Tileset winter | **18** |
| Tileset wasteland | **10** |
| Tileset swamp (variante `G,2`) | **438** |

`war2_gfx <MAINDAT.WAR> <PALETTE_IDX> <ARCHIVE_IDX> <out.png>`.

## Unidades (archive idx → faction/path)

| idx | paleta | path | inferência |
|---|---|---|---|
| 33 | 2 | human/units/%16 | |
| 34 | 2 | orc/units/%17 | |
| 35 | 2 | human/units/%44 | **Gryphon Rider** (voador) |
| 36 | 2 | orc/units/%45 | **Dragão** (voador) |
| 37 | 2 | orc/units/%47 | |
| 38 | 2 | human/units/%42 | |
| 39 | 2 | human/units/%-30 | navio |
| 40 | 2 | orc/units/%-31 | navio |
| 41 | 2 | human/units/%34 | |
| 42 | 2 | orc/units/%35 | |
| 43 | 2 | human/units/%40 | |
| 44 | 2 | orc/units/%41 | |
| 45 | 2 | human/units/%2 | melee humano (footman?) |
| 46 | 2 | orc/units/%3 | melee orc (grunt?) |
| **47** | 2 | human/units/%4 | **Peasant** (carrega wood/gold → 122/124) |
| **48** | 2 | orc/units/%5 | **Peon** (→ 123/125) |
| 49 | 2 | human/units/%6 | |
| 50 | 2 | orc/units/%7 | |
| 51 | 2 | human/units/%8 | |
| 52 | 2 | orc/units/%9 | |
| 53 | 2 | human/units/%10 | |
| 54 | 2 | orc/units/%11 | |
| 55 | 2 | human/units/%12 | |
| 58 | 2 | orc/units/%13 | |
| 59 | 2 | human/units/%-28_empty | navio/transporte vazio |
| 60 | 2 | orc/units/%-29_empty | |
| 61 | 2 | human/units/%32 | navio |
| 62 | 2 | orc/units/%33 | navio |
| 63 | 2 | orc/units/%43 | |
| 64 | 2 | summer/neutral/units/%59 | crítter neutro (summer) |
| 65 | 10 | wasteland/neutral/units/%59 | crítter (wasteland) |
| 66 | 18 | winter/neutral/units/%59 | crítter (winter) |
| 69 | 2 | neutral/units/%57 | |
| 70 | 2 | neutral/units/%58 | |
| 120 | 2 | neutral/units/corpses | corpos |
| 122 | 2 | human/units/%4_with_wood | Peasant c/ lenha |
| 123 | 2 | orc/units/%5_with_wood | Peon c/ lenha |
| 124 | 2 | human/units/%4_with_gold | Peasant c/ ouro |
| 125 | 2 | orc/units/%5_with_gold | Peon c/ ouro |
| 126 | 2 | human/units/%-28_full | transporte cheio |
| 127 | 2 | orc/units/%-29_full | |
| 182 | 10 | wasteland/human/units/%40 | |
| 183 | 10 | wasteland/orc/units/%41 | |
| 470/526/527 | 438 | swamp neutral/human/orc units | variantes swamp |

## Prédios (archive idx → faction/path), por tileset

- **Summer:** 80–119 (human/orc pares, `%-98`…`%95`) + 121 destroyed_site.
- **Winter:** 128–163, 169–172, 184, 186, 262–276 (paleta **18**).
- **Wasteland:** 173–180, 185, 188, 191, 271–276 (paleta **10**).
- **Swamp:** 473–525 (paleta **438**, variante `G,2`).
- **Construction sites:** 252–276 (land/shipyard/oil_well/refinery/foundry/wall).
- **Startpoints:** 164 (human x_startpoint), 165 (orc o_startpoint).

## Efeitos / projéteis / ícones

idx 324–358, 470–471 (16×16 / 32×32 / 48×48) — explosões, projéteis, etc.
(não vêm da faixa de unidades; classificar quando precisarmos deles).

## Pendente

- Resolver `%NN` → nome canônico (peasant/footman/knight/…): a numeração `%N`
  é o id de tipo do WC2; vem da tabela de unidades do wargus. Peasant=%4,
  Peon=%5 já confirmados pelos sufixos with_wood/with_gold.
