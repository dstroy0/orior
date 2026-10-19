// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept_internal.h: what kept.c and kept_keep.c share: where an entry is kept, a file of them read, and the JSON files
// of a folder
#ifndef KEPT_INTERNAL_H
#define KEPT_INTERNAL_H

#include "kept.h"

// where an entry is kept
enum
{
    CLI_KEPT_SETTINGS,
    CLI_KEPT_STATE,
    CLI_KEPT_THEMES,
    CLI_KEPT_BACKUP
};

// the most bytes of a file of entries, and the most nodes of its tree, and the most JSON files of a folder
#define CLI_KEPT_BYTES (1u << 22u)

#define CLI_KEPT_NODES (1u << 17u)

#define CLI_KEPT_FILES 4096u

// The JSON files of a folder, by name, in order
typedef struct
{
    char name[CLI_KEPT_FILES][CLI_NAME_LONGEST];
    unsigned int count;
} CliJsonFiles;

// where the entry `key` is kept; the object the file at `path` holds, read as JSON with comments and trailing commas,
// into `json`, whose storage it begins, an empty one where it is not there or does not read; and the JSON files of
// `folder` into `files`, by name, in order (kept.c)
unsigned int cli_kept_place(const char *key);
unsigned int cli_kept_map_read(CliJson *json, CliJsonNode *nodes, char *text, const char *path);
void cli_kept_json_files(const char *folder, CliJsonFiles *files);

#endif
