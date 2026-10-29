// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_test.c: the link's check: a call queued is answered on a worker and its answer waits in the queue with its
// ticket, the window's thread woken; an event emitted waits there with no ticket; the queue drains in order and is
// then empty; the window's own calls are answered in the order queued; and the link ends with its workers
#include "../../../../../ui/gui/src/link.h"
#include "../cli/cli_check.h"

// what the window's thread is handed as it drains: the last item's ticket, name, outcome and text, how many came, and
// whether every answer's ticket so far came after the one before it
typedef struct
{
    unsigned long long ticket;
    char name[64];
    int ok;
    char text[256];
    unsigned int came;
    int ordered;
} Drained;

static void each(void *context, unsigned long long ticket, const char *name, int ok, const char *text,
                 unsigned long long size)
{
    Drained *const drained = (Drained *)context;
    drained->ordered = drained->ordered && ((ticket == 0ull) || (ticket > drained->ticket));
    drained->ticket = ticket;
    drained->ok = ok;
    drained->came += 1u;
    VerbaLinea named = {drained->name, sizeof(drained->name), 0ull};
    mmgr_line_put(&named, name);
    (void)mmgr_line_finish(&named);
    VerbaLinea texted = {drained->text, sizeof(drained->text), 0ull};
    mmgr_line_put_n(&texted, text, (size < 200ull) ? size : 200ull);
    (void)mmgr_line_finish(&texted);
}

// the wake the link gives the window's thread, counted
typedef struct
{
    CliLock lock;
    CliWake wake;
    unsigned int woken;
} Woken;

static void woken(void *context)
{
    Woken *const count = (Woken *)context;
    cli_lock_take(&count->lock);
    count->woken += 1u;
    cli_wake_all(&count->wake);
    cli_lock_give(&count->lock);
}

// the window's thread waiting up to two seconds for `wanted` wakes
static int waited(Woken *count, unsigned int wanted)
{
    cli_lock_take(&count->lock);
    unsigned int left = 200u;
    while ((count->woken < wanted) && (left > 0u))
    {
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        left -= 1u;
    }
    const int came = count->woken >= wanted;
    cli_lock_give(&count->lock);
    return came;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    Woken count = {CLI_LOCK_INIT, CLI_WAKE_INIT, 0u};
    GuiLink link;
    cli_check(gui_link_begun(&link, woken, &count), "the link begins with its workers");
    Drained drained = {0ull, "", 0, "", 0u, 1};
    cli_check(gui_link_drained(&link, each, &drained) == 0u, "a link begun has nothing to drain");

    const unsigned long long ticket = gui_link_called(&link, 1, "root", "{}", 2ull);
    cli_check(ticket != 0ull, "a call queued takes a ticket");
    cli_check(waited(&count, 1u), "the window's thread is woken as the call is answered");
    cli_check(gui_link_drained(&link, each, &drained) == 1u, "the answer drains as one item");
    cli_check(drained.ticket == ticket, "the answer carries the call's ticket");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = drained.name, .other = "root", .cap = 64ull}), "the answer carries the call's name");
    cli_check(drained.text[0] != '\0', "the answer carries a text, an answer or why it failed");

    gui_link_emitted(&link, "tree-changed", "{\"paths\":[]}", 12ull);
    cli_check(waited(&count, 2u), "the window's thread is woken as an event comes");
    cli_check(gui_link_drained(&link, each, &drained) == 1u, "the event drains as one item");
    cli_check(drained.ticket == 0ull, "an event carries no ticket");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = drained.name, .other = "tree-changed", .cap = 64ull}), "an event carries its name");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = drained.text, .other = "{\"paths\":[]}", .cap = 256ull}), "an event carries its body whole");

    const unsigned long long first = gui_link_called(&link, 1, "root", "{}", 2ull);
    const unsigned long long second = gui_link_called(&link, 1, "root_now", "{}", 2ull);
    cli_check(second == first + 1ull, "tickets count up");
    cli_check(waited(&count, 4u), "both calls are answered");
    drained.came = 0u;
    cli_check(gui_link_drained(&link, each, &drained) == 2u, "two answers drain together");
    cli_check(gui_link_drained(&link, each, &drained) == 0u, "the queue drained is empty");

    // the window's own calls, many at once, are answered in the order queued; the tree's are answered as the pool runs
    // them
    drained.ticket = 0ull;
    drained.ordered = 1;
    drained.came = 0u;
    int queued = 1;
    for (unsigned int at = 0u; at < 40u; at += 1u)
    {
        queued = queued && (gui_link_called(&link, 1, "root", "{}", 2ull) != 0ull);
    }
    cli_check(queued, "forty own calls queue");
    cli_check(waited(&count, 44u), "every own call is answered");
    cli_check(gui_link_drained(&link, each, &drained) == 40u, "forty answers drain");
    cli_check(drained.ordered, "the own calls' answers come in the order queued");

    gui_link_ended(&link);
    cli_check(link.workers == 0u, "the link ended has no workers");
    return cli_checked();
}
