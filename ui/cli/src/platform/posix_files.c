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
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

unsigned int cli_system_error(void)
{
    // errno is not negative after a call that failed
    return (unsigned int)errno;
}

void cli_system_reason(unsigned int error, VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, ": ");
    // an error number from errno is a small int
    mmgr_line_put(err_msg, strerror((int)error));
    mmgr_line_put(err_msg, " (os error ");
    mmgr_line_decimal(err_msg, error, 1u);
    mmgr_line_ch(err_msg, ')');
}

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

// `count` bytes of the open `file` from `from` read into `bytes`, their count into `got`, fewer where the file ends
static void file_read_from(int file, unsigned long long from, unsigned char *bytes, unsigned long long count,
                           unsigned long long *got)
{
    *got = 0ull;
    int reading = 1;
    while (reading && (*got < count))
    {
        // a read's count and its place are what the system's types represent
        const ssize_t read_now = pread(file, bytes + *got, (size_t)(count - *got), (off_t)(from + *got));
        reading = read_now > 0;
        // a count read is not negative where reading goes on
        *got += reading ? (unsigned long long)read_now : 0ull;
    }
}

int cli_file_read_at(const char *path, unsigned long long from, unsigned char *bytes, unsigned long long count,
                     unsigned long long *got)
{
    const int file = open(path, O_RDONLY);
    *got = 0ull;
    if (file < 0)
    {
        return 0;
    }
    file_read_from(file, from, bytes, count, got);
    close(file);
    return 1;
}

int cli_file_read_whole(const char *path, unsigned char *(*room)(void *context, unsigned long long size), void *context,
                        unsigned long long *got)
{
    *got = 0ull;
    const int file = open(path, O_RDONLY);
    if (file < 0)
    {
        return 0;
    }
    struct stat found;
    if (fstat(file, &found) != 0)
    {
        close(file);
        return 0;
    }
    // a file's size is not negative
    const unsigned long long size = (unsigned long long)found.st_size;
    unsigned char *const bytes = room(context, size);
    if (bytes == NULL)
    {
        close(file);
        return 0;
    }
    file_read_from(file, 0ull, bytes, size, got);
    close(file);
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

int cli_resource_added(const char *path, const char *name, const void *data, unsigned long long size,
                       VerbaLinea *err_msg)
{
    (void)name;
    (void)data;
    (void)size;
    mmgr_line_put(err_msg, path);
    mmgr_line_put(err_msg, " is a Windows program, which only Windows adds a resource to");
    return 0;
}

// the bytes of randomness a temporary file's name carries
#define CLI_TEMPORARY_NAME_BYTES 8u

// the file loses its name the moment it is made, so the system deletes it once the last descriptor is closed, however
// the process ends
int cli_temporary_file_made(CliTemporaryFile *file)
{
    char path[CLI_PATH_MAX_LEN];
    cli_temp_folder(path);
    unsigned char random[CLI_TEMPORARY_NAME_BYTES];
    if (!cli_random_bytes(random, sizeof(random)))
    {
        return 0;
    }
    VerbaLinea named = {path, sizeof(path), cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL})};
    mmgr_line_put(&named, "/orior-");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    for (unsigned int at = 0u; at < CLI_TEMPORARY_NAME_BYTES; at += 1u)
    {
        mmgr_line_hex(&named, random[at], 2u);
    }
    if (mmgr_line_finish(&named) == 0ull)
    {
        return 0;
    }
    const int made = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (made < 0)
    {
        return 0;
    }
    (void)unlink(path);
    // a descriptor from open is not negative here
    file->handle = (unsigned long long)made;
    return 1;
}

int cli_temporary_file_written(const CliTemporaryFile *file, unsigned long long at, const void *bytes,
                               unsigned long long size)
{
    const unsigned char *const from = (const unsigned char *)bytes;
    unsigned long long done = 0ull;
    while (done < size)
    {
        // a descriptor stored is one open gave, and an offset in a file is below what an off_t counts
        const ssize_t wrote = pwrite((int)file->handle, from + done, (size_t)(size - done), (off_t)(at + done));
        if (wrote <= 0)
        {
            return 0;
        }
        done += (unsigned long long)wrote;
    }
    return 1;
}

int cli_temporary_file_read(const CliTemporaryFile *file, unsigned long long at, void *bytes, unsigned long long size)
{
    unsigned char *const into = (unsigned char *)bytes;
    unsigned long long done = 0ull;
    while (done < size)
    {
        // a descriptor stored is one open gave, and an offset in a file is below what an off_t counts
        const ssize_t got = pread((int)file->handle, into + done, (size_t)(size - done), (off_t)(at + done));
        if (got <= 0)
        {
            return 0;
        }
        done += (unsigned long long)got;
    }
    return 1;
}

void cli_temporary_file_let_go(CliTemporaryFile *file)
{
    if (file->handle != 0ull)
    {
        // a descriptor stored is one open gave
        (void)close((int)file->handle);
    }
    file->handle = 0ull;
}
#endif
