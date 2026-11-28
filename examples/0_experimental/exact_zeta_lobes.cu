// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-027
//
// The curves R contains at every point of a cell, and the logarithm the phase turns by, one lane a point, swept on the
// device as two programs of the record machine, one job on the device's tessera daemon. Every value is a mantissa in a
// register and a binary exponent the program stores: a product multiplies the mantissas and adds the exponents, a power
// of two is where a mantissa is positioned in the record, and nothing is divided.
//
//   Usage:  exact_zeta_lobes <input> <output>
//
// Lane l is at x = nu + l / 2^b, X = nu 2^b + l, and z = 1 - 2p = Zt / 2^b with Zt = 2^b - 2l. C_n(z) is
// sum over j of g_(n,j) z^j, each g a mantissa at the exponent -E, read by Horner's rule as the integer
// H_n = sum over j of g_(n,j) Zt^j 2^(b (J_n - 1 - j)) at the exponent -(E + b (J_n - 1)). The record contains each
// g_(n,j) 2^(b (J_n - 1 - j)), the mantissa positioned b (J_n - 1 - j) bits up with zeros below, and each step of Horner's
// rule is then one product by Zt and one sum. With the sign s = (-1)^(nu - 1),
// the curve n's part of R x^(1/2) is s C_n(z) x^(-n) = s H_n 2^(b n) / X^n, and over the common denominator X^K every
// curve is the integer T_n = s H_n 2^(b n) X^(K - n), each at its own exponent, each stored as an output.
//
// The logarithm: ln(x / m) = ln(nu / m) + A for every m, with A = ln(X / (nu 2^b)) = 2 artanh(l / D) and
// D = X + nu 2^b: one series a lane serves every term of the main sum, and ln(nu / m) is the cell's constant. With
// c_k = Lambda / (2k + 1), Lambda the least common multiple of the odd numbers below 2L, the first L terms of the
// series are 2 l S / (Lambda D^(2L - 1)) with S = sum over k < L of c_k l^(2k) D^(2(L - 1 - k)), an integer read by
// Horner's rule in l^2 against the powers of D^2. Every term is positive and each is below (l / D)^2 times the one
// before it: the tail past the last term is positive and below 2 l^(2L + 1) / ((2L + 1) D^(2L - 1) (D^2 - l^2)):
// A lies at or above the first quotient and below the first plus the second, and the stage stores the four integers
// l S, D^(2L - 1), l^(2L + 1) and D^2 - l^2 as its outputs.
//
// The input, little-endian: 64-bit words lanes, checked, nu, b, K, E, L, then K + 1 words J_n, then the mantissas
// g_(n,j), then the L constants c_k, each a 64-bit word count w and w 32-bit limbs of its magnitude least significant
// first, and a 64-bit sign word, -1, 0 or 1.
//
// The output, a stage at a time, R's curves and then the logarithm: the line "stage name out_limbs L", one line an
// output "output name offset bits exponent", one line a lane of L hex limbs, least significant first, the line
// "host 1" where the host's records over the first `checked` lanes equal the device's word for word, else "host 0",
// and the line "steps P compiled C".

#include "../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../src/cu/engine/analysis/keymath/keymath.h"
#include "memoria_operor/memoria_operor.h"
#include "sim.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>

// The program: its steps, `count` of them, and its fields' bits and offsets, `field_count` of them. The first build
// takes no cell and counts them. The second fills the cells of the persistent tier at those counts. `full` is 1 where
// the second build passed a count of the first
typedef struct
{
    EngineRecordStep *steps;
    unsigned int count;
    unsigned int room;
    unsigned int *field_bits;
    unsigned int *field_offset;
    unsigned int field_count;
    unsigned int field_room;
    unsigned int shared_bits;
    int full;
} LobesProgram;

// the bytes of the text of a check
#define LOBES_CHECK_BYTES 128u

// a value: the register storing its mantissa and the binary exponent the program stores for it
typedef struct
{
    unsigned int reg;
    long long exponent;
} LobesReal;

// the name of an output and its binary exponent. The name is `name`, and `index` follows it where `index` is 0 or more
typedef struct
{
    const char *name;
    long long index;
    long long exponent;
} LobesName;

// a mantissa of the input: its magnitude's limbs, `limbs` of them in a cell of the persistent tier, its sign, and the
// signed width that holds it
typedef struct
{
    unsigned int *magnitude;
    unsigned int limbs;
    long long sign;
    unsigned int bits;
} LobesMantissa;

// A program, the outputs it lists with their names and exponents, its shared record, and what the device made of it.
// The outputs and their names are cells of the persistent tier at the count of the first build, `output_count` of
// them. The shared record and the records are cells of the temporary tier, `shared_count` and `record_count` words
typedef struct
{
    const char *name;
    LobesProgram program;
    unsigned int *outputs;
    LobesName *names;
    unsigned int output_count;
    unsigned int output_room;
    unsigned int *shared;
    unsigned int shared_count;
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
    unsigned int *records;
    unsigned long long record_count;
    int same;
} LobesStage;

// a cell of `count` elements of `size` bytes, cleared, on the temporary tier where `ephemeral` is 1. NULL where the
// pool cannot meet it
static void *lobes_cleared(unsigned long long count, unsigned long long size, int ephemeral)
{
    return platform_cell_cleared((count != 0ull) ? count : 1ull, size, platform_place_g_program_owner, ephemeral);
}

// a check whose text is `front`, the stage's name and `back`, in a line of a fixed bound
static void lobes_check(SimResults *job, int passed, const char *front, const char *name, const char *back)
{
    char text[LOBES_CHECK_BYTES];
    VerbaLinea line = mmgr_line_over(text, sizeof(text));
    mmgr_line_put(&line, front);
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, back);
    sim_check(job, passed, text);
}

static unsigned int lobes_step(LobesProgram *program, EngineRecordOperation operation, unsigned int left,
                               unsigned int right, unsigned int member)
{
    if (program->steps != NULL)
    {
        if (program->count >= program->room)
        {
            program->full = 1;
            return 0u;
        }
        program->steps[program->count] = EngineRecordStep{operation, left, right, member};
    }
    program->count += 1u;
    return program->count - 1u;
}

static unsigned int lobes_constant(LobesProgram *program, unsigned long long value)
{
    return lobes_step(program, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFull), (unsigned int)(value >> 32u),
                      0u);
}

// a field of the shared record, `bits` wide and signed, positioned at the record's next bit. The step that reads it is
// written where it is read, and the register is live only from there to its reader
static unsigned int lobes_field(LobesProgram *program, unsigned int bits)
{
    const unsigned int field = program->field_count;
    if (program->field_bits != NULL)
    {
        if (field >= program->field_room)
        {
            program->full = 1;
            return 0u;
        }
        program->field_bits[field] = bits;
        program->field_offset[field] = program->shared_bits;
    }
    program->field_count += 1u;
    program->shared_bits += bits;
    return field;
}

static unsigned int lobes_read(LobesProgram *program, unsigned int field)
{
    return lobes_step(program, ENGINE_RECORD_FIELD_SIGNED, field, 0u, 0u);
}

// the output `step`, its name `name` and `index`, and its exponent, put at the end of the stage's outputs
static void lobes_output(LobesStage *stage, const char *name, long long index, unsigned int step, long long exponent)
{
    if (stage->outputs != NULL)
    {
        if (stage->output_count >= stage->output_room)
        {
            stage->program.full = 1;
            return;
        }
        stage->outputs[stage->output_count] = step;
        stage->names[stage->output_count] = LobesName{name, index, exponent};
    }
    stage->output_count += 1u;
}

// The cells of a stage at the counts of its first build, and its counts set back to 0 for the second build to fill
// them. 1, or 0 where the pool cannot meet a cell, and then the program is full
static int lobes_stage_taken(LobesStage *stage)
{
    LobesProgram *const program = &stage->program;
    program->room = program->count;
    program->field_room = program->field_count;
    stage->output_room = stage->output_count;
    program->steps = (EngineRecordStep *)lobes_cleared(program->room, sizeof(EngineRecordStep), 0);
    program->field_bits = (unsigned int *)lobes_cleared(program->field_room, sizeof(unsigned int), 0);
    program->field_offset = (unsigned int *)lobes_cleared(program->field_room, sizeof(unsigned int), 0);
    stage->outputs = (unsigned int *)lobes_cleared(stage->output_room, sizeof(unsigned int), 0);
    stage->names = (LobesName *)lobes_cleared(stage->output_room, sizeof(LobesName), 0);
    program->count = 0u;
    program->field_count = 0u;
    stage->output_count = 0u;
    const int taken = (program->steps != NULL) && (program->field_bits != NULL) && (program->field_offset != NULL) &&
                      (stage->outputs != NULL) && (stage->names != NULL);
    program->full = program->full || !taken;
    return taken;
}

// the lane, at max 2^b, and the and with 2^(b + 1) - 1 gives its register that width and preserves its value
static unsigned int lobes_lane(LobesProgram *program, unsigned int b)
{
    const unsigned int counted = lobes_step(program, ENGINE_RECORD_LANE, 0u, 0u, 0u);
    return lobes_step(program, ENGINE_RECORD_AND, counted, lobes_constant(program, (2ull << b) - 1ull), 0u);
}

// The curves: T_n for n from 0 to K over the denominator X^K. The mantissas of curve n are mantissas[starts[n]] up to
// mantissas[starts[n + 1]], and their fields follow each other in that order
static void lobes_build(LobesStage *stage, unsigned int b, long long big_e, const LobesMantissa *mantissas,
                        const unsigned long long *starts, unsigned int curves)
{
    LobesProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int sign_field = lobes_field(program, 2u);
    // nu 2^b, read from the record at a width b alone sets: one program serves every cell up to nu = 127
    const unsigned int floor_field = lobes_field(program, b + 8u);
    const unsigned int gamma_first = program->field_count;
    for (unsigned int n = 0u; n < curves; n += 1u)
    {
        const unsigned int count = (unsigned int)(starts[n + 1u] - starts[n]);
        for (unsigned int j = 0u; j < count; j += 1u)
        {
            lobes_field(program, mantissas[starts[n] + j].bits + b * (count - 1u - j));
        }
    }
    const unsigned int lane = lobes_lane(program, b);
    const unsigned int big_x = lobes_step(program, ENGINE_RECORD_SUM, lane, lobes_read(program, floor_field), 0u);
    const unsigned int doubled = lobes_step(program, ENGINE_RECORD_SUM, lane, lane, 0u);
    const unsigned int zt = lobes_step(program, ENGINE_RECORD_DIFFERENCE, lobes_constant(program, 1ull << b), doubled, 0u);
    const LobesReal z = {zt, -(long long)b};
    const unsigned int top = curves - 1u;
    // the powers of X, a cell of the temporary tier that the call gives back
    unsigned int *const x_power = (unsigned int *)lobes_cleared(curves, sizeof(unsigned int), 1);
    if (x_power == NULL)
    {
        program->full = 1;
        return;
    }
    x_power[0] = lobes_constant(program, 1ull);
    for (unsigned int n = 1u; n < curves; n += 1u)
    {
        x_power[n] = lobes_step(program, ENGINE_RECORD_PRODUCT, x_power[n - 1u], big_x, 0u);
    }
    for (unsigned int n = 0u; n < curves; n += 1u)
    {
        const unsigned int count = (unsigned int)(starts[n + 1u] - starts[n]);
        const unsigned int gamma = gamma_first + (unsigned int)starts[n];
        unsigned int acc = lobes_read(program, gamma + count - 1u);
        for (unsigned int j = count - 1u; j > 0u; j -= 1u)
        {
            const unsigned int turned = lobes_step(program, ENGINE_RECORD_PRODUCT, acc, z.reg, 0u);
            acc = lobes_step(program, ENGINE_RECORD_SUM, turned, lobes_read(program, gamma + j - 1u), 0u);
        }
        const LobesReal lobe = {acc, -big_e - (long long)b * (long long)(count - 1u)};
        // s H_n 2^(b n) X^(K - n): the 2^(b n) is the exponent's, the rest the mantissa's
        const unsigned int sign = lobes_read(program, sign_field);
        const unsigned int signed_h = lobes_step(program, ENGINE_RECORD_PRODUCT, lobe.reg, sign, 0u);
        lobes_output(stage, "curve", (long long)n,
                     lobes_step(program, ENGINE_RECORD_PRODUCT, signed_h, x_power[top - n], 0u),
                     lobe.exponent + (long long)b * n);
    }
    lobes_output(stage, "denominator", -1, x_power[top], 0);
    platform_cell_released(x_power, platform_place_g_program_owner);
}

// The logarithm: l S, D^(2L - 1), l^(2L + 1) and D^2 - l^2, from the widths of the constants c_k. Every power of D
// is as wide as the field nu 2^b is read at. The field is as wide as nu's own bits set, and one program serves every
// nu of that many bits. The fields of c_k follow each other in the order of k
static void lobes_log_build(LobesStage *stage, unsigned int b, unsigned int nu_bits, const LobesMantissa *c_mantissas,
                            unsigned int terms)
{
    LobesProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int floor_field = lobes_field(program, b + nu_bits + 1u);
    const unsigned int c = program->field_count;
    for (unsigned int k = 0u; k < terms; k += 1u)
    {
        lobes_field(program, c_mantissas[k].bits);
    }
    const unsigned int lane = lobes_lane(program, b);
    const unsigned int big_x = lobes_step(program, ENGINE_RECORD_SUM, lane, lobes_read(program, floor_field), 0u);
    const unsigned int big_d = lobes_step(program, ENGINE_RECORD_SUM, big_x, lobes_read(program, floor_field), 0u);
    const unsigned int u = lobes_step(program, ENGINE_RECORD_PRODUCT, lane, lane, 0u);
    const unsigned int v = lobes_step(program, ENGINE_RECORD_PRODUCT, big_d, big_d, 0u);
    // Horner's rule from c_(L-1) down: the step that adds c_(k-1) turns the sum by l^2 and positions c_(k-1) at D^(2(L - k))
    unsigned int acc = lobes_read(program, c + terms - 1u);
    unsigned int v_power = v;
    for (unsigned int k = terms - 1u; k > 0u; k -= 1u)
    {
        const unsigned int turned = lobes_step(program, ENGINE_RECORD_PRODUCT, acc, u, 0u);
        const unsigned int positioned =
            lobes_step(program, ENGINE_RECORD_PRODUCT, lobes_read(program, c + k - 1u), v_power, 0u);
        acc = lobes_step(program, ENGINE_RECORD_SUM, turned, positioned, 0u);
        if (k > 1u)
        {
            v_power = lobes_step(program, ENGINE_RECORD_PRODUCT, v_power, v, 0u);
        }
    }
    lobes_output(stage, "numerator", -1, lobes_step(program, ENGINE_RECORD_PRODUCT, lane, acc, 0u), 0);
    lobes_output(stage, "power", -1, lobes_step(program, ENGINE_RECORD_PRODUCT, big_d, v_power, 0u), 0);
    unsigned int tail = lane;
    for (unsigned int k = 0u; k < terms; k += 1u)
    {
        tail = lobes_step(program, ENGINE_RECORD_PRODUCT, tail, u, 0u);
    }
    lobes_output(stage, "tail", -1, tail, 0);
    lobes_output(stage, "gap", -1, lobes_step(program, ENGINE_RECORD_DIFFERENCE, v, u, 0u), 0);
}

static int lobes_word(FILE *in, long long *value)
{
    return fread(value, sizeof(long long), 1u, in) == 1u;
}

// a mantissa of the input: its magnitude's limbs, its sign, and the signed width that represents it
static int lobes_mantissa(FILE *in, LobesMantissa *mantissa)
{
    long long limbs = 0;
    int read = lobes_word(in, &limbs) && (limbs >= 0) && (limbs < (1 << 20));
    // the magnitude, a cell of the persistent tier at the count the input gives: the program reads it once and keeps it
    mantissa->limbs = read ? (unsigned int)limbs : 0u;
    mantissa->magnitude = (unsigned int *)lobes_cleared(mantissa->limbs, sizeof(unsigned int), 0);
    read = read && (mantissa->magnitude != NULL) &&
           (fread(mantissa->magnitude, sizeof(unsigned int), mantissa->limbs, in) == mantissa->limbs);
    read = read && lobes_word(in, &mantissa->sign);
    unsigned int width = read ? (32u * mantissa->limbs) : 0u;
    while ((width > 0u) && (((mantissa->magnitude[(width - 1u) / 32u] >> ((width - 1u) % 32u)) & 1u) == 0u))
    {
        width -= 1u;
    }
    mantissa->bits = width + 1u;
    return read;
}

// a magnitude's `count` limbs moved `shift` bits up, zeros below, into `out`. `out` holds count + shift / 32 + 1
// words, each 0 before the call
static void lobes_shifted(const unsigned int *limbs, unsigned int count, unsigned int shift, unsigned int *out)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long moved = (unsigned long long)limbs[at] << (shift % 32u);
        out[at + shift / 32u] |= (unsigned int)moved;
        out[at + shift / 32u + 1u] |= (unsigned int)(moved >> 32u);
    }
}

// `bits` of a two's complement value, its magnitude's `limb_count` limbs given, positioned into the record at `offset`
static void lobes_put(unsigned int *record, unsigned int offset, unsigned int bits, const unsigned int *limbs,
                      unsigned int limb_count, long long sign)
{
    // the value's words, a cell of the temporary tier at the width the field takes, that the call gives back
    const unsigned int words = (bits + 31u) / 32u + 1u;
    unsigned int *const word = (unsigned int *)lobes_cleared(words, sizeof(unsigned int), 1);
    if (word == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; (at < limb_count) && (at < words); at += 1u)
    {
        word[at] = limbs[at];
    }
    if (sign < 0)
    {
        unsigned long long carry = 1ull;
        for (unsigned int at = 0u; at < words; at += 1u)
        {
            const unsigned int flipped = ~word[at];
            carry += (unsigned long long)flipped;
            word[at] = (unsigned int)carry;
            carry >>= 32u;
        }
    }
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int to = offset + bit;
        const unsigned int value = (word[bit / 32u] >> (bit % 32u)) & 1u;
        record[to / 32u] = (record[to / 32u] & ~(1u << (to % 32u))) | (value << (to % 32u));
    }
    platform_cell_released(word, platform_place_g_program_owner);
}

// the record a stage's fields are positioned into, sized to them, a cell of the temporary tier, 0 before it fills
static void lobes_open_shared(LobesStage *stage)
{
    stage->shared_count = (stage->program.shared_bits + 31u) / 32u + 1u;
    stage->shared = (unsigned int *)lobes_cleared(stage->shared_count, sizeof(unsigned int), 1);
    stage->program.full = stage->program.full || (stage->shared == NULL);
}

// nu 2^b into the stage's field `field`
static void lobes_put_floor(LobesStage *stage, unsigned int field, unsigned long long nu, unsigned int b)
{
    const unsigned long long floor_value = nu << b;
    const unsigned int floor_limbs[2] = {(unsigned int)floor_value, (unsigned int)(floor_value >> 32u)};
    lobes_put(stage->shared, stage->program.field_offset[field], stage->program.field_bits[field], floor_limbs, 2u, 1);
}

static void lobes_open_stage(LobesStage *stage, const char *name)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
    stage->name = name;
    stage->record = NULL;
}

// imprints and arranges a stage's program, and reports where the imprint stops when it does
static int lobes_position(SimResults *job, LobesStage *stage, EngineError *error)
{
    LobesProgram *const program = &stage->program;
    const unsigned int fields = program->field_count;
    const KeymathRecordRequest encode = {
        program->steps, program->count, program->field_bits, fields, 1u, stage->outputs, stage->output_count, NULL, 0u,
        &stage->key,    error};
    int ok = !program->full && (keymath_record_encode(&encode) != KEYMATH_ERROR);
    if (!ok && !program->full)
    {
        const EngineRecordStep *const first = program->steps;
        const EngineRecordStep *const last = first + program->count;
        const EngineRecordStep *const at = (const EngineRecordStep *)error->evacaddr;
        const long long step = ((at >= first) && (at < last)) ? (long long)(at - first) : -1;
        fprintf(
            stderr,
            "  exact_zeta_lobes: the %s imprint errors, site %u, at step %lld of %u, operation %d left %u right %u\n",
            stage->name, error->site, step, program->count, (step >= 0) ? (int)at->operation : -1,
            (step >= 0) ? at->left : 0u, (step >= 0) ? at->right : 0u);
    }
    lobes_check(job, ok, "keymath imprints the ", stage->name, " program");
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {(program->shared_bits + 31u) / 32u, 0u, 0u};
    const KeyScheduleRecordRequest position = {&stage->key, program->field_offset, fields, in_limbs,
                                               1,           &stage->layout,        error};
    const int positioned = ok && (key_schedule_record_layout(&position) != KEY_SCHEDULE_ERROR);
    if (ok && !positioned)
    {
        const char *const end = (error->evacaddr == (const void *)&stage->key)       ? "the register file"
                                : (error->evacaddr == (const void *)stage->key.output) ? "the outputs"
                                                                                        : "a step";
        const long long step = (end[0] == 'a') ? (long long)((const EngineRecordTerm *)error->evacaddr - stage->key.term)
                                               : -1;
        fprintf(stderr, "  exact_zeta_lobes: the %s layout ends at %s, step %lld of %u\n", stage->name, end, step,
                program->count);
    }
    ok = positioned;
    lobes_check(job, ok, "the scheduler positions the ", stage->name, " program out");
    stage->shared_count = (program->shared_bits + 31u) / 32u;
    return ok;
}

// the bytes a stage occupies on the device over `lanes` lanes
static unsigned long long lobes_declared(const LobesStage *stage, unsigned long long lanes)
{
    return lanes * stage->layout.out_limbs * sizeof(unsigned int) + stage->shared_count * sizeof(unsigned int) +
           (unsigned long long)stage->layout.steps * sizeof(DeviceRecordStep);
}

// loads a stage onto the device, runs every lane, and checks the host's first `checked` records against the device's
static int lobes_sweep(SimResults *job, LobesStage *stage, unsigned long long lanes, unsigned long long checked,
                       EngineError *error)
{
    const char *const name = stage->name;
    int ok = cycle_record_load(&stage->layout, &stage->record, error) != CYCLE_ERROR;
    lobes_check(job, ok, "the ", name, " program loads onto the device");
    const unsigned int out_limbs = stage->layout.out_limbs;
    unsigned int *device_shared = NULL;
    unsigned int *device_out = NULL;
    ok = ok && (cudaMalloc((void **)&device_shared, stage->shared_count * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, lanes * out_limbs * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_shared, stage->shared, stage->shared_count * sizeof(unsigned int),
                     cudaMemcpyHostToDevice) == cudaSuccess);
    const CycleRecordRunRequest run = {stage->record, {device_shared, NULL, NULL}, {1ull, 0ull, 0ull}, NULL, lanes,
                                       device_out, error};
    ok = ok && (cycle_record_run(&run) == (long)lanes);
    lobes_check(job, ok, "every lane of the cell runs the ", name, " program on the device");
    // the records and the host's records, cells of the temporary tier, each 0 before it fills
    stage->record_count = ok ? (lanes * out_limbs) : 0ull;
    stage->records = (unsigned int *)lobes_cleared(stage->record_count, sizeof(unsigned int), 1);
    unsigned int *const host_out = (unsigned int *)lobes_cleared(checked * out_limbs, sizeof(unsigned int), 1);
    ok = ok && (stage->records != NULL) && (host_out != NULL) &&
         (cudaMemcpy(stage->records, device_out, stage->record_count * sizeof(unsigned int), cudaMemcpyDeviceToHost) ==
          cudaSuccess);
    const CycleRecordHostRequest host = {
        &stage->layout, {stage->shared, NULL, NULL}, {1ull, 0ull, 0ull}, NULL, checked, host_out, error};
    stage->same = ok && (cycle_record_run_host(&host) == (long)checked) &&
                  (EMBED_CALL(memor.cmp, MemoriaCfg, .src = host_out, .other = stage->records,
                              .bytes = checked * out_limbs * sizeof(unsigned int)) == 0);
    lobes_check(job, stage->same, "the host's ", name, " records equal the device's word for word");
    platform_cell_released(host_out, platform_place_g_program_owner);
    cudaFree(device_shared);
    cudaFree(device_out);
    return ok;
}

static void lobes_write(FILE *out, const LobesStage *stage, unsigned long long lanes)
{
    const unsigned int out_limbs = stage->layout.out_limbs;
    fprintf(out, "stage %s out_limbs %u\n", stage->name, out_limbs);
    for (unsigned int at = 0u; at < stage->output_count; at += 1u)
    {
        const DeviceRecordStep *const step = &stage->layout.step_table[stage->outputs[at]];
        const LobesName *const name = &stage->names[at];
        fprintf(out, "output %s", name->name);
        if (name->index >= 0)
        {
            fprintf(out, "%lld", name->index);
        }
        fprintf(out, " %u %u %lld\n", step->out_offset, step->out_bits, name->exponent);
    }
    for (unsigned long long lane = 0ull; lane < lanes; lane += 1ull)
    {
        for (unsigned int limb = 0u; limb < out_limbs; limb += 1u)
        {
            fprintf(out, (limb == 0u) ? "%x" : " %x", stage->records[(size_t)(lane * out_limbs + limb)]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "host %d\nsteps %u compiled %d\n", stage->same, stage->layout.steps,
            (stage->record != NULL) ? cycle_record_compiled(stage->record) : 0);
}

static void lobes_release(LobesStage *stage)
{
    if (stage->record != NULL)
    {
        cycle_record_release(stage->record);
    }
    key_schedule_record_release(&stage->layout);
    keymath_record_release(&stage->key);
    void *const cells[7] = {
        stage->records, stage->shared, stage->program.steps, stage->program.field_bits, stage->program.field_offset,
        stage->outputs, stage->names};
    for (unsigned int each = 0u; each < 7u; each += 1u)
    {
        platform_cell_released(cells[each], platform_place_g_program_owner);
    }
}

int program_main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if (count != 3)
    {
        fprintf(stderr, "  usage: exact_zeta_lobes <input> <output>\n");
        return 2;
    }
    FILE *in = fopen(arguments[1], "rb");
    long long header[7] = {0, 0, 0, 0, 0, 0, 0};
    int read = (in != NULL);
    for (unsigned int at = 0u; read && (at < 7u); at += 1u)
    {
        read = lobes_word(in, &header[at]);
    }
    read = read && (header[0] > 0) && (header[1] > 0) && (header[1] <= header[0]) && (header[2] > 0) &&
           (header[2] < 128) && (header[3] > 0) && (header[3] <= 48) && (header[0] <= (2ll << header[3])) &&
           (header[4] >= 0) && (header[5] >= 0) && (header[6] >= 2) && (header[6] < 4096);
    const unsigned long long lanes = read ? (unsigned long long)header[0] : 0ull;
    const unsigned long long checked = read ? (unsigned long long)header[1] : 0ull;
    const unsigned long long nu = read ? (unsigned long long)header[2] : 0ull;
    const unsigned int b = read ? (unsigned int)header[3] : 0u;
    const unsigned int curves = read ? (unsigned int)header[4] + 1u : 0u;
    const long long big_e = read ? header[5] : 0;
    const unsigned int log_terms = read ? (unsigned int)header[6] : 0u;
    // The input, cells of the persistent tier at the counts the input gives: the program reads it once and keeps it to
    // its end. The mantissas of curve n are mantissas[starts[n]] up to mantissas[starts[n + 1]]
    unsigned long long *const starts =
        (unsigned long long *)lobes_cleared(curves + 1ull, sizeof(unsigned long long), 0);
    read = read && (starts != NULL);
    for (unsigned int n = 0u; read && (n < curves); n += 1u)
    {
        long long terms = 0;
        read = lobes_word(in, &terms) && (terms > 0);
        starts[n + 1u] = starts[n] + (read ? (unsigned long long)terms : 0ull);
    }
    const unsigned long long mantissa_count = read ? starts[curves] : 0ull;
    LobesMantissa *const mantissas = (LobesMantissa *)lobes_cleared(mantissa_count, sizeof(LobesMantissa), 0);
    LobesMantissa *const c_mantissas = (LobesMantissa *)lobes_cleared(log_terms, sizeof(LobesMantissa), 0);
    read = read && (mantissas != NULL) && (c_mantissas != NULL);
    for (unsigned long long at = 0ull; read && (at < mantissa_count); at += 1ull)
    {
        read = lobes_mantissa(in, &mantissas[at]);
    }
    for (unsigned int k = 0u; read && (k < log_terms); k += 1u)
    {
        read = lobes_mantissa(in, &c_mantissas[k]);
    }
    if (in != NULL)
    {
        fclose(in);
    }
    if (!read)
    {
        fprintf(stderr, "  exact_zeta_lobes: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // each program has two builds: the first counts its steps, fields and outputs, and the second fills cells at those
    // counts
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    LobesStage curve_stage;
    lobes_open_stage(&curve_stage, "curves");
    lobes_build(&curve_stage, b, big_e, mantissas, starts, curves);
    if (lobes_stage_taken(&curve_stage))
    {
        lobes_build(&curve_stage, b, big_e, mantissas, starts, curves);
    }
    lobes_open_shared(&curve_stage);
    if (!curve_stage.program.full)
    {
        const unsigned int one_limb[1] = {1u};
        lobes_put(curve_stage.shared, curve_stage.program.field_offset[0], curve_stage.program.field_bits[0], one_limb,
                  1u, ((nu - 1ull) % 2ull) ? -1 : 1);
        lobes_put_floor(&curve_stage, 1u, nu, b);
        unsigned int field = 2u;
        for (unsigned int n = 0u; n < curves; n += 1u)
        {
            const unsigned int magnitude_count = (unsigned int)(starts[n + 1u] - starts[n]);
            for (unsigned int j = 0u; j < magnitude_count; j += 1u, field += 1u)
            {
                // the magnitude moved up, a cell of the temporary tier that the step gives back
                const LobesMantissa *const mantissa = &mantissas[starts[n] + j];
                const unsigned int shift = b * (magnitude_count - 1u - j);
                const unsigned int shifted_count = mantissa->limbs + shift / 32u + 1u;
                unsigned int *const positioned = (unsigned int *)lobes_cleared(shifted_count, sizeof(unsigned int), 1);
                curve_stage.program.full = curve_stage.program.full || (positioned == NULL);
                if (positioned != NULL)
                {
                    lobes_shifted(mantissa->magnitude, mantissa->limbs, shift, positioned);
                    lobes_put(curve_stage.shared, curve_stage.program.field_offset[field],
                              curve_stage.program.field_bits[field], positioned, shifted_count, mantissa->sign);
                }
                platform_cell_released(positioned, platform_place_g_program_owner);
            }
        }
    }

    LobesStage log_stage;
    lobes_open_stage(&log_stage, "log");
    unsigned int nu_bits = 0u;
    while ((nu >> nu_bits) != 0ull)
    {
        nu_bits += 1u;
    }
    lobes_log_build(&log_stage, b, nu_bits, c_mantissas, log_terms);
    if (lobes_stage_taken(&log_stage))
    {
        lobes_log_build(&log_stage, b, nu_bits, c_mantissas, log_terms);
    }
    lobes_open_shared(&log_stage);
    if (!log_stage.program.full)
    {
        lobes_put_floor(&log_stage, 0u, nu, b);
        for (unsigned int k = 0u; k < log_terms; k += 1u)
        {
            lobes_put(log_stage.shared, log_stage.program.field_offset[1u + k], log_stage.program.field_bits[1u + k],
                      c_mantissas[k].magnitude, c_mantissas[k].limbs, c_mantissas[k].sign);
        }
    }

    int ok = lobes_position(&job, &curve_stage, &error);
    ok = ok && lobes_position(&job, &log_stage, &error);
    const unsigned long long declared =
        lobes_declared(&curve_stage, lanes) + lobes_declared(&log_stage, lanes) + (256ull << 20u);
    ok = ok && sim_job_submit(&job, "exact_zeta_lobes", count, arguments, declared);
    ok = ok && lobes_sweep(&job, &curve_stage, lanes, checked, &error);
    ok = ok && lobes_sweep(&job, &log_stage, lanes, checked, &error);

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        lobes_write(out, &curve_stage, lanes);
        lobes_write(out, &log_stage, lanes);
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    lobes_release(&curve_stage);
    lobes_release(&log_stage);
    for (unsigned long long at = 0ull; at < mantissa_count; at += 1ull)
    {
        platform_cell_released(mantissas[at].magnitude, platform_place_g_program_owner);
    }
    for (unsigned int k = 0u; k < log_terms; k += 1u)
    {
        platform_cell_released(c_mantissas[k].magnitude, platform_place_g_program_owner);
    }
    void *const cells[3] = {starts, mantissas, c_mantissas};
    for (unsigned int each = 0u; each < 3u; each += 1u)
    {
        platform_cell_released(cells[each], platform_place_g_program_owner);
    }
    return sim_close(&job, "exact_zeta_lobes");
}
