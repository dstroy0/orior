// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_listing.c: the runs of a tree, from its folders of runs: those one walk of the tree found, and each folder the
// tree's watch since saw a run written in, the tree walked again only where the watch lost changes; each folder read
// again on every listing so a run that appears in it shows at once; and the lessons of each folder's `overseer.txt`
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "qry.h"

// A tree's walk: its root, when it was walked on the clock that only goes forward, whether the tree's watch lost
// changes since, and its folders of runs in order, in a cell of `capacity` at their most
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    int used;
    unsigned long long walked;
    int lost;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliWalk;
// A run as the window lists it
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size;
    unsigned long long changed;
    int live;
    unsigned long long panicked;
    unsigned int folder;
} CliListResult;
// the most trees whose walks are cached, the most folders of runs one contains, the most folders a walk has yet to
// read, the most runs listed, and the places a list is first taken with
#define CLI_WALKS_MAX 16u

#define CLI_RUN_FOLDERS 256u

#define CLI_WALK_PENDING 4096u

#define CLI_LISTING_MAX 4096u

// the walks cached, under the lock
static CliWalk s_walks[CLI_WALKS_MAX];

// The folders a walk has yet to read, in a cell of `capacity`
typedef struct
{
    char (*folder)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliPending;

// The runs listed, in a cell of `capacity`
typedef struct
{
    CliListResult *run;
    unsigned int count;
    unsigned int capacity;
} CliRuns;

// What one folder read of a walk hands on: the walk, the folder read, and the folders that wait
typedef struct
{
    CliWalk *walk;
    const char *folder;
    CliPending *pending;
} CliWalking;

typedef struct
{
    CliWalk *walk;
    const char *folder;
    const char *root;
    const CliListResult *one;
    const CliListResult *other;
    char *full;
} QryListingCtx;
// `folder`, a path from the top of the tree, put in its place among the walk's folders of runs where it is not one
// already; under the lock
EMBED_INLINE void cli_walk_folder_append(const QryListingCtx *args)
{
    for (unsigned int at = 0u; at < args->walk->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->walk->folders[at], .other = args->folder, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (args->walk->count == args->walk->capacity)
    {
        return;
    }
    unsigned int place = args->walk->count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                       .one = args->walk->folders[place - 1u], .other_pointer = args->folder) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->walk->folders[place],
            .src = args->walk->folders[place - 1u],
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = args->walk->folders[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->walk->folders[place],
                            .src = args->folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}) + 1u});
    args->walk->count += 1u;
}

// the walk of the tree at `root`, or CLI_WALKS_MAX where none is cached; under the lock
EMBED_INLINE unsigned int cli_walk_find(const QryListingCtx *args)
{
    for (unsigned int at = 0u; at < CLI_WALKS_MAX; at += 1u)
    {
        if (s_walks[at].used &&
            cellul.eq(&(CatenaFinitaCfg){.src = s_walks[at].root, .other = args->root, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return CLI_WALKS_MAX;
}

void cli_qry_run_write(const CliQryPathRootCfg *args)
{
    if ((args->length <= 4u) || (args->length >= CLI_PATH_MAX_LEN) ||
        (memor.cmp(&(MemoriaCfg){.src = args->path + args->length - 4u, .other = ".qry", .bytes = 4u}) != 0))
    {
        return;
    }
    // the folder is the path before its last slash, the top of the tree where it has none
    unsigned long long cut = args->length;
    cut = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = cut,
                     .set = "/");
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folder, .src = args->path, .bytes = (size_t)((cut > 0u) ? cut - 1u : 0u)});
    folder[(cut > 0u) ? cut - 1u : 0u] = '\0';
    const unsigned int found = EMBED_CALL(cli_walk_find, QryListingCtx, .root = args->root);
    if (found != CLI_WALKS_MAX)
    {
        EMBED_CALL(cli_walk_folder_append, QryListingCtx, .walk = &s_walks[found], .folder = folder);
    }
}

void cli_qry_runs_lose(const CliQryOutRootCfg *args)
{
    const unsigned int found = EMBED_CALL(cli_walk_find, QryListingCtx, .root = args->root);
    if (found != CLI_WALKS_MAX)
    {
        s_walks[found].lost = 1;
    }
}

// What one folder of runs read hands on: its full path, its place among the folders, and the runs listed
typedef struct
{
    const char *full;
    const char *folder;
    unsigned int index;
    CliRuns *runs;
} CliFolderRead;
