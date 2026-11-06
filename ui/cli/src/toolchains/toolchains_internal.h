// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_internal.h: what toolchains_added.c and toolchains_chosen.c share: the files of orior's own folder they
// cache, read and written under one lock; and what the toolchains' check reaches inside toolchains.c and
// toolchains_path.c
#ifndef TOOLCHAINS_INTERNAL_H
#define TOOLCHAINS_INTERNAL_H
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"
#include "toolchains.h"

typedef struct CliToolchainsPathCtx CliToolchainsPathCtx;
typedef struct CliToolchainsJsonCtx CliToolchainsJsonCtx;
typedef struct CliToolchainsIdCtx CliToolchainsIdCtx;
typedef struct CliToolchainsIdPathCtx CliToolchainsIdPathCtx;
typedef struct CliToolchainsShellCtx CliToolchainsShellCtx;
typedef struct CliToolchainsTextCtx CliToolchainsTextCtx;
typedef struct CliToolchainsCtx CliToolchainsCtx;
typedef struct CliToolchainsPatternCtx CliToolchainsPatternCtx;
typedef struct CliToolchainsACtx CliToolchainsACtx;

// whether `name` matches `pattern`, whose * matches any run of letters, case aside on Windows; and names ordered as
// their numbers read, so that Python314 comes after Python39: below 0 where `a` comes first (toolchains.c)
int cli_toolchains_matches(const CliToolchainsPatternCtx *args);
int cli_toolchains_natural(const CliToolchainsACtx *args);
// the PATH `shell` sets as a login shell into `path`, which is sized for `size`: 1, or 0 where it sets none within the
// patience (toolchains_path.c)
int cli_toolchains_path_of_shell(const CliToolchainsShellCtx *args);

// the lock the files are read and written under

// tool `id` as its search found it against the PATH's `count` folders, as cached in toolchains_found.json, into
// `found`: 1, or 0 where it is not cached, what the search reads has changed since, or its program is gone; a search's
// `found` cached, where it was found by what the search reads, written at once unless a check locks the cache; and a
// check of every tool locking the cache, which is written once as the last lets it go (toolchains_chosen.c)
int cli_toolchains_found_cache(const CliToolchainsIdCtx *args);
void cli_toolchains_found_append(const CliToolchainsIdPathCtx *args);
void cli_toolchains_found_ref(void);
void cli_toolchains_found_let(void);

// the file `name` in orior's own folder into `path`: 1, or 0, `path` empty, where orior_cli has no folder of its own;
// the file at `path` read into the tree in a cell sized to it, with `nodes_more` nodes and `text_more` bytes over for
// what the caller puts in it, which persistent_buf_release releases, an empty object where it is not there or does not
// read: its node; the node written to `path` as serde_json's pretty writer lays it out, its keys in order where
// `sorted`, its folder made: 1, or 0 with why given; and a text trimmed of ASCII's spaces at each end into `out`, sized
// for `capacity`
int cli_toolchains_file(const CliToolchainsCtx *args);
unsigned int cli_toolchains_file_read(const CliToolchainsJsonCtx *args);
int cli_toolchains_file_save(const CliToolchainsPathCtx *args);
void cli_toolchains_trim(const CliToolchainsTextCtx *args);

// the PATH's folders for one call, a cell taken under their lock, and the lock (toolchains_find.c)
extern char (*g_cli_toolchains_path)[CLI_PATH_MAX_LEN];

// the PATH's folders taken for the call that acquires the lock, into g_cli_toolchains_path: their count; and the call's
// manifest, where `json` is not NULL, and its folders given back, and the lock with them (toolchains_find.c)
unsigned int cli_toolchains_find_path(void);
void cli_toolchains_find_release(const CliToolchainsJsonCtx *args);

struct CliToolchainsPathCtx
{
    const char *path_read;
    const CliJson *json_read;
    unsigned int node;
    int sorted;
    VerbaLinea *err_msg;
};

struct CliToolchainsJsonCtx
{
    CliJson *json;
    const char *path_read;
    unsigned int nodes_more;
    unsigned long long text_more;
};

struct CliToolchainsIdCtx
{
    const char *id;
    const char (*path_callback)[CLI_PATH_MAX_LEN];
    unsigned int count;
    CliFound *found;
};

struct CliToolchainsIdPathCtx
{
    const char *id;
    const char (*path_callback)[CLI_PATH_MAX_LEN];
    unsigned int count;
    const CliFound *found_read;
};

struct CliToolchainsShellCtx
{
    const char *shell;
    char *path;
    unsigned long long size;
};

struct CliToolchainsTextCtx
{
    const char *text;
    char *out;
    unsigned long long capacity;
};

struct CliToolchainsCtx
{
    const char *name;
    char *path;
};

struct CliToolchainsPatternCtx
{
    const char *pattern;
    const char *name;
};

struct CliToolchainsACtx
{
    const char *a;
    const char *b;
};

typedef struct
{
    int (*matches)(const CliToolchainsPatternCtx *args);
} CliToolchainsPatternNs;
EMBED_TABLE_LAYOUT(CliToolchainsPatternNs, matches);
EMBED_TABLE_STORAGE CliToolchainsPatternNs cli_toolchains_pattern_internal EMBED_UNUSED = {
    .matches = cli_toolchains_matches,
};

typedef struct
{
    int (*natural)(const CliToolchainsACtx *args);
} CliToolchainsANs;
EMBED_TABLE_LAYOUT(CliToolchainsANs, natural);
EMBED_TABLE_STORAGE CliToolchainsANs cli_toolchains_a_internal EMBED_UNUSED = {
    .natural = cli_toolchains_natural,
};

typedef struct
{
    int (*path_of_shell)(const CliToolchainsShellCtx *args);
} CliToolchainsShellNs;
EMBED_TABLE_LAYOUT(CliToolchainsShellNs, path_of_shell);
EMBED_TABLE_STORAGE CliToolchainsShellNs cli_toolchains_shell_internal EMBED_UNUSED = {
    .path_of_shell = cli_toolchains_path_of_shell,
};

typedef struct
{
    int (*found_cache)(const CliToolchainsIdCtx *args);
} CliToolchainsIdNsTable;
EMBED_TABLE_LAYOUT(CliToolchainsIdNsTable, found_cache);
EMBED_TABLE_STORAGE CliToolchainsIdNsTable cli_toolchains_id_internal EMBED_UNUSED = {
    .found_cache = cli_toolchains_found_cache,
};

typedef struct
{
    void (*found_append)(const CliToolchainsIdPathCtx *args);
} CliToolchainsIdPathNsTable;
EMBED_TABLE_LAYOUT(CliToolchainsIdPathNsTable, found_append);
EMBED_TABLE_STORAGE CliToolchainsIdPathNsTable cli_toolchains_id_path_internal EMBED_UNUSED = {
    .found_append = cli_toolchains_found_append,
};

typedef struct
{
    int (*file)(const CliToolchainsCtx *args);
} CliToolchainsNsTable;
EMBED_TABLE_LAYOUT(CliToolchainsNsTable, file);
EMBED_TABLE_STORAGE CliToolchainsNsTable cli_toolchains_internal EMBED_UNUSED = {
    .file = cli_toolchains_file,
};

typedef struct
{
    unsigned int (*file_read)(const CliToolchainsJsonCtx *args);
    void (*find_release)(const CliToolchainsJsonCtx *args);
} CliToolchainsJsonNsTable;
EMBED_TABLE_LAYOUT(CliToolchainsJsonNsTable, file_read, find_release);
EMBED_TABLE_STORAGE CliToolchainsJsonNsTable cli_toolchains_json_internal EMBED_UNUSED = {
    .file_read = cli_toolchains_file_read,
    .find_release = cli_toolchains_find_release,
};

typedef struct
{
    int (*file_save)(const CliToolchainsPathCtx *args);
} CliToolchainsPathNs;
EMBED_TABLE_LAYOUT(CliToolchainsPathNs, file_save);
EMBED_TABLE_STORAGE CliToolchainsPathNs cli_toolchains_path_internal EMBED_UNUSED = {
    .file_save = cli_toolchains_file_save,
};

typedef struct
{
    void (*trim)(const CliToolchainsTextCtx *args);
} CliToolchainsTextNs;
EMBED_TABLE_LAYOUT(CliToolchainsTextNs, trim);
EMBED_TABLE_STORAGE CliToolchainsTextNs cli_toolchains_text_internal EMBED_UNUSED = {
    .trim = cli_toolchains_trim,
};

#endif
