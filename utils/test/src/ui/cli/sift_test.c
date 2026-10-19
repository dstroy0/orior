// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift_test.c: the byte search's check: every path finds what the plain comparison finds on texts flat, skewed and
// repeating; a search without case finds each letter in either case and only letters fold; a needle across two windows
// is found once; the first anchor stands on the rarest byte and a level that refutes nothing more ends the descent; and
// empty and too long needles stand nowhere
#include "../../../../../ui/cli/src/text/sift.h"
#include "../../../../../ui/cli/src/text/vector.h"
#include "cli_check.h"

// the most offsets a search keeps
#define SIFT_FOUND (1u << 14u)

static unsigned char s_texts[6][6000];
static unsigned long long s_lengths[6];
static unsigned long long s_found[SIFT_FOUND];
static unsigned long long s_found_count;
static unsigned long long s_every[SIFT_FOUND];
static unsigned long long s_every_count;
static unsigned char s_wide[CLI_SIFT_WINDOW * 2u + 100u];
static unsigned char s_flat[20000];

// an offset found kept
static void sift_kept(void *context, unsigned long long at)
{
    (void)context;
    if (s_found_count < SIFT_FOUND)
    {
        s_found[s_found_count] = at;
    }
    s_found_count += 1ull;
}

// where the needle stands, by comparing at every alignment, into s_every
static void sift_every(const unsigned char *text, unsigned long long length, const unsigned char *needle,
                       unsigned long long needle_length, int fold)
{
    s_every_count = 0ull;
    for (unsigned long long at = 0ull; (needle_length > 0ull) && (at + needle_length <= length); at += 1ull)
    {
        unsigned long long same = 0ull;
        while ((same < needle_length) &&
               (cli_sift_folded(text[at + same], fold) == cli_sift_folded(needle[same], fold)))
        {
            same += 1ull;
        }
        if ((same == needle_length) && (s_every_count < SIFT_FOUND))
        {
            s_every[s_every_count] = at;
            s_every_count += 1ull;
        }
    }
}

// whether the path `path` finds what the plain comparison finds
static int sift_agrees(unsigned int path, const unsigned char *text, unsigned long long length,
                       const unsigned char *needle, unsigned long long needle_length, int fold)
{
    s_found_count = 0ull;
    (void)cli_sift_find_all_on(path, text, length, needle, needle_length, fold, sift_kept, NULL);
    sift_every(text, length, needle, needle_length, fold);
    return (s_found_count == s_every_count) &&
           (memor.cmp(&(MemoriaCfg){.src = s_found, .other = s_every, .bytes = s_every_count * sizeof(s_every[0])}) ==
            0);
}

// `length` bytes from a small generator, over `alphabet` symbols from `base`, into `out`
static void sift_made(unsigned char *out, unsigned long long length, unsigned int alphabet, unsigned char base,
                      unsigned long long seed)
{
    unsigned long long state = seed;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        // a symbol is below the alphabet, a few dozen at most
        out[at] = (unsigned char)(base + (unsigned char)((state >> 33u) % alphabet));
    }
}

// every path finds what the plain comparison finds on texts flat, skewed and repeating
static void sift_paths(void)
{
    static const unsigned long long s_needles[8][2] = {{10u, 1u},   {100u, 2u},   {200u, 3u}, {300u, 5u},
                                                       {1000u, 8u}, {2000u, 17u}, {40u, 33u}, {3000u, 64u}};
    sift_made(s_texts[0], 5000ull, 4u, 'a', 1ull);
    sift_made(s_texts[1], 5000ull, 26u, 'a', 2ull);
    sift_made(s_texts[2], 5000ull, 2u, 'x', 3ull);
    for (unsigned int at = 0u; at < 2500u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_texts[3] + 2u * at, .src = "ab", .bytes = 2u});
    }
    for (unsigned int at = 0u; at < 120u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_texts[4] + 44u * at, .src = "the quick brown fox jumps over the lazy dog\n", .bytes = 44u});
    }
    sift_made(s_texts[5], 5000ull, 3u, 'a', 4ull);
    for (unsigned int at = 0u; at < 5000u; at += 97u)
    {
        s_texts[5][at] = 'Q';
    }
    const unsigned long long lengths[6] = {5000u, 5000u, 5000u, 5000u, 5280u, 5000u};
    memor.cpy(&(MemoriaCfg){.dst = s_lengths, .src = lengths, .bytes = sizeof(lengths)});
    unsigned int paths[4];
    const unsigned int path_count = cli_vector_paths(paths);
    int agree = 1;
    for (unsigned int text = 0u; text < 6u; text += 1u)
    {
        for (unsigned int needle = 0u; needle < 8u; needle += 1u)
        {
            for (unsigned int path = 0u; path < path_count; path += 1u)
            {
                agree &= sift_agrees(paths[path], s_texts[text], s_lengths[text], s_texts[text] + s_needles[needle][0],
                                     s_needles[needle][1], 0);
            }
        }
        for (unsigned int path = 0u; path < path_count; path += 1u)
        {
            agree &= sift_agrees(paths[path], s_texts[text], s_lengths[text], (const unsigned char *)"zzzz", 4ull, 0);
        }
    }
    cli_check(agree, "every path finds what the plain comparison finds");
}

// a search without case finds each letter in either case and only letters fold
static void sift_case(void)
{
    for (unsigned int at = 0u; at < 50u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_texts[0] + 33u * at, .src = "Fox fox FOX fOx f@x F`X fox\n", .bytes = 28u});
    }
    unsigned int paths[4];
    const unsigned int path_count = cli_vector_paths(paths);
    int agree = 1;
    int counted = 1;
    for (unsigned int path = 0u; path < path_count; path += 1u)
    {
        agree &= sift_agrees(paths[path], s_texts[0], 33u * 50u, (const unsigned char *)"fox", 3ull, 1);
        counted &= cli_sift_find_all_on(paths[path], s_texts[0], 33u * 50u, (const unsigned char *)"FOX", 3ull, 1,
                                        sift_kept, NULL) == 4ull * 50ull + 50ull;
        counted &= cli_sift_find_all_on(paths[path], s_texts[0], 33u * 50u, (const unsigned char *)"f@x", 3ull, 1,
                                        sift_kept, NULL) == 50ull;
    }
    cli_check(agree && counted, "a search without case finds each letter in either case, and only letters fold");
}

// a needle across two windows is found once
static void sift_seam(void)
{
    static const char s_needle[] = "across the seam";
    const unsigned long long length = sizeof(s_wide);
    const unsigned long long needle_length = sizeof(s_needle) - 1u;
    memor.set(&(MemoriaCfg){.dst = s_wide, .val = '.', .bytes = length});
    const unsigned long long places[4] = {CLI_SIFT_WINDOW - 7u, CLI_SIFT_WINDOW * 2u - 3u, 0u, length - needle_length};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_wide + places[at], .src = s_needle, .bytes = needle_length});
    }
    unsigned int paths[4];
    const unsigned int path_count = cli_vector_paths(paths);
    int agree = 1;
    for (unsigned int path = 0u; path < path_count; path += 1u)
    {
        agree &= sift_agrees(paths[path], s_wide, length, (const unsigned char *)s_needle, needle_length, 0);
    }
    cli_check(agree, "a needle across two windows is found once");
}

int main(void)
{
    sift_paths();
    sift_case();
    sift_seam();
    static unsigned char s_text[4010];
    for (unsigned int at = 0u; at < 400u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_text + 10u * at, .src = "aaaaaaaaab", .bytes = 10u});
    }
    memor.cpy(&(MemoriaCfg){.dst = s_text + 4000, .src = "aaaQaaaa", .bytes = 8u});
    unsigned long long anchors[CLI_SIFT_ANCHORS];
    const unsigned int placed = cli_sift_plan(s_text, 4008ull, (const unsigned char *)"aaaQaaaa", 8ull, 0, anchors);
    cli_check((placed == 1u) && (anchors[0] == 3ull), "the first anchor stands on the rarest byte, the Q, alone");
    sift_made(s_flat, sizeof(s_flat), 26u, 'a', 9ull);
    cli_check(cli_sift_plan(s_flat, sizeof(s_flat), s_flat + 100, 8ull, 0, anchors) == 3u,
              "three anchors of twenty-six letters, and a fourth refutes nothing more");
    cli_check((cli_sift_find_all((const unsigned char *)"text", 4ull, (const unsigned char *)"", 0ull, 0, sift_kept,
                                 NULL) == 0ull) &&
                  (cli_sift_find_all((const unsigned char *)"text", 4ull, (const unsigned char *)"longer text", 11ull,
                                     0, sift_kept, NULL) == 0ull) &&
                  (cli_sift_find_all((const unsigned char *)"", 0ull, (const unsigned char *)"x", 1ull, 0, sift_kept,
                                     NULL) == 0ull),
              "empty and too long needles stand nowhere");
    return cli_checked();
}
