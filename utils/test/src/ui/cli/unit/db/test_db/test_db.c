// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_db.c: databases.json stores each connection by its name, the keychain stores a password and forgets it, a SQLite database runs, pages, guards and
// reads its schema, a slow SQLite statement is stopped, a password is wiped from every cell it passes through, and every connection closed is released to
// the cellblock. The cases on a SQLite database are ignored on a machine with no Python
#include "ui/cli/src/databases/db_internal.h"
#include "ui/cli/src/tests/test_runs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 20u];
static char s_text[256];
static CliDbFailure s_failure;
static const CliJson *s_json;
static CliDbConnection s_prod;
static char s_root[CLI_PATH_MAX_LEN];
static char s_names[256];
static char s_error[256];
static int s_lite_tried;
static CliDbOpen *s_lite;
static char s_insert_done[64];
static char s_lite_root[CLI_PATH_MAX_LEN];
static int s_lite_version_named;

// the connection `name` of `type`, the rest empty, into `connection`
static void connection_set(CliDbConnection *connection, const char *name, const char *type)
{
    memor.set(&(MemoriaCfg){.dst = connection, .val = 0, .bytes = sizeof(*connection)});
    memor.cpy(&(MemoriaCfg){.dst = connection->name, .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = connection->type, .src = type, .bytes = CLI_UNITY_LENGTH(type) + 1u});
}

// `text` copied into `out`, which is sized for its text and its 0
static void text_set(char *out, const char *text)
{
    memor.cpy(&(MemoriaCfg){.dst = out, .src = text, .bytes = CLI_UNITY_LENGTH(text) + 1u});
}

// the names of the connections the tree names, `|` between them, in s_names
static const char *names_listing(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea list_entries = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_db_out_root.list, CliDbOutRootCfg, .root = s_root, .opens = NULL, .count = 0u, .out = &list_entries, .err_msg = &err_msg);
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, list_entries.at, &list);
    VerbaLinea line = {s_names, sizeof(s_names), 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = list, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = list, .index = at), .name = "connection"), "name"));
    }
    (void)mmgr_line_finish(&line);
    return s_names;
}

// a tree with a SQLite and a Postgres connection stored, the Postgres one in s_prod
static void connections_stored(void)
{
    CliDbConnection lite;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    cli_unity_folder("db_json", s_root);
    connection_set(&lite, "local", "sqlite");
    text_set(lite.file, "data/app.db");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .root = s_root, .connection = &lite, .was = NULL, .err_msg = &err_msg));
    connection_set(&s_prod, "prod", "postgres");
    text_set(s_prod.host, "db.example");
    text_set(s_prod.user, "app");
    text_set(s_prod.database, "app");
    text_set(s_prod.ssh, "me@bastion");
    s_prod.matters = 1;
    text_set(s_prod.guard, "read-only");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .root = s_root, .connection = &s_prod, .was = NULL, .err_msg = &err_msg));
}

// `statement` run on `open`, its answer read into the shared tree: its node, CLI_JSON_NONE where it failed
static unsigned int statement_run(CliDbOpen *open, const char *statement, int answered, unsigned long long page)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (!EMBED_CALL(cli_db_out_failure.run, CliDbOutFailureCfg, .open = open, .statement = statement, .size = CLI_UNITY_LENGTH(statement), .answered = answered, .page = page, .out = &out, .failure = &s_failure))
    {
        return CLI_JSON_NONE;
    }
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// a node's value written compact in s_text
static const char *node_written(unsigned int node)
{
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = node, .out = &line, .indent = 0u, .sorted = 0);
    (void)mmgr_line_finish(&line);
    return s_text;
}

// the schema's level under `count` names, read into the shared tree: its node
static unsigned int children_read(const char *const *path, unsigned int count)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_db_out_count.children, CliDbOutCountCfg, .open = s_lite, .path_pointer = path, .count = count, .out = &out, .failure = &s_failure);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// a SQLite database of a table of a thousand rows opened once for the process, the case ignored with no Python
static void lite_ready(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("no Python on this machine: SQLite is not opened");
    }
    if (!s_lite_tried)
    {
        CliDbConnection connection;
        s_lite_tried = 1;
        cli_unity_folder("db_sqlite", s_lite_root);
        connection_set(&connection, "lite", "sqlite");
        text_set(connection.file, "app.db");
        s_lite = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = s_lite_root, .config = &connection, .password = NULL, .failure = &s_failure);
        if (s_lite != NULL)
        {
            s_lite_version_named = EMBED_CALL(cli_db.version, CliDbCfg, .open_read = s_lite)[0] != '\0';
            (void)statement_run(s_lite, "CREATE TABLE t (n INTEGER PRIMARY KEY, name TEXT NOT NULL DEFAULT 'x', b BLOB)", 1, 100ull);
            (void)statement_run(s_lite, "CREATE INDEX t_name ON t (name)", 1, 100ull);
            const unsigned int found = statement_run(s_lite,
                                                     "INSERT INTO t (n, name, b) WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM k "
                                                     "WHERE i < 1000) SELECT i, 'n' || i, x'00ff' FROM k",
                                                     1, 100ull);
            VerbaLinea done = {s_insert_done, sizeof(s_insert_done), 0ull};
            mmgr_line_put(&done, cli_unity_member_text(found, "done"));
            (void)mmgr_line_finish(&done);
        }
    }
    TEST_ASSERT_NOT_NULL(s_lite);
}

// a stop sent from a thread of its own once half a second has passed
static void statement_stopped(void *context)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 500u);
    (void)EMBED_CALL(cli_db_err_msg.stop, CliDbErrMsgCfg, .open = (CliDbOpen *)context, .err_msg = &err_msg);
}

// whether `marked` lies anywhere among the pool's bytes
static int pool_contains(const char *marked)
{
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    cli_pool_report(&pool, &persistent, &temporary);
    char *const probe = g_cli_guard->temporary_buf_alloc(64ull, g_checks_owner);
    const unsigned long long top = (unsigned long long)g_cli_guard->temporary_buf_mark();
    // the probe is at the top of the temporary tier, its distance from the pool's start: a page short of each end
    const unsigned char *const start = (const unsigned char *)probe - top + 4096ull;
    const unsigned long long bytes = pool - 8192ull;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = marked, .cap = 64u});
    int holds = 0;
    for (unsigned long long at = 0ull; !holds && ((at + length) < bytes); at += 1ull)
    {
        // a character's bits, as a byte
        holds = (start[at] == (unsigned char)marked[0]) && (memor.cmp(&(MemoriaCfg){.src = start + at, .other = marked, .bytes = (size_t)length}) == 0);
    }
    g_cli_guard->persistent_buf_release(probe, g_checks_owner);
    return holds;
}

// the password made here, so that what the search through the pool finds is not the suite's own text
static void password_created(char *marked, unsigned long long capacity)
{
    VerbaLinea made = {marked, capacity, 0ull};
    mmgr_line_put(&made, "marked-");
    mmgr_line_put(&made, "password-7f3a");
    (void)mmgr_line_finish(&made);
}

// a connection of `type` to a port nothing listens on, with a password, asserted refused and the password wiped
static void assert_password_wiped(const char *type)
{
    char root[CLI_PATH_MAX_LEN];
    char marked[32];
    CliDbConnection connection;
    cli_unity_folder("db_password_wiped", root);
    password_created(marked, sizeof(marked));
    connection_set(&connection, "closed", type);
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.host, .src = "127.0.0.1", .cap = CLI_NAME_MAX_LEN});
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.user, .src = "app", .cap = CLI_NAME_MAX_LEN});
    connection.port = 1u;
    TEST_ASSERT_NULL(EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = root, .config = &connection, .password = marked, .failure = &s_failure));
    TEST_ASSERT_FALSE(pool_contains(marked));
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_connections_are_stored_each_by_its_name(void)
{
    connections_stored();
    TEST_ASSERT_EQUAL_STRING("local|prod", names_listing());
}

void test_a_name_another_connection_has_is_refused(void)
{
    CliDbConnection renamed;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    connections_stored();
    renamed = s_prod;
    text_set(renamed.name, "local");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .root = s_root, .connection = &renamed, .was = "prod", .err_msg = &err_msg));
}

void test_a_connection_renamed_stays_in_its_position(void)
{
    CliDbConnection renamed;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    connections_stored();
    renamed = s_prod;
    text_set(renamed.name, "live");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .root = s_root, .connection = &renamed, .was = "prod", .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("local|live", names_listing());
}

void test_no_password_is_stored_in_the_file(void)
{
    char text[4096];
    char path[CLI_PATH_MAX_LEN];
    connections_stored();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "databases.json", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text) - 1u);
    text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    CLI_TEST_ASSERT_NOT_CONTAINS("password", text);
}

void test_a_connection_is_removed(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    connections_stored();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_root_name.remove, CliDbRootNameCfg, .root = s_root, .name = "local", .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("prod", names_listing());
}

void test_a_connections_account_names_its_server_and_its_tunnel(void)
{
    char account[256];
    VerbaLinea line = {account, sizeof(account), 0ull};
    connections_stored();
    EMBED_CALL(cli_db_connection_out_type.account, CliDbConnectionOutTypeCfg, .connection = &s_prod, .out = &line);
    CLI_TEST_ASSERT_TEXT("postgres://app@db.example:5432/app via me@bastion", account, line.at);
}

void test_the_keychain_stores_a_password_and_forgets_it(void)
{
    char account[64];
    char read[64];
    VerbaLinea line = {account, sizeof(account), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    mmgr_line_put(&line, "test://orior@");
    mmgr_line_decimal(&line, cli_process_own(), 1u);
    (void)mmgr_line_finish(&line);
    if (!EMBED_CALL(cli_err_msg_service_internal.keychain_store, CliErrMsgServiceCtx, .service = "database", .account = account, .secret = "stored secret", .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no keychain on this machine");
    }
    const int was_read = EMBED_CALL(cli_service_internal.keychain_read, CliServiceCtx, .service = "database", .account = account, .out = read, .capacity = sizeof(read));
    (void)EMBED_CALL(cli_err_msg_service_internal.keychain_forget, CliErrMsgServiceCtx, .service = "database", .account = account, .err_msg = &err_msg);
    TEST_ASSERT_TRUE(was_read);
    TEST_ASSERT_EQUAL_STRING("stored secret", read);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_service_internal.keychain_read, CliServiceCtx, .service = "database", .account = account, .out = read, .capacity = sizeof(read)));
}

void test_a_sqlite_database_opens_with_its_version(void)
{
    lite_ready();
    TEST_ASSERT_TRUE(s_lite_version_named);
}

void test_an_insert_reports_what_it_did(void)
{
    lite_ready();
    TEST_ASSERT_EQUAL_STRING("1000 rows affected", s_insert_done);
}

void test_a_querys_first_page_comes_the_remainder_waiting(void)
{
    lite_ready();
    const unsigned int found = statement_run(s_lite, "SELECT n, name, b, NULL AS z FROM t ORDER BY n", 0, 100ull);
    const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "rows");
    TEST_ASSERT_EQUAL_UINT(100u, s_json->node[rows].count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "done"), .type = CFG_JSON_NULL));
}

void test_a_numbers_column_is_numeric(void)
{
    lite_ready();
    const unsigned int found = statement_run(s_lite, "SELECT n, name, b, NULL AS z FROM t ORDER BY n", 0, 100ull);
    const unsigned int column = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "columns"), .index = 0u);
    TEST_ASSERT_EQUAL_STRING("true", node_written(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = column, .name = "numeric")));
}

void test_each_value_is_written_as_the_window_shows_it(void)
{
    lite_ready();
    const unsigned int found = statement_run(s_lite, "SELECT n, name, b, NULL AS z FROM t ORDER BY n", 0, 100ull);
    const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "rows");
    TEST_ASSERT_EQUAL_STRING("[\"1\",\"n1\",\"0x00FF\",null]", node_written(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = rows, .index = 0u)));
}

void test_a_later_page_is_read_from_the_run(void)
{
    unsigned int found = CLI_JSON_NONE;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lite_ready();
    (void)statement_run(s_lite, "SELECT n, name, b, NULL AS z FROM t ORDER BY n", 0, 100ull);
    (void)EMBED_CALL(cli_db_out_failure_open.rows, CliDbOutFailureOpenCfg, .open = s_lite, .from = 950ull, .count_u64 = 100ull, .out = &out, .failure = &s_failure);
    s_json = cli_unity_parsed(s_out, out.at, &found);
    const unsigned int later = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "rows");
    TEST_ASSERT_EQUAL_UINT(50u, s_json->node[later].count);
    TEST_ASSERT_EQUAL_STRING("951", EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = later, .index = 0u), .index = 0u), .length_pointer = NULL));
    TEST_ASSERT_EQUAL_STRING("1000 rows read", cli_unity_member_text(found, "done"));
}

void test_a_delete_of_every_row_prompts_first_with_its_count_and_does_nothing(void)
{
    lite_ready();
    unsigned int found = statement_run(s_lite, "DELETE FROM t", 0, 100ull);
    const unsigned int confirm_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "confirm");
    TEST_ASSERT_EQUAL_STRING("every row", cli_unity_member_text(confirm_node, "why"));
    TEST_ASSERT_EQUAL_STRING("1000", cli_unity_member_text(confirm_node, "count"));
    found = statement_run(s_lite, "SELECT count(*) FROM t", 0, 10ull);
    const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "rows");
    TEST_ASSERT_EQUAL_STRING("\"1000\"", node_written(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = rows, .index = 0u), .index = 0u)));
}

void test_a_transaction_is_open_until_it_is_rolled_back(void)
{
    lite_ready();
    (void)statement_run(s_lite, "BEGIN", 1, 10ull);
    (void)statement_run(s_lite, "DELETE FROM t WHERE n > 10", 0, 10ull);
    const int open = cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = s_lite), .other = "open", .cap = CLI_POOL_BYTES});
    (void)statement_run(s_lite, "ROLLBACK", 1, 10ull);
    TEST_ASSERT_TRUE(open);
    TEST_ASSERT_EQUAL_STRING("idle", EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = s_lite));
}

void test_a_failure_reports_error(void)
{
    lite_ready();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, statement_run(s_lite, "SELECT * FROM none", 0, 10ull));
    CLI_TEST_ASSERT_CONTAINS("no such table", s_failure.message);
}

void test_the_schema_is_main(void)
{
    lite_ready();
    const unsigned int found = children_read(NULL, 0u);
    TEST_ASSERT_EQUAL_STRING("main", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u), "label"));
}

void test_the_schema_lists_its_objects(void)
{
    static const char *const s_main[1] = {"main"};
    lite_ready();
    const unsigned int found = children_read(s_main, 1u);
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[found].count);
    TEST_ASSERT_EQUAL_STRING("t", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u), "label"));
}

void test_a_table_lists_its_columns_and_indexes(void)
{
    static const char *const s_table[3] = {"main", "table", "t"};
    char displayed[256];
    VerbaLinea line = {displayed, sizeof(displayed), 0ull};
    lite_ready();
    const unsigned int found = children_read(s_table, 3u);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int part = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, cli_unity_member_text(part, "kind"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(part, "label"));
    }
    CLI_TEST_ASSERT_TEXT("column n|column name|column b|index t_name", displayed, line.at);
}

void test_each_column_has_its_type_and_default(void)
{
    static const char *const s_table[3] = {"main", "table", "t"};
    lite_ready();
    const unsigned int found = children_read(s_table, 3u);
    TEST_ASSERT_EQUAL_STRING("TEXT not null default 'x'", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 1u), "detail"));
}

void test_a_csv_export_writes_every_row_with_its_head(void)
{
    static char s_csv[1u << 16u];
    char path[CLI_PATH_MAX_LEN];
    unsigned int found = CLI_JSON_NONE;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lite_ready();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_lite_root, .relative = "t.csv", .joined = path);
    (void)statement_run(s_lite, "SELECT n, name FROM t ORDER BY n", 0, 10ull);
    (void)EMBED_CALL(cli_db_out.export, CliDbOutCfg, .open = s_lite, .path = path, .format = "csv", .progress = NULL, .context = NULL, .out = &out, .failure = &s_failure);
    s_json = cli_unity_parsed(s_out, out.at, &found);
    TEST_ASSERT_EQUAL_STRING("1000", node_written(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "written")));
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_csv, sizeof(s_csv) - 1u);
    s_csv[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    unsigned int lines = 0u;
    for (const char *at = s_csv; at[0] != '\0'; at += 1)
    {
        lines += (at[0] == '\n') ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(1001u, lines);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_csv, .other = "n,name\n1,n1\n", .cap = CLI_POOL_BYTES}));
}

void test_a_json_export_writes_each_row_an_object_of_its_columns(void)
{
    static char s_lines[1u << 16u];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lite_ready();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_lite_root, .relative = "t.jsonl", .joined = path);
    (void)statement_run(s_lite, "SELECT n, name FROM t ORDER BY n", 0, 10ull);
    (void)EMBED_CALL(cli_db_out.export, CliDbOutCfg, .open = s_lite, .path = path, .format = "json", .progress = NULL, .context = NULL, .out = &out, .failure = &s_failure);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_lines, sizeof(s_lines) - 1u);
    s_lines[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_lines, .other = "{\"n\":\"1\",\"name\":\"n1\"}\n", .cap = CLI_POOL_BYTES}));
}

void test_each_statement_is_recorded_in_the_history(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lite_ready();
    (void)statement_run(s_lite, "SELECT * FROM none", 0, 10ull);
    EMBED_CALL(cli_db_out_count.history, CliDbOutCountCfg, .open = s_lite, .out = &out);
    (void)mmgr_line_finish(&out);
    CLI_TEST_ASSERT_CONTAINS("\"sql\":\"SELECT * FROM none\"", s_out);
}

void test_a_statement_that_shapes_the_schema_names_its_object(void)
{
    lite_ready();
    const unsigned int found = statement_run(s_lite, "ALTER TABLE t ADD COLUMN extra TEXT", 1, 10ull);
    TEST_ASSERT_EQUAL_STRING("t", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "shaped"), "name"));
}

void test_a_slow_statement_is_stopped_at_once_and_the_connection_answers_after(void)
{
    static CliThread s_thread;
    char python[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    CliDbConnection connection;
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("no Python on this machine: SQLite is not opened");
    }
    cli_unity_folder("db_stop", root);
    connection_set(&connection, "lite", "sqlite");
    text_set(connection.file, "slow.db");
    CliDbOpen *const open = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = root, .config = &connection, .password = NULL, .failure = &s_failure);
    TEST_ASSERT_NOT_NULL(open);
    s_thread = (CliThread){statement_stopped, open};
    (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread);
    const unsigned long long started = cli_clock_milliseconds();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, statement_run(open, "WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM k) SELECT count(*) FROM k", 0, 10ull));
    CLI_TEST_ASSERT_CONTAINS("interrupt", s_failure.message);
    TEST_ASSERT_LESS_THAN_UINT64(10000ull, cli_clock_milliseconds() - started);
    const unsigned int found = statement_run(open, "SELECT 1", 0, 10ull);
    const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "rows");
    TEST_ASSERT_EQUAL_STRING("\"1\"", node_written(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = rows, .index = 0u), .index = 0u)));
    EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = open);
}

void test_a_refused_postgres_connection_stores_no_password_in_a_cell(void)
{
    assert_password_wiped("postgres");
}

void test_a_refused_mysql_connection_stores_no_password_in_a_cell(void)
{
    assert_password_wiped("mysql");
}

void test_every_connection_closed_is_released(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (s_lite != NULL)
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = s_lite);
        s_lite = NULL;
    }
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_db_owner));
    TEST_ASSERT_TRUE((mmgr_owner_hw(g_cli_db_owner) != 0ull) || !EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python));
}
