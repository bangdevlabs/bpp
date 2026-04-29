# ELF Dynamic Linking — Implementation Plan

The Linux backend (`src/backend/target/x86_64_linux/x64_elf.bsm`) currently
emits **static ELF only**: 388 lines, no dynamic section, no PLT, no GOT, no
DT_NEEDED. Programs running on Linux interact with the kernel exclusively
through `sys_*` syscalls. This works for headless tests, file I/O, X11 over
the wire protocol, and the CPU framebuffer path. It does NOT work for
anything that lives in a shared library — pthread, OpenAL/PulseAudio,
Vulkan, DBus.

The macOS backend (`a64_macho.bsm`, 1462 lines) handles the equivalent via
Mach-O chained fixups + LC_LOAD_DYLIB. The split is from a 4× LOC gap that
this document closes.

This file is the **single canonical reference** for closing that gap. Read
it, follow the steps, ship.

## Goal of the first cut

One end-to-end test passing on Linux through Docker:

```bash
./bpp --linux64 tests/test_thread.bpp -o /tmp/t
docker run --rm -v /tmp:/tmp ubuntu:22.04 /tmp/t   # exit 0, "thread: OK"
```

That is the win condition. `test_thread` calls `_stb_thread_create` which
on macOS routes to `pthread_create`. On Linux today it is stubbed to
return 0 (failure). The test is currently `SKIP:phase1-macos-only` in
`tests/run_all.sh:54`. After dynlink ships, it runs.

## Reference shape — what gcc emits

A minimal `int main() { puts("hi"); return 0; }` cross-compiled to Linux
x86_64 produces 25 dynamic-section entries and 9 program headers. The
**B++ first cut** does not have to match this — many of those entries are
optional. Required for a working dynamic ELF:

```
Program headers
├── PT_PHDR     (loader walks this to find everything else)
├── PT_INTERP   (path to /lib64/ld-linux-x86-64.so.2 — 29 bytes)
├── PT_LOAD     (text + read-only — code, .interp, .dynsym, .dynstr, .hash, .rela.plt, .plt)
├── PT_LOAD     (writable — .got.plt, .data, .dynamic)
└── PT_DYNAMIC  (points at .dynamic table)

Dynamic section (DT_*)
├── DT_NEEDED   "libc.so.6"          — pthread_create lives here on glibc 2.34+
├── DT_STRTAB   (.dynstr address)
├── DT_SYMTAB   (.dynsym address)
├── DT_STRSZ    (.dynstr size in bytes)
├── DT_SYMENT   24                   — sizeof(Elf64_Sym)
├── DT_HASH     (.hash address)      — minimal 1-bucket hash works
├── DT_PLTGOT   (.got.plt address)
├── DT_PLTRELSZ (size of .rela.plt)
├── DT_PLTREL   DT_RELA (7)
├── DT_JMPREL   (.rela.plt address)
├── DT_FLAGS_1  DF_1_NOW (0x1)       — eager binding, skip lazy stubs
└── DT_NULL     0
```

**Symbol entries** (24 bytes each, struct Elf64_Sym):

```
struct {
    uint32_t st_name;    // offset into .dynstr
    uint8_t  st_info;    // (binding << 4) | type — STB_GLOBAL=1 << 4 | STT_FUNC=2
    uint8_t  st_other;   // 0
    uint16_t st_shndx;   // SHN_UNDEF=0 for imports
    uint64_t st_value;   // 0 for imports
    uint64_t st_size;    // 0 for imports
}
```

The first entry (index 0) is always all-zero by ELF convention. Imported
symbols start at index 1.

**Hash table** — minimal 1-bucket layout:

```
nbucket = 1, nchain = N+1                  // 8 bytes
bucket[0] = 1                              // 4 bytes — first symbol index
chain[0..N] = [0, 2, 3, ..., 0]            // 4 bytes per entry — chain[k]=k+1, last=0
```

ELF hash function (0 for all = all collide → all in 1 bucket → linear scan,
fine for 5 symbols).

**Relocations** (24 bytes each, struct Elf64_Rela):

```
struct {
    uint64_t r_offset;   // address of GOT entry to patch
    uint64_t r_info;     // (sym_index << 32) | R_X86_64_JUMP_SLOT (7)
    int64_t  r_addend;   // 0
}
```

**.plt entries** (16 bytes per imported symbol, eager binding):

```
ff 25 xx xx xx xx    jmp QWORD PTR [rip+disp32]     ; jump to GOT[i]
0f 1f 40 00          nop dword ptr [rax+0x0]         ; pad to 16
... 4 bytes pad ...
```

**.got.plt** (8 bytes per entry, plus 3 reserved):

```
got.plt[0] = 0   // address of .dynamic — kernel fills
got.plt[1] = 0   // link_map  — dynamic linker fills
got.plt[2] = 0   // _dl_runtime_resolve — dynamic linker fills (unused with eager binding)
got.plt[3..] = address of corresponding PLT[N+1]
                 // initially = "first PLT instruction after the jmp",
                 // dynamic linker overwrites with resolved address
```

With **eager binding** (DT_FLAGS_1 = DF_1_NOW), `got.plt[3..]` is filled
with the **resolved symbol address** before `main()` runs. The PLT stubs
just `jmp [got.plt[3+i]]` and that lands in the real function.

## Step-by-step implementation

### Step 1 — Track imported symbols at codegen time

`x64_codegen.bsm` currently emits direct `call rel32` for every function
call. For dynlinked symbols we need to call the PLT instead, with a
relocation that the ELF writer patches.

Add to `x64_enc.bsm`:

- A new relocation type, **type 4 = PLT call**.
- `x64_enc_pltcall(name_p)` — emits `call rel32` with reloc type 4 and
  the imported symbol name as the symbol.

Add to `x64_codegen.bsm`:

- A list `cg_imports` populated when emitting a call to a known FFI
  symbol (initially: pthread_create, pthread_join). Detection: the
  symbol is in a known `import "libc.B" { ... }` block.

The B++ source side: ensure `_core_linux.bsm` declares the imports.

### Step 2 — Add dynamic ELF emission to `x64_elf.bsm`

New section: **`write_elf_dyn(filename, main_label, gl_names, gl_count, imports)`**.

Layout pass:

```
hdr_off       = 0
phdr_off      = 64
interp_off    = 64 + nphdr * 56
dynsym_off    = align(interp_off + 29, 8)
dynstr_off    = dynsym_off + (nimport + 1) * 24
hash_off      = align(dynstr_off + dynstr_size, 8)
relaplt_off   = align(hash_off + (8 + nimport * 8), 8)
plt_off       = align(relaplt_off + nimport * 24, 16)
text_off      = align(plt_off + (nimport + 1) * 16, 16)
                                  // PLT[0] is reserved for lazy stub (16 bytes), unused with eager binding
text_end      = text_off + code_size
data_off      = align_page(text_end)
gotplt_off    = data_off
data_after_got = gotplt_off + (3 + nimport) * 8
flt_off       = data_after_got
str_off       = flt_off + nflt * 8
gl_off        = str_off + str_size
dyn_off       = gl_off + gl_count * 8
file_end      = dyn_off + (12 dyn entries) * 16
```

Virtual addresses follow file layout 1:1 with `ELF_BASE = 0x400000` for
text, `ELF_BASE + first_page_after_text` for data.

Emit each section in order. For `.dynstr`, intern names: position 0 is
empty string, then "libc.so.6\0pthread_create\0pthread_join\0...".

For `.hash`, generate the 1-bucket layout (nbucket=1, nchain=N+1).

For `.rela.plt`, emit one entry per import:
- `r_offset` = `gotplt_off + (3 + i) * 8`
- `r_info` = `((i + 1) << 32) | 7`
- `r_addend` = 0

For `.plt`, emit one stub per import (16 bytes):
- bytes `ff 25` then 4-byte disp32 to `got.plt[3+i]`
- bytes `0f 1f 40 00` (NOP padding) and 6 bytes more to total 16

For `.got.plt`, write 3 reserved zeros + initial values pointing at `plt_off + 16 + i*16` for each import (the dynamic linker will overwrite these).

For `.dynamic`, emit the 12 entries above, terminated by DT_NULL.

### Step 3 — Patch text relocations

After layout, walk the relocation list and patch:

- Type 0/1/2 (existing — global/string/float): unchanged behaviour.
- **Type 4 (new — PLT call)**: target = `plt_off + 16 + (sym_index * 16)`.
  Disp32 = `target - (rip + 4)`.

### Step 4 — Switch the entry point

The entry point goes to `_start` which sets up argc/argv/envp before
calling `main`. The current Linux backend ALREADY emits `_start` (see
`a64_macho.bsm` and `x64_elf.bsm` — `start_lbl`). With dynamic linking,
the dynamic linker resolves all PLT entries before jumping to entry, so
`_start` doesn't need to do anything new.

### Step 5 — Wire `_thread_spawn` on Linux

In `_core_linux.bsm`:

```bpp
import "libc.B" {
    int pthread_create(ptr, ptr, ptr, ptr);
    int pthread_join(long, ptr);
}

_thread_spawn(tid_buf, start_fn, arg) {
    return pthread_create(tid_buf, 0, start_fn, arg);
}

_thread_wait(tid) {
    return pthread_join(tid, 0);
}
```

The `import "libc.B"` block tells the codegen these are FFI imports.
On macOS, `libc.B` resolves to `libSystem.B.dylib`. On Linux, to `libc.so.6`.
Both are equivalent in the import-block convention.

### Step 6 — Detection: when to emit dynamic vs static

The cheapest detection: count `cg_imports` after codegen. If > 0, emit
dynamic. Else emit static.

`bpp.bpp` calls either `write_elf` (static) or `write_elf_dyn` (dynamic)
based on this. Both signatures stay backward compatible.

### Step 7 — Validation

```bash
# 1. Compile a thread test
./bpp --linux64 tests/test_thread.bpp -o /tmp/test_thread

# 2. Inspect the binary
readelf -d /tmp/test_thread       # should show NEEDED libc.so.6, all dynamic entries
readelf -l /tmp/test_thread       # should show PT_INTERP + PT_DYNAMIC
readelf -r /tmp/test_thread       # should show JUMP_SLOT relocations for pthread_*
ldd /tmp/test_thread              # should show "libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6"

# 3. Run in Docker
docker run --rm -v /tmp:/tmp ubuntu:22.04 /tmp/test_thread
# expected: "thread: OK", exit 0

# 4. Update tests/run_all.sh:54 to drop "phase1-macos-only" skip for thread/job/maestro
```

## What this does NOT cover (yet)

- **Lazy binding** (PLT[0] + _dl_runtime_resolve trampolines). Eager
  binding via DF_1_NOW is simpler, slightly slower at startup, fine
  for our purposes. Switch to lazy when startup cost of resolving 100+
  symbols becomes measurable.
- **GNU_HASH** instead of legacy ELF DT_HASH. GNU_HASH is faster but
  more complex (Bloom filter + hash). Stick with DT_HASH for the first
  cut; libc resolves either.
- **Vulkan / OpenAL / DBus / X11-via-libX11**. Same machinery applies
  — just register more imports. The first cut hardcodes `libc.so.6`
  with pthread_*. Adding `libvulkan.so.1` is a one-line `dyn_libs`
  addition + symbol declarations.
- **Pre-existing macOS pattern reuse**. Mach-O chained fixups don't
  port directly — different format. But the `mo_got_syms` /
  `mo_ext_libs` bookkeeping in `a64_macho.bsm:88-90` is the model for
  the Linux side's `cg_imports` / `cg_dyn_libs`.
- **Section headers**. Modern Linux loaders don't need `.shstrtab` /
  section header table for execution — only program headers matter.
  The reference gcc binary has them for `objdump -d`/`gdb` symbol
  display; skipping them is a minor disassembler ergonomics loss.
  Add them if needed for production debugging.

## Estimated effort

- Section emission: ~200 lines in `x64_elf.bsm`
- Codegen reloc type 4 + import tracking: ~100 lines across
  `x64_enc.bsm` + `x64_codegen.bsm`
- `_core_linux.bsm` import block: ~10 lines
- `tests/run_all.sh` skip removal: 1 line
- Docker test loop: existing
- Debug + trace through `readelf` / `ldd` until the binary loads
  cleanly: 2-4 hours

Total: a focused half-day session, including the inevitable byte-level
debugging that ELF demands.

## Why not now (this session)

This document was written at the end of an overnight refactor
(`bpp_mem.bsm` + `bpp_time.bsm` + `bpp_thread.bsm` + docs split + Option B
+ Linux cross-compile unblocked + C emitter restored). Plate is full;
ELF dynlink with cold tools at 7 a.m. is risk for risk's sake. Better in a
fresh session with `readelf` muscle memory primed.

The skeleton in `x64_elf.bsm` (just below this comment when it lands) is
the entry point — empty function with TODOs. Wire it up next time.
