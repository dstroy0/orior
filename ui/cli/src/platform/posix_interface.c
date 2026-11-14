// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_ipc.c: the interface on Linux and macOS: a socket in a folder only the reader can open, the process at the other
// end of each connection as the system reports it, and each descriptor that comes with the bytes read queued for the
// frame that names it
#if !defined(_WIN32)
// the peer's credentials and the socket's calls are POSIX and the systems' own, outside strict C11
#if defined(__linux__)
#define _GNU_SOURCE
#else
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#endif
#include "../comms/interface_internal.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct
{
#if (!defined(_WIN32))
    VerbaLinea *err_msg;
    const char *what;
    const char *path;
    struct sockaddr_un *address;
    CliIpcEnd *end;
    int fd;
    unsigned int *peer;
#endif
} PosixInterfaceCtx;

// what the system reported of the last call that failed, after `what`
EMBED_INLINE void ipc_fail(const PosixInterfaceCtx *args)
{
    const int error = errno;
    mmgr_line_put(args->err_msg, args->what);
    mmgr_line_put(args->err_msg, ": the system reported ");
    mmgr_line_decimal(args->err_msg, (unsigned long long)error, 1u);
}

// the socket address of the path `path` into `address`: 1, or 0 where the path is longer than a socket's name fits
EMBED_INLINE int ipc_address(const PosixInterfaceCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->address, .val = 0, .bytes = sizeof(*args->address)});
    args->address->sun_family = AF_UNIX;
    if (cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) >= sizeof(args->address->sun_path))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->address->sun_path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    return 1;
}

// an end made of the socket `fd`, marked to close as a program starts
EMBED_INLINE void ipc_end_init(const PosixInterfaceCtx *args)
{
    (void)fcntl(args->fd, F_SETFD, FD_CLOEXEC);
#if defined(__APPLE__)
    // a write to an end the peer closed fails, and sends no signal, as MSG_NOSIGNAL has it elsewhere
    const int on = 1;
    (void)setsockopt(args->fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
    memor.set(&(MemoriaCfg){.dst = args->end, .val = 0, .bytes = sizeof(*args->end)});
    args->end->handle = (unsigned long long)args->fd;
    args->end->open = 1;
}

int cli_ipc_listening(const CliIpcListenerAddressCtx *args)
{
    unsigned char secret[8];
    char temporary[CLI_PATH_MAX_LEN];
    char name[96];
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = secret, .size = sizeof(secret)))
    {
        mmgr_line_put(args->err_msg, "the system gave no secret");
        return 0;
    }
    VerbaLinea named = {name, sizeof(name), 0ULL};
    mmgr_line_put(&named, "orior-comms-");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&named, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&named);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = folder);
    if (mkdir(folder, 0700) != 0)
    {
        EMBED_CALL(ipc_fail, PosixInterfaceCtx, .err_msg = args->err_msg, .what = folder);
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "interface", .joined = args->address);
    struct sockaddr_un bound;
    if (!EMBED_CALL(ipc_address, PosixInterfaceCtx, .path = args->address, .address = &bound))
    {
        mmgr_line_put(args->err_msg, args->address);
        mmgr_line_put(args->err_msg, ": the path is longer than a socket's name fits");
        return 0;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        EMBED_CALL(ipc_fail, PosixInterfaceCtx, .err_msg = args->err_msg, .what = args->address);
        return 0;
    }
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    if ((bind(fd, (const struct sockaddr *)&bound, sizeof(bound)) != 0) || (listen(fd, 128) != 0))
    {
        EMBED_CALL(ipc_fail, PosixInterfaceCtx, .err_msg = args->err_msg, .what = args->address);
        close(fd);
        return 0;
    }
    // a descriptor that opened is not negative
    args->listener->waiting = (unsigned long long)fd;
    return 1;
}

// the process at the other end of the socket `fd`, as the system reports: 1 with it into `peer`, or 0 where it does not
EMBED_INLINE int ipc_peer(const PosixInterfaceCtx *args)
{
    unsigned int *peer = args->peer;
#if defined(__linux__)
    struct ucred credentials;
    socklen_t length = sizeof(credentials);
    const int read_well = (getsockopt(args->fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0);
    *peer = read_well ? (unsigned int)credentials.pid : 0u;
    return read_well;
#elif defined(__APPLE__)
    // LOCAL_PEERPID of SOL_LOCAL gives the process
    int pid = 0;
    socklen_t length = sizeof(pid);
    const int read_well = (getsockopt(args->fd, 0, 2, &pid, &length) == 0);
    *peer = read_well ? (unsigned int)pid : 0u;
    return read_well;
#else
    *peer = 0u;
    return 0;
#endif
}

int cli_ipc_accept(const CliIpcListenerCtx *args)
{
    int *has_peer = args->has_peer;
    // the socket was a descriptor when it was stored
    const int socket_fd = (int)args->listener->waiting;
    int fd = -1;
    do
    {
        fd = accept(socket_fd, NULL, NULL);
    } while ((fd < 0) && (errno == EINTR));
    if (fd < 0)
    {
        EMBED_CALL(ipc_fail, PosixInterfaceCtx, .err_msg = args->err_msg, .what = "no process joined");
        return 0;
    }
    EMBED_CALL(ipc_end_init, PosixInterfaceCtx, .end = args->end, .fd = fd);
    *has_peer = EMBED_CALL(ipc_peer, PosixInterfaceCtx, .fd = fd, .peer = args->peer);
    return 1;
}

int cli_ipc_connect(const CliIpcCtx *args)
{
    struct sockaddr_un joined;
    if (!EMBED_CALL(ipc_address, PosixInterfaceCtx, .path = args->address_read, .address = &joined))
    {
        mmgr_line_put(args->err_msg, args->address_read);
        mmgr_line_put(args->err_msg, ": the path is longer than a socket's name fits");
        return 0;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        EMBED_CALL(ipc_fail, PosixInterfaceCtx, .err_msg = args->err_msg, .what = args->address_read);
        return 0;
    }
    if (connect(fd, (const struct sockaddr *)&joined, sizeof(joined)) != 0)
    {
        EMBED_CALL(ipc_fail, PosixInterfaceCtx, .err_msg = args->err_msg, .what = args->address_read);
        close(fd);
        return 0;
    }
    EMBED_CALL(ipc_end_init, PosixInterfaceCtx, .end = args->end, .fd = fd);
    return 1;
}

int cli_ipc_read(const CliIpcEndBytesCtx *args)
{
    unsigned char *const into = (unsigned char *)args->bytes;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const long long got =
            EMBED_CALL(cli_socket_receive, CliTransferSocketCfg, .socket = (int)args->end->handle, .buffer = into + at,
                       .capacity = args->size - at, .pending = &args->end->pending);
        if (got <= 0LL)
        {
            return 0;
        }
        at += (unsigned long long)got;
    }
    return 1;
}

int cli_ipc_write(const CliIpcEndSizeCtx *args)
{
    if (args->region != NULL)
    {
        return EMBED_CALL(cli_socket_sent_with, CliTransferSizeCfg, .socket = (int)args->end->handle,
                          .frame = args->bytes_read, .size = args->size, .fd = (int)args->region->section);
    }
    const unsigned char *const from = (const unsigned char *)args->bytes_read;
#if defined(MSG_NOSIGNAL)
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const ssize_t wrote = send((int)args->end->handle, from + at, (embed_word)(args->size - at), flags);
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

int cli_ipc_descriptor(const CliIpcErrMsgCtx *args)
{
    return EMBED_CALL(cli_descriptor_alloc, CliTransferSocketCfg, .pending = &args->end->pending);
}

void cli_ipc_shut(const CliIpcCtx *args)
{
    if (args->end->open)
    {
        (void)shutdown((int)args->end->handle, SHUT_RDWR);
    }
}

void cli_ipc_close(const CliIpcCtx *args)
{
    if (args->end->open)
    {
        close((int)args->end->handle);
        EMBED_CALL(cli_descriptors_close, CliTransferSocketCfg, .pending = &args->end->pending);
    }
    args->end->open = 0;
    args->end->handle = 0ULL;
}
#endif
