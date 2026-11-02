// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_read.c: what a make leaves read: the names the engine writes read as the tree's files, the errors its log
// names, and the pages pdftocairo draws, every id in them made the preview's own
#include "latex_internal.h"

#include "../text/text.h"

typedef struct
{
    const char *text;
    unsigned long long length;
    int plain;
    char *out_room;
    const char *name;
    VerbaLinea *out;
    unsigned long long at;
    const char *needle;
    const char *tag;
    const char *svg;
    unsigned long long size;
    double width;
    double height;
    int first;
} LatexReadCtx;

// `length` bytes of a path, trimmed, written with forward slashes and `/./` as `/` into `out`, sized for
// CLI_PATH_MAX_LEN; and, where `plain`, with no `./` before it and in lower case on Windows, where case does not
// distinguish two files apart
static void latex_slash(const LatexReadCtx *args)
{
    const char *text = args->text;
    unsigned long long start = 0ull;
    unsigned long long end = args->length;
    while ((start < end) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = text[start]))
    {
        start += 1ull;
    }
    while ((end > start) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = text[end - 1ull]))
    {
        end -= 1ull;
    }
    VerbaLinea line = {args->out_room, CLI_PATH_MAX_LEN, 0ull};
    unsigned long long at = start;
    while (at < end)
    {
        const int slash = (text[at] == '/') || (text[at] == '\\');
        if (slash && (at + 2ull < end) && (text[at + 1ull] == '.') &&
            ((text[at + 2ull] == '/') || (text[at + 2ull] == '\\')))
        {
            mmgr_line_ch(&line, '/');
            at += 3ull;
            continue;
        }
        mmgr_line_ch(&line, slash ? '/' : text[at]);
        at += 1ull;
    }
    mmgr_line_finish(&line);
    args->out_room[CLI_PATH_MAX_LEN - 1u] = '\0';
    if (!args->plain)
    {
        return;
    }
    if ((args->out_room[0] == '.') && (args->out_room[1] == '/'))
    {
        memor.move_down(&(MemoriaCfg){
            .dst = args->out_room,
            .src = args->out_room + 2,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->out_room + 2, .cap = CLI_CELLS_POOL}) + 1u});
    }
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out_room,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->out_room, .cap = CLI_CELLS_POOL}));
#endif
}

void cli_tex_names_init(const CliLatexNamesRootCtx *args)
{
    const char *path = args->path;
    char named[CLI_PATH_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_put(&line, args->job);
    mmgr_line_put(&line, ".tex");
    mmgr_line_finish(&line);
    EMBED_CALL(latex_slash, LatexReadCtx, .text = named,
               .length = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), .plain = 1,
               .out_room = args->names_out->job);
    char made[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->dir, .relative = named, .joined = made);
    EMBED_CALL(latex_slash, LatexReadCtx, .text = made,
               .length = cellul.len(&(CatenaFinitaCfg){.src = made, .cap = CLI_CELLS_POOL}), .plain = 1,
               .out_room = args->names_out->made);
    EMBED_CALL(latex_slash, LatexReadCtx, .text = args->root,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}), .plain = 1,
               .out_room = args->names_out->root);
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->names_out->root, .cap = CLI_CELLS_POOL});
    if (((size == 0u) || (args->names_out->root[size - 1u] != '/')) && (size + 1u < CLI_PATH_MAX_LEN))
    {
        args->names_out->root[size] = '/';
        args->names_out->root[size + 1u] = '\0';
    }
    args->names_out->path = path;
}

int cli_tex_names_get(const CliLatexLengthCtx *args)
{
    char given[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_slash, LatexReadCtx, .text = args->name, .length = args->length, .plain = 1, .out_room = given);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = given, .other = args->names->job, .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = args->names->made, .cap = CLI_CELLS_POOL})))
    {
        VerbaLinea line = {args->file, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, args->names->path);
        mmgr_line_finish(&line);
        return 1;
    }
    const size_t root = cellul.len(&(CatenaFinitaCfg){.src = args->names->root, .cap = CLI_CELLS_POOL});
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = given, .other_pointer = args->names->root,
                    .size = root))
    {
        return 0;
    }
    char start[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_slash, LatexReadCtx, .text = args->name, .length = args->length, .plain = 0, .out_room = start);
    // the rest is none where the root's length falls past the name or inside one of its letters
    if ((cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL}) < root) ||
        (((unsigned char)start[root] & 0xC0u) == 0x80u))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->file,
                            .src = start + root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = start + root, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// `length` bytes of a name the log gives written as a JSON string with forward slashes, where it stands for no file
// of the tree
EMBED_INLINE void latex_name_write(const LatexReadCtx *args)
{
    unsigned long long length = args->length;
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '"');
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at <= length; at += 1ull)
    {
        if ((at == length) || (args->name[at] == '\\'))
        {
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)args->name + from, .length = at - from);
            if (at < length)
            {
                mmgr_line_ch(out, '/');
            }
            from = at + 1ull;
        }
    }
    mmgr_line_ch(out, '"');
}

unsigned int cli_tex_errors_write(const CliLatexNamesLogCtx *args)
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    while ((count < CLI_LATEX_ERRORS) &&
           EMBED_CALL(cli_latex_text_internal.tex_line, CliLatexTextCtx, .text = args->log, .length = args->size,
                      .at = &at, .start = &start, .size_pointer = &length))
    {
        const char *const line = args->log + start;
        unsigned long long colon = 1ull;
        unsigned long long end = 0ull;
        for (; colon < length; colon += 1ull)
        {
            if (line[colon] != ':')
            {
                continue;
            }
            const unsigned long long digits =
                EMBED_CALL(cli_latex_text_length_at_internal.tex_digits, CliLatexTextLengthAtCtx, .text = line,
                           .length = length, .at_value = colon + 1ull);
            end = colon + 1ull + digits;
            if ((digits > 0ull) && (end + 1ull < length) && (line[end] == ':') && (line[end + 1ull] == ' '))
            {
                break;
            }
        }
        if (colon >= length)
        {
            continue;
        }
        unsigned long long first = end + 2ull;
        unsigned long long last = length;
        while ((first < last) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = line[first]))
        {
            first += 1ull;
        }
        while ((last > first) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = line[last - 1ull]))
        {
            last -= 1ull;
        }
        const unsigned long long text = last - first;
        if ((text == 0ull) ||
            ((text >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = line + first, .other = "==>", .bytes = 3u}) == 0)) ||
            ((text >= 14ull) &&
             (memor.cmp(&(MemoriaCfg){.src = line + first, .other = "Emergency stop", .bytes = 14u}) == 0)))
        {
            continue;
        }
        if (count > 0u)
        {
            mmgr_line_ch(args->out, ',');
        }
        mmgr_line_put(args->out, "{\"file\":");
        char file[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(cli_latex_length_internal.tex_names_get, CliLatexLengthCtx, .names = args->names, .name = line,
                       .length = colon, .file = file))
        {
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)file,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            EMBED_CALL(latex_name_write, LatexReadCtx, .name = line, .length = colon, .out = args->out);
        }
        mmgr_line_put(args->out, ",\"line\":");
        unsigned long long number = 0ull;
        if (EMBED_CALL(cli_latex_text_length_internal.tex_count, CliLatexTextLengthCtx, .text = line + colon + 1ull,
                       .length = end - colon - 1ull, .most = ~0ull, .value_u64s = &number))
        {
            mmgr_line_decimal(args->out, number, 1u);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_put(args->out, ",\"text\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)line + first, .length = text);
        mmgr_line_ch(args->out, '}');
        count += 1u;
    }
    return count;
}

// where `needle` is first found in `length` bytes of `text` from `at`, `length` where it is not
EMBED_INLINE unsigned long long latex_find(const LatexReadCtx *args)
{
    unsigned long long at = args->at;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->needle, .cap = CLI_CELLS_POOL});
    for (; at + size <= args->length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = args->needle, .bytes = size}) == 0)
        {
            return at;
        }
    }
    return args->length;
}

// the number a tag gives `name` as, ` name="595.276pt"`, its unit's letters taken off: 0 where it gives none
EMBED_INLINE double latex_number_get(const LatexReadCtx *args)
{
    unsigned long long length = args->length;
    char sought[64];
    VerbaLinea line = {sought, sizeof(sought), 0ull};
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, args->name);
    mmgr_line_put(&line, "=\"");
    mmgr_line_finish(&line);
    const unsigned long long at =
        EMBED_CALL(latex_find, LatexReadCtx, .text = args->tag, .length = length, .at = 0ull, .needle = sought);
    if (at == length)
    {
        return 0.0;
    }
    const unsigned long long from = at + cellul.len(&(CatenaFinitaCfg){.src = sought, .cap = CLI_CELLS_POOL});
    const unsigned long long piece =
        EMBED_CALL(latex_find, LatexReadCtx, .text = args->tag, .length = length, .at = from, .needle = sought);
    const char *const quote = memor.chr(&(MemoriaCfg){.src = args->tag + from, .val = '"', .bytes = piece - from});
    unsigned long long end = (quote != NULL) ? (unsigned long long)(quote - args->tag) : piece;
    end = from + EMBED_CALL(cli_text_at_size_classes.back_past_class, CliTextAtSizeClassesCfg, .text = args->tag + from,
                            .at = end - from, .classes = CLI_TEXT_LETTERS, .set = "");
    double value = 0.0;
    return EMBED_CALL(cli_latex_text_length_value_internal.tex_double, CliLatexTextLengthValueCtx,
                      .text = args->tag + from, .length = end - from, .value = &value)
               ? value
               : 0.0;
}

// the SVG with ` id="`, `href="#` and `url(#` each written with `tex-` after it into the room: its size, or more than
// the room fits where it does not fit
EMBED_INLINE unsigned long long latex_ids_new_own(const LatexReadCtx *args)
{
    static const char *const marks[3] = {" id=\"", "href=\"#", "url(#"};
    char *const room = g_cli_latex->svg;
    unsigned long long used = 0ull;
    unsigned long long at = 0ull;
    while (at < args->size)
    {
        unsigned long long taken = 1ull;
        int marked = 0;
        for (unsigned int mark = 0u; mark < 3u; mark += 1u)
        {
            const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = marks[mark], .cap = CLI_CELLS_POOL});
            if ((at + length <= args->size) &&
                (memor.cmp(&(MemoriaCfg){.src = args->svg + at, .other = marks[mark], .bytes = length}) == 0))
            {
                taken = length;
                marked = 1;
                break;
            }
        }
        if (used + taken + (marked ? 4ull : 0ull) > CLI_LATEX_SVG)
        {
            return CLI_LATEX_SVG + 1ull;
        }
        memor.cpy(&(MemoriaCfg){.dst = room + used, .src = args->svg + at, .bytes = taken});
        used += taken;
        if (marked)
        {
            memor.cpy(&(MemoriaCfg){.dst = room + used, .src = "tex-", .bytes = 4u});
            used += 4ull;
        }
        at += taken;
    }
    return used;
}

// a page written as `{"width", "height", "svg"}`, a comma before it where it is not the first
EMBED_INLINE void latex_page_write(const LatexReadCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, args->first ? "{\"width\":" : ",{\"width\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out, .value_f64 = args->width);
    mmgr_line_put(out, ",\"height\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = out, .value_f64 = args->height);
    mmgr_line_put(out, ",\"svg\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->svg,
               .length = args->size);
    mmgr_line_ch(out, '}');
}

int cli_tex_pages_write(const CliLatexSizeCtx *args)
{
    const unsigned long long length = EMBED_CALL(latex_ids_new_own, LatexReadCtx, .svg = args->svg, .size = args->size);
    if (length > CLI_LATEX_SVG)
    {
        return 0;
    }
    char *const text = g_cli_latex->svg;
    const unsigned long long start =
        EMBED_CALL(latex_find, LatexReadCtx, .text = text, .length = length, .at = 0ull, .needle = "<svg");
    if (start == length)
    {
        mmgr_line_put(args->out, "\"defs\":\"\",\"pages\":[]");
        return 1;
    }
    const char *const closed = memor.chr(&(MemoriaCfg){.src = text + start, .val = '>', .bytes = length - start});
    const unsigned long long open_end = (closed != NULL) ? (unsigned long long)(closed - text) + 1ull : length;
    const double width =
        EMBED_CALL(latex_number_get, LatexReadCtx, .tag = text + start, .length = open_end - start, .name = "width");
    const double height =
        EMBED_CALL(latex_number_get, LatexReadCtx, .tag = text + start, .length = open_end - start, .name = "height");
    unsigned long long body_end = length;
    for (unsigned long long at = length; at >= 6ull; at -= 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at - 6ull, .other = "</svg>", .bytes = 6u}) == 0)
        {
            body_end = at - 6ull;
            break;
        }
    }
    body_end = (body_end > open_end) ? body_end : open_end;
    char *const body = text + open_end;
    unsigned long long body_size = body_end - open_end;
    const unsigned long long from =
        EMBED_CALL(latex_find, LatexReadCtx, .text = body, .length = body_size, .at = 0ull, .needle = "<defs>");
    const unsigned long long to =
        EMBED_CALL(latex_find, LatexReadCtx, .text = body, .length = body_size, .at = 0ull, .needle = "</defs>");
    mmgr_line_put(args->out, "\"defs\":");
    if ((from < body_size) && (to < body_size) && (from < to))
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = body + from + 6ull, .length = to - from - 6ull);
        // what is left of the body is what stands before the defs and after them, joined where they stood
        memor.move_down(&(MemoriaCfg){.dst = body + from, .src = body + to + 7ull, .bytes = body_size - to - 7ull});
        body_size -= (to + 7ull) - from;
    }
    else
    {
        mmgr_line_put(args->out, "\"\"");
    }
    mmgr_line_put(args->out, ",\"pages\":[");
    if (EMBED_CALL(latex_find, LatexReadCtx, .text = body, .length = body_size, .at = 0ull, .needle = "<pageSet>") ==
        body_size)
    {
        unsigned long long first = 0ull;
        unsigned long long last = body_size;
        while ((first < last) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = body[first]))
        {
            first += 1ull;
        }
        while ((last > first) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = body[last - 1ull]))
        {
            last -= 1ull;
        }
        EMBED_CALL(latex_page_write, LatexReadCtx, .width = width, .height = height, .svg = body + first,
                   .size = last - first, .first = 1, .out = args->out);
        mmgr_line_ch(args->out, ']');
        return 1;
    }
    int first = 1;
    unsigned long long at =
        EMBED_CALL(latex_find, LatexReadCtx, .text = body, .length = body_size, .at = 0ull, .needle = "<page>");
    while (at < body_size)
    {
        const unsigned long long part = at + 6ull;
        const unsigned long long next =
            EMBED_CALL(latex_find, LatexReadCtx, .text = body, .length = body_size, .at = part, .needle = "<page>");
        const unsigned long long ended =
            EMBED_CALL(latex_find, LatexReadCtx, .text = body, .length = next, .at = part, .needle = "</page>");
        EMBED_CALL(latex_page_write, LatexReadCtx, .width = width, .height = height, .svg = body + part,
                   .size = ended - part, .first = first, .out = args->out);
        first = 0;
        at = next;
    }
    mmgr_line_ch(args->out, ']');
    return 1;
}
