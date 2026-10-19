// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_internal.h: what catalog_read.c and catalog_scan.c share: a folder's entries in order, and what a file says
#ifndef CATALOG_INTERNAL_H
#define CATALOG_INTERNAL_H

#include "catalog.h"

// the most entries of a folder read in order
#define CLI_ENTRIES_MOST 4096u

// The entries of a folder in order: each name, `count` of them
typedef struct
{
    char name[CLI_ENTRIES_MOST][CLI_NAME_LONGEST];
    unsigned int count;
} CliEntries;

// the files of `folder` whose names pass `keep`, or its folders where `keep` is NULL and `folders` is 1, into
// `entries`, sorted by name
void cli_catalog_entries(const char *folder, int folders, int (*keep)(const char *name), CliEntries *entries);
// `name` joined onto `folder` into `path`, which holds CLI_PATH_LONGEST
void cli_catalog_join(const char *folder, const char *name, char *path);
// the names a `case` in a script accepts on the line after `case "$<variable>" in`, each into `names`, which holds
// `room`: their count
unsigned int cli_catalog_case_names(const char *text, const char *variable, char (*names)[CLI_NAME_LONGEST],
                                    unsigned int room);
// the parts the track driver runs, read from its usage text: the list after "in the order given:" across however many
// string literals the source splits it into, each into `parts`, which holds `room`: their count
unsigned int cli_catalog_driver_parts(const char *source, char (*parts)[CLI_NAME_LONGEST], unsigned int room);
// the first paragraph of a README that is prose and not a heading, into `out`
void cli_catalog_markdown_opening(const char *path, CliLine *out);
// a path's name without its extension into `out`, which holds CLI_NAME_LONGEST
void cli_catalog_stem(const char *path, char *out);
// a path's folder and name, by which two scripts of one name differ in the job list, into `out`, which holds
// CLI_PATH_LONGEST (catalog_read.c)
void cli_catalog_short(const char *file, char *out);

// the most texts a list of the catalog holds
#define CLI_LISTED_MOST 1024u

// a job of a script, its id the group and its file, its title `title` or its file; a bash script that takes free
// words as its job, traced where it is a build; a script read into the one text, NULL where it could not be; the
// files of a folder whose names pass `keep`, each by its path in the tree, into `list` from `at`, which holds `room`:
// the count after; the build scripts; and the sims (catalog_read.c)
void cli_catalog_script_job(CliCatalog *catalog, const char *root, const char *group, const char *path,
                            const char *title, const char *opens);
void cli_catalog_bash_alone(CliCatalog *catalog, const char *root, const char *group, const char *path);
const char *cli_catalog_script_read(const char *path);
unsigned int cli_catalog_paths(const char *root, const char *dir, int (*keep)(const char *name),
                               char (*list)[CLI_PATH_LONGEST], unsigned int at, unsigned int room);
void cli_catalog_builds(const char *root, CliCatalog *catalog);
void cli_catalog_sims(const char *root, CliCatalog *catalog);
// whether a name is a cfg's, a qasm circuit's, a Python script's, a view builder's, or a figure builder's
int cli_catalog_is_cfg(const char *name);
int cli_catalog_is_qasm(const char *name);
int cli_catalog_is_py(const char *name);
int cli_catalog_is_builder(const char *name);
int cli_catalog_is_figure(const char *name);

#endif
