// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_internal.h: what the command line's sources share: its output and its errors, a line each; the
// terminal's width; a text cut to a width and broken into lines; a word quoted as the runner's shell words read it; the
// tree's jobs read, a job found by a word, and the values its words give; and the codes it exits with
#ifndef COMMAND_LINE_INTERNAL_H
#define COMMAND_LINE_INTERNAL_H

#include "catalog.h"
#include "command_line.h"

// the code a run exits with where a step could not start or gave no code, and the code for words the program does not
// take
#define CLI_NO_CODE 1
#define CLI_WRONG 2

// the most jobs the catalog the command line reads holds
#define CLI_LINE_JOBS 4096u

// `size` bytes of `text` written as a line of the output, and of the errors; and the same of a text ended by a 0
// (command_line.c)
void cli_out_line(const char *text, unsigned long long size);
void cli_err_line(const char *text, unsigned long long size);
void cli_out(const char *text);
void cli_err(const char *text);
// the terminal's width, COLUMNS where it is 40 or more, else 100
unsigned int cli_width(void);
// `size` bytes of `text` cut to `most` letters, ending in `...` where it was cut, written into `out`; and broken into
// lines of at most `most` letters at whitespace, each led by `lead`, each handed to `each`
void cli_cut(const char *text, unsigned long long size, unsigned long long most, CliLine *out);
void cli_wrap(const char *text, unsigned long long size, const char *lead, unsigned long long most,
              void (*each)(void *context, const char *line, unsigned long long size), void *context);
// a word written quoted so the runner's shell words give it back whole; and whether `text` holds `word`, case aside
// as ASCII has it
void cli_quoted_word(const char *word, CliLine *out);
int cli_holds_word(const char *text, const char *word);
// the tree's jobs read, held until the next read
CliCatalog *cli_command_line_catalog(const char *root);
// the job a word names among the jobs of `catalog` at `among`, `among_count` of them, or among every one where `among`
// is NULL: the one whose id it is, else the only one whose id ends in it after a slash or whose title it is: its place,
// or CLI_CATALOG_NONE with why said
unsigned int cli_job_named(const CliCatalog *catalog, const unsigned int *among, unsigned int among_count,
                           const char *name, CliLine *said);
// the values a run's `count` words give the job at `job`: `key=value` for each, a key given twice holding both, and
// the words after `--` as the job's free words, quoted; every param not given its default. 1, or 0 with why said
int cli_job_values(const CliCatalog *catalog, unsigned int job, const char *const *words, unsigned int count,
                   CliValues *values, CliLine *said);
// what a stage job works on: the second part of its file's path, into `subject`, which holds CLI_NAME_LONGEST
void cli_job_subject(const CliCatalog *catalog, unsigned int job, char *subject);

// the tree's tests listed, each with its line, or as JSON with `--json`; the tree's jobs listed, those that hold `word`
// where it is given; a job shown; and a job run here, told to the window where a terminal of the window's started
// this, each giving the code to exit with (command_line_jobs.c)
int cli_tests_list(const char *root, const char *const *words, unsigned int count);
int cli_jobs_list(const char *root, const char *word);
void cli_job_show(const char *root, const CliCatalog *catalog, unsigned int job);
int cli_job_run(const char *root, const CliCatalog *catalog, unsigned int job, const char *const *words,
                unsigned int count);
// a run of the command line's made one the window is told of as it goes, on a channel of its own under runs, where a
// terminal of the window's started this process, else none; and `size` bytes of `body`, JSON, told to it
void cli_line_told_begun(void);
void cli_line_told(const char *body, unsigned long long size);

// the jobs of the menu's groups, their places in the catalog into `listed`, which holds CLI_LINE_JOBS: their count; a
// menu's commands and the jobs of a menu of jobs listed, as `orior_cli <menu>` alone shows them, its jobs from the tree
// at `root`, NULL where there is none; and a job of a menu of jobs shown, as the window shows it on choosing, each
// giving the code to exit with (command_line_menus.c)
unsigned int cli_menu_jobs(const CliCatalog *catalog, unsigned int menu, unsigned int *listed);
int cli_menu_lines(const char *root, unsigned int menu);
int cli_menu_job(const char *named, unsigned int menu, const char *const *words, unsigned int count);

// the keys of the bridge listed, those that hold `word` where it is given, or the one it names shown whole: the code to
// exit with (command_line_bridge.c)
int cli_bridge_words(const char *root, const char *word);

// the help, made from the menus, written into `out`; the keys of every command that has some, by menu; the words that
// can follow the menu `menu`; and the completion script for `shell` written into `out`: 1, or 0 with why said
// (command_line_help.c)
void cli_help(CliLine *out);
// the width the help's lines are wrapped to; and `text` broken at spaces into lines of at most `width` letters, each
// after the first indented `indent` more, each handed to `each`, a word wider than the width alone on its line
#define CLI_HELP_WIDTH 100ull
void cli_help_wrapped(const char *text, unsigned long long width, unsigned long long indent,
                      void (*each)(void *context, const char *line, unsigned long long size), void *context);
void cli_keys(void);
void cli_next_words(const char *named, const char *menu);
int cli_completions(const char *shell, CliLine *out, CliLine *said);

// a command the command line runs itself, in the terminal: the code to exit with (command_line_console.c)
int cli_console(const char *command, const char *named, const char *const *words, unsigned int count);
// `orior_cli edit search [--case] [--word] [--regex] <text>`: every line in the tree's files that holds the text, as
// path:line:column: line; the code to exit with, 1 where none does, as grep exits (command_line_search.c)
int cli_console_search(const char *named, const char *const *words, unsigned int count);
// the path in the tree and what follows it of a word that names a file of the tree, as path, path:line or
// path:line:column, into `out`: 1 where it names one, 0 with why said where it names a file outside the tree, -1 where
// it names no file
int cli_file_word(const char *named, const char *word, CliLine *out, CliLine *said);

// the commands of `orior_cli file` and `orior_cli run` that work on plugins, toolchains, files and repositories, each
// giving the code to exit with (command_line_tools.c, command_line_toolchains.c, command_line_files.c)
int cli_plugins_list(void);
int cli_new_plugin(const char *const *words, unsigned int count);
int cli_toolchains_words(const char *const *words, unsigned int count);
// the command line's words that are programs of their own, `serve`, `write-raised`, `keep-run`, `deploy`, `libraries`
// and `sea`, where `first` is one: 1 with the code to exit with into `code`, or 0 where it is none of them
// (command_line_programs.c)
int cli_line_program(const char *first, const char *named, const char *const *words, unsigned int count, int *code);
// the toolchain `add` describes, its name, then each flag and its value, added: 1 with what was added said into
// `out`, or 0 with why said (command_line_tools.c)
int cli_toolchain_added(const char *const *words, unsigned int count, CliLine *out, CliLine *said);
int cli_run_file_words(const char *named, const char *const *words, unsigned int count);
int cli_clone_words(const char *const *words, unsigned int count);
int cli_validate_files(const char *const *words, unsigned int count);
int cli_format_files(const char *const *words, unsigned int count);

#endif
