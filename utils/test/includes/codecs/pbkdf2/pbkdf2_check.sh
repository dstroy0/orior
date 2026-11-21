#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Builds pbkdf2_check and holds the HMAC-SHA-512 and PBKDF2 of includes/codecs/pbkdf2 to published vectors: RFC 4231,
# Wycheproof, and the BIP-39 vectors of trezor/python-mnemonic. The compiler is CC where it is set, else cc.
#
# Run utils/test/includes/codecs/pbkdf2/pbkdf2_check.sh to use each cached vector file whose hash matches.
# Run it with --fetch to fetch every vector file again.
#
# The vector files are not in git. MANIFEST.tsv names each file, its source at a pinned commit, and its SHA-256.
# The script keeps the files in build/kat/pbkdf2. It uses a cached file only where its SHA-256 matches the manifest,
# and fetches it again otherwise. A fetched file with the wrong SHA-256 stops the run before any check.
#
# sha256sum computes every hash, and not the SHA-256 of this tree. Thus the integrity check depends on no code
# that the vectors test.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
CACHE="$TOP/build/kat/pbkdf2"
OUT="$TOP/build/pbkdf2"
mkdir -p "$CACHE" "$OUT"

FETCH=0
if [ "${1:-}" = "--fetch" ]; then
    FETCH=1
elif [ $# -gt 0 ]; then
    echo "usage: pbkdf2_check.sh [--fetch]"
    exit 2
fi

hash_of() {
    sha256sum "$1" | cut -d' ' -f1
}

FILES=()
while IFS=$'\t' read -r file sha256 source what; do
    if [ "$file" = "file" ]; then
        continue
    fi
    path="$CACHE/$file"
    if [ "$FETCH" -eq 0 ] && [ -f "$path" ] && [ "$(hash_of "$path")" = "$sha256" ]; then
        echo "  cached   $file"
    else
        part="$path.part"
        rm -f "$part"
        if ! curl -sSfL -o "$part" "$source"; then
            echo "  FAIL: cannot fetch $source"
            exit 1
        fi
        got="$(hash_of "$part")"
        if [ "$got" != "$sha256" ]; then
            echo "  FAIL: $file has SHA-256 $got, and MANIFEST.tsv expects $sha256"
            rm -f "$part"
            exit 1
        fi
        mv "$part" "$path"
        echo "  fetched  $file"
    fi
    FILES+=("$path")
done < "$HERE/MANIFEST.tsv"

if [ "${#FILES[@]}" -ne 4 ]; then
    echo "  FAIL: MANIFEST.tsv names ${#FILES[@]} files, and the check reads 4"
    exit 1
fi

# the codecs copy and fill through MMgr, whose sources the check compiles beside them
source "$TOP/includes/file_defs/file_defs.sh"
source "$TOP/includes/exact_types/exact_types.sh"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra "${FILE_DEFS_MMGR_FLAGS[@]}" -o "$OUT/pbkdf2_check" "$HERE/pbkdf2_check.c" \
    "$TOP/includes/codecs/pbkdf2/pbkdf2.c" "$TOP/includes/codecs/sha512/sha512.c" "${EXACT_TYPES_PLATFORM_PROGRAM[@]}" || exit 1
"$OUT/pbkdf2_check" "${FILES[@]}"
