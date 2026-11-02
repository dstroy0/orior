// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_repositories.c: the repositories of the tree, the one it is in and each folder under it that contains its own
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "git_internal.h"

// the folders a search for the tree's repositories does not go into, those a build or a package manager fills; how many
// folders deep under the tree it goes; the most folders it queues to look in; and the folders and repositories its
// lists are taken with
static const char *const s_not_searched[6] = {"node_modules", "target", "build", "dist", "__pycache__", "venv"};

#define CLI_GIT_DEPTH 4u

#define CLI_GIT_PENDING 4096u

// A folder a search for repositories still looks in, and how deep under the tree it is
typedef struct
{
    unsigned int depth;
    char path[CLI_PATH_MAX_LEN];
} CliSearchFolder;

// The search for the tree's repositories: the folders still to look in and those found containing one, each a cell of
// its capacity, and the folder looked in now and its depth
typedef struct
{
    CliSearchFolder *pending;
    unsigned int pending_count;
    unsigned int pending_capacity;
    char (*nested)[CLI_PATH_MAX_LEN];
    unsigned int nested_count;
    unsigned int nested_capacity;
    const char *folder;
    unsigned int at_depth;
} CliSearch;

static void cli_search_each(const CliEachNameIsFolderCfg *args)
{
    CliSearch *const search = (CliSearch *)args->context;
    if (!args->is_folder || (args->name[0] == '.'))
    {
        return;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_not_searched[at], .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    char path[CLI_PATH_MAX_LEN];
    char git[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, search->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
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
    const int is_repository = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) ||
                              EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git);
    if (is_repository)
    {
    }
    if (is_repository && (search->nested_count < search->nested_capacity))
    {
        memor.cpy(&(MemoriaCfg){.dst = search->nested[search->nested_count],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        search->nested_count += 1u;
    }
    if (((search->at_depth + 1u) < CLI_GIT_DEPTH) && (search->pending_count < search->pending_capacity))
    {
        CliSearchFolder *const pending = &search->pending[search->pending_count];
        memor.cpy(&(MemoriaCfg){.dst = pending->path,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        pending->depth = search->at_depth + 1u;
        search->pending_count += 1u;
    }
}

typedef struct
{
    CliRepositories *found;
    const char *relative;
    const char *folder;
} GitRepositoriesCtx;
// a repository put after those found, its path in the tree, its name and its branch stored
EMBED_INLINE void cli_repository_append(const GitRepositoriesCtx *args)
{
    if (args->found->count >= CLI_GIT_REPOSITORIES)
    {
        return;
    }
    CliRepository *const one = &args->found->item[args->found->count];
    args->found->count += 1u;
    char name[CLI_NAME_MAX_LEN];
    char branch[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_git_table.name, CliGitCfg, .root = args->folder, .name = name);
    one->path = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = args->relative,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = args->relative, .cap = CLI_CELLS_POOL}));
    one->name = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = name,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    one->has_branch = EMBED_CALL(cli_git_table.branch, CliGitCfg, .root = args->folder, .name = branch);
    one->branch = one->has_branch
                      ? EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = branch,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = branch, .cap = CLI_CELLS_POOL}))
                      : 0u;
}

void cli_git_repositories(const CliGitRootFoundRepositoriesCfg *args)
{
    static const char *const s_inside[2] = {"rev-parse", "--is-inside-work-tree"};
    args->found_repositories_pointer->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_repositories_pointer->text);
    CliGitResult *const result = cli_git_alloc();
    const int inside = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root,
                                  .request = s_inside, .count = 2u, .result = result);
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    if (inside)
    {
        EMBED_CALL(cli_repository_append, GitRepositoriesCtx, .found = args->found_repositories_pointer, .relative = "",
                   .folder = args->root);
    }
    CliSearch search_copy = {0};
    CliSearch *const search = &search_copy;
    const unsigned long long pending_bytes = (unsigned long long)CLI_GIT_PENDING * sizeof(*search->pending);
    char *const cell = g_cli_cells->temporary_buf_alloc(
        pending_bytes + ((unsigned long long)CLI_GIT_REPOSITORIES * sizeof(*search->nested)), &g_cli_git_cells);
    if (cell != NULL)
    {
        search->pending = (CliSearchFolder *)(void *)cell;
        search->pending_capacity = CLI_GIT_PENDING;
        search->nested = (void *)(cell + pending_bytes);
        search->nested_capacity = CLI_GIT_REPOSITORIES;
        memor.cpy(
            &(MemoriaCfg){.dst = search->pending[0].path,
                          .src = args->root,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
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
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                   .each = cli_search_each, .context_bytess = search);
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
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                           .one = search->nested[place - 1u], .other_pointer = swap_path) > 0))
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
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = search->nested[at],
                   .relative_out = relative);
        EMBED_CALL(cli_repository_append, GitRepositoriesCtx, .found = args->found_repositories_pointer,
                   .relative = relative, .folder = search->nested[at]);
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_git_cells);
}
