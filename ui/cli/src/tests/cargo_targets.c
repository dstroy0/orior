// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_targets.c: a Cargo.toml read as the search for tests reads it: its lines and values, its package's name, and
// its package's targets, each with its first file, as Cargo finds them
#include "cargo_tests_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most targets of a package and entries of a folder Cargo finds targets in
#define CLI_CARGO_TARGETS 1024u
#define CLI_CARGO_ENTRIES 1024u

// a TOML value as text: a string's contents in either quotes, or the bare word before a comment, into `out`, which
// holds `room`
static void found_toml_value(const char *value, unsigned long long length, char *out, unsigned long long room)
{
    while ((length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = value[0]}) != NULL))
    {
        value += 1;
        length -= 1ull;
    }
    while ((length > 0ull) && (cellul.chr(&(CatenaFinitaCfg){
                                   .src = " \t\r", .cap = CLI_CELLS_POOL, .byte = value[length - 1ull]}) != NULL))
    {
        length -= 1ull;
    }
    CliLine line = {out, room, 0ull};
    if ((length > 0ull) && ((value[0] == '"') || (value[0] == '\'')))
    {
        const char *const closed = memor.chr(&(MemoriaCfg){.src = value + 1, .val = value[0], .bytes = length - 1ull});
        cli_line_counted(&line, value + 1,
                         (closed != NULL) ? (unsigned long long)(closed - value) - 1ull : length - 1ull);
        cli_line_closed(&line);
        return;
    }
    const char *const comment = memor.chr(&(MemoriaCfg){.src = value, .val = '#', .bytes = length});
    unsigned long long size = (comment != NULL) ? (unsigned long long)(comment - value) : length;
    while ((size > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = value[size - 1ull]}) != NULL))
    {
        size -= 1ull;
    }
    cli_line_counted(&line, value, size);
    cli_line_closed(&line);
}

// `length` bytes trimmed of ASCII whitespace into `out`, which holds `room`
static void found_trimmed(const char *text, unsigned long long length, char *out, unsigned long long room)
{
    while ((length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = text[0]}) != NULL))
    {
        text += 1;
        length -= 1ull;
    }
    while ((length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = text[length - 1ull]}) != NULL))
    {
        length -= 1ull;
    }
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, text, length);
    cli_line_closed(&line);
}

int cli_cargo_line(const char *text, unsigned long long size, unsigned long long *at, const char **line,
                   unsigned long long *length)
{
    if (*at >= size)
    {
        return 0;
    }
    const char *const newline = memor.chr(&(MemoriaCfg){.src = text + *at, .val = '\n', .bytes = size - *at});
    unsigned long long start = *at;
    unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
    *at = (newline != NULL) ? end + 1ull : size;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = text[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = text[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    *line = text + start;
    *length = end - start;
    return 1;
}

int cli_cargo_package_name(const char *text, unsigned long long size, char *name)
{
    int in_package = 0;
    unsigned long long at = 0ull;
    const char *line = NULL;
    unsigned long long length = 0ull;
    while (cli_cargo_line(text, size, &at, &line, &length))
    {
        if ((length > 0ull) && (line[0] == '['))
        {
            in_package =
                (length == 9ull) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "[package]", .bytes = 9u}) == 0);
            continue;
        }
        const char *const equal =
            in_package ? memor.chr(&(MemoriaCfg){.src = line, .val = '=', .bytes = length}) : NULL;
        if (equal == NULL)
        {
            continue;
        }
        unsigned long long key = (unsigned long long)(equal - line);
        while ((key > 0ull) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = line[key - 1ull]}) != NULL))
        {
            key -= 1ull;
        }
        if ((key == 4ull) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "name", .bytes = 4u}) == 0))
        {
            found_toml_value(equal + 1, length - (unsigned long long)(equal - line) - 1ull, name, CLI_NAME_LONGEST);
            return 1;
        }
    }
    return 0;
}

void cli_cargo_joined(const char *a, const char *b, const char *between, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    cli_line_text(&line, a);
    cli_line_text(&line, ((a[0] != '\0') && (b[0] != '\0')) ? between : "");
    cli_line_text(&line, b);
    cli_line_closed(&line);
}

// The targets of one package and what Cargo finds on its own in a folder of it
typedef struct
{
    CliCargoFirst firsts[CLI_CARGO_TARGETS];
    unsigned int count;
    CliCargoFirst entries[CLI_CARGO_ENTRIES];
    unsigned int entry_count;
    const char *folder;
    const char *dir;
} CliCargoTargets;

static CliCargoTargets s_targets;

// a target put with its first file
static void found_first(unsigned int kind, const char *name, const char *path)
{
    if (s_targets.count < CLI_CARGO_TARGETS)
    {
        CliCargoFirst *const first = &s_targets.firsts[s_targets.count];
        first->kind = kind;
        memor.cpy(&(MemoriaCfg){.dst = first->name,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = first->path,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        s_targets.count += 1u;
    }
}

// whether a target's first file is taken already by one the manifest lists, from `from` to `to`
static int found_taken(unsigned int from, unsigned int to, const char *path)
{
    for (unsigned int at = from; at < to; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_targets.firsts[at].path, .other = path, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// an entry of a folder Cargo finds targets in: a file by its stem, or a folder of its own with a main.rs
static void found_entry(void *context, const char *name, int is_folder)
{
    (void)context;
    if (s_targets.entry_count == CLI_CARGO_ENTRIES)
    {
        return;
    }
    CliCargoFirst *const entry = &s_targets.entries[s_targets.entry_count];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char path[CLI_PATH_LONGEST];
    if (!is_folder && (length >= 3u) &&
        (memor.cmp(&(MemoriaCfg){.src = name + length - 3u, .other = ".rs", .bytes = 3u}) == 0))
    {
        CliLine line = {entry->name, sizeof(entry->name), 0ull};
        cli_line_counted(&line, name, length - 3u);
        cli_line_closed(&line);
        cli_cargo_joined(s_targets.dir, name, "/", entry->path, sizeof(entry->path));
        s_targets.entry_count += 1u;
        return;
    }
    char main_file[CLI_PATH_LONGEST];
    char inside[CLI_PATH_LONGEST];
    cli_cargo_joined(s_targets.dir, name, "/", path, sizeof(path));
    cli_cargo_joined(path, "main.rs", "/", inside, sizeof(inside));
    cli_full(s_targets.folder, inside, main_file);
    if (is_folder && query_is_file(main_file))
    {
        memor.cpy(&(MemoriaCfg){.dst = entry->name,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = entry->path,
                                .src = inside,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = inside, .cap = CLI_CELLS_POOL}) + 1u});
        s_targets.entry_count += 1u;
    }
}

static int found_entry_order(const void *left, const void *right)
{
    const CliCargoFirst *const one = (const CliCargoFirst *)left;
    const CliCargoFirst *const other = (const CliCargoFirst *)right;
    const int named = cli_text_order(one->name, other->name);
    return (named != 0) ? named : cli_text_order(one->path, other->path);
}

// the files Cargo finds on its own in `dir` of the package's folder, by their names, each put as a target of `kind`
// where the manifest lists none with that first file
static void found_auto(const char *dir, unsigned int kind, unsigned int from, unsigned int to)
{
    char folder[CLI_PATH_LONGEST];
    cli_full(s_targets.folder, dir, folder);
    s_targets.dir = dir;
    s_targets.entry_count = 0u;
    (void)query_folder_each(folder, found_entry, NULL);
    qsort(s_targets.entries, s_targets.entry_count, sizeof(s_targets.entries[0]), found_entry_order);
    for (unsigned int at = 0u; at < s_targets.entry_count; at += 1u)
    {
        if (!found_taken(from, to, s_targets.entries[at].path))
        {
            found_first(kind, s_targets.entries[at].name, s_targets.entries[at].path);
        }
    }
}

// a path as Cargo reads one: forward slashes, with no `./` before it
static void found_clean(char *path)
{
    for (char *letter = path; *letter != '\0'; letter += 1)
    {
        *letter = (*letter == '\\') ? '/' : *letter;
    }
    size_t start = 0u;
    while ((path[start] == '.') && (path[start + 1u] == '/'))
    {
        start += 2u;
    }
    memor.move_down(
        &(MemoriaCfg){.dst = path,
                      .src = path + start,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = path + start, .cap = CLI_CELLS_POOL}) + 1u});
}

// The tables of a manifest that list a binary or a test, as they are read
typedef struct
{
    int bin;
    char name[CLI_NAME_LONGEST];
    char path[CLI_PATH_LONGEST];
    int named;
    int pathed;
} CliCargoListed;

static CliCargoListed s_listed[CLI_CARGO_TARGETS];

const CliCargoFirst *cli_cargo_targets(const char *folder, const char *text, unsigned long long size,
                                       const char *package, unsigned int *count)
{
    s_targets.count = 0u;
    s_targets.folder = folder;
    char table[CLI_NAME_LONGEST] = "";
    int autobins = 1;
    int autotests = 1;
    char lib[CLI_PATH_LONGEST] = "src/lib.rs";
    unsigned int listed = 0u;
    unsigned long long at = 0ull;
    const char *line = NULL;
    unsigned long long length = 0ull;
    while (cli_cargo_line(text, size, &at, &line, &length))
    {
        const int doubled = (length >= 2ull) && (line[0] == '[') && (line[1] == '[');
        if (doubled || ((length >= 1ull) && (line[0] == '[')))
        {
            const unsigned long long open = doubled ? 2ull : 1ull;
            const char *const closed =
                doubled ? cellul.find(&(CatenaFinitaCfg){
                              .src = line + open, .cap = CLI_CELLS_POOL, .other = "]]", .other_cap = CLI_CELLS_POOL})
                        : memor.chr(&(MemoriaCfg){.src = line + open, .val = ']', .bytes = length - open});
            const unsigned long long end =
                ((closed != NULL) && (closed < line + length)) ? (unsigned long long)(closed - line) : length;
            found_trimmed(line + open, end - open, table, sizeof(table));
            if (doubled &&
                ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "test", .cap = CLI_CELLS_POOL}))) &&
                (listed < CLI_CARGO_TARGETS))
            {
                s_listed[listed] = (CliCargoListed){
                    cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL}), "", "", 0, 0};
                listed += 1u;
            }
            continue;
        }
        const char *const equal = memor.chr(&(MemoriaCfg){.src = line, .val = '=', .bytes = length});
        if (equal == NULL)
        {
            continue;
        }
        char key[64];
        char value[CLI_PATH_LONGEST];
        found_trimmed(line, (unsigned long long)(equal - line), key, sizeof(key));
        found_toml_value(equal + 1, length - (unsigned long long)(equal - line) - 1ull, value, sizeof(value));
        const int listing = (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL})) ||
                            (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "test", .cap = CLI_CELLS_POOL}));
        if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "package", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "autobins", .cap = CLI_CELLS_POOL})))
        {
            autobins = (!cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "false", .cap = CLI_CELLS_POOL}));
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "package", .cap = CLI_CELLS_POOL})) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "autotests", .cap = CLI_CELLS_POOL})))
        {
            autotests = (!cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "false", .cap = CLI_CELLS_POOL}));
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "lib", .cap = CLI_CELLS_POOL})) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){.dst = lib,
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            found_clean(lib);
        }
        else if (listing && (listed > 0u) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "name", .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_listed[listed - 1u].name,
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            s_listed[listed - 1u].named = 1;
        }
        else if (listing && (listed > 0u) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_listed[listed - 1u].path,
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            s_listed[listed - 1u].pathed = 1;
        }
    }
    char lib_full[CLI_PATH_LONGEST];
    cli_full(folder, lib, lib_full);
    if (query_is_file(lib_full))
    {
        found_first(CLI_CARGO_LIB, "", lib);
    }
    const unsigned int listed_first = s_targets.count;
    for (unsigned int one = 0u; one < listed; one += 1u)
    {
        CliCargoListed *const entry = &s_listed[one];
        if (!entry->named)
        {
            continue;
        }
        char path[CLI_PATH_LONGEST];
        CliLine path_line = {path, sizeof(path), 0ull};
        if (entry->pathed)
        {
            cli_line_text(&path_line, entry->path);
        }
        else if (entry->bin &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = entry->name, .other = package, .cap = CLI_CELLS_POOL})))
        {
            cli_line_text(&path_line, "src/main.rs");
        }
        else
        {
            cli_line_text(&path_line, entry->bin ? "src/bin/" : "tests/");
            cli_line_text(&path_line, entry->name);
            cli_line_text(&path_line, ".rs");
        }
        cli_line_closed(&path_line);
        found_clean(path);
        found_first(entry->bin ? CLI_CARGO_BIN : CLI_CARGO_TEST, entry->name, path);
    }
    const unsigned int taken = s_targets.count;
    char main_file[CLI_PATH_LONGEST];
    cli_full(folder, "src/main.rs", main_file);
    if (autobins && query_is_file(main_file) && !found_taken(listed_first, taken, "src/main.rs"))
    {
        found_first(CLI_CARGO_BIN, package, "src/main.rs");
    }
    if (autobins)
    {
        found_auto("src/bin", CLI_CARGO_BIN, listed_first, taken);
    }
    if (autotests)
    {
        found_auto("tests", CLI_CARGO_TEST, listed_first, taken);
    }
    *count = s_targets.count;
    return s_targets.firsts;
}
