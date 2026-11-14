// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_notebook_kernel.c: an open notebook's kernel: the bar's tools and the kernel box's text with no kernel, the
// kernels the machine has a row each, a code cell run on a kernel started for it with its outputs and run count read
// back from the step, every code cell run in order, and the kernel restarted and stopped with no cell left running.
// The cases on a live kernel are ignored on a machine with no Python kernel
#include "ui/cli/src/notebooks/notebook_cells_internal.h"
#include "ui/cli/src/notebooks/notebook_kernel.h"
#include "ui/query/src/query_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include "utils/test/src/ui/cli/unit/support/notebook_model_unity.h"

// how long a run is waited for, and the pause between two steps, in milliseconds
#define KERNEL_PATIENCE 60000ULL
#define KERNEL_PAUSE 20u

// a notebook of a code cell that prints, a markdown cell and a code cell whose value is shown, recording Python 3;
// and a notebook of markdown alone
static const char s_notebook[] =
    "{\"cells\":[{\"cell_type\":\"code\",\"execution_count\":null,\"metadata\":{},\"outputs\":[],\"source\":["
    "\"print('hi')\"]},{\"cell_type\":\"markdown\",\"metadata\":{},\"source\":[\"# Title\"]},{\"cell_type\":\"code\","
    "\"execution_count\":null,\"metadata\":{},\"outputs\":[],\"source\":[\"1 + 1\"]}],\"metadata\":{\"kernelspec\":{"
    "\"display_name\":\"Python 3\",\"language\":\"python\",\"name\":\"python3\"}},\"nbformat\":4,\"nbformat_minor\":5}\n";
static const char s_prose[] = "{\"cells\":[{\"cell_type\":\"markdown\",\"metadata\":{},\"source\":[\"text\"]}],"
                              "\"metadata\":{},\"nbformat\":4,\"nbformat_minor\":5}\n";

static char s_why[512];
static VerbaLinea s_err_msg;
static char s_path[CLI_PATH_MAX_LEN];

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ULL};
}

void tearDown(void)
{
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = "kernel:a");
}

// the notebook `text` opened under kernel:a as a file of a folder of the suite's own: its model
static CliNotebookModel *opened(const char *text)
{
    char folder[CLI_PATH_MAX_LEN];
    cli_unity_folder("notebook_kernel", folder);
    VerbaLinea path = {s_path, sizeof(s_path), 0ULL};
    mmgr_line_put(&path, folder);
    mmgr_line_put(&path, "/run.ipynb");
    (void)mmgr_line_finish(&path);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_cells_open.open, CliNotebookCellsOpenCfg, .key = "kernel:a",
                                        .path = s_path, .text = text, .size = CLI_UNITY_LENGTH(text),
                                        .err_msg = &s_err_msg));
    return EMBED_CALL(cli_notebook_model_internal.of, CliNotebookModelCtx, .key = "kernel:a");
}

// whether the machine has a Python kernel lacking nothing
static int python_found(void)
{
    const unsigned int kernels = EMBED_CALL(cli_notebook_kernel_pick.kernels, CliNotebookKernelPickCfg, .key = "");
    for (unsigned int at = 0u; at < kernels; at += 1u)
    {
        CliNotebookKernelRow row;
        if (EMBED_CALL(cli_notebook_kernel_pick.kernel, CliNotebookKernelPickCfg, .at = at, .row = &row) &&
            !row.lacks &&
            cellul.eq(&(CatenaFinitaCfg){.src = row.language, .other = "python", .cap = CLI_NAME_MAX_LEN}))
        {
            return 1;
        }
    }
    return 0;
}

// the step run until no cell of the model runs, up to KERNEL_PATIENCE: 1 where none runs
static int runs_waited(const CliNotebookModel *model)
{
    const unsigned long long until = cli_clock_milliseconds() + KERNEL_PATIENCE;
    for (;;)
    {
        (void)EMBED_CALL(cli_notebook_cells_step.step, CliNotebookCellsStepCfg, .woken = NULL);
        int running = 0;
        for (unsigned int at = 0u; at < model->cells; at += 1u)
        {
            running = running || notebook_cell_at(model, at).running;
        }
        if (!running)
        {
            return 1;
        }
        if (cli_clock_milliseconds() >= until)
        {
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = KERNEL_PAUSE);
    }
}

// the text of the first line of the output `output` of the notebook under kernel:a, read through its frame
static int output_shows(const CliNotebookModel *model, unsigned int cell, const char *expected)
{
    CliNotebookFrame *frame = NULL;
    (void)EMBED_CALL(cli_notebook_cells_frame.alloc, CliNotebookCellsFrameCfg, .key = "kernel:a", .count = 64u,
                     .frame_out = &frame);
    int found = 0;
    for (unsigned int at = 0u; (frame != NULL) && (at < frame->count); at += 1u)
    {
        const CliNotebookFrameRow *const row = &frame->row[at];
        found = found || ((row->row.type == CLI_NOTEBOOK_ROW_OUTPUT) && (row->row.cell == cell) &&
                          (row->length == CLI_UNITY_LENGTH(expected)) &&
                          (memor.cmp(&(MemoriaCfg){.src = frame->bytes + row->text, .other = expected,
                                                   .bytes = row->length}) == 0));
    }
    EMBED_CALL(cli_notebook_cells_frame.release, CliNotebookCellsFrameCfg, .frame_out = &frame);
    (void)model;
    return found;
}

void test_the_tools_and_the_kernel_box_with_no_kernel(void)
{
    (void)opened(s_notebook);
    TEST_ASSERT_EQUAL_UINT(CLI_NOTEBOOK_TOOL_RUN_ALL | CLI_NOTEBOOK_TOOL_RESTART_RUN_ALL,
                           EMBED_CALL(cli_notebook_kernel_state.tools, CliNotebookKernelStateCfg, .key = "kernel:a"));
    unsigned long long length = 0ULL;
    const char *const label = EMBED_CALL(cli_notebook_kernel_state.label, CliNotebookKernelStateCfg,
                                         .key = "kernel:a", .length_out = &length);
    CLI_TEST_ASSERT_TEXT("no kernel", label, length);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_kernel_stop.stop, CliNotebookKernelStopCfg, .key = "kernel:a",
                                        .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_kernel_stop.interrupt, CliNotebookKernelStopCfg,
                                        .key = "kernel:a", .err_msg = &s_err_msg));
    EMBED_CALL(cli_notebook_cells_open.close, CliNotebookCellsOpenCfg, .key = "kernel:a");
    (void)opened(s_prose);
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_notebook_kernel_state.tools, CliNotebookKernelStateCfg,
                                          .key = "kernel:a"));
}

void test_the_kernels_the_machine_has_are_a_row_each(void)
{
    const unsigned int kernels = EMBED_CALL(cli_notebook_kernel_pick.kernels, CliNotebookKernelPickCfg, .key = "");
    if (kernels == 0u)
    {
        TEST_IGNORE_MESSAGE("no kernel on this machine");
    }
    for (unsigned int at = 0u; at < kernels; at += 1u)
    {
        CliNotebookKernelRow row;
        TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_pick.kernel, CliNotebookKernelPickCfg, .at = at,
                                            .row = &row));
        TEST_ASSERT_TRUE(row.name[0] != '\0');
        TEST_ASSERT_TRUE(!row.lacks || (row.install[0] != '\0'));
    }
    CliNotebookKernelRow past;
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_notebook_kernel_pick.kernel, CliNotebookKernelPickCfg, .at = kernels,
                                        .row = &past));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_pick.menu, CliNotebookKernelPickCfg, .key = ""));
}

void test_a_code_cell_runs_on_a_kernel_started_for_it(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    CliNotebookModel *const model = opened(s_notebook);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_run.run, CliNotebookKernelRunCfg, .key = "kernel:a",
                                        .cell = 0u, .err_msg = &s_err_msg));
    TEST_ASSERT_NOT_NULL(model->kernel);
    TEST_ASSERT_EQUAL_INT(1, notebook_cell_at(model, 0u).running);
    TEST_ASSERT_TRUE(notebook_cell_at(model, 0u).last_run != 0u);
    TEST_ASSERT_TRUE((EMBED_CALL(cli_notebook_kernel_state.tools, CliNotebookKernelStateCfg, .key = "kernel:a") &
                      CLI_NOTEBOOK_TOOL_STOP) != 0u);
    TEST_ASSERT_EQUAL_INT(1, runs_waited(model));
    TEST_ASSERT_TRUE(notebook_cell_at(model, 0u).count != CLI_NOTEBOOK_NONE);
    TEST_ASSERT_EQUAL_INT(1, output_shows(model, 0u, "hi"));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_run.run_all, CliNotebookKernelRunCfg, .key = "kernel:a",
                                        .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT(1, runs_waited(model));
    TEST_ASSERT_TRUE(notebook_cell_at(model, 2u).count > notebook_cell_at(model, 0u).count);
    TEST_ASSERT_EQUAL_INT(1, output_shows(model, 2u, "2"));
    TEST_ASSERT_EQUAL_UINT(1u, notebook_cell_at(model, 0u).outputs);
}

void test_restart_and_stop_leave_no_cell_running(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    CliNotebookModel *const model = opened(s_notebook);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_run.run, CliNotebookKernelRunCfg, .key = "kernel:a",
                                        .cell = 0u, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_stop.interrupt, CliNotebookKernelStopCfg,
                                        .key = "kernel:a", .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_stop.restart, CliNotebookKernelStopCfg, .key = "kernel:a",
                                        .err_msg = &s_err_msg));
    TEST_ASSERT_NOT_NULL(model->kernel);
    TEST_ASSERT_EQUAL_INT(0, notebook_cell_at(model, 0u).running);
    TEST_ASSERT_EQUAL_UINT(0u, notebook_cell_at(model, 0u).last_run);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_run.restart_run_all, CliNotebookKernelRunCfg,
                                        .key = "kernel:a", .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_notebook_kernel_stop.stop, CliNotebookKernelStopCfg, .key = "kernel:a",
                                        .err_msg = &s_err_msg));
    TEST_ASSERT_NULL(model->kernel);
    for (unsigned int at = 0u; at < model->cells; at += 1u)
    {
        TEST_ASSERT_EQUAL_INT(0, notebook_cell_at(model, at).running);
    }
    unsigned long long length = 0ULL;
    const char *const label = EMBED_CALL(cli_notebook_kernel_state.label, CliNotebookKernelStateCfg,
                                         .key = "kernel:a", .length_out = &length);
    CLI_TEST_ASSERT_TEXT("no kernel", label, length);
}
