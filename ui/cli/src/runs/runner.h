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

#include "../command_line/catalog.h"

// the variable a job's step finds the run's number in: set on every step a run starts, and read by a command that
// would otherwise reach a window open on the tree
#define CLI_RUN_VARIABLE "ORIOR_RUN"

// the most settings, and the bytes of their texts, a command holds
#define CLI_COMMAND_SETTINGS 64u

#define CLI_COMMAND_TEXT (1u << 16u)

// A command a step runs: its words, the folder it runs in where it names one, and the settings of its environment,
// their texts kept in its own
typedef struct
{
    CliWords words;
    char folder[CLI_PATH_LONGEST];
    int has_folder;
    CliSetting setting[CLI_COMMAND_SETTINGS];
    unsigned int settings;
    char text[CLI_COMMAND_TEXT];
    unsigned int used;
} CliCommand;

// the most lines and bytes of them a run keeps of what its steps wrote
#define CLI_LINES_MOST 16384u

#define CLI_LINES_TEXT (1u << 20u)

// The lines a run's steps wrote, each where it starts in the text, the oldest let go where there is no more room
typedef struct
{
    unsigned int start[CLI_LINES_MOST];
    unsigned int count;
    char text[CLI_LINES_TEXT];
    unsigned int used;
} CliLines;

// what a run says as it goes: its start, each line, then its end, in that order
enum
{
    CLI_SAID_START,
    CLI_SAID_LINE,
    CLI_SAID_END
};

// the most pages a run's end names
#define CLI_VIEWS_MOST 256u

// What a run says: its kind and number; for its start, its job; for a line, its stream, "stdout", "stderr" or
// "command", and its text; for its end, its exit code where it has one, whether it was stopped, the pages it wrote by
// their paths in the tree, and the program it made, by its path in the tree where it is in the tree; and the
// milliseconds since the run started
typedef struct
{
    unsigned int kind;
    unsigned long long run;
    const char *job;
    const char *stream;
    const char *text;
    int has_code;
    long long code;
    int stopped;
    const char (*views)[CLI_PATH_LONGEST];
    unsigned int view_count;
    const char *made;
    double ms;
} CliSaid;

// Where a run's lines and end go
typedef struct
{
    void (*said)(void *context, const CliSaid *said);
    void *context;
} CliRunSink;

// a command begun with its program; a word put after its words; a setting put; whether it sets `name`; and its folder
// set
void cli_command_begun(CliCommand *command, const char *program);
void cli_command_word(CliCommand *command, const char *word);
void cli_command_set(CliCommand *command, const char *name, const char *value);
int cli_command_sets(const CliCommand *command, const char *name);
void cli_command_folder(CliCommand *command, const char *folder);

// a line put after the lines; and the line at `index`
void cli_lines_put(CliLines *lines, const char *text, unsigned long long length);
const char *cli_lines_at(const CliLines *lines, unsigned int index);

// the bash that runs the tree's scripts into `path`, which holds CLI_PATH_LONGEST: ORIOR_BASH, else the one Toolchains
// names, else bash off Windows and Git's on it: 1, or 0 with why said; and the Python that runs them: ORIOR_PYTHON,
// else the one Toolchains names, else python on Windows and python3 elsewhere
int cli_runner_bash(char *path, CliLine *said);
void cli_runner_python(char *path);
// a built program under build/ into `path`: 1, or 0 with why said
int cli_runner_built(const char *root, const char *name, char *path, CliLine *said);
// the program and its words an environment variable names into `words`: its text as one word where it is a file, as a
// Windows path with its backslashes is, and else its words as a shell splits them; 1, or 0 where it is not set or
// names nothing
int cli_runner_program_words(const char *variable, CliWords *words);
// `text` split into words the way a shell does for plain words, single quotes and double quotes: 1, or 0 with why said
int cli_runner_shell_words(const char *text, CliWords *words, CliLine *said);
// why `values` do not do for the job: a value it needs not given, or one given that is not among a choice's values;
// 1 where they do. And the line each step the job runs with these values shows, handed to `each`: 1, or 0 with why said
int cli_runner_check(const CliCatalog *catalog, unsigned int job, const CliValues *values, CliLine *said);
int cli_runner_shown(const char *root, const CliCatalog *catalog, unsigned int job, const CliValues *values,
                     void (*each)(void *context, const char *shown), void *context, CliLine *said);
// the command a step runs into `command`, with the line the window shows for it into `shown`: 1, or 0 with why said
int cli_runner_command(const char *root, const CliCatalog *catalog, const CliStep *step, const CliValues *values,
                       CliCommand *command, CliLine *shown, CliLine *said);
// whether a step runs with these values: every param an Only names holds the value it names, and every param a Given
// names holds a value
int cli_runner_runs(const CliCatalog *catalog, const CliStep *step, const CliValues *values);

// the runs made attached, sharing this process's console, as the command line's are; a job started, its steps on a
// thread of their own, its number into `run`: 1, or 0 with why said; a run waited on until its end is said; and a run
// and everything it started stopped (runner_run.c)
void cli_runs_attached(void);
int cli_runs_start(CliRunSink sink, const char *root, const CliCatalog *catalog, unsigned int job,
                   const CliValues *values, unsigned long long *run, CliLine *said);
void cli_runs_wait(unsigned long long run);
int cli_runs_stop(unsigned long long run, CliLine *said);

// what a run says written as the page reads it: a start as `{"run", "job"}`, a line as `{"run", "stream", "text",
// "ms"}`, and an end as `{"run", "code", "stopped", "views", "made", "ms"}`, a code or a program it has not `null`
// (runner_said.c)
void cli_said_written(const CliSaid *said, CliLine *out);

#endif
