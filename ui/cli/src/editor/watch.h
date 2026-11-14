// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch.h: changes to the tree's files from outside the window, gathered in batches for the window's tree to take: the
// folders whose entries changed, the files whose text did, and whether git's view did.
//
// On Windows the system reports each change as it is made, through ReadDirectoryChangesW over the whole tree.
// Elsewhere, and where the system cannot watch the tree, the files git tracks or would track, and the folders
// containing them, are read every two seconds and set against the last reading. A batch goes once 150 ms pass with no
// change, or once a second passes from its first.
//
// Of git's own folder only its index, HEAD and refs count, and a change to them means git's view changed. A change the
// watching's patterns hide is left out (watch.c, watch_read.c)
#ifndef WATCH_H
#define WATCH_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"
#include "files.h"

typedef struct CliWatchCfg CliWatchCfg;
typedef struct CliWatchBatchCfg CliWatchBatchCfg;

// a batch taken: the folders whose entries changed and the files whose text did, each from the tree's top and valid
// while the call runs; whether git's view changed, and whether changes were lost
typedef void (*CliWatchTake)(const CliWatchBatchCfg *args);

// the tree at `root` watched, each batch of changes passed to `take` on a thread of the watch's own, any watch before
// it stopped first; and the watch stopped, its threads waited for
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
    const CliFiles *folders;
    const CliFiles *files;
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
