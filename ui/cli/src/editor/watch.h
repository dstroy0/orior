// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch.h: changes to the tree's files made outside the window, gathered in batches for the window to take: the
// folders whose entries changed, the files whose text did, and whether git's view did.
//
// On Windows the system tells of each change as it is made, through ReadDirectoryChangesW over the whole tree.
// Elsewhere, and where the system cannot watch the tree, the files git tracks or would track, and the folders holding
// them, are read every two seconds and set against the last reading. A batch goes once 150 ms pass with no change, or
// once a second passes from its first.
//
// Of git's own folder only its index, HEAD and refs count, and a change to them says git's view changed. A change the
// watching's patterns hide is left out (watch.c, watch_read.c)
#ifndef WATCH_H
#define WATCH_H

#include "../command_line/cli_internal.h"

// a batch handed to the window, written as `{"folders", "files", "git", "lost"}`, `size` bytes
typedef void (*CliTake)(void *context, const char *batch, unsigned long long size);

// the tree at `root` watched, each batch of changes handed to `take` on a thread of the watch's own, any watch before
// it stopped first; and the watch stopped, its threads waited for
void cli_watch_start(const char *root, CliTake take, void *context);
void cli_watch_stop(void);

#endif
