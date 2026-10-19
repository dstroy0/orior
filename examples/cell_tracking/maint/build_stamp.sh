#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

# The engine is the repository's own, at src/cu/engine: every build reads it there, and a tree without it fails
# here, before anything compiles
ENGINE="$(cd "$TOP/.." && pwd)/src/cu/engine"

# the top of the repository TOP stands in, where every build goes under build/; TOP itself where git gives none
BUILD_ROOT="$(git -C "$TOP" rev-parse --show-toplevel 2> /dev/null)"
[ -n "$BUILD_ROOT" ] || BUILD_ROOT="$TOP"
if [ ! -f "$ENGINE/engine_config.h" ]; then
    echo "  build failed: no engine at $ENGINE"
    exit 1
fi

# a build's path: cu/..., cu/... and sims/... are the engine's, under src/, and anything else the project's
build_path()
{
    case "$1" in
        engine/*) printf '%s\n' "$ENGINE/${1#engine/}" ;;
        c/* | cu/* | sims/*) printf '%s\n' "$ENGINE/../../$1" ;;
        *) printf '%s\n' "$TOP/$1" ;;
    esac
}

build_stamp()
{
    local label="$1"
    if [ -n "${BUILD_OUT:-}" ]; then
        OUT="$BUILD_OUT"
        FINAL="$BUILD_OUT"
        mkdir -p "$OUT"
        return 0
    fi
    FINAL="$BUILD_ROOT/build"
    mkdir -p "$FINAL"
    find "$FINAL" -mindepth 1 -maxdepth 1 -type d \
        -name '[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]_[0-9][0-9][0-9][0-9][0-9][0-9]_*' \
        -mmin +1440 -exec rm -rf {} +
    OUT="$FINAL/$(date +%Y%m%d_%H%M%S)_$label"
    mkdir -p "$OUT"
    echo "  build directory: $OUT"
}

build_publish()
{
    local artifact="$1"
    [ "$FINAL" = "$OUT" ] && return 0
    cp -f "$artifact" "$FINAL/" || { echo "  build failed: could not copy $artifact to $FINAL (is it running?)"; return 1; }
    echo "  published $FINAL/$(basename "$artifact")"
}
