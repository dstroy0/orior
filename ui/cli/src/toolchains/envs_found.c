// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_find.c: the environments a tree contains found, at any depth to CLI_ENVS_DEPTH, nearest the top first, each
// with the packages the files beside it require
#include "../command_line/callback_cfg.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs_internal.h"

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
// how many folders below the top a search for environments goes, the most folders it queues to look in, and the most
// environments found; and the folders and the environments their rooms are taken with
#define CLI_ENVS_DEPTH 5u
#define CLI_ENVS_PENDING 4096u
#define CLI_ENVS_MOST 1024u

static void cli_below_each(const CliEachNameIsFolderCfg *args)
{
    int is_folder = args->is_folder;
    CliEnvSearch *const search = args->context;
    static const char *const s_not[6] = {".git", "node_modules", "target", "build", "__pycache__", ".tox"};
    for (unsigned int at = 0u; is_folder && (at < 6u); at += 1u)
    {
        is_folder = !cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_not[at], .cap = CLI_CELLS_POOL});
    }
    if (is_folder && (search->pending_count < search->pending_room))
    {
        CliEnvFolder *const below = &search->pending[search->pending_count];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = search->folder, .relative = args->name,
                   .joined = below->path);
        below->depth = search->folder_depth + 1u;
        search->pending_count += 1u;
    }
}

typedef struct
{
    CliEnvSearch *search;
    const char *kind;
    const char *place;
    const char *requires_from;
    const CliFoundEnv *one;
    const CliFoundEnv *other;
    const char *folder;
    const char *name;
    const char *root;
} EnvsFoundCtx;
// an environment put among those found
EMBED_INLINE void cli_env_put(const EnvsFoundCtx *args)
{
    const char *kind = args->kind;
    if (args->search->found_room == 0u)
    {
        args->search->found = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_ENVS_MOST * sizeof(CliFoundEnv),
                                                               &g_cli_envs_cells);
        args->search->found_room = (args->search->found != NULL) ? CLI_ENVS_MOST : 0u;
    }
    if (args->search->count < args->search->found_room)
    {
        CliFoundEnv *const one = &args->search->found[args->search->count];
        one->kind = kind;
        memor.cpy(
            &(MemoriaCfg){.dst = one->place,
                          .src = args->place,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->place, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = one->requires_from,
            .src = args->requires_from,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->requires_from, .cap = CLI_CELLS_POOL}) + 1u});
        args->search->count += 1u;
    }
}

// how deep an environment's place is, for its order: its slashes, and one more where it is not the top
EMBED_INLINE unsigned int cli_env_depth(const EnvsFoundCtx *args)
{
    unsigned int depth = (args->one->place[0] != '\0') ? 1u : 0u;
    depth += (unsigned int)EMBED_CALL(
        cli_text_size_byte.count, CliTextSizeByteCfg, .text = args->one->place,
        .size = cellul.len(&(CatenaFinitaCfg){.src = args->one->place, .cap = CLI_CELLS_POOL}), .byte = '/');
    return depth;
}

// whether one environment comes before another: by depth, then place, then kind
EMBED_INLINE int cli_env_before(const EnvsFoundCtx *args)
{
    const CliFoundEnv *one = args->one;
    const CliFoundEnv *other = args->other;
    const unsigned int one_depth = EMBED_CALL(cli_env_depth, EnvsFoundCtx, .one = one);
    const unsigned int other_depth = EMBED_CALL(cli_env_depth, EnvsFoundCtx, .one = other);
    if (one_depth != other_depth)
    {
        return one_depth < other_depth;
    }
    const int order =
        EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = one->place, .other_pointer = other->place);
    return (order < 0) || ((order == 0) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = one->kind,
                                                       .other_pointer = other->kind) < 0));
}

// whether the file `name` is in `folder`
EMBED_INLINE int cli_has_file(const EnvsFoundCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = args->name, .joined = path);
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}

// the folders below `root` walked to CLI_ENVS_DEPTH, each environment found put
EMBED_INLINE void cli_envs_search(const EnvsFoundCtx *args)
{
    const char *root = args->root;
    args->search->pending = g_cli_cells->temporary_buf_alloc(
        (unsigned long long)CLI_ENVS_PENDING * sizeof(CliEnvFolder), &g_cli_envs_cells);
    if (args->search->pending == NULL)
    {
        return;
    }
    args->search->pending_room = CLI_ENVS_PENDING;
    memor.cpy(&(MemoriaCfg){.dst = args->search->pending[0].path,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    args->search->pending[0].depth = 0u;
    args->search->pending_count = 1u;
    while (args->search->pending_count > 0u)
    {
        args->search->pending_count -= 1u;
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = folder, .bytes = sizeof(folder), .src = args->search->pending[args->search->pending_count].path});
        const unsigned int depth = args->search->pending[args->search->pending_count].depth;
        char place[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = folder,
                   .relative_out = place);
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = place,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}), .from = '\\',
                   .to = '/');
        if (EMBED_CALL(cli_has_file, EnvsFoundCtx, .folder = folder, .name = "pyvenv.cfg"))
        {
            char parent[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_env_put, EnvsFoundCtx, .search = args->search, .kind = "venv", .place = place,
                       .requires_from = EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx,
                                                   .path = folder, .out = parent)
                                            ? parent
                                            : "");
            continue;
        }
        if (EMBED_CALL(cli_has_file, EnvsFoundCtx, .folder = folder, .name = "Pipfile"))
        {
            EMBED_CALL(cli_env_put, EnvsFoundCtx, .search = args->search, .kind = "pipenv", .place = place,
                       .requires_from = folder);
        }
        if (EMBED_CALL(cli_has_file, EnvsFoundCtx, .folder = folder, .name = "shell.nix") ||
            EMBED_CALL(cli_has_file, EnvsFoundCtx, .folder = folder, .name = "flake.nix") ||
            EMBED_CALL(cli_has_file, EnvsFoundCtx, .folder = folder, .name = "default.nix"))
        {
            EMBED_CALL(cli_env_put, EnvsFoundCtx, .search = args->search, .kind = "nix", .place = place,
                       .requires_from = folder);
        }
        if (depth >= CLI_ENVS_DEPTH)
        {
            continue;
        }
        args->search->folder = folder;
        args->search->folder_depth = depth;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = cli_below_each,
                   .context_bytess = args->search);
    }
}

void cli_envs_find(const CliEnvsCfg *args)
{
    CliEnvSearch search = {NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, NULL, {NULL, 0u, 0u, NULL, 0u, 0u}};
    EMBED_CALL(cli_envs_internal.required_take, CliEnvsCtx, .required = &search.required);
    EMBED_CALL(cli_envs_search, EnvsFoundCtx, .search = &search, .root = args->root);
    search.order = g_cli_cells->persistent_buf_alloc(((unsigned long long)search.count + 1ull) * sizeof(unsigned int),
                                                     &g_cli_envs_cells);
    const unsigned int count = (search.order != NULL) ? search.count : 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && EMBED_CALL(cli_env_before, EnvsFoundCtx, .one = &search.found[at],
                                          .other = &search.found[search.order[place - 1u]]))
        {
            search.order[place] = search.order[place - 1u];
            place -= 1u;
        }
        search.order[place] = at;
    }
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliFoundEnv *const one = &search.found[search.order[at]];
        mmgr_line_put(args->out, (at != 0u) ? ",{\"kind\":" : "{\"kind\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = one->kind,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"place\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = one->place,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = one->place, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"requires\":");
        search.required.count = 0u;
        if (one->requires_from[0] != '\0')
        {
            EMBED_CALL(cli_envs_internal.required_in, CliEnvsCtx, .folder = one->requires_from,
                       .required = &search.required);
        }
        EMBED_CALL(cli_envs_out_internal.required_write, CliEnvsOutCtx, .required_read = &search.required,
                   .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
    EMBED_CALL(cli_envs_internal.required_free, CliEnvsCtx, .required = &search.required);
    g_cli_cells->persistent_buf_release(search.order, &g_cli_envs_cells);
    g_cli_cells->persistent_buf_release(search.found, &g_cli_envs_cells);
    g_cli_cells->persistent_buf_release(search.pending, &g_cli_envs_cells);
}
