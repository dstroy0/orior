// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_lists.c: the catalog's lists and texts, each in a cell of its size counted to the catalog's tally: a
// folder's entries in order, a list's room made as it fills, and each given back
#include "../text/text.h"
#include "catalog_internal.h"

CliCellsTally g_cli_catalog_cells = CLI_CELLS_TALLY("cli_catalog");

// What a folder's walk keeps: its entries, what it keeps of them, and the count of those it keeps
typedef struct
{
    CliEntries *entries;
    int folders;
    int (*keep)(const char *name);
    unsigned int seen;
} CliEntryWalk;

// whether a walk keeps the entry `name`
static int cli_entry_kept(const CliEntryWalk *walk, const char *name, int is_folder)
{
    const int kept = walk->folders ? is_folder : (!is_folder && (walk->keep != NULL) && walk->keep(name));
    return kept && (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) < CLI_NAME_LONGEST);
}

static void cli_entry_counted(void *context, const char *name, int is_folder)
{
    CliEntryWalk *const walk = (CliEntryWalk *)context;
    walk->seen += cli_entry_kept(walk, name, is_folder) ? 1u : 0u;
}

static void cli_entry_each(void *context, const char *name, int is_folder)
{
    CliEntryWalk *const walk = (CliEntryWalk *)context;
    CliEntries *const entries = walk->entries;
    if (!cli_entry_kept(walk, name, is_folder) || (entries->count >= entries->room))
    {
        return;
    }
    unsigned int place = entries->count;
    while ((place > 0u) && (cli_text_order(entries->name[place - 1u], name) > 0))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = entries->name[place], .bytes = CLI_NAME_LONGEST, .src = entries->name[place - 1u]});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = entries->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    entries->count += 1u;
}

void cli_catalog_entries(const char *folder, int folders, int (*keep)(const char *name), CliEntries *entries)
{
    cli_catalog_entries_given(entries);
    CliEntryWalk walk = {entries, folders, keep, 0u};
    query_folder_each(folder, cli_entry_counted, &walk);
    (void)cli_catalog_names_room(entries, walk.seen);
    query_folder_each(folder, cli_entry_each, &walk);
}

// a cell of `most` texts of `width` bytes, or of twice `*room` where that is more, holding the `*room` texts of `cell`,
// `cell` given back and `*room` raised: the cell, or NULL where it is refused and `cell` is kept
static void *cli_catalog_grown(void *cell, unsigned int *room, unsigned int most, unsigned long long width,
                               const char *what)
{
    const unsigned int doubled = 2u * *room;
    const unsigned int taken = (most > doubled) ? most : doubled;
    void *const grown = cli_cell_taken_by(&g_cli_catalog_cells, (unsigned long long)taken * width, what);
    if (grown == NULL)
    {
        return NULL;
    }
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = cell, .bytes = (size_t)((unsigned long long)*room * width)});
        cli_cell_given_by(&g_cli_catalog_cells, cell);
    }
    *room = taken;
    return grown;
}

int cli_catalog_names_room(CliEntries *names, unsigned int most)
{
    if (most <= names->room)
    {
        return 1;
    }
    void *const grown = cli_catalog_grown(names->name, &names->room, most, CLI_NAME_LONGEST, "a list of names");
    names->name = (grown != NULL) ? grown : names->name;
    return grown != NULL;
}

int cli_catalog_paths_room(CliPaths *paths, unsigned int most)
{
    if (most <= paths->room)
    {
        return 1;
    }
    void *const grown = cli_catalog_grown(paths->path, &paths->room, most, CLI_PATH_LONGEST, "a list of paths");
    paths->path = (grown != NULL) ? grown : paths->path;
    return grown != NULL;
}

void cli_catalog_entries_given(CliEntries *entries)
{
    cli_cell_given_by(&g_cli_catalog_cells, entries->name);
    entries->name = NULL;
    entries->count = 0u;
    entries->room = 0u;
}

void cli_catalog_paths_given(CliPaths *paths)
{
    cli_cell_given_by(&g_cli_catalog_cells, paths->path);
    paths->path = NULL;
    paths->room = 0u;
}

void cli_catalog_text_given(char *text)
{
    cli_cell_given_by(&g_cli_catalog_cells, text);
}
