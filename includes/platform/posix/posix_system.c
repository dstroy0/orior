// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_system.c: what the command line requests of the POSIX systems: the civil clock, their randomness from
// /dev/urandom, the folder TMPDIR or XDG_RUNTIME_DIR names, else one of the reader's own, and the tables that their
// facts share; an event a started process inherits is Windows' alone. When the machine started, the reader's log in,
// the folders of the fonts and the facts are the system's (linux/linux_system.c, macos/macos_system.c)
#if !defined(_WIN32)
// O_CLOEXEC is POSIX and MAP_ANONYMOUS is the C library's own, both outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../ui/cli/src/text/text.h"
#include "posix_systems.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void cli_clock_civil(const CliClockCivilCtx *args)
{
    CliCivil *const civil = args->civil_out;
    *civil = (CliCivil){1970u, 1u, 1u, 0u, 0u};
    // a reading before 1970 counts as 1970; the system's time counts the seconds from 1970 as the reading does
    const time_t since = (time_t)((args->seconds > 0LL) ? args->seconds : 0LL);
    struct tm local;
    if (localtime_r(&since, &local) == NULL)
    {
        return;
    }
    // the calendar's fields are not negative: the year counts from 1900 and the month from 0
    *civil = (CliCivil){(unsigned int)(local.tm_year + 1900), (unsigned int)(local.tm_mon + 1),
                        (unsigned int)local.tm_mday, (unsigned int)local.tm_hour, (unsigned int)local.tm_min};
}

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
        const ssize_t read_now = read(source, into + supplied, (embed_word)(args->size - supplied));
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
    embed_word end = cellul.len(&(CatenaFinitaCfg){.src = args->folder_out, .cap = CLI_POOL_BYTES});
    end = (embed_word)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
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
        const ssize_t wrote = write(file, from + written, (embed_word)(args->size - written));
        if (wrote <= 0)
        {
            break;
        }
        written += (unsigned long long)wrote;
    }
    return (close(file) == 0) && (written == args->size);
}

// the ending that a name on the PATH answers to, none
const char *const g_cli_posix_program_ends[1] = {""};

// the folders that hold the reader's settings, XDG's first
const CliSystemFolder g_cli_posix_config_folders[2] = {{"XDG_CONFIG_HOME", ""}, {"HOME", "/.config"}};
#endif
