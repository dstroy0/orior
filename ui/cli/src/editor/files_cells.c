// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_cells.c: the files' cells and the lists made of them: a list's paths and places taken at the sizes a count of
// them gives, put, sorted and given back; and the files of the tree a reader picks for one call, by their places in it.
// The cells are the one cellblock's, counted to the files' own mark, which their report gives
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "files_internal.h"

void cli_files_cells_usage_write(const CliFilesRootOutDirCfg *args)
{
    EMBED_CALL(cli_cells_table.owner_usage_write, CliCellsCfg, .owner = &g_cli_files_views_cells, .out = args->out);
}

void *cli_files_cell_take(const CliFilesBytesCtx *args)
{
    return g_cli_cells->persistent_buf_alloc(args->bytes, &g_cli_files_views_cells);
}

void *cli_files_work_take(const CliFilesBytesCtx *args)
{
    return g_cli_cells->temporary_buf_alloc(args->bytes, &g_cli_files_views_cells);
}

void cli_files_cell_free(const CliFilesCtx *args)
{
    g_cli_cells->persistent_buf_release(args->cell, &g_cli_files_views_cells);
}

const char *cli_files_at(const CliFilesCfg *args)
{
    return args->files->text + args->files->start[args->index];
}

int cli_files_put(const CliFilesPathLengthCtx *args)
{
    if ((args->files->count >= args->files->most) ||
        ((args->files->used + args->length + 1ull) > args->files->text_most))
    {
        return 0;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int stores
    args->files->start[args->files->count] = (unsigned int)args->files->used;
    memor.cpy(&(MemoriaCfg){.dst = args->files->text + args->files->used, .src = args->path, .bytes = args->length});
    args->files->text[args->files->used + args->length] = '\0';
    args->files->used += args->length + 1ull;
    args->files->count += 1u;
    return 1;
}

int cli_files_contains(const CliFilesPathKindCfg *args)
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
            return 1;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return 0;
}

typedef struct
{
    CliFiles *files;
    unsigned int *spare;
} FilesCellsCtx;
// the places of `files` sorted by their paths' bytes, runs of one, two, four and on merged in turn between the places
// and `spare`, sized for as many
EMBED_INLINE void files_merge(const FilesCellsCtx *args)
{
    unsigned int *from = args->files->start;
    unsigned int *to = args->spare;
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
    unsigned int *const spare = EMBED_CALL(cli_files_bytes_internal.work_take, CliFilesBytesCtx,
                                           .bytes = (unsigned long long)args->files->count * sizeof(unsigned int));
    if (spare == NULL)
    {
        return 0;
    }
    EMBED_CALL(files_merge, FilesCellsCtx, .files = args->files, .spare = spare);
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = spare);
    // each path listed once
    unsigned int sorted_count = 0u;
    for (unsigned int at = 0u; at < args->files->count; at += 1u)
    {
        if ((sorted_count == 0u) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = args->files->text + args->files->start[sorted_count - 1u],
                                           .other = args->files->text + args->files->start[at],
                                           .cap = CLI_CELLS_POOL})))
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
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = files->start);
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = files->text);
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
}

int cli_files_init(const CliFilesBytesCtx *args)
{
    CliFiles *files = args->files;
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
    if (args->count == 0ull)
    {
        return 1;
    }
    // each bounded by its limit, which an unsigned int counts
    files->most = (unsigned int)((args->count < CLI_FILES_MOST) ? args->count : CLI_FILES_MOST);
    files->text_most = (args->bytes < CLI_FILES_TEXT) ? args->bytes : CLI_FILES_TEXT;
    files->start = EMBED_CALL(cli_files_bytes_internal.cell_take, CliFilesBytesCtx,
                              .bytes = (unsigned long long)files->most * sizeof(unsigned int));
    files->text = EMBED_CALL(cli_files_bytes_internal.cell_take, CliFilesBytesCtx, .bytes = files->text_most);
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
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
    if (args->count == 0ull)
    {
        return 1;
    }
    // each bounded by its limit, which an unsigned int counts
    files->most = (unsigned int)((args->count < CLI_FILES_MOST) ? args->count : CLI_FILES_MOST);
    files->text_most = (args->bytes < CLI_FILES_TEXT) ? args->bytes : CLI_FILES_TEXT;
    files->start = EMBED_CALL(cli_files_bytes_internal.work_take, CliFilesBytesCtx,
                              .bytes = (unsigned long long)files->most * sizeof(unsigned int));
    files->text = EMBED_CALL(cli_files_bytes_internal.work_take, CliFilesBytesCtx, .bytes = files->text_most);
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
    unsigned long long bytes = 0ull;
    EMBED_CALL(cli_symbols_count.tree_files_count, CliSymbolsCountCfg, .count_pointer = &count, .bytes = &bytes);
    // the most the tree holds, one straight take: a filter admits no more than every file
    CliFiles *const files =
        EMBED_CALL(cli_files_bytes_internal.work_take, CliFilesBytesCtx,
                   .bytes = sizeof(*files) + ((unsigned long long)count * sizeof(unsigned int)) + bytes);
    if (files == NULL)
    {
        return 0;
    }
    unsigned int *const start = (unsigned int *)(void *)(files + 1);
    *files = (CliFiles){start, 0u, count, (char *)(start + count), 0ull, bytes};
    char path[CLI_PATH_MAX_LEN];
    unsigned int at = 0u;
    while (EMBED_CALL(cli_symbols_at.tree_next_file, CliSymbolsAtCfg, .at = &at, .path_out = path))
    {
        if (EMBED_CALL(args->filter, CliEachPathCfg, .context = args->context, .path = path))
        {
            (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = files, .path = path,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
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
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = picked->files);
    *picked = (CliPicked){NULL, 0u};
}
