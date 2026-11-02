// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_explorer.c: the explorer's folders and files, a folder at a time as each is opened: a listing read on the
// files' lane stands as nodes under its folder, a level deeper, in the listing's order, kept as the folder closes and
// opens again. Where each row stands and which the pointer marks are the window's
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../text/text.h"

// A node, a folder or file of the tree: its path's and its name's starts in the text, how deep it stands, whether it is
// a folder, whether git ignores it, whether it is open, whether its entries are listed and whether they have been asked
// for
typedef struct
{
    unsigned long long path;
    unsigned long long name;
    unsigned int depth;
    int dir;
    int ignored;
    int open;
    int listed;
    int asked;
} ControllerNode;

// The explorer: the nodes in the tree's order, a folder's entries after it, and their capacity; the text their paths
// and names stand in, how much is used and its capacity; and whether the top folder's listing was asked for
typedef struct
{
    ControllerNode *node;
    unsigned int nodes;
    unsigned int nodes_capacity;
    char *text;
    unsigned long long used;
    unsigned long long text_capacity;
    int root_asked;
} ControllerExplorer;

static ControllerExplorer s_explorer;

// A listing being read on the files' lane: the tree and the folder after it in its cell, and the entries it found
typedef struct
{
    const char *root;
    const char *dir;
    CliFileEntries *entries;
} ControllerListing;

typedef struct
{
    const char *dir;
    const char *text;
    unsigned long long size;
    const CliFileEntry *entry;
    unsigned int at;
    unsigned int depth;
    unsigned int row;
    unsigned int more_nodes;
    unsigned long long more_text;
} ControllerExplorerCtx;

// the node's cells grown to hold `more_nodes` nodes and `more_text` bytes past those used: 1, or 0 where the cells
// cannot meet it
EMBED_INLINE int explorer_room(const ControllerExplorerCtx *args)
{
    if ((s_explorer.nodes + args->more_nodes) > s_explorer.nodes_capacity)
    {
        const unsigned int capacity = (s_explorer.nodes + args->more_nodes) * 2u;
        ControllerNode *const grown =
            g_cli_cells->persistent_buf_alloc(capacity * sizeof(*grown), &g_cli_controller_cells);
        if (grown == NULL)
        {
            return 0;
        }
        if (s_explorer.node != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_explorer.node, .bytes = s_explorer.nodes * sizeof(*grown)});
        }
        g_cli_cells->persistent_buf_release(s_explorer.node, &g_cli_controller_cells);
        s_explorer.node = grown;
        s_explorer.nodes_capacity = capacity;
    }
    if ((s_explorer.used + args->more_text) > s_explorer.text_capacity)
    {
        const unsigned long long capacity = (s_explorer.used + args->more_text) * 2ull;
        char *const grown = g_cli_cells->persistent_buf_alloc(capacity, &g_cli_controller_cells);
        if (grown == NULL)
        {
            return 0;
        }
        if (s_explorer.text != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_explorer.text, .bytes = s_explorer.used});
        }
        g_cli_cells->persistent_buf_release(s_explorer.text, &g_cli_controller_cells);
        s_explorer.text = grown;
        s_explorer.text_capacity = capacity;
    }
    return 1;
}

// a text saved among the explorer's: where it starts
EMBED_INLINE unsigned long long explorer_text_append(const ControllerExplorerCtx *args)
{
    const unsigned long long at = s_explorer.used;
    memor.cpy(&(MemoriaCfg){.dst = s_explorer.text + at, .src = args->text, .bytes = args->size});
    s_explorer.text[at + args->size] = '\0';
    s_explorer.used += args->size + 1ull;
    return at;
}

// the node whose path is `dir`, or the nodes' count where none is
EMBED_INLINE unsigned int explorer_node_at(const ControllerExplorerCtx *args)
{
    for (unsigned int at = 0u; at < s_explorer.nodes; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = s_explorer.text + s_explorer.node[at].path, .other = args->dir, .cap = CLI_PATH_MAX_LEN}))
        {
            return at;
        }
    }
    return s_explorer.nodes;
}

// what runs on the files' lane: the folder listed
static void explorer_list_run(void *context)
{
    ControllerListing *const listing = (ControllerListing *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    listing->entries = EMBED_CALL(cli_files_listing.entries_alloc, CliFilesEntriesCfg, .root = listing->root,
                                  .dir = listing->dir, .owner = &g_cli_controller_cells, .err_msg = &err_msg);
}

// what comes back on the controller's thread: the entries stood under their folder
static void explorer_list_done(void *context)
{
    ControllerListing *const listing = (ControllerListing *)context;
    const CliFileEntries *const entries = listing->entries;
    unsigned int at = s_explorer.nodes;
    unsigned int depth = 0u;
    int stands = listing->dir[0] == '\0';
    if (!stands)
    {
        const unsigned int parent = EMBED_CALL(explorer_node_at, ControllerExplorerCtx, .dir = listing->dir);
        stands = parent < s_explorer.nodes;
        if (stands)
        {
            s_explorer.node[parent].listed = 1;
            s_explorer.node[parent].asked = 0;
            at = parent + 1u;
            depth = s_explorer.node[parent].depth + 1u;
        }
    }
    unsigned long long bytes = 0ull;
    for (unsigned int each = 0u; (entries != NULL) && (each < entries->count); each += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = entries->entry[each].path, .cap = CLI_PATH_MAX_LEN}) +
                 cellul.len(&(CatenaFinitaCfg){.src = entries->entry[each].name, .cap = CLI_PATH_MAX_LEN}) + 2ull;
    }
    if (stands && (entries != NULL) && (entries->count != 0u) &&
        EMBED_CALL(explorer_room, ControllerExplorerCtx, .more_nodes = entries->count, .more_text = bytes))
    {
        memor.move_up(&(MemoriaCfg){.dst = s_explorer.node + at + entries->count,
                                    .src = s_explorer.node + at,
                                    .bytes = (s_explorer.nodes - at) * sizeof(*s_explorer.node)});
        for (unsigned int each = 0u; each < entries->count; each += 1u)
        {
            const CliFileEntry *const entry = &entries->entry[each];
            const unsigned long long path =
                EMBED_CALL(explorer_text_append, ControllerExplorerCtx, .text = entry->path,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = entry->path, .cap = CLI_PATH_MAX_LEN}));
            const unsigned long long name =
                EMBED_CALL(explorer_text_append, ControllerExplorerCtx, .text = entry->name,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = entry->name, .cap = CLI_PATH_MAX_LEN}));
            s_explorer.node[at + each] = (ControllerNode){path, name, depth, entry->dir, entry->ignored, 0, 0, 0};
        }
        s_explorer.nodes += entries->count;
    }
    g_cli_cells->persistent_buf_release(listing->entries, &g_cli_controller_cells);
    g_cli_cells->persistent_buf_release(listing, &g_cli_controller_cells);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_EXPLORER);
}

// the folder `dir`, the top folder where empty, asked for on the files' lane
EMBED_INLINE void explorer_list_post(const ControllerExplorerCtx *args)
{
    if (g_cli_model.root == NULL)
    {
        return;
    }
    const unsigned long long root_size =
        cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.root, .cap = CLI_PATH_MAX_LEN}) + 1ull;
    const unsigned long long dir_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->dir, .cap = CLI_PATH_MAX_LEN}) + 1ull;
    ControllerListing *const listing =
        g_cli_cells->persistent_buf_alloc(sizeof(*listing) + root_size + dir_size, &g_cli_controller_cells);
    if (listing == NULL)
    {
        return;
    }
    char *const root = (char *)(void *)(listing + 1);
    char *const dir = root + root_size;
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_model.root, .bytes = root_size});
    memor.cpy(&(MemoriaCfg){.dst = dir, .src = args->dir, .bytes = dir_size});
    *listing = (ControllerListing){root, dir, NULL};
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane = CLI_WORK_FILES, .run = explorer_list_run,
               .done = explorer_list_done, .context = listing);
}

void cli_model_explorer_show(const CliModelCtx *args)
{
    (void)args;
    if ((g_cli_model.tool >= 0) && g_cli_tools[g_cli_model.tool].side && !s_explorer.root_asked)
    {
        s_explorer.root_asked = 1;
        EMBED_CALL(explorer_list_post, ControllerExplorerCtx, .dir = "");
    }
}

// the row after the node `at` in the walk: the next node, past a closed folder's entries
EMBED_INLINE unsigned int explorer_row_next(const ControllerExplorerCtx *args)
{
    const ControllerNode *const node = &s_explorer.node[args->at];
    unsigned int next = args->at + 1u;
    if (node->dir && !node->open)
    {
        while ((next < s_explorer.nodes) && (s_explorer.node[next].depth > node->depth))
        {
            next += 1u;
        }
    }
    return next;
}

void cli_model_explorer_press(const CliModelCtx *args)
{
    unsigned int at = 0u;
    for (unsigned int row = 0u; (row < args->at) && (at < s_explorer.nodes); row += 1u)
    {
        at = EMBED_CALL(explorer_row_next, ControllerExplorerCtx, .at = at);
    }
    if (at >= s_explorer.nodes)
    {
        return;
    }
    ControllerNode *const node = &s_explorer.node[at];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, s_explorer.text + node->path);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    if (!node->dir)
    {
        EMBED_CALL(cli_model.file_open, CliModelCtx, .path = path);
        return;
    }
    node->open = !node->open;
    if (node->open && !node->listed && !node->asked)
    {
        node->asked = 1;
        EMBED_CALL(explorer_list_post, ControllerExplorerCtx, .dir = path);
    }
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_EXPLORER);
}

void *cli_model_explorer_alloc(const CliModelCtx *args)
{
    (void)args;
    // the rows and their texts counted, then laid
    unsigned int rows = 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < s_explorer.nodes;
         at = EMBED_CALL(explorer_row_next, ControllerExplorerCtx, .at = at))
    {
        rows += 1u;
        bytes +=
            cellul.len(&(CatenaFinitaCfg){.src = s_explorer.text + s_explorer.node[at].path, .cap = CLI_PATH_MAX_LEN}) +
            cellul.len(&(CatenaFinitaCfg){.src = s_explorer.text + s_explorer.node[at].name, .cap = CLI_PATH_MAX_LEN}) +
            2ull;
    }
    CliExplorerRows *const explorer = g_cli_cells->persistent_buf_alloc(
        sizeof(*explorer) + (rows * sizeof(CliExplorerRow)) + bytes, &g_cli_controller_cells);
    if (explorer == NULL)
    {
        return NULL;
    }
    explorer->rows = rows;
    explorer->displayed = -1;
    explorer->row = (CliExplorerRow *)(void *)(explorer + 1);
    char *text = (char *)(void *)(explorer->row + rows);
    const char *const shown = (g_cli_model.active >= 0) ? g_cli_model.tab[g_cli_model.active]->path : "";
    unsigned int row = 0u;
    for (unsigned int at = 0u; at < s_explorer.nodes;
         at = EMBED_CALL(explorer_row_next, ControllerExplorerCtx, .at = at))
    {
        const ControllerNode *const node = &s_explorer.node[at];
        const char *const path = s_explorer.text + node->path;
        const char *const name = s_explorer.text + node->name;
        const unsigned long long path_size =
            cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ull;
        const unsigned long long name_size =
            cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_PATH_MAX_LEN}) + 1ull;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = path, .bytes = path_size});
        memor.cpy(&(MemoriaCfg){.dst = text + path_size, .src = name, .bytes = name_size});
        explorer->row[row] =
            (CliExplorerRow){text, text + path_size, node->depth, node->dir, node->ignored, node->open};
        if (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = shown, .cap = CLI_PATH_MAX_LEN}))
        {
            // the rows are fewer than an int counts
            explorer->displayed = (int)row;
        }
        text += path_size + name_size;
        row += 1u;
    }
    return explorer;
}
