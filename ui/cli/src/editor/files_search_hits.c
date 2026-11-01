// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.c: the lines a search finds, stored in one cell taken at their most, sorted by their files and
// their places, the first CLI_HITS_MOST retained where more are found than are stored
#include "files_search_hits.h"

// whether the line in slot `one` comes before the line in slot `other`: by its file, then its place in it
static int hits_before(const CliSearchHits *hits, unsigned int one, unsigned int other)
{
    const CliSearchHit *const left = &hits->pool[one];
    const CliSearchHit *const right = &hits->pool[other];
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
    g_cli_cells->persistent_buf_release(hits->pool, &g_cli_search_cells);
    *hits = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0u};
}

void cli_hits_taken(CliSearchHits *hits)
{
    const unsigned long long pool_bytes = (unsigned long long)CLI_POOL_MOST * sizeof(CliSearchHit);
    const unsigned long long list_bytes = (unsigned long long)CLI_POOL_MOST * sizeof(unsigned int);
    char *const cell = g_cli_cells->temporary_buf_alloc(pool_bytes + (3ull * list_bytes), &g_cli_search_cells);
    *hits = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0u};
    if (cell == NULL)
    {
        return;
    }
    hits->pool = (CliSearchHit *)(void *)cell;
    hits->live = (unsigned int *)(void *)(cell + pool_bytes);
    hits->spare = hits->live + CLI_POOL_MOST;
    hits->free = hits->spare + CLI_POOL_MOST;
    // the slots are put free from the last, so the first is the next taken
    for (unsigned int entry = CLI_POOL_MOST; entry > 0u; hits->frees += 1u)
    {
        entry -= 1u;
        hits->free[hits->frees] = entry;
    }
}

CliSearchHit *cli_hits_take(CliSearchHits *hits)
{
    if ((hits->frees == 0u) && (hits->lives > CLI_HITS_MOST))
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
    const unsigned int entry = hits->free[hits->frees];
    hits->live[hits->lives] = entry;
    hits->lives += 1u;
    return &hits->pool[entry];
}
