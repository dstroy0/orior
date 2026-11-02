// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_watch.c: a stop where the bytes of a name change, as the adapter takes data breakpoints: each set again whole
// with each one added; and through lldb's own watchpoints where lldb-dap on Windows ends at reporting the stop of one
#include "../text/text.h"
#include "debug_internal.h"

typedef struct
{
    CliDebugSession *session;
    const char *data;
    VerbaLinea *err_msg;
#if (defined(_WIN32))
    const char *text;
    unsigned long long size;
    unsigned int *number;
#endif
} DebugWatchCtx;

#if defined(_WIN32)
// the spaces a text is trimmed of
static const char s_spaces[] = " \t\n\r\f\v";

// the watchpoint's number lldb names in what it answers, "Watchpoint created: Watchpoint 1: addr = ...", into
// `number`: 1, or 0 where it names none
EMBED_INLINE int watch_number(const DebugWatchCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int *number = args->number;
    static const char s_mark[] = "Watchpoint ";
    const unsigned long long mark = sizeof(s_mark) - 1u;
    // each piece of the text between the marks, the first before any, read to its first colon
    for (unsigned long long start = 0ull; start <= size;)
    {
        unsigned long long end = start;
        while ((end + mark <= size) &&
               (memor.cmp(&(MemoriaCfg){.src = text + end, .other = s_mark, .bytes = mark}) != 0))
        {
            end += 1ull;
        }
        end = (end + mark <= size) ? end : size;
        unsigned long long colon = start;
        colon = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = colon,
                           .size = end, .set = ":");
        unsigned long long first = start;
        unsigned long long last = colon;
        while ((first < last) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text[first]}) != NULL))
        {
            first += 1ull;
        }
        while ((last > first) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text[last - 1ull]}) != NULL))
        {
            last -= 1ull;
        }
        unsigned long long value = 0ull;
        int digits = (last > first) && (last - first <= 10ull);
        for (unsigned long long at = first; digits && (at < last); at += 1ull)
        {
            digits = (text[at] >= '0') && (text[at] <= '9');
            value = (value * 10ull) + (unsigned long long)(text[at] - '0');
        }
        if (digits && (value <= 0xffffffffull))
        {
            *number = (unsigned int)value;
            return 1;
        }
        if (end == size)
        {
            break;
        }
        start = end + mark;
    }
    return 0;
}

// the bytes lldb names as "address/size" watched by a watchpoint of lldb's own, whose stop orior's script turns into a
// stop at the next instruction the program runs: 1, or 0 with why given
EMBED_INLINE int watch_by_lldb(const DebugWatchCtx *args)
{
    CliDebugSession *session = args->session;
    VerbaLinea *err_msg = args->err_msg;
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = args->data, .cap = CLI_CELLS_POOL, .byte = '/'});
    if (slash == NULL)
    {
        mmgr_line_put(err_msg, "lldb named the bytes ");
        mmgr_line_put(err_msg, args->data);
        return 0;
    }
    const char *address = args->data;
    while ((address < slash) && (address[0] == '0') && ((address[1] == 'x') || (address[1] == 'X')))
    {
        address += 2;
    }
    char command[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea line = {command, sizeof(command), 0ull};
    mmgr_line_put(&line, "watchpoint set expression -w write -s ");
    mmgr_line_put(&line, slash + 1);
    mmgr_line_put(&line, " -- 0x");
    mmgr_line_put_n(&line, address, (unsigned long long)(slash - address));
    mmgr_line_finish(&line);
    char answer_buf[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea answer = {answer_buf, sizeof(answer_buf), 0ull};
    unsigned int number = 0u;
    if (!EMBED_CALL(cli_debug_session_err_msg_internal.lldb_script, CliDebugSessionErrMsgCtx, .session = session,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_debug_session_err_msg_internal.lldb_command, CliDebugSessionErrMsgCtx, .session = session,
                    .text = command, .out = &answer, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(watch_number, DebugWatchCtx, .text = answer.out, .size = answer.at, .number = &number))
    {
        unsigned long long start = 0ull;
        unsigned long long end = answer.at;
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = answer.out[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = answer.out[end - 1ull]}) != NULL))
        {
            end -= 1ull;
        }
        mmgr_line_put_n(err_msg, answer.out + start, end - start);
        return 0;
    }
    line.at = 0ull;
    mmgr_line_put(&line, "watchpoint command add -F orior_lldb.changed ");
    mmgr_line_decimal(&line, number, 1u);
    mmgr_line_finish(&line);
    answer.at = 0ull;
    if (!EMBED_CALL(cli_debug_session_err_msg_internal.lldb_command, CliDebugSessionErrMsgCtx, .session = session,
                    .text = command, .out = &answer, .err_msg = err_msg))
    {
        return 0;
    }
    session->watching = 1;
    return 1;
}
#endif

// the data breakpoint `data` added to the session's and all of them set again
EMBED_INLINE int watch_set(const DebugWatchCtx *args)
{
    CliDebugSession *session = args->session;
    VerbaLinea *err_msg = args->err_msg;
    CliDebugAnswer *const answer = cli_debug_answer_take();
    const int room = (session->data_count < session->data_room);
    if (room)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = session->data[session->data_count],
                          .src = args->data,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->data, .cap = CLI_CELLS_POOL}) + 1u});
        session->data_count += 1u;
    }
    mmgr_line_put(&answer->args, "{\"breakpoints\":[");
    for (unsigned int at = 0u; at < session->data_count; at += 1u)
    {
        mmgr_line_put(&answer->args, (at > 0u) ? ",{\"dataId\":" : "{\"dataId\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args,
                   .text_bytes = (const unsigned char *)session->data[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = session->data[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&answer->args, ",\"accessType\":\"write\"}");
    }
    mmgr_line_put(&answer->args, "]}");
    if (!room)
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        mmgr_line_put(err_msg, "the session watches as many bytes as it can already");
        return 0;
    }
    const int set = EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx,
                               .session = session, .command = "setDataBreakpoints", .answer = answer,
                               .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS, .err_msg = err_msg);
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    return set;
}

int cli_debug_watch_data(const CliDebugOutCfg *args)
{
    CliDebugSession *const session =
        EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx, .id = args->id, .err_msg = args->err_msg);
    if (session == NULL)
    {
        return 0;
    }
    CliDebugAnswer *const answer = cli_debug_answer_take();
    mmgr_line_put(&answer->args, "{\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args,
               .text_bytes = (const unsigned char *)args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
    if (args->has_reference)
    {
        mmgr_line_put(&answer->args, ",\"variablesReference\":");
        mmgr_line_decimal_signed(&answer->args, args->reference);
    }
    if (args->has_bytes)
    {
        mmgr_line_put(&answer->args, ",\"asAddress\":true,\"bytes\":");
        mmgr_line_decimal(&answer->args, args->bytes, 1u);
    }
    mmgr_line_ch(&answer->args, '}');
    if (!EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = session,
                    .command = "dataBreakpointInfo", .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS,
                    .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
        return 0;
    }
    const CliJson *const info = &answer->json;
    char data[CLI_NAME_MAX_LEN];
    char description[CLI_ERR_MSG_MAX_LEN];
    const char *const named =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = info,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = info,
                                      .object = answer->root, .name = "dataId"),
                   .length_pointer = NULL);
    const char *const described =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = info,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = info,
                                      .object = answer->root, .name = "description"),
                   .length_pointer = NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = data, .text = named, .otherwise = "");
    (void)cellul.copy(&(CatenaFinitaCfg){
        .dst = description, .src = (described != NULL) ? described : args->name, .cap = sizeof(description)});
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    if (named == NULL)
    {
        mmgr_line_put(args->err_msg, (described != NULL) ? description : "the adapter cannot watch that");
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
        return 0;
    }
    int watched = 0;
#if defined(_WIN32)
    // lldb-dap on Windows ends where it reports a stop at a watchpoint; the watchpoint is lldb's own, and its script
    // stops the program at the next instruction it runs
    if (cellul.eq(&(CatenaFinitaCfg){.src = session->program, .other = "lldb-dap", .cap = CLI_CELLS_POOL}))
    {
        watched = EMBED_CALL(watch_by_lldb, DebugWatchCtx, .session = session, .data = data, .err_msg = args->err_msg)
                      ? 1
                      : -1;
    }
#endif
    if (watched == 0)
    {
        watched =
            EMBED_CALL(watch_set, DebugWatchCtx, .session = session, .data = data, .err_msg = args->err_msg) ? 1 : -1;
    }
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
    if (watched < 0)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)description,
               .length = cellul.len(&(CatenaFinitaCfg){.src = description, .cap = CLI_CELLS_POOL}));
    return 1;
}
