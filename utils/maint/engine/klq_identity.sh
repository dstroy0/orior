#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds and runs klq_identity over the measuring stick (klq_identity.cu): the stick's questions sliced into
# identities, the integer questions computed on the host over every case, and each identity's verdict written to
# Lstar.klq after its keys, beside the forms of the rulesets given, and the engine's writing of the stick sifted
# through what nvcc writes to the run's blob known.txt. Reads the stick measuring_stick.sh and
# measuring_stick_engine.sh last wrote. No device. Every file a mode writes goes through the run's writer: a current
# file of the tree, Lstar.klq, sm_86.ksc, sm_86.kqr or sm_86.kdm, written by it in the file's place, and every table
# of the run's own, slices.txt, chains.txt, host_questions.cpp and the rest, a blob of the run (qry_buffer.h).
#
#     utils/maint/engine/klq_identity.sh
#     utils/maint/engine/klq_identity.sh sass.krs
#     utils/maint/engine/klq_identity.sh stall
#     utils/maint/engine/klq_identity.sh register
#     utils/maint/engine/klq_identity.sh curve <task>...
#     utils/maint/engine/klq_identity.sh queue
#     utils/maint/engine/klq_identity.sh text_identity
#     utils/maint/engine/klq_identity.sh pair
#     utils/maint/engine/klq_identity.sh cost
#
# With no arguments the forms are sass.krs's. Given stall alone, it runs nothing else: the soonest each operation's
# result is read is walked down on the part over the engine's writing of the stick, every question carried by
# vendor_bin_layouts/nvidia/cubin_run and held to cubin_safe before the driver sees it, and the answers written to
# sm_86.ksc. Given register alone, the last register a question's code can name is walked down the same way and
# written to sm_86.ksc. Given curve and the numbers of chains of ours, each chain's time is taken on the part against
# the registers it declares and the threads of its blocks, and each knee written to sm_86.ksc. Given queue alone,
# every question of the stick is read off the record in sm_86.ksc or put to the part, and each counted by its
# category as reading 1, 0 or gray to the run's blob queue.txt. Each of those runs
# reads the host answers an earlier run handed its .qry, carried into its own, and puts questions to the device. Given text_identity alone, every
# side of every identity between texts in Lstar.klq is written as a kernel of a stick of its own
# (measuring_stick.py), listed by nvcc, written by the engine and computed on the host, each identity is held on
# the part, and its verdict is written beneath it in Lstar.klq. Given pair alone, each pair of forms in Lstar.klq is
# put to the part, one form standing in for the other in a chain of ours, and its verdict written beneath it. Every run
# of a mode that puts questions has one query container, query<n>-<datetime>.qry (src/cu/types/file_defs/qry/qry.h),
# and a writer that alone writes it: every file the run's channel and carrier pass between them, every answer, the
# carrier's output and every panic go into it, and no file of theirs is written beside it. It sits in the folder
# KLQ_TRACE names, or in build/engine/qry where KLQ_TRACE is not set. The run's trace and its log go into it, and what
# every program prints, unless QRY_TRACE keeps a path out (qry_buffer.h); klq_decoder reads the log back out of it,
# and the set of our coherence each pair is read into is written beneath its verdict with its concept, read off the
# values the part holds at each link. Each pair is put at its links' least vector first, the links between a case's load and the
# form, then the registers the chain names, and links of one vector are tried in an order drawn from KLQ_SEED, 1
# where it is not given. Given cost alone, each arrangement of sm_86.kdm whose nodes sass.krs writes is put in a chain
# of ours in place of the link its operator stands at, timed on the part's clock where it answers alike, and its cost
# and runs written in its row.
set -u

TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="$TOP/build/engine"
WORK="$OUT/identity"
mkdir -p "$WORK"
CODEGEN="$TOP/src/cu/engine/rmc"
PARSER="$TOP/src/cu/types/file_defs/readers"
source "$TOP/src/cu/types/file_defs/qry/qry_run.sh"
PROTOCOL="$TOP/src/cu/transpiler/lstar/protocol"
INTERFACE="$TOP/src/cu/transpiler/lstar/interface"
COHERENCE="$TOP/src/cu/transpiler/lstar/protocol/table"
LAYOUTS="$TOP/src/cu/transpiler/vendor_bin_layouts"
STICK="$TOP/build/measuring_stick"
[ -f "$STICK/measuring_stick_nvcc.sass" ] || { echo "  no stick: run $LAYOUTS/nvidia/measuring_stick.sh"; exit 1; }

INCLUDES=(-I "$TOP/src/cu/engine" -I "$CODEGEN" -I "$PARSER" -I "$QRY_SOURCE")
BINARY="$OUT/klq_identity"
rm -f "$BINARY"
OBJECTS=()
# an object is built again where it is missing, or where its source or a header of the folders it reads is newer, and
# one that does not compile is no object
stale() {
    [ ! -f "$2" ] || [ "$1" -nt "$2" ] ||
        [ -n "$(find "$CODEGEN" "$PARSER" "$TOP/src/cu/types/file_defs" "$PROTOCOL" "$INTERFACE" "$LAYOUTS" "$TOP/src/cu/engine" -name '*.h' -newer "$2" -print -quit)" ]
}
for source in "$CODEGEN"/*.cu "$PARSER"/*.cu "$PROTOCOL/table/klq_identity.cu"; do
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
# the run channel, the interface it carries each question through, the overseer and the run's buffer, in C
for source in "$PROTOCOL/teacher/run_channel.c" "$PROTOCOL/overseer/overseer.c" \
    "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" "$LAYOUTS/nvidia/sass_assemble.c" \
    "$LAYOUTS/nvidia/sass_machine.c" "${FILE_DEFS[@]}"; do
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
qry_writer_built "$OUT"
# the carrier, every channel input read from the run's buffer and every answer handed to it
carrier_built() {
    CARRIER="$OUT/cubin_run"
    cc -std=c11 -O2 -Wall -o "$CARRIER" "$LAYOUTS/nvidia/cubin_run.c" "$LAYOUTS/nvidia/cubin_safe.c" \
        "$LAYOUTS/nvidia/cubin_write.c" "$LAYOUTS/container_write.c" "$LAYOUTS/container_pattern.c" \
        "$LAYOUTS/container_layout.c" "$LAYOUTS/nvidia/sass_assemble.c" "$LAYOUTS/nvidia/sass_machine.c" \
        "${FILE_DEFS[@]}" || { echo "  build failed: cubin_run did not compile"; exit 1; }
    # the cross the airlock runs over every round before the part (run_channel.h), the vendor's reader beside the
    # compiler
    CROSS="$OUT/measuring_stick_query"
    cc -std=c11 -O2 -Wall -Wextra -pthread -I "$TOP/src/cu/engine" -o "$CROSS" \
        "$LAYOUTS/nvidia/measuring_stick_query.c" "$INTERFACE/interface.c" "$INTERFACE/interface_names.c" \
        "${FILE_DEFS[@]}" || { echo "  build failed: the cross did not compile"; exit 1; }
    NVDISASM="$(dirname "$(command -v nvcc)")/nvdisasm"
    [ -x "$NVDISASM" ] || [ -x "$NVDISASM.exe" ] || { echo "  no nvdisasm beside nvcc: the airlock has no reader"; exit 1; }
    # every round through the airlock: the carrier holds what the round's cross holds, and carries nothing the vendor
    # has not read
    CARRY=("$CARRIER" --held held.txt "$COHERENCE/sm_86.khw" "$LAYOUTS/nvidia/elf64_nvidia.tsv"
        --pop "$CARRIER" --dry --full "$COHERENCE/sm_86.khw" "$LAYOUTS/nvidia/elf64_nvidia.tsv"
        --cross "$CROSS" "$NVDISASM" SM86 airlock.md held.txt)
}

# The run's query container, query<n>-<datetime>.qry (qry_run.sh): in the folder KLQ_TRACE names where it is set, and
# in build/engine/qry where it is not. It holds every file the run reads as it read it and those it changes as it left
# them, the run's trace, and the host's answers, host_answers.txt, which a run that puts questions carries in from the
# latest run that holds them
RUNS="${KLQ_TRACE:-$OUT/qry}"
# a program run with what it prints passed to the screen and handed to the run as <name>.console (qry_writer tee): its
# exit
traced() {
    local name="$1"
    shift
    "$@" 2>&1 | "$QRY_WRITER" tee "$name"
    return "${PIPESTATUS[0]}"
}
run_configs() {
    run_read "$COHERENCE/sm_86.khw" "$COHERENCE/sm_86.ksc" "$COHERENCE/sm_86.kqr" "$COHERENCE/Lstar.klq" \
        "$COHERENCE/cu.krs" "$COHERENCE/sass.krs" "$STICK/measuring_stick.tsv"
}

if [ "$#" -eq 1 ] && [ "$1" = "text_identity" ]; then
    [ -x "$STICK/engine/measuring_stick_engine" ] || [ -x "$STICK/engine/measuring_stick_engine.exe" ] ||
        { echo "  no engine: run $LAYOUTS/nvidia/measuring_stick_engine.sh"; exit 1; }
    CUDA="/c/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.3"
    source "$TOP/utils/maint/engine/build_stamp.sh"
    run_started "$RUNS"
    run_configs
    SIDES="$WORK/text_identity"
    rm -rf "$SIDES"
    mkdir -p "$SIDES/engine"
    WIN_SIDES="$(cygpath -m "$SIDES")"
    python "$LAYOUTS/nvidia/measuring_stick.py" "$WIN_SIDES/measuring_stick.cu" "$WIN_SIDES/measuring_stick.tsv" \
        "$(cygpath -m "$COHERENCE")/Lstar.klq" || exit 1
    "$CUDA/bin/nvcc" ${CYCLE_HOST_CCBIN:+-ccbin "$CYCLE_HOST_CCBIN"} -cubin -arch=sm_86 -O3 -diag-suppress 177 -o "$WIN_SIDES/measuring_stick_nvcc.cubin" \
        "$WIN_SIDES/measuring_stick.cu" || { echo "  nvcc did not compile the sides"; exit 1; }
    "$CUDA/bin/cuobjdump" -sass "$SIDES/measuring_stick_nvcc.cubin" > "$SIDES/measuring_stick_nvcc.sass" || exit 1
    # the engine names the rulesets from the top of the tree; what it prints goes into the run's .qry as assembler.log
    (cd "$TOP" && "$STICK/engine/measuring_stick_engine" "$WIN_SIDES/measuring_stick.cu" \
        "$WIN_SIDES/measuring_stick_nvcc.sass" "$WIN_SIDES/engine") 2>&1 | "$QRY_WRITER" hand assembler.log
    [ "${PIPESTATUS[0]}" -eq 0 ] || exit 1
    for code in "$SIDES"/engine/*.bin; do
        [ -f "$code" ] || continue
        "$CUDA/bin/nvdisasm" -b SM86 "$(cygpath -m "$code")" > "${code%.bin}.dis" 2>&1
    done
    "$BINARY" slice "$SIDES/measuring_stick_nvcc.sass" "$SIDES/measuring_stick.tsv" "$SIDES/measuring_stick.cu" \
        "$SIDES" "$SIDES/engine" || exit 1
    # the host's questions compiled from their blob in the run, read through the compiler's standard input
    "$QRY_WRITER" latest host_questions.cpp | c++ -std=c++17 -O0 -fwrapv -w -x c++ -o "$SIDES/host_questions" - ||
        { echo "  the host questions did not compile"; exit 1; }
    "$SIDES/host_questions" | "$QRY_WRITER" hand host_answers.txt
    [ "${PIPESTATUS[0]}" -eq 0 ] || exit 1
    carrier_built
    traced klq_identity "$BINARY" text_identity "$SIDES/engine" host_answers.txt "$COHERENCE/sm_86.ksc" "$SIDES" \
        "$COHERENCE/Lstar.klq" "$SIDES/measuring_stick.tsv" -- "${CARRY[@]}"
    exit $?
fi

if { [ "$#" -eq 1 ] && { [ "$1" = "stall" ] || [ "$1" = "register" ] || [ "$1" = "queue" ] || [ "$1" = "pair" ] || [ "$1" = "cost" ]; }; } || { [ "$#" -ge 2 ] && [ "$1" = "curve" ]; }; then
    carrier_built
    MODE="$1"
    shift
    [ "$MODE" = "queue" ] && set -- "$STICK/measuring_stick.tsv"
    [ "$MODE" = "pair" ] && set -- "$COHERENCE/Lstar.klq" "$COHERENCE/sm_86.khw"
    [ "$MODE" = "stall" ] && set -- "$COHERENCE/sm_86.khw"
    [ "$MODE" = "cost" ] && set -- "$COHERENCE/sm_86.kdm" "$COHERENCE/sm_86.khw"
    mkdir -p "$WORK/$MODE"
    run_started "$RUNS"
    # the host's answers, and the overseer's lessons where a run before this one was taught any (overseer.h)
    run_carried "$RUNS" "$WORK/host_answers.txt" "$WORK/overseer.txt"
    "$QRY_WRITER" latest host_answers.txt > /dev/null 2>&1 ||
        { echo "  no host answers: run utils/maint/engine/klq_identity.sh first"; exit 1; }
    run_configs
    [ "$MODE" = "cost" ] && run_read "$COHERENCE/sm_86.kdm"
    traced klq_identity "$BINARY" "$MODE" "$STICK/engine" host_answers.txt "$COHERENCE/sm_86.ksc" "$WORK/$MODE" "$@" \
        -- "${CARRY[@]}" || exit 1
    # the pair's log read back by the decoder, where the learner traced it into the run
    if [ "$MODE" = "pair" ] && "$QRY_WRITER" latest log > /dev/null 2>&1; then
        DECODER="$OUT/klq_decoder"
        rm -f "$DECODER"
        cc -std=c11 -O2 -Wall -Wextra -c "$QRY_SOURCE/qry_buffer.c" -o "$OUT/qry_buffer_decoder.o" &&
            c++ -std=c++17 -O2 -Wall -Wextra -I "$PROTOCOL/table" -I "$PARSER" -I "$QRY_SOURCE" -o "$DECODER" -x c++ \
                "$PROTOCOL/table/klq_decoder.cu" "$PARSER/query_trace.cu" -x none "$OUT/qry_buffer_decoder.o" -static ||
            { echo "  build failed: klq_decoder did not compile"; exit 1; }
        traced klq_decoder "$DECODER" "$QRY" "$STICK/engine" "$COHERENCE/Lstar.klq"
        exit $?
    fi
    exit 0
fi

if [ "$#" -eq 0 ]; then
    set -- sass.krs
fi
run_started "$RUNS"
run_configs
traced klq_identity "$BINARY" slice "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" \
    "$STICK/measuring_stick.cu" "$WORK" "$STICK/engine" || exit 1
# the host computes each question as its C says, signed arithmetic wrapping as the part's does, its answers handed to
# the run
"$QRY_WRITER" latest host_questions.cpp | c++ -std=c++17 -O0 -fwrapv -w -x c++ -o "$WORK/host_questions" - ||
    { echo "  the host questions did not compile"; exit 1; }
"$WORK/host_questions" | "$QRY_WRITER" hand host_answers.txt
[ "${PIPESTATUS[0]}" -eq 0 ] || exit 1
traced klq_identity "$BINARY" read "$WORK" "$COHERENCE/Lstar.klq" "$@" || exit 1
# the parts of nvcc's chains put together by their categories, and the windows nvcc never writes kept as questions
traced klq_identity "$BINARY" permute "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$WORK" || exit 1
# the engine's writing of the stick sifted through what nvcc writes, read off the disassembly and never run
ENGINE="$STICK/engine"
if compgen -G "$ENGINE/*.dis" > /dev/null; then
    traced klq_identity "$BINARY" known "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$WORK" \
        "$ENGINE" || exit 1
    # ours held against the answer question by question, each place they part traced to the forms that wrote it
    traced klq_identity "$BINARY" broken "$STICK/measuring_stick_nvcc.sass" "$STICK/measuring_stick.tsv" "$WORK" \
        "$ENGINE" -- "$@"
fi
