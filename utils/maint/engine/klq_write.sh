#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs klq_write, which writes Lstar.klq, the bridge between languages, and each given language's .klm
# beside the rulesets (klq_write.cu). The code generator is host code in .cu files, compiled as C++, and needs no CUDA
# toolchain and no device. It is one query run, query<n>-<datetime>.qry in build/engine/klq/qry (qry_run.sh), which
# holds every ruleset it reads and the bridge as it read it, and hands the bridge and each map to the run's writer,
# which writes them in their places, and what klq_write prints, its blob klq_write.console (qry_writer tee). The
# script exits as klq_write did.
#
#     utils/maint/engine/klq_write.sh
#     utils/maint/engine/klq_write.sh cu.krs sass.krs
#
# With no arguments it maps cu.krs, sass.krs and openqasm2_0.krs.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/klq"
mkdir -p "$WORK"
CODEGEN="$TOP/src/cu/engine/rmc"
PARSER="$TOP/src/cu/types/file_defs/readers"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"
COHERENCE="$PROTOCOL/table"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
qry_writer_built "$OUT"

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$PARSER" -I "$QRY_SOURCE")
BINARY="$OUT/klq_write"
rm -f "$BINARY"
OBJECTS=("$OUT/qry_buffer_klq.o")
rm -f "${OBJECTS[0]}"
cc -std=c11 -O2 -Wall -Wextra -c "$QRY_SOURCE/qry_buffer.c" -o "${OBJECTS[0]}" ||
    { echo "  build failed: qry_buffer.c did not compile"; exit 1; }
for source in "$CODEGEN"/*.cu "$PARSER"/*.cu "$PROTOCOL/table/klq_write.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu|codegen*.cu) continue ;;
    esac
    object="$OUT/$(basename "$source")_klq.o"
    rm -f "$object"
    c++ -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
c++ -o "$BINARY" "${OBJECTS[@]}" -static
[ -f "$BINARY" ] || { echo "  build failed: klq_write did not link"; exit 1; }

if [ "$#" -eq 0 ]; then
    set -- cu.krs sass.krs openqasm2_0.krs
fi
run_started "$WORK/qry"
for given in "$@"; do
    run_read "$COHERENCE/$given"
done
run_read "$COHERENCE/Lstar.klq"
"$BINARY" "$COHERENCE" "$@" 2>&1 | "$QRY_WRITER" tee klq_write
exit "${PIPESTATUS[0]}"
