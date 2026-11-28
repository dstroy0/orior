// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The two tesseras checked against each other: the device's tessera (tessera_device.h) makes the host daemon's ledger's
// decisions (tessera_ledger.c), one source for both (tessera_ledger_core.h). Each round draws a stream of calls,
// every type of call with its times, bytes and jobs drawn from a seeded stream, and makes them of a host ledger and
// of a ledger on the device, arranged with arrays of one to make it grow. Every answer must be the host's field for
// field, and the ledger each is left with the host's job for job, recorded peak for recorded peak and deadline for
// deadline. The test is one job on the device's tessera daemon, submitted before its first device work.
#include "../../../../../../../src/cu/engine/runtime/daemon/tessera_device.h"
#include "../../../../../../../src/cu/engine/runtime/daemon/tessera_ledger.h"
#include "../../../../../../../includes/platform/platform.h"
#include "sim.h"

#include <cuda_runtime.h>

#include "memoria_operor/memoria_operor.h"
#include <stdio.h>

// the max the test puts on the device at once: a round's calls and answers, and the ledger's arrays
#define DEVICE_TEST_DECLARED (64ull << 20u)

#define DEVICE_TEST_ROUNDS 200u
#define DEVICE_TEST_CALLS 400u

// the signums a round's jobs are drawn from, few enough that a signum's recorded peak judges later jobs
#define DEVICE_TEST_SIGNUMS 6u

// the device's capacity in each round
#define DEVICE_TEST_CAPACITY 1000ull

static unsigned long long s_device_test_state = 0x9E3779B97F4A7C15ull;

static unsigned long long device_test_random(void)
{
    s_device_test_state ^= s_device_test_state << 13u;
    s_device_test_state ^= s_device_test_state >> 7u;
    s_device_test_state ^= s_device_test_state << 17u;
    return s_device_test_state;
}

static unsigned long long device_test_below(unsigned long long bound)
{
    return device_test_random() % bound;
}

// the checks made and failed
struct DeviceTestResults
{
    unsigned int checks;
    unsigned int failed;
};

static void device_test_check(DeviceTestResults *results, int passed, const char *what, unsigned int round)
{
    results->checks += 1u;
    results->failed += passed ? 0u : 1u;
    if (!passed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  FAIL round ");
        mmgr_line_decimal(output_line, round, 1u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, what);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

static EngineSignum device_test_signum(unsigned long long seed)
{
    EngineSignum signum;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &signum, .bytes = sizeof(signum), .val = 0);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // each byte is one eighth of the seed, which fits in an unsigned char
        signum.bytes[at] = (unsigned char)(seed >> (8u * at));
    }
    return signum;
}

// one call drawn: its type, and what that type reads, at `now`; `submitted` is the submits drawn before it, and a
// job it names is one of those or one past them
static TesseraCall device_test_call(unsigned long long now, unsigned long long submitted)
{
    TesseraCall call;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &call, .bytes = sizeof(call), .val = 0);
    // the types are ten, and the draw is below ten
    call.type = (TesseraCallType)device_test_below(10ull);
    call.now = now;
    call.identity = 1ull + device_test_below(submitted + 1ull);
    // one ledger holds every device: each call and each job it requests is on one of three, drawn
    call.device = (unsigned int)device_test_below(3ull);
    call.request.device = call.device;
    // a job inside another names one of the jobs submitted before it as its parent, or none
    call.request.parent = device_test_below(submitted + 1ull);
    call.capacity = DEVICE_TEST_CAPACITY;
    call.in_use = device_test_below(DEVICE_TEST_CAPACITY);
    call.used = device_test_below(900ull);
    call.finished = (int)device_test_below(2ull);
    call.start_limit = 1ull + device_test_below(TESSERA_CALL_EVENTS + 4ull);
    call.idle_microseconds = 1000ull + device_test_below(5000ull);
    call.request.signum = device_test_signum(device_test_below(DEVICE_TEST_SIGNUMS));
    call.request.declared = 1ull + device_test_below(600ull);
    call.request.grace_microseconds = 1000ull;
    call.request.sweep_microseconds = 100ull + device_test_below(400ull);
    call.request.idle_microseconds = 5000ull;
    call.request.override_budget = (device_test_below(4ull) == 0ull) ? 1u : 0u;
    call.request.located = device_test_below(300ull);
    call.history.signum = device_test_signum(device_test_below(DEVICE_TEST_SIGNUMS));
    call.history.peak = device_test_below(900ull);
    call.history.duration = device_test_below(3000ull);
    return call;
}

static int device_test_event_same(const TesseraEvent *left, const TesseraEvent *right)
{
    return (left->type == right->type) && (left->identity == right->identity) && (left->bytes == right->bytes) &&
           (left->measured == right->measured);
}

static int device_test_answer_same(const TesseraAnswer *left, const TesseraAnswer *right)
{
    int same = (left->result == right->result) && (left->full == right->full) && (left->identity == right->identity) &&
               (left->headroom == right->headroom) && (left->root.when == right->root.when) &&
               (left->root.identity == right->root.identity) && (left->root.type == right->root.type);
    for (unsigned int at = 0u; at < TESSERA_CALL_EVENTS; at += 1u)
    {
        same = same && device_test_event_same(&left->events[at], &right->events[at]);
    }
    return same;
}

static int device_test_signum_same(const EngineSignum *left, const EngineSignum *right)
{
    return EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->bytes, .other = right->bytes, .bytes = sizeof(left->bytes)) ==
           0;
}

static int device_test_job_same(const TesseraJob *left, const TesseraJob *right)
{
    const TesseraJobRequest *const request = &left->request;
    const TesseraJobRequest *const other = &right->request;
    return (left->identity == right->identity) && device_test_signum_same(&request->signum, &other->signum) &&
           (request->declared == other->declared) && (request->grace_microseconds == other->grace_microseconds) &&
           (request->sweep_microseconds == other->sweep_microseconds) &&
           (request->idle_microseconds == other->idle_microseconds) &&
           (request->override_budget == other->override_budget) && (request->located == other->located) &&
           (left->state == right->state) && (left->reservation == right->reservation) && (left->used == right->used) &&
           (left->peak == right->peak) && (left->measures == right->measures) &&
           (left->submitted == right->submitted) && (left->started == right->started) &&
           (left->expected_end == right->expected_end) && (left->grace_until == right->grace_until) &&
           (left->next_sweep == right->next_sweep);
}

// 1 where two ledgers contain the same device, jobs, recorded peaks, deadlines and idle time
static int device_test_ledger_same(const TesseraLedger *left, const TesseraLedger *right)
{
    int same = (left->capacity == right->capacity) && (left->in_use == right->in_use) &&
               (left->job_count == right->job_count) && (left->history_count == right->history_count) &&
               (left->heap_count == right->heap_count) && (left->next_identity == right->next_identity) &&
               (left->idle_since == right->idle_since) && (left->idle_microseconds == right->idle_microseconds);
    for (unsigned long long at = 0ull; same && (at < left->job_count); at += 1ull)
    {
        same = device_test_job_same(&left->jobs[at], &right->jobs[at]);
    }
    for (unsigned long long at = 0ull; same && (at < left->history_count); at += 1ull)
    {
        same = device_test_signum_same(&left->history[at].signum, &right->history[at].signum) &&
               (left->history[at].peak == right->history[at].peak) &&
               (left->history[at].duration == right->history[at].duration);
    }
    for (unsigned long long at = 0ull; same && (at < left->heap_count); at += 1ull)
    {
        same = (left->heap[at].when == right->heap[at].when) && (left->heap[at].identity == right->heap[at].identity) &&
               (left->heap[at].type == right->heap[at].type);
    }
    return same;
}

// one round: a stream of calls made of the host's ledger and the device's, the answers and the ledgers compared equal
static void device_test_round(DeviceTestResults *results, unsigned int round, unsigned long long *grown)
{
    TesseraCall calls[DEVICE_TEST_CALLS];
    unsigned long long now = 0ull;
    unsigned long long submitted = 0ull;
    for (unsigned int at = 0u; at < DEVICE_TEST_CALLS; at += 1u)
    {
        now += device_test_below(200ull);
        calls[at] = device_test_call(now, submitted);
        submitted += (calls[at].type == TESSERA_CALL_SUBMIT) ? 1ull : 0ull;
    }
    TesseraLedger host;
    tessera_ledger_open(&host);
    TesseraAnswer host_answers[DEVICE_TEST_CALLS];
    for (unsigned int at = 0u; at < DEVICE_TEST_CALLS; at += 1u)
    {
        tessera_ledger_call(&host, &calls[at], &host_answers[at]);
    }
    TesseraDevice device;
    TesseraAnswer device_answers[DEVICE_TEST_CALLS];
    const int opened = tessera_device_open(&device, 1ull);
    const int made = opened && tessera_device_calls(&device, calls, DEVICE_TEST_CALLS, device_answers);
    device_test_check(results, made, "the device makes the calls", round);
    if (made)
    {
        unsigned int differ = 0u;
        for (unsigned int at = 0u; at < DEVICE_TEST_CALLS; at += 1u)
        {
            differ += device_test_answer_same(&host_answers[at], &device_answers[at]) ? 0u : 1u;
        }
        device_test_check(results, differ == 0u, "each answer is the host's", round);
        TesseraLedger copy;
        const int read = tessera_device_read(&device, &copy);
        device_test_check(results, read && device_test_ledger_same(&host, &copy), "the device's ledger is the host's",
                          round);
        *grown += (device.host_ledger.job_capacity > 1ull) ? 1ull : 0ull;
        if (read)
        {
            tessera_ledger_close(&copy);
        }
    }
    if (opened)
    {
        tessera_device_close(&device);
    }
    tessera_ledger_close(&host);
}

int program_main(int count, char **arguments)
{
    DeviceTestResults results = {0u, 0u};
    char job_capacity[SIM_LINE_CAPACITY];
    SimResults job;
    sim_open(&job, job_capacity);
    const int admitted = sim_job_submit(&job, "tessera_device_test", count, arguments, DEVICE_TEST_DECLARED);
    unsigned long long grown = 0ull;
    if (admitted != 0)
    {
        for (unsigned int round = 0u; round < DEVICE_TEST_ROUNDS; round += 1u)
        {
            device_test_round(&results, round, &grown);
        }
    }
    sim_job_release(&job);
    sim_flush(&job);
    device_test_check(&results, (admitted != 0) && (job.failures == 0ull),
                      "tessera: the device's daemon admits the test's job and it releases", 0u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera device test: ");
        mmgr_line_decimal(output_line, (admitted != 0) ? DEVICE_TEST_ROUNDS : 0u, 1u);
        mmgr_line_put(output_line, " rounds of ");
        mmgr_line_decimal(output_line, DEVICE_TEST_CALLS, 1u);
        mmgr_line_put(output_line, " calls, ");
        mmgr_line_decimal(output_line, grown, 1u);
        mmgr_line_put(output_line, " grew the device's job array past one; ");
        mmgr_line_decimal(output_line, results.checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, results.failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (results.failed == 0u) ? 0 : 1;
}
