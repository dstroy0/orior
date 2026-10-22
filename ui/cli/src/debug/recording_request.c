// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_request.c: the adapter that records a Python program's run and steps through it, served a request at a
// time: the capabilities, the launch, the breakpoints, the run started as configuration is done, the recording taken as
// the run ends, and the run ended where the client lets the adapter go. One replay is held at a time: an adapter begun
// lets go of the replay before it and ends the run that replay was recording
#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "recording_internal.h"

// the bytes of a command's name and of why a request failed, and of each piece an answer's body and its reply are
// written in
#define CLI_RECORDING_COMMAND 256u
#define CLI_RECORDING_SAID 4096u
#define CLI_RECORDING_PIECE 4096u

// the request served, read into a tree taken to its size and given back as it is answered; and the replies sent
static CliJson s_json;
static unsigned long long s_seq;

// a node of the request written as it came, null where it is none
static void request_raw(CliLine *out, unsigned int node)
{
    if (node == CLI_JSON_NONE)
    {
        cli_line_text(out, "null");
    }
    else
    {
        cli_json_write(&s_json, node, out, 0u, 0);
    }
}

// a piece of what is written put on the line that is `context`
static void request_put(void *context, const char *bytes, unsigned long long size)
{
    cli_line_counted((CliLine *)context, bytes, size);
}

// the reply to request `message` sent, written through pieces into a cell of its size: the body its pieces hold where
// it succeeded, why it failed where it did not; the body's pieces given back
static void request_answered(CliDap *dap, unsigned int message, const char *command, int success, CliPieces *body,
                             CliLine *body_line, const CliLine *said)
{
    s_seq += 1ull;
    char piece[CLI_RECORDING_PIECE];
    CliPieces pieces;
    CliLine reply;
    cli_pieces_begun(&pieces, &reply, piece, sizeof(piece));
    cli_line_text(&reply, "{\"seq\":");
    cli_line_decimal(&reply, s_seq, 1u);
    cli_line_text(&reply, ",\"type\":\"response\",\"request_seq\":");
    request_raw(&reply, cli_json_member(&s_json, message, "seq"));
    cli_line_text(&reply, ",\"command\":");
    cli_json_quoted_lossy(&reply, (const unsigned char *)command,
                          cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}));
    cli_line_text(&reply, success ? ",\"success\":true,\"body\":" : ",\"success\":false,\"message\":");
    const int body_whole = cli_pieces_ended(body, body_line);
    if (success && body_whole)
    {
        cli_pieces_each(body, request_put, &reply);
    }
    else if (success)
    {
        cli_line_text(&reply, "null");
    }
    else
    {
        cli_json_quoted_lossy(&reply, (const unsigned char *)said->out, said->at);
    }
    cli_pieces_given(body);
    cli_line_character(&reply, '}');
    unsigned long long size = 0ull;
    char *const sent = cli_pieces_joined(&pieces, &reply, "a recording's reply", &size);
    if (sent != NULL)
    {
        (void)cli_dap_told(dap, sent, size);
    }
    cli_cell_given(sent);
}

// the breakpoints of one file set, each line once, its place among the files the one its path's key had, or a new
// one; and each told as placed and verified at the line the client asked for
static void request_breakpoints(unsigned int arguments, CliLine *body)
{
    const unsigned int source = cli_json_member(&s_json, arguments, "source");
    const char *const path = cli_json_string(&s_json, cli_json_member(&s_json, source, "path"), NULL);
    char key[CLI_PATH_LONGEST];
    cli_replay_key((path != NULL) ? path : "", key);
    unsigned int file = 0u;
    while (
        (file < g_cli_replay->break_file_count) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_replay->break_files[file], .other = key, .cap = CLI_CELLS_POOL})))
    {
        file += 1u;
    }
    const int kept = (file < CLI_RECORDING_BREAK_FILES);
    if (kept && (file == g_cli_replay->break_file_count))
    {
        memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->break_files[file],
                                .src = key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
        g_cli_replay->break_file_count += 1u;
    }
    if (kept)
    {
        g_cli_replay->break_count[file] = 0u;
    }
    const unsigned int breakpoints = cli_json_member(&s_json, arguments, "breakpoints");
    cli_line_text(body, "{\"breakpoints\":[");
    for (unsigned int at = 0u; cli_json_item(&s_json, breakpoints, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(&s_json, breakpoints, at);
        const unsigned int line_node = cli_json_member(&s_json, one, "line");
        unsigned long long line = 0ull;
        const int whole = cli_replay_whole(&s_json, line_node, &line);
        // a line past 32 bits keeps its low 32, the width of the line numbers the recording keeps
        const unsigned int narrow = (unsigned int)(line & 0xffffffffull);
        unsigned int held = 0u;
        while (kept && whole && (held < g_cli_replay->break_count[file]) &&
               (g_cli_replay->break_lines[file][held] != narrow))
        {
            held += 1u;
        }
        if (kept && whole && (held == g_cli_replay->break_count[file]) && (held < CLI_RECORDING_BREAK_LINES))
        {
            g_cli_replay->break_lines[file][held] = narrow;
            g_cli_replay->break_count[file] += 1u;
        }
        cli_line_text(body, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        cli_line_decimal(body, (unsigned long long)at + 1ull, 1u);
        cli_line_text(body, ",\"verified\":true,\"line\":");
        request_raw(body, line_node);
        cli_line_character(body, '}');
    }
    cli_line_text(body, "]}");
}

// whether the exception filters asked for take in the raised one
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

int cli_replay_held(void)
{
    if (g_cli_replay == NULL)
    {
        g_cli_replay = cli_cell_taken(sizeof(CliReplay), "a recording's replay");
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
    if (!cli_replay_held())
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
    char why[CLI_RECORDING_SAID];
    CliLine said = {why, sizeof(why), 0ull};
    // the request before this one given back with its tree, and this one read into a tree of its size
    cli_json_given(&s_json);
    const unsigned int message = cli_json_taken(&s_json, request, size, 0u, 0ull, &said);
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
    memor.cpy(&(MemoriaCfg){
        .dst = command,
        .src = (named != NULL) ? named : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (named != NULL) ? named : "", .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned int arguments = cli_json_member(&s_json, message, "arguments");
    // the answer's body held in pieces as it is written, given back as the reply is sent
    char body_piece[CLI_RECORDING_PIECE];
    CliPieces body_pieces;
    CliLine body;
    cli_pieces_begun(&body_pieces, &body, body_piece, sizeof(body_piece));
    int success = 1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "initialize", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(&body, "{\"supportsConfigurationDoneRequest\":true,\"supportsStepBack\":true,"
                             "\"supportsExceptionInfoRequest\":true,\"supportsEvaluateForHovers\":true,"
                             "\"exceptionBreakpointFilters\":[{\"filter\":\"raised\",\"label\":\"Raised "
                             "Exceptions\",\"default\":false}]}");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "launch", .cap = CLI_CELLS_POOL}))
    {
        const char *const program = cli_json_string(&s_json, cli_json_member(&s_json, arguments, "program"), NULL);
        const char *const python = cli_json_string(&s_json, cli_json_member(&s_json, arguments, "python"), NULL);
        memor.cpy(&(MemoriaCfg){
            .dst = g_cli_replay->program,
            .src = (program != NULL) ? program : "",
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = (program != NULL) ? program : "", .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = g_cli_replay->python,
            .src = (python != NULL) ? python : "",
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = (python != NULL) ? python : "", .cap = CLI_CELLS_POOL}) + 1u});
        cli_line_text(&body, "null");
        request_answered(dap, message, command, 1, &body_pieces, &body, &said);
        const char initialized[] = "{\"type\":\"event\",\"event\":\"initialized\"}";
        (void)cli_dap_told(dap, initialized, sizeof(initialized) - 1u);
        return 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "setBreakpoints", .cap = CLI_CELLS_POOL}))
    {
        request_breakpoints(arguments, &body);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "setExceptionBreakpoints", .cap = CLI_CELLS_POOL}))
    {
        g_cli_replay->on_raised = request_on_raised(arguments);
        cli_line_text(&body, "null");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "configurationDone", .cap = CLI_CELLS_POOL}))
    {
        success = cli_recording_run(dap, &said);
        cli_line_text(&body, "null");
        request_answered(dap, message, command, success, &body_pieces, &body, &said);
        if (!success)
        {
            const char terminated[] = "{\"type\":\"event\",\"event\":\"terminated\",\"body\":{}}";
            (void)cli_dap_told(dap, terminated, sizeof(terminated) - 1u);
        }
        return 1;
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disconnect", .cap = CLI_CELLS_POOL})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "terminate", .cap = CLI_CELLS_POOL})))
    {
        cli_recording_run_killed();
        cli_line_text(&body, "null");
        request_answered(dap, message, command, 1, &body_pieces, &body, &said);
        return !cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disconnect", .cap = CLI_CELLS_POOL});
    }
    else
    {
        success = cli_replay_answer(dap, command, &s_json, arguments, &body, &said);
    }
    request_answered(dap, message, command, success, &body_pieces, &body, &said);
    return 1;
}
