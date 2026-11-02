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
typedef struct CliHistoryBeforeCfg CliHistoryBeforeCfg;
typedef struct CliHistoryCfg CliHistoryCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// `size` bytes of `text` stored as the newest snapshot of the file at `path` under the tree at `root`, and
// `before_size` bytes of `before`, the text the file contained, ahead of it where the file has none yet and `before` is
// not NULL: 1, or 0 with why given
int cli_history_save(const CliHistoryBeforeCfg *args);
// the text of the snapshot stored at `at` put into `out`: 1, or 0 with why given
int cli_history_read(const CliHistoryCfg *args);

struct CliHistoryBeforeCfg
{
    const char *root;
    const char *path;
    const char *before;
    unsigned long long before_size;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
};

struct CliHistoryCfg
{
    const char *root;
    const char *path;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    unsigned long long at;
};

typedef struct
{
    int (*save)(const CliHistoryBeforeCfg *args);
} CliHistoryBeforeNs;
EMBED_TABLE_LAYOUT(CliHistoryBeforeNs, save);
EMBED_TABLE_STORAGE CliHistoryBeforeNs cli_history_before EMBED_UNUSED = {
    .save = cli_history_save,
};

typedef struct
{
    int (*read)(const CliHistoryCfg *args);
} CliHistoryNs;
EMBED_TABLE_LAYOUT(CliHistoryNs, read);
EMBED_TABLE_STORAGE CliHistoryNs cli_history EMBED_UNUSED = {
    .read = cli_history_read,
};

#endif
