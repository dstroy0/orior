// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_findings.c: a checker's findings stored, by file, each file's findings by the checkers' ids in order, and the
// diagnostics of each file whose findings from that checker changed passed on whole
#include "../command_line/cells.h"
#include "servers_internal.h"

// the findings read, under the reading's lock
static CliServersReadState s_found = {0};

typedef struct
{
    unsigned int pair;
    char *uri;
    char *key;
    const char *key_read;
    const char *uri_read;
    const char *id;
    unsigned int items;
} ServersFindingsCtx;
// the path of a pair of the findings, and its URI and key, each into what is sized for CLI_PATH_MAX_LEN: 1, or 0 where
// the pair names no path
EMBED_INLINE int findings_file_get(const ServersFindingsCtx *args)
{
    char *uri = args->uri;
    char *key = args->key;
    unsigned long long size = 0ull;
    const char *const named =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_found.json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = &s_found.json, .array = args->pair, .index = 0u),
                   .length_pointer = &size);
    char path[CLI_PATH_MAX_LEN];
    if ((named == NULL) || (size >= sizeof(path)))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path, .src = named, .bytes = (size_t)size});
    path[size] = '\0';
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = path, .uri_out = uri);
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
    return 1;
}

// the findings `items` of the checker `id` stored for the file of `key` and `uri`: 1 where they changed; under the
// checks' lock. The file's entry is read into a tree with capacity over for the findings copied in, their key and an
// object made where none was stored, and written into a cell of the most the tree writes to
EMBED_INLINE int findings_append(const ServersFindingsCtx *args)
{
    const char *key = args->key_read;
    const char *adjacent = NULL;
    unsigned long long size = 0ull;
    const char *const stored =
        EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_checked, .key = key,
                   .adjacent = &adjacent, .size = &size);
    unsigned int nodes_more = 1u;
    unsigned long long text_more = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}) + 1ull;
    EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = &s_found.json,
               .node = args->items, .nodes_pointer = &nodes_more, .text_u64s = &text_more);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    unsigned int entry = CLI_JSON_NONE;
    if (stored != NULL)
    {
        entry = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                           .text = stored, .size = size, .nodes_more = nodes_more, .text_more = text_more,
                           .err_msg = &err_msg, .owner = &g_cli_servers_cells);
    }
    else if (!EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &json,
                         .nodes_max = nodes_more, .text_max = text_more, .what = "a file's findings",
                         .owner = &g_cli_servers_cells))
    {
        return 0;
    }
    entry = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = entry,
                       .kind = CFG_JSON_OBJECT)
                ? entry
                : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_OBJECT);
    const unsigned int had = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                        .object = entry, .name = args->id);
    if ((had != CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = &json,
                                             .one = had, .other_json = &s_found.json, .other = args->items))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return 0;
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = entry,
               .name = args->id,
               .value = EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = &json,
                                   .from = &s_found.json, .node = args->items));
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &json, .object = entry);
    const unsigned long long capacity =
        EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = &json);
    char *const entry_written = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_servers_cells);
    VerbaLinea written = mmgr_line_over(entry_written, capacity);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = entry,
               .out = &written, .indent = 0u, .sorted = 1);
    if (!json.full && mmgr_line_ok(&written))
    {
        (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_checked,
                         .key = key, .adjacent_value = args->uri_read, .text = written.out, .size_value = written.at);
    }
    g_cli_cells->persistent_buf_release(entry_written, &g_cli_servers_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

void cli_servers_findings_store(const CliServersSizeCtx *args)
{
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_found);
    const unsigned int root = EMBED_CALL(cli_servers_read_state_internal.read, CliServersReadStateCtx,
                                         .read_state = &s_found, .text = args->found, .size = args->size);
    const CliJson *const json = &s_found.json;
    // the files whose findings changed, in a cell of one place for each file the findings name
    const unsigned int files = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = root, .kind = CFG_JSON_ARRAY)
                                   ? json->node[root].count
                                   : 0u;
    unsigned int *const changed_files =
        (files > 0u)
            ? g_cli_cells->temporary_buf_alloc((unsigned long long)files * sizeof(unsigned int), &g_cli_servers_cells)
            : NULL;
    unsigned int changed = 0u;
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    for (unsigned int pair = (changed_files != NULL) ? json->node[root].first : CLI_JSON_NONE; pair != CLI_JSON_NONE;
         pair = json->node[pair].next)
    {
        const unsigned int items = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = pair, .index = 1u);
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = items,
                        .kind = CFG_JSON_ARRAY) ||
            !EMBED_CALL(findings_file_get, ServersFindingsCtx, .pair = pair, .uri = uri, .key = key))
        {
            continue;
        }
        const int put = EMBED_CALL(findings_append, ServersFindingsCtx, .key_read = key, .uri_read = uri,
                                   .id = args->id, .items = items);
        if (put || args->always)
        {
            changed_files[changed] = pair;
            changed += 1u;
        }
    }
    for (unsigned int at = 0u; at < changed; at += 1u)
    {
        if (EMBED_CALL(findings_file_get, ServersFindingsCtx, .pair = changed_files[at], .uri = uri, .key = key))
        {
            EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri,
                       .sink = args->sink);
        }
    }
    g_cli_cells->persistent_buf_release(changed_files, &g_cli_servers_cells);
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_release, CliServersReadStateAnswerCtx,
               .read_state = &s_found);
}
