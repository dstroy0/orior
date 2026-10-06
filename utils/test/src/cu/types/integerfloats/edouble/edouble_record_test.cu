// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Proves edouble_record, the exact double on the device: one program over random lanes, a = m_a 2^e_a and
// b = m_b 2^e_b with signed 12-bit mantissas and signed 4-bit exponents, run on the device and on the host, the two
// agreeing word for word, and every output held against the exact integers of value 2^64, read on the host:
// - a b and a + b equal to the exact product and sum;
// - [a > b] equal to the exact order;
// - the cut of a b to 8 bits: its ends below and above a b, one unit apart or equal where a b is whole there, each at
//   most 2^8;
// - a / b to 10 bits past b: its ends below and above a / b, one unit apart or equal where the quotient is whole.
// A second program holds a = [m_a, m_a + d_a] 2^e_a and b likewise, b holding no 0:
// - the held sum's ends the exact sums, and the held product's the least and the most corner;
// - the held cut the lower end's floor and the upper end's ceiling, each at most 2^8;
// - every corner quotient inside the held quotient, each end within one unit of one;
// - held above, a's lower end over b's upper.
// A third program holds its exponents as constants the program knows: the held sum in six steps, and reads at a known
// exponent, lifted exact or the floor and ceiling below it, and at a lane's own exponent. A fourth holds balls: the sum
// and product exact, the read holding the product's ball within a unit of each exponent, every corner quotient inside
// the quotient's ball and the flag 0 where the divisor's ball holds 0, and the order.
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

// the most outputs a program here gives
#define EDOUBLE_TEST_OUTPUTS 15u

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
        const char *const at = (const char *)error.evacaddr;
        const char *const first_step = (const char *)program->steps;
        const char *const first_output = (const char *)outputs;
        if ((at >= first_step) && (at < first_step + ((size_t)program->count * sizeof(EngineRecordStep))))
        {
            const unsigned int step = (unsigned int)((size_t)(at - first_step) / sizeof(EngineRecordStep));
            scriptura_text(&results->line, ", step ");
            scriptura_decimal(&results->line, (unsigned long long)step, 1u);
            scriptura_text(&results->line, " operation ");
            scriptura_decimal(&results->line, (unsigned long long)program->steps[step].operation, 1u);
            scriptura_text(&results->line, " left ");
            scriptura_decimal(&results->line, (unsigned long long)program->steps[step].left, 1u);
            scriptura_text(&results->line, " right ");
            scriptura_decimal(&results->line, (unsigned long long)program->steps[step].right, 1u);
        }
        else if ((at >= first_output) && (at < first_output + ((size_t)output_count * sizeof(unsigned int))))
        {
            scriptura_text(&results->line, ", output ");
            scriptura_decimal(&results->line, (unsigned long long)((size_t)(at - first_output) / sizeof(unsigned int)), 1u);
        }
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

// S(m, e) S(n, f) against S(o, g) 2^64: the order of (m 2^e)(n 2^f) and o 2^g
static int edouble_order_product(const AnchorExactInteger *m, long long e, const AnchorExactInteger *n, long long f,
                                 const AnchorExactInteger *o, long long g, int *order)
{
    AnchorExactInteger left_one;
    AnchorExactInteger left_two;
    AnchorExactInteger left;
    AnchorExactInteger right_one;
    AnchorExactInteger right;
    const int ok = edouble_lifted(m, e, &left_one) && edouble_lifted(n, f, &left_two) &&
                   (anchor_exact_multiply(&left_one, &left_two, &left) == ANCHOR_EXACT_OK) &&
                   edouble_lifted(o, g, &right_one) && edouble_lifted(&right_one, 0ll, &right);
    *order = ok ? anchor_exact_compare(&left, &right) : 0;
    return ok;
}

// held values: a = [m_a, m_a + d_a] 2^e_a and b likewise, m signed 10 bits, d 3 bits and e signed 3 bits, b holding
// no 0; their held sum, product, cut, quotient and order
static void edouble_held_case(EdoubleResults *results)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    EdoubleRecordHeld held[2];
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        const unsigned int m = exact_record_read(&program, exact_record_field(&program, 10u), 0u);
        const unsigned int d = exact_record_read_unsigned(&program, exact_record_field(&program, 3u), 0u);
        const unsigned int e = exact_record_read(&program, exact_record_field(&program, 3u), 0u);
        held[side].down = m;
        held[side].up = exact_record_sum(&program, m, d);
        held[side].exponent = e;
    }
    const EdoubleRecordHeld sum = edouble_record_held_sum(&program, held[0], held[1], 3u);
    const EdoubleRecordHeld product = edouble_record_held_product(&program, held[0], held[1]);
    const EdoubleRecordHeld cut = edouble_record_held_cut(&program, product, 8u, 5u);
    const EdoubleRecordHeld quotient = edouble_record_held_quotient(&program, held[0], held[1], 10u, 11u);
    const unsigned int outputs[13] = {sum.down,      sum.up,        sum.exponent, product.down,  product.up,
                                      product.exponent, cut.down,   cut.up,       cut.exponent,  quotient.down,
                                      quotient.up,   quotient.exponent, edouble_record_held_above(&program, held[0], held[1], 3u)};
    for (unsigned int lane = 0u; lane < EDOUBLE_TEST_LANES; lane += 1u)
    {
        unsigned int atom = edouble_random();
        const long long mb = edouble_field(atom, 16u, 10u);
        const long long db = (long long)((atom >> 26u) & 7u);
        // b holding 0 is moved to [1, 1 + d]
        if ((mb <= 0) && ((mb + db) >= 0))
        {
            atom = (atom & ~(0x3FFu << 16u)) | (1u << 16u);
        }
        s_atoms[lane] = atom;
    }
    const int same = edouble_run(results, &program, outputs, 13u);
    edouble_check(results, same, "held: one program, device = host word for word over 4096 lanes");
    unsigned int sums = 0u;
    unsigned int products = 0u;
    unsigned int cuts = 0u;
    unsigned int quotients = 0u;
    unsigned int ordered = 0u;
    for (unsigned int lane = 0u; (same != 0) && (lane < EDOUBLE_TEST_LANES); lane += 1u)
    {
        const AnchorExactInteger *const read = &s_read[(size_t)lane * 13u];
        long long m[2];
        long long top[2];
        long long e[2];
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            m[side] = edouble_field(s_atoms[lane], 16u * side, 10u);
            top[side] = m[side] + (long long)((s_atoms[lane] >> ((16u * side) + 10u)) & 7u);
            e[side] = edouble_field(s_atoms[lane], (16u * side) + 13u, 3u);
        }
        AnchorExactInteger small;
        AnchorExactInteger lifted[2][2];
        int ok = 1;
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            edouble_small(&small, m[side]);
            ok = ok && edouble_lifted(&small, e[side], &lifted[side][0]);
            edouble_small(&small, top[side]);
            ok = ok && edouble_lifted(&small, e[side], &lifted[side][1]);
        }
        // the sum's ends, exact
        {
            AnchorExactInteger want;
            AnchorExactInteger got;
            int held_sum = ok;
            for (unsigned int end = 0u; end < 2u; end += 1u)
            {
                held_sum = held_sum && (anchor_exact_add(&lifted[0][end], &lifted[1][end], &want) == ANCHOR_EXACT_OK) &&
                           edouble_lifted(&read[end], edouble_word(&read[2]), &got) && anchor_exact_equal(&want, &got);
            }
            sums += (unsigned int)held_sum;
        }
        // the product's ends: the least and the most corner, exact
        long long corner_least = 0;
        long long corner_most = 0;
        for (unsigned int corner = 0u; corner < 4u; corner += 1u)
        {
            const long long value = ((corner / 2u) ? top[0] : m[0]) * ((corner % 2u) ? top[1] : m[1]);
            corner_least = (corner == 0u) ? value : ((value < corner_least) ? value : corner_least);
            corner_most = (corner == 0u) ? value : ((value > corner_most) ? value : corner_most);
        }
        const long long product_at = edouble_word(&read[5]);
        products += (unsigned int)(ok && (product_at == (e[0] + e[1])) && (edouble_word(&read[3]) == corner_least) &&
                                   (edouble_word(&read[4]) == corner_most));
        // the cut: the floor of the product's lower end and the ceiling of its upper at 2^k, each at most 2^8
        {
            const long long k = edouble_word(&read[8]) - product_at;
            const long long down = edouble_word(&read[6]);
            const long long up = edouble_word(&read[7]);
            const long long unit = (k >= 0 && k < 62) ? (1ll << k) : 0;
            int held_cut = ok && (unit != 0);
            held_cut = held_cut && (down * unit <= corner_least) && (corner_least < (down + 1) * unit);
            held_cut = held_cut && ((up - 1) * unit < corner_most) && (corner_most <= up * unit);
            held_cut = held_cut && (down >= -256) && (up <= 256);
            cuts += (unsigned int)held_cut;
        }
        // the quotient: every corner a_i / b_j inside it, and its ends one unit in from them fall outside one corner
        {
            const long long at = edouble_word(&read[11]);
            AnchorExactInteger ends[2];
            AnchorExactInteger moved[2];
            AnchorExactInteger one;
            edouble_small(&one, 1);
            int held_quotient = ok && (anchor_exact_add(&read[9], &one, &moved[0]) == ANCHOR_EXACT_OK) &&
                                (anchor_exact_subtract(&read[10], &one, &moved[1]) == ANCHOR_EXACT_OK);
            ends[0] = read[9];
            ends[1] = read[10];
            int down_tight = 0;
            int up_tight = 0;
            for (unsigned int corner = 0u; held_quotient && (corner < 4u); corner += 1u)
            {
                AnchorExactInteger numerator;
                AnchorExactInteger divisor;
                edouble_small(&numerator, (corner / 2u) ? top[0] : m[0]);
                edouble_small(&divisor, (corner % 2u) ? top[1] : m[1]);
                const int positive = (divisor.sign > 0);
                int low = 0;
                int high = 0;
                int low_moved = 0;
                int high_moved = 0;
                held_quotient = held_quotient &&
                                edouble_order_product(&ends[0], at, &divisor, e[1], &numerator, e[0], &low) &&
                                edouble_order_product(&ends[1], at, &divisor, e[1], &numerator, e[0], &high) &&
                                edouble_order_product(&moved[0], at, &divisor, e[1], &numerator, e[0], &low_moved) &&
                                edouble_order_product(&moved[1], at, &divisor, e[1], &numerator, e[0], &high_moved);
                // with b's corner above 0, down b <= a <= up b; below 0 the order turns
                held_quotient = held_quotient && (positive ? ((low <= 0) && (high >= 0)) : ((low >= 0) && (high <= 0)));
                down_tight = down_tight || (positive ? (low_moved > 0) : (low_moved < 0));
                up_tight = up_tight || (positive ? (high_moved < 0) : (high_moved > 0));
            }
            quotients += (unsigned int)(held_quotient && down_tight && up_tight);
        }
        // a above b: a's lower end above b's upper
        {
            const int above = anchor_exact_compare(&lifted[0][0], &lifted[1][1]) > 0;
            ordered += (unsigned int)(ok && (edouble_word(&read[12]) == (above ? 1ll : 0ll)));
        }
    }
    edouble_check(results, (same != 0) && (sums == EDOUBLE_TEST_LANES), "held: the sum's ends are the exact sums");
    edouble_check(results, (same != 0) && (products == EDOUBLE_TEST_LANES),
                  "held: the product's ends are the least and the most corner, exact");
    edouble_check(results, (same != 0) && (cuts == EDOUBLE_TEST_LANES),
                  "held: the cut gives the lower end's floor and the upper end's ceiling, each at most 2^8");
    edouble_check(results, (same != 0) && (quotients == EDOUBLE_TEST_LANES),
                  "held: every corner quotient lies inside, and each end is within one unit of one");
    edouble_check(results, (same != 0) && (ordered == EDOUBLE_TEST_LANES), "held: above is a's lower end over b's upper");
    exact_record_close(&program);
}

// the floor and ceiling of n / 2^k, k from 0 to 62
static long long edouble_floor_by(long long n, unsigned int k)
{
    const long long unit = 1ll << k;
    return (n >= 0) ? (n / unit) : -((-n + unit - 1) / unit);
}

static long long edouble_ceiling_by(long long n, unsigned int k)
{
    return -edouble_floor_by(-n, k);
}

// exponents the program knows: a = [m_a, m_a + d_a] 2^3 and b = [m_b, m_b + d_b] 2^-5, m signed 10 bits and d 3 bits;
// the held sum lifts a's side by the known 2^8 in six steps, and the held product's and the sum's reads at the known
// exponents 2 and -7; and the read at -2 of [m_a, m_a + d_a] 2^e with the lane's own e, signed 3 bits
static void edouble_known_case(EdoubleResults *results)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const long long exponents[2] = {3ll, -5ll};
    EdoubleRecordHeld held[2];
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        const unsigned int m = exact_record_read(&program, exact_record_field(&program, 10u), 0u);
        const unsigned int d = exact_record_read_unsigned(&program, exact_record_field(&program, 3u), 0u);
        const unsigned int e = exact_record_read(&program, exact_record_field(&program, 3u), 0u);
        held[side].down = m;
        held[side].up = exact_record_sum(&program, m, d);
        held[side].exponent = (side == 0u) ? e : 0u;
    }
    const EdoubleRecordHeld lane_held = held[0];
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        held[side].exponent = exact_record_signed(&program, exponents[side]);
    }
    const unsigned int before = program.count;
    const EdoubleRecordHeld sum = edouble_record_held_sum(&program, held[0], held[1], 4u);
    const unsigned int sum_steps = program.count - before;
    const EdoubleRecordHeld product = edouble_record_held_product(&program, held[0], held[1]);
    const EdoubleRecordHeld lifted = edouble_record_held_at(&program, sum, -7ll, 24u, 0u);
    const EdoubleRecordHeld cut = edouble_record_held_at(&program, product, 2ll, 20u, 0u);
    const EdoubleRecordHeld lane = edouble_record_held_at(&program, lane_held, -2ll, 20u, 3u);
    const unsigned int outputs[15] = {sum.down,    sum.up,     sum.exponent,    product.down, product.up,
                                      product.exponent, lifted.down, lifted.up, lifted.exponent, cut.down,
                                      cut.up,      cut.exponent, lane.down,     lane.up,      lane.exponent};
    for (unsigned int lane_at = 0u; lane_at < EDOUBLE_TEST_LANES; lane_at += 1u)
    {
        s_atoms[lane_at] = edouble_random();
    }
    const int same = edouble_run(results, &program, outputs, 15u);
    edouble_check(results, same, "known exponents: one program, device = host word for word over 4096 lanes");
    edouble_check(results, sum_steps <= 6u, "known exponents: the held sum takes six steps, no lane-side exponent work");
    unsigned int sums = 0u;
    unsigned int products = 0u;
    unsigned int lifts = 0u;
    unsigned int cuts = 0u;
    unsigned int lanes = 0u;
    for (unsigned int lane_at = 0u; (same != 0) && (lane_at < EDOUBLE_TEST_LANES); lane_at += 1u)
    {
        const AnchorExactInteger *const read = &s_read[(size_t)lane_at * 15u];
        long long m[2];
        long long top[2];
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            m[side] = edouble_field(s_atoms[lane_at], 16u * side, 10u);
            top[side] = m[side] + (long long)((s_atoms[lane_at] >> ((16u * side) + 10u)) & 7u);
        }
        const long long e = edouble_field(s_atoms[lane_at], 13u, 3u);
        const long long sum_down = m[0] * 256 + m[1];
        const long long sum_up = top[0] * 256 + top[1];
        sums += (unsigned int)((edouble_word(&read[0]) == sum_down) && (edouble_word(&read[1]) == sum_up) &&
                               (edouble_word(&read[2]) == -5));
        long long least = 0;
        long long most = 0;
        for (unsigned int corner = 0u; corner < 4u; corner += 1u)
        {
            const long long value = ((corner / 2u) ? top[0] : m[0]) * ((corner % 2u) ? top[1] : m[1]);
            least = (corner == 0u) ? value : ((value < least) ? value : least);
            most = (corner == 0u) ? value : ((value > most) ? value : most);
        }
        products += (unsigned int)((edouble_word(&read[3]) == least) && (edouble_word(&read[4]) == most) &&
                                   (edouble_word(&read[5]) == -2));
        lifts += (unsigned int)((edouble_word(&read[6]) == 4 * sum_down) && (edouble_word(&read[7]) == 4 * sum_up) &&
                                (edouble_word(&read[8]) == -7));
        cuts += (unsigned int)((edouble_word(&read[9]) == edouble_floor_by(least, 4u)) &&
                               (edouble_word(&read[10]) == edouble_ceiling_by(most, 4u)) && (edouble_word(&read[11]) == 2));
        const long long spread = e + 2;
        const long long lane_down = (spread >= 0) ? (m[0] << spread) : edouble_floor_by(m[0], (unsigned int)-spread);
        const long long lane_up = (spread >= 0) ? (top[0] << spread) : edouble_ceiling_by(top[0], (unsigned int)-spread);
        lanes += (unsigned int)((edouble_word(&read[12]) == lane_down) && (edouble_word(&read[13]) == lane_up) &&
                                (edouble_word(&read[14]) == -2));
    }
    edouble_check(results, (same != 0) && (sums == EDOUBLE_TEST_LANES),
                  "known exponents: the held sum's ends are the exact sums at 2^-5");
    edouble_check(results, (same != 0) && (products == EDOUBLE_TEST_LANES),
                  "known exponents: the held product's ends are the least and the most corner at 2^-2");
    edouble_check(results, (same != 0) && (lifts == EDOUBLE_TEST_LANES), "the sum read at 2^-7 is the sum, exact");
    edouble_check(results, (same != 0) && (cuts == EDOUBLE_TEST_LANES),
                  "the product read at 2^2 is the lower end's floor and the upper end's ceiling");
    edouble_check(results, (same != 0) && (lanes == EDOUBLE_TEST_LANES),
                  "a lane's own exponent read at 2^-2: lifted exact, or the floor and ceiling below it");
    exact_record_close(&program);
}

// m 2^e times 2^64, an exact integer, or sign 2 where it does not fit
static AnchorExactInteger edouble_value(long long m, long long e)
{
    AnchorExactInteger small;
    AnchorExactInteger out;
    edouble_small(&small, m);
    if (!edouble_lifted(&small, e, &out))
    {
        out.sign = 2;
    }
    return out;
}

static AnchorExactInteger edouble_plus(const AnchorExactInteger &a, const AnchorExactInteger &b)
{
    AnchorExactInteger out;
    if (anchor_exact_add(&a, &b, &out) != ANCHOR_EXACT_OK)
    {
        out.sign = 2;
    }
    return out;
}

static AnchorExactInteger edouble_minus(const AnchorExactInteger &a, const AnchorExactInteger &b)
{
    AnchorExactInteger out;
    if (anchor_exact_subtract(&a, &b, &out) != ANCHOR_EXACT_OK)
    {
        out.sign = 2;
    }
    return out;
}

static AnchorExactInteger edouble_times(const AnchorExactInteger &a, const AnchorExactInteger &b)
{
    AnchorExactInteger out;
    if (anchor_exact_multiply(&a, &b, &out) != ANCHOR_EXACT_OK)
    {
        out.sign = 2;
    }
    return out;
}

static AnchorExactInteger edouble_size(AnchorExactInteger a)
{
    a.sign = (a.sign < 0) ? 1 : a.sign;
    return a;
}

// balls at exponents the program knows, their fields laid end to end: a = C_a 2^2 within R_a 2^-1 and b = C_b 2^-3
// within R_b 2^-4, C signed 10 bits
// and R 4 and 3 bits; their sum and product exact, the product read at 2^1 within 2^-2, the quotient at 2^-6 within
// 2^-8, and the order, each held against the exact integers of value 2^64
static void edouble_ball_case(EdoubleResults *results)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int fields[4] = {exact_record_field(&program, 10u), exact_record_field(&program, 4u),
                                    exact_record_field(&program, 10u), exact_record_field(&program, 3u)};
    const EdoubleRecordBall a = edouble_record_ball_of(
        edouble_record_of(exact_record_read(&program, fields[0], 0u), exact_record_signed(&program, 2ll)),
        edouble_record_of(exact_record_read_unsigned(&program, fields[1], 0u), exact_record_signed(&program, -1ll)));
    const EdoubleRecordBall b = edouble_record_ball_of(
        edouble_record_of(exact_record_read(&program, fields[2], 0u), exact_record_signed(&program, -3ll)),
        edouble_record_of(exact_record_read_unsigned(&program, fields[3], 0u), exact_record_signed(&program, -4ll)));
    const EdoubleRecordBall sum = edouble_record_ball_sum(&program, a, b, 3u);
    const EdoubleRecordBall product = edouble_record_ball_product(&program, a, b, 3u);
    unsigned int at_fits = 0u;
    unsigned int quotient_fits = 0u;
    const EdoubleRecordBall at = edouble_record_ball_at(&program, product, 1ll, 24u, -2ll, 16u, 0u, &at_fits);
    const EdoubleRecordBall quotient = edouble_record_ball_quotient(&program, a, b, -6ll, 32u, -8ll, 40u, &quotient_fits);
    const unsigned int outputs[15] = {sum.center.mantissa,     sum.center.exponent,     sum.radius.mantissa,
                                      sum.radius.exponent,     product.center.mantissa, product.center.exponent,
                                      product.radius.mantissa, product.radius.exponent, at.center.mantissa,
                                      at.radius.mantissa,      at_fits,                 quotient.center.mantissa,
                                      quotient.radius.mantissa, quotient_fits,          edouble_record_ball_above(&program, a, b, 3u)};
    for (unsigned int lane = 0u; lane < EDOUBLE_TEST_LANES; lane += 1u)
    {
        s_atoms[lane] = edouble_random();
    }
    const int same = edouble_run(results, &program, outputs, 15u);
    edouble_check(results, same, "balls: one program, device = host word for word over 4096 lanes");
    unsigned int sums = 0u;
    unsigned int products = 0u;
    unsigned int reads = 0u;
    unsigned int quotients = 0u;
    unsigned int ordered = 0u;
    unsigned int cleared = 0u;
    for (unsigned int lane = 0u; (same != 0) && (lane < EDOUBLE_TEST_LANES); lane += 1u)
    {
        const AnchorExactInteger *const read = &s_read[(size_t)lane * 15u];
        const unsigned int atom = s_atoms[lane];
        const long long ca = edouble_field(atom, 0u, 10u);
        const long long ra = (long long)((atom >> 10u) & 15u);
        const long long cb = edouble_field(atom, 14u, 10u);
        const long long rb = (long long)((atom >> 24u) & 7u);
        const AnchorExactInteger center_a = edouble_value(ca, 2);
        const AnchorExactInteger radius_a = edouble_value(ra, -1);
        const AnchorExactInteger center_b = edouble_value(cb, -3);
        const AnchorExactInteger radius_b = edouble_value(rb, -4);
        // the sum, exact
        {
            const AnchorExactInteger center = edouble_value(edouble_word(&read[0]), edouble_word(&read[1]));
            const AnchorExactInteger radius = edouble_value(edouble_word(&read[2]), edouble_word(&read[3]));
            sums += (unsigned int)(anchor_exact_equal(&center, &edouble_plus(center_a, center_b)) &&
                                   anchor_exact_equal(&radius, &edouble_plus(radius_a, radius_b)));
        }
        // the product, exact: c_a c_b within |c_a| r_b + |c_b| r_a + r_a r_b
        const AnchorExactInteger product_center = edouble_value(edouble_word(&read[4]), edouble_word(&read[5]));
        const AnchorExactInteger product_radius = edouble_value(edouble_word(&read[6]), edouble_word(&read[7]));
        {
            const long long size_a = (ca < 0) ? -ca : ca;
            const long long size_b = (cb < 0) ? -cb : cb;
            const AnchorExactInteger want_center = edouble_value(ca * cb, -1);
            const AnchorExactInteger want_radius = edouble_plus(
                edouble_plus(edouble_value(size_a * rb, -2), edouble_value(size_b * ra, -4)), edouble_value(ra * rb, -5));
            products += (unsigned int)(anchor_exact_equal(&product_center, &want_center) &&
                                       anchor_exact_equal(&product_radius, &want_radius));
        }
        // the read: it holds the product's ball, |c' - c| + r <= r', and r' <= r + 2^1 + 2^-2
        {
            const AnchorExactInteger center = edouble_value(edouble_word(&read[8]), 1);
            const AnchorExactInteger radius = edouble_value(edouble_word(&read[9]), -2);
            const AnchorExactInteger reach = edouble_plus(edouble_size(edouble_minus(center, product_center)), product_radius);
            const AnchorExactInteger most = edouble_plus(edouble_plus(product_radius, edouble_value(1, 1)), edouble_value(1, -2));
            reads += (unsigned int)((anchor_exact_compare(&reach, &radius) <= 0) && (anchor_exact_compare(&radius, &most) <= 0) &&
                                    (edouble_word(&read[10]) == 1));
        }
        // the quotient: where b's ball holds no 0, every corner x / y lies within r' of q, |x - q y| <= r' |y| at
        // 2^128; where it holds 0, the flag is 0
        {
            const int clear = (2 * ((cb < 0) ? -cb : cb)) > rb;
            const AnchorExactInteger q = edouble_value(edouble_word(&read[11]), -6);
            const AnchorExactInteger r = edouble_value(edouble_word(&read[12]), -8);
            const AnchorExactInteger unit = edouble_value(1, 0);
            int held = clear ? (edouble_word(&read[13]) == 1) : (edouble_word(&read[13]) == 0);
            for (unsigned int corner = 0u; clear && held && (corner < 4u); corner += 1u)
            {
                const AnchorExactInteger x =
                    (corner / 2u) ? edouble_plus(center_a, radius_a) : edouble_minus(center_a, radius_a);
                const AnchorExactInteger y =
                    (corner % 2u) ? edouble_plus(center_b, radius_b) : edouble_minus(center_b, radius_b);
                const AnchorExactInteger gap = edouble_size(edouble_minus(edouble_times(x, unit), edouble_times(q, y)));
                const AnchorExactInteger allowed = edouble_times(r, edouble_size(y));
                held = held && (gap.sign != 2) && (allowed.sign != 2) && (anchor_exact_compare(&gap, &allowed) <= 0);
            }
            quotients += (unsigned int)held;
            cleared += (unsigned int)clear;
        }
        // above: a's lower end over b's upper
        {
            const int above = anchor_exact_compare(&edouble_minus(center_a, radius_a), &edouble_plus(center_b, radius_b)) > 0;
            ordered += (unsigned int)(edouble_word(&read[14]) == (above ? 1ll : 0ll));
        }
    }
    edouble_check(results, (same != 0) && (sums == EDOUBLE_TEST_LANES), "balls: the sum's center and radius are exact");
    edouble_check(results, (same != 0) && (products == EDOUBLE_TEST_LANES),
                  "balls: the product is c_a c_b within |c_a| r_b + |c_b| r_a + r_a r_b, exact");
    edouble_check(results, (same != 0) && (reads == EDOUBLE_TEST_LANES),
                  "balls: the read at 2^1 holds the product's ball within a unit of each exponent, and fits");
    edouble_check(results, (same != 0) && (quotients == EDOUBLE_TEST_LANES) && (cleared > EDOUBLE_TEST_LANES / 2u),
                  "balls: every corner quotient lies in the quotient's ball, flagged 0 where b's ball holds 0");
    edouble_check(results, (same != 0) && (ordered == EDOUBLE_TEST_LANES), "balls: above is a's lower end over b's upper");
    exact_record_close(&program);
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
        const unsigned int outputs[11] = {product.mantissa,
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
        const int same = edouble_run(&results, &program, outputs, 11u);
        edouble_check(&results, same, "one program, device = host word for word over 4096 lanes");
        unsigned int exact_product = 0u;
        unsigned int exact_sum = 0u;
        unsigned int ordered = 0u;
        unsigned int cut_held = 0u;
        unsigned int quotient_held = 0u;
        for (unsigned int lane = 0u; (same != 0) && (lane < EDOUBLE_TEST_LANES); lane += 1u)
        {
            const AnchorExactInteger *const read = &s_read[(size_t)lane * 11u];
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
        edouble_held_case(&results);
        edouble_known_case(&results);
        edouble_ball_case(&results);
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
