#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs klq_identity over the measuring stick (klq_identity.cu): the stick's questions sliced into
# identities, the integer questions computed on the host over every case, and each identity's verdict written to
# Lstar.klq after its keys, beside the forms of the rulesets given, and the engine's writing of the stick sifted
# through what nvcc writes to build/engine/identity/known.txt. Reads the stick measuring_stick.sh and
# measuring_stick_engine.sh last wrote. No device.
#
#     utils/maint/engine/klq_identity.sh
#     utils/maint/engine/klq_identity.sh sass.krs
#     utils/maint/engine/klq_identity.sh stall
#     utils/maint/engine/klq_identity.sh register
#     utils/maint/engine/klq_identity.sh curve <task>...
#     utils/maint/engine/klq_identity.sh queue
#
# With no arguments the forms are sass.krs's. Given stall alone, it runs nothing else: the soonest each operation's
# result is read is walked down on the part over the engine's writing of the stick, every question carried by
# vendor_bin_layouts/nvidia/cubin_run and held to cubin_safe before the driver sees it, and the answers written to
# sm_86.ksc. Given register alone, the last register a question's code can name is walked down the same way and
# written to sm_86.ksc. Given curve and the numbers of chains of ours, each chain's time is taken on the part against
# the registers it declares and the threads of its blocks, and each knee written to sm_86.ksc. Given queue alone,
# every question of the stick is read off the record in sm_86.ksc or put to the part, and each counted by its
# category as reading 1, 0 or gray to build/engine/identity/queue/queue.txt. Each of those runs
# reads the host answers an earlier run wrote, and puts questions to the device.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/identity"
mkdir -p "$WORK"
CODEGEN="$TOP/src/cu/transpiler/codegen"
PARSER="$TOP/src/cu/transpiler/lstar/parser"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
COHERENCE="$TOP/src/cu/transpiler/lstar/coherence"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"
STICK="$TOP/build/measuring_stick"
[ -f "$STICK/measuring_stick_nvcc.sass" ] || { echo "  no stick: run src/cu/scaffolding/measuring_stick.sh"; exit 1; }

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$PARSER")
BINARY="$OUT/klq_identity"
rm -f "$BINARY"
OBJECTS=()
# an object is built again where it is missing, or where its source or a header of the folders it reads is newer
stale() {
    [ ! -f "$2" ] || [ "$1" -nt "$2" ] ||
        [ -n "$(find "$CODEGEN" "$PARSER" "$PROTOCOL" "$INTERFACE" "$TOP/src/cu/engine" -name '*.h' -newer "$2" -print -quit)" ]
}
for source in "$CODEGEN"/*.cu "$PARSER"/*.cu "$PROTOCOL/klq_identity.cu"; do
    case "$(basename "$source")" in
        asm_printer_*.cu|codegen*.cu) continue ;;
    esac
    object="$OUT/$(basename "$source")_identity.o"
    if stale "$source" "$object"; then
        c++ -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    fi
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
# the run channel and the interface it carries each question through, in C
for source in "$PROTOCOL/run_channel.c" "$INTERFACE/interface.c" "$INTERFACE/interface_names.c"; do
    object="$OUT/$(basename "$source")_identity.o"
    if stale "$source" "$object"; then
        cc -std=c11 -O2 -Wall -Wextra -I "$TOP/src/cu/engine" -c "$source" -o "$object"
    fi
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
c++ -o "$BINARY" "${OBJECTS[@]}" -static
[ -f "$BINARY" ] || { echo "  build failed: klq_identity did not link"; exit 1; }

if { [ "$#" -eq 1 ] && { [ "$1" = "stall" ] || [ "$1" = "register" ] || [ "$1" = "queue" ]; }; } || { [ "$#" -ge 2 ] && [ "$1" = "curve" ]; }; then
    [ -f "$WORK/host_answers.txt" ] || { echo "  no host answers: run utils/maint/engine/klq_identity.sh first"; exit 1; }
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
        { echo "  build failed: cubin_run did not compile"; exit 1; }
    MODE="$1"
    shift
    [ "$MODE" = "queue" ] && set -- "$STICK/measuring_stick.tsv"
    mkdir -p "$WORK/$MODE"
    exec "$BINARY" "$MODE" "$STICK/engine" "$WORK/host_answers.txt" "$COHERENCE/sm_86.ksc" "$WORK/$MODE" "$@" -- \
        "$CARRIER" "$COHERENCE/sm_86" "$COHERENCE/sm_86.ksc"
fi

if [ "$#" -eq 0 ]; then
    set -- sass.krs
fi
"$BINARY" slice "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$STICK/measuring_stick.cu" "$WORK" "$STICK/engine" || exit 1
# the host computes each question as its C says, signed arithmetic wrapping as the part's does
c++ -std=c++17 -O0 -fwrapv -w -o "$WORK/host_questions" "$WORK/host_questions.cpp" || { echo "  the host questions did not compile"; exit 1; }
"$WORK/host_questions" > "$WORK/host_answers.txt" || exit 1
"$BINARY" read "$WORK" "$COHERENCE/Lstar.klq" "$@" || exit 1
# the parts of nvcc's chains put together by their categories, and the windows nvcc never writes kept as questions
"$BINARY" permute "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$WORK" || exit 1
# the engine's writing of the stick sifted through what nvcc writes, read off the disassembly and never run
ENGINE="$STICK/engine"
if compgen -G "$ENGINE/*.dis" > /dev/null; then
    "$BINARY" known "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$WORK" "$ENGINE" || exit 1
    # ours held against the answer question by question, each place they part traced to the forms that wrote it
    "$BINARY" broken "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$WORK" "$ENGINE" -- "$@"
fi
