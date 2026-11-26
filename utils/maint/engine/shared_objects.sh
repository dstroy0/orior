#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# shared_objects.sh, sourced by a build script: shared_objects_built, which compiles the sources that many programs
# link. Each source compiles one time for each set of words, and every script that compiles it with those words links
# the same object.
#
#     source utils/maint/engine/shared_objects.sh
#     WORDS=(cc -std=c11 -O2 ...)
#     shared_objects_built WORDS <source> ... || exit 1
#     cc ... "${SHARED_OBJECTS[@]}"
SHARED_OBJECTS_TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SHARED_OBJECTS_ROOT="$SHARED_OBJECTS_TOP/build/shared_objects"
# The system, the extension of an object and the compiles that run at the same time. Then the top of the tree as a
# compiler writes it in a .d file: D:/... on Windows, and the path itself elsewhere. This file finds each of them one
# time, as a fork costs much time on MSYS
SHARED_OBJECTS_SYSTEM="$(uname -s)"
SHARED_OBJECTS_EXTENSION=o
SHARED_OBJECTS_WRITTEN_TOP="$SHARED_OBJECTS_TOP"
case "$SHARED_OBJECTS_SYSTEM" in
MINGW* | MSYS* | CYGWIN*)
    SHARED_OBJECTS_EXTENSION=obj
    SHARED_OBJECTS_WRITTEN_TOP="$(cygpath -m "$SHARED_OBJECTS_TOP")"
    ;;
*) ;;
esac
SHARED_OBJECTS_JOBS="${COMPILE_CACHE_JOBS:-$(nproc 2> /dev/null || echo 4)}"

# SHARED_OBJECTS_PLAIN set to the path $1 with each pair of a folder and the "/.." after it taken out. Two paths of one
# source then name one object. It sets a name and starts no subshell, as a fork costs much time on MSYS
shared_objects_plain()
{
    SHARED_OBJECTS_PLAIN="$1"
    while [[ "$SHARED_OBJECTS_PLAIN" =~ ^(.*)/[^/.][^/]*/\.\./(.*)$ ]]; do
        SHARED_OBJECTS_PLAIN="${BASH_REMATCH[1]}/${BASH_REMATCH[2]}"
    done
}

# 0 where the object $2 of the source $1 needs a compile. No object is there, or the source is newer than it, or a
# header of the tree that its last compile read is newer. Each compile writes the files of the tree that it reads into
# the object's .d file. The check tests only those. nvcc has -MD alone, which writes the headers of the toolchain too,
# and a .cu reads some hundreds of them. The key of the folder names the compiler. A new toolchain thus compiles into a
# new folder.
#
# The words of the file go through read, and the check starts no process
shared_objects_stale()
{
    local source="$1"
    local object="$2"
    local depend="$object.d"
    if [ ! -f "$object" ] || [ ! -f "$depend" ] || [ "$source" -nt "$object" ]; then
        return 0
    fi
    local words
    local header
    while read -r -a words; do
        for header in "${words[@]}"; do
            case "$header" in
            "$SHARED_OBJECTS_WRITTEN_TOP"/*) [ "$header" -nt "$object" ] && return 0 ;;
            *) ;;
            esac
        done
    done < "$depend"
    return 1
}

# The source $2 compiled to the object $1 by the words after them. The object and its .d file go beside their places
# first, and into their places when the compile ends. The .d file keeps the lines that name a file of the tree, and
# the source is one of them. Two scripts that compile one object at the same time then each put a whole object there. A
# source that does not compile goes in the file $SHARED_OBJECTS_FAILED, and the words of the compiler stay beside the
# object
shared_objects_compiled()
{
    local object="$1"
    local source="$2"
    local part="$object.$$.part"
    shift 2
    local depend=-MMD
    [ "$(basename "$1")" = nvcc ] && depend=-MD
    if "$@" "$depend" -MF "$part.d" -MT object -c "$source" -o "$part" > "$object.log" 2>&1 &&
        grep -F "$SHARED_OBJECTS_WRITTEN_TOP" "$part.d" > "$part.kept" && mv -f "$part.kept" "$object.d" &&
        mv -f "$part" "$object"; then
        rm -f "$object.log" "$part.d"
    else
        rm -f "$part" "$part.d" "$part.kept"
        echo "$source $object.log" >> "$SHARED_OBJECTS_FAILED"
    fi
}

# shared_objects_built WORDS SOURCE...: each source compiled with -c by the words of the array that WORDS names, into a
# folder of build/shared_objects. The folder holds the objects of one set of words. A cksum of the system, the path of
# the compiler and the words names it. The path of nvcc holds the version of the toolkit, and -ccbin names the version
# of MSVC. An object compiles again only where shared_objects_stale says so.
#
# The compiles run at the same time, as many as the host has processors, or COMPILE_CACHE_JOBS where the harness sets
# it. SHARED_OBJECTS holds the objects in the order of the sources, and SHARED_OBJECTS_COMPILED the count that
# compiled. 0, or 1 where a source did not compile
shared_objects_built()
{
    local -n shared_objects_words="$1"
    shift
    local compiler
    compiler="$(type -P "${shared_objects_words[0]}")"
    local key
    read -r key _ < <(printf '%s\n' "$SHARED_OBJECTS_SYSTEM" "$compiler" "${shared_objects_words[@]}" | cksum)
    local folder="$SHARED_OBJECTS_ROOT/$key"
    [ -d "$folder" ] || mkdir -p "$folder"
    SHARED_OBJECTS_FAILED="$folder/failed.$$"
    [ ! -f "$SHARED_OBJECTS_FAILED" ] || rm -f "$SHARED_OBJECTS_FAILED"
    SHARED_OBJECTS=()
    SHARED_OBJECTS_COMPILED=0
    local source
    local relative
    local object
    local running=0
    for source in "$@"; do
        shared_objects_plain "$source"
        relative="${SHARED_OBJECTS_PLAIN#"$SHARED_OBJECTS_TOP"/}"
        object="$folder/${relative//\//__}.$SHARED_OBJECTS_EXTENSION"
        SHARED_OBJECTS+=("$object")
        if shared_objects_stale "$SHARED_OBJECTS_PLAIN" "$object"; then
            if [ "$running" -ge "$SHARED_OBJECTS_JOBS" ]; then
                wait -n
                running=$((running - 1))
            fi
            shared_objects_compiled "$object" "$SHARED_OBJECTS_PLAIN" "${shared_objects_words[@]}" &
            running=$((running + 1))
            SHARED_OBJECTS_COMPILED=$((SHARED_OBJECTS_COMPILED + 1))
        fi
    done
    wait
    if [ -f "$SHARED_OBJECTS_FAILED" ]; then
        local log
        while read -r source log; do
            echo "  build failed: ${source#"$SHARED_OBJECTS_TOP"/} did not compile"
            tail -20 "$log"
        done < "$SHARED_OBJECTS_FAILED"
        rm -f "$SHARED_OBJECTS_FAILED"
        return 1
    fi
    return 0
}
