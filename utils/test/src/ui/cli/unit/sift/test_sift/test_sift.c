// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sift.c: the byte search: every path finds what the plain comparison finds on flat, skewed and repeating texts, a search without case
// finds each letter in either case and only letters fold, a needle across two windows is found once, the first anchor is on the rarest
// byte, a level that refutes nothing more ends the descent, and empty and too long needles are found nowhere
#include "ui/cli/src/text/sift.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/text/vector.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the most offsets a search records
#define SIFT_FOUND (1u << 14u)

static const char s_seam_needle[] = "across the seam";

static unsigned char s_texts[6][6000];
static unsigned long long s_lengths[6];
static unsigned long long s_found[SIFT_FOUND];
static unsigned long long s_found_count;
static unsigned long long s_every[SIFT_FOUND];
static unsigned long long s_every_count;
static unsigned char s_wide[CLI_SIFT_WINDOW * 2u + 100u];
static unsigned char s_flat[20000];
static unsigned int s_paths[4];
static unsigned int s_path_count;

// a found offset recorded
static void offset_recorded(const CliEachAtCfg *args)
{
    if (s_found_count < SIFT_FOUND)
    {
        s_found[s_found_count] = args->at;
    }
    s_found_count += 1ull;
}

// a found offset counted
static void offset_counted(const CliEachAtCfg *args)
{
    void *context = args->context;
    *(unsigned long long *)context += 1ull;
}

// the path `path` finds where the needle is, by comparing at every alignment
static void assert_path_agrees(unsigned int path, const unsigned char *text, unsigned long long length, const unsigned char *needle, unsigned long long needle_length, int fold)
{
    s_found_count = 0ull;
    (void)EMBED_CALL(cli_sift_length.find_all_on, CliSiftLengthCfg, .path = path, .text = text, .length = length, .needle = needle, .needle_length = needle_length, .fold = fold, .found = offset_recorded, .context = NULL);
    s_every_count = 0ull;
    for (unsigned long long at = 0ull; (needle_length > 0ull) && (at + needle_length <= length); at += 1ull)
    {
        unsigned long long same = 0ull;
        while ((same < needle_length) && (EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = text[at + same], .fold = fold) == EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[same], .fold = fold)))
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
        // a symbol is below the alphabet, a few dozen at most
        out[at] = (unsigned char)(base + (unsigned char)((state >> 33u) % alphabet));
    }
}

// the wide text of four needles: two across windows, one first and one last
static void seam_text_written(void)
{
    const unsigned long long length = sizeof(s_wide);
    const unsigned long long needle_length = sizeof(s_seam_needle) - 1u;
    memor.set(&(MemoriaCfg){.dst = s_wide, .val = '.', .bytes = length});
    const unsigned long long positions[4] = {CLI_SIFT_WINDOW - 7u, CLI_SIFT_WINDOW * 2u - 3u, 0u, length - needle_length};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_wide + positions[at], .src = s_seam_needle, .bytes = needle_length});
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
    s_path_count = EMBED_CALL(cli_vector.paths, CliVectorCfg, .paths = s_paths);
}

void tearDown(void)
{
}

void test_every_path_finds_what_the_plain_comparison_finds(void)
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
        for (unsigned int path = 0u; path < s_path_count; path += 1u)
        {
            for (unsigned int needle = 0u; needle < 8u; needle += 1u)
            {
                assert_path_agrees(s_paths[path], s_texts[text], s_lengths[text], s_texts[text] + s_needles[needle][0], s_needles[needle][1], 0);
            }
            assert_path_agrees(s_paths[path], s_texts[text], s_lengths[text], (const unsigned char *)"zzzz", 4ull, 0);
        }
    }
}

void test_a_search_without_case_agrees_with_the_plain_comparison(void)
{
    const unsigned long long length = case_text_written();
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        assert_path_agrees(s_paths[path], s_texts[0], length, (const unsigned char *)"fox", 3ull, 1);
    }
}

void test_a_search_without_case_finds_each_letter_in_either_case(void)
{
    const unsigned long long length = case_text_written();
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(4ull * 50ull + 50ull, EMBED_CALL(cli_sift_length.find_all_on, CliSiftLengthCfg, .path = s_paths[path], .text = s_texts[0], .length = length, .needle = (const unsigned char *)"FOX", .needle_length = 3ull, .fold = 1, .found = offset_recorded, .context = NULL));
    }
}

void test_only_letters_fold(void)
{
    const unsigned long long length = case_text_written();
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(50ull, EMBED_CALL(cli_sift_length.find_all_on, CliSiftLengthCfg, .path = s_paths[path], .text = s_texts[0], .length = length, .needle = (const unsigned char *)"f@x", .needle_length = 3ull, .fold = 1, .found = offset_recorded, .context = NULL));
    }
}

void test_a_needle_across_two_windows_is_found_once(void)
{
    seam_text_written();
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        assert_path_agrees(s_paths[path], s_wide, sizeof(s_wide), (const unsigned char *)s_seam_needle, sizeof(s_seam_needle) - 1u, 0);
    }
}

void test_a_histogram_counts_each_byte(void)
{
    static const unsigned char s_bytes[] = "Aa Bb AAA zz\nQ";
    CliHistogram histogram;
    memor.set(&(MemoriaCfg){.dst = &histogram, .val = 0, .bytes = sizeof(histogram)});
    EMBED_CALL(cli_sift_length_histogram.histogram_count, CliSiftLengthHistogramCfg, .histogram = &histogram, .bytes = s_bytes, .length = sizeof(s_bytes) - 1u);
    TEST_ASSERT_EQUAL_UINT64(sizeof(s_bytes) - 1u, histogram.total);
    TEST_ASSERT_EQUAL_UINT64(4ull, histogram.occurrences['A']);
    TEST_ASSERT_EQUAL_UINT64(1ull, histogram.occurrences['a']);
}

void test_bytes_uncounted_leave_a_histogram(void)
{
    static const unsigned char s_bytes[] = "Aa Bb AAA zz\nQ";
    CliHistogram histogram;
    memor.set(&(MemoriaCfg){.dst = &histogram, .val = 0, .bytes = sizeof(histogram)});
    EMBED_CALL(cli_sift_length_histogram.histogram_count, CliSiftLengthHistogramCfg, .histogram = &histogram, .bytes = s_bytes, .length = sizeof(s_bytes) - 1u);
    EMBED_CALL(cli_sift_length_histogram.histogram_uncounted, CliSiftLengthHistogramCfg, .histogram = &histogram, .bytes = s_bytes + 6, .length = 3ull);
    TEST_ASSERT_EQUAL_UINT64(sizeof(s_bytes) - 4u, histogram.total);
    TEST_ASSERT_EQUAL_UINT64(1ull, histogram.occurrences['A']);
}

void test_a_folded_histogram_adds_each_capitals_count_to_its_small_letter(void)
{
    static const unsigned char s_bytes[] = "Aa Bb AAA zz\nQ";
    CliHistogram histogram;
    CliHistogram folded;
    memor.set(&(MemoriaCfg){.dst = &histogram, .val = 0, .bytes = sizeof(histogram)});
    EMBED_CALL(cli_sift_length_histogram.histogram_count, CliSiftLengthHistogramCfg, .histogram = &histogram, .bytes = s_bytes, .length = sizeof(s_bytes) - 1u);
    EMBED_CALL(cli_sift_length_histogram.histogram_uncounted, CliSiftLengthHistogramCfg, .histogram = &histogram, .bytes = s_bytes + 6, .length = 3ull);
    EMBED_CALL(cli_sift.histogram_fold, CliSiftCfg, .exact = &histogram, .folded = &folded);
    TEST_ASSERT_EQUAL_UINT64(2ull, folded.occurrences['a']);
    TEST_ASSERT_EQUAL_UINT64(0ull, folded.occurrences['A']);
    TEST_ASSERT_EQUAL_UINT64(1ull, folded.occurrences['q']);
    TEST_ASSERT_EQUAL_UINT64(histogram.total, folded.total);
}

void test_a_search_planned_from_a_stored_histogram_finds_every_position(void)
{
    CliHistogram whole;
    unsigned long long found = 0ull;
    seam_text_written();
    memor.set(&(MemoriaCfg){.dst = &whole, .val = 0, .bytes = sizeof(whole)});
    EMBED_CALL(cli_sift_length_histogram.histogram_count, CliSiftLengthHistogramCfg, .histogram = &whole, .bytes = s_wide, .length = sizeof(s_wide));
    TEST_ASSERT_EQUAL_UINT64(4ull, EMBED_CALL(cli_sift_length_fold.find_all_with, CliSiftLengthFoldCfg, .histogram_read = &whole, .text = s_wide, .length = sizeof(s_wide), .needle = (const unsigned char *)s_seam_needle, .needle_length = sizeof(s_seam_needle) - 1u, .fold = 0, .found = offset_counted, .context = &found));
}

void test_the_first_anchor_is_on_the_rarest_byte_alone(void)
{
    static unsigned char s_text[4010];
    unsigned long long anchors[CLI_SIFT_ANCHORS];
    for (unsigned int at = 0u; at < 400u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_text + 10u * at, .src = "aaaaaaaaab", .bytes = 10u});
    }
    memor.cpy(&(MemoriaCfg){.dst = s_text + 4000, .src = "aaaQaaaa", .bytes = 8u});
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_sift_window_length_fold.plan, CliSiftWindowLengthFoldCfg, .window = s_text, .length = 4008ull, .needle = (const unsigned char *)"aaaQaaaa", .needle_length = 8ull, .fold = 0, .anchors_out = anchors));
    TEST_ASSERT_EQUAL_UINT64(3ull, anchors[0]);
}

void test_three_anchors_of_a_flat_alphabet_and_a_fourth_refutes_nothing_more(void)
{
    unsigned long long anchors[CLI_SIFT_ANCHORS];
    bytes_generated(s_flat, sizeof(s_flat), 26u, 'a', 9ull);
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_sift_window_length_fold.plan, CliSiftWindowLengthFoldCfg, .window = s_flat, .length = sizeof(s_flat), .needle = s_flat + 100, .needle_length = 8ull, .fold = 0, .anchors_out = anchors));
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
