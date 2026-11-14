// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane_act.c: the Database pane's head, the connection it works on and the bar of tools, each tool's label and
// whether it can act read from the pane's state, and what each does: a connection opened, its password asked for
// where its server refuses the one it has, or closed; the form opened; the schema read again; a run stopped; and the
// transaction committed or rolled back through a run
#include "db_pane_internal.h"

// the sentence the ask for a password stands under, and its button
#define ACT_PASSWORD "The server refused the connection's password:"

static const char *const s_connect_choices[1] = {"Connect"};

// the tools' labels from the bar's first tool, Connect's while the connection is not open
static const char *const s_tool_label[CLI_DB_PANE_LAST_TOOL - CLI_DB_PANE_FIRST_TOOL + 1u] = {
    "Connect", "New", "Edit", "Read Again", "Stop Reading", "Stop", "Commit", "Roll Back"};

// the words of a refusal that ask for a password: PostgreSQL's code for one, MySQL's, and the words both servers use
static const char *const s_refusal_words[4] = {"28P01", "1045", "password", "authentication"};

typedef struct
{
    const CliDbFailure *failure;
    const char *text;
    unsigned int at;
    CliDbOpen *open;
} DbPaneActCtx;

// the open connection `at`, NULL where it is not open, read out of the connections cell
EMBED_INLINE CliDbOpen *open_read(const DbPaneActCtx *args)
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

// the connection `at`'s open one set to `open`, NULL for none, with its transaction's state as the open one has it,
// empty for none, each written into the connections cell
EMBED_INLINE void open_written(const DbPaneActCtx *args)
{
    CliDbOpen *const open = args->open;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                             .offset = CLI_DB_PANE_OPEN_AT(args->at),
                                                             .src = &open,
                                                             .bytes = sizeof(open)});
    char transaction[CLI_DB_PANE_TRANSACTION] = {0};
    if (open != NULL)
    {
        (void)cellul.copy(
            &(CatenaFinitaCfg){.dst = transaction,
                               .src = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = open),
                               .cap = CLI_DB_PANE_TRANSACTION});
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = g_cli_db_pane_state.connections_cell,
                                                             .offset = CLI_DB_PANE_TRANSACTION_AT(args->at),
                                                             .src = transaction,
                                                             .bytes = sizeof(transaction)});
}

unsigned int cli_db_pane_head_rows(const CliDbPaneHeadCfg *args)
{
    (void)args;
    return CLI_DB_PANE_HEAD_ROWS;
}

// whether the chosen connection is open
EMBED_INLINE int chosen_open(const DbPaneActCtx *args)
{
    (void)args;
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    return (pane->chosen != CLI_DB_PANE_NONE) && (EMBED_CALL(open_read, DbPaneActCtx, .at = pane->chosen) != NULL);
}

const char *cli_db_pane_head_tool(const CliDbPaneHeadCfg *args)
{
    const CliDbPaneState *const pane = &g_cli_db_pane_state;
    const int open = EMBED_CALL(chosen_open, DbPaneActCtx, .text = NULL);
    const int running = pane->run.standing;
    // the chosen connection's transaction state read out of the connections cell
    char transaction[CLI_DB_PANE_TRANSACTION] = {0};
    if (open)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = pane->connections_cell,
                                                                .offset = CLI_DB_PANE_TRANSACTION_AT(pane->chosen),
                                                                .dst = transaction,
                                                                .bytes = sizeof(transaction)});
        transaction[sizeof(transaction) - 1u] = '\0';
    }
    const int in_transaction = cellul.eq(&(CatenaFinitaCfg){.src = transaction, .other = "open", .cap = 16ULL}) ||
                               cellul.eq(&(CatenaFinitaCfg){.src = transaction, .other = "failed", .cap = 16ULL});
    if (args->which == CLI_DB_PANE_PICK)
    {
        *args->enabled_out = pane->connections != 0u;
        return EMBED_CALL(cli_db_pane.title, CliDbPaneCfg, .length_out = args->length_out);
    }
    if ((args->which < CLI_DB_PANE_FIRST_TOOL) || (args->which > CLI_DB_PANE_LAST_TOOL))
    {
        *args->enabled_out = 0;
        *args->length_out = 0ULL;
        return "";
    }
    const char *const label = ((args->which == CLI_DB_PANE_CONNECT) && open)
                                  ? "Disconnect"
                                  : s_tool_label[args->which - CLI_DB_PANE_FIRST_TOOL];
    switch (args->which)
    {
    case CLI_DB_PANE_CONNECT:
        *args->enabled_out = (pane->chosen != CLI_DB_PANE_NONE) && !running;
        break;
    case CLI_DB_PANE_EDIT:
        *args->enabled_out = (pane->chosen != CLI_DB_PANE_NONE) && !pane->form.open;
        break;
    case CLI_DB_PANE_NEW:
        *args->enabled_out = !pane->form.open;
        break;
    case CLI_DB_PANE_STOP_READING:
        // a level is read on the view's thread, between frames, so that no reading stands for the bar to stop
        *args->enabled_out = 0;
        break;
    case CLI_DB_PANE_STOP:
        *args->enabled_out = running;
        break;
    case CLI_DB_PANE_COMMIT:
    case CLI_DB_PANE_ROLLBACK:
        *args->enabled_out = open && in_transaction && !running;
        break;
    default:
        *args->enabled_out = 1;
        break;
    }
    *args->length_out = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 32ULL});
    return label;
}

// whether a failure is the server's refusal of the password
EMBED_INLINE int refused_password(const DbPaneActCtx *args)
{
    int refused = 0;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        refused =
            refused ||
            cellul.has(&(CatenaFinitaCfg){.src = args->failure->message,
                                          .cap = CLI_DB_FIELD,
                                          .other = s_refusal_words[at],
                                          .other_cap = 32ULL,
                                          .ci = 1}) ||
            cellul.has(&(CatenaFinitaCfg){
                .src = args->failure->code, .cap = 64ULL, .other = s_refusal_words[at], .other_cap = 32ULL, .ci = 1});
    }
    return refused;
}

// the ask for the password answered: the connection opened again with the password typed
static void password_answered(const CliPromptCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    pane->asking = 0;
    if (args->at != 0u)
    {
        return;
    }
    // the password typed, with its end, in a cell of this answer alone, wiped before it is given back
    char *const password = g_cli_guard->temporary_buf_alloc(args->size + 1ULL, g_cli_db_pane_owner);
    if (password == NULL)
    {
        return;
    }
    if (args->size != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = password, .src = args->text, .bytes = args->size});
    }
    password[args->size] = '\0';
    (void)EMBED_CALL(cli_db_pane_internal.connected, CliDbPaneCtx, .text = password, .root = pane->root);
    memor.set(&(MemoriaCfg){.dst = password, .val = 0, .bytes = args->size + 1ULL});
    g_cli_guard->persistent_buf_release(password, g_cli_db_pane_owner);
}

int cli_db_pane_connected(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->chosen == CLI_DB_PANE_NONE)
    {
        return 0;
    }
    const unsigned int chosen = pane->chosen;
    CliDbFailure failure;
    // the chosen connection read out of the connections cell for the connect
    CliDbConnection connection;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = pane->connections_cell,
                                                            .offset = CLI_DB_PANE_CONNECTION_AT(chosen),
                                                            .dst = &connection,
                                                            .bytes = sizeof(connection)});
    CliDbOpen *const open = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = args->root, .config = &connection,
                                       .password = args->text, .failure = &failure);
    if (open == NULL)
    {
        if (EMBED_CALL(refused_password, DbPaneActCtx, .failure = &failure))
        {
            pane->asking =
                EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_PASSWORD, .sentence = ACT_PASSWORD,
                           .choices = s_connect_choices, .count = 1u, .then = password_answered, .context = pane);
        }
        EMBED_CALL(cli_db_pane_internal.note_set, CliDbPaneCtx, .text = failure.message, .failed = 1);
        return 0;
    }
    EMBED_CALL(open_written, DbPaneActCtx, .at = chosen, .open = open);
    EMBED_CALL(cli_db_pane_internal.labels_laid, CliDbPaneCtx, .at = 0u);
    EMBED_CALL(cli_db_pane_internal.note_set, CliDbPaneCtx, .text = NULL);
    EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    (void)EMBED_CALL(cli_db_pane_internal.level_read, CliDbPaneCtx, .at = CLI_DB_PANE_NONE);
    EMBED_CALL(cli_db_pane_internal.shown_laid, CliDbPaneCtx, .at = 0u);
    return 1;
}

// the chosen connection closed, the grid's result of it let go
EMBED_INLINE void chosen_closed(const DbPaneActCtx *args)
{
    (void)args;
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbOpen *const open = EMBED_CALL(open_read, DbPaneActCtx, .at = pane->chosen);
    EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .open = open);
    EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open);
    EMBED_CALL(open_written, DbPaneActCtx, .at = pane->chosen, .open = NULL);
    EMBED_CALL(cli_db_pane_internal.labels_laid, CliDbPaneCtx, .at = 0u);
    EMBED_CALL(cli_db_pane_internal.schema_released, CliDbPaneCtx, .at = 0u);
    EMBED_CALL(cli_db_pane_internal.note_set, CliDbPaneCtx, .text = NULL);
}

void cli_db_pane_action(const CliDbPaneHeadCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    int enabled = 0;
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_db_pane_head.head_tool, CliDbPaneHeadCfg, .which = args->which, .enabled_out = &enabled,
                     .length_out = &length);
    if (!enabled)
    {
        return;
    }
    switch (args->which)
    {
    case CLI_DB_PANE_PICK: {
        const unsigned int next = (pane->chosen == CLI_DB_PANE_NONE) ? 0u : (pane->chosen + 1u) % pane->connections;
        // the next connection's name read out of the connections cell
        CliDbConnection connection;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = pane->connections_cell,
                                                                .offset = CLI_DB_PANE_CONNECTION_AT(next),
                                                                .dst = &connection,
                                                                .bytes = sizeof(connection)});
        const char *const words[1] = {connection.name};
        (void)EMBED_CALL(cli_db_pane_tree.command, CliDbPaneTreeCfg, .name = "db-choose", .words = words, .count = 1u,
                         .root = args->root);
        return;
    }
    case CLI_DB_PANE_CONNECT:
        if (EMBED_CALL(chosen_open, DbPaneActCtx, .text = NULL))
        {
            EMBED_CALL(chosen_closed, DbPaneActCtx, .text = NULL);
        }
        else
        {
            (void)EMBED_CALL(cli_db_pane_internal.connected, CliDbPaneCtx, .text = NULL, .root = args->root);
        }
        return;
    case CLI_DB_PANE_NEW:
    case CLI_DB_PANE_EDIT:
        EMBED_CALL(cli_db_pane_internal.form_open, CliDbPaneCtx, .editing = args->which == CLI_DB_PANE_EDIT);
        return;
    case CLI_DB_PANE_READ:
        EMBED_CALL(cli_db_pane.read, CliDbPaneCfg, .root = args->root);
        (void)EMBED_CALL(cli_db_pane_tree.command, CliDbPaneTreeCfg, .name = "db-read-again", .root = args->root);
        return;
    case CLI_DB_PANE_STOP: {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_db_err_msg.stop, CliDbErrMsgCfg, .open = pane->run.open, .err_msg = &err_msg);
        return;
    }
    case CLI_DB_PANE_COMMIT:
        (void)EMBED_CALL(cli_db_pane.run, CliDbPaneCfg, .statement = "COMMIT", .size = 6ULL, .answered = 1,
                         .root = args->root);
        return;
    case CLI_DB_PANE_ROLLBACK:
        (void)EMBED_CALL(cli_db_pane.run, CliDbPaneCfg, .statement = "ROLLBACK", .size = 8ULL, .answered = 1,
                         .root = args->root);
        return;
    default:
        return;
    }
}
