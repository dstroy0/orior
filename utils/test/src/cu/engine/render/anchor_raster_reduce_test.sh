#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../.." && pwd)"
RENDER="$TOP/src/cu/engine/render"
source "$TOP/utils/maint/engine/build_stamp.sh"
build_stamp anchor_raster_reduce_test

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) BINARY="$OUT/anchor_raster_reduce_test.exe" ;;
    *) BINARY="$OUT/anchor_raster_reduce_test" ;;
esac

rm -f "$BINARY"
# the host arms alone: the build carries no device renderer, and the device entry points answer 0
cc -std=c11 -Wall -Wextra -O2 -I "$RENDER" -o "$BINARY" "$TEST/anchor_raster_reduce_test.c" "$RENDER/anchor_raster.c" \
    "$RENDER/anchor_raster_output.c"
[ -f "$BINARY" ] || { echo "  build failed: cc could not build the test"; exit 1; }

"$BINARY"
STATUS=$?
echo "  anchor_raster reduce test exit $STATUS"
exit "$STATUS"
