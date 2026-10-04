// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The gold standard corpora read against each other on the record machine, every distance an exact rational, and
// each pair's verdict taken by an exact comparison instead of a float.
//
//   Usage:  gold_readings <input> <output>
//
// The measure is orior.py's, sections 1 to 3. A corpus is its count of adjacent byte pairs, a_k in cell
// k = 256 b_i + b_(i+1) over each line's UTF-8 bytes, A of them in all. Two corpora are apart by their total
// variation, which over the counts is the exact rational
//
//   D = sum over k of |a_k B - b_k A| / (2 A B).
//
// A corpus's split-half distance is D between its two halves, cut at the midpoint (lines [0, n/2) and [n/2, n)) and
// alternating (the even lines and the odd). A pair of corpora is read where D clears the larger split-half distance
// of the two by the input's margin, m D_s < D, held exactly as m_num D_s_num D_den < m_den D_num D_s_den.
//
// Two programs of the record machine, each built by a request that carries only its field widths:
//   the distance program, one lane a cell of one comparison, with fields a, b, A and B, and the output |a B - b A|;
//     the sum over each run of 65536 lanes is the numerator of that comparison's D.
//   the verdict program, one lane a distance and a split-half distance, with fields D_num, D_den, S_num, S_den,
//     m_num and m_den, and the output COMPARE(D_num S_den m_den, S_num D_den m_num), +1 where the pair reads.
// Counting the pairs is the host's, and every product, difference, sum and comparison is the machine's.
//
// The input is what gold_readings.py writes: "margin <numerator> <denominator>", then each corpus as
// "corpus <name> <lines>" and that many lines.
//
// The output holds the records whole: each comparison's numerator and both totals, then each verdict lane.
//
// Checks:
//   1. each program imprints and lays out;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sum of each run equals the host's;
//   5. every distance lies in [0, 1], its numerator at most its denominator;
//   6. the records are written out.

#include "../../../src/c/engine/analysis/cycle/cycle.h"
#include "../../../src/c/engine/analysis/key_schedule/key_schedule.h"
#include "../../../src/c/engine/analysis/keymath/keymath.h"
#include "../../../src/c/engine/runtime/scriptura/scriptura.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#define GOLD_CELLS 65536u

#define GOLD_DISTANCE_FIELDS 4u
#define GOLD_DISTANCE_STEPS 8u
#define GOLD_DISTANCE_OUTPUT 7u

#define GOLD_VERDICT_FIELDS 6u
#define GOLD_VERDICT_STEPS 11u
#define GOLD_VERDICT_OUTPUT 10u

// the widest total a field holds: 2 A B then fits 59 bits, and ten times a remainder of D fits 63 by long division
#define GOLD_TOTAL_BITS_MOST 29u

typedef struct
{
    std::string name;
    std::vector<std::string> lines;
} GoldCorpus;

// a corpus's pair counts over the 2^16 cells, and how many pairs there are in all
typedef struct
{
    std::vector<unsigned int> counts;
    unsigned long long total;
} GoldTable;

// two tables measured against each other, and the numerator the distance program sums for them
typedef struct
{
    std::string name;
    unsigned int left;
    unsigned int right;
    unsigned long long numerator;
} GoldComparison;

typedef struct
{
    unsigned int count_bits;
    unsigned int total_bits;
} GoldDistanceRequest;

typedef struct
{
    unsigned int numerator_bits;
    unsigned int denominator_bits;
    unsigned int margin_bits;
} GoldVerdictRequest;

// a program, its fields' widths and places in one member's record, and its one output
typedef struct
{
    EngineRecordStep steps[GOLD_VERDICT_STEPS];
    unsigned int count;
    unsigned int field_bits[GOLD_VERDICT_FIELDS];
    unsigned int field_offset[GOLD_VERDICT_FIELDS];
    unsigned int fields;
    unsigned int record_bits;
    unsigned int output;
} GoldProgram;

typedef struct
{
    const char *name;
    GoldProgram program;
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} GoldStage;

static unsigned int gold_step(GoldProgram *program, EngineRecordOperation operation, unsigned int left,
                              unsigned int right)
{
    EngineRecordStep step = {operation, left, right, 0u};
    program->steps[program->count] = step;
    program->count += 1u;
    return program->count - 1u;
}

static unsigned int gold_field(GoldProgram *program, unsigned int bits)
{
    const unsigned int field = program->fields;
    program->field_bits[field] = bits;
    program->field_offset[field] = program->record_bits;
    program->record_bits += bits;
    program->fields += 1u;
    return gold_step(program, ENGINE_RECORD_FIELD, field, 0u);
}

// |a B - b A| for one cell of one comparison
static void gold_distance_program(const GoldDistanceRequest *request, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = gold_field(program, request->count_bits);
    const unsigned int b = gold_field(program, request->count_bits);
    const unsigned int big_a = gold_field(program, request->total_bits);
    const unsigned int big_b = gold_field(program, request->total_bits);
    const unsigned int a_by_b = gold_step(program, ENGINE_RECORD_PRODUCT, a, big_b);
    const unsigned int b_by_a = gold_step(program, ENGINE_RECORD_PRODUCT, b, big_a);
    const unsigned int apart = gold_step(program, ENGINE_RECORD_DIFFERENCE, a_by_b, b_by_a);
    program->output = gold_step(program, ENGINE_RECORD_ABSOLUTE, apart, 0u);
}

// the sign of D_num S_den m_den - S_num D_den m_num, +1 where the distance clears the margin times the split-half
static void gold_verdict_program(const GoldVerdictRequest *request, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int d_num = gold_field(program, request->numerator_bits);
    const unsigned int d_den = gold_field(program, request->denominator_bits);
    const unsigned int s_num = gold_field(program, request->numerator_bits);
    const unsigned int s_den = gold_field(program, request->denominator_bits);
    const unsigned int m_num = gold_field(program, request->margin_bits);
    const unsigned int m_den = gold_field(program, request->margin_bits);
    const unsigned int distance_side = gold_step(program, ENGINE_RECORD_PRODUCT, d_num, s_den);
    const unsigned int distance_whole = gold_step(program, ENGINE_RECORD_PRODUCT, distance_side, m_den);
    const unsigned int half_side = gold_step(program, ENGINE_RECORD_PRODUCT, s_num, d_den);
    const unsigned int half_whole = gold_step(program, ENGINE_RECORD_PRODUCT, half_side, m_num);
    program->output = gold_step(program, ENGINE_RECORD_COMPARE, distance_whole, half_whole);
}

static unsigned int gold_bits(unsigned long long value)
{
    unsigned int bits = 1u;
    while ((bits < 64u) && ((value >> bits) != 0ull))
    {
        bits += 1u;
    }
    return bits;
}

// `bits` of an unsigned value laid into a record at `offset`
static void gold_put(unsigned int *record, unsigned int offset, unsigned int bits, unsigned long long value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int to = offset + bit;
        const unsigned int one = (bit < 64u) ? (unsigned int)((value >> bit) & 1ull) : 0u;
        record[to / 32u] = (record[to / 32u] & ~(1u << (to % 32u))) | (one << (to % 32u));
    }
}

// the pairs of the lines whose numbers `pick` lists, each line's bytes read alone
static GoldTable gold_squash(const std::vector<std::string> &lines, const std::vector<size_t> &pick)
{
    GoldTable table;
    table.counts.assign(GOLD_CELLS, 0u);
    table.total = 0ull;
    for (size_t at = 0u; at < pick.size(); at += 1u)
    {
        const std::string &line = lines[pick[at]];
        for (size_t place = 0u; (place + 1u) < line.size(); place += 1u)
        {
            const unsigned int cell = ((unsigned int)(unsigned char)line[place] << 8u) |
                                      (unsigned int)(unsigned char)line[place + 1u];
            table.counts[cell] += 1u;
            table.total += 1ull;
        }
    }
    return table;
}

static std::vector<size_t> gold_run(size_t from, size_t until, size_t step)
{
    std::vector<size_t> picked;
    for (size_t at = from; at < until; at += step)
    {
        picked.push_back(at);
    }
    return picked;
}

static int gold_read(const char *path, std::vector<GoldCorpus> *corpora, unsigned long long margin[2])
{
    FILE *in = fopen(path, "rb");
    if (in == NULL)
    {
        return 0;
    }
    std::string text;
    char chunk[65536];
    size_t got = 0u;
    while ((got = fread(chunk, 1u, sizeof(chunk), in)) > 0u)
    {
        text.append(chunk, got);
    }
    fclose(in);
    std::vector<std::string> lines;
    size_t start = 0u;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
        {
            end = text.size();
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1u;
    }
    size_t at = 0u;
    if ((lines.size() < 1u) || (sscanf(lines[0].c_str(), "margin %llu %llu", &margin[0], &margin[1]) != 2) ||
        (margin[0] == 0ull) || (margin[1] == 0ull))
    {
        return 0;
    }
    at = 1u;
    while (at < lines.size())
    {
        const std::string &head = lines[at];
        if (head.compare(0u, 7u, "corpus ") != 0)
        {
            return 0;
        }
        const size_t space = head.rfind(' ');
        if (space <= 7u)
        {
            return 0;
        }
        GoldCorpus corpus;
        corpus.name = head.substr(7u, space - 7u);
        const unsigned long long count = strtoull(head.c_str() + space + 1u, NULL, 10);
        at += 1u;
        if ((at + count) > lines.size())
        {
            return 0;
        }
        corpus.lines.assign(lines.begin() + (long long)at, lines.begin() + (long long)(at + count));
        at += (size_t)count;
        corpora->push_back(corpus);
    }
    return corpora->size() >= 2u;
}

// imprints and lays out a stage's program over one member's records
static int gold_lay(SimResults *job, GoldStage *stage, EngineError *error)
{
    const GoldProgram *program = &stage->program;
    const unsigned int outputs[1] = {program->output};
    const KeymathRecordRequest encode = {program->steps, program->count, program->field_bits, program->fields,
                                         1u, outputs, 1u, NULL, 0u, &stage->key, error};
    int ok = keymath_record_encode(&encode) != KEYMATH_ERROR;
    sim_check(job, ok, (std::string("keymath imprints the ") + stage->name + " program").c_str());
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {(program->record_bits + 31u) / 32u, 0u, 0u};
    const KeyScheduleRecordRequest lay = {&stage->key, program->field_offset, program->fields, in_limbs, 1,
                                          &stage->layout, error};
    ok = ok && (key_schedule_record_layout(&lay) != KEY_SCHEDULE_ERROR);
    sim_check(job, ok, (std::string("the scheduler lays the ") + stage->name + " program out").c_str());
    return ok;
}

// runs every lane on the device and on the host, holds the two word for word, and returns the device's records
static int gold_sweep(SimResults *job, GoldStage *stage, const std::vector<unsigned int> &records,
                      unsigned long long lanes, std::vector<unsigned int> *out, unsigned int **device_out,
                      EngineError *error)
{
    const std::string name(stage->name);
    int ok = cycle_record_load(&stage->layout, &stage->record, error) != CYCLE_ERROR;
    sim_check(job, ok, ("the " + name + " program loads onto the device").c_str());
    const unsigned int out_limbs = stage->layout.out_limbs;
    unsigned int *device_in = NULL;
    ok = ok && (cudaMalloc((void **)&device_in, records.size() * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)device_out, lanes * out_limbs * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_in, records.data(), records.size() * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    const CycleRecordRunRequest run = {stage->record, {device_in, NULL, NULL}, {lanes, 0ull, 0ull}, NULL, lanes,
                                       *device_out, error};
    ok = ok && (cycle_record_run(&run) == (long)lanes);
    sim_check(job, ok, ("every lane runs the " + name + " program on the device").c_str());
    out->assign((size_t)(lanes * out_limbs), 0u);
    ok = ok && (cudaMemcpy(out->data(), *device_out, out->size() * sizeof(unsigned int), cudaMemcpyDeviceToHost) ==
                cudaSuccess);
    std::vector<unsigned int> host_out((size_t)(lanes * out_limbs), 0u);
    const CycleRecordHostRequest host = {&stage->layout, {records.data(), NULL, NULL}, {lanes, 0ull, 0ull}, NULL,
                                         lanes, host_out.data(), error};
    const int same = ok && (cycle_record_run_host(&host) == (long)lanes) &&
                     (memcmp(host_out.data(), out->data(), host_out.size() * sizeof(unsigned int)) == 0);
    sim_check(job, same, ("the host's " + name + " records equal the device's word for word").c_str());
    cudaFree(device_in);
    return ok && same;
}

static void gold_release(GoldStage *stage)
{
    if (stage->record != NULL)
    {
        cycle_record_release(stage->record);
    }
    key_schedule_record_release(&stage->layout);
    keymath_record_release(&stage->key);
}

// D as its first `digits` decimal digits, by long division of the exact rational
static std::string gold_digits(unsigned long long numerator, unsigned long long denominator, unsigned int digits)
{
    std::string text = std::to_string(numerator / denominator) + ".";
    unsigned long long rest = numerator % denominator;
    for (unsigned int at = 0u; at < digits; at += 1u)
    {
        rest *= 10ull;
        text += (char)('0' + (int)(rest / denominator));
        rest %= denominator;
    }
    return text;
}

int main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if (count != 3)
    {
        fprintf(stderr, "  usage: gold_readings <input> <output>\n");
        return 2;
    }
    std::vector<GoldCorpus> corpora;
    unsigned long long margin[2] = {0ull, 0ull};
    if (!gold_read(arguments[1], &corpora, margin))
    {
        fprintf(stderr, "  gold_readings: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // the tables: each corpus whole, then its midpoint halves, then its alternating halves
    const size_t named = corpora.size();
    std::vector<GoldTable> tables;
    for (size_t at = 0u; at < named; at += 1u)
    {
        const size_t lines = corpora[at].lines.size();
        tables.push_back(gold_squash(corpora[at].lines, gold_run(0u, lines, 1u)));
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        const size_t lines = corpora[at].lines.size();
        tables.push_back(gold_squash(corpora[at].lines, gold_run(0u, lines / 2u, 1u)));
        tables.push_back(gold_squash(corpora[at].lines, gold_run(lines / 2u, lines, 1u)));
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        const size_t lines = corpora[at].lines.size();
        tables.push_back(gold_squash(corpora[at].lines, gold_run(0u, lines, 2u)));
        tables.push_back(gold_squash(corpora[at].lines, gold_run(1u, lines, 2u)));
    }

    std::vector<GoldComparison> comparisons;
    for (size_t one = 0u; one < named; one += 1u)
    {
        for (size_t two = one + 1u; two < named; two += 1u)
        {
            comparisons.push_back({corpora[one].name + " to " + corpora[two].name, (unsigned int)one,
                                   (unsigned int)two, 0ull});
        }
    }
    const size_t pairs = comparisons.size();
    for (size_t at = 0u; at < named; at += 1u)
    {
        const unsigned int first = (unsigned int)(named + (2u * at));
        comparisons.push_back({corpora[at].name + " midpoint", first, first + 1u, 0ull});
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        const unsigned int first = (unsigned int)((3u * named) + (2u * at));
        comparisons.push_back({corpora[at].name + " alternating", first, first + 1u, 0ull});
    }

    unsigned int most_count = 0u;
    unsigned long long most_total = 0ull;
    int whole = 1;
    for (size_t at = 0u; at < tables.size(); at += 1u)
    {
        for (unsigned int cell = 0u; cell < GOLD_CELLS; cell += 1u)
        {
            most_count = (tables[at].counts[cell] > most_count) ? tables[at].counts[cell] : most_count;
        }
        most_total = (tables[at].total > most_total) ? tables[at].total : most_total;
        whole = whole && (tables[at].total > 0ull);
    }
    const GoldDistanceRequest distance_request = {gold_bits(most_count), gold_bits(most_total)};
    if (!whole || (distance_request.total_bits > GOLD_TOTAL_BITS_MOST))
    {
        fprintf(stderr, "  gold_readings: a table is empty or holds more than 2^%u pairs\n", GOLD_TOTAL_BITS_MOST);
        return 2;
    }

    EngineError error;
    memset(&error, 0, sizeof(error));
    GoldStage distance_stage;
    memset(&distance_stage, 0, sizeof(distance_stage));
    distance_stage.name = "distance";
    gold_distance_program(&distance_request, &distance_stage.program);

    const unsigned long long lanes = (unsigned long long)comparisons.size() * GOLD_CELLS;
    const unsigned int in_limbs = (distance_stage.program.record_bits + 31u) / 32u;
    std::vector<unsigned int> records((size_t)(lanes * in_limbs), 0u);
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        for (unsigned int cell = 0u; cell < GOLD_CELLS; cell += 1u)
        {
            unsigned int *record = &records[(size_t)(((unsigned long long)at * GOLD_CELLS + cell) * in_limbs)];
            const unsigned int *bits = distance_stage.program.field_bits;
            const unsigned int *offset = distance_stage.program.field_offset;
            gold_put(record, offset[0], bits[0], left.counts[cell]);
            gold_put(record, offset[1], bits[1], right.counts[cell]);
            gold_put(record, offset[2], bits[2], left.total);
            gold_put(record, offset[3], bits[3], right.total);
        }
    }

    int ok = gold_lay(&job, &distance_stage, &error);
    const unsigned long long declared = lanes * (in_limbs + distance_stage.layout.out_limbs) * sizeof(unsigned int) +
                                        (256ull << 20u);
    ok = ok && sim_job_submit(&job, "gold_readings", count, arguments, declared);
    std::vector<unsigned int> distance_out;
    unsigned int *device_out = NULL;
    ok = ok && gold_sweep(&job, &distance_stage, records, lanes, &distance_out, &device_out, &error);

    // the sum over each comparison's run of cells, on the device and on the host
    const DeviceRecordStep *apart = ok ? &distance_stage.layout.step_table[distance_stage.program.output] : NULL;
    const unsigned int sum_limbs = ok ? ((apart->out_bits + gold_bits(GOLD_CELLS) + 31u) / 32u) + 1u : 0u;
    std::vector<unsigned int> device_sums(comparisons.size() * sum_limbs, 0u);
    std::vector<unsigned int> host_sums(comparisons.size() * sum_limbs, 0u);
    if (ok)
    {
        const CycleRecordSumRequest on_device = {device_out, lanes, GOLD_CELLS, distance_stage.layout.out_limbs,
                                                 apart->out_offset, apart->out_bits, sum_limbs, device_sums.data(),
                                                 &error};
        const CycleRecordSumRequest on_host = {distance_out.data(), lanes, GOLD_CELLS, distance_stage.layout.out_limbs,
                                               apart->out_offset, apart->out_bits, sum_limbs, host_sums.data(),
                                               &error};
        ok = (cycle_record_sum(&on_device) != CYCLE_ERROR) && (cycle_record_sum_host(&on_host) != CYCLE_ERROR);
        ok = ok && (memcmp(device_sums.data(), host_sums.data(), device_sums.size() * sizeof(unsigned int)) == 0);
    }
    sim_check(&job, ok, "the device's sum of each comparison equals the host's");
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }

    int bounded = ok;
    for (size_t at = 0u; ok && (at < comparisons.size()); at += 1u)
    {
        const unsigned int *sum = &device_sums[at * sum_limbs];
        for (unsigned int limb = 2u; limb < sum_limbs; limb += 1u)
        {
            bounded = bounded && (sum[limb] == 0u);
        }
        comparisons[at].numerator = (unsigned long long)sum[0] | ((unsigned long long)sum[1] << 32u);
        const unsigned long long denominator =
            2ull * tables[comparisons[at].left].total * tables[comparisons[at].right].total;
        bounded = bounded && (comparisons[at].numerator <= denominator);
    }
    sim_check(&job, bounded, "every distance lies in [0, 1]");

    // the verdicts: each pair against each corpus's split-half distance, at the midpoint and alternating
    GoldStage verdict_stage;
    memset(&verdict_stage, 0, sizeof(verdict_stage));
    verdict_stage.name = "verdict";
    const unsigned int denominator_bits = (2u * distance_request.total_bits) + 1u;
    const GoldVerdictRequest verdict_request = {denominator_bits, denominator_bits,
                                                gold_bits(margin[0] > margin[1] ? margin[0] : margin[1])};
    gold_verdict_program(&verdict_request, &verdict_stage.program);
    const unsigned int verdict_limbs = (verdict_stage.program.record_bits + 31u) / 32u;
    const unsigned long long verdict_lanes = (unsigned long long)pairs * 4ull;
    std::vector<unsigned int> verdict_records((size_t)(verdict_lanes * verdict_limbs), 0u);
    for (size_t at = 0u; at < pairs; at += 1u)
    {
        const GoldComparison &pair = comparisons[at];
        const unsigned long long pair_den = 2ull * tables[pair.left].total * tables[pair.right].total;
        for (unsigned int kind = 0u; kind < 2u; kind += 1u)
        {
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const unsigned int corpus = (side == 0u) ? pair.left : pair.right;
                const GoldComparison &half = comparisons[pairs + (kind * named) + corpus];
                const unsigned long long half_den = 2ull * tables[half.left].total * tables[half.right].total;
                unsigned int *record = &verdict_records[(size_t)(((at * 4u) + (kind * 2u) + side) * verdict_limbs)];
                const unsigned int *bits = verdict_stage.program.field_bits;
                const unsigned int *offset = verdict_stage.program.field_offset;
                gold_put(record, offset[0], bits[0], pair.numerator);
                gold_put(record, offset[1], bits[1], pair_den);
                gold_put(record, offset[2], bits[2], half.numerator);
                gold_put(record, offset[3], bits[3], half_den);
                gold_put(record, offset[4], bits[4], margin[0]);
                gold_put(record, offset[5], bits[5], margin[1]);
            }
        }
    }
    ok = ok && gold_lay(&job, &verdict_stage, &error);
    std::vector<unsigned int> verdict_out;
    unsigned int *verdict_device = NULL;
    ok = ok && gold_sweep(&job, &verdict_stage, verdict_records, verdict_lanes, &verdict_out, &verdict_device, &error);
    if (verdict_device != NULL)
    {
        cudaFree(verdict_device);
    }

    // each verdict lane's sign, read from its output
    std::vector<int> reads((size_t)verdict_lanes, 0);
    if (ok)
    {
        const DeviceRecordStep *sign = &verdict_stage.layout.step_table[verdict_stage.program.output];
        for (unsigned long long lane = 0ull; lane < verdict_lanes; lane += 1ull)
        {
            const unsigned int *record = &verdict_out[(size_t)(lane * verdict_stage.layout.out_limbs)];
            unsigned int word = 0u;
            for (unsigned int bit = 0u; bit < sign->out_bits; bit += 1u)
            {
                const unsigned int from = sign->out_offset + bit;
                word |= ((record[from / 32u] >> (from % 32u)) & 1u) << bit;
            }
            // two's complement over the output's width: the top bit set is -1, and otherwise the word is 0 or +1
            const int negative = (sign->out_bits > 0u) && (((word >> (sign->out_bits - 1u)) & 1u) != 0u);
            reads[(size_t)lane] = negative ? -1 : ((word == 0u) ? 0 : 1);
        }
    }

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "margin %llu/%llu\n", margin[0], margin[1]);
        for (size_t at = 0u; at < named; at += 1u)
        {
            fprintf(out, "corpus %s lines %zu pairs %llu\n", corpora[at].name.c_str(), corpora[at].lines.size(),
                    tables[at].total);
        }
        for (size_t at = 0u; at < comparisons.size(); at += 1u)
        {
            const unsigned long long left = tables[comparisons[at].left].total;
            const unsigned long long right = tables[comparisons[at].right].total;
            fprintf(out, "distance %s = %llu / (2 * %llu * %llu)\n", comparisons[at].name.c_str(),
                    comparisons[at].numerator, left, right);
        }
        for (size_t at = 0u; at < pairs; at += 1u)
        {
            fprintf(out, "verdict %s midpoint %d %d alternating %d %d\n", comparisons[at].name.c_str(),
                    reads[at * 4u], reads[(at * 4u) + 1u], reads[(at * 4u) + 2u], reads[(at * 4u) + 3u]);
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        printf("  %-16s %8s %10s  %-12s %-12s\n", "corpus", "lines", "pairs", "midpoint", "alternating");
        for (size_t at = 0u; at < named; at += 1u)
        {
            const GoldComparison &mid = comparisons[pairs + at];
            const GoldComparison &alt = comparisons[pairs + named + at];
            printf("  %-16s %8zu %10llu  %-12s %-12s\n", corpora[at].name.c_str(), corpora[at].lines.size(),
                   tables[at].total,
                   gold_digits(mid.numerator, 2ull * tables[mid.left].total * tables[mid.right].total, 6u).c_str(),
                   gold_digits(alt.numerator, 2ull * tables[alt.left].total * tables[alt.right].total, 6u).c_str());
        }
        printf("\n  %-34s %-10s %-9s %-9s\n", "pair", "distance", "midpoint", "alternating");
        unsigned int read_mid = 0u;
        unsigned int read_alt = 0u;
        for (size_t at = 0u; at < pairs; at += 1u)
        {
            const GoldComparison &pair = comparisons[at];
            const int mid = (reads[at * 4u] == 1) && (reads[(at * 4u) + 1u] == 1);
            const int alt = (reads[(at * 4u) + 2u] == 1) && (reads[(at * 4u) + 3u] == 1);
            read_mid += (unsigned int)mid;
            read_alt += (unsigned int)alt;
            printf("  %-34s %-10s %-9s %-9s\n", pair.name.c_str(),
                   gold_digits(pair.numerator, 2ull * tables[pair.left].total * tables[pair.right].total, 6u).c_str(),
                   mid ? "reads" : "does not", alt ? "reads" : "does not");
        }
        printf("\n  %u of %zu pairs read against the midpoint split-half distances, %u against the alternating\n",
               read_mid, pairs, read_alt);
        printf("  each decimal is the first six digits of the exact rational in the records, not a rounding\n");
    }

    gold_release(&distance_stage);
    gold_release(&verdict_stage);
    return sim_close(&job, "gold_readings");
}
