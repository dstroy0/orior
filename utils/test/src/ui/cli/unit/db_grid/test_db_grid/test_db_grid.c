// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_db_grid.c: the grid over a SQLite table of 500 rows: an answer adopted stores its columns, its first page and
// its end, the rows read and one more while the server has more; each column's width is the longest of its label and
// its first rows cut to the most, with the gap, laid at the adopt; the step reads the stretch a view moved past the read
// rows heads into and lets go the rows far behind; the reach stores two numbers and takes no cell; a null cell has its
// flag and a numbers column its own; the pan stays inside the columns; a failure is its lines; and a statement with no
// rows is what it did. The cases need the tree's Python, which runs SQLite, and are ignored on a machine with none
#include "ui/cli/src/databases/db_grid.h"
#include "ui/cli/src/databases/db_internal.h"
#include "ui/cli/src/databases/db_pane.h"
#include "ui/cli/src/tests/test_runs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_answer[1u << 20u];
static unsigned long long s_answer_size;
static CliDbFailure s_failure;
static CliDbOpen *s_lite;
static int s_lite_tried;

// `statement` run on the table's connection, its answer in s_answer: 1, or 0 with its failure in s_failure
static int statement_run(const char *statement)
{
    VerbaLinea out = {s_answer, sizeof(s_answer), 0ULL};
    const int run = EMBED_CALL(cli_db_out_failure.run, CliDbOutFailureCfg, .open = s_lite, .statement = statement,
                               .size = CLI_UNITY_LENGTH(statement), .answered = 1, .page = CLI_DB_GRID_PAGE,
                               .out = &out, .failure = &s_failure);
    s_answer_size = out.at;
    return run;
}

// `statement` run and its answer adopted
static void adopted(const char *statement)
{
    TEST_ASSERT_TRUE(statement_run(statement));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.adopt, CliDbGridCfg, .answer = s_answer, .size = s_answer_size,
                                .open = s_lite, .title = "lite"));
}

// a SQLite database of a table of 500 rows opened once for the process, the case ignored with no Python
static void lite_ready(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("no Python on this machine: SQLite is not opened");
    }
    if (!s_lite_tried)
    {
        s_lite_tried = 1;
        char root[CLI_PATH_MAX_LEN];
        cli_unity_folder("db_grid_tree", root);
        CliDbConnection connection;
        memor.set(&(MemoriaCfg){.dst = &connection, .val = 0, .bytes = sizeof(connection)});
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.name, .src = "lite", .cap = CLI_NAME_MAX_LEN});
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.kind, .src = "sqlite", .cap = 16ULL});
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.file, .src = "grid.db", .cap = CLI_PATH_MAX_LEN});
        s_lite = EMBED_CALL(cli_db_root.connect, CliDbRootCfg, .root = root, .config = &connection, .password = NULL,
                            .failure = &s_failure);
        if (s_lite != NULL)
        {
            (void)statement_run("CREATE TABLE t (n INTEGER PRIMARY KEY, word TEXT, note TEXT)");
            (void)statement_run("INSERT INTO t (n, word, note) WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 "
                                "FROM k WHERE i < 500) SELECT i, 'w' || i, CASE WHEN i = 2 THEN NULL ELSE "
                                "'this note runs well past the forty letters a column is laid at' END FROM k");
        }
    }
    TEST_ASSERT_NOT_NULL(s_lite);
}

// the column `at` into a column
static CliDbGridColumn column_at(unsigned int at)
{
    CliDbGridColumn column;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.column, CliDbGridCfg, .column_at = at, .column = &column));
    return column;
}

// the step taken with the view reaching `first` to `last`
static int stepped(unsigned long long first, unsigned long long last, unsigned long long now_ms)
{
    EMBED_CALL(cli_db_grid.reach, CliDbGridCfg, .first = first, .last = last);
    return EMBED_CALL(cli_db_grid_step.step, CliDbGridStepCfg, .now_ms = now_ms);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_an_answer_adopted_stores_its_columns_its_first_page_and_its_end(void)
{
    lite_ready();
    adopted("SELECT n, word, note FROM t ORDER BY n");
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_db_grid.columns, CliDbGridCfg, .at = 0ULL));
    const CliDbGridColumn word = column_at(1u);
    CLI_TEST_ASSERT_TEXT("word", word.label, word.label_length);
    TEST_ASSERT_EQUAL_UINT64(CLI_DB_GRID_PAGE + 1ULL, EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL));
    CliDbGridRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.row, CliDbGridCfg, .at = CLI_DB_GRID_PAGE, .row = &row));
    TEST_ASSERT_EQUAL_UINT(CLI_DB_GRID_ROW_MORE, row.kind);
    unsigned long long length = 0ULL;
    const char *const sentence = EMBED_CALL(cli_db_grid.head_text, CliDbGridCfg, .length_out = &length);
    CLI_TEST_ASSERT_CONTAINS("lite: 200+ rows", sentence);
    adopted("SELECT n FROM t WHERE n <= 3");
    TEST_ASSERT_EQUAL_UINT64(3ULL, EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL));
    CLI_TEST_ASSERT_CONTAINS("lite: 3 rows", EMBED_CALL(cli_db_grid.head_text, CliDbGridCfg, .length_out = &length));
}

void test_each_width_is_the_longest_of_its_label_and_first_rows_cut_to_the_most(void)
{
    lite_ready();
    adopted("SELECT n, word, note FROM t ORDER BY n");
    // n's longest of its first fifty rows is "50", shorter than nothing but its two digits, with the gap after
    TEST_ASSERT_EQUAL_UINT(2u + CLI_DB_GRID_WIDTH_GAP, column_at(0u).width);
    TEST_ASSERT_EQUAL_UINT(4u + CLI_DB_GRID_WIDTH_GAP, column_at(1u).width);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_GRID_WIDTH_MOST + CLI_DB_GRID_WIDTH_GAP, column_at(2u).width);
    TEST_ASSERT_EQUAL_INT64(0LL, column_at(0u).start);
    TEST_ASSERT_EQUAL_INT64((long long)(column_at(0u).width + column_at(1u).width), column_at(2u).start);
    // a step reads rows and lays no width again
    (void)stepped(300ULL, 320ULL, 1000ULL);
    TEST_ASSERT_EQUAL_UINT(2u + CLI_DB_GRID_WIDTH_GAP, column_at(0u).width);
}

void test_the_step_reads_what_the_view_heads_into_and_lets_go_the_rows_behind(void)
{
    lite_ready();
    adopted("SELECT n, word, note FROM t ORDER BY n");
    TEST_ASSERT_FALSE(stepped(0ULL, 20ULL, 1000ULL));
    TEST_ASSERT_TRUE(stepped(300ULL, 320ULL, 1100ULL));
    unsigned long long first = 0ULL;
    const unsigned long long stored = EMBED_CALL(cli_db_grid.stored, CliDbGridCfg, .first_out = &first);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(300ULL, first);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(321ULL, first + stored);
    TEST_ASSERT_GREATER_THAN_UINT64(0ULL, first);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(3ULL * CLI_DB_GRID_PAGE + 21ULL + CLI_DB_GRID_SPARE, stored);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(321ULL, EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL));
    CliDbGridCell cell;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.cell, CliDbGridCfg, .at = 300ULL, .column_at = 0u, .cell = &cell));
    CLI_TEST_ASSERT_TEXT("301", cell.text, cell.length);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_grid.cell, CliDbGridCfg, .at = 0ULL, .column_at = 0u, .cell = &cell));
    CliDbGridRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.row, CliDbGridCfg, .at = 0ULL, .row = &row));
    TEST_ASSERT_EQUAL_UINT(CLI_DB_GRID_ROW_WAIT, row.kind);
    // a view standing inside the stretch reads nothing
    TEST_ASSERT_FALSE(stepped(305ULL, 325ULL, 1200ULL));
}

void test_the_reach_stores_two_numbers_and_takes_no_cell(void)
{
    lite_ready();
    adopted("SELECT n, word, note FROM t ORDER BY n");
    const unsigned long long before = mmgr_owner_in_use(g_cli_db_pane_owner);
    EMBED_CALL(cli_db_grid.reach, CliDbGridCfg, .first = 450ULL, .last = 470ULL);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_cli_db_pane_owner));
}

void test_a_null_cell_has_its_flag_and_a_numbers_column_its_own(void)
{
    lite_ready();
    adopted("SELECT n, word, note FROM t ORDER BY n");
    CliDbGridCell cell;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.cell, CliDbGridCfg, .at = 1ULL, .column_at = 2u, .cell = &cell));
    TEST_ASSERT_TRUE(cell.null);
    CLI_TEST_ASSERT_TEXT("NULL", cell.text, cell.length);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.cell, CliDbGridCfg, .at = 0ULL, .column_at = 0u, .cell = &cell));
    TEST_ASSERT_FALSE(cell.null);
    TEST_ASSERT_TRUE(cell.numeric);
    TEST_ASSERT_TRUE(column_at(0u).numeric);
    TEST_ASSERT_FALSE(column_at(1u).numeric);
}

void test_the_pan_stays_inside_the_columns(void)
{
    lite_ready();
    adopted("SELECT n, word, note FROM t ORDER BY n");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = -1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid_step.key, CliDbGridStepCfg, .name = "Right"));
    TEST_ASSERT_EQUAL_INT64(0LL, column_at(1u).start);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = 10));
    TEST_ASSERT_EQUAL_INT64(0LL, column_at(2u).start);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_grid_step.pan, CliDbGridStepCfg, .columns = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid_step.key, CliDbGridStepCfg, .name = "Home"));
    TEST_ASSERT_EQUAL_INT64(0LL, column_at(0u).start);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_grid_step.key, CliDbGridStepCfg, .name = "PageDown"));
}

void test_a_failure_is_its_lines(void)
{
    lite_ready();
    TEST_ASSERT_FALSE(statement_run("SELECT * FROM no_such_table"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.adopt, CliDbGridCfg, .answer = NULL, .failure = &s_failure,
                                .open = s_lite, .title = "lite"));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_db_grid.columns, CliDbGridCfg, .at = 0ULL));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(1ULL, EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL));
    CliDbGridRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.row, CliDbGridCfg, .at = 0ULL, .row = &row));
    TEST_ASSERT_EQUAL_UINT(CLI_DB_GRID_ROW_FAILURE, row.kind);
    TEST_ASSERT_NOT_NULL(cellul.find(&(CatenaFinitaCfg){.src = row.text, .cap = row.length,
                                                        .other = "no_such_table", .other_cap = 32ULL}));
}

void test_a_statement_with_no_rows_is_what_it_did(void)
{
    lite_ready();
    adopted("UPDATE t SET word = word WHERE n = 1");
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_db_grid.columns, CliDbGridCfg, .at = 0ULL));
    CliDbGridRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.row, CliDbGridCfg, .at = 0ULL, .row = &row));
    TEST_ASSERT_EQUAL_UINT(CLI_DB_GRID_ROW_NOTICE, row.kind);
    CLI_TEST_ASSERT_CONTAINS("1", row.text);
    EMBED_CALL(cli_db_grid.clear, CliDbGridCfg, .open = NULL);
    TEST_ASSERT_NULL(EMBED_CALL(cli_db_grid.open, CliDbGridCfg, .at = 0ULL));
}
