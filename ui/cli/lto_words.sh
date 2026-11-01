# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# lto_words.sh: sourced by build.sh and the scripts that link what it builds, with CC and TOP set. Every object is
# bitcode and every program is optimized whole at its link, so that a call through a guard folds across files and what
# a check runs is what ships. LTO_WORDS is what a compile takes, LINK_WORDS what a link takes. The optimized modules
# are kept under build/lto_cache, and a link reads back what it shares with the link before it.
LTO_CACHE="$TOP/build/lto_cache"
# GCC's link-time optimization is one flag on both sides; clang's is thin, linked by lld where the system's linker is
# not Apple's, which optimizes at its link itself
case "$CC" in
    *gcc*) LTO_WORDS=(-flto); LINK_WORDS=(-flto) ;;
    *) LTO_WORDS=(-flto=thin); LINK_WORDS=(-flto=thin)
       case "$(uname -s)" in
           # lld by its name, which clang's MSVC driver runs as lld-link
           MINGW*|MSYS*|CYGWIN*)
               LINK_WORDS+=(-fuse-ld=lld "-Wl,/lldltocache:$(cygpath -m "$LTO_CACHE")"
                   "-Wl,/lldltocachepolicy:cache_size_bytes=8g") ;;
           Darwin) LINK_WORDS+=("-Wl,-cache_path_lto,$LTO_CACHE") ;;
           # lld where the machine has it, on PATH or beside the compiler, under its plain name or a versioned one, the
           # newest first; else the system's linker, which links thin LTO through the compiler's plugin
           *) lld=""
              for name in ld.lld ld.lld-21 ld.lld-20 ld.lld-19 ld.lld-18 ld.lld-17; do
                  found="$(command -v "$name" 2>/dev/null || true)"
                  [ -n "$found" ] || { beside="$(dirname "$(command -v "$CC")")/$name"; [ -x "$beside" ] && found="$beside"; }
                  if [ -n "$found" ]; then lld="$found"; break; fi
              done
              if [ -n "$lld" ]; then
                  LINK_WORDS+=("-fuse-ld=$lld" "-Wl,--thinlto-cache-dir=$LTO_CACHE"
                      "-Wl,--thinlto-cache-policy=cache_size_bytes=8g")
              fi ;;
       esac ;;
esac
mkdir -p "$LTO_CACHE"
