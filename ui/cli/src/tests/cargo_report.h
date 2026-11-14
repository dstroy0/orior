// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_report.h: what cargo test and the test harness write, read: a result's line, each failure's message and the
// place it panicked, a build's first error and the place it names, and the folder a package's paths are written from
// (cargo_report.c)
#ifndef CARGO_REPORT_H
#define CARGO_REPORT_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliCargoReportSizeCfg CliCargoReportSizeCfg;
typedef struct CliCargoReportSizeTextCfg CliCargoReportSizeTextCfg;
typedef struct CliCargoReportLineCfg CliCargoReportLineCfg;
typedef struct CliCargoReportSizeOutCfg CliCargoReportSizeOutCfg;
typedef struct CliCargoReportSizeTextPathCfg CliCargoReportSizeTextPathCfg;
typedef struct CliCargoReportCfg CliCargoReportCfg;

// a line of the harness's own, `test name ... ok`: the test's name as its start and size in the line, and `passed`,
// `failed` or `skipped`: 1, or 0 where the line is no result
int cli_cargo_result_line(const CliCargoReportLineCfg *args);

// each failed test's message and the place it panicked, a path as the compiler wrote it, NULL for none, and a line
// counted from 1, from the harness's report of its failures, passed to `each`, a test failing twice reported by its
// last
void cli_cargo_failures_each(const CliCargoReportSizeOutCfg *args);

// a path and a line from `path:line:column` into `path`, sized for CLI_PATH_MAX_LEN, with forward slashes, and
// `line`: 1, or 0 where the text is none
int cli_cargo_position_get(const CliCargoReportSizeTextPathCfg *args);

// the compiler's first error in what a build printed into `error`, sized for `capacity`, its size into `length`, and
// the place it names into `path` and `line`, `path` empty where it names none: 1, or 0 where it printed no error; and
// the last line of a text that is not blank, trimmed, or `none` where every line is, the same way
int cli_cargo_build_error(const CliCargoReportSizeCfg *args);
void cli_cargo_last_line(const CliCargoReportSizeTextCfg *args);

// the folder whose paths a package's panics and errors are written from into `workspace`, is sized for
// CLI_PATH_MAX_LEN: the workspace's, where the package is one of a workspace in the tree, or the package's own
void cli_cargo_workspace_get(const CliCargoReportCfg *args);

struct CliCargoReportSizeCfg
{
    unsigned long long size;
    char *path;
    long long *line;
    const char *output;
    char *error;
    unsigned long long capacity;
    unsigned long long *length;
};

struct CliCargoReportSizeTextCfg
{
    unsigned long long size;
    const char *text;
    unsigned long long capacity;
    unsigned long long *length;
    const char *none;
    char *out_out;
};

struct CliCargoReportLineCfg
{
    const char *line_text;
    unsigned long long size;
    unsigned long long *name;
    unsigned long long *name_size;
    const char **outcome;
};

struct CliCargoReportSizeOutCfg
{
    unsigned long long size;
    const char *out;
    void (*each)(const CliEachNameMessagePathLineCfg *args);
    void *context;
};

struct CliCargoReportSizeTextPathCfg
{
    unsigned long long size;
    const char *text;
    char *path;
    long long *line;
};

struct CliCargoReportCfg
{
    const char *root;
    const char *package;
    char *workspace;
};

typedef struct
{
    int (*cargo_result_line)(const CliCargoReportLineCfg *args);
} CliCargoReportLineNs;
EMBED_TABLE_LAYOUT(CliCargoReportLineNs, cargo_result_line);
EMBED_TABLE_STORAGE CliCargoReportLineNs cli_cargo_report_line EMBED_UNUSED = {
    .cargo_result_line = cli_cargo_result_line,
};

typedef struct
{
    void (*cargo_failures_each)(const CliCargoReportSizeOutCfg *args);
} CliCargoReportSizeOutNs;
EMBED_TABLE_LAYOUT(CliCargoReportSizeOutNs, cargo_failures_each);
EMBED_TABLE_STORAGE CliCargoReportSizeOutNs cli_cargo_report_size_out EMBED_UNUSED = {
    .cargo_failures_each = cli_cargo_failures_each,
};

typedef struct
{
    int (*cargo_position_get)(const CliCargoReportSizeTextPathCfg *args);
} CliCargoReportSizeTextPathNs;
EMBED_TABLE_LAYOUT(CliCargoReportSizeTextPathNs, cargo_position_get);
EMBED_TABLE_STORAGE CliCargoReportSizeTextPathNs cli_cargo_report_size_text_path EMBED_UNUSED = {
    .cargo_position_get = cli_cargo_position_get,
};

typedef struct
{
    int (*cargo_build_error)(const CliCargoReportSizeCfg *args);
} CliCargoReportSizeNs;
EMBED_TABLE_LAYOUT(CliCargoReportSizeNs, cargo_build_error);
EMBED_TABLE_STORAGE CliCargoReportSizeNs cli_cargo_report_size EMBED_UNUSED = {
    .cargo_build_error = cli_cargo_build_error,
};

typedef struct
{
    void (*cargo_last_line)(const CliCargoReportSizeTextCfg *args);
} CliCargoReportSizeTextNs;
EMBED_TABLE_LAYOUT(CliCargoReportSizeTextNs, cargo_last_line);
EMBED_TABLE_STORAGE CliCargoReportSizeTextNs cli_cargo_report_size_text EMBED_UNUSED = {
    .cargo_last_line = cli_cargo_last_line,
};

typedef struct
{
    void (*cargo_workspace_get)(const CliCargoReportCfg *args);
} CliCargoReportNs;
EMBED_TABLE_LAYOUT(CliCargoReportNs, cargo_workspace_get);
EMBED_TABLE_STORAGE CliCargoReportNs cli_cargo_report EMBED_UNUSED = {
    .cargo_workspace_get = cli_cargo_workspace_get,
};

#endif
