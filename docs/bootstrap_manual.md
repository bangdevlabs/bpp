# B++ Bootstrap Manual

How to evolve the B++ compiler safely. The compiler is self-hosting: `bpp`
compiles itself. Every change to the compiler source must go through the
bootstrap cycle described here.

## The Golden Rule

**Never overwrite `./bpp` with an untested binary.** The repo binary is the
only tool that can build the compiler. If it gets corrupted, you must recover
it from git (`git show HEAD:bpp > bpp && chmod +x bpp`).

## Quick Reference

```bash
# 1. Build gen1 from current source (using the repo bpp)
./bpp src/bpp.bpp -o /tmp/bpp_gen1

# 2. Use gen1 to compile itself → gen2
/tmp/bpp_gen1 src/bpp.bpp -o /tmp/bpp_gen2

# 3. Verify stability: gen1 and gen2 must be identical
diff <(xxd /tmp/bpp_gen1) <(xxd /tmp/bpp_gen2)
# (no output = stable bootstrap)

# 4. Install
cp /tmp/bpp_gen2 ./bpp
```

## Why Two Generations?

gen1 is compiled by the OLD compiler. If your source changes affect how code
is generated (new opcodes, different register allocation, etc.), gen1 may
produce slightly different machine code than what the old compiler would. gen2
is compiled by gen1 (the NEW compiler), so it reflects the new codegen fully.

If gen1 == gen2 (byte-identical), the bootstrap is stable. If they differ,
you need gen3:

```bash
/tmp/bpp_gen2 src/bpp.bpp -o /tmp/bpp_gen3
diff <(xxd /tmp/bpp_gen2) <(xxd /tmp/bpp_gen3)
# gen2 == gen3 means stable; install gen3
```

## macOS Gotchas

### Exit 137 (SIGKILL) — Code Signing Cache

macOS caches code signatures per filesystem path. If you overwrite a binary
at the same path and run it, the kernel may kill it based on the OLD cached
signature. **This is not a compiler bug.**

Fix: always compile to a FRESH path (use `/tmp/bpp_genN`), never overwrite
and immediately run from the same path.

### Exit 138 (SIGBUS) — Usually Alignment

On ARM64, SIGBUS typically means misaligned memory access. Common causes in
B++:
- Unicode characters in source files (see below)
- `auto` declarations inside loop bodies (move to function top)

### Exit 139 (SIGSEGV) — Null Pointer

Often caused by accessing compiler data structures that are NULL for cached
modules. Functions loaded from `.bo` cache have `FN_PARS = 0` but may have
`FN_PCNT > 0`. Always guard pointer access.

## Source File Rules

### Unicode in Comments is OK

The B++ compiler handles UTF-8 in comments correctly. Previous sessions
incorrectly blamed unicode for crashes — the actual cause was stale .bo cache.
Unicode in string literals is NOT tested.

### All Variables at Function Top

Declare all `auto` variables at the top of each function body. Do NOT declare
variables inside `if`, `for`, or `while` blocks. The codegen may not allocate
stack space for them, causing silent stack corruption.

```bpp
// GOOD
my_func() {
    auto i, j, tmp, result;
    for (i = 0; ...) {
        tmp = something();
    }
}

// BAD — may cause SIGBUS or Heisenbug
my_func() {
    auto i;
    for (i = 0; ...) {
        auto tmp;          // <- stack corruption risk
        tmp = something();
    }
}
```

### Comments in English

All code comments must be in English, written like a user manual.

## Testing Changes

### Simple Smoke Test

```bash
echo 'main() { putchar(72); putchar(10); return 0; }' > /tmp/test.bpp
./bpp /tmp/test.bpp -o /tmp/test_out
/tmp/test_out
# Should print: H
```

### Testing Both Pipelines

The compiler has two pipelines. Both must work:

```bash
# Modular (default for native binaries)
./bpp /tmp/test.bpp -o /tmp/test_mod

# Monolithic (forced)
./bpp --monolithic /tmp/test.bpp -o /tmp/test_mono
```

### Never Do

- Do NOT run `codesign` on bpp — it changes bytes and breaks hash-based cache.
- Do NOT manually delete `~/.bpp/cache/` — use `bpp --clean-cache` instead.
  The cache is at `~/.bpp/cache/` (NOT `.bpp_cache/` in the repo).
- Do NOT test with binaries in `build/` — use `/tmp/` or project root.
- Do NOT leave `bpp` processes running in the background.
- Do NOT create intermediate binaries in the repo (bpp_new, bpp_check, etc.).

## Step-by-Step: Adding a New Compiler Module

1. Create `src/bpp_mymodule.bsm`.
2. Add explicit imports at the top of your module for everything it uses:
   ```
   import "defs.bsm";
   import "stbarray.bsm";
   import "bpp_internal.bsm";  // if you use buf_eq, packed_eq, etc.
   ```
   **This is critical for the modular cache.** Without explicit imports,
   the dependency graph won't have edges for your module, and changes to
   your dependencies won't invalidate your .bo cache file.
3. Add `import "bpp_mymodule.bsm";` to `src/bpp.bpp`.
4. Add `init_mymodule();` to the init block in `main()`.
5. Wire the module's entry point into the pipeline.
6. Run the full bootstrap cycle (gen1 → gen2 → diff).
7. Test with both simple programs AND a different program (to verify cache isolation).

## Cache System

The build cache lives at `~/.bpp/cache/`. It's global per user (like Go's GOCACHE).

### How it works
- Each module gets a hash: `fnv1a(source) ^ compiler_hash ^ dep_hashes ^ main_hash`
- The .bo filename IS the hash — different content = different file
- When source changes, hash changes, old .bo is ignored, module recompiled
- `main_hash` isolates programs: compiling `bug.bpp` won't pollute `hello.bpp`

### When to clean
- `bpp --clean-cache` deletes everything. Next build is slower (recompiles all).
- Only needed if cache grows too large. The cache auto-invalidates correctly.
- Never needed for correctness — only for disk space.

### Common cache issues
If a build produces unexpected results:
1. Try `bpp --clean-cache` first
2. If that fixes it, the cache was stale (should not happen with explicit imports)
3. If not, the bug is in the source, not the cache

## Compiler Flags Reference

| Flag            | Effect                                      |
|-----------------|---------------------------------------------|
| (default)       | Native Mach-O ARM64 binary, modular pipeline |
| `--monolithic`  | Force single-pass pipeline (all modules at once) |
| `--asm`         | Emit ARM64 assembly to stdout               |
| `--c`           | Emit C code to stdout                       |
| `--linux64`     | Cross-compile to Linux x86_64 ELF           |
| `--bug`         | Emit `.bug` debug map alongside the binary   |
| `--show-deps`   | Print module dependency graph and exit       |
| `--clean-cache` | Delete all .bo files in `~/.bpp/cache/`     |
| `-o <name>`     | Output filename                              |

## Recovering from a Broken bpp

If `./bpp` is corrupted or produces broken binaries:

```bash
git show HEAD:bpp > ./bpp
chmod +x ./bpp
```

This restores the last committed working binary.
