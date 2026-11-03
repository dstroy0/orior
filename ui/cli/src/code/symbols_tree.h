// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_tree.h: what the tree's sources share: its branches stored, each its declarations in a cell of its own, the
// lock they are read and written under, and whether a walk goes
#ifndef SYMBOLS_TREE_H
#define SYMBOLS_TREE_H
#include "embed_dispatch_layout.h"
#include "symbols.h"

typedef struct CliSymbolsTreeBranchCfg CliSymbolsTreeBranchCfg;
typedef struct CliSymbolsTreeBranchScratchCfg CliSymbolsTreeBranchScratchCfg;
typedef struct CliSymbolsTreeCountCfg CliSymbolsTreeCountCfg;
typedef struct CliSymbolsTreePathCfg CliSymbolsTreePathCfg;
typedef struct CliSymbolsTreeBranchPathCfg CliSymbolsTreeBranchPathCfg;
typedef struct CliSymbolsTreeCfg CliSymbolsTreeCfg;
typedef struct CliSymbolsTreeFileCfg CliSymbolsTreeFileCfg;

// the most branches stored; and what a branch's closest parent reads where it stands at the tree's top, and where it
// is mounted beside the tree, whose folders have no branch
#define CLI_BRANCHES_MAX 131072u
#define CLI_BRANCH_TOP CLI_BRANCHES_MAX
#define CLI_BRANCH_MOUNTED (CLI_BRANCHES_MAX + 1u)

// One declaration stored: its name among its file's cell, what it is among the same cell, and its line
typedef struct
{
    unsigned int name;
    unsigned int length;
    unsigned int kind;
    unsigned int line;
} CliDeclaration;

// One branch stored, a file or the folder of one: a movable cell of its own containing its declarations, then its path,
// then their names, reached only through the cells' guard; its bytes, its path's place in the cell, whether it stands,
// whether it is a folder, whether a walk has read what it declares, the stamp of the file as last seen, 0 until then,
// its count of declarations, whether a listing or a walk saw it, its closest parent, and how many branches stand under
// a folder. A place left is reused, so that a branch's place stands for as long as it does
typedef struct
{
    const CarcerMovableSlot *cell;
    unsigned int bytes;
    unsigned int path;
    int used;
    int folder;
    int walked;
    unsigned long long written;
    unsigned int count;
    int seen;
    unsigned int parent;
    unsigned int below;
} CliTreeBranch;

// the branches stored, in a cell at their most, CLI_BRANCHES_MAX at the most, how many places among them were taken,
// the lock, and whether a walk goes (symbols.c, symbols_table.c); the folder the tree reads, its top, written under the
// lock as a tree is opened (symbols.c)
extern char g_cli_tree_root[CLI_PATH_MAX_LEN];
extern CliTreeBranch *g_cli_branches;
extern unsigned int g_cli_branch_count;
extern int g_cli_tree_walking;

// a cell for a reader to scan a file's cell in, sized for the largest stored, NULL where the cells cannot meet it; a
// file's cell copied into it; and it given back. A reader scans its copy: a stored cell moves, and the guard is the
// only way to it. Then a copy's path and declarations, a stored file's path copied into `path`, sized for
// CLI_PATH_MAX_LEN, and a cell's bytes counted toward the largest stored; the place of a path's branch,
// CLI_BRANCHES_MAX where none is stored; the branch of `path` standing, joined where none is, every folder down to it
// joined as it goes, a cell of its path alone: its place, or CLI_BRANCHES_MAX where the table or the cells cannot meet
// it; a branch let go, its place taken out of the paths' table and its cell given back, and each folder over it left
// with nothing under it let go in turn; the files standing, how many and the bytes of their paths with their ends; and
// every branch let go with the table's cells. Called under the lock (symbols_table.c)
char *cli_branch_copy_alloc(void);
void cli_branch_copy(const CliSymbolsTreeBranchCfg *args);
void cli_branch_copy_free(const CliSymbolsTreeBranchCfg *args);
const char *cli_branch_copy_path(const CliSymbolsTreeBranchScratchCfg *args);
const CliDeclaration *cli_branch_copy_declarations(const CliSymbolsTreeBranchScratchCfg *args);
void cli_branch_path_read(const CliSymbolsTreeBranchPathCfg *args);
void cli_branch_cell_count(const CliSymbolsTreeCfg *args);
unsigned int cli_branch_get(const CliSymbolsTreePathCfg *args);
unsigned int cli_branch_exists(const CliSymbolsTreePathCfg *args);
void cli_branch_leave(const CliSymbolsTreeFileCfg *args);
void cli_branch_files_count(const CliSymbolsTreeCountCfg *args);
void cli_tree_clear(void);

struct CliSymbolsTreeBranchCfg
{
    const CliTreeBranch *branch;
    char *scratch;
};

struct CliSymbolsTreeBranchScratchCfg
{
    const CliTreeBranch *branch;
    const char *scratch_read;
};

struct CliSymbolsTreeCountCfg
{
    unsigned int *count;
    unsigned long long *bytes_u64s;
};

struct CliSymbolsTreePathCfg
{
    const char *path;
    int folder;
};

struct CliSymbolsTreeBranchPathCfg
{
    const CliTreeBranch *branch;
    char *path_out;
};

struct CliSymbolsTreeCfg
{
    unsigned int bytes;
};

struct CliSymbolsTreeFileCfg
{
    unsigned int file;
};

typedef struct
{
    void (*branch_copy)(const CliSymbolsTreeBranchCfg *args);
    void (*branch_copy_free)(const CliSymbolsTreeBranchCfg *args);
} CliSymbolsTreeBranchNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreeBranchNs, branch_copy, branch_copy_free);
EMBED_TABLE_STORAGE CliSymbolsTreeBranchNs cli_symbols_tree_branch EMBED_UNUSED = {
    .branch_copy = cli_branch_copy,
    .branch_copy_free = cli_branch_copy_free,
};

typedef struct
{
    const char *(*branch_copy_path)(const CliSymbolsTreeBranchScratchCfg *args);
    const CliDeclaration *(*branch_copy_declarations)(const CliSymbolsTreeBranchScratchCfg *args);
} CliSymbolsTreeBranchScratchNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreeBranchScratchNs, branch_copy_path, branch_copy_declarations);
EMBED_TABLE_STORAGE CliSymbolsTreeBranchScratchNs cli_symbols_tree_branch_scratch EMBED_UNUSED = {
    .branch_copy_path = cli_branch_copy_path,
    .branch_copy_declarations = cli_branch_copy_declarations,
};

typedef struct
{
    void (*branch_path_read)(const CliSymbolsTreeBranchPathCfg *args);
} CliSymbolsTreeBranchPathNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreeBranchPathNs, branch_path_read);
EMBED_TABLE_STORAGE CliSymbolsTreeBranchPathNs cli_symbols_tree_branch_path EMBED_UNUSED = {
    .branch_path_read = cli_branch_path_read,
};

typedef struct
{
    void (*branch_cell_count)(const CliSymbolsTreeCfg *args);
} CliSymbolsTreeNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreeNs, branch_cell_count);
EMBED_TABLE_STORAGE CliSymbolsTreeNs cli_symbols_tree EMBED_UNUSED = {
    .branch_cell_count = cli_branch_cell_count,
};

typedef struct
{
    unsigned int (*branch_get)(const CliSymbolsTreePathCfg *args);
    unsigned int (*branch_exists)(const CliSymbolsTreePathCfg *args);
} CliSymbolsTreePathNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreePathNs, branch_get, branch_exists);
EMBED_TABLE_STORAGE CliSymbolsTreePathNs cli_symbols_tree_path EMBED_UNUSED = {
    .branch_get = cli_branch_get,
    .branch_exists = cli_branch_exists,
};

typedef struct
{
    void (*branch_leave)(const CliSymbolsTreeFileCfg *args);
} CliSymbolsTreeFileNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreeFileNs, branch_leave);
EMBED_TABLE_STORAGE CliSymbolsTreeFileNs cli_symbols_tree_file EMBED_UNUSED = {
    .branch_leave = cli_branch_leave,
};

typedef struct
{
    void (*branch_files_count)(const CliSymbolsTreeCountCfg *args);
} CliSymbolsTreeCountNs;
EMBED_TABLE_LAYOUT(CliSymbolsTreeCountNs, branch_files_count);
EMBED_TABLE_STORAGE CliSymbolsTreeCountNs cli_symbols_tree_count EMBED_UNUSED = {
    .branch_files_count = cli_branch_files_count,
};

#endif
