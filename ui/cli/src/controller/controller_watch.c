// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_watch.c: the tree watched for changes made outside the window. The watch's thread notes each file written
// among the tree's runs and pushes the files' paths, copied into one cell, on a list any thread pushes to and the
// controller's thread alone takes whole. The batches waiting go together, each file once and in order, those git leaves
// out passed over: first to the tree on its lane and then to the servers on theirs, so a check of many files stands in
// front of no other lane's work. A file git leaves out, as a build's output is, is shown in the tree and not checked
#include "controller_internal.h"
#include "controller_work.h"

#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../editor/watch.h"
#include "../runs/qry.h"
#include "../text/text.h"

#include <stdlib.h>

// One batch's files: the batch before it, how many, and their paths one after another, each ended by a 0
typedef struct ControllerWatched
{
    struct ControllerWatched *next;
    unsigned int files;
    const char *text;
} ControllerWatched;

// The watch: the batches pushed and not yet taken, a pointer in the word; the batches taken and waiting for the work
// under way to end; and whether work is under way
typedef struct
{
    _Alignas(64) embed_atomic_u64 pushed;
    ControllerWatched *waiting;
    int working;
} ControllerWatch;

static ControllerWatch s_watch;

// The work on one or more batches: the batches, and their files in order, each once, those git leaves out passed over
typedef struct
{
    ControllerWatched *batches;
    const char **path;
    unsigned int count;
} ControllerWatchWork;

// two paths in the order of their bytes
static int watch_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = *(const char *const *)one,
                      .other_pointer = *(const char *const *)other);
}

// from the watch's thread: each file written noted among the tree's runs, a lost batch marked for them, and the files
// pushed
void cli_controller_watch_taken(const CliWatchBatchCfg *args)
{
    const char *const root = cli_controller_root();
    if (args->lost)
    {
        EMBED_CALL(cli_qry_out_root.runs_lose, CliQryOutRootCfg, .root = root);
    }
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < args->files->count; at += 1u)
    {
        const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->files, .index = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        EMBED_CALL(cli_qry_path_root.run_write, CliQryPathRootCfg, .root = root, .path = path, .length = length);
        bytes += length + 1ULL;
    }
    if (args->files->count == 0u)
    {
        return;
    }
    ControllerWatched *const batch = g_cli_cells->persistent_buf_alloc(sizeof(*batch) + bytes, &g_cli_controller_cells);
    if (batch == NULL)
    {
        return;
    }
    char *const text = (char *)(void *)(batch + 1);
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < args->files->count; at += 1u)
    {
        const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->files, .index = at);
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = text + used, .src = path, .bytes = size});
        used += size;
    }
    batch->files = args->files->count;
    batch->text = text;
    // a pointer fits the 64-bit word the list's head stands in
    embed_u64 head = EMBED_ATOMIC_LOAD(&s_watch.pushed);
    do
    {
        batch->next = (ControllerWatched *)(uintptr_t)head;
    } while (!EMBED_ATOMIC_CAS_64(&s_watch.pushed, &head, (embed_u64)(uintptr_t)batch));
    cli_controller_wake();
}

// what runs on the tree's lane: the files put in order, each once, those git leaves out passed over, and the tree's
// branches of them walked
static void watch_tree_run(void *context)
{
    ControllerWatchWork *const work = (ControllerWatchWork *)context;
    const char *const root = cli_controller_root();
    unsigned long long files = 0ULL;
    for (const ControllerWatched *batch = work->batches; batch != NULL; batch = batch->next)
    {
        files += batch->files;
    }
    work->path = g_cli_cells->persistent_buf_alloc(files * sizeof(*work->path), &g_cli_controller_cells);
    if (work->path == NULL)
    {
        return;
    }
    unsigned int count = 0u;
    for (const ControllerWatched *batch = work->batches; batch != NULL; batch = batch->next)
    {
        const char *at = batch->text;
        for (unsigned int one = 0u; one < batch->files; one += 1u)
        {
            work->path[count] = at;
            count += 1u;
            at += cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_PATH_MAX_LEN}) + 1u;
        }
    }
    qsort(work->path, count, sizeof(work->path[0]), watch_order);
    unsigned int unique = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int again =
            (unique > 0u) && cellul.eq(&(CatenaFinitaCfg){
                                 .src = work->path[unique - 1u], .other = work->path[at], .cap = CLI_PATH_MAX_LEN});
        if (!again && EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = root, .path = work->path[at]))
        {
            work->path[unique] = work->path[at];
            unique += 1u;
        }
    }
    work->count = unique;
    if (unique > 0u)
    {
        EMBED_CALL(cli_symbols_root.tree_branches_walk, CliSymbolsRootCfg, .root = root, .paths = work->path,
                   .count = unique);
    }
}

// what runs on the servers' lane: the servers and the tree's check told of the files
static void watch_servers_run(void *context)
{
    const ControllerWatchWork *const work = (const ControllerWatchWork *)context;
    if (work->count > 0u)
    {
        EMBED_CALL(cli_servers_root_sink_count.change, CliServersRootSinkCountCfg, .root = cli_controller_root(),
                   .files = work->path, .count = work->count);
    }
}

// what comes back on the controller's thread: the work's cells released
static void watch_servers_done(void *context)
{
    ControllerWatchWork *const work = (ControllerWatchWork *)context;
    g_cli_cells->persistent_buf_release(work->path, &g_cli_controller_cells);
    ControllerWatched *batch = work->batches;
    while (batch != NULL)
    {
        ControllerWatched *const next = batch->next;
        g_cli_cells->persistent_buf_release(batch, &g_cli_controller_cells);
        batch = next;
    }
    g_cli_cells->persistent_buf_release(work, &g_cli_controller_cells);
    s_watch.working = 0;
}

// what comes back on the controller's thread: the files handed to the servers' lane
static void watch_tree_done(void *context)
{
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane = CLI_WORK_SERVERS, .run = watch_servers_run,
               .done = watch_servers_done, .context = context);
}

int cli_model_watch_step(const CliModelCtx *args)
{
    (void)args;
    ControllerWatched *batch = (ControllerWatched *)(uintptr_t)EMBED_ATOMIC_EXCHANGE_64(&s_watch.pushed, (embed_u64)0u);
    const int took = batch != NULL;
    while (batch != NULL)
    {
        ControllerWatched *const next = batch->next;
        batch->next = s_watch.waiting;
        s_watch.waiting = batch;
        batch = next;
    }
    if (!s_watch.working && (s_watch.waiting != NULL))
    {
        ControllerWatchWork *const work = g_cli_cells->persistent_buf_alloc(sizeof(*work), &g_cli_controller_cells);
        if (work != NULL)
        {
            *work = (ControllerWatchWork){s_watch.waiting, NULL, 0u};
            s_watch.waiting = NULL;
            s_watch.working = 1;
            EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane = CLI_WORK_TREE, .run = watch_tree_run,
                       .done = watch_tree_done, .context = work);
        }
    }
    return took;
}

void cli_model_watch_end(const CliModelCtx *args)
{
    (void)args;
    cli_watch_stop();
    ControllerWatched *batch = (ControllerWatched *)(uintptr_t)EMBED_ATOMIC_EXCHANGE_64(&s_watch.pushed, (embed_u64)0u);
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        while (batch != NULL)
        {
            ControllerWatched *const next = batch->next;
            g_cli_cells->persistent_buf_release(batch, &g_cli_controller_cells);
            batch = next;
        }
        batch = s_watch.waiting;
        s_watch.waiting = NULL;
    }
}
