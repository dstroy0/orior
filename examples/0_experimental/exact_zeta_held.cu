// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-035
//
// Turing's method in held form over one cell of the lattice in u, t = 2 pi u^4: the remainder, ln u, theta / pi, the
// main sum, Z's sign, the sign changes and Turing's sums, swept on the device as programs of the record machine, one job on the
// device's tessera daemon. Every value is a mantissa in a register and a binary exponent the program holds: a product
// multiplies the mantissas and adds the exponents, and a power of two is where a mantissa is laid in the record. No
// value is held at a fixed scale. The quotients are the phase's and the clock's reads at 2^-W, below and above,
// which the verdict and the count ask for.
//
//   Usage:  exact_zeta_held <input> <output>
//
// Lane l stands at u = U / 2^b, U = U_0 + l, U_0 the least U with U^2 >= nu 4^b, over every U with U^2 < (nu + 1) 4^b.
// Then x = u^2 = U^2 / 4^b, s = u^4, z = 1 - 2 (x - nu) = Zt / 4^b with Zt = 4^b - 2 (U^2 - nu 4^b), and
// x^(-1/2) = 2^b / U, each exact: no root is taken and nothing is divided by 2 pi.
//
// The curves: C_n(z) is the sum over j of g_(n,j) z^j, each g a mantissa at the exponent -E, read by Horner's rule as
// the integer H_n = sum over j of g_(n,j) Zt^j 4^(b (J_n - 1 - j)) at the exponent -(E + 2 b (J_n - 1)). The record
// holds each g_(n,j) 4^(b (J_n - 1 - j)), the mantissa laid 2 b (J_n - 1 - j) bits up with zeros below, and each step
// of Horner's rule is one product by Zt and one sum. With the sign s = (-1)^(nu - 1), the remainder's curve n is
// s C_n(z) x^(-n - 1/2) = s H_n 2^(b (2n + 1)) / U^(2n + 1). Each curve's coefficients are laid further up until every
// curve stands at the least exponent. Over the common denominator U^(2K + 1) the remainder is the integer
// s sum over n of H_n U^(2 (K - n)), by Horner's rule in U^2: each curve a program, which reads the sum the program
// before it gives as member 1, turns it by U^2 and adds H_n. The last gives the remainder, the denominator and U^2.
//
// The logarithm: ln u = ln(U_0 / 2^b) + A, A = ln(U / U_0) = 2 artanh(l / D), D = U + U_0: one series a lane serves
// theta and every term of the main sum, and ln(U_0 / 2^b) is the cell's. With c_k = Lambda / (2k + 1), Lambda the
// least common multiple of the odd numbers below 2L, the first L terms of the series are 2 l S / (Lambda D^(2L - 1))
// with S = sum over k < L of c_k l^(2k) D^(2(L - 1 - k)), an integer read by Horner's rule in l^2 against the powers
// of D^2. Every term is positive and each is below (l / D)^2 times the one before it: the rest past the last term is
// positive and below 2 l^(2L + 1) / ((2L + 1) D^(2L - 1) (D^2 - l^2)), and the stage holds the four integers l S,
// D^(2L - 1), l^(2L + 1) and D^2 - l^2 as its outputs: A held as its real and its operator.
//
// theta / pi = 4 u^4 ln u - u^4 - 1/8 + 1 / (96 pi^2 u^4) + 7 / (46080 pi^4 u^12) + 31 / (2580480 pi^6 u^20) + R / pi,
// |R| < 1 / (3322 t^7) for t >= 10 (Gabcke's thesis, introduction, (4) and (5)), at t = 2 pi u^4. The held constants
// come at 2^-W, each below and above: ln(U_0 / 2^b), 1 / (96 pi^2), 7 / (46080 pi^4), 31 / (2580480 pi^6), and
// 1 / (425216 pi^8) above, the bound on |R| / pi times u^28. Over the denominator Qa U^28 2^(4b + W + 3), Qa =
// Lambda D^(2L - 1) (2L + 1) (D^2 - l^2), every term is an integer: two programs read the log stage's records as
// member 1 and give theta / pi's numerator below and above, with the denominator's odd part.
//
// The main sum: Z = 2 sum over n to nu of n^(-1/2) cos(pi phi_n) + R, phi_n = theta / pi - 2 u^4 ln n, one lane a
// pair (point, n), each pole's record a body read through the index. The phase stage reads phi_n at 2^-W below and
// above and folds it to [0, 1/2] by cos's symmetries; the cos stage sums cos's series in y = m^2 by Horner's rule
// over held coefficients; the term stages take the term below and above, and the device sums them over each point.
//
// The verdict: Z's bracket, the sums doubled, R added, and Gabcke's bound on R_K and the coefficient reads on either
// side, over one denominator, and Z's sign where the bracket holds no zero. The change stage reads each point's sign
// against the next, and the device sums the changes over the cell.
//
// The count: the clock stage reads theta / pi at every point at 2^-W below and above, and the Turing stage, one lane a
// step, lays each step's share of the sums of Turing's method over the window below point P / 4 and the one past
// point 3P / 4, which the device sums over the cell; the driver holds N at the two points from them.
//
// The input, little-endian: 64-bit words lanes, checked, nu, b, K, E, L, W, J, then U_0 as a mantissa, then K + 1
// words J_n, then the mantissas g_(n,j), then the L constants c_k, then theta's nine held constants, Lambda and
// 2L + 1, then cos's bound B and its J coefficients k_j below, then each pole's ln n below and above and n^(-1/2)
// below and above, then the verdict's bound; a mantissa is a 64-bit word count w, w 32-bit limbs of its magnitude
// least significant first, and a 64-bit sign word, -1, 0 or 1.
//
// The output, a stage at a time, the last curve, the logarithm, theta / pi below and above, and the verdict: the line
// "stage name out_limbs L", one line an output "output name offset bits exponent", one line a lane of L hex limbs,
// least significant first, the line "host 1" where the host's records over the first `checked` lanes equal the
// device's word for word, else "host 0", and the line "steps P compiled C". Then the sums, the term stages' over each
// point and the change stage's over the cell: the line "sum stage output exponent L runs R host H" and a line a run.
// Then the clock as a stage, and Turing's sums over the cell.

#include "../../src/c/engine/analysis/cycle/cycle.h"
#include "../../src/c/engine/analysis/key_schedule/key_schedule.h"
#include "../../src/c/engine/analysis/keymath/keymath.h"
#include "../../src/c/engine/runtime/scriptura/scriptura.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

typedef struct
{
    std::vector<EngineRecordStep> steps;
    std::vector<unsigned int> field_bits;
    std::vector<unsigned int> field_offset;
    unsigned int shared_bits;
} HeldProgram;

// a value: the register holding its mantissa and the binary exponent the program holds for it
typedef struct
{
    unsigned int reg;
    long long exponent;
} HeldReal;

// a program, the outputs it lists with their names and exponents, its shared record, and what the device made of it
typedef struct
{
    const char *name;
    HeldProgram program;
    std::vector<unsigned int> outputs;
    std::vector<std::string> names;
    std::vector<long long> exponents;
    std::vector<unsigned int> shared;
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
    std::vector<unsigned int> records;
    int same;
    // the shared record's width in limbs; `shared` holds one record or one a body, read through the index
    unsigned int shared_limbs;
    // the records this one reads as member 1 and as member 2, and their widths in limbs, or none
    const std::vector<unsigned int> *before;
    unsigned int before_limbs;
    const std::vector<unsigned int> *also;
    unsigned int also_limbs;
    // the body each lane reads of each member, a lane's members in a row, or empty where a lane reads its own
    std::vector<unsigned int> index;
    // the outputs summed over each run of `group` lanes on the device, and the sums, `sum_limbs` limbs each
    unsigned long long group;
    std::vector<unsigned int> summed;
    std::vector<unsigned int> sum_limbs;
    std::vector<std::vector<unsigned int>> sums;
    int sums_same;
} HeldStage;

static unsigned int held_step(HeldProgram *program, EngineRecordOperation operation, unsigned int left,
                               unsigned int right, unsigned int member)
{
    EngineRecordStep step = {operation, left, right, member};
    program->steps.push_back(step);
    return (unsigned int)program->steps.size() - 1u;
}

static unsigned int held_constant(HeldProgram *program, unsigned long long value)
{
    return held_step(program, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFull), (unsigned int)(value >> 32u),
                      0u);
}

// a field of the shared record, `bits` wide and signed, laid at the record's next bit; the step that reads it is
// written where it is read, and the register is live only from there to its reader
static unsigned int held_field(HeldProgram *program, unsigned int bits)
{
    const unsigned int field = (unsigned int)program->field_bits.size();
    program->field_bits.push_back(bits);
    program->field_offset.push_back(program->shared_bits);
    program->shared_bits += bits;
    return field;
}

static unsigned int held_read(HeldProgram *program, unsigned int field)
{
    return held_step(program, ENGINE_RECORD_FIELD_SIGNED, field, 0u, 0u);
}

// a field of member 1's record, `bits` wide at `offset`, an output of the stage before
static unsigned int held_member_field(HeldProgram *program, unsigned int bits, unsigned int offset)
{
    const unsigned int field = (unsigned int)program->field_bits.size();
    program->field_bits.push_back(bits);
    program->field_offset.push_back(offset);
    return field;
}

static unsigned int held_read_before(HeldProgram *program, unsigned int field)
{
    return held_step(program, ENGINE_RECORD_FIELD_SIGNED, field, 0u, 1u);
}

static unsigned int held_read_also(HeldProgram *program, unsigned int field)
{
    return held_step(program, ENGINE_RECORD_FIELD_SIGNED, field, 0u, 2u);
}

// the field of a stage's output `at`, read from that stage's records as a member
static unsigned int held_output_field(HeldProgram *program, const HeldStage *from, unsigned int at)
{
    const DeviceRecordStep *const place = &from->layout.step_table[from->outputs[at]];
    return held_member_field(program, place->out_bits, place->out_offset);
}

static void held_output(HeldStage *stage, const std::string &name, unsigned int step, long long exponent)
{
    stage->outputs.push_back(step);
    stage->names.push_back(name);
    stage->exponents.push_back(exponent);
}

// the lane, below 2^bits, and the and with 2^bits - 1 gives its register that width and keeps its value
static unsigned int held_lane_below(HeldProgram *program, unsigned int bits)
{
    const unsigned int counted = held_step(program, ENGINE_RECORD_LANE, 0u, 0u, 0u);
    return held_step(program, ENGINE_RECORD_AND, counted, held_constant(program, (1ull << bits) - 1ull), 0u);
}

// the lane, at most 2^b
static unsigned int held_lane(HeldProgram *program, unsigned int b)
{
    return held_lane_below(program, b + 1u);
}

// [left > right], 1 or 0: (c + |c|) / 2, c = compare(left, right)
static unsigned int held_above(HeldProgram *program, unsigned int left, unsigned int right)
{
    const unsigned int c = held_step(program, ENGINE_RECORD_COMPARE, left, right, 0u);
    return held_step(program, ENGINE_RECORD_QUOTIENT,
                     held_step(program, ENGINE_RECORD_SUM, c, held_step(program, ENGINE_RECORD_ABSOLUTE, c, 0u, 0u), 0u),
                     held_constant(program, 2ull), 0u);
}

// 2^bits as a register: one constant below 64 bits, a product of constants past it
static unsigned int held_power(HeldProgram *program, unsigned int bits)
{
    unsigned int out = held_constant(program, 1ull << (bits % 63u));
    for (unsigned int at = 0u; at < bits / 63u; at += 1u)
    {
        out = held_step(program, ENGINE_RECORD_PRODUCT, out, held_constant(program, 1ull << 63u), 0u);
    }
    return out;
}

// the exponent curve n's H_n 2^(b (2n + 1)) stands at, and the least of them over the curves
static long long held_curve_exponent(long long big_e, unsigned int b, unsigned int n, size_t count)
{
    return -big_e - 2ll * (long long)b * (long long)(count - 1u) + (long long)b * (long long)(2u * n + 1u);
}

static long long held_least_exponent(long long big_e, unsigned int b, const std::vector<std::vector<unsigned int>> &gamma_bits)
{
    long long least = held_curve_exponent(big_e, b, 0u, gamma_bits[0].size());
    for (size_t n = 1u; n < gamma_bits.size(); n += 1u)
    {
        const long long at = held_curve_exponent(big_e, b, (unsigned int)n, gamma_bits[n].size());
        least = (at < least) ? at : least;
    }
    return least;
}

// curve n of the remainder: the sum before, turned by U^2, and H_n added, every curve at the exponent `least`, its
// coefficients' widths gamma_bits[j], each laid `lift` bits further up; the field of U_0 `first_bits` wide. Zt =
// 4^b - 2V, V = U^2 - nu 4^b, the remainder of U^2 by 4^b where nu 4^b <= U^2 < (nu + 1) 4^b, and 2b + 1 bits wide.
// The first curve has no sum before; the last gives s times the sum, the denominator U^(2K + 1) and U^2
static void held_build(HeldStage *stage, unsigned int b, unsigned int n, unsigned int top, long long least,
                       unsigned int first_bits, const std::vector<unsigned int> &gamma_bits, unsigned int lift,
                       const HeldStage *before)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int sign_field = held_field(program, 2u);
    const unsigned int first_field = held_field(program, first_bits);
    const unsigned int count = (unsigned int)gamma_bits.size();
    std::vector<unsigned int> gamma;
    for (unsigned int j = 0u; j < count; j += 1u)
    {
        gamma.push_back(held_field(program, gamma_bits[j] + 2u * b * (count - 1u - j) + lift));
    }
    unsigned int sum_field = 0u;
    if (before != NULL)
    {
        const DeviceRecordStep *const place = &before->layout.step_table[before->outputs[0]];
        sum_field = held_member_field(program, place->out_bits, place->out_offset);
    }
    const unsigned int lane = held_lane(program, b);
    const unsigned int big_u = held_step(program, ENGINE_RECORD_SUM, lane, held_read(program, first_field), 0u);
    const unsigned int square = held_step(program, ENGINE_RECORD_PRODUCT, big_u, big_u, 0u);
    const unsigned int whole = held_power(program, 2u * b);
    const unsigned int past = held_step(program, ENGINE_RECORD_REMAINDER, square, whole, 0u);
    const unsigned int zt = held_step(program, ENGINE_RECORD_DIFFERENCE, whole, held_step(program, ENGINE_RECORD_SUM, past, past, 0u),
                                      0u);
    // H_n by Horner's rule in Zt over its coefficients
    unsigned int acc = held_read(program, gamma[count - 1u]);
    for (unsigned int j = count - 1u; j > 0u; j -= 1u)
    {
        const unsigned int turned = held_step(program, ENGINE_RECORD_PRODUCT, acc, zt, 0u);
        acc = held_step(program, ENGINE_RECORD_SUM, turned, held_read(program, gamma[j - 1u]), 0u);
    }
    const unsigned int total =
        (before == NULL) ? acc
                         : held_step(program, ENGINE_RECORD_SUM,
                                     held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, sum_field), square, 0u),
                                     acc, 0u);
    if (n < top)
    {
        held_output(stage, "sum", total, least);
        return;
    }
    const unsigned int sign = held_read(program, sign_field);
    held_output(stage, "remainder", held_step(program, ENGINE_RECORD_PRODUCT, total, sign, 0u), least);
    unsigned int power = big_u;
    for (unsigned int k = 0u; k < top; k += 1u)
    {
        power = held_step(program, ENGINE_RECORD_PRODUCT, power, square, 0u);
    }
    held_output(stage, "denominator", power, 0);
    held_output(stage, "square", square, -2ll * (long long)b);
}

// the logarithm: l S, D^(2L - 1), l^(2L + 1) and D^2 - l^2, D = U + U_0, from the widths of the constants c_k and the
// field of U_0, `first_bits` wide
static void held_log_build(HeldStage *stage, unsigned int b, unsigned int first_bits, const std::vector<unsigned int> &c_bits)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int first_field = held_field(program, first_bits);
    const unsigned int terms = (unsigned int)c_bits.size();
    std::vector<unsigned int> c(terms);
    for (unsigned int k = 0u; k < terms; k += 1u)
    {
        c[k] = held_field(program, c_bits[k]);
    }
    const unsigned int lane = held_lane(program, b);
    const unsigned int big_u = held_step(program, ENGINE_RECORD_SUM, lane, held_read(program, first_field), 0u);
    const unsigned int big_d = held_step(program, ENGINE_RECORD_SUM, big_u, held_read(program, first_field), 0u);
    const unsigned int u = held_step(program, ENGINE_RECORD_PRODUCT, lane, lane, 0u);
    const unsigned int v = held_step(program, ENGINE_RECORD_PRODUCT, big_d, big_d, 0u);
    // Horner's rule from c_(L-1) down: the step that adds c_(k-1) turns the sum by l^2 and lays c_(k-1) at D^(2(L - k))
    unsigned int acc = held_read(program, c[terms - 1u]);
    unsigned int v_power = v;
    for (unsigned int k = terms - 1u; k > 0u; k -= 1u)
    {
        const unsigned int turned = held_step(program, ENGINE_RECORD_PRODUCT, acc, u, 0u);
        const unsigned int laid = held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, c[k - 1u]), v_power, 0u);
        acc = held_step(program, ENGINE_RECORD_SUM, turned, laid, 0u);
        if (k > 1u)
        {
            v_power = held_step(program, ENGINE_RECORD_PRODUCT, v_power, v, 0u);
        }
    }
    held_output(stage, "numerator", held_step(program, ENGINE_RECORD_PRODUCT, lane, acc, 0u), 0);
    held_output(stage, "power", held_step(program, ENGINE_RECORD_PRODUCT, big_d, v_power, 0u), 0);
    unsigned int tail = lane;
    for (unsigned int k = 0u; k < terms; k += 1u)
    {
        tail = held_step(program, ENGINE_RECORD_PRODUCT, tail, u, 0u);
    }
    held_output(stage, "tail", tail, 0);
    held_output(stage, "gap", held_step(program, ENGINE_RECORD_DIFFERENCE, v, u, 0u), 0);
}

// theta / pi at u = U / 2^b, below where `side` is 0 and above where it is 1, over its denominator, from the log stage's
// records as member 1 and the held constants at 2^-W: ln(U_0 / 2^b) less and more, 1 / (96 pi^2), 7 / (46080 pi^4) and 31 / (2580480 pi^6) less
// and more, the bound on |R_theta| / pi times u^28, Lambda and 2L + 1. Every constant field is `widths[k]` wide in the
// order the stage reads them, and the field of U_0 `first_bits` wide
static void held_theta_build(HeldStage *stage, unsigned int side, unsigned int b, unsigned int w, unsigned int first_bits,
                             const std::vector<unsigned int> &widths, const HeldStage *log)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int first_field = held_field(program, first_bits);
    std::vector<unsigned int> constant(widths.size());
    for (size_t k = 0u; k < widths.size(); k += 1u)
    {
        constant[k] = held_field(program, widths[k]);
    }
    std::vector<unsigned int> from_log(4u);
    for (unsigned int k = 0u; k < 4u; k += 1u)
    {
        const DeviceRecordStep *const place = &log->layout.step_table[log->outputs[k]];
        from_log[k] = held_member_field(program, place->out_bits, place->out_offset);
    }
    const unsigned int lane = held_lane(program, b);
    const unsigned int big_u = held_step(program, ENGINE_RECORD_SUM, lane, held_read(program, first_field), 0u);
    const unsigned int u2 = held_step(program, ENGINE_RECORD_PRODUCT, big_u, big_u, 0u);
    const unsigned int u4 = held_step(program, ENGINE_RECORD_PRODUCT, u2, u2, 0u);
    const unsigned int u8 = held_step(program, ENGINE_RECORD_PRODUCT, u4, u4, 0u);
    const unsigned int u16 = held_step(program, ENGINE_RECORD_PRODUCT, u8, u8, 0u);
    const unsigned int u24 = held_step(program, ENGINE_RECORD_PRODUCT, u16, u8, 0u);
    const unsigned int u28 = held_step(program, ENGINE_RECORD_PRODUCT, u24, u4, 0u);
    const unsigned int u32 = held_step(program, ENGINE_RECORD_PRODUCT, u16, u16, 0u);
    // A lies in [2 l S / Q, that plus 2 l^(2L + 1) / ((2L + 1) D^(2L - 1) (D^2 - l^2))], Q = Lambda D^(2L - 1); over
    // Qa = Q (2L + 1) (D^2 - l^2) its ends are 2 l S (2L + 1) (D^2 - l^2) and that plus 2 Lambda l^(2L + 1)
    const unsigned int lambda = held_read(program, constant[9]);
    const unsigned int odd = held_read(program, constant[10]);
    const unsigned int numerator = held_read_before(program, from_log[0]);
    const unsigned int power = held_read_before(program, from_log[1]);
    const unsigned int rest = held_read_before(program, from_log[2]);
    const unsigned int gap = held_read_before(program, from_log[3]);
    const unsigned int odd_gap = held_step(program, ENGINE_RECORD_PRODUCT, odd, gap, 0u);
    const unsigned int qa = held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, lambda, power, 0u),
                                      odd_gap, 0u);
    const unsigned int a_low = held_step(program, ENGINE_RECORD_PRODUCT,
                                         held_step(program, ENGINE_RECORD_SUM, numerator, numerator, 0u), odd_gap, 0u);
    const unsigned int lambda_rest = held_step(program, ENGINE_RECORD_PRODUCT, lambda, rest, 0u);
    const unsigned int a_end = (side == 0u) ? a_low
                                            : held_step(program, ENGINE_RECORD_SUM, a_low,
                                                        held_step(program, ENGINE_RECORD_SUM, lambda_rest, lambda_rest, 0u), 0u);
    const unsigned int big_w = held_power(program, w);
    const unsigned int qa_w = held_step(program, ENGINE_RECORD_PRODUCT, qa, big_w, 0u);
    // the cell's part 1 / 8, over the denominator Qa U^28 2^(4b + W + 3)
    const unsigned int eighth = held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, qa_w, u28, 0u),
                                          held_power(program, 4u * b), 0u);
    {
        // ln u over Qa 2^W: ln(U_0 / 2^b) Qa + A's end 2^W
        const unsigned int ln_u = held_step(program, ENGINE_RECORD_SUM,
                                            held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, constant[side]), qa, 0u),
                                            held_step(program, ENGINE_RECORD_PRODUCT, a_end, big_w, 0u), 0u);
        // u^4 (4 ln u - 1): 8 U^32 (4 ln u's numerator - Qa 2^W)
        const unsigned int four = held_step(program, ENGINE_RECORD_SUM, ln_u, ln_u, 0u);
        const unsigned int inner = held_step(program, ENGINE_RECORD_DIFFERENCE, held_step(program, ENGINE_RECORD_SUM, four, four, 0u),
                                             qa_w, 0u);
        const unsigned int head = held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, u32, inner, 0u),
                                            held_constant(program, 8ull), 0u);
        // the series past the head: Qa 2^(8b + 3) (P2 U^24 + P4 U^16 2^(8b) + P6 U^8 2^(16b) -+ B U^0 2^(24b))
        const unsigned int p2 = held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, constant[2u + side]), u24, 0u);
        const unsigned int p4 = held_step(program, ENGINE_RECORD_PRODUCT,
                                          held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, constant[4u + side]), u16, 0u),
                                          held_power(program, 8u * b), 0u);
        const unsigned int p6 = held_step(program, ENGINE_RECORD_PRODUCT,
                                          held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, constant[6u + side]), u8, 0u),
                                          held_power(program, 16u * b), 0u);
        const unsigned int bound = held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, constant[8]),
                                             held_power(program, 24u * b), 0u);
        const unsigned int series = held_step(program, (side == 0u) ? ENGINE_RECORD_DIFFERENCE : ENGINE_RECORD_SUM,
                                              held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_SUM, p2, p4, 0u),
                                                        p6, 0u),
                                              bound, 0u);
        const unsigned int tail = held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, qa, series, 0u),
                                            held_power(program, 8u * b + 3u), 0u);
        held_output(stage, (side == 0u) ? "lower" : "upper",
                    held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_DIFFERENCE, head, eighth, 0u), tail, 0u),
                    -(long long)(4u * b + w + 3u));
    }
    held_output(stage, "denominator", held_step(program, ENGINE_RECORD_PRODUCT, qa, u28, 0u), 0);
}

// the pole's record, one a body, read through the index as member 0: U_0, then ln n below and above and n^(-1/2)
// below and above at 2^-W, then B, the bound on cos's rest at 2^-W; every program that reads it lays it alike
enum
{
    HELD_POLE_FIRST,
    HELD_POLE_LN_LOW,
    HELD_POLE_LN_HIGH,
    HELD_POLE_ROOT_LOW,
    HELD_POLE_ROOT_HIGH,
    HELD_POLE_BOUND,
    HELD_POLE_FIELDS
};

static std::vector<unsigned int> held_pole_fields(HeldProgram *program, const std::vector<unsigned int> &widths)
{
    program->shared_bits = 0u;
    std::vector<unsigned int> fields(HELD_POLE_FIELDS);
    for (unsigned int k = 0u; k < HELD_POLE_FIELDS; k += 1u)
    {
        fields[k] = held_field(program, widths[k]);
    }
    return fields;
}

// the phase phi_n = theta / pi - 2 u^4 ln n at lane (point, n), point nu + n - 1, read at 2^-W below and above and
// folded: over theta's denominator Qa U^28 2^(4b + W + 3) the phase is N - 16 U^4 Qa U^28 Ln, N and Ln each below
// and above; a and c, the quotients by Qa U^28 2^(4b + 3) less 1 and more 1, stand below and above phi 2^W. cos is
// even and of period 2: a's remainder by 2^(W + 1), its magnitude m, 2^(W + 1) - m where m passes 2^W, and
// 2^W - m with the sign turned where m passes 2^(W - 1), leave m in [0, 2^(W - 1)] with cos(pi a 2^-W) = s cos(pi m
// 2^-W). The outputs: y = m^2 at 2^-2W, s, and c - a, which bounds |phi - a 2^-W| by 2^-W times it
static void held_phase_build(HeldStage *stage, unsigned int b, unsigned int w, unsigned long long nu, unsigned int lane_bits,
                             const std::vector<unsigned int> &pole_widths, const HeldStage *lower, const HeldStage *upper)
{
    HeldProgram *const program = &stage->program;
    const std::vector<unsigned int> pole = held_pole_fields(program, pole_widths);
    const unsigned int low_field = held_output_field(program, lower, 0u);
    const unsigned int den_field = held_output_field(program, lower, 1u);
    const unsigned int high_field = held_output_field(program, upper, 0u);
    const unsigned int lane = held_lane_below(program, lane_bits);
    const unsigned int point = held_step(program, ENGINE_RECORD_QUOTIENT, lane, held_constant(program, nu), 0u);
    const unsigned int big_u = held_step(program, ENGINE_RECORD_SUM, point, held_read(program, pole[HELD_POLE_FIRST]), 0u);
    const unsigned int u2 = held_step(program, ENGINE_RECORD_PRODUCT, big_u, big_u, 0u);
    const unsigned int den = held_read_before(program, den_field);
    const unsigned int slope = held_step(program, ENGINE_RECORD_PRODUCT,
                                         held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, u2, u2, 0u),
                                                   den, 0u),
                                         held_constant(program, 16ull), 0u);
    const unsigned int p_low = held_step(program, ENGINE_RECORD_DIFFERENCE, held_read_before(program, low_field),
                                         held_step(program, ENGINE_RECORD_PRODUCT, slope, held_read(program, pole[HELD_POLE_LN_HIGH]), 0u),
                                         0u);
    const unsigned int p_high = held_step(program, ENGINE_RECORD_DIFFERENCE, held_read_also(program, high_field),
                                          held_step(program, ENGINE_RECORD_PRODUCT, slope, held_read(program, pole[HELD_POLE_LN_LOW]), 0u),
                                          0u);
    const unsigned int unit = held_step(program, ENGINE_RECORD_PRODUCT, den, held_power(program, 4u * b + 3u), 0u);
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int a = held_step(program, ENGINE_RECORD_DIFFERENCE, held_step(program, ENGINE_RECORD_QUOTIENT, p_low, unit, 0u),
                                     one, 0u);
    const unsigned int c = held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_QUOTIENT, p_high, unit, 0u), one,
                                     0u);
    const unsigned int whole = held_power(program, w);
    const unsigned int period = held_step(program, ENGINE_RECORD_SUM, whole, whole, 0u);
    const unsigned int m = held_step(program, ENGINE_RECORD_ABSOLUTE, held_step(program, ENGINE_RECORD_REMAINDER, a, period, 0u),
                                     0u, 0u);
    const unsigned int past_whole = held_above(program, m, whole);
    const unsigned int m_twice = held_step(program, ENGINE_RECORD_SUM, m, m, 0u);
    const unsigned int m1 = held_step(program, ENGINE_RECORD_SUM, m,
                                      held_step(program, ENGINE_RECORD_PRODUCT, past_whole,
                                                held_step(program, ENGINE_RECORD_DIFFERENCE, period, m_twice, 0u), 0u),
                                      0u);
    const unsigned int past_half = held_above(program, m1, held_power(program, w - 1u));
    const unsigned int m1_twice = held_step(program, ENGINE_RECORD_SUM, m1, m1, 0u);
    const unsigned int m2 = held_step(program, ENGINE_RECORD_SUM, m1,
                                      held_step(program, ENGINE_RECORD_PRODUCT, past_half,
                                                held_step(program, ENGINE_RECORD_DIFFERENCE, whole, m1_twice, 0u), 0u),
                                      0u);
    held_output(stage, "y", held_step(program, ENGINE_RECORD_PRODUCT, m2, m2, 0u), -2ll * (long long)w);
    held_output(stage, "sign",
                held_step(program, ENGINE_RECORD_DIFFERENCE, one, held_step(program, ENGINE_RECORD_SUM, past_half, past_half, 0u),
                          0u),
                0);
    // c - a held within W + 3 bits: its remainder by 2^(W + 1), and 2^(W + 1) more where that is not c - a itself, so
    // that a spread of 2^(W + 1) or more still bounds |cos(pi phi) - s cos(pi m 2^-W)| by 2 or more
    const unsigned int spread = held_step(program, ENGINE_RECORD_DIFFERENCE, c, a, 0u);
    const unsigned int kept = held_step(program, ENGINE_RECORD_REMAINDER, spread, period, 0u);
    held_output(stage, "spread",
                held_step(program, ENGINE_RECORD_SUM, kept,
                          held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, spread, kept), period, 0u), 0u),
                -(long long)w);
}

// cos(pi m 2^-W) by its first J terms, sum over j < J of (-1)^j k_j y^j 2^(-2Wj), k_j = pi^(2j) / (2j)!: each k_j
// held at 2^-W below, laid 2W (J - 1 - j) bits up with its sign, so that Horner's rule in y gives the sum at 2^-X,
// X = W (2J - 1). The rest past J terms and the k_j's reads, y 2^-2W at most 1/4, together lie within B 2^-W, B the
// pole record's. The phase's sign and spread pass on
static void held_cos_build(HeldStage *stage, const std::vector<unsigned int> &k_bits, const HeldStage *phase)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int terms = (unsigned int)k_bits.size();
    std::vector<unsigned int> k(terms);
    for (unsigned int j = 0u; j < terms; j += 1u)
    {
        k[j] = held_field(program, k_bits[j]);
    }
    const unsigned int y_field = held_output_field(program, phase, 0u);
    const unsigned int sign_field = held_output_field(program, phase, 1u);
    const unsigned int spread_field = held_output_field(program, phase, 2u);
    const unsigned int y = held_read_before(program, y_field);
    unsigned int acc = held_read(program, k[terms - 1u]);
    for (unsigned int j = terms - 1u; j > 0u; j -= 1u)
    {
        acc = held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_PRODUCT, acc, y, 0u),
                        held_read(program, k[j - 1u]), 0u);
    }
    const long long width = -phase->exponents[2];
    held_output(stage, "cos", acc, -(long long)(2u * terms - 1u) * width);
    held_output(stage, "sign", held_read_before(program, sign_field), 0);
    held_output(stage, "spread", held_read_before(program, spread_field), phase->exponents[2]);
}

// the term n^(-1/2) cos(pi phi_n), below where `side` is 0 and above where it is 1, at 2^-(X + W), from the pole's
// record as member 0 and the cos stage's as member 1: with s the phase's sign and v the sum, cos(pi phi) lies within
// (B + 4 (c - a)) 2^-W of s v, pi below 4; and the product with [r_lo, r_hi] takes r_hi at a lower end below zero
// and r_lo at one above, and at an upper end the other way
static void held_term_build(HeldStage *stage, unsigned int side, unsigned int w, const std::vector<unsigned int> &pole_widths,
                            const HeldStage *cos)
{
    HeldProgram *const program = &stage->program;
    const std::vector<unsigned int> pole = held_pole_fields(program, pole_widths);
    const unsigned int cos_field = held_output_field(program, cos, 0u);
    const unsigned int sign_field = held_output_field(program, cos, 1u);
    const unsigned int spread_field = held_output_field(program, cos, 2u);
    const long long x = -cos->exponents[0];
    const unsigned int extra = held_step(program, ENGINE_RECORD_PRODUCT,
                                         held_step(program, ENGINE_RECORD_SUM, held_read(program, pole[HELD_POLE_BOUND]),
                                                   held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, spread_field),
                                                             held_constant(program, 4ull), 0u),
                                                   0u),
                                         held_power(program, (unsigned int)(x - (long long)w)), 0u);
    const unsigned int centre = held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, cos_field),
                                          held_read_before(program, sign_field), 0u);
    const unsigned int root_low = held_read(program, pole[HELD_POLE_ROOT_LOW]);
    const unsigned int root_high = held_read(program, pole[HELD_POLE_ROOT_HIGH]);
    const unsigned int zero = held_constant(program, 0ull);
    const unsigned int span = held_step(program, ENGINE_RECORD_DIFFERENCE, root_high, root_low, 0u);
    const unsigned int end = held_step(program, (side == 0u) ? ENGINE_RECORD_DIFFERENCE : ENGINE_RECORD_SUM, centre, extra, 0u);
    const unsigned int turn = held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, end, zero), span, 0u);
    const unsigned int root = (side == 0u) ? held_step(program, ENGINE_RECORD_DIFFERENCE, root_high, turn, 0u)
                                           : held_step(program, ENGINE_RECORD_SUM, root_low, turn, 0u);
    held_output(stage, (side == 0u) ? "lower" : "upper", held_step(program, ENGINE_RECORD_PRODUCT, end, root, 0u),
                -(x + (long long)w));
}

// Z at the point against zero: Z = 2 sum + R and |Z - 2 sum - R| < G + E_r, G = d_K t^(-(2K + 3) / 4) at most
// d_K (2 pi nu^2)^(-(2K + 3) / 4) over the cell (Gabcke's thesis, Satz 3.2.2, for t >= 200) and E_r the bound on the
// curves' coefficient reads, the two together B 2^-W, B the shared record's. With D = U^(2K + 1), R = r 2^least / D,
// the sums at 2^-S and e the least of -S + 1, least and -W, Z D 2^-e is an integer: 2 sum D 2^(-S + 1 - e) +
// r 2^(least - e) -+ B D 2^(-W - e). Z > 0 where sum_lo D 2^(-S + 1 - e) > T_lo 2^c, and Z < 0 where
// T_hi 2^c > sum_hi D 2^(-S + 1 - e), c the lesser of least - e and -W - e, T_lo = B D 2^(-W - e - c) - r 2^(least - e - c)
// and T_hi = -(B D 2^(-W - e - c) + r 2^(least - e - c)): each side sums before a power of two is laid on, and the
// verdict lays 2^c on where it compares. The tolerance gives T_lo where `side` is 0, with D, and T_hi where it is 1,
// from the remainder's records as member 1
static void held_tolerance_build(HeldStage *stage, unsigned int side, unsigned int bound_bits, const HeldStage *curves,
                                 long long least_shift, long long bound_shift)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int bound_field = held_field(program, bound_bits);
    const unsigned int remainder_field = held_output_field(program, curves, 0u);
    const unsigned int denominator_field = held_output_field(program, curves, 1u);
    const unsigned int den = held_read_before(program, denominator_field);
    const unsigned int bound = held_step(program, ENGINE_RECORD_PRODUCT,
                                         held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, bound_field), den, 0u),
                                         held_power(program, (unsigned int)bound_shift), 0u);
    const unsigned int r = held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, remainder_field),
                                     held_power(program, (unsigned int)least_shift), 0u);
    const unsigned int side_value = (side == 0u) ? held_step(program, ENGINE_RECORD_DIFFERENCE, bound, r, 0u)
                                                 : held_step(program, ENGINE_RECORD_DIFFERENCE, held_constant(program, 0ull),
                                                             held_step(program, ENGINE_RECORD_SUM, bound, r, 0u), 0u);
    held_output(stage, (side == 0u) ? "low" : "high", side_value, 0);
    held_output(stage, "denominator", held_read_before(program, denominator_field), 0);
}

// one side of Z's sign at the point: the term sums below and above as member 0, one body a point, `sum_bits` wide each
// and laid `sum_stride` bits apart, and the tolerance of `side` as member 1, 2^c on it and 2^(-S + 1 - e) on the sum.
// Where `side` is 0 the output is [Z > 0]; where it is 1, member 2 is that side's records and the output is the sign,
// 1, -1, or 0 where neither holds
static void held_verdict_build(HeldStage *stage, unsigned int side, unsigned int sum_bits, unsigned int sum_stride,
                               long long sum_shift, long long common_shift, const HeldStage *tolerance, const HeldStage *above)
{
    HeldProgram *const program = &stage->program;
    const unsigned int sum_field = held_member_field(program, sum_bits, side * sum_stride);
    program->shared_bits = 2u * sum_stride;
    const unsigned int tolerance_field = held_output_field(program, tolerance, 0u);
    const unsigned int den_field = held_output_field(program, tolerance, 1u);
    const unsigned int held_side = held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, tolerance_field),
                                             held_power(program, (unsigned int)common_shift), 0u);
    const unsigned int sum = held_step(program, ENGINE_RECORD_PRODUCT,
                                       held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, sum_field),
                                                 held_read_before(program, den_field), 0u),
                                       held_power(program, (unsigned int)sum_shift), 0u);
    if (side == 0u)
    {
        held_output(stage, "above", held_above(program, sum, held_side), 0);
        return;
    }
    const unsigned int above_field = held_output_field(program, above, 0u);
    const unsigned int below = held_above(program, held_side, sum);
    held_output(stage, "sign", held_step(program, ENGINE_RECORD_DIFFERENCE, held_read_also(program, above_field), below, 0u), 0);
}

// a sign change between neighboring points: 1 where the verdicts at points l and l + 1, members 1 and 2, are of
// opposite signs, and 0 where either is 0 or they agree
static void held_change_build(HeldStage *stage, const HeldStage *verdict)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    held_field(program, 1u);
    const unsigned int sign_field = held_output_field(program, verdict, 0u);
    const unsigned int product = held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, sign_field),
                                           held_read_also(program, sign_field), 0u);
    held_output(stage, "change", held_above(program, held_constant(program, 0ull), product), 0);
}

// the clock at the point: theta / pi read at 2^-W below and above, the quotients of theta's numerators by
// Qa U^28 2^(4b + 3) less 1 and more 1, from theta's records below and above as members 1 and 2; and U^4 at 4^-2b
static void held_clock_build(HeldStage *stage, unsigned int b, unsigned int first_bits, const HeldStage *lower,
                             const HeldStage *upper)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int first_field = held_field(program, first_bits);
    const unsigned int low_field = held_output_field(program, lower, 0u);
    const unsigned int den_field = held_output_field(program, lower, 1u);
    const unsigned int high_field = held_output_field(program, upper, 0u);
    const unsigned int unit = held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, den_field),
                                        held_power(program, 4u * b + 3u), 0u);
    const unsigned int one = held_constant(program, 1ull);
    const long long w = -lower->exponents[0] - 4ll * (long long)b - 3ll;
    held_output(stage, "low",
                held_step(program, ENGINE_RECORD_DIFFERENCE,
                          held_step(program, ENGINE_RECORD_QUOTIENT, held_read_before(program, low_field), unit, 0u), one, 0u),
                -w);
    held_output(stage, "high",
                held_step(program, ENGINE_RECORD_SUM,
                          held_step(program, ENGINE_RECORD_QUOTIENT, held_read_also(program, high_field), unit, 0u), one, 0u),
                -w);
    const unsigned int big_u = held_step(program, ENGINE_RECORD_SUM, held_lane(program, b), held_read(program, first_field), 0u);
    const unsigned int square = held_step(program, ENGINE_RECORD_PRODUCT, big_u, big_u, 0u);
    held_output(stage, "fourth", held_step(program, ENGINE_RECORD_PRODUCT, square, square, 0u), -4ll * (long long)b);
}

// Turing's sums over the cell, one lane a step i from point i to i + 1, the change at the step as member 0 and the
// clock at points i and i + 1 as members 1 and 2, d_i = x^2 at i + 1 less x^2 at i, x^2 = U^4 / 16^b. Below point a,
// the window under N at point a: d_i Theta_i below, and at a change, U_i^4 and 1. From point e on, the window over N
// at point e: d_i Theta_(i + 1) above, and at a change, U_(i + 1)^4 and 1. Between them, the changes alone
static void held_turing_build(HeldStage *stage, unsigned int lane_bits, unsigned long long a, unsigned long long e,
                              const HeldStage *change, const HeldStage *clock)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int change_field = held_output_field(program, change, 0u);
    program->shared_bits = change->layout.out_limbs * 32u;
    const unsigned int low_field = held_output_field(program, clock, 0u);
    const unsigned int high_field = held_output_field(program, clock, 1u);
    const unsigned int fourth_field = held_output_field(program, clock, 2u);
    const unsigned int lane = held_lane_below(program, lane_bits);
    const unsigned int below = held_above(program, held_constant(program, a), lane);
    const unsigned int past = held_above(program, lane, held_constant(program, e - 1ull));
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int between = held_step(program, ENGINE_RECORD_DIFFERENCE,
                                           held_step(program, ENGINE_RECORD_DIFFERENCE, one, below, 0u), past, 0u);
    const unsigned int changed = held_read(program, change_field);
    const unsigned int here = held_read_before(program, fourth_field);
    const unsigned int next = held_read_also(program, fourth_field);
    const unsigned int step = held_step(program, ENGINE_RECORD_DIFFERENCE, next, here, 0u);
    const long long w = -clock->exponents[0];
    const long long x = clock->exponents[2];
    const unsigned int changed_below = held_step(program, ENGINE_RECORD_PRODUCT, below, changed, 0u);
    const unsigned int changed_past = held_step(program, ENGINE_RECORD_PRODUCT, past, changed, 0u);
    held_output(stage, "below_theta",
                held_step(program, ENGINE_RECORD_PRODUCT, below,
                          held_step(program, ENGINE_RECORD_PRODUCT, step, held_read_before(program, low_field), 0u), 0u),
                x - w);
    held_output(stage, "below_zeros", held_step(program, ENGINE_RECORD_PRODUCT, changed_below, here, 0u), x);
    held_output(stage, "below_count", changed_below, 0);
    held_output(stage, "past_theta",
                held_step(program, ENGINE_RECORD_PRODUCT, past,
                          held_step(program, ENGINE_RECORD_PRODUCT, step, held_read_also(program, high_field), 0u), 0u),
                x - w);
    held_output(stage, "past_zeros", held_step(program, ENGINE_RECORD_PRODUCT, changed_past, next, 0u), x);
    held_output(stage, "past_count", changed_past, 0);
    held_output(stage, "between_count", held_step(program, ENGINE_RECORD_PRODUCT, between, changed, 0u), 0);
}

static int held_word(FILE *in, long long *value)
{
    return fread(value, sizeof(long long), 1u, in) == 1u;
}

// a mantissa of the input: its magnitude's limbs, its sign, and the signed width that holds it
static int held_mantissa(FILE *in, std::vector<unsigned int> *magnitude, long long *sign, unsigned int *bits)
{
    long long limbs = 0;
    int read = held_word(in, &limbs) && (limbs >= 0) && (limbs < (1 << 20));
    magnitude->assign(read ? (size_t)limbs : 0u, 0u);
    read = read && (fread(magnitude->data(), sizeof(unsigned int), magnitude->size(), in) == magnitude->size());
    read = read && held_word(in, sign);
    unsigned int width = 32u * (unsigned int)magnitude->size();
    while ((width > 0u) && ((((*magnitude)[(width - 1u) / 32u] >> ((width - 1u) % 32u)) & 1u) == 0u))
    {
        width -= 1u;
    }
    *bits = width + 1u;
    return read;
}

// a magnitude's limbs moved `shift` bits up, zeros below
static std::vector<unsigned int> held_shifted(const std::vector<unsigned int> &limbs, unsigned int shift)
{
    std::vector<unsigned int> out(limbs.size() + shift / 32u + 1u, 0u);
    for (size_t at = 0u; at < limbs.size(); at += 1u)
    {
        const unsigned long long moved = (unsigned long long)limbs[at] << (shift % 32u);
        out[at + shift / 32u] |= (unsigned int)moved;
        out[at + shift / 32u + 1u] |= (unsigned int)(moved >> 32u);
    }
    return out;
}

// `bits` of a two's complement value, its magnitude's limbs given, laid into the record at `offset`
static void held_put(unsigned int *record, unsigned int offset, unsigned int bits, const std::vector<unsigned int> &limbs,
                      long long sign)
{
    std::vector<unsigned int> word((bits + 31u) / 32u + 1u, 0u);
    for (size_t at = 0u; at < limbs.size() && at < word.size(); at += 1u)
    {
        word[at] = limbs[at];
    }
    if (sign < 0)
    {
        unsigned long long carry = 1ull;
        for (size_t at = 0u; at < word.size(); at += 1u)
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
}

// the record a stage's fields are laid into, sized to them
static void held_open_shared(HeldStage *stage)
{
    stage->shared.assign((stage->program.shared_bits + 31u) / 32u + 1u, 0u);
}

// the poles' records, one a body, laid as held_pole_fields lays them in the stage's program, which is laid out
static void held_pole_records(HeldStage *stage, const std::vector<unsigned int> &first, const std::vector<unsigned int> &bound,
                              const std::vector<std::vector<unsigned int>> &magnitudes, const std::vector<long long> &signs)
{
    const HeldProgram *const program = &stage->program;
    const unsigned int limbs = stage->shared_limbs;
    const size_t poles = magnitudes.size() / 4u;
    stage->shared.assign(poles * limbs, 0u);
    for (size_t n = 0u; n < poles; n += 1u)
    {
        std::vector<unsigned int> record(limbs + 1u, 0u);
        held_put(record.data(), program->field_offset[HELD_POLE_FIRST], program->field_bits[HELD_POLE_FIRST], first, 1);
        for (unsigned int k = 0u; k < 4u; k += 1u)
        {
            held_put(record.data(), program->field_offset[HELD_POLE_LN_LOW + k], program->field_bits[HELD_POLE_LN_LOW + k],
                     magnitudes[4u * n + k], signs[4u * n + k]);
        }
        held_put(record.data(), program->field_offset[HELD_POLE_BOUND], program->field_bits[HELD_POLE_BOUND], bound, 1);
        memcpy(&stage->shared[n * limbs], record.data(), limbs * sizeof(unsigned int));
    }
}

static void held_open_stage(HeldStage *stage, const char *name)
{
    stage->name = name;
    memset(&stage->key, 0, sizeof(stage->key));
    memset(&stage->layout, 0, sizeof(stage->layout));
    stage->record = NULL;
    stage->same = 0;
    stage->shared_limbs = 0u;
    stage->before = NULL;
    stage->before_limbs = 0u;
    stage->also = NULL;
    stage->also_limbs = 0u;
    stage->group = 0ull;
    stage->sums_same = 1;
}

// the members a stage's program reads
static unsigned int held_members(const HeldStage *stage)
{
    return (stage->also != NULL) ? 3u : (stage->before != NULL) ? 2u : 1u;
}

// imprints and lays out a stage's program, and says where the imprint stops when it does
static int held_lay(SimResults *job, HeldStage *stage, EngineError *error)
{
    HeldProgram *const program = &stage->program;
    const unsigned int fields = (unsigned int)program->field_bits.size();
    const KeymathRecordRequest encode = {program->steps.data(), (unsigned int)program->steps.size(),
                                         program->field_bits.data(),
                                         fields,
                                         held_members(stage),
                                         stage->outputs.data(),
                                         (unsigned int)stage->outputs.size(),
                                         NULL,
                                         0u,
                                         &stage->key,
                                         error};
    int ok = keymath_record_encode(&encode) != KEYMATH_ERROR;
    if (!ok)
    {
        const EngineRecordStep *const first = program->steps.data();
        const EngineRecordStep *const last = first + program->steps.size();
        const EngineRecordStep *const at = (const EngineRecordStep *)error->evacaddr;
        const long long step = ((at >= first) && (at < last)) ? (long long)(at - first) : -1;
        fprintf(stderr,
                "  exact_zeta_held: the %s imprint errors, site %u, at step %lld of %zu, operation %d left %u right %u\n",
                stage->name, error->site, step, program->steps.size(), (step >= 0) ? (int)at->operation : -1,
                (step >= 0) ? at->left : 0u, (step >= 0) ? at->right : 0u);
    }
    sim_check(job, ok, (std::string("keymath imprints the ") + stage->name + " program").c_str());
    stage->shared_limbs = (program->shared_bits + 31u) / 32u;
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {stage->shared_limbs, stage->before_limbs, stage->also_limbs};
    const KeyScheduleRecordRequest lay = {&stage->key, program->field_offset.data(), fields, in_limbs, 1, &stage->layout,
                                          error};
    const int laid = ok && (key_schedule_record_layout(&lay) != KEY_SCHEDULE_ERROR);
    if (ok && !laid)
    {
        const char *const end = (error->evacaddr == (const void *)&stage->key)       ? "the register file"
                                : (error->evacaddr == (const void *)stage->key.output) ? "the outputs"
                                                                                        : "a step";
        const long long step = (end[0] == 'a') ? (long long)((const EngineRecordTerm *)error->evacaddr - stage->key.term)
                                               : -1;
        fprintf(stderr, "  exact_zeta_held: the %s layout ends at %s, step %lld of %zu\n", stage->name, end, step,
                program->steps.size());
        // the limbs live at each step, a register live from its step to its last reader and an output to the end
        const unsigned int steps = stage->key.steps;
        std::vector<unsigned int> last(steps, 0u);
        for (unsigned int at = 0u; at < steps; at += 1u)
        {
            last[at] = at;
            const EngineRecordTerm *const term = &stage->key.term[at];
            const int reads = (term->operation != ENGINE_RECORD_FIELD) && (term->operation != ENGINE_RECORD_FIELD_SIGNED) &&
                              (term->operation != ENGINE_RECORD_CONSTANT) && (term->operation != ENGINE_RECORD_LANE);
            if (reads)
            {
                last[term->left] = at;
                last[term->right] = (term->operation == ENGINE_RECORD_ABSOLUTE) ? last[term->right] : at;
            }
        }
        for (unsigned int at = 0u; at < stage->key.outputs; at += 1u)
        {
            last[stage->key.output[at]] = steps;
        }
        unsigned long long peak = 0ull;
        unsigned int peak_at = 0u;
        for (unsigned int at = 0u; at < steps; at += 1u)
        {
            unsigned long long live = 0ull;
            for (unsigned int from = 0u; from <= at; from += 1u)
            {
                live += (last[from] >= at) ? (stage->key.term[from].bits + 31u) / 32u : 0u;
            }
            peak_at = (live > peak) ? at : peak_at;
            peak = (live > peak) ? live : peak;
        }
        fprintf(stderr, "  exact_zeta_held: %llu limbs live at step %u, the widest:", peak, peak_at);
        for (unsigned int from = 0u; from <= peak_at; from += 1u)
        {
            if ((last[from] >= peak_at) && (stage->key.term[from].bits > 256u))
            {
                fprintf(stderr, " step %u op %d %u bits;", from, (int)stage->key.term[from].operation,
                        stage->key.term[from].bits);
            }
        }
        fprintf(stderr, "\n");
    }
    ok = laid;
    sim_check(job, ok, (std::string("the scheduler lays the ") + stage->name + " program out").c_str());
    if (stage->shared.size() <= stage->shared_limbs + 1u)
    {
        stage->shared.resize(stage->shared_limbs);
    }
    return ok;
}

// the bytes a stage holds on the device over `lanes` lanes
static unsigned long long held_declared(const HeldStage *stage, unsigned long long lanes)
{
    const unsigned long long members = (stage->before != NULL ? stage->before->size() : 0u) +
                                       (stage->also != NULL ? stage->also->size() : 0u) + stage->index.size();
    return lanes * stage->layout.out_limbs * sizeof(unsigned int) + (stage->shared.size() + members) * sizeof(unsigned int) +
           (unsigned long long)stage->layout.steps * sizeof(DeviceRecordStep);
}

// a device copy of `values`, or NULL where there are none or the copy fails
static unsigned int *held_upload(const std::vector<unsigned int> *values, int *ok)
{
    unsigned int *out = NULL;
    if ((values == NULL) || values->empty() || !*ok)
    {
        return NULL;
    }
    *ok = (cudaMalloc((void **)&out, values->size() * sizeof(unsigned int)) == cudaSuccess) &&
          (cudaMemcpy(out, values->data(), values->size() * sizeof(unsigned int), cudaMemcpyHostToDevice) == cudaSuccess);
    return out;
}

// the limbs a sum of output `at` over a run of `group` lanes takes: the output's bits and the group's beside them
static unsigned int held_sum_limbs(const HeldStage *stage, unsigned int at)
{
    unsigned int group_bits = 0u;
    while ((stage->group >> group_bits) != 0ull)
    {
        group_bits += 1u;
    }
    return (stage->layout.step_table[stage->outputs[at]].out_bits + group_bits + 31u) / 32u + 1u;
}

// loads a stage onto the device, runs every lane, and holds the host's first `checked` records against the device's
static int held_sweep(SimResults *job, HeldStage *stage, unsigned long long lanes, unsigned long long checked,
                       EngineError *error)
{
    const std::string name(stage->name);
    int ok = cycle_record_load(&stage->layout, &stage->record, error) != CYCLE_ERROR;
    sim_check(job, ok, ("the " + name + " program loads onto the device").c_str());
    const unsigned int out_limbs = stage->layout.out_limbs;
    const std::vector<unsigned int> *const in[3] = {&stage->shared, stage->before, stage->also};
    const unsigned int in_limbs[3] = {stage->shared_limbs, stage->before_limbs, stage->also_limbs};
    unsigned int *device_in[3] = {NULL, NULL, NULL};
    unsigned long long bodies[3] = {0ull, 0ull, 0ull};
    for (unsigned int member = 0u; member < 3u; member += 1u)
    {
        device_in[member] = held_upload(in[member], &ok);
        bodies[member] = ((in[member] != NULL) && (in_limbs[member] > 0u)) ? in[member]->size() / in_limbs[member] : 0ull;
    }
    unsigned int *const device_index = held_upload(&stage->index, &ok);
    unsigned int *device_out = NULL;
    ok = ok && (cudaMalloc((void **)&device_out, lanes * out_limbs * sizeof(unsigned int)) == cudaSuccess);
    const CycleRecordRunRequest run = {stage->record, {device_in[0], device_in[1], device_in[2]},
                                       {bodies[0], bodies[1], bodies[2]}, device_index, lanes, device_out, error};
    ok = ok && (cycle_record_run(&run) == (long)lanes);
    sim_check(job, ok, ("every lane runs the " + name + " program on the device").c_str());
    stage->records.assign((size_t)(ok ? lanes * out_limbs : 0ull), 0u);
    ok = ok && (cudaMemcpy(stage->records.data(), device_out, stage->records.size() * sizeof(unsigned int),
                           cudaMemcpyDeviceToHost) == cudaSuccess);
    std::vector<unsigned int> host_out((size_t)(checked * out_limbs));
    const CycleRecordHostRequest host = {&stage->layout,
                                         {in[0]->data(), (in[1] != NULL) ? in[1]->data() : NULL, (in[2] != NULL) ? in[2]->data() : NULL},
                                         {bodies[0], bodies[1], bodies[2]},
                                         stage->index.empty() ? NULL : stage->index.data(),
                                         checked,
                                         host_out.data(),
                                         error,
                                         0ull};
    stage->same = ok && (cycle_record_run_host(&host) == (long)checked) &&
                  (memcmp(host_out.data(), stage->records.data(), host_out.size() * sizeof(unsigned int)) == 0);
    sim_check(job, stage->same, ("the host's " + name + " records equal the device's word for word").c_str());
    // each summed output over each run of `group` lanes, on the device, and the same sums over the host's copy
    stage->sums.assign(stage->summed.size(), std::vector<unsigned int>());
    stage->sum_limbs.assign(stage->summed.size(), 0u);
    for (size_t at = 0u; ok && (at < stage->summed.size()); at += 1u)
    {
        const DeviceRecordStep *const place = &stage->layout.step_table[stage->outputs[stage->summed[at]]];
        stage->sum_limbs[at] = held_sum_limbs(stage, stage->summed[at]);
        const size_t size = (size_t)((lanes / stage->group) * stage->sum_limbs[at]);
        stage->sums[at].assign(size, 0u);
        std::vector<unsigned int> host_sums(size, 0u);
        const CycleRecordSumRequest device_sum = {device_out, lanes, stage->group, out_limbs, place->out_offset,
                                                  place->out_bits, stage->sum_limbs[at], stage->sums[at].data(), error};
        const CycleRecordSumRequest host_sum = {stage->records.data(), lanes, stage->group, out_limbs, place->out_offset,
                                                place->out_bits, stage->sum_limbs[at], host_sums.data(), error};
        ok = (cycle_record_sum(&device_sum) != CYCLE_ERROR) && (cycle_record_sum_host(&host_sum) != CYCLE_ERROR);
        stage->sums_same = stage->sums_same && ok && (host_sums == stage->sums[at]);
    }
    if (!stage->summed.empty())
    {
        sim_check(job, ok && stage->sums_same,
                  ("the device's sums of the " + name + " records over each point equal the host's").c_str());
    }
    for (unsigned int member = 0u; member < 3u; member += 1u)
    {
        cudaFree(device_in[member]);
    }
    cudaFree(device_index);
    cudaFree(device_out);
    return ok;
}

static void held_write(FILE *out, const HeldStage *stage, unsigned long long lanes)
{
    const unsigned int out_limbs = stage->layout.out_limbs;
    fprintf(out, "stage %s out_limbs %u\n", stage->name, out_limbs);
    for (size_t at = 0u; at < stage->outputs.size(); at += 1u)
    {
        const DeviceRecordStep *const step = &stage->layout.step_table[stage->outputs[at]];
        fprintf(out, "output %s %u %u %lld\n", stage->names[at].c_str(), step->out_offset, step->out_bits,
                stage->exponents[at]);
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

// a stage's sums alone: the line "sum stage output exponent L runs R host H", then a line a run of L hex limbs of
// two's complement, least significant first
static void held_write_sums(FILE *out, const HeldStage *stage)
{
    for (size_t at = 0u; at < stage->summed.size(); at += 1u)
    {
        const unsigned int limbs = stage->sum_limbs[at];
        const size_t runs = stage->sums[at].size() / limbs;
        fprintf(out, "sum %s %s %lld %u runs %zu host %d\n", stage->name, stage->names[stage->summed[at]].c_str(),
                stage->exponents[stage->summed[at]], limbs, runs, stage->sums_same);
        for (size_t run = 0u; run < runs; run += 1u)
        {
            for (unsigned int limb = 0u; limb < limbs; limb += 1u)
            {
                fprintf(out, (limb == 0u) ? "%x" : " %x", stage->sums[at][run * limbs + limb]);
            }
            fprintf(out, "\n");
        }
    }
}

static void held_release(HeldStage *stage)
{
    if (stage->record != NULL)
    {
        cycle_record_release(stage->record);
    }
    key_schedule_record_release(&stage->layout);
    keymath_record_release(&stage->key);
}

int main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if (count != 3)
    {
        fprintf(stderr, "  usage: exact_zeta_held <input> <output>\n");
        return 2;
    }
    FILE *in = fopen(arguments[1], "rb");
    long long header[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    int read = (in != NULL);
    for (unsigned int at = 0u; read && (at < 9u); at += 1u)
    {
        read = held_word(in, &header[at]);
    }
    // the input is read for its form alone: the lane is read at b + 1 bits, a pair's lane below 2^62, and a count is at
    // least what it counts
    read = read && (header[0] >= 8) && (header[1] > 0) && (header[1] <= header[0]) && (header[2] > 0) && (header[3] > 0) &&
           (header[3] < 63) && (header[0] <= (2ll << header[3])) && (header[2] < (1ll << 30)) &&
           (header[0] < (1ll << 32)) && (header[4] >= 0) && (header[5] >= 0) && (header[6] >= 2) && (header[7] > 1) &&
           (header[8] > 0);
    const unsigned long long lanes = read ? (unsigned long long)header[0] : 0ull;
    const unsigned long long checked = read ? (unsigned long long)header[1] : 0ull;
    const unsigned long long nu = read ? (unsigned long long)header[2] : 0ull;
    const unsigned int b = read ? (unsigned int)header[3] : 0u;
    const unsigned int curves = read ? (unsigned int)header[4] + 1u : 0u;
    const long long big_e = read ? header[5] : 0;
    const unsigned int log_terms = read ? (unsigned int)header[6] : 0u;
    const unsigned int w = read ? (unsigned int)header[7] : 0u;
    const unsigned int cos_terms = read ? (unsigned int)header[8] : 0u;
    std::vector<unsigned int> first;
    long long first_sign = 0;
    unsigned int first_bits = 0u;
    read = read && held_mantissa(in, &first, &first_sign, &first_bits) && (first_sign > 0);
    // the field of U_0 holds its value with room to spare
    first_bits += 2u;
    std::vector<long long> terms(curves, 0);
    for (unsigned int n = 0u; read && (n < curves); n += 1u)
    {
        read = held_word(in, &terms[n]) && (terms[n] > 0);
    }
    std::vector<std::vector<std::vector<unsigned int>>> magnitudes(curves);
    std::vector<std::vector<long long>> signs(curves);
    std::vector<std::vector<unsigned int>> gamma_bits(curves);
    for (unsigned int n = 0u; read && (n < curves); n += 1u)
    {
        for (long long j = 0; read && (j < terms[n]); j += 1)
        {
            std::vector<unsigned int> magnitude;
            long long sign = 0;
            unsigned int bits = 0u;
            read = held_mantissa(in, &magnitude, &sign, &bits);
            magnitudes[n].push_back(magnitude);
            signs[n].push_back(sign);
            gamma_bits[n].push_back(bits);
        }
    }
    std::vector<std::vector<unsigned int>> c_magnitudes(log_terms);
    std::vector<long long> c_signs(log_terms, 0);
    std::vector<unsigned int> c_bits(log_terms, 0u);
    for (unsigned int k = 0u; read && (k < log_terms); k += 1u)
    {
        read = held_mantissa(in, &c_magnitudes[k], &c_signs[k], &c_bits[k]);
    }
    // theta's held constants at 2^-W, then Lambda and 2L + 1
    std::vector<std::vector<unsigned int>> theta_magnitudes(11u);
    std::vector<long long> theta_signs(11u, 0);
    std::vector<unsigned int> theta_bits(11u, 0u);
    for (unsigned int k = 0u; read && (k < 11u); k += 1u)
    {
        read = held_mantissa(in, &theta_magnitudes[k], &theta_signs[k], &theta_bits[k]) && (theta_signs[k] > 0);
    }
    // B, then k_j below for j < J, then each pole's ln n below and above and n^(-1/2) below and above
    std::vector<unsigned int> bound;
    long long bound_sign = 0;
    unsigned int bound_bits = 0u;
    read = read && held_mantissa(in, &bound, &bound_sign, &bound_bits) && (bound_sign > 0);
    std::vector<std::vector<unsigned int>> k_magnitudes(cos_terms);
    std::vector<long long> k_signs(cos_terms, 0);
    std::vector<unsigned int> k_widths(cos_terms, 0u);
    for (unsigned int k = 0u; read && (k < cos_terms); k += 1u)
    {
        read = held_mantissa(in, &k_magnitudes[k], &k_signs[k], &k_widths[k]) && (k_signs[k] > 0);
    }
    const size_t pole_count = read ? (size_t)nu : 0u;
    std::vector<std::vector<unsigned int>> pole_magnitudes(4u * pole_count);
    std::vector<long long> pole_signs(4u * pole_count, 0);
    std::vector<unsigned int> pole_widths(HELD_POLE_FIELDS, 0u);
    pole_widths[HELD_POLE_FIRST] = first_bits;
    pole_widths[HELD_POLE_BOUND] = bound_bits;
    for (size_t at = 0u; read && (at < 4u * pole_count); at += 1u)
    {
        unsigned int bits = 0u;
        read = held_mantissa(in, &pole_magnitudes[at], &pole_signs[at], &bits) && (pole_signs[at] >= 0);
        const unsigned int field = HELD_POLE_LN_LOW + (unsigned int)(at % 4u);
        pole_widths[field] = (bits > pole_widths[field]) ? bits : pole_widths[field];
    }
    // the verdict's bound B above at 2^-W
    std::vector<unsigned int> verdict_bound;
    long long verdict_sign = 0;
    unsigned int verdict_bits = 0u;
    read = read && held_mantissa(in, &verdict_bound, &verdict_sign, &verdict_bits) && (verdict_sign > 0);
    if (in != NULL)
    {
        fclose(in);
    }
    if (!read)
    {
        fprintf(stderr, "  exact_zeta_held: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    EngineError error;
    memset(&error, 0, sizeof(error));
    const long long least = held_least_exponent(big_e, b, gamma_bits);
    std::vector<HeldStage> curve_stages(curves);
    std::vector<std::string> curve_names(curves);
    std::vector<unsigned int> one_limb(1u, 1u);
    int ok = 1;
    for (unsigned int n = 0u; ok && (n < curves); n += 1u)
    {
        HeldStage *const stage = &curve_stages[n];
        curve_names[n] = (n + 1u == curves) ? std::string("curves") : "curve" + std::to_string(n);
        held_open_stage(stage, curve_names[n].c_str());
        const HeldStage *const before = (n > 0u) ? &curve_stages[n - 1u] : NULL;
        const unsigned int lift = (unsigned int)(held_curve_exponent(big_e, b, n, gamma_bits[n].size()) - least);
        held_build(stage, b, n, curves - 1u, least, first_bits, gamma_bits[n], lift, before);
        stage->before = (before != NULL) ? &before->records : NULL;
        stage->before_limbs = (before != NULL) ? before->layout.out_limbs : 0u;
        held_open_shared(stage);
        held_put(stage->shared.data(), stage->program.field_offset[0], stage->program.field_bits[0], one_limb,
                 ((nu - 1ull) % 2ull) ? -1 : 1);
        held_put(stage->shared.data(), stage->program.field_offset[1], stage->program.field_bits[1], first, 1);
        const unsigned int held = (unsigned int)magnitudes[n].size();
        for (unsigned int j = 0u; j < held; j += 1u)
        {
            const std::vector<unsigned int> laid = held_shifted(magnitudes[n][j], 2u * b * (held - 1u - j) + lift);
            held_put(stage->shared.data(), stage->program.field_offset[2u + j], stage->program.field_bits[2u + j], laid,
                     signs[n][j]);
        }
        ok = held_lay(&job, stage, &error);
    }

    HeldStage log_stage;
    held_open_stage(&log_stage, "log");
    held_log_build(&log_stage, b, first_bits, c_bits);
    held_open_shared(&log_stage);
    held_put(log_stage.shared.data(), log_stage.program.field_offset[0], log_stage.program.field_bits[0], first, 1);
    for (unsigned int k = 0u; k < log_terms; k += 1u)
    {
        held_put(log_stage.shared.data(), log_stage.program.field_offset[1u + k], log_stage.program.field_bits[1u + k],
                 c_magnitudes[k], c_signs[k]);
    }

    ok = ok && held_lay(&job, &log_stage, &error);
    HeldStage theta_stages[2];
    const char *const theta_names[2] = {"theta_lower", "theta_upper"};
    unsigned long long declared = held_declared(&log_stage, lanes) + (256ull << 20u);
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        HeldStage *const stage = &theta_stages[side];
        held_open_stage(stage, theta_names[side]);
        if (!ok)
        {
            continue;
        }
        held_theta_build(stage, side, b, w, first_bits, theta_bits, &log_stage);
        stage->before = &log_stage.records;
        stage->before_limbs = log_stage.layout.out_limbs;
        held_open_shared(stage);
        held_put(stage->shared.data(), stage->program.field_offset[0], stage->program.field_bits[0], first, 1);
        for (unsigned int k = 0u; k < 11u; k += 1u)
        {
            held_put(stage->shared.data(), stage->program.field_offset[1u + k], stage->program.field_bits[1u + k],
                     theta_magnitudes[k], theta_signs[k]);
        }
        ok = held_lay(&job, stage, &error);
        declared += ok ? held_declared(stage, lanes) : 0ull;
    }
    for (unsigned int n = 0u; ok && (n < curves); n += 1u)
    {
        declared += held_declared(&curve_stages[n], lanes);
    }

    // the pairs: lane point nu + n - 1 over every point of the cell and every n to nu, each pole's record a body
    const unsigned long long pairs = lanes * nu;
    unsigned int lane_bits = 1u;
    while ((pairs >> lane_bits) != 0ull)
    {
        lane_bits += 1u;
    }
    std::vector<unsigned int> pair_index((size_t)(3ull * pairs), 0u);
    for (unsigned long long lane = 0ull; lane < pairs; lane += 1ull)
    {
        pair_index[(size_t)(3ull * lane)] = (unsigned int)(lane % nu);
        pair_index[(size_t)(3ull * lane + 1ull)] = (unsigned int)(lane / nu);
        pair_index[(size_t)(3ull * lane + 2ull)] = (unsigned int)(lane / nu);
    }
    HeldStage phase_stage;
    held_open_stage(&phase_stage, "phase");
    if (ok)
    {
        held_phase_build(&phase_stage, b, w, nu, lane_bits, pole_widths, &theta_stages[0], &theta_stages[1]);
        phase_stage.before = &theta_stages[0].records;
        phase_stage.before_limbs = theta_stages[0].layout.out_limbs;
        phase_stage.also = &theta_stages[1].records;
        phase_stage.also_limbs = theta_stages[1].layout.out_limbs;
        ok = held_lay(&job, &phase_stage, &error);
        held_pole_records(&phase_stage, first, bound, pole_magnitudes, pole_signs);
        phase_stage.index = pair_index;
        declared += held_declared(&phase_stage, pairs) +
                    lanes * (phase_stage.before_limbs + phase_stage.also_limbs) * sizeof(unsigned int);
    }
    HeldStage cos_stage;
    held_open_stage(&cos_stage, "cos");
    if (ok)
    {
        std::vector<unsigned int> k_bits(cos_terms);
        for (unsigned int j = 0u; j < cos_terms; j += 1u)
        {
            k_bits[j] = k_widths[j] + 2u * w * (cos_terms - 1u - j);
        }
        held_cos_build(&cos_stage, k_bits, &phase_stage);
        cos_stage.before = &phase_stage.records;
        cos_stage.before_limbs = phase_stage.layout.out_limbs;
        held_open_shared(&cos_stage);
        for (unsigned int j = 0u; j < cos_terms; j += 1u)
        {
            held_put(cos_stage.shared.data(), cos_stage.program.field_offset[j], cos_stage.program.field_bits[j],
                     held_shifted(k_magnitudes[j], 2u * w * (cos_terms - 1u - j)), (j % 2u == 0u) ? 1 : -1);
        }
        ok = held_lay(&job, &cos_stage, &error);
        declared += ok ? held_declared(&cos_stage, pairs) + pairs * cos_stage.before_limbs * sizeof(unsigned int) : 0ull;
    }
    HeldStage term_stages[2];
    const char *const term_names[2] = {"term_lower", "term_upper"};
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        HeldStage *const stage = &term_stages[side];
        held_open_stage(stage, term_names[side]);
        if (!ok)
        {
            continue;
        }
        held_term_build(stage, side, w, pole_widths, &cos_stage);
        stage->before = &cos_stage.records;
        stage->before_limbs = cos_stage.layout.out_limbs;
        ok = held_lay(&job, stage, &error);
        held_pole_records(stage, first, bound, pole_magnitudes, pole_signs);
        stage->index.assign((size_t)(2ull * pairs), 0u);
        for (size_t lane = 0u; lane < (size_t)pairs; lane += 1u)
        {
            stage->index[2u * lane] = (unsigned int)(lane % nu);
            stage->index[2u * lane + 1u] = (unsigned int)lane;
        }
        stage->group = nu;
        stage->summed = {0u};
        declared += held_declared(stage, pairs);
    }

    // the verdict at every point, its sums record laid by the host from the term sums, each sign-extended to the wider
    unsigned int sum_limbs = 0u;
    unsigned int sum_bits = 0u;
    unsigned int nu_bits = 0u;
    while ((nu >> nu_bits) != 0ull)
    {
        nu_bits += 1u;
    }
    for (unsigned int side = 0u; ok && (side < 2u); side += 1u)
    {
        const unsigned int limbs = held_sum_limbs(&term_stages[side], 0u);
        sum_limbs = (limbs > sum_limbs) ? limbs : sum_limbs;
        const unsigned int bits = term_stages[side].layout.step_table[term_stages[side].outputs[0]].out_bits + nu_bits;
        sum_bits = (bits > sum_bits) ? bits : sum_bits;
    }
    // e the least of the doubled sums' exponent, the remainder's and -W; c the lesser of the remainder's and the bound's
    // shifts to e
    const long long sum_exponent = ok ? term_stages[0].exponents[0] + 1 : 0;
    const long long least_exponent = ok ? curve_stages[curves - 1u].exponents[0] : 0;
    long long lowest = (least_exponent < sum_exponent) ? least_exponent : sum_exponent;
    lowest = (-(long long)w < lowest) ? -(long long)w : lowest;
    const long long least_to = least_exponent - lowest;
    const long long bound_to = -(long long)w - lowest;
    const long long common_shift = (least_to < bound_to) ? least_to : bound_to;
    HeldStage tolerance_stages[2];
    const char *const tolerance_names[2] = {"tolerance_lower", "tolerance_upper"};
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        HeldStage *const stage = &tolerance_stages[side];
        held_open_stage(stage, tolerance_names[side]);
        if (!ok)
        {
            continue;
        }
        held_tolerance_build(stage, side, verdict_bits, &curve_stages[curves - 1u], least_to - common_shift,
                             bound_to - common_shift);
        stage->before = &curve_stages[curves - 1u].records;
        stage->before_limbs = curve_stages[curves - 1u].layout.out_limbs;
        held_open_shared(stage);
        held_put(stage->shared.data(), stage->program.field_offset[0], stage->program.field_bits[0], verdict_bound, 1);
        ok = held_lay(&job, stage, &error);
        declared += held_declared(stage, lanes);
    }
    std::vector<unsigned int> sum_records;
    HeldStage verdict_stages[2];
    const char *const verdict_names[2] = {"above", "verdict"};
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        HeldStage *const stage = &verdict_stages[side];
        held_open_stage(stage, verdict_names[side]);
        if (!ok)
        {
            continue;
        }
        held_verdict_build(stage, side, sum_bits, 32u * sum_limbs, sum_exponent - lowest, common_shift,
                           &tolerance_stages[side], &verdict_stages[0]);
        stage->before = &tolerance_stages[side].records;
        stage->before_limbs = tolerance_stages[side].layout.out_limbs;
        stage->also = (side == 1u) ? &verdict_stages[0].records : NULL;
        stage->also_limbs = (side == 1u) ? verdict_stages[0].layout.out_limbs : 0u;
        ok = held_lay(&job, stage, &error);
        declared += held_declared(stage, lanes) + lanes * 2u * sum_limbs * sizeof(unsigned int);
    }
    HeldStage &verdict_stage = verdict_stages[1];
    // the sign changes between neighboring points, counted by one sum over the cell
    HeldStage change_stage;
    held_open_stage(&change_stage, "change");
    const unsigned long long steps_between = lanes - 1ull;
    if (ok && (steps_between > 0ull))
    {
        held_change_build(&change_stage, &verdict_stage);
        change_stage.before = &verdict_stage.records;
        change_stage.before_limbs = verdict_stage.layout.out_limbs;
        change_stage.also = &verdict_stage.records;
        change_stage.also_limbs = verdict_stage.layout.out_limbs;
        held_open_shared(&change_stage);
        ok = held_lay(&job, &change_stage, &error);
        change_stage.index.assign((size_t)(3ull * steps_between), 0u);
        for (unsigned long long lane = 0ull; lane < steps_between; lane += 1ull)
        {
            change_stage.index[(size_t)(3ull * lane + 1ull)] = (unsigned int)lane;
            change_stage.index[(size_t)(3ull * lane + 2ull)] = (unsigned int)(lane + 1ull);
        }
        change_stage.group = steps_between;
        change_stage.summed = {0u};
        declared += held_declared(&change_stage, steps_between);
    }
    // the clock at every point, and Turing's sums over the windows below point a = P / 4 and past point e = 3P / 4
    HeldStage clock_stage;
    held_open_stage(&clock_stage, "clock");
    if (ok)
    {
        held_clock_build(&clock_stage, b, first_bits, &theta_stages[0], &theta_stages[1]);
        clock_stage.before = &theta_stages[0].records;
        clock_stage.before_limbs = theta_stages[0].layout.out_limbs;
        clock_stage.also = &theta_stages[1].records;
        clock_stage.also_limbs = theta_stages[1].layout.out_limbs;
        held_open_shared(&clock_stage);
        held_put(clock_stage.shared.data(), clock_stage.program.field_offset[0], clock_stage.program.field_bits[0], first, 1);
        ok = held_lay(&job, &clock_stage, &error);
        declared += held_declared(&clock_stage, lanes);
    }
    const unsigned long long window_low = lanes / 4ull;
    const unsigned long long window_high = 3ull * lanes / 4ull;
    HeldStage turing_stage;
    held_open_stage(&turing_stage, "turing");
    if (ok)
    {
        unsigned int step_bits = 1u;
        while ((steps_between >> step_bits) != 0ull)
        {
            step_bits += 1u;
        }
        held_turing_build(&turing_stage, step_bits, window_low, window_high, &change_stage, &clock_stage);
        turing_stage.before = &clock_stage.records;
        turing_stage.before_limbs = clock_stage.layout.out_limbs;
        turing_stage.also = &clock_stage.records;
        turing_stage.also_limbs = clock_stage.layout.out_limbs;
        ok = held_lay(&job, &turing_stage, &error);
        turing_stage.index = change_stage.index;
        for (unsigned long long lane = 0ull; lane < steps_between; lane += 1ull)
        {
            turing_stage.index[(size_t)(3ull * lane)] = (unsigned int)lane;
        }
        turing_stage.group = steps_between;
        turing_stage.summed = {0u, 1u, 2u, 3u, 4u, 5u, 6u};
        declared += held_declared(&turing_stage, steps_between) + steps_between * change_stage.layout.out_limbs * 4ull;
    }
    ok = ok && sim_job_submit(&job, "exact_zeta_held", count, arguments, declared);
    int same = 1;
    for (unsigned int n = 0u; ok && (n < curves); n += 1u)
    {
        ok = held_sweep(&job, &curve_stages[n], lanes, checked, &error);
        same = same && curve_stages[n].same;
    }
    curve_stages[curves - 1u].same = same;
    ok = ok && held_sweep(&job, &log_stage, lanes, checked, &error);
    ok = ok && held_sweep(&job, &theta_stages[0], lanes, checked, &error);
    ok = ok && held_sweep(&job, &theta_stages[1], lanes, checked, &error);
    const unsigned long long pairs_checked = (pairs < 3ull * checked) ? pairs : 3ull * checked;
    ok = ok && held_sweep(&job, &phase_stage, pairs, pairs_checked, &error);
    ok = ok && held_sweep(&job, &cos_stage, pairs, pairs_checked, &error);
    ok = ok && held_sweep(&job, &term_stages[0], pairs, pairs_checked, &error);
    ok = ok && held_sweep(&job, &term_stages[1], pairs, pairs_checked, &error);
    const int pairs_same = phase_stage.same && cos_stage.same && term_stages[0].same && term_stages[1].same;
    term_stages[0].sums_same = term_stages[0].sums_same && pairs_same;
    term_stages[1].sums_same = term_stages[1].sums_same && pairs_same;
    if (ok)
    {
        sum_records.assign((size_t)(lanes * 2ull * sum_limbs), 0u);
        for (unsigned long long point = 0ull; point < lanes; point += 1ull)
        {
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const unsigned int limbs = term_stages[side].sum_limbs[0];
                const unsigned int *const from = &term_stages[side].sums[0][(size_t)(point * limbs)];
                const unsigned int fill = (from[limbs - 1u] >> 31u) ? 0xFFFFFFFFu : 0u;
                for (unsigned int limb = 0u; limb < sum_limbs; limb += 1u)
                {
                    sum_records[(size_t)((point * 2ull + side) * sum_limbs + limb)] = (limb < limbs) ? from[limb] : fill;
                }
            }
        }
    }
    ok = ok && held_sweep(&job, &tolerance_stages[0], lanes, checked, &error);
    ok = ok && held_sweep(&job, &tolerance_stages[1], lanes, checked, &error);
    verdict_stages[0].shared = sum_records;
    verdict_stages[1].shared = sum_records;
    ok = ok && held_sweep(&job, &verdict_stages[0], lanes, checked, &error);
    ok = ok && held_sweep(&job, &verdict_stages[1], lanes, checked, &error);
    ok = ok && ((steps_between == 0ull) || held_sweep(&job, &change_stage, steps_between,
                                                      (steps_between < checked) ? steps_between : checked, &error));
    ok = ok && held_sweep(&job, &clock_stage, lanes, checked, &error);
    turing_stage.shared = change_stage.records;
    ok = ok && held_sweep(&job, &turing_stage, steps_between, (steps_between < checked) ? steps_between : checked, &error);
    turing_stage.sums_same = turing_stage.sums_same && turing_stage.same && change_stage.same && clock_stage.same;

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        held_write(out, &curve_stages[curves - 1u], lanes);
        held_write(out, &log_stage, lanes);
        held_write(out, &theta_stages[0], lanes);
        held_write(out, &theta_stages[1], lanes);
        held_write_sums(out, &term_stages[0]);
        held_write_sums(out, &term_stages[1]);
        held_write(out, &verdict_stage, lanes);
        held_write_sums(out, &change_stage);
        held_write(out, &clock_stage, lanes);
        held_write_sums(out, &turing_stage);
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    for (unsigned int n = 0u; n < curves; n += 1u)
    {
        held_release(&curve_stages[n]);
    }
    held_release(&log_stage);
    held_release(&theta_stages[0]);
    held_release(&theta_stages[1]);
    held_release(&phase_stage);
    held_release(&cos_stage);
    held_release(&term_stages[0]);
    held_release(&term_stages[1]);
    held_release(&tolerance_stages[0]);
    held_release(&tolerance_stages[1]);
    held_release(&verdict_stages[0]);
    held_release(&verdict_stages[1]);
    held_release(&change_stage);
    held_release(&clock_stage);
    held_release(&turing_stage);
    return sim_close(&job, "exact_zeta_held");
}
