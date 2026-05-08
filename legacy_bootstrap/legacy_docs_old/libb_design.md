# libb — B++ Runtime Library Design

**STATUS: IMPLEMENTED.** Landed in commits `0d3f282` (bsys tables),
`f5ce95a` (brt0 globals), and `9eca4de` (bmem allocator). Linux static
ELF binaries link nothing at all; macOS binaries link libSystem only
because dyld requires it. See `docs/journal.md` 2026-04-13/14 entry
for implementation notes.

Final directory layout differs slightly from the original proposal:
runtime files live under `src/backend/os/<target>/` (not a top-level
`libb/`), matching the chip/os/target split applied to the rest of
the backend. The functional design below is unchanged.

---

B++ ships its own runtime library instead of depending on libc. The
runtime provides three services that every B++ binary needs: process
startup (brt0), system call abstraction (bsys), and memory allocation
(bmem). Each service has a single public API and one implementation
file per target platform. Adding a new platform means writing three
small files — zero changes to the compiler, zero changes to user code.

## Why libb exists

B++ compiles to static binaries with no external dependencies. On
macOS the only implicit link is libSystem (for dyld and the dynamic
linker); on Linux the binary is completely self-contained. This is a
feature, not an accident — it means a B++ game works on any machine
with the right kernel, no package manager, no shared library versioning.

libc is battle-tested and universal. Rust, Odin, and Jai depend on it.
Zig calls libc on macOS and does direct syscalls only on Linux. The
trade-off is clear: libc gives you decades of hardening and platform
coverage; independence gives you control and portability to bare metal.

B++ takes the hybrid path:

- **Native backends** (ARM64 macOS, x86_64 Linux): the runtime handles
  startup and syscalls directly. Memory allocation uses platform APIs
  (libSystem on macOS, sys_mmap on Linux). No libc dependency beyond
  what the OS requires (libSystem on macOS is mandatory for any Mach-O
  binary; Linux ELF needs nothing).

- **C emitter** (`bpp --c`): generated C code uses libc naturally —
  malloc, printf, read, write. This is the universal fallback for any
  platform where a native backend does not exist yet.

The result: native backends are as independent as the OS allows, and
the C emitter provides instant portability to everywhere else.

## Architecture

```
libb/
  brt0.bsm              ← Public API: _brt0_entry() → argc/argv/envp → main → exit
  _brt0_macos.bsm       ← Mach-O: LC_MAIN entry, saves argc/argv/envp from registers
  _brt0_linux.bsm       ← ELF: _start, reads argc/argv/envp from kernel stack
  _brt0_windows.bsm     ← Future: PE entry, GetCommandLineW → argv conversion
  _brt0_wasm.bsm        ← Future: WASI _start, args_get/args_sizes_get

  bsys.bsm              ← Public API: bsys_write(fd,buf,len), bsys_exit(code), etc.
  _bsys_macos.bsm       ← Syscall numbers + SVC #0x80 trap
  _bsys_linux.bsm       ← Syscall numbers + syscall instruction
  _bsys_windows.bsm     ← Future: ntdll NtWriteFile / kernel32 wrappers
  _bsys_wasm.bsm        ← Future: WASI fd_write / fd_read imports

  bmem.bsm              ← Public API: malloc(size), free(ptr), realloc(ptr,size)
  _bmem_macos.bsm       ← mmap via libSystem (or own allocator over sys_mmap)
  _bmem_linux.bsm       ← Own allocator over sys_mmap (no libc)
  _bmem_windows.bsm     ← Future: VirtualAlloc + own free list
  _bmem_wasm.bsm        ← Future: memory.grow + own free list
  _bmem_bare.bsm        ← Future: fixed buffer from linker script
```

The compiler resolves platform files automatically via the existing
target-suffix fallback in `bpp_import.bsm`. When bmem.bsm does
`import "_bmem.bsm";`, the resolver appends `_macos` or `_linux`
based on the compilation target. This is the same mechanism that
already resolves `_stb_platform.bsm` to the per-OS file.

Adding a new platform = write 3 files + add a target flag to the
compiler. The public API files (brt0.bsm, bsys.bsm, bmem.bsm)
never change.

## 1. brt0 — Process Startup

### What it does

The code that runs before `main()`. Receives process state from the
kernel (or host environment), stores it in globals accessible to the
program, calls `main()`, and exits with the return value.

### Current state (what exists today)

**ARM64 macOS** (`a64_codegen.bsm:2170-2183`):
macOS uses LC_MAIN, so dyld calls `main(argc, argv, envp, apple)`
directly using the standard calling convention. The codegen detects
when it is emitting `main()` and inserts a prologue that saves x0/x1/x2
to hidden globals (`_bpp_argc`, `_bpp_argv`, `_bpp_envp`).

**x86_64 Linux** (`x64_codegen.bsm:1862-1937`):
The codegen emits an explicit `_start` stub. On Linux at process
entry, the kernel stack contains `[argc, argv[0], ..., NULL, envp[0],
..., NULL]`. The stub pops argc into RDI, computes argv (RSP) and
envp (&argv[argc+1]), aligns the stack, calls main, then calls
sys_exit with the return value.

### What changes with libb

The startup code moves OUT of the codegens into dedicated files:

- `_brt0_macos.bsm`: the argc/argv/envp save logic. The codegen
  still detects `main()` but instead of emitting the save inline,
  it calls `_brt0_save_args()` which lives in the runtime module.

- `_brt0_linux.bsm`: the `_start` stub. Instead of `x64_emit_start_stub()`
  living inside `x64_codegen.bsm`, the stub is emitted from the
  runtime module. The binary writer (`x64_elf.bsm`) references the
  runtime's `_start` label instead of generating it.

The hidden globals (`_bpp_argc`, `_bpp_argv`, `_bpp_envp`) move to
brt0.bsm and become part of the runtime's data section.

### Future platforms

| Platform | Entry mechanism | argc/argv source |
|----------|----------------|-----------------|
| macOS ARM64 | LC_MAIN → main(argc,argv,envp) | Registers x0,x1,x2 |
| Linux x86_64 | _start, kernel stack | Stack: [argc, argv..., envp...] |
| Linux ARM64 | _start, kernel stack | Stack: same layout as x86_64 |
| Windows x86_64 | mainCRTStartup or WinMain | GetCommandLineW → parse |
| WebAssembly | _start export | WASI args_get / args_sizes_get |
| Bare metal | Linker _start | Board-specific or none |

Each `_brt0_<target>.bsm` handles its platform's entry convention
and presents the same interface to the rest of the runtime.

## 2. bsys — System Call Abstraction

### What it does

Provides a single function per syscall operation with a platform-
independent signature. The implementation file per platform contains
the actual syscall numbers and trap mechanism.

### Current state

Both codegens have 18 near-identical if-blocks for syscall builtins.
Each block:
1. Evaluates arguments and puts them in the right registers
2. Loads a hardcoded syscall number
3. Emits the trap instruction (SVC #0x80 on ARM64, syscall on x86_64)

The only difference between platforms is the syscall number and the
register convention. The argument evaluation and register setup logic
is duplicated across both codegens.

### What changes with libb

The 18 per-syscall if-blocks collapse into:

**A syscall table** (per platform):
```
// _bsys_linux.bsm
const BSYS_EXIT      = 60;
const BSYS_WRITE     = 1;
const BSYS_READ      = 0;
const BSYS_OPEN      = 2;
const BSYS_CLOSE     = 3;
const BSYS_MMAP      = 9;
const BSYS_MUNMAP    = 11;
const BSYS_UNLINK    = 87;
const BSYS_MKDIR     = 83;
const BSYS_FORK      = 57;
const BSYS_EXECVE    = 59;
const BSYS_WAITPID   = 61;
const BSYS_WAIT4     = 61;
const BSYS_PTRACE    = 101;
const BSYS_NANOSLEEP = 35;
const BSYS_SOCKET    = 41;
const BSYS_CONNECT   = 42;
const BSYS_GETDENTS  = 217;
const BSYS_LSEEK     = 8;
const BSYS_FCHMOD    = 91;
```

**A generic emit function** in each codegen:
```
// In the codegen — ONE function replaces 18 if-blocks
emit_syscall(syscall_number, arg_count) {
    // 1. Evaluate and load arguments into platform registers
    // 2. Load syscall_number into the syscall register
    // 3. Emit trap instruction
}
```

**A name→(number, argc) lookup** in the codegen that maps builtin
names to their table entries. The codegen's builtin dispatcher
becomes: look up the name in the syscall table; if found, call
emit_syscall with the values. One dispatch path for all syscalls.

### Syscall table (current platforms)

| Operation | macOS ARM64 | Linux x86_64 | Args |
|-----------|------------|-------------|------|
| exit | 1 | 60 | 1 |
| write | 4 | 1 | 3 |
| read | 3 | 0 | 3 |
| open | 5 | 2 | 3 |
| close | 6 | 3 | 1 |
| mmap | 197 | 9 | 6 |
| munmap | 73 | 11 | 2 |
| unlink | 10 | 87 | 1 |
| mkdir | 136 | 83 | 2 |
| fork | 2 | 57 | 0 |
| execve | 59 | 59 | 3 |
| waitpid | 7 | 61 | 3 |
| wait4 | 7 | 61 | 4 |
| ptrace | 26 | 101 | 4 |
| nanosleep | — | 35 | 1 |
| socket | 97 | 41 | 3 |
| connect | 98 | 42 | 3 |
| getdents | 344 | 217 | 3 |
| lseek | 199 | 8 | 3 |
| fchmod | 124 | 91 | 2 |

Note: macOS `usleep` is implemented via nanosleep or a Mach absolute
time spin, not a direct syscall. The platform file handles this.

### Future platforms

| Platform | Trap mechanism | Syscall register | Notes |
|----------|---------------|-----------------|-------|
| macOS ARM64 | SVC #0x80 | x16 | Args in x0-x5 |
| Linux x86_64 | syscall | rax | Args in rdi,rsi,rdx,r10,r8,r9 |
| Linux ARM64 | SVC #0 | x8 | Args in x0-x5 |
| Windows | int 0x2E / syscall | rax | Unstable numbers, use ntdll |
| WebAssembly | WASI imports | N/A | Function calls, not traps |

Windows syscalls are explicitly unstable (numbers change between
builds). The correct approach for Windows is calling ntdll.dll via
FFI, which B++ can do once ELF/PE dynamic linking is implemented.

## 3. bmem — Memory Allocator

### What it does

General-purpose heap allocator. The public API is three functions:

```
malloc(size)           → returns pointer to size bytes, or 0 on failure
free(ptr)              → releases memory previously returned by malloc
realloc(ptr, new_size) → grows or shrinks an allocation, may move it
```

These are the same names and semantics as C's malloc/free/realloc.
User code does not change.

### Design: hybrid per platform

**macOS**: calls libSystem's malloc/free/realloc via FFI. libSystem
is already linked (mandatory for any Mach-O binary), and Apple's
allocator has decades of hardening, thread safety, and integration
with the OS memory pressure system. Replacing it gains nothing and
loses all of that.

**Linux (static ELF)**: own allocator over sys_mmap. No libc is
available in a static binary. This is where bmem is code, not a
wrapper.

**C emitter**: uses libc malloc/free/realloc. The generated C file
includes `<stdlib.h>`.

**Future bare metal / WASM**: own allocator over the platform's
page allocation primitive (memory.grow for WASM, fixed buffer for
bare metal).

### Linux allocator design

The allocator manages a heap built from pages requested via sys_mmap.
It never calls brk (deprecated on some platforms, not available on
WASM/bare metal). Design follows the classical free-list approach
used by musl, dlmalloc, and every game engine allocator.

#### Data structures

Every allocation has an 8-byte header immediately before the returned
pointer:

```
[ size:56 | free:1 | flags:7 ] [ ... user data ... ]
                                ^
                                returned pointer
```

- `size` (56 bits): total block size including header. Max allocation
  ~72 PB, far beyond any real use.
- `free` (1 bit): 1 if the block is on the free list, 0 if allocated.
- `flags` (7 bits): reserved for future use (alignment, arena tag, etc.).

#### Page management

```
BLOCK_MIN      = 32         // minimum allocation (header + 24 bytes usable)
PAGE_SIZE      = 4096       // OS page size (configurable per platform)
LARGE_THRESH   = 65536      // allocations >= 64KB get their own mmap region
```

Small/medium allocations (< LARGE_THRESH):
1. Walk the free list for a block >= requested size
2. If found and significantly larger, split: return the front, put
   the remainder back on the free list
3. If not found, request a new page (or page group) from the OS
   via sys_mmap and carve from it

Large allocations (>= LARGE_THRESH):
1. sys_mmap a dedicated region (rounded up to page size)
2. Header marks it as large (flag bit)
3. free() calls sys_munmap directly — no free list

#### Free and coalescing

When free(ptr) is called:
1. Read the header at ptr - 8
2. Mark the block as free
3. Check the next adjacent block: if also free, merge (coalesce)
4. Check the previous adjacent block: if also free, merge
5. Add the (possibly merged) block to the free list

Coalescing prevents fragmentation from repeated alloc/free cycles.
Without it, the heap grows without bound even when total live memory
is small.

#### realloc

1. If new_size fits in the current block (shrink or same): return ptr
2. If the next adjacent block is free and combined size is enough:
   merge in place, return ptr
3. Otherwise: malloc(new_size), memcpy, free(old ptr)

#### Thread safety (future)

Phase 1 (single-threaded): no locking. All B++ programs are
single-threaded today. The maestro pattern runs workers on OS threads
but workers communicate via SPSC queues, not shared heap allocation.

Phase 2 (when needed): per-thread arenas for small allocations,
global lock for large allocations. The header flags byte has room
for a thread-local tag. This is the same approach musl and jemalloc
use at different scales.

### Estimated size

| Component | Lines (estimate) |
|-----------|-----------------|
| Header and free list management | ~80 |
| malloc (free list search + page request) | ~60 |
| free (coalesce + return to free list) | ~50 |
| realloc (in-place grow or copy) | ~40 |
| sys_mmap wrapper | ~20 |
| Init and bookkeeping | ~30 |
| **Total** | **~280** |

This is a working allocator, not a toy. It handles the common case
(small allocations from free list) efficiently and the uncommon case
(large allocations) correctly. It does not handle thread-local
caches, per-size-class bins, or ASLR integration — those are
post-1.0 optimizations.

## Impact on the Codegen

### Before libb (current state)

```
a64_codegen.bsm:
  38 builtin if-blocks (string comparison dispatch)
  17 node type if-blocks (integer dispatch)
  + startup code in main() prologue
  + hidden globals management

x64_codegen.bsm:
  41 builtin if-blocks
  17 node type if-blocks
  + _start stub (~40 lines)
  + startup code in main() prologue
  + hidden globals management
```

### After libb

```
a64_codegen.bsm:
  ~9 primitive builtins (peek, poke, str_peek, shr, mem_barrier,
     float_ret, float_ret2, fn_ptr, call) — stay inline, 1-2 instructions each
  1 generic emit_syscall() + table lookup — replaces 18 if-blocks
  17 node type dispatches — stay, become switch with jump table
  NO startup code — moved to _brt0_macos.bsm
  NO hidden globals — moved to brt0.bsm

x64_codegen.bsm:
  ~9 primitive builtins — same set
  1 generic emit_syscall() + table lookup — replaces 18 if-blocks
  17 node type dispatches — stay, become switch with jump table
  NO _start stub — moved to _brt0_linux.bsm
  NO startup code — moved to _brt0_linux.bsm
  NO hidden globals — moved to brt0.bsm
```

### Builtins that move out entirely

These become B++ runtime functions (callable, not codegen-inlined):

| Builtin | Destination | Reason |
|---------|------------|--------|
| malloc | bmem.bsm | Platform-specific allocator |
| free | bmem.bsm | Paired with malloc |
| realloc | bmem.bsm | Paired with malloc |
| memcpy | bmem.bsm | Can be B++ loop or platform-optimized |
| putchar | bpp_io.bsm | Wrapper: bsys_write(1, &byte, 1) |
| putchar_err | bpp_io.bsm | Wrapper: bsys_write(2, &byte, 1) |
| getchar | bpp_io.bsm | Wrapper: bsys_read(0, &byte, 1) |
| assert | brt0.bsm | Check + print + bsys_exit(1) |
| argc_get | brt0.bsm | Reads startup global |
| argv_get | brt0.bsm | Reads startup global + indexes |
| envp_get | brt0.bsm | Reads startup global + indexes |

### Builtins that stay inline in codegen

These are single-instruction primitives. Function call overhead would
be larger than the operation itself:

| Builtin | Instructions | Why inline |
|---------|-------------|-----------|
| peek | LDRB (1) | Single byte load |
| poke | STRB (1-2) | Single byte store |
| str_peek | LDRB reg offset (1-2) | Byte load with index |
| shr | LSR / SHR (1) | Bit shift |
| mem_barrier | DMB ISH / MFENCE (1) | Memory fence |
| float_ret | FMOV (1) | ABI: float to int register |
| float_ret2 | FMOV (1) | ABI: second float return |
| fn_ptr | ADR label (1) | Address of function |
| call | BLR + arg setup (~5) | Indirect function call |

## Implementation Plan

### Phase 1 — Extract and centralize (reorganization)

No new code. Move existing code into the libb structure.

1. Create `libb/` directory
2. Extract startup code from codegens into `_brt0_macos.bsm` and
   `_brt0_linux.bsm`
3. Extract syscall numbers into `_bsys_macos.bsm` and `_bsys_linux.bsm`
4. Replace 18 per-syscall if-blocks with table lookup + generic
   `emit_syscall()` in each codegen
5. Move hidden globals (`_bpp_argc/argv/envp`) to brt0.bsm
6. Bootstrap verify after each step

Estimated: ~200 lines moved, ~80 lines new (emit_syscall + table).
Risk: low — same behavior, different file layout.

### Phase 2 — bmem for Linux (new code)

Implement the general-purpose allocator for Linux static binaries.

1. Write `_bmem_linux.bsm` (~280 lines)
2. Write `bmem.bsm` public API that imports `_bmem.bsm`
3. Write `_bmem_macos.bsm` as thin wrapper around libSystem FFI
4. Convert malloc/free/realloc from codegen builtins to function calls
   that route through bmem
5. Test with the full test suite + games
6. Bootstrap verify (the compiler itself uses malloc heavily)

Estimated: ~350 lines new code. Risk: medium — the compiler's own
malloc calls go through the new allocator. Bootstrap must converge.

### Phase 3 — Move I/O builtins to B++ functions

Convert putchar, putchar_err, getchar from codegen builtins to B++
functions in bpp_io.bsm that call bsys_write / bsys_read.

Estimated: ~30 lines. Risk: low.

### Phase 4 — Jump table optimization

With the codegen now clean (~27 dispatch points instead of ~55),
implement jump table emission for dense switch statements.

1. Add BR Xn to ARM64 encoder (~3 lines)
2. Add MOV r64,[base+idx*8] to x86_64 encoder (~8 lines)
3. Detect dense-constant switches at codegen time
4. Emit jump table for dense, comparison chain for sparse
5. Bootstrap verify

### Phase 5 — Tonify batch 6

Apply the tonify checklist to the now-final codegen files. The files
are smaller (~27 dispatch points instead of ~55), cleaner (no startup
code, no syscall number duplication), and in their final structural
form.

## Design Principles

1. **Platform is implementation, not exception.** The public API is
   one set of files. Each platform provides its own implementation.
   No `#ifdef`, no runtime detection, no fallback chains.

2. **The C emitter is the universal fallback.** For any platform
   where a native backend does not exist, `bpp --c` generates
   portable C that uses libc. This is not a second-class path — it
   is how B++ runs everywhere from day one.

3. **Use what the OS guarantees.** macOS guarantees libSystem; use
   it. Linux guarantees syscall ABI stability; use it. Windows
   guarantees ntdll; use it (when the backend exists). Do not fight
   the platform.

4. **Evolve, do not rewrite.** Phase 1 moves code. Phase 2 adds
   code. No phase deletes working functionality. The compiler keeps
   working at every step.

5. **The allocator is B++ code.** On every platform where B++ does
   not link libc, the allocator is written in B++ itself. This is
   what makes bare metal and new platforms possible without external
   dependencies.

## Reference: How Other Languages Do It

| | Allocator | Syscalls | Why |
|---|---|---|---|
| Rust | libc malloc (global trait) | Always libc | Stability, coverage |
| Zig | Own allocator, explicit param | Direct on Linux, libc on macOS | Independence + macOS pragmatism |
| Odin | libc malloc (implicit context) | Always libc | Simplicity |
| Jai | libc malloc (implicit context) | Always libc | Simplicity |
| **B++** | **Own on Linux, libSystem on macOS** | **Direct on both** | **Independence where possible, pragmatism where required** |

B++ is closest to Zig's approach: own everything where the platform
allows it, defer to the platform where it mandates an intermediary.
The key difference is that B++ has the C emitter as an additional
portability layer that Zig does not need (Zig has its own LLVM-based
cross-compilation).
