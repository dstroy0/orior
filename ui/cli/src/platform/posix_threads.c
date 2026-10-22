// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_threads.c: the command line's locks, wakes and threads on Linux and macOS: a POSIX mutex, a condition
// variable, and a thread of its own, let go; and the processors the machine has online
#if !defined(_WIN32)
// the count of processors online is the system's, outside strict C11 and POSIX alike
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
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

void cli_lock_take(CliLock *lock)
{
    pthread_mutex_lock(&lock->mutex);
}

void cli_lock_give(CliLock *lock)
{
    pthread_mutex_unlock(&lock->mutex);
}

void cli_wake_wait(CliWake *wake, CliLock *lock)
{
    pthread_cond_wait(&wake->condition, &lock->mutex);
}

int cli_wake_wait_for(CliWake *wake, CliLock *lock, unsigned long long milliseconds)
{
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    // the seconds and nanoseconds to wait, each within what a time_t and a long hold
    until.tv_sec += (time_t)(milliseconds / 1000ull);
    until.tv_nsec += (long)((milliseconds % 1000ull) * 1000000ull);
    if (until.tv_nsec >= 1000000000l)
    {
        until.tv_sec += 1;
        until.tv_nsec -= 1000000000l;
    }
    return pthread_cond_timedwait(&wake->condition, &lock->mutex, &until) == 0;
}

void cli_wake_all(CliWake *wake)
{
    pthread_cond_broadcast(&wake->condition);
}

static void *cli_thread_entry(void *given)
{
    // the thread is handed what it runs, which its starter keeps for as long as the thread lives
    const CliThread *const thread = (const CliThread *)given;
    cli_priority_own(thread->priority);
    thread->run(thread->context);
    return NULL;
}

// On Linux a thread has a nice value of its own, and an idle one the idle policy; on macOS a thread has a quality of
// service. A thread of the run class keeps the level it was started at: raising one takes a privilege orior does not
// ask for
void cli_priority_own(unsigned int priority)
{
    if ((priority != CLI_PRIORITY_GUEST) && (priority != CLI_PRIORITY_IDLE))
    {
        return;
    }
#if defined(__APPLE__)
    pthread_set_qos_class_self_np((priority == CLI_PRIORITY_IDLE) ? QOS_CLASS_BACKGROUND : QOS_CLASS_UTILITY, 0);
#elif defined(__linux__)
    // the thread's own id, which setpriority takes as a process's on Linux
    const int self = (int)syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, (id_t)self, (priority == CLI_PRIORITY_IDLE) ? 19 : 9);
    if (priority == CLI_PRIORITY_IDLE)
    {
        const struct sched_param none = {0};
        sched_setscheduler(0, SCHED_IDLE, &none);
    }
#else
    setpriority(PRIO_PROCESS, 0, (priority == CLI_PRIORITY_IDLE) ? 19 : 9);
#endif
}

int cli_thread_started(CliThread *thread)
{
    pthread_t started;
    if (pthread_create(&started, NULL, cli_thread_entry, thread) != 0)
    {
        return 0;
    }
    pthread_detach(started);
    return 1;
}

unsigned int cli_processors(void)
{
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    // a count of processors is positive and small, which an unsigned int holds
    return (count > 0l) ? (unsigned int)count : 1u;
}
#endif
