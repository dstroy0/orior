// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_internal.h: what the watch sources share: a change put for the batches, whether the watch is stopping, and the
// tree read every two seconds where the system does not report its changes
#ifndef WATCH_INTERNAL_H
#define WATCH_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"
#include "watch.h"

typedef struct CliWatchPathsCtx CliWatchPathsCtx;
typedef struct CliWatchCtx CliWatchCtx;

// a change put for the next batch, its path from the tree's top; whether the watch is stopping; the end of the thread
void cli_watch_change(const CliEachPathTypeCfg *args);
int cli_watch_stopping(void);
void cli_watch_poll_end(void);

// the tree at `root` read as the first reading, its readings cells counted to the files: 1, a reading that could not
// be made standing as none, which the next reading takes the place of; on a thread of its own, the tree read every two
// seconds and each change since the last reading put for the batches, until the watch stops; and the readings given
// back, which the polling does as it ends (watch_read.c)
int cli_watch_first_read_state(const CliWatchCtx *args);
void cli_watch_poll(void *context);

// a change reported: its path from the tree's top, and its type
typedef void (*CliWatchChangeFn)(const CliEachPathTypeCfg *args);

// the tree at `root` read again and set against the last reading, each change since it handed to `emit` (watch_read.c)
void cli_watch_read_again(const CliWatchCtx *args);

// the changes `paths`, `count` of them, each of its type in `types`, of the tree at `root` gathered and handed to
// `take` as a batch, while no watch runs: 1 where any are left (watch.c)
int cli_watch_gathered_get(const CliWatchPathsCtx *args);

struct CliWatchPathsCtx
{
    const char *root;
    const char *const *paths;
    const unsigned int *types;
    unsigned int count;
    CliWatchTake take;
    void *context;
};

struct CliWatchCtx
{
    const char *root;
    CliWatchChangeFn emit;
    void *context;
};

typedef struct
{
    void (*change)(const CliEachPathTypeCfg *args);
} CliEachPathTypeNs;
EMBED_TABLE_LAYOUT(CliEachPathTypeNs, change);
EMBED_TABLE_STORAGE CliEachPathTypeNs cli_each_path_type EMBED_UNUSED = {
    .change = cli_watch_change,
};

typedef struct
{
    int (*first_read_state)(const CliWatchCtx *args);
    void (*read_again)(const CliWatchCtx *args);
} CliWatchNsTable;
EMBED_TABLE_LAYOUT(CliWatchNsTable, first_read_state, read_again);
EMBED_TABLE_STORAGE CliWatchNsTable cli_watch_internal EMBED_UNUSED = {
    .first_read_state = cli_watch_first_read_state,
    .read_again = cli_watch_read_again,
};

typedef struct
{
    int (*gathered_get)(const CliWatchPathsCtx *args);
} CliWatchPathsNs;
EMBED_TABLE_LAYOUT(CliWatchPathsNs, gathered_get);
EMBED_TABLE_STORAGE CliWatchPathsNs cli_watch_paths_internal EMBED_UNUSED = {
    .gathered_get = cli_watch_gathered_get,
};

#endif
