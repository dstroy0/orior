// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql_test.c: SQL's reading's check: a semicolon in a string, a comment or a dollar quote ends no statement; a
// trigger's body is one statement; the statement at a place is the one it stands in and none on a blank line; what a
// statement does is read past its comments; a write with no where is counted whole; and a statement that shapes the
// schema names its object
#include "../../../../../ui/cli/src/databases/sql.h"
#include "cli_check.h"

// the statements of a text, `|` between them
typedef struct
{
    const char *text;
    CliLine line;
    unsigned int count;
} SqlStatements;

// a statement put after the others
static void sql_statement(void *context, CliSqlSpan span)
{
    SqlStatements *const statements = context;
    cli_line_text(&statements->line, (statements->count > 0u) ? "|" : "");
    cli_line_counted(&statements->line, statements->text + span.start, span.end - span.start);
    statements->count += 1u;
}

// the statements of `text` in `dialect`, checked against `wanted`
static void sql_texts(const char *text, unsigned int dialect, const char *wanted, const char *what)
{
    char out[512];
    SqlStatements statements = {text, {out, sizeof(out), 0ull}, 0u};
    cli_sql_statements(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), dialect, sql_statement,
                       &statements);
    cli_check_text(out, statements.line.at, wanted, what);
}

// how many statements `text` holds in `dialect`
static unsigned int sql_count(const char *text, unsigned int dialect)
{
    char out[512];
    SqlStatements statements = {text, {out, sizeof(out), 0ull}, 0u};
    cli_sql_statements(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), dialect, sql_statement,
                       &statements);
    return statements.count;
}

// the statement at `at` of `text`, the empty text where none, into `out`, which holds 64
static const char *sql_at(const char *text, unsigned long long at, char *out)
{
    CliSqlSpan span;
    CliLine line = {out, 64u, 0ull};
    if (cli_sql_statement_at(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), at,
                             CLI_SQL_POSTGRES, &span))
    {
        cli_line_counted(&line, text + span.start, span.end - span.start);
    }
    (void)cli_line_closed(&line);
    return out;
}

// the place of `needle` in `text`
static unsigned long long sql_place(const char *text, const char *needle)
{
    // the needle lies in the text, at or past its start
    return (unsigned long long)(cellul.find(&(CatenaFinitaCfg){
                                    .src = text, .cap = CLI_CELLS_POOL, .other = needle, .other_cap = CLI_CELLS_POOL}) -
                                text);
}

// the count a write with no where asks for, checked against `wanted`, NULL for none
static void sql_unbounded(const char *statement, unsigned int dialect, const char *wanted, const char *what)
{
    char out[256];
    CliLine line = {out, sizeof(out), 0ull};
    const int counted = cli_sql_unbounded(
        statement, cellul.len(&(CatenaFinitaCfg){.src = statement, .cap = CLI_CELLS_POOL}), dialect, &line);
    cli_check((wanted == NULL)
                  ? !counted
                  : (counted && (line.at == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
                     (memor.cmp(&(MemoriaCfg){.src = out, .other = wanted, .bytes = line.at}) == 0)),
              what);
}

// what a statement does
static unsigned int sql_does(const char *statement, unsigned int dialect)
{
    return cli_sql_does(statement, cellul.len(&(CatenaFinitaCfg){.src = statement, .cap = CLI_CELLS_POOL}), dialect);
}

// a statement that shapes the schema names its object
static void sql_shaped(void)
{
    CliShaped shaped;
    static const char s_made[] = "CREATE TABLE IF NOT EXISTS public.\"Orders\" (id int)";
    cli_check(
        cli_sql_shaped(s_made, sizeof(s_made) - 1u, CLI_SQL_POSTGRES, &shaped) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = shaped.verb, .other = "create", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = shaped.kind, .other = "table", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = shaped.name, .other = "public.\"Orders\"", .cap = CLI_CELLS_POOL})),
        "a table made is named");
    static const char s_index[] = "create unique index concurrently ix on only s.t (a)";
    cli_check(cli_sql_shaped(s_index, sizeof(s_index) - 1u, CLI_SQL_POSTGRES, &shaped) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.kind, .other = "index", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.name, .other = "ix", .cap = CLI_CELLS_POOL})) &&
                  shaped.has_on &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.on, .other = "s.t", .cap = CLI_CELLS_POOL})),
              "an index with its table");
    static const char s_view[] = "create or replace materialized view v as select 1";
    cli_check(cli_sql_shaped(s_view, sizeof(s_view) - 1u, CLI_SQL_POSTGRES, &shaped) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.kind, .other = "view", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.name, .other = "v", .cap = CLI_CELLS_POOL})),
              "a view");
    static const char s_gone[] = "drop table if exists `db`.`t`";
    cli_check(cli_sql_shaped(s_gone, sizeof(s_gone) - 1u, CLI_SQL_MYSQL, &shaped) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.verb, .other = "drop", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.name, .other = "`db`.`t`", .cap = CLI_CELLS_POOL})),
              "a table dropped");
    cli_check(cli_sql_shaped("truncate t", 10ull, CLI_SQL_POSTGRES, &shaped) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = shaped.name, .other = "t", .cap = CLI_CELLS_POOL})),
              "a table truncated");
    cli_check(!cli_sql_shaped("select 1", 8ull, CLI_SQL_POSTGRES, &shaped), "and a read shapes nothing");
}

int main(void)
{
    sql_texts("select ';' -- a; comment\n; /* x; /* y; */ z; */ select $f$ a; b $f$ ; select E'it\\'s; ok'",
              CLI_SQL_POSTGRES, "select ';'|select $f$ a; b $f$|select E'it\\'s; ok'",
              "a semicolon in a string, a comment or a dollar quote ends no statement");
    sql_texts("select 'it\\'s; ok' # a; comment\n; select `a;b` from t", CLI_SQL_MYSQL,
              "select 'it\\'s; ok'|select `a;b` from t", "nor in MySQL's quotes and comments");
    sql_texts("select [a;b] from t; select \"c;d\"", CLI_SQL_SQLITE, "select [a;b] from t|select \"c;d\"",
              "nor in SQLite's");
    sql_texts(
        "create trigger t after insert on a begin insert into b values (1); update c set n = n + 1; end; select 1",
        CLI_SQL_SQLITE,
        "create trigger t after insert on a begin insert into b values (1); update c set n = n + 1; end|select 1",
        "a trigger's body is one statement");
    cli_check(sql_count("CREATE PROCEDURE p() BEGIN IF 1 THEN SELECT 1; END IF; SELECT CASE WHEN 1 THEN 2 END; END; "
                        "SELECT 2",
                        CLI_SQL_MYSQL) == 2u,
              "and a routine's");
    static const char s_text[] = "select 1;\n\n-- note\nselect 2; -- after\nselect\n  3";
    char out[64];
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = sql_at(s_text, sql_place(s_text, "1"), out), .other = "select 1", .cap = CLI_CELLS_POOL}),
              "the statement at a place");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = sql_at(s_text, sql_place(s_text, "1;") + 2u, out),
                                           .other = "select 1",
                                           .cap = CLI_CELLS_POOL}),
              "and just past its end");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = sql_at(s_text, sql_place(s_text, "\n\n") + 1u, out), .other = "", .cap = CLI_CELLS_POOL}),
              "none on a blank line");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = sql_at(s_text, sql_place(s_text, "note"), out), .other = "", .cap = CLI_CELLS_POOL}),
              "nor in a comment of its own");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = sql_at(s_text, sql_place(s_text, "after"), out), .other = "select 2", .cap = CLI_CELLS_POOL}),
              "a comment after is its own");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = sql_at(s_text, sizeof(s_text) - 1u, out), .other = "select\n  3", .cap = CLI_CELLS_POOL}),
              "and the last at the text's end");
    cli_check(sql_does("/* x */ SELECT * FROM t", CLI_SQL_POSTGRES) == CLI_DOES_READS, "a read past its comment");
    cli_check(sql_does("with gone as (delete from t returning *) select * from gone", CLI_SQL_POSTGRES) ==
                  CLI_DOES_WRITES,
              "a write within a with");
    cli_check(sql_does("explain analyze delete from t", CLI_SQL_POSTGRES) == CLI_DOES_WRITES,
              "explain analyze runs it");
    cli_check(sql_does("explain delete from t", CLI_SQL_POSTGRES) == CLI_DOES_READS, "explain alone does not");
    cli_check(sql_does("select * into copy from t", CLI_SQL_POSTGRES) == CLI_DOES_SHAPES, "select into makes a table");
    cli_check(sql_does("drop table t", CLI_SQL_MYSQL) == CLI_DOES_SHAPES, "a drop shapes");
    cli_check(sql_does("begin", CLI_SQL_SQLITE) == CLI_DOES_TRANSACTION, "begin opens a transaction");
    cli_check(sql_does("pragma table_info(t)", CLI_SQL_SQLITE) == CLI_DOES_READS, "a pragma that asks reads");
    cli_check(sql_does("set search_path = a", CLI_SQL_POSTGRES) == CLI_DOES_OTHER, "and a setting is other");
    sql_unbounded("update only t as x set n = 1", CLI_SQL_POSTGRES, "SELECT count(*) FROM only t as x",
                  "an update with no where is counted whole");
    sql_unbounded("update t set n = (select max(n) from u where u.k = 1)", CLI_SQL_POSTGRES, "SELECT count(*) FROM t",
                  "a where in a subquery is not its own");
    sql_unbounded("update t set n = 1 where k = 2", CLI_SQL_POSTGRES, NULL, "an update with a where is not counted");
    sql_unbounded("delete from s.t using u returning *", CLI_SQL_POSTGRES, "SELECT count(*) FROM s.t, u",
                  "a delete's using is counted with it");
    sql_unbounded("DELETE FROM t", CLI_SQL_MYSQL, "SELECT count(*) FROM t", "in MySQL");
    sql_unbounded("update a join b on a.k = b.k set a.n = b.n", CLI_SQL_MYSQL,
                  "SELECT count(*) FROM a join b on a.k = b.k", "and a join's update");
    sql_unbounded("truncate table t restart identity", CLI_SQL_POSTGRES, "SELECT count(*) FROM t", "a truncate");
    sql_unbounded("truncate a, b", CLI_SQL_POSTGRES, "SELECT (SELECT count(*) FROM a) + (SELECT count(*) FROM b)",
                  "of many tables");
    sql_unbounded("delete from t where 'where' = ''", CLI_SQL_POSTGRES, NULL, "a where in a string is no where");
    sql_unbounded("select 1", CLI_SQL_POSTGRES, NULL, "and a read is no write");
    sql_shaped();
    return cli_checked();
}
