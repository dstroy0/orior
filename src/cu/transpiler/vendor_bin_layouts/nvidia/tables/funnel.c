// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// funnel.c: the vendor's tables read off a capture of its disassembler: the records, the instruction set unpacked
// from its LZ4 archive, and the latencies gathered from the copies out of their buffer
#include "funnel.h"
#include "../../../../includes/parsers/line_read.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most an archive unpacks to: a table is a few million bytes, and a header past this is no header
#define TABLES_MOST (256u * 1024u * 1024u)

int tables_text_added(TablesText *text, const void *bytes, size_t count)
{
    if (text->length + count > text->room)
    {
        size_t room = (text->room == 0u) ? 65536u : text->room;
        while (room < text->length + count)
        {
            room *= 2u;
        }
        unsigned char *const grown = (unsigned char *)realloc(text->bytes, room);
        if (grown == NULL)
        {
            return 0;
        }
        text->bytes = grown;
        text->room = room;
    }
    memcpy(text->bytes + text->length, bytes, count);
    text->length += count;
    return 1;
}

void tables_text_freed(TablesText *text)
{
    free(text->bytes);
    text->bytes = NULL;
    text->length = 0u;
    text->room = 0u;
}

int tables_record_read(const unsigned char *bytes, size_t size, size_t *at, TablesRecord *record)
{
    // the line is short: two addresses and a count, past which nothing of it stands
    char line[96];
    size_t place = *at;
    if ((place + 2u > size) || (bytes[place] != '\n') || (bytes[place + 1u] != '<'))
    {
        return 0;
    }
    size_t length = 0u;
    while ((place + 1u + length < size) && (length + 1u < sizeof(line)) && (bytes[place + 1u + length] != '\n'))
    {
        line[length] = (char)bytes[place + 1u + length];
        length += 1u;
    }
    line[length] = '\0';
    unsigned long long to = 0ull;
    unsigned long long from = 0ull;
    unsigned long long count = 0ull;
    const char *read = line + 1;
    if ((line[0] != '<') || !line_number(&read, 1, &to) || !line_number(&read, 1, &from) ||
        !line_number(&read, 0, &count) || (*read != '>'))
    {
        return 0;
    }
    place += 1u + length + 1u;
    if (count > (unsigned long long)(size - place))
    {
        return 0;
    }
    record->to = to;
    record->from = from;
    record->bytes = bytes + place;
    // the count is within the capture, which a size_t spans
    record->size = (size_t)count;
    *at = place + record->size;
    return 1;
}

static unsigned long long tables_word(const unsigned char *bytes)
{
    return (unsigned long long)bytes[0] | ((unsigned long long)bytes[1] << 8u) | ((unsigned long long)bytes[2] << 16u) |
           ((unsigned long long)bytes[3] << 24u);
}

int tables_archive(const unsigned char *bytes, size_t size)
{
    if (size < 16u)
    {
        return 0;
    }
    const unsigned long long own = tables_word(bytes);
    const unsigned long long zero = tables_word(bytes + 4u);
    const unsigned long long unpacked = tables_word(bytes + 8u);
    const unsigned long long packed = tables_word(bytes + 12u);
    return (own == (unsigned long long)size) && (zero == 0ull) && (unpacked == TABLES_BLOCK) && (packed != 0ull) &&
           (packed <= (unsigned long long)(size - 16u));
}

// a length of an LZ4 sequence past its nibble: each byte added, until one below 255
static int tables_length_read(const unsigned char *bytes, size_t size, size_t *at, size_t *length)
{
    for (;;)
    {
        if (*at >= size)
        {
            return 0;
        }
        const unsigned char more = bytes[*at];
        *at += 1u;
        *length += more;
        if (more != 255u)
        {
            return 1;
        }
    }
}

// one LZ4 block `bytes`, `size` long, unpacked onto the end of `text`, its matches reaching back into all of the
// text: the count unpacked, or 0 where the block is malformed or there was no memory
static size_t tables_block_unpacked(const unsigned char *bytes, size_t size, TablesText *text)
{
    const size_t before = text->length;
    size_t at = 0u;
    while (at < size)
    {
        const unsigned char token = bytes[at];
        at += 1u;
        size_t literals = (size_t)(token >> 4u);
        if ((literals == 15u) && !tables_length_read(bytes, size, &at, &literals))
        {
            return 0u;
        }
        if ((literals > size - at) || !tables_text_added(text, bytes + at, literals))
        {
            return 0u;
        }
        at += literals;
        if (at >= size)
        {
            break;
        }
        if (at + 2u > size)
        {
            return 0u;
        }
        const size_t offset = (size_t)bytes[at] | ((size_t)bytes[at + 1u] << 8u);
        at += 2u;
        size_t match = (size_t)(token & 15u) + 4u;
        if (((token & 15u) == 15u) && !tables_length_read(bytes, size, &at, &match))
        {
            return 0u;
        }
        if ((offset == 0u) || (offset > text->length))
        {
            return 0u;
        }
        // a match reaching past the text's end repeats what it copies: one byte at a time where it overlaps
        for (size_t copied = 0u; copied < match; copied += 1u)
        {
            const unsigned char byte = text->bytes[text->length - offset];
            if (!tables_text_added(text, &byte, 1u))
            {
                return 0u;
            }
        }
    }
    return text->length - before;
}

int tables_unpacked(const unsigned char *bytes, size_t size, TablesText *text)
{
    if (!tables_archive(bytes, size))
    {
        return 0;
    }
    size_t at = 8u;
    while (at + 8u <= size)
    {
        const unsigned long long unpacked = tables_word(bytes + at);
        const unsigned long long packed = tables_word(bytes + at + 4u);
        at += 8u;
        if ((packed == 0ull) || (packed > (unsigned long long)(size - at)) || (unpacked > TABLES_MOST))
        {
            return 0;
        }
        // the counts are within the archive, which a size_t spans
        if (tables_block_unpacked(bytes + at, (size_t)packed, text) != (size_t)unpacked)
        {
            return 0;
        }
        at += (size_t)packed;
    }
    return 1;
}

static const char s_latencies_head[] = "OPERATION SETS";

unsigned int tables_funnelled(const unsigned char *bytes, size_t size, TablesText *instructions, TablesText *latencies)
{
    unsigned int found = 0u;
    int archive_read = 0;
    unsigned long long buffer = 0ull;
    int buffer_found = 0;
    size_t at = 0u;
    TablesRecord record;
    while (tables_record_read(bytes, size, &at, &record))
    {
        if (!archive_read && tables_archive(record.bytes, record.size))
        {
            archive_read = 1;
            found += tables_unpacked(record.bytes, record.size, instructions) ? 1u : 0u;
        }
        const int copy = (record.from != record.to);
        // the buffer's first copy begins with the head, past the line ends and spaces before it
        size_t lead = 0u;
        while ((lead < record.size) && ((record.bytes[lead] == '\n') || (record.bytes[lead] == ' ')))
        {
            lead += 1u;
        }
        if (!buffer_found && copy && (record.size - lead >= sizeof(s_latencies_head) - 1u) &&
            (memcmp(record.bytes + lead, s_latencies_head, sizeof(s_latencies_head) - 1u) == 0))
        {
            buffer_found = 1;
            buffer = record.from;
            found += 1u;
        }
        if (buffer_found && copy && (record.from == buffer))
        {
            if (!tables_text_added(latencies, record.bytes, record.size))
            {
                return found;
            }
        }
    }
    return found;
}
