// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_notebook_cells.c: an open notebook's model: a Jupyter notebook and a percent script read as the same rows, a row
// found among the cells' starts, a kernel's messages applied by the step to the cell their run is bound to and to the
// model their context is, one standing until its run is bound, the ring sized to the bytes pending and released
// drained, the out of date marks and their sentence, the sentences the rows show, the save and the changed mark, and the
// frame laid and released with the owner's bytes back where they began, the top moved and stopped at the last row
#include "ui/cli/src/notebooks/notebook_cells_internal.h"
#include "ui/cli/src/notebooks/notebook_edit.h"
#include "ui/cli/src/notebooks/notebook_kernel.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include "utils/test/src/ui/cli/unit/support/notebook_model_unity.h"

// a notebook of three cells, code, markdown and code, the first with a stream printed, and the same cells as a
// percent script and as a notebook with no outputs
static const char s_notebook[] =
    "{\"cells\":[{\"cell_type\":\"code\",\"execution_count\":1,\"id\":\"a\",\"metadata\":{},\"outputs\":[{\"name\":"
    "\"stdout\",\"output_type\":\"stream\",\"text\":[\"hi\\n\"]}],\"source\":[\"x = 1\\n\",\"print(x)\"]},{\"cell_type\":"
    "\"markdown\",\"id\":\"b\",\"metadata\":{},\"source\":[\"# Title\"]},{\"cell_type\":\"code\",\"execution_count\":null,"
    "\"id\":\"c\",\"metadata\":{},\"outputs\":[],\"source\":[\"y = x + 1\"]}],\"metadata\":{\"kernelspec\":{"
    "\"display_name\":\"Python 3\",\"language\":\"python\",\"name\":\"python3\"}},\"nbformat\":4,\"nbformat_minor\":5}\n";
static const char s_bare[] =
    "{\"cells\":[{\"cell_type\":\"code\",\"execution_count\":null,\"metadata\":{},\"outputs\":[],\"source\":[\"x = 1\\n\","
    "\"print(x)\"]},{\"cell_type\":\"markdown\",\"metadata\":{},\"source\":[\"# Title\"]},{\"cell_type\":\"code\","
    "\"execution_count\":null,\"metadata\":{},\"outputs\":[],\"source\":[\"y = x + 1\"]}],\"metadata\":{},\"nbformat\":4,"
    "\"nbformat_minor\":5}\n";
static const char s_script[] = "# %%\nx = 1\nprint(x)\n\n# %% [markdown]\n# # Title\n\n# %%\ny = x + 1\n";

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = "nb:a");
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = "nb:b");
}

// the notebook `text` of the file `path` opened under `key`: its model
static CliNotebookModel *opened(const char *key, const char *path, const char *text)
{
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_open.open, CliNotebookCellsOpenCfg, .key = key, .path = path,
                                        .text = text, .size = CLI_UNITY_LENGTH(text), .err_msg = &err_msg));
    CliNotebookModel *const model = EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = key);
    TEST_ASSERT_NOT_NULL(model);
    return model;
}

// the row `at` of the notebook under `key`
static CliNotebookRow row_of(const char *key, unsigned int at)
{
    CliNotebookRow row = {0};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_row.row, CliNotebookCellsRowCfg, .key = key, .at = at,
                                        .row = &row));
    return row;
}

// a kernel's message of `msg_type` answering `parent` with `content` handed to the receiver as the model's kernel
static void received(CliNotebookModel *model, const char *msg_type, const char *parent, const char *content)
{
    char message[1u << 15u];
    VerbaLinea line = {message, sizeof(message), 0ULL};
    mmgr_line_put(&line, "{\"channel\":\"iopub\",\"msg_type\":\"");
    mmgr_line_put(&line, msg_type);
    mmgr_line_put(&line, "\",\"msg_id\":\"m\",\"parent\":\"");
    mmgr_line_put(&line, parent);
    mmgr_line_put(&line, "\",\"content\":");
    mmgr_line_put(&line, content);
    mmgr_line_ch(&line, '}');
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    cli_notebook_model_received(&(CliKernelRecvCfg){.context = model, .message = message, .size = line.at});
}

// whether `size` bytes of `text`, which may carry ends of texts among them, contain the string `part`
static int bytes_contain(const char *text, unsigned long long size, const char *part)
{
    const unsigned long long length = CLI_UNITY_LENGTH(part);
    for (unsigned long long at = 0ULL; (at + length) <= size; at += 1ULL)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at, .other = part, .bytes = length}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// the sentence `which` of the cell `cell` and output `output` of the notebook under `key` is `expected`
static void sentence_is(const char *key, unsigned int which, unsigned int cell, unsigned int output,
                        const char *expected)
{
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_notebook_cells_text.text, CliNotebookCellsTextCfg, .key = key,
                                        .which = which, .cell = cell, .output = output, .length_out = &length);
    CLI_TEST_ASSERT_TEXT(expected, text, length);
}

void test_a_notebook_and_a_percent_script_read_as_the_same_rows(void)
{
    (void)opened("nb:a", "bare.ipynb", s_bare);
    (void)opened("nb:b", "bare.py", s_script);
    const unsigned int rows = EMBED_CALL(cli_notebook_cells_row.rows, CliNotebookCellsRowCfg, .key = "nb:a");
    TEST_ASSERT_EQUAL_UINT(7u, rows);
    TEST_ASSERT_EQUAL_UINT(rows, EMBED_CALL(cli_notebook_cells_row.rows, CliNotebookCellsRowCfg, .key = "nb:b"));
    for (unsigned int at = 0u; at < rows; at += 1u)
    {
        const CliNotebookRow notebook = row_of("nb:a", at);
        const CliNotebookRow script = row_of("nb:b", at);
        TEST_ASSERT_EQUAL_UINT(notebook.type, script.type);
        TEST_ASSERT_EQUAL_UINT(notebook.cell, script.cell);
        TEST_ASSERT_EQUAL_UINT64(notebook.line, script.line);
        TEST_ASSERT_EQUAL_UINT(notebook.flags, script.flags);
    }
}

void test_a_row_is_found_among_the_sorted_starts_of_the_cells(void)
{
    const CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        TEST_ASSERT_TRUE(notebook_first_row_at(model, at) < notebook_first_row_at(model, at + 1u));
    }
    TEST_ASSERT_EQUAL_UINT(8u, EMBED_CALL(cli_notebook_cells_row.rows, CliNotebookCellsRowCfg, .key = "nb:a"));
    const CliNotebookRow first = row_of("nb:a", 0u);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_SOURCE, first.type);
    TEST_ASSERT_TRUE((first.flags & CLI_NOTEBOOK_FLAG_FIRST) != 0u);
    const CliNotebookRow printed = row_of("nb:a", 2u);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_OUTPUT, printed.type);
    TEST_ASSERT_EQUAL_UINT(0u, printed.cell);
    const CliNotebookRow added = row_of("nb:a", 3u);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_ADD, added.type);
    TEST_ASSERT_TRUE((added.flags & CLI_NOTEBOOK_FLAG_LAST) != 0u);
    const CliNotebookRow markdown = row_of("nb:a", 4u);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_MARKDOWN, markdown.type);
    TEST_ASSERT_EQUAL_UINT(1u, markdown.cell);
    const CliNotebookRow last = row_of("nb:a", 6u);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_SOURCE, last.type);
    TEST_ASSERT_EQUAL_UINT(2u, last.cell);
    CliNotebookRow past = {0};
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_cells_row.row, CliNotebookCellsRowCfg, .key = "nb:a", .at = 8u,
                                        .row = &past));
    CliNotebookBlock block = {0};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_row.block, CliNotebookCellsRowCfg, .key = "nb:a", .at = 1u,
                                        .block = &block));
    TEST_ASSERT_EQUAL_UINT(4u, block.first_row);
    TEST_ASSERT_EQUAL_UINT(2u, block.row_count);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_MARKDOWN, block.type);
}

void test_the_step_applies_each_message_to_the_cell_its_run_answers(void)
{
    CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    CliNotebookCell first = notebook_cell_at(model, 0u);
    first.running = 1;
    notebook_cell_set(model, 0u, &first);
    EMBED_CALL(cli_notebook_model_internal.bound, CliNotebookModelCtx, .model = model, .cell = 0u, .id = "r1");
    received(model, "clear_output", "r1", "{\"wait\":false}");
    received(model, "execute_input", "r1", "{\"code\":\"x\",\"execution_count\":5}");
    received(model, "stream", "r1", "{\"name\":\"stdout\",\"text\":\"a\\n\"}");
    received(model, "stream", "r1", "{\"name\":\"stdout\",\"text\":\"b\\n\"}");
    received(model, "execute_result", "r1", "{\"data\":{\"text/plain\":\"3\"},\"execution_count\":5,\"metadata\":{}}");
    received(model, "display_data", "r1", "{\"data\":{\"text/plain\":\"<shown>\"},\"metadata\":{},\"transient\":{}}");
    received(model, "error", "r1", "{\"ename\":\"E\",\"evalue\":\"v\",\"traceback\":[\"first\",\"second\"]}");
    received(model, "execute_reply", "r1", "{\"status\":\"error\",\"execution_count\":5}");
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL));
    const CliNotebookCell cell = notebook_cell_at(model, 0u);
    TEST_ASSERT_EQUAL_UINT(5u, cell.count);
    TEST_ASSERT_EQUAL_INT(0, cell.running);
    TEST_ASSERT_EQUAL_INT(1, cell.failed);
    TEST_ASSERT_EQUAL_UINT(4u, cell.outputs);
    TEST_ASSERT_EQUAL_UINT(2u, notebook_output_at(model, cell.output_first).lines);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_RESULT, notebook_output_at(model, cell.output_first + 1u).type);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_DISPLAY, notebook_output_at(model, cell.output_first + 2u).type);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ERROR, notebook_output_at(model, cell.output_first + 3u).type);
    TEST_ASSERT_EQUAL_UINT(2u, notebook_output_at(model, cell.output_first + 3u).lines);
    TEST_ASSERT_TRUE(notebook_texts_contain(model, "\"output_type\":\"display_data\""));
    TEST_ASSERT_FALSE(notebook_texts_contain(model, "transient"));
    const CliNotebookRow printed = row_of("nb:a", 2u);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_OUTPUT, printed.type);
    TEST_ASSERT_TRUE((printed.flags & CLI_NOTEBOOK_FLAG_FAILED) != 0u);
}

void test_a_message_whose_run_is_not_yet_bound_stands_until_it_is(void)
{
    CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    received(model, "stream", "r9", "{\"name\":\"stderr\",\"text\":\"late\"}");
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL));
    TEST_ASSERT_EQUAL_UINT(0u, notebook_cell_at(model, 2u).outputs);
    TEST_ASSERT_TRUE(model->ring_used != 0ULL);
    EMBED_CALL(cli_notebook_model_internal.bound, CliNotebookModelCtx, .model = model, .cell = 2u, .id = "r9");
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL));
    TEST_ASSERT_EQUAL_UINT(1u, notebook_cell_at(model, 2u).outputs);
    TEST_ASSERT_EQUAL_INT(1, notebook_output_at(model, notebook_cell_at(model, 2u).output_first).stderr_stream);
    TEST_ASSERT_EQUAL_UINT64(0ULL, model->ring_used);
}

void test_each_message_reaches_the_model_its_context_is(void)
{
    CliNotebookModel *const first = opened("nb:a", "one.ipynb", s_notebook);
    CliNotebookModel *const second = opened("nb:b", "two.ipynb", s_notebook);
    EMBED_CALL(cli_notebook_model_internal.bound, CliNotebookModelCtx, .model = first, .cell = 2u, .id = "same");
    EMBED_CALL(cli_notebook_model_internal.bound, CliNotebookModelCtx, .model = second, .cell = 2u, .id = "same");
    received(first, "stream", "same", "{\"name\":\"stdout\",\"text\":\"one\"}");
    received(first, "stream", "same", "{\"name\":\"stdout\",\"text\":\" line\"}");
    received(second, "stream", "same", "{\"name\":\"stdout\",\"text\":\"two\\nlines\"}");
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL));
    TEST_ASSERT_EQUAL_UINT(1u, notebook_cell_at(first, 2u).outputs);
    TEST_ASSERT_EQUAL_UINT(1u, notebook_output_at(first, notebook_cell_at(first, 2u).output_first).lines);
    TEST_ASSERT_EQUAL_INT(0, notebook_output_at(first, notebook_cell_at(first, 2u).output_first).line_ended);
    TEST_ASSERT_EQUAL_UINT(2u, notebook_output_at(second, notebook_cell_at(second, 2u).output_first).lines);
}

void test_the_ring_grows_to_the_bytes_pending_and_is_given_back_cleared(void)
{
    CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    TEST_ASSERT_NULL(model->ring_cell);
    // a text past the ring's least capacity, so the second message grows it
    static char s_long[CLI_NOTEBOOK_RING_LEAST + 4096u];
    VerbaLinea line = {s_long, sizeof(s_long), 0ULL};
    mmgr_line_put(&line, "{\"name\":\"stdout\",\"text\":\"");
    for (unsigned int at = 0u; at < (CLI_NOTEBOOK_RING_LEAST / 10u) + 1u; at += 1u)
    {
        mmgr_line_put(&line, "0123456789");
    }
    mmgr_line_put(&line, "\"}");
    (void)mmgr_line_finish(&line);
    received(model, "status", "", "{\"execution_state\":\"busy\"}");
    const unsigned long long small = model->ring_capacity;
    received(model, "stream", "nobody", s_long);
    TEST_ASSERT_TRUE(model->ring_capacity > small);
    TEST_ASSERT_TRUE(model->ring_used <= model->ring_capacity);
    TEST_ASSERT_TRUE(model->ring_used > CLI_NOTEBOOK_RING_LEAST);
    const unsigned long long pending = model->ring_used;
    (void)EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_KERNEL_BUSY, model->state);
    // the status message applied; the stream message, bound to no run, put back to stand in the kept cell
    TEST_ASSERT_TRUE(model->ring_used < pending);
    TEST_ASSERT_TRUE(model->ring_used != 0ULL);
    TEST_ASSERT_NOT_NULL(model->ring_cell);
    cli_notebook_model_ring_cleared(&(CliNotebookModelCtx){.model = model});
    TEST_ASSERT_NULL(model->ring_cell);
    TEST_ASSERT_EQUAL_UINT64(0ULL, model->ring_capacity);
}

void test_a_cell_is_out_of_date_after_a_cell_it_stands_on_changes_runs_again_or_goes(void)
{
    CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    model->clock = 2u;
    CliNotebookCell first = notebook_cell_at(model, 0u);
    first.first_run = 1u;
    first.last_run = 1u;
    notebook_cell_set(model, 0u, &first);
    CliNotebookCell third = notebook_cell_at(model, 2u);
    third.first_run = 2u;
    third.last_run = 2u;
    notebook_cell_set(model, 2u, &third);
    EMBED_CALL(cli_notebook_model_internal.stale_settled, CliNotebookModelCtx, .model = model);
    TEST_ASSERT_EQUAL_INT(0, notebook_cell_at(model, 2u).stale);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_STALE, 2u, CLI_NOTEBOOK_NONE, "");
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_focus.focus, CliNotebookEditFocusCfg, .key = "nb:a",
                                        .cell = 0u, .line = 0ULL, .column = 0ULL, .err_msg = &err_msg));
    const CliViewInput typed = {.type = CLI_VIEW_TEXT, .text = "z", .length = 1ULL};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_focus.input, CliNotebookEditFocusCfg, .key = "nb:a",
                                        .input = &typed, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(1, notebook_cell_at(model, 2u).stale);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_STALE, 2u, CLI_NOTEBOOK_NONE,
                "Out of date: it stands on cell 1, changed, run again or deleted since this cell ran");
    TEST_ASSERT_EQUAL_INT(1,
                          EMBED_CALL(cli_notebook_edit_cell.remove, CliNotebookEditCellCfg, .key = "nb:a", .cell = 0u));
    TEST_ASSERT_EQUAL_INT(1, notebook_cell_at(model, 1u).stale);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_STALE, 1u, CLI_NOTEBOOK_NONE,
                "Out of date: it stands on a cell since deleted, changed, run again or deleted since this cell ran");
    (void)EMBED_CALL(cli_notebook_kernel_stop.restart, CliNotebookKernelStopCfg, .key = "nb:a", .err_msg = &err_msg);
    TEST_ASSERT_EQUAL_INT(0, notebook_cell_at(model, 1u).stale);
    TEST_ASSERT_EQUAL_UINT(0u, model->gone_count);
    (void)EMBED_CALL(cli_notebook_kernel_stop.stop, CliNotebookKernelStopCfg, .key = "nb:a", .err_msg = &err_msg);
}

void test_the_sentences_the_rows_show(void)
{
    CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_COUNT, 0u, CLI_NOTEBOOK_NONE, "[1]");
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_COUNT, 1u, CLI_NOTEBOOK_NONE, "");
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_COUNT, 2u, CLI_NOTEBOOK_NONE, "[ ]");
    CliNotebookCell third = notebook_cell_at(model, 2u);
    third.running = 1;
    notebook_cell_set(model, 2u, &third);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_COUNT, 2u, CLI_NOTEBOOK_NONE, "[*]");
    third.running = 0;
    notebook_cell_set(model, 2u, &third);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_STATE, CLI_NOTEBOOK_NONE, CLI_NOTEBOOK_NONE, "no kernel");
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_PLACEHOLDER, 1u, CLI_NOTEBOOK_NONE, "Markdown");
    // an output of a hundred lines cut to its first and last forty
    EMBED_CALL(cli_notebook_model_internal.bound, CliNotebookModelCtx, .model = model, .cell = 2u, .id = "long");
    static char s_lines[2048];
    VerbaLinea line = {s_lines, sizeof(s_lines), 0ULL};
    mmgr_line_put(&line, "{\"name\":\"stdout\",\"text\":\"");
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, "\\n");
    }
    mmgr_line_put(&line, "\"}");
    (void)mmgr_line_finish(&line);
    received(model, "stream", "long", s_lines);
    (void)EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL);
    const unsigned int output = notebook_cell_at(model, 2u).output_first;
    TEST_ASSERT_EQUAL_UINT(100u, notebook_output_at(model, output).lines);
    TEST_ASSERT_EQUAL_UINT((2u * CLI_NOTEBOOK_CUT_LINES) + 1u, notebook_output_at(model, output).rows);
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_CUT, 2u, output, "... 20 lines ...");
    sentence_is("nb:a", CLI_NOTEBOOK_TEXT_MORE, 2u, output, "Show all 100 lines");
    const unsigned int rows = EMBED_CALL(cli_notebook_cells_row.rows, CliNotebookCellsRowCfg, .key = "nb:a");
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_menu.command, CliNotebookEditMenuCfg, .key = "nb:a",
                                        .cell = 2u, .output = output, .word = "notebook-show-all"));
    TEST_ASSERT_EQUAL_UINT(rows + 100u - ((2u * CLI_NOTEBOOK_CUT_LINES) + 1u),
                           EMBED_CALL(cli_notebook_cells_row.rows, CliNotebookCellsRowCfg, .key = "nb:a"));
    // the kernel's state as its status messages set it, the display text the notebook records before it
    static char s_kernel_stand_in;
    model->kernel = (CliKernel *)(void *)&s_kernel_stand_in;
    const char *const states[3][2] = {{"starting", "Python 3: starting"}, {"idle", "Python 3: idle"},
                                      {"busy", "Python 3: busy"}};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char content[64];
        VerbaLinea status = {content, sizeof(content), 0ULL};
        mmgr_line_put(&status, "{\"execution_state\":\"");
        mmgr_line_put(&status, states[at][0]);
        mmgr_line_put(&status, "\"}");
        (void)mmgr_line_finish(&status);
        received(model, "status", "", content);
        (void)EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL);
        sentence_is("nb:a", CLI_NOTEBOOK_TEXT_STATE, CLI_NOTEBOOK_NONE, CLI_NOTEBOOK_NONE, states[at][1]);
    }
    model->kernel = NULL;
}

void test_the_save_writes_the_notebook_and_clears_the_changed_mark(void)
{
    char folder[CLI_PATH_MAX_LEN];
    cli_unity_folder("notebook_cells_save", folder);
    char full[CLI_PATH_MAX_LEN];
    VerbaLinea full_line = {full, sizeof(full), 0ULL};
    mmgr_line_put(&full_line, folder);
    mmgr_line_put(&full_line, "/saved.ipynb");
    (void)mmgr_line_finish(&full_line);
    (void)opened("nb:a", full, s_notebook);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_cells_save.dirty, CliNotebookCellsSaveCfg, .key = "nb:a"));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_edit_cell.add, CliNotebookEditCellCfg, .key = "nb:a",
                                        .cell = 2u, .type = CLI_NOTEBOOK_MARKDOWN));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_save.dirty, CliNotebookCellsSaveCfg, .key = "nb:a"));
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_save.save, CliNotebookCellsSaveCfg, .key = "nb:a",
                                        .full = full, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_cells_save.dirty, CliNotebookCellsSaveCfg, .key = "nb:a"));
    unsigned long long size = 0ULL;
    unsigned char *const written = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                              .size_pointer = &size, .owner = g_cli_notebook_open_owner);
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_TRUE(bytes_contain((const char *)written, size, "\"name\": \"python3\""));
    const CliNotebookModel *const saved = opened("nb:b", full, (const char *)written);
    g_cli_guard->persistent_buf_release(written, g_cli_notebook_open_owner);
    TEST_ASSERT_EQUAL_UINT(4u, saved->cells);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_MARKDOWN, notebook_cell_at(saved, 3u).type);
    TEST_ASSERT_EQUAL_UINT(1u, notebook_cell_at(saved, 0u).outputs);
    TEST_ASSERT_EQUAL_UINT(1u, notebook_cell_at(saved, 0u).count);
}

void test_the_frame_lays_the_rows_asked_and_its_release_returns_its_cells(void)
{
    (void)opened("nb:a", "one.ipynb", s_notebook);
    const unsigned long long before = mmgr_owner_in_use(g_cli_notebook_open_owner);
    CliNotebookFrame *frame = NULL;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_frame.alloc, CliNotebookCellsFrameCfg, .key = "nb:a",
                                        .count = 5u, .frame_out = &frame));
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT(5u, frame->count);
    TEST_ASSERT_EQUAL_UINT(8u, frame->rows);
    CLI_TEST_ASSERT_TEXT("x = 1", frame->bytes + frame->row[0].text, frame->row[0].length);
    CLI_TEST_ASSERT_TEXT("print(x)", frame->bytes + frame->row[1].text, frame->row[1].length);
    CLI_TEST_ASSERT_TEXT("hi", frame->bytes + frame->row[2].text, frame->row[2].length);
    CLI_TEST_ASSERT_TEXT("[1]", frame->bytes + frame->row[0].label, frame->row[0].label_length);
    CLI_TEST_ASSERT_TEXT("# Title", frame->bytes + frame->row[4].text, frame->row[4].length);
    TEST_ASSERT_EQUAL_UINT(frame->row[0].part, frame->row[1].part);
    TEST_ASSERT_EQUAL_UINT(1u, frame->row[1].part_line);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_NONE, frame->row[3].part);
    EMBED_CALL(cli_notebook_cells_frame.release, CliNotebookCellsFrameCfg, .frame_out = &frame);
    TEST_ASSERT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_cli_notebook_open_owner));
}

void test_the_top_moves_by_the_wheel_and_to_a_place_and_stops_at_the_last_row(void)
{
    const CliNotebookModel *const model = opened("nb:a", "one.ipynb", s_notebook);
    CliNotebookFrame *frame = NULL;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_frame.alloc, CliNotebookCellsFrameCfg, .key = "nb:a",
                                        .count = 5u, .frame_out = &frame));
    EMBED_CALL(cli_notebook_cells_frame.release, CliNotebookCellsFrameCfg, .frame_out = &frame);
    EMBED_CALL(cli_notebook_cells_scroll.scroll, CliNotebookCellsScrollCfg, .key = "nb:a", .rows = 2);
    TEST_ASSERT_EQUAL_UINT64(2ULL, model->top);
    EMBED_CALL(cli_notebook_cells_scroll.scroll, CliNotebookCellsScrollCfg, .key = "nb:a", .rows = 100);
    TEST_ASSERT_EQUAL_UINT64(3ULL, model->top);
    EMBED_CALL(cli_notebook_cells_scroll.scroll, CliNotebookCellsScrollCfg, .key = "nb:a", .rows = -100);
    TEST_ASSERT_EQUAL_UINT64(0ULL, model->top);
    EMBED_CALL(cli_notebook_cells_scroll.to, CliNotebookCellsScrollCfg, .key = "nb:a", .permille = 500u);
    TEST_ASSERT_EQUAL_UINT64(2ULL, model->top);
    EMBED_CALL(cli_notebook_cells_scroll.to, CliNotebookCellsScrollCfg, .key = "nb:a", .permille = 1000u);
    TEST_ASSERT_EQUAL_UINT64(3ULL, model->top);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_frame.alloc, CliNotebookCellsFrameCfg, .key = "nb:a",
                                        .count = 5u, .frame_out = &frame));
    TEST_ASSERT_EQUAL_UINT64(3ULL, frame->top);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_ROW_ADD, frame->row[0].row.type);
    EMBED_CALL(cli_notebook_cells_frame.release, CliNotebookCellsFrameCfg, .frame_out = &frame);
}
