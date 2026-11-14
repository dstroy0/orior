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

// the max descriptors one read brings
#define CLI_DESCRIPTORS_READ 16u

typedef struct
{
#if (!defined(_WIN32))
    CliDescriptors *pending;
    int fd;
#endif
} PosixDescriptorsCtx;
// a descriptor that came queued, or closed where the queued are full
EMBED_INLINE void descriptors_append(const PosixDescriptorsCtx *args)
{
    (void)fcntl(args->fd, F_SETFD, FD_CLOEXEC);
    if (args->pending->count < CLI_DESCRIPTORS_PENDING_MAX)
    {
        args->pending->fd[(args->pending->start + args->pending->count) % CLI_DESCRIPTORS_PENDING_MAX] = args->fd;
        args->pending->count += 1u;
        return;
    }
    close(args->fd);
}

long long cli_socket_receive(const CliTransferSocketCfg *args)
{
    unsigned long long capacity = args->capacity;
    CliDescriptors *pending = args->pending;
    union {
        struct cmsghdr header;
        unsigned char capacity[CMSG_SPACE(sizeof(int) * CLI_DESCRIPTORS_READ)];
    } control;
    struct iovec piece = {args->buffer, (embed_word)capacity};
    struct msghdr message;
    ssize_t got = -1;
    for (;;)
    {
        memor.set(&(MemoriaCfg){.dst = &message, .val = 0, .bytes = sizeof(message)});
        message.msg_iov = &piece;
        message.msg_iovlen = 1;
        message.msg_control = control.capacity;
        message.msg_controllen = sizeof(control.capacity);
#if defined(MSG_CMSG_CLOEXEC)
        got = recvmsg(args->socket, &message, MSG_CMSG_CLOEXEC);
#else
        got = recvmsg(args->socket, &message, 0);
#endif
        if ((got >= 0) || (errno != EINTR))
        {
            break;
        }
    }
    if (got < 0)
    {
        return -1LL;
    }
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header != NULL; header = CMSG_NXTHDR(&message, header))
    {
        if ((header->cmsg_level != SOL_SOCKET) || (header->cmsg_type != SCM_RIGHTS))
        {
            continue;
        }
        const embed_word count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (embed_word one = 0u; one < count; one += 1u)
        {
            int fd = -1;
            memor.cpy(&(MemoriaCfg){.dst = &fd, .src = CMSG_DATA(header) + (one * sizeof(int)), .bytes = sizeof(int)});
            EMBED_CALL(descriptors_append, PosixDescriptorsCtx, .pending = pending, .fd = fd);
        }
    }
    return (long long)got;
}

int cli_descriptor_alloc(const CliTransferSocketCfg *args)
{
    int fd = -1;
    if (args->pending->count > 0u)
    {
        fd = args->pending->fd[args->pending->start];
        args->pending->start = (args->pending->start + 1u) % CLI_DESCRIPTORS_PENDING_MAX;
        args->pending->count -= 1u;
    }
    return fd;
}

void cli_descriptors_close(const CliTransferSocketCfg *args)
{
    for (int fd = cli_descriptor_alloc(args); fd >= 0; fd = cli_descriptor_alloc(args))
    {
        close(fd);
    }
}
#endif
