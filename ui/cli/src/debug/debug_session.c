// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_session.c: the sessions in their places: a session taken for a request and let go, a place freed once its
// session is no longer listed, its adapter has ended and no request uses it, the requests asked of an adapter in rooms
// of their own, the sessions listed, a file's breakpoints set in each session that debugs it, and sessions ended
#include "debug_internal.h"

CliDebugRoom g_cli_debug = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};

static CliDebugAnswer s_answers[CLI_DEBUG_ANSWERS];
static CliLock s_answers_lock = CLI_LOCK_INIT;
static CliWake s_answers_wake = CLI_WAKE_INIT;

// a place freed where its session is no longer listed, its adapter's last event was heard and nothing uses it, under
// the sessions' lock
static void session_freed_where_done(CliDebugSession *session)
{
    if (session->taken && !session->listed && session->heard_end && (session->users == 0u))
    {
        cli_debug_place_freed(session);
    }
}

CliDebugSession *cli_debug_taken(unsigned long long id, CliLine *said)
{
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == id))
        {
            session->users += 1u;
            cli_dap_held(session->dap);
            cli_lock_give(&g_cli_debug.lock);
            return session;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    cli_line_text(said, "that session has ended");
    return NULL;
}

void cli_debug_let_go(CliDebugSession *session)
{
    cli_dap_given(session->dap);
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
            session->heard_end = 0;
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
            cli_dap_held(session->dap);
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
    cli_pieces_begun(&answer->pieces, &answer->asked, answer->piece, sizeof(answer->piece));
    answer->body = NULL;
    cli_json_begun(&answer->json, NULL, 0u, NULL, 0ull);
    answer->root = CLI_JSON_NONE;
    return answer;
}

void cli_debug_answer_given(CliDebugAnswer *answer)
{
    cli_pieces_given(&answer->pieces);
    cli_cell_given(answer->body);
    answer->body = NULL;
    cli_json_given(&answer->json);
    cli_lock_take(&s_answers_lock);
    answer->taken = 0;
    cli_wake_all(&s_answers_wake);
    cli_lock_give(&s_answers_lock);
}

int cli_debug_asked(CliDebugSession *session, const char *command, CliDebugAnswer *answer, unsigned long long wait,
                    CliLine *said)
{
    // the arguments joined into a cell of their size, an empty object where none are written
    unsigned long long written = 0ull;
    char *const joined = cli_pieces_joined(&answer->pieces, &answer->asked, "a debug request's arguments", &written);
    if (joined == NULL)
    {
        cli_line_text(said, "the request's arguments could not be held");
        return 0;
    }
    char piece[1024];
    CliPieces pieces;
    CliLine body;
    cli_pieces_begun(&pieces, &body, piece, sizeof(piece));
    const int answered = cli_dap_requested(session->dap, command, (written > 0ull) ? joined : "{}",
                                           (written > 0ull) ? written : 2ull, wait, &body, said);
    cli_cell_given(joined);
    unsigned long long size = 0ull;
    answer->body = cli_pieces_joined(&pieces, &body, "a debug request's answer", &size);
    if (!answered || (answer->body == NULL))
    {
        return 0;
    }
    answer->root = cli_json_taken(&answer->json, answer->body, size, 0u, 0ull, said);
    return answer->root != CLI_JSON_NONE;
}

void cli_debug_info_written(const CliDebugInfo *info, CliLine *out)
{
    cli_line_text(out, "{\"id\":");
    cli_line_decimal(out, info->id, 1u);
    cli_line_text(out, ",\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)info->name,
                          cellul.len(&(CatenaFinitaCfg){.src = info->name, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"tool\":");
    cli_json_quoted_lossy(out, (const unsigned char *)info->tool,
                          cellul.len(&(CatenaFinitaCfg){.src = info->tool, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"language\":");
    cli_json_quoted_lossy(out, (const unsigned char *)info->language,
                          cellul.len(&(CatenaFinitaCfg){.src = info->language, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"parent\":");
    if (info->has_parent)
    {
        cli_line_decimal(out, info->parent, 1u);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"filters\":[");
    for (unsigned int at = 0u; at < info->filter_count; at += 1u)
    {
        const CliDebugFilter *const filter = &info->filters[at];
        cli_line_text(out, (at > 0u) ? ",{\"filter\":" : "{\"filter\":");
        cli_json_quoted_lossy(out, (const unsigned char *)filter->filter,
                              cellul.len(&(CatenaFinitaCfg){.src = filter->filter, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"label\":");
        cli_json_quoted_lossy(out, (const unsigned char *)filter->label,
                              cellul.len(&(CatenaFinitaCfg){.src = filter->label, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, filter->on ? ",\"on\":true}" : ",\"on\":false}");
    }
    const int can[7] = {info->conditions,  info->hits,           info->logs, info->memory,
                        info->disassembly, info->exception_info, info->data};
    const char *const names[7] = {"conditions", "hits", "logs", "memory", "disassembly", "exception_info", "data"};
    cli_line_character(out, ']');
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        cli_line_text(out, ",\"");
        cli_line_text(out, names[at]);
        cli_line_text(out, can[at] ? "\":true" : "\":false");
    }
    cli_line_text(out, ",\"process\":");
    if (info->has_process)
    {
        cli_line_decimal(out, info->process, 1u);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, info->step_back ? ",\"step_back\":true" : ",\"step_back\":false");
    cli_line_text(out, info->recorded ? ",\"recorded\":true}" : ",\"recorded\":false}");
}

void cli_debug_sessions(CliLine *out)
{
    unsigned long long last = 0ull;
    int written = 0;
    cli_line_character(out, '[');
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
        cli_line_text(out, written ? "," : "");
        cli_debug_info_written(&next->info, out);
        written = 1;
        last = next->info.id;
    }
    cli_lock_give(&g_cli_debug.lock);
    cli_line_character(out, ']');
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
                  CliLine *out, CliLine *said)
{
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    cli_line_text(&answer->asked, "{\"source\":{\"path\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)file,
                          cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    cli_line_text(&answer->asked, "},\"breakpoints\":");
    cli_debug_breakpoints_written(json, breakpoints, &answer->asked);
    cli_line_character(&answer->asked, '}');
    if (!cli_debug_asked(session, "setBreakpoints", answer, CLI_DEBUG_ASKING, said))
    {
        cli_debug_answer_given(answer);
        return 0;
    }
    const CliJson *const said_json = &answer->json;
    const unsigned int placed = cli_json_member(said_json, answer->root, "breakpoints");
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; cli_json_item(said_json, placed, at) != CLI_JSON_NONE; at += 1u)
    {
        long long number = 0ll;
        const unsigned int one = cli_json_item(said_json, placed, at);
        if (!cli_debug_signed(said_json, cli_json_member(said_json, one, "id"), &number))
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
    // each answer paired with the breakpoint asked for in its turn, as many as both have
    cli_line_character(out, '[');
    for (unsigned int at = 0u; (cli_json_item(said_json, placed, at) != CLI_JSON_NONE) &&
                               (cli_json_item(json, breakpoints, at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = cli_json_item(said_json, placed, at);
        unsigned long long line = 0ull;
        if (cli_debug_unsigned(said_json, cli_json_member(said_json, one, "line"), &line))
        {
            line = (line > 0ull) ? line - 1ull : 0ull;
        }
        else
        {
            (void)cli_debug_unsigned(json, cli_json_member(json, cli_json_item(json, breakpoints, at), "line"), &line);
        }
        cli_line_text(out, (at > 0u) ? ",[" : "[");
        // a line keeps its low 32 bits, as the window's line numbers are
        cli_line_decimal(out, line & 0xffffffffull, 1u);
        cli_line_text(out,
                      cli_debug_true(said_json, cli_json_member(said_json, one, "verified")) ? ",true]" : ",false]");
    }
    cli_line_character(out, ']');
    cli_debug_answer_given(answer);
    return 1;
}

void cli_debug_breakpoints(const char *root, const char *path, const CliJson *json, unsigned int breakpoints,
                           CliLine *out)
{
    char full[CLI_PATH_LONGEST];
    char why[CLI_SAID_LONGEST];
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
        CliLine said = {why, sizeof(why), 0ull};
        CliDebugSession *const taken = takes ? cli_debug_taken(id, &said) : NULL;
        if (taken == NULL)
        {
            continue;
        }
        // the first session's lines are what is told; the others are set and their lines passed over
        const unsigned long long before = out->at;
        if (cli_debug_set(taken, full, json, breakpoints, out, &said) && !placed)
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
        cli_line_text(out, "null");
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

// the session's own use given back on a thread of its own, as an adapter's event, heard on the adapter's reader, ends
// it: the adapter waits on that reader as it is let go
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
