// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_internal.h: what the watch sources share: a change put for the batches, whether the watch is stopping, and the
// tree read every two seconds where the system does not tell of its changes
#ifndef WATCH_INTERNAL_H
#define WATCH_INTERNAL_H

#include "watch.h"

// a change put for the next batch, its path from the tree's top; whether the watch is stopping; and the end of the
// thread that reads the tree told, so the watch can stop (watch.c)
void cli_watch_told(void *context, const char *path, unsigned int kind);
int cli_watch_stopping(void);
void cli_watch_poll_ended(void);

// the tree at `root` read as the first reading, its readings cells counted to the files: 1, a reading that could not
// be made standing as none, which the next reading takes the place of; on a thread of its own, the tree read every two
// seconds and each change since the last reading put for the batches, until the watch stops; and the readings given
// back, which the polling does as it ends (watch_read.c)
int cli_watch_first_reading(const char *root);
void cli_watch_polled(void *context);
void cli_watch_readings_given(void);

// a change told: its path from the tree's top, and its kind
typedef void (*CliWatchTold)(void *context, const char *path, unsigned int kind);

// the tree at `root` read again and set against the last reading, each change since it told to `told` (watch_read.c)
void cli_watch_read_again(const char *root, CliWatchTold told, void *context);

// the changes `paths`, `count` of them, each of its kind in `kinds`, of the tree at `root` gathered and written as a
// batch, while no watch runs: 1 where any are left (watch.c)
int cli_watch_gathered_of(const char *root, const char *const *paths, const unsigned int *kinds, unsigned int count,
                          CliLine *out);

#endif
