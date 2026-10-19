// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_test.c: PostgreSQL's check: SCRAM gives RFC 7677's proof and signature; a query's rows come a page at a time and a
// cancel stops it; and md5 and cleartext passwords sign in where the server asks for them. The last two run on the
// server orior's tests sign in to, where one runs: PostgreSQL at ORIOR_PG_ADDRESS, else on 127.0.0.1:5432, with the
// password ORIOR_PG_PASSWORD names for the user postgres
#include "../../../../../ui/cli/src/databases/pg.h"
#include "../../../../../ui/cli/src/databases/pg_internal.h"
#include "cli_check.h"

// the rows of a page kept, the first values of each, and the bytes they take
#define PG_ROWS 1000u
#define PG_VALUES 4u
#define PG_VALUE 64u

static char s_host[CLI_NAME_LONGEST];
static unsigned int s_port;
static char s_password[256];
static CliDbFailure s_failure;
static char s_value[PG_ROWS][PG_VALUES][PG_VALUE];
static int s_null[PG_ROWS][PG_VALUES];
static unsigned int s_rows;

// SCRAM gives RFC 7677's proof and signature
static void pg_scram(void)
{
    char client_final[CLI_PG_SIGN_TEXT];
    char signature[CLI_PG_SIGN_TEXT];
    cli_check(
        cli_pg_scram_final("pencil", "n=user,r=rOprNGfwEbeRWgbNEkqO", "rOprNGfwEbeRWgbNEkqO",
                           "r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096",
                           client_final, signature, &s_failure),
        "SCRAM's final message is made");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = client_final,
                                           .other = "c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,"
                                                    "p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=",
                                           .cap = CLI_CELLS_POOL}),
              "with RFC 7677's proof");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = signature, .other = "6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=", .cap = CLI_CELLS_POOL}),
              "and its signature");
}

// a row kept, its first PG_VALUES values
static int pg_kept(void *context, const CliDbValue *values, unsigned int count)
{
    (void)context;
    for (unsigned int at = 0u; (s_rows < PG_ROWS) && (at < count) && (at < PG_VALUES); at += 1u)
    {
        s_null[s_rows][at] = values[at].null;
        const unsigned long long size = values[at].null ? 0ull : values[at].size;
        const unsigned long long kept = (size < PG_VALUE - 1u) ? size : PG_VALUE - 1u;
        memor.cpy(
            &(MemoriaCfg){.dst = s_value[s_rows][at], .src = values[at].null ? "" : values[at].text, .bytes = kept});
        s_value[s_rows][at][kept] = '\0';
    }
    s_rows += (s_rows < PG_ROWS) ? 1u : 0u;
    return 1;
}

// the server the tests sign in to, as `user` with `password`: the connection, or NULL where none answers
static CliPg *pg_server(const char *user, const char *password)
{
    return cli_pg_connected(s_host, s_port, user, password, "postgres", 5000ull, &s_failure);
}

// whether the column `at` of the head is `name` of `kind`, a number where `numeric`
static int pg_column(const CliDbHead *head, unsigned int at, const char *name, const char *kind, int numeric)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = head->columns[at].name, .other = name, .cap = CLI_CELLS_POOL})) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = head->columns[at].kind, .other = kind, .cap = CLI_CELLS_POOL})) &&
           (head->columns[at].numeric == numeric);
}

// What stops a statement, sent from a thread of its own once 400 milliseconds have passed, and whether it was
typedef struct
{
    CliPgCancel cancel;
    CliLock lock;
    int sent;
} PgStopper;

static PgStopper s_stopper = {{"", 0u, 0u, {0u}, 0u}, CLI_LOCK_INIT, 0};

static void pg_stop(void *context)
{
    PgStopper *const stopper = context;
    query_paused(400u);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    const int sent = cli_pg_cancel_sent(&stopper->cancel, &said);
    cli_lock_take(&stopper->lock);
    stopper->sent = sent ? 1 : -1;
    cli_lock_give(&stopper->lock);
}

// a query's rows come a page at a time and a cancel stops it
static void pg_pages(void)
{
    CliPg *const pg = pg_server("postgres", s_password);
    if (pg == NULL)
    {
        cli_check(1, "no PostgreSQL answers, and no rows are read");
        return;
    }
    cli_check(cli_pg_parameter(pg, "server_version") != NULL, "the server says its version");
    static const char s_query[] = "select n, n::text as t, null::int as z, now() as at from generate_series(1, 2500) n";
    cli_check(cli_pg_start(pg, s_query, sizeof(s_query) - 1u, &s_failure) && cli_pg_head(pg)->rows,
              "the query gives rows");
    const CliDbHead *const head = cli_pg_head(pg);
    cli_check((head->column_count == 4u) && pg_column(head, 0u, "n", "int4", 1) &&
                  pg_column(head, 1u, "t", "text", 0) && pg_column(head, 2u, "z", "int4", 1) &&
                  pg_column(head, 3u, "at", "timestamptz", 0),
              "in its columns, each of its type");
    int done = 0;
    char tag[CLI_DB_TAG];
    s_rows = 0u;
    (void)cli_pg_rows(pg, 1000ull, pg_kept, NULL, &done, tag, &s_failure);
    cli_check((s_rows == 1000u) && !done, "a page of rows comes, and the rest wait");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_value[999][0], .other = "1000", .cap = CLI_CELLS_POOL}),
              "each in its order");
    cli_check(s_null[0][2], "a null is none");
    cli_check((cellul.chr(&(CatenaFinitaCfg){.src = s_value[0][3], .cap = CLI_CELLS_POOL, .byte = '+'}) != NULL) ||
                  (cellul.chr(&(CatenaFinitaCfg){.src = s_value[0][3], .cap = CLI_CELLS_POOL, .byte = '-'}) != NULL),
              "a time with a zone has its offset");
    s_rows = 0u;
    (void)cli_pg_rows(pg, 1000ull, pg_kept, NULL, &done, tag, &s_failure);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_value[0][0], .other = "1001", .cap = CLI_CELLS_POOL}),
              "the next page follows on");
    s_rows = 0u;
    (void)cli_pg_rows(pg, 1000ull, pg_kept, NULL, &done, tag, &s_failure);
    cli_check(done && (cellul.eq(&(CatenaFinitaCfg){.src = tag, .other = "SELECT 2500", .cap = CLI_CELLS_POOL})),
              "the last page ends with the statement's tag");
    cli_check(!cli_pg_start(pg, "select 1/0", 10ull, &s_failure) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_failure.code, .other = "22012", .cap = CLI_CELLS_POOL})),
              "a failure gives the server's code");
    cli_check(cli_pg_start(pg, "begin", 5ull, &s_failure) && !cli_pg_head(pg)->rows && (cli_pg_status(pg) == 'T'),
              "a transaction opened is told");
    (void)cli_pg_start(pg, "rollback", 8ull, &s_failure);
    cli_check(cli_pg_status(pg) == 'I', "and closed");
    cli_check(cli_pg_canceller(pg, &s_stopper.cancel), "the server gives the key that stops a statement");
    static CliThread s_thread = {pg_stop, &s_stopper};
    cli_check(cli_thread_started(&s_thread), "the stop is sent from a thread of its own");
    (void)cli_pg_start(pg, "select pg_sleep(30)", 19ull, &s_failure);
    const int rows = cli_pg_head(pg)->rows;
    cli_check(rows && !cli_pg_rows(pg, 10ull, pg_kept, NULL, &done, tag, &s_failure), "the sleep was stopped");
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    int sent = 0;
    while ((sent == 0) && (cli_clock_milliseconds() < until))
    {
        query_paused(20u);
        cli_lock_take(&s_stopper.lock);
        sent = s_stopper.sent;
        cli_lock_give(&s_stopper.lock);
    }
    cli_check((sent == 1) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_failure.code, .other = "57014", .cap = CLI_CELLS_POOL})),
              "by the cancel");
    s_rows = 0u;
    cli_check(cli_pg_all(pg, "select 'it''s'", 14ull, pg_kept, NULL, &s_failure) && (s_rows == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_value[0][0], .other = "it's", .cap = CLI_CELLS_POOL})),
              "the connection answers after it");
    cli_pg_closed(pg);
}

// md5 and cleartext passwords sign in where the server asks for them
static void pg_passwords(void)
{
    static const char *const s_setup[6] = {
        "drop role if exists orior_md5",   "drop role if exists orior_plain",
        "set password_encryption = 'md5'", "create role orior_md5 login password 'md5 secret'",
        "reset password_encryption",       "create role orior_plain login password 'plain secret'"};
    CliPg *const pg = pg_server("postgres", s_password);
    if (pg == NULL)
    {
        cli_check(1, "no PostgreSQL answers, and no one signs in");
        return;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        (void)cli_pg_all(pg, s_setup[at], cellul.len(&(CatenaFinitaCfg){.src = s_setup[at], .cap = CLI_CELLS_POOL}),
                         pg_kept, NULL, &s_failure);
    }
    cli_pg_closed(pg);
    static const char *const s_users[2][2] = {{"orior_md5", "md5 secret"}, {"orior_plain", "plain secret"}};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        CliPg *const signed_in = pg_server(s_users[at][0], s_users[at][1]);
        cli_check(signed_in != NULL, "a user signs in with its password");
        if (signed_in != NULL)
        {
            cli_pg_closed(signed_in);
        }
        CliPg *const wrong = pg_server(s_users[at][0], "not it");
        cli_check((wrong == NULL) &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = s_failure.code, .other = "28P01", .cap = CLI_CELLS_POOL})),
                  "and not with another");
        if (wrong != NULL)
        {
            cli_pg_closed(wrong);
        }
    }
}

int main(void)
{
    pg_scram();
    cli_check_server("PG", 5432u, s_host, &s_port);
    if (!query_environment_read("ORIOR_PG_PASSWORD", s_password, sizeof(s_password)))
    {
        cli_check(1, "ORIOR_PG_PASSWORD names no password, and no server is reached");
        return cli_checked();
    }
    pg_pages();
    pg_passwords();
    return cli_checked();
}
