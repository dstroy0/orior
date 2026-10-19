// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs.c: the oracle tables, the k-file head and the language tables read from the tree and written as the window
// reads them
#include "defs.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most bytes of a table or a header read, and the most folders and tables listed
#define CLI_DEFS_BYTES (1u << 22u)

#define CLI_DEFS_NAMES 512u

// a file's text and the names listed, held under the lock while the definitions are read
static char s_text[CLI_DEFS_BYTES];
static char s_names[CLI_DEFS_NAMES][CLI_NAME_LONGEST];
static unsigned int s_count;
static CliLock s_lock = CLI_LOCK_INIT;

// the file at `path` read into the text: its size, or QRY_MEMBER_UNREAD
static unsigned long long cli_read(const char *path)
{
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    return size;
}

// the line from `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`; where the next starts
static const char *cli_line(const char *at, const char *end, const char **stop)
{
    const char *next = at;
    while ((next < end) && (next[0] != '\n'))
    {
        next += 1;
    }
    *stop = ((next > at) && (next[-1] == '\r')) ? (next - 1) : next;
    return (next < end) ? (next + 1) : next;
}

static int cli_is_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// whether a line holds no table: a comment, or blank
static int cli_skipped(const char *from, const char *to)
{
    if ((from < to) && (from[0] == '#'))
    {
        return 1;
    }
    while ((from < to) && cli_is_space(from[0]))
    {
        from += 1;
    }
    return from == to;
}

// a line's fields between tabs written as a JSON list
static void cli_fields_written(CliLine *out, const char *from, const char *to)
{
    cli_line_character(out, '[');
    const char *field = from;
    for (const char *at = from;; at += 1)
    {
        if ((at == to) || (at[0] == '\t'))
        {
            cli_line_text(out, (field != from) ? "," : "");
            cli_json_quoted(out, field, (unsigned long long)(at - field));
            field = at + 1;
            if (at == to)
            {
                break;
            }
        }
    }
    cli_line_character(out, ']');
}

// A tab separated table whose first line that is not a comment names its columns, written; 0 where it has no such line
// or could not be read, nothing written then
static int cli_table_written(const char *root, const char *path, const char *name, CliLine *out)
{
    const unsigned long long size = cli_read(path);
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    const char *const end = s_text + size;
    const char *at = s_text;
    const char *stop = NULL;
    const char *next = NULL;
    while (at < end)
    {
        next = cli_line(at, end, &stop);
        if (!cli_skipped(at, stop))
        {
            break;
        }
        at = next;
    }
    if (at >= end)
    {
        return 0;
    }
    char relative[CLI_PATH_LONGEST];
    cli_relative(root, path, relative);
    cli_line_text(out, "{\"name\":");
    cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"path\":");
    cli_json_quoted(out, relative, cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"columns\":");
    cli_fields_written(out, at, stop);
    cli_line_text(out, ",\"rows\":[");
    int first = 1;
    at = next;
    while (at < end)
    {
        next = cli_line(at, end, &stop);
        if (!cli_skipped(at, stop))
        {
            cli_line_text(out, first ? "" : ",");
            cli_fields_written(out, at, stop);
            first = 0;
        }
        at = next;
    }
    cli_line_text(out, "]}");
    return 1;
}

// The comment lines a C header opens with, past its license line, each trimmed of its slashes and joined by a space,
// written as one JSON string
static void cli_header_written(const char *path, CliLine *out)
{
    static char s_joined[CLI_DEFS_BYTES];
    CliLine joined = {s_joined, sizeof(s_joined), 0ull};
    const unsigned long long size = cli_read(path);
    const char *const end = s_text + ((size == QRY_MEMBER_UNREAD) ? 0ull : size);
    int first = 1;
    const char *at = s_text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next = cli_line(at, end, &stop);
        const char *from = at;
        while ((from < stop) && cli_is_space(from[0]))
        {
            from += 1;
        }
        while ((stop > from) && cli_is_space(stop[-1]))
        {
            stop -= 1;
        }
        if (((stop - from) < 2) || (from[0] != '/') || (from[1] != '/'))
        {
            break;
        }
        int licensed = 0;
        for (const char *letter = from; !licensed && ((letter + 4) <= stop); letter += 1)
        {
            licensed = memor.cmp(&(MemoriaCfg){.src = letter, .other = "SPDX", .bytes = 4u}) == 0;
        }
        while (!licensed && (from < stop) && (from[0] == '/'))
        {
            from += 1;
        }
        while (!licensed && (from < stop) && cli_is_space(from[0]))
        {
            from += 1;
        }
        if (!licensed)
        {
            cli_line_text(&joined, first ? "" : " ");
            cli_line_counted(&joined, from, (unsigned long long)(stop - from));
            first = 0;
        }
        at = next;
    }
    const unsigned long long length = cli_line_fits(&joined) ? joined.at : 0ull;
    cli_json_quoted(out, s_joined, length);
}

// A `#define NAME value` read out of the text: where its value starts, trimmed, and its length; NULL where none is
static const char *cli_define(const char *name, unsigned long long size, unsigned long long *length)
{
    char key[CLI_NAME_LONGEST];
    CliLine line = {key, sizeof(key), 0ull};
    cli_line_text(&line, "#define ");
    cli_line_text(&line, name);
    cli_line_character(&line, ' ');
    const unsigned long long key_length = cli_line_closed(&line);
    const char *const end = s_text + size;
    const char *at = s_text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next = cli_line(at, end, &stop);
        if (((unsigned long long)(stop - at) >= key_length) &&
            (memor.cmp(&(MemoriaCfg){.src = at, .other = key, .bytes = key_length}) == 0))
        {
            const char *from = at + key_length;
            while ((from < stop) && cli_is_space(from[0]))
            {
                from += 1;
            }
            while ((stop > from) && cli_is_space(stop[-1]))
            {
                stop -= 1;
            }
            *length = (unsigned long long)(stop - from);
            return from;
        }
        at = next;
    }
    return NULL;
}

// a define's value as a whole number, its `u` suffix taken off: 1, or 0 where it is none or past 32 bits
static int cli_define_number(const char *name, unsigned long long size, unsigned long long *number)
{
    unsigned long long length = 0ull;
    const char *const value = cli_define(name, size, &length);
    while ((value != NULL) && (length != 0ull) && (value[length - 1u] == 'u'))
    {
        length -= 1ull;
    }
    unsigned long long total = 0ull;
    const unsigned long long sign = ((value != NULL) && (length != 0ull) && (value[0] == '+')) ? 1ull : 0ull;
    if ((value == NULL) || (length == sign))
    {
        return 0;
    }
    for (unsigned long long at = sign; at < length; at += 1ull)
    {
        if ((value[at] < '0') || (value[at] > '9'))
        {
            return 0;
        }
        total = (total * 10ull) + (unsigned long long)(value[at] - '0');
        if (total > 0xffffffffull)
        {
            return 0;
        }
    }
    *number = total;
    return 1;
}

// the head every k-file opens with, as krep_head.h defines it, written; `null` where it cannot be read
static void cli_head_written(const char *root, CliLine *out)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/src/cu/includes/file_defs/krep/krep_head.h");
    const unsigned long long size = (cli_line_closed(&line) != 0ull) ? cli_read(path) : QRY_MEMBER_UNREAD;
    unsigned long long bytes = 0ull;
    unsigned long long version = 0ull;
    unsigned long long magic_length = 0ull;
    const char *magic = (size != QRY_MEMBER_UNREAD) ? cli_define("KREP_MAGIC", size, &magic_length) : NULL;
    if ((magic == NULL) || !cli_define_number("KREP_HEAD_BYTES", size, &bytes) ||
        !cli_define_number("KREP_VERSION", size, &version))
    {
        cli_line_text(out, "null");
        return;
    }
    // the magic's quotes taken off, and its text up to its first escape
    while ((magic_length != 0ull) && (magic[0] == '"'))
    {
        magic += 1;
        magic_length -= 1ull;
    }
    while ((magic_length != 0ull) && (magic[magic_length - 1u] == '"'))
    {
        magic_length -= 1ull;
    }
    unsigned long long kept = 0ull;
    while ((kept < magic_length) && (magic[kept] != '\\'))
    {
        kept += 1ull;
    }
    cli_line_text(out, "{\"bytes\":");
    cli_line_decimal(out, bytes, 1u);
    cli_line_text(out, ",\"magic\":");
    cli_json_quoted(out, magic, kept);
    cli_line_text(out, ",\"version\":");
    cli_line_decimal(out, version, 1u);
    cli_line_character(out, '}');
}

// The names a folder lists: its folders, or its files ending `.tsv`, in order
static void cli_name_each(void *context, const char *name, int is_folder)
{
    const int *const folders = (const int *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const int wanted =
        *folders ? is_folder
                 : ((length > 4u) &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".tsv", .cap = CLI_CELLS_POOL})));
    if (wanted && (s_count < CLI_DEFS_NAMES) && (length < CLI_NAME_LONGEST))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_names[s_count],
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        s_count += 1u;
    }
}

static void cli_names_listed(const char *folder, int folders)
{
    s_count = 0u;
    query_folder_each(folder, cli_name_each, &folders);
    for (unsigned int at = 1u; at < s_count; at += 1u)
    {
        char held[CLI_NAME_LONGEST];
        memor.cpy(
            &(MemoriaCfg){.dst = held,
                          .src = s_names[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(s_names[place - 1u], held) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = s_names[place],
                .src = s_names[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_names[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_names[place],
                                .src = held,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

void cli_defs_read(const char *root, CliLine *out)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_lock_take(&s_lock);
    CliLine line = {folder, sizeof(folder), 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/src/cu/includes/file_defs");
    cli_line_closed(&line);
    cli_names_listed(folder, 1);
    cli_line_text(out, "{\"types\":[");
    int first = 1;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        const char *const ext = s_names[at];
        CliLine table = {path, sizeof(path), 0ull};
        cli_line_text(&table, folder);
        cli_line_character(&table, '/');
        cli_line_text(&table, ext);
        cli_line_character(&table, '/');
        cli_line_text(&table, ext);
        cli_line_text(&table, ".oracle.tsv");
        if (cli_line_closed(&table) == 0ull)
        {
            continue;
        }
        const unsigned long long begun = out->at;
        cli_line_text(out, first ? "{\"ext\":" : ",{\"ext\":");
        cli_json_quoted(out, ext, cellul.len(&(CatenaFinitaCfg){.src = ext, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"table\":");
        char name[CLI_NAME_LONGEST];
        CliLine named = {name, sizeof(name), 0ull};
        cli_line_text(&named, ext);
        cli_line_text(&named, ".oracle.tsv");
        cli_line_closed(&named);
        if (!cli_table_written(root, path, name, out))
        {
            out->at = begun;
            continue;
        }
        CliLine header = {path, sizeof(path), 0ull};
        cli_line_text(&header, folder);
        cli_line_character(&header, '/');
        cli_line_text(&header, ext);
        cli_line_character(&header, '/');
        cli_line_text(&header, ext);
        cli_line_text(&header, ".h");
        cli_line_closed(&header);
        cli_line_text(out, ",\"header\":");
        cli_header_written(path, out);
        cli_line_character(out, '}');
        first = 0;
    }
    cli_line_text(out, "],\"head\":");
    cli_head_written(root, out);
    cli_line_text(out, ",\"languages\":[");
    CliLine languages = {folder, sizeof(folder), 0ull};
    cli_line_text(&languages, root);
    cli_line_text(&languages, "/src/lng");
    cli_line_closed(&languages);
    cli_names_listed(folder, 0);
    first = 1;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        CliLine table = {path, sizeof(path), 0ull};
        cli_line_text(&table, folder);
        cli_line_character(&table, '/');
        cli_line_text(&table, s_names[at]);
        const unsigned long long begun = out->at;
        cli_line_text(out, first ? "" : ",");
        if ((cli_line_closed(&table) == 0ull) || !cli_table_written(root, path, s_names[at], out))
        {
            out->at = begun;
            continue;
        }
        first = 0;
    }
    cli_line_text(out, "]}");
    cli_lock_give(&s_lock);
}
