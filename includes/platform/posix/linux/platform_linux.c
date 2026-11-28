// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_linux.c: the platform's calls that Linux alone has (../platform_posix.h). The pages give up their bytes at
// once, an address wait is a futex, a thread has an id and a nice value of its own, and an idle thread takes the idle
// policy
#if defined(__linux__)
// the futex call, the thread's id and the idle policy are the system's, outside strict C11
#define _GNU_SOURCE
#endif
#include "../platform_posix.h"

#if defined(__linux__)
#include <limits.h>
#include <linux/futex.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

const int g_platform_pages_discard = MADV_DONTNEED;

unsigned long long platform_stat_changed_milliseconds(const struct stat *found)
{
    // a file's change since 1970 is not negative here
    return ((unsigned long long)found->st_mtim.tv_sec * 1000ULL) +
           ((unsigned long long)found->st_mtim.tv_nsec / 1000000ULL);
}

int platform_socket_peer(int fd, unsigned int *peer)
{
    struct ucred credentials;
    socklen_t length = sizeof(credentials);
    const int read_well = (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0);
    // a process number that the system reports is positive, which an unsigned int holds
    *peer = read_well ? (unsigned int)credentials.pid : 0u;
    return read_well;
}

void platform_word_slept(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds)
{
    struct timespec span;
    // the seconds and the nanoseconds of the limit, each in the range of its field
    span.tv_sec = (time_t)((embed_u64)nanoseconds / 1000000000ULL);
    span.tv_nsec = (long)((embed_u64)nanoseconds % 1000000000ULL);
    // The futex compares 32 bits: on this little-endian host the low half of the word, where the bits that a sleeper
    // watches lie. The sleeper tests the whole word again after the wake
    (void)syscall(SYS_futex, (void *)(uintptr_t)&word->value, FUTEX_WAIT_PRIVATE, (unsigned int)old_value,
                  (nanoseconds == EMBED_WAIT_INFINITE) ? NULL : &span, NULL, 0);
}

void platform_word_woken(embed_atomic_word *word)
{
    (void)syscall(SYS_futex, (void *)(uintptr_t)&word->value, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

embed_word platform_thread_id(void)
{
    // the id of a thread on Linux is a small positive count
    return (embed_word)syscall(SYS_gettid);
}

// A thread on Linux has a nice value of its own, and an idle thread the idle policy. A thread of the run level keeps
// the level that it started at. A rise takes a right that the program does not ask for
void platform_thread_priority(unsigned int level)
{
    if ((level != PLATFORM_PRIORITY_GUEST) && (level != PLATFORM_PRIORITY_IDLE))
    {
        return;
    }
    // the thread's own id, which setpriority takes as a process's on Linux
    const int self = (int)syscall(SYS_gettid);
    (void)setpriority(PRIO_PROCESS, (id_t)self, (level == PLATFORM_PRIORITY_IDLE) ? 19 : 9);
    if (level == PLATFORM_PRIORITY_IDLE)
    {
        const struct sched_param none = {0};
        (void)sched_setscheduler(0, SCHED_IDLE, &none);
    }
}
#endif
