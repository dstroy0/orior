// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_system.c: what the command line requests of Linux and macOS themselves: their randomness from /dev/urandom,
// the folder TMPDIR or XDG_RUNTIME_DIR names, else one of the reader's own, when the machine started, and the reader's
// log in; an event a started process
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
#include "../text/text.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

void cli_clock_civil(const CliClockCivilCtx *args)
{
    CliCivil *const civil = args->civil_out;
    *civil = (CliCivil){1970u, 1u, 1u, 0u, 0u};
    // a reading before 1970 is read as 1970; the system's time counts the seconds from 1970 as the reading does
    const time_t since = (time_t)((args->seconds > 0LL) ? args->seconds : 0LL);
    struct tm local;
    if (localtime_r(&since, &local) == NULL)
    {
        return;
    }
    // the calendar's fields are not negative: the year is counted from 1900 and the month from 0
    *civil = (CliCivil){(unsigned int)(local.tm_year + 1900), (unsigned int)(local.tm_mon + 1),
                        (unsigned int)local.tm_mday, (unsigned int)local.tm_hour, (unsigned int)local.tm_min};
}

typedef struct
{
#if (!defined(_WIN32)) && (defined(__linux__))
    const char *path;
    char *out;
    unsigned long long capacity;
#endif
} PosixSystemCtx;

int cli_random_bytes(const CliSizeBytesCtx *args)
{
    const int source = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (source < 0)
    {
        return 0;
    }
    unsigned char *const into = (unsigned char *)args->bytes_out;
    unsigned long long supplied = 0ULL;
    while (supplied < args->size)
    {
        const ssize_t read_now = read(source, into + supplied, (size_t)(args->size - supplied));
        if (read_now <= 0)
        {
            close(source);
            return 0;
        }
        supplied += (unsigned long long)read_now;
    }
    close(source);
    return 1;
}

void *cli_pages_alloc(const CliSizeBytesCtx *args)
{
    // the pages are the C library's here, zero; no surface of the system's reads them where they lie
    return calloc(1u, (size_t)args->size);
}

void cli_pages_free(const CliSizeBytesCtx *args)
{
    free(args->bytes_out);
}

void cli_temp_folder(const CliFolderCtx *args)
{
    static const char *const s_named[2] = {"TMPDIR", "XDG_RUNTIME_DIR"};
    int found = 0;
    {
        unsigned int at = 0u;
        while (!found && (at < 2u))
        {
            found = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_named[at],
                               .value_out = args->folder_out, .capacity = CLI_PATH_MAX_LEN) &&
                    (args->folder_out[0] != '\0');
            at += 1u;
        }
    }
    if (!found)
    {
        // a folder of this reader's own, since in a folder every user writes in another user can set a name first
        char home[CLI_PATH_MAX_LEN];
        VerbaLinea line = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line,
                      EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home) ? home : ".");
        mmgr_line_put(&line, "/temp");
        if ((mmgr_line_finish(&line) == 0ULL) ||
            !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->folder_out) ||
            (chmod(args->folder_out, 0700) != 0))
        {
            args->folder_out[0] = '.';
            args->folder_out[1] = '\0';
        }
        return;
    }
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = args->folder_out, .cap = CLI_POOL_BYTES});
    end = (size_t)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                   .text = args->folder_out + 1u, .at = end - 1u, .set = "/"));
    args->folder_out[end] = '\0';
}

unsigned long long cli_event_inheritable(void)
{
    return 0ULL;
}

int cli_event_set(const CliEventCtx *args)
{
    (void)args;
    return 0;
}

void cli_event_close(const CliEventCtx *args)
{
    (void)args;
}

int cli_private_folder_init(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->folder) &&
           (chmod(args->folder, 0700) == 0);
}

int cli_private_file_write(const CliPathBytesCtx *args)
{
    const int file = open(args->path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (file < 0)
    {
        return 0;
    }
    const unsigned char *const from = (const unsigned char *)args->bytes;
    unsigned long long written = 0ULL;
    while (written < args->size)
    {
        const ssize_t wrote = write(file, from + written, (size_t)(args->size - written));
        if (wrote <= 0)
        {
            break;
        }
        written += (unsigned long long)wrote;
    }
    return (close(file) == 0) && (written == args->size);
}

#if defined(__linux__)
// the most bytes of /proc/stat and of a session's id read
#define CLI_SYSTEM_STAT_MAX 65536u

// the file at `path` read into `out`, sized for `capacity`, a 0 after what was read: the bytes read, 0 where it cannot
// be
EMBED_INLINE unsigned long long system_read(const PosixSystemCtx *args)
{
    const int file = open(args->path, O_RDONLY | O_CLOEXEC);
    unsigned long long used = 0ULL;
    while ((file >= 0) && (used + 1ULL < args->capacity))
    {
        const unsigned long long capacity = args->capacity - 1ULL - used;
        const ssize_t count = read(file, args->out + used, (size_t)capacity);
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
    args->out[used] = '\0';
    return used;
}

// the machine's start as the kernel saves it, the line `btime` of /proc/stat
unsigned long long cli_booted_seconds(void)
{
    static char s_stat[CLI_SYSTEM_STAT_MAX];
    (void)EMBED_CALL(system_read, PosixSystemCtx, .path = "/proc/stat", .out = s_stat, .capacity = sizeof(s_stat));
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = s_stat, .cap = sizeof(s_stat), .other = "\nbtime ", .other_cap = CLI_POOL_BYTES});
    unsigned long long seconds = 0ULL;
    seconds = (at != NULL) ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at + 7, .at = 0ULL,
                                        .size = cellul.len(&(CatenaFinitaCfg){.src = at + 7, .cap = CLI_POOL_BYTES}),
                                        .end = NULL)
                           : 0ULL;
    return seconds;
}

// the audit session the kernel gives each log in, or the desktop's own name for the session where the kernel saves none
void cli_logon_by_name(const CliCapacityLogonCtx *args)
{
    char session[64];
    unsigned long long length = EMBED_CALL(system_read, PosixSystemCtx, .path = "/proc/self/sessionid", .out = session,
                                           .capacity = sizeof(session));
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = session, .at = length,
                        .set = "\n ");
    session[length] = '\0';
    const int saved =
        (length > 0ULL) && !cellul.eq(&(CatenaFinitaCfg){.src = session, .other = "4294967295", .cap = CLI_POOL_BYTES});
    VerbaLinea named = {args->logon, args->capacity_bytes, 0ULL};
    if (saved)
    {
        mmgr_line_put(&named, session);
        (void)mmgr_line_finish(&named);
    }
    else if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "XDG_SESSION_ID",
                         .value_out = args->logon, .capacity = args->capacity_bytes))
    {
        (void)mmgr_line_finish(&named);
    }
}
#elif defined(__APPLE__)
// the machine's start as sysctl saves it
unsigned long long cli_booted_seconds(void)
{
    struct timeval booted;
    size_t size = sizeof(booted);
    // a start the system gives is after 1970, never below 0
    return (sysctlbyname("kern.boottime", &booted, &size, NULL, 0u) == 0) ? (unsigned long long)booted.tv_sec : 0ULL;
}

// the reader's log in is not named on macOS; the machine's start alone marks a session there
void cli_logon_by_name(const CliCapacityLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->capacity_bytes, 0ULL};
    (void)mmgr_line_finish(&named);
}
#else
unsigned long long cli_booted_seconds(void)
{
    return 0ULL;
}

void cli_logon_by_name(const CliCapacityLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->capacity_bytes, 0ULL};
    (void)mmgr_line_finish(&named);
}
#endif

#endif
