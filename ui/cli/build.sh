#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# build.sh: orior_cli built from the tree's C for the target given, the machine's own where none is, into
# build/cli/<target>/orior_cli. The bundler is built for this machine first and writes plugins_bundled.c from the
# plugins, toolchains.json and commands.json; then every source is compiled for the target, 16 at once, each only
# where it changed since its object was made. The compiler is CC where it is set, else clang; a musl target is linked
# whole, with no library left to load. `build.sh library [target]` archives the same objects, but the program's entry,
# into build/cli/<target>/lib$CLI_NAME.a, and $CLI_NAME.lib beside it for an MSVC target, for the window to link. MMgr is
# built for each target by orior's CMake into build/orior_cmake/<target>, and the objects of its modules join the CLI's.
set -u
# A build is a guest of the machine: it runs niced, below the window and the host's own work, which take the
# processor back from it the moment they want it.
if [ -z "${ORIOR_GUEST:-}" ]; then
    ORIOR_GUEST=1 exec nice -n 9 "$0" "$@"
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"

# the name of a program the tree builds, by its id in bin_names.tsv at the top of the tree, where every name is picked
bin_named() {
    awk -F'\t' -v id="$1" '$1 == id { print $2 }' "$TOP/bin_names.tsv"
}
CLI_NAME="$(bin_named cli)"
BUNDLE_NAME="$(bin_named plugins_bundle)"
CC="${CC:-clang}"
# the archiver: LLVM's where the machine has it, else the system's ar, which takes the same words
AR="${AR:-$(command -v llvm-ar > /dev/null 2>&1 && echo llvm-ar || echo ar)}"
JOBS=16
# LTO=1 builds every object as bitcode and links each program with the whole program optimized at its link, into
# build/cli/<target>-lto beside the plain build; the archive it leaves is bitcode, for an LTO link alone
LTO="${LTO:-0}"
LTO_SUFFIX=""
LTO_WORDS=()
LINK_WORDS=()
if [ "$LTO" = 1 ]; then
    LTO_SUFFIX="-lto"
    LTO_WORDS=(-flto=thin)
    LINK_WORDS=(-flto=thin -fuse-ld=lld)
fi
# ORIOR_SANITIZE=address or thread builds every object and link under that sanitizer, with frame pointers kept for
# its stacks and the code position-independent, since the sanitizer's constructors keep every object's sections in
# the window's shared library, into build/cli/<target>-<sanitizer> beside the plain build; the window's build links
# the same. Every report is recoverable, so a program run with halting off tells each one and goes on, and the
# bundler runs so here
SANITIZE="${ORIOR_SANITIZE:-}"
SANITIZE_WORDS=()
if [ -n "$SANITIZE" ]; then
    LTO_SUFFIX="$LTO_SUFFIX-$SANITIZE"
    SANITIZE_WORDS=("-fsanitize=$SANITIZE" "-fsanitize-recover=$SANITIZE" -fno-omit-frame-pointer -fPIC)
    LINK_WORDS+=("-fsanitize=$SANITIZE")
    export ASAN_OPTIONS="halt_on_error=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
    export TSAN_OPTIONS="halt_on_error=0${TSAN_OPTIONS:+:$TSAN_OPTIONS}"
fi

# MMgr's two include roots and embedded_types' headers, and the capabilities MMgr is built with, so a CLI source
# reaching a module header sees the layouts the module was compiled to; each cellblock keeps its high-water mark, in
# the CLI's objects and in MMgr's alike, since the mark is a member of every cellblock's state
MMGR="$TOP/src/cu/includes/mmgr"
EMBEDDED_TYPES="$TOP/src/cu/includes/embedded_types"
MMGR_HIGH_WATER=-DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1
MMGR_WORDS=(-I "$MMGR/include" -I "$MMGR/src" -I "$EMBEDDED_TYPES/include" -DMMGR_ENABLE_DMA=0
    -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ENABLE_HOST=1 -DMMGR_ALIGN_BYTES=32 "$MMGR_HIGH_WATER")

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
    # the program's own source is compiled apart, for the program alone, not into the library
    case "$source" in
        */command_line/orior_cli.c) ;;
        *) SOURCES+=("$source") ;;
    esac
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
# the vector unit the target's code is built for, into VECTOR_WORDS: ORIOR_VECTOR names one, `avx2` on x86_64, or
# `none` for the word walks alone; unset, the target's own baseline, SSE2 on x86_64 and NEON on aarch64, which MMgr
# finds on its own and which needs no word
vector_words_of() {
    local target="$1"
    VECTOR_WORDS=()
    case "${ORIOR_VECTOR:-}" in
        avx2) case "$target" in x86_64*) VECTOR_WORDS=(-mavx2) ;; esac ;;
        none) VECTOR_WORDS=(-DMMGR_VECTOR_OFF=1) ;;
    esac
}

words_of() {
    local target="$1"
    case "$target" in
        *windows*) SUFFIX=".exe"; LIBRARIES=(-lws2_32 -lsecur32 -lcrypt32 -lshell32 -ladvapi32 -luser32) ;;
        *darwin*) SUFFIX=""; LIBRARIES=(-lpthread -lm) ;;
        *musl*) SUFFIX=""; LIBRARIES=(-static -lpthread -lm) ;;
        *) SUFFIX=""; LIBRARIES=(-lpthread -ldl -lm -lrt) ;;
    esac
    vector_words_of "$target"
    WORDS=("$CC" "--target=$target" -std=c11 -O2 -g -c "${MMGR_WORDS[@]}" "${VECTOR_WORDS[@]}" "${LTO_WORDS[@]}"
           "${SANITIZE_WORDS[@]}")
}

# MMgr built for `target` by orior's CMake, src/cu, into build/orior_cmake/<target>, as orior builds it, with no CUDA
# looked for; only its orior_mmgr target is built; the objects of its host environment's modules into MMGR_OBJECTS
mmgr_built() {
    local target="$1"
    local into="$TOP/build/orior_cmake/$target$LTO_SUFFIX"
    mkdir -p "$TOP/build/orior_cmake"
    vector_words_of "$target"
    { cmake -S "$TOP/src/cu" -B "$into" -G Ninja -DCMAKE_C_COMPILER="$CC" -DCMAKE_C_COMPILER_TARGET="$target" \
        -DCMAKE_BUILD_TYPE=Release -DORIOR_SKIP_CUDA=ON -DORIOR_MMGR_ONLY=ON \
        -DCMAKE_C_FLAGS="$MMGR_HIGH_WATER -DMMGR_ENABLE_HOST=1 ${VECTOR_WORDS[*]} ${SANITIZE_WORDS[*]}" \
        && cmake --build "$into" --target orior_mmgr; } > "$into.log" 2>&1 || {
        echo "  build failed: MMgr did not build for $target"
        tail -20 "$into.log"
        exit 1
    }
    MMGR_OBJECTS=()
    while IFS= read -r object; do
        MMGR_OBJECTS+=("$object")
    done < <(find "$into/includes/mmgr/src" -path '*_host.dir/*' \( -name '*.obj' -o -name '*.o' \) | sort)
}

# each source compiled for `target` into `objects`, those whose object is older than the source, any header of the
# tree's or this script, whose words the objects are compiled with; the objects into OBJECTS
compiled() {
    local target="$1" objects="$2"
    shift 2
    words_of "$target"
    mkdir -p "$objects"
    local newest
    newest="$(find "$TOP/ui" "$TOP/src/cu/includes" -name '*.h' \
              -newer "$objects" 2>/dev/null | head -1)"
    if [ "${BASH_SOURCE[0]}" -nt "$objects" ]; then
        newest="${BASH_SOURCE[0]}"
    fi
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

OUT="$TOP/build/cli/$TARGET$LTO_SUFFIX"
mkdir -p "$OUT"

# the bundler, built for this machine from the library it shares, run to write what comes with orior
BUNDLE="$TOP/build/cli/$HOST$LTO_SUFFIX/bundle"
compiled "$HOST" "$TOP/build/cli/$HOST$LTO_SUFFIX/objects" "${SOURCES[@]}"
mmgr_built "$HOST"
HOST_OBJECTS=("${OBJECTS[@]}" "${MMGR_OBJECTS[@]}")
words_of "$HOST"
mkdir -p "$BUNDLE"
archived "$BUNDLE/library.a" "${HOST_OBJECTS[@]}" || { echo "  build failed: the library was not archived"; exit 1; }
"${WORDS[@]}" -o "$BUNDLE/plugins_bundle.o" "$HERE/src/bundle/plugins_bundle.c" || exit 1
"$CC" "--target=$HOST" "${LINK_WORDS[@]}" -o "$BUNDLE/$BUNDLE_NAME$SUFFIX" "$BUNDLE/plugins_bundle.o" "$BUNDLE/library.a" \
    "${LIBRARIES[@]}" || { echo "  build failed: the bundler did not link"; exit 1; }
"$BUNDLE/$BUNDLE_NAME$SUFFIX" "$TOP/ui/plugins" "$HERE/toolchains.json" "$HERE/commands.json" \
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
    rm -f "$OUT/$CLI_NAME.lib"
    archived "$OUT/lib$CLI_NAME.a" "$OUT/plugins_bundled.o" "${TARGET_OBJECTS[@]}" \
        || { echo "  build failed: the library was not archived"; exit 1; }
    case "$TARGET" in *msvc*) cp "$OUT/lib$CLI_NAME.a" "$OUT/$CLI_NAME.lib" ;; esac
    echo "  built ${OUT#"$TOP/"}/lib$CLI_NAME.a"
    exit 0
fi
listed_into "$OUT/$CLI_NAME.objects" "$OUT/orior_cli.o" "$OUT/plugins_bundled.o" "${TARGET_OBJECTS[@]}"
( cd "$TOP" && "$CC" "--target=$TARGET" "${LINK_WORDS[@]}" -o "${OUT#"$TOP/"}/$CLI_NAME$SUFFIX" "@${OUT#"$TOP/"}/$CLI_NAME.objects" \
    "${LIBRARIES[@]}" ) || { echo "  build failed: $CLI_NAME did not link"; exit 1; }
echo "  built ${OUT#"$TOP/"}/$CLI_NAME$SUFFIX"
