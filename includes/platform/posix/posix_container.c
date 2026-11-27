// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_container.c: the command line's calls on the POSIX systems: the container is this process and every process
// under it in the system's list, less what it started outside, each its own process group; every member ended,
// requested first and made to after; this process's folder and words; and its sockets. The system's list of
// processes, this program's path and words, and a copy with raised rights are the system's (linux/linux_container.c,
// macos/macos_container.c)
#if !defined(_WIN32)
// the process calls, the clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../ui/cli/src/comms/container.h"
#include "../../parsers/line_read.h"
#include "posix_systems.h"

#if !defined(_WIN32)
#include "../../../ui/cli/src/text/text.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int cli_container_init(void)
{
    return 1;
}

unsigned int cli_container_members(const CliIdsCtx *args)
{
    unsigned int capacity = args->capacity_u32;
    static CliProcess s_all[CLI_PROCESSES_MAX];
    static unsigned int s_found[CLI_PROCESSES_MAX];
    const unsigned int all = EMBED_CALL(cli_capacity_processes_internal.processes_list, CliCapacityProcessesCtx,
                                        .processes = s_all, .capacity_u32 = CLI_PROCESSES_MAX);
    const unsigned int found =
        EMBED_CALL(cli_container_root.descendants, CliContainerRootCfg, .root = cli_process_own(), .parents = s_all,
                   .count = all, .found = s_found, .capacity = CLI_PROCESSES_MAX);
    const unsigned int *outsides = NULL;
    const unsigned int outside_count = cli_started_outside_listed(&outsides);
    unsigned int count = 0u;
    unsigned int at = 0u;
    while ((at < found) && (count < capacity))
    {
        int outside = 0;
        unsigned int each = 0u;
        while (!outside && (each < outside_count))
        {
            outside = EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = outsides[each],
                                 .id = s_found[at], .parents = s_all, .count = all);
            each += 1u;
        }
        if (!outside)
        {
            args->ids[count] = s_found[at];
            count += 1u;
        }
        at += 1u;
    }
    return count;
}

void cli_container_members_end(void)
{
    static unsigned int s_ids[CLI_PROCESSES_MAX];
    const unsigned int count =
        EMBED_CALL(cli_ids_internal.container_members, CliIdsCtx, .ids = s_ids, .capacity_u32 = CLI_PROCESSES_MAX);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        // kill takes any process id and signal number, and reports by its answer whether the process was there
        kill((pid_t)s_ids[at], SIGTERM);
    }
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 300u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        kill((pid_t)s_ids[at], SIGKILL);
    }
}

int cli_errors_terminal(void)
{
    return isatty(2);
}

int cli_output_terminal(void)
{
    return isatty(1);
}

// a pipe handle is the descriptor and one, as the spawn entries read it
unsigned long long cli_standard_input(void)
{
    return 1ULL;
}

int cli_machine_name(VerbaLinea *out)
{
    char own[256];
    own[0] = '\0';
    if (gethostname(own, sizeof(own) - 1u) != 0)
    {
        return 0;
    }
    own[sizeof(own) - 1u] = '\0';
    mmgr_line_put(out, own);
    return (own[0] != '\0') && mmgr_line_ok(out);
}

unsigned long long cli_standard_output(void)
{
    return 2ULL;
}

unsigned int cli_arguments(const CliCapacityCountCtx *args)
{
    if (args->supplied == NULL)
    {
        return cli_arguments_own_read(args->words_read, args->capacity_u32);
    }
    unsigned int reserved = 0u;
    for (int at = 1; (at < args->count_i32) && (reserved < args->capacity_u32); at += 1)
    {
        args->words_read[reserved] = args->supplied[at];
        reserved += 1u;
    }
    return reserved;
}

void cli_console_let_go(void)
{
    // this system gives a program no console of its own to let go of
}

int cli_folder_current(const CliPathCapacityCtx *args)
{
    return getcwd(args->path_out, args->capacity_bytes) != NULL;
}

int cli_file_runnable(const CliPathFileCtx *args)
{
    // whoever may read the file may run it, and no one gains a read
    struct stat st;
    return (stat(args->path, &st) == 0) &&
           (chmod(args->path, (st.st_mode & 07777u) | ((st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) >> 2u)) == 0);
}

int cli_file_runs(const CliPathFileCtx *args)
{
    struct stat st;
    return (stat(args->path, &st) == 0) && ((st.st_mode & S_IXUSR) != 0u);
}

int cli_is_link(const CliPathFileCtx *args)
{
    struct stat st;
    return (lstat(args->path, &st) == 0) && S_ISLNK(st.st_mode);
}

int cli_socket_connect(const CliErrMsgCtx *args)
{
    args->sock_out->handle = 0ULL;
    char service[16];
    VerbaLinea service_line = {service, sizeof(service), 0ULL};
    mmgr_line_decimal(&service_line, args->port, 0u);
    mmgr_line_finish(&service_line);
    struct addrinfo hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *found = NULL;
    if (getaddrinfo(args->host, service, &hints, &found) != 0)
    {
        mmgr_line_put(args->err_msg, args->host);
        mmgr_line_ch(args->err_msg, ':');
        mmgr_line_put(args->err_msg, service);
        mmgr_line_put(args->err_msg, ": the name was not found");
        return 0;
    }
    int made = -1;
    for (struct addrinfo *at = found; (at != NULL) && (made < 0); at = at->ai_next)
    {
        made = socket(at->ai_family, at->ai_socktype, at->ai_protocol);
        if ((made >= 0) && (connect(made, at->ai_addr, at->ai_addrlen) != 0))
        {
            close(made);
            made = -1;
        }
    }
    freeaddrinfo(found);
    if (made < 0)
    {
        mmgr_line_put(args->err_msg, args->host);
        mmgr_line_ch(args->err_msg, ':');
        mmgr_line_put(args->err_msg, service);
        mmgr_line_put(args->err_msg, ": the connection was refused");
        return 0;
    }
    const int no_delay = 1;
    setsockopt(made, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
    // a descriptor is not negative; one past it so that 0 is for none
    args->sock_out->handle = (unsigned long long)made + 1ULL;
    return 1;
}

int cli_socket_write(const CliBytesSizeCtx *args)
{
    const int fd = (int)(args->sock->handle - 1ULL);
    unsigned long long put = 0ULL;
    while (put < args->size)
    {
        const ssize_t sent = send(fd, (const char *)args->bytes + put, (embed_word)(args->size - put), 0);
        if (sent <= 0)
        {
            return 0;
        }
        put += (unsigned long long)sent;
    }
    return 1;
}

int cli_socket_read(const CliSizeBytesCtx *args)
{
    const int fd = (int)(args->sock->handle - 1ULL);
    unsigned long long got = 0ULL;
    while (got < args->size)
    {
        const ssize_t read = recv(fd, (char *)args->bytes_out + got, (embed_word)(args->size - got), 0);
        if (read <= 0)
        {
            return 0;
        }
        got += (unsigned long long)read;
    }
    return 1;
}

long long cli_socket_some(const CliCapacityCtx *args)
{
    const ssize_t read = recv((int)(args->sock->handle - 1ULL), (char *)args->bytes_out, (embed_word)args->capacity, 0);
    if (read > 0)
    {
        return (long long)read;
    }
    if (read == 0)
    {
        return CLI_SOCKET_CLOSED;
    }
    return ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ? CLI_SOCKET_WAITED : CLI_SOCKET_FAILED;
}

void cli_socket_wait(const CliSockCtx *args)
{
    struct timeval wait = {(time_t)(args->milliseconds / 1000u), (suseconds_t)((args->milliseconds % 1000u) * 1000u)};
    setsockopt((int)(args->sock->handle - 1ULL), SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
}

void cli_socket_write_wait(const CliSockCtx *args)
{
    struct timeval wait = {(time_t)(args->milliseconds / 1000u), (suseconds_t)((args->milliseconds % 1000u) * 1000u)};
    setsockopt((int)(args->sock->handle - 1ULL), SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait));
}

void cli_socket_end(const CliSockCtx *args)
{
    if (args->sock->handle != 0ULL)
    {
        shutdown((int)(args->sock->handle - 1ULL), SHUT_RDWR);
    }
}

void cli_socket_close(const CliErrMsgCtx *args)
{
    if (args->sock_out->handle != 0ULL)
    {
        close((int)(args->sock_out->handle - 1ULL));
    }
    args->sock_out->handle = 0ULL;
}

int cli_port_free(const CliErrMsgPortCtx *args)
{
    unsigned int *port = args->port_pointer;
    const int made = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    memor.set(&(MemoriaCfg){.dst = &address, .val = 0, .bytes = sizeof(address)});
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = (socklen_t)sizeof(address);
    const int bound = (made >= 0) && (bind(made, (const struct sockaddr *)&address, size) == 0) &&
                      (getsockname(made, (struct sockaddr *)&address, &size) == 0);
    if (made >= 0)
    {
        close(made);
    }
    if (!bound)
    {
        mmgr_line_put(args->err_msg, "127.0.0.1: no port could be bound");
        return 0;
    }
    *port = ntohs(address.sin_port);
    return 1;
}

#endif
