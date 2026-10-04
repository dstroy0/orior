// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The gold standard corpora read against each other and the papers read against them on the record machine, every
// distance an exact rational, and every verdict, order and median taken without a float.
//
//   Usage:  gold_readings <input> <output> [<figures>]
//
// The measure is orior.py's, sections 1 to 3. A text is its count of adjacent byte pairs, a_k in cell
// k = 256 b_i + b_(i+1) over each line's UTF-8 bytes, A of them in all. Two texts are apart by their total
// variation, which over the counts is the exact rational
//
//   D = sum over k of |a_k B - b_k A| / (2 A B).
//
// A text's split-half distance is D between its two halves, cut at the midpoint (lines [0, n/2) and [n/2, n)) and,
// for a corpus, alternating as well (the even lines and the odd). A text of fewer than two lines, or with a half
// holding no pair, has split-half distance 1, as self_distance gives it.
//
// What is read:
//   a pair of corpora reads where D clears the larger split-half distance of the two by the input's margin m;
//   a paper is read only where it holds at least the input's least count of pairs. Its distances to the language
//     corpora, English left out, are ordered with ties broken by name. The gap is the runner-up's distance less the
//     nearest's, and the paper reads where the gap clears m times its own midpoint split-half distance, as
//     language_check.py asks it;
//   the medians of the gaps and of the split-half distances over the papers held and not read, each the middle
//     value, or the mean of the middle two where the count is even.
//
// Eight programs of the record machine, each built by a request that carries only its field widths:
//   distance  one lane a cell of one comparison, a, b, A and B in, |a B - b A| and 2 A B out; the sum over each run
//             of a comparison's lanes is the numerator of its D;
//   ratio     x and y in, x / y out as (x_num y_den) / (x_den y_num);
//   compare   x, y and m in, the sign of x_num y_den m_den - y_num x_den m_num out, +1 where x > m y;
//   gap       nearest and runner-up in, (n2 d1 - n1 d2) / (d1 d2) out;
//   mean      two values in, (a d + c b) / (2 b d) out;
//   digits    a value in, its whole part and the first six digits after the point out, by quotient and remainder.
// The compare program runs three times, on the corpora, on the papers' gaps, and on the orders the medians need.
// Counting the pairs is the host's, and every product, difference, sum, quotient and comparison is the machine's.
//
// The input is what gold_readings.py writes: "margin <numerator> <denominator>", "least <pairs>", each corpus as
// "corpus <name> <lines>" and that many lines, then each paper as "paper <stem> <language> <lines>" and its lines.
//
// The output holds the records whole. The figures, where named, are the TeX macros the Salishan research paper reads.
//
// Checks:
//   1. each program imprints and lays out;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sum of each comparison's run equals the host's;
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

#include <algorithm>
#include <string>
#include <vector>

#define GOLD_CELLS 65536u
#define GOLD_STEPS_MOST 24u
#define GOLD_FIELDS_MOST 8u
#define GOLD_OUTPUTS_MOST 2u
#define GOLD_DIGITS 6u
#define GOLD_DIGITS_SCALE 1000000ull

// the widest total a field holds, so that 2 A B fits 64 bits
#define GOLD_TOTAL_BITS_MOST 31u

// a whole number as its magnitude's 32-bit limbs, least significant first
typedef std::vector<unsigned int> GoldWhole;

typedef struct
{
    GoldWhole num;
    GoldWhole den;
} GoldRational;

typedef struct
{
    std::string name;
    std::string language;
    std::vector<std::string> lines;
} GoldText;

// a text's pair counts over the 2^16 cells, and how many pairs there are in all
typedef struct
{
    std::vector<unsigned int> counts;
    unsigned long long total;
} GoldTable;

// two tables measured against each other, and the distance the distance program gives them
typedef struct
{
    std::string name;
    size_t left;
    size_t right;
    GoldRational distance;
} GoldComparison;

typedef struct
{
    EngineRecordStep steps[GOLD_STEPS_MOST];
    unsigned int count;
    unsigned int field_bits[GOLD_FIELDS_MOST];
    unsigned int field_offset[GOLD_FIELDS_MOST];
    unsigned int fields;
    unsigned int record_bits;
    unsigned int outputs[GOLD_OUTPUTS_MOST];
    unsigned int output_count;
} GoldProgram;

typedef struct
{
    const char *name;
    GoldProgram program;
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} GoldStage;

// a paper's reading
typedef struct
{
    size_t text;
    size_t whole;
    int held;
    std::vector<size_t> against;
    size_t own;
    std::vector<size_t> order;
    GoldRational floor;
    GoldRational gap;
    int reads;
} GoldPaper;

static GoldWhole gold_whole(unsigned long long value)
{
    GoldWhole whole;
    whole.push_back((unsigned int)(value & 0xFFFFFFFFull));
    whole.push_back((unsigned int)(value >> 32u));
    return whole;
}

static unsigned int gold_bits(const GoldWhole &value)
{
    for (size_t limb = value.size(); limb > 0u; limb -= 1u)
    {
        if (value[limb - 1u] != 0u)
        {
            unsigned int bits = 32u * (unsigned int)(limb - 1u);
            unsigned int word = value[limb - 1u];
            while (word != 0u)
            {
                bits += 1u;
                word >>= 1u;
            }
            return bits;
        }
    }
    return 1u;
}

// -1, 0 or +1 as the magnitude of one is below, equal to or above the other's, for the bounds check
static int gold_order_of(const GoldWhole &one, const GoldWhole &two)
{
    const size_t limbs = (one.size() > two.size()) ? one.size() : two.size();
    for (size_t limb = limbs; limb > 0u; limb -= 1u)
    {
        const unsigned int left = (limb - 1u < one.size()) ? one[limb - 1u] : 0u;
        const unsigned int right = (limb - 1u < two.size()) ? two[limb - 1u] : 0u;
        if (left != right)
        {
            return (left < right) ? -1 : 1;
        }
    }
    return 0;
}

static std::string gold_text_of(const GoldWhole &value)
{
    if (gold_bits(value) <= 64u)
    {
        const unsigned long long low = (value.size() > 0u) ? value[0] : 0u;
        const unsigned long long high = (value.size() > 1u) ? value[1] : 0u;
        return std::to_string(low | (high << 32u));
    }
    std::string text = "0x";
    char limb[16];
    int leading = 1;
    for (size_t at = value.size(); at > 0u; at -= 1u)
    {
        if (leading && (value[at - 1u] == 0u))
        {
            continue;
        }
        snprintf(limb, sizeof(limb), leading ? "%x" : "%08x", value[at - 1u]);
        text += limb;
        leading = 0;
    }
    return text;
}

static unsigned int gold_step(GoldProgram *program, EngineRecordOperation operation, unsigned int left,
                              unsigned int right)
{
    EngineRecordStep step = {operation, left, right, 0u};
    program->steps[program->count] = step;
    program->count += 1u;
    return program->count - 1u;
}

static unsigned int gold_constant(GoldProgram *program, unsigned long long value)
{
    return gold_step(program, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFull),
                     (unsigned int)(value >> 32u));
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

static void gold_output(GoldProgram *program, unsigned int step)
{
    program->outputs[program->output_count] = step;
    program->output_count += 1u;
}

// |a B - b A| and 2 A B for one cell of one comparison
static void gold_distance_program(unsigned int count_bits, unsigned int total_bits, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = gold_field(program, count_bits);
    const unsigned int b = gold_field(program, count_bits);
    const unsigned int big_a = gold_field(program, total_bits);
    const unsigned int big_b = gold_field(program, total_bits);
    const unsigned int a_by_b = gold_step(program, ENGINE_RECORD_PRODUCT, a, big_b);
    const unsigned int b_by_a = gold_step(program, ENGINE_RECORD_PRODUCT, b, big_a);
    const unsigned int apart = gold_step(program, ENGINE_RECORD_DIFFERENCE, a_by_b, b_by_a);
    gold_output(program, gold_step(program, ENGINE_RECORD_ABSOLUTE, apart, 0u));
    const unsigned int both = gold_step(program, ENGINE_RECORD_PRODUCT, big_a, big_b);
    gold_output(program, gold_step(program, ENGINE_RECORD_PRODUCT, both, gold_constant(program, 2ull)));
}

// x / y as (x_num y_den) / (x_den y_num)
static void gold_ratio_program(unsigned int bits, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int x_num = gold_field(program, bits);
    const unsigned int x_den = gold_field(program, bits);
    const unsigned int y_num = gold_field(program, bits);
    const unsigned int y_den = gold_field(program, bits);
    gold_output(program, gold_step(program, ENGINE_RECORD_PRODUCT, x_num, y_den));
    gold_output(program, gold_step(program, ENGINE_RECORD_PRODUCT, x_den, y_num));
}

// the sign of x_num y_den m_den - y_num x_den m_num, +1 where x clears m times y
static void gold_compare_program(unsigned int bits, unsigned int margin_bits, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int x_num = gold_field(program, bits);
    const unsigned int x_den = gold_field(program, bits);
    const unsigned int y_num = gold_field(program, bits);
    const unsigned int y_den = gold_field(program, bits);
    const unsigned int m_num = gold_field(program, margin_bits);
    const unsigned int m_den = gold_field(program, margin_bits);
    const unsigned int x_side = gold_step(program, ENGINE_RECORD_PRODUCT, x_num, y_den);
    const unsigned int x_whole = gold_step(program, ENGINE_RECORD_PRODUCT, x_side, m_den);
    const unsigned int y_side = gold_step(program, ENGINE_RECORD_PRODUCT, y_num, x_den);
    const unsigned int y_whole = gold_step(program, ENGINE_RECORD_PRODUCT, y_side, m_num);
    gold_output(program, gold_step(program, ENGINE_RECORD_COMPARE, x_whole, y_whole));
}

// the runner-up's distance less the nearest's, (n2 d1 - n1 d2) / (d1 d2)
static void gold_gap_program(unsigned int bits, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int n1 = gold_field(program, bits);
    const unsigned int d1 = gold_field(program, bits);
    const unsigned int n2 = gold_field(program, bits);
    const unsigned int d2 = gold_field(program, bits);
    const unsigned int second = gold_step(program, ENGINE_RECORD_PRODUCT, n2, d1);
    const unsigned int first = gold_step(program, ENGINE_RECORD_PRODUCT, n1, d2);
    gold_output(program, gold_step(program, ENGINE_RECORD_DIFFERENCE, second, first));
    gold_output(program, gold_step(program, ENGINE_RECORD_PRODUCT, d1, d2));
}

// the mean of a / b and c / d, (a d + c b) / (2 b d)
static void gold_mean_program(unsigned int bits, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = gold_field(program, bits);
    const unsigned int b = gold_field(program, bits);
    const unsigned int c = gold_field(program, bits);
    const unsigned int d = gold_field(program, bits);
    const unsigned int left = gold_step(program, ENGINE_RECORD_PRODUCT, a, d);
    const unsigned int right = gold_step(program, ENGINE_RECORD_PRODUCT, c, b);
    gold_output(program, gold_step(program, ENGINE_RECORD_SUM, left, right));
    const unsigned int both = gold_step(program, ENGINE_RECORD_PRODUCT, b, d);
    gold_output(program, gold_step(program, ENGINE_RECORD_PRODUCT, both, gold_constant(program, 2ull)));
}

// the whole part of num / den and the first GOLD_DIGITS digits after the point
static void gold_digits_program(unsigned int bits, GoldProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int num = gold_field(program, bits);
    const unsigned int den = gold_field(program, bits);
    gold_output(program, gold_step(program, ENGINE_RECORD_QUOTIENT, num, den));
    const unsigned int rest = gold_step(program, ENGINE_RECORD_REMAINDER, num, den);
    const unsigned int scaled = gold_step(program, ENGINE_RECORD_PRODUCT, rest, gold_constant(program, GOLD_DIGITS_SCALE));
    gold_output(program, gold_step(program, ENGINE_RECORD_QUOTIENT, scaled, den));
}

// `bits` of a whole number laid into a record at `offset`
static void gold_put(unsigned int *record, unsigned int offset, unsigned int bits, const GoldWhole &value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int to = offset + bit;
        const unsigned int one = ((bit / 32u) < value.size()) ? ((value[bit / 32u] >> (bit % 32u)) & 1u) : 0u;
        record[to / 32u] = (record[to / 32u] & ~(1u << (to % 32u))) | (one << (to % 32u));
    }
}

// an output of a lane's record, as limbs of its two's complement
static GoldWhole gold_take(const unsigned int *record, unsigned int offset, unsigned int bits)
{
    GoldWhole value((bits + 31u) / 32u, 0u);
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int from = offset + bit;
        value[bit / 32u] |= ((record[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    return value;
}

static int gold_sign(const GoldWhole &value, unsigned int bits)
{
    const int negative = ((value[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u) != 0u;
    int zero = 1;
    for (size_t at = 0u; at < value.size(); at += 1u)
    {
        zero = zero && (value[at] == 0u);
    }
    return negative ? -1 : (zero ? 0 : 1);
}

// the pairs of the lines `pick` lists, each line's bytes read alone
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

static int gold_block(const std::vector<std::string> &lines, size_t *at, const char *kind, int with_language,
                      GoldText *text)
{
    const std::string &head = lines[*at];
    const std::string opening = std::string(kind) + " ";
    if (head.compare(0u, opening.size(), opening) != 0)
    {
        return 0;
    }
    const size_t last = head.rfind(' ');
    if (last <= opening.size())
    {
        return 0;
    }
    std::string named = head.substr(opening.size(), last - opening.size());
    if (with_language)
    {
        const size_t space = named.find(' ');
        if (space == std::string::npos)
        {
            return 0;
        }
        text->language = named.substr(space + 1u);
        named = named.substr(0u, space);
    }
    text->name = named;
    const unsigned long long count = strtoull(head.c_str() + last + 1u, NULL, 10);
    *at += 1u;
    if ((*at + count) > lines.size())
    {
        return 0;
    }
    text->lines.assign(lines.begin() + (long long)*at, lines.begin() + (long long)(*at + count));
    *at += (size_t)count;
    return 1;
}

static int gold_read(const char *path, std::vector<GoldText> *corpora, std::vector<GoldText> *papers,
                     unsigned long long margin[2], unsigned long long *least)
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
    if ((lines.size() < 2u) || (sscanf(lines[0].c_str(), "margin %llu %llu", &margin[0], &margin[1]) != 2) ||
        (margin[0] == 0ull) || (margin[1] == 0ull) || (sscanf(lines[1].c_str(), "least %llu", least) != 1))
    {
        return 0;
    }
    size_t at = 2u;
    while (at < lines.size())
    {
        GoldText one;
        if (lines[at].compare(0u, 7u, "corpus ") == 0)
        {
            if (!gold_block(lines, &at, "corpus", 0, &one))
            {
                return 0;
            }
            corpora->push_back(one);
        }
        else if (lines[at].compare(0u, 6u, "paper ") == 0)
        {
            if (!gold_block(lines, &at, "paper", 1, &one))
            {
                return 0;
            }
            papers->push_back(one);
        }
        else
        {
            return 0;
        }
    }
    return corpora->size() >= 2u;
}

// imprints and lays out a stage's program over one member's records
static int gold_lay(SimResults *job, GoldStage *stage, EngineError *error)
{
    const GoldProgram *program = &stage->program;
    const KeymathRecordRequest encode = {program->steps, program->count, program->field_bits, program->fields,
                                         1u, program->outputs, program->output_count, NULL, 0u, &stage->key, error};
    int ok = keymath_record_encode(&encode) != KEYMATH_ERROR;
    sim_check(job, ok, (std::string("keymath imprints the ") + stage->name + " program").c_str());
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {(program->record_bits + 31u) / 32u, 0u, 0u};
    const KeyScheduleRecordRequest lay = {&stage->key, program->field_offset, program->fields, in_limbs, 1,
                                          &stage->layout, error};
    ok = ok && (key_schedule_record_layout(&lay) != KEY_SCHEDULE_ERROR);
    sim_check(job, ok, (std::string("the scheduler lays the ") + stage->name + " program out").c_str());
    return ok;
}

// runs every lane on the device and on the host and holds the two word for word. The device's records come back in
// `out`, and stay on the device at `device_out` for the caller to sum and free
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

// one lane a row of whole numbers, every field `bits` wide
typedef std::vector<std::vector<GoldWhole>> GoldRows;

// lays a stage out over rows, sweeps it, and returns each lane's outputs
static int gold_rows_stage(SimResults *job, GoldStage *stage, const GoldRows &rows,
                           std::vector<std::vector<GoldWhole>> *outputs, EngineError *error)
{
    outputs->clear();
    if (rows.empty())
    {
        return 1;
    }
    int ok = gold_lay(job, stage, error);
    const unsigned int limbs = (stage->program.record_bits + 31u) / 32u;
    std::vector<unsigned int> records(rows.size() * limbs, 0u);
    for (size_t lane = 0u; lane < rows.size(); lane += 1u)
    {
        for (unsigned int field = 0u; field < stage->program.fields; field += 1u)
        {
            gold_put(&records[lane * limbs], stage->program.field_offset[field], stage->program.field_bits[field],
                     rows[lane][field]);
        }
    }
    std::vector<unsigned int> out;
    unsigned int *device_out = NULL;
    ok = ok && gold_sweep(job, stage, records, rows.size(), &out, &device_out, error);
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }
    for (size_t lane = 0u; ok && (lane < rows.size()); lane += 1u)
    {
        std::vector<GoldWhole> lane_out;
        for (unsigned int at = 0u; at < stage->program.output_count; at += 1u)
        {
            const DeviceRecordStep *step = &stage->layout.step_table[stage->program.outputs[at]];
            lane_out.push_back(gold_take(&out[lane * stage->layout.out_limbs], step->out_offset, step->out_bits));
        }
        outputs->push_back(lane_out);
    }
    return ok;
}

static unsigned int gold_widest(const std::vector<GoldRational> &values)
{
    unsigned int bits = 1u;
    for (size_t at = 0u; at < values.size(); at += 1u)
    {
        bits = std::max(bits, std::max(gold_bits(values[at].num), gold_bits(values[at].den)));
    }
    return bits;
}

// the comparison lanes an order of values needs, every pair once, and the order the machine's signs give, least
// first, ties broken by `names` where given and by place otherwise
typedef struct
{
    std::vector<GoldRational> values;
    std::vector<std::string> names;
    size_t first_lane;
    std::vector<size_t> sorted;
} GoldOrder;

static void gold_order_lanes(GoldOrder *order, std::vector<GoldRational> *xs, std::vector<GoldRational> *ys)
{
    order->first_lane = xs->size();
    for (size_t one = 0u; one < order->values.size(); one += 1u)
    {
        for (size_t two = one + 1u; two < order->values.size(); two += 1u)
        {
            xs->push_back(order->values[one]);
            ys->push_back(order->values[two]);
        }
    }
}

static void gold_order_read(GoldOrder *order, const std::vector<int> &signs)
{
    const size_t count = order->values.size();
    std::vector<int> table(count * count, 0);
    size_t lane = order->first_lane;
    for (size_t one = 0u; one < count; one += 1u)
    {
        for (size_t two = one + 1u; two < count; two += 1u)
        {
            table[one * count + two] = signs[lane];
            table[two * count + one] = -signs[lane];
            lane += 1u;
        }
    }
    order->sorted = gold_run(0u, count, 1u);
    std::stable_sort(order->sorted.begin(), order->sorted.end(), [&](size_t one, size_t two) {
        const int sign = table[one * count + two];
        if (sign != 0)
        {
            return sign < 0;
        }
        if (!order->names.empty())
        {
            return order->names[one] < order->names[two];
        }
        return one < two;
    });
}

// runs the compare program over x and y lanes at margin m, and returns each lane's sign
static int gold_compare(SimResults *job, GoldStage *stage, const char *name, const std::vector<GoldRational> &xs,
                        const std::vector<GoldRational> &ys, const std::vector<int> &margined,
                        const unsigned long long margin[2], std::vector<int> *signs, EngineError *error)
{
    signs->clear();
    if (xs.empty())
    {
        return 1;
    }
    memset(stage, 0, sizeof(*stage));
    stage->name = name;
    const GoldWhole m_num = gold_whole(margin[0]);
    const GoldWhole m_den = gold_whole(margin[1]);
    const GoldWhole one = gold_whole(1ull);
    const unsigned int bits = std::max(gold_widest(xs), gold_widest(ys));
    gold_compare_program(bits, std::max(gold_bits(m_num), gold_bits(m_den)), &stage->program);
    GoldRows rows;
    for (size_t lane = 0u; lane < xs.size(); lane += 1u)
    {
        const int with = margined[lane];
        rows.push_back({xs[lane].num, xs[lane].den, ys[lane].num, ys[lane].den, with ? m_num : one,
                        with ? m_den : one});
    }
    std::vector<std::vector<GoldWhole>> outputs;
    const int ok = gold_rows_stage(job, stage, rows, &outputs, error);
    const DeviceRecordStep *step = ok ? &stage->layout.step_table[stage->program.outputs[0]] : NULL;
    for (size_t lane = 0u; ok && (lane < outputs.size()); lane += 1u)
    {
        signs->push_back(gold_sign(outputs[lane][0], step->out_bits));
    }
    return ok;
}

static GoldRational gold_unit(void)
{
    return {gold_whole(1ull), gold_whole(1ull)};
}

static GoldRational gold_pair_of(const std::vector<GoldWhole> &out)
{
    return {out[0], out[1]};
}

int main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if ((count != 3) && (count != 4))
    {
        fprintf(stderr, "  usage: gold_readings <input> <output> [<figures>]\n");
        return 2;
    }
    std::vector<GoldText> corpora;
    std::vector<GoldText> texts;
    unsigned long long margin[2] = {0ull, 0ull};
    unsigned long long least = 0ull;
    if (!gold_read(arguments[1], &corpora, &texts, margin, &least))
    {
        fprintf(stderr, "  gold_readings: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }
    const unsigned long long unit_margin[2] = {1ull, 1ull};

    // the tables: each corpus whole, its midpoint halves and its alternating halves, then each paper whole and its
    // midpoint halves
    const size_t named = corpora.size();
    std::vector<GoldTable> tables;
    for (size_t at = 0u; at < named; at += 1u)
    {
        tables.push_back(gold_squash(corpora[at].lines, gold_run(0u, corpora[at].lines.size(), 1u)));
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
    std::vector<size_t> anchors;
    for (size_t at = 0u; at < named; at += 1u)
    {
        if (corpora[at].name != "English")
        {
            anchors.push_back(at);
        }
    }

    std::vector<GoldComparison> comparisons;
    for (size_t one = 0u; one < named; one += 1u)
    {
        for (size_t two = one + 1u; two < named; two += 1u)
        {
            comparisons.push_back({corpora[one].name + " to " + corpora[two].name, one, two, {}});
        }
    }
    const size_t pairs = comparisons.size();
    for (size_t at = 0u; at < named; at += 1u)
    {
        comparisons.push_back({corpora[at].name + " midpoint", named + (2u * at), named + (2u * at) + 1u, {}});
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        comparisons.push_back(
            {corpora[at].name + " alternating", (3u * named) + (2u * at), (3u * named) + (2u * at) + 1u, {}});
    }

    std::vector<GoldPaper> papers;
    for (size_t at = 0u; at < texts.size(); at += 1u)
    {
        GoldPaper paper;
        paper.text = at;
        paper.whole = tables.size();
        paper.own = (size_t)-1;
        paper.reads = 0;
        const size_t lines = texts[at].lines.size();
        tables.push_back(gold_squash(texts[at].lines, gold_run(0u, lines, 1u)));
        paper.held = tables.back().total >= least;
        if (paper.held)
        {
            for (size_t one = 0u; one < anchors.size(); one += 1u)
            {
                paper.against.push_back(comparisons.size());
                comparisons.push_back({texts[at].name + " to " + corpora[anchors[one]].name, paper.whole,
                                       anchors[one], {}});
            }
            const size_t first = tables.size();
            tables.push_back(gold_squash(texts[at].lines, gold_run(0u, lines / 2u, 1u)));
            tables.push_back(gold_squash(texts[at].lines, gold_run(lines / 2u, lines, 1u)));
            if ((lines >= 2u) && (tables[first].total > 0ull) && (tables[first + 1u].total > 0ull))
            {
                paper.own = comparisons.size();
                comparisons.push_back({texts[at].name + " midpoint", first, first + 1u, {}});
            }
        }
        papers.push_back(paper);
    }

    unsigned int most_count = 0u;
    unsigned long long most_total = 0ull;
    size_t run = 1u;
    int whole = 1;
    std::vector<std::vector<unsigned int>> cells(comparisons.size());
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        for (unsigned int cell = 0u; cell < GOLD_CELLS; cell += 1u)
        {
            if ((left.counts[cell] != 0u) || (right.counts[cell] != 0u))
            {
                cells[at].push_back(cell);
                most_count = std::max(most_count, std::max(left.counts[cell], right.counts[cell]));
            }
        }
        run = std::max(run, cells[at].size());
        most_total = std::max(most_total, std::max(left.total, right.total));
        whole = whole && (left.total > 0ull) && (right.total > 0ull);
    }
    const unsigned int count_bits = gold_bits(gold_whole(most_count));
    const unsigned int total_bits = gold_bits(gold_whole(most_total));
    if (!whole || (total_bits > GOLD_TOTAL_BITS_MOST))
    {
        fprintf(stderr, "  gold_readings: a table is empty or holds more than 2^%u pairs\n", GOLD_TOTAL_BITS_MOST);
        return 2;
    }

    // the distance program: each comparison a run of `run` lanes, its occupied cells first and zeros after
    EngineError error;
    memset(&error, 0, sizeof(error));
    GoldStage distance_stage;
    memset(&distance_stage, 0, sizeof(distance_stage));
    distance_stage.name = "distance";
    gold_distance_program(count_bits, total_bits, &distance_stage.program);
    const unsigned long long lanes = (unsigned long long)comparisons.size() * run;
    const unsigned int in_limbs = (distance_stage.program.record_bits + 31u) / 32u;
    std::vector<unsigned int> records((size_t)(lanes * in_limbs), 0u);
    const unsigned int *field_bits = distance_stage.program.field_bits;
    const unsigned int *field_offset = distance_stage.program.field_offset;
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        for (size_t place = 0u; place < run; place += 1u)
        {
            unsigned int *record = &records[(size_t)(((unsigned long long)at * run + place) * in_limbs)];
            if (place < cells[at].size())
            {
                gold_put(record, field_offset[0], field_bits[0], gold_whole(left.counts[cells[at][place]]));
                gold_put(record, field_offset[1], field_bits[1], gold_whole(right.counts[cells[at][place]]));
            }
            gold_put(record, field_offset[2], field_bits[2], gold_whole(left.total));
            gold_put(record, field_offset[3], field_bits[3], gold_whole(right.total));
        }
    }

    int ok = gold_lay(&job, &distance_stage, &error);
    // the buffers the program names for itself: the distance stage's records in and out, the most it holds at once
    const unsigned long long declared =
        lanes * ((unsigned long long)in_limbs + distance_stage.layout.out_limbs) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "gold_readings", count, arguments, declared);
    std::vector<unsigned int> distance_out;
    unsigned int *device_out = NULL;
    ok = ok && gold_sweep(&job, &distance_stage, records, lanes, &distance_out, &device_out, &error);
    records.clear();
    records.shrink_to_fit();

    const DeviceRecordStep *apart = ok ? &distance_stage.layout.step_table[distance_stage.program.outputs[0]] : NULL;
    const DeviceRecordStep *twice = ok ? &distance_stage.layout.step_table[distance_stage.program.outputs[1]] : NULL;
    const unsigned int sum_limbs = ok ? ((apart->out_bits + gold_bits(gold_whole(run)) + 31u) / 32u) + 1u : 0u;
    std::vector<unsigned int> device_sums(comparisons.size() * sum_limbs, 0u);
    std::vector<unsigned int> host_sums(comparisons.size() * sum_limbs, 0u);
    if (ok)
    {
        const unsigned int out_limbs = distance_stage.layout.out_limbs;
        const CycleRecordSumRequest on_device = {device_out, lanes, run, out_limbs, apart->out_offset, apart->out_bits,
                                                 sum_limbs, device_sums.data(), &error};
        const CycleRecordSumRequest on_host = {distance_out.data(), lanes, run, out_limbs, apart->out_offset,
                                               apart->out_bits, sum_limbs, host_sums.data(), &error};
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
        comparisons[at].distance.num.assign(device_sums.begin() + (long long)(at * sum_limbs),
                                            device_sums.begin() + (long long)((at + 1u) * sum_limbs));
        const unsigned int *first = &distance_out[(size_t)((unsigned long long)at * run * distance_stage.layout.out_limbs)];
        comparisons[at].distance.den = gold_take(first, twice->out_offset, twice->out_bits);
        bounded = bounded && (gold_sign(comparisons[at].distance.num, 32u * sum_limbs) >= 0) &&
                  (gold_order_of(comparisons[at].distance.num, comparisons[at].distance.den) <= 0);
    }
    sim_check(&job, bounded, "every distance lies in [0, 1]");
    distance_out.clear();
    distance_out.shrink_to_fit();
    ok = ok && bounded;

    // the ratio of each corpus's midpoint split-half distance to its alternating one
    GoldStage ratio_stage;
    memset(&ratio_stage, 0, sizeof(ratio_stage));
    ratio_stage.name = "ratio";
    std::vector<GoldRational> halves;
    for (size_t at = 0u; at < (2u * named); at += 1u)
    {
        halves.push_back(comparisons[pairs + at].distance);
    }
    gold_ratio_program(gold_widest(halves), &ratio_stage.program);
    GoldRows ratio_rows;
    for (size_t at = 0u; at < named; at += 1u)
    {
        const GoldRational &mid = comparisons[pairs + at].distance;
        const GoldRational &alt = comparisons[pairs + named + at].distance;
        ratio_rows.push_back({mid.num, mid.den, alt.num, alt.den});
    }
    std::vector<std::vector<GoldWhole>> ratio_out;
    ok = ok && gold_rows_stage(&job, &ratio_stage, ratio_rows, &ratio_out, &error);
    std::vector<GoldRational> ratios;
    for (size_t at = 0u; ok && (at < named); at += 1u)
    {
        ratios.push_back(gold_pair_of(ratio_out[at]));
    }

    // the first compare: each pair against each split-half distance, the orders the ranges need, and each paper's
    // distances to the language corpora
    std::vector<GoldRational> xs;
    std::vector<GoldRational> ys;
    std::vector<int> margined;
    for (size_t at = 0u; at < pairs; at += 1u)
    {
        for (size_t kind = 0u; kind < 2u; kind += 1u)
        {
            const size_t sides[2] = {comparisons[at].left, comparisons[at].right};
            for (size_t side = 0u; side < 2u; side += 1u)
            {
                xs.push_back(comparisons[at].distance);
                ys.push_back(comparisons[pairs + (kind * named) + sides[side]].distance);
                margined.push_back(1);
            }
        }
    }
    GoldOrder pair_order;
    GoldOrder midpoint_order;
    GoldOrder alternating_order;
    GoldOrder ratio_order;
    for (size_t at = 0u; at < pairs; at += 1u)
    {
        pair_order.values.push_back(comparisons[at].distance);
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        midpoint_order.values.push_back(comparisons[pairs + at].distance);
        alternating_order.values.push_back(comparisons[pairs + named + at].distance);
    }
    ratio_order.values = ratios;
    GoldOrder *orders[4] = {&pair_order, &midpoint_order, &alternating_order, &ratio_order};
    for (size_t at = 0u; at < 4u; at += 1u)
    {
        gold_order_lanes(orders[at], &xs, &ys);
    }
    std::vector<GoldOrder> paper_orders(papers.size());
    for (size_t at = 0u; at < papers.size(); at += 1u)
    {
        if (!papers[at].held)
        {
            continue;
        }
        for (size_t one = 0u; one < anchors.size(); one += 1u)
        {
            paper_orders[at].values.push_back(comparisons[papers[at].against[one]].distance);
            paper_orders[at].names.push_back(corpora[anchors[one]].name);
        }
        gold_order_lanes(&paper_orders[at], &xs, &ys);
    }
    margined.resize(xs.size(), 0);
    GoldStage first_stage;
    memset(&first_stage, 0, sizeof(first_stage));
    std::vector<int> first_signs;
    ok = ok && gold_compare(&job, &first_stage, "first compare", xs, ys, margined, margin, &first_signs, &error);
    for (size_t at = 0u; ok && (at < 4u); at += 1u)
    {
        gold_order_read(orders[at], first_signs);
    }
    for (size_t at = 0u; ok && (at < papers.size()); at += 1u)
    {
        if (papers[at].held)
        {
            gold_order_read(&paper_orders[at], first_signs);
            for (size_t one = 0u; one < paper_orders[at].sorted.size(); one += 1u)
            {
                papers[at].order.push_back(anchors[paper_orders[at].sorted[one]]);
            }
        }
    }

    // each held paper's gap between its nearest corpus and the runner-up
    GoldStage gap_stage;
    memset(&gap_stage, 0, sizeof(gap_stage));
    gap_stage.name = "gap";
    std::vector<GoldRational> near;
    GoldRows gap_rows;
    std::vector<size_t> gapped;
    for (size_t at = 0u; ok && (at < papers.size()); at += 1u)
    {
        if (papers[at].held && (papers[at].order.size() >= 2u))
        {
            const GoldRational &one = comparisons[papers[at].against[paper_orders[at].sorted[0]]].distance;
            const GoldRational &two = comparisons[papers[at].against[paper_orders[at].sorted[1]]].distance;
            near.push_back(one);
            near.push_back(two);
            gap_rows.push_back({one.num, one.den, two.num, two.den});
            gapped.push_back(at);
        }
    }
    gold_gap_program(gold_widest(near), &gap_stage.program);
    std::vector<std::vector<GoldWhole>> gap_out;
    ok = ok && gold_rows_stage(&job, &gap_stage, gap_rows, &gap_out, &error);
    for (size_t at = 0u; ok && (at < gapped.size()); at += 1u)
    {
        GoldPaper &paper = papers[gapped[at]];
        paper.gap = gold_pair_of(gap_out[at]);
        paper.floor = (paper.own != (size_t)-1) ? comparisons[paper.own].distance : gold_unit();
    }

    // the second compare: each gap against the margin times its paper's split-half distance
    std::vector<GoldRational> gap_xs;
    std::vector<GoldRational> gap_ys;
    for (size_t at = 0u; at < gapped.size(); at += 1u)
    {
        gap_xs.push_back(papers[gapped[at]].gap);
        gap_ys.push_back(papers[gapped[at]].floor);
    }
    GoldStage second_stage;
    memset(&second_stage, 0, sizeof(second_stage));
    std::vector<int> second_signs;
    ok = ok && gold_compare(&job, &second_stage, "second compare", gap_xs, gap_ys, std::vector<int>(gap_xs.size(), 1),
                            margin, &second_signs, &error);
    for (size_t at = 0u; ok && (at < gapped.size()); at += 1u)
    {
        papers[gapped[at]].reads = second_signs[at] == 1;
    }

    // the third compare: the orders of the gaps and of the split-half distances over the papers held and not read
    GoldOrder gap_order;
    GoldOrder floor_order;
    for (size_t at = 0u; at < gapped.size(); at += 1u)
    {
        if (!papers[gapped[at]].reads)
        {
            gap_order.values.push_back(papers[gapped[at]].gap);
            floor_order.values.push_back(papers[gapped[at]].floor);
        }
    }
    std::vector<GoldRational> third_xs;
    std::vector<GoldRational> third_ys;
    gold_order_lanes(&gap_order, &third_xs, &third_ys);
    gold_order_lanes(&floor_order, &third_xs, &third_ys);
    GoldStage third_stage;
    memset(&third_stage, 0, sizeof(third_stage));
    std::vector<int> third_signs;
    ok = ok && gold_compare(&job, &third_stage, "third compare", third_xs, third_ys,
                            std::vector<int>(third_xs.size(), 0), unit_margin, &third_signs, &error);
    if (ok)
    {
        gold_order_read(&gap_order, third_signs);
        gold_order_read(&floor_order, third_signs);
    }

    // the medians, a middle value or the mean of the middle two
    GoldStage mean_stage;
    memset(&mean_stage, 0, sizeof(mean_stage));
    mean_stage.name = "mean";
    GoldOrder *medians_of[2] = {&gap_order, &floor_order};
    GoldRational medians[2] = {gold_unit(), gold_unit()};
    int median_held[2] = {0, 0};
    GoldRows mean_rows;
    std::vector<size_t> mean_for;
    std::vector<GoldRational> mean_values;
    for (size_t at = 0u; ok && (at < 2u); at += 1u)
    {
        const size_t held = medians_of[at]->sorted.size();
        if (held == 0u)
        {
            continue;
        }
        median_held[at] = 1;
        const GoldRational &upper = medians_of[at]->values[medians_of[at]->sorted[held / 2u]];
        if ((held % 2u) == 1u)
        {
            medians[at] = upper;
            continue;
        }
        const GoldRational &lower = medians_of[at]->values[medians_of[at]->sorted[(held / 2u) - 1u]];
        mean_rows.push_back({lower.num, lower.den, upper.num, upper.den});
        mean_values.push_back(lower);
        mean_values.push_back(upper);
        mean_for.push_back(at);
    }
    gold_mean_program(gold_widest(mean_values), &mean_stage.program);
    std::vector<std::vector<GoldWhole>> mean_out;
    ok = ok && gold_rows_stage(&job, &mean_stage, mean_rows, &mean_out, &error);
    for (size_t at = 0u; ok && (at < mean_for.size()); at += 1u)
    {
        medians[mean_for[at]] = gold_pair_of(mean_out[at]);
    }

    // every value printed, as its whole part and six digits
    std::vector<GoldRational> printed;
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        printed.push_back(comparisons[at].distance);
    }
    const size_t ratio_at = printed.size();
    printed.insert(printed.end(), ratios.begin(), ratios.end());

    for (size_t at = 0u; at < gapped.size(); at += 1u)
    {
        printed.push_back(papers[gapped[at]].gap);
        printed.push_back(papers[gapped[at]].floor);
    }
    const size_t median_at = printed.size();
    printed.push_back(medians[0]);
    printed.push_back(medians[1]);

    // how many times the median gap fits inside the median split-half distance
    GoldStage noise_stage;
    memset(&noise_stage, 0, sizeof(noise_stage));
    noise_stage.name = "noise ratio";
    std::vector<GoldRational> noise_values = {medians[0], medians[1]};
    gold_ratio_program(gold_widest(noise_values), &noise_stage.program);
    GoldRows noise_rows;
    if (median_held[0] && median_held[1] && (gold_sign(medians[0].num, 32u * (unsigned int)medians[0].num.size()) > 0))
    {
        noise_rows.push_back({medians[1].num, medians[1].den, medians[0].num, medians[0].den});
    }
    std::vector<std::vector<GoldWhole>> noise_out;
    ok = ok && gold_rows_stage(&job, &noise_stage, noise_rows, &noise_out, &error);
    const size_t noise_at = printed.size();
    if (ok && !noise_out.empty())
    {
        printed.push_back(gold_pair_of(noise_out[0]));
    }
    GoldStage digits_stage;
    memset(&digits_stage, 0, sizeof(digits_stage));
    digits_stage.name = "digits";
    gold_digits_program(gold_widest(printed), &digits_stage.program);
    GoldRows digit_rows;
    for (size_t at = 0u; at < printed.size(); at += 1u)
    {
        digit_rows.push_back({printed[at].num, printed[at].den});
    }
    std::vector<std::vector<GoldWhole>> digit_out;
    ok = ok && gold_rows_stage(&job, &digits_stage, digit_rows, &digit_out, &error);
    std::vector<std::string> decimal(printed.size());
    for (size_t at = 0u; ok && (at < printed.size()); at += 1u)
    {
        // the six digits are below 10^6, which the first limb holds
        char fraction[32];
        snprintf(fraction, sizeof(fraction), "%06u", digit_out[at][1][0]);
        decimal[at] = gold_text_of(digit_out[at][0]) + "." + fraction;
    }

    // the counts the readings come to
    unsigned int pairs_read[2] = {0u, 0u};
    std::vector<int> pair_reads(pairs * 2u, 0);
    for (size_t at = 0u; ok && (at < pairs); at += 1u)
    {
        for (size_t kind = 0u; kind < 2u; kind += 1u)
        {
            const int reads = (first_signs[(at * 4u) + (kind * 2u)] == 1) &&
                              (first_signs[(at * 4u) + (kind * 2u) + 1u] == 1);
            pair_reads[(at * 2u) + kind] = reads;
            pairs_read[kind] += (unsigned int)reads;
        }
    }
    unsigned int small = 0u;
    unsigned int read = 0u;
    unsigned int agreed = 0u;
    for (size_t at = 0u; at < papers.size(); at += 1u)
    {
        small += (unsigned int)!papers[at].held;
        if (papers[at].reads)
        {
            read += 1u;
            agreed += (unsigned int)(corpora[papers[at].order[0]].name == texts[papers[at].text].language);
        }
    }
    const unsigned int unread = (unsigned int)papers.size() - small - read;

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "margin %llu/%llu least %llu\n", margin[0], margin[1], least);
        for (size_t at = 0u; at < named; at += 1u)
        {
            fprintf(out, "corpus %s lines %zu pairs %llu\n", corpora[at].name.c_str(), corpora[at].lines.size(),
                    tables[at].total);
        }
        for (size_t at = 0u; at < comparisons.size(); at += 1u)
        {
            fprintf(out, "distance %s = %s / %s\n", comparisons[at].name.c_str(),
                    gold_text_of(comparisons[at].distance.num).c_str(),
                    gold_text_of(comparisons[at].distance.den).c_str());
        }
        for (size_t at = 0u; at < named; at += 1u)
        {
            fprintf(out, "ratio %s midpoint to alternating = %s / %s\n", corpora[at].name.c_str(),
                    gold_text_of(ratios[at].num).c_str(), gold_text_of(ratios[at].den).c_str());
        }
        for (size_t at = 0u; at < pairs; at += 1u)
        {
            fprintf(out, "verdict %s midpoint %d %d alternating %d %d\n", comparisons[at].name.c_str(),
                    first_signs[at * 4u], first_signs[(at * 4u) + 1u], first_signs[(at * 4u) + 2u],
                    first_signs[(at * 4u) + 3u]);
        }
        size_t gap_place = 0u;
        for (size_t at = 0u; at < papers.size(); at += 1u)
        {
            const GoldPaper &paper = papers[at];
            const GoldText &text = texts[paper.text];
            if (!paper.held)
            {
                fprintf(out, "paper %s says %s pairs %llu held 0\n", text.name.c_str(), text.language.c_str(),
                        tables[paper.whole].total);
                continue;
            }
            const int gapped_here = (gap_place < gapped.size()) && (gapped[gap_place] == at);
            fprintf(out, "paper %s says %s pairs %llu held 1 nearest %s", text.name.c_str(), text.language.c_str(),
                    tables[paper.whole].total, paper.order.empty() ? "-" : corpora[paper.order[0]].name.c_str());
            if (gapped_here)
            {
                fprintf(out, " gap %s / %s split %s / %s reads %d", gold_text_of(paper.gap.num).c_str(),
                        gold_text_of(paper.gap.den).c_str(), gold_text_of(paper.floor.num).c_str(),
                        gold_text_of(paper.floor.den).c_str(), paper.reads);
                gap_place += 1u;
            }
            fprintf(out, "\n");
        }
        for (size_t at = 0u; at < 2u; at += 1u)
        {
            if (median_held[at])
            {
                fprintf(out, "median %s = %s / %s\n", (at == 0u) ? "gap" : "split", gold_text_of(medians[at].num).c_str(),
                        gold_text_of(medians[at].den).c_str());
            }
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        printf("  %-16s %8s %10s  %-12s %-12s %-10s\n", "corpus", "lines", "pairs", "midpoint", "alternating", "ratio");
        for (size_t at = 0u; at < named; at += 1u)
        {
            printf("  %-16s %8zu %10llu  %-12s %-12s %-10s\n", corpora[at].name.c_str(), corpora[at].lines.size(),
                   tables[at].total, decimal[pairs + at].c_str(), decimal[pairs + named + at].c_str(),
                   decimal[ratio_at + at].c_str());
        }
        printf("\n  %-34s %-10s %-9s %-9s\n", "pair", "distance", "midpoint", "alternating");
        for (size_t at = 0u; at < pairs; at += 1u)
        {
            printf("  %-34s %-10s %-9s %-9s\n", comparisons[at].name.c_str(), decimal[at].c_str(),
                   pair_reads[at * 2u] ? "reads" : "does not", pair_reads[(at * 2u) + 1u] ? "reads" : "does not");
        }
        printf("\n  %u of %zu pairs read against the midpoint split-half distances, %u against the alternating\n",
               pairs_read[0], pairs, pairs_read[1]);
        printf("  %zu papers name a language the corpora cover: %u hold fewer than %llu pairs, %u read and agree with"
               " their prose on %u, %u do not read\n",
               papers.size(), small, least, read, agreed, unread);
        if (median_held[0])
        {
            printf("  over the %u that do not read, the median gap is %s and the median split-half distance %s\n",
                   unread, decimal[median_at].c_str(), decimal[median_at + 1u].c_str());
        }
        printf("  each decimal is the whole part and first six digits of the exact rational in the records\n");
    }

    if (ok && (count == 4))
    {
        FILE *figures = fopen(arguments[3], "w");
        if (figures != NULL)
        {
            const std::string *least_pair = &decimal[pair_order.sorted.front()];
            const std::string *most_pair = &decimal[pair_order.sorted.back()];
            fprintf(figures, "\\newcommand{\\GoldProfiles}{%zu}\n", named);
            fprintf(figures, "\\newcommand{\\GoldLanguages}{%zu}\n", anchors.size());
            fprintf(figures, "\\newcommand{\\GoldPairs}{%zu}\n", pairs);
            fprintf(figures, "\\newcommand{\\GoldPairsReadMidpoint}{%u}\n", pairs_read[0]);
            fprintf(figures, "\\newcommand{\\GoldPairsReadAlternating}{%u}\n", pairs_read[1]);
            fprintf(figures, "\\newcommand{\\GoldPairLeast}{%.5s}\n", least_pair->c_str());
            fprintf(figures, "\\newcommand{\\GoldPairMost}{%.5s}\n", most_pair->c_str());
            fprintf(figures, "\\newcommand{\\GoldSplitLeast}{%.5s}\n",
                    decimal[pairs + midpoint_order.sorted.front()].c_str());
            fprintf(figures, "\\newcommand{\\GoldSplitMost}{%.5s}\n",
                    decimal[pairs + midpoint_order.sorted.back()].c_str());
            fprintf(figures, "\\newcommand{\\GoldAlternatingLeast}{%.5s}\n",
                    decimal[pairs + named + alternating_order.sorted.front()].c_str());
            fprintf(figures, "\\newcommand{\\GoldAlternatingMost}{%.5s}\n",
                    decimal[pairs + named + alternating_order.sorted.back()].c_str());
            fprintf(figures, "\\newcommand{\\GoldRatioLeast}{%.4s}\n", decimal[ratio_at + ratio_order.sorted.front()].c_str());
            fprintf(figures, "\\newcommand{\\GoldRatioMost}{%.4s}\n", decimal[ratio_at + ratio_order.sorted.back()].c_str());
            unsigned int unread_pairs = 0u;
            for (size_t at = 0u; at < pairs; at += 1u)
            {
                if (pair_reads[at * 2u])
                {
                    continue;
                }
                unread_pairs += 1u;
                const GoldComparison &pair = comparisons[at];
                // the side whose midpoint split-half distance the pair did not clear
                const size_t wider = (first_signs[at * 4u] != 1) ? pair.left : pair.right;
                fprintf(figures, "\\newcommand{\\GoldUnreadLeft}{%s}\n", corpora[pair.left].name.c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadRight}{%s}\n", corpora[pair.right].name.c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadDistance}{%.5s}\n", decimal[at].c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadWider}{%s}\n", corpora[wider].name.c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadWiderLines}{%zu}\n", corpora[wider].lines.size());
                fprintf(figures, "\\newcommand{\\GoldUnreadSplit}{%.5s}\n", decimal[pairs + wider].c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadAlternating}{%.5s}\n", decimal[pairs + named + wider].c_str());
            }
            fprintf(figures, "\\newcommand{\\GoldPairsUnread}{%u}\n", unread_pairs);
            fprintf(figures, "\\newcommand{\\GoldPapers}{%zu}\n", papers.size());
            fprintf(figures, "\\newcommand{\\GoldPapersSmall}{%u}\n", small);
            fprintf(figures, "\\newcommand{\\GoldPapersLeast}{%llu}\n", least);
            fprintf(figures, "\\newcommand{\\GoldPapersRead}{%u}\n", read);
            fprintf(figures, "\\newcommand{\\GoldPapersAgreed}{%u}\n", agreed);
            fprintf(figures, "\\newcommand{\\GoldPapersUnread}{%u}\n", unread);
            fprintf(figures, "\\newcommand{\\GoldPapersNotRead}{%u}\n", unread + small);
            fprintf(figures, "\\newcommand{\\GoldMedianGap}{%.6s}\n", median_held[0] ? decimal[median_at].c_str() : "--");
            fprintf(figures, "\\newcommand{\\GoldMedianSplit}{%.6s}\n",
                    median_held[1] ? decimal[median_at + 1u].c_str() : "--");
            const std::string noise = (noise_at < decimal.size()) ? decimal[noise_at] : std::string("--");
            fprintf(figures, "\\newcommand{\\GoldNoiseTimes}{%s}\n", noise.substr(0u, noise.find('.')).c_str());
            // the pairs of languages alone, English left out: how many, how many read alternating, and how many
            // change verdict between the midpoint reading and the alternating one
            unsigned int language_pairs = 0u;
            unsigned int language_read = 0u;
            unsigned int language_changed = 0u;
            for (size_t at = 0u; at < pairs; at += 1u)
            {
                if ((corpora[comparisons[at].left].name == "English") ||
                    (corpora[comparisons[at].right].name == "English"))
                {
                    continue;
                }
                language_pairs += 1u;
                language_read += (unsigned int)pair_reads[(at * 2u) + 1u];
                language_changed += (unsigned int)(pair_reads[at * 2u] != pair_reads[(at * 2u) + 1u]);
            }
            fprintf(figures, "\\newcommand{\\GoldLanguagePairs}{%u}\n", language_pairs);
            fprintf(figures, "\\newcommand{\\GoldLanguagePairsReadAlternating}{%u}\n", language_read);
            fprintf(figures, "\\newcommand{\\GoldLanguagePairsChanged}{%u}\n", language_changed);
            size_t language_lines = 0u;
            for (size_t at = 0u; at < anchors.size(); at += 1u)
            {
                language_lines += corpora[anchors[at]].lines.size();
            }
            fprintf(figures, "\\newcommand{\\GoldLanguageLines}{%zu}\n", language_lines);
            fclose(figures);
        }
        sim_check(&job, figures != NULL, "the figures are written out");
    }

    gold_release(&distance_stage);
    gold_release(&ratio_stage);
    gold_release(&first_stage);
    gold_release(&gap_stage);
    gold_release(&second_stage);
    gold_release(&third_stage);
    gold_release(&mean_stage);
    gold_release(&noise_stage);
    gold_release(&digits_stage);
    return sim_close(&job, "gold_readings");
}
