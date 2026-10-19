// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_step.c: a session's steps, the step from the tree's Python into its own C and back out, lldb's own commands
// and orior's script in lldb, and a stop where the bytes of a name change
#include "debug_internal.h"

// the spaces a text is trimmed of
static const char s_spaces[] = " \t\n\r\f\v";

int cli_debug_lldb_command(CliDebugSession *session, const char *text, CliLine *out, CliLine *said)
{
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    char expression[CLI_PATH_LONGEST * 2u];
    CliLine command = {expression, sizeof(expression), 0ull};
    cli_line_character(&command, '`');
    cli_line_text(&command, text);
    cli_line_text(&answer->asked, "{\"expression\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)command.out, command.at);
    cli_line_text(&answer->asked, ",\"context\":\"repl\"}");
    if (!cli_debug_asked(session, "evaluate", answer, CLI_DEBUG_ASKING, said))
    {
        cli_debug_answer_given(answer);
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const result =
        cli_json_string(&answer->json, cli_json_member(&answer->json, answer->root, "result"), &length);
    const char *const text_said = (result != NULL) ? result : "";
    const unsigned long long size = (result != NULL) ? length : 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = size;
    while ((start < size) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text_said[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text_said[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    const int failed = (end - start >= 6ull) &&
                       (memor.cmp(&(MemoriaCfg){.src = text_said + start, .other = "error:", .bytes = 6u}) == 0);
    if (failed)
    {
        // lldb says a command failed in the answer's text
        cli_lossy(said, (const unsigned char *)text_said + start, end - start);
    }
    else
    {
        cli_lossy(out, (const unsigned char *)text_said, size);
    }
    cli_debug_answer_given(answer);
    return !failed;
}

int cli_debug_lldb_script(CliDebugSession *session, CliLine *said)
{
    cli_lock_take(&g_cli_debug.lock);
    const int scripted = session->scripted;
    cli_lock_give(&g_cli_debug.lock);
    if (scripted)
    {
        return 1;
    }
    char temporary[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    cli_temp_folder(temporary);
    cli_full(temporary, "orior-lldb", folder);
    cli_full(folder, "orior_lldb.py", script);
    if (!query_folder_made(folder) ||
        !query_file_written(script, g_cli_debug_lldb_script,
                            cellul.len(&(CatenaFinitaCfg){.src = g_cli_debug_lldb_script, .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, "orior's script for lldb could not be written at ");
        cli_line_text(said, script);
        return 0;
    }
    char command[CLI_PATH_LONGEST + 64u];
    CliLine line = {command, sizeof(command), 0ull};
    cli_line_text(&line, "command script import \"");
    for (const char *letter = script; *letter != '\0'; letter += 1)
    {
        cli_line_character(&line, (*letter == '\\') ? '/' : *letter);
    }
    cli_line_character(&line, '"');
    cli_line_closed(&line);
    char heard[CLI_SAID_LONGEST];
    CliLine out = {heard, sizeof(heard), 0ull};
    if (!cli_debug_lldb_command(session, command, &out, said))
    {
        return 0;
    }
    cli_lock_take(&g_cli_debug.lock);
    session->scripted = 1;
    cli_lock_give(&g_cli_debug.lock);
    return 1;
}

// whether the native session taken steps out into the Python that called it: its caller is a frame lldb names by its
// module and symbol, which is no file
static int step_out_to_python(CliDebugSession *session, long long thread, CliLine *said)
{
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    cli_line_text(&answer->asked, "{\"threadId\":");
    cli_line_signed(&answer->asked, thread);
    cli_line_text(&answer->asked, ",\"startFrame\":0,\"levels\":200}");
    if (!cli_debug_asked(session, "stackTrace", answer, CLI_DEBUG_ASKING, said))
    {
        cli_debug_answer_given(answer);
        return -1;
    }
    const CliJson *const told = &answer->json;
    const unsigned int caller = cli_json_item(told, cli_json_member(told, answer->root, "stackFrames"), 1u);
    const char *const path =
        cli_json_string(told, cli_json_member(told, cli_json_member(told, caller, "source"), "path"), NULL);
    const int out = (caller == CLI_JSON_NONE) || (path == NULL) || !query_is_file(path);
    cli_debug_answer_given(answer);
    return out;
}

// `how` sent to the session taken for `thread`, waited on where `wait`: 1, or 0 with why said
static int step_sent(CliDebugSession *session, const char *how, long long thread, int wait, CliLine *said)
{
    char arguments[64];
    CliLine asked = {arguments, sizeof(arguments), 0ull};
    cli_line_text(&asked, "{\"threadId\":");
    cli_line_signed(&asked, thread);
    cli_line_character(&asked, '}');
    if (wait)
    {
        char body[1024];
        CliLine answer = {body, sizeof(body), 0ull};
        return cli_dap_requested(session->dap, how, asked.out, asked.at, CLI_DEBUG_ASKING, &answer, said);
    }
    unsigned long long seq = 0ull;
    if (!cli_dap_sent(session->dap, how, asked.out, asked.at, &seq, said))
    {
        return 0;
    }
    cli_dap_forgotten(session->dap, seq);
    return 1;
}

int cli_debug_step(unsigned long long id, const char *how, long long thread, CliLine *out, CliLine *said)
{
    static const char *const s_hows[7] = {"continue", "next",     "stepIn",         "stepOut",
                                          "pause",    "stepBack", "reverseContinue"};
    int known = 0;
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        known = known || (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = s_hows[at], .cap = CLI_CELLS_POOL}));
    }
    if (!known)
    {
        cli_line_text(said, how);
        cli_line_text(said, " is not a step");
        return 0;
    }
    CliDebugSession *const session = cli_debug_taken(id, said);
    if (session == NULL)
    {
        return 0;
    }
    cli_lock_take(&g_cli_debug.lock);
    const int has_parent = session->info.has_parent;
    const unsigned long long parent = session->info.parent;
    cli_lock_give(&g_cli_debug.lock);
    // a step out of the tree's C into the Python that called it lets the native code run on and stops the Python at
    // the next line it runs
    CliDebugSession *const sibling =
        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "stepOut", .cap = CLI_CELLS_POOL})) && has_parent
            ? cli_debug_native_of(parent)
            : NULL;
    const int across = (sibling != NULL) && (sibling == session);
    if (sibling != NULL)
    {
        cli_debug_let_go(sibling);
    }
    if (across)
    {
        const int leaves = step_out_to_python(session, thread, said);
        if (leaves != 0)
        {
            CliDebugSession *const python = (leaves > 0) ? cli_debug_taken(parent, said) : NULL;
            // debugpy pauses every thread, whichever is named
            const int stepped = (python != NULL) && step_sent(session, "continue", thread, 1, said) &&
                                step_sent(python, "pause", thread, 0, said);
            if (python != NULL)
            {
                cli_debug_let_go(python);
            }
            cli_debug_let_go(session);
            cli_line_text(out, stepped ? "null" : "");
            return stepped;
        }
    }
    // the C a Python step runs can stop the whole process at a native breakpoint before the Python's debugger, which
    // answers from inside the process, says the step began; in a session whose process a native session also debugs,
    // the step is sent and not waited on
    CliDebugSession *const native =
        (!cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "pause", .cap = CLI_CELLS_POOL})) ? cli_debug_native_of(id)
                                                                                              : NULL;
    const int stepped = step_sent(session, how, thread, native == NULL, said);
    if (native != NULL)
    {
        cli_debug_let_go(native);
    }
    cli_debug_let_go(session);
    if (stepped)
    {
        cli_line_text(out, "null");
    }
    return stepped;
}

int cli_debug_step_across(const char *root, unsigned long long id, long long thread, CliLine *out, CliLine *said)
{
    CliDebugSession *const python = cli_debug_taken(id, said);
    if (python == NULL)
    {
        return 0;
    }
    CliDebugSession *const native = cli_debug_native_of(id);
    if (native == NULL)
    {
        cli_debug_let_go(python);
        return cli_debug_step(id, "stepIn", thread, out, said);
    }
    char command[CLI_PATH_LONGEST * 2u];
    CliLine line = {command, sizeof(command), 0ull};
    char slashed[CLI_PATH_LONGEST];
    CliLine slashed_line = {slashed, sizeof(slashed), 0ull};
    for (const char *letter = root; *letter != '\0'; letter += 1)
    {
        cli_line_character(&slashed_line, (*letter == '\\') ? '/' : *letter);
    }
    cli_line_text(&line, "script print(orior_lldb.across(lldb.debugger.GetSelectedTarget(), ");
    cli_json_quoted_lossy(&line, (const unsigned char *)slashed_line.out, slashed_line.at);
    cli_line_text(&line, ", \"" CLI_DEBUG_ACROSS "\"))");
    cli_line_closed(&line);
    char heard[CLI_SAID_LONGEST];
    CliLine count = {heard, sizeof(heard), 0ull};
    if (!cli_debug_lldb_script(native, said) || !cli_debug_lldb_command(native, command, &count, said))
    {
        cli_debug_let_go(native);
        cli_debug_let_go(python);
        return 0;
    }
    // the count of places the script set a breakpoint at, the whole answer trimmed, 0 where it is no number
    unsigned long long start = 0ull;
    unsigned long long end = count.at;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = count.out[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = count.out[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    unsigned long long placed = 0ull;
    int number = (end > start) && (end - start <= 19ull);
    for (unsigned long long at = start; number && (at < end); at += 1ull)
    {
        number = (count.out[at] >= '0') && (count.out[at] <= '9');
        placed = (placed * 10ull) + (unsigned long long)(count.out[at] - '0');
    }
    placed = number ? placed : 0ull;
    if (placed > 0ull)
    {
        cli_lock_take(&g_cli_debug.lock);
        python->has_across = 1;
        python->across = native->info.id;
        native->has_across = 1;
        native->across = native->info.id;
        cli_lock_give(&g_cli_debug.lock);
    }
    cli_debug_let_go(native);
    cli_debug_let_go(python);
    return cli_debug_step(id, "stepIn", thread, out, said);
}
