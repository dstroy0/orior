// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_servers_test.c: the database connections' check on the servers orior's tests reach: a PostgreSQL connection that
// matters guards its writes, a tunnel through ssh reaches the server and is down once its ssh ends, and a MySQL
// connection reads its schema and its zone. Each runs where its server and its password are named
#include "../../../../../ui/cli/src/databases/db.h"
#include "../../../../../ui/cli/src/databases/db_internal.h"
#include "../../../../../ui/cli/src/databases/pg.h"
#include "cli_check.h"

static char s_out[1u << 18u];
static CliDbFailure s_failure;

// `statement` run on `open`, its answer read into the shared tree: its node, CLI_JSON_NONE where it failed
static unsigned int servers_run(CliDbOpen *open, const char *statement, int answered, const CliJson **json)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    if (!cli_db_run(open, statement, cellul.len(&(CatenaFinitaCfg){.src = statement, .cap = CLI_CELLS_POOL}), answered,
                    10ull, &out, &s_failure))
    {
        return CLI_JSON_NONE;
    }
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// the schema's level under `count` names, `kind label` of each joined by `|` into `out`, which holds 512
static const char *servers_children(CliDbOpen *open, const char *const *path, unsigned int count, char *out)
{
    CliLine read = {s_out, sizeof(s_out), 0ull};
    (void)cli_db_children(open, path, count, &read, &s_failure);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, read.at, &found);
    CliLine line = {out, 512u, 0ull};
    for (unsigned int at = 0u; cli_json_item(json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int part = cli_json_item(json, found, at);
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_check_member_text(part, "kind"));
        cli_line_character(&line, ' ');
        cli_line_text(&line, cli_check_member_text(part, "label"));
        cli_line_character(&line, ' ');
        cli_line_text(&line, cli_check_member_text(part, "detail"));
    }
    (void)cli_line_closed(&line);
    return out;
}

// the server connection `name` of `kind` at the host and port ORIOR_<server>_ADDRESS names, into `connection`
static void servers_connection(CliDbConnection *connection, const char *name, const char *kind, const char *server,
                               unsigned int port, const char *user, const char *database)
{
    memor.set(&(MemoriaCfg){.dst = connection, .val = 0, .bytes = sizeof(*connection)});
    memor.cpy(&(MemoriaCfg){.dst = connection->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = connection->kind,
                            .src = kind,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) + 1u});
    cli_check_server(server, port, connection->host, &connection->port);
    memor.cpy(&(MemoriaCfg){.dst = connection->user,
                            .src = user,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = user, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = connection->database,
                            .src = database,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = database, .cap = CLI_CELLS_POOL}) + 1u});
}

// a PostgreSQL connection that matters guards its writes
static void servers_pg(const char *password)
{
    static const char *const s_public[1] = {"public"};
    static const char *const s_table[3] = {"public", "table", "orior_guard"};
    char root[CLI_PATH_LONGEST];
    char text[512];
    cli_check_folder("db_pg", root);
    CliDbConnection base;
    servers_connection(&base, "pg", "postgres", "PG", 5432u, "postgres", "postgres");
    CliDbOpen *const open = cli_db_connect(root, &base, password, &s_failure);
    if (open == NULL)
    {
        cli_check(1, "no PostgreSQL answers, and no connection is guarded");
        return;
    }
    const CliJson *json = NULL;
    (void)servers_run(open, "DROP TABLE IF EXISTS orior_guard", 1, &json);
    (void)servers_run(open, "CREATE TABLE orior_guard (n int)", 1, &json);
    (void)servers_run(open, "INSERT INTO orior_guard SELECT generate_series(1, 42)", 1, &json);
    CliLine zone = {text, sizeof(text), 0ull};
    cli_db_zone(open, &zone);
    cli_check(zone.at > 0ull, "the session's zone is read");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = servers_children(open, NULL, 0u, text),
                                             .cap = CLI_CELLS_POOL,
                                             .other = " public ",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "the schemas are read");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = servers_children(open, s_public, 1u, text),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "table orior_guard",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "and their tables");
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = servers_children(open, s_table, 3u, text),
                                               .other = "column n integer",
                                               .cap = CLI_CELLS_POOL}),
              "and their columns");
    unsigned int found = servers_run(open, "UPDATE orior_guard SET n = 0", 0, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, found, "ask"), "count"),
                                           .other = "42",
                                           .cap = CLI_CELLS_POOL}),
              "an update of every row asks with its count");
    cli_db_close(open);
    CliDbConnection asks = base;
    asks.matters = 1;
    memor.cpy(&(MemoriaCfg){.dst = asks.guard,
                            .src = "ask",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "ask", .cap = CLI_CELLS_POOL}) + 1u});
    CliDbOpen *const asked = cli_db_connect(root, &asks, password, &s_failure);
    if (asked != NULL)
    {
        found = servers_run(asked, "INSERT INTO orior_guard VALUES (1)", 0, &json);
        cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, found, "ask"), "why"),
                                               .other = "matters",
                                               .cap = CLI_CELLS_POOL}),
                  "a connection that matters asks before a write");
        found = servers_run(asked, "SELECT 1", 0, &json);
        cli_check(cli_json_member(json, found, "ask") == CLI_JSON_NONE ||
                      cli_json_is(json, cli_json_member(json, found, "ask"), CFG_JSON_NULL),
                  "and not before a read");
        cli_db_close(asked);
    }
    CliDbConnection read_only = base;
    read_only.matters = 1;
    memor.cpy(&(MemoriaCfg){.dst = read_only.guard,
                            .src = "read-only",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "read-only", .cap = CLI_CELLS_POOL}) + 1u});
    CliDbOpen *const refused = cli_db_connect(root, &read_only, password, &s_failure);
    if (refused != NULL)
    {
        cli_check((servers_run(refused, "INSERT INTO orior_guard VALUES (1)", 0, &json) == CLI_JSON_NONE) &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = s_failure.code, .other = "25006", .cap = CLI_CELLS_POOL})),
                  "a read-only connection's server refuses a write");
        cli_db_close(refused);
    }
    CliDbOpen *const cleanup = cli_db_connect(root, &base, password, &s_failure);
    if (cleanup != NULL)
    {
        (void)servers_run(cleanup, "DROP TABLE orior_guard", 1, &json);
        cli_db_close(cleanup);
    }
}

// a row's first value kept
static int servers_kept(void *context, const CliDbValue *values, unsigned int count)
{
    CliLine *const line = context;
    if ((count > 0u) && !values[0].null)
    {
        cli_line_counted(line, values[0].text, values[0].size);
    }
    return 1;
}

// a tunnel through ssh reaches the server and is down once its ssh ends
static void servers_tunnel(const char *password)
{
    char ssh[CLI_PATH_LONGEST];
    char through[CLI_NAME_LONGEST];
    char to[CLI_NAME_LONGEST];
    if (!query_environment_read("ORIOR_TEST_SSH", ssh, sizeof(ssh)) ||
        !query_environment_read("ORIOR_TEST_SSH_HOST", through, sizeof(through)) ||
        !query_environment_read("ORIOR_TEST_SSH_TO", to, sizeof(to)))
    {
        cli_check(1, "no ssh is named for the tests, and no tunnel is made");
        return;
    }
    (void)query_environment_set("ORIOR_SSH", ssh);
    char host[CLI_NAME_LONGEST];
    unsigned int port = 0u;
    cli_check_server("PG", 5432u, host, &port);
    static CliDbTunnel s_tunnel;
    cli_check(cli_db_tunnel_open(&s_tunnel, through, to, port, &s_failure), "a tunnel opens through ssh");
    CliPg *const pg =
        cli_pg_connected("127.0.0.1", s_tunnel.port, "postgres", password, "postgres", 5000ull, &s_failure);
    char value[64];
    CliLine line = {value, sizeof(value), 0ull};
    cli_check((pg != NULL) && cli_pg_all(pg, "select 'through ssh'", 19ull, servers_kept, &line, &s_failure) &&
                  (line.at == 11ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = value, .other = "through ssh", .bytes = 11u}) == 0),
              "the server answers through it");
    if (pg != NULL)
    {
        cli_pg_closed(pg);
    }
    cli_check(cli_db_tunnel_alive(&s_tunnel), "the tunnel stands");
    cli_spawn_killed(&s_tunnel.child);
    (void)cli_spawn_wait(&s_tunnel.child);
    cli_check(!cli_db_tunnel_alive(&s_tunnel), "a tunnel whose ssh ended is down");
    cli_db_tunnel_closed(&s_tunnel);
    char nobody[CLI_NAME_LONGEST + 8u];
    CliLine named = {nobody, sizeof(nobody), 0ull};
    cli_line_text(&named, "nobody@");
    cli_line_text(&named, through);
    (void)cli_line_closed(&named);
    cli_check(
        !cli_db_tunnel_open(&s_tunnel, nobody, to, port, &s_failure) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_failure.message, .cap = CLI_CELLS_POOL, .other = "ended", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "a tunnel ssh refuses says its ssh ended");
}

// a MySQL connection reads its schema and its zone
static void servers_mysql(const char *password)
{
    static const char *const s_table[3] = {"orior_schema", "table", "t"};
    char root[CLI_PATH_LONGEST];
    char text[512];
    cli_check_folder("db_mysql", root);
    CliDbConnection connection;
    servers_connection(&connection, "my", "mysql", "MYSQL", 3306u, "root", "");
    CliDbOpen *const open = cli_db_connect(root, &connection, password, &s_failure);
    if (open == NULL)
    {
        cli_check(1, "no MySQL answers, and no schema is read");
        return;
    }
    const CliJson *json = NULL;
    (void)servers_run(open, "CREATE DATABASE IF NOT EXISTS orior_schema", 1, &json);
    (void)servers_run(open,
                      "CREATE TABLE IF NOT EXISTS orior_schema.t (id INT PRIMARY KEY, name VARCHAR(20) NOT NULL, KEY "
                      "by_name (name))",
                      1, &json);
    CliLine zone = {text, sizeof(text), 0ull};
    cli_db_zone(open, &zone);
    cli_check(zone.at > 0ull, "the session's zone is read");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = servers_children(open, NULL, 0u, text),
                                             .cap = CLI_CELLS_POOL,
                                             .other = " orior_schema ",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "the databases are read");
    servers_children(open, s_table, 3u, text);
    cli_check(
        (cellul.starts(&(CatenaFinitaCfg){.src = text, .other = "column id ", .cap = CLI_CELLS_POOL})) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = text, .cap = CLI_CELLS_POOL, .other = "|column name ", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = text, .cap = CLI_CELLS_POOL, .other = "|index by_name ", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = text, .cap = CLI_CELLS_POOL, .other = "|index PRIMARY ", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a table's columns and indexes");
    (void)servers_run(open, "DROP DATABASE orior_schema", 1, &json);
    cli_db_close(open);
}

int main(void)
{
    char password[256];
    if (query_environment_read("ORIOR_PG_PASSWORD", password, sizeof(password)))
    {
        servers_pg(password);
        servers_tunnel(password);
    }
    if (query_environment_read("ORIOR_MYSQL_PASSWORD", password, sizeof(password)))
    {
        servers_mysql(password);
    }
    cli_check(1, "the servers named are reached");
    return cli_checked();
}
