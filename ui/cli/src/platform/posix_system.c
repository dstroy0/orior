// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_system.c: what the command line requests of Linux and macOS themselves: their randomness from /dev/urandom,
// the folder TMPDIR names, else /tmp, when the machine started, and the reader's log in; an event a started process
// inherits is Windows' alone
#if !defined(_WIN32)
// O_CLOEXEC is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(__APPLE__)
// sysctlbyname is the system's own, outside POSIX
#define _DARWIN_C_SOURCE
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

int cli_random_bytes(void *bytes, unsigned long long size)
{
    const int source = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (source < 0)
    {
        return 0;
    }
    unsigned char *const into = (unsigned char *)bytes;
    unsigned long long given = 0ull;
    while (given < size)
    {
        const ssize_t read_now = read(source, into + given, (size_t)(size - given));
        if (read_now <= 0)
        {
            close(source);
            return 0;
        }
        given += (unsigned long long)read_now;
    }
    close(source);
    return 1;
}

void cli_temp_folder(char *folder)
{
    if (!query_environment_read("TMPDIR", folder, CLI_PATH_MAX_LEN) || (folder[0] == '\0'))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = "/tmp",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "/tmp", .cap = CLI_CELLS_POOL}) + 1u});
        return;
    }
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    while ((end > 1u) && (folder[end - 1u] == '/'))
    {
        end -= 1u;
    }
    folder[end] = '\0';
}

unsigned long long cli_event_inheritable(void)
{
    return 0ull;
}

int cli_event_set(unsigned long long event)
{
    (void)event;
    return 0;
}

void cli_event_closed(unsigned long long event)
{
    (void)event;
}

int cli_private_folder_made(const char *folder)
{
    return query_folder_made(folder) && (chmod(folder, 0700) == 0);
}

int cli_private_file_written(const char *path, const void *bytes, unsigned long long size)
{
    const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (file < 0)
    {
        return 0;
    }
    const unsigned char *const from = (const unsigned char *)bytes;
    unsigned long long written = 0ull;
    while (written < size)
    {
        const ssize_t wrote = write(file, from + written, (size_t)(size - written));
        if (wrote <= 0)
        {
            break;
        }
        written += (unsigned long long)wrote;
    }
    return (close(file) == 0) && (written == size);
}

#if defined(__linux__)
// the most bytes of /proc/stat and of a session's id read
#define CLI_SYSTEM_STAT_MOST 65536u

// the file at `path` read into `out`, sized for `room`, a 0 after what was read: the bytes read, 0 where it cannot be
static unsigned long long system_read(const char *path, char *out, unsigned long long room)
{
    const int file = open(path, O_RDONLY | O_CLOEXEC);
    unsigned long long used = 0ull;
    while ((file >= 0) && (used + 1ull < room))
    {
        const ssize_t count = read(file, out + used, (size_t)(room - 1ull - used));
        if (count <= 0)
        {
            break;
        }
        used += (unsigned long long)count;
    }
    if (file >= 0)
    {
        close(file);
    }
    out[used] = '\0';
    return used;
}

// the machine's start as the kernel keeps it, the line `btime` of /proc/stat
unsigned long long cli_booted_seconds(void)
{
    static char s_stat[CLI_SYSTEM_STAT_MOST];
    static CliLock s_stat_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_stat_lock);
    (void)system_read("/proc/stat", s_stat, sizeof(s_stat));
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = s_stat, .cap = sizeof(s_stat), .other = "\nbtime ", .other_cap = CLI_CELLS_POOL});
    unsigned long long seconds = 0ull;
    for (at = (at != NULL) ? at + 7 : s_stat + sizeof(s_stat) - 1u; (*at >= '0') && (*at <= '9'); at += 1)
    {
        seconds = (seconds * 10ull) + (unsigned long long)(*at - '0');
    }
    cli_lock_give(&s_stat_lock);
    return seconds;
}

// the audit session the kernel gives each log in, or the desktop's own name for the session where the kernel keeps none
void cli_logon_named(char *logon, size_t room)
{
    char session[64];
    unsigned long long length = system_read("/proc/self/sessionid", session, sizeof(session));
    while ((length > 0ull) && ((session[length - 1ull] == '\n') || (session[length - 1ull] == ' ')))
    {
        length -= 1ull;
    }
    session[length] = '\0';
    const int kept =
        (length > 0ull) && !cellul.eq(&(CatenaFinitaCfg){.src = session, .other = "4294967295", .cap = CLI_CELLS_POOL});
    VerbaLinea named = {logon, room, 0ull};
    if (kept)
    {
        mmgr_line_put(&named, session);
        (void)mmgr_line_finish(&named);
    }
    else if (!query_environment_read("XDG_SESSION_ID", logon, room))
    {
        (void)mmgr_line_finish(&named);
    }
}
#elif defined(__APPLE__)
// the machine's start as sysctl keeps it
unsigned long long cli_booted_seconds(void)
{
    struct timeval booted;
    size_t size = sizeof(booted);
    // a start the system gives is after 1970, never below 0
    return (sysctlbyname("kern.boottime", &booted, &size, NULL, 0u) == 0) ? (unsigned long long)booted.tv_sec : 0ull;
}

// the reader's log in is not named on macOS; the machine's start alone marks a session there
void cli_logon_named(char *logon, size_t room)
{
    VerbaLinea named = {logon, room, 0ull};
    (void)mmgr_line_finish(&named);
}
#else
unsigned long long cli_booted_seconds(void)
{
    return 0ull;
}

void cli_logon_named(char *logon, size_t room)
{
    VerbaLinea named = {logon, room, 0ull};
    (void)mmgr_line_finish(&named);
}
#endif

#endif
