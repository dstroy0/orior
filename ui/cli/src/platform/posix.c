// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix.c: the command line's calls on Linux and macOS: the container is this process and every process under it in
// the system's list, less what it started outside, each its own process group; the system's processes listed, from
// /proc on Linux and proc_listpids on macOS; every member ended, asked first and made to after; and this process's id,
// program and folder
#if !defined(_WIN32)
// the process calls, the clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "../../../../src/cu/includes/parsers/line_read.h"
#include "../comms/container.h"

#if !defined(_WIN32)
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
#include "../text/text.h"
#if defined(__APPLE__)
#include <libproc.h>
#include <mach-o/dyld.h>
#endif

// the processes started outside the container, each its own process group, which the container leaves out
static unsigned int s_outside[1024];
static unsigned int s_outsides;

// what the walk over /proc keeps: the processes found, their count and room
typedef struct
{
    CliProcess *processes;
    unsigned int count;
    unsigned int room;
} CliProcWalk;

#if defined(__linux__)
static void cli_proc_walked(void *context, const char *name, int is_folder);
#endif

int cli_container_held(void)
{
    return 1;
}

int cli_started_outside(const char *const *words, unsigned int *id)
{
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
        execvp(words[0], (char *const *)words);
        _exit(127);
    }
    // a process number from fork is positive, and an unsigned int holds it
    *id = (unsigned int)child;
    if (s_outsides < 1024u)
    {
        s_outside[s_outsides] = *id;
        s_outsides += 1u;
    }
    return 1;
}

unsigned int cli_processes_listed(CliProcess *processes, unsigned int room)
{
    unsigned int count = 0u;
#if defined(__linux__)
    // each numbered folder of /proc, its stat's fourth field the parent, after the program's name in parentheses
    // which may hold spaces of its own
    CliProcWalk walk = {processes, 0u, room};
    query_folder_each("/proc", cli_proc_walked, &walk);
    count = walk.count;
#elif defined(__APPLE__)
    static int s_ids[8192];
    const int listed = proc_listpids(PROC_ALL_PIDS, 0u, s_ids, (int)sizeof(s_ids));
    for (int at = 0; (listed > 0) && (at < (listed / (int)sizeof(int))) && (count < room); at += 1)
    {
        struct proc_bsdinfo info;
        if (proc_pidinfo(s_ids[at], PROC_PIDTBSDINFO, 0u, &info, (int)sizeof(info)) == (int)sizeof(info))
        {
            // an id the system lists is positive
            processes[count].id = (unsigned int)s_ids[at];
            processes[count].parent = info.pbi_ppid;
            count += 1u;
        }
    }
#else
    (void)processes;
    (void)room;
#endif
    return count;
}

#if defined(__linux__)
// one entry of /proc read: where it is a process's folder, its id and its parent kept
static void cli_proc_walked(void *context, const char *name, int is_folder)
{
    CliProcWalk *const walk = (CliProcWalk *)context;
    if (!is_folder || (name[0] < '0') || (name[0] > '9') || (walk->count >= walk->room))
    {
        return;
    }
    char path[64];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, "/proc/");
    cli_line_text(&line, name);
    cli_line_text(&line, "/stat");
    char stat[1024];
    const unsigned long long size =
        (cli_line_closed(&line) != 0ull) ? qry_member_file_read(path, stat, sizeof(stat) - 1u) : QRY_MEMBER_UNREAD;
    if ((size == QRY_MEMBER_UNREAD) || (size == 0ull))
    {
        return;
    }
    stat[size] = '\0';
    const char *const close =
        cli_text_last(stat, cellul.len(&(CatenaFinitaCfg){.src = stat, .cap = CLI_CELLS_POOL}), ')');
    if (close == NULL)
    {
        return;
    }
    // past the parenthesis: the state, then the parent
    const char *at = close + 1;
    at += cli_text_over(at, " ");
    at += cli_text_upto(at, " ");
    at += cli_text_over(at, " ");
    unsigned long long parent = 0ull;
    unsigned long long id = 0ull;
    const char *id_at = name;
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

unsigned int cli_container_members(unsigned int *ids, unsigned int room)
{
    static CliProcess s_all[CLI_PROCESSES_MOST];
    static unsigned int s_found[CLI_PROCESSES_MOST];
    const unsigned int all = cli_processes_listed(s_all, CLI_PROCESSES_MOST);
    const unsigned int found = cli_descendants(cli_process_own(), s_all, all, s_found, CLI_PROCESSES_MOST);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < found) && (count < room); at += 1u)
    {
        int outside = 0;
        for (unsigned int each = 0u; !outside && (each < s_outsides); each += 1u)
        {
            outside = cli_under(s_outside[each], s_found[at], s_all, all);
        }
        if (!outside)
        {
            ids[count] = s_found[at];
            count += 1u;
        }
    }
    return count;
}

void cli_container_members_ended(void)
{
    static unsigned int s_ids[CLI_PROCESSES_MOST];
    const unsigned int count = cli_container_members(s_ids, CLI_PROCESSES_MOST);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        // kill takes any process id and signal number, and says by its answer whether the process was there
        kill((pid_t)s_ids[at], SIGTERM);
    }
    query_paused(300u);
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

int cli_program_own(char *path, size_t room)
{
#if defined(__linux__)
    const ssize_t length = readlink("/proc/self/exe", path, room - 1u);
    if (length <= 0)
    {
        return 0;
    }
    // a link's length read is positive and under the room
    path[length] = '\0';
    return 1;
#elif defined(__APPLE__)
    // a room is far below what a uint32_t counts
    uint32_t size = (uint32_t)room;
    return _NSGetExecutablePath(path, &size) == 0;
#else
    (void)path;
    (void)room;
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

unsigned int cli_arguments(int count, char **given, const char **words, unsigned int room)
{
    unsigned int taken = 0u;
    for (int at = 1; (at < count) && (taken < room); at += 1)
    {
        words[taken] = given[at];
        taken += 1u;
    }
    return taken;
}

void cli_console_let_go(void)
{
}

int cli_folder_current(char *path, size_t room)
{
    return getcwd(path, room) != NULL;
}

int cli_file_runnable(const char *path)
{
    return chmod(path, 0755) == 0;
}

int cli_is_link(const char *path)
{
    struct stat held;
    return (lstat(path, &held) == 0) && S_ISLNK(held.st_mode);
}

unsigned long long cli_clock_milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * 1000ull) + ((unsigned long long)now.tv_nsec / 1000000ull);
}

int cli_raised_platform(const char *from, const char *to, CliLine *said)
{
#if defined(__APPLE__)
    // the script `do shell script "/bin/cp '<from>' '<to>'" with administrator privileges`, each path quoted for the
    // shell and the script's quotes escaped
    static char s_script[(4u * CLI_PATH_LONGEST) + 128u];
    CliLine script = {s_script, sizeof(s_script), 0ull};
    cli_line_text(&script, "do shell script \"/bin/cp ");
    const char *const paths[2] = {from, to};
    for (unsigned int path = 0u; path < 2u; path += 1u)
    {
        cli_line_text(&script, (path == 0u) ? "'" : " '");
        for (const char *at = paths[path]; *at != '\0'; at += 1)
        {
            if (*at == '\'')
            {
                cli_line_text(&script, "'\\\\''");
            }
            else if (*at == '"')
            {
                cli_line_text(&script, "\\\"");
            }
            else
            {
                cli_line_character(&script, *at);
            }
        }
        cli_line_character(&script, '\'');
    }
    cli_line_text(&script, "\" with administrator privileges");
    if (cli_line_closed(&script) == 0ull)
    {
        return 0;
    }
    const char *const words[4] = {"/usr/bin/osascript", "-e", s_script, NULL};
    QueryProcess asked;
    if (!query_process_start(&asked, words, NULL, QUERY_OUTPUT_NONE) || (query_process_wait(&asked) != 0ll))
    {
        cli_line_text(said, "the system's authorization did not write ");
        cli_line_text(said, to);
        return 0;
    }
    return 1;
#else
    const char *const words[6] = {"pkexec", "/bin/cp", "--", from, to, NULL};
    QueryProcess asked;
    if (!query_process_start(&asked, words, NULL, QUERY_OUTPUT_OWN))
    {
        cli_line_text(said, "pkexec: it could not be started");
        return 0;
    }
    const long long code = query_process_wait(&asked);
    if ((code == 126ll) || (code == 127ll))
    {
        cli_line_text(said, "the reader did not give the rights");
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "pkexec did not write ");
        cli_line_text(said, to);
        return 0;
    }
    return 1;
#endif
}

int cli_socket_connected(CliSocket *socket_held, const char *host, unsigned int port, CliLine *said)
{
    socket_held->handle = 0ull;
    char service[16];
    CliLine service_line = {service, sizeof(service), 0ull};
    cli_line_decimal(&service_line, port, 0u);
    cli_line_closed(&service_line);
    struct addrinfo hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *found = NULL;
    if (getaddrinfo(host, service, &hints, &found) != 0)
    {
        cli_line_text(said, host);
        cli_line_character(said, ':');
        cli_line_text(said, service);
        cli_line_text(said, ": the name was not found");
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
        cli_line_text(said, host);
        cli_line_character(said, ':');
        cli_line_text(said, service);
        cli_line_text(said, ": the connection was refused");
        return 0;
    }
    const int no_delay = 1;
    setsockopt(made, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
    // a descriptor is not negative; one past it so that 0 stands for none
    socket_held->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_written(const CliSocket *socket_held, const void *bytes, unsigned long long size)
{
    const int held = (int)(socket_held->handle - 1ull);
    unsigned long long put = 0ull;
    while (put < size)
    {
        const ssize_t sent = send(held, (const char *)bytes + put, (size_t)(size - put), 0);
        if (sent <= 0)
        {
            return 0;
        }
        put += (unsigned long long)sent;
    }
    return 1;
}

int cli_socket_read(const CliSocket *socket_held, void *bytes, unsigned long long size)
{
    const int held = (int)(socket_held->handle - 1ull);
    unsigned long long got = 0ull;
    while (got < size)
    {
        const ssize_t read = recv(held, (char *)bytes + got, (size_t)(size - got), 0);
        if (read <= 0)
        {
            return 0;
        }
        got += (unsigned long long)read;
    }
    return 1;
}

long long cli_socket_some(const CliSocket *socket_held, void *bytes, unsigned long long room)
{
    const ssize_t read = recv((int)(socket_held->handle - 1ull), (char *)bytes, (size_t)room, 0);
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

void cli_socket_waited(const CliSocket *socket_held, unsigned int milliseconds)
{
    struct timeval wait = {(time_t)(milliseconds / 1000u), (suseconds_t)((milliseconds % 1000u) * 1000u)};
    setsockopt((int)(socket_held->handle - 1ull), SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
}

void cli_socket_write_waited(const CliSocket *socket_held, unsigned int milliseconds)
{
    struct timeval wait = {(time_t)(milliseconds / 1000u), (suseconds_t)((milliseconds % 1000u) * 1000u)};
    setsockopt((int)(socket_held->handle - 1ull), SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait));
}

void cli_socket_ended(const CliSocket *socket_held)
{
    if (socket_held->handle != 0ull)
    {
        shutdown((int)(socket_held->handle - 1ull), SHUT_RDWR);
    }
}

void cli_socket_closed(CliSocket *socket_held)
{
    if (socket_held->handle != 0ull)
    {
        close((int)(socket_held->handle - 1ull));
    }
    socket_held->handle = 0ull;
}

int cli_port_free(unsigned int *port, CliLine *said)
{
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
        cli_line_text(said, "127.0.0.1: no port could be bound");
        return 0;
    }
    *port = ntohs(address.sin_port);
    return 1;
}

#endif
