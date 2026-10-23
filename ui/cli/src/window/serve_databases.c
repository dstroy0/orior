// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_databases.c: the database connections open, each by its name in the tree's databases.json, and the database
// window's calls: listed, kept, taken out, connected and closed, a statement run and its rows read a page at a time,
// stopped, the schema's children read, the rows exported, the history read, and the statements of a text found. A
// connection in use stays open until its call ends, one closed meanwhile closing then
#include "../databases/db.h"
#include "serve_internal.h"

// the most connections open at once
#define CLI_SERVE_DATABASES 64u

// A connection open: whether its place is taken, its name, the connection, how many calls use it, and whether it is
// let go, to close as its last call ends; and what its server last said went wrong, under its own lock, which a call
// that may fail holds
typedef struct
{
    int used;
    char name[CLI_NAME_LONGEST];
    CliDbOpen *open;
    unsigned int users;
    int letting_go;
    CliLock failing;
    CliDbFailure failure;
} CliServeDatabase;

static CliServeDatabase s_database[CLI_SERVE_DATABASES];
static CliLock s_lock = CLI_LOCK_INIT;

// a connection let go of its name, closed now where no call uses it; under the lock
static void databases_let_go(CliServeDatabase *one)
{
    one->name[0] = '\0';
    one->letting_go = 1;
    if (one->users == 0u)
    {
        cli_db_close(one->open);
        one->used = 0;
    }
}

void cli_serve_databases_closed(void)
{
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < CLI_SERVE_DATABASES; at += 1u)
    {
        if (s_database[at].used && !s_database[at].letting_go)
        {
            databases_let_go(&s_database[at]);
        }
    }
    cli_lock_give(&s_lock);
}

// the connection open as `name` taken for a call: its place, or CLI_SERVE_DATABASES with why said
static unsigned int databases_taken(const char *name, CliLine *said)
{
    cli_lock_take(&s_lock);
    unsigned int at = 0u;
    while ((at < CLI_SERVE_DATABASES) &&
           (!s_database[at].used || s_database[at].letting_go ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = s_database[at].name, .other = name, .cap = CLI_CELLS_POOL}))))
    {
        at += 1u;
    }
    if (at < CLI_SERVE_DATABASES)
    {
        s_database[at].users += 1u;
    }
    cli_lock_give(&s_lock);
    if (at == CLI_SERVE_DATABASES)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not connected");
    }
    return at;
}

// a call's use of a connection ended, the connection closed where it was let go meanwhile
static void databases_given(unsigned int at)
{
    cli_lock_take(&s_lock);
    s_database[at].users -= 1u;
    if (s_database[at].letting_go && (s_database[at].users == 0u))
    {
        cli_db_close(s_database[at].open);
        s_database[at].used = 0;
    }
    cli_lock_give(&s_lock);
}

// the connection open as `name` let go, where one is
static void databases_named_let_go(const char *name)
{
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < CLI_SERVE_DATABASES; at += 1u)
    {
        if (s_database[at].used && !s_database[at].letting_go &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_database[at].name, .other = name, .cap = CLI_CELLS_POOL})))
        {
            databases_let_go(&s_database[at]);
        }
    }
    cli_lock_give(&s_lock);
}

// what a server said went wrong, as one text: its message, its code, its detail and its hint
static void databases_failure_said(const CliDbFailure *failure, CliLine *said)
{
    cli_line_text(said, failure->message);
    if (failure->code[0] != '\0')
    {
        cli_line_text(said, " (");
        cli_line_text(said, failure->code);
        cli_line_character(said, ')');
    }
    if (failure->detail[0] != '\0')
    {
        cli_line_character(said, '\n');
        cli_line_text(said, failure->detail);
    }
    if (failure->hint[0] != '\0')
    {
        cli_line_character(said, '\n');
        cli_line_text(said, failure->hint);
    }
}

// what a server said went wrong as the page reads it: `{"message", "code", "detail", "hint", "position"}`
static void databases_failure_written(const CliDbFailure *failure, CliLine *out)
{
    const char *const field_name[4] = {"{\"message\":", ",\"code\":", ",\"detail\":", ",\"hint\":"};
    const char *const field[4] = {failure->message, failure->code, failure->detail, failure->hint};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        cli_line_text(out, field_name[at]);
        cli_json_quoted(out, field[at], cellul.len(&(CatenaFinitaCfg){.src = field[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, ",\"position\":");
    cli_line_decimal(out, failure->position, 1u);
    cli_line_character(out, '}');
}

static int db_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    static CliDbOpen *s_open[CLI_SERVE_DATABASES];
    cli_lock_take(&s_lock);
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < CLI_SERVE_DATABASES; at += 1u)
    {
        if (s_database[at].used && !s_database[at].letting_go)
        {
            s_open[count] = s_database[at].open;
            count += 1u;
        }
    }
    const int listed = cli_db_listed(root, s_open, count, out, said);
    cli_lock_give(&s_lock);
    return listed;
}

static int db_save(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    static CliDbConnection s_connection;
    static CliLock s_connection_lock = CLI_LOCK_INIT;
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_lock_take(&s_connection_lock);
    int done = cli_db_connection_of(args->json, cli_serve_arg(args, "connection"), &s_connection, said) &&
               cli_db_save(root, &s_connection, cli_serve_string_or(args, "was", NULL), said);
    const char *const password = cli_serve_string_or(args, "password", "");
    if (done && (password[0] != '\0'))
    {
        char account[CLI_NAME_LONGEST * 2u];
        CliLine named = {account, sizeof(account), 0ull};
        cli_db_account(&s_connection, &named);
        (void)cli_line_closed(&named);
        done = cli_keychain_keep("database", account, password, said);
    }
    cli_lock_give(&s_connection_lock);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int db_remove(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const name = cli_serve_string(args, "name", said);
    if ((name == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    databases_named_let_go(name);
    const int done = cli_db_remove(root, name, said);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int db_forget_password(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    static CliDbConnection s_connection;
    static CliLock s_connection_lock = CLI_LOCK_INIT;
    const char *const name = cli_serve_string(args, "name", said);
    if ((name == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_lock_take(&s_connection_lock);
    int done = cli_db_named(root, name, &s_connection, said);
    if (done)
    {
        char account[CLI_NAME_LONGEST * 2u];
        CliLine named = {account, sizeof(account), 0ull};
        cli_db_account(&s_connection, &named);
        (void)cli_line_closed(&named);
        done = cli_keychain_forget("database", account, said);
    }
    cli_lock_give(&s_connection_lock);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int db_connect(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    static CliDbConnection s_connection;
    static CliDbFailure s_failure;
    static CliLock s_connection_lock = CLI_LOCK_INIT;
    const char *const name = cli_serve_string(args, "name", said);
    if ((name == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    const char *const password = cli_serve_string_or(args, "password", NULL);
    const int keep = cli_serve_bool_or(args, "keep", 0);
    cli_lock_take(&s_connection_lock);
    if (!cli_db_named(root, name, &s_connection, said))
    {
        cli_lock_give(&s_connection_lock);
        return 0;
    }
    databases_named_let_go(name);
    memor.set(&(MemoriaCfg){.dst = &s_failure, .val = 0, .bytes = sizeof(s_failure)});
    CliDbOpen *const open = cli_db_connect(root, &s_connection, password, &s_failure);
    int done = (open != NULL);
    if (!done)
    {
        databases_failure_said(&s_failure, said);
    }
    if (done && keep && (password != NULL) && (password[0] != '\0'))
    {
        char account[CLI_NAME_LONGEST * 2u];
        CliLine named = {account, sizeof(account), 0ull};
        cli_db_account(&s_connection, &named);
        (void)cli_line_closed(&named);
        done = cli_keychain_keep("database", account, password, said);
    }
    cli_lock_give(&s_connection_lock);
    if (!done)
    {
        if (open != NULL)
        {
            cli_db_close(open);
        }
        return 0;
    }
    cli_line_text(out, "{\"version\":");
    cli_json_quoted(out, cli_db_version(open),
                    cellul.len(&(CatenaFinitaCfg){.src = cli_db_version(open), .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"transaction\":");
    cli_json_quoted(out, cli_db_transaction(open),
                    cellul.len(&(CatenaFinitaCfg){.src = cli_db_transaction(open), .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"zone\":\"");
    char zone[CLI_NAME_LONGEST];
    CliLine zoned = {zone, sizeof(zone), 0ull};
    cli_db_zone(open, &zoned);
    cli_json_escaped_lossy(out, (const unsigned char *)zone, cli_line_fits(&zoned) ? zoned.at : 0ull);
    cli_line_text(out, "\"}");
    cli_lock_take(&s_lock);
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
    cli_lock_give(&s_lock);
    if (at == CLI_SERVE_DATABASES)
    {
        cli_db_close(open);
    }
    return done;
}

static int db_close(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    if (name == NULL)
    {
        return 0;
    }
    databases_named_let_go(name);
    cli_line_text(out, "null");
    return 1;
}

static int db_run(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    const char *const sql = (name != NULL) ? cli_serve_string(args, "sql", said) : NULL;
    const unsigned int at = (sql != NULL) ? databases_taken(name, said) : CLI_SERVE_DATABASES;
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    cli_lock_take(&one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const unsigned long long start = out->at;
    if (!cli_db_run(one->open, sql, cellul.len(&(CatenaFinitaCfg){.src = sql, .cap = CLI_CELLS_POOL}),
                    cli_serve_bool_or(args, "answered", 0), cli_serve_whole_or(args, "page", 200ull), out,
                    &one->failure))
    {
        out->at = start;
        cli_line_text(out, "{\"failure\":");
        databases_failure_written(&one->failure, out);
        cli_line_text(out, ",\"transaction\":");
        const char *const transaction = cli_db_transaction(one->open);
        cli_json_quoted(out, transaction, cellul.len(&(CatenaFinitaCfg){.src = transaction, .cap = CLI_CELLS_POOL}));
        cli_line_character(out, '}');
    }
    cli_lock_give(&one->failing);
    databases_given(at);
    return 1;
}

// the connection of the argument `name` taken, its place, or CLI_SERVE_DATABASES with why said
static unsigned int databases_of(const CliServeArgs *args, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    return (name != NULL) ? databases_taken(name, said) : CLI_SERVE_DATABASES;
}

static int db_rows(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long from = 0ull;
    unsigned long long count = 0ull;
    const unsigned int at = databases_of(args, said);
    if ((at == CLI_SERVE_DATABASES) || !cli_serve_whole(args, "from", &from, said) ||
        !cli_serve_whole(args, "count", &count, said))
    {
        if (at != CLI_SERVE_DATABASES)
        {
            databases_given(at);
        }
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    cli_lock_take(&one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const int done = cli_db_rows(one->open, from, count, out, &one->failure);
    if (!done)
    {
        databases_failure_said(&one->failure, said);
    }
    cli_lock_give(&one->failing);
    databases_given(at);
    return done;
}

static int db_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const unsigned int at = databases_of(args, said);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    const int done = cli_db_stop(s_database[at].open, said);
    databases_given(at);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int db_stop_reading(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const unsigned int at = databases_of(args, said);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    const int done = cli_db_stop_reading(s_database[at].open, said);
    databases_given(at);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int db_children(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_path[64];
    unsigned int count = 0u;
    const unsigned int at = databases_of(args, said);
    if ((at == CLI_SERVE_DATABASES) || !cli_serve_strings(args, "path", s_path, 64u, &count, said))
    {
        if (at != CLI_SERVE_DATABASES)
        {
            databases_given(at);
        }
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    cli_lock_take(&one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const int done = cli_db_children(one->open, s_path, count, out, &one->failure);
    if (!done)
    {
        databases_failure_said(&one->failure, said);
    }
    cli_lock_give(&one->failing);
    databases_given(at);
    return done;
}

// how far along an export is told to the window as it writes
static void databases_exported(void *context, unsigned long long written)
{
    (void)context;
    char body[128];
    CliLine line = {body, sizeof(body), 0ull};
    cli_line_text(&line, "{\"call\":\"db_export\",\"done\":");
    cli_line_decimal(&line, written, 1u);
    cli_line_text(&line, ",\"unit\":\"rows written\"}");
    cli_serve_emitted("call-progress", body, line.at);
}

static int db_export(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char target[CLI_PATH_LONGEST];
    const char *const format = cli_serve_string(args, "format", said);
    if ((format == NULL) || !cli_serve_at(args, root, target, said))
    {
        return 0;
    }
    const unsigned int at = databases_of(args, said);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    CliServeDatabase *const one = &s_database[at];
    cli_lock_take(&one->failing);
    memor.set(&(MemoriaCfg){.dst = &one->failure, .val = 0, .bytes = sizeof(one->failure)});
    const int done = cli_db_export(one->open, target, format, databases_exported, NULL, out, &one->failure);
    if (!done)
    {
        databases_failure_said(&one->failure, said);
    }
    cli_lock_give(&one->failing);
    databases_given(at);
    return done;
}

static int db_history(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const unsigned int at = databases_of(args, said);
    if (at == CLI_SERVE_DATABASES)
    {
        return 0;
    }
    cli_db_history(s_database[at].open, out);
    databases_given(at);
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
