// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_threads.c: the command line's wakes, address waits and threads on Windows: a thread asleep on a word's
// address with WaitOnAddress until the word moves, woken with WakeByAddressAll, and a thread of its own
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the longest an untimed wait sleeps before its waiter tests again, which bounds a wake landing between the test and
// the sleep
#define CLI_WAKE_RECHECK_MS 10ul

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds)
{
    embed_atomic_word_value was = seen;

    if (nanoseconds == 0u)
    {
        return embed_atomic_word_load(word) != seen;
    }
    // WaitOnAddress takes whole milliseconds: the limit is rounded up, and a limit past the longest a DWORD counts
    // waits that long
    const embed_u64 milliseconds = (nanoseconds / 1000000u) + (embed_u64)((nanoseconds % 1000000u) != 0u);
    const DWORD waited = (nanoseconds == EMBED_WAIT_FOREVER) ? INFINITE
                         : (milliseconds < 0xfffffffeull)    ? (DWORD)milliseconds
                                                             : (DWORD)0xfffffffeull;
    (void)WaitOnAddress(&word->value, &was, sizeof(was), waited);
    return embed_atomic_word_load(word) != seen;
}

void embed_word_wake(embed_atomic_word *word)
{
    WakeByAddressAll(&word->value);
}

void cli_wake_wait(const CliWakeCtx *args)
{
    embed_atomic_word_value seen = embed_atomic_word_load(&args->wake->generation);
    (void)WaitOnAddress(&args->wake->generation.value, &seen, sizeof(seen), CLI_WAKE_RECHECK_MS);
}

int cli_wake_wait_for(const CliWakeCtx *args)
{
    embed_atomic_word_value seen = embed_atomic_word_load(&args->wake->generation);
    // a wait past the longest a DWORD counts waits that long
    const DWORD waited = (DWORD)((args->milliseconds_u64 < 0xfffffffeull) ? args->milliseconds_u64 : 0xfffffffeull);
    return WaitOnAddress(&args->wake->generation.value, &seen, sizeof(seen), waited) != FALSE;
}

int cli_wake_wait_seen_for(const CliWakeCtx *args)
{
    embed_atomic_word_value seen = args->seen;
    // a wait past the longest a DWORD counts waits that long
    const DWORD waited = (DWORD)((args->milliseconds_u64 < 0xfffffffeull) ? args->milliseconds_u64 : 0xfffffffeull);
    return WaitOnAddress(&args->wake->generation.value, &seen, sizeof(seen), waited) != FALSE;
}

unsigned int cli_wake_read(const CliWakeCtx *args)
{
    return (unsigned int)embed_atomic_word_load(&args->wake->generation);
}

void cli_wake_wait_seen(const CliWakeCtx *args)
{
    embed_atomic_word_value seen = args->seen;
    (void)WaitOnAddress(&args->wake->generation.value, &seen, sizeof(seen), INFINITE);
}

void cli_wake_all(const CliWakeCtx *args)
{
    (void)embed_atomic_word_fetch_add(&args->wake->generation, 1u);
    WakeByAddressAll(&args->wake->generation.value);
}

static DWORD WINAPI cli_thread_entry(LPVOID supplied)
{
    // the thread is passed what it runs, which its starter retains for as long as the thread lives
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

embed_word cli_thread_id(void)
{
    return (embed_word)GetCurrentThreadId();
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
