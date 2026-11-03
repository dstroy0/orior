// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal.h: the commands of the window's menus that run at the terminal where no window is open on the tree:
// git's pushes, pulls, branches, stashes and cherry-picks, the tree's tests run, and the settings carried to and from
// another machine. Each does what its menu does in the window, with the words the command line gives in place of what
// the window would confirm, and reports what it did. A command the window alone can do is not among them, and a window
// opens to run it (at_terminal.c, at_terminal_tests.c)
#ifndef AT_TERMINAL_H
#define AT_TERMINAL_H
#include "cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliAtTerminalCommandCfg CliAtTerminalCommandCfg;
typedef struct CliAtTerminalCountCfg CliAtTerminalCountCfg;
typedef struct CliAtTerminalRootCfg CliAtTerminalRootCfg;
typedef struct CliAtTerminalCfg CliAtTerminalCfg;

// whether `command` runs at the terminal where no window is open; and `command` with `args`, `count` of them, run on
// the tree at `root` as its menu runs it in the window, what it prints written into `out`: 1, or 0 with why given
int cli_at_terminal_runs(const CliAtTerminalCommandCfg *args);
int cli_at_terminal_run(const CliAtTerminalCommandCfg *args);

// the tests or files of tests `supplied`, `count` of them, run on the tree at `root` across the machine's cores, with
// coverage where `cover`, and waited for: each test that failed with the first line it printed, each file's coverage,
// then the counts, written into `out` where every test ran and none failed and into `err_msg` otherwise; 1, or 0
// (at_terminal_tests.c)
int cli_at_terminal_tests(const CliAtTerminalRootCfg *args);
// what the file or folder `from` carries that differs from this machine's taken in: each file and each setting of
// settings.json named among `chosen`, `count` of them, or every one where none is named: 1 with what it took given, or
// 0 with why given
int cli_at_terminal_alloc(const CliAtTerminalCountCfg *args);
// the taking `id` undone, what it did given: 1, or 0 with why given
int cli_at_terminal_undo(const CliAtTerminalCfg *args);

struct CliAtTerminalCommandCfg
{
    const char *command;
    const char *root;
    const char *const *request;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
};

struct CliAtTerminalCountCfg
{
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *home;
    const char *from;
    const char *const *chosen;
};

struct CliAtTerminalRootCfg
{
    const char *root;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *const *supplied;
    int cover;
};

struct CliAtTerminalCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *home;
    const char *id;
};

typedef struct
{
    int (*runs)(const CliAtTerminalCommandCfg *args);
    int (*run)(const CliAtTerminalCommandCfg *args);
} CliAtTerminalCommandNs;
EMBED_TABLE_LAYOUT(CliAtTerminalCommandNs, runs, run);
EMBED_TABLE_STORAGE CliAtTerminalCommandNs cli_at_terminal_command EMBED_UNUSED = {
    .runs = cli_at_terminal_runs,
    .run = cli_at_terminal_run,
};

typedef struct
{
    int (*tests)(const CliAtTerminalRootCfg *args);
} CliAtTerminalRootNs;
EMBED_TABLE_LAYOUT(CliAtTerminalRootNs, tests);
EMBED_TABLE_STORAGE CliAtTerminalRootNs cli_at_terminal_root EMBED_UNUSED = {
    .tests = cli_at_terminal_tests,
};

typedef struct
{
    int (*take)(const CliAtTerminalCountCfg *args);
} CliAtTerminalCountNs;
EMBED_TABLE_LAYOUT(CliAtTerminalCountNs, take);
EMBED_TABLE_STORAGE CliAtTerminalCountNs cli_at_terminal_count EMBED_UNUSED = {
    .take = cli_at_terminal_alloc,
};

typedef struct
{
    int (*undo)(const CliAtTerminalCfg *args);
} CliAtTerminalNs;
EMBED_TABLE_LAYOUT(CliAtTerminalNs, undo);
EMBED_TABLE_STORAGE CliAtTerminalNs cli_at_terminal EMBED_UNUSED = {
    .undo = cli_at_terminal_undo,
};

#endif
