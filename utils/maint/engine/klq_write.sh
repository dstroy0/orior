#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs klq_write, which writes Lstar.klq, the bridge between languages, and each given language's .klm
# beside the rulesets (klq_write.cu). The code generator is host code in .cu files, compiled as C++, and needs no CUDA
# toolchain and no device
#
#     utils/maint/engine/klq_write.sh
#     utils/maint/engine/klq_write.sh cu.krs sass.krs
#
# With no arguments it maps cu.krs, sass.krs and openqasm2_0.krs.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
mkdir -p "$OUT"
CODEGEN="$TOP/src/cu/transpiler/codegen"
PARSER="$TOP/src/cu/transpiler/lstar/parser"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$PARSER")
BINARY="$OUT/klq_write"
rm -f "$BINARY"
OBJECTS=()
for source in "$CODEGEN"/*.cu "$PARSER"/*.cu "$PROTOCOL/klq_write.cu"; do
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
"$BINARY" "$TOP/src/cu/transpiler/lstar/coherence" "$@"
