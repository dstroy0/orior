// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_sync.c: the boxes of text a SyncTeX file reports each page contains, each with the lines of a file of the tree
// that set it, in the page's own points from its top left
#include "latex_internal.h"

#include <math.h>

// One record of a SyncTeX file: its file's tag, its line, its point and, for a box, its width, height and depth, all
// in the file's own units
typedef struct
{
    unsigned int tag;
    unsigned long long line;
    long long x;
    long long y;
    long long size[3];
} CliTexRecord;
// The scale a SyncTeX file gives its units by
typedef struct
{
    double unit;
    double magnification;
    double x_offset;
    double y_offset;
} CliTexScale;

typedef struct
{
    const char *text;
    unsigned long long *start;
    unsigned long long *size;
    unsigned long long length;
    unsigned long long at;
    char letter;
    unsigned long long start_value;
    long long *value;
    CliTexRecord *record;
    long long value_value;
    double offset;
    const CliTexScale *scale;
    unsigned int files;
    unsigned int tag;
    const char *rest;
    const CliTexNames *names;
    unsigned int *files_pointer;
    unsigned int *used;
    const CliTexOpen *done;
    unsigned int marks;
    unsigned long long page;
    int *written;
    VerbaLinea *out;
    const char *line;
    unsigned long long size_value;
    CliTexScale *scale_out;
    int *content;
} LatexSyncCtx;
// `length` bytes from `start` of `text` with the ASCII whitespace about them taken off, as their start and size
EMBED_INLINE void latex_trim(const LatexSyncCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *size = args->size;
    while ((*size > 0ull) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->text[*start]))
    {
        *start += 1ull;
        *size -= 1ull;
    }
    while ((*size > 0ull) &&
           EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->text[*start + *size - 1ull]))
    {
        *size -= 1ull;
    }
}

// where `letter` is first in `length` bytes of `text` from `at`, `length` where it is not
EMBED_INLINE unsigned long long latex_at(const LatexSyncCtx *args)
{
    const char *const found =
        (args->at < args->length)
            ? memor.chr(
                  &(MemoriaCfg){.src = args->text + args->at, .val = args->letter, .bytes = args->length - args->at})
            : NULL;
    return (found != NULL) ? (unsigned long long)(found - args->text) : args->length;
}

// `length` bytes from `start` of `text` read trimmed as a signed number into `value`: 1, or 0 where they read as none
EMBED_INLINE int latex_signed_trim(const LatexSyncCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    long long *value = args->value;
    unsigned long long from = args->start_value;
    unsigned long long size = length;
    EMBED_CALL(latex_trim, LatexSyncCtx, .text = text, .start = &from, .size = &size);
    return EMBED_CALL(cli_latex_text_length_value_i_64s_internal.tex_sign, CliLatexTextLengthValueI64sCtx,
                      .text = text + from, .length = size, .value_i64s = value);
}

// a record, `tag,line:x,y:width,height,depth` after its kind, read into `record`: 1, or 0 where it is not one
static int latex_record(const LatexSyncCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    const unsigned long long colon =
        EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = length, .at = 0ull, .letter = ':');
    if (colon == length)
    {
        return 0;
    }
    const unsigned long long comma =
        EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = colon, .at = 0ull, .letter = ',');
    if (comma == colon)
    {
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long size = comma;
    EMBED_CALL(latex_trim, LatexSyncCtx, .text = text, .start = &from, .size = &size);
    unsigned long long tag = 0ull;
    if (!EMBED_CALL(cli_latex_text_length_internal.tex_count, CliLatexTextLengthCtx, .text = text + from,
                    .length = size, .most = 4294967295ull, .value_u64s = &tag))
    {
        return 0;
    }
    const unsigned long long line_end =
        EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = colon, .at = comma + 1ull, .letter = ',');
    from = comma + 1ull;
    size = line_end - from;
    EMBED_CALL(latex_trim, LatexSyncCtx, .text = text, .start = &from, .size = &size);
    if (!EMBED_CALL(cli_latex_text_length_internal.tex_count, CliLatexTextLengthCtx, .text = text + from,
                    .length = size, .most = ~0ull, .value_u64s = &args->record->line))
    {
        return 0;
    }
    args->record->tag = (unsigned int)tag;
    const unsigned long long place = colon + 1ull;
    const unsigned long long place_end =
        EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = length, .at = place, .letter = ':');
    const unsigned long long split =
        EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = place_end, .at = place, .letter = ',');
    if (split == place_end)
    {
        return 0;
    }
    args->record->size[0] = 0ll;
    args->record->size[1] = 0ll;
    args->record->size[2] = 0ll;
    if (place_end < length)
    {
        const unsigned long long dims_end =
            EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = length, .at = place_end + 1ull, .letter = ':');
        unsigned long long value = place_end + 1ull;
        for (unsigned int at = 0u; (at < 3u) && (value <= dims_end); at += 1u)
        {
            const unsigned long long value_end =
                EMBED_CALL(latex_at, LatexSyncCtx, .text = text, .length = dims_end, .at = value, .letter = ',');
            if (!EMBED_CALL(latex_signed_trim, LatexSyncCtx, .text = text, .start_value = value,
                            .length = value_end - value, .value = &args->record->size[at]))
            {
                args->record->size[at] = 0ll;
            }
            value = value_end + 1ull;
        }
    }
    return EMBED_CALL(latex_signed_trim, LatexSyncCtx, .text = text, .start_value = place, .length = split - place,
                      .value = &args->record->x) &&
           EMBED_CALL(latex_signed_trim, LatexSyncCtx, .text = text, .start_value = split + 1ull,
                      .length = place_end - split - 1ull, .value = &args->record->y);
}

// a length in the file's units, `offset` added, in the page's points to the thousandth
EMBED_INLINE double latex_points(const LatexSyncCtx *args)
{
    return round((((double)args->value_value * args->scale->unit) + args->offset) * args->scale->magnification /
                 1000.0 / 65536.0 * 72.0 / 72.27 * 1000.0) /
           1000.0;
}

// the file a tag names that is a file of the tree: its path in the room, or NULL where it names none
EMBED_INLINE const char *latex_file_get(const LatexSyncCtx *args)
{
    for (unsigned int at = 0u; at < args->files; at += 1u)
    {
        if (g_cli_latex->files[at].tag == args->tag)
        {
            return g_cli_latex->files[at].named ? g_cli_latex->names + g_cli_latex->files[at].at : NULL;
        }
    }
    return NULL;
}

// an `Input:tag:name` line's file recorded by its tag, a tag recorded before replaced
EMBED_INLINE void latex_input_store(const LatexSyncCtx *args)
{
    unsigned long long length = args->length;
    const CliTexNames *names = args->names;
    unsigned int *files = args->files_pointer;
    unsigned int *used = args->used;
    const unsigned long long colon =
        EMBED_CALL(latex_at, LatexSyncCtx, .text = args->rest, .length = length, .at = 0ull, .letter = ':');
    unsigned long long tag = 0ull;
    if ((colon == length) ||
        !EMBED_CALL(cli_latex_text_length_internal.tex_count, CliLatexTextLengthCtx, .text = args->rest,
                    .length = colon, .most = 4294967295ull, .value_u64s = &tag))
    {
        return;
    }
    char file[CLI_PATH_MAX_LEN];
    const int named = EMBED_CALL(cli_latex_length_internal.tex_names_get, CliLatexLengthCtx, .names = names,
                                 .name = args->rest + colon + 1ull, .length = length - colon - 1ull, .file = file);
    const size_t size = named ? cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u : 0u;
    unsigned int at = 0u;
    while ((at < *files) && (g_cli_latex->files[at].tag != (unsigned int)tag))
    {
        at += 1u;
    }
    if (((at == *files) && (*files == CLI_LATEX_INPUTS)) || (*used + size > CLI_LATEX_INPUT_TEXT))
    {
        return;
    }
    if (at == *files)
    {
        *files += 1u;
    }
    g_cli_latex->files[at] = (CliTexInput){(unsigned int)tag, named, *used};
    memor.cpy(&(MemoriaCfg){.dst = g_cli_latex->names + *used, .src = file, .bytes = size});
    *used += (unsigned int)size;
}

// a box closed: one written for each file of the tree whose lines it marks, in the order it first marks them, with
// each line where it changes and the point across where its part starts
EMBED_INLINE void latex_box_write(const LatexSyncCtx *args)
{
    int *written = args->written;
    VerbaLinea *out = args->out;
    for (unsigned int mark = args->done->marks; mark < args->marks; mark += 1u)
    {
        const unsigned int tag = g_cli_latex->marks[mark].tag;
        unsigned int before = args->done->marks;
        while ((before < mark) && (g_cli_latex->marks[before].tag != tag))
        {
            before += 1u;
        }
        const char *const file = EMBED_CALL(latex_file_get, LatexSyncCtx, .files = args->files, .tag = tag);
        if ((before < mark) || (file == NULL))
        {
            continue;
        }
        const long long width = args->done->size[0];
        const long long height = args->done->size[1];
        const long long depth = args->done->size[2];
        // the top is the baseline less the height, and the box as tall as its height and depth together, each worked
        // as two's complement does where it would wrap
        const long long top = (long long)((unsigned long long)args->done->y - (unsigned long long)height);
        const long long tall = (long long)((unsigned long long)height + (unsigned long long)depth);
        mmgr_line_put(out, *written ? ",{\"page\":" : "{\"page\":");
        *written = 1;
        mmgr_line_decimal(out, args->page, 1u);
        mmgr_line_put(out, ",\"file\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)file,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"x\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out,
                   .value_f64 = EMBED_CALL(latex_points, LatexSyncCtx, .value_value = args->done->x,
                                           .offset = args->scale->x_offset, .scale = args->scale));
        mmgr_line_put(out, ",\"y\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out,
                   .value_f64 = EMBED_CALL(latex_points, LatexSyncCtx, .value_value = top,
                                           .offset = args->scale->y_offset, .scale = args->scale));
        mmgr_line_put(out, ",\"w\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out,
                   .value_f64 = EMBED_CALL(latex_points, LatexSyncCtx, .value_value = width, .offset = 0.0,
                                           .scale = args->scale));
        mmgr_line_put(out, ",\"h\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out,
                   .value_f64 = EMBED_CALL(latex_points, LatexSyncCtx, .value_value = tall, .offset = 0.0,
                                           .scale = args->scale));
        mmgr_line_put(out, ",\"lines\":[");
        int first = 1;
        unsigned long long last = 0ull;
        for (unsigned int at = mark; at < args->marks; at += 1u)
        {
            const CliTexMark *const read = &g_cli_latex->marks[at];
            if ((read->tag != tag) || (!first && (read->line == last)))
            {
                continue;
            }
            mmgr_line_put(out, first ? "[" : ",[");
            EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out,
                       .value_f64 = EMBED_CALL(latex_points, LatexSyncCtx, .value_value = read->x,
                                               .offset = args->scale->x_offset, .scale = args->scale));
            mmgr_line_ch(out, ',');
            mmgr_line_decimal(out, read->line, 1u);
            mmgr_line_ch(out, ']');
            first = 0;
            last = read->line;
        }
        mmgr_line_put(out, "]}");
    }
}

// a line of the preamble read: the scale it sets, and 1 into `content` where it opens the content
EMBED_INLINE void latex_preamble_read(const LatexSyncCtx *args)
{
    int *content = args->content;
    static const char *const heads[4] = {"Unit:", "Magnification:", "X Offset:", "Y Offset:"};
    double *const values[4] = {&args->scale_out->unit, &args->scale_out->magnification, &args->scale_out->x_offset,
                               &args->scale_out->y_offset};
    for (unsigned int head = 0u; head < 4u; head += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = heads[head], .cap = CLI_CELLS_POOL});
        if ((args->size_value < length) ||
            (memor.cmp(&(MemoriaCfg){.src = args->line, .other = heads[head], .bytes = length}) != 0))
        {
            continue;
        }
        unsigned long long from = length;
        unsigned long long rest = args->size_value - length;
        EMBED_CALL(latex_trim, LatexSyncCtx, .text = args->line, .start = &from, .size = &rest);
        double value = 0.0;
        if (EMBED_CALL(cli_latex_text_length_value_internal.tex_double, CliLatexTextLengthValueCtx,
                       .text = args->line + from, .length = rest, .value = &value))
        {
            *values[head] = value;
            break;
        }
    }
    *content = (args->size_value >= 8ull) &&
               (memor.cmp(&(MemoriaCfg){.src = args->line, .other = "Content:", .bytes = 8u}) == 0);
}

void cli_tex_boxes_write(const CliLatexNamesCtx *args)
{
    CliTexScale scale = {1.0, 1000.0, 0.0, 0.0};
    unsigned long long page = 0ull;
    unsigned int open = 0u;
    unsigned int marks = 0u;
    unsigned int files = 0u;
    unsigned int used = 0u;
    int content = 0;
    int written = 0;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    mmgr_line_ch(args->out, '[');
    while (EMBED_CALL(cli_latex_text_internal.tex_line, CliLatexTextCtx, .text = args->sync, .length = args->size,
                      .at = &at, .start = &start, .size_pointer = &length))
    {
        const char *const line = args->sync + start;
        if ((length >= 6ull) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "Input:", .bytes = 6u}) == 0))
        {
            EMBED_CALL(latex_input_store, LatexSyncCtx, .rest = line + 6ull, .length = length - 6ull,
                       .names = args->names, .files_pointer = &files, .used = &used);
            continue;
        }
        if (!content)
        {
            EMBED_CALL(latex_preamble_read, LatexSyncCtx, .line = line, .size_value = length, .scale_out = &scale,
                       .content = &content);
            continue;
        }
        if (length == 0ull)
        {
            continue;
        }
        const char *const rest = line + 1ull;
        const unsigned long long rest_size = length - 1ull;
        CliTexRecord read;
        switch (line[0])
        {
        case '{': {
            unsigned long long from = 0ull;
            unsigned long long count = rest_size;
            EMBED_CALL(latex_trim, LatexSyncCtx, .text = rest, .start = &from, .size = &count);
            unsigned long long number = 0ull;
            page = EMBED_CALL(cli_latex_text_length_internal.tex_count, CliLatexTextLengthCtx, .text = rest + from,
                              .length = count, .most = ~0ull, .value_u64s = &number)
                       ? number
                       : page + 1ull;
            open = 0u;
            marks = 0u;
            break;
        }
        case '}':
            open = 0u;
            marks = 0u;
            break;
        case '(':
            if ((open < CLI_LATEX_OPEN) &&
                EMBED_CALL(latex_record, LatexSyncCtx, .text = rest, .length = rest_size, .record = &read))
            {
                g_cli_latex->open[open] =
                    (CliTexOpen){read.x, read.y, {read.size[0], read.size[1], read.size[2]}, marks};
                open += 1u;
            }
            break;
        case ')':
            if (open > 0u)
            {
                open -= 1u;
                EMBED_CALL(latex_box_write, LatexSyncCtx, .done = &g_cli_latex->open[open], .marks = marks,
                           .page = page, .files = files, .scale = &scale, .written = &written, .out = args->out);
                marks = g_cli_latex->open[open].marks;
            }
            break;
        case 'x':
        case 'k':
        case 'g':
        case '$':
        case 'h':
        case 'v':
            if ((open > 0u) && (marks < CLI_LATEX_MARKS) &&
                EMBED_CALL(latex_record, LatexSyncCtx, .text = rest, .length = rest_size, .record = &read) &&
                (read.line > 0ull))
            {
                g_cli_latex->marks[marks] = (CliTexMark){read.tag, read.x, read.line};
                marks += 1u;
            }
            break;
        default:
            break;
        }
    }
    mmgr_line_ch(args->out, ']');
}
