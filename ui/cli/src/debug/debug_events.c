// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_events.c: an adapter's events as a session receives them: its readiness, the process it debugs, a stop, which
// ends a step across, and a stop at a breakpoint the reader did not place reported as the stop of a data breakpoint,
// which it is; then each event handed on with the session's number
#include "debug_internal.h"

// the breakpoints a step across set on the native session taken away, on a thread of their own
static void step_across_cleared(void *context)
{
    CliDebugSession *const session = (CliDebugSession *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliDebugSession *const native = cli_debug_taken(session->cleared, &err_msg);
    if (native != NULL)
    {
        CliDebugAnswer *const answer = cli_debug_answer_taken();
        mmgr_line_put(&answer->args,
                      "{\"expression\":\"`breakpoint delete --force " CLI_DEBUG_ACROSS "\",\"context\":\"repl\"}");
        (void)cli_debug_request(native, "evaluate", answer, CLI_DEBUG_REQUEST_TIMEOUT_MS, &err_msg);
        cli_debug_answer_given(answer);
        cli_debug_let_go(native);
    }
    cli_lock_take(&g_cli_debug.lock);
    session->clearing = 0;
    session->users -= 1u;
    if (!session->listed && session->adapter_ended && (session->users == 0u))
    {
        cli_debug_place_freed(session);
    }
    cli_lock_give(&g_cli_debug.lock);
}

// a stop read into `json` at `root` reported as a data breakpoint's where the session's data breakpoints are lldb's
// watchpoints and the stop is at a breakpoint none of whose numbers the reader placed: 1 with it written into `out`,
// or 0 where it is reported as it came
static int stop_rewritten(CliDebugSession *session, CliJson *json, unsigned int root, VerbaLinea *out)
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
    return !json->full && mmgr_line_ok(out);
}

// a stop's body reported as a data breakpoint's, as stop_rewritten reports it, the body read into a tree taken to its
// size and given back: 1 with the body written into `out`, or 0 where it is reported as it came
static int data_stop_rewritten(CliDebugSession *session, const char *body, unsigned long long size, VerbaLinea *out)
{
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, body, size, CLI_DEBUG_EVENT_MORE_NODES, CLI_DEBUG_EVENT_MORE_TEXT,
                                             &err_msg, &g_cli_debug_cells);
    const int written = stop_rewritten(session, &json, root, out);
    cli_json_given(&json);
    return written;
}

void cli_debug_event(void *context, const char *event, const char *body, unsigned long long size)
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
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        unsigned long long pid = 0ull;
        const unsigned int root = cli_json_taken(&json, body, size, 0u, 0ull, &err_msg, &g_cli_debug_cells);
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
            session->clearer = (CliThread){step_across_cleared, session};
        }
        cli_lock_give(&g_cli_debug.lock);
        if (clears && !cli_thread_started(&session->clearer))
        {
            step_across_cleared(session);
        }
    }
    // a stop rewritten is the body with two members written again, in a cell of the body's size and some over
    const int stopped = cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "stopped", .cap = CLI_CELLS_POOL});
    const unsigned long long rewritten_room = stopped ? (size + CLI_DEBUG_EVENT_MORE_TEXT) : 0ull;
    char *const rewritten_cell = stopped ? g_cli_cells->temporary_buf_alloc(rewritten_room, &g_cli_debug_cells) : NULL;
    VerbaLinea rewritten_body = mmgr_line_over(rewritten_cell, rewritten_room);
    const int rewritten = stopped && data_stop_rewritten(session, body, size, &rewritten_body);
    session->sink.emit(session->sink.context, session->info.id, event, rewritten ? rewritten_body.out : body,
                       rewritten ? rewritten_body.at : size);
    g_cli_cells->persistent_buf_release(rewritten_body.out, &g_cli_debug_cells);
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "adapterStopped", .cap = CLI_CELLS_POOL}))
    {
        // the adapter's last event: its place may be freed once nothing else uses it
        cli_lock_take(&g_cli_debug.lock);
        session->adapter_ended = 1;
        if (!session->listed && (session->users == 0u))
        {
            cli_debug_place_freed(session);
        }
        cli_lock_give(&g_cli_debug.lock);
    }
}
