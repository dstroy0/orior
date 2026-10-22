// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_internal.h: what catalog_read.c and catalog_scan.c share: a folder's entries in order, and what a file says
#ifndef CATALOG_INTERNAL_H
#define CATALOG_INTERNAL_H

#include "catalog.h"
#include "cells.h"

// the cells the catalog holds while it is read (catalog_lists.c, as are the lists and the givings back below)
extern CliCellsTally g_cli_catalog_cells;

// The entries of a folder in order, or a list of names: each name, `count` of them, in a cell of `room`
typedef struct
{
    char (*name)[CLI_NAME_LONGEST];
    unsigned int count;
    unsigned int room;
} CliEntries;

// A list of paths in a cell of `room`
typedef struct
{
    char (*path)[CLI_PATH_LONGEST];
    unsigned int room;
} CliPaths;

// the files of `folder` whose names pass `keep`, or its folders where `keep` is NULL and `folders` is 1, into
// `entries`, sorted by name, in a cell the size of the folder's count of them, the cell `entries` held given back first
void cli_catalog_entries(const char *folder, int folders, int (*keep)(const char *name), CliEntries *entries);
// room for `most` in a list, what it holds kept: twice its room where that is more, so a list put one at a time is
// taken again seldom; 1, or 0 where the cell is refused and the list is left as it was
int cli_catalog_names_room(CliEntries *names, unsigned int most);
int cli_catalog_paths_room(CliPaths *paths, unsigned int most);
// a list's cell given back, the list left empty
void cli_catalog_entries_given(CliEntries *entries);
void cli_catalog_paths_given(CliPaths *paths);
// what a file says of itself, as cli_catalog_about says it, and the first paragraph of a README that is prose and not
// a heading, each in a cell the size of the lines it is read from: NULL where the file could not be read or the cell
// is refused; and a text the catalog took given back
char *cli_catalog_about_taken(const char *path);
char *cli_catalog_markdown_taken(const char *path);
void cli_catalog_text_given(char *text);
// `name` joined onto `folder` into `path`, which holds CLI_PATH_LONGEST
void cli_catalog_join(const char *folder, const char *name, char *path);
// the names a `case` in a script accepts on the line after `case "$<variable>" in`, each into `names`, which holds
// `room`: their count; or, where `names` is NULL, the count of names it gives, the most it would put
unsigned int cli_catalog_case_names(const char *text, const char *variable, char (*names)[CLI_NAME_LONGEST],
                                    unsigned int room);
// the parts the track driver runs, read from its usage text: the list after "in the order given:" across however many
// string literals the source splits it into, each into `parts`, which holds `room`: their count; or, where `parts` is
// NULL, the count of parts it gives, the most it would put
unsigned int cli_catalog_driver_parts(const char *source, char (*parts)[CLI_NAME_LONGEST], unsigned int room);
// a path's name without its extension into `out`, which holds CLI_NAME_LONGEST
void cli_catalog_stem(const char *path, char *out);
// a path's folder and name, by which two scripts of one name differ in the job list, into `out`, which holds
// CLI_PATH_LONGEST (catalog_read.c)
void cli_catalog_short(const char *file, char *out);

// a job of a script, its id the group and its file, its title `title` or its file; a bash script that takes free
// words as its job, traced where it is a build; a script's text in a cell of its size and a 0 after, given back by
// cli_catalog_text_given, NULL where it could not be read or the cell is refused; the files of a folder whose names
// pass `keep`, each by its path in the tree, into `list` from `at`, its room made for them: the count after; the build
// scripts; and the sims (catalog_read.c)
void cli_catalog_script_job(CliCatalog *catalog, const char *root, const char *group, const char *path,
                            const char *title, const char *opens);
void cli_catalog_bash_alone(CliCatalog *catalog, const char *root, const char *group, const char *path);
char *cli_catalog_script_taken(const char *path);
unsigned int cli_catalog_paths(const char *root, const char *dir, int (*keep)(const char *name), CliPaths *list,
                               unsigned int at);
void cli_catalog_builds(const char *root, CliCatalog *catalog);
void cli_catalog_sims(const char *root, CliCatalog *catalog);
// whether a name is a cfg's, a qasm circuit's, a Python script's, a view builder's, or a figure builder's
int cli_catalog_is_cfg(const char *name);
int cli_catalog_is_qasm(const char *name);
int cli_catalog_is_py(const char *name);
int cli_catalog_is_builder(const char *name);
int cli_catalog_is_figure(const char *name);

#endif
