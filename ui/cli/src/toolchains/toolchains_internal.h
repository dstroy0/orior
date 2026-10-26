// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_internal.h: what toolchains_added.c and toolchains_chosen.c share: the files of orior's own folder they
// cache, read and written under one lock; and what the toolchains' check reaches inside toolchains.c and
// toolchains_path.c
#ifndef TOOLCHAINS_INTERNAL_H
#define TOOLCHAINS_INTERNAL_H

#include "../command_line/cells.h"
#include "toolchains.h"

// whether `name` matches `pattern`, whose * matches any run of letters, case aside on Windows; and names ordered as
// their numbers read, so that Python314 comes after Python39: below 0 where `a` comes first (toolchains.c)
int cli_toolchains_matches(const char *pattern, const char *name);
int cli_toolchains_natural(const char *a, const char *b);
// the PATH `shell` sets as a login shell into `path`, which is sized for `size`: 1, or 0 where it sets none within the
// patience (toolchains_path.c)
int cli_toolchains_path_of_shell(const char *shell, char *path, unsigned long long size);

// the lock the files are read and written under
extern CliLock g_cli_toolchains_lock;

// tool `id` as its search found it against the PATH's `count` folders, as cached in toolchains_found.json, into
// `found`: 1, or 0 where it is not cached, what the search reads has changed since, or its program is gone; a search's
// `found` cached, where it was found by what the search reads, written at once unless a check locks the cache; and a
// check of every tool locking the cache, which is written once as the last lets it go (toolchains_chosen.c)
int cli_toolchains_found_cached(const char *id, const char (*path)[CLI_PATH_MAX_LEN], unsigned int count,
                                CliFound *found);
void cli_toolchains_found_put(const char *id, const char (*path)[CLI_PATH_MAX_LEN], unsigned int count,
                              const CliFound *found);
void cli_toolchains_found_ref(void);
void cli_toolchains_found_let(void);

// the file `name` in orior's own folder into `path`: 1, or 0, `path` empty, where orior_cli has no folder of its own;
// the file at `path` read into the tree in a cell sized to it, with `nodes_more` nodes and `text_more` bytes over for
// what the caller puts in it, which cli_json_given gives back, an empty object where it is not there or does not read:
// its node; the node written to `path` as serde_json's pretty writer lays it out, its keys in order where `sorted`, its
// folder made: 1, or 0 with why given; and a text trimmed of ASCII's spaces at each end into `out`, sized for `room`
int cli_toolchains_file(const char *name, char *path);
unsigned int cli_toolchains_file_read(CliJson *json, const char *path, unsigned int nodes_more,
                                      unsigned long long text_more);
int cli_toolchains_file_saved(const CliJson *json, unsigned int node, const char *path, int sorted,
                              VerbaLinea *err_msg);
void cli_toolchains_trimmed(const char *text, char *out, unsigned long long room);

// the PATH's folders for one call, a cell taken under their lock, and the lock (toolchains_find.c)
extern char (*g_cli_toolchains_path)[CLI_PATH_MAX_LEN];
extern CliLock g_cli_toolchains_find_lock;

// the PATH's folders taken for the call that acquires the lock, into g_cli_toolchains_path: their count; and the call's
// manifest, where `json` is not NULL, and its folders given back, and the lock with them (toolchains_find.c)
unsigned int cli_toolchains_find_path(void);
void cli_toolchains_find_released(CliJson *json);

#endif
