// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree.h: the tree: each Python and JavaScript file's facts, by its path in the tree, stored in a capacity of
// its own; what the tree shows across its files, an import that leads back round to the file that makes it and an
// attribute no class of its family sets; what each Python class's family sets; and the call hierarchy where no
// language server answers. One tree, under its own lock (inspect_tree.c, inspect_tree_*.c)
#ifndef INSPECT_TREE_H
#define INSPECT_TREE_H
#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "inspect.h"

typedef struct CliInspectTreePathCfg CliInspectTreePathCfg;
typedef struct CliInspectTreePathLanguageCfg CliInspectTreePathLanguageCfg;
typedef struct CliInspectTreePathLineCfg CliInspectTreePathLineCfg;
typedef struct CliInspectTreeCfg CliInspectTreeCfg;
typedef struct CliInspectTreeEachCfg CliInspectTreeEachCfg;
typedef struct CliInspectTreeContextEachSuppliedCfg CliInspectTreeContextEachSuppliedCfg;

// the facts of the file at `path` set from `size` bytes of its text in `language`, or the file taken out where the
// inspections do not read its language or its facts do not fit: 1 where it is set; a file taken out; every file taken
// out; and whether the tree contains a file
int cli_inspect_tree_set(const CliInspectTreePathLanguageCfg *args);
void cli_inspect_tree_remove(const CliInspectTreePathLineCfg *args);
void cli_inspect_tree_clear(void);
int cli_inspect_tree_contains(const CliInspectTreePathLineCfg *args);

// each file of the tree handed to `each`
void cli_inspect_tree_paths(const CliInspectTreeEachCfg *args);

// the function of the file at `path` that contains `line` most closely, and the one whose name starts on `line`, into
// `at`, its place among the file's functions: 1, or 0 where it has none
int cli_inspect_tree_function_at(const CliInspectTreePathLineCfg *args);
int cli_inspect_tree_function_on(const CliInspectTreePathLineCfg *args);

typedef struct
{
    void *context;
    const char *path;
    const CliFacts *facts;
    unsigned int function;
    const CliInspectSpan *sites;
    unsigned int count;
} CliInspectReachedCfg;

// A function the call hierarchy reaches: the path of its file, that file's facts and its place among their functions,
// and the spans of the calls that lead there, `count` of them
typedef void (*CliInspectReached)(const CliInspectReachedCfg *args);

// the function at `at` of the file at `path` handed to `each` with no calls, under the tree's lock: 1, or 0 where the
// tree has none there
int cli_inspect_tree_function(const CliInspectTreePathCfg *args);

// the functions the function at `at` of the file at `path` calls, each with the spans of its calls there; and the
// functions that call it, each with the spans of its calls of it; each handed to `each` under the tree's lock
void cli_inspect_tree_callees(const CliInspectTreePathCfg *args);
void cli_inspect_tree_callers(const CliInspectTreePathCfg *args);

// every finding in every file: each file's path handed to `each` with no finding first, then each of its findings
// written as the protocol's diagnostic, what the file shows alone and then what the tree shows
typedef void (*CliInspectFound)(const CliInspectFoundCfg *args);
void cli_inspect_tree_findings(const CliInspectTreeCfg *args);

// what each Python class's family sets, by its file and its name: what it, the classes it comes from, the classes that
// come from it and the classes those come from set, each attribute handed to `each`
typedef void (*CliInspectSupplied)(const CliInspectSuppliedCfg *args);
void cli_inspect_tree_supplied(const CliInspectTreeContextEachSuppliedCfg *args);

struct CliInspectTreePathCfg
{
    const char *path;
    void *context;
    unsigned int at;
    CliInspectReached each;
};

struct CliInspectTreePathLanguageCfg
{
    const char *path;
    const char *language;
    const char *text;
    unsigned long long size;
};

struct CliInspectTreePathLineCfg
{
    const char *path;
    unsigned int line;
    unsigned int *at_pointer;
};

struct CliInspectTreeCfg
{
    void *context;
    CliInspectFound each_inspect_found;
};

struct CliInspectTreeEachCfg
{
    void (*each_callback)(const CliEachPathCfg *args);
    void *context;
};

struct CliInspectTreeContextEachSuppliedCfg
{
    void *context;
    CliInspectSupplied each_inspect_supplied;
};

typedef struct
{
    int (*set)(const CliInspectTreePathLanguageCfg *args);
} CliInspectTreePathLanguageNs;
EMBED_TABLE_LAYOUT(CliInspectTreePathLanguageNs, set);
EMBED_TABLE_STORAGE CliInspectTreePathLanguageNs cli_inspect_tree_path_language EMBED_UNUSED = {
    .set = cli_inspect_tree_set,
};

typedef struct
{
    void (*remove)(const CliInspectTreePathLineCfg *args);
    int (*contains)(const CliInspectTreePathLineCfg *args);
    int (*function_at)(const CliInspectTreePathLineCfg *args);
    int (*function_on)(const CliInspectTreePathLineCfg *args);
} CliInspectTreePathLineNs;
EMBED_TABLE_LAYOUT(CliInspectTreePathLineNs, remove, contains, function_at, function_on);
EMBED_TABLE_STORAGE CliInspectTreePathLineNs cli_inspect_tree_path_line EMBED_UNUSED = {
    .remove = cli_inspect_tree_remove,
    .contains = cli_inspect_tree_contains,
    .function_at = cli_inspect_tree_function_at,
    .function_on = cli_inspect_tree_function_on,
};

typedef struct
{
    void (*paths)(const CliInspectTreeEachCfg *args);
} CliInspectTreeEachNs;
EMBED_TABLE_LAYOUT(CliInspectTreeEachNs, paths);
EMBED_TABLE_STORAGE CliInspectTreeEachNs cli_inspect_tree_each EMBED_UNUSED = {
    .paths = cli_inspect_tree_paths,
};

typedef struct
{
    int (*function)(const CliInspectTreePathCfg *args);
    void (*callees)(const CliInspectTreePathCfg *args);
    void (*callers)(const CliInspectTreePathCfg *args);
} CliInspectTreePathNs;
EMBED_TABLE_LAYOUT(CliInspectTreePathNs, function, callees, callers);
EMBED_TABLE_STORAGE CliInspectTreePathNs cli_inspect_tree_path EMBED_UNUSED = {
    .function = cli_inspect_tree_function,
    .callees = cli_inspect_tree_callees,
    .callers = cli_inspect_tree_callers,
};

typedef struct
{
    void (*findings)(const CliInspectTreeCfg *args);
} CliInspectTreeNs;
EMBED_TABLE_LAYOUT(CliInspectTreeNs, findings);
EMBED_TABLE_STORAGE CliInspectTreeNs cli_inspect_tree EMBED_UNUSED = {
    .findings = cli_inspect_tree_findings,
};

typedef struct
{
    void (*supplied)(const CliInspectTreeContextEachSuppliedCfg *args);
} CliInspectTreeContextEachSuppliedNs;
EMBED_TABLE_LAYOUT(CliInspectTreeContextEachSuppliedNs, supplied);
EMBED_TABLE_STORAGE CliInspectTreeContextEachSuppliedNs cli_inspect_tree_context_each_supplied EMBED_UNUSED = {
    .supplied = cli_inspect_tree_supplied,
};

#endif
