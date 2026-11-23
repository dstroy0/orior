// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chaitin_omega_engine.cu: the engine
#include "chaitin_omega_internal.h"
#include "memoria_operor/memoria_operor.h"

// the reduct programs at max, one for each set of widths that a round needs
#define OMEGA_PROGRAMS_MAX 256u

// The reduct programs that the run loaded, each with its key of widths, in a table at a fixed bound with their count.
// The widths of the rounds take few values, and a search of the table finds a program again
static OmegaProgram s_programs[OMEGA_PROGRAMS_MAX];
static unsigned long long s_program_keys[OMEGA_PROGRAMS_MAX];
static size_t s_program_count = 0u;

int omega_engine(SimResults *results, const OmegaCounts *counts, const char *ledger, OmegaEngineReport *report,
                 OmegaCrossed *crossed, OmegaResults *fates)
{
    omega_results_open(fates);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = report, .bytes = sizeof(*report), .val = 0);
    OmegaEngineJob *retained = NULL;
    const size_t retained_count = (ledger != NULL) ? omega_engine_ledger_read(ledger, &retained) : 0u;
    // The plan: each length in rank order, cut every OMEGA_ENGINE_JOB terms. The call counts the jobs first, then takes
    // one cell of the temporary tier at that count, and gives it back before it returns
    size_t job_room = 0u;
    for (unsigned int length = 2u; length <= counts->length; length += 1u)
    {
        const unsigned long long total = omega_count(counts, length, 0u);
        job_room += (size_t)((total + OMEGA_ENGINE_JOB - 1ULL) / OMEGA_ENGINE_JOB);
    }
    OmegaEngineJob *const jobs = (OmegaEngineJob *)omega_taken(job_room, sizeof(OmegaEngineJob), 1);
    size_t job_count = 0u;
    for (unsigned int length = 2u; length <= counts->length; length += 1u)
    {
        const unsigned long long total = omega_count(counts, length, 0u);
        for (unsigned long long from = 0ULL; from < total; from += OMEGA_ENGINE_JOB)
        {
            OmegaEngineJob job;
            job.length = length;
            job.from = from;
            job.count = ((total - from) < OMEGA_ENGINE_JOB) ? (total - from) : OMEGA_ENGINE_JOB;
            job.settled = 0ULL;
            omega_results_open(&job.results);
            const OmegaEngineJob *found = NULL;
            for (size_t at = 0u; at < retained_count; at += 1u)
            {
                if ((retained[at].length == length) && (retained[at].from == from) && (retained[at].count == job.count))
                {
                    found = &retained[at];
                    break;
                }
            }
            if (found != NULL)
            {
                omega_merge(fates, &found->results);
                report->jobs_retained += 1ULL;
                continue;
            }
            jobs[job_count] = job;
            job_count += 1u;
            crossed->owed += (length <= OMEGA_ENGINE_CROSS_MAX) ? job.count : 0ULL;
        }
    }
    omega_given(retained);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    OmegaBuffer buffers[OMEGA_BUFFERS];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = buffers, .bytes = sizeof(buffers), .val = 0);
    OmegaPoolExtent extent = {0ULL, 0ULL, 0ULL, 0ULL};
    // the tables of the settle's type check, for a program of OMEGA_LENGTH_MAX bits at max
    OmegaRewrite rewrite;
    omega_rewrite_taken(&rewrite, OMEGA_LENGTH_MAX, 1);
    const size_t word = sizeof(unsigned long long);
    int ok = (omega_buffer_reserve(&buffers[OMEGA_BUFFER_COUNTS], sizeof(counts->count), 0) == 1) &&
             (omega_buffer_reserve(&buffers[OMEGA_BUFFER_TOTALS], sizeof(OmegaRoundTotals), 0) == 1);
    if (ok == 0)
    {
        sim_check(results, 0, "engine: the device stores the counts and the round's totals");
    }
    ok = ok && sim_status_check(results,
                                cudaMemcpy(buffers[OMEGA_BUFFER_COUNTS].data, counts->count, sizeof(counts->count),
                                           cudaMemcpyHostToDevice),
                                "engine: counts");
    OmegaRoundTotals *const totals = OMEGA_BUFFER(buffers, OMEGA_BUFFER_TOTALS, OmegaRoundTotals);
    size_t next_job = 0u;
    const unsigned long long start = platform_clock_ns();
    unsigned long long report_at = 1ULL;
    while ((ok != 0) && ((next_job < job_count) || (extent.live > 0ULL)))
    {
        // jobs join while fewer than OMEGA_ENGINE_REFILL terms are live and the device fits them beside the rest
        while ((ok != 0) && (next_job < job_count) && (extent.live < OMEGA_ENGINE_REFILL))
        {
            const int admitted =
                omega_pool_insert_job(results, buffers, &jobs[next_job], (unsigned int)next_job, &extent);
            ok = (admitted != 0);
            if (admitted != 1)
            {
                break;
            }
            next_job += 1u;
        }
        if ((ok != 0) && (extent.live == 0ULL))
        {
            sim_check(results, 0, "engine: the device stores a job's terms");
            ok = 0;
        }
        const size_t live_bytes = (size_t)extent.live * word;
        if (ok != 0)
        {
            ok = (omega_buffer_reserve(&buffers[OMEGA_BUFFER_FRAMES], (size_t)extent.tokens, 0) == 1) &&
                 (omega_buffer_reserve(&buffers[OMEGA_BUFFER_INNER_FRAMES], (size_t)extent.tokens, 0) == 1) &&
                 (omega_buffer_reserve(&buffers[OMEGA_BUFFER_OUT_SIZES], live_bytes, 0) == 1) &&
                 (omega_buffer_reserve(&buffers[OMEGA_BUFFER_OUT_BASES], live_bytes, 0) == 1);
            if (ok == 0)
            {
                sim_check(results, 0, "engine: the device stores the round's survey");
            }
        }
        if (ok == 0)
        {
            break;
        }
        OmegaTerm *const terms = OMEGA_BUFFER(buffers, OMEGA_BUFFER_TERMS, OmegaTerm);
        const OmegaToken *const tokens = OMEGA_BUFFER(buffers, OMEGA_BUFFER_TOKENS, OmegaToken);
        unsigned long long *const device_out_sizes = OMEGA_BUFFER(buffers, OMEGA_BUFFER_OUT_SIZES, unsigned long long);
        unsigned long long *const out_bases = OMEGA_BUFFER(buffers, OMEGA_BUFFER_OUT_BASES, unsigned long long);
        // the terms that step this round and the widths of their records
        ok = sim_status_check(results, cudaMemset(totals, 0, sizeof(OmegaRoundTotals)), "engine: totals");
        omega_pool_survey<<<omega_pool_grid(extent.live), OMEGA_POOL_BLOCK>>>(
            terms, extent.live, tokens, OMEGA_BUFFER(buffers, OMEGA_BUFFER_FRAMES, unsigned char), device_out_sizes,
            totals);
        OmegaRoundTotals found;
        ok = ok && sim_status_check(results, cudaGetLastError(), "engine: survey") &&
             sim_status_check(results, cudaMemcpy(&found, totals, sizeof(found), cudaMemcpyDeviceToHost),
                              "engine: survey read");
        unsigned long long out_total = 0ULL;
        ok = ok && omega_buffer_scan(results, &buffers[OMEGA_BUFFER_SCAN], device_out_sizes, out_bases, extent.live,
                                     &out_total);
        if (ok == 0)
        {
            break;
        }
        report->max_tokens = (found.max_tokens > report->max_tokens) ? found.max_tokens : report->max_tokens;
        OmegaRoundWidths widths;
        widths.token_bits = omega_engine_bits_of(found.max_token) + 1u;
        widths.depth_bits = omega_engine_bits_of(found.max_depth);
        widths.bound_bits = omega_engine_bits_of(found.max_bound);
        widths.token_limbs = (widths.token_bits + 31u) / 32u;
        widths.plan_limbs = (widths.depth_bits + widths.bound_bits + 2u + 31u) / 32u;
        // the record program for this round's widths
        const unsigned long long key = ((unsigned long long)widths.token_bits << 40u) |
                                       ((unsigned long long)widths.depth_bits << 20u) | widths.bound_bits;
        size_t found_program = s_program_count;
        for (size_t at = 0u; at < s_program_count; at += 1u)
        {
            found_program = (s_program_keys[at] == key) ? at : found_program;
        }
        if (found_program == s_program_count)
        {
            if (s_program_count == OMEGA_PROGRAMS_MAX)
            {
                omega_refused("the reduct programs pass their table");
            }
            OmegaProgram *const made = &s_programs[s_program_count];
            made->token_bits = widths.token_bits;
            made->depth_bits = widths.depth_bits;
            made->bound_bits = widths.bound_bits;
            if (omega_program_load(made, &error) == 0)
            {
                sim_check(results, 0, "engine: the reduct program encodes and loads for the round's widths");
                ok = 0;
                break;
            }
            s_program_keys[s_program_count] = key;
            s_program_count += 1u;
        }
        const OmegaProgram *const program = &s_programs[found_program];
        const unsigned int out_limbs = program->layout.out_limbs;
        // The round's buffers. While the device cannot fit them, the run parks the stepping term with the largest
        // reduct as outgrown and arranges the reducts again without it
        const OmegaBufferName round_buffers[19] = {OMEGA_BUFFER_PACKED,          OMEGA_BUFFER_PLANS,
                                                   OMEGA_BUFFER_INDEX,           OMEGA_BUFFER_OUT,
                                                   OMEGA_BUFFER_REDUCTS,         OMEGA_BUFFER_REDUCT_ENDS,
                                                   OMEGA_BUFFER_RETAINED_TERMS,  OMEGA_BUFFER_RETAINED_TOKENS,
                                                   OMEGA_BUFFER_RETAINED_STORED, OMEGA_BUFFER_RETAINED_ENDS,
                                                   OMEGA_BUFFER_SETTLED,         OMEGA_BUFFER_PLACES,
                                                   OMEGA_BUFFER_PLACE_BASES,     OMEGA_BUFFER_TOKEN_SIZES,
                                                   OMEGA_BUFFER_TOKEN_BASES,     OMEGA_BUFFER_STORED_SIZES,
                                                   OMEGA_BUFFER_STORED_BASES,    OMEGA_BUFFER_END_SIZES,
                                                   OMEGA_BUFFER_END_BASES};
        int reserved = 0;
        while ((ok != 0) && (reserved == 0))
        {
            const unsigned long long lanes = (out_total < OMEGA_ENGINE_SWEEP) ? out_total : OMEGA_ENGINE_SWEEP;
            const size_t limb = sizeof(unsigned int);
            const size_t bytes[19] = {(size_t)extent.tokens * widths.token_limbs * limb,
                                      (size_t)out_total * widths.plan_limbs * limb,
                                      (size_t)out_total * 2u * limb,
                                      (size_t)lanes * out_limbs * limb,
                                      (size_t)out_total * sizeof(OmegaToken),
                                      (size_t)(out_total + extent.live) * word,
                                      (size_t)extent.live * sizeof(OmegaTerm),
                                      (size_t)out_total * sizeof(OmegaToken),
                                      (size_t)(extent.stored + out_total) * sizeof(OmegaToken),
                                      (size_t)(extent.ends + out_total + extent.live) * word,
                                      (size_t)extent.live * sizeof(OmegaSettled),
                                      live_bytes,
                                      live_bytes,
                                      live_bytes,
                                      live_bytes,
                                      live_bytes,
                                      live_bytes,
                                      live_bytes,
                                      live_bytes};
            reserved = 1;
            for (unsigned int buffer = 0u; buffer < 19u; buffer += 1u)
            {
                const int one = omega_buffer_reserve(&buffers[round_buffers[buffer]], bytes[buffer], 0);
                if (one < 0)
                {
                    sim_check(results, 0, "engine: a round's buffer is allocated");
                    ok = 0;
                }
                if (one != 1)
                {
                    reserved = 0;
                    break;
                }
            }
            if ((ok == 0) || (reserved != 0))
            {
                continue;
            }
            // the reduct sizes of the live terms, in a cell of the temporary tier at their count
            unsigned long long *const out_sizes =
                (unsigned long long *)omega_taken(extent.live, sizeof(unsigned long long), 1);
            ok = sim_status_check(results, cudaMemcpy(out_sizes, device_out_sizes, live_bytes, cudaMemcpyDeviceToHost),
                                  "engine: reduct sizes");
            size_t largest = 0u;
            for (size_t at = 1u; at < (size_t)extent.live; at += 1u)
            {
                largest = (out_sizes[at] > out_sizes[largest]) ? at : largest;
            }
            const unsigned long long largest_size = out_sizes[largest];
            omega_given(out_sizes);
            if ((ok != 0) && (largest_size == 0ULL))
            {
                sim_check(results, 0, "engine: the device fits a round once every reduct is parked");
                ok = 0;
            }
            const int grew = OMEGA_GREW;
            const unsigned long long none = 0ULL;
            ok =
                ok &&
                sim_status_check(results, cudaMemcpy(&terms[largest].fate, &grew, sizeof(grew), cudaMemcpyHostToDevice),
                                 "engine: park") &&
                sim_status_check(results,
                                 cudaMemcpy(&device_out_sizes[largest], &none, sizeof(none), cudaMemcpyHostToDevice),
                                 "engine: park") &&
                omega_buffer_scan(results, &buffers[OMEGA_BUFFER_SCAN], device_out_sizes, out_bases, extent.live,
                                  &out_total);
            report->parked += (ok != 0) ? 1ULL : 0ULL;
        }
        if ((ok != 0) && (extent.tokens > OMEGA_ENGINE_INDEX_MAX))
        {
            sim_check(results, 0, "engine: every token a round reads sits within the engine's 32-bit index");
            ok = 0;
        }
        if (ok == 0)
        {
            break;
        }
        unsigned int *const packed = OMEGA_BUFFER(buffers, OMEGA_BUFFER_PACKED, unsigned int);
        unsigned int *const plans = OMEGA_BUFFER(buffers, OMEGA_BUFFER_PLANS, unsigned int);
        unsigned int *const index = OMEGA_BUFFER(buffers, OMEGA_BUFFER_INDEX, unsigned int);
        unsigned int *const out = OMEGA_BUFFER(buffers, OMEGA_BUFFER_OUT, unsigned int);
        OmegaToken *const reducts = OMEGA_BUFFER(buffers, OMEGA_BUFFER_REDUCTS, OmegaToken);
        if (out_total > 0ULL)
        {
            omega_pool_pack<<<omega_pool_grid(extent.tokens), OMEGA_POOL_BLOCK>>>(tokens, extent.tokens, widths,
                                                                                  packed);
            omega_pool_place<<<omega_pool_grid(extent.live), OMEGA_POOL_BLOCK>>>(
                terms, extent.live, tokens, out_bases, OMEGA_BUFFER(buffers, OMEGA_BUFFER_FRAMES, unsigned char),
                OMEGA_BUFFER(buffers, OMEGA_BUFFER_INNER_FRAMES, unsigned char), widths, plans, index);
            ok = sim_status_check(results, cudaGetLastError(), "engine: pack and arrange");
            // the sweeps: every token of the pool is member 0, and each sweep's plans member 1 from its first lane
            for (unsigned long long first = 0ULL; (ok != 0) && (first < out_total); first += OMEGA_ENGINE_SWEEP)
            {
                const unsigned long long lanes =
                    ((out_total - first) < OMEGA_ENGINE_SWEEP) ? (out_total - first) : OMEGA_ENGINE_SWEEP;
                const CycleRecordRunRequest run = {program->record,
                                                   {packed, &plans[first * widths.plan_limbs], NULL},
                                                   {extent.tokens, lanes, 0ULL},
                                                   &index[2ULL * first],
                                                   lanes,
                                                   out,
                                                   &error};
                if (cycle_record_run(&run) == CYCLE_ERROR)
                {
                    sim_check(results, 0, "engine: the record machine runs the round's sweep");
                    ok = 0;
                    break;
                }
                omega_pool_unpack<<<omega_pool_grid(lanes), OMEGA_POOL_BLOCK>>>(
                    out, lanes, out_limbs, program->out_offset, program->out_bits, &reducts[first], totals);
                ok = sim_status_check(results, cudaGetLastError(), "engine: reducts");
                report->sweeps += 1ULL;
                report->records += lanes;
            }
            unsigned int unread = 0u;
            ok = ok &&
                 sim_status_check(results, cudaMemcpy(&unread, &totals->unread, sizeof(unread), cudaMemcpyDeviceToHost),
                                  "engine: reducts read");
            if ((ok != 0) && (unread != 0u))
            {
                sim_check(results, 0, "engine: every reduct token is read back whole");
                ok = 0;
            }
            omega_pool_watch<<<omega_pool_grid(extent.live), OMEGA_POOL_BLOCK>>>(
                terms, extent.live, reducts, out_bases, OMEGA_BUFFER(buffers, OMEGA_BUFFER_STORED, OmegaToken),
                OMEGA_BUFFER(buffers, OMEGA_BUFFER_ENDS, unsigned long long),
                OMEGA_BUFFER(buffers, OMEGA_BUFFER_REDUCT_ENDS, unsigned long long));
            ok = ok && sim_status_check(results, cudaGetLastError(), "engine: watch");
        }
        if (ok == 0)
        {
            break;
        }
        report->rounds += 1ULL;
        // the settled leave the pool into their jobs; the ledger takes a job whose every term settled
        omega_pool_settled<<<omega_pool_grid(extent.live), OMEGA_POOL_BLOCK>>>(
            terms, extent.live, OMEGA_BUFFER(buffers, OMEGA_BUFFER_SETTLED, OmegaSettled), totals);
        unsigned long long settled_count = 0ULL;
        ok = sim_status_check(results, cudaGetLastError(), "engine: settled") &&
             sim_status_check(
                 results, cudaMemcpy(&settled_count, &totals->settled, sizeof(settled_count), cudaMemcpyDeviceToHost),
                 "engine: settled count");
        // the round's settled terms, in a cell of the temporary tier at their count
        OmegaSettled *const settled = (OmegaSettled *)omega_taken(ok ? settled_count : 0ULL, sizeof(OmegaSettled), 1);
        ok = ok && ((settled_count == 0ULL) ||
                    sim_status_check(results,
                                     cudaMemcpy(settled, buffers[OMEGA_BUFFER_SETTLED].data,
                                                (size_t)settled_count * sizeof(OmegaSettled), cudaMemcpyDeviceToHost),
                                     "engine: settled read"));
        for (size_t at = 0u; (ok != 0) && (at < (size_t)settled_count); at += 1u)
        {
            const OmegaSettled *const one = &settled[at];
            report->max_steps = (one->steps > report->max_steps) ? one->steps : report->max_steps;
            OmegaEngineJob *const job = &jobs[one->job];
            omega_engine_settle(counts, job, one, &rewrite, crossed);
            if (job->settled == job->count)
            {
                if ((ledger != NULL) && (omega_engine_ledger_write(ledger, job) == 0))
                {
                    sim_check(results, 0, "engine: the ledger takes each finished job");
                    ok = 0;
                }
                omega_merge(fates, &job->results);
                report->jobs_run += 1ULL;
            }
        }
        omega_given(settled);
        // the retained terms move into the other buffer of each pair, in pool order
        unsigned long long *const places = OMEGA_BUFFER(buffers, OMEGA_BUFFER_PLACES, unsigned long long);
        unsigned long long *const token_sizes = OMEGA_BUFFER(buffers, OMEGA_BUFFER_TOKEN_SIZES, unsigned long long);
        unsigned long long *const stored_sizes = OMEGA_BUFFER(buffers, OMEGA_BUFFER_STORED_SIZES, unsigned long long);
        unsigned long long *const end_sizes = OMEGA_BUFFER(buffers, OMEGA_BUFFER_END_SIZES, unsigned long long);
        unsigned long long *const place_bases = OMEGA_BUFFER(buffers, OMEGA_BUFFER_PLACE_BASES, unsigned long long);
        unsigned long long *const token_bases = OMEGA_BUFFER(buffers, OMEGA_BUFFER_TOKEN_BASES, unsigned long long);
        unsigned long long *const stored_bases = OMEGA_BUFFER(buffers, OMEGA_BUFFER_STORED_BASES, unsigned long long);
        unsigned long long *const end_bases = OMEGA_BUFFER(buffers, OMEGA_BUFFER_END_BASES, unsigned long long);
        omega_pool_retained_sizes<<<omega_pool_grid(extent.live), OMEGA_POOL_BLOCK>>>(
            terms, extent.live, places, token_sizes, stored_sizes, end_sizes);
        OmegaPoolExtent retained_extent = {0ULL, 0ULL, 0ULL, 0ULL};
        ok = ok && sim_status_check(results, cudaGetLastError(), "engine: retained sizes") &&
             omega_buffer_scan(results, &buffers[OMEGA_BUFFER_SCAN], places, place_bases, extent.live,
                               &retained_extent.live) &&
             omega_buffer_scan(results, &buffers[OMEGA_BUFFER_SCAN], token_sizes, token_bases, extent.live,
                               &retained_extent.tokens) &&
             omega_buffer_scan(results, &buffers[OMEGA_BUFFER_SCAN], stored_sizes, stored_bases, extent.live,
                               &retained_extent.stored) &&
             omega_buffer_scan(results, &buffers[OMEGA_BUFFER_SCAN], end_sizes, end_bases, extent.live,
                               &retained_extent.ends);
        if (ok == 0)
        {
            break;
        }
        const unsigned long long retain_blocks =
            (extent.live < OMEGA_POOL_GRID_MAX) ? extent.live : OMEGA_POOL_GRID_MAX;
        // at max OMEGA_POOL_GRID_MAX blocks, which an unsigned int stores
        omega_pool_retain<<<(unsigned int)retain_blocks, 128u>>>(
            terms, extent.live, reducts, OMEGA_BUFFER(buffers, OMEGA_BUFFER_STORED, OmegaToken),
            OMEGA_BUFFER(buffers, OMEGA_BUFFER_ENDS, unsigned long long), place_bases, token_bases, stored_bases,
            end_bases, OMEGA_BUFFER(buffers, OMEGA_BUFFER_RETAINED_TERMS, OmegaTerm),
            OMEGA_BUFFER(buffers, OMEGA_BUFFER_RETAINED_TOKENS, OmegaToken),
            OMEGA_BUFFER(buffers, OMEGA_BUFFER_RETAINED_STORED, OmegaToken),
            OMEGA_BUFFER(buffers, OMEGA_BUFFER_RETAINED_ENDS, unsigned long long));
        ok = sim_status_check(results, cudaGetLastError(), "engine: retain") &&
             sim_status_check(results, cudaDeviceSynchronize(), "engine: retain");
        omega_buffer_swap(&buffers[OMEGA_BUFFER_TERMS], &buffers[OMEGA_BUFFER_RETAINED_TERMS]);
        omega_buffer_swap(&buffers[OMEGA_BUFFER_TOKENS], &buffers[OMEGA_BUFFER_RETAINED_TOKENS]);
        omega_buffer_swap(&buffers[OMEGA_BUFFER_STORED], &buffers[OMEGA_BUFFER_RETAINED_STORED]);
        omega_buffer_swap(&buffers[OMEGA_BUFFER_ENDS], &buffers[OMEGA_BUFFER_RETAINED_ENDS]);
        extent = retained_extent;
        if (report->rounds == report_at)
        {
            // the nanoseconds since the start, as seconds in a double
            const double seconds = (double)(platform_clock_ns() - start) / 1000000000.0;
            mmgr_line_put(&results->line, "  round ");
            mmgr_line_decimal(&results->line, report->rounds, 1u);
            mmgr_line_put(&results->line, ": ");
            mmgr_line_decimal(&results->line, extent.live, 1u);
            mmgr_line_put(&results->line, " live, ");
            mmgr_line_decimal(&results->line, report->jobs_run, 1u);
            mmgr_line_put(&results->line, " of ");
            mmgr_line_decimal(&results->line, job_count, 1u);
            mmgr_line_put(&results->line, " jobs done, ");
            mmgr_line_decimal(&results->line, report->records, 1u);
            mmgr_line_put(&results->line, " records swept, the largest term ");
            mmgr_line_decimal(&results->line, report->max_tokens, 1u);
            mmgr_line_put(&results->line, " tokens, ");
            mmgr_line_decimal(&results->line, (unsigned long long)(seconds * 1000.0), 1u);
            mmgr_line_put(&results->line, " ms\n");
            sim_flush(results);
            report_at *= 2ULL;
        }
    }
    for (size_t at = s_program_count; at > 0u; at -= 1u)
    {
        omega_program_free(&s_programs[at - 1u]);
    }
    s_program_count = 0u;
    for (unsigned int buffer = 0u; buffer < (unsigned int)OMEGA_BUFFERS; buffer += 1u)
    {
        omega_buffer_release(&buffers[buffer]);
    }
    omega_rewrite_given(&rewrite);
    omega_given(jobs);
    return ok;
}
