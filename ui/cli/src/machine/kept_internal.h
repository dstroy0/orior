// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept_internal.h: what kept.c and kept_keep.c share: where an entry is kept, a file of them read, and the JSON files
// of a folder
#ifndef KEPT_INTERNAL_H
#define KEPT_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "kept.h"

// the cells the entries are kept and read back in (kept.c)
extern CliCellsTally g_cli_kept_cells;

// where an entry is kept
enum
{
    CLI_KEPT_SETTINGS,
    CLI_KEPT_STATE,
    CLI_KEPT_THEMES,
    CLI_KEPT_BACKUP
};

// the most bytes of a file of entries read, the most JSON files of a folder, and the bytes of a piece a text is written
// in before the piece is held in a cell of its own
#define CLI_KEPT_BYTES (1u << 22u)

#define CLI_KEPT_FILES 4096u

#define CLI_KEPT_PIECE 4096u

// The JSON files of a folder, by name, in order, in a cell of their count
typedef struct
{
    char (*name)[CLI_NAME_LONGEST];
    unsigned int count;
} CliJsonFiles;

// A text written in pieces, each piece's cell taken as it fills
typedef struct
{
    CliPieces pieces;
    CliLine line;
    char piece[CLI_KEPT_PIECE];
} CliKeptWriting;

// where the entry `key` is kept; the object the file at `path` holds, read as JSON with comments and trailing commas,
// into `json`, a tree of its own sized to the file's tokens and bytes and `nodes_more` nodes and `text_more` bytes
// over for what is put in it after, an empty one where it is not there or does not read: its root, CLI_JSON_NONE
// where the tree's cell is refused, the tree given back with cli_json_given either way; the JSON files of `folder`
// into `files`, by name, in order, and the cell they are held in given back; a text begun in pieces, and joined into a
// cell of its size, the pieces given back: the cell, its size into `size`, NULL where it is not whole (kept.c)
unsigned int cli_kept_place(const char *key);
unsigned int cli_kept_map_read(CliJson *json, const char *path, unsigned int nodes_more, unsigned long long text_more);
void cli_kept_json_files(const char *folder, CliJsonFiles *files);
void cli_kept_json_files_given(CliJsonFiles *files);
void cli_kept_writing_begun(CliKeptWriting *writing);
char *cli_kept_writing_joined(CliKeptWriting *writing, unsigned long long *size);

#endif
