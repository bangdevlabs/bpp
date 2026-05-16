#!/bin/bash
# sprite16_to_atlas.sh — Combine N ModuLab sprite16 JSON files into a
# single atlas pack JSON consumable by `atlas_load_modulab`.
#
# Each input file is the one-sprite format ModuLab writes via
# `mlab_save_sprite_compat` (sprite16 schema). The output matches
# `mlab_save_atlas_pack`'s schema v1 — type "atlas_pack",
# palette "MCU-8", tile_w/tile_h taken from the first input,
# sprites array with one entry per input file.
#
# The sprite name is derived from the input filename: trailing
# "_sprite" is stripped, plus the .json extension. So
# `cat_sprite.json` → name "cat", `apple.json` → name "apple".
#
# Usage:
#   sprite16_to_atlas.sh OUT.json IN1.json [IN2.json ...]
#
# Example:
#   tools/sprite16_to_atlas.sh \
#     games/pathfind/assets/sprites/pathfind.json \
#     games/pathfind/assets/sprites/cat_sprite.json \
#     games/pathfind/assets/sprites/rat_sprite.json
#
# Future B++ tool — when this script accumulates real complexity
# (validation, palette overrides, frame ordering controls), port to
# `tools/sprite16_to_atlas.bpp` and retire this shell wrapper.

set -e

if [ $# -lt 2 ]; then
    echo "usage: $0 OUT.json IN1.json [IN2.json ...]" >&2
    exit 1
fi

OUT="$1"; shift

# Take tile dimensions from the first input. All sprites in a pack
# must agree on size; mismatched inputs would produce a broken pack
# at runtime — better to fail fast here than to surface it as a
# garbled atlas at game startup.
tw=$(jq -r .width  "$1")
th=$(jq -r .height "$1")
for f in "$@"; do
    fw=$(jq -r .width  "$f")
    fh=$(jq -r .height "$f")
    if [ "$fw" != "$tw" ] || [ "$fh" != "$th" ]; then
        echo "$0: $f tile size ${fw}x${fh} does not match pack ${tw}x${th}" >&2
        exit 2
    fi
done

{
    printf '{"type":"atlas_pack","version":1,"palette":"MCU-8","tile_w":%d,"tile_h":%d,"sprites":[' "$tw" "$th"
    first=1
    for f in "$@"; do
        name=$(basename "$f" .json | sed 's/_sprite$//')
        data=$(jq -c .data "$f")
        if [ "$first" = 1 ]; then first=0; else printf ','; fi
        printf '{"name":"%s","data":%s}' "$name" "$data"
    done
    printf ']}\n'
} > "$OUT"

echo "$0: wrote $OUT (${#@} sprites @ ${tw}x${th} MCU-8)" >&2
