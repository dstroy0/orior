// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_comment.c: a `/** */` comment written again whole. Its lines are read without their stars, the opening and the
// closing lines' text retained where they contain any; split into paragraphs at blank lines and at each tag; each
// paragraph's words filled under ` * `, and each run of tags of one kind lined up: the tags, their types in braces, the
// names of those that name one, and their descriptions each in one column, a description that runs past the margin
// going on under its own first word. A run's tags are joined from their lines into a room sized to the comment's bytes
// it left out of the run
#include "../command_line/cells.h"
#include "../text/text.h"
#include "fill_internal.h"

// A tag of a run: the tag, its type in braces, its name, and its description
typedef struct
{
    const char *tag;
    unsigned long long tag_size;
    const char *kind;
    unsigned long long kind_size;
    const char *name;
    unsigned long long name_size;
    const char *description;
    unsigned long long description_size;
} CliFillTag;
// the bytes a tag line's head, its hanging lead and the comment's lead each occupy
#define CLI_FILL_LINE_ROOM (1u << 16u)

// the tags whose first word after a type is a name
static const char *const s_named_tags[7] = {"param", "arg", "argument", "property", "prop", "template", "typedef"};

// A fill's room, parts of one cell taken for the comment filled, sized to its lines and bytes, and given back as the
// fill ends: each part's first line among the comment's lines and its count, 0 for a blank line between parts; each
// part's tag; the tags' texts joined; and a tag line's head, its hanging lead and the comment's lead
typedef struct
{
    unsigned int *part_first;
    unsigned int *part_lines;
    unsigned int parts_most;
    CliFillTag *tag;
    char *joined;
    unsigned long long joined_most;
    char *head;
    char *hang;
    char *lead;
} CliFillRoom;

static CliFillRoom s_room;
static CliFillRoom *s_fill;

int cli_fill_comment_lock(const CliFillCtx *args)
{
    (void)args;
    s_fill = &s_room;
    return 1;
}

void cli_fill_comment_release(void)
{
    s_fill = NULL;
}

typedef struct
{
    unsigned char **at;
    unsigned long long bytes;
    unsigned int lines;
    VerbaLinea *err_msg;
    const char *text;
    const CliFillLine *line_fill_line_pointer;
    unsigned long long *size_pointer;
    unsigned long long size;
    const CliFillLine *at_fill_line_pointer;
    CliFillLine *first;
    CliFillLine *last;
    unsigned int count;
    const char *line;
    int stored;
    const CliFillLine *first_read;
    const CliFillLine *last_read;
    unsigned long long *tag_size;
    const char **kind;
    unsigned long long *kind_size;
    CliFillTag *tag;
    VerbaLinea *out;
    unsigned long long width;
    unsigned int part;
    unsigned long long *used;
    CliFilled *filled;
    unsigned int *part_pointer;
    unsigned int parts;
    const char *lead;
    unsigned long long lead_size;
    unsigned int width_u32;
} FillCommentCtx;
// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *comment_part(const FillCommentCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the fill's cell taken for a comment of `lines` lines and `bytes` bytes and the room's parts set into it: the cell,
// or NULL with why given where it is refused
EMBED_INLINE void *comment_room_take(const FillCommentCtx *args)
{
    const unsigned long long joined = args->bytes + args->lines + 1ull;
    const unsigned long long size =
        ((unsigned long long)args->lines * ((2ull * sizeof(unsigned int)) + sizeof(CliFillTag))) + joined +
        (3ull * CLI_FILL_LINE_ROOM) + (8ull * 8ull);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(size, &g_cli_editor_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a comment's fill could not be taken");
        return NULL;
    }
    unsigned char *at = cell;
    s_fill->part_first = EMBED_CALL(comment_part, FillCommentCtx, .at = &at,
                                    .bytes = (unsigned long long)args->lines * sizeof(unsigned int));
    s_fill->part_lines = EMBED_CALL(comment_part, FillCommentCtx, .at = &at,
                                    .bytes = (unsigned long long)args->lines * sizeof(unsigned int));
    s_fill->parts_most = args->lines;
    s_fill->tag = EMBED_CALL(comment_part, FillCommentCtx, .at = &at,
                             .bytes = (unsigned long long)args->lines * sizeof(CliFillTag));
    s_fill->joined = EMBED_CALL(comment_part, FillCommentCtx, .at = &at, .bytes = joined);
    s_fill->joined_most = joined;
    s_fill->head = EMBED_CALL(comment_part, FillCommentCtx, .at = &at, .bytes = CLI_FILL_LINE_ROOM);
    s_fill->hang = EMBED_CALL(comment_part, FillCommentCtx, .at = &at, .bytes = CLI_FILL_LINE_ROOM);
    s_fill->lead = EMBED_CALL(comment_part, FillCommentCtx, .at = &at, .bytes = CLI_FILL_LINE_ROOM);
    return cell;
}

// the start of a line's text, its whitespace taken off
EMBED_INLINE const char *comment_trimmed_start(const FillCommentCtx *args)
{
    const char *text = args->text;
    unsigned long long *size = args->size_pointer;
    const unsigned long long lead = EMBED_CALL(
        cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text + args->line_fill_line_pointer->start,
        .size = args->line_fill_line_pointer->end - args->line_fill_line_pointer->start);
    *size = args->line_fill_line_pointer->end - args->line_fill_line_pointer->start - lead;
    return text + args->line_fill_line_pointer->start + lead;
}

// the bounds of the comment around `at`, its opening line and its closing line: 1, or 0 where `at` is in none
EMBED_INLINE int comment_bounds(const FillCommentCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliFillLine *first = args->first;
    CliFillLine *last = args->last;
    *first = *args->at_fill_line_pointer;
    for (;;)
    {
        unsigned long long length = 0ull;
        const char *const line = EMBED_CALL(comment_trimmed_start, FillCommentCtx, .text = text,
                                            .line_fill_line_pointer = first, .size_pointer = &length);
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = line, .size = length, .prefix = "/*"))
        {
            break;
        }
        if (!EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = line, .size = length,
                        .prefix = "*") ||
            (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = line, .size = length,
                        .prefix = "*/") &&
             (first->index != args->at_fill_line_pointer->index)) ||
            (first->index == 0u))
        {
            return 0;
        }
        (void)EMBED_CALL(cli_fill_text_size_internal.before, CliFillTextSizeCtx, .text = text, .line = first);
    }
    *last = *args->at_fill_line_pointer;
    for (;;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg,
                                                  .text = text + last->start, .size = last->end - last->start);
        if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = text + last->start, .size = end,
                       .suffix = "*/"))
        {
            break;
        }
        if (!EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx, .text = text, .size = size,
                        .line = last))
        {
            return 0;
        }
        unsigned long long length = 0ull;
        const char *const line = EMBED_CALL(comment_trimmed_start, FillCommentCtx, .text = text,
                                            .line_fill_line_pointer = last, .size_pointer = &length);
        if (!EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = line, .size = length, .prefix = "*"))
        {
            return 0;
        }
    }
    return first->index != last->index;
}

// a comment's line put among its lines where it is retained, one place left after them for a description: their count
// after
EMBED_INLINE unsigned int comment_body_put(const FillCommentCtx *args)
{
    if (args->stored && (args->count + 1u < g_cli_fill_slices))
    {
        g_cli_fill_slice[args->count] = args->line;
        g_cli_fill_slice_size[args->count] = args->size;
        return args->count + 1u;
    }
    return args->count;
}

// the comment's lines between `first` and `last`, each without its star, the opening and closing lines' text where
// they contain any: their count
EMBED_INLINE unsigned int comment_body(const FillCommentCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + args->first_read->start,
               .size = args->first_read->end - args->first_read->start, .from_u64s = &from, .to_u64s = &to);
    const char *open = text + args->first_read->start + from;
    unsigned long long open_size = to - from;
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                   .text = open, .at = 0ull, .size = open_size, .set = "/");
        open += lead;
        open_size -= lead;
    }
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                   .text = open, .at = 0ull, .size = open_size, .set = "*");
        open += lead;
        open_size -= lead;
    }
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = open, .size = open_size,
               .from_u64s = &from, .to_u64s = &to);
    unsigned int count = EMBED_CALL(comment_body_put, FillCommentCtx, .count = 0u, .line = open + from,
                                    .size = to - from, .stored = to > from);
    CliFillLine line = *args->first_read;
    while (
        EMBED_CALL(cli_fill_text_size_internal.after, CliFillTextSizeCtx, .text = text, .size = size, .line = &line) &&
        (line.index < args->last_read->index))
    {
        unsigned long long length = 0ull;
        const char *body = EMBED_CALL(comment_trimmed_start, FillCommentCtx, .text = text,
                                      .line_fill_line_pointer = &line, .size_pointer = &length);
        {
            const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                       .text = body, .at = 0ull, .size = length, .set = "*");
            body += lead;
            length -= lead;
        }
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = body, .size = length,
                   .from_u64s = &from, .to_u64s = &to);
        count = EMBED_CALL(comment_body_put, FillCommentCtx, .count = count, .line = body + from, .size = to - from,
                           .stored = 1);
    }
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + args->last_read->start,
               .size = args->last_read->end - args->last_read->start, .from_u64s = &from, .to_u64s = &to);
    const char *close = text + args->last_read->start + from;
    unsigned long long close_size = to - from;
    close_size = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = close,
                            .at = close_size, .set = "/");
    close_size = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = close,
                            .at = close_size, .set = "*");
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                   .text = close, .at = 0ull, .size = close_size, .set = "*");
        close += lead;
        close_size -= lead;
    }
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = close, .size = close_size,
               .from_u64s = &from, .to_u64s = &to);
    return EMBED_CALL(comment_body_put, FillCommentCtx, .count = count, .line = close + from, .size = to - from,
                      .stored = to > from);
}

// the comment's lines split into parts: each blank line a part of no lines, each tag the start of a part, and each
// other line put on the part before it where that contains lines: their count
EMBED_INLINE unsigned int comment_parts(const FillCommentCtx *args)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < args->lines; at += 1u)
    {
        const int blank = (g_cli_fill_slice_size[at] == 0u);
        const int tag = !blank && (g_cli_fill_slice[at][0] == '@');
        if (!blank && !tag && (count > 0u) && (s_fill->part_lines[count - 1u] > 0u))
        {
            s_fill->part_lines[count - 1u] += 1u;
        }
        else if (count < s_fill->parts_most)
        {
            s_fill->part_first[count] = at;
            s_fill->part_lines[count] = blank ? 0u : 1u;
            count += 1u;
        }
    }
    return count;
}

// the kind of the tag a part of `size` bytes starts with: its tag, its first word, into `tag`, and its kind, the tag
// without its `@`, into `kind`
EMBED_INLINE void comment_tag_get(const FillCommentCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *tag_size = args->tag_size;
    const char **kind = args->kind;
    unsigned long long *kind_size = args->kind_size;
    unsigned long long walk = 0ull;
    unsigned long long word = 0ull;
    *tag_size = 0ull;
    (void)EMBED_CALL(cli_text_size_length.word, CliTextSizeLengthCfg, .text = args->line, .size = size,
                     .at_pointer = &walk, .word = &word, .length = tag_size);
    *kind = args->line;
    *kind_size = *tag_size;
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                   .text = *kind, .at = 0ull, .size = *kind_size, .set = "@");
        *kind += lead;
        *kind_size -= lead;
    }
}

// a tag of `size` bytes split into its parts
EMBED_INLINE void comment_split_tag(const FillCommentCtx *args)
{
    unsigned long long size = args->size;
    CliFillTag *tag = args->tag;
    const char *kind = NULL;
    unsigned long long kind_size = 0ull;
    EMBED_CALL(comment_tag_get, FillCommentCtx, .line = args->line, .size = size, .tag_size = &tag->tag_size,
               .kind = &kind, .kind_size = &kind_size);
    tag->tag = args->line;
    const char *rest = args->line + tag->tag_size;
    unsigned long long rest_size = size - tag->tag_size;
    unsigned long long skipped =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = rest, .size = rest_size);
    rest += skipped;
    rest_size -= skipped;
    tag->kind = rest;
    tag->kind_size = 0ull;
    if ((rest_size > 0u) && (rest[0] == '{'))
    {
        long long depth = 0;
        unsigned long long end = 0ull;
        for (unsigned long long at = 0ull; at < rest_size; at += 1u)
        {
            depth += (rest[at] == '{') ? 1 : (rest[at] == '}') ? -1 : 0;
            if ((rest[at] == '}') && (depth == 0))
            {
                end = at + 1u;
                break;
            }
        }
        if (end > 0u)
        {
            tag->kind_size = end;
            rest += end;
            rest_size -= end;
            skipped =
                EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = rest, .size = rest_size);
            rest += skipped;
            rest_size -= skipped;
        }
    }
    tag->name = rest;
    tag->name_size = 0ull;
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_named_tags[at], .cap = CLI_CELLS_POOL}) == kind_size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_named_tags[at], .other = kind, .bytes = (size_t)kind_size}) == 0))
        {
            unsigned long long walk = 0ull;
            unsigned long long word = 0ull;
            (void)EMBED_CALL(cli_text_size_length.word, CliTextSizeLengthCfg, .text = rest, .size = rest_size,
                             .at_pointer = &walk, .word = &word, .length = &tag->name_size);
            rest += tag->name_size;
            rest_size -= tag->name_size;
            skipped =
                EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = rest, .size = rest_size);
            rest += skipped;
            rest_size -= skipped;
            break;
        }
    }
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                   .text = rest, .at = 0ull, .size = rest_size, .set = "-");
        rest += lead;
        rest_size -= lead;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = rest, .size = rest_size,
               .from_u64s = &from, .to_u64s = &to);
    tag->description = rest + from;
    tag->description_size = to - from;
}

// `size` bytes of `text` written padded with spaces to `width` letters
EMBED_INLINE void comment_pad(const FillCommentCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    mmgr_line_put_n(args->out, text, size);
    for (unsigned long long letters =
             EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = text, .size = size);
         letters < args->width; letters += 1u)
    {
        mmgr_line_ch(args->out, ' ');
    }
}

// the part at `part` of the comment's lines joined by spaces into the room from `*used`: its start and size
EMBED_INLINE const char *comment_join(const FillCommentCtx *args)
{
    unsigned long long *used = args->used;
    unsigned long long *size = args->size_pointer;
    char *const start = s_fill->joined + *used;
    VerbaLinea line = {start, s_fill->joined_most - *used, 0ull};
    for (unsigned int at = 0u; at < s_fill->part_lines[args->part]; at += 1u)
    {
        const unsigned int index = s_fill->part_first[args->part] + at;
        mmgr_line_put(&line, (at > 0u) ? " " : "");
        mmgr_line_put_n(&line, g_cli_fill_slice[index], g_cli_fill_slice_size[index]);
    }
    if (!mmgr_line_ok(&line))
    {
        return NULL;
    }
    *used += line.at;
    *size = line.at;
    return start;
}

// the run of tags of one kind from the part at `*part`, lined up and written, `*part` past it
EMBED_INLINE void comment_run(const FillCommentCtx *args)
{
    CliFilled *filled = args->filled;
    unsigned int *part = args->part_pointer;
    unsigned int width = args->width_u32;
    const char *kind = NULL;
    unsigned long long kind_size = 0ull;
    unsigned long long tag_size = 0ull;
    const unsigned int head_line = s_fill->part_first[*part];
    EMBED_CALL(comment_tag_get, FillCommentCtx, .line = g_cli_fill_slice[head_line],
               .size = g_cli_fill_slice_size[head_line], .tag_size = &tag_size, .kind = &kind, .kind_size = &kind_size);
    unsigned int count = 0u;
    unsigned long long used = 0ull;
    unsigned long long widths[3] = {0ull, 0ull, 0ull};
    while ((*part < args->parts) && (s_fill->part_lines[*part] > 0u) &&
           (g_cli_fill_slice[s_fill->part_first[*part]][0] == '@'))
    {
        const char *other = NULL;
        unsigned long long other_size = 0ull;
        const unsigned int line = s_fill->part_first[*part];
        EMBED_CALL(comment_tag_get, FillCommentCtx, .line = g_cli_fill_slice[line], .size = g_cli_fill_slice_size[line],
                   .tag_size = &tag_size, .kind = &other, .kind_size = &other_size);
        if ((other_size != kind_size) ||
            (memor.cmp(&(MemoriaCfg){.src = other, .other = kind, .bytes = (size_t)kind_size}) != 0))
        {
            break;
        }
        unsigned long long size = 0ull;
        const char *const joined =
            EMBED_CALL(comment_join, FillCommentCtx, .part = *part, .used = &used, .size_pointer = &size);
        *part += 1u;
        if (joined == NULL)
        {
            continue;
        }
        CliFillTag *const tag = &s_fill->tag[count];
        EMBED_CALL(comment_split_tag, FillCommentCtx, .line = joined, .size = size, .tag = tag);
        const unsigned long long letters[3] = {
            EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = tag->tag, .size = tag->tag_size),
            EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = tag->kind, .size = tag->kind_size),
            EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = tag->name, .size = tag->name_size)};
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            widths[at] = (letters[at] > widths[at]) ? letters[at] : widths[at];
        }
        count += 1u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliFillTag *const tag = &s_fill->tag[at];
        VerbaLinea head = {s_fill->head, CLI_FILL_LINE_ROOM, 0ull};
        mmgr_line_put_n(&head, args->lead, args->lead_size);
        EMBED_CALL(comment_pad, FillCommentCtx, .out = &head, .text = tag->tag, .size = tag->tag_size,
                   .width = widths[0]);
        if (widths[1] > 0u)
        {
            mmgr_line_ch(&head, ' ');
            EMBED_CALL(comment_pad, FillCommentCtx, .out = &head, .text = tag->kind, .size = tag->kind_size,
                       .width = widths[1]);
        }
        if (widths[2] > 0u)
        {
            mmgr_line_ch(&head, ' ');
            EMBED_CALL(comment_pad, FillCommentCtx, .out = &head, .text = tag->name, .size = tag->name_size,
                       .width = widths[2]);
        }
        if (tag->description_size == 0u)
        {
            EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = filled, .bytes = s_fill->head,
                       .size = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = s_fill->head,
                                          .size = head.at));
            continue;
        }
        mmgr_line_ch(&head, ' ');
        VerbaLinea hang = {s_fill->hang, CLI_FILL_LINE_ROOM, 0ull};
        mmgr_line_put_n(&hang, args->lead, args->lead_size);
        for (unsigned long long letters = EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->lead,
                                                     .size = args->lead_size);
             letters <
             EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = s_fill->head, .size = head.at);
             letters += 1u)
        {
            mmgr_line_ch(&hang, ' ');
        }
        g_cli_fill_slice[args->lines] = tag->description;
        g_cli_fill_slice_size[args->lines] = tag->description_size;
        EMBED_CALL(cli_fill_filled_internal.words, CliFillFilledCtx, .filled = filled, .slice = args->lines,
                   .count = 1u, .first = s_fill->head, .first_size = head.at, .rest = s_fill->hang,
                   .rest_size = hang.at, .width = width);
    }
}

int cli_fill_comment(const CliFillTextCtx *args)
{
    CliFillLine first = *args->at;
    CliFillLine last = *args->at;
    if (!EMBED_CALL(comment_bounds, FillCommentCtx, .text = args->text, .size = args->size,
                    .at_fill_line_pointer = args->at, .first = &first, .last = &last))
    {
        return 0;
    }
    const char *const indent = args->text + first.start;
    const unsigned long long indent_size = EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                      .text = indent, .size = first.end - first.start);
    const unsigned int lines = EMBED_CALL(comment_body, FillCommentCtx, .text = args->text, .size = args->size,
                                          .first_read = &first, .last_read = &last);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    void *const cell = EMBED_CALL(comment_room_take, FillCommentCtx, .lines = lines, .bytes = last.end - first.start,
                                  .err_msg = &err_msg);
    if (cell == NULL)
    {
        return 0;
    }
    VerbaLinea lead = {s_fill->lead, CLI_FILL_LINE_ROOM, 0ull};
    mmgr_line_put_n(&lead, indent, indent_size);
    mmgr_line_put(&lead, " * ");
    const unsigned int parts = EMBED_CALL(comment_parts, FillCommentCtx, .lines = lines);
    EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = args->filled, .bytes = indent,
               .size = indent_size);
    mmgr_line_put(args->filled->out, "/**");
    for (unsigned int part = 0u; part < parts;)
    {
        if (s_fill->part_lines[part] == 0u)
        {
            unsigned long long last_size = 0ull;
            const char *const written = EMBED_CALL(cli_fill_filled_size_internal.filled_last, CliFillFilledSizeCtx,
                                                   .filled_read = args->filled, .size_pointer = &last_size);
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = written, .size = last_size,
                       .from_u64s = &from, .to_u64s = &to);
            const int star = (to - from == 1u) && (written[from] == '*');
            const int open = (to - from == 3u) &&
                             (memor.cmp(&(MemoriaCfg){.src = written + from, .other = "/**", .bytes = 3u}) == 0);
            if (!star && !open)
            {
                EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = args->filled, .bytes = indent,
                           .size = indent_size);
                mmgr_line_put(args->filled->out, " *");
            }
            part += 1u;
            continue;
        }
        if (g_cli_fill_slice[s_fill->part_first[part]][0] != '@')
        {
            EMBED_CALL(cli_fill_filled_internal.words, CliFillFilledCtx, .filled = args->filled,
                       .slice = s_fill->part_first[part], .count = s_fill->part_lines[part], .first = s_fill->lead,
                       .first_size = lead.at, .rest = s_fill->lead, .rest_size = lead.at, .width = args->width);
            part += 1u;
            continue;
        }
        EMBED_CALL(comment_run, FillCommentCtx, .filled = args->filled, .part_pointer = &part, .parts = parts,
                   .lead = s_fill->lead, .lead_size = lead.at, .width_u32 = args->width, .lines = lines);
    }
    for (;;)
    {
        unsigned long long last_size = 0ull;
        const char *const written = EMBED_CALL(cli_fill_filled_size_internal.filled_last, CliFillFilledSizeCtx,
                                               .filled_read = args->filled, .size_pointer = &last_size);
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = written, .size = last_size,
                   .from_u64s = &from, .to_u64s = &to);
        if ((to - from != 1u) || (written[from] != '*'))
        {
            break;
        }
        cli_fill_text_internal.filled_drop(args);
    }
    EMBED_CALL(cli_fill_size_internal.filled_line, CliFillSizeCtx, .filled = args->filled, .bytes = indent,
               .size = indent_size);
    mmgr_line_put(args->filled->out, " */");
    EMBED_CALL(cli_fill_text_filled_internal.edit_get, CliFillTextFilledCtx, .text = args->text,
               .first_fill_line_pointer = &first, .last = &last, .filled_read = args->filled, .edit = args->edit);
    g_cli_cells->persistent_buf_release(cell, &g_cli_editor_cells);
    return 1;
}
