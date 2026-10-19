// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_files.c: the command line's file sizes, moves and writes, and the wall clock on Linux and macOS
#if !defined(_WIN32)
// clock_gettime is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int cli_file_size(const char *path, unsigned long long *size)
{
    struct stat found;
    if (stat(path, &found) != 0)
    {
        return 0;
    }
    // a file's size is not negative
    *size = (unsigned long long)found.st_size;
    return 1;
}

int cli_file_stamp(const char *path, unsigned long long *size, unsigned long long *changed)
{
    struct stat found;
    if (stat(path, &found) != 0)
    {
        return 0;
    }
    // a file's size and its change since 1970 are not negative here
    *size = (unsigned long long)found.st_size;
#if defined(__APPLE__)
    *changed = ((unsigned long long)found.st_mtimespec.tv_sec * 1000ull) +
               ((unsigned long long)found.st_mtimespec.tv_nsec / 1000000ull);
#else
    *changed =
        ((unsigned long long)found.st_mtim.tv_sec * 1000ull) + ((unsigned long long)found.st_mtim.tv_nsec / 1000000ull);
#endif
    return 1;
}

unsigned long long cli_wall_milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    // the time since 1970 and its nanoseconds are not negative here
    return ((unsigned long long)now.tv_sec * 1000ull) + ((unsigned long long)now.tv_nsec / 1000000ull);
}

int cli_moved(const char *from, const char *to)
{
    if (rename(from, to) == 0)
    {
        return CLI_MOVE_DONE;
    }
    return (errno == EXDEV) ? CLI_MOVE_ACROSS : CLI_MOVE_FAILED;
}

int cli_file_appended(const char *path, const void *bytes, unsigned long long size, int fresh)
{
    const int file = open(path, fresh ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_APPEND), 0644);
    if (file < 0)
    {
        return 0;
    }
    const unsigned char *at = (const unsigned char *)bytes;
    unsigned long long left = size;
    int written = 1;
    while (written && (left > 0ull))
    {
        // a piece is at most a gibibyte, which a size_t counts
        const ssize_t done = write(file, at, (size_t)((left < (1ull << 30u)) ? left : (1ull << 30u)));
        written = done > 0;
        // a count written is positive here
        const unsigned long long wrote = written ? (unsigned long long)done : 0ull;
        at += wrote;
        left -= wrote;
    }
    return (close(file) == 0) && written;
}

int cli_resource_added(const char *path, const char *name, const void *data, unsigned long long size, CliLine *said)
{
    (void)name;
    (void)data;
    (void)size;
    cli_line_text(said, path);
    cli_line_text(said, " is a Windows program, which only Windows adds a resource to");
    return 0;
}
#endif
