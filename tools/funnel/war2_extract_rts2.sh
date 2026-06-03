#!/bin/sh
# war2_extract_rts2.sh — batch-convert Warcraft 2 MAINDAT.WAR unit sprites into
# the rts2 asset tree, in the canonical Bang9/Aseprite format (.png + .json
# sidecar). Each row of the table below drives one war2_gfx run.
#
# The table (index palette name) IS the source of truth. Names are semantic
# where confirmed (peasant/peon/gryphon/...) and unit_NNN as a safe fallback
# for chunks not yet identified — re-run after renaming a row to regenerate
# that asset (the sidecar's meta.image is derived from the output path, so
# renames stay consistent with no manual JSON editing).
#
# Index/palette pairings come from tools/funnel/war2_maindat_index.md
# (extracted from Wargus wartool.h, verified against our DOS Tides of
# Darkness MAINDAT.WAR).
#
# Usage:  sh war2_extract_rts2.sh [MAINDAT.WAR] [OUT_DIR]
set -e

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WAR="${1:-/Users/Codes/Warcraft/Warcraft_II_-_Tides_of_Darkness_1995/WarcrafD/DATA/MAINDAT.WAR}"
OUT="${2:-$REPO/games/rts2/assets/sprites/wc2}"
TOOL=/tmp/war2_gfx

if [ ! -f "$WAR" ]; then echo "MAINDAT.WAR not found: $WAR" >&2; exit 1; fi

TILETOOL=/tmp/war2_tiles
MAPTOOL=/tmp/war2_pud
IMGTOOL=/tmp/war2_img
echo "building war2_gfx + war2_tiles + war2_pud + war2_img ..."
"$REPO/bpp" "$REPO/tools/funnel/war2_gfx.bpp"   -o "$TOOL"
"$REPO/bpp" "$REPO/tools/funnel/war2_tiles.bpp" -o "$TILETOOL"
"$REPO/bpp" "$REPO/tools/funnel/war2_pud.bpp"   -o "$MAPTOOL"
"$REPO/bpp" "$REPO/tools/funnel/war2_img.bpp"   -o "$IMGTOOL"
mkdir -p "$OUT"

# --- PUD maps -> level JSON (all .PUD under the game dir) ---
MAPOUT="$(dirname "$OUT")/../levels/wc2"
mkdir -p "$MAPOUT"
PUDDIR="$(dirname "$(dirname "$WAR")")"
find "$PUDDIR" -iname '*.pud' | while read pud; do
    base="$(basename "$pud" | sed -E 's/\.[Pp][Uu][Dd]$//' | tr '[:upper:] ' '[:lower:]_')"
    "$MAPTOOL" "$pud" "$MAPOUT/$base.json" >/dev/null 2>&1 || true
done
echo "maps -> $MAPOUT ($(ls "$MAPOUT" | wc -l | tr -d ' ') files)"

# --- terrain tilesets (pal, mega, mini per Wargus wartool.h) ---
TILEOUT="$(dirname "$OUT")/../tiles/wc2"
mkdir -p "$TILEOUT"
"$TILETOOL" "$WAR" 2   3   4   5   "$TILEOUT/summer.png"    >/dev/null
"$TILETOOL" "$WAR" 10  11  12  13  "$TILEOUT/wasteland.png" >/dev/null
"$TILETOOL" "$WAR" 18  19  20  21  "$TILEOUT/winter.png"    >/dev/null
"$TILETOOL" "$WAR" 438 439 440 441 "$TILEOUT/swamp.png"     >/dev/null
echo "tilesets -> $TILEOUT"

count=0
# idx palette name  (# = comment, blank = skip)
while read idx pal name rest; do
    case "$idx" in ''|\#*) continue;; esac
    # Units get directional animation frameTags (5th arg "unit"); buildings
    # stay as a plain frame grid.
    mode=unit
    case "$name" in bldg_*) mode="";; esac
    "$TOOL" "$WAR" "$pal" "$idx" "$OUT/$name.png" $mode >/dev/null
    count=$((count + 1))
done <<'TABLE'
# --- confirmed (semantic) ---
47  2  peasant
48  2  peon
122 2  peasant_with_wood
123 2  peon_with_wood
124 2  peasant_with_gold
125 2  peon_with_gold
35  2  gryphon_rider
36  2  dragon
120 2  corpses
59  2  human_transport_empty
60  2  orc_transport_empty
126 2  human_transport_full
127 2  orc_transport_full
# --- combat roster (identified visually @ frame 5, paired human/orc) ---
45  2  footman
46  2  grunt
49  2  ballista
50  2  catapult
51  2  knight
52  2  ogre
53  2  archer
54  2  axethrower
55  2  mage
58  2  death_knight
# --- ships / flyers / special (index fallback — verify + rename) ---
33  2  unit_033
34  2  unit_034
37  2  unit_037
38  2  unit_038
39  2  unit_039
40  2  unit_040
41  2  unit_041
42  2  unit_042
43  2  unit_043
44  2  unit_044
61  2  unit_061
62  2  unit_062
63  2  unit_063
69  2  unit_069
70  2  unit_070
# --- tileset-specific neutral critters (different palettes) ---
64  2  critter_summer
65  10 critter_wasteland
66  18 critter_winter
# --- summer buildings (palette 2; even idx = human, odd = orc per Todo[]) ---
# names by index for now; rename to town_hall/farm/barracks/... once confirmed.
80  2  bldg_human_080
81  2  bldg_orc_081
82  2  bldg_human_082
83  2  bldg_orc_083
84  2  bldg_human_084
85  2  bldg_orc_085
86  2  bldg_human_086
87  2  bldg_orc_087
88  2  bldg_human_088
89  2  bldg_orc_089
90  2  bldg_human_090
91  2  bldg_orc_091
92  2  bldg_human_092
93  2  bldg_orc_093
94  2  bldg_human_094
95  2  bldg_orc_095
96  2  bldg_human_096
97  2  bldg_orc_097
98  2  bldg_human_098
99  2  bldg_orc_099
100 2  bldg_human_100
101 2  bldg_orc_101
102 2  bldg_human_102
103 2  bldg_orc_103
104 2  bldg_human_104
105 2  bldg_orc_105
106 2  bldg_human_106
107 2  bldg_orc_107
108 2  bldg_human_108
109 2  bldg_orc_109
110 2  bldg_human_110
111 2  bldg_orc_111
112 2  bldg_human_112
113 2  bldg_orc_113
114 2  bldg_human_114
115 2  bldg_orc_115
116 2  bldg_human_116
117 2  bldg_orc_117
118 2  bldg_neutral_118
119 2  bldg_neutral_119
TABLE

# --- named buildings (palette 2 summer; identified visually, idx from
# war2_maindat_index.md). Orc mirrors human at idx+1. Construction uses the
# generic land construction-site sprite (idx 252) aliased per building. ---
BH="$OUT/buildings/human"; BO="$OUT/buildings/orc"; BN="$OUT/buildings/neutral"
mkdir -p "$BH" "$BO" "$BN"
for e in town_hall:100 farm:92 barracks:102 lumber_mill:104 blacksmith:90 \
         stable:108 church:96 tower:80 stormwind_keep:116; do
    "$TOOL" "$WAR" 2 "${e##*:}" "$BH/${e%%:*}.png" >/dev/null
done
for e in town_hall:101 farm:93 barracks:103 lumber_mill:105 blacksmith:91 \
         kennel:109 temple:97 tower:81 blackrock_spire:117; do
    "$TOOL" "$WAR" 2 "${e##*:}" "$BO/${e%%:*}.png" >/dev/null
done
"$TOOL" "$WAR" 2 119 "$BN/gold_mine.png" >/dev/null
"$TOOL" "$WAR" 2 252 "$BH/_construction_site.png" >/dev/null
cp "$BH/_construction_site.json" "$BO/_construction_site.json"
cp "$BH/_construction_site.png"  "$BO/_construction_site.png"
for n in town_hall farm barracks lumber_mill blacksmith stable church tower; do
    cp "$BH/_construction_site.json" "$BH/${n}_construction.json"
done
for n in town_hall farm barracks lumber_mill blacksmith kennel temple tower; do
    cp "$BO/_construction_site.json" "$BO/${n}_construction.json"
done
echo "buildings -> $OUT/buildings/{human,orc,neutral}"

# --- HUD chrome: portrait icons (graphic, 196 unit/building icons) +
# UI panels (Image type: button panel / resource bar / status line). ---
HF="$OUT/hud/forest"; HH="$OUT/hud/human"; HO="$OUT/hud/orc"
mkdir -p "$HF" "$HH" "$HO"
"$TOOL"    "$WAR" 2 356 "$HF/portrait_icons.png"   >/dev/null
"$IMGTOOL" "$WAR" 2 287 "$HH/top_resource_bar.png" >/dev/null
"$IMGTOOL" "$WAR" 2 291 "$HH/bottom_panel.png"     >/dev/null
"$IMGTOOL" "$WAR" 2 297 "$HH/buttonpanel.png"      >/dev/null
"$IMGTOOL" "$WAR" 2 298 "$HO/buttonpanel.png"      >/dev/null
echo "hud -> $OUT/hud/{forest,human,orc}"

echo "war2_extract_rts2: $count sprites -> $OUT"
