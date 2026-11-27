#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# gui_test.sh: the window's C checked, one check alone opening a window of its own. ui/cli/build.sh builds the command
# line's library for this machine. The view's modules, the window's, its system body and the checks' cli_check.c are
# compiled once, on all processors. Each *_test.c here is compiled and linked against them and the library, several at
# once. Then each check runs from build/gui_test, one at a time, and its exit is said as the harness reads it. The
# checks named after it run alone where any are, and a program of its own here, the frame bench, by its name. The
# compiler is CC where it is set, else clang.
set -u
# The checks are guests of the machine: they run niced, below the window and the host's own work.
if [ -z "${ORIOR_GUEST:-}" ]; then
    ORIOR_GUEST=1 exec nice -n 9 "$0" "$@"
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
CLI_CHECKS="$TOP/utils/test/src/ui/cli"
WINDOW="$TOP/ui/gui/src"
VIEW=("$TOP/includes/arith" "$TOP/includes/utf8" "$TOP/includes/coverage" "$TOP/includes/font" "$TOP/includes/raster" "$TOP/includes/frame_buffer" "$TOP/includes/glass" "$TOP/includes/window" "$TOP/includes/compositor")
CC="${CC:-clang}"

# the system's body for this machine joins the modules, with the libraries it links. The checks test the body too
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST="x86_64-pc-windows-msvc"; SUFFIX=".exe"; BODY="$TOP/includes/platform/windows/windows_window.c"
                          LIBRARIES=(-lws2_32 -lsynchronization -lsecur32 -lcrypt32 -lshell32 -ladvapi32 -luser32 -lole32 -luuid -lgdi32 -ldwmapi -ld3d12 -ldxgi -ldxguid) ;;
    Darwin) HOST="$(uname -m)-apple-darwin"; SUFFIX=""; BODY=""; LIBRARIES=(-lpthread -lm) ;;
    *) HOST="$(uname -m)-unknown-linux-gnu"; SUFFIX=""; BODY="$TOP/includes/platform/posix/linux/x11_window.c"; LIBRARIES=(-lpthread -ldl -lm -lrt -lX11) ;;
esac
if [ "$(uname -s)" = "Linux" ] && ldd --version 2>&1 | grep -qi musl; then
    HOST="$(uname -m)-alpine-linux-musl"
    LIBRARIES=(-static -lpthread -lm)
fi

# MMgr's two include roots, embedded_types' headers, and the capabilities that ui/cli/build.sh gives MMgr. A check
# that includes a CLI header then sees the module layouts that the library has
MMGR="$TOP/includes/mmgr"
MMGR_WORDS=(-I "$MMGR/include" -I "$MMGR/src" -I "$TOP/includes/embedded_types/include" -DMMGR_ENABLE_DMA=0
    -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ALIGN_BYTES=32 -DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1
    -DMMGR_CARCER_OWNERS=1024u
    -DMMGR_CARCER_REGIONS=16u -DMMGR_CARCER_REGION_BYTES=4194304u -DMMGR_CARCER_LARGE_REGION_BYTES=68157440u)
# the window block's parts of the program's own, as ui/cli/build.sh gives them. A check sees the block that the
# library has
WINDOW_WORDS=('-DGUI_BLOCK_PARTS_OWN(part_)=part_(TAB_BEADS,2u)')

source "$HERE/../sanitized.sh"
source "$TOP/ui/cli/lto_words.sh"

bash "$TOP/ui/cli/build.sh" library "$HOST" || { echo "test exit 1"; exit 1; }
# the program too, from the same objects: the sessions check starts it as a session's far end
bash "$TOP/ui/cli/build.sh" "$HOST" || { echo "test exit 1"; exit 1; }
CLI_NAME="$(awk -F'\t' '$1 == "cli" { print $2 }' "$TOP/bin_names.tsv")"
LIBRARY="$TOP/build/cli/$HOST$SANITIZE_SUFFIX/lib$CLI_NAME.a"
export ORIOR_IMP="$TOP/build/cli/$HOST$SANITIZE_SUFFIX/$CLI_NAME$SUFFIX"
RUN="$TOP/build/gui_test$SANITIZE_SUFFIX"
mkdir -p "$RUN"
sanitized_checks_begin
# Each check saves its state in a new folder of its own. No check reads or writes the reader's state, and no check
# reads what another saved
export ORIOR_HOME="$RUN/home"
# the checks run the tree's language server, which reads the database under build/, and a tree new to this machine has none
if [ ! -f "$TOP/build/compile_commands.json" ]; then
    "$(command -v python3 || command -v python)" "$TOP/utils/maint/engine/clangd_database.py" > /dev/null || exit 1
fi

# The modules a check can call, compiled here with the check's clock. They are every source of the view and the
# window, without the program's own. They also take the body of this system from the folders of the platform
MODULES=()
[ -n "$BODY" ] && MODULES+=("$BODY")
while IFS= read -r source; do
    case "$source" in
        */main.c) ;;
        *) MODULES+=("$source") ;;
    esac
done < <(find "$WINDOW" "${VIEW[@]}" -name '*.c' | sort)

# a name is a check, `<name>_test.c`, or a program of its own here, `<name>.c`, as the frame bench is
TESTS=()
for named in "$@"; do
    if [ -f "$HERE/$named.c" ]; then
        TESTS+=("$HERE/$named.c")
    else
        TESTS+=("$HERE/${named}_test.c")
    fi
done
[ "${#TESTS[@]}" -gt 0 ] || TESTS=("$HERE"/*_test.c)

# A check takes -O1, which builds fast. A program of its own here, the bench, takes the window's own -O2
optimized_of() {
    case "$1" in
        *_test.c) echo -O1 ;;
        *) echo -O2 ;;
    esac
}
COMPILE_WORDS=("--target=$HOST" -std=c11 -g "${LTO_WORDS[@]}" "${SANITIZE_WORDS[@]}" "${MMGR_WORDS[@]}" "${WINDOW_WORDS[@]}"
    -Dcli_clock_milliseconds=test_clock_milliseconds)

# The jobs that run at once: a compile for each processor, and fewer links, as each link optimizes the whole program on
# all processors itself
PROCESSORS="$(nproc 2>/dev/null || echo 4)"
LINKS=$(( PROCESSORS / 4 ))
[ "$LINKS" -ge 2 ] || LINKS=2

# "$@" run in the background after a job ends, where $LIMIT jobs run already
pool_run() {
    while [ "$(jobs -rp | wc -l)" -ge "$LIMIT" ]; do
        wait -n
    done
    "$@" &
}

# The modules, the checks' support and the check clock are the same in each check. The script compiles them once at
# each optimization that a check takes, each object under a name from its path. Each check links them
OBJECTS="$RUN/objects"
SHARED=("${MODULES[@]}" "$CLI_CHECKS/cli_check.c" "$HERE/test_clock.c")
compile_one() {
    local optimized="$1" source="$2" object="$3"
    "$CC" "${COMPILE_WORDS[@]}" "$optimized" -c -o "$object" "$source" > "$object.log" 2>&1 || : > "$object.failed"
}
# the objects of each optimization, OBJECTS_O1 and OBJECTS_O2, empty until a check takes that optimization
OBJECTS_O1=()
OBJECTS_O2=()
LIMIT="$PROCESSORS"
for test in "${TESTS[@]}"; do
    optimized="$(optimized_of "$test")"
    declare -n level="OBJECTS_${optimized#-}"
    if [ "${#level[@]}" -eq 0 ]; then
        folder="$OBJECTS/${optimized#-}"
        rm -rf "$folder"
        mkdir -p "$folder"
        for source in "${SHARED[@]}"; do
            relative="${source#"$TOP"/}"
            object="$folder/${relative//\//_}.o"
            level+=("$object")
            pool_run compile_one "$optimized" "$source" "$object"
        done
    fi
    unset -n level
done
wait
SHARED_FAILED=0
for failed in "$OBJECTS"/*/*.failed; do
    [ -f "$failed" ] || continue
    echo "  build failed: ${failed%.o.failed} did not compile"
    tail -20 "${failed%.failed}.log"
    SHARED_FAILED=1
done
if [ "$SHARED_FAILED" -ne 0 ]; then
    echo "test exit 1"
    exit 1
fi

# Each check compiled and linked against the shared objects, the links at once to $LINKS. A check that does not build
# leaves its log and a mark
link_one() {
    local test="$1" name="$2" optimized="$3"
    local -n objects="OBJECTS_${optimized#-}"
    "$CC" "${COMPILE_WORDS[@]}" "$optimized" "${LINK_WORDS[@]}" -o "$RUN/$name$SUFFIX" "$test" "${objects[@]}" \
        "$LIBRARY" "${LIBRARIES[@]}" > "$RUN/$name.log" 2>&1 || : > "$RUN/$name.failed"
}
LIMIT="$LINKS"
for test in "${TESTS[@]}"; do
    name="$(basename "$test" .c)"
    rm -f "$RUN/$name.failed"
    pool_run link_one "$test" "$name" "$(optimized_of "$test")"
done
wait

# The checks run one at a time, in their order, as a check can open a window of its own
for test in "${TESTS[@]}"; do
    name="$(basename "$test" .c)"
    echo "== $name"
    if [ -f "$RUN/$name.failed" ]; then
        echo "  build failed: $name did not compile"
        tail -20 "$RUN/$name.log"
        echo "test exit 1"
        continue
    fi
    rm -rf "$ORIOR_HOME"
    mkdir -p "$ORIOR_HOME"
    (cd "$RUN" && "./$name$SUFFIX")
    sanitized_exit "$name" "$RUN" "$?"
    echo "test exit $?"
done
