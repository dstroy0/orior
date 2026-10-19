// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_descriptors.c: what comes on a socket read, each descriptor that comes with the bytes kept in the order it
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

// a descriptor that came kept, or closed where the kept are full
static void descriptors_kept(CliDescriptors *kept, int fd)
{
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    cli_lock_take(&kept->lock);
    if (kept->count < CLI_DESCRIPTORS_KEPT)
    {
        kept->fd[(kept->start + kept->count) % CLI_DESCRIPTORS_KEPT] = fd;
        kept->count += 1u;
        cli_lock_give(&kept->lock);
        return;
    }
    cli_lock_give(&kept->lock);
    close(fd);
}

long long cli_socket_received(int socket, void *buffer, unsigned long long room, CliDescriptors *kept)
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
            descriptors_kept(kept, fd);
        }
    }
    return (long long)got;
}

int cli_descriptor_taken(CliDescriptors *kept)
{
    cli_lock_take(&kept->lock);
    int fd = -1;
    if (kept->count > 0u)
    {
        fd = kept->fd[kept->start];
        kept->start = (kept->start + 1u) % CLI_DESCRIPTORS_KEPT;
        kept->count -= 1u;
    }
    cli_lock_give(&kept->lock);
    return fd;
}

void cli_descriptors_closed(CliDescriptors *kept)
{
    for (int fd = cli_descriptor_taken(kept); fd >= 0; fd = cli_descriptor_taken(kept))
    {
        close(fd);
    }
}
#endif
