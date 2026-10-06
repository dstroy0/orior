// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Proves edouble_record, the exact double on the device: one program over random lanes, a = m_a 2^e_a and
// b = m_b 2^e_b with signed 12-bit mantissas and signed 4-bit exponents, run on the device and on the host, the two
// agreeing word for word, and every output held against the exact integers of value 2^64, read on the host:
// - a b and a + b equal to the exact product and sum;
// - [a > b] equal to the exact order;
// - the cut of a b to 8 bits: its ends below and above a b, one unit apart or equal where a b is whole there, each at
//   most 2^8;
// - a / b to 10 bits past b: its ends below and above a / b, one unit apart or equal where the quotient is whole.
// The test is one job on the device's tessera daemon.
#include "../../../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../../../../../../../src/cu/types/integerfloats/edouble/edouble_record.h"
#include "../../../../../../../src/cu/types/integers/exact_integer.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

#define EDOUBLE_TEST_LINE 8192ull

#define EDOUBLE_TEST_LANES 4096u

#define EDOUBLE_TEST_OUTPUTS 11u

// every exponent here, the quotient's included, lies above -64: a value times 2^64 is an integer
#define EDOUBLE_TEST_LIFT 64u

#define EDOUBLE_TEST_CUT 8u
#define EDOUBLE_TEST_CUT_RANGE 5u
#define EDOUBLE_TEST_QUOTIENT 10u
#define EDOUBLE_TEST_DIVISOR 12u
#define EDOUBLE_TEST_SPREAD 4u

#define EDOUBLE_TEST_DECLARED ((unsigned long long)EDOUBLE_TEST_LANES * 1024ull)

typedef struct
{
    unsigned long long checks;
    unsigned long long failures;
    ScripturaLine line;
} EdoubleResults;

static unsigned long long s_edouble_state = 0xED0B1E5EEDC0FFEEull;

static unsigned int edouble_random(void)
{
    s_edouble_state ^= s_edouble_state << 13u;
    s_edouble_state ^= s_edouble_state >> 7u;
    s_edouble_state ^= s_edouble_state << 17u;
    return (unsigned int)(s_edouble_state >> 16u);
}

static void edouble_check(EdoubleResults *results, int passed, const char *what)
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

// `bits` of two's complement at bit `offset` of `words`, as an exact integer; an output wider than the integer holds
// reads as sign 2, which equals no worked-out value
static void edouble_at(const unsigned int *words, unsigned int offset, unsigned int bits, AnchorExactInteger *value)
{
    anchor_exact_zero(value);
    if (bits > ANCHOR_EXACT_BITS)
    {
        value->sign = 2;
        return;
    }
    const unsigned int limbs = (bits + 31u) / 32u;
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int from = offset + bit;
        value->limb[bit / 32u] |= ((words[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    const unsigned int negative = (value->limb[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u;
    if (negative != 0u)
    {
        if ((bits % 32u) != 0u)
        {
            value->limb[limbs - 1u] |= ~0u << (bits % 32u);
        }
        unsigned long long carry = 1ull;
        for (unsigned int at = 0u; at < limbs; at += 1u)
        {
            carry += (unsigned long long)(unsigned int)~value->limb[at];
            value->limb[at] = (unsigned int)carry;
            carry >>= 32u;
        }
    }
    int any = 0;
    for (unsigned int at = 0u; at < limbs; at += 1u)
    {
        any = any || (value->limb[at] != 0u);
    }
    value->sign = (negative != 0u) ? -1 : (any ? 1 : 0);
}

static void edouble_small(AnchorExactInteger *value, long long small)
{
    anchor_exact_zero(value);
    const unsigned long long magnitude = (small < 0) ? (0ull - (unsigned long long)small) : (unsigned long long)small;
    value->limb[0] = (unsigned int)magnitude;
    value->limb[1] = (unsigned int)(magnitude >> 32u);
    value->sign = (small > 0) - (small < 0);
}

static long long edouble_word(const AnchorExactInteger *value)
{
    const long long magnitude = (long long)(((unsigned long long)value->limb[1] << 32u) | value->limb[0]);
    return (value->sign < 0) ? -magnitude : magnitude;
}

// m 2^(e + 64), e above -64; 1 where it fits
static int edouble_lifted(const AnchorExactInteger *mantissa, long long exponent, AnchorExactInteger *out)
{
    AnchorExactInteger power;
    anchor_exact_zero(&power);
    const unsigned long long place = (unsigned long long)(exponent + (long long)EDOUBLE_TEST_LIFT);
    power.limb[place / 32u] = 1u << (place % 32u);
    power.sign = 1;
    return anchor_exact_multiply(mantissa, &power, out) == ANCHOR_EXACT_OK;
}

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} EdoubleLoaded;

static int edouble_load(ExactRecordProgram *program, const unsigned int *outputs, unsigned int output_count,
                        EdoubleLoaded *loaded, EngineError *error)
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

static AnchorExactInteger s_read[EDOUBLE_TEST_LANES * EDOUBLE_TEST_OUTPUTS];
static unsigned int s_atoms[EDOUBLE_TEST_LANES];

// runs the program on the device and the host and reads every output of every lane; 1 where both ran and agree
static int edouble_run(EdoubleResults *results, ExactRecordProgram *program, const unsigned int *outputs,
                       unsigned int output_count)
{
    EdoubleLoaded loaded;
    EngineError error;
    if (edouble_load(program, outputs, output_count, &loaded, &error) == 0)
    {
        scriptura_text(&results->line, "  the program did not load: module ");
        scriptura_decimal(&results->line, (unsigned long long)error.module, 1u);
        scriptura_text(&results->line, ", site ");
        scriptura_decimal(&results->line, (unsigned long long)error.site, 1u);
        scriptura_character(&results->line, '\n');
        return 0;
    }
    const unsigned int out_limbs = loaded.layout.out_limbs;
    const size_t words = (size_t)EDOUBLE_TEST_LANES * out_limbs;
    unsigned int *const host = (unsigned int *)calloc(words, sizeof(unsigned int));
    unsigned int *const device = (unsigned int *)calloc(words, sizeof(unsigned int));
    unsigned int *device_atoms = NULL;
    unsigned int *device_out = NULL;
    int ok = (host != NULL) && (device != NULL);
    if (ok != 0)
    {
        const CycleRecordHostRequest request = {&loaded.layout, {s_atoms, NULL, NULL}, {EDOUBLE_TEST_LANES, 0ull, 0ull},
                                                NULL, EDOUBLE_TEST_LANES, host, &error};
        ok = cycle_record_run_host(&request) != CYCLE_ERROR;
    }
    ok = ok && (cudaMalloc((void **)&device_atoms, EDOUBLE_TEST_LANES * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, words * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_atoms, s_atoms, EDOUBLE_TEST_LANES * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    if (ok != 0)
    {
        const CycleRecordRunRequest run = {loaded.record, {device_atoms, NULL, NULL}, {EDOUBLE_TEST_LANES, 0ull, 0ull},
                                           NULL, EDOUBLE_TEST_LANES, device_out, &error};
        ok = (cycle_record_run(&run) != CYCLE_ERROR) &&
             (cudaMemcpy(device, device_out, words * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess);
    }
    const int same = (ok != 0) && (memcmp(host, device, words * sizeof(unsigned int)) == 0);
    for (unsigned int lane = 0u; (same != 0) && (lane < EDOUBLE_TEST_LANES); lane += 1u)
    {
        for (unsigned int out = 0u; out < output_count; out += 1u)
        {
            const DeviceRecordStep *const place = &loaded.layout.step_table[outputs[out]];
            edouble_at(&device[(size_t)lane * out_limbs], place->out_offset, place->out_bits,
                       &s_read[((size_t)lane * output_count) + out]);
        }
    }
    cudaFree(device_atoms);
    cudaFree(device_out);
    free(host);
    free(device);
    cycle_record_release(loaded.record);
    key_schedule_record_release(&loaded.layout);
    keymath_record_release(&loaded.key);
    return same;
}

// the signed field of `bits` at `offset` of an atom
static long long edouble_field(unsigned int atom, unsigned int offset, unsigned int bits)
{
    const long long raw = (long long)((atom >> offset) & ((1u << bits) - 1u));
    return (raw >= (1ll << (bits - 1u))) ? (raw - (1ll << bits)) : raw;
}

static int edouble_order(const AnchorExactInteger *left, const AnchorExactInteger *right)
{
    return anchor_exact_compare(left, right);
}

int main(int count, char **arguments)
{
    EdoubleResults results;
    results.checks = 0ull;
    results.failures = 0ull;
    results.line.capacity = EDOUBLE_TEST_LINE;
    results.line.out = (char *)malloc((size_t)EDOUBLE_TEST_LINE);
    results.line.at = 0ull;
    if (results.line.out == NULL)
    {
        return 2;
    }
    char job_capacity[SIM_LINE_CAPACITY];
    SimResults job;
    sim_open(&job, job_capacity);
    const int admitted = sim_job_submit(&job, "edouble_record_test", count, arguments, EDOUBLE_TEST_DECLARED);
    if (admitted != 0)
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int fields[4] = {exact_record_field(&program, 12u), exact_record_field(&program, 4u),
                                        exact_record_field(&program, 12u), exact_record_field(&program, 4u)};
        const EdoubleRecord a = edouble_record_of(exact_record_read(&program, fields[0], 0u),
                                                  exact_record_read(&program, fields[1], 0u));
        const EdoubleRecord b = edouble_record_of(exact_record_read(&program, fields[2], 0u),
                                                  exact_record_read(&program, fields[3], 0u));
        const EdoubleRecord product = edouble_record_product(&program, a, b);
        const EdoubleRecord sum = edouble_record_sum(&program, a, b, EDOUBLE_TEST_SPREAD);
        const EdoubleRecordHeld cut = edouble_record_cut(&program, product, EDOUBLE_TEST_CUT, EDOUBLE_TEST_CUT_RANGE);
        const EdoubleRecordHeld quotient =
            edouble_record_quotient(&program, a, b, EDOUBLE_TEST_QUOTIENT, EDOUBLE_TEST_DIVISOR);
        const unsigned int outputs[EDOUBLE_TEST_OUTPUTS] = {product.mantissa,
                                                            product.exponent,
                                                            sum.mantissa,
                                                            sum.exponent,
                                                            edouble_record_above(&program, a, b, EDOUBLE_TEST_SPREAD),
                                                            cut.down,
                                                            cut.up,
                                                            cut.exponent,
                                                            quotient.down,
                                                            quotient.up,
                                                            quotient.exponent};
        for (unsigned int lane = 0u; lane < EDOUBLE_TEST_LANES; lane += 1u)
        {
            unsigned int atom = edouble_random();
            // b's mantissa never 0, a equal to b on one lane in sixteen
            if (((atom >> 16u) & 0xFFFu) == 0u)
            {
                atom |= 1u << 16u;
            }
            s_atoms[lane] = ((lane % 16u) == 0u) ? ((atom & 0xFFFF0000u) | (atom >> 16u)) : atom;
        }
        const int same = edouble_run(&results, &program, outputs, EDOUBLE_TEST_OUTPUTS);
        edouble_check(&results, same, "one program, device = host word for word over 4096 lanes");
        unsigned int exact_product = 0u;
        unsigned int exact_sum = 0u;
        unsigned int ordered = 0u;
        unsigned int cut_held = 0u;
        unsigned int quotient_held = 0u;
        for (unsigned int lane = 0u; (same != 0) && (lane < EDOUBLE_TEST_LANES); lane += 1u)
        {
            const AnchorExactInteger *const read = &s_read[(size_t)lane * EDOUBLE_TEST_OUTPUTS];
            const long long ma = edouble_field(s_atoms[lane], 0u, 12u);
            const long long ea = edouble_field(s_atoms[lane], 12u, 4u);
            const long long mb = edouble_field(s_atoms[lane], 16u, 12u);
            const long long eb = edouble_field(s_atoms[lane], 28u, 4u);
            AnchorExactInteger value_a;
            AnchorExactInteger value_b;
            AnchorExactInteger small;
            AnchorExactInteger want;
            AnchorExactInteger got;
            AnchorExactInteger other;
            edouble_small(&small, ma);
            int ok = edouble_lifted(&small, ea, &value_a);
            edouble_small(&small, mb);
            ok = ok && edouble_lifted(&small, eb, &value_b);
            // the product: (m_a m_b) 2^(e_a + e_b + 64)
            edouble_small(&small, ma * mb);
            ok = ok && edouble_lifted(&small, ea + eb, &want) && edouble_lifted(&read[0], edouble_word(&read[1]), &got);
            exact_product += (unsigned int)(ok && anchor_exact_equal(&want, &got));
            ok = ok && (anchor_exact_add(&value_a, &value_b, &want) == ANCHOR_EXACT_OK) &&
                 edouble_lifted(&read[2], edouble_word(&read[3]), &got);
            exact_sum += (unsigned int)(ok && anchor_exact_equal(&want, &got));
            ordered += (unsigned int)(ok && ((edouble_order(&value_a, &value_b) > 0) == (edouble_word(&read[4]) == 1ll)) &&
                                      ((edouble_word(&read[4]) == 0ll) || (edouble_word(&read[4]) == 1ll)));
            // the cut: down <= a b <= up at its exponent, up - down 0 or 1 and 0 only where a b is whole there
            {
                AnchorExactInteger exact;
                AnchorExactInteger low;
                AnchorExactInteger high;
                edouble_small(&small, ma * mb);
                const long long down = edouble_word(&read[5]);
                const long long up = edouble_word(&read[6]);
                const long long at = edouble_word(&read[7]);
                int held = ok && edouble_lifted(&small, ea + eb, &exact) && edouble_lifted(&read[5], at, &low) &&
                           edouble_lifted(&read[6], at, &high);
                held = held && (anchor_exact_compare(&low, &exact) <= 0) && (anchor_exact_compare(&exact, &high) <= 0);
                held = held && ((up - down) == ((anchor_exact_equal(&low, &exact) != 0) ? 0ll : 1ll));
                held = held && (down >= -(1ll << EDOUBLE_TEST_CUT)) && (up <= (1ll << EDOUBLE_TEST_CUT));
                cut_held += (unsigned int)held;
            }
            // the quotient: down b <= a <= up b where b > 0, turned where b < 0, at q's exponent; each side times 2^64
            {
                AnchorExactInteger low;
                AnchorExactInteger high;
                AnchorExactInteger lifted_a;
                AnchorExactInteger unit;
                const long long down = edouble_word(&read[8]);
                const long long up = edouble_word(&read[9]);
                const long long at = edouble_word(&read[10]);
                edouble_small(&unit, 1);
                int held = ok && edouble_lifted(&read[8], at, &got) &&
                           (anchor_exact_multiply(&got, &value_b, &low) == ANCHOR_EXACT_OK) &&
                           edouble_lifted(&read[9], at, &other) &&
                           (anchor_exact_multiply(&other, &value_b, &high) == ANCHOR_EXACT_OK) &&
                           edouble_lifted(&value_a, 0ll, &lifted_a);
                const int turned = (mb < 0);
                const AnchorExactInteger *const below = turned ? &high : &low;
                const AnchorExactInteger *const above = turned ? &low : &high;
                held = held && (anchor_exact_compare(below, &lifted_a) <= 0) && (anchor_exact_compare(&lifted_a, above) <= 0);
                held = held && ((up - down) == ((anchor_exact_equal(&low, &lifted_a) != 0) ? 0ll : 1ll));
                quotient_held += (unsigned int)held;
            }
        }
        edouble_check(&results, (same != 0) && (exact_product == EDOUBLE_TEST_LANES), "a b is the exact product");
        edouble_check(&results, (same != 0) && (exact_sum == EDOUBLE_TEST_LANES),
                      "a + b is the exact sum, the mantissas lined up by each lane's own power of two");
        edouble_check(&results, (same != 0) && (ordered == EDOUBLE_TEST_LANES), "[a > b] is the exact order");
        edouble_check(&results, (same != 0) && (cut_held == EDOUBLE_TEST_LANES),
                      "the cut of a b to 8 bits lies below and above it, one unit apart or equal where whole");
        edouble_check(&results, (same != 0) && (quotient_held == EDOUBLE_TEST_LANES),
                      "a / b to 10 bits past b lies below and above it, one unit apart or equal where whole");
        exact_record_close(&program);
    }
    sim_job_release(&job);
    sim_flush(&job);
    edouble_check(&results, (admitted != 0) && (job.failures == 0ull),
                  "tessera: the device's daemon admits the test's job and it releases");

    scriptura_text(&results.line, "  edouble record test: ");
    scriptura_decimal(&results.line, results.checks, 1u);
    scriptura_text(&results.line, " checks, ");
    scriptura_decimal(&results.line, results.failures, 1u);
    scriptura_text(&results.line, " failed\n");
    scriptura_write(&results.line, stdout);
    free(results.line.out);
    return (results.failures == 0ull) ? 0 : 1;
}
