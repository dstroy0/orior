// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_room.c: the tree's check's room: four cells taken as the check starts and given back as it is let go,
// each sized from the tree's files: their paths, where each starts, the files waiting in a ring, and the paths of
// those. Every call with the queue's lock acquired
#include "../command_line/cells.h"
#include "servers_internal.h"

#include <stdlib.h>

// the most files waiting; and the bytes and the files a room holds past those of the tree it is made for
#define CLI_SERVERS_WAITING (1u << 16u)
#define CLI_SERVERS_ROOM_MORE (64ull << 10u)
#define CLI_SERVERS_PLACES_MORE 64u

// the files waiting by their places in the ring, sorted by where their paths stand, and the queue they are of
static unsigned int s_order[CLI_SERVERS_WAITING];
static const CliServersQueue *s_ordering;

void cli_servers_check_room_free(const CliServersQueueFilesCtx *args)
{
    g_cli_cells->persistent_buf_release(args->queue->files, &g_cli_servers_check_room_cells);
    g_cli_cells->persistent_buf_release(args->queue->file_at, &g_cli_servers_check_room_cells);
    g_cli_cells->persistent_buf_release(args->queue->waiting, &g_cli_servers_check_room_cells);
    g_cli_cells->persistent_buf_release(args->queue->room, &g_cli_servers_check_room_cells);
    args->queue->files = NULL;
    args->queue->file_at = NULL;
    args->queue->waiting = NULL;
    args->queue->room = NULL;
}

int cli_servers_check_room_init(const CliServersQueueFilesCtx *args)
{
    const char *const *files = args->files;
    // the files of the tree checked and their paths' bytes, and as many over for those a change adds
    unsigned long long bytes = CLI_SERVERS_ROOM_MORE;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = files[at], .cap = CLI_CELLS_POOL}) + 2ull;
    }
    const unsigned int wanted = args->count + CLI_SERVERS_PLACES_MORE;
    const unsigned int places = (wanted < CLI_SERVERS_WAITING) ? wanted : CLI_SERVERS_WAITING;
    if ((args->queue->files != NULL) &&
        ((args->queue->count != 0u) || ((args->queue->files_most >= bytes) && (args->queue->file_at_most >= places))))
    {
        return 1;
    }
    cli_servers_queue_files_internal.check_room_free(args);
    args->queue->files = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_servers_check_room_cells);
    args->queue->file_at =
        g_cli_cells->persistent_buf_alloc(places * sizeof(*args->queue->file_at), &g_cli_servers_check_room_cells);
    args->queue->waiting =
        g_cli_cells->persistent_buf_alloc(places * sizeof(*args->queue->waiting), &g_cli_servers_check_room_cells);
    args->queue->room = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_servers_check_room_cells);
    args->queue->files_most = bytes;
    args->queue->file_at_most = places;
    args->queue->waiting_most = places;
    args->queue->room_most = bytes;
    if ((args->queue->files == NULL) || (args->queue->file_at == NULL) || (args->queue->waiting == NULL) ||
        (args->queue->room == NULL))
    {
        cli_servers_queue_files_internal.check_room_free(args);
        return 0;
    }
    return 1;
}

CliServersQueued *cli_servers_check_queued_at(const CliServersQueueAtCtx *args)
{
    return &args->queue_read->waiting[(args->queue_read->head + args->at) % args->queue_read->waiting_most];
}

// two files waiting in the order their paths stand in the room
static int room_order(const void *one, const void *other)
{
    const unsigned long long left = EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                               .queue_read = s_ordering, .at = *(const unsigned int *)one)
                                        ->path;
    const unsigned long long right = EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                                .queue_read = s_ordering, .at = *(const unsigned int *)other)
                                         ->path;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

typedef struct
{
    CliServersQueue *queue;
} ServersCheckRoomCtx;
// the room's paths moved down together, its free bytes all after the last
EMBED_INLINE void room_compact(const ServersCheckRoomCtx *args)
{
    CliServersQueue *queue = args->queue;
    for (unsigned int at = 0u; at < queue->count; at += 1u)
    {
        s_order[at] = at;
    }
    s_ordering = queue;
    qsort(s_order, queue->count, sizeof(s_order[0]), room_order);
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < queue->count; at += 1u)
    {
        CliServersQueued *const one = EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                                 .queue_read = queue, .at = s_order[at]);
        memor.move_down(&(MemoriaCfg){.dst = queue->room + used, .src = queue->room + one->path, .bytes = one->whole});
        one->path = used;
        used += one->whole;
    }
    queue->used = used;
}

int cli_servers_check_queue_put(const CliServersQueueCtx *args)
{
    const unsigned int size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    if (args->queue->room == NULL)
    {
        return 0;
    }
    // the path and its 0, then the name and its 0
    const unsigned int named =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = args->waiting->id, .cap = CLI_NAME_MAX_LEN}) + 1u;
    const unsigned int whole = size + 1u + named;
    if (args->queue->used + whole > args->queue->room_most)
    {
        EMBED_CALL(room_compact, ServersCheckRoomCtx, .queue = args->queue);
    }
    if ((args->queue->count == args->queue->waiting_most) || (args->queue->used + whole > args->queue->room_most))
    {
        return 0;
    }
    if (args->front)
    {
        args->queue->head = (args->queue->head + args->queue->waiting_most - 1u) % args->queue->waiting_most;
    }
    CliServersQueued *const one = EMBED_CALL(cli_servers_queue_at_internal.check_queued_at, CliServersQueueAtCtx,
                                             .queue_read = args->queue, .at = args->front ? 0u : args->queue->count);
    args->queue->count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = args->queue->room + args->queue->used, .src = args->path, .bytes = size + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = args->queue->room + args->queue->used + size + 1u, .src = args->waiting->id, .bytes = named});
    *one = (CliServersQueued){args->queue->used, size, whole, args->waiting->server};
    args->queue->used += whole;
    return 1;
}

void cli_servers_check_files_put(const CliServersQueueFilesCtx *args)
{
    for (unsigned int at = 0u; (args->queue->files != NULL) && (at < args->count); at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->files[at], .cap = CLI_CELLS_POOL}) + 1u;
        if ((args->queue->file_count == args->queue->file_at_most) ||
            (args->queue->files_used + size > args->queue->files_most))
        {
            return;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = args->queue->files + args->queue->files_used, .src = args->files[at], .bytes = size});
        args->queue->file_at[args->queue->file_count] = args->queue->files_used;
        args->queue->file_count += 1u;
        args->queue->files_used += size;
    }
}
