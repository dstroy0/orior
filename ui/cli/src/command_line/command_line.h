// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line.h: the command line: the window's menus as words. `orior_cli <menu> <command>` is the command of that
// menu, `orior_cli <menu> <job>` the job of a menu of jobs, and the help is the menus listed.
//
// A command the terminal can do, starting a job, listing, showing, the bridge, the keys and the version, runs here.
// Every other acts in the window, and the words come to the window to open with the command to run in it. Every job
// the window lists is a job here, read from the tree the same way, and a run goes through the same runner with the
// same checks. The steps share the terminal, its output streams as it comes, and the program exits with the job's
// code (command_line*.c)
#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H
#include "../machine/state.h"
#include "cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliCommandLineInputCfg CliCommandLineInputCfg;
typedef struct CliCommandLineNamedCfg CliCommandLineNamedCfg;
typedef struct CliCommandLineCfg CliCommandLineCfg;
typedef struct CliCommandLineLaunchOutCfg CliCommandLineLaunchOutCfg;

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
    char root[CLI_PATH_MAX_LEN];
    int has_root;
    char menu[CLI_NAME_MAX_LEN];
    char command[CLI_NAME_MAX_LEN];
    unsigned int args;
    unsigned int arg_at[CLI_LAUNCH_ARGS];
    char text[CLI_LAUNCH_TEXT];
    unsigned int used;
} CliOutcome;

// the words given on the command line, `count` of them, run: a command run here, with the code to exit with, or the
// window to open for one that acts there, into `outcome`
void cli_command_line_run(const CliCommandLineInputCfg *args);
// the words that name a launch's command, as the command line takes them, written into `out`
void cli_launch_words(const CliCommandLineLaunchOutCfg *args);
// the word `at` of a launch's command
const char *cli_launch_arg(const CliCommandLineCfg *args);
// the tree a command works on: the folder `named` names, else the one the tree's marker is found above, made the
// tree whose tools come first, into `root`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_command_line_tree(const CliCommandLineNamedCfg *args);
// the console let go where this process is the only one on it: the console Windows opens for a console program started
// from outside a terminal; started from a terminal, the process retains the terminal's console; elsewhere nothing
// (platform/windows.c, platform/posix.c)
void cli_console_let_go(void);

// the window's command line and its tree (command_line_window.c): the command line run on the words the program was
// started with, as the system gives them: the code to exit with, 1 in `window` where the words open the window
// instead; the tree the window opens on found and readied: orior_cli run on the machine where it is in a Flatpak's
// sandbox, every process it starts contained under it, the door opened for the command line, `woken` called from the
// door's thread as a command comes, and the tree watched, each batch of files changed outside the window noted among
// its runs, walked into the tree's branches and handed to the servers on the watch's own thread; every server,
// session and process of the window's ended as it ends; the tree open, NULL for none; and the words the program was
// started with
int cli_command_line_window(int *window);
void cli_command_line_tree_begin(void (*woken)(void));
void cli_command_line_tree_end(void);
const char *cli_command_line_root(void);
const CliOutcome *cli_command_line_launched(void);

// the parts of the status bar a press can land on: the notice, its Silence and Focus
#define CLI_NOTICES_PRESS_NOTICE 1u
#define CLI_NOTICES_PRESS_SILENCE 2u
#define CLI_NOTICES_PRESS_FOCUS 3u

// the notices (notices.c), as the page's status bar shows them: the notice `text` posted, a failure's where `failed` is
// 1: one that answers a press or a key that gives a command within CLI_NOTICES_ANSWERS_MS shows at once, any other
// waits for a break in the work, CLI_NOTICES_REST_MS of no input or a save, and Focus stops every one until it ends; a
// notice whose kind is put away shows not at all; one shown stands CLI_NOTICES_SHOWN_MS on the status bar, a failure's
// CLI_NOTICES_FAILED_MS, and a press on it takes it away, one on Silence beside it putting its kind away for good,
// saved in orior's folder. Any input, a press of the pointer, or the key `name` while the editor takes the keys where
// `editing` is 1, so the notices know when the reader commanded and when the work rests; a break taken; a press on the
// status bar's
// part `part`; Focus on or off; the kind `kind` shown again; the timers whose time came taken, 1 where anything
// changed; the clock's reading of the next timer, 0 for none; the kinds put away read in place in `entries` as the
// window begins, which stand until the window ends; and every notice let go as the window ends, before its entries go
#define CLI_NOTICES_ANSWERS_MS 1500ULL
#define CLI_NOTICES_REST_MS 2000ULL
#define CLI_NOTICES_SHOWN_MS 5000ULL
#define CLI_NOTICES_FAILED_MS 15000ULL
void cli_notice_post(const char *text, int failed);
void cli_notices_input(void);
void cli_notices_pointer(void);
void cli_notices_key(const char *name, int editing);
void cli_notices_break(void);
void cli_notices_press(unsigned int part);
void cli_notices_focus(int on);
void cli_notices_show_again(const char *kind);
int cli_notices_step(void);
unsigned long long cli_notices_due_ms(void);
void cli_notices_restore(const CliStateEntries *entries);
void cli_notices_end(void);
// the notice the status bar shows, its first line, NULL for none, and whether it is a failure's; whether Focus is on,
// and how many notices wait on it
const char *cli_notice_shown(void);
int cli_notice_failed(void);
int cli_notices_focused(void);
unsigned int cli_notices_waiting(void);

struct CliCommandLineInputCfg
{
    const char *const *supplied;
    unsigned int count;
    CliOutcome *outcome;
};

struct CliCommandLineNamedCfg
{
    const char *named;
    char *root;
    VerbaLinea *err_msg;
};

struct CliCommandLineCfg
{
    const CliOutcome *launch;
    unsigned int at;
};

struct CliCommandLineLaunchOutCfg
{
    const CliOutcome *launch;
    VerbaLinea *out;
};

typedef struct
{
    void (*run)(const CliCommandLineInputCfg *args);
} CliCommandLineInputNs;
EMBED_TABLE_LAYOUT(CliCommandLineInputNs, run);
EMBED_TABLE_STORAGE CliCommandLineInputNs cli_command_line_input EMBED_UNUSED = {
    .run = cli_command_line_run,
};

typedef struct
{
    void (*launch_words)(const CliCommandLineLaunchOutCfg *args);
} CliCommandLineLaunchOutNs;
EMBED_TABLE_LAYOUT(CliCommandLineLaunchOutNs, launch_words);
EMBED_TABLE_STORAGE CliCommandLineLaunchOutNs cli_command_line_launch_out EMBED_UNUSED = {
    .launch_words = cli_launch_words,
};

typedef struct
{
    const char *(*launch_arg)(const CliCommandLineCfg *args);
} CliCommandLineNs;
EMBED_TABLE_LAYOUT(CliCommandLineNs, launch_arg);
EMBED_TABLE_STORAGE CliCommandLineNs cli_command_line_table EMBED_UNUSED = {
    .launch_arg = cli_launch_arg,
};

typedef struct
{
    int (*tree)(const CliCommandLineNamedCfg *args);
} CliCommandLineNamedNs;
EMBED_TABLE_LAYOUT(CliCommandLineNamedNs, tree);
EMBED_TABLE_STORAGE CliCommandLineNamedNs cli_command_line_named EMBED_UNUSED = {
    .tree = cli_command_line_tree,
};

#endif
