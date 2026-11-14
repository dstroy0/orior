// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_internal.h: what the tree's sources share: its files, each its path and its facts stored in a
// cell of its own, found by path through a table; its lock; and the paths of the tree's files by their folders and
// names
#ifndef INSPECT_TREE_INTERNAL_H
#define INSPECT_TREE_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "inspect_tree.h"

typedef struct CliInspectTreeSizeCtx CliInspectTreeSizeCtx;
typedef struct CliInspectTreeFileCtx CliInspectTreeFileCtx;
typedef struct CliInspectTreeFolderCtx CliInspectTreeFolderCtx;
typedef struct CliInspectTreeSizeFileCtx CliInspectTreeSizeFileCtx;
typedef struct CliInspectTreeCtx CliInspectTreeCtx;
typedef struct CliInspectTreePathCtx CliInspectTreePathCtx;

// the max files the tree contains, and no file
#define CLI_TREE_FILES 65536u
#define CLI_TREE_NONE 0xffffffffu

// A file of the tree: whether it contains one, whether its language is Python, the length of its path and the bucket
// its path hashes to, and its facts. The path is at the start of the facts' movable cell, a 0 after it, and the
// facts' lists after the path; the cell is taken as the file is set and given back as it is taken out
typedef struct
{
    int used;
    int python;
    unsigned int path_size;
    unsigned int hash;
    CliFactsCell facts;
} CliTreeFile;

// where a file's path is in its facts' cell
#define CLI_TREE_PATH_AT 0ULL

// the max bytes a path of the tree is read into on the stack
#define CLI_TREE_PATH_BYTES (CLI_PATH_MAX_LEN * 2u)

// the tree's files in pages of CLI_TREE_PAGE, each page a movable cell taken as the first place in it is and given
// back as the tree is cleared, and how many places among them were ever taken; the page the file at `file_` is
// in and where its record is in that page. A reader reads the record through the guard's movable read into a
// record of its own (inspect_tree.c)
#define CLI_TREE_PAGE 256u
#define CLI_TREE_PAGES (CLI_TREE_FILES / CLI_TREE_PAGE)
extern const CarcerMovableEntry *g_cli_tree_page[CLI_TREE_PAGES];
extern unsigned int g_cli_tree_files;
#define CLI_TREE_PAGE_OF(file_) (g_cli_tree_page[(file_) / CLI_TREE_PAGE])
#define CLI_TREE_FILE_AT(file_) ((unsigned long long)((file_) % CLI_TREE_PAGE) * sizeof(CliTreeFile))

// the record of the file at `file_` read into `record_`, and `record_` written as it
#define CLI_TREE_FILE_READ(file_, record_)                                                                             \
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = CLI_TREE_PAGE_OF(file_),                           \
                                                            .offset = CLI_TREE_FILE_AT(file_),                         \
                                                            .dst = (record_),                                          \
                                                            .bytes = sizeof(CliTreeFile)})
#define CLI_TREE_FILE_WRITE(file_, record_)                                                                            \
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = CLI_TREE_PAGE_OF(file_),                          \
                                                             .offset = CLI_TREE_FILE_AT(file_),                        \
                                                             .src = (record_),                                         \
                                                             .bytes = sizeof(CliTreeFile)})

// the file of `size` bytes of path at `path`: its place, CLI_TREE_NONE where the tree contains none; the same of a
// path written as `folder` and `name` joined by a slash, `folder` empty for none (inspect_tree.c)
unsigned int cli_tree_find(const CliInspectTreePathCtx *args);
unsigned int cli_tree_find_join(const CliInspectTreeFolderCtx *args);
// the length of the folder of `size` bytes of path, everything before its last slash, 0 where it has none
unsigned long long cli_tree_parent(const CliInspectTreePathCtx *args);

// the file a Python module names, found from the file at `file`: its place, CLI_TREE_NONE where none is; and the
// files an import at `import` of the file at `file` runs, each passed to `each` (inspect_tree_imports.c)
unsigned int cli_tree_python_module(const CliInspectTreeSizeFileCtx *args);
void cli_tree_targets(const CliInspectTreeFileCtx *args);

typedef struct
{
    void *context;
    unsigned int file;
    CliInspectPosition from;
    CliInspectPosition to;
    const char *code;
    const VerbaLinea *message;
} CliTreeFindingCfg;

// A finding the tree makes across its files: the file it is in, its span, its name and what it reports
typedef void (*CliTreeFindingFn)(const CliTreeFindingCfg *args);

// each import that leads back round to the file that makes it, passed to `each` (inspect_tree_cycles.c)
void cli_tree_cycles(const CliInspectTreeCtx *args);

// the families of the tree's Python classes worked out again, in cells sized to them, where the tree changed since
// they last were; those cells given back as the tree changes; and each attribute a method reads of `self` that no
// class of its family sets, passed to `each` (inspect_tree_family.c, inspect_tree_bases.c)
void cli_tree_families(void);
void cli_tree_families_free(void);
void cli_tree_attributes(const CliInspectTreeCtx *args);

// the class a Python file's base names, by a class of the file, a `from` import, or an import of the module it is read
// from, `depth` imports in: 1 with its file and its place among the file's classes, or 0 where it is outside the tree
// (inspect_tree_bases.c)
int cli_tree_base_get(const CliInspectTreeSizeCtx *args);

struct CliInspectTreeSizeCtx
{
    unsigned long long size;
    unsigned int file;
    const char *base;
    unsigned int depth;
    unsigned int *found_file;
    unsigned int *found_class;
};

struct CliInspectTreeFileCtx
{
    unsigned int file;
    unsigned int import;
    void (*each_callback)(const CliEachTargetCfg *args);
    void *context;
};

struct CliInspectTreeFolderCtx
{
    const char *folder;
    unsigned long long folder_size;
    const char *name;
};

struct CliInspectTreeSizeFileCtx
{
    unsigned long long size;
    unsigned int file;
    const char *spec;
};

struct CliInspectTreeCtx
{
    void *context;
    CliTreeFindingFn each;
};

struct CliInspectTreePathCtx
{
    const char *path;
    unsigned long long size;
};

typedef struct
{
    unsigned int (*tree_find)(const CliInspectTreePathCtx *args);
    unsigned long long (*tree_parent)(const CliInspectTreePathCtx *args);
} CliInspectTreePathNsTable;
EMBED_TABLE_LAYOUT(CliInspectTreePathNsTable, tree_find, tree_parent);
EMBED_TABLE_STORAGE CliInspectTreePathNsTable cli_inspect_tree_path_internal EMBED_UNUSED = {
    .tree_find = cli_tree_find,
    .tree_parent = cli_tree_parent,
};

typedef struct
{
    unsigned int (*tree_find_join)(const CliInspectTreeFolderCtx *args);
} CliInspectTreeFolderNs;
EMBED_TABLE_LAYOUT(CliInspectTreeFolderNs, tree_find_join);
EMBED_TABLE_STORAGE CliInspectTreeFolderNs cli_inspect_tree_folder_internal EMBED_UNUSED = {
    .tree_find_join = cli_tree_find_join,
};

typedef struct
{
    unsigned int (*tree_python_module)(const CliInspectTreeSizeFileCtx *args);
} CliInspectTreeSizeFileNs;
EMBED_TABLE_LAYOUT(CliInspectTreeSizeFileNs, tree_python_module);
EMBED_TABLE_STORAGE CliInspectTreeSizeFileNs cli_inspect_tree_size_file_internal EMBED_UNUSED = {
    .tree_python_module = cli_tree_python_module,
};

typedef struct
{
    void (*tree_targets)(const CliInspectTreeFileCtx *args);
} CliInspectTreeFileNs;
EMBED_TABLE_LAYOUT(CliInspectTreeFileNs, tree_targets);
EMBED_TABLE_STORAGE CliInspectTreeFileNs cli_inspect_tree_file_internal EMBED_UNUSED = {
    .tree_targets = cli_tree_targets,
};

typedef struct
{
    void (*tree_cycles)(const CliInspectTreeCtx *args);
    void (*tree_attributes)(const CliInspectTreeCtx *args);
} CliInspectTreeNsTable;
EMBED_TABLE_LAYOUT(CliInspectTreeNsTable, tree_cycles, tree_attributes);
EMBED_TABLE_STORAGE CliInspectTreeNsTable cli_inspect_tree_internal EMBED_UNUSED = {
    .tree_cycles = cli_tree_cycles,
    .tree_attributes = cli_tree_attributes,
};

typedef struct
{
    int (*tree_base_get)(const CliInspectTreeSizeCtx *args);
} CliInspectTreeSizeNs;
EMBED_TABLE_LAYOUT(CliInspectTreeSizeNs, tree_base_get);
EMBED_TABLE_STORAGE CliInspectTreeSizeNs cli_inspect_tree_size_internal EMBED_UNUSED = {
    .tree_base_get = cli_tree_base_get,
};

#endif
