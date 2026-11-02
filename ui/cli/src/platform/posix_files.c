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

void cli_system_reason(const CliErrorCtx *args)
{
    mmgr_line_put(args->err_msg, ": ");
    // an error number from errno is a small int
    mmgr_line_put(args->err_msg, strerror((int)args->error));
    mmgr_line_put(args->err_msg, " (os error ");
    mmgr_line_decimal(args->err_msg, args->error, 1u);
    mmgr_line_ch(args->err_msg, ')');
}

int cli_file_size(const CliPathSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    struct stat found;
    if (stat(args->path, &found) != 0)
    {
        return 0;
    }
    // a file's size is not negative
    *size = (unsigned long long)found.st_size;
    return 1;
}

typedef struct
{
#if (!defined(_WIN32))
    int file;
    unsigned long long from;
    unsigned char *bytes;
    unsigned long long count;
    unsigned long long *got;
#endif
} PosixFilesCtx;
// `count` bytes of the open `file` from `from` read into `bytes`, their count into `got`, fewer where the file ends
EMBED_INLINE void file_read_from(const PosixFilesCtx *args)
{
    unsigned long long *got = args->got;
    *got = 0ull;
    int read_state = 1;
    while (read_state && (*got < args->count))
    {
        // a read's count and its place are what the system's types represent
        const ssize_t read_now =
            pread(args->file, args->bytes + *got, (size_t)(args->count - *got), (off_t)(args->from + *got));
        read_state = read_now > 0;
        // a count read is not negative where reading goes on
        *got += read_state ? (unsigned long long)read_now : 0ull;
    }
}

int cli_file_read_at(const CliPathFromCtx *args)
{
    unsigned long long *got = args->got;
    const int file = open(args->path, O_RDONLY);
    *got = 0ull;
    if (file < 0)
    {
        return 0;
    }
    EMBED_CALL(file_read_from, PosixFilesCtx, .file = file, .from = args->from, .bytes = args->bytes_bytes,
               .count = args->count_u64, .got = got);
    close(file);
    return 1;
}

int cli_file_stamp(const CliPathSizeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned long long *changed = args->changed;
    struct stat found;
    if (stat(args->path, &found) != 0)
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

int cli_move(const CliErrMsgFromCtx *args)
{
    if (rename(args->from_text, args->to) == 0)
    {
        return CLI_MOVE_DONE;
    }
    return (errno == EXDEV) ? CLI_MOVE_ACROSS : CLI_MOVE_FAILED;
}

int cli_file_append(const CliPathBytesCtx *args)
{
    const int file = open(args->path, args->fresh ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_APPEND), 0644);
    if (file < 0)
    {
        return 0;
    }
    const unsigned char *at = (const unsigned char *)args->bytes;
    unsigned long long left = args->size;
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

int cli_resource_add(const CliPathErrMsgCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, " is a Windows program, which only Windows adds a resource to");
    return 0;
}

// the bytes of randomness a temporary file's name carries
#define CLI_TEMPORARY_NAME_BYTES 8u

// the file loses its name the moment it is made, so the system deletes it once the last descriptor is closed, however
// the process ends
int cli_temporary_file_init(const CliFileTemporaryFileCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = path);
    unsigned char random[CLI_TEMPORARY_NAME_BYTES];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = random, .size = sizeof(random)))
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
    args->file_temporary_file_pointer->handle = (unsigned long long)made;
    return 1;
}

int cli_temporary_file_write(const CliBytesCtx *args)
{
    const unsigned char *const from = (const unsigned char *)args->bytes;
    unsigned long long done = 0ull;
    while (done < args->size)
    {
        // a descriptor stored is one open gave, and an offset in a file is below what an off_t counts
        const ssize_t wrote = pwrite((int)args->file_temporary_file_pointer_other->handle, from + done,
                                     (size_t)(args->size - done), (off_t)(args->at + done));
        if (wrote <= 0)
        {
            return 0;
        }
        done += (unsigned long long)wrote;
    }
    return 1;
}

int cli_temporary_file_read(const CliSizeCtx *args)
{
    unsigned char *const into = (unsigned char *)args->bytes_out;
    unsigned long long done = 0ull;
    while (done < args->size)
    {
        // a descriptor stored is one open gave, and an offset in a file is below what an off_t counts
        const ssize_t got = pread((int)args->file_temporary_file_pointer_other->handle, into + done,
                                  (size_t)(args->size - done), (off_t)(args->at + done));
        if (got <= 0)
        {
            return 0;
        }
        done += (unsigned long long)got;
    }
    return 1;
}

void cli_temporary_file_let_go(const CliFileTemporaryFileCtx *args)
{
    if (args->file_temporary_file_pointer->handle != 0ull)
    {
        // a descriptor stored is one open gave
        (void)close((int)args->file_temporary_file_pointer->handle);
    }
    args->file_temporary_file_pointer->handle = 0ull;
}
#endif
