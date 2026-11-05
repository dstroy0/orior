// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_internal.h: what catalog_read.c and catalog_scan.c share: a folder's entries in order, and what a file
// declares
#ifndef CATALOG_INTERNAL_H
#define CATALOG_INTERNAL_H
#include "callback_cfg.h"
#include "catalog.h"
#include "cells.h"
#include "embed_dispatch_layout.h"

typedef struct CliCatalogPathCtx CliCatalogPathCtx;
typedef struct CliCatalogFilterCtx CliCatalogFilterCtx;
typedef struct CliCatalogTextVariableCtx CliCatalogTextVariableCtx;
typedef struct CliCatalogFolderCtx CliCatalogFolderCtx;
typedef struct CliCatalogCapacityCtx CliCatalogCapacityCtx;
typedef struct CliCatalogFolderNameCtx CliCatalogFolderNameCtx;
typedef struct CliCatalogNamesCtx CliCatalogNamesCtx;
typedef struct CliCatalogMaxCtx CliCatalogMaxCtx;
typedef struct CliCatalogOutCtx CliCatalogOutCtx;
typedef struct CliCatalogPathOutCtx CliCatalogPathOutCtx;
typedef struct CliCatalogCtx CliCatalogCtx;

// the cells the catalog occupies while it is read (catalog_lists.c, as are the lists and the givings back below)
extern CellOwner g_cli_catalog_cells;

// The entries of a folder in order, or a list of names: each name, `count` of them, in a cell of `capacity`
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliEntries;

// A list of paths in a cell of `capacity`
typedef struct
{
    char (*path)[CLI_PATH_MAX_LEN];
    unsigned int capacity;
} CliPaths;

// the files of `folder` whose names pass `filter`, or its folders where `filter` is NULL and `folders` is 1, into
// `entries`, sorted by name, in a cell the size of the folder's count of them, the cell `entries` occupied given back
// first
void cli_catalog_entries(const CliCatalogFolderCtx *args);
// a list's cell taken at the most a list holds as it is first made capacity in: 1 where `most` fit, or 0 where they do
// not or the cell is refused
int cli_catalog_names_capacity(const CliCatalogNamesCtx *args);
int cli_catalog_paths_capacity(const CliCatalogMaxCtx *args);
// a list's cell given back, the list left empty
// what a file declares of itself, as cli_catalog_about reads it, and the first paragraph of a README that is prose and
// not a heading, each in a cell the size of the lines it is read from: NULL where the file could not be read or the
// cell is refused; and a text the catalog took given back
char *cli_catalog_about_alloc(const CliCatalogPathCtx *args);
char *cli_catalog_markdown_alloc(const CliCatalogPathOutCtx *args);
// `name` joined onto `folder` into `path`, sized for CLI_PATH_MAX_LEN
void cli_catalog_join(const CliCatalogFolderNameCtx *args);
// the names a `case` in a script accepts on the line after `case "$<variable>" in`, each into `names`, sized for
// `capacity`: their count; or, where `names` is NULL, the count of names it gives, the most it would put
unsigned int cli_catalog_case_names(const CliCatalogTextVariableCtx *args);
// the parts the track driver runs, read from its usage text: the list after "in the order given:" across however many
// string literals the source splits it into, each into `parts`, sized for `capacity`: their count; or, where `parts` is
// NULL, the count of parts it gives, the most it would put
unsigned int cli_catalog_driver_parts(const CliCatalogCapacityCtx *args);
// a path's name without its extension into `out`, sized for CLI_NAME_MAX_LEN
void cli_catalog_stem(const CliCatalogPathOutCtx *args);
// a path's folder and name, by which two scripts of one name differ in the job list, into `out`, which is sized for
// CLI_PATH_MAX_LEN (catalog_read.c)
void cli_catalog_short(const CliCatalogOutCtx *args);

// a job of a script, its id the group and its file, its title `title` or its file; a bash script that takes free
// words as its job, traced where it is a build; a script's text in a cell of its size and a 0 after, given back by
// persistent_buf_release, NULL where it could not be read or the cell is refused; the files of a folder whose names
// pass `filter`, each by its path in the tree, into `list` from `at`, its capacity made for them: the count after; the
// build scripts; and the sims (catalog_read.c)
void cli_catalog_script_job(const CliCatalogPathCtx *args);
void cli_catalog_bash_alone(const CliCatalogPathCtx *args);
char *cli_catalog_script_alloc(const CliCatalogPathOutCtx *args);
unsigned int cli_catalog_paths(const CliCatalogFilterCtx *args);
void cli_catalog_builds(const CliCatalogPathCtx *args);
void cli_catalog_sims(const CliCatalogPathCtx *args);
// whether a name is a cfg's, a qasm circuit's, a Python script's, a view builder's, or a figure builder's
int cli_catalog_is_cfg(const CliNameCfg *args);
int cli_catalog_is_qasm(const CliNameCfg *args);
int cli_catalog_is_py(const CliNameCfg *args);
int cli_catalog_is_builder(const CliNameCfg *args);
int cli_catalog_is_figure(const CliNameCfg *args);

struct CliCatalogPathCtx
{
    const char *path;
    CliCatalog *catalog;
    const char *root;
    const char *group;
    const char *title;
    const char *opens;
};

struct CliCatalogFilterCtx
{
    int (*filter)(const CliNameCfg *args);
    const char *root;
    const char *dir;
    CliPaths *list;
    unsigned int at;
};

struct CliCatalogTextVariableCtx
{
    const char *text_read;
    const char *variable;
    char (*names_callback)[CLI_NAME_MAX_LEN];
    unsigned int capacity;
};

struct CliCatalogFolderCtx
{
    const char *folder;
    int folders;
    int (*filter)(const CliNameCfg *args);
    CliEntries *entries;
};

struct CliCatalogCapacityCtx
{
    unsigned int capacity;
    const char *source;
    char (*parts)[CLI_NAME_MAX_LEN];
};

struct CliCatalogFolderNameCtx
{
    const char *folder;
    const char *name;
    char *path_out;
};

struct CliCatalogNamesCtx
{
    CliEntries *names;
    unsigned int most;
};

struct CliCatalogMaxCtx
{
    unsigned int most;
    CliPaths *paths;
};

struct CliCatalogOutCtx
{
    char *out;
    const char *file;
};

struct CliCatalogPathOutCtx
{
    const char *path;
    char *out;
};

struct CliCatalogCtx
{
    char *text;
};

typedef struct
{
    void (*entries)(const CliCatalogFolderCtx *args);
} CliCatalogFolderNs;
EMBED_TABLE_LAYOUT(CliCatalogFolderNs, entries);
EMBED_TABLE_STORAGE CliCatalogFolderNs cli_catalog_folder_internal EMBED_UNUSED = {
    .entries = cli_catalog_entries,
};

typedef struct
{
    int (*names_capacity)(const CliCatalogNamesCtx *args);
} CliCatalogNamesNs;
EMBED_TABLE_LAYOUT(CliCatalogNamesNs, names_capacity);
EMBED_TABLE_STORAGE CliCatalogNamesNs cli_catalog_names_internal EMBED_UNUSED = {
    .names_capacity = cli_catalog_names_capacity,
};

typedef struct
{
    int (*paths_capacity)(const CliCatalogMaxCtx *args);
} CliCatalogMaxNs;
EMBED_TABLE_LAYOUT(CliCatalogMaxNs, paths_capacity);
EMBED_TABLE_STORAGE CliCatalogMaxNs cli_catalog_max_internal EMBED_UNUSED = {
    .paths_capacity = cli_catalog_paths_capacity,
};

typedef struct
{
    char *(*about_alloc)(const CliCatalogPathCtx *args);
    void (*script_job)(const CliCatalogPathCtx *args);
    void (*bash_alone)(const CliCatalogPathCtx *args);
    void (*builds)(const CliCatalogPathCtx *args);
    void (*sims)(const CliCatalogPathCtx *args);
} CliCatalogPathNs;
EMBED_TABLE_LAYOUT(CliCatalogPathNs, about_alloc, script_job, bash_alone, builds, sims);
EMBED_TABLE_STORAGE CliCatalogPathNs cli_catalog_path_internal EMBED_UNUSED = {
    .about_alloc = cli_catalog_about_alloc,
    .script_job = cli_catalog_script_job,
    .bash_alone = cli_catalog_bash_alone,
    .builds = cli_catalog_builds,
    .sims = cli_catalog_sims,
};

typedef struct
{
    char *(*markdown_alloc)(const CliCatalogPathOutCtx *args);
    void (*stem)(const CliCatalogPathOutCtx *args);
    char *(*script_alloc)(const CliCatalogPathOutCtx *args);
} CliCatalogPathOutNs;
EMBED_TABLE_LAYOUT(CliCatalogPathOutNs, markdown_alloc, stem, script_alloc);
EMBED_TABLE_STORAGE CliCatalogPathOutNs cli_catalog_path_out_internal EMBED_UNUSED = {
    .markdown_alloc = cli_catalog_markdown_alloc,
    .stem = cli_catalog_stem,
    .script_alloc = cli_catalog_script_alloc,
};

typedef struct
{
    void (*join)(const CliCatalogFolderNameCtx *args);
} CliCatalogFolderNameNs;
EMBED_TABLE_LAYOUT(CliCatalogFolderNameNs, join);
EMBED_TABLE_STORAGE CliCatalogFolderNameNs cli_catalog_folder_name_internal EMBED_UNUSED = {
    .join = cli_catalog_join,
};

typedef struct
{
    unsigned int (*case_names)(const CliCatalogTextVariableCtx *args);
} CliCatalogTextVariableNs;
EMBED_TABLE_LAYOUT(CliCatalogTextVariableNs, case_names);
EMBED_TABLE_STORAGE CliCatalogTextVariableNs cli_catalog_text_variable_internal EMBED_UNUSED = {
    .case_names = cli_catalog_case_names,
};

typedef struct
{
    unsigned int (*driver_parts)(const CliCatalogCapacityCtx *args);
} CliCatalogCapacityNs;
EMBED_TABLE_LAYOUT(CliCatalogCapacityNs, driver_parts);
EMBED_TABLE_STORAGE CliCatalogCapacityNs cli_catalog_capacity_internal EMBED_UNUSED = {
    .driver_parts = cli_catalog_driver_parts,
};

typedef struct
{
    void (*catalog_short)(const CliCatalogOutCtx *args);
} CliCatalogOutNsTable;
EMBED_TABLE_LAYOUT(CliCatalogOutNsTable, catalog_short);
EMBED_TABLE_STORAGE CliCatalogOutNsTable cli_catalog_out_internal EMBED_UNUSED = {
    .catalog_short = cli_catalog_short,
};

typedef struct
{
    unsigned int (*paths)(const CliCatalogFilterCtx *args);
} CliCatalogFilterNs;
EMBED_TABLE_LAYOUT(CliCatalogFilterNs, paths);
EMBED_TABLE_STORAGE CliCatalogFilterNs cli_catalog_filter_internal EMBED_UNUSED = {
    .paths = cli_catalog_paths,
};

typedef struct
{
    int (*is_cfg)(const CliNameCfg *args);
    int (*is_qasm)(const CliNameCfg *args);
    int (*is_py)(const CliNameCfg *args);
    int (*is_builder)(const CliNameCfg *args);
    int (*is_figure)(const CliNameCfg *args);
} CliNameNs;
EMBED_TABLE_LAYOUT(CliNameNs, is_cfg, is_qasm, is_py, is_builder, is_figure);
EMBED_TABLE_STORAGE CliNameNs cli_name_table EMBED_UNUSED = {
    .is_cfg = cli_catalog_is_cfg,
    .is_qasm = cli_catalog_is_qasm,
    .is_py = cli_catalog_is_py,
    .is_builder = cli_catalog_is_builder,
    .is_figure = cli_catalog_is_figure,
};

#endif
