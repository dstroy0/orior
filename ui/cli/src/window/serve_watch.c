// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_watch.c: the tree open watched for changes made outside the window, each batch told as tree-changed, in place
// of the watch over the tree open before. The page is told first; the servers and the tree's check hear of the files on
// a thread of their own, the batches that come while they work taken as one, so that no check of many files holds up
// the page's word of them. A file git leaves out, as a build's output is, is shown in the tree and not checked
#include "../editor/files.h"
#include "../editor/watch.h"
#include "serve_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// One batch's files waiting for the servers: the batch before it, how many files, and their paths one after another,
// each ended by a 0; a cell sized to the batch, which the thread takes off and gives back once the servers have heard
// of its files
typedef struct CliWatchedBatchStruct
{
    struct CliWatchedBatchStruct *next;
    unsigned int files;
    char room[];
} CliWatched;

// the batches waiting and how many files they hold, their tree, and whether the thread that takes them started, under
// the lock
static CliWatched *s_waiting;
static unsigned int s_waiting_files;
static char s_tree[CLI_PATH_LONGEST];
static int s_started;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static CliThread s_thread;

// two paths in the order of their bytes
static int watch_order(const void *one, const void *other)
{
    return cli_text_order(*(const char *const *)one, *(const char *const *)other);
}

// a chain of batches given back
static void watch_batches_given(CliWatched *batch)
{
    while (batch != NULL)
    {
        CliWatched *const next = batch->next;
        cli_cell_given(batch);
        batch = next;
    }
}

// the batches waiting, taken as one, each file once and in order, those git leaves out passed over, handed to the
// servers; where no cell meets the files' order, the batches are let go unheard
static void watch_taker(void *context)
{
    (void)context;
    char root[CLI_PATH_LONGEST];
    for (;;)
    {
        cli_lock_take(&s_lock);
        while (s_waiting == NULL)
        {
            cli_wake_wait(&s_wake, &s_lock);
        }
        CliWatched *const taken = s_waiting;
        const unsigned int files = s_waiting_files;
        s_waiting = NULL;
        s_waiting_files = 0u;
        memor.cpy(&(MemoriaCfg){.dst = root, .src = s_tree, .bytes = sizeof(root)});
        cli_lock_give(&s_lock);
        const char **const paths = cli_cell_taken((files + 1ull) * sizeof(const char *), "the files waiting in order");
        unsigned int count = 0u;
        for (const CliWatched *batch = taken; (paths != NULL) && (batch != NULL); batch = batch->next)
        {
            const char *at = batch->room;
            for (unsigned int one = 0u; one < batch->files; one += 1u)
            {
                paths[count] = at;
                count += 1u;
                at += cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}) + 1u;
            }
        }
        if (paths != NULL)
        {
            qsort(paths, count, sizeof(paths[0]), watch_order);
        }
        unsigned int kept = 0u;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const int again =
                (kept > 0u) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = paths[kept - 1u], .other = paths[at], .cap = CLI_CELLS_POOL}));
            if (!again && cli_files_tracked(root, paths[at]))
            {
                paths[kept] = paths[at];
                kept += 1u;
            }
        }
        if (kept > 0u)
        {
            cli_servers_changed(root, paths, kept);
        }
        cli_cell_given(paths);
        watch_batches_given(taken);
    }
}

// a batch of changes told to the page, and its files handed to the thread that takes them to the servers in a cell
// sized to them; where no cell meets the batch's reading or its files, the page alone hears of it
static void watch_took(void *context, const char *batch, unsigned long long size)
{
    (void)context;
    cli_serve_emitted("tree-changed", batch, size);
    CliJson json;
    char quiet[256];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = cli_json_taken(&json, batch, size, 0u, 0ull, &said);
    const unsigned int files = cli_json_member(&json, root, "files");
    const unsigned int first = cli_json_is(&json, files, CFG_JSON_ARRAY) ? json.node[files].first : CLI_JSON_NONE;
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json.node[one].next)
    {
        unsigned long long length = 0ull;
        if (cli_json_string(&json, one, &length) != NULL)
        {
            count += 1u;
            bytes += length + 1ull;
        }
    }
    CliWatched *const watched =
        (count != 0u) ? cli_cell_taken(sizeof(CliWatched) + bytes, "a batch of files waiting") : NULL;
    unsigned long long used = 0ull;
    for (unsigned int one = (watched != NULL) ? first : CLI_JSON_NONE; one != CLI_JSON_NONE; one = json.node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path = cli_json_string(&json, one, &length);
        if (path != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = watched->room + used, .src = path, .bytes = (size_t)length});
            watched->room[used + length] = '\0';
            used += length + 1ull;
        }
    }
    cli_json_given(&json);
    if (watched == NULL)
    {
        return;
    }
    watched->files = count;
    cli_lock_take(&s_lock);
    watched->next = s_waiting;
    s_waiting = watched;
    s_waiting_files += count;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

void cli_serve_watch(void)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_root_now(root))
    {
        return;
    }
    cli_lock_take(&s_lock);
    memor.cpy(&(MemoriaCfg){.dst = s_tree, .src = root, .bytes = sizeof(s_tree)});
    CliWatched *const unheard = s_waiting;
    s_waiting = NULL;
    s_waiting_files = 0u;
    const int started = s_started;
    s_started = 1;
    cli_lock_give(&s_lock);
    watch_batches_given(unheard);
    if (!started)
    {
        s_thread = (CliThread){watch_taker, NULL};
        (void)cli_thread_started(&s_thread);
    }
    cli_watch_start(root, watch_took, NULL);
}
