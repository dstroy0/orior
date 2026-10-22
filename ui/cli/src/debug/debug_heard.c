// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_heard.c: an adapter's events as a session hears them: its readiness, the process it debugs, a stop, which ends
// a step across, and a stop at a breakpoint the reader did not place told as the stop of a data breakpoint, which it
// is; then each event handed on with the session's number
#include "debug_internal.h"

// the breakpoints a step across set on the native session taken away, on a thread of their own
static void heard_cleared(void *context)
{
    CliDebugSession *const session = (CliDebugSession *)context;
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    CliDebugSession *const native = cli_debug_taken(session->cleared, &said);
    if (native != NULL)
    {
        CliDebugAnswer *const answer = cli_debug_answer_taken();
        cli_line_text(&answer->asked,
                      "{\"expression\":\"`breakpoint delete --force " CLI_DEBUG_ACROSS "\",\"context\":\"repl\"}");
        (void)cli_debug_asked(native, "evaluate", answer, CLI_DEBUG_ASKING, &said);
        cli_debug_answer_given(answer);
        cli_debug_let_go(native);
    }
    cli_lock_take(&g_cli_debug.lock);
    session->clearing = 0;
    session->users -= 1u;
    if (!session->listed && session->heard_end && (session->users == 0u))
    {
        cli_debug_place_freed(session);
    }
    cli_lock_give(&g_cli_debug.lock);
}

// a stop read into `json` at `root` told as a data breakpoint's where the session's data breakpoints are lldb's
// watchpoints and the stop is at a breakpoint none of whose numbers the reader placed: 1 with it written into `out`,
// or 0 where it is told as it came
static int heard_rewritten(CliDebugSession *session, CliJson *json, unsigned int root, CliLine *out)
{
    const char *const reason = cli_json_string(json, cli_json_member(json, root, "reason"), NULL);
    cli_lock_take(&g_cli_debug.lock);
    const int watching = session->watching;
    cli_lock_give(&g_cli_debug.lock);
    if ((root == CLI_JSON_NONE) || (reason == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "breakpoint", .cap = CLI_CELLS_POOL})) || !watching)
    {
        return 0;
    }
    const unsigned int hit = cli_json_member(json, root, "hitBreakpointIds");
    int any = 0;
    int placed = 0;
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; cli_json_item(json, hit, at) != CLI_JSON_NONE; at += 1u)
    {
        long long number = 0ll;
        if (!cli_debug_signed(json, cli_json_item(json, hit, at), &number))
        {
            continue;
        }
        any = 1;
        for (unsigned int one = 0u; one < session->placed_count; one += 1u)
        {
            placed = placed || (session->placed[one] == number);
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    if (!any || placed)
    {
        return 0;
    }
    cli_json_set(json, root, "reason", cli_json_string_made(json, "data breakpoint", 15ull));
    cli_json_set(json, root, "description", cli_json_string_made(json, "data changed", 12ull));
    cli_json_write(json, root, out, 0u, 0);
    return !json->full && cli_line_fits(out);
}

// a stop's body told as a data breakpoint's, as heard_rewritten tells it, the body read into a tree taken to its size
// and given back: 1 with the body written into `out`, or 0 where it is told as it came
static int heard_data_stop(CliDebugSession *session, const char *body, unsigned long long size, CliLine *out)
{
    CliJson json = {0};
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root =
        cli_json_taken(&json, body, size, CLI_DEBUG_EVENT_MORE_NODES, CLI_DEBUG_EVENT_MORE_TEXT, &said);
    const int written = heard_rewritten(session, &json, root, out);
    cli_json_given(&json);
    return written;
}

void cli_debug_heard(void *context, const char *event, const char *body, unsigned long long size)
{
    CliDebugSession *const session = (CliDebugSession *)context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "initialized", .cap = CLI_CELLS_POOL}))
    {
        cli_lock_take(&g_cli_debug.lock);
        session->initialized = 1;
        cli_wake_all(&g_cli_debug.wake);
        cli_lock_give(&g_cli_debug.lock);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "process", .cap = CLI_CELLS_POOL}))
    {
        CliJson json = {0};
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        unsigned long long pid = 0ull;
        const unsigned int root = cli_json_taken(&json, body, size, 0u, 0ull, &said);
        if (cli_debug_unsigned(&json, cli_json_member(&json, root, "systemProcessId"), &pid))
        {
            cli_lock_take(&g_cli_debug.lock);
            session->pid = pid;
            cli_lock_give(&g_cli_debug.lock);
        }
        cli_json_given(&json);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "stopped", .cap = CLI_CELLS_POOL}))
    {
        // a stop ends a step across, and its breakpoints go
        cli_lock_take(&g_cli_debug.lock);
        const int clears = session->has_across && !session->clearing;
        if (clears)
        {
            session->has_across = 0;
            session->cleared = session->across;
            session->clearing = 1;
            session->users += 1u;
            session->clearer = (CliThread){heard_cleared, session};
        }
        cli_lock_give(&g_cli_debug.lock);
        if (clears && !cli_thread_started(&session->clearer))
        {
            heard_cleared(session);
        }
    }
    // a stop rewritten is the body with two members said again, in a cell of the body's size and some over, grown where
    // it is longer
    CliLine told = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int stopped = cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "stopped", .cap = CLI_CELLS_POOL});
    if (stopped)
    {
        (void)cli_line_growing(&told, size + CLI_DEBUG_EVENT_MORE_TEXT, "a stop told as a data breakpoint's");
    }
    const int rewritten = stopped && heard_data_stop(session, body, size, &told);
    session->emit.told(session->emit.context, session->info.id, event, rewritten ? told.out : body,
                       rewritten ? told.at : size);
    cli_cell_given(told.out);
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "adapterStopped", .cap = CLI_CELLS_POOL}))
    {
        // the adapter's last event: its place may be freed once nothing else uses it
        cli_lock_take(&g_cli_debug.lock);
        session->heard_end = 1;
        if (!session->listed && (session->users == 0u))
        {
            cli_debug_place_freed(session);
        }
        cli_lock_give(&g_cli_debug.lock);
    }
}
