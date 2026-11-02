// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_test.c: the link's check: a call queued is answered on a worker and its answer waits in the queue with its
// ticket in the call's room, the window's thread woken; an event emitted waits there with no ticket; the queue drains
// in order and is then empty; the window's own calls are answered in the order queued; and the link ends with its
// workers, the window's own calls still queued run first
#include "../../../../../ui/cli/src/machine/state.h"
#include "../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../../ui/cli/src/text/text.h"
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

static void each(const GuiLinkEachCfg *args)
{
    unsigned long long ticket = args->ticket;
    int ok = args->ok;
    Drained *const drained = (Drained *)args->context;
    drained->ordered = drained->ordered && ((ticket == 0ull) || (ticket > drained->ticket));
    drained->ticket = ticket;
    drained->ok = ok;
    drained->came += 1u;
    VerbaLinea named = {drained->name, sizeof(drained->name), 0ull};
    mmgr_line_put(&named, args->name);
    (void)mmgr_line_finish(&named);
    VerbaLinea texted = {drained->text, sizeof(drained->text), 0ull};
    mmgr_line_put_n(&texted, args->text, (args->size < 200ull) ? args->size : 200ull);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &count->lock);
    count->woken += 1u;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &count->wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &count->lock);
}

// the window's thread waiting up to two seconds for `wanted` wakes
static int waited(Woken *count, unsigned int wanted)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &count->lock);
    unsigned int left = 200u;
    while ((count->woken < wanted) && (left > 0u))
    {
        (void)EMBED_CALL(cli_lock_internal.wake_wait_for, CliLockCtx, .wake = &count->wake, .lock = &count->lock, .milliseconds_u64 = 10ull);
        left -= 1u;
    }
    const int came = count->woken >= wanted;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &count->lock);
    return came;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    Woken count = {CLI_LOCK_INIT, CLI_WAKE_INIT, 0u};
    GuiLink link;
    cli_check(EMBED_CALL(cli_link.begin, GuiLinkCfg, .link = &link, .woken = woken, .context = &count), "the link begins with its workers");
    Drained drained = {0ull, "", 0, "", 0u, 1};
    cli_check(EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &link, .each = each, .context = &drained) == 0u, "a link begun has nothing to drain");

    const unsigned long long ticket = EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &link, .own = 1, .name = "root", .request = "{}", .size = 2ull);
    cli_check(ticket != 0ull, "a call queued takes a ticket");
    cli_check(waited(&count, 1u), "the window's thread is woken as the call is answered");
    cli_check(EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &link, .each = each, .context = &drained) == 1u, "the answer drains as one item");
    cli_check(drained.ticket == ticket, "the answer carries the call's ticket");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = drained.name, .other = "root", .cap = 64ull}),
              "the answer carries the call's name");
    cli_check(drained.text[0] != '\0', "the answer carries a text, an answer or why it failed");

    gui_link_emit(&link, "tree-changed", "{\"paths\":[]}", 12ull);
    cli_check(waited(&count, 2u), "the window's thread is woken as an event comes");
    cli_check(EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &link, .each = each, .context = &drained) == 1u, "the event drains as one item");
    cli_check(drained.ticket == 0ull, "an event carries no ticket");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = drained.name, .other = "tree-changed", .cap = 64ull}),
              "an event carries its name");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = drained.text, .other = "{\"paths\":[]}", .cap = 256ull}),
              "an event carries its body whole");

    const unsigned long long first = EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &link, .own = 1, .name = "root", .request = "{}", .size = 2ull);
    const unsigned long long second = EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &link, .own = 1, .name = "root_now", .request = "{}", .size = 2ull);
    cli_check(second == first + 1ull, "tickets count up");
    cli_check(waited(&count, 4u), "both calls are answered");
    drained.came = 0u;
    cli_check(EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &link, .each = each, .context = &drained) == 2u, "two answers drain together");
    cli_check(EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &link, .each = each, .context = &drained) == 0u, "the queue drained is empty");

    // the window's own calls, many at once, are answered in the order queued; the tree's are answered as the pool runs
    // them
    drained.ticket = 0ull;
    drained.ordered = 1;
    drained.came = 0u;
    int queued = 1;
    for (unsigned int at = 0u; at < 40u; at += 1u)
    {
        queued = queued && (EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &link, .own = 1, .name = "root", .request = "{}", .size = 2ull) != 0ull);
    }
    cli_check(queued, "forty own calls queue");
    // an answer stays in its room until it is drained: at most as many answers wait as there are rooms, and the
    // window's thread drains as they come
    unsigned int answered = 0u;
    for (unsigned int rounds = 0u; (answered < 40u) && (rounds < 400u); rounds += 1u)
    {
        (void)waited(&count, 4u + answered + 1u);
        answered += EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &link, .each = each, .context = &drained);
    }
    cli_check(answered == 40u, "every own call is answered as the window's thread drains");
    cli_check(drained.came == 40u, "forty answers drain");
    cli_check(drained.ordered, "the own calls' answers come in the order queued");

    // every room taken by an answer no one drains and a worker at the next call waiting for a room: the link still ends,
    // and a run where it does not is ended as stalled
    queued = 1;
    for (unsigned int at = 0u; at < 40u; at += 1u)
    {
        queued = queued && (EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &link, .own = 1, .name = "root", .request = "{}", .size = 2ull) != 0ull);
    }
    cli_check(queued, "forty more own calls queue");
    cli_check(waited(&count, 44u + CLI_WINDOW_ROOMS), "the answers fill every room");
    // a save queued behind them, the window closing with it unanswered, still reaches orior's folder
    static const char s_saved[] = "{\"changes\":{\"orior.link.last\":\"saved\"}}";
    cli_check(EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &link, .own = 1, .name = "state_write", .request = s_saved, .size = sizeof(s_saved) - 1ull) != 0ull, "a save queues behind them");

    EMBED_CALL(cli_link.end, GuiLinkCfg, .link = &link);
    cli_check(link.workers == 0u, "the link ended has no workers");
    char folder[CLI_PATH_MAX_LEN];
    static char state_text[1u << 16u];
    VerbaLinea state = {state_text, sizeof(state_text), 0ull};
    unsigned long long found_at = 0ull;
    cli_check(EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder) && EMBED_CALL(cli_state_out.read, CliStateOutCfg, .folder = folder, .out = &state) && mmgr_line_ok(&state) && EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = state.out, .length_value = state.at, .wanted = "link.last", .size = 9ull, .at_pointer = &found_at),
              "the window's own calls queued as the link ends are run: a save is not lost");
    return cli_on_check();
}
