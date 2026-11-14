// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane.c: the Database pane's state, its rows and its runs. The connections the tree lists stand in one movable
// cell sized to them with the open one of each, each transaction's state and the tree kept of each, laid in a cell of
// the read and written once; a read again matches the open ones by name, and a connection the tree no longer lists is
// closed. The labels stand in one movable cell, laid again as a
// connection opens or closes, and read out of it a label at a time. A statement runs on a thread of its own into a
// cell sized to the most an answer takes;
// the thread sets its end and wakes the view's thread, whose step adopts the answer into the grid, or opens the ask
// where the run confirms first, and gives the run's cells back
#include "db_pane_internal.h"

// the most bytes of the connections' list; the sentences of the pane with no connection and of one not connected; the
// open mark; and the buttons of the ask before a guarded statement
#define PANE_LIST (1ULL << 18u)
#define PANE_NONE_CHOSEN "No connection"
#define PANE_NOT_CONNECTED " is not connected: Connect reads its schema."
#define PANE_OPEN_MARK " \xE2\x97\x8F"

static const char *const s_run_choices[2] = {"Run It", "Do Not Run It"};

CliDbPaneState g_cli_db_pane_state;

typedef struct
{
    const char *name;
    const CliJson *json;
    unsigned int node;
    CliDbOpen *open;
    unsigned int at;
    const char *text;
    unsigned long long size;
    CliDbConnection *connection_out;
    CliDbPaneTree *kept_out;
} DbPaneCtx;

// the connection `at` read out of the connections cell into `connection_out`
EMBED_INLINE void connection_read(const DbPaneCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                            .offset = CLI_DB_PANE_CONNECTION_AT(args->at),
                                                            .dst = args->connection_out,
                                                            .bytes = sizeof(CliDbConnection)});
}

// the open connection `at`, NULL where it is not open, read out of the connections cell
EMBED_INLINE CliDbOpen *open_read(const DbPaneCtx *args)
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

// the transaction's state of the connection `at` as its open one `open` has it written into the connections cell
EMBED_INLINE void transaction_written(const DbPaneCtx *args)
{
    char transaction[CLI_DB_PANE_TRANSACTION] = {0};
    (void)cellul.copy(
        &(CatenaFinitaCfg){.dst = transaction,
                           .src = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = args->open),
                           .cap = CLI_DB_PANE_TRANSACTION});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                             .offset = CLI_DB_PANE_TRANSACTION_AT(args->at),
                                                             .src = transaction,
                                                             .bytes = sizeof(transaction)});
}

void cli_db_pane_begin(const CliDbPaneCfg *args)
{
    g_cli_db_pane_state.woken = args->woken;
    g_cli_db_pane_state.chosen = CLI_DB_PANE_NONE;
    g_cli_db_pane_state.menu_node = CLI_DB_PANE_NONE;
    g_cli_db_pane_state.form.focused = CLI_DB_PANE_NONE;
}

void cli_db_pane_labels_laid(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    unsigned long long bytes = 0ULL;
    CliDbConnection connection;
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        EMBED_CALL(connection_read, DbPaneCtx, .at = at, .connection_out = &connection);
        bytes += cellul.len(&(CatenaFinitaCfg){.src = connection.name, .cap = CLI_NAME_MAX_LEN}) +
                 cellul.len(&(CatenaFinitaCfg){.src = connection.type, .cap = 16ULL}) + sizeof(PANE_OPEN_MARK) + 4ULL;
    }
    // the labels laid in the temporary tier, each's place first and the texts after, then written into their movable
    // cell whole; the cell before given back first
    const unsigned long long starts = (unsigned long long)pane->connections * sizeof(unsigned long long);
    if (pane->labels != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->labels, g_cli_db_pane_owner);
        pane->labels = NULL;
    }
    unsigned char *const laid =
        (pane->connections != 0u) ? g_cli_guard->temporary_buf_alloc(starts + bytes, g_cli_db_pane_owner) : NULL;
    if (laid == NULL)
    {
        return;
    }
    unsigned long long *const label_at = (unsigned long long *)(void *)laid;
    VerbaLinea line = {(char *)(laid + starts), bytes, 0ULL};
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        EMBED_CALL(connection_read, DbPaneCtx, .at = at, .connection_out = &connection);
        label_at[at] = starts + line.at;
        (void)mmgr_line_put(&line, connection.name);
        (void)mmgr_line_put(&line, " (");
        (void)mmgr_line_put(&line, connection.type);
        (void)mmgr_line_put(&line, (EMBED_CALL(open_read, DbPaneCtx, .at = at) != NULL) ? ")" PANE_OPEN_MARK : ")");
        mmgr_line_ch(&line, '\0');
    }
    pane->labels = g_cli_guard->movable_buf_alloc(starts + line.at, g_cli_db_pane_owner);
    if (pane->labels != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = pane->labels, .offset = 0ULL, .src = laid, .bytes = starts + line.at});
    }
    g_cli_guard->persistent_buf_release(laid, g_cli_db_pane_owner);
}

unsigned long long cli_db_pane_label_read(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    pane->label[0] = '\0';
    if ((pane->labels == NULL) || (args->at >= pane->connections))
    {
        return 0ULL;
    }
    unsigned long long place = 0ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = pane->labels,
                                                            .offset = (unsigned long long)args->at * sizeof(place),
                                                            .dst = &place,
                                                            .bytes = sizeof(place)});
    const unsigned long long measured =
        g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = pane->labels, .offset = place});
    const unsigned long long length = (measured < sizeof(pane->label) - 1ULL) ? measured : sizeof(pane->label) - 1ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = pane->labels, .offset = place, .dst = pane->label, .bytes = length});
    pane->label[length] = '\0';
    return length;
}

void cli_db_pane_note_set(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    char words[CLI_NAME_MAX_LEN + sizeof(PANE_NOT_CONNECTED)];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    const char *text = args->text;
    if (text == NULL)
    {
        // the sentence the state stands as: no connection chosen, one chosen and not connected, or none where it is
        if (pane->chosen == CLI_DB_PANE_NONE)
        {
            text = PANE_NONE_CHOSEN;
        }
        else if (EMBED_CALL(open_read, DbPaneCtx, .at = pane->chosen) == NULL)
        {
            CliDbConnection connection;
            EMBED_CALL(connection_read, DbPaneCtx, .at = pane->chosen, .connection_out = &connection);
            (void)mmgr_line_put(&line, connection.name);
            (void)mmgr_line_put(&line, PANE_NOT_CONNECTED);
            (void)mmgr_line_finish(&line);
            text = words;
        }
    }
    // the sentence cut to the pane's note bytes
    const unsigned long long length =
        (text != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = text, .cap = sizeof(pane->note) - 1ULL}) : 0ULL;
    pane->noted = text != NULL;
    if (pane->noted)
    {
        memor.cpy(&(MemoriaCfg){.dst = pane->note, .src = text, .bytes = length});
    }
    pane->note[length] = '\0';
    pane->note_length = length;
    pane->note_failed = pane->noted && args->failed;
}

// the connection of the name `name` among the open ones before the read, NULL where none is
EMBED_INLINE CliDbOpen *open_named(const DbPaneCtx *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbConnection connection;
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        CliDbOpen *const open = EMBED_CALL(open_read, DbPaneCtx, .at = at);
        EMBED_CALL(connection_read, DbPaneCtx, .at = at, .connection_out = &connection);
        if ((open != NULL) &&
            cellul.eq(&(CatenaFinitaCfg){.src = connection.name, .other = args->name, .cap = CLI_NAME_MAX_LEN}))
        {
            return open;
        }
    }
    return NULL;
}

// the kept tree of the connection `at` read out of the connections cell into `kept_out`
EMBED_INLINE void kept_read(const DbPaneCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                            .offset = CLI_DB_PANE_KEPT_AT(args->at),
                                                            .dst = args->kept_out,
                                                            .bytes = sizeof(CliDbPaneTree)});
}

void cli_db_pane_read(const CliDbPaneCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    pane->root = args->root;
    char *const listed = (args->root != NULL) ? g_cli_guard->temporary_buf_alloc(PANE_LIST, g_cli_db_pane_owner) : NULL;
    VerbaLinea out = {listed, (listed != NULL) ? PANE_LIST : 0ULL, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int list_read = (listed != NULL) &&
                          EMBED_CALL(cli_db_out_root.list, CliDbOutRootCfg, .root = args->root, .opens = NULL,
                                     .count = 0u, .out = &out, .err_msg = &err_msg) &&
                          mmgr_line_ok(&out);
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root = list_read
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                               .json = &json, .text = listed, .size = out.at, .nodes_more = 0u,
                                               .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_db_pane_owner)
                                  : CLI_JSON_NONE;
    const unsigned int count = (root != CLI_JSON_NONE) ? json.node[root].count : 0u;
    // the connections, the open one of each, the transactions and the kept trees laid in a cell of the read sized to
    // the list, then written once into the connections cell
    const unsigned long long connection_bytes = (unsigned long long)count * sizeof(CliDbConnection);
    const unsigned long long open_bytes = (unsigned long long)count * sizeof(CliDbOpen *);
    const unsigned long long transaction_bytes = (unsigned long long)count * CLI_DB_PANE_TRANSACTION;
    const unsigned long long kept_bytes = (unsigned long long)count * sizeof(CliDbPaneTree);
    const unsigned long long bytes = connection_bytes + open_bytes + transaction_bytes + kept_bytes;
    unsigned char *const cell = (count != 0u) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_db_pane_owner) : NULL;
    CliDbConnection *const connection = (CliDbConnection *)(void *)cell;
    CliDbOpen **const open = (cell != NULL) ? (CliDbOpen **)(void *)(cell + connection_bytes) : NULL;
    char (*const transaction)[CLI_DB_PANE_TRANSACTION] =
        (cell != NULL) ? (char (*)[CLI_DB_PANE_TRANSACTION])(void *)(cell + connection_bytes + open_bytes) : NULL;
    CliDbPaneTree *const kept =
        (cell != NULL) ? (CliDbPaneTree *)(void *)(cell + connection_bytes + open_bytes + transaction_bytes) : NULL;
    if (kept != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = kept, .val = 0, .bytes = kept_bytes});
    }
    unsigned int read_count = 0u;
    for (unsigned int at = 0u; (cell != NULL) && (at < count); at += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &json, .array = root, .index = at);
        if (!EMBED_CALL(cli_db_connection_err_msg.connection_get, CliDbConnectionErrMsgCfg,
                        .connection_out = &connection[read_count], .err_msg = &err_msg, .json = &json,
                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                           .object = item, .name = "connection")))
        {
            continue;
        }
        open[read_count] = EMBED_CALL(open_named, DbPaneCtx, .name = connection[read_count].name);
        transaction[read_count][0] = '\0';
        if (open[read_count] != NULL)
        {
            (void)cellul.copy(&(CatenaFinitaCfg){
                .dst = transaction[read_count],
                .src = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = open[read_count]),
                .cap = CLI_DB_PANE_TRANSACTION});
        }
        read_count += 1u;
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(listed, g_cli_db_pane_owner);
    // an open connection the tree no longer lists is closed; a kept tree moves to its connection's new place, and one
    // of a connection no longer listed is released
    CliDbConnection before;
    CliDbPaneTree kept_before;
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        CliDbOpen *const open_before = EMBED_CALL(open_read, DbPaneCtx, .at = at);
        EMBED_CALL(connection_read, DbPaneCtx, .at = at, .connection_out = &before);
        EMBED_CALL(kept_read, DbPaneCtx, .at = at, .kept_out = &kept_before);
        int listed_still = 0;
        int moved = 0;
        for (unsigned int each = 0u; each < read_count; each += 1u)
        {
            listed_still = listed_still || (open[each] == open_before);
            if ((kept != NULL) && !moved &&
                cellul.eq(
                    &(CatenaFinitaCfg){.src = connection[each].name, .other = before.name, .cap = CLI_NAME_MAX_LEN}))
            {
                kept[each] = kept_before;
                moved = 1;
            }
        }
        if ((open_before != NULL) && !listed_still && (open_before != pane->run.open))
        {
            EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .open = open_before);
            EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open_before);
        }
        if (!moved && (kept_before.node != NULL))
        {
            (void)g_cli_guard->movable_buf_release(kept_before.node, g_cli_db_pane_owner);
            (void)g_cli_guard->movable_buf_release(kept_before.text, g_cli_db_pane_owner);
        }
        if (!moved && (kept_before.shown != NULL))
        {
            (void)g_cli_guard->movable_buf_release(kept_before.shown, g_cli_db_pane_owner);
        }
    }
    if (pane->connections_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(pane->connections_cell, g_cli_db_pane_owner);
    }
    pane->connections_cell = (cell != NULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_db_pane_owner) : NULL;
    if (pane->connections_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = pane->connections_cell, .offset = 0ULL, .src = cell, .bytes = bytes});
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_db_pane_owner);
    pane->open_at = connection_bytes;
    pane->transaction_at = connection_bytes + open_bytes;
    pane->kept_at = connection_bytes + open_bytes + transaction_bytes;
    pane->connections = (pane->connections_cell != NULL) ? read_count : 0u;
    // the connection chosen stands where the tree still lists it
    const unsigned int was = pane->chosen;
    pane->chosen = CLI_DB_PANE_NONE;
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        EMBED_CALL(connection_read, DbPaneCtx, .at = at, .connection_out = &before);
        pane->chosen =
            cellul.eq(&(CatenaFinitaCfg){.src = before.name, .other = pane->chosen_name, .cap = CLI_NAME_MAX_LEN})
                ? at
                : pane->chosen;
    }
    if ((pane->chosen == CLI_DB_PANE_NONE) || (was == CLI_DB_PANE_NONE))
    {
        EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    }
    if (pane->chosen == CLI_DB_PANE_NONE)
    {
        pane->chosen_name[0] = '\0';
    }
    EMBED_CALL(cli_db_pane_internal.labels_laid, CliDbPaneCtx, .at = 0u);
    EMBED_CALL(cli_db_pane_internal.note_set, CliDbPaneCtx, .text = NULL);
    if ((pane->chosen != CLI_DB_PANE_NONE) && (EMBED_CALL(open_read, DbPaneCtx, .at = pane->chosen) != NULL) &&
        (pane->nodes == 0u))
    {
        (void)EMBED_CALL(cli_db_pane_internal.level_read, CliDbPaneCtx, .at = CLI_DB_PANE_NONE);
        EMBED_CALL(cli_db_pane_internal.shown_laid, CliDbPaneCtx, .at = 0u);
    }
}

const char *cli_db_pane_title(const CliDbPaneCfg *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    if ((pane->chosen == CLI_DB_PANE_NONE) || (pane->labels == NULL))
    {
        *args->length_out = sizeof(PANE_NONE_CHOSEN) - 1ULL;
        return PANE_NONE_CHOSEN;
    }
    *args->length_out = EMBED_CALL(cli_db_pane_internal.label_read, CliDbPaneCtx, .at = pane->chosen);
    return pane->label;
}

// whether the ask stands for the pane
EMBED_INLINE int pane_asking(const DbPaneCtx *args)
{
    (void)args;
    return g_cli_db_pane_state.asking && EMBED_CALL(cli_prompt.asked, CliPromptCfg, .context = &g_cli_db_pane_state);
}

unsigned int cli_db_pane_rows(const CliDbPaneCfg *args)
{
    (void)args;
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (EMBED_CALL(pane_asking, DbPaneCtx, .at = 0u))
    {
        return EMBED_CALL(cli_prompt_rows.rows, CliPromptRowsCfg, .at = 0u);
    }
    if (pane->form.open)
    {
        return pane->form.rows;
    }
    return pane->noted ? 1u : pane->shown_count;
}

int cli_db_pane_row(const CliDbPaneCfg *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneRow *const row = args->row;
    *row = (CliDbPaneRow){0};
    if (EMBED_CALL(pane_asking, DbPaneCtx, .at = 0u))
    {
        row->type = CLI_DB_PANE_ROW_PROMPT;
        return EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = args->at, .row = &row->prompt);
    }
    if (pane->form.open)
    {
        return EMBED_CALL(cli_db_pane_internal.form_row, CliDbPaneCtx, .at = args->at, .row = row);
    }
    if (pane->noted)
    {
        if (args->at != 0u)
        {
            return 0;
        }
        row->type = CLI_DB_PANE_ROW_NOTE;
        row->text = pane->note;
        row->text_length = pane->note_length;
        row->failed = pane->note_failed;
        return 1;
    }
    if (args->at >= pane->shown_count)
    {
        return 0;
    }
    // the node's place read out of the shown cell, the node out of the nodes, and its texts into the row's own bytes
    unsigned int place = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = pane->shown,
                                                            .offset = (unsigned long long)args->at * sizeof(place),
                                                            .dst = &place,
                                                            .bytes = sizeof(place)});
    CliDbPaneNode node;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = pane->node, .offset = (unsigned long long)place * sizeof(node), .dst = &node, .bytes = sizeof(node)});
    row->type = CLI_DB_PANE_ROW_NODE;
    row->depth = node.depth;
    const int detailed = node.detail_length != 0ULL;
    const unsigned long long detail_at = detailed ? node.detail : node.type;
    const unsigned long long detail_length = detailed ? node.detail_length : node.type_length;
    row->text_length =
        (node.label_length < sizeof(row->text_copied)) ? node.label_length : (sizeof(row->text_copied) - 1ULL);
    row->detail_length =
        (detail_length < sizeof(row->detail_copied)) ? detail_length : (sizeof(row->detail_copied) - 1ULL);
    if (row->text_length != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = pane->text, .offset = node.label, .dst = row->text_copied, .bytes = row->text_length});
    }
    if (row->detail_length != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = pane->text, .offset = detail_at, .dst = row->detail_copied, .bytes = row->detail_length});
    }
    row->text_copied[row->text_length] = '\0';
    row->detail_copied[row->detail_length] = '\0';
    row->text = row->text_copied;
    row->detail = row->detail_copied;
    row->opens = !node.leaf && !node.failed;
    row->open = node.open;
    row->failed = node.failed;
    return 1;
}

// the run on its own thread: the statement run on the query link into the answer's cell, the end set and the view's
// thread woken
static void pane_run_work(void *context)
{
    CliDbPaneRunning *const run = context;
    run->ran = EMBED_CALL(cli_db_out_failure.run, CliDbOutFailureCfg, .open = run->open, .statement = run->statement,
                          .size = run->size, .answered = run->answered, .page = CLI_DB_PANE_PAGE, .out = &run->out,
                          .failure = &run->failure);
    EMBED_ATOMIC_STORE(&run->ended, 1u);
    if (g_cli_db_pane_state.woken != NULL)
    {
        g_cli_db_pane_state.woken();
    }
}

int cli_db_pane_run(const CliDbPaneCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->run.standing || (pane->chosen == CLI_DB_PANE_NONE))
    {
        return 0;
    }
    if ((EMBED_CALL(open_read, DbPaneCtx, .at = pane->chosen) == NULL) &&
        !EMBED_CALL(cli_db_pane_internal.connected, CliDbPaneCtx, .text = NULL, .root = args->root))
    {
        return 0;
    }
    CliDbConnection connection;
    EMBED_CALL(connection_read, DbPaneCtx, .at = pane->chosen, .connection_out = &connection);
    CliDbPaneRunning *const run = &pane->run;
    // the statement and its answer are the run's thread's buffers, so they stand fixed outside the pool, one set for
    // the one run standing at a time, where they punch no hole in the persistent tier
    static char s_statement[CLI_DB_PANE_STATEMENT];
    static char s_answer[CLI_DB_PANE_ANSWER];
    if (args->size >= sizeof(s_statement))
    {
        return 0;
    }
    run->statement = s_statement;
    run->answer = s_answer;
    memor.cpy(&(MemoriaCfg){.dst = run->statement, .src = args->statement, .bytes = args->size});
    run->statement[args->size] = '\0';
    run->size = args->size;
    run->answered = args->answered;
    run->open = EMBED_CALL(open_read, DbPaneCtx, .at = pane->chosen);
    run->connection = pane->chosen;
    run->out = (VerbaLinea){run->answer, CLI_DB_PANE_ANSWER, 0ULL};
    run->ran = 0;
    run->standing = 1;
    EMBED_ATOMIC_STORE(&run->ended, 0u);
    EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = connection.name);
    run->thread = (CliThread){pane_run_work, run, 0u};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &run->thread))
    {
        run->statement = NULL;
        run->answer = NULL;
        run->standing = 0;
        EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = NULL);
        return 0;
    }
    return 1;
}

int cli_db_pane_running(const CliDbPaneCfg *args)
{
    (void)args;
    return g_cli_db_pane_state.run.standing;
}

// the ask before a guarded statement answered: the statement it confirms run, Run It pressed, or let go
static void pane_confirmed(const CliPromptCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    pane->asking = 0;
    char *const statement = pane->confirming;
    const unsigned long long size = pane->confirming_size;
    pane->confirming = NULL;
    pane->confirming_size = 0ULL;
    if ((args->at == 0u) && (statement != NULL))
    {
        (void)EMBED_CALL(cli_db_pane.run, CliDbPaneCfg, .statement = statement, .size = size, .answered = 1,
                         .root = pane->root);
    }
    g_cli_guard->persistent_buf_release(statement, g_cli_db_pane_owner);
}

// the run's answer, a confirm, read for the ask's sentence: the statement's first word, the connection and the count
// of rows it reaches, or that the connection matters; the ask opened with the statement
EMBED_INLINE void confirm_asked(const DbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneRunning *const run = &pane->run;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = args->text, .size = args->size, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_db_pane_owner);
    const unsigned int confirm = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "confirm");
    const char *const reason =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = confirm, .name = "why"),
                   .length_pointer = NULL);
    const char *const count =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = confirm, .name = "count"),
                   .length_pointer = NULL);
    CliDbConnection connection;
    EMBED_CALL(connection_read, DbPaneCtx, .at = run->connection, .connection_out = &connection);
    const char *const name = connection.name;
    char sentence[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {sentence, sizeof(sentence), 0ULL};
    if ((reason != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "every row", .cap = 16ULL}))
    {
        // the statement's first word, as the server reads it, in capitals
        unsigned long long start = 0ULL;
        while ((start < run->size) &&
               cellul.ws(&(CatenaFinitaCfg){.src = run->statement, .cap = run->size, .at = start}))
        {
            start += 1ULL;
        }
        (void)mmgr_line_put(&line, "This ");
        for (unsigned long long at = start;
             (at < run->size) && !cellul.ws(&(CatenaFinitaCfg){.src = run->statement, .cap = run->size, .at = at});
             at += 1ULL)
        {
            const char letter = run->statement[at];
            // a lower-case letter's capital is 32 below it in ASCII
            mmgr_line_ch(&line, ((letter >= 'a') && (letter <= 'z')) ? (char)(letter - 32) : letter);
        }
        (void)mmgr_line_put(&line, " reaches every row on ");
        (void)mmgr_line_put(&line, name);
        (void)mmgr_line_put(&line, ": ");
        (void)mmgr_line_put(&line, (count != NULL) ? count : "an unknown count of");
        (void)mmgr_line_put(&line, " rows.");
    }
    else
    {
        (void)mmgr_line_put(&line, name);
        (void)mmgr_line_put(&line, " is a connection that matters, and this statement writes.");
    }
    (void)mmgr_line_finish(&line);
    g_cli_guard->persistent_buf_release(json.node, g_cli_db_pane_owner);
    // the statement the ask confirms stands until it is answered, copied into a buffer of its own outside the pool
    static char s_confirming[CLI_DB_PANE_STATEMENT];
    memor.cpy(&(MemoriaCfg){.dst = s_confirming, .src = run->statement, .bytes = run->size + 1ULL});
    pane->confirming = s_confirming;
    pane->confirming_size = run->size;
    run->statement = NULL;
    pane->asking = EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_CONFIRM, .sentence = sentence,
                              .statement = pane->confirming, .statement_size = pane->confirming_size,
                              .choices = s_run_choices, .count = 2u, .then = pane_confirmed, .context = pane);
}

int cli_db_pane_step(const CliDbPaneCfg *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneRunning *const run = &pane->run;
    if (!run->standing || (EMBED_ATOMIC_EXCHANGE_32(&run->ended, 0u) == 0u))
    {
        return 0;
    }
    run->standing = 0;
    CliDbConnection connection;
    EMBED_CALL(connection_read, DbPaneCtx, .at = run->connection, .connection_out = &connection);
    const char *const name = connection.name;
    const int confirms =
        run->ran && cellul.starts(&(CatenaFinitaCfg){.src = run->answer, .cap = run->out.at, .other = "{\"confirm\""});
    if (confirms)
    {
        EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = NULL);
        EMBED_CALL(confirm_asked, DbPaneCtx, .text = run->answer, .size = run->out.at);
    }
    else if (run->ran)
    {
        (void)EMBED_CALL(cli_db_grid.adopt, CliDbGridCfg, .answer = run->answer, .size = run->out.at, .open = run->open,
                         .title = name);
    }
    else
    {
        (void)EMBED_CALL(cli_db_grid.adopt, CliDbGridCfg, .answer = NULL, .failure = &run->failure, .open = run->open,
                         .title = name);
    }
    // the transaction as the run left it, read on the view's thread now that the run's thread has ended
    if ((run->connection < pane->connections) && (EMBED_CALL(open_read, DbPaneCtx, .at = run->connection) == run->open))
    {
        EMBED_CALL(transaction_written, DbPaneCtx, .at = run->connection, .open = run->open);
    }
    g_cli_guard->persistent_buf_release(run->statement, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(run->answer, g_cli_db_pane_owner);
    run->statement = NULL;
    run->answer = NULL;
    return 1;
}
