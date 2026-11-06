// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs.h: the definitions the editor reads its languages from, as the tree stores them (defs.c).
//
// Each file type's oracle table under src/cu/includes/file_defs, the head every k-file opens with, and the language
// tables under src/lng. The editor builds its highlighting, its hover and its completion from these and contains no
// word of its own. A form added to an oracle table is a form the editor knows the next time it opens
#ifndef DEFS_H
#define DEFS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliDefsPathCfg CliDefsPathCfg;
typedef struct CliDefsCfg CliDefsCfg;

// the most cells of a table's line read, those past it left out
#define CLI_DEFS_CELLS 32u

// One line of a table: its cells between tabs, where each starts and its length, and how many
typedef struct
{
    const char *cell[CLI_DEFS_CELLS];
    unsigned long long length[CLI_DEFS_CELLS];
    unsigned int count;
} CliDefsLine;

typedef struct
{
    void *context;
    const CliDefsLine *line;
    int named;
} CliDefsLineEachCfg;

// what each line of a table is handed to, `named` 1 for the line that names its columns
typedef void (*CliDefsLineEach)(const CliDefsLineEachCfg *args);

// the tab separated table at `path` read: its first line that is not a comment, naming its columns, handed to `each`
// with `named` 1, then each line after it that is not a comment with `named` 0: 1, or 0 where it could not be read or
// names no columns
int cli_defs_lines(const CliDefsPathCfg *args);

struct CliDefsPathCfg
{
    const char *path;
    CliDefsLineEach each;
    void *context;
};

struct CliDefsCfg
{
    const char *root;
    VerbaLinea *out;
};

typedef struct
{
    int (*lines)(const CliDefsPathCfg *args);
} CliDefsPathNs;
EMBED_TABLE_LAYOUT(CliDefsPathNs, lines);
EMBED_TABLE_STORAGE CliDefsPathNs cli_defs_path EMBED_UNUSED = {
    .lines = cli_defs_lines,
};

#endif
