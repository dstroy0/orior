// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_read.c: what a make leaves read: the names the engine writes read as the tree's files, the errors its log
// names, and the pages pdftocairo draws, every id in them made the preview's own
#include "latex_internal.h"

#include "../text/text.h"

// `length` bytes of a path, trimmed, written with forward slashes and `/./` as `/` into `out`, which holds
// CLI_PATH_LONGEST; and, where `plain`, with no `./` before it and in lower case on Windows, where case does not tell
// two files apart
static void latex_slashed(const char *text, unsigned long long length, int plain, char *out)
{
    unsigned long long start = 0ull;
    unsigned long long end = length;
    while ((start < end) && cli_tex_white(text[start]))
    {
        start += 1ull;
    }
    while ((end > start) && cli_tex_white(text[end - 1ull]))
    {
        end -= 1ull;
    }
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    unsigned long long at = start;
    while (at < end)
    {
        const int slash = (text[at] == '/') || (text[at] == '\\');
        if (slash && (at + 2ull < end) && (text[at + 1ull] == '.') &&
            ((text[at + 2ull] == '/') || (text[at + 2ull] == '\\')))
        {
            cli_line_character(&line, '/');
            at += 3ull;
            continue;
        }
        cli_line_character(&line, slash ? '/' : text[at]);
        at += 1ull;
    }
    cli_line_closed(&line);
    out[CLI_PATH_LONGEST - 1u] = '\0';
    if (!plain)
    {
        return;
    }
    if ((out[0] == '.') && (out[1] == '/'))
    {
        memor.move_down(
            &(MemoriaCfg){.dst = out,
                          .src = out + 2,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = out + 2, .cap = CLI_CELLS_POOL}) + 1u});
    }
#if defined(_WIN32)
    for (char *letter = out; *letter != '\0'; letter += 1)
    {
        if ((*letter >= 'A') && (*letter <= 'Z'))
        {
            *letter = (char)(*letter - 'A' + 'a');
        }
    }
#endif
}

void cli_tex_names_made(CliTexNames *names, const char *root, const char *dir, const char *job, const char *path)
{
    char named[CLI_PATH_LONGEST];
    CliLine line = {named, sizeof(named), 0ull};
    cli_line_text(&line, job);
    cli_line_text(&line, ".tex");
    cli_line_closed(&line);
    latex_slashed(named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), 1, names->job);
    char made[CLI_PATH_LONGEST];
    cli_full(dir, named, made);
    latex_slashed(made, cellul.len(&(CatenaFinitaCfg){.src = made, .cap = CLI_CELLS_POOL}), 1, names->made);
    latex_slashed(root, cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}), 1, names->root);
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = names->root, .cap = CLI_CELLS_POOL});
    if (((size == 0u) || (names->root[size - 1u] != '/')) && (size + 1u < CLI_PATH_LONGEST))
    {
        names->root[size] = '/';
        names->root[size + 1u] = '\0';
    }
    names->path = path;
}

int cli_tex_names_of(const CliTexNames *names, const char *name, unsigned long long length, char *file)
{
    char given[CLI_PATH_LONGEST];
    latex_slashed(name, length, 1, given);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = given, .other = names->job, .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = names->made, .cap = CLI_CELLS_POOL})))
    {
        CliLine line = {file, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, names->path);
        cli_line_closed(&line);
        return 1;
    }
    const size_t root = cellul.len(&(CatenaFinitaCfg){.src = names->root, .cap = CLI_CELLS_POOL});
    if (!cli_text_same(given, names->root, root))
    {
        return 0;
    }
    char start[CLI_PATH_LONGEST];
    latex_slashed(name, length, 0, start);
    // the rest is none where the root's length falls past the name or inside one of its letters
    if ((cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL}) < root) ||
        (((unsigned char)start[root] & 0xC0u) == 0x80u))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = file,
                            .src = start + root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = start + root, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// `length` bytes of a name the log gives written as a JSON string with forward slashes, where it stands for no file
// of the tree
static void latex_name_written(const char *name, unsigned long long length, CliLine *out)
{
    cli_line_character(out, '"');
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at <= length; at += 1ull)
    {
        if ((at == length) || (name[at] == '\\'))
        {
            cli_json_escaped_lossy(out, (const unsigned char *)name + from, at - from);
            if (at < length)
            {
                cli_line_character(out, '/');
            }
            from = at + 1ull;
        }
    }
    cli_line_character(out, '"');
}

unsigned int cli_tex_errors_written(const char *log, unsigned long long size, const CliTexNames *names, CliLine *out)
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    while ((count < CLI_LATEX_ERRORS) && cli_tex_line(log, size, &at, &start, &length))
    {
        const char *const line = log + start;
        unsigned long long colon = 1ull;
        unsigned long long end = 0ull;
        for (; colon < length; colon += 1ull)
        {
            if (line[colon] != ':')
            {
                continue;
            }
            const unsigned long long digits = cli_tex_digits(line, length, colon + 1ull);
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
        while ((first < last) && cli_tex_white(line[first]))
        {
            first += 1ull;
        }
        while ((last > first) && cli_tex_white(line[last - 1ull]))
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
            cli_line_character(out, ',');
        }
        cli_line_text(out, "{\"file\":");
        char file[CLI_PATH_LONGEST];
        if (cli_tex_names_of(names, line, colon, file))
        {
            cli_json_quoted_lossy(out, (const unsigned char *)file,
                                  cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            latex_name_written(line, colon, out);
        }
        cli_line_text(out, ",\"line\":");
        unsigned long long number = 0ull;
        if (cli_tex_count(line + colon + 1ull, end - colon - 1ull, ~0ull, &number))
        {
            cli_line_decimal(out, number, 1u);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_text(out, ",\"text\":");
        cli_json_quoted_lossy(out, (const unsigned char *)line + first, text);
        cli_line_character(out, '}');
        count += 1u;
    }
    return count;
}

// where `needle` is first found in `length` bytes of `text` from `at`, `length` where it is not
static unsigned long long latex_found(const char *text, unsigned long long length, unsigned long long at,
                                      const char *needle)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = needle, .cap = CLI_CELLS_POOL});
    for (; at + size <= length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at, .other = needle, .bytes = size}) == 0)
        {
            return at;
        }
    }
    return length;
}

// the number a tag gives `name` as, ` name="595.276pt"`, its unit's letters taken off: 0 where it gives none
static double latex_number_of(const char *tag, unsigned long long length, const char *name)
{
    char sought[64];
    CliLine line = {sought, sizeof(sought), 0ull};
    cli_line_character(&line, ' ');
    cli_line_text(&line, name);
    cli_line_text(&line, "=\"");
    cli_line_closed(&line);
    const unsigned long long at = latex_found(tag, length, 0ull, sought);
    if (at == length)
    {
        return 0.0;
    }
    const unsigned long long from = at + cellul.len(&(CatenaFinitaCfg){.src = sought, .cap = CLI_CELLS_POOL});
    const unsigned long long piece = latex_found(tag, length, from, sought);
    const char *const quote = memor.chr(&(MemoriaCfg){.src = tag + from, .val = '"', .bytes = piece - from});
    unsigned long long end = (quote != NULL) ? (unsigned long long)(quote - tag) : piece;
    while ((end > from) && (((tag[end - 1ull] >= 'a') && (tag[end - 1ull] <= 'z')) ||
                            ((tag[end - 1ull] >= 'A') && (tag[end - 1ull] <= 'Z'))))
    {
        end -= 1ull;
    }
    double value = 0.0;
    return cli_tex_double(tag + from, end - from, &value) ? value : 0.0;
}

// the SVG with ` id="`, `href="#` and `url(#` each written with `tex-` after it into the room: its size, or more than
// the room holds where it does not fit
static unsigned long long latex_ids_made_own(const char *svg, unsigned long long size)
{
    static const char *const marks[3] = {" id=\"", "href=\"#", "url(#"};
    char *const room = g_cli_latex->svg;
    unsigned long long used = 0ull;
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned long long taken = 1ull;
        int marked = 0;
        for (unsigned int mark = 0u; mark < 3u; mark += 1u)
        {
            const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = marks[mark], .cap = CLI_CELLS_POOL});
            if ((at + length <= size) &&
                (memor.cmp(&(MemoriaCfg){.src = svg + at, .other = marks[mark], .bytes = length}) == 0))
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
        memor.cpy(&(MemoriaCfg){.dst = room + used, .src = svg + at, .bytes = taken});
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
static void latex_page_written(double width, double height, const char *svg, unsigned long long size, int first,
                               CliLine *out)
{
    cli_line_text(out, first ? "{\"width\":" : ",{\"width\":");
    cli_json_double(out, width);
    cli_line_text(out, ",\"height\":");
    cli_json_double(out, height);
    cli_line_text(out, ",\"svg\":");
    cli_json_quoted(out, svg, size);
    cli_line_character(out, '}');
}

int cli_tex_pages_written(const char *svg, unsigned long long size, CliLine *out)
{
    const unsigned long long length = latex_ids_made_own(svg, size);
    if (length > CLI_LATEX_SVG)
    {
        return 0;
    }
    char *const text = g_cli_latex->svg;
    const unsigned long long start = latex_found(text, length, 0ull, "<svg");
    if (start == length)
    {
        cli_line_text(out, "\"defs\":\"\",\"pages\":[]");
        return 1;
    }
    const char *const closed = memor.chr(&(MemoriaCfg){.src = text + start, .val = '>', .bytes = length - start});
    const unsigned long long open_end = (closed != NULL) ? (unsigned long long)(closed - text) + 1ull : length;
    const double width = latex_number_of(text + start, open_end - start, "width");
    const double height = latex_number_of(text + start, open_end - start, "height");
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
    const unsigned long long from = latex_found(body, body_size, 0ull, "<defs>");
    const unsigned long long to = latex_found(body, body_size, 0ull, "</defs>");
    cli_line_text(out, "\"defs\":");
    if ((from < body_size) && (to < body_size) && (from < to))
    {
        cli_json_quoted(out, body + from + 6ull, to - from - 6ull);
        // what is left of the body is what stands before the defs and after them, joined where they stood
        memor.move_down(&(MemoriaCfg){.dst = body + from, .src = body + to + 7ull, .bytes = body_size - to - 7ull});
        body_size -= (to + 7ull) - from;
    }
    else
    {
        cli_line_text(out, "\"\"");
    }
    cli_line_text(out, ",\"pages\":[");
    if (latex_found(body, body_size, 0ull, "<pageSet>") == body_size)
    {
        unsigned long long first = 0ull;
        unsigned long long last = body_size;
        while ((first < last) && cli_tex_white(body[first]))
        {
            first += 1ull;
        }
        while ((last > first) && cli_tex_white(body[last - 1ull]))
        {
            last -= 1ull;
        }
        latex_page_written(width, height, body + first, last - first, 1, out);
        cli_line_character(out, ']');
        return 1;
    }
    int first = 1;
    unsigned long long at = latex_found(body, body_size, 0ull, "<page>");
    while (at < body_size)
    {
        const unsigned long long part = at + 6ull;
        const unsigned long long next = latex_found(body, body_size, part, "<page>");
        const unsigned long long ended = latex_found(body, next, part, "</page>");
        latex_page_written(width, height, body + part, ended - part, first, out);
        first = 0;
        at = next;
    }
    cli_line_character(out, ']');
    return 1;
}
