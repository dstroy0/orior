// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_open.c: a connection opened as two links, the one that runs the reader's statements and the one that reads the
// schema, through an ssh tunnel where it goes through one; each link acquired for its work and made again, tunnel and
// all, where its connection broke; the session's zone; and the statement each link runs stopped from another thread.
// Each connection is a cell taken as it opens and given back as it closes, its place here empty while none is open in
// it. A stop is sent under the stop lock, and a link's stop is let go under it before the link closes, so no stop
// reaches a link closed
#include "db_internal.h"

static CliDbOpen *s_opens[CLI_DB_OPENS];

typedef struct
{
    CliDbOpen *open;
    CliDbLink *link;
    int runs;
    CliDbFailure *failure;
    CliDbStop *stop;
    int *has_stop;
    int (*work)(const CliEachLinkFailureCfg *args);
    void *context;
} DbOpenCtx;
// a link to the database made, through the tunnel where the connection goes through ssh: 1, or 0 with the failure
// given
EMBED_INLINE int open_link(const DbOpenCtx *args)
{
    CliDbLink *link = args->link;
    int runs = args->runs;
    CliDbFailure *failure = args->failure;
    const CliDbConnection *const config = &args->open->config;
    const char *const host = (config->host[0] != '\0') ? config->host : "127.0.0.1";
    if ((config->ssh[0] == '\0') ||
        (cellul.eq(&(CatenaFinitaCfg){.src = config->kind, .other = "sqlite", .cap = CLI_CELLS_POOL})))
    {
        return EMBED_CALL(
            cli_db_link_internal.link_open, CliDbLinkCtx, .link = link, .root = args->open->root, .config = config,
            .password = args->open->password, .host = host,
            .port = EMBED_CALL(cli_db_connection_out_kind.port, CliDbConnectionOutKindCfg, .connection = config),
            .runs = runs, .failure = failure);
    }
    if (!EMBED_CALL(cli_db_host_internal.tunnel_alive, CliDbHostCtx, .tunnel = &args->open->tunnel))
    {
        EMBED_CALL(cli_db_host_internal.tunnel_close, CliDbHostCtx, .tunnel = &args->open->tunnel);
        if (!EMBED_CALL(
                cli_db_host_internal.tunnel_open, CliDbHostCtx, .tunnel = &args->open->tunnel, .through = config->ssh,
                .host = host,
                .port = EMBED_CALL(cli_db_connection_out_kind.port, CliDbConnectionOutKindCfg, .connection = config),
                .failure = failure))
        {
            return 0;
        }
    }
    const unsigned int port = args->open->tunnel.port;
    return EMBED_CALL(cli_db_link_internal.link_open, CliDbLinkCtx, .link = link, .root = args->open->root,
                      .config = config, .password = args->open->password, .host = "127.0.0.1", .port = port,
                      .runs = runs, .failure = failure);
}

// the first value of the first row a question gives, retained
static int open_first(const CliDbRowCfg *args)
{
    VerbaLinea *const out = (VerbaLinea *)args->context;
    if ((out->at == 0ULL) && (args->count > 0u) && !args->values[0].null)
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->values[0].text, .length = args->values[0].size);
    }
    return 1;
}

void cli_db_zone_read(const CliDbFailureContextCtx *args)
{
    static const char s_zone[] = "SELECT IF(@@session.time_zone = 'SYSTEM', @@system_time_zone, @@session.time_zone)";
    char zone[CLI_NAME_MAX_LEN];
    VerbaLinea out = {zone, sizeof(zone) - 1u, 0ULL};
    if (args->open->query.kind == CLI_DB_LINK_PG)
    {
        const char *const tz_name =
            EMBED_CALL(cli_pg_name.parameter, CliPgNameCfg, .pg_read = args->open->query.pg, .name = "TimeZone");
        mmgr_line_put(&out, (tz_name != NULL) ? tz_name : "");
    }
    else if (args->open->query.kind == CLI_DB_LINK_MY)
    {
        CliDbFailure failure;
        (void)EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = args->open->query.my,
                         .sql = s_zone, .size = sizeof(s_zone) - 1u, .each_row = open_first, .context = &out,
                         .failure = &failure);
    }
    zone[out.at] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = args->open->zone,
                            .src = zone,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = zone, .cap = CLI_CELLS_POOL}) + 1u});
}

CliDbOpen *cli_db_connect(const CliDbRootCfg *args)
{
    const CliDbConnection *config = args->config;
    CliDbOpen *open = NULL;
    unsigned int entry_at = 0u;
    while ((entry_at < CLI_DB_OPENS) && (s_opens[entry_at] != NULL))
    {
        entry_at += 1u;
    }
    if (entry_at < CLI_DB_OPENS)
    {
        open = g_cli_cells->persistent_buf_alloc(sizeof(CliDbOpen), &g_cli_db_cells);
        s_opens[entry_at] = open;
    }
    if (open == NULL)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = (entry_at < CLI_DB_OPENS) ? "no capacity is left to open a database connection in"
                                                        : "orior_cli has as many database connections open as it can");
        return NULL;
    }
    open->config = *config;
    memor.cpy(&(MemoriaCfg){.dst = open->root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    if (args->password != NULL)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = open->password,
                          .src = args->password,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        char account[CLI_PATH_MAX_LEN];
        VerbaLinea account_line = {account, sizeof(account), 0ULL};
        EMBED_CALL(cli_db_connection_out_kind.account, CliDbConnectionOutKindCfg, .connection = config,
                   .out = &account_line);
        mmgr_line_finish(&account_line);
        if (!EMBED_CALL(cli_service_internal.keychain_read, CliServiceCtx, .service = "database", .account = account,
                        .out = open->password, .capacity = sizeof(open->password)))
        {
            open->password[0] = '\0';
        }
    }
    open->busy = 0;
    open->zone[0] = '\0';
    memor.set(&(MemoriaCfg){.dst = &open->tunnel, .val = 0, .bytes = sizeof(open->tunnel)});
    memor.set(&(MemoriaCfg){.dst = &open->results, .val = 0, .bytes = sizeof(open->results)});
    if (!EMBED_CALL(open_link, DbOpenCtx, .open = open, .link = &open->query, .runs = 1, .failure = args->failure))
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open);
        return NULL;
    }
    if (!EMBED_CALL(open_link, DbOpenCtx, .open = open, .link = &open->meta, .runs = 0, .failure = args->failure))
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open);
        return NULL;
    }
    const char *const version =
        (open->query.kind == CLI_DB_LINK_PG)
            ? EMBED_CALL(cli_pg_name.parameter, CliPgNameCfg, .pg_read = open->query.pg, .name = "server_version")
        : (open->query.kind == CLI_DB_LINK_MY)
            ? EMBED_CALL(cli_mysql_cancel.version, CliMysqlCancelCfg, .mysql = open->query.my)
            : open->query.lite->version;
    (void)cellul.copy(&(CatenaFinitaCfg){
        .dst = open->version, .src = (version != NULL) ? version : "", .cap = sizeof(open->version)});
    open->has_stop =
        EMBED_CALL(cli_db_internal.link_cancel_thread, CliDbCtx, .link_read = &open->query, .stop = &open->stop);
    open->has_meta_stop =
        EMBED_CALL(cli_db_internal.link_cancel_thread, CliDbCtx, .link_read = &open->meta, .stop = &open->meta_stop);
    EMBED_CALL(cli_db_failure_context_internal.zone_read, CliDbFailureContextCtx, .open = open);
    return open;
}

void cli_db_close(const CliDbErrMsgCfg *args)
{
    // no stop reaches the links once they close: one sent finishes first, and none is sent after
    args->open->has_stop = 0;
    args->open->has_meta_stop = 0;
    EMBED_CALL(cli_db_link_internal.link_close, CliDbLinkCtx, .link = &args->open->query);
    EMBED_CALL(cli_db_link_internal.link_close, CliDbLinkCtx, .link = &args->open->meta);
    EMBED_CALL(cli_db_host_internal.tunnel_close, CliDbHostCtx, .tunnel = &args->open->tunnel);
    if (args->open->results.path[0] != '\0')
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = args->open->results.path);
    }
    for (unsigned int entry_at = 0u; entry_at < CLI_DB_OPENS; entry_at += 1u)
    {
        s_opens[entry_at] = (s_opens[entry_at] == args->open) ? NULL : s_opens[entry_at];
    }
    // the connection's password lies in its cell, which the pool leaves as it is
    mmgr_zero_buf(args->open, sizeof(*args->open));
    g_cli_cells->persistent_buf_release(args->open, &g_cli_db_cells);
}

// a link acquired and handed to `work`, made first where it is none, and let go where its connection broke, the next
// statement connecting again
static int open_with(const DbOpenCtx *args)
{
    CliDbLink *link = args->link;
    CliDbStop *stop = args->stop;
    int *has_stop = args->has_stop;
    if (link->kind == CLI_DB_LINK_NONE)
    {
        if (!EMBED_CALL(open_link, DbOpenCtx, .open = args->open, .link = link, .runs = args->runs,
                        .failure = args->failure))
        {
            return 0;
        }
        *has_stop = EMBED_CALL(cli_db_internal.link_cancel_thread, CliDbCtx, .link_read = link, .stop = stop);
    }
    args->open->busy += args->runs ? 1 : 0;
    const int done =
        EMBED_CALL(args->work, CliEachLinkFailureCfg, .context = args->context, .link = link, .failure = args->failure);
    args->open->busy -= args->runs ? 1 : 0;
    if (!done && (cellul.starts(&(CatenaFinitaCfg){
                     .src = args->failure->message, .other = "the connection broke", .cap = CLI_CELLS_POOL})))
    {
        // the link's stop let go before it closes: no stop reaches it closed
        *has_stop = 0;
        EMBED_CALL(cli_db_link_internal.link_close, CliDbLinkCtx, .link = link);
        if (args->runs)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->failure->hint,
                                    .src = "The next statement connects again.",
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = "The next statement connects again.",
                                                                           .cap = CLI_CELLS_POOL}) +
                                             1u});
        }
    }
    return done;
}

int cli_db_with_query(const CliDbFailureContextCtx *args)
{
    CliDbOpen *open = args->open;
    int (*work)(const CliEachLinkFailureCfg *) = args->work;
    void *context = args->context;
    CliDbFailure *failure = args->failure;
    return EMBED_CALL(open_with, DbOpenCtx, .open = open, .link = &open->query, .stop = &open->stop,
                      .has_stop = &open->has_stop, .runs = 1, .work = work, .context = context, .failure = failure);
}

int cli_db_with_meta(const CliDbFailureContextCtx *args)
{
    CliDbOpen *open = args->open;
    int (*work)(const CliEachLinkFailureCfg *) = args->work;
    void *context = args->context;
    CliDbFailure *failure = args->failure;
    return EMBED_CALL(open_with, DbOpenCtx, .open = open, .link = &open->meta, .stop = &open->meta_stop,
                      .has_stop = &open->has_meta_stop, .runs = 0, .work = work, .context = context,
                      .failure = failure);
}

const char *cli_db_name(const CliDbCfg *args)
{
    return args->open_read->config.name;
}

const char *cli_db_version(const CliDbCfg *args)
{
    return args->open_read->version;
}

const char *cli_db_transaction(const CliDbErrMsgCfg *args)
{
    const int busy = (args->open->busy > 0);
    if (busy)
    {
        return "busy";
    }
    const char *const state =
        (args->open->query.kind == CLI_DB_LINK_NONE)
            ? "busy"
            : EMBED_CALL(cli_db_internal.link_transaction, CliDbCtx, .link_read = &args->open->query);
    return state;
}

void cli_db_zone(const CliDbOutCountCfg *args)
{
    mmgr_line_put(args->out, args->open->zone);
}

int cli_db_stop(const CliDbErrMsgCfg *args)
{
    const int has = args->open->has_stop;
    const int sent = has && EMBED_CALL(cli_db_stop_internal.stop_send, CliDbStopCtx, .stop_read = &args->open->stop,
                                       .err_msg = args->err_msg);
    if (!has)
    {
        mmgr_line_put(args->err_msg, "nothing runs");
    }
    return sent;
}

int cli_db_stop_read_state(const CliDbErrMsgCfg *args)
{
    const int has = args->open->has_meta_stop;
    const int sent = has && EMBED_CALL(cli_db_stop_internal.stop_send, CliDbStopCtx,
                                       .stop_read = &args->open->meta_stop, .err_msg = args->err_msg);
    if (!has)
    {
        mmgr_line_put(args->err_msg, "nothing is read");
    }
    return sent;
}
