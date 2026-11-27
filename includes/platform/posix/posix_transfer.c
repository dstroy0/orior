// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_transfer.c: a region of the transfer layer on the POSIX systems, and a descriptor sent over a socket with a
// frame's first byte and queued as it comes with the bytes read. How a region's memory is made, kept fast once
// written, and taken from another process is the system's (linux/linux_transfer.c, macos/macos_transfer.c)
#if !defined(_WIN32)
// sendmsg's control headers are POSIX, outside strict C11. Each system reads the macro of its own name
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../../../ui/cli/src/comms/transfer.h"
#include "posix_systems.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

void cli_transfer_failed(VerbaLinea *err_msg, const char *what)
{
    const int error = errno;
    mmgr_line_put(err_msg, what);
    mmgr_line_put(err_msg, ": the system reported ");
    mmgr_line_decimal(err_msg, (unsigned long long)error, 1u);
}

int cli_transfer_region_made(unsigned long long size, VerbaLinea *err_msg)
{
    const int fd = cli_transfer_memory_opened(err_msg);
    if (fd < 0)
    {
        return -1;
    }
    // a region's size is at max CLI_TRANSFER_MAX, which an off_t holds
    if (ftruncate(fd, (off_t)size) != 0)
    {
        cli_transfer_failed(err_msg, "the region was not sized");
        close(fd);
        return -1;
    }
    return fd;
}

int cli_region_with(const CliTransferRegionBytesCfg *args)
{
    const void *bytes = args->bytes;
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
    if (args->size > CLI_TRANSFER_MAX)
    {
        mmgr_line_decimal(args->err_msg, args->size, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the max a region supports");
        return 0;
    }
    const unsigned long long capacity = (args->size > 0ULL) ? args->size : 1ULL;
    const int fd = cli_transfer_region_made(capacity, args->err_msg);
    if (fd < 0)
    {
        return 0;
    }
    void *view = mmap(NULL, (embed_word)capacity, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        cli_transfer_failed(args->err_msg, "the system showed no region");
        close(fd);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = view, .src = bytes, .bytes = (embed_word)args->size});
    view = cli_transfer_region_kept(fd, view, capacity, args->err_msg);
    if (view == NULL)
    {
        close(fd);
        return 0;
    }
    args->region->section = (unsigned long long)fd;
    args->region->view = (const unsigned char *)view;
    args->region->length = args->size;
    args->region->mapped = 1;
    return 1;
}

int cli_region_receive(const CliTransferRegionCfg *args)
{
    unsigned long long length = args->length;
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
    // the descriptor is the region's from now on, and closed on each path that fails
    const int fd = (int)args->handle;
    const off_t size = lseek(fd, 0, SEEK_END);
    const unsigned long long capacity = (length > 0ULL) ? length : 1ULL;
    if (length > CLI_TRANSFER_MAX)
    {
        mmgr_line_decimal(args->err_msg, length, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the max a region supports");
        close(fd);
        return 0;
    }
    if ((size < 0) || ((unsigned long long)size < length))
    {
        mmgr_line_put(args->err_msg, "the region contains ");
        mmgr_line_decimal_signed(args->err_msg, (long long)size);
        mmgr_line_put(args->err_msg, " bytes, and was declared to contain ");
        mmgr_line_decimal(args->err_msg, length, 1u);
        close(fd);
        return 0;
    }
    int section = -1;
    const void *const view = cli_transfer_region_taken(fd, length, capacity, args->err_msg, &section);
    if (view == NULL)
    {
        close(fd);
        return 0;
    }
    // a descriptor that the system gives is not negative, which an unsigned long long holds
    args->region->section = (unsigned long long)section;
    args->region->view = (const unsigned char *)view;
    args->region->length = length;
    args->region->mapped = 1;
    return 1;
}

void cli_region_close(const CliTransferRegionBytesCfg *args)
{
    if (args->region->mapped)
    {
        // the view's bytes given back as the system took them, writable or not
        munmap((void *)(embed_word)args->region->view,
               (embed_word)((args->region->length > 0ULL) ? args->region->length : 1ULL));
        close((int)args->region->section);
    }
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
}

int cli_socket_sent_with(const CliTransferSizeCfg *args)
{
    int fd = args->fd;
    if (args->size == 0ULL)
    {
        return 1;
    }
    union {
        struct cmsghdr header;
        unsigned char capacity[CMSG_SPACE(sizeof(int))];
    } control;
    memor.set(&(MemoriaCfg){.dst = &control, .val = 0, .bytes = sizeof(control)});
    struct iovec piece = {(void *)(embed_word)args->frame, 1u};
    struct msghdr message;
    memor.set(&(MemoriaCfg){.dst = &message, .val = 0, .bytes = sizeof(message)});
    message.msg_iov = &piece;
    message.msg_iovlen = 1;
    message.msg_control = control.capacity;
    message.msg_controllen = sizeof(control.capacity);
    struct cmsghdr *const header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int));
    memor.cpy(&(MemoriaCfg){.dst = CMSG_DATA(header), .src = &fd, .bytes = sizeof(int)});
#if defined(MSG_NOSIGNAL)
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    for (;;)
    {
        if (sendmsg(args->socket, &message, flags) == 1)
        {
            break;
        }
        if (errno != EINTR)
        {
            return 0;
        }
    }
    const unsigned char *const rest = (const unsigned char *)args->frame + 1;
    for (unsigned long long sent = 0ULL; sent < args->size - 1ULL;)
    {
        const ssize_t wrote = send(args->socket, rest + sent, (embed_word)(args->size - 1ULL - sent), flags);
        if ((wrote < 0) && (errno == EINTR))
        {
            continue;
        }
        if (wrote < 0)
        {
            return 0;
        }
        sent += (unsigned long long)wrote;
    }
    return 1;
}
#endif
