// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_sync_import.c: what is synced here compared with orior's own folder, taken in as the reader chooses with
// each file it writes over saved, and an import undone
#include "../command_line/pool.h"
#include "../text/text.h"
#include "settings_sync_internal.h"

// the max keys a settings comparison reads
#define CLI_SETTINGS_SYNC_KEYS 4096u

// The work state import needs: what travels here, the settings read, the keys of two settings, and an import
typedef struct
{
    CliTransferred here;
    CliSettingsSyncWorkState work_state;
    const char *key[CLI_SETTINGS_SYNC_KEYS];
    unsigned int keys;
    CliAllocating allocating;
} CliAllocWorkState;

static CliAllocWorkState s_alloc;

typedef struct
{
    unsigned int object;
    unsigned int ours_index;
    unsigned int theirs;
    VerbaLinea *out;
    unsigned int value;
    const CliTransferred *there;
    unsigned int ours_at;
    unsigned int theirs_at;
    const char *to;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *before;
    unsigned long long before_size;
    const char *const *keys;
    unsigned int key_count;
    const char *home;
    const char *backup_dir;
    const char *place;
} SettingsSyncImportCtx;
// the members of a settings object
EMBED_INLINE unsigned int cli_member_first(const SettingsSyncImportCtx *args)
{
    return s_alloc.work_state.json.node[args->object].first;
}

// the keys of two settings objects, in order, each once, less the settings of this machine alone
EMBED_INLINE void cli_keys_get(const SettingsSyncImportCtx *args)
{
    const CliJson *const json = &s_alloc.work_state.json;
    s_alloc.keys = 0u;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        for (unsigned int member = EMBED_CALL(cli_member_first, SettingsSyncImportCtx,
                                              .object = (side == 0u) ? args->ours_index : args->theirs);
             member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            unsigned int at = 0u;
            while ((at < s_alloc.keys) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                      .one = s_alloc.key[at], .other_pointer = key) < 0))
            {
                at += 1u;
            }
            if (((at < s_alloc.keys) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_alloc.key[at], .other = key, .cap = CLI_POOL_BYTES}))) ||
                EMBED_CALL(cli_settings_sync_internal.machine, CliSettingsSyncCtx, .key = key) ||
                (s_alloc.keys >= CLI_SETTINGS_SYNC_KEYS))
            {
                continue;
            }
            for (unsigned int move = s_alloc.keys; move > at; move -= 1u)
            {
                s_alloc.key[move] = s_alloc.key[move - 1u];
            }
            s_alloc.key[at] = key;
            s_alloc.keys += 1u;
        }
    }
}

// a setting's value written as JSON in a string, or null where it is not set
EMBED_INLINE void cli_value_write(const SettingsSyncImportCtx *args)
{
    VerbaLinea *out = args->out;
    const unsigned long long line_capacity = EMBED_CALL(cli_json_tree_node_out.json_write_capacity,
                                                        CliJsonTreeNodeOutCfg, .json_read = &s_alloc.work_state.json);
    char *const line_cell =
        (args->value != CLI_JSON_NONE) ? g_cli_guard->temporary_buf_alloc(line_capacity, g_cli_machine_owner) : NULL;
    VerbaLinea line = mmgr_line_over(line_cell, line_capacity);
    if (line_cell == NULL)
    {
        mmgr_line_put(out, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_alloc.work_state.json,
               .node = args->value, .out = &line, .indent = 0u, .sorted = 0);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = line.out,
               .length = mmgr_line_ok(&line) ? line.at : 0ULL);
    g_cli_guard->persistent_buf_release(line.out, g_cli_machine_owner);
}

// the settings that differ between two settings texts written, each with its value here and there: 1 where any do
EMBED_INLINE int cli_settings_differ(const SettingsSyncImportCtx *args)
{
    VerbaLinea *out = args->out;
    const CliJson *const json = &s_alloc.work_state.json;
    const unsigned long long theirs_size =
        (args->theirs_at < args->there->count) ? args->there->size[args->theirs_at] : 0ULL;
    const unsigned int ours_index = EMBED_CALL(
        cli_settings_sync_capacity_internal.settings_get, CliSettingsSyncWorkStateCtx,
        .work_state = &s_alloc.work_state,
        .text = (args->ours_at < s_alloc.here.count)
                    ? EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                 CliSettingsSyncTransferredAtCtx, .synced = &s_alloc.here, .at = args->ours_at)
                    : "",
        .size = (args->ours_at < s_alloc.here.count) ? s_alloc.here.size[args->ours_at] : 0ULL,
        .adjacent = theirs_size);
    unsigned int theirs;
    if (ours_index != CLI_JSON_NONE)
    {
        theirs = EMBED_CALL(cli_settings_sync_capacity_internal.settings_adjacent, CliSettingsSyncWorkStateCtx,
                            .work_state = &s_alloc.work_state,
                            .text = (args->theirs_at < args->there->count)
                                        ? EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                                     CliSettingsSyncTransferredAtCtx, .synced = args->there,
                                                     .at = args->theirs_at)
                                        : "",
                            .size = theirs_size);
    }
    else
    {
        theirs = CLI_JSON_NONE;
    }
    if (theirs == CLI_JSON_NONE)
    {
        return 0;
    }
    EMBED_CALL(cli_keys_get, SettingsSyncImportCtx, .ours_index = ours_index, .theirs = theirs);
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_alloc.keys; at += 1u)
    {
        const unsigned int here = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = ours_index, .name = s_alloc.key[at]);
        const unsigned int away = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = theirs, .name = s_alloc.key[at]);
        const int same = (here != CLI_JSON_NONE) && (away != CLI_JSON_NONE) &&
                         EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json, .one = here,
                                    .other_json = json, .other = away);
        if (same || ((here == CLI_JSON_NONE) && (away == CLI_JSON_NONE)))
        {
            continue;
        }
        mmgr_line_put(out, (written != 0u) ? ",{\"key\":" : "{\"key\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = s_alloc.key[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = s_alloc.key[at], .cap = CLI_POOL_BYTES}));
        mmgr_line_put(out, ",\"here\":");
        EMBED_CALL(cli_value_write, SettingsSyncImportCtx, .out = out, .value = here);
        mmgr_line_put(out, ",\"there\":");
        EMBED_CALL(cli_value_write, SettingsSyncImportCtx, .out = out, .value = away);
        mmgr_line_ch(out, '}');
        written += 1u;
    }
    return written != 0u;
}

void cli_settings_sync_differences(const CliSettingsSyncHomeCfg *args)
{
    CliGrownLine settings;
    cli_grown_begin(&settings, CLI_SETTINGS_SYNC_WRITTEN_CAPACITY, g_cli_machine_owner);
    if (settings.cell == NULL)
    {
        mmgr_line_put(args->out, "[]");
        return;
    }
    EMBED_CALL(cli_settings_sync_capacity_object_internal.capacity_begin, CliSettingsSyncWorkStateObjectCtx,
               .work_state = &s_alloc.work_state);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unpacked = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_settings_sync.pack, CliSettingsSyncCfg, .home = args->home, .packed = &s_alloc.here,
                    .err_msg = &unpacked))
    {
        EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_clear,
                   CliSettingsSyncTransferredPositionCtx, .transferred_out = &s_alloc.here);
    }
    mmgr_line_ch(args->out, '[');
    unsigned int written = 0u;
    unsigned int ours_at = 0u;
    unsigned int theirs_at = 0u;
    // the places of both sides, in order
    while ((ours_at < s_alloc.here.count) || (theirs_at < args->there->count))
    {
        const int has_ours = ours_at < s_alloc.here.count;
        const int has_theirs = theirs_at < args->there->count;
        int order;
        if (!has_ours)
        {
            order = 1;
        }
        else if (!has_theirs)
        {
            order = -1;
        }
        else
        {
            order = EMBED_CALL(
                cli_text_size_one.order, CliTextSizeOneCfg,
                .one = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position,
                                  CliSettingsSyncTransferredAtCtx, .synced = &s_alloc.here, .at = ours_at),
                .other_pointer = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position,
                                            CliSettingsSyncTransferredAtCtx, .synced = args->there, .at = theirs_at));
        }
        const unsigned int ours_index = (order <= 0) ? ours_at : s_alloc.here.count;
        const unsigned int theirs = (order >= 0) ? theirs_at : args->there->count;
        const char *const place =
            (order <= 0) ? EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position,
                                      CliSettingsSyncTransferredAtCtx, .synced = &s_alloc.here, .at = ours_at)
                         : EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position,
                                      CliSettingsSyncTransferredAtCtx, .synced = args->there, .at = theirs_at);
        ours_at += (order <= 0) ? 1u : 0u;
        theirs_at += (order >= 0) ? 1u : 0u;
        const char *how = NULL;
        cli_grown_reset(&settings);
        if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_POOL_BYTES}))
        {
            how = EMBED_CALL(cli_settings_differ, SettingsSyncImportCtx, .there = args->there, .ours_at = ours_index,
                             .theirs_at = theirs, .out = &settings.line)
                      ? "changed"
                      : NULL;
        }
        else if ((ours_index < s_alloc.here.count) && (theirs < args->there->count))
        {
            const int same =
                (s_alloc.here.size[ours_index] == args->there->size[theirs]) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                       CliSettingsSyncTransferredAtCtx, .synced = &s_alloc.here, .at = ours_index),
                     .other = EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                         CliSettingsSyncTransferredAtCtx, .synced = args->there, .at = theirs),
                     .bytes = args->there->size[theirs]}) == 0);
            how = same ? NULL : "changed";
        }
        else
        {
            how = (ours_index < s_alloc.here.count) ? "here" : "there";
        }
        if (how == NULL)
        {
            continue;
        }
        mmgr_line_put(args->out, (written != 0u) ? ",{\"place\":" : "{\"place\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = place,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_POOL_BYTES}));
        mmgr_line_put(args->out, ",\"how\":\"");
        mmgr_line_put(args->out, how);
        mmgr_line_put(args->out, "\",\"settings\":[");
        mmgr_line_put_n(args->out, settings.cell, mmgr_line_ok(&settings.line) ? cli_grown_size(&settings) : 0ULL);
        mmgr_line_put(args->out, "]}");
        written += 1u;
    }
    mmgr_line_ch(args->out, ']');
    EMBED_CALL(cli_settings_sync_transferred_position_internal.transferred_clear, CliSettingsSyncTransferredPositionCtx,
               .transferred_out = &s_alloc.here);
    g_cli_guard->persistent_buf_release(s_alloc.work_state.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_alloc.work_state.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_alloc.work_state.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    cli_grown_end(&settings);
}

// the text of the file at `path` written whole at `to`, the folder above it made: 1, or 0 with why given
EMBED_INLINE int cli_write_on_to(const SettingsSyncImportCtx *args)
{
    unsigned long long size = args->size;
    char parent[CLI_PATH_MAX_LEN];
    if ((EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->to, .out = parent) &&
         !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent)) ||
        !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = args->to,
                    .bytes = args->text, .size = size))
    {
        mmgr_line_put(args->err_msg, args->to);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// the settings of this machine retained and of `keys` taken from there, written into `out`: 1, or 0 where the settings'
// tree is refused its cell or they do not fit
EMBED_INLINE int cli_settings_alloc(const SettingsSyncImportCtx *args)
{
    VerbaLinea *out = args->out;
    CliJson *const json = &s_alloc.work_state.json;
    const unsigned int theirs_at =
        EMBED_CALL(cli_settings_sync_transferred_internal.transferred_find, CliSettingsSyncTransferredCtx,
                   .synced = args->there, .place = "settings.json");
    const unsigned long long theirs_size = (theirs_at < args->there->count) ? args->there->size[theirs_at] : 0ULL;
    const unsigned int ours_index = EMBED_CALL(
        cli_settings_sync_capacity_internal.settings_get, CliSettingsSyncWorkStateCtx,
        .work_state = &s_alloc.work_state, .text = args->before, .size = args->before_size, .adjacent = theirs_size);
    unsigned int theirs;
    if (ours_index != CLI_JSON_NONE)
    {
        theirs =
            EMBED_CALL(cli_settings_sync_capacity_internal.settings_adjacent, CliSettingsSyncWorkStateCtx,
                       .work_state = &s_alloc.work_state,
                       .text = (theirs_at < args->there->count)
                                   ? EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                                CliSettingsSyncTransferredAtCtx, .synced = args->there, .at = theirs_at)
                                   : "",
                       .size = theirs_size);
    }
    else
    {
        theirs = CLI_JSON_NONE;
    }
    if (theirs == CLI_JSON_NONE)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->key_count; at += 1u)
    {
        if (EMBED_CALL(cli_settings_sync_internal.machine, CliSettingsSyncCtx, .key = args->keys[at]))
        {
            continue;
        }
        const unsigned int value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = theirs, .name = args->keys[at]);
        if (value != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = ours_index,
                       .name = args->keys[at],
                       .value = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = value));
        }
        else
        {
            EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json,
                       .object = ours_index, .name = args->keys[at]);
        }
    }
    EMBED_CALL(cli_settings_sync_capacity_object_internal.settings_write, CliSettingsSyncWorkStateObjectCtx,
               .work_state = &s_alloc.work_state, .object = ours_index, .out = out);
    return mmgr_line_ok(out);
}

// one place taken in: its file saved in `backup_dir` where one was, and written as there: 1, or 0 with why given
EMBED_INLINE int cli_position_alloc(const SettingsSyncImportCtx *args)
{
    const char *place = args->place;
    CliGrownLine settings;
    cli_grown_begin(&settings, CLI_SETTINGS_SYNC_WRITTEN_CAPACITY, g_cli_machine_owner);
    if (settings.cell == NULL)
    {
        mmgr_line_put(args->err_msg, "the settings written could not be given a cell");
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = place, .joined = path);
    unsigned long long before_size = 0ULL;
    const unsigned char *const before = EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = path,
                                                   .size_pointer = &before_size, .owner = g_cli_machine_owner);
    const char *text = NULL;
    unsigned long long size = 0ULL;
    int reserved = 1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_POOL_BYTES}))
    {
        reserved =
            EMBED_CALL(cli_settings_alloc, SettingsSyncImportCtx,
                       .before = (before != NULL) ? (const char *)before : "", .before_size = before_size,
                       .there = args->there, .keys = args->keys, .key_count = args->key_count, .out = &settings.line);
        if (!reserved)
        {
            mmgr_line_put(args->err_msg, "the settings taken could not be given cells");
        }
        text = settings.cell;
        size = cli_grown_size(&settings);
    }
    else
    {
        const unsigned int at = EMBED_CALL(cli_settings_sync_transferred_internal.transferred_find,
                                           CliSettingsSyncTransferredCtx, .synced = args->there, .place = place);
        text = (at < args->there->count) ? EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value,
                                                      CliSettingsSyncTransferredAtCtx, .synced = args->there, .at = at)
                                         : NULL;
        size = (at < args->there->count) ? args->there->size[at] : 0ULL;
    }
    if (reserved && (text != NULL) && (before != NULL))
    {
        char saved[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->backup_dir, .relative = place, .joined = saved);
        reserved = EMBED_CALL(cli_write_on_to, SettingsSyncImportCtx, .to = saved, .text = (const char *)before,
                              .size = before_size, .err_msg = args->err_msg);
    }
    // the file is let go of before it is written over: a file still mapped is not truncated on Windows, and reading a
    // view of one that was is a fault elsewhere; what the writing needs of it is in the settings written and the saved
    // copy already
    const unsigned int existed = (before != NULL) ? 1u : 0u;
    g_cli_guard->persistent_buf_release((void *)(embed_word)before, (g_cli_machine_owner));
    if (reserved && (text != NULL))
    {
        reserved = EMBED_CALL(cli_write_on_to, SettingsSyncImportCtx, .to = path, .text = text, .size = size,
                              .err_msg = args->err_msg);
    }
    if (reserved && (text != NULL) && (s_alloc.allocating.files < CLI_ALLOCATING_FILES))
    {
        (void)cellul.copy(&(CatenaFinitaCfg){
            .dst = s_alloc.allocating.place[s_alloc.allocating.files], .src = place, .cap = CLI_NAME_MAX_LEN});
        s_alloc.allocating.existed[s_alloc.allocating.files] = (unsigned char)((existed != 0u) ? 1u : 0u);
        s_alloc.allocating.files += 1u;
    }
    cli_grown_end(&settings);
    return reserved;
}

int cli_settings_sync_alloc(const CliSettingsSyncHomeCfg *args)
{
    CliGrownLine written;
    cli_grown_begin(&written, CLI_SETTINGS_SYNC_WRITTEN_CAPACITY, g_cli_machine_owner);
    if (written.cell == NULL)
    {
        mmgr_line_put(args->err_msg, "an import written could not be given a cell");
        return 0;
    }
    EMBED_CALL(cli_settings_sync_capacity_object_internal.capacity_begin, CliSettingsSyncWorkStateObjectCtx,
               .work_state = &s_alloc.work_state);
    CliAllocating *const allocating = &s_alloc.allocating;
    allocating->place = g_cli_guard->temporary_buf_alloc(CLI_ALLOCATING_BYTES, g_cli_machine_owner);
    if (allocating->place == NULL)
    {
        mmgr_line_put(args->err_msg, "an import written could not be given a cell");
        cli_grown_end(&written);
        return 0;
    }
    allocating->existed = (unsigned char *)(void *)(allocating->place + CLI_ALLOCATING_FILES);
    allocating->at = cli_wall_milliseconds();
    VerbaLinea id = {allocating->id, sizeof(allocating->id), 0ULL};
    mmgr_line_decimal(&id, allocating->at, 0u);
    mmgr_line_finish(&id);
    memor.cpy(&(MemoriaCfg){.dst = allocating->from,
                            .src = args->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->from, .cap = CLI_POOL_BYTES}) + 1u});
    allocating->files = 0u;
    char synced[CLI_PATH_MAX_LEN];
    char backup_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = "synced", .joined = synced);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = synced, .relative = allocating->id, .joined = backup_dir);
    int reserved = 1;
    for (unsigned int at = 0u; reserved && (at < args->count); at += 1u)
    {
        if (EMBED_CALL(cli_settings_sync_transferred_internal.allow, CliSettingsSyncTransferredCtx,
                       .place = args->positions[at]))
        {
            reserved = EMBED_CALL(cli_position_alloc, SettingsSyncImportCtx, .home = args->home,
                                  .backup_dir = backup_dir, .place = args->positions[at], .there = args->there,
                                  .keys = args->keys, .key_count = args->key_count, .err_msg = args->err_msg);
        }
    }
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = backup_dir, .relative = "import.json", .joined = file);
    EMBED_CALL(cli_settings_sync_out_internal.allocating_write, CliSettingsSyncOutCtx, .allocating = allocating,
               .pretty = 1, .out = &written.line);
    reserved = reserved && mmgr_line_ok(&written.line) &&
               EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = backup_dir) &&
               EMBED_CALL(cli_write_on_to, SettingsSyncImportCtx, .to = file, .text = written.cell,
                          .size = cli_grown_size(&written), .err_msg = args->err_msg);
    if (reserved)
    {
        EMBED_CALL(cli_settings_sync_out_internal.allocating_write, CliSettingsSyncOutCtx, .allocating = allocating,
                   .pretty = 0, .out = args->out);
    }
    g_cli_guard->persistent_buf_release(s_alloc.work_state.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_alloc.work_state.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_alloc.work_state.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(allocating->place, g_cli_machine_owner);
    allocating->place = NULL;
    cli_grown_end(&written);
    return reserved;
}
