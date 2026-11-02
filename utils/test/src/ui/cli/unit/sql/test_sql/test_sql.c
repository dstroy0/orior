// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sql.c: SQL reading: a semicolon in a string, a comment or a dollar quote ends no statement, a trigger's body is one statement, the
// statement at a position is the one around it, what a statement does is read past its comments, a write with no where is counted, and
// a statement that changes the schema names its object
#include "ui/cli/src/databases/sql.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_positions[] = "select 1;\n\n-- note\nselect 2; -- after\nselect\n  3";

static CliShaped s_shaped;

// The statements of a text, `|` between them
typedef struct
{
    const char *text;
    VerbaLinea line;
    unsigned int count;
} SqlStatements;

// a statement appended to the others
static void statement_recorded(const CliEachSpanCfg *args)
{
    SqlStatements *const statements = args->context;
    mmgr_line_put(&statements->line, (statements->count > 0u) ? "|" : "");
    mmgr_line_put_n(&statements->line, statements->text + args->span.start, args->span.end - args->span.start);
    statements->count += 1u;
}

// the statements of `text` in `dialect` are `wanted`
static void assert_statements(const char *wanted, const char *text, unsigned int dialect)
{
    char out[512];
    SqlStatements statements = {text, {out, sizeof(out), 0ull}, 0u};
    EMBED_CALL(cli_sql_text_length.statements, CliSqlTextLengthCfg, .text = text, .length = CLI_UNITY_LENGTH(text), .dialect = dialect, .each = statement_recorded, .context = &statements);
    CLI_TEST_ASSERT_TEXT(wanted, out, statements.line.at);
}

// the statement at `at` of the sample is `wanted`, the empty string for none
static void assert_statement_at(const char *wanted, unsigned long long at)
{
    char out[64];
    CliSqlSpan span;
    VerbaLinea line = {out, sizeof(out), 0ull};
    if (EMBED_CALL(cli_sql_text.statement_at, CliSqlTextCfg, .text = s_positions, .length = sizeof(s_positions) - 1u, .at_value = at, .dialect = CLI_SQL_POSTGRES, .span = &span))
    {
        mmgr_line_put_n(&line, s_positions + span.start, span.end - span.start);
    }
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING(wanted, out);
}

// the position of `needle` in the sample
static unsigned long long position_of(const char *needle)
{
    // the needle lies in the sample, at or past its start
    return (unsigned long long)(cellul.find(&(CatenaFinitaCfg){.src = s_positions, .cap = CLI_CELLS_POOL, .other = needle, .other_cap = CLI_CELLS_POOL}) - s_positions);
}

// the count `statement` asks for is `wanted`, NULL for none
static void assert_counted(const char *wanted, const char *statement, unsigned int dialect)
{
    char out[256];
    VerbaLinea line = {out, sizeof(out), 0ull};
    const int counted = EMBED_CALL(cli_sql_length_dialect.unbounded, CliSqlLengthDialectCfg, .statement = statement, .length = CLI_UNITY_LENGTH(statement), .dialect = dialect, .out = &line);
    if (wanted == NULL)
    {
        TEST_ASSERT_FALSE(counted);
    }
    else
    {
        TEST_ASSERT_TRUE(counted);
        CLI_TEST_ASSERT_TEXT(wanted, out, line.at);
    }
}

// what `statement` does
static unsigned int does(const char *statement, unsigned int dialect)
{
    return EMBED_CALL(cli_sql_length.does, CliSqlLengthCfg, .statement = statement, .length = CLI_UNITY_LENGTH(statement), .dialect = dialect);
}

// `statement` read as a change of the schema into s_shaped
static void shaped(const char *statement, unsigned int dialect)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_sql_length.shape, CliSqlLengthCfg, .statement = statement, .length = CLI_UNITY_LENGTH(statement), .dialect = dialect, .shaped = &s_shaped));
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_semicolon_in_a_string_a_comment_or_a_dollar_quote_ends_no_statement(void)
{
    assert_statements("select ';'|select $f$ a; b $f$|select E'it\\'s; ok'", "select ';' -- a; comment\n; /* x; /* y; */ z; */ select $f$ a; b $f$ ; select E'it\\'s; ok'", CLI_SQL_POSTGRES);
}

void test_a_semicolon_in_mysqls_quotes_and_comments_ends_no_statement(void)
{
    assert_statements("select 'it\\'s; ok'|select `a;b` from t", "select 'it\\'s; ok' # a; comment\n; select `a;b` from t", CLI_SQL_MYSQL);
}

void test_a_semicolon_in_sqlites_quotes_ends_no_statement(void)
{
    assert_statements("select [a;b] from t|select \"c;d\"", "select [a;b] from t; select \"c;d\"", CLI_SQL_SQLITE);
}

void test_a_triggers_body_is_one_statement(void)
{
    assert_statements("create trigger t after insert on a begin insert into b values (1); update c set n = n + 1; end|select 1",
                      "create trigger t after insert on a begin insert into b values (1); update c set n = n + 1; end; select 1", CLI_SQL_SQLITE);
}

void test_a_routines_body_is_one_statement(void)
{
    static const char s_text[] = "CREATE PROCEDURE p() BEGIN IF 1 THEN SELECT 1; END IF; SELECT CASE WHEN 1 THEN 2 END; END; SELECT 2";
    char out[512];
    SqlStatements statements = {s_text, {out, sizeof(out), 0ull}, 0u};
    EMBED_CALL(cli_sql_text_length.statements, CliSqlTextLengthCfg, .text = s_text, .length = sizeof(s_text) - 1u, .dialect = CLI_SQL_MYSQL, .each = statement_recorded, .context = &statements);
    TEST_ASSERT_EQUAL_UINT(2u, statements.count);
}

void test_the_statement_at_a_position(void)
{
    assert_statement_at("select 1", position_of("1"));
}

void test_the_statement_just_past_its_end(void)
{
    assert_statement_at("select 1", position_of("1;") + 2u);
}

void test_no_statement_on_a_blank_line(void)
{
    assert_statement_at("", position_of("\n\n") + 1u);
}

void test_no_statement_in_a_comment_of_its_own(void)
{
    assert_statement_at("", position_of("note"));
}

void test_a_comment_after_a_statement_belongs_to_it(void)
{
    assert_statement_at("select 2", position_of("after"));
}

void test_the_last_statement_at_the_texts_end(void)
{
    assert_statement_at("select\n  3", sizeof(s_positions) - 1u);
}

void test_a_read_past_its_comment_reads(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_READS, does("/* x */ SELECT * FROM t", CLI_SQL_POSTGRES));
}

void test_a_write_within_a_with_writes(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_WRITES, does("with gone as (delete from t returning *) select * from gone", CLI_SQL_POSTGRES));
}

void test_explain_analyze_runs_its_statement(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_WRITES, does("explain analyze delete from t", CLI_SQL_POSTGRES));
}

void test_explain_alone_does_not_run_its_statement(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_READS, does("explain delete from t", CLI_SQL_POSTGRES));
}

void test_select_into_makes_a_table(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_SHAPES, does("select * into copy from t", CLI_SQL_POSTGRES));
}

void test_a_drop_changes_the_schema(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_SHAPES, does("drop table t", CLI_SQL_MYSQL));
}

void test_begin_opens_a_transaction(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_TRANSACTION, does("begin", CLI_SQL_SQLITE));
}

void test_a_pragma_that_queries_reads(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_READS, does("pragma table_info(t)", CLI_SQL_SQLITE));
}

void test_a_setting_is_other(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DOES_OTHER, does("set search_path = a", CLI_SQL_POSTGRES));
}

void test_an_update_with_no_where_is_counted(void)
{
    assert_counted("SELECT count(*) FROM only t as x", "update only t as x set n = 1", CLI_SQL_POSTGRES);
}

void test_a_where_in_a_subquery_does_not_bound_the_update(void)
{
    assert_counted("SELECT count(*) FROM t", "update t set n = (select max(n) from u where u.k = 1)", CLI_SQL_POSTGRES);
}

void test_an_update_with_a_where_is_not_counted(void)
{
    assert_counted(NULL, "update t set n = 1 where k = 2", CLI_SQL_POSTGRES);
}

void test_a_deletes_using_is_counted_with_it(void)
{
    assert_counted("SELECT count(*) FROM s.t, u", "delete from s.t using u returning *", CLI_SQL_POSTGRES);
}

void test_a_delete_with_no_where_is_counted_in_mysql(void)
{
    assert_counted("SELECT count(*) FROM t", "DELETE FROM t", CLI_SQL_MYSQL);
}

void test_a_joined_update_is_counted_in_mysql(void)
{
    assert_counted("SELECT count(*) FROM a join b on a.k = b.k", "update a join b on a.k = b.k set a.n = b.n", CLI_SQL_MYSQL);
}

void test_a_truncate_is_counted(void)
{
    assert_counted("SELECT count(*) FROM t", "truncate table t restart identity", CLI_SQL_POSTGRES);
}

void test_a_truncate_of_many_tables_adds_their_counts(void)
{
    assert_counted("SELECT (SELECT count(*) FROM a) + (SELECT count(*) FROM b)", "truncate a, b", CLI_SQL_POSTGRES);
}

void test_a_where_beside_a_string_of_where_bounds_the_delete(void)
{
    assert_counted(NULL, "delete from t where 'where' = ''", CLI_SQL_POSTGRES);
}

void test_a_read_is_not_counted(void)
{
    assert_counted(NULL, "select 1", CLI_SQL_POSTGRES);
}

void test_a_created_table_is_named(void)
{
    shaped("CREATE TABLE IF NOT EXISTS public.\"Orders\" (id int)", CLI_SQL_POSTGRES);
    TEST_ASSERT_EQUAL_STRING("create", s_shaped.verb);
    TEST_ASSERT_EQUAL_STRING("table", s_shaped.kind);
    TEST_ASSERT_EQUAL_STRING("public.\"Orders\"", s_shaped.name);
}

void test_a_created_index_names_its_table(void)
{
    shaped("create unique index concurrently ix on only s.t (a)", CLI_SQL_POSTGRES);
    TEST_ASSERT_EQUAL_STRING("index", s_shaped.kind);
    TEST_ASSERT_EQUAL_STRING("ix", s_shaped.name);
    TEST_ASSERT_TRUE(s_shaped.has_on);
    TEST_ASSERT_EQUAL_STRING("s.t", s_shaped.on);
}

void test_a_created_view_is_named(void)
{
    shaped("create or replace materialized view v as select 1", CLI_SQL_POSTGRES);
    TEST_ASSERT_EQUAL_STRING("view", s_shaped.kind);
    TEST_ASSERT_EQUAL_STRING("v", s_shaped.name);
}

void test_a_dropped_table_is_named(void)
{
    shaped("drop table if exists `db`.`t`", CLI_SQL_MYSQL);
    TEST_ASSERT_EQUAL_STRING("drop", s_shaped.verb);
    TEST_ASSERT_EQUAL_STRING("`db`.`t`", s_shaped.name);
}

void test_a_truncated_table_is_named(void)
{
    shaped("truncate t", CLI_SQL_POSTGRES);
    TEST_ASSERT_EQUAL_STRING("t", s_shaped.name);
}

void test_a_read_changes_no_schema(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_sql_length.shape, CliSqlLengthCfg, .statement = "select 1", .length = 8ull, .dialect = CLI_SQL_POSTGRES, .shaped = &s_shaped));
}
