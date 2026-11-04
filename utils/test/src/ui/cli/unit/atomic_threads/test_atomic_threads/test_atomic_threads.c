// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_atomic_threads.c: threads asleep on a word's address until it moves, and turnstiles: a pass marks its turnstile
// and its end counts it, a drain waits out the passes under way and no others, the next pass through a set with every
// turnstile taken waits for one, and readers never read what a writer released after its drain
#include "ui/cli/src/command_line/atomic_threads.h"
#include "ui/cli/src/command_line/cli_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the most milliseconds a test waits on another thread before it fails
#define TEST_PATIENCE_MS 5000ULL

// the clock let run `milliseconds`
static void clock_passes(unsigned long long milliseconds)
{
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (cli_clock_milliseconds() < until)
    {
    }
}

// `flag` read until it reads `value` or TEST_PATIENCE_MS pass: 1 where it did
static int flag_reached(_Atomic int *flag, int value)
{
    const unsigned long long until = cli_clock_milliseconds() + TEST_PATIENCE_MS;
    while ((atomic_load(flag) != value) && (cli_clock_milliseconds() < until))
    {
    }
    return atomic_load(flag) == value;
}

// a thread started on `run` with `context`, the CliThread in `thread`, which outlives it
static void thread_started(CliThread *thread, void (*run)(void *), void *context)
{
    *thread = (CliThread){run, context, CLI_PRIORITY_WORK};
    TEST_ASSERT_EQUAL_INT(1, cli_thread_start(&(CliThreadCtx){.thread = thread}));
}

static _Atomic unsigned int s_word;
static _Atomic int s_woken;

// asleep on s_word while it reads 0, then marked woken
static void sleeper_run(void *context)
{
    (void)context;
    while (atomic_load(&s_word) == 0u)
    {
        (void)cli_atomic_wait(&(CliAtomicThreadsCfg){.word = &s_word, .seen = 0u, .milliseconds_u64 = CLI_ATOMIC_FOREVER});
    }
    atomic_store(&s_woken, 1);
}

void test_a_sleep_on_a_word_that_moved_ends_at_once_reporting_the_move(void)
{
    _Atomic unsigned int word = 7u;
    const unsigned long long began = cli_clock_milliseconds();
    TEST_ASSERT_EQUAL_INT(1, cli_atomic_wait(&(CliAtomicThreadsCfg){.word = &word, .seen = 6u, .milliseconds_u64 = 1000ULL}));
    TEST_ASSERT_TRUE((cli_clock_milliseconds() - began) < 500ULL);
}

void test_a_sleep_on_a_still_word_ends_with_its_time_and_reports_no_move(void)
{
    _Atomic unsigned int word = 7u;
    TEST_ASSERT_EQUAL_INT(0, cli_atomic_wait(&(CliAtomicThreadsCfg){.word = &word, .seen = 7u, .milliseconds_u64 = 10ULL}));
}

void test_a_thread_asleep_on_a_word_wakes_as_another_moves_it_and_wakes_its_address(void)
{
    static CliThread s_thread;
    atomic_store(&s_word, 0u);
    atomic_store(&s_woken, 0);
    thread_started(&s_thread, sleeper_run, NULL);
    clock_passes(30ULL);
    TEST_ASSERT_EQUAL_INT(0, atomic_load(&s_woken));
    atomic_store(&s_word, 1u);
    cli_atomic_wake(&(CliAtomicThreadsCfg){.word = &s_word});
    TEST_ASSERT_TRUE(flag_reached(&s_woken, 1));
}

void test_a_pass_marks_its_turnstile_passing_and_its_end_clears_it_and_counts_it(void)
{
    static CliTurnstiles s_turnstiles;
    const unsigned int at = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_turnstiles});
    TEST_ASSERT_TRUE(at < CLI_TURNSTILES);
    const unsigned int during = atomic_load(&s_turnstiles.turnstile[at].word);
    TEST_ASSERT_EQUAL_UINT(CLI_TURNSTILE_PASSING, during & (CLI_TURNSTILE_PASSING | CLI_TURNSTILE_SLEEPER));
    cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_turnstiles, .at = at});
    const unsigned int after = atomic_load(&s_turnstiles.turnstile[at].word);
    TEST_ASSERT_EQUAL_UINT(0u, after & (CLI_TURNSTILE_PASSING | CLI_TURNSTILE_SLEEPER));
    TEST_ASSERT_EQUAL_UINT((during & ~(CLI_TURNSTILE_PASSING | CLI_TURNSTILE_SLEEPER)) + CLI_TURNSTILE_PASS, after);
}

void test_a_drain_with_no_pass_under_way_returns_at_once(void)
{
    static CliTurnstiles s_turnstiles;
    const unsigned long long began = cli_clock_milliseconds();
    cli_turnstiles_drain(&(CliAtomicThreadsCfg){.turnstiles = &s_turnstiles});
    TEST_ASSERT_TRUE((cli_clock_milliseconds() - began) < 100ULL);
}

static CliTurnstiles s_reading;
static _Atomic int s_entered;
static _Atomic int s_finished;

// a pass through s_reading that marks its beginning, reads for 60 milliseconds, and marks its end before it leaves
static void long_pass_run(void *context)
{
    (void)context;
    const unsigned int at = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_reading});
    atomic_store(&s_entered, 1);
    clock_passes(60ULL);
    atomic_store(&s_finished, 1);
    cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_reading, .at = at});
}

void test_a_drain_waits_out_a_pass_begun_before_it(void)
{
    static CliThread s_thread;
    atomic_store(&s_entered, 0);
    atomic_store(&s_finished, 0);
    thread_started(&s_thread, long_pass_run, NULL);
    TEST_ASSERT_TRUE(flag_reached(&s_entered, 1));
    cli_turnstiles_drain(&(CliAtomicThreadsCfg){.turnstiles = &s_reading});
    TEST_ASSERT_EQUAL_INT(1, atomic_load(&s_finished));
}

static CliTurnstiles s_full;
static _Atomic int s_passed;

// a pass through s_full, marked once it begins, and ended
static void late_pass_run(void *context)
{
    (void)context;
    const unsigned int at = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_full});
    atomic_store(&s_passed, 1);
    cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_full, .at = at});
}

void test_a_pass_through_a_set_with_every_turnstile_taken_waits_until_one_is_free(void)
{
    static CliThread s_thread;
    unsigned int taken[CLI_TURNSTILES];
    for (unsigned int pass = 0u; pass < CLI_TURNSTILES; pass += 1u)
    {
        taken[pass] = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_full});
    }
    atomic_store(&s_passed, 0);
    thread_started(&s_thread, late_pass_run, NULL);
    clock_passes(40ULL);
    TEST_ASSERT_EQUAL_INT(0, atomic_load(&s_passed));
    cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_full, .at = taken[0]});
    TEST_ASSERT_TRUE(flag_reached(&s_passed, 1));
    for (unsigned int pass = 1u; pass < CLI_TURNSTILES; pass += 1u)
    {
        cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_full, .at = taken[pass]});
    }
}

// the readers that read beside the writer, and the writer's swaps
#define TEST_READERS 6u
#define TEST_SWAPS 20000u

// A record a writer publishes: 1 while it is published or a pass may still read it, 0 once the writer released it
typedef struct
{
    _Atomic int live;
} TestRecord;

static TestRecord s_records[2];
static TestRecord *_Atomic s_published;
static CliTurnstiles s_swapped;
static _Atomic int s_writing;
static _Atomic int s_readers_done;
static _Atomic unsigned long long s_released_reads;
static _Atomic unsigned long long s_reads;

// passes through s_swapped while the writer writes, each reading the published record, counting a read of one released
static void reader_run(void *context)
{
    (void)context;
    while (atomic_load(&s_writing) != 0)
    {
        const unsigned int at = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_swapped});
        const TestRecord *const record = atomic_load_explicit(&s_published, memory_order_acquire);
        if (atomic_load_explicit(&record->live, memory_order_relaxed) == 0)
        {
            atomic_fetch_add(&s_released_reads, 1ULL);
        }
        atomic_fetch_add_explicit(&s_reads, 1ULL, memory_order_relaxed);
        cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_swapped, .at = at});
    }
    atomic_fetch_add(&s_readers_done, 1);
}

void test_readers_never_read_a_record_the_writer_released_after_its_drain(void)
{
    static CliThread s_threads[TEST_READERS];
    atomic_store(&s_records[0].live, 1);
    atomic_store(&s_records[1].live, 0);
    atomic_store(&s_published, &s_records[0]);
    atomic_store(&s_writing, 1);
    atomic_store(&s_readers_done, 0);
    atomic_store(&s_released_reads, 0ULL);
    atomic_store(&s_reads, 0ULL);
    for (unsigned int reader = 0u; reader < TEST_READERS; reader += 1u)
    {
        thread_started(&s_threads[reader], reader_run, NULL);
    }
    for (unsigned int swap = 0u; swap < TEST_SWAPS; swap += 1u)
    {
        TestRecord *const old = atomic_load(&s_published);
        TestRecord *const next = (old == &s_records[0]) ? &s_records[1] : &s_records[0];
        atomic_store_explicit(&next->live, 1, memory_order_relaxed);
        (void)atomic_exchange_explicit(&s_published, next, memory_order_seq_cst);
        cli_turnstiles_drain(&(CliAtomicThreadsCfg){.turnstiles = &s_swapped});
        atomic_store_explicit(&old->live, 0, memory_order_relaxed);
    }
    atomic_store(&s_writing, 0);
    TEST_ASSERT_TRUE(flag_reached(&s_readers_done, (int)TEST_READERS));
    TEST_ASSERT_TRUE(atomic_load(&s_reads) != 0ULL);
    TEST_ASSERT_EQUAL_UINT64(0ULL, atomic_load(&s_released_reads));
}
