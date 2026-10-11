# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# qry_run.sh, sourced by a script: the script's query run (qry_buffer.h). Every process the script runs reaches the
# run by QRY. A run's .qry holds everything the run did and every file it ran against, as it read each one and as it
# left each one it changed, so that one .qry says what one iteration saw and did.
#
# As a run the script started ends, every other run of its folder still a .qry, and no writer's, is kept as the
# engine's crystal (qry_crystal.c), the folder query<n>-<datetime> beside where its .qry stood, and the .qry let go once
# the crystal held byte for byte; the newest run stays a .qry for the next to read. Only the newest QRY_KEPT runs of a
# folder are kept, 64 where it is not set. A run kept as a crystal is read back out of it where a run before it is
# read and no .qry holds what is sought.
#
#     source src/cu/types/file_defs/qry/qry_run.sh
#     qry_writer_built <folder>    the writer built into <folder> where its sources are newer, QRY_WRITER its path, and
#                                  the crystal's program beside the engine in build/qry_crystal where nvcc is found,
#                                  QRY_CRYSTAL its path, empty where it is not built
#     run_started <folder>         where QRY names a .qry whose writer runs, the script joins that run and QRY_JOINED
#                                  is 1; where it names none, query<n>-<datetime>.qry is made in <folder>, <n> one past
#                                  the highest it holds, its writer started and stopped as the script ends however it
#                                  ends, and QRY_JOINED is 0
#     run_latest <folder> <name>   the latest .qry of <folder> other than the run's own whose blobs hold <name>,
#                                  printed, for a run to carry what a run before it handed; 1 where none does
#     run_carried <folder> <file>...
#                                  each file's name carried into the run from the latest run of <folder> that holds
#                                  it; where none does, the file handed in as it is on disk, where it is there
#     run_read <file>...           each file that is there handed to the run as the run reads it, the blob named by the
#                                  file's path from the top of the tree
#     run_left <file>...           each file handed to the run as the script ends, as the run leaves it, named the same
#                                  way; run_named <file> prints the name
QRY_SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QRY_TOP="$(cd "$QRY_SOURCE/../../../../.." && pwd)"
QRY_LEFT=()
QRY_KEPT="${QRY_KEPT:-64}"

run_named() {
    local file
    file="$(cd "$(dirname "$1")" 2> /dev/null && pwd)/$(basename "$1")"
    case "$file" in
        "$QRY_TOP"/*) echo "${file#"$QRY_TOP"/}" ;;
        *) basename "$1" ;;
    esac
}

qry_writer_built() {
    QRY_WRITER="$1/qry_writer"
    local binary="$QRY_WRITER"
    [ -f "$binary.exe" ] && binary="$binary.exe"
    if ! { [ -f "$binary" ] && [ "$binary" -nt "$QRY_SOURCE/qry_writer.c" ] &&
        [ "$binary" -nt "$QRY_SOURCE/qry_buffer.c" ] && [ "$binary" -nt "$QRY_SOURCE/qry_buffer.h" ] &&
        [ "$binary" -nt "$QRY_SOURCE/qry.h" ]; }; then
        mkdir -p "$1"
        cc -std=c11 -O2 -Wall -Wextra -o "$QRY_WRITER" "$QRY_SOURCE/qry_writer.c" "$QRY_SOURCE/qry_buffer.c" ||
            { echo "  build failed: qry_writer did not compile"; exit 1; }
    fi
    qry_crystal_built
}

# the crystal's program, built against the engine (src/build_engine.sh) in build/qry_crystal/engine, the engine built
# there once where it is not, and the program again where its source is newer. QRY_CRYSTAL is left empty where nvcc or
# a host compiler it takes is not found, and the runs are then kept as their .qry
qry_crystal_built() {
    QRY_CRYSTAL=""
    command -v nvcc > /dev/null || return 0
    local engine="$QRY_TOP/build/qry_crystal/engine" library host=() link=() binary
    case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*)
            library="$engine/cell_tracking_engine.lib"
            binary="$engine/qry_crystal.exe"
            local msvc
            msvc="$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64 \
                "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64 2> /dev/null | tail -1)"
            [ -n "$msvc" ] || return 0
            host=(-ccbin "$msvc" -Xcompiler "/std:c11 /O2 /Zc:preprocessor /D_CRT_SECURE_NO_WARNINGS")
            link=("$library")
            ;;
        *)
            library="$engine/libcell_tracking_engine.so"
            binary="$engine/qry_crystal"
            link=(-L "$engine" -lcell_tracking_engine -Xlinker -rpath -Xlinker "$engine")
            ;;
    esac
    if [ ! -f "$library" ]; then
        echo "  the crystal's engine is built once, into $engine"
        mkdir -p "$engine"
        BUILD_OUT="$engine" bash "$QRY_TOP/src/build_engine.sh" > "$engine/build.log" 2>&1 ||
            { echo "  the crystal's engine did not build ($engine/build.log); runs are kept as their .qry"; return 0; }
    fi
    if [ ! -f "$binary" ] || [ "$QRY_SOURCE/qry_crystal.c" -nt "$binary" ]; then
        nvcc "${host[@]}" -I "$QRY_TOP/src/cu/engine" -o "$binary" "$QRY_SOURCE/qry_crystal.c" "${link[@]}" \
            > "$engine/qry_crystal_build.log" 2>&1 ||
            { echo "  qry_crystal did not build ($engine/qry_crystal_build.log); runs are kept as their .qry"; return 0; }
    fi
    QRY_CRYSTAL="$binary"
}

# the runs of the folder `$1`, newest first: a line each, its <n> and its .qry, or the folder of its crystal where it
# is kept as one
run_each() {
    local held number
    for held in "$1"/query*-*; do
        number="${held##*/query}"
        number="${number%%-*}"
        case "$number" in
            '' | *[!0-9]*) continue ;;
        esac
        case "$held" in
            *.qry) [ -f "$held" ] && echo "$number $held" ;;
            *) [ -d "$held" ] && echo "$number $held" ;;
        esac
    done | sort -k1,1rn
}

# the <n> of every run of the folder `$1`, highest first
run_numbers() {
    run_each "$1" | cut -d' ' -f1
}

# the run at `$1` as a .qry to read, printed: its .qry, or one read back out of its crystal into the folder restored
# beside it, which the script's end lets go; 1 where none could be had
run_opened() {
    local named="$1"
    case "$1" in
        *.qry) ;;
        *)
            [ -n "${QRY_CRYSTAL:-}" ] || return 1
            mkdir -p "$(dirname "$1")/restored"
            named="$(dirname "$1")/restored/$(basename "$1").qry"
            [ -f "$named" ] || "$QRY_CRYSTAL" read "$1" "$named" > /dev/null || return 1
            ;;
    esac
    command -v cygpath > /dev/null && named="$(cygpath -m "$named")"
    echo "$named"
}

run_started() {
    trap run_ended EXIT
    if [ -n "${QRY:-}" ] && "$QRY_WRITER" runs "$QRY"; then
        QRY_JOINED=1
        return 0
    fi
    QRY_JOINED=0
    QRY_RUNS="$1"
    mkdir -p "$1" || { echo "  $1 is no folder"; exit 1; }
    local highest
    highest="$(run_numbers "$1" | head -1)"
    QRY="$1/query$((${highest:-0} + 1))-$(date +%Y%m%dT%H%M%S).qry"
    command -v cygpath > /dev/null && QRY="$(cygpath -m "$QRY")"
    export QRY
    "$QRY_WRITER" start "$QRY" &
    QRY_WRITER_PROCESS=$!
    "$QRY_WRITER" ready || { echo "  the run's writer did not start"; exit 1; }
}

# the script's end: every file run_left named handed as the run leaves it, the writer stopped where the script started
# it, and the runs of its folder kept
run_ended() {
    local left
    for left in "${QRY_LEFT[@]}"; do
        [ -f "$left" ] && "$QRY_WRITER" hand "$(run_named "$left")" < "$left"
    done
    QRY_LEFT=()
    [ -n "${QRY_WRITER_PROCESS:-}" ] || return 0
    run_stopped
    run_kept "$QRY_RUNS"
}

run_stopped() {
    [ -n "${QRY_WRITER_PROCESS:-}" ] || return 0
    "$QRY_WRITER" stop || echo "  the run's writer did not stop"
    wait "$QRY_WRITER_PROCESS"
    QRY_WRITER_PROCESS=""
}

# the runs of the folder `$1` kept: each .qry but the newest and any whose writer runs kept as its crystal, the .qry
# let go only once the crystal held, then only the newest QRY_KEPT runs kept at all
run_kept() {
    local number held named newest=""
    rm -rf "$1/restored"
    if [ -n "${QRY_CRYSTAL:-}" ]; then
        while read -r number held; do
            case "$held" in
                *.qry) ;;
                *) continue ;;
            esac
            [ -n "$newest" ] || { newest="$held"; continue; }
            named="$held"
            command -v cygpath > /dev/null && named="$(cygpath -m "$held")"
            "$QRY_WRITER" runs "$named" && continue
            if "$QRY_CRYSTAL" write "$named" "${held%.qry}"; then
                rm -f "$held"
            else
                echo "  $(basename "$held") is kept as its .qry: its crystal did not hold"
                rm -rf "${held%.qry}"
            fi
        done < <(run_each "$1")
    fi
    run_each "$1" | tail -n "+$((QRY_KEPT + 1))" | while read -r number held; do
        rm -rf "$held"
    done
}

run_latest() {
    local number held named
    while read -r number held; do
        named="$(run_opened "$held")" || continue
        [ "$named" = "${QRY:-}" ] && continue
        if QRY="$named" "$QRY_WRITER" latest "$2" > /dev/null 2>&1; then
            echo "$named"
            return 0
        fi
    done < <(run_each "$1")
    return 1
}

# each run of the folder read once, newest first, for every name the run does not hold yet, until it holds them all
run_carried() {
    local folder="$1" file number held named missing
    shift
    while read -r number held; do
        missing=()
        for file in "$@"; do
            "$QRY_WRITER" latest "$(basename "$file")" > /dev/null 2>&1 || missing+=("$(basename "$file")")
        done
        [ "${#missing[@]}" -ne 0 ] || break
        named="$(run_opened "$held")" || continue
        [ "$named" = "${QRY:-}" ] && continue
        "$QRY_WRITER" carry "$named" "${missing[@]}" > /dev/null 2>&1
    done < <(run_each "$folder")
    for file in "$@"; do
        if ! "$QRY_WRITER" latest "$(basename "$file")" > /dev/null 2>&1 && [ -f "$file" ]; then
            "$QRY_WRITER" hand "$(basename "$file")" < "$file" || echo "  $(basename "$file") was not handed to the run"
        fi
    done
}

run_read() {
    local file
    for file in "$@"; do
        [ -f "$file" ] && { "$QRY_WRITER" hand "$(run_named "$file")" < "$file" || echo "  $file was not handed"; }
    done
}

run_left() {
    QRY_LEFT+=("$@")
}
