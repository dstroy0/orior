// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_posix.c: the platform on Linux and macOS. The pages come from mmap, a thread from pthread_create, an address
// wait from a futex on Linux and from __ulock_wait on macOS, the clock from the monotonic clock, and the streams from
// write
#if !defined(_WIN32)
// the futex call, the count of processors online and the monotonic clock are the system's, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "platform_internal.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "memoria_operor/memoria_linea.h"

#if !defined(_WIN32)
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// the environment of the process, which posix_spawnp passes to the program that it starts
extern char **environ;
#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#endif

#if defined(__APPLE__)
// the address wait and wake of macOS. The operation is compare and wait, the wake goes to every sleeper, and the call
// returns the error in place of errno. The limit is in microseconds, and 0 is no end
extern int __ulock_wait(uint32_t operation, void *address, uint64_t value, uint32_t timeout_us);
extern int __ulock_wake(uint32_t operation, void *address, uint64_t wake_value);
#define PLATFORM_ULOCK_COMPARE_AND_WAIT 1u
#define PLATFORM_ULOCK_COMPARE_AND_WAIT64 5u
#define PLATFORM_ULOCK_WAKE_ALL 0x00000100u
#define PLATFORM_ULOCK_NO_ERRNO 0x01000000u
#endif

void *platform_pages_mapped(unsigned long long bytes)
{
    void *const at = mmap(NULL, (size_t)bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (at == MAP_FAILED) ? NULL : at;
}

void platform_pages_released(void *at, unsigned long long bytes)
{
    (void)munmap(at, (size_t)bytes);
}

// what a thread of the platform runs, which the starter keeps for the life of the thread
typedef struct
{
    void (*run)(void *context);
    void *context;
} PlatformThread;

// the threads that the platform starts at once. The mover is the single thread that the platform starts
#define PLATFORM_THREADS_MAX 4u
static PlatformThread s_threads[PLATFORM_THREADS_MAX];
static embed_atomic_u32 s_threads_given = EMBED_ATOMIC_INIT(0u);

static void *platform_thread_entered(void *supplied)
{
    const PlatformThread *const thread = supplied;
    thread->run(thread->context);
    return NULL;
}

int platform_thread_started(void (*run)(void *context), void *context)
{
    const embed_u32 place = embed_atomic_u32_fetch_add(&s_threads_given, 1u);
    if (place >= PLATFORM_THREADS_MAX)
    {
        return 0;
    }
    s_threads[place] = (PlatformThread){run, context};
    pthread_t started;
    if (pthread_create(&started, NULL, platform_thread_entered, &s_threads[place]) != 0)
    {
        return 0;
    }
    (void)pthread_detach(started);
    return 1;
}

embed_word platform_thread_id(void)
{
#if defined(__linux__)
    // the id of a thread on Linux is a small positive count
    return (embed_word)syscall(SYS_gettid);
#else
    // the handle of a thread read as the integer that it is, which names the thread for its life
    return (embed_word)(uintptr_t)pthread_self();
#endif
}

void platform_thread_yield(void)
{
    (void)sched_yield();
}

unsigned int platform_processors(void)
{
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    // a count of processors is positive and small, and an unsigned int holds it
    return (count > 0L) ? (unsigned int)count : 1u;
}

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds)
{
    if (nanoseconds == 0u)
    {
        return embed_atomic_word_load(word) != old_value;
    }
#if defined(__linux__)
    struct timespec span;
    // the seconds and the nanoseconds of the limit, each in the range of its field
    span.tv_sec = (time_t)((embed_u64)nanoseconds / 1000000000ULL);
    span.tv_nsec = (long)((embed_u64)nanoseconds % 1000000000ULL);
    // The futex compares 32 bits: on this little-endian host the low half of the word, where the bits that a sleeper
    // watches lie. The sleeper tests the whole word again after the wake
    (void)syscall(SYS_futex, (void *)(uintptr_t)&word->value, FUTEX_WAIT_PRIVATE, (unsigned int)old_value,
                  (nanoseconds == EMBED_WAIT_INFINITE) ? NULL : &span, NULL, 0);
#elif defined(__APPLE__)
    // the limit in microseconds, rounded up, and 0 for no end
    const uint32_t timeout_us =
        (nanoseconds == EMBED_WAIT_INFINITE) ? 0u : (uint32_t)(((embed_u64)nanoseconds + 999ULL) / 1000ULL);
    (void)__ulock_wait(
        ((sizeof(old_value) == 8u) ? PLATFORM_ULOCK_COMPARE_AND_WAIT64 : PLATFORM_ULOCK_COMPARE_AND_WAIT) |
            PLATFORM_ULOCK_NO_ERRNO,
        (void *)(uintptr_t)&word->value, (uint64_t)old_value, timeout_us);
#else
    // a system with no address wait sleeps the limit, or one millisecond for no end, and the caller tests again
    const embed_u64 sleep_ns = (nanoseconds == EMBED_WAIT_INFINITE) ? 1000000ULL : (embed_u64)nanoseconds;
    struct timespec span;
    span.tv_sec = (time_t)(sleep_ns / 1000000000ULL);
    span.tv_nsec = (long)(sleep_ns % 1000000000ULL);
    if (embed_atomic_word_load(word) == old_value)
    {
        (void)nanosleep(&span, NULL);
    }
#endif
    return embed_atomic_word_load(word) != old_value;
}

void embed_word_wake(embed_atomic_word *word)
{
#if defined(__linux__)
    (void)syscall(SYS_futex, (void *)(uintptr_t)&word->value, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
#elif defined(__APPLE__)
    const uint32_t operation =
        (sizeof(embed_atomic_word_value) == 8u) ? PLATFORM_ULOCK_COMPARE_AND_WAIT64 : PLATFORM_ULOCK_COMPARE_AND_WAIT;
    (void)__ulock_wake(operation | PLATFORM_ULOCK_WAKE_ALL | PLATFORM_ULOCK_NO_ERRNO, (void *)(uintptr_t)&word->value,
                       0u);
#else
    (void)word;
#endif
}

embed_word embed_clock_ns(void)
{
    struct timespec clock_time;
    (void)clock_gettime(CLOCK_MONOTONIC, &clock_time);
    // the monotonic clock gives no negative field, and so both casts keep the value
    const embed_u64 clock_ns = ((embed_u64)clock_time.tv_sec * 1000000000ULL) + (embed_u64)clock_time.tv_nsec;
    // Explicit cast takes the nanoseconds modulo the word, as the contract of the clock says
    return (embed_word)clock_ns;
}

void platform_terminal_write(int errors, const char *text, unsigned long long length)
{
    unsigned long long put = 0ULL;
    while (put < length)
    {
        const ssize_t wrote = write(errors ? 2 : 1, text + put, (size_t)(length - put));
        if (wrote <= 0)
        {
            return;
        }
        // a write that put bytes answers a positive count
        put += (unsigned long long)wrote;
    }
}

unsigned long long platform_environment(const char *name, char *out, unsigned long long capacity)
{
    const char *const value = getenv(name);
    if ((value == NULL) || (capacity == 0ULL))
    {
        if (capacity != 0ULL)
        {
            out[0] = '\0';
        }
        return 0ULL;
    }
    const unsigned long long length = (unsigned long long)mmgr_text_length(value, (size_t)capacity);
    if (length >= capacity)
    {
        out[0] = '\0';
        return 0ULL;
    }
    mmgr_bytes_copy(out, value, (size_t)length + 1u);
    return length;
}

int platform_file_size(const char *path, unsigned long long *bytes)
{
    struct stat status;
    const int read = stat(path, &status) == 0;
    // a file size is never negative
    *bytes = read ? (unsigned long long)status.st_size : 0ULL;
    return read;
}

int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes)
{
    *bytes = 0ULL;
    const int file = open(path, O_RDONLY);
    if (file < 0)
    {
        return 0;
    }
    struct stat status;
    // a file size is never negative
    int ok = (fstat(file, &status) == 0) && ((unsigned long long)status.st_size <= capacity);
    const unsigned long long whole = ok ? (unsigned long long)status.st_size : 0ULL;
    // Explicit cast reads the destination as its bytes
    char *const into = (char *)out;
    while (ok && (*bytes < whole))
    {
        const ssize_t got = read(file, into + *bytes, (size_t)(whole - *bytes));
        ok = got > 0;
        // a read that took bytes answers a positive count
        *bytes += ok ? (unsigned long long)got : 0ULL;
    }
    (void)close(file);
    return ok;
}

int platform_file_written(const char *path, const void *data, unsigned long long count)
{
    const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0)
    {
        return 0;
    }
    // Explicit cast reads the source as its bytes
    const char *const from = (const char *)data;
    unsigned long long put = 0ULL;
    int ok = 1;
    while (ok && (put < count))
    {
        const ssize_t wrote = write(file, from + put, (size_t)(count - put));
        ok = wrote > 0;
        // a write that put bytes answers a positive count
        put += ok ? (unsigned long long)wrote : 0ULL;
    }
    ok = (close(file) == 0) && ok;
    return ok;
}

int platform_file_moved(const char *from, const char *to)
{
    return rename(from, to) == 0;
}

int platform_file_removed(const char *path)
{
    return unlink(path) == 0;
}

int platform_folder_made(const char *path)
{
    if (mkdir(path, 0755) == 0)
    {
        return 1;
    }
    struct stat status;
    return (stat(path, &status) == 0) && S_ISDIR(status.st_mode);
}

unsigned long long platform_process_id(void)
{
    // a process id is a positive count
    return (unsigned long long)getpid();
}

void *platform_library_opened(const char *name)
{
    return dlopen(name, RTLD_NOW | RTLD_LOCAL);
}

void *platform_library_symbol(void *library, const char *name)
{
    return dlsym(library, name);
}

void platform_library_closed(void *library)
{
    (void)dlclose(library);
}

long platform_program_run(const char *const *arguments, const char *log)
{
    posix_spawn_file_actions_t actions;
    (void)posix_spawn_file_actions_init(&actions);
    (void)posix_spawn_file_actions_addopen(&actions, 1, log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    (void)posix_spawn_file_actions_adddup2(&actions, 1, 2);
    pid_t child = 0;
    // posix_spawnp takes the arguments as plain pointers, and it reads them alone
    const int started =
        posix_spawnp(&child, arguments[0], &actions, NULL, (char *const *)(uintptr_t)arguments, environ) == 0;
    (void)posix_spawn_file_actions_destroy(&actions);
    if (!started)
    {
        return -1L;
    }
    int status = 1;
    if (waitpid(child, &status, 0) != child)
    {
        return -1L;
    }
    return WIFEXITED(status) ? (long)WEXITSTATUS(status) : -1L;
}

#endif
