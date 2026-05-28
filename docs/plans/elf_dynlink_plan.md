# ELF Dynamic Linking — the Linux x86_64 portability engine

**Status:** engine **BUILT + Docker-verified + DOOR OPEN 2026-05-28.** Both
parity gaps closed; the default `--linux64` path routes FFI externs and the
`DT_NEEDED` soname is resolved from the FFI library tag, so the engine links
libc today and `libvulkan`/`libasound`/`libGL` the moment a program imports
them. The GPU/audio calls themselves stay **stubbed** until the binary runs on
real x86 hardware. Threads are **not** part of this plan (they use `clone(2)`,
not dynlink). General technique: `docs/manual/nomad_manual.md`.

Commits: `af1e449` write_elf_dyn + `7f62b0b` detection + `340b5da` elf_code_off
fix + `c59a585` extern parity (gap #1) + `c391738` portable extern smoke +
`21d278e` soname-from-tag door (gap #2).

## Engine status — 2026-05-28 (COMPLETE)

**The mechanism is cemented.** `write_elf_dyn` emits a full dynamic ELF
(PT_PHDR/PT_INTERP/PT_LOAD×2/PT_DYNAMIC, .interp/.dynsym/.dynstr/.hash/
.rela.plt/.plt/.got.plt/.dynamic, eager `BIND_NOW`+`FLAGS_1=NOW`), and
`write_elf` routes programs with FFI-extern relocs to it. In Docker
(gcc:13/glibc 2.36): `readelf` shows the complete dynamic section, `ldd`
resolves the soname + `ld-linux`, and the **default `--linux64`** smokes run
correctly — `strlen("hi")`→exit 2, `abs(-5)`→exit 5 (the full chain: ld.so
binds the lib, SysV arg marshaling, RIP-relative string reloc, real call).
Two engine bugs found + fixed en route: the engine forgot to set the global
`elf_code_off` (rip-relative relocs mis-resolved — `strlen` first returned 0),
and the layout keeps globals/floats/strings first so `elf_resolve_relocations`
handles reloc types 0/1/2 unchanged.

**GAP #1 — FIXED (`c59a585`): default path routes FFI externs (x64).** x64's
T_CALL conflated externs and cross-module B++ functions under reloc type 4, and
`bo_resolve_calls_x64` resolves-and-removes type-4 relocs as function calls — so
an extern's reloc was deleted before the ELF writer saw it (only `--monolithic`
worked). Fix mirrors a64: T_CALL is 3-way (local fn → call label; FFI extern via
`cg_find_ext` → reloc **type 3**; cross-module fn → type 4), and
`emit_module_x86_64` refreshes `cg_ex_name` from the parser `externs` per module
(parity with `cg_bridge_data` + a64). `bo` only touches type 4, so type-3
externs survive to the writer. `write_elf` detection + the PLT patch key on
type 3. Verified: default `--linux64` `strlen`→2 / `abs`→5, static cross-module
`helper(4)`→5 still intact, gen1==gen2. Now covered by
`tests/test_extern_strlen.bpp` (runs on every host: libSystem on macOS, libc on
Linux).

**GAP #2 — FIXED (`21d278e`): the hardware door (per-library soname).** The
engine hardcoded `DT_NEEDED libc.so.6`. Now `elf_lib_soname` resolves the
soname from the program's FFI library tag (the import-block name carried as
`EX_LIB`): `System.B`→`libc.so.6`, `Vulkan.B`→`libvulkan.so.1`,
`ALSA.B`→`libasound.so.2`, `GL.B`→`libGL.so.1`, `X11.B`→`libX11.so.6`,
`Math.B`→`libm.so.6` (unknown → libc). The B++ runtime on Linux is
syscall-based, so a program imports exactly one FFI library and a single
`DT_NEEDED` suffices. **Verified in Docker:** `import "Vulkan.B" { … }` →
`readelf -d` shows `NEEDED libvulkan.so.1` + INTERP present; `import "System.B"`
still links + runs against libc. *This is the door:* no engine change is needed
to link a GPU/audio library — only the stubbed extern body that ships with the
hardware (see "Activation on x86 hardware").

**Remaining follow-ups (documented, not blocking):**
- **Two FFI libraries in one program.** The single-`DT_NEEDED` path assumes one
  FFI library per program (true for GPU-only/audio-only/libc-only). A program
  that mixes (e.g. Vulkan + a direct libc call) needs one `DT_NEEDED` per
  distinct soname — collect distinct sonames in `write_elf_dyn`, widen `.dynstr`
  + the DT entry count. Avoidable today (use syscalls instead of a 2nd FFI lib).
- **PT_NOTE on dynamic binaries.** The dynamic ELF skips the PT_NOTE (build-id +
  minisym), so `bug` can't symbolicate dynamic binaries yet (the static path
  has it) and `bug --bytes/--disasm` (which read PT_NOTE for code_base) don't
  work on them — add the note region to `write_elf_dyn`.

## What this is, and what it is not

B++'s Linux backend emits **static ELF only** — programs reach the kernel through
`sys_*` syscalls, with no `ld.so` and no `DT_NEEDED`. That is the right default
and it stays the default: everything syscall-reachable (compute, files, time, the
CPU framebuffer over the X11 wire protocol, **and threads via `clone(2)`**)
remains a zero-dependency static binary.

Dynamic linking exists for exactly one reason: **capabilities the OS exposes
*only* as a shared library** — the GPU (`libvulkan.so.1`) and audio
(`libasound.so.2`). There is no syscall for Vulkan; the only door to the GPU is
its driver's `.so`. So dynamic linking is **opt-in**, auto-triggered only when a
program reaches such a library — and it does not compromise the zero-dependency
identity, because a CPU/headless program never links anything. `ld.so` itself is
platform ABI, the same category as the syscall interface we already rely on.

**Threads are a sibling arc, not this one.** Threads are syscall-reachable via
raw `clone(2)` + `futex(2)` — no `ld.so`, no libc, no TLS — so they stay in the
static lane. B++'s job runtime fits cleanly: `@safe` workers (no malloc), the
parallel-for model is "main allocates → workers compute on disjoint slices →
main collects", and B++ uses its own `bpp_mem` (mmap), not glibc malloc, so there
is no concurrent glibc state and no errno/TLS in worker bodies. Building threads
belongs in its own `clone()`-based plan; do **not** route them through this
dynlink engine. (This corrects the 2026-04-29 draft, which made pthread the first
target — pthread drags in the entire C runtime; see "the libc-init reality".)

## The shape: engine now, door later

This is **Discipline #2** (stub the parity floor) applied to a binary-format
capability — "build the engine, test what hardware allows, stub the rest behind a
wired door, couple on arrival." Two phases:

- **Phase 1 — the engine (build + verify NOW, no hardware needed).** The ELF
  dynamic-emission machinery in `x64_elf.bsm` plus the reloc-driven
  static-vs-dynamic detection. **The engine is payload-agnostic**: the PLT/GOT
  machinery is byte-identical whether the `DT_NEEDED` is `libc`, `libvulkan`, or
  `libasound`. So we build it and verify it in Docker against a library that
  *exists* in the container (`libc.so.6`): link it, call a symbol, confirm the
  binary loads and binds under a real `ld.so`. That proves the engine for *every*
  future payload, and it answers the one real unknown (libc-init) cheaply.

- **Phase 2 — the door (stub NOW, couple when x86 hardware arrives).** The
  GPU/audio API surface + Linux platform hooks (`_gpu_linux.bsm` /
  `_audio_linux.bsm`, Tier-3 per the manual's Portability Tiers), wired to the
  engine, with the real driver `DT_NEEDED` + calls **stubbed** and marked
  `// ships when x86 hardware + driver present`. When the hardware lands,
  coupling is the small, known step in the Activation checklist — the engine
  underneath is already proven.

**End state of this arc:** engine done + Docker-verified, door wired + stubbed,
payload pending hardware. The motor is mounted and the entry door is linked; you
bolt on the driver when the hardware shows up.

## What's already done (the chip + spine side)

Most of the *call* side already exists — this arc is almost entirely the ELF
*writer*:

- **The extern model is shared spine.** `cg_ex_name`/`cg_ex_ret`/`cg_ex_args`/
  `cg_ex_acnt` live in `bpp_codegen.bsm`, populated from the parser's import
  records. **Reloc type 4 = "call this extern, patch me"** is emitted by *both*
  backends (a64 `a64_codegen.bsm:1353`; x64 `_x64_emit_call_extern` → `0xE8 call
  rel32` + a type-4 reloc carrying the symbol name). The Mach-O writer already
  consumes type-4 via `mo_add_got` to build its GOT + chained fixups; the ELF
  writer needs to consume the *same* relocs. No new shared abstraction is needed.
- **`_start` is already emitted** on both backends.

So Phase 1 is concentrated in one file: the ELF writer (`write_elf_dyn`) + the
detection predicate. The chip is ready.

## The libc-init reality (the one real unknown)

Dog-fooding the cousins (gcc 13 / glibc 2.36 in Docker, three modes) showed:
every gcc binary — even `-no-pie` — routes `_start → __libc_start_main → main`,
carries `INIT_ARRAY`, and references **versioned** symbols
(`pthread_create@GLIBC_2.34`). The reason is load-bearing: **glibc's runtime
(TCB, the `%fs` TLS base, the stack canary) must be initialized** before most
libc functions — including pthread — work. A hand-rolled `_start → main` that
calls such a function crashes.

This is why pthread is the *wrong* first payload (it forces "become a C-runtime
program") and why threads go to `clone()` instead. For the GPU/audio cut:
`libvulkan`/`libasound` are C libraries that transitively pull `libc.so.6`;
`ld.so` loads and *initializes* those libraries (runs their `INIT_ARRAY`, sets up
the static TLS block) before our `_start`. The open question is whether that is
enough for their entry points, or whether the *main thread* still needs
`__libc_start_main`-style setup. **Phase 1c settles this empirically in Docker**
(pure symbol vs stdio symbol) so Phase 2 has the answer in hand.

## Phase 1 — the engine

### 1a. `write_elf_dyn` in `x64_elf.bsm` (the bulk)

Replace the `write_elf_dyn` stub (`error: dynamic ELF emission not yet
implemented`) with real emission.

`write_elf_dyn(filename, main_label, gl_names, gl_count, imports)` — `imports` is
the list of *referenced* externs (those with a type-4 reloc), `imports ⊆
cg_ex_name`, built by scanning the reloc list for type-4 and collecting distinct
symbol names (also the detection signal — 1b).

Layout pass (file offsets; virtual addresses follow file layout 1:1, `ELF_BASE =
0x400000` for text, next page for data):

```
hdr_off       = 0
phdr_off      = 64
interp_off    = 64 + nphdr * 56
dynsym_off    = align(interp_off + 28, 8)     // "/lib64/ld-linux-x86-64.so.2\0" = 28 bytes
dynstr_off    = dynsym_off + (nimport + 1) * 24
hash_off      = align(dynstr_off + dynstr_size, 8)
relaplt_off   = align(hash_off + (8 + nimport * 8), 8)
plt_off       = align(relaplt_off + nimport * 24, 16)
text_off      = align(plt_off + (nimport + 1) * 16, 16)   // PLT[0] reserved, unused w/ eager
text_end      = text_off + code_size
data_off      = align_page(text_end)                       // page-aligned writable segment
gotplt_off    = data_off                                   // GOT contiguous at segment start
data_after_got= gotplt_off + (3 + nimport) * 8
flt_off       = data_after_got
str_off       = flt_off + nflt * 8
gl_off        = str_off + str_size
dyn_off       = gl_off + gl_count * 8
file_end      = dyn_off + (dyn_entry_count) * 16
```

Emit each section in order (see the ELF reference shape appendix for exact byte
layouts of `Elf64_Sym`, the hash table, `Elf64_Rela`, the PLT stub, and the
`DT_*` set):
- `.dynstr` — position 0 empty, then `"<lib>\0<sym0>\0<sym1>\0…"`.
- `.dynsym` — index 0 zero; one `Elf64_Sym` per import (STB_GLOBAL|STT_FUNC, SHN_UNDEF).
- `.hash` — 1-bucket (nbucket=1, nchain=N+1, bucket[0]=1, chain[k]=k+1, last 0).
- `.rela.plt` — one per import: `r_offset = gotplt_off + (3+i)*8`, `r_info = ((i+1)<<32)|7`, `r_addend = 0`.
- `.plt` — one 16-byte stub per import: `ff 25` + disp32 → `got.plt[3+i]`, then `0f 1f 40 00` NOP pad.
- `.got.plt` — 3 reserved zeros + per-import initial value `plt_off + 16 + i*16`.
- `.dynamic` — the `DT_*` entries (NEEDED, STRTAB/SYMTAB/STRSZ/SYMENT, HASH, PLTGOT, PLTRELSZ, PLTREL=DT_RELA, JMPREL, FLAGS_1=DF_1_NOW), terminated by DT_NULL.

Then patch the text relocations: types 0/1/2 (global/string/float) unchanged;
**type 4 (extern call)** → `target = plt_off + 16 + (sym_index * 16)`, `disp32 =
target - (reloc_site + 4)`, where `sym_index` is the import's PLT slot.

### 1b. Reloc-driven detection (inside the writer)

After `mark_reachable()` + codegen, the deciding signal is **any type-4 reloc**:
≥ 1 → dynamic ELF (`imports` = distinct type-4 names); 0 → static, exactly as
today. `bpp.bpp` has a single ELF call site (`write_elf(...)` at `bpp.bpp:539`);
do **not** add an `if dynamic` branch in the orchestrator — `write_elf` itself
scans for type-4 and dispatches internally to the static path or `write_elf_dyn`
(Rule 41 audit test 3: push the decision into the target layer). `write_elf`'s
signature stays backward compatible.

Reachability keeps this honest: a program that reaches no extern call (every
plain test, and `bpp` itself) emits zero type-4 relocs and stays **static**.

### 1c. Verify in Docker against libc (mechanism smoke + libc-init answer)

The engine's acceptance test needs **no GPU/audio**. Link `libc.so.6`, call a
symbol, confirm load + bind under a real `ld.so`:

- **Pure symbol first** (e.g. `strlen`, `abs`) — exercises the *whole* dynlink
  path (PT_INTERP → ld.so, `.dynsym` lookup, `.rela.plt` JUMP_SLOT, PLT/GOT bind)
  with **no** libc-runtime dependency. `exit=0` in Docker proves the engine.
- **Then a runtime-dependent symbol** (e.g. `puts`) — forces the **libc-init
  answer** (does our `_start` suffice, or must we route through
  `__libc_start_main` / set up `%fs`/TCB?). Record the verdict in this plan.

```bash
./bpp --linux64 tests/test_dynlink_smoke.bpp -o /tmp/t
readelf -dlr /tmp/t        # NEEDED libc.so.6, PT_INTERP + PT_DYNAMIC, JUMP_SLOT relocs
docker run --rm --platform linux/amd64 -v /tmp:/tmp ubuntu:22.04 /tmp/t   # exit 0
```

Add `tests/test_dynlink_smoke.bpp` (a tiny program that calls one libc symbol) as
the permanent regression for the engine. This is the full Phase-1 win condition.

## Phase 2 — the GPU/audio door (stub + hardware-gated)

The mechanism is proven and the **soname door is open** (`21d278e`); Phase 2
wires the *consumers* and leaves the hardware-specific parts stubbed.

- **Soname from the FFI library tag — DONE (`21d278e`).** The single-`DT_NEEDED`
  is no longer hardcoded `libc.so.6`: `elf_lib_soname` resolves it from the
  import-block tag via the existing `EX_LIB` field — `Vulkan.B`→`libvulkan.so.1`,
  `ALSA.B`→`libasound.so.2`, etc. Docker-verified: `import "Vulkan.B"` emits
  `NEEDED libvulkan.so.1`. (One `DT_NEEDED` per *distinct* soname — for a program
  that mixes two FFI libraries — remains a follow-up; see Engine status.)
- **Create the Tier-3 platform files** `_gpu_linux.bsm` / `_audio_linux.bsm`
  (separate files per the manual — GPU/Window/Audio are always per-OS files).
  Wire the abstract API (`bpp_gpu` / `bpp_audio`) to the engine, with each driver
  call **stubbed** and marked `// ships when x86 hardware + driver present`.
- The stubs return sane "unavailable" defaults so a program *compiles and runs*
  on a box without the driver (falling back to CPU render / silent audio), and
  *links + uses* the driver on a box that has it.

Phase 2 lands the door; it is not "done" until Activation runs on real hardware.
Per the plan-lifecycle rule, this plan stays **active** with Phase 2 as the
explicit "what's left" until that happens.

## Activation on x86 hardware (the treasure map)

When a real x86_64 Linux box with a GPU/audio driver arrives, coupling a stubbed
payload is this checklist — the engine is already proven, so this is small:

1. **Confirm the driver is present:** `ldconfig -p | grep -E 'libvulkan|libasound'`.
2. **Flip the stub to real** in `_gpu_linux.bsm` / `_audio_linux.bsm`: replace
   the stub body with the real `import "Vulkan.B" { … }` declaration + call.
3. **Name the library** — the tag→soname map already exists (`elf_lib_soname`),
   so `import "Vulkan.B"` emits `DT_NEEDED libvulkan.so.1` with no engine change.
   For a brand-new library, add one `elf_pk_eq_cstr` line to that map.
4. **Build:** `./bpp --linux64 <gpu_demo>.bpp -o demo`.
5. **Verify on hardware:** `readelf -d demo` shows the driver `.so`; `ldd demo`
   resolves it; run it — the window renders / sound plays.
6. **If a linked C library misbehaves at its entry**, apply the **libc-init
   verdict from Phase 1c** (route `_start` through `__libc_start_main`, or set up
   the main-thread `%fs`/TCB). This is the single pre-identified risk; everything
   else is mechanical.
7. **Drop the `// ships when hardware lands` markers** — the door is live. Update
   this plan's status (close it → move to `docs/plans/legacy/` per the lifecycle
   rule).

The *general* version of this technique — build the engine, stub the
hardware-gated payload, couple on arrival — is doctrine in
`docs/manual/nomad_manual.md`.

## Reference shape — what gcc emits (the ELF spec; does not drift)

`Elf64_Sym` (24 B): `st_name`(u32 dynstr off) · `st_info`(u8, STB_GLOBAL<<4 |
STT_FUNC) · `st_other`(u8 0) · `st_shndx`(u16 SHN_UNDEF=0) · `st_value`(u64 0) ·
`st_size`(u64 0). Index 0 all-zero; imports start at 1.

`Elf64_Rela` (24 B): `r_offset`(u64 GOT slot addr) · `r_info`(u64
(sym_index<<32)|R_X86_64_JUMP_SLOT(7)) · `r_addend`(i64 0).

`.plt` stub (16 B, eager): `ff 25 <disp32>` jmp `[rip+GOT[i]]`, then `0f 1f 40
00` NOP pad. With `BIND_NOW`, GOT[i] is pre-resolved so the stub is a single jmp;
the lazy `push $idx; jmp PLT0` tail is dead code we don't emit.

`.got.plt` (8 B each, 3 reserved): `[0]`=&_DYNAMIC, `[1]`=link_map (ld.so),
`[2]`=_dl_runtime_resolve (unused w/ eager), `[3..]`=resolved addresses.

Confirmed in Docker (gcc 13 / glibc 2.36): a **non-PIE `ET_EXEC`** dynamic binary
at `0x400000` loads + runs (`exit=0`); legacy **`DT_HASH`** is accepted (gcc emits
both `.hash` and `.gnu.hash`, glibc uses either); interpreter is
`/lib64/ld-linux-x86-64.so.2` (28 bytes incl. NUL).

## GOT-layout policy (realized 2026-05-28)

A GOT must be laid out so the loader's fixup mechanism resolves every entry.
ELF makes this trivial — relocations are **per-entry** (`R_X86_64_JUMP_SLOT`, one
`Elf64_Rela` per slot), so a `.got.plt` of any size resolves regardless of page
boundaries. We still keep the writable `PT_LOAD` page-aligned and the `.got.plt`
contiguous (free, and the shared discipline).

The sibling lesson came from Mach-O, whose chained fixups are **per-page**: a GOT
straddling a 16 KB page boundary left the spilled slots un-rebased. That bug was
**fixed 2026-05-28** (`4d0b9ea`, `a64_macho.bsm` — pad the GOT off a straddle +
make the byte-writer honor the same offset). ELF is immune by construction; a
future per-page format (Windows PE `.reloc`) would inherit the Mach-O-style care.
The shared, format-agnostic part (which externs, GOT slot assignment) lives in the
spine (`cg_ex_*`); the per-format emission lives in each target writer. See
`docs/manual/nomad_manual.md` ("format-agnostic model, format-specific emission").

## Verification gates (every step)

- Native a64 suite stays green + bootstrap **gen1 == gen2** byte-stable (this is
  an x64/ELF-writer change; it must not perturb a64 output).
- C-emit suite unaffected.
- The **static ELF path is byte-identical** for non-linking programs — detection
  only *adds* a dynamic route; it must not change the static one.
- `bpp` itself stays a **static** ELF on Linux (it reaches no extern call) — the
  self-host bootstrap must never silently acquire a `libc.so.6` dependency.
- Phase 1c: `test_dynlink_smoke` loads + binds in Docker (`exit=0`).

## Doctrine compliance (Rule 41 + Layer 4)

Additive portability (Rule 41): brings the Linux target up in capability,
touching **only** the Linux layers.
1. **No `stb/` file modified** — the abstract `bpp_gpu`/`bpp_audio` API is the
   contract; only the Linux backing changes.
2. **No other target's backend touched** — changes confined to
   `src/backend/target/x86_64_linux/x64_elf.bsm` + the new `_gpu_linux.bsm` /
   `_audio_linux.bsm`. macOS untouched.
3. **Source byte-identical across targets** — a `import "stbgpu.bsm"; main(){…}`
   program compiles unchanged for every target; no `#if TARGET`.

Decision-tree placement: the ELF-writer expansion is point 4 (chip+OS binary
format) → `target/` layer, not the spine. The GPU/audio platform hooks are
Tier-3 → separate per-OS files. New files → update `install.sh` in the same
commit (Discipline #4).

## Estimated effort

- `write_elf_dyn` section emission: ~200 lines in `x64_elf.bsm` (the bulk).
- Detection (reloc scan) + type-4 patch in the dyn path: ~40 lines.
- `test_dynlink_smoke.bpp` + Docker verification + libc-init settle: 2–4 hours.
- Phase 2 door (EX_LIB + stubbed `_gpu_linux`/`_audio_linux`): a separate sitting.

Phase 1 (engine + Docker-verify) is a focused half-day, front-loaded on the ELF
writer and the byte-level `readelf`/`ldd` debugging ELF always demands — now with
warm tools and a self-hosted Linux box to debug on. Phase 2 is bounded but waits
on hardware to *finish*.
