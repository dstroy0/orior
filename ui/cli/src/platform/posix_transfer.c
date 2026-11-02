// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_transfer.c: a region of the transfer layer on Linux and macOS: a memory file sealed against every change once
// written on Linux, an unlinked file in the reader's own temporary folder on macOS; and a descriptor sent over a socket
// with a frame's first byte, and queued as it comes with the bytes read
#if !defined(_WIN32)
// memfd_create and its seals are Linux's own; sendmsg's control headers are POSIX, outside strict C11
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
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdint.h>

#if defined(__linux__)
// the seals a region from another process carries before it is read: against writing, shrinking and growing, and
// against any of those seals taken off; and the calls that add them and read them, where the headers do not name them
#define CLI_SEALED 15
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034
#endif
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1u
#define MFD_ALLOW_SEALING 2u
#endif
#endif

typedef struct
{
#if (!defined(_WIN32))
    VerbaLinea *err_msg;
    const char *what;
    unsigned long long size;
    int fd;
#endif
} PosixTransferCtx;
// what the system reported of the last call that failed, after `what`
EMBED_INLINE void transfer_fail(const PosixTransferCtx *args)
{
    const int error = errno;
    mmgr_line_put(args->err_msg, args->what);
    mmgr_line_put(args->err_msg, ": the system reported ");
    mmgr_line_decimal(args->err_msg, (unsigned long long)error, 1u);
}

// a descriptor of new memory of `size` bytes no other process can find: -1 with why given where none is made
static int transfer_init(const PosixTransferCtx *args)
{
    unsigned long long size = args->size;
#if defined(__linux__)
    const int fd = memfd_create("orior-transfer", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the system made no region");
        return -1;
    }
#else
    unsigned char secret[8];
    char folder[CLI_PATH_MAX_LEN];
    char name[96];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = secret, .size = sizeof(secret)))
    {
        mmgr_line_put(args->err_msg, "the system gave no secret");
        return -1;
    }
    VerbaLinea named = {name, sizeof(name), 0ull};
    mmgr_line_put(&named, "orior-transfer-");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&named);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = path);
    const int fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = path);
        return -1;
    }
    (void)unlink(path);
#endif
    if (ftruncate(fd, (off_t)size) != 0)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the region was not sized");
        close(fd);
        return -1;
    }
    return fd;
}

int cli_region_with(const CliTransferRegionBytesCfg *args)
{
    const void *bytes = args->bytes;
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
    if (args->size > CLI_TRANSFER_MOST)
    {
        mmgr_line_decimal(args->err_msg, args->size, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the most a region supports");
        return 0;
    }
    const unsigned long long room = (args->size > 0ull) ? args->size : 1ull;
    const int fd = EMBED_CALL(transfer_init, PosixTransferCtx, .size = room, .err_msg = args->err_msg);
    if (fd < 0)
    {
        return 0;
    }
    void *view = mmap(NULL, (size_t)room, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the system showed no region");
        close(fd);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = view, .src = bytes, .bytes = (size_t)args->size});
#if defined(__linux__)
    // a region is sealed against writing only once no view of it can write
    munmap(view, (size_t)room);
    if (fcntl(fd, F_ADD_SEALS, CLI_SEALED) != 0)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the region was not sealed");
        close(fd);
        return 0;
    }
    view = mmap(NULL, (size_t)room, PROT_READ, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the system showed no region");
        close(fd);
        return 0;
    }
#endif
    args->region->section = (unsigned long long)fd;
    args->region->view = (const unsigned char *)view;
    args->region->length = args->size;
    args->region->mapped = 1;
    return 1;
}

// a refusal of the descriptor `fd` that came from another process: it closed, why given already
EMBED_INLINE int transfer_reject(const PosixTransferCtx *args)
{
    close(args->fd);
    return 0;
}

int cli_region_receive(const CliTransferRegionCfg *args)
{
    unsigned long long length = args->length;
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
    // the descriptor is the region's from now on, and closed on each path that fails
    const int fd = (int)args->handle;
    const off_t size = lseek(fd, 0, SEEK_END);
    const unsigned long long room = (length > 0ull) ? length : 1ull;
    if (length > CLI_TRANSFER_MOST)
    {
        mmgr_line_decimal(args->err_msg, length, 1u);
        mmgr_line_put(args->err_msg, " bytes is past the most a region supports");
        return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
    }
    if ((size < 0) || ((unsigned long long)size < length))
    {
        mmgr_line_put(args->err_msg, "the region contains ");
        mmgr_line_decimal_signed(args->err_msg, (long long)size);
        mmgr_line_put(args->err_msg, " bytes, and was declared to contain ");
        mmgr_line_decimal(args->err_msg, length, 1u);
        return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
    }
#if defined(__linux__)
    if ((fcntl(fd, F_GET_SEALS) & CLI_SEALED) != CLI_SEALED)
    {
        mmgr_line_put(args->err_msg, "the region is not sealed against change");
        return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
    }
    // the view is no longer than the region, which no one can shrink
    const void *const view = mmap(NULL, (size_t)room, PROT_READ, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the system showed no region");
        return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
    }
    args->region->section = (unsigned long long)fd;
#else
    // the process that made the region could change it still, so it is copied whole into a region of this process's
    // own, which no one else has
    const int own = EMBED_CALL(transfer_init, PosixTransferCtx, .size = room, .err_msg = args->err_msg);
    if (own < 0)
    {
        return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
    }
    void *const view = mmap(NULL, (size_t)room, PROT_READ | PROT_WRITE, MAP_SHARED, own, 0);
    if (view == MAP_FAILED)
    {
        EMBED_CALL(transfer_fail, PosixTransferCtx, .err_msg = args->err_msg, .what = "the system showed no region");
        close(own);
        return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
    }
    for (unsigned long long at = 0ull; at < length;)
    {
        const ssize_t got = pread(fd, (unsigned char *)view + at, (size_t)(length - at), (off_t)at);
        if (got <= 0)
        {
            mmgr_line_put(args->err_msg, "the region ended at ");
            mmgr_line_decimal(args->err_msg, at, 1u);
            mmgr_line_put(args->err_msg, " of its ");
            mmgr_line_decimal(args->err_msg, length, 1u);
            mmgr_line_put(args->err_msg, " bytes");
            munmap(view, (size_t)room);
            close(own);
            return EMBED_CALL(transfer_reject, PosixTransferCtx, .fd = fd);
        }
        at += (unsigned long long)got;
    }
    close(fd);
    args->region->section = (unsigned long long)own;
#endif
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
        munmap((void *)(uintptr_t)args->region->view,
               (size_t)((args->region->length > 0ull) ? args->region->length : 1ull));
        close((int)args->region->section);
    }
    memor.set(&(MemoriaCfg){.dst = args->region, .val = 0, .bytes = sizeof(*args->region)});
}

int cli_socket_sent_with(const CliTransferSizeCfg *args)
{
    int fd = args->fd;
    if (args->size == 0ull)
    {
        return 1;
    }
    union {
        struct cmsghdr header;
        unsigned char room[CMSG_SPACE(sizeof(int))];
    } control;
    memor.set(&(MemoriaCfg){.dst = &control, .val = 0, .bytes = sizeof(control)});
    struct iovec piece = {(void *)(uintptr_t)args->frame, 1u};
    struct msghdr message;
    memor.set(&(MemoriaCfg){.dst = &message, .val = 0, .bytes = sizeof(message)});
    message.msg_iov = &piece;
    message.msg_iovlen = 1;
    message.msg_control = control.room;
    message.msg_controllen = sizeof(control.room);
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
    for (unsigned long long sent = 0ull; sent < args->size - 1ull;)
    {
        const ssize_t wrote = send(args->socket, rest + sent, (size_t)(args->size - 1ull - sent), flags);
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
