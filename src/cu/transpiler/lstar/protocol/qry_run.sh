# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# qry_run.sh, sourced by a script: the script's query run (qry_member.h). Every process the script runs is a member of
# the run QRY names; a run's .qry holds every record of the run, and every file a member wrote is written in its place
# as the run ends. A file a member reads that the run does not hold is read from the disk into the run as it is read.
#
#     source src/cu/transpiler/lstar/protocol/qry_run.sh
#     qry_writer_built <folder>      the writer built into <folder> where its sources are newer, QRY_WRITER its path
#     run_started <folder>           where QRY names a .qry whose writer runs, the script joins that run; else
#                                    query<n>-<datetime>.qry is made in <folder>, <n> one past the highest it holds, its
#                                    writer started, and asked to stop as the script ends however it ends
#     run_filed <file>               the standard input appended as the file <file>, which the writer writes in its
#                                    place as the run ends
QRY_SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QRY_TOP="$(cd "$QRY_SOURCE/../../../../.." && pwd)"
export QRY_TOP

qry_writer_built() {
    QRY_WRITER="$1/orior_qry_writer"
    local binary="$QRY_WRITER" sources=("$QRY_TOP/src/cu/includes/file_defs/qry/qry_writer.c" "$QRY_TOP/src/cu/includes/file_defs/qry/writer"/*.c "$QRY_TOP/src/cu/includes/file_defs/qry/writer"/platform/*.c
        "$QRY_TOP/src/cu/includes/file_defs/qry/qry_read.c" "$QRY_TOP/src/cu/includes/file_defs/ksg/ksg_seal.c"
        "$QRY_TOP/src/cu/includes/file_defs/ksg/ksg_writing.c")
    [ -f "$binary.exe" ] && binary="$binary.exe"
    local stale=0 source
    [ -f "$binary" ] || stale=1
    for source in "${sources[@]}" "$QRY_TOP/src/cu/includes/file_defs/qry/writer"/*.h "$QRY_TOP/src/cu/includes/file_defs/qry/qry.h" \
        "$QRY_TOP/src/cu/includes/file_defs/qry/qry_read.h"; do
        [ "$source" -nt "$binary" ] && stale=1
    done
    if [ "$stale" -eq 1 ]; then
        mkdir -p "$1"
        cc -std=c11 -O2 -Wall -Wextra -o "$QRY_WRITER" "${sources[@]}" ||
            { echo "  build failed: orior_qry_writer did not compile"; exit 1; }
    fi
}

run_started() {
    if [ -n "${QRY:-}" ] && [ -f "$QRY" ]; then
        return 0
    fi
    mkdir -p "$1"
    local highest=0 held number
    for held in "$1"/query*-*.qry; do
        [ -f "$held" ] || continue
        number="${held##*/query}"
        number="${number%%-*}"
        case "$number" in '' | *[!0-9]*) continue ;; esac
        [ "$number" -gt "$highest" ] && highest="$number"
    done
    QRY="$1/query$((highest + 1))-$(date -u +%Y%m%dT%H%M%S).qry"
    export QRY
    "$QRY_WRITER" "$QRY" "$QRY_TOP" &
    QRY_WRITER_PROCESS=$!
    trap 'run_stopped' EXIT
    local waited=0
    # the writer makes the run's shared memory and then its .qry, so the .qry standing says the run is made
    until [ -f "$QRY" ]; do
        sleep 0.01
        waited=$((waited + 1))
        [ "$waited" -lt 3000 ] || { echo "  the run's writer did not start"; exit 1; }
    done
}

run_stopped() {
    "$QRY_WRITER" --stop "$QRY" || true
    wait "$QRY_WRITER_PROCESS" 2> /dev/null || true
}

run_filed() {
    "$QRY_WRITER" --filed "$QRY" "$1"
}
