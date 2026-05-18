# Sidequest — WC1 sprite pipeline migrates to Aseprite-compatible format

**Status:** READY-TO-EXECUTE (escrito 2026-05-16, rewritten do plano original Nível 2).
**Goal:** Adotar **Aseprite-compatible sprite sheet** como canonical format pra WC1 sprites (e qualquer sprite com animação futuro). Modulab pivota de "editor primário tentando competir com Aseprite" pra "IDE-integrated companion: Aseprite pra trabalho heavy, Modulab pra tweaks rápidos in-context." WC1 Session 5 fecha em cima desse pipeline.
**Source of truth do estado atual:**
- WC1 Session 4 closed (commit `7498f6a`). Selection + camera + start_view consumer alive.
- WC1 Session 5 EM PROGRESSO em working tree — movement code working (~28 right-clicks testados pelo usuário), animation NÃO shipada. Working tree tem partial anim hardcode em `wc1_units.bsm` (PEASANT_WALK_* + FACE_* consts) que NÃO foi acordada com o usuário — abandonar essa abordagem.
- Decisão estratégica desta sessão (2026-05-16): adotar Nível 3 (Aseprite-compatible) em vez de Nível 2 (bespoke Aseprite-inspired). Vide seção "Por que Nível 3 vs Nível 2" abaixo.

---

## Por que Nível 3 vs Nível 2

Plano original (versão anterior deste doc) propunha Nível 2 — bespoke atlas_pack v3 com `animations` field shape-inspired pelo Aseprite. Após discussão estratégica, pivotamos pra Nível 3 — adoção literal do schema Aseprite-export.

**A frase que fechou a decisão**: "Aseprite pra trabalho heavy, Modulab pra tweaks rápidos in-context."

### Implicação concreta — Modulab role muda

| Aspecto | Nível 2 (rejeitado) | Nível 3 (este plano) |
|---|---|---|
| Modulab role | Editor primário, anos de roadmap pra Aseprite-parity | IDE-integrated companion, scope nítido (slice-edit + hot-reload) |
| Asset packs internet | Sempre convert | Cola direto (Aseprite is the industry standard) |
| War1tool import | ~200 LOC (palette extraction + 30 JSONs) | ~30 LOC (preserve PNG + emit sidecar JSON) |
| GPU upload | N pequenas | 1 grande (batch-friendly) |
| Dev onboarding | "Aprenda Modulab" | "Você já usa Aseprite? Esse é nosso pipeline" |
| BangDev cara permanece em | Format proprietary | Bang 9 + hot-reload + B++ runtime + IDE integration |

A "cara" do BangDev não tava no formato — tava no rail de runtime (Bang 9 + hot-reload + B++). Adotar formato industry libera energia pra investir onde é único.

### Memory update needed

Memória atual `project_bangdev_studio_philosophy.md` diz: "Bespoke formats, no Aseprite/Tiled adoption. Industry interop is NOT the anchor for format decisions."

Essa memória precisa ser **atualizada** pós-execução deste sidequest pra refletir Nível 3 — algo como "Aseprite-compatible sprite sheets adopted as canonical (2026-05-16). Modulab repositions as IDE-companion not Aseprite-replacement. Level JSON + atlas_pack manifest convention stays bespoke. Tonify Rule 31 stays — what changed is which formats earn 'canonical' status."

---

## Contexto

### O que pathfind já tem (rail funcionando, atlas_pack-style)

```
games/pathfind/assets/sprites/
├── cat_sprite.json         ← Modulab native (type:"sprite", v3)
├── rat_sprite.json         ← idem
└── pathfind.atlas.json     ← manifest (type:"atlas_pack", v2, sprites:[{name,path}])
```

**Esse pattern (atlas_pack manifest + per-sprite JSON) é o caso "individually-authored, no animation"**. Permanece válido pra games que autoram sprites direto no Modulab sem animação. NÃO é migrado.

### O que WC1 hoje (atlas_grid — formato errado, deviation S3)

```
games/rts1/assets/sprites/wc1/
└── peasant.json            ← atlas_grid v2 (tile_w/tile_h/image apontando pra peasant.png)
```

`peasant.png` é saída raw do war1tool. Frames endereçados por índice numérico (PEASANT_IDLE_N = 0, etc) como const hardcoded em `wc1_units.bsm`. Sem anim metadata.

S3 silent deviation: `wc1_handoff.md:31-33` PEDIU "Sprites ship as Modulab atlas-pack files (`*.atlas.json`, same shape pathfind uses)". O agente shipou atlas_grid e não flagou.

### O que migra pra Nível 3

**WC1 sprites com animação** (peasant + futuros footman/archer/cleric/etc) viram **Aseprite-compatible sprite sheets**:

```
games/rts1/assets/sprites/wc1/
├── peasant.png            ← PRESERVADO (war1tool output, 160×416)
└── peasant.json           ← sidecar Aseprite-schema (frames object + meta.frameTags)
```

**Pathfind cat/rat permanecem atlas_pack** — não tem anim, formato atual serve.

### Convention pending — `.atlas.json` double-suffix

Journal 2026-05-13: `.level.json` retirado em favor de `.json`. Journal 2026-04-23: `.modulab.json` retirado em favor de `.json`. **Holdout único: `.atlas.json` em `pathfind.atlas.json`.** Esse sidequest fecha o ciclo: vira `pathfind.json` (folder `assets/sprites/` + content sniffing em `image_load` desambiguam).

---

## Plan — 6 commits ordenados (bisect-friendly)

**Visão geral**: Commits 1-4 entregam infra (movement + format + game animation). Commits 5-6 fecham o pivot completando a "Modulab as IDE-companion" promise — read+display de Aseprite sheets primeiro, slice-edit depois. Sem Commits 5-6 o pivot fica hollow ("Aseprite faz tudo, Modulab vê pathfind só, WC1 vive fora do IDE"). User goal explícito: poder fazer mod de WC1, brincar com isso, editando in-context no Modulab.

### Commit 1 — WC1 S5 PARTIAL: movement (sem animation)

**Por quê primeiro:** o trabalho de movement está pronto no working tree mas vinha bundlado com partial anim hardcode que o usuário rejeitou. Extrair movement-only, livrar a parte que funciona, deixar animation pra depois da migração do pipeline.

**Working tree state esperado antes deste commit (verificar via `git status --short`):**
```
games/rts1/rts.bpp                  ← modified (movement wiring)
games/rts1/wc1_hud.bsm              ← modified
games/rts1/wc1_input.bsm            ← modified (right-click target write + path req)
games/rts1/wc1_render.bsm           ← modified (Pos in pixels + Path component)
games/rts1/wc1_units.bsm            ← modified (PARTIAL ANIM HARDCODE aqui — REVERTER)
games/rts1/wc1_movement.bsm         ← NEW (A* + per-unit cursor + pixel interp)
games/rts1/rts                      ← rebuilt binary
```

**Passos:**

1. **Reverter partial anim hardcode em `wc1_units.bsm`** — remover qualquer PEASANT_WALK_N/NE/E/SE/S const, PEASANT_WALK_CYCLE const, FACE_N/NE/E/SE/S/SW/W/NW const adicionados nesta sessão. Manter:
   - PEASANT_IDLE_N..S consts (foram do S3, válidos)
   - speed_q12 → speed_pxs rename (legitimate movement work, não anim)
   - UnitDef struct + unit_defs_init + unit_def_for (não foram tocados pelo anim hardcode)
2. **Verificar que o resto do diff é movement-only:** wc1_movement.bsm (NEW), wc1_input.bsm (right-click target + path req), wc1_render.bsm (Pos in pixels + Path component + drop multiplies), wc1_hud.bsm (drop multiplies), rts.bpp (load + init + tick).
3. **Atualizar `wc1_handoff.md`** S5 entry — marcar PARTIAL com honest accounting do que ficou pra animation sidequest.

**Commit message:**

```
WC1 mod Session 5 PARTIAL: movement (animation pending Aseprite pipeline migration)

Ships the A* movement core from the canonical S5 scope; animation
deferred to a follow-up sidequest after the sprite pipeline migrates
to Aseprite-compatible sheet format.

Why split: animation requires walk frames addressable BY NAME (so
the game says "play walk_N" not "show frame 5"). Today's
peasant.json is atlas_grid (S3 silent deviation from the handoff
plan). Forcing animation in atlas_grid means hardcoding frame
indices + facing→base mapping per game — exactly the copy-paste
the asset format spec was designed to prevent. The pipeline
migration (this sidequest's later commits) shifts WC1 sprites to
Aseprite-compatible JSON sidecars where frameTags carry the anim
metadata, then S5 close wires the game-side animation lookup.

Movement pieces shipped:
- games/rts1/wc1_movement.bsm (NEW)
  - Path component: list of waypoint tile coords + cursor index
  - A* call via stbpath when right-click resolves to a target tile
  - Per-frame: advance cursor at unit speed_pxs; pixel-interpolate
    between current waypoint and next for smooth render
- games/rts1/wc1_input.bsm — right-click writes Target component +
  invokes wc1_movement_request which kicks off A*
- games/rts1/wc1_render.bsm — Pos now lives in pixel coords (was
  grid coords) so per-frame interpolation is straightforward;
  _render_unit reads interpolated Pos directly
- games/rts1/wc1_units.bsm — speed_q12 → speed_pxs (clearer name,
  same semantic: pixels per second)
- games/rts1/rts.bpp — game loop calls wc1_movement_tick

Tested: user reported ~28 right-clicks resolved correctly,
peasant walks to each target, redirect mid-path works, arrival
snaps to tile center.

NOT shipped (defer to follow-up commits in this sidequest):
- Walk frame cycling (needs Aseprite-format anim metadata)
- Facing direction calc + sprite mirror for west variants
- Multi-unit simultaneous walk verification (code supports it
  but visual confirmation pending until anim lands)

Handoff updated: games/rts1/wc1_handoff.md marks S5 PARTIAL with
honest accounting.
```

**Verification:**
```bash
./bpp games/rts1/rts.bpp -o games/rts1/rts
./games/rts1/rts                    # camera + selection + movement work as before
# (no compiler/stb touch — bootstrap byte-stable + suite irrelevant per scope)
```

---

### Commit 2 — Convention cleanup: `.atlas.json` → `.json`

**Por quê:** fechar o ciclo dos 3 retirements de double-suffix. Convention nasce consistente ANTES do pipeline migration pra não nascer com nome legacy.

**Pontos a tocar (verificados via grep — confirmar via grep antes do commit):**

**Asset rename (git mv preserva history):**
- `games/pathfind/assets/sprites/pathfind.atlas.json` → `pathfind.json`

**Code updates:**
- `games/pathfind/pathfind.bpp:533`:
  - `image_load("assets/sprites/pathfind.atlas.json")` → `image_load("assets/sprites/pathfind.json")`
- `games/pathfind/pathfind.bpp:49` (comment): `pathfind.atlas.json` → `pathfind.json`
- `tools/modulab/modulab_lib.bsm:606`: comment "Writes <prefix>.atlas.json" → "Writes <prefix>.json"
- `tools/modulab/modulab_lib.bsm:616`: `strbuf_cat(atlas_path, ".atlas.json")` → `strbuf_cat(atlas_path, ".json")`
- `tools/modulab/io.bsm:289-291` (comments): refs a `pathfind.atlas.json` → versão sem `.atlas`
- `tools/sprite16_to_atlas.sh` (se existir — confirmar via `ls` antes): usage examples + error message migrate
- `stb/stbimage.bsm:36, 1249, 1425` (comments): `hero.atlas.json` / `pathfind.atlas.json` → versão sem `.atlas`

**Doc updates:**
- `docs/gpu_pipeline_roadmap.md` (5 refs)
- `docs/bang9_space_manual.md:112` (`tileset.atlas.json` → `tileset.json`)
- `docs/stb++_lib.md` (2 refs)
- `games/rts1/wc1_handoff.md:31, 263, 266, 269` — substituir `*.atlas.json` → `*.json` mantendo o resto

**Journal entry novo** documentando o cycle closure.

**Preservar (NÃO tocar):** historical journal entries que mencionam `pathfind.atlas.json` accurately describe past state.

**Verification:**
```bash
./bpp games/pathfind/pathfind.bpp -o games/pathfind/path
./games/pathfind/path                 # boots, cat/rat sprites render
./bpp tools/modulab/modulab.bpp -o /tmp/m  # compiles clean
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2) | head    # empty
bash tests/run_all.sh
bash tests/run_all_c.sh
```

**Stop conditions:** pathfind doesn't open after rename → revert; Modulab atlas save broken → check writer path concat; bootstrap diverges → unexpected, investigate.

---

### Commit 3 — `wc1_sprite_convert` + Aseprite-schema sidecar

**Por quê:** trazer peasant.png pro pipeline Aseprite. O converter PRESERVE the PNG e emite sidecar JSON no schema exato do Aseprite export.

**Novo arquivo:** `tools/wc1_sprite_convert/wc1_sprite_convert.bpp`

**Input:**
- `games/rts1/assets/converted/graphics/human/units/peasant.png` (war1tool output)
- Knowledge embedded sobre o layout 5×13:
  - row 0 (frames 0..4): idle facing N, NE, E, SE, S
  - rows 1..5 (frames 5..29): walk cycle N, NE, E, SE, S — 5 frames per row
  - rows 6..12: chop / carry / die — emit como frameTags com names genéricos `action_<row>_<col>`, anims dedicadas ficam pra S7+

**Output:**
- `games/rts1/assets/sprites/wc1/peasant.png` (COPIADO do source path, vive junto do sidecar)
- `games/rts1/assets/sprites/wc1/peasant.json` (Aseprite-schema sidecar)

**Schema do sidecar — Aseprite-compatible** (validar contra `aseprite --batch --data export.json` output):

```json
{
  "frames": {
    "peasant 0": {
      "frame": {"x": 0, "y": 0, "w": 32, "h": 32},
      "rotated": false,
      "trimmed": false,
      "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32},
      "sourceSize": {"w": 32, "h": 32},
      "duration": 100
    },
    "peasant 1": {...},
    ...
    "peasant 64": {...}
  },
  "meta": {
    "image": "peasant.png",
    "size": {"w": 160, "h": 416},
    "scale": "1",
    "format": "RGBA8888",
    "frameTags": [
      {"name": "idle_N",  "from": 0, "to": 0,  "direction": "forward"},
      {"name": "idle_NE", "from": 1, "to": 1,  "direction": "forward"},
      {"name": "idle_E",  "from": 2, "to": 2,  "direction": "forward"},
      {"name": "idle_SE", "from": 3, "to": 3,  "direction": "forward"},
      {"name": "idle_S",  "from": 4, "to": 4,  "direction": "forward"},
      {"name": "walk_N",  "from": 5, "to": 9,  "direction": "forward"},
      {"name": "walk_NE", "from": 10, "to": 14, "direction": "forward"},
      {"name": "walk_E",  "from": 15, "to": 19, "direction": "forward"},
      {"name": "walk_SE", "from": 20, "to": 24, "direction": "forward"},
      {"name": "walk_S",  "from": 25, "to": 29, "direction": "forward"}
    ]
  }
}
```

**Notas de schema:**
- `frames` é OBJECT (não array) keyed por string `"<sheet_name> <index>"` — convention literal do Aseprite.
- `frame` = bounding box dentro do PNG. Para uniform-grid (nosso caso) é `{x: col*tw, y: row*th, w: tw, h: th}`.
- `duration` ms PER-FRAME (Aseprite suporta variable rate; nós emitimos uniforme 100ms pra v1, mas o LOADER suporta variable).
- `meta.image` resolve relative ao sidecar (mesma convention dos outros JSONs do projeto).
- `frameTags[].direction` = "forward" sempre na v1; "reverse"/"pingpong" deferred pra schema bump quando jogo pedir.
- `rotated`/`trimmed`/`spriteSourceSize`/`sourceSize` campos do Aseprite que nós emitimos com defaults (rotated=false, trimmed=false, source=frame) pra compatibilidade total.

**Tool steps (no .bpp):**

1. Parse argv: `wc1_sprite_convert <input.png> <output_dir>`.
2. Load PNG via `pixels_load(path, &w, &h)` — só pra ler dimensions; pixels não precisam ser parsed (vamos COPIAR o PNG bit-a-bit).
3. Compute frame grid: `cols = w/32, rows = h/32` (assert cols=5, rows=13 pra peasant; warn se outro).
4. Copy PNG: `file_copy(input_path, output_dir + "/" + basename(input))`.
5. Build sidecar JSON via strbuf:
   - frames object com `cols * rows` entradas, cada uma com bounding box computado + duration default
   - meta object com image basename + size + frameTags array
6. Write sidecar via `file_write_all(output_dir + "/" + basename_no_ext + ".json")`.
7. Stats: `"imported peasant: 65 frames, 10 anims, source=<png>, output=<sidecar>"`.

**Asset cleanup:**
- Delete `games/rts1/assets/sprites/wc1/peasant.json` ANTIGO (atlas_grid v2) — substituído pelo Aseprite sidecar com o MESMO nome.
- PNG vai pra `games/rts1/assets/sprites/wc1/peasant.png` (copia do converted dir; manter source path original NÃO tocado).

**Game-side update (INCLUÍDO NESTE COMMIT — atomic):**
- `games/rts1/rts.bpp`: `image_load` path stays `assets/sprites/wc1/peasant.json` (renamed atlas_grid → renamed Aseprite sidecar, mesmo nome).
- Frame access NÃO MUDA neste commit — game continua usando `PEASANT_IDLE_S` const = 4 (index direto). API de Aseprite-aware lookup vem no Commit 4.
- A razão pra game compilar e rodar sem mudar API: stbimage neste commit ainda não sabe Aseprite schema. Loader detecta `meta.image` presence + falls back pra "use as atlas_grid" mode interpreting `frames` count vs uniform tile_w/tile_h. **Verificar viabilidade — se loader não consegue parse Aseprite shape no Commit 3, mover game-side rendering pra Commit 4.**

**Decisão sobre file_copy:** se `file_copy` não existe ainda no runtime, options:
- (a) Read source bytes + write to dest (~10 LOC, tools-only não polui stb)
- (b) Adicionar `file_copy` em bpp_file.bsm como new primitive (mais limpo mas escopo extra)
- Recomendação: opção (a) inline no tool. Promove pra primitive se segundo consumer pedir.

**Verification:**
```bash
./bpp tools/wc1_sprite_convert/wc1_sprite_convert.bpp -o tools/wc1_sprite_convert/wc1_sprite_convert
tools/wc1_sprite_convert/wc1_sprite_convert \
  games/rts1/assets/converted/graphics/human/units/peasant.png \
  games/rts1/assets/sprites/wc1
ls games/rts1/assets/sprites/wc1/        # peasant.png + peasant.json
head -1 games/rts1/assets/sprites/wc1/peasant.json | grep frames    # contains "frames"
./bpp games/rts1/rts.bpp -o games/rts1/rts
./games/rts1/rts                          # peasant ainda renderiza idle facing south
```

**Stop conditions:**
- PNG copy falha → check file io paths
- Game renderiza errado depois da migração → verify image_load fallback semantic; pode precisar adiantar Commit 4 parser
- Sidecar JSON inválido (Aseprite tool rejeita) → comparar byte-a-byte com `aseprite --batch --data` output de um asset trivial pra ajustar shape

---

### Commit 4 — stbimage parses Aseprite + game-side animation wires S5

**Por quê:** stbimage aprende o schema Aseprite + S5 fecha em cima dele. Bundled em single commit pq game-side API consumption + library API depend em landar juntos pra não quebrar build.

**`stb/stbimage.bsm` extensions:**

1. **Parser** — image_load detecta Aseprite shape via `meta.frameTags` presence em parsed JSON, dispatches pra `_image_load_aseprite_sheet`:
   - Reads `meta.image` → loads PNG (relative to sidecar dir)
   - Walks `frames` object, builds parallel arrays: `frame_x[]`, `frame_y[]`, `frame_w[]`, `frame_h[]`, `frame_duration[]`, `frame_name[]`
   - Walks `meta.frameTags` array, builds parallel arrays: `tag_name[]`, `tag_from[]`, `tag_to[]`, `tag_direction[]` (direction enum: 0=forward, 1=reverse, 2=pingpong — só forward suportado v1, others ficam parsed mas execução é forward)
   - Stores no Image struct (cresce com aseprite-specific fields, ou wraps em sub-struct se preferir)

2. **API nova**:
   - `image_anim_id(atlas, name_cstr)` — linear search tag_name[] for matching string, return idx or -1. Cache na primeira chamada via hash se necessário (probably overkill v1, linear is fine pra ~10 tags).
   - `image_anim_frame(atlas, anim_id, elapsed_ms)` — returns sprite_id (index into frames array) of current frame:
     ```
     tag = tags[anim_id]
     frame_count = tag.to - tag.from + 1
     // For uniform duration: cycle_total_ms = frame_duration[tag.from] * frame_count
     // For variable: sum frame_duration[tag.from..tag.to]
     // v1 assumes uniform (use first frame's duration)
     cycle_total_ms = frame_duration[tag.from] * frame_count
     phase = elapsed_ms % cycle_total_ms
     frame_offset = phase / frame_duration[tag.from]
     return tag.from + frame_offset
     ```
   - `image_draw_size_flipped(atlas, sprite_id, x, y, w, h)` — same as image_draw_size but swap u0/u1. Useful pra WC1's 5-facing mirror (W/NW/SW reuse E/NE/SE flipped).
   - `image_anim_name(atlas, anim_id)` — debug accessor, returns tag name. Útil pra logs.

3. **Smoke test** — `tests/test_atlas_aseprite.bpp` (NEW, ~50 LOC). Generates fixture sidecar JSON in /tmp pointing to a 2-frame test PNG (also generated). Validates:
   - `image_load(fixture_sidecar)` succeeds
   - `image_anim_id(atlas, "walk_test")` returns non-negative
   - `image_anim_id(atlas, "nonexistent")` returns -1
   - `image_anim_frame(atlas, walk_id, 0)` returns frame `from`
   - `image_anim_frame(atlas, walk_id, frame_duration)` returns frame `from + 1`
   - `image_anim_frame(atlas, walk_id, frame_duration * frame_count)` returns frame `from` (cycle wraps)

**`docs/asset_formats.md` update:**

Add new section "Sprite Sheet JSON (Aseprite-compatible)" entre "Atlas Pack" e "Atlas Grid":
- Schema explicit com Aseprite shape
- Detecção via `meta.frameTags` presence
- Animação via frameTags com from/to indices
- frame_duration per-frame (uniform comum, variable suportado)
- Direction enum (forward only v1)
- Note that this format is LITERAL Aseprite export — `aseprite --batch --data output.json input.aseprite` gera shape que our loader consome
- Mark atlas_grid como deprecated-for-new-sprites (still supported pra back-compat); novos sprites com anim ou imported via converter usam Aseprite sheet shape

Update "Atlas Pack" section pra mencionar quando usar qual:
- atlas_pack: per-sprite individually-authored Modulab content (pathfind cat/rat). Sem animation today.
- aseprite sheet: row-major sliced PNG + tag metadata. Anims first-class.
- atlas_grid: legacy, slowly retired

Update Rule 31 table:
- "Sprites (com animação)" row: Aseprite OR Modulab → Aseprite sheet JSON → `image_load` + `image_anim_id` + `image_anim_frame`
- "Sprites (sem animação)" row: Modulab → atlas_pack manifest → `image_load` + `image_named_id` (current shape stays)

**`games/rts1/wc1_render.bsm` updates:**

- Add `Anim` component: `struct Anim { state, facing, elapsed_ms }`
  - state: const enum (UNIT_STATE_IDLE = 0, UNIT_STATE_WALK = 1)
  - facing: enum FACE_N..FACE_NW (0..7)
  - elapsed_ms: accumulated ms desde state change (reset on facing change OR state change)
- Register anim_comp, peasant archetype grows from 5 → 6 components
- `_render_unit` ganha:
  ```
  auto a: Anim;
  a = ecs_get(w, anim_comp, eid);
  auto anim_id, flip;
  wc1_anim_id_for(a.state, a.facing, &anim_id, &flip);
  auto sprite_id;
  sprite_id = image_anim_frame(s.atlas, anim_id, a.elapsed_ms);
  if (flip) {
      image_draw_size_flipped(s.atlas, sprite_id, p.x - 8 - cam_x(cam), p.y - 16 - cam_y(cam), 32, 32);
  } else {
      image_draw_size(s.atlas, sprite_id, p.x - 8 - cam_x(cam), p.y - 16 - cam_y(cam), 32, 32);
  }
  ```
- Spawn loop init: anim.state = IDLE, anim.facing = FACE_S, anim.elapsed_ms = 0

**`games/rts1/wc1_anims.bsm` (NEW):**

- Const enums: UNIT_STATE_IDLE/WALK + FACE_N..FACE_NW
- Cache anim ids at startup:
  ```
  global _anim_idle_N, _anim_idle_NE, ..., _anim_idle_S;
  global _anim_walk_N, _anim_walk_NE, ..., _anim_walk_S;
  
  wc1_anims_init(atlas) {
      _anim_idle_N = image_anim_id(atlas, "idle_N");
      ...
      _anim_walk_S = image_anim_id(atlas, "walk_S");
  }
  ```
- `wc1_facing_for_delta(dx, dy)` — 8-octant bucket, vector → FACE_*
- `wc1_anim_id_for(state, facing, &out_anim, &out_flip)` — given state + facing, write anim_id + flip flag:
  - facings 0..4 (N..S): use direct anim, flip=0
  - facing 5 (SW): use anim of SE, flip=1
  - facing 6 (W): use anim of E, flip=1
  - facing 7 (NW): use anim of NE, flip=1

**`games/rts1/wc1_movement.bsm` updates:**

- After advancing Pos each tick:
  - Compute facing from movement vector (`new.x - old.x, new.y - old.y`)
  - If facing changed: reset elapsed_ms = 0
  - Increment elapsed_ms by dt
- When unit starts moving (Path populated): state = WALK, elapsed_ms = 0
- When unit arrives (cursor exceeds count): state = IDLE, elapsed_ms = 0

**`games/rts1/wc1_units.bsm` cleanup:**

- Remove old PEASANT_IDLE_N..S consts (replaced by Aseprite lookups)
- Remove `atlas_frame` field do UnitDef (redundante — Anim component carries state+facing)
- Manter speed_pxs, hp, armor, px_size, kind

**`games/rts1/rts.bpp` wiring:**

- After `image_hot_reload_enable(peasant_atlas)`: call `wc1_anims_init(peasant_atlas)` to cache anim ids
- Load wc1_anims.bsm

**`games/rts1/wc1_handoff.md` update:**

- Mark S5 CLOSED com honest accounting do shipping em multi-commit:
  - Commit 1: movement
  - Commits 2-3: pipeline migration
  - Commit 4 (this): animation infra + game-side wire
- Verification line: "3 peasants on the map, click each, send them to different corners — all walk along their A* paths simultaneously." Confirmar (forest1 já tem 2 peasants player 0+2; manual add 3rd via level_editor se quiser smoke completo).

**Verification:**
```bash
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2) | head    # empty
bash tests/run_all.sh    # 172 + 1 (test_atlas_aseprite) expected
bash tests/run_all_c.sh  # 137 + 1 expected
./bpp games/rts1/rts.bpp -o games/rts1/rts
./games/rts1/rts
# - Select peasant, right-click → walks toward target
# - Walk frames CYCLE visually (not slide)
# - Direction changes: facing rotates appropriately, mirror for W/NW/SW
# - Idle on arrival returns to idle anim
# - Modulab edit hot-reload still works (edit peasant.png externally, see change)
```

**Stop conditions:**
- Bootstrap diverges → Image struct field add affecting codegen unexpectedly; revert + isolate
- atlas_pack v2 (pathfind) breaks → dispatcher in image_load broken; check that meta.frameTags absence falls through to atlas_pack path
- Aseprite sidecar parse fails → JSON shape mismatch; cross-reference with real Aseprite export
- Walk frames don't cycle → elapsed_ms not accumulating; check movement tick path
- Wrong facing → wc1_facing_for_delta math; should bucket into 8 octants atan2-style
- Mirror moonwalk → flip flag inverted OR sprite x position needs adjustment (flip mirrors around sprite center which assumes top-left origin)

---

### Commit 5 — Modulab abre Aseprite sheets (read + display)

**Por quê:** sem Modulab abrindo o formato Aseprite, o pivot fica hollow — "companion in-context" exige que Modulab pelo menos VEJA o que Aseprite produziu. Este commit dá Modulab a capacidade de ABRIR uma Aseprite sheet, mostrar grid de frames, listar animations, e tocar preview. Edição de pixels vem no Commit 6.

**Pré-requisitos:**
- Commit 4 shipado (stbimage parser de Aseprite + `image_anim_id`/`image_anim_frame` disponíveis)
- WC1 peasant.json existe em formato Aseprite

**`tools/modulab/io.bsm` updates:**

1. **File dispatcher** — `mlab_load_any(path)` (or equivalent open path) detecta Aseprite shape:
   - Parse JSON, check for `meta.frameTags` presence
   - Se presente → dispatch pra `mlab_load_aseprite_sheet(path)` (novo)
   - Se ausente → existing path (atlas_pack, single sprite)

2. **`mlab_load_aseprite_sheet(path)` (NEW):**
   - Parse JSON: frames object, meta.image, meta.size, meta.frameTags
   - Resolve meta.image relative to sidecar dir
   - Load PNG via `pixels_load` (RGBA buffer)
   - Store parsed state em globals novos:
     - `g_aseprite_active` (1 quando uma sheet está carregada)
     - `g_aseprite_png_rgba` (ptr to RGBA buffer)
     - `g_aseprite_png_w`, `g_aseprite_png_h` (PNG dimensions)
     - `g_aseprite_frame_x[]`, `g_aseprite_frame_y[]`, `g_aseprite_frame_w[]`, `g_aseprite_frame_h[]` (per-frame bounding boxes)
     - `g_aseprite_frame_duration[]` (per-frame ms)
     - `g_aseprite_frame_count` (total frames)
     - `g_aseprite_tag_name[]`, `g_aseprite_tag_from[]`, `g_aseprite_tag_to[]` (animation tags)
     - `g_aseprite_tag_count`
     - `g_aseprite_active_frame` (currently-selected frame index, default 0)
     - `g_aseprite_active_tag` (currently-playing animation, default -1 = none)
     - `g_aseprite_preview_elapsed_ms` (animation preview timer)

3. **State free on close** — `mlab_close_aseprite_sheet()` libera o PNG buffer + arrays paralelos. Chamado quando user opens different file.

**`tools/modulab/canvas.bsm` ou novo `tools/modulab/aseprite_view.bsm` updates:**

4. **Sheet display panel** — substitui o canvas tradicional QUANDO `g_aseprite_active == 1`:
   - Renderiza PNG inteiro (scaled to fit panel) via `pixels` blit
   - Grid overlay desenhando border de cada frame usando `g_aseprite_frame_*` arrays
   - Highlight do `g_aseprite_active_frame` (border colored)
   - Tag boundaries visualmente (cor diferente nos frames que pertencem ao active tag)

5. **Frame picker click** — mouse click no PNG mapeia coord → frame index via bbox check:
   - Walk frame_* arrays, find frame containing click position
   - Update `g_aseprite_active_frame`
   - If active frame está dentro de algum tag range, update `g_aseprite_active_tag` pra esse tag (visualiza qual anim contém aquele frame)

6. **Animation list panel** — sidebar listando todas as tags:
   - Cada entrada: `<tag_name>` + `(<from>..<to>, <count> frames)`
   - Click → set `g_aseprite_active_tag = idx`, reset `preview_elapsed_ms = 0`
   - Active tag highlighted

7. **Animation preview** — quando `g_aseprite_active_tag >= 0`:
   - Each frame: increment `preview_elapsed_ms` by dt
   - Compute current frame in cycle: `tag.from + (preview_elapsed_ms / frame_duration) % (tag.to - tag.from + 1)`
   - Renderiza esse frame em painel dedicado (canvas-sized scaled view, separate do sheet display)
   - Loop automático

**`tools/modulab/ui.bsm` updates:**

8. **Top bar buttons** quando Aseprite sheet carregada:
   - "Sheet view" toggle (vs single-frame edit view — placeholder pra Commit 6)
   - "Play preview" / "Pause preview" — toggle animation playback
   - "Save" disabled em read-only mode (Commit 6 ativa)

**Hot-reload:** quando peasant.png muda externamente (Aseprite externa salvou), `file_watch_tick` reload pickle:
   - Re-load PNG bytes via pixels_load
   - Resparsear sidecar (caso frameTags mudaram)
   - Refresh display

**Verification:**

```bash
./bpp tools/modulab/modulab.bpp -o /tmp/m
/tmp/m
# File → Open → games/rts1/assets/sprites/wc1/peasant.json
# - PNG renderiza com grid de frames sobre ele
# - Sidebar mostra 10 animations (idle_N..S, walk_N..S)
# - Click "walk_S" → animation preview anima na sub-panel
# - Click frame 25 no sheet → active_frame highlights, walk_S tag fica destacada
# - Edit peasant.png externamente (Aseprite ou GIMP) → Modulab reflete em ~30 ms
# - File → Open → games/pathfind/assets/sprites/pathfind.json
# - Pathfind atlas_pack ABRE normal (path antigo não quebra)
```

**Stop conditions:**
- PNG load via meta.image falha → check relative path resolution (vs JSON file dir)
- Grid overlay misaligned → frame bbox math wrong; cross-reference com Aseprite preview
- Pathfind quebra → file dispatcher broke; verify meta.frameTags presence check é narrow
- Animation preview cycles wrong → off-by-one no phase math; mesmo bug class do stbimage Commit 4

**Out of scope deste commit:**
- Pixel edit no canvas — vai pro Commit 6
- Salvar de volta no PNG — vai pro Commit 6
- Tag edit (criar/renomear/mudar from/to) — sidequest futura
- Multi-PNG sheets (Aseprite supports multiple sheets per JSON) — assumir single sheet v1

---

### Commit 6 — Modulab slice-edit Aseprite sheets

**Por quê:** completa o pivot. User edita pixel art de frame específico no canvas do Modulab, save escreve de volta no PNG na região correta, hot-reload propaga pro WC1 rodando. User goal "brincar com mod de WC1" fica achievable end-to-end.

**Pré-requisitos:**
- Commit 5 shipado (Modulab abre + mostra Aseprite sheets)
- `pixels_save_png(path, rgba, w, h)` disponível em stbimage (verified — line 1069)

**`tools/modulab/aseprite_view.bsm` ou `canvas.bsm` updates:**

1. **Frame → edit canvas** — quando user faz double-click num frame (ou clica "Edit" button):
   - Extract frame region from `g_aseprite_png_rgba` usando bbox do active_frame
   - Copia pixels pra canvas buffer (`g_frame_data` ou similar dependendo da convenção atual de Modulab)
   - Set `g_canvas_n` = frame width (assume square frames v1; non-square é edge case raro)
   - Switch UI pra "frame edit mode" (canvas visível em vez de sheet view)
   - Track `g_aseprite_edit_frame` (qual frame está sendo editado)

2. **Palette handling** — RGBA frame pode ter qualquer combinação de cores. Modulab canvas usa palette-indexed. Duas opções:
   - **(a) Quantize on load** — walk frame pixels, build palette de cores únicas (up to 256), convert pixels para indices. Edit normal. On save, palette → RGBA.
   - **(b) Bypass palette** — frame edit mode usa RGBA buffer direto, paint tool sample/pick a partir do RGBA. Skip palette mechanic.
   - **Recomendação: (a)** — preserva o workflow Modulab existente, palette dedup é trivial (~30 LOC).

3. **Edit canvas usa tools existentes** — paint pixel, fill, eyedropper, undo já funcionam no `canvas.bsm`. Não precisa mudar.

4. **Save back to PNG** — quando user faz Cmd+S no frame edit mode:
   - Convert canvas data back to RGBA usando palette
   - Blit RGBA region (frame.w × frame.h pixels) de volta em `g_aseprite_png_rgba` no offset `(frame.x, frame.y)` por scanline
   - Call `pixels_save_png(g_aseprite_png_path, g_aseprite_png_rgba, g_aseprite_png_w, g_aseprite_png_h)` — escreve PNG inteiro com a região modificada
   - Set `g_dirty = 0` (sheet limpa)
   - Stay em frame edit mode (don't auto-close)

5. **Switch back to sheet view** — "Back to sheet" button (ou ESC):
   - Discard or save (prompt if dirty)
   - Set `g_aseprite_edit_frame = -1`
   - Reload sheet display

6. **Dirty tracking per-frame** — track quais frames foram modificados antes do save:
   - `g_aseprite_dirty_frames[]` — array de frame indices que foram edited mas not yet saved
   - On save, walk dirty list, blit each back to PNG, then clear list
   - Sheet view marca dirty frames visualmente (border yellow ou similar)

7. **Hot-reload coordination** — quando user salva no Modulab, WC1 game (running em outro process) faz `file_watch_tick` e vê peasant.png mudou. stbimage re-uploads textura. Frame seguinte mostra novo pixel art.
   - Risk: race entre Modulab save + game read. Aceitar (sistema operacional escreve PNG atomicamente quando tamanho não muda).
   - Test: edit peasant_walk_n_f0 → save → game (com peasant andando N) deve mostrar walk frame 0 modificado em <100ms.

**`tools/modulab/ui.bsm` updates:**

8. **Mode toggle buttons:**
   - "Sheet view" / "Edit frame" — toggle entre os dois modos
   - "Save" enabled em edit mode (era disabled no Commit 5)
   - Dirty indicator no título (`* peasant.json` quando há frames editados não salvos)

9. **Frame navigation em edit mode:**
   - "Previous frame" / "Next frame" arrows — navega entre frames sem voltar pra sheet view
   - Prompt save se mudar de frame com dirty changes (or auto-save current before switching)

**Verification:**

```bash
# 1. Build Modulab + WC1
./bpp tools/modulab/modulab.bpp -o /tmp/m
./bpp games/rts1/rts.bpp -o games/rts1/rts

# 2. Start WC1 game (window 1)
./games/rts1/rts &
# - Select peasant, send walking → walk frames cycle

# 3. Start Modulab (window 2)
/tmp/m &
# - File → Open → games/rts1/assets/sprites/wc1/peasant.json
# - Sheet view, see grid
# - Double-click frame walk_S_f0 (frame 25) → edit canvas opens with that frame's pixels
# - Paint a few pixels with a distinct color
# - Cmd+S → "* peasant.json" loses asterisk, file_watch_tick fires no game
# - Switch to game window: peasant in walk_S anim now shows modified pixels mid-cycle
# - Back in Modulab: Back to sheet → see edited frame (no longer dirty)

# 4. Edit second frame, verify isolated
# - Double-click walk_N_f2, paint different color
# - Save → game peasant when walking north shows modified mid-cycle

# 5. Hot-reload from external editor still works
# - Open peasant.png in GIMP/Aseprite, modify outside Modulab's edit range
# - Save in external tool → game picks up + Modulab sheet view refreshes

# 6. Bootstrap verification
./bpp src/bpp.bpp -o /tmp/bpp_gen1
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2) | head    # empty
bash tests/run_all.sh
bash tests/run_all_c.sh
```

**Stop conditions:**
- Palette quantization perde cores → cap em 256, error se source frame tem mais (provavelmente n/a for WC1)
- Save corrompe PNG → pixels_save_png integration bug; verify byte layout matches loader
- Edit canvas mostra frame errado → bbox extraction off-by-one; check x/y vs row/col
- Hot-reload trigger no game não fire → file_watch path mismatch (Modulab salva no PATH que game está watching)
- Dirty tracking leaks → mode switch sem save perde edits silenciosamente; explicit prompt antes
- Race entre Modulab save + game read corrupting frame → improvável (atomic write OS-level) mas se acontecer, escrever em temp file + rename

**Out of scope (próximas sidequests):**
- Edit Aseprite tags (criar nova anim, renomear, mudar from/to ranges) — separate UI sidequest
- Aseprite layers support (Aseprite sprites podem ter layers; flatten on import v1)
- Onion-skinning entre frames de um anim cycle — animator-only feature, defer
- Importing pure .aseprite binary files — schema parsing big work, kept JSON-export only
- Multi-PNG sheets per JSON (Aseprite supports) — single sheet assumption v1

---

## Order matters

```
Commit 1 (movement)        — pode rodar SEM os demais
Commit 2 (convention)      — independente, precede 3+
Commit 3 (convert + asset) — depende de 2
Commit 4 (parser + S5)     — depende de 3
Commit 5 (Modulab view)    — depende de 3 (sidecar exists) + 4 (parser shape consistent)
Commit 6 (Modulab edit)    — depende de 5
```

Cada commit bisect-friendly. Commits 5-6 podem ser deferred sem comprometer o gameplay (game já funciona end-to-end depois do Commit 4); são "Modulab capability" que fecha o pivot.

---

## Decisões fechadas (não re-discutir mid-execution)

- **Schema = Aseprite-export literal**. Não bespoke variant. `frames` é object keyed por string, frameTags carry from/to/direction.
- **`type` field bespoke NÃO adicionado** ao sidecar — pure Aseprite compatibility. Detection via `meta.frameTags` presence.
- **Direction "forward" only v1**. Reverse/pingpong parsed mas executados como forward até game pedir.
- **Variable per-frame duration suportado pelo loader**, mas converter emite uniforme. Aseprite-authored sheets podem ter variable rate.
- **Anim.state como const enum** (UNIT_STATE_IDLE = 0, UNIT_STATE_WALK = 1) — cheaper que packed string compare per frame.
- **PNG copy via inline file read+write** no converter, não new file_copy primitive.
- **Walk frame reset on facing change** — elapsed_ms volta pra 0 pra começar cycle fresh.
- **atlas_pack permanece** pra pathfind e qualquer game que autora per-sprite no Modulab sem anim.
- **atlas_grid marca-se deprecated-for-new** mas back-compat mantido (não quebra games existentes).
- **Modulab slice-edit via palette quantize** — frame RGBA carregada vira indexed (até 256 cores únicas) pro canvas paint existente funcionar; save converte de volta. Bypass RGBA-direct rejeitado pra preservar canvas tools existentes.
- **Save back via `pixels_save_png` whole-file rewrite**, sub-region blit em memória + escrita atômica do PNG inteiro. Não chunked PNG patching (que exigiria deflate stream manipulation, infraestrutura grande).
- **Modulab edits SHEET PNG diretamente** — não cria intermediate sprite files. Source of truth = a PNG mesmo, sidecar JSON só metadata. Aseprite + Modulab + qualquer PNG editor convergem na mesma source.
- **Read+display (Commit 5) e edit (Commit 6) em commits separados** — read+display sozinho já fecha o pivot pra "Modulab vê o que Aseprite produz"; edit fecha pra "Modulab tweak in-context". Bisect-friendly se algum bug no edit aparecer.

---

## Out of scope (sidequests futuros, todos Rule 28)

- **Aseprite tag editing UI** — criar nova animation, renomear, mudar from/to ranges no Modulab. Commit 6 deixa frames editáveis mas tags read-only. Sidequest separada quando autoria de novas anims dentro do Modulab demandar.
- **Aseprite layers / onion-skinning** — Aseprite suporta multi-layer sprites + onion display entre frames de um cycle. Animator-focused, deferred até alguém usar.
- **`.aseprite` binary parser** — Aseprite tem source code (MIT until 1.3, paid after); parsing binary é big work. Consumimos só o JSON export ("File > Export Sprite Sheet > JSON Hash > Frame Tags ON"). Modulab edits via PNG round-trip, não binary.
- **Multi-PNG per JSON** — Aseprite supports multiple sheets per JSON. V1 assume single sheet.
- **Aseprite direction variants (reverse, pingpong)** — schema bump quando algum jogo pedir
- **WC1 walk frames row 6-12 (chop / die / carry)** — converter já emite frameTags genéricos pra elas, mas anims dedicadas (`chop_N` etc) entram em S7+ (combat) e S9 (resources)
- **WC1 footman / archer / cleric / conjurer / knight / catapult sprites** — wc1_sprite_convert é generalizável; rodar 1× por unit conforme war1tool emite
- **Modulab full Aseprite parity** — explicitly out of scope agora. Modulab é IDE-companion, não Aseprite-replacement
- **`.aseprite` binary parser** — Aseprite has source code (MIT until 1.3, paid after); parsing binary é big work. Nós consumimos só o JSON export ("File > Export Sprite Sheet > JSON Hash > Frame Tags ON")
- **9-patch slices** (Aseprite supports `meta.slices`) — when a game asks
- **Direction-aware Aseprite tag metadata** — quando algum jogo precisar of tag → facing mapping convention explícito (vs WC1's hardcoded "walk_<facing>" naming convention)
- **Multi-select drag-rectangle marquee** — S5 ainda single-select
- **Edge-of-screen mouse pan** — stbcamera ganha `cam_pan_edge` quando RTS demand justificar

---

## Lessons embedded (Tonify-style)

1. **Asset format decisions live in docs/asset_formats.md** — antes de extend qualquer formato, ler a spec atual + Rule 31. Game-side hardcode de animation metadata é red flag.
2. **`.json` single-suffix is the convention** — qualquer novo file tipo `*.something.json` viola convention; usar folder context pra desambiguar tipo.
3. **Industry compatibility é decisão estratégica explícita** — não default mute. Quando adopting (Aseprite), document the WHY + memory update. Quando rejecting, idem.
4. **Smart-dispatch em `image_load` sniffs JSON content fields, não extension** — rename de arquivo é cosmético; loader não muda.
5. **Silent scope deviation kills bisect** — se um commit vai shipar menos que o canonical scope, flag explícito (PARTIAL marker), nunca silenciar.
6. **One-shot converters > runtime parsers para foreign formats** — war1tool SMS, war1tool PNG → tools/ converter → canonical B++ JSON, mesma rail que map convert estabeleceu.
7. **Tool role clarity > tool feature parity** — Modulab fica AFIADO em IDE-integrated companion role, não tenta replicar Aseprite. "Aseprite pra trabalho heavy, Modulab pra tweaks rápidos in-context."
8. **Companion role exige TWO-WAY format support** — adoptar formato externo só vale se a tool interna também CONSOME ele. Se Modulab não abrisse Aseprite sheets, "companion in-context" seria slogan, não capability. Por isso Commits 5-6 são INTEGRAIS ao arc, não follow-up sidequest.
