// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix.c: the command line's calls on Linux and macOS: the container is this process and every process under it in
// the system's list, less what it started outside, each its own process group; the system's processes listed, from
// /proc on Linux and proc_listpids on macOS; every member ended, requested first and made to after; and this process's
// id, program and folder
#if !defined(_WIN32)
// the process calls, the clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "../../../../src/cu/includes/parsers/line_read.h"
#include "../command_line/callback_cfg.h"
#include "../comms/container.h"

#if !defined(_WIN32)
#include "../text/text.h"
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
#if defined(__APPLE__)
#include <crt_externs.h>
#include <libproc.h>
#include <mach-o/dyld.h>
#endif

// the processes started outside the container, each its own process group, which the container leaves out
static unsigned int s_outside[1024];
static unsigned int s_outsides;

// what the walk over /proc records: the processes found, their count and capacity
typedef struct
{
    CliProcess *processes;
    unsigned int count;
    unsigned int capacity;
} CliProcWalk;

#if defined(__linux__)
static void cli_proc_walk(const CliEachNameIsFolderCfg *args);
#endif

int cli_container_init(void)
{
    return 1;
}

int cli_started_outside(const CliWordsIdCtx *args)
{
    unsigned int *id = args->id;
    const pid_t child = fork();
    if (child < 0)
    {
        return 0;
    }
    if (child == 0)
    {
        setpgid(0, 0);
        // none of the starter's standard descriptors, so no reader of the starter's output waits on a process that
        // outlives it: each the null device
        const int none = open("/dev/null", O_RDWR);
        for (int descriptor = 0; (none >= 0) && (descriptor < 3); descriptor += 1)
        {
            (void)dup2(none, descriptor);
        }
        if (none > 2)
        {
            (void)close(none);
        }
        // exec takes its words as it was declared; it writes to none of them
        execvp(args->words[0], (char *const *)(uintptr_t)args->words);
        _exit(127);
    }
    // a process number from fork is positive, and an unsigned int stores it
    *id = (unsigned int)child;
    if (s_outsides < 1024u)
    {
        s_outside[s_outsides] = *id;
        s_outsides += 1u;
    }
    return 1;
}

unsigned int cli_processes_list(const CliCapacityProcessesCtx *args)
{
    unsigned int count = 0u;
#if defined(__linux__)
    // each numbered folder of /proc, its stat's fourth field the parent, after the program's name in parentheses
    // which may contain spaces of its own
    CliProcWalk walk = {args->processes, 0u, args->capacity_u32};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = "/proc", .each = cli_proc_walk,
               .context_bytess = &walk);
    count = walk.count;
#elif defined(__APPLE__)
    static int s_ids[8192];
    const int list_entries = proc_listpids(PROC_ALL_PIDS, 0u, s_ids, (int)sizeof(s_ids));
    for (int at = 0; (list_entries > 0) && (at < (list_entries / (int)sizeof(int))) && (count < args->capacity_u32);
         at += 1)
    {
        struct proc_bsdinfo info;
        if (proc_pidinfo(s_ids[at], PROC_PIDTBSDINFO, 0u, &info, (int)sizeof(info)) == (int)sizeof(info))
        {
            // an id the system lists is positive
            args->processes[count].id = (unsigned int)s_ids[at];
            args->processes[count].parent = info.pbi_ppid;
            count += 1u;
        }
    }
#else
#endif
    return count;
}

#if defined(__linux__)
// one entry of /proc read: where it is a process's folder, its id and its parent recorded
static void cli_proc_walk(const CliEachNameIsFolderCfg *args)
{
    CliProcWalk *const walk = (CliProcWalk *)args->context;
    if (!args->is_folder || (args->name[0] < '0') || (args->name[0] > '9') || (walk->count >= walk->capacity))
    {
        return;
    }
    char path[64];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, "/proc/");
    mmgr_line_put(&line, args->name);
    mmgr_line_put(&line, "/stat");
    char stat[1024];
    // the file's bytes are read as bytes into text that is read as letters
    const unsigned long long size = (mmgr_line_finish(&line) != 0ULL)
                                        ? qry_member_file_read(path, (unsigned char *)stat, sizeof(stat) - 1u)
                                        : QRY_MEMBER_UNREAD;
    // QRY_MEMBER_UNREAD stands above every size
    if ((size == 0ULL) || (size >= sizeof(stat)))
    {
        return;
    }
    stat[size] = '\0';
    const char *const close =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = stat,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = stat, .cap = CLI_CELLS_POOL}), .byte = ')');
    if (close == NULL)
    {
        return;
    }
    // past the parenthesis: the state, then the parent
    const char *at = close + 1;
    at += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = at, .set = " ");
    at += EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = at, .set = " ");
    at += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = at, .set = " ");
    unsigned long long parent = 0ULL;
    unsigned long long id = 0ULL;
    const char *id_at = args->name;
    if (!line_number(&at, 0, &parent) || !line_number(&id_at, 0, &id))
    {
        return;
    }
    // a process id fits an unsigned int
    walk->processes[walk->count].id = (unsigned int)id;
    walk->processes[walk->count].parent = (unsigned int)parent;
    walk->count += 1u;
}
#endif

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
    unsigned int count = 0u;
    unsigned int at = 0u;
    while ((at < found) && (count < capacity))
    {
        int outside = 0;
        unsigned int each = 0u;
        while (!outside && (each < s_outsides))
        {
            outside = EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = s_outside[each],
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

unsigned int cli_process_own(void)
{
    // this process's id is positive
    return (unsigned int)getpid();
}

int cli_program_own(const CliPathCapacityCtx *args)
{
#if defined(__linux__)
    // readlink ends nothing with a 0, and the capacity's last byte stays the 0 written here
    memor.set(&(MemoriaCfg){.dst = args->path_out, .val = 0, .bytes = args->capacity_bytes});
    return readlink("/proc/self/exe", args->path_out, args->capacity_bytes - 1u) > 0;
#elif defined(__APPLE__)
    // a capacity is far below what a uint32_t counts
    uint32_t size = (uint32_t)args->capacity_bytes;
    return _NSGetExecutablePath(args->path_out, &size) == 0;
#else
    return 0;
#endif
}

int cli_errors_terminal(void)
{
    return isatty(2);
}

int cli_output_terminal(void)
{
    return isatty(1);
}

// the most bytes of the words this program was started with, read where none are given
#define CLI_ARGUMENTS_TEXT (1u << 16u)

typedef struct
{
#if (!defined(_WIN32))
    const char **words;
    unsigned int capacity;
#endif
} PosixCtx;
// the words this program was started with, read of the system where its main hands none: on Linux /proc/self/cmdline,
// each word ended by a 0, and on macOS the words the system saved for the program
EMBED_INLINE unsigned int cli_arguments_own(const PosixCtx *args)
{
    unsigned int reserved = 0u;
#if defined(__linux__)
    static char s_text[CLI_ARGUMENTS_TEXT];
    const int file = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    unsigned long long used = 0ULL;
    while ((file >= 0) && (used + 1ULL < sizeof(s_text)))
    {
        const unsigned long long capacity = sizeof(s_text) - 1u - used;
        const ssize_t count = read(file, s_text + used, (size_t)capacity);
        // a count that is positive fits an unsigned long long
        if ((count <= 0) || ((unsigned long long)count > capacity))
        {
            break;
        }
        used += (unsigned long long)count;
    }
    if (file >= 0)
    {
        close(file);
    }
    s_text[used] = '\0';
    // the program's own name first, passed over
    unsigned long long at = cellul.len(&(CatenaFinitaCfg){.src = s_text, .cap = sizeof(s_text)}) + 1ULL;
    while ((at < used) && (reserved < args->capacity))
    {
        args->words[reserved] = s_text + at;
        reserved += 1u;
        at += cellul.len(&(CatenaFinitaCfg){.src = s_text + at, .cap = sizeof(s_text)}) + 1ULL;
    }
#elif defined(__APPLE__)
    const int count = *_NSGetArgc();
    char **const supplied = *_NSGetArgv();
    for (int at = 1; (at < count) && (reserved < args->capacity); at += 1)
    {
        args->words[reserved] = supplied[at];
        reserved += 1u;
    }
#else
#endif
    return reserved;
}

unsigned int cli_arguments(const CliCapacityCountCtx *args)
{
    if (args->supplied == NULL)
    {
        return EMBED_CALL(cli_arguments_own, PosixCtx, .words = args->words_read, .capacity = args->capacity_u32);
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

unsigned long long cli_clock_milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * 1000ULL) + ((unsigned long long)now.tv_nsec / 1000000ULL);
}

int cli_raised_platform(const CliErrMsgFromCtx *args)
{
#if defined(__APPLE__)
    // the script `do shell script "/bin/cp '<from>' '<to>'" with administrator privileges`, each path quoted for the
    // shell and the script's quotes escaped
    static char s_script[(4u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea script = {s_script, sizeof(s_script), 0ULL};
    mmgr_line_put(&script, "do shell script \"/bin/cp ");
    const char *const paths[2] = {args->from_text, args->to};
    for (unsigned int path = 0u; path < 2u; path += 1u)
    {
        mmgr_line_put(&script, (path == 0u) ? "'" : " '");
        const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = paths[path], .cap = CLI_CELLS_POOL});
        for (unsigned long long at = 0ULL; at < whole;)
        {
            const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                       .text = paths[path], .at = at, .size = whole, .set = "'\"");
            mmgr_line_put_n(&script, paths[path] + at, (size_t)(stop - at));
            if (stop < whole)
            {
                mmgr_line_put(&script, (paths[path][stop] == '\'') ? "'\\\\''" : "\\\"");
            }
            at = stop + 1ULL;
        }
        mmgr_line_ch(&script, '\'');
    }
    mmgr_line_put(&script, "\" with administrator privileges");
    if (mmgr_line_finish(&script) == 0ULL)
    {
        return 0;
    }
    const char *const words[4] = {"/usr/bin/osascript", "-e", s_script, NULL};
    QueryProcess child;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &child, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_NONE) ||
        (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &child) != 0LL))
    {
        mmgr_line_put(args->err_msg, "the system's authorization did not write ");
        mmgr_line_put(args->err_msg, args->to);
        return 0;
    }
    return 1;
#else
    const char *const words[6] = {"pkexec", "/bin/cp", "--", args->from_text, args->to, NULL};
    QueryProcess child;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &child, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_OWN))
    {
        mmgr_line_put(args->err_msg, "pkexec: it could not be started");
        return 0;
    }
    const long long code = EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &child);
    if ((code == 126LL) || (code == 127LL))
    {
        mmgr_line_put(args->err_msg, "the reader did not give the rights");
        return 0;
    }
    if (code != 0LL)
    {
        mmgr_line_put(args->err_msg, "pkexec did not write ");
        mmgr_line_put(args->err_msg, args->to);
        return 0;
    }
    return 1;
#endif
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
    // a descriptor is not negative; one past it so that 0 stands for none
    args->sock_out->handle = (unsigned long long)made + 1ULL;
    return 1;
}

int cli_socket_write(const CliBytesSizeCtx *args)
{
    const int fd = (int)(args->sock->handle - 1ULL);
    unsigned long long put = 0ULL;
    while (put < args->size)
    {
        const ssize_t sent = send(fd, (const char *)args->bytes + put, (size_t)(args->size - put), 0);
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
        const ssize_t read = recv(fd, (char *)args->bytes_out + got, (size_t)(args->size - got), 0);
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
    const ssize_t read = recv((int)(args->sock->handle - 1ULL), (char *)args->bytes_out, (size_t)args->capacity, 0);
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
