// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_repositories.c: the repositories of the tree, the one it is in and each folder under it that contains its own
#include "../text/text.h"
#include "git_internal.h"

// the folders a search for the tree's repositories does not go into, those a build or a package manager fills; how
// many folders deep under the tree it goes; the most folders it queues to look in; and the folders and repositories
// its lists are begun with, doubling as they fill
static const char *const s_not_searched[6] = {"node_modules", "target", "build", "dist", "__pycache__", "venv"};

#define CLI_GIT_DEPTH 4u

#define CLI_GIT_PENDING 4096u

#define CLI_GIT_SEARCH_FIRST 16u

// A folder a search for repositories still looks in, and how deep under the tree it is
typedef struct
{
    unsigned int depth;
    char path[CLI_PATH_MAX_LEN];
} CliSearchFolder;

// The search for the tree's repositories: the folders still to look in and those found containing one, each a cell of
// its room, and the folder looked in now and its depth
typedef struct
{
    CliSearchFolder *pending;
    unsigned int pending_count;
    unsigned int pending_room;
    char (*nested)[CLI_PATH_MAX_LEN];
    unsigned int nested_count;
    unsigned int nested_room;
    const char *folder;
    unsigned int at_depth;
} CliSearch;

// a list of `*room` items of `size` bytes, `count` of them stored, given room for one more: where it is full and below
// `most`, the list moved into a cell of twice its room, CLI_GIT_SEARCH_FIRST where it has none, its items copied and
// its cell given back. The list as it stands after, `count` below `*room` where there is room
static void *cli_search_grown(void *list, unsigned int count, unsigned int *room, unsigned long long size,
                              unsigned int most)
{
    if ((count < *room) || (*room >= most))
    {
        return list;
    }
    const unsigned int doubled = (*room == 0u) ? CLI_GIT_SEARCH_FIRST : (*room * 2u);
    const unsigned int grown = (doubled < most) ? doubled : most;
    void *const cell = g_cli_cells->persistent_buf_alloc(grown * size, &g_cli_git_cells);
    if (cell == NULL)
    {
        return list;
    }
    if (count != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = list, .bytes = count * size});
    }
    g_cli_cells->persistent_buf_release(list, &g_cli_git_cells);
    *room = grown;
    return cell;
}

static void cli_search_each(void *context, const char *name, int is_folder)
{
    CliSearch *const search = (CliSearch *)context;
    if (!is_folder || (name[0] == '.'))
    {
        return;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_not_searched[at], .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    char path[CLI_PATH_MAX_LEN];
    char git[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, search->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    // the folder's path ended before its .git is written from it
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    VerbaLinea marked = {git, sizeof(git), 0ull};
    mmgr_line_put(&marked, path);
    mmgr_line_put(&marked, "/.git");
    if (mmgr_line_finish(&marked) == 0ull)
    {
        return;
    }
    const int is_repository = query_is_file(git) || query_is_folder(git);
    if (is_repository)
    {
        search->nested = cli_search_grown(search->nested, search->nested_count, &search->nested_room,
                                          sizeof(*search->nested), CLI_GIT_REPOSITORIES);
    }
    if (is_repository && (search->nested_count < search->nested_room))
    {
        memor.cpy(&(MemoriaCfg){.dst = search->nested[search->nested_count],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        search->nested_count += 1u;
    }
    if ((search->at_depth + 1u) < CLI_GIT_DEPTH)
    {
        search->pending = cli_search_grown(search->pending, search->pending_count, &search->pending_room,
                                           sizeof(*search->pending), CLI_GIT_PENDING);
    }
    if (((search->at_depth + 1u) < CLI_GIT_DEPTH) && (search->pending_count < search->pending_room))
    {
        CliSearchFolder *const pending = &search->pending[search->pending_count];
        memor.cpy(&(MemoriaCfg){.dst = pending->path,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        pending->depth = search->at_depth + 1u;
        search->pending_count += 1u;
    }
}

// a repository put after those found, its path in the tree, its name and its branch stored
static void cli_repository_put(CliRepositories *found, const char *relative, const char *folder)
{
    if (found->count >= CLI_GIT_REPOSITORIES)
    {
        return;
    }
    CliRepository *const one = &found->item[found->count];
    found->count += 1u;
    char name[CLI_NAME_MAX_LEN];
    char branch[CLI_NAME_MAX_LEN];
    cli_git_name(folder, name);
    one->path = cli_git_text_put(&found->text, relative,
                                 cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}));
    one->name =
        cli_git_text_put(&found->text, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    one->has_branch = cli_git_branch(folder, branch);
    one->branch = one->has_branch
                      ? cli_git_text_put(&found->text, branch,
                                         cellul.len(&(CatenaFinitaCfg){.src = branch, .cap = CLI_CELLS_POOL}))
                      : 0u;
}

void cli_git_repositories(const char *root, CliRepositories *found)
{
    static const char *const s_inside[2] = {"rev-parse", "--is-inside-work-tree"};
    found->count = 0u;
    cli_git_text_begun(&found->text);
    CliGitGave *const gave = cli_git_taken();
    const int inside = cli_git_query(root, s_inside, 2u, gave);
    cli_git_given(gave);
    if (inside)
    {
        cli_repository_put(found, "", root);
    }
    CliSearch search_copy = {0};
    CliSearch *const search = &search_copy;
    search->pending = cli_search_grown(NULL, 0u, &search->pending_room, sizeof(*search->pending), CLI_GIT_PENDING);
    if (search->pending_room != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = search->pending[0].path,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        search->pending[0].depth = 0u;
        search->pending_count = 1u;
    }
    while (search->pending_count > 0u)
    {
        search->pending_count -= 1u;
        const CliSearchFolder *const next = &search->pending[search->pending_count];
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(
            &(MemoriaCfg){.dst = folder,
                          .src = next->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = next->path, .cap = CLI_CELLS_POOL}) + 1u});
        search->folder = folder;
        search->at_depth = next->depth;
        query_folder_each(folder, cli_search_each, search);
    }
    // the repositories under the tree in the order of their paths
    for (unsigned int at = 1u; at < search->nested_count; at += 1u)
    {
        char swap_path[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = swap_path,
            .src = search->nested[at],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = search->nested[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(search->nested[place - 1u], swap_path) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = search->nested[place],
                .src = search->nested[place - 1u],
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = search->nested[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = search->nested[place],
                                .src = swap_path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = swap_path, .cap = CLI_CELLS_POOL}) + 1u});
    }
    for (unsigned int at = 0u; at < search->nested_count; at += 1u)
    {
        char relative[CLI_PATH_MAX_LEN];
        cli_relative(root, search->nested[at], relative);
        cli_repository_put(found, relative, search->nested[at]);
    }
    g_cli_cells->persistent_buf_release(search->pending, &g_cli_git_cells);
    g_cli_cells->persistent_buf_release(search->nested, &g_cli_git_cells);
}
