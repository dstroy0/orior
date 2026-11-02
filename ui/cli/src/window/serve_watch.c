// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_watch.c: the tree open watched for changes made outside the window, each batch sent as tree-changed, in place
// of the watch over the tree open before. The page is notified first, and the tree's runs of each run written; the
// servers and the tree's check are notified of the files on a thread of their own, the batches that come while they
// work taken as one, so that no check of many files blocks the page's word of them. A file git leaves out, as a
// build's output is, is shown in the tree and not checked
#include "../command_line/callback_cfg.h"
#include "../editor/files.h"
#include "../editor/watch.h"
#include "../runs/qry.h"
#include "serve_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// One round's files waiting for the servers: the round before it, how many files, and their paths one after another,
// each ended by a 0; a cell sized to the round, which the thread takes off and gives back once the servers have
// processed of its files
typedef struct CliWatchedBatchStruct
{
    struct CliWatchedBatchStruct *next;
    unsigned int files;
    char room[];
} CliWatched;

// the rounds waiting and how many files they contain, their tree, and whether the thread that takes them started,
// under the lock
static CliWatched *s_waiting;
static unsigned int s_waiting_files;
static char s_tree[CLI_PATH_MAX_LEN];
static int s_started;
static CliWake s_wake = CLI_WAKE_INIT;
static CliThread s_thread;

// two paths in the order of their bytes
static int watch_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = *(const char *const *)one,
                      .other_pointer = *(const char *const *)other);
}

typedef struct
{
    CliWatched *batch;
} ServeWatchCtx;
// a chain of rounds given back
EMBED_INLINE void watch_batches_free(const ServeWatchCtx *args)
{
    CliWatched *batch = args->batch;
    while (batch != NULL)
    {
        CliWatched *const next = batch->next;
        g_cli_cells->persistent_buf_release(batch, &g_cli_window_cells);
        batch = next;
    }
}

// the rounds waiting, taken as one, each file once and in order, those git leaves out passed over, handed to the
// servers; where no cell meets the files' order, the rounds are let go unheard
static void watch_taker(void *context)
{
    (void)context;
    char root[CLI_PATH_MAX_LEN];
    for (;;)
    {
        while (s_waiting == NULL)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
        }
        CliWatched *const taken = s_waiting;
        const unsigned int files = s_waiting_files;
        s_waiting = NULL;
        s_waiting_files = 0u;
        memor.cpy(&(MemoriaCfg){.dst = root, .src = s_tree, .bytes = sizeof(root)});
        const char **const paths =
            g_cli_cells->temporary_buf_alloc((files + 1ull) * sizeof(const char *), &g_cli_window_cells);
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
        unsigned int unique_count = 0u;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const int again = (unique_count > 0u) &&
                              (cellul.eq(&(CatenaFinitaCfg){
                                  .src = paths[unique_count - 1u], .other = paths[at], .cap = CLI_CELLS_POOL}));
            if (!again && EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = root, .path = paths[at]))
            {
                paths[unique_count] = paths[at];
                unique_count += 1u;
            }
        }
        if (unique_count > 0u)
        {
            EMBED_CALL(cli_symbols_root.tree_branches_walk, CliSymbolsRootCfg, .root = root, .paths = paths,
                       .count = unique_count);
            EMBED_CALL(cli_servers_root_sink_count.change, CliServersRootSinkCountCfg, .root = root, .files = paths,
                       .count = unique_count);
        }
        g_cli_cells->persistent_buf_release(paths, &g_cli_window_cells);
        EMBED_CALL(watch_batches_free, ServeWatchCtx, .batch = taken);
    }
}

// a round of changes sent to the page, each run it wrote noted among the tree's runs and a lost round marked for them,
// and its files handed to the thread that takes them to the servers in a cell sized to them; where no cell meets the
// round's reading or its files, the page alone is notified of it
static void watch_took(const CliEachBatchSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "tree-changed", .body = args->batch,
               .size = size);
    char tree[CLI_PATH_MAX_LEN];
    const int rooted = EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = tree);
    CliJson json;
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->batch, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg,
                   .owner = &g_cli_window_cells);
    const unsigned int lost = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = root, .name = "lost");
    if (rooted && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = lost,
                             .kind = CFG_JSON_TRUE))
    {
        EMBED_CALL(cli_qry_out_root.runs_lose, CliQryOutRootCfg, .root = tree);
    }
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "files");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                          .node = files, .kind = CFG_JSON_ARRAY)
                                   ? json.node[files].first
                                   : CLI_JSON_NONE;
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json.node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &json, .node = one, .length_pointer = &length);
        if (path != NULL)
        {
            count += 1u;
            bytes += length + 1ull;
        }
        if ((path != NULL) && rooted)
        {
            EMBED_CALL(cli_qry_path_root.run_write, CliQryPathRootCfg, .root = tree, .path = path, .length = length);
        }
    }
    CliWatched *const watched =
        (count != 0u) ? g_cli_cells->persistent_buf_alloc(sizeof(CliWatched) + bytes, &g_cli_window_cells) : NULL;
    unsigned long long used = 0ull;
    for (unsigned int one = (watched != NULL) ? first : CLI_JSON_NONE; one != CLI_JSON_NONE; one = json.node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &json, .node = one, .length_pointer = &length);
        if (path != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = watched->room + used, .src = path, .bytes = (size_t)length});
            watched->room[used + length] = '\0';
            used += length + 1ull;
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (watched == NULL)
    {
        return;
    }
    watched->files = count;
    watched->next = s_waiting;
    s_waiting = watched;
    s_waiting_files += count;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

void cli_serve_watch(void)
{
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_tree, .src = root, .bytes = sizeof(s_tree)});
    CliWatched *const pending = s_waiting;
    s_waiting = NULL;
    s_waiting_files = 0u;
    const int started = s_started;
    s_started = 1;
    EMBED_CALL(watch_batches_free, ServeWatchCtx, .batch = pending);
    if (!started)
    {
        s_thread = (CliThread){watch_taker, NULL};
        (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread);
    }
    EMBED_CALL(cli_watch.start, CliWatchCfg, .root = root, .take = watch_took, .context = NULL);
}
