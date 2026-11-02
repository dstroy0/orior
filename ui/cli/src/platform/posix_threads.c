// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_threads.c: the command line's wakes and threads on Linux and macOS: a wake is a futex on a generation word on
// Linux and a timed sleep elsewhere, a thread is started and let go, and the processors the machine has are read
#if !defined(_WIN32)
// the count of processors online is the system's, outside strict C11 and POSIX alike
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include <limits.h>
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
#define CLI_WAKE_RECHECK_MS 10ull

// a sleep of `milliseconds` on the wake's word, ended early where the word moves off `seen`: 1 where it moved
static int wake_sleep(CliWake *wake, unsigned int seen, unsigned long long milliseconds)
{
    struct timespec span;
    // the seconds and nanoseconds to wait, each within what a time_t and a long represent
    span.tv_sec = (time_t)(milliseconds / 1000ull);
    span.tv_nsec = (long)((milliseconds % 1000ull) * 1000000ull);
#if defined(__linux__)
    (void)syscall(SYS_futex, &wake->generation, FUTEX_WAIT_PRIVATE, seen, &span, NULL, 0);
#else
    (void)nanosleep(&span, NULL);
#endif
    return atomic_load_explicit(&wake->generation, memory_order_acquire) != seen;
}

void cli_wake_wait(const CliWakeCtx *args)
{
    (void)wake_sleep(args->wake, atomic_load_explicit(&args->wake->generation, memory_order_acquire),
                     CLI_WAKE_RECHECK_MS);
}

int cli_wake_wait_for(const CliWakeCtx *args)
{
    return wake_sleep(args->wake, atomic_load_explicit(&args->wake->generation, memory_order_acquire),
                      args->milliseconds_u64);
}

unsigned int cli_wake_read(const CliWakeCtx *args)
{
    return atomic_load_explicit(&args->wake->generation, memory_order_acquire);
}

void cli_wake_wait_seen(const CliWakeCtx *args)
{
    while (!wake_sleep(args->wake, args->seen, CLI_WAKE_RECHECK_MS))
    {
    }
}

void cli_wake_all(const CliWakeCtx *args)
{
    (void)atomic_fetch_add_explicit(&args->wake->generation, 1u, memory_order_release);
#if defined(__linux__)
    (void)syscall(SYS_futex, &args->wake->generation, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
#endif
}

static void *cli_thread_entry(void *given)
{
    // the thread is handed what it runs, which its starter retains for as long as the thread lives
    const CliThread *const thread = (const CliThread *)given;
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
    return (count > 0l) ? (unsigned int)count : 1u;
}
#endif
