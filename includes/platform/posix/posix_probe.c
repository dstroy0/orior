// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_probe.c: the probe on POSIX: the rule that a signal names, the launch queue that every process on the host
// shares, and the lock that one start of a probe holds. The start itself is the platform's (platform.h)
#if !defined(_WIN32)
// the named semaphores and the threads are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../src/cu/transpiler/lstar/interface/interface_internal.h"
#include "../platform.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

// the rule that a signal names
static InterfaceFault interface_fault_of(unsigned long long signal_number)
{
    switch (signal_number)
    {
    case SIGFPE:
        return INTERFACE_FAULT_ARITHMETIC;
    case SIGSEGV:
    case SIGBUS:
        return INTERFACE_FAULT_ADDRESS;
    case SIGILL:
        return INTERFACE_FAULT_INSTRUCTION;
    case SIGTRAP:
        return INTERFACE_FAULT_TRAP;
    case SIGABRT:
        return INTERFACE_FAULT_ABORT;
    default:
        return INTERFACE_FAULT_OTHER;
    }
}

void interface_ending_read(const PlatformEnding *ending, InterfaceAnswer *answer)
{
    answer->code = ending->code;
    answer->ending = ending->signaled ? INTERFACE_ENDING_SIGNALED : INTERFACE_ENDING_EXITED;
    answer->fault = ending->signaled ? interface_fault_of(ending->code) : INTERFACE_FAULT_NONE;
}

// The lock of one start of a probe (interface_internal.h). The macOS system sets a pipe that it makes to close on
// exec only after it makes it. A probe forked from another thread between the two takes the pipe into its program.
// That pipe stays open past the end of this probe
static pthread_mutex_t s_interface_starting = PTHREAD_MUTEX_INITIALIZER;

void interface_starting_held(void)
{
    pthread_mutex_lock(&s_interface_starting);
}

void interface_starting_let_go(void)
{
    pthread_mutex_unlock(&s_interface_starting);
}

// The launch queue (interface.h): two semaphores that every process on the host opens by name. One counts the places
// for the children that run at once. The other is the start under way, which its taker holds for the spacing after the
// start. A process that the system gives no named semaphore queues nothing
static sem_t *s_launches = SEM_FAILED;
static sem_t *s_launching = SEM_FAILED;
static pthread_once_t s_queue_opened = PTHREAD_ONCE_INIT;

static void interface_queue_open(void)
{
    s_launches = sem_open("/orior_launches", O_CREAT, 0600, INTERFACE_LAUNCHES_AT_ONCE);
    s_launching = sem_open("/orior_launching", O_CREAT, 0600, 1u);
}

void interface_launch_taken(void)
{
    pthread_once(&s_queue_opened, interface_queue_open);
    if (s_launches != SEM_FAILED)
    {
        while ((sem_wait(s_launches) != 0) && (errno == EINTR))
        {
        }
    }
    if (s_launching != SEM_FAILED)
    {
        while ((sem_wait(s_launching) != 0) && (errno == EINTR))
        {
        }
    }
}

void interface_launch_started(void)
{
    platform_paused(INTERFACE_LAUNCH_SPACING_MILLISECONDS);
    if (s_launching != SEM_FAILED)
    {
        sem_post(s_launching);
    }
}

void interface_launch_given(void)
{
    if (s_launches != SEM_FAILED)
    {
        sem_post(s_launches);
    }
}
#endif
