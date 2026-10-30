// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_lines.c: the lines a reading of a paged text gives, in one cell sized to the lines requested and the bytes
// read, moved into a cell of twice the room as either fills
#include "../command_line/cells.h"
#include "pages_internal.h"

// the bytes the starts and the lengths of `lines_most` lines take ahead of the text; and `wanted` bounded by `most`
static unsigned long long lines_heads(unsigned long long lines_most)
{
    return 2ull * lines_most * sizeof(unsigned long long);
}

static unsigned long long lines_bounded(unsigned long long wanted, unsigned long long most)
{
    return (wanted < most) ? wanted : most;
}

// `lines` laid over `cell`, sized for `lines_most` lines' starts and lengths and `text_most` bytes of text
static void lines_laid(CliPageLines *lines, unsigned char *cell, unsigned long long lines_most,
                       unsigned long long text_most)
{
    lines->cell = cell;
    lines->start = (unsigned long long *)cell;
    lines->length = lines->start + lines_most;
    lines->lines_most = lines_most;
    lines->text = (char *)(cell + lines_heads(lines_most));
    lines->text_most = text_most;
}

int cli_page_lines_reserved(CliPageLines *lines, unsigned long long lines_first, unsigned long long text_first,
                            VerbaLinea *err_msg)
{
    const unsigned long long lines_most = (lines_first < 1ull) ? 1ull : lines_bounded(lines_first, CLI_PAGE_READ_LINES);
    const unsigned long long text_most = (text_first < 1ull) ? 1ull : lines_bounded(text_first, CLI_PAGE_READ_TEXT);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(lines_heads(lines_most) + text_most, &g_cli_editor_cells);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "the lines a reading gives could not be taken");
        return 0;
    }
    lines_laid(lines, cell, lines_most, text_most);
    cli_page_lines_emptied(lines);
    return 1;
}

void cli_page_lines_emptied(CliPageLines *lines)
{
    lines->count = 0ull;
    lines->used = 0ull;
    lines->full = 0;
}

int cli_page_lines_room(CliPageLines *lines, unsigned long long lines_more, unsigned long long text_more)
{
    const unsigned long long lines_wanted = lines->count + lines_more;
    const unsigned long long text_wanted = lines->used + text_more;
    if ((lines_wanted <= lines->lines_most) && (text_wanted <= lines->text_most))
    {
        return 1;
    }
    if ((lines_wanted > CLI_PAGE_READ_LINES) || (text_wanted > CLI_PAGE_READ_TEXT))
    {
        lines->full = 1;
        return 0;
    }
    const unsigned long long lines_twice = 2ull * lines->lines_most;
    const unsigned long long text_twice = 2ull * lines->text_most;
    const unsigned long long lines_most =
        lines_bounded((lines_twice > lines_wanted) ? lines_twice : lines_wanted, CLI_PAGE_READ_LINES);
    const unsigned long long text_most =
        lines_bounded((text_twice > text_wanted) ? text_twice : text_wanted, CLI_PAGE_READ_TEXT);
    // the cell grown where it stands while the starts and lengths keep their places ahead of the text, and the text
    // alone gains room; a cell with more lines ahead is laid anew in a larger one
    unsigned char *const grown =
        (lines_most == lines->lines_most)
            ? g_cli_cells->buf_grow(lines->cell, lines_heads(lines_most) + text_most, &g_cli_editor_cells)
            : NULL;
    if (grown != NULL)
    {
        lines_laid(lines, grown, lines_most, text_most);
        return 1;
    }
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(lines_heads(lines_most) + text_most, &g_cli_editor_cells);
    if (cell == NULL)
    {
        lines->full = 1;
        return 0;
    }
    CliPageLines moved;
    lines_laid(&moved, cell, lines_most, text_most);
    memor.cpy(&(MemoriaCfg){.dst = moved.start, .src = lines->start, .bytes = lines->count * sizeof(*moved.start)});
    memor.cpy(&(MemoriaCfg){.dst = moved.length, .src = lines->length, .bytes = lines->count * sizeof(*moved.length)});
    memor.cpy(&(MemoriaCfg){.dst = moved.text, .src = lines->text, .bytes = lines->used});
    g_cli_cells->persistent_buf_release(lines->cell, &g_cli_editor_cells);
    lines_laid(lines, cell, lines_most, text_most);
    return 1;
}

void cli_page_lines_put(CliPageLines *lines, const char *bytes, unsigned long long size)
{
    if (!cli_page_lines_room(lines, 1ull, size))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = lines->text + lines->used, .src = bytes, .bytes = size});
    lines->start[lines->count] = lines->used;
    lines->length[lines->count] = size;
    lines->used += size;
    lines->count += 1ull;
}

int cli_page_block_read(CliPaged *one, unsigned long long from, unsigned long long total,
                        unsigned long long *block_most, unsigned long long *to, CliPageLines *lines,
                        VerbaLinea *err_msg)
{
    for (;;)
    {
        *to = ((from + *block_most) < total) ? (from + *block_most) : total;
        if (!cli_page_lines_of(one, from, *to, lines, err_msg))
        {
            return 0;
        }
        if (!lines->full)
        {
            return 1;
        }
        if (*block_most == 1ull)
        {
            mmgr_line_put(err_msg, "line ");
            mmgr_line_decimal(err_msg, from + 1ull, 1u);
            mmgr_line_put(err_msg, " is longer than a reading of the text contains");
            return 0;
        }
        *block_most /= 2ull;
    }
}

void cli_page_lines_given(CliPageLines *lines)
{
    g_cli_cells->persistent_buf_release(lines->cell, &g_cli_editor_cells);
    lines->cell = NULL;
    lines->start = NULL;
    lines->length = NULL;
    lines->text = NULL;
    lines->lines_most = 0ull;
    lines->text_most = 0ull;
    cli_page_lines_emptied(lines);
}
