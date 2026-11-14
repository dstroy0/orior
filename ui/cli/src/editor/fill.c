// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill.c: Fill Paragraph: the text's lines walked from the cursor's, the paragraph's words gathered by the texts they
// stand in, and filled into lines no wider than the margin. A paragraph of a comment a line takes the lines that start
// with the cursor line's indent and mark and contain text after it, a line of a longer mark not among them; a
// docstring's the lines of its quotes that are not blank; plain text's the lines that are not blank. One fill runs at a
// time
#include "../text/text.h"
#include "fill_internal.h"

const char **g_cli_fill_slice;
unsigned long long *g_cli_fill_slice_size;
unsigned int g_cli_fill_slices;

int cli_fill_line_at(const CliFillTextSizeCtx *args)
{
    unsigned int index = args->index;
    args->line->start = 0ULL;
    args->line->index = 0u;
    for (;;)
    {
        const char *const newline =
            (args->line->start < args->size)
                ? memor.chr(&(MemoriaCfg){.src = args->text + args->line->start,
                                          .val = '\n',
                                          .bytes = (embed_word)(args->size - args->line->start)})
                : NULL;
        args->line->end = (newline != NULL) ? (unsigned long long)(newline - args->text) : args->size;
        if (args->line->index == index)
        {
            return 1;
        }
        if (newline == NULL)
        {
            return 0;
        }
        args->line->start = args->line->end + 1u;
        args->line->index += 1u;
    }
}

int cli_fill_before(const CliFillTextSizeCtx *args)
{
    if (args->line->index == 0u)
    {
        return 0;
    }
    args->line->end = args->line->start - 1u;
    args->line->start = args->line->end;
    args->line->start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->text,
                                   .at = args->line->start, .set = "\n");
    args->line->index -= 1u;
    return 1;
}

int cli_fill_after(const CliFillTextSizeCtx *args)
{
    if (args->line->end >= args->size)
    {
        return 0;
    }
    args->line->start = args->line->end + 1u;
    const char *const newline = (args->line->start < args->size)
                                    ? memor.chr(&(MemoriaCfg){.src = args->text + args->line->start,
                                                              .val = '\n',
                                                              .bytes = (embed_word)(args->size - args->line->start)})
                                    : NULL;
    args->line->end = (newline != NULL) ? (unsigned long long)(newline - args->text) : args->size;
    args->line->index += 1u;
    return 1;
}

void cli_filled_begin(const CliFillSizeCtx *args)
{
    if (args->filled->lines > 0u)
    {
        mmgr_line_ch(args->filled->out, '\n');
    }
    args->filled->line_start = args->filled->out->at;
    args->filled->lines += 1u;
}

const char *cli_filled_last(const CliFillFilledSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    *size = (args->filled_read->lines > 0u) ? args->filled_read->out->at - args->filled_read->line_start : 0ULL;
    return args->filled_read->out->out + args->filled_read->line_start;
}

void cli_filled_line(const CliFillSizeCtx *args)
{
    cli_fill_size_internal.filled_begin(args);
    mmgr_line_put_n(args->filled->out, args->bytes, args->size);
}

void cli_filled_drop(const CliFillTextCtx *args)
{
    if (args->filled->lines == 0u)
    {
        return;
    }
    args->filled->lines -= 1u;
    args->filled->out->at = (args->filled->lines > 0u) ? args->filled->line_start - 1u : 0ULL;
    unsigned long long start = args->filled->out->at;
    if (args->filled->lines > 0u)
    {
        start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->filled->out->out,
                           .at = start, .set = "\n");
    }
    args->filled->line_start = start;
}

typedef struct
{
    CliFilled *filled;
    const char *trimmed;
    unsigned long long size;
    const char *text;
    const CliFillLine *line;
    const char *prefix;
    unsigned long long prefix_size;
    const char **body;
    unsigned long long *body_size;
    unsigned int mark;
    const CliFillLine *here;
    unsigned int width;
    CliTextEdit *edit;
    VerbaLinea *err_msg;
} FillCtx;
// the last line written with its whitespace at its end taken off
EMBED_INLINE void fill_end(const FillCtx *args)
{
    const unsigned long long size = args->filled->out->at - args->filled->line_start;
    args->filled->out->at =
        args->filled->line_start + EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg,
                                              .text = args->filled->out->out + args->filled->line_start, .size = size);
}

void cli_fill_words(const CliFillFilledCtx *args)
{
    EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = args->filled, .bytes = args->first,
               .size = args->first_size);
    unsigned long long letters =
        EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->first, .size = args->first_size);
    int empty = 1;
    for (unsigned int at = args->slice; at < args->slice + args->count; at += 1u)
    {
        const char *const text = g_cli_fill_slice[at];
        const unsigned long long size = g_cli_fill_slice_size[at];
        unsigned long long walk = 0ULL;
        unsigned long long word_text = 0ULL;
        unsigned long long length = 0ULL;
        while (EMBED_CALL(cli_text_size_length.word_text, CliTextSizeLengthCfg, .text = text, .size = size,
                          .at_pointer = &walk, .word_text = &word_text, .length = &length))
        {
            const unsigned long long word_letters =
                EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = text + word_text, .size = length);
            const unsigned long long lead = empty ? letters : letters + 1u;
            if (!empty && (lead + word_letters > args->width))
            {
                EMBED_CALL(fill_end, FillCtx, .filled = args->filled);
                EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = args->filled,
                           .bytes = args->rest, .size = args->remainder_size);
                letters = EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->rest,
                                     .size = args->remainder_size);
                empty = 1;
            }
            if (!empty)
            {
                mmgr_line_ch(args->filled->out, ' ');
                letters += 1u;
            }
            mmgr_line_put_n(args->filled->out, text + word_text, length);
            letters += word_letters;
            empty = 0;
        }
    }
    EMBED_CALL(fill_end, FillCtx, .filled = args->filled);
}

void cli_fill_edit_get(const CliFillTextFilledCtx *args)
{
    const char *text = args->text;
    args->edit->from.line = args->first_fill_line_pointer->index;
    args->edit->from.col = 0u;
    args->edit->to.line = args->last->index;
    args->edit->to.col =
        (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text + args->last->start,
                                 .size = args->last->end - args->last->start);
    args->edit->text = args->filled_read->out->out;
    args->edit->size = args->filled_read->out->at;
}
