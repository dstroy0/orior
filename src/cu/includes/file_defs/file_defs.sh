# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# file_defs.sh, sourced by a script: FILE_DEFS, every C source of the formats' readers and writers a program links, by
# its path. A program built with any of them is built with them all, and a reader added here is linked by every script
# that sources this.
#
#     source src/cu/includes/file_defs/file_defs.sh
#     cc ... "${FILE_DEFS[@]}"
FILE_DEFS_SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FILE_DEFS=("$FILE_DEFS_SOURCE"/qry/writer/qry_member.c
    "$FILE_DEFS_SOURCE"/qry/writer/platform/*.c "$FILE_DEFS_SOURCE"/../parsers/line_read.c
    "$FILE_DEFS_SOURCE"/qry/qry_read.c "$FILE_DEFS_SOURCE"/../parsers/who.c
    "$FILE_DEFS_SOURCE"/../../transpiler/lstar/protocol/table_text.c
    "$FILE_DEFS_SOURCE"/krep/krep_records/*.c "$FILE_DEFS_SOURCE"/kfile_lines/kfile_lines.c
    "$FILE_DEFS_SOURCE"/kqr/kqr_record/*.c
    "$FILE_DEFS_SOURCE"/ksc/ksc.c "$FILE_DEFS_SOURCE"/kdm/kdm.c "$FILE_DEFS_SOURCE"/krf/krf.c
    "$FILE_DEFS_SOURCE"/ksg/ksg_seal.c "$FILE_DEFS_SOURCE"/ksg/ksg_writing.c)

# FILE_DEFS_FORMATS, the formats' sources alone with no flag among them, for a script that compiles one source at a
# time: it compiles these and FILE_DEFS_LINEA, each with FILE_DEFS_MMGR_FLAGS
FILE_DEFS_FORMATS=("${FILE_DEFS[@]}")

# FILE_DEFS_CXX, the C++ compiler of cc's own toolchain, which compiles and links a C++ program that links what cc
# compiled: a C++ compiler of another toolchain earlier on the PATH links those objects against a runtime they were
# not compiled for. Where cc's folder holds no c++, the c++ the PATH finds
FILE_DEFS_CXX="$(dirname "$(command -v cc)")/c++"
[ -x "$FILE_DEFS_CXX" ] || FILE_DEFS_CXX=c++

# MMgr, which the formats write their text through (verba_scribo.h): FILE_DEFS_MMGR, its include folders and settings
# and its modules' sources, for a program that links MMgr and none of the formats, and in FILE_DEFS with the formats'.
# The compiler takes an include folder and a setting for every source it is given wherever they stand among them, so a
# program that links the formats and includes MMgr itself is built with them too
FILE_DEFS_MMGR_TOP="$FILE_DEFS_SOURCE/../mmgr"
FILE_DEFS_MMGR_FLAGS=(-I "$FILE_DEFS_MMGR_TOP/include" -I "$FILE_DEFS_MMGR_TOP/src"
    -I "$FILE_DEFS_SOURCE/../embedded_types/include" -DMMGR_ENABLE_DMA=0 -DMMGR_ENABLE_EXTRAM=0 -DMMGR_ALIGN_BYTES=32)
FILE_DEFS_MMGR=("${FILE_DEFS_MMGR_FLAGS[@]}")

# FILE_DEFS_LINEA, the sources of MMgr's line (verba_scribo/verba_linea.h) and of the modules it reaches, for a program
# that writes its text through the line alone and is built with FILE_DEFS_MMGR_FLAGS. Each compiles under MSVC's C11,
# which a module MMgr builds with atomics or packed enums does not
FILE_DEFS_LINEA=()
for FILE_DEFS_MODULE in verba_scribo cellularum_laboro clz fractio memoria_operor proximus_operor transformo \
    verbum_scrutor; do
    FILE_DEFS_LINEA+=("$FILE_DEFS_MMGR_TOP/src/$FILE_DEFS_MODULE"/*.c)
done
FILE_DEFS_LINEA+=("$FILE_DEFS_MMGR_TOP/src/impensa_ancorae_acus/impensa_ancorae_acus_generic.c")

# every module's sources, and of impensa_ancorae_acus's profiles, each a source of its own, the one MMgr's build takes
# where none is named, generic
for FILE_DEFS_MODULE in "$FILE_DEFS_MMGR_TOP"/src/*/*.c; do
    case "$FILE_DEFS_MODULE" in
    */impensa_ancorae_acus/impensa_ancorae_acus_*.c) ;;
    *) FILE_DEFS_MMGR+=("$FILE_DEFS_MODULE") ;;
    esac
done
FILE_DEFS_MMGR+=("$FILE_DEFS_MMGR_TOP/src/impensa_ancorae_acus/impensa_ancorae_acus_generic.c")
FILE_DEFS+=("${FILE_DEFS_MMGR[@]}")
