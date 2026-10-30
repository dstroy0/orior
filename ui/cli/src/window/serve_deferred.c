// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_deferred.c: a call answered later. Where its arguments carry `request`, a number the page chose, the call runs
// on a thread of its own and answers `{"deferred":true}` at once, its room given back; what it answers, or why it
// failed, goes to the page as the event `call-answered` with that number. Without `request` the call answers in place.
// At most CLI_SERVE_DEFERRED_MAX run at once; one more is refused at once with why given
#include "serve_internal.h"

#define CLI_SERVE_DEFERRED_MAX 32u
// the bytes an deferred call's answer and its event are begun to be written in, doubling as they fill
#define CLI_SERVE_DEFERRED_INIT_CAP 4096ull

// A call running on its own: the thread, what answers it, the request's number, and the arguments as text
typedef struct
{
    CliThread thread;
    CliServeAnswer answer;
    unsigned long long request_id;
    unsigned long long size;
    char args[];
} CliServeDeferred;

static CliLock s_lock = CLI_LOCK_INIT;
static unsigned int s_running;

// the event of a request's answer sent: `{"request", "answer"}`, or `{"request", "error"}` with why
static void deferred_event_sent(unsigned long long request_id, int ok, const VerbaLinea *out, const VerbaLinea *err_msg)
{
    VerbaLinea event_json;
    if (!cli_growing_line_made(&event_json, CLI_SERVE_DEFERRED_INIT_CAP))
    {
        return;
    }
    mmgr_line_put(&event_json, "{\"request\":");
    mmgr_line_decimal(&event_json, request_id, 1u);
    if (ok)
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
        cli_json_quoted(&event_json, err_msg->out, err_msg->at);
    }
    mmgr_line_ch(&event_json, '}');
    if (mmgr_line_ok(&event_json))
    {
        cli_serve_emitted("call-answered", event_json.out, event_json.at);
    }
    cli_cell_release(event_json.out, NULL);
}

// the call run where it was requested, its answer sent, and the request's cell given back
static void deferred_run(void *context)
{
    CliServeDeferred *const deferred = context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out;
    const int grown = cli_growing_line_made(&out, CLI_SERVE_DEFERRED_INIT_CAP);
    if (!grown)
    {
        mmgr_line_put(&err_msg, "the answer could not be given a cell");
    }
    int ok = grown && cli_serve_answered(deferred->answer, deferred->args, deferred->size, &out, &err_msg);
    if (ok && !mmgr_line_ok(&out))
    {
        err_msg.at = 0ull;
        mmgr_line_put(&err_msg, "the answer does not fit the room it is made in");
        ok = 0;
    }
    deferred_event_sent(deferred->request_id, ok, &out, &err_msg);
    if (grown)
    {
        cli_cell_release(out.out, NULL);
    }
    cli_lock_take(&s_lock);
    s_running -= 1u;
    cli_lock_give(&s_lock);
    cli_cell_release(deferred, NULL);
}

int cli_serve_deferred_or(const CliServeArgs *args, CliServeAnswer answer, VerbaLinea *out, VerbaLinea *err_msg)
{
    if (cli_serve_arg(args, "request") == CLI_JSON_NONE)
    {
        return answer(args, out, err_msg);
    }
    unsigned long long request_id = 0ull;
    if (!cli_serve_whole(args, "request", &request_id, err_msg))
    {
        return 0;
    }
    // the arguments written again as text, so the thread reads them once the room they came in is given back
    VerbaLinea text;
    if (!cli_growing_line_made(&text, CLI_SERVE_DEFERRED_INIT_CAP))
    {
        mmgr_line_put(err_msg, "the deferred call's arguments could not be given a cell");
        return 0;
    }
    cli_json_write(args->json, args->object, &text, 0u, 0);
    CliServeDeferred *const deferred =
        mmgr_line_ok(&text) ? g_cli_cells->persistent_buf_alloc(sizeof(*deferred) + text.at + 1ull, NULL) : NULL;
    if (deferred == NULL)
    {
        cli_cell_release(text.out, NULL);
        mmgr_line_put(err_msg, "the deferred call could not be given a cell");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = deferred->args, .src = text.out, .bytes = text.at});
    deferred->args[text.at] = '\0';
    deferred->size = text.at;
    deferred->answer = answer;
    deferred->request_id = request_id;
    deferred->thread = (CliThread){deferred_run, deferred, CLI_PRIORITY_WORK};
    cli_cell_release(text.out, NULL);
    cli_lock_take(&s_lock);
    const int room = s_running < CLI_SERVE_DEFERRED_MAX;
    s_running += room ? 1u : 0u;
    cli_lock_give(&s_lock);
    if (!room)
    {
        cli_cell_release(deferred, NULL);
        mmgr_line_put(err_msg, "too many calls are deferred at once");
        return 0;
    }
    if (!cli_thread_started(&deferred->thread))
    {
        cli_lock_take(&s_lock);
        s_running -= 1u;
        cli_lock_give(&s_lock);
        cli_cell_release(deferred, NULL);
        mmgr_line_put(err_msg, "the deferred call's thread did not start");
        return 0;
    }
    mmgr_line_put(out, "{\"deferred\":true}");
    return 1;
}
