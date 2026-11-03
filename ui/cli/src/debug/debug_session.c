// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_session.c: the sessions in their places: a session taken for a request and let go, a place freed once its
// session is no longer listed, its adapter has ended and no request uses it, the requests sent to an adapter in work
// states of their own, the sessions listed, a file's breakpoints set in each session that debugs it, and sessions ended
#include "debug_internal.h"

CliDebugWorkState g_cli_debug = {.wake = CLI_WAKE_INIT};

static CliDebugAnswer s_answers[CLI_DEBUG_ANSWERS];
static CliWake s_answers_wake = CLI_WAKE_INIT;

typedef struct
{
    CliDebugSession *session;
} DebugSessionCtx;
// a place freed where its session is no longer listed, its adapter's last event was received and nothing uses it, under
// the sessions' lock
EMBED_INLINE void session_freed_where_finish(const DebugSessionCtx *args)
{
    if (args->session->reserved && !args->session->list_entries && args->session->adapter_ended &&
        (args->session->users == 0u))
    {
        EMBED_CALL(cli_debug_internal.position_free, CliDebugCtx, .place = args->session);
    }
}

CliDebugSession *cli_debug_alloc(const CliDebugErrMsgIdCtx *args)
{
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->reserved && session->list_entries && (session->info.id == args->id))
        {
            session->users += 1u;
            EMBED_CALL(cli_dap.ref, CliDapCfg, .dap = session->dap);
            return session;
        }
    }
    mmgr_line_put(args->err_msg, "that session has ended");
    return NULL;
}

void cli_debug_let_go(const CliDebugSessionErrMsgCtx *args)
{
    EMBED_CALL(cli_dap.unref, CliDapCfg, .dap = args->session->dap);
    args->session->users -= 1u;
    EMBED_CALL(session_freed_where_finish, DebugSessionCtx, .session = args->session);
}

CliDebugSession *cli_debug_position(void)
{
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (!session->reserved)
        {
            session->reserved = 1;
            session->list_entries = 0;
            session->adapter_ended = 0;
            session->users = 1u;
            session->dap = NULL;
            const unsigned long long positioned_bytes =
                (unsigned long long)CLI_DEBUG_POSITIONED * sizeof(*session->positioned);
            char *const lists = g_cli_cells->persistent_buf_alloc(
                positioned_bytes + ((unsigned long long)CLI_DEBUG_DATA * sizeof(*session->data)), &g_cli_debug_cells);
            session->positioned = NULL;
            session->positioned_count = 0u;
            session->positioned_capacity = 0u;
            session->data = NULL;
            session->data_count = 0u;
            session->data_capacity = 0u;
            if (lists != NULL)
            {
                session->positioned = (void *)lists;
                session->positioned_capacity = CLI_DEBUG_POSITIONED;
                session->data = (void *)(lists + positioned_bytes);
                session->data_capacity = CLI_DEBUG_DATA;
            }
            session->watching = 0;
            session->scripted = 0;
            session->has_across = 0;
            session->pid = 0ULL;
            session->initialized = 0;
            memor.set(&(MemoriaCfg){.dst = &session->info, .val = 0, .bytes = sizeof(session->info)});
            return session;
        }
    }
    return NULL;
}

CliDebugSession *cli_debug_native_get(const CliDebugErrMsgIdCtx *args)
{
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->reserved && session->list_entries && session->info.has_parent &&
            (session->info.parent == args->id) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = session->info.language, .other = "python", .cap = CLI_CELLS_POOL})) &&
            session->info.has_process)
        {
            session->users += 1u;
            EMBED_CALL(cli_dap.ref, CliDapCfg, .dap = session->dap);
            return session;
        }
    }
    return NULL;
}

CliDebugAnswer *cli_debug_answer_alloc(void)
{
    CliDebugAnswer *answer = NULL;
    while (answer == NULL)
    {
        unsigned int at = 0u;
        while ((at < CLI_DEBUG_ANSWERS) && (answer == NULL))
        {
            answer = s_answers[at].reserved ? NULL : &s_answers[at];
            at += 1u;
        }
        if (answer == NULL)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_answers_wake);
        }
    }
    answer->reserved = 1;
    char *const args_text = g_cli_cells->temporary_buf_alloc(CLI_DEBUG_ARGS, &g_cli_debug_cells);
    answer->args = mmgr_line_over(args_text, CLI_DEBUG_ARGS);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &answer->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    answer->root = CLI_JSON_NONE;
    return answer;
}

void cli_debug_answer_free(const CliDebugSessionCommandCtx *args)
{
    g_cli_cells->persistent_buf_release(args->answer->args.out, &g_cli_debug_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->answer->json);
    args->answer->reserved = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_answers_wake);
}

int cli_debug_request(const CliDebugSessionCommandCtx *args)
{
    const char *command = args->command;
    CliDebugAnswer *answer = args->answer;
    unsigned long long wait = args->wait;
    VerbaLinea *err_msg = args->err_msg;
    if (!mmgr_line_ok(&answer->args))
    {
        mmgr_line_put(err_msg, "the request's arguments could not be stored: they do not fit");
        return 0;
    }
    // the arguments as written, an empty object where none are; and the work state emptied for a request after this one
    const unsigned long long written = answer->args.at;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &answer->json);
    CliLspAnswer reply;
    const int answered =
        EMBED_CALL(cli_dap_err_msg.request, CliDapErrMsgCfg, .dap = args->session->dap, .command = command,
                   .arguments = (written > 0ULL) ? answer->args.out : "{}", .length = (written > 0ULL) ? written : 2ULL,
                   .wait = wait, .answer = &reply, .err_msg = err_msg);
    answer->args.at = 0ULL;
    if (!answered)
    {
        return 0;
    }
    answer->root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                              .json = &answer->json, .text = reply.text, .size = reply.size, .nodes_more = 0u,
                              .text_more = 0ULL, .err_msg = err_msg, .owner = &g_cli_debug_cells);
    g_cli_cells->persistent_buf_release(reply.message, &g_cli_debug_cells);
    return answer->root != CLI_JSON_NONE;
}

void cli_debug_info_write(const CliDebugInfoCtx *args)
{
    mmgr_line_put(args->out, "{\"id\":");
    mmgr_line_decimal(args->out, args->info->id, 1u);
    mmgr_line_put(args->out, ",\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->info->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->info->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"tool\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->info->tool,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->info->tool, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"language\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->info->language,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->info->language, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"parent\":");
    if (args->info->has_parent)
    {
        mmgr_line_decimal(args->out, args->info->parent, 1u);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"filters\":[");
    for (unsigned int at = 0u; at < args->info->filter_count; at += 1u)
    {
        const CliDebugFilter *const filter = &args->info->filters[at];
        mmgr_line_put(args->out, (at > 0u) ? ",{\"filter\":" : "{\"filter\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)filter->filter,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = filter->filter, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"label\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)filter->label,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = filter->label, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, filter->on ? ",\"on\":true}" : ",\"on\":false}");
    }
    const int can[7] = {args->info->conditions,  args->info->hits,           args->info->logs, args->info->memory,
                        args->info->disassembly, args->info->exception_info, args->info->data};
    const char *const names[7] = {"conditions", "hits", "logs", "memory", "disassembly", "exception_info", "data"};
    mmgr_line_ch(args->out, ']');
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        mmgr_line_put(args->out, ",\"");
        mmgr_line_put(args->out, names[at]);
        mmgr_line_put(args->out, can[at] ? "\":true" : "\":false");
    }
    mmgr_line_put(args->out, ",\"process\":");
    if (args->info->has_process)
    {
        mmgr_line_decimal(args->out, args->info->process, 1u);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, args->info->step_back ? ",\"step_back\":true" : ",\"step_back\":false");
    mmgr_line_put(args->out, args->info->recorded ? ",\"recorded\":true}" : ",\"recorded\":false}");
}

void cli_debug_sessions(const CliDebugOutErrMsgIdStepCfg *args)
{
    unsigned long long last = 0ULL;
    int written = 0;
    mmgr_line_ch(args->out, '[');
    // by number, the order they were started in
    for (;;)
    {
        const CliDebugSession *next = NULL;
        for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
        {
            const CliDebugSession *const one = &g_cli_debug.sessions[at];
            if (one->reserved && one->list_entries && (one->info.id > last) &&
                ((next == NULL) || (one->info.id < next->info.id)))
            {
                next = one;
            }
        }
        if (next == NULL)
        {
            break;
        }
        mmgr_line_put(args->out, written ? "," : "");
        EMBED_CALL(cli_debug_info_internal.info_write, CliDebugInfoCtx, .info = &next->info, .out = args->out);
        written = 1;
        last = next->info.id;
    }
    mmgr_line_ch(args->out, ']');
}

int cli_debug_running(void)
{
    int running = 0;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        running = running || (g_cli_debug.sessions[at].reserved && g_cli_debug.sessions[at].list_entries);
    }
    return running;
}

int cli_debug_set(const CliDebugSessionCtx *args)
{
    CliDebugAnswer *const answer = cli_debug_answer_alloc();
    mmgr_line_put(&answer->args, "{\"source\":{\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args,
               .text_bytes = (const unsigned char *)args->file,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&answer->args, "},\"breakpoints\":");
    EMBED_CALL(cli_debug_session_internal.breakpoints_write, CliDebugSessionCtx, .json = args->json,
               .breakpoints = args->breakpoints, .out = &answer->args);
    mmgr_line_ch(&answer->args, '}');
    if (!EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = args->session,
                    .command = "setBreakpoints", .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS,
                    .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int positioned = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = reply, .object = answer->root, .name = "breakpoints");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = positioned, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        long long number = 0LL;
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = positioned, .index = at);
        if (!EMBED_CALL(cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = reply,
                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                           .object = one, .name = "id"),
                        .value = &number))
        {
            continue;
        }
        if (args->session->positioned_count < args->session->positioned_capacity)
        {
            args->session->positioned[args->session->positioned_count] = number;
            args->session->positioned_count += 1u;
        }
    }
    // each answer paired with the breakpoint requested in its turn, as many as both have
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u;
         (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = reply,
                     .array = positioned, .index = at) != CLI_JSON_NONE) &&
         (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                     .array = args->breakpoints, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = positioned, .index = at);
        unsigned long long line = 0ULL;
        if (EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                       .json = reply,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                          .object = one, .name = "line"),
                       .value_u64s = &line))
        {
            line = (line > 0ULL) ? line - 1ULL : 0ULL;
        }
        else
        {
            (void)EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                             .json = args->json,
                             .node = EMBED_CALL(
                                 cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                 .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                      .json_read = args->json, .array = args->breakpoints, .index = at),
                                 .name = "line"),
                             .value_u64s = &line);
        }
        mmgr_line_put(args->out, (at > 0u) ? ",[" : "[");
        // a line retains its low 32 bits, as the window's line numbers are
        mmgr_line_decimal(args->out, line & 0xffffffffULL, 1u);
        mmgr_line_put(args->out, EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                               .json_read = reply, .object = one, .name = "verified"))
                                     ? ",true]"
                                     : ",false]");
    }
    mmgr_line_ch(args->out, ']');
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    return 1;
}

void cli_debug_breakpoints(const CliDebugRootJsonCfg *args)
{
    char full[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->path, .joined = full);
    const unsigned long long start = args->out->at;
    int positioned = 0;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        const CliDebugSession *const session = &g_cli_debug.sessions[at];
        const int allocs = session->reserved && session->list_entries &&
                           EMBED_CALL(cli_debug_file_internal.allocs, CliDebugFileCtx,
                                      .language = session->info.language, .file = args->path);
        const unsigned long long id = session->info.id;
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        CliDebugSession *const reserved =
            allocs ? EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx, .id = id, .err_msg = &err_msg)
                   : NULL;
        if (reserved == NULL)
        {
            continue;
        }
        // the first session's lines are what is reported; the others are set and their lines passed over
        const unsigned long long before = args->out->at;
        if (EMBED_CALL(cli_debug_session_internal.set, CliDebugSessionCtx, .session = reserved, .file = full,
                       .json = args->json, .breakpoints = args->breakpoints, .out = args->out, .err_msg = &err_msg) &&
            !positioned)
        {
            positioned = 1;
        }
        else
        {
            args->out->at = before;
        }
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = reserved);
    }
    if (!positioned)
    {
        args->out->at = start;
        mmgr_line_put(args->out, "null");
    }
}

void cli_debug_process(const CliDebugCfg *args)
{
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->reserved && session->list_entries && (session->info.id == args->id))
        {
            session->info.has_process = 1;
            session->info.process = args->pid;
        }
    }
}

// the session's own use given back on a thread of its own, as an adapter's event, received on the adapter's reader,
// ends it: the adapter waits on that reader as it is let go
static void session_free(void *context)
{
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx,
               .session = (CliDebugSession *)context);
}

void cli_debug_end(const CliDebugCfg *args)
{
    CliDebugSession *ended = NULL;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->reserved && session->list_entries && (session->info.id == args->id))
        {
            session->list_entries = 0;
            ended = session;
        }
    }
    if (ended == NULL)
    {
        return;
    }
    ended->provider = (CliThread){session_free, ended};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &ended->provider))
    {
        session_free(ended);
    }
}

void cli_debug_stop(const CliDebugCfg *args)
{
    CliDebugSession *ended[CLI_DEBUG_SESSIONS];
    unsigned int count = 0u;
    unsigned long long deleted[CLI_DEBUG_SESSIONS + 1u];
    unsigned int going = 1u;
    deleted[0] = args->id;
    // the session, then each it started, and each they started in turn
    while (going > 0u)
    {
        going -= 1u;
        const unsigned long long one = deleted[going];
        for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
        {
            CliDebugSession *const session = &g_cli_debug.sessions[at];
            const int reserved =
                session->reserved && session->list_entries &&
                ((session->info.id == one) || (session->info.has_parent && (session->info.parent == one)));
            if (!reserved)
            {
                continue;
            }
            session->list_entries = 0;
            ended[count] = session;
            count += 1u;
            if ((session->info.id != one) && (going < CLI_DEBUG_SESSIONS))
            {
                deleted[going] = session->info.id;
                going += 1u;
            }
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_dap.stop, CliDapCfg, .dap = ended[at]->dap);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = ended[at]);
    }
}

void cli_debug_stop_all(void)
{
    CliDebugSession *ended[CLI_DEBUG_SESSIONS];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->reserved && session->list_entries)
        {
            session->list_entries = 0;
            ended[count] = session;
            count += 1u;
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_dap.stop, CliDapCfg, .dap = ended[at]->dap);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = ended[at]);
    }
}
