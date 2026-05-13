# rts_1.0 — Asset bundle

Converted assets used by the `games/rts_1.0/` RTS demo. All files
are in formats that B++ standard cartridges consume directly
(`stbimage` for PNG, `stbsound` for WAV) or that game-side B++ code
parses (SMS / SMP / TXT maps + scripts). MIDI awaits the upcoming
`stbmidi` cartridge.

## Source

`DATA.WAR` from **Warcraft: Orcs & Humans** (Blizzard Entertainment,
1994). The DATA.WAR archive is *not* checked into this repository —
keep your own copy outside the working tree (the developer here
parks it at `/Users/Codes/wc1_extract/DATA.WAR`). Educational /
development use only; not for redistribution.

## Layout

```
assets/
├── README.md          (this file)
└── converted/
    ├── graphics/      229 PNG — tilesets, sprites, UI, cursors
    ├── sounds/        111 WAV — SFX, speech, UI clicks
    ├── music/          45 MID — XMI tracks converted to standard MIDI
    ├── maps/           16 files — 8 skirmish maps (SMS script + SMP data)
    ├── scripts/         1 Lua — stratagus main config
    └── campaigns/     113 files — 24 missions × (SMS + SMP + intro WAV + TXT briefing)
```

Total: ~17 MB across ~500 files.

## Reproducing the conversion

The conversion is one-shot — assets here are stable across re-runs.
If you need to regenerate (e.g. you tweak the converter or pull a
new DATA.WAR), use the upstream `war1gus` project's `war1tool`:

```bash
# 1. Clone war1gus and its third-party submodule.
git clone https://github.com/Wargus/war1gus /Users/Codes/war1gus
cd /Users/Codes/war1gus && git submodule update --init third-party

# 2. war1tool needs a single header from the Stratagus engine.
#    A minimal local stub covers everything war1tool actually uses:
mkdir -p /tmp/sg_stubs && cat > /tmp/sg_stubs/stratagus-gameutils.h <<'EOF'
#ifndef STRATAGUS_GAMEUTILS_STUB_H
#define STRATAGUS_GAMEUTILS_STUB_H
#include <filesystem>
#include <string.h>
namespace fs = std::filesystem;
#ifdef _WIN32
#define SLASH "\\"
#else
#define SLASH "/"
#endif
static inline void mkdir_p(const char* path) {
    std::error_code ec; fs::create_directories(fs::path(path), ec);
}
static inline void parentdir(char* path) {
    if (!path || !*path) return;
    size_t n = strlen(path);
    while (n > 1 && path[n-1] == '/') path[--n] = 0;
    char* s = strrchr(path, '/');
    if (s) *s = 0; else { path[0] = '.'; path[1] = 0; }
}
#endif
EOF

# 3. Configure with libpng + zlib (Homebrew installs both):
cd /Users/Codes/war1gus && mkdir -p build && cd build
cmake -DCMAKE_CXX_FLAGS="-I/tmp/sg_stubs" ..

# 4. war1tool aborts if the campaign-cutscene .war files aren't
#    next to DATA.WAR. We don't have those (they come on the original
#    CD as separate archives) and don't need them for gameplay.
#    Touch empties so the copy step passes:
grep -oE '^\{FLC,0,"[^"]+"' /Users/Codes/war1gus/war1tool.cpp \
  | sed -E 's/.*"([^"]+)".*/\1/' \
  | while read f; do touch "/path/to/your/DATA.WAR-dir/$f"; done

# 5. Build and run (-m converts XMI music to MID; skip -v for videos).
cmake --build . --target war1tool
./war1tool -m /path/to/your/DATA.WAR-dir <repo>/games/rts_1.0/assets/converted

# 6. war1tool gzips audio; decompress in place:
find <repo>/games/rts_1.0/assets/converted -name "*.gz" | xargs gunzip
```

## License notice

These assets are the intellectual property of Blizzard Entertainment.
They are included here for the personal educational use of the
developer studying B++ engine work against a real retro RTS corpus.
**Do not redistribute.** Anyone forking this repo for actual use is
expected to bring their own legally-obtained copy of Warcraft: Orcs
& Humans and re-run the conversion against their own DATA.WAR.
