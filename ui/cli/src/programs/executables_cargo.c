// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_cargo.c: the binaries of the package a Cargo.toml describes, as Cargo finds them
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "executables_internal.h"

// the max [[bin]] tables a Cargo.toml is read for
#define CLI_SCAN_BINS 256u

void cli_toml_value(const CliExecutablesLengthValueCtx *args)
{
    const char *value = args->value;
    unsigned long long length = args->length;
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
               .length_pointer = &length);
    if ((length > 0ULL) && ((value[0] == '"') || (value[0] == '\'')))
    {
        unsigned long long end = 1ULL;
        while ((end < length) && (value[end] != value[0]))
        {
            end += 1ULL;
        }
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value + 1,
                   .length = end - 1ULL, .out = args->out, .capacity = args->capacity);
        return;
    }
    unsigned long long end = 0ULL;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = value, .at = end, .size = length,
                     .set = "#");
    length = end;
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
               .length_pointer = &length);
    EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value,
               .length = length, .out = args->out, .capacity = args->capacity);
}

// A Cargo.toml's [[bin]] tables: each one's name and path, where it gives them
typedef struct
{
    char name[CLI_SCAN_BINS][CLI_NAME_MAX_LEN];
    char path[CLI_SCAN_BINS][CLI_PATH_MAX_LEN];
    int has_name[CLI_SCAN_BINS];
    int has_path[CLI_SCAN_BINS];
    unsigned int count;
    char reserved[CLI_SCAN_BINS][CLI_PATH_MAX_LEN];
    unsigned int allocations;
    const char *folder;
    CliNames more;
} CliBins;

// a file or a folder of src/bin as Cargo finds a binary in it
static void cli_bin_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliBins *const bins = (CliBins *)args->context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, bins->folder);
    mmgr_line_put(&line, "/src/bin/");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, args->is_folder ? "/main.rs" : "");
    if ((mmgr_line_finish(&line) == 0ULL) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
    char relative[CLI_PATH_MAX_LEN];
    VerbaLinea relative_line = {relative, sizeof(relative), 0ULL};
    mmgr_line_put(&relative_line, "src/bin/");
    mmgr_line_put(&relative_line, name);
    mmgr_line_put(&relative_line, args->is_folder ? "/main.rs" : "");
    mmgr_line_finish(&relative_line);
    if (args->is_folder)
    {
        EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx, .names = &bins->more,
                   .name = name, .length = length, .value = relative);
    }
    else if ((length > 3ULL) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3ULL, .other = ".rs", .cap = CLI_POOL_BYTES})))
    {
        EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx, .names = &bins->more,
                   .name = name, .length = length - 3ULL, .value = relative);
    }
}

// A Cargo.toml read for its binaries: its [[bin]] tables; the table a line is in, the package's name and whether
// one is given, and whether Cargo finds binaries itself; a line, `length` bytes from `from`; the folder the manifest
// is in; the names found, those Cargo finds itself, and the place of one of src/bin's binaries
typedef struct
{
    CliBins *bins;
    char *table;
    char *package;
    int *has_package;
    int *autobins;
    const char *from;
    unsigned long long length;
    const char *folder;
    CliNames *names;
    unsigned int place;
} ExecutablesCargoCtx;

// a line of the manifest that opens a table: the table's name, the lines after it are in, and a [[bin]] put after
// those before it; 1, or 0 where the line opens none
EMBED_INLINE int cargo_table_opened(const ExecutablesCargoCtx *args)
{
    const char *from = args->from;
    const unsigned long long length = args->length;
    CliBins *const bins = args->bins;
    const int doubled = (length >= 2ULL) && (from[0] == '[') && (from[1] == '[');
    if (!doubled && ((length == 0ULL) || (from[0] != '[')))
    {
        return 0;
    }
    const char *inner = from + (doubled ? 2 : 1);
    unsigned long long size = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = inner,
                                         .at = 0ULL, .size = (unsigned long long)((from + length) - inner), .set = "]");
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &inner,
               .length_pointer = &size);
    EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = inner,
               .length = size, .out = args->table, .capacity = CLI_NAME_MAX_LEN);
    if (doubled && (cellul.eq(&(CatenaFinitaCfg){.src = args->table, .other = "bin", .cap = CLI_POOL_BYTES})) &&
        (bins->count < CLI_SCAN_BINS))
    {
        bins->has_name[bins->count] = 0;
        bins->has_path[bins->count] = 0;
        bins->count += 1u;
    }
    return 1;
}

// a line of the manifest that gives a key its value, read in the table it is in: the package's name and whether
// Cargo finds binaries itself, and a [[bin]]'s name and path
EMBED_INLINE void cargo_key_read(const ExecutablesCargoCtx *args)
{
    const char *from = args->from;
    const unsigned long long length = args->length;
    CliBins *const bins = args->bins;
    const char *equals = from;
    equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ULL,
                         .size = length, .set = "=");
    if (equals == (from + length))
    {
        return;
    }
    const char *key = from;
    unsigned long long key_length = (unsigned long long)(equals - from);
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &key,
               .length_pointer = &key_length);
    char value[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_length_value_internal.toml_value, CliExecutablesLengthValueCtx, .value = equals + 1,
               .length = (unsigned long long)((from + length) - (equals + 1)), .out = value, .capacity = sizeof(value));
    const int is_name =
        (key_length == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "name", .bytes = 4u}) == 0);
    const int is_path =
        (key_length == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "path", .bytes = 4u}) == 0);
    const int in_package = cellul.eq(&(CatenaFinitaCfg){.src = args->table, .other = "package", .cap = CLI_POOL_BYTES});
    const unsigned long long value_bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_POOL_BYTES}) + 1u;
    if (in_package && is_name)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->package, .src = value, .bytes = value_bytes});
        *args->has_package = 1;
    }
    else if (in_package && (key_length == 8ULL) &&
             (memor.cmp(&(MemoriaCfg){.src = key, .other = "autobins", .bytes = 8u}) == 0))
    {
        *args->autobins = !cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "false", .cap = CLI_POOL_BYTES});
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = args->table, .other = "bin", .cap = CLI_POOL_BYTES})) &&
             (bins->count > 0u) && (is_name || is_path))
    {
        const unsigned int last = bins->count - 1u;
        memor.cpy(
            &(MemoriaCfg){.dst = is_name ? bins->name[last] : bins->path[last], .src = value, .bytes = value_bytes});
        *(is_name ? &bins->has_name[last] : &bins->has_path[last]) = 1;
    }
}

// the binaries the [[bin]] tables name put among `names`, each table with a name, the path each is read from stored so
// that none of src/bin's is named again
EMBED_INLINE void cargo_bins_listed(const ExecutablesCargoCtx *args)
{
    CliBins *const bins = args->bins;
    for (unsigned int at = 0u; at < bins->count; at += 1u)
    {
        if (!bins->has_name[at])
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line_created = {path, sizeof(path), 0ULL};
        if (bins->has_path[at])
        {
            mmgr_line_put(&line_created, bins->path[at]);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = bins->name[at], .other = args->package, .cap = CLI_POOL_BYTES}))
        {
            mmgr_line_put(&line_created, "src/main.rs");
        }
        else
        {
            mmgr_line_put(&line_created, "src/bin/");
            mmgr_line_put(&line_created, bins->name[at]);
            mmgr_line_put(&line_created, ".rs");
        }
        mmgr_line_finish(&line_created);
        // the path as Cargo reads it: `/` between its parts and no `./` before it
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}), .from = '\\', .to = '/');
        const char *trimmed = path;
        while (cellul.starts(&(CatenaFinitaCfg){.src = trimmed, .other = "./", .cap = CLI_POOL_BYTES}))
        {
            trimmed += 2;
        }
        if (bins->allocations < CLI_SCAN_BINS)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = bins->reserved[bins->allocations],
                              .src = trimmed,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = trimmed, .cap = CLI_POOL_BYTES}) + 1u});
            bins->allocations += 1u;
        }
        if (args->names->count < CLI_EXECUTABLES_NAMES)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->names->name[args->names->count], .bytes = CLI_NAME_MAX_LEN, .src = bins->name[at]});
            args->names->value[args->names->count][0] = '\0';
            args->names->count += 1u;
        }
    }
}

// whether src/bin's binary at `place` comes after the one before it, by its name, then its path
EMBED_INLINE int cargo_more_after(const ExecutablesCargoCtx *args)
{
    const CliNames *const more = &args->bins->more;
    const unsigned int place = args->place;
    const int order = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = more->name[place - 1u],
                                 .other_pointer = more->name[place]);
    return (order > 0) ||
           ((order == 0) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = more->value[place - 1u],
                                        .other_pointer = more->value[place]) > 0));
}

// src/bin's binary at `place` and the one before it each put in the other's place
EMBED_INLINE void cargo_more_swapped(const ExecutablesCargoCtx *args)
{
    CliNames *const more = &args->bins->more;
    const unsigned int place = args->place;
    char name[CLI_NAME_MAX_LEN];
    char value[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = name, .src = more->name[place], .bytes = CLI_NAME_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = value, .src = more->value[place], .bytes = CLI_PATH_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = more->name[place], .src = more->name[place - 1u], .bytes = CLI_NAME_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = more->value[place], .src = more->value[place - 1u], .bytes = CLI_PATH_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = more->name[place - 1u], .src = name, .bytes = CLI_NAME_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = more->value[place - 1u], .src = value, .bytes = CLI_PATH_MAX_LEN});
}

// the binaries Cargo finds itself put among `names`: src/main.rs as the package's, then src/bin's files and folders in
// the order of their names, then their paths, each a [[bin]] table reads passed over
EMBED_INLINE void cargo_bins_found(const ExecutablesCargoCtx *args)
{
    CliBins *const bins = args->bins;
    char main[CLI_PATH_MAX_LEN];
    VerbaLinea main_line = {main, sizeof(main), 0ULL};
    mmgr_line_put(&main_line, args->folder);
    mmgr_line_put(&main_line, "/src/main.rs");
    CliNames found;
    found.count = 0u;
    if ((mmgr_line_finish(&main_line) != 0ULL) &&
        EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = main))
    {
        EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx, .names = &found,
                   .name = args->package,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->package, .cap = CLI_POOL_BYTES}),
                   .value = "src/main.rs");
    }
    char bin_folder[CLI_PATH_MAX_LEN];
    VerbaLinea bin_line = {bin_folder, sizeof(bin_folder), 0ULL};
    mmgr_line_put(&bin_line, args->folder);
    mmgr_line_put(&bin_line, "/src/bin");
    bins->folder = args->folder;
    if (mmgr_line_finish(&bin_line) != 0ULL)
    {
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = bin_folder,
                   .each = cli_bin_each, .context_bytess = bins);
    }
    // the folder is the walk's alone: no address of the caller's frame stays in the bins, which outlive it
    bins->folder = NULL;
    for (unsigned int at = 1u; at < bins->more.count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && EMBED_CALL(cargo_more_after, ExecutablesCargoCtx, .bins = bins, .place = place))
        {
            EMBED_CALL(cargo_more_swapped, ExecutablesCargoCtx, .bins = bins, .place = place);
            place -= 1u;
        }
    }
    for (unsigned int at = 0u; at < (found.count + bins->more.count); at += 1u)
    {
        const char *const name = (at < found.count) ? found.name[at] : bins->more.name[at - found.count];
        const char *const path = (at < found.count) ? found.value[at] : bins->more.value[at - found.count];
        int reserved = 0;
        for (unsigned int each = 0u; !reserved && (each < bins->allocations); each += 1u)
        {
            reserved = cellul.eq(&(CatenaFinitaCfg){.src = bins->reserved[each], .other = path, .cap = CLI_POOL_BYTES});
        }
        if (!reserved)
        {
            EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                       .names = args->names, .name = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .value = NULL);
        }
    }
}

void cli_cargo_bins(const CliExecutablesNamesCtx *args)
{
    static CliBins s_bins;
    args->names->count = 0u;
    CliBins *const bins = &s_bins;
    bins->count = 0u;
    bins->allocations = 0u;
    bins->more.count = 0u;
    char table[CLI_NAME_MAX_LEN] = "";
    char package[CLI_NAME_MAX_LEN] = "";
    int has_package = 0;
    int autobins = 1;
    const char *line = args->text;
    while (line[0] != '\0')
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_POOL_BYTES}), .set = "\n");
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        if (!EMBED_CALL(cargo_table_opened, ExecutablesCargoCtx, .bins = bins, .table = table, .from = from,
                        .length = length))
        {
            EMBED_CALL(cargo_key_read, ExecutablesCargoCtx, .bins = bins, .table = table, .package = package,
                       .has_package = &has_package, .autobins = &autobins, .from = from, .length = length);
        }
    }
    if (!has_package)
    {
        return;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->manifest, .out = folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = ".", .bytes = 2u});
    }
    EMBED_CALL(cargo_bins_listed, ExecutablesCargoCtx, .bins = bins, .package = package, .names = args->names);
    if (autobins)
    {
        EMBED_CALL(cargo_bins_found, ExecutablesCargoCtx, .bins = bins, .package = package, .folder = folder,
                   .names = args->names);
    }
}
