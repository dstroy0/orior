// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_docstring.c: the paragraph of a Python docstring: the triple-quoted strings found from the text's first line,
// the first that holds the cursor's line taken, and its paragraph the lines around the cursor's that are neither blank
// nor its quotes alone. The closing quotes stay inside the margin: the last word goes down a line where they would not
#include "fill_internal.h"

// A triple-quoted string: its quote and its first and last lines
typedef struct
{
    const char *quote;
    unsigned int start;
    unsigned int end;
} CliFillString;

// the first string from the text's first line that holds the line `at`: 1, or 0 where none does
static int docstring_around(const char *text, unsigned long long size, unsigned int at, CliFillString *found)
{
    CliFillLine line = {0ull, 0ull, 0u};
    (void)cli_fill_line_at(text, size, 0u, &line);
    const char *open = NULL;
    unsigned int open_start = 0u;
    for (;;)
    {
        if ((line.index > at) && (open == NULL))
        {
            return 0;
        }
        unsigned long long rest = line.start;
        for (;;)
        {
            unsigned long long place = 0ull;
            if (open != NULL)
            {
                if (!cli_text_find(text + rest, line.end - rest, open, 3u, &place))
                {
                    break;
                }
                if ((open_start <= at) && (at <= line.index))
                {
                    found->quote = open;
                    found->start = open_start;
                    found->end = line.index;
                    return 1;
                }
                open = NULL;
                rest += place + 3u;
                continue;
            }
            unsigned long long double_at = 0ull;
            unsigned long long single_at = 0ull;
            const int has_double = cli_text_find(text + rest, line.end - rest, "\"\"\"", 3u, &double_at);
            const int has_single = cli_text_find(text + rest, line.end - rest, "'''", 3u, &single_at);
            if (!has_double && !has_single)
            {
                break;
            }
            const int double_first = has_double && (!has_single || (double_at < single_at));
            open = double_first ? "\"\"\"" : "'''";
            open_start = line.index;
            rest += (double_first ? double_at : single_at) + 3u;
        }
        if (!cli_fill_after(text, size, &line))
        {
            return 0;
        }
    }
}

// whether a line is blank or the quotes alone
static int docstring_blank(const char *text, const CliFillLine *line, const char *quote)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(text + line->start, line->end - line->start, &from, &to);
    return (to == from) ||
           ((to - from == 3u) &&
            (memor.cmp(&(MemoriaCfg){.src = text + line->start + from, .other = quote, .bytes = 3u}) == 0));
}

// the last line written with its last word sent down a line under `lead` where the closing quotes would pass the
// margin, its words starting at `start_of_words`
static void docstring_closed(CliFilled *filled, const char *lead, unsigned long long lead_size,
                             unsigned long long start_of_words, unsigned int width)
{
    unsigned long long size = 0ull;
    const char *const last = cli_filled_last(filled, &size);
    unsigned long long space = 0ull;
    if ((cli_text_letters(last, size) + 3u <= width) || (start_of_words > size) ||
        !cli_text_find(last + start_of_words, size - start_of_words, " ", 1u, &space) ||
        !cli_text_find_last(last, size, " ", 1u, &space))
    {
        return;
    }
    CliLine *const out = filled->out;
    const unsigned long long cut = filled->line_start + space;
    const unsigned long long word = size - space - 1u;
    const unsigned long long end = cut + 1u + lead_size + word;
    if (end >= out->capacity)
    {
        out->at = out->capacity;
        return;
    }
    memor.move_up(
        &(MemoriaCfg){.dst = out->out + cut + 1u + lead_size, .src = out->out + cut + 1u, .bytes = (size_t)word});
    memor.cpy(&(MemoriaCfg){.dst = out->out + cut + 1u, .src = lead, .bytes = (size_t)lead_size});
    out->out[cut] = '\n';
    out->at = end;
    filled->line_start = cut + 1u;
    filled->lines += 1u;
}

int cli_fill_docstring(const char *text, unsigned long long size, const CliFillLine *at, unsigned int width,
                       CliFilled *filled, CliTextEdit *edit)
{
    CliFillString string = {NULL, 0u, 0u};
    if (!docstring_around(text, size, at->index, &string) || docstring_blank(text, at, string.quote))
    {
        return 0;
    }
    CliFillLine first = *at;
    CliFillLine before = first;
    while ((first.index > string.start) && cli_fill_before(text, &before) &&
           !docstring_blank(text, &before, string.quote))
    {
        first = before;
    }
    CliFillLine last = *at;
    CliFillLine after = last;
    while ((last.index < string.end) && cli_fill_after(text, size, &after) &&
           !docstring_blank(text, &after, string.quote))
    {
        last = after;
    }
    const char *const lead = text + first.start;
    const unsigned long long lead_size = cli_text_lead(lead, first.end - first.start);
    unsigned long long opening = 0ull;
    if (first.index == string.start)
    {
        if (!cli_text_find(lead + lead_size, first.end - first.start - lead_size, string.quote, 3u, &opening))
        {
            return 0;
        }
        opening += 3u;
    }
    const int closing = (last.index == string.end);
    unsigned int count = 0u;
    for (CliFillLine line = first; count < g_cli_fill_slices;)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(text + line.start, line.end - line.start, &from, &to);
        const char *body = text + line.start + from;
        unsigned long long body_size = to - from;
        if ((line.index == first.index) && (opening > 0u))
        {
            body += opening;
            body_size -= opening;
            const unsigned long long skipped = cli_text_lead(body, body_size);
            body += skipped;
            body_size -= skipped;
        }
        if ((line.index == last.index) && closing)
        {
            if ((body_size >= 3u) &&
                (memor.cmp(&(MemoriaCfg){.src = body + body_size - 3u, .other = string.quote, .bytes = 3u}) == 0))
            {
                body_size -= 3u;
            }
            body_size = cli_text_trimmed_end(body, body_size);
        }
        g_cli_fill_slice[count] = body;
        g_cli_fill_slice_size[count] = body_size;
        count += 1u;
        if ((line.index == last.index) || !cli_fill_after(text, size, &line))
        {
            break;
        }
    }
    cli_fill_words(filled, 0u, count, lead, lead_size + opening, lead, lead_size, width);
    if (closing)
    {
        const unsigned long long start_of_words = (filled->lines == 1u) ? lead_size + opening : lead_size;
        docstring_closed(filled, lead, lead_size, start_of_words, width);
        cli_line_counted(filled->out, string.quote, 3u);
    }
    cli_fill_edit_of(text, &first, &last, filled, edit);
    return 1;
}
