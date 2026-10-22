#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# cli_test.sh: orior's C side checked: its library built for this machine by ui/cli/build.sh, then each
# *_test.c here compiled against it, with cli_check.c, and run from build/cli_test, its exit said as the harness reads
# it; the checks named after it alone where any are. The compiler is CC where it is set, else clang.
set -u
# The checks are guests of the machine: they run niced, below the window and the host's own work.
if [ -z "${ORIOR_GUEST:-}" ]; then
    ORIOR_GUEST=1 exec nice -n 9 "$0" "$@"
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
CC="${CC:-clang}"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST="x86_64-pc-windows-msvc"; SUFFIX=".exe"
                          LIBRARIES=(-lws2_32 -lsecur32 -lcrypt32 -lshell32 -ladvapi32 -luser32) ;;
    Darwin) HOST="$(uname -m)-apple-darwin"; SUFFIX=""; LIBRARIES=(-lpthread -lm) ;;
    *) HOST="$(uname -m)-unknown-linux-gnu"; SUFFIX=""; LIBRARIES=(-lpthread -ldl -lm -lrt) ;;
esac
# a Linux whose C library is musl's, as Alpine's is: its own target, the program linked whole
if [ "$(uname -s)" = "Linux" ] && ldd --version 2>&1 | grep -qi musl; then
    HOST="$(uname -m)-alpine-linux-musl"
    LIBRARIES=(-static -lpthread -lm)
fi

# MMgr's two include roots and embedded_types' headers, and the capabilities MMgr is built with, as ui/cli/build.sh
# gives them; a check reaching a CLI header therefore sees the module layouts the library was compiled to
MMGR="$TOP/src/cu/includes/mmgr"
MMGR_WORDS=(-I "$MMGR/include" -I "$MMGR/src" -I "$TOP/src/cu/includes/embedded_types/include" -DMMGR_ENABLE_DMA=0
    -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ENABLE_HOST=1 -DMMGR_ALIGN_BYTES=32 -DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1)

bash "$TOP/ui/cli/build.sh" library "$HOST" || { echo "test exit 1"; exit 1; }
# the command line's library, named as bin_names.tsv at the top of the tree names the command line
CLI_NAME="$(awk -F'\t' '$1 == "cli" { print $2 }' "$TOP/bin_names.tsv")"
LIBRARY="$TOP/build/cli/$HOST/lib$CLI_NAME.a"
RUN="$TOP/build/cli_test"
mkdir -p "$RUN"

# the checks named on the command line, each as its file's name without _test.c, or every check where none is named
TESTS=()
for named in "$@"; do
    TESTS+=("$HERE/${named}_test.c")
done
[ "${#TESTS[@]}" -gt 0 ] || TESTS=("$HERE"/*_test.c)

# the repositories a check makes are its own: git signs nothing in them, whatever the reader's git signs
export GIT_CONFIG_COUNT=2 GIT_CONFIG_KEY_0=commit.gpgsign GIT_CONFIG_VALUE_0=false GIT_CONFIG_KEY_1=tag.gpgsign     GIT_CONFIG_VALUE_1=false

for test in "${TESTS[@]}"; do
    name="$(basename "$test" .c)"
    echo "== $name"
    if ! "$CC" "--target=$HOST" -std=c11 -O1 -g "${MMGR_WORDS[@]}" -o "$RUN/$name$SUFFIX" "$test" "$HERE/cli_check.c" "$LIBRARY" \
        "${LIBRARIES[@]}" > "$RUN/$name.log" 2>&1; then
        echo "  build failed: $name did not compile"
        tail -20 "$RUN/$name.log"
        echo "test exit 1"
        continue
    fi
    (cd "$RUN" && "./$name$SUFFIX")
    echo "test exit $?"
done
