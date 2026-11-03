// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_work.c: the model's work that blocks, on a thread for each lane: a file read on one, what waits on a
// language server on another, so a server that does not answer holds up no file. The controller's thread writes a
// lane's work ring and reads its done ring and the lane's thread does the other, so each ring has one writer and one
// reader. A piece of work runs on the lane and what it found goes back to the controller, whose thread runs its `done`;
// nothing of it reaches the window
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

// One lane: its two rings and the thread that runs it
typedef struct
{
    mmgr_ring work_ring;
    mmgr_ring done_ring;
    CliWake wake;
    CliThread thread;
} CliWorkLane;

static CliWorkLane s_lane[CLI_WORK_LANES];
static uint8_t *s_cell;
static embed_atomic_u32 s_ending;
static embed_atomic_u32 s_running;
static CliWake s_ended;

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
        (void)EMBED_ATOMIC_ADD_32(&s_running, 1u);
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &lane_at->thread))
        {
            (void)EMBED_ATOMIC_ADD_32(&s_running, (embed_u32)-1);
        }
    }
    return 1;
}

int cli_controller_work_try_post(const CliControllerWorkCfg *args)
{
    CliWorkLane *const lane_at = &s_lane[args->lane_at];
    size_t at = 0u;
    if (!anularis.seg_next(&(AnularisCfg){.ring = &lane_at->work_ring, .out_index = &at}))
    {
        return 0;
    }
    *(CliWorkPiece *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &lane_at->work_ring, .index = at}) =
        (CliWorkPiece){args->run, args->done, args->context, args->busy};
    if (args->busy)
    {
        g_cli_model.dirty |= (g_cli_model.busy == 0u) ? CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS) : 0u;
        g_cli_model.busy += 1u;
    }
    anularis.seg_publish(&(AnularisCfg){.ring = &lane_at->work_ring});
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &lane_at->wake);
    return 1;
}

void cli_controller_work_post(const CliControllerWorkCfg *args)
{
    // a full ring runs the piece here, a lane far behind already, which keeps every piece's `done` promised
    if (!cli_controller_work_try_post(args))
    {
        args->run(args->context);
        args->done(args->context);
    }
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
            piece.done(piece.context);
            did = 1;
        }
    }
    return did;
}

void cli_controller_work_end(void)
{
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
    g_cli_cells->persistent_buf_release(s_cell, &g_cli_controller_cells);
    s_cell = NULL;
}
