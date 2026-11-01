// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_tree.h: what the tree's sources share: its files stored, each its declarations in a cell of its own, the
// lock they are read and written under, and whether a reading goes
#ifndef SYMBOLS_TREE_H
#define SYMBOLS_TREE_H

#include "symbols.h"

// the most files stored
#define CLI_SYMBOL_FILES 131072u

// One declaration stored: its name among its file's cell, what it is among the same cell, and its line
typedef struct
{
    unsigned int name;
    unsigned int length;
    unsigned int kind;
    unsigned int line;
} CliSymbol;

// One file stored: a movable cell of its own containing its declarations, then its path, then their names, reached only
// through the cells' guard; its bytes, its path's place in the cell, the time it was written, its count of declarations,
// and whether a reading saw it
typedef struct
{
    const CarcerMovableSlot *cell;
    unsigned int bytes;
    unsigned int path;
    int used;
    unsigned long long written;
    unsigned int count;
    int seen;
} CliSymbolFile;

// the files stored, in a cell at their most, CLI_SYMBOL_FILES at the most, how many places among them were taken, the
// lock, and whether a reading goes (symbols.c, symbols_table.c) the folder the tree reads, its top, written under the
// lock as a tree is opened (symbols.c)
extern char g_cli_symbols_root[CLI_PATH_MAX_LEN];
extern CliSymbolFile *g_cli_symbol_files;
extern unsigned int g_cli_symbol_file_count;
extern CliLock g_cli_symbols_lock;
extern int g_cli_symbols_reading;

// a cell for a reader to scan a file's cell in, sized for the largest stored, NULL where the cells cannot meet it; a
// file's cell copied into it; and it given back. A reader scans its copy: a stored cell moves, and the guard is the only
// way to it. Then a copy's path and declarations, a stored file's path copied into `path`, sized for CLI_PATH_MAX_LEN,
// and a cell's bytes counted toward the largest stored; the place of a path's file, CLI_SYMBOL_FILES where none is stored;
// a file's place put in the paths' table; room made among the files for one more, the table taken or put together: 1, or
// 0 where none is made; a file stored let go, its place taken out of the paths' table and its cell given back; and every
// file let go with the table's cells. Called under the lock (symbols_table.c)
char *cli_symbol_scratch_taken(void);
void cli_symbol_file_read(const CliSymbolFile *branch, char *scratch);
void cli_symbol_scratch_given(char *scratch);
const char *cli_symbol_file_path(const CliSymbolFile *branch, const char *scratch);
const CliSymbol *cli_symbol_file_symbols(const char *scratch);
void cli_symbol_path_read(const CliSymbolFile *branch, char *path);
void cli_symbol_cell_counted(unsigned int bytes);
unsigned int cli_symbols_file_of(const char *path);
void cli_symbols_bucket_put(const char *path, unsigned int file);
int cli_symbols_room_made(void);
void cli_symbols_let_go(unsigned int file);
void cli_symbols_tree_cleared(void);

#endif
