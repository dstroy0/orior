// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_targets.c: a Cargo.toml read as the search for tests reads it: its lines and values, its package's name, and
// its package's targets, each with its first file, as Cargo finds them
#include "../command_line/callback_cfg.h"
#include "../text/sort.h"
#include "cargo_tests_internal.h"

#include "../text/text.h"

// the max targets of a package and entries of a folder Cargo finds targets in, and those each list is taken with
#define CLI_CARGO_TARGETS 1024u
#define CLI_CARGO_ENTRIES 1024u

// The tables of a manifest that list a binary or a test, as they are read
typedef struct
{
    int bin;
    char name[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    int named;
    int pathed;
} CliCargoListing;

typedef struct
{
    const char *value;
    unsigned long long length;
    char *out;
    unsigned long long capacity;
    const char *text;
    unsigned int type;
    const char *name;
    const char *path;
    unsigned int from;
    unsigned int to;
    const char *dir;
    char *path_out;
    CliCargoListing *tables;
    unsigned int count;
    int *autobins_out;
    int *autotests_out;
    const char *table;
    const char *key;
    const CliCargoListing *listing;
    const char *package;
} CargoTargetsCtx;
// a TOML value as text: a string's contents in either quotes, or the bare word before a comment, into `out`, which
// sized for `capacity`
EMBED_INLINE void found_toml_value(const CargoTargetsCtx *args)
{
    const char *value = args->value;
    unsigned long long length = args->length;
    while ((length > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_POOL_BYTES, .byte = value[0]}) != NULL))
    {
        value += 1;
        length -= 1ULL;
    }
    while ((length > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                   .src = " \t\r", .cap = CLI_POOL_BYTES, .byte = value[length - 1ULL]}) != NULL))
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
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_POOL_BYTES, .byte = value[size - 1ULL]}) != NULL))
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
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_POOL_BYTES, .byte = text[0]}) != NULL))
    {
        text += 1;
        length -= 1ULL;
    }
    while ((length > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r", .cap = CLI_POOL_BYTES, .byte = text[length - 1ULL]}) != NULL))
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
                                 .src = " \t\r\f\v", .cap = CLI_POOL_BYTES, .byte = args->text[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\f\v", .cap = CLI_POOL_BYTES, .byte = args->text[end - 1ULL]}) != NULL))
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
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_POOL_BYTES, .byte = line[key - 1ULL]}) != NULL))
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
// their max
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
    g_cli_guard->persistent_buf_release(s_targets.firsts, g_cli_cargo_owner);
    s_targets.firsts = NULL;
    s_targets.count = 0u;
    s_targets.capacity = 0u;
}

// a target put with its first file
EMBED_INLINE void found_first(const CargoTargetsCtx *args)
{
    unsigned int type = args->type;
    if (s_targets.count < s_targets.capacity)
    {
        CliCargoFirst *const first = &s_targets.firsts[s_targets.count];
        first->type = type;
        memor.cpy(
            &(MemoriaCfg){.dst = first->name,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
        memor.cpy(
            &(MemoriaCfg){.dst = first->path,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
        s_targets.count += 1u;
    }
}

// whether a target's first file is taken already by one the manifest lists, from `from` to `to`
EMBED_INLINE int found_alloc(const CargoTargetsCtx *args)
{
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_targets.firsts[at].path, .other = args->path, .cap = CLI_POOL_BYTES}))
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
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
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
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = entry->path,
                                .src = inside,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = inside, .cap = CLI_POOL_BYTES}) + 1u});
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

// the files Cargo finds on its own in `dir` of the package's folder, by their names, each put as a target of `type`
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
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_targets.entries, .count = s_targets.entry_count,
               .size = sizeof(s_targets.entries[0]), .order = found_entry_order);
    for (unsigned int at = 0u; at < s_targets.entry_count; at += 1u)
    {
        if (!EMBED_CALL(found_alloc, CargoTargetsCtx, .from = args->from, .to = args->to,
                        .path = s_targets.entries[at].path))
        {
            EMBED_CALL(found_first, CargoTargetsCtx, .type = args->type, .name = s_targets.entries[at].name,
                       .path = s_targets.entries[at].path);
        }
    }
}

// a path as Cargo reads one: forward slashes, with no `./` before it
EMBED_INLINE void found_clean(const CargoTargetsCtx *args)
{
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->path_out,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->path_out, .cap = CLI_POOL_BYTES}), .from = '\\',
               .to = '/');
    embed_word start = 0u;
    while ((args->path_out[start] == '.') && (args->path_out[start + 1u] == '/'))
    {
        start += 2u;
    }
    memor.move_down(&(MemoriaCfg){
        .dst = args->path_out,
        .src = args->path_out + start,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path_out + start, .cap = CLI_POOL_BYTES}) + 1u});
}

// a table's header line, the `length` bytes at `text`: its name trimmed into `out`, sized for `capacity`; whether it
// lists a target, an array table [[bin]] or [[test]]
EMBED_INLINE int found_header(const CargoTargetsCtx *args)
{
    const char *const line = args->text;
    const unsigned long long length = args->length;
    const int doubled = (length >= 2ULL) && (line[0] == '[') && (line[1] == '[');
    const unsigned long long open = doubled ? 2ULL : 1ULL;
    const char *const closed =
        doubled ? cellul.find(&(CatenaFinitaCfg){
                      .src = line + open, .cap = CLI_POOL_BYTES, .other = "]]", .other_cap = CLI_POOL_BYTES})
                : memor.chr(&(MemoriaCfg){.src = line + open, .val = ']', .bytes = length - open});
    const unsigned long long end =
        ((closed != NULL) && (closed < line + length)) ? (unsigned long long)(closed - line) : length;
    EMBED_CALL(found_trim, CargoTargetsCtx, .text = line + open, .length = end - open, .out = args->out,
               .capacity = args->capacity);
    return doubled && ((cellul.eq(&(CatenaFinitaCfg){.src = args->out, .other = "bin", .cap = CLI_POOL_BYTES})) ||
                       (cellul.eq(&(CatenaFinitaCfg){.src = args->out, .other = "test", .cap = CLI_POOL_BYTES})));
}

// a `key = value` line read under the table `table`: the package's autobins and autotests into `autobins_out` and
// `autotests_out`, the lib's path into `path_out`, and a listed target's name and path into the last of the `count`
// tables listed in `tables`
EMBED_INLINE void found_setting_put(const CargoTargetsCtx *args)
{
    const char *const table = args->table;
    const char *const key = args->key;
    const char *const value = args->value;
    const int package = cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "package", .cap = CLI_POOL_BYTES});
    const int listing = (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_POOL_BYTES})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "test", .cap = CLI_POOL_BYTES}));
    CliCargoListing *const last = (listing && (args->count > 0u)) ? &args->tables[args->count - 1u] : NULL;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_POOL_BYTES}) + 1u;
    if (package && (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "autobins", .cap = CLI_POOL_BYTES})))
    {
        *args->autobins_out = (!cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "false", .cap = CLI_POOL_BYTES}));
    }
    else if (package && (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "autotests", .cap = CLI_POOL_BYTES})))
    {
        *args->autotests_out = (!cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "false", .cap = CLI_POOL_BYTES}));
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "lib", .cap = CLI_POOL_BYTES})) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_POOL_BYTES})))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->path_out, .src = value, .bytes = size});
        EMBED_CALL(found_clean, CargoTargetsCtx, .path_out = args->path_out);
    }
    else if ((last != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "name", .cap = CLI_POOL_BYTES})))
    {
        memor.cpy(&(MemoriaCfg){.dst = last->name, .src = value, .bytes = size});
        last->named = 1;
    }
    else if ((last != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_POOL_BYTES})))
    {
        memor.cpy(&(MemoriaCfg){.dst = last->path, .src = value, .bytes = size});
        last->pathed = 1;
    }
}

// the manifest, the `length` bytes at `text`, read: the [[bin]] and [[test]] tables it lists into `tables`, at max
// `capacity` of them, the package's autobins and autotests into `autobins_out` and `autotests_out` and the lib's path
// into `path_out`; the count of tables listed
EMBED_INLINE unsigned int found_manifest_read(const CargoTargetsCtx *args)
{
    char table[CLI_NAME_MAX_LEN] = "";
    unsigned int list_entries = 0u;
    unsigned long long at = 0ULL;
    const char *line = NULL;
    unsigned long long length = 0ULL;
    while (EMBED_CALL(cli_cargo_tests_text_internal.cargo_line, CliCargoTestsTextCtx, .text = args->text,
                      .size = args->length, .at = &at, .line = &line, .length = &length))
    {
        const char *const equal = memor.chr(&(MemoriaCfg){.src = line, .val = '=', .bytes = length});
        if ((length >= 1ULL) && (line[0] == '['))
        {
            const int lists = EMBED_CALL(found_header, CargoTargetsCtx, .text = line, .length = length, .out = table,
                                         .capacity = sizeof(table));
            if (lists && (list_entries < args->capacity))
            {
                args->tables[list_entries] = (CliCargoListing){
                    cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_POOL_BYTES}), "", "", 0, 0};
                list_entries += 1u;
            }
        }
        else if (equal != NULL)
        {
            char key[64];
            char value[CLI_PATH_MAX_LEN];
            EMBED_CALL(found_trim, CargoTargetsCtx, .text = line, .length = (unsigned long long)(equal - line),
                       .out = key, .capacity = sizeof(key));
            EMBED_CALL(found_toml_value, CargoTargetsCtx, .value = equal + 1,
                       .length = length - (unsigned long long)(equal - line) - 1ULL, .out = value,
                       .capacity = sizeof(value));
            EMBED_CALL(found_setting_put, CargoTargetsCtx, .table = table, .key = key, .value = value,
                       .tables = args->tables, .count = list_entries, .autobins_out = args->autobins_out,
                       .autotests_out = args->autotests_out, .path_out = args->path_out);
        }
    }
    return list_entries;
}

// the target `listing` lists put among the targets with its first file: the path it gives, or src/main.rs for a binary
// named as the `package` is, or else its name under src/bin or tests
EMBED_INLINE void found_listed(const CargoTargetsCtx *args)
{
    const CliCargoListing *const entry = args->listing;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    if (entry->pathed)
    {
        mmgr_line_put(&path_line, entry->path);
    }
    else if (entry->bin &&
             (cellul.eq(&(CatenaFinitaCfg){.src = entry->name, .other = args->package, .cap = CLI_POOL_BYTES})))
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
    EMBED_CALL(found_first, CargoTargetsCtx, .type = entry->bin ? CLI_CARGO_BIN : CLI_CARGO_TEST, .name = entry->name,
               .path = path);
}

const CliCargoFirst *cli_cargo_targets(const CliCargoTestsTextSizeCtx *args)
{
    const char *folder = args->folder;
    unsigned int *count = args->count;
    cli_cargo_firsts_free();
    s_targets.folder = folder;
    s_targets.firsts = g_cli_guard->temporary_buf_alloc((unsigned long long)CLI_CARGO_TARGETS * sizeof(CliCargoFirst),
                                                        g_cli_cargo_owner);
    s_targets.capacity = (s_targets.firsts != NULL) ? CLI_CARGO_TARGETS : 0u;
    // the folder's entries and the tables the manifest lists, given back as the targets are found
    const unsigned long long entry_bytes = (unsigned long long)CLI_CARGO_ENTRIES * sizeof(CliCargoFirst);
    char *const work = g_cli_guard->temporary_buf_alloc(
        entry_bytes + ((unsigned long long)CLI_CARGO_TARGETS * sizeof(CliCargoListing)), g_cli_cargo_owner);
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
    int autobins = 1;
    int autotests = 1;
    char lib[CLI_PATH_MAX_LEN] = "src/lib.rs";
    const unsigned int list_entries = EMBED_CALL(
        found_manifest_read, CargoTargetsCtx, .text = args->text, .length = args->size, .tables = tables,
        .capacity = listing_capacity, .autobins_out = &autobins, .autotests_out = &autotests, .path_out = lib);
    char lib_full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = lib, .joined = lib_full);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = lib_full))
    {
        EMBED_CALL(found_first, CargoTargetsCtx, .type = CLI_CARGO_LIB, .name = "", .path = lib);
    }
    const unsigned int listing_first = s_targets.count;
    for (unsigned int one = 0u; one < list_entries; one += 1u)
    {
        if (tables[one].named)
        {
            EMBED_CALL(found_listed, CargoTargetsCtx, .listing = &tables[one], .package = args->package);
        }
    }
    const unsigned int reserved = s_targets.count;
    char main_file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "src/main.rs", .joined = main_file);
    if (autobins && EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = main_file) &&
        !EMBED_CALL(found_alloc, CargoTargetsCtx, .from = listing_first, .to = reserved, .path = "src/main.rs"))
    {
        EMBED_CALL(found_first, CargoTargetsCtx, .type = CLI_CARGO_BIN, .name = args->package, .path = "src/main.rs");
    }
    if (autobins)
    {
        EMBED_CALL(found_auto, CargoTargetsCtx, .dir = "src/bin", .type = CLI_CARGO_BIN, .from = listing_first,
                   .to = reserved);
    }
    if (autotests)
    {
        EMBED_CALL(found_auto, CargoTargetsCtx, .dir = "tests", .type = CLI_CARGO_TEST, .from = listing_first,
                   .to = reserved);
    }
    g_cli_guard->persistent_buf_release(work, g_cli_cargo_owner);
    s_targets.entries = NULL;
    s_targets.entry_count = 0u;
    s_targets.entry_capacity = 0u;
    *count = s_targets.count;
    return s_targets.firsts;
}
