// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_db_pane.c: the Database pane over a tree of the suite's own with a SQLite connection: the connections read with
// none chosen stand as one sentence; the one chosen is not connected until Connect opens it and its schema's top
// level is the rows; a node pressed reads its level once and a second press closes it; Commit and Roll Back act only
// while a transaction is open; a DELETE of every row asks first with its count and runs only on Run It; a refused
// connection stands as its failure; a node's menu and the pane's own are the catalog's commands; the form adds a
// connection to databases.json and removes it; and Show Its First Rows hands its rows to the grid. The cases need the
// tree's Python, which runs SQLite, and are ignored on a machine with none
#include "ui/cli/src/databases/db_grid.h"
#include "ui/cli/src/databases/db_internal.h"
#include "ui/cli/src/databases/db_pane.h"
#include "ui/cli/src/tests/test_runs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[CLI_PATH_MAX_LEN];
static int s_tree_laid;

// the connection `name` of SQLite at `file`, the rest empty, saved in the suite's tree
static void lite_saved(const char *name, const char *file)
{
    CliDbConnection connection;
    memor.set(&(MemoriaCfg){.dst = &connection, .val = 0, .bytes = sizeof(connection)});
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.name, .src = name, .cap = CLI_NAME_MAX_LEN});
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.kind, .src = "sqlite", .cap = 16ULL});
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.file, .src = file, .cap = CLI_PATH_MAX_LEN});
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .root = s_root, .connection = &connection,
                                .was = NULL, .err_msg = &err_msg));
}

// the tree laid once for the process, the case ignored with no Python
static void tree_ready(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("no Python on this machine: SQLite is not opened");
    }
    if (!s_tree_laid)
    {
        s_tree_laid = 1;
        cli_unity_folder("db_pane_tree", s_root);
        lite_saved("lite", "app.db");
        lite_saved("broken", "no/such/folder/app.db");
        EMBED_CALL(cli_db_pane.begin, CliDbPaneCfg, .woken = NULL);
        EMBED_CALL(cli_db_pane.read, CliDbPaneCfg, .root = s_root);
    }
}

// the step taken until the run on its thread ends: 1 where it ended within ten seconds
static int run_ended(void)
{
    for (unsigned int tries = 0u; tries < 1000u; tries += 1u)
    {
        if (EMBED_CALL(cli_db_pane.step, CliDbPaneCfg, .root = s_root, .now_ms = cli_clock_milliseconds()))
        {
            return 1;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
    return 0;
}

// `statement` run on the chosen connection and its run waited for
static void ran(const char *statement)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane.run, CliDbPaneCfg, .root = s_root, .statement = statement,
                                .size = CLI_UNITY_LENGTH(statement), .answered = 0));
    TEST_ASSERT_TRUE(run_ended());
}

// the row `at` into `row`
static CliDbPaneRow row_at(unsigned int at)
{
    CliDbPaneRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane.row, CliDbPaneCfg, .at = at, .row = &row));
    return row;
}

// the row whose text is `text`, CLI_DB_PANE_NONE where none is
static unsigned int row_of(const char *text)
{
    const unsigned int rows = EMBED_CALL(cli_db_pane.rows, CliDbPaneCfg, .at = 0u);
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        const CliDbPaneRow row = row_at(at);
        if ((row.text != NULL) && (row.text_length == CLI_UNITY_LENGTH(text)) &&
            (memor.cmp(&(MemoriaCfg){.src = row.text, .other = text, .bytes = row.text_length}) == 0))
        {
            return at;
        }
    }
    return CLI_DB_PANE_NONE;
}

// whether the head's tool `which` can act
static int tool_enabled(unsigned int which)
{
    int enabled = 0;
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_db_pane_head.head_tool, CliDbPaneHeadCfg, .which = which, .enabled_out = &enabled,
                     .length_out = &length);
    return enabled;
}

// the head's tool `which`'s label
static const char *tool_label(unsigned int which, unsigned long long *length)
{
    int enabled = 0;
    return EMBED_CALL(cli_db_pane_head.head_tool, CliDbPaneHeadCfg, .which = which, .enabled_out = &enabled,
                      .length_out = length);
}

// the chosen connection set to `name` through db-choose
static void chosen(const char *name)
{
    const char *const words[1] = {name};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane_tree.command, CliDbPaneTreeCfg, .name = "db-choose", .words = words,
                                .count = 1u, .root = s_root));
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_the_connections_read_with_none_chosen_stand_as_one_sentence(void)
{
    tree_ready();
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_db_pane.rows, CliDbPaneCfg, .at = 0u));
    const CliDbPaneRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_NOTE, row.kind);
    CLI_TEST_ASSERT_TEXT("No connection", row.text, row.text_length);
    TEST_ASSERT_TRUE(tool_enabled(CLI_DB_PANE_PICK));
    TEST_ASSERT_FALSE(tool_enabled(CLI_DB_PANE_CONNECT));
}

void test_the_chosen_connection_is_not_connected_until_connect_opens_its_schema(void)
{
    tree_ready();
    chosen("lite");
    unsigned long long length = 0ULL;
    const char *const title = EMBED_CALL(cli_db_pane.title, CliDbPaneCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("lite (sqlite)", title, length);
    CliDbPaneRow row = row_at(0u);
    CLI_TEST_ASSERT_TEXT("lite is not connected: Connect reads its schema.", row.text, row.text_length);
    const char *label = tool_label(CLI_DB_PANE_CONNECT, &length);
    CLI_TEST_ASSERT_TEXT("Connect", label, length);
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_CONNECT, .root = s_root);
    label = tool_label(CLI_DB_PANE_CONNECT, &length);
    CLI_TEST_ASSERT_TEXT("Disconnect", label, length);
    row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_NODE, row.kind);
    CLI_TEST_ASSERT_TEXT("main", row.text, row.text_length);
    TEST_ASSERT_TRUE(row.opens);
    TEST_ASSERT_FALSE(row.open);
    CLI_TEST_ASSERT_CONTAINS("\xE2\x97\x8F", EMBED_CALL(cli_db_pane.title, CliDbPaneCfg, .length_out = &length));
}

void test_a_node_pressed_reads_its_level_once_and_a_second_press_closes_it(void)
{
    tree_ready();
    ran("CREATE TABLE t (n INTEGER PRIMARY KEY, name TEXT)");
    ran("INSERT INTO t (n, name) VALUES (1, 'one'), (2, 'two'), (3, NULL)");
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_READ, .root = s_root);
    const unsigned int main_row = row_of("main");
    TEST_ASSERT_NOT_EQUAL_UINT(CLI_DB_PANE_NONE, main_row);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = main_row, .root = s_root));
    const unsigned int table_row = row_of("t");
    TEST_ASSERT_NOT_EQUAL_UINT(CLI_DB_PANE_NONE, table_row);
    TEST_ASSERT_EQUAL_UINT(1u, row_at(table_row).depth);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = table_row, .root = s_root));
    const unsigned int column_row = row_of("name");
    TEST_ASSERT_NOT_EQUAL_UINT(CLI_DB_PANE_NONE, column_row);
    const CliDbPaneRow column = row_at(column_row);
    CLI_TEST_ASSERT_CONTAINS("TEXT", column.detail);
    TEST_ASSERT_FALSE(column.opens);
    const unsigned int open_rows = EMBED_CALL(cli_db_pane.rows, CliDbPaneCfg, .at = 0u);
    // the level stands stored: opened again it is read from the store, no cell taken for it, the owner's bytes in use
    // back where the first open left them
    const unsigned long long open_use = mmgr_owner_in_use(g_cli_db_pane_owner);
    (void)EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = table_row, .root = s_root);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_NONE, row_of("name"));
    (void)EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = table_row, .root = s_root);
    TEST_ASSERT_EQUAL_UINT(open_rows, EMBED_CALL(cli_db_pane.rows, CliDbPaneCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT64(open_use, mmgr_owner_in_use(g_cli_db_pane_owner));
}

void test_commit_and_roll_back_act_only_while_a_transaction_is_open(void)
{
    tree_ready();
    TEST_ASSERT_FALSE(tool_enabled(CLI_DB_PANE_COMMIT));
    TEST_ASSERT_FALSE(tool_enabled(CLI_DB_PANE_ROLLBACK));
    TEST_ASSERT_FALSE(tool_enabled(CLI_DB_PANE_STOP));
    ran("BEGIN");
    TEST_ASSERT_TRUE(tool_enabled(CLI_DB_PANE_COMMIT));
    TEST_ASSERT_TRUE(tool_enabled(CLI_DB_PANE_ROLLBACK));
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_ROLLBACK, .root = s_root);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane.running, CliDbPaneCfg, .at = 0u));
    TEST_ASSERT_TRUE(tool_enabled(CLI_DB_PANE_STOP));
    TEST_ASSERT_TRUE(run_ended());
    TEST_ASSERT_FALSE(tool_enabled(CLI_DB_PANE_COMMIT));
}

void test_a_delete_of_every_row_asks_first_and_runs_only_on_run_it(void)
{
    tree_ready();
    ran("DELETE FROM t");
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_PROMPT, row_at(0u).kind);
    CliDbPaneRow sentence = row_at(0u);
    CLI_TEST_ASSERT_TEXT("This DELETE reaches every row on lite: 3 rows.", sentence.prompt.text,
                         sentence.prompt.length);
    const CliDbPaneRow statement = row_at(1u);
    CLI_TEST_ASSERT_TEXT("DELETE FROM t", statement.prompt.text, statement.prompt.length);
    const CliDbPaneRow buttons = row_at(2u);
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_BUTTONS, buttons.prompt.kind);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = 2u, .choice = 1u, .root = s_root));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_pane.running, CliDbPaneCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_NODE, row_at(0u).kind);
    ran("DELETE FROM t");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = 2u, .choice = 0u, .root = s_root));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane.running, CliDbPaneCfg, .at = 0u));
    TEST_ASSERT_TRUE(run_ended());
    ran("SELECT count(*) FROM t");
    CliDbGridCell cell;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_grid.cell, CliDbGridCfg, .at = 0ULL, .column_at = 0u, .cell = &cell));
    CLI_TEST_ASSERT_TEXT("0", cell.text, cell.length);
    ran("INSERT INTO t (n, name) VALUES (1, 'one'), (2, 'two'), (3, NULL)");
}

void test_a_refused_connection_stands_as_its_failure(void)
{
    tree_ready();
    chosen("broken");
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_CONNECT, .root = s_root);
    const CliDbPaneRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_NOTE, row.kind);
    TEST_ASSERT_TRUE(row.failed);
    TEST_ASSERT_NOT_EQUAL_UINT64(0ULL, row.text_length);
    chosen("lite");
}

void test_a_nodes_menu_and_the_panes_own_are_the_catalogs_commands(void)
{
    tree_ready();
    const unsigned int table_row = row_of("t");
    TEST_ASSERT_NOT_EQUAL_UINT(CLI_DB_PANE_NONE, table_row);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane_tree.menu, CliDbPaneTreeCfg, .at = table_row));
    unsigned int count = 0u;
    const CliItem *rows = cli_menus_own_rows(&count);
    TEST_ASSERT_EQUAL_UINT(4u, count);
    TEST_ASSERT_EQUAL_STRING("db-copy-name", rows[0].command);
    TEST_ASSERT_EQUAL_STRING("t", rows[0].args);
    TEST_ASSERT_EQUAL_STRING("db-first-rows", rows[1].command);
    TEST_ASSERT_TRUE(rows[2].line);
    TEST_ASSERT_EQUAL_STRING("db-read-again", rows[3].command);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane_tree.menu, CliDbPaneTreeCfg, .at = CLI_DB_PANE_NONE));
    rows = cli_menus_own_rows(&count);
    TEST_ASSERT_EQUAL_UINT(2u, count);
    TEST_ASSERT_EQUAL_STRING("db-choose", rows[0].command);
    TEST_ASSERT_EQUAL_STRING("lite", rows[0].args);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg);
    CliItem item;
    TEST_ASSERT_TRUE(cli_menus_named("db-choose", &item));
    TEST_ASSERT_TRUE(cli_menus_named("db-first-rows", &item));
}

void test_show_its_first_rows_hands_the_tables_rows_to_the_grid(void)
{
    tree_ready();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane_tree.menu, CliDbPaneTreeCfg, .at = row_of("t")));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane_tree.command, CliDbPaneTreeCfg, .name = "db-first-rows", .count = 0u,
                                .root = s_root));
    TEST_ASSERT_TRUE(run_ended());
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_db_grid.columns, CliDbGridCfg, .at = 0ULL));
    TEST_ASSERT_EQUAL_UINT64(3ULL, EMBED_CALL(cli_db_grid.rows, CliDbGridCfg, .at = 0ULL));
}

void test_the_form_adds_a_connection_and_removes_it(void)
{
    tree_ready();
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_NEW, .root = s_root);
    CliDbPaneRow row = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_FIELD, row.kind);
    TEST_ASSERT_TRUE(row.focused);
    EMBED_CALL(cli_db_pane_input.type, CliDbPaneInputCfg, .text = "second", .size = 6ULL);
    // the kind stepped from PostgreSQL to SQLite, and the file typed into its field
    (void)EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = 1u, .root = s_root);
    (void)EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = 1u, .root = s_root);
    row = row_at(1u);
    CLI_TEST_ASSERT_TEXT("SQLite", row.detail, row.detail_length);
    row = row_at(2u);
    CLI_TEST_ASSERT_TEXT("File", row.text, row.text_length);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = 2u, .root = s_root));
    EMBED_CALL(cli_db_pane_input.type, CliDbPaneInputCfg, .text = "two.db", .size = 6ULL);
    const unsigned int rows = EMBED_CALL(cli_db_pane.rows, CliDbPaneCfg, .at = 0u);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_BUTTONS, row_at(rows - 1u).kind);
    (void)EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = rows - 1u, .choice = 0u, .root = s_root);
    unsigned long long length = 0ULL;
    // the title read before the assertion, which reads the length first
    const char *const title = EMBED_CALL(cli_db_pane.title, CliDbPaneCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("second (sqlite)", title, length);
    CliDbConnection found;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_db_root_name.by_name, CliDbRootNameCfg, .root = s_root, .name = "second",
                                .connection_out = &found, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("two.db", found.file);
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_EDIT, .root = s_root);
    const unsigned int edit_rows = EMBED_CALL(cli_db_pane.rows, CliDbPaneCfg, .at = 0u);
    const CliDbPaneRow buttons = row_at(edit_rows - 1u);
    TEST_ASSERT_EQUAL_UINT(2u, buttons.prompt.choices);
    CLI_TEST_ASSERT_TEXT("Remove", buttons.prompt.choice[1], buttons.prompt.choice_length[1]);
    (void)EMBED_CALL(cli_db_pane_tree.press, CliDbPaneTreeCfg, .at = edit_rows - 1u, .choice = 1u, .root = s_root);
    err_msg.at = 0ULL;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_db_root_name.by_name, CliDbRootNameCfg, .root = s_root, .name = "second",
                                 .connection_out = &found, .err_msg = &err_msg));
    const char *const none = EMBED_CALL(cli_db_pane.title, CliDbPaneCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("No connection", none, length);
}

void test_disconnect_closes_the_connection_and_lets_its_result_go(void)
{
    tree_ready();
    chosen("lite");
    unsigned long long length = 0ULL;
    const char *label = tool_label(CLI_DB_PANE_CONNECT, &length);
    CLI_TEST_ASSERT_TEXT("Disconnect", label, length);
    ran("SELECT 1");
    TEST_ASSERT_NOT_NULL(EMBED_CALL(cli_db_grid.open, CliDbGridCfg, .at = 0ULL));
    EMBED_CALL(cli_db_pane_head.action, CliDbPaneHeadCfg, .which = CLI_DB_PANE_CONNECT, .root = s_root);
    TEST_ASSERT_NULL(EMBED_CALL(cli_db_grid.open, CliDbGridCfg, .at = 0ULL));
    label = tool_label(CLI_DB_PANE_CONNECT, &length);
    CLI_TEST_ASSERT_TEXT("Connect", label, length);
    TEST_ASSERT_EQUAL_UINT(CLI_DB_PANE_ROW_NOTE, row_at(0u).kind);
}
