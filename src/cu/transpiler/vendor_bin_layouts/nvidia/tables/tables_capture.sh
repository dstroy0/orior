#!/usr/bin/env sh
# The tables of the vendor, captured from its disassembler, on Linux, for every key container given. The script
# builds the hook, and runs the disassembler over each container with the hook loaded before the C library. It writes
# every copy of it to `<out>/<name>.capture`. Run inside the container that the tables job starts, with the folder of
# the tree mounted.
#
#     tables_capture.sh <nvdisasm> <hook.c> <out> <cubin>...
#
# Exit 0 where the script captured every container, and 1 where the hook did not build or the disassembler failed
# on one.
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
