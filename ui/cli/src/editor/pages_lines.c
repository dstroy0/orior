// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_lines.c: the lines a reading of a paged text gives, in one cell sized to the lines requested and the bytes
// read
#include "../command_line/pool.h"
#include "pages_internal.h"

typedef struct
{
    unsigned long long lines_max;
    unsigned long long required;
    unsigned long long max;
    CliPageLines *lines;
    unsigned char *cell;
    unsigned long long text_max;
} PagesLinesCtx;

// the bytes the starts and the lengths of `lines_max` lines take ahead of the text; and `required` bounded by `max`
EMBED_INLINE unsigned long long lines_heads(const PagesLinesCtx *args)
{
    return 2ULL * args->lines_max * sizeof(unsigned long long);
}

EMBED_INLINE unsigned long long lines_bound(const PagesLinesCtx *args)
{
    return (args->required < args->max) ? args->required : args->max;
}

// `lines` positioned over `cell`, sized for `lines_max` lines' starts and lengths and `text_max` bytes of text
EMBED_INLINE void lines_layout(const PagesLinesCtx *args)
{
    unsigned char *cell = args->cell;
    unsigned long long lines_max = args->lines_max;
    unsigned long long text_max = args->text_max;
    args->lines->cell = cell;
    args->lines->start = (unsigned long long *)cell;
    args->lines->length = args->lines->start + lines_max;
    args->lines->lines_max = lines_max;
    args->lines->text = (char *)(cell + EMBED_CALL(lines_heads, PagesLinesCtx, .lines_max = lines_max));
    args->lines->text_max = text_max;
}

int cli_page_lines_reserve(const CliPagesLinesLinesFirstCtx *args)
{
    const unsigned long long lines_max =
        (args->lines_first < 1ULL)
            ? 1ULL
            : EMBED_CALL(lines_bound, PagesLinesCtx, .required = args->lines_first, .max = CLI_PAGE_READ_LINES);
    const unsigned long long text_max =
        (args->text_first < 1ULL)
            ? 1ULL
            : EMBED_CALL(lines_bound, PagesLinesCtx, .required = args->text_first, .max = CLI_PAGE_READ_TEXT);
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(
        EMBED_CALL(lines_heads, PagesLinesCtx, .lines_max = lines_max) + text_max, g_cli_editor_owner);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "the lines a reading gives could not be taken");
        return 0;
    }
    EMBED_CALL(lines_layout, PagesLinesCtx, .lines = args->lines, .cell = cell, .lines_max = lines_max,
               .text_max = text_max);
    cli_pages_lines_lines_first_internal.page_lines_clear(args);
    return 1;
}

void cli_page_lines_clear(const CliPagesLinesLinesFirstCtx *args)
{
    args->lines->count = 0ULL;
    args->lines->used = 0ULL;
    args->lines->full = 0;
}

int cli_page_lines_capacity(const CliPagesLinesLinesMoreCtx *args)
{
    const int fits = ((args->lines->count + args->lines_more) <= args->lines->lines_max) &&
                     ((args->lines->used + args->text_more) <= args->lines->text_max);
    args->lines->full = args->lines->full || !fits;
    return fits;
}

void cli_page_lines_append(const CliPagesLinesBytesCtx *args)
{
    const char *bytes = args->bytes;
    if (!EMBED_CALL(cli_pages_lines_lines_more_internal.page_lines_capacity, CliPagesLinesLinesMoreCtx,
                    .lines = args->lines, .lines_more = 1ULL, .text_more = args->size))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->lines->text + args->lines->used, .src = bytes, .bytes = args->size});
    args->lines->start[args->lines->count] = args->lines->used;
    args->lines->length[args->lines->count] = args->size;
    args->lines->used += args->size;
    args->lines->count += 1ULL;
}

int cli_page_block_read(const CliPagesLinesCtx *args)
{
    unsigned long long *block_max = args->block_max;
    unsigned long long *to = args->to;
    for (;;)
    {
        *to = ((args->from + *block_max) < args->total) ? (args->from + *block_max) : args->total;
        if (!EMBED_CALL(cli_pages_lines_err_msg_internal.page_lines_get, CliPagesLinesErrMsgCtx, .one = args->one,
                        .from = args->from, .to_value = *to, .lines = args->lines, .err_msg = args->err_msg))
        {
            return 0;
        }
        if (!args->lines->full)
        {
            return 1;
        }
        if (*block_max == 1ULL)
        {
            mmgr_line_put(args->err_msg, "line ");
            mmgr_line_decimal(args->err_msg, args->from + 1ULL, 1u);
            mmgr_line_put(args->err_msg, " is longer than a reading of the text contains");
            return 0;
        }
        *block_max /= 2ULL;
    }
}

void cli_page_lines_free(const CliPagesLinesLinesFirstCtx *args)
{
    g_cli_guard->persistent_buf_release(args->lines->cell, g_cli_editor_owner);
    args->lines->cell = NULL;
    args->lines->start = NULL;
    args->lines->length = NULL;
    args->lines->text = NULL;
    args->lines->lines_max = 0ULL;
    args->lines->text_max = 0ULL;
    cli_pages_lines_lines_first_internal.page_lines_clear(args);
}
