// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_debug.c: the calls of the debugger: a session started, its events going to the page as debug-event, a
// session forgotten once its adapter says its program ended and its process kept as the adapter names it; the
// sessions, the breakpoints, the exceptions a session stops on, its threads, its stack, scopes, variables and values;
// its steps, a step across into the tree's own C, a watch of data, a recorded run's history and a step of it gone to;
// memory and instructions; a Python line's bytecode; the processes; and a session or every one stopped. A path an
// adapter names is written as it stands in the tree
#include "../debug/debug.h"
#include "serve_internal.h"

// what a session says on its own sent to the page as debug-event, a session's end and its process kept first
static void debug_told(void *context, unsigned long long session, const char *event, const char *body,
                       unsigned long long size)
{
    (void)context;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "terminated", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "adapterStopped", .cap = CLI_CELLS_POOL})))
    {
        cli_debug_ended(session);
    }
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "process", .cap = CLI_CELLS_POOL})) &&
        cli_lsp_member(body, size, "systemProcessId", &at, &length))
    {
        unsigned long long pid = 0ull;
        int number = (length > 0ull);
        for (unsigned long long digit = 0ull; number && (digit < length); digit += 1ull)
        {
            number = (body[at + digit] >= '0') && (body[at + digit] <= '9') && (pid <= 0xffffffffull);
            pid = (pid * 10ull) + (unsigned long long)(body[at + digit] - '0');
        }
        if (number && (pid <= 0xffffffffull))
        {
            cli_debug_process(session, (unsigned int)pid);
        }
    }
    static char s_event[1u << 22u];
    static CliLock s_event_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_event_lock);
    CliLine line = {s_event, sizeof(s_event), 0ull};
    cli_line_text(&line, "{\"session\":");
    cli_line_decimal(&line, session, 1u);
    cli_line_text(&line, ",\"event\":");
    cli_json_quoted(&line, event, cellul.len(&(CatenaFinitaCfg){.src = event, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, ",\"body\":");
    cli_line_counted(&line, (size > 0ull) ? body : "null", (size > 0ull) ? size : 4ull);
    cli_line_character(&line, '}');
    if (cli_line_fits(&line))
    {
        cli_serve_emitted("debug-event", s_event, line.at);
    }
    cli_lock_give(&s_event_lock);
}

// a path an adapter named written as it stands in the tree, the tree its context
static void debug_path_written(void *context, const char *path, CliLine *out)
{
    const char *const root = context;
    char shown[CLI_PATH_LONGEST];
    CliLine line = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(root, path, &line);
    cli_json_quoted_lossy(out, (const unsigned char *)shown, cli_line_fits(&line) ? line.at : 0ull);
}

// the argument `session`: 1, or 0 with why said
static int debug_session(const CliServeArgs *args, unsigned long long *session, CliLine *said)
{
    return cli_serve_whole(args, "session", session, said);
}

static int debug_start(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    const CliDebugEmit emit = {debug_told, NULL};
    return cli_debug_start(root, args->json, cli_serve_arg(args, "start"), cli_serve_arg(args, "options"), emit, out,
                           said);
}

static int debug_sessions(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_debug_sessions(out);
    return 1;
}

static int debug_breakpoints(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_debug_breakpoints(root, path, args->json, cli_serve_arg(args, "breakpoints"), out);
    return 1;
}

static int debug_exceptions(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    return debug_session(args, &session, said) &&
           cli_debug_exceptions(session, args->json, cli_serve_arg(args, "on"), out, said);
}

static int debug_raised(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long thread = 0ll;
    return debug_session(args, &session, said) && cli_serve_signed(args, "thread", &thread, said) &&
           cli_debug_raised(session, thread, out, said);
}

static int debug_threads(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    return debug_session(args, &session, said) && cli_debug_threads(session, out, said);
}

static int debug_stack(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long session = 0ull;
    long long thread = 0ll;
    return cli_serve_tree(root, said) && debug_session(args, &session, said) &&
           cli_serve_signed(args, "thread", &thread, said) &&
           cli_debug_stack(session, thread, debug_path_written, root, out, said);
}

static int debug_scopes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long frame = 0ll;
    return debug_session(args, &session, said) && cli_serve_signed(args, "frame", &frame, said) &&
           cli_debug_scopes(session, frame, out, said);
}

static int debug_variables(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long reference = 0ll;
    return debug_session(args, &session, said) && cli_serve_signed(args, "reference", &reference, said) &&
           cli_debug_variables(session, reference, out, said);
}

static int debug_evaluate(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long frame = 0ll;
    const char *const expression = cli_serve_string(args, "expression", said);
    const char *const context = (expression != NULL) ? cli_serve_string(args, "context", said) : NULL;
    if ((context == NULL) || !debug_session(args, &session, said))
    {
        return 0;
    }
    const unsigned int given = cli_serve_arg(args, "frame");
    const int has_frame = (given != CLI_JSON_NONE) && !cli_json_is(args->json, given, CFG_JSON_NULL);
    if (has_frame && !cli_serve_signed(args, "frame", &frame, said))
    {
        return 0;
    }
    return cli_debug_evaluate(session, expression, has_frame, frame, context, out, said);
}

static int debug_step(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long thread = 0ll;
    const char *const how = cli_serve_string(args, "how", said);
    return (how != NULL) && debug_session(args, &session, said) && cli_serve_signed(args, "thread", &thread, said) &&
           cli_debug_step(session, how, thread, out, said);
}

static int debug_memory(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long offset = 0ll;
    unsigned long long count = 0ull;
    const char *const reference = cli_serve_string(args, "reference", said);
    return (reference != NULL) && debug_session(args, &session, said) &&
           cli_serve_signed(args, "offset", &offset, said) && cli_serve_whole(args, "count", &count, said) &&
           cli_debug_memory(session, reference, offset, count, out, said);
}

static int debug_instructions(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long offset = 0ll;
    long long count = 0ll;
    const char *const reference = cli_serve_string(args, "reference", said);
    return (reference != NULL) && debug_session(args, &session, said) &&
           cli_serve_signed(args, "offset", &offset, said) && cli_serve_signed(args, "count", &count, said) &&
           cli_debug_instructions(session, reference, offset, count, out, said);
}

static int debug_bytecode(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long line = 0ull;
    // the line is cut to its low word, as no file holds more
    return cli_serve_at(args, root, full, said) && cli_serve_whole(args, "line", &line, said) &&
           cli_debug_bytecode(root, full, (unsigned int)line, out, said);
}

static int debug_watch_data(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    long long reference = 0ll;
    unsigned long long bytes = 0ull;
    const char *const name = cli_serve_string(args, "name", said);
    if ((name == NULL) || !debug_session(args, &session, said))
    {
        return 0;
    }
    const unsigned int referenced = cli_serve_arg(args, "reference");
    const unsigned int sized = cli_serve_arg(args, "bytes");
    const int has_reference = (referenced != CLI_JSON_NONE) && !cli_json_is(args->json, referenced, CFG_JSON_NULL);
    const int has_bytes = (sized != CLI_JSON_NONE) && !cli_json_is(args->json, sized, CFG_JSON_NULL);
    if ((has_reference && !cli_serve_signed(args, "reference", &reference, said)) ||
        (has_bytes && !cli_serve_whole(args, "bytes", &bytes, said)))
    {
        return 0;
    }
    return cli_debug_watch_data(session, name, has_reference, reference, has_bytes, bytes, out, said);
}

static int debug_history(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long session = 0ull;
    long long frame = 0ll;
    const char *const name = cli_serve_string(args, "name", said);
    return (name != NULL) && cli_serve_tree(root, said) && debug_session(args, &session, said) &&
           cli_serve_signed(args, "frame", &frame, said) &&
           cli_debug_history(session, frame, name, debug_path_written, root, out, said);
}

static int debug_step_across(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long session = 0ull;
    long long thread = 0ll;
    return cli_serve_tree(root, said) && debug_session(args, &session, said) &&
           cli_serve_signed(args, "thread", &thread, said) && cli_debug_step_across(root, session, thread, out, said);
}

static int debug_goto(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long session = 0ull;
    unsigned long long step = 0ull;
    return debug_session(args, &session, said) && cli_serve_whole(args, "step", &step, said) &&
           cli_debug_goto(session, step, out, said);
}

static int debug_processes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_debug_processes(out);
    return 1;
}

static int debug_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const unsigned int given = cli_serve_arg(args, "session");
    if ((given == CLI_JSON_NONE) || cli_json_is(args->json, given, CFG_JSON_NULL))
    {
        cli_debug_stop_all();
    }
    else
    {
        unsigned long long session = 0ull;
        if (!debug_session(args, &session, said))
        {
            return 0;
        }
        cli_debug_stop(session);
    }
    cli_line_text(out, "null");
    return 1;
}

const CliServeCall g_cli_serve_debug_calls[] = {
    {"debug_start", debug_start},
    {"debug_sessions", debug_sessions},
    {"debug_breakpoints", debug_breakpoints},
    {"debug_exceptions", debug_exceptions},
    {"debug_raised", debug_raised},
    {"debug_threads", debug_threads},
    {"debug_stack", debug_stack},
    {"debug_scopes", debug_scopes},
    {"debug_variables", debug_variables},
    {"debug_evaluate", debug_evaluate},
    {"debug_step", debug_step},
    {"debug_memory", debug_memory},
    {"debug_instructions", debug_instructions},
    {"debug_bytecode", debug_bytecode},
    {"debug_watch_data", debug_watch_data},
    {"debug_history", debug_history},
    {"debug_step_across", debug_step_across},
    {"debug_goto", debug_goto},
    {"debug_processes", debug_processes},
    {"debug_stop", debug_stop},
};
const unsigned int g_cli_serve_debug_count = sizeof(g_cli_serve_debug_calls) / sizeof(g_cli_serve_debug_calls[0]);
