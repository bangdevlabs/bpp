# Sidequest — Commit 7 fixup do Modulab Aseprite viewer/editor

**Status:** READY-TO-EXECUTE (escrito 2026-05-16).
**Trigger:** Audit pós-shipping de Commits 5-6 do arc WC1 Modulab pipeline. Funcionalmente o pivot está fechado (Modulab abre Aseprite sheet, edita frame, salva PNG, hot-reload propaga pro game), mas 4 issues identificáveis ficaram pra trás — 2 bugs concretos + 2 gaps do plano original.
**Predecessor:** `docs/sidequest_wc1_modulab_pipeline.md` (Commits 1-6 shipados em `3adccfb`..`9742457`).

## Sumário das 4 issues

| # | Tipo | Severidade | Fix LOC |
|---|---|---|---|
| 1 | Bug: single-click vira edit em frame 0 após load | Funcional crítico | ~4 |
| 2 | Bug: Close silencia dirty edits | UX perde trabalho | ~20 |
| 3 | Gap: sem smoke test pra Aseprite parsing | Risco contínuo | ~80 |
| 4 | Gap: dirty single-flag em vez de per-frame array | Plano não cumprido | ~30 |

Total ~130 LOC. Pode ser **single commit** ou split em 2 (bugs primeiro, gaps depois). Voto single — todos tocam `tools/modulab/aseprite_view.bsm` + 1 test file.

---

## Issue 1 — `_aseprite_last_click_frame` init bug

### Diagnóstico (verificado)

`tools/modulab/aseprite_view.bsm:769-770`:
```b++
static extrn _aseprite_last_click_frame;
static extrn _aseprite_last_click_ms;
```

Ambas globais. Default = 0.

`mlab_close_aseprite_sheet` (linha 175-187) reseta `_aseprite_active`, `_aseprite_edit_mode`, `_aseprite_dirty`, etc — **mas NÃO essas duas.**
`mlab_load_aseprite_sheet` (linha 343-352) reseta um monte de estado novo — **mas NÃO essas duas.**

Sequência que dispara:
1. Fresh Modulab launch: `_aseprite_last_click_frame = 0`, `_aseprite_last_click_ms = 0` (globais zerados)
2. User abre peasant.json → load sem tocar nessas duas
3. User clica frame 0 (idle_N, primeiro frame, entry-point natural):
   - `frame_id (0) == _aseprite_last_click_frame (0)` → true
   - `_aseprite_last_click_ms (0) <= 400` → true
   - **Entra edit mode em single click** em vez de exigir double

### Fix

Adicionar 2 linhas em CADA bloco de reset.

**`mlab_close_aseprite_sheet` (linha ~186, antes de `_aseprite_active = 0;`):**
```b++
_aseprite_last_click_frame    = -1;
_aseprite_last_click_ms       = 0;
```

**`mlab_load_aseprite_sheet` (linha ~352, antes de `_aseprite_active = 1;`):**
```b++
_aseprite_last_click_frame    = -1;
_aseprite_last_click_ms       = 0;
```

`-1` é sentinela: nenhum frame_id valid casa com -1, então o "frame_id == last_click_frame" só pode ser true depois de uma single click real registrar.

### Verification

- Abre peasant.json no Modulab
- Single click no frame 0 → highlight aparece, MAS edit mode NÃO ativa
- Double click no frame 0 (dois clicks dentro de 400 ms) → edit mode ativa
- Single click no frame 5, esperar 1 segundo, single click no frame 5 → NÃO entra edit (>400ms gap reseta o stash via tick)
- Double click no frame 5 → edit mode ativa

---

## Issue 2 — Close silencia dirty edits

### Diagnóstico (verificado)

`tools/modulab/aseprite_view.bsm:1022-1025` em `_render_edit_mode`:
```b++
if (gui_button(px + pw - 120, py + 4, 110, 22, "Close")) {
    mlab_close_aseprite_sheet();
    return;
}
```

Plano explicitamente flagrou:
> "Dirty tracking leaks → mode switch sem save perde edits silenciosamente; explicit prompt antes"

Sem prompt, sem auto-save, sem branch que olha `_aseprite_dirty`. Usuário edita, clica Close por engano (botão fica ao lado de "Back to sheet" no sidebar — mistura provável), perde trabalho.

Mesma vulnerabilidade na transição **edit → sheet** ("Back to sheet" button no sidebar, linha ~981 do editor):
```b++
if (gui_button(px + 8, y, pw - 16, btn_h, "Back to sheet")) {
    _aseprite_edit_mode = 0;
    consumed = 1;
}
```

E na transição **load novo sheet enquanto edit ativo** (user abre File → Open com edits pending).

### Pattern existente em Modulab

`tools/modulab/modulab_lib.bsm:654`:
```b++
ok = window_confirm("Switch universe?",
                    "Discard unsaved edits and switch?");
```

`window_confirm(title, message)` retorna 1 quando user OK, 0 quando cancel. Bloqueia até resposta.

### Fix

Helper `static` no topo do `aseprite_view.bsm` (ou inline em cada call site — mas factor pra reuso é mais limpo):

```b++
// Confirm discard of unsaved edits. Returns 1 if safe to discard
// (no dirty edits OR user confirmed discard); 0 if caller should
// abort the destructive action (close / mode-switch / load-other).
static _aseprite_confirm_discard(action_label: ptr) {
    auto title_sb, msg_sb, title, msg, ok;
    if (_aseprite_dirty == 0) { return 1; }
    title_sb = strbuf_new();
    title_sb = strbuf_cat(title_sb, "Discard unsaved edits?");
    title    = strbuf_cstr(title_sb);
    msg_sb   = strbuf_new();
    msg_sb   = strbuf_cat(msg_sb, "Sheet has unsaved pixel edits. ");
    msg_sb   = strbuf_cat(msg_sb, action_label);
    msg_sb   = strbuf_cat(msg_sb, " anyway?");
    msg      = strbuf_cstr(msg_sb);
    ok = window_confirm(title, msg);
    strbuf_free(title_sb);
    strbuf_free(msg_sb);
    return ok;
}
```

**Wrap 3 call sites:**

1. **Close button** (linha ~1022):
```b++
if (gui_button(px + pw - 120, py + 4, 110, 22, "Close")) {
    if (_aseprite_confirm_discard("Close sheet") != 0) {
        mlab_close_aseprite_sheet();
    }
    return;
}
```

2. **Back to sheet button** (linha ~981):
```b++
if (gui_button(px + 8, y, pw - 16, btn_h, "Back to sheet")) {
    if (_aseprite_confirm_discard("Discard frame edits and") != 0) {
        _aseprite_edit_mode = 0;
        // Clear dirty since we just threw the edits away by sheet logic
        // ...actually NO — dirty tracks at SHEET level. Back to sheet
        // doesn't reset dirty; only save does. So leave _aseprite_dirty.
        // The prompt is about leaving edit-mode-without-saving, but the
        // edits are still in the in-memory RGBA buffer. Sheet view
        // shows them (via dirty tint per Issue 4 fix).
    }
    consumed = 1;
}
```

   **Decisão importante**: edits no in-memory RGBA buffer persistem até a próxima save OU close. Voltar pro sheet view não joga eles fora — só remove o "actively editing this frame" mode. Se Issue 4 (per-frame dirty) entra junto, sheet view marca o frame como dirty visualmente.

   Então o prompt aqui é defensivo APENAS se o user achar que "Back to sheet" descarta. Vale colocar prompt mesmo assim porque o behavior pode confundir. **Alternative**: rename button pra "Done editing" e skip prompt. Voto: skip prompt, rename pra "Done editing" porque mais honesto. Confirmar com user da decisão.

3. **File → Open quando edit ativo** — verificar onde isso é dispatched. Provavelmente em `io.bsm` `mlab_open_dialog_from_path` ou `mlab_open_dialog`. Adicionar guard:
```b++
// Antes de fazer mlab_load_aseprite_sheet(new_path) ou
// mlab_load_canonical(new_path), check dirty do active sheet:
if (mlab_aseprite_active() != 0 && _aseprite_dirty != 0) {
    if (_aseprite_confirm_discard("Open new file") == 0) { return -1; }
}
```

Mas isso exige expor `_aseprite_dirty` ou criar accessor `mlab_aseprite_dirty()`. Cleaner: accessor.

### Verification

- Open peasant.json → enter edit on frame 25 → paint 10 pixels → Close button → **prompt aparece** "Discard unsaved edits? Close sheet anyway?"
- Click Cancel → still in editor with pixels
- Click OK → sheet closes, edits lost
- Same fluxo pra File → Open novo arquivo
- Cmd+S → save → Close → **sem prompt** (dirty era 0)

---

## Issue 3 — Sem smoke test automatizado

### Diagnóstico (verificado)

Plano Commit 4 disse:
> "`tests/test_atlas_aseprite.bpp` (NEW, ~50 LOC). Generates fixture sidecar JSON in /tmp pointing to a 2-frame test PNG (also generated)..."

E:
> "`./bpp examples/atlas_anim_smoke.bpp -o /tmp/aa_smoke; /tmp/aa_smoke  # all PASS lines, exit 0`"

**Nenhum dos dois arquivos existe.** Suite passa porque os 172 tests não cobrem o novo código de Aseprite parsing nem `image_anim_id` / `image_anim_frame`. Bootstrap byte-stable não pega bugs interativos.

### Fix — Criar `tests/test_atlas_aseprite.bpp`

Smoke test ~60-80 LOC. Localização padrão pra tests da suite. Geração de fixture em `/tmp`:

```b++
// test_atlas_aseprite.bpp — Aseprite sheet loader smoke test.
//
// Generates a fixture in /tmp: a 16×8 PNG (2 frames of 8×8) + JSON
// sidecar in Aseprite-export shape with 1 animation tag. Exercises
// image_load + image_anim_id + image_anim_frame end-to-end without
// touching any production asset.

import "stbimage.bsm";
import "bpp_io.bsm";
import "bpp_file.bsm";

main() {
    auto png_path: ptr, json_path: ptr;
    auto rgba, w, h, atlas, anim_id, sprite_id, expected;
    auto ok;
    ok = 1;

    png_path  = "/tmp/test_atlas_aseprite.png";
    json_path = "/tmp/test_atlas_aseprite.json";

    // 1. Generate fixture PNG: 16×8 RGBA buffer
    //    Frame 0 (cols 0..7): all red
    //    Frame 1 (cols 8..15): all green
    w = 16; h = 8;
    rgba = malloc(w * h * 4);
    auto x, y, off;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            off = (y * w + x) * 4;
            if (x < 8) {
                poke(rgba + off,     255);  // R
                poke(rgba + off + 1, 0);    // G
                poke(rgba + off + 2, 0);    // B
                poke(rgba + off + 3, 255);  // A
            } else {
                poke(rgba + off,     0);
                poke(rgba + off + 1, 255);
                poke(rgba + off + 2, 0);
                poke(rgba + off + 3, 255);
            }
        }
    }
    if (pixels_save_png(png_path, rgba, w, h) != 0) {
        put_err("FAIL: pixels_save_png\n"); return 1;
    }
    free(rgba);

    // 2. Generate fixture JSON sidecar in Aseprite shape
    auto sb;
    sb = strbuf_new();
    sb = strbuf_cat(sb, "{\"frames\":[");
    sb = strbuf_cat(sb, "{\"filename\":\"test 0\",\"frame\":{\"x\":0,\"y\":0,\"w\":8,\"h\":8},\"duration\":100},");
    sb = strbuf_cat(sb, "{\"filename\":\"test 1\",\"frame\":{\"x\":8,\"y\":0,\"w\":8,\"h\":8},\"duration\":100}");
    sb = strbuf_cat(sb, "],\"meta\":{\"image\":\"test_atlas_aseprite.png\",");
    sb = strbuf_cat(sb, "\"size\":{\"w\":16,\"h\":8},\"format\":\"RGBA8888\",");
    sb = strbuf_cat(sb, "\"frameTags\":[{\"name\":\"blink\",\"from\":0,\"to\":1,\"direction\":\"forward\"}]");
    sb = strbuf_cat(sb, "}}\n");
    if (file_write_all(json_path, sb, strbuf_len(sb)) != 0) {
        put_err("FAIL: file_write_all\n"); return 1;
    }
    strbuf_free(sb);

    // 3. Load via image_load + assert
    atlas = image_load(json_path);
    if (atlas == 0) { put_err("FAIL: image_load returned 0\n"); return 1; }

    // 4. image_anim_id("blink") returns >= 0
    anim_id = image_anim_id(atlas, "blink");
    if (anim_id < 0) { put_err("FAIL: anim_id < 0\n"); return 1; }
    put("PASS: image_anim_id\n");

    // 5. image_anim_id("nonexistent") returns -1
    if (image_anim_id(atlas, "nope") != -1) {
        put_err("FAIL: nonexistent anim_id != -1\n"); return 1;
    }
    put("PASS: image_anim_id nonexistent\n");

    // 6. image_anim_frame(atlas, anim_id, 0) returns frame 0
    sprite_id = image_anim_frame(atlas, anim_id, 0);
    if (sprite_id != 0) { put_err("FAIL: anim_frame(0) != 0\n"); return 1; }
    put("PASS: image_anim_frame at 0\n");

    // 7. image_anim_frame(atlas, anim_id, 100) returns frame 1
    sprite_id = image_anim_frame(atlas, anim_id, 100);
    if (sprite_id != 1) { put_err("FAIL: anim_frame(100) != 1\n"); return 1; }
    put("PASS: image_anim_frame at 100\n");

    // 8. image_anim_frame(atlas, anim_id, 200) wraps to frame 0
    sprite_id = image_anim_frame(atlas, anim_id, 200);
    if (sprite_id != 0) { put_err("FAIL: anim_frame(200) != 0 (cycle)\n"); return 1; }
    put("PASS: image_anim_frame cycle wrap\n");

    // 9. Cleanup
    image_free(atlas);
    sys_unlink(png_path);
    sys_unlink(json_path);

    put("OK\n");
    return 0;
}
```

### Notas de implementação

- Smoke usa `pixels_save_png` (já existe) pra gerar fixture real
- JSON shape literal Aseprite — exatamente o que real war1tool output produz
- Test self-contained — não depende de assets de games
- Cleanup com `sys_unlink` se existir; senão deixar lixo em /tmp (tolerável)
- Naming: `test_atlas_aseprite.bpp` casa convention dos outros tests

### Verification

```bash
./bpp tests/test_atlas_aseprite.bpp -o /tmp/t_aa
/tmp/t_aa
# Expected output:
#   PASS: image_anim_id
#   PASS: image_anim_id nonexistent
#   PASS: image_anim_frame at 0
#   PASS: image_anim_frame at 100
#   PASS: image_anim_frame cycle wrap
#   OK
# Exit 0

bash tests/run_all.sh
# Expected: 173/0/12 (was 172 + new test)
```

---

## Issue 4 — Dirty tracking per-frame em vez de single-flag

### Diagnóstico (verificado)

`_aseprite_dirty` é single int global em `aseprite_view.bsm`. Plano Commit 6 disse:

> "Dirty tracking per-frame — track quais frames foram modificados antes do save: `g_aseprite_dirty_frames[]` — array de frame indices que foram edited mas not yet saved. Sheet view marca dirty frames visualmente (border yellow ou similar)"

Implementação atual marca asterisco no topbar pro sheet inteiro. Funciona pra "tem edits pending" mas perde a granularidade pra "qual frame eu já mexi".

### Fix

**State substituído:**
```b++
// REMOVE: static extrn _aseprite_dirty;
// ADD:
static extrn _aseprite_dirty_frames;   // arr of frame indices with pending edits
```

**Inicialização (load):**
```b++
if (_aseprite_dirty_frames != 0) { arr_free(_aseprite_dirty_frames); }
_aseprite_dirty_frames = arr_new();
```

**Close:**
```b++
if (_aseprite_dirty_frames != 0) {
    arr_free(_aseprite_dirty_frames);
    _aseprite_dirty_frames = 0;
}
```

**Helper accessors:**
```b++
// Returns 1 if frame_id is in the dirty list.
static _aseprite_frame_is_dirty(frame_id) {
    auto i, n;
    n = arr_len(_aseprite_dirty_frames);
    for (i = 0; i < n; i++) {
        if (arr_get(_aseprite_dirty_frames, i) == frame_id) { return 1; }
    }
    return 0;
}

// Marks frame_id dirty (idempotent — no-op if already in list).
static void _aseprite_mark_frame_dirty(frame_id) {
    if (_aseprite_frame_is_dirty(frame_id) != 0) { return; }
    _aseprite_dirty_frames = arr_push(_aseprite_dirty_frames, frame_id);
}

// Returns 1 if ANY frame is dirty (replaces old _aseprite_dirty check).
mlab_aseprite_dirty() {
    return arr_len(_aseprite_dirty_frames) > 0 ? 1 : 0;
}
```

**Paint hook** (linha ~921 `_edit_paint_pixel`):
```b++
// Replace `_aseprite_dirty = 1;` with:
_aseprite_mark_frame_dirty(_aseprite_active_frame);
```

**Save hook** (linha ~843 `_aseprite_save_png`):
```b++
// Replace `_aseprite_dirty = 0;` with:
arr_free(_aseprite_dirty_frames);
_aseprite_dirty_frames = arr_new();
```

**Sheet view tint** — em `_draw_sheet_frame_grid` (ou onde frame borders são drawn no sheet view), adicionar:
```b++
// Per-frame dirty tint — yellow border quando frame tem edits pending
for (i = 0; i < _aseprite_frame_count; i++) {
    if (_aseprite_frame_is_dirty(i) != 0) {
        auto fx, fy, fw, fh;
        fx = arr_get(_aseprite_frame_x, i);
        fy = arr_get(_aseprite_frame_y, i);
        fw = arr_get(_aseprite_frame_w, i);
        fh = arr_get(_aseprite_frame_h, i);
        // Convert sheet coords to screen coords via _sheet_origin + _sheet_scale
        auto sx, sy, sw, sh;
        sx = _sheet_origin_x + fx * _sheet_scale;
        sy = _sheet_origin_y + fy * _sheet_scale;
        sw = fw * _sheet_scale;
        sh = fh * _sheet_scale;
        draw_border(sx, sy, sw, sh, rgba(255, 200, 80, 255), 2);
    }
}
```

(Ajustar `_sheet_origin_x/y` + `_sheet_scale` nomes conforme o que existe no código atual.)

**Topbar marker** (linha ~672 `aseprite_view_render`):
```b++
// Replace `if (_aseprite_dirty != 0)` with:
if (mlab_aseprite_dirty() != 0) {
    draw_text("*", px + 150, py + 8, 8, rgba(255, 180, 80, 255));
}
```

**Editor sidebar dirty hint** (linha ~995):
```b++
// Replace `if (_aseprite_dirty != 0)` with per-frame check:
if (_aseprite_frame_is_dirty(_aseprite_active_frame) != 0) {
    draw_text("[unsaved]", px + 8, y + 16, 8, rgba(255, 180, 80, 255));
}
```

### Verification

- Open peasant.json
- Double click frame 5 → edit
- Paint 1 pixel
- Click "Back to sheet" → frame 5 tem **yellow border** no sheet view (não outros)
- Topbar tem asterisco `*` antes do path
- Double click frame 10 → edit
- Paint 1 pixel → also marks dirty
- Back to sheet → frames 5 AND 10 yellow border
- Cmd+S no editor (de qualquer frame) → save → return to sheet → **ZERO yellow borders**, topbar asterisco sumiu

---

## Commit message proposto

```
Modulab Aseprite fixup: double-click init + dirty discard prompt + per-frame tracking + smoke

Closes 4 issues from audit of Commits 5-6 (`e5ece55` viewer + `9742457`
editor). The pivot was functionally complete but had 2 bugs and 2 plan
gaps. Bundling as single commit since all touch tools/modulab/
aseprite_view.bsm + 1 new test file.

Issue 1 (bug): _aseprite_last_click_frame defaulted to 0 globally and
neither load nor close reset it. First click on frame 0 immediately
entered edit mode in a single click because the stash claimed a match
against last_frame=0/last_ms=0. Initialize both to -1/0 on load AND
close.

Issue 2 (bug): Close button silently discarded unsaved pixel edits.
Added _aseprite_confirm_discard helper using existing
window_confirm() pattern from modulab_lib. Wraps Close button,
Back-to-sheet (rename pending decision), and File→Open transition
when edits pending.

Issue 3 (gap): No smoke test for Aseprite parsing existed; suite
passed because 172 tests don't exercise the new code path. Added
tests/test_atlas_aseprite.bpp generating /tmp fixture (PNG +
sidecar JSON in literal Aseprite shape) and asserting image_anim_id
+ image_anim_frame behavior including cycle wrap. Brings suite to
173/0/12.

Issue 4 (gap): Plan called for per-frame dirty tracking with
visual feedback in sheet view; implementation used a single global
flag. Replaced _aseprite_dirty with _aseprite_dirty_frames arr_*;
added _aseprite_mark_frame_dirty / _aseprite_frame_is_dirty
helpers; sheet view tints dirty frames with yellow border;
editor sidebar's [unsaved] indicator now per-frame.

Verification: bootstrap byte-stable, suite 173/0/12 native +
137/0/47 C, Modulab/rts/pathfind/platformer all build clean.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

---

## Decisões abertas pro user antes de commit

1. **"Back to sheet" button** — manter prompt OU rename pra "Done editing" e skip prompt? Edits ficam in-memory de qualquer jeito (só Cmd+S persiste). Voto rename, mais honesto. **User confirma.**

2. **Cleanup `sys_unlink`** — verificar se existe. Se não, smoke deixa /tmp lixo (aceitável) ou usa shell-like remove (não existe em B++). Voto deixar lixo em /tmp.

3. **Single commit ou split 2** — todos os 4 issues no mesmo commit OU bugs (1+2) primeiro, gaps (3+4) depois? Voto single — bem coeso, ~130 LOC, single file principal.

---

## Stop conditions

1. **Bootstrap não byte-stable** → revert, investigar. Mudanças são pure game-side (tools/modulab) + 1 novo test, não deveria afetar compiler.

2. **Suite regride** → verificar que test_atlas_aseprite passa standalone primeiro. Se falhar, debug isolado.

3. **`window_confirm` não existe ou tem signature diferente** → conferir `tools/modulab/modulab_lib.bsm:654` pra signature exata. Pode ter mudado.

4. **Aseprite viewer não abre depois do fixup** → bug introduzido. Bisect by reverting issues one-at-a-time.

---

## Out of scope (sidequests futuros)

- **Auto-save em mode transitions** — quando user troca de frame com dirty, opção de auto-save em vez de prompt. UX polish.
- **Undo/redo per-frame** — paint pixel → Cmd+Z reverte. Big work (~200 LOC), separate sidequest.
- **Multi-frame batch edit** — pintar mesma cor em N frames selecionados. Power-user feature.
- **Onion-skinning** — render adjacent frames semi-transparent atrás do active frame. Animator-only.
- **Aseprite tag editor** — criar/renomear/mudar from/to no Modulab (tags são read-only atualmente).

---

## Lessons embedded

1. **Init state on TWO sides — load AND close** — qualquer state que default-zero pode comprometer comportamento precisa init em load (start clean) E close (clean for next load). Issue 1 cabia no plano original mas não foi codificado.

2. **Suite passing != feature works** — bootstrap byte-stable + 172 tests verdes não validam código novo se nenhum test exercita o code path. Smoke test pra cada nova capability é leverage > opcional.

3. **Plan vs implementation gap audit** — pós-shipping audit lendo plan vs source code achou 2 gaps que o agente escrevendo o commit não pegou. Não é falha do agente, é padrão saudável: "plano disse X, código entregou Y, fixup fecha gap" como rotina.

4. **Confirm prompts são UX livre** — `window_confirm` já existia em Modulab. Não usar quando destrutivo é decisão ativa (que custou Issue 2). Pattern: qualquer botão que descarta state precisa confirm.
