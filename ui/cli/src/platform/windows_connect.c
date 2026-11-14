// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_connect.c: a connection made on Windows to the first address a name gives that takes it, each tried within
// a time, its delay of small writes off; a clock read to the microsecond; and a socket listening on the loopback and
// the connections it takes
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

unsigned long long cli_clock_microseconds(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    // the counter and its frequency are not negative, and the frequency is not 0 on any Windows that runs orior
    const unsigned long long ticks = (unsigned long long)now.QuadPart;
    const unsigned long long per_second = (unsigned long long)frequency.QuadPart;
    return ((ticks / per_second) * 1000000ull) + (((ticks % per_second) * 1000000ull) / per_second);
}

typedef struct
{
#if (defined(_WIN32))
    const ADDRINFOA *address;
    unsigned int milliseconds;
#endif
} WindowsConnectCtx;
// a connection made to one address within `milliseconds`, nonblocking for the wait alone: the socket, or
// INVALID_SOCKET where it was refused or took too long
EMBED_INLINE SOCKET cli_connect_one(const WindowsConnectCtx *args)
{
    SOCKET made = socket(args->address->ai_family, args->address->ai_socktype, args->address->ai_protocol);
    if (made == INVALID_SOCKET)
    {
        return made;
    }
    u_long waiting = 1ul;
    (void)ioctlsocket(made, FIONBIO, &waiting);
    int connected = (connect(made, args->address->ai_addr, (int)args->address->ai_addrlen) == 0);
    if (!connected && (WSAGetLastError() == WSAEWOULDBLOCK))
    {
        fd_set writable;
        fd_set failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(made, &writable);
        FD_SET(made, &failed);
        struct timeval wait = {(long)(args->milliseconds / 1000u), (long)((args->milliseconds % 1000u) * 1000u)};
        connected = (select(0, NULL, &writable, &failed, &wait) == 1) && FD_ISSET(made, &writable);
    }
    waiting = 0ul;
    (void)ioctlsocket(made, FIONBIO, &waiting);
    if (!connected)
    {
        closesocket(made);
        return INVALID_SOCKET;
    }
    return made;
}

// the system's sockets started once: 1, or 0 where they did not start. Windows counts its sockets' starts, so this
// start beside windows.c's own is let go only as the process ends
static int cli_connect_start(void)
{
    static int s_started;
    if (!s_started)
    {
        WSADATA data;
        s_started = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }
    const int started = s_started;
    return started;
}

int cli_socket_listening(const CliErrMsgPortCtx *args)
{
    unsigned int *port = args->port_pointer;
    args->listener->handle = 0ull;
    SOCKET made = cli_connect_start() ? socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) : INVALID_SOCKET;
    struct sockaddr_in address;
    memor.set(&(MemoriaCfg){.dst = &address, .val = 0, .bytes = sizeof(address)});
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int size = (int)sizeof(address);
    const int bound = (made != INVALID_SOCKET) && (bind(made, (const struct sockaddr *)&address, size) == 0) &&
                      (getsockname(made, (struct sockaddr *)&address, &size) == 0) && (listen(made, 16) == 0);
    if (!bound)
    {
        if (made != INVALID_SOCKET)
        {
            closesocket(made);
        }
        mmgr_line_put(args->err_msg, "127.0.0.1: no port could be listened on");
        return 0;
    }
    *port = ntohs(address.sin_port);
    // a socket is a pointer's width, which an unsigned long long stores; one past it so that 0 stands for none
    args->listener->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_accept(const CliListenerCtx *args)
{
    args->peer->handle = 0ull;
    const SOCKET made = (args->listener_read->handle != 0ull)
                            ? accept((SOCKET)(args->listener_read->handle - 1ull), NULL, NULL)
                            : INVALID_SOCKET;
    if (made == INVALID_SOCKET)
    {
        return 0;
    }
    // a socket is a pointer's width, which an unsigned long long stores; one past it so that 0 stands for none
    args->peer->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_connected_time(const CliErrMsgCtx *args)
{
    unsigned long long *resolved = args->resolved;
    args->sock_out->handle = 0ull;
    char service[16];
    VerbaLinea service_line = {service, sizeof(service), 0ull};
    mmgr_line_decimal(&service_line, args->port, 0u);
    mmgr_line_finish(&service_line);
    (void)cli_connect_start();
    ADDRINFOA hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    ADDRINFOA *found = NULL;
    const unsigned long long began = cli_clock_microseconds();
    const int named = (getaddrinfo(args->host, service, &hints, &found) == 0) && (found != NULL);
    if (resolved != NULL)
    {
        *resolved = cli_clock_microseconds() - began;
    }
    if (!named)
    {
        mmgr_line_put(args->err_msg, args->host);
        mmgr_line_ch(args->err_msg, ':');
        mmgr_line_put(args->err_msg, service);
        mmgr_line_put(args->err_msg, " names no address");
        return 0;
    }
    SOCKET made = INVALID_SOCKET;
    for (const ADDRINFOA *address = found; (made == INVALID_SOCKET) && (address != NULL); address = address->ai_next)
    {
        made = EMBED_CALL(cli_connect_one, WindowsConnectCtx, .address = address, .milliseconds = args->milliseconds);
    }
    freeaddrinfo(found);
    if (made == INVALID_SOCKET)
    {
        mmgr_line_put(args->err_msg, args->host);
        mmgr_line_ch(args->err_msg, ':');
        mmgr_line_put(args->err_msg, service);
        mmgr_line_put(args->err_msg, ": the connection was refused or took too long");
        return 0;
    }
    const BOOL no_delay = TRUE;
    setsockopt(made, IPPROTO_TCP, TCP_NODELAY, (const char *)&no_delay, sizeof(no_delay));
    // a socket is a pointer's width, which an unsigned long long stores; one past it so that 0 stands for none
    args->sock_out->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_connected_within(const CliErrMsgCtx *args)
{
    return EMBED_CALL(cli_err_msg_internal.socket_connected_time, CliErrMsgCtx, .sock_out = args->sock_out,
                      .host = args->host, .port = args->port, .milliseconds = args->milliseconds, .resolved = NULL,
                      .err_msg = args->err_msg);
}

#endif
