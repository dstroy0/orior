// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_deferred.c: a call answered later. Where its arguments carry `request`, a number the page chose, the call runs
// on a thread of its own and answers `{"deferred":true}` at once, its room given back; what it answers, or why it
// failed, goes to the page as the event `call-answered` with that number. Without `request` the call answers in place.
// At most CLI_SERVE_DEFERRED_MAX run at once; one more is refused at once with why given
#include "serve_internal.h"

#define CLI_SERVE_DEFERRED_MAX 32u
// the bytes an deferred call's answer and its event are written in
#define CLI_SERVE_DEFERRED_ROOM (16ull << 20u)

// A call running on its own: the thread, what answers it, the request's number, and the arguments as text
typedef struct
{
    CliThread thread;
    CliServeAnswer answer;
    unsigned long long request_id;
    unsigned long long size;
    char args[];
} CliServeDeferred;

static unsigned int s_running;

typedef struct
{
    unsigned long long request_id;
    int ok;
    const VerbaLinea *out;
    const VerbaLinea *err_msg;
} ServeDeferredCtx;
// the event of a request's answer sent: `{"request", "answer"}`, or `{"request", "error"}` with why
EMBED_INLINE void deferred_event_send(const ServeDeferredCtx *args)
{
    const VerbaLinea *out = args->out;
    const unsigned long long event_json_room = out->at + (args->err_msg->at * 6ull) + 64ull;
    char *const event_json_cell = g_cli_cells->temporary_buf_alloc(event_json_room, &g_cli_window_cells);
    VerbaLinea event_json = mmgr_line_over(event_json_cell, event_json_room);
    if (event_json.out == NULL)
    {
        return;
    }
    mmgr_line_put(&event_json, "{\"request\":");
    mmgr_line_decimal(&event_json, args->request_id, 1u);
    if (args->ok)
    {
        mmgr_line_put(&event_json, ",\"answer\":");
        if (out->at > 0ull)
        {
            mmgr_line_put_n(&event_json, out->out, out->at);
        }
        else
        {
            mmgr_line_put(&event_json, "null");
        }
    }
    else
    {
        mmgr_line_put(&event_json, ",\"error\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &event_json,
                   .text = args->err_msg->out, .length = args->err_msg->at);
    }
    mmgr_line_ch(&event_json, '}');
    if (mmgr_line_ok(&event_json))
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "call-answered", .body = event_json.out,
                   .size = event_json.at);
    }
    g_cli_cells->persistent_buf_release(event_json.out, &g_cli_window_cells);
}

// the call run where it was requested, its answer sent, and the request's cell given back
static void deferred_run(void *context)
{
    CliServeDeferred *const deferred = context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char *const out_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_DEFERRED_ROOM, &g_cli_window_cells);
    VerbaLinea out = mmgr_line_over(out_cell, CLI_SERVE_DEFERRED_ROOM);
    const int grown = out.out != NULL;
    if (!grown)
    {
        mmgr_line_put(&err_msg, "the answer could not be given a cell");
    }
    int ok = grown && EMBED_CALL(cli_serve_size_internal.answer, CliServeSizeCtx, .answer = deferred->answer,
                                 .args_text = deferred->args, .size = deferred->size, .out = &out, .err_msg = &err_msg);
    if (ok && !mmgr_line_ok(&out))
    {
        err_msg.at = 0ull;
        mmgr_line_put(&err_msg, "the answer does not fit the room it is made in");
        ok = 0;
    }
    EMBED_CALL(deferred_event_send, ServeDeferredCtx, .request_id = deferred->request_id, .ok = ok, .out = &out,
               .err_msg = &err_msg);
    if (grown)
    {
        g_cli_cells->persistent_buf_release(out.out, &g_cli_window_cells);
    }
    s_running -= 1u;
    g_cli_cells->persistent_buf_release(deferred, &g_cli_window_cells);
}

int cli_serve_deferred_or(const CliServeErrMsgOutRequestCtx *args)
{
    CliServeAnswer answer = args->answer;
    VerbaLinea *out = args->out;
    if (EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = args->request, .key = "request") ==
        CLI_JSON_NONE)
    {
        return EMBED_CALL(answer, CliServeAnswerCfg, .request = args->request, .out = out, .err_msg = args->err_msg);
    }
    unsigned long long request_id = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = args->request, .key = "request",
                    .value = &request_id, .err_msg = args->err_msg))
    {
        return 0;
    }
    // the arguments written again as text: the thread reads them once the room they came in is given back
    const unsigned long long text_room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = args->request->json);
    char *const text_cell = g_cli_cells->temporary_buf_alloc(text_room, &g_cli_window_cells);
    VerbaLinea text = mmgr_line_over(text_cell, text_room);
    if (text.out == NULL)
    {
        mmgr_line_put(args->err_msg, "the deferred call's arguments could not be given a cell");
        return 0;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->request->json,
               .node = args->request->object, .out = &text, .indent = 0u, .sorted = 0);
    CliServeDeferred *const deferred =
        mmgr_line_ok(&text) ? g_cli_cells->persistent_buf_alloc(sizeof(*deferred) + text.at + 1ull, &g_cli_window_cells)
                            : NULL;
    if (deferred == NULL)
    {
        g_cli_cells->persistent_buf_release(text.out, &g_cli_window_cells);
        mmgr_line_put(args->err_msg, "the deferred call could not be given a cell");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = deferred->args, .src = text.out, .bytes = text.at});
    deferred->args[text.at] = '\0';
    deferred->size = text.at;
    deferred->answer = answer;
    deferred->request_id = request_id;
    deferred->thread = (CliThread){deferred_run, deferred, CLI_PRIORITY_WORK};
    g_cli_cells->persistent_buf_release(text.out, &g_cli_window_cells);
    const int room = s_running < CLI_SERVE_DEFERRED_MAX;
    s_running += room ? 1u : 0u;
    if (!room)
    {
        g_cli_cells->persistent_buf_release(deferred, &g_cli_window_cells);
        mmgr_line_put(args->err_msg, "too many calls are deferred at once");
        return 0;
    }
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &deferred->thread))
    {
        s_running -= 1u;
        g_cli_cells->persistent_buf_release(deferred, &g_cli_window_cells);
        mmgr_line_put(args->err_msg, "the deferred call's thread did not start");
        return 0;
    }
    mmgr_line_put(out, "{\"deferred\":true}");
    return 1;
}
