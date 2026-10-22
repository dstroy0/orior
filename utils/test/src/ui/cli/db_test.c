// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_test.c: the database connections' check: databases.json keeps each connection by its name, the keychain keeps a
// password and forgets it, a SQLite database runs, pages, guards and reads its schema, a slow SQLite statement is
// stopped, and every connection closed is given back to the cellblock. The servers' checks are db_servers_test.c's
#include "../../../../../ui/cli/src/databases/db_internal.h"
#include "../../../../../ui/cli/src/tests/test_runs.h"
#include "cli_check.h"

static char s_out[1u << 20u];
static CliDbFailure s_failure;

// the connection `name` of `kind`, the rest empty, into `connection`
static void db_connection(CliDbConnection *connection, const char *name, const char *kind)
{
    memor.set(&(MemoriaCfg){.dst = connection, .val = 0, .bytes = sizeof(*connection)});
    memor.cpy(&(MemoriaCfg){.dst = connection->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = connection->kind,
                            .src = kind,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) + 1u});
}

// the names of the connections the tree names, `|` between them, into `out`, which holds 256
static const char *db_names(const char *root, char *out)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine listed = {s_out, sizeof(s_out), 0ull};
    (void)cli_db_listed(root, NULL, 0u, &listed, &said);
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, listed.at, &list);
    CliLine line = {out, 256u, 0ull};
    for (unsigned int at = 0u; cli_json_item(json, list, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(
            &line, cli_check_member_text(cli_json_member(json, cli_json_item(json, list, at), "connection"), "name"));
    }
    (void)cli_line_closed(&line);
    return out;
}

// databases.json keeps each connection by its name
static void db_json(void)
{
    char root[CLI_PATH_LONGEST];
    char why[256];
    char names[256];
    char path[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("db_json", root);
    CliDbConnection lite;
    db_connection(&lite, "local", "sqlite");
    memor.cpy(&(MemoriaCfg){.dst = lite.file,
                            .src = "data/app.db",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "data/app.db", .cap = CLI_CELLS_POOL}) + 1u});
    cli_check(cli_db_save(root, &lite, NULL, &said), "a connection is kept");
    CliDbConnection pg;
    db_connection(&pg, "prod", "postgres");
    memor.cpy(&(MemoriaCfg){.dst = pg.host,
                            .src = "db.example",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "db.example", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = pg.user,
                            .src = "app",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "app", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = pg.database,
                            .src = "app",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "app", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = pg.ssh,
                            .src = "me@bastion",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "me@bastion", .cap = CLI_CELLS_POOL}) + 1u});
    pg.matters = 1;
    memor.cpy(&(MemoriaCfg){.dst = pg.guard,
                            .src = "read-only",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "read-only", .cap = CLI_CELLS_POOL}) + 1u});
    (void)cli_db_save(root, &pg, NULL, &said);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = db_names(root, names), .other = "local|prod", .cap = CLI_CELLS_POOL}),
              "each by its name");
    CliDbConnection renamed = pg;
    memor.cpy(&(MemoriaCfg){.dst = renamed.name,
                            .src = "local",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "local", .cap = CLI_CELLS_POOL}) + 1u});
    cli_check(!cli_db_save(root, &renamed, "prod", &said), "a name another connection holds is refused");
    memor.cpy(&(MemoriaCfg){.dst = renamed.name,
                            .src = "live",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "live", .cap = CLI_CELLS_POOL}) + 1u});
    (void)cli_db_save(root, &renamed, "prod", &said);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = db_names(root, names), .other = "local|live", .cap = CLI_CELLS_POOL}),
              "a connection renamed keeps its place");
    char text[4096];
    cli_full(root, "databases.json", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text) - 1u);
    text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = text, .cap = CLI_CELLS_POOL, .other = "password", .other_cap = CLI_CELLS_POOL}) == NULL,
              "no password is kept in the file");
    (void)cli_db_remove(root, "local", &said);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = db_names(root, names), .other = "live", .cap = CLI_CELLS_POOL}),
              "a connection is taken out");
    char account[256];
    CliLine line = {account, sizeof(account), 0ull};
    cli_db_account(&pg, &line);
    cli_check_text(account, line.at, "postgres://app@db.example:5432/app via me@bastion",
                   "a connection's account names its server and its tunnel");
}

// the keychain keeps a password and forgets it
static void db_keychain(void)
{
    char account[64];
    CliLine line = {account, sizeof(account), 0ull};
    cli_line_text(&line, "test://orior@");
    cli_line_decimal(&line, cli_process_own(), 1u);
    (void)cli_line_closed(&line);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_keychain_keep("database", account, "kept secret", &said))
    {
        cli_check(1, "the machine has no keychain, and none is kept");
        return;
    }
    char read[64];
    cli_check(cli_keychain_read("database", account, read, sizeof(read)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = read, .other = "kept secret", .cap = CLI_CELLS_POOL})),
              "the keychain keeps a password");
    (void)cli_keychain_forget("database", account, &said);
    cli_check(!cli_keychain_read("database", account, read, sizeof(read)), "and forgets it");
}

// `statement` run on `open`, its answer read into the shared tree: its node, CLI_JSON_NONE where it failed
static unsigned int db_run(CliDbOpen *open, const char *statement, int answered, unsigned long long page,
                           const CliJson **json)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    if (!cli_db_run(open, statement, cellul.len(&(CatenaFinitaCfg){.src = statement, .cap = CLI_CELLS_POOL}), answered,
                    page, &out, &s_failure))
    {
        return CLI_JSON_NONE;
    }
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// a member's value written compact into `out`, which holds 256
static const char *db_written(const CliJson *json, unsigned int node, char *out)
{
    CliLine line = {out, 256u, 0ull};
    cli_json_write(json, node, &line, 0u, 0);
    (void)cli_line_closed(&line);
    return out;
}

// the schema's level under `count` names, read into the shared tree: its node
static unsigned int db_children(CliDbOpen *open, const char *const *path, unsigned int count, const CliJson **json)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_db_children(open, path, count, &out, &s_failure);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// a SQLite database's rows, guards and schema
static void db_sqlite_rows(CliDbOpen *open)
{
    const CliJson *json = NULL;
    char text[256];
    (void)db_run(open, "CREATE TABLE t (n INTEGER PRIMARY KEY, name TEXT NOT NULL DEFAULT 'x', b BLOB)", 1, 100ull,
                 &json);
    (void)db_run(open, "CREATE INDEX t_name ON t (name)", 1, 100ull, &json);
    unsigned int found =
        db_run(open,
               "INSERT INTO t (n, name, b) WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM k "
               "WHERE i < 1000) SELECT i, 'n' || i, x'00ff' FROM k",
               1, 100ull, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(found, "done"), .other = "1000 rows affected", .cap = CLI_CELLS_POOL}),
              "an insert says what it did");
    found = db_run(open, "SELECT n, name, b, NULL AS z FROM t ORDER BY n", 0, 100ull, &json);
    const unsigned int rows = cli_json_member(json, found, "rows");
    cli_check(json->node[rows].count == 100u, "a query's first page comes");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = db_written(
                json,
                cli_json_member(json, cli_json_item(json, cli_json_member(json, found, "columns"), 0u), "numeric"),
                text),
            .other = "true",
            .cap = CLI_CELLS_POOL}),
        "a number's column is numeric");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = db_written(json, cli_json_item(json, rows, 0u), text),
                                           .other = "[\"1\",\"n1\",\"0x00FF\",null]",
                                           .cap = CLI_CELLS_POOL}),
              "each value as the window shows it");
    cli_check(cli_json_is(json, cli_json_member(json, found, "done"), CFG_JSON_NULL), "and the rest wait");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_db_rows(open, 950ull, 100ull, &out, &s_failure);
    found = CLI_JSON_NONE;
    json = cli_check_parsed(s_out, out.at, &found);
    const unsigned int later = cli_json_member(json, found, "rows");
    cli_check((json->node[later].count == 50u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_json_string(json, cli_json_item(json, cli_json_item(json, later, 0u), 0u), NULL),
                      .other = "951",
                      .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(found, "done"), .other = "1000 rows read", .cap = CLI_CELLS_POOL})),
              "a later page is read from the run");
    found = db_run(open, "DELETE FROM t", 0, 100ull, &json);
    const unsigned int ask = cli_json_member(json, found, "ask");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(ask, "why"), .other = "every row", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(ask, "count"), .other = "1000", .cap = CLI_CELLS_POOL})),
              "a delete of every row asks first, with its count");
    found = db_run(open, "SELECT count(*) FROM t", 0, 10ull, &json);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = db_written(
                json, cli_json_item(json, cli_json_item(json, cli_json_member(json, found, "rows"), 0u), 0u), text),
            .other = "\"1000\"",
            .cap = CLI_CELLS_POOL}),
        "and does nothing until answered");
    (void)db_run(open, "BEGIN", 1, 10ull, &json);
    (void)db_run(open, "DELETE FROM t WHERE n > 10", 0, 10ull, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_db_transaction(open), .other = "open", .cap = CLI_CELLS_POOL}),
              "a transaction is open");
    (void)db_run(open, "ROLLBACK", 1, 10ull, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_db_transaction(open), .other = "idle", .cap = CLI_CELLS_POOL}),
              "until it is rolled back");
    cli_check((db_run(open, "SELECT * FROM none", 0, 10ull, &json) == CLI_JSON_NONE) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_failure.message,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "no such table",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a failure says why");
}

// a SQLite database's schema, export and history
static void db_sqlite_schema(CliDbOpen *open, const char *root)
{
    static const char *const s_main[1] = {"main"};
    static const char *const s_table[3] = {"main", "table", "t"};
    const CliJson *json = NULL;
    unsigned int found = db_children(open, NULL, 0u, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, found, 0u), "label"),
                                           .other = "main",
                                           .cap = CLI_CELLS_POOL}),
              "the schema is main");
    found = db_children(open, s_main, 1u, &json);
    cli_check((json->node[found].count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, found, 0u), "label"),
                                                .other = "t",
                                                .cap = CLI_CELLS_POOL})),
              "its objects");
    found = db_children(open, s_table, 3u, &json);
    char shown[256];
    CliLine line = {shown, sizeof(shown), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int part = cli_json_item(json, found, at);
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_check_member_text(part, "kind"));
        cli_line_character(&line, ' ');
        cli_line_text(&line, cli_check_member_text(part, "label"));
    }
    cli_check_text(shown, line.at, "column n|column name|column b|index t_name", "a table's columns and indexes");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, found, 1u), "detail"),
                                           .other = "TEXT not null default 'x'",
                                           .cap = CLI_CELLS_POOL}),
              "each column with its type and default");
    char path[CLI_PATH_LONGEST];
    cli_full(root, "t.csv", path);
    (void)db_run(open, "SELECT n, name FROM t ORDER BY n", 0, 10ull, &json);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_db_export(open, path, "csv", NULL, NULL, &out, &s_failure);
    found = CLI_JSON_NONE;
    json = cli_check_parsed(s_out, out.at, &found);
    char text[256];
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = db_written(json, cli_json_member(json, found, "written"), text),
                                           .other = "1000",
                                           .cap = CLI_CELLS_POOL}),
              "an export writes every row");
    static char s_csv[1u << 16u];
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_csv, sizeof(s_csv) - 1u);
    s_csv[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    unsigned int lines = 0u;
    for (const char *at = s_csv; at[0] != '\0'; at += 1)
    {
        lines += (at[0] == '\n') ? 1u : 0u;
    }
    cli_check((lines == 1001u) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_csv, .other = "n,name\n1,n1\n", .cap = CLI_CELLS_POOL})),
              "with its head");
    cli_full(root, "t.jsonl", path);
    out.at = 0ull;
    (void)cli_db_export(open, path, "json", NULL, NULL, &out, &s_failure);
    const unsigned long long lines_size = qry_member_file_read(path, (unsigned char *)s_csv, sizeof(s_csv) - 1u);
    s_csv[(lines_size == QRY_MEMBER_UNREAD) ? 0u : lines_size] = '\0';
    cli_check(cellul.starts(
                  &(CatenaFinitaCfg){.src = s_csv, .other = "{\"n\":\"1\",\"name\":\"n1\"}\n", .cap = CLI_CELLS_POOL}),
              "and as JSON lines, each row an object of its columns");
    out.at = 0ull;
    cli_db_history(open, &out);
    (void)cli_line_closed(&out);
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                             .cap = CLI_CELLS_POOL,
                                             .other = "\"sql\":\"SELECT * FROM none\"",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "each statement is kept in the history");
    found = db_run(open, "ALTER TABLE t ADD COLUMN extra TEXT", 1, 10ull, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, found, "shaped"), "name"),
                                           .other = "t",
                                           .cap = CLI_CELLS_POOL}),
              "a statement that shapes the schema names its object");
}

// a SQLite database runs, pages, guards and reads its schema
static void db_sqlite(void)
{
    char python[CLI_PATH_LONGEST];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and SQLite is not opened");
        return;
    }
    char root[CLI_PATH_LONGEST];
    cli_check_folder("db_sqlite", root);
    CliDbConnection connection;
    db_connection(&connection, "lite", "sqlite");
    memor.cpy(&(MemoriaCfg){.dst = connection.file,
                            .src = "app.db",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "app.db", .cap = CLI_CELLS_POOL}) + 1u});
    CliDbOpen *const open = cli_db_connect(root, &connection, NULL, &s_failure);
    cli_check((open != NULL) && (cli_db_version(open)[0] != '\0'), "a SQLite database opens with its version");
    if (open == NULL)
    {
        return;
    }
    db_sqlite_rows(open);
    db_sqlite_schema(open, root);
    cli_db_close(open);
}

// a stop sent from a thread of its own once half a second has passed
static void db_stop(void *context)
{
    query_paused(500u);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_db_stop((CliDbOpen *)context, &said);
}

// a slow SQLite statement is stopped
static void db_slow(void)
{
    char python[CLI_PATH_LONGEST];
    if (!cli_test_python(python))
    {
        return;
    }
    char root[CLI_PATH_LONGEST];
    cli_check_folder("db_stop", root);
    CliDbConnection connection;
    db_connection(&connection, "lite", "sqlite");
    memor.cpy(&(MemoriaCfg){.dst = connection.file,
                            .src = "slow.db",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "slow.db", .cap = CLI_CELLS_POOL}) + 1u});
    CliDbOpen *const open = cli_db_connect(root, &connection, NULL, &s_failure);
    if (open == NULL)
    {
        cli_check(0, "the slow database opens");
        return;
    }
    static CliThread s_thread;
    s_thread = (CliThread){db_stop, open};
    (void)cli_thread_started(&s_thread);
    const unsigned long long started = cli_clock_milliseconds();
    const CliJson *json = NULL;
    cli_check(
        (db_run(open, "WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM k) SELECT count(*) FROM k", 0,
                10ull, &json) == CLI_JSON_NONE) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_failure.message, .cap = CLI_CELLS_POOL, .other = "interrupt", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (cli_clock_milliseconds() - started < 10000ull),
        "a slow statement is stopped at once");
    char text[256];
    const unsigned int found = db_run(open, "SELECT 1", 0, 10ull, &json);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = db_written(
                json, cli_json_item(json, cli_json_item(json, cli_json_member(json, found, "rows"), 0u), 0u), text),
            .other = "\"1\"",
            .cap = CLI_CELLS_POOL}),
        "and the connection answers after it");
    cli_db_close(open);
}

int main(void)
{
    db_json();
    db_keychain();
    db_sqlite();
    db_slow();
    char python[CLI_PATH_LONGEST];
    cli_check((g_cli_db_cells.holds == 0ull) && ((g_cli_db_cells.hw != 0ull) || !cli_test_python(python)),
              "every connection and SQLite process closed is given back, the most they held marked");
    return cli_checked();
}
