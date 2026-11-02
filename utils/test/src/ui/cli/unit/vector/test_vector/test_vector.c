// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_vector.c: every vector path finds a unit and a run where the plain loop does, compares runs and counts their common start as it
// does, a search without case folds as the page does, and the machine runs its widest path
#include "ui/cli/src/text/vector.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const unsigned short s_star[4] = {'s', 't', 'a', 'r'};
static const unsigned short s_king[4] = {'k', 'i', 'n', 'g'};
static const unsigned short s_xx[2] = {'X', 'X'};

static unsigned int s_paths[4];
static unsigned int s_path_count;
static unsigned short s_folding[12];

// text of every kind the loops meet, ASCII of both cases, letters outside ASCII, the two that fold into it, and surrogate units,
// `length` units from `seed`, into `out`
static void text_written(unsigned long long seed, unsigned long long length, unsigned short *out)
{
    static const unsigned short s_kinds[16] = {'a', 'b', 'A', 'B', 's', 'S', 'k', 'K', CLI_LONG_S, CLI_KELVIN, 0x00E9u, 0x00C9u, 0xD83Du, 0xDE00u, ' ', 0xFFFFu};
    unsigned long long state = (seed * 0x9E3779B97F4A7C15ull) | 1ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        out[at] = s_kinds[state % 16ull];
    }
}

void setUp(void)
{
    static const unsigned short s_text[12] = {'X', 'x', ' ', CLI_LONG_S, 't', 'a', 'r', ' ', CLI_KELVIN, 'I', 'N', 'G'};
    cli_unity_begun();
    s_path_count = EMBED_CALL(cli_vector.paths, CliVectorCfg, .paths = s_paths);
    memor.cpy(&(MemoriaCfg){.dst = s_folding, .src = s_text, .bytes = sizeof(s_text)});
}

void tearDown(void)
{
}

void test_every_path_finds_a_unit_where_the_plain_loop_does(void)
{
    static const unsigned short s_units[6] = {'a', 'K', CLI_KELVIN, 0xDE00u, 0xFFFFu, 'z'};
    unsigned short text[80];
    for (unsigned long long seed = 0ull; seed < 300ull; seed += 1ull)
    {
        const unsigned long long length = (seed * 7ull) % 70ull;
        text_written(seed, length, text);
        for (unsigned int unit = 0u; unit < 6u; unit += 1u)
        {
            const unsigned long long plain = EMBED_CALL(cli_vector_unit.find_unit_on, CliVectorUnitCfg, .path = CLI_VECTOR_PLAIN, .text = text, .length = length, .unit = s_units[unit]);
            for (unsigned int path = 0u; path < s_path_count; path += 1u)
            {
                TEST_ASSERT_EQUAL_UINT64(plain, EMBED_CALL(cli_vector_unit.find_unit_on, CliVectorUnitCfg, .path = s_paths[path], .text = text, .length = length, .unit = s_units[unit]));
            }
        }
    }
}

void test_every_path_finds_a_run_with_and_without_case_where_the_plain_loop_does(void)
{
    static const unsigned long long s_from[3] = {0u, 1u, 9u};
    unsigned short text[100];
    unsigned short sought[8];
    unsigned short ascii[8];
    for (unsigned long long seed = 0ull; seed < 400ull; seed += 1ull)
    {
        const unsigned long long length = 10ull + (seed * 13ull) % 90ull;
        text_written(seed, length, text);
        const unsigned long long start = (seed * 3ull) % 8ull;
        const unsigned long long sought_length = 1ull + seed % 6ull;
        if ((seed % 3ull) == 0ull)
        {
            text_written(seed + 1000ull, sought_length, sought);
        }
        else
        {
            memor.cpy(&(MemoriaCfg){.dst = sought, .src = text + start + 5u, .bytes = sought_length * sizeof(sought[0])});
        }
        for (unsigned long long at = 0ull; at < sought_length; at += 1ull)
        {
            ascii[at] = (sought[at] < 0x80u) ? sought[at] : (unsigned short)'x';
        }
        for (unsigned int from = 0u; from < 3u; from += 1u)
        {
            const unsigned long long plain = EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = CLI_VECTOR_PLAIN, .text = text, .length = length, .sought = sought, .sought_length = sought_length, .from = s_from[from], .folded = 0);
            const unsigned long long folded = EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = CLI_VECTOR_PLAIN, .text = text, .length = length, .sought = ascii, .sought_length = sought_length, .from = s_from[from], .folded = 1);
            for (unsigned int path = 0u; path < s_path_count; path += 1u)
            {
                TEST_ASSERT_EQUAL_UINT64(plain, EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = s_paths[path], .text = text, .length = length, .sought = sought, .sought_length = sought_length, .from = s_from[from], .folded = 0));
                TEST_ASSERT_EQUAL_UINT64(folded, EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = s_paths[path], .text = text, .length = length, .sought = ascii, .sought_length = sought_length, .from = s_from[from], .folded = 1));
            }
        }
    }
}

void test_a_search_without_case_folds_the_long_s_to_s(void)
{
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(3ull, EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = s_paths[path], .text = s_folding, .length = 12ull, .sought = s_star, .sought_length = 4ull, .from = 0ull, .folded = 1));
    }
}

void test_a_search_without_case_folds_the_kelvin_sign_to_k(void)
{
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(8ull, EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = s_paths[path], .text = s_folding, .length = 12ull, .sought = s_king, .sought_length = 4ull, .from = 0ull, .folded = 1));
    }
}

void test_a_search_without_case_folds_ascii(void)
{
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(0ull, EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = s_paths[path], .text = s_folding, .length = 12ull, .sought = s_xx, .sought_length = 2ull, .from = 0ull, .folded = 1));
    }
}

void test_a_search_with_case_does_not_fold(void)
{
    for (unsigned int path = 0u; path < s_path_count; path += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(CLI_VECTOR_NONE, EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = s_paths[path], .text = s_folding, .length = 12ull, .sought = s_star, .sought_length = 4ull, .from = 0ull, .folded = 0));
    }
}

void test_every_path_compares_runs_as_the_plain_loop_does(void)
{
    unsigned short left[60];
    unsigned short right[60];
    for (unsigned long long seed = 0ull; seed < 300ull; seed += 1ull)
    {
        const unsigned long long length = seed % 50ull;
        text_written(seed, length, left);
        memor.cpy(&(MemoriaCfg){.dst = right, .src = left, .bytes = length * sizeof(left[0])});
        if (((seed % 2ull) == 1ull) && (length > 0ull))
        {
            right[(seed * 11ull) % length] ^= 1u;
        }
        const int equal = memor.cmp(&(MemoriaCfg){.src = left, .other = right, .bytes = length * sizeof(left[0])}) == 0;
        for (unsigned int path = 0u; path < s_path_count; path += 1u)
        {
            TEST_ASSERT_EQUAL_INT(equal, EMBED_CALL(cli_vector_a.same_on, CliVectorACfg, .path = s_paths[path], .a = left, .a_length = length, .b = right, .b_length = length));
        }
    }
}

void test_every_path_counts_a_common_start_as_the_plain_loop_does(void)
{
    unsigned short text[60];
    unsigned long long numbers_left[60];
    unsigned long long numbers_right[60];
    for (unsigned long long seed = 0ull; seed < 300ull; seed += 1ull)
    {
        const unsigned long long count = (seed * 5ull) % 60ull;
        text_written(seed, count, text);
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            numbers_left[at] = ((unsigned long long)text[at] % 3ull) << ((seed % 8ull) * 8ull);
            numbers_right[at] = numbers_left[at];
        }
        if (((seed % 3ull) > 0ull) && (count > 0ull))
        {
            numbers_right[(seed * 7ull) % count] ^= 1ull << ((seed % 8ull) * 8ull);
        }
        const unsigned long long shorter = ((seed % 5ull) == 0ull) ? count / 2ull : count;
        const unsigned long long plain = EMBED_CALL(cli_vector_length.common_on, CliVectorLengthCfg, .path = CLI_VECTOR_PLAIN, .a_u64s = numbers_left, .b_u64s = numbers_right, .length = shorter);
        for (unsigned int path = 0u; path < s_path_count; path += 1u)
        {
            TEST_ASSERT_EQUAL_UINT64(plain, EMBED_CALL(cli_vector_length.common_on, CliVectorLengthCfg, .path = s_paths[path], .a_u64s = numbers_left, .b_u64s = numbers_right, .length = shorter));
        }
    }
}

void test_the_plain_path_is_listed_first(void)
{
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_path_count);
    TEST_ASSERT_EQUAL_UINT(CLI_VECTOR_PLAIN, s_paths[0]);
}

void test_the_machines_vector_path_is_listed(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    const unsigned int wanted = CLI_VECTOR_SSE2;
#elif defined(__aarch64__) || defined(_M_ARM64)
    const unsigned int wanted = CLI_VECTOR_NEON;
#else
    const unsigned int wanted = CLI_VECTOR_PLAIN;
#endif
    int listed = 0;
    for (unsigned int at = 0u; at < s_path_count; at += 1u)
    {
        listed |= s_paths[at] == wanted;
    }
    TEST_ASSERT_TRUE(listed);
}

void test_the_machine_runs_its_widest_path(void)
{
    TEST_ASSERT_EQUAL_UINT(s_paths[s_path_count - 1u], cli_vector_best());
}
