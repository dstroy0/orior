// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_internal.h: what the command line's sources share: its output and its errors, a line each; the
// terminal's width; a text cut to a width and broken into lines; a word quoted as the runner's shell words read it; the
// tree's jobs read, a job found by a word, and the values its words give; and the codes it exits with
#ifndef COMMAND_LINE_INTERNAL_H
#define COMMAND_LINE_INTERNAL_H
#include "callback_cfg.h"
#include "catalog.h"
#include "command_line.h"
#include "embed_dispatch_layout.h"

typedef struct CliCommandLineCatalogCtx CliCommandLineCatalogCtx;
typedef struct CliCommandLineTextCtx CliCommandLineTextCtx;
typedef struct CliCommandLineTextEachCtx CliCommandLineTextEachCtx;
typedef struct CliCommandLineCatalogAmongCtx CliCommandLineCatalogAmongCtx;
typedef struct CliCommandLineRootCtx CliCommandLineRootCtx;
typedef struct CliCommandLineWordsCtx CliCommandLineWordsCtx;
typedef struct CliCommandLineWordsCountCtx CliCommandLineWordsCountCtx;
typedef struct CliCommandLineTextSizeCtx CliCommandLineTextSizeCtx;
typedef struct CliCommandLineOutCtx CliCommandLineOutCtx;
typedef struct CliCommandLineWordsCountMenuCtx CliCommandLineWordsCountMenuCtx;
typedef struct CliCommandLineOutErrMsgCtx CliCommandLineOutErrMsgCtx;
typedef struct CliCommandLineWordCtx CliCommandLineWordCtx;
typedef struct CliCommandLineOutErrMsgShellCtx CliCommandLineOutErrMsgShellCtx;
typedef struct CliCommandLineCatalogJobCtx CliCommandLineCatalogJobCtx;
typedef struct CliCommandLineCatalogMenuCtx CliCommandLineCatalogMenuCtx;
typedef struct CliCommandLineCtx CliCommandLineCtx;
typedef struct CliCommandLineNamedCtx CliCommandLineNamedCtx;
typedef struct CliCommandLineSizeCtx CliCommandLineSizeCtx;

// the code a run exits with where a step could not start or gave no code, and the code for words the program does not
// take
#define CLI_NO_CODE 1
#define CLI_WRONG 2

// the max jobs the catalog the command line reads contains
#define CLI_LINE_JOBS 4096u

// `size` bytes of `text` written as a line of the output, and of the errors; and the same of a text ended by a 0
// (command_line.c)
void cli_out_line(const CliCommandLineTextSizeCtx *args);
void cli_err_line(const CliCommandLineTextSizeCtx *args);
void cli_out(const CliCommandLineCtx *args);
void cli_err(const CliCommandLineCtx *args);
// the terminal's width, COLUMNS where it is 40 or more, else 100
unsigned int cli_width(void);
// `size` bytes of `text` cut to `max` characters, ending in `...` where it was cut, written into `out`; and broken into
// lines of at max `max` characters at whitespace, each led by `lead`, each passed to `each`
void cli_cut(const CliCommandLineTextSizeCtx *args);
void cli_wrap(const CliCommandLineTextCtx *args);
// a word written quoted so the runner's shell words give it back whole; and whether `text` contains `word`, case aside
// as ASCII has it
void cli_quoted_word(const CliCommandLineOutCtx *args);
int cli_contains_word(const CliCommandLineCtx *args);
// the tree's jobs read into the catalog's cell, stored until the next read
CliCatalog *cli_command_line_catalog(const CliCommandLineWordCtx *args);
// the job a word names among the jobs of `catalog` at `among`, `among_count` of them, or among every one where `among`
// is NULL: the one whose id it is, else the only one whose id ends in it after a slash or whose title it is: its place,
// or CLI_CATALOG_NONE with why given
unsigned int cli_job_by_name(const CliCommandLineCatalogAmongCtx *args);
// the values a run's `count` words give the job at `job`: `key=value` for each, a key given twice retaining both, and
// the words after `--` as the job's free words, quoted; every param not given its default. 1, or 0 with why given
int cli_job_values(const CliCommandLineCatalogCtx *args);
// what a stage job works on: the second part of its file's path, into `subject`, sized for CLI_NAME_MAX_LEN
void cli_job_subject(const CliCommandLineCatalogJobCtx *args);

// the tree's tests listed, each with its line, or as JSON with `--json`; the tree's jobs listed, those that contain
// `word` where it is given; a job displayed; and a job run here, reported to the window where a terminal of the window's
// started this, each giving the code to exit with (command_line_jobs.c)
int cli_tests_list(const CliCommandLineRootCtx *args);
int cli_jobs_list(const CliCommandLineWordCtx *args);
void cli_job_show(const CliCommandLineRootCtx *args);
int cli_job_run(const CliCommandLineRootCtx *args);
// a run of the command line's made one the window is notified of as it goes, on a channel of its own under runs, where
// a terminal of the window's started this process, else none; and `size` bytes of `body`, JSON, sent to it

// the jobs of the menu's groups, their places in the catalog into `list_entries`, sized for CLI_LINE_JOBS: their count;
// a menu's commands and the jobs of a menu of jobs listed, as `imp <menu>` alone shows them, its jobs from the
// tree at `root`, NULL where there is none; and a job of a menu of jobs displayed, as the window shows it on choosing, each
// giving the code to exit with (command_line_menus.c)
unsigned int cli_menu_jobs(const CliCommandLineCatalogMenuCtx *args);
int cli_menu_lines(const CliCommandLineWordCtx *args);
int cli_menu_job(const CliCommandLineWordsCountMenuCtx *args);

// the keys of the bridge listed, those that contain `word` where it is given, or the one it names displayed whole: the code
// to exit with (command_line_bridge.c)
int cli_bridge_words(const CliCommandLineWordCtx *args);

// the help, made from the menus, written into `out`; the keys of every command that has some, by menu; the words that
// can follow the menu `menu`; and the completion script for `shell` written into `out`: 1, or 0 with why given
// (command_line_help.c)
void cli_help(const CliCommandLineOutErrMsgShellCtx *args);
// the width the help's lines are wrapped to; and `text` broken at spaces into lines of at max `width` characters, each
// after the first indented `indent` more, each passed to `each`, a word wider than the width alone on its line
#define CLI_HELP_WIDTH 100ull
void cli_help_wrap(const CliCommandLineTextEachCtx *args);
void cli_keys(void);
void cli_next_words(const CliCommandLineNamedCtx *args);
int cli_completions(const CliCommandLineOutErrMsgShellCtx *args);

// a command the command line runs itself, in the terminal: the code to exit with (command_line_console.c)
int cli_console(const CliCommandLineWordsCountCtx *args);
// a setting of the tree of commands, its key the chain `command` and its words its value, as the node's `args_text`
// takes them: the value written to orior's settings.json and sent as it is to the window open on the tree, which takes
// it into what it draws with and reads no file for it; with no words, the text located written out. The code to exit
// with (command_line_console.c)
int cli_console_setting(const CliCommandLineWordsCountCtx *args);
// `imp edit search [--case] [--word] [--regex] <text>`: every line in the tree's files that contains the text, as
// path:line:column: line; the code to exit with, 1 where none does, as grep exits (command_line_search.c)
int cli_console_search(const CliCommandLineWordsCountCtx *args);
// the path in the tree and what follows it of a word that names a file of the tree, as path, path:line or
// path:line:column, into `out`: 1 where it names one, 0 with why given where it names a file outside the tree, -1 where
// it names no file
int cli_file_word(const CliCommandLineOutCtx *args);

// the commands of `imp file` and `imp run` that work on plugins, toolchains, files and repositories, each
// giving the code to exit with (command_line_tools.c, command_line_toolchains.c, command_line_files.c)
int cli_plugins_list(void);
int cli_new_plugin(const CliCommandLineWordsCountCtx *args);
int cli_toolchains_words(const CliCommandLineWordsCountCtx *args);
// the command line's words that are programs of their own, `write-raised`, `deploy`,
// `libraries` and `sea`, where `first` is one: 1 with the code to exit with into `code`, or 0 where it is none of them
// (command_line_programs.c)
int cli_line_program(const CliCommandLineWordsCtx *args);
// the toolchain `add` describes, its name, then each flag and its value, added: 1 with what was added reported into
// `out`, or 0 with why given (command_line_tools.c)
int cli_toolchain_add(const CliCommandLineOutErrMsgCtx *args);
int cli_run_file_words(const CliCommandLineWordsCountCtx *args);
int cli_clone_words(const CliCommandLineWordsCountCtx *args);
int cli_validate_files(const CliCommandLineWordsCountCtx *args);
int cli_format_files(const CliCommandLineWordsCountCtx *args);

struct CliCommandLineCatalogCtx
{
    const CliCatalog *catalog;
    VerbaLinea *err_msg;
    unsigned int job;
    const char *const *words;
    unsigned int count;
    CliValues *values;
};

struct CliCommandLineTextCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long max;
    const char *lead;
    void (*each)(const CliEachLineSizeCfg *args);
    void *context;
};

struct CliCommandLineTextEachCtx
{
    const char *text;
    void (*each)(const CliEachLineSizeCfg *args);
    void *context;
    unsigned long long width;
    unsigned long long indent;
};

struct CliCommandLineCatalogAmongCtx
{
    const CliCatalog *catalog;
    const unsigned int *among;
    unsigned int among_count;
    const char *name;
    VerbaLinea *err_msg;
};

struct CliCommandLineRootCtx
{
    const char *root;
    const CliCatalog *catalog;
    unsigned int job;
    const char *const *words;
    unsigned int count;
};

struct CliCommandLineWordsCtx
{
    const char *const *words;
    unsigned int count;
    const char *named;
    const char *first;
    int *code;
};

struct CliCommandLineWordsCountCtx
{
    const char *const *words;
    unsigned int count;
    const char *named;
    const char *command;
    const char *args_text;
};

struct CliCommandLineTextSizeCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long max;
    VerbaLinea *out;
};

struct CliCommandLineOutCtx
{
    VerbaLinea *out;
    const char *word_text;
    VerbaLinea *err_msg;
    const char *named;
};

struct CliCommandLineWordsCountMenuCtx
{
    const char *const *words;
    unsigned int count;
    unsigned int menu;
    const char *named;
};

struct CliCommandLineOutErrMsgCtx
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *const *words;
    unsigned int count;
};

struct CliCommandLineWordCtx
{
    const char *word_text;
    const char *root;
    unsigned int menu;
};

struct CliCommandLineOutErrMsgShellCtx
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *shell;
};

struct CliCommandLineCatalogJobCtx
{
    const CliCatalog *catalog;
    unsigned int job;
    char *subject;
};

struct CliCommandLineCatalogMenuCtx
{
    const CliCatalog *catalog;
    unsigned int menu;
    unsigned int *list_entries;
};

struct CliCommandLineCtx
{
    const char *text;
    const char *word_text;
};

struct CliCommandLineNamedCtx
{
    const char *named;
    const char *menu_word;
};

struct CliCommandLineSizeCtx
{
    unsigned long long size;
    const char *body;
};

typedef struct
{
    void (*out_line)(const CliCommandLineTextSizeCtx *args);
    void (*err_line)(const CliCommandLineTextSizeCtx *args);
    void (*cut)(const CliCommandLineTextSizeCtx *args);
} CliCommandLineTextSizeNs;
EMBED_TABLE_LAYOUT(CliCommandLineTextSizeNs, out_line, err_line, cut);
EMBED_TABLE_STORAGE CliCommandLineTextSizeNs cli_command_line_text_size_internal EMBED_UNUSED = {
    .out_line = cli_out_line,
    .err_line = cli_err_line,
    .cut = cli_cut,
};

typedef struct
{
    void (*out)(const CliCommandLineCtx *args);
    void (*err)(const CliCommandLineCtx *args);
    int (*contains_word)(const CliCommandLineCtx *args);
} CliCommandLineNsTable;
EMBED_TABLE_LAYOUT(CliCommandLineNsTable, out, err, contains_word);
EMBED_TABLE_STORAGE CliCommandLineNsTable cli_command_line_internal EMBED_UNUSED = {
    .out = cli_out,
    .err = cli_err,
    .contains_word = cli_contains_word,
};

typedef struct
{
    void (*wrap)(const CliCommandLineTextCtx *args);
} CliCommandLineTextNs;
EMBED_TABLE_LAYOUT(CliCommandLineTextNs, wrap);
EMBED_TABLE_STORAGE CliCommandLineTextNs cli_command_line_text_internal EMBED_UNUSED = {
    .wrap = cli_wrap,
};

typedef struct
{
    void (*quoted_word)(const CliCommandLineOutCtx *args);
    int (*file_word)(const CliCommandLineOutCtx *args);
} CliCommandLineOutNs;
EMBED_TABLE_LAYOUT(CliCommandLineOutNs, quoted_word, file_word);
EMBED_TABLE_STORAGE CliCommandLineOutNs cli_command_line_out_internal EMBED_UNUSED = {
    .quoted_word = cli_quoted_word,
    .file_word = cli_file_word,
};

typedef struct
{
    CliCatalog *(*catalog)(const CliCommandLineWordCtx *args);
    int (*jobs_list)(const CliCommandLineWordCtx *args);
    int (*menu_lines)(const CliCommandLineWordCtx *args);
    int (*bridge_words)(const CliCommandLineWordCtx *args);
} CliCommandLineWordNs;
EMBED_TABLE_LAYOUT(CliCommandLineWordNs, catalog, jobs_list, menu_lines, bridge_words);
EMBED_TABLE_STORAGE CliCommandLineWordNs cli_command_line_word_internal EMBED_UNUSED = {
    .catalog = cli_command_line_catalog,
    .jobs_list = cli_jobs_list,
    .menu_lines = cli_menu_lines,
    .bridge_words = cli_bridge_words,
};

typedef struct
{
    unsigned int (*job_by_name)(const CliCommandLineCatalogAmongCtx *args);
} CliCommandLineCatalogAmongNs;
EMBED_TABLE_LAYOUT(CliCommandLineCatalogAmongNs, job_by_name);
EMBED_TABLE_STORAGE CliCommandLineCatalogAmongNs cli_command_line_catalog_among_internal EMBED_UNUSED = {
    .job_by_name = cli_job_by_name,
};

typedef struct
{
    int (*job_values)(const CliCommandLineCatalogCtx *args);
} CliCommandLineCatalogNs;
EMBED_TABLE_LAYOUT(CliCommandLineCatalogNs, job_values);
EMBED_TABLE_STORAGE CliCommandLineCatalogNs cli_command_line_catalog_internal EMBED_UNUSED = {
    .job_values = cli_job_values,
};

typedef struct
{
    void (*job_subject)(const CliCommandLineCatalogJobCtx *args);
} CliCommandLineCatalogJobNs;
EMBED_TABLE_LAYOUT(CliCommandLineCatalogJobNs, job_subject);
EMBED_TABLE_STORAGE CliCommandLineCatalogJobNs cli_command_line_catalog_job_internal EMBED_UNUSED = {
    .job_subject = cli_job_subject,
};

typedef struct
{
    int (*tests_list)(const CliCommandLineRootCtx *args);
    void (*job_show)(const CliCommandLineRootCtx *args);
    int (*job_run)(const CliCommandLineRootCtx *args);
} CliCommandLineRootNs;
EMBED_TABLE_LAYOUT(CliCommandLineRootNs, tests_list, job_show, job_run);
EMBED_TABLE_STORAGE CliCommandLineRootNs cli_command_line_root_internal EMBED_UNUSED = {
    .tests_list = cli_tests_list,
    .job_show = cli_job_show,
    .job_run = cli_job_run,
};

typedef struct
{
    unsigned int (*menu_jobs)(const CliCommandLineCatalogMenuCtx *args);
} CliCommandLineCatalogMenuNs;
EMBED_TABLE_LAYOUT(CliCommandLineCatalogMenuNs, menu_jobs);
EMBED_TABLE_STORAGE CliCommandLineCatalogMenuNs cli_command_line_catalog_menu_internal EMBED_UNUSED = {
    .menu_jobs = cli_menu_jobs,
};

typedef struct
{
    int (*menu_job)(const CliCommandLineWordsCountMenuCtx *args);
} CliCommandLineWordsCountMenuNs;
EMBED_TABLE_LAYOUT(CliCommandLineWordsCountMenuNs, menu_job);
EMBED_TABLE_STORAGE CliCommandLineWordsCountMenuNs cli_command_line_words_count_menu_internal EMBED_UNUSED = {
    .menu_job = cli_menu_job,
};

typedef struct
{
    void (*help)(const CliCommandLineOutErrMsgShellCtx *args);
    int (*completions)(const CliCommandLineOutErrMsgShellCtx *args);
} CliCommandLineOutErrMsgShellNs;
EMBED_TABLE_LAYOUT(CliCommandLineOutErrMsgShellNs, help, completions);
EMBED_TABLE_STORAGE CliCommandLineOutErrMsgShellNs cli_command_line_out_err_msg_shell_internal EMBED_UNUSED = {
    .help = cli_help,
    .completions = cli_completions,
};

typedef struct
{
    void (*help_wrap)(const CliCommandLineTextEachCtx *args);
} CliCommandLineTextEachNs;
EMBED_TABLE_LAYOUT(CliCommandLineTextEachNs, help_wrap);
EMBED_TABLE_STORAGE CliCommandLineTextEachNs cli_command_line_text_each_internal EMBED_UNUSED = {
    .help_wrap = cli_help_wrap,
};

typedef struct
{
    void (*next_words)(const CliCommandLineNamedCtx *args);
} CliCommandLineNamedNsTable;
EMBED_TABLE_LAYOUT(CliCommandLineNamedNsTable, next_words);
EMBED_TABLE_STORAGE CliCommandLineNamedNsTable cli_command_line_named_internal EMBED_UNUSED = {
    .next_words = cli_next_words,
};

typedef struct
{
    int (*console)(const CliCommandLineWordsCountCtx *args);
    int (*console_setting)(const CliCommandLineWordsCountCtx *args);
    int (*console_search)(const CliCommandLineWordsCountCtx *args);
    int (*new_plugin)(const CliCommandLineWordsCountCtx *args);
    int (*toolchains_words)(const CliCommandLineWordsCountCtx *args);
    int (*run_file_words)(const CliCommandLineWordsCountCtx *args);
    int (*clone_words)(const CliCommandLineWordsCountCtx *args);
    int (*validate_files)(const CliCommandLineWordsCountCtx *args);
    int (*format_files)(const CliCommandLineWordsCountCtx *args);
} CliCommandLineWordsCountNs;
EMBED_TABLE_LAYOUT(CliCommandLineWordsCountNs, console, console_setting, console_search, new_plugin, toolchains_words,
                   run_file_words, clone_words, validate_files, format_files);
EMBED_TABLE_STORAGE CliCommandLineWordsCountNs cli_command_line_words_count_internal EMBED_UNUSED = {
    .console = cli_console,
    .console_setting = cli_console_setting,
    .console_search = cli_console_search,
    .new_plugin = cli_new_plugin,
    .toolchains_words = cli_toolchains_words,
    .run_file_words = cli_run_file_words,
    .clone_words = cli_clone_words,
    .validate_files = cli_validate_files,
    .format_files = cli_format_files,
};

typedef struct
{
    int (*line_program)(const CliCommandLineWordsCtx *args);
} CliCommandLineWordsNs;
EMBED_TABLE_LAYOUT(CliCommandLineWordsNs, line_program);
EMBED_TABLE_STORAGE CliCommandLineWordsNs cli_command_line_words_internal EMBED_UNUSED = {
    .line_program = cli_line_program,
};

typedef struct
{
    int (*toolchain_add)(const CliCommandLineOutErrMsgCtx *args);
} CliCommandLineOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliCommandLineOutErrMsgNs, toolchain_add);
EMBED_TABLE_STORAGE CliCommandLineOutErrMsgNs cli_command_line_out_err_msg_internal EMBED_UNUSED = {
    .toolchain_add = cli_toolchain_add,
};

#endif
