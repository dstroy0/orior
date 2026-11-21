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

#if !defined(_WIN32)
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
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

#endif
