// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller.c: the controller's thread and its rings. The view's thread writes the input's two rings and moves each
// part's ring on as it reads; the controller's thread reads the input's rings and writes the parts' rings, so each ring
// has one writer and one reader. A part's ring holds where each snapshot stands; the view keeps its place on the
// newest, and every segment behind that place is one the controller releases the snapshot of, read off the ring's own
// count. No byte of a ring is scrubbed, so a segment still names its snapshot after the view has moved past it. A
// thread that finds nothing to do tests its rings again after reading the wake's generation and sleeps on that
// generation, so a write after the test ends the sleep.
#include "controller.h"
#include "controller_internal.h"

#include "../command_line/cells.h"

// the inputs and the snapshots of a part the rings hold, each a power of two, and the bytes of the inputs' names, text
// and paths on their way
#define CLI_CONTROLLER_INPUTS 256u
#define CLI_CONTROLLER_SNAPSHOTS 8u
#define CLI_CONTROLLER_INPUT_BYTES 65536u
// the smallest power of two a CliInput's segment fits, which the ring's segments are a division of
#define CLI_CONTROLLER_INPUT_SEGMENT 64u
EMBED_STATIC_ASSERT(sizeof(CliInput) <= CLI_CONTROLLER_INPUT_SEGMENT,
                    "a CliInput fits one segment of the input's ring");
EMBED_STATIC_ASSERT(sizeof(void *) <= 8u, "where a snapshot stands fits one segment of a part's ring");

// One part: its ring, where its snapshots stand, those published and those released, and whether its newest waits where
// the ring is full, and that one, NULL where the part now shows nothing
typedef struct
{
    mmgr_ring ring;
    size_t published;
    size_t released;
    int has_waiting;
    void *waiting;
} CliControllerPart;

typedef struct
{
    mmgr_ring input_ring;
    mmgr_ring input_bytes_ring;
    CliControllerPart part[CLI_SNAPSHOT_PARTS];
    uint8_t *cell;
    CliWake wake;
    CliThread thread;
    CliControllerWoken woken;
    void *context;
    _Alignas(64) embed_atomic_u32 ending;
    _Alignas(64) embed_atomic_u32 running;
    CliWake ended;
} CliController;

static CliController s_controller;

// the snapshots of `part` the view has moved past released, the ring's count saying how many
static void controller_part_reclaim(unsigned int part)
{
    CliControllerPart *const one = &s_controller.part[part];
    const size_t held = anularis.seg_inflight(&(AnularisCfg){.ring = &one->ring});
    while ((one->released + held) < one->published)
    {
        void *const *const passed = (void *const *)(void *)anularis.seg_at(
            &(AnularisCfg){.ring = &one->ring, .index = one->released & (CLI_CONTROLLER_SNAPSHOTS - 1u)});
        cli_controller_snapshot_release(part, *passed);
        one->released += 1u;
    }
}

// `snapshot` written to the ring of `part`: 1, or 0 where the ring is full
static int controller_part_write(unsigned int part, void *snapshot)
{
    CliControllerPart *const one = &s_controller.part[part];
    size_t at = 0u;
    if (!anularis.seg_next(&(AnularisCfg){.ring = &one->ring, .out_index = &at}))
    {
        return 0;
    }
    *(void **)(void *)anularis.seg_at(&(AnularisCfg){.ring = &one->ring, .index = at}) = snapshot;
    anularis.seg_publish(&(AnularisCfg){.ring = &one->ring});
    one->published += 1u;
    s_controller.woken(s_controller.context);
    return 1;
}

void cli_controller_publish(unsigned int part, void *snapshot)
{
    CliControllerPart *const one = &s_controller.part[part];
    controller_part_reclaim(part);
    if (one->has_waiting)
    {
        cli_controller_snapshot_release(part, one->waiting);
        one->has_waiting = 0;
    }
    if (!controller_part_write(part, snapshot))
    {
        one->waiting = snapshot;
        one->has_waiting = 1;
    }
}

// every part's passed snapshots released and its waiting one written where its ring has moved on: 1 where one still
// waits
static int controller_parts_step(void)
{
    int waits = 0;
    for (unsigned int part = 0u; part < CLI_SNAPSHOT_PARTS; part += 1u)
    {
        CliControllerPart *const one = &s_controller.part[part];
        controller_part_reclaim(part);
        if (one->has_waiting && controller_part_write(part, one->waiting))
        {
            one->has_waiting = 0;
        }
        waits = waits || one->has_waiting;
    }
    return waits;
}

// what the controller's thread runs: each input in turn, until the controller ends. While a snapshot waits on a full
// ring the thread looks again a millisecond on, the view's moving on being no call of its own
static void controller_run(void *context)
{
    (void)context;
    cli_controller_model_begin();
    char *const bytes = g_cli_cells->persistent_buf_alloc(CLI_CONTROLLER_INPUT_BYTES, &g_cli_controller_cells);
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_controller.wake);
        const int waits = controller_parts_step();
        size_t at = 0u;
        if (anularis.seg_front(&(AnularisCfg){.ring = &s_controller.input_ring, .out_index = &at}))
        {
            const CliInput input = *(const CliInput *)(const void *)anularis.seg_at(
                &(AnularisCfg){.ring = &s_controller.input_ring, .index = at});
            anularis.seg_release(&(AnularisCfg){.ring = &s_controller.input_ring});
            const size_t length = anularis.read(&(AnularisCfg){
                .ring = &s_controller.input_bytes_ring, .dst = (uint8_t *)(void *)bytes, .bytes = input.length});
            bytes[(length < (CLI_CONTROLLER_INPUT_BYTES - 1u)) ? length : (CLI_CONTROLLER_INPUT_BYTES - 1u)] = '\0';
            cli_controller_model_input(&input, bytes);
            continue;
        }
        if (EMBED_ATOMIC_LOAD(&s_controller.ending) != 0u)
        {
            break;
        }
        if (cli_controller_model_step())
        {
            continue;
        }
        const unsigned long long due_ms = waits ? 1ull : cli_controller_model_due_ms();
        if (due_ms != 0ull)
        {
            (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &s_controller.wake, .seen = seen,
                             .milliseconds_u64 = due_ms);
        }
        else
        {
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_controller.wake, .seen = seen);
        }
    }
    g_cli_cells->persistent_buf_release(bytes, &g_cli_controller_cells);
    cli_controller_model_end();
    (void)EMBED_ATOMIC_ADD_32(&s_controller.running, (embed_u32)-1);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_controller.ended);
}

void cli_controller_wake(void)
{
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_controller.wake);
}

int cli_controller_begin(CliControllerWoken woken, void *context)
{
    memor.set(&(MemoriaCfg){.dst = &s_controller, .val = 0, .bytes = sizeof(s_controller)});
    s_controller.woken = woken;
    s_controller.context = context;
    // the rings' bytes in one cell: the inputs, their bytes, and where each part's snapshots stand
    const size_t input_bytes = (size_t)CLI_CONTROLLER_INPUTS * CLI_CONTROLLER_INPUT_SEGMENT;
    const size_t part_bytes = CLI_CONTROLLER_SNAPSHOTS * sizeof(uint64_t);
    s_controller.cell = g_cli_cells->persistent_buf_alloc(
        input_bytes + CLI_CONTROLLER_INPUT_BYTES + (CLI_SNAPSHOT_PARTS * part_bytes), &g_cli_controller_cells);
    if (s_controller.cell == NULL)
    {
        return 0;
    }
    (void)anularis.init(&(AnularisCfg){.ring = &s_controller.input_ring,
                                       .buf = s_controller.cell,
                                       .capacity = input_bytes,
                                       .segment_count = CLI_CONTROLLER_INPUTS});
    (void)anularis.init(&(AnularisCfg){.ring = &s_controller.input_bytes_ring,
                                       .buf = s_controller.cell + input_bytes,
                                       .capacity = CLI_CONTROLLER_INPUT_BYTES,
                                       .segment_count = 1u});
    uint8_t *const parts = s_controller.cell + input_bytes + CLI_CONTROLLER_INPUT_BYTES;
    for (unsigned int part = 0u; part < CLI_SNAPSHOT_PARTS; part += 1u)
    {
        (void)anularis.init(&(AnularisCfg){.ring = &s_controller.part[part].ring,
                                           .buf = parts + (part * part_bytes),
                                           .capacity = part_bytes,
                                           .segment_count = CLI_CONTROLLER_SNAPSHOTS});
    }
    s_controller.thread.run = controller_run;
    s_controller.thread.context = NULL;
    s_controller.thread.priority = CLI_PRIORITY_WORK;
    (void)EMBED_ATOMIC_ADD_32(&s_controller.running, 1u);
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_controller.thread))
    {
        (void)EMBED_ATOMIC_ADD_32(&s_controller.running, (embed_u32)-1);
        g_cli_cells->persistent_buf_release(s_controller.cell, &g_cli_controller_cells);
        s_controller.cell = NULL;
        return 0;
    }
    return 1;
}

void cli_controller_end(void)
{
    EMBED_ATOMIC_STORE(&s_controller.ending, 1u);
    EMBED_ATOMIC_ORDERED();
    cli_controller_wake();
    while (EMBED_ATOMIC_LOAD(&s_controller.running) != 0u)
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_controller.ended,
                         .milliseconds_u64 = 1ull);
    }
    // the controller's thread has ended and the view reads no more: every snapshot still standing released
    for (unsigned int part = 0u; part < CLI_SNAPSHOT_PARTS; part += 1u)
    {
        CliControllerPart *const one = &s_controller.part[part];
        if (one->has_waiting)
        {
            cli_controller_snapshot_release(part, one->waiting);
            one->has_waiting = 0;
        }
        while (one->released < one->published)
        {
            void *const *const passed = (void *const *)(void *)anularis.seg_at(
                &(AnularisCfg){.ring = &one->ring, .index = one->released & (CLI_CONTROLLER_SNAPSHOTS - 1u)});
            cli_controller_snapshot_release(part, *passed);
            one->released += 1u;
        }
    }
    g_cli_cells->persistent_buf_release(s_controller.cell, &g_cli_controller_cells);
    s_controller.cell = NULL;
}

void cli_controller_input(const CliInput *input, const char *bytes)
{
    size_t at = 0u;
    if ((anularis.vacant(&(AnularisCfg){.ring = &s_controller.input_bytes_ring}) >= input->length) &&
        anularis.seg_next(&(AnularisCfg){.ring = &s_controller.input_ring, .out_index = &at}))
    {
        (void)anularis.put(&(AnularisCfg){.ring = &s_controller.input_bytes_ring,
                                          .src = (const uint8_t *)(const void *)bytes,
                                          .bytes = input->length});
        *(CliInput *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &s_controller.input_ring, .index = at}) = *input;
        anularis.seg_publish(&(AnularisCfg){.ring = &s_controller.input_ring});
        cli_controller_wake();
    }
}

const void *cli_controller_latest(unsigned int part)
{
    CliControllerPart *const one = &s_controller.part[part];
    while (anularis.seg_inflight(&(AnularisCfg){.ring = &one->ring}) > 1u)
    {
        anularis.seg_release(&(AnularisCfg){.ring = &one->ring});
    }
    size_t at = 0u;
    if (!anularis.seg_front(&(AnularisCfg){.ring = &one->ring, .out_index = &at}))
    {
        return NULL;
    }
    return *(void *const *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = &one->ring, .index = at});
}
