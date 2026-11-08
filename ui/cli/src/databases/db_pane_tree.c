// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane_tree.c: the schema of the chosen connection as the pane's rows. A level is read from the connection's schema
// link as its node first opens, its nodes put after those stored in a cell taken anew at the size of both and its
// texts the same, so that a node's children stand together and a node is a place among them; a failed level stands as
// one failed node. The rows shown are the places of the nodes under open nodes, in order, laid again as a node opens
// or closes. Here too the press on a row, the menus and the pane's commands
#include "db_pane_internal.h"

#include "db_internal.h"
#include "sql.h"

// the most bytes a level's answer may take; and the rows a first look at a table reads
#define TREE_LEVEL (1ULL << 20u)
#define TREE_FIRST_ROWS 200u

typedef struct
{
    const CliJson *json;
    unsigned int node;
    unsigned int at;
    const char *name;
    const char *table;
    unsigned long long bytes;
} DbPaneTreeCtx;

void cli_db_pane_schema_released(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    g_cli_guard->persistent_buf_release(pane->node, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(pane->text, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(pane->shown, g_cli_db_pane_owner);
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
    if ((pane->chosen == CLI_DB_PANE_NONE) || (pane->kept == NULL))
    {
        EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
        return;
    }
    CliDbPaneTree *const kept = &pane->kept[pane->chosen];
    g_cli_guard->persistent_buf_release(kept->node, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(kept->text, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(kept->shown, g_cli_db_pane_owner);
    kept->node = pane->node;
    kept->nodes = pane->nodes;
    kept->text = pane->text;
    kept->text_used = pane->text_used;
    kept->shown = pane->shown;
    kept->shown_count = pane->shown_count;
    kept->top_count = pane->top_count;
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
    if ((pane->kept == NULL) || (args->at >= pane->connections) || (pane->kept[args->at].node == NULL))
    {
        return 0;
    }
    CliDbPaneTree *const kept = &pane->kept[args->at];
    EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    pane->node = kept->node;
    pane->nodes = kept->nodes;
    pane->text = kept->text;
    pane->text_used = kept->text_used;
    pane->shown = kept->shown;
    pane->shown_count = kept->shown_count;
    pane->top_count = kept->top_count;
    memor.set(&(MemoriaCfg){.dst = kept, .val = 0, .bytes = sizeof(*kept)});
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

// a text put after the pane's texts in the cell `text` laid for them: its place
EMBED_INLINE unsigned long long text_put(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const unsigned long long at = pane->text_used;
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = pane->text + at, .src = args->text, .bytes = length});
    }
    pane->text[at + length] = '\0';
    pane->text_used += length + 1ULL;
    return at;
}

// the string member `name` of the object `node` put after the pane's texts: its place, with its bytes into `length`
EMBED_INLINE unsigned long long member_put(const DbPaneTreeCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = args->name),
                   .length_pointer = &length);
    const unsigned long long at = pane->text_used;
    const unsigned long long bytes = (text != NULL) ? length : 0ULL;
    if (bytes != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = pane->text + at, .src = text, .bytes = bytes});
    }
    pane->text[at + bytes] = '\0';
    pane->text_used += bytes + 1ULL;
    return at;
}

// the nodes and the texts grown by `at` nodes and `bytes` bytes in cells taken anew: 1, or 0 where the cells cannot
// meet them, both as they were
EMBED_INLINE int store_grown(const DbPaneTreeCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    const unsigned long long node_bytes = ((unsigned long long)pane->nodes + args->at) * sizeof(CliDbPaneNode);
    CliDbPaneNode *const node = g_cli_guard->persistent_buf_alloc(node_bytes, g_cli_db_pane_owner);
    char *const text =
        (node != NULL) ? g_cli_guard->persistent_buf_alloc(pane->text_used + args->bytes + 1ULL, g_cli_db_pane_owner)
                       : NULL;
    if (text == NULL)
    {
        g_cli_guard->persistent_buf_release(node, g_cli_db_pane_owner);
        return 0;
    }
    if (pane->nodes != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = node, .src = pane->node, .bytes = pane->nodes * sizeof(CliDbPaneNode)});
    }
    if (pane->text_used != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = pane->text, .bytes = pane->text_used});
    }
    g_cli_guard->persistent_buf_release(pane->node, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(pane->text, g_cli_db_pane_owner);
    pane->node = node;
    pane->text = text;
    return 1;
}

int cli_db_pane_level_read(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if ((pane->chosen == CLI_DB_PANE_NONE) || (pane->open[pane->chosen] == NULL))
    {
        return 0;
    }
    const unsigned int parent = args->at;
    // the path of the node under which the level stands, its names among the texts
    const char *path[CLI_DB_PANE_PATH];
    unsigned int path_count = 0u;
    if (parent != CLI_DB_PANE_NONE)
    {
        const CliDbPaneNode *const above = &pane->node[parent];
        for (unsigned int at = 0u; at < above->path_count; at += 1u)
        {
            path[at] = pane->text + above->path[at];
        }
        path_count = above->path_count;
    }
    char *const answer = g_cli_guard->temporary_buf_alloc(TREE_LEVEL, g_cli_db_pane_owner);
    if (answer == NULL)
    {
        return 0;
    }
    VerbaLinea out = {answer, TREE_LEVEL, 0ULL};
    CliDbFailure failure;
    const int read = EMBED_CALL(cli_db_out_count.children, CliDbOutCountCfg, .open = pane->open[pane->chosen],
                                .path_pointer = path, .count = path_count, .out = &out, .failure = &failure) &&
                     mmgr_line_ok(&out);
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        read ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                          .json = &json, .text = answer, .size = out.at, .nodes_more = 0u, .text_more = 0ULL,
                          .err_msg = &err_msg, .owner = g_cli_db_pane_owner)
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
        const unsigned int names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = item, .name = "path");
        for (unsigned int name = 0u; (names != CLI_JSON_NONE) && (name < json.node[names].count); name += 1u)
        {
            unsigned long long length = 0ULL;
            (void)EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                             .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = &json, .array = names, .index = name),
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
    const unsigned int depth = (parent != CLI_DB_PANE_NONE) ? pane->node[parent].depth + 1u : 0u;
    for (unsigned int at = 0u; stored && (at < count); at += 1u)
    {
        CliDbPaneNode *const node = &pane->node[first + at];
        *node = (CliDbPaneNode){0};
        node->parent = parent;
        node->depth = depth;
        node->first = CLI_DB_PANE_NONE;
        if (root == CLI_JSON_NONE)
        {
            node->label = EMBED_CALL(text_put, CliDbPaneCtx, .text = first_line);
            node->label_length = failed_line.at;
            node->type = EMBED_CALL(text_put, CliDbPaneCtx, .text = "");
            node->detail = node->type;
            node->leaf = 1u;
            node->failed = 1u;
            continue;
        }
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &json, .array = root, .index = at);
        node->label_length = EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "label");
        node->label = EMBED_CALL(member_put, DbPaneTreeCtx, .json = &json, .node = item, .name = "label");
        node->type_length = EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "kind");
        node->type = EMBED_CALL(member_put, DbPaneTreeCtx, .json = &json, .node = item, .name = "kind");
        node->detail_length = EMBED_CALL(member_length, DbPaneTreeCtx, .json = &json, .node = item, .name = "detail");
        node->detail = EMBED_CALL(member_put, DbPaneTreeCtx, .json = &json, .node = item, .name = "detail");
        node->leaf = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = &json, .object = item, .name = "leaf"),
                                .type = CFG_JSON_TRUE)
                         ? 1u
                         : 0u;
        const unsigned int names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = item, .name = "path");
        for (unsigned int name = 0u;
             (names != CLI_JSON_NONE) && (name < json.node[names].count) && (name < CLI_DB_PANE_PATH); name += 1u)
        {
            const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = &json, .array = names, .index = name);
            unsigned long long length = 0ULL;
            const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = &json, .node = one, .length_pointer = &length);
            const unsigned long long place = pane->text_used;
            if ((text != NULL) && (length != 0ULL))
            {
                memor.cpy(&(MemoriaCfg){.dst = pane->text + place, .src = text, .bytes = length});
            }
            pane->text[place + ((text != NULL) ? length : 0ULL)] = '\0';
            pane->text_used += ((text != NULL) ? length : 0ULL) + 1ULL;
            node->path[name] = place;
            node->path_count = name + 1u;
        }
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
        pane->node[parent].first = first;
        pane->node[parent].children = count;
        pane->node[parent].read = 1u;
    }
    return 1;
}

void cli_db_pane_shown_laid(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    // two walks of the open nodes in order: the first counts the rows, the second lays them in a cell sized to them;
    // a node's rows follow it, its next sibling after them, each walk's stack the path to the node it stands on
    unsigned int *shown = NULL;
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
            const CliDbPaneNode *const node = &pane->node[at];
            if (shown != NULL)
            {
                shown[put] = at;
            }
            put += 1u;
            if (node->open && node->read && (node->children != 0u) && (depth < CLI_DB_PANE_PATH + 2u))
            {
                stack[depth] = at + 1u;
                ends[depth] = end;
                depth += 1u;
                at = node->first;
                end = node->first + node->children;
                continue;
            }
            at += 1u;
        }
        count = put;
        if ((walk == 0u) && (count != 0u))
        {
            shown = g_cli_guard->persistent_buf_alloc((unsigned long long)count * sizeof(unsigned int),
                                                      g_cli_db_pane_owner);
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
    g_cli_guard->persistent_buf_release(pane->shown, g_cli_db_pane_owner);
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
    (void)cellul.copy(
        &(CatenaFinitaCfg){.dst = pane->chosen_name, .src = pane->connection[args->at].name, .cap = CLI_NAME_MAX_LEN});
    EMBED_CALL(cli_db_pane_internal.note_set, CliDbPaneCtx, .text = NULL);
    const int restored = EMBED_CALL(cli_db_pane_internal.tree_restored, CliDbPaneCtx, .at = args->at);
    if (!restored)
    {
        EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    }
    if (!restored && (pane->open[args->at] != NULL))
    {
        (void)EMBED_CALL(cli_db_pane_internal.level_read, CliDbPaneCtx, .at = CLI_DB_PANE_NONE);
    }
    if (pane->open[args->at] != NULL)
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
    if ((pane->note != NULL) || (args->at >= pane->shown_count))
    {
        return 0;
    }
    const unsigned int place = pane->shown[args->at];
    CliDbPaneNode *const node = &pane->node[place];
    if (node->leaf || node->failed)
    {
        return 0;
    }
    node->open = node->open ? 0u : 1u;
    if (node->open && !node->read)
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
    const CliDbPaneNode *const node = &pane->node[args->at];
    const char *const type = pane->text + node->type;
    static const char *const s_types[4] = {"table", "view", "materialized view", "foreign table"};
    int readable = 0;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        readable = readable || cellul.eq(&(CatenaFinitaCfg){.src = type, .other = s_types[at], .cap = 32ULL});
    }
    return readable && (node->path_count == CLI_DB_PANE_PATH);
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
        for (unsigned int at = 0u; (at < pane->connections) && (at < CLI_MENU_OWN_ROWS); at += 1u)
        {
            cli_menus_own_row(&(CliItem){pane->labels + pane->label_at[at], "db-choose", "", pane->connection[at].name,
                                         "", "", 0, 0, 0, 0, 0u});
        }
        return 1;
    }
    if (pane->asking || pane->form.open || (pane->note != NULL) || (args->at >= pane->shown_count))
    {
        return 0;
    }
    const unsigned int place = pane->shown[args->at];
    const CliDbPaneNode *const node = &pane->node[place];
    if (node->failed)
    {
        return 0;
    }
    pane->menu_node = place;
    cli_menus_own_begin();
    cli_menus_own_row(&(CliItem){"Copy Name", "db-copy-name", "", pane->text + node->label, "", "", 0, 0, 0, 0, 0u});
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
    const unsigned int dialect = EMBED_CALL(cli_sql.dialect, CliSqlCfg, .name = pane->connection[pane->chosen].type);
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
        for (unsigned int at = 0u; (word != NULL) && (at < pane->connections); at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = pane->connection[at].name, .other = word, .cap = CLI_NAME_MAX_LEN}))
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
            const CliDbPaneNode *const node = &pane->node[pane->menu_node];
            (void)EMBED_CALL(first_rows_run, DbPaneTreeCtx, .name = pane->text + node->path[0],
                             .table = pane->text + node->path[CLI_DB_PANE_PATH - 1u]);
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
