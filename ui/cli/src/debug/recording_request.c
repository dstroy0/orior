// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_request.c: the adapter that records a Python program's run and steps through it, served a request at a
// time: the capabilities, the launch, the breakpoints, the run started as configuration is done, the recording taken as
// the run ends, and the run ended where the client lets the adapter go. One replay is open at a time: an adapter begun
// lets go of the replay before it and ends the run that replay was recording
#include "../command_line/cells.h"
#include "../text/text.h"
#include "recording_internal.h"

// the bytes of a command's name and of why a request failed, and the most bytes of an answer's body
#define CLI_RECORDING_COMMAND 256u
#define CLI_RECORDING_ERR_MSG_MAX_LEN 4096u
#define CLI_RECORDING_BODY (8ull << 20u)

// the request served, read into a tree taken to its size and given back as it is answered; and the replies sent
static CliJson s_json;
static unsigned long long s_seq;

// a node of the request written as it came, null where it is none
static void request_raw(VerbaLinea *out, unsigned int node)
{
    if (node == CLI_JSON_NONE)
    {
        mmgr_line_put(out, "null");
    }
    else
    {
        cli_json_write(&s_json, node, out, 0u, 0);
    }
}

// the reply to request `message` sent, written into a cell of the most it holds: the body the line `body` holds where
// it succeeded, why it failed where it did not
static void request_answered(CliDap *dap, unsigned int message, const char *command, int success,
                             const VerbaLinea *body, const VerbaLinea *err_msg)
{
    s_seq += 1ull;
    const int body_whole = mmgr_line_ok(body);
    const unsigned long long command_size = cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL});
    // the members around the body, the body or null, and the command and the reason with every byte escaped
    const unsigned long long room = (body_whole ? body->at : 4ull) + ((command_size + err_msg->at) * 6ull) + 512ull;
    char *const sent = g_cli_cells->temporary_buf_alloc(room, &g_cli_debug_cells);
    VerbaLinea reply = mmgr_line_over(sent, room);
    mmgr_line_put(&reply, "{\"seq\":");
    mmgr_line_decimal(&reply, s_seq, 1u);
    mmgr_line_put(&reply, ",\"type\":\"response\",\"request_seq\":");
    request_raw(&reply, cli_json_member(&s_json, message, "seq"));
    mmgr_line_put(&reply, ",\"command\":");
    cli_json_quoted_lossy(&reply, (const unsigned char *)command, command_size);
    mmgr_line_put(&reply, success ? ",\"success\":true,\"body\":" : ",\"success\":false,\"message\":");
    if (success && body_whole)
    {
        (void)mmgr_line_put_n(&reply, body->out, body->at);
    }
    else if (success)
    {
        mmgr_line_put(&reply, "null");
    }
    else
    {
        cli_json_quoted_lossy(&reply, (const unsigned char *)err_msg->out, err_msg->at);
    }
    mmgr_line_ch(&reply, '}');
    if (mmgr_line_ok(&reply))
    {
        (void)cli_dap_send(dap, reply.out, reply.at);
    }
    g_cli_cells->persistent_buf_release(sent, &g_cli_debug_cells);
}

// the breakpoints of one file set, each line once, its place among the files the one its path's key had, or a new
// one; and each reported as placed and verified at the line the client requested
static void request_breakpoints(unsigned int arguments, VerbaLinea *body)
{
    const unsigned int source = cli_json_member(&s_json, arguments, "source");
    const char *const path = cli_json_string(&s_json, cli_json_member(&s_json, source, "path"), NULL);
    char key[CLI_PATH_MAX_LEN];
    cli_replay_key((path != NULL) ? path : "", key);
    unsigned int file = 0u;
    while (
        (file < g_cli_replay->break_file_count) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_replay->break_files[file], .other = key, .cap = CLI_CELLS_POOL})))
    {
        file += 1u;
    }
    const int fits = (file < CLI_RECORDING_BREAK_FILES);
    if (fits && (file == g_cli_replay->break_file_count))
    {
        memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->break_files[file],
                                .src = key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
        g_cli_replay->break_file_count += 1u;
    }
    if (fits)
    {
        g_cli_replay->break_count[file] = 0u;
    }
    const unsigned int breakpoints = cli_json_member(&s_json, arguments, "breakpoints");
    mmgr_line_put(body, "{\"breakpoints\":[");
    for (unsigned int at = 0u; cli_json_item(&s_json, breakpoints, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(&s_json, breakpoints, at);
        const unsigned int line_node = cli_json_member(&s_json, one, "line");
        unsigned long long line = 0ull;
        const int whole = cli_replay_whole(&s_json, line_node, &line);
        // a line past 32 bits retains its low 32, the width of the line numbers the recording stores
        const unsigned int narrow = (unsigned int)(line & 0xffffffffull);
        unsigned int count = 0u;
        while (fits && whole && (count < g_cli_replay->break_count[file]) &&
               (g_cli_replay->break_lines[file][count] != narrow))
        {
            count += 1u;
        }
        if (fits && whole && (count == g_cli_replay->break_count[file]) && (count < CLI_RECORDING_BREAK_LINES))
        {
            g_cli_replay->break_lines[file][count] = narrow;
            g_cli_replay->break_count[file] += 1u;
        }
        mmgr_line_put(body, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        mmgr_line_decimal(body, (unsigned long long)at + 1ull, 1u);
        mmgr_line_put(body, ",\"verified\":true,\"line\":");
        request_raw(body, line_node);
        mmgr_line_ch(body, '}');
    }
    mmgr_line_put(body, "]}");
}

// whether the exception filters requested take in the raised one
static int request_on_raised(unsigned int arguments)
{
    const unsigned int filters = cli_json_member(&s_json, arguments, "filters");
    for (unsigned int at = 0u; cli_json_item(&s_json, filters, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const filter = cli_json_string(&s_json, cli_json_item(&s_json, filters, at), NULL);
        if ((filter != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = filter, .other = "raised", .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

int cli_replay_locked(void)
{
    if (g_cli_replay == NULL)
    {
        g_cli_replay = g_cli_cells->persistent_buf_alloc(sizeof(CliReplay), &g_cli_debug_cells);
        if (g_cli_replay != NULL)
        {
            memor.set(&(MemoriaCfg){.dst = g_cli_replay, .val = 0, .bytes = sizeof(CliReplay)});
            cli_recording_let_go();
        }
    }
    return g_cli_replay != NULL;
}

void *cli_recording_begun(const char *root)
{
    cli_recording_run_ended();
    cli_recording_let_go();
    if (!cli_replay_locked())
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    g_cli_replay->program[0] = '\0';
    g_cli_replay->python[0] = '\0';
    g_cli_replay->at = 0u;
    g_cli_replay->break_file_count = 0u;
    g_cli_replay->on_raised = 0;
    s_seq = 0ull;
    return g_cli_replay;
}

int cli_recording_serve(void *context, CliDap *dap, const char *request, unsigned long long size)
{
    (void)context;
    char why[CLI_RECORDING_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    // the request before this one given back with its tree, and this one read into a tree of its size
    cli_json_given(&s_json);
    const unsigned int message = cli_json_taken(&s_json, request, size, 0u, 0ull, &err_msg, &g_cli_debug_cells);
    if (message == CLI_JSON_NONE)
    {
        return 1;
    }
    const char *const type = cli_json_string(&s_json, cli_json_member(&s_json, message, "type"), NULL);
    if ((type != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "orior", .cap = CLI_CELLS_POOL})))
    {
        cli_recording_recorded(dap, &s_json, message);
        return 1;
    }
    char command[CLI_RECORDING_COMMAND];
    const char *const named = cli_json_string(&s_json, cli_json_member(&s_json, message, "command"), NULL);
    cli_text_copied_or(command, named, "");
    const unsigned int arguments = cli_json_member(&s_json, message, "arguments");
    // the answer's body in a cell of the most it holds, given back once the reply is sent
    char *const body_text = g_cli_cells->temporary_buf_alloc(CLI_RECORDING_BODY, &g_cli_debug_cells);
    VerbaLinea body = mmgr_line_over(body_text, CLI_RECORDING_BODY);
    int success = 1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "initialize", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(&body, "{\"supportsConfigurationDoneRequest\":true,\"supportsStepBack\":true,"
                             "\"supportsExceptionInfoRequest\":true,\"supportsEvaluateForHovers\":true,"
                             "\"exceptionBreakpointFilters\":[{\"filter\":\"raised\",\"label\":\"Raised "
                             "Exceptions\",\"default\":false}]}");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "launch", .cap = CLI_CELLS_POOL}))
    {
        const char *const program = cli_json_string(&s_json, cli_json_member(&s_json, arguments, "program"), NULL);
        const char *const python = cli_json_string(&s_json, cli_json_member(&s_json, arguments, "python"), NULL);
        cli_text_copied_or(g_cli_replay->program, program, "");
        cli_text_copied_or(g_cli_replay->python, python, "");
        mmgr_line_put(&body, "null");
        request_answered(dap, message, command, 1, &body, &err_msg);
        g_cli_cells->persistent_buf_release(body_text, &g_cli_debug_cells);
        const char initialized[] = "{\"type\":\"event\",\"event\":\"initialized\"}";
        (void)cli_dap_send(dap, initialized, sizeof(initialized) - 1u);
        return 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "setBreakpoints", .cap = CLI_CELLS_POOL}))
    {
        request_breakpoints(arguments, &body);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "setExceptionBreakpoints", .cap = CLI_CELLS_POOL}))
    {
        g_cli_replay->on_raised = request_on_raised(arguments);
        mmgr_line_put(&body, "null");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "configurationDone", .cap = CLI_CELLS_POOL}))
    {
        success = cli_recording_run(dap, &err_msg);
        mmgr_line_put(&body, "null");
        request_answered(dap, message, command, success, &body, &err_msg);
        g_cli_cells->persistent_buf_release(body_text, &g_cli_debug_cells);
        if (!success)
        {
            const char terminated[] = "{\"type\":\"event\",\"event\":\"terminated\",\"body\":{}}";
            (void)cli_dap_send(dap, terminated, sizeof(terminated) - 1u);
        }
        return 1;
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disconnect", .cap = CLI_CELLS_POOL})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "terminate", .cap = CLI_CELLS_POOL})))
    {
        cli_recording_run_killed();
        mmgr_line_put(&body, "null");
        request_answered(dap, message, command, 1, &body, &err_msg);
        g_cli_cells->persistent_buf_release(body_text, &g_cli_debug_cells);
        return !cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disconnect", .cap = CLI_CELLS_POOL});
    }
    else
    {
        success = cli_replay_answer(dap, command, &s_json, arguments, &body, &err_msg);
    }
    request_answered(dap, message, command, success, &body, &err_msg);
    g_cli_cells->persistent_buf_release(body_text, &g_cli_debug_cells);
    return 1;
}
