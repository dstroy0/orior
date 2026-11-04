// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atomic_threads.c: turnstiles entered with a compare and swap and left with an exchange, and a writer waiting out the
// passes under way at its swap: a run of pauses doubling with each step, then the sleeper bit set and a sleep on the
// turnstile's address whose time doubles up to CLI_ATOMIC_BACKOFF_MOST_MS, the leaving reader waking it at once
#include "atomic_threads.h"

// the processor's pause in a spin, a compiler barrier alone where the processor has none the compiler reaches
#if defined(__has_builtin)
#if __has_builtin(__builtin_ia32_pause)
#define CLI_ATOMIC_PAUSE() __builtin_ia32_pause()
#elif __has_builtin(__builtin_arm_yield)
#define CLI_ATOMIC_PAUSE() __builtin_arm_yield()
#endif
#endif
#if !defined(CLI_ATOMIC_PAUSE)
#define CLI_ATOMIC_PAUSE() atomic_signal_fence(memory_order_seq_cst)
#endif

// the step of a backoff at which its spins end and its sleeps begin, each sleep a millisecond for every
// CLI_ATOMIC_SLEEP_STEP of the step; and the longest sleep, which the steps double up to
#define CLI_ATOMIC_SLEEP_STEP 128u
#define CLI_ATOMIC_BACKOFF_MOST_MS 64u
#define CLI_ATOMIC_STEP_MOST (CLI_ATOMIC_SLEEP_STEP * CLI_ATOMIC_BACKOFF_MOST_MS)

_Static_assert((CLI_TURNSTILES & (CLI_TURNSTILES - 1u)) == 0u, "CLI_TURNSTILES must be a power of two");
_Static_assert(sizeof(CliTurnstile) == 64u, "a turnstile takes one cache line");

// the bits of a turnstile's word that are not its count
#define CLI_TURNSTILE_BITS (CLI_TURNSTILE_PASSING | CLI_TURNSTILE_SLEEPER)

// the turnstile this thread passed through last
static _Thread_local unsigned int t_turnstile_at = 0u;

// One step `step` of a backoff on `word`, last read as `read`. Below CLI_ATOMIC_SLEEP_STEP, `step` pauses. From it, where
// a reader passes, the sleeper bit set and a sleep on the word while it reads that, `step` / CLI_ATOMIC_SLEEP_STEP
// milliseconds at the most, unless the word moved first. The step after it, doubled up to CLI_ATOMIC_STEP_MOST
static unsigned int atomic_threads_backed_off(_Atomic unsigned int *word, unsigned int read, unsigned int step)
{
    if (step < CLI_ATOMIC_SLEEP_STEP)
    {
        for (unsigned int pause = 0u; pause < step; pause += 1u)
        {
            CLI_ATOMIC_PAUSE();
        }
        return step * 2u;
    }
    unsigned int expected = read;
    if ((read & CLI_TURNSTILE_BITS) == CLI_TURNSTILE_PASSING)
    {
        (void)atomic_compare_exchange_strong_explicit(word, &expected, read | CLI_TURNSTILE_SLEEPER,
                                                      memory_order_relaxed, memory_order_relaxed);
    }
    // a word that moved before the bit went on ends the step awake; one with the bit on is slept on
    if (((read & CLI_TURNSTILE_PASSING) != 0u) && (expected == read))
    {
        (void)cli_atomic_wait(&(CliAtomicThreadsCfg){.word = word,
                                                     .seen = read | CLI_TURNSTILE_SLEEPER,
                                                     .milliseconds_u64 = step / CLI_ATOMIC_SLEEP_STEP});
    }
    return (step < CLI_ATOMIC_STEP_MOST) ? (step * 2u) : CLI_ATOMIC_STEP_MOST;
}

unsigned int cli_turnstile_enter(const CliAtomicThreadsCfg *args)
{
    unsigned int step = 1u;
    for (;;)
    {
        for (unsigned int tried = 0u; tried < CLI_TURNSTILES; tried += 1u)
        {
            const unsigned int at = (t_turnstile_at + tried) & (CLI_TURNSTILES - 1u);
            _Atomic unsigned int *const word = &args->turnstiles->turnstile[at].word;
            unsigned int read = atomic_load_explicit(word, memory_order_relaxed);
            // the compare and swap is the test: it begins the pass where the turnstile is still free
            if (((read & CLI_TURNSTILE_PASSING) == 0u) &&
                atomic_compare_exchange_strong_explicit(word, &read, read | CLI_TURNSTILE_PASSING, memory_order_relaxed,
                                                        memory_order_relaxed))
            {
                // the pass begun before anything it reads is read, which the fence of a writer's drain pairs with
                atomic_thread_fence(memory_order_seq_cst);
                t_turnstile_at = at;
                return at;
            }
        }
        _Atomic unsigned int *const word = &args->turnstiles->turnstile[t_turnstile_at].word;
        step = atomic_threads_backed_off(word, atomic_load_explicit(word, memory_order_relaxed), step);
    }
}

void cli_turnstile_leave(const CliAtomicThreadsCfg *args)
{
    _Atomic unsigned int *const word = &args->turnstiles->turnstile[args->at].word;
    // the count moves only at the end of a pass, so it reads the same until the exchange; the sleeper bit may come on
    // between, and the exchange returns it
    const unsigned int read = atomic_load_explicit(word, memory_order_relaxed);
    const unsigned int was = atomic_exchange_explicit(word, (read & ~CLI_TURNSTILE_BITS) + CLI_TURNSTILE_PASS,
                                                      memory_order_release);
    if ((was & CLI_TURNSTILE_SLEEPER) != 0u)
    {
        cli_atomic_wake(&(CliAtomicThreadsCfg){.word = word});
    }
}

void cli_turnstiles_drain(const CliAtomicThreadsCfg *args)
{
    // the writer's swap ordered before every read of the turnstiles, which the fence of a pass's beginning pairs with:
    // a pass this misses reads the swapped pointer
    atomic_thread_fence(memory_order_seq_cst);
    for (unsigned int at = 0u; at < CLI_TURNSTILES; at += 1u)
    {
        _Atomic unsigned int *const word = &args->turnstiles->turnstile[at].word;
        const unsigned int first = atomic_load_explicit(word, memory_order_acquire);
        unsigned int read = first;
        unsigned int step = 1u;
        // a pass under way is over once the count moves off the one it began on
        while (((first & CLI_TURNSTILE_PASSING) != 0u) && (((read ^ first) & ~CLI_TURNSTILE_BITS) == 0u))
        {
            step = atomic_threads_backed_off(word, read, step);
            read = atomic_load_explicit(word, memory_order_acquire);
        }
    }
}
