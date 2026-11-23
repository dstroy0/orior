// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chaitin_omega.cu: the kernel, the ledger and the device run
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "chaitin_omega_internal.h"

// the bytes of one thread's buffers, each rounded to 16
static __host__ __device__ size_t omega_buffers_bytes(unsigned int tokens, unsigned int token_capacity)
{
    const size_t shorts = (((size_t)token_capacity * 2u) + 15u) & ~(size_t)15u;
    const size_t token_bytes = (((size_t)tokens * 2u) + 15u) & ~(size_t)15u;
    const size_t ends = ((((size_t)token_capacity + 1u) * 2u) + 15u) & ~(size_t)15u;
    const size_t stored_ends = ((((size_t)tokens + 1u) * 2u) + 15u) & ~(size_t)15u;
    const size_t stack = (((size_t)token_capacity + 1u) + 15u) & ~(size_t)15u;
    return (2u * shorts) + token_bytes + ends + stored_ends + stack;
}

static __global__ void omega_kernel(OmegaBatch *batch, const unsigned long long *__restrict__ counts,
                                    unsigned int length, unsigned long long from, unsigned long long total,
                                    unsigned int steps, unsigned int tokens, int steps_short, int tokens_short,
                                    unsigned int buffer, unsigned char *scratch, unsigned long long *hand)
{
    const size_t thread = ((size_t)blockIdx.x * blockDim.x) + threadIdx.x;
    unsigned char *base = scratch + (thread * omega_buffers_bytes(tokens, buffer));
    const size_t shorts = (((size_t)buffer * 2u) + 15u) & ~(size_t)15u;
    OmegaBuffers buffers;
    buffers.term = (short *)base;
    buffers.next = (short *)(base + shorts);
    buffers.stored = (short *)(base + (2u * shorts));
    base += (2u * shorts) + ((((size_t)tokens * 2u) + 15u) & ~(size_t)15u);
    buffers.ends = (unsigned short *)base;
    base += ((((size_t)buffer + 1u) * 2u) + 15u) & ~(size_t)15u;
    buffers.stored_ends = (unsigned short *)base;
    base += ((((size_t)tokens + 1u) * 2u) + 15u) & ~(size_t)15u;
    buffers.stack = base;
    unsigned long long halts = 0ULL;
    unsigned long long steps_key = 0ULL;
    unsigned long long bits_key = 0ULL;
    for (;;)
    {
        const unsigned long long offset = atomicAdd(&batch->next, 1ULL);
        if (offset >= total)
        {
            break;
        }
        short *term = buffers.term;
        short *next = buffers.next;
        unsigned int size = omega_device_unrank(counts, length, from + offset, term);
        unsigned int taken = 0u;
        unsigned int fate = omega_device_run(&buffers, &term, &next, &size, steps, tokens, buffer, &taken);
        if (fate == OMEGA_HALTS)
        {
            unsigned long long bits = 0ULL;
            for (unsigned int at = 0u; at < size; at += 1u)
            {
                bits += (term[at] <= 0) ? 2ULL : ((unsigned long long)term[at] + 1ULL);
            }
            if (bits <= 0xFFFFFFFFULL)
            {
                const unsigned long long rank = 0xFFFFFFFFULL - offset;
                halts += 1ULL;
                steps_key = max(steps_key, ((unsigned long long)taken << 32u) | rank);
                bits_key = max(bits_key, (bits << 32u) | rank);
                continue;
            }
            fate = OMEGA_HOST_RUNS;
        }
        // a small budget reached is no fate: the host runs the term under the full ones
        fate = (((fate == OMEGA_OPEN) && (steps_short != 0)) || ((fate == OMEGA_GREW) && (tokens_short != 0)))
                   ? OMEGA_HOST_RUNS
                   : fate;
        hand[atomicAdd(&batch->passed, 1ULL)] = (offset << 8u) | fate;
    }
    atomicAdd(&batch->halts, halts);
    atomicMax(&batch->steps_key, steps_key);
    atomicMax(&batch->bits_key, bits_key);
}

// what one worker that settles a launch's passed back terms reads and writes: the terms it takes, its tables and its
// tally
typedef struct
{
    const OmegaCounts *counts;
    unsigned int length;
    unsigned long long from;
    const unsigned long long *hand;
    unsigned long long passed;
    unsigned int worker;
    unsigned int workers;
    OmegaRewrite rewrite;
    OmegaResults results;
} OmegaHandWorker;

// a launch's passed back terms settled on the host: the fate each carries, or the host's own run
static void omega_hand_worker(void *context)
{
    OmegaHandWorker *const state = (OmegaHandWorker *)context;
    const OmegaCounts *const counts = state->counts;
    for (unsigned long long at = state->worker; at < state->passed; at += state->workers)
    {
        const unsigned long long index = state->from + (state->hand[at] >> 8u);
        OmegaFate fate = (OmegaFate)(state->hand[at] & 0xFFULL);
        unsigned int taken = 0u;
        if ((state->hand[at] & 0xFFULL) == OMEGA_HOST_RUNS)
        {
            state->rewrite.term.count = 0u;
            omega_unrank(counts, state->length, 0u, index, &state->rewrite.term);
            fate = omega_run(&state->rewrite, counts->steps, counts->tokens, &taken);
        }
        omega_settle(counts, state->length, index, fate, taken, &state->rewrite, &state->results);
    }
}

// the `passed` terms of a launch settled by the workers, whose states the caller holds
static void omega_hand_settle(const OmegaCounts *counts, unsigned int length, unsigned long long from,
                              const unsigned long long *hand, unsigned long long passed, unsigned int workers,
                              OmegaHandWorker *states, OmegaResults *fates)
{
    static PlatformWorker s_threads[OMEGA_WORKERS_MAX];
    const unsigned int helpers = (passed < 64ULL) ? 1u : workers;
    for (unsigned int worker = 0u; worker < helpers; worker += 1u)
    {
        OmegaHandWorker *const state = &states[worker];
        omega_results_open(&state->results);
        state->counts = counts;
        state->length = length;
        state->from = from;
        state->hand = hand;
        state->passed = passed;
        state->worker = worker;
        state->workers = helpers;
        if (platform_worker_started(&s_threads[worker], omega_hand_worker, state) == 0)
        {
            omega_refused("the system refused a worker");
        }
    }
    for (unsigned int worker = 0u; worker < helpers; worker += 1u)
    {
        platform_worker_joined(&s_threads[worker]);
    }
    for (unsigned int worker = 0u; worker < helpers; worker += 1u)
    {
        omega_merge(fates, &states[worker].results);
    }
}

// The run planned as jobs: each length's terms in rank order, cut every OMEGA_JOB_TERMS. The plan is the same on every
// run. A finished job's exact counts are one line of the ledger, which the run writes whole and flushes. A run finds
// every job that its ledger already records under the same budgets and runs only the rest. A run stopped at any point
// resumes where it stopped, and a longer L costs only the new lengths.
#define OMEGA_JOB_TERMS (1ULL << 28u)

// the ledger's fields a line: steps, tokens, length, from, count, the six fates, the contradictions, and the two
// busy beavers with their champions
#define OMEGA_LEDGER_FIELDS 16

// Every whole line of the ledger. A line cut short by a stop mid-write is not a job, and the run makes it again. The
// text is a cell of the temporary tier. The jobs go in one cell of the persistent tier at the count of the ledger's
// lines, which `jobs` names. The caller reads them to the end of its run. The answer is their count [RETURNS
// OWNERSHIP]
static size_t omega_ledger_read(const char *path, OmegaLedgerJob **jobs)
{
    *jobs = NULL;
    size_t length = 0u;
    char *const text = omega_text_read(path, &length);
    if (text == NULL)
    {
        return 0u;
    }
    OmegaLedgerJob *const read_jobs =
        (OmegaLedgerJob *)omega_taken(omega_text_lines(text, length) + 1u, sizeof(OmegaLedgerJob), 0);
    size_t count = 0u;
    char line[1024];
    size_t at = 0u;
    while (omega_text_line(text, length, &at, line, sizeof(line)) != 0)
    {
        OmegaLedgerJob job;
        omega_results_open(&job.results);
        unsigned long long fate[6];
        unsigned long long contradictions = 0ULL;
        unsigned long long max[4];
        const size_t written = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        // the line's numbers, each after its white space, at max OMEGA_LEDGER_FIELDS of them
        unsigned long long field[OMEGA_LEDGER_FIELDS];
        const char *walk = line;
        int fields = 0;
        while (fields < OMEGA_LEDGER_FIELDS)
        {
            const char *end = walk;
            const embed_word value =
                EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = walk, .end = &end, .base = 10u);
            if (end == walk)
            {
                break;
            }
            field[fields] = value;
            walk = end;
            fields += 1;
        }
        if (fields != OMEGA_LEDGER_FIELDS)
        {
            continue;
        }
        // Explicit casts narrow the job's three counts, which the ledger writes as unsigned ints, to that width
        job.steps = (unsigned int)field[0];
        job.tokens = (unsigned int)field[1];
        job.length = (unsigned int)field[2];
        job.from = field[3];
        job.count = field[4];
        for (unsigned int one = 0u; one < 6u; one += 1u)
        {
            fate[one] = field[5u + one];
        }
        contradictions = field[11];
        for (unsigned int one = 0u; one < 4u; one += 1u)
        {
            max[one] = field[12u + one];
        }
        // the whole line and nothing after, and every term of the job given one fate
        if (((size_t)(walk - line) != (written - 1u)) || (job.length > OMEGA_LENGTH_MAX) ||
            ((fate[0] + fate[1] + fate[2] + fate[3] + fate[4] + fate[5]) != job.count))
        {
            continue;
        }
        for (unsigned int one = 0u; one < 6u; one += 1u)
        {
            job.results.fate[one][job.length] = fate[one];
        }
        job.results.contradictions = contradictions;
        job.results.max_steps[job.length] = max[0];
        job.results.steps_champion[job.length] = max[1];
        job.results.max_bits[job.length] = max[2];
        job.results.bits_champion[job.length] = max[3];
        read_jobs[count] = job;
        count += 1u;
    }
    omega_given(text);
    *jobs = read_jobs;
    return count;
}

static int omega_ledger_write(const char *path, const OmegaCounts *counts, unsigned int length, unsigned long long from,
                              unsigned long long count, const OmegaResults *results)
{
    // the call first ends a line cut short by a stop mid-write. This job's line starts a line of its own
    int ended = 1;
    PlatformFile *before = platform_file_open(path, "rb");
    if (before != NULL)
    {
        if (platform_file_seek(before, -1L, PLATFORM_FILE_FROM_END) == 0)
        {
            ended = (platform_file_char_get(before) == '\n');
        }
        platform_file_close(before);
    }
    PlatformFile *file = platform_file_open(path, "a");
    if (file == NULL)
    {
        return 0;
    }
    if (ended == 0)
    {
        platform_file_char_put('\n', file);
    }
    {
        mmgr_line_decimal(platform_file_line(file), counts->steps, 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), counts->tokens, 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), length, 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), from, 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), count, 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->fate[0][length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->fate[1][length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->fate[2][length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->fate[3][length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->fate[4][length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->fate[5][length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->contradictions, 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->max_steps[length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->steps_champion[length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->max_bits[length], 1u);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), results->bits_champion[length], 1u);
        mmgr_line_put(platform_file_line(file), "\n");
    }
    const int written = (platform_file_flush(file) == 0);
    return (platform_file_close(file) == 0) && written;
}

// Every closed term through L bits run on the device as jobs. The host settles each launch's passed back terms while
// the device runs the next. The run sizes the launches toward a quarter second of device time. `ledger` may be NULL.
int omega_device(SimResults *results, const OmegaCounts *counts, unsigned int workers, const char *ledger,
                 unsigned int *threads_used, unsigned long long *host_runs, unsigned long long *jobs_run,
                 unsigned long long *jobs_retained, OmegaResults *fates)
{
    omega_results_open(fates);
    OmegaLedgerJob *done = NULL;
    size_t done_count = 0u;
    if (ledger != NULL)
    {
        done_count = omega_ledger_read(ledger, &done);
    }
    const unsigned int steps = (counts->steps < OMEGA_DEVICE_STEPS) ? counts->steps : OMEGA_DEVICE_STEPS;
    const unsigned int tokens = (counts->tokens < OMEGA_DEVICE_TOKENS) ? counts->tokens : OMEGA_DEVICE_TOKENS;
    const unsigned int token_capacity = 2u * tokens;
    cudaDeviceProp properties;
    int ok = sim_status_check(results, cudaGetDeviceProperties(&properties, 0), "device: properties");
    size_t free_bytes = 0u;
    size_t total_bytes = 0u;
    ok = ok && sim_status_check(results, cudaMemGetInfo(&free_bytes, &total_bytes), "device: memory");
    if (ok == 0)
    {
        return 0;
    }
    const size_t buffers_bytes = omega_buffers_bytes(tokens, token_capacity);
    size_t blocks = ((size_t)properties.multiProcessorCount * OMEGA_THREADS_PER_SM) / OMEGA_BLOCK;
    while ((blocks > 1u) && ((blocks * OMEGA_BLOCK * buffers_bytes) > (free_bytes / 2u)))
    {
        blocks -= 1u;
    }
    *threads_used = (unsigned int)(blocks * OMEGA_BLOCK);
    unsigned char *scratch = NULL;
    unsigned long long *hand = NULL;
    unsigned long long *device_counts = NULL;
    OmegaBatch *batch = NULL;
    cudaEvent_t launched = NULL;
    cudaEvent_t finished = NULL;
    ok = sim_status_check(results, cudaMalloc((void **)&device_counts, sizeof(counts->count)), "device: counts");
    ok = ok && sim_status_check(results,
                                cudaMemcpy(device_counts, counts->count, sizeof(counts->count), cudaMemcpyHostToDevice),
                                "device: counts");
    ok = ok && sim_status_check(results, cudaMalloc((void **)&scratch, blocks * OMEGA_BLOCK * buffers_bytes),
                                "device: buffers");
    ok = ok && sim_status_check(results, cudaMalloc((void **)&hand, OMEGA_BATCH_MAX * sizeof(unsigned long long)),
                                "device: hand");
    ok = ok && sim_status_check(results, cudaMalloc((void **)&batch, sizeof(OmegaBatch)), "device: batch");
    ok = ok && sim_status_check(results, cudaEventCreate(&launched), "device: event");
    ok = ok && sim_status_check(results, cudaEventCreate(&finished), "device: event");
    // the state of each worker that settles the passed back terms, its tables at the room of the token budget
    OmegaHandWorker *const states = (OmegaHandWorker *)omega_taken(workers, sizeof(OmegaHandWorker), 1);
    const size_t room = omega_rewrite_room(counts->tokens);
    for (unsigned int worker = 0u; worker < workers; worker += 1u)
    {
        omega_rewrite_taken(&states[worker].rewrite, room, 1);
    }
    // the launch before this one, whose passed back terms the host settles while this one runs
    unsigned long long *stored_hand = NULL;
    unsigned long long stored_passed = 0ULL;
    unsigned int stored_length = 0u;
    unsigned long long stored_from = 0ULL;
    unsigned long long size = 1ULL << 12u;
    static OmegaResults job;
    for (unsigned int length = 2u; (ok != 0) && (length <= counts->length); length += 1u)
    {
        const unsigned long long total = omega_count(counts, length, 0u);
        for (unsigned long long job_from = 0ULL; (ok != 0) && (job_from < total); job_from += OMEGA_JOB_TERMS)
        {
            const unsigned long long job_count =
                ((total - job_from) < OMEGA_JOB_TERMS) ? (total - job_from) : OMEGA_JOB_TERMS;
            const OmegaLedgerJob *ledger_job = NULL;
            for (size_t at = 0u; at < done_count; at += 1u)
            {
                if ((done[at].steps == counts->steps) && (done[at].tokens == counts->tokens) &&
                    (done[at].length == length) && (done[at].from == job_from) && (done[at].count == job_count))
                {
                    ledger_job = &done[at];
                    break;
                }
            }
            if (ledger_job != NULL)
            {
                omega_merge(fates, &ledger_job->results);
                *jobs_retained += 1ULL;
                continue;
            }
            omega_results_open(&job);
            const unsigned long long job_end = job_from + job_count;
            unsigned long long from = job_from;
            while ((ok != 0) && (from < job_end))
            {
                const unsigned long long take = ((job_end - from) < size) ? (job_end - from) : size;
                ok = sim_status_check(results, cudaMemset(batch, 0, sizeof(OmegaBatch)), "device: open");
                ok = ok && sim_status_check(results, cudaEventRecord(launched), "device: event");
                omega_kernel<<<(unsigned int)blocks, OMEGA_BLOCK>>>(
                    batch, device_counts, length, from, take, steps, tokens, steps < counts->steps,
                    tokens < counts->tokens, token_capacity, scratch, hand);
                ok = ok && sim_status_check(results, cudaGetLastError(), "device: launch");
                ok = ok && sim_status_check(results, cudaEventRecord(finished), "device: event");
                omega_hand_settle(counts, stored_length, stored_from, stored_hand, stored_passed, workers, states,
                                  &job);
                omega_given(stored_hand);
                stored_hand = NULL;
                stored_passed = 0ULL;
                ok = ok && sim_status_check(results, cudaEventSynchronize(finished), "device: run");
                OmegaBatch result;
                ok = ok && sim_status_check(results, cudaMemcpy(&result, batch, sizeof(result), cudaMemcpyDeviceToHost),
                                            "device: batch read");
                float milliseconds = 0.0f;
                ok = ok && sim_status_check(results, cudaEventElapsedTime(&milliseconds, launched, finished),
                                            "device: event");
                if (ok == 0)
                {
                    break;
                }
                stored_passed = result.passed;
                stored_hand = (unsigned long long *)omega_taken(stored_passed, sizeof(unsigned long long), 1);
                ok = (result.passed == 0ULL) ||
                     sim_status_check(results,
                                      cudaMemcpy(stored_hand, hand, result.passed * sizeof(unsigned long long),
                                                 cudaMemcpyDeviceToHost),
                                      "device: hand read");
                stored_length = length;
                stored_from = from;
                job.fate[OMEGA_HALTS][length] += result.halts;
                if (result.steps_key != 0ULL)
                {
                    omega_champion(&job.max_steps[length], &job.steps_champion[length], result.steps_key >> 32u,
                                   from + (0xFFFFFFFFULL - (result.steps_key & 0xFFFFFFFFULL)));
                    omega_champion(&job.max_bits[length], &job.bits_champion[length], result.bits_key >> 32u,
                                   from + (0xFFFFFFFFULL - (result.bits_key & 0xFFFFFFFFULL)));
                }
                for (unsigned long long at = 0ULL; at < result.passed; at += 1ULL)
                {
                    *host_runs += ((stored_hand[at] & 0xFFULL) == OMEGA_HOST_RUNS) ? 1ULL : 0ULL;
                }
                from += take;
                if ((milliseconds < 100.0f) && (take == size) && (size < OMEGA_BATCH_MAX))
                {
                    size *= 2ULL;
                }
                else if ((milliseconds > 500.0f) && (size > 1024ULL))
                {
                    size /= 2ULL;
                }
            }
            // the job is whole only once the host settles its last launch's passed back terms
            omega_hand_settle(counts, stored_length, stored_from, stored_hand, stored_passed, workers, states, &job);
            omega_given(stored_hand);
            stored_hand = NULL;
            stored_passed = 0ULL;
            if (ok == 0)
            {
                break;
            }
            if ((ledger != NULL) && (omega_ledger_write(ledger, counts, length, job_from, job_count, &job) == 0))
            {
                sim_check(results, 0, "device: the ledger takes each finished job");
                ok = 0;
                break;
            }
            omega_merge(fates, &job);
            *jobs_run += 1ULL;
        }
    }
    (void)cudaEventDestroy(finished);
    (void)cudaEventDestroy(launched);
    (void)cudaFree(batch);
    (void)cudaFree(hand);
    (void)cudaFree(scratch);
    (void)cudaFree(device_counts);
    for (unsigned int worker = workers; worker > 0u; worker -= 1u)
    {
        omega_rewrite_given(&states[worker - 1u].rewrite);
    }
    omega_given(states);
    omega_given(done);
    return ok;
}
