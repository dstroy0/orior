// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_kernel.h: an open notebook's kernel: the kernels the machine has, read once into one cell, one of them
// picked and started with the notebook receiver, the model its context; a code cell run on it, its outputs cleared
// and its run bound to the id the kernel's messages answer; the kernel interrupted, restarted and stopped; and what the
// notebook's bar shows of it, its display text and the tools that can act now (notebook_kernel.c)
#ifndef NOTEBOOK_KERNEL_H
#define NOTEBOOK_KERNEL_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliNotebookKernelStateCfg CliNotebookKernelStateCfg;
typedef struct CliNotebookKernelPickCfg CliNotebookKernelPickCfg;
typedef struct CliNotebookKernelRunCfg CliNotebookKernelRunCfg;
typedef struct CliNotebookKernelStopCfg CliNotebookKernelStopCfg;

// the bar's tools, a bit each in the mask the tools entry answers: Run All, Restart and Run All, Interrupt, Restart and
// Stop
#define CLI_NOTEBOOK_TOOL_RUN_ALL 1u
#define CLI_NOTEBOOK_TOOL_RESTART_RUN_ALL 2u
#define CLI_NOTEBOOK_TOOL_INTERRUPT 4u
#define CLI_NOTEBOOK_TOOL_RESTART 8u
#define CLI_NOTEBOOK_TOOL_STOP 16u

// A kernel the machine has, a row of the kernels' movable cell read out by copy: its name, the text it is displayed by,
// its language, whether it lacks what it runs on, and the line that installs that where it lacks it, empty where not
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    char display[CLI_NAME_MAX_LEN];
    char language[CLI_NAME_MAX_LEN];
    int lacks;
    char install[CLI_PATH_MAX_LEN];
} CliNotebookKernelRow;

// the text the kernel box of the notebook under `key` shows, the running kernel's display text or `no kernel`, its
// bytes into `length_out`; and the mask of the bar's tools that can act now: Run All and Restart and Run All with a
// code cell, Interrupt, Restart and Stop while a kernel runs
const char *cli_notebook_kernel_label(const CliNotebookKernelStateCfg *args);
unsigned int cli_notebook_kernel_tools(const CliNotebookKernelStateCfg *args);
// how many kernels the machine has, read once; the kernel `at` into `row`: 1, or 0 past the last; the kernels as the
// menus' own rows, a kernel lacking what it needs a row that posts its install line: 1 where built; and the kernel
// `name`, else the kernel `at`, started for the notebook under `key` in place of its kernel, recorded in its
// metadata: 1, or 0 with why written and posted, a kernel lacking what it needs posting its install line
unsigned int cli_notebook_kernels(const CliNotebookKernelPickCfg *args);
int cli_notebook_kernel_at(const CliNotebookKernelPickCfg *args);
int cli_notebook_kernels_menu(const CliNotebookKernelPickCfg *args);
int cli_notebook_kernel_choose(const CliNotebookKernelPickCfg *args);
// the cell `cell` of the notebook under `key` run: a markdown cell left, a code cell's outputs cleared and its text
// sent to the notebook's kernel, the kernel the notebook records started first where none runs, else the first of
// its language: 1, or 0 with why written and posted; every code cell run in order; and the kernel restarted, then
// every code cell run
int cli_notebook_kernel_execute(const CliNotebookKernelRunCfg *args);
int cli_notebook_kernel_execute_all(const CliNotebookKernelRunCfg *args);
int cli_notebook_kernel_restart_execute_all(const CliNotebookKernelRunCfg *args);
// the notebook's kernel interrupted: 1, or 0 with why written; the kernel stopped and started again, every cell's runs,
// edits and the cells deleted after they ran forgotten: 1, or 0 with why written; and the kernel stopped, the
// sentence posted where its process had to be ended: 1, or 0 where none ran. Each leaves no cell running
int cli_notebook_kernel_interrupt(const CliNotebookKernelStopCfg *args);
int cli_notebook_kernel_restart(const CliNotebookKernelStopCfg *args);
int cli_notebook_kernel_end(const CliNotebookKernelStopCfg *args);

struct CliNotebookKernelStateCfg
{
    const char *key;
    unsigned long long *length_out;
};

struct CliNotebookKernelPickCfg
{
    const char *key;
    unsigned int at;
    const char *name;
    CliNotebookKernelRow *row;
    VerbaLinea *err_msg;
};

struct CliNotebookKernelRunCfg
{
    const char *key;
    unsigned int cell;
    VerbaLinea *err_msg;
};

struct CliNotebookKernelStopCfg
{
    const char *key;
    VerbaLinea *err_msg;
};

typedef struct
{
    const char *(*label)(const CliNotebookKernelStateCfg *args);
    unsigned int (*tools)(const CliNotebookKernelStateCfg *args);
} CliNotebookKernelStateNs;
EMBED_TABLE_LAYOUT(CliNotebookKernelStateNs, label, tools);
EMBED_TABLE_STORAGE CliNotebookKernelStateNs cli_notebook_kernel_state EMBED_UNUSED = {
    .label = cli_notebook_kernel_label,
    .tools = cli_notebook_kernel_tools,
};

typedef struct
{
    unsigned int (*kernels)(const CliNotebookKernelPickCfg *args);
    int (*kernel)(const CliNotebookKernelPickCfg *args);
    int (*menu)(const CliNotebookKernelPickCfg *args);
    int (*pick)(const CliNotebookKernelPickCfg *args);
} CliNotebookKernelPickNs;
EMBED_TABLE_LAYOUT(CliNotebookKernelPickNs, kernels, kernel, menu, pick);
EMBED_TABLE_STORAGE CliNotebookKernelPickNs cli_notebook_kernel_pick EMBED_UNUSED = {
    .kernels = cli_notebook_kernels,
    .kernel = cli_notebook_kernel_at,
    .menu = cli_notebook_kernels_menu,
    .pick = cli_notebook_kernel_choose,
};

typedef struct
{
    int (*run)(const CliNotebookKernelRunCfg *args);
    int (*run_all)(const CliNotebookKernelRunCfg *args);
    int (*restart_run_all)(const CliNotebookKernelRunCfg *args);
} CliNotebookKernelRunNs;
EMBED_TABLE_LAYOUT(CliNotebookKernelRunNs, run, run_all, restart_run_all);
EMBED_TABLE_STORAGE CliNotebookKernelRunNs cli_notebook_kernel_run EMBED_UNUSED = {
    .run = cli_notebook_kernel_execute,
    .run_all = cli_notebook_kernel_execute_all,
    .restart_run_all = cli_notebook_kernel_restart_execute_all,
};

typedef struct
{
    int (*interrupt)(const CliNotebookKernelStopCfg *args);
    int (*restart)(const CliNotebookKernelStopCfg *args);
    int (*stop)(const CliNotebookKernelStopCfg *args);
} CliNotebookKernelStopNs;
EMBED_TABLE_LAYOUT(CliNotebookKernelStopNs, interrupt, restart, stop);
EMBED_TABLE_STORAGE CliNotebookKernelStopNs cli_notebook_kernel_stop EMBED_UNUSED = {
    .interrupt = cli_notebook_kernel_interrupt,
    .restart = cli_notebook_kernel_restart,
    .stop = cli_notebook_kernel_end,
};

#endif
