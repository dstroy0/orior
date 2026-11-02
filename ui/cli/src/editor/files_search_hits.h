// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.h: the lines a search finds, stored until it ends in one cell taken at their most, the first
// CLI_HITS_MAX in the order of the files and of the lines in each retained (files_search_hits.c)
#ifndef FILES_SEARCH_HITS_H
#define FILES_SEARCH_HITS_H
typedef struct CliFilesSearchHitsCfg CliFilesSearchHitsCfg;

#include "embed_dispatch_layout.h"
#include "files_internal.h"

// the most lines stored while a search runs
#define CLI_POOL_MAX (CLI_HITS_MAX * 4u)

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

// The lines stored: each in a entry of `pool`, the entries in use in `live`, `lives` of them, the capacity they are
// sorted through, and the entries free, `frees` of them; each list CLI_POOL_MAX entries
typedef struct
{
    CliSearchHit *pool;
    unsigned int *live;
    unsigned int lives;
    unsigned int *reserve;
    unsigned int *free;
    unsigned int frees;
} CliSearchHits;

// the cell taken, every entry free, and none where it is refused
void cli_hits_init(const CliFilesSearchHitsCfg *args);
// a entry taken for one line more and put among the live, under the search's lock: the line's capacity, or NULL where
// none is left. Where the lines stored are the most, those after the first CLI_HITS_MAX are let go first: no line found
// later can bring one of them back among the first
CliSearchHit *cli_hits_alloc(const CliFilesSearchHitsCfg *args);
// the live entries sorted by their lines' files and places
void cli_hits_sort(const CliFilesSearchHitsCfg *args);
// the cells given back, the lines stored let go
void cli_hits_free(const CliFilesSearchHitsCfg *args);

struct CliFilesSearchHitsCfg
{
    CliSearchHits *hits;
};

typedef struct
{
    void (*hits_init)(const CliFilesSearchHitsCfg *args);
    CliSearchHit *(*hits_alloc)(const CliFilesSearchHitsCfg *args);
    void (*hits_sort)(const CliFilesSearchHitsCfg *args);
    void (*hits_free)(const CliFilesSearchHitsCfg *args);
} CliFilesSearchHitsNs;
EMBED_TABLE_LAYOUT(CliFilesSearchHitsNs, hits_init, hits_alloc, hits_sort, hits_free);
EMBED_TABLE_STORAGE CliFilesSearchHitsNs cli_files_search_hits EMBED_UNUSED = {
    .hits_init = cli_hits_init,
    .hits_alloc = cli_hits_alloc,
    .hits_sort = cli_hits_sort,
    .hits_free = cli_hits_free,
};

#endif
