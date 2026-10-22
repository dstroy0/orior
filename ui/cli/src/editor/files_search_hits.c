// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.c: the lines a search finds, held in cells whose room doubles as they are found, sorted by their
// files and their places, the first CLI_HITS_MOST kept where more are found than are held
#include "files_search_hits.h"

CliCellsTally g_cli_search_cells = CLI_CELLS_TALLY("cli_files_search");

// whether the line in slot `one` comes before the line in slot `other`: by its file, then its place in it
static int hits_before(const CliSearchHits *hits, unsigned int one, unsigned int other)
{
    const CliHeldHit *const left = &hits->pool[one];
    const CliHeldHit *const right = &hits->pool[other];
    return (left->file < right->file) || ((left->file == right->file) && (left->order < right->order));
}

void cli_hits_sorted(CliSearchHits *hits)
{
    unsigned int *from = hits->live;
    unsigned int *into = hits->spare;
    const unsigned int count = hits->lives;
    for (unsigned int width = 1u; width < count; width *= 2u)
    {
        for (unsigned int start = 0u; start < count; start += 2u * width)
        {
            const unsigned int middle = ((start + width) < count) ? (start + width) : count;
            const unsigned int end = ((middle + width) < count) ? (middle + width) : count;
            unsigned int left = start;
            unsigned int right = middle;
            for (unsigned int at = start; at < end; at += 1u)
            {
                const int take_left =
                    (left < middle) && ((right >= end) || !hits_before(hits, from[right], from[left]));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        unsigned int *const turned = from;
        from = into;
        into = turned;
    }
    if (from != hits->live)
    {
        memor.cpy(&(MemoriaCfg){.dst = hits->live, .src = from, .bytes = count * sizeof(unsigned int)});
    }
}

void cli_hits_given(CliSearchHits *hits)
{
    cli_cell_given_by(&g_cli_search_cells, hits->pool);
    cli_cell_given_by(&g_cli_search_cells, hits->live);
    cli_cell_given_by(&g_cli_search_cells, hits->spare);
    cli_cell_given_by(&g_cli_search_cells, hits->free);
    *hits = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0u, 0u};
}

// the room doubled, the slots it adds free: 1, or 0 where it holds the most or a cell is refused
static int hits_grown(CliSearchHits *hits)
{
    const unsigned int was = hits->room;
    const unsigned int doubled = (was == 0u) ? CLI_POOL_FIRST : (was * 2u);
    const unsigned int room = (doubled < CLI_POOL_MOST) ? doubled : CLI_POOL_MOST;
    const unsigned long long slots = (unsigned long long)room * sizeof(unsigned int);
    CliHeldHit *const pool =
        (was < CLI_POOL_MOST)
            ? cli_cell_taken_by(&g_cli_search_cells, (unsigned long long)room * sizeof(CliHeldHit), "the lines found")
            : NULL;
    unsigned int *const live =
        (pool != NULL) ? cli_cell_taken_by(&g_cli_search_cells, slots, "the slots of the lines found") : NULL;
    unsigned int *const spare =
        (live != NULL) ? cli_cell_taken_by(&g_cli_search_cells, slots, "the room the lines are sorted in") : NULL;
    unsigned int *const vacant =
        (spare != NULL) ? cli_cell_taken_by(&g_cli_search_cells, slots, "the slots free for lines") : NULL;
    if (vacant == NULL)
    {
        cli_cell_given_by(&g_cli_search_cells, pool);
        cli_cell_given_by(&g_cli_search_cells, live);
        cli_cell_given_by(&g_cli_search_cells, spare);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = pool, .src = hits->pool, .bytes = (size_t)was * sizeof(CliHeldHit)});
    memor.cpy(&(MemoriaCfg){.dst = live, .src = hits->live, .bytes = (size_t)hits->lives * sizeof(unsigned int)});
    memor.cpy(&(MemoriaCfg){.dst = vacant, .src = hits->free, .bytes = (size_t)hits->frees * sizeof(unsigned int)});
    const unsigned int lives = hits->lives;
    unsigned int frees = hits->frees;
    cli_hits_given(hits);
    // the slots added are put free from the last, so the first of them is the next taken
    for (unsigned int slot = room; slot > was; frees += 1u)
    {
        slot -= 1u;
        vacant[frees] = slot;
    }
    *hits = (CliSearchHits){pool, live, lives, spare, vacant, frees, room};
    return 1;
}

CliHeldHit *cli_hits_slot(CliSearchHits *hits)
{
    if ((hits->frees == 0u) && !hits_grown(hits) && (hits->lives > CLI_HITS_MOST))
    {
        cli_hits_sorted(hits);
        for (unsigned int at = CLI_HITS_MOST; at < hits->lives; at += 1u)
        {
            hits->free[hits->frees] = hits->live[at];
            hits->frees += 1u;
        }
        hits->lives = CLI_HITS_MOST;
    }
    if (hits->frees == 0u)
    {
        return NULL;
    }
    hits->frees -= 1u;
    const unsigned int slot = hits->free[hits->frees];
    hits->live[hits->lives] = slot;
    hits->lives += 1u;
    return &hits->pool[slot];
}
