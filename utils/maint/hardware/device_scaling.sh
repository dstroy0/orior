#!/bin/sh
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# How many device consumers the card actually supports, measured, and not assumed.
#
# THE QUESTION. A worker pool needs a size: how many miners can run on the card. That has an
# architectural answer and a measured one. Act only on the measured one.
#
# THE ARCHITECTURAL ANSWER, stated first, so that the measurement can refute it. The driver
# time-slices separate CUDA PROCESSES on one GPU, and does not run them concurrently, unless NVIDIA
# MPS runs. Thus N processes should each get about 1/N of the card. The aggregate should stay flat,
# less the cost of context switches. Multiple STREAMS inside ONE process do run concurrently. The
# survey stream of cuda_miner uses that.
#
# If the aggregate below scales with N, that argument is wrong, and the driver does something better
# than a time slice. If it stays flat, size the worker pool at one device process, and put the
# parallelism in streams.
#
# Build the bench first with utils/bench/bench_cuda.ps1. Then run
# sh utils/maint/hardware/device_scaling.sh with the largest instance count, 4 by default.
#
# Each instance of bench_cuda prints the median rate of each arm. The script reads the arm that the
# miner runs: shared front + anchor.

set -e
here=$(cd "$(dirname "$0")/../../.." && pwd)
bench="$here/build/bench/bench_cuda.exe"
out="$here/build/hardware/device_scaling"
mkdir -p "$out"

if [ ! -x "$bench" ]; then
    echo "no bench_cuda.exe at $bench. Build it with utils/bench/bench_cuda.ps1."
    exit 1
fi

top=${1:-4}

echo "  Concurrent instances of bench_cuda.exe, each with its own median device rate."
echo "  Aggregate is the sum. A flat aggregate means that the card time-slices."
echo ""
printf "  %10s %14s %16s %16s\n" "instances" "each (MH/s)" "aggregate" "vs 1 instance"

single=""
for count in $(seq 1 "$top"); do
    rm -f "$out"/run_*.txt
    pids=""
    at=1
    while [ "$at" -le "$count" ]; do
        (cd "$here/build/bench" && "$bench" > "$out/run_$at.txt" 2>&1) &
        pids="$pids $!"
        at=$((at + 1))
    done
    for pid in $pids; do
        wait "$pid" || true
    done

    # The medians section prints one line per arm, as the arm name and then the rate in MH/s.
    rates=$(grep -hE "^  shared front \+ anchor +[0-9.]+ MH/s" "$out"/run_*.txt 2>/dev/null |
        sed 's/.*anchor *//; s/ MH.*//')
    got=$(echo "$rates" | grep -c . || true)
    total=$(echo "$rates" | awk '{s += $1} END {printf "%.2f", s}')
    mean=$(echo "$rates" | awk '{s += $1; n += 1} END {if (n > 0) printf "%.2f", s / n; else print "0"}')

    if [ "$count" -eq 1 ]; then
        single="$total"
    fi
    ratio=$(awk -v a="$total" -v b="$single" 'BEGIN {if (b > 0) printf "%.3f", a / b; else print "-"}')

    if [ "$got" -ne "$count" ]; then
        printf "  %10s %14s %16s %16s\n" "$count" "INCOMPLETE" "$got of $count reported" "-"
    else
        printf "  %10s %14s %16s %16s\n" "$count" "$mean" "$total" "$ratio"
    fi
done

echo ""
echo "  READ THE LAST COLUMN. A ratio near 1.0 at every row means that the aggregate is flat, and"
echo "  that the card time-slices between processes. More miner processes then buy nothing, and"
echo "  cost context switches. A ratio that rises with the instance count would mean the opposite,"
echo "  and would refute the argument in the header of this script."
