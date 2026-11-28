// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts_server.c: what the language server of a script plugin sends, taken to the plugin's host (scripts_internal.h).
// The servers module sends each message as the event `plugin-server`. The event goes into a queue, and one worker
// thread takes the queue in order. The worker asks the host with the source `server`. Where the message is a request,
// the worker answers the server with the host's answer, or null where the host has none
#include "../command_line/pool.h"
#include "../servers/lsp.h"
#include "../servers/servers.h"
#include "scripts_internal.h"

// the most messages that wait in the queue, and the most bytes of the host's answer to one
#define CLI_SCRIPTS_SERVER_QUEUE 64u
#define CLI_SCRIPTS_SERVER_ANSWER (1ULL << 20u)

// One message in the queue: the event's JSON in a cell of the queue's own, and its bytes
typedef struct
{
    char *json;
    unsigned long long size;
} ScriptsServerMessage;

// the queue under its gate, the wake that its worker waits on, and the worker, whether it runs
static ScriptsServerMessage s_queue[CLI_SCRIPTS_SERVER_QUEUE];
static unsigned int s_first;
static unsigned int s_count;
static CliGate s_queue_gate = CLI_GATE_INIT;
static CliWake s_queue_wake = CLI_WAKE_INIT;
static CliThread s_worker;
static int s_working;

typedef struct
{
    const char *body;
    unsigned long long size;
    const char *name;
    char *out;
    unsigned long long capacity;
    unsigned long long *start;
    unsigned long long *length;
} ScriptsServerCtx;

// the member `name` of the event's JSON: where its JSON starts and its bytes. 1, or 0 where the event has none
EMBED_INLINE int server_member(const ScriptsServerCtx *args)
{
    return EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->body, .size_value = args->size,
                      .name = args->name, .start = args->start, .length_pointer = args->length);
}

// the string member `name` of the event's JSON into `out`, sized for `capacity`: 1, or 0 where it is no string or
// does not fit
EMBED_INLINE int server_string(const ScriptsServerCtx *args)
{
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    return EMBED_CALL(server_member, ScriptsServerCtx, .body = args->body, .size = args->size, .name = args->name,
                      .start = &start, .length = &length) &&
           EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->body + start, .length = length,
                      .out_capacity = args->out, .capacity = args->capacity);
}

// One message taken to the host of its plugin. A request of the server gets the host's answer
EMBED_INLINE void server_taken(const ScriptsServerCtx *args)
{
    char id[CLI_NAME_MAX_LEN];
    char method[256];
    unsigned long long params = 0ULL;
    unsigned long long params_size = 0ULL;
    unsigned long long request = 0ULL;
    unsigned long long request_size = 0ULL;
    if (!EMBED_CALL(server_string, ScriptsServerCtx, .body = args->body, .size = args->size, .name = "plugin",
                    .out = id, .capacity = sizeof(id)) ||
        !EMBED_CALL(server_string, ScriptsServerCtx, .body = args->body, .size = args->size, .name = "method",
                    .out = method, .capacity = sizeof(method)) ||
        !EMBED_CALL(server_member, ScriptsServerCtx, .body = args->body, .size = args->size, .name = "params",
                    .start = &params, .length = &params_size) ||
        !EMBED_CALL(server_member, ScriptsServerCtx, .body = args->body, .size = args->size, .name = "request",
                    .start = &request, .length = &request_size))
    {
        return;
    }
    char *const answer_text = g_platform_guard->temporary_buf_alloc(CLI_SCRIPTS_SERVER_ANSWER, g_cli_scripts_owner);
    VerbaLinea answer = mmgr_line_over(answer_text, (answer_text != NULL) ? CLI_SCRIPTS_SERVER_ANSWER : 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int answered =
        EMBED_CALL(cli_scripts.ask, CliScriptsCfg, .id = id, .source = CLI_SCRIPTS_SERVER, .name = method,
                   .value = args->body + params, .length = params_size, .answer = &answer, .err_msg = &err_msg);
    const int is_request =
        !((request_size == 4ULL) &&
          cellul.starts(&(CatenaFinitaCfg){.src = args->body + request, .other = "null", .cap = 4u}));
    char request_text[64];
    if (is_request && (request_size < sizeof(request_text)))
    {
        memor.cpy(&(MemoriaCfg){.dst = request_text, .src = args->body + request, .bytes = (embed_word)request_size});
        request_text[request_size] = '\0';
        const int has = answered && mmgr_line_ok(&answer) && (answer.at > 0ULL);
        err_msg.at = 0ULL;
        (void)EMBED_CALL(cli_servers_err_msg_id.plugin_answer, CliServersErrMsgIdCfg, .err_msg = &err_msg, .id = id,
                         .request = request_text, .result_text = has ? answer_text : "null",
                         .length = has ? answer.at : 4ULL);
    }
    g_platform_guard->persistent_buf_release(answer_text, g_cli_scripts_owner);
}

// the worker: each message of the queue taken in order, and a wait while the queue is empty
static void server_work(void *context)
{
    (void)context;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_queue_wake);
        cli_gate_shut(&s_queue_gate);
        const int has = s_count > 0u;
        const ScriptsServerMessage message = has ? s_queue[s_first] : (ScriptsServerMessage){NULL, 0ULL};
        s_first = has ? ((s_first + 1u) % CLI_SCRIPTS_SERVER_QUEUE) : s_first;
        s_count -= has ? 1u : 0u;
        cli_gate_opened(&s_queue_gate);
        if (!has)
        {
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_queue_wake, .seen = seen);
            continue;
        }
        EMBED_CALL(server_taken, ScriptsServerCtx, .body = message.json, .size = message.size);
        g_platform_guard->persistent_buf_release(message.json, g_cli_scripts_owner);
    }
}

void cli_scripts_server_emit(const CliEachEventJsonSizeCfg *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->event, .other = "plugin-server", .cap = 16u}))
    {
        return;
    }
    char *const json = g_platform_guard->persistent_buf_alloc(args->size + 1ULL, g_cli_scripts_owner);
    if (json == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = json, .src = args->json, .bytes = (embed_word)args->size});
    json[args->size] = '\0';
    cli_gate_shut(&s_queue_gate);
    const int queued = s_count < CLI_SCRIPTS_SERVER_QUEUE;
    if (queued)
    {
        s_queue[(s_first + s_count) % CLI_SCRIPTS_SERVER_QUEUE] = (ScriptsServerMessage){json, args->size};
        s_count += 1u;
    }
    if (!s_working)
    {
        s_worker = (CliThread){server_work, NULL, 0u};
        s_working = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_worker);
    }
    cli_gate_opened(&s_queue_gate);
    if (!queued)
    {
        g_platform_guard->persistent_buf_release(json, g_cli_scripts_owner);
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
}
