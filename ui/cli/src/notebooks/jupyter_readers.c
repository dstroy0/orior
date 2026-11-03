// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_readers.c: a kernel's readers, each on a thread of its own: its sockets' messages, each a reply handed to
// the request waiting on it or received by whoever started the kernel, and the lines its process writes of itself
#include "../command_line/callback_cfg.h"
#include "jupyter_internal.h"

typedef struct
{
    CliKernel *kernel;
    const CliKernelReader *reader;
    const char *message;
    unsigned long long size;
    const char *parent;
    const unsigned char *line;
} JupyterReadersCtx;

// a reader's end notified, the kernel's waiters woken
EMBED_INLINE void jupyter_reader_end(const JupyterReadersCtx *args)
{
    args->kernel->read_state -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->kernel->wake);
}

// a message received: a reply on the shell ending the request in flight there, a reply handed to the request waiting on
// it where one waits, and any other to whoever started the kernel
EMBED_INLINE void jupyter_recv(const JupyterReadersCtx *args)
{
    const char *parent = args->parent;
    CliKernel *const kernel = args->reader->kernel;
    if ((args->reader->socket == &kernel->shell) && (parent[0] != '\0'))
    {
        EMBED_CALL(cli_jupyter_kernel_parent_internal.kernel_shell_answer, CliJupyterKernelParentCtx, .kernel = kernel,
                   .parent = parent);
    }
    if (args->reader->replies && (parent[0] != '\0'))
    {
        for (unsigned int at = 0u; at < CLI_KERNEL_WAITERS; at += 1u)
        {
            CliKernelWaiter *const waiter = &kernel->waiters[at];
            if (waiter->waiting &&
                (cellul.eq(&(CatenaFinitaCfg){.src = waiter->id, .other = parent, .cap = CLI_CELLS_POOL})))
            {
                mmgr_line_put_n(waiter->reply, args->message, args->size);
                waiter->answered = 1;
                waiter->waiting = 0;
                EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &kernel->wake);
                return;
            }
        }
    }
    EMBED_CALL(kernel->recv, CliKernelRecvCfg, .context = kernel->context, .message = args->message,
               .size = args->size);
}

// a socket's messages read until it closes, each received; half the reader's capacity stores the frames read and half
// the message written
static void jupyter_socket_read(void *context)
{
    CliKernelReader *const reader = (CliKernelReader *)context;
    const unsigned long long half = reader->capacity_bytes / 2ULL;
    CliZmtpFrame frames[CLI_KERNEL_FRAMES];
    for (;;)
    {
        const unsigned int count = EMBED_CALL(cli_zmtp_socket.receive, CliZmtpSocketCfg, .socket = reader->socket,
                                              .capacity = reader->capacity, .capacity_bytes = half,
                                              .frames_out = frames, .most = CLI_KERNEL_FRAMES);
        if (count == 0u)
        {
            break;
        }
        VerbaLinea out = {(char *)reader->capacity + half, half, 0ULL};
        char parent[CLI_KERNEL_ID];
        parent[0] = '\0';
        if (EMBED_CALL(cli_jupyter_out_internal.kernel_message_write, CliJupyterOutCtx, .reader = reader,
                       .frames = frames, .count = count, .out = &out, .parent_out = parent))
        {
            EMBED_CALL(jupyter_recv, JupyterReadersCtx, .reader = reader, .message = out.out, .size = out.at,
                       .parent = parent);
        }
    }
    EMBED_CALL(jupyter_reader_end, JupyterReadersCtx, .kernel = reader->kernel);
}

// a line the kernel's process wrote of itself received as a message of the log channel, read as cli_lossy reads it
EMBED_INLINE void jupyter_line_recv(const JupyterReadersCtx *args)
{
    VerbaLinea out = {(char *)args->reader->capacity + CLI_KERNEL_LINE, args->reader->capacity_bytes - CLI_KERNEL_LINE,
                      0ULL};
    mmgr_line_put(&out, "{\"channel\":\"log\",\"msg_type\":\"log\",\"msg_id\":\"\",\"parent\":\"\",\"content\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = args->line, .length = args->size);
    mmgr_line_ch(&out, '}');
    if (mmgr_line_ok(&out))
    {
        EMBED_CALL(args->reader->kernel->recv, CliKernelRecvCfg, .context = args->reader->kernel->context,
                   .message = out.out, .size = out.at);
    }
}

// what the kernel's process writes of itself read until the pipe closes, each line received; a line longer than
// CLI_KERNEL_LINE is received in pieces of that length
static void jupyter_log_read(void *context)
{
    CliKernelReader *const reader = (CliKernelReader *)context;
    unsigned char *const line = reader->capacity;
    unsigned long long line_start = 0ULL;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = reader->pipe, .piece = piece, .capacity = sizeof(piece));
        if (count == 0ULL)
        {
            break;
        }
        for (unsigned long long at = 0ULL; at < count; at += 1ULL)
        {
            if (piece[at] == '\n')
            {
                const unsigned long long size =
                    ((line_start > 0ULL) && (line[line_start - 1ULL] == '\r')) ? line_start - 1ULL : line_start;
                EMBED_CALL(jupyter_line_recv, JupyterReadersCtx, .reader = reader, .line = line, .size = size);
                line_start = 0ULL;
                continue;
            }
            if (line_start == CLI_KERNEL_LINE)
            {
                EMBED_CALL(jupyter_line_recv, JupyterReadersCtx, .reader = reader, .line = line, .size = line_start);
                line_start = 0ULL;
            }
            line[line_start] = (unsigned char)piece[at];
            line_start += 1ULL;
        }
    }
    if (line_start > 0ULL)
    {
        EMBED_CALL(jupyter_line_recv, JupyterReadersCtx, .reader = reader, .line = line, .size = line_start);
    }
    EMBED_CALL(jupyter_reader_end, JupyterReadersCtx, .kernel = reader->kernel);
}

void cli_kernel_reader_start(const CliJupyterAtCtx *args)
{
    CliKernel *kernel = args->kernel;
    CliKernelReader *const reader = &kernel->readers[args->at];
    reader->kernel = kernel;
    reader->thread = (CliThread){(args->at < 3u) ? &jupyter_socket_read : &jupyter_log_read, reader};
    kernel->read_state += 1u;
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &reader->thread))
    {
        EMBED_CALL(jupyter_reader_end, JupyterReadersCtx, .kernel = kernel);
    }
}
