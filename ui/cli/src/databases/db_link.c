// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_link.c: one link to a database, PostgreSQL's, MySQL's or SQLite's, each spoken alike: opened, read-only where the
// connection matters and is guarded so, a statement started and its rows read, its transaction's state, what stops it,
// what the server noted, and the link ended
#include "db_internal.h"

// whether a path is whole, from a root or a drive, and not by its place in the tree
static int link_whole(const char *path)
{
    const int drive = (((path[0] >= 'A') && (path[0] <= 'Z')) || ((path[0] >= 'a') && (path[0] <= 'z'))) &&
                      (path[1] == ':') && ((path[2] == '\\') || (path[2] == '/'));
    return (path[0] == '/') || (path[0] == '\\') || drive;
}

// a row passed over, as a setting's answer is
static int link_passed(void *context, const CliDbValue *values, unsigned int count)
{
    (void)context;
    (void)values;
    (void)count;
    return 1;
}

int cli_db_link_open(CliDbLink *link, const char *root, const CliDbConnection *config, const char *password,
                     const char *host, unsigned int port, int runs, CliDbFailure *failure)
{
    const int read_only =
        config->matters &&
        (cellul.eq(&(CatenaFinitaCfg){.src = config->guard, .other = "read-only", .cap = CLI_CELLS_POOL}));
    memor.set(&(MemoriaCfg){.dst = link, .val = 0, .bytes = sizeof(*link)});
    if (cellul.eq(&(CatenaFinitaCfg){.src = config->kind, .other = "postgres", .cap = CLI_CELLS_POOL}))
    {
        static const char s_read_only[] = "SET SESSION CHARACTERISTICS AS TRANSACTION READ ONLY";
        const char *const database = (config->database[0] != '\0') ? config->database : config->user;
        link->pg = cli_pg_connected(host, port, config->user, password, database, CLI_DB_WAIT, failure);
        if (link->pg == NULL)
        {
            return 0;
        }
        link->kind = CLI_DB_LINK_PG;
        if (runs && read_only &&
            !cli_pg_all(link->pg, s_read_only, sizeof(s_read_only) - 1u, link_passed, NULL, failure))
        {
            cli_db_link_closed(link);
            return 0;
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = config->kind, .other = "mysql", .cap = CLI_CELLS_POOL}))
    {
        static const char s_read_only[] = "SET SESSION TRANSACTION READ ONLY";
        link->my = cli_mysql_connected(host, port, config->user, password, config->database, CLI_DB_WAIT, failure);
        if (link->my == NULL)
        {
            return 0;
        }
        link->kind = CLI_DB_LINK_MY;
        if (runs && read_only &&
            !cli_mysql_all(link->my, s_read_only, sizeof(s_read_only) - 1u, link_passed, NULL, failure))
        {
            cli_db_link_closed(link);
            return 0;
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = config->kind, .other = "sqlite", .cap = CLI_CELLS_POOL}))
    {
        char file[CLI_PATH_LONGEST];
        if (link_whole(config->file))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = file,
                .src = config->file,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = config->file, .cap = CLI_CELLS_POOL}) + 1u});
        }
        else
        {
            cli_full(root, config->file, file);
        }
        // the schema's link reads alone, and the one that runs the reader's statements reads alone where it is set so
        link->lite = cli_db_lite_open(file, (runs && read_only) || !runs, failure);
        link->kind = (link->lite != NULL) ? CLI_DB_LINK_LITE : CLI_DB_LINK_NONE;
        return link->lite != NULL;
    }
    char said[CLI_NAME_LONGEST + 64u];
    CliLine line = {said, sizeof(said), 0ull};
    cli_line_text(&line, config->kind);
    cli_line_text(&line, " is no kind of database orior speaks");
    cli_line_closed(&line);
    cli_db_failure_said(failure, said);
    return 0;
}

int cli_db_link_start(CliDbLink *link, const char *statement, unsigned long long size, CliDbFailure *failure)
{
    if (link->kind == CLI_DB_LINK_PG)
    {
        return cli_pg_start(link->pg, statement, size, failure);
    }
    if (link->kind == CLI_DB_LINK_MY)
    {
        return cli_mysql_start(link->my, statement, size, failure);
    }
    return cli_db_lite_start(link->lite, statement, size, failure);
}

const CliDbHead *cli_db_link_head(const CliDbLink *link)
{
    if (link->kind == CLI_DB_LINK_PG)
    {
        return cli_pg_head(link->pg);
    }
    if (link->kind == CLI_DB_LINK_MY)
    {
        return cli_mysql_head(link->my);
    }
    return &link->lite->head;
}

int cli_db_link_rows(CliDbLink *link, unsigned long long count, CliDbRowTold told, void *context, int *done, char *tag,
                     CliDbFailure *failure)
{
    if (link->kind == CLI_DB_LINK_PG)
    {
        return cli_pg_rows(link->pg, count, told, context, done, tag, failure);
    }
    if (link->kind == CLI_DB_LINK_MY)
    {
        return cli_mysql_rows(link->my, count, told, context, done, tag, failure);
    }
    return cli_db_lite_rows(link->lite, count, told, context, done, tag, failure);
}

int cli_db_link_all(CliDbLink *link, const char *statement, unsigned long long size, CliDbRowTold told, void *context,
                    CliDbFailure *failure)
{
    char tag[CLI_DB_TAG];
    if (!cli_db_link_start(link, statement, size, failure))
    {
        return 0;
    }
    for (int done = !cli_db_link_head(link)->rows; !done;)
    {
        if (!cli_db_link_rows(link, 4096ull, told, context, &done, tag, failure))
        {
            return 0;
        }
    }
    return 1;
}

const char *cli_db_link_transaction(const CliDbLink *link)
{
    if (link->kind == CLI_DB_LINK_PG)
    {
        const char status = cli_pg_status(link->pg);
        return (status == 'T') ? "open" : (status == 'E') ? "failed" : "idle";
    }
    if (link->kind == CLI_DB_LINK_MY)
    {
        return cli_mysql_in_transaction(link->my) ? "open" : "idle";
    }
    return link->lite->in_transaction ? "open" : "idle";
}

int cli_db_link_stopper(const CliDbLink *link, CliDbStop *stop)
{
    memor.set(&(MemoriaCfg){.dst = stop, .val = 0, .bytes = sizeof(*stop)});
    stop->kind = link->kind;
    if (link->kind == CLI_DB_LINK_PG)
    {
        return cli_pg_canceller(link->pg, &stop->pg);
    }
    if (link->kind == CLI_DB_LINK_MY)
    {
        cli_mysql_canceller(link->my, &stop->my);
        return 1;
    }
    stop->lite = link->lite;
    return link->kind == CLI_DB_LINK_LITE;
}

int cli_db_stop_sent(const CliDbStop *stop, CliLine *said)
{
    if (stop->kind == CLI_DB_LINK_PG)
    {
        return cli_pg_cancel_sent(&stop->pg, said);
    }
    if (stop->kind == CLI_DB_LINK_MY)
    {
        return cli_mysql_cancel_sent(&stop->my, said);
    }
    return cli_db_lite_interrupted(stop->lite, said);
}

// The warnings MySQL gave, written each a row's values joined by spaces, and how many there are
typedef struct
{
    void (*each)(void *context, const char *notice);
    void *context;
    unsigned int said;
} CliDbLinkWarnings;

static int link_warning(void *context, const CliDbValue *values, unsigned int count)
{
    CliDbLinkWarnings *const warnings = (CliDbLinkWarnings *)context;
    char notice[CLI_DB_FIELD];
    CliLine line = {notice, sizeof(notice), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!values[at].null)
        {
            cli_line_text(&line, (line.at > 0ull) ? " " : "");
            cli_lossy(&line, (const unsigned char *)values[at].text, values[at].size);
        }
    }
    cli_line_closed(&line);
    warnings->each(warnings->context, notice);
    warnings->said += 1u;
    return 1;
}

void cli_db_link_notices(CliDbLink *link, void (*each)(void *context, const char *notice), void *context)
{
    if (link->kind == CLI_DB_LINK_PG)
    {
        for (unsigned int at = 0u; cli_pg_notice(link->pg, at) != NULL; at += 1u)
        {
            each(context, cli_pg_notice(link->pg, at));
        }
        return;
    }
    if ((link->kind != CLI_DB_LINK_MY) || (cli_mysql_warnings(link->my) == 0u))
    {
        return;
    }
    static const char s_show[] = "SHOW WARNINGS";
    const unsigned int count = cli_mysql_warnings(link->my);
    CliDbLinkWarnings warnings = {each, context, 0u};
    CliDbFailure failure;
    (void)cli_mysql_all(link->my, s_show, sizeof(s_show) - 1u, link_warning, &warnings, &failure);
    if (warnings.said == 0u)
    {
        char notice[64];
        CliLine line = {notice, sizeof(notice), 0ull};
        cli_line_decimal(&line, count, 1u);
        cli_line_text(&line, (count == 1u) ? " warning" : " warnings");
        cli_line_closed(&line);
        each(context, notice);
    }
}

void cli_db_link_closed(CliDbLink *link)
{
    if (link->kind == CLI_DB_LINK_PG)
    {
        cli_pg_closed(link->pg);
    }
    else if (link->kind == CLI_DB_LINK_MY)
    {
        cli_mysql_closed(link->my);
    }
    else if (link->kind == CLI_DB_LINK_LITE)
    {
        cli_db_lite_closed(link->lite);
    }
    memor.set(&(MemoriaCfg){.dst = link, .val = 0, .bytes = sizeof(*link)});
}
