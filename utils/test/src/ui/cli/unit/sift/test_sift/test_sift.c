// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sift.c: the byte search: the sift finds what the plain comparison finds on flat, skewed and repeating texts, a search without case
// finds each character in either case and only characters fold, a needle at the ends and in the middle of a long text is found once each, and
// empty and too long needles are found nowhere
#include "ui/cli/src/text/sift.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the max offsets a search records, and the bytes of the long text
#define SIFT_FOUND (1u << 14u)
#define SIFT_LONG ((2u << 20u) + 100u)

static const char s_long_needle[] = "across the text";

static unsigned char s_texts[6][6000];
static unsigned long long s_lengths[6];
static unsigned long long s_found[SIFT_FOUND];
static unsigned long long s_found_count;
static unsigned long long s_every[SIFT_FOUND];
static unsigned long long s_every_count;
static unsigned char s_long[SIFT_LONG];

// a found offset recorded
static void offset_recorded(const CliEachAtCfg *args)
{
    if (s_found_count < SIFT_FOUND)
    {
        s_found[s_found_count] = args->at;
    }
    s_found_count += 1ull;
}

// `byte` as a search without case reads it where `fold` is set: a capital to its small character
static unsigned char folded(unsigned char byte, int fold)
{
    return (fold && (byte >= 'A') && (byte <= 'Z')) ? (unsigned char)(byte | 0x20u) : byte;
}

// the sift finds where the needle is, by comparing at every alignment
static void assert_sift_agrees(const unsigned char *text, unsigned long long length, const unsigned char *needle, unsigned long long needle_length, int fold)
{
    s_found_count = 0ull;
    (void)EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = text, .length = length, .needle = needle, .needle_length = needle_length, .fold = fold, .found = offset_recorded, .context = NULL);
    s_every_count = 0ull;
    for (unsigned long long at = 0ull; (needle_length > 0ull) && (at + needle_length <= length); at += 1ull)
    {
        unsigned long long same = 0ull;
        while ((same < needle_length) && (folded(text[at + same], fold) == folded(needle[same], fold)))
        {
            same += 1ull;
        }
        if ((same == needle_length) && (s_every_count < SIFT_FOUND))
        {
            s_every[s_every_count] = at;
            s_every_count += 1ull;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(s_every_count, s_found_count);
    if (s_every_count > 0ull)
    {
        TEST_ASSERT_EQUAL_MEMORY(s_every, s_found, s_every_count * sizeof(s_every[0]));
    }
}

// `length` bytes from a small generator, over `alphabet` symbols from `base`, into `out`
static void bytes_generated(unsigned char *out, unsigned long long length, unsigned int alphabet, unsigned char base, unsigned long long seed)
{
    unsigned long long state = seed;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        // a symbol is below the alphabet, a few dozen at max
        out[at] = (unsigned char)(base + (unsigned char)((state >> 33u) % alphabet));
    }
}

// the long text of four needles: one first, two in the middle and one last
static void long_text_written(void)
{
    const unsigned long long length = sizeof(s_long);
    const unsigned long long needle_length = sizeof(s_long_needle) - 1u;
    memor.set(&(MemoriaCfg){.dst = s_long, .val = '.', .bytes = length});
    const unsigned long long positions[4] = {(1u << 20u) - 7u, (2u << 20u) - 3u, 0u, length - needle_length};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_long + positions[at], .src = s_long_needle, .bytes = needle_length});
    }
}

// the mixed-case fifty lines in the first text: their length
static unsigned long long case_text_written(void)
{
    for (unsigned int at = 0u; at < 50u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_texts[0] + 33u * at, .src = "Fox fox FOX fOx f@x F`X fox\n", .bytes = 28u});
    }
    return 33ull * 50ull;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_the_sift_finds_what_the_plain_comparison_finds(void)
{
    static const unsigned long long s_needles[8][2] = {{10u, 1u}, {100u, 2u}, {200u, 3u}, {300u, 5u}, {1000u, 8u}, {2000u, 17u}, {40u, 33u}, {3000u, 64u}};
    static const unsigned long long s_text_lengths[6] = {5000u, 5000u, 5000u, 5000u, 5280u, 5000u};
    bytes_generated(s_texts[0], 5000ull, 4u, 'a', 1ull);
    bytes_generated(s_texts[1], 5000ull, 26u, 'a', 2ull);
    bytes_generated(s_texts[2], 5000ull, 2u, 'x', 3ull);
    for (unsigned int at = 0u; at < 2500u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_texts[3] + 2u * at, .src = "ab", .bytes = 2u});
    }
    for (unsigned int at = 0u; at < 120u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_texts[4] + 44u * at, .src = "the quick brown fox jumps over the lazy dog\n", .bytes = 44u});
    }
    bytes_generated(s_texts[5], 5000ull, 3u, 'a', 4ull);
    for (unsigned int at = 0u; at < 5000u; at += 97u)
    {
        s_texts[5][at] = 'Q';
    }
    memor.cpy(&(MemoriaCfg){.dst = s_lengths, .src = s_text_lengths, .bytes = sizeof(s_text_lengths)});
    for (unsigned int text = 0u; text < 6u; text += 1u)
    {
        for (unsigned int needle = 0u; needle < 8u; needle += 1u)
        {
            assert_sift_agrees(s_texts[text], s_lengths[text], s_texts[text] + s_needles[needle][0], s_needles[needle][1], 0);
        }
        assert_sift_agrees(s_texts[text], s_lengths[text], (const unsigned char *)"zzzz", 4ull, 0);
    }
}

void test_a_search_without_case_agrees_with_the_plain_comparison(void)
{
    const unsigned long long length = case_text_written();
    assert_sift_agrees(s_texts[0], length, (const unsigned char *)"fox", 3ull, 1);
}

void test_a_search_without_case_finds_each_character_in_either_case(void)
{
    const unsigned long long length = case_text_written();
    TEST_ASSERT_EQUAL_UINT64(4ull * 50ull + 50ull, EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = s_texts[0], .length = length, .needle = (const unsigned char *)"FOX", .needle_length = 3ull, .fold = 1, .found = offset_recorded, .context = NULL));
}

void test_only_characters_fold(void)
{
    const unsigned long long length = case_text_written();
    TEST_ASSERT_EQUAL_UINT64(50ull, EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = s_texts[0], .length = length, .needle = (const unsigned char *)"f@x", .needle_length = 3ull, .fold = 1, .found = offset_recorded, .context = NULL));
}

void test_a_needle_at_the_ends_and_in_the_middle_of_a_long_text_is_found_once_each(void)
{
    long_text_written();
    assert_sift_agrees(s_long, sizeof(s_long), (const unsigned char *)s_long_needle, sizeof(s_long_needle) - 1u, 0);
    TEST_ASSERT_EQUAL_UINT64(4ull, s_found_count);
}

void test_a_text_with_zero_bytes_is_searched_past_them(void)
{
    // the units of "fox fox FOX" positioned as two bytes each, the high byte zero, as the editor's text is searched
    static const char s_units[] = "f\0o\0x\0 \0f\0o\0x\0 \0F\0O\0X\0";
    assert_sift_agrees((const unsigned char *)s_units, sizeof(s_units) - 1u, (const unsigned char *)"f\0o\0x", 6ull, 0);
    TEST_ASSERT_EQUAL_UINT64(2ull, s_found_count);
    assert_sift_agrees((const unsigned char *)s_units, sizeof(s_units) - 1u, (const unsigned char *)"f\0o\0x", 6ull, 1);
    TEST_ASSERT_EQUAL_UINT64(3ull, s_found_count);
}

void test_an_empty_needle_is_found_nowhere(void)
{
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = (const unsigned char *)"text", .length = 4ull, .needle = (const unsigned char *)"", .needle_length = 0ull, .fold = 0, .found = offset_recorded, .context = NULL));
}

void test_a_needle_longer_than_the_text_is_found_nowhere(void)
{
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = (const unsigned char *)"text", .length = 4ull, .needle = (const unsigned char *)"longer text", .needle_length = 11ull, .fold = 0, .found = offset_recorded, .context = NULL));
}

void test_an_empty_text_contains_nothing(void)
{
    TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = (const unsigned char *)"", .length = 0ull, .needle = (const unsigned char *)"x", .needle_length = 1ull, .fold = 0, .found = offset_recorded, .context = NULL));
}
