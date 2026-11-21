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
#include <time.h>
#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"

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

// the count of waiters a wake reaches so that none is left asleep, the largest int
#define CLI_FUTEX_WAKE_ALL 0x7FFFFFFF

#if defined(__APPLE__)
// macOS's address wait and wake, which the C++ library's atomic waits call as well: the operation, compare and wait
// with the wake reaching every sleeper and the error returned rather than set in errno; the timeout in microseconds, 0
// for none
extern int __ulock_wait(embed_u32 operation, void *address, embed_u64 value, embed_u32 timeout_us);
extern int __ulock_wake(embed_u32 operation, void *address, embed_u64 wake_value);
#define CLI_ULOCK_COMPARE_AND_WAIT 1u
#define CLI_ULOCK_COMPARE_AND_WAIT64 5u
#define CLI_ULOCK_WAKE_ALL 0x00000100u
#define CLI_ULOCK_NO_ERRNO 0x01000000u
#endif

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    embed_atomic_word *const atomic_word = args->word;
    const embed_atomic_word_value seen = args->old_value;
    const embed_u64 nanoseconds = args->nanoseconds;
    if (nanoseconds == 0u)
    {
        return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = atomic_word) != seen;
    }
#if defined(__linux__)
    struct timespec span;
    // the seconds and nanoseconds to wait, each within what a time_t and a long represent
    span.tv_sec = (time_t)(nanoseconds / 1000000000ULL);
    span.tv_nsec = (long)(nanoseconds % 1000000000ULL);
    // the futex compares 32 bits: on this little-endian host the word's low half, where the bits a sleeper watches
    // lie; the sleeper tests the whole word again on waking
    (void)syscall(SYS_futex, (void *)&atomic_word->value, FUTEX_WAIT_PRIVATE, (unsigned int)seen,
                  (nanoseconds == EMBED_WAIT_INFINITE) ? NULL : &span, NULL, 0);
#elif defined(__APPLE__)
    // the microseconds a embed_u32 counts, rounded up, since 0 waits with no end
    const unsigned long long micro = (nanoseconds / 1000ULL) + (unsigned long long)((nanoseconds % 1000ULL) != 0ULL);
    const embed_u32 timeout =
        (nanoseconds == EMBED_WAIT_INFINITE) ? 0u : (embed_u32)((micro < 4294967295ULL) ? micro : 4294967295ULL);
    (void)__ulock_wait(CLI_ULOCK_COMPARE_AND_WAIT64 | CLI_ULOCK_NO_ERRNO, (void *)&atomic_word->value, (embed_u64)seen,
                       timeout);
#else
    // a system with no address wait sleeps the time, or a millisecond for a sleep with no end, and its sleeper tests
    // again
    const unsigned long long slept = (nanoseconds == EMBED_WAIT_INFINITE) ? 1000000ULL : nanoseconds;
    struct timespec span;
    span.tv_sec = (time_t)(slept / 1000000000ULL);
    span.tv_nsec = (long)(slept % 1000000000ULL);
    if (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = atomic_word) == seen)
    {
        (void)nanosleep(&span, NULL);
    }
#endif
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = atomic_word) != seen;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    embed_atomic_word *const atomic_word = args->word;
#if defined(__linux__)
    (void)syscall(SYS_futex, (void *)&atomic_word->value, FUTEX_WAKE_PRIVATE, CLI_FUTEX_WAKE_ALL, NULL, NULL, 0);
#elif defined(__APPLE__)
    (void)__ulock_wake(CLI_ULOCK_COMPARE_AND_WAIT64 | CLI_ULOCK_WAKE_ALL | CLI_ULOCK_NO_ERRNO,
                       (void *)&atomic_word->value, 0u);
#else
    (void)atomic_word;
#endif
}

typedef struct
{
    CliWake *wake;
    embed_atomic_word_value seen;
    unsigned long long milliseconds;
} PosixThreadsCtx;

// a sleep of `milliseconds` on the wake's word, ended early where the word moves off `seen`: 1 where it moved
static int wake_sleep(const PosixThreadsCtx *args)
{
    // the word's wait takes nanoseconds: a sleep past what they count sleeps with no end
    return EMBED_CALL(embed_word_wait, EmbedWordWaitCfg, .word = &args->wake->generation, .old_value = args->seen,
                      .nanoseconds = (args->milliseconds < (EMBED_WAIT_INFINITE / 1000000ULL))
                                         ? (embed_u64)args->milliseconds * 1000000ULL
                                         : EMBED_WAIT_INFINITE);
}

int cli_wake_wait_for(const CliWakeCtx *args)
{
    return EMBED_CALL(
        wake_sleep, PosixThreadsCtx, .wake = args->wake,
        .seen = EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &args->wake->generation),
        .milliseconds = args->milliseconds_u64);
}

int cli_wake_wait_seen_for(const CliWakeCtx *args)
{
    return EMBED_CALL(wake_sleep, PosixThreadsCtx, .wake = args->wake, .seen = args->seen,
                      .milliseconds = args->milliseconds_u64);
}

unsigned int cli_wake_read(const CliWakeCtx *args)
{
    return (unsigned int)EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg,
                                    .word = &args->wake->generation);
}

void cli_wake_wait_seen(const CliWakeCtx *args)
{
    // The word's wait compares the word with seen before it sleeps, so a wake that lands first ends it at once. No
    // timed recheck is needed, and the sleep ends only when the word moves.
    while (!EMBED_CALL(wake_sleep, PosixThreadsCtx, .wake = args->wake, .seen = args->seen,
                       .milliseconds = EMBED_WAIT_INFINITE))
    {
        // a sleep that ends with the word still at seen sleeps again
    }
}

void cli_wake_all(const CliWakeCtx *args)
{
    (void)EMBED_CALL(embed_atomic_word_table.fetch_add, EmbedAtomicWordFetchAddCfg, .word = &args->wake->generation,
                     .value = 1u);
    EMBED_CALL(embed_word_wake, EmbedWordWakeCfg, .word = &args->wake->generation);
}

static void *cli_thread_entry(void *supplied)
{
    // the thread is passed what it runs, which its starter retains for as long as the thread lives
    const CliThread *const thread = (const CliThread *)supplied;
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx, .priority = thread->priority);
    thread->run(thread->context);
    cli_thread_place_released();
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

embed_word cli_thread_id(void)
{
#if defined(__linux__)
    return (embed_word)syscall(SYS_gettid);
#else
    // a thread's handle read as the integer it is, which identifies it for as long as it lives
    return (embed_word)(embed_word)pthread_self();
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
