#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
set -u

TEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$TEST/../../../../../.." && pwd)"
RENDER="$TOP/src/cu/engine/render"
source "$TOP/utils/maint/engine/build_stamp.sh"
# the test is a program of the platform, which writes its lines through MMgr's line
source "$TOP/includes/exact_types/exact_types.sh"
build_stamp orior_raster_reduce_test

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) BINARY="$OUT/orior_raster_reduce_test.exe" ;;
    *) BINARY="$OUT/orior_raster_reduce_test" ;;
esac

rm -f "$BINARY"
# the host arms alone: the build carries no device renderer, and the device entry points answer 0
cc -std=c11 "${FILE_DEFS_LTO_CC[@]}" -Wall -Wextra -O2 -I "$RENDER" -o "$BINARY" "$TEST/orior_raster_reduce_test.c" "$RENDER/orior_raster.c" \
    "$RENDER/orior_raster_output.c" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}"
[ -f "$BINARY" ] || { echo "  build failed: cc could not build the test"; exit 1; }

"$BINARY"
STATUS=$?
echo "  orior_raster reduce test exit $STATUS"
exit "$STATUS"
