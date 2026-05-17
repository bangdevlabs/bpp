# Sidequest — Aseprite frame edit deve usar canvas principal do Modulab

**Status:** PROPOSED (escrito 2026-05-17).
**Trigger:** User feedback após testar Modulab + peasant.json: "o que eu achei estranho é que tu clica no png e ele abre pra editar ali dentro do sprite viewer ao invés de levar o png pra tela principal do modulab, meio que estamos duplicando o canvas dentro do modulab".

## Diagnóstico

Commit 6 (`9742457` Modulab Aseprite editor) implementou um mini-editor inline dentro do aseprite_view.bsm: paint pixel + erase + eyedropper, sem palette, sem layers, sem undo. ~400 LOC de UI duplicada.

O modulab editor JÁ TEM toda essa infraestrutura matura:
- Tools: brush, eraser, fill, line, rect, oval, eyedrop, select
- Palette com swatches + customization
- Layers per-frame com blend modes
- Undo/redo (Z/Y plus Cmd+Z/Cmd+Shift+Z após commit `9005e7c`)
- History snapshots automáticos
- Frame timeline (ainda que não usado pra Aseprite frames)
- Selection + copy/paste

Duplicar isso no viewer foi escopo errado. O caminho correto é:

1. Double-click frame no viewer → extract RGBA → load no canvas principal do Modulab
2. Hide viewer (já feito automatic via mode switch)
3. User edita com TODAS as ferramentas que já tem
4. Save back to sheet: canvas → RGBA → blit no sheet RGBA → pixels_save_png

## Por que não foi feito assim originalmente

Plan Commit 6 (linha 590-593 do `docs/sidequest_wc1_modulab_pipeline.md`) listou as duas opções:

> "**(a) Quantize on load** — walk frame pixels, build palette de cores únicas (up to 256), convert pixels para indices. Edit normal. On save, palette → RGBA.
> **(b) Bypass palette** — frame edit mode usa RGBA buffer direto, paint tool sample/pick a partir do RGBA. Skip palette mechanic.
> **Recomendação: (a)** — preserva o workflow Modulab existente, palette dedup é trivial (~30 LOC)."

Mas eu shipei **(b)** porque "(a) precisaria de quantização e o palette Modulab é MCU-8 hardcoded com 8 cores". WC1 sprites têm muito mais que 8 cores, então a quantização destruiria os pixels.

Saída que eu não vi na hora: **palette custom por sheet**. Em vez de forçar MCU-8, build uma palette das cores únicas do frame que tá sendo editado (até 256 entries). Modulab já suporta palette de 256 entries por canvas (`g_pal_n` é variável), só precisa popular a partir do frame.

## Implementação proposta (~250-300 LOC)

### Bloco A — Aseprite → canvas (entry path)

Em `aseprite_view.bsm` `_aseprite_handle_double_click`:
```bpp
static void _aseprite_handle_double_click(frame_id) {
    if (frame_id == _aseprite_last_click_frame
     && _aseprite_last_click_ms <= 400) {
        // BEFORE: _aseprite_edit_mode = 1;
        // AFTER: hand off to main canvas
        _aseprite_load_frame_into_canvas(frame_id);
        return;
    }
    ...
}
```

New `_aseprite_load_frame_into_canvas(frame_id)`:
1. Resolve frame bbox via existing arrays
2. Build palette: walk frame pixels, dedup unique RGBA values, populate `g_palette[]` (clamp at 256)
3. Resize Modulab canvas to frame dimensions (32×32 for peasant): `state_resize(fw)` — need to verify helper exists OR refactor
4. Convert RGBA → palette indices, write into `g_frame_data` (or layer 0)
5. Hide viewer: `mlab_aseprite_view_hide()`
6. Mark Modulab dirty so save knows there are edits
7. Record source binding: which sheet + which frame → so save-back knows where to write

### Bloco B — Save back to sheet (exit path)

New action in `io.bsm` "Save to sheet" (or override S behavior when binding active):
1. Convert canvas palette indices → RGBA
2. Blit into `_aseprite_png_rgba` at the source frame's bbox
3. `pixels_save_png(_aseprite_json_path's sister PNG, full sheet)`
4. Clear Modulab dirty
5. Mark sheet's frame as dirty in `_aseprite_dirty_frames` (so sheet view shows it edited)

### Bloco C — UI surface

- Topbar gains "Save to sheet" button when binding active
- Cmd+S routes through binding-aware save (not generic `_do_save`)
- "Back to viewer" or Tab returns to sheet view
- If user navigates to different frame while bound, save? prompt? snapshot?

### Bloco D — Remove duplicate edit mode

After A+B+C work, delete `_render_edit_mode`, `_draw_edit_canvas`, `_draw_edit_sidebar`, `_edit_paint_pixel`, `_edit_eyedropper`, `_edit_pixel_at`, `_sheet_pixel_write` (only used by edit), `_aseprite_save_png` (replaced by binding-aware path). ~250 LOC removed.

Net code delta: ~zero or slight negative (replaces 400 LOC mini-editor with ~300 LOC binding layer + reuses existing editor).

## Wins

- **Free undo/redo**: Modulab's history_undo/redo already handles per-pixel + bulk via snapshot promotion. Cmd+Z / Cmd+Shift+Z já funciona via commit `9005e7c`.
- **Free tools**: line, rect, oval, fill, select, paste — não dá pra reimplementar isso no mini-editor.
- **Free layers**: pintar overlay sem destruir base, ajustar alpha, blend modes.
- **Consistent palette UX**: usuário troca cor com `[`/`]` igual qualquer canvas Modulab.
- **No duplicate code path to maintain**: bug fix em paint pixel só rola num lugar.

## Riscos / decisões abertas

1. **Quantização lossy**: se sheet tem 256+ cores, dedup vai cap. Default cap 256 é suficiente pra pixel art (WC1 sheets típicos têm 16-64 cores). Maior precision = palette por tier no save? Defer.

2. **Resize canvas**: Modulab assume canvas size global (`g_canvas_n`). Mudando ele em runtime pode quebrar UI assumptions. Pode precisar `state_resize` helper se não existe.

3. **Save path navigation**: usuário edita frame A, vai pra outro arquivo via File→Open. Atual dirty guard pega — mas precisa testar interação com novo binding.

4. **Source binding lifecycle**: o binding (sheet+frame_id) precisa ser limpo em:
   - User clicked "Done editing" / "Back to viewer"
   - Save to sheet (preserve? clear?)
   - mlab_close_aseprite_sheet
   - Loading a different file

5. **Multi-frame edit session**: workflow comum vai ser edit frame N → save → edit frame N+1 (next walk frame). Convém UX shortcut tipo "Next frame from sheet" que reload o canvas com frame N+1.

## Verification

- Open peasant.json → double-click frame 25 (walk_S f0) → Modulab editor aparece com pixel arte daquele frame, palette populada com cores únicas
- Pinta uns pixels com brush
- Cmd+Z → reverte
- Cmd+S → escreve de volta no peasant.png na região correta
- Re-abre peasant.json → frame 25 mostra os pixels editados
- WC1 game rodando em paralelo → hot-reload propaga após save

## Out of scope

- Multi-frame batch edit (selecionar N frames + pintar nos N de uma vez) — futuro
- Animação preview no canvas mode (mostrar o frame ciclando enquanto edita) — futuro
- Auto-save on frame navigation — futuro
- Layer support na save-back (canvas com layers → flatten antes de blit) — provavelmente ship com flatten implícito

## Lessons embedded

1. **Reuse over reinvent (Rule 31 + Rule 28)**: antes de construir paint tools inline, verificar se a infra já existe. Modulab editor já tem TUDO que o mini-editor tinha + muito mais.

2. **Plan diz uma coisa, eu escolhi outra silenciosamente**: o plan original recomendou (a) quantize, eu fui (b) RGBA-direct alegando palette mismatch. Não documentei a divergência no commit message. Deveria ter sinalizado decision point pro user antes de escolher.

3. **Mini-editor é red flag**: qualquer UI nova que duplica funcionalidade de UI existente merece pause + question. "Por que estou re-implementando isto?" → resposta deve ser muito mais forte que "porque é mais simples".
