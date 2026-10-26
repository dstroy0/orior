// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_descriptors.c: what comes on a socket read, each descriptor that comes with the bytes queued in the order it
// came, marked to close as a program starts, until it is taken; those never taken are closed with the rest
#if !defined(_WIN32)
// sendmsg's control headers are POSIX, outside strict C11
#if defined(__linux__)
#define _GNU_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#endif
#include "../comms/transfer.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

// the most descriptors one read brings
#define CLI_DESCRIPTORS_READ 16u

// a descriptor that came queued, or closed where the queued are full
static void descriptors_put(CliDescriptors *pending, int fd)
{
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    cli_lock_take(&pending->lock);
    if (pending->count < CLI_DESCRIPTORS_PENDING_MAX)
    {
        pending->fd[(pending->start + pending->count) % CLI_DESCRIPTORS_PENDING_MAX] = fd;
        pending->count += 1u;
        cli_lock_give(&pending->lock);
        return;
    }
    cli_lock_give(&pending->lock);
    close(fd);
}

long long cli_socket_received(int socket, void *buffer, unsigned long long room, CliDescriptors *pending)
{
    union {
        struct cmsghdr header;
        unsigned char room[CMSG_SPACE(sizeof(int) * CLI_DESCRIPTORS_READ)];
    } control;
    struct iovec piece = {buffer, (size_t)room};
    struct msghdr message;
    ssize_t got = -1;
    for (;;)
    {
        memor.set(&(MemoriaCfg){.dst = &message, .val = 0, .bytes = sizeof(message)});
        message.msg_iov = &piece;
        message.msg_iovlen = 1;
        message.msg_control = control.room;
        message.msg_controllen = sizeof(control.room);
#if defined(MSG_CMSG_CLOEXEC)
        got = recvmsg(socket, &message, MSG_CMSG_CLOEXEC);
#else
        got = recvmsg(socket, &message, 0);
#endif
        if ((got >= 0) || (errno != EINTR))
        {
            break;
        }
    }
    if (got < 0)
    {
        return -1ll;
    }
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header != NULL; header = CMSG_NXTHDR(&message, header))
    {
        if ((header->cmsg_level != SOL_SOCKET) || (header->cmsg_type != SCM_RIGHTS))
        {
            continue;
        }
        const size_t count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (size_t one = 0u; one < count; one += 1u)
        {
            int fd = -1;
            memor.cpy(&(MemoriaCfg){.dst = &fd, .src = CMSG_DATA(header) + (one * sizeof(int)), .bytes = sizeof(int)});
            descriptors_put(pending, fd);
        }
    }
    return (long long)got;
}

int cli_descriptor_taken(CliDescriptors *pending)
{
    cli_lock_take(&pending->lock);
    int fd = -1;
    if (pending->count > 0u)
    {
        fd = pending->fd[pending->start];
        pending->start = (pending->start + 1u) % CLI_DESCRIPTORS_PENDING_MAX;
        pending->count -= 1u;
    }
    cli_lock_give(&pending->lock);
    return fd;
}

void cli_descriptors_closed(CliDescriptors *pending)
{
    for (int fd = cli_descriptor_taken(pending); fd >= 0; fd = cli_descriptor_taken(pending))
    {
        close(fd);
    }
}
#endif
