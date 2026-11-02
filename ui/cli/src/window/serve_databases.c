// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_databases.c: the database connections open, each by its name in the tree's databases.json, and the database
// window's calls: listed, saved, taken out, connected and closed, a statement run and its rows read a page at a time,
// stopped, the schema's children read, the rows exported, the history read, and the statements of a text found. A
// connection in use stays open until its call ends, one closed meanwhile closing then
#include "../command_line/callback_cfg.h"
#include "../databases/db.h"
#include "serve_internal.h"

// A connection open: whether its place is taken, its name, the connection, how many calls use it, and whether it is
// let go, to close as its last call ends; and what its server last reported went wrong, under its own lock, which a
// call that may fail acquires
typedef struct
{
    int used;
    char name[CLI_NAME_MAX_LEN];
    CliDbOpen *open;
    unsigned int users;
    int letting_go;
    CliLock failing;
    CliDbFailure failure;
} CliServeDatabase;
// the most connections open at once
#define CLI_SERVE_DATABASES 64u

static CliServeDatabase s_database[CLI_SERVE_DATABASES];
static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    CliServeDatabase *one;
    const char *name;
    VerbaLinea *err_msg;
    unsigned int at;
    const CliDbFailure *failure;
    VerbaLinea *out;
    const CliServeArgs *request;
} ServeDatabasesCtx;
// a connection let go of its name, closed now where no call uses it; under the lock
EMBED_INLINE void databases_let_go(const ServeDatabasesCtx *args)
{
    args->one->name[0] = '\0';
    args->one->letting_go = 1;
    if (args->one->users == 0u)
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = args->one->open);
        args->one->used = 0;
    }
}

void cli_serve_databases_close(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    for (unsigned int at = 0u; at < CLI_SERVE_DATABASES; at += 1u)
    {
        if (s_database[at].used && !s_database[at].letting_go)
        {
            EMBED_CALL(databases_let_go, ServeDatabasesCtx, .one = &s_database[at]);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// the connection open as `name` taken for a call: its place, or CLI_SERVE_DATABASES with why given
EMBED_INLINE unsigned int databases_take(const ServeDatabasesCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    unsigned int at = 0u;
    while ((at < CLI_SERVE_DATABASES) &&
           (!s_database[at].used || s_database[at].letting_go ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = s_database[at].name, .other = args->name, .cap = CLI_CELLS_POOL}))))
    {
        at += 1u;
    }
    if (at < CLI_SERVE_DATABASES)
    {
        s_database[at].users += 1u;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (at == CLI_SERVE_DATABASES)
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " is not connected");
    }
    return at;
}

// a call's use of a connection ended, the connection closed where it was let go meanwhile
EMBED_INLINE void databases_free(const ServeDatabasesCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_database[args->at].users -= 1u;
    if (s_database[args->at].letting_go && (s_database[args->at].users == 0u))
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = s_database[args->at].open);
        s_database[args->at].used = 0;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// the connection open as `name` let go, where one is
EMBED_INLINE void databases_name_let_go(const ServeDatabasesCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    for (unsigned int at = 0u; at < CLI_SERVE_DATABASES; at += 1u)
    {
        if (s_database[at].used && !s_database[at].letting_go &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_database[at].name, .other = args->name, .cap = CLI_CELLS_POOL})))
        {
            EMBED_CALL(databases_let_go, ServeDatabasesCtx, .one = &s_database[at]);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// what a server reported went wrong, as one text: its message, its code, its detail and its hint
EMBED_INLINE void databases_failure_msg_write(const ServeDatabasesCtx *args)
{
    mmgr_line_put(args->err_msg, args->failure->message);
    if (args->failure->code[0] != '\0')
    {
        mmgr_line_put(args->err_msg, " (");
        mmgr_line_put(args->err_msg, args->failure->code);
        mmgr_line_ch(args->err_msg, ')');
    }
    if (args->failure->detail[0] != '\0')
    {
        mmgr_line_ch(args->err_msg, '\n');
        mmgr_line_put(args->err_msg, args->failure->detail);
    }
    if (args->failure->hint[0] != '\0')
    {
        mmgr_line_ch(args->err_msg, '\n');
        mmgr_line_put(args->err_msg, args->failure->hint);
    }
}

// what a server reported went wrong as the page reads it: `{"message", "code", "detail", "hint", "position"}`
EMBED_INLINE void databases_failure_write(const ServeDatabasesCtx *args)
{
    VerbaLinea *out = args->out;
    const char *const field_name[4] = {"{\"message\":", ",\"code\":", ",\"detail\":", ",\"hint\":"};
    const char *const field[4] = {args->failure->message, args->failure->code, args->failure->detail,
                                  args->failure->hint};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        mmgr_line_put(out, field_name[at]);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = field[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = field[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, ",\"position\":");
    mmgr_line_decimal(out, args->failure->position, 1u);
    mmgr_line_ch(out, '}');
}

static int db_list(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    static CliDbOpen *s_open[CLI_SERVE_DATABASES];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < CLI_SERVE_DATABASES; at += 1u)
    {
        if (s_database[at].used && !s_database[at].letting_go)
        {
            s_open[count] = s_database[at].open;
            count += 1u;
        }
    }
    const int listed = EMBED_CALL(cli_db_out_root.list, CliDbOutRootCfg, .root = root, .opens = s_open, .count = count,
                                  .out = out, .err_msg = err_msg);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return listed;
}

static int db_save(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    static CliDbConnection s_connection;
    static CliLock s_connection_lock = CLI_LOCK_INIT;
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_connection_lock);
    int done = EMBED_CALL(cli_db_connection_err_msg.connection_get, CliDbConnectionErrMsgCfg, .json = arguments->json,
                          .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments,
                                             .key = "connection"),
                          .connection_out = &s_connection, .err_msg = err_msg) &&
               EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .root = root, .connection = &s_connection,
                          .was = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                            .request = arguments, .key = "was", .otherwise = NULL),
                          .err_msg = err_msg);
    const char *const password = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                            .request = arguments, .key = "password", .otherwise = "");
    if (done && (password[0] != '\0'))
    {
        char account[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea named = {account, sizeof(account), 0ull};
        EMBED_CALL(cli_db_connection_out_kind.account, CliDbConnectionOutKindCfg, .connection = &s_connection,
                   .out = &named);
        (void)mmgr_line_finish(&named);
        done = EMBED_CALL(cli_err_msg_service_internal.keychain_store, CliErrMsgServiceCtx, .service = "database",
                          .account = account, .secret = password, .err_msg = err_msg);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_connection_lock);
    mmgr_line_put(out, done ? "null" : "");
    return done;
}

static int db_remove(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if ((name == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(databases_name_let_go, ServeDatabasesCtx, .name = name);
    const int done =
        EMBED_CALL(cli_db_root_name.remove, CliDbRootNameCfg, .root = root, .name = name, .err_msg = err_msg);
    mmgr_line_put(args->out, done ? "null" : "");
    return done;
}

static int db_forget_password(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    static CliDbConnection s_connection;
    static CliLock s_connection_lock = CLI_LOCK_INIT;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if ((name == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_connection_lock);
    int done = EMBED_CALL(cli_db_root_name.by_name, CliDbRootNameCfg, .root = root, .name = name,
                          .connection_out = &s_connection, .err_msg = err_msg);
    if (done)
    {
        char account[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea named = {account, sizeof(account), 0ull};
        EMBED_CALL(cli_db_connection_out_kind.account, CliDbConnectionOutKindCfg, .connection = &s_connection,
                   .out = &named);
        (void)mmgr_line_finish(&named);
        done = EMBED_CALL(cli_err_msg_service_internal.keychain_forget, CliErrMsgServiceCtx, .service = "database",
                          .account = account, .err_msg = err_msg);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_connection_lock);
    mmgr_line_put(out, done ? "null" : "");
    return done;
}

static int db_connect(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    static CliDbConnection s_connection;
    static CliDbFailure s_failure;
    static CliLock s_connection_lock = CLI_LOCK_INIT;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if ((name == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    const char *const password = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                            .request = arguments, .key = "password", .otherwise = NULL);
    const int save_password = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                         .key = "save", .otherwise_i32 = 0);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_connection_lock);
    if (!EMBED_CALL(cli_db_root_name.by_name, CliDbRootNameCfg, .root = root, .name = name,
                    .connection_out = &s_connection, .err_msg = err_msg))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_connection_lock);
        return 0;
    }
    EMBED_CALL(databases_name_let_go, ServeDatabasesCtx, .name = name);
    memor.set(&(MemoriaCfg){.dst = &s_failure, .val = 0, .bytes = sizeof(s_failure)});
    CliDbOpen *const open = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = root, .config = &s_connection,
                                       .password = password, .failure = &s_failure);
    int done = (open != NULL);
    if (!done)
    {
        EMBED_CALL(databases_failure_msg_write, ServeDatabasesCtx, .failure = &s_failure, .err_msg = err_msg);
    }
    if (done && save_password && (password != NULL) && (password[0] != '\0'))
    {
        char account[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea named = {account, sizeof(account), 0ull};
        EMBED_CALL(cli_db_connection_out_kind.account, CliDbConnectionOutKindCfg, .connection = &s_connection,
                   .out = &named);
        (void)mmgr_line_finish(&named);
        done = EMBED_CALL(cli_err_msg_service_internal.keychain_store, CliErrMsgServiceCtx, .service = "database",
                          .account = account, .secret = password, .err_msg = err_msg);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_connection_lock);
    if (!done)
    {
        if (open != NULL)
        {
            EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open);
        }
        return 0;
    }
    mmgr_line_put(out, "{\"version\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = EMBED_CALL(cli_db.version, CliDbCfg, .open_read = open),
               .length = cellul.len(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_db.version, CliDbCfg, .open_read = open),
                                                       .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"transaction\":");
    EMBED_CALL(
        cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
        .text = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = open),
        .length = cellul.len(&(CatenaFinitaCfg){
            .src = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = open), .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"zone\":\"");
    char zone[CLI_NAME_MAX_LEN];
    VerbaLinea zoned = {zone, sizeof(zone), 0ull};
    EMBED_CALL(cli_db_out_count.zone, CliDbOutCountCfg, .open = open, .out = &zoned);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)zone, .length = mmgr_line_ok(&zoned) ? zoned.at : 0ull);
    mmgr_line_put(out, "\"}");
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    unsigned int at = 0u;
    while ((at < CLI_SERVE_DATABASES) && s_database[at].used)
    {
        at += 1u;
    }
    if (at < CLI_SERVE_DATABASES)
    {
        s_database[at] = (CliServeDatabase){1, "", open, 0u, 0};
        memor.cpy(&(MemoriaCfg){.dst = s_database[at].name,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (at == CLI_SERVE_DATABASES)
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open);
    }
    return done;
}

static int db_close(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if (name == NULL)
    {
        return 0;
    }
    EMBED_CALL(databases_name_let_go, ServeDatabasesCtx, .name = name);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int db_run(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    const char *const sql = (name != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "sql", .err_msg = err_msg)
                                           : NULL;
    const unsigned int at = (sql != NULL)
                                ? EMBED_CALL(databases_take, ServeDatabasesCtx, .name = name, .err_msg = err_msg)
                                : CLI_SERVE_DATABASES;
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const unsigned long long start = out->at;
    if (!EMBED_CALL(cli_db_out_failure.run, CliDbOutFailureCfg, .open = one->open, .statement = sql,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = sql, .cap = CLI_CELLS_POOL}),
                    .answered = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                           .key = "answered", .otherwise_i32 = 0),
                    .page = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                       .key = "page", .otherwise_u64 = 200ull),
                    .out = out, .failure = &one->failure))
    {
        out->at = start;
        mmgr_line_put(out, "{\"failure\":");
        EMBED_CALL(databases_failure_write, ServeDatabasesCtx, .failure = &one->failure, .out = out);
        mmgr_line_put(out, ",\"transaction\":");
        const char *const transaction = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = one->open);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = transaction,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = transaction, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, '}');
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &one->failing);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    return 1;
}

// the connection of the argument `name` taken, its place, or CLI_SERVE_DATABASES with why given
EMBED_INLINE unsigned int databases_get(const ServeDatabasesCtx *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    return (name != NULL) ? EMBED_CALL(databases_take, ServeDatabasesCtx, .name = name, .err_msg = err_msg)
                          : CLI_SERVE_DATABASES;
}

static int db_rows(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long from = 0ull;
    unsigned long long count = 0ull;
    const unsigned int at = EMBED_CALL(databases_get, ServeDatabasesCtx, .request = arguments, .err_msg = err_msg);
    if ((at == CLI_SERVE_DATABASES) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "from",
                    .value = &from, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "count",
                    .value = &count, .err_msg = err_msg))
    {
        if (at != CLI_SERVE_DATABASES)
        {
            EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
        }
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const int done = EMBED_CALL(cli_db_out_failure_open.rows, CliDbOutFailureOpenCfg, .open = one->open, .from = from,
                                .count_u64 = count, .out = out, .failure = &one->failure);
    if (!done)
    {
        EMBED_CALL(databases_failure_msg_write, ServeDatabasesCtx, .failure = &one->failure, .err_msg = err_msg);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &one->failing);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    return done;
}

static int db_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int at = EMBED_CALL(databases_get, ServeDatabasesCtx, .request = arguments, .err_msg = err_msg);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    const int done = EMBED_CALL(cli_db_err_msg.stop, CliDbErrMsgCfg, .open = s_database[at].open, .err_msg = err_msg);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    mmgr_line_put(args->out, done ? "null" : "");
    return done;
}

static int db_stop_reading(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int at = EMBED_CALL(databases_get, ServeDatabasesCtx, .request = arguments, .err_msg = err_msg);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    const int done =
        EMBED_CALL(cli_db_err_msg.stop_reading, CliDbErrMsgCfg, .open = s_database[at].open, .err_msg = err_msg);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    mmgr_line_put(args->out, done ? "null" : "");
    return done;
}

static int db_children(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_path[64];
    unsigned int count = 0u;
    const unsigned int at = EMBED_CALL(databases_get, ServeDatabasesCtx, .request = arguments, .err_msg = err_msg);
    if ((at == CLI_SERVE_DATABASES) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "path",
                    .items = s_path, .room = 64u, .count = &count, .err_msg = err_msg))
    {
        if (at != CLI_SERVE_DATABASES)
        {
            EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
        }
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const int done = EMBED_CALL(cli_db_out_count.children, CliDbOutCountCfg, .open = one->open, .path_pointer = s_path,
                                .count = count, .out = out, .failure = &one->failure);
    if (!done)
    {
        EMBED_CALL(databases_failure_msg_write, ServeDatabasesCtx, .failure = &one->failure, .err_msg = err_msg);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &one->failing);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    return done;
}

// how far along an export is reported to the window as it writes
static void databases_export(const CliEachWrittenCfg *args)
{
    char body[128];
    VerbaLinea line = {body, sizeof(body), 0ull};
    mmgr_line_put(&line, "{\"call\":\"db_export\",\"done\":");
    mmgr_line_decimal(&line, args->written, 1u);
    mmgr_line_put(&line, ",\"unit\":\"rows written\"}");
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "call-progress", .body = body,
               .size = line.at);
}

static int db_export(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char target[CLI_PATH_MAX_LEN];
    const char *const format = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                          .key = "format", .err_msg = err_msg);
    if ((format == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                        .request = arguments, .root = root, .full = target, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned int at = EMBED_CALL(databases_get, ServeDatabasesCtx, .request = arguments, .err_msg = err_msg);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const int done = EMBED_CALL(cli_db_out.export, CliDbOutCfg, .open = one->open, .path = target, .format = format,
                                .progress = databases_export, .context = NULL, .out = out, .failure = &one->failure);
    if (!done)
    {
        EMBED_CALL(databases_failure_msg_write, ServeDatabasesCtx, .failure = &one->failure, .err_msg = err_msg);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &one->failing);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    return done;
}

static int db_history(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int at = EMBED_CALL(databases_get, ServeDatabasesCtx, .request = arguments, .err_msg = err_msg);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    EMBED_CALL(cli_db_out_count.history, CliDbOutCountCfg, .open = s_database[at].open, .out = out);
    EMBED_CALL(databases_free, ServeDatabasesCtx, .at = at);
    return 1;
}

const CliServeCall g_cli_serve_db_calls[] = {
    {"db_list", db_list},
    {"db_save", db_save},
    {"db_remove", db_remove},
    {"db_forget_password", db_forget_password},
    {"db_connect", db_connect},
    {"db_close", db_close},
    {"db_run", db_run},
    {"db_rows", db_rows},
    {"db_stop", db_stop},
    {"db_stop_reading", db_stop_reading},
    {"db_children", db_children},
    {"db_export", db_export},
    {"db_history", db_history},
    {"db_statement_at", cli_serve_db_statement_at},
    {"db_statements", cli_serve_db_statements},
};
const unsigned int g_cli_serve_db_count = sizeof(g_cli_serve_db_calls) / sizeof(g_cli_serve_db_calls[0]);
