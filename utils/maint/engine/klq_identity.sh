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
#     utils/maint/engine/klq_identity.sh text_identity
#     utils/maint/engine/klq_identity.sh pair
#
# With no arguments the forms are sass.krs's. Given stall alone, it runs nothing else: the soonest each operation's
# result is read is walked down on the part over the engine's writing of the stick, every question carried by
# vendor_bin_layouts/nvidia/cubin_run and held to cubin_safe before the driver sees it, and the answers written to
# sm_86.ksc. Given register alone, the last register a question's code can name is walked down the same way and
# written to sm_86.ksc. Given curve and the numbers of chains of ours, each chain's time is taken on the part against
# the registers it declares and the threads of its blocks, and each knee written to sm_86.ksc. Given queue alone,
# every question of the stick is read off the record in sm_86.ksc or put to the part, and each counted by its
# category as reading 1, 0 or gray to build/engine/identity/queue/queue.txt. Each of those runs
# reads the host answers an earlier run wrote, and puts questions to the device. Given text_identity alone, every
# side of every identity between texts in Lstar.klq is written as a kernel of a stick of its own
# (measuring_stick.py), listed by nvcc, written by the engine and computed on the host, each identity is held on
# the part, and its verdict is written beneath it in Lstar.klq. Given pair alone, each pair of forms in Lstar.klq is
# put to the part, one form standing in for the other in a chain of ours, and its verdict written beneath it. Where
# KLQ_TRACE names the trace, the log beside it is read by klq_decoder, and the set of our coherence each pair is read
# into is written beneath its verdict with its concept, read off the values the part holds at each link. Each pair is
# put at its links' least vector first, the links between a case's load and the form, then the registers the chain
# names, and links of one vector are tried in an order drawn from KLQ_SEED, 1 where it is not given.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/identity"
mkdir -p "$WORK"
CODEGEN="$TOP/src/cu/engine/rmc"
PARSER="$TOP/src/cu/types/file_defs/readers"
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
# an object is built again where it is missing, or where its source or a header of the folders it reads is newer, and
# one that does not compile is no object
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
        rm -f "$object"
        c++ -std=c++17 -O2 -Wall -Wextra "${INCLUDES[@]}" -x c++ -c "$source" -o "$object"
    fi
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
# the run channel and the interface it carries each question through, in C
for source in "$PROTOCOL/run_channel.c" "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" \
    "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c"; do
    object="$OUT/$(basename "$source")_identity.o"
    if stale "$source" "$object"; then
        rm -f "$object"
        cc -std=c11 -O2 -Wall -Wextra -I "$TOP/src/cu/engine" -c "$source" -o "$object"
    fi
    [ -f "$object" ] || { echo "  build failed: $(basename "$source") did not compile"; exit 1; }
    OBJECTS+=("$object")
done
c++ -o "$BINARY" "${OBJECTS[@]}" -static
[ -f "$BINARY" ] || { echo "  build failed: klq_identity did not link"; exit 1; }

if [ "$#" -eq 1 ] && [ "$1" = "text_identity" ]; then
    [ -x "$STICK/engine/measuring_stick_engine" ] || [ -x "$STICK/engine/measuring_stick_engine.exe" ] ||
        { echo "  no engine: run src/cu/scaffolding/measuring_stick_engine.sh"; exit 1; }
    CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
    source "$TOP/utils/maint/engine/build_stamp.sh"
    SIDES="$WORK/text_identity"
    rm -rf "$SIDES"
    mkdir -p "$SIDES/engine"
    WIN_SIDES="$(cygpath -m "$SIDES")"
    python "$TOP/src/cu/scaffolding/measuring_stick.py" "$WIN_SIDES/measuring_stick.cu" "$WIN_SIDES/measuring_stick.tsv" \
        "$(cygpath -m "$COHERENCE")/Lstar.klq" || exit 1
    "$CUDA/bin/nvcc" ${CYCLE_HOST_CCBIN:+-ccbin "$CYCLE_HOST_CCBIN"} -cubin -arch=sm_86 -O3 -diag-suppress 177 -o "$WIN_SIDES/measuring_stick_nvcc.cubin" \
        "$WIN_SIDES/measuring_stick.cu" || { echo "  nvcc did not compile the sides"; exit 1; }
    "$CUDA/bin/cuobjdump" -sass "$SIDES/measuring_stick_nvcc.cubin" > "$SIDES/measuring_stick_nvcc.sass" || exit 1
    # the engine names the rulesets from the top of the tree
    (cd "$TOP" && "$STICK/engine/measuring_stick_engine" "$WIN_SIDES/measuring_stick.cu" \
        "$WIN_SIDES/measuring_stick_nvcc.sass" "$WIN_SIDES/engine" > "$SIDES/engine/assembler.log") || exit 1
    for code in "$SIDES"/engine/*.bin; do
        [ -f "$code" ] || continue
        "$CUDA/bin/nvdisasm" -b SM86 "$(cygpath -m "$code")" > "${code%.bin}.dis" 2>&1
    done
    "$BINARY" slice "$SIDES/measuring_stick_nvcc.sass" "$SIDES/measuring_stick.tsv" "$SIDES/measuring_stick.cu" \
        "$SIDES" "$SIDES/engine" || exit 1
    c++ -std=c++17 -O0 -fwrapv -w -o "$SIDES/host_questions" "$SIDES/host_questions.cpp" ||
        { echo "  the host questions did not compile"; exit 1; }
    "$SIDES/host_questions" > "$SIDES/host_answers.txt" || exit 1
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
        { echo "  build failed: cubin_run did not compile"; exit 1; }
    exec "$BINARY" text_identity "$SIDES/engine" "$SIDES/host_answers.txt" "$COHERENCE/sm_86.ksc" "$SIDES" \
        "$COHERENCE/Lstar.klq" "$SIDES/measuring_stick.tsv" -- "$CARRIER" "$COHERENCE/sm_86" "$COHERENCE/sm_86.ksc"
fi

if { [ "$#" -eq 1 ] && { [ "$1" = "stall" ] || [ "$1" = "register" ] || [ "$1" = "queue" ] || [ "$1" = "pair" ]; }; } || { [ "$#" -ge 2 ] && [ "$1" = "curve" ]; }; then
    [ -f "$WORK/host_answers.txt" ] || { echo "  no host answers: run utils/maint/engine/klq_identity.sh first"; exit 1; }
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" ||
        { echo "  build failed: cubin_run did not compile"; exit 1; }
    MODE="$1"
    shift
    [ "$MODE" = "queue" ] && set -- "$STICK/measuring_stick.tsv"
    [ "$MODE" = "pair" ] && set -- "$COHERENCE/Lstar.klq" "$COHERENCE/sm_86"
    mkdir -p "$WORK/$MODE"
    if [ "$MODE" = "pair" ] && [ -n "${KLQ_TRACE:-}" ]; then
        DECODER="$OUT/klq_decoder"
        rm -f "$DECODER"
        c++ -std=c++17 -O2 -Wall -Wextra -I "$PROTOCOL" -o "$DECODER" -x c++ "$PROTOCOL/klq_decoder.cu" -static ||
            { echo "  build failed: klq_decoder did not compile"; exit 1; }
        "$BINARY" "$MODE" "$STICK/engine" "$WORK/host_answers.txt" "$COHERENCE/sm_86.ksc" "$WORK/$MODE" "$@" -- \
            "$CARRIER" "$COHERENCE/sm_86" "$COHERENCE/sm_86.ksc" || exit 1
        exec "$DECODER" "$KLQ_TRACE.log" "$STICK/engine" "$COHERENCE/Lstar.klq"
    fi
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
