// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_sync_undo.c: the imports saved, read back and written, and an import undone
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "settings_sync_internal.h"

void cli_settings_sync_allocating_write(const CliSettingsSyncOutCtx *args)
{
    mmgr_line_put(args->out, args->pretty ? "{\n  \"id\": " : "{\"id\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->allocating->id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->allocating->id, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->out, args->pretty ? ",\n  \"at\": " : ",\"at\":");
    mmgr_line_decimal(args->out, args->allocating->at, 0u);
    mmgr_line_put(args->out, args->pretty ? ",\n  \"from\": " : ",\"from\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->allocating->from,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->allocating->from, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->out, args->pretty ? ",\n  \"files\": [" : ",\"files\":[");
    for (unsigned int at = 0u; at < args->allocating->files; at += 1u)
    {
        if (args->pretty)
        {
            mmgr_line_put(args->out, (at != 0u) ? ",\n    [\n      " : "\n    [\n      ");
        }
        else
        {
            mmgr_line_put(args->out, (at != 0u) ? ",[" : "[");
        }
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->allocating->place[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->allocating->place[at], .cap = CLI_POOL_BYTES}));
        mmgr_line_put(args->out, args->pretty ? ",\n      " : ",");
        mmgr_line_put(args->out, args->allocating->existed[at] ? "true" : "false");
        mmgr_line_put(args->out, args->pretty ? "\n    ]" : "]");
    }
    if (args->pretty)
    {
        mmgr_line_put(args->out, (args->allocating->files != 0u) ? "\n  ]\n}" : "]\n}");
    }
    else
    {
        mmgr_line_put(args->out, "]}");
    }
}

// The imports saved: each one's folder name and when it was taken, and the work state one is read into
typedef struct
{
    char (*name)[64];
    unsigned long long at[CLI_ALLOCATIONS];
    unsigned int count;
    const char *synced;
    CliAllocating allocating;
    CliSettingsSyncWorkState work_state;
} CliAllocations;

static CliAllocations s_allocations;

// the lock taken and the work state begun empty under it
static int undo_lock(void)
{
    // the imports' names and the import read are in one cell for as long as the work runs
    s_allocations.name =
        g_cli_guard->temporary_buf_alloc((CLI_ALLOCATIONS * 64ULL) + CLI_ALLOCATING_BYTES, g_cli_machine_owner);
    if (s_allocations.name == NULL)
    {
        return 0;
    }
    s_allocations.allocating.place = (char (*)[CLI_NAME_MAX_LEN])(void *)(s_allocations.name + CLI_ALLOCATIONS);
    s_allocations.allocating.existed = (unsigned char *)(void *)(s_allocations.allocating.place + CLI_ALLOCATING_FILES);
    EMBED_CALL(cli_settings_sync_capacity_object_internal.capacity_begin, CliSettingsSyncWorkStateObjectCtx,
               .work_state = &s_allocations.work_state);
    return 1;
}

typedef struct
{
    const char *file;
    CliAllocating *allocating;
    const char *home;
    char *synced;
    const char *transferred_read;
    const char *name;
} SettingsSyncUndoCtx;
// the import saved at `file` read into `allocating`: 1, or 0 where it is not there or not one
static int cli_allocating_read(const SettingsSyncUndoCtx *args)
{
    CliJson *const json = &s_allocations.work_state.json;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->file,
                                                  .size_pointer = &size, .owner = g_cli_machine_owner);
    if (bytes == NULL)
    {
        return 0;
    }
    g_cli_guard->persistent_buf_release(json->node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = json, .text = (const char *)bytes, .size = size, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &unread, .owner = g_cli_machine_owner);
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_machine_owner));
    const char *const id =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = root, .name = "id"),
                   .length_pointer = NULL);
    const char *const from =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = root, .name = "from"),
                   .length_pointer = NULL);
    const unsigned int at = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = root, .name = "at");
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = root, .name = "files");
    if ((id == NULL) || (from == NULL) ||
        !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = at,
                    .type = CFG_JSON_NUMBER) ||
        !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = files,
                    .type = CFG_JSON_ARRAY))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->allocating->id,
                            .src = id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->allocating->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES}) + 1u});
    args->allocating->at = 0ULL;
    const CliJsonNode *const number = &json->node[at];
    for (unsigned long long digit = 0ULL; digit < number->length; digit += 1ULL)
    {
        const char character = json->text[number->text + digit];
        if ((character < '0') || (character > '9'))
        {
            return 0;
        }
        args->allocating->at = (args->allocating->at * 10ULL) + (unsigned long long)(character - '0');
    }
    args->allocating->files = 0u;
    for (unsigned int one = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = files, .index = one) != CLI_JSON_NONE;
         one += 1u)
    {
        const unsigned int pair = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = files, .index = one);
        const char *const place =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = pair, .index = 0u),
                       .length_pointer = NULL);
        const unsigned int existed = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = json, .array = pair, .index = 1u);
        const int truth = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                     .node = existed, .type = CFG_JSON_TRUE);
        if ((place == NULL) || (!truth && !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                      .json_read = json, .node = existed, .type = CFG_JSON_FALSE)))
        {
            return 0;
        }
        if (args->allocating->files < CLI_ALLOCATING_FILES)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->allocating->place[args->allocating->files],
                                    .src = place,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_POOL_BYTES}) + 1u});
            args->allocating->existed[args->allocating->files] = truth ? 1u : 0u;
            args->allocating->files += 1u;
        }
    }
    return 1;
}

static void cli_allocating_each(const CliEachNameIsFolderCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_allocations.synced, .relative = args->name,
               .joined = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "import.json", .joined = file);
    if (!args->is_folder || (s_allocations.count >= CLI_ALLOCATIONS) ||
        (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) >= 64u) ||
        !EMBED_CALL(cli_allocating_read, SettingsSyncUndoCtx, .file = file, .allocating = &s_allocations.allocating))
    {
        return;
    }
    // the imports saved by when they were taken, the last first, those taken at once in the order they were read
    unsigned int place = s_allocations.count;
    while ((place > 0u) && (s_allocations.at[place - 1u] < s_allocations.allocating.at))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_allocations.name[place],
            .src = s_allocations.name[place - 1u],
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = s_allocations.name[place - 1u], .cap = CLI_POOL_BYTES}) + 1u});
        s_allocations.at[place] = s_allocations.at[place - 1u];
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_allocations.name[place],
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
    s_allocations.at[place] = s_allocations.allocating.at;
    s_allocations.count += 1u;
}

// every import saved under `home`, by its folder, the last first
EMBED_INLINE void cli_allocations_read(const SettingsSyncUndoCtx *args)
{
    char *synced = args->synced;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = "synced", .joined = synced);
    s_allocations.synced = synced;
    s_allocations.count = 0u;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = synced,
               .each = cli_allocating_each, .context_bytess = NULL);
}

// the import saved in the folder `name` read into the work state's import: 1, or 0
EMBED_INLINE int cli_allocating_get(const SettingsSyncUndoCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->transferred_read, .relative = args->name,
               .joined = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "import.json", .joined = file);
    return EMBED_CALL(cli_allocating_read, SettingsSyncUndoCtx, .file = file, .allocating = &s_allocations.allocating);
}

void cli_settings_sync_allocations(const CliSettingsSyncHomeToCfg *args)
{
    if (!undo_lock())
    {
        mmgr_line_put(args->out, "[]");
        return;
    }
    char synced[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_allocations_read, SettingsSyncUndoCtx, .home = args->home, .synced = synced);
    mmgr_line_ch(args->out, '[');
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_allocations.count; at += 1u)
    {
        if (EMBED_CALL(cli_allocating_get, SettingsSyncUndoCtx, .transferred_read = synced,
                       .name = s_allocations.name[at]))
        {
            mmgr_line_put(args->out, (written != 0u) ? "," : "");
            EMBED_CALL(cli_settings_sync_out_internal.allocating_write, CliSettingsSyncOutCtx,
                       .allocating = &s_allocations.allocating, .pretty = 0, .out = args->out);
            written += 1u;
        }
    }
    mmgr_line_ch(args->out, ']');
    g_cli_guard->persistent_buf_release(
        s_allocations.work_state.json.node,
        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_allocations.work_state.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_allocations.work_state.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_allocations.name, g_cli_machine_owner);
    s_allocations.name = NULL;
}

int cli_settings_sync_undo(const CliSettingsSyncHomeOutCfg *args)
{
    if (!undo_lock())
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the imports saved");
        return 0;
    }
    char synced[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_allocations_read, SettingsSyncUndoCtx, .home = args->home, .synced = synced);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < s_allocations.count); at += 1u)
    {
        found = EMBED_CALL(cli_allocating_get, SettingsSyncUndoCtx, .transferred_read = synced,
                           .name = s_allocations.name[at]) &&
                (cellul.eq(
                    &(CatenaFinitaCfg){.src = s_allocations.allocating.id, .other = args->id, .cap = CLI_POOL_BYTES}));
    }
    if (!found)
    {
        mmgr_line_put(args->err_msg, "no import ");
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is saved");
        g_cli_guard->persistent_buf_release(
            s_allocations.work_state.json.node,
            mmgr_owner_of_buf(cli_pool_series_cellularum(), s_allocations.work_state.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_allocations.work_state.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(s_allocations.name, g_cli_machine_owner);
        s_allocations.name = NULL;
        return 0;
    }
    const CliAllocating *const allocating = &s_allocations.allocating;
    char backup_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = synced, .relative = allocating->id, .joined = backup_dir);
    for (unsigned int at = 0u; at < allocating->files; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        char saved[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = allocating->place[at],
                   .joined = path);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = backup_dir, .relative = allocating->place[at],
                   .joined = saved);
        if (allocating->existed[at])
        {
            unsigned long long size = 0ULL;
            const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved,
                                                          .size_pointer = &size, .owner = g_cli_machine_owner);
            const int back =
                (bytes != NULL) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                              .path = path, .bytes = bytes, .size = size);
            g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_machine_owner));
            if (!back)
            {
                mmgr_line_put(args->err_msg, (bytes == NULL) ? saved : path);
                mmgr_line_put(args->err_msg, (bytes == NULL) ? ": it could not be read" : ": it could not be written");
                g_cli_guard->persistent_buf_release(
                    s_allocations.work_state.json.node,
                    mmgr_owner_of_buf(cli_pool_series_cellularum(), s_allocations.work_state.json.node));
                EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg,
                           .json = (&s_allocations.work_state.json), .node_json_node_pointer = NULL, .nodes_max = 0u,
                           .text_out = NULL, .text_max = 0ULL);
                g_cli_guard->persistent_buf_release(s_allocations.name, g_cli_machine_owner);
                s_allocations.name = NULL;
                return 0;
            }
        }
        else if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) &&
                 !EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, ": it could not be taken out");
            g_cli_guard->persistent_buf_release(
                s_allocations.work_state.json.node,
                mmgr_owner_of_buf(cli_pool_series_cellularum(), s_allocations.work_state.json.node));
            EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg,
                       .json = (&s_allocations.work_state.json), .node_json_node_pointer = NULL, .nodes_max = 0u,
                       .text_out = NULL, .text_max = 0ULL);
            g_cli_guard->persistent_buf_release(s_allocations.name, g_cli_machine_owner);
            s_allocations.name = NULL;
            return 0;
        }
    }
    if (!EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = backup_dir))
    {
        mmgr_line_put(args->err_msg, backup_dir);
        mmgr_line_put(args->err_msg, ": it could not be taken out");
        g_cli_guard->persistent_buf_release(
            s_allocations.work_state.json.node,
            mmgr_owner_of_buf(cli_pool_series_cellularum(), s_allocations.work_state.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_allocations.work_state.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(s_allocations.name, g_cli_machine_owner);
        s_allocations.name = NULL;
        return 0;
    }
    EMBED_CALL(cli_settings_sync_out_internal.allocating_write, CliSettingsSyncOutCtx, .allocating = allocating,
               .pretty = 0, .out = args->out);
    g_cli_guard->persistent_buf_release(
        s_allocations.work_state.json.node,
        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_allocations.work_state.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_allocations.work_state.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_allocations.name, g_cli_machine_owner);
    s_allocations.name = NULL;
    return 1;
}
