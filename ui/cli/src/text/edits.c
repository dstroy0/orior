// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// edits.c: edits written as the page reads them, and written over a text: each place found as a byte by one walk of
// the text's lines, its places taken in the order of their lines, and the spans then written in the order of their
// bytes, those of one span in the order they were given
#include "edits.h"
#include "../command_line/cells.h"
#include "json_tree.h"

#include <stdlib.h>

// the places of the edits being written, each edit's from and then its to; their bytes; the order they are taken in,
// one cell of twice the edits' count; and the lock they are held under, which the sorts read them through
static CliTextPlace *s_place;
static unsigned long long *s_byte;
static unsigned int *s_order;
static CliLock s_edits_lock = CLI_LOCK_INIT;

void cli_text_place_written(CliTextPlace place, CliLine *out)
{
    cli_line_text(out, "{\"line\":");
    cli_line_decimal(out, place.line, 1u);
    cli_line_text(out, ",\"col\":");
    cli_line_decimal(out, place.col, 1u);
    cli_line_character(out, '}');
}

void cli_text_edit_written(const CliTextEdit *edit, CliLine *out)
{
    cli_line_text(out, "{\"from\":");
    cli_text_place_written(edit->from, out);
    cli_line_text(out, ",\"to\":");
    cli_text_place_written(edit->to, out);
    cli_line_text(out, ",\"text\":");
    cli_json_quoted(out, edit->text, edit->size);
    cli_line_character(out, '}');
}

unsigned long long cli_text_byte_of(const char *line, unsigned long long size, unsigned int col)
{
    unsigned long long units = 0ull;
    for (unsigned long long at = 0ull; at < size;)
    {
        if (units >= col)
        {
            return at;
        }
        const unsigned char lead = (unsigned char)line[at];
        const unsigned long long letter = (lead < 0xc0u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
        units += (letter == 4u) ? 2u : 1u;
        at += letter;
    }
    return size;
}

// two places in the order of their lines
static int edits_by_line(const void *one, const void *other)
{
    const unsigned int left = s_place[*(const unsigned int *)one].line;
    const unsigned int right = s_place[*(const unsigned int *)other].line;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

// two edits in the order of their spans' bytes, then of their places among the edits
static int edits_by_span(const void *one, const void *other)
{
    const unsigned int left = *(const unsigned int *)one;
    const unsigned int right = *(const unsigned int *)other;
    const unsigned long long left_from = s_byte[2u * left];
    const unsigned long long right_from = s_byte[2u * right];
    if (left_from != right_from)
    {
        return (left_from < right_from) ? -1 : 1;
    }
    const unsigned long long left_to = s_byte[(2u * left) + 1u];
    const unsigned long long right_to = s_byte[(2u * right) + 1u];
    if (left_to != right_to)
    {
        return (left_to < right_to) ? -1 : 1;
    }
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

// each place's byte in `size` bytes of `text`, `count` places
static void edits_bytes(const char *text, unsigned long long size, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_order[at] = at;
    }
    qsort(s_order, count, sizeof(s_order[0]), edits_by_line);
    unsigned int line = 0u;
    unsigned long long start = 0ull;
    int beyond = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliTextPlace place = s_place[s_order[at]];
        while (!beyond && (line < place.line))
        {
            const char *const newline =
                (start < size)
                    ? memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = (size_t)(size - start)})
                    : NULL;
            beyond = (newline == NULL);
            start = beyond ? size : (unsigned long long)(newline - text) + 1u;
            line += 1u;
        }
        if (beyond)
        {
            s_byte[s_order[at]] = size;
            continue;
        }
        const char *const newline =
            (start < size) ? memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = (size_t)(size - start)})
                           : NULL;
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        if ((end > start) && (text[end - 1u] == '\r'))
        {
            end -= 1u;
        }
        s_byte[s_order[at]] = start + cli_text_byte_of(text + start, end - start, place.col);
    }
}

int cli_text_applied(const char *text, unsigned long long size, const CliTextEdit *edits, unsigned int count,
                     CliLine *out)
{
    if (count > CLI_TEXT_EDITS)
    {
        return 0;
    }
    // the bytes first on the cell's boundary, then the places, then the order
    const unsigned long long places = 2ull * count;
    char *const cell = cli_cell_taken((places + 1ull) * (sizeof(*s_byte) + sizeof(*s_place) + sizeof(*s_order)),
                                      "the places of the edits written");
    if (cell == NULL)
    {
        return 0;
    }
    cli_lock_take(&s_edits_lock);
    s_byte = (unsigned long long *)(void *)cell;
    s_place = (CliTextPlace *)(void *)(s_byte + places + 1ull);
    s_order = (unsigned int *)(void *)(s_place + places + 1ull);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_place[2u * at] = edits[at].from;
        s_place[(2u * at) + 1u] = edits[at].to;
    }
    edits_bytes(text, size, 2u * count);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_order[at] = at;
    }
    qsort(s_order, count, sizeof(s_order[0]), edits_by_span);
    unsigned long long held = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int edit = s_order[at];
        const unsigned long long from = (s_byte[2u * edit] > held) ? s_byte[2u * edit] : held;
        const unsigned long long to = s_byte[(2u * edit) + 1u];
        cli_line_counted(out, text + held, from - held);
        cli_line_counted(out, edits[edit].text, edits[edit].size);
        held = (to > from) ? to : from;
    }
    cli_line_counted(out, text + held, size - held);
    s_byte = NULL;
    s_place = NULL;
    s_order = NULL;
    cli_lock_give(&s_edits_lock);
    cli_cell_given(cell);
    return 1;
}
