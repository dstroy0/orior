// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs.c: the oracle tables, the k-file head and the language tables read from the tree and written as the window
// reads them
#include "defs.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the folders or tables a listing's room is begun with, doubling as they are put, and the most it holds
#define CLI_DEFS_NAMES_FIRST 32u
#define CLI_DEFS_NAMES 512u

CliCellsTally g_cli_defs_cells = CLI_CELLS_TALLY("cli_defs");

// A file read: its text, mapped, and its size; the text NULL where it could not be read
typedef struct
{
    const char *text;
    unsigned long long size;
} CliDefsFile;

// The names a folder lists, its folders where `folders` is 1, else its files ending `.tsv`, in a cell whose room
// doubles as they are put
typedef struct
{
    char (*name)[CLI_NAME_LONGEST];
    unsigned int count;
    unsigned int room;
    int folders;
} CliDefsNames;

// the file at `path` mapped whole, let go with cli_unmapped
static CliDefsFile cli_mapped(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    // a file's bytes are read as its text
    return (CliDefsFile){(const char *)bytes, size};
}

static void cli_unmapped(CliDefsFile file)
{
    if (file.text != NULL)
    {
        // the text is the bytes the file was mapped as
        cli_file_let_go((const unsigned char *)file.text, file.size);
    }
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

// The table `file` holds written, its first line that is not a comment naming its columns; 0 where it has no such line,
// nothing written then
static int cli_rows_written(const char *root, const char *path, const char *name, CliDefsFile file, CliLine *out)
{
    const char *const end = file.text + file.size;
    const char *at = file.text;
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

// A tab separated table whose first line that is not a comment names its columns, written; 0 where it has no such line
// or could not be read, nothing written then
static int cli_table_written(const char *root, const char *path, const char *name, CliLine *out)
{
    const CliDefsFile file = cli_mapped(path);
    if (file.text == NULL)
    {
        return 0;
    }
    const int written = cli_rows_written(root, path, name, file, out);
    cli_unmapped(file);
    return written;
}

// The comment lines a C header opens with, past its license line, each trimmed of its slashes and joined by a space,
// written as one JSON string, joined in a cell of the header's size
static void cli_header_written(const char *path, CliLine *out)
{
    const CliDefsFile file = cli_mapped(path);
    const unsigned long long size = (file.text != NULL) ? file.size : 0ull;
    char *const text = cli_cell_taken_by(&g_cli_defs_cells, size + 1ull, "a header's comment");
    CliLine joined = {text, (text != NULL) ? (size + 1ull) : 0ull, 0ull};
    const char *const end = (text != NULL) ? (file.text + size) : file.text;
    int first = 1;
    const char *at = file.text;
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
    cli_json_quoted(out, (text != NULL) ? text : "", length);
    cli_cell_given_by(&g_cli_defs_cells, text);
    cli_unmapped(file);
}

// A `#define NAME value` read out of the text: where its value starts, trimmed, and its length; NULL where none is
static const char *cli_define(const char *name, CliDefsFile file, unsigned long long *length)
{
    char key[CLI_NAME_LONGEST];
    CliLine line = {key, sizeof(key), 0ull};
    cli_line_text(&line, "#define ");
    cli_line_text(&line, name);
    cli_line_character(&line, ' ');
    const unsigned long long key_length = cli_line_closed(&line);
    const char *const end = file.text + file.size;
    const char *at = file.text;
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
static int cli_define_number(const char *name, CliDefsFile file, unsigned long long *number)
{
    unsigned long long length = 0ull;
    const char *const value = cli_define(name, file, &length);
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

// the head every k-file opens with, as the header `file` defines it, written; `null` where it defines none
static void cli_head_of(CliDefsFile file, CliLine *out)
{
    unsigned long long bytes = 0ull;
    unsigned long long version = 0ull;
    unsigned long long magic_length = 0ull;
    const char *magic = (file.text != NULL) ? cli_define("KREP_MAGIC", file, &magic_length) : NULL;
    if ((magic == NULL) || !cli_define_number("KREP_HEAD_BYTES", file, &bytes) ||
        !cli_define_number("KREP_VERSION", file, &version))
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

// the head every k-file opens with, as krep_head.h defines it, written; `null` where it cannot be read
static void cli_head_written(const char *root, CliLine *out)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/src/cu/includes/file_defs/krep/krep_head.h");
    const CliDefsFile file = (cli_line_closed(&line) != 0ull) ? cli_mapped(path) : (CliDefsFile){NULL, 0ull};
    cli_head_of(file, out);
    cli_unmapped(file);
}

// The names a folder lists: its folders, or its files ending `.tsv`, in order
// room made for one name more in `names`: 1, or 0 where it holds the most or its larger cell is refused
static int cli_name_room(CliDefsNames *names)
{
    if (names->count < names->room)
    {
        return 1;
    }
    const unsigned int doubled = (names->room == 0u) ? CLI_DEFS_NAMES_FIRST : (names->room * 2u);
    const unsigned int room = (doubled < CLI_DEFS_NAMES) ? doubled : CLI_DEFS_NAMES;
    char (*const larger)[CLI_NAME_LONGEST] =
        (names->count < CLI_DEFS_NAMES)
            ? cli_cell_taken_by(&g_cli_defs_cells, (unsigned long long)room * CLI_NAME_LONGEST,
                                "the folders and tables of the definitions")
            : NULL;
    if (larger == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = names->name, .bytes = (size_t)names->count * CLI_NAME_LONGEST});
    cli_cell_given_by(&g_cli_defs_cells, names->name);
    names->name = larger;
    names->room = room;
    return 1;
}

static void cli_name_each(void *context, const char *name, int is_folder)
{
    CliDefsNames *const names = context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const int wanted =
        names->folders
            ? is_folder
            : ((length > 4u) &&
               (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".tsv", .cap = CLI_CELLS_POOL})));
    if (wanted && (length < CLI_NAME_LONGEST) && cli_name_room(names))
    {
        memor.cpy(&(MemoriaCfg){.dst = names->name[names->count], .src = name, .bytes = length + 1u});
        names->count += 1u;
    }
}

// the names `folder` lists into `names`, in order, those it held before given back
static void cli_names_listed(const char *folder, int folders, CliDefsNames *names)
{
    cli_cell_given_by(&g_cli_defs_cells, names->name);
    *names = (CliDefsNames){NULL, 0u, 0u, folders};
    query_folder_each(folder, cli_name_each, names);
    char (*const name)[CLI_NAME_LONGEST] = names->name;
    for (unsigned int at = 1u; at < names->count; at += 1u)
    {
        char held[CLI_NAME_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = held,
                                .src = name[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(name[place - 1u], held) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = name[place],
                .src = name[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = name[place],
                                .src = held,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

void cli_defs_read(const char *root, CliLine *out)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    CliDefsNames names = {NULL, 0u, 0u, 1};
    CliLine line = {folder, sizeof(folder), 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/src/cu/includes/file_defs");
    cli_line_closed(&line);
    cli_names_listed(folder, 1, &names);
    cli_line_text(out, "{\"types\":[");
    int first = 1;
    for (unsigned int at = 0u; at < names.count; at += 1u)
    {
        const char *const ext = names.name[at];
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
    cli_names_listed(folder, 0, &names);
    first = 1;
    for (unsigned int at = 0u; at < names.count; at += 1u)
    {
        CliLine table = {path, sizeof(path), 0ull};
        cli_line_text(&table, folder);
        cli_line_character(&table, '/');
        cli_line_text(&table, names.name[at]);
        const unsigned long long begun = out->at;
        cli_line_text(out, first ? "" : ",");
        if ((cli_line_closed(&table) == 0ull) || !cli_table_written(root, path, names.name[at], out))
        {
            out->at = begun;
            continue;
        }
        first = 0;
    }
    cli_line_text(out, "]}");
    cli_cell_given_by(&g_cli_defs_cells, names.name);
}
