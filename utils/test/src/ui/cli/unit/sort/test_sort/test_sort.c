// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sort.c: the sort: no records and one record are, two records swap, a short run goes by insertion, a long run
// by the quicksort and a run of one value by the heap's fallback, records of an odd size move whole, every record
// stays present, and the order given is the order the records end in
#include "ui/cli/src/text/sort.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define SORT_MANY 5000u

static unsigned int s_values[SORT_MANY];
static unsigned int s_seen[SORT_MANY];

// A record of an odd size: its key and the bytes that ride with it
typedef struct
{
    unsigned int key;
    unsigned char tail[9];
} SortOdd;

static SortOdd s_odd[257];

static int order_up(const void *one, const void *other)
{
    const unsigned int left = *(const unsigned int *)one;
    const unsigned int right = *(const unsigned int *)other;
    return (left < right) ? -1 : ((left > right) ? 1 : 0);
}

static int order_down(const void *one, const void *other)
{
    return order_up(other, one);
}

static int order_odd(const void *one, const void *other)
{
    const SortOdd *const left = one;
    const SortOdd *const right = other;
    return (left->key < right->key) ? -1 : ((left->key > right->key) ? 1 : 0);
}

// the values filled from a seeded draw
static void values_drawn(unsigned int count, unsigned int seed)
{
    unsigned int state = seed;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        state = (state * 1664525u) + 1013904223u;
        s_values[at] = state >> 8u;
    }
}

// whether `count` values are in rising order
static int values_rise(unsigned int count)
{
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        if (s_values[at - 1u] > s_values[at])
        {
            return 0;
        }
    }
    return 1;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_no_records_and_one_record_are(void)
{
    s_values[0] = 7u;
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 0ULL, .size = sizeof(s_values[0]), .order = order_up);
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 1ULL, .size = sizeof(s_values[0]), .order = order_up);
    TEST_ASSERT_EQUAL_UINT(7u, s_values[0]);
}

void test_two_records_swap(void)
{
    s_values[0] = 9u;
    s_values[1] = 3u;
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 2ULL, .size = sizeof(s_values[0]), .order = order_up);
    TEST_ASSERT_EQUAL_UINT(3u, s_values[0]);
    TEST_ASSERT_EQUAL_UINT(9u, s_values[1]);
}

void test_a_short_run_is_put_in_order(void)
{
    values_drawn(15u, 11u);
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 15ULL, .size = sizeof(s_values[0]), .order = order_up);
    TEST_ASSERT_TRUE(values_rise(15u));
}

void test_a_long_run_is_put_in_order_and_retains_every_record(void)
{
    values_drawn(SORT_MANY, 29u);
    unsigned long long before = 0ULL;
    for (unsigned int at = 0u; at < SORT_MANY; at += 1u)
    {
        before += s_values[at];
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = SORT_MANY, .size = sizeof(s_values[0]),
               .order = order_up);
    TEST_ASSERT_TRUE(values_rise(SORT_MANY));
    unsigned long long after = 0ULL;
    for (unsigned int at = 0u; at < SORT_MANY; at += 1u)
    {
        after += s_values[at];
    }
    TEST_ASSERT_EQUAL_UINT64(before, after);
}

void test_a_run_already_in_order_and_one_reversed_are_put_in_order(void)
{
    for (unsigned int at = 0u; at < 1000u; at += 1u)
    {
        s_values[at] = at;
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 1000ULL, .size = sizeof(s_values[0]),
               .order = order_up);
    TEST_ASSERT_TRUE(values_rise(1000u));
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 1000ULL, .size = sizeof(s_values[0]),
               .order = order_down);
    for (unsigned int at = 0u; at < 1000u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(999u - at, s_values[at]);
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 1000ULL, .size = sizeof(s_values[0]),
               .order = order_up);
    TEST_ASSERT_TRUE(values_rise(1000u));
}

void test_a_run_of_one_value_and_of_few_values_is_put_in_order(void)
{
    for (unsigned int at = 0u; at < 3000u; at += 1u)
    {
        s_values[at] = 5u;
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 3000ULL, .size = sizeof(s_values[0]),
               .order = order_up);
    TEST_ASSERT_TRUE(values_rise(3000u));
    for (unsigned int at = 0u; at < 3000u; at += 1u)
    {
        s_values[at] = at % 3u;
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = 3000ULL, .size = sizeof(s_values[0]),
               .order = order_up);
    TEST_ASSERT_TRUE(values_rise(3000u));
    TEST_ASSERT_EQUAL_UINT(0u, s_values[0]);
    TEST_ASSERT_EQUAL_UINT(2u, s_values[2999]);
}

void test_records_of_an_odd_size_move_whole(void)
{
    for (unsigned int at = 0u; at < 257u; at += 1u)
    {
        s_odd[at].key = (at * 7919u) % 257u;
        for (unsigned int byte = 0u; byte < 9u; byte += 1u)
        {
            // Explicit cast narrows a value below 256 to the byte
            s_odd[at].tail[byte] = (unsigned char)((s_odd[at].key + byte) & 0xFFu);
        }
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_odd, .count = 257ULL, .size = sizeof(s_odd[0]), .order = order_odd);
    for (unsigned int at = 0u; at < 257u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(at, s_odd[at].key);
        for (unsigned int byte = 0u; byte < 9u; byte += 1u)
        {
            TEST_ASSERT_EQUAL_UINT8((unsigned char)((at + byte) & 0xFFu), s_odd[at].tail[byte]);
        }
    }
}

void test_every_record_is_present_once_after_a_sort(void)
{
    for (unsigned int at = 0u; at < SORT_MANY; at += 1u)
    {
        s_values[at] = (at * 2654435761u) % SORT_MANY;
        s_seen[at] = 0u;
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_values, .count = SORT_MANY, .size = sizeof(s_values[0]),
               .order = order_up);
    for (unsigned int at = 0u; at < SORT_MANY; at += 1u)
    {
        TEST_ASSERT_TRUE(s_values[at] < SORT_MANY);
        s_seen[s_values[at]] += 1u;
    }
    unsigned int max = 0u;
    for (unsigned int at = 0u; at < SORT_MANY; at += 1u)
    {
        max = (s_seen[at] > max) ? s_seen[at] : max;
    }
    TEST_ASSERT_TRUE(values_rise(SORT_MANY));
    TEST_ASSERT_TRUE(max >= 1u);
}
