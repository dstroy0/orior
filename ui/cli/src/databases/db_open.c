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
static CliLock s_opens_lock = CLI_LOCK_INIT;

// a link to the database made, through the tunnel where the connection goes through ssh: 1, or 0 with the failure
// given
static int open_link(CliDbOpen *open, CliDbLink *link, int runs, CliDbFailure *failure)
{
    const CliDbConnection *const config = &open->config;
    const char *const host = (config->host[0] != '\0') ? config->host : "127.0.0.1";
    if ((config->ssh[0] == '\0') ||
        (cellul.eq(&(CatenaFinitaCfg){.src = config->kind, .other = "sqlite", .cap = CLI_CELLS_POOL})))
    {
        return cli_db_link_open(link, open->root, config, open->password, host, cli_db_port(config), runs, failure);
    }
    cli_lock_take(&open->tunnel_lock);
    if (!cli_db_tunnel_alive(&open->tunnel))
    {
        cli_db_tunnel_closed(&open->tunnel);
        if (!cli_db_tunnel_open(&open->tunnel, config->ssh, host, cli_db_port(config), failure))
        {
            cli_lock_give(&open->tunnel_lock);
            return 0;
        }
    }
    const unsigned int port = open->tunnel.port;
    cli_lock_give(&open->tunnel_lock);
    return cli_db_link_open(link, open->root, config, open->password, "127.0.0.1", port, runs, failure);
}

// the first value of the first row a question gives, retained
static int open_first(void *context, const CliDbValue *values, unsigned int count)
{
    VerbaLinea *const out = (VerbaLinea *)context;
    if ((out->at == 0ull) && (count > 0u) && !values[0].null)
    {
        cli_lossy(out, (const unsigned char *)values[0].text, values[0].size);
    }
    return 1;
}

void cli_db_zone_read(CliDbOpen *open)
{
    static const char s_zone[] = "SELECT IF(@@session.time_zone = 'SYSTEM', @@system_time_zone, @@session.time_zone)";
    char zone[CLI_NAME_MAX_LEN];
    VerbaLinea out = {zone, sizeof(zone) - 1u, 0ull};
    cli_lock_take(&open->query_lock);
    if (open->query.kind == CLI_DB_LINK_PG)
    {
        const char *const tz_name = cli_pg_parameter(open->query.pg, "TimeZone");
        mmgr_line_put(&out, (tz_name != NULL) ? tz_name : "");
    }
    else if (open->query.kind == CLI_DB_LINK_MY)
    {
        CliDbFailure failure;
        (void)cli_mysql_all(open->query.my, s_zone, sizeof(s_zone) - 1u, open_first, &out, &failure);
    }
    cli_lock_give(&open->query_lock);
    zone[out.at] = '\0';
    cli_lock_take(&open->zone_lock);
    memor.cpy(&(MemoriaCfg){.dst = open->zone,
                            .src = zone,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = zone, .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&open->zone_lock);
}

CliDbOpen *cli_db_connect(const char *root, const CliDbConnection *config, const char *password, CliDbFailure *failure)
{
    CliDbOpen *open = NULL;
    cli_lock_take(&s_opens_lock);
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
    cli_lock_give(&s_opens_lock);
    if (open == NULL)
    {
        cli_db_failure_set(failure, (entry_at < CLI_DB_OPENS)
                                        ? "no room is left to open a database connection in"
                                        : "orior_cli has as many database connections open as it can");
        return NULL;
    }
    open->config = *config;
    memor.cpy(&(MemoriaCfg){.dst = open->root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    if (password != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = open->password,
                                .src = password,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        char account[CLI_PATH_MAX_LEN];
        VerbaLinea account_line = {account, sizeof(account), 0ull};
        cli_db_account(config, &account_line);
        mmgr_line_finish(&account_line);
        if (!cli_keychain_read("database", account, open->password, sizeof(open->password)))
        {
            open->password[0] = '\0';
        }
    }
    open->query_lock = (CliLock)CLI_LOCK_INIT;
    open->meta_lock = (CliLock)CLI_LOCK_INIT;
    open->stop_lock = (CliLock)CLI_LOCK_INIT;
    open->tunnel_lock = (CliLock)CLI_LOCK_INIT;
    open->results_lock = (CliLock)CLI_LOCK_INIT;
    open->zone_lock = (CliLock)CLI_LOCK_INIT;
    open->busy = 0;
    open->zone[0] = '\0';
    memor.set(&(MemoriaCfg){.dst = &open->tunnel, .val = 0, .bytes = sizeof(open->tunnel)});
    memor.set(&(MemoriaCfg){.dst = &open->results, .val = 0, .bytes = sizeof(open->results)});
    if (!open_link(open, &open->query, 1, failure))
    {
        cli_db_close(open);
        return NULL;
    }
    if (!open_link(open, &open->meta, 0, failure))
    {
        cli_db_close(open);
        return NULL;
    }
    const char *const version = (open->query.kind == CLI_DB_LINK_PG)
                                    ? cli_pg_parameter(open->query.pg, "server_version")
                                : (open->query.kind == CLI_DB_LINK_MY) ? cli_mysql_version(open->query.my)
                                                                       : open->query.lite->version;
    (void)cellul.copy(&(CatenaFinitaCfg){
        .dst = open->version, .src = (version != NULL) ? version : "", .cap = sizeof(open->version)});
    open->has_stop = cli_db_link_stopper(&open->query, &open->stop);
    open->has_meta_stop = cli_db_link_stopper(&open->meta, &open->meta_stop);
    cli_db_zone_read(open);
    return open;
}

void cli_db_close(CliDbOpen *open)
{
    // no stop reaches the links once they close: one sent finishes first, and none is sent after
    cli_lock_take(&open->stop_lock);
    open->has_stop = 0;
    open->has_meta_stop = 0;
    cli_lock_give(&open->stop_lock);
    cli_lock_take(&open->query_lock);
    cli_db_link_closed(&open->query);
    cli_lock_give(&open->query_lock);
    cli_lock_take(&open->meta_lock);
    cli_db_link_closed(&open->meta);
    cli_lock_give(&open->meta_lock);
    cli_lock_take(&open->tunnel_lock);
    cli_db_tunnel_closed(&open->tunnel);
    cli_lock_give(&open->tunnel_lock);
    if (open->results.path[0] != '\0')
    {
        (void)query_removed(open->results.path);
    }
    cli_lock_take(&s_opens_lock);
    for (unsigned int entry_at = 0u; entry_at < CLI_DB_OPENS; entry_at += 1u)
    {
        s_opens[entry_at] = (s_opens[entry_at] == open) ? NULL : s_opens[entry_at];
    }
    cli_lock_give(&s_opens_lock);
    // the connection's password lies in its cell, which the pool leaves as it is
    mmgr_zero_buf(open, sizeof(*open));
    g_cli_cells->persistent_buf_release(open, &g_cli_db_cells);
}

// a link acquired and handed to `work`, made first where it is none, and let go where its connection broke, the next
// statement connecting again
static int open_with(CliDbOpen *open, CliDbLink *link, CliLock *lock, CliDbStop *stop, int *has_stop, int runs,
                     int (*work)(void *context, CliDbLink *link, CliDbFailure *failure), void *context,
                     CliDbFailure *failure)
{
    cli_lock_take(lock);
    if (link->kind == CLI_DB_LINK_NONE)
    {
        if (!open_link(open, link, runs, failure))
        {
            cli_lock_give(lock);
            return 0;
        }
        cli_lock_take(&open->stop_lock);
        *has_stop = cli_db_link_stopper(link, stop);
        cli_lock_give(&open->stop_lock);
    }
    cli_lock_take(&open->stop_lock);
    open->busy += runs ? 1 : 0;
    cli_lock_give(&open->stop_lock);
    const int done = work(context, link, failure);
    cli_lock_take(&open->stop_lock);
    open->busy -= runs ? 1 : 0;
    cli_lock_give(&open->stop_lock);
    if (!done && (cellul.starts(&(CatenaFinitaCfg){
                     .src = failure->message, .other = "the connection broke", .cap = CLI_CELLS_POOL})))
    {
        // the link's stop let go before it closes: no stop reaches it closed
        cli_lock_take(&open->stop_lock);
        *has_stop = 0;
        cli_lock_give(&open->stop_lock);
        cli_db_link_closed(link);
        if (runs)
        {
            memor.cpy(&(MemoriaCfg){.dst = failure->hint,
                                    .src = "The next statement connects again.",
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = "The next statement connects again.",
                                                                           .cap = CLI_CELLS_POOL}) +
                                             1u});
        }
    }
    cli_lock_give(lock);
    return done;
}

int cli_db_with_query(CliDbOpen *open, int (*work)(void *context, CliDbLink *link, CliDbFailure *failure),
                      void *context, CliDbFailure *failure)
{
    return open_with(open, &open->query, &open->query_lock, &open->stop, &open->has_stop, 1, work, context, failure);
}

int cli_db_with_meta(CliDbOpen *open, int (*work)(void *context, CliDbLink *link, CliDbFailure *failure), void *context,
                     CliDbFailure *failure)
{
    return open_with(open, &open->meta, &open->meta_lock, &open->meta_stop, &open->has_meta_stop, 0, work, context,
                     failure);
}

const char *cli_db_name(const CliDbOpen *open)
{
    return open->config.name;
}

const char *cli_db_version(const CliDbOpen *open)
{
    return open->version;
}

const char *cli_db_transaction(CliDbOpen *open)
{
    cli_lock_take(&open->stop_lock);
    const int busy = (open->busy > 0);
    cli_lock_give(&open->stop_lock);
    if (busy)
    {
        return "busy";
    }
    cli_lock_take(&open->query_lock);
    const char *const state = (open->query.kind == CLI_DB_LINK_NONE) ? "busy" : cli_db_link_transaction(&open->query);
    cli_lock_give(&open->query_lock);
    return state;
}

void cli_db_zone(CliDbOpen *open, VerbaLinea *out)
{
    cli_lock_take(&open->zone_lock);
    mmgr_line_put(out, open->zone);
    cli_lock_give(&open->zone_lock);
}

int cli_db_stop(CliDbOpen *open, VerbaLinea *err_msg)
{
    cli_lock_take(&open->stop_lock);
    const int has = open->has_stop;
    const int sent = has && cli_db_stop_sent(&open->stop, err_msg);
    cli_lock_give(&open->stop_lock);
    if (!has)
    {
        mmgr_line_put(err_msg, "nothing runs");
    }
    return sent;
}

int cli_db_stop_reading(CliDbOpen *open, VerbaLinea *err_msg)
{
    cli_lock_take(&open->stop_lock);
    const int has = open->has_meta_stop;
    const int sent = has && cli_db_stop_sent(&open->meta_stop, err_msg);
    cli_lock_give(&open->stop_lock);
    if (!has)
    {
        mmgr_line_put(err_msg, "nothing is read");
    }
    return sent;
}
