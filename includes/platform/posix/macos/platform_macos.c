// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_macos.c: the platform's calls that macOS alone has (../platform_posix.h). The pages are marked free and
// taken back as the system needs the memory, an address wait is __ulock_wait, and a thread has a quality of service
#if defined(__APPLE__)
// the quality of service of a thread is the system's, outside strict C11
#define _DARWIN_C_SOURCE
#endif
#include "../platform_posix.h"

#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/socket.h>

// The address wait and wake of macOS. The operation is compare and wait, the wake goes to every sleeper, and the call
// returns the error in place of errno. The limit is in microseconds, and 0 is no end
extern int __ulock_wait(uint32_t operation, void *address, uint64_t value, uint32_t timeout_us);
extern int __ulock_wake(uint32_t operation, void *address, uint64_t wake_value);
#define PLATFORM_ULOCK_COMPARE_AND_WAIT 1u
#define PLATFORM_ULOCK_COMPARE_AND_WAIT64 5u
#define PLATFORM_ULOCK_WAKE_ALL 0x00000100u
#define PLATFORM_ULOCK_NO_ERRNO 0x01000000u

const int g_platform_pages_discard = MADV_FREE;

unsigned long long platform_stat_changed_milliseconds(const struct stat *found)
{
    // a file's change since 1970 is not negative here
    return ((unsigned long long)found->st_mtimespec.tv_sec * 1000ULL) +
           ((unsigned long long)found->st_mtimespec.tv_nsec / 1000000ULL);
}

int platform_socket_peer(int fd, unsigned int *peer)
{
    // LOCAL_PEERPID of SOL_LOCAL gives the process
    int pid = 0;
    socklen_t length = sizeof(pid);
    const int read_well = (getsockopt(fd, 0, 2, &pid, &length) == 0);
    // a process number that the system reports is positive, which an unsigned int holds
    *peer = read_well ? (unsigned int)pid : 0u;
    return read_well;
}

void platform_word_slept(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds)
{
    // the limit in microseconds, rounded up, and 0 for no end
    const uint32_t timeout_us =
        (nanoseconds == EMBED_WAIT_INFINITE) ? 0u : (uint32_t)(((embed_u64)nanoseconds + 999ULL) / 1000ULL);
    (void)__ulock_wait(
        ((sizeof(old_value) == 8u) ? PLATFORM_ULOCK_COMPARE_AND_WAIT64 : PLATFORM_ULOCK_COMPARE_AND_WAIT) |
            PLATFORM_ULOCK_NO_ERRNO,
        (void *)(uintptr_t)&word->value, (uint64_t)old_value, timeout_us);
}

void platform_word_woken(embed_atomic_word *word)
{
    const uint32_t operation =
        (sizeof(embed_atomic_word_value) == 8u) ? PLATFORM_ULOCK_COMPARE_AND_WAIT64 : PLATFORM_ULOCK_COMPARE_AND_WAIT;
    (void)__ulock_wake(operation | PLATFORM_ULOCK_WAKE_ALL | PLATFORM_ULOCK_NO_ERRNO, (void *)(uintptr_t)&word->value,
                       0u);
}

embed_word platform_thread_id(void)
{
    // the handle of a thread read as the integer that it is, which names the thread for its life
    return (embed_word)(uintptr_t)pthread_self();
}

// A thread on macOS has a quality of service. A thread of the run level keeps the level that it started at. A rise
// takes a right that the program does not ask for
void platform_thread_priority(unsigned int level)
{
    if ((level != PLATFORM_PRIORITY_GUEST) && (level != PLATFORM_PRIORITY_IDLE))
    {
        return;
    }
    (void)pthread_set_qos_class_self_np((level == PLATFORM_PRIORITY_IDLE) ? QOS_CLASS_BACKGROUND : QOS_CLASS_UTILITY,
                                        0);
}
#endif
