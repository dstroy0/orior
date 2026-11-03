// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_targets.c: a Cargo.toml read as the search for tests reads it: its lines and values, its package's name, and
// its package's targets, each with its first file, as Cargo finds them
#include "../command_line/callback_cfg.h"
#include "cargo_tests_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most targets of a package and entries of a folder Cargo finds targets in, and those each list is taken with
#define CLI_CARGO_TARGETS 1024u
#define CLI_CARGO_ENTRIES 1024u

typedef struct
{
    const char *value;
    unsigned long long length;
    char *out;
    unsigned long long capacity;
    const char *text;
    unsigned int kind;
    const char *name;
    const char *path;
    unsigned int from;
    unsigned int to;
    const char *dir;
    char *path_out;
} CargoTargetsCtx;
// a TOML value as text: a string's contents in either quotes, or the bare word before a comment, into `out`, which
// sized for `capacity`
EMBED_INLINE void found_toml_value(const CargoTargetsCtx *args)
{
    const char *value = args->value;
    unsigned long long length = args->length;
    while ((length > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = value[0]}) != NULL))
    {
        value += 1;
        length -= 1ULL;
    }
    while ((length > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                   .src = " \t\r", .cap = CLI_CELLS_POOL, .byte = value[length - 1ULL]}) != NULL))
    {
        length -= 1ULL;
    }
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    if ((length > 0ULL) && ((value[0] == '"') || (value[0] == '\'')))
    {
        const char *const closed = memor.chr(&(MemoriaCfg){.src = value + 1, .val = value[0], .bytes = length - 1ULL});
        mmgr_line_put_n(&line, value + 1,
                        (closed != NULL) ? (unsigned long long)(closed - value) - 1ULL : length - 1ULL);
        mmgr_line_finish(&line);
        return;
    }
    const char *const comment = memor.chr(&(MemoriaCfg){.src = value, .val = '#', .bytes = length});
    unsigned long long size = (comment != NULL) ? (unsigned long long)(comment - value) : length;
    while ((size > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = value[size - 1ULL]}) != NULL))
    {
        size -= 1ULL;
    }
    mmgr_line_put_n(&line, value, size);
    mmgr_line_finish(&line);
}

// `length` bytes trimmed of ASCII whitespace into `out`, sized for `capacity`
EMBED_INLINE void found_trim(const CargoTargetsCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    while ((length > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = text[0]}) != NULL))
    {
        text += 1;
        length -= 1ULL;
    }
    while ((length > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_CELLS_POOL, .byte = text[length - 1ULL]}) != NULL))
    {
        length -= 1ULL;
    }
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, text, length);
    mmgr_line_finish(&line);
}

int cli_cargo_line(const CliCargoTestsTextCtx *args)
{
    unsigned long long *at = args->at;
    const char **line = args->line;
    unsigned long long *length = args->length;
    if (*at >= args->size)
    {
        return 0;
    }
    const CliTextLine bounds =
        EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = *at, .size = args->size);
    unsigned long long start = *at;
    unsigned long long end = bounds.stop;
    *at = bounds.next;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->text[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->text[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    *line = args->text + start;
    *length = end - start;
    return 1;
}

int cli_cargo_package_name(const CliCargoTestsCtx *args)
{
    int in_package = 0;
    unsigned long long at = 0ULL;
    const char *line = NULL;
    unsigned long long length = 0ULL;
    while (EMBED_CALL(cli_cargo_tests_text_internal.cargo_line, CliCargoTestsTextCtx, .text = args->text,
                      .size = args->size, .at = &at, .line = &line, .length = &length))
    {
        if ((length > 0ULL) && (line[0] == '['))
        {
            in_package =
                (length == 9ULL) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "[package]", .bytes = 9u}) == 0);
            continue;
        }
        const char *const equal =
            in_package ? memor.chr(&(MemoriaCfg){.src = line, .val = '=', .bytes = length}) : NULL;
        if (equal == NULL)
        {
            continue;
        }
        unsigned long long key = (unsigned long long)(equal - line);
        while ((key > 0ULL) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = line[key - 1ULL]}) != NULL))
        {
            key -= 1ULL;
        }
        if ((key == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = line, .other = "name", .bytes = 4u}) == 0))
        {
            EMBED_CALL(found_toml_value, CargoTargetsCtx, .value = equal + 1,
                       .length = length - (unsigned long long)(equal - line) - 1ULL, .out = args->name,
                       .capacity = CLI_NAME_MAX_LEN);
            return 1;
        }
    }
    return 0;
}

void cli_cargo_join(const CliCargoTestsACtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put(&line, args->a);
    mmgr_line_put(&line, ((args->a[0] != '\0') && (args->b[0] != '\0')) ? args->between : "");
    mmgr_line_put(&line, args->b);
    mmgr_line_finish(&line);
}

// The targets of one package and what Cargo finds on its own in a folder of it, each list a cell of its capacity at
// their most
typedef struct
{
    CliCargoFirst *firsts;
    unsigned int count;
    unsigned int capacity;
    CliCargoFirst *entries;
    unsigned int entry_count;
    unsigned int entry_capacity;
    const char *folder;
    const char *dir;
} CliCargoTargets;

static CliCargoTargets s_targets;

void cli_cargo_firsts_free(void)
{
    g_cli_cells->persistent_buf_release(s_targets.firsts, &g_cli_cargo_cells);
    s_targets.firsts = NULL;
    s_targets.count = 0u;
    s_targets.capacity = 0u;
}

// a target put with its first file
EMBED_INLINE void found_first(const CargoTargetsCtx *args)
{
    unsigned int kind = args->kind;
    if (s_targets.count < s_targets.capacity)
    {
        CliCargoFirst *const first = &s_targets.firsts[s_targets.count];
        first->kind = kind;
        memor.cpy(
            &(MemoriaCfg){.dst = first->name,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(
            &(MemoriaCfg){.dst = first->path,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
        s_targets.count += 1u;
    }
}

// whether a target's first file is taken already by one the manifest lists, from `from` to `to`
EMBED_INLINE int found_alloc(const CargoTargetsCtx *args)
{
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_targets.firsts[at].path, .other = args->path, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// an entry of a folder Cargo finds targets in: a file by its stem, or a folder of its own with a main.rs
static void found_entry(const CliEachNameIsFolderCfg *args)
{
    if (s_targets.entry_count == s_targets.entry_capacity)
    {
        return;
    }
    CliCargoFirst *const entry = &s_targets.entries[s_targets.entry_count];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    char path[CLI_PATH_MAX_LEN];
    if (!args->is_folder && (length >= 3u) &&
        (memor.cmp(&(MemoriaCfg){.src = args->name + length - 3u, .other = ".rs", .bytes = 3u}) == 0))
    {
        VerbaLinea line = {entry->name, sizeof(entry->name), 0ULL};
        mmgr_line_put_n(&line, args->name, length - 3u);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = s_targets.dir, .b = args->name,
                   .between = "/", .out = entry->path, .capacity = sizeof(entry->path));
        s_targets.entry_count += 1u;
        return;
    }
    char main_file[CLI_PATH_MAX_LEN];
    char inside[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = s_targets.dir, .b = args->name,
               .between = "/", .out = path, .capacity = sizeof(path));
    EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = path, .b = "main.rs", .between = "/",
               .out = inside, .capacity = sizeof(inside));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_targets.folder, .relative = inside, .joined = main_file);
    if (args->is_folder && EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = main_file))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = entry->name,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
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
    const int named =
        EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = one->name, .other_pointer = other->name);
    return (named != 0)
               ? named
               : EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = one->path, .other_pointer = other->path);
}

// the files Cargo finds on its own in `dir` of the package's folder, by their names, each put as a target of `kind`
// where the manifest lists none with that first file
EMBED_INLINE void found_auto(const CargoTargetsCtx *args)
{
    const char *dir = args->dir;
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_targets.folder, .relative = dir, .joined = folder);
    s_targets.dir = dir;
    s_targets.entry_count = 0u;
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = found_entry,
                     .context_bytess = NULL);
    qsort(s_targets.entries, s_targets.entry_count, sizeof(s_targets.entries[0]), found_entry_order);
    for (unsigned int at = 0u; at < s_targets.entry_count; at += 1u)
    {
        if (!EMBED_CALL(found_alloc, CargoTargetsCtx, .from = args->from, .to = args->to,
                        .path = s_targets.entries[at].path))
        {
            EMBED_CALL(found_first, CargoTargetsCtx, .kind = args->kind, .name = s_targets.entries[at].name,
                       .path = s_targets.entries[at].path);
        }
    }
}

// a path as Cargo reads one: forward slashes, with no `./` before it
EMBED_INLINE void found_clean(const CargoTargetsCtx *args)
{
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->path_out,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->path_out, .cap = CLI_CELLS_POOL}), .from = '\\',
               .to = '/');
    size_t start = 0u;
    while ((args->path_out[start] == '.') && (args->path_out[start + 1u] == '/'))
    {
        start += 2u;
    }
    memor.move_down(&(MemoriaCfg){
        .dst = args->path_out,
        .src = args->path_out + start,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path_out + start, .cap = CLI_CELLS_POOL}) + 1u});
}

// The tables of a manifest that list a binary or a test, as they are read
typedef struct
{
    int bin;
    char name[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    int named;
    int pathed;
} CliCargoListing;

const CliCargoFirst *cli_cargo_targets(const CliCargoTestsTextSizeCtx *args)
{
    const char *folder = args->folder;
    unsigned int *count = args->count;
    cli_cargo_firsts_free();
    s_targets.folder = folder;
    s_targets.firsts = g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_CARGO_TARGETS * sizeof(CliCargoFirst),
                                                         &g_cli_cargo_cells);
    s_targets.capacity = (s_targets.firsts != NULL) ? CLI_CARGO_TARGETS : 0u;
    // the folder's entries and the tables the manifest lists, given back as the targets are found
    const unsigned long long entry_bytes = (unsigned long long)CLI_CARGO_ENTRIES * sizeof(CliCargoFirst);
    char *const work = g_cli_cells->temporary_buf_alloc(
        entry_bytes + ((unsigned long long)CLI_CARGO_TARGETS * sizeof(CliCargoListing)), &g_cli_cargo_cells);
    s_targets.entries = NULL;
    s_targets.entry_capacity = 0u;
    CliCargoListing *tables = NULL;
    unsigned int listing_capacity = 0u;
    if (work != NULL)
    {
        s_targets.entries = (CliCargoFirst *)(void *)work;
        s_targets.entry_capacity = CLI_CARGO_ENTRIES;
        tables = (CliCargoListing *)(void *)(work + entry_bytes);
        listing_capacity = CLI_CARGO_TARGETS;
    }
    char table[CLI_NAME_MAX_LEN] = "";
    int autobins = 1;
    int autotests = 1;
    char lib[CLI_PATH_MAX_LEN] = "src/lib.rs";
    unsigned int list_entries = 0u;
    unsigned long long at = 0ULL;
    const char *line = NULL;
    unsigned long long length = 0ULL;
    while (EMBED_CALL(cli_cargo_tests_text_internal.cargo_line, CliCargoTestsTextCtx, .text = args->text,
                      .size = args->size, .at = &at, .line = &line, .length = &length))
    {
        const int doubled = (length >= 2ULL) && (line[0] == '[') && (line[1] == '[');
        if (doubled || ((length >= 1ULL) && (line[0] == '[')))
        {
            const unsigned long long open = doubled ? 2ULL : 1ULL;
            const char *const closed =
                doubled ? cellul.find(&(CatenaFinitaCfg){
                              .src = line + open, .cap = CLI_CELLS_POOL, .other = "]]", .other_cap = CLI_CELLS_POOL})
                        : memor.chr(&(MemoriaCfg){.src = line + open, .val = ']', .bytes = length - open});
            const unsigned long long end =
                ((closed != NULL) && (closed < line + length)) ? (unsigned long long)(closed - line) : length;
            EMBED_CALL(found_trim, CargoTargetsCtx, .text = line + open, .length = end - open, .out = table,
                       .capacity = sizeof(table));
            const int lists =
                doubled && ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL})) ||
                            (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "test", .cap = CLI_CELLS_POOL})));
            if (lists && (list_entries < listing_capacity))
            {
                tables[list_entries] = (CliCargoListing){
                    cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL}), "", "", 0, 0};
                list_entries += 1u;
            }
            continue;
        }
        const char *const equal = memor.chr(&(MemoriaCfg){.src = line, .val = '=', .bytes = length});
        if (equal == NULL)
        {
            continue;
        }
        char key[64];
        char value[CLI_PATH_MAX_LEN];
        EMBED_CALL(found_trim, CargoTargetsCtx, .text = line, .length = (unsigned long long)(equal - line), .out = key,
                   .capacity = sizeof(key));
        EMBED_CALL(found_toml_value, CargoTargetsCtx, .value = equal + 1,
                   .length = length - (unsigned long long)(equal - line) - 1ULL, .out = value,
                   .capacity = sizeof(value));
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
            EMBED_CALL(found_clean, CargoTargetsCtx, .path_out = lib);
        }
        else if (listing && (list_entries > 0u) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "name", .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){.dst = tables[list_entries - 1u].name,
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            tables[list_entries - 1u].named = 1;
        }
        else if (listing && (list_entries > 0u) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){.dst = tables[list_entries - 1u].path,
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            tables[list_entries - 1u].pathed = 1;
        }
    }
    char lib_full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = lib, .joined = lib_full);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = lib_full))
    {
        EMBED_CALL(found_first, CargoTargetsCtx, .kind = CLI_CARGO_LIB, .name = "", .path = lib);
    }
    const unsigned int listing_first = s_targets.count;
    for (unsigned int one = 0u; one < list_entries; one += 1u)
    {
        const CliCargoListing *const entry = &tables[one];
        if (!entry->named)
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea path_line = {path, sizeof(path), 0ULL};
        if (entry->pathed)
        {
            mmgr_line_put(&path_line, entry->path);
        }
        else if (entry->bin &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = entry->name, .other = args->package, .cap = CLI_CELLS_POOL})))
        {
            mmgr_line_put(&path_line, "src/main.rs");
        }
        else
        {
            mmgr_line_put(&path_line, entry->bin ? "src/bin/" : "tests/");
            mmgr_line_put(&path_line, entry->name);
            mmgr_line_put(&path_line, ".rs");
        }
        mmgr_line_finish(&path_line);
        EMBED_CALL(found_clean, CargoTargetsCtx, .path_out = path);
        EMBED_CALL(found_first, CargoTargetsCtx, .kind = entry->bin ? CLI_CARGO_BIN : CLI_CARGO_TEST,
                   .name = entry->name, .path = path);
    }
    const unsigned int reserved = s_targets.count;
    char main_file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "src/main.rs", .joined = main_file);
    if (autobins && EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = main_file) &&
        !EMBED_CALL(found_alloc, CargoTargetsCtx, .from = listing_first, .to = reserved, .path = "src/main.rs"))
    {
        EMBED_CALL(found_first, CargoTargetsCtx, .kind = CLI_CARGO_BIN, .name = args->package, .path = "src/main.rs");
    }
    if (autobins)
    {
        EMBED_CALL(found_auto, CargoTargetsCtx, .dir = "src/bin", .kind = CLI_CARGO_BIN, .from = listing_first,
                   .to = reserved);
    }
    if (autotests)
    {
        EMBED_CALL(found_auto, CargoTargetsCtx, .dir = "tests", .kind = CLI_CARGO_TEST, .from = listing_first,
                   .to = reserved);
    }
    g_cli_cells->persistent_buf_release(work, &g_cli_cargo_cells);
    s_targets.entries = NULL;
    s_targets.entry_count = 0u;
    s_targets.entry_capacity = 0u;
    *count = s_targets.count;
    return s_targets.firsts;
}
