// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_door.c: the door on Linux and macOS: a socket in a folder only the reader can open, the process at the other
// end of each connection as the system says it, and each descriptor that comes with the bytes read kept for the frame
// that names it
#if !defined(_WIN32)
// the peer's credentials and the socket's calls are POSIX and the systems' own, outside strict C11
#if defined(__linux__)
#define _GNU_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#endif
#include "../comms/door_internal.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

// what the system said of the last call that failed, after `what`
static void door_failed(CliLine *said, const char *what)
{
    const int error = errno;
    cli_line_text(said, what);
    cli_line_text(said, ": the system said ");
    cli_line_decimal(said, (unsigned long long)error, 1u);
}

// the socket address of the path `path` into `address`: 1, or 0 where the path is longer than a socket's name holds
static int door_address(const char *path, struct sockaddr_un *address)
{
    memor.set(&(MemoriaCfg){.dst = address, .val = 0, .bytes = sizeof(*address)});
    address->sun_family = AF_UNIX;
    if (cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) >= sizeof(address->sun_path))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = address->sun_path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// an end made of the socket `fd`, marked to close as a program starts
static void door_end_made(CliDoorEnd *end, int fd)
{
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
#if defined(__APPLE__)
    // a write to an end the peer closed fails, and sends no signal, as MSG_NOSIGNAL has it elsewhere
    const int on = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
    memor.set(&(MemoriaCfg){.dst = end, .val = 0, .bytes = sizeof(*end)});
    end->handle = (unsigned long long)fd;
    end->open = 1;
    end->kept.lock = (CliLock)CLI_LOCK_INIT;
}

int cli_door_listening(CliDoorListener *listener, char *address, CliLine *said)
{
    unsigned char secret[8];
    char temporary[CLI_PATH_LONGEST];
    char name[96];
    char folder[CLI_PATH_LONGEST];
    if (!cli_random_bytes(secret, sizeof(secret)))
    {
        cli_line_text(said, "the system gave no secret");
        return 0;
    }
    CliLine named = {name, sizeof(name), 0ull};
    cli_line_text(&named, "orior-comms-");
    cli_line_decimal(&named, cli_process_own(), 1u);
    cli_line_character(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        cli_line_character(&named, "0123456789abcdef"[secret[at] >> 4u]);
        cli_line_character(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    cli_line_closed(&named);
    cli_temp_folder(temporary);
    cli_full(temporary, name, folder);
    if (mkdir(folder, 0700) != 0)
    {
        door_failed(said, folder);
        return 0;
    }
    cli_full(folder, "door", address);
    struct sockaddr_un bound;
    if (!door_address(address, &bound))
    {
        cli_line_text(said, address);
        cli_line_text(said, ": the path is longer than a socket's name holds");
        return 0;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        door_failed(said, address);
        return 0;
    }
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    if ((bind(fd, (const struct sockaddr *)&bound, sizeof(bound)) != 0) || (listen(fd, 128) != 0))
    {
        door_failed(said, address);
        close(fd);
        return 0;
    }
    listener->lock = (CliLock)CLI_LOCK_INIT;
    // a descriptor that opened is not negative
    listener->waiting = (unsigned long long)fd;
    return 1;
}

// the process at the other end of the socket `fd`, as the system says: 1 with it into `peer`, or 0 where it does not
static int door_peer(int fd, unsigned int *peer)
{
#if defined(__linux__)
    struct ucred credentials;
    socklen_t length = sizeof(credentials);
    const int read_well = (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0);
    *peer = read_well ? (unsigned int)credentials.pid : 0u;
    return read_well;
#elif defined(__APPLE__)
    // LOCAL_PEERPID of SOL_LOCAL gives the process
    int pid = 0;
    socklen_t length = sizeof(pid);
    const int read_well = (getsockopt(fd, 0, 2, &pid, &length) == 0);
    *peer = read_well ? (unsigned int)pid : 0u;
    return read_well;
#else
    (void)fd;
    *peer = 0u;
    return 0;
#endif
}

int cli_door_accepted(CliDoorListener *listener, CliDoorEnd *end, int *has_peer, unsigned int *peer, CliLine *said)
{
    cli_lock_take(&listener->lock);
    // the socket was a descriptor when it was kept
    const int socket_fd = (int)listener->waiting;
    cli_lock_give(&listener->lock);
    int fd = -1;
    do
    {
        fd = accept(socket_fd, NULL, NULL);
    } while ((fd < 0) && (errno == EINTR));
    if (fd < 0)
    {
        door_failed(said, "no process joined");
        return 0;
    }
    door_end_made(end, fd);
    *has_peer = door_peer(fd, peer);
    return 1;
}

int cli_door_connected(const char *address, CliDoorEnd *end, CliLine *said)
{
    struct sockaddr_un joined;
    if (!door_address(address, &joined))
    {
        cli_line_text(said, address);
        cli_line_text(said, ": the path is longer than a socket's name holds");
        return 0;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        door_failed(said, address);
        return 0;
    }
    if (connect(fd, (const struct sockaddr *)&joined, sizeof(joined)) != 0)
    {
        door_failed(said, address);
        close(fd);
        return 0;
    }
    door_end_made(end, fd);
    return 1;
}

int cli_door_read(CliDoorEnd *end, void *bytes, unsigned long long size)
{
    unsigned char *const into = (unsigned char *)bytes;
    for (unsigned long long at = 0ull; at < size;)
    {
        const long long got = cli_socket_received((int)end->handle, into + at, size - at, &end->kept);
        if (got <= 0ll)
        {
            return 0;
        }
        at += (unsigned long long)got;
    }
    return 1;
}

int cli_door_written(CliDoorEnd *end, const void *bytes, unsigned long long size, const CliRegion *region)
{
    if (region != NULL)
    {
        return cli_socket_sent_with((int)end->handle, bytes, size, (int)region->section);
    }
    const unsigned char *const from = (const unsigned char *)bytes;
#if defined(MSG_NOSIGNAL)
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    for (unsigned long long at = 0ull; at < size;)
    {
        const ssize_t wrote = send((int)end->handle, from + at, (size_t)(size - at), flags);
        if ((wrote < 0) && (errno == EINTR))
        {
            continue;
        }
        if (wrote <= 0)
        {
            return 0;
        }
        at += (unsigned long long)wrote;
    }
    return 1;
}

int cli_door_descriptor(CliDoorEnd *end)
{
    return cli_descriptor_taken(&end->kept);
}

void cli_door_shut(CliDoorEnd *end)
{
    if (end->open)
    {
        (void)shutdown((int)end->handle, SHUT_RDWR);
    }
}

void cli_door_closed(CliDoorEnd *end)
{
    if (end->open)
    {
        close((int)end->handle);
        cli_descriptors_closed(&end->kept);
    }
    end->open = 0;
    end->handle = 0ull;
}
#endif
