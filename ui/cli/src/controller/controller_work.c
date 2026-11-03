// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_work.c: the model's work that blocks, on a thread for each lane: a file read on one, what waits on a
// language server on another, so a server that does not answer holds up no file. The controller's thread writes a
// lane's work ring and reads its done ring and the lane's thread does the other, so each ring has one writer and one
// reader. A piece of work runs on the lane and what it found goes back to the controller, whose thread runs its `done`;
// nothing of it reaches the window. No piece runs on the controller's thread: one posted while its lane's ring has no
// segment free waits in a cell of the controller's, in the order posted, and each step posts those waiting as the lane
// frees segments
#include "controller_work.h"

#include "../command_line/cells.h"
#include "controller_internal.h"

// the pieces of work a lane's rings hold, a power of two, and the bytes of one, a power of two a piece fits
#define CLI_WORK_PIECES 256u
#define CLI_WORK_PIECE_BYTES 32u

// One piece of work: what runs on the lane, what runs on the controller's thread after it, what they share, a cell the
// `done` releases, and whether the window's busy circle turns for it
typedef struct
{
    void (*run)(void *context);
    void (*done)(void *context);
    void *context;
    int busy;
} CliWorkPiece;
EMBED_STATIC_ASSERT(sizeof(CliWorkPiece) <= CLI_WORK_PIECE_BYTES, "a piece of work fits one segment of a lane's rings");

// A piece waiting for a segment of its lane's ring, and the piece posted after it to the same lane
typedef struct CliWorkWaiting
{
    struct CliWorkWaiting *next;
    CliWorkPiece piece;
} CliWorkWaiting;

// One lane: its two rings, the thread that runs it, and on the controller's thread alone whether that thread started
// and the first and last piece waiting for a segment of its work ring
typedef struct
{
    mmgr_ring work_ring;
    mmgr_ring done_ring;
    CliWake wake;
    CliThread thread;
    int started;
    CliWorkWaiting *waiting;
    CliWorkWaiting *waiting_last;
} CliWorkLane;

// the lanes, their rings' cell, whether the work ends, how many lanes run, and what wakes as the last ends; and on the
// controller's thread alone, how many pieces were posted whose `done` has not run
static CliWorkLane s_lane[CLI_WORK_LANES];
static uint8_t *s_cell;
static embed_atomic_u32 s_ending;
static embed_atomic_u32 s_running;
static CliWake s_ended;
static unsigned int s_outstanding;

typedef struct
{
    CliWorkLane *lane_at;
    const CliWorkPiece *piece;
} ControllerWorkCtx;

// `piece` written into a segment of its lane's work ring and the lane woken: 1, or 0 where no segment is free
EMBED_INLINE int work_publish(const ControllerWorkCtx *args)
{
    size_t at = 0u;
    if (!anularis.seg_next(&(AnularisCfg){.ring = &args->lane_at->work_ring, .out_index = &at}))
    {
        return 0;
    }
    *(CliWorkPiece *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &args->lane_at->work_ring, .index = at}) =
        *args->piece;
    anularis.seg_publish(&(AnularisCfg){.ring = &args->lane_at->work_ring});
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->lane_at->wake);
    return 1;
}

// what a lane's thread runs: each piece in turn and its `done` handed back, until the work ends
static void work_lane_run(void *context)
{
    CliWorkLane *const lane_at = (CliWorkLane *)context;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &lane_at->wake);
        size_t at = 0u;
        if (anularis.seg_front(&(AnularisCfg){.ring = &lane_at->work_ring, .out_index = &at}))
        {
            const CliWorkPiece piece = *(const CliWorkPiece *)(const void *)anularis.seg_at(
                &(AnularisCfg){.ring = &lane_at->work_ring, .index = at});
            anularis.seg_release(&(AnularisCfg){.ring = &lane_at->work_ring});
            piece.run(piece.context);
            size_t done_at = 0u;
            // the controller drains the ring ahead of every input, so it stands full only for the moment that takes
            while (!anularis.seg_next(&(AnularisCfg){.ring = &lane_at->done_ring, .out_index = &done_at}))
            {
                (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &lane_at->wake,
                                 .milliseconds_u64 = 1ull);
            }
            *(CliWorkPiece *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &lane_at->done_ring, .index = done_at}) =
                piece;
            anularis.seg_publish(&(AnularisCfg){.ring = &lane_at->done_ring});
            cli_controller_wake();
            continue;
        }
        if (EMBED_ATOMIC_LOAD(&s_ending) != 0u)
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &lane_at->wake, .seen = seen);
    }
    (void)EMBED_ATOMIC_ADD_32(&s_running, (embed_u32)-1);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_ended);
}

int cli_controller_work_begin(void)
{
    const size_t ring_bytes = (size_t)CLI_WORK_PIECES * CLI_WORK_PIECE_BYTES;
    s_cell = g_cli_cells->persistent_buf_alloc(2u * CLI_WORK_LANES * ring_bytes, &g_cli_controller_cells);
    if (s_cell == NULL)
    {
        return 0;
    }
    EMBED_ATOMIC_STORE(&s_ending, 0u);
    for (unsigned int at = 0u; at < CLI_WORK_LANES; at += 1u)
    {
        CliWorkLane *const lane_at = &s_lane[at];
        uint8_t *const first = s_cell + (2u * at * ring_bytes);
        (void)anularis.init(&(AnularisCfg){
            .ring = &lane_at->work_ring, .buf = first, .capacity = ring_bytes, .segment_count = CLI_WORK_PIECES});
        (void)anularis.init(&(AnularisCfg){.ring = &lane_at->done_ring,
                                           .buf = first + ring_bytes,
                                           .capacity = ring_bytes,
                                           .segment_count = CLI_WORK_PIECES});
        lane_at->thread.run = work_lane_run;
        lane_at->thread.context = lane_at;
        lane_at->thread.priority = CLI_PRIORITY_WORK;
        lane_at->waiting = NULL;
        lane_at->waiting_last = NULL;
        (void)EMBED_ATOMIC_ADD_32(&s_running, 1u);
        lane_at->started = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &lane_at->thread);
        if (!lane_at->started)
        {
            (void)EMBED_ATOMIC_ADD_32(&s_running, (embed_u32)-1);
        }
    }
    return 1;
}

int cli_controller_work_post(const CliControllerWorkCfg *args)
{
    CliWorkLane *const lane_at = &s_lane[args->lane_at];
    if (!lane_at->started)
    {
        return 0;
    }
    const CliWorkPiece piece = {args->run, args->done, args->context, args->busy};
    // a piece waits behind those waiting before it, so each lane runs its pieces in the order posted
    if ((lane_at->waiting != NULL) || !EMBED_CALL(work_publish, ControllerWorkCtx, .lane_at = lane_at, .piece = &piece))
    {
        CliWorkWaiting *const waiting =
            g_cli_cells->persistent_buf_alloc(sizeof(*waiting), &g_cli_controller_cells);
        if (waiting == NULL)
        {
            return 0;
        }
        *waiting = (CliWorkWaiting){NULL, piece};
        if (lane_at->waiting == NULL)
        {
            lane_at->waiting = waiting;
        }
        else
        {
            lane_at->waiting_last->next = waiting;
        }
        lane_at->waiting_last = waiting;
    }
    if (args->busy)
    {
        g_cli_model.dirty |= (g_cli_model.busy == 0u) ? CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS) : 0u;
        g_cli_model.busy += 1u;
    }
    s_outstanding += 1u;
    return 1;
}

int cli_controller_work_step(void)
{
    int did = 0;
    for (unsigned int each = 0u; each < CLI_WORK_LANES; each += 1u)
    {
        CliWorkLane *const lane_at = &s_lane[each];
        size_t at = 0u;
        while (anularis.seg_front(&(AnularisCfg){.ring = &lane_at->done_ring, .out_index = &at}))
        {
            const CliWorkPiece piece = *(const CliWorkPiece *)(const void *)anularis.seg_at(
                &(AnularisCfg){.ring = &lane_at->done_ring, .index = at});
            anularis.seg_release(&(AnularisCfg){.ring = &lane_at->done_ring});
            if (piece.busy)
            {
                g_cli_model.busy -= 1u;
                g_cli_model.dirty |= (g_cli_model.busy == 0u) ? CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS) : 0u;
            }
            s_outstanding -= 1u;
            piece.done(piece.context);
            did = 1;
        }
    }
    for (unsigned int each = 0u; each < CLI_WORK_LANES; each += 1u)
    {
        CliWorkLane *const lane_at = &s_lane[each];
        while ((lane_at->waiting != NULL) &&
               EMBED_CALL(work_publish, ControllerWorkCtx, .lane_at = lane_at, .piece = &lane_at->waiting->piece))
        {
            CliWorkWaiting *const posted = lane_at->waiting;
            lane_at->waiting = posted->next;
            g_cli_cells->persistent_buf_release(posted, &g_cli_controller_cells);
            did = 1;
        }
    }
    return did;
}

void cli_controller_work_end(void)
{
    // every piece posted finishes before the lanes end, those waiting posted as segments free and any piece a `done`
    // posts as well
    CliWake pause = CLI_WAKE_INIT;
    while (s_outstanding != 0u)
    {
        if (!cli_controller_work_step())
        {
            (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 1ull);
        }
    }
    EMBED_ATOMIC_STORE(&s_ending, 1u);
    EMBED_ATOMIC_ORDERED();
    for (unsigned int at = 0u; at < CLI_WORK_LANES; at += 1u)
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_lane[at].wake);
    }
    while (EMBED_ATOMIC_LOAD(&s_running) != 0u)
    {
        (void)cli_controller_work_step();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_ended, .milliseconds_u64 = 1ull);
    }
    (void)cli_controller_work_step();
    for (unsigned int at = 0u; at < CLI_WORK_LANES; at += 1u)
    {
        s_lane[at].started = 0;
    }
    g_cli_cells->persistent_buf_release(s_cell, &g_cli_controller_cells);
    s_cell = NULL;
}
