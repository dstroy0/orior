// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// threads.c: the command line's wakes and threads on every system. A thread sleeps on a word's address through
// embed_word_wait until the word moves, and embed_word_wake wakes it (the files of the platform). A thread starts on
// the platform's start and runs at its priority
#include "cli_internal.h"
#include "pool.h"

typedef struct
{
    CliWake *wake;
    embed_atomic_word_value seen;
    unsigned long long milliseconds;
} ThreadsSleepCtx;

// a sleep of `milliseconds` on the wake's word, ended early where the word moves off `seen`: 1 where it moved
static int wake_sleep(const ThreadsSleepCtx *args)
{
    // the word's wait takes nanoseconds, and a sleep past what they count sleeps with no end
    return EMBED_CALL(embed_word_wait, EmbedWordWaitCfg, .word = &args->wake->generation, .old_value = args->seen,
                      .nanoseconds = (args->milliseconds < (EMBED_WAIT_INFINITE / 1000000ULL))
                                         ? (embed_u64)args->milliseconds * 1000000ULL
                                         : EMBED_WAIT_INFINITE);
}

int cli_wake_wait_for(const CliWakeCtx *args)
{
    return EMBED_CALL(
        wake_sleep, ThreadsSleepCtx, .wake = args->wake,
        .seen = EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &args->wake->generation),
        .milliseconds = args->milliseconds_u64);
}

int cli_wake_wait_seen_for(const CliWakeCtx *args)
{
    return EMBED_CALL(wake_sleep, ThreadsSleepCtx, .wake = args->wake, .seen = args->seen,
                      .milliseconds = args->milliseconds_u64);
}

unsigned int cli_wake_read(const CliWakeCtx *args)
{
    return (unsigned int)EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg,
                                    .word = &args->wake->generation);
}

void cli_wake_wait_seen(const CliWakeCtx *args)
{
    // The word's wait compares the word with seen before it sleeps, so a wake that lands first ends it at once. The
    // sleep ends only when the word moves, and so no timer checks it again.
    while (!EMBED_CALL(wake_sleep, ThreadsSleepCtx, .wake = args->wake, .seen = args->seen,
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

// what the platform's thread runs: the thread put at its priority, its call, and its place of the pool given back
static void cli_thread_entered(void *supplied)
{
    // Explicit cast takes the context that the starter gave as the thread that it retains for the thread's life
    const CliThread *const thread = (const CliThread *)supplied;
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx, .priority = thread->priority);
    thread->run(thread->context);
    platform_thread_place_released();
}

void cli_priority_own(const CliWordsErrMsgCtx *args)
{
    platform_thread_priority(args->priority);
}

int cli_thread_start(const CliThreadCtx *args)
{
    args->thread->started = (PlatformThread){cli_thread_entered, args->thread};
    return platform_thread_let_go(&args->thread->started);
}
