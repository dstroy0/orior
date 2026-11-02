// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_usages.c: the definition, the usages, whether a name can be renamed and the edits that rename it, for a file
// its server is given as the tree's slice, answered from the tree (symbols_usages.c), the usages in the order of their
// files, lines and columns, each file's text the editor's where it holds one. A name the tree knows no definition of
// has none: the server is never asked
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "servers_internal.h"

#include <stdlib.h>

// The usages read: the tree's top, the usages, their paths and their lines' texts, each a line, and how many
typedef struct
{
    const char *tree;
    VerbaLinea usage;
    VerbaLinea paths;
    VerbaLinea texts;
    unsigned int count;
} CliServersUsagesRead;
// the bytes the usages, their paths and their lines' texts are each written in
#define CLI_SERVERS_USAGES_CAPACITY (8ull << 20u)

// One usage read: its path among the paths, its line, its span in UTF-16 units, and its line's text among the texts
// and how many bytes
typedef struct
{
    unsigned long long path;
    unsigned int line;
    unsigned int from;
    unsigned int to;
    unsigned long long text;
    unsigned long long length;
} CliServersUsageRead;

// the usages read, sorted under their own lock
static const CliServersUsagesRead *s_sorting;

typedef struct
{
    const CliServersUsagesRead *read_read;
    unsigned int at;
    const char *tree;
    const char *path;
    unsigned int line;
    unsigned int col;
    int across;
    CliServersUsagesRead *read;
    unsigned int **order;
    VerbaLinea *err_msg;
    unsigned int *order_value;
} ServersUsagesCtx;
