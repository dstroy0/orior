// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_cells.c: the files' cells and the lists built of them: a list's paths and places taken at the sizes a count of
// them sets, put, sorted and released; and the files of the tree a reader picks for one call, by their places in it.
// The cells are the one cellblock's, counted to the files' own mark, which their report writes
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "files_internal.h"

void cli_files_cells_usage_write(const CliFilesRootOutDirCfg *args)
{
    cli_pool_owner_usage_write(g_cli_files_views_owner, args->out);
}

void *cli_files_cell_alloc(const CliFilesBytesCtx *args)
{
    return g_cli_guard->persistent_buf_alloc(args->bytes, g_cli_files_views_owner);
}

void *cli_files_work_alloc(const CliFilesBytesCtx *args)
{
    return g_cli_guard->temporary_buf_alloc(args->bytes, g_cli_files_views_owner);
}

const char *cli_files_at(const CliFilesCfg *args)
{
    return args->files->text + args->files->start[args->index];
}

int cli_files_append(const CliFilesPathLengthCtx *args)
{
    if ((args->files->count >= args->files->most) ||
        ((args->files->used + args->length + 1ULL) > args->files->text_max))
    {
        return 0;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int stores
    args->files->start[args->files->count] = (unsigned int)args->files->used;
    memor.cpy(&(MemoriaCfg){.dst = args->files->text + args->files->used, .src = args->path, .bytes = args->length});
    args->files->text[args->files->used + args->length] = '\0';
    args->files->used += args->length + 1ULL;
    args->files->count += 1u;
    return 1;
}

unsigned int cli_files_index(const CliFilesPathTypeCfg *args)
{
    unsigned int low = 0u;
    unsigned int high = args->files->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order =
            EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                       .one = EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->files, .index = middle),
                       .other_pointer = args->path);
        if (order == 0)
        {
            return middle;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return args->files->count;
}

int cli_files_contains(const CliFilesPathTypeCfg *args)
{
    return EMBED_CALL(cli_files_path_type.index, CliFilesPathTypeCfg, .files = args->files, .path = args->path) <
           args->files->count;
}

typedef struct
{
    CliFiles *files;
    unsigned int *reserve;
} FilesCellsCtx;
// the places of `files` sorted by their paths' bytes, runs of one, two, four and on merged in turn between the places
// and `reserve`, sized for as many
EMBED_INLINE void files_merge(const FilesCellsCtx *args)
{
    unsigned int *from = args->files->start;
    unsigned int *to = args->reserve;
    for (unsigned int width = 1u; width < args->files->count; width *= 2u)
    {
        for (unsigned int left = 0u; left < args->files->count; left += 2u * width)
        {
            const unsigned int middle = ((left + width) < args->files->count) ? (left + width) : args->files->count;
            const unsigned int end =
                ((left + (2u * width)) < args->files->count) ? (left + (2u * width)) : args->files->count;
            unsigned int one = left;
            unsigned int other = middle;
            for (unsigned int put = left; put < end; put += 1u)
            {
                const int first =
                    (other >= end) ||
                    ((one < middle) &&
                     (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->files->text + from[one],
                                 .other_pointer = args->files->text + from[other]) <= 0));
                to[put] = first ? from[one] : from[other];
                one += first ? 1u : 0u;
                other += first ? 0u : 1u;
            }
        }
        unsigned int *const src = from;
        from = to;
        to = src;
    }
    if (from != args->files->start)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->files->start, .src = from, .bytes = args->files->count * sizeof(unsigned int)});
    }
}

int cli_files_sort(const CliFilesBytesCtx *args)
{
    if (args->files->count == 0u)
    {
        return 1;
    }
    unsigned int *const reserve = EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx,
                                             .bytes = (unsigned long long)args->files->count * sizeof(unsigned int));
    if (reserve == NULL)
    {
        return 0;
    }
    EMBED_CALL(files_merge, FilesCellsCtx, .files = args->files, .reserve = reserve);
    g_cli_guard->persistent_buf_release(reserve, g_cli_files_views_owner);
    // each path listed once
    unsigned int sorted_count = 0u;
    for (unsigned int at = 0u; at < args->files->count; at += 1u)
    {
        if ((sorted_count == 0u) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = args->files->text + args->files->start[sorted_count - 1u],
                                           .other = args->files->text + args->files->start[at],
                                           .cap = CLI_POOL_BYTES})))
        {
            args->files->start[sorted_count] = args->files->start[at];
            sorted_count += 1u;
        }
    }
    args->files->count = sorted_count;
    return 1;
}

void cli_files_clear(const CliFilesBytesCtx *args)
{
    CliFiles *files = args->files;
    g_cli_guard->persistent_buf_release(files->start, g_cli_files_views_owner);
    g_cli_guard->persistent_buf_release(files->text, g_cli_files_views_owner);
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ULL, 0ULL};
}

int cli_files_init(const CliFilesBytesCtx *args)
{
    CliFiles *files = args->files;
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ULL, 0ULL};
    if (args->count == 0ULL)
    {
        return 1;
    }
    // each bounded by its limit, which an unsigned int counts
    files->most = (unsigned int)((args->count < CLI_FILES_MAX) ? args->count : CLI_FILES_MAX);
    files->text_max = (args->bytes < CLI_FILES_TEXT) ? args->bytes : CLI_FILES_TEXT;
    files->start = EMBED_CALL(cli_files_bytes_internal.cell_alloc, CliFilesBytesCtx,
                              .bytes = (unsigned long long)files->most * sizeof(unsigned int));
    files->text = EMBED_CALL(cli_files_bytes_internal.cell_alloc, CliFilesBytesCtx, .bytes = files->text_max);
    if ((files->start == NULL) || (files->text == NULL))
    {
        EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = files);
        return 0;
    }
    return 1;
}

int cli_files_work_init(const CliFilesBytesCtx *args)
{
    CliFiles *files = args->files;
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ULL, 0ULL};
    if (args->count == 0ULL)
    {
        return 1;
    }
    // each bounded by its limit, which an unsigned int counts
    files->most = (unsigned int)((args->count < CLI_FILES_MAX) ? args->count : CLI_FILES_MAX);
    files->text_max = (args->bytes < CLI_FILES_TEXT) ? args->bytes : CLI_FILES_TEXT;
    files->start = EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx,
                              .bytes = (unsigned long long)files->most * sizeof(unsigned int));
    files->text = EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx, .bytes = files->text_max);
    if ((files->start == NULL) || (files->text == NULL))
    {
        EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = files);
        return 0;
    }
    return 1;
}

int cli_files_pick(const CliFilesPickedCtx *args)
{
    CliPicked *picked = args->picked;
    const char *root = args->root;
    *picked = (CliPicked){NULL, 0u};
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    unsigned int count = 0u;
    unsigned long long bytes = 0ULL;
    EMBED_CALL(cli_symbols_count.tree_files_count, CliSymbolsCountCfg, .count_pointer = &count, .bytes = &bytes);
    // the most the tree has, one straight take: a filter admits no more than every file; a list that stands past
    // the call, between the window's frames, is taken in the persistent tier
    const unsigned long long cell_bytes = sizeof(CliFiles) + ((unsigned long long)count * sizeof(unsigned int)) + bytes;
    CliFiles *const files =
        args->standing ? EMBED_CALL(cli_files_bytes_internal.cell_alloc, CliFilesBytesCtx, .bytes = cell_bytes)
                       : EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx, .bytes = cell_bytes);
    if (files == NULL)
    {
        return 0;
    }
    unsigned int *const start = (unsigned int *)(void *)(files + 1);
    *files = (CliFiles){start, 0u, count, (char *)(start + count), 0ULL, bytes};
    char path[CLI_PATH_MAX_LEN];
    unsigned int at = 0u;
    while (EMBED_CALL(cli_symbols_at.tree_next_file, CliSymbolsAtCfg, .at = &at, .path_out = path))
    {
        if (EMBED_CALL(args->filter, CliEachPathCfg, .context = args->context, .path = path))
        {
            (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = files, .path = path,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
        }
    }
    *picked = (CliPicked){files, files->count};
    return 1;
}

const char *cli_picked_at(const CliFilesPickedIndexCtx *args)
{
    unsigned int index = args->index;
    return EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->picked_read->files, .index = index);
}

void cli_picked_let(const CliFilesPickedCtx *args)
{
    CliPicked *picked = args->picked;
    g_cli_guard->persistent_buf_release(picked->files, g_cli_files_views_owner);
    *picked = (CliPicked){NULL, 0u};
}
