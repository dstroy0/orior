#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# build.sh: orior-cli built from the tree's C for the target given, the machine's own where none is, into
# build/cli/<target>/orior-cli. The bundler is built for this machine first and writes plugins_bundled.c from the
# plugins, toolchains.json and commands.json; then every source is compiled for the target, 16 at once, each only
# where it changed since its object was made. The compiler is CC where it is set, else clang; a musl target is linked
# whole, with no library left to load. `build.sh library [target]` archives the same objects, but the program's entry,
# into build/cli/<target>/liborior.a, and orior.lib beside it for an MSVC target, for the window to link. MMgr is
# built for each target by its own CMake into build/mmgr/<target>, and the objects of its modules join the CLI's.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
CC="${CC:-clang}"
AR="${AR:-llvm-ar}"
JOBS=16

# MMgr's two include roots and embedded_types' headers, and the capabilities MMgr is built with, so a CLI source
# reaching a module header sees the layouts the module was compiled to; each cellblock keeps its high-water mark, in
# the CLI's objects and in MMgr's alike, since the mark is a member of every cellblock's state
MMGR="$TOP/src/cu/includes/mmgr"
EMBEDDED_TYPES="$TOP/src/cu/includes/embedded_types"
MMGR_HIGH_WATER=-DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1
MMGR_WORDS=(-I "$MMGR/include" -I "$MMGR/src" -I "$EMBEDDED_TYPES/include" -DMMGR_ENABLE_DMA=0
    -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ALIGN_BYTES=32 "$MMGR_HIGH_WATER")

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST="x86_64-pc-windows-msvc" ;;
    Darwin) HOST="$(uname -m)-apple-darwin" ;;
    *) HOST="$(uname -m)-unknown-linux-gnu" ;;
esac
LIBRARY=0
if [ "${1:-}" = "library" ]; then
    LIBRARY=1
    shift
fi
TARGET="${1:-$HOST}"

SOURCES=()
while IFS= read -r source; do
    [ "$source" != "$HERE/src/command_line/orior_cli.c" ] && SOURCES+=("$source")
done < <(find "$TOP/ui/cli/src" "$TOP/ui/query/src" -name '*.c' -not -path '*/bundle/*' | sort)
for folder in src/cu/includes/file_defs/qry/writer src/cu/includes/file_defs/qry/writer/platform; do
    for source in "$TOP/$folder"/*.c; do
        [ -f "$source" ] && SOURCES+=("$source")
    done
done
SOURCES+=("$TOP/src/cu/includes/file_defs/qry/qry_read.c" "$TOP/src/cu/includes/formats/cfg_json/cfg_json.c"
          "$TOP/src/cu/includes/parsers/line_read.c" "$TOP/src/cu/includes/file_defs/ksg/ksg_seal.c"
          "$TOP/src/cu/includes/file_defs/ksg/ksg_writing.c")

# the libraries a target's program links, and the name its program takes
words_of() {
    local target="$1"
    case "$target" in
        *windows*) SUFFIX=".exe"; LIBRARIES=(-lws2_32 -lsecur32 -lcrypt32 -lshell32 -ladvapi32 -luser32) ;;
        *darwin*) SUFFIX=""; LIBRARIES=(-lpthread -lm) ;;
        *musl*) SUFFIX=""; LIBRARIES=(-static -lpthread -lm) ;;
        *) SUFFIX=""; LIBRARIES=(-lpthread -ldl -lm -lrt) ;;
    esac
    WORDS=("$CC" "--target=$target" -std=c11 -O2 -g -c "${MMGR_WORDS[@]}")
}

# MMgr built for `target` by its own CMake into build/mmgr/<target>, its suites left to its own runs, with
# embedded_types taken from the tree's submodule; the objects of its host environment's modules into MMGR_OBJECTS
mmgr_built() {
    local target="$1"
    local into="$TOP/build/mmgr/$target"
    mkdir -p "$TOP/build/mmgr"
    { cmake -S "$MMGR" -B "$into" -G Ninja -DCMAKE_C_COMPILER="$CC" -DCMAKE_C_COMPILER_TARGET="$target" \
        -DCMAKE_BUILD_TYPE=Release -DMMGR_BUILD_TESTS=OFF -DCMAKE_C_FLAGS="$MMGR_HIGH_WATER" \
        -DFETCHCONTENT_SOURCE_DIR_EMBEDDED_TYPES="$EMBEDDED_TYPES" && cmake --build "$into"; } > "$into.log" 2>&1 || {
        echo "  build failed: MMgr did not build for $target"
        tail -20 "$into.log"
        exit 1
    }
    MMGR_OBJECTS=()
    while IFS= read -r object; do
        MMGR_OBJECTS+=("$object")
    done < <(find "$into/src" -path '*_host.dir/*' \( -name '*.obj' -o -name '*.o' \) | sort)
}

# each source compiled for `target` into `objects`, those whose object is older than the source or any header of the
# tree's; the objects into OBJECTS
compiled() {
    local target="$1" objects="$2"
    shift 2
    words_of "$target"
    mkdir -p "$objects"
    local newest
    newest="$(find "$TOP/ui" "$TOP/src/cu/includes" -name '*.h' \
              -newer "$objects" 2>/dev/null | head -1)"
    OBJECTS=()
    local failed="$objects/failed"
    rm -f "$failed"
    for source in "$@"; do
        local relative="${source#"$TOP/"}"
        local object="$objects/${relative//\//__}.o"
        OBJECTS+=("$object")
        if [ -n "$newest" ] || [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then
            while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do
                wait -n
            done
            ( "${WORDS[@]}" -o "$object" "$source" > "$object.log" 2>&1 || echo "$source $object.log" >> "$failed" ) &
        fi
    done
    wait
    touch "$objects"
    if [ -f "$failed" ]; then
        while read -r source log; do
            echo "  build failed: ${source#"$TOP/"} did not compile"
            tail -20 "$log"
        done < "$failed"
        exit 1
    fi
}

# the objects after `listed`, each as a path from the tree's top, one a line into the file `listed`, which a program
# run from the top takes as @listed, so no command line grows past what Windows takes
listed_into() {
    local listed="$1"
    shift
    printf '%s\n' "${@#"$TOP/"}" > "$listed"
}

# the archive `archive` made anew of the objects after it
archived() {
    local archive="$1"
    shift
    rm -f "$archive"
    listed_into "$archive.objects" "$@"
    ( cd "$TOP" && "$AR" rcs "${archive#"$TOP/"}" "@${archive#"$TOP/"}.objects" )
}

OUT="$TOP/build/cli/$TARGET"
mkdir -p "$OUT"

# the bundler, built for this machine from the library it shares, run to write what comes with orior
BUNDLE="$TOP/build/cli/$HOST/bundle"
compiled "$HOST" "$TOP/build/cli/$HOST/objects" "${SOURCES[@]}"
mmgr_built "$HOST"
HOST_OBJECTS=("${OBJECTS[@]}" "${MMGR_OBJECTS[@]}")
words_of "$HOST"
mkdir -p "$BUNDLE"
archived "$BUNDLE/library.a" "${HOST_OBJECTS[@]}" || { echo "  build failed: the library was not archived"; exit 1; }
"${WORDS[@]}" -o "$BUNDLE/plugins_bundle.o" "$HERE/src/bundle/plugins_bundle.c" || exit 1
"$CC" "--target=$HOST" -o "$BUNDLE/plugins_bundle$SUFFIX" "$BUNDLE/plugins_bundle.o" "$BUNDLE/library.a" \
    "${LIBRARIES[@]}" || { echo "  build failed: the bundler did not link"; exit 1; }
"$BUNDLE/plugins_bundle$SUFFIX" "$TOP/ui/plugins" "$HERE/toolchains.json" "$HERE/commands.json" \
    "$OUT/plugins_bundled.c" || { echo "  build failed: the bundler did not write plugins_bundled.c"; exit 1; }

# the program, for the target
if [ "$TARGET" = "$HOST" ]; then
    TARGET_OBJECTS=("${HOST_OBJECTS[@]}")
else
    compiled "$TARGET" "$OUT/objects" "${SOURCES[@]}"
    mmgr_built "$TARGET"
    TARGET_OBJECTS=("${OBJECTS[@]}" "${MMGR_OBJECTS[@]}")
fi
words_of "$TARGET"
for source in "$OUT/plugins_bundled.c" "$HERE/src/command_line/orior_cli.c"; do
    name="$(basename "$source" .c)"
    "${WORDS[@]}" -I "$HERE/src" -o "$OUT/$name.o" "$source" || { echo "  build failed: $name.c did not compile"; exit 1; }
done
if [ "$LIBRARY" = 1 ]; then
    rm -f "$OUT/orior.lib"
    archived "$OUT/liborior.a" "$OUT/plugins_bundled.o" "${TARGET_OBJECTS[@]}" \
        || { echo "  build failed: the library was not archived"; exit 1; }
    case "$TARGET" in *msvc*) cp "$OUT/liborior.a" "$OUT/orior.lib" ;; esac
    echo "  built ${OUT#"$TOP/"}/liborior.a"
    exit 0
fi
listed_into "$OUT/orior-cli.objects" "$OUT/orior_cli.o" "$OUT/plugins_bundled.o" "${TARGET_OBJECTS[@]}"
( cd "$TOP" && "$CC" "--target=$TARGET" -o "${OUT#"$TOP/"}/orior-cli$SUFFIX" "@${OUT#"$TOP/"}/orior-cli.objects" \
    "${LIBRARIES[@]}" ) || { echo "  build failed: orior-cli did not link"; exit 1; }
echo "  built ${OUT#"$TOP/"}/orior-cli$SUFFIX"
