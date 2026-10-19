// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_keep.c: the link made: the server started on the machine and waited on to say it serves, the link kept on a
// thread of its own and made again each time it drops until it is ended, the calls on the tree's side written to the
// server and waited on, and the server ended
#include "../command_line/cells.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "link_internal.h"

// the machine, as the window names it, into `out`
static void keep_machine(CliLine *out)
{
    cli_link_machine(&g_cli_link.address, out);
}

// the server's process ended and let go, on the keeper's thread: ended under both locks, so that no call writes to
// it and nothing ends it again, then waited on with its errors' reader, and let go under both locks
static void keep_stopped(void)
{
    cli_lock_take(&g_cli_link_writing);
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.joined = 0;
    g_cli_link.stopping = 1;
    cli_spawn_input_closed(&g_cli_link.spawned);
    cli_spawn_killed(&g_cli_link.spawned);
    cli_lock_give(&g_cli_link_lock);
    cli_lock_give(&g_cli_link_writing);
    (void)cli_spawn_wait(&g_cli_link.spawned);
    cli_lock_take(&g_cli_link_lock);
    while (g_cli_link.reading_errors)
    {
        cli_wake_wait(&g_cli_link_changed, &g_cli_link_lock);
    }
    cli_lock_give(&g_cli_link_lock);
    cli_lock_take(&g_cli_link_writing);
    cli_lock_take(&g_cli_link_lock);
    cli_spawn_closed(&g_cli_link.spawned);
    g_cli_link.running = 0;
    g_cli_link.stopping = 0;
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
    cli_lock_give(&g_cli_link_writing);
}

// the link dropped: its server let go of, and every call waiting answered with the drop and why, `size` bytes of it
static void keep_dropped(const char *why, unsigned long long size)
{
    keep_stopped();
    cli_lock_take(&g_cli_link_lock);
    for (unsigned int at = 0u; at < CLI_LINK_CALLS; at += 1u)
    {
        CliLinkCall *const call = &g_cli_link.calls[at];
        if (call->used && !call->done)
        {
            call->done = 2;
            cli_line_text(call->said, "the link to ");
            keep_machine(call->said);
            cli_line_text(call->said, " dropped");
            if (size > 0u)
            {
                cli_line_text(call->said, ": ");
                cli_line_counted(call->said, why, size);
            }
        }
    }
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
}

// whether the line the server wrote first says it serves
static int keep_serving(unsigned long long size)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char event[16];
    CliLine none = {NULL, 0ull, 0ull};
    return cli_json_compacted(g_cli_link_line, size, &none) &&
           cli_lsp_member(g_cli_link_line, size, "event", &start, &length) &&
           cli_lsp_string(g_cli_link_line + start, length, event, sizeof(event)) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "serving", .cap = CLI_CELLS_POOL}));
}

// the server started on the machine and waited on to say it serves: 1, or 0 with why said
static int keep_join(CliLine *why)
{
    if (!g_cli_link.has_start)
    {
        CliLine start = {g_cli_link.start, sizeof(g_cli_link.start), 0ull};
        if (!cli_link_prepare(&g_cli_link.address, cli_link_stated, NULL, &start, why))
        {
            return 0;
        }
        if (cli_line_closed(&start) == 0ull)
        {
            cli_line_text(why, "the line that starts orior's server is longer than orior runs");
            return 0;
        }
        g_cli_link.has_start = 1;
    }
    static CliWords s_words;
    cli_link_runs(&g_cli_link.address, g_cli_link.start, 0, &s_words);
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.said_size = 0ull;
    cli_lock_give(&g_cli_link_lock);
    if (s_words.full || !cli_spawn_fed(&g_cli_link.spawned, s_words.word, NULL, NULL, 0u, 1, why))
    {
        cli_line_text(why, s_words.full ? "the line that starts orior's server is longer than orior runs" : "");
        return 0;
    }
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.running = 1;
    g_cli_link.reading_errors = 1;
    cli_lock_give(&g_cli_link_lock);
    g_cli_link.errors = (CliThread){cli_link_errors_read, NULL};
    if (!cli_thread_started(&g_cli_link.errors))
    {
        cli_lock_take(&g_cli_link_lock);
        g_cli_link.reading_errors = 0;
        cli_lock_give(&g_cli_link_lock);
    }
    g_cli_link.piece_at = 0ull;
    g_cli_link.piece_end = 0ull;
    unsigned long long size = 0ull;
    int whole = 0;
    if (!cli_link_line(&size, &whole) || !whole || !keep_serving(size))
    {
        keep_stopped();
        // a server that will not start is sent again on the next try
        g_cli_link.has_start = 0;
        const unsigned long long from = why->at;
        cli_link_said(why);
        if (why->at == from)
        {
            cli_line_text(why, "orior's server on ");
            keep_machine(why);
            cli_line_text(why, " did not start");
        }
        return 0;
    }
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.joined = 1;
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
    return 1;
}

// the link made, and made again each time it drops, until it is ended
static void keep_joined(void *context)
{
    (void)context;
    static char s_why[CLI_SAID_LONGEST + CLI_LINK_SAID];
    unsigned long long wait = 1000ull;
    int once = 0;
    // the server's line a cell for as long as the keeper runs; with no room for it the link fails at once
    g_cli_link_line = cli_cell_taken(CLI_LINK_LINE, "the line the link's server wrote");
    if (g_cli_link_line == NULL)
    {
        static const char s_untaken[] = "the line the link's server writes could not be taken";
        cli_link_tell("failed", s_untaken, sizeof(s_untaken) - 1u);
    }
    while (g_cli_link_line != NULL)
    {
        cli_lock_take(&g_cli_link_lock);
        const int ended = g_cli_link.ended;
        cli_lock_give(&g_cli_link_lock);
        if (ended)
        {
            break;
        }
        cli_link_tell(once ? "joining-again" : "joining", "", 0ull);
        CliLine why = {s_why, sizeof(s_why), 0ull};
        if (keep_join(&why))
        {
            once = 1;
            wait = 1000ull;
            cli_link_tell("joined", "", 0ull);
            unsigned long long size = 0ull;
            int whole = 0;
            while (cli_link_line(&size, &whole))
            {
                cli_link_heard(size, whole);
            }
            cli_link_said(&why);
            keep_dropped(s_why, why.at);
            cli_lock_take(&g_cli_link_lock);
            const int over = g_cli_link.ended;
            cli_lock_give(&g_cli_link_lock);
            if (over)
            {
                break;
            }
            cli_link_tell("dropped", s_why, why.at);
            continue;
        }
        cli_link_tell("failed", s_why, why.at);
        cli_lock_take(&g_cli_link_lock);
        if (!g_cli_link.ended)
        {
            (void)cli_wake_wait_for(&g_cli_link_changed, &g_cli_link_lock, wait);
        }
        cli_lock_give(&g_cli_link_lock);
        wait = (wait * 2u < CLI_LINK_JOIN_MOST) ? (wait * 2u) : CLI_LINK_JOIN_MOST;
    }
    cli_cell_given(g_cli_link_line);
    g_cli_link_line = NULL;
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.open = 0;
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
}

int cli_link_open(const CliLinkAddress *address, CliLinkTold told, void *context, CliLine *said)
{
    cli_lock_take(&g_cli_link_lock);
    if (g_cli_link.open)
    {
        cli_lock_give(&g_cli_link_lock);
        cli_line_text(said, "a link is open already");
        return 0;
    }
    g_cli_link.address = *address;
    g_cli_link.told = told;
    g_cli_link.context = context;
    g_cli_link.open = 1;
    g_cli_link.ended = 0;
    g_cli_link.joined = 0;
    g_cli_link.has_start = 0;
    g_cli_link.standing_size = 0ull;
    g_cli_link.next = 1ull;
    memor.set(&(MemoriaCfg){.dst = g_cli_link.calls, .val = 0, .bytes = sizeof(g_cli_link.calls)});
    cli_lock_give(&g_cli_link_lock);
    g_cli_link.keeper = (CliThread){keep_joined, NULL};
    if (!cli_thread_started(&g_cli_link.keeper))
    {
        cli_lock_take(&g_cli_link_lock);
        g_cli_link.open = 0;
        cli_lock_give(&g_cli_link_lock);
        cli_line_text(said, "the link's thread could not start");
        return 0;
    }
    return 1;
}

int cli_link_address(CliLinkAddress *address)
{
    cli_lock_take(&g_cli_link_lock);
    const int open = g_cli_link.open;
    if (open)
    {
        *address = g_cli_link.address;
    }
    cli_lock_give(&g_cli_link_lock);
    return open;
}

void cli_link_standing(CliLine *out)
{
    cli_lock_take(&g_cli_link_lock);
    if (g_cli_link.standing_size > 0u)
    {
        cli_line_counted(out, g_cli_link.standing, g_cli_link.standing_size);
    }
    else
    {
        char text[CLI_PATH_LONGEST * 3u];
        CliLine line = {text, sizeof(text), 0ull};
        cli_line_text(out, "{\"state\":\"joining\",\"machine\":");
        keep_machine(&line);
        cli_json_quoted(out, text, line.at);
        cli_line_text(out, ",\"address\":");
        line.at = 0ull;
        cli_link_address_text(&g_cli_link.address, &line);
        cli_json_quoted(out, text, line.at);
        cli_line_text(out, ",\"said\":\"\"}");
    }
    cli_lock_give(&g_cli_link_lock);
}

// a place among the calls taken for a call, waiting while all are taken, the lock held: the place, or NULL where the
// link ended or was not made within `until`
static CliLinkCall *keep_call_taken(unsigned long long until)
{
    for (;;)
    {
        const unsigned long long now = cli_clock_milliseconds();
        if (g_cli_link.ended || (now >= until))
        {
            return NULL;
        }
        for (unsigned int at = 0u; g_cli_link.joined && (at < CLI_LINK_CALLS); at += 1u)
        {
            if (!g_cli_link.calls[at].used)
            {
                return &g_cli_link.calls[at];
            }
        }
        (void)cli_wake_wait_for(&g_cli_link_changed, &g_cli_link_lock, until - now);
    }
}

// a call's line written to the server: 1, or 0 where the server stopped reading
static int keep_written(unsigned long long pipe, unsigned long long id, const char *name, const char *args,
                        unsigned long long size)
{
    char head[CLI_NAME_LONGEST * 8u];
    CliLine line = {head, sizeof(head), 0ull};
    cli_line_text(&line, "{\"id\":");
    cli_line_decimal(&line, id, 1u);
    cli_line_text(&line, ",\"name\":");
    cli_json_quoted(&line, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, ",\"args\":");
    return cli_line_fits(&line) && cli_spawn_written(pipe, head, line.at) &&
           cli_spawn_written(pipe, (size > 0u) ? args : "null", (size > 0u) ? size : 4u) &&
           cli_spawn_written(pipe, "}\n", 2u);
}

int cli_link_call(const char *name, const char *args, unsigned long long size, CliLine *out, CliLine *said)
{
    const unsigned long long until = cli_clock_milliseconds() + CLI_LINK_JOINING;
    cli_lock_take(&g_cli_link_lock);
    CliLinkCall *const call = keep_call_taken(until);
    if (call == NULL)
    {
        keep_machine(said);
        cli_line_text(said, " is not reached");
        cli_lock_give(&g_cli_link_lock);
        return 0;
    }
    *call = (CliLinkCall){1, 0, g_cli_link.next, out, said};
    g_cli_link.next += 1ull;
    cli_lock_give(&g_cli_link_lock);
    // the server's input is written under its own lock, taken before the link's, and only while the link is joined
    cli_lock_take(&g_cli_link_writing);
    cli_lock_take(&g_cli_link_lock);
    const int joined = g_cli_link.joined && !call->done;
    const unsigned long long pipe = g_cli_link.spawned.in;
    cli_lock_give(&g_cli_link_lock);
    const int written = joined && keep_written(pipe, call->id, name, args, size);
    cli_lock_give(&g_cli_link_writing);
    cli_lock_take(&g_cli_link_lock);
    if (!written && !call->done)
    {
        call->done = 2;
        cli_line_text(said, "the link to ");
        keep_machine(said);
        cli_line_text(said, " dropped");
    }
    while (!call->done)
    {
        cli_wake_wait(&g_cli_link_changed, &g_cli_link_lock);
    }
    const int answered = (call->done == 1);
    call->used = 0;
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
    return answered;
}

// the server ended where it runs and is not being stopped already, both locks held
static void keep_ended(void)
{
    if (g_cli_link.running && !g_cli_link.stopping)
    {
        g_cli_link.joined = 0;
        cli_spawn_input_closed(&g_cli_link.spawned);
        cli_spawn_killed(&g_cli_link.spawned);
    }
    cli_wake_all(&g_cli_link_changed);
}

void cli_link_end(void)
{
    cli_lock_take(&g_cli_link_writing);
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.ended = 1;
    keep_ended();
    cli_lock_give(&g_cli_link_lock);
    cli_lock_give(&g_cli_link_writing);
}

void cli_link_drop_now(void)
{
    cli_lock_take(&g_cli_link_writing);
    cli_lock_take(&g_cli_link_lock);
    keep_ended();
    cli_lock_give(&g_cli_link_lock);
    cli_lock_give(&g_cli_link_writing);
}
