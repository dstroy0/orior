// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_session.c: the sessions in their places: a session taken for a request and let go, a place freed once its
// session is no longer listed, its adapter has ended and no request uses it, the requests sent to an adapter in rooms
// of their own, the sessions listed, a file's breakpoints set in each session that debugs it, and sessions ended
#include "debug_internal.h"

CliDebugRoom g_cli_debug = {.lock = CLI_LOCK_INIT, .wake = CLI_WAKE_INIT};

static CliDebugAnswer s_answers[CLI_DEBUG_ANSWERS];
static CliLock s_answers_lock = CLI_LOCK_INIT;
static CliWake s_answers_wake = CLI_WAKE_INIT;

typedef struct
{
    CliDebugSession *session;
} DebugSessionCtx;
// a place freed where its session is no longer listed, its adapter's last event was received and nothing uses it, under
// the sessions' lock
EMBED_INLINE void session_freed_where_finish(const DebugSessionCtx *args)
{
    if (args->session->taken && !args->session->listed && args->session->adapter_ended && (args->session->users == 0u))
    {
        EMBED_CALL(cli_debug_internal.place_free, CliDebugCtx, .place = args->session);
    }
}

CliDebugSession *cli_debug_take(const CliDebugErrMsgIdCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == args->id))
        {
            session->users += 1u;
            EMBED_CALL(cli_dap.ref, CliDapCfg, .dap = session->dap);
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
            return session;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    mmgr_line_put(args->err_msg, "that session has ended");
    return NULL;
}

void cli_debug_let_go(const CliDebugSessionErrMsgCtx *args)
{
    EMBED_CALL(cli_dap.unref, CliDapCfg, .dap = args->session->dap);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    args->session->users -= 1u;
    EMBED_CALL(session_freed_where_finish, DebugSessionCtx, .session = args->session);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
}

CliDebugSession *cli_debug_place(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (!session->taken)
        {
            session->taken = 1;
            session->listed = 0;
            session->adapter_ended = 0;
            session->users = 1u;
            session->dap = NULL;
            const unsigned long long placed_bytes = (unsigned long long)CLI_DEBUG_PLACED * sizeof(*session->placed);
            char *const lists = g_cli_cells->persistent_buf_alloc(
                placed_bytes + ((unsigned long long)CLI_DEBUG_DATA * sizeof(*session->data)), &g_cli_debug_cells);
            session->placed = NULL;
            session->placed_count = 0u;
            session->placed_room = 0u;
            session->data = NULL;
            session->data_count = 0u;
            session->data_room = 0u;
            if (lists != NULL)
            {
                session->placed = (void *)lists;
                session->placed_room = CLI_DEBUG_PLACED;
                session->data = (void *)(lists + placed_bytes);
                session->data_room = CLI_DEBUG_DATA;
            }
            session->watching = 0;
            session->scripted = 0;
            session->has_across = 0;
            session->pid = 0ull;
            session->initialized = 0;
            memor.set(&(MemoriaCfg){.dst = &session->info, .val = 0, .bytes = sizeof(session->info)});
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
            return session;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    return NULL;
}

CliDebugSession *cli_debug_native_get(const CliDebugErrMsgIdCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && session->info.has_parent && (session->info.parent == args->id) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = session->info.language, .other = "python", .cap = CLI_CELLS_POOL})) &&
            session->info.has_process)
        {
            session->users += 1u;
            EMBED_CALL(cli_dap.ref, CliDapCfg, .dap = session->dap);
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
            return session;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    return NULL;
}

CliDebugAnswer *cli_debug_answer_take(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_answers_lock);
    CliDebugAnswer *answer = NULL;
    while (answer == NULL)
    {
        for (unsigned int at = 0u; (at < CLI_DEBUG_ANSWERS) && (answer == NULL); at += 1u)
        {
            answer = s_answers[at].taken ? NULL : &s_answers[at];
        }
        if (answer == NULL)
        {
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_answers_wake, .lock = &s_answers_lock);
        }
    }
    answer->taken = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_answers_lock);
    char *const args_text = g_cli_cells->temporary_buf_alloc(CLI_DEBUG_ARGS, &g_cli_debug_cells);
    answer->args = mmgr_line_over(args_text, CLI_DEBUG_ARGS);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &answer->json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    answer->root = CLI_JSON_NONE;
    return answer;
}

void cli_debug_answer_free(const CliDebugSessionCommandCtx *args)
{
    g_cli_cells->persistent_buf_release(args->answer->args.out, &g_cli_debug_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->answer->json);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_answers_lock);
    args->answer->taken = 0;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_answers_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_answers_lock);
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
    // the arguments as written, an empty object where none are; and the room emptied for a request after this one
    const unsigned long long written = answer->args.at;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &answer->json);
    CliLspAnswer reply;
    const int answered =
        EMBED_CALL(cli_dap_err_msg.request, CliDapErrMsgCfg, .dap = args->session->dap, .command = command,
                   .arguments = (written > 0ull) ? answer->args.out : "{}", .length = (written > 0ull) ? written : 2ull,
                   .wait = wait, .answer = &reply, .err_msg = err_msg);
    answer->args.at = 0ull;
    if (!answered)
    {
        return 0;
    }
    answer->root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg,
                              .json = &answer->json, .text = reply.text, .size = reply.size, .nodes_more = 0u,
                              .text_more = 0ull, .err_msg = err_msg, .owner = &g_cli_debug_cells);
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
    unsigned long long last = 0ull;
    int written = 0;
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    // by number, the order they were started in
    for (;;)
    {
        const CliDebugSession *next = NULL;
        for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
        {
            const CliDebugSession *const one = &g_cli_debug.sessions[at];
            if (one->taken && one->listed && (one->info.id > last) &&
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
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    mmgr_line_ch(args->out, ']');
}

int cli_debug_running(void)
{
    int running = 0;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        running = running || (g_cli_debug.sessions[at].taken && g_cli_debug.sessions[at].listed);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    return running;
}

int cli_debug_set(const CliDebugSessionCtx *args)
{
    CliDebugAnswer *const answer = cli_debug_answer_take();
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
    const unsigned int placed = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                           .object = answer->root, .name = "breakpoints");
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = placed, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        long long number = 0ll;
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = placed, .index = at);
        if (!EMBED_CALL(cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = reply,
                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                           .object = one, .name = "id"),
                        .value = &number))
        {
            continue;
        }
        if (args->session->placed_count < args->session->placed_room)
        {
            args->session->placed[args->session->placed_count] = number;
            args->session->placed_count += 1u;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    // each answer paired with the breakpoint requested in its turn, as many as both have
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u;
         (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = reply, .array = placed,
                     .index = at) != CLI_JSON_NONE) &&
         (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                     .array = args->breakpoints, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = placed, .index = at);
        unsigned long long line = 0ull;
        if (EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                       .json = reply,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                          .object = one, .name = "line"),
                       .value_u64s = &line))
        {
            line = (line > 0ull) ? line - 1ull : 0ull;
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
        mmgr_line_decimal(args->out, line & 0xffffffffull, 1u);
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
    int placed = 0;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        const int takes = session->taken && session->listed &&
                          EMBED_CALL(cli_debug_file_internal.takes, CliDebugFileCtx, .language = session->info.language,
                                     .file = args->path);
        const unsigned long long id = session->info.id;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        CliDebugSession *const taken =
            takes ? EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx, .id = id, .err_msg = &err_msg)
                  : NULL;
        if (taken == NULL)
        {
            continue;
        }
        // the first session's lines are what is reported; the others are set and their lines passed over
        const unsigned long long before = args->out->at;
        if (EMBED_CALL(cli_debug_session_internal.set, CliDebugSessionCtx, .session = taken, .file = full,
                       .json = args->json, .breakpoints = args->breakpoints, .out = args->out, .err_msg = &err_msg) &&
            !placed)
        {
            placed = 1;
        }
        else
        {
            args->out->at = before;
        }
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = taken);
    }
    if (!placed)
    {
        args->out->at = start;
        mmgr_line_put(args->out, "null");
    }
}

void cli_debug_process(const CliDebugCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == args->id))
        {
            session->info.has_process = 1;
            session->info.process = args->pid;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == args->id))
        {
            session->listed = 0;
            ended = session;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    if (ended == NULL)
    {
        return;
    }
    ended->giver = (CliThread){session_free, ended};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &ended->giver))
    {
        session_free(ended);
    }
}

void cli_debug_stop(const CliDebugCfg *args)
{
    CliDebugSession *ended[CLI_DEBUG_SESSIONS];
    unsigned int count = 0u;
    unsigned long long gone[CLI_DEBUG_SESSIONS + 1u];
    unsigned int going = 1u;
    gone[0] = args->id;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    // the session, then each it started, and each they started in turn
    while (going > 0u)
    {
        going -= 1u;
        const unsigned long long one = gone[going];
        for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
        {
            CliDebugSession *const session = &g_cli_debug.sessions[at];
            const int taken =
                session->taken && session->listed &&
                ((session->info.id == one) || (session->info.has_parent && (session->info.parent == one)));
            if (!taken)
            {
                continue;
            }
            session->listed = 0;
            ended[count] = session;
            count += 1u;
            if ((session->info.id != one) && (going < CLI_DEBUG_SESSIONS))
            {
                gone[going] = session->info.id;
                going += 1u;
            }
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed)
        {
            session->listed = 0;
            ended[count] = session;
            count += 1u;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_debug.lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_dap.stop, CliDapCfg, .dap = ended[at]->dap);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = ended[at]);
    }
}
