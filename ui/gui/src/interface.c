// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface.c: the CLI in the window's process. A worker that finds its ring empty sets its idle bit and tests the ring
// again before it sleeps; the window's thread writes the call and then reads that bit, each with a full fence between,
// so whichever runs second sees the other's write and no wake is lost. A worker that is running is never woken.
#include "interface.h"
#include "../../cli/src/command_line/callback_cfg.h"

#include "../../cli/src/command_line/cells.h"

// the item ring this thread writes as its own, one past its index, 0 on a thread that is no worker; and the emitters'
// ring this thread tries first, one past its index, 0 until it first emits. An emitting thread returns to one ring, and
// its items keep their order unless another thread stands in that ring as it writes
static _Thread_local unsigned int t_own_items;
static _Thread_local unsigned int t_emitter;

typedef struct
{
    const char *name;
    const char *text;
    unsigned long long size;
    char **text_at;
    const GuiInterfaceItem *item;
    GuiInterface *cli;
    const GuiInterfaceItem *put;
    unsigned int ring;
    int own;
    int *woken;
} GuiInterfaceCtx;

// the mask bit of ring or worker `index`
EMBED_INLINE embed_u64 interface_bit(unsigned int index)
{
    return (embed_u64)1u << index;
}

// `name` and `size` bytes of `text` copied into one cell, each ended by a NUL, `*text_at` the text's start: the cell,
// or NULL where it could not be taken
EMBED_INLINE char *interface_copy(const GuiInterfaceCtx *args)
{
    char **text_at = args->text_at;
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    char *const cell = g_cli_cells->persistent_buf_alloc(name_length + 1ull + args->size + 1ull, &g_gui_interface_cells);
    if (cell == NULL)
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->name, .bytes = name_length});
    cell[name_length] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = cell + name_length + 1ull, .src = args->text, .bytes = args->size});
    cell[name_length + 1ull + args->size] = '\0';
    *text_at = cell + name_length + 1ull;
    return cell;
}

// an item released: its room, where it has one, and its name's cell
EMBED_INLINE void interface_item_free(const GuiInterfaceCtx *args)
{
    if (args->item->room < CLI_WINDOW_ROOMS)
    {
        cli_window_given(args->item->room);
    }
    g_cli_cells->persistent_buf_release(args->item->name, &g_gui_interface_cells);
}

// `put` written to item ring `ring`, which this thread alone writes as it stands here, sleeping a millisecond at a time
// while the ring is full: 1, or 0 where the interface ended first
EMBED_INLINE int interface_items_write(const GuiInterfaceCtx *args)
{
    mmgr_ring *const ring = &args->cli->items[args->ring].ring;
    size_t at = 0u;
    while (!anularis.seg_next(&(AnularisCfg){.ring = ring, .out_index = &at}))
    {
        if (EMBED_ATOMIC_LOAD(&args->cli->ending) != 0u)
        {
            return 0;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->cli->full,
                         .milliseconds_u64 = 1ull);
    }
    *(GuiInterfaceItem *)(void *)anularis.seg_at(&(AnularisCfg){.ring = ring, .index = at}) = *args->put;
    anularis.seg_publish(&(AnularisCfg){.ring = ring});
    return 1;
}

// `put` written and the window's thread woken: to this worker's own ring, or to an emitters' ring this thread claims
// for the one write, passing over any another thread stands in. An item coming once the interface ends, as the CLI's threads
// still emit, is released at once
EMBED_INLINE void interface_item_put(const GuiInterfaceCtx *args)
{
    GuiInterface *const cli = args->cli;
    if (EMBED_ATOMIC_LOAD(&cli->ending) != 0u)
    {
        EMBED_CALL(interface_item_free, GuiInterfaceCtx, .item = args->put);
        return;
    }
    int written = 0;
    unsigned int ring = 0u;
    if (t_own_items != 0u)
    {
        ring = t_own_items - 1u;
        written = EMBED_CALL(interface_items_write, GuiInterfaceCtx, .cli = cli, .ring = ring, .put = args->put);
    }
    else
    {
        if (t_emitter == 0u)
        {
            t_emitter = (EMBED_ATOMIC_ADD_32(&cli->emitter_next, 1u) % GUI_INTERFACE_EMITTERS) + 1u;
        }
        unsigned int at = t_emitter - 1u;
        while ((EMBED_ATOMIC_OR_64(&cli->emitters_taken, interface_bit(at)) & interface_bit(at)) != 0ull)
        {
            at = (at + 1u) % GUI_INTERFACE_EMITTERS;
        }
        t_emitter = at + 1u;
        ring = GUI_INTERFACE_WORKERS + at;
        written = EMBED_CALL(interface_items_write, GuiInterfaceCtx, .cli = cli, .ring = ring, .put = args->put);
        (void)EMBED_ATOMIC_AND_64(&cli->emitters_taken, ~interface_bit(at));
    }
    if (!written)
    {
        EMBED_CALL(interface_item_free, GuiInterfaceCtx, .item = args->put);
        return;
    }
    (void)EMBED_ATOMIC_OR_64(&cli->pending, interface_bit(ring));
    cli->woken(cli->context);
}

// what a worker runs: each call taken from its ring in turn, run through the CLI, its answer written, until the interface
// ends. The own lane runs what stands queued first, a save among it, its answers released as no one reads them; a pool
// worker's queued calls are asked of no one
static void interface_work(void *context)
{
    GuiInterfaceWorker *const worker = (GuiInterfaceWorker *)context;
    GuiInterface *const cli = worker->cli;
    const embed_u64 bit = interface_bit(worker->index);
    const int finishes = worker->index == GUI_INTERFACE_POOL;
    int marked = 0;
    t_own_items = worker->index + 1u;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &worker->wake);
        const int ending = EMBED_ATOMIC_LOAD(&cli->ending) != 0u;
        if (ending && !finishes)
        {
            break;
        }
        size_t at = 0u;
        if (!anularis.seg_front(&(AnularisCfg){.ring = &worker->calls_ring, .out_index = &at}))
        {
            if (ending)
            {
                break;
            }
            if (!marked)
            {
                (void)EMBED_ATOMIC_OR_64(&cli->idle, bit);
                EMBED_ATOMIC_ORDERED();
                marked = 1;
                continue;
            }
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &worker->wake, .seen = seen);
            marked = 0;
            continue;
        }
        (void)EMBED_ATOMIC_AND_64(&cli->idle, ~bit);
        marked = 0;
        const GuiInterfaceCall call = *(const GuiInterfaceCall *)(const void *)anularis.seg_at(
            &(AnularisCfg){.ring = &worker->calls_ring, .index = at});
        anularis.seg_release(&(AnularisCfg){.ring = &worker->calls_ring});
        // the answer stays in the call's room, read where it lies by the window's thread, which releases the room with
        // the call's cell once it has read it
        const unsigned int room = cli_window_called(call.own, call.name, call.args, call.size);
        const char *text = NULL;
        unsigned long long size = 0ull;
        const int ok = cli_window_answer(room, &text, &size);
        const GuiInterfaceItem put = {call.ticket, ok, call.name, (text != NULL) ? text : "", (text != NULL) ? size : 0ull,
                                 room};
        EMBED_CALL(interface_item_put, GuiInterfaceCtx, .cli = cli, .put = &put);
    }
    (void)EMBED_ATOMIC_ADD_32(&cli->workers, (embed_u32)-1);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &cli->ended);
}

// every item waiting in every ring released, from the window's thread as the interface ends
EMBED_INLINE void interface_items_free(const GuiInterfaceCtx *args)
{
    for (unsigned int ring = 0u; ring < GUI_INTERFACE_RINGS; ring += 1u)
    {
        mmgr_ring *const items = &args->cli->items[ring].ring;
        size_t at = 0u;
        while (anularis.seg_front(&(AnularisCfg){.ring = items, .out_index = &at}))
        {
            const GuiInterfaceItem item =
                *(const GuiInterfaceItem *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = items, .index = at});
            anularis.seg_release(&(AnularisCfg){.ring = items});
            EMBED_CALL(interface_item_free, GuiInterfaceCtx, .item = &item);
        }
    }
}

int gui_interface_begin(const GuiInterfaceCfg *args)
{
    GuiInterface *const cli = args->cli;
    memor.set(&(MemoriaCfg){.dst = cli, .val = 0, .bytes = sizeof(*cli)});
    cli->woken = args->woken;
    cli->context = args->context;
    for (unsigned int at = 0u; at < GUI_INTERFACE_RINGS; at += 1u)
    {
        (void)anularis.init(&(AnularisCfg){.ring = &cli->items[at].ring,
                                           .buf = (uint8_t *)(void *)cli->items[at].items,
                                           .capacity = sizeof(cli->items[at].items),
                                           .segment_count = GUI_INTERFACE_ITEMS,
                                           .segment_bytes = sizeof(GuiInterfaceItem)});
    }
    for (unsigned int at = 0u; at < GUI_INTERFACE_WORKERS; at += 1u)
    {
        GuiInterfaceWorker *const worker = &cli->worker[at];
        (void)anularis.init(&(AnularisCfg){.ring = &worker->calls_ring,
                                           .buf = (uint8_t *)(void *)worker->calls,
                                           .capacity = sizeof(worker->calls),
                                           .segment_count = GUI_INTERFACE_CALLS,
                                           .segment_bytes = sizeof(GuiInterfaceCall)});
        worker->index = at;
        worker->cli = cli;
        worker->thread.run = interface_work;
        worker->thread.context = worker;
        worker->thread.priority = CLI_PRIORITY_WORK;
    }
    for (unsigned int at = 0u; at < GUI_INTERFACE_WORKERS; at += 1u)
    {
        (void)EMBED_ATOMIC_ADD_32(&cli->workers, 1u);
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &cli->worker[at].thread))
        {
            (void)EMBED_ATOMIC_ADD_32(&cli->workers, (embed_u32)-1);
            gui_interface.end(args);
            return 0;
        }
    }
    return 1;
}

void gui_interface_end(const GuiInterfaceCfg *args)
{
    GuiInterface *const cli = args->cli;
    EMBED_ATOMIC_STORE(&cli->ending, 1u);
    EMBED_ATOMIC_ORDERED();
    for (unsigned int at = 0u; at < GUI_INTERFACE_WORKERS; at += 1u)
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &cli->worker[at].wake);
    }
    // the answers waiting stand in their rooms, and a worker at a call waits for a room: released ahead of the workers'
    // end
    while (EMBED_ATOMIC_LOAD(&cli->workers) != 0u)
    {
        EMBED_CALL(interface_items_free, GuiInterfaceCtx, .cli = cli);
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &cli->ended, .milliseconds_u64 = 1ull);
    }
    EMBED_CALL(interface_items_free, GuiInterfaceCtx, .cli = cli);
    for (unsigned int at = 0u; at < GUI_INTERFACE_WORKERS; at += 1u)
    {
        mmgr_ring *const calls = &cli->worker[at].calls_ring;
        size_t front = 0u;
        while (anularis.seg_front(&(AnularisCfg){.ring = calls, .out_index = &front}))
        {
            const GuiInterfaceCall *const call =
                (const GuiInterfaceCall *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = calls, .index = front});
            g_cli_cells->persistent_buf_release(call->name, &g_gui_interface_cells);
            anularis.seg_release(&(AnularisCfg){.ring = calls});
        }
    }
}

// the worker a call goes to: the own lane for the window's own; an idle pool worker where one is, its bit cleared here
// so that the next call goes to another; else the pool in turn. `*woken` 1 where the worker chosen was idle
EMBED_INLINE GuiInterfaceWorker *interface_worker_pick(const GuiInterfaceCtx *args)
{
    GuiInterface *const cli = args->cli;
    *args->woken = 0;
    if (args->own)
    {
        return &cli->worker[GUI_INTERFACE_POOL];
    }
    const embed_u64 idle = EMBED_ATOMIC_LOAD(&cli->idle) & (interface_bit(GUI_INTERFACE_POOL) - 1ull);
    if (idle != 0ull)
    {
        // a set bit of a mask of at most GUI_INTERFACE_POOL bits names a worker
        const unsigned int at = (unsigned int)anularis.loculus_next(&(AnularisCfg){.mask = (embed_word)idle});
        (void)EMBED_ATOMIC_AND_64(&cli->idle, ~interface_bit(at));
        *args->woken = 1;
        return &cli->worker[at];
    }
    const unsigned int at = cli->pool_next;
    cli->pool_next = (at + 1u) % GUI_INTERFACE_POOL;
    return &cli->worker[at];
}

unsigned long long gui_interface_call(const GuiInterfaceOwnCfg *args)
{
    GuiInterface *const cli = args->cli;
    if (EMBED_ATOMIC_LOAD(&cli->ending) != 0u)
    {
        return 0ull;
    }
    char *args_at = NULL;
    char *const cell = EMBED_CALL(interface_copy, GuiInterfaceCtx, .name = args->name, .text = args->request, .size = args->size,
                                  .text_at = &args_at);
    if (cell == NULL)
    {
        return 0ull;
    }
    int woken = 0;
    GuiInterfaceWorker *const worker =
        EMBED_CALL(interface_worker_pick, GuiInterfaceCtx, .cli = cli, .own = args->own, .woken = &woken);
    size_t at = 0u;
    if (!anularis.seg_next(&(AnularisCfg){.ring = &worker->calls_ring, .out_index = &at}))
    {
        g_cli_cells->persistent_buf_release(cell, &g_gui_interface_cells);
        return 0ull;
    }
    cli->tickets += 1ull;
    *(GuiInterfaceCall *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &worker->calls_ring, .index = at}) =
        (GuiInterfaceCall){cli->tickets, args->own, cell, args_at, args->size};
    anularis.seg_publish(&(AnularisCfg){.ring = &worker->calls_ring});
    EMBED_ATOMIC_ORDERED();
    if (woken || ((EMBED_ATOMIC_LOAD(&cli->idle) & interface_bit(worker->index)) != 0ull))
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &worker->wake);
    }
    return cli->tickets;
}

void gui_interface_emit(void *context, const char *event, const char *body, unsigned long long size)
{
    char *text_at = NULL;
    char *const cell =
        EMBED_CALL(interface_copy, GuiInterfaceCtx, .name = event, .text = body, .size = size, .text_at = &text_at);
    if (cell == NULL)
    {
        return;
    }
    const GuiInterfaceItem put = {0ull, 1, cell, text_at, size, CLI_WINDOW_ROOMS};
    EMBED_CALL(interface_item_put, GuiInterfaceCtx, .cli = (GuiInterface *)context, .put = &put);
}

unsigned int gui_interface_drain(const GuiInterfaceContextCfg *args)
{
    GuiInterface *const cli = args->cli;
    unsigned int count = 0u;
    for (embed_u64 rings = EMBED_ATOMIC_EXCHANGE_64(&cli->pending, 0ull); rings != 0ull;
         rings = EMBED_ATOMIC_EXCHANGE_64(&cli->pending, 0ull))
    {
        while (rings != 0ull)
        {
            // a set bit of a mask of at most GUI_INTERFACE_RINGS bits names a ring
            mmgr_ring *const ring =
                &cli->items[(unsigned int)anularis.loculus_next(&(AnularisCfg){.mask = (embed_word)rings})].ring;
            rings &= rings - 1ull;
            size_t at = 0u;
            while (anularis.seg_front(&(AnularisCfg){.ring = ring, .out_index = &at}))
            {
                const GuiInterfaceItem item =
                    *(const GuiInterfaceItem *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = ring, .index = at});
                anularis.seg_release(&(AnularisCfg){.ring = ring});
                EMBED_CALL(args->each, GuiInterfaceEachCfg, .context = args->context, .ticket = item.ticket,
                           .name = item.name, .ok = item.ok, .text = item.text, .size = item.size);
                EMBED_CALL(interface_item_free, GuiInterfaceCtx, .item = &item);
                count += 1u;
            }
        }
    }
    return count;
}
