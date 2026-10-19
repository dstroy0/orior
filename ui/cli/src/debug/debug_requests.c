// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_requests.c: what a session is asked about its stopped program: the exceptions it stops on and the one it
// stopped on, its threads and their stacks, the scopes and variables of a frame, what an expression comes to, a
// recorded run's history of a name and a step gone to, and the memory and the instructions at an address
#include "debug_internal.h"

// session `id` taken and a request's room with it: the room, or NULL with why said
static CliDebugAnswer *requests_begun(unsigned long long id, CliDebugSession **session, CliLine *said)
{
    *session = cli_debug_taken(id, said);
    return (*session != NULL) ? cli_debug_answer_taken() : NULL;
}

// the request asked of the session taken and the session let go where it failed: 1, or 0 with why said
static int requests_asked(CliDebugSession *session, const char *command, CliDebugAnswer *answer, CliLine *said)
{
    if (!cli_debug_asked(session, command, answer, CLI_DEBUG_ASKING, said))
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
static void requests_text(const CliJson *json, unsigned int node, const char *name, const char *fallback, CliLine *out)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, cli_json_member(json, node, name), &length);
    if ((text == NULL) && (fallback == NULL))
    {
        cli_line_text(out, "null");
        return;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)((text != NULL) ? text : fallback),
                          (text != NULL) ? length
                                         : cellul.len(&(CatenaFinitaCfg){.src = fallback, .cap = CLI_CELLS_POOL}));
}

// a signed member of a node written, 0 where it is none
static void requests_signed(const CliJson *json, unsigned int node, const char *name, CliLine *out)
{
    long long value = 0ll;
    (void)cli_debug_signed(json, cli_json_member(json, node, name), &value);
    cli_line_signed(out, value);
}

// a line counted from 1 written counted from 0, `fallback` where it is none, its low 32 bits kept as the window's
static void requests_line(const CliJson *json, unsigned int node, const char *name, unsigned long long fallback,
                          CliLine *out)
{
    unsigned long long line = fallback;
    (void)cli_debug_unsigned(json, cli_json_member(json, node, name), &line);
    cli_line_decimal(out, ((line > 0ull) ? line - 1ull : 0ull) & 0xffffffffull, 1u);
}

// a path written by `named`, or as it came where none is given
static void requests_path(CliDebugPathWritten named, void *context, const char *path, CliLine *out)
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
static void requests_variable(const CliJson *json, unsigned int one, const char *name, CliLine *out)
{
    cli_line_text(out, "{\"name\":");
    if (name != NULL)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        requests_text(json, one, "name", "", out);
    }
    cli_line_text(out, ",\"value\":");
    const int has_value = (cli_json_string(json, cli_json_member(json, one, "value"), NULL) != NULL);
    requests_text(json, one, has_value ? "value" : "result", "", out);
    cli_line_text(out, ",\"kind\":");
    requests_text(json, one, "type", "", out);
    cli_line_text(out, ",\"reference\":");
    requests_signed(json, one, "variablesReference", out);
    cli_line_text(out, ",\"memory\":");
    requests_text(json, one, "memoryReference", NULL, out);
    cli_line_character(out, '}');
}

int cli_debug_exceptions(unsigned long long id, const CliJson *json, unsigned int on, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"filters\":[");
    for (unsigned int at = 0u; cli_json_item(json, on, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(&answer->asked, (at > 0u) ? "," : "");
        cli_json_write(json, cli_json_item(json, on, at), &answer->asked, 0u, 0);
    }
    cli_line_text(&answer->asked, "]}");
    if (!requests_asked(session, "setExceptionBreakpoints", answer, said))
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
    cli_line_text(out, "null");
    return requests_done(session, answer);
}

int cli_debug_raised(unsigned long long id, long long thread, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"threadId\":");
    cli_line_signed(&answer->asked, thread);
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "exceptionInfo", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    cli_line_text(out, "{\"id\":");
    requests_text(told, answer->root, "exceptionId", "", out);
    cli_line_text(out, ",\"description\":");
    requests_text(told, answer->root, "description", "", out);
    cli_line_text(out, ",\"stack\":");
    requests_text(told, cli_json_member(told, answer->root, "details"), "stackTrace", "", out);
    cli_line_character(out, '}');
    return requests_done(session, answer);
}

int cli_debug_threads(unsigned long long id, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if ((answer == NULL) || !requests_asked(session, "threads", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    const unsigned int threads = cli_json_member(told, answer->root, "threads");
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(told, threads, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(told, threads, at);
        cli_line_text(out, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        requests_signed(told, one, "id", out);
        cli_line_text(out, ",\"name\":");
        requests_text(told, one, "name", "", out);
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    return requests_done(session, answer);
}

int cli_debug_stack(unsigned long long id, long long thread, CliDebugPathWritten named, void *context, CliLine *out,
                    CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"threadId\":");
    cli_line_signed(&answer->asked, thread);
    cli_line_text(&answer->asked, ",\"startFrame\":0,\"levels\":200}");
    if (!requests_asked(session, "stackTrace", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    const unsigned int frames = cli_json_member(told, answer->root, "stackFrames");
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(told, frames, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_debug_frame_written(told, cli_json_item(told, frames, at), named, context, out);
    }
    cli_line_character(out, ']');
    return requests_done(session, answer);
}

void cli_debug_frame_written(const CliJson *json, unsigned int frame, CliDebugPathWritten named, void *context,
                             CliLine *out)
{
    const char *const path =
        cli_json_string(json, cli_json_member(json, cli_json_member(json, frame, "source"), "path"), NULL);
    cli_line_text(out, "{\"id\":");
    requests_signed(json, frame, "id", out);
    cli_line_text(out, ",\"name\":");
    requests_text(json, frame, "name", "", out);
    cli_line_text(out, ",\"path\":");
    if (path != NULL)
    {
        requests_path(named, context, path, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"line\":");
    requests_line(json, frame, "line", 1ull, out);
    cli_line_text(out, ",\"col\":");
    requests_line(json, frame, "column", 1ull, out);
    cli_line_text(out, ",\"ip\":");
    requests_text(json, frame, "instructionPointerReference", NULL, out);
    cli_line_character(out, '}');
}

int cli_debug_scopes(unsigned long long id, long long frame, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"frameId\":");
    cli_line_signed(&answer->asked, frame);
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "scopes", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    const unsigned int scopes = cli_json_member(told, answer->root, "scopes");
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(told, scopes, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(told, scopes, at);
        cli_line_text(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        requests_text(told, one, "name", "", out);
        cli_line_text(out, ",\"reference\":");
        requests_signed(told, one, "variablesReference", out);
        cli_line_text(out, cli_debug_true(told, cli_json_member(told, one, "expensive")) ? ",\"expensive\":true}"
                                                                                         : ",\"expensive\":false}");
    }
    cli_line_character(out, ']');
    return requests_done(session, answer);
}

int cli_debug_variables(unsigned long long id, long long reference, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"variablesReference\":");
    cli_line_signed(&answer->asked, reference);
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "variables", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    const unsigned int variables = cli_json_member(told, answer->root, "variables");
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(told, variables, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        requests_variable(told, cli_json_item(told, variables, at), NULL, out);
    }
    cli_line_character(out, ']');
    return requests_done(session, answer);
}

int cli_debug_evaluate(unsigned long long id, const char *expression, int has_frame, long long frame,
                       const char *context, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    // with no frame chosen the frame is left out, which an adapter takes as its global scope
    cli_line_text(&answer->asked, "{\"expression\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)expression,
                          cellul.len(&(CatenaFinitaCfg){.src = expression, .cap = CLI_CELLS_POOL}));
    cli_line_text(&answer->asked, ",\"context\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)context,
                          cellul.len(&(CatenaFinitaCfg){.src = context, .cap = CLI_CELLS_POOL}));
    if (has_frame)
    {
        cli_line_text(&answer->asked, ",\"frameId\":");
        cli_line_signed(&answer->asked, frame);
    }
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "evaluate", answer, said))
    {
        return 0;
    }
    requests_variable(&answer->json, answer->root, expression, out);
    return requests_done(session, answer);
}

int cli_debug_history(unsigned long long id, long long frame, const char *name, CliDebugPathWritten named,
                      void *context, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"frameId\":");
    cli_line_signed(&answer->asked, frame);
    cli_line_text(&answer->asked, ",\"name\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "orior/history", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    const unsigned int changes = cli_json_member(told, answer->root, "changes");
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(told, changes, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(told, changes, at);
        unsigned long long step = 0ull;
        (void)cli_debug_unsigned(told, cli_json_member(told, one, "step"), &step);
        cli_line_text(out, (at > 0u) ? ",{\"step\":" : "{\"step\":");
        cli_line_decimal(out, step, 1u);
        cli_line_text(out, ",\"line\":");
        requests_line(told, one, "line", 1ull, out);
        cli_line_text(out, ",\"path\":");
        requests_path(named, context, cli_debug_text(told, cli_json_member(told, one, "path"), ""), out);
        cli_line_text(out, ",\"value\":");
        requests_text(told, one, "value", NULL, out);
        cli_line_text(out, cli_debug_true(told, cli_json_member(told, one, "before")) ? ",\"before\":true}"
                                                                                      : ",\"before\":false}");
    }
    cli_line_character(out, ']');
    return requests_done(session, answer);
}

int cli_debug_goto(unsigned long long id, unsigned long long step, CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"step\":");
    cli_line_decimal(&answer->asked, step, 1u);
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "orior/goto", answer, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return requests_done(session, answer);
}

int cli_debug_memory(unsigned long long id, const char *reference, long long offset, unsigned long long count,
                     CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"memoryReference\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)reference,
                          cellul.len(&(CatenaFinitaCfg){.src = reference, .cap = CLI_CELLS_POOL}));
    cli_line_text(&answer->asked, ",\"offset\":");
    cli_line_signed(&answer->asked, offset);
    cli_line_text(&answer->asked, ",\"count\":");
    cli_line_decimal(&answer->asked, count, 1u);
    cli_line_character(&answer->asked, '}');
    if (!requests_asked(session, "readMemory", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    unsigned long long length = 0ull;
    const char *const data = cli_json_string(told, cli_json_member(told, answer->root, "data"), &length);
    unsigned long long unreadable = 0ull;
    (void)cli_debug_unsigned(told, cli_json_member(told, answer->root, "unreadableBytes"), &unreadable);
    cli_line_text(out, "{\"address\":");
    requests_text(told, answer->root, "address", reference, out);
    cli_line_text(out, ",\"bytes\":[");
    // the bytes read from memory, in a cell of the most the base64 text decodes to
    const unsigned long long most = ((length / 4ull) + 1ull) * 3ull;
    unsigned char *const decoded = (data != NULL) ? cli_cell_taken(most, "the bytes read from memory") : NULL;
    const unsigned long long bytes = (decoded != NULL) ? cli_debug_base64(data, length, decoded, most) : 0ull;
    for (unsigned long long at = 0ull; at < bytes; at += 1ull)
    {
        cli_line_text(out, (at > 0ull) ? "," : "");
        cli_line_decimal(out, decoded[at], 1u);
    }
    cli_cell_given(decoded);
    cli_line_text(out, "],\"unreadable\":");
    cli_line_decimal(out, unreadable, 1u);
    cli_line_character(out, '}');
    return requests_done(session, answer);
}

int cli_debug_instructions(unsigned long long id, const char *reference, long long offset, long long count,
                           CliLine *out, CliLine *said)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = requests_begun(id, &session, said);
    if (answer == NULL)
    {
        return 0;
    }
    cli_line_text(&answer->asked, "{\"memoryReference\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)reference,
                          cellul.len(&(CatenaFinitaCfg){.src = reference, .cap = CLI_CELLS_POOL}));
    cli_line_text(&answer->asked, ",\"instructionOffset\":");
    cli_line_signed(&answer->asked, offset);
    cli_line_text(&answer->asked, ",\"instructionCount\":");
    cli_line_signed(&answer->asked, count);
    cli_line_text(&answer->asked, ",\"resolveSymbols\":true}");
    if (!requests_asked(session, "disassemble", answer, said))
    {
        return 0;
    }
    const CliJson *const told = &answer->json;
    const unsigned int instructions = cli_json_member(told, answer->root, "instructions");
    cli_line_character(out, '[');
    for (unsigned int at = 0u; cli_json_item(told, instructions, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(told, instructions, at);
        unsigned long long line = 0ull;
        cli_line_text(out, (at > 0u) ? ",{\"address\":" : "{\"address\":");
        requests_text(told, one, "address", "", out);
        cli_line_text(out, ",\"text\":");
        requests_text(told, one, "instruction", "", out);
        cli_line_text(out, ",\"bytes\":");
        requests_text(told, one, "instructionBytes", "", out);
        cli_line_text(out, ",\"symbol\":");
        requests_text(told, one, "symbol", "", out);
        cli_line_text(out, ",\"line\":");
        if (cli_debug_unsigned(told, cli_json_member(told, one, "line"), &line))
        {
            cli_line_decimal(out, ((line > 0ull) ? line - 1ull : 0ull) & 0xffffffffull, 1u);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    return requests_done(session, answer);
}
