// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog.h: every job the app can start, read from the tree each time it is requested (catalog.c stores the jobs,
// catalog_read.c reads them from the tree, catalog_scan.c reads what a file declares of itself).
//
// Nothing here stores a list of sims, parts, programs, viewers or stages. Each is read from the file that defines it:
// the sims from the case in src/sims/run.sh, the driver's parts from its own usage text, the viewers and the stage
// scripts from where they sit. A sim added to run.sh is a job the next time the catalog is read, and a job whose file
// is gone is gone with it.
//
// A catalog stores its jobs, their params, steps and arguments, and every text they contain in the storage its holder
// hands it, so the whole catalog and a run's one job are each stored in a room of their own and nothing is allocated
#ifndef CATALOG_H
#define CATALOG_H
typedef struct CliCatalogJobsMostCfg CliCatalogJobsMostCfg;
typedef struct CliCatalogKindCfg CliCatalogKindCfg;
typedef struct CliCatalogIdGroupCfg CliCatalogIdGroupCfg;
typedef struct CliCatalogTextProgramCfg CliCatalogTextProgramCfg;
typedef struct CliCatalogKeyKindCfg CliCatalogKeyKindCfg;
typedef struct CliCatalogFromCfg CliCatalogFromCfg;
typedef struct CliCatalogFromIndexCfg CliCatalogFromIndexCfg;
typedef struct CliCatalogIndexCfg CliCatalogIndexCfg;
typedef struct CliCatalogCellsCfg CliCatalogCellsCfg;
typedef struct CliCatalogKeyCfg CliCatalogKeyCfg;
typedef struct CliCatalogKeyValuesCfg CliCatalogKeyValuesCfg;
typedef struct CliCatalogCfg CliCatalogCfg;
typedef struct CliCatalogTextCfg CliCatalogTextCfg;
typedef struct CliCatalogChoiceCfg CliCatalogChoiceCfg;
typedef struct CliCatalogIdCfg CliCatalogIdCfg;
typedef struct CliCatalogRootCfg CliCatalogRootCfg;
typedef struct CliCatalogWordCfg CliCatalogWordCfg;
typedef struct CliCatalogTextValuesCfg CliCatalogTextValuesCfg;

#include "cli_internal.h"
#include "embed_dispatch_layout.h"

// none: no text, no job
#define CLI_CATALOG_NONE 0xffffffffu

// how much of what a build does it prints as it runs: every command it runs, as it runs it, where verbose, the first
// choice and so the one a build takes where none is given, and what its tools print of their own, where quiet
#define CLI_TRACE "trace"
#define CLI_TRACE_VERBOSE "verbose"
#define CLI_TRACE_QUIET "quiet"

// How a step's argument list is made from the values given. Each kind names the texts it contains in turn:
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
//   UNPATH name       no argument: the step's PATH without the folders that contain the program named
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

// An argument: its kind, its texts as its kind names them, whether WORDS requires the separator, and the words WHEN
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

// One input a job takes: its key; its kind, how the app prompts for it: one of `choices`, several of them, a file, a
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

// A job: its id, group, title, file and what it declares of itself; its params and steps; and what it writes for a
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
// it contains and how many it may; 1 in `full` where one did not fit; and the count its cell is taken under, NULL where
// its storage was handed
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
    CellOwner *cells;
} CliCatalog;

// The values a job is run with, by param: each key, and its values, `count` of the list from `first`, all texts in the
// storage handed them
#define CLI_VALUES_KEYS 64u

#define CLI_VALUES_ITEMS 1024u

#define CLI_VALUES_TEXT (1u << 16u)

typedef struct
{
    char key[CLI_VALUES_KEYS][CLI_NAME_MAX_LEN];
    unsigned int first[CLI_VALUES_KEYS];
    unsigned int count[CLI_VALUES_KEYS];
    unsigned int keys;
    unsigned int item[CLI_VALUES_ITEMS];
    unsigned int items;
    char text[CLI_VALUES_TEXT];
    unsigned int used;
} CliValues;

// a catalog made empty whose lists and texts are taken at their most in one cell counted to `cells`, `jobs_most` jobs
// at the most; and its cell given back, the catalog left empty and containing nothing
void cli_catalog_take(const CliCatalogCellsCfg *args);
void cli_catalog_free(const CliCatalogChoiceCfg *args);
// a catalog made empty over the storage handed it
void cli_catalog_begin(const CliCatalogJobsMostCfg *args);
// a text stored in the catalog: where it starts; and the text at a place, the empty text for none
unsigned int cli_catalog_text(const CliCatalogTextProgramCfg *args);
const char *cli_catalog_at(const CliCatalogTextCfg *args);
// a job begun with its id, group, title, file and about: its place, CLI_CATALOG_NONE where the catalog is full; a
// param put on the last job; a choice put on the last param; a step put on the last job; an argument put on the last
// step, its texts NULL past the last it contains; and a word put on the last argument's list
unsigned int cli_catalog_job(const CliCatalogIdGroupCfg *args);
void cli_catalog_param(const CliCatalogKeyKindCfg *args);
void cli_catalog_choice(const CliCatalogChoiceCfg *args);
void cli_catalog_step(const CliCatalogTextProgramCfg *args);
void cli_catalog_arg(const CliCatalogKindCfg *args);
void cli_catalog_word(const CliCatalogWordCfg *args);
// the params of a free-words input, and of the build's trace, a choice of verbose or quiet, verbose the first
void cli_catalog_words(const CliCatalogChoiceCfg *args);
void cli_catalog_trace(const CliCatalogChoiceCfg *args);
// the job of `id`, CLI_CATALOG_NONE where none is; and the job at `index` of `from` copied whole into `to`: its place
// there, CLI_CATALOG_NONE where it does not fit
unsigned int cli_catalog_find(const CliCatalogIdCfg *args);
unsigned int cli_catalog_copy(const CliCatalogFromCfg *args);
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
void cli_catalog_job_extent(const CliCatalogFromIndexCfg *args);
// the job at `index` written as the window reads it: `{"id", "group", "title", "file", "about", "params": [{"key",
// "kind", "choices", "default", "required"}], "opens"}`
void cli_catalog_job_write(const CliCatalogIndexCfg *args);

// the values made empty; `value` put after the values of `key`; the values of `key`, `count` of them into `count`,
// NULL where it has none; and the first value of `key`, NULL where it has none
void cli_values_begin(const CliCatalogKeyValuesCfg *args);
void cli_values_put(const CliCatalogKeyValuesCfg *args);
const unsigned int *cli_values_get(const CliCatalogKeyCfg *args);
const char *cli_values_first(const CliCatalogKeyCfg *args);
const char *cli_values_at(const CliCatalogTextValuesCfg *args);

// The whole catalog read from the tree at `root` into `catalog`, in the order the engine's own steps run
// (catalog_read.c); and the opening paragraph a file gives of itself: a Python module's docstring, or the comment
// lines a script or a source file opens with, past its license lines, into `out` (catalog_scan.c)
void cli_catalog_read(const CliCatalogRootCfg *args);
void cli_catalog_about(const CliCatalogCfg *args);
// the jobs of the programs a tree's builds make, of deploying them, and of the query, put after the catalog's
// (executables_jobs.c, deploy.c, query_jobs.c)
void cli_executables_jobs(const CliCatalogRootCfg *args);
void cli_deploy_jobs(const CliCatalogRootCfg *args);
void cli_query_jobs(const CliCatalogRootCfg *args);

struct CliCatalogJobsMostCfg
{
    CliCatalog *catalog;
    unsigned int jobs_most;
    CliJob *jobs;
    CliParam *params;
    unsigned int params_most;
    CliStep *steps;
    unsigned int steps_most;
    CliArg *request;
    unsigned int args_most;
    unsigned int *lists;
    unsigned int lists_most;
    char *text_out;
    unsigned int text_most;
};

struct CliCatalogKindCfg
{
    CliCatalog *catalog;
    unsigned int kind_u32;
    const char *a;
    const char *b;
    const char *c;
    const char *d;
    int separated;
};

struct CliCatalogIdGroupCfg
{
    CliCatalog *catalog;
    const char *id;
    const char *group;
    const char *title;
    const char *file;
    const char *about;
    const char *opens;
};

struct CliCatalogTextProgramCfg
{
    CliCatalog *catalog;
    const char *text;
    unsigned int program;
    const char *tool_program;
    int system;
};

struct CliCatalogKeyKindCfg
{
    CliCatalog *catalog;
    const char *key;
    const char *kind;
    int required;
};

struct CliCatalogFromCfg
{
    const CliCatalog *from;
    unsigned int index;
    CliCatalog *to;
};

struct CliCatalogFromIndexCfg
{
    const CliCatalog *from;
    unsigned int index;
    CliCatalogExtent *extent;
};

struct CliCatalogIndexCfg
{
    const CliCatalog *catalog_read;
    unsigned int index;
    VerbaLinea *out;
};

struct CliCatalogCellsCfg
{
    CliCatalog *catalog;
    CellOwner *cells;
    unsigned int jobs_most;
};

struct CliCatalogKeyCfg
{
    const char *key;
    const CliValues *values;
    unsigned int *count;
};

struct CliCatalogKeyValuesCfg
{
    const char *key;
    CliValues *values_out;
    const char *value;
};

struct CliCatalogCfg
{
    VerbaLinea *out;
    const char *path;
};

struct CliCatalogTextCfg
{
    const CliCatalog *catalog_read;
    unsigned int text_u32;
};

struct CliCatalogChoiceCfg
{
    CliCatalog *catalog;
    const char *choice;
};

struct CliCatalogIdCfg
{
    const CliCatalog *catalog_read;
    const char *id;
};

struct CliCatalogRootCfg
{
    CliCatalog *catalog;
    const char *root;
};

struct CliCatalogWordCfg
{
    CliCatalog *catalog;
    const char *word;
};

struct CliCatalogTextValuesCfg
{
    unsigned int text_u32;
    const CliValues *values;
};

typedef struct
{
    void (*take)(const CliCatalogCellsCfg *args);
} CliCatalogCellsNs;
EMBED_TABLE_LAYOUT(CliCatalogCellsNs, take);
EMBED_TABLE_STORAGE CliCatalogCellsNs cli_catalog_cells EMBED_UNUSED = {
    .take = cli_catalog_take,
};

typedef struct
{
    void (*free)(const CliCatalogChoiceCfg *args);
    void (*choice)(const CliCatalogChoiceCfg *args);
    void (*words)(const CliCatalogChoiceCfg *args);
    void (*trace)(const CliCatalogChoiceCfg *args);
} CliCatalogChoiceNs;
EMBED_TABLE_LAYOUT(CliCatalogChoiceNs, free, choice, words, trace);
EMBED_TABLE_STORAGE CliCatalogChoiceNs cli_catalog_choice_table EMBED_UNUSED = {
    .free = cli_catalog_free,
    .choice = cli_catalog_choice,
    .words = cli_catalog_words,
    .trace = cli_catalog_trace,
};

typedef struct
{
    void (*begin)(const CliCatalogJobsMostCfg *args);
} CliCatalogJobsMostNs;
EMBED_TABLE_LAYOUT(CliCatalogJobsMostNs, begin);
EMBED_TABLE_STORAGE CliCatalogJobsMostNs cli_catalog_jobs_most EMBED_UNUSED = {
    .begin = cli_catalog_begin,
};

typedef struct
{
    unsigned int (*text)(const CliCatalogTextProgramCfg *args);
    void (*step)(const CliCatalogTextProgramCfg *args);
} CliCatalogTextProgramNs;
EMBED_TABLE_LAYOUT(CliCatalogTextProgramNs, text, step);
EMBED_TABLE_STORAGE CliCatalogTextProgramNs cli_catalog_text_program EMBED_UNUSED = {
    .text = cli_catalog_text,
    .step = cli_catalog_step,
};

typedef struct
{
    const char *(*at)(const CliCatalogTextCfg *args);
} CliCatalogTextNsTable;
EMBED_TABLE_LAYOUT(CliCatalogTextNsTable, at);
EMBED_TABLE_STORAGE CliCatalogTextNsTable cli_catalog_text_table EMBED_UNUSED = {
    .at = cli_catalog_at,
};

typedef struct
{
    unsigned int (*job)(const CliCatalogIdGroupCfg *args);
} CliCatalogIdGroupNs;
EMBED_TABLE_LAYOUT(CliCatalogIdGroupNs, job);
EMBED_TABLE_STORAGE CliCatalogIdGroupNs cli_catalog_id_group EMBED_UNUSED = {
    .job = cli_catalog_job,
};

typedef struct
{
    void (*param)(const CliCatalogKeyKindCfg *args);
} CliCatalogKeyKindNs;
EMBED_TABLE_LAYOUT(CliCatalogKeyKindNs, param);
EMBED_TABLE_STORAGE CliCatalogKeyKindNs cli_catalog_key_kind EMBED_UNUSED = {
    .param = cli_catalog_param,
};

typedef struct
{
    void (*arg)(const CliCatalogKindCfg *args);
} CliCatalogKindNs;
EMBED_TABLE_LAYOUT(CliCatalogKindNs, arg);
EMBED_TABLE_STORAGE CliCatalogKindNs cli_catalog_kind EMBED_UNUSED = {
    .arg = cli_catalog_arg,
};

typedef struct
{
    void (*word)(const CliCatalogWordCfg *args);
} CliCatalogWordNs;
EMBED_TABLE_LAYOUT(CliCatalogWordNs, word);
EMBED_TABLE_STORAGE CliCatalogWordNs cli_catalog_word_table EMBED_UNUSED = {
    .word = cli_catalog_word,
};

typedef struct
{
    unsigned int (*find)(const CliCatalogIdCfg *args);
} CliCatalogIdNs;
EMBED_TABLE_LAYOUT(CliCatalogIdNs, find);
EMBED_TABLE_STORAGE CliCatalogIdNs cli_catalog_id EMBED_UNUSED = {
    .find = cli_catalog_find,
};

typedef struct
{
    unsigned int (*copy)(const CliCatalogFromCfg *args);
} CliCatalogFromNs;
EMBED_TABLE_LAYOUT(CliCatalogFromNs, copy);
EMBED_TABLE_STORAGE CliCatalogFromNs cli_catalog_from EMBED_UNUSED = {
    .copy = cli_catalog_copy,
};

typedef struct
{
    void (*job_extent)(const CliCatalogFromIndexCfg *args);
} CliCatalogFromIndexNs;
EMBED_TABLE_LAYOUT(CliCatalogFromIndexNs, job_extent);
EMBED_TABLE_STORAGE CliCatalogFromIndexNs cli_catalog_from_index EMBED_UNUSED = {
    .job_extent = cli_catalog_job_extent,
};

typedef struct
{
    void (*job_write)(const CliCatalogIndexCfg *args);
} CliCatalogIndexNs;
EMBED_TABLE_LAYOUT(CliCatalogIndexNs, job_write);
EMBED_TABLE_STORAGE CliCatalogIndexNs cli_catalog_index EMBED_UNUSED = {
    .job_write = cli_catalog_job_write,
};

typedef struct
{
    void (*values_begin)(const CliCatalogKeyValuesCfg *args);
    void (*values_put)(const CliCatalogKeyValuesCfg *args);
} CliCatalogKeyValuesNs;
EMBED_TABLE_LAYOUT(CliCatalogKeyValuesNs, values_begin, values_put);
EMBED_TABLE_STORAGE CliCatalogKeyValuesNs cli_catalog_key_values EMBED_UNUSED = {
    .values_begin = cli_values_begin,
    .values_put = cli_values_put,
};

typedef struct
{
    const unsigned int *(*values_get)(const CliCatalogKeyCfg *args);
    const char *(*values_first)(const CliCatalogKeyCfg *args);
} CliCatalogKeyNs;
EMBED_TABLE_LAYOUT(CliCatalogKeyNs, values_get, values_first);
EMBED_TABLE_STORAGE CliCatalogKeyNs cli_catalog_key EMBED_UNUSED = {
    .values_get = cli_values_get,
    .values_first = cli_values_first,
};

typedef struct
{
    const char *(*values_at)(const CliCatalogTextValuesCfg *args);
} CliCatalogTextValuesNs;
EMBED_TABLE_LAYOUT(CliCatalogTextValuesNs, values_at);
EMBED_TABLE_STORAGE CliCatalogTextValuesNs cli_catalog_text_values EMBED_UNUSED = {
    .values_at = cli_values_at,
};

typedef struct
{
    void (*read)(const CliCatalogRootCfg *args);
    void (*executables_jobs)(const CliCatalogRootCfg *args);
    void (*deploy_jobs)(const CliCatalogRootCfg *args);
    void (*query_jobs)(const CliCatalogRootCfg *args);
} CliCatalogRootNs;
EMBED_TABLE_LAYOUT(CliCatalogRootNs, read, executables_jobs, deploy_jobs, query_jobs);
EMBED_TABLE_STORAGE CliCatalogRootNs cli_catalog_root EMBED_UNUSED = {
    .read = cli_catalog_read,
    .executables_jobs = cli_executables_jobs,
    .deploy_jobs = cli_deploy_jobs,
    .query_jobs = cli_query_jobs,
};

typedef struct
{
    void (*about)(const CliCatalogCfg *args);
} CliCatalogNs;
EMBED_TABLE_LAYOUT(CliCatalogNs, about);
EMBED_TABLE_STORAGE CliCatalogNs cli_catalog_table EMBED_UNUSED = {
    .about = cli_catalog_about,
};

#endif
