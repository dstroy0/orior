// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_tower_bbp.cu: the BBP sum, certificate and lanes
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "pi_tower_internal.h"

// The run on the device. The tail's lanes reduce to the first total. Then sweep after sweep of term lanes follows.
// Each sweep is one cycle of the term program over as many lanes as the device supports at once. It reduces with the
// total carried in
int pi_tower_bbp_sum(SimResults *results, PiTowerBbp *bbp, int report, PiTowerBbpRun *run)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = run, .bytes = sizeof(*run), .val = 0);
    unsigned int *inputs = NULL;
    unsigned int *buffers[2] = {NULL, NULL};
    unsigned int *index = NULL;
    unsigned int *total = NULL;
    const size_t sum_bytes = bbp->out_limbs * sizeof(unsigned int);
    const unsigned long long capacity_bytes = (bbp->lanes + 1ULL) * sum_bytes;
    int ok = (cycle_record_load(&bbp->term.layout, &bbp->term.record, &error) != CYCLE_ERROR) &&
             (cycle_record_load(&bbp->tail.layout, &bbp->tail.record, &error) != CYCLE_ERROR) &&
             (cycle_record_load(&bbp->pair.layout, &bbp->pair.record, &error) != CYCLE_ERROR);
    sim_check(results, ok, "engine: the term, tail and pair programs load");
    ok = ok &&
         sim_status_check(results, cudaMalloc((void **)&inputs, bbp->lanes * bbp->in_limbs * sizeof(unsigned int)),
                          "engine: the lanes' inputs") &&
         sim_status_check(results, cudaMalloc((void **)&buffers[0], capacity_bytes), "engine: the sums") &&
         sim_status_check(results, cudaMalloc((void **)&buffers[1], capacity_bytes), "engine: the sums") &&
         sim_status_check(results, cudaMalloc((void **)&index, (bbp->lanes + 1ULL) * sizeof(unsigned int)),
                          "engine: the index") &&
         sim_status_check(results, cudaMalloc((void **)&total, sum_bytes), "engine: the total") &&
         pi_tower_bbp_counted(results, index, bbp->lanes + 1ULL, 1u, 0ULL) &&
         pi_tower_bbp_counted(results, inputs, bbp->tail_terms, 1u, 1ULL);
    if (ok != 0)
    {
        const CycleRecordRunRequest tail = {bbp->tail.record,
                                            {inputs, NULL, NULL},
                                            {bbp->tail_terms, 0ULL, 0ULL},
                                            NULL,
                                            bbp->tail_terms,
                                            buffers[0],
                                            &error};
        ok = cycle_record_run(&tail) != CYCLE_ERROR;
        sim_check(results, ok, "engine: the tail program runs");
    }
    unsigned int *landed =
        (ok != 0) ? pi_tower_bbp_reduce(results, bbp, buffers, index, bbp->tail_terms, &error) : NULL;
    ok = (landed != NULL) &&
         sim_status_check(results, cudaMemcpy(total, landed, sum_bytes, cudaMemcpyDeviceToDevice), "engine: the total");
    const unsigned long long start = platform_clock_ns();
    double next_report = 1.0;
    while ((ok != 0) && (g_pi_tower_stopped == 0) && (pi_tower_compare(pi_tower_unsigned(run->done), bbp->terms) < 0))
    {
        const PiWide left = pi_tower_difference(bbp->terms, pi_tower_unsigned(run->done));
        const unsigned long long sweep =
            (pi_tower_compare(left, pi_tower_unsigned(bbp->lanes)) > 0) ? bbp->lanes : pi_tower_word(left);
        ok = pi_tower_bbp_counted(results, inputs, sweep, bbp->in_limbs, run->done);
        const CycleRecordRunRequest terms = {
            bbp->term.record, {inputs, NULL, NULL}, {sweep, 0ULL, 0ULL}, NULL, sweep, buffers[0], &error};
        if ((ok != 0) && (cycle_record_run(&terms) == CYCLE_ERROR))
        {
            sim_check(results, 0, "engine: the term program runs a sweep");
            ok = 0;
        }
        ok = ok &&
             sim_status_check(
                 results, cudaMemcpy(&buffers[0][sweep * bbp->out_limbs], total, sum_bytes, cudaMemcpyDeviceToDevice),
                 "engine: the total carried in");
        landed = (ok != 0) ? pi_tower_bbp_reduce(results, bbp, buffers, index, sweep + 1ULL, &error) : NULL;
        ok = (landed != NULL) &&
             sim_status_check(results, cudaMemcpy(total, landed, sum_bytes, cudaMemcpyDeviceToDevice),
                              "engine: the total");
        run->done += (ok != 0) ? sweep : 0ULL;
        run->sweeps += (ok != 0) ? 1ULL : 0ULL;
        // the nanoseconds since the start, as seconds in a double
        run->seconds = (double)(platform_clock_ns() - start) / 1000000000.0;
        if ((report != 0) && (run->seconds >= next_report))
        {
            pi_tower_bbp_progress(results, bbp, run);
            next_report *= 2.0;
        }
    }
    run->finished = (ok != 0) && (pi_tower_compare(pi_tower_unsigned(run->done), bbp->terms) == 0);
    ok = ok && ((run->finished == 0) || pi_tower_bbp_read(results, bbp, total, &run->sum));
    cudaFree(inputs);
    cudaFree(buffers[0]);
    cudaFree(buffers[1]);
    cudaFree(index);
    cudaFree(total);
    cycle_record_release(bbp->term.record);
    cycle_record_release(bbp->tail.record);
    cycle_record_release(bbp->pair.record);
    bbp->term.record = NULL;
    bbp->tail.record = NULL;
    bbp->pair.record = NULL;
    return ok;
}

// The leading bits that every value within the error of the sum shares. The real {16^d pi} 2^W lies strictly within E
// units of the sum S. S - E and S + E decide floor(x / 2^(W - k)) wherever they agree on it, and neither wraps
unsigned int pi_tower_bbp_certified(const PiTowerBbp *bbp, const PiWide &sum)
{
    const PiWide error = pi_tower_bbp_error(bbp);
    const PiWide low = pi_tower_difference(sum, error);
    const PiWide high = pi_tower_sum(sum, error);
    if ((low.sign < 0) || (pi_tower_compare(high, pi_tower_power_two(bbp->fraction_bits)) >= 0))
    {
        return 0u;
    }
    unsigned int bits = bbp->fraction_bits;
    while ((bits > 0u) &&
           (pi_tower_compare(pi_tower_quotient(low, pi_tower_power_two(bbp->fraction_bits - bits)),
                             pi_tower_quotient(high, pi_tower_power_two(bbp->fraction_bits - bits))) != 0))
    {
        bits -= 1u;
    }
    return bits;
}

// The first `digits` hex digits of the sum's fraction, terminated, into the `room` bytes at `hex`. The run ends where
// the digits and their terminator pass the room
void pi_tower_bbp_hex(const PiTowerBbp *bbp, const PiWide &sum, unsigned int digits, char *hex, size_t room)
{
    if ((size_t)digits >= room)
    {
        pi_tower_refused("a sum's hex digits pass their room");
    }
    for (unsigned int digit = 0u; digit < digits; digit += 1u)
    {
        const PiWide shifted = pi_tower_quotient(sum, pi_tower_power_two(bbp->fraction_bits - (4u * (digit + 1u))));
        hex[digit] = "0123456789ABCDEF"[pi_tower_word(shifted) & 15ULL];
    }
    hex[digits] = '\0';
}

// one run's line: its size on the engine, how it went, and the digits it certified
void pi_tower_bbp_print(SimResults *results, const PiTowerBbp *bbp, const PiTowerBbpRun *run, unsigned int certified)
{
    mmgr_line_put(&results->line, "    hex position ");
    pi_tower_print_decimal(&results->line, bbp->position);
    mmgr_line_put(&results->line, ": ");
    pi_tower_print_decimal(&results->line, bbp->terms);
    mmgr_line_put(&results->line, " term lanes of ");
    mmgr_line_decimal(&results->line, bbp->term.layout.steps, 1u);
    mmgr_line_put(&results->line, " steps on ");
    mmgr_line_decimal(&results->line, bbp->term.layout.file_limbs, 1u);
    mmgr_line_put(&results->line, " register limbs, ");
    mmgr_line_decimal(&results->line, bbp->tail_terms, 1u);
    mmgr_line_put(&results->line, " tail lanes, W ");
    mmgr_line_decimal(&results->line, bbp->fraction_bits, 1u);
    mmgr_line_put(&results->line, "; ");
    mmgr_line_decimal(&results->line, run->sweeps, 1u);
    mmgr_line_put(&results->line, " cycles of up to ");
    mmgr_line_decimal(&results->line, bbp->lanes, 1u);
    mmgr_line_put(&results->line, " lanes in ");
    // a run's seconds are non-negative and far below 2^64 microseconds. The floor fits the word
    sim_fraction_print(&results->line, (unsigned long long)(run->seconds * 1000000.0), 1000000ULL, 3u);
    mmgr_line_put(&results->line, " s");
    if (run->finished != 0)
    {
        mmgr_line_put(&results->line, "; ");
        mmgr_line_decimal(&results->line, certified, 1u);
        mmgr_line_put(&results->line, " bits certified: ");
        char hex[PI_TOWER_HEX_MAX];
        pi_tower_bbp_hex(bbp, run->sum, certified / 4u, hex, sizeof(hex));
        mmgr_line_put(&results->line, hex);
    }
    mmgr_line_ch(&results->line, '\n');
    sim_flush(results);
}

// the lanes a sweep spans: every thread resident on the device at once
unsigned long long pi_tower_bbp_lanes(void)
{
    int device = 0;
    cudaDeviceProp properties;
    if ((cudaGetDevice(&device) != cudaSuccess) || (cudaGetDeviceProperties(&properties, device) != cudaSuccess))
    {
        return 0ULL;
    }
    // both counts are positive device properties
    return (unsigned long long)properties.multiProcessorCount *
           (unsigned long long)properties.maxThreadsPerMultiProcessor;
}

// The hex position whose window contains the turn at depth n, and the bits of the window through it. The cell at 2^n
// is floor(alpha 2^n), and its low bits are the bits n - 63 .. n of alpha. The run from position floor((n - 64) / 4)
// contains them within its first 67
PiWide pi_tower_depth_position(const PiWide &depth, unsigned int *window)
{
    const PiWide range = pi_tower_unsigned(64ULL);
    if (pi_tower_compare(depth, range) <= 0)
    {
        // at max 64
        *window = (unsigned int)pi_tower_word(depth);
        return pi_tower_unsigned(0ULL);
    }
    const PiWide position = pi_tower_quotient(pi_tower_difference(depth, range), pi_tower_unsigned(4ULL));
    // depth - 4 d is 64 to 67
    *window =
        (unsigned int)pi_tower_word(pi_tower_difference(depth, pi_tower_product(position, pi_tower_unsigned(4ULL))));
    return position;
}

// the low 64 bits of the engine's cell at depth n: the window's first `window` bits
unsigned long long pi_tower_bbp_cell(const PiTowerBbp *bbp, const PiWide &sum, unsigned int window)
{
    return pi_tower_word(pi_tower_quotient(sum, pi_tower_power_two(bbp->fraction_bits - window)));
}

// the bytes a segment's largest window occupies on the device: its last, whose position and widths are the largest
static int pi_tower_segment_declared(const PiWide &position, unsigned int digits, unsigned long long lanes,
                                     unsigned long long *declared, EngineError *error)
{
    const unsigned int windows = (digits + PI_TOWER_SEGMENT_STEP - 1u) / PI_TOWER_SEGMENT_STEP;
    const PiWide last =
        pi_tower_sum(position, pi_tower_unsigned((unsigned long long)(windows - 1u) * PI_TOWER_SEGMENT_STEP));
    PiTowerBbp bbp;
    if (pi_tower_bbp_plan(&bbp, last, lanes, error) == 0)
    {
        return 0;
    }
    const unsigned long long bytes = pi_tower_bbp_bytes(&bbp);
    *declared = (bytes > *declared) ? bytes : *declared;
    pi_tower_bbp_free(&bbp);
    return 1;
}

// 15. A segment of `digits` hex digits from hex position d + 1. The sim reads it as windows at d, d + 20, d + 40, ...
// Each window is its own run on the engine. Each window must certify PI_TOWER_BBP_CERTIFIED / 4 = 24 digits. It gives
// the segment its first 20. The 4 or more that it certifies past them must be the first of the next window. Bailey
// checks a result at position d with a run at position d - 1.
// The segment's digits go into `segment`, which holds PI_TOWER_SEGMENT_MAX of them and a terminator, and their count
// into `length`
static PiTowerSegment pi_tower_bbp_segment(SimResults *results, const PiWide &position, unsigned int digits,
                                           unsigned long long lanes, int report, char *segment, unsigned int *length)
{
    PiTowerSegment result = {1, 1, 1, 1, 0u};
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    *length = 0u;
    segment[0] = '\0';
    // the digits a window certifies past the ones it gives, which the next window's first must be
    char pending[PI_TOWER_HEX_MAX];
    unsigned int pending_length = 0u;
    while ((result.ran != 0) && (result.finished != 0) && (*length < digits))
    {
        const PiWide window =
            pi_tower_sum(position, pi_tower_unsigned((unsigned long long)result.windows * PI_TOWER_SEGMENT_STEP));
        PiTowerBbp bbp;
        if (pi_tower_bbp_plan(&bbp, window, lanes, &error) == 0)
        {
            result.ran = 0;
            break;
        }
        PiTowerBbpRun run;
        result.ran = pi_tower_bbp_sum(results, &bbp, report, &run);
        result.finished = (result.ran != 0) && (run.finished != 0);
        const unsigned int certified = (result.finished != 0) ? pi_tower_bbp_certified(&bbp, run.sum) : 0u;
        if (report != 0)
        {
            pi_tower_bbp_print(results, &bbp, &run, certified);
        }
        if (result.finished != 0)
        {
            result.windows += 1u;
            char read[PI_TOWER_HEX_MAX];
            const unsigned int read_length = certified / 4u;
            pi_tower_bbp_hex(&bbp, run.sum, read_length, read, sizeof(read));
            result.certified = result.certified && (read_length >= (PI_TOWER_BBP_CERTIFIED / 4u));
            result.overlapped =
                result.overlapped && (read_length >= pending_length) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = read, .other = pending, .bytes = pending_length) == 0);
            const unsigned int wanted =
                ((digits - *length) < PI_TOWER_SEGMENT_STEP) ? (digits - *length) : PI_TOWER_SEGMENT_STEP;
            // a window gives the digits it read, up to the ones the segment wants
            const unsigned int taken = (read_length < wanted) ? read_length : wanted;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &segment[*length], .src = read, .bytes = taken);
            *length += taken;
            segment[*length] = '\0';
            pending_length = (read_length > PI_TOWER_SEGMENT_STEP) ? (read_length - PI_TOWER_SEGMENT_STEP) : 0u;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = pending, .src = &read[PI_TOWER_SEGMENT_STEP],
                       .bytes = pending_length);
        }
        pi_tower_bbp_free(&bbp);
    }
    return result;
}

// a segment's digits, 64 to a line, each line headed by the position of its first digit
static void pi_tower_segment_print(SimResults *results, const PiWide &position, const char *segment,
                                   unsigned int length)
{
    for (unsigned int at = 0u; at < length; at += 64u)
    {
        mmgr_line_put(&results->line, "    ");
        pi_tower_print_decimal(&results->line,
                               pi_tower_sum(position, pi_tower_unsigned((unsigned long long)at + 1ULL)));
        mmgr_line_put(&results->line, "  ");
        mmgr_line_put_n(&results->line, &segment[at], ((length - at) < 64u) ? (length - at) : 64u);
        mmgr_line_ch(&results->line, '\n');
        sim_flush(results);
    }
}

// 15 on the engine: the segment the request names, the job declared at its last window's bytes, and the segment printed
// 64 digits a line. Where it starts at one of Bailey's positions, its head against his
void pi_tower_segment_request(SimResults *results, int count, char **arguments, const PiWide &position,
                              unsigned int digits)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned long long lanes = pi_tower_bbp_lanes();
    unsigned long long declared = 0ULL;
    const int planned = (lanes != 0ULL) && pi_tower_segment_declared(position, digits, lanes, &declared, &error);
    sim_check(results, planned, "engine: keymath encodes and the scheduler arranges the term, tail and pair programs");
    if ((planned == 0) || !sim_job_submit(results, "pi_tower", count, arguments, declared))
    {
        return;
    }
    mmgr_line_put(&results->line, "  a segment on the engine: ");
    mmgr_line_decimal(&results->line, digits, 1u);
    mmgr_line_put(&results->line, " hex digits from position ");
    pi_tower_print_decimal(&results->line, pi_tower_sum(position, pi_tower_unsigned(1ULL)));
    mmgr_line_put(&results->line, ", a window every ");
    mmgr_line_decimal(&results->line, PI_TOWER_SEGMENT_STEP, 1u);
    mmgr_line_ch(&results->line, '\n');
    sim_flush(results);
    // the segment's digits: one request reads one segment, and the table holds the most a request may name
    static char s_segment[PI_TOWER_SEGMENT_MAX + 1u];
    unsigned int length = 0u;
    const PiTowerSegment result = pi_tower_bbp_segment(results, position, digits, lanes, 1, s_segment, &length);
    pi_tower_segment_print(results, position, s_segment, length);
    mmgr_line_put(&results->line, "    ");
    mmgr_line_decimal(&results->line, result.windows, 1u);
    mmgr_line_put(&results->line, " windows\n");
    sim_flush(results);
    sim_check(results, result.ran && result.finished && (length == digits),
              "engine: every window ran and summed all its terms");
    sim_check(results, result.certified, "every window certified at least 24 hex digits");
    sim_check(results, result.overlapped, "every window's digits past the 20 it gives are the next window's first");
    int matched = 1;
    unsigned int compared = 0u;
    for (size_t at = 0u; at < PI_TOWER_PUBLISHED_COUNT; at += 1u)
    {
        const char *const expected = s_pi_tower_published[at].digits;
        const size_t expected_length =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = expected, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        if ((pi_tower_compare(pi_tower_unsigned(s_pi_tower_published[at].position - 1ULL), position) == 0) &&
            ((size_t)length >= expected_length))
        {
            mmgr_line_put(&results->line, "    Bailey's at position ");
            mmgr_line_decimal(&results->line, s_pi_tower_published[at].position, 1u);
            mmgr_line_put(&results->line, ": ");
            mmgr_line_put(&results->line, expected);
            mmgr_line_ch(&results->line, '\n');
            matched = matched && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = s_segment, .other = expected,
                                             .bytes = expected_length) == 0);
            compared += 1u;
        }
    }
    if (compared != 0u)
    {
        sim_check(results, matched, "the segment's head is Bailey's published digits at its position");
    }
    sim_flush(results);
}
