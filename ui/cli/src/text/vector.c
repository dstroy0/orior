// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector.c: the paths this machine runs, each loop sent down the path given or the widest, and the plain loops
#include "vector.h"

unsigned int cli_vector_paths(unsigned int *paths)
{
    unsigned int count = 0u;
    paths[count] = CLI_VECTOR_PLAIN;
    count += 1u;
#if ENGINE_TARGET_X86_64
    paths[count] = CLI_VECTOR_SSE2;
    count += 1u;
    if (cli_vector_avx2_present())
    {
        paths[count] = CLI_VECTOR_AVX2;
        count += 1u;
    }
#endif
#if ENGINE_TARGET_AARCH64
    paths[count] = CLI_VECTOR_NEON;
    count += 1u;
#endif
    return count;
}

// the widest path, read once: every thread that reads it before it is kept reads the same answer and keeps it
#if defined(_MSC_VER) && !defined(__clang__)
static volatile int s_best = -1;
#else
static int s_best = -1;
#endif

unsigned int cli_vector_best(void)
{
    if (s_best < 0)
    {
        unsigned int paths[4];
        const unsigned int count = cli_vector_paths(paths);
        // a path is one of four, which an int holds
        s_best = (int)paths[count - 1u];
    }
    // read back as the path it was kept as
    return (unsigned int)s_best;
}

unsigned short cli_vector_fold(unsigned short unit)
{
    if ((unit >= 0x41u) && (unit <= 0x5au))
    {
        // a capital and its small letter differ in the one bit 0x20
        return (unsigned short)(unit | 0x20u);
    }
    return (unit == CLI_LONG_S) ? (unsigned short)'s' : (unit == CLI_KELVIN) ? (unsigned short)'k' : unit;
}

int cli_vector_stands_at(const unsigned short *text, const unsigned short *sought, unsigned long long sought_length,
                         unsigned long long at, int folded)
{
    for (unsigned long long step = 0ull; step < sought_length; step += 1ull)
    {
        const unsigned short here = folded ? cli_vector_fold(text[at + step]) : text[at + step];
        const unsigned short wanted = folded ? cli_vector_fold(sought[step]) : sought[step];
        if (here != wanted)
        {
            return 0;
        }
    }
    return 1;
}

unsigned long long cli_vector_plain_find(const unsigned short *text, unsigned long long length,
                                         const unsigned short *sought, unsigned long long sought_length, int folded,
                                         unsigned long long start)
{
    if (sought_length > length)
    {
        return CLI_VECTOR_NONE;
    }
    for (unsigned long long at = start; at <= (length - sought_length); at += 1ull)
    {
        if (cli_vector_stands_at(text, sought, sought_length, at, folded))
        {
            return at;
        }
    }
    return CLI_VECTOR_NONE;
}

unsigned long long cli_vector_common_on(unsigned int path, const unsigned long long *a, const unsigned long long *b,
                                        unsigned long long length)
{
#if ENGINE_TARGET_X86_64
    if (path == CLI_VECTOR_SSE2)
    {
        return cli_vector_common_sse2(a, b, length);
    }
    if ((path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return cli_vector_common_avx2(a, b, length);
    }
#endif
#if ENGINE_TARGET_AARCH64
    if (path == CLI_VECTOR_NEON)
    {
        return cli_vector_common_neon(a, b, length);
    }
#endif
    unsigned long long at = 0ull;
    while ((at < length) && (a[at] == b[at]))
    {
        at += 1ull;
    }
    return at;
}

unsigned long long cli_vector_find_unit_on(unsigned int path, const unsigned short *text, unsigned long long length,
                                           unsigned short unit)
{
#if ENGINE_TARGET_X86_64
    if (path == CLI_VECTOR_SSE2)
    {
        return cli_vector_find_unit_sse2(text, length, unit);
    }
    if ((path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return cli_vector_find_unit_avx2(text, length, unit);
    }
#endif
#if ENGINE_TARGET_AARCH64
    if (path == CLI_VECTOR_NEON)
    {
        return cli_vector_find_unit_neon(text, length, unit);
    }
#endif
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (text[at] == unit)
        {
            return at;
        }
    }
    return CLI_VECTOR_NONE;
}

int cli_vector_same_on(unsigned int path, const unsigned short *a, unsigned long long a_length, const unsigned short *b,
                       unsigned long long b_length)
{
    if (a_length != b_length)
    {
        return 0;
    }
#if ENGINE_TARGET_X86_64
    if (path == CLI_VECTOR_SSE2)
    {
        return cli_vector_same_sse2(a, b, a_length);
    }
    if ((path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return cli_vector_same_avx2(a, b, a_length);
    }
#endif
#if ENGINE_TARGET_AARCH64
    if (path == CLI_VECTOR_NEON)
    {
        return cli_vector_same_neon(a, b, a_length);
    }
#endif
    return memor.cmp(&(MemoriaCfg){.src = a, .other = b, .bytes = a_length * sizeof(a[0])}) == 0;
}

// Each place both the first and the last unit sought stand where they would, found a block at a time, then checked
// whole; folded where `folded` is 1
unsigned long long cli_vector_find_on(unsigned int path, const unsigned short *text, unsigned long long length,
                                      const unsigned short *sought, unsigned long long sought_length,
                                      unsigned long long from, int folded)
{
    if (sought_length == 0ull)
    {
        return (from <= length) ? from : CLI_VECTOR_NONE;
    }
    if ((from >= length) || ((length - from) < sought_length))
    {
        return CLI_VECTOR_NONE;
    }
    const unsigned short *const rest = text + from;
    const unsigned long long rest_length = length - from;
    if ((sought_length == 1ull) && !folded)
    {
        const unsigned long long at = cli_vector_find_unit_on(path, rest, rest_length, sought[0]);
        return (at == CLI_VECTOR_NONE) ? at : (at + from);
    }
    unsigned long long found = CLI_VECTOR_NONE;
#if ENGINE_TARGET_X86_64
    if (path == CLI_VECTOR_SSE2)
    {
        found = cli_vector_find_sse2(rest, rest_length, sought, sought_length, folded);
    }
    else if ((path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        found = cli_vector_find_avx2(rest, rest_length, sought, sought_length, folded);
    }
    else
#endif
#if ENGINE_TARGET_AARCH64
        if (path == CLI_VECTOR_NEON)
    {
        found = cli_vector_find_neon(rest, rest_length, sought, sought_length, folded);
    }
    else
#endif
    {
        found = cli_vector_plain_find(rest, rest_length, sought, sought_length, folded, 0ull);
    }
    return (found == CLI_VECTOR_NONE) ? found : (found + from);
}

unsigned long long cli_vector_find_unit(const unsigned short *text, unsigned long long length, unsigned short unit)
{
    return cli_vector_find_unit_on(cli_vector_best(), text, length, unit);
}

unsigned long long cli_vector_find(const unsigned short *text, unsigned long long length, const unsigned short *sought,
                                   unsigned long long sought_length, unsigned long long from)
{
    return cli_vector_find_on(cli_vector_best(), text, length, sought, sought_length, from, 0);
}

unsigned long long cli_vector_find_folded(const unsigned short *text, unsigned long long length,
                                          const unsigned short *sought, unsigned long long sought_length,
                                          unsigned long long from)
{
    return cli_vector_find_on(cli_vector_best(), text, length, sought, sought_length, from, 1);
}

int cli_vector_same(const unsigned short *a, unsigned long long a_length, const unsigned short *b,
                    unsigned long long b_length)
{
    return cli_vector_same_on(cli_vector_best(), a, a_length, b, b_length);
}

unsigned long long cli_vector_common(const unsigned long long *a, unsigned long long a_length,
                                     const unsigned long long *b, unsigned long long b_length)
{
    return cli_vector_common_on(cli_vector_best(), a, b, (a_length < b_length) ? a_length : b_length);
}
