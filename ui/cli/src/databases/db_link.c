// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_link.c: one link to a database, PostgreSQL's, MySQL's or SQLite's, each implemented alike: opened, read-only where
// the connection matters and is guarded so, a statement started and its rows read, its transaction's state, what stops
// it, what the server noted, and the link ended
#include "../command_line/callback_cfg.h"
#include "db_internal.h"

typedef struct
{
    const char *path;
} DbLinkCtx;

// whether a path is whole, from a root or a drive, and not by its place in the tree
EMBED_INLINE int link_path_is_absolute(const DbLinkCtx *args)
{
    const int drive =
        (((args->path[0] >= 'A') && (args->path[0] <= 'Z')) || ((args->path[0] >= 'a') && (args->path[0] <= 'z'))) &&
        (args->path[1] == ':') && ((args->path[2] == '\\') || (args->path[2] == '/'));
    return (args->path[0] == '/') || (args->path[0] == '\\') || drive;
}

// a row passed over, as a setting's answer is
static int link_pass(const CliDbRowCfg *args)
{
    (void)args;
    return 1;
}

int cli_db_link_open(const CliDbLinkCtx *args)
{
    const int read_only =
        args->config->matters &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->config->guard, .other = "read-only", .cap = CLI_POOL_BYTES}));
    memor.set(&(MemoriaCfg){.dst = args->link, .val = 0, .bytes = sizeof(*args->link)});
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->config->kind, .other = "postgres", .cap = CLI_POOL_BYTES}))
    {
        static const char s_read_only[] = "SET SESSION CHARACTERISTICS AS TRANSACTION READ ONLY";
        const char *const database = (args->config->database[0] != '\0') ? args->config->database : args->config->user;
        args->link->pg = EMBED_CALL(cli_pg_host.connect, CliPgHostCfg, .host = args->host, .port = args->port,
                                    .user = args->config->user, .password = args->password, .database = database,
                                    .wait = CLI_DB_WAIT, .failure = args->failure);
        if (args->link->pg == NULL)
        {
            return 0;
        }
        args->link->kind = CLI_DB_LINK_PG;
        if (args->runs && read_only &&
            !EMBED_CALL(cli_pg_failure_sql.all, CliPgFailureSqlCfg, .pg = args->link->pg, .sql = s_read_only,
                        .size = sizeof(s_read_only) - 1u, .each_row = link_pass, .context = NULL,
                        .failure = args->failure))
        {
            cli_db_link_internal.link_close(args);
            return 0;
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->config->kind, .other = "mysql", .cap = CLI_POOL_BYTES}))
    {
        static const char s_read_only[] = "SET SESSION TRANSACTION READ ONLY";
        args->link->my = EMBED_CALL(cli_mysql_host.connect, CliMysqlHostCfg, .host = args->host, .port = args->port,
                                    .user = args->config->user, .password = args->password,
                                    .database = args->config->database, .wait = CLI_DB_WAIT, .failure = args->failure);
        if (args->link->my == NULL)
        {
            return 0;
        }
        args->link->kind = CLI_DB_LINK_MY;
        if (args->runs && read_only &&
            !EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = args->link->my,
                        .sql = s_read_only, .size = sizeof(s_read_only) - 1u, .each_row = link_pass, .context = NULL,
                        .failure = args->failure))
        {
            cli_db_link_internal.link_close(args);
            return 0;
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->config->kind, .other = "sqlite", .cap = CLI_POOL_BYTES}))
    {
        char file[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(link_path_is_absolute, DbLinkCtx, .path = args->config->file))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = file,
                .src = args->config->file,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->config->file, .cap = CLI_POOL_BYTES}) + 1u});
        }
        else
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->config->file,
                       .joined = file);
        }
        // the schema's link reads alone, and the one that runs the reader's statements reads alone where it is set so
        args->link->lite = EMBED_CALL(cli_db_failure_file_internal.lite_open, CliDbFailureFileCtx, .file = file,
                                      .read_only = (args->runs && read_only) || !args->runs, .failure = args->failure);
        args->link->kind = (args->link->lite != NULL) ? CLI_DB_LINK_LITE : CLI_DB_LINK_NONE;
        return args->link->lite != NULL;
    }
    char warned[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea line = {warned, sizeof(warned), 0ULL};
    mmgr_line_put(&line, args->config->kind);
    mmgr_line_put(&line, " is no kind of database orior_cli supports");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = warned);
    return 0;
}

int cli_db_link_start(const CliDbLinkFailureStatementCtx *args)
{
    if (args->link->kind == CLI_DB_LINK_PG)
    {
        return EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = args->link->pg, .sql = args->statement,
                          .size = args->size, .failure = args->failure);
    }
    if (args->link->kind == CLI_DB_LINK_MY)
    {
        return EMBED_CALL(cli_mysql_failure_sql.start, CliMysqlFailureSqlCfg, .mysql_out = args->link->my,
                          .sql = args->statement, .size = args->size, .failure = args->failure);
    }
    return EMBED_CALL(cli_db_failure_statement_internal.lite_start, CliDbFailureStatementCtx, .lite = args->link->lite,
                      .statement = args->statement, .size = args->size, .failure = args->failure);
}

const CliDbHead *cli_db_link_head(const CliDbCtx *args)
{
    if (args->link_read->kind == CLI_DB_LINK_PG)
    {
        return EMBED_CALL(cli_pg_cancel.head, CliPgCancelCfg, .pg_read = args->link_read->pg);
    }
    if (args->link_read->kind == CLI_DB_LINK_MY)
    {
        return EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = args->link_read->my);
    }
    return &args->link_read->lite->head;
}

int cli_db_link_rows(const CliDbLinkFailureCtx *args)
{
    if (args->link->kind == CLI_DB_LINK_PG)
    {
        return EMBED_CALL(cli_pg_failure.rows, CliPgFailureCfg, .pg = args->link->pg, .count = args->count,
                          .each_row = args->each_row, .context = args->context, .done = args->done, .tag = args->tag,
                          .failure = args->failure);
    }
    if (args->link->kind == CLI_DB_LINK_MY)
    {
        return EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = args->link->my, .count = args->count,
                          .each_row = args->each_row, .context = args->context, .done = args->done, .tag = args->tag,
                          .failure = args->failure);
    }
    return EMBED_CALL(cli_db_failure_internal.lite_rows, CliDbFailureCtx, .lite = args->link->lite,
                      .count = args->count, .each_row = args->each_row, .context = args->context, .done = args->done,
                      .tag = args->tag, .failure = args->failure);
}

int cli_db_link_all(const CliDbLinkFailureStatementCtx *args)
{
    char tag[CLI_DB_TAG];
    if (!cli_db_link_failure_statement_internal.link_start(args))
    {
        return 0;
    }
    for (int done = !EMBED_CALL(cli_db_internal.link_head, CliDbCtx, .link_read = args->link)->rows; !done;)
    {
        if (!EMBED_CALL(cli_db_link_failure_internal.link_rows, CliDbLinkFailureCtx, .link = args->link,
                        .count = 4096ULL, .each_row = args->each_row, .context = args->context, .done = &done,
                        .tag = tag, .failure = args->failure))
        {
            return 0;
        }
    }
    return 1;
}

const char *cli_db_link_transaction(const CliDbCtx *args)
{
    if (args->link_read->kind == CLI_DB_LINK_PG)
    {
        const char status = EMBED_CALL(cli_pg_cancel.status, CliPgCancelCfg, .pg_read = args->link_read->pg);
        if (status == 'T')
        {
            return "open";
        }
        if (status == 'E')
        {
            return "failed";
        }
        return "idle";
    }
    if (args->link_read->kind == CLI_DB_LINK_MY)
    {
        return EMBED_CALL(cli_mysql_cancel.in_transaction, CliMysqlCancelCfg, .mysql = args->link_read->my) ? "open"
                                                                                                            : "idle";
    }
    return args->link_read->lite->in_transaction ? "open" : "idle";
}

int cli_db_link_cancel_thread(const CliDbCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->stop, .val = 0, .bytes = sizeof(*args->stop)});
    args->stop->kind = args->link_read->kind;
    if (args->link_read->kind == CLI_DB_LINK_PG)
    {
        return EMBED_CALL(cli_pg_cancel.canceller, CliPgCancelCfg, .pg_read = args->link_read->pg,
                          .cancel = &args->stop->pg);
    }
    if (args->link_read->kind == CLI_DB_LINK_MY)
    {
        EMBED_CALL(cli_mysql_cancel.canceller, CliMysqlCancelCfg, .mysql = args->link_read->my,
                   .cancel = &args->stop->my);
        return 1;
    }
    args->stop->lite = args->link_read->lite;
    return args->link_read->kind == CLI_DB_LINK_LITE;
}

int cli_db_stop_send(const CliDbStopCtx *args)
{
    if (args->stop_read->kind == CLI_DB_LINK_PG)
    {
        return EMBED_CALL(cli_pg.cancel_send, CliPgCfg, .cancel_read = &args->stop_read->pg, .err_msg = args->err_msg);
    }
    if (args->stop_read->kind == CLI_DB_LINK_MY)
    {
        return EMBED_CALL(cli_mysql.cancel_send, CliMysqlCfg, .cancel_read = &args->stop_read->my,
                          .err_msg = args->err_msg);
    }
    return EMBED_CALL(cli_db_err_msg_internal.lite_interrupt, CliDbErrMsgCtx, .lite = args->stop_read->lite,
                      .err_msg = args->err_msg);
}

// The warnings MySQL gave, written each a row's values joined by spaces, and how many there are
typedef struct
{
    void (*each)(const CliEachNoticeCfg *args);
    void *context;
    unsigned int warned;
} CliDbLinkWarnings;

static int link_warning(const CliDbRowCfg *args)
{
    CliDbLinkWarnings *const warnings = (CliDbLinkWarnings *)args->context;
    char notice[CLI_DB_FIELD];
    VerbaLinea line = {notice, sizeof(notice), 0ULL};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (!args->values[at].null)
        {
            mmgr_line_put(&line, (line.at > 0ULL) ? " " : "");
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
                       .text_bytes = (const unsigned char *)args->values[at].text, .length = args->values[at].size);
        }
    }
    mmgr_line_finish(&line);
    EMBED_CALL(warnings->each, CliEachNoticeCfg, .context = warnings->context, .notice = notice);
    warnings->warned += 1u;
    return 1;
}

void cli_db_link_notices(const CliDbLinkContextCtx *args)
{
    void *context = args->context;
    if (args->link->kind == CLI_DB_LINK_PG)
    {
        for (unsigned int at = 0u;
             EMBED_CALL(cli_pg_at.notice, CliPgAtCfg, .pg_read = args->link->pg, .at = at) != NULL; at += 1u)
        {
            EMBED_CALL(args->each, CliEachNoticeCfg, .context = context,
                       .notice = EMBED_CALL(cli_pg_at.notice, CliPgAtCfg, .pg_read = args->link->pg, .at = at));
        }
        return;
    }
    if ((args->link->kind != CLI_DB_LINK_MY) ||
        (EMBED_CALL(cli_mysql_cancel.warnings, CliMysqlCancelCfg, .mysql = args->link->my) == 0u))
    {
        return;
    }
    static const char s_show[] = "SHOW WARNINGS";
    const unsigned int count = EMBED_CALL(cli_mysql_cancel.warnings, CliMysqlCancelCfg, .mysql = args->link->my);
    CliDbLinkWarnings warnings = {args->each, context, 0u};
    CliDbFailure failure;
    (void)EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = args->link->my, .sql = s_show,
                     .size = sizeof(s_show) - 1u, .each_row = link_warning, .context = &warnings, .failure = &failure);
    if (warnings.warned == 0u)
    {
        char notice[64];
        VerbaLinea line = {notice, sizeof(notice), 0ULL};
        mmgr_line_decimal(&line, count, 1u);
        mmgr_line_put(&line, (count == 1u) ? " warning" : " warnings");
        mmgr_line_finish(&line);
        EMBED_CALL(args->each, CliEachNoticeCfg, .context = context, .notice = notice);
    }
}

void cli_db_link_close(const CliDbLinkCtx *args)
{
    if (args->link->kind == CLI_DB_LINK_PG)
    {
        EMBED_CALL(cli_pg_failure_sql.close, CliPgFailureSqlCfg, .pg = args->link->pg);
    }
    else if (args->link->kind == CLI_DB_LINK_MY)
    {
        EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = args->link->my);
    }
    else if (args->link->kind == CLI_DB_LINK_LITE)
    {
        EMBED_CALL(cli_db_err_msg_internal.lite_close, CliDbErrMsgCtx, .lite = args->link->lite);
    }
    memor.set(&(MemoriaCfg){.dst = args->link, .val = 0, .bytes = sizeof(*args->link)});
}
