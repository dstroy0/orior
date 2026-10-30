// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_room.c: the tree's check's room: four cells taken as the check starts and given back as it is let go,
// each begun small and doubled as it fills up to its most: the tree's files' paths, where each starts, the files
// waiting in a ring, and the paths of those. Every call with the queue's lock acquired
#include "../command_line/cells.h"
#include "servers_internal.h"

#include <stdlib.h>

// the most files waiting, the bytes their paths take, and the most of the tree's files and the bytes they take; and
// what each part of the room begins with
#define CLI_SERVERS_WAITING (1u << 16u)
#define CLI_SERVERS_WAITING_ROOM (32ull << 20u)
#define CLI_SERVERS_TREE_FILES (1u << 18u)
#define CLI_SERVERS_TREE_ROOM (32ull << 20u)
#define CLI_SERVERS_WAITING_FIRST 64u
#define CLI_SERVERS_FILES_FIRST 1024u
#define CLI_SERVERS_ROOM_FIRST (64ull << 10u)

// the owner every cell of the room is counted to, as the cells' audit lists it
static CellOwner s_owner = MMGR_CELL_OWNER("the tree's check");

// the files waiting by their places in the ring, sorted by where their paths stand, and the queue they are of
static unsigned int s_order[CLI_SERVERS_WAITING];
static const CliServersQueue *s_ordering;

void cli_servers_check_room_given(CliServersQueue *queue)
{
    g_cli_cells->persistent_buf_release(queue->files, &s_owner);
    g_cli_cells->persistent_buf_release(queue->file_at, &s_owner);
    g_cli_cells->persistent_buf_release(queue->waiting, &s_owner);
    g_cli_cells->persistent_buf_release(queue->room, &s_owner);
    queue->files = NULL;
    queue->file_at = NULL;
    queue->waiting = NULL;
    queue->room = NULL;
}

int cli_servers_check_room_made(CliServersQueue *queue)
{
    if (queue->files != NULL)
    {
        return 1;
    }
    queue->files = g_cli_cells->persistent_buf_alloc(CLI_SERVERS_ROOM_FIRST, &s_owner);
    queue->file_at = g_cli_cells->persistent_buf_alloc(CLI_SERVERS_FILES_FIRST * sizeof(*queue->file_at), &s_owner);
    queue->waiting = g_cli_cells->persistent_buf_alloc(CLI_SERVERS_WAITING_FIRST * sizeof(*queue->waiting), &s_owner);
    queue->room = g_cli_cells->persistent_buf_alloc(CLI_SERVERS_ROOM_FIRST, &s_owner);
    queue->files_most = CLI_SERVERS_ROOM_FIRST;
    queue->file_at_most = CLI_SERVERS_FILES_FIRST;
    queue->waiting_most = CLI_SERVERS_WAITING_FIRST;
    queue->room_most = CLI_SERVERS_ROOM_FIRST;
    if ((queue->files == NULL) || (queue->file_at == NULL) || (queue->waiting == NULL) || (queue->room == NULL))
    {
        cli_servers_check_room_given(queue);
        return 0;
    }
    return 1;
}

// `most` doubled, capped at `ceiling`
static unsigned long long room_doubled(unsigned long long most, unsigned long long ceiling)
{
    return ((most * 2ull) < ceiling) ? (most * 2ull) : ceiling;
}

// a cell of `bytes` containing the first `used` bytes of `cell`, which is given back: the larger, or NULL with `cell`
// retained where the cells cannot meet it
static void *room_grown(void *cell, unsigned long long used, unsigned long long bytes)
{
    return cli_cell_regrown(cell, (size_t)used, bytes, &s_owner);
}

CliServersQueued *cli_servers_check_queued_at(const CliServersQueue *queue, unsigned int at)
{
    return &queue->waiting[(queue->head + at) % queue->waiting_most];
}

// the ring of files waiting moved into one of twice the places, no more than CLI_SERVERS_WAITING, laid in order from
// its first: 1, or 0 where it contains the most or the cells cannot meet the larger
static int room_waiting_grown(CliServersQueue *queue)
{
    // the places are at most CLI_SERVERS_WAITING, which an unsigned int stores
    const unsigned int most = (unsigned int)room_doubled(queue->waiting_most, CLI_SERVERS_WAITING);
    CliServersQueued *const larger =
        (most > queue->waiting_most) ? g_cli_cells->persistent_buf_alloc(most * sizeof(*larger), &s_owner) : NULL;
    if (larger == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < queue->count; at += 1u)
    {
        larger[at] = *cli_servers_check_queued_at(queue, at);
    }
    g_cli_cells->persistent_buf_release(queue->waiting, &s_owner);
    queue->waiting = larger;
    queue->waiting_most = most;
    queue->head = 0u;
    return 1;
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
        memor.move_down(
            &(MemoriaCfg){.dst = queue->room + used, .src = queue->room + one->path, .bytes = one->size + 1u});
        one->path = used;
        used += one->size + 1u;
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
    if (queue->used + size + 1u > queue->room_most)
    {
        room_compacted(queue);
    }
    // the paths' room doubled while the path does not fit and the room is short of its most
    while ((queue->used + size + 1u > queue->room_most) && (queue->room_most < CLI_SERVERS_WAITING_ROOM))
    {
        const unsigned long long most = room_doubled(queue->room_most, CLI_SERVERS_WAITING_ROOM);
        char *const larger = room_grown(queue->room, queue->used, most);
        if (larger == NULL)
        {
            break;
        }
        queue->room = larger;
        queue->room_most = most;
    }
    if (queue->count == queue->waiting_most)
    {
        (void)room_waiting_grown(queue);
    }
    if ((queue->count == queue->waiting_most) || (queue->used + size + 1u > queue->room_most))
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
    *one = (CliServersQueued){queue->used, size, *waiting};
    queue->used += size + 1u;
    return 1;
}

void cli_servers_check_files_put(CliServersQueue *queue, const char *const *files, unsigned int count)
{
    for (unsigned int at = 0u; (queue->files != NULL) && (at < count); at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = files[at], .cap = CLI_CELLS_POOL}) + 1u;
        while ((queue->files_used + size > queue->files_most) && (queue->files_most < CLI_SERVERS_TREE_ROOM))
        {
            const unsigned long long most = room_doubled(queue->files_most, CLI_SERVERS_TREE_ROOM);
            char *const larger = room_grown(queue->files, queue->files_used, most);
            if (larger == NULL)
            {
                return;
            }
            queue->files = larger;
            queue->files_most = most;
        }
        if ((queue->file_count == queue->file_at_most) && (queue->file_at_most < CLI_SERVERS_TREE_FILES))
        {
            // the starts are at most CLI_SERVERS_TREE_FILES, which an unsigned int stores
            const unsigned int most = (unsigned int)room_doubled(queue->file_at_most, CLI_SERVERS_TREE_FILES);
            unsigned long long *const larger =
                room_grown(queue->file_at, queue->file_count * sizeof(*larger), most * sizeof(*larger));
            if (larger == NULL)
            {
                return;
            }
            queue->file_at = larger;
            queue->file_at_most = most;
        }
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
