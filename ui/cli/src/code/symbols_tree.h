// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_tree.h: what the tree's sources share: its branches stored, each its declarations in a cell of its own, the
// lock they are read and written under, and whether a walk goes
#ifndef SYMBOLS_TREE_H
#define SYMBOLS_TREE_H

#include "symbols.h"

// the most branches stored; and what a branch's closest parent reads where it stands at the tree's top, and where it
// is mounted beside the tree, whose folders have no branch
#define CLI_BRANCHES_MOST 131072u
#define CLI_BRANCH_TOP CLI_BRANCHES_MOST
#define CLI_BRANCH_MOUNTED (CLI_BRANCHES_MOST + 1u)

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

// the branches stored, in a cell at their most, CLI_BRANCHES_MOST at the most, how many places among them were taken,
// the lock, and whether a walk goes (symbols.c, symbols_table.c); the folder the tree reads, its top, written under the
// lock as a tree is opened (symbols.c)
extern char g_cli_tree_root[CLI_PATH_MAX_LEN];
extern CliTreeBranch *g_cli_branches;
extern unsigned int g_cli_branch_count;
extern CliLock g_cli_walk_lock;
extern int g_cli_tree_walking;

// a cell for a reader to scan a file's cell in, sized for the largest stored, NULL where the cells cannot meet it; a
// file's cell copied into it; and it given back. A reader scans its copy: a stored cell moves, and the guard is the only
// way to it. Then a copy's path and declarations, a stored file's path copied into `path`, sized for CLI_PATH_MAX_LEN,
// and a cell's bytes counted toward the largest stored; the place of a path's branch, CLI_BRANCHES_MOST where none is
// stored; the branch of `path` standing, joined where none is, every folder down to it joined as it goes, a cell of its
// path alone: its place, or CLI_BRANCHES_MOST where the table or the cells cannot meet it; a branch let go, its place
// taken out of the paths' table and its cell given back, and each folder over it left with nothing under it let go in
// turn; the files standing, how many and the bytes of their paths with their ends; and every branch let go with the
// table's cells. Called under the lock (symbols_table.c)
char *cli_branch_copy_taken(void);
void cli_branch_copied(const CliTreeBranch *branch, char *scratch);
void cli_branch_copy_given(char *scratch);
const char *cli_branch_copy_path(const CliTreeBranch *branch, const char *scratch);
const CliDeclaration *cli_branch_copy_declarations(const char *scratch);
void cli_branch_path_read(const CliTreeBranch *branch, char *path);
void cli_branch_cell_counted(unsigned int bytes);
unsigned int cli_branch_of(const char *path);
unsigned int cli_branch_stands(const char *path, int folder);
void cli_branch_left(unsigned int file);
void cli_branch_files_counted(unsigned int *count, unsigned long long *bytes);
void cli_tree_cleared(void);

#endif
