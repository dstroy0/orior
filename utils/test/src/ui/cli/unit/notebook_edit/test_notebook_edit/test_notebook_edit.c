// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_notebook_edit.c: an open notebook's cells edited: one cell at a time has a view, a line put in grows the cell's
// rows and moves the rows after it while a letter typed does not, the notebook's own keys are taken and others are
// not, cells added, changed in type, moved, cleared and deleted with the notebook marked changed and a deleted cell
// that ran standing among the cells gone, and the menu of a cell refusing a move past either end
#include "ui/cli/src/command_line/commands.h"
#include "ui/cli/src/notebooks/notebook_cells_internal.h"
#include "ui/cli/src/notebooks/notebook_edit.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include "utils/test/src/ui/cli/unit/support/notebook_model_unity.h"

// a percent script of three cells, code, markdown and code, and the same as a notebook with a printed stream
static const char s_script[] = "# %%\nx = 1\nprint(x)\n\n# %% [markdown]\n# # Title\n\n# %%\ny = x + 1\n";
static const char s_notebook[] =
    "{\"cells\":[{\"cell_type\":\"code\",\"execution_count\":1,\"metadata\":{},\"outputs\":[{\"name\":\"stdout\","
    "\"output_type\":\"stream\",\"text\":[\"hi\\n\"]}],\"source\":[\"x = 1\"]},{\"cell_type\":\"markdown\",\"metadata\":{},"
    "\"source\":[\"# Title\"]},{\"cell_type\":\"code\",\"execution_count\":null,\"metadata\":{},\"outputs\":[],"
    "\"source\":[\"y = x + 1\"]}],\"metadata\":{},\"nbformat\":4,\"nbformat_minor\":5}\n";

static char s_why[512];
static VerbaLinea s_err_msg;

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ULL};
}

void tearDown(void)
{
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = "edit:a");
}

// the notebook `text` of the file `path` opened under edit:a: its model
static CliNotebookModel *opened(const char *path, const char *text)
{
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_open.open, CliNotebookCellsOpenCfg, .key = "edit:a",
                                        .path = path, .text = text, .size = CLI_UNITY_LENGTH(text),
                                        .err_msg = &s_err_msg));
    return EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = "edit:a");
}

// the keys handed to the cell `cell` of edit:a at `line` and `column`
static void focused(unsigned int cell, unsigned long long line, unsigned long long column)
{
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_focus.focus, CliNotebookEditFocusCfg, .key = "edit:a",
                                        .cell = cell, .line = line, .column = column, .err_msg = &s_err_msg));
}

// the input `input` handed to the cell of edit:a that has the keys
static void handed(const CliViewInput *input)
{
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_focus.input, CliNotebookEditFocusCfg, .key = "edit:a",
                                        .input = input, .err_msg = &s_err_msg));
}

// the rows of edit:a
static unsigned int rows_of(void)
{
    return EMBED_CALL(cli_notebook_cells_row.rows, CliNotebookCellsRowCfg, .key = "edit:a");
}

// whether the cell `cell` of `model` has a view open under its key
static int cell_viewed(CliNotebookModel *model, unsigned int cell)
{
    char key[CLI_PATH_MAX_LEN + CLI_NOTEBOOK_KEY_MORE];
    EMBED_CALL(cli_notebook_model_internal.key, CliNotebookModelCtx, .model = model, .cell = cell,
               .output = CLI_NOTEBOOK_NONE, .out = key);
    return EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key);
}

void test_one_cell_at_a_time_has_a_view(void)
{
    CliNotebookModel *const model = opened("edit.py", s_script);
    TEST_ASSERT_FALSE(cell_viewed(model, 0u));
    focused(0u, 0ULL, 0ULL);
    TEST_ASSERT_TRUE(cell_viewed(model, 0u));
    TEST_ASSERT_EQUAL_UINT(0u, model->focused);
    focused(2u, 0ULL, 0ULL);
    TEST_ASSERT_FALSE(cell_viewed(model, 0u));
    TEST_ASSERT_TRUE(cell_viewed(model, 2u));
    TEST_ASSERT_EQUAL_UINT(2u, model->focused);
    EMBED_CALL(cli_notebook_edit_focus.blur, CliNotebookEditFocusCfg, .key = "edit:a", .err_msg = &s_err_msg);
    TEST_ASSERT_FALSE(cell_viewed(model, 2u));
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_NONE, model->focused);
}

void test_a_line_put_in_grows_the_cells_rows_and_a_letter_typed_does_not(void)
{
    const CliNotebookModel *const model = opened("edit.py", s_script);
    focused(0u, 1ULL, 8ULL);
    const unsigned int rows = rows_of();
    const unsigned int after = notebook_first_row_at(model, 2u);
    const CliViewInput letter = {.type = CLI_VIEW_TEXT, .text = "z", .length = 1ULL};
    handed(&letter);
    TEST_ASSERT_EQUAL_UINT(rows, rows_of());
    TEST_ASSERT_EQUAL_UINT(after, notebook_first_row_at(model, 2u));
    TEST_ASSERT_TRUE(notebook_cell_at(model, 0u).edited != 0u);
    const CliViewInput line = {.type = CLI_VIEW_KEY, .name = "Enter"};
    handed(&line);
    TEST_ASSERT_EQUAL_UINT(3u, notebook_cell_at(model, 0u).lines);
    TEST_ASSERT_EQUAL_UINT(rows + 1u, rows_of());
    TEST_ASSERT_EQUAL_UINT(after + 1u, notebook_first_row_at(model, 2u));
}

void test_the_notebooks_keys_are_taken_and_others_are_not(void)
{
    const CliNotebookModel *const model = opened("edit.py", s_script);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_edit_key.key, CliNotebookEditKeyCfg, .key = "edit:a",
                                        .word = "Escape"));
    focused(1u, 0ULL, 0ULL);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_edit_key.key, CliNotebookEditKeyCfg, .key = "edit:a",
                                        .word = "a"));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_key.key, CliNotebookEditKeyCfg, .key = "edit:a",
                                        .word = "Shift+Enter"));
    TEST_ASSERT_EQUAL_UINT(2u, model->focused);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_key.key, CliNotebookEditKeyCfg, .key = "edit:a",
                                        .word = "Escape"));
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_NONE, model->focused);
    focused(1u, 0ULL, 0ULL);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_key.key, CliNotebookEditKeyCfg, .key = "edit:a",
                                        .word = "Ctrl+Enter"));
    focused(1u, 0ULL, 0ULL);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_key.key, CliNotebookEditKeyCfg, .key = "edit:a",
                                        .word = "Meta+Enter"));
}

void test_cells_added_changed_moved_cleared_and_deleted(void)
{
    CliNotebookModel *const model = opened("edit.ipynb", s_notebook);
    TEST_ASSERT_EQUAL_INT(0, model->dirty);
    const unsigned int rows = rows_of();
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_cell.add, CliNotebookEditCellCfg, .key = "edit:a",
                                        .cell = 0u, .type = CLI_NOTEBOOK_CODE));
    TEST_ASSERT_EQUAL_UINT(4u, model->cells);
    TEST_ASSERT_EQUAL_UINT(1u, model->focused);
    TEST_ASSERT_EQUAL_UINT(rows + 2u, rows_of());
    TEST_ASSERT_EQUAL_INT(1, model->dirty);
    model->dirty = 0;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_cell.retype, CliNotebookEditCellCfg, .key = "edit:a",
                                        .cell = 1u));
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_MARKDOWN, notebook_cell_at(model, 1u).type);
    TEST_ASSERT_EQUAL_INT(1, model->dirty);
    const unsigned int serial = notebook_cell_at(model, 0u).serial;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_cell.move, CliNotebookEditCellCfg, .key = "edit:a",
                                        .cell = 0u, .by = 1));
    TEST_ASSERT_EQUAL_UINT(serial, notebook_cell_at(model, 1u).serial);
    TEST_ASSERT_EQUAL_UINT(1u, notebook_cell_at(model, 1u).outputs);
    TEST_ASSERT_EQUAL_UINT(0u, notebook_cell_at(model, 1u).output_first);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_edit_cell.move, CliNotebookEditCellCfg, .key = "edit:a",
                                        .cell = 3u, .by = 1));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_cell.outputs_clear, CliNotebookEditCellCfg,
                                        .key = "edit:a", .cell = 1u));
    TEST_ASSERT_EQUAL_UINT(0u, model->outputs);
    // the cell at 1 ran first and the last cell after it; the first deleted, the last stands on a cell gone
    model->clock = 2u;
    CliNotebookCell second = notebook_cell_at(model, 1u);
    second.first_run = 1u;
    second.last_run = 1u;
    notebook_cell_set(model, 1u, &second);
    CliNotebookCell fourth = notebook_cell_at(model, 3u);
    fourth.first_run = 2u;
    fourth.last_run = 2u;
    notebook_cell_set(model, 3u, &fourth);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_cell.remove, CliNotebookEditCellCfg, .key = "edit:a",
                                        .cell = 1u));
    TEST_ASSERT_EQUAL_UINT(3u, model->cells);
    TEST_ASSERT_EQUAL_UINT(1u, model->gone_count);
    TEST_ASSERT_EQUAL_UINT(1u, notebook_gone_at(model, 0u).last_run);
    TEST_ASSERT_EQUAL_INT(1, notebook_cell_at(model, 2u).stale);
}

void test_the_cell_menu_refuses_a_move_past_either_end(void)
{
    (void)opened("edit.py", s_script);
    unsigned int count = 0u;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_menu.menu, CliNotebookEditMenuCfg, .key = "edit:a",
                                        .cell = 0u));
    const CliItem *rows = cli_menus_own_rows(&count);
    int up_refused = 0;
    int down_refused = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (rows[at].line || (rows[at].label == NULL))
        {
            continue;
        }
        const int refused = cellul.eq(&(CatenaFinitaCfg){.src = rows[at].needs, .other = "never", .cap = 8ULL});
        up_refused = cellul.eq(&(CatenaFinitaCfg){.src = rows[at].label, .other = "Move Up", .cap = 16ULL}) ? refused
                                                                                                         : up_refused;
        down_refused = cellul.eq(&(CatenaFinitaCfg){.src = rows[at].label, .other = "Move Down", .cap = 16ULL})
                           ? refused
                           : down_refused;
    }
    TEST_ASSERT_EQUAL_INT(1, up_refused);
    TEST_ASSERT_EQUAL_INT(0, down_refused);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_menu.menu, CliNotebookEditMenuCfg, .key = "edit:a",
                                        .cell = 2u));
    rows = cli_menus_own_rows(&count);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (rows[at].line || (rows[at].label == NULL))
        {
            continue;
        }
        const int refused = cellul.eq(&(CatenaFinitaCfg){.src = rows[at].needs, .other = "never", .cap = 8ULL});
        up_refused = cellul.eq(&(CatenaFinitaCfg){.src = rows[at].label, .other = "Move Up", .cap = 16ULL}) ? refused
                                                                                                         : up_refused;
        down_refused = cellul.eq(&(CatenaFinitaCfg){.src = rows[at].label, .other = "Move Down", .cap = 16ULL})
                           ? refused
                           : down_refused;
    }
    TEST_ASSERT_EQUAL_INT(0, up_refused);
    TEST_ASSERT_EQUAL_INT(1, down_refused);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_menu.command, CliNotebookEditMenuCfg, .key = "edit:a",
                                        .cell = CLI_NOTEBOOK_NONE, .output = CLI_NOTEBOOK_NONE,
                                        .word = "notebook-up"));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_edit_menu.command, CliNotebookEditMenuCfg, .key = "edit:a",
                                        .cell = 0u, .output = CLI_NOTEBOOK_NONE, .word = "file-open"));
}
