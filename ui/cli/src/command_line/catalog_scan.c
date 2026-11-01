// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_scan.c: a path joined and a path's stem, and what a file declares of itself: its opening paragraph, the names
// a case accepts, the parts a driver's usage names, and a README's first paragraph
#include "../command_line/cells.h"
#include "../text/text.h"
#include "catalog_internal.h"

void cli_catalog_join(const char *folder, const char *name, char *path)
{
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_finish(&line);
}

void cli_catalog_stem(const char *path, char *out)
{
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    name = path + cli_text_back_any(path, (unsigned long long)(name - path), "/\\");
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const unsigned long long length = ((dot != NULL) && (dot != name))
                                          ? (unsigned long long)(dot - name)
                                          : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    VerbaLinea line = {out, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put_n(&line, name, length);
    mmgr_line_finish(&line);
}

// A line of a text: where it starts and its length, its `\r` before `\n` left off
typedef struct
{
    const char *at;
    unsigned long long length;
} CatalogTextLine;

// the line from `*at` before `end`, `*at` moved past it: 1, or 0 at the end
static int cli_line(const char **at, const char *end, CatalogTextLine *line)
{
    if (*at >= end)
    {
        return 0;
    }
    const char *stop = *at;
    stop += cli_text_to_any(stop, 0ull, (unsigned long long)(end - stop), "\n");
    line->at = *at;
    line->length = (unsigned long long)(stop - *at);
    line->length -= ((line->length != 0ull) && (line->at[line->length - 1u] == '\r')) ? 1ull : 0ull;
    *at = (stop < end) ? (stop + 1) : stop;
    return 1;
}

static int cli_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

static CatalogTextLine cli_trimmed(CatalogTextLine line)
{
    while ((line.length != 0ull) && cli_space(line.at[0]))
    {
        line.at += 1;
        line.length -= 1ull;
    }
    while ((line.length != 0ull) && cli_space(line.at[line.length - 1u]))
    {
        line.length -= 1ull;
    }
    return line;
}

static int cli_line_contains(CatalogTextLine line, const char *word)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; (at + size) <= line.length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = line.at + at, .other = word, .bytes = size}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int cli_starts(CatalogTextLine line, const char *word)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    return (line.length >= size) && (memor.cmp(&(MemoriaCfg){.src = line.at, .other = word, .bytes = size}) == 0);
}

// a retained line put after the paragraph, a space before each but the first
static void cli_paragraph_put(VerbaLinea *out, int *count, const char *text, unsigned long long length)
{
    mmgr_line_put(out, (*count != 0) ? " " : "");
    mmgr_line_put_n(out, text, length);
    *count += 1;
}

// A Python module's docstring: from the line it opens on, each line trimmed of its quotes, up to its close or a blank
// line after text
static void cli_docstring(const char *open, const char *end, VerbaLinea *out)
{
    int count = 0;
    int first = 1;
    CatalogTextLine line;
    const char *at = open;
    while (cli_line(&at, end, &line))
    {
        line = cli_trimmed(line);
        while (first && cli_starts(line, "\"\"\""))
        {
            line.at += 3;
            line.length -= 3ull;
        }
        first = 0;
        const int ended = cli_line_contains(line, "\"\"\"");
        while ((line.length >= 3ull) &&
               (memor.cmp(&(MemoriaCfg){.src = line.at + line.length - 3u, .other = "\"\"\"", .bytes = 3u}) == 0))
        {
            line.length -= 3ull;
        }
        line = cli_trimmed(line);
        if ((line.length == 0ull) && (count != 0))
        {
            return;
        }
        if (line.length != 0ull)
        {
            cli_paragraph_put(out, &count, line.at, line.length);
        }
        if (ended)
        {
            return;
        }
    }
}

// the end of the first 120 lines of the `size` bytes at `text`, the lines what a file declares of itself is read from
static const char *cli_about_end(const char *text, unsigned long long size)
{
    const char *end = text;
    const char *const whole = text + size;
    for (unsigned int lines = 0u; (lines < 120u) && (end < whole); lines += 1u)
    {
        end += cli_text_to_any(end, 0ull, (unsigned long long)(whole - end), "\n");
        end += (end < whole) ? 1 : 0;
    }
    return end;
}

// what the file at `path`, its text from `text` to `end`, declares of itself into `out`
static void cli_about_written(const char *path, const char *text, const char *end, VerbaLinea *out)
{
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    const char *const extension = (dot != NULL) ? (dot + 1) : "";
    CatalogTextLine line;
    const char *at = text;
    if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL}))
    {
        while (cli_line(&at, end, &line))
        {
            if (cli_starts(cli_trimmed(line), "\"\"\""))
            {
                cli_docstring(line.at, end, out);
                return;
            }
        }
        at = text;
    }
    const char *const mark = ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "sh", .cap = CLI_CELLS_POOL})) ||
                              (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL})) ||
                              (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "ps1", .cap = CLI_CELLS_POOL})))
                                 ? "#"
                                 : "//";
    int count = 0;
    while (cli_line(&at, end, &line))
    {
        line = cli_trimmed(line);
        const int license =
            cli_line_contains(line, "SPDX") || cli_line_contains(line, "Copyright") || cli_starts(line, "#!");
        if ((line.length == 0ull) && (count == 0))
        {
            continue;
        }
        if (!cli_starts(line, mark))
        {
            if ((count == 0) && license)
            {
                continue;
            }
            break;
        }
        if (license)
        {
            continue;
        }
        CatalogTextLine about_text = line;
        while (cli_starts(about_text, mark))
        {
            about_text.at += cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
            about_text.length -= cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
        }
        {
            const unsigned long long lead = cli_text_past_any(about_text.at, 0ull, about_text.length, "!");
            about_text.at += lead;
            about_text.length -= lead;
        }
        about_text = cli_trimmed(about_text);
        if (about_text.length == 0ull)
        {
            if (count == 0)
            {
                continue;
            }
            break;
        }
        cli_paragraph_put(out, &count, about_text.at, about_text.length);
    }
}

void cli_catalog_about(const char *path, VerbaLinea *out)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(path, &size, &g_cli_catalog_cells);
    if (bytes == NULL)
    {
        return;
    }
    // the file's bytes read as its text
    const char *const text = (const char *)bytes;
    cli_about_written(path, text, cli_about_end(text, size), out);
    cli_file_given(bytes, &g_cli_catalog_cells);
}

// what a file declares of itself is the text of the lines it is read from, a space put for each line's end it passes,
// so it is no longer than those lines
char *cli_catalog_about_taken(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(path, &size, &g_cli_catalog_cells);
    if (bytes == NULL)
    {
        return NULL;
    }
    // the file's bytes read as its text
    const char *const text = (const char *)bytes;
    const char *const end = cli_about_end(text, size);
    const unsigned long long room = (unsigned long long)(end - text) + 1ull;
    char *const about = g_cli_cells->persistent_buf_alloc(room, &g_cli_catalog_cells);
    if (about != NULL)
    {
        VerbaLinea line = {about, room, 0ull};
        cli_about_written(path, text, end, &line);
        mmgr_line_finish(&line);
    }
    cli_file_given(bytes, &g_cli_catalog_cells);
    return about;
}

unsigned int cli_catalog_case_names(const char *text, const char *variable, char (*names)[CLI_NAME_MAX_LEN],
                                    unsigned int room)
{
    char opener[CLI_NAME_MAX_LEN];
    VerbaLinea line = {opener, sizeof(opener), 0ull};
    mmgr_line_put(&line, "case \"$");
    mmgr_line_put(&line, variable);
    mmgr_line_put(&line, "\" in");
    mmgr_line_finish(&line);
    const char *const found = cellul.find(
        &(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = opener, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return 0u;
    }
    const char *at = found + cellul.len(&(CatenaFinitaCfg){.src = opener, .cap = CLI_CELLS_POOL});
    const char *const end = text + cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    CatalogTextLine read;
    CatalogTextLine first = {"", 0ull};
    while (cli_line(&at, end, &read))
    {
        read = cli_trimmed(read);
        if (read.length != 0ull)
        {
            first = read;
            break;
        }
    }
    unsigned long long close = 0ull;
    close = cli_text_to_any(first.at, 0ull, first.length, ")");
    if (close == first.length)
    {
        return 0u;
    }
    unsigned int count = 0u;
    unsigned long long from = 0ull;
    for (unsigned long long cut = 0ull; cut <= close; cut += 1ull)
    {
        if ((cut == close) || (first.at[cut] == '|'))
        {
            CatalogTextLine name = {first.at + from, cut - from};
            name = cli_trimmed(name);
            const int named = (name.length != 0ull) && !((name.length == 1u) && (name.at[0] == '*'));
            if (named && (names == NULL))
            {
                count += 1u;
            }
            else if (named && (count < room))
            {
                VerbaLinea name_line = {names[count], CLI_NAME_MAX_LEN, 0ull};
                mmgr_line_put_n(&name_line, name.at, name.length);
                count += (mmgr_line_finish(&name_line) != 0ull) ? 1u : 0u;
            }
            from = cut + 1ull;
        }
    }
    return count;
}

// the text of the literals from `from` up to the first `\n` written in them, joined into `joined` where it is not
// NULL: its length
static unsigned long long cli_driver_joined(const char *from, char *joined)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    unsigned long long length = 0ull;
    // the walk starts inside a literal, and each piece runs to its closing quote
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long close = cli_text_to_any(from, at, size, "\"");
        const unsigned long long stop = cli_text_to_pair(from, at, close, '\\', 'n');
        if (joined != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = joined + length, .src = from + at, .bytes = (size_t)(stop - at)});
        }
        length += stop - at;
        if (stop < close)
        {
            return length;
        }
        const unsigned long long open = (close < size) ? cli_text_to_any(from, close + 1ull, size, "\"") : size;
        at = (open < size) ? (open + 1ull) : size;
    }
    return length;
}

unsigned int cli_catalog_driver_parts(const char *source, char (*parts)[CLI_NAME_MAX_LEN], unsigned int room)
{
    static const char s_marker[] = "in the order given:";
    const char *const found = cellul.find(
        &(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL, .other = s_marker, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return 0u;
    }
    const char *const literals = found + cellul.len(&(CatenaFinitaCfg){.src = s_marker, .cap = CLI_CELLS_POOL});
    const unsigned long long length = cli_driver_joined(literals, NULL);
    char *const joined = g_cli_cells->temporary_buf_alloc(length + 1ull, &g_cli_catalog_cells);
    if (joined == NULL)
    {
        return 0u;
    }
    (void)cli_driver_joined(literals, joined);
    unsigned int count = 0u;
    unsigned long long from = 0ull;
    for (unsigned long long cut = 0ull; cut <= length; cut += 1ull)
    {
        if ((cut == length) || (joined[cut] == ','))
        {
            CatalogTextLine part = {joined + from, cut - from};
            part = cli_trimmed(part);
            if ((part.length != 0ull) && (parts == NULL))
            {
                count += 1u;
            }
            else if ((part.length != 0ull) && (count < room))
            {
                VerbaLinea name_line = {parts[count], CLI_NAME_MAX_LEN, 0ull};
                mmgr_line_put_n(&name_line, part.at, part.length);
                count += (mmgr_line_finish(&name_line) != 0ull) ? 1u : 0u;
            }
            from = cut + 1ull;
        }
    }
    g_cli_cells->persistent_buf_release(joined, &g_cli_catalog_cells);
    return count;
}

// a README's first paragraph is the text of its lines, a space put for each line's end it passes, so it is no longer
// than the README
char *cli_catalog_markdown_taken(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(path, &size, &g_cli_catalog_cells);
    if (bytes == NULL)
    {
        return NULL;
    }
    char *const opening = g_cli_cells->persistent_buf_alloc(size + 1ull, &g_cli_catalog_cells);
    if (opening == NULL)
    {
        cli_file_given(bytes, &g_cli_catalog_cells);
        return NULL;
    }
    VerbaLinea opened = {opening, size + 1ull, 0ull};
    // the file's bytes read as its text
    const char *at = (const char *)bytes;
    const char *const end = at + size;
    int count = 0;
    CatalogTextLine line;
    while (cli_line(&at, end, &line))
    {
        line = cli_trimmed(line);
        if ((line.length == 0ull) || cli_starts(line, "#") || cli_starts(line, "**"))
        {
            if (count != 0)
            {
                break;
            }
            continue;
        }
        cli_paragraph_put(&opened, &count, line.at, line.length);
    }
    mmgr_line_finish(&opened);
    cli_file_given(bytes, &g_cli_catalog_cells);
    return opening;
}
