// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane_tree.c: the schema of the chosen connection as the pane's rows. A level is read from the connection's schema
// link as its node first opens, its nodes put after those stored in a movable cell taken anew at the size of both and
// its texts the same, so that a node's children stand together and a node is a place among them; a failed level stands
// as one failed node. The rows shown are the places of the nodes under open nodes, in order, laid again in a movable
// cell as a node opens or closes. Every record and text is read out of its cell by copy where it is used. Here too the
// press on a row, the menus and the pane's commands
#include "db_pane_internal.h"

#include "db_internal.h"
#include "sql.h"

// the most bytes a level's answer may take; and the rows a first look at a table reads
#define TREE_LEVEL (1ULL << 20u)
#define TREE_FIRST_ROWS 200u

// the place of the node `at` in the nodes' cell, and of the row `at` in the shown cell
#define TREE_NODE_AT(at_) ((unsigned long long)(at_) * sizeof(CliDbPaneNode))
#define TREE_SHOWN_AT(at_) ((unsigned long long)(at_) * sizeof(unsigned int))

typedef struct
{
    const CliJson *json;
    unsigned int node;
    unsigned int at;
    const char *name;
    const char *table;
    unsigned long long bytes;
    CliDbPaneNode *node_out;
    char *text_out;
    unsigned long long capacity;
    CliDbConnection *connection_out;
    CliDbPaneTree *kept_out;
    const CliDbPaneTree *kept_read;
} DbPaneTreeCtx;

// the connection `at` read out of the connections cell into `connection_out`
EMBED_INLINE void connection_read(const DbPaneTreeCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                            .offset = CLI_DB_PANE_CONNECTION_AT(args->at),
                                                            .dst = args->connection_out,
                                                            .bytes = sizeof(CliDbConnection)});
}

// the open connection `at`, NULL where it is not open, read out of the connections cell
EMBED_INLINE CliDbOpen *open_read(const DbPaneTreeCtx *args)
{
    CliDbOpen *open = NULL;
    if (g_cli_db_pane_state.connections_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                                .offset = CLI_DB_PANE_OPEN_AT(args->at),
                                                                .dst = &open,
                                                                .bytes = sizeof(open)});
    }
    return open;
}

// the kept tree of the connection `at` read out of the connections cell into `kept_out`, and `kept_read` written as
// the kept tree of the connection `at`
EMBED_INLINE void kept_tree_read(const DbPaneTreeCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                            .offset = CLI_DB_PANE_KEPT_AT(args->at),
                                                            .dst = args->kept_out,
                                                            .bytes = sizeof(CliDbPaneTree)});
}

EMBED_INLINE void kept_tree_written(const DbPaneTreeCtx *args)
{
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                             .offset = CLI_DB_PANE_KEPT_AT(args->at),
                                                             .src = args->kept_read,
                                                             .bytes = sizeof(CliDbPaneTree)});
}

// the node `at` read out of the pane's nodes into `node_out`
EMBED_INLINE void node_read(const DbPaneTreeCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.node,
                                                            .offset = TREE_NODE_AT(args->at),
                                                            .dst = args->node_out,
                                                            .bytes = sizeof(CliDbPaneNode)});
}

// the node `at` of the pane's nodes written from `node_out`
EMBED_INLINE void node_written(const DbPaneTreeCtx *args)
{
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.node,
                                                             .offset = TREE_NODE_AT(args->at),
                                                             .src = args->node_out,
                                                             .bytes = sizeof(CliDbPaneNode)});
}

// the place of the row `at` among the nodes, read out of the shown cell
EMBED_INLINE unsigned int shown_read(const DbPaneTreeCtx *args)
{
    unsigned int place = CLI_DB_PANE_NONE;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = g_cli_db_pane_state.shown, .offset = TREE_SHOWN_AT(args->at), .dst = &place, .bytes = sizeof(place)});
    return place;
}

// the text at `bytes` of the pane's texts copied into `text_out` of `capacity` bytes with its end, cut to the capacity
EMBED_INLINE void text_read(const DbPaneTreeCtx *args)
{
    const unsigned long long length =
        g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.text, .offset = args->bytes});
    const unsigned long long copied = (length < args->capacity) ? length : (args->capacity - 1ULL);
    if (copied != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = g_cli_db_pane_state.text, .offset = args->bytes, .dst = args->text_out, .bytes = copied});
    }
    args->text_out[copied] = '\0';
}

void cli_db_pane_schema_released(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->node != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->node, g_cli_db_pane_owner);
    }
    if (pane->text != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->text, g_cli_db_pane_owner);
    }
    if (pane->shown != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->shown, g_cli_db_pane_owner);
    }
    pane->node = NULL;
    pane->nodes = 0u;
    pane->text = NULL;
    pane->text_used = 0ULL;
    pane->shown = NULL;
    pane->shown_count = 0u;
    pane->top_count = 0u;
    pane->menu_node = CLI_DB_PANE_NONE;
}

void cli_db_pane_tree_kept(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if ((pane->chosen == CLI_DB_PANE_NONE) || (pane->connections_cell == NULL))
    {
        EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
        return;
    }
    // the chosen connection's kept tree read out of the connections cell, exchanged and written back
    CliDbPaneTree kept_tree;
    CliDbPaneTree *const kept = &kept_tree;
    EMBED_CALL(kept_tree_read, DbPaneTreeCtx, .at = pane->chosen, .kept_out = kept);
    if (kept->node != NULL)
    {
        (void)g_cli_guard->movable_buf_release(kept->node, g_cli_db_pane_owner);
    }
    if (kept->text != NULL)
    {
        (void)g_cli_guard->movable_buf_release(kept->text, g_cli_db_pane_owner);
    }
    if (kept->shown != NULL)
    {
        (void)g_cli_guard->movable_buf_release(kept->shown, g_cli_db_pane_owner);
    }
    kept->node = pane->node;
    kept->nodes = pane->nodes;
    kept->text = pane->text;
    kept->text_used = pane->text_used;
    kept->shown = pane->shown;
    kept->shown_count = pane->shown_count;
    kept->top_count = pane->top_count;
    EMBED_CALL(kept_tree_written, DbPaneTreeCtx, .at = pane->chosen, .kept_read = kept);
    pane->node = NULL;
    pane->nodes = 0u;
    pane->text = NULL;
    pane->text_used = 0ULL;
    pane->shown = NULL;
    pane->shown_count = 0u;
    pane->top_count = 0u;
    pane->menu_node = CLI_DB_PANE_NONE;
}

int cli_db_pane_tree_restored(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if ((pane->connections_cell == NULL) || (args->at >= pane->connections))
    {
        return 0;
    }
    // the connection's kept tree read out of the connections cell, taken as the pane's and written back empty
    CliDbPaneTree kept_tree;
    CliDbPaneTree *const kept = &kept_tree;
    EMBED_CALL(kept_tree_read, DbPaneTreeCtx, .at = args->at, .kept_out = kept);
    if (kept->node == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    pane->node = kept->node;
    pane->nodes = kept->nodes;
    pane->text = kept->text;
    pane->text_used = kept->text_used;
    pane->shown = kept->shown;
    pane->shown_count = kept->shown_count;
    pane->top_count = kept->top_count;
    memor.set(&(MemoriaCfg){.dst = kept, .val = 0, .bytes = sizeof(*kept)});
    EMBED_CALL(kept_tree_written, DbPaneTreeCtx, .at = args->at, .kept_read = kept);
    return 1;
}

// the bytes of a string member `name` of the object `node`, 0 where it has none
EMBED_INLINE unsigned long long member_length(const DbPaneTreeCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = args->name),
                   .length_pointer = &length);
    return (text != NULL) ? length : 0ULL;
}

// `bytes` bytes of `name` put after the pane's texts in the cell laid for them with an end: its place
EMBED_INLINE unsigned long long bytes_put(const DbPaneTreeCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    const unsigned long long at = pane->text_used;
    const char end = '\0';
    if (args->bytes != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = pane->text, .offset = at, .src = args->name, .bytes = args->bytes});
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = pane->text, .offset = at + args->bytes, .src = &end, .bytes = 1ULL});
    pane->text_used += args->bytes + 1ULL;
    return at;
}

// a text put after the pane's texts in the cell laid for them: its place
EMBED_INLINE unsigned long long text_put(const CliDbPaneCtx *args)
{
    return EMBED_CALL(bytes_put, DbPaneTreeCtx, .name = args->text,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

// the string member `name` of the object `node` put after the pane's texts: its place
EMBED_INLINE unsigned long long member_put(const DbPaneTreeCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = args->name),
                   .length_pointer = &length);
    return EMBED_CALL(bytes_put, DbPaneTreeCtx, .name = (text != NULL) ? text : "",
                      .bytes = (text != NULL) ? length : 0ULL);
}

// a movable cell of `bytes` bytes taken with the first `at` bytes of `cell` copied in through a cell of this call: the
// new cell, or NULL where the cells cannot meet it, the old left as it is
EMBED_INLINE const CarcerMovableEntry *cell_grown(const DbPaneTreeCtx *args)
{
    const CarcerMovableEntry *const grown = g_cli_guard->movable_buf_alloc(args->bytes, g_cli_db_pane_owner);
    if ((grown == NULL) || (args->at == 0u))
    {
        return grown;
    }
    unsigned char *const copied = g_cli_guard->temporary_buf_alloc(args->at, g_cli_db_pane_owner);
    if (copied == NULL)
    {
        (void)g_cli_guard->movable_buf_release(grown, g_cli_db_pane_owner);
        return NULL;
    }
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = (const CarcerMovableEntry *)args->name, .offset = 0ULL, .dst = copied, .bytes = args->at});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = grown, .offset = 0ULL, .src = copied, .bytes = args->at});
    g_cli_guard->persistent_buf_release(copied, g_cli_db_pane_owner);
    return grown;
}

// the nodes and the texts grown by `at` nodes and `bytes` bytes in movable cells taken anew: 1, or 0 where the cells
// cannot meet them, both as they were
EMBED_INLINE int store_grown(const DbPaneTreeCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    const unsigned long long node_bytes = TREE_NODE_AT(pane->nodes + args->at);
    // the cells copied from are handed as the name, the bytes copied as the place
    const CarcerMovableEntry *const node =
        EMBED_CALL(cell_grown, DbPaneTreeCtx, .bytes = node_bytes, .name = (const char *)pane->node,
                   .at = (unsigned int)TREE_NODE_AT(pane->nodes));
    const CarcerMovableEntry *const text =
        (node != NULL) ? EMBED_CALL(cell_grown, DbPaneTreeCtx, .bytes = pane->text_used + args->bytes + 1ULL,
                                    .name = (const char *)pane->text, .at = (unsigned int)pane->text_used)
                       : NULL;
    if (text == NULL)
    {
        if (node != NULL)
        {
            (void)g_cli_guard->movable_buf_release(node, g_cli_db_pane_owner);
        }
        return 0;
    }
    if (pane->node != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->node, g_cli_db_pane_owner);
    }
    if (pane->text != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->text, g_cli_db_pane_owner);
    }
    pane->node = node;
    pane->text = text;
    return 1;
}

int cli_db_pane_level_read(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbOpen *const open =
        (pane->chosen != CLI_DB_PANE_NONE) ? EMBED_CALL(open_read, DbPaneTreeCtx, .at = pane->chosen) : NULL;
    if (open == NULL)
    {
        return 0;
    }
    const unsigned int parent = args->at;
    // the path of the node under which the level stands, its names read out of the texts
    char names[CLI_DB_PANE_PATH][CLI_DB_FIELD];
    const char *path[CLI_DB_PANE_PATH];
    unsigned int path_count = 0u;
    CliDbPaneNode above = {0};
    if (parent != CLI_DB_PANE_NONE)
    {
        EMBED_CALL(node_read, DbPaneTreeCtx, .at = parent, .node_out = &above);
        for (unsigned int at = 0u; at < above.path_count; at += 1u)
        {
            EMBED_CALL(text_read, DbPaneTreeCtx, .bytes = above.path[at], .text_out = names[at],
                       .capacity = sizeof(names[at]));
            path[at] = names[at];
        }
        path_count = above.path_count;
    }
    char *const answer = g_cli_guard->temporary_buf_alloc(TREE_LEVEL, g_cli_db_pane_owner);
    if (answer == NULL)
    {
        return 0;
    }
    VerbaLinea out = {answer, TREE_LEVEL, 0ULL};
    CliDbFailure failure;
    const int read = EMBED_CALL(cli_db_out_count.children, CliDbOutCountCfg, .open = open, .path_pointer = path,
                                .count = path_count, .out = &out, .failure = &failure) &&
                     mmgr_line_ok(&out);
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root = read
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                               .json = &json, .text = answer, .size = out.at, .nodes_more = 0u,
                                               .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_db_pane_owner)
                                  : CLI_JSON_NONE;
    const unsigned int count = (root != CLI_JSON_NONE) ? json.node[root].count : 1u;
    // the bytes the level's texts take: each node's label, type, detail and path names with their ends, or the
    // failure's first line
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; (root != CLI_JSON_NONE) && (at < count); at += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &json, .array = root, .index = at);
        bytes += EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "label") +
                 EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "kind") +
                 EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "detail") + 3ULL;
        const unsigned int path_names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = &json, .object = item, .name = "path");
        for (unsigned int name = 0u; (path_names != CLI_JSON_NONE) && (name < json.node[path_names].count); name += 1u)
        {
            unsigned long long length = 0ULL;
            (void)EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                             .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = &json, .array = path_names, .index = name),
                             .length_pointer = &length);
            bytes += length + 1ULL;
        }
    }
    char first_line[CLI_DB_FIELD];
    VerbaLinea failed_line = {first_line, sizeof(first_line), 0ULL};
    if (root == CLI_JSON_NONE)
    {
        const char *const message = read ? why : failure.message;
        unsigned long long length = 0ULL;
        while ((message[length] != '\0') && (message[length] != '\n') && (length < sizeof(first_line) - 1ULL))
        {
            length += 1ULL;
        }
        (void)mmgr_line_put_n(&failed_line, message, length);
        (void)mmgr_line_finish(&failed_line);
        bytes = failed_line.at + 2ULL;
    }
    const int stored = EMBED_CALL(store_grown, DbPaneTreeCtx, .at = count, .bytes = bytes);
    const unsigned int first = pane->nodes;
    const unsigned int depth = (parent != CLI_DB_PANE_NONE) ? above.depth + 1u : 0u;
    for (unsigned int at = 0u; stored && (at < count); at += 1u)
    {
        CliDbPaneNode node = {0};
        node.parent = parent;
        node.depth = depth;
        node.first = CLI_DB_PANE_NONE;
        if (root == CLI_JSON_NONE)
        {
            node.label = EMBED_CALL(text_put, CliDbPaneCtx, .text = first_line);
            node.label_length = failed_line.at;
            node.type = EMBED_CALL(text_put, CliDbPaneCtx, .text = "");
            node.detail = node.type;
            node.leaf = 1u;
            node.failed = 1u;
            EMBED_CALL(node_written, DbPaneTreeCtx, .at = first + at, .node_out = &node);
            continue;
        }
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &json, .array = root, .index = at);
        node.label_length = EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "label");
        node.label = EMBED_CALL(member_put, DbPaneTreeCtx, .json = &json, .node = item, .name = "label");
        node.type_length = EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "kind");
        node.type = EMBED_CALL(member_put, DbPaneTreeCtx, .json = &json, .node = item, .name = "kind");
        node.detail_length = EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "detail");
        node.detail = EMBED_CALL(member_put, DbPaneTreeCtx, .json = &json, .node = item, .name = "detail");
        node.leaf = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = item, .name = "leaf"),
                               .type = CFG_JSON_TRUE)
                        ? 1u
                        : 0u;
        const unsigned int path_names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = &json, .object = item, .name = "path");
        for (unsigned int name = 0u;
             (path_names != CLI_JSON_NONE) && (name < json.node[path_names].count) && (name < CLI_DB_PANE_PATH);
             name += 1u)
        {
            const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = &json, .array = path_names, .index = name);
            unsigned long long length = 0ULL;
            const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = &json, .node = one, .length_pointer = &length);
            node.path[name] = EMBED_CALL(bytes_put, DbPaneTreeCtx, .name = (text != NULL) ? text : "",
                                         .bytes = (text != NULL) ? length : 0ULL);
            node.path_count = name + 1u;
        }
        EMBED_CALL(node_written, DbPaneTreeCtx, .at = first + at, .node_out = &node);
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(answer, g_cli_db_pane_owner);
    if (!stored)
    {
        return 0;
    }
    pane->nodes = first + count;
    if (parent == CLI_DB_PANE_NONE)
    {
        pane->top_count = count;
    }
    else
    {
        above.first = first;
        above.children = count;
        above.read = 1u;
        EMBED_CALL(node_written, DbPaneTreeCtx, .at = parent, .node_out = &above);
    }
    return 1;
}

void cli_db_pane_shown_laid(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    // two walks of the open nodes in order: the first counts the rows, the second lays them in a movable cell sized
    // to them; a node's rows follow it, its next sibling after them, each walk's stack the path to the node it stands
    // on
    const CarcerMovableEntry *shown = NULL;
    unsigned int count = 0u;
    for (unsigned int walk = 0u; walk < 2u; walk += 1u)
    {
        unsigned int stack[CLI_DB_PANE_PATH + 2u];
        unsigned int ends[CLI_DB_PANE_PATH + 2u];
        unsigned int depth = 0u;
        unsigned int at = 0u;
        unsigned int end = pane->top_count;
        unsigned int put = 0u;
        while ((pane->node != NULL) && ((at < end) || (depth != 0u)))
        {
            if (at >= end)
            {
                depth -= 1u;
                at = stack[depth];
                end = ends[depth];
                continue;
            }
            CliDbPaneNode node;
            EMBED_CALL(node_read, DbPaneTreeCtx, .at = at, .node_out = &node);
            if (shown != NULL)
            {
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = shown, .offset = TREE_SHOWN_AT(put), .src = &at, .bytes = sizeof(at)});
            }
            put += 1u;
            if (node.open && node.read && (node.children != 0u) && (depth < CLI_DB_PANE_PATH + 2u))
            {
                stack[depth] = at + 1u;
                ends[depth] = end;
                depth += 1u;
                at = node.first;
                end = node.first + node.children;
                continue;
            }
            at += 1u;
        }
        count = put;
        if ((walk == 0u) && (count != 0u))
        {
            shown = g_cli_guard->movable_buf_alloc(TREE_SHOWN_AT(count), g_cli_db_pane_owner);
            if (shown == NULL)
            {
                count = 0u;
                break;
            }
        }
        if (shown == NULL)
        {
            break;
        }
    }
    if (pane->shown != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->shown, g_cli_db_pane_owner);
    }
    pane->shown = shown;
    pane->shown_count = (shown != NULL) ? count : 0u;
}

// the next connection, or the first, chosen: the tree of the one chosen until now kept, and the one chosen shown with
// its tree as it was left, or its schema read afresh where none is kept and it is open
EMBED_INLINE void chosen_set(const DbPaneTreeCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (args->at >= pane->connections)
    {
        return;
    }
    if (pane->chosen != args->at)
    {
        EMBED_CALL(cli_db_pane_internal.tree_kept, CliDbPaneCtx, .at = 0u);
    }
    pane->chosen = args->at;
    CliDbConnection connection;
    EMBED_CALL(connection_read, DbPaneTreeCtx, .at = args->at, .connection_out = &connection);
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = pane->chosen_name, .src = connection.name, .cap = CLI_NAME_MAX_LEN});
    EMBED_CALL(cli_db_pane_internal.note_set, CliDbPaneCtx, .text = NULL);
    const int restored = EMBED_CALL(cli_db_pane_internal.tree_restored, CliDbPaneCtx, .at = args->at);
    if (!restored)
    {
        EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    }
    const int open = EMBED_CALL(open_read, DbPaneTreeCtx, .at = args->at) != NULL;
    if (!restored && open)
    {
        (void)EMBED_CALL(cli_db_pane_internal.level_read, CliDbPaneCtx, .at = CLI_DB_PANE_NONE);
    }
    if (open)
    {
        EMBED_CALL(cli_db_pane_internal.shown_laid, CliDbPaneCtx, .at = 0u);
    }
}

int cli_db_pane_press(const CliDbPaneTreeCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->asking && EMBED_CALL(cli_prompt.asked, CliPromptCfg, .context = pane))
    {
        CliPromptRow row;
        if (!EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = args->at, .row = &row))
        {
            return 0;
        }
        if (row.type == CLI_PROMPT_ROW_BUTTONS)
        {
            (void)EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = args->choice);
            return 0;
        }
        return row.type == CLI_PROMPT_ROW_FIELD;
    }
    if (pane->form.open)
    {
        return EMBED_CALL(cli_db_pane_internal.form_press, CliDbPaneCtx, .at = args->at, .choice = args->choice,
                          .root = args->root);
    }
    if (pane->noted || (args->at >= pane->shown_count))
    {
        return 0;
    }
    const unsigned int place = EMBED_CALL(shown_read, DbPaneTreeCtx, .at = args->at);
    CliDbPaneNode node;
    EMBED_CALL(node_read, DbPaneTreeCtx, .at = place, .node_out = &node);
    if (node.leaf || node.failed)
    {
        return 0;
    }
    node.open = node.open ? 0u : 1u;
    EMBED_CALL(node_written, DbPaneTreeCtx, .at = place, .node_out = &node);
    if (node.open && !node.read)
    {
        (void)EMBED_CALL(cli_db_pane_internal.level_read, CliDbPaneCtx, .at = place);
    }
    EMBED_CALL(cli_db_pane_internal.shown_laid, CliDbPaneCtx, .at = 0u);
    return 0;
}

// whether the node `at` is a table, a view, a materialized view or a foreign table under a schema, whose first rows
// can be read
EMBED_INLINE int node_readable(const DbPaneTreeCtx *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneNode node;
    EMBED_CALL(node_read, DbPaneTreeCtx, .at = args->at, .node_out = &node);
    static const char *const s_types[4] = {"table", "view", "materialized view", "foreign table"};
    int readable = 0;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_types[at], .cap = 32ULL});
        readable = readable ||
                   ((node.type_length == length) &&
                    (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                         .cell = pane->text, .offset = node.type, .src = s_types[at], .bytes = length + 1ULL}) == 0));
    }
    return readable && (node.path_count == CLI_DB_PANE_PATH);
}

int cli_db_pane_menu(const CliDbPaneTreeCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (args->at == CLI_DB_PANE_NONE)
    {
        if (pane->connections == 0u)
        {
            return 0;
        }
        // the connections to choose among, each running db-choose with its name, which stands in the list while the
        // menu is open
        cli_menus_own_begin();
        CliDbConnection connection;
        for (unsigned int at = 0u; (at < pane->connections) && (at < CLI_MENU_OWN_ROWS); at += 1u)
        {
            // the menu copies the label and the name, so the pane's label and this copy serve each row in turn
            (void)EMBED_CALL(cli_db_pane_internal.label_read, CliDbPaneCtx, .at = at);
            EMBED_CALL(connection_read, DbPaneTreeCtx, .at = at, .connection_out = &connection);
            cli_menus_own_row(&(CliItem){pane->label, "db-choose", "", connection.name, "", "", 0, 0, 0, 0, 0u});
        }
        return 1;
    }
    if (pane->asking || pane->form.open || pane->noted || (args->at >= pane->shown_count))
    {
        return 0;
    }
    const unsigned int place = EMBED_CALL(shown_read, DbPaneTreeCtx, .at = args->at);
    CliDbPaneNode node;
    EMBED_CALL(node_read, DbPaneTreeCtx, .at = place, .node_out = &node);
    if (node.failed)
    {
        return 0;
    }
    pane->menu_node = place;
    // the menu copies the row, so the label read into this call's bytes serves it
    char label[CLI_DB_FIELD];
    EMBED_CALL(text_read, DbPaneTreeCtx, .bytes = node.label, .text_out = label, .capacity = sizeof(label));
    cli_menus_own_begin();
    cli_menus_own_row(&(CliItem){"Copy Name", "db-copy-name", "", label, "", "", 0, 0, 0, 0, 0u});
    if (EMBED_CALL(node_readable, DbPaneTreeCtx, .at = place))
    {
        cli_menus_own_row(&(CliItem){"Show Its First Rows", "db-first-rows", "", "", "", "", 0, 0, 0, 0, 0u});
    }
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Read Again", "db-read-again", "", "", "", "", 0, 0, 0, 0, 0u});
    return 1;
}

// the first rows of the table `table` under the schema `name` laid as a statement and run on the chosen connection
EMBED_INLINE int first_rows_run(const DbPaneTreeCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->chosen == CLI_DB_PANE_NONE)
    {
        return 0;
    }
    const char *const schema = args->name;
    const char *const table = args->table;
    CliDbConnection connection;
    EMBED_CALL(connection_read, DbPaneTreeCtx, .at = pane->chosen, .connection_out = &connection);
    const unsigned int dialect = EMBED_CALL(cli_sql.dialect, CliSqlCfg, .name = connection.type);
    char statement[3u * CLI_NAME_MAX_LEN];
    VerbaLinea line = {statement, sizeof(statement), 0ULL};
    (void)mmgr_line_put(&line, "SELECT * FROM ");
    EMBED_CALL(cli_db_out_name_internal.quoted_name, CliDbOutNameCtx, .out = &line, .name = schema, .dialect = dialect);
    mmgr_line_ch(&line, '.');
    EMBED_CALL(cli_db_out_name_internal.quoted_name, CliDbOutNameCtx, .out = &line, .name = table, .dialect = dialect);
    (void)mmgr_line_put(&line, " LIMIT ");
    mmgr_line_decimal(&line, TREE_FIRST_ROWS, 1u);
    const unsigned long long length = mmgr_line_finish(&line);
    return EMBED_CALL(cli_db_pane.run, CliDbPaneCfg, .statement = statement, .size = length, .answered = 0,
                      .root = pane->root);
}

int cli_db_pane_command(const CliDbPaneTreeCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    const char *const word = (args->count != 0u) ? args->words[0] : NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "db-choose", .cap = 32ULL}))
    {
        CliDbConnection connection;
        for (unsigned int at = 0u; (word != NULL) && (at < pane->connections); at += 1u)
        {
            EMBED_CALL(connection_read, DbPaneTreeCtx, .at = at, .connection_out = &connection);
            if (cellul.eq(&(CatenaFinitaCfg){.src = connection.name, .other = word, .cap = CLI_NAME_MAX_LEN}))
            {
                EMBED_CALL(chosen_set, DbPaneTreeCtx, .at = at);
            }
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "db-copy-name", .cap = 32ULL}))
    {
        if (word != NULL)
        {
            (void)EMBED_CALL(cli_size_text_internal.clipboard_text_write, CliSizeTextCtx, .text = word,
                             .size = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_POOL_BYTES}));
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "db-first-rows", .cap = 32ULL}))
    {
        // the schema and the table the words carry, else the node the menu was built for
        if (args->count >= 2u)
        {
            (void)EMBED_CALL(first_rows_run, DbPaneTreeCtx, .name = args->words[0], .table = args->words[1]);
        }
        else if ((pane->menu_node < pane->nodes) && EMBED_CALL(node_readable, DbPaneTreeCtx, .at = pane->menu_node))
        {
            CliDbPaneNode node;
            EMBED_CALL(node_read, DbPaneTreeCtx, .at = pane->menu_node, .node_out = &node);
            char schema[CLI_DB_FIELD];
            char table[CLI_DB_FIELD];
            EMBED_CALL(text_read, DbPaneTreeCtx, .bytes = node.path[0], .text_out = schema, .capacity = sizeof(schema));
            EMBED_CALL(text_read, DbPaneTreeCtx, .bytes = node.path[CLI_DB_PANE_PATH - 1u], .text_out = table,
                       .capacity = sizeof(table));
            (void)EMBED_CALL(first_rows_run, DbPaneTreeCtx, .name = schema, .table = table);
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "db-read-again", .cap = 32ULL}))
    {
        if (pane->chosen != CLI_DB_PANE_NONE)
        {
            EMBED_CALL(chosen_set, DbPaneTreeCtx, .at = pane->chosen);
        }
        return 1;
    }
    return 0;
}
