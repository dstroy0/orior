// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_session.c: the sessions in their places: a session taken for a request and let go, a place freed once its
// session is no longer listed, its adapter has ended and no request uses it, the requests sent to an adapter in rooms
// of their own, the sessions listed, a file's breakpoints set in each session that debugs it, and sessions ended
#include "debug_internal.h"

CliDebugRoom g_cli_debug = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};

static CliDebugAnswer s_answers[CLI_DEBUG_ANSWERS];
static CliLock s_answers_lock = CLI_LOCK_INIT;
static CliWake s_answers_wake = CLI_WAKE_INIT;

// a place freed where its session is no longer listed, its adapter's last event was received and nothing uses it, under
// the sessions' lock
static void session_freed_where_done(CliDebugSession *session)
{
    if (session->taken && !session->listed && session->adapter_ended && (session->users == 0u))
    {
        cli_debug_place_freed(session);
    }
}

CliDebugSession *cli_debug_taken(unsigned long long id, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == id))
        {
            session->users += 1u;
            cli_dap_ref(session->dap);
            cli_lock_give(&g_cli_debug.lock);
            return session;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    mmgr_line_put(err_msg, "that session has ended");
    return NULL;
}

void cli_debug_let_go(CliDebugSession *session)
{
    cli_dap_unref(session->dap);
    cli_lock_take(&g_cli_debug.lock);
    session->users -= 1u;
    session_freed_where_done(session);
    cli_lock_give(&g_cli_debug.lock);
}

CliDebugSession *cli_debug_place(void)
{
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (!session->taken)
        {
            session->taken = 1;
            session->listed = 0;
            session->adapter_ended = 0;
            session->users = 1u;
            session->dap = NULL;
            session->data_count = 0u;
            session->placed_count = 0u;
            session->watching = 0;
            session->scripted = 0;
            session->has_across = 0;
            session->pid = 0ull;
            session->initialized = 0;
            memor.set(&(MemoriaCfg){.dst = &session->info, .val = 0, .bytes = sizeof(session->info)});
            cli_lock_give(&g_cli_debug.lock);
            return session;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    return NULL;
}

CliDebugSession *cli_debug_native_of(unsigned long long id)
{
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && session->info.has_parent && (session->info.parent == id) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = session->info.language, .other = "python", .cap = CLI_CELLS_POOL})) &&
            session->info.has_process)
        {
            session->users += 1u;
            cli_dap_ref(session->dap);
            cli_lock_give(&g_cli_debug.lock);
            return session;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    return NULL;
}

CliDebugAnswer *cli_debug_answer_taken(void)
{
    cli_lock_take(&s_answers_lock);
    CliDebugAnswer *answer = NULL;
    while (answer == NULL)
    {
        for (unsigned int at = 0u; (at < CLI_DEBUG_ANSWERS) && (answer == NULL); at += 1u)
        {
            answer = s_answers[at].taken ? NULL : &s_answers[at];
        }
        if (answer == NULL)
        {
            cli_wake_wait(&s_answers_wake, &s_answers_lock);
        }
    }
    answer->taken = 1;
    cli_lock_give(&s_answers_lock);
    cli_pieces_begun(&answer->pieces, &answer->args, answer->piece, sizeof(answer->piece), &g_cli_debug_cells);
    answer->body = NULL;
    cli_json_begun(&answer->json, NULL, 0u, NULL, 0ull);
    answer->root = CLI_JSON_NONE;
    return answer;
}

void cli_debug_answer_given(CliDebugAnswer *answer)
{
    cli_pieces_given(&answer->pieces);
    cli_cell_release(answer->body, &g_cli_debug_cells);
    answer->body = NULL;
    cli_json_given(&answer->json);
    cli_lock_take(&s_answers_lock);
    answer->taken = 0;
    cli_wake_all(&s_answers_wake);
    cli_lock_give(&s_answers_lock);
}

int cli_debug_request(CliDebugSession *session, const char *command, CliDebugAnswer *answer, unsigned long long wait,
                      VerbaLinea *err_msg)
{
    // the arguments joined into a cell of their size, an empty object where none are written
    unsigned long long written = 0ull;
    char *const joined = cli_pieces_joined(&answer->pieces, &answer->args, "a debug request's arguments", &written);
    if (joined == NULL)
    {
        mmgr_line_put(err_msg, "the request's arguments could not be stored");
        return 0;
    }
    char piece[1024];
    CliPieces pieces;
    VerbaLinea body;
    cli_pieces_begun(&pieces, &body, piece, sizeof(piece), &g_cli_debug_cells);
    const int answered = cli_dap_requested(session->dap, command, (written > 0ull) ? joined : "{}",
                                           (written > 0ull) ? written : 2ull, wait, &body, err_msg);
    cli_cell_release(joined, &g_cli_debug_cells);
    unsigned long long size = 0ull;
    answer->body = cli_pieces_joined(&pieces, &body, "a debug request's answer", &size);
    if (!answered || (answer->body == NULL))
    {
        return 0;
    }
    answer->root = cli_json_taken(&answer->json, answer->body, size, 0u, 0ull, err_msg, &g_cli_debug_cells);
    return answer->root != CLI_JSON_NONE;
}

void cli_debug_info_written(const CliDebugInfo *info, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"id\":");
    mmgr_line_decimal(out, info->id, 1u);
    mmgr_line_put(out, ",\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)info->name,
                          cellul.len(&(CatenaFinitaCfg){.src = info->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"tool\":");
    cli_json_quoted_lossy(out, (const unsigned char *)info->tool,
                          cellul.len(&(CatenaFinitaCfg){.src = info->tool, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"language\":");
    cli_json_quoted_lossy(out, (const unsigned char *)info->language,
                          cellul.len(&(CatenaFinitaCfg){.src = info->language, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"parent\":");
    if (info->has_parent)
    {
        mmgr_line_decimal(out, info->parent, 1u);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"filters\":[");
    for (unsigned int at = 0u; at < info->filter_count; at += 1u)
    {
        const CliDebugFilter *const filter = &info->filters[at];
        mmgr_line_put(out, (at > 0u) ? ",{\"filter\":" : "{\"filter\":");
        cli_json_quoted_lossy(out, (const unsigned char *)filter->filter,
                              cellul.len(&(CatenaFinitaCfg){.src = filter->filter, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"label\":");
        cli_json_quoted_lossy(out, (const unsigned char *)filter->label,
                              cellul.len(&(CatenaFinitaCfg){.src = filter->label, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, filter->on ? ",\"on\":true}" : ",\"on\":false}");
    }
    const int can[7] = {info->conditions,  info->hits,           info->logs, info->memory,
                        info->disassembly, info->exception_info, info->data};
    const char *const names[7] = {"conditions", "hits", "logs", "memory", "disassembly", "exception_info", "data"};
    mmgr_line_ch(out, ']');
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        mmgr_line_put(out, ",\"");
        mmgr_line_put(out, names[at]);
        mmgr_line_put(out, can[at] ? "\":true" : "\":false");
    }
    mmgr_line_put(out, ",\"process\":");
    if (info->has_process)
    {
        mmgr_line_decimal(out, info->process, 1u);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, info->step_back ? ",\"step_back\":true" : ",\"step_back\":false");
    mmgr_line_put(out, info->recorded ? ",\"recorded\":true}" : ",\"recorded\":false}");
}

void cli_debug_sessions(VerbaLinea *out)
{
    unsigned long long last = 0ull;
    int written = 0;
    mmgr_line_ch(out, '[');
    cli_lock_take(&g_cli_debug.lock);
    // by number, the order they were started in
    for (;;)
    {
        const CliDebugSession *next = NULL;
        for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
        {
            const CliDebugSession *const one = &g_cli_debug.sessions[at];
            if (one->taken && one->listed && (one->info.id > last) &&
                ((next == NULL) || (one->info.id < next->info.id)))
            {
                next = one;
            }
        }
        if (next == NULL)
        {
            break;
        }
        mmgr_line_put(out, written ? "," : "");
        cli_debug_info_written(&next->info, out);
        written = 1;
        last = next->info.id;
    }
    cli_lock_give(&g_cli_debug.lock);
    mmgr_line_ch(out, ']');
}

int cli_debug_running(void)
{
    int running = 0;
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        running = running || (g_cli_debug.sessions[at].taken && g_cli_debug.sessions[at].listed);
    }
    cli_lock_give(&g_cli_debug.lock);
    return running;
}

int cli_debug_set(CliDebugSession *session, const char *file, const CliJson *json, unsigned int breakpoints,
                  VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    mmgr_line_put(&answer->args, "{\"source\":{\"path\":");
    cli_json_quoted_lossy(&answer->args, (const unsigned char *)file,
                          cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&answer->args, "},\"breakpoints\":");
    cli_debug_breakpoints_written(json, breakpoints, &answer->args);
    mmgr_line_ch(&answer->args, '}');
    if (!cli_debug_request(session, "setBreakpoints", answer, CLI_DEBUG_REQUEST_TIMEOUT_MS, err_msg))
    {
        cli_debug_answer_given(answer);
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int placed = cli_json_member(reply, answer->root, "breakpoints");
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; cli_json_item(reply, placed, at) != CLI_JSON_NONE; at += 1u)
    {
        long long number = 0ll;
        const unsigned int one = cli_json_item(reply, placed, at);
        if (!cli_debug_signed(reply, cli_json_member(reply, one, "id"), &number))
        {
            continue;
        }
        session->placed = cli_debug_grown(session->placed, session->placed_count, &session->placed_room,
                                          sizeof(*session->placed), CLI_DEBUG_PLACED);
        if (session->placed_count < session->placed_room)
        {
            session->placed[session->placed_count] = number;
            session->placed_count += 1u;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    // each answer paired with the breakpoint requested in its turn, as many as both have
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u;
         (cli_json_item(reply, placed, at) != CLI_JSON_NONE) && (cli_json_item(json, breakpoints, at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = cli_json_item(reply, placed, at);
        unsigned long long line = 0ull;
        if (cli_debug_unsigned(reply, cli_json_member(reply, one, "line"), &line))
        {
            line = (line > 0ull) ? line - 1ull : 0ull;
        }
        else
        {
            (void)cli_debug_unsigned(json, cli_json_member(json, cli_json_item(json, breakpoints, at), "line"), &line);
        }
        mmgr_line_put(out, (at > 0u) ? ",[" : "[");
        // a line retains its low 32 bits, as the window's line numbers are
        mmgr_line_decimal(out, line & 0xffffffffull, 1u);
        mmgr_line_put(out, cli_debug_true(reply, cli_json_member(reply, one, "verified")) ? ",true]" : ",false]");
    }
    mmgr_line_ch(out, ']');
    cli_debug_answer_given(answer);
    return 1;
}

void cli_debug_breakpoints(const char *root, const char *path, const CliJson *json, unsigned int breakpoints,
                           VerbaLinea *out)
{
    char full[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    cli_full(root, path, full);
    const unsigned long long start = out->at;
    int placed = 0;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        cli_lock_take(&g_cli_debug.lock);
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        const int takes = session->taken && session->listed && cli_debug_takes(session->info.language, path);
        const unsigned long long id = session->info.id;
        cli_lock_give(&g_cli_debug.lock);
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        CliDebugSession *const taken = takes ? cli_debug_taken(id, &err_msg) : NULL;
        if (taken == NULL)
        {
            continue;
        }
        // the first session's lines are what is reported; the others are set and their lines passed over
        const unsigned long long before = out->at;
        if (cli_debug_set(taken, full, json, breakpoints, out, &err_msg) && !placed)
        {
            placed = 1;
        }
        else
        {
            out->at = before;
        }
        cli_debug_let_go(taken);
    }
    if (!placed)
    {
        out->at = start;
        mmgr_line_put(out, "null");
    }
}

void cli_debug_process(unsigned long long id, unsigned int pid)
{
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == id))
        {
            session->info.has_process = 1;
            session->info.process = pid;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
}

// the session's own use given back on a thread of its own, as an adapter's event, received on the adapter's reader,
// ends it: the adapter waits on that reader as it is let go
static void session_given(void *context)
{
    cli_debug_let_go((CliDebugSession *)context);
}

void cli_debug_ended(unsigned long long id)
{
    CliDebugSession *ended = NULL;
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == id))
        {
            session->listed = 0;
            ended = session;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    if (ended == NULL)
    {
        return;
    }
    ended->giver = (CliThread){session_given, ended};
    if (!cli_thread_started(&ended->giver))
    {
        session_given(ended);
    }
}

void cli_debug_stop(unsigned long long id)
{
    CliDebugSession *ended[CLI_DEBUG_SESSIONS];
    unsigned int count = 0u;
    unsigned long long gone[CLI_DEBUG_SESSIONS + 1u];
    unsigned int going = 1u;
    gone[0] = id;
    cli_lock_take(&g_cli_debug.lock);
    // the session, then each it started, and each they started in turn
    while (going > 0u)
    {
        going -= 1u;
        const unsigned long long one = gone[going];
        for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
        {
            CliDebugSession *const session = &g_cli_debug.sessions[at];
            const int taken =
                session->taken && session->listed &&
                ((session->info.id == one) || (session->info.has_parent && (session->info.parent == one)));
            if (!taken)
            {
                continue;
            }
            session->listed = 0;
            ended[count] = session;
            count += 1u;
            if ((session->info.id != one) && (going < CLI_DEBUG_SESSIONS))
            {
                gone[going] = session->info.id;
                going += 1u;
            }
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_dap_stopped(ended[at]->dap);
        cli_debug_let_go(ended[at]);
    }
}

void cli_debug_stop_all(void)
{
    CliDebugSession *ended[CLI_DEBUG_SESSIONS];
    unsigned int count = 0u;
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed)
        {
            session->listed = 0;
            ended[count] = session;
            count += 1u;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_dap_stopped(ended[at]->dap);
        cli_debug_let_go(ended[at]);
    }
}
