// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_docstring.c: the paragraph of a Python docstring: the triple-quoted strings found from the text's first line,
// the first that contains the cursor's line taken, and its paragraph the lines around the cursor's that are neither
// blank nor its quotes alone. The closing quotes stay inside the margin: the last word goes down a line where they
// would not
#include "fill_internal.h"

// A triple-quoted string: its quote and its first and last lines
typedef struct
{
    const char *quote;
    unsigned int start;
    unsigned int end;
} CliFillString;

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned int at;
    CliFillString *found;
    const CliFillLine *line;
    const char *quote;
    CliFilled *filled;
    const char *lead;
    unsigned long long comment_prefix_size;
    unsigned long long start_of_words;
    unsigned int width;
} FillDocstringCtx;
// the first string from the text's first line that contains the line `at`: 1, or 0 where none does
EMBED_INLINE int docstring_around(const FillDocstringCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int at = args->at;
    CliFillLine line = {0ULL, 0ULL, 0u};
    (void)EMBED_CALL(cli_fill_text_size_internal.line_at, CliFillTextSizeCtx, .text = text, .size = size, .index = 0u,
                     .line = &line);
    const char *open = NULL;
    unsigned int open_start = 0u;
    for (;;)
    {
        if ((line.index > at) && (open == NULL))
        {
            return 0;
        }
        unsigned long long rest = line.start;
        int going = 1;
        while (going)
        {
            unsigned long long place = 0ULL;
            if (open != NULL)
            {
                if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + rest,
                                .length_value = line.end - rest, .required = open, .size = 3u, .at_pointer = &place))
                {
                    going = 0;
                    continue;
                }
                if ((open_start <= at) && (at <= line.index))
                {
                    args->found->quote = open;
                    args->found->start = open_start;
                    args->found->end = line.index;
                    return 1;
                }
                open = NULL;
                rest += place + 3u;
                continue;
            }
            unsigned long long double_at = 0ULL;
            unsigned long long single_at = 0ULL;
            const int has_double =
                EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + rest,
                           .length_value = line.end - rest, .required = "\"\"\"", .size = 3u, .at_pointer = &double_at);
            const int has_single =
                EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + rest,
                           .length_value = line.end - rest, .required = "'''", .size = 3u, .at_pointer = &single_at);
            if (!has_double && !has_single)
            {
                going = 0;
                continue;
            }
            const int double_first = has_double && (!has_single || (double_at < single_at));
            open = double_first ? "\"\"\"" : "'''";
            open_start = line.index;
            rest += (double_first ? double_at : single_at) + 3u;
        }
        if (!EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx, .text = text, .size = size,
                        .line = &line))
        {
            return 0;
        }
    }
}

// whether a line is blank or the quotes alone
EMBED_INLINE int docstring_blank(const FillDocstringCtx *args)
{
    const char *text = args->text;
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + args->line->start,
               .size = args->line->end - args->line->start, .from_u64s = &from, .to_u64s = &to);
    return (to == from) ||
           ((to - from == 3u) &&
            (memor.cmp(&(MemoriaCfg){.src = text + args->line->start + from, .other = args->quote, .bytes = 3u}) == 0));
}

// the last line written with its last word sent down a line under `lead` where the closing quotes would pass the
// margin, its words starting at `start_of_words`
EMBED_INLINE void docstring_close(const FillDocstringCtx *args)
{
    CliFilled *filled = args->filled;
    unsigned long long size = 0ULL;
    const char *const last = EMBED_CALL(cli_fill_filled_size_internal.filled_last, CliFillFilledSizeCtx,
                                        .filled_read = filled, .size_pointer = &size);
    unsigned long long space = 0ULL;
    if ((EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = last, .size = size) + 3u <= args->width) ||
        (args->start_of_words > size) ||
        !EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = last + args->start_of_words,
                    .length_value = size - args->start_of_words, .required = " ", .size = 1u, .at_pointer = &space) ||
        !EMBED_CALL(cli_text_size_at.find_last, CliTextSizeAtCfg, .text = last, .length_value = size, .required = " ",
                    .size = 1u, .at_pointer = &space))
    {
        return;
    }
    VerbaLinea *const out = filled->out;
    const unsigned long long cut = filled->line_start + space;
    const unsigned long long word_text = size - space - 1u;
    const unsigned long long end = cut + 1u + args->comment_prefix_size + word_text;
    if (end >= out->capacity)
    {
        out->at = out->capacity;
        return;
    }
    memor.move_up(&(MemoriaCfg){.dst = out->out + cut + 1u + args->comment_prefix_size,
                                .src = out->out + cut + 1u,
                                .bytes = (size_t)word_text});
    memor.cpy(&(MemoriaCfg){.dst = out->out + cut + 1u, .src = args->lead, .bytes = (size_t)args->comment_prefix_size});
    out->out[cut] = '\n';
    out->at = end;
    filled->line_start = cut + 1u;
    filled->lines += 1u;
}

int cli_fill_docstring(const CliFillTextCtx *args)
{
    CliFillString string = {NULL, 0u, 0u};
    if (!EMBED_CALL(docstring_around, FillDocstringCtx, .text = args->text, .size = args->size, .at = args->at->index,
                    .found = &string) ||
        EMBED_CALL(docstring_blank, FillDocstringCtx, .text = args->text, .line = args->at, .quote = string.quote))
    {
        return 0;
    }
    CliFillLine first = *args->at;
    CliFillLine before = first;
    while ((first.index > string.start) &&
           EMBED_CALL(cli_fill_text_size_internal.before, CliFillTextSizeCtx, .text = args->text, .line = &before) &&
           !EMBED_CALL(docstring_blank, FillDocstringCtx, .text = args->text, .line = &before, .quote = string.quote))
    {
        first = before;
    }
    CliFillLine last = *args->at;
    CliFillLine after = last;
    while ((last.index < string.end) &&
           EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx, .text = args->text, .size = args->size,
                      .line = &after) &&
           !EMBED_CALL(docstring_blank, FillDocstringCtx, .text = args->text, .line = &after, .quote = string.quote))
    {
        last = after;
    }
    const char *const lead = args->text + first.start;
    const unsigned long long comment_prefix_size = EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                              .text = lead, .size = first.end - first.start);
    unsigned long long opening = 0ULL;
    if (first.index == string.start)
    {
        if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = lead + comment_prefix_size,
                        .length_value = first.end - first.start - comment_prefix_size, .required = string.quote,
                        .size = 3u, .at_pointer = &opening))
        {
            return 0;
        }
        opening += 3u;
    }
    const int closing = (last.index == string.end);
    unsigned int count = 0u;
    for (CliFillLine line = first; count < g_cli_fill_slices;)
    {
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->text + line.start,
                   .size = line.end - line.start, .from_u64s = &from, .to_u64s = &to);
        const char *body = args->text + line.start + from;
        unsigned long long body_size = to - from;
        if ((line.index == first.index) && (opening > 0u))
        {
            body += opening;
            body_size -= opening;
            const unsigned long long skipped =
                EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = body, .size = body_size);
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
            body_size = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = body, .size = body_size);
        }
        g_cli_fill_slice[count] = body;
        g_cli_fill_slice_size[count] = body_size;
        count += 1u;
        if ((line.index == last.index) || !EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx,
                                                      .text = args->text, .size = args->size, .line = &line))
        {
            break;
        }
    }
    EMBED_CALL(cli_fill_filled_internal.words, CliFillFilledCtx, .filled = args->filled, .slice = 0u, .count = count,
               .first = lead, .first_size = comment_prefix_size + opening, .rest = lead,
               .remainder_size = comment_prefix_size, .width = args->width);
    if (closing)
    {
        const unsigned long long start_of_words =
            (args->filled->lines == 1u) ? comment_prefix_size + opening : comment_prefix_size;
        EMBED_CALL(docstring_close, FillDocstringCtx, .filled = args->filled, .lead = lead,
                   .comment_prefix_size = comment_prefix_size, .start_of_words = start_of_words, .width = args->width);
        mmgr_line_put_n(args->filled->out, string.quote, 3u);
    }
    EMBED_CALL(cli_fill_text_filled_internal.edit_get, CliFillTextFilledCtx, .text = args->text,
               .first_fill_line_pointer = &first, .last = &last, .filled_read = args->filled, .edit = args->edit);
    return 1;
}
