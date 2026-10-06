// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Proves exact_record, the exact integer's arithmetic as record programs: each program is built through the API, run
// on the device and on the host over random lanes, and the two must agree word for word and equal the values the
// test works out in limbs:
// - base^e of a register base below 2^8 and e below 16, by square and multiply over e's bits;
// - 2^e and 3^e of a register e, by the nibble table, e below 2^8 and below 2^6;
// - the outward pair of a quotient, l / r less and more 1, for signed l and r;
// - the two's complement wrap and narrow, a bit of a signed value, [v > w], [v == w], [v == v] and the larger of the
//   two by select.
// The test is one job on the device's tessera daemon.
#include "../../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../../../../../../src/cu/types/integers/exact_record/exact_record.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

#define RECORD_TEST_LINE 8192ull

#define RECORD_TEST_LANES 4096u

// the widest value a case reads back, in limbs: 2^255 and 255^15 fit
#define RECORD_TEST_VALUE_LIMBS 9u

#define RECORD_TEST_OUTPUTS 8u

// the most the test puts on the device at once: a sweep's atoms and its records, each well under a kilobyte a lane
#define RECORD_TEST_DECLARED ((unsigned long long)RECORD_TEST_LANES * 1024ull)

typedef struct
{
    unsigned long long checks;
    unsigned long long failures;
    ScripturaLine line;
} RecordResults;

// a value read back or worked out: its magnitude's limbs and its sign
typedef struct
{
    unsigned int limb[RECORD_TEST_VALUE_LIMBS];
    int sign;
} RecordValue;

static unsigned long long s_record_state = 0x0E8AC7F00DD0C5EEull;

static unsigned int record_random(void)
{
    s_record_state ^= s_record_state << 13u;
    s_record_state ^= s_record_state >> 7u;
    s_record_state ^= s_record_state << 17u;
    return (unsigned int)(s_record_state >> 16u);
}

static void record_check(RecordResults *results, int passed, const char *what)
{
    results->checks += 1ull;
    if (passed == 0)
    {
        results->failures += 1ull;
        scriptura_text(&results->line, "  FAILED: ");
    }
    else
    {
        scriptura_text(&results->line, "  ok     ");
    }
    scriptura_text(&results->line, what);
    scriptura_character(&results->line, '\n');
}

static void record_small(RecordValue *value, long long small)
{
    memset(value, 0, sizeof(*value));
    const unsigned long long magnitude = (small < 0) ? (0ull - (unsigned long long)small) : (unsigned long long)small;
    value->limb[0] = (unsigned int)magnitude;
    value->limb[1] = (unsigned int)(magnitude >> 32u);
    value->sign = (small > 0) - (small < 0);
}

// value *= factor, a single limb
static void record_times(RecordValue *value, unsigned int factor)
{
    unsigned long long carry = 0ull;
    for (unsigned int at = 0u; at < RECORD_TEST_VALUE_LIMBS; at += 1u)
    {
        carry += (unsigned long long)value->limb[at] * factor;
        value->limb[at] = (unsigned int)carry;
        carry >>= 32u;
    }
    value->sign = (factor == 0u) ? 0 : value->sign;
}

static int record_same(const RecordValue *left, const RecordValue *right)
{
    return (left->sign == right->sign) && (memcmp(left->limb, right->limb, sizeof(left->limb)) == 0);
}

// `bits` of two's complement at bit `offset` of `words`, as a magnitude and a sign
static void record_at(const unsigned int *words, unsigned int offset, unsigned int bits, RecordValue *value)
{
    unsigned int raw[RECORD_TEST_VALUE_LIMBS + 1u];
    memset(raw, 0, sizeof(raw));
    for (unsigned int bit = 0u; (bit < bits) && (bit < 32u * (RECORD_TEST_VALUE_LIMBS + 1u)); bit += 1u)
    {
        const unsigned int from = offset + bit;
        raw[bit / 32u] |= ((words[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    const unsigned int negative = (raw[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u;
    if (negative != 0u)
    {
        if ((bits % 32u) != 0u)
        {
            raw[(bits - 1u) / 32u] |= ~0u << (bits % 32u);
        }
        for (unsigned int at = (bits + 31u) / 32u; at <= RECORD_TEST_VALUE_LIMBS; at += 1u)
        {
            raw[at] = ~0u;
        }
        unsigned long long carry = 1ull;
        for (unsigned int at = 0u; at <= RECORD_TEST_VALUE_LIMBS; at += 1u)
        {
            carry += (unsigned long long)(unsigned int)~raw[at];
            raw[at] = (unsigned int)carry;
            carry >>= 32u;
        }
    }
    memcpy(value->limb, raw, sizeof(value->limb));
    int any = 0;
    for (unsigned int at = 0u; at < RECORD_TEST_VALUE_LIMBS; at += 1u)
    {
        any = any || (value->limb[at] != 0u);
    }
    value->sign = (negative != 0u) ? -1 : (any ? 1 : 0);
}

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} RecordLoaded;

// lays the program out and loads it, its fields read from one-word atoms; 1 where it loaded
static int record_load(ExactRecordProgram *program, const unsigned int *outputs, unsigned int output_count,
                       RecordLoaded *loaded, EngineError *error)
{
    memset(error, 0, sizeof(*error));
    loaded->record = NULL;
    const KeymathRecordRequest encode = {program->steps, program->count, program->field_bits, program->fields, 1u,
                                         outputs, output_count, program->tables, program->table_count, &loaded->key,
                                         error};
    if ((program->failed != 0) || (keymath_record_encode(&encode) == KEYMATH_ERROR))
    {
        return 0;
    }
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 0u, 0u};
    const KeyScheduleRecordRequest layout = {&loaded->key, program->field_offset, program->fields, in_limbs, 1,
                                             &loaded->layout, error};
    if (key_schedule_record_layout(&layout) == KEY_SCHEDULE_ERROR)
    {
        keymath_record_release(&loaded->key);
        return 0;
    }
    if (cycle_record_load(&loaded->layout, &loaded->record, error) == CYCLE_ERROR)
    {
        key_schedule_record_release(&loaded->layout);
        keymath_record_release(&loaded->key);
        return 0;
    }
    return 1;
}

static void record_unload(RecordLoaded *loaded)
{
    cycle_record_release(loaded->record);
    key_schedule_record_release(&loaded->layout);
    keymath_record_release(&loaded->key);
}

// runs the program over the atoms on the device and on the host, and reads every output of every lane from both;
// 1 where both ran and agree word for word
static int record_run(RecordResults *results, ExactRecordProgram *program, const unsigned int *outputs,
                      unsigned int output_count, const unsigned int *atoms, RecordValue *read, const char *name)
{
    RecordLoaded loaded;
    EngineError error;
    if (record_load(program, outputs, output_count, &loaded, &error) == 0)
    {
        scriptura_text(&results->line, "  the program did not load: module ");
        scriptura_decimal(&results->line, (unsigned long long)error.module, 1u);
        scriptura_text(&results->line, ", site ");
        scriptura_decimal(&results->line, (unsigned long long)error.site, 1u);
        scriptura_character(&results->line, '\n');
        record_check(results, 0, name);
        return 0;
    }
    const unsigned int out_limbs = loaded.layout.out_limbs;
    const size_t words = (size_t)RECORD_TEST_LANES * out_limbs;
    unsigned int *const host = (unsigned int *)calloc(words, sizeof(unsigned int));
    unsigned int *const device = (unsigned int *)calloc(words, sizeof(unsigned int));
    unsigned int *device_atoms = NULL;
    unsigned int *device_out = NULL;
    int ok = (host != NULL) && (device != NULL);
    if (ok != 0)
    {
        const CycleRecordHostRequest request = {&loaded.layout, {atoms, NULL, NULL}, {RECORD_TEST_LANES, 0ull, 0ull},
                                                NULL, RECORD_TEST_LANES, host, &error};
        ok = cycle_record_run_host(&request) != CYCLE_ERROR;
    }
    ok = ok && (cudaMalloc((void **)&device_atoms, RECORD_TEST_LANES * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, words * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_atoms, atoms, RECORD_TEST_LANES * sizeof(unsigned int), cudaMemcpyHostToDevice) == cudaSuccess);
    if (ok != 0)
    {
        const CycleRecordRunRequest run = {loaded.record, {device_atoms, NULL, NULL}, {RECORD_TEST_LANES, 0ull, 0ull},
                                           NULL, RECORD_TEST_LANES, device_out, &error};
        ok = (cycle_record_run(&run) != CYCLE_ERROR) &&
             (cudaMemcpy(device, device_out, words * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess);
    }
    const int same = (ok != 0) && (memcmp(host, device, words * sizeof(unsigned int)) == 0);
    for (unsigned int lane = 0u; (same != 0) && (lane < RECORD_TEST_LANES); lane += 1u)
    {
        for (unsigned int out = 0u; out < output_count; out += 1u)
        {
            const DeviceRecordStep *const place = &loaded.layout.step_table[outputs[out]];
            record_at(&device[(size_t)lane * out_limbs], place->out_offset, place->out_bits,
                      &read[((size_t)lane * output_count) + out]);
        }
    }
    cudaFree(device_atoms);
    cudaFree(device_out);
    free(host);
    free(device);
    record_unload(&loaded);
    return same;
}

static RecordValue s_read[RECORD_TEST_LANES * RECORD_TEST_OUTPUTS];
static unsigned int s_atoms[RECORD_TEST_LANES];

// base^e, base and e registers: field 0 the base, 8 bits, field 1 e, 4 bits
static void record_power_case(RecordResults *results)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int base_field = exact_record_field(&program, 8u);
    const unsigned int exponent_field = exact_record_field(&program, 4u);
    const unsigned int base = exact_record_read_unsigned(&program, base_field, 0u);
    const unsigned int exponent = exact_record_read_unsigned(&program, exponent_field, 0u);
    const unsigned int outputs[1] = {exact_record_power(&program, base, exponent, 4u)};
    for (unsigned int lane = 0u; lane < RECORD_TEST_LANES; lane += 1u)
    {
        s_atoms[lane] = record_random() & 0xFFFu;
    }
    const int same = record_run(results, &program, outputs, 1u, s_atoms, s_read, "base^e");
    unsigned int matched = 0u;
    for (unsigned int lane = 0u; (same != 0) && (lane < RECORD_TEST_LANES); lane += 1u)
    {
        RecordValue want;
        record_small(&want, 1);
        for (unsigned int times = 0u; times < (s_atoms[lane] >> 8u); times += 1u)
        {
            record_times(&want, s_atoms[lane] & 0xFFu);
        }
        matched += (unsigned int)record_same(&want, &s_read[lane]);
    }
    record_check(results, (same != 0) && (matched == RECORD_TEST_LANES),
                 "base^e by square and multiply, base below 2^8 and e below 16, device = host = the limbs' product");
    exact_record_close(&program);
}

// base^e for a constant base by the nibble table, e a register of `bits` bits
static void record_power_of_case(RecordResults *results, unsigned int base, unsigned int bits, const char *what)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int exponent_field = exact_record_field(&program, bits);
    const unsigned int exponent = exact_record_read_unsigned(&program, exponent_field, 0u);
    const unsigned int outputs[1] = {exact_record_power_of(&program, base, exponent, bits)};
    for (unsigned int lane = 0u; lane < RECORD_TEST_LANES; lane += 1u)
    {
        // every exponent in turn, then random ones
        s_atoms[lane] = (lane < (1u << bits)) ? lane : (record_random() & ((1u << bits) - 1u));
    }
    const int same = record_run(results, &program, outputs, 1u, s_atoms, s_read, what);
    unsigned int matched = 0u;
    for (unsigned int lane = 0u; (same != 0) && (lane < RECORD_TEST_LANES); lane += 1u)
    {
        RecordValue want;
        record_small(&want, 1);
        for (unsigned int times = 0u; times < s_atoms[lane]; times += 1u)
        {
            record_times(&want, base);
        }
        matched += (unsigned int)record_same(&want, &s_read[lane]);
    }
    record_check(results, (same != 0) && (matched == RECORD_TEST_LANES), what);
    exact_record_close(&program);
}

// l / r less and more 1: field 0 l, signed 16 bits, field 1 r, signed 8 bits and never 0
static void record_bracket_case(RecordResults *results)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int left_field = exact_record_field(&program, 16u);
    const unsigned int right_field = exact_record_field(&program, 8u);
    const unsigned int left = exact_record_read(&program, left_field, 0u);
    const unsigned int right = exact_record_read(&program, right_field, 0u);
    const unsigned int outputs[2] = {exact_record_down(&program, left, right), exact_record_up(&program, left, right)};
    for (unsigned int lane = 0u; lane < RECORD_TEST_LANES; lane += 1u)
    {
        unsigned int r = record_random() & 0xFFu;
        r = (r == 0u) ? 1u : r;
        s_atoms[lane] = (record_random() & 0xFFFFu) | (r << 16u);
    }
    const int same = record_run(results, &program, outputs, 2u, s_atoms, s_read, "the outward pair");
    unsigned int matched = 0u;
    for (unsigned int lane = 0u; (same != 0) && (lane < RECORD_TEST_LANES); lane += 1u)
    {
        const long long l = (long long)(short)(s_atoms[lane] & 0xFFFFu);
        const long long r = (long long)(signed char)((s_atoms[lane] >> 16u) & 0xFFu);
        RecordValue down;
        RecordValue up;
        record_small(&down, (l / r) - 1);
        record_small(&up, (l / r) + 1);
        matched += (unsigned int)(record_same(&down, &s_read[2u * lane]) && record_same(&up, &s_read[(2u * lane) + 1u]));
    }
    record_check(results, (same != 0) && (matched == RECORD_TEST_LANES),
                 "down and up: l / r toward zero less and more 1, signed l and r, device = host = C's division");
    exact_record_close(&program);
}

// two's complement and comparison: field 0 v and field 1 w, signed 16 bits each
static void record_compare_case(RecordResults *results)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int v_field = exact_record_field(&program, 16u);
    const unsigned int w_field = exact_record_field(&program, 16u);
    const unsigned int v = exact_record_read(&program, v_field, 0u);
    const unsigned int w = exact_record_read(&program, w_field, 0u);
    const unsigned int above = exact_record_above(&program, v, w);
    unsigned int outputs[7];
    outputs[0] = exact_record_wrap(&program, exact_record_product(&program, v, exact_record_constant(&program, 3ull)), 19u);
    outputs[1] = exact_record_narrow(&program, exact_record_sum(&program, v, exact_record_constant(&program, 32768ull)), 16u);
    outputs[2] = exact_record_bit(&program, v, 5u);
    outputs[3] = above;
    outputs[4] = exact_record_equal(&program, v, w);
    outputs[5] = exact_record_equal(&program, v, v);
    outputs[6] = exact_record_select(&program, above, v, w);
    for (unsigned int lane = 0u; lane < RECORD_TEST_LANES; lane += 1u)
    {
        const unsigned int v_bits = record_random() & 0xFFFFu;
        // w equal to v on one lane in eight
        const unsigned int w_bits = ((lane % 8u) == 0u) ? v_bits : (record_random() & 0xFFFFu);
        s_atoms[lane] = v_bits | (w_bits << 16u);
    }
    const int same = record_run(results, &program, outputs, 7u, s_atoms, s_read, "two's complement and comparison");
    unsigned int matched = 0u;
    for (unsigned int lane = 0u; (same != 0) && (lane < RECORD_TEST_LANES); lane += 1u)
    {
        const long long vv = (long long)(short)(s_atoms[lane] & 0xFFFFu);
        const long long ww = (long long)(short)(s_atoms[lane] >> 16u);
        const long long want[7] = {3ll * vv,
                                   vv + 32768ll,
                                   (long long)((((unsigned long long)vv) >> 5u) & 1ull),
                                   (vv > ww) ? 1ll : 0ll,
                                   (vv == ww) ? 1ll : 0ll,
                                   1ll,
                                   (vv > ww) ? vv : ww};
        unsigned int all = 1u;
        for (unsigned int out = 0u; out < 7u; out += 1u)
        {
            RecordValue value;
            record_small(&value, want[out]);
            all &= (unsigned int)record_same(&value, &s_read[(7u * lane) + out]);
        }
        matched += all;
    }
    record_check(results, (same != 0) && (matched == RECORD_TEST_LANES),
                 "wrap, narrow, bit, above, equal and select, signed v and w, device = host = C's integers");
    exact_record_close(&program);
}

int main(int count, char **arguments)
{
    RecordResults results;
    results.checks = 0ull;
    results.failures = 0ull;
    results.line.capacity = RECORD_TEST_LINE;
    results.line.out = (char *)malloc((size_t)RECORD_TEST_LINE);
    results.line.at = 0ull;
    if (results.line.out == NULL)
    {
        return 2;
    }
    char job_capacity[SIM_LINE_CAPACITY];
    SimResults job;
    sim_open(&job, job_capacity);
    const int admitted = sim_job_submit(&job, "exact_record_test", count, arguments, RECORD_TEST_DECLARED);
    if (admitted != 0)
    {
        record_power_case(&results);
        record_power_of_case(&results, 2u, 8u, "2^e by the nibble table, e below 2^8, device = host = the limbs' power");
        record_power_of_case(&results, 3u, 6u, "3^e by the nibble table, e below 2^6, device = host = the limbs' power");
        record_bracket_case(&results);
        record_compare_case(&results);
    }
    sim_job_release(&job);
    sim_flush(&job);
    record_check(&results, (admitted != 0) && (job.failures == 0ull),
                 "tessera: the device's daemon admits the test's job and it releases");

    scriptura_text(&results.line, "  exact record test: ");
    scriptura_decimal(&results.line, results.checks, 1u);
    scriptura_text(&results.line, " checks, ");
    scriptura_decimal(&results.line, results.failures, 1u);
    scriptura_text(&results.line, " failed\n");
    scriptura_write(&results.line, stdout);
    free(results.line.out);
    return (results.failures == 0ull) ? 0 : 1;
}
