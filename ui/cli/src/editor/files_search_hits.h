// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.h: the lines a search finds, stored until it ends in one cell taken at their most, the first
// CLI_HITS_MOST in the order of the files and of the lines in each retained (files_search_hits.c)
#ifndef FILES_SEARCH_HITS_H
#define FILES_SEARCH_HITS_H

#include "files_internal.h"

// the most lines stored while a search runs
#define CLI_POOL_MOST (CLI_HITS_MOST * 4u)

// the cells the lines a search finds are stored in, counted, each given back as the search ends
extern CellOwner g_cli_search_cells;

// A line found, stored until the search ends: its file's place, its place among its file's, the line and column, and
// its text's bytes
typedef struct
{
    unsigned int file;
    unsigned int order;
    unsigned long long line;
    unsigned long long col;
    unsigned int size;
    unsigned char text[CLI_HIT_BYTES];
} CliSearchHit;

// The lines stored: each in a slot of `pool`, the slots in use in `live`, `lives` of them, the room they are sorted
// through, and the slots free, `frees` of them; each list CLI_POOL_MOST slots
typedef struct
{
    CliSearchHit *pool;
    unsigned int *live;
    unsigned int lives;
    unsigned int *spare;
    unsigned int *free;
    unsigned int frees;
} CliSearchHits;

// the cell taken, every slot free, and none where it is refused
void cli_hits_taken(CliSearchHits *hits);
// a slot taken for one line more and put among the live, under the search's lock: the line's room, or NULL where none
// is left. Where the lines stored are the most, those after the first CLI_HITS_MOST are let go first: no line found
// later can bring one of them back among the first
CliSearchHit *cli_hits_take(CliSearchHits *hits);
// the live slots sorted by their lines' files and places
void cli_hits_sorted(CliSearchHits *hits);
// the cells given back, the lines stored let go
void cli_hits_given(CliSearchHits *hits);

#endif
