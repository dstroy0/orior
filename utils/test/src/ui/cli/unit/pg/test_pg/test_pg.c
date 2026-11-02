// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_pg.c: PostgreSQL: SCRAM gives RFC 7677's proof and signature, a query's rows come a page at a time, a cancel stops a statement, and
// md5 and cleartext passwords sign in where the server requests them. The server tests run where PostgreSQL answers at ORIOR_PG_ADDRESS,
// else on 127.0.0.1:5432, with the password ORIOR_PG_PASSWORD names for the user postgres
#include "ui/cli/src/databases/pg.h"
#include "ui/cli/src/databases/pg_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the rows of a page recorded, the first values of each, and the bytes they use
#define PG_ROWS 1000u
#define PG_VALUES 4u
#define PG_VALUE 64u

// What stops a statement, sent from its own thread once 400 milliseconds have passed, and whether it was sent
typedef struct
{
    CliPgCancel cancel;
    int sent;
} PgStopper;

static char s_host[CLI_NAME_MAX_LEN];
static unsigned int s_port;
static char s_password[256];
static CliDbFailure s_failure;
static char s_value[PG_ROWS][PG_VALUES][PG_VALUE];
static int s_null[PG_ROWS][PG_VALUES];
static unsigned int s_rows;
static CliPg *s_pg;
static char s_client_final[CLI_PG_SIGN_TEXT];
static char s_signature[CLI_PG_SIGN_TEXT];
static int s_done;
static char s_tag[CLI_DB_TAG];
static PgStopper s_stopper = {{"", 0u, 0u, {0u}, 0u}, 0};

// SCRAM's final message made from RFC 7677's values
static void scram_final_made(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pg_failure_internal.scram_final, CliPgFailureCtx, .password = "pencil", .first_bare = "n=user,r=rOprNGfwEbeRWgbNEkqO", .nonce = "rOprNGfwEbeRWgbNEkqO", .server_first = "r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096", .client_final = s_client_final, .signature = s_signature, .failure = &s_failure));
}

// a row recorded, its first PG_VALUES values
static int row_recorded(const CliDbRowCfg *args)
{
    for (unsigned int at = 0u; (s_rows < PG_ROWS) && (at < args->count) && (at < PG_VALUES); at += 1u)
    {
        s_null[s_rows][at] = args->values[at].null;
        const unsigned long long size = args->values[at].null ? 0ull : args->values[at].size;
        const unsigned long long copied = (size < PG_VALUE - 1u) ? size : PG_VALUE - 1u;
        memor.cpy(&(MemoriaCfg){.dst = s_value[s_rows][at], .src = args->values[at].null ? "" : args->values[at].text, .bytes = copied});
        s_value[s_rows][at][copied] = '\0';
    }
    s_rows += (s_rows < PG_ROWS) ? 1u : 0u;
    return 1;
}

// the connection the tests sign in as postgres on, in s_pg; the test ignored where no password is named or no server answers
static void server_required(void)
{
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_PG_PASSWORD", .value_out = s_password, .room = sizeof(s_password)))
    {
        TEST_IGNORE_MESSAGE("ORIOR_PG_PASSWORD names no password");
    }
    cli_unity_server("PG", 5432u, s_host, &s_port);
    s_pg = EMBED_CALL(cli_pg_host.connect, CliPgHostCfg, .host = s_host, .port = s_port, .user = "postgres", .password = s_password, .database = "postgres", .wait = 5000ull, .failure = &s_failure);
    if (s_pg == NULL)
    {
        TEST_IGNORE_MESSAGE("no PostgreSQL answers");
    }
}

// the column `at` of the head is `name` of `kind`, a number where `numeric`
static void assert_column(unsigned int at, const char *name, const char *kind, int numeric)
{
    const CliDbHead *const head = EMBED_CALL(cli_pg_cancel.head, CliPgCancelCfg, .pg_read = s_pg);
    TEST_ASSERT_EQUAL_STRING(name, head->columns[at].name);
    TEST_ASSERT_EQUAL_STRING(kind, head->columns[at].kind);
    TEST_ASSERT_EQUAL_INT(numeric, head->columns[at].numeric);
}

// a query of 2500 rows of four columns started on the server
static void query_started(void)
{
    static const char s_query[] = "select n, n::text as t, null::int as z, now() as at from generate_series(1, 2500) n";
    server_required();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = s_pg, .sql = s_query, .size = sizeof(s_query) - 1u, .failure = &s_failure));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pg_cancel.head, CliPgCancelCfg, .pg_read = s_pg)->rows);
}

// the next page of a thousand rows read
static void page_read(void)
{
    s_rows = 0u;
    (void)EMBED_CALL(cli_pg_failure.rows, CliPgFailureCfg, .pg = s_pg, .count = 1000ull, .each_row = row_recorded, .context = NULL, .done = &s_done, .tag = s_tag, .failure = &s_failure);
}

// the cancel sent after 400 milliseconds
static void cancel_sent(void *context)
{
    PgStopper *const stopper = context;
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 400u);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int sent = EMBED_CALL(cli_pg.cancel_send, CliPgCfg, .cancel_read = &stopper->cancel, .err_msg = &err_msg);
    stopper->sent = sent ? 1 : -1;
}

// the roles orior_md5 and orior_plain made on the server, and its connection closed
static void roles_made(void)
{
    static const char *const s_setup[6] = {"drop role if exists orior_md5", "drop role if exists orior_plain", "set password_encryption = 'md5'", "create role orior_md5 login password 'md5 secret'", "reset password_encryption", "create role orior_plain login password 'plain secret'"};
    server_required();
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        (void)EMBED_CALL(cli_pg_failure_sql.all, CliPgFailureSqlCfg, .pg = s_pg, .sql = s_setup[at], .size = CLI_UNITY_LENGTH(s_setup[at]), .each_row = row_recorded, .context = NULL, .failure = &s_failure);
    }
    EMBED_CALL(cli_pg_failure_sql.close, CliPgFailureSqlCfg, .pg = s_pg);
    s_pg = NULL;
}

// `user` signs in with `password` and not with another
static void assert_signs_in(const char *user, const char *password)
{
    roles_made();
    CliPg *const signed_in = EMBED_CALL(cli_pg_host.connect, CliPgHostCfg, .host = s_host, .port = s_port, .user = user, .password = password, .database = "postgres", .wait = 5000ull, .failure = &s_failure);
    TEST_ASSERT_NOT_NULL(signed_in);
    EMBED_CALL(cli_pg_failure_sql.close, CliPgFailureSqlCfg, .pg = signed_in);
    CliPg *const wrong = EMBED_CALL(cli_pg_host.connect, CliPgHostCfg, .host = s_host, .port = s_port, .user = user, .password = "not it", .database = "postgres", .wait = 5000ull, .failure = &s_failure);
    if (wrong != NULL)
    {
        EMBED_CALL(cli_pg_failure_sql.close, CliPgFailureSqlCfg, .pg = wrong);
    }
    TEST_ASSERT_NULL(wrong);
    TEST_ASSERT_EQUAL_STRING("28P01", s_failure.code);
}

void setUp(void)
{
    cli_unity_begun();
    s_pg = NULL;
    s_rows = 0u;
    s_done = 0;
}

void tearDown(void)
{
    if (s_pg != NULL)
    {
        EMBED_CALL(cli_pg_failure_sql.close, CliPgFailureSqlCfg, .pg = s_pg);
    }
}

void test_scrams_final_message_has_rfc_7677s_proof(void)
{
    scram_final_made();
    TEST_ASSERT_EQUAL_STRING("c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=", s_client_final);
}

void test_scrams_signature_is_rfc_7677s(void)
{
    scram_final_made();
    TEST_ASSERT_EQUAL_STRING("6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=", s_signature);
}

void test_the_server_reports_its_version(void)
{
    server_required();
    TEST_ASSERT_NOT_NULL(EMBED_CALL(cli_pg_name.parameter, CliPgNameCfg, .pg_read = s_pg, .name = "server_version"));
}

void test_a_query_gives_its_columns_each_of_its_type(void)
{
    query_started();
    TEST_ASSERT_EQUAL_UINT(4u, EMBED_CALL(cli_pg_cancel.head, CliPgCancelCfg, .pg_read = s_pg)->column_count);
    assert_column(0u, "n", "int4", 1);
    assert_column(1u, "t", "text", 0);
    assert_column(2u, "z", "int4", 1);
    assert_column(3u, "at", "timestamptz", 0);
}

void test_a_page_of_rows_comes_and_the_rest_wait(void)
{
    query_started();
    page_read();
    TEST_ASSERT_EQUAL_UINT(1000u, s_rows);
    TEST_ASSERT_FALSE(s_done);
}

void test_a_pages_rows_come_in_order(void)
{
    query_started();
    page_read();
    TEST_ASSERT_EQUAL_STRING("1000", s_value[999][0]);
}

void test_a_null_is_read_as_null(void)
{
    query_started();
    page_read();
    TEST_ASSERT_TRUE(s_null[0][2]);
}

void test_a_time_with_a_zone_has_its_offset(void)
{
    query_started();
    page_read();
    const int plus = cellul.chr(&(CatenaFinitaCfg){.src = s_value[0][3], .cap = CLI_CELLS_POOL, .byte = '+'}) != NULL;
    const int minus = cellul.chr(&(CatenaFinitaCfg){.src = s_value[0][3], .cap = CLI_CELLS_POOL, .byte = '-'}) != NULL;
    TEST_ASSERT_TRUE(plus || minus);
}

void test_the_next_page_follows_on(void)
{
    query_started();
    page_read();
    page_read();
    TEST_ASSERT_EQUAL_STRING("1001", s_value[0][0]);
}

void test_the_last_page_ends_with_the_statements_tag(void)
{
    query_started();
    page_read();
    page_read();
    page_read();
    TEST_ASSERT_TRUE(s_done);
    TEST_ASSERT_EQUAL_STRING("SELECT 2500", s_tag);
}

void test_a_failure_gives_the_servers_code(void)
{
    server_required();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = s_pg, .sql = "select 1/0", .size = 10ull, .failure = &s_failure));
    TEST_ASSERT_EQUAL_STRING("22012", s_failure.code);
}

void test_an_open_transaction_is_reported(void)
{
    server_required();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = s_pg, .sql = "begin", .size = 5ull, .failure = &s_failure));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_pg_cancel.head, CliPgCancelCfg, .pg_read = s_pg)->rows);
    TEST_ASSERT_EQUAL_CHAR('T', EMBED_CALL(cli_pg_cancel.status, CliPgCancelCfg, .pg_read = s_pg));
}

void test_a_closed_transaction_is_reported(void)
{
    server_required();
    (void)EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = s_pg, .sql = "begin", .size = 5ull, .failure = &s_failure);
    (void)EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = s_pg, .sql = "rollback", .size = 8ull, .failure = &s_failure);
    TEST_ASSERT_EQUAL_CHAR('I', EMBED_CALL(cli_pg_cancel.status, CliPgCancelCfg, .pg_read = s_pg));
}

void test_a_cancel_stops_a_statement_and_the_connection_answers_after_it(void)
{
    static CliThread s_thread = {cancel_sent, &s_stopper};
    server_required();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pg_cancel.canceller, CliPgCancelCfg, .pg_read = s_pg, .cancel = &s_stopper.cancel));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread));
    (void)EMBED_CALL(cli_pg_failure_sql.start, CliPgFailureSqlCfg, .pg = s_pg, .sql = "select pg_sleep(30)", .size = 19ull, .failure = &s_failure);
    const int rows = EMBED_CALL(cli_pg_cancel.head, CliPgCancelCfg, .pg_read = s_pg)->rows;
    const int stopped = rows && !EMBED_CALL(cli_pg_failure.rows, CliPgFailureCfg, .pg = s_pg, .count = 10ull, .each_row = row_recorded, .context = NULL, .done = &s_done, .tag = s_tag, .failure = &s_failure);
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    int sent = 0;
    while ((sent == 0) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
        sent = s_stopper.sent;
    }
    TEST_ASSERT_TRUE(stopped);
    TEST_ASSERT_EQUAL_INT(1, sent);
    TEST_ASSERT_EQUAL_STRING("57014", s_failure.code);
    s_rows = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pg_failure_sql.all, CliPgFailureSqlCfg, .pg = s_pg, .sql = "select 'it''s'", .size = 14ull, .each_row = row_recorded, .context = NULL, .failure = &s_failure));
    TEST_ASSERT_EQUAL_UINT(1u, s_rows);
    TEST_ASSERT_EQUAL_STRING("it's", s_value[0][0]);
}

void test_an_md5_user_signs_in_with_its_password_and_not_another(void)
{
    assert_signs_in("orior_md5", "md5 secret");
}

void test_a_cleartext_user_signs_in_with_its_password_and_not_another(void)
{
    assert_signs_in("orior_plain", "plain secret");
}
