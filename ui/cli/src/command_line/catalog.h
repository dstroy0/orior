// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog.h: every job the app can start, read from the tree each time it is asked for (catalog.c holds the jobs,
// catalog_read.c reads them from the tree, catalog_scan.c reads what a file says of itself).
//
// Nothing here holds a list of sims, parts, programs, viewers or stages. Each is read from the file that defines it:
// the sims from the case in src/sims/run.sh, the driver's parts from its own usage text, the viewers and the stage
// scripts from where they sit. A sim added to run.sh is a job the next time the catalog is read, and a job whose file
// is gone is gone with it.
//
// A catalog keeps its jobs, their params, steps and arguments, and every text they hold in the storage its holder hands
// it, so the whole catalog and a run's one job are each kept in a room of their own and nothing is allocated
#ifndef CATALOG_H
#define CATALOG_H

#include "cli_internal.h"

// none: no text, no job
#define CLI_CATALOG_NONE 0xffffffffu

// how much of what a build does it says as it runs: every command it runs, as it runs it, where verbose, the first
// choice and so the one a build takes where none is given, and what its tools say of their own, where quiet
#define CLI_TRACE "trace"
#define CLI_TRACE_VERBOSE "verbose"
#define CLI_TRACE_QUIET "quiet"

// How a step's argument list is made from the values given. Each kind names the texts it holds in turn:
//   LIT text          written as it is
//   VALUE key         the value of a param, where one is given
//   FLAG flag key     a flag and then the value of a param, where one is given
//   EACH flag key     the flag before each value chosen of a param
//   WORDS key         the words of a param, split as a shell splits them, after `--` where the given text is not empty
//                     and `separated` is set
//   ENV key           no argument: the value of a param, where one is given, set in the step's environment under the
//                     param's key as the variable's name
//   AROUND before key after   the value of a param, where one is given, between the two texts, as one word
//   WHEN key value    the words listed, where the param's value is the value named
//   SET variable value        no argument: the variable set to the value in the step's environment
//   SET_WHEN param value variable setting   no argument: the variable set where the param's value is the value named
//   UNPATH name       no argument: the step's PATH without the folders that hold the program named
//   ONLY key value    no argument: the step runs only where the param's value is the value named
//   GIVEN key         no argument: the step runs only where a value of the param is given
//   SET_FROM variable key     no argument: the value of a param, where one is given, set under the variable named
//   FORMAT text       the text with each {key} in it filled with that param's value, as one word
//   FLAG_BUT flag key but     a flag and then the value of a param, where one is given other than the value named
//   ENV_BUT key but   no argument: the value of a param, where one is given other than the value named, set under the
//                     param's key
//   FOLDER folder     no argument: the step runs in the folder named, a path in the tree, and not its top folder
//   SOURCE_DATE       no argument: SOURCE_DATE_EPOCH set to the time of the commit the tree stands at, where the tree
//                     is in git and it is not set
//   CROSS kind        no argument: the C compiler and linker for the system and processor the job's params choose,
//                     set in the step's environment where they are another machine's, for a build of the kind named
enum
{
    CLI_ARG_LIT,
    CLI_ARG_VALUE,
    CLI_ARG_FLAG,
    CLI_ARG_EACH,
    CLI_ARG_WORDS,
    CLI_ARG_ENV,
    CLI_ARG_AROUND,
    CLI_ARG_WHEN,
    CLI_ARG_SET,
    CLI_ARG_SET_WHEN,
    CLI_ARG_UNPATH,
    CLI_ARG_ONLY,
    CLI_ARG_GIVEN,
    CLI_ARG_SET_FROM,
    CLI_ARG_FORMAT,
    CLI_ARG_FLAG_BUT,
    CLI_ARG_ENV_BUT,
    CLI_ARG_FOLDER,
    CLI_ARG_SOURCE_DATE,
    CLI_ARG_CROSS
};

// What a step runs: a bash script or a Python script by its path in the tree; a program a build job writes under one
// of the build folders, by its name; the program `program` of the toolchain `tool`; orior_cli itself; or the program
// the job's build made, found once the steps before it end, run with the PATH a program opened on its own gets where
// `system`
enum
{
    CLI_PROGRAM_BASH,
    CLI_PROGRAM_PYTHON,
    CLI_PROGRAM_BUILT,
    CLI_PROGRAM_TOOL,
    CLI_PROGRAM_ORIOR,
    CLI_PROGRAM_MADE
};

// An argument: its kind, its texts as its kind names them, whether WORDS asks for the separator, and the words WHEN
// lists, `words` of the list from `first`
typedef struct
{
    unsigned int kind;
    unsigned int text[4];
    int separated;
    unsigned int first;
    unsigned int words;
} CliArg;

// A step: its program's kind, its texts (the script, the name or the toolchain, and the toolchain's program), whether
// a made program runs with the system's PATH, and its arguments, `args` from `first`
typedef struct
{
    unsigned int program;
    unsigned int text;
    unsigned int tool_program;
    int system;
    unsigned int first;
    unsigned int args;
} CliStep;

// One input a job takes: its key; its kind, how the app asks for it: one of `choices`, several of them, a file, a
// folder, a path to write, a word, or free words added to the end; its choices, `choices` of the list from `first`;
// its default; and whether it is required
typedef struct
{
    unsigned int key;
    const char *kind;
    unsigned int first;
    unsigned int choices;
    unsigned int fallback;
    int required;
} CliParam;

// A job: its id, group, title, file and what it says of itself; its params and steps; and what it writes for a
// reader to open, "views" where it writes pages, "program" where it builds one, else empty
typedef struct
{
    unsigned int id;
    const char *group;
    unsigned int title;
    unsigned int file;
    unsigned int about;
    unsigned int param_first;
    unsigned int params;
    const char *opens;
    unsigned int step_first;
    unsigned int steps;
} CliJob;

// A catalog: its jobs, params, steps, arguments, lists of texts and texts in the storage handed it, each with how many
// it holds and how many it may; 1 in `full` where one did not fit; the count its cells are taken under where its
// storage grows, NULL where it was handed; and the most jobs it takes
typedef struct
{
    CliJob *job;
    unsigned int jobs;
    unsigned int jobs_most;
    CliParam *param;
    unsigned int params;
    unsigned int params_most;
    CliStep *step;
    unsigned int steps;
    unsigned int steps_most;
    CliArg *arg;
    unsigned int args;
    unsigned int args_most;
    unsigned int *list;
    unsigned int lists;
    unsigned int lists_most;
    char *text;
    unsigned int used;
    unsigned int text_most;
    int full;
    CliCellsTally *cells;
    unsigned int jobs_cap;
} CliCatalog;

// The values a job is run with, by param: each key, and its values, `count` of the list from `first`, all texts in the
// storage handed them
#define CLI_VALUES_KEYS 64u

#define CLI_VALUES_ITEMS 1024u

#define CLI_VALUES_TEXT (1u << 16u)

typedef struct
{
    char key[CLI_VALUES_KEYS][CLI_NAME_LONGEST];
    unsigned int first[CLI_VALUES_KEYS];
    unsigned int count[CLI_VALUES_KEYS];
    unsigned int keys;
    unsigned int item[CLI_VALUES_ITEMS];
    unsigned int items;
    char text[CLI_VALUES_TEXT];
    unsigned int used;
} CliValues;

// a catalog made empty whose storage is taken in cells counted to `cells`, each list doubled as it is put, `jobs_most`
// jobs at the most; and its cells given back, the catalog left empty and holding nothing
void cli_catalog_growing(CliCatalog *catalog, CliCellsTally *cells, unsigned int jobs_most);
void cli_catalog_given(CliCatalog *catalog);
// a catalog made empty over the storage handed it
void cli_catalog_begun(CliCatalog *catalog, CliJob *jobs, unsigned int jobs_most, CliParam *params,
                       unsigned int params_most, CliStep *steps, unsigned int steps_most, CliArg *args,
                       unsigned int args_most, unsigned int *lists, unsigned int lists_most, char *text,
                       unsigned int text_most);
// a text kept in the catalog: where it starts; and the text at a place, the empty text for none
unsigned int cli_catalog_text(CliCatalog *catalog, const char *text);
const char *cli_catalog_at(const CliCatalog *catalog, unsigned int text);
// a job begun with its id, group, title, file and about: its place, CLI_CATALOG_NONE where the catalog is full; a
// param put on the last job; a choice put on the last param; a step put on the last job; an argument put on the last
// step, its texts NULL past the last it holds; and a word put on the last argument's list
unsigned int cli_catalog_job(CliCatalog *catalog, const char *id, const char *group, const char *title,
                             const char *file, const char *about, const char *opens);
void cli_catalog_param(CliCatalog *catalog, const char *key, const char *kind, int required);
void cli_catalog_choice(CliCatalog *catalog, const char *choice);
void cli_catalog_step(CliCatalog *catalog, unsigned int program, const char *text, const char *tool_program,
                      int system);
void cli_catalog_arg(CliCatalog *catalog, unsigned int kind, const char *a, const char *b, const char *c, const char *d,
                     int separated);
void cli_catalog_word(CliCatalog *catalog, const char *word);
// the params of a free-words input, and of the build's trace, a choice of verbose or quiet, verbose the first
void cli_catalog_words(CliCatalog *catalog);
void cli_catalog_trace(CliCatalog *catalog);
// the job of `id`, CLI_CATALOG_NONE where none is; and the job at `index` of `from` copied whole into `to`: its place
// there, CLI_CATALOG_NONE where it does not fit
unsigned int cli_catalog_find(const CliCatalog *catalog, const char *id);
unsigned int cli_catalog_copy(const CliCatalog *from, unsigned int index, CliCatalog *to);
// The room a job copied takes: its params, steps, args and list entries, and the bytes of its texts
typedef struct
{
    unsigned int params;
    unsigned int steps;
    unsigned int args;
    unsigned int lists;
    unsigned long long text;
} CliCatalogExtent;
// the room the job at `index` of `from` takes copied into a catalog of its own
void cli_catalog_job_extent(const CliCatalog *from, unsigned int index, CliCatalogExtent *extent);
// the job at `index` written as the window reads it: `{"id", "group", "title", "file", "about", "params": [{"key",
// "kind", "choices", "default", "required"}], "opens"}`
void cli_catalog_job_written(const CliCatalog *catalog, unsigned int index, CliLine *out);

// the values made empty; `value` put after the values of `key`; the values of `key`, `count` of them into `count`,
// NULL where it has none; and the first value of `key`, NULL where it has none
void cli_values_begun(CliValues *values);
void cli_values_put(CliValues *values, const char *key, const char *value);
const unsigned int *cli_values_of(const CliValues *values, const char *key, unsigned int *count);
const char *cli_values_first(const CliValues *values, const char *key);
const char *cli_values_at(const CliValues *values, unsigned int text);

// The whole catalog read from the tree at `root` into `catalog`, in the order the engine's own steps run
// (catalog_read.c); and the opening paragraph a file gives of itself: a Python module's docstring, or the comment
// lines a script or a source file opens with, past its license lines, into `out` (catalog_scan.c)
void cli_catalog_read(const char *root, CliCatalog *catalog);
void cli_catalog_about(const char *path, CliLine *out);
// the jobs of the programs a tree's builds make, of deploying them, and of the query, put after the catalog's
// (executables_jobs.c, deploy.c, query_jobs.c)
void cli_executables_jobs(const char *root, CliCatalog *catalog);
void cli_deploy_jobs(const char *root, CliCatalog *catalog);
void cli_query_jobs(const char *root, CliCatalog *catalog);

#endif
