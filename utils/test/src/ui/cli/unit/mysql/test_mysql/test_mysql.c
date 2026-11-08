// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_mysql.c: a power is computed by its modulus, OAEP unmasks to its message, the scrambles are the ones MySQL computes, each sign-in works and a
// query's rows come a page at a time, and MariaDB signs in, reads rows and stops a query. The live cases run on the server orior's tests sign in to:
// MySQL, or MariaDB, at ORIOR_<name>_ADDRESS, else on 127.0.0.1:3306, with the password ORIOR_<name>_PASSWORD names for root, and are ignored otherwise
#include "ui/cli/src/databases/mysql.h"
#include "ui/cli/src/databases/mysql_internal.h"
#include "ui/cli/src/text/digest.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the rows of a page recorded, the first values of each, and the bytes they use
#define MYSQL_ROWS 1000u
#define MYSQL_VALUES 4u
#define MYSQL_VALUE 64u

// What stops a statement, sent from a thread of its own once 400 milliseconds have passed
typedef struct
{
    CliMysqlCancel cancel;
    CliThread thread;
} MysqlStopper;

static CliBig s_big[6];
static CliDbFailure s_failure;
static char s_value[MYSQL_ROWS][MYSQL_VALUES][MYSQL_VALUE];
static int s_null[MYSQL_ROWS][MYSQL_VALUES];
static unsigned int s_rows;
static MysqlStopper s_cancel_thread;

// what the live MySQL sequence recorded
static int s_mysql_run_result;
static int s_mysql_named;
static int s_mysql_answered;
static int s_sha2_signed_in[2];
static int s_sha2_sealed[2];
static int s_native_signed_in;
static int s_wrong_refused;
static char s_wrong_code[64];
static int s_insert_run_result;
static int s_insert_rows;
static char s_insert_tag[CLI_DB_TAG];
static int s_select_rows;
static unsigned int s_select_columns;
static int s_columns_typed;
static unsigned int s_page_rows;
static int s_page_done;
static char s_page_last[MYSQL_VALUE];
static char s_page_bytes[MYSQL_VALUE];
static int s_page_null;
static unsigned int s_remainder_rows;
static int s_remainder_done;
static int s_begun;
static int s_begun_rows;
static int s_open_in_transaction;
static int s_rolled_in_transaction;
static int s_missing_run_result;
static char s_missing_code[64];
static unsigned long long s_sleep_consumed;
static int s_sleep_returned;
static unsigned int s_sleep_rows;
static char s_sleep_value[MYSQL_VALUE];
static int s_literal_run_result;
static unsigned int s_literal_rows;
static char s_literal_value[MYSQL_VALUE];

// what the live MariaDB sequence recorded
static int s_maria_run_result;
static int s_maria_named;
static int s_maria_answered;
static int s_maria_signed_in;
static int s_maria_wrong_refused;
static char s_maria_wrong_code[64];
static int s_maria_select_rows;
static unsigned int s_maria_columns;
static int s_maria_columns_typed;
static unsigned int s_maria_page_rows;
static int s_maria_page_done;
static char s_maria_page_last[MYSQL_VALUE];
static unsigned int s_maria_remainder_rows;
static int s_maria_remainder_done;
static unsigned long long s_maria_sleep_consumed;
static int s_maria_sleep_returned;
static char s_maria_sleep_code[64];
static int s_maria_after_run_result;
static char s_maria_after_value[MYSQL_VALUE];

// whether two numbers are the same
static int numbers_same(const CliBig *one, const CliBig *other)
{
    return (one->count == other->count) && (memor.cmp(&(MemoriaCfg){.src = one->word_text, .other = other->word_text, .bytes = one->count * sizeof(one->word_text[0])}) == 0);
}

// a row recorded, its first MYSQL_VALUES values
static int row_recorded(const CliDbRowCfg *args)
{
    for (unsigned int at = 0u; (s_rows < MYSQL_ROWS) && (at < args->count) && (at < MYSQL_VALUES); at += 1u)
    {
        s_null[s_rows][at] = args->values[at].null;
        const unsigned long long size = args->values[at].null ? 0ull : args->values[at].size;
        const unsigned long long copy_len = (size < MYSQL_VALUE - 1u) ? size : MYSQL_VALUE - 1u;
        memor.cpy(&(MemoriaCfg){.dst = s_value[s_rows][at], .src = args->values[at].null ? "" : args->values[at].text, .bytes = copy_len});
        s_value[s_rows][at][copy_len] = '\0';
    }
    s_rows += (s_rows < MYSQL_ROWS) ? 1u : 0u;
    return 1;
}

// the running statement canceled from its own thread after 400 milliseconds
static void statement_cancelled(void *context)
{
    MysqlStopper *const cancel_thread = context;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 400u);
    (void)EMBED_CALL(cli_mysql.cancel_send, CliMysqlCfg, .cancel_read = &cancel_thread->cancel, .err_msg = &err_msg);
}

// every statement of `count` run on `my`, each read whole
static void statements_run(CliMysql *my, const char *const *sql, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_rows = 0u;
        (void)EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = sql[at], .size = CLI_UNITY_LENGTH(sql[at]), .each_row = row_recorded, .context = NULL, .failure = &s_failure);
    }
}

// whether the column `at` of the head is `name` of `type`, a number where `numeric`, the type not compared where NULL
static int column_is(const CliDbHead *head, unsigned int at, const char *name, const char *type, int numeric)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = head->columns[at].name, .other = name, .cap = CLI_POOL_BYTES}) && ((type == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = head->columns[at].type, .other = type, .cap = CLI_POOL_BYTES})) && (head->columns[at].numeric == numeric);
}

// the server `name` names connected to as `user`, NULL where none answers or none is named
static CliMysql *server_connected(const char *name, const char *user, const char *password)
{
    char host[CLI_NAME_MAX_LEN];
    unsigned int port = 0u;
    cli_unity_server(name, 3306u, host, &port);
    return EMBED_CALL(cli_mysql_host.connect, CliMysqlHostCfg, .host = host, .port = port, .user = user, .password = password, .database = "", .wait = 5000ull, .failure = &s_failure);
}

// the password ORIOR_<name>_PASSWORD names into `password`, which is sized for 256: 1, or 0 where it names none
static int password_read(const char *name, char *password)
{
    char variable[64];
    VerbaLinea line = {variable, sizeof(variable), 0ull};
    mmgr_line_put(&line, "ORIOR_");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, "_PASSWORD");
    (void)mmgr_line_finish(&line);
    return EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = variable, .value_out = password, .capacity = 256u);
}

// a sleep started on `my` stopped by a cancel from a thread of its own: the milliseconds it took, and whether it returned rows
static unsigned long long sleep_stopped(CliMysql *my, int *returned)
{
    EMBED_CALL(cli_mysql_cancel.canceller, CliMysqlCancelCfg, .mysql = my, .cancel = &s_cancel_thread.cancel);
    s_cancel_thread.thread = (CliThread){statement_cancelled, &s_cancel_thread};
    (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_cancel_thread.thread);
    const unsigned long long started = cli_clock_milliseconds();
    s_rows = 0u;
    *returned = EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = "SELECT SLEEP(30)", .size = 16ull, .each_row = row_recorded, .context = NULL, .failure = &s_failure);
    return cli_clock_milliseconds() - started;
}

// the INSERT of a thousand rows numbered from 1, each with `x`, two bytes and no time, into `out`, which is sized for 2048
static void thousand_written(char *out)
{
    static const char *const s_names[3] = {"a", "b", "c"};
    VerbaLinea line = {out, 2048u, 0ull};
    mmgr_line_put(&line, "INSERT INTO orior_test.n SELECT 1 + a.d + 10 * b.d + 100 * c.d, 'x', 0x00FF, NULL FROM ");
    for (unsigned int table = 0u; table < 3u; table += 1u)
    {
        mmgr_line_put(&line, (table > 0u) ? ", (SELECT 0 d" : "(SELECT 0 d");
        for (unsigned int digit = 1u; digit < 10u; digit += 1u)
        {
            mmgr_line_put(&line, " UNION SELECT ");
            mmgr_line_decimal(&line, digit, 1u);
        }
        mmgr_line_put(&line, ") ");
        mmgr_line_put(&line, s_names[table]);
    }
    (void)mmgr_line_finish(&line);
}

// `text` copied into `out`, which is sized for `capacity`
static void text_stored(char *out, unsigned long long capacity, const char *text)
{
    VerbaLinea line = {out, capacity, 0ull};
    mmgr_line_put(&line, text);
    (void)mmgr_line_finish(&line);
}

// the live MySQL sequence, once for the process
static void mysql_run_result(void)
{
    static const char *const s_users[4] = {"DROP USER IF EXISTS 'orior_sha2'@'%', 'orior_native'@'%'", "CREATE USER 'orior_sha2'@'%' IDENTIFIED WITH caching_sha2_password BY 'sha2 secret'", "CREATE USER 'orior_native'@'%' IDENTIFIED WITH mysql_native_password BY 'native secret'", "FLUSH PRIVILEGES"};
    static const char *const s_table[3] = {"CREATE DATABASE IF NOT EXISTS orior_test", "CREATE TABLE IF NOT EXISTS orior_test.n (n INT, t VARCHAR(10), b VARBINARY(4), at TIMESTAMP NULL)", "TRUNCATE orior_test.n"};
    static const char *const s_delete[1] = {"DELETE FROM orior_test.n WHERE n = 1"};
    static const char *const s_rollback[1] = {"ROLLBACK"};
    static const char *const s_drop[1] = {"DROP DATABASE orior_test"};
    static const char s_select[] = "SELECT n, t, b, at FROM orior_test.n ORDER BY n";
    char password[256];
    char insert[2048];
    char tag[CLI_DB_TAG];
    char literal[64];
    if (s_mysql_run_result)
    {
        return;
    }
    s_mysql_run_result = 1;
    s_mysql_named = password_read("MYSQL", password);
    if (!s_mysql_named)
    {
        return;
    }
    CliMysql *const my = server_connected("MYSQL", "root", password);
    s_mysql_answered = my != NULL;
    if (my == NULL)
    {
        return;
    }
    statements_run(my, s_users, 4u);
    // the first sign-in after the cache is cleared sends the password whole, sealed by the server's key; the second is answered from the server's cache
    for (unsigned int attempt = 0u; attempt < 2u; attempt += 1u)
    {
        CliMysql *const signed_in = server_connected("MYSQL", "orior_sha2", "sha2 secret");
        s_sha2_signed_in[attempt] = signed_in != NULL;
        if (signed_in != NULL)
        {
            s_sha2_sealed[attempt] = EMBED_CALL(cli_mysql_cancel.seal, CliMysqlCancelCfg, .mysql = signed_in);
            EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = signed_in);
        }
    }
    CliMysql *const native = server_connected("MYSQL", "orior_native", "native secret");
    s_native_signed_in = native != NULL;
    if (native != NULL)
    {
        EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = native);
    }
    s_wrong_refused = server_connected("MYSQL", "orior_sha2", "not it") == NULL;
    text_stored(s_wrong_code, sizeof(s_wrong_code), s_failure.code);
    statements_run(my, s_table, 3u);
    thousand_written(insert);
    s_insert_run_result = EMBED_CALL(cli_mysql_failure_sql.start, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = insert, .size = CLI_UNITY_LENGTH(insert), .failure = &s_failure);
    s_insert_rows = EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my)->rows;
    text_stored(s_insert_tag, sizeof(s_insert_tag), EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my)->tag);
    s_select_rows = EMBED_CALL(cli_mysql_failure_sql.start, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = s_select, .size = sizeof(s_select) - 1u, .failure = &s_failure) && EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my)->rows;
    const CliDbHead *const head = EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my);
    s_select_columns = head->column_count;
    s_columns_typed = column_is(head, 0u, "n", "int", 1) && column_is(head, 1u, "t", "varchar", 0) && column_is(head, 2u, "b", "binary", 0) && column_is(head, 3u, "at", "timestamp", 0);
    s_rows = 0u;
    (void)EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = my, .count = 400ull, .each_row = row_recorded, .context = NULL, .done = &s_page_done, .tag = tag, .failure = &s_failure);
    s_page_rows = s_rows;
    text_stored(s_page_last, sizeof(s_page_last), s_value[399][0]);
    text_stored(s_page_bytes, sizeof(s_page_bytes), s_value[0][2]);
    s_page_null = s_null[0][3];
    s_rows = 0u;
    (void)EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = my, .count = 1000ull, .each_row = row_recorded, .context = NULL, .done = &s_remainder_done, .tag = tag, .failure = &s_failure);
    s_remainder_rows = s_rows;
    s_begun = EMBED_CALL(cli_mysql_failure_sql.start, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = "BEGIN", .size = 5ull, .failure = &s_failure);
    s_begun_rows = EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my)->rows;
    statements_run(my, s_delete, 1u);
    s_open_in_transaction = EMBED_CALL(cli_mysql_cancel.in_transaction, CliMysqlCancelCfg, .mysql = my);
    statements_run(my, s_rollback, 1u);
    s_rolled_in_transaction = EMBED_CALL(cli_mysql_cancel.in_transaction, CliMysqlCancelCfg, .mysql = my);
    s_missing_run_result = EMBED_CALL(cli_mysql_failure_sql.start, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = "SELECT * FROM orior_test.none", .size = 29ull, .failure = &s_failure);
    text_stored(s_missing_code, sizeof(s_missing_code), s_failure.code);
    s_sleep_consumed = sleep_stopped(my, &s_sleep_returned);
    s_sleep_rows = s_rows;
    text_stored(s_sleep_value, sizeof(s_sleep_value), s_value[0][0]);
    VerbaLinea line = {literal, sizeof(literal), 0ull};
    mmgr_line_put(&line, "SELECT ");
    EMBED_CALL(cli_mysql_text.literal, CliMysqlTextCfg, .text = "it's \\ ok", .out = &line);
    s_rows = 0u;
    s_literal_run_result = EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = literal, .size = line.at, .each_row = row_recorded, .context = NULL, .failure = &s_failure);
    s_literal_rows = s_rows;
    text_stored(s_literal_value, sizeof(s_literal_value), s_value[0][0]);
    statements_run(my, s_drop, 1u);
    EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = my);
}

// the MySQL sequence run, the case ignored with no server named or answering
static void mysql_ready(void)
{
    mysql_run_result();
    if (!s_mysql_named || !s_mysql_answered)
    {
        TEST_IGNORE_MESSAGE("no MySQL answers");
    }
}

// the live MariaDB sequence, once for the process
static void mariadb_run_result(void)
{
    static const char *const s_user[2] = {"DROP USER IF EXISTS 'orior_native'@'%'", "CREATE USER 'orior_native'@'%' IDENTIFIED BY 'native secret'"};
    static const char *const s_drop[1] = {"DROP USER 'orior_native'@'%'"};
    // MariaDB recurses 1000 times at most unless max_recursive_iterations specifies otherwise
    static const char s_select[] = "WITH RECURSIVE k(seq) AS (SELECT 1 UNION ALL SELECT seq + 1 FROM k WHERE seq < 900) "
                                   "SELECT seq, CONCAT('n', seq) AS t FROM k";
    char password[256];
    char tag[CLI_DB_TAG];
    if (s_maria_run_result)
    {
        return;
    }
    s_maria_run_result = 1;
    s_maria_named = password_read("MARIADB", password);
    if (!s_maria_named)
    {
        return;
    }
    CliMysql *const my = server_connected("MARIADB", "root", password);
    s_maria_answered = my != NULL;
    if (my == NULL)
    {
        return;
    }
    statements_run(my, s_user, 2u);
    CliMysql *const native = server_connected("MARIADB", "orior_native", "native secret");
    s_maria_signed_in = native != NULL;
    if (native != NULL)
    {
        EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = native);
    }
    s_maria_wrong_refused = server_connected("MARIADB", "orior_native", "not it") == NULL;
    text_stored(s_maria_wrong_code, sizeof(s_maria_wrong_code), s_failure.code);
    s_maria_select_rows = EMBED_CALL(cli_mysql_failure_sql.start, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = s_select, .size = sizeof(s_select) - 1u, .failure = &s_failure) && EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my)->rows;
    const CliDbHead *const head = EMBED_CALL(cli_mysql_cancel.head, CliMysqlCancelCfg, .mysql = my);
    s_maria_columns = head->column_count;
    s_maria_columns_typed = column_is(head, 0u, "seq", NULL, 1) && column_is(head, 1u, "t", NULL, 0);
    s_rows = 0u;
    (void)EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = my, .count = 600ull, .each_row = row_recorded, .context = NULL, .done = &s_maria_page_done, .tag = tag, .failure = &s_failure);
    s_maria_page_rows = s_rows;
    text_stored(s_maria_page_last, sizeof(s_maria_page_last), s_value[599][1]);
    s_rows = 0u;
    (void)EMBED_CALL(cli_mysql_failure.rows, CliMysqlFailureCfg, .mysql_out = my, .count = 600ull, .each_row = row_recorded, .context = NULL, .done = &s_maria_remainder_done, .tag = tag, .failure = &s_failure);
    s_maria_remainder_rows = s_rows;
    // MariaDB fails a statement its KILL QUERY stops, where MySQL has SLEEP return 1
    s_maria_sleep_consumed = sleep_stopped(my, &s_maria_sleep_returned);
    text_stored(s_maria_sleep_code, sizeof(s_maria_sleep_code), s_failure.code);
    s_rows = 0u;
    s_maria_after_run_result = EMBED_CALL(cli_mysql_failure_sql.all, CliMysqlFailureSqlCfg, .mysql_out = my, .sql = "SELECT 2", .size = 8ull, .each_row = row_recorded, .context = NULL, .failure = &s_failure);
    text_stored(s_maria_after_value, sizeof(s_maria_after_value), s_value[0][0]);
    statements_run(my, s_drop, 1u);
    EMBED_CALL(cli_mysql_failure_sql.close, CliMysqlFailureSqlCfg, .mysql_out = my);
}

// the MariaDB sequence run, the case ignored with no server named or answering
static void mariadb_ready(void)
{
    mariadb_run_result();
    if (!s_maria_named || !s_maria_answered)
    {
        TEST_IGNORE_MESSAGE("no MariaDB answers");
    }
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_power_is_computed_by_its_modulus(void)
{
    static const unsigned char s_four[1] = {4u};
    static const unsigned char s_thirteen[1] = {13u};
    static const unsigned char s_modulus[2] = {0x01u, 0xf1u};
    static const unsigned char s_requested[2] = {0x01u, 0xbdu};
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[0], .bytes = s_four, .size = 1ull);
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[1], .bytes = s_thirteen, .size = 1ull);
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[2], .bytes = s_modulus, .size = 2ull);
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[3], .bytes = s_requested, .size = 2ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_mysql_out_big_internal.big_pow_mod, CliMysqlOutBigCtx, .base = &s_big[0], .exponent = &s_big[1], .modulus = &s_big[2], .out_big_pointer = &s_big[4]));
    TEST_ASSERT_TRUE(numbers_same(&s_big[4], &s_big[3]));
}

void test_a_products_remainder_is_the_remainders_products(void)
{
    unsigned char big[64];
    unsigned char modulus[40];
    memor.set(&(MemoriaCfg){.dst = big, .val = 0xff, .bytes = sizeof(big)});
    memor.set(&(MemoriaCfg){.dst = modulus, .val = 0xfd, .bytes = sizeof(modulus)});
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[0], .bytes = big, .size = sizeof(big));
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[1], .bytes = modulus, .size = sizeof(modulus));
    (void)EMBED_CALL(cli_mysql_a_internal.big_mul, CliMysqlACtx, .a = &s_big[0], .b = &s_big[0], .out_big_pointer = &s_big[2]);
    EMBED_CALL(cli_mysql_big_internal.big_rem, CliMysqlBigCtx, .big = &s_big[2], .modulus = &s_big[1], .out_big_pointer = &s_big[3]);
    EMBED_CALL(cli_mysql_big_internal.big_rem, CliMysqlBigCtx, .big = &s_big[0], .modulus = &s_big[1], .out_big_pointer = &s_big[4]);
    (void)EMBED_CALL(cli_mysql_a_internal.big_mul, CliMysqlACtx, .a = &s_big[4], .b = &s_big[4], .out_big_pointer = &s_big[5]);
    EMBED_CALL(cli_mysql_big_internal.big_rem, CliMysqlBigCtx, .big = &s_big[5], .modulus = &s_big[1], .out_big_pointer = &s_big[2]);
    TEST_ASSERT_TRUE(numbers_same(&s_big[3], &s_big[2]));
}

void test_a_number_writes_back_padded_to_its_length(void)
{
    static const unsigned char s_five[5] = {1u, 2u, 3u, 4u, 5u};
    static const unsigned char s_six[6] = {0u, 1u, 2u, 3u, 4u, 5u};
    unsigned char out[6];
    (void)EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_big[0], .bytes = s_five, .size = 5ull);
    EMBED_CALL(cli_mysql_big_out_internal.big_to_be, CliMysqlBigOutCtx, .big = &s_big[0], .out = out, .length = 6ull);
    TEST_ASSERT_EQUAL_MEMORY(s_six, out, 6u);
}

void test_a_padded_message_starts_with_a_zero(void)
{
    unsigned char seed[20];
    unsigned char padded[128];
    memor.set(&(MemoriaCfg){.dst = seed, .val = 7, .bytes = sizeof(seed)});
    EMBED_CALL(cli_mysql_size_length_internal.oaep, CliMysqlSizeLengthCtx, .message = (const unsigned char *)"secret", .size = 7ull, .seed = seed, .length = 128ull, .padded = padded);
    TEST_ASSERT_EQUAL_UINT8(0u, padded[0]);
}

void test_an_oaep_seed_unmasks(void)
{
    unsigned char seed[20];
    unsigned char padded[128];
    unsigned char mask[128];
    memor.set(&(MemoriaCfg){.dst = seed, .val = 7, .bytes = sizeof(seed)});
    EMBED_CALL(cli_mysql_size_length_internal.oaep, CliMysqlSizeLengthCtx, .message = (const unsigned char *)"secret", .size = 7ull, .seed = seed, .length = 128ull, .padded = padded);
    EMBED_CALL(cli_mysql_size_out_internal.mgf1, CliMysqlSizeOutCtx, .seed = padded + 21, .size = 107ull, .out = mask, .length = 20ull);
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8(seed[at], padded[1u + at] ^ mask[at]);
    }
}

void test_an_oaep_block_is_the_empty_labels_hash_then_a_one_and_the_message(void)
{
    unsigned char seed[20];
    unsigned char padded[128];
    unsigned char mask[128];
    unsigned char block[107];
    unsigned char empty[20];
    memor.set(&(MemoriaCfg){.dst = seed, .val = 7, .bytes = sizeof(seed)});
    EMBED_CALL(cli_mysql_size_length_internal.oaep, CliMysqlSizeLengthCtx, .message = (const unsigned char *)"secret", .size = 7ull, .seed = seed, .length = 128ull, .padded = padded);
    EMBED_CALL(cli_mysql_size_out_internal.mgf1, CliMysqlSizeOutCtx, .seed = seed, .size = 20ull, .out = mask, .length = 107ull);
    for (unsigned int at = 0u; at < 107u; at += 1u)
    {
        // the exclusive or of two bytes is a byte
        block[at] = (unsigned char)(padded[21u + at] ^ mask[at]);
    }
    EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = "", .size = 0ull, .digest = empty);
    TEST_ASSERT_EQUAL_MEMORY(empty, block, 20u);
    TEST_ASSERT_EQUAL_MEMORY("\x01secret\0", block + 107u - 8u, 8u);
}

void test_mysql_native_passwords_answer_is_mysqls(void)
{
    static const unsigned char s_scramble[20] = "abcdefghijklmnopqrst";
    unsigned char once[20];
    unsigned char twice[20];
    unsigned char joined[40];
    unsigned char check[20];
    unsigned char answer[32];
    EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = "secret", .size = 6ull, .digest = once);
    EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = once, .size = 20ull, .digest = twice);
    memor.cpy(&(MemoriaCfg){.dst = joined, .src = s_scramble, .bytes = 20u});
    memor.cpy(&(MemoriaCfg){.dst = joined + 20, .src = twice, .bytes = 20u});
    EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = joined, .size = 40ull, .digest = check);
    TEST_ASSERT_EQUAL_UINT(20u, EMBED_CALL(cli_mysql_size_password_internal.answer, CliMysqlSizePasswordCtx, .password = "secret", .scramble = s_scramble, .size = 20ull, .caching = 0, .answer = answer));
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8(once[at], answer[at] ^ check[at]);
    }
}

void test_an_empty_password_answers_nothing(void)
{
    static const unsigned char s_scramble[20] = "abcdefghijklmnopqrst";
    unsigned char answer[32];
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_mysql_size_password_internal.answer, CliMysqlSizePasswordCtx, .password = "", .scramble = s_scramble, .size = 20ull, .caching = 0, .answer = answer));
}

void test_caching_sha2_passwords_answer_is_32_bytes(void)
{
    static const unsigned char s_scramble[20] = "abcdefghijklmnopqrst";
    unsigned char answer[32];
    TEST_ASSERT_EQUAL_UINT(32u, EMBED_CALL(cli_mysql_size_password_internal.answer, CliMysqlSizePasswordCtx, .password = "secret", .scramble = s_scramble, .size = 20ull, .caching = 1, .answer = answer));
}

void test_mysql_caching_sha2_signs_in_sealed_then_from_the_servers_cache(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_sha2_signed_in[0]);
    TEST_ASSERT_TRUE(s_sha2_sealed[0]);
    TEST_ASSERT_TRUE(s_sha2_signed_in[1]);
    TEST_ASSERT_FALSE(s_sha2_sealed[1]);
}

void test_mysql_native_password_signs_in(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_native_signed_in);
}

void test_mysql_a_wrong_password_is_refused(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_wrong_refused);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_wrong_code, .other = "1045", .cap = CLI_POOL_BYTES}));
}

void test_mysql_an_insert_reports_the_rows_it_affected(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_insert_run_result);
    TEST_ASSERT_FALSE(s_insert_rows);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_insert_tag, .other = "1000 rows affected", .cap = CLI_POOL_BYTES}));
}

void test_mysql_a_query_lists_its_columns_each_of_its_type(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_select_rows);
    TEST_ASSERT_EQUAL_UINT(4u, s_select_columns);
    TEST_ASSERT_TRUE(s_columns_typed);
}

void test_mysql_a_page_of_rows_comes(void)
{
    mysql_ready();
    TEST_ASSERT_EQUAL_UINT(400u, s_page_rows);
    TEST_ASSERT_FALSE(s_page_done);
    TEST_ASSERT_EQUAL_STRING("400", s_page_last);
}

void test_mysql_bytes_read_as_hex_and_a_null_as_none(void)
{
    mysql_ready();
    TEST_ASSERT_EQUAL_STRING("0x00FF", s_page_bytes);
    TEST_ASSERT_TRUE(s_page_null);
}

void test_mysql_the_remainder_of_the_rows_come(void)
{
    mysql_ready();
    TEST_ASSERT_EQUAL_UINT(600u, s_remainder_rows);
    TEST_ASSERT_TRUE(s_remainder_done);
}

void test_mysql_a_transaction_is_open_until_rolled_back(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_begun);
    TEST_ASSERT_FALSE(s_begun_rows);
    TEST_ASSERT_TRUE(s_open_in_transaction);
    TEST_ASSERT_FALSE(s_rolled_in_transaction);
}

void test_mysql_a_failure_has_the_servers_code(void)
{
    mysql_ready();
    TEST_ASSERT_FALSE(s_missing_run_result);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_missing_code, .other = "1146", .cap = CLI_POOL_BYTES}));
}

void test_mysql_a_sleep_stopped_returns_1_at_once(void)
{
    mysql_ready();
    TEST_ASSERT_LESS_THAN_UINT64(10000ull, s_sleep_consumed);
    TEST_ASSERT_TRUE(s_sleep_returned);
    TEST_ASSERT_EQUAL_UINT(1u, s_sleep_rows);
    TEST_ASSERT_EQUAL_STRING("1", s_sleep_value);
}

void test_mysql_a_literal_reads_back_as_itself(void)
{
    mysql_ready();
    TEST_ASSERT_TRUE(s_literal_run_result);
    TEST_ASSERT_EQUAL_UINT(1u, s_literal_rows);
    TEST_ASSERT_EQUAL_STRING("it's \\ ok", s_literal_value);
}

void test_mariadb_an_account_signs_in(void)
{
    mariadb_ready();
    TEST_ASSERT_TRUE(s_maria_signed_in);
}

void test_mariadb_a_wrong_password_is_refused(void)
{
    mariadb_ready();
    TEST_ASSERT_TRUE(s_maria_wrong_refused);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_maria_wrong_code, .other = "1045", .cap = CLI_POOL_BYTES}));
}

void test_mariadb_a_query_lists_its_columns(void)
{
    mariadb_ready();
    TEST_ASSERT_TRUE(s_maria_select_rows);
    TEST_ASSERT_EQUAL_UINT(2u, s_maria_columns);
    TEST_ASSERT_TRUE(s_maria_columns_typed);
}

void test_mariadb_a_page_of_rows_comes(void)
{
    mariadb_ready();
    TEST_ASSERT_EQUAL_UINT(600u, s_maria_page_rows);
    TEST_ASSERT_FALSE(s_maria_page_done);
    TEST_ASSERT_EQUAL_STRING("n600", s_maria_page_last);
}

void test_mariadb_the_remainder_of_the_rows_come(void)
{
    mariadb_ready();
    TEST_ASSERT_EQUAL_UINT(300u, s_maria_remainder_rows);
    TEST_ASSERT_TRUE(s_maria_remainder_done);
}

void test_mariadb_a_sleep_stopped_fails_at_once(void)
{
    mariadb_ready();
    TEST_ASSERT_LESS_THAN_UINT64(10000ull, s_maria_sleep_consumed);
    TEST_ASSERT_FALSE(s_maria_sleep_returned);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_maria_sleep_code, .other = "1317", .cap = CLI_POOL_BYTES}));
}

void test_mariadb_the_connection_answers_after_a_stopped_sleep(void)
{
    mariadb_ready();
    TEST_ASSERT_TRUE(s_maria_after_run_result);
    TEST_ASSERT_EQUAL_STRING("2", s_maria_after_value);
}
