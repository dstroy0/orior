// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_requests.c: what orior_cli requests of a running kernel: requests sent and their replies waited for, code
// run, what it runs interrupted, whether it still runs, and the kernel stopped, its process ended where it does not
// stop
#include "jupyter_internal.h"

// how often a stopping kernel is looked at, in milliseconds
#define CLI_KERNEL_STOP_LOOK 50u

void cli_kernel_end(const CliJupyterAtCtx *args)
{
    if (!args->kernel->ended && (args->kernel->spawned.process != 0ull))
    {
#if defined(_WIN32)
        EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = args->kernel->spawned.id);
#endif
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &args->kernel->spawned);
        args->kernel->code =
            EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &args->kernel->spawned);
        args->kernel->ended = 1;
    }
}

void cli_kernel_forgotten(const CliJupyterAtCtx *args)
{
    if (args->kernel->connection[0] != '\0')
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = args->kernel->connection);
        args->kernel->connection[0] = '\0';
    }
    EMBED_CALL(cli_event_internal.event_close, CliEventCtx, .event = args->kernel->interrupt_event);
    args->kernel->interrupt_event = 0ull;
}

typedef struct
{
    CliKernel *kernel;
    const char *channel;
} JupyterRequestsCtx;
// the socket of the channel named, the control socket for "control" and the shell socket for any other, and the lock
// a write to it is made under
EMBED_INLINE const CliSocket *jupyter_socket_get(const JupyterRequestsCtx *args)
{
    const int control =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->channel, .other = "control", .cap = CLI_CELLS_POOL}));
    return control ? &args->kernel->control : &args->kernel->shell;
}

int cli_jupyter_send(const CliJupyterErrMsgKernelCfg *args)
{
    EMBED_CALL(cli_jupyter_session_internal.kernel_id_init, CliJupyterSessionCtx, .session = args->kernel->session,
               .id = args->id);
    const CliSocket *const socket =
        EMBED_CALL(jupyter_socket_get, JupyterRequestsCtx, .kernel = args->kernel, .channel = args->channel);
    if (!EMBED_CALL(cli_jupyter_kernel_internal.kernel_frames_send, CliJupyterKernelCtx, .kernel = args->kernel,
                    .socket = socket, .id_read = args->id, .msg_type = args->msg_type, .content = args->content,
                    .length = args->length))
    {
        mmgr_line_put(args->err_msg, "the kernel's socket could not be written");
        return 0;
    }
    return 1;
}

int cli_jupyter_request(const CliJupyterErrMsgCfg *args)
{
    VerbaLinea *reply = args->reply;
    char id[CLI_KERNEL_ID];
    EMBED_CALL(cli_jupyter_session_internal.kernel_id_init, CliJupyterSessionCtx, .session = args->kernel->session,
               .id = id);
    CliKernelWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_KERNEL_WAITERS) && (waiter == NULL); at += 1u)
    {
        if (!args->kernel->waiters[at].waiting)
        {
            waiter = &args->kernel->waiters[at];
            waiter->waiting = 1;
            waiter->answered = 0;
            waiter->reply = reply;
            memor.cpy(&(MemoriaCfg){.dst = waiter->id, .src = id, .bytes = sizeof(id)});
        }
    }
    if (waiter == NULL)
    {
        mmgr_line_put(args->err_msg, "no more requests wait on the kernel at once");
        return 0;
    }
    const CliSocket *const socket =
        EMBED_CALL(jupyter_socket_get, JupyterRequestsCtx, .kernel = args->kernel, .channel = args->channel);
    const int sent = EMBED_CALL(cli_jupyter_kernel_internal.kernel_frames_send, CliJupyterKernelCtx,
                                .kernel = args->kernel, .socket = socket, .id_read = id, .msg_type = args->msg_type,
                                .content = args->content, .length = args->length);
    const unsigned long long until = cli_clock_milliseconds() + args->wait;
    while (sent && !waiter->answered && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->kernel->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int answered = waiter->answered;
    waiter->waiting = 0;
    if (!sent)
    {
        mmgr_line_put(args->err_msg, "the kernel's socket could not be written");
        return 0;
    }
    if (!answered)
    {
        mmgr_line_put(args->err_msg, "the kernel did not answer ");
        mmgr_line_put(args->err_msg, args->msg_type);
        return 0;
    }
    return 1;
}

int cli_jupyter_execute(const CliJupyterErrMsgKernelLengthCfg *args)
{
    VerbaLinea content = {args->kernel->content, CLI_KERNEL_CONTENT, 0ull};
    mmgr_line_put(&content, "{\"code\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &content, .text = args->code,
               .length = args->length);
    mmgr_line_put(&content, ",\"silent\":false,\"store_history\":true,\"user_expressions\":{},\"allow_stdin\":false,"
                            "\"stop_on_error\":true}");
    int sent = 0;
    if (!mmgr_line_ok(&content))
    {
        mmgr_line_put(args->err_msg, "the code is larger than orior_cli supports");
    }
    else
    {
        sent = EMBED_CALL(cli_jupyter_err_msg_kernel.send, CliJupyterErrMsgKernelCfg, .kernel = args->kernel,
                          .channel = "shell", .msg_type = "execute_request", .content = content.out,
                          .length = content.at, .id = args->id, .err_msg = args->err_msg);
    }
    return sent;
}

int cli_jupyter_interrupt(const CliJupyterErrMsgKernelFileCfg *args)
{
    if (cellul.eq(
            &(CatenaFinitaCfg){.src = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                 .spec = &args->kernel->spec, .at = args->kernel->spec.interrupt_mode),
                               .other = "message",
                               .cap = CLI_CELLS_POOL}))
    {
        char answer[65536];
        VerbaLinea reply = {answer, sizeof(answer), 0ull};
        return EMBED_CALL(cli_jupyter_err_msg.request, CliJupyterErrMsgCfg, .kernel = args->kernel,
                          .channel = "control", .msg_type = "interrupt_request", .content = "{}", .length = 2ull,
                          .wait = 5000ull, .reply = &reply, .err_msg = args->err_msg);
    }
#if defined(_WIN32)
    if (!EMBED_CALL(cli_event_internal.event_set, CliEventCtx, .event = args->kernel->interrupt_event))
    {
        mmgr_line_put(args->err_msg, "the kernel's interrupt event could not be signaled");
        return 0;
    }
#else
    // what kill reports of the signal is not read, as the reader is informed of no failure of it
    (void)EMBED_CALL(cli_cols_internal.spawn_interrupt, CliColsCtx, .spawned_read = &args->kernel->spawned);
#endif
    return 1;
}

int cli_jupyter_alive(const CliJupyterErrMsgCfg *args)
{
    long long code = 0ll;
    if (!args->kernel->ended &&
        EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &args->kernel->spawned, .code = &code))
    {
        args->kernel->ended = 1;
        args->kernel->code = code;
    }
    const int alive = !args->kernel->ended;
    return alive;
}

int cli_jupyter_stop(const CliJupyterErrMsgCfg *args)
{
    char id[CLI_KERNEL_ID];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unsent = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(cli_jupyter_err_msg_kernel.send, CliJupyterErrMsgKernelCfg, .kernel = args->kernel,
                     .channel = "control", .msg_type = "shutdown_request", .content = "{\"restart\":false}",
                     .length = 17ull, .id = id, .err_msg = &unsent);
    const unsigned long long until = cli_clock_milliseconds() + args->wait;
    while (cli_clock_milliseconds() < until)
    {
        if (!cli_jupyter_err_msg.alive(args))
        {
            EMBED_CALL(cli_jupyter_at_internal.kernel_forgotten, CliJupyterAtCtx, .kernel = args->kernel);
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_KERNEL_STOP_LOOK);
    }
    EMBED_CALL(cli_jupyter_at_internal.kernel_end, CliJupyterAtCtx, .kernel = args->kernel);
    EMBED_CALL(cli_jupyter_at_internal.kernel_forgotten, CliJupyterAtCtx, .kernel = args->kernel);
    return 1;
}
