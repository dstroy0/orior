#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds cubin_safe_check and holds cubin_safe to one case a rule against the part's machine file, then reads every
# cubin named on the host. Nothing goes to a device.
#
#     src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/cubin_safe_check.sh [<cubin>...]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../../.." && pwd)"
CUB="$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding"
KRS="$TOP/src/cu/types/file_defs/readers"
OUT="$TOP/build/cubin_safe"
mkdir -p "$OUT"

cc -std=c11 -O2 -Wall -Wextra -o "$OUT/cubin_safe_check" "$HERE/cubin_safe_check.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe.c" \
    "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_write.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_pattern.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/container_layout.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble.c" "$TOP/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine.c" || exit 1
"$OUT/cubin_safe_check" "$TOP/src/cu/transpiler/lstar/protocol/table/sm_86.khw" "$@"
STATUS=$?
echo "  cubin safe check exit $STATUS"
exit "$STATUS"
