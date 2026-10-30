// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_requests.c: what a session is queried about its stopped program: the exceptions it stops on and the one it
// stopped on, its threads and their stacks, the scopes and variables of a frame, what an expression comes to, a
// recorded run's history of a name and a step gone to, and the memory and the instructions at an address
#include "debug_internal.h"

// session `id` taken and a request's room with it: the room, or NULL with why given
static CliDebugAnswer *requests_begun(unsigned long long id, CliDebugSession **session, VerbaLinea *err_msg)
{
    *session = cli_debug_taken(id, err_msg);
    return (*session != NULL) ? cli_debug_answer_taken() : NULL;
}

// the request sent to the session taken and the session let go where it failed: 1, or 0 with why given
static int requests_send(CliDebugSession *session, const char *command, CliDebugAnswer *answer, VerbaLinea *err_msg)
{
    if (!cli_debug_request(session, command, answer, CLI_DEBUG_REQUEST_TIMEOUT_MS, err_msg))
    {
        cli_debug_answer_given(answer);
        cli_debug_let_go(session);
        return 0;
    }
    return 1;
}

// the request's room and the session let go
static int requests_done(CliDebugSession *session, CliDebugAnswer *answer)
{
    cli_debug_answer_given(answer);
    cli_debug_let_go(session);
    return 1;
}

// a string member of a node written as JSON, `fallback` where it is none, or null where `fallback` is NULL
static void requests_text(const CliJson *json, unsigned int node, const char *name, const char *fallback,
                          VerbaLinea *out)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, cli_json_member(json, node, name), &length);
    if ((text == NULL) && (fallback == NULL))
    {
        mmgr_line_put(out, "null");
        return;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)((text != NULL) ? text : fallback),
                          (text != NULL) ? length
                                         : cellul.len(&(CatenaFinitaCfg){.src = fallback, .cap = CLI_CELLS_POOL}));
}

// a signed member of a node written, 0 where it is none
static void requests_signed(const CliJson *json, unsigned int node, const char *name, VerbaLinea *out)
{
    long long value = 0ll;
    (void)cli_debug_signed(json, cli_json_member(json, node, name), &value);
    mmgr_line_decimal_signed(out, value);
}

// a line counted from 1 written counted from 0, `fallback` where it is none, its low 32 bits retained as the window's
static void requests_line(const CliJson *json, unsigned int node, const char *name, unsigned long long fallback,
                          VerbaLinea *out)
{
    unsigned long long line = fallback;
    (void)cli_debug_unsigned(json, cli_json_member(json, node, name), &line);
    mmgr_line_decimal(out, ((line > 0ull) ? line - 1ull : 0ull) & 0xffffffffull, 1u);
}

// a path written by `named`, or as it came where none is given
static void requests_path(CliDebugPathWritten named, void *context, const char *path, VerbaLinea *out)
{
    if (named != NULL)
    {
        named(context, path, out);
        return;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
}

// a variable an adapter wrote, its name `name` where that is given
static void requests_variable(const CliJson *json, unsigned int one, const char *name, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"name\":");
    if (name != NULL)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        requests_text(json, one, "name", "", out);
    }
    mmgr_line_put(out, ",\"value\":");
    const int has_value = (cli_json_string(json, cli_json_member(json, one, "value"), NULL) != NULL);
    requests_text(json, one, has_value ? "value" : "result", "", out);
    mmgr_line_put(out, ",\"kind\":");
    requests_text(json, one, "type", "", out);
    mmgr_line_put(out, ",\"reference\":");
    requests_signed(json, one, "variablesReference", out);
    mmgr_line_put(out, ",\"memory\":");
    requests_text(json, one, "memoryReference", NULL, out);
    mmgr_line_ch(out, '}');
}

int cli_debug_exceptions(unsigned long long id, const CliJson *json, unsigned int on, VerbaLinea *out,
                         VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"filters\":[");
    for (unsigned int at = 0u; cli_json_item(json, on, at) != CLI_JSON_NONE; at += 1u)
    {
        mmgr_line_put(&answer->args, (at > 0u) ? "," : "");
        cli_json_write(json, cli_json_item(json, on, at), &answer->args, 0u, 0);
    }
    mmgr_line_put(&answer->args, "]}");
    if (!requests_send(session, "setExceptionBreakpoints", answer, err_msg))
    {
        return 0;
    }
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < session->info.filter_count; at += 1u)
    {
        CliDebugFilter *const filter = &session->info.filters[at];
        filter->on = 0;
        for (unsigned int one = 0u; cli_json_item(json, on, one) != CLI_JSON_NONE; one += 1u)
        {
            const char *const named = cli_json_string(json, cli_json_item(json, on, one), NULL);
            filter->on =
                filter->on ||
                ((named != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = filter->filter, .cap = CLI_CELLS_POOL})));
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    mmgr_line_put(out, "null");
    return requests_done(session, answer);
}

int cli_debug_raised(unsigned long long id, long long thread, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"threadId\":");
    mmgr_line_decimal_signed(&answer->args, thread);
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "exceptionInfo", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    mmgr_line_put(out, "{\"id\":");
    requests_text(reply, answer->root, "exceptionId", "", out);
    mmgr_line_put(out, ",\"description\":");
    requests_text(reply, answer->root, "description", "", out);
    mmgr_line_put(out, ",\"stack\":");
    requests_text(reply, cli_json_member(reply, answer->root, "details"), "stackTrace", "", out);
    mmgr_line_ch(out, '}');
    return requests_done(session, answer);
}

int cli_debug_threads(unsigned long long id, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if ((answer == NULL) || !requests_send(session, "threads", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int threads = cli_json_member(reply, answer->root, "threads");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(reply, threads, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(reply, threads, at);
        mmgr_line_put(out, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        requests_signed(reply, one, "id", out);
        mmgr_line_put(out, ",\"name\":");
        requests_text(reply, one, "name", "", out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
    return requests_done(session, answer);
}

int cli_debug_stack(unsigned long long id, long long thread, CliDebugPathWritten named, void *context, VerbaLinea *out,
                    VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"threadId\":");
    mmgr_line_decimal_signed(&answer->args, thread);
    mmgr_line_put(&answer->args, ",\"startFrame\":0,\"levels\":200}");
    if (!requests_send(session, "stackTrace", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int frames = cli_json_member(reply, answer->root, "stackFrames");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(reply, frames, at) != CLI_JSON_NONE; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_debug_frame_written(reply, cli_json_item(reply, frames, at), named, context, out);
    }
    mmgr_line_ch(out, ']');
    return requests_done(session, answer);
}

void cli_debug_frame_written(const CliJson *json, unsigned int frame, CliDebugPathWritten named, void *context,
                             VerbaLinea *out)
{
    const char *const path =
        cli_json_string(json, cli_json_member(json, cli_json_member(json, frame, "source"), "path"), NULL);
    mmgr_line_put(out, "{\"id\":");
    requests_signed(json, frame, "id", out);
    mmgr_line_put(out, ",\"name\":");
    requests_text(json, frame, "name", "", out);
    mmgr_line_put(out, ",\"path\":");
    if (path != NULL)
    {
        requests_path(named, context, path, out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"line\":");
    requests_line(json, frame, "line", 1ull, out);
    mmgr_line_put(out, ",\"col\":");
    requests_line(json, frame, "column", 1ull, out);
    mmgr_line_put(out, ",\"ip\":");
    requests_text(json, frame, "instructionPointerReference", NULL, out);
    mmgr_line_ch(out, '}');
}

int cli_debug_scopes(unsigned long long id, long long frame, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"frameId\":");
    mmgr_line_decimal_signed(&answer->args, frame);
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "scopes", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int scopes = cli_json_member(reply, answer->root, "scopes");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(reply, scopes, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(reply, scopes, at);
        mmgr_line_put(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        requests_text(reply, one, "name", "", out);
        mmgr_line_put(out, ",\"reference\":");
        requests_signed(reply, one, "variablesReference", out);
        mmgr_line_put(out, cli_debug_true(reply, cli_json_member(reply, one, "expensive")) ? ",\"expensive\":true}"
                                                                                           : ",\"expensive\":false}");
    }
    mmgr_line_ch(out, ']');
    return requests_done(session, answer);
}

int cli_debug_variables(unsigned long long id, long long reference, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"variablesReference\":");
    mmgr_line_decimal_signed(&answer->args, reference);
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "variables", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int variables = cli_json_member(reply, answer->root, "variables");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(reply, variables, at) != CLI_JSON_NONE; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        requests_variable(reply, cli_json_item(reply, variables, at), NULL, out);
    }
    mmgr_line_ch(out, ']');
    return requests_done(session, answer);
}

int cli_debug_evaluate(unsigned long long id, const char *expression, int has_frame, long long frame,
                       const char *context, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    // with no frame chosen the frame is left out, which an adapter takes as its global scope
    mmgr_line_put(&answer->args, "{\"expression\":");
    cli_json_quoted_lossy(&answer->args, (const unsigned char *)expression,
                          cellul.len(&(CatenaFinitaCfg){.src = expression, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&answer->args, ",\"context\":");
    cli_json_quoted_lossy(&answer->args, (const unsigned char *)context,
                          cellul.len(&(CatenaFinitaCfg){.src = context, .cap = CLI_CELLS_POOL}));
    if (has_frame)
    {
        mmgr_line_put(&answer->args, ",\"frameId\":");
        mmgr_line_decimal_signed(&answer->args, frame);
    }
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "evaluate", answer, err_msg))
    {
        return 0;
    }
    requests_variable(&answer->json, answer->root, expression, out);
    return requests_done(session, answer);
}

int cli_debug_history(unsigned long long id, long long frame, const char *name, CliDebugPathWritten named,
                      void *context, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"frameId\":");
    mmgr_line_decimal_signed(&answer->args, frame);
    mmgr_line_put(&answer->args, ",\"name\":");
    cli_json_quoted_lossy(&answer->args, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "orior/history", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int changes = cli_json_member(reply, answer->root, "changes");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(reply, changes, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(reply, changes, at);
        unsigned long long step = 0ull;
        (void)cli_debug_unsigned(reply, cli_json_member(reply, one, "step"), &step);
        mmgr_line_put(out, (at > 0u) ? ",{\"step\":" : "{\"step\":");
        mmgr_line_decimal(out, step, 1u);
        mmgr_line_put(out, ",\"line\":");
        requests_line(reply, one, "line", 1ull, out);
        mmgr_line_put(out, ",\"path\":");
        requests_path(named, context, cli_debug_text(reply, cli_json_member(reply, one, "path"), ""), out);
        mmgr_line_put(out, ",\"value\":");
        requests_text(reply, one, "value", NULL, out);
        mmgr_line_put(out, cli_debug_true(reply, cli_json_member(reply, one, "before")) ? ",\"before\":true}"
                                                                                        : ",\"before\":false}");
    }
    mmgr_line_ch(out, ']');
    return requests_done(session, answer);
}

int cli_debug_goto(unsigned long long id, unsigned long long step, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"step\":");
    mmgr_line_decimal(&answer->args, step, 1u);
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "orior/goto", answer, err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, "null");
    return requests_done(session, answer);
}

int cli_debug_memory(unsigned long long id, const char *reference, long long offset, unsigned long long count,
                     VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"memoryReference\":");
    cli_json_quoted_lossy(&answer->args, (const unsigned char *)reference,
                          cellul.len(&(CatenaFinitaCfg){.src = reference, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&answer->args, ",\"offset\":");
    mmgr_line_decimal_signed(&answer->args, offset);
    mmgr_line_put(&answer->args, ",\"count\":");
    mmgr_line_decimal(&answer->args, count, 1u);
    mmgr_line_ch(&answer->args, '}');
    if (!requests_send(session, "readMemory", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    unsigned long long length = 0ull;
    const char *const data = cli_json_string(reply, cli_json_member(reply, answer->root, "data"), &length);
    unsigned long long unreadable = 0ull;
    (void)cli_debug_unsigned(reply, cli_json_member(reply, answer->root, "unreadableBytes"), &unreadable);
    mmgr_line_put(out, "{\"address\":");
    requests_text(reply, answer->root, "address", reference, out);
    mmgr_line_put(out, ",\"bytes\":[");
    // the bytes read from memory, in a cell of the most the base64 text decodes to
    const unsigned long long most = ((length / 4ull) + 1ull) * 3ull;
    unsigned char *const decoded = (data != NULL) ? g_cli_cells->temporary_buf_alloc(most, &g_cli_debug_cells) : NULL;
    const unsigned long long bytes = (decoded != NULL) ? cli_debug_base64(data, length, decoded, most) : 0ull;
    for (unsigned long long at = 0ull; at < bytes; at += 1ull)
    {
        mmgr_line_put(out, (at > 0ull) ? "," : "");
        mmgr_line_decimal(out, decoded[at], 1u);
    }
    g_cli_cells->persistent_buf_release(decoded, &g_cli_debug_cells);
    mmgr_line_put(out, "],\"unreadable\":");
    mmgr_line_decimal(out, unreadable, 1u);
    mmgr_line_ch(out, '}');
    return requests_done(session, answer);
}

int cli_debug_instructions(unsigned long long id, const char *reference, long long offset, long long count,
                           VerbaLinea *out, VerbaLinea *err_msg)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args, "{\"memoryReference\":");
    cli_json_quoted_lossy(&answer->args, (const unsigned char *)reference,
                          cellul.len(&(CatenaFinitaCfg){.src = reference, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&answer->args, ",\"instructionOffset\":");
    mmgr_line_decimal_signed(&answer->args, offset);
    mmgr_line_put(&answer->args, ",\"instructionCount\":");
    mmgr_line_decimal_signed(&answer->args, count);
    mmgr_line_put(&answer->args, ",\"resolveSymbols\":true}");
    if (!requests_send(session, "disassemble", answer, err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int instructions = cli_json_member(reply, answer->root, "instructions");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(reply, instructions, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(reply, instructions, at);
        unsigned long long line = 0ull;
        mmgr_line_put(out, (at > 0u) ? ",{\"address\":" : "{\"address\":");
        requests_text(reply, one, "address", "", out);
        mmgr_line_put(out, ",\"text\":");
        requests_text(reply, one, "instruction", "", out);
        mmgr_line_put(out, ",\"bytes\":");
        requests_text(reply, one, "instructionBytes", "", out);
        mmgr_line_put(out, ",\"symbol\":");
        requests_text(reply, one, "symbol", "", out);
        mmgr_line_put(out, ",\"line\":");
        if (cli_debug_unsigned(reply, cli_json_member(reply, one, "line"), &line))
        {
            mmgr_line_decimal(out, ((line > 0ull) ? line - 1ull : 0ull) & 0xffffffffull, 1u);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
    return requests_done(session, answer);
}
