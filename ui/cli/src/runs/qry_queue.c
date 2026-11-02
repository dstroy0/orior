// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_queue.c: the backend's channels to a run. Each party of the window stores what it gathers in its own buffer and
// queues it here at once; each channel's thread appends each record to the run in the order queued. A record waits for
// room in its channel's ring, never behind the records of another channel, and one larger than half a ring, a whole
// file as a rule, is appended to the run as it is queued, the party waiting on it. A channel's ring is a cell of the
// cellblock, counted to the queue, taken as the channel is first queued to and retained while its thread runs, so a
// channel never queued to contains nothing
#include "../text/json_tree.h"
#include "qry.h"

// One channel: its ring, where the next item goes and where the next is taken, the bytes taken, its thread, and
// whether the thread runs
typedef struct
{
    unsigned long long *ring;
    unsigned long long head;
    unsigned long long tail;
    unsigned long long used;
    CliThread thread;
    int started;
} CliChannel;
// the bytes of each channel's ring, and of an item's head in it
#define CLI_QUEUE_BYTES (8u << 20u)

#define CLI_QUEUE_HEAD 32u

// An item's head in a ring: the bytes of the path and the name after it, each with a NUL, the record's priority, the
// blob's bytes after the name, and every byte the item takes, a whole number of words; 0 there marks the ring's end
typedef struct
{
    unsigned int path_length;
    unsigned int name_length;
    unsigned int priority;
    unsigned int unused;
    unsigned long long size;
    unsigned long long total;
} CliQueued;

_Static_assert(sizeof(CliQueued) == CLI_QUEUE_HEAD, "CliQueued must be the bytes an item's head takes");

static CliChannel s_channels[CLI_CHANNELS];
static CliWake s_wake = CLI_WAKE_INIT;
// what the queue has done: the records appended, those that found no run or no room, and what the last of those
// reported
static unsigned long long s_handed;
static unsigned long long s_failed;
static char s_last_err[CLI_ERR_MSG_MAX_LEN];

typedef struct
{
    CliChannel *channel;
} QryQueueCtx;
EMBED_INLINE unsigned char *cli_bytes(const QryQueueCtx *args)
{
    return (unsigned char *)(void *)args->channel->ring;
}

// one channel's thread: each item taken in turn, appended to its run with the lock let go, and its room given back
static void cli_channel_run(void *context)
{
    CliChannel *const channel = (CliChannel *)context;
    for (;;)
    {
        while (channel->used == 0ull)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
        }
        unsigned char *const at = EMBED_CALL(cli_bytes, QryQueueCtx, .channel = channel) + channel->tail;
        const CliQueued *const item = (const CliQueued *)(const void *)at;
        if (((CLI_QUEUE_BYTES - channel->tail) < CLI_QUEUE_HEAD) || (item->total == 0ull))
        {
            // the ring's end, past which no item stands: the next is at its start
            channel->used -= CLI_QUEUE_BYTES - channel->tail;
            channel->tail = 0ull;
            continue;
        }
        const char *const path = (const char *)(at + CLI_QUEUE_HEAD);
        const char *const name = path + item->path_length + 1u;
        const unsigned char *const bytes = (const unsigned char *)(name + item->name_length + 1u);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        const int done = EMBED_CALL(cli_qry_path.append, CliQryPathCfg, .path = path, .name = name, .bytes = bytes,
                                    .size = item->size, .priority = item->priority, .err_msg = &err_msg);
        mmgr_line_finish(&err_msg);
        s_handed += done ? 1ull : 0ull;
        s_failed += done ? 0ull : 1ull;
        if (!done)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_last_err,
                                    .src = why,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = why, .cap = CLI_CELLS_POOL}) + 1u});
        }
        channel->tail += item->total;
        channel->used -= item->total;
        channel->tail = (channel->tail == CLI_QUEUE_BYTES) ? 0ull : channel->tail;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
    }
}

void cli_qry_queue(const CliQryPathNameCfg *args)
{
    const void *bytes = args->bytes;
    unsigned int priority = args->priority;
    const unsigned int named = (args->channel_named < CLI_CHANNELS) ? args->channel_named : CLI_CHANNEL_PRIMARY;
    CliChannel *const channel = &s_channels[(priority < QRY_ORDINARY) ? CLI_CHANNEL_SIGNAL : named];
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    const unsigned long long need =
        (CLI_QUEUE_HEAD + path_length + 1ull + name_length + 1ull + args->size + 7ull) & ~7ull;
    if (need > (CLI_QUEUE_BYTES / 2u))
    {
        // a record too large for the ring, a whole file as a rule, is appended as it is queued
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        const int done = EMBED_CALL(cli_qry_path.append, CliQryPathCfg, .path = args->path, .name = args->name,
                                    .bytes = bytes, .size = args->size, .priority = priority, .err_msg = &err_msg);
        mmgr_line_finish(&err_msg);
        s_handed += done ? 1ull : 0ull;
        s_failed += done ? 0ull : 1ull;
        if (!done)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_last_err,
                                    .src = why,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = why, .cap = CLI_CELLS_POOL}) + 1u});
        }
        return;
    }
    channel->ring = (channel->ring != NULL) ? channel->ring
                                            : g_cli_cells->persistent_buf_alloc(CLI_QUEUE_BYTES, &g_cli_queue_cells);
    if (channel->ring == NULL)
    {
        s_failed += 1ull;
        memor.cpy(&(MemoriaCfg){.dst = s_last_err,
                                .src = "no ring could be taken for the channel",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "no ring could be taken for the channel",
                                                                       .cap = CLI_CELLS_POOL}) +
                                         1u});
        return;
    }
    if (!channel->started)
    {
        channel->thread.run = cli_channel_run;
        channel->thread.context = channel;
        channel->thread.priority = CLI_PRIORITY_RUN;
        channel->started = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &channel->thread);
    }
    for (;;)
    {
        const unsigned long long end_room = CLI_QUEUE_BYTES - channel->head;
        const unsigned long long wrap = (end_room < need) ? end_room : 0ull;
        if ((CLI_QUEUE_BYTES - channel->used) >= (need + wrap))
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    const unsigned long long end_room = CLI_QUEUE_BYTES - channel->head;
    if (end_room < need)
    {
        if (end_room >= CLI_QUEUE_HEAD)
        {
            CliQueued *const marker =
                (CliQueued *)(void *)(EMBED_CALL(cli_bytes, QryQueueCtx, .channel = channel) + channel->head);
            marker->total = 0ull;
        }
        channel->used += end_room;
        channel->head = 0ull;
    }
    unsigned char *const at = EMBED_CALL(cli_bytes, QryQueueCtx, .channel = channel) + channel->head;
    // the path and the name are below a ring, and a priority one of three, which an unsigned int stores
    const CliQueued item = {(unsigned int)path_length, (unsigned int)name_length, priority, 0u, args->size, need};
    memor.cpy(&(MemoriaCfg){.dst = at, .src = &item, .bytes = sizeof(item)});
    memor.cpy(&(MemoriaCfg){.dst = at + CLI_QUEUE_HEAD, .src = args->path, .bytes = path_length + 1ull});
    memor.cpy(
        &(MemoriaCfg){.dst = at + CLI_QUEUE_HEAD + path_length + 1ull, .src = args->name, .bytes = name_length + 1ull});
    memor.cpy(&(MemoriaCfg){
        .dst = at + CLI_QUEUE_HEAD + path_length + 1ull + name_length + 1ull, .src = bytes, .bytes = args->size});
    channel->head += need;
    channel->head = (channel->head == CLI_QUEUE_BYTES) ? 0ull : channel->head;
    channel->used += need;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

void cli_qry_on_queue(const CliQryOutRootCfg *args)
{
    mmgr_line_put(args->out, "{\"handed\":");
    mmgr_line_decimal(args->out, s_handed, 1u);
    mmgr_line_put(args->out, ",\"dumped\":0,\"failed\":");
    mmgr_line_decimal(args->out, s_failed, 1u);
    mmgr_line_put(args->out, ",\"message\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = s_last_err,
               .length = cellul.len(&(CatenaFinitaCfg){.src = s_last_err, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(args->out, '}');
}
