# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# file_defs.sh, sourced by a script: FILE_DEFS, every C source of the file definitions a program links, the run's
# writer (qry/buffer), the k-file of records and of lines, and each format's one reader and writer, by its path. A
# program built with any of them is built with them all, and a format added here is linked by every script that
# sources this.
#
#     source src/cu/types/file_defs/file_defs.sh
#     cc ... "${FILE_DEFS[@]}"
FILE_DEFS_SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FILE_DEFS=("$FILE_DEFS_SOURCE"/qry/buffer/*.c "$FILE_DEFS_SOURCE"/krep_records/*.c "$FILE_DEFS_SOURCE/ktext.c"
    "$FILE_DEFS_SOURCE"/kqr/record/*.c "$FILE_DEFS_SOURCE/ksc/ksc.c" "$FILE_DEFS_SOURCE/kdm/kdm.c")
