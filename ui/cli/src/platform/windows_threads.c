// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_threads.c: the command line's locks, wakes and threads on Windows: a slim reader-writer lock taken for
// writing, a condition variable waited on with it, and a thread of its own
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// a slim reader-writer lock and a condition variable are each one pointer, zero where free, the size of their storage
_Static_assert(sizeof(SRWLOCK) == sizeof(void *), "CliLock: SRWLOCK is not one pointer wide");
_Static_assert(sizeof(CONDITION_VARIABLE) == sizeof(void *), "CliWake: CONDITION_VARIABLE is not one pointer wide");

void cli_lock_take(const CliLockCtx *args)
{
    // the storage is the lock's one word, laid out as SRWLOCK is
    AcquireSRWLockExclusive((PSRWLOCK)(void *)&args->lock->system_word);
}

void cli_lock_give(const CliLockCtx *args)
{
    ReleaseSRWLockExclusive((PSRWLOCK)(void *)&args->lock->system_word);
}

void cli_wake_wait(const CliLockCtx *args)
{
    // each is its one word, laid out as the system's type is
    SleepConditionVariableSRW((PCONDITION_VARIABLE)(void *)&args->wake->system_word,
                              (PSRWLOCK)(void *)&args->lock->system_word, INFINITE, 0ul);
}

int cli_wake_wait_for(const CliLockCtx *args)
{
    // a wait past the longest a DWORD counts waits that long
    const DWORD waited = (DWORD)((args->milliseconds_u64 < 0xfffffffeull) ? args->milliseconds_u64 : 0xfffffffeull);
    return SleepConditionVariableSRW((PCONDITION_VARIABLE)(void *)&args->wake->system_word,
                                     (PSRWLOCK)(void *)&args->lock->system_word, waited, 0ul) != 0;
}

void cli_wake_all(const CliLockCtx *args)
{
    WakeAllConditionVariable((PCONDITION_VARIABLE)(void *)&args->wake->system_word);
}

static DWORD WINAPI cli_thread_entry(LPVOID given)
{
    // the thread is handed what it runs, which its starter retains for as long as the thread lives
    const CliThread *const thread = (const CliThread *)given;
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx, .priority = thread->priority);
    thread->run(thread->context);
    return 0u;
}

void cli_priority_own(const CliWordsErrMsgCtx *args)
{
    const int level = (args->priority == CLI_PRIORITY_RUN)     ? THREAD_PRIORITY_ABOVE_NORMAL
                      : (args->priority == CLI_PRIORITY_GUEST) ? THREAD_PRIORITY_BELOW_NORMAL
                      : (args->priority == CLI_PRIORITY_IDLE)  ? THREAD_PRIORITY_IDLE
                                                               : THREAD_PRIORITY_NORMAL;
    SetThreadPriority(GetCurrentThread(), level);
}

size_t cli_thread_id(void)
{
    return (size_t)GetCurrentThreadId();
}

void cli_thread_yield(void)
{
    (void)SwitchToThread();
}

int cli_thread_start(const CliThreadCtx *args)
{
    const HANDLE started = CreateThread(NULL, 0u, cli_thread_entry, args->thread, 0u, NULL);
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
