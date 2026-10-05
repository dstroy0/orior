// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The record programs and stages the Salishan measurements share: whole numbers as limbs, exact rationals as
// their numerator and denominator, a program built step by step over one member's fields, and a stage that
// imprints it, lays it out, sweeps every lane on the device and on the host, and holds the two word for word.
//
//   Usage:  #include "record_stages.h", after sim.h
//
// The programs here take exact values in and give exact values out:
//   ratio     x / y as (x_num y_den) / (x_den y_num);
//   compare   the sign of x_num y_den m_den - y_num x_den m_num, +1 where x clears m times y;
//   gap       (n2 d1 - n1 d2) / (d1 d2), the second value less the first;
//   mean      (a d + c b) / (2 b d), the mean of two values;
//   digits    the whole part of a value and its first six digits after the point, by quotient and remainder;
//   count     how many bits of a 32-bit word are set, the word ANDed with a mask first where asked;
//   above     [m_k >= m] and m_k, a shuffle's count against the observed count.
// An order of values is read from the compare program's signs, every pair compared once. The sums of an output over
// runs of lanes are taken on the device and on the host and held equal.

#ifndef RECORD_STAGES_H
#define RECORD_STAGES_H

#include "../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../src/cu/engine/analysis/keymath/keymath.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#define STAGE_STEPS_MOST 32u
#define STAGE_FIELDS_MOST 8u
#define STAGE_OUTPUTS_MOST 4u
#define STAGE_DIGITS 6u
#define STAGE_DIGITS_SCALE 1000000ull

// a whole number as its magnitude's 32-bit limbs, least significant first
typedef std::vector<unsigned int> StageWhole;

typedef struct
{
    StageWhole num;
    StageWhole den;
} StageRational;

typedef struct
{
    EngineRecordStep steps[STAGE_STEPS_MOST];
    unsigned int count;
    unsigned int field_bits[STAGE_FIELDS_MOST];
    unsigned int field_offset[STAGE_FIELDS_MOST];
    unsigned int fields;
    unsigned int record_bits;
    unsigned int outputs[STAGE_OUTPUTS_MOST];
    unsigned int output_count;
} StageProgram;

typedef struct
{
    const char *name;
    StageProgram program;
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} Stage;

static StageWhole stage_whole(unsigned long long value)
{
    StageWhole whole;
    whole.push_back((unsigned int)(value & 0xFFFFFFFFull));
    whole.push_back((unsigned int)(value >> 32u));
    return whole;
}

static unsigned int stage_bits(const StageWhole &value)
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
static int stage_order_of(const StageWhole &one, const StageWhole &two)
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

static std::string stage_text_of(const StageWhole &value)
{
    if (stage_bits(value) <= 64u)
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

static unsigned int stage_step(StageProgram *program, EngineRecordOperation operation, unsigned int left,
                              unsigned int right)
{
    EngineRecordStep step = {operation, left, right, 0u};
    program->steps[program->count] = step;
    program->count += 1u;
    return program->count - 1u;
}

static unsigned int stage_constant(StageProgram *program, unsigned long long value)
{
    return stage_step(program, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFull),
                     (unsigned int)(value >> 32u));
}

static unsigned int stage_field(StageProgram *program, unsigned int bits)
{
    const unsigned int field = program->fields;
    program->field_bits[field] = bits;
    program->field_offset[field] = program->record_bits;
    program->record_bits += bits;
    program->fields += 1u;
    return stage_step(program, ENGINE_RECORD_FIELD, field, 0u);
}

static void stage_output(StageProgram *program, unsigned int step)
{
    program->outputs[program->output_count] = step;
    program->output_count += 1u;
}

// x / y as (x_num y_den) / (x_den y_num)
static void stage_ratio_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int x_num = stage_field(program, bits);
    const unsigned int x_den = stage_field(program, bits);
    const unsigned int y_num = stage_field(program, bits);
    const unsigned int y_den = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, x_num, y_den));
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, x_den, y_num));
}

// the sign of x_num y_den m_den - y_num x_den m_num, +1 where x clears m times y
static void stage_compare_program(unsigned int bits, unsigned int margin_bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int x_num = stage_field(program, bits);
    const unsigned int x_den = stage_field(program, bits);
    const unsigned int y_num = stage_field(program, bits);
    const unsigned int y_den = stage_field(program, bits);
    const unsigned int m_num = stage_field(program, margin_bits);
    const unsigned int m_den = stage_field(program, margin_bits);
    const unsigned int x_side = stage_step(program, ENGINE_RECORD_PRODUCT, x_num, y_den);
    const unsigned int x_whole = stage_step(program, ENGINE_RECORD_PRODUCT, x_side, m_den);
    const unsigned int y_side = stage_step(program, ENGINE_RECORD_PRODUCT, y_num, x_den);
    const unsigned int y_whole = stage_step(program, ENGINE_RECORD_PRODUCT, y_side, m_num);
    stage_output(program, stage_step(program, ENGINE_RECORD_COMPARE, x_whole, y_whole));
}

// the runner-up's distance less the nearest's, (n2 d1 - n1 d2) / (d1 d2)
static void stage_gap_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int n1 = stage_field(program, bits);
    const unsigned int d1 = stage_field(program, bits);
    const unsigned int n2 = stage_field(program, bits);
    const unsigned int d2 = stage_field(program, bits);
    const unsigned int second = stage_step(program, ENGINE_RECORD_PRODUCT, n2, d1);
    const unsigned int first = stage_step(program, ENGINE_RECORD_PRODUCT, n1, d2);
    stage_output(program, stage_step(program, ENGINE_RECORD_DIFFERENCE, second, first));
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, d1, d2));
}

// the mean of a / b and c / d, (a d + c b) / (2 b d)
static void stage_mean_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = stage_field(program, bits);
    const unsigned int b = stage_field(program, bits);
    const unsigned int c = stage_field(program, bits);
    const unsigned int d = stage_field(program, bits);
    const unsigned int left = stage_step(program, ENGINE_RECORD_PRODUCT, a, d);
    const unsigned int right = stage_step(program, ENGINE_RECORD_PRODUCT, c, b);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, left, right));
    const unsigned int both = stage_step(program, ENGINE_RECORD_PRODUCT, b, d);
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, both, stage_constant(program, 2ull)));
}

// the whole part of num / den and the first STAGE_DIGITS digits after the point
static void stage_digits_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int num = stage_field(program, bits);
    const unsigned int den = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, num, den));
    const unsigned int rest = stage_step(program, ENGINE_RECORD_REMAINDER, num, den);
    const unsigned int scaled = stage_step(program, ENGINE_RECORD_PRODUCT, rest, stage_constant(program, STAGE_DIGITS_SCALE));
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, scaled, den));
}

// `bits` of a whole number laid into a record at `offset`
static void stage_put(unsigned int *record, unsigned int offset, unsigned int bits, const StageWhole &value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int to = offset + bit;
        const unsigned int one = ((bit / 32u) < value.size()) ? ((value[bit / 32u] >> (bit % 32u)) & 1u) : 0u;
        record[to / 32u] = (record[to / 32u] & ~(1u << (to % 32u))) | (one << (to % 32u));
    }
}

// an output of a lane's record, as limbs of its two's complement
static StageWhole stage_take(const unsigned int *record, unsigned int offset, unsigned int bits)
{
    StageWhole value((bits + 31u) / 32u, 0u);
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int from = offset + bit;
        value[bit / 32u] |= ((record[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    return value;
}

static int stage_sign(const StageWhole &value, unsigned int bits)
{
    const int negative = ((value[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u) != 0u;
    int zero = 1;
    for (size_t at = 0u; at < value.size(); at += 1u)
    {
        zero = zero && (value[at] == 0u);
    }
    return negative ? -1 : (zero ? 0 : 1);
}

static std::vector<size_t> stage_run(size_t from, size_t until, size_t step)
{
    std::vector<size_t> picked;
    for (size_t at = from; at < until; at += step)
    {
        picked.push_back(at);
    }
    return picked;
}

// imprints and lays out a stage's program over one member's records
static int stage_lay(SimResults *job, Stage *stage, EngineError *error)
{
    const StageProgram *program = &stage->program;
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
static int stage_sweep(SimResults *job, Stage *stage, const std::vector<unsigned int> &records,
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

static void stage_release(Stage *stage)
{
    if (stage->record != NULL)
    {
        cycle_record_release(stage->record);
    }
    key_schedule_record_release(&stage->layout);
    keymath_record_release(&stage->key);
}

// one lane a row of whole numbers, every field `bits` wide
typedef std::vector<std::vector<StageWhole>> StageRows;

// lays a stage out over rows, sweeps it, and returns each lane's outputs
static int stage_rows(SimResults *job, Stage *stage, const StageRows &rows,
                           std::vector<std::vector<StageWhole>> *outputs, EngineError *error)
{
    outputs->clear();
    if (rows.empty())
    {
        return 1;
    }
    int ok = stage_lay(job, stage, error);
    const unsigned int limbs = (stage->program.record_bits + 31u) / 32u;
    std::vector<unsigned int> records(rows.size() * limbs, 0u);
    for (size_t lane = 0u; lane < rows.size(); lane += 1u)
    {
        for (unsigned int field = 0u; field < stage->program.fields; field += 1u)
        {
            stage_put(&records[lane * limbs], stage->program.field_offset[field], stage->program.field_bits[field],
                     rows[lane][field]);
        }
    }
    std::vector<unsigned int> out;
    unsigned int *device_out = NULL;
    ok = ok && stage_sweep(job, stage, records, rows.size(), &out, &device_out, error);
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }
    for (size_t lane = 0u; ok && (lane < rows.size()); lane += 1u)
    {
        std::vector<StageWhole> lane_out;
        for (unsigned int at = 0u; at < stage->program.output_count; at += 1u)
        {
            const DeviceRecordStep *step = &stage->layout.step_table[stage->program.outputs[at]];
            lane_out.push_back(stage_take(&out[lane * stage->layout.out_limbs], step->out_offset, step->out_bits));
        }
        outputs->push_back(lane_out);
    }
    return ok;
}

static unsigned int stage_widest(const std::vector<StageRational> &values)
{
    unsigned int bits = 1u;
    for (size_t at = 0u; at < values.size(); at += 1u)
    {
        bits = std::max(bits, std::max(stage_bits(values[at].num), stage_bits(values[at].den)));
    }
    return bits;
}

// the comparison lanes an order of values needs, every pair once, and the order the machine's signs give, least
// first, ties broken by `names` where given and by place otherwise
typedef struct
{
    std::vector<StageRational> values;
    std::vector<std::string> names;
    size_t first_lane;
    std::vector<size_t> sorted;
} StageOrder;

static void stage_order_lanes(StageOrder *order, std::vector<StageRational> *xs, std::vector<StageRational> *ys)
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

static void stage_order_read(StageOrder *order, const std::vector<int> &signs)
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
    order->sorted = stage_run(0u, count, 1u);
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
static int stage_compare(SimResults *job, Stage *stage, const char *name, const std::vector<StageRational> &xs,
                        const std::vector<StageRational> &ys, const std::vector<int> &margined,
                        const unsigned long long margin[2], std::vector<int> *signs, EngineError *error)
{
    signs->clear();
    if (xs.empty())
    {
        return 1;
    }
    memset(stage, 0, sizeof(*stage));
    stage->name = name;
    const StageWhole m_num = stage_whole(margin[0]);
    const StageWhole m_den = stage_whole(margin[1]);
    const StageWhole one = stage_whole(1ull);
    const unsigned int bits = std::max(stage_widest(xs), stage_widest(ys));
    stage_compare_program(bits, std::max(stage_bits(m_num), stage_bits(m_den)), &stage->program);
    StageRows rows;
    for (size_t lane = 0u; lane < xs.size(); lane += 1u)
    {
        const int with = margined[lane];
        rows.push_back({xs[lane].num, xs[lane].den, ys[lane].num, ys[lane].den, with ? m_num : one,
                        with ? m_den : one});
    }
    std::vector<std::vector<StageWhole>> outputs;
    const int ok = stage_rows(job, stage, rows, &outputs, error);
    const DeviceRecordStep *step = ok ? &stage->layout.step_table[stage->program.outputs[0]] : NULL;
    for (size_t lane = 0u; ok && (lane < outputs.size()); lane += 1u)
    {
        signs->push_back(stage_sign(outputs[lane][0], step->out_bits));
    }
    return ok;
}

static StageRational stage_unit(void)
{
    return {stage_whole(1ull), stage_whole(1ull)};
}

static StageRational stage_pair_of(const std::vector<StageWhole> &out)
{
    return {out[0], out[1]};
}

// a file's lines, without their line breaks; 0 where it cannot be read
static int stage_lines(const char *path, std::vector<std::string> *lines)
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
    size_t start = 0u;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        end = (end == std::string::npos) ? text.size() : end;
        lines->push_back(text.substr(start, end - start));
        start = end + 1u;
    }
    return 1;
}

// a line's tab-separated fields
static std::vector<std::string> stage_split(const std::string &line)
{
    std::vector<std::string> fields;
    size_t start = 0u;
    while (true)
    {
        const size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start, (tab == std::string::npos) ? std::string::npos : tab - start));
        if (tab == std::string::npos)
        {
            return fields;
        }
        start = tab + 1u;
    }
}

// a name cut to `width` characters and padded to them, counting a UTF-8 character once and not by its bytes
static std::string stage_shown(const std::string &text, size_t width, int pad)
{
    std::string shown;
    size_t characters = 0u;
    for (size_t at = 0u; at < text.size(); at += 1u)
    {
        const int starts = (((unsigned char)text[at]) & 0xC0u) != 0x80u;
        if (starts && (characters == width))
        {
            break;
        }
        characters += (size_t)starts;
        shown += text[at];
    }
    while (pad && (characters < width))
    {
        shown += ' ';
        characters += 1u;
    }
    return shown;
}

// how many bits of a 32-bit word are set, by the halving sums of adjacent fields; where `masked`, the word is
// ANDed with a second 32-bit field first
static void stage_count_program(int masked, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int given = stage_field(program, 32u);
    const unsigned int word = masked ? stage_step(program, ENGINE_RECORD_AND, given, stage_field(program, 32u)) : given;
    const unsigned int halved = stage_step(program, ENGINE_RECORD_QUOTIENT, word, stage_constant(program, 2ull));
    const unsigned int odd = stage_step(program, ENGINE_RECORD_AND, halved, stage_constant(program, 0x55555555ull));
    const unsigned int twos = stage_step(program, ENGINE_RECORD_DIFFERENCE, word, odd);
    const unsigned int pairs_mask = stage_constant(program, 0x33333333ull);
    const unsigned int low_twos = stage_step(program, ENGINE_RECORD_AND, twos, pairs_mask);
    const unsigned int shifted = stage_step(program, ENGINE_RECORD_QUOTIENT, twos, stage_constant(program, 4ull));
    const unsigned int high_twos = stage_step(program, ENGINE_RECORD_AND, shifted, pairs_mask);
    const unsigned int fours = stage_step(program, ENGINE_RECORD_SUM, low_twos, high_twos);
    const unsigned int moved = stage_step(program, ENGINE_RECORD_QUOTIENT, fours, stage_constant(program, 16ull));
    const unsigned int joined = stage_step(program, ENGINE_RECORD_SUM, fours, moved);
    const unsigned int bytes = stage_step(program, ENGINE_RECORD_AND, joined, stage_constant(program, 0x0F0F0F0Full));
    const unsigned int spread = stage_step(program, ENGINE_RECORD_PRODUCT, bytes, stage_constant(program, 0x01010101ull));
    const unsigned int top = stage_step(program, ENGINE_RECORD_QUOTIENT, spread, stage_constant(program, 1ull << 24u));
    stage_output(program, stage_step(program, ENGINE_RECORD_AND, top, stage_constant(program, 0xFFull)));
}

// [m_k >= m] as the quotient of COMPARE(m_k, m) + 2 by 2, and m_k beside it
static void stage_above_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int shuffled = stage_field(program, bits);
    const unsigned int observed = stage_field(program, bits);
    const unsigned int sign = stage_step(program, ENGINE_RECORD_COMPARE, shuffled, observed);
    const unsigned int raised = stage_step(program, ENGINE_RECORD_SUM, sign, stage_constant(program, 2ull));
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, raised, stage_constant(program, 2ull)));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, shuffled, stage_constant(program, 0ull)));
}

// the exact sum of each run of `group` lanes of a stage's output, on the device and on the host, held equal
static int stage_sums(SimResults *job, const char *what, const unsigned int *device_out,
                      const std::vector<unsigned int> &host_out, unsigned long long lanes, unsigned long long group,
                      unsigned int out_limbs, const DeviceRecordStep *step, std::vector<StageWhole> *sums,
                      EngineError *error)
{
    const unsigned int sum_limbs = ((step->out_bits + stage_bits(stage_whole(group)) + 31u) / 32u) + 1u;
    const unsigned long long runs = lanes / group;
    std::vector<unsigned int> device_sums(runs * sum_limbs, 0u);
    std::vector<unsigned int> host_sums(runs * sum_limbs, 0u);
    const CycleRecordSumRequest on_device = {device_out, lanes, group, out_limbs, step->out_offset, step->out_bits,
                                             sum_limbs, device_sums.data(), error};
    const CycleRecordSumRequest on_host = {host_out.data(), lanes, group, out_limbs, step->out_offset, step->out_bits,
                                           sum_limbs, host_sums.data(), error};
    int ok = (cycle_record_sum(&on_device) != CYCLE_ERROR) && (cycle_record_sum_host(&on_host) != CYCLE_ERROR);
    ok = ok && (memcmp(device_sums.data(), host_sums.data(), device_sums.size() * sizeof(unsigned int)) == 0);
    sim_check(job, ok, (std::string("the device's sums of ") + what + " equal the host's").c_str());
    sums->clear();
    for (unsigned long long run = 0ull; ok && (run < runs); run += 1ull)
    {
        sums->push_back(StageWhole(device_sums.begin() + (long long)(run * sum_limbs),
                                   device_sums.begin() + (long long)((run + 1ull) * sum_limbs)));
    }
    return ok;
}

// lays a stage out over prepared records, sweeps it, and sums one output over runs of `group` lanes
static int stage_summed(SimResults *job, Stage *stage, const std::vector<unsigned int> &records,
                        unsigned long long lanes, unsigned long long group, const std::vector<unsigned int> &outputs,
                        std::vector<std::vector<StageWhole>> *sums, EngineError *error)
{
    std::vector<unsigned int> out;
    unsigned int *device_out = NULL;
    int ok = stage_sweep(job, stage, records, lanes, &out, &device_out, error);
    sums->assign(outputs.size(), std::vector<StageWhole>());
    for (size_t at = 0u; ok && (at < outputs.size()); at += 1u)
    {
        const DeviceRecordStep *step = &stage->layout.step_table[stage->program.outputs[outputs[at]]];
        ok = stage_sums(job, stage->name, device_out, out, lanes, group, stage->layout.out_limbs, step, &(*sums)[at],
                        error);
    }
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }
    return ok;
}

#endif
