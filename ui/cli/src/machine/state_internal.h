// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state_internal.h: what state.c and state_save.c share: where an entry is stored, a file of them read, and the JSON
// files of a folder
#ifndef STATE_INTERNAL_H
#define STATE_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/pieces.h"
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

// the most bytes of a file of entries read, the most JSON files of a folder, and the bytes of a piece a text is written
// in before the piece is stored in a cell of its own
#define CLI_STATE_BYTES (1u << 22u)

#define CLI_STATE_FILES 4096u

#define CLI_STATE_PIECE 4096u

// The JSON files of a folder, by name, in order, in a cell of their count
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
} CliJsonFiles;

// A text written in pieces, each piece's cell taken as it fills
typedef struct
{
    CliPieces pieces;
    VerbaLinea line;
    char piece[CLI_STATE_PIECE];
} CliStateWriting;

// where the entry `key` is stored; the object the file at `path` contains, read as JSON with comments and trailing
// commas, into `json`, a tree of its own sized to the file's tokens and bytes and `nodes_more` nodes and `text_more`
// bytes over to begin with, growing past them as entries are put, an empty one where it is not there or does not read:
// its root, CLI_JSON_NONE where the tree's cell is refused, the tree given back with cli_json_given either way; the
// JSON files of `folder` into `files`, by name, in order, and the cell they are stored in given back; a text begun in
// pieces, and joined into a cell of its size, the pieces given back: the cell, its size into `size`, NULL where it is
// not whole (state.c)
unsigned int cli_state_place(const char *key);
unsigned int cli_state_map_read(CliJson *json, const char *path, unsigned int nodes_more, unsigned long long text_more);
void cli_state_json_files(const char *folder, CliJsonFiles *files);
void cli_state_json_files_given(CliJsonFiles *files);
void cli_state_writing_begun(CliStateWriting *writing);
char *cli_state_writing_joined(CliStateWriting *writing, unsigned long long *size);

#endif
