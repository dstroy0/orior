// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_named.c: the checkers the reader names, by name or id: stored by their ids, a checker no longer named taking
// its findings with it, each file the editor has open checked again with them, and the tree where its check is on; the
// names no checker answers to given back
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../toolchains/checkers.h"
#include "servers_internal.h"

static CliLock s_lock = CLI_LOCK_INIT;

// a key and the text beside it put on the line that is `context`, each ended by a 0
static void name_on_put(const CliEachKeyBesideTextSizeCfg *args)
{
    VerbaLinea *const files = args->context;
    mmgr_line_put_n(files, args->key, cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1ull);
    mmgr_line_put_n(files, args->beside,
                    cellul.len(&(CatenaFinitaCfg){.src = args->beside, .cap = CLI_CELLS_POOL}) + 1ull);
}

typedef struct
{
    CliKeyed *keyed;
    unsigned long long *size;
    const char *id;
    const char *key;
    const char *uri;
    const CliServersSink *sink;
} ServersNamedCtx;
// the keys of `keyed` and the texts beside them, each ended by a 0, in a cell of their size, their size into `size`:
// the cell, or NULL where none are or the cell could not be taken; under the store's lock
EMBED_INLINE char *name_files(const ServersNamedCtx *args)
{
    CliKeyed *keyed = args->keyed;
    unsigned long long *size = args->size;
    const unsigned long long room = EMBED_CALL(cli_keyed_table.listed_size, CliKeyedCfg, .keyed = keyed) + 1ull;
    char *const text = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea files = mmgr_line_over(text, room);
    EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = keyed, .each = name_on_put, .context = &files);
    *size = mmgr_line_ok(&files) ? files.at : 0ull;
    return text;
}

// whether `id` is among the checkers named
EMBED_INLINE int name_among(const ServersNamedCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_servers.checkers; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.checker[at], .other = args->id, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return cellul.eq(&(CatenaFinitaCfg){.src = args->id, .other = CLI_SERVERS_OWN, .cap = CLI_CELLS_POOL});
}

// the findings of the checkers no longer named taken out of the file of `key`: 1 where it had some; under the
// servers' and the checks' locks. The file's entry is read into a tree of its size and written into a cell of the most
// the tree writes to
EMBED_INLINE int name_drop(const ServersNamedCtx *args)
{
    const char *key = args->key;
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const stored =
        EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &g_cli_servers_checked, .key = key,
                   .beside = &beside, .size = &size);
    if (stored == NULL)
    {
        return 0;
    }
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const unsigned int entry = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                          .json = &json, .text = stored, .size = size, .nodes_more = 0u,
                                          .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_servers_cells);
    int dropped = 0;
    unsigned int id = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                 .node = entry, .kind = CFG_JSON_OBJECT)
                          ? json.node[entry].first
                          : CLI_JSON_NONE;
    while (id != CLI_JSON_NONE)
    {
        const unsigned int next = json.node[id].next;
        const char *const name = json.text + json.node[id].key;
        if (!EMBED_CALL(name_among, ServersNamedCtx, .id = name))
        {
            EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = &json, .object = entry,
                       .name = name);
            dropped = 1;
        }
        id = next;
    }
    if (dropped)
    {
        const unsigned long long room =
            EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = &json);
        char *const entry_written = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
        VerbaLinea written = mmgr_line_over(entry_written, room);
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = entry,
                   .out = &written, .indent = 0u, .sorted = 1);
        if (mmgr_line_ok(&written))
        {
            (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &g_cli_servers_checked,
                             .key = key, .beside_value = args->uri, .text = written.out, .size_value = written.at);
        }
        g_cli_cells->persistent_buf_release(entry_written, &g_cli_servers_cells);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return dropped;
}

// the checkers no longer named take their findings with them, each file that had some passed on whole
EMBED_INLINE void name_findings_drop(const ServersNamedCtx *args)
{
    const CliServersSink *sink = args->sink;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_checks_lock);
    unsigned long long size = 0ull;
    char *const files = EMBED_CALL(name_files, ServersNamedCtx, .keyed = &g_cli_servers_checked, .size = &size);
    // the files that had findings of a checker gone are among those listed
    char *const passed = g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_servers_cells);
    VerbaLinea dropped = mmgr_line_over(passed, size + 1ull);
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const key = files + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        if (EMBED_CALL(name_drop, ServersNamedCtx, .key = key, .uri = uri))
        {
            mmgr_line_put_n(&dropped, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1ull);
            mmgr_line_put_n(&dropped, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1ull);
        }
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_checks_lock);
    g_cli_cells->persistent_buf_release(files, &g_cli_servers_cells);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    const unsigned long long dropped_size = mmgr_line_ok(&dropped) ? dropped.at : 0ull;
    for (unsigned long long at = 0ull; at < dropped_size;)
    {
        const char *const key = passed + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        EMBED_CALL(cli_servers_sink_key_internal.emit_whole, CliServersSinkKeyCtx, .key = key, .uri = uri,
                   .sink = sink);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    g_cli_cells->persistent_buf_release(passed, &g_cli_servers_cells);
}

int cli_servers_set_checkers(const CliServersRootSinkOutCfg *args)
{
    const CliServersSink *sink = args->sink;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned long long start = args->out->at;
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    g_cli_servers.checkers = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->names[at],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->names[at], .cap = CLI_CELLS_POOL}),
                   .from_u64s = &from, .to_u64s = &to);
        char name[CLI_NAME_MAX_LEN];
        if ((to == from) || (to - from >= sizeof(name)))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = name, .src = args->names[at] + from, .bytes = (size_t)(to - from)});
        name[to - from] = '\0';
        char id[CLI_NAME_MAX_LEN];
        if (EMBED_CALL(cli_checkers_name.by_name, CliCheckersNameCfg, .name = name, .id_out = id) &&
            (g_cli_servers.checkers < CLI_SERVERS_CHECKERS))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = g_cli_servers.checker[g_cli_servers.checkers], .src = id, .bytes = sizeof(id)});
            g_cli_servers.checkers += 1u;
            continue;
        }
        mmgr_line_put(args->out, (args->out->at > start + 1u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = name,
                   .length = to - from);
    }
    mmgr_line_ch(args->out, ']');
    // a checker no longer named takes its findings with it; the servers' lock is given back there
    EMBED_CALL(name_findings_drop, ServersNamedCtx, .sink = sink);
    if (args->root == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return 1;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    g_cli_servers.sink = *sink;
    g_cli_servers.has_sink = 1;
    VerbaLinea checked = {g_cli_servers.checked_root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&checked, args->root);
    (void)mmgr_line_finish(&checked);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    unsigned long long size = 0ull;
    char *const files_text = EMBED_CALL(name_files, ServersNamedCtx, .keyed = &g_cli_servers_texts, .size = &size);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const key = files_text + at;
        const char *const path = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        unsigned long long text_size = 0ull;
        char *const text = EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg, .path = path,
                                      .size_pointer = &text_size);
        if (text != NULL)
        {
            EMBED_CALL(cli_servers_path_size_root_sink_internal.run_checkers, CliServersPathSizeRootSinkCtx,
                       .root = args->root, .path = path, .text = text, .size = text_size, .disk = 1, .sink = sink);
        }
        g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    }
    g_cli_cells->persistent_buf_release(files_text, &g_cli_servers_cells);
    if (cli_servers_check_on())
    {
        EMBED_CALL(cli_servers_files_internal.check_tree_with_checkers, CliServersFilesCtx, .root = args->root,
                   .sink = sink);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return 1;
}
