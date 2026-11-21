// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch.h: changes to the tree's files from outside the window, gathered in batches for the window's tree to take: the
// folders whose entries changed, the files whose text did, and whether git's view did.
//
// On Windows the system reports each change as it occurs, through ReadDirectoryChangesW over the whole tree.
// Elsewhere, and where the system cannot watch the tree, the watch reads the files git tracks or would track every two
// seconds. It also reads the folders that hold them, and sets each reading against the last. A batch goes once 150 ms
// pass with no change, or once a second passes from its first.
//
// Of git's own folder only its index, HEAD and refs count, and a change to them means git's view changed. The batch
// leaves out a change the watching's patterns hide (watch.c, watch_read.c)
#ifndef WATCH_H
#define WATCH_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"
#include "files.h"

typedef struct CliWatchCfg CliWatchCfg;
typedef struct CliWatchBatchCfg CliWatchBatchCfg;

// A batch taken. It holds the folders whose entries changed and the files whose text did, each from the tree's top.
// Each list is in a movable cell of the files module, valid for the call alone. A reader reads a path into a buffer
// of its own call. Last, whether git's view changed, and whether the watch lost changes
typedef void (*CliWatchTake)(const CliWatchBatchCfg *args);

// The tree at `root` watched, each batch of changes passed to `take` on a thread of the watch's own. Any watch before
// it stops first. Then the watch stopped, and its threads waited for
void cli_watch_start(const CliWatchCfg *args);
void cli_watch_stop(void);

struct CliWatchCfg
{
    const char *root;
    CliWatchTake take;
    void *context;
};

struct CliWatchBatchCfg
{
    void *context;
    const CliPicked *folders;
    const CliPicked *files;
    int git;
    int lost;
};

typedef struct
{
    void (*start)(const CliWatchCfg *args);
} CliWatchNs;
EMBED_TABLE_LAYOUT(CliWatchNs, start);
EMBED_TABLE_STORAGE CliWatchNs cli_watch EMBED_UNUSED = {
    .start = cli_watch_start,
};

#endif
