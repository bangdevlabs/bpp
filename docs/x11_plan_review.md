# X11 Plan Review — What Changed Since the Plan Was Written

Report for the agent implementing the X11 wire protocol on Linux.

## CRITICAL: New Compiler Feature — `&` (Address-of)

The B++ compiler now supports `&variable` to get the address of a local or global.
This was added TODAY (2026-04-06). The plan uses the old `malloc(8)` workaround
pattern in several places. **Use `&` instead.**

Example — the plan's FIONREAD pattern:
```
// OLD (plan uses this — DON'T)
count_buf = malloc(4);
sys_ioctl(_x11_fd, 0x541B, count_buf);
avail = read_u32(count_buf, 0);
free(count_buf);

// NEW (use this)
auto avail;
sys_ioctl(_x11_fd, 0x541B, &avail);
```

This applies everywhere the plan allocates scratch buffers for out-parameters.

## New Modules Created Today

- **stbtile.bsm** (module #22) — tilemap engine, now in `stb/`
- **stbphys.bsm** (module #23) — platformer physics, now in `stb/`
- These don't affect the X11 work directly, but the module count is now 24 (not 21).

## stbimage.bsm — tRNS Support Added

The PNG loader now reads the tRNS chunk for palette-indexed PNGs.
If the X11 platform ever needs to load PNG assets, transparency will work correctly.

## Metal Shader Change — Alpha Discard

`_stb_platform_macos.bsm` fragment shader now has `discard_fragment()` for
alpha < 0.01. This is macOS-only and doesn't affect the Linux platform.
But if/when Vulkan shaders are written for Linux, apply the same pattern.

## Bootstrap Workflow Reminders

Any compiler changes (like adding `sys_poll`) MUST follow:
```
./bpp src/bpp.bpp --clean-cache -o bpp2
./bpp2 src/bpp.bpp -o bpp3
shasum bpp2 bpp3    # must match
cp bpp2 bpp
```

Rules:
- NO codesign (breaks reproducibility)
- Use `;` not `&&` between commands
- `--clean-cache` only cleans, does NOT compile — run separately
- NEVER git stash/checkout/restore the `bpp` binary

## Specific Plan Concerns

### 1. Plan says "X11 via libX11.so FFI" in P4 — WRONG
The plan correctly uses raw X11 wire protocol (syscalls only, no FFI).
But `docs/todo.md` P4 still says "X11 window via libX11.so FFI". This is outdated.
The todo needs updating to reflect the wire protocol approach.

### 2. ELF Dynamic Linking — NOT needed for wire protocol X11
The plan's P0 lists "ELF dynamic linking (blocks Linux GPU)". This was true for
the old FFI approach. With raw X11 wire protocol, no shared libraries are needed.
ELF dynamic linking is only needed for Vulkan (Phase 4 of the X11 plan).

### 3. `_stb_shutdown` missing
The plan correctly identifies this. Currently Linux platform has no shutdown function.
macOS has `_stb_shutdown()` that calls `_plt_terminate`. Add it.

### 4. `ptr` is a reserved keyword in B++
Don't use `ptr` as a variable name. Use `p`, `buf`, `addr`, etc.

### 5. X11 Auth — MIT-MAGIC-COOKIE
The plan says "send empty auth" for Phase 1. This is correct for most local
setups and Docker with `xhost +localhost`. But be aware: some systems require
reading `~/.Xauthority`. Add a TODO comment for future auth support.

### 6. Maximum Request Size
For 320x180 (our default game size), pixel data = 230400 bytes — fits in one
X11 request. The strip-splitting code for larger windows is nice-to-have but
not needed for Phase 1-3. Keep it simple.

### 7. Keycode Mapping Verification
The plan's keycode-to-slot mapping MUST match the KEY_* constants in stbinput.bsm.
Read `stb/stbinput.bsm` to verify that slot 1 = KEY_UP, slot 5 = KEY_W, etc.
A mismatch here means all games would have broken input.

### 8. read_u16/write_u16 Endianness
The plan says "write_u16/write_u32 from stbbuf.bsm write LE". Verify this.
X11 LE protocol expects LE, but some fields (like TCP port in sockaddr_in)
need BE (big-endian). You may need manual byte-swapping for network byte order.

### 9. Dual Rendering Path
The plan correctly keeps terminal rendering as fallback. Make sure the existing
`_stb_present()` terminal path is not broken. Test both paths.

### 10. Docker TCP Connection
For `host.docker.internal`, the plan suggests hardcoding IPs. A cleaner approach:
read `/etc/hosts` inside the container — Docker adds the mapping there automatically.
