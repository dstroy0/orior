// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_sync.c: the files of orior's own folder that travel, packed and sent to a file or a folder, and read back from one
#include "../code/git.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/commands.h"
#include "../command_line/pool.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "settings_sync_internal.h"

// the most a file of the reader's that travels syncs
#define CLI_SETTINGS_SYNC_LARGEST (16ULL << 20u)

// the files that travel, each a file or a folder of orior's own folder, by its place there; the settings that stay on
// this machine are those whose node in commands.json is marked `machine`
static const char *const s_transferred[7] = {"settings.json", "menus.json",       "user.css",  "themes/",
                                             "plugins/",      "plugin_settings/", "languages/"};

void cli_transferred_clear(const CliSettingsSyncTransferredPositionCtx *args)
{
    for (unsigned int at = 0u; at < args->transferred_out->count; at += 1u)
    {
        g_cli_guard->persistent_buf_release(args->transferred_out->value[at], g_cli_machine_owner);
        g_cli_guard->persistent_buf_release(args->transferred_out->place[at], g_cli_machine_owner);
        args->transferred_out->value[at] = NULL;
        args->transferred_out->place[at] = NULL;
    }
    args->transferred_out->count = 0u;
}

unsigned int cli_transferred_find(const CliSettingsSyncTransferredCtx *args)
{
    for (unsigned int at = 0u; at < args->synced->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->synced->place[at], .other = args->place, .cap = CLI_POOL_BYTES}))
        {
            return at;
        }
    }
    return args->synced->count;
}

const char *cli_transferred_position(const CliSettingsSyncTransferredAtCtx *args)
{
    return args->synced->place[args->at];
}

const char *cli_transferred_value(const CliSettingsSyncTransferredAtCtx *args)
{
    return args->synced->value[args->at];
}

void cli_settings_sync_work_state_begin(const CliSettingsSyncWorkStateObjectCtx *args)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->work_state->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

typedef struct
{
    CliTransferred *paths;
    CliSettingsSyncWorkState *work_state;
    const char *text;
    unsigned long long size;
    const char *what;
    int fresh;
    unsigned int nodes_more;
    unsigned long long text_more;
    const char *folder;
    unsigned int object;
} SettingsSyncCtx;
// the cells a sync's work took given back, its paths and its work state, and its lock
EMBED_INLINE void settings_sync_release(const SettingsSyncCtx *args)
{
    CliSettingsSyncWorkState *work_state = args->work_state;
    EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_clear,
               CliSettingsSyncTransferredPositionCtx, .transferred_out = args->paths);
    g_cli_guard->persistent_buf_release(work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

// `size` bytes of `text` in a cell of their own, a 0 after them: the cell, or NULL where none could be taken
EMBED_INLINE char *cli_transferred_cell(const SettingsSyncCtx *args)
{
    char *const cell = (args->size < CLI_TRANSFERRED_TEXT)
                           ? g_cli_guard->persistent_buf_alloc(args->size + 1ULL, g_cli_machine_owner)
                           : NULL;
    if (cell != NULL)
    {
        // a size below CLI_TRANSFERRED_TEXT fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->text, .bytes = (size_t)args->size});
        cell[args->size] = '\0';
    }
    return cell;
}

unsigned long long cli_transferred_sent_capacity(const CliSettingsSyncTransferredCtx *args)
{
    unsigned long long capacity = 128ULL;
    for (unsigned int at = 0u; at < args->synced->count; at += 1u)
    {
        capacity += (args->synced->size[at] * 6ULL) + (CLI_NAME_MAX_LEN * 6ULL) + 16ULL;
    }
    return capacity;
}

int cli_transferred_append(const CliSettingsSyncTransferredPositionCtx *args)
{
    unsigned int at = 0u;
    while ((at < args->transferred_out->count) &&
           (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->transferred_out->place[at],
                       .other_pointer = args->place) < 0))
    {
        at += 1u;
    }
    const int there = (at < args->transferred_out->count) &&
                      (cellul.eq(&(CatenaFinitaCfg){
                          .src = args->transferred_out->place[at], .other = args->place, .cap = CLI_POOL_BYTES}));
    if (!there && (args->transferred_out->count >= CLI_TRANSFERRED_MAX))
    {
        return 0;
    }
    char *const pooled_value = EMBED_CALL(cli_transferred_cell, SettingsSyncCtx, .text = args->value, .size = args->size,
                                          .what = "a synced file's text");
    char *const pooled_position =
        there ? args->transferred_out->place[at]
              : EMBED_CALL(cli_transferred_cell, SettingsSyncCtx, .text = args->place,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = args->place, .cap = CLI_POOL_BYTES}),
                           .what = "a synced place");
    if ((pooled_position == NULL) || (pooled_value == NULL))
    {
        g_cli_guard->persistent_buf_release(pooled_value, g_cli_machine_owner);
        if (!there)
        {
            g_cli_guard->persistent_buf_release(pooled_position, g_cli_machine_owner);
        }
        return 0;
    }
    if (there)
    {
        g_cli_guard->persistent_buf_release(args->transferred_out->value[at], g_cli_machine_owner);
    }
    else
    {
        for (unsigned int move = args->transferred_out->count; move > at; move -= 1u)
        {
            args->transferred_out->place[move] = args->transferred_out->place[move - 1u];
            args->transferred_out->value[move] = args->transferred_out->value[move - 1u];
            args->transferred_out->size[move] = args->transferred_out->size[move - 1u];
        }
        args->transferred_out->count += 1u;
    }
    args->transferred_out->place[at] = pooled_position;
    args->transferred_out->value[at] = pooled_value;
    args->transferred_out->size[at] = args->size;
    return 1;
}

int cli_settings_sync_machine(const CliSettingsSyncCtx *args)
{
    CliItem node;
    return cli_setting_node(&(CliCommandsMenuCfg){.word_text = args->key, .item = &node}) && node.machine;
}

// `size` bytes of settings text made plain, which is never longer than the text, and read into the work state's tree, a
// fresh one in a cell sized to it with `nodes_more` nodes and `text_more` bytes over where `fresh` is 1, else beside
// what it contains: the object read, an empty one where it is no object, CLI_JSON_NONE where the tree has no cell
static unsigned int settings_sync_plain_read(const SettingsSyncCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int nodes_more = args->nodes_more;
    unsigned long long text_more = args->text_more;
    char *const plain_text = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_machine_owner);
    VerbaLinea plain = mmgr_line_over(plain_text, size + 1ULL);
    EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = text, .size = size, .plain = &plain);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    unsigned int root = CLI_JSON_NONE;
    if (args->fresh)
    {
        g_cli_guard->persistent_buf_release(args->work_state->json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), args->work_state->json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&args->work_state->json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        root = (plain_text != NULL)
                   ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                .json = &args->work_state->json, .text = plain_text, .size = plain.at,
                                .nodes_more = nodes_more, .text_more = text_more, .err_msg = &unread,
                                .owner = g_cli_machine_owner)
                   : CLI_JSON_NONE;
        if (args->work_state->json.node == NULL)
        {
            (void)EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg,
                             .json = &args->work_state->json, .nodes_max = nodes_more, .text_max = text_more,
                             .what = "a settings tree", .owner = g_cli_machine_owner);
        }
    }
    else if (plain_text != NULL)
    {
        const unsigned int tokens_max =
            EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = plain_text, .size = plain.at) +
            1u;
        CfgJsonToken *const tokens =
            g_cli_guard->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(*tokens), g_cli_machine_owner);
        root = (tokens != NULL) ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                             .json = &args->work_state->json, .text = plain_text, .size = plain.at,
                                             .tokens = tokens, .tokens_max = tokens_max, .err_msg = &unread)
                                : CLI_JSON_NONE;
        g_cli_guard->persistent_buf_release(tokens, g_cli_machine_owner);
    }
    g_cli_guard->persistent_buf_release(plain_text, g_cli_machine_owner);
    if (args->work_state->json.node == NULL)
    {
        return CLI_JSON_NONE;
    }
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &args->work_state->json,
                      .node = root, .kind = CFG_JSON_OBJECT)
               ? root
               : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &args->work_state->json,
                            .kind = CFG_JSON_OBJECT);
}

unsigned int cli_settings_sync_settings_get(const CliSettingsSyncWorkStateCtx *args)
{
    // a text read beside makes at most as many nodes as its bytes, and at most twice them in text with a 0 after each,
    // and a copy of what it read takes as much again; the four over are the empty objects either side may be made
    return EMBED_CALL(settings_sync_plain_read, SettingsSyncCtx, .work_state = args->work_state, .text = args->text,
                      .size = args->size, .fresh = 1, .nodes_more = (unsigned int)((2ULL * args->adjacent) + 4ULL),
                      .text_more = (4ULL * args->adjacent) + 64ULL);
}

unsigned int cli_settings_sync_settings_adjacent(const CliSettingsSyncWorkStateCtx *args)
{
    return EMBED_CALL(settings_sync_plain_read, SettingsSyncCtx, .work_state = args->work_state, .text = args->text,
                      .size = args->size, .fresh = 0, .nodes_more = 0u, .text_more = 0ULL);
}

void cli_settings_sync_settings_write(const CliSettingsSyncWorkStateObjectCtx *args)
{
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &args->work_state->json,
               .object = args->object);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &args->work_state->json,
               .node = args->object, .out = args->out, .indent = 2u, .sorted = 0);
    mmgr_line_ch(args->out, '\n');
}

int cli_settings_sync_allow(const CliSettingsSyncTransferredCtx *args)
{
    const char *part = args->place;
    for (const char *at = args->place;; at += 1)
    {
        if ((at[0] == '/') || (at[0] == '\0'))
        {
            // the part lies before `at`, its end at or past its start
            const unsigned long long size = (unsigned long long)(at - part);
            if ((size == 0ULL) || ((size == 1ULL) && (part[0] == '.')) ||
                ((size == 2ULL) && (part[0] == '.') && (part[1] == '.')))
            {
                return 0;
            }
            if (at[0] == '\0')
            {
                break;
            }
            part = at + 1;
        }
    }
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = s_transferred[at], .cap = CLI_POOL_BYTES});
        const int under = (s_transferred[at][length - 1ULL] == '/') &&
                          EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->place,
                                     .other_pointer = s_transferred[at], .size = length);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = args->place, .other = s_transferred[at], .cap = CLI_POOL_BYTES})) ||
            under)
        {
            return 1;
        }
    }
    return 0;
}

// A walk of a folder whose files travel: the folder, the place its files start with, and the files found
typedef struct
{
    const char *folder;
    const char *under;
    CliTransferred *found;
} CliUnder;

static void cli_under_each(const CliEachNameIsFolderCfg *args)
{
    const void *context = args->context;
    const CliUnder *const walk = (const CliUnder *)context;
    if (args->name[0] == '.')
    {
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    char place[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = walk->folder, .relative = args->name, .joined = path);
    VerbaLinea line = {place, sizeof(place), 0ULL};
    mmgr_line_put(&line, walk->under);
    mmgr_line_put(&line, args->name);
    if (args->is_folder)
    {
        mmgr_line_ch(&line, '/');
    }
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    if (args->is_folder)
    {
        const CliUnder below = {path, place, walk->found};
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path, .each = cli_under_each,
                   .context_bytess = (void *)&below);
        return;
    }
    EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_append,
               CliSettingsSyncTransferredPositionCtx, .transferred_out = walk->found, .place = place, .value = path,
               .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
}

void cli_settings_sync_files(const CliSettingsSyncFolderCtx *args)
{
    EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_clear,
               CliSettingsSyncTransferredPositionCtx, .transferred_out = args->found);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = s_transferred[at], .cap = CLI_POOL_BYTES});
        if (s_transferred[at][length - 1ULL] == '/')
        {
            char dir[CLI_NAME_MAX_LEN];
            memor.cpy(&(MemoriaCfg){
                .dst = dir,
                .src = s_transferred[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_transferred[at], .cap = CLI_POOL_BYTES}) + 1u});
            dir[length - 1ULL] = '\0';
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = dir, .joined = path);
            const CliUnder walk = {path, s_transferred[at], args->found};
            EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path,
                       .each = cli_under_each, .context_bytess = (void *)&walk);
            continue;
        }
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = s_transferred[at],
                   .joined = path);
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_append,
                       CliSettingsSyncTransferredPositionCtx, .transferred_out = args->found,
                       .place = s_transferred[at], .value = path,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
        }
    }
}

// each key of the settings `object` that belongs to one machine alone taken out of it
EMBED_INLINE void sync_machine_keys_remove(const SettingsSyncCtx *args)
{
    CliJson *const json = &args->work_state->json;
    for (unsigned int member = (args->object != CLI_JSON_NONE) ? json->node[args->object].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[member].next;
        const char *const key = json->text + json->node[member].key;
        if (EMBED_CALL(cli_settings_sync_internal.machine, CliSettingsSyncCtx, .key = key))
        {
            EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json,
                       .object = args->object, .name = key);
        }
        member = next;
    }
}

int cli_settings_sync_pack(const CliSettingsSyncCfg *args)
{
    static CliTransferred s_paths;
    static CliSettingsSyncWorkState s_work_state;
    EMBED_CALL(cli_settings_sync_capacity_object_internal.capacity_begin, CliSettingsSyncWorkStateObjectCtx,
               .work_state = &s_work_state);
    EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_clear,
               CliSettingsSyncTransferredPositionCtx, .transferred_out = args->packed);
    EMBED_CALL(cli_settings_sync_folder_internal.files, CliSettingsSyncFolderCtx, .folder = args->home,
               .found = &s_paths);
    for (unsigned int at = 0u; at < s_paths.count; at += 1u)
    {
        const char *const place = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position,
                                             CliSettingsSyncTransferredAtCtx, .synced = &s_paths, .at = at);
        const char *const path = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                            CliSettingsSyncTransferredAtCtx, .synced = &s_paths, .at = at);
        unsigned long long size = 0ULL;
        if (EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size) &&
            (size > CLI_SETTINGS_SYNC_LARGEST))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, " is over 16 MB, the most a file that travels syncs");
            EMBED_CALL(settings_sync_release, SettingsSyncCtx, .paths = &s_paths, .work_state = &s_work_state);
            return 0;
        }
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = path,
                                                      .size_pointer = &size, .owner = g_cli_machine_owner);
        if (bytes == NULL)
        {
            continue;
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_POOL_BYTES}))
        {
            const unsigned int object =
                EMBED_CALL(cli_settings_sync_capacity_internal.settings_get, CliSettingsSyncWorkStateCtx,
                           .work_state = &s_work_state, .text = (const char *)bytes, .size = size, .adjacent = 0ULL);
            EMBED_CALL(sync_machine_keys_remove, SettingsSyncCtx, .work_state = &s_work_state, .object = object);
            const unsigned long long written_capacity =
                EMBED_CALL(cli_json_tree_node_out.json_pretty_capacity, CliJsonTreeNodeOutCfg,
                           .json_read = &s_work_state.json, .indent = 2u);
            char *const written_cell = (object != CLI_JSON_NONE)
                                           ? g_cli_guard->temporary_buf_alloc(written_capacity, g_cli_machine_owner)
                                           : NULL;
            VerbaLinea written = mmgr_line_over(written_cell, written_capacity);
            EMBED_CALL(cli_settings_sync_capacity_object_internal.settings_write,
                       CliSettingsSyncWorkStateObjectCtx, .work_state = &s_work_state, .object = object,
                       .out = &written);
            if (mmgr_line_ok(&written))
            {
                EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_append,
                           CliSettingsSyncTransferredPositionCtx, .transferred_out = args->packed, .place = place,
                           .value = written.out, .size = written.at);
            }
            g_cli_guard->persistent_buf_release(written.out, g_cli_machine_owner);
        }
        else
        {
            EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_append,
                       CliSettingsSyncTransferredPositionCtx, .transferred_out = args->packed, .place = place,
                       .value = (const char *)bytes, .size = size);
        }
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_machine_owner));
    }
    EMBED_CALL(settings_sync_release, SettingsSyncCtx, .paths = &s_paths, .work_state = &s_work_state);
    return 1;
}

// whether the repository at `folder` has a remote
EMBED_INLINE int cli_has_remote(const SettingsSyncCtx *args)
{
    static const char *const s_remote[1] = {"remote"};
    char remotes[CLI_NAME_MAX_LEN * 4u];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea out = {remotes, sizeof(remotes), 0ULL};
    VerbaLinea unrun = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                      .request = s_remote, .count = 1u, .out = &out, .err_msg = &unrun) &&
           (out.at > 0ULL);
}

int cli_settings_sync_read(const CliSettingsSyncFromCfg *args)
{
    static CliTransferred s_paths;
    static CliSettingsSyncWorkState s_work_state;
    EMBED_CALL(cli_settings_sync_capacity_object_internal.capacity_begin, CliSettingsSyncWorkStateObjectCtx,
               .work_state = &s_work_state);
    EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_clear,
               CliSettingsSyncTransferredPositionCtx, .transferred_out = args->there_out);
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->from))
    {
        char git[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->from, .relative = ".git", .joined = git);
        char words[CLI_NAME_MAX_LEN * 4u];
        VerbaLinea pulled = {words, sizeof(words), 0ULL};
        if ((EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) ||
             EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git)) &&
            EMBED_CALL(cli_has_remote, SettingsSyncCtx, .folder = args->from) &&
            !EMBED_CALL(cli_git_root_out_count_err_msg.pull, CliGitRootOutCountErrMsgCfg, .root = args->from,
                        .out = &pulled, .err_msg = args->err_msg))
        {
            EMBED_CALL(settings_sync_release, SettingsSyncCtx, .paths = &s_paths, .work_state = &s_work_state);
            return 0;
        }
        EMBED_CALL(cli_settings_sync_folder_internal.files, CliSettingsSyncFolderCtx, .folder = args->from,
                   .found = &s_paths);
        for (unsigned int at = 0u; at < s_paths.count; at += 1u)
        {
            unsigned long long size = 0ULL;
            const unsigned char *const bytes =
                EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx,
                           .path = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                              CliSettingsSyncTransferredAtCtx, .synced = &s_paths, .at = at),
                           .size_pointer = &size, .owner = g_cli_machine_owner);
            if (bytes != NULL)
            {
                EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_append,
                           CliSettingsSyncTransferredPositionCtx, .transferred_out = args->there_out,
                           .place = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position,
                                               CliSettingsSyncTransferredAtCtx, .synced = &s_paths, .at = at),
                           .value = (const char *)bytes, .size = size);
                g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_machine_owner));
            }
        }
        EMBED_CALL(settings_sync_release, SettingsSyncCtx, .paths = &s_paths, .work_state = &s_work_state);
        return 1;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = args->from,
                                                  .size_pointer = &size, .owner = g_cli_machine_owner);
    if (bytes == NULL)
    {
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg, ": it could not be read");
        EMBED_CALL(settings_sync_release, SettingsSyncCtx, .paths = &s_paths, .work_state = &s_work_state);
        return 0;
    }
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_work_state.json,
                   .text = (const char *)bytes, .size = size, .nodes_more = 0u, .text_more = 0ULL, .err_msg = &unread,
                   .owner = g_cli_machine_owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_machine_owner));
    const char *const mark =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state.json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state.json, .object = root, .name = "orior"),
                   .length_pointer = NULL);
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_work_state.json, .object = root, .name = "files");
    int read =
        (root != CLI_JSON_NONE) && (mark != NULL) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = mark, .other = CLI_SETTINGS_SYNC_MARK, .cap = CLI_POOL_BYTES})) &&
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state.json,
                   .node = files, .kind = CFG_JSON_OBJECT);
    for (unsigned int member = read ? s_work_state.json.node[files].first : CLI_JSON_NONE; member != CLI_JSON_NONE;
         member = s_work_state.json.node[member].next)
    {
        unsigned long long length = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &s_work_state.json, .node = member, .length_pointer = &length);
        const char *const place = s_work_state.json.text + s_work_state.json.node[member].key;
        read = read && (text != NULL);
        if ((text != NULL) && EMBED_CALL(cli_settings_sync_transferred_internal.allow,
                                         CliSettingsSyncTransferredCtx, .place = place))
        {
            EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_append,
                       CliSettingsSyncTransferredPositionCtx, .transferred_out = args->there_out, .place = place,
                       .value = text, .size = length);
        }
    }
    if (!read)
    {
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg,
                      (root == CLI_JSON_NONE) ? ": it is no JSON" : " contains no settings orior_cli synced");
    }
    EMBED_CALL(settings_sync_release, SettingsSyncCtx, .paths = &s_paths, .work_state = &s_work_state);
    return read;
}
