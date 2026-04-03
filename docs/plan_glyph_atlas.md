# Plano: GPU Text via Glyph Atlas Texture

## Contexto

O `stbrender.bsm` renderiza rects, circles e lines via Metal mas não tem `render_text` nem `render_number`. O `snake_full.bpp` usa CPU rendering (`stbdraw`) porque precisa de texto pro HUD. O objetivo é adicionar texto GPU pra que jogos rodem 100% GPU.

A mesma infraestrutura de texture (upload, sampling, UV quads) será reutilizada depois pra sprites/textures (P0 do roadmap).

## Memórias relevantes

- `project_session_20260402.md` — contexto completo da sessão
- `project_gpu_philosophy.md` — GPU é nativo por plataforma, sem third-party APIs
- `feedback_codegen_discipline.md` — uma mudança por vez, bootstrap imediato
- `feedback_bpp_idioms.md` — usar buf[i], for loops, struct field sugar

## Arquitetura atual do GPU renderer

### Vertex format (8 bytes por vertex):
```
offset 0: int16 x
offset 2: int16 y
offset 4: uint8 r
offset 5: uint8 g
offset 6: uint8 b
offset 7: uint8 a
```

### Pipeline Metal:
- Vertex shader (`vert`): lê bytes do vertex buffer, converte pra NDC
- Fragment shader (`frag`): retorna `in.color` direto (sem textura)
- Vertex buffer: 256KB shared buffer, 6 vertices por rect (2 triangles)
- Uniform buffer 1: screen size (2 x int32)

### Arquivos:
- `stb/stbrender.bsm` — API pública (render_rect, render_circle, etc.)
- `stb/_stb_platform_macos.bsm` — Metal backend (init, shader, vertex write, present)
- `stb/stbfont.bsm` — 8x8 bitmap font data (768 bytes, poke'd at init)
- `stb/stbdraw.bsm` — CPU draw_text (referência da lógica de glyph rendering)

---

## O que precisa mudar

### 1. Shader — adicionar texture sampling (~20 linhas)

Arquivo: `stb/_stb_platform_macos.bsm`, função `_gpu_build_shader()` (linha 287)

**Vertex struct** — adicionar UV coords:
```metal
struct V {
    float4 position [[position]];
    float4 color;
    float2 uv;       // NOVO
    float  use_tex;   // NOVO: 0.0 = color only, 1.0 = sample texture
};
```

**Vertex shader** — ler UV do vertex buffer:
```metal
// Novo vertex format: 16 bytes por vertex (era 8)
// int16 x, int16 y, uint8 r,g,b,a, uint16 u, uint16 v, uint8 use_tex, uint8 pad
o.uv = float2(float(u) / 128.0, float(v) / 64.0);  // atlas é 128x64
o.use_tex = float(vd[b+12]) / 255.0;
```

**Fragment shader** — sample ou color:
```metal
fragment float4 frag(V in [[stage_in]], texture2d<float> tex [[texture(0)]]) {
    if (in.use_tex > 0.5) {
        float4 t = tex.sample(sampler(filter::nearest), in.uv);
        return float4(in.color.rgb, in.color.a * t.r);  // glyph alpha
    }
    return in.color;
}
```

### 2. Vertex format — expandir de 8 pra 16 bytes

Arquivo: `stb/_stb_platform_macos.bsm`, função `_stb_gpu_vertex()` (linha 492)

**Novo layout (16 bytes)**:
```
offset 0:  int16  x
offset 2:  int16  y
offset 4:  uint8  r, g, b, a
offset 8:  uint16 u (texel X em pixels)
offset 10: uint16 v (texel Y em pixels)
offset 12: uint8  use_tex (0 ou 255)
offset 13: uint8  pad (0)
offset 14: uint16 pad (0)
```

Mudar `_stb_gpu_vertex` pra aceitar u, v, use_tex:
```
_stb_gpu_vertex(x, y, r, g, b, a, u, v, tex_flag) {
    auto off;
    off = _gpu_vert_count * 16;    // era * 8
    write_u16(_gpu_vptr, off, x);
    write_u16(_gpu_vptr, off + 2, y);
    poke(_gpu_vptr + off + 4, r);
    poke(_gpu_vptr + off + 5, g);
    poke(_gpu_vptr + off + 6, b);
    poke(_gpu_vptr + off + 7, a);
    write_u16(_gpu_vptr, off + 8, u);
    write_u16(_gpu_vptr, off + 10, v);
    poke(_gpu_vptr + off + 12, tex_flag);
    poke(_gpu_vptr + off + 13, 0);
    write_u16(_gpu_vptr, off + 14, 0);
    _gpu_vert_count = _gpu_vert_count + 1;
    return 0;
}
```

**IMPORTANTE**: todos os callers de `_stb_gpu_vertex` precisam ser atualizados pra passar 9 args (u=0, v=0, tex=0 pra rects/circles sem textura). São estes:
- `_stb_gpu_rect` (linha 506) — 6 chamadas
- `render_line` em stbrender.bsm (linha 82-87) — 6 chamadas
- `render_circle` em stbrender.bsm (linha 98-104) — 3 chamadas

Todas passam `0, 0, 0` pros novos args.

### 3. Glyph atlas texture — criar e fazer upload

Arquivo: `stb/_stb_platform_macos.bsm`, dentro de `_stb_gpu_init()`

O font 8x8 tem 96 chars (ASCII 32-127). Layout da textura atlas: 16 chars por linha × 6 linhas = 128×48 pixels. Arredondar pra 128×64 (power of 2 friendly).

```
// Criar textura 128x64, formato R8Unorm (1 byte per pixel).
auto tex_desc, tex_data, cx, cy, ch, glyph, row, px, py;
tex_desc = objc_msgSend(objc_getClass("MTLTextureDescriptor"), sel_registerName("alloc"));
tex_desc = objc_msgSend(tex_desc, sel_registerName("init"));
objc_msgSend(tex_desc, sel_registerName("setTextureType:"), 2);  // MTLTextureType2D
objc_msgSend(tex_desc, sel_registerName("setPixelFormat:"), 10); // MTLPixelFormatR8Unorm
objc_msgSend(tex_desc, sel_registerName("setWidth:"), 128);
objc_msgSend(tex_desc, sel_registerName("setHeight:"), 64);

_gpu_font_tex = objc_msgSend(_gpu_device, sel_registerName("newTextureWithDescriptor:"), tex_desc);

// Rasterize font data into a pixel buffer.
tex_data = malloc(128 * 64);
// Clear to 0 (transparent).
for (i = 0; i < 128 * 64; i = i + 1) { poke(tex_data + i, 0); }

// Blit each glyph: char (ch-32) goes at grid position (ch%16, ch/16) × 8.
for (ch = 32; ch < 128; ch = ch + 1) {
    cx = ((ch - 32) % 16) * 8;
    cy = ((ch - 32) / 16) * 8;
    glyph = _stb_font + (ch - 32) * 8;
    for (py = 0; py < 8; py = py + 1) {
        row = peek(glyph + py);
        for (px = 0; px < 8; px = px + 1) {
            if (row & (128 >> px)) {
                poke(tex_data + (cy + py) * 128 + cx + px, 255);
            }
        }
    }
}

// Upload to GPU via replaceRegion (MTLRegion = {x,y,z, w,h,d} = 6 ints).
auto region;
region = malloc(48);
write_u64(region, 0, 0);      // origin.x
write_u64(region, 8, 0);      // origin.y
write_u64(region, 16, 0);     // origin.z
write_u64(region, 24, 128);   // size.width
write_u64(region, 32, 64);    // size.height
write_u64(region, 40, 1);     // size.depth

// [texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:128]
call(_gpu_msg, _gpu_font_tex,
     sel_registerName("replaceRegion:mipmapLevel:withBytes:bytesPerRow:"),
     region, 0, tex_data, 128);
// Nota: replaceRegion com MTLRegion é passado por valor como 6 palavras.
// Isso pode precisar de tratamento especial — testar.

free(tex_data);
free(region);
```

**NOTA IMPORTANTE**: `replaceRegion:` recebe MTLRegion BY VALUE, que são 6 campos de NSUInteger (8 bytes cada no ARM64 = 48 bytes). O `call()` do B++ passa args como words de 8 bytes. Pode funcionar direto ou pode precisar de wrapper. Testar empiricamente.

### 4. Bind texture no render pass

Arquivo: `stb/_stb_platform_macos.bsm`, dentro de `_stb_gpu_begin()` (linha 456)

Após `setRenderPipelineState:`, adicionar:
```
// Bind font texture for text rendering.
if (_gpu_font_tex != 0) {
    call(_gpu_msg, _gpu_enc, sel_registerName("setFragmentTexture:atIndex:"),
         _gpu_font_tex, 0);
}
```

### 5. render_text e render_number

Arquivo: `stb/stbrender.bsm`

```
// Draw text on GPU using the glyph atlas texture.
// Each character becomes a textured quad (6 vertices).
render_text(text, x, y, sz, color) {
    auto i, ch, r, g, b, a, cx, cy, u0, v0, u1, v1, dx, dy;
    r = _rcol_r(color); g = _rcol_g(color);
    b = _rcol_b(color); a = _rcol_a(color);
    i = 0;
    ch = str_peek(text, i);
    while (ch != 0) {
        if (ch >= 32) { if (ch < 128) {
            // UV coords in the 128x64 atlas: 16 chars per row, 8px each.
            u0 = ((ch - 32) % 16) * 8;
            v0 = ((ch - 32) / 16) * 8;
            u1 = u0 + 8;
            v1 = v0 + 8;
            dx = x + i * 8 * sz;
            dy = y;
            // 2 triangles = 6 textured vertices.
            _stb_gpu_vertex(dx,          dy,          r, g, b, a, u0, v0, 255);
            _stb_gpu_vertex(dx + 8 * sz, dy,          r, g, b, a, u1, v0, 255);
            _stb_gpu_vertex(dx,          dy + 8 * sz, r, g, b, a, u0, v1, 255);
            _stb_gpu_vertex(dx + 8 * sz, dy,          r, g, b, a, u1, v0, 255);
            _stb_gpu_vertex(dx + 8 * sz, dy + 8 * sz, r, g, b, a, u1, v1, 255);
            _stb_gpu_vertex(dx,          dy + 8 * sz, r, g, b, a, u0, v1, 255);
        } }
        i = i + 1;
        ch = str_peek(text, i);
    }
    return 0;
}

// Draw an integer number on GPU. Converts to string, then render_text.
render_number(x, y, sz, color, val) {
    auto buf, i, len, tmp, digit, j, t;
    buf = malloc(24);
    len = 0;
    if (val == 0) { poke(buf, 48); len = 1; }
    else {
        if (val < 0) { poke(buf, 45); len = 1; val = 0 - val; }
        tmp = val;
        while (tmp > 0) {
            digit = tmp % 10;
            poke(buf + len, 48 + digit);
            len = len + 1;
            tmp = tmp / 10;
        }
        // Reverse digits (skip minus sign if present).
        i = 0;
        if (peek(buf) == 45) { i = 1; }
        j = len - 1;
        while (i < j) {
            t = peek(buf + i);
            poke(buf + i, peek(buf + j));
            poke(buf + j, t);
            i = i + 1; j = j - 1;
        }
    }
    poke(buf + len, 0);
    render_text(buf, x, y, sz, color);
    free(buf);
    return 0;
}
```

### 6. Global: `_gpu_font_tex`

Arquivo: `stb/_stb_platform_macos.bsm`, linha 272

Adicionar entre os globals do Metal:
```
auto _gpu_font_tex;
```

---

## Ordem de execução

1. **Expandir vertex format** (16 bytes) — `_stb_gpu_vertex` + atualizar todos os callers
2. **Atualizar shader** — UV, use_tex, texture sampling
3. **Upload glyph atlas** — no `_stb_gpu_init`
4. **Bind texture** — no `_stb_gpu_begin`
5. **render_text + render_number** — no `stbrender.bsm`
6. **Testar** — programa simples que mostra texto GPU
7. **snake_full.bpp** — trocar pra GPU rendering

**NÃO precisa de bootstrap** — são mudanças em módulos stb, não no compilador.

## Verificação

```bpp
import "stbgame.bsm";
import "stbrender.bsm";

main() {
    game_init(320, 180, "GPU Text Test", 60);
    render_init();
    while (game_should_quit() == 0) {
        game_frame_begin();
        if (key_pressed(KEY_ESC)) { break; }
        render_begin();
        render_clear(DARKGRAY);
        render_text("Hello GPU!", 80, 60, 2, WHITE);
        render_number(80, 100, 2, YELLOW, 42);
        render_rect(10, 10, 50, 20, RED);
        render_end();
    }
    return 0;
}
```

Saída esperada: janela com fundo cinza, retângulo vermelho, texto "Hello GPU!" branco, "42" amarelo. Tudo via Metal.

## Riscos

1. **MTLRegion by value** — `replaceRegion:` recebe struct by value (48 bytes). B++ passa args como 8-byte words. Pode funcionar se o ABI trata structs pequenos como palavras, ou pode precisar de um wrapper C via FFI. Testar primeiro.

2. **Vertex stride** — mudar de 8 pra 16 bytes afeta o shader. O `vid * 8` no vertex shader precisa virar `vid * 16`. Esqueceu = tudo bugado.

3. **Texture binding** — se o fragment shader espera texture2d mas nenhuma textura tá bound pra quads sem textura (use_tex=0), pode crashar. O shader precisa do `if (in.use_tex > 0.5)` check.

4. **stbrender importa stbdraw** — linha 19: `import "stbdraw.bsm"`. Isso é necessário porque render_circle usa `_stb_font` (de stbdraw/stbfont). Não remover.
