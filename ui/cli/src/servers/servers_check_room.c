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

void cli_servers_check_room_given(CliServersQueue *queue)
{
    g_cli_cells->persistent_buf_release(queue->files, &g_cli_servers_check_room_cells);
    g_cli_cells->persistent_buf_release(queue->file_at, &g_cli_servers_check_room_cells);
    g_cli_cells->persistent_buf_release(queue->waiting, &g_cli_servers_check_room_cells);
    g_cli_cells->persistent_buf_release(queue->room, &g_cli_servers_check_room_cells);
    queue->files = NULL;
    queue->file_at = NULL;
    queue->waiting = NULL;
    queue->room = NULL;
}

int cli_servers_check_room_made(CliServersQueue *queue, const char *const *files, unsigned int count)
{
    // the files of the tree checked and their paths' bytes, and as many over for those a change adds
    unsigned long long bytes = CLI_SERVERS_ROOM_MORE;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = files[at], .cap = CLI_CELLS_POOL}) + 2ull;
    }
    const unsigned int wanted = count + CLI_SERVERS_PLACES_MORE;
    const unsigned int places = (wanted < CLI_SERVERS_WAITING) ? wanted : CLI_SERVERS_WAITING;
    if ((queue->files != NULL) &&
        ((queue->count != 0u) || ((queue->files_most >= bytes) && (queue->file_at_most >= places))))
    {
        return 1;
    }
    cli_servers_check_room_given(queue);
    queue->files = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_servers_check_room_cells);
    queue->file_at =
        g_cli_cells->persistent_buf_alloc(places * sizeof(*queue->file_at), &g_cli_servers_check_room_cells);
    queue->waiting =
        g_cli_cells->persistent_buf_alloc(places * sizeof(*queue->waiting), &g_cli_servers_check_room_cells);
    queue->room = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_servers_check_room_cells);
    queue->files_most = bytes;
    queue->file_at_most = places;
    queue->waiting_most = places;
    queue->room_most = bytes;
    if ((queue->files == NULL) || (queue->file_at == NULL) || (queue->waiting == NULL) || (queue->room == NULL))
    {
        cli_servers_check_room_given(queue);
        return 0;
    }
    return 1;
}

CliServersQueued *cli_servers_check_queued_at(const CliServersQueue *queue, unsigned int at)
{
    return &queue->waiting[(queue->head + at) % queue->waiting_most];
}

// two files waiting in the order their paths stand in the room
static int room_order(const void *one, const void *other)
{
    const unsigned long long left = cli_servers_check_queued_at(s_ordering, *(const unsigned int *)one)->path;
    const unsigned long long right = cli_servers_check_queued_at(s_ordering, *(const unsigned int *)other)->path;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

// the room's paths moved down together, its free bytes all after the last
static void room_compacted(CliServersQueue *queue)
{
    for (unsigned int at = 0u; at < queue->count; at += 1u)
    {
        s_order[at] = at;
    }
    s_ordering = queue;
    qsort(s_order, queue->count, sizeof(s_order[0]), room_order);
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < queue->count; at += 1u)
    {
        CliServersQueued *const one = cli_servers_check_queued_at(queue, s_order[at]);
        memor.move_down(&(MemoriaCfg){.dst = queue->room + used, .src = queue->room + one->path, .bytes = one->whole});
        one->path = used;
        used += one->whole;
    }
    queue->used = used;
}

int cli_servers_check_queue_put(CliServersQueue *queue, const char *path, const CliServersWaiting *waiting, int front)
{
    const unsigned int size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    if (queue->room == NULL)
    {
        return 0;
    }
    // the path and its 0, then the name and its 0
    const unsigned int named =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = waiting->id, .cap = CLI_NAME_MAX_LEN}) + 1u;
    const unsigned int whole = size + 1u + named;
    if (queue->used + whole > queue->room_most)
    {
        room_compacted(queue);
    }
    if ((queue->count == queue->waiting_most) || (queue->used + whole > queue->room_most))
    {
        return 0;
    }
    if (front)
    {
        queue->head = (queue->head + queue->waiting_most - 1u) % queue->waiting_most;
    }
    CliServersQueued *const one = cli_servers_check_queued_at(queue, front ? 0u : queue->count);
    queue->count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = queue->room + queue->used, .src = path, .bytes = size + 1u});
    memor.cpy(&(MemoriaCfg){.dst = queue->room + queue->used + size + 1u, .src = waiting->id, .bytes = named});
    *one = (CliServersQueued){queue->used, size, whole, waiting->server};
    queue->used += whole;
    return 1;
}

void cli_servers_check_files_put(CliServersQueue *queue, const char *const *files, unsigned int count)
{
    for (unsigned int at = 0u; (queue->files != NULL) && (at < count); at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = files[at], .cap = CLI_CELLS_POOL}) + 1u;
        if ((queue->file_count == queue->file_at_most) || (queue->files_used + size > queue->files_most))
        {
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = queue->files + queue->files_used, .src = files[at], .bytes = size});
        queue->file_at[queue->file_count] = queue->files_used;
        queue->file_count += 1u;
        queue->files_used += size;
    }
}
