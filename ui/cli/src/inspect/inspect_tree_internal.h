// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_internal.h: what the tree's index's sources share: its files, each its path and its facts kept in a
// cell of its own, found by path through a table; its lock; and the paths of the tree's files by their folders and
// names
#ifndef INSPECT_TREE_INTERNAL_H
#define INSPECT_TREE_INTERNAL_H

#include "inspect_tree.h"

// the most files the index holds, and no file
#define CLI_TREE_FILES 65536u
#define CLI_TREE_NONE 0xffffffffu

// A file of the index: whether it holds one, whether its language is Python, its path, and its facts, both kept in
// `cell`, `size` bytes taken as the file is set and given back as it is taken out
typedef struct
{
    int used;
    int python;
    const char *path;
    unsigned int path_size;
    CliFacts facts;
    unsigned char *cell;
    unsigned long long size;
} CliTreeFile;

// the index's files in pages of CLI_TREE_PAGE, each page a cell taken as the first place in it is and given back as
// the index is cleared, how many places among them were ever taken, and its lock; and the file at `file_`
// (inspect_tree.c)
#define CLI_TREE_PAGE 256u
#define CLI_TREE_PAGES (CLI_TREE_FILES / CLI_TREE_PAGE)
extern CliTreeFile *g_cli_tree_page[CLI_TREE_PAGES];
extern unsigned int g_cli_tree_files;
extern CliLock g_cli_tree_lock;
#define CLI_TREE_FILE(file_) (g_cli_tree_page[(file_) / CLI_TREE_PAGE][(file_) % CLI_TREE_PAGE])

// the file of `size` bytes of path at `path`: its place, CLI_TREE_NONE where the index holds none; the same of a path
// written as `folder` and `name` joined by a slash, `folder` empty for none (inspect_tree.c)
unsigned int cli_tree_find(const char *path, unsigned long long size);
unsigned int cli_tree_find_joined(const char *folder, unsigned long long folder_size, const char *name);
// the length of the folder of `size` bytes of path, everything before its last slash, 0 where it has none
unsigned long long cli_tree_parent(const char *path, unsigned long long size);

// the file a Python module names, found from the file at `file`: its place, CLI_TREE_NONE where none is; and the
// files an import at `import` of the file at `file` runs, each handed to `each` (inspect_tree_imports.c)
unsigned int cli_tree_python_module(unsigned int file, const char *spec, unsigned long long size);
void cli_tree_targets(unsigned int file, unsigned int import, void (*each)(void *context, unsigned int target),
                      void *context);

// A finding the tree makes across its files: the file it is in, its span, its name and what it says
typedef void (*CliTreeSaid)(void *context, unsigned int file, CliInspectPlace from, CliInspectPlace to,
                            const char *code, const CliLine *message);

// each import that leads back round to the file that makes it, handed to `each` (inspect_tree_cycles.c)
void cli_tree_cycles(CliTreeSaid each, void *context);

// the families of the index's Python classes worked out again, in cells sized to them, where the index changed since
// they last were; those cells given back as the index changes; and each attribute a method reads of `self` that no
// class of its family sets, handed to `each` (inspect_tree_family.c, inspect_tree_bases.c)
void cli_tree_families(void);
void cli_tree_families_given(void);
void cli_tree_attributes(CliTreeSaid each, void *context);

// the class a Python file's base names, by a class of the file, a `from` import, or an import of the module it is read
// from, `depth` imports in: 1 with its file and its place among the file's classes, or 0 where it is outside the tree
// (inspect_tree_bases.c)
int cli_tree_base_of(unsigned int file, const char *base, unsigned long long size, unsigned int depth,
                     unsigned int *found_file, unsigned int *found_class);

#endif
