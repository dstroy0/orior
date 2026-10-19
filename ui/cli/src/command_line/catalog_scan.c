// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_scan.c: a folder's entries in order, and what a file says of itself: its opening paragraph, the names a case
// accepts, the parts a driver's usage names, and a README's first paragraph
#include "../text/text.h"
#include "catalog_internal.h"

// the most bytes of a file read for what it says of itself
#define CLI_SCAN_BYTES (1u << 22u)

static char s_text[CLI_SCAN_BYTES];
static CliLock s_lock = CLI_LOCK_INIT;

// What a folder's walk keeps: its entries and what it keeps of them
typedef struct
{
    CliEntries *entries;
    int folders;
    int (*keep)(const char *name);
} CliEntryWalk;

static void cli_entry_each(void *context, const char *name, int is_folder)
{
    CliEntryWalk *const walk = (CliEntryWalk *)context;
    CliEntries *const entries = walk->entries;
    const int kept = walk->folders ? is_folder : (!is_folder && (walk->keep != NULL) && walk->keep(name));
    if (!kept || (entries->count >= CLI_ENTRIES_MOST) ||
        (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_LONGEST))
    {
        return;
    }
    unsigned int place = entries->count;
    while ((place > 0u) && (cli_text_order(entries->name[place - 1u], name) > 0))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = entries->name[place], .bytes = CLI_NAME_LONGEST, .src = entries->name[place - 1u]});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = entries->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    entries->count += 1u;
}

void cli_catalog_entries(const char *folder, int folders, int (*keep)(const char *name), CliEntries *entries)
{
    entries->count = 0u;
    CliEntryWalk walk = {entries, folders, keep};
    query_folder_each(folder, cli_entry_each, &walk);
}

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

void cli_catalog_about(const char *path, CliLine *out)
{
    cli_lock_take(&s_lock);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text));
    if (size == QRY_MEMBER_UNREAD)
    {
        cli_lock_give(&s_lock);
        return;
    }
    // the first 120 lines are read
    const char *end = s_text;
    const char *const whole = s_text + size;
    for (unsigned int lines = 0u; (lines < 120u) && (end < whole); lines += 1u)
    {
        while ((end < whole) && (end[0] != '\n'))
        {
            end += 1;
        }
        end += (end < whole) ? 1 : 0;
    }
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    const char *const extension = (dot != NULL) ? (dot + 1) : "";
    CatalogTextLine line;
    const char *at = s_text;
    if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL}))
    {
        while (cli_line(&at, end, &line))
        {
            if (cli_starts(cli_trimmed(line), "\"\"\""))
            {
                cli_docstring(line.at, end, out);
                cli_lock_give(&s_lock);
                return;
            }
        }
        at = s_text;
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
    cli_lock_give(&s_lock);
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
            if ((name.length != 0ull) && !((name.length == 1u) && (name.at[0] == '*')) && (count < room))
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

unsigned int cli_catalog_driver_parts(const char *source, char (*parts)[CLI_NAME_LONGEST], unsigned int room)
{
    static const char s_marker[] = "in the order given:";
    const char *const found = cellul.find(
        &(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL, .other = s_marker, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return 0u;
    }
    static char s_joined[1u << 16u];
    CliLine joined = {s_joined, sizeof(s_joined), 0ull};
    int inside = 1;
    for (const char *at = found + cellul.len(&(CatenaFinitaCfg){.src = s_marker, .cap = CLI_CELLS_POOL}); at[0] != '\0';
         at += 1)
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
        cli_line_character(&joined, at[0]);
        if (cli_line_fits(&joined) && (joined.at >= 2u) && (s_joined[joined.at - 2u] == '\\') &&
            (s_joined[joined.at - 1u] == 'n'))
        {
            joined.at -= 2u;
            break;
        }
    }
    const unsigned long long length = cli_line_fits(&joined) ? joined.at : 0ull;
    unsigned int count = 0u;
    unsigned long long from = 0ull;
    for (unsigned long long cut = 0ull; cut <= length; cut += 1ull)
    {
        if ((cut == length) || (s_joined[cut] == ','))
        {
            CatalogTextLine part = {s_joined + from, cut - from};
            part = cli_trimmed(part);
            if ((part.length != 0ull) && (count < room))
            {
                CliLine kept = {parts[count], CLI_NAME_LONGEST, 0ull};
                cli_line_counted(&kept, part.at, part.length);
                count += (cli_line_closed(&kept) != 0ull) ? 1u : 0u;
            }
            from = cut + 1ull;
        }
    }
    return count;
}

void cli_catalog_markdown_opening(const char *path, CliLine *out)
{
    cli_lock_take(&s_lock);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text));
    const char *at = s_text;
    const char *const end = s_text + ((size == QRY_MEMBER_UNREAD) ? 0ull : size);
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
        cli_kept(out, &count, line.at, line.length);
    }
    cli_lock_give(&s_lock);
}
