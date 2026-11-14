// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner.h: starting a job, carrying its output line by line, and stopping it (runner.c makes each step's command,
// runner_run.c runs a job's steps on a thread of their own, runner_lines.c carries the lines and finds the pages).
//
// A job's steps run one after another in the tree's top folder, and the first that exits other than 0 ends the job.
// Every line either stream writes goes to the run's sink as it comes, and then the end, carrying the exit code and the
// pages the job wrote. Each line and the end carry the milliseconds since the run started, to the microsecond, which
// the window's time ruler reads. The window's sink sends them to the page; the command line's prints them.
//
// The window's runs are detached: their steps get no console of their own on Windows and a process group of their own
// elsewhere, and only Stop ends them. The command line's are attached: they share its console, and the key that stops
// it stops them
#ifndef RUNNER_H
#define RUNNER_H
#include "../command_line/callback_cfg.h"
#include "../command_line/catalog.h"
#include "../command_line/rings.h"
#include "embed_dispatch_layout.h"
#include "memoria_anularis/memoria_anularis.h"

typedef struct CliRunnerErrMsgCfg CliRunnerErrMsgCfg;
typedef struct CliRunnerCommandCfg CliRunnerCommandCfg;
typedef struct CliRunnerErrMsgRootCfg CliRunnerErrMsgRootCfg;
typedef struct CliRunnerNameCfg CliRunnerNameCfg;
typedef struct CliRunnerCommandNameCfg CliRunnerCommandNameCfg;
typedef struct CliRunnerLinesCfg CliRunnerLinesCfg;
typedef struct CliRunnerTextCfg CliRunnerTextCfg;
typedef struct CliRunnerCfg CliRunnerCfg;
typedef struct CliRunnerCommandFolderCfg CliRunnerCommandFolderCfg;
typedef struct CliRunnerNameCommandCfg CliRunnerNameCommandCfg;
typedef struct CliRunnerCommandWordCfg CliRunnerCommandWordCfg;
typedef struct CliRunnerLinesIndexCfg CliRunnerLinesIndexCfg;
typedef struct CliRunnerEventCfg CliRunnerEventCfg;
typedef struct CliRunnerVariableCfg CliRunnerVariableCfg;
typedef struct CliRunnerErrMsgRunNumberCfg CliRunnerErrMsgRunNumberCfg;

// the variable a job's step finds the run's number in: set on every step a run starts, and read by a command that
// would otherwise reach a window open on the tree
#define CLI_RUN_VARIABLE "ORIOR_RUN"

// the most settings, and the bytes of their texts, a command contains
#define CLI_COMMAND_SETTINGS 64u

#define CLI_COMMAND_TEXT (1u << 16u)

// A command a step runs: its words, the folder it runs in where it names one, and the settings of its environment,
// their texts stored in its own
typedef struct
{
    CliWords words;
    char folder[CLI_PATH_MAX_LEN];
    int has_folder;
    CliSetting setting[CLI_COMMAND_SETTINGS];
    unsigned int settings;
    char text[CLI_COMMAND_TEXT];
    unsigned int used;
} CliCommand;

// the most runs open at once; the most lines and bytes of them a run retains of what its steps wrote; the bytes one
// run's rings take in the rings block, its text then its line records each on the block's boundary; and the bytes
// every run's take, the runner's part of the block
#define CLI_RUNS_MAX 16u
#define CLI_LINES_MAX 16384u

#define CLI_LINES_TEXT (1u << 20u)

// A line a run's step wrote: the count of the text bytes put before its first, and its bytes
typedef struct
{
    unsigned int start;
    unsigned int length;
} CliRunnerLine;

// The lines a run's steps wrote, in MMgr's rings over the run's span of the rings block: the text, a byte ring; the
// line records, a segment ring with a record a segment; the text bytes ever put and ever consumed, whose difference
// places a line's bytes ahead of the text ring's tail; whether the rings are laid; and the bytes each ring streams.
// The oldest lines are let go as a new one needs the room
typedef struct
{
    mmgr_ring text;
    mmgr_ring line;
    unsigned int text_put;
    unsigned int text_taken;
    int laid;
    embed_u8 *text_bytes;
    CliRunnerLine *line_bytes;
} CliRunLines;
#define CLI_RUN_LINES_TEXT_AT 0ULL
#define CLI_RUN_LINES_LINE_AT CLI_RINGS_ALIGNED(CLI_LINES_TEXT)
#define CLI_RUN_LINES_SET_BYTES                                                                                        \
    (CLI_RUN_LINES_LINE_AT + CLI_RINGS_ALIGNED((unsigned long long)CLI_LINES_MAX * sizeof(CliRunnerLine)))
#define CLI_RUNNER_LINES_RING_BYTES ((unsigned long long)CLI_RUNS_MAX * CLI_RUN_LINES_SET_BYTES)

// the ring lays its byte stream and its segments only over counts of two's powers
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_LINES_TEXT), "the lines' text ring's bytes are a power of two");
EMBED_STATIC_ASSERT(MMGR_RING_POW2(CLI_LINES_MAX), "the lines' record ring's segments are a power of two");

// what a run reports as it goes: its start, each line, then its end, in that order
enum
{
    CLI_RUN_EVENT_START,
    CLI_RUN_EVENT_LINE,
    CLI_RUN_EVENT_END
};

// the most pages a run's end names
#define CLI_VIEWS_MAX 256u

// What a run reports: its type and number; for its start, its job; for a line, its stream, "stdout", "stderr" or
// "command", and its text; for its end, its exit code where it has one, whether it was stopped, the pages it wrote by
// their paths in the tree, and the program it made, by its path in the tree where it is in the tree; and the
// milliseconds since the run started
typedef struct
{
    unsigned int type;
    unsigned long long run;
    const char *job;
    const char *stream;
    const char *text;
    int has_code;
    long long code;
    int stopped;
    const char (*views)[CLI_PATH_MAX_LEN];
    unsigned int view_count;
    const char *made;
    double ms;
} CliRunEvent;

typedef struct
{
    void *context;
    const CliRunEvent *event;
} CliEachEventCfg;

// Where a run's lines and end go
typedef struct
{
    void (*emit)(const CliEachEventCfg *args);
    void *context;
} CliRunSink;

// a command begun with its program; a word put after its words; a setting put; whether it sets `name`; and its folder
// set
void cli_command_begin(const CliRunnerCfg *args);
void cli_command_word(const CliRunnerCommandWordCfg *args);
void cli_command_set(const CliRunnerCommandNameCfg *args);
int cli_command_sets(const CliRunnerNameCommandCfg *args);
void cli_command_folder(const CliRunnerCommandFolderCfg *args);

// a line put after the lines, the rings laid where they are not yet and the oldest lines let go for the room; the
// lines emptied; how many lines stand; the bytes of the longest; and the line at `index` copied into `out`, cut to
// `capacity` with its 0, where `out` is set: its bytes
void cli_lines_append(const CliRunnerLinesCfg *args);
void cli_lines_clear(CliRunLines *lines);
unsigned int cli_lines_count(const CliRunnerLinesIndexCfg *args);
unsigned long long cli_lines_longest(const CliRunnerLinesIndexCfg *args);
unsigned long long cli_lines_read(const CliRunnerLinesIndexCfg *args);

// the bash that runs the tree's scripts into `path`, sized for CLI_PATH_MAX_LEN: ORIOR_BASH, else the one Toolchains
// names, else bash off Windows and Git's on it: 1, or 0 with why given; and the Python that runs them: ORIOR_PYTHON,
// else the one Toolchains names, else python on Windows and python3 elsewhere
int cli_runner_bash(const CliRunnerNameCfg *args);
void cli_runner_python(const CliRunnerNameCfg *args);
// a built program under build/ into `path`: 1, or 0 with why given
int cli_runner_build(const CliRunnerNameCfg *args);
// the program and its words an environment variable names into `words`: its text as one word where it is a file, as a
// Windows path with its backslashes is, and else its words as a shell splits them; 1, or 0 where it is not set or
// names nothing
int cli_runner_program_words(const CliRunnerVariableCfg *args);
// `text` split into words the way a shell does for plain words, single quotes and double quotes: 1, or 0 with why given
int cli_runner_shell_words(const CliRunnerTextCfg *args);
// why `values` do not do for the job: a value it needs not given, or one given that is not among a choice's values;
// 1 where they do. And the line each step the job runs with these values shows, handed to `each`: 1, or 0 with why
// given
int cli_runner_check(const CliRunnerErrMsgCfg *args);
int cli_runner_show(const CliRunnerErrMsgRootCfg *args);
// the command a step runs into `command`, with the line the window shows for it into `displayed`: 1, or 0 with why
// given
int cli_runner_command(const CliRunnerCommandCfg *args);
// whether a step runs with these values: every param an Only names has the value it names, and every param a Given
// names has a value
int cli_runner_runs(const CliRunnerCommandCfg *args);

// the runs made attached, sharing this process's console, as the command line's are; a job started, its steps on a
// thread of their own, its number into `run`: 1, or 0 with why given; a run waited on until its end is reported; and a
// run and everything it started stopped (runner_run.c)
void cli_runs_attach(void);
int cli_runs_start(const CliRunnerErrMsgCfg *args);
void cli_runs_wait(const CliRunnerErrMsgRunNumberCfg *args);
int cli_runs_stop(const CliRunnerErrMsgRunNumberCfg *args);

struct CliRunnerErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *root;
    const CliCatalog *catalog;
    unsigned int job;
    const CliValues *values;
    CliRunSink sink;
    unsigned long long *run_number_pointer;
};

struct CliRunnerCommandCfg
{
    CliCommand *command;
    VerbaLinea *err_msg;
    const char *root;
    const CliCatalog *catalog;
    const CliValues *values;
    const CliStep *step;
    VerbaLinea *displayed;
};

struct CliRunnerErrMsgRootCfg
{
    VerbaLinea *err_msg;
    const char *root;
    const CliCatalog *catalog;
    unsigned int job;
    const CliValues *values;
    void (*each)(const CliEachDisplayedCfg *args);
    void *context;
};

struct CliRunnerNameCfg
{
    const char *name;
    char *path;
    VerbaLinea *err_msg;
    const char *root;
};

struct CliRunnerCommandNameCfg
{
    CliCommand *command;
    const char *name;
    const char *value;
};

struct CliRunnerLinesCfg
{
    CliRunLines *lines;
    const char *text;
    unsigned long long length;
};

struct CliRunnerTextCfg
{
    const char *text;
    VerbaLinea *err_msg;
    CliWords *words;
};

struct CliRunnerCfg
{
    CliCommand *command;
    const char *program;
};

struct CliRunnerCommandFolderCfg
{
    CliCommand *command;
    const char *folder;
};

struct CliRunnerNameCommandCfg
{
    const char *name;
    const CliCommand *command_read;
};

struct CliRunnerCommandWordCfg
{
    CliCommand *command;
    const char *word_text;
};

struct CliRunnerLinesIndexCfg
{
    const CliRunLines *lines_read;
    unsigned int index;
    char *out;
    unsigned long long capacity;
};

struct CliRunnerEventCfg
{
    const CliRunEvent *event;
    VerbaLinea *out;
};

struct CliRunnerVariableCfg
{
    const char *variable;
    CliWords *words;
};

struct CliRunnerErrMsgRunNumberCfg
{
    VerbaLinea *err_msg;
    unsigned long long run_number;
};

typedef struct
{
    void (*command_begin)(const CliRunnerCfg *args);
} CliRunnerNs;
EMBED_TABLE_LAYOUT(CliRunnerNs, command_begin);
EMBED_TABLE_STORAGE CliRunnerNs cli_runner EMBED_UNUSED = {
    .command_begin = cli_command_begin,
};

typedef struct
{
    void (*command_word)(const CliRunnerCommandWordCfg *args);
} CliRunnerCommandWordNs;
EMBED_TABLE_LAYOUT(CliRunnerCommandWordNs, command_word);
EMBED_TABLE_STORAGE CliRunnerCommandWordNs cli_runner_command_word EMBED_UNUSED = {
    .command_word = cli_command_word,
};

typedef struct
{
    void (*command_set)(const CliRunnerCommandNameCfg *args);
} CliRunnerCommandNameNs;
EMBED_TABLE_LAYOUT(CliRunnerCommandNameNs, command_set);
EMBED_TABLE_STORAGE CliRunnerCommandNameNs cli_runner_command_name EMBED_UNUSED = {
    .command_set = cli_command_set,
};

typedef struct
{
    int (*command_sets)(const CliRunnerNameCommandCfg *args);
} CliRunnerNameCommandNs;
EMBED_TABLE_LAYOUT(CliRunnerNameCommandNs, command_sets);
EMBED_TABLE_STORAGE CliRunnerNameCommandNs cli_runner_name_command EMBED_UNUSED = {
    .command_sets = cli_command_sets,
};

typedef struct
{
    void (*command_folder)(const CliRunnerCommandFolderCfg *args);
} CliRunnerCommandFolderNs;
EMBED_TABLE_LAYOUT(CliRunnerCommandFolderNs, command_folder);
EMBED_TABLE_STORAGE CliRunnerCommandFolderNs cli_runner_command_folder EMBED_UNUSED = {
    .command_folder = cli_command_folder,
};

typedef struct
{
    void (*lines_append)(const CliRunnerLinesCfg *args);
} CliRunnerLinesNs;
EMBED_TABLE_LAYOUT(CliRunnerLinesNs, lines_append);
EMBED_TABLE_STORAGE CliRunnerLinesNs cli_runner_lines EMBED_UNUSED = {
    .lines_append = cli_lines_append,
};

typedef struct
{
    unsigned int (*lines_count)(const CliRunnerLinesIndexCfg *args);
    unsigned long long (*lines_longest)(const CliRunnerLinesIndexCfg *args);
    unsigned long long (*lines_read)(const CliRunnerLinesIndexCfg *args);
} CliRunnerLinesIndexNs;
EMBED_TABLE_LAYOUT(CliRunnerLinesIndexNs, lines_count, lines_longest, lines_read);
EMBED_TABLE_STORAGE CliRunnerLinesIndexNs cli_runner_lines_index EMBED_UNUSED = {
    .lines_count = cli_lines_count,
    .lines_longest = cli_lines_longest,
    .lines_read = cli_lines_read,
};

typedef struct
{
    int (*bash)(const CliRunnerNameCfg *args);
    void (*python)(const CliRunnerNameCfg *args);
    int (*build)(const CliRunnerNameCfg *args);
} CliRunnerNameNs;
EMBED_TABLE_LAYOUT(CliRunnerNameNs, bash, python, build);
EMBED_TABLE_STORAGE CliRunnerNameNs cli_runner_name EMBED_UNUSED = {
    .bash = cli_runner_bash,
    .python = cli_runner_python,
    .build = cli_runner_build,
};

typedef struct
{
    int (*program_words)(const CliRunnerVariableCfg *args);
} CliRunnerVariableNs;
EMBED_TABLE_LAYOUT(CliRunnerVariableNs, program_words);
EMBED_TABLE_STORAGE CliRunnerVariableNs cli_runner_variable EMBED_UNUSED = {
    .program_words = cli_runner_program_words,
};

typedef struct
{
    int (*shell_words)(const CliRunnerTextCfg *args);
} CliRunnerTextNs;
EMBED_TABLE_LAYOUT(CliRunnerTextNs, shell_words);
EMBED_TABLE_STORAGE CliRunnerTextNs cli_runner_text EMBED_UNUSED = {
    .shell_words = cli_runner_shell_words,
};

typedef struct
{
    int (*check)(const CliRunnerErrMsgCfg *args);
    int (*runs_start)(const CliRunnerErrMsgCfg *args);
} CliRunnerErrMsgNs;
EMBED_TABLE_LAYOUT(CliRunnerErrMsgNs, check, runs_start);
EMBED_TABLE_STORAGE CliRunnerErrMsgNs cli_runner_err_msg EMBED_UNUSED = {
    .check = cli_runner_check,
    .runs_start = cli_runs_start,
};

typedef struct
{
    int (*show)(const CliRunnerErrMsgRootCfg *args);
} CliRunnerErrMsgRootNs;
EMBED_TABLE_LAYOUT(CliRunnerErrMsgRootNs, show);
EMBED_TABLE_STORAGE CliRunnerErrMsgRootNs cli_runner_err_msg_root EMBED_UNUSED = {
    .show = cli_runner_show,
};

typedef struct
{
    int (*command)(const CliRunnerCommandCfg *args);
    int (*runs)(const CliRunnerCommandCfg *args);
} CliRunnerCommandNs;
EMBED_TABLE_LAYOUT(CliRunnerCommandNs, command, runs);
EMBED_TABLE_STORAGE CliRunnerCommandNs cli_runner_command_table EMBED_UNUSED = {
    .command = cli_runner_command,
    .runs = cli_runner_runs,
};

typedef struct
{
    void (*runs_wait)(const CliRunnerErrMsgRunNumberCfg *args);
    int (*runs_stop)(const CliRunnerErrMsgRunNumberCfg *args);
} CliRunnerErrMsgRunNumberNs;
EMBED_TABLE_LAYOUT(CliRunnerErrMsgRunNumberNs, runs_wait, runs_stop);
EMBED_TABLE_STORAGE CliRunnerErrMsgRunNumberNs cli_runner_err_msg_run_number EMBED_UNUSED = {
    .runs_wait = cli_runs_wait,
    .runs_stop = cli_runs_stop,
};

#endif
