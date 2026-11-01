// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_found.c: the environments a tree contains found, at any depth to CLI_ENVS_DEPTH, nearest the top first, each
// with the packages the files beside it require
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs_internal.h"

// how many folders below the top a search for environments goes, the most folders it queues to look in, and the most
// environments found; and the folders and the environments their rooms are taken with
#define CLI_ENVS_DEPTH 5u
#define CLI_ENVS_PENDING 4096u
#define CLI_ENVS_MOST 1024u

// A folder to look in, and how deep it is below the top
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int depth;
} CliEnvFolder;

// An environment found: its kind, its place in the tree, and where its requirements are read
typedef struct
{
    const char *kind;
    char place[CLI_PATH_MAX_LEN];
    char requires_from[CLI_PATH_MAX_LEN];
} CliFoundEnv;

// The search: the folders to look in, the folder whose folders are being read and its depth, what it found, the order
// it is written in, and the names an environment's folder requires
typedef struct
{
    CliEnvFolder *pending;
    unsigned int pending_count;
    unsigned int pending_room;
    const char *folder;
    unsigned int folder_depth;
    CliFoundEnv *found;
    unsigned int count;
    unsigned int found_room;
    unsigned int *order;
    CliRequired required;
} CliEnvSearch;

static void cli_below_each(void *context, const char *name, int is_folder)
{
    CliEnvSearch *const search = context;
    static const char *const s_not[6] = {".git", "node_modules", "target", "build", "__pycache__", ".tox"};
    for (unsigned int at = 0u; is_folder && (at < 6u); at += 1u)
    {
        is_folder = !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_not[at], .cap = CLI_CELLS_POOL});
    }
    if (is_folder && (search->pending_count < search->pending_room))
    {
        CliEnvFolder *const below = &search->pending[search->pending_count];
        cli_full(search->folder, name, below->path);
        below->depth = search->folder_depth + 1u;
        search->pending_count += 1u;
    }
}

// an environment put among those found
static void cli_env_put(CliEnvSearch *search, const char *kind, const char *place, const char *requires_from)
{
    if (search->found_room == 0u)
    {
        search->found = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_ENVS_MOST * sizeof(CliFoundEnv),
                                                         &g_cli_envs_cells);
        search->found_room = (search->found != NULL) ? CLI_ENVS_MOST : 0u;
    }
    if (search->count < search->found_room)
    {
        CliFoundEnv *const one = &search->found[search->count];
        one->kind = kind;
        memor.cpy(&(MemoriaCfg){.dst = one->place,
                                .src = place,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(
            &(MemoriaCfg){.dst = one->requires_from,
                          .src = requires_from,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = requires_from, .cap = CLI_CELLS_POOL}) + 1u});
        search->count += 1u;
    }
}

// how deep an environment's place is, for its order: its slashes, and one more where it is not the top
static unsigned int cli_env_depth(const CliFoundEnv *one)
{
    unsigned int depth = (one->place[0] != '\0') ? 1u : 0u;
    depth += (unsigned int)cli_text_counted(
        one->place, cellul.len(&(CatenaFinitaCfg){.src = one->place, .cap = CLI_CELLS_POOL}), '/');
    return depth;
}

// whether one environment comes before another: by depth, then place, then kind
static int cli_env_before(const CliFoundEnv *one, const CliFoundEnv *other)
{
    const unsigned int one_depth = cli_env_depth(one);
    const unsigned int other_depth = cli_env_depth(other);
    if (one_depth != other_depth)
    {
        return one_depth < other_depth;
    }
    const int order = cli_text_order(one->place, other->place);
    return (order < 0) || ((order == 0) && (cli_text_order(one->kind, other->kind) < 0));
}

// whether the file `name` is in `folder`
static int cli_has_file(const char *folder, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(folder, name, path);
    return query_is_file(path);
}

// the folders below `root` walked to CLI_ENVS_DEPTH, each environment found put
static void cli_envs_searched(CliEnvSearch *search, const char *root)
{
    search->pending = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_ENVS_PENDING * sizeof(CliEnvFolder),
                                                       &g_cli_envs_cells);
    if (search->pending == NULL)
    {
        return;
    }
    search->pending_room = CLI_ENVS_PENDING;
    memor.cpy(&(MemoriaCfg){.dst = search->pending[0].path,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    search->pending[0].depth = 0u;
    search->pending_count = 1u;
    while (search->pending_count > 0u)
    {
        search->pending_count -= 1u;
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(
            &(MemoriaCfg){.dst = folder, .bytes = sizeof(folder), .src = search->pending[search->pending_count].path});
        const unsigned int depth = search->pending[search->pending_count].depth;
        char place[CLI_PATH_MAX_LEN];
        cli_relative(root, folder, place);
        cli_text_swapped(place, cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}), '\\', '/');
        if (cli_has_file(folder, "pyvenv.cfg"))
        {
            char parent[CLI_PATH_MAX_LEN];
            cli_env_put(search, "venv", place, cli_path_parent(folder, parent) ? parent : "");
            continue;
        }
        if (cli_has_file(folder, "Pipfile"))
        {
            cli_env_put(search, "pipenv", place, folder);
        }
        if (cli_has_file(folder, "shell.nix") || cli_has_file(folder, "flake.nix") ||
            cli_has_file(folder, "default.nix"))
        {
            cli_env_put(search, "nix", place, folder);
        }
        if (depth >= CLI_ENVS_DEPTH)
        {
            continue;
        }
        search->folder = folder;
        search->folder_depth = depth;
        query_folder_each(folder, cli_below_each, search);
    }
}

void cli_envs_found(const char *root, VerbaLinea *out)
{
    CliEnvSearch search = {NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, NULL, {NULL, 0u, 0u, NULL, 0u, 0u}};
    cli_required_taken(&search.required);
    cli_envs_searched(&search, root);
    search.order = g_cli_cells->persistent_buf_alloc(((unsigned long long)search.count + 1ull) * sizeof(unsigned int),
                                                     &g_cli_envs_cells);
    const unsigned int count = (search.order != NULL) ? search.count : 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && cli_env_before(&search.found[at], &search.found[search.order[place - 1u]]))
        {
            search.order[place] = search.order[place - 1u];
            place -= 1u;
        }
        search.order[place] = at;
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliFoundEnv *const one = &search.found[search.order[at]];
        mmgr_line_put(out, (at != 0u) ? ",{\"kind\":" : "{\"kind\":");
        cli_json_quoted(out, one->kind, cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"place\":");
        cli_json_quoted(out, one->place, cellul.len(&(CatenaFinitaCfg){.src = one->place, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"requires\":");
        search.required.count = 0u;
        if (one->requires_from[0] != '\0')
        {
            cli_required_in(one->requires_from, &search.required);
        }
        cli_required_written(&search.required, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
    cli_required_given(&search.required);
    g_cli_cells->persistent_buf_release(search.order, &g_cli_envs_cells);
    g_cli_cells->persistent_buf_release(search.found, &g_cli_envs_cells);
    g_cli_cells->persistent_buf_release(search.pending, &g_cli_envs_cells);
}
