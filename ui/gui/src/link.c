// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.c: the CLI in the window's process. A worker that finds its ring empty sets its idle bit and tests the ring
// again before it sleeps; the window's thread writes the call and then reads that bit, each with a full fence between,
// so whichever runs second sees the other's write and no wake is lost. A worker that is running is never woken.
#include "link.h"
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
    const GuiLinkItem *item;
    GuiLink *link;
    const GuiLinkItem *put;
    unsigned int ring;
    int own;
    int *woken;
} GuiLinkCtx;

// the mask bit of ring or worker `index`
EMBED_INLINE embed_u64 link_bit(unsigned int index)
{
    return (embed_u64)1u << index;
}

// `name` and `size` bytes of `text` copied into one cell, each ended by a NUL, `*text_at` the text's start: the cell,
// or NULL where it could not be taken
EMBED_INLINE char *link_copy(const GuiLinkCtx *args)
{
    char **text_at = args->text_at;
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    char *const cell = g_cli_cells->persistent_buf_alloc(name_length + 1ull + args->size + 1ull, &g_gui_link_cells);
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
EMBED_INLINE void link_item_free(const GuiLinkCtx *args)
{
    if (args->item->room < CLI_WINDOW_ROOMS)
    {
        cli_window_given(args->item->room);
    }
    g_cli_cells->persistent_buf_release(args->item->name, &g_gui_link_cells);
}

// `put` written to item ring `ring`, which this thread alone writes as it stands here, sleeping a millisecond at a time
// while the ring is full: 1, or 0 where the link ended first
EMBED_INLINE int link_items_write(const GuiLinkCtx *args)
{
    mmgr_ring *const ring = &args->link->items[args->ring].ring;
    size_t at = 0u;
    while (!anularis.seg_next(&(AnularisCfg){.ring = ring, .out_index = &at}))
    {
        if (EMBED_ATOMIC_LOAD(&args->link->ending) != 0u)
        {
            return 0;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->link->full,
                         .milliseconds_u64 = 1ull);
    }
    *(GuiLinkItem *)(void *)anularis.seg_at(&(AnularisCfg){.ring = ring, .index = at}) = *args->put;
    anularis.seg_publish(&(AnularisCfg){.ring = ring});
    return 1;
}

// `put` written and the window's thread woken: to this worker's own ring, or to an emitters' ring this thread claims
// for the one write, passing over any another thread stands in. An item coming once the link ends, as the CLI's threads
// still emit, is released at once
EMBED_INLINE void link_item_put(const GuiLinkCtx *args)
{
    GuiLink *const link = args->link;
    if (EMBED_ATOMIC_LOAD(&link->ending) != 0u)
    {
        EMBED_CALL(link_item_free, GuiLinkCtx, .item = args->put);
        return;
    }
    int written = 0;
    unsigned int ring = 0u;
    if (t_own_items != 0u)
    {
        ring = t_own_items - 1u;
        written = EMBED_CALL(link_items_write, GuiLinkCtx, .link = link, .ring = ring, .put = args->put);
    }
    else
    {
        if (t_emitter == 0u)
        {
            t_emitter = (EMBED_ATOMIC_ADD_32(&link->emitter_next, 1u) % GUI_LINK_EMITTERS) + 1u;
        }
        unsigned int at = t_emitter - 1u;
        while ((EMBED_ATOMIC_OR_64(&link->emitters_taken, link_bit(at)) & link_bit(at)) != 0ull)
        {
            at = (at + 1u) % GUI_LINK_EMITTERS;
        }
        t_emitter = at + 1u;
        ring = GUI_LINK_WORKERS + at;
        written = EMBED_CALL(link_items_write, GuiLinkCtx, .link = link, .ring = ring, .put = args->put);
        (void)EMBED_ATOMIC_AND_64(&link->emitters_taken, ~link_bit(at));
    }
    if (!written)
    {
        EMBED_CALL(link_item_free, GuiLinkCtx, .item = args->put);
        return;
    }
    (void)EMBED_ATOMIC_OR_64(&link->pending, link_bit(ring));
    link->woken(link->context);
}

// what a worker runs: each call taken from its ring in turn, run through the CLI, its answer written, until the link
// ends. The own lane runs what stands queued first, a save among it, its answers released as no one reads them; a pool
// worker's queued calls are asked of no one
static void link_work(void *context)
{
    GuiLinkWorker *const worker = (GuiLinkWorker *)context;
    GuiLink *const link = worker->link;
    const embed_u64 bit = link_bit(worker->index);
    const int finishes = worker->index == GUI_LINK_POOL;
    int marked = 0;
    t_own_items = worker->index + 1u;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &worker->wake);
        const int ending = EMBED_ATOMIC_LOAD(&link->ending) != 0u;
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
                (void)EMBED_ATOMIC_OR_64(&link->idle, bit);
                EMBED_ATOMIC_ORDERED();
                marked = 1;
                continue;
            }
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &worker->wake, .seen = seen);
            marked = 0;
            continue;
        }
        (void)EMBED_ATOMIC_AND_64(&link->idle, ~bit);
        marked = 0;
        const GuiLinkCall call = *(const GuiLinkCall *)(const void *)anularis.seg_at(
            &(AnularisCfg){.ring = &worker->calls_ring, .index = at});
        anularis.seg_release(&(AnularisCfg){.ring = &worker->calls_ring});
        // the answer stays in the call's room, read where it lies by the window's thread, which releases the room with
        // the call's cell once it has read it
        const unsigned int room = cli_window_called(call.own, call.name, call.args, call.size);
        const char *text = NULL;
        unsigned long long size = 0ull;
        const int ok = cli_window_answer(room, &text, &size);
        const GuiLinkItem put = {call.ticket, ok, call.name, (text != NULL) ? text : "", (text != NULL) ? size : 0ull,
                                 room};
        EMBED_CALL(link_item_put, GuiLinkCtx, .link = link, .put = &put);
    }
    (void)EMBED_ATOMIC_ADD_32(&link->workers, (embed_u32)-1);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &link->ended);
}

// every item waiting in every ring released, from the window's thread as the link ends
EMBED_INLINE void link_items_free(const GuiLinkCtx *args)
{
    for (unsigned int ring = 0u; ring < GUI_LINK_RINGS; ring += 1u)
    {
        mmgr_ring *const items = &args->link->items[ring].ring;
        size_t at = 0u;
        while (anularis.seg_front(&(AnularisCfg){.ring = items, .out_index = &at}))
        {
            const GuiLinkItem item =
                *(const GuiLinkItem *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = items, .index = at});
            anularis.seg_release(&(AnularisCfg){.ring = items});
            EMBED_CALL(link_item_free, GuiLinkCtx, .item = &item);
        }
    }
}

int gui_link_begin(const GuiLinkCfg *args)
{
    GuiLink *const link = args->link;
    memor.set(&(MemoriaCfg){.dst = link, .val = 0, .bytes = sizeof(*link)});
    link->woken = args->woken;
    link->context = args->context;
    for (unsigned int at = 0u; at < GUI_LINK_RINGS; at += 1u)
    {
        (void)anularis.init(&(AnularisCfg){.ring = &link->items[at].ring,
                                           .buf = (uint8_t *)(void *)link->items[at].items,
                                           .capacity = sizeof(link->items[at].items),
                                           .segment_count = GUI_LINK_ITEMS,
                                           .segment_bytes = sizeof(GuiLinkItem)});
    }
    for (unsigned int at = 0u; at < GUI_LINK_WORKERS; at += 1u)
    {
        GuiLinkWorker *const worker = &link->worker[at];
        (void)anularis.init(&(AnularisCfg){.ring = &worker->calls_ring,
                                           .buf = (uint8_t *)(void *)worker->calls,
                                           .capacity = sizeof(worker->calls),
                                           .segment_count = GUI_LINK_CALLS,
                                           .segment_bytes = sizeof(GuiLinkCall)});
        worker->index = at;
        worker->link = link;
        worker->thread.run = link_work;
        worker->thread.context = worker;
        worker->thread.priority = CLI_PRIORITY_WORK;
    }
    for (unsigned int at = 0u; at < GUI_LINK_WORKERS; at += 1u)
    {
        (void)EMBED_ATOMIC_ADD_32(&link->workers, 1u);
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &link->worker[at].thread))
        {
            (void)EMBED_ATOMIC_ADD_32(&link->workers, (embed_u32)-1);
            cli_link.end(args);
            return 0;
        }
    }
    return 1;
}

void gui_link_end(const GuiLinkCfg *args)
{
    GuiLink *const link = args->link;
    EMBED_ATOMIC_STORE(&link->ending, 1u);
    EMBED_ATOMIC_ORDERED();
    for (unsigned int at = 0u; at < GUI_LINK_WORKERS; at += 1u)
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &link->worker[at].wake);
    }
    // the answers waiting stand in their rooms, and a worker at a call waits for a room: released ahead of the workers'
    // end
    while (EMBED_ATOMIC_LOAD(&link->workers) != 0u)
    {
        EMBED_CALL(link_items_free, GuiLinkCtx, .link = link);
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &link->ended, .milliseconds_u64 = 1ull);
    }
    EMBED_CALL(link_items_free, GuiLinkCtx, .link = link);
    for (unsigned int at = 0u; at < GUI_LINK_WORKERS; at += 1u)
    {
        mmgr_ring *const calls = &link->worker[at].calls_ring;
        size_t front = 0u;
        while (anularis.seg_front(&(AnularisCfg){.ring = calls, .out_index = &front}))
        {
            const GuiLinkCall *const call =
                (const GuiLinkCall *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = calls, .index = front});
            g_cli_cells->persistent_buf_release(call->name, &g_gui_link_cells);
            anularis.seg_release(&(AnularisCfg){.ring = calls});
        }
    }
}

// the worker a call goes to: the own lane for the window's own; an idle pool worker where one is, its bit cleared here
// so that the next call goes to another; else the pool in turn. `*woken` 1 where the worker chosen was idle
EMBED_INLINE GuiLinkWorker *link_worker_pick(const GuiLinkCtx *args)
{
    GuiLink *const link = args->link;
    *args->woken = 0;
    if (args->own)
    {
        return &link->worker[GUI_LINK_POOL];
    }
    const embed_u64 idle = EMBED_ATOMIC_LOAD(&link->idle) & (link_bit(GUI_LINK_POOL) - 1ull);
    if (idle != 0ull)
    {
        // a set bit of a mask of at most GUI_LINK_POOL bits names a worker
        const unsigned int at = (unsigned int)anularis.loculus_next(&(AnularisCfg){.mask = (embed_word)idle});
        (void)EMBED_ATOMIC_AND_64(&link->idle, ~link_bit(at));
        *args->woken = 1;
        return &link->worker[at];
    }
    const unsigned int at = link->pool_next;
    link->pool_next = (at + 1u) % GUI_LINK_POOL;
    return &link->worker[at];
}

unsigned long long gui_link_call(const GuiLinkOwnCfg *args)
{
    GuiLink *const link = args->link;
    if (EMBED_ATOMIC_LOAD(&link->ending) != 0u)
    {
        return 0ull;
    }
    char *args_at = NULL;
    char *const cell = EMBED_CALL(link_copy, GuiLinkCtx, .name = args->name, .text = args->request, .size = args->size,
                                  .text_at = &args_at);
    if (cell == NULL)
    {
        return 0ull;
    }
    int woken = 0;
    GuiLinkWorker *const worker =
        EMBED_CALL(link_worker_pick, GuiLinkCtx, .link = link, .own = args->own, .woken = &woken);
    size_t at = 0u;
    if (!anularis.seg_next(&(AnularisCfg){.ring = &worker->calls_ring, .out_index = &at}))
    {
        g_cli_cells->persistent_buf_release(cell, &g_gui_link_cells);
        return 0ull;
    }
    link->tickets += 1ull;
    *(GuiLinkCall *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &worker->calls_ring, .index = at}) =
        (GuiLinkCall){link->tickets, args->own, cell, args_at, args->size};
    anularis.seg_publish(&(AnularisCfg){.ring = &worker->calls_ring});
    EMBED_ATOMIC_ORDERED();
    if (woken || ((EMBED_ATOMIC_LOAD(&link->idle) & link_bit(worker->index)) != 0ull))
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &worker->wake);
    }
    return link->tickets;
}

void gui_link_emit(void *context, const char *event, const char *body, unsigned long long size)
{
    char *text_at = NULL;
    char *const cell =
        EMBED_CALL(link_copy, GuiLinkCtx, .name = event, .text = body, .size = size, .text_at = &text_at);
    if (cell == NULL)
    {
        return;
    }
    const GuiLinkItem put = {0ull, 1, cell, text_at, size, CLI_WINDOW_ROOMS};
    EMBED_CALL(link_item_put, GuiLinkCtx, .link = (GuiLink *)context, .put = &put);
}

unsigned int gui_link_drain(const GuiLinkContextCfg *args)
{
    GuiLink *const link = args->link;
    unsigned int count = 0u;
    for (embed_u64 rings = EMBED_ATOMIC_EXCHANGE_64(&link->pending, 0ull); rings != 0ull;
         rings = EMBED_ATOMIC_EXCHANGE_64(&link->pending, 0ull))
    {
        while (rings != 0ull)
        {
            // a set bit of a mask of at most GUI_LINK_RINGS bits names a ring
            mmgr_ring *const ring =
                &link->items[(unsigned int)anularis.loculus_next(&(AnularisCfg){.mask = (embed_word)rings})].ring;
            rings &= rings - 1ull;
            size_t at = 0u;
            while (anularis.seg_front(&(AnularisCfg){.ring = ring, .out_index = &at}))
            {
                const GuiLinkItem item =
                    *(const GuiLinkItem *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = ring, .index = at});
                anularis.seg_release(&(AnularisCfg){.ring = ring});
                EMBED_CALL(args->each, GuiLinkEachCfg, .context = args->context, .ticket = item.ticket,
                           .name = item.name, .ok = item.ok, .text = item.text, .size = item.size);
                EMBED_CALL(link_item_free, GuiLinkCtx, .item = &item);
                count += 1u;
            }
        }
    }
    return count;
}
