// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_found.c: the environments a tree holds found, at any depth to CLI_ENVS_DEPTH, nearest the top first, each with
// the packages the files beside it require
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs_internal.h"

// how many folders below the top a search for environments goes, the most folders it holds to look in, and the most
// environments found; and the folders and the environments their rooms are begun with, each doubling as they are put
#define CLI_ENVS_DEPTH 5u
#define CLI_ENVS_PENDING 4096u
#define CLI_ENVS_MOST 1024u
#define CLI_ENVS_PENDING_FIRST 16u
#define CLI_ENVS_FOUND_FIRST 8u

// A folder to look in, and how deep it is below the top
typedef struct
{
    char path[CLI_PATH_LONGEST];
    unsigned int depth;
} CliEnvFolder;

// An environment found: its kind, its place in the tree, and where its requirements are read
typedef struct
{
    const char *kind;
    char place[CLI_PATH_LONGEST];
    char requires_from[CLI_PATH_LONGEST];
} CliFoundEnv;

// The search: the folders to look in, the folder whose folders are being read and its depth, what it found, the order
// it is written in, and the names an environment's folder requires, each list in a cell whose room doubles as it is put
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

// room made for one folder more to look in: 1, or 0 where the search holds the most or its larger cell is refused
static int cli_pending_room(CliEnvSearch *search)
{
    if (search->pending_count < search->pending_room)
    {
        return 1;
    }
    const unsigned int doubled = (search->pending_room == 0u) ? CLI_ENVS_PENDING_FIRST : (search->pending_room * 2u);
    const unsigned int room = (doubled < CLI_ENVS_PENDING) ? doubled : CLI_ENVS_PENDING;
    CliEnvFolder *const moved =
        (search->pending_count < CLI_ENVS_PENDING)
            ? cli_envs_moved(search->pending, (unsigned long long)search->pending_count * sizeof(CliEnvFolder),
                             (unsigned long long)room * sizeof(CliEnvFolder),
                             "the folders an environment is looked for in")
            : NULL;
    if (moved == NULL)
    {
        return 0;
    }
    search->pending = moved;
    search->pending_room = room;
    return 1;
}

static void cli_below_each(void *context, const char *name, int is_folder)
{
    CliEnvSearch *const search = context;
    static const char *const s_not[6] = {".git", "node_modules", "target", "build", "__pycache__", ".tox"};
    for (unsigned int at = 0u; is_folder && (at < 6u); at += 1u)
    {
        is_folder = !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_not[at], .cap = CLI_CELLS_POOL});
    }
    if (is_folder && cli_pending_room(search))
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
    if ((search->count == search->found_room) && (search->count < CLI_ENVS_MOST))
    {
        const unsigned int doubled = (search->found_room == 0u) ? CLI_ENVS_FOUND_FIRST : (search->found_room * 2u);
        const unsigned int room = (doubled < CLI_ENVS_MOST) ? doubled : CLI_ENVS_MOST;
        CliFoundEnv *const moved =
            cli_envs_moved(search->found, (unsigned long long)search->count * sizeof(CliFoundEnv),
                           (unsigned long long)room * sizeof(CliFoundEnv), "the environments found");
        search->found = (moved != NULL) ? moved : search->found;
        search->found_room = (moved != NULL) ? room : search->found_room;
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
    for (const char *at = one->place; at[0] != '\0'; at += 1)
    {
        depth += (at[0] == '/') ? 1u : 0u;
    }
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
static int cli_holds(const char *folder, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    return query_is_file(path);
}

// the folders below `root` walked to CLI_ENVS_DEPTH, each environment found put
static void cli_envs_searched(CliEnvSearch *search, const char *root)
{
    if (!cli_pending_room(search))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = search->pending[0].path,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    search->pending[0].depth = 0u;
    search->pending_count = 1u;
    while (search->pending_count > 0u)
    {
        search->pending_count -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(
            &(MemoriaCfg){.dst = folder, .bytes = sizeof(folder), .src = search->pending[search->pending_count].path});
        const unsigned int depth = search->pending[search->pending_count].depth;
        char place[CLI_PATH_LONGEST];
        cli_relative(root, folder, place);
        for (char *at = place; at[0] != '\0'; at += 1)
        {
            at[0] = (at[0] == '\\') ? '/' : at[0];
        }
        if (cli_holds(folder, "pyvenv.cfg"))
        {
            char parent[CLI_PATH_LONGEST];
            cli_env_put(search, "venv", place, cli_path_parent(folder, parent) ? parent : "");
            continue;
        }
        if (cli_holds(folder, "Pipfile"))
        {
            cli_env_put(search, "pipenv", place, folder);
        }
        if (cli_holds(folder, "shell.nix") || cli_holds(folder, "flake.nix") || cli_holds(folder, "default.nix"))
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

void cli_envs_found(const char *root, CliLine *out)
{
    CliEnvSearch search = {NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, NULL, {NULL, 0u, 0u, NULL, 0u, 0u}};
    cli_envs_searched(&search, root);
    search.order =
        cli_cell_taken_by(&g_cli_envs_cells, ((unsigned long long)search.count + 1ull) * sizeof(unsigned int),
                          "the order the environments are written in");
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
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliFoundEnv *const one = &search.found[search.order[at]];
        cli_line_text(out, (at != 0u) ? ",{\"kind\":" : "{\"kind\":");
        cli_json_quoted(out, one->kind, cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"place\":");
        cli_json_quoted(out, one->place, cellul.len(&(CatenaFinitaCfg){.src = one->place, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"requires\":");
        search.required.count = 0u;
        if (one->requires_from[0] != '\0')
        {
            cli_required_in(one->requires_from, &search.required);
        }
        cli_required_written(&search.required, out);
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    cli_required_given(&search.required);
    cli_cell_given_by(&g_cli_envs_cells, search.order);
    cli_cell_given_by(&g_cli_envs_cells, search.found);
    cli_cell_given_by(&g_cli_envs_cells, search.pending);
}
