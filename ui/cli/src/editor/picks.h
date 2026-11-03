// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// picks.h: the lists a quick open of the window's shows that are not the tree's files or symbols, as the tree's tests
// and the machine's processes: each stored here under a number as it opens, and matched as each letter is typed, the
// best first, as files_fuzzy.c matches the tree's files. A list of tens of thousands answers a letter in a few
// milliseconds, spread over the cores (picks.c)
#ifndef PICKS_H
#define PICKS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliPicksIdCfg CliPicksIdCfg;
typedef struct CliPicksCfg CliPicksCfg;

// the most lists stored at once
#define CLI_PICKS_LISTS 64u

// the rows of `length` bytes of `texts`, one to a line, stored in a cell of their size: the number they are stored
// under, from 1, or 0 where the lists stored are already the most or no cell is left; the list stored under `id` let
// go; and the rows of the list stored under `id` that answer `query`, at most `most` of them, the best first and rows
// that score alike in the list's order, with no query its first `most` rows, written as `[{"at", "score", "hits"}]`: 1,
// or 0 with why given where no list is stored under `id`
unsigned long long cli_picks_store(const CliPicksCfg *args);
void cli_picks_forget(const CliPicksIdCfg *args);

struct CliPicksIdCfg
{
    unsigned long long id;
    const char *query;
    unsigned long long most;
    VerbaLinea *out;
    VerbaLinea *err_msg;
};

struct CliPicksCfg
{
    const char *texts;
    unsigned long long length;
};

typedef struct
{
    unsigned long long (*store)(const CliPicksCfg *args);
} CliPicksNs;
EMBED_TABLE_LAYOUT(CliPicksNs, store);
EMBED_TABLE_STORAGE CliPicksNs cli_picks EMBED_UNUSED = {
    .store = cli_picks_store,
};

typedef struct
{
    void (*forget)(const CliPicksIdCfg *args);
} CliPicksIdNs;
EMBED_TABLE_LAYOUT(CliPicksIdNs, forget);
EMBED_TABLE_STORAGE CliPicksIdNs cli_picks_id EMBED_UNUSED = {
    .forget = cli_picks_forget,
};

#endif
