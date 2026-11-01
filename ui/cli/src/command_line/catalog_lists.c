// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_lists.c: the catalog's lists and texts, each in a cell of its size counted to the catalog's owner: a
// folder's entries in order, a list's room made as it fills, and each given back
#include "../text/text.h"
#include "catalog_internal.h"

// What a folder's walk includes: its entries, what it includes of them, and the count of those it includes
typedef struct
{
    CliEntries *entries;
    int folders;
    int (*filter)(const char *name);
    unsigned int seen;
} CliEntryWalk;

// whether a walk includes the entry `name`
static int cli_entry_included(const CliEntryWalk *walk, const char *name, int is_folder)
{
    const int included = walk->folders ? is_folder : (!is_folder && (walk->filter != NULL) && walk->filter(name));
    return included && (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) < CLI_NAME_MAX_LEN);
}

static void cli_entry_counted(void *context, const char *name, int is_folder)
{
    CliEntryWalk *const walk = (CliEntryWalk *)context;
    walk->seen += cli_entry_included(walk, name, is_folder) ? 1u : 0u;
}

static void cli_entry_each(void *context, const char *name, int is_folder)
{
    CliEntryWalk *const walk = (CliEntryWalk *)context;
    CliEntries *const entries = walk->entries;
    if (!cli_entry_included(walk, name, is_folder) || (entries->count >= entries->room))
    {
        return;
    }
    unsigned int place = entries->count;
    while ((place > 0u) && (cli_text_order(entries->name[place - 1u], name) > 0))
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

void cli_catalog_entries(const char *folder, int folders, int (*filter)(const char *name), CliEntries *entries)
{
    cli_catalog_entries_given(entries);
    CliEntryWalk walk = {entries, folders, filter, 0u};
    query_folder_each(folder, cli_entry_counted, &walk);
    (void)cli_catalog_names_room(entries, walk.seen);
    query_folder_each(folder, cli_entry_each, &walk);
}

// the most names or paths one list contains, its cell taken at them as the list is first made room in
#define CLI_CATALOG_ENTRIES_MOST 16384u

int cli_catalog_names_room(CliEntries *names, unsigned int most)
{
    if ((names->name == NULL) && (most <= CLI_CATALOG_ENTRIES_MOST))
    {
        names->name = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_CATALOG_ENTRIES_MOST * CLI_NAME_MAX_LEN,
                                                       &g_cli_catalog_cells);
        names->room = (names->name != NULL) ? CLI_CATALOG_ENTRIES_MOST : 0u;
    }
    return most <= names->room;
}

int cli_catalog_paths_room(CliPaths *paths, unsigned int most)
{
    if ((paths->path == NULL) && (most <= CLI_CATALOG_ENTRIES_MOST))
    {
        paths->path = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_CATALOG_ENTRIES_MOST * CLI_PATH_MAX_LEN,
                                                       &g_cli_catalog_cells);
        paths->room = (paths->path != NULL) ? CLI_CATALOG_ENTRIES_MOST : 0u;
    }
    return most <= paths->room;
}

void cli_catalog_entries_given(CliEntries *entries)
{
    g_cli_cells->persistent_buf_release(entries->name, &g_cli_catalog_cells);
    entries->name = NULL;
    entries->count = 0u;
    entries->room = 0u;
}

void cli_catalog_paths_given(CliPaths *paths)
{
    g_cli_cells->persistent_buf_release(paths->path, &g_cli_catalog_cells);
    paths->path = NULL;
    paths->room = 0u;
}

void cli_catalog_text_given(char *text)
{
    g_cli_cells->persistent_buf_release(text, &g_cli_catalog_cells);
}
