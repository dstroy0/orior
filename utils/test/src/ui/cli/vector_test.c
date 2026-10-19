// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector_test.c: the vector loops' check: every path finds a unit where the plain loop does, finds a run with case and
// without where the plain loop does, compares runs and counts the numbers two runs start with as the plain loop does; a
// search without case folds as the page does; and the machine runs its widest path
#include "../../../../../ui/cli/src/text/vector.h"
#include "cli_check.h"

static unsigned int s_paths[4];
static unsigned int s_path_count;

// text of every kind the loops meet, ASCII of both cases, letters outside ASCII, the two that fold into it, and units
// of surrogate pairs, `length` units from `seed`, into `out`
static void vector_text(unsigned long long seed, unsigned long long length, unsigned short *out)
{
    static const unsigned short s_kinds[16] = {'a',     'b',     'A',        'B',        's',     'S',
                                               'k',     'K',     CLI_LONG_S, CLI_KELVIN, 0x00E9u, 0x00C9u,
                                               0xD83Du, 0xDE00u, ' ',        0xFFFFu};
    unsigned long long state = (seed * 0x9E3779B97F4A7C15ull) | 1ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        out[at] = s_kinds[state % 16ull];
    }
}

// every path finds a unit where the plain loop does
static void vector_units(void)
{
    static const unsigned short s_units[6] = {'a', 'K', CLI_KELVIN, 0xDE00u, 0xFFFFu, 'z'};
    unsigned short text[80];
    int same = 1;
    for (unsigned long long seed = 0ull; seed < 300ull; seed += 1ull)
    {
        const unsigned long long length = (seed * 7ull) % 70ull;
        vector_text(seed, length, text);
        for (unsigned int unit = 0u; unit < 6u; unit += 1u)
        {
            const unsigned long long plain = cli_vector_find_unit_on(CLI_VECTOR_PLAIN, text, length, s_units[unit]);
            for (unsigned int path = 0u; path < s_path_count; path += 1u)
            {
                same &= cli_vector_find_unit_on(s_paths[path], text, length, s_units[unit]) == plain;
            }
        }
    }
    cli_check(same, "every path finds a unit where the plain loop does");
}

// every path finds a run with case and without where the plain loop does
static void vector_runs(void)
{
    static const unsigned long long s_from[3] = {0u, 1u, 9u};
    unsigned short text[100];
    unsigned short sought[8];
    unsigned short ascii[8];
    int same = 1;
    for (unsigned long long seed = 0ull; seed < 400ull; seed += 1ull)
    {
        const unsigned long long length = 10ull + (seed * 13ull) % 90ull;
        vector_text(seed, length, text);
        const unsigned long long start = (seed * 3ull) % 8ull;
        const unsigned long long sought_length = 1ull + seed % 6ull;
        if ((seed % 3ull) == 0ull)
        {
            vector_text(seed + 1000ull, sought_length, sought);
        }
        else
        {
            memor.cpy(
                &(MemoriaCfg){.dst = sought, .src = text + start + 5u, .bytes = sought_length * sizeof(sought[0])});
        }
        for (unsigned long long at = 0ull; at < sought_length; at += 1ull)
        {
            ascii[at] = (sought[at] < 0x80u) ? sought[at] : (unsigned short)'x';
        }
        for (unsigned int from = 0u; from < 3u; from += 1u)
        {
            const unsigned long long plain =
                cli_vector_find_on(CLI_VECTOR_PLAIN, text, length, sought, sought_length, s_from[from], 0);
            const unsigned long long folded =
                cli_vector_find_on(CLI_VECTOR_PLAIN, text, length, ascii, sought_length, s_from[from], 1);
            for (unsigned int path = 0u; path < s_path_count; path += 1u)
            {
                same &=
                    cli_vector_find_on(s_paths[path], text, length, sought, sought_length, s_from[from], 0) == plain;
                same &=
                    cli_vector_find_on(s_paths[path], text, length, ascii, sought_length, s_from[from], 1) == folded;
            }
        }
    }
    cli_check(same, "every path finds a run with case and without where the plain loop does");
}

// a search without case folds as the page does
static void vector_folding(void)
{
    static const unsigned short s_text[10] = {'X', 'x', ' ', CLI_LONG_S, 't', 'a', 'r', ' ', CLI_KELVIN, 'I'};
    static const unsigned short s_text_end[2] = {'N', 'G'};
    unsigned short text[12];
    memor.cpy(&(MemoriaCfg){.dst = text, .src = s_text, .bytes = sizeof(s_text)});
    memor.cpy(&(MemoriaCfg){.dst = text + 10, .src = s_text_end, .bytes = sizeof(s_text_end)});
    static const unsigned short s_star[4] = {'s', 't', 'a', 'r'};
    static const unsigned short s_king[4] = {'k', 'i', 'n', 'g'};
    static const unsigned short s_xx[2] = {'X', 'X'};
    int folds = 1;
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        folds &= cli_vector_find_on(s_paths[path], text, 12ull, s_star, 4ull, 0ull, 1) == 3ull;
        folds &= cli_vector_find_on(s_paths[path], text, 12ull, s_king, 4ull, 0ull, 1) == 8ull;
        folds &= cli_vector_find_on(s_paths[path], text, 12ull, s_xx, 2ull, 0ull, 1) == 0ull;
        folds &= cli_vector_find_on(s_paths[path], text, 12ull, s_star, 4ull, 0ull, 0) == CLI_VECTOR_NONE;
    }
    cli_check(folds, "a search without case folds as the page does, the long s to s and the Kelvin sign to k");
}

// every path compares runs and counts the numbers two runs start with as the plain loop does
static void vector_compares(void)
{
    unsigned short a[60];
    unsigned short b[60];
    unsigned long long numbers_a[60];
    unsigned long long numbers_b[60];
    int same = 1;
    for (unsigned long long seed = 0ull; seed < 300ull; seed += 1ull)
    {
        const unsigned long long length = seed % 50ull;
        vector_text(seed, length, a);
        memor.cpy(&(MemoriaCfg){.dst = b, .src = a, .bytes = length * sizeof(a[0])});
        if (((seed % 2ull) == 1ull) && (length > 0ull))
        {
            b[(seed * 11ull) % length] ^= 1u;
        }
        const int equal = memor.cmp(&(MemoriaCfg){.src = a, .other = b, .bytes = length * sizeof(a[0])}) == 0;
        for (unsigned int path = 0u; path < s_path_count; path += 1u)
        {
            same &= cli_vector_same_on(s_paths[path], a, length, b, length) == equal;
        }
        const unsigned long long count = (seed * 5ull) % 60ull;
        vector_text(seed, count, a);
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            numbers_a[at] = ((unsigned long long)a[at] % 3ull) << ((seed % 8ull) * 8ull);
            numbers_b[at] = numbers_a[at];
        }
        if (((seed % 3ull) > 0ull) && (count > 0ull))
        {
            numbers_b[(seed * 7ull) % count] ^= 1ull << ((seed % 8ull) * 8ull);
        }
        const unsigned long long shorter = ((seed % 5ull) == 0ull) ? count / 2ull : count;
        const unsigned long long plain = cli_vector_common_on(CLI_VECTOR_PLAIN, numbers_a, numbers_b, shorter);
        for (unsigned int path = 0u; path < s_path_count; path += 1u)
        {
            same &= cli_vector_common_on(s_paths[path], numbers_a, numbers_b, shorter) == plain;
        }
    }
    cli_check(same, "every path compares runs and counts their common start as the plain loop does");
}

int main(void)
{
    s_path_count = cli_vector_paths(s_paths);
    vector_units();
    vector_runs();
    vector_folding();
    vector_compares();
    int widest = (s_path_count > 0u) && (s_paths[0] == CLI_VECTOR_PLAIN);
#if defined(__x86_64__) || defined(_M_X64)
    int sse2 = 0;
    for (unsigned int at = 0u; at < s_path_count; at += 1u)
    {
        sse2 |= s_paths[at] == CLI_VECTOR_SSE2;
    }
    widest &= sse2;
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    int neon = 0;
    for (unsigned int at = 0u; at < s_path_count; at += 1u)
    {
        neon |= s_paths[at] == CLI_VECTOR_NEON;
    }
    widest &= neon;
#endif
    cli_check(widest && (cli_vector_best() == s_paths[s_path_count - 1u]), "the machine runs its widest path");
    return cli_checked();
}
