// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_request.c: the adapter that records a Python program's run and steps through it, served a request at a
// time: the capabilities, the launch, the breakpoints, the run started as configuration is done, the recording taken as
// the run ends, and the run ended where the client lets the adapter go. One replay is open at a time: an adapter begun
// lets go of the replay before it and ends the run that replay was recording
#include "../command_line/pool.h"
#include "../text/text.h"
#include "recording_internal.h"

// the bytes of a command's name and of why a request failed, and the most bytes of an answer's body
#define CLI_RECORDING_COMMAND 256u
#define CLI_RECORDING_ERR_MSG_MAX_LEN 4096u
#define CLI_RECORDING_BODY (8ULL << 20u)

// the request served, read into a tree taken to its size and given back as it is answered; and the replies sent
static CliJson s_json;
static unsigned long long s_seq;

typedef struct
{
    VerbaLinea *out;
    unsigned int node;
    CliDap *dap;
    unsigned int message;
    const char *command;
    int success;
    const VerbaLinea *body;
    const VerbaLinea *err_msg;
    unsigned int arguments;
    VerbaLinea *body_out;
} RecordingRequestCtx;
// a node of the request written as it came, null where it is none
EMBED_INLINE void request_raw(const RecordingRequestCtx *args)
{
    VerbaLinea *out = args->out;
    unsigned int node = args->node;
    if (node == CLI_JSON_NONE)
    {
        mmgr_line_put(out, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_json, .node = node,
                   .out = out, .indent = 0u, .sorted = 0);
    }
}

// the reply to request `message` sent, written into a cell of the most it holds: the body the line `body` holds where
// it succeeded, why it failed where it did not
EMBED_INLINE void request_answer(const RecordingRequestCtx *args)
{
    CliDap *dap = args->dap;
    unsigned int message = args->message;
    s_seq += 1ULL;
    const int body_untruncated = mmgr_line_ok(args->body);
    const unsigned long long command_size = cellul.len(&(CatenaFinitaCfg){.src = args->command, .cap = CLI_POOL_BYTES});
    // the members around the body, the body or null, and the command and the reason with every byte escaped
    const unsigned long long capacity =
        (body_untruncated ? args->body->at : 4ULL) + ((command_size + args->err_msg->at) * 6ULL) + 512ULL;
    char *const sent = g_cli_guard->temporary_buf_alloc(capacity, g_cli_debug_owner);
    VerbaLinea reply = mmgr_line_over(sent, capacity);
    mmgr_line_put(&reply, "{\"seq\":");
    mmgr_line_decimal(&reply, s_seq, 1u);
    mmgr_line_put(&reply, ",\"type\":\"response\",\"request_seq\":");
    EMBED_CALL(request_raw, RecordingRequestCtx, .out = &reply,
               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                  .object = message, .name = "seq"));
    mmgr_line_put(&reply, ",\"command\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &reply,
               .text_bytes = (const unsigned char *)args->command, .length = command_size);
    mmgr_line_put(&reply, args->success ? ",\"success\":true,\"body\":" : ",\"success\":false,\"message\":");
    if (args->success && body_untruncated)
    {
        (void)mmgr_line_put_n(&reply, args->body->out, args->body->at);
    }
    else if (args->success)
    {
        mmgr_line_put(&reply, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &reply,
                   .text_bytes = (const unsigned char *)args->err_msg->out, .length = args->err_msg->at);
    }
    mmgr_line_ch(&reply, '}');
    if (mmgr_line_ok(&reply))
    {
        (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = reply.out, .size = reply.at);
    }
    g_cli_guard->persistent_buf_release(sent, g_cli_debug_owner);
}

// the breakpoints of one file set, each line once, its place among the files the one its path's key had, or a new
// one; and each reported as placed and verified at the line the client requested
EMBED_INLINE void request_breakpoints(const RecordingRequestCtx *args)
{
    const unsigned int source = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                           .object = args->arguments, .name = "source");
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                      .object = source, .name = "path"),
                   .length_pointer = NULL);
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_recording_path_internal.replay_key, CliRecordingPathCtx, .path = (path != NULL) ? path : "",
               .key = key);
    unsigned int file = 0u;
    while (
        (file < g_cli_replay->break_file_count) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_replay->break_files[file], .other = key, .cap = CLI_POOL_BYTES})))
    {
        file += 1u;
    }
    const int fits = (file < CLI_RECORDING_BREAK_FILES);
    if (fits && (file == g_cli_replay->break_file_count))
    {
        memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->break_files[file],
                                .src = key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u});
        g_cli_replay->break_file_count += 1u;
    }
    if (fits)
    {
        g_cli_replay->break_count[file] = 0u;
    }
    const unsigned int breakpoints = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &s_json, .object = args->arguments, .name = "breakpoints");
    mmgr_line_put(args->body_out, "{\"breakpoints\":[");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &s_json, .array = breakpoints, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &s_json, .array = breakpoints, .index = at);
        const unsigned int line_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &s_json, .object = one, .name = "line");
        unsigned long long line = 0ULL;
        const int whole = EMBED_CALL(cli_recording_json_internal.replay_integer, CliRecordingJsonCtx, .json = &s_json,
                                     .id = line_node, .value = &line);
        // a line past 32 bits retains its low 32, the width of the line numbers the recording stores
        const unsigned int narrow = (unsigned int)(line & 0xffffffffULL);
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
        mmgr_line_put(args->body_out, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        mmgr_line_decimal(args->body_out, (unsigned long long)at + 1ULL, 1u);
        mmgr_line_put(args->body_out, ",\"verified\":true,\"line\":");
        EMBED_CALL(request_raw, RecordingRequestCtx, .out = args->body_out, .node = line_node);
        mmgr_line_ch(args->body_out, '}');
    }
    mmgr_line_put(args->body_out, "]}");
}

// whether the exception filters requested take in the raised one
EMBED_INLINE int request_on_raise(const RecordingRequestCtx *args)
{
    const unsigned int filters = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_json, .object = args->arguments, .name = "filters");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &s_json, .array = filters, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const char *const filter =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &s_json, .array = filters, .index = at),
                       .length_pointer = NULL);
        if ((filter != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = filter, .other = "raised", .cap = CLI_POOL_BYTES})))
        {
            return 1;
        }
    }
    return 0;
}

// the replay stands fixed outside the pool, one for the process, where it punches no hole in the persistent tier
static CliReplay s_replay;

int cli_replay_lock(void)
{
    if (g_cli_replay == NULL)
    {
        g_cli_replay = &s_replay;
        memor.set(&(MemoriaCfg){.dst = g_cli_replay, .val = 0, .bytes = sizeof(CliReplay)});
        cli_recording_let_go();
    }
    return 1;
}

void *cli_recording_begin(const CliRecordingCfg *args)
{
    cli_recording_run_end();
    cli_recording_let_go();
    if (!cli_replay_lock())
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    g_cli_replay->program[0] = '\0';
    g_cli_replay->python[0] = '\0';
    g_cli_replay->at = 0u;
    g_cli_replay->break_file_count = 0u;
    g_cli_replay->on_raised = 0;
    s_seq = 0ULL;
    return g_cli_replay;
}

// the request served, read into a tree of its size: 1, or 0 where the adapter is let go
static int request_served(const CliDapServeCfg *args)
{
    CliDap *dap = args->dap;
    unsigned long long size = args->size;
    char why[CLI_RECORDING_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    // the request read into a tree of its size, given back as cli_recording_serve ends
    const unsigned int message = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                            .json = &s_json, .text = args->request, .size = size, .nodes_more = 0u,
                                            .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_debug_owner);
    if (message == CLI_JSON_NONE)
    {
        return 1;
    }
    const char *const type =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                      .object = message, .name = "type"),
                   .length_pointer = NULL);
    if ((type != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "orior", .cap = CLI_POOL_BYTES})))
    {
        EMBED_CALL(cli_recording_dap_json_internal.record, CliRecordingDapJsonCtx, .dap = dap, .json = &s_json,
                   .message = message);
        return 1;
    }
    char command[CLI_RECORDING_COMMAND];
    const char *const named =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                      .object = message, .name = "command"),
                   .length_pointer = NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = command, .text = named, .otherwise = "");
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &s_json, .object = message, .name = "arguments");
    // the answer's body on a line grown to it, ended once the reply is sent
    CliGrownLine body_grown;
    cli_grown_begin(&body_grown, CLI_RECORDING_BODY, g_cli_debug_owner);
    VerbaLinea *const body_line = &body_grown.line;
    int success = 1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "initialize", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(body_line, "{\"supportsConfigurationDoneRequest\":true,\"supportsStepBack\":true,"
                                 "\"supportsExceptionInfoRequest\":true,\"supportsEvaluateForHovers\":true,"
                                 "\"exceptionBreakpointFilters\":[{\"filter\":\"raised\",\"label\":\"Raised "
                                 "Exceptions\",\"default\":false}]}");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "launch", .cap = CLI_POOL_BYTES}))
    {
        const char *const program =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                          .object = arguments, .name = "program"),
                       .length_pointer = NULL);
        const char *const python =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                          .object = arguments, .name = "python"),
                       .length_pointer = NULL);
        EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = g_cli_replay->program, .text = program,
                   .otherwise = "");
        EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = g_cli_replay->python, .text = python,
                   .otherwise = "");
        mmgr_line_put(body_line, "null");
        EMBED_CALL(request_answer, RecordingRequestCtx, .dap = dap, .message = message, .command = command,
                   .success = 1, .body = body_line, .err_msg = &err_msg);
        cli_grown_end(&body_grown);
        const char initialized[] = "{\"type\":\"event\",\"event\":\"initialized\"}";
        (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = initialized,
                         .size = sizeof(initialized) - 1u);
        return 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "setBreakpoints", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(request_breakpoints, RecordingRequestCtx, .arguments = arguments, .body_out = body_line);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "setExceptionBreakpoints", .cap = CLI_POOL_BYTES}))
    {
        g_cli_replay->on_raised = EMBED_CALL(request_on_raise, RecordingRequestCtx, .arguments = arguments);
        mmgr_line_put(body_line, "null");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "configurationDone", .cap = CLI_POOL_BYTES}))
    {
        success = EMBED_CALL(cli_recording_dap_internal.run, CliRecordingDapCtx, .dap = dap, .err_msg = &err_msg);
        mmgr_line_put(body_line, "null");
        EMBED_CALL(request_answer, RecordingRequestCtx, .dap = dap, .message = message, .command = command,
                   .success = success, .body = body_line, .err_msg = &err_msg);
        cli_grown_end(&body_grown);
        if (!success)
        {
            const char terminated[] = "{\"type\":\"event\",\"event\":\"terminated\",\"body\":{}}";
            (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = terminated,
                             .size = sizeof(terminated) - 1u);
        }
        return 1;
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disconnect", .cap = CLI_POOL_BYTES})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "terminate", .cap = CLI_POOL_BYTES})))
    {
        cli_recording_run_kill();
        mmgr_line_put(body_line, "null");
        EMBED_CALL(request_answer, RecordingRequestCtx, .dap = dap, .message = message, .command = command,
                   .success = 1, .body = body_line, .err_msg = &err_msg);
        cli_grown_end(&body_grown);
        return !cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disconnect", .cap = CLI_POOL_BYTES});
    }
    else
    {
        success =
            EMBED_CALL(cli_recording_dap_internal.replay_answer, CliRecordingDapCtx, .dap = dap, .command = command,
                       .json = &s_json, .arguments = arguments, .body = body_line, .err_msg = &err_msg);
    }
    EMBED_CALL(request_answer, RecordingRequestCtx, .dap = dap, .message = message, .command = command,
               .success = success, .body = body_line, .err_msg = &err_msg);
    cli_grown_end(&body_grown);
    return 1;
}

int cli_recording_serve(const CliDapServeCfg *args)
{
    const int served = request_served(args);
    g_cli_guard->persistent_buf_release(s_json.node, g_cli_debug_owner);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return served;
}
