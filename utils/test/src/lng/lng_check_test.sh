#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# lng_check_test.sh: lng_check_test (lng_check_test.c) built into build/lng and run: src/lng/lng.c graded on tables
# whose defects and findings were worked out by hand. Its exit is the test's.
#
#     utils/test/src/lng/lng_check_test.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../.." && pwd)"
source "$TOP/src/cu/includes/file_defs/file_defs.sh"
OUT="$TOP/build/lng"
mkdir -p "$OUT"
cc -std=c11 -O2 -Wall -Wextra -o "$OUT/lng_check_test" "$HERE/lng_check_test.c" "$TOP/src/lng/lng.c" \
    "${FILE_DEFS[@]}" || { echo "  build failed: lng_check_test did not compile"; exit 2; }
"$OUT/lng_check_test"
