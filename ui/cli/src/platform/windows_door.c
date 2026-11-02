// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_ipc.c: the door on Windows: a named pipe of the container's own, which takes no one from another machine,
// opened for overlapped work so that a read and a write go on at once on one end, each waited for alone. An instance
// waits from the start, and the next is made as each is joined: a process that joins finds one ready
#include "../comms/door_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stdint.h>

// the bytes each instance of the pipe buffers one way, and how long a process joining waits while every instance is
// taken, in milliseconds
#define CLI_IPC_PIPE_CAPACITY 65536u
#define CLI_IPC_BUSY_WAIT 5000ull

typedef struct
{
#if (defined(_WIN32))
    const CliIpcListener *listener;
    int first;
    HANDLE handle;
    HANDLE shut;
    int kind;
    void *bytes;
    DWORD size;
    DWORD *moved;
    CliIpcEnd *end;
#endif
} WindowsDoorCtx;
// an instance of the listener's pipe made, the first where `first`: INVALID_HANDLE_VALUE where none was
EMBED_INLINE HANDLE ipc_instance(const WindowsDoorCtx *args)
{
    const DWORD mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (args->first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0u);
    return CreateNamedPipeW(args->listener->name, mode,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                            PIPE_UNLIMITED_INSTANCES, CLI_IPC_PIPE_CAPACITY, CLI_IPC_PIPE_CAPACITY, 0u, NULL);
}

// one overlapped operation on `handle`, begun by `kind` (0 a read, 1 a write, 2 a join waited for) and waited for: the
// bytes it moved into `moved`: 1, or 0 where it failed
static int ipc_overlap(const WindowsDoorCtx *args)
{
    void *bytes = args->bytes;
    DWORD *moved = args->moved;
    // an end shut takes no read or write, so one begun after the shut does not wait for what never comes
    if ((args->shut != NULL) && (WaitForSingleObject(args->shut, 0u) == WAIT_OBJECT_0))
    {
        return 0;
    }
    OVERLAPPED work;
    memor.set(&(MemoriaCfg){.dst = &work, .val = 0, .bytes = sizeof(work)});
    work.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (work.hEvent == NULL)
    {
        return 0;
    }
    const BOOL started = (args->kind == 0)   ? ReadFile(args->handle, bytes, args->size, NULL, &work)
                         : (args->kind == 1) ? WriteFile(args->handle, bytes, args->size, NULL, &work)
                                             : ConnectNamedPipe(args->handle, &work);
    if (!started)
    {
        const DWORD error = GetLastError();
        // a process that joined before the wait began needs no wait
        if ((args->kind == 2) && (error == ERROR_PIPE_CONNECTED))
        {
            CloseHandle(work.hEvent);
            *moved = 0u;
            return 1;
        }
        if (error != ERROR_IO_PENDING)
        {
            CloseHandle(work.hEvent);
            return 0;
        }
    }
    if (args->shut != NULL)
    {
        const HANDLE either[2] = {work.hEvent, args->shut};
        if (WaitForMultipleObjects(2u, either, FALSE, INFINITE) != WAIT_OBJECT_0)
        {
            // the end was shut while the work waited: the work is cancelled and seen through before its capacity goes
            (void)CancelIoEx(args->handle, &work);
            (void)GetOverlappedResult(args->handle, &work, moved, TRUE);
            CloseHandle(work.hEvent);
            return 0;
        }
    }
    const BOOL waited = GetOverlappedResult(args->handle, &work, moved, TRUE);
    CloseHandle(work.hEvent);
    return waited != FALSE;
}

// an end opened on `handle`, with the event it is shut by: 1, or 0 where Windows gave no event, the handle closed
EMBED_INLINE int ipc_end_open(const WindowsDoorCtx *args)
{
    HANDLE handle = args->handle;
    const HANDLE shut = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (shut == NULL)
    {
        CloseHandle(handle);
        return 0;
    }
    args->end->handle = (unsigned long long)(uintptr_t)handle;
    args->end->shut = (unsigned long long)(uintptr_t)shut;
    args->end->open = 1;
    return 1;
}

int cli_ipc_listening(const CliIpcListenerAddressCtx *args)
{
    unsigned char secret[8];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = secret, .size = sizeof(secret)))
    {
        mmgr_line_put(args->err_msg, "the system gave no secret");
        return 0;
    }
    VerbaLinea named = {args->address, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&named, "\\\\.\\pipe\\orior-comms-");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&named);
    (void)MultiByteToWideChar(CP_UTF8, 0u, args->address, -1, args->listener->name, CLI_PATH_MAX_LEN);
    const HANDLE waiting = EMBED_CALL(ipc_instance, WindowsDoorCtx, .listener = args->listener, .first = 1);
    const DWORD error = GetLastError();
    args->listener->waiting = (unsigned long long)(uintptr_t)waiting;
    if (waiting == INVALID_HANDLE_VALUE)
    {
        mmgr_line_put(args->err_msg, "the door's pipe was not made: Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    return 1;
}

int cli_ipc_accept(const CliIpcListenerCtx *args)
{
    int *has_peer = args->has_peer;
    unsigned int *peer = args->peer;
    // the waiting instance's handle was a pointer's width when it was stored
    const HANDLE joined = (HANDLE)(uintptr_t)args->listener->waiting;
    DWORD moved = 0u;
    const int joining = EMBED_CALL(ipc_overlap, WindowsDoorCtx, .handle = joined, .shut = NULL, .kind = 2,
                                   .bytes = NULL, .size = 0u, .moved = &moved);
    const DWORD error = GetLastError();
    // the next instance waits for the process after, whether this one was joined or not
    args->listener->waiting =
        (unsigned long long)(uintptr_t)EMBED_CALL(ipc_instance, WindowsDoorCtx, .listener = args->listener, .first = 0);
    if (!joining)
    {
        CloseHandle(joined);
        mmgr_line_put(args->err_msg, "no process joined: Windows reported ");
        mmgr_line_decimal(args->err_msg, error, 1u);
        return 0;
    }
    ULONG id = 0u;
    *has_peer = (GetNamedPipeClientProcessId(joined, &id) != FALSE);
    *peer = (unsigned int)id;
    if (!EMBED_CALL(ipc_end_open, WindowsDoorCtx, .end = args->end, .handle = joined))
    {
        mmgr_line_put(args->err_msg, "Windows gave no event for the end");
        return 0;
    }
    return 1;
}

int cli_ipc_connect(const CliIpcCtx *args)
{
    wchar_t name[CLI_PATH_MAX_LEN];
    (void)MultiByteToWideChar(CP_UTF8, 0u, args->address_read, -1, name, CLI_PATH_MAX_LEN);
    const unsigned long long until = cli_clock_milliseconds() + CLI_IPC_BUSY_WAIT;
    for (;;)
    {
        const HANDLE handle =
            CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0u, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (handle != INVALID_HANDLE_VALUE)
        {
            if (!EMBED_CALL(ipc_end_open, WindowsDoorCtx, .end = args->end, .handle = handle))
            {
                mmgr_line_put(args->err_msg, "Windows gave no event for the end");
                return 0;
            }
            return 1;
        }
        const DWORD error = GetLastError();
        if ((error != ERROR_PIPE_BUSY) || (cli_clock_milliseconds() >= until))
        {
            mmgr_line_put(args->err_msg, args->address_read);
            mmgr_line_put(args->err_msg, ": Windows reported ");
            mmgr_line_decimal(args->err_msg, error, 1u);
            return 0;
        }
        (void)WaitNamedPipeW(name, 1000u);
    }
}

int cli_ipc_read(const CliIpcEndBytesCtx *args)
{
    unsigned char *const into = (unsigned char *)args->bytes;
    for (unsigned long long at = 0ull; at < args->size;)
    {
        DWORD moved = 0u;
        const unsigned long long left = args->size - at;
        // a piece is at most what a DWORD counts
        const DWORD piece = (left > 0x7fffffffull) ? 0x7fffffffu : (DWORD)left;
        if (!EMBED_CALL(ipc_overlap, WindowsDoorCtx, .handle = (HANDLE)(uintptr_t)args->end->handle,
                        .shut = (HANDLE)(uintptr_t)args->end->shut, .kind = 0, .bytes = into + at, .size = piece,
                        .moved = &moved) ||
            (moved == 0u))
        {
            return 0;
        }
        at += moved;
    }
    return 1;
}

int cli_ipc_write(const CliIpcEndSizeCtx *args)
{
    // the region's handle is in the frame already: the pipe carries the frame alone
    const unsigned char *const from = (const unsigned char *)args->bytes_read;
    for (unsigned long long at = 0ull; at < args->size;)
    {
        DWORD moved = 0u;
        const unsigned long long left = args->size - at;
        const DWORD piece = (left > 0x7fffffffull) ? 0x7fffffffu : (DWORD)left;
        // the bytes are only read, as WriteFile takes them
        if (!EMBED_CALL(ipc_overlap, WindowsDoorCtx, .handle = (HANDLE)(uintptr_t)args->end->handle,
                        .shut = (HANDLE)(uintptr_t)args->end->shut, .kind = 1, .bytes = (void *)(uintptr_t)(from + at),
                        .size = piece, .moved = &moved) ||
            (moved == 0u))
        {
            return 0;
        }
        at += moved;
    }
    return 1;
}

int cli_ipc_descriptor(const CliIpcErrMsgCtx *args)
{
    (void)args;
    return -1;
}

void cli_ipc_shut(const CliIpcCtx *args)
{
    if (args->end->open)
    {
        (void)SetEvent((HANDLE)(uintptr_t)args->end->shut);
        (void)CancelIoEx((HANDLE)(uintptr_t)args->end->handle, NULL);
    }
}

void cli_ipc_close(const CliIpcCtx *args)
{
    if (args->end->open)
    {
        CloseHandle((HANDLE)(uintptr_t)args->end->handle);
        CloseHandle((HANDLE)(uintptr_t)args->end->shut);
    }
    args->end->open = 0;
    args->end->handle = 0ull;
    args->end->shut = 0ull;
}
#endif
