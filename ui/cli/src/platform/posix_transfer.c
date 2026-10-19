// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_transfer.c: a region of the transfer layer on Linux and macOS: a memory file sealed against every change once
// written on Linux, an unlinked file in the reader's own temporary folder on macOS; and a descriptor sent over a socket
// with a frame's first byte, and kept as it comes with the bytes read
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
// the seals a region from another process holds before it is read: against writing, shrinking and growing, and
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

// what the system said of the last call that failed, after `what`
static void transfer_failed(CliLine *said, const char *what)
{
    const int error = errno;
    cli_line_text(said, what);
    cli_line_text(said, ": the system said ");
    cli_line_decimal(said, (unsigned long long)error, 1u);
}

// a descriptor of new memory of `size` bytes no other process can find: -1 with why said where none is made
static int transfer_made(unsigned long long size, CliLine *said)
{
#if defined(__linux__)
    const int fd = memfd_create("orior-transfer", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
    {
        transfer_failed(said, "the system made no region");
        return -1;
    }
#else
    unsigned char secret[8];
    char folder[CLI_PATH_LONGEST];
    char name[96];
    char path[CLI_PATH_LONGEST];
    if (!cli_random_bytes(secret, sizeof(secret)))
    {
        cli_line_text(said, "the system gave no secret");
        return -1;
    }
    CliLine named = {name, sizeof(name), 0ull};
    cli_line_text(&named, "orior-transfer-");
    cli_line_decimal(&named, cli_process_own(), 1u);
    cli_line_character(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        cli_line_character(&named, "0123456789abcdef"[secret[at] >> 4u]);
        cli_line_character(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    cli_line_closed(&named);
    cli_temp_folder(folder);
    cli_full(folder, name, path);
    const int fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
    {
        transfer_failed(said, path);
        return -1;
    }
    (void)unlink(path);
#endif
    if (ftruncate(fd, (off_t)size) != 0)
    {
        transfer_failed(said, "the region was not sized");
        close(fd);
        return -1;
    }
    return fd;
}

int cli_region_with(CliRegion *region, const void *bytes, unsigned long long size, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
    if (size > CLI_TRANSFER_MOST)
    {
        cli_line_decimal(said, size, 1u);
        cli_line_text(said, " bytes is past the most a region holds");
        return 0;
    }
    const unsigned long long room = (size > 0ull) ? size : 1ull;
    const int fd = transfer_made(room, said);
    if (fd < 0)
    {
        return 0;
    }
    void *view = mmap(NULL, (size_t)room, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        transfer_failed(said, "the system showed no region");
        close(fd);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = view, .src = bytes, .bytes = (size_t)size});
#if defined(__linux__)
    // a region is sealed against writing only once no view of it can write
    munmap(view, (size_t)room);
    if (fcntl(fd, F_ADD_SEALS, CLI_SEALED) != 0)
    {
        transfer_failed(said, "the region was not sealed");
        close(fd);
        return 0;
    }
    view = mmap(NULL, (size_t)room, PROT_READ, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        transfer_failed(said, "the system showed no region");
        close(fd);
        return 0;
    }
#endif
    region->section = (unsigned long long)fd;
    region->view = (const unsigned char *)view;
    region->length = size;
    region->held = 1;
    return 1;
}

// a refusal of the descriptor `fd` that came from another process: it closed, why said already
static int transfer_refused(int fd)
{
    close(fd);
    return 0;
}

int cli_region_received(CliRegion *region, unsigned long long handle, unsigned long long length, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
    // the descriptor is the region's from now on, and closed on each path that fails
    const int fd = (int)handle;
    const off_t size = lseek(fd, 0, SEEK_END);
    const unsigned long long room = (length > 0ull) ? length : 1ull;
    if (length > CLI_TRANSFER_MOST)
    {
        cli_line_decimal(said, length, 1u);
        cli_line_text(said, " bytes is past the most a region holds");
        return transfer_refused(fd);
    }
    if ((size < 0) || ((unsigned long long)size < length))
    {
        cli_line_text(said, "the region holds ");
        cli_line_signed(said, (long long)size);
        cli_line_text(said, " bytes, and was said to hold ");
        cli_line_decimal(said, length, 1u);
        return transfer_refused(fd);
    }
#if defined(__linux__)
    if ((fcntl(fd, F_GET_SEALS) & CLI_SEALED) != CLI_SEALED)
    {
        cli_line_text(said, "the region is not sealed against change");
        return transfer_refused(fd);
    }
    // the view is no longer than the region, which no one can shrink
    const void *const view = mmap(NULL, (size_t)room, PROT_READ, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        transfer_failed(said, "the system showed no region");
        return transfer_refused(fd);
    }
    region->section = (unsigned long long)fd;
#else
    // the process that made the region could change it still, so it is copied whole into a region of this process's
    // own, which no one else holds
    const int own = transfer_made(room, said);
    if (own < 0)
    {
        return transfer_refused(fd);
    }
    void *const view = mmap(NULL, (size_t)room, PROT_READ | PROT_WRITE, MAP_SHARED, own, 0);
    if (view == MAP_FAILED)
    {
        transfer_failed(said, "the system showed no region");
        close(own);
        return transfer_refused(fd);
    }
    for (unsigned long long at = 0ull; at < length;)
    {
        const ssize_t got = pread(fd, (unsigned char *)view + at, (size_t)(length - at), (off_t)at);
        if (got <= 0)
        {
            cli_line_text(said, "the region ended at ");
            cli_line_decimal(said, at, 1u);
            cli_line_text(said, " of its ");
            cli_line_decimal(said, length, 1u);
            cli_line_text(said, " bytes");
            munmap(view, (size_t)room);
            close(own);
            return transfer_refused(fd);
        }
        at += (unsigned long long)got;
    }
    close(fd);
    region->section = (unsigned long long)own;
#endif
    region->view = (const unsigned char *)view;
    region->length = length;
    region->held = 1;
    return 1;
}

void cli_region_closed(CliRegion *region)
{
    if (region->held)
    {
        // the view's bytes given back as the system took them, writable or not
        munmap((void *)(uintptr_t)region->view, (size_t)((region->length > 0ull) ? region->length : 1ull));
        close((int)region->section);
    }
    memor.set(&(MemoriaCfg){.dst = region, .val = 0, .bytes = sizeof(*region)});
}

int cli_socket_sent_with(int socket, const void *frame, unsigned long long size, int fd)
{
    if (size == 0ull)
    {
        return 1;
    }
    union {
        struct cmsghdr header;
        unsigned char room[CMSG_SPACE(sizeof(int))];
    } control;
    memor.set(&(MemoriaCfg){.dst = &control, .val = 0, .bytes = sizeof(control)});
    struct iovec piece = {(void *)(uintptr_t)frame, 1u};
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
        if (sendmsg(socket, &message, flags) == 1)
        {
            break;
        }
        if (errno != EINTR)
        {
            return 0;
        }
    }
    const unsigned char *const rest = (const unsigned char *)frame + 1;
    for (unsigned long long sent = 0ull; sent < size - 1ull;)
    {
        const ssize_t wrote = send(socket, rest + sent, (size_t)(size - 1ull - sent), flags);
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
