// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_requests.c: what a session is queried about its stopped program: the exceptions it stops on and the one it
// stopped on, its threads and their stacks, the scopes and variables of a frame, what an expression comes to, a
// recorded run's history of a name and a step gone to, and the memory and the instructions at an address
#include "../command_line/callback_cfg.h"
#include "debug_internal.h"

typedef struct
{
    unsigned long long id;
    CliDebugSession **session_pointer;
    VerbaLinea *err_msg;
    CliDebugSession *session;
    const char *command;
    CliDebugAnswer *answer;
    const CliJson *json;
    unsigned int node;
    const char *name;
    const char *fallback;
    VerbaLinea *out;
    unsigned long long fallback_u64;
    CliDebugPathWritten named;
    void *context;
    const char *path;
    unsigned int one;
} DebugRequestsCtx;

// session `id` taken and a request's capacity with it: the capacity, or NULL with why given
EMBED_INLINE CliDebugAnswer *requests_begin(const DebugRequestsCtx *args)
{
    unsigned long long id = args->id;
    CliDebugSession **session = args->session_pointer;
    VerbaLinea *err_msg = args->err_msg;
    *session = EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx, .id = id, .err_msg = err_msg);
    return (*session != NULL) ? cli_debug_answer_alloc() : NULL;
}

// the request sent to the session taken and the session let go where it failed: 1, or 0 with why given
EMBED_INLINE int requests_send(const DebugRequestsCtx *args)
{
    CliDebugSession *session = args->session;
    const char *command = args->command;
    CliDebugAnswer *answer = args->answer;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = session,
                    .command = command, .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS, .err_msg = err_msg))
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
        return 0;
    }
    return 1;
}

// the request's capacity and the session let go
EMBED_INLINE int requests_finish(const DebugRequestsCtx *args)
{
    CliDebugSession *session = args->session;
    CliDebugAnswer *answer = args->answer;
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
    return 1;
}

// a string member of a node written as JSON, `fallback` where it is none, or null where `fallback` is NULL
EMBED_INLINE void requests_text(const DebugRequestsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    VerbaLinea *out = args->out;
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = node, .name = name),
                   .length_pointer = &length);
    if ((text == NULL) && (args->fallback == NULL))
    {
        mmgr_line_put(out, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)((text != NULL) ? text : args->fallback),
               .length = (text != NULL) ? length
                                        : cellul.len(&(CatenaFinitaCfg){.src = args->fallback, .cap = CLI_POOL_BYTES}));
}

// a signed member of a node written, 0 where it is none
EMBED_INLINE void requests_sign(const DebugRequestsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    long long value = 0LL;
    (void)EMBED_CALL(cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = json,
                     .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = node, .name = name),
                     .value = &value);
    mmgr_line_decimal_signed(args->out, value);
}

// a line counted from 1 written counted from 0, `fallback` where it is none, its low 32 bits retained as the window's
EMBED_INLINE void requests_line(const DebugRequestsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    unsigned long long line = args->fallback_u64;
    (void)EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                     .json = json,
                     .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = node, .name = name),
                     .value_u64s = &line);
    mmgr_line_decimal(args->out, ((line > 0ULL) ? line - 1ULL : 0ULL) & 0xffffffffULL, 1u);
}

// a path written by `named`, or as it came where none is given
EMBED_INLINE void requests_path(const DebugRequestsCtx *args)
{
    VerbaLinea *out = args->out;
    if (args->named != NULL)
    {
        EMBED_CALL(args->named, CliDebugPathWrittenCfg, .context = args->context, .path = args->path, .out = out);
        return;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
}

// a variable an adapter wrote, its name `name` where that is given
EMBED_INLINE void requests_variable(const DebugRequestsCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"name\":");
    if (name != NULL)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    }
    else
    {
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = json, .node = args->one, .name = "name", .fallback = "",
                   .out = out);
    }
    mmgr_line_put(out, ",\"value\":");
    const int has_value =
        (EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = args->one, .name = "value"),
                    .length_pointer = NULL) != NULL);
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = json, .node = args->one, .name = has_value ? "value" : "result",
               .fallback = "", .out = out);
    mmgr_line_put(out, ",\"kind\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = json, .node = args->one, .name = "type", .fallback = "",
               .out = out);
    mmgr_line_put(out, ",\"reference\":");
    EMBED_CALL(requests_sign, DebugRequestsCtx, .json = json, .node = args->one, .name = "variablesReference",
               .out = out);
    mmgr_line_put(out, ",\"memory\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = json, .node = args->one, .name = "memoryReference",
               .fallback = NULL, .out = out);
    mmgr_line_ch(out, '}');
}

int cli_debug_exceptions(const CliDebugJsonCfg *args)
{
    unsigned int on = args->on;
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"filters\":[");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = args->json, .array = on, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        mmgr_line_put(&answer->args.line, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = args->json, .array = on, .index = at),
                   .out = &answer->args.line, .indent = 0u, .sorted = 0);
    }
    mmgr_line_put(&answer->args.line, "]}");
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "setExceptionBreakpoints",
                    .answer = answer, .err_msg = args->err_msg))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < session->info.filter_count; at += 1u)
    {
        CliDebugFilter *const filter = &session->info.filters[at];
        filter->on = 0;
        for (unsigned int one = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = args->json, .array = on, .index = one) != CLI_JSON_NONE;
             one += 1u)
        {
            const char *const named =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = on, .index = one),
                           .length_pointer = NULL);
            filter->on =
                filter->on ||
                ((named != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = filter->filter, .cap = CLI_POOL_BYTES})));
        }
    }
    mmgr_line_put(args->out, "null");
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_raise(const CliDebugOutErrMsgIdThreadHowCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"threadId\":");
    mmgr_line_decimal_signed(&answer->args.line, args->thread);
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "exceptionInfo", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    mmgr_line_put(args->out, "{\"id\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = answer->root, .name = "exceptionId",
               .fallback = "", .out = args->out);
    mmgr_line_put(args->out, ",\"description\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = answer->root, .name = "description",
               .fallback = "", .out = args->out);
    mmgr_line_put(args->out, ",\"stack\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply,
               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                  .object = answer->root, .name = "details"),
               .name = "stackTrace", .fallback = "", .out = args->out);
    mmgr_line_ch(args->out, '}');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_threads(const CliDebugOutErrMsgIdStepCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if ((answer == NULL) || !EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "threads",
                                        .answer = answer, .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int threads = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                            .object = answer->root, .name = "threads");
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = threads, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = threads, .index = at);
        mmgr_line_put(args->out, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        EMBED_CALL(requests_sign, DebugRequestsCtx, .json = reply, .node = one, .name = "id", .out = args->out);
        mmgr_line_put(args->out, ",\"name\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "name", .fallback = "",
                   .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_stack(const CliDebugOutErrMsgIdThreadCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"threadId\":");
    mmgr_line_decimal_signed(&answer->args.line, args->thread);
    mmgr_line_put(&answer->args.line, ",\"startFrame\":0,\"levels\":200}");
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "stackTrace", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int frames = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                           .object = answer->root, .name = "stackFrames");
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = frames, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_debug_out_internal.frame_write, CliDebugOutCtx, .json = reply,
                   .frame = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                       .json_read = reply, .array = frames, .index = at),
                   .named = args->named, .context = args->context, .out = args->out);
    }
    mmgr_line_ch(args->out, ']');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

void cli_debug_frame_write(const CliDebugOutCtx *args)
{
    const char *const path = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                           .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = args->json, .object = args->frame, .name = "source"),
                           .name = "path"),
        .length_pointer = NULL);
    mmgr_line_put(args->out, "{\"id\":");
    EMBED_CALL(requests_sign, DebugRequestsCtx, .json = args->json, .node = args->frame, .name = "id",
               .out = args->out);
    mmgr_line_put(args->out, ",\"name\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = args->json, .node = args->frame, .name = "name", .fallback = "",
               .out = args->out);
    mmgr_line_put(args->out, ",\"path\":");
    if (path != NULL)
    {
        EMBED_CALL(requests_path, DebugRequestsCtx, .named = args->named, .context = args->context, .path = path,
                   .out = args->out);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"line\":");
    EMBED_CALL(requests_line, DebugRequestsCtx, .json = args->json, .node = args->frame, .name = "line",
               .fallback_u64 = 1ULL, .out = args->out);
    mmgr_line_put(args->out, ",\"col\":");
    EMBED_CALL(requests_line, DebugRequestsCtx, .json = args->json, .node = args->frame, .name = "column",
               .fallback_u64 = 1ULL, .out = args->out);
    mmgr_line_put(args->out, ",\"ip\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = args->json, .node = args->frame,
               .name = "instructionPointerReference", .fallback = NULL, .out = args->out);
    mmgr_line_ch(args->out, '}');
}

int cli_debug_scopes(const CliDebugOutErrMsgCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"frameId\":");
    mmgr_line_decimal_signed(&answer->args.line, args->frame);
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "scopes", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int scopes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                           .object = answer->root, .name = "scopes");
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = scopes, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = scopes, .index = at);
        mmgr_line_put(args->out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "name", .fallback = "",
                   .out = args->out);
        mmgr_line_put(args->out, ",\"reference\":");
        EMBED_CALL(requests_sign, DebugRequestsCtx, .json = reply, .node = one, .name = "variablesReference",
                   .out = args->out);
        mmgr_line_put(args->out, EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                               .json_read = reply, .object = one, .name = "expensive"))
                                     ? ",\"expensive\":true}"
                                     : ",\"expensive\":false}");
    }
    mmgr_line_ch(args->out, ']');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_variables(const CliDebugOutCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"variablesReference\":");
    mmgr_line_decimal_signed(&answer->args.line, args->reference);
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "variables", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int variables = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = reply, .object = answer->root, .name = "variables");
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = variables, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        EMBED_CALL(requests_variable, DebugRequestsCtx, .json = reply,
                   .one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = reply,
                                     .array = variables, .index = at),
                   .name = NULL, .out = args->out);
    }
    mmgr_line_ch(args->out, ']');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_evaluate(const CliDebugOutErrMsgCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    // with no frame chosen the frame is left out, which an adapter takes as its global scope
    mmgr_line_put(&answer->args.line, "{\"expression\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args.line,
               .text_bytes = (const unsigned char *)args->expression,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->expression, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&answer->args.line, ",\"context\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args.line,
               .text_bytes = (const unsigned char *)args->context_text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->context_text, .cap = CLI_POOL_BYTES}));
    if (args->has_frame)
    {
        mmgr_line_put(&answer->args.line, ",\"frameId\":");
        mmgr_line_decimal_signed(&answer->args.line, args->frame);
    }
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "evaluate", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(requests_variable, DebugRequestsCtx, .json = &answer->json, .one = answer->root,
               .name = args->expression, .out = args->out);
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_history(const CliDebugOutErrMsgIdCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"frameId\":");
    mmgr_line_decimal_signed(&answer->args.line, args->frame);
    mmgr_line_put(&answer->args.line, ",\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args.line,
               .text_bytes = (const unsigned char *)args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "orior/history", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int changes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                            .object = answer->root, .name = "changes");
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = changes, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = changes, .index = at);
        unsigned long long step = 0ULL;
        (void)EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                         .json = reply,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                            .object = one, .name = "step"),
                         .value_u64s = &step);
        mmgr_line_put(args->out, (at > 0u) ? ",{\"step\":" : "{\"step\":");
        mmgr_line_decimal(args->out, step, 1u);
        mmgr_line_put(args->out, ",\"line\":");
        EMBED_CALL(requests_line, DebugRequestsCtx, .json = reply, .node = one, .name = "line", .fallback_u64 = 1ULL,
                   .out = args->out);
        mmgr_line_put(args->out, ",\"path\":");
        EMBED_CALL(requests_path, DebugRequestsCtx, .named = args->named, .context = args->context,
                   .path = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                      .json = reply,
                                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                         .json_read = reply, .object = one, .name = "path"),
                                      .fallback = ""),
                   .out = args->out);
        mmgr_line_put(args->out, ",\"value\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "value", .fallback = NULL,
                   .out = args->out);
        mmgr_line_put(args->out, EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                               .json_read = reply, .object = one, .name = "before"))
                                     ? ",\"before\":true}"
                                     : ",\"before\":false}");
    }
    mmgr_line_ch(args->out, ']');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_goto(const CliDebugOutErrMsgIdStepCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"step\":");
    mmgr_line_decimal(&answer->args.line, args->step, 1u);
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "orior/goto", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_memory(const CliDebugOutErrMsgIdReferenceOffsetCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"memoryReference\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args.line,
               .text_bytes = (const unsigned char *)args->reference_text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->reference_text, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&answer->args.line, ",\"offset\":");
    mmgr_line_decimal_signed(&answer->args.line, args->offset);
    mmgr_line_put(&answer->args.line, ",\"count\":");
    mmgr_line_decimal(&answer->args.line, args->count, 1u);
    mmgr_line_ch(&answer->args.line, '}');
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "readMemory", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    unsigned long long length = 0ULL;
    const char *const data =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = answer->root, .name = "data"),
                   .length_pointer = &length);
    unsigned long long unreadable = 0ULL;
    (void)EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                     .json = reply,
                     .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                        .object = answer->root, .name = "unreadableBytes"),
                     .value_u64s = &unreadable);
    mmgr_line_put(args->out, "{\"address\":");
    EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = answer->root, .name = "address",
               .fallback = args->reference_text, .out = args->out);
    mmgr_line_put(args->out, ",\"bytes\":[");
    // the bytes read from memory, in a cell of the most the base64 text decodes to
    const unsigned long long most = ((length / 4ULL) + 1ULL) * 3ULL;
    unsigned char *const decoded = (data != NULL) ? g_cli_guard->temporary_buf_alloc(most, g_cli_debug_owner) : NULL;
    const unsigned long long bytes = (decoded != NULL)
                                         ? EMBED_CALL(cli_debug_text_internal.base64, CliDebugTextCtx, .text = data,
                                                      .size = length, .out_bytes = decoded, .capacity = most)
                                         : 0ULL;
    for (unsigned long long at = 0ULL; at < bytes; at += 1ULL)
    {
        mmgr_line_put(args->out, (at > 0ULL) ? "," : "");
        mmgr_line_decimal(args->out, decoded[at], 1u);
    }
    g_cli_guard->persistent_buf_release(decoded, g_cli_debug_owner);
    mmgr_line_put(args->out, "],\"unreadable\":");
    mmgr_line_decimal(args->out, unreadable, 1u);
    mmgr_line_ch(args->out, '}');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}

int cli_debug_instructions(const CliDebugOutErrMsgIdReferenceCfg *args)
{
    CliDebugSession *session = NULL;
    CliDebugAnswer *const answer = EMBED_CALL(requests_begin, DebugRequestsCtx, .id = args->id,
                                              .session_pointer = &session, .err_msg = args->err_msg);
    if (answer == NULL)
    {
        return 0;
    }
    mmgr_line_put(&answer->args.line, "{\"memoryReference\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args.line,
               .text_bytes = (const unsigned char *)args->reference_text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->reference_text, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&answer->args.line, ",\"instructionOffset\":");
    mmgr_line_decimal_signed(&answer->args.line, args->offset);
    mmgr_line_put(&answer->args.line, ",\"instructionCount\":");
    mmgr_line_decimal_signed(&answer->args.line, args->count_i64);
    mmgr_line_put(&answer->args.line, ",\"resolveSymbols\":true}");
    if (!EMBED_CALL(requests_send, DebugRequestsCtx, .session = session, .command = "disassemble", .answer = answer,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int instructions = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = reply, .object = answer->root, .name = "instructions");
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = reply, .array = instructions, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = instructions, .index = at);
        unsigned long long line = 0ULL;
        mmgr_line_put(args->out, (at > 0u) ? ",{\"address\":" : "{\"address\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "address", .fallback = "",
                   .out = args->out);
        mmgr_line_put(args->out, ",\"text\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "instruction", .fallback = "",
                   .out = args->out);
        mmgr_line_put(args->out, ",\"bytes\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "instructionBytes",
                   .fallback = "", .out = args->out);
        mmgr_line_put(args->out, ",\"symbol\":");
        EMBED_CALL(requests_text, DebugRequestsCtx, .json = reply, .node = one, .name = "symbol", .fallback = "",
                   .out = args->out);
        mmgr_line_put(args->out, ",\"line\":");
        if (EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                       .json = reply,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                          .object = one, .name = "line"),
                       .value_u64s = &line))
        {
            mmgr_line_decimal(args->out, ((line > 0ULL) ? line - 1ULL : 0ULL) & 0xffffffffULL, 1u);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
    return EMBED_CALL(requests_finish, DebugRequestsCtx, .session = session, .answer = answer);
}
