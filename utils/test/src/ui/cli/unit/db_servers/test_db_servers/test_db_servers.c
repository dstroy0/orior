// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_db_servers.c: database connections on real servers: a PostgreSQL connection that matters guards its writes, a tunnel through ssh reaches
// the server and is down once its ssh ends, and a MySQL connection reads its schema and its zone. Each runs where its server and its password
// are named
#include "ui/cli/src/databases/db.h"
#include "ui/cli/src/databases/db_internal.h"
#include "ui/cli/src/databases/pg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 18u];
static char s_text[512];
static CliDbFailure s_failure;
static const CliJson *s_json;
static char s_password[256];
static char s_root[CLI_PATH_MAX_LEN];
static CliDbConnection s_connection;
static CliDbOpen *s_open;

// `statement` run on `open`, its answer read into the shared tree: its node, CLI_JSON_NONE where it failed
static unsigned int statement_run(CliDbOpen *open, const char *statement, int answered)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (!EMBED_CALL(cli_db_out_failure.run, CliDbOutFailureCfg, .open = open, .statement = statement, .size = CLI_UNITY_LENGTH(statement), .answered = answered, .page = 10ull, .out = &out, .failure = &s_failure))
    {
        return CLI_JSON_NONE;
    }
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the schema's level under `count` names, `type label detail` of each joined by `|` into s_text
static const char *children_of(const char *const *path, unsigned int count)
{
    VerbaLinea read = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_db_out_count.children, CliDbOutCountCfg, .open = s_open, .path_pointer = path, .count = count, .out = &read, .failure = &s_failure);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, read.at, &found);
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int part = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, cli_unity_member_text(part, "kind"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(part, "label"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(part, "detail"));
    }
    (void)mmgr_line_finish(&line);
    return s_text;
}

// the connection `name` of `type` at the host and port ORIOR_<server>_ADDRESS names, into s_connection
static void connection_named(const char *name, const char *type, const char *server, unsigned int port, const char *user, const char *database)
{
    memor.set(&(MemoriaCfg){.dst = &s_connection, .val = 0, .bytes = sizeof(s_connection)});
    memor.cpy(&(MemoriaCfg){.dst = s_connection.name, .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1ull});
    memor.cpy(&(MemoriaCfg){.dst = s_connection.type, .src = type, .bytes = CLI_UNITY_LENGTH(type) + 1ull});
    cli_unity_server(server, port, s_connection.host, &s_connection.port);
    memor.cpy(&(MemoriaCfg){.dst = s_connection.user, .src = user, .bytes = CLI_UNITY_LENGTH(user) + 1ull});
    memor.cpy(&(MemoriaCfg){.dst = s_connection.database, .src = database, .bytes = CLI_UNITY_LENGTH(database) + 1ull});
}

// PostgreSQL connected with a table of 42 rows, s_open the connection; the test ignored where no password is named or no server answers
static void postgres_connected(void)
{
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_PG_PASSWORD", .value_out = s_password, .capacity = sizeof(s_password)))
    {
        TEST_IGNORE_MESSAGE("ORIOR_PG_PASSWORD names no password");
    }
    cli_unity_folder("db_pg", s_root);
    connection_named("pg", "postgres", "PG", 5432u, "postgres", "postgres");
    s_open = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = s_root, .config = &s_connection, .password = s_password, .failure = &s_failure);
    if (s_open == NULL)
    {
        TEST_IGNORE_MESSAGE("no PostgreSQL answers");
    }
    (void)statement_run(s_open, "DROP TABLE IF EXISTS orior_guard", 1);
    (void)statement_run(s_open, "CREATE TABLE orior_guard (n int)", 1);
    (void)statement_run(s_open, "INSERT INTO orior_guard SELECT generate_series(1, 42)", 1);
}

// the connection closed and reopened with the guard `guard`
static void postgres_guarded(const char *guard)
{
    postgres_connected();
    EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = s_open);
    s_open = NULL;
    s_connection.matters = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_connection.guard, .src = guard, .bytes = CLI_UNITY_LENGTH(guard) + 1ull});
    s_open = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = s_root, .config = &s_connection, .password = s_password, .failure = &s_failure);
    TEST_ASSERT_NOT_NULL(s_open);
}

// MySQL connected with a database of one table, s_open the connection; the test ignored where no password is named or no server answers
static void mysql_connected(void)
{
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_MYSQL_PASSWORD", .value_out = s_password, .capacity = sizeof(s_password)))
    {
        TEST_IGNORE_MESSAGE("ORIOR_MYSQL_PASSWORD names no password");
    }
    cli_unity_folder("db_mysql", s_root);
    connection_named("my", "mysql", "MYSQL", 3306u, "root", "");
    s_open = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = s_root, .config = &s_connection, .password = s_password, .failure = &s_failure);
    if (s_open == NULL)
    {
        TEST_IGNORE_MESSAGE("no MySQL answers");
    }
    (void)statement_run(s_open, "CREATE DATABASE IF NOT EXISTS orior_schema", 1);
    (void)statement_run(s_open, "CREATE TABLE IF NOT EXISTS orior_schema.t (id INT PRIMARY KEY, name VARCHAR(20) NOT NULL, KEY by_name (name))", 1);
}

// a row's first value recorded
static int first_value_recorded(const CliDbRowCfg *args)
{
    VerbaLinea *const line = args->context;
    if ((args->count > 0u) && !args->values[0].null)
    {
        mmgr_line_put_n(line, args->values[0].text, args->values[0].size);
    }
    return 1;
}

void setUp(void)
{
    cli_unity_begun();
    s_open = NULL;
}

void tearDown(void)
{
    if (s_open != NULL)
    {
        EMBED_CALL(cli_db_err_msg.close, CliDbErrMsgCfg, .open = s_open);
    }
}

void test_postgres_reads_the_sessions_zone(void)
{
    postgres_connected();
    VerbaLinea zone = {s_text, sizeof(s_text), 0ull};
    EMBED_CALL(cli_db_out_count.zone, CliDbOutCountCfg, .open = s_open, .out = &zone);
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, zone.at);
}

void test_postgres_reads_the_schemas(void)
{
    postgres_connected();
    CLI_TEST_ASSERT_CONTAINS(" public ", children_of(NULL, 0u));
}

void test_postgres_reads_a_schemas_tables(void)
{
    static const char *const s_public[1] = {"public"};
    postgres_connected();
    CLI_TEST_ASSERT_CONTAINS("table orior_guard", children_of(s_public, 1u));
}

void test_postgres_reads_a_tables_columns(void)
{
    static const char *const s_table[3] = {"public", "table", "orior_guard"};
    postgres_connected();
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = children_of(s_table, 3u), .other = "column n integer", .cap = CLI_POOL_BYTES}));
}

void test_an_update_of_every_row_prompts_with_its_count(void)
{
    postgres_connected();
    const unsigned int found = statement_run(s_open, "UPDATE orior_guard SET n = 0", 0);
    TEST_ASSERT_EQUAL_STRING("42", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "confirm"), "count"));
}

void test_a_connection_that_matters_prompts_before_a_write(void)
{
    postgres_guarded("confirm");
    const unsigned int found = statement_run(s_open, "INSERT INTO orior_guard VALUES (1)", 0);
    TEST_ASSERT_EQUAL_STRING("matters", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "confirm"), "why"));
}

void test_a_connection_that_matters_does_not_prompt_before_a_read(void)
{
    postgres_guarded("confirm");
    const unsigned int found = statement_run(s_open, "SELECT 1", 0);
    const unsigned int confirm = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "confirm");
    TEST_ASSERT_TRUE((confirm == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = confirm, .type = CFG_JSON_NULL));
}

void test_a_read_only_connections_server_refuses_a_write(void)
{
    postgres_guarded("read-only");
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, statement_run(s_open, "INSERT INTO orior_guard VALUES (1)", 0));
    TEST_ASSERT_EQUAL_STRING("25006", s_failure.code);
}

void test_a_tunnel_through_ssh_reaches_the_server_and_is_down_once_its_ssh_ends(void)
{
    static CliDbTunnel s_tunnel;
    char ssh[CLI_PATH_MAX_LEN];
    char through[CLI_NAME_MAX_LEN];
    char to[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_PG_PASSWORD", .value_out = s_password, .capacity = sizeof(s_password)) || !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_TEST_SSH", .value_out = ssh, .capacity = sizeof(ssh)) || !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_TEST_SSH_HOST", .value_out = through, .capacity = sizeof(through)) || !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_TEST_SSH_TO", .value_out = to, .capacity = sizeof(to)))
    {
        TEST_IGNORE_MESSAGE("no ssh or password is named for the tests");
    }
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_SSH", .value = ssh));
    char host[CLI_NAME_MAX_LEN];
    unsigned int port = 0u;
    cli_unity_server("PG", 5432u, host, &port);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_host_internal.tunnel_open, CliDbHostCtx, .tunnel = &s_tunnel, .through = through, .host = to, .port = port, .failure = &s_failure));
    CliPg *const pg = EMBED_CALL(cli_pg_host.connect, CliPgHostCfg, .host = "127.0.0.1", .port = s_tunnel.port, .user = "postgres", .password = s_password, .database = "postgres", .wait = 5000ull, .failure = &s_failure);
    TEST_ASSERT_NOT_NULL(pg);
    char value[64];
    VerbaLinea line = {value, sizeof(value), 0ull};
    const int answered = EMBED_CALL(cli_pg_failure_sql.all, CliPgFailureSqlCfg, .pg = pg, .sql = "select 'through ssh'", .size = 19ull, .each_row = first_value_recorded, .context = &line, .failure = &s_failure);
    EMBED_CALL(cli_pg_failure_sql.close, CliPgFailureSqlCfg, .pg = pg);
    TEST_ASSERT_TRUE(answered);
    CLI_TEST_ASSERT_TEXT("through ssh", value, line.at);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_host_internal.tunnel_alive, CliDbHostCtx, .tunnel = &s_tunnel));
    EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &s_tunnel.child);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &s_tunnel.child);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_host_internal.tunnel_alive, CliDbHostCtx, .tunnel = &s_tunnel));
    EMBED_CALL(cli_db_host_internal.tunnel_close, CliDbHostCtx, .tunnel = &s_tunnel);
    char nobody[CLI_NAME_MAX_LEN + 8u];
    VerbaLinea named = {nobody, sizeof(nobody), 0ull};
    mmgr_line_put(&named, "nobody@");
    mmgr_line_put(&named, through);
    (void)mmgr_line_finish(&named);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_host_internal.tunnel_open, CliDbHostCtx, .tunnel = &s_tunnel, .through = nobody, .host = to, .port = port, .failure = &s_failure));
    CLI_TEST_ASSERT_CONTAINS("ended", s_failure.message);
}

void test_mysql_reads_the_sessions_zone(void)
{
    mysql_connected();
    VerbaLinea zone = {s_text, sizeof(s_text), 0ull};
    EMBED_CALL(cli_db_out_count.zone, CliDbOutCountCfg, .open = s_open, .out = &zone);
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, zone.at);
    (void)statement_run(s_open, "DROP DATABASE orior_schema", 1);
}

void test_mysql_reads_the_databases(void)
{
    mysql_connected();
    CLI_TEST_ASSERT_CONTAINS(" orior_schema ", children_of(NULL, 0u));
    (void)statement_run(s_open, "DROP DATABASE orior_schema", 1);
}

void test_mysql_reads_a_tables_columns_and_indexes(void)
{
    static const char *const s_table[3] = {"orior_schema", "table", "t"};
    mysql_connected();
    const char *const children = children_of(s_table, 3u);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = children, .other = "column id ", .cap = CLI_POOL_BYTES}));
    CLI_TEST_ASSERT_CONTAINS("|column name ", children);
    CLI_TEST_ASSERT_CONTAINS("|index by_name ", children);
    CLI_TEST_ASSERT_CONTAINS("|index PRIMARY ", children);
    (void)statement_run(s_open, "DROP DATABASE orior_schema", 1);
}
