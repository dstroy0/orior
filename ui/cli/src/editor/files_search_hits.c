// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.c: a worker's lines stored in cells taken as they are found, and the stores of every worker read
// back as one list in the order of the files. Nothing here is shared while a store is written: only its worker writes
// it, and only the search reads it, after that worker ends
#include "files_search_hits.h"

EMBED_STATIC_ASSERT((sizeof(CliHitCell) + sizeof(CliSearchHit) + CLI_HIT_BYTES + _Alignof(CliSearchHit)) <=
                        CLI_HIT_CELL_BYTES,
                    "CLI_HIT_CELL_BYTES must fit the longest line a search stores");
EMBED_STATIC_ASSERT((sizeof(CliHitCell) % _Alignof(CliSearchHit)) == 0u,
                    "the first line in a cell must are on a line's alignment");

typedef struct
{
    CliSearchHits *hits;
    unsigned long long need;
} FilesSearchHitsCtx;

// the store's last cell where `need` bytes fit in it, or a new cell put after it: NULL where the cellblock refuses
EMBED_INLINE CliHitCell *hits_cell(const FilesSearchHitsCtx *args)
{
    CliSearchHits *const hits = args->hits;
    CliHitCell *const last = hits->cell_last;
    if ((last != NULL) && ((CLI_HIT_CELL_BYTES - last->used) >= args->need))
    {
        return last;
    }
    // a store lasts the search: its cells are the temporary tier's, given back as the lines are read out
    CliHitCell *const cell = g_cli_guard->temporary_buf_alloc(CLI_HIT_CELL_BYTES, g_cli_search_owner);
    if (cell == NULL)
    {
        return NULL;
    }
    *cell = (CliHitCell){NULL, sizeof(CliHitCell)};
    if (last == NULL)
    {
        hits->cell_first = cell;
    }
    else
    {
        last->next = cell;
    }
    hits->cell_last = cell;
    return cell;
}

void cli_hits_put(const CliFilesSearchHitsCfg *args)
{
    CliSearchHits *const hits = args->hits;
    if (hits->lives >= CLI_HITS_MAX)
    {
        return;
    }
    // a line's bytes are at max CLI_HIT_BYTES, which an unsigned int stores
    const unsigned int copied_len = (unsigned int)((args->size < CLI_HIT_BYTES) ? args->size : CLI_HIT_BYTES);
    // the next line starts on the boundary a line is aligned to
    const unsigned long long aligned = (unsigned long long)_Alignof(CliSearchHit) - 1ULL;
    const unsigned long long need = (sizeof(CliSearchHit) + copied_len + aligned) & ~aligned;
    CliHitCell *const cell = EMBED_CALL(hits_cell, FilesSearchHitsCtx, .hits = hits, .need = need);
    if (cell == NULL)
    {
        return;
    }
    // the cell's used bytes are a multiple of a line's alignment, so the place is aligned for a line
    CliSearchHit *const hit = (CliSearchHit *)(void *)((unsigned char *)cell + cell->used);
    unsigned char *const text = (unsigned char *)(hit + 1);
    cell->used += need;
    *hit = (CliSearchHit){NULL, text, args->line, args->col, args->file, copied_len};
    memor.cpy(&(MemoriaCfg){.dst = text, .src = args->text, .bytes = copied_len});
    if (hits->last == NULL)
    {
        hits->first = hit;
    }
    else
    {
        hits->last->next = hit;
    }
    hits->last = hit;
    hits->lives += 1u;
}

const CliSearchHit *cli_hits_next(const CliFilesSearchHitsCfg *args)
{
    CliSearchHits *least = NULL;
    for (unsigned int store = 0u; store < args->stores; store += 1u)
    {
        CliSearchHits *const hits = &args->hits[store];
        if ((hits->first != NULL) && ((least == NULL) || (hits->first->file < least->first->file)))
        {
            least = hits;
        }
    }
    if (least == NULL)
    {
        return NULL;
    }
    const CliSearchHit *const hit = least->first;
    least->first = hit->next;
    return hit;
}
