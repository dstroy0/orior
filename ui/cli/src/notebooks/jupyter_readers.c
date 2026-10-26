// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_readers.c: a kernel's readers, each on a thread of its own: its sockets' messages, each a reply handed to
// the request waiting on it or received by whoever started the kernel, and the lines its process writes of itself
#include "jupyter_internal.h"

// a reader's end notified, the kernel's waiters woken
static void jupyter_reader_ended(CliKernel *kernel)
{
    cli_lock_take(&kernel->lock);
    kernel->reading -= 1u;
    cli_wake_all(&kernel->wake);
    cli_lock_give(&kernel->lock);
}

// a message received: a reply on the shell ending the request in flight there, a reply handed to the request waiting on
// it where one waits, and any other to whoever started the kernel
static void jupyter_recv(const CliKernelReader *reader, const char *message, unsigned long long size,
                         const char *parent)
{
    CliKernel *const kernel = reader->kernel;
    if ((reader->socket == &kernel->shell) && (parent[0] != '\0'))
    {
        cli_kernel_shell_answered(kernel, parent);
    }
    if (reader->replies && (parent[0] != '\0'))
    {
        cli_lock_take(&kernel->lock);
        for (unsigned int at = 0u; at < CLI_KERNEL_WAITERS; at += 1u)
        {
            CliKernelWaiter *const waiter = &kernel->waiters[at];
            if (waiter->waiting &&
                (cellul.eq(&(CatenaFinitaCfg){.src = waiter->id, .other = parent, .cap = CLI_CELLS_POOL})))
            {
                mmgr_line_put_n(waiter->reply, message, size);
                waiter->answered = 1;
                waiter->waiting = 0;
                cli_wake_all(&kernel->wake);
                cli_lock_give(&kernel->lock);
                return;
            }
        }
        cli_lock_give(&kernel->lock);
    }
    kernel->recv(kernel->context, message, size);
}

// a socket's messages read until it closes, each received; half the reader's room stores the frames read and half the
// message written
static void jupyter_socket_read(void *context)
{
    CliKernelReader *const reader = (CliKernelReader *)context;
    const unsigned long long half = reader->room_size / 2ull;
    CliZmtpFrame frames[CLI_KERNEL_FRAMES];
    for (;;)
    {
        const unsigned int count = cli_zmtp_received(reader->socket, reader->room, half, frames, CLI_KERNEL_FRAMES);
        if (count == 0u)
        {
            break;
        }
        VerbaLinea out = {(char *)reader->room + half, half, 0ull};
        char parent[CLI_KERNEL_ID];
        parent[0] = '\0';
        if (cli_kernel_message_written(reader, frames, count, &out, parent))
        {
            jupyter_recv(reader, out.out, out.at, parent);
        }
    }
    jupyter_reader_ended(reader->kernel);
}

// a line the kernel's process wrote of itself received as a message of the log channel, read as cli_lossy reads it
static void jupyter_line_recv(const CliKernelReader *reader, const unsigned char *line, unsigned long long size)
{
    VerbaLinea out = {(char *)reader->room + CLI_KERNEL_LINE, reader->room_size - CLI_KERNEL_LINE, 0ull};
    mmgr_line_put(&out, "{\"channel\":\"log\",\"msg_type\":\"log\",\"msg_id\":\"\",\"parent\":\"\",\"content\":");
    cli_json_quoted_lossy(&out, line, size);
    mmgr_line_ch(&out, '}');
    if (mmgr_line_ok(&out))
    {
        reader->kernel->recv(reader->kernel->context, out.out, out.at);
    }
}

// what the kernel's process writes of itself read until the pipe closes, each line received; a line longer than
// CLI_KERNEL_LINE is received in pieces of that length
static void jupyter_log_read(void *context)
{
    CliKernelReader *const reader = (CliKernelReader *)context;
    unsigned char *const line = reader->room;
    unsigned long long line_start = 0ull;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count = cli_spawn_read(reader->pipe, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if (piece[at] == '\n')
            {
                const unsigned long long size =
                    ((line_start > 0ull) && (line[line_start - 1ull] == '\r')) ? line_start - 1ull : line_start;
                jupyter_line_recv(reader, line, size);
                line_start = 0ull;
                continue;
            }
            if (line_start == CLI_KERNEL_LINE)
            {
                jupyter_line_recv(reader, line, line_start);
                line_start = 0ull;
            }
            line[line_start] = (unsigned char)piece[at];
            line_start += 1ull;
        }
    }
    if (line_start > 0ull)
    {
        jupyter_line_recv(reader, line, line_start);
    }
    jupyter_reader_ended(reader->kernel);
}

void cli_kernel_reader_started(CliKernel *kernel, unsigned int at)
{
    CliKernelReader *const reader = &kernel->readers[at];
    reader->kernel = kernel;
    reader->thread = (CliThread){(at < 3u) ? jupyter_socket_read : jupyter_log_read, reader};
    cli_lock_take(&kernel->lock);
    kernel->reading += 1u;
    cli_lock_give(&kernel->lock);
    if (!cli_thread_started(&reader->thread))
    {
        jupyter_reader_ended(kernel);
    }
}
