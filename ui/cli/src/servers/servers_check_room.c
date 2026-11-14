// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_room.c: the tree's check's room: the movable cell for the tree's files, taken as the check starts and
// given back as it is let go, sized from the tree's files; and the files waiting, each one movable cell whose token is
// pushed on one of the check's two ticket rings under a ticket of its own, the ring's proofs holding the checkers'
// pops in ticket order. Every call with the queue's lock acquired
#include "../command_line/pool.h"
#include "../command_line/rings.h"
#include "servers_internal.h"

// the files a capacity holds past those of the tree it is made for
#define CLI_SERVERS_POSITIONS_MORE 64u

int cli_servers_check_work_state_init(const CliServersQueueFilesCtx *args)
{
    CliServersQueue *const queue = args->queue;
    const char *const *files = args->files;
    // the files of the tree checked and their paths' bytes, and as many over for those a change adds
    unsigned long long bytes = 64ULL << 10u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = files[at], .cap = CLI_POOL_BYTES}) + 2ULL;
    }
    const unsigned int required = args->count + CLI_SERVERS_POSITIONS_MORE;
    const unsigned int positions = (required < CLI_SERVERS_FILES_MAX) ? required : CLI_SERVERS_FILES_MAX;
    if (!queue->positioned)
    {
        EmbedTicketRingSlot *const slots =
            (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_SERVERS_CHECK);
        embed_ticket_ring_begin(&queue->waiting.ring, slots, CLI_SERVERS_CHECK_RING_SHIFT);
        embed_ticket_ring_begin(&queue->first.ring, slots + (1u << CLI_SERVERS_CHECK_RING_SHIFT),
                                CLI_SERVERS_CHECK_FIRST_SHIFT);
        queue->positioned = 1;
    }
    if ((queue->files_cell != NULL) && (queue->files_max >= bytes) && (queue->file_at_max >= positions))
    {
        return 1;
    }
    if (queue->files_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(queue->files_cell, g_cli_servers_check_work_state_owner);
    }
    // the starts, the waiting bytes and the texts in one cell
    queue->files_cell = g_cli_guard->movable_buf_alloc(
        CLI_SERVERS_FILE_AT(positions) + (unsigned long long)positions + bytes, g_cli_servers_check_work_state_owner);
    queue->files_max = bytes;
    queue->file_at_max = positions;
    if (queue->files_cell == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_servers_queue_files_internal.check_queued_cleared, CliServersQueueFilesCtx, .queue = queue);
    return 1;
}

void cli_servers_check_queued_cleared(const CliServersQueueFilesCtx *args)
{
    static const unsigned char s_zeros[256];
    CliServersQueue *const queue = args->queue;
    for (unsigned int at = 0u; (queue->files_cell != NULL) && (at < queue->file_at_max); at += sizeof(s_zeros))
    {
        const unsigned int left = queue->file_at_max - at;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = queue->files_cell,
                                .offset = CLI_SERVERS_QUEUED_AT(queue, at),
                                .src = s_zeros,
                                .bytes = (left < sizeof(s_zeros)) ? left : sizeof(s_zeros)});
    }
}

int cli_servers_check_queue_append(const CliServersQueueCtx *args)
{
    CliServersQueue *const queue = args->queue;
    CliServersCheckRing *const ring = args->front ? &queue->first : &queue->waiting;
    if (!queue->positioned)
    {
        return 0;
    }
    // the ring full, the file is refused; the queuing threads hold the queue's lock, so the ticket read here is the
    // one taken below, and a checker only makes room
    const embed_u32 ticket = embed_atomic_u32_load(&ring->ticket);
    if (!cli_ticket_room(&ring->ring, ticket))
    {
        return 0;
    }
    // the head, then the path and its 0, then the name and its 0, in one movable cell whose token rides the ring
    const unsigned int size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    const unsigned int named =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = args->waiting->id, .cap = CLI_NAME_MAX_LEN}) + 1u;
    const CarcerMovableEntry *const job = g_cli_guard->movable_buf_alloc(sizeof(CliServersQueued) + size + 1u + named,
                                                                         g_cli_servers_check_work_state_owner);
    if (job == NULL)
    {
        return 0;
    }
    const CliServersQueued head = {args->waiting->server, embed_atomic_u64_load(&queue->generation), args->file, size};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = job, .offset = 0ULL, .src = &head, .bytes = sizeof(head)});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = job, .offset = sizeof(head), .src = args->path, .bytes = size + 1u});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = job, .offset = sizeof(head) + size + 1u, .src = args->waiting->id, .bytes = named});
    (void)EMBED_ATOMIC_ADD_32(&ring->ticket, 1u);
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    // Explicit cast carries the cell's token as the pass's entry, and as the word the slot holds
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)job;
    embed_u64 seen = 0u;
    (void)embed_ticket_ring_granted(&ring->turnstiles, address, &ring->ring, stamp, &seen);
    (void)embed_ticket_ring_pushed(&ring->turnstiles, address, &ring->ring, stamp, (void *)(embed_word)job, &seen);
    if ((args->file != CLI_SERVERS_NO_FILE) && (args->file < queue->file_at_max))
    {
        static const unsigned char s_waiting = 1u;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = queue->files_cell,
                                                                 .offset = CLI_SERVERS_QUEUED_AT(queue, args->file),
                                                                 .src = &s_waiting,
                                                                 .bytes = 1u});
    }
    return 1;
}

unsigned int cli_servers_check_waiting(const CliServersQueue *queue)
{
    unsigned int waiting = 0u;
    const CliServersCheckRing *const rings[2] = {&queue->waiting, &queue->first};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        // the checkers take tickets ahead of the pushes, so the taken past the pushed count as none waiting
        const embed_u32 pushed = embed_atomic_u32_load(&rings[at]->ticket);
        const embed_u32 taken = embed_atomic_u32_load(&rings[at]->taken);
        waiting += (pushed > taken) ? pushed - taken : 0u;
    }
    return waiting;
}

void cli_servers_check_files_append(const CliServersQueueFilesCtx *args)
{
    static const unsigned char s_idle = 0u;
    CliServersQueue *const queue = args->queue;
    for (unsigned int at = 0u; (queue->files_cell != NULL) && (at < args->count); at += 1u)
    {
        const embed_word size = cellul.len(&(CatenaFinitaCfg){.src = args->files[at], .cap = CLI_POOL_BYTES}) + 1u;
        if ((queue->file_count == queue->file_at_max) || (queue->files_used + size > queue->files_max))
        {
            return;
        }
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = queue->files_cell,
                                .offset = CLI_SERVERS_FILES_TEXT_AT(queue, queue->files_used),
                                .src = args->files[at],
                                .bytes = size});
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = queue->files_cell,
                                                                 .offset = CLI_SERVERS_FILE_AT(queue->file_count),
                                                                 .src = &queue->files_used,
                                                                 .bytes = sizeof(queue->files_used)});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = queue->files_cell,
                                .offset = CLI_SERVERS_QUEUED_AT(queue, queue->file_count),
                                .src = &s_idle,
                                .bytes = 1u});
        queue->file_count += 1u;
        queue->files_used += size;
    }
}
