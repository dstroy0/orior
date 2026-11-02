// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.c: the CLI in the window's process. Each queue is a ring; a queueing into a full ring of items waits for the
// window's thread to drain one, and a call into a full ring of calls is refused with no ticket. The pool's workers
// take the tree's calls from their ring as they come; the own lane's worker takes the window's own from theirs, so
// each runs after the one before it
#include "link.h"
#include "../../cli/src/command_line/callback_cfg.h"

#include "../../cli/src/command_line/cells.h"

typedef struct
{
    const char *name;
    const char *text;
    unsigned long long size;
    char **text_at;
    const GuiLinkItem *item;
    GuiLink *link;
    const GuiLinkItem *put;
    GuiLinkRing *ring;
} GuiLinkCtx;

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

// an item given back: its room, where it has one, and its name's cell
EMBED_INLINE void link_item_free(const GuiLinkCtx *args)
{
    if (args->item->room < CLI_WINDOW_ROOMS)
    {
        cli_window_given(args->item->room);
    }
    g_cli_cells->persistent_buf_release(args->item->name, &g_gui_link_cells);
}

// an item put at the ring's tail, under the lock, waiting for room where the ring is full, and the window woken; an
// item coming once the link ends, as the CLI's threads still emit, is given back at once
EMBED_INLINE void link_done_put(const GuiLinkCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
    while ((args->link->done_count == GUI_LINK_DONE) && !args->link->ending)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &args->link->room, .lock = &args->link->lock);
    }
    if (args->link->ending || (args->link->done == NULL))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
        EMBED_CALL(link_item_free, GuiLinkCtx, .item = args->put);
        return;
    }
    args->link->done[(args->link->done_head + args->link->done_count) % GUI_LINK_DONE] = *args->put;
    args->link->done_count += 1u;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
    args->link->woken(args->link->context);
}

// what a worker runs over `ring`: each call taken from the ring's head in turn, run through the CLI, its answer queued,
// until the link ends. The window's own lane runs what stands queued first, a save among it, its answers given back as
// no one reads them; a call of the tree's left queued is asked of no one
EMBED_INLINE void link_ring_work(const GuiLinkCtx *args)
{
    const int finishes = args->ring == &args->link->own;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
    for (;;)
    {
        while (!args->link->ending && (args->ring->count == 0u))
        {
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &args->link->wake, .lock = &args->link->lock);
        }
        if (args->link->ending && (!finishes || (args->ring->count == 0u)))
        {
            break;
        }
        const GuiLinkCall call = args->ring->call[args->ring->head];
        args->ring->head = (args->ring->head + 1u) % GUI_LINK_PENDING;
        args->ring->count -= 1u;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
        // the answer stays in the call's room, read where it lies by the window's thread, which gives the room back
        // with the call's cell once it has read it
        const unsigned int room = cli_window_called(call.own, call.name, call.args, call.size);
        const char *text = NULL;
        unsigned long long size = 0ull;
        const int ok = cli_window_answer(room, &text, &size);
        const GuiLinkItem put = {call.ticket, ok, call.name, (text != NULL) ? text : "", (text != NULL) ? size : 0ull,
                                 room};
        EMBED_CALL(link_done_put, GuiLinkCtx, .link = args->link, .put = &put);
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
    }
    args->link->workers -= 1u;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &args->link->wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
}

// what a worker of the pool runs, and what the own lane's runs
static void link_pool_work(void *context)
{
    GuiLink *const link = (GuiLink *)context;
    EMBED_CALL(link_ring_work, GuiLinkCtx, .link = link, .ring = &link->tree);
}

static void link_own_work(void *context)
{
    GuiLink *const link = (GuiLink *)context;
    EMBED_CALL(link_ring_work, GuiLinkCtx, .link = link, .ring = &link->own);
}

// every call waiting in `ring` given back
EMBED_INLINE void link_ring_free(const GuiLinkCtx *args)
{
    if (args->ring->call != NULL)
    {
        for (unsigned int at = 0u; at < args->ring->count; at += 1u)
        {
            g_cli_cells->persistent_buf_release(args->ring->call[(args->ring->head + at) % GUI_LINK_PENDING].name,
                                                &g_gui_link_cells);
        }
    }
    g_cli_cells->persistent_buf_release(args->ring->call, &g_gui_link_cells);
    args->ring->call = NULL;
    args->ring->count = 0u;
}

int gui_link_begin(const GuiLinkCfg *args)
{
    GuiLinkWoken woken = args->woken;
    void *context = args->context;
    memor.set(&(MemoriaCfg){.dst = args->link, .val = 0, .bytes = sizeof(*args->link)});
    args->link->lock = (CliLock)CLI_LOCK_INIT;
    args->link->wake = (CliWake)CLI_WAKE_INIT;
    args->link->room = (CliWake)CLI_WAKE_INIT;
    args->link->woken = woken;
    args->link->context = context;
    args->link->own.call =
        g_cli_cells->persistent_buf_alloc(GUI_LINK_PENDING * sizeof(*args->link->own.call), &g_gui_link_cells);
    args->link->tree.call =
        g_cli_cells->persistent_buf_alloc(GUI_LINK_PENDING * sizeof(*args->link->tree.call), &g_gui_link_cells);
    args->link->done = g_cli_cells->persistent_buf_alloc(GUI_LINK_DONE * sizeof(*args->link->done), &g_gui_link_cells);
    if ((args->link->own.call == NULL) || (args->link->tree.call == NULL) || (args->link->done == NULL))
    {
        cli_link.end(args);
        return 0;
    }
    for (unsigned int at = 0u; at < GUI_LINK_WORKERS; at += 1u)
    {
        args->link->thread[at].run = (at < GUI_LINK_POOL) ? link_pool_work : link_own_work;
        args->link->thread[at].context = args->link;
        args->link->thread[at].priority = CLI_PRIORITY_WORK;
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
        args->link->workers += 1u;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &args->link->thread[at]))
        {
            EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
            args->link->workers -= 1u;
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
            cli_link.end(args);
            return 0;
        }
    }
    return 1;
}

void gui_link_end(const GuiLinkCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
    args->link->ending = 1;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &args->link->wake);
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &args->link->room);
    // the answers waiting hold their rooms, and a worker at a call waits for a room: given back ahead of the workers'
    // end
    if (args->link->done != NULL)
    {
        for (unsigned int at = 0u; at < args->link->done_count; at += 1u)
        {
            EMBED_CALL(link_item_free, GuiLinkCtx,
                       .item = &args->link->done[(args->link->done_head + at) % GUI_LINK_DONE]);
        }
    }
    args->link->done_count = 0u;
    while (args->link->workers > 0u)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &args->link->wake, .lock = &args->link->lock);
    }
    EMBED_CALL(link_ring_free, GuiLinkCtx, .ring = &args->link->own);
    EMBED_CALL(link_ring_free, GuiLinkCtx, .ring = &args->link->tree);
    g_cli_cells->persistent_buf_release(args->link->done, &g_gui_link_cells);
    args->link->done = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
}

unsigned long long gui_link_call(const GuiLinkOwnCfg *args)
{
    int own = args->own;
    const char *name = args->name;
    const char *arguments = args->request;
    unsigned long long size = args->size;
    char *args_at = NULL;
    char *const cell =
        EMBED_CALL(link_copy, GuiLinkCtx, .name = name, .text = arguments, .size = size, .text_at = &args_at);
    if (cell == NULL)
    {
        return 0ull;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
    GuiLinkRing *const ring = own ? &args->link->own : &args->link->tree;
    if ((ring->count == GUI_LINK_PENDING) || args->link->ending)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
        g_cli_cells->persistent_buf_release(cell, &g_gui_link_cells);
        return 0ull;
    }
    args->link->tickets += 1ull;
    GuiLinkCall *const call = &ring->call[(ring->head + ring->count) % GUI_LINK_PENDING];
    call->ticket = args->link->tickets;
    call->own = own;
    call->name = cell;
    call->args = args_at;
    call->size = size;
    ring->count += 1u;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &args->link->wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
    return call->ticket;
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
    EMBED_CALL(link_done_put, GuiLinkCtx, .link = (GuiLink *)context, .put = &put);
}

unsigned int gui_link_drain(const GuiLinkContextCfg *args)
{
    unsigned int count = 0u;
    for (;;)
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->link->lock);
        if (args->link->done_count == 0u)
        {
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
            return count;
        }
        const GuiLinkItem item = args->link->done[args->link->done_head];
        args->link->done_head = (args->link->done_head + 1u) % GUI_LINK_DONE;
        args->link->done_count -= 1u;
        EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &args->link->room);
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->link->lock);
        EMBED_CALL(args->each, GuiLinkEachCfg, .context = args->context, .ticket = item.ticket, .name = item.name,
                   .ok = item.ok, .text = item.text, .size = item.size);
        EMBED_CALL(link_item_free, GuiLinkCtx, .item = &item);
        count += 1u;
    }
}
