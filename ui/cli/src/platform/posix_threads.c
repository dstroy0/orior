// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_threads.c: the command line's wakes, address waits and threads on Linux and macOS: a thread asleep on a word's
// address until the word moves, a futex on Linux and __ulock_wait on macOS, and a timed sleep on a system with neither;
// a thread is started and let go, and the processors the machine has are read
#if !defined(_WIN32)
// the count of processors online is the system's, outside strict C11 and POSIX alike
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include <limits.h>
#include <stdint.h>
#include <time.h>
#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

// the longest an untimed wait sleeps before its waiter tests again, which bounds a wake landing between the test and
// the sleep
#define CLI_WAKE_RECHECK_MS 10ULL

#if defined(__APPLE__)
// macOS's address wait and wake, which the C++ library's atomic waits call as well: the operation, compare and wait
// with the wake reaching every sleeper and the error returned rather than set in errno; the timeout in microseconds, 0
// for none
extern int __ulock_wait(uint32_t operation, void *address, uint64_t value, uint32_t timeout_us);
extern int __ulock_wake(uint32_t operation, void *address, uint64_t wake_value);
#define CLI_ULOCK_COMPARE_AND_WAIT 1u
#define CLI_ULOCK_COMPARE_AND_WAIT64 5u
#define CLI_ULOCK_WAKE_ALL 0x00000100u
#define CLI_ULOCK_NO_ERRNO 0x01000000u
#endif

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u32 milliseconds)
{
#if defined(__linux__)
    struct timespec span;
    // the seconds and nanoseconds to wait, each within what a time_t and a long represent
    span.tv_sec = (time_t)(milliseconds / 1000ULL);
    span.tv_nsec = (long)((milliseconds % 1000ULL) * 1000000ULL);
    // the futex compares 32 bits: on this little-endian host the word's low half, where the bits a sleeper watches
    // lie; the sleeper tests the whole word again on waking
    (void)syscall(SYS_futex, (void *)&word->value, FUTEX_WAIT_PRIVATE, (unsigned int)seen,
                  (milliseconds == EMBED_WAIT_FOREVER) ? NULL : &span, NULL, 0);
#elif defined(__APPLE__)
    // the microseconds a uint32_t counts, at least 1, since 0 waits with no end
    const unsigned long long micro = (milliseconds < 4294967ULL) ? (milliseconds * 1000ULL) : 4294967295ULL;
    const uint32_t timeout = (milliseconds == EMBED_WAIT_FOREVER) ? 0u : (uint32_t)((micro != 0ULL) ? micro : 1ULL);
    (void)__ulock_wait(CLI_ULOCK_COMPARE_AND_WAIT64 | CLI_ULOCK_NO_ERRNO, (void *)&word->value, (uint64_t)seen,
                       timeout);
#else
    // a system with no address wait sleeps the time, or a millisecond for a sleep with no end, and its sleeper tests
    // again
    const unsigned long long slept = (milliseconds == EMBED_WAIT_FOREVER) ? 1ULL : milliseconds;
    struct timespec span;
    span.tv_sec = (time_t)(slept / 1000ULL);
    span.tv_nsec = (long)((slept % 1000ULL) * 1000000ULL);
    if (embed_atomic_word_load(word) == seen)
    {
        (void)nanosleep(&span, NULL);
    }
#endif
    return embed_atomic_word_load(word) != seen;
}

void embed_word_wake(embed_atomic_word *word)
{
#if defined(__linux__)
    (void)syscall(SYS_futex, (void *)&word->value, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
#elif defined(__APPLE__)
    (void)__ulock_wake(CLI_ULOCK_COMPARE_AND_WAIT64 | CLI_ULOCK_WAKE_ALL | CLI_ULOCK_NO_ERRNO, (void *)&word->value,
                       0u);
#else
    (void)word;
#endif
}

// a sleep of `milliseconds` on the wake's word, ended early where the word moves off `seen`: 1 where it moved
static int wake_sleep(CliWake *wake, embed_atomic_word_value seen, unsigned long long milliseconds)
{
    // a sleep past what the word's wait counts sleeps with no end
    return embed_word_wait(&wake->generation, seen,
                           (milliseconds < EMBED_WAIT_FOREVER) ? (embed_u32)milliseconds : EMBED_WAIT_FOREVER);
}

void cli_wake_wait(const CliWakeCtx *args)
{
    (void)wake_sleep(args->wake, embed_atomic_word_load(&args->wake->generation), CLI_WAKE_RECHECK_MS);
}

int cli_wake_wait_for(const CliWakeCtx *args)
{
    return wake_sleep(args->wake, embed_atomic_word_load(&args->wake->generation), args->milliseconds_u64);
}

int cli_wake_wait_seen_for(const CliWakeCtx *args)
{
    return wake_sleep(args->wake, args->seen, args->milliseconds_u64);
}

unsigned int cli_wake_read(const CliWakeCtx *args)
{
    return (unsigned int)embed_atomic_word_load(&args->wake->generation);
}

void cli_wake_wait_seen(const CliWakeCtx *args)
{
    while (!wake_sleep(args->wake, args->seen, CLI_WAKE_RECHECK_MS))
    {
        // each pass sleeps on the wake and reads it again
    }
}

void cli_wake_all(const CliWakeCtx *args)
{
    (void)embed_atomic_word_fetch_add(&args->wake->generation, 1u);
    embed_word_wake(&args->wake->generation);
}

static void *cli_thread_entry(void *supplied)
{
    // the thread is handed what it runs, which its starter retains for as long as the thread lives
    const CliThread *const thread = (const CliThread *)supplied;
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx, .priority = thread->priority);
    thread->run(thread->context);
    return NULL;
}

// On Linux a thread has a nice value of its own, and an idle one the idle policy; on macOS a thread has a quality of
// service. A thread of the run class retains the level it was started at: raising one takes a privilege orior does not
// request
void cli_priority_own(const CliWordsErrMsgCtx *args)
{
    if ((args->priority != CLI_PRIORITY_GUEST) && (args->priority != CLI_PRIORITY_IDLE))
    {
        return;
    }
#if defined(__APPLE__)
    pthread_set_qos_class_self_np((args->priority == CLI_PRIORITY_IDLE) ? QOS_CLASS_BACKGROUND : QOS_CLASS_UTILITY, 0);
#elif defined(__linux__)
    // the thread's own id, which setpriority takes as a process's on Linux
    const int self = (int)syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, (id_t)self, (args->priority == CLI_PRIORITY_IDLE) ? 19 : 9);
    if (args->priority == CLI_PRIORITY_IDLE)
    {
        const struct sched_param none = {0};
        sched_setscheduler(0, SCHED_IDLE, &none);
    }
#else
    setpriority(PRIO_PROCESS, 0, (args->priority == CLI_PRIORITY_IDLE) ? 19 : 9);
#endif
}

size_t cli_thread_id(void)
{
#if defined(__linux__)
    return (size_t)syscall(SYS_gettid);
#else
    // a thread's handle read as the integer it is, which identifies it for as long as it lives
    return (size_t)(uintptr_t)pthread_self();
#endif
}

void cli_thread_yield(void)
{
    (void)sched_yield();
}

int cli_thread_start(const CliThreadCtx *args)
{
    pthread_t started;
    if (pthread_create(&started, NULL, cli_thread_entry, args->thread) != 0)
    {
        return 0;
    }
    pthread_detach(started);
    return 1;
}

unsigned int cli_processors(void)
{
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    // a count of processors is positive and small, which an unsigned int stores
    return (count > 0L) ? (unsigned int)count : 1u;
}
#endif
