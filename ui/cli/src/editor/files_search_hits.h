// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.h: the lines a search's workers find, each worker's in a store only it writes until it ends, and
// the stores read back as one list in the order of the files and of the lines in each (files_search_hits.c). A worker
// takes its files in their order and reads each from its start, so the lines in its store stand in that order, and no
// file's lines stand in two stores
#ifndef FILES_SEARCH_HITS_H
#define FILES_SEARCH_HITS_H
#include "embed_dispatch_layout.h"
#include "files_internal.h"

typedef struct CliFilesSearchHitsCfg CliFilesSearchHitsCfg;

// the bytes of each cell a store takes, which fits many lines of the most bytes one takes
#define CLI_HIT_CELL_BYTES (64ULL * 1024ULL)

// the cells the lines a search finds are stored in, counted, each released as the search ends
extern CellOwner g_cli_search_cells;

// A line found, stored until the search ends: the line its worker stored after it, the bytes of its text, which stand
// in the cell directly after it, the line and column, its file's place, and its text's size
typedef struct CliSearchHit CliSearchHit;
struct CliSearchHit
{
    CliSearchHit *next;
    const unsigned char *text;
    unsigned long long line;
    unsigned long long col;
    unsigned int file;
    unsigned int size;
};

// A cell of lines, which stand directly after this: the cell its store took after it and the cell's bytes in use
typedef struct CliHitCell CliHitCell;
struct CliHitCell
{
    CliHitCell *next;
    unsigned long long used;
};

// One worker's lines: the first still to be read back, the last stored, how many were stored, the cells they stand in,
// first and last, and whether the cells stand after the search, in the persistent tier, for the window's rows to point
// into. All zero is an empty store
typedef struct
{
    CliSearchHit *first;
    CliSearchHit *last;
    unsigned int lives;
    CliHitCell *cell_first;
    CliHitCell *cell_last;
    int standing;
} CliSearchHits;

// a line stored last in the store `hits`, its text cut to CLI_HIT_BYTES. A store with CLI_HITS_MAX lines lets the line
// go, since it comes after each of them, and so does a store whose take the cellblock refuses
void cli_hits_put(const CliFilesSearchHitsCfg *args);
// the first line left among the `stores` stores from `hits`, taken off its store: NULL where none is left
const CliSearchHit *cli_hits_next(const CliFilesSearchHitsCfg *args);
// the cells of the `stores` stores from `hits` released, each store empty again

struct CliFilesSearchHitsCfg
{
    CliSearchHits *hits;
    unsigned int stores;
    unsigned int file;
    unsigned long long line;
    unsigned long long col;
    const unsigned char *text;
    unsigned long long size;
};

// the lines the last search left standing, where it was asked to (files_search.c): how many; the line `at`, in the
// order of the files and of the lines in each; how many files they were found in, and the first line of the file
// `file` among them, the count where `file` is past the last; a standing line's file's path from the tree's top; and
// the lines let go with the files they were found in, their cells released
unsigned int cli_search_standing_count(void);
const CliSearchHit *cli_search_standing_hit(unsigned int at);
unsigned int cli_search_standing_files(void);
unsigned int cli_search_standing_file_first(unsigned int file);
const char *cli_search_standing_path(const CliSearchHit *hit);
void cli_search_standing_free(void);

typedef struct
{
    void (*hits_put)(const CliFilesSearchHitsCfg *args);
    const CliSearchHit *(*hits_next)(const CliFilesSearchHitsCfg *args);
} CliFilesSearchHitsNs;
EMBED_TABLE_LAYOUT(CliFilesSearchHitsNs, hits_put, hits_next);
EMBED_TABLE_STORAGE CliFilesSearchHitsNs cli_files_search_hits EMBED_UNUSED = {
    .hits_put = cli_hits_put,
    .hits_next = cli_hits_next,
};

#endif
