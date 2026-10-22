// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_requests.c: what orior_cli asks of a running kernel: requests sent and their replies waited for, code run, what
// it runs interrupted, whether it still runs, and the kernel stopped, its process ended where it does not stop
#include "jupyter_internal.h"

// how often a stopping kernel is looked at, in milliseconds
#define CLI_KERNEL_STOP_LOOK 50u

void cli_kernel_ended(CliKernel *kernel)
{
    cli_lock_take(&kernel->lock);
    if (!kernel->ended && (kernel->spawned.process != 0ull))
    {
#if defined(_WIN32)
        cli_spawn_tree_end(kernel->spawned.id);
#endif
        cli_spawn_killed(&kernel->spawned);
        kernel->code = cli_spawn_wait(&kernel->spawned);
        kernel->ended = 1;
    }
    cli_lock_give(&kernel->lock);
}

void cli_kernel_forgotten(CliKernel *kernel)
{
    if (kernel->connection[0] != '\0')
    {
        (void)query_removed(kernel->connection);
        kernel->connection[0] = '\0';
    }
    cli_event_closed(kernel->interrupt_event);
    kernel->interrupt_event = 0ull;
}

// the socket of the channel named, the control socket for "control" and the shell socket for any other, and the lock
// a write to it is made under
static const CliSocket *jupyter_socket_of(CliKernel *kernel, const char *channel, CliLock **lock)
{
    const int control = (cellul.eq(&(CatenaFinitaCfg){.src = channel, .other = "control", .cap = CLI_CELLS_POOL}));
    *lock = control ? &kernel->control_lock : &kernel->shell_lock;
    return control ? &kernel->control : &kernel->shell;
}

int cli_jupyter_sent(CliKernel *kernel, const char *channel, const char *msg_type, const char *content,
                     unsigned long long length, char *id, CliLine *said)
{
    cli_kernel_id_made(kernel->session, id);
    CliLock *lock = NULL;
    const CliSocket *const socket = jupyter_socket_of(kernel, channel, &lock);
    if (!cli_kernel_frames_sent(kernel, socket, lock, id, msg_type, content, length))
    {
        cli_line_text(said, "the kernel's socket could not be written");
        return 0;
    }
    return 1;
}

int cli_jupyter_requested(CliKernel *kernel, const char *channel, const char *msg_type, const char *content,
                          unsigned long long length, unsigned long long wait, CliLine *reply, CliLine *said)
{
    char id[CLI_KERNEL_ID];
    cli_kernel_id_made(kernel->session, id);
    cli_lock_take(&kernel->lock);
    CliKernelWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_KERNEL_WAITERS) && (waiter == NULL); at += 1u)
    {
        if (!kernel->waiters[at].waiting)
        {
            waiter = &kernel->waiters[at];
            waiter->waiting = 1;
            waiter->answered = 0;
            waiter->reply = reply;
            memor.cpy(&(MemoriaCfg){.dst = waiter->id, .src = id, .bytes = sizeof(id)});
        }
    }
    cli_lock_give(&kernel->lock);
    if (waiter == NULL)
    {
        cli_line_text(said, "no more requests wait on the kernel at once");
        return 0;
    }
    CliLock *lock = NULL;
    const CliSocket *const socket = jupyter_socket_of(kernel, channel, &lock);
    const int sent = cli_kernel_frames_sent(kernel, socket, lock, id, msg_type, content, length);
    const unsigned long long until = cli_clock_milliseconds() + wait;
    cli_lock_take(&kernel->lock);
    while (sent && !waiter->answered && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&kernel->wake, &kernel->lock, until - cli_clock_milliseconds());
    }
    const int answered = waiter->answered;
    waiter->waiting = 0;
    cli_lock_give(&kernel->lock);
    if (!sent)
    {
        cli_line_text(said, "the kernel's socket could not be written");
        return 0;
    }
    if (!answered)
    {
        cli_line_text(said, "the kernel did not answer ");
        cli_line_text(said, msg_type);
        return 0;
    }
    return 1;
}

int cli_jupyter_executed(CliKernel *kernel, const char *code, unsigned long long length, char *id, CliLine *said)
{
    cli_lock_take(&kernel->content_lock);
    CliLine content = {kernel->content, sizeof(kernel->content), 0ull};
    cli_line_text(&content, "{\"code\":");
    cli_json_quoted(&content, code, length);
    cli_line_text(&content, ",\"silent\":false,\"store_history\":true,\"user_expressions\":{},\"allow_stdin\":false,"
                            "\"stop_on_error\":true}");
    int sent = 0;
    if (!cli_line_fits(&content))
    {
        cli_line_text(said, "the code is larger than orior_cli holds");
    }
    else
    {
        sent = cli_jupyter_sent(kernel, "shell", "execute_request", content.out, content.at, id, said);
    }
    cli_lock_give(&kernel->content_lock);
    return sent;
}

int cli_jupyter_interrupted(CliKernel *kernel, CliLine *said)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = cli_kernel_text(&kernel->spec, kernel->spec.interrupt_mode),
                                     .other = "message",
                                     .cap = CLI_CELLS_POOL}))
    {
        char answer[65536];
        CliLine reply = {answer, sizeof(answer), 0ull};
        return cli_jupyter_requested(kernel, "control", "interrupt_request", "{}", 2ull, 5000ull, &reply, said);
    }
#if defined(_WIN32)
    if (!cli_event_set(kernel->interrupt_event))
    {
        cli_line_text(said, "the kernel's interrupt event could not be signaled");
        return 0;
    }
#else
    // what kill says of the signal is not read, as the reader is told of no failure of it
    (void)cli_spawn_interrupted(&kernel->spawned);
#endif
    return 1;
}

int cli_jupyter_alive(CliKernel *kernel)
{
    cli_lock_take(&kernel->lock);
    long long code = 0ll;
    if (!kernel->ended && cli_spawn_poll(&kernel->spawned, &code))
    {
        kernel->ended = 1;
        kernel->code = code;
    }
    const int alive = !kernel->ended;
    cli_lock_give(&kernel->lock);
    return alive;
}

int cli_jupyter_stopped(CliKernel *kernel, unsigned long long wait)
{
    char id[CLI_KERNEL_ID];
    char why[CLI_SAID_LONGEST];
    CliLine unsent = {why, sizeof(why), 0ull};
    (void)cli_jupyter_sent(kernel, "control", "shutdown_request", "{\"restart\":false}", 17ull, id, &unsent);
    const unsigned long long until = cli_clock_milliseconds() + wait;
    while (cli_clock_milliseconds() < until)
    {
        if (!cli_jupyter_alive(kernel))
        {
            cli_kernel_forgotten(kernel);
            return 0;
        }
        query_paused(CLI_KERNEL_STOP_LOOK);
    }
    cli_kernel_ended(kernel);
    cli_kernel_forgotten(kernel);
    return 1;
}
