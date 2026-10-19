// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_queue.c: the backend's channels to a run. Each party of the window holds what it gathers in its own buffer and
// queues it here at once; each channel's thread appends each record to the run in the order queued. A record waits for
// room in its channel's ring, never behind the records of another channel, and one larger than half a ring is counted
// failed and said. A channel's ring is a cell of the cellblock, counted to the queue, taken as the channel is first
// queued to and held while its thread runs, so a channel never queued to holds nothing
#include "../text/json_tree.h"
#include "qry.h"

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

static CliChannel s_channels[CLI_CHANNELS];
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
// what the channels' rings hold in the cellblock
static CliCellsTally s_tally = CLI_CELLS_TALLY("cli_queue");
// what the queue has done: the records appended, those that found no run or no room, and what the last of those said
static unsigned long long s_handed;
static unsigned long long s_failed;
static char s_said[CLI_SAID_LONGEST];

static unsigned char *cli_bytes(CliChannel *channel)
{
    return (unsigned char *)(void *)channel->ring;
}

// one channel's thread: each item taken in turn, appended to its run with the lock let go, and its room given back
static void cli_channel_run(void *context)
{
    CliChannel *const channel = (CliChannel *)context;
    cli_lock_take(&s_lock);
    for (;;)
    {
        while (channel->used == 0ull)
        {
            cli_wake_wait(&s_wake, &s_lock);
        }
        unsigned char *const at = cli_bytes(channel) + channel->tail;
        const CliQueued *const item = (const CliQueued *)(const void *)at;
        if (((CLI_QUEUE_BYTES - channel->tail) < CLI_QUEUE_HEAD) || (item->total == 0ull))
        {
            // the ring's end, past which no item stands: the next is at its start
            channel->used -= CLI_QUEUE_BYTES - channel->tail;
            channel->tail = 0ull;
            continue;
        }
        cli_lock_give(&s_lock);
        const char *const path = (const char *)(at + CLI_QUEUE_HEAD);
        const char *const name = path + item->path_length + 1u;
        const unsigned char *const bytes = (const unsigned char *)(name + item->name_length + 1u);
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        const int done = cli_qry_append(path, name, bytes, item->size, item->priority, &said);
        cli_line_closed(&said);
        cli_lock_take(&s_lock);
        s_handed += done ? 1ull : 0ull;
        s_failed += done ? 0ull : 1ull;
        if (!done)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_said,
                                    .src = why,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = why, .cap = CLI_CELLS_POOL}) + 1u});
        }
        channel->tail += item->total;
        channel->used -= item->total;
        channel->tail = (channel->tail == CLI_QUEUE_BYTES) ? 0ull : channel->tail;
        cli_wake_all(&s_wake);
    }
}

void cli_qry_queue(unsigned int channel_named, const char *path, const char *name, const void *bytes,
                   unsigned long long size, unsigned int priority)
{
    const unsigned int named = (channel_named < CLI_CHANNELS) ? channel_named : CLI_CHANNEL_PRIMARY;
    CliChannel *const channel = &s_channels[(priority < QRY_ORDINARY) ? CLI_CHANNEL_SIGNAL : named];
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const unsigned long long need = (CLI_QUEUE_HEAD + path_length + 1ull + name_length + 1ull + size + 7ull) & ~7ull;
    cli_lock_take(&s_lock);
    if (need > (CLI_QUEUE_BYTES / 2u))
    {
        s_failed += 1ull;
        memor.cpy(&(MemoriaCfg){
            .dst = s_said,
            .src = "a record larger than half the queue's ring holds",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "a record larger than half the queue's ring holds",
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        cli_lock_give(&s_lock);
        return;
    }
    channel->ring =
        (channel->ring != NULL) ? channel->ring : cli_cell_taken_by(&s_tally, CLI_QUEUE_BYTES, "a channel's ring");
    if (channel->ring == NULL)
    {
        s_failed += 1ull;
        memor.cpy(&(MemoriaCfg){.dst = s_said,
                                .src = "no ring could be taken for the channel",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "no ring could be taken for the channel",
                                                                       .cap = CLI_CELLS_POOL}) +
                                         1u});
        cli_lock_give(&s_lock);
        return;
    }
    if (!channel->started)
    {
        channel->thread.run = cli_channel_run;
        channel->thread.context = channel;
        channel->started = cli_thread_started(&channel->thread);
    }
    for (;;)
    {
        const unsigned long long end_room = CLI_QUEUE_BYTES - channel->head;
        const unsigned long long wrap = (end_room < need) ? end_room : 0ull;
        if ((CLI_QUEUE_BYTES - channel->used) >= (need + wrap))
        {
            break;
        }
        cli_wake_wait(&s_wake, &s_lock);
    }
    const unsigned long long end_room = CLI_QUEUE_BYTES - channel->head;
    if (end_room < need)
    {
        if (end_room >= CLI_QUEUE_HEAD)
        {
            CliQueued *const marker = (CliQueued *)(void *)(cli_bytes(channel) + channel->head);
            marker->total = 0ull;
        }
        channel->used += end_room;
        channel->head = 0ull;
    }
    unsigned char *const at = cli_bytes(channel) + channel->head;
    // the path and the name are below a ring, and a priority one of three, which an unsigned int holds
    const CliQueued item = {(unsigned int)path_length, (unsigned int)name_length, priority, 0u, size, need};
    memor.cpy(&(MemoriaCfg){.dst = at, .src = &item, .bytes = sizeof(item)});
    memor.cpy(&(MemoriaCfg){.dst = at + CLI_QUEUE_HEAD, .src = path, .bytes = path_length + 1ull});
    memor.cpy(&(MemoriaCfg){.dst = at + CLI_QUEUE_HEAD + path_length + 1ull, .src = name, .bytes = name_length + 1ull});
    memor.cpy(&(MemoriaCfg){
        .dst = at + CLI_QUEUE_HEAD + path_length + 1ull + name_length + 1ull, .src = bytes, .bytes = size});
    channel->head += need;
    channel->head = (channel->head == CLI_QUEUE_BYTES) ? 0ull : channel->head;
    channel->used += need;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

void cli_qry_queued(CliLine *out)
{
    cli_lock_take(&s_lock);
    cli_line_text(out, "{\"handed\":");
    cli_line_decimal(out, s_handed, 1u);
    cli_line_text(out, ",\"dumped\":0,\"failed\":");
    cli_line_decimal(out, s_failed, 1u);
    cli_line_text(out, ",\"said\":");
    cli_json_quoted(out, s_said, cellul.len(&(CatenaFinitaCfg){.src = s_said, .cap = CLI_CELLS_POOL}));
    cli_line_character(out, '}');
    cli_lock_give(&s_lock);
}
