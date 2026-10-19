// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line.h: the command line: the window's menus as words. `orior <menu> <command>` is the command of that menu,
// `orior <menu> <job>` the job of a menu of jobs, and the help is the menus listed.
//
// A command the terminal can do, starting a job, listing, showing, the bridge, the keys and the version, runs here.
// Every other acts in the window, and the words come to the window to open with the command to run in it. Every job
// the window lists is a job here, read from the tree the same way, and a run goes through the same runner with the
// same checks. The steps share the terminal, its output streams as it comes, and the program exits with the job's
// code (command_line*.c)
#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include "cli_internal.h"

// the most words a command the window runs is given, and the bytes they take
#define CLI_LAUNCH_ARGS 256u
#define CLI_LAUNCH_TEXT (1u << 16u)

// What the command line's words come to: the code to exit with, or, where `window`, the window to open: the tree to
// open it on, where one was named, the tree on another machine to open it on, as the link reads its address, and the
// command of its menus to run once it is up with its words; with no command the window opens and runs none
typedef struct
{
    int window;
    int code;
    char root[CLI_PATH_LONGEST];
    int has_root;
    char remote[CLI_PATH_LONGEST];
    int has_remote;
    char menu[CLI_NAME_LONGEST];
    char command[CLI_NAME_LONGEST];
    unsigned int args;
    unsigned int arg_at[CLI_LAUNCH_ARGS];
    char text[CLI_LAUNCH_TEXT];
    unsigned int used;
} CliOutcome;

// the words given on the command line, `count` of them, run: a command run here, with the code to exit with, or the
// window to open for one that acts there, into `outcome`
void cli_command_line_run(const char *const *given, unsigned int count, CliOutcome *outcome);
// the words that name a launch's command, as the command line takes them, written into `out`
void cli_launch_words(const CliOutcome *launch, CliLine *out);
// the word `at` of a launch's command
const char *cli_launch_arg(const CliOutcome *launch, unsigned int at);
// the tree a command works on: the folder `named` names, else the one the tree's marker is found above, made the
// tree whose tools come first, into `root`, which holds CLI_PATH_LONGEST: 1, or 0 with why said
int cli_command_line_tree(const char *named, char *root, CliLine *said);
// the console let go where this process is the only one on it: the console Windows opens for a console program started
// from outside a terminal; started from a terminal, the process keeps the terminal's console; elsewhere nothing
// (platform/windows.c, platform/posix.c)
void cli_console_let_go(void);

#endif
