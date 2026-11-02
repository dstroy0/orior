// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_debug.c: the calls of the debugger: a session started, its events going to the page as debug-event, a
// session forgotten once its adapter reports its program ended and its process recorded as the adapter names it; the
// sessions, the breakpoints, the exceptions a session stops on, its threads, its stack, scopes, variables and values;
// its steps, a step across into the tree's own C, a watch of data, a recorded run's history and a step of it gone to;
// memory and instructions; a Python line's bytecode; the processes; and a session or every one stopped. A path an
// adapter names is written as it stands in the tree
#include "../command_line/callback_cfg.h"
#include "../debug/debug.h"
#include "serve_internal.h"

// what a session sends unprompted sent to the page as debug-event, a session's end and its process recorded first
static void debug_event(const CliDebugEventCfg *args)
{
    const char *event = args->event;
    const char *body = args->body;
    unsigned long long size = args->size;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "terminated", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "adapterStopped", .cap = CLI_CELLS_POOL})))
    {
        EMBED_CALL(cli_debug_table.end, CliDebugCfg, .id = args->session);
    }
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "process", .cap = CLI_CELLS_POOL})) &&
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size, .name = "systemProcessId",
                   .start = &at, .length_pointer = &length))
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
            EMBED_CALL(cli_debug_table.process, CliDebugCfg, .id = args->session, .pid = (unsigned int)pid);
        }
    }
    // the event's line in a cell of its body and the words about it, the event's name quoted on top
    const unsigned long long named = cellul.len(&(CatenaFinitaCfg){.src = event, .cap = CLI_CELLS_POOL});
    const unsigned long long line_room = size + named + 64ull;
    char *const line_cell = g_cli_cells->temporary_buf_alloc(line_room, &g_cli_window_cells);
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    mmgr_line_put(&line, "{\"session\":");
    mmgr_line_decimal(&line, args->session, 1u);
    mmgr_line_put(&line, ",\"event\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = event,
               .length = named);
    mmgr_line_put(&line, ",\"body\":");
    mmgr_line_put_n(&line, (size > 0ull) ? body : "null", (size > 0ull) ? size : 4ull);
    mmgr_line_ch(&line, '}');
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "debug-event", .body = line.out,
                   .size = line.at);
    }
    g_cli_cells->persistent_buf_release(line.out, &g_cli_window_cells);
}

// a path an adapter named written as it stands in the tree, the tree its context
static void debug_path_write(const CliDebugPathWrittenCfg *args)
{
    const char *path = args->path;
    VerbaLinea *out = args->out;
    const char *const root = args->context;
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea line = {shown, sizeof(shown), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = root, .path = path,
               .out = &line);
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)shown, .length = mmgr_line_ok(&line) ? line.at : 0ull);
}

typedef struct
{
    const CliServeArgs *request;
    unsigned long long *session;
    VerbaLinea *err_msg;
} ServeCallDebugCtx;
// the argument `session`: 1, or 0 with why given
EMBED_INLINE int debug_session(const ServeCallDebugCtx *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "session",
                      .value = args->session, .err_msg = err_msg);
}

static int debug_start(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const CliDebugSink sink = {debug_event, NULL};
    return EMBED_CALL(
        cli_debug_root.start, CliDebugRootCfg, .root = root, .json = arguments->json,
        .start = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "start"),
        .options =
            EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "options"),
        .sink = sink, .out = out, .err_msg = err_msg);
}

static int debug_sessions(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_debug_out_err_msg_id_step.sessions, CliDebugOutErrMsgIdStepCfg, .out = out);
    return 1;
}

static int debug_breakpoints(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_debug_root_json.breakpoints, CliDebugRootJsonCfg, .root = root, .path = path,
               .json = arguments->json,
               .breakpoints = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments,
                                         .key = "breakpoints"),
               .out = out);
    return 1;
}

static int debug_exceptions(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    return EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(
               cli_debug_json.exceptions, CliDebugJsonCfg, .id = session, .json = arguments->json,
               .on = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "on"),
               .out = out, .err_msg = err_msg);
}

static int debug_raise(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long thread = 0ll;
    return EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "thread",
                      .value_i64s = &thread, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_thread_how.raise, CliDebugOutErrMsgIdThreadHowCfg, .id = session,
                      .thread = thread, .out = out, .err_msg = err_msg);
}

static int debug_threads(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    return EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_step.threads, CliDebugOutErrMsgIdStepCfg, .id = session, .out = out,
                      .err_msg = err_msg);
}

static int debug_stack(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long session = 0ull;
    long long thread = 0ll;
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "thread",
                      .value_i64s = &thread, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_thread.stack, CliDebugOutErrMsgIdThreadCfg, .id = session,
                      .thread = thread, .named = debug_path_write, .context = root, .out = out, .err_msg = err_msg);
}

static int debug_scopes(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long frame = 0ll;
    return EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "frame",
                      .value_i64s = &frame, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg.scopes, CliDebugOutErrMsgCfg, .id = session, .frame = frame, .out = out,
                      .err_msg = err_msg);
}

static int debug_variables(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long reference = 0ll;
    return EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments,
                      .key = "reference", .value_i64s = &reference, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out.variables, CliDebugOutCfg, .id = session, .reference = reference, .out = out,
                      .err_msg = err_msg);
}

static int debug_evaluate(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long frame = 0ll;
    const char *const expression = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                              .request = arguments, .key = "expression", .err_msg = err_msg);
    const char *const context = (expression != NULL)
                                    ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                 .request = arguments, .key = "context", .err_msg = err_msg)
                                    : NULL;
    if ((context == NULL) ||
        !EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned int given =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "frame");
    const int has_frame =
        (given != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = arguments->json, .node = given, .kind = CFG_JSON_NULL);
    if (has_frame && !EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments,
                                 .key = "frame", .value_i64s = &frame, .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(cli_debug_out_err_msg.evaluate, CliDebugOutErrMsgCfg, .id = session, .expression = expression,
                      .has_frame = has_frame, .frame = frame, .context_text = context, .out = out, .err_msg = err_msg);
}

static int debug_step(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long thread = 0ll;
    const char *const how = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "how", .err_msg = err_msg);
    return (how != NULL) &&
           EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "thread",
                      .value_i64s = &thread, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_thread_how.step, CliDebugOutErrMsgIdThreadHowCfg, .id = session,
                      .how = how, .thread = thread, .out = out, .err_msg = err_msg);
}

static int debug_memory(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long offset = 0ll;
    unsigned long long count = 0ull;
    const char *const reference = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                             .key = "reference", .err_msg = err_msg);
    return (reference != NULL) &&
           EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "offset",
                      .value_i64s = &offset, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "count",
                      .value = &count, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_reference_offset.memory, CliDebugOutErrMsgIdReferenceOffsetCfg,
                      .id = session, .reference_text = reference, .offset = offset, .count = count, .out = out,
                      .err_msg = err_msg);
}

static int debug_instructions(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long offset = 0ll;
    long long count = 0ll;
    const char *const reference = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                             .key = "reference", .err_msg = err_msg);
    return (reference != NULL) &&
           EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "offset",
                      .value_i64s = &offset, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "count",
                      .value_i64s = &count, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_reference.instructions, CliDebugOutErrMsgIdReferenceCfg, .id = session,
                      .reference_text = reference, .offset = offset, .count_i64 = count, .out = out,
                      .err_msg = err_msg);
}

static int debug_bytecode(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long line = 0ull;
    // the line is cut to its low word, as no file has more
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                      .root = root, .full = full, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                      .value = &line, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_root_out.bytecode, CliDebugRootOutCfg, .root = root, .path = full,
                      .line = (unsigned int)line, .out = out, .err_msg = err_msg);
}

static int debug_watch_data(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    long long reference = 0ll;
    unsigned long long bytes = 0ull;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if ((name == NULL) ||
        !EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned int referenced =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "reference");
    const unsigned int sized =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "bytes");
    const int has_reference = (referenced != CLI_JSON_NONE) &&
                              !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = arguments->json, .node = referenced, .kind = CFG_JSON_NULL);
    const int has_bytes =
        (sized != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = arguments->json, .node = sized, .kind = CFG_JSON_NULL);
    if ((has_reference && !EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments,
                                      .key = "reference", .value_i64s = &reference, .err_msg = err_msg)) ||
        (has_bytes && !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments,
                                  .key = "bytes", .value = &bytes, .err_msg = err_msg)))
    {
        return 0;
    }
    return EMBED_CALL(cli_debug_out.watch_data, CliDebugOutCfg, .id = session, .name = name,
                      .has_reference = has_reference, .reference = reference, .has_bytes = has_bytes, .bytes = bytes,
                      .out = out, .err_msg = err_msg);
}

static int debug_history(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long session = 0ull;
    long long frame = 0ll;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    return (name != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "frame",
                      .value_i64s = &frame, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id.history, CliDebugOutErrMsgIdCfg, .id = session, .frame = frame,
                      .name = name, .named = debug_path_write, .context = root, .out = out, .err_msg = err_msg);
}

static int debug_step_across(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long session = 0ull;
    long long thread = 0ll;
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = arguments, .key = "thread",
                      .value_i64s = &thread, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_root_out_err_msg.step_across, CliDebugRootOutErrMsgCfg, .root = root, .id = session,
                      .thread = thread, .out = out, .err_msg = err_msg);
}

static int debug_goto(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long session = 0ull;
    unsigned long long step = 0ull;
    return EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "step",
                      .value = &step, .err_msg = err_msg) &&
           EMBED_CALL(cli_debug_out_err_msg_id_step.debug_goto, CliDebugOutErrMsgIdStepCfg, .id = session, .step = step,
                      .out = out, .err_msg = err_msg);
}

static int debug_processes(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_debug_out_err_msg_id_step.processes, CliDebugOutErrMsgIdStepCfg, .out = out);
    return 1;
}

static int debug_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int given =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "session");
    if ((given == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                               .json_read = arguments->json, .node = given, .kind = CFG_JSON_NULL))
    {
        cli_debug_stop_all();
    }
    else
    {
        unsigned long long session = 0ull;
        if (!EMBED_CALL(debug_session, ServeCallDebugCtx, .request = arguments, .session = &session,
                        .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_debug_table.stop, CliDebugCfg, .id = session);
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

const CliServeCall g_cli_serve_debug_calls[] = {
    {"debug_start", debug_start},
    {"debug_sessions", debug_sessions},
    {"debug_breakpoints", debug_breakpoints},
    {"debug_exceptions", debug_exceptions},
    {"debug_raised", debug_raise},
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
