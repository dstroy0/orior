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

// the marks a comment line starts with, the longest first
static const char *const s_marks[6] = {"///", "//!", "//", "##", "#", "--"};

int cli_fill_line_at(const CliFillTextSizeCtx *args)
{
    unsigned int index = args->index;
    args->line->start = 0ull;
    args->line->index = 0u;
    for (;;)
    {
        const char *const newline = (args->line->start < args->size)
                                        ? memor.chr(&(MemoriaCfg){.src = args->text + args->line->start,
                                                                  .val = '\n',
                                                                  .bytes = (size_t)(args->size - args->line->start)})
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
                                                              .bytes = (size_t)(args->size - args->line->start)})
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
    *size = (args->filled_read->lines > 0u) ? args->filled_read->out->at - args->filled_read->line_start : 0ull;
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
    args->filled->out->at = (args->filled->lines > 0u) ? args->filled->line_start - 1u : 0ull;
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
        unsigned long long walk = 0ull;
        unsigned long long word = 0ull;
        unsigned long long length = 0ull;
        while (EMBED_CALL(cli_text_size_length.word, CliTextSizeLengthCfg, .text = text, .size = size,
                          .at_pointer = &walk, .word = &word, .length = &length))
        {
            const unsigned long long word_letters =
                EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = text + word, .size = length);
            const unsigned long long lead = empty ? letters : letters + 1u;
            if (!empty && (lead + word_letters > args->width))
            {
                EMBED_CALL(fill_end, FillCtx, .filled = args->filled);
                EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = args->filled,
                           .bytes = args->rest, .size = args->rest_size);
                letters = EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->rest,
                                     .size = args->rest_size);
                empty = 1;
            }
            if (!empty)
            {
                mmgr_line_ch(args->filled->out, ' ');
                letters += 1u;
            }
            mmgr_line_put_n(args->filled->out, text + word, length);
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

// the mark of the comment a line of `size` bytes, whitespace taken off its start, starts with: its place among the
// marks, 6 where it starts with none
EMBED_INLINE unsigned int fill_mark_get(const FillCtx *args)
{
    unsigned long long size = args->size;
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = args->trimmed, .size = size,
                       .prefix = s_marks[at]))
        {
            return at;
        }
    }
    return 6u;
}

// the text after `prefix` of a line that starts with it, trimmed, into `body`: 1, or 0 where the line does not start
// with it
EMBED_INLINE int fill_body_get(const FillCtx *args)
{
    const char *text = args->text;
    const char **body = args->body;
    unsigned long long *body_size = args->body_size;
    const unsigned long long size = args->line->end - args->line->start;
    if ((size < args->prefix_size) ||
        (memor.cmp(&(MemoriaCfg){
             .src = text + args->line->start, .other = args->prefix, .bytes = (size_t)args->prefix_size}) != 0))
    {
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg,
               .text = text + args->line->start + args->prefix_size, .size = size - args->prefix_size,
               .from_u64s = &from, .to_u64s = &to);
    *body = text + args->line->start + args->prefix_size + from;
    *body_size = to - from;
    return 1;
}

// whether a line is of the paragraph of the comment of mark `mark` and `prefix`: it starts with the prefix and contains
// text after it, and starts with no longer mark
EMBED_INLINE int fill_in_comment(const FillCtx *args)
{
    const char *text = args->text;
    const char *prefix = args->prefix;
    const char *body = NULL;
    unsigned long long body_size = 0ull;
    if (!EMBED_CALL(fill_body_get, FillCtx, .text = text, .line = args->line, .prefix = prefix,
                    .prefix_size = args->prefix_size, .body = &body, .body_size = &body_size) ||
        (body_size == 0u))
    {
        return 0;
    }
    const unsigned long long size = args->line->end - args->line->start;
    const unsigned long long lead = EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                               .text = text + args->line->start, .size = size);
    for (unsigned int other = 0u; other < 6u; other += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_marks[other], .cap = CLI_CELLS_POOL}) >
             cellul.len(&(CatenaFinitaCfg){.src = s_marks[args->mark], .cap = CLI_CELLS_POOL})) &&
            EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + args->line->start + lead,
                       .size = size - lead, .prefix = s_marks[other]))
        {
            return 0;
        }
    }
    return 1;
}

// the paragraph of a comment a line around `here`, whose mark is that at `mark`
EMBED_INLINE int fill_comment_lines(const FillCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int width = args->width;
    CliFilled *filled = args->filled;
    CliTextEdit *edit = args->edit;
    const char *const prefix = text + args->here->start;
    const unsigned long long prefix_size =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = prefix,
                   .size = args->here->end - args->here->start) +
        cellul.len(&(CatenaFinitaCfg){.src = s_marks[args->mark], .cap = CLI_CELLS_POOL});
    if (!EMBED_CALL(fill_in_comment, FillCtx, .text = text, .line = args->here, .mark = args->mark, .prefix = prefix,
                    .prefix_size = prefix_size))
    {
        mmgr_line_put(args->err_msg, "Fill Paragraph fills the paragraph the cursor is in.");
        return 0;
    }
    CliFillLine first = *args->here;
    CliFillLine before = first;
    while (EMBED_CALL(cli_fill_text_size_internal.before, CliFillTextSizeCtx, .text = text, .line = &before) &&
           EMBED_CALL(fill_in_comment, FillCtx, .text = text, .line = &before, .mark = args->mark, .prefix = prefix,
                      .prefix_size = prefix_size))
    {
        first = before;
    }
    CliFillLine last = *args->here;
    CliFillLine after = last;
    while (
        EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx, .text = text, .size = size, .line = &after) &&
        EMBED_CALL(fill_in_comment, FillCtx, .text = text, .line = &after, .mark = args->mark, .prefix = prefix,
                   .prefix_size = prefix_size))
    {
        last = after;
    }
    unsigned int count = 0u;
    for (CliFillLine line = first; count < g_cli_fill_slices;)
    {
        if (!EMBED_CALL(fill_body_get, FillCtx, .text = text, .line = &line, .prefix = prefix,
                        .prefix_size = prefix_size, .body = &g_cli_fill_slice[count],
                        .body_size = &g_cli_fill_slice_size[count]))
        {
            g_cli_fill_slice_size[count] = 0ull;
        }
        count += 1u;
        if ((line.index == last.index) || !EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx,
                                                      .text = text, .size = size, .line = &line))
        {
            break;
        }
    }
    static char s_lead[1u << 16u];
    VerbaLinea lead = {s_lead, sizeof(s_lead), 0ull};
    mmgr_line_put_n(&lead, prefix, prefix_size);
    mmgr_line_ch(&lead, ' ');
    EMBED_CALL(cli_fill_filled_internal.words, CliFillFilledCtx, .filled = filled, .slice = 0u, .count = count,
               .first = s_lead, .first_size = lead.at, .rest = s_lead, .rest_size = lead.at, .width = width);
    EMBED_CALL(cli_fill_text_filled_internal.edit_get, CliFillTextFilledCtx, .text = text,
               .first_fill_line_pointer = &first, .last = &last, .filled_read = filled, .edit = edit);
    return 1;
}

// whether a line is blank
EMBED_INLINE int fill_blank(const FillCtx *args)
{
    const char *text = args->text;
    return EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text + args->line->start,
                      .size = args->line->end - args->line->start) == args->line->end - args->line->start;
}

// the paragraph of plain text around `here`
EMBED_INLINE int fill_plain(const FillCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int width = args->width;
    CliFilled *filled = args->filled;
    CliTextEdit *edit = args->edit;
    if (EMBED_CALL(fill_blank, FillCtx, .text = text, .line = args->here))
    {
        mmgr_line_put(args->err_msg, "Fill Paragraph fills the paragraph the cursor is in.");
        return 0;
    }
    CliFillLine first = *args->here;
    CliFillLine before = first;
    while (EMBED_CALL(cli_fill_text_size_internal.before, CliFillTextSizeCtx, .text = text, .line = &before) &&
           !EMBED_CALL(fill_blank, FillCtx, .text = text, .line = &before))
    {
        first = before;
    }
    CliFillLine last = *args->here;
    CliFillLine after = last;
    while (
        EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx, .text = text, .size = size, .line = &after) &&
        !EMBED_CALL(fill_blank, FillCtx, .text = text, .line = &after))
    {
        last = after;
    }
    unsigned int count = 0u;
    for (CliFillLine line = first; count < g_cli_fill_slices;)
    {
        g_cli_fill_slice[count] = text + line.start;
        g_cli_fill_slice_size[count] = line.end - line.start;
        count += 1u;
        if ((line.index == last.index) || !EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx,
                                                      .text = text, .size = size, .line = &line))
        {
            break;
        }
    }
    const char *const lead = text + first.start;
    const unsigned long long lead_size = EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = lead,
                                                    .size = first.end - first.start);
    EMBED_CALL(cli_fill_filled_internal.words, CliFillFilledCtx, .filled = filled, .slice = 0u, .count = count,
               .first = lead, .first_size = lead_size, .rest = lead, .rest_size = lead_size, .width = width);
    EMBED_CALL(cli_fill_text_filled_internal.edit_get, CliFillTextFilledCtx, .text = text,
               .first_fill_line_pointer = &first, .last = &last, .filled_read = filled, .edit = edit);
    return 1;
}

int cli_fill(const CliFillCfg *args)
{
    if (!EMBED_CALL(cli_fill_internal.comment_lock, CliFillCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    // a slice is a line of the text, and one place is left after the lines: its newlines and two fit them
    unsigned long long slices = 2ull;
    for (unsigned long long at = 0ull; (at < args->size) && (slices < CLI_FILL_SLICES); at += 1ull)
    {
        slices += (args->text[at] == '\n') ? 1ull : 0ull;
    }
    // the slices are under CLI_FILL_SLICES, which an unsigned int stores
    g_cli_fill_slices = (unsigned int)slices;
    g_cli_fill_slice = g_cli_cells->persistent_buf_alloc(slices * (sizeof(const char *) + sizeof(unsigned long long)),
                                                         &g_cli_editor_cells);
    if (g_cli_fill_slice == NULL)
    {
        cli_fill_comment_release();
        mmgr_line_put(args->err_msg, "the texts a fill fills could not be taken");
        return 0;
    }
    g_cli_fill_slice_size = (unsigned long long *)(void *)(g_cli_fill_slice + slices);
    CliFilled filled = {args->written, 0ull, 0u};
    args->written->at = 0ull;
    CliFillLine here = {0ull, 0ull, 0u};
    int done = 0;
    if (!EMBED_CALL(cli_fill_text_size_internal.line_at, CliFillTextSizeCtx, .text = args->text, .size = args->size,
                    .index = args->line, .line = &here))
    {
        mmgr_line_put(args->err_msg, "No line is there.");
    }
    else if (EMBED_CALL(cli_fill_text_internal.comment, CliFillTextCtx, .text = args->text, .size = args->size,
                        .at = &here, .width = args->width, .filled = &filled, .edit = args->edit))
    {
        done = 1;
    }
    else
    {
        const unsigned long long lead = EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                   .text = args->text + here.start, .size = here.end - here.start);
        const unsigned int mark = EMBED_CALL(fill_mark_get, FillCtx, .trimmed = args->text + here.start + lead,
                                             .size = here.end - here.start - lead);
        if (mark < 6u)
        {
            done = EMBED_CALL(fill_comment_lines, FillCtx, .text = args->text, .size = args->size, .here = &here,
                              .mark = mark, .width = args->width, .filled = &filled, .edit = args->edit,
                              .err_msg = args->err_msg);
        }
        else if (EMBED_CALL(cli_fill_text_internal.docstring, CliFillTextCtx, .text = args->text, .size = args->size,
                            .at = &here, .width = args->width, .filled = &filled, .edit = args->edit))
        {
            done = 1;
        }
        else
        {
            done = EMBED_CALL(fill_plain, FillCtx, .text = args->text, .size = args->size, .here = &here,
                              .width = args->width, .filled = &filled, .edit = args->edit, .err_msg = args->err_msg);
        }
    }
    done = done && mmgr_line_ok(args->written);
    g_cli_cells->persistent_buf_release(g_cli_fill_slice, &g_cli_editor_cells);
    g_cli_fill_slice = NULL;
    g_cli_fill_slice_size = NULL;
    g_cli_fill_slices = 0u;
    cli_fill_comment_release();
    return done;
}
