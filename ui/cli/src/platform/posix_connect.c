// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_connect.c: a connection made on Linux and macOS to the first address a name gives that takes it, each tried
// within a time, its delay of small writes off; a clock read to the microsecond; and a socket listening on the
// loopback and the connections it takes
#if !defined(_WIN32)
// getaddrinfo, poll and clock_gettime are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <errno.h>

unsigned long long cli_clock_microseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * 1000000ull) + ((unsigned long long)now.tv_nsec / 1000ull);
}

// a connection made to one address within `milliseconds`, nonblocking for the wait alone: the descriptor, or -1
// where it was refused or took too long
static int cli_connect_one(const struct addrinfo *address, unsigned int milliseconds)
{
    const int made = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (made < 0)
    {
        return -1;
    }
    const int flags = fcntl(made, F_GETFL, 0);
    (void)fcntl(made, F_SETFL, flags | O_NONBLOCK);
    int connected = (connect(made, address->ai_addr, address->ai_addrlen) == 0);
    if (!connected && (errno == EINPROGRESS))
    {
        struct pollfd waited = {made, POLLOUT, 0};
        int error = 0;
        socklen_t size = (socklen_t)sizeof(error);
        connected = (poll(&waited, 1, (int)milliseconds) == 1) &&
                    (getsockopt(made, SOL_SOCKET, SO_ERROR, &error, &size) == 0) && (error == 0);
    }
    (void)fcntl(made, F_SETFL, flags);
    if (!connected)
    {
        close(made);
        return -1;
    }
    return made;
}

int cli_socket_connected_timed(CliSocket *socket_held, const char *host, unsigned int port, unsigned int milliseconds,
                               unsigned long long *resolved, CliLine *said)
{
    socket_held->handle = 0ull;
    char service[16];
    CliLine service_line = {service, sizeof(service), 0ull};
    cli_line_decimal(&service_line, port, 0u);
    cli_line_closed(&service_line);
    struct addrinfo hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *found = NULL;
    const unsigned long long began = cli_clock_microseconds();
    const int named = (getaddrinfo(host, service, &hints, &found) == 0) && (found != NULL);
    if (resolved != NULL)
    {
        *resolved = cli_clock_microseconds() - began;
    }
    if (!named)
    {
        cli_line_text(said, host);
        cli_line_character(said, ':');
        cli_line_text(said, service);
        cli_line_text(said, " names no address");
        return 0;
    }
    int made = -1;
    for (const struct addrinfo *address = found; (made < 0) && (address != NULL); address = address->ai_next)
    {
        made = cli_connect_one(address, milliseconds);
    }
    freeaddrinfo(found);
    if (made < 0)
    {
        cli_line_text(said, host);
        cli_line_character(said, ':');
        cli_line_text(said, service);
        cli_line_text(said, ": the connection was refused or took too long");
        return 0;
    }
    const int no_delay = 1;
    setsockopt(made, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
    // a descriptor is not negative; one past it so that 0 stands for none
    socket_held->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_connected_within(CliSocket *socket_held, const char *host, unsigned int port, unsigned int milliseconds,
                                CliLine *said)
{
    return cli_socket_connected_timed(socket_held, host, port, milliseconds, NULL, said);
}

int cli_socket_listening(CliSocket *listener, unsigned int *port, CliLine *said)
{
    listener->handle = 0ull;
    const int made = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    memor.set(&(MemoriaCfg){.dst = &address, .val = 0, .bytes = sizeof(address)});
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = (socklen_t)sizeof(address);
    const int bound = (made >= 0) && (bind(made, (const struct sockaddr *)&address, size) == 0) &&
                      (getsockname(made, (struct sockaddr *)&address, &size) == 0) && (listen(made, 16) == 0);
    if (!bound)
    {
        if (made >= 0)
        {
            close(made);
        }
        cli_line_text(said, "127.0.0.1: no port could be listened on");
        return 0;
    }
    *port = ntohs(address.sin_port);
    // a descriptor is not negative; one past it so that 0 stands for none
    listener->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_accepted(const CliSocket *listener, CliSocket *peer)
{
    peer->handle = 0ull;
    // the handle is one past a descriptor, which an int holds
    const int made = (listener->handle != 0ull) ? accept((int)(listener->handle - 1ull), NULL, NULL) : -1;
    if (made < 0)
    {
        return 0;
    }
    // a descriptor is not negative; one past it so that 0 stands for none
    peer->handle = (unsigned long long)made + 1ull;
    return 1;
}

#endif
