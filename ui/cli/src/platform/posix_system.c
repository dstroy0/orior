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
#include "../text/text.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

typedef struct
{
#if (!defined(_WIN32)) && (defined(__linux__))
    const char *path;
    char *out;
    unsigned long long room;
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
    unsigned long long given = 0ull;
    while (given < args->size)
    {
        const ssize_t read_now = read(source, into + given, (size_t)(args->size - given));
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

void cli_temp_folder(const CliFolderCtx *args)
{
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "TMPDIR",
                    .value_out = args->folder_out, .room = CLI_PATH_MAX_LEN) ||
        (args->folder_out[0] == '\0'))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->folder_out,
                                .src = "/tmp",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "/tmp", .cap = CLI_CELLS_POOL}) + 1u});
        return;
    }
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = args->folder_out, .cap = CLI_CELLS_POOL});
    end = (size_t)(1u + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg,
                                   .text = args->folder_out + 1u, .at = end - 1u, .set = "/"));
    args->folder_out[end] = '\0';
}

unsigned long long cli_event_inheritable(void)
{
    return 0ull;
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
    unsigned long long written = 0ull;
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
#define CLI_SYSTEM_STAT_MOST 65536u

// the file at `path` read into `out`, sized for `room`, a 0 after what was read: the bytes read, 0 where it cannot be
EMBED_INLINE unsigned long long system_read(const PosixSystemCtx *args)
{
    const int file = open(args->path, O_RDONLY | O_CLOEXEC);
    unsigned long long used = 0ull;
    while ((file >= 0) && (used + 1ull < args->room))
    {
        const ssize_t count = read(file, args->out + used, (size_t)(args->room - 1ull - used));
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
    args->out[used] = '\0';
    return used;
}

// the machine's start as the kernel saves it, the line `btime` of /proc/stat
unsigned long long cli_booted_seconds(void)
{
    static char s_stat[CLI_SYSTEM_STAT_MOST];
    static CliLock s_stat_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_stat_lock);
    (void)EMBED_CALL(system_read, PosixSystemCtx, .path = "/proc/stat", .out = s_stat, .room = sizeof(s_stat));
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = s_stat, .cap = sizeof(s_stat), .other = "\nbtime ", .other_cap = CLI_CELLS_POOL});
    unsigned long long seconds = 0ull;
    seconds = (at != NULL) ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at + 7, .at = 0ull,
                                        .size = cellul.len(&(CatenaFinitaCfg){.src = at + 7, .cap = CLI_CELLS_POOL}),
                                        .end = NULL)
                           : 0ull;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_stat_lock);
    return seconds;
}

// the audit session the kernel gives each log in, or the desktop's own name for the session where the kernel saves none
void cli_logon_by_name(const CliRoomLogonCtx *args)
{
    char session[64];
    unsigned long long length = EMBED_CALL(system_read, PosixSystemCtx, .path = "/proc/self/sessionid", .out = session,
                                           .room = sizeof(session));
    length = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = session, .at = length,
                        .set = "\n ");
    session[length] = '\0';
    const int saved =
        (length > 0ull) && !cellul.eq(&(CatenaFinitaCfg){.src = session, .other = "4294967295", .cap = CLI_CELLS_POOL});
    VerbaLinea named = {args->logon, args->room_size, 0ull};
    if (saved)
    {
        mmgr_line_put(&named, session);
        (void)mmgr_line_finish(&named);
    }
    else if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "XDG_SESSION_ID",
                         .value_out = args->logon, .room = args->room_size))
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
    return (sysctlbyname("kern.boottime", &booted, &size, NULL, 0u) == 0) ? (unsigned long long)booted.tv_sec : 0ull;
}

// the reader's log in is not named on macOS; the machine's start alone marks a session there
void cli_logon_by_name(const CliRoomLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->room_size, 0ull};
    (void)mmgr_line_finish(&named);
}
#else
unsigned long long cli_booted_seconds(void)
{
    return 0ull;
}

void cli_logon_by_name(const CliRoomLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->room_size, 0ull};
    (void)mmgr_line_finish(&named);
}
#endif

#endif
