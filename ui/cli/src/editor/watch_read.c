// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// watch_read.c: the tree read every two seconds where the system does not tell of its changes: each file git tracks or
// would track, each folder holding one, and git's index and HEAD, with when each was last written and how long it is,
// set against the last reading
#include "../command_line/cells.h"
#include "../text/text.h"
#include "files_internal.h"
#include "watch_internal.h"

// how often the tree is read, and the slices the wait between readings is taken in, in milliseconds
#define CLI_WATCH_POLL_EVERY 2000u
#define CLI_WATCH_SLICE 50u

// A reading: the paths read, sorted, each with whether it is there, when it was last written and its length, 0 for a
// folder; the paths a list and the rest cells counted to the files, each made at the reading's own count
typedef struct
{
    CliFiles paths;
    unsigned char *there;
    unsigned long long *time;
    unsigned long long *size;
} CliReading;

// the tree's top, git's index and git's HEAD, read in every reading, and their bytes with their ends
static const char *const s_always[3] = {"", ".git/index", ".git/HEAD"};
#define CLI_WATCH_ALWAYS_BYTES 21ull

// The last two readings, made at the first reading and given back as the polling ends, and which is the last
static CliReading s_readings[2];
static unsigned int s_last;

// a reading's cells given back and the reading emptied
static void cli_reading_emptied(CliReading *reading)
{
    cli_files_emptied(&reading->paths);
    cli_files_cell_given(reading->there);
    cli_files_cell_given(reading->time);
    cli_files_cell_given(reading->size);
    *reading = (CliReading){{NULL, 0u, 0u, NULL, 0ull, 0ull}, NULL, NULL, NULL};
}

// the paths of the view's files and of each folder holding one counted, or put where `paths` is given, after the three
// read always; their count and bytes into `counted` and `bytes`
static void cli_reading_paths(const CliSeen *seen, CliFiles *paths, unsigned long long *counted,
                              unsigned long long *bytes)
{
    *counted = 3ull;
    *bytes = CLI_WATCH_ALWAYS_BYTES;
    for (unsigned int one = 0u; (paths != NULL) && (one < 3u); one += 1u)
    {
        cli_files_put(paths, s_always[one],
                      cellul.len(&(CatenaFinitaCfg){.src = s_always[one], .cap = CLI_CELLS_POOL}));
    }
    for (unsigned int index = 0u; index < seen->files.count; index += 1u)
    {
        const char *const file = cli_files_at(&seen->files, index);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
        *counted += 1ull;
        *bytes += length + 1ull;
        if (paths != NULL)
        {
            cli_files_put(paths, file, length);
        }
        for (const char *at = file; at[0] != '\0'; at += 1)
        {
            // the slash lies in the path, past its start
            const unsigned long long folder = (unsigned long long)(at - file);
            *counted += (at[0] == '/') ? 1ull : 0ull;
            *bytes += (at[0] == '/') ? (folder + 1ull) : 0ull;
            if ((paths != NULL) && (at[0] == '/'))
            {
                cli_files_put(paths, file, folder);
            }
        }
    }
}

// the tree at `root` read into `reading`, its cells from before given back first: 1, or 0 where no view of the tree
// could be held or the cellblock cannot meet the reading, the reading left empty. A reading is of the tree as it is
// now, so the view kept is let go and read again, and the view read stands for the list and the search as well
static int cli_reading_of(const char *root, CliReading *reading)
{
    cli_reading_emptied(reading);
    cli_files_forget_seen();
    const CliSeen *const seen = cli_seen_held(root);
    if (seen == NULL)
    {
        return 0;
    }
    unsigned long long counted = 0ull;
    unsigned long long bytes = 0ull;
    cli_reading_paths(seen, NULL, &counted, &bytes);
    CliFiles *const paths = &reading->paths;
    const int made = cli_files_made(paths, counted, bytes);
    if (made)
    {
        cli_reading_paths(seen, paths, &counted, &bytes);
    }
    cli_seen_let(seen);
    if (!made || !cli_files_sorted(paths))
    {
        cli_reading_emptied(reading);
        return 0;
    }
    reading->there = cli_files_cell_taken(paths->count);
    reading->time = cli_files_cell_taken((unsigned long long)paths->count * sizeof(unsigned long long));
    reading->size = cli_files_cell_taken((unsigned long long)paths->count * sizeof(unsigned long long));
    if ((reading->there == NULL) || (reading->time == NULL) || (reading->size == NULL))
    {
        cli_reading_emptied(reading);
        return 0;
    }
    for (unsigned int index = 0u; index < paths->count; index += 1u)
    {
        const char *const path = cli_files_at(paths, index);
        char full[CLI_PATH_LONGEST];
        if (path[0] == '\0')
        {
            memor.cpy(&(MemoriaCfg){.dst = full,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        }
        else
        {
            cli_full(root, path, full);
        }
        unsigned long long size = 0ull;
        reading->there[index] = (unsigned char)cli_file_stamp(full, &size, &reading->time[index]);
        reading->size[index] = query_is_folder(full) ? 0ull : size;
    }
    return 1;
}

int cli_watch_first_reading(const char *root)
{
    s_last = 0u;
    (void)cli_reading_of(root, &s_readings[s_last]);
    return 1;
}

void cli_watch_readings_given(void)
{
    cli_reading_emptied(&s_readings[0]);
    cli_reading_emptied(&s_readings[1]);
}

// the next path of a reading that is there from `at` on, or the count
static unsigned int cli_next_there(const CliReading *reading, unsigned int at)
{
    while ((at < reading->paths.count) && !reading->there[at])
    {
        at += 1u;
    }
    return at;
}

// what changed from the last reading to this one put for the batches: each path made, written or taken out
static void cli_readings_compared(const CliReading *last, const CliReading *now, CliWatchTold told, void *context)
{
    unsigned int was = cli_next_there(last, 0u);
    unsigned int is = cli_next_there(now, 0u);
    while ((was < last->paths.count) || (is < now->paths.count))
    {
        const int has_was = was < last->paths.count;
        const int has_is = is < now->paths.count;
        const int order = !has_was  ? 1
                          : !has_is ? -1
                                    : cli_text_order(cli_files_at(&last->paths, was), cli_files_at(&now->paths, is));
        if (order < 0)
        {
            told(context, cli_files_at(&last->paths, was), CLI_CHANGE_REMOVED);
            was = cli_next_there(last, was + 1u);
            continue;
        }
        if (order > 0)
        {
            told(context, cli_files_at(&now->paths, is), CLI_CHANGE_ADDED);
            is = cli_next_there(now, is + 1u);
            continue;
        }
        if ((last->time[was] != now->time[is]) || (last->size[was] != now->size[is]))
        {
            told(context, cli_files_at(&now->paths, is), CLI_CHANGE_MODIFIED);
        }
        was = cli_next_there(last, was + 1u);
        is = cli_next_there(now, is + 1u);
    }
}

void cli_watch_read_again(const char *root, CliWatchTold told, void *context)
{
    const unsigned int now = 1u - s_last;
    if (!cli_reading_of(root, &s_readings[now]))
    {
        return;
    }
    // a last reading that could not be made tells nothing; this one stands as the last
    if (s_readings[s_last].paths.count != 0u)
    {
        cli_readings_compared(&s_readings[s_last], &s_readings[now], told, context);
    }
    s_last = now;
}

void cli_watch_polled(void *context)
{
    const char *const root = (const char *)context;
    for (;;)
    {
        int stopping = 0;
        for (unsigned int waited = 0u; !stopping && (waited < CLI_WATCH_POLL_EVERY); waited += CLI_WATCH_SLICE)
        {
            query_paused(CLI_WATCH_SLICE);
            stopping = cli_watch_stopping();
        }
        if (stopping)
        {
            break;
        }
        cli_watch_read_again(root, cli_watch_told, NULL);
    }
    cli_watch_readings_given();
    cli_watch_poll_ended();
}
