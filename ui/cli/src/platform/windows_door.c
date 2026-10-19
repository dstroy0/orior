// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_door.c: the door on Windows: a named pipe of the container's own, which takes no one from another machine,
// opened for overlapped work so that a read and a write go on at once on one end, each waited for alone. An instance
// waits from the start, and the next is made as each is joined: a process that joins finds one ready
#include "../comms/door_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <stdint.h>

// the bytes each instance of the pipe holds one way, and how long a process joining waits while every instance is
// taken, in milliseconds
#define CLI_DOOR_PIPE_ROOM 65536u
#define CLI_DOOR_BUSY_WAIT 5000ull

// an instance of the listener's pipe made, the first where `first`: INVALID_HANDLE_VALUE where none was
static HANDLE door_instance(const CliDoorListener *listener, int first)
{
    const DWORD mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0u);
    return CreateNamedPipeW(listener->name, mode,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                            PIPE_UNLIMITED_INSTANCES, CLI_DOOR_PIPE_ROOM, CLI_DOOR_PIPE_ROOM, 0u, NULL);
}

// one overlapped operation on `handle`, begun by `kind` (0 a read, 1 a write, 2 a join waited for) and waited for: the
// bytes it moved into `moved`: 1, or 0 where it failed
static int door_overlapped(HANDLE handle, HANDLE shut, int kind, void *bytes, DWORD size, DWORD *moved)
{
    // an end shut takes no read or write, so one begun after the shut does not wait for what never comes
    if ((shut != NULL) && (WaitForSingleObject(shut, 0u) == WAIT_OBJECT_0))
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
    const BOOL started = (kind == 0)   ? ReadFile(handle, bytes, size, NULL, &work)
                         : (kind == 1) ? WriteFile(handle, bytes, size, NULL, &work)
                                       : ConnectNamedPipe(handle, &work);
    if (!started)
    {
        const DWORD error = GetLastError();
        // a process that joined before the wait began needs no wait
        if ((kind == 2) && (error == ERROR_PIPE_CONNECTED))
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
    if (shut != NULL)
    {
        const HANDLE either[2] = {work.hEvent, shut};
        if (WaitForMultipleObjects(2u, either, FALSE, INFINITE) != WAIT_OBJECT_0)
        {
            // the end was shut while the work waited: the work is cancelled and seen through before its room goes
            (void)CancelIoEx(handle, &work);
            (void)GetOverlappedResult(handle, &work, moved, TRUE);
            CloseHandle(work.hEvent);
            return 0;
        }
    }
    const BOOL waited = GetOverlappedResult(handle, &work, moved, TRUE);
    CloseHandle(work.hEvent);
    return waited != FALSE;
}

// an end opened on `handle`, with the event it is shut by: 1, or 0 where Windows gave no event, the handle closed
static int door_end_opened(CliDoorEnd *end, HANDLE handle)
{
    const HANDLE shut = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (shut == NULL)
    {
        CloseHandle(handle);
        return 0;
    }
    end->handle = (unsigned long long)(uintptr_t)handle;
    end->shut = (unsigned long long)(uintptr_t)shut;
    end->open = 1;
    return 1;
}

int cli_door_listening(CliDoorListener *listener, char *address, CliLine *said)
{
    unsigned char secret[8];
    if (!cli_random_bytes(secret, sizeof(secret)))
    {
        cli_line_text(said, "the system gave no secret");
        return 0;
    }
    CliLine named = {address, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&named, "\\\\.\\pipe\\orior-comms-");
    cli_line_decimal(&named, cli_process_own(), 1u);
    cli_line_character(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        cli_line_character(&named, "0123456789abcdef"[secret[at] >> 4u]);
        cli_line_character(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    cli_line_closed(&named);
    listener->lock = (CliLock)CLI_LOCK_INIT;
    (void)MultiByteToWideChar(CP_UTF8, 0u, address, -1, listener->name, CLI_PATH_LONGEST);
    const HANDLE waiting = door_instance(listener, 1);
    const DWORD error = GetLastError();
    listener->waiting = (unsigned long long)(uintptr_t)waiting;
    if (waiting == INVALID_HANDLE_VALUE)
    {
        cli_line_text(said, "the door's pipe was not made: Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    return 1;
}

int cli_door_accepted(CliDoorListener *listener, CliDoorEnd *end, int *has_peer, unsigned int *peer, CliLine *said)
{
    cli_lock_take(&listener->lock);
    // the waiting instance's handle was a pointer's width when it was kept
    const HANDLE joined = (HANDLE)(uintptr_t)listener->waiting;
    cli_lock_give(&listener->lock);
    DWORD moved = 0u;
    const int joining = door_overlapped(joined, NULL, 2, NULL, 0u, &moved);
    const DWORD error = GetLastError();
    // the next instance waits for the process after, whether this one was joined or not
    cli_lock_take(&listener->lock);
    listener->waiting = (unsigned long long)(uintptr_t)door_instance(listener, 0);
    cli_lock_give(&listener->lock);
    if (!joining)
    {
        CloseHandle(joined);
        cli_line_text(said, "no process joined: Windows said ");
        cli_line_decimal(said, error, 1u);
        return 0;
    }
    ULONG id = 0u;
    *has_peer = (GetNamedPipeClientProcessId(joined, &id) != FALSE);
    *peer = (unsigned int)id;
    if (!door_end_opened(end, joined))
    {
        cli_line_text(said, "Windows gave no event for the end");
        return 0;
    }
    return 1;
}

int cli_door_connected(const char *address, CliDoorEnd *end, CliLine *said)
{
    wchar_t name[CLI_PATH_LONGEST];
    (void)MultiByteToWideChar(CP_UTF8, 0u, address, -1, name, CLI_PATH_LONGEST);
    const unsigned long long until = cli_clock_milliseconds() + CLI_DOOR_BUSY_WAIT;
    for (;;)
    {
        const HANDLE handle =
            CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0u, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (handle != INVALID_HANDLE_VALUE)
        {
            if (!door_end_opened(end, handle))
            {
                cli_line_text(said, "Windows gave no event for the end");
                return 0;
            }
            return 1;
        }
        const DWORD error = GetLastError();
        if ((error != ERROR_PIPE_BUSY) || (cli_clock_milliseconds() >= until))
        {
            cli_line_text(said, address);
            cli_line_text(said, ": Windows said ");
            cli_line_decimal(said, error, 1u);
            return 0;
        }
        (void)WaitNamedPipeW(name, 1000u);
    }
}

int cli_door_read(CliDoorEnd *end, void *bytes, unsigned long long size)
{
    unsigned char *const into = (unsigned char *)bytes;
    for (unsigned long long at = 0ull; at < size;)
    {
        DWORD moved = 0u;
        const unsigned long long left = size - at;
        // a piece is at most what a DWORD counts
        const DWORD piece = (left > 0x7fffffffull) ? 0x7fffffffu : (DWORD)left;
        if (!door_overlapped((HANDLE)(uintptr_t)end->handle, (HANDLE)(uintptr_t)end->shut, 0, into + at, piece,
                             &moved) ||
            (moved == 0u))
        {
            return 0;
        }
        at += moved;
    }
    return 1;
}

int cli_door_written(CliDoorEnd *end, const void *bytes, unsigned long long size, const CliRegion *region)
{
    // the region's handle is in the frame already: the pipe carries the frame alone
    (void)region;
    const unsigned char *const from = (const unsigned char *)bytes;
    for (unsigned long long at = 0ull; at < size;)
    {
        DWORD moved = 0u;
        const unsigned long long left = size - at;
        const DWORD piece = (left > 0x7fffffffull) ? 0x7fffffffu : (DWORD)left;
        // the bytes are only read, as WriteFile takes them
        if (!door_overlapped((HANDLE)(uintptr_t)end->handle, (HANDLE)(uintptr_t)end->shut, 1,
                             (void *)(uintptr_t)(from + at), piece, &moved) ||
            (moved == 0u))
        {
            return 0;
        }
        at += moved;
    }
    return 1;
}

int cli_door_descriptor(CliDoorEnd *end)
{
    (void)end;
    return -1;
}

void cli_door_shut(CliDoorEnd *end)
{
    if (end->open)
    {
        (void)SetEvent((HANDLE)(uintptr_t)end->shut);
        (void)CancelIoEx((HANDLE)(uintptr_t)end->handle, NULL);
    }
}

void cli_door_closed(CliDoorEnd *end)
{
    if (end->open)
    {
        CloseHandle((HANDLE)(uintptr_t)end->handle);
        CloseHandle((HANDLE)(uintptr_t)end->shut);
    }
    end->open = 0;
    end->handle = 0ull;
    end->shut = 0ull;
}
#endif
