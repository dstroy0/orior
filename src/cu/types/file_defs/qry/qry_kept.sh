#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# The runs of the tree kept while the host is idle, in a run of its own in build/qry_kept/qry (qry_run.sh). Every folder
# of the tree that holds a run is taken in turn, but one whose newest run's writer runs, which a script is still using:
#
#   writers   each run still a .qry has its writer's account read (qry_buffer.h), its bytes, its writes and the
#             slowest of them, and the folder's median rate said beside every run that wrote at under half of it.
#             This is read before the run is kept as its crystal, which only the part reads back
#   crystals  where the part is free, each run but the newest kept as its crystal (run_crystals); the sweep's runs are
#             left as their .qry for the sweep's pick to read
#   kept      only the newest QRY_KEPT runs of the folder kept (run_kept)
#   swept     the host swept once more (qry_sweep.sh) where no sweep of it is newer than QRY_SWEEP_HOURS, 12 where it
#             is not set
#
# Each is handed to the run as the blob of its name. The part is free where no process run from the tree is on it and
# it is used QRY_PART_USED percent or less over five readings a second apart, 10 where it is not set.
#
#     src/cu/types/file_defs/qry/qry_kept.sh
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
OUT="$TOP/build/qry_kept"
SWEEPS="$TOP/build/qry_sweep/qry"
QRY_SWEEP_HOURS="${QRY_SWEEP_HOURS:-12}"
QRY_PART_USED="${QRY_PART_USED:-10}"
mkdir -p "$OUT"
source "$HERE/qry_run.sh"
qry_writer_built "$OUT"

# 0 where the part is free, said where it is not
part_free() {
    command -v nvidia-smi > /dev/null || { echo "  no nvidia-smi: the part is not read"; return 1; }
    local top="$TOP" reading used
    command -v cygpath > /dev/null && top="$(cygpath -w "$TOP")"
    if nvidia-smi --query-compute-apps=process_name --format=csv,noheader 2> /dev/null | grep -Fqi "$top"; then
        echo "  a process of the tree is on the part"
        return 1
    fi
    for reading in 1 2 3 4 5; do
        used="$(nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2> /dev/null | head -1)"
        used="${used//[!0-9]/}"
        if [ -z "$used" ] || [ "$used" -gt "$QRY_PART_USED" ]; then
            echo "  the part is used ${used:-an unread} percent"
            return 1
        fi
        [ "$reading" -eq 5 ] || sleep 1
    done
}

# the writer's account of every run of the folder `$1` still a .qry, a line each, and the folder's median rate
writers_read() {
    local number held named account
    while read -r number held; do
        case "$held" in
            *.qry) ;;
            *) continue ;;
        esac
        named="$held"
        command -v cygpath > /dev/null && named="$(cygpath -m "$held")"
        account="$(QRY="$named" "$QRY_WRITER" latest writer 2> /dev/null | head -1)"
        echo "$(basename "$held") $account"
    done < <(run_each "$1") | awk -v folder="${1#"$TOP"/}" '
        # the run named first, then its account: its bytes the 7th word, its writes the 10th, the slowest write the
        # 14th and its bytes the 17th, the milliseconds writing in all the 19th and the panics the 24th
        NF >= 24 {
            runs += 1
            run[runs] = $1
            rate[runs] = ($19 > 0) ? ($7 / 1048576) / ($19 / 1000) : 0
            printf "    %s: %.1f MiB in %d writes, %.3f ms writing, %.0f MiB/s, the slowest %.3f ms for %d bytes; %d panics\n",
                $1, $7 / 1048576, $10, $19, rate[runs], $14, $17, $24
        }
        END {
            if (runs == 0) {
                exit
            }
            for (at = 1; at <= runs; at += 1) {
                sorted[at] = rate[at]
                for (below = at; (below > 1) && (sorted[below - 1] > sorted[below]); below -= 1) {
                    swap = sorted[below]
                    sorted[below] = sorted[below - 1]
                    sorted[below - 1] = swap
                }
            }
            median = sorted[int((runs + 1) / 2)]
            printf "  %s: %d runs read, the median %.0f MiB/s\n", folder, runs, median
            for (at = 1; at <= runs; at += 1) {
                if (rate[at] < median / 2) {
                    printf "    %s wrote at under half the median\n", run[at]
                }
            }
        }'
}

unset QRY
run_started "$OUT/qry"
folders=()
while read -r folder; do
    folders+=("$folder")
done < <(find "$TOP" -path "$TOP/.git" -prune -o -path '*/restored' -prune -o -name 'query[0-9]*-*.qry' -printf '%h\n' |
    sort -u)

free=1
part_said="$(part_free)" || free=0
crystals_said=()
[ "$free" -eq 1 ] || crystals_said+=("  no run is kept as its crystal this time:" "$part_said")
if [ "$free" -eq 1 ]; then
    qry_crystal_built
    [ -n "$QRY_CRYSTAL" ] || crystals_said+=("  no run is kept as its crystal this time: the crystal's program is not built")
fi
writers_said=()
kept_said=()
for folder in "${folders[@]}"; do
    newest="$(run_each "$folder" | head -1 | cut -d' ' -f2-)"
    command -v cygpath > /dev/null && newest="$(cygpath -m "$newest")"
    if [ "$newest" != "$QRY" ] && "$QRY_WRITER" runs "$newest" 2> /dev/null; then
        kept_said+=("  ${folder#"$TOP"/}: left as it is, its newest run's writer runs")
        continue
    fi
    mapfile -t -O "${#writers_said[@]}" writers_said < <(writers_read "$folder")
    if [ "$free" -eq 1 ] && [ -n "$QRY_CRYSTAL" ] && [ "$folder" != "$SWEEPS" ]; then
        mapfile -t -O "${#crystals_said[@]}" crystals_said < <(run_crystals "$folder")
    fi
    before="$(run_each "$folder" | wc -l)"
    run_kept "$folder"
    kept_said+=("  ${folder#"$TOP"/}: $(run_each "$folder" | wc -l) of $before runs kept")
done
printf '%s\n' "${writers_said[@]}" | "$QRY_WRITER" hand writers
printf '%s\n' "${crystals_said[@]}" | "$QRY_WRITER" hand crystals
printf '%s\n' "${kept_said[@]}" | "$QRY_WRITER" hand kept
printf '%s\n' "${writers_said[@]}" "${crystals_said[@]}" "${kept_said[@]}"

if [ -n "$(find "$SWEEPS" -name 'query[0-9]*-*.qry' -mmin "-$((QRY_SWEEP_HOURS * 60))" -print -quit 2> /dev/null)" ]; then
    swept_said="  a sweep of the host is newer than $QRY_SWEEP_HOURS hours: none is run"
else
    swept_said="$(bash "$HERE/qry_sweep.sh" 2>&1)"
fi
printf '%s\n' "$swept_said" | "$QRY_WRITER" hand swept
printf '%s\n' "$swept_said"
