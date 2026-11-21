#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# lng_check_test.sh: lng_check_test (lng_check_test.c) built into build/lng and run. It grades src/lng/lng.c and
# src/lng/lng_lift.c on tables whose defects and findings the test states by hand. Its exit is the test's.
#
# lng_lift.c runs the lift that LngLift.lean holds. Lake builds that module into Lng_LngLift.a, and leanc links the
# test with it and with the Lean runtime. cc compiles each C source first.
#
#     utils/test/src/lng/lng_check_test.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../.." && pwd)"
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
OUT="$TOP/build/lng"
OBJECTS="$OUT/lng_check_test_objects"
LNG="$TOP/theory/lng"
mkdir -p "$OUT" "$OBJECTS"

LEAN_PREFIX="$(cd "$LNG" && lean --print-prefix)" || { echo "  build failed: no Lean toolchain for theory/lng"; exit 2; }
(cd "$LNG" && lake build LngLift:static > /dev/null) || { echo "  build failed: LngLift did not build"; exit 2; }
# Lake names the archive Lng_LngLift.a on Windows and libLng_LngLift.a on every other system
ARCHIVE="$LNG/.lake/build/lib/Lng_LngLift.a"
[ -f "$ARCHIVE" ] || ARCHIVE="$LNG/.lake/build/lib/libLng_LngLift.a"
[ -f "$ARCHIVE" ] || { echo "  build failed: Lake wrote no archive of LngLift"; exit 2; }

# EXACT_TYPES_PLATFORM_PROGRAM holds flags, sources and libraries in one list. Each flag goes to every compile, each
# source becomes an object, and each library goes to the link
flags=(-I "$LEAN_PREFIX/include")
sources=("$HERE/lng_check_test.c" "$TOP/src/lng/lng.c" "$TOP/src/lng/lng_lift.c" "${FILE_DEFS_FORMATS[@]}")
libraries=()
program=("${EXACT_TYPES_PLATFORM_PROGRAM[@]}")
at=0
while [ "$at" -lt "${#program[@]}" ]; do
    item="${program[$at]}"
    case "$item" in
    -I) flags+=(-I "${program[$((at + 1))]}"); at=$((at + 1)) ;;
    -l*)
        # leanc links with the folders of Lean's toolchain alone. A library therefore goes by the path of its archive
        archive="$(cc -print-file-name="lib${item#-l}.a")"
        if [ -f "$archive" ]; then libraries+=("$archive"); else libraries+=("$item"); fi
        ;;
    -*) flags+=("$item") ;;
    *) sources+=("$item") ;;
    esac
    at=$((at + 1))
done

objects=()
place=0
for source in "${sources[@]}"; do
    # the place in front of the name keeps two sources with one name apart
    object="$OBJECTS/${place}_$(basename "${source%.c}").o"
    cc -std=c11 -O2 -Wall -Wextra "${flags[@]}" -c -o "$object" "$source" ||
        { echo "  build failed: $(basename "$source") did not compile"; exit 2; }
    objects+=("$object")
    place=$((place + 1))
done
"$LEAN_PREFIX/bin/leanc" -o "$OUT/lng_check_test" "${objects[@]}" "$ARCHIVE" \
    "${libraries[@]}" || { echo "  build failed: lng_check_test did not link"; exit 2; }
"$OUT/lng_check_test"
