// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atomic_threads.h: threads that share memory with no mutex, through atomic operations and bits in one word. A thread
// sleeps on a word's address until the word moves off the value it read: a futex on Linux, WaitOnAddress on Windows and
// __ulock_wait on macOS; and a thread that moves the word wakes the sleepers on its address (platform/posix_threads.c,
// platform/windows_threads.c). Turnstiles let a writer release what readers may still be reading: a reader passes
// through one for as long as it reads, and a writer that has swapped a published pointer waits until every pass that
// began before the swap is over, spinning with a doubling backoff and then asleep on the turnstile's address, before it
// releases what the old pointer led to (atomic_threads.c)
#ifndef ATOMIC_THREADS_H
#define ATOMIC_THREADS_H
#include <stdatomic.h>

typedef struct CliAtomicThreadsCfg CliAtomicThreadsCfg;

#ifdef __cplusplus
extern "C"
{
#endif

// a sleep with no end
#define CLI_ATOMIC_FOREVER 0xffffffffffffffffULL

// the turnstiles of a set, a power of two, each on a cache line of its own
#define CLI_TURNSTILES 64u

// A turnstile's word: bit 0 set while a reader passes, bit 1 set while a thread sleeps on the word, and the bits above
// it the count of passes, which the end of each pass moves on by CLI_TURNSTILE_PASS
#define CLI_TURNSTILE_PASSING 1u
#define CLI_TURNSTILE_SLEEPER 2u
#define CLI_TURNSTILE_PASS 4u

    typedef struct
    {
        _Alignas(64) _Atomic unsigned int word;
    } CliTurnstile;

    // A set of turnstiles, zeros at rest: one set to each thing that readers read while a writer releases its parts
    typedef struct
    {
        CliTurnstile turnstile[CLI_TURNSTILES];
    } CliTurnstiles;

    // A sleep on `word` while it reads `seen`, at most `milliseconds_u64`, CLI_ATOMIC_FOREVER for no end: 1 where the
    // word moved off `seen`, 0 where it did not. A sleep is free to end early, so a sleeper tests what it waits for in
    // a loop. And every thread asleep on `word` woken
    int cli_atomic_wait(const CliAtomicThreadsCfg *args);
    void cli_atomic_wake(const CliAtomicThreadsCfg *args);

    // A pass begun through a free turnstile of `turnstiles`, the one this thread passed through last tried first: its
    // place, which the end of the pass takes as `at`. Where every turnstile has a reader passing, the thread backs off
    // and sleeps until one is free. What the pass reads of a published pointer is read after the pass begins, and no
    // part of it is read after the pass ends
    unsigned int cli_turnstile_enter(const CliAtomicThreadsCfg *args);
    // the pass through the turnstile at `at` of `turnstiles` ended, and a thread asleep on the turnstile woken
    void cli_turnstile_leave(const CliAtomicThreadsCfg *args);
    // Every pass through `turnstiles` begun before this call waited out: a writer swaps a published pointer, calls
    // this, and then releases what the old pointer led to. A pass begun after the swap reads the new pointer, and is
    // not waited on
    // @warning A turnstile's count wraps after 2^30 passes; a writer asleep on a turnstile through exactly
    //          that many passes reads the count it began with and waits on until the pass after them ends
    void cli_turnstiles_drain(const CliAtomicThreadsCfg *args);

    // the word slept on, the value it was read at, and the most milliseconds the sleep lasts; the turnstiles passed
    // through, and the place of the one a pass ends
    struct CliAtomicThreadsCfg
    {
        _Atomic unsigned int *word;
        unsigned int seen;
        unsigned long long milliseconds_u64;
        CliTurnstiles *turnstiles;
        unsigned int at;
    };

#ifdef __cplusplus
}
#endif

#endif
