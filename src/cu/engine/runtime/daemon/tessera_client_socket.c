// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_client_socket.c: connecting, spawning, sending and receiving
// the internal header comes first, since it sets the macros of the system before any header
#include "tessera_client_internal.h"

#include "../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if !defined(_WIN32)
_Alignas(8) static const char s_client_lock[] = "/daemon.lock";
#endif

// how long the client waits on the daemon that it started between two tries to connect
#define TESSERA_CLIENT_STEP_MICROSECONDS 10000ULL

TesseraConnect tessera_connect(TesseraClient *client, const char *endpoint)
{
#if defined(_WIN32)
    for (;;)
    {
        client->pipe = CreateFileA(endpoint, GENERIC_READ | GENERIC_WRITE, 0u, NULL, OPEN_EXISTING, 0u, NULL);
        if (client->pipe != INVALID_HANDLE_VALUE)
        {
            return TESSERA_CONNECT_MADE;
        }
        const DWORD failure = GetLastError();
        if (failure == ERROR_FILE_NOT_FOUND)
        {
            return TESSERA_CONNECT_ABSENT;
        }
        if ((failure != ERROR_PIPE_BUSY) || !WaitNamedPipeA(endpoint, NMPWAIT_WAIT_FOREVER))
        {
            return (GetLastError() == ERROR_FILE_NOT_FOUND) ? TESSERA_CONNECT_ABSENT : TESSERA_CONNECT_FAILED;
        }
    }
#else
    struct sockaddr_un address;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &address, .bytes = sizeof(address), .val = 0);
    address.sun_family = AF_UNIX;
    if (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = endpoint, .cap = PLATFORM_TEXT_TO_TERMINATOR) >=
        sizeof(address.sun_path))
    {
        return TESSERA_CONNECT_FAILED;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = address.sun_path, .src = endpoint,
               .bytes =
                   EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = endpoint, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    // the connection closes on exec: a program this process starts never leaves its job open
    client->socket_descriptor = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (client->socket_descriptor < 0)
    {
        return TESSERA_CONNECT_FAILED;
    }
    if (connect(client->socket_descriptor, (const struct sockaddr *)&address, sizeof(address)) == 0)
    {
        return TESSERA_CONNECT_MADE;
    }
    const int failure = errno;
    close(client->socket_descriptor);
    client->socket_descriptor = -1;
    return ((failure == ENOENT) || (failure == ECONNREFUSED)) ? TESSERA_CONNECT_ABSENT : TESSERA_CONNECT_FAILED;
#endif
}

#if !defined(_WIN32)
static int tessera_daemon_owns_lock(void)
{
    _Alignas(8) char lock_path[ENGINE_PATH_CAPACITY];
    if (!tessera_path_root(lock_path, ENGINE_PATH_CAPACITY))
    {
        return 0;
    }
    VerbaLinea line = {lock_path, ENGINE_PATH_CAPACITY,
                       EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = lock_path, .cap = PLATFORM_TEXT_TO_TERMINATOR)};
    mmgr_line_put(&line, s_client_lock);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return 0;
    }
    const int lock = open(lock_path, O_RDONLY);
    if (lock < 0)
    {
        return 0;
    }
    const int free_now = flock(lock, LOCK_SH | LOCK_NB) == 0;
    close(lock);
    return !free_now;
}
#endif

int tessera_spawn_and_connect(TesseraClient *client, const TesseraJobSubmit *submit, const char *endpoint)
{
    _Alignas(8) char idle_text[TESSERA_ARGUMENT_CAPACITY];
    VerbaLinea idle_line = {idle_text, TESSERA_ARGUMENT_CAPACITY, 0ULL};
    mmgr_line_decimal(&idle_line, submit->idle_microseconds, 1u);
    if (mmgr_line_finish(&idle_line) == 0ULL)
    {
        return 0;
    }
    // The system's daemon outlives the client that starts it, out of every job and group of the client's. It has no
    // console and no output, and a fault shows no dialog
    const char *const words[] = {submit->daemon_path, "--idle", idle_text, NULL};
    char why[ENGINE_PATH_CAPACITY];
    VerbaLinea err_msg = mmgr_line_over(why, sizeof(why));
    PlatformStart start;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &start, .bytes = sizeof(start), .val = 0);
    start.words = words;
    start.output = PLATFORM_OUTPUT_NONE;
    start.windowless = 1;
    start.outlives = 1;
    start.fault_silent = 1;
    start.err_msg = &err_msg;
    PlatformProgram daemon;
    if (!platform_program_started(&start, &daemon))
    {
        return 0;
    }
    // The client waits a step on the daemon's end between each try to connect. A daemon that ended errored or found
    // the system's daemon in its place. On POSIX the system's lock tells the two apart, while the daemon in place makes
    // its socket
    int ended = 0;
    TesseraConnect outcome = tessera_connect(client, endpoint);
    while (outcome == TESSERA_CONNECT_ABSENT)
    {
        ended = ended || platform_program_ended_within(&daemon, TESSERA_CLIENT_STEP_MICROSECONDS);
#if defined(_WIN32)
        const int in_place = 0;
#else
        const int in_place = tessera_daemon_owns_lock();
#endif
        if (ended && !in_place)
        {
            outcome = tessera_connect(client, endpoint);
            break;
        }
        outcome = tessera_connect(client, endpoint);
    }
    platform_program_closed(&daemon);
    return outcome == TESSERA_CONNECT_MADE;
}

int tessera_send(TesseraClient *client, const TesseraFrame *frame)
{
    unsigned char bytes[TESSERA_FRAME_BYTES];
    if (!tessera_frame_pack(frame, bytes))
    {
        return 0;
    }
    unsigned int sent = 0u;
#if !defined(_WIN32)
    // the thread that measures and the caller share the socket, and each frame goes out whole under the lock
    pthread_mutex_lock(&client->sending);
#endif
    int ok = 1;
    while (ok && (sent < TESSERA_FRAME_BYTES))
    {
#if defined(_WIN32)
        DWORD moved = 0u;
        ok = WriteFile(client->pipe, bytes + sent, TESSERA_FRAME_BYTES - sent, &moved, NULL) && (moved != 0u);
#else
        const ssize_t moved = write(client->socket_descriptor, bytes + sent, TESSERA_FRAME_BYTES - sent);
        if ((moved < 0) && (errno == EINTR))
        {
            continue;
        }
        ok = moved > 0;
#endif
        // a count of bytes moved is positive and at max one frame
        sent += ok ? (unsigned int)moved : 0u;
    }
#if !defined(_WIN32)
    pthread_mutex_unlock(&client->sending);
#endif
    return ok;
}

int tessera_receive(TesseraClient *client, TesseraFrame *frame)
{
    unsigned char bytes[TESSERA_FRAME_BYTES];
    unsigned int got = 0u;
    while (got < TESSERA_FRAME_BYTES)
    {
#if defined(_WIN32)
        DWORD moved = 0u;
        if (!ReadFile(client->pipe, bytes + got, TESSERA_FRAME_BYTES - got, &moved, NULL) || (moved == 0u))
        {
            return 0;
        }
#else
        const ssize_t moved = read(client->socket_descriptor, bytes + got, TESSERA_FRAME_BYTES - got);
        if ((moved < 0) && (errno == EINTR))
        {
            continue;
        }
        if (moved <= 0)
        {
            return 0;
        }
#endif
        // a count of bytes moved is positive and at max one frame
        got += (unsigned int)moved;
    }
    return tessera_frame_unpack(bytes, frame);
}

unsigned long long tessera_client_now(void)
{
    return platform_clock_ns() / 1000ULL;
}

int tessera_readable(TesseraClient *client, unsigned long long deadline)
{
    if (deadline == 0ULL)
    {
        return 1;
    }
    for (;;)
    {
        const unsigned long long now = tessera_client_now();
#if defined(_WIN32)
        DWORD waiting = 0u;
        if (!PeekNamedPipe(client->pipe, NULL, 0u, NULL, &waiting, NULL))
        {
            return -1;
        }
        if (waiting != 0u)
        {
            return 1;
        }
        if (now >= deadline)
        {
            return 0;
        }
        // a synchronous pipe has no timed read, and the call looks at the bytes that wait every few milliseconds
        Sleep(TESSERA_CLIENT_LOOK_MILLISECONDS);
#else
        if (now >= deadline)
        {
            return 0;
        }
        const unsigned long long left = ((deadline - now) + 999ULL) / 1000ULL;
        struct pollfd watched = {client->socket_descriptor, POLLIN, 0};
        // the bound keeps the milliseconds left below 2^31 here, and they narrow to int exactly
        const int ready = poll(&watched, 1u, (int)((left < 0x7FFFFFFFULL) ? left : 0x7FFFFFFFULL));
        if ((ready < 0) && (errno == EINTR))
        {
            continue;
        }
        if (ready < 0)
        {
            return -1;
        }
        if (ready > 0)
        {
            return 1;
        }
#endif
    }
}

void tessera_frame_start(const TesseraClient *client, TesseraFrame *frame, unsigned int type)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = frame, .bytes = sizeof(*frame), .val = 0);
    frame->magic = TESSERA_MAGIC;
    frame->version = TESSERA_VERSION;
    frame->type = type;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = frame->device, .src = client->device, .bytes = TESSERA_DEVICE_BYTES);
    frame->signum = client->signum;
    frame->identity = client->identity;
    frame->luid = client->luid;
}
