// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_index.h: what the index's sources share: its files held, each its declarations in a cell of its own, the
// lock they are read and written under, and whether a reading goes
#ifndef SYMBOLS_INDEX_H
#define SYMBOLS_INDEX_H

#include "symbols.h"

// the most files held
#define CLI_SYMBOL_FILES 131072u

// One declaration held: its name among its file's cell, what it is, and its line
typedef struct
{
    unsigned int name;
    unsigned int length;
    const char *kind;
    unsigned int line;
} CliSymbol;

// One file held: a cell of its own holding its declarations, then its path, then their names; its path's place in the
// cell, the time it was written, its count of declarations, and whether a reading saw it
typedef struct
{
    char *cell;
    unsigned int path;
    int used;
    unsigned long long written;
    unsigned int count;
    int seen;
} CliHeldFile;

// the files held, how many places among them were taken, the lock, and whether a reading goes (symbols.c)
extern CliHeldFile g_cli_symbol_files[CLI_SYMBOL_FILES];
extern unsigned int g_cli_symbol_file_count;
extern CliLock g_cli_symbols_lock;
extern int g_cli_symbols_reading;

// a file held's path, and its declarations (symbols.c)
const char *cli_held_path(const CliHeldFile *held);
const CliSymbol *cli_held_symbols(const CliHeldFile *held);

#endif
