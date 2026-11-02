// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_hits.c: the lines a search finds, stored in one cell taken at their most, sorted by their files and
// their places, the first CLI_HITS_MAX retained where more are found than are stored
#include "files_search_hits.h"

typedef struct
{
    const CliSearchHits *hits;
    unsigned int one;
    unsigned int other;
} FilesSearchHitsCtx;

// whether the line in entry `one` comes before the line in entry `other`: by its file, then its place in it
EMBED_INLINE int hits_before(const FilesSearchHitsCtx *args)
{
    const CliSearchHit *const left = &args->hits->pool[args->one];
    const CliSearchHit *const right = &args->hits->pool[args->other];
    return (left->file < right->file) || ((left->file == right->file) && (left->order < right->order));
}

void cli_hits_sort(const CliFilesSearchHitsCfg *args)
{
    unsigned int *from = args->hits->live;
    unsigned int *into = args->hits->reserve;
    const unsigned int count = args->hits->lives;
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
                    (left < middle) &&
                    ((right >= end) || !EMBED_CALL(hits_before, FilesSearchHitsCtx, .hits = args->hits,
                                                   .one = from[right], .other = from[left]));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        unsigned int *const turned = from;
        from = into;
        into = turned;
    }
    if (from != args->hits->live)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->hits->live, .src = from, .bytes = count * sizeof(unsigned int)});
    }
}

void cli_hits_free(const CliFilesSearchHitsCfg *args)
{
    CliSearchHits *hits = args->hits;
    g_cli_cells->persistent_buf_release(hits->pool, &g_cli_search_cells);
    *hits = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0u};
}

void cli_hits_init(const CliFilesSearchHitsCfg *args)
{
    CliSearchHits *hits = args->hits;
    const unsigned long long pool_bytes = (unsigned long long)CLI_POOL_MAX * sizeof(CliSearchHit);
    const unsigned long long list_bytes = (unsigned long long)CLI_POOL_MAX * sizeof(unsigned int);
    char *const cell = g_cli_cells->temporary_buf_alloc(pool_bytes + (3ull * list_bytes), &g_cli_search_cells);
    *hits = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0u};
    if (cell == NULL)
    {
        return;
    }
    hits->pool = (CliSearchHit *)(void *)cell;
    hits->live = (unsigned int *)(void *)(cell + pool_bytes);
    hits->reserve = hits->live + CLI_POOL_MAX;
    hits->free = hits->reserve + CLI_POOL_MAX;
    // the entries are put free from the last, so the first is the next taken
    for (unsigned int entry = CLI_POOL_MAX; entry > 0u; hits->frees += 1u)
    {
        entry -= 1u;
        hits->free[hits->frees] = entry;
    }
}

CliSearchHit *cli_hits_alloc(const CliFilesSearchHitsCfg *args)
{
    if ((args->hits->frees == 0u) && (args->hits->lives > CLI_HITS_MAX))
    {
        cli_files_search_hits.hits_sort(args);
        for (unsigned int at = CLI_HITS_MAX; at < args->hits->lives; at += 1u)
        {
            args->hits->free[args->hits->frees] = args->hits->live[at];
            args->hits->frees += 1u;
        }
        args->hits->lives = CLI_HITS_MAX;
    }
    if (args->hits->frees == 0u)
    {
        return NULL;
    }
    args->hits->frees -= 1u;
    const unsigned int entry = args->hits->free[args->hits->frees];
    args->hits->live[args->hits->lives] = entry;
    args->hits->lives += 1u;
    return &args->hits->pool[entry];
}
