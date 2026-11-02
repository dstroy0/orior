// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_cargo.c: the binaries of the package a Cargo.toml describes, as Cargo finds them
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most [[bin]] tables a Cargo.toml is read for
#define CLI_SCAN_BINS 256u

void cli_toml_value(const CliExecutablesLengthValueCtx *args)
{
    const char *value = args->value;
    unsigned long long length = args->length;
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
               .length_pointer = &length);
    if ((length > 0ull) && ((value[0] == '"') || (value[0] == '\'')))
    {
        unsigned long long end = 1ull;
        while ((end < length) && (value[end] != value[0]))
        {
            end += 1ull;
        }
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value + 1,
                   .length = end - 1ull, .out = args->out, .room = args->room);
        return;
    }
    unsigned long long end = 0ull;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = value, .at = end, .size = length,
                     .set = "#");
    length = end;
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
               .length_pointer = &length);
    EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value,
               .length = length, .out = args->out, .room = args->room);
}

// A Cargo.toml's [[bin]] tables: each one's name and path, where it gives them
typedef struct
{
    char name[CLI_SCAN_BINS][CLI_NAME_MAX_LEN];
    char path[CLI_SCAN_BINS][CLI_PATH_MAX_LEN];
    int has_name[CLI_SCAN_BINS];
    int has_path[CLI_SCAN_BINS];
    unsigned int count;
    char taken[CLI_SCAN_BINS][CLI_PATH_MAX_LEN];
    unsigned int takens;
    const char *folder;
    CliNames more;
} CliBins;

// a file or a folder of src/bin as Cargo finds a binary in it
static void cli_bin_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliBins *const bins = (CliBins *)args->context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, bins->folder);
    mmgr_line_put(&line, "/src/bin/");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, args->is_folder ? "/main.rs" : "");
    if ((mmgr_line_finish(&line) == 0ull) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char relative[CLI_PATH_MAX_LEN];
    VerbaLinea relative_line = {relative, sizeof(relative), 0ull};
    mmgr_line_put(&relative_line, "src/bin/");
    mmgr_line_put(&relative_line, name);
    mmgr_line_put(&relative_line, args->is_folder ? "/main.rs" : "");
    mmgr_line_finish(&relative_line);
    if (args->is_folder)
    {
        EMBED_CALL(cli_executables_name_names_internal.names_put, CliExecutablesNameNamesCtx, .names = &bins->more,
                   .name = name, .length = length, .value = relative);
    }
    else if ((length > 3ull) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3ull, .other = ".rs", .cap = CLI_CELLS_POOL})))
    {
        EMBED_CALL(cli_executables_name_names_internal.names_put, CliExecutablesNameNamesCtx, .names = &bins->more,
                   .name = name, .length = length - 3ull, .value = relative);
    }
}

void cli_cargo_bins(const CliExecutablesNamesCtx *args)
{
    static CliBins s_bins;
    static CliLock s_lock = CLI_LOCK_INIT;
    args->names->count = 0u;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliBins *const bins = &s_bins;
    bins->count = 0u;
    bins->takens = 0u;
    bins->more.count = 0u;
    char table[CLI_NAME_MAX_LEN] = "";
    char package[CLI_NAME_MAX_LEN] = "";
    int has_package = 0;
    int autobins = 1;
    const char *line = args->text;
    while (line[0] != '\0')
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *const next = (stop[0] != '\0') ? (stop + 1) : stop;
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        line = next;
        const int doubled = (length >= 2ull) && (from[0] == '[') && (from[1] == '[');
        if (doubled || ((length >= 1ull) && (from[0] == '[')))
        {
            const char *inner = from + (doubled ? 2 : 1);
            unsigned long long size = 0ull;
            size = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = inner, .at = 0ull,
                              .size = (unsigned long long)((from + length) - inner), .set = "]");
            EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx,
                       .text_pointer = &inner, .length_pointer = &size);
            EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = inner,
                       .length = size, .out = table, .room = sizeof(table));
            if (doubled && (cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL})) &&
                (bins->count < CLI_SCAN_BINS))
            {
                bins->has_name[bins->count] = 0;
                bins->has_path[bins->count] = 0;
                bins->count += 1u;
            }
            continue;
        }
        const char *equals = from;
        equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ull,
                             .size = (unsigned long long)((from + length) - equals), .set = "=");
        if (equals == (from + length))
        {
            continue;
        }
        const char *key = from;
        unsigned long long key_length = (unsigned long long)(equals - from);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &key,
                   .length_pointer = &key_length);
        char value[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_executables_length_value_internal.toml_value, CliExecutablesLengthValueCtx, .value = equals + 1,
                   .length = (unsigned long long)((from + length) - (equals + 1)), .out = value, .room = sizeof(value));
        const int is_name =
            (key_length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "name", .bytes = 4u}) == 0);
        const int is_path =
            (key_length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "path", .bytes = 4u}) == 0);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "package", .cap = CLI_CELLS_POOL})) && is_name)
        {
            memor.cpy(&(MemoriaCfg){.dst = package,
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            has_package = 1;
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "package", .cap = CLI_CELLS_POOL})) &&
                 (key_length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "autobins", .bytes = 8u}) == 0))
        {
            autobins = !cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "false", .cap = CLI_CELLS_POOL});
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = table, .other = "bin", .cap = CLI_CELLS_POOL})) &&
                 (bins->count > 0u) && (is_name || is_path))
        {
            const unsigned int last = bins->count - 1u;
            memor.cpy(&(MemoriaCfg){.dst = is_name ? bins->name[last] : bins->path[last],
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            *(is_name ? &bins->has_name[last] : &bins->has_path[last]) = 1;
        }
    }
    if (!has_package)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->manifest, .out = folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    for (unsigned int at = 0u; at < bins->count; at += 1u)
    {
        if (!bins->has_name[at])
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line_made = {path, sizeof(path), 0ull};
        if (bins->has_path[at])
        {
            mmgr_line_put(&line_made, bins->path[at]);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = bins->name[at], .other = package, .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line_made, "src/main.rs");
        }
        else
        {
            mmgr_line_put(&line_made, "src/bin/");
            mmgr_line_put(&line_made, bins->name[at]);
            mmgr_line_put(&line_made, ".rs");
        }
        mmgr_line_finish(&line_made);
        // the path as Cargo reads it: `/` between its parts and no `./` before it
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .from = '\\', .to = '/');
        const char *trimmed = path;
        while (cellul.starts(&(CatenaFinitaCfg){.src = trimmed, .other = "./", .cap = CLI_CELLS_POOL}))
        {
            trimmed += 2;
        }
        if (bins->takens < CLI_SCAN_BINS)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = bins->taken[bins->takens],
                              .src = trimmed,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = trimmed, .cap = CLI_CELLS_POOL}) + 1u});
            bins->takens += 1u;
        }
        if (args->names->count < CLI_EXECUTABLES_NAMES)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->names->name[args->names->count], .bytes = CLI_NAME_MAX_LEN, .src = bins->name[at]});
            args->names->value[args->names->count][0] = '\0';
            args->names->count += 1u;
        }
    }
    if (autobins)
    {
        char main[CLI_PATH_MAX_LEN];
        VerbaLinea main_line = {main, sizeof(main), 0ull};
        mmgr_line_put(&main_line, folder);
        mmgr_line_put(&main_line, "/src/main.rs");
        CliNames found;
        found.count = 0u;
        if ((mmgr_line_finish(&main_line) != 0ull) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = main))
        {
            EMBED_CALL(cli_executables_name_names_internal.names_put, CliExecutablesNameNamesCtx, .names = &found,
                       .name = package, .length = cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_CELLS_POOL}),
                       .value = "src/main.rs");
        }
        char bin_folder[CLI_PATH_MAX_LEN];
        VerbaLinea bin_line = {bin_folder, sizeof(bin_folder), 0ull};
        mmgr_line_put(&bin_line, folder);
        mmgr_line_put(&bin_line, "/src/bin");
        bins->folder = folder;
        if (mmgr_line_finish(&bin_line) != 0ull)
        {
            EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = bin_folder,
                       .each = cli_bin_each, .context_bytess = bins);
        }
        // the folder is the walk's alone: no address of this frame stays in the bins, which outlive it
        bins->folder = NULL;
        // src/bin's binaries in the order of their names, then their paths
        for (unsigned int at = 1u; at < bins->more.count; at += 1u)
        {
            for (unsigned int place = at; place > 0u; place -= 1u)
            {
                const int order =
                    EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = bins->more.name[place - 1u],
                               .other_pointer = bins->more.name[place]);
                if ((order < 0) || ((order == 0) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                                .one = bins->more.value[place - 1u],
                                                                .other_pointer = bins->more.value[place]) <= 0)))
                {
                    break;
                }
                char name[CLI_NAME_MAX_LEN];
                char value[CLI_PATH_MAX_LEN];
                memor.cpy(&(MemoriaCfg){
                    .dst = name,
                    .src = bins->more.name[place],
                    .bytes =
                        cellul.len(&(CatenaFinitaCfg){.src = bins->more.name[place], .cap = CLI_CELLS_POOL}) + 1u});
                memor.cpy(&(MemoriaCfg){
                    .dst = value,
                    .src = bins->more.value[place],
                    .bytes =
                        cellul.len(&(CatenaFinitaCfg){.src = bins->more.value[place], .cap = CLI_CELLS_POOL}) + 1u});
                memor.cpy(&(MemoriaCfg){
                    .dst = bins->more.name[place],
                    .src = bins->more.name[place - 1u],
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = bins->more.name[place - 1u], .cap = CLI_CELLS_POOL}) +
                             1u});
                memor.cpy(&(MemoriaCfg){.dst = bins->more.value[place],
                                        .src = bins->more.value[place - 1u],
                                        .bytes = cellul.len(&(CatenaFinitaCfg){.src = bins->more.value[place - 1u],
                                                                               .cap = CLI_CELLS_POOL}) +
                                                 1u});
                memor.cpy(
                    &(MemoriaCfg){.dst = bins->more.name[place - 1u],
                                  .src = name,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
                memor.cpy(
                    &(MemoriaCfg){.dst = bins->more.value[place - 1u],
                                  .src = value,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
            }
        }
        for (unsigned int at = 0u; at < (found.count + bins->more.count); at += 1u)
        {
            const char *const name = (at < found.count) ? found.name[at] : bins->more.name[at - found.count];
            const char *const path = (at < found.count) ? found.value[at] : bins->more.value[at - found.count];
            int taken = 0;
            for (unsigned int each = 0u; !taken && (each < bins->takens); each += 1u)
            {
                taken = cellul.eq(&(CatenaFinitaCfg){.src = bins->taken[each], .other = path, .cap = CLI_CELLS_POOL});
            }
            if (!taken)
            {
                EMBED_CALL(cli_executables_name_names_internal.names_put, CliExecutablesNameNamesCtx,
                           .names = args->names, .name = name,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .value = NULL);
            }
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
