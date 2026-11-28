// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The record programs and stages the Salishan measurements share: whole numbers as limbs, exact rationals as
// their numerator and denominator, a program built step by step over one member's fields, and a stage that
// imprints it, positions it out, sweeps every lane on the device and on the host, and compares the two word for word.
//
//   Usage:  #include "record_stages.h", after sim.h
//
// The programs here take exact values in and give exact values out:
//   ratio     x / y as (x_num y_den) / (x_den y_num);
//   compare   the sign of x_num y_den m_den - y_num x_den m_num, +1 where x clears m times y;
//   gap       (n2 d1 - n1 d2) / (d1 d2), the second value less the first;
//   mean      (a d + c b) / (2 b d), the mean of two values;
//   digits    the whole part of a value and its first six digits after the point, by quotient and remainder;
//   count     how many bits of a 32-bit word are set, the word ANDed with a mask first where requested;
//   above     [m_k >= m] and m_k, a shuffle's count against the observed count.
// An order of values is read from the compare program's signs, every pair compared once. The sums of an output over
// runs of lanes are taken on the device and on the host and checked equal.
//
// The memory is the pool's. An input is read once into one cell and split in place, its lines at their line breaks
// and its fields at their tabs, and every name points into that cell. The tables of rows and of outputs, the sums and
// the orders are cells of the temporary tier that the caller gives back.

#ifndef RECORD_STAGES_H
#define RECORD_STAGES_H

#include "../../../includes/platform/platform.h"
#include "../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../src/cu/engine/analysis/keymath/keymath.h"
#include "sim.h"

#include <cuda_runtime.h>

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

#define STAGE_STEPS_MAX 32u
#define STAGE_FIELDS_MAX 8u
#define STAGE_OUTPUTS_MAX 4u
#define STAGE_DIGITS 6u
#define STAGE_DIGITS_SCALE 1000000ull

// the limbs a whole number holds at max: an output wider than this is a refusal, and its stage fails
#define STAGE_WHOLE_LIMBS 64u

// the bytes of a printed text: a whole number as hex of STAGE_WHOLE_LIMBS limbs, or a name cut to its columns
#define STAGE_TEXT_BYTES 528u

// the tab fields a line holds at max; a line with more is a refusal of its input
#define STAGE_LINE_FIELDS 256u

// a whole number as its magnitude's 32-bit limbs, least significant first, `count` of them
typedef struct
{
    unsigned int limb[STAGE_WHOLE_LIMBS];
    unsigned int count;
} StageWhole;

typedef struct
{
    StageWhole num;
    StageWhole den;
} StageRational;

// a text that a stage prints, terminated
typedef struct
{
    char bytes[STAGE_TEXT_BYTES];
} StageText;

typedef struct
{
    EngineRecordStep steps[STAGE_STEPS_MAX];
    unsigned int count;
    unsigned int field_bits[STAGE_FIELDS_MAX];
    unsigned int field_offset[STAGE_FIELDS_MAX];
    unsigned int fields;
    unsigned int record_bits;
    unsigned int outputs[STAGE_OUTPUTS_MAX];
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

// the outputs that passed STAGE_WHOLE_LIMBS, which fail the stage that took them
static unsigned int s_stage_wide;

// a cell of `count` elements of `size` bytes, each byte 0, on the temporary tier; NULL where the pool cannot meet it
static void *stage_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_cleared((count != 0ull) ? count : 1ull, size, platform_place_g_program_owner, 1);
}

static void stage_cell_released(void *cell)
{
    platform_cell_released(cell, platform_place_g_program_owner);
}

static StageWhole stage_whole(unsigned long long value)
{
    StageWhole whole;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &whole, .bytes = sizeof(whole), .val = 0);
    whole.limb[0] = (unsigned int)(value & 0xFFFFFFFFull);
    whole.limb[1] = (unsigned int)(value >> 32u);
    whole.count = 2u;
    return whole;
}

static unsigned int stage_bits(const StageWhole &value)
{
    for (unsigned int limb = value.count; limb > 0u; limb -= 1u)
    {
        if (value.limb[limb - 1u] != 0u)
        {
            unsigned int bits = 32u * (limb - 1u);
            unsigned int word = value.limb[limb - 1u];
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
    const unsigned int limbs = (one.count > two.count) ? one.count : two.count;
    for (unsigned int limb = limbs; limb > 0u; limb -= 1u)
    {
        const unsigned int left = (limb - 1u < one.count) ? one.limb[limb - 1u] : 0u;
        const unsigned int right = (limb - 1u < two.count) ? two.limb[limb - 1u] : 0u;
        if (left != right)
        {
            return (left < right) ? -1 : 1;
        }
    }
    return 0;
}

// a whole number as decimal where it fits 64 bits, and as hex after 0x otherwise
static StageText stage_text_of(const StageWhole &value)
{
    StageText text;
    VerbaLinea line = mmgr_line_over(text.bytes, sizeof(text.bytes));
    if (stage_bits(value) <= 64u)
    {
        const unsigned long long low = (value.count > 0u) ? value.limb[0] : 0u;
        const unsigned long long high = (value.count > 1u) ? value.limb[1] : 0u;
        mmgr_line_decimal(&line, low | (high << 32u), 1u);
        (void)mmgr_line_finish(&line);
        return text;
    }
    mmgr_line_put(&line, "0x");
    char limb[16];
    int leading = 1;
    for (unsigned int at = value.count; at > 0u; at -= 1u)
    {
        if (leading && (value.limb[at - 1u] == 0u))
        {
            continue;
        }
        snprintf(limb, sizeof(limb), leading ? "%x" : "%08x", value.limb[at - 1u]);
        mmgr_line_put(&line, limb);
        leading = 0;
    }
    (void)mmgr_line_finish(&line);
    return text;
}

// a whole part and its six digits after the point, the text of a value the digits program reads
static StageText stage_decimal_of(const StageWhole &whole, const StageWhole &digits)
{
    StageText text;
    VerbaLinea line = mmgr_line_over(text.bytes, sizeof(text.bytes));
    mmgr_line_put(&line, stage_text_of(whole).bytes);
    // the six digits are below 10^6, which the first limb stores
    char fraction[16];
    snprintf(fraction, sizeof(fraction), ".%06u", digits.limb[0]);
    mmgr_line_put(&line, fraction);
    (void)mmgr_line_finish(&line);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int num = stage_field(program, bits);
    const unsigned int den = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, num, den));
    const unsigned int rest = stage_step(program, ENGINE_RECORD_REMAINDER, num, den);
    const unsigned int scaled = stage_step(program, ENGINE_RECORD_PRODUCT, rest, stage_constant(program, STAGE_DIGITS_SCALE));
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, scaled, den));
}

// `bits` of a whole number positioned into a record at `offset`
static void stage_put(unsigned int *record, unsigned int offset, unsigned int bits, const StageWhole &value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int to = offset + bit;
        const unsigned int one = ((bit / 32u) < value.count) ? ((value.limb[bit / 32u] >> (bit % 32u)) & 1u) : 0u;
        record[to / 32u] = (record[to / 32u] & ~(1u << (to % 32u))) | (one << (to % 32u));
    }
}

// an output of a lane's record, as limbs of its two's complement. An output past STAGE_WHOLE_LIMBS keeps its low limbs
// and counts in s_stage_wide
static StageWhole stage_take(const unsigned int *record, unsigned int offset, unsigned int bits)
{
    StageWhole value;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &value, .bytes = sizeof(value), .val = 0);
    const unsigned int limbs = (bits + 31u) / 32u;
    s_stage_wide += (limbs > STAGE_WHOLE_LIMBS) ? 1u : 0u;
    value.count = (limbs > STAGE_WHOLE_LIMBS) ? STAGE_WHOLE_LIMBS : limbs;
    for (unsigned int bit = 0u; bit < (32u * value.count) && (bit < bits); bit += 1u)
    {
        const unsigned int from = offset + bit;
        value.limb[bit / 32u] |= ((record[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    return value;
}

// the limbs at `limbs`, `count` of them, as a whole number. A count past STAGE_WHOLE_LIMBS counts in s_stage_wide
static StageWhole stage_whole_of(const unsigned int *limbs, unsigned int count)
{
    StageWhole value;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &value, .bytes = sizeof(value), .val = 0);
    s_stage_wide += (count > STAGE_WHOLE_LIMBS) ? 1u : 0u;
    value.count = (count > STAGE_WHOLE_LIMBS) ? STAGE_WHOLE_LIMBS : count;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = value.limb, .src = limbs, .bytes = value.count * sizeof(unsigned int));
    return value;
}

static int stage_sign(const StageWhole &value, unsigned int bits)
{
    const int negative = ((value.limb[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u) != 0u;
    int zero = 1;
    for (unsigned int at = 0u; at < value.count; at += 1u)
    {
        zero = zero && (value.limb[at] == 0u);
    }
    return negative ? -1 : (zero ? 0 : 1);
}

// a check whose text is `front`, the stage's name and `back`, in a line of a fixed bound
static void stage_check(SimResults *job, int passed, const char *front, const char *name, const char *back)
{
    char text[256];
    VerbaLinea line = mmgr_line_over(text, sizeof(text));
    mmgr_line_put(&line, front);
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, back);
    (void)mmgr_line_finish(&line);
    sim_check(job, passed, text);
}

// 1 where no output passed STAGE_WHOLE_LIMBS, and 0 where one did, the refusal printed
static int stage_fits(const char *name)
{
    if (s_stage_wide == 0u)
    {
        return 1;
    }
    fprintf(stderr, "  the %s stage refuses an output wider than %u limbs\n", name, STAGE_WHOLE_LIMBS);
    return 0;
}

// imprints and arranges a stage's program over one member's records
static int stage_position(SimResults *job, Stage *stage, EngineError *error)
{
    const StageProgram *program = &stage->program;
    const KeymathRecordRequest encode = {program->steps, program->count, program->field_bits, program->fields,
                                         1u, program->outputs, program->output_count, NULL, 0u, &stage->key, error};
    int ok = keymath_record_encode(&encode) != KEYMATH_ERROR;
    stage_check(job, ok, "keymath imprints the ", stage->name, " program");
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {(program->record_bits + 31u) / 32u, 0u, 0u};
    const KeyScheduleRecordRequest position = {
        &stage->key, program->field_offset, program->fields, in_limbs, 1, &stage->layout, error};
    ok = ok && (key_schedule_record_layout(&position) != KEY_SCHEDULE_ERROR);
    stage_check(job, ok, "the scheduler positions the ", stage->name, " program out");
    return ok;
}

// Runs every lane on the device and on the host and compares the two word for word. The records in are
// `record_words` words. The device's records come back in *out, a cell of the temporary tier that the caller gives
// back, and stay on the device at *device_out for the caller to sum and free
static int stage_sweep(SimResults *job, Stage *stage, const unsigned int *records, unsigned long long record_words,
                       unsigned long long lanes, unsigned int **out, unsigned int **device_out, EngineError *error)
{
    const char *const name = stage->name;
    int ok = cycle_record_load(&stage->layout, &stage->record, error) != CYCLE_ERROR;
    stage_check(job, ok, "the ", name, " program loads onto the device");
    const unsigned int out_limbs = stage->layout.out_limbs;
    const unsigned long long out_words = lanes * out_limbs;
    unsigned int *device_in = NULL;
    ok = ok && (cudaMalloc((void **)&device_in, record_words * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)device_out, out_words * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_in, records, record_words * sizeof(unsigned int), cudaMemcpyHostToDevice) == cudaSuccess);
    const CycleRecordRunRequest run = {stage->record, {device_in, NULL, NULL}, {lanes, 0ull, 0ull}, NULL, lanes,
                                       *device_out, error};
    ok = ok && (cycle_record_run(&run) == (long)lanes);
    stage_check(job, ok, "every lane runs the ", name, " program on the device");
    *out = (unsigned int *)stage_cell(out_words, sizeof(unsigned int));
    unsigned int *const host_out = (unsigned int *)stage_cell(out_words, sizeof(unsigned int));
    ok = ok && (*out != NULL) && (host_out != NULL) &&
         (cudaMemcpy(*out, *device_out, out_words * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess);
    const CycleRecordHostRequest host = {
        &stage->layout, {records, NULL, NULL}, {lanes, 0ull, 0ull}, NULL, lanes, host_out, error};
    const int same = ok && (cycle_record_run_host(&host) == (long)lanes) &&
                     (EMBED_CALL(memor.cmp, MemoriaCfg, .src = host_out, .other = *out,
                                 .bytes = out_words * sizeof(unsigned int)) == 0);
    stage_check(job, same, "the host's ", name, " records equal the device's word for word");
    stage_cell_released(host_out);
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

// A table of whole numbers, `rows` rows of `fields` each, row r's field f at entry[r * fields + f], in a cell of the
// temporary tier that the caller gives back
typedef struct
{
    StageWhole *entry;
    unsigned long long rows;
    unsigned int fields;
} StageTable;

// the table's cell taken at its rows and fields, every entry 0; 0 where the pool cannot meet it
static int stage_table_taken(StageTable *table, unsigned long long rows, unsigned int fields)
{
    table->rows = rows;
    table->fields = fields;
    table->entry = (StageWhole *)stage_cell(rows * fields, sizeof(StageWhole));
    return table->entry != NULL;
}

static StageWhole *stage_row(const StageTable *table, unsigned long long row)
{
    return &table->entry[row * table->fields];
}

static void stage_table_released(StageTable *table)
{
    stage_cell_released(table->entry);
    table->entry = NULL;
    table->rows = 0ull;
}

// positions a stage out over the rows, sweeps it, and gives each lane's outputs in *outputs, one row a lane
static int stage_rows(SimResults *job, Stage *stage, const StageTable *rows, StageTable *outputs, EngineError *error)
{
    outputs->entry = NULL;
    outputs->rows = 0ull;
    outputs->fields = stage->program.output_count;
    if (rows->rows == 0ull)
    {
        return 1;
    }
    int ok = stage_position(job, stage, error);
    const unsigned int limbs = (stage->program.record_bits + 31u) / 32u;
    unsigned int *const records = (unsigned int *)stage_cell(rows->rows * limbs, sizeof(unsigned int));
    ok = ok && (records != NULL);
    for (unsigned long long lane = 0ull; ok && (lane < rows->rows); lane += 1ull)
    {
        for (unsigned int field = 0u; field < stage->program.fields; field += 1u)
        {
            stage_put(&records[lane * limbs], stage->program.field_offset[field], stage->program.field_bits[field],
                      stage_row(rows, lane)[field]);
        }
    }
    unsigned int *out = NULL;
    unsigned int *device_out = NULL;
    ok = ok && stage_sweep(job, stage, records, rows->rows * limbs, rows->rows, &out, &device_out, error);
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }
    stage_cell_released(records);
    ok = ok && stage_table_taken(outputs, rows->rows, stage->program.output_count);
    for (unsigned long long lane = 0ull; ok && (lane < rows->rows); lane += 1ull)
    {
        for (unsigned int at = 0u; at < stage->program.output_count; at += 1u)
        {
            const DeviceRecordStep *step = &stage->layout.step_table[stage->program.outputs[at]];
            stage_row(outputs, lane)[at] =
                stage_take(&out[lane * stage->layout.out_limbs], step->out_offset, step->out_bits);
        }
    }
    stage_cell_released(out);
    return ok && stage_fits(stage->name);
}

static unsigned int stage_widest(const StageRational *values, unsigned long long count)
{
    unsigned int bits = 1u;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int num = stage_bits(values[at].num);
        const unsigned int den = stage_bits(values[at].den);
        const unsigned int wider = (num > den) ? num : den;
        bits = (wider > bits) ? wider : bits;
    }
    return bits;
}

// the comparison lanes an order of `count` values needs, every pair once
static unsigned long long stage_order_pairs(unsigned long long count)
{
    return (count * ((count != 0ull) ? (count - 1ull) : 0ull)) / 2ull;
}

// The order of `count` values the machine's signs give, least first, ties broken by `names` where given and by place
// otherwise. Its lanes start at first_lane, and sorted is a cell of the temporary tier that the caller gives back
typedef struct
{
    const StageRational *values;
    const char *const *names;
    unsigned long long count;
    unsigned long long first_lane;
    unsigned long long *sorted;
} StageOrder;

// the order's pairs put at xs and ys from *lanes on, and *lanes moved past them
static void stage_order_lanes(StageOrder *order, StageRational *xs, StageRational *ys, unsigned long long *lanes)
{
    order->first_lane = *lanes;
    for (unsigned long long one = 0ull; one < order->count; one += 1ull)
    {
        for (unsigned long long two = one + 1ull; two < order->count; two += 1ull)
        {
            xs[*lanes] = order->values[one];
            ys[*lanes] = order->values[two];
            *lanes += 1ull;
        }
    }
}

// the order the sort reads: the signs of every pair, and the names where given
static const int *s_stage_order_signs;
static const StageOrder *s_stage_order;

// the order of two strings by their bytes, and the shorter first where one begins the other
static int stage_text_order(const char *one, const char *two)
{
    const size_t one_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = one, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t two_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = two, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t shared = ((one_length < two_length) ? one_length : two_length) + 1u;
    const embed_iword compared = EMBED_CALL(memor.cmp, MemoriaCfg, .src = one, .other = two, .bytes = shared);
    return (compared < 0) ? -1 : ((compared > 0) ? 1 : 0);
}

static int stage_same(const char *one, const char *two)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = one, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = two,
                      .ci = EMBED_FALSE);
}

// 1 where value `one` goes before value `two` of the order, as the comparator of std::stable_sort reads it
static int stage_order_before(unsigned long long one, unsigned long long two)
{
    const unsigned long long count = s_stage_order->count;
    const int sign = s_stage_order_signs[one * count + two];
    if (sign != 0)
    {
        return sign < 0;
    }
    if (s_stage_order->names != NULL)
    {
        return stage_text_order(s_stage_order->names[one], s_stage_order->names[two]) < 0;
    }
    return one < two;
}

// memor.sort's order: the left value stays first unless the right goes before it
static int stage_order_compared(const void *left, const void *right)
{
    const unsigned long long one = *(const unsigned long long *)left;
    const unsigned long long two = *(const unsigned long long *)right;
    return stage_order_before(two, one) ? 1 : (stage_order_before(one, two) ? -1 : 0);
}

// the order read from the signs of its lanes; 0 where the pool cannot meet a cell
static int stage_order_read(StageOrder *order, const int *signs)
{
    const unsigned long long count = order->count;
    int *const table = (int *)stage_cell(count * count, sizeof(int));
    unsigned long long *const room = (unsigned long long *)stage_cell(count, sizeof(unsigned long long));
    order->sorted = (unsigned long long *)stage_cell(count, sizeof(unsigned long long));
    const int taken = (table != NULL) && (room != NULL) && (order->sorted != NULL);
    unsigned long long lane = order->first_lane;
    for (unsigned long long one = 0ull; taken && (one < count); one += 1ull)
    {
        order->sorted[one] = one;
        for (unsigned long long two = one + 1ull; two < count; two += 1ull)
        {
            table[one * count + two] = signs[lane];
            table[two * count + one] = -signs[lane];
            lane += 1ull;
        }
    }
    if (taken)
    {
        s_stage_order_signs = table;
        s_stage_order = order;
        EMBED_CALL(memor.sort, MemoriaCfg, .dst = order->sorted, .bytes = sizeof(unsigned long long), .scratch = room,
                   .count = count, .order = stage_order_compared);
    }
    stage_cell_released(room);
    stage_cell_released(table);
    return taken;
}

static void stage_order_released(StageOrder *order)
{
    stage_cell_released(order->sorted);
    order->sorted = NULL;
}

// runs the compare program over `count` x and y lanes at margin m, and puts each lane's sign at signs
static int stage_compare(SimResults *job, Stage *stage, const char *name, const StageRational *xs,
                         const StageRational *ys, const int *margined, unsigned long long count,
                         const unsigned long long margin[2], int *signs, EngineError *error)
{
    if (count == 0ull)
    {
        return 1;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
    stage->name = name;
    const StageWhole m_num = stage_whole(margin[0]);
    const StageWhole m_den = stage_whole(margin[1]);
    const StageWhole one = stage_whole(1ull);
    const unsigned int x_bits = stage_widest(xs, count);
    const unsigned int y_bits = stage_widest(ys, count);
    const unsigned int num_bits = stage_bits(m_num);
    const unsigned int den_bits = stage_bits(m_den);
    stage_compare_program((x_bits > y_bits) ? x_bits : y_bits, (num_bits > den_bits) ? num_bits : den_bits,
                          &stage->program);
    StageTable rows;
    int ok = stage_table_taken(&rows, count, 6u);
    for (unsigned long long lane = 0ull; ok && (lane < count); lane += 1ull)
    {
        const int with = margined[lane];
        StageWhole *const row = stage_row(&rows, lane);
        row[0] = xs[lane].num;
        row[1] = xs[lane].den;
        row[2] = ys[lane].num;
        row[3] = ys[lane].den;
        row[4] = with ? m_num : one;
        row[5] = with ? m_den : one;
    }
    StageTable outputs;
    ok = ok && stage_rows(job, stage, &rows, &outputs, error);
    const DeviceRecordStep *step = ok ? &stage->layout.step_table[stage->program.outputs[0]] : NULL;
    for (unsigned long long lane = 0ull; ok && (lane < outputs.rows); lane += 1ull)
    {
        signs[lane] = stage_sign(stage_row(&outputs, lane)[0], step->out_bits);
    }
    stage_table_released(&outputs);
    stage_table_released(&rows);
    return ok;
}

static StageRational stage_unit(void)
{
    return {stage_whole(1ull), stage_whole(1ull)};
}

static StageRational stage_pair_of(const StageWhole *out)
{
    return {out[0], out[1]};
}

// A file read once into one cell of the temporary tier, `size` bytes and a terminator, and split in place at its line
// breaks: line l is the terminated text at line[l], `count` lines, without their line breaks. length[l] is the bytes of
// line l up to its line break, a NUL byte inside it counted
typedef struct
{
    char *bytes;
    unsigned long long size;
    char **line;
    unsigned long long *length;
    unsigned long long count;
} StageLines;

// the file's lines; 0 where it cannot be read or the pool cannot meet its cells
static int stage_lines(const char *path, StageLines *lines)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = lines, .bytes = sizeof(*lines), .val = 0);
    FILE *in = fopen(path, "rb");
    if (in == NULL)
    {
        return 0;
    }
    int ok = (fseek(in, 0, SEEK_END) == 0);
    const long size = ok ? ftell(in) : -1l;
    ok = ok && (size >= 0l) && (fseek(in, 0, SEEK_SET) == 0);
    lines->size = ok ? (unsigned long long)size : 0ull;
    lines->bytes = ok ? (char *)stage_cell(lines->size + 1ull, 1u) : NULL;
    ok = ok && (lines->bytes != NULL) && (fread(lines->bytes, 1u, (size_t)lines->size, in) == (size_t)lines->size);
    fclose(in);
    // the lines counted, then their cell taken, then each break made a terminator
    for (int pass = 0; ok && (pass < 2); pass += 1)
    {
        unsigned long long start = 0ull;
        unsigned long long count = 0ull;
        while (start < lines->size)
        {
            const char *const found = (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = lines->bytes + start,
                                                               .bytes = (size_t)(lines->size - start), .val = '\n');
            const unsigned long long end = (found != NULL) ? (unsigned long long)(found - lines->bytes) : lines->size;
            if (pass == 1)
            {
                lines->line[count] = lines->bytes + start;
                lines->length[count] = end - start;
                lines->bytes[end] = '\0';
            }
            count += 1ull;
            start = end + 1ull;
        }
        if (pass == 0)
        {
            lines->line = (char **)stage_cell(count, sizeof(char *));
            lines->length = (unsigned long long *)stage_cell(count, sizeof(unsigned long long));
            ok = (lines->line != NULL) && (lines->length != NULL);
        }
        lines->count = count;
    }
    return ok;
}

static void stage_lines_released(StageLines *lines)
{
    stage_cell_released(lines->length);
    stage_cell_released(lines->line);
    stage_cell_released(lines->bytes);
    lines->line = NULL;
    lines->bytes = NULL;
}

// A line's tab-separated fields, split in place: field f is the terminated text at field[f]. `count` counts every
// field, and field holds the first STAGE_LINE_FIELDS of them
typedef struct
{
    char *field[STAGE_LINE_FIELDS];
    unsigned int count;
} StageFields;

static void stage_split(char *line, StageFields *fields)
{
    fields->count = 0u;
    char *start = line;
    for (;;)
    {
        if (fields->count < STAGE_LINE_FIELDS)
        {
            fields->field[fields->count] = start;
        }
        fields->count += 1u;
        char *const tab = (char *)EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = start,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
        if (tab == NULL)
        {
            return;
        }
        *tab = '\0';
        start = tab + 1;
    }
}

// a field's count where the line holds `wanted` fields, and 0 where it holds another count or more than
// STAGE_LINE_FIELDS
static int stage_fields_are(const StageFields *fields, unsigned int wanted)
{
    return (fields->count == wanted) && (wanted <= STAGE_LINE_FIELDS);
}

// a name cut to `width` characters and padded to them, counting a UTF-8 character once and not by its bytes
static StageText stage_displayed(const char *text, unsigned int width, int pad)
{
    StageText displayed;
    VerbaLinea line = mmgr_line_over(displayed.bytes, sizeof(displayed.bytes));
    unsigned int characters = 0u;
    // each byte is a step of the count of characters: a byte that does not continue a character starts one
    for (const char *at = text; *at != '\0'; at += 1)
    {
        const int starts = (((unsigned char)*at) & 0xC0u) != 0x80u;
        if (starts && (characters == width))
        {
            break;
        }
        characters += (unsigned int)starts;
        mmgr_line_ch(&line, *at);
    }
    while (pad && (characters < width))
    {
        mmgr_line_ch(&line, ' ');
        characters += 1u;
    }
    (void)mmgr_line_finish(&line);
    return displayed;
}

// how many bits of a 32-bit word are set, by the halving sums of adjacent fields. Where `masked`, the word is
// ANDed with a second 32-bit field first
static void stage_count_program(int masked, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int shuffled = stage_field(program, bits);
    const unsigned int observed = stage_field(program, bits);
    const unsigned int sign = stage_step(program, ENGINE_RECORD_COMPARE, shuffled, observed);
    const unsigned int raised = stage_step(program, ENGINE_RECORD_SUM, sign, stage_constant(program, 2ull));
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, raised, stage_constant(program, 2ull)));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, shuffled, stage_constant(program, 0ull)));
}

// The exact sum of each run of `group` lanes of a stage's output, on the device and on the host, checked equal. The
// sums go to `sums`, lanes / group of them
static int stage_sums(SimResults *job, const char *what, const unsigned int *device_out, const unsigned int *host_out,
                      unsigned long long lanes, unsigned long long group, unsigned int out_limbs,
                      const DeviceRecordStep *step, StageWhole *sums, EngineError *error)
{
    const unsigned int sum_limbs = ((step->out_bits + stage_bits(stage_whole(group)) + 31u) / 32u) + 1u;
    const unsigned long long runs = lanes / group;
    unsigned int *const device_sums = (unsigned int *)stage_cell(runs * sum_limbs, sizeof(unsigned int));
    unsigned int *const host_sums = (unsigned int *)stage_cell(runs * sum_limbs, sizeof(unsigned int));
    int ok = (device_sums != NULL) && (host_sums != NULL);
    const CycleRecordSumRequest on_device = {device_out,     lanes,     group,       out_limbs, step->out_offset,
                                             step->out_bits, sum_limbs, device_sums, error};
    const CycleRecordSumRequest on_host = {host_out,       lanes,     group,     out_limbs, step->out_offset,
                                           step->out_bits, sum_limbs, host_sums, error};
    ok = ok && (cycle_record_sum(&on_device) != CYCLE_ERROR) && (cycle_record_sum_host(&on_host) != CYCLE_ERROR);
    ok = ok && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device_sums, .other = host_sums,
                           .bytes = runs * sum_limbs * sizeof(unsigned int)) == 0);
    stage_check(job, ok, "the device's sums of ", what, " equal the host's");
    for (unsigned long long run = 0ull; ok && (run < runs); run += 1ull)
    {
        sums[run] = stage_whole_of(&device_sums[run * sum_limbs], sum_limbs);
    }
    stage_cell_released(host_sums);
    stage_cell_released(device_sums);
    return ok && stage_fits(what);
}

// Positions a stage out over prepared records, sweeps it, and sums each of `output_count` outputs over runs of `group`
// lanes. Row o of *sums holds output o's sums, a field a run
static int stage_summed(SimResults *job, Stage *stage, const unsigned int *records, unsigned long long record_words,
                        unsigned long long lanes, unsigned long long group, const unsigned int *outputs,
                        unsigned int output_count, StageTable *sums, EngineError *error)
{
    unsigned int *out = NULL;
    unsigned int *device_out = NULL;
    int ok = stage_sweep(job, stage, records, record_words, lanes, &out, &device_out, error);
    ok = stage_table_taken(sums, output_count, (unsigned int)(lanes / group)) && ok;
    for (unsigned int at = 0u; ok && (at < output_count); at += 1u)
    {
        const DeviceRecordStep *step = &stage->layout.step_table[stage->program.outputs[outputs[at]]];
        ok = stage_sums(job, stage->name, device_out, out, lanes, group, stage->layout.out_limbs, step,
                        stage_row(sums, at), error);
    }
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }
    stage_cell_released(out);
    return ok;
}

#endif
