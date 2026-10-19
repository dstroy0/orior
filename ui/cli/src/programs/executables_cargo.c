// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_cargo.c: the binaries of the package a Cargo.toml describes, as Cargo finds them
#include "../text/text.h"
#include "executables_internal.h"

// the most [[bin]] tables a Cargo.toml is read for
#define CLI_SCAN_BINS 256u

void cli_toml_value(const char *value, unsigned long long length, char *out, unsigned long long room)
{
    cli_scan_trim(&value, &length);
    if ((length > 0ull) && ((value[0] == '"') || (value[0] == '\'')))
    {
        unsigned long long end = 1ull;
        while ((end < length) && (value[end] != value[0]))
        {
            end += 1ull;
        }
        cli_scan_copied(value + 1, end - 1ull, out, room);
        return;
    }
    unsigned long long end = 0ull;
    while ((end < length) && (value[end] != '#'))
    {
        end += 1ull;
    }
    length = end;
    cli_scan_trim(&value, &length);
    cli_scan_copied(value, length, out, room);
}

// A Cargo.toml's [[bin]] tables: each one's name and path, where it gives them
typedef struct
{
    char name[CLI_SCAN_BINS][CLI_NAME_LONGEST];
    char path[CLI_SCAN_BINS][CLI_PATH_LONGEST];
    int has_name[CLI_SCAN_BINS];
    int has_path[CLI_SCAN_BINS];
    unsigned int count;
    char taken[CLI_SCAN_BINS][CLI_PATH_LONGEST];
    unsigned int takens;
    const char *folder;
    CliNames more;
} CliBins;

// a file or a folder of src/bin as Cargo finds a binary in it
static void cli_bin_each(void *context, const char *name, int is_folder)
{
    CliBins *const bins = (CliBins *)context;
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, bins->folder);
    cli_line_text(&line, "/src/bin/");
    cli_line_text(&line, name);
    cli_line_text(&line, is_folder ? "/main.rs" : "");
    if ((cli_line_closed(&line) == 0ull) || !query_is_file(path))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char relative[CLI_PATH_LONGEST];
    CliLine kept = {relative, sizeof(relative), 0ull};
    cli_line_text(&kept, "src/bin/");
    cli_line_text(&kept, name);
    cli_line_text(&kept, is_folder ? "/main.rs" : "");
    cli_line_closed(&kept);
    if (is_folder)
    {
        cli_names_put(&bins->more, name, length, relative);
    }
    else if ((length > 3ull) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3ull, .other = ".rs", .cap = CLI_CELLS_POOL})))
    {
        cli_names_put(&bins->more, name, length - 3ull, relative);
    }
}

void cli_cargo_bins(const char *manifest, const char *text, CliNames *names)
{
    static CliBins s_bins;
    static CliLock s_lock = CLI_LOCK_INIT;
    names->count = 0u;
    cli_lock_take(&s_lock);
    CliBins *const bins = &s_bins;
    bins->count = 0u;
    bins->takens = 0u;
    bins->more.count = 0u;
    char table[CLI_NAME_LONGEST] = "";
    char package[CLI_NAME_LONGEST] = "";
    int has_package = 0;
    int autobins = 1;
    const char *line = text;
    while (line[0] != '\0')
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = (stop[0] != '\0') ? (stop + 1) : stop;
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        cli_scan_trim(&from, &length);
        line = next;
        const int doubled = (length >= 2ull) && (from[0] == '[') && (from[1] == '[');
        if (doubled || ((length >= 1ull) && (from[0] == '[')))
        {
            const char *inner = from + (doubled ? 2 : 1);
            unsigned long long size = 0ull;
            while ((inner + size) < (from + length) && (inner[size] != ']'))
            {
                size += 1ull;
            }
            cli_scan_trim(&inner, &size);
            cli_scan_copied(inner, size, table, sizeof(table));
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
        while ((equals < (from + length)) && (equals[0] != '='))
        {
            equals += 1;
        }
        if (equals == (from + length))
        {
            continue;
        }
        const char *key = from;
        unsigned long long key_length = (unsigned long long)(equals - from);
        cli_scan_trim(&key, &key_length);
        char value[CLI_PATH_LONGEST];
        cli_toml_value(equals + 1, (unsigned long long)((from + length) - (equals + 1)), value, sizeof(value));
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
        cli_lock_give(&s_lock);
        return;
    }
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(manifest, folder))
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
        char path[CLI_PATH_LONGEST];
        CliLine line_made = {path, sizeof(path), 0ull};
        if (bins->has_path[at])
        {
            cli_line_text(&line_made, bins->path[at]);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = bins->name[at], .other = package, .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line_made, "src/main.rs");
        }
        else
        {
            cli_line_text(&line_made, "src/bin/");
            cli_line_text(&line_made, bins->name[at]);
            cli_line_text(&line_made, ".rs");
        }
        cli_line_closed(&line_made);
        // the path as Cargo reads it: `/` between its parts and no `./` before it
        for (char *slash = path; slash[0] != '\0'; slash += 1)
        {
            slash[0] = (slash[0] == '\\') ? '/' : slash[0];
        }
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
        if (names->count < CLI_EXECUTABLES_NAMES)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = names->name[names->count], .bytes = CLI_NAME_LONGEST, .src = bins->name[at]});
            names->value[names->count][0] = '\0';
            names->count += 1u;
        }
    }
    if (autobins)
    {
        char main[CLI_PATH_LONGEST];
        CliLine main_line = {main, sizeof(main), 0ull};
        cli_line_text(&main_line, folder);
        cli_line_text(&main_line, "/src/main.rs");
        CliNames found;
        found.count = 0u;
        if ((cli_line_closed(&main_line) != 0ull) && query_is_file(main))
        {
            cli_names_put(&found, package, cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_CELLS_POOL}),
                          "src/main.rs");
        }
        char bin_folder[CLI_PATH_LONGEST];
        CliLine bin_line = {bin_folder, sizeof(bin_folder), 0ull};
        cli_line_text(&bin_line, folder);
        cli_line_text(&bin_line, "/src/bin");
        bins->folder = folder;
        if (cli_line_closed(&bin_line) != 0ull)
        {
            query_folder_each(bin_folder, cli_bin_each, bins);
        }
        // src/bin's binaries in the order of their names, then their paths
        for (unsigned int at = 1u; at < bins->more.count; at += 1u)
        {
            for (unsigned int place = at; place > 0u; place -= 1u)
            {
                const int order = cli_text_order(bins->more.name[place - 1u], bins->more.name[place]);
                if ((order < 0) ||
                    ((order == 0) && (cli_text_order(bins->more.value[place - 1u], bins->more.value[place]) <= 0)))
                {
                    break;
                }
                char name[CLI_NAME_LONGEST];
                char value[CLI_PATH_LONGEST];
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
                cli_names_put(names, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), NULL);
            }
        }
    }
    cli_lock_give(&s_lock);
}
