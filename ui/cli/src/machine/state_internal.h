// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state_internal.h: what state.c and state_save.c share: where an entry is stored, a file of them read, and the JSON
// files of a folder
#ifndef STATE_INTERNAL_H
#define STATE_INTERNAL_H
typedef struct CliStateJsonCtx CliStateJsonCtx;
typedef struct CliStateFolderCtx CliStateFolderCtx;
typedef struct CliStateCtx CliStateCtx;

#include "../command_line/cells.h"
#include "embed_dispatch_layout.h"
#include "state.h"

// the cells the entries are stored and read back in (state.c)
extern CellOwner g_cli_state_cells;

// where an entry is stored
enum
{
    CLI_STATE_SETTINGS,
    CLI_STATE_FILE,
    CLI_STATE_THEMES,
    CLI_STATE_BACKUP
};

// the most bytes of a file of entries read, and so of one written, and the most JSON files of a folder
#define CLI_STATE_BYTES (1u << 22u)

#define CLI_STATE_FILES 4096u

// The JSON files of a folder, by name, in order, in a cell of their count
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
} CliJsonFiles;

// where the entry `key` is stored; the object the file at `path` contains, read as JSON with comments and trailing
// commas, into `json`, a tree of its own sized to the file's tokens and bytes and `nodes_more` nodes and `text_more`
// bytes over to begin with, an empty one where it is not there or does not read: its root, CLI_JSON_NONE where the
// tree's cell is refused, the tree given back with cli_json_free either way; and the JSON files of `folder` into
// `files`, by name, in order, and the cell they are stored in given back (state.c)
unsigned int cli_state_place(const CliStateCtx *args);
unsigned int cli_state_map_read(const CliStateJsonCtx *args);
void cli_state_json_files(const CliStateFolderCtx *args);
void cli_state_json_files_free(const CliStateFolderCtx *args);

struct CliStateJsonCtx
{
    CliJson *json;
    const char *path;
    unsigned int nodes_more;
    unsigned long long text_more;
};

struct CliStateFolderCtx
{
    const char *folder;
    CliJsonFiles *files;
};

struct CliStateCtx
{
    const char *key;
};

typedef struct
{
    unsigned int (*place)(const CliStateCtx *args);
} CliStateNsTable;
EMBED_TABLE_LAYOUT(CliStateNsTable, place);
EMBED_TABLE_STORAGE CliStateNsTable cli_state_internal EMBED_UNUSED = {
    .place = cli_state_place,
};

typedef struct
{
    unsigned int (*map_read)(const CliStateJsonCtx *args);
} CliStateJsonNsTable;
EMBED_TABLE_LAYOUT(CliStateJsonNsTable, map_read);
EMBED_TABLE_STORAGE CliStateJsonNsTable cli_state_json_internal EMBED_UNUSED = {
    .map_read = cli_state_map_read,
};

typedef struct
{
    void (*json_files)(const CliStateFolderCtx *args);
    void (*json_files_free)(const CliStateFolderCtx *args);
} CliStateFolderNsTable;
EMBED_TABLE_LAYOUT(CliStateFolderNsTable, json_files, json_files_free);
EMBED_TABLE_STORAGE CliStateFolderNsTable cli_state_folder_internal EMBED_UNUSED = {
    .json_files = cli_state_json_files,
    .json_files_free = cli_state_json_files_free,
};

#endif
