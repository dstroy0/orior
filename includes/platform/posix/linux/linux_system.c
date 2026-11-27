// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_system.c: what the command line requests of Linux itself: when the machine started and the reader's log
// in, each from /proc, the folders of its fonts, and its facts
#if defined(__linux__)
// O_CLOEXEC is POSIX, outside strict C11
#define _GNU_SOURCE
#endif
#include "../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../ui/cli/src/text/text.h"
#include "../posix_systems.h"

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>

typedef struct
{
    const char *path;
    char *out;
    unsigned long long capacity;
} PosixSystemCtx;

// the max bytes of /proc/stat and of a session's id read
#define CLI_SYSTEM_STAT_MAX 65536u

// the file at `path` read into `out`, sized for `capacity`, with a 0 after the bytes: the bytes read, 0 where the read
// fails
EMBED_INLINE unsigned long long system_read(const PosixSystemCtx *args)
{
    const int file = open(args->path, O_RDONLY | O_CLOEXEC);
    unsigned long long used = 0ULL;
    while ((file >= 0) && (used + 1ULL < args->capacity))
    {
        const unsigned long long capacity = args->capacity - 1ULL - used;
        const ssize_t count = read(file, args->out + used, (embed_word)capacity);
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

// the folders the system saves its fonts in, for every reader and for the reader alone
static const CliSystemFolder s_font_folders[3] = {
    {NULL, "/usr/share/fonts"}, {NULL, "/usr/local/share/fonts"}, {"HOME", "/.local/share/fonts"}};

// Linux's facts, its .NET name by the processor that the build targets
#if defined(__aarch64__)
#define SYSTEM_DOTNET_RUNTIME "linux-arm64"
#else
#define SYSTEM_DOTNET_RUNTIME "linux-x64"
#endif
const CliSystemFacts g_cli_system_facts =
    CLI_POSIX_SYSTEM_FACTS("linux", "xdg-open", 0, s_font_folders, "linux", SYSTEM_DOTNET_RUNTIME, 1);
#endif
