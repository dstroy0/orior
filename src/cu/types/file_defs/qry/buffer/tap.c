// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tap.c: the writer's tap: a named pipe beside the buffer's wake that the writer writes one line into for each blob as it places it, and a reader connected to it reads as it is written
#include "qry_buffer_internal.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

void qry_tap_name(const char *path, char *name, size_t room)
{
    const unsigned long long hash = qry_hash(path, strlen(path));
#if defined(_WIN32)
    snprintf(name, room, "\\\\.\\pipe\\qry-%016llx-tap", hash);
#else
    snprintf(name, room, "/tmp/qry-%016llx-tap", hash);
#endif
}

#if defined(_WIN32)
// the pipe's reader connected where one is waiting: 1 where a reader is on the pipe
static int qry_tap_connected(QryBuffer *buffer)
{
    if (buffer->tap_connected)
    {
        return 1;
    }
    if (ConnectNamedPipe(buffer->tap, NULL) || (GetLastError() == ERROR_PIPE_CONNECTED))
    {
        buffer->tap_connected = 1;
    }
    return buffer->tap_connected;
}
#endif

int qry_tap_opened(QryBuffer *buffer)
{
    if (buffer->tap_name[0] != '\0')
    {
        return 1;
    }
    qry_tap_name(buffer->path, buffer->tap_name, sizeof(buffer->tap_name));
#if defined(_WIN32)
    buffer->tap = CreateNamedPipeA(buffer->tap_name, PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, 1u, 65536u, 0u, 0u, NULL);
    buffer->tap_connected = 0;
    if (buffer->tap == INVALID_HANDLE_VALUE)
    {
        buffer->tap_name[0] = '\0';
        return 0;
    }
    qry_tap_connected(buffer);
    return 1;
#else
    // a reader that goes away leaves the next write a broken pipe, read as an error and not a signal
    signal(SIGPIPE, SIG_IGN);
    unlink(buffer->tap_name);
    buffer->tap = -1;
    if (mkfifo(buffer->tap_name, 0600) != 0)
    {
        buffer->tap_name[0] = '\0';
        return 0;
    }
    return 1;
#endif
}

void qry_tapped(QryBuffer *buffer, unsigned long long sequence, unsigned long long nanoseconds, unsigned int priority,
                unsigned int member, const char *name, size_t name_length, unsigned long long size)
{
    if (buffer->tap_name[0] == '\0')
    {
        return;
    }
    char line[QRY_NAME_LONGEST + 128u];
    char place[16];
    if (member < QRY_MEMBERS)
    {
        snprintf(place, sizeof(place), "%u", member);
    }
    else
    {
        snprintf(place, sizeof(place), "-");
    }
    // the name's length is at most QRY_NAME_LONGEST, an int
    const int length = snprintf(line, sizeof(line), "%llu %llu %u %s %.*s %llu\n", sequence, nanoseconds, priority,
                                place, (int)name_length, name, size);
    if (length <= 0)
    {
        return;
    }
#if defined(_WIN32)
    if (!qry_tap_connected(buffer))
    {
        return;
    }
    DWORD written = 0u;
    // the line's length is positive and fits a DWORD
    if (!WriteFile(buffer->tap, line, (DWORD)length, &written, NULL))
    {
        // the reader went away: the pipe listens for the next
        DisconnectNamedPipe(buffer->tap);
        buffer->tap_connected = 0;
        qry_tap_connected(buffer);
    }
#else
    if (buffer->tap < 0)
    {
        // no reader yet: the open says so, and the line is dropped
        buffer->tap = open(buffer->tap_name, O_WRONLY | O_NONBLOCK);
        if (buffer->tap < 0)
        {
            return;
        }
    }
    // the line's length is positive
    if (write(buffer->tap, line, (size_t)length) < 0)
    {
        // the reader went away, or the pipe is full: the line is dropped, and a gone reader is let go of
        if (errno == EPIPE)
        {
            close(buffer->tap);
            buffer->tap = -1;
        }
    }
#endif
}

void qry_tap_closed(QryBuffer *buffer)
{
    if (buffer->tap_name[0] == '\0')
    {
        return;
    }
#if defined(_WIN32)
    if (buffer->tap_connected)
    {
        FlushFileBuffers(buffer->tap);
        DisconnectNamedPipe(buffer->tap);
    }
    CloseHandle(buffer->tap);
    buffer->tap = INVALID_HANDLE_VALUE;
#else
    if (buffer->tap >= 0)
    {
        close(buffer->tap);
    }
    unlink(buffer->tap_name);
    buffer->tap = -1;
#endif
    buffer->tap_name[0] = '\0';
}
