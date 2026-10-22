#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# cli_bench.sh: the command line's common paths timed: its library built by ui/cli/build.sh, bench/cli_bench.c
# compiled against it at -O2, and run over the corpus, the C and headers under ui/cli/src, from build/cli_test.
# Each row it writes is kept in build/harness/bench/bench.tsv with the run's stamp and label before it, so a run
# before a change stands beside the run after it. The first argument is the label, the second the repeats.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
CC="${CC:-clang}"
LABEL="${1:-}"
REPEAT="${2:-5}"
# LTO=1 benches the library built as bitcode and linked whole, from build/cli/<host>-lto
LTO="${LTO:-0}"
LTO_SUFFIX=""
LTO_WORDS=()
if [ "$LTO" = 1 ]; then
    LTO_SUFFIX="-lto"
    LTO_WORDS=(-flto=thin -fuse-ld=lld)
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST="x86_64-pc-windows-msvc"; SUFFIX=".exe"
                          LIBRARIES=(-lws2_32 -lsecur32 -lcrypt32 -lshell32 -ladvapi32 -luser32) ;;
    Darwin) HOST="$(uname -m)-apple-darwin"; SUFFIX=""; LIBRARIES=(-lpthread -lm) ;;
    *) HOST="$(uname -m)-unknown-linux-gnu"; SUFFIX=""; LIBRARIES=(-lpthread -ldl -lm -lrt) ;;
esac
MMGR="$TOP/src/cu/includes/mmgr"
MMGR_WORDS=(-I "$MMGR/include" -I "$MMGR/src" -I "$TOP/src/cu/includes/embedded_types/include" -DMMGR_ENABLE_DMA=0
    -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ENABLE_HOST=1 -DMMGR_ALIGN_BYTES=32 -DMMGR_ENABLE_HW_MEM_CAPACITY_CB=1)

bash "$TOP/ui/cli/build.sh" library "$HOST" || { echo "bench exit 1"; exit 1; }
CLI_NAME="$(awk -F'\t' '$1 == "cli" { print $2 }' "$TOP/bin_names.tsv")"
LIBRARY="$TOP/build/cli/$HOST$LTO_SUFFIX/lib$CLI_NAME.a"
RUN="$TOP/build/cli_test"
KEPT="$TOP/build/harness/bench"
mkdir -p "$RUN" "$KEPT"

if ! "$CC" "--target=$HOST" -std=c11 -O2 "${LTO_WORDS[@]}" "${MMGR_WORDS[@]}" -o "$RUN/cli_bench$LTO_SUFFIX$SUFFIX" "$HERE/bench/cli_bench.c" "$HERE/bench/cli_bench_rules.c" "$HERE/bench/cli_bench_files.c" "$LIBRARY" \
    "${LIBRARIES[@]}" > "$RUN/cli_bench.log" 2>&1; then
    echo "  build failed: cli_bench did not compile"
    tail -20 "$RUN/cli_bench.log"
    echo "bench exit 1"
    exit 1
fi

CORPUS="$RUN/bench_corpus.txt"
TREE="$TOP"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) TREE="$(cygpath -m "$TOP")" ;; esac
(cd "$TOP" && find ui/cli/src -name '*.c' -o -name '*.h' | sort | sed "s|^|$TREE/|") > "$CORPUS"
STAMP="$(date +%Y%m%d-%H%M%S)"
echo "== cli_bench $STAMP $LABEL"
printf 'name\tbytes\tus\tMiB_s\n'
ROWS="$RUN/bench_rows.tsv"
(cd "$RUN" && "./cli_bench$LTO_SUFFIX$SUFFIX" "$CORPUS" "$TREE" "$REPEAT") > "$ROWS"
CODE=$?
cat "$ROWS"
sed "s|^|$STAMP\t$LABEL\t|" "$ROWS" >> "$KEPT/bench.tsv"
echo "bench exit $CODE"
