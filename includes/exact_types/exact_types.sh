# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# exact_types.sh, sourced by a build script: the C sources of exact_types, of the platform of the program, and of the
# modules of MMgr that they reach, with the include folders and the settings that each of them compiles with. A program
# that links exact_types links all of them. Each source compiles as C11, one at a time, with EXACT_TYPES_FLAGS.
#
#     source includes/exact_types/exact_types.sh
#     cc -std=c11 "${EXACT_TYPES_FLAGS[@]}" -c <source>       for each of "${EXACT_TYPES_SOURCES[@]}"
#     ... the objects ... "${EXACT_TYPES_LIBRARIES[@]}"
#
# EXACT_TYPES_INTEGERS holds the sources of the integers alone, for a script that names its own subset of them.
EXACT_TYPES_TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXACT_TYPES_REPO="$(cd "$EXACT_TYPES_TOP/../.." && pwd)"
source "$EXACT_TYPES_REPO/includes/file_defs/file_defs.sh"

EXACT_TYPES_INTEGERS=("$EXACT_TYPES_TOP"/integers/exact_integer_{add,limbs,multiply,divide,gcd,decimal,hash}.c)
EXACT_TYPES_RECORD=("$EXACT_TYPES_TOP/integers/exact_record/exact_record.c")

# the platform of a library: the pool, the owners, and the calls into the system. The process that loads the library
# runs the process stage, platform_open of platform.h, before its first call into it
EXACT_TYPES_PLATFORM_LIBRARY=("$EXACT_TYPES_REPO/src/platform/platform_pool.c" "$EXACT_TYPES_REPO/src/platform/platform_windows.c"
    "$EXACT_TYPES_REPO/src/platform/platform_posix.c")

# the platform of a program: main and the process stage, and the platform of a library. A program that links them
# defines program_main (platform.h) and no main
EXACT_TYPES_PLATFORM=("$EXACT_TYPES_REPO/src/platform/platform_process.c" "${EXACT_TYPES_PLATFORM_LIBRARY[@]}")

# the cellblock of MMgr, which the platform places. It calls the hooks that the platform defines. A build keeps it out
# of the objects that the tessera daemon links
EXACT_TYPES_CARCER=("$FILE_DEFS_MMGR_TOP/src/locus_carcerum/locus_carcerum.c")

# the modules of MMgr: the line and the modules that it reaches, and the cellblock
EXACT_TYPES_MMGR=("${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_CARCER[@]}")

EXACT_TYPES_SOURCES=("${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_RECORD[@]}" "${EXACT_TYPES_PLATFORM[@]}"
    "${EXACT_TYPES_MMGR[@]}")

# the sources of a program that does arithmetic on the exact integers and builds no record program
EXACT_TYPES_ARITHMETIC=("${EXACT_TYPES_INTEGERS[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_MMGR[@]}")

EXACT_TYPES_FLAGS=("${FILE_DEFS_MMGR_FLAGS[@]}" -I "$EXACT_TYPES_TOP/integers" -I "$EXACT_TYPES_TOP/integers/exact_record"
    -I "$EXACT_TYPES_REPO/src/platform")

# the libraries of the system that the platform calls: the address wait on Windows, and the threads and the loader on
# Linux and macOS.
# The object of platform_windows.c names the library of the address wait to MSVC itself. A link that names no libraries still
# gets it there
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) EXACT_TYPES_LIBRARIES=(-lsynchronization) ;;
*) EXACT_TYPES_LIBRARIES=(-lpthread -ldl) ;;
esac

# what one command of the compiler adds to the sources of a program of the platform. It holds the flags of MMgr and
# of the platform, the line of MMgr, the platform and the cellblock. The libraries of the system come last in it, and
# so it goes after every source of the command. It goes with FILE_DEFS_FORMATS, as FILE_DEFS holds a second copy of
# MMgr
EXACT_TYPES_PLATFORM_PROGRAM=("${FILE_DEFS_MMGR_FLAGS[@]}" -I "$EXACT_TYPES_REPO/src/platform" "${FILE_DEFS_LINEA[@]}"
    "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" "${EXACT_TYPES_LIBRARIES[@]}")

# exact_types_compile SUFFIX SOURCE...: each source compiled as C11 into $OUT, its object named for the source and
# SUFFIX. The compile takes the caller's HOST_FLAGS, the caller's EXACT_TYPES_EXTRA where set, and EXACT_TYPES_FLAGS.
# EXACT_TYPES_OBJECTS holds the objects in the order of the sources. 0, or 1 where a source did not compile
exact_types_compile()
{
    local suffix="$1"
    shift
    local extension=o
    local extra=("${EXACT_TYPES_EXTRA[@]+"${EXACT_TYPES_EXTRA[@]}"}")
    local host=("${HOST_FLAGS[@]+"${HOST_FLAGS[@]}"}")
    local source
    local object
    case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) extension=obj ;;
    *) ;;
    esac
    EXACT_TYPES_OBJECTS=()
    for source in "$@"; do
        object="$OUT/$(basename "$source" .c)_${suffix}.${extension}"
        rm -f "$object"
        case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*)
            nvcc "${host[@]}" -Xcompiler "/std:c11 /O2 /experimental:c11atomics" "${extra[@]}" "${EXACT_TYPES_FLAGS[@]}" \
                -c "$source" -o "$object"
            ;;
        *) cc -std=c11 -O2 -fPIC "${extra[@]}" "${EXACT_TYPES_FLAGS[@]}" -c "$source" -o "$object" ;;
        esac
        [ -f "$object" ] || {
            echo "  build failed: $(basename "$source") did not compile"
            return 1
        }
        EXACT_TYPES_OBJECTS+=("$object")
    done
    return 0
}

# exact_types_platform_link SUFFIX: what a program of the platform links, compiled into $OUT. PLATFORM_LINK holds the
# objects of the MMgr sources and of the process stage, the pool and the system calls of the platform. It also holds
# the carcer of the pool, then the libraries of the system. The caller's EXACT_TYPES_OBJECTS stay as they were. 0, or
# 1 where a source did not compile
exact_types_platform_link()
{
    local caller=("${EXACT_TYPES_OBJECTS[@]+"${EXACT_TYPES_OBJECTS[@]}"}")
    exact_types_compile "$1" "${FILE_DEFS_LINEA[@]}" "${EXACT_TYPES_PLATFORM[@]}" "${EXACT_TYPES_CARCER[@]}" || return 1
    PLATFORM_LINK=("${EXACT_TYPES_OBJECTS[@]}" "${EXACT_TYPES_LIBRARIES[@]}")
    EXACT_TYPES_OBJECTS=("${caller[@]+"${caller[@]}"}")
    return 0
}
