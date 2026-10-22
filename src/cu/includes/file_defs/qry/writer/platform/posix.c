// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix.c: the run on POSIX: the shared memory a named object whose pages the system gives as they are touched, the
// .qry a file descriptor, the one clock the monotonic clock, and the pause a sleep
#if !defined(_WIN32)
// shared memory, the monotonic clock and nanosleep are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../qry_writer_internal.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// the object's name, `/qry-` and the hash of the .qry's path in hex
static void qry_shared_named(const char *path, char *name)
{
    static const char s_prefix[] = "/qry-";
    static const char s_digits[] = "0123456789abcdef";
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)*at) * 0x100000001b3ull;
    }
    memcpy(name, s_prefix, sizeof(s_prefix) - 1u);
    for (unsigned int digit = 0u; digit < 16u; digit += 1u)
    {
        name[sizeof(s_prefix) - 1u + digit] = s_digits[(hash >> (60u - (4u * digit))) & 0xfull];
    }
    name[sizeof(s_prefix) - 1u + 16u] = '\0';
}

static char s_name[32];
static unsigned long long s_bytes;

QryShared *qry_writer_shared_made(const char *path, unsigned long long bytes)
{
    qry_shared_named(path, s_name);
    const int memory = shm_open(s_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    // the region is sized whole, its bytes what the system holds
    if ((memory < 0) || (ftruncate(memory, (off_t)bytes) != 0))
    {
        return NULL;
    }
    void *const view = mmap(NULL, (size_t)bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0);
    close(memory);
    s_bytes = bytes;
    return (view == MAP_FAILED) ? NULL : (QryShared *)view;
}

void qry_writer_shared_released(QryShared *shared)
{
    munmap(shared, (size_t)s_bytes);
    shm_unlink(s_name);
}

long long qry_writer_file_made(const char *path)
{
    return (long long)open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
}

int qry_writer_file_put(long long file, const unsigned char *bytes, unsigned long long size)
{
    unsigned long long done = 0ull;
    while (done < size)
    {
        // a write is at most QRY_WRITER_PIECE_BYTES, at most a gibibyte, which a size_t counts
        const unsigned long long piece =
            ((size - done) < QRY_WRITER_PIECE_BYTES) ? (size - done) : QRY_WRITER_PIECE_BYTES;
        const ssize_t wrote = write((int)file, bytes + done, (size_t)piece);
        if ((wrote < 0) && (errno == EINTR))
        {
            continue;
        }
        if (wrote <= 0)
        {
            return 0;
        }
        done += (unsigned long long)wrote;
    }
    return 1;
}

int qry_writer_file_durable(long long file)
{
    return fsync((int)file) == 0;
}

int qry_writer_file_closed(long long file)
{
    const int flushed = qry_writer_file_durable(file);
    return (close((int)file) == 0) && flushed;
}

// the folder `path` is in made sure of on the disk; a rename into it therefore stands once the call returns
static void qry_writer_folder_synced(const char *path)
{
    char folder[QRY_WRITER_TOP_LONGEST + QRY_NAME_LONGEST + 2u];
    const size_t length = strlen(path);
    const char *const cut = strrchr(path, '/');
    if ((cut == NULL) || (length >= sizeof(folder)))
    {
        return;
    }
    // the cut stands inside the path; its place is therefore not negative and below its length
    const size_t kept = (cut == path) ? 1u : (size_t)(cut - path);
    memcpy(folder, path, kept);
    folder[kept] = '\0';
    const int opened = open(folder, O_RDONLY | O_CLOEXEC);
    if (opened >= 0)
    {
        (void)fsync(opened);
        (void)close(opened);
    }
}

int qry_writer_file_removed(const char *path)
{
    return unlink(path) == 0;
}

int qry_writer_file_placed(const char *beside, const char *placed)
{
    if (rename(beside, placed) == 0)
    {
        qry_writer_folder_synced(placed);
        return 1;
    }
    unlink(beside);
    return 0;
}

void qry_writer_paused(void)
{
    const struct timespec pause = {0, 1000000};
    nanosleep(&pause, NULL);
}

// Raising a process takes a privilege the writer does not ask for: it keeps the level it was started at
void qry_writer_raised(void)
{
}

unsigned long long qry_member_file_read(const char *path, unsigned char *bytes, unsigned long long room)
{
    const int file = open(path, O_RDONLY);
    struct stat held;
    if ((file < 0) || (fstat(file, &held) != 0) || ((unsigned long long)held.st_size > room))
    {
        if (file >= 0)
        {
            close(file);
        }
        return QRY_MEMBER_UNREAD;
    }
    // a file's length is not negative and fits `room`
    const unsigned long long size = (unsigned long long)held.st_size;
    unsigned long long done = 0ull;
    while (done < size)
    {
        const ssize_t got = read(file, bytes + done, (size_t)(size - done));
        if (got <= 0)
        {
            close(file);
            return QRY_MEMBER_UNREAD;
        }
        done += (unsigned long long)got;
    }
    close(file);
    return size;
}

QryShared *qry_member_shared(const char *path)
{
    char name[32];
    qry_shared_named(path, name);
    const int memory = shm_open(name, O_RDWR, 0600);
    if (memory < 0)
    {
        return NULL;
    }
    // the writer sizes the object after it names it; one not yet sized, or whose whole the writer has not written yet,
    // is no run yet
    struct stat held;
    if ((fstat(memory, &held) != 0) || ((unsigned long long)held.st_size < sizeof(QryShared)))
    {
        close(memory);
        return NULL;
    }
    // the region's bytes a size_t counts
    void *const view = mmap(NULL, (size_t)held.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0);
    close(memory);
    if (view == MAP_FAILED)
    {
        return NULL;
    }
    QryShared *const shared = (QryShared *)view;
    if (atomic_load(&shared->whole) != (unsigned long long)held.st_size)
    {
        munmap(view, (size_t)held.st_size);
        return NULL;
    }
    return shared;
}

int qry_writer_stream_put(const char *qry, const char *name, unsigned int name_length, const unsigned char *bytes,
                          unsigned long long size)
{
    // POSIX gives a file no named streams
    (void)qry;
    (void)name;
    (void)name_length;
    (void)bytes;
    (void)size;
    return 0;
}

void qry_member_unshared(QryShared *run)
{
    // the region's bytes, which its head holds, a size_t counts
    munmap(run, (size_t)atomic_load(&run->whole));
}

unsigned long long qry_writer_input_read(unsigned char *bytes, unsigned long long room)
{
    unsigned long long size = 0ull;
    for (;;)
    {
        // what is left of the room, which a size_t counts as it is in memory
        const ssize_t got = read(0, bytes + size, (size_t)(room - size));
        if (got < 0)
        {
            return QRY_MEMBER_UNREAD;
        }
        if (got == 0)
        {
            return size;
        }
        size += (unsigned long long)got;
        if (size == room)
        {
            return QRY_MEMBER_UNREAD;
        }
    }
}

unsigned long long qry_member_clock(void)
{
    struct timespec moment;
    clock_gettime(CLOCK_MONOTONIC, &moment);
    return ((unsigned long long)moment.tv_sec * 1000000000ull) + (unsigned long long)moment.tv_nsec;
}

unsigned long long qry_member_process(void)
{
    return (unsigned long long)getpid();
}

const unsigned char *qry_member_file_mapped(const char *path, unsigned long long *size)
{
    static const unsigned char s_none[1] = {0u};
    const int file = open(path, O_RDONLY);
    struct stat held;
    if ((file < 0) || (fstat(file, &held) != 0))
    {
        if (file >= 0)
        {
            close(file);
        }
        return NULL;
    }
    // a file's size is not negative
    *size = (unsigned long long)held.st_size;
    if (*size == 0ull)
    {
        close(file);
        return s_none;
    }
    // a file mapped whole is in memory, whose bytes a size_t counts
    void *const view = mmap(NULL, (size_t)*size, PROT_READ, MAP_PRIVATE, file, 0);
    close(file);
    return (view == MAP_FAILED) ? NULL : (const unsigned char *)view;
}

void qry_member_file_unmapped(const unsigned char *bytes, unsigned long long size)
{
    if ((bytes != NULL) && (size != 0ull))
    {
        // the bytes were mapped whole, their size a size_t counts
        munmap((void *)bytes, (size_t)size);
    }
}

extern char **environ;

int qry_member_named(char *path, size_t room)
{
    for (char **each = environ; (each != NULL) && (*each != NULL); each += 1)
    {
        const size_t named = sizeof(QRY_ENVIRONMENT) - 1u;
        if ((strncmp(*each, QRY_ENVIRONMENT, named) == 0) && ((*each)[named] == '='))
        {
            const size_t length = strlen(*each + named + 1u);
            if ((length == 0u) || (length >= room))
            {
                return 0;
            }
            memcpy(path, *each + named + 1u, length + 1u);
            return 1;
        }
    }
    return 0;
}

int qry_member_program(char *path, size_t room)
{
    const ssize_t length = readlink("/proc/self/exe", path, room);
    if ((length <= 0) || ((size_t)length >= room))
    {
        return 0;
    }
    // a length readlink gives is the count it read, below the room it was given
    path[(size_t)length] = '\0';
    return 1;
}
#endif
