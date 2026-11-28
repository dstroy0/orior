// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_daemon_peers.c: writing to, delivering to and serving the peers
// the internal header comes first, since it sets the macros of the system before any header
#include "tessera_daemon_internal.h"

#include "../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

_Alignas(8) static const char s_daemon_reason_closed[] = "its connection closed";

static int daemon_write(TesseraPeer *peer, const TesseraFrame *frame)
{
    unsigned char bytes[TESSERA_FRAME_BYTES];
    if (!tessera_frame_pack(frame, bytes))
    {
        return 0;
    }
#if defined(_WIN32)
    OVERLAPPED writing;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &writing, .bytes = sizeof(writing), .val = 0);
    writing.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (writing.hEvent == NULL)
    {
        return 0;
    }
    DWORD moved = 0u;
    const int started =
        WriteFile(peer->pipe, bytes, TESSERA_FRAME_BYTES, NULL, &writing) || (GetLastError() == ERROR_IO_PENDING);
    const int done =
        started && GetOverlappedResult(peer->pipe, &writing, &moved, TRUE) && (moved == TESSERA_FRAME_BYTES);
    CloseHandle(writing.hEvent);
    return done;
#else
    unsigned int sent = 0u;
    while (sent < TESSERA_FRAME_BYTES)
    {
        const ssize_t moved = send(peer->socket_descriptor, bytes + sent, TESSERA_FRAME_BYTES - sent, MSG_NOSIGNAL);
        if ((moved < 0) && (errno == EINTR))
        {
            continue;
        }
        if (moved <= 0)
        {
            return 0;
        }
        // a count of bytes moved is positive and at max one frame
        sent += (unsigned int)moved;
    }
    return 1;
#endif
}

static int daemon_deliver(TesseraPeer *peer)
{
    daemon_lock();
    const int has_grew = peer->has_grew;
    const int has_decisive = peer->has_decisive;
    TesseraFrame grew;
    daemon_frame_start(&grew, TESSERA_NOTICE_GREW, peer->identity);
    grew.signum = peer->signum;
    grew.bytes = peer->declared;
    grew.measured = peer->grew_to;
    const TesseraFrame decisive = peer->decisive;
    peer->has_grew = 0;
    peer->has_decisive = 0;
    daemon_unlock();
    return (!has_grew || daemon_write(peer, &grew)) && (!has_decisive || daemon_write(peer, &decisive));
}

static void daemon_peer_gone(TesseraPeer *peer)
{
    daemon_lock();
    const unsigned long long now = daemon_now();
    if (peer->identity != 0ULL)
    {
        daemon_job_dropped(peer, now, s_daemon_reason_closed);
    }
    TesseraPeer **link = &g_daemon.peers;
    while ((*link != NULL) && (*link != peer))
    {
        link = &(*link)->next;
    }
    if (*link == peer)
    {
        *link = peer->next;
    }
    g_daemon.peer_count -= 1ULL;
    if ((g_daemon.peer_count == 0ULL) && (g_daemon.ledger.job_count == 0ULL))
    {
        tessera_ledger_idle(&g_daemon.ledger, now, g_daemon.ledger.idle_microseconds);
        daemon_signal_changed();
    }
    daemon_unlock();
    tessera_process_release(peer->process);
#if defined(_WIN32)
    DisconnectNamedPipe(peer->pipe);
    CloseHandle(peer->pipe);
    CloseHandle(peer->wake);
#else
    close(peer->socket_descriptor);
    close(peer->wake[0]);
    close(peer->wake[1]);
#endif
    platform_cell_released(peer, platform_place_g_program_owner);
}

static void daemon_frame_arrived(TesseraPeer *peer, const unsigned char bytes[TESSERA_FRAME_BYTES], int *open)
{
    TesseraFrame frame;
    if (!tessera_frame_unpack(bytes, &frame))
    {
        *open = 0;
        return;
    }
    daemon_lock();
    daemon_handle(peer, &frame);
    daemon_unlock();
}

#if defined(_WIN32)
static DWORD WINAPI daemon_peer(LPVOID argument)
{
    TesseraPeer *const peer = (TesseraPeer *)argument;
    OVERLAPPED measurement;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &measurement, .bytes = sizeof(measurement), .val = 0);
    measurement.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    unsigned char bytes[TESSERA_FRAME_BYTES];
    unsigned int got = 0u;
    int pending = 0;
    int open = measurement.hEvent != NULL;
    while (open)
    {
        if (!pending)
        {
            ResetEvent(measurement.hEvent);
            const int started = ReadFile(peer->pipe, bytes + got, TESSERA_FRAME_BYTES - got, NULL, &measurement) ||
                                (GetLastError() == ERROR_IO_PENDING);
            if (!started)
            {
                break;
            }
            pending = 1;
        }
        const HANDLE waits[2] = {measurement.hEvent, peer->wake};
        const DWORD which = WaitForMultipleObjects(2u, waits, FALSE, INFINITE);
        if (which == (WAIT_OBJECT_0 + 1u))
        {
            open = daemon_deliver(peer);
            continue;
        }
        DWORD moved = 0u;
        if ((which != WAIT_OBJECT_0) || !GetOverlappedResult(peer->pipe, &measurement, &moved, FALSE) || (moved == 0u))
        {
            break;
        }
        pending = 0;
        got += moved;
        if (got == TESSERA_FRAME_BYTES)
        {
            got = 0u;
            daemon_frame_arrived(peer, bytes, &open);
        }
    }
    if (pending)
    {
        CancelIoEx(peer->pipe, &measurement);
        DWORD moved = 0u;
        GetOverlappedResult(peer->pipe, &measurement, &moved, TRUE);
    }
    if (measurement.hEvent != NULL)
    {
        CloseHandle(measurement.hEvent);
    }
    daemon_peer_gone(peer);
    return 0u;
}
#else
static void *daemon_peer(void *argument)
{
    TesseraPeer *const peer = (TesseraPeer *)argument;
    unsigned char bytes[TESSERA_FRAME_BYTES];
    unsigned int got = 0u;
    int open = 1;
    while (open)
    {
        struct pollfd watch[2];
        watch[0].fd = peer->socket_descriptor;
        watch[0].events = POLLIN;
        watch[0].revents = 0;
        watch[1].fd = peer->wake[0];
        watch[1].events = POLLIN;
        watch[1].revents = 0;
        if (poll(watch, 2u, -1) < 0)
        {
            open = errno == EINTR;
            continue;
        }
        if (watch[1].revents != 0)
        {
            char drained[TESSERA_FRAME_BYTES];
            while (read(peer->wake[0], drained, sizeof(drained)) > 0)
            {
            }
            open = daemon_deliver(peer);
        }
        if (open && (watch[0].revents != 0))
        {
            const ssize_t moved = read(peer->socket_descriptor, bytes + got, TESSERA_FRAME_BYTES - got);
            if (moved <= 0)
            {
                open = (moved < 0) && (errno == EINTR);
                continue;
            }
            // a count of bytes moved is positive and at max one frame
            got += (unsigned int)moved;
            if (got == TESSERA_FRAME_BYTES)
            {
                got = 0u;
                daemon_frame_arrived(peer, bytes, &open);
            }
        }
    }
    daemon_peer_gone(peer);
    return NULL;
}
#endif

int daemon_peer_start(TesseraPeer *peer, unsigned long long pid)
{
    peer->pid = pid;
    peer->process = tessera_process_open(pid);
    if (peer->process == NULL)
    {
        return 0;
    }
    daemon_lock();
    peer->next = g_daemon.peers;
    g_daemon.peers = peer;
    g_daemon.peer_count += 1ULL;
    daemon_unlock();
#if defined(_WIN32)
    const HANDLE thread = CreateThread(NULL, 0u, daemon_peer, peer, 0u, NULL);
    if (thread == NULL)
    {
        return 0;
    }
    CloseHandle(thread);
    return 1;
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, daemon_peer, peer) != 0)
    {
        return 0;
    }
    pthread_detach(thread);
    return 1;
#endif
}

static int daemon_hex(const char *text, unsigned char *bytes, unsigned int count)
{
    if (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR) != (2u * count))
    {
        return 0;
    }
    for (unsigned int byte = 0u; byte < count; byte += 1u)
    {
        unsigned int value = 0u;
        for (unsigned int half = 0u; half < 2u; half += 1u)
        {
            const char digit = text[(2u * byte) + half];
            const int decimal = (digit >= '0') && (digit <= '9');
            const int lower = (digit >= 'a') && (digit <= 'f');
            if (!decimal && !lower)
            {
                return 0;
            }
            // a hexadecimal digit's value is below sixteen
            value = (value << 4u) | (decimal ? (unsigned int)(digit - '0') : (unsigned int)(digit - 'a' + 10));
        }
        // two hexadecimal digits make one byte
        bytes[byte] = (unsigned char)value;
    }
    return 1;
}

int daemon_arguments(int count, char **arguments, unsigned long long *idle)
{
    int have_device = 0;
    int have_luid = 0;
    int have_idle = 0;
    for (int at = 1; (at + 1) < count; at += 2)
    {
        const char *end = NULL;
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "--device", .ci = EMBED_FALSE))
        {
            have_device = daemon_hex(arguments[at + 1], g_daemon.device, TESSERA_DEVICE_BYTES);
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--luid", .ci = EMBED_FALSE))
        {
            g_daemon.luid =
                EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = arguments[at + 1], .end = &end, .base = 16);
            have_luid = (end != arguments[at + 1]) && (*end == '\0');
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--idle", .ci = EMBED_FALSE))
        {
            *idle = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = arguments[at + 1], .end = &end, .base = 10);
            have_idle = (end != arguments[at + 1]) && (*end == '\0');
        }
        else
        {
            return 0;
        }
    }
    return have_device && have_luid && have_idle;
}

int daemon_socket_passed(void)
{
    // systemd passes one socket that listens over as fd 3, and names this process as its receiver. A system with no
    // systemd sets neither variable, and then the answer is 0
    char listen_pid_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const listen_pid =
        (platform_environment("LISTEN_PID", listen_pid_text, sizeof(listen_pid_text)) != 0ULL) ? listen_pid_text : NULL;
    char listen_fds_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const listen_fds =
        (platform_environment("LISTEN_FDS", listen_fds_text, sizeof(listen_fds_text)) != 0ULL) ? listen_fds_text : NULL;
    return (listen_pid != NULL) && (listen_fds != NULL) &&
           (EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = listen_pid, .end = NULL, .base = 10) ==
            platform_process_id()) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = listen_fds, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "1",
                       .ci = EMBED_FALSE));
}

int daemon_error(void)
{
#if !defined(_WIN32)
    // the connections that made systemd start this daemon wait on its socket. The call closes each with no answer.
    // Its client gives an error, and systemd has none left to start the daemon again for
    if (daemon_socket_passed())
    {
        const int flags = fcntl(3, F_GETFL);
        fcntl(3, F_SETFL, flags | O_NONBLOCK);
        for (int waiting = accept(3, NULL, NULL); waiting >= 0; waiting = accept(3, NULL, NULL))
        {
            close(waiting);
        }
    }
#endif
    return 1;
}
