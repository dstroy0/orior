// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_events.c: an adapter's events as a session receives them: its readiness, the process it debugs, a stop, which
// ends a step across, and a stop at a breakpoint the reader did not place reported as the stop of a data breakpoint,
// which it is; then each event handed on with the session's number
#include "../command_line/callback_cfg.h"
#include "debug_internal.h"

// the breakpoints a step across set on the native session taken away, on a thread of their own
static void step_across_clear(void *context)
{
    CliDebugSession *const session = (CliDebugSession *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliDebugSession *const native = EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx,
                                               .id = session->cleared, .err_msg = &err_msg);
    if (native != NULL)
    {
        CliDebugAnswer *const answer = cli_debug_answer_alloc();
        mmgr_line_put(&answer->args,
                      "{\"expression\":\"`breakpoint delete --force " CLI_DEBUG_ACROSS "\",\"context\":\"repl\"}");
        (void)EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = native,
                         .command = "evaluate", .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS,
                         .err_msg = &err_msg);
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = native);
    }
    session->clearing = 0;
    session->users -= 1u;
    if (!session->list_entries && session->adapter_ended && (session->users == 0u))
    {
        g_cli_cells->persistent_buf_release(session->positioned, &g_cli_debug_cells);
        session->data = NULL;
        session->data_count = 0u;
        session->data_capacity = 0u;
        session->positioned = NULL;
        session->positioned_count = 0u;
        session->positioned_capacity = 0u;
        g_cli_cells->persistent_buf_release(
            session->start.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = session->start.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&session->start),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        session->reserved = 0;
    }
}

typedef struct
{
    CliDebugSession *session;
    CliJson *json;
    unsigned int root;
    VerbaLinea *out;
    const char *body;
    unsigned long long size;
} DebugEventsCtx;
// a stop read into `json` at `root` reported as a data breakpoint's where the session's data breakpoints are lldb's
// watchpoints and the stop is at a breakpoint none of whose numbers the reader placed: 1 with it written into `out`,
// or 0 where it is reported as it came
EMBED_INLINE int stop_rewritten(const DebugEventsCtx *args)
{
    CliJson *json = args->json;
    VerbaLinea *out = args->out;
    const char *const reason =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "reason"),
                   .length_pointer = NULL);
    const int watching = args->session->watching;
    if ((args->root == CLI_JSON_NONE) || (reason == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "breakpoint", .cap = CLI_CELLS_POOL})) || !watching)
    {
        return 0;
    }
    const unsigned int hit = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->root, .name = "hitBreakpointIds");
    int any = 0;
    int positioned = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = hit, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        long long number = 0LL;
        if (!EMBED_CALL(cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = json,
                        .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = hit, .index = at),
                        .value = &number))
        {
            continue;
        }
        any = 1;
        for (unsigned int one = 0u; one < args->session->positioned_count; one += 1u)
        {
            positioned = positioned || (args->session->positioned[one] == number);
        }
    }
    if (!any || positioned)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->root,
               .name = "reason",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = "data breakpoint", .length = 15ULL));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->root,
               .name = "description",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = "data changed", .length = 12ULL));
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = args->root,
               .out = out, .indent = 0u, .sorted = 0);
    return !json->full && mmgr_line_ok(out);
}

// a stop's body reported as a data breakpoint's, as stop_rewritten reports it, the body read into a tree taken to its
// size and given back: 1 with the body written into `out`, or 0 where it is reported as it came
EMBED_INLINE int data_stop_rewritten(const DebugEventsCtx *args)
{
    unsigned long long size = args->size;
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->body, .size = size, .nodes_more = CLI_DEBUG_EVENT_MORE_NODES,
                   .text_more = CLI_DEBUG_EVENT_MORE_TEXT, .err_msg = &err_msg, .owner = &g_cli_debug_cells);
    const int written = EMBED_CALL(stop_rewritten, DebugEventsCtx, .session = args->session, .json = &json,
                                   .root = root, .out = args->out);
    g_cli_cells->persistent_buf_release(json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return written;
}

void cli_debug_event(void *context, const char *event, const char *body, unsigned long long size)
{
    CliDebugSession *const session = (CliDebugSession *)context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "initialized", .cap = CLI_CELLS_POOL}))
    {
        session->initialized = 1;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_debug.wake);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "process", .cap = CLI_CELLS_POOL}))
    {
        CliJson json = {0};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        unsigned long long pid = 0ULL;
        const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                             .json = &json, .text = body, .size = size, .nodes_more = 0u,
                                             .text_more = 0ULL, .err_msg = &err_msg, .owner = &g_cli_debug_cells);
        if (EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                       .json = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "systemProcessId"),
                       .value_u64s = &pid))
        {
            session->pid = pid;
        }
        g_cli_cells->persistent_buf_release(json.node,
                                            EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "stopped", .cap = CLI_CELLS_POOL}))
    {
        // a stop ends a step across, and its breakpoints go
        const int clears = session->has_across && !session->clearing;
        if (clears)
        {
            session->has_across = 0;
            session->cleared = session->across;
            session->clearing = 1;
            session->users += 1u;
            session->clearer = (CliThread){step_across_clear, session};
        }
        if (clears && !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &session->clearer))
        {
            step_across_clear(session);
        }
    }
    // a stop rewritten is the body with two members written again, in a cell of the body's size and some over
    const int stopped = cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "stopped", .cap = CLI_CELLS_POOL});
    const unsigned long long rewritten_capacity = stopped ? (size + CLI_DEBUG_EVENT_MORE_TEXT) : 0ULL;
    char *const rewritten_cell =
        stopped ? g_cli_cells->temporary_buf_alloc(rewritten_capacity, &g_cli_debug_cells) : NULL;
    VerbaLinea rewritten_body = mmgr_line_over(rewritten_cell, rewritten_capacity);
    const int rewritten = stopped && EMBED_CALL(data_stop_rewritten, DebugEventsCtx, .session = session, .body = body,
                                                .size = size, .out = &rewritten_body);
    EMBED_CALL(session->sink.emit, CliDebugEventCfg, .context = session->sink.context, .session = session->info.id,
               .event = event, .body = rewritten ? rewritten_body.out : body,
               .size = rewritten ? rewritten_body.at : size);
    g_cli_cells->persistent_buf_release(rewritten_body.out, &g_cli_debug_cells);
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "adapterStopped", .cap = CLI_CELLS_POOL}))
    {
        // the adapter's last event: its place may be freed once nothing else uses it
        session->adapter_ended = 1;
        if (!session->list_entries && (session->users == 0u))
        {
            g_cli_cells->persistent_buf_release(session->positioned, &g_cli_debug_cells);
            session->data = NULL;
            session->data_count = 0u;
            session->data_capacity = 0u;
            session->positioned = NULL;
            session->positioned_count = 0u;
            session->positioned_capacity = 0u;
            g_cli_cells->persistent_buf_release(
                session->start.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = session->start.node));
            EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&session->start),
                       .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
            session->reserved = 0;
        }
    }
}
