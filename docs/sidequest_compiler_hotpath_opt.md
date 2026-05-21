# Sidequest — Compiler hot path optimization (3 layered stages)

**Status:** SHIPPED 2026-05-20 (commit pending). All four stages
landed (S1 + S2 + S3a + S3b) with profile sanity gate between
each. Bootstrap 0.51s → 0.37s = **~27% improvement** (well past
the doc's original 20-25% estimate). Suite 177/0/12 native +
141/0/48 C-emit, both byte-stable.

Final hot-list state vs baseline:

| Function | Baseline samples | After S3b | Change |
|---|---|---|---|
| `unpack_l` | 69 (20%) | <5 (out of top) | inlined as builtin |
| `_dsp_find_func_idx` | 27 (8%) | <5 (out of top) | replaced by hash lookup |
| `packed_eq` | 55 (16%) | 54 (15%) | CSE saved ~20% of body cost but the function itself stayed in the top |
| `unpack_s` | (not in top) | (not in top) | unchanged — call-frame overhead remains; queued for future polish |

Per stage:

- **S1** (unpack_l SDIV→LSR): bootstrap 0.51→0.50, ~2%
- **S2** (hash for _dsp_find_func_idx): bootstrap 0.50→0.42, ~17%
- **S3a** (CSE in packed_eq): 0.42→0.41, ~2%
- **S3b** (unpack_l as builtin): 0.41→0.37, ~10%

S3b was originally deferred as "ROI marginal" — that turned out
to be wrong. The function-call frame around `unpack_l` was where
most of its 20% lived; the LSR-vs-SDIV improvement in S1 only
shaved the body, not the prologue/epilogue. Lifting it into the
builtin lane (5-op inline sequence, mirror of the existing `shr`
builtin) eliminated the frame entirely. Lesson: when a hot
function's body is already trivial, the call frame is the
remaining cost — inlining via `cg_builtin_dispatch` is the right
next step, not parser-level rewrite.

**Original proposal text below, preserved as-is for the
historical record of how the audit landed.**

---

**Status (original):** PROPOSED 2026-05-20, ready-to-execute.
**Trigger:** Profile audit do bootstrap (`time ./bpp src/bpp.bpp -o /tmp/bpp_g1`) mostra que 3 funções
triviais consomem ~44% das amostras de CPU. Cada uma tem fix profile-justified com Rule 28 gate batido.

**Profile baseline** (348 samples / 0.48s bootstrap):

| Function | % samples | What it does |
|---|---|---|
| `unpack_l` | 20% | Extract length from packed string ref |
| `packed_eq` | 16% | Compare two packed strings by content |
| `_dsp_find_func_idx` | 8% | Linear scan func table by name |
| `arr_get` | 8% | Bounds-checked array element access |
| `malloc + _mem_free` | 12% | Allocation hot path |
| `arr_struct_at` | 3% | Recent migration (Rule 36b) |

**Os 3 primeiros (44% combined)** têm fix concreto, mechanical, baixo-risk. Esse documento é o plan de execução.

---

## Stage 1 — `unpack_l` signed division → unsigned shift (TRIVIAL, ship first)

### O bug latente

`src/bpp_defs.bsm:163-165`:

```b++
unpack_l(p) {
    return p / 0x100000000;
}
```

Comentário documenta: "Lower 32 bits = start offset in vbuf, upper 32 bits = length."

`p` é sempre **não-negativo** (vbuf offsets são positivos, lengths são positivas, soma fica < 2^33 em qualquer codebase realista). Logicamente `p / 2^32` = `p >> 32` = LSR.

**Mas B++ compila `/` como SDIV (signed div) sempre.** Documentação confirma — Phase D.1 strength reduction explicitamente NÃO reduce `/` de 2^k porque signed ASR vs integer divide diferem em negativos. O compiler emite literal SDIV.

### O custo concreto

| Operação | Ciclos ARM64 |
|---|---|
| SDIV (current) | ~12-20 cycles |
| LSR (target) | 1 cycle |
| Plus call overhead | ~8-10 cycles |

Per call atual: **~20-30 ciclos**.
Per call target (inline): **1-2 ciclos**.

Se `unpack_l` está em 20% do sample, isso é ~70K calls/s. Cada call gasta ~20-30 cycles where 1-2 would suffice. **Recoverable: ~80-95% do tempo dessa função.**

### Fix concreto

Migrar `pack`/`unpack_s`/`unpack_l` pra usar unsigned annotation:

```b++
// src/bpp_defs.bsm:151-165 — proposed:

// Pack a string reference into a single 64-bit value.
// Lower 32 bits = start offset in vbuf, upper 32 bits = length.
pack(s, l) {
    return s + (l * 0x100000000);    // shl by 32 — already optimal (D.1 covers x * 2^k)
}

// Extract the start offset from a packed string reference.
unpack_s(p) {
    return p & 0xFFFFFFFF;            // was `p % 0x100000000` — same result, single AND
}

// Extract the length from a packed string reference.
unpack_l(p) {
    auto pu: u_word;                  // unsigned cast for safe LSR semantics
    pu = p;
    return pu >> 32;                  // single LSR instead of SDIV
}
```

3 mudanças:

1. `unpack_l`: `/ 0x100000000` → `: u_word + >> 32` (LSR via signed-to-unsigned cast, Rule 32 family)
2. `unpack_s`: `% 0x100000000` → `& 0xFFFFFFFF` (single AND; same result, no branch)
3. `pack`: já é optimal via D.1 strength reduction (multiplication by 2^32 → shl)

### Verificação

```bash
# Disassemble unpack_l before:
./bpp src/bpp.bpp --dump-disasm 2>&1 | grep -A 3 "_unpack_l"
# Expect: sdiv ... 32-bit imm load + bl

# Apply fix, disasm after:
./bpp src/bpp.bpp --dump-disasm 2>&1 | grep -A 3 "_unpack_l"  
# Expect: lsr x0, x0, #32 ; ret

# Profile sanity:
time ./bpp src/bpp.bpp -o /tmp/bpp_g1   # 3 runs avg
# Compare with baseline 0.47-0.48s

# Bootstrap byte-stability mandatory:
diff <(md5 /tmp/bpp_g1) <(md5 /tmp/bpp_g2)
```

### Risk

**Baixo.** 3 funções primitivas, semantics preservadas (math equivalente pra non-negative inputs).

Edge case: `pack(s, l)` com `l = 2^31` ou maior NÃO existe em B++ — máximo length é vbuf size < 1MB. Não bate no bit 31.

### LOC

~10 LOC delta em 1 arquivo (`bpp_defs.bsm`).

### Expected impact

Conservador: bootstrap **~3-5% faster** (de 0.48s pra 0.45-0.46s).
Otimista: **8-10% faster** se unpack_l realmente é call-bound não compute-bound.

---

## Stage 2 — Hash for `_dsp_find_func_idx` (small, ship second)

### O TODO já documentado

`src/bpp_dispatch.bsm:760-775`:

```b++
// Find a function index by its packed name. Returns -1 if not found.
// Linear scan for now — only called by call_graph_build during AST walk.
// Future optimization: build a name→idx hash up front, like val_find_func.
static _dsp_find_func_idx(name_p) {
    auto i, rec, fname;
    for (i = 0; i < func_cnt; i++) {
        rec = funcs[i];
        fname = *(rec + FN_NAME);
        if (fname == name_p) { return i; }
        if (packed_eq(fname, name_p)) { return i; }
    }
    return -1;
}
```

**O comment literalmente diz "Future optimization: build a name→idx hash up front, like val_find_func."**

Profile mostra essa função em 8% do sample. Linear scan sobre ~thousands de functions × cada call site = quadratic em func_cnt.

### Fix concreto

B++ já tem `bpp_hash` no prelude (auto-injected). `bpp_validate.bsm` usa pattern similar (`val_find_func`). Just apply o mesmo aqui:

```b++
// src/bpp_dispatch.bsm — additions:

// Hash mapping packed name content → func_idx. Built lazily on
// first call after func_cnt stabilizes. Rebuilt on any func_cnt
// change (synth-fn additions invalidate it). Uses hash_str on
// the packed string's vbuf bytes for content-aware hashing.
static auto _dsp_func_hash;
static auto _dsp_hash_built_for_cnt;

static _dsp_func_hash_build() {
    auto i, fname, s, l;
    if (_dsp_func_hash != 0) { hash_free(_dsp_func_hash); }
    _dsp_func_hash = hash_new(func_cnt * 2);  // 2x load factor headroom
    for (i = 0; i < func_cnt; i++) {
        fname = *(funcs[i] + FN_NAME);
        s = unpack_s(fname);
        l = unpack_l(fname);
        hash_set_str(_dsp_func_hash, vbuf + s, l, i);
    }
    _dsp_hash_built_for_cnt = func_cnt;
}

static _dsp_find_func_idx(name_p) {
    auto s, l, idx;
    // Rebuild hash if func table grew since last build (handles
    // synth-fn workers added by smart-dispatch after initial build).
    if (_dsp_hash_built_for_cnt != func_cnt) { _dsp_func_hash_build(); }
    s = unpack_s(name_p);
    l = unpack_l(name_p);
    idx = hash_get_str(_dsp_func_hash, vbuf + s, l);
    if (idx >= 0) { return idx; }
    return -1;
}
```

### Risk

**Baixo-médio.** Synth-fn safety é a preocupação principal — smart-dispatch adiciona functions APÓS `call_graph_build`. O cache de `_dsp_hash_built_for_cnt` resolve detectando count mismatch.

Edge case: hash collisions. `bpp_hash` já lida via chaining — não regride correctness.

### LOC

~40 LOC delta em 1 arquivo (`bpp_dispatch.bsm`).

### Expected impact

8% CPU eliminado se hash O(1) substitui linear O(func_cnt). Bootstrap **~5-7% faster** absolute.

Combinado com Stage 1: **~10-15% bootstrap improvement** total.

---

## Stage 3 — Inline `packed_eq` + CSE in loop (medium, ship third)

### O problema combinado

`src/bpp_internal.bsm:75-86`:

```b++
packed_eq(a, b) {
    auto al, bl, i;
    al = unpack_l(a);                  // call 1
    bl = unpack_l(b);                  // call 2
    if (al != bl) { return 0; }
    i = 0;
    while (i < al) {
        if (peek(vbuf + unpack_s(a) + i) != peek(vbuf + unpack_s(b) + i)) { return 0; }
        //                ^ call per iter      ^ call per iter
        i++;
    }
    return 1;
}
```

Per call: 2× `unpack_l` + 2× `unpack_s` per iteration. For a 10-char name, that's 22 function calls.

### Fix concreto

Dois passes:

**Sub-stage 3a — CSE in loop (~5 LOC):**

```b++
packed_eq(a, b) {
    auto al, bl, i, sa, sb;
    al = unpack_l(a);
    bl = unpack_l(b);
    if (al != bl) { return 0; }
    sa = unpack_s(a);          // hoisted out of loop
    sb = unpack_s(b);          // hoisted out of loop
    i = 0;
    while (i < al) {
        if (peek(vbuf + sa + i) != peek(vbuf + sb + i)) { return 0; }
        i++;
    }
    return 1;
}
```

Saves 2 calls per iteration. For 10-char compare: 22 calls → 4 calls. **~5× speedup in packed_eq body.**

**Sub-stage 3b — Inline candidate (D.4 expansion, ~30 LOC):**

Expand `D.4 inline trivials` whitelist in `src/bpp_parser.bsm` to include:
- `unpack_s` (1 line: AND with mask)
- `unpack_l` (1 line: LSR by 32 after Stage 1 fix)
- `pack` (1 line: ADD + shift)
- `peek` (already builtin)

Each call site materializes the body inline. For `packed_eq`'s inner loop, the byte compare becomes:

```b++
// After inlining:
while (i < al) {
    if (peek(vbuf + (a & 0xFFFFFFFF) + i) != peek(vbuf + (b & 0xFFFFFFFF) + i)) { return 0; }
    i++;
}
```

Combined com CSE: `sa = a & 0xFFFFFFFF` once before loop, `peek` directly per iteration. Zero function calls per byte.

### Risk

**Médio.** D.4 expansion já existe (whitelist mechanism). Adding to whitelist = data change, not new mechanism. Bootstrap byte-stable é gate obrigatório.

Edge case: inlined sites might generate different code at struct/pointer arithmetic boundaries. Mitigation: smoke test on tests that exercise hot path heavily (test_smart_dispatch, test_bench).

### LOC

- Sub-stage 3a: ~5 LOC delta in 1 file
- Sub-stage 3b: ~30 LOC delta (whitelist addition + cost model tweak)
- Total: ~35 LOC

### Expected impact

`packed_eq` consome 16%. Saving 5× via CSE + inline = **~12-13% absolute bootstrap reduction.**

Combined com Stages 1+2: **~20-30% bootstrap improvement total.**

---

## Sequencing + verification protocol

### Order matters

```
C0 (baseline measurement, 5 min):
  3 runs `time ./bpp src/bpp.bpp -o /tmp/bpp_x` 
  Record real time average. Document as baseline.

C1 (Stage 1 - unpack_l shift, 10 LOC, 30 min):
  Apply fix to bpp_defs.bsm.
  Bootstrap byte-stable check (gen2==gen3 mandatory).
  Suite verde: 177/0/12 native.
  Profile sanity: 3 runs vs baseline. Expect 3-8% faster.

C2 (Stage 2 - func hash, 40 LOC, 1h):
  Apply hash to bpp_dispatch.bsm.
  Smart-dispatch safety: test_smart_dispatch + test_dispatch_maestro mandatory pass.
  Bootstrap byte-stable.
  Profile sanity: 3 runs. Expect cumulative 8-15% from baseline.

C3 (Stage 3a - CSE, 5 LOC, 15 min):
  Apply CSE to bpp_internal.bsm packed_eq.
  Bootstrap byte-stable.
  Suite verde.

C4 (Stage 3b - inline expansion, 30 LOC, 1.5h):
  Expand D.4 whitelist in bpp_parser.bsm.
  Bootstrap byte-stable check (CRITICAL — parser changes are sensitive).
  Suite verde.
  Profile sanity: 3 runs. Expect cumulative 20-30% from baseline.

Each commit bisect-friendly. Profile sanity gate >5% degradation → revert + investigate.
```

### Stop conditions

1. **Bootstrap diverges (gen2 ≠ gen3)** at any stage → revert that stage, investigate before continuing
2. **Suite regression** (test count drops, any new fail) → same
3. **Profile sanity shows regression** (slower than baseline) → revert + investigate. Inlining can sometimes cause i-cache pressure that reverses gains.
4. **Edge case fails** (synth-fn finds wrong idx, packed_eq returns wrong result) → check semantic preservation carefully

### Honest commit message template

```
compiler: hot path optimization stage <N>/4

What: <specific change>
Why: profile sample showed <X>% in <function>
How: <mechanism>

Profile delta:
- Before: <baseline_time>s
- After: <new_time>s  
- Improvement: <pct>%

Verification:
- Bootstrap fixed-point: gen2==gen3
- Suite: <N>/<F>/<S>
- Hot test smoke: test_smart_dispatch + bench_ecs_iter pass
```

---

## Out of scope (defer to Tier F roadmap)

These are deliberately NOT in this sidequest — they're bigger work without profile justification right now:

- **CSE general** (across statements, not just within function body) — Tier F.1, ~2-3 weeks
- **Register allocator v2** with liveness/SSA — Tier F.2, ~3-4 weeks
- **Auto-vectorization** — Tier F.3, ~2-3 months
- **Loop invariant code motion (LICM)** — needs CFG analysis
- **Tail call optimization** — useful for parser eventually, no current forcing function
- **General cost-model inlining beyond whitelist** — Tier F-adjacent, ~150-200 LOC

Stages 1-3 são **profile-justified, mechanical, low-risk**. Tier F é principled deferral.

---

## Expected total impact

| Metric | Baseline | After Stage 1 | After Stage 2 | After Stage 3 |
|---|---|---|---|---|
| Bootstrap time | 0.48s | 0.45s | 0.42s | 0.36-0.38s |
| Improvement | — | ~6% | ~12% | ~20-25% |
| LOC delta | — | ~10 | ~50 | ~85 |
| Risk | — | low | low-med | medium |

CI burn reduction (3-cycle bootstrap × 60+ CI runs/day) at 25% = ~20 minutes/day saved. Compounds over the project lifetime.

Dev loop reduction (compile-test cycle local) — same 25% applies to every recompile. Subjective but real.

---

## Decisions to confirm before agent executes

1. **Topa o sequencing C1→C4 com profile sanity gate entre cada?**
2. **OK em pular Stage 3b se Stage 3a já dá ganho suficiente?** (CSE-only sem inline expansion — mais conservador)
3. **Bench protocol — `time ./bpp src/bpp.bpp` 3 runs avg, ou algo mais robusto?** (hyperfine? not installed by default; manual time avg é OK)
4. **Tonify rule update** — registra Stages 1-3 como precedent na Rule 36b ou cria Rule nova de "compiler self-host optimization"?

Voto: sequence C1→C4 with sanity gate, ship 3b se 3a passa cleanly, manual time 3-run avg é suficiente, update Rule 36b com cluster table (já tem framework).
