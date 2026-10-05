#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds cubin_safe_check and holds cubin_safe to one case a rule against the part's machine file, then reads every
# cubin named on the host. Nothing goes to a device.
#
#     src/cu/scaffolding/cubin_safe_check.sh [<cubin>...]
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../.." && pwd)"
CUB="$TOP/src/cu/scaffolding"
KRS="$TOP/src/cu/transpiler/lstar/parser"
OUT="$TOP/build/cubin_safe"
mkdir -p "$OUT"

cc -std=c11 -O2 -Wall -Wextra -o "$OUT/cubin_safe_check" "$HERE/cubin_safe_check.c" "$TOP/src/cu/scaffolding/cubin_safe.c" \
    "$TOP/src/cu/scaffolding/cubin_write.c" "$TOP/src/cu/scaffolding/container_write.c" "$TOP/src/cu/scaffolding/container_pattern.c" "$TOP/src/cu/scaffolding/container_layout.c" "$TOP/src/cu/scaffolding/sass_assemble.c" "$TOP/src/cu/scaffolding/sass_machine.c" || exit 1
"$OUT/cubin_safe_check" "$TOP/src/cu/transpiler/lstar/coherence/sm_86" "$@"
STATUS=$?
echo "  cubin safe check exit $STATUS"
exit "$STATUS"
