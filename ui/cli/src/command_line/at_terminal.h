// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal.h: the commands of the window's menus that run at the terminal where no window is open on the tree:
// git's pushes, pulls, branches, stashes and cherry-picks, the tree's tests run, and the settings carried to and from
// another machine. Each does what its menu does in the window, with the words the command line gives in place of what
// the window would ask, and says what it did. A command the window alone can do is not among them, and a window opens
// to run it (at_terminal.c, at_terminal_tests.c)
#ifndef AT_TERMINAL_H
#define AT_TERMINAL_H

#include "cli_internal.h"

// whether `command` runs at the terminal where no window is open; and `command` with `args`, `count` of them, run on
// the tree at `root` as its menu runs it in the window, what it says written into `out`: 1, or 0 with why said
int cli_at_terminal_runs(const char *command);
int cli_at_terminal_run(const char *root, const char *command, const char *const *args, unsigned int count,
                        CliLine *out, CliLine *said);

// the tests or files of tests `given`, `count` of them, run on the tree at `root` across the machine's cores, with
// coverage where `cover`, and waited for: each test that failed with the first line it said, each file's coverage,
// then the counts, written into `out` where every test ran and none failed and into `said` otherwise; 1, or 0
// (at_terminal_tests.c)
int cli_at_terminal_tests(const char *root, const char *const *given, unsigned int count, int cover, CliLine *out,
                          CliLine *said);
// what the file or folder `from` carries that differs from this machine's taken in: each file and each setting of
// settings.json named among `chosen`, `count` of them, or every one where none is named: 1 with what it took said, or
// 0 with why said
int cli_at_terminal_take(const char *home, const char *from, const char *const *chosen, unsigned int count,
                         CliLine *out, CliLine *said);
// the taking `id` undone, what it did said: 1, or 0 with why said
int cli_at_terminal_undo(const char *home, const char *id, CliLine *out, CliLine *said);

#endif
