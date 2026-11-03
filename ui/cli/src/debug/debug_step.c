// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_step.c: a session's steps, the step from the tree's Python into its own C and back out, lldb's own commands
// and orior's script in lldb, and a stop where the bytes of a name change
#include "../text/text.h"
#include "debug_internal.h"

// the spaces a text is trimmed of
static const char s_spaces[] = " \t\n\r\f\v";

int cli_debug_lldb_command(const CliDebugSessionErrMsgCtx *args)
{
    CliDebugAnswer *const answer = cli_debug_answer_alloc();
    char expression[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea command = {expression, sizeof(expression), 0ULL};
    mmgr_line_ch(&command, '`');
    mmgr_line_put(&command, args->text);
    mmgr_line_put(&answer->args, "{\"expression\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args,
               .text_bytes = (const unsigned char *)command.out, .length = command.at);
    mmgr_line_put(&answer->args, ",\"context\":\"repl\"}");
    if (!EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = args->session,
                    .command = "evaluate", .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS,
                    .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        return 0;
    }
    unsigned long long length = 0ULL;
    const char *const result =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &answer->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &answer->json, .object = answer->root, .name = "result"),
                   .length_pointer = &length);
    const char *const result_text = (result != NULL) ? result : "";
    const unsigned long long size = (result != NULL) ? length : 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long end = size;
    while ((start < size) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = result_text[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = result_text[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    const int failed = (end - start >= 6ULL) &&
                       (memor.cmp(&(MemoriaCfg){.src = result_text + start, .other = "error:", .bytes = 6u}) == 0);
    if (failed)
    {
        // lldb reports a command failed in the answer's text
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)result_text + start, .length = end - start);
    }
    else
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)result_text, .length = size);
    }
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    return !failed;
}

int cli_debug_lldb_script(const CliDebugSessionErrMsgCtx *args)
{
    const int scripted = args->session->scripted;
    if (scripted)
    {
        return 1;
    }
    char temporary[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = "orior-lldb", .joined = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "orior_lldb.py", .joined = script);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder) ||
        !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = script,
                    .bytes = g_cli_debug_lldb_script,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_debug_lldb_script, .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(args->err_msg, "orior's script for lldb could not be written at ");
        mmgr_line_put(args->err_msg, script);
        return 0;
    }
    char command[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea line = {command, sizeof(command), 0ULL};
    mmgr_line_put(&line, "command script import \"");
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = script, .from = '\\', .to = '/');
    mmgr_line_ch(&line, '"');
    mmgr_line_finish(&line);
    char out_buf[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea out = {out_buf, sizeof(out_buf), 0ULL};
    if (!EMBED_CALL(cli_debug_session_err_msg_internal.lldb_command, CliDebugSessionErrMsgCtx, .session = args->session,
                    .text = command, .out = &out, .err_msg = args->err_msg))
    {
        return 0;
    }
    args->session->scripted = 1;
    return 1;
}

typedef struct
{
    CliDebugSession *session;
    long long thread;
    VerbaLinea *err_msg;
    const char *how;
    int wait;
} DebugStepCtx;
// whether the native session taken steps out into the Python that called it: its caller is a frame lldb names by its
// module and symbol, which is no file
EMBED_INLINE int step_out_to_python(const DebugStepCtx *args)
{
    CliDebugSession *session = args->session;
    VerbaLinea *err_msg = args->err_msg;
    CliDebugAnswer *const answer = cli_debug_answer_alloc();
    mmgr_line_put(&answer->args, "{\"threadId\":");
    mmgr_line_decimal_signed(&answer->args, args->thread);
    mmgr_line_put(&answer->args, ",\"startFrame\":0,\"levels\":200}");
    if (!EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = session,
                    .command = "stackTrace", .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS,
                    .err_msg = err_msg))
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        return -1;
    }
    const CliJson *const reply = &answer->json;
    const unsigned int caller =
        EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = reply,
                   .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                       .object = answer->root, .name = "stackFrames"),
                   .index = 1u);
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = reply, .object = caller, .name = "source"),
                                      .name = "path"),
                   .length_pointer = NULL);
    const int out = (caller == CLI_JSON_NONE) || (path == NULL) ||
                    !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    return out;
}

// `how` sent to the session taken for `thread`, waited on where `wait`: 1, or 0 with why given
EMBED_INLINE int step_send(const DebugStepCtx *args)
{
    int wait = args->wait;
    VerbaLinea *err_msg = args->err_msg;
    char arguments[64];
    VerbaLinea args_local = {arguments, sizeof(arguments), 0ULL};
    mmgr_line_put(&args_local, "{\"threadId\":");
    mmgr_line_decimal_signed(&args_local, args->thread);
    mmgr_line_ch(&args_local, '}');
    if (wait)
    {
        CliLspAnswer answer;
        const int answered = EMBED_CALL(cli_dap_err_msg.request, CliDapErrMsgCfg, .dap = args->session->dap,
                                        .command = args->how, .arguments = args_local.out, .length = args_local.at,
                                        .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS, .answer = &answer, .err_msg = err_msg);
        g_cli_cells->persistent_buf_release(answer.message, &g_cli_debug_cells);
        return answered;
    }
    unsigned long long seq = 0ULL;
    if (!EMBED_CALL(cli_dap_err_msg_command.send, CliDapErrMsgCommandCfg, .dap = args->session->dap,
                    .command = args->how, .arguments = args_local.out, .length = args_local.at, .seq_pointer = &seq,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_dap_err_msg_command_seq.forgotten, CliDapErrMsgCommandSeqCfg, .dap = args->session->dap, .seq = seq);
    return 1;
}

int cli_debug_step(const CliDebugOutErrMsgIdThreadHowCfg *args)
{
    static const char *const s_hows[7] = {"continue", "next",     "stepIn",         "stepOut",
                                          "pause",    "stepBack", "reverseContinue"};
    int known = 0;
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        known = known || (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = s_hows[at], .cap = CLI_CELLS_POOL}));
    }
    if (!known)
    {
        mmgr_line_put(args->err_msg, args->how);
        mmgr_line_put(args->err_msg, " is not a step");
        return 0;
    }
    CliDebugSession *const session =
        EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx, .id = args->id, .err_msg = args->err_msg);
    if (session == NULL)
    {
        return 0;
    }
    const int has_parent = session->info.has_parent;
    const unsigned long long parent = session->info.parent;
    // a step out of the tree's C into the Python that called it lets the native code run on and stops the Python at
    // the next line it runs
    CliDebugSession *const sibling =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "stepOut", .cap = CLI_CELLS_POOL})) && has_parent
            ? EMBED_CALL(cli_debug_err_msg_id_internal.native_get, CliDebugErrMsgIdCtx, .id = parent)
            : NULL;
    const int across = (sibling != NULL) && (sibling == session);
    if (sibling != NULL)
    {
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = sibling);
    }
    if (across)
    {
        const int leaves = EMBED_CALL(step_out_to_python, DebugStepCtx, .session = session, .thread = args->thread,
                                      .err_msg = args->err_msg);
        if (leaves != 0)
        {
            CliDebugSession *const python = (leaves > 0)
                                                ? EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx,
                                                             .id = parent, .err_msg = args->err_msg)
                                                : NULL;
            // debugpy pauses every thread, whichever is named
            const int stepped = (python != NULL) &&
                                EMBED_CALL(step_send, DebugStepCtx, .session = session, .how = "continue",
                                           .thread = args->thread, .wait = 1, .err_msg = args->err_msg) &&
                                EMBED_CALL(step_send, DebugStepCtx, .session = python, .how = "pause",
                                           .thread = args->thread, .wait = 0, .err_msg = args->err_msg);
            if (python != NULL)
            {
                EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = python);
            }
            EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
            mmgr_line_put(args->out, stepped ? "null" : "");
            return stepped;
        }
    }
    // the C a Python step runs can stop the whole process at a native breakpoint before the Python's debugger, which
    // answers from inside the process, reports the step began; in a session whose process a native session also debugs,
    // the step is sent and not waited on
    CliDebugSession *const native =
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "pause", .cap = CLI_CELLS_POOL}))
            ? EMBED_CALL(cli_debug_err_msg_id_internal.native_get, CliDebugErrMsgIdCtx, .id = args->id)
            : NULL;
    const int stepped = EMBED_CALL(step_send, DebugStepCtx, .session = session, .how = args->how,
                                   .thread = args->thread, .wait = native == NULL, .err_msg = args->err_msg);
    if (native != NULL)
    {
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = native);
    }
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = session);
    if (stepped)
    {
        mmgr_line_put(args->out, "null");
    }
    return stepped;
}

int cli_debug_step_across(const CliDebugRootOutErrMsgCfg *args)
{
    CliDebugSession *const python =
        EMBED_CALL(cli_debug_err_msg_id_internal.take, CliDebugErrMsgIdCtx, .id = args->id, .err_msg = args->err_msg);
    if (python == NULL)
    {
        return 0;
    }
    CliDebugSession *const native =
        EMBED_CALL(cli_debug_err_msg_id_internal.native_get, CliDebugErrMsgIdCtx, .id = args->id);
    if (native == NULL)
    {
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = python);
        return EMBED_CALL(cli_debug_out_err_msg_id_thread_how.step, CliDebugOutErrMsgIdThreadHowCfg, .id = args->id,
                          .how = "stepIn", .thread = args->thread, .out = args->out, .err_msg = args->err_msg);
    }
    char command[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {command, sizeof(command), 0ULL};
    char slashed[CLI_PATH_MAX_LEN];
    VerbaLinea slashed_line = {slashed, sizeof(slashed), 0ULL};
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &slashed_line, .text = args->root, .from = '\\',
                     .to = '/');
    mmgr_line_put(&line, "script print(orior_lldb.across(lldb.debugger.GetSelectedTarget(), ");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &line,
               .text_bytes = (const unsigned char *)slashed_line.out, .length = slashed_line.at);
    mmgr_line_put(&line, ", \"" CLI_DEBUG_ACROSS "\"))");
    mmgr_line_finish(&line);
    char count_buf[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea count = {count_buf, sizeof(count_buf), 0ULL};
    if (!EMBED_CALL(cli_debug_session_err_msg_internal.lldb_script, CliDebugSessionErrMsgCtx, .session = native,
                    .err_msg = args->err_msg) ||
        !EMBED_CALL(cli_debug_session_err_msg_internal.lldb_command, CliDebugSessionErrMsgCtx, .session = native,
                    .text = command, .out = &count, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = native);
        EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = python);
        return 0;
    }
    // the count of places the script set a breakpoint at, the whole answer trimmed, 0 where it is no number
    unsigned long long start = 0ULL;
    unsigned long long end = count.at;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = count.out[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = count.out[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    unsigned long long positioned = 0ULL;
    int number = (end > start) && (end - start <= 19ULL);
    {
        unsigned long long at = start;
        while (number && (at < end))
        {
            number = (count.out[at] >= '0') && (count.out[at] <= '9');
            positioned = (positioned * 10ULL) + (unsigned long long)(count.out[at] - '0');
            at += 1ULL;
        }
    }
    positioned = number ? positioned : 0ULL;
    if (positioned > 0ULL)
    {
        python->has_across = 1;
        python->across = native->info.id;
        native->has_across = 1;
        native->across = native->info.id;
    }
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = native);
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = python);
    return EMBED_CALL(cli_debug_out_err_msg_id_thread_how.step, CliDebugOutErrMsgIdThreadHowCfg, .id = args->id,
                      .how = "stepIn", .thread = args->thread, .out = args->out, .err_msg = args->err_msg);
}
