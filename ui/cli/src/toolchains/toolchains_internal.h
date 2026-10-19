// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_internal.h: what toolchains_added.c and toolchains_chosen.c share: the files of orior's own folder they
// keep, read and written under one lock; and what the toolchains' check reaches inside toolchains.c and
// toolchains_path.c
#ifndef TOOLCHAINS_INTERNAL_H
#define TOOLCHAINS_INTERNAL_H

#include "../command_line/cells.h"
#include "toolchains.h"

// whether `name` matches `pattern`, whose * matches any run of letters, case aside on Windows; and names ordered as
// their numbers read, so that Python314 comes after Python39: below 0 where `a` comes first (toolchains.c)
int cli_toolchains_matches(const char *pattern, const char *name);
int cli_toolchains_natural(const char *a, const char *b);
// the PATH `shell` sets as a login shell into `path`, which holds `size`: 1, or 0 where it says none within the
// patience (toolchains_path.c)
int cli_toolchains_path_of_shell(const char *shell, char *path, unsigned long long size);

// the lock the files are read and written under
extern CliLock g_cli_toolchains_lock;

// the file `name` in orior's own folder into `path`: 1, or 0, `path` empty, where orior has no folder of its own; the
// file at `path` read into the tree in a cell sized to it, with `nodes_more` nodes and `text_more` bytes over for what
// the caller puts in it, which cli_json_given gives back, an empty object where it is not there or does not read: its
// node; the node written
// to `path` as serde_json's pretty writer lays it out, its keys in order where `sorted`, its folder made: 1, or 0 with
// why said; and a text trimmed of ASCII's spaces at each end into `out`, which holds `room`
int cli_toolchains_file(const char *name, char *path);
unsigned int cli_toolchains_file_read(CliJson *json, const char *path, unsigned int nodes_more,
                                      unsigned long long text_more);
int cli_toolchains_file_kept(const CliJson *json, unsigned int node, const char *path, int sorted, CliLine *said);
void cli_toolchains_trimmed(const char *text, char *out, unsigned long long room);

// the PATH's folders for one call, a cell taken under their lock, and the lock (toolchains_find.c)
extern char (*g_cli_toolchains_path)[CLI_PATH_LONGEST];
extern CliLock g_cli_toolchains_find_lock;

// the PATH's folders taken for the call that holds the lock, into g_cli_toolchains_path: their count; and the call's
// manifest, where `json` is not NULL, and its folders given back, and the lock with them (toolchains_find.c)
unsigned int cli_toolchains_find_path(void);
void cli_toolchains_find_released(CliJson *json);

#endif
