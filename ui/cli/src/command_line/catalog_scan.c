// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_scan.c: a path joined and a path's stem, and what a file says of itself: its opening paragraph, the names a
// case accepts, the parts a driver's usage names, and a README's first paragraph
#include "../text/text.h"
#include "catalog_internal.h"

void cli_catalog_join(const char *folder, const char *name, char *path)
{
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

void cli_catalog_stem(const char *path, char *out)
{
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((name > path) && (name[-1] != '/') && (name[-1] != '\\'))
    {
        name -= 1;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const unsigned long long length = ((dot != NULL) && (dot != name))
                                          ? (unsigned long long)(dot - name)
                                          : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    CliLine line = {out, CLI_NAME_LONGEST, 0ull};
    cli_line_counted(&line, name, length);
    cli_line_closed(&line);
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
    while ((stop < end) && (stop[0] != '\n'))
    {
        stop += 1;
    }
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

static int cli_holds(CatalogTextLine line, const char *word)
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

// a kept line put after the paragraph, a space before each but the first
static void cli_kept(CliLine *out, int *count, const char *text, unsigned long long length)
{
    cli_line_text(out, (*count != 0) ? " " : "");
    cli_line_counted(out, text, length);
    *count += 1;
}

// A Python module's docstring: from the line it opens on, each line trimmed of its quotes, up to its close or a blank
// line after text
static void cli_docstring(const char *open, const char *end, CliLine *out)
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
        const int ended = cli_holds(line, "\"\"\"");
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
            cli_kept(out, &count, line.at, line.length);
        }
        if (ended)
        {
            return;
        }
    }
}

// the end of the first 120 lines of the `size` bytes at `text`, the lines what a file says of itself is read from
static const char *cli_about_end(const char *text, unsigned long long size)
{
    const char *end = text;
    const char *const whole = text + size;
    for (unsigned int lines = 0u; (lines < 120u) && (end < whole); lines += 1u)
    {
        while ((end < whole) && (end[0] != '\n'))
        {
            end += 1;
        }
        end += (end < whole) ? 1 : 0;
    }
    return end;
}

// what the file at `path`, its text from `text` to `end`, says of itself into `out`
static void cli_about_said(const char *path, const char *text, const char *end, CliLine *out)
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
        const int license = cli_holds(line, "SPDX") || cli_holds(line, "Copyright") || cli_starts(line, "#!");
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
        CatalogTextLine said = line;
        while (cli_starts(said, mark))
        {
            said.at += cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
            said.length -= cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
        }
        while ((said.length != 0ull) && (said.at[0] == '!'))
        {
            said.at += 1;
            said.length -= 1ull;
        }
        said = cli_trimmed(said);
        if (said.length == 0ull)
        {
            if (count == 0)
            {
                continue;
            }
            break;
        }
        cli_kept(out, &count, said.at, said.length);
    }
}

void cli_catalog_about(const char *path, CliLine *out)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        return;
    }
    // the file's bytes read as its text
    const char *const text = (const char *)bytes;
    cli_about_said(path, text, cli_about_end(text, size), out);
    cli_file_let_go(bytes, size);
}

// what a file says of itself is the text of the lines it is read from, a space put for each line's end it passes, so
// it is no longer than those lines
char *cli_catalog_about_taken(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        return NULL;
    }
    // the file's bytes read as its text
    const char *const text = (const char *)bytes;
    const char *const end = cli_about_end(text, size);
    const unsigned long long room = (unsigned long long)(end - text) + 1ull;
    char *const about = cli_cell_taken_by(&g_cli_catalog_cells, room, "what a file says of itself");
    if (about != NULL)
    {
        CliLine line = {about, room, 0ull};
        cli_about_said(path, text, end, &line);
        cli_line_closed(&line);
    }
    cli_file_let_go(bytes, size);
    return about;
}

unsigned int cli_catalog_case_names(const char *text, const char *variable, char (*names)[CLI_NAME_LONGEST],
                                    unsigned int room)
{
    char opener[CLI_NAME_LONGEST];
    CliLine line = {opener, sizeof(opener), 0ull};
    cli_line_text(&line, "case \"$");
    cli_line_text(&line, variable);
    cli_line_text(&line, "\" in");
    cli_line_closed(&line);
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
    while ((close < first.length) && (first.at[close] != ')'))
    {
        close += 1ull;
    }
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
                CliLine kept = {names[count], CLI_NAME_LONGEST, 0ull};
                cli_line_counted(&kept, name.at, name.length);
                count += (cli_line_closed(&kept) != 0ull) ? 1u : 0u;
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
    unsigned long long length = 0ull;
    char previous = '\0';
    int inside = 1;
    for (const char *at = from; at[0] != '\0'; at += 1)
    {
        if (at[0] == '"')
        {
            inside = !inside;
            continue;
        }
        if (!inside)
        {
            continue;
        }
        if ((previous == '\\') && (at[0] == 'n'))
        {
            return length - 1ull;
        }
        if (joined != NULL)
        {
            joined[length] = at[0];
        }
        length += 1ull;
        previous = at[0];
    }
    return length;
}

unsigned int cli_catalog_driver_parts(const char *source, char (*parts)[CLI_NAME_LONGEST], unsigned int room)
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
    char *const joined = cli_cell_taken_by(&g_cli_catalog_cells, length + 1ull, "the driver's parts joined");
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
                CliLine kept = {parts[count], CLI_NAME_LONGEST, 0ull};
                cli_line_counted(&kept, part.at, part.length);
                count += (cli_line_closed(&kept) != 0ull) ? 1u : 0u;
            }
            from = cut + 1ull;
        }
    }
    cli_cell_given_by(&g_cli_catalog_cells, joined);
    return count;
}

// a README's first paragraph is the text of its lines, a space put for each line's end it passes, so it is no longer
// than the README
char *cli_catalog_markdown_taken(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        return NULL;
    }
    char *const opening = cli_cell_taken_by(&g_cli_catalog_cells, size + 1ull, "a README's first paragraph");
    if (opening == NULL)
    {
        cli_file_let_go(bytes, size);
        return NULL;
    }
    CliLine opened = {opening, size + 1ull, 0ull};
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
        cli_kept(&opened, &count, line.at, line.length);
    }
    cli_line_closed(&opened);
    cli_file_let_go(bytes, size);
    return opening;
}
