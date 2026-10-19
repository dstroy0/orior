// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_threads.c: the command line's locks, wakes and threads on Windows: a slim reader-writer lock taken for
// writing, a condition variable waited on with it, and a thread of its own
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// a slim reader-writer lock and a condition variable are each one pointer, zero where free, the size of their slot
_Static_assert(sizeof(SRWLOCK) == sizeof(void *), "CliLock: SRWLOCK is not one pointer wide");
_Static_assert(sizeof(CONDITION_VARIABLE) == sizeof(void *), "CliWake: CONDITION_VARIABLE is not one pointer wide");

void cli_lock_take(CliLock *lock)
{
    // the slot is the lock's one word, laid out as SRWLOCK is
    AcquireSRWLockExclusive((PSRWLOCK)(void *)&lock->slot);
}

void cli_lock_give(CliLock *lock)
{
    ReleaseSRWLockExclusive((PSRWLOCK)(void *)&lock->slot);
}

void cli_wake_wait(CliWake *wake, CliLock *lock)
{
    // each slot is its one word, laid out as the system's type is
    SleepConditionVariableSRW((PCONDITION_VARIABLE)(void *)&wake->slot, (PSRWLOCK)(void *)&lock->slot, INFINITE, 0ul);
}

int cli_wake_wait_for(CliWake *wake, CliLock *lock, unsigned long long milliseconds)
{
    // a wait past the longest a DWORD counts waits that long
    const DWORD waited = (DWORD)((milliseconds < 0xfffffffeull) ? milliseconds : 0xfffffffeull);
    return SleepConditionVariableSRW((PCONDITION_VARIABLE)(void *)&wake->slot, (PSRWLOCK)(void *)&lock->slot, waited,
                                     0ul) != 0;
}

void cli_wake_all(CliWake *wake)
{
    WakeAllConditionVariable((PCONDITION_VARIABLE)(void *)&wake->slot);
}

static DWORD WINAPI cli_thread_entry(LPVOID given)
{
    // the thread is handed what it runs, which its starter keeps for as long as the thread lives
    const CliThread *const thread = (const CliThread *)given;
    thread->run(thread->context);
    return 0u;
}

int cli_thread_started(CliThread *thread)
{
    const HANDLE started = CreateThread(NULL, 0u, cli_thread_entry, thread, 0u, NULL);
    if (started == NULL)
    {
        return 0;
    }
    CloseHandle(started);
    return 1;
}

unsigned int cli_processors(void)
{
    const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return (count != 0u) ? (unsigned int)count : 1u;
}
#endif
