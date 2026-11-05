// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_lists.c: the catalog's lists and texts, each in a cell of its size counted to the catalog's owner: a
// folder's entries in order, a list's capacity made as it fills, and each given back
#include "../text/text.h"
#include "callback_cfg.h"
#include "catalog_internal.h"

// What a folder's walk includes: its entries, what it includes of them, and the count of those it includes
typedef struct
{
    CliEntries *entries;
    int folders;
    int (*filter)(const CliNameCfg *args);
    unsigned int seen;
} CliEntryWalk;

typedef struct
{
    const CliEntryWalk *walk;
    const char *name;
    int is_folder;
} CatalogListsCtx;
// whether a walk includes the entry `name`
EMBED_INLINE int cli_entry_include(const CatalogListsCtx *args)
{
    const int included = args->walk->folders ? args->is_folder
                                             : (!args->is_folder && (args->walk->filter != NULL) &&
                                                EMBED_CALL(args->walk->filter, CliNameCfg, .name = args->name));
    return included && (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) < CLI_NAME_MAX_LEN);
}

static void cli_entry_count(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    CliEntryWalk *const walk = (CliEntryWalk *)args->context;
    walk->seen +=
        EMBED_CALL(cli_entry_include, CatalogListsCtx, .walk = walk, .name = name, .is_folder = is_folder) ? 1u : 0u;
}

static void cli_entry_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    CliEntryWalk *const walk = (CliEntryWalk *)args->context;
    CliEntries *const entries = walk->entries;
    if (!EMBED_CALL(cli_entry_include, CatalogListsCtx, .walk = walk, .name = name, .is_folder = is_folder) ||
        (entries->count >= entries->capacity))
    {
        return;
    }
    unsigned int place = entries->count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = entries->name[place - 1u],
                                       .other_pointer = name) > 0))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = entries->name[place], .bytes = CLI_NAME_MAX_LEN, .src = entries->name[place - 1u]});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = entries->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    entries->count += 1u;
}

void cli_catalog_entries(const CliCatalogFolderCtx *args)
{
    CliEntries *entries = args->entries;
    g_cli_cells->persistent_buf_release(entries->name, &g_cli_catalog_cells);
    entries->name = NULL;
    entries->count = 0u;
    entries->capacity = 0u;
    CliEntryWalk walk = {entries, args->folders, args->filter, 0u};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_entry_count, .context_bytess = &walk);
    (void)EMBED_CALL(cli_catalog_names_internal.names_capacity, CliCatalogNamesCtx, .names = entries,
                     .most = walk.seen);
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_entry_each, .context_bytess = &walk);
}

// the most names or paths one list contains, its cell taken at them as the list is first made capacity in
#define CLI_CATALOG_ENTRIES_MAX 16384u

int cli_catalog_names_capacity(const CliCatalogNamesCtx *args)
{
    if ((args->names->name == NULL) && (args->most <= CLI_CATALOG_ENTRIES_MAX))
    {
        args->names->name = g_cli_cells->temporary_buf_alloc(
            (unsigned long long)CLI_CATALOG_ENTRIES_MAX * CLI_NAME_MAX_LEN, &g_cli_catalog_cells);
        args->names->capacity = (args->names->name != NULL) ? CLI_CATALOG_ENTRIES_MAX : 0u;
    }
    return args->most <= args->names->capacity;
}

int cli_catalog_paths_capacity(const CliCatalogMaxCtx *args)
{
    if ((args->paths->path == NULL) && (args->most <= CLI_CATALOG_ENTRIES_MAX))
    {
        args->paths->path = g_cli_cells->temporary_buf_alloc(
            (unsigned long long)CLI_CATALOG_ENTRIES_MAX * CLI_PATH_MAX_LEN, &g_cli_catalog_cells);
        args->paths->capacity = (args->paths->path != NULL) ? CLI_CATALOG_ENTRIES_MAX : 0u;
    }
    return args->most <= args->paths->capacity;
}
