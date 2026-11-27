// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// table_text.c: a text numbered by its hash, its row appended to the table of texts once a process
#include "table_text.h"
#include "memoria_operor/memoria_operor.h"

#include "../../../../../includes/file_defs/qry/writer/qry_member.h"

// The numbers this process appended, TABLE_TEXT_NUMBERS at max: an order of them sorted, `s_table_text_ordered` of
// them, and a run of the newest, `s_table_text_run_count` of them, with the room that the sort of a full run writes
#define TABLE_TEXT_NUMBERS (1u << 16u)
#define TABLE_TEXT_RUN 256u

static unsigned long long s_table_text_order[TABLE_TEXT_NUMBERS];
static unsigned int s_table_text_ordered;
static unsigned long long s_table_text_run[TABLE_TEXT_RUN];
static unsigned int s_table_text_run_count;
static unsigned long long s_table_text_room[TABLE_TEXT_RUN];
static unsigned long long s_table_text_row[QRY_WRITER_RING_BYTES / 16ULL];

// the order of two numbers: below 0, 0 or above 0
static int table_text_order(const void *left, const void *right)
{
    const unsigned long long one = *(const unsigned long long *)left;
    const unsigned long long other = *(const unsigned long long *)right;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

// 1 where this process appended the text of the number `number`: the run first, then the order by a binary search
static int table_text_appended(unsigned long long number)
{
    for (unsigned int each = 0u; each < s_table_text_run_count; each += 1u)
    {
        if (s_table_text_run[each] == number)
        {
            return 1;
        }
    }
    unsigned int low = 0u;
    unsigned int high = s_table_text_ordered;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_table_text_order[middle] < number)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return (low < s_table_text_ordered) && (s_table_text_order[low] == number);
}

// the number `number` noted: into the run, the run sorted once and merged into the order from its end first where it
// is full
static void table_text_noted(unsigned long long number)
{
    if (s_table_text_run_count == TABLE_TEXT_RUN)
    {
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = s_table_text_run, .bytes = sizeof(unsigned long long),
                         .scratch = s_table_text_room, .count = s_table_text_run_count, .order = table_text_order);
        unsigned int from_order = s_table_text_ordered;
        unsigned int from_run = s_table_text_run_count;
        unsigned int to = s_table_text_ordered + s_table_text_run_count;
        while (from_run != 0u)
        {
            const int order_first =
                (from_order != 0u) && (s_table_text_order[from_order - 1u] > s_table_text_run[from_run - 1u]);
            to -= 1u;
            if (order_first)
            {
                from_order -= 1u;
                s_table_text_order[to] = s_table_text_order[from_order];
            }
            else
            {
                from_run -= 1u;
                s_table_text_order[to] = s_table_text_run[from_run];
            }
        }
        s_table_text_ordered += s_table_text_run_count;
        s_table_text_run_count = 0u;
    }
    s_table_text_run[s_table_text_run_count] = number;
    s_table_text_run_count += 1u;
}

unsigned long long table_text(const void *bytes, unsigned long long length)
{
    if (length == 0ULL)
    {
        return 0ULL;
    }
    const unsigned char *const text = (const unsigned char *)bytes;
    unsigned long long number = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        number = (number ^ text[at]) * 0x100000001b3ULL;
    }
    number = (number == 0ULL) ? 1ULL : number;
    // the row's head, its two words of no who, its number and its length, then its bytes filled to a whole word
    const unsigned long long words = 4ULL + ((length + 7ULL) / 8ULL);
    if (words > (sizeof(s_table_text_row) / sizeof(s_table_text_row[0])))
    {
        return number;
    }
    if (((s_table_text_ordered + s_table_text_run_count) < TABLE_TEXT_NUMBERS) && !table_text_appended(number))
    {
        s_table_text_row[0] = 0ULL;
        s_table_text_row[1] = 0ULL;
        s_table_text_row[2] = number;
        s_table_text_row[3] = length;
        s_table_text_row[words - 1ULL] = 0ULL;
        // the row fits half a ring, whose bytes a size_t counts
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &s_table_text_row[4], .src = text, .bytes = (size_t)length);
        if (qry_member_append(TABLE_TEXTS, s_table_text_row, words * 8ULL, QRY_ORDINARY))
        {
            table_text_noted(number);
        }
    }
    return number;
}
