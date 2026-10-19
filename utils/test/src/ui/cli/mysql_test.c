// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_test.c: MySQL's check: a power is taken by its modulus, OAEP unmasks to its message, the scrambles are the ones
// MySQL computes, each sign-in works and a query's rows come a page at a time, and MariaDB signs in, reads rows and
// stops a query. The last two run on the server orior's tests sign in to, where one runs: MySQL, or MariaDB where
// `name` is MARIADB, at ORIOR_<name>_ADDRESS, else on 127.0.0.1:3306, with the password ORIOR_<name>_PASSWORD names
// for root
#include "../../../../../ui/cli/src/databases/mysql.h"
#include "../../../../../ui/cli/src/databases/mysql_internal.h"
#include "../../../../../ui/cli/src/text/digest.h"
#include "cli_check.h"

// the rows of a page kept, the first values of each, and the bytes they take
#define MYSQL_ROWS 1000u
#define MYSQL_VALUES 4u
#define MYSQL_VALUE 64u

static CliBig s_big[6];
static CliDbFailure s_failure;
static char s_value[MYSQL_ROWS][MYSQL_VALUES][MYSQL_VALUE];
static int s_null[MYSQL_ROWS][MYSQL_VALUES];
static unsigned int s_rows;

// whether two numbers are the same
static int mysql_same(const CliBig *one, const CliBig *other)
{
    return (one->count == other->count) &&
           (memor.cmp(&(MemoriaCfg){
                .src = one->word, .other = other->word, .bytes = one->count * sizeof(one->word[0])}) == 0);
}

// a power is taken by its modulus
static void mysql_power(void)
{
    static const unsigned char s_four[1] = {4u};
    static const unsigned char s_thirteen[1] = {13u};
    static const unsigned char s_modulus[2] = {0x01u, 0xf1u};
    static const unsigned char s_wanted[2] = {0x01u, 0xbdu};
    (void)cli_big_from_be(&s_big[0], s_four, 1ull);
    (void)cli_big_from_be(&s_big[1], s_thirteen, 1ull);
    (void)cli_big_from_be(&s_big[2], s_modulus, 2ull);
    (void)cli_big_from_be(&s_big[3], s_wanted, 2ull);
    cli_check(cli_big_pow_mod(&s_big[0], &s_big[1], &s_big[2], &s_big[4]) && mysql_same(&s_big[4], &s_big[3]),
              "a power is taken by its modulus");
    unsigned char big[64];
    unsigned char modulus[40];
    memor.set(&(MemoriaCfg){.dst = big, .val = 0xff, .bytes = sizeof(big)});
    memor.set(&(MemoriaCfg){.dst = modulus, .val = 0xfd, .bytes = sizeof(modulus)});
    (void)cli_big_from_be(&s_big[0], big, sizeof(big));
    (void)cli_big_from_be(&s_big[1], modulus, sizeof(modulus));
    (void)cli_big_mul(&s_big[0], &s_big[0], &s_big[2]);
    cli_big_rem(&s_big[2], &s_big[1], &s_big[3]);
    cli_big_rem(&s_big[0], &s_big[1], &s_big[4]);
    (void)cli_big_mul(&s_big[4], &s_big[4], &s_big[5]);
    cli_big_rem(&s_big[5], &s_big[1], &s_big[2]);
    cli_check(mysql_same(&s_big[3], &s_big[2]), "a product's remainder is the remainders' product's");
    static const unsigned char s_five[5] = {1u, 2u, 3u, 4u, 5u};
    static const unsigned char s_six[6] = {0u, 1u, 2u, 3u, 4u, 5u};
    unsigned char out[6];
    (void)cli_big_from_be(&s_big[0], s_five, 5ull);
    cli_big_to_be(&s_big[0], out, 6ull);
    cli_check(memor.cmp(&(MemoriaCfg){.src = out, .other = s_six, .bytes = 6u}) == 0,
              "a number writes back padded to its length");
}

// OAEP unmasks to its message
static void mysql_oaep(void)
{
    unsigned char seed[20];
    unsigned char padded[128];
    unsigned char mask[128];
    unsigned char block[107];
    unsigned char empty[20];
    memor.set(&(MemoriaCfg){.dst = seed, .val = 7, .bytes = sizeof(seed)});
    cli_mysql_oaep((const unsigned char *)"secret", 7ull, seed, 128ull, padded);
    cli_check(padded[0] == 0u, "a padded message starts with a zero");
    cli_mysql_mgf1(padded + 21, 107ull, mask, 20ull);
    int seed_back = 1;
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        seed_back &= (padded[1u + at] ^ mask[at]) == seed[at];
    }
    cli_check(seed_back, "its seed unmasks");
    cli_mysql_mgf1(seed, 20ull, mask, 107ull);
    for (unsigned int at = 0u; at < 107u; at += 1u)
    {
        block[at] = (unsigned char)(padded[21u + at] ^ mask[at]);
    }
    cli_sha1_bytes("", 0ull, empty);
    cli_check((memor.cmp(&(MemoriaCfg){.src = block, .other = empty, .bytes = 20u}) == 0) &&
                  (memor.cmp(&(MemoriaCfg){.src = block + 107u - 8u, .other = "\x01secret\0", .bytes = 8u}) == 0),
              "and its block is the empty label's hash, then a one and the message");
}

// the scrambles are the ones MySQL computes
static void mysql_scrambles(void)
{
    static const unsigned char s_scramble[20] = "abcdefghijklmnopqrst";
    unsigned char once[20];
    unsigned char twice[20];
    unsigned char joined[40];
    unsigned char check[20];
    unsigned char answer[32];
    cli_sha1_bytes("secret", 6ull, once);
    cli_sha1_bytes(once, 20ull, twice);
    memor.cpy(&(MemoriaCfg){.dst = joined, .src = s_scramble, .bytes = 20u});
    memor.cpy(&(MemoriaCfg){.dst = joined + 20, .src = twice, .bytes = 20u});
    cli_sha1_bytes(joined, 40ull, check);
    const unsigned int length = cli_mysql_answer("secret", s_scramble, 20ull, 0, answer);
    int back = length == 20u;
    for (unsigned int at = 0u; back && (at < 20u); at += 1u)
    {
        back = (answer[at] ^ check[at]) == once[at];
    }
    cli_check(back, "mysql_native_password's answer is MySQL's");
    cli_check(cli_mysql_answer("", s_scramble, 20ull, 0, answer) == 0u, "an empty password answers nothing");
    cli_check(cli_mysql_answer("secret", s_scramble, 20ull, 1, answer) == 32u, "caching_sha2_password's is 32 bytes");
}

// a row kept, its first MYSQL_VALUES values
static int mysql_kept(void *context, const CliDbValue *values, unsigned int count)
{
    (void)context;
    for (unsigned int at = 0u; (s_rows < MYSQL_ROWS) && (at < count) && (at < MYSQL_VALUES); at += 1u)
    {
        s_null[s_rows][at] = values[at].null;
        const unsigned long long size = values[at].null ? 0ull : values[at].size;
        const unsigned long long kept = (size < MYSQL_VALUE - 1u) ? size : MYSQL_VALUE - 1u;
        memor.cpy(
            &(MemoriaCfg){.dst = s_value[s_rows][at], .src = values[at].null ? "" : values[at].text, .bytes = kept});
        s_value[s_rows][at][kept] = '\0';
    }
    s_rows += (s_rows < MYSQL_ROWS) ? 1u : 0u;
    return 1;
}

// What stops a statement, sent from a thread of its own once 400 milliseconds have passed
typedef struct
{
    CliMysqlCancel cancel;
    CliThread thread;
} MysqlStopper;

static MysqlStopper s_stopper;

static void mysql_stop(void *context)
{
    MysqlStopper *const stopper = context;
    query_paused(400u);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_mysql_cancel_sent(&stopper->cancel, &said);
}

// every statement of `count` run on `my`, each read whole
static void mysql_all(CliMysql *my, const char *const *sql, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_rows = 0u;
        (void)cli_mysql_all(my, sql[at], cellul.len(&(CatenaFinitaCfg){.src = sql[at], .cap = CLI_CELLS_POOL}),
                            mysql_kept, NULL, &s_failure);
    }
}

// whether the column `at` of the head is `name` of `kind`, a number where `numeric`, the kind not held to where NULL
static int mysql_column(const CliDbHead *head, unsigned int at, const char *name, const char *kind, int numeric)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = head->columns[at].name, .other = name, .cap = CLI_CELLS_POOL})) &&
           ((kind == NULL) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = head->columns[at].kind, .other = kind, .cap = CLI_CELLS_POOL}))) &&
           (head->columns[at].numeric == numeric);
}

// the server `name` names connected to as `user`, NULL where none answers or none is named
static CliMysql *mysql_server(const char *name, const char *user, const char *password)
{
    char host[CLI_NAME_LONGEST];
    unsigned int port = 0u;
    cli_check_server(name, 3306u, host, &port);
    return cli_mysql_connected(host, port, user, password, "", 5000ull, &s_failure);
}

// the password ORIOR_<name>_PASSWORD names into `password`, which holds 256: 1, or 0 where it names none
static int mysql_password(const char *name, char *password)
{
    char variable[64];
    CliLine line = {variable, sizeof(variable), 0ull};
    cli_line_text(&line, "ORIOR_");
    cli_line_text(&line, name);
    cli_line_text(&line, "_PASSWORD");
    (void)cli_line_closed(&line);
    return query_environment_read(variable, password, 256u);
}

// the sleep started on `my` stopped by a cancel from a thread of its own: the milliseconds it took, and whether it
// gave rows
static unsigned long long mysql_stopped(CliMysql *my, int *gave)
{
    cli_mysql_canceller(my, &s_stopper.cancel);
    s_stopper.thread = (CliThread){mysql_stop, &s_stopper};
    (void)cli_thread_started(&s_stopper.thread);
    const unsigned long long started = cli_clock_milliseconds();
    s_rows = 0u;
    *gave = cli_mysql_all(my, "SELECT SLEEP(30)", 16ull, mysql_kept, NULL, &s_failure);
    return cli_clock_milliseconds() - started;
}

// the INSERT of a thousand rows numbered from 1, each with `x`, two bytes and no time, into `out`, which holds 2048
static void mysql_thousand(char *out)
{
    CliLine line = {out, 2048u, 0ull};
    cli_line_text(&line, "INSERT INTO orior_test.n SELECT 1 + a.d + 10 * b.d + 100 * c.d, 'x', 0x00FF, NULL FROM ");
    static const char *const s_names[3] = {"a", "b", "c"};
    for (unsigned int table = 0u; table < 3u; table += 1u)
    {
        cli_line_text(&line, (table > 0u) ? ", (SELECT 0 d" : "(SELECT 0 d");
        for (unsigned int digit = 1u; digit < 10u; digit += 1u)
        {
            cli_line_text(&line, " UNION SELECT ");
            cli_line_decimal(&line, digit, 1u);
        }
        cli_line_text(&line, ") ");
        cli_line_text(&line, s_names[table]);
    }
    (void)cli_line_closed(&line);
}

// each sign-in works and a query's rows come a page at a time
static void mysql_mysql(const char *password)
{
    static const char *const s_users[4] = {
        "DROP USER IF EXISTS 'orior_sha2'@'%', 'orior_native'@'%'",
        "CREATE USER 'orior_sha2'@'%' IDENTIFIED WITH caching_sha2_password BY 'sha2 secret'",
        "CREATE USER 'orior_native'@'%' IDENTIFIED WITH mysql_native_password BY 'native secret'", "FLUSH PRIVILEGES"};
    static const char *const s_table[3] = {
        "CREATE DATABASE IF NOT EXISTS orior_test",
        "CREATE TABLE IF NOT EXISTS orior_test.n (n INT, t VARCHAR(10), b VARBINARY(4), at TIMESTAMP NULL)",
        "TRUNCATE orior_test.n"};
    CliMysql *const my = mysql_server("MYSQL", "root", password);
    if (my == NULL)
    {
        cli_check(1, "no MySQL answers, and no rows are read");
        return;
    }
    mysql_all(my, s_users, 4u);
    // the first sign-in after the flush takes the password whole, sealed by the server's key; the second is answered
    // from the server's cache
    for (int sealed = 1; sealed >= 0; sealed -= 1)
    {
        CliMysql *const signed_in = mysql_server("MYSQL", "orior_sha2", "sha2 secret");
        cli_check((signed_in != NULL) && (cli_mysql_sealed(signed_in) == sealed),
                  sealed ? "caching_sha2_password signs in sealed" : "and then from the server's cache");
        if (signed_in != NULL)
        {
            cli_mysql_closed(signed_in);
        }
    }
    CliMysql *const native = mysql_server("MYSQL", "orior_native", "native secret");
    cli_check(native != NULL, "mysql_native_password signs in");
    if (native != NULL)
    {
        cli_mysql_closed(native);
    }
    cli_check((mysql_server("MYSQL", "orior_sha2", "not it") == NULL) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_failure.code, .other = "1045", .cap = CLI_CELLS_POOL})),
              "a wrong password is refused");
    mysql_all(my, s_table, 3u);
    char insert[2048];
    mysql_thousand(insert);
    cli_check(
        cli_mysql_start(my, insert, cellul.len(&(CatenaFinitaCfg){.src = insert, .cap = CLI_CELLS_POOL}), &s_failure) &&
            !cli_mysql_head(my)->rows &&
            (cellul.starts(&(CatenaFinitaCfg){
                .src = cli_mysql_head(my)->tag, .other = "1000 rows affected", .cap = CLI_CELLS_POOL})),
        "an insert tells the rows it affected");
    static const char s_select[] = "SELECT n, t, b, at FROM orior_test.n ORDER BY n";
    const int rows = cli_mysql_start(my, s_select, sizeof(s_select) - 1u, &s_failure) && cli_mysql_head(my)->rows;
    const CliDbHead *const head = cli_mysql_head(my);
    cli_check(rows && (head->column_count == 4u) && mysql_column(head, 0u, "n", "int", 1) &&
                  mysql_column(head, 1u, "t", "varchar", 0) && mysql_column(head, 2u, "b", "binary", 0) &&
                  mysql_column(head, 3u, "at", "timestamp", 0),
              "a query gives its columns, each of its type");
    int done = 0;
    char tag[CLI_DB_TAG];
    s_rows = 0u;
    (void)cli_mysql_rows(my, 400ull, mysql_kept, NULL, &done, tag, &s_failure);
    cli_check((s_rows == 400u) && !done &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_value[399][0], .other = "400", .cap = CLI_CELLS_POOL})),
              "a page of rows comes");
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = s_value[0][2], .other = "0x00FF", .cap = CLI_CELLS_POOL})) &&
                  s_null[0][3],
              "bytes as hex and a null as none");
    s_rows = 0u;
    (void)cli_mysql_rows(my, 1000ull, mysql_kept, NULL, &done, tag, &s_failure);
    cli_check((s_rows == 600u) && done, "and the rest");
    cli_check(cli_mysql_start(my, "BEGIN", 5ull, &s_failure) && !cli_mysql_head(my)->rows, "a transaction begins");
    static const char *const s_delete[1] = {"DELETE FROM orior_test.n WHERE n = 1"};
    mysql_all(my, s_delete, 1u);
    cli_check(cli_mysql_in_transaction(my), "and is open");
    static const char *const s_rollback[1] = {"ROLLBACK"};
    mysql_all(my, s_rollback, 1u);
    cli_check(!cli_mysql_in_transaction(my), "until it is rolled back");
    cli_check(!cli_mysql_start(my, "SELECT * FROM orior_test.none", 29ull, &s_failure) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_failure.code, .other = "1146", .cap = CLI_CELLS_POOL})),
              "a failure gives the server's code");
    int gave = 0;
    const unsigned long long took = mysql_stopped(my, &gave);
    cli_check((took < 10000ull) && gave && (s_rows == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_value[0][0], .other = "1", .cap = CLI_CELLS_POOL})),
              "a sleep stopped gives 1, at once");
    char literal[64];
    CliLine line = {literal, sizeof(literal), 0ull};
    cli_line_text(&line, "SELECT ");
    cli_mysql_literal("it's \\ ok", &line);
    s_rows = 0u;
    cli_check(cli_mysql_all(my, literal, line.at, mysql_kept, NULL, &s_failure) && (s_rows == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_value[0][0], .other = "it's \\ ok", .cap = CLI_CELLS_POOL})),
              "a literal reads back as itself");
    static const char *const s_drop[1] = {"DROP DATABASE orior_test"};
    mysql_all(my, s_drop, 1u);
    cli_mysql_closed(my);
}

// MariaDB signs in, reads rows and stops a query
static void mysql_mariadb(const char *password)
{
    static const char *const s_user[2] = {"DROP USER IF EXISTS 'orior_native'@'%'",
                                          "CREATE USER 'orior_native'@'%' IDENTIFIED BY 'native secret'"};
    CliMysql *const my = mysql_server("MARIADB", "root", password);
    if (my == NULL)
    {
        cli_check(1, "no MariaDB answers, and no rows are read");
        return;
    }
    mysql_all(my, s_user, 2u);
    CliMysql *const native = mysql_server("MARIADB", "orior_native", "native secret");
    cli_check(native != NULL, "MariaDB's account signs in");
    if (native != NULL)
    {
        cli_mysql_closed(native);
    }
    cli_check((mysql_server("MARIADB", "orior_native", "not it") == NULL) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_failure.code, .other = "1045", .cap = CLI_CELLS_POOL})),
              "a wrong password is refused");
    // MariaDB recurses 1000 times at most unless max_recursive_iterations says otherwise
    static const char s_select[] =
        "WITH RECURSIVE k(seq) AS (SELECT 1 UNION ALL SELECT seq + 1 FROM k WHERE seq < 900) "
        "SELECT seq, CONCAT('n', seq) AS t FROM k";
    const int rows = cli_mysql_start(my, s_select, sizeof(s_select) - 1u, &s_failure) && cli_mysql_head(my)->rows;
    const CliDbHead *const head = cli_mysql_head(my);
    cli_check(rows && (head->column_count == 2u) && mysql_column(head, 0u, "seq", NULL, 1) &&
                  mysql_column(head, 1u, "t", NULL, 0),
              "a query gives its columns");
    int done = 0;
    char tag[CLI_DB_TAG];
    s_rows = 0u;
    (void)cli_mysql_rows(my, 600ull, mysql_kept, NULL, &done, tag, &s_failure);
    cli_check((s_rows == 600u) && !done &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_value[599][1], .other = "n600", .cap = CLI_CELLS_POOL})),
              "a page of rows comes");
    s_rows = 0u;
    (void)cli_mysql_rows(my, 600ull, mysql_kept, NULL, &done, tag, &s_failure);
    cli_check((s_rows == 300u) && done, "and the rest");
    int gave = 0;
    // MariaDB fails a statement its KILL QUERY stops, where MySQL has SLEEP give 1
    const unsigned long long took = mysql_stopped(my, &gave);
    cli_check((took < 10000ull) && !gave &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_failure.code, .other = "1317", .cap = CLI_CELLS_POOL})),
              "a sleep stopped fails, at once");
    s_rows = 0u;
    cli_check(cli_mysql_all(my, "SELECT 2", 8ull, mysql_kept, NULL, &s_failure) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_value[0][0], .other = "2", .cap = CLI_CELLS_POOL})),
              "and the connection answers after it");
    static const char *const s_drop[1] = {"DROP USER 'orior_native'@'%'"};
    mysql_all(my, s_drop, 1u);
    cli_mysql_closed(my);
}

int main(void)
{
    mysql_power();
    mysql_oaep();
    mysql_scrambles();
    char password[256];
    if (mysql_password("MYSQL", password))
    {
        mysql_mysql(password);
    }
    if (mysql_password("MARIADB", password))
    {
        mysql_mariadb(password);
    }
    return cli_checked();
}
