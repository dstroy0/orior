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
                  # lld before 15 resolves a weak definition over the strong one across thin LTO's modules, so MMgr's weak
                  # owner table defaults would win over the program's table; that lld links the whole program as one
                  # module instead, which resolves the two as the language says
                  lld_major="$("$lld" --version 2>/dev/null | sed -n 's/.*LLD \([0-9][0-9]*\)\..*/\1/p' | head -1)"
                  if [ -n "$lld_major" ] && [ "$lld_major" -lt 15 ]; then
                      LTO_WORDS=(-flto)
                      LINK_WORDS=(-flto "-fuse-ld=$lld")
                  else
                      LINK_WORDS+=("-fuse-ld=$lld" "-Wl,--thinlto-cache-dir=$LTO_CACHE"
                          "-Wl,--thinlto-cache-policy=cache_size_bytes=8g")
                  fi
              fi ;;
       esac ;;
esac
mkdir -p "$LTO_CACHE"
