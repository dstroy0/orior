#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# gui_test.sh: the window's C checked without a display: the command line's library built for this machine by
# ui/cli/build.sh, then each *_test.c here compiled against it with the window's modules that include no system header
# and the checks' cli_check.c, and run from build/gui_test, its exit said as the harness reads it; the checks named
# after it alone where any are, and a program of its own here, the frame bench, by its name. The compiler is CC where
# it is set, else clang.
set -u
# The checks are guests of the machine: they run niced, below the window and the host's own work.
if [ -z "${ORIOR_GUEST:-}" ]; then
    ORIOR_GUEST=1 exec nice -n 9 "$0" "$@"
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
CLI_CHECKS="$TOP/utils/test/src/ui/cli"
WINDOW="$TOP/ui/gui/src"
CC="${CC:-clang}"

# the system's body for this machine joins the modules, with the libraries it links: the body is checked too
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST="x86_64-pc-windows-msvc"; SUFFIX=".exe"; BODY="system_windows.c"
                          LIBRARIES=(-lws2_32 -lsecur32 -lcrypt32 -lshell32 -ladvapi32 -luser32 -lole32 -luuid -lgdi32 -ldwmapi -ld3d12 -ldxgi -ldxguid) ;;
    Darwin) HOST="$(uname -m)-apple-darwin"; SUFFIX=""; BODY=""; LIBRARIES=(-lpthread -lm) ;;
    *) HOST="$(uname -m)-unknown-linux-gnu"; SUFFIX=""; BODY="system_x11.c"; LIBRARIES=(-lpthread -ldl -lm -lrt -lX11) ;;
esac
if [ "$(uname -s)" = "Linux" ] && ldd --version 2>&1 | grep -qi musl; then
    HOST="$(uname -m)-alpine-linux-musl"
    LIBRARIES=(-static -lpthread -lm)
fi

# MMgr's two include roots and embedded_types' headers, and the capabilities MMgr is built with, as ui/cli/build.sh
# gives them: a check reaching a CLI header sees the module layouts the library was compiled to
MMGR="$TOP/src/cu/includes/mmgr"
MMGR_WORDS=(-I "$MMGR/include" -I "$MMGR/src" -I "$TOP/src/cu/includes/embedded_types/include" -DMMGR_ENABLE_DMA=0
    -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ENABLE_HOST=1 -DMMGR_ALIGN_BYTES=32 -DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1
    -DMMGR_CARCER_OWNERS=1024u)

source "$HERE/../sanitized.sh"

bash "$TOP/ui/cli/build.sh" library "$HOST" || { echo "test exit 1"; exit 1; }
CLI_NAME="$(awk -F'\t' '$1 == "cli" { print $2 }' "$TOP/bin_names.tsv")"
LIBRARY="$TOP/build/cli/$HOST$SANITIZE_SUFFIX/lib$CLI_NAME.a"
RUN="$TOP/build/gui_test$SANITIZE_SUFFIX"
mkdir -p "$RUN"
sanitized_checks_begin
# the checks keep what they keep in a folder of their own, made fresh each run: nothing of the reader's is read or
# written
export ORIOR_HOME="$RUN/home"
rm -rf "$ORIOR_HOME"
mkdir -p "$ORIOR_HOME"

# the window's modules a check may reach: every source but the program's own and the other systems' bodies
MODULES=()
while IFS= read -r source; do
    case "$source" in
        */main.c) ;;
        */system_*.c) [ "$(basename "$source")" = "$BODY" ] && MODULES+=("$source") ;;
        *) MODULES+=("$source") ;;
    esac
done < <(find "$WINDOW" -name '*.c' | sort)

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

for test in "${TESTS[@]}"; do
    name="$(basename "$test" .c)"
    echo "== $name"
    # a check is built lightly optimized for its build time; a program of its own, the bench, at the window's own -O2
    case "$test" in
        *_test.c) OPTIMIZED=-O1 ;;
        *) OPTIMIZED=-O2 ;;
    esac
    if ! "$CC" "--target=$HOST" -std=c11 "$OPTIMIZED" -g "${SANITIZE_WORDS[@]}" "${MMGR_WORDS[@]}" -o "$RUN/$name$SUFFIX" "$test" "$CLI_CHECKS/cli_check.c" \
        "${MODULES[@]}" "$LIBRARY" "${LIBRARIES[@]}" > "$RUN/$name.log" 2>&1; then
        echo "  build failed: $name did not compile"
        tail -20 "$RUN/$name.log"
        echo "test exit 1"
        continue
    fi
    (cd "$RUN" && "./$name$SUFFIX")
    sanitized_exit "$name" "$RUN" "$?"
    echo "test exit $?"
done
