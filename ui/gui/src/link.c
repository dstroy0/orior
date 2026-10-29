// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.c: the CLI in the window's process. Each queue is a ring; a queueing into a full ring of items waits for the
// window's thread to drain one, and a call into a full ring of calls is refused with no ticket. The pool's workers
// take the tree's calls from their ring as they come; the own lane's worker takes the window's own from theirs, so
// each runs after the one before it
#include "link.h"

#include "../../cli/src/command_line/cells.h"

static CellOwner s_owner = MMGR_CELL_OWNER("gui_link");

// `name` and `size` bytes of `text` copied into one cell, each ended by a NUL, `*text_at` the text's start: the cell,
// or NULL where it could not be taken
static char *link_copied(const char *name, const char *text, unsigned long long size, char **text_at)
{
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char *const cell = g_cli_cells->persistent_buf_alloc(name_length + 1ull + size + 1ull, &s_owner);
    if (cell == NULL)
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = name, .bytes = name_length});
    cell[name_length] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = cell + name_length + 1ull, .src = text, .bytes = size});
    cell[name_length + 1ull + size] = '\0';
    *text_at = cell + name_length + 1ull;
    return cell;
}

// an item put at the ring's tail, under the lock, waiting for room where the ring is full, and the window woken; an
// item coming once the link ends, as the CLI's threads still emit, is dropped
static void link_done_put(GuiLink *link, unsigned long long ticket, int ok, const char *name, const char *text,
                          unsigned long long size)
{
    char *text_at = NULL;
    char *const cell = link_copied(name, text, size, &text_at);
    if (cell == NULL)
    {
        return;
    }
    cli_lock_take(&link->lock);
    while ((link->done_count == GUI_LINK_DONE) && !link->ending)
    {
        cli_wake_wait(&link->room, &link->lock);
    }
    if (link->ending || (link->done == NULL))
    {
        cli_lock_give(&link->lock);
        g_cli_cells->persistent_buf_release(cell, &s_owner);
        return;
    }
    GuiLinkItem *const item = &link->done[(link->done_head + link->done_count) % GUI_LINK_DONE];
    item->ticket = ticket;
    item->ok = ok;
    item->name = cell;
    item->text = text_at;
    item->size = size;
    link->done_count += 1u;
    cli_lock_give(&link->lock);
    link->woken(link->context);
}

// what a worker runs over `ring`: each call taken from the ring's head in turn, run through the CLI, its answer queued,
// until the link ends
static void link_ring_worked(GuiLink *link, GuiLinkRing *ring)
{
    cli_lock_take(&link->lock);
    for (;;)
    {
        while (!link->ending && (ring->count == 0u))
        {
            cli_wake_wait(&link->wake, &link->lock);
        }
        if (link->ending)
        {
            break;
        }
        const GuiLinkCall call = ring->call[ring->head];
        ring->head = (ring->head + 1u) % GUI_LINK_PENDING;
        ring->count -= 1u;
        cli_lock_give(&link->lock);
        const unsigned int room = cli_window_called(call.own, call.name, call.args, call.size);
        const char *text = NULL;
        unsigned long long size = 0ull;
        const int ok = cli_window_answer(room, &text, &size);
        link_done_put(link, call.ticket, ok, call.name, (text != NULL) ? text : "", (text != NULL) ? size : 0ull);
        cli_window_given(room);
        g_cli_cells->persistent_buf_release(call.name, &s_owner);
        cli_lock_take(&link->lock);
    }
    link->workers -= 1u;
    cli_wake_all(&link->wake);
    cli_lock_give(&link->lock);
}

// what a worker of the pool runs, and what the own lane's runs
static void link_pool_worked(void *context)
{
    GuiLink *const link = (GuiLink *)context;
    link_ring_worked(link, &link->tree);
}

static void link_own_worked(void *context)
{
    GuiLink *const link = (GuiLink *)context;
    link_ring_worked(link, &link->own);
}

// every call waiting in `ring` given back
static void link_ring_given(GuiLinkRing *ring)
{
    if (ring->call != NULL)
    {
        for (unsigned int at = 0u; at < ring->count; at += 1u)
        {
            g_cli_cells->persistent_buf_release(ring->call[(ring->head + at) % GUI_LINK_PENDING].name, &s_owner);
        }
    }
    g_cli_cells->persistent_buf_release(ring->call, &s_owner);
    ring->call = NULL;
    ring->count = 0u;
}

int gui_link_begun(GuiLink *link, GuiLinkWoken woken, void *context)
{
    memor.set(&(MemoriaCfg){.dst = link, .val = 0, .bytes = sizeof(*link)});
    link->lock = (CliLock)CLI_LOCK_INIT;
    link->wake = (CliWake)CLI_WAKE_INIT;
    link->room = (CliWake)CLI_WAKE_INIT;
    link->woken = woken;
    link->context = context;
    link->own.call = g_cli_cells->persistent_buf_alloc(GUI_LINK_PENDING * sizeof(*link->own.call), &s_owner);
    link->tree.call = g_cli_cells->persistent_buf_alloc(GUI_LINK_PENDING * sizeof(*link->tree.call), &s_owner);
    link->done = g_cli_cells->persistent_buf_alloc(GUI_LINK_DONE * sizeof(*link->done), &s_owner);
    if ((link->own.call == NULL) || (link->tree.call == NULL) || (link->done == NULL))
    {
        gui_link_ended(link);
        return 0;
    }
    for (unsigned int at = 0u; at < GUI_LINK_WORKERS; at += 1u)
    {
        link->thread[at].run = (at < GUI_LINK_POOL) ? link_pool_worked : link_own_worked;
        link->thread[at].context = link;
        link->thread[at].priority = CLI_PRIORITY_WORK;
        cli_lock_take(&link->lock);
        link->workers += 1u;
        cli_lock_give(&link->lock);
        if (!cli_thread_started(&link->thread[at]))
        {
            cli_lock_take(&link->lock);
            link->workers -= 1u;
            cli_lock_give(&link->lock);
            gui_link_ended(link);
            return 0;
        }
    }
    return 1;
}

void gui_link_ended(GuiLink *link)
{
    cli_lock_take(&link->lock);
    link->ending = 1;
    cli_wake_all(&link->wake);
    cli_wake_all(&link->room);
    while (link->workers > 0u)
    {
        cli_wake_wait(&link->wake, &link->lock);
    }
    link_ring_given(&link->own);
    link_ring_given(&link->tree);
    if (link->done != NULL)
    {
        for (unsigned int at = 0u; at < link->done_count; at += 1u)
        {
            g_cli_cells->persistent_buf_release(link->done[(link->done_head + at) % GUI_LINK_DONE].name, &s_owner);
        }
    }
    g_cli_cells->persistent_buf_release(link->done, &s_owner);
    link->done = NULL;
    link->done_count = 0u;
    cli_lock_give(&link->lock);
}

unsigned long long gui_link_called(GuiLink *link, int own, const char *name, const char *args,
                                   unsigned long long size)
{
    char *args_at = NULL;
    char *const cell = link_copied(name, args, size, &args_at);
    if (cell == NULL)
    {
        return 0ull;
    }
    cli_lock_take(&link->lock);
    GuiLinkRing *const ring = own ? &link->own : &link->tree;
    if ((ring->count == GUI_LINK_PENDING) || link->ending)
    {
        cli_lock_give(&link->lock);
        g_cli_cells->persistent_buf_release(cell, &s_owner);
        return 0ull;
    }
    link->tickets += 1ull;
    GuiLinkCall *const call = &ring->call[(ring->head + ring->count) % GUI_LINK_PENDING];
    call->ticket = link->tickets;
    call->own = own;
    call->name = cell;
    call->args = args_at;
    call->size = size;
    ring->count += 1u;
    cli_wake_all(&link->wake);
    cli_lock_give(&link->lock);
    return call->ticket;
}

void gui_link_emitted(void *context, const char *event, const char *body, unsigned long long size)
{
    link_done_put((GuiLink *)context, 0ull, 1, event, body, size);
}

unsigned int gui_link_drained(GuiLink *link, GuiLinkEach each, void *context)
{
    unsigned int count = 0u;
    for (;;)
    {
        cli_lock_take(&link->lock);
        if (link->done_count == 0u)
        {
            cli_lock_give(&link->lock);
            return count;
        }
        const GuiLinkItem item = link->done[link->done_head];
        link->done_head = (link->done_head + 1u) % GUI_LINK_DONE;
        link->done_count -= 1u;
        cli_wake_all(&link->room);
        cli_lock_give(&link->lock);
        each(context, item.ticket, item.name, item.ok, item.text, item.size);
        g_cli_cells->persistent_buf_release(item.name, &s_owner);
        count += 1u;
    }
}
