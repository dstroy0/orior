// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_sync.c: the boxes of text a SyncTeX file says each page holds, each with the lines of a file of the tree that
// set it, in the page's own points from its top left
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

// `length` bytes from `start` of `text` with the ASCII whitespace about them taken off, as their start and size
static void latex_trimmed(const char *text, unsigned long long *start, unsigned long long *size)
{
    while ((*size > 0ull) && cli_tex_white(text[*start]))
    {
        *start += 1ull;
        *size -= 1ull;
    }
    while ((*size > 0ull) && cli_tex_white(text[*start + *size - 1ull]))
    {
        *size -= 1ull;
    }
}

// where `letter` is first in `length` bytes of `text` from `at`, `length` where it is not
static unsigned long long latex_at(const char *text, unsigned long long length, unsigned long long at, char letter)
{
    const char *const found =
        (at < length) ? memor.chr(&(MemoriaCfg){.src = text + at, .val = letter, .bytes = length - at}) : NULL;
    return (found != NULL) ? (unsigned long long)(found - text) : length;
}

// `length` bytes from `start` of `text` read trimmed as a signed number into `value`: 1, or 0 where they read as none
static int latex_signed_trimmed(const char *text, unsigned long long start, unsigned long long length, long long *value)
{
    unsigned long long from = start;
    unsigned long long size = length;
    latex_trimmed(text, &from, &size);
    return cli_tex_signed(text + from, size, value);
}

// a record, `tag,line:x,y:width,height,depth` after its kind, read into `record`: 1, or 0 where it is not one
static int latex_record(const char *text, unsigned long long length, CliTexRecord *record)
{
    const unsigned long long colon = latex_at(text, length, 0ull, ':');
    if (colon == length)
    {
        return 0;
    }
    const unsigned long long comma = latex_at(text, colon, 0ull, ',');
    if (comma == colon)
    {
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long size = comma;
    latex_trimmed(text, &from, &size);
    unsigned long long tag = 0ull;
    if (!cli_tex_count(text + from, size, 4294967295ull, &tag))
    {
        return 0;
    }
    const unsigned long long line_end = latex_at(text, colon, comma + 1ull, ',');
    from = comma + 1ull;
    size = line_end - from;
    latex_trimmed(text, &from, &size);
    if (!cli_tex_count(text + from, size, ~0ull, &record->line))
    {
        return 0;
    }
    record->tag = (unsigned int)tag;
    const unsigned long long place = colon + 1ull;
    const unsigned long long place_end = latex_at(text, length, place, ':');
    const unsigned long long split = latex_at(text, place_end, place, ',');
    if (split == place_end)
    {
        return 0;
    }
    record->size[0] = 0ll;
    record->size[1] = 0ll;
    record->size[2] = 0ll;
    if (place_end < length)
    {
        const unsigned long long dims_end = latex_at(text, length, place_end + 1ull, ':');
        unsigned long long value = place_end + 1ull;
        for (unsigned int slot = 0u; (slot < 3u) && (value <= dims_end); slot += 1u)
        {
            const unsigned long long value_end = latex_at(text, dims_end, value, ',');
            if (!latex_signed_trimmed(text, value, value_end - value, &record->size[slot]))
            {
                record->size[slot] = 0ll;
            }
            value = value_end + 1ull;
        }
    }
    return latex_signed_trimmed(text, place, split - place, &record->x) &&
           latex_signed_trimmed(text, split + 1ull, place_end - split - 1ull, &record->y);
}

// a length in the file's units, `offset` added, in the page's points to the thousandth
static double latex_points(long long value, double offset, const CliTexScale *scale)
{
    return round((((double)value * scale->unit) + offset) * scale->magnification / 1000.0 / 65536.0 * 72.0 / 72.27 *
                 1000.0) /
           1000.0;
}

// the file a tag names that is a file of the tree: its path in the room, or NULL where it names none
static const char *latex_file_of(unsigned int files, unsigned int tag)
{
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        if (g_cli_latex->files[at].tag == tag)
        {
            return g_cli_latex->files[at].named ? g_cli_latex->names + g_cli_latex->files[at].at : NULL;
        }
    }
    return NULL;
}

// an `Input:tag:name` line's file kept by its tag, a tag kept before replaced
static void latex_input_kept(const char *rest, unsigned long long length, const CliTexNames *names, unsigned int *files,
                             unsigned int *used)
{
    const unsigned long long colon = latex_at(rest, length, 0ull, ':');
    unsigned long long tag = 0ull;
    if ((colon == length) || !cli_tex_count(rest, colon, 4294967295ull, &tag))
    {
        return;
    }
    char file[CLI_PATH_LONGEST];
    const int named = cli_tex_names_of(names, rest + colon + 1ull, length - colon - 1ull, file);
    const size_t size = named ? cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u : 0u;
    unsigned int slot = 0u;
    while ((slot < *files) && (g_cli_latex->files[slot].tag != (unsigned int)tag))
    {
        slot += 1u;
    }
    if (((slot == *files) && (*files == CLI_LATEX_INPUTS)) || (*used + size > CLI_LATEX_INPUT_TEXT))
    {
        return;
    }
    if (slot == *files)
    {
        *files += 1u;
    }
    g_cli_latex->files[slot] = (CliTexInput){(unsigned int)tag, named, *used};
    memor.cpy(&(MemoriaCfg){.dst = g_cli_latex->names + *used, .src = file, .bytes = size});
    *used += (unsigned int)size;
}

// a box closed: one written for each file of the tree whose lines it marks, in the order it first marks them, with
// each line where it changes and the point across where its part starts
static void latex_box_written(const CliTexOpen *done, unsigned int marks, unsigned long long page, unsigned int files,
                              const CliTexScale *scale, int *written, CliLine *out)
{
    for (unsigned int mark = done->marks; mark < marks; mark += 1u)
    {
        const unsigned int tag = g_cli_latex->marks[mark].tag;
        unsigned int before = done->marks;
        while ((before < mark) && (g_cli_latex->marks[before].tag != tag))
        {
            before += 1u;
        }
        const char *const file = latex_file_of(files, tag);
        if ((before < mark) || (file == NULL))
        {
            continue;
        }
        const long long width = done->size[0];
        const long long height = done->size[1];
        const long long depth = done->size[2];
        // the top is the baseline less the height, and the box as tall as its height and depth together, each worked
        // as two's complement does where it would wrap
        const long long top = (long long)((unsigned long long)done->y - (unsigned long long)height);
        const long long tall = (long long)((unsigned long long)height + (unsigned long long)depth);
        cli_line_text(out, *written ? ",{\"page\":" : "{\"page\":");
        *written = 1;
        cli_line_decimal(out, page, 1u);
        cli_line_text(out, ",\"file\":");
        cli_json_quoted_lossy(out, (const unsigned char *)file,
                              cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"x\":");
        cli_json_double(out, latex_points(done->x, scale->x_offset, scale));
        cli_line_text(out, ",\"y\":");
        cli_json_double(out, latex_points(top, scale->y_offset, scale));
        cli_line_text(out, ",\"w\":");
        cli_json_double(out, latex_points(width, 0.0, scale));
        cli_line_text(out, ",\"h\":");
        cli_json_double(out, latex_points(tall, 0.0, scale));
        cli_line_text(out, ",\"lines\":[");
        int first = 1;
        unsigned long long last = 0ull;
        for (unsigned int at = mark; at < marks; at += 1u)
        {
            const CliTexMark *const read = &g_cli_latex->marks[at];
            if ((read->tag != tag) || (!first && (read->line == last)))
            {
                continue;
            }
            cli_line_text(out, first ? "[" : ",[");
            cli_json_double(out, latex_points(read->x, scale->x_offset, scale));
            cli_line_character(out, ',');
            cli_line_decimal(out, read->line, 1u);
            cli_line_character(out, ']');
            first = 0;
            last = read->line;
        }
        cli_line_text(out, "]}");
    }
}

// a line of the preamble read: the scale it sets, and 1 into `content` where it opens the content
static void latex_preamble_read(const char *line, unsigned long long size, CliTexScale *scale, int *content)
{
    static const char *const heads[4] = {"Unit:", "Magnification:", "X Offset:", "Y Offset:"};
    double *const values[4] = {&scale->unit, &scale->magnification, &scale->x_offset, &scale->y_offset};
    for (unsigned int head = 0u; head < 4u; head += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = heads[head], .cap = CLI_CELLS_POOL});
        if ((size < length) || (memor.cmp(&(MemoriaCfg){.src = line, .other = heads[head], .bytes = length}) != 0))
        {
            continue;
        }
        unsigned long long from = length;
        unsigned long long rest = size - length;
        latex_trimmed(line, &from, &rest);
        double value = 0.0;
        if (cli_tex_double(line + from, rest, &value))
        {
            *values[head] = value;
            break;
        }
    }
    *content = (size >= 8ull) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "Content:", .bytes = 8u}) == 0);
}

void cli_tex_boxes_written(const char *sync, unsigned long long size, const CliTexNames *names, CliLine *out)
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
    cli_line_character(out, '[');
    while (cli_tex_line(sync, size, &at, &start, &length))
    {
        const char *const line = sync + start;
        if ((length >= 6ull) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "Input:", .bytes = 6u}) == 0))
        {
            latex_input_kept(line + 6ull, length - 6ull, names, &files, &used);
            continue;
        }
        if (!content)
        {
            latex_preamble_read(line, length, &scale, &content);
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
            latex_trimmed(rest, &from, &count);
            unsigned long long number = 0ull;
            page = cli_tex_count(rest + from, count, ~0ull, &number) ? number : page + 1ull;
            open = 0u;
            marks = 0u;
            break;
        }
        case '}':
            open = 0u;
            marks = 0u;
            break;
        case '(':
            if ((open < CLI_LATEX_OPEN) && latex_record(rest, rest_size, &read))
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
                latex_box_written(&g_cli_latex->open[open], marks, page, files, &scale, &written, out);
                marks = g_cli_latex->open[open].marks;
            }
            break;
        case 'x':
        case 'k':
        case 'g':
        case '$':
        case 'h':
        case 'v':
            if ((open > 0u) && (marks < CLI_LATEX_MARKS) && latex_record(rest, rest_size, &read) && (read.line > 0ull))
            {
                g_cli_latex->marks[marks] = (CliTexMark){read.tag, read.x, read.line};
                marks += 1u;
            }
            break;
        default:
            break;
        }
    }
    cli_line_character(out, ']');
}
