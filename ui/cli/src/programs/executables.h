// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables.h: the programs a tree's builds make: the jobs that build them, where each is found once it is made, how
// it is signed and run, and what a reader is reported of it (executables.c and the executables_*.c beside it). The
// build scripts are read from the tree's folders five deep, past the folders a build writes into or a package manager
// fills and those whose names start with a dot. A job's id is `executable/<script>:<name>`, the script by its path in
// the tree, and the program it made is read back from the id and the build's lines
#ifndef EXECUTABLES_H
#define EXECUTABLES_H
typedef struct CliExecutablesPathCfg CliExecutablesPathCfg;
typedef struct CliExecutablesRootCfg CliExecutablesRootCfg;
typedef struct CliExecutablesValuesCfg CliExecutablesValuesCfg;
typedef struct CliExecutablesErrMsgCfg CliExecutablesErrMsgCfg;
typedef struct CliExecutablesCfg CliExecutablesCfg;
typedef struct CliExecutablesKindCfg CliExecutablesKindCfg;
typedef struct CliExecutablesToolCfg CliExecutablesToolCfg;

#include "../command_line/callback_cfg.h"
#include "../runs/runner.h"
#include "embed_dispatch_layout.h"

// the program `name` of the toolchain `tool`, found where the toolchains window finds it, into `path`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_executables_program(const CliExecutablesToolCfg *args);
// the C compiler and linker for the system and processor the job's params choose, each variable and its value handed
// to `each`, where they are another machine's, for a build of the kind named, "go" or "rust"
void cli_executables_cross_env(const CliExecutablesKindCfg *args);
// whether this machine signs the programs its builds make
int cli_executables_signs(void);
// the program the job `id`'s build made, found from the lines its steps printed, into `path`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_executables_init(const CliExecutablesPathCfg *args);
// what a reader is reported after a build: what the build's lines report of it; how the program compares with the one
// built at the tree's last commit; what a signing reported; what a run of the program reported; and what a build that
// failed reported; each line handed to `sink`
void cli_executables_built_advice(const CliExecutablesRootCfg *args);
void cli_executables_compare_build(const CliExecutablesCfg *args);
void cli_executables_signature_print(const CliExecutablesCfg *args);
void cli_executables_run_result_advice(const CliExecutablesRootCfg *args);
void cli_executables_advice(const CliExecutablesRootCfg *args);
// the commands that sign the program `file`, each into `commands` with its line into `displayed`, sized for `capacity`:
// their count, or -1 with why given
int cli_executables_sign_steps(const CliExecutablesErrMsgCfg *args);
// the command that runs the program `file` the build made, with the PATH a program opened on its own gets where
// `system`, into `command`, with its line into `displayed`
void cli_executables_run_command(const CliExecutablesValuesCfg *args);

struct CliExecutablesPathCfg
{
    char *path;
    VerbaLinea *err_msg;
    const CliValues *values;
    const char *root;
    const char *id;
    const CliLines *lines;
};

struct CliExecutablesRootCfg
{
    const char *root;
    const char *id;
    const CliLines *lines;
    QuerySink sink;
    int has_code;
    long long code;
};

struct CliExecutablesValuesCfg
{
    const CliValues *values;
    const char *root;
    const char *file;
    int system;
    CliCommand *command;
    VerbaLinea *displayed_verba_linea_pointer;
};

struct CliExecutablesErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *file;
    CliCommand *commands;
    char (*displayed)[CLI_PATH_MAX_LEN * 4u];
    unsigned int capacity;
};

struct CliExecutablesCfg
{
    const char *root;
    const char *id;
    QuerySink sink;
    const char *file;
};

struct CliExecutablesKindCfg
{
    const char *kind;
    const CliValues *values;
    void (*each)(const CliEachNameValueCfg *args);
    void *context;
};

struct CliExecutablesToolCfg
{
    const char *tool;
    const char *name;
    char *path;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*program)(const CliExecutablesToolCfg *args);
} CliExecutablesToolNs;
EMBED_TABLE_LAYOUT(CliExecutablesToolNs, program);
EMBED_TABLE_STORAGE CliExecutablesToolNs cli_executables_tool EMBED_UNUSED = {
    .program = cli_executables_program,
};

typedef struct
{
    void (*cross_env)(const CliExecutablesKindCfg *args);
} CliExecutablesKindNs;
EMBED_TABLE_LAYOUT(CliExecutablesKindNs, cross_env);
EMBED_TABLE_STORAGE CliExecutablesKindNs cli_executables_kind EMBED_UNUSED = {
    .cross_env = cli_executables_cross_env,
};

typedef struct
{
    int (*init)(const CliExecutablesPathCfg *args);
} CliExecutablesPathNs;
EMBED_TABLE_LAYOUT(CliExecutablesPathNs, init);
EMBED_TABLE_STORAGE CliExecutablesPathNs cli_executables_path EMBED_UNUSED = {
    .init = cli_executables_init,
};

typedef struct
{
    void (*built_advice)(const CliExecutablesRootCfg *args);
    void (*run_result_advice)(const CliExecutablesRootCfg *args);
    void (*advice)(const CliExecutablesRootCfg *args);
} CliExecutablesRootNs;
EMBED_TABLE_LAYOUT(CliExecutablesRootNs, built_advice, run_result_advice, advice);
EMBED_TABLE_STORAGE CliExecutablesRootNs cli_executables_root EMBED_UNUSED = {
    .built_advice = cli_executables_built_advice,
    .run_result_advice = cli_executables_run_result_advice,
    .advice = cli_executables_advice,
};

typedef struct
{
    void (*compare_build)(const CliExecutablesCfg *args);
    void (*signature_print)(const CliExecutablesCfg *args);
} CliExecutablesNs;
EMBED_TABLE_LAYOUT(CliExecutablesNs, compare_build, signature_print);
EMBED_TABLE_STORAGE CliExecutablesNs cli_executables_table EMBED_UNUSED = {
    .compare_build = cli_executables_compare_build,
    .signature_print = cli_executables_signature_print,
};

typedef struct
{
    int (*sign_steps)(const CliExecutablesErrMsgCfg *args);
} CliExecutablesErrMsgNs;
EMBED_TABLE_LAYOUT(CliExecutablesErrMsgNs, sign_steps);
EMBED_TABLE_STORAGE CliExecutablesErrMsgNs cli_executables_err_msg EMBED_UNUSED = {
    .sign_steps = cli_executables_sign_steps,
};

typedef struct
{
    void (*run_command)(const CliExecutablesValuesCfg *args);
} CliExecutablesValuesNs;
EMBED_TABLE_LAYOUT(CliExecutablesValuesNs, run_command);
EMBED_TABLE_STORAGE CliExecutablesValuesNs cli_executables_values EMBED_UNUSED = {
    .run_command = cli_executables_run_command,
};

#endif
