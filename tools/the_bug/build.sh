#!/bin/sh
# Build the_bug GUI debugger from this directory.
#
# B++'s import resolver expects stb/ + src/ to exist relative to cwd
# (or via a global install at /usr/local/lib/bpp/). To keep the
# command short regardless of where the user invokes it from, this
# script cd's to the project root, runs bpp, and writes the binary
# back here under the canonical tool name.
#
# Usage:  sh tools/the_bug/build.sh
#         (or from anywhere: cd into tools/the_bug && sh build.sh)
#
# Output: tools/the_bug/the_bug

set -e

# Walk up from this script's directory until we find ./bpp (the
# project-root marker). Two levels up is the typical case
# (tools/the_bug/ -> repo root) but any depth works.
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$SCRIPT_DIR
while [ "$ROOT" != "/" ] && [ ! -x "$ROOT/bpp" ]; do
    ROOT=$(dirname "$ROOT")
done

if [ ! -x "$ROOT/bpp" ]; then
    echo "build.sh: cannot find ./bpp walking up from $SCRIPT_DIR" >&2
    exit 1
fi

cd "$ROOT"
./bpp tools/the_bug/the_bug.bpp -o tools/the_bug/the_bug
echo "built tools/the_bug/the_bug"
