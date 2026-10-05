#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs klq_identity over the measuring stick (klq_identity.cu): the stick's questions sliced into
# identities, the integer questions computed on the host over every case, and each identity's verdict written to
# Lstar.klq after its keys, beside the forms of the rulesets given. Reads the stick measuring_stick.sh last wrote. No
# device.
#
#     utils/maint/engine/klq_identity.sh
#     utils/maint/engine/klq_identity.sh sass.krs
#
# With no arguments the forms are sass.krs's.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/identity"
mkdir -p "$WORK"
CODEGEN="$TOP/src/cu/transpiler/codegen"
PARSER="$TOP/src/cu/transpiler/lstar/parser"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"
COHERENCE="$TOP/src/cu/transpiler/lstar/coherence"
STICK="$TOP/build/measuring_stick"
[ -f "$STICK/measuring_stick_nvcc.sass" ] || { echo "  no stick: run src/cu/scaffolding/measuring_stick.sh"; exit 1; }

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$PARSER")
BINARY="$OUT/klq_identity"
rm -f "$BINARY"
OBJECTS=()
for source in "$CODEGEN"/*.cu "$PARSER"/*.cu "$PROTOCOL/klq_identity.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu|codegen*.cu) continue ;;
    esac
    object="$OUT/$(basename "$source")_identity.o"
    if [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then
        c++ -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    fi
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
c++ -o "$BINARY" "${OBJECTS[@]}" -static
[ -f "$BINARY" ] || { echo "  build failed: klq_identity did not link"; exit 1; }

if [ "$#" -eq 0 ]; then
    set -- sass.krs
fi
"$BINARY" slice "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$STICK/measuring_stick.cu" "$WORK" || exit 1
# the host computes each question as its C says, signed arithmetic wrapping as the part's does
c++ -std=c++17 -O0 -fwrapv -w -o "$WORK/host_questions" "$WORK/host_questions.cpp" || { echo "  the host questions did not compile"; exit 1; }
"$WORK/host_questions" > "$WORK/host_answers.txt" || exit 1
"$BINARY" read "$WORK" "$COHERENCE/Lstar.klq" "$@"
