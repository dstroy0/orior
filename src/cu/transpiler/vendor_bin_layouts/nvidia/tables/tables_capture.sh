#!/usr/bin/env sh
# The vendor's tables captured off its disassembler, on Linux, for every key container given: the hook built, and
# the disassembler run over each container with the hook loaded before the C library, its every copy written to
# <out>/<name>.capture. Run inside the container the tables job starts, the tree's folder mounted.
#
#     tables_capture.sh <nvdisasm> <hook.c> <out> <cubin>...
#
# Exit 0 where every container was captured, 1 where the hook did not build or the disassembler failed on one.
set -u
nvdisasm="$1"
hook="$2"
out="$3"
shift 3
mkdir -p "$out"
cc -std=c11 -O2 -fPIC -shared -o "$out/tables_intercept.so" "$hook" -ldl || exit 1
code=0
for cubin in "$@"; do
    name=$(basename "$cubin" .cubin)
    LD_PRELOAD="$out/tables_intercept.so" OMP_NUM_THREADS=1 "$nvdisasm" "$cubin" > "$out/$name.capture" 2> "$out/$name.err"
    if [ $? -ne 0 ]; then
        echo "tables: $name: the disassembler did not end clean" >&2
        code=1
    fi
done
exit $code
