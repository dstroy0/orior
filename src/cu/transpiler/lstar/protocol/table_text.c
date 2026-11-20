// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// table_text.c: a text numbered by its hash, its row appended to the table of texts once a process
#include "table_text.h"

#include "../../../../../includes/file_defs/qry/writer/qry_member.h"

#include <string.h>

// the numbers this process appended, an open table twice the texts a run of it numbers
#define TABLE_TEXT_SLOTS (1u << 16u)

static unsigned long long s_table_text_numbers[TABLE_TEXT_SLOTS];
static unsigned long long s_table_text_row[QRY_WRITER_RING_BYTES / 16ULL];

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
    unsigned int slot = (unsigned int)(number & (TABLE_TEXT_SLOTS - 1u));
    unsigned int tried = 0u;
    while ((s_table_text_numbers[slot] != 0ULL) && (s_table_text_numbers[slot] != number) && (tried < TABLE_TEXT_SLOTS))
    {
        slot = (slot + 1u) & (TABLE_TEXT_SLOTS - 1u);
        tried += 1u;
    }
    if ((tried < TABLE_TEXT_SLOTS) && (s_table_text_numbers[slot] == 0ULL))
    {
        s_table_text_row[0] = 0ULL;
        s_table_text_row[1] = 0ULL;
        s_table_text_row[2] = number;
        s_table_text_row[3] = length;
        s_table_text_row[words - 1ULL] = 0ULL;
        // the row fits half a ring, whose bytes a size_t counts
        memcpy(&s_table_text_row[4], text, (size_t)length);
        if (qry_member_append(TABLE_TEXTS, s_table_text_row, words * 8ULL, QRY_ORDINARY))
        {
            s_table_text_numbers[slot] = number;
        }
    }
    return number;
}
