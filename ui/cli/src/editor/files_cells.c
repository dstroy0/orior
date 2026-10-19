// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_cells.c: the files' cells and the lists made of them: a list's paths and places taken at the sizes a count of
// them gives, put, sorted and given back; and the files of a held view a reader keeps, by their places in it. The cells
// are the one cellblock's, counted to the files' own mark, which their report gives
#include "../command_line/cells.h"
#include "../text/text.h"
#include "files_internal.h"

// what the views, a walk's folders, and the lists the search, the quick open, a folder's listing and the watch make
// hold in the cellblock
static CliCellsTally s_tally = CLI_CELLS_TALLY("cli_files_views");

void cli_files_cells_said(CliLine *out)
{
    cli_cells_tally_said(&s_tally, out);
}

void *cli_files_cell_taken(unsigned long long bytes)
{
    return cli_cell_taken_by(&s_tally, bytes, "the tree's files");
}

void cli_files_cell_given(void *cell)
{
    cli_cell_given_by(&s_tally, cell);
}

const char *cli_files_at(const CliFiles *files, unsigned int index)
{
    return files->text + files->start[index];
}

int cli_files_put(CliFiles *files, const char *path, unsigned long long length)
{
    if ((files->count >= files->most) || ((files->used + length + 1ull) > files->text_most))
    {
        return 0;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int holds
    files->start[files->count] = (unsigned int)files->used;
    memor.cpy(&(MemoriaCfg){.dst = files->text + files->used, .src = path, .bytes = length});
    files->text[files->used + length] = '\0';
    files->used += length + 1ull;
    files->count += 1u;
    return 1;
}

int cli_files_holds(const CliFiles *files, const char *path)
{
    unsigned int low = 0u;
    unsigned int high = files->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = cli_text_order(cli_files_at(files, middle), path);
        if (order == 0)
        {
            return 1;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return 0;
}

// the places of `files` sorted by their paths' bytes, runs of one, two, four and on merged in turn between the places
// and `spare`, which holds as many
static void files_merged(CliFiles *files, unsigned int *spare)
{
    unsigned int *from = files->start;
    unsigned int *to = spare;
    for (unsigned int width = 1u; width < files->count; width *= 2u)
    {
        for (unsigned int left = 0u; left < files->count; left += 2u * width)
        {
            const unsigned int middle = ((left + width) < files->count) ? (left + width) : files->count;
            const unsigned int end = ((left + (2u * width)) < files->count) ? (left + (2u * width)) : files->count;
            unsigned int one = left;
            unsigned int other = middle;
            for (unsigned int put = left; put < end; put += 1u)
            {
                const int first =
                    (other >= end) ||
                    ((one < middle) && (cli_text_order(files->text + from[one], files->text + from[other]) <= 0));
                to[put] = first ? from[one] : from[other];
                one += first ? 1u : 0u;
                other += first ? 0u : 1u;
            }
        }
        unsigned int *const held = from;
        from = to;
        to = held;
    }
    if (from != files->start)
    {
        memor.cpy(&(MemoriaCfg){.dst = files->start, .src = from, .bytes = files->count * sizeof(unsigned int)});
    }
}

int cli_files_sorted(CliFiles *files)
{
    if (files->count == 0u)
    {
        return 1;
    }
    unsigned int *const spare = cli_files_cell_taken((unsigned long long)files->count * sizeof(unsigned int));
    if (spare == NULL)
    {
        return 0;
    }
    files_merged(files, spare);
    cli_files_cell_given(spare);
    // each path kept once
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        if ((kept == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = files->text + files->start[kept - 1u],
                                                           .other = files->text + files->start[at],
                                                           .cap = CLI_CELLS_POOL})))
        {
            files->start[kept] = files->start[at];
            kept += 1u;
        }
    }
    files->count = kept;
    return 1;
}

void cli_files_emptied(CliFiles *files)
{
    cli_files_cell_given(files->start);
    cli_files_cell_given(files->text);
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
}

int cli_files_made(CliFiles *files, unsigned long long count, unsigned long long bytes)
{
    *files = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
    if (count == 0ull)
    {
        return 1;
    }
    // each held to its limit, which an unsigned int counts
    files->most = (unsigned int)((count < CLI_FILES_MOST) ? count : CLI_FILES_MOST);
    files->text_most = (bytes < CLI_FILES_TEXT) ? bytes : CLI_FILES_TEXT;
    files->start = cli_files_cell_taken((unsigned long long)files->most * sizeof(unsigned int));
    files->text = cli_files_cell_taken(files->text_most);
    if ((files->start == NULL) || (files->text == NULL))
    {
        cli_files_emptied(files);
        return 0;
    }
    return 1;
}

int cli_files_picked(CliPicked *picked, const char *root, int (*keeps)(void *context, const char *path), void *context)
{
    *picked = (CliPicked){NULL, NULL, 0u};
    const CliSeen *const seen = cli_seen_held(root);
    if (seen == NULL)
    {
        return 0;
    }
    // the files kept counted first, so their places are a cell of as many
    unsigned int count = 0u;
    for (unsigned int index = 0u; index < seen->files.count; index += 1u)
    {
        count += keeps(context, cli_files_at(&seen->files, index)) ? 1u : 0u;
    }
    unsigned int *const place =
        (count != 0u) ? cli_files_cell_taken((unsigned long long)count * sizeof(unsigned int)) : NULL;
    if ((count != 0u) && (place == NULL))
    {
        cli_seen_let(seen);
        return 0;
    }
    unsigned int kept = 0u;
    for (unsigned int index = 0u; (index < seen->files.count) && (kept < count); index += 1u)
    {
        if (keeps(context, cli_files_at(&seen->files, index)))
        {
            place[kept] = index;
            kept += 1u;
        }
    }
    *picked = (CliPicked){seen, place, kept};
    return 1;
}

const char *cli_picked_at(const CliPicked *picked, unsigned int index)
{
    return cli_files_at(&picked->seen->files, picked->place[index]);
}

void cli_picked_let(CliPicked *picked)
{
    cli_files_cell_given(picked->place);
    cli_seen_let(picked->seen);
    *picked = (CliPicked){NULL, NULL, 0u};
}
