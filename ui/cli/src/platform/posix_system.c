// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_system.c: what the command line asks of Linux and macOS themselves: their randomness from /dev/urandom, and
// the folder TMPDIR names, else /tmp; an event a started process inherits is Windows' alone
#if !defined(_WIN32)
// O_CLOEXEC is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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
    if (!query_environment_read("TMPDIR", folder, CLI_PATH_LONGEST) || (folder[0] == '\0'))
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

#endif
