// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_threads.c: the command line's locks, wakes and threads on Windows: a slim reader-writer lock taken for
// writing, a condition variable waited on with it, and a thread of its own
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the longest an untimed wait sleeps before its waiter tests again, which bounds a wake landing between the test and
// the sleep
#define CLI_WAKE_RECHECK_MS 10ul

void cli_wake_wait(const CliWakeCtx *args)
{
    unsigned int seen = atomic_load_explicit(&args->wake->generation, memory_order_acquire);
    (void)WaitOnAddress(&args->wake->generation, &seen, sizeof(seen), CLI_WAKE_RECHECK_MS);
}

int cli_wake_wait_for(const CliWakeCtx *args)
{
    unsigned int seen = atomic_load_explicit(&args->wake->generation, memory_order_acquire);
    // a wait past the longest a DWORD counts waits that long
    const DWORD waited = (DWORD)((args->milliseconds_u64 < 0xfffffffeull) ? args->milliseconds_u64 : 0xfffffffeull);
    return WaitOnAddress(&args->wake->generation, &seen, sizeof(seen), waited) != FALSE;
}

int cli_wake_wait_seen_for(const CliWakeCtx *args)
{
    unsigned int seen = args->seen;
    // a wait past the longest a DWORD counts waits that long
    const DWORD waited = (DWORD)((args->milliseconds_u64 < 0xfffffffeull) ? args->milliseconds_u64 : 0xfffffffeull);
    return WaitOnAddress(&args->wake->generation, &seen, sizeof(seen), waited) != FALSE;
}

unsigned int cli_wake_read(const CliWakeCtx *args)
{
    return atomic_load_explicit(&args->wake->generation, memory_order_acquire);
}

void cli_wake_wait_seen(const CliWakeCtx *args)
{
    unsigned int seen = args->seen;
    (void)WaitOnAddress(&args->wake->generation, &seen, sizeof(seen), INFINITE);
}

void cli_wake_all(const CliWakeCtx *args)
{
    (void)atomic_fetch_add_explicit(&args->wake->generation, 1u, memory_order_release);
    WakeByAddressAll(&args->wake->generation);
}

static DWORD WINAPI cli_thread_entry(LPVOID supplied)
{
    // the thread is handed what it runs, which its starter retains for as long as the thread lives
    const CliThread *const thread = (const CliThread *)supplied;
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx, .priority = thread->priority);
    thread->run(thread->context);
    return 0u;
}

void cli_priority_own(const CliWordsErrMsgCtx *args)
{
    int level;
    if (args->priority == CLI_PRIORITY_RUN)
    {
        level = THREAD_PRIORITY_ABOVE_NORMAL;
    }
    else if (args->priority == CLI_PRIORITY_GUEST)
    {
        level = THREAD_PRIORITY_BELOW_NORMAL;
    }
    else if (args->priority == CLI_PRIORITY_IDLE)
    {
        level = THREAD_PRIORITY_IDLE;
    }
    else
    {
        level = THREAD_PRIORITY_NORMAL;
    }
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
