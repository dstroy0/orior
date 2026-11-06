// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane.c: the Database pane's state, its rows and its runs. The connections the tree lists stand in a cell sized to
// them, beside a cell of the open one of each and one of each transaction's state; a read again matches the open ones
// by name, and a connection the tree no longer lists is closed. The labels stand in one cell, laid again as a
// connection opens or closes. A statement runs on a thread of its own into a cell sized to the most an answer takes;
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
} DbPaneCtx;

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
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = pane->connection[at].name, .cap = CLI_NAME_MAX_LEN}) +
                 cellul.len(&(CatenaFinitaCfg){.src = pane->connection[at].kind, .cap = 16ULL}) +
                 sizeof(PANE_OPEN_MARK) + 4ULL;
    }
    const unsigned long long starts = (unsigned long long)pane->connections * sizeof(unsigned long long);
    unsigned char *const cell =
        (pane->connections != 0u) ? g_cli_guard->persistent_buf_alloc(starts + bytes, g_cli_db_pane_owner) : NULL;
    g_cli_guard->persistent_buf_release(pane->label_at, g_cli_db_pane_owner);
    pane->label_at = (unsigned long long *)(void *)cell;
    pane->labels = (cell != NULL) ? (char *)(cell + starts) : NULL;
    VerbaLinea line = {pane->labels, (cell != NULL) ? bytes : 0ULL, 0ULL};
    for (unsigned int at = 0u; (cell != NULL) && (at < pane->connections); at += 1u)
    {
        pane->label_at[at] = line.at;
        (void)mmgr_line_put(&line, pane->connection[at].name);
        (void)mmgr_line_put(&line, " (");
        (void)mmgr_line_put(&line, pane->connection[at].kind);
        (void)mmgr_line_put(&line, (pane->open[at] != NULL) ? ")" PANE_OPEN_MARK : ")");
        mmgr_line_ch(&line, '\0');
    }
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
        else if (pane->open[pane->chosen] == NULL)
        {
            (void)mmgr_line_put(&line, pane->connection[pane->chosen].name);
            (void)mmgr_line_put(&line, PANE_NOT_CONNECTED);
            (void)mmgr_line_finish(&line);
            text = words;
        }
    }
    const unsigned long long length =
        (text != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_ERR_MSG_MAX_LEN}) : 0ULL;
    char *const note = (text != NULL) ? g_cli_guard->persistent_buf_alloc(length + 1ULL, g_cli_db_pane_owner) : NULL;
    if (note != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = note, .src = text, .bytes = length});
        note[length] = '\0';
    }
    g_cli_guard->persistent_buf_release(pane->note, g_cli_db_pane_owner);
    pane->note = note;
    pane->note_length = (note != NULL) ? length : 0ULL;
    pane->note_failed = (note != NULL) && args->failed;
}

// the connection of the name `name` among the open ones before the read, NULL where none is
EMBED_INLINE CliDbOpen *open_named(const DbPaneCtx *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        if ((pane->open[at] != NULL) &&
            cellul.eq(
                &(CatenaFinitaCfg){.src = pane->connection[at].name, .other = args->name, .cap = CLI_NAME_MAX_LEN}))
        {
            return pane->open[at];
        }
    }
    return NULL;
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
    const unsigned int root =
        list_read ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                               .json = &json, .text = listed, .size = out.at, .nodes_more = 0u, .text_more = 0ULL,
                               .err_msg = &err_msg, .owner = g_cli_db_pane_owner)
                  : CLI_JSON_NONE;
    const unsigned int count = (root != CLI_JSON_NONE) ? json.node[root].count : 0u;
    // the connections, the open one of each, the transactions and the kept trees in cells sized to the list
    const unsigned long long connection_bytes = (unsigned long long)count * sizeof(CliDbConnection);
    const unsigned long long open_bytes = (unsigned long long)count * sizeof(CliDbOpen *);
    const unsigned long long transaction_bytes = (unsigned long long)count * 16ULL;
    const unsigned long long kept_bytes = (unsigned long long)count * sizeof(CliDbPaneTree);
    unsigned char *const cell =
        (count != 0u) ? g_cli_guard->persistent_buf_alloc(connection_bytes + open_bytes + transaction_bytes + kept_bytes,
                                                          g_cli_db_pane_owner)
                      : NULL;
    CliDbConnection *const connection = (CliDbConnection *)(void *)cell;
    CliDbOpen **const open = (cell != NULL) ? (CliDbOpen **)(void *)(cell + connection_bytes) : NULL;
    char (*const transaction)[16] =
        (cell != NULL) ? (char (*)[16])(void *)(cell + connection_bytes + open_bytes) : NULL;
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
                .cap = 16ULL});
        }
        read_count += 1u;
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(listed, g_cli_db_pane_owner);
    // an open connection the tree no longer lists is closed; a kept tree moves to its connection's new place, and one
    // of a connection no longer listed is released
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        int listed_still = 0;
        for (unsigned int each = 0u; each < read_count; each += 1u)
        {
            listed_still = listed_still || (open[each] == pane->open[at]);
            if ((kept != NULL) && (pane->kept != NULL) &&
                cellul.eq(&(CatenaFinitaCfg){.src = connection[each].name,
                                             .other = pane->connection[at].name,
                                             .cap = CLI_NAME_MAX_LEN}))
            {
                kept[each] = pane->kept[at];
                memor.set(&(MemoriaCfg){.dst = &pane->kept[at], .val = 0, .bytes = sizeof(pane->kept[at])});
            }
        }
        if ((pane->open[at] != NULL) && !listed_still && (pane->open[at] != pane->run.open))
        {
            EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .open = pane->open[at]);
            EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = pane->open[at]);
        }
        if (pane->kept != NULL)
        {
            g_cli_guard->persistent_buf_release(pane->kept[at].node, g_cli_db_pane_owner);
            g_cli_guard->persistent_buf_release(pane->kept[at].text, g_cli_db_pane_owner);
            g_cli_guard->persistent_buf_release(pane->kept[at].shown, g_cli_db_pane_owner);
        }
    }
    g_cli_guard->persistent_buf_release(pane->connection, g_cli_db_pane_owner);
    pane->connection = connection;
    pane->open = open;
    pane->transaction = transaction;
    pane->kept = kept;
    pane->connections = (cell != NULL) ? read_count : 0u;
    // the connection chosen stands where the tree still lists it
    const unsigned int was = pane->chosen;
    pane->chosen = CLI_DB_PANE_NONE;
    for (unsigned int at = 0u; at < pane->connections; at += 1u)
    {
        pane->chosen = cellul.eq(&(CatenaFinitaCfg){
                           .src = pane->connection[at].name, .other = pane->chosen_name, .cap = CLI_NAME_MAX_LEN})
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
    if ((pane->chosen != CLI_DB_PANE_NONE) && (pane->open[pane->chosen] != NULL) && (pane->nodes == 0u))
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
    const char *const label = pane->labels + pane->label_at[pane->chosen];
    *args->length_out = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_NAME_MAX_LEN + 32ULL});
    return label;
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
    return (pane->note != NULL) ? 1u : pane->shown_count;
}

int cli_db_pane_row(const CliDbPaneCfg *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneRow *const row = args->row;
    *row = (CliDbPaneRow){0};
    if (EMBED_CALL(pane_asking, DbPaneCtx, .at = 0u))
    {
        row->kind = CLI_DB_PANE_ROW_PROMPT;
        return EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = args->at, .row = &row->prompt);
    }
    if (pane->form.open)
    {
        return EMBED_CALL(cli_db_pane_internal.form_row, CliDbPaneCtx, .at = args->at, .row = row);
    }
    if (pane->note != NULL)
    {
        if (args->at != 0u)
        {
            return 0;
        }
        row->kind = CLI_DB_PANE_ROW_NOTE;
        row->text = pane->note;
        row->text_length = pane->note_length;
        row->failed = pane->note_failed;
        return 1;
    }
    if (args->at >= pane->shown_count)
    {
        return 0;
    }
    const CliDbPaneNode *const node = &pane->node[pane->shown[args->at]];
    row->kind = CLI_DB_PANE_ROW_NODE;
    row->depth = node->depth;
    row->text = pane->text + node->label;
    row->text_length = node->label_length;
    const int detailed = node->detail_length != 0ULL;
    row->detail = pane->text + (detailed ? node->detail : node->kind);
    row->detail_length = detailed ? node->detail_length : node->kind_length;
    row->opens = !node->leaf && !node->failed;
    row->open = node->open;
    row->failed = node->failed;
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
    if ((pane->open[pane->chosen] == NULL) &&
        !EMBED_CALL(cli_db_pane_internal.connected, CliDbPaneCtx, .text = NULL, .root = args->root))
    {
        return 0;
    }
    CliDbPaneRunning *const run = &pane->run;
    run->statement = g_cli_guard->persistent_buf_alloc(args->size + 1ULL, g_cli_db_pane_owner);
    run->answer =
        (run->statement != NULL) ? g_cli_guard->persistent_buf_alloc(CLI_DB_PANE_ANSWER, g_cli_db_pane_owner) : NULL;
    if (run->answer == NULL)
    {
        g_cli_guard->persistent_buf_release(run->statement, g_cli_db_pane_owner);
        run->statement = NULL;
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->statement, .src = args->statement, .bytes = args->size});
    run->statement[args->size] = '\0';
    run->size = args->size;
    run->answered = args->answered;
    run->open = pane->open[pane->chosen];
    run->connection = pane->chosen;
    run->out = (VerbaLinea){run->answer, CLI_DB_PANE_ANSWER, 0ULL};
    run->ran = 0;
    run->standing = 1;
    EMBED_ATOMIC_STORE(&run->ended, 0u);
    EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .title = pane->connection[pane->chosen].name);
    run->thread = (CliThread){pane_run_work, run, 0u};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &run->thread))
    {
        g_cli_guard->persistent_buf_release(run->statement, g_cli_db_pane_owner);
        g_cli_guard->persistent_buf_release(run->answer, g_cli_db_pane_owner);
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
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->text, .size = args->size, .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg,
                   .owner = g_cli_db_pane_owner);
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
    const char *const name = pane->connection[run->connection].name;
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
    // the statement the ask confirms stands until it is answered
    g_cli_guard->persistent_buf_release(pane->confirming, g_cli_db_pane_owner);
    pane->confirming = run->statement;
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
    const char *const name = pane->connection[run->connection].name;
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
    if ((run->connection < pane->connections) && (pane->open[run->connection] == run->open))
    {
        (void)cellul.copy(
            &(CatenaFinitaCfg){.dst = pane->transaction[run->connection],
                               .src = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = run->open),
                               .cap = 16ULL});
    }
    g_cli_guard->persistent_buf_release(run->statement, g_cli_db_pane_owner);
    g_cli_guard->persistent_buf_release(run->answer, g_cli_db_pane_owner);
    run->statement = NULL;
    run->answer = NULL;
    return 1;
}
