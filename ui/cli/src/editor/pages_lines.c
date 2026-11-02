// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_lines.c: the lines a reading of a paged text gives, in one cell sized to the lines requested and the bytes
// read
#include "../command_line/cells.h"
#include "pages_internal.h"

typedef struct
{
    unsigned long long lines_most;
    unsigned long long wanted;
    unsigned long long most;
    CliPageLines *lines;
    unsigned char *cell;
    unsigned long long text_most;
} PagesLinesCtx;

// the bytes the starts and the lengths of `lines_most` lines take ahead of the text; and `wanted` bounded by `most`
EMBED_INLINE unsigned long long lines_heads(const PagesLinesCtx *args)
{
    return 2ull * args->lines_most * sizeof(unsigned long long);
}

EMBED_INLINE unsigned long long lines_bound(const PagesLinesCtx *args)
{
    return (args->wanted < args->most) ? args->wanted : args->most;
}

// `lines` laid over `cell`, sized for `lines_most` lines' starts and lengths and `text_most` bytes of text
EMBED_INLINE void lines_layout(const PagesLinesCtx *args)
{
    unsigned char *cell = args->cell;
    unsigned long long lines_most = args->lines_most;
    unsigned long long text_most = args->text_most;
    args->lines->cell = cell;
    args->lines->start = (unsigned long long *)cell;
    args->lines->length = args->lines->start + lines_most;
    args->lines->lines_most = lines_most;
    args->lines->text = (char *)(cell + EMBED_CALL(lines_heads, PagesLinesCtx, .lines_most = lines_most));
    args->lines->text_most = text_most;
}

int cli_page_lines_reserve(const CliPagesLinesLinesFirstCtx *args)
{
    const unsigned long long lines_most =
        (args->lines_first < 1ull)
            ? 1ull
            : EMBED_CALL(lines_bound, PagesLinesCtx, .wanted = args->lines_first, .most = CLI_PAGE_READ_LINES);
    const unsigned long long text_most =
        (args->text_first < 1ull)
            ? 1ull
            : EMBED_CALL(lines_bound, PagesLinesCtx, .wanted = args->text_first, .most = CLI_PAGE_READ_TEXT);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        EMBED_CALL(lines_heads, PagesLinesCtx, .lines_most = lines_most) + text_most, &g_cli_editor_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "the lines a reading gives could not be taken");
        return 0;
    }
    EMBED_CALL(lines_layout, PagesLinesCtx, .lines = args->lines, .cell = cell, .lines_most = lines_most,
               .text_most = text_most);
    cli_pages_lines_lines_first_internal.page_lines_clear(args);
    return 1;
}

void cli_page_lines_clear(const CliPagesLinesLinesFirstCtx *args)
{
    args->lines->count = 0ull;
    args->lines->used = 0ull;
    args->lines->full = 0;
}

int cli_page_lines_room(const CliPagesLinesLinesMoreCtx *args)
{
    const int fits = ((args->lines->count + args->lines_more) <= args->lines->lines_most) &&
                     ((args->lines->used + args->text_more) <= args->lines->text_most);
    args->lines->full = args->lines->full || !fits;
    return fits;
}

void cli_page_lines_put(const CliPagesLinesBytesCtx *args)
{
    const char *bytes = args->bytes;
    if (!EMBED_CALL(cli_pages_lines_lines_more_internal.page_lines_room, CliPagesLinesLinesMoreCtx,
                    .lines = args->lines, .lines_more = 1ull, .text_more = args->size))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->lines->text + args->lines->used, .src = bytes, .bytes = args->size});
    args->lines->start[args->lines->count] = args->lines->used;
    args->lines->length[args->lines->count] = args->size;
    args->lines->used += args->size;
    args->lines->count += 1ull;
}

int cli_page_block_read(const CliPagesLinesCtx *args)
{
    unsigned long long *block_most = args->block_most;
    unsigned long long *to = args->to;
    for (;;)
    {
        *to = ((args->from + *block_most) < args->total) ? (args->from + *block_most) : args->total;
        if (!EMBED_CALL(cli_pages_lines_err_msg_internal.page_lines_get, CliPagesLinesErrMsgCtx, .one = args->one,
                        .from = args->from, .to_value = *to, .lines = args->lines, .err_msg = args->err_msg))
        {
            return 0;
        }
        if (!args->lines->full)
        {
            return 1;
        }
        if (*block_most == 1ull)
        {
            mmgr_line_put(args->err_msg, "line ");
            mmgr_line_decimal(args->err_msg, args->from + 1ull, 1u);
            mmgr_line_put(args->err_msg, " is longer than a reading of the text contains");
            return 0;
        }
        *block_most /= 2ull;
    }
}

void cli_page_lines_free(const CliPagesLinesLinesFirstCtx *args)
{
    g_cli_cells->persistent_buf_release(args->lines->cell, &g_cli_editor_cells);
    args->lines->cell = NULL;
    args->lines->start = NULL;
    args->lines->length = NULL;
    args->lines->text = NULL;
    args->lines->lines_most = 0ull;
    args->lines->text_most = 0ull;
    cli_pages_lines_lines_first_internal.page_lines_clear(args);
}
