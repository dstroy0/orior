// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// history.h: Local History (history.c): each file of a tree as it was saved, stored in orior's own folder apart from
// git. A version git never saw can be seen and taken back. A snapshot is stored each time the editor writes a file, the
// text as it was before the first one among them, and none the same as the one before it. Each file retains its newest
// 100.
//
// The snapshots of a file are in history/<tree>/<file>/<milliseconds>.txt under orior's folder, the tree and the file
// each named by a hash of its path and the file by its name after it too
#ifndef HISTORY_H
#define HISTORY_H

#include "../command_line/cli_internal.h"

// `size` bytes of `text` stored as the newest snapshot of the file at `path` under the tree at `root`, and
// `before_size` bytes of `before`, the text the file contained, ahead of it where the file has none yet and `before` is
// not NULL: 1, or 0 with why given
int cli_history_save(const char *root, const char *path, const char *before, unsigned long long before_size,
                     const char *text, unsigned long long size, VerbaLinea *err_msg);
// the snapshots of the file at `path`, the newest first, written as `[{"at": milliseconds, "size": bytes}...]`
void cli_history_list(const char *root, const char *path, VerbaLinea *out);
// the text of the snapshot stored at `at` put into `out`: 1, or 0 with why given
int cli_history_read(const char *root, const char *path, unsigned long long at, VerbaLinea *out, VerbaLinea *err_msg);

#endif
