// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-035
//
// Turing's method in held form over a range of cells of the lattice in u, t = 2 pi u^4: the remainder, ln u, theta / pi,
// the main sum, Z's sign, the sign changes, Turing's sums and N, swept on the device as programs of the record machine,
// one job on the device's tessera daemon. Every value is a mantissa in a register and a binary exponent the program
// holds: a product multiplies the mantissas and adds the exponents, and a power of two is where a mantissa is laid in
// the record. No value is held at a fixed scale. The quotients are reads below and above, outward, at the width the
// value is read at.
//
//   Usage:  exact_zeta_held <first> <last> <E> <W> <rate>
//
// Every constant the cells read is a program's output on the device: pi, the logarithms, the roots, cos's and theta's
// constants, Gabcke's curves from the Euler numbers, the verdict's bound, and each cell's lattice, the least b that
// gives it `rate` points or more for each unit theta / pi rises across it. Cells `first` to `last` run in turn, from
// cell 10, where Trudgian's bound holds; N at each cell's point P / 4 and 3P / 4 is held by Turing's method, and the
// sign changes between, and across each seam to the next cell, must number N's rise. The report goes to stdout.
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
// point 3P / 4, which the device sums over the cell; the count stage holds N at the two points from them.
//
// Every stage's records over its first 64 lanes are run again on the host and must equal the device's word for word,
// and every sum the device takes, the host's over the same records.

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

// a whole number off the device: its magnitude's limbs, least significant first, and its sign
typedef struct
{
    std::vector<unsigned int> limbs;
    long long sign;
} HeldWide;

// the poles' records, one a body, laid as held_pole_fields lays them in the stage's program, which is laid out: U_0,
// four values a pole, and B
static void held_pole_records(HeldStage *stage, const HeldWide &first, const HeldWide &bound, const std::vector<HeldWide> &values)
{
    const HeldProgram *const program = &stage->program;
    const unsigned int limbs = stage->shared_limbs;
    const size_t poles = values.size() / 4u;
    stage->shared.assign(poles * limbs, 0u);
    for (size_t n = 0u; n < poles; n += 1u)
    {
        std::vector<unsigned int> record(limbs + 1u, 0u);
        held_put(record.data(), program->field_offset[HELD_POLE_FIRST], program->field_bits[HELD_POLE_FIRST], first.limbs,
                 first.sign);
        for (unsigned int k = 0u; k < 4u; k += 1u)
        {
            held_put(record.data(), program->field_offset[HELD_POLE_LN_LOW + k], program->field_bits[HELD_POLE_LN_LOW + k],
                     values[4u * n + k].limbs, values[4u * n + k].sign);
        }
        held_put(record.data(), program->field_offset[HELD_POLE_BOUND], program->field_bits[HELD_POLE_BOUND], bound.limbs,
                 bound.sign);
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
    int ok = (stage->record != NULL) || (cycle_record_load(&stage->layout, &stage->record, error) != CYCLE_ERROR);
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

static void held_release(HeldStage *stage)
{
    if (stage->record != NULL)
    {
        cycle_record_release(stage->record);
    }
    key_schedule_record_release(&stage->layout);
    keymath_record_release(&stage->key);
}

// THE CONSTANTS, ON THE DEVICE
//
// Every constant the stages read is a program's output: pi by Machin's arctangents, each logarithm by artanh, every root
// by Newton's rule on integers, cos's coefficients, theta's, Gabcke's curves from the Euler numbers, and each bound. A
// value read at a width is read below and above it, the bracket outward.

static HeldWide held_wide_small(long long value)
{
    HeldWide out;
    const unsigned long long magnitude = (value < 0) ? (unsigned long long)(-value) : (unsigned long long)value;
    out.limbs = {(unsigned int)magnitude, (unsigned int)(magnitude >> 32u)};
    out.sign = (value > 0) - (value < 0);
    return out;
}

// `bits` of two's complement at bit `offset` of `words`, as a magnitude and a sign
static HeldWide held_wide_at(const unsigned int *words, unsigned int offset, unsigned int bits)
{
    std::vector<unsigned int> value((bits + 31u) / 32u, 0u);
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int from = offset + bit;
        value[bit / 32u] |= ((words[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    const unsigned int negative = (value[(bits - 1u) / 32u] >> ((bits - 1u) % 32u)) & 1u;
    if ((negative != 0u) && ((bits % 32u) != 0u))
    {
        value[(bits - 1u) / 32u] |= ~0u << (bits % 32u);
    }
    if (negative != 0u)
    {
        unsigned long long carry = 1ull;
        for (size_t at = 0u; at < value.size(); at += 1u)
        {
            carry += (unsigned long long)(unsigned int)~value[at];
            value[at] = (unsigned int)carry;
            carry >>= 32u;
        }
    }
    HeldWide out;
    out.limbs = value;
    int any = 0;
    for (size_t at = 0u; at < value.size(); at += 1u)
    {
        any = any || (value[at] != 0u);
    }
    out.sign = (negative != 0u) ? -1 : (any ? 1 : 0);
    return out;
}

// output `at` of lane `lane` of a stage's records
static HeldWide held_wide_out(const HeldStage *stage, unsigned long long lane, unsigned int at)
{
    const DeviceRecordStep *const place = &stage->layout.step_table[stage->outputs[at]];
    return held_wide_at(&stage->records[(size_t)(lane * stage->layout.out_limbs)], place->out_offset, place->out_bits);
}

// the sum of summed output `which` over run `run`
static HeldWide held_wide_sum(const HeldStage *stage, unsigned int which, unsigned long long run)
{
    const unsigned int limbs = stage->sum_limbs[which];
    return held_wide_at(&stage->sums[which][(size_t)(run * limbs)], 0u, 32u * limbs);
}

// the signed width that holds a value
static unsigned int held_wide_width(const HeldWide &value)
{
    unsigned int width = 32u * (unsigned int)value.limbs.size();
    while ((width > 0u) && (((value.limbs[(width - 1u) / 32u] >> ((width - 1u) % 32u)) & 1u) == 0u))
    {
        width -= 1u;
    }
    return width + 1u;
}

// the low 64 bits of a value's magnitude, with its sign: a count, an index or a lattice's width read off the device
static long long held_wide_word(const HeldWide &value)
{
    const unsigned long long low = (value.limbs.empty() ? 0ull : value.limbs[0]) |
                                   ((value.limbs.size() > 1u ? (unsigned long long)value.limbs[1] : 0ull) << 32u);
    return (value.sign < 0) ? -(long long)low : (long long)low;
}

// rows of values laid as records, one row a body, each column as wide as its widest value
typedef struct
{
    std::vector<unsigned int> words;
    unsigned int limbs;
    std::vector<unsigned int> offset;
    std::vector<unsigned int> bits;
} HeldTable;

static HeldTable held_table(const std::vector<std::vector<HeldWide>> &rows, const std::vector<unsigned int> &floor_bits)
{
    HeldTable table;
    const size_t columns = floor_bits.size();
    unsigned int at = 0u;
    for (size_t column = 0u; column < columns; column += 1u)
    {
        unsigned int widest = floor_bits[column];
        for (size_t row = 0u; row < rows.size(); row += 1u)
        {
            const unsigned int width = held_wide_width(rows[row][column]);
            widest = (width > widest) ? width : widest;
        }
        table.offset.push_back(at);
        table.bits.push_back(widest);
        at += widest;
    }
    table.limbs = (at + 31u) / 32u;
    table.limbs = (table.limbs == 0u) ? 1u : table.limbs;
    table.words.assign(rows.size() * table.limbs, 0u);
    for (size_t row = 0u; row < rows.size(); row += 1u)
    {
        std::vector<unsigned int> record(table.limbs + 1u, 0u);
        for (size_t column = 0u; column < columns; column += 1u)
        {
            held_put(record.data(), table.offset[column], table.bits[column], rows[row][column].limbs, rows[row][column].sign);
        }
        memcpy(&table.words[row * table.limbs], record.data(), table.limbs * sizeof(unsigned int));
    }
    return table;
}

// a column of a table read as a member's field
static unsigned int held_table_field(HeldProgram *program, const HeldTable &table, unsigned int column)
{
    return held_member_field(program, table.bits[column], table.offset[column]);
}

// a table as a stage's member 0, its fields declared with held_table_field
static void held_shared_table(HeldStage *stage, const HeldTable &table)
{
    stage->program.shared_bits = 32u * table.limbs;
    stage->shared = table.words;
}

// a stage with no member 0 of its own: one unread bit
static void held_no_shared(HeldStage *stage)
{
    stage->program.shared_bits = 0u;
    held_field(&stage->program, 1u);
    held_open_shared(stage);
}

// lays a stage out where it is not yet, and runs `lanes` lanes, the host's first 64 against the device's
static int held_run(SimResults *job, HeldStage *stage, unsigned long long lanes, EngineError *error)
{
    int ok = (stage->layout.steps != 0u) || held_lay(job, stage, error);
    return ok && held_sweep(job, stage, lanes, (lanes < 64ull) ? lanes : 64ull, error);
}

// 2^bits - 1, a register known never negative: sums and products of constants
static unsigned int held_mask(HeldProgram *program, unsigned int bits)
{
    if (bits < 63u)
    {
        return held_constant(program, (1ull << bits) - 1ull);
    }
    const unsigned int rest = held_mask(program, bits - 63u);
    return held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_PRODUCT, rest, held_constant(program, 1ull << 63u), 0u),
                     held_constant(program, (1ull << 63u) - 1ull), 0u);
}

// a value known to lie in [0, 2^bits), its register given that width: the and with 2^bits - 1 keeps it
static unsigned int held_narrow(HeldProgram *program, unsigned int value, unsigned int bits)
{
    return held_step(program, ENGINE_RECORD_AND, value, held_mask(program, bits), 0u);
}

// the quotient less 1 and more 1: below and above left / right, whatever their signs
static unsigned int held_down(HeldProgram *program, unsigned int left, unsigned int right)
{
    return held_step(program, ENGINE_RECORD_DIFFERENCE, held_step(program, ENGINE_RECORD_QUOTIENT, left, right, 0u),
                     held_constant(program, 1ull), 0u);
}

static unsigned int held_up(HeldProgram *program, unsigned int left, unsigned int right)
{
    return held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_QUOTIENT, left, right, 0u),
                     held_constant(program, 1ull), 0u);
}

// [left == right], 1 or 0
static unsigned int held_equal(HeldProgram *program, unsigned int left, unsigned int right)
{
    return held_step(program, ENGINE_RECORD_DIFFERENCE,
                     held_step(program, ENGINE_RECORD_DIFFERENCE, held_constant(program, 1ull), held_above(program, left, right), 0u),
                     held_above(program, right, left), 0u);
}

static unsigned int held_bits_of(unsigned long long value);

// floor(x^(1/2)) for 2^least <= x < 2^bits, by Newton's rule from 2^ceil(bits / 2), above the root by at most
// 2^((bits - least + 1) / 2) times: each step takes the lesser of r and (r + x / r) / 2, which about halves r while it
// is twice the root or more, then doubles the bits it holds, then falls to the root and stays there
static unsigned int held_root_from(HeldProgram *program, unsigned int x, unsigned int bits, unsigned int least)
{
    const unsigned int top = (bits + 1u) / 2u + 1u;
    unsigned int r = held_power(program, (bits + 1u) / 2u);
    const unsigned int two = held_constant(program, 2ull);
    const unsigned int mask = held_mask(program, top);
    const unsigned int steps = (bits - least) / 2u + held_bits_of(bits) + 6u;
    for (unsigned int step = 0u; step < steps; step += 1u)
    {
        const unsigned int next = held_step(program, ENGINE_RECORD_QUOTIENT,
                                            held_step(program, ENGINE_RECORD_SUM, r,
                                                      held_step(program, ENGINE_RECORD_QUOTIENT, x, r, 0u), 0u),
                                            two, 0u);
        const unsigned int lesser = held_step(program, ENGINE_RECORD_DIFFERENCE, r,
                                              held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, r, next),
                                                        held_step(program, ENGINE_RECORD_DIFFERENCE, r, next, 0u), 0u),
                                              0u);
        r = held_step(program, ENGINE_RECORD_AND, lesser, mask, 0u);
    }
    return r;
}

// floor(x^(1/2)) for 1 <= x < 2^bits
static unsigned int held_root(HeldProgram *program, unsigned int x, unsigned int bits)
{
    return held_root_from(program, x, bits, 0u);
}

// 1 where r is floor(x^(1/2)): r^2 <= x < (r + 1)^2
static unsigned int held_root_holds(HeldProgram *program, unsigned int x, unsigned int r)
{
    const unsigned int next = held_step(program, ENGINE_RECORD_SUM, r, held_constant(program, 1ull), 0u);
    return held_step(program, ENGINE_RECORD_PRODUCT,
                     held_step(program, ENGINE_RECORD_DIFFERENCE, held_constant(program, 1ull),
                               held_above(program, held_step(program, ENGINE_RECORD_PRODUCT, r, r, 0u), x), 0u),
                     held_above(program, held_step(program, ENGINE_RECORD_PRODUCT, next, next, 0u), x), 0u);
}

// base^e for 0 <= e < 2^bits by squaring, e a register and base a register: the product over e's bits of 1, or of
// base^(2^i) where the bit is set
static unsigned int held_raise(HeldProgram *program, unsigned int base, unsigned int e, unsigned int bits)
{
    unsigned int out = held_constant(program, 1ull);
    unsigned int square = base;
    const unsigned int one = held_constant(program, 1ull);
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int set = held_step(program, ENGINE_RECORD_REMAINDER,
                                           held_step(program, ENGINE_RECORD_QUOTIENT, e, held_power(program, bit), 0u),
                                           held_constant(program, 2ull), 0u);
        out = held_step(program, ENGINE_RECORD_PRODUCT, out,
                        held_step(program, ENGINE_RECORD_SUM, one,
                                  held_step(program, ENGINE_RECORD_PRODUCT, set,
                                            held_step(program, ENGINE_RECORD_DIFFERENCE, square, one, 0u), 0u),
                                  0u),
                        0u);
        if (bit + 1u < bits)
        {
            square = held_step(program, ENGINE_RECORD_PRODUCT, square, square, 0u);
        }
    }
    return out;
}

static unsigned int held_bits_of(unsigned long long value)
{
    unsigned int bits = 0u;
    while ((value >> bits) != 0ull)
    {
        bits += 1u;
    }
    return bits;
}

// arctan(1 / k) at 2^-P, one lane a term j to `count`, count even: term_j = 1 / ((2j + 1) k^(2j + 1)), read below
// and above at 2^-P. The partial sums alternate about arctan, and the one ending at the odd term count - 1 lies below
// it, the one ending at term count above: each lane gives its term to the lower end where j < count, with the reads
// outward, and to the upper end
static void held_machin_build(HeldStage *stage, unsigned long long k, unsigned int precision, unsigned int count)
{
    HeldProgram *const program = &stage->program;
    held_no_shared(stage);
    const unsigned int lane = held_lane_below(program, held_bits_of(count) + 1u);
    const unsigned int odd = held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_SUM, lane, lane, 0u),
                                       held_constant(program, 1ull), 0u);
    const unsigned int denominator = held_step(program, ENGINE_RECORD_PRODUCT, odd,
                                               held_raise(program, held_constant(program, k), odd, held_bits_of(2ull * count + 1ull)), 0u);
    const unsigned int whole = held_power(program, precision);
    const unsigned int below = held_step(program, ENGINE_RECORD_QUOTIENT, whole, denominator, 0u);
    const unsigned int above = held_step(program, ENGINE_RECORD_SUM, below,
                                         held_above(program, whole, held_step(program, ENGINE_RECORD_PRODUCT, below, denominator, 0u)),
                                         0u);
    const unsigned int odd_term = held_step(program, ENGINE_RECORD_REMAINDER, lane, held_constant(program, 2ull), 0u);
    const unsigned int both = held_step(program, ENGINE_RECORD_SUM, below, above, 0u);
    const unsigned int low = held_step(program, ENGINE_RECORD_DIFFERENCE, below,
                                       held_step(program, ENGINE_RECORD_PRODUCT, odd_term, both, 0u), 0u);
    const unsigned int high = held_step(program, ENGINE_RECORD_DIFFERENCE, above,
                                        held_step(program, ENGINE_RECORD_PRODUCT, odd_term, both, 0u), 0u);
    held_output(stage, "low", held_step(program, ENGINE_RECORD_PRODUCT, low, held_above(program, held_constant(program, count), lane), 0u),
                -(long long)precision);
    held_output(stage, "high", high, -(long long)precision);
    stage->group = count + 1u;
    stage->summed = {0u, 1u};
}

// pi = 16 arctan(1/5) - 4 arctan(1/239) below and above at 2^-P from the four sums, member 0's one record, and the 64
// bits after the point at each end
static void held_pi_build(HeldStage *stage, const HeldTable &sums, unsigned int precision)
{
    HeldProgram *const program = &stage->program;
    std::vector<unsigned int> field(4u);
    for (unsigned int k = 0u; k < 4u; k += 1u)
    {
        field[k] = held_table_field(program, sums, k);
    }
    held_shared_table(stage, sums);
    const unsigned int sixteen = held_constant(program, 16ull);
    const unsigned int four = held_constant(program, 4ull);
    const unsigned int low = held_step(program, ENGINE_RECORD_DIFFERENCE,
                                       held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, field[0]), sixteen, 0u),
                                       held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, field[3]), four, 0u), 0u);
    const unsigned int high = held_step(program, ENGINE_RECORD_DIFFERENCE,
                                        held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, field[1]), sixteen, 0u),
                                        held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, field[2]), four, 0u), 0u);
    held_output(stage, "low", low, -(long long)precision);
    held_output(stage, "high", high, -(long long)precision);
    const unsigned int three = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 3ull), held_power(program, precision), 0u);
    const unsigned int point = held_power(program, precision - 64u);
    held_output(stage, "lead_low",
                held_step(program, ENGINE_RECORD_QUOTIENT, held_step(program, ENGINE_RECORD_DIFFERENCE, low, three, 0u), point, 0u), 0);
    held_output(stage, "lead_high",
                held_step(program, ENGINE_RECORD_QUOTIENT, held_step(program, ENGINE_RECORD_DIFFERENCE, high, three, 0u), point, 0u), 0);
}

// Lambda, the least common multiple of the odd numbers below 2L, by gcds, and c_k = Lambda / (2k + 1) for k < L
static void held_lambda_build(HeldStage *stage, unsigned int terms)
{
    HeldProgram *const program = &stage->program;
    held_no_shared(stage);
    unsigned int lambda = held_constant(program, 1ull);
    for (unsigned long long odd = 3ull; odd < 2ull * terms; odd += 2ull)
    {
        const unsigned int o = held_constant(program, odd);
        lambda = held_step(program, ENGINE_RECORD_EXACT_QUOTIENT, held_step(program, ENGINE_RECORD_PRODUCT, lambda, o, 0u),
                           held_step(program, ENGINE_RECORD_GCD, lambda, o, 0u), 0u);
    }
    held_output(stage, "lambda", lambda, 0);
    for (unsigned long long k = 0ull; k < terms; k += 1ull)
    {
        held_output(stage, "c", held_step(program, ENGINE_RECORD_EXACT_QUOTIENT, lambda, held_constant(program, 2ull * k + 1ull), 0u), 0);
    }
}

// ln(p / q) below and above at 2^-P, one lane a value, member 0's record a, c, m with p / (q 2^m) = (c + a) / (c - a):
// 2 artanh(a / c) by its first L terms, 2 a S / (Lambda c^(2L - 1)), S = sum over k < L of c_k a^(2k) c^(2(L - 1 - k))
// by Horner's rule, and its rest, positive and below 2 a^(2L + 1) / ((2L + 1) c^(2L - 1) (c^2 - a^2)); then m ln 2.
// Member 1's one record holds Lambda, the c_k and ln 2 below and above. Each end is read again at 2^-`read`, outward
static void held_ln_build(HeldStage *stage, unsigned int precision, const HeldTable &values, const HeldTable &constants,
                          unsigned int terms, unsigned int read)
{
    HeldProgram *const program = &stage->program;
    const unsigned int a_field = held_table_field(program, values, 0u);
    const unsigned int c_field = held_table_field(program, values, 1u);
    const unsigned int m_field = held_table_field(program, values, 2u);
    held_shared_table(stage, values);
    const unsigned int lambda_field = held_table_field(program, constants, 0u);
    std::vector<unsigned int> c(terms);
    for (unsigned int k = 0u; k < terms; k += 1u)
    {
        c[k] = held_table_field(program, constants, 1u + k);
    }
    const unsigned int two_low_field = held_table_field(program, constants, 1u + terms);
    const unsigned int two_high_field = held_table_field(program, constants, 2u + terms);
    const unsigned int a = held_read(program, a_field);
    const unsigned int big_c = held_read(program, c_field);
    const unsigned int u = held_step(program, ENGINE_RECORD_PRODUCT, a, a, 0u);
    const unsigned int v = held_step(program, ENGINE_RECORD_PRODUCT, big_c, big_c, 0u);
    unsigned int acc = held_read_before(program, c[terms - 1u]);
    unsigned int v_power = v;
    for (unsigned int k = terms - 1u; k > 0u; k -= 1u)
    {
        acc = held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_PRODUCT, acc, u, 0u),
                        held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, c[k - 1u]), v_power, 0u), 0u);
        if (k > 1u)
        {
            v_power = held_step(program, ENGINE_RECORD_PRODUCT, v_power, v, 0u);
        }
    }
    const unsigned int power = (terms > 1u) ? held_step(program, ENGINE_RECORD_PRODUCT, big_c, v_power, 0u) : big_c;
    const unsigned int whole = held_power(program, precision + 1u);
    const unsigned int low = held_step(program, ENGINE_RECORD_QUOTIENT,
                                       held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, a, acc, 0u), whole, 0u),
                                       held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, lambda_field), power, 0u), 0u);
    unsigned int tail = a;
    for (unsigned int k = 0u; k < terms; k += 1u)
    {
        tail = held_step(program, ENGINE_RECORD_PRODUCT, tail, u, 0u);
    }
    const unsigned int rest = held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, tail, whole, 0u),
                                      held_step(program, ENGINE_RECORD_PRODUCT,
                                                held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2ull * terms + 1ull), power, 0u),
                                                held_step(program, ENGINE_RECORD_DIFFERENCE, v, u, 0u), 0u));
    const unsigned int m = held_read(program, m_field);
    const unsigned int below = held_step(program, ENGINE_RECORD_SUM, low,
                                         held_step(program, ENGINE_RECORD_PRODUCT, m, held_read_before(program, two_low_field), 0u),
                                         0u);
    const unsigned int above = held_step(program, ENGINE_RECORD_SUM,
                                         held_step(program, ENGINE_RECORD_SUM, low,
                                                   held_step(program, ENGINE_RECORD_SUM, rest, held_constant(program, 1ull), 0u), 0u),
                                         held_step(program, ENGINE_RECORD_PRODUCT, m, held_read_before(program, two_high_field), 0u),
                                         0u);
    held_output(stage, "low", below, -(long long)precision);
    held_output(stage, "high", above, -(long long)precision);
    const unsigned int coarse = held_power(program, precision - read);
    held_output(stage, "read_low", held_down(program, below, coarse), -(long long)read);
    held_output(stage, "read_high", held_up(program, above, coarse), -(long long)read);
}

// half the sum of two values below and above at 2^-P, member 0's one record x below and above and y below and above,
// read at 2^-W outward
static void held_half_build(HeldStage *stage, const HeldTable &record, unsigned int precision, unsigned int w)
{
    HeldProgram *const program = &stage->program;
    std::vector<unsigned int> field(4u);
    for (unsigned int k = 0u; k < 4u; k += 1u)
    {
        field[k] = held_table_field(program, record, k);
    }
    held_shared_table(stage, record);
    const unsigned int coarse = held_power(program, precision - w + 1u);
    held_output(stage, "low",
                held_down(program, held_step(program, ENGINE_RECORD_SUM, held_read(program, field[0]), held_read(program, field[2]), 0u),
                          coarse),
                -(long long)w);
    held_output(stage, "high",
                held_up(program, held_step(program, ENGINE_RECORD_SUM, held_read(program, field[1]), held_read(program, field[3]), 0u),
                        coarse),
                -(long long)w);
}

// cos's coefficients k_j = pi^(2j) / (2j)! below and above at 2^-W, each from the one before by pi^2 / ((2j - 1) 2j)
// with pi below and above at 2^-P, member 0's one record, the reads outward; and for each J, B_J = the rest past J
// terms, below k_J 4^-J, and every read k_j - K_j 2^-W to J, above at 2^-W, with [k_J 2^W < 4^J], where the rest
// falls below 2^-W
static void held_cosine_build(HeldStage *stage, const HeldTable &pi, unsigned int w, unsigned int precision, unsigned int top)
{
    HeldProgram *const program = &stage->program;
    const unsigned int low_field = held_table_field(program, pi, 0u);
    const unsigned int high_field = held_table_field(program, pi, 1u);
    held_shared_table(stage, pi);
    const unsigned int pi_low = held_read(program, low_field);
    const unsigned int pi_high = held_read(program, high_field);
    const unsigned int low_square = held_step(program, ENGINE_RECORD_PRODUCT, pi_low, pi_low, 0u);
    const unsigned int high_square = held_step(program, ENGINE_RECORD_PRODUCT, pi_high, pi_high, 0u);
    unsigned int low = held_power(program, w);
    unsigned int high = held_power(program, w);
    unsigned int reads = held_constant(program, 0ull);
    const unsigned int scale = held_power(program, 2u * precision);
    for (unsigned int j = 0u; j <= top; j += 1u)
    {
        if (j > 0u)
        {
            const unsigned int divisor = held_step(program, ENGINE_RECORD_PRODUCT,
                                                   held_constant(program, (2ull * j - 1ull) * (2ull * j)), scale, 0u);
            reads = held_step(program, ENGINE_RECORD_SUM, reads, held_step(program, ENGINE_RECORD_DIFFERENCE, high, low, 0u), 0u);
            low = held_narrow(program, held_step(program, ENGINE_RECORD_QUOTIENT,
                                                 held_step(program, ENGINE_RECORD_PRODUCT, low, low_square, 0u), divisor, 0u),
                              w + 3u);
            high = held_narrow(program, held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, high, high_square, 0u), divisor),
                               w + 3u);
        }
        held_output(stage, "low", low, -(long long)w);
        held_output(stage, "high", high, -(long long)w);
        const unsigned int quarter = held_power(program, 2u * j);
        held_output(stage, "bound",
                    held_step(program, ENGINE_RECORD_SUM, held_up(program, high, quarter), reads, 0u), -(long long)w);
        held_output(stage, "fits", held_above(program, quarter, high), 0);
    }
}

// theta's constants at 2^-W: 1 / (96 pi^2), 7 / (46080 pi^4) and 31 / (2580480 pi^6) below and above, and the bound
// on |R_theta| / pi times u^28, 1 / (425216 pi^8), above
static void held_theta_constants_build(HeldStage *stage, const HeldTable &pi, unsigned int w, unsigned int precision)
{
    HeldProgram *const program = &stage->program;
    const unsigned int low_field = held_table_field(program, pi, 0u);
    const unsigned int high_field = held_table_field(program, pi, 1u);
    held_shared_table(stage, pi);
    const unsigned int pi_low = held_read(program, low_field);
    const unsigned int pi_high = held_read(program, high_field);
    const unsigned long long numerators[4] = {1ull, 7ull, 31ull, 1ull};
    const unsigned long long denominators[4] = {96ull, 46080ull, 2580480ull, 425216ull};
    unsigned int low_power = held_step(program, ENGINE_RECORD_PRODUCT, pi_low, pi_low, 0u);
    unsigned int high_power = held_step(program, ENGINE_RECORD_PRODUCT, pi_high, pi_high, 0u);
    const unsigned int low_square = low_power;
    const unsigned int high_square = high_power;
    for (unsigned int k = 0u; k < 4u; k += 1u)
    {
        const unsigned int top = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, numerators[k]),
                                           held_power(program, w + 2u * (k + 1u) * precision), 0u);
        const unsigned int over = held_constant(program, denominators[k]);
        if (k < 3u)
        {
            held_output(stage, "low",
                        held_step(program, ENGINE_RECORD_QUOTIENT, top, held_step(program, ENGINE_RECORD_PRODUCT, over, high_power, 0u), 0u),
                        -(long long)w);
        }
        held_output(stage, "high", held_up(program, top, held_step(program, ENGINE_RECORD_PRODUCT, over, low_power, 0u)), -(long long)w);
        low_power = held_step(program, ENGINE_RECORD_PRODUCT, low_power, low_square, 0u);
        high_power = held_step(program, ENGINE_RECORD_PRODUCT, high_power, high_square, 0u);
    }
}

// the cell at 2^-b: U_0 = floor((nu 4^b - 1)^(1/2)) + 1, the least U with U^2 >= nu 4^b, the lanes to the least U with
// U^2 >= (nu + 1) 4^b, U_0^2 - nu 4^b and U_0^2 + nu 4^b, and floor(nu^(1/2)), each root held to its definition
static void held_setup_build(HeldStage *stage, unsigned long long nu, unsigned int b)
{
    HeldProgram *const program = &stage->program;
    held_no_shared(stage);
    const unsigned int bits = 2u * b + held_bits_of(nu + 1ull) + 1u;
    const unsigned int four = held_power(program, 2u * b);
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int low = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, nu), four, 0u);
    const unsigned int high = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, nu + 1ull), four, 0u);
    const unsigned int x0 = held_step(program, ENGINE_RECORD_DIFFERENCE, low, one, 0u);
    const unsigned int x1 = held_step(program, ENGINE_RECORD_DIFFERENCE, high, one, 0u);
    const unsigned int r0 = held_root(program, x0, bits);
    const unsigned int r1 = held_root(program, x1, bits);
    const unsigned int rnu = held_root(program, held_constant(program, nu), held_bits_of(nu) + 1u);
    const unsigned int first = held_step(program, ENGINE_RECORD_SUM, r0, one, 0u);
    const unsigned int square = held_step(program, ENGINE_RECORD_PRODUCT, first, first, 0u);
    held_output(stage, "first", first, 0);
    held_output(stage, "lanes", held_step(program, ENGINE_RECORD_DIFFERENCE, r1, r0, 0u), 0);
    held_output(stage, "a", held_step(program, ENGINE_RECORD_DIFFERENCE, square, low, 0u), 0);
    held_output(stage, "c", held_step(program, ENGINE_RECORD_SUM, square, low, 0u), 0);
    held_output(stage, "root_nu", rnu, 0);
    held_output(stage, "holds",
                held_step(program, ENGINE_RECORD_PRODUCT,
                          held_step(program, ENGINE_RECORD_PRODUCT, held_root_holds(program, x0, r0), held_root_holds(program, x1, r1), 0u),
                          held_root_holds(program, held_constant(program, nu), rnu), 0u),
                0);
}

// each pole n from 1 to `count`, one lane: a = n - 2^m, c = n + 2^m, m, with 2^m <= n < 2^(m + 1), for ln n; and
// n^(-1/2) below and above at 2^-W, floor((4^W / n)^(1/2)) and one more, the root held to its definition
static void held_poles_build(HeldStage *stage, unsigned int w, unsigned long long count)
{
    HeldProgram *const program = &stage->program;
    held_no_shared(stage);
    const unsigned int bits = held_bits_of(count) + 1u;
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int n = held_step(program, ENGINE_RECORD_SUM, held_lane_below(program, bits), one, 0u);
    unsigned int two_m = one;
    unsigned int m = held_constant(program, 0ull);
    for (unsigned int i = 1u; i < bits; i += 1u)
    {
        const unsigned int past = held_above(program, n, held_constant(program, (1ull << i) - 1ull));
        two_m = held_step(program, ENGINE_RECORD_PRODUCT, two_m, held_step(program, ENGINE_RECORD_SUM, one, past, 0u), 0u);
        m = held_step(program, ENGINE_RECORD_SUM, m, past, 0u);
    }
    held_output(stage, "a", held_step(program, ENGINE_RECORD_DIFFERENCE, n, two_m, 0u), 0);
    held_output(stage, "c", held_step(program, ENGINE_RECORD_SUM, n, two_m, 0u), 0);
    held_output(stage, "m", m, 0);
    const unsigned int x = held_step(program, ENGINE_RECORD_QUOTIENT, held_power(program, 2u * w), n, 0u);
    const unsigned int root = held_root(program, x, 2u * w + 1u);
    held_output(stage, "root_low", root, -(long long)w);
    held_output(stage, "root_high", held_step(program, ENGINE_RECORD_SUM, root, one, 0u), -(long long)w);
    held_output(stage, "holds", held_root_holds(program, x, root), 0);
}

// the lattice for cell nu at `rate` points for each unit theta / pi rises across it, theta / pi read as
// x^2 (ln x^2 - 1) at x^2 = u^4 from ln nu below and ln(nu + 1) above at 2^-P, member 0's one record: b = the bits of
// rate rise 2 (floor((nu + 1)^(1/2)) + 1), whose cell holds as many lanes; a choice of lattice, which the count holds
// whatever it is
static void held_rate_build(HeldStage *stage, const HeldTable &logs, unsigned long long nu, unsigned long long rate,
                            unsigned int precision)
{
    HeldProgram *const program = &stage->program;
    const unsigned int low_field = held_table_field(program, logs, 0u);
    const unsigned int high_field = held_table_field(program, logs, 1u);
    held_shared_table(stage, logs);
    const unsigned int whole = held_power(program, precision);
    const unsigned int upper = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, (nu + 1ull) * (nu + 1ull)),
                                         held_step(program, ENGINE_RECORD_DIFFERENCE,
                                                   held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2ull),
                                                             held_read(program, high_field), 0u),
                                                   whole, 0u), 0u);
    const unsigned int lower = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, nu * nu),
                                         held_step(program, ENGINE_RECORD_DIFFERENCE,
                                                   held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2ull),
                                                             held_read(program, low_field), 0u),
                                                   whole, 0u), 0u);
    const unsigned int root = held_root(program, held_constant(program, nu + 1ull), held_bits_of(nu + 1ull) + 1u);
    const unsigned int spread = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2ull * rate),
                                          held_step(program, ENGINE_RECORD_SUM, root, held_constant(program, 1ull), 0u), 0u);
    const unsigned int need = held_up(program, held_step(program, ENGINE_RECORD_PRODUCT,
                                                         held_step(program, ENGINE_RECORD_DIFFERENCE, upper, lower, 0u), spread, 0u),
                                      whole);
    unsigned int bits = held_constant(program, 0ull);
    for (unsigned int i = 0u; i < 62u; i += 1u)
    {
        bits = held_step(program, ENGINE_RECORD_SUM, bits, held_above(program, need, held_constant(program, (1ull << i) - 1ull)), 0u);
    }
    held_output(stage, "b", bits, 0);
}

// GABCKE'S CURVES
//
// C_n(z)'s coefficient of z^i is gamma_(n,i) = 2^(-2n) sum over k <= 3n/4 of d_k^(n) C(i + m, m) pi^(2k - 2n)
// phi_((i + m) / 2), m = 3n - 4k, zero where i + m is odd, and phi_j, F(z) = cos(pi z^2 / 2 + 3 pi / 8) / cos(pi z)'s
// coefficient of z^(2j), is the sum over m' <= j of s_m' |E_(2(j - m'))| / (2^m' m'! (2(j - m'))!) pi^(2j - m')
// times sin(pi / 8) where m' is even and cos(pi / 8) where it is odd, s running +, -, -, + in m' mod 4. The Euler
// numbers by sum over j <= n of C(2n, 2j) E_2j = 0; lambda by (l + 1) lambda_(l + 1) = sum over k <= l of
// 2^(4k + 1) |E_(2k + 2)| lambda_(l - k), lambda_0 = 1; d^(n + 1)_k = (3n + 1 - 4k)(3n + 2 - 4k) d^(n)_k + d^(n)_(k - 1),
// d^(0) = (1), and lambda_((n + 1) / 4) where 3(n + 1) = 4k (Gabcke's thesis, section 2). Each gamma is held below and
// above at 2^-E.

// x! for each lane x to `top`
static void held_factorial_build(HeldStage *stage, unsigned int top)
{
    HeldProgram *const program = &stage->program;
    held_no_shared(stage);
    const unsigned int lane = held_lane_below(program, held_bits_of(top) + 1u);
    unsigned int acc = held_constant(program, 1ull);
    for (unsigned long long i = 2ull; i <= top; i += 1ull)
    {
        acc = held_step(program, ENGINE_RECORD_PRODUCT, acc,
                        held_step(program, ENGINE_RECORD_SUM, held_constant(program, 1ull),
                                  held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, lane, held_constant(program, i - 1ull)),
                                            held_constant(program, i - 1ull), 0u),
                                  0u),
                        0u);
    }
    held_output(stage, "factorial", acc, 0);
}

// C(a, b) = a! / (b! (a - b)!), the factorials' records as all three members, each lane's three read through the index;
// below 2^a, and a at most `top`
static void held_binomial_build(HeldStage *stage, const HeldStage *factorials, unsigned int top)
{
    HeldProgram *const program = &stage->program;
    const unsigned int field = held_output_field(program, factorials, 0u);
    program->shared_bits = 32u * factorials->layout.out_limbs;
    const unsigned int whole = held_step(program, ENGINE_RECORD_FIELD_SIGNED, field, 0u, 0u);
    const unsigned int part = held_step(program, ENGINE_RECORD_PRODUCT, held_read_before(program, field), held_read_also(program, field), 0u);
    held_output(stage, "binomial", held_narrow(program, held_step(program, ENGINE_RECORD_EXACT_QUOTIENT, whole, part, 0u), top + 1u), 0);
}

// one term of the Euler numbers' recurrence: -C(2n, 2j) E_2j, the binomial's record as member 0 and the table of E as
// member 1; summed over j < n, E_2n
static void held_euler_build(HeldStage *stage, const HeldStage *binomials, unsigned int value_bits)
{
    HeldProgram *const program = &stage->program;
    const unsigned int c = held_output_field(program, binomials, 0u);
    program->shared_bits = 32u * binomials->layout.out_limbs;
    const unsigned int e = held_member_field(program, value_bits, 0u);
    held_output(stage, "term",
                held_step(program, ENGINE_RECORD_DIFFERENCE, held_constant(program, 0ull),
                          held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, c), held_read_before(program, e), 0u), 0u),
                0);
    stage->summed = {0u};
}

// one term of lambda's recurrence, lane k: 2^(4k + 1) |E_(2k + 2)| lambda_(l - k), the table of E as member 0 and that
// of lambda as member 1
static void held_lambda_term_build(HeldStage *stage, unsigned int value_bits, unsigned int lambda_bits, unsigned int top)
{
    HeldProgram *const program = &stage->program;
    const unsigned int e = held_member_field(program, value_bits, 0u);
    program->shared_bits = 32u * ((value_bits + 31u) / 32u);
    const unsigned int lambda = held_member_field(program, lambda_bits, 0u);
    const unsigned int k = held_lane_below(program, held_bits_of(top) + 1u);
    const unsigned int power = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2ull),
                                         held_raise(program, held_constant(program, 16ull), k, held_bits_of(top) + 1u), 0u);
    held_output(stage, "term",
                held_step(program, ENGINE_RECORD_PRODUCT,
                          held_step(program, ENGINE_RECORD_PRODUCT, power,
                                    held_step(program, ENGINE_RECORD_ABSOLUTE, held_read(program, e), 0u, 0u), 0u),
                          held_read_before(program, lambda), 0u),
                0);
    stage->summed = {0u};
}

// an exact quotient of the two values of member 0's one record
static void held_divide_build(HeldStage *stage, const HeldTable &pair)
{
    HeldProgram *const program = &stage->program;
    const unsigned int top = held_table_field(program, pair, 0u);
    const unsigned int bottom = held_table_field(program, pair, 1u);
    held_shared_table(stage, pair);
    held_output(stage, "quotient",
                held_step(program, ENGINE_RECORD_EXACT_QUOTIENT, held_read(program, top), held_read(program, bottom), 0u), 0);
}

// row n + 1 of d, lane k: member 0's one record lambda_((n + 1) / 4), 3n + 1, 3n + 2, 3(n + 1) and row n's length;
// row n's table as members 1 and 2, read at k and k - 1, each 0 past the row's ends
static void held_d_build(HeldStage *stage, const HeldTable &row_constants, unsigned int value_bits, unsigned int top)
{
    HeldProgram *const program = &stage->program;
    std::vector<unsigned int> field(5u);
    for (unsigned int k = 0u; k < 5u; k += 1u)
    {
        field[k] = held_table_field(program, row_constants, k);
    }
    held_shared_table(stage, row_constants);
    const unsigned int d = held_member_field(program, value_bits, 0u);
    const unsigned int k = held_lane_below(program, held_bits_of(top) + 1u);
    const unsigned int four_k = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 4ull), k, 0u);
    const unsigned int here = held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, held_read(program, field[4]), k),
                                        held_read_before(program, d), 0u);
    const unsigned int before = held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, k, held_constant(program, 0ull)),
                                          held_read_also(program, d), 0u);
    const unsigned int grown = held_step(program, ENGINE_RECORD_SUM,
                                         held_step(program, ENGINE_RECORD_PRODUCT,
                                                   held_step(program, ENGINE_RECORD_PRODUCT,
                                                             held_step(program, ENGINE_RECORD_DIFFERENCE, held_read(program, field[1]), four_k, 0u),
                                                             held_step(program, ENGINE_RECORD_DIFFERENCE, held_read(program, field[2]), four_k, 0u), 0u),
                                                   here, 0u),
                                         before, 0u);
    const unsigned int edge = held_equal(program, held_read(program, field[3]), four_k);
    held_output(stage, "d",
                held_step(program, ENGINE_RECORD_SUM, grown,
                          held_step(program, ENGINE_RECORD_PRODUCT, edge,
                                    held_step(program, ENGINE_RECORD_DIFFERENCE, held_read(program, field[0]), grown, 0u), 0u),
                          0u),
                0);
}

// pi^e below and above at 2^-P, lane l for e = l - `least`, by squaring with each product read outward at 2^-P, and
// for e < 0 the reciprocal of pi^|e| outward; member 0's one record pi below and above
static void held_pi_power_build(HeldStage *stage, const HeldTable &pi, unsigned int precision, unsigned int least, unsigned int most)
{
    HeldProgram *const program = &stage->program;
    const unsigned int low_field = held_table_field(program, pi, 0u);
    const unsigned int high_field = held_table_field(program, pi, 1u);
    held_shared_table(stage, pi);
    const unsigned int span = (least > most) ? least : most;
    const unsigned int bits = held_bits_of(span);
    const unsigned int lane = held_lane_below(program, held_bits_of(least + most) + 1u);
    const unsigned int shift = held_constant(program, least);
    const unsigned int size = held_step(program, ENGINE_RECORD_ABSOLUTE, held_step(program, ENGINE_RECORD_DIFFERENCE, lane, shift, 0u), 0u, 0u);
    const unsigned int whole = held_power(program, precision);
    const unsigned int one = held_constant(program, 1ull);
    // every power and every square held is pi^span or less, below 2^(2 span)
    const unsigned int wide = precision + 2u * span + 8u;
    // the power below where `side` is 0 and above where it is 1, each end's chain whole before the other's begins
    auto raise = [&](unsigned int side) -> unsigned int
    {
        unsigned int out = whole;
        unsigned int base = held_read(program, (side == 0u) ? low_field : high_field);
        for (unsigned int bit = 0u; bit < bits; bit += 1u)
        {
            const unsigned int set = held_step(program, ENGINE_RECORD_REMAINDER,
                                               held_step(program, ENGINE_RECORD_QUOTIENT, size, held_power(program, bit), 0u),
                                               held_constant(program, 2ull), 0u);
            const unsigned int by = held_step(program, ENGINE_RECORD_SUM, whole,
                                              held_step(program, ENGINE_RECORD_PRODUCT, set,
                                                        held_step(program, ENGINE_RECORD_DIFFERENCE, base, whole, 0u), 0u), 0u);
            const unsigned int read = held_step(program, ENGINE_RECORD_QUOTIENT, held_step(program, ENGINE_RECORD_PRODUCT, out, by, 0u),
                                                whole, 0u);
            out = held_narrow(program, (side == 0u) ? read : held_step(program, ENGINE_RECORD_SUM, read, set, 0u), wide);
            if (bit + 1u < bits)
            {
                const unsigned int squared = held_step(program, ENGINE_RECORD_PRODUCT, base, base, 0u);
                base = held_narrow(program, (side == 0u) ? held_step(program, ENGINE_RECORD_QUOTIENT, squared, whole, 0u)
                                                         : held_up(program, squared, whole),
                                   wide);
            }
        }
        return out;
    };
    const unsigned int low = raise(0u);
    const unsigned int high = raise(1u);
    const unsigned int below = held_above(program, shift, lane);
    const unsigned int square = held_power(program, 2u * precision);
    const unsigned int low_back = held_step(program, ENGINE_RECORD_QUOTIENT, square, high, 0u);
    const unsigned int high_back = held_step(program, ENGINE_RECORD_SUM, held_step(program, ENGINE_RECORD_QUOTIENT, square, low, 0u), one, 0u);
    held_output(stage, "low",
                held_step(program, ENGINE_RECORD_SUM, low,
                          held_step(program, ENGINE_RECORD_PRODUCT, below, held_step(program, ENGINE_RECORD_DIFFERENCE, low_back, low, 0u), 0u),
                          0u),
                -(long long)precision);
    held_output(stage, "high",
                held_step(program, ENGINE_RECORD_SUM, high,
                          held_step(program, ENGINE_RECORD_PRODUCT, below, held_step(program, ENGINE_RECORD_DIFFERENCE, high_back, high, 0u), 0u),
                          0u),
                -(long long)precision);
}

// sin(pi / 8) and cos(pi / 8) below and above at 2^-P: (2 -+ 2^(1/2))^(1/2) / 2, 2^(1/2) 2^P between s and s + 1,
// s = floor((2 4^P)^(1/2)), each root held to its definition; every radicand is 4^P / 2 or more. Lane 0 gives sin
// below, lane 1 sin above, lane 2 cos below and lane 3 cos above
static void held_eighth_build(HeldStage *stage, unsigned int precision)
{
    HeldProgram *const program = &stage->program;
    held_no_shared(stage);
    const unsigned int k = held_lane_below(program, 3u);
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int two = held_constant(program, 2ull);
    const unsigned int whole = held_power(program, precision);
    const unsigned int doubled = held_power(program, precision + 1u);
    const unsigned int square = held_step(program, ENGINE_RECORD_PRODUCT, two, held_power(program, 2u * precision), 0u);
    const unsigned int s = held_root_from(program, square, 2u * precision + 3u, 2u * precision + 1u);
    const unsigned int s_holds = held_root_holds(program, square, s);
    // 2 2^P - s - 1, 2 2^P - s, 2 2^P + s and 2 2^P + s + 1
    const unsigned int turn = held_step(program, ENGINE_RECORD_DIFFERENCE,
                                        held_step(program, ENGINE_RECORD_PRODUCT, two, held_above(program, k, one), 0u), one, 0u);
    const unsigned int nudge = held_step(program, ENGINE_RECORD_DIFFERENCE, held_equal(program, k, held_constant(program, 3ull)),
                                         held_equal(program, k, held_constant(program, 0ull)), 0u);
    const unsigned int arg = held_step(program, ENGINE_RECORD_SUM,
                                       held_step(program, ENGINE_RECORD_SUM, doubled, held_step(program, ENGINE_RECORD_PRODUCT, turn, s, 0u), 0u),
                                       nudge, 0u);
    const unsigned int x = held_step(program, ENGINE_RECORD_PRODUCT, arg, whole, 0u);
    const unsigned int r = held_root_from(program, x, 2u * precision + 4u, 2u * precision - 1u);
    const unsigned int up = held_step(program, ENGINE_RECORD_PRODUCT, two,
                                      held_step(program, ENGINE_RECORD_REMAINDER, k, two, 0u), 0u);
    held_output(stage, "value", held_step(program, ENGINE_RECORD_QUOTIENT, held_step(program, ENGINE_RECORD_SUM, r, up, 0u), two, 0u),
                -(long long)precision);
    held_output(stage, "holds", held_step(program, ENGINE_RECORD_PRODUCT, s_holds, held_root_holds(program, x, r), 0u), 0);
}

// pi^e sin(pi / 8) and pi^e cos(pi / 8) below and above at 2^-P, lane 2e + w, w = 0 for sin and 1 for cos: the power's
// record as member 0 through the index, and member 1's one record sin below and above and cos below and above; e at
// most `span`, and each below 2^(P + 2 span)
static void held_pi_eighth_build(HeldStage *stage, const HeldStage *powers, const HeldTable &eighth, unsigned int precision,
                                 unsigned int lane_bits, unsigned int span)
{
    HeldProgram *const program = &stage->program;
    const unsigned int power_low = held_output_field(program, powers, 0u);
    const unsigned int power_high = held_output_field(program, powers, 1u);
    program->shared_bits = 32u * powers->layout.out_limbs;
    std::vector<unsigned int> r(4u);
    for (unsigned int k = 0u; k < 4u; k += 1u)
    {
        r[k] = held_table_field(program, eighth, k);
    }
    const unsigned int w = held_step(program, ENGINE_RECORD_REMAINDER, held_lane_below(program, lane_bits), held_constant(program, 2ull), 0u);
    const unsigned int r_low = held_step(program, ENGINE_RECORD_SUM, held_read_before(program, r[0]),
                                         held_step(program, ENGINE_RECORD_PRODUCT, w,
                                                   held_step(program, ENGINE_RECORD_DIFFERENCE, held_read_before(program, r[2]),
                                                             held_read_before(program, r[0]), 0u), 0u), 0u);
    const unsigned int r_high = held_step(program, ENGINE_RECORD_SUM, held_read_before(program, r[1]),
                                          held_step(program, ENGINE_RECORD_PRODUCT, w,
                                                    held_step(program, ENGINE_RECORD_DIFFERENCE, held_read_before(program, r[3]),
                                                              held_read_before(program, r[1]), 0u), 0u), 0u);
    const unsigned int whole = held_power(program, precision);
    const unsigned int wide = precision + 2u * span + 8u;
    held_output(stage, "low",
                held_narrow(program,
                            held_step(program, ENGINE_RECORD_QUOTIENT,
                                      held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, power_low), r_low, 0u), whole, 0u),
                            wide),
                -(long long)precision);
    held_output(stage, "high",
                held_narrow(program,
                            held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, power_high), r_high, 0u), whole),
                            wide),
                -(long long)precision);
}

// phi_j below and above at 2^-P, lane j (top + 1) + m': member 0 the table of |E_2x| and (2x)! at x = j - m', member 1
// the factorials' records at m', member 2 pi^(2j - m') times the eighth of w = m' mod 2; each term read outward and 0
// where m' > j, summed over m'
static void held_phi_build(HeldStage *stage, const HeldTable &euler, const HeldStage *factorials, const HeldStage *pi_eighths,
                           unsigned int top, unsigned int precision)
{
    HeldProgram *const program = &stage->program;
    const unsigned int e_field = held_table_field(program, euler, 0u);
    const unsigned int even_field = held_table_field(program, euler, 1u);
    held_shared_table(stage, euler);
    const unsigned int factorial = held_output_field(program, factorials, 0u);
    const unsigned int low_field = held_output_field(program, pi_eighths, 0u);
    const unsigned int high_field = held_output_field(program, pi_eighths, 1u);
    const unsigned int row = top + 1u;
    const unsigned int lane = held_lane_below(program, held_bits_of((unsigned long long)row * row) + 1u);
    const unsigned int j = held_step(program, ENGINE_RECORD_QUOTIENT, lane, held_constant(program, row), 0u);
    const unsigned int mp = held_step(program, ENGINE_RECORD_REMAINDER, lane, held_constant(program, row), 0u);
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int valid = held_above(program, held_step(program, ENGINE_RECORD_SUM, j, one, 0u), mp);
    const unsigned int quarter = held_step(program, ENGINE_RECORD_REMAINDER, mp, held_constant(program, 4ull), 0u);
    const unsigned int turned = held_step(program, ENGINE_RECORD_DIFFERENCE, held_above(program, quarter, held_constant(program, 0ull)),
                                          held_above(program, quarter, held_constant(program, 2ull)), 0u);
    const unsigned int den = held_step(program, ENGINE_RECORD_PRODUCT,
                                       held_step(program, ENGINE_RECORD_PRODUCT,
                                                 held_raise(program, held_constant(program, 2ull), mp, held_bits_of(top) + 1u),
                                                 held_read_before(program, factorial), 0u),
                                       held_read(program, even_field), 0u);
    // each term at most 2 pi^(2j) at 2^-P, |E_2x| <= 2 (2x)!, below 2^(P + 4 top + 2)
    const unsigned int wide = 8u + 4u * top;
    const unsigned int e = held_step(program, ENGINE_RECORD_ABSOLUTE, held_read(program, e_field), 0u, 0u);
    const unsigned int q_low = held_narrow(program, held_step(program, ENGINE_RECORD_QUOTIENT,
                                                              held_step(program, ENGINE_RECORD_PRODUCT, e, held_read_also(program, low_field), 0u),
                                                              den, 0u),
                                           precision + wide);
    const unsigned int q_high = held_narrow(program,
                                            held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, e, held_read_also(program, high_field), 0u),
                                                    den),
                                            precision + wide);
    const unsigned int both = held_step(program, ENGINE_RECORD_SUM, q_low, q_high, 0u);
    const unsigned int low = held_step(program, ENGINE_RECORD_DIFFERENCE, q_low,
                                       held_step(program, ENGINE_RECORD_PRODUCT, turned, both, 0u), 0u);
    const unsigned int high = held_step(program, ENGINE_RECORD_DIFFERENCE, q_high,
                                        held_step(program, ENGINE_RECORD_PRODUCT, turned, both, 0u), 0u);
    held_output(stage, "low", held_step(program, ENGINE_RECORD_PRODUCT, valid, low, 0u), 0);
    held_output(stage, "high", held_step(program, ENGINE_RECORD_PRODUCT, valid, high, 0u), 0);
    stage->group = row;
    stage->summed = {0u, 1u};
}

// gamma_(n,i) below and above at 2^-E, lane (n count + i) 8 + k: member 0 the table of d_k^(n), pi^(2k - 2n) below and
// above at (n, k), member 1 the binomials' records at (i + m, m), member 2 the table of phi below and above at
// (i + m) / 2; each term read outward and 0 where 4k > 3n or i + n is odd, summed over k
static void held_gamma_build(HeldStage *stage, const HeldTable &d, const HeldStage *binomials, const HeldTable &phi,
                             unsigned int top, unsigned int count, unsigned int precision, unsigned int big_e)
{
    HeldProgram *const program = &stage->program;
    const unsigned int d_field = held_table_field(program, d, 0u);
    const unsigned int p_low_field = held_table_field(program, d, 1u);
    const unsigned int p_high_field = held_table_field(program, d, 2u);
    held_shared_table(stage, d);
    const unsigned int c_field = held_output_field(program, binomials, 0u);
    const unsigned int f_low_field = held_table_field(program, phi, 0u);
    const unsigned int f_high_field = held_table_field(program, phi, 1u);
    const unsigned long long lanes = (unsigned long long)(top + 1u) * count * 8ull;
    const unsigned int lane = held_lane_below(program, held_bits_of(lanes) + 1u);
    const unsigned int n = held_step(program, ENGINE_RECORD_QUOTIENT, lane, held_constant(program, 8ull * count), 0u);
    const unsigned int i = held_step(program, ENGINE_RECORD_REMAINDER, held_step(program, ENGINE_RECORD_QUOTIENT, lane, held_constant(program, 8ull), 0u),
                                     held_constant(program, count), 0u);
    const unsigned int k = held_step(program, ENGINE_RECORD_REMAINDER, lane, held_constant(program, 8ull), 0u);
    const unsigned int one = held_constant(program, 1ull);
    const unsigned int valid = held_step(program, ENGINE_RECORD_PRODUCT,
                                         held_step(program, ENGINE_RECORD_DIFFERENCE, one,
                                                   held_above(program, held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 4ull), k, 0u),
                                                              held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 3ull), n, 0u)),
                                                   0u),
                                         held_step(program, ENGINE_RECORD_DIFFERENCE, one,
                                                   held_step(program, ENGINE_RECORD_REMAINDER, held_step(program, ENGINE_RECORD_SUM, i, n, 0u),
                                                             held_constant(program, 2ull), 0u),
                                                   0u),
                                         0u);
    const unsigned int dc = held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, d_field), held_read_before(program, c_field), 0u);
    const unsigned int a_low = held_step(program, ENGINE_RECORD_PRODUCT, dc, held_read(program, p_low_field), 0u);
    const unsigned int a_high = held_step(program, ENGINE_RECORD_PRODUCT, dc, held_read(program, p_high_field), 0u);
    const unsigned int spread = held_step(program, ENGINE_RECORD_DIFFERENCE, a_high, a_low, 0u);
    const unsigned int f_low = held_read_also(program, f_low_field);
    const unsigned int f_high = held_read_also(program, f_high_field);
    const unsigned int zero = held_constant(program, 0ull);
    const unsigned int low = held_step(program, ENGINE_RECORD_PRODUCT, f_low,
                                       held_step(program, ENGINE_RECORD_DIFFERENCE, a_high,
                                                 held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, f_low, zero), spread, 0u), 0u),
                                       0u);
    const unsigned int high = held_step(program, ENGINE_RECORD_PRODUCT, f_high,
                                        held_step(program, ENGINE_RECORD_SUM, a_low,
                                                  held_step(program, ENGINE_RECORD_PRODUCT, held_above(program, f_high, zero), spread, 0u), 0u),
                                        0u);
    const unsigned int divisor = held_step(program, ENGINE_RECORD_PRODUCT, held_power(program, 2u * precision - big_e),
                                           held_raise(program, held_constant(program, 4ull), n, held_bits_of(top) + 1u), 0u);
    held_output(stage, "low", held_step(program, ENGINE_RECORD_PRODUCT, valid, held_down(program, low, divisor), 0u), -(long long)big_e);
    held_output(stage, "high", held_step(program, ENGINE_RECORD_PRODUCT, valid, held_up(program, high, divisor), 0u), -(long long)big_e);
    stage->group = 8u;
    stage->summed = {0u, 1u};
}

// the spread of each gamma, above less below, member 0's table at the lane, summed over each curve
static void held_spread_build(HeldStage *stage, const HeldTable &gammas, unsigned int count)
{
    HeldProgram *const program = &stage->program;
    const unsigned int low = held_table_field(program, gammas, 0u);
    const unsigned int high = held_table_field(program, gammas, 1u);
    held_shared_table(stage, gammas);
    held_output(stage, "spread", held_step(program, ENGINE_RECORD_DIFFERENCE, held_read(program, high), held_read(program, low), 0u), 0);
    stage->group = count;
    stage->summed = {0u};
}

// the verdict's bound B at 2^-W for cell nu: Gabcke's G = d_K t^(-(2K + 3) / 4) at most d_K (2 pi nu^2)^(-(2K + 3) / 4),
// its 2^W times above as the ceiling of the fourth root of the ceiling of d_K^4 2^(4W) / (2 pi nu^2)^(2K + 3), pi below
// read at 2^-40; and E_r, the coefficients' spreads s_n, each gamma within its spread of the one held, |z| <= 1 and
// x^(-n - 1/2) <= nu^-n / floor(nu^(1/2)): the ceiling of sum over n of s_n nu^(K - n) 2^W / (nu^K floor(nu^(1/2)) 2^E).
// Member 0's one record: pi below, then s_0 to s_K
static void held_bound_build(HeldStage *stage, const HeldTable &record, unsigned long long nu, unsigned int top, unsigned int w,
                             unsigned int precision, unsigned int big_e, unsigned long long gabcke_thousandths)
{
    HeldProgram *const program = &stage->program;
    const unsigned int pi_field = held_table_field(program, record, 0u);
    std::vector<unsigned int> s(top + 1u);
    for (unsigned int n = 0u; n <= top; n += 1u)
    {
        s[n] = held_table_field(program, record, 1u + n);
    }
    held_shared_table(stage, record);
    const unsigned int coarse = 40u;
    const unsigned int pi_low = held_narrow(program,
                                            held_step(program, ENGINE_RECORD_QUOTIENT, held_read(program, pi_field),
                                                      held_power(program, precision - coarse), 0u),
                                            coarse + 2u);
    const unsigned int base = held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2ull * nu * nu), pi_low, 0u);
    unsigned int raised = base;
    for (unsigned int k = 1u; k < 2u * top + 3u; k += 1u)
    {
        raised = held_step(program, ENGINE_RECORD_PRODUCT, raised, base, 0u);
    }
    const unsigned int d = held_constant(program, gabcke_thousandths);
    const unsigned int d2 = held_step(program, ENGINE_RECORD_PRODUCT, d, d, 0u);
    const unsigned int thousand = held_constant(program, 1000ull * 1000ull);
    const unsigned int top_part = held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, d2, d2, 0u),
                                            held_power(program, 4u * w + (2u * top + 3u) * coarse), 0u);
    const unsigned int bottom = held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, thousand, thousand, 0u),
                                          raised, 0u);
    const unsigned int x = held_narrow(program, held_up(program, top_part, bottom), 4u * w + 4u * held_bits_of(gabcke_thousandths) + 2u);
    const unsigned int r = held_root(program, x, 4u * w + 4u * held_bits_of(gabcke_thousandths) + 2u);
    const unsigned int ceiling = held_step(program, ENGINE_RECORD_SUM, r, held_above(program, x, held_step(program, ENGINE_RECORD_PRODUCT, r, r, 0u)), 0u);
    const unsigned int r2 = held_root(program, ceiling, 2u * w + 2u * held_bits_of(gabcke_thousandths) + 3u);
    const unsigned int gabcke = held_step(program, ENGINE_RECORD_SUM, r2,
                                          held_above(program, ceiling, held_step(program, ENGINE_RECORD_PRODUCT, r2, r2, 0u)), 0u);
    unsigned int reads = held_constant(program, 0ull);
    unsigned int power = held_constant(program, 1ull);
    for (unsigned int n = top + 1u; n > 0u; n -= 1u)
    {
        reads = held_step(program, ENGINE_RECORD_SUM, reads,
                          held_step(program, ENGINE_RECORD_PRODUCT, held_read(program, s[n - 1u]), power, 0u), 0u);
        power = held_step(program, ENGINE_RECORD_PRODUCT, power, held_constant(program, nu), 0u);
    }
    const unsigned int root_nu = held_root(program, held_constant(program, nu), held_bits_of(nu) + 1u);
    const unsigned int over = held_step(program, ENGINE_RECORD_PRODUCT,
                                        held_step(program, ENGINE_RECORD_PRODUCT,
                                                  held_step(program, ENGINE_RECORD_QUOTIENT, power, held_constant(program, nu), 0u), root_nu, 0u),
                                        held_power(program, big_e), 0u);
    const unsigned int reads_bound = held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, reads, held_power(program, w), 0u), over);
    held_output(stage, "bound", held_step(program, ENGINE_RECORD_SUM, gabcke, reads_bound, 0u), -(long long)w);
    held_output(stage, "gabcke", gabcke, -(long long)w);
    held_output(stage, "reads", reads_bound, -(long long)w);
    held_output(stage, "holds", held_step(program, ENGINE_RECORD_PRODUCT, held_root_holds(program, x, r), held_root_holds(program, ceiling, r2), 0u), 0);
}

// N at points a and e by Turing's method, member 0's one record: the Turing stage's sums below_theta, below_zeros,
// below_count, past_theta, past_zeros, past_count, U^4 at points 0, a, e and the last, pi below and above at 2^-P and
// ln 2 above at 2^-P. With D = U_a^4 - U_0^4 and t at most 2 pi U^4 / 16^b, B = 2.067 + 0.059 ln t_2 (Trudgian,
// Improvements to Turing's method, Theorem 2.2), ln t_2 at most the bits of the ceiling of t_2's bound times ln 2:
// N(T_a) D 2^W >= D 2^W + below_theta + (below_zeros - U_0^4 below_count) 2^W - B 16^b 2^W / (2 pi), and
// N(T_e) D' 2^W <= D' 2^W + B' 16^b 2^W / (2 pi) + past_theta - (U_l^4 past_count - past_zeros) 2^W, D' = U_l^4 - U_e^4.
// N is whole: the least whole number at or above the first, and the greatest at or below the second
static void held_count_build(HeldStage *stage, const HeldTable &record, unsigned int b, unsigned int w, unsigned int precision)
{
    HeldProgram *const program = &stage->program;
    std::vector<unsigned int> field(13u);
    for (unsigned int k = 0u; k < 13u; k += 1u)
    {
        field[k] = held_table_field(program, record, k);
    }
    held_shared_table(stage, record);
    std::vector<unsigned int> v(13u);
    for (unsigned int k = 0u; k < 13u; k += 1u)
    {
        v[k] = held_read(program, field[k]);
    }
    const unsigned int big_w = held_power(program, w);
    const unsigned int sixteen = held_power(program, 4u * b);
    const unsigned int whole = held_power(program, precision);
    // B 16^b 2^W / (2 pi) above, at t = 2 pi U^4 / 16^b
    auto trudgian = [&](unsigned int fourth) -> unsigned int
    {
        const unsigned int t = held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_SUM, v[11], v[11], 0u), fourth, 0u),
                                       held_step(program, ENGINE_RECORD_PRODUCT, sixteen, whole, 0u));
        unsigned int bits = held_constant(program, 0ull);
        for (unsigned int i = 0u; i < 62u; i += 1u)
        {
            bits = held_step(program, ENGINE_RECORD_SUM, bits, held_above(program, t, held_constant(program, (1ull << i) - 1ull)), 0u);
        }
        const unsigned int top = held_step(program, ENGINE_RECORD_SUM,
                                           held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2067ull), whole, 0u),
                                           held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 59ull),
                                                     held_step(program, ENGINE_RECORD_PRODUCT, bits, v[12], 0u), 0u), 0u);
        return held_up(program, held_step(program, ENGINE_RECORD_PRODUCT, held_step(program, ENGINE_RECORD_PRODUCT, top, sixteen, 0u), big_w, 0u),
                       held_step(program, ENGINE_RECORD_PRODUCT, held_constant(program, 2000ull), v[10], 0u));
    };
    const unsigned int below_d = held_step(program, ENGINE_RECORD_DIFFERENCE, v[7], v[6], 0u);
    const unsigned int below_scale = held_step(program, ENGINE_RECORD_PRODUCT, below_d, big_w, 0u);
    const unsigned int below_top = held_step(program, ENGINE_RECORD_DIFFERENCE,
                                             held_step(program, ENGINE_RECORD_SUM,
                                                       held_step(program, ENGINE_RECORD_SUM, below_scale, v[0], 0u),
                                                       held_step(program, ENGINE_RECORD_PRODUCT,
                                                                 held_step(program, ENGINE_RECORD_DIFFERENCE, v[1],
                                                                           held_step(program, ENGINE_RECORD_PRODUCT, v[6], v[2], 0u), 0u),
                                                                 big_w, 0u), 0u),
                                             trudgian(v[7]), 0u);
    held_output(stage, "low",
                held_step(program, ENGINE_RECORD_QUOTIENT,
                          held_step(program, ENGINE_RECORD_DIFFERENCE, held_step(program, ENGINE_RECORD_SUM, below_top, below_scale, 0u),
                                    held_constant(program, 1ull), 0u),
                          below_scale, 0u),
                0);
    const unsigned int past_d = held_step(program, ENGINE_RECORD_DIFFERENCE, v[9], v[8], 0u);
    const unsigned int past_scale = held_step(program, ENGINE_RECORD_PRODUCT, past_d, big_w, 0u);
    const unsigned int past_top = held_step(program, ENGINE_RECORD_DIFFERENCE,
                                            held_step(program, ENGINE_RECORD_SUM,
                                                      held_step(program, ENGINE_RECORD_SUM, past_scale, trudgian(v[9]), 0u), v[3], 0u),
                                            held_step(program, ENGINE_RECORD_PRODUCT,
                                                      held_step(program, ENGINE_RECORD_DIFFERENCE,
                                                                held_step(program, ENGINE_RECORD_PRODUCT, v[9], v[5], 0u), v[4], 0u),
                                                      big_w, 0u), 0u);
    held_output(stage, "high", held_step(program, ENGINE_RECORD_QUOTIENT, past_top, past_scale, 0u), 0);
}

// THE RANGE
//
// Every stage of a cell, from the inputs the constants' programs give, then Turing's count; and over a range of cells,
// each at the lattice its rate asks for, the seams.

// the job's declaration: the device holds one stage's records and members at a time
static const unsigned long long HELD_DECLARED = 192ull << 20u;

// Gabcke's curves to C_10, and d_10 in thousandths: of his bounds d_K t^(-(2K + 3) / 4) at t = 200 (Satz 3.2.2, by (a)
// for K = 10), K = 10's is least, and it is least at every t past 200
static const unsigned int HELD_TOP = 10u;
static const unsigned long long HELD_GABCKE_D = 25966000ull;

// one cell's inputs, each a value the constants' programs gave: the lattice, Gabcke's coefficients, the log series'
// c_k, theta's eleven constants, cos's bound and coefficients, each pole's ln n and n^(-1/2) below and above, the
// verdict's bound, and pi below and above and ln 2 above at 2^-P for Turing's count
typedef struct
{
    unsigned long long nu;
    unsigned int b;
    unsigned long long lanes;
    unsigned int big_e;
    unsigned int w;
    unsigned int precision;
    HeldWide first;
    const std::vector<std::vector<HeldWide>> *gammas;
    std::vector<HeldWide> c;
    std::vector<HeldWide> theta;
    const HeldWide *cos_bound;
    const std::vector<HeldWide> *k_low;
    std::vector<HeldWide> poles;
    HeldWide verdict;
    const HeldWide *pi_low;
    const HeldWide *pi_high;
    const HeldWide *two_high;
} HeldCell;

// what a cell gives the range: its checks, the signs at its ends, N held at points a and e, and the sign changes
// before a, past e and between them
typedef struct
{
    int checked;
    unsigned long long a;
    unsigned long long e;
    long long first_sign;
    long long last_sign;
    long long low;
    long long high;
    long long before;
    long long after;
    long long between;
} HeldCount;

// the constants every cell reads: pi below and above and ln 2 above at 2^-P, Lambda, the c_k and ln 2 below and above
// for each logarithm, theta's seven, cos's bound and coefficients, Gabcke's coefficients and each curve's spread
typedef struct
{
    unsigned int precision;
    unsigned int w;
    unsigned int big_e;
    HeldWide pi_low;
    HeldWide pi_high;
    HeldWide two_high;
    HeldTable logs;
    unsigned int log_terms;
    std::vector<HeldWide> theta;
    HeldWide cos_bound;
    std::vector<HeldWide> k_low;
    std::vector<std::vector<HeldWide>> gammas;
    std::vector<HeldWide> spreads;
} HeldConstants;

static void held_put_wide(HeldStage *stage, unsigned int field, const HeldWide &value)
{
    held_put(stage->shared.data(), stage->program.field_offset[field], stage->program.field_bits[field], value.limbs,
             value.sign);
}

// one cell, every stage laid out and then swept, and Turing's count from the device's sums
static int held_cell(SimResults *job, const HeldCell &cell, HeldCount *count, EngineError *error)
{
    const unsigned long long lanes = cell.lanes;
    const unsigned long long checked = (lanes < 64ull) ? lanes : 64ull;
    const unsigned long long nu = cell.nu;
    const unsigned int b = cell.b;
    const std::vector<std::vector<HeldWide>> &gammas = *cell.gammas;
    const unsigned int curves = (unsigned int)gammas.size();
    const long long big_e = (long long)cell.big_e;
    const unsigned int w = cell.w;
    const unsigned int log_terms = (unsigned int)cell.c.size();
    const unsigned int cos_terms = (unsigned int)cell.k_low->size();
    // the field of U_0 holds its value with room to spare
    const unsigned int first_bits = held_wide_width(cell.first) + 2u;
    std::vector<std::vector<unsigned int>> gamma_bits(curves);
    for (unsigned int n = 0u; n < curves; n += 1u)
    {
        for (size_t j = 0u; j < gammas[n].size(); j += 1u)
        {
            gamma_bits[n].push_back(held_wide_width(gammas[n][j]));
        }
    }
    std::vector<unsigned int> c_bits(log_terms);
    for (unsigned int k = 0u; k < log_terms; k += 1u)
    {
        c_bits[k] = held_wide_width(cell.c[k]);
    }
    std::vector<unsigned int> theta_bits(cell.theta.size());
    for (size_t k = 0u; k < cell.theta.size(); k += 1u)
    {
        theta_bits[k] = held_wide_width(cell.theta[k]);
    }
    std::vector<unsigned int> k_widths(cos_terms);
    for (unsigned int j = 0u; j < cos_terms; j += 1u)
    {
        k_widths[j] = held_wide_width((*cell.k_low)[j]);
    }
    std::vector<unsigned int> pole_widths(HELD_POLE_FIELDS, 0u);
    pole_widths[HELD_POLE_FIRST] = first_bits;
    pole_widths[HELD_POLE_BOUND] = held_wide_width(*cell.cos_bound);
    for (size_t at = 0u; at < cell.poles.size(); at += 1u)
    {
        const unsigned int field = HELD_POLE_LN_LOW + (unsigned int)(at % 4u);
        const unsigned int bits = held_wide_width(cell.poles[at]);
        pole_widths[field] = (bits > pole_widths[field]) ? bits : pole_widths[field];
    }
    const unsigned int verdict_bits = held_wide_width(cell.verdict);

    const long long least = held_least_exponent(big_e, b, gamma_bits);
    std::vector<HeldStage> curve_stages(curves);
    std::vector<std::string> curve_names(curves);
    const std::vector<unsigned int> one_limb(1u, 1u);
    int ok = 1;
    for (unsigned int n = 0u; n < curves; n += 1u)
    {
        curve_names[n] = (n + 1u == curves) ? std::string("curves") : "curve" + std::to_string(n);
        held_open_stage(&curve_stages[n], curve_names[n].c_str());
    }
    for (unsigned int n = 0u; ok && (n < curves); n += 1u)
    {
        HeldStage *const stage = &curve_stages[n];
        const HeldStage *const before = (n > 0u) ? &curve_stages[n - 1u] : NULL;
        const unsigned int lift = (unsigned int)(held_curve_exponent(big_e, b, n, gamma_bits[n].size()) - least);
        held_build(stage, b, n, curves - 1u, least, first_bits, gamma_bits[n], lift, before);
        stage->before = (before != NULL) ? &before->records : NULL;
        stage->before_limbs = (before != NULL) ? before->layout.out_limbs : 0u;
        held_open_shared(stage);
        held_put(stage->shared.data(), stage->program.field_offset[0], stage->program.field_bits[0], one_limb,
                 ((nu - 1ull) % 2ull) ? -1 : 1);
        held_put_wide(stage, 1u, cell.first);
        const unsigned int held = (unsigned int)gammas[n].size();
        for (unsigned int j = 0u; j < held; j += 1u)
        {
            const std::vector<unsigned int> laid = held_shifted(gammas[n][j].limbs, 2u * b * (held - 1u - j) + lift);
            held_put(stage->shared.data(), stage->program.field_offset[2u + j], stage->program.field_bits[2u + j], laid,
                     gammas[n][j].sign);
        }
        ok = held_lay(job, stage, error);
    }

    HeldStage log_stage;
    held_open_stage(&log_stage, "log");
    if (ok)
    {
        held_log_build(&log_stage, b, first_bits, c_bits);
        held_open_shared(&log_stage);
        held_put_wide(&log_stage, 0u, cell.first);
        for (unsigned int k = 0u; k < log_terms; k += 1u)
        {
            held_put_wide(&log_stage, 1u + k, cell.c[k]);
        }
        ok = held_lay(job, &log_stage, error);
    }
    HeldStage theta_stages[2];
    const char *const theta_names[2] = {"theta_lower", "theta_upper"};
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
        held_put_wide(stage, 0u, cell.first);
        for (size_t k = 0u; k < cell.theta.size(); k += 1u)
        {
            held_put_wide(stage, 1u + (unsigned int)k, cell.theta[k]);
        }
        ok = held_lay(job, stage, error);
    }

    // the pairs: lane point nu + n - 1 over every point of the cell and every n to nu, each pole's record a body
    const unsigned long long pairs = lanes * nu;
    const unsigned int lane_bits = (held_bits_of(pairs) > 0u) ? held_bits_of(pairs) : 1u;
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
        ok = held_lay(job, &phase_stage, error);
        held_pole_records(&phase_stage, cell.first, *cell.cos_bound, cell.poles);
        phase_stage.index = pair_index;
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
                     held_shifted((*cell.k_low)[j].limbs, 2u * w * (cos_terms - 1u - j)), (j % 2u == 0u) ? 1 : -1);
        }
        ok = held_lay(job, &cos_stage, error);
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
        ok = held_lay(job, stage, error);
        held_pole_records(stage, cell.first, *cell.cos_bound, cell.poles);
        stage->index.assign((size_t)(2ull * pairs), 0u);
        for (size_t lane = 0u; lane < (size_t)pairs; lane += 1u)
        {
            stage->index[2u * lane] = (unsigned int)(lane % nu);
            stage->index[2u * lane + 1u] = (unsigned int)lane;
        }
        stage->group = nu;
        stage->summed = {0u};
    }

    // the verdict at every point, its sums record laid by the host from the term sums, each sign-extended to the wider
    unsigned int sum_limbs = 0u;
    unsigned int sum_bits = 0u;
    const unsigned int nu_bits = held_bits_of(nu);
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
        held_put_wide(stage, 0u, cell.verdict);
        ok = held_lay(job, stage, error);
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
        ok = held_lay(job, stage, error);
    }
    HeldStage &verdict_stage = verdict_stages[1];
    // the sign changes between neighboring points, counted by one sum over the cell
    HeldStage change_stage;
    held_open_stage(&change_stage, "change");
    const unsigned long long steps_between = lanes - 1ull;
    if (ok)
    {
        held_change_build(&change_stage, &verdict_stage);
        change_stage.before = &verdict_stage.records;
        change_stage.before_limbs = verdict_stage.layout.out_limbs;
        change_stage.also = &verdict_stage.records;
        change_stage.also_limbs = verdict_stage.layout.out_limbs;
        held_open_shared(&change_stage);
        ok = held_lay(job, &change_stage, error);
        change_stage.index.assign((size_t)(3ull * steps_between), 0u);
        for (unsigned long long lane = 0ull; lane < steps_between; lane += 1ull)
        {
            change_stage.index[(size_t)(3ull * lane + 1ull)] = (unsigned int)lane;
            change_stage.index[(size_t)(3ull * lane + 2ull)] = (unsigned int)(lane + 1ull);
        }
        change_stage.group = steps_between;
        change_stage.summed = {0u};
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
        held_put_wide(&clock_stage, 0u, cell.first);
        ok = held_lay(job, &clock_stage, error);
    }
    const unsigned long long window_low = lanes / 4ull;
    const unsigned long long window_high = 3ull * lanes / 4ull;
    HeldStage turing_stage;
    held_open_stage(&turing_stage, "turing");
    if (ok)
    {
        held_turing_build(&turing_stage, held_bits_of(steps_between) + 1u, window_low, window_high, &change_stage,
                          &clock_stage);
        turing_stage.before = &clock_stage.records;
        turing_stage.before_limbs = clock_stage.layout.out_limbs;
        turing_stage.also = &clock_stage.records;
        turing_stage.also_limbs = clock_stage.layout.out_limbs;
        ok = held_lay(job, &turing_stage, error);
        turing_stage.index = change_stage.index;
        for (unsigned long long lane = 0ull; lane < steps_between; lane += 1ull)
        {
            turing_stage.index[(size_t)(3ull * lane)] = (unsigned int)lane;
        }
        turing_stage.group = steps_between;
        turing_stage.summed = {0u, 1u, 2u, 3u, 4u, 5u, 6u};
    }
    for (unsigned int n = 0u; ok && (n < curves); n += 1u)
    {
        ok = held_sweep(job, &curve_stages[n], lanes, checked, error);
    }
    ok = ok && held_sweep(job, &log_stage, lanes, checked, error);
    ok = ok && held_sweep(job, &theta_stages[0], lanes, checked, error);
    ok = ok && held_sweep(job, &theta_stages[1], lanes, checked, error);
    const unsigned long long pairs_checked = (pairs < 3ull * checked) ? pairs : 3ull * checked;
    ok = ok && held_sweep(job, &phase_stage, pairs, pairs_checked, error);
    ok = ok && held_sweep(job, &cos_stage, pairs, pairs_checked, error);
    ok = ok && held_sweep(job, &term_stages[0], pairs, pairs_checked, error);
    ok = ok && held_sweep(job, &term_stages[1], pairs, pairs_checked, error);
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
    ok = ok && held_sweep(job, &tolerance_stages[0], lanes, checked, error);
    ok = ok && held_sweep(job, &tolerance_stages[1], lanes, checked, error);
    verdict_stages[0].shared = sum_records;
    verdict_stages[1].shared = sum_records;
    ok = ok && held_sweep(job, &verdict_stages[0], lanes, checked, error);
    ok = ok && held_sweep(job, &verdict_stages[1], lanes, checked, error);
    const unsigned long long steps_checked = (steps_between < checked) ? steps_between : checked;
    ok = ok && held_sweep(job, &change_stage, steps_between, steps_checked, error);
    ok = ok && held_sweep(job, &clock_stage, lanes, checked, error);
    turing_stage.shared = change_stage.records;
    ok = ok && held_sweep(job, &turing_stage, steps_between, steps_checked, error);

    // N at points a and e from the device's sums, U^4 at points 0, a, e and the last, pi and ln 2
    HeldStage count_stage;
    held_open_stage(&count_stage, "count");
    if (ok)
    {
        std::vector<HeldWide> row;
        for (unsigned int k = 0u; k < 6u; k += 1u)
        {
            row.push_back(held_wide_sum(&turing_stage, k, 0ull));
        }
        const unsigned long long points[4] = {0ull, window_low, window_high, lanes - 1ull};
        for (unsigned int k = 0u; k < 4u; k += 1u)
        {
            row.push_back(held_wide_out(&clock_stage, points[k], 2u));
        }
        row.push_back(*cell.pi_low);
        row.push_back(*cell.pi_high);
        row.push_back(*cell.two_high);
        held_count_build(&count_stage, held_table({row}, std::vector<unsigned int>(row.size(), 0u)), b, w, cell.precision);
        ok = held_run(job, &count_stage, 1ull, error);
    }

    int same = ok && log_stage.same && theta_stages[0].same && theta_stages[1].same && phase_stage.same && cos_stage.same;
    for (unsigned int n = 0u; n < curves; n += 1u)
    {
        same = same && curve_stages[n].same;
    }
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        same = same && term_stages[side].same && term_stages[side].sums_same && tolerance_stages[side].same &&
               verdict_stages[side].same;
    }
    same = same && change_stage.same && change_stage.sums_same && clock_stage.same && turing_stage.same &&
           turing_stage.sums_same && count_stage.same;
    unsigned long long decided = 0ull;
    long long first_sign = 0;
    long long last_sign = 0;
    long long at_a = 0;
    long long at_e = 0;
    for (unsigned long long lane = 0ull; ok && (lane < lanes); lane += 1ull)
    {
        const long long sign = held_wide_word(held_wide_out(&verdict_stage, lane, 0u));
        decided += (sign != 0) ? 1ull : 0ull;
        first_sign = (lane == 0ull) ? sign : first_sign;
        at_a = (lane == window_low) ? sign : at_a;
        at_e = (lane == window_high) ? sign : at_e;
        last_sign = sign;
    }
    count->checked = same && (at_a != 0) && (at_e != 0);
    count->a = window_low;
    count->e = window_high;
    count->first_sign = first_sign;
    count->last_sign = last_sign;
    count->low = ok ? held_wide_word(held_wide_out(&count_stage, 0ull, 0u)) : 0;
    count->high = ok ? held_wide_word(held_wide_out(&count_stage, 0ull, 1u)) : 0;
    count->before = ok ? held_wide_word(held_wide_sum(&turing_stage, 2u, 0ull)) : 0;
    count->after = ok ? held_wide_word(held_wide_sum(&turing_stage, 5u, 0ull)) : 0;
    count->between = ok ? held_wide_word(held_wide_sum(&turing_stage, 6u, 0ull)) : 0;
    const long long changes = ok ? held_wide_word(held_wide_sum(&change_stage, 0u, 0ull)) : 0;
    const int closed = count->checked && (count->between == count->high - count->low);
    printf("  cell %llu: U from %lld, %llu lanes at 2^-%u; ln u by %u terms; the host's records %s the device's word for "
           "word\n",
           nu, held_wide_word(cell.first), lanes, b, log_terms, same ? "equal" : "differ from");
    printf("  Z's sign decided at %llu of %llu points, %lld sign changes between neighbors; Turing's method: N at point "
           "%llu at least %lld, at point %llu at most %lld\n",
           decided, lanes, changes, count->a, count->low, count->e, count->high);
    printf("  %lld sign changes between them, %lld zeros there: %s\n", count->between, count->high - count->low,
           closed ? "every zero between is on the line and simple" : "the count does not close");
    fflush(stdout);

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
    held_release(&count_stage);
    return ok;
}

// pi below and above at 2^-P by Machin's formula, each arctangent's terms one lane a term, and the 64 bits after the
// point at either end held against pi's
static int held_pi(SimResults *job, unsigned int precision, HeldWide *low, HeldWide *high, EngineError *error)
{
    const unsigned long long ks[2] = {5ull, 239ull};
    // the terms to the count, even, the last below 2^-(P + 8): 5^-2 is below 2^-4.6 and 239^-2 below 2^-15.8
    const unsigned int counts[2] = {2u * (precision / 8u + 2u), 2u * (precision / 30u + 2u)};
    HeldStage arctan[2];
    HeldStage pi;
    held_open_stage(&arctan[0], "arctan_5");
    held_open_stage(&arctan[1], "arctan_239");
    held_open_stage(&pi, "pi");
    int ok = 1;
    std::vector<HeldWide> row;
    for (unsigned int at = 0u; ok && (at < 2u); at += 1u)
    {
        held_machin_build(&arctan[at], ks[at], precision, counts[at]);
        ok = held_run(job, &arctan[at], counts[at] + 1u, error);
        if (ok)
        {
            row.push_back(held_wide_sum(&arctan[at], 0u, 0ull));
            row.push_back(held_wide_sum(&arctan[at], 1u, 0ull));
        }
    }
    if (ok)
    {
        held_pi_build(&pi, held_table({row}, {0u, 0u, 0u, 0u}), precision);
        ok = held_run(job, &pi, 1ull, error);
    }
    const long long lead = 0x243F6A8885A308D3ll;
    const int led = ok && (held_wide_word(held_wide_out(&pi, 0ull, 2u)) == lead) &&
                    (held_wide_word(held_wide_out(&pi, 0ull, 3u)) == lead);
    sim_check(job, led, "pi's 64 bits after the point are 243f6a8885a308d3 at either end");
    if (ok)
    {
        *low = held_wide_out(&pi, 0ull, 0u);
        *high = held_wide_out(&pi, 0ull, 1u);
    }
    held_release(&arctan[0]);
    held_release(&arctan[1]);
    held_release(&pi);
    return ok && led;
}

// Lambda and c_0 to c_(L - 1) for a series of L terms
static int held_lambda_row(SimResults *job, unsigned int terms, std::vector<HeldWide> *row, EngineError *error)
{
    HeldStage stage;
    held_open_stage(&stage, "lambda");
    held_lambda_build(&stage, terms);
    const int ok = held_run(job, &stage, 1ull, error);
    row->clear();
    for (unsigned int k = 0u; ok && (k <= terms); k += 1u)
    {
        row->push_back(held_wide_out(&stage, 0ull, k));
    }
    held_release(&stage);
    return ok;
}

// the lane of C(a, b) among the binomials, row a of the triangle from a (a + 1) / 2
static unsigned int held_triangle(unsigned int a, unsigned int b)
{
    return a * (a + 1u) / 2u + b;
}

// a table whose columns the program that reads it was laid out for
static int held_fixed(SimResults *job, const HeldTable &table, const std::vector<unsigned int> &bits, const char *what)
{
    const int fits = (table.bits == bits);
    sim_check(job, fits, what);
    return fits;
}

// Gabcke's coefficients of C_0 to C_K at 2^-E, `count` a curve, count doubled until the last eight of every curve hold
// 0 between their ends: each coefficient held as its lower end, its spread above it, and the ones past the curve's
// last that does not hold 0 dropped, each within its spread of 0; and s_n, the sum of curve n's spreads. The work is
// at 2^-P_g, P_g = E + 4 (count + 3K) + 64, wide enough for d_k C(i + m, m) pi^(2k - 2n) phi_j's reads
static int held_curves(SimResults *job, unsigned int top, unsigned int big_e, HeldConstants *out, EngineError *error)
{
    int ok = 1;
    int done = 0;
    for (unsigned int count = 64u; ok && !done && (count <= 1024u); count *= 2u)
    {
        const unsigned int precision = big_e + 4u * (count + 3u * top) + 64u;
        const unsigned int factorial_top = count - 1u + 3u * top;
        const unsigned int jmax = factorial_top / 2u;
        const unsigned int row = jmax + 1u;
        HeldStage factorials, binomials, euler, lambda_term, divide, d_stage, powers, eighth, pi_eighths, phi, gamma,
            spread;
        held_open_stage(&factorials, "factorials");
        held_open_stage(&binomials, "binomials");
        held_open_stage(&euler, "euler");
        held_open_stage(&lambda_term, "lambda_term");
        held_open_stage(&divide, "divide");
        held_open_stage(&d_stage, "d");
        held_open_stage(&powers, "pi_powers");
        held_open_stage(&eighth, "eighth");
        held_open_stage(&pi_eighths, "pi_eighths");
        held_open_stage(&phi, "phi");
        held_open_stage(&gamma, "gamma");
        held_open_stage(&spread, "spread");
        HeldWide pi_low;
        HeldWide pi_high;
        ok = held_pi(job, precision, &pi_low, &pi_high, error);
        if (ok)
        {
            held_factorial_build(&factorials, factorial_top);
            ok = held_run(job, &factorials, factorial_top + 1u, error);
        }
        if (ok)
        {
            binomials.before = &factorials.records;
            binomials.before_limbs = factorials.layout.out_limbs;
            binomials.also = &factorials.records;
            binomials.also_limbs = factorials.layout.out_limbs;
            held_binomial_build(&binomials, &factorials, factorial_top);
            binomials.shared = factorials.records;
            for (unsigned int a = 0u; a <= factorial_top; a += 1u)
            {
                for (unsigned int b = 0u; b <= a; b += 1u)
                {
                    binomials.index.push_back(a);
                    binomials.index.push_back(b);
                    binomials.index.push_back(a - b);
                }
            }
            ok = held_run(job, &binomials, held_triangle(factorial_top + 1u, 0u), error);
        }
        // E_0 to E_2jmax, each |E_2n| at most 2 (2n)!, and one width serves the table
        const unsigned int value_bits = ok ? factorials.layout.step_table[factorials.outputs[0]].out_bits + 2u : 0u;
        std::vector<std::vector<HeldWide>> e_rows(1u, std::vector<HeldWide>(1u, held_wide_small(1)));
        HeldTable e_table;
        if (ok)
        {
            held_euler_build(&euler, &binomials, value_bits);
            euler.shared = binomials.records;
            euler.before = &e_table.words;
            euler.before_limbs = (value_bits + 31u) / 32u;
        }
        for (unsigned int n = 1u; ok && (n <= jmax); n += 1u)
        {
            e_table = held_table(e_rows, {value_bits});
            ok = held_fixed(job, e_table, {value_bits}, "every Euler number fits its table");
            euler.index.clear();
            for (unsigned int j = 0u; j < n; j += 1u)
            {
                euler.index.push_back(held_triangle(2u * n, 2u * j));
                euler.index.push_back(j);
            }
            euler.group = n;
            ok = ok && held_run(job, &euler, n, error);
            if (ok)
            {
                e_rows.push_back(std::vector<HeldWide>(1u, held_wide_sum(&euler, 0u, 0ull)));
            }
        }
        // lambda_0 to lambda_((K + 1) / 4)
        const unsigned int lambda_top = (top + 1u) / 4u;
        const unsigned int lambda_bits = 2u + lambda_top * (4u * lambda_top + 3u + value_bits);
        std::vector<std::vector<HeldWide>> lambda_rows(1u, std::vector<HeldWide>(1u, held_wide_small(1)));
        const HeldTable e_only = held_table(e_rows, {value_bits});
        HeldTable lambda_table;
        if (ok && (lambda_top > 0u))
        {
            held_lambda_term_build(&lambda_term, value_bits, lambda_bits, lambda_top);
            lambda_term.shared = e_only.words;
            lambda_term.before = &lambda_table.words;
            lambda_term.before_limbs = (lambda_bits + 31u) / 32u;
        }
        for (unsigned int l = 0u; ok && (l < lambda_top); l += 1u)
        {
            lambda_table = held_table(lambda_rows, {lambda_bits});
            ok = held_fixed(job, lambda_table, {lambda_bits}, "every lambda fits its table");
            lambda_term.index.clear();
            for (unsigned int k = 0u; k <= l; k += 1u)
            {
                lambda_term.index.push_back(k + 1u);
                lambda_term.index.push_back(l - k);
            }
            lambda_term.group = l + 1u;
            ok = ok && held_run(job, &lambda_term, l + 1u, error);
            if (ok)
            {
                const std::vector<unsigned int> pair_bits = {lambda_bits + 8u, 16u};
                const HeldTable pair = held_table({{held_wide_sum(&lambda_term, 0u, 0ull), held_wide_small(l + 1u)}}, pair_bits);
                ok = held_fixed(job, pair, pair_bits, "each lambda's sum fits its table");
                if (ok && (divide.layout.steps == 0u))
                {
                    held_divide_build(&divide, pair);
                }
                divide.shared = pair.words;
                ok = ok && held_run(job, &divide, 1ull, error);
            }
            if (ok)
            {
                lambda_rows.push_back(std::vector<HeldWide>(1u, held_wide_out(&divide, 0ull, 0u)));
            }
        }
        // the rows of d to row K, each entry at most (3n + 2)^2 + 1 times the row before's largest, or a lambda
        unsigned int d_bits = 2u + lambda_bits;
        for (unsigned int n = 0u; n < top; n += 1u)
        {
            d_bits += 2u * held_bits_of(3ull * n + 2ull) + 1u;
        }
        std::vector<std::vector<HeldWide>> d_rows(top + 1u);
        d_rows[0].push_back(held_wide_small(1));
        HeldTable d_row_table;
        const std::vector<unsigned int> d_constant_bits = {lambda_bits, 16u, 16u, 16u, 16u};
        for (unsigned int n = 0u; ok && (n < top); n += 1u)
        {
            const unsigned int length = 3u * n / 4u + 1u;
            const unsigned int next = 3u * (n + 1u) / 4u + 1u;
            std::vector<std::vector<HeldWide>> rows;
            for (size_t k = 0u; k < d_rows[n].size(); k += 1u)
            {
                rows.push_back(std::vector<HeldWide>(1u, d_rows[n][k]));
            }
            d_row_table = held_table(rows, {d_bits});
            const HeldTable constants = held_table({{lambda_rows[(n + 1u) / 4u][0], held_wide_small(3ll * n + 1ll),
                                                     held_wide_small(3ll * n + 2ll), held_wide_small(3ll * n + 3ll),
                                                     held_wide_small(length)}},
                                                   d_constant_bits);
            ok = held_fixed(job, d_row_table, {d_bits}, "every d fits its table") &&
                 held_fixed(job, constants, d_constant_bits, "each row's constants fit their table");
            if (ok && (d_stage.layout.steps == 0u))
            {
                held_d_build(&d_stage, constants, d_bits, 3u * top / 4u + 1u);
                d_stage.before = &d_row_table.words;
                d_stage.before_limbs = d_row_table.limbs;
                d_stage.also = &d_row_table.words;
                d_stage.also_limbs = d_row_table.limbs;
            }
            d_stage.shared = constants.words;
            d_stage.index.clear();
            for (unsigned int k = 0u; k < next; k += 1u)
            {
                d_stage.index.push_back(0u);
                d_stage.index.push_back((k < length) ? k : length - 1u);
                d_stage.index.push_back((k > 0u) ? k - 1u : 0u);
            }
            ok = ok && held_run(job, &d_stage, next, error);
            for (unsigned int k = 0u; ok && (k < next); k += 1u)
            {
                d_rows[n + 1u].push_back(held_wide_out(&d_stage, k, 0u));
            }
        }
        // pi^e for e from -2K to 2 jmax, then pi^e sin(pi / 8) and pi^e cos(pi / 8) for e from 0
        if (ok)
        {
            held_pi_power_build(&powers, held_table({{pi_low, pi_high}}, {0u, 0u}), precision, 2u * top, 2u * jmax);
            ok = held_run(job, &powers, 2u * top + 2u * jmax + 1u, error);
        }
        HeldTable eighth_table;
        if (ok)
        {
            held_eighth_build(&eighth, precision);
            ok = held_run(job, &eighth, 4ull, error);
            int holds = ok;
            std::vector<HeldWide> ends;
            for (unsigned long long lane = 0ull; ok && (lane < 4ull); lane += 1ull)
            {
                holds = holds && (held_wide_word(held_wide_out(&eighth, lane, 1u)) == 1);
                ends.push_back(held_wide_out(&eighth, lane, 0u));
            }
            sim_check(job, holds, "the roots of sin(pi / 8) and cos(pi / 8) hold their definitions");
            ok = ok && holds;
            eighth_table = held_table({ends}, {0u, 0u, 0u, 0u});
        }
        const unsigned int eighths = 2u * (2u * jmax + 1u);
        if (ok)
        {
            pi_eighths.before = &eighth_table.words;
            pi_eighths.before_limbs = eighth_table.limbs;
            held_pi_eighth_build(&pi_eighths, &powers, eighth_table, precision, held_bits_of(eighths) + 1u, 2u * jmax);
            pi_eighths.shared = powers.records;
            for (unsigned int lane = 0u; lane < eighths; lane += 1u)
            {
                pi_eighths.index.push_back(lane / 2u + 2u * top);
                pi_eighths.index.push_back(0u);
            }
            ok = held_run(job, &pi_eighths, eighths, error);
        }
        // phi_0 to phi_jmax
        HeldTable phi_table;
        if (ok)
        {
            std::vector<std::vector<HeldWide>> rows;
            for (unsigned int x = 0u; x <= jmax; x += 1u)
            {
                rows.push_back({e_rows[x][0], held_wide_out(&factorials, 2ull * x, 0u)});
            }
            phi.before = &factorials.records;
            phi.before_limbs = factorials.layout.out_limbs;
            phi.also = &pi_eighths.records;
            phi.also_limbs = pi_eighths.layout.out_limbs;
            held_phi_build(&phi, held_table(rows, {0u, 0u}), &factorials, &pi_eighths, jmax, precision);
            for (unsigned int j = 0u; j < row; j += 1u)
            {
                for (unsigned int m = 0u; m < row; m += 1u)
                {
                    const int valid = (m <= j);
                    phi.index.push_back(valid ? j - m : 0u);
                    phi.index.push_back(m);
                    phi.index.push_back(valid ? 2u * (2u * j - m) + (m % 2u) : 0u);
                }
            }
            ok = held_run(job, &phi, (unsigned long long)row * row, error);
        }
        if (ok)
        {
            std::vector<std::vector<HeldWide>> rows;
            for (unsigned int j = 0u; j < row; j += 1u)
            {
                rows.push_back({held_wide_sum(&phi, 0u, j), held_wide_sum(&phi, 1u, j)});
            }
            phi_table = held_table(rows, {0u, 0u});
        }
        // gamma_(n,i) below and above, lane (n count + i) 8 + k
        if (ok)
        {
            std::vector<std::vector<HeldWide>> rows;
            for (unsigned int n = 0u; n <= top; n += 1u)
            {
                for (unsigned int k = 0u; k < 8u; k += 1u)
                {
                    const unsigned int power = 2u * k + 2u * top - 2u * n;
                    rows.push_back({(k < d_rows[n].size()) ? d_rows[n][k] : held_wide_small(0),
                                    held_wide_out(&powers, power, 0u), held_wide_out(&powers, power, 1u)});
                }
            }
            gamma.before = &binomials.records;
            gamma.before_limbs = binomials.layout.out_limbs;
            gamma.also = &phi_table.words;
            gamma.also_limbs = phi_table.limbs;
            held_gamma_build(&gamma, held_table(rows, {0u, 0u, 0u}), &binomials, phi_table, top, count, precision, big_e);
            for (unsigned int n = 0u; n <= top; n += 1u)
            {
                for (unsigned int i = 0u; i < count; i += 1u)
                {
                    for (unsigned int k = 0u; k < 8u; k += 1u)
                    {
                        const int valid = (4u * k <= 3u * n) && ((i + n) % 2u == 0u);
                        const unsigned int m = valid ? 3u * n - 4u * k : 0u;
                        gamma.index.push_back(n * 8u + k);
                        gamma.index.push_back(valid ? held_triangle(i + m, m) : 0u);
                        gamma.index.push_back(valid ? (i + m) / 2u : 0u);
                    }
                }
            }
            gamma.group = 8u;
            ok = held_run(job, &gamma, (unsigned long long)(top + 1u) * count * 8ull, error);
        }
        std::vector<std::vector<HeldWide>> lows(top + 1u);
        std::vector<std::vector<HeldWide>> highs(top + 1u);
        if (ok)
        {
            std::vector<std::vector<HeldWide>> rows;
            for (unsigned int n = 0u; n <= top; n += 1u)
            {
                for (unsigned int i = 0u; i < count; i += 1u)
                {
                    lows[n].push_back(held_wide_sum(&gamma, 0u, (unsigned long long)n * count + i));
                    highs[n].push_back(held_wide_sum(&gamma, 1u, (unsigned long long)n * count + i));
                    rows.push_back({lows[n][i], highs[n][i]});
                }
            }
            held_spread_build(&spread, held_table(rows, {0u, 0u}), count);
            ok = held_run(job, &spread, (unsigned long long)(top + 1u) * count, error);
        }
        // the last eight of each curve hold 0 between their ends, or the count doubles
        int closes = ok;
        for (unsigned int n = 0u; ok && (n <= top); n += 1u)
        {
            for (unsigned int i = count - 8u; i < count; i += 1u)
            {
                closes = closes && (lows[n][i].sign <= 0) && (highs[n][i].sign >= 0);
            }
        }
        if (ok && closes)
        {
            out->gammas.assign(top + 1u, std::vector<HeldWide>());
            out->spreads.clear();
            for (unsigned int n = 0u; n <= top; n += 1u)
            {
                unsigned int last = 0u;
                for (unsigned int i = 0u; i < count; i += 1u)
                {
                    last = ((lows[n][i].sign > 0) || (highs[n][i].sign < 0)) ? i : last;
                }
                out->gammas[n].assign(lows[n].begin(), lows[n].begin() + last + 1u);
                out->spreads.push_back(held_wide_sum(&spread, 0u, n));
            }
            printf("  C_0 to C_%u: Gabcke's coefficients from the Euler numbers at 2^-%u, %u a curve, held at 2^-%u:",
                   top, precision, count, big_e);
            for (unsigned int n = 0u; n <= top; n += 1u)
            {
                printf(" %zu", out->gammas[n].size());
            }
            printf("\n");
            fflush(stdout);
            done = 1;
        }
        held_release(&factorials);
        held_release(&binomials);
        held_release(&euler);
        held_release(&lambda_term);
        held_release(&divide);
        held_release(&d_stage);
        held_release(&powers);
        held_release(&eighth);
        held_release(&pi_eighths);
        held_release(&phi);
        held_release(&gamma);
        held_release(&spread);
    }
    sim_check(job, done, "the last eight coefficients of every curve hold 0 within 1024 a curve");
    return ok && done;
}

// pi, the logarithms' constants, ln 2, cos's coefficients and bound, theta's constants and Gabcke's curves
static int held_constants(SimResults *job, HeldConstants *k, EngineError *error)
{
    int ok = held_pi(job, k->precision, &k->pi_low, &k->pi_high, error);
    // Lambda and the c_k for each logarithm, a / c at most 1/3 and 9^-L below 2^-P, and ln 2 = 2 artanh(1/3)
    k->log_terms = k->precision / 3u + 4u;
    std::vector<HeldWide> row;
    ok = ok && held_lambda_row(job, k->log_terms, &row, error);
    HeldStage two;
    HeldStage cosine;
    HeldStage theta;
    held_open_stage(&two, "ln_2");
    held_open_stage(&cosine, "cos_constants");
    held_open_stage(&theta, "theta_constants");
    if (ok)
    {
        std::vector<HeldWide> bare = row;
        bare.push_back(held_wide_small(0));
        bare.push_back(held_wide_small(0));
        const HeldTable constants = held_table({bare}, std::vector<unsigned int>(bare.size(), 0u));
        two.before = &constants.words;
        two.before_limbs = constants.limbs;
        held_ln_build(&two, k->precision, held_table({{held_wide_small(1), held_wide_small(3), held_wide_small(0)}}, {0u, 0u, 0u}),
                      constants, k->log_terms, k->precision);
        ok = held_run(job, &two, 1ull, error);
    }
    if (ok)
    {
        row.push_back(held_wide_out(&two, 0ull, 0u));
        row.push_back(held_wide_out(&two, 0ull, 1u));
        k->two_high = held_wide_out(&two, 0ull, 1u);
        k->logs = held_table({row}, std::vector<unsigned int>(row.size(), 0u));
    }
    const HeldTable pi = held_table({{k->pi_low, k->pi_high}}, {0u, 0u});
    const unsigned int cos_top = k->w / 4u + 8u;
    if (ok)
    {
        held_cosine_build(&cosine, pi, k->w, k->precision, cos_top);
        ok = held_run(job, &cosine, 1ull, error);
    }
    unsigned int terms = 0u;
    for (unsigned int j = 1u; ok && (terms == 0u) && (j <= cos_top); j += 1u)
    {
        terms = (held_wide_word(held_wide_out(&cosine, 0ull, 4u * j + 3u)) == 1) ? j : 0u;
    }
    sim_check(job, !ok || (terms > 0u), "cos's rest falls below 2^-W within the terms its program holds");
    ok = ok && (terms > 0u);
    if (ok)
    {
        k->cos_bound = held_wide_out(&cosine, 0ull, 4u * terms + 2u);
        for (unsigned int j = 0u; j < terms; j += 1u)
        {
            k->k_low.push_back(held_wide_out(&cosine, 0ull, 4u * j));
        }
        held_theta_constants_build(&theta, pi, k->w, k->precision);
        ok = held_run(job, &theta, 1ull, error);
    }
    for (unsigned int at = 0u; ok && (at < 7u); at += 1u)
    {
        k->theta.push_back(held_wide_out(&theta, 0ull, at));
    }
    held_release(&two);
    held_release(&cosine);
    held_release(&theta);
    if (ok)
    {
        printf("  pi at 2^-%u by Machin's formula, ln 2 by artanh, cos by %u terms, theta's constants at 2^-%u\n",
               k->precision, terms, k->w);
        fflush(stdout);
    }
    return ok && held_curves(job, HELD_TOP, k->big_e, k, error);
}

// a cell's inputs: each pole's ln n and n^(-1/2), the lattice its rate asks for, U_0, ln(U_0 / 2^b), the log series'
// constants, and the verdict's bound
static int held_inputs(SimResults *job, const HeldConstants &k, unsigned long long nu, unsigned long long rate, HeldCell *cell,
                       EngineError *error)
{
    const unsigned int precision = k.precision;
    const unsigned int w = k.w;
    HeldStage poles;
    HeldStage logs;
    HeldStage lattice;
    HeldStage setup;
    HeldStage small_log;
    HeldStage half;
    HeldStage bound;
    held_open_stage(&poles, "poles");
    held_open_stage(&logs, "pole_logs");
    held_open_stage(&lattice, "rate");
    held_open_stage(&setup, "setup");
    held_open_stage(&small_log, "first_log");
    held_open_stage(&half, "half");
    held_open_stage(&bound, "bound");
    // each pole n to nu + 1: a, c and m for ln n, and n^(-1/2) below and above
    held_poles_build(&poles, w, nu + 1ull);
    int ok = held_run(job, &poles, nu + 1ull, error);
    int holds = ok;
    std::vector<std::vector<HeldWide>> rows;
    for (unsigned long long n = 0ull; ok && (n <= nu); n += 1ull)
    {
        rows.push_back({held_wide_out(&poles, n, 0u), held_wide_out(&poles, n, 1u), held_wide_out(&poles, n, 2u)});
        holds = holds && (held_wide_word(held_wide_out(&poles, n, 5u)) == 1);
    }
    sim_check(job, holds, "each pole's root holds its definition");
    ok = ok && holds;
    if (ok)
    {
        logs.before = &k.logs.words;
        logs.before_limbs = k.logs.limbs;
        held_ln_build(&logs, precision, held_table(rows, {0u, 0u, 0u}), k.logs, k.log_terms, w);
        ok = held_run(job, &logs, nu + 1ull, error);
    }
    if (ok)
    {
        held_rate_build(&lattice, held_table({{held_wide_out(&logs, nu - 1ull, 0u), held_wide_out(&logs, nu, 1u)}}, {0u, 0u}),
                        nu, rate, precision);
        ok = held_run(job, &lattice, 1ull, error);
    }
    const unsigned int b = ok ? (unsigned int)held_wide_word(held_wide_out(&lattice, 0ull, 0u)) : 0u;
    sim_check(job, !ok || ((b >= 4u) && (b <= 40u)), "the rate's lattice is 2^-4 to 2^-40");
    ok = ok && (b >= 4u) && (b <= 40u);
    if (ok)
    {
        held_setup_build(&setup, nu, b);
        ok = held_run(job, &setup, 1ull, error);
        const int rooted = ok && (held_wide_word(held_wide_out(&setup, 0ull, 5u)) == 1);
        sim_check(job, rooted, "the cell's roots hold their definitions");
        ok = ok && rooted;
    }
    // ln(U_0 / 2^b) = (ln nu + 2 artanh(a / c)) / 2, a = U_0^2 - nu 4^b and c = U_0^2 + nu 4^b, a / c below 2^-(b - 1)
    const unsigned int small_terms = ok ? precision / (2u * (b - 1u)) + 2u : 2u;
    std::vector<HeldWide> small_row;
    ok = ok && held_lambda_row(job, small_terms, &small_row, error);
    if (ok)
    {
        small_row.push_back(held_wide_small(0));
        small_row.push_back(held_wide_small(0));
        const HeldTable constants = held_table({small_row}, std::vector<unsigned int>(small_row.size(), 0u));
        small_log.before = &constants.words;
        small_log.before_limbs = constants.limbs;
        held_ln_build(&small_log, precision,
                      held_table({{held_wide_out(&setup, 0ull, 2u), held_wide_out(&setup, 0ull, 3u), held_wide_small(0)}},
                                 {0u, 0u, 0u}),
                      constants, small_terms, w);
        ok = held_run(job, &small_log, 1ull, error);
    }
    if (ok)
    {
        held_half_build(&half,
                        held_table({{held_wide_out(&logs, nu - 1ull, 0u), held_wide_out(&logs, nu - 1ull, 1u),
                                     held_wide_out(&small_log, 0ull, 0u), held_wide_out(&small_log, 0ull, 1u)}},
                                   {0u, 0u, 0u, 0u}),
                        precision, w);
        ok = held_run(job, &half, 1ull, error);
    }
    cell->nu = nu;
    cell->b = b;
    cell->lanes = ok ? (unsigned long long)held_wide_word(held_wide_out(&setup, 0ull, 1u)) : 0ull;
    cell->first = ok ? held_wide_out(&setup, 0ull, 0u) : held_wide_small(0);
    const unsigned long long first = (unsigned long long)held_wide_word(cell->first);
    // A's series takes L terms, its rest below 2^-(W + 4) at the last lane: l / D below 2^-g, g the bits of
    // D = 2 U_0 + l less one less those of l, and the rest below 2^(-g (2L + 1))
    const unsigned int d_bits = held_bits_of(2ull * first + cell->lanes - 1ull);
    const unsigned int l_bits = held_bits_of(cell->lanes - 1ull);
    sim_check(job, !ok || ((cell->lanes > 8ull) && (d_bits > l_bits + 1u)), "the cell's lanes are fewer than U_0");
    ok = ok && (cell->lanes > 8ull) && (d_bits > l_bits + 1u);
    const unsigned int gap = ok ? d_bits - 1u - l_bits : 1u;
    unsigned int terms = 2u;
    while (gap * (2u * terms + 1u) < w + 4u)
    {
        terms += 1u;
    }
    std::vector<HeldWide> row;
    ok = ok && held_lambda_row(job, terms, &row, error);
    cell->c.clear();
    cell->theta.clear();
    cell->poles.clear();
    if (ok)
    {
        cell->c.assign(row.begin() + 1, row.end());
        cell->theta.push_back(held_wide_out(&half, 0ull, 0u));
        cell->theta.push_back(held_wide_out(&half, 0ull, 1u));
        cell->theta.insert(cell->theta.end(), k.theta.begin(), k.theta.end());
        cell->theta.push_back(row[0]);
        cell->theta.push_back(held_wide_small(2ll * terms + 1ll));
        for (unsigned long long n = 0ull; n < nu; n += 1ull)
        {
            cell->poles.push_back(held_wide_out(&logs, n, 2u));
            cell->poles.push_back(held_wide_out(&logs, n, 3u));
            cell->poles.push_back(held_wide_out(&poles, n, 3u));
            cell->poles.push_back(held_wide_out(&poles, n, 4u));
        }
        std::vector<HeldWide> record(1u, k.pi_low);
        record.insert(record.end(), k.spreads.begin(), k.spreads.end());
        held_bound_build(&bound, held_table({record}, std::vector<unsigned int>(record.size(), 0u)), nu,
                         (unsigned int)k.spreads.size() - 1u, w, precision, k.big_e, HELD_GABCKE_D);
        ok = held_run(job, &bound, 1ull, error);
        const int bounded = ok && (held_wide_word(held_wide_out(&bound, 0ull, 3u)) == 1);
        sim_check(job, bounded, "the verdict's roots hold their definitions");
        ok = ok && bounded;
    }
    cell->verdict = ok ? held_wide_out(&bound, 0ull, 0u) : held_wide_small(0);
    cell->big_e = k.big_e;
    cell->w = w;
    cell->precision = precision;
    cell->gammas = &k.gammas;
    cell->cos_bound = &k.cos_bound;
    cell->k_low = &k.k_low;
    cell->pi_low = &k.pi_low;
    cell->pi_high = &k.pi_high;
    cell->two_high = &k.two_high;
    held_release(&poles);
    held_release(&logs);
    held_release(&lattice);
    held_release(&setup);
    held_release(&small_log);
    held_release(&half);
    held_release(&bound);
    return ok;
}

int main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if (count != 6)
    {
        fprintf(stderr, "  usage: exact_zeta_held <first> <last> <E> <W> <rate>\n");
        return 2;
    }
    unsigned long long given[5] = {0ull, 0ull, 0ull, 0ull, 0ull};
    int read = 1;
    for (int at = 0; at < 5; at += 1)
    {
        char *end = NULL;
        given[at] = strtoull(arguments[1 + at], &end, 10);
        read = read && (end != arguments[1 + at]) && (*end == '\0');
    }
    // Trudgian's bound holds past t = 168 pi, from cell 10; E and W are widths the record holds
    read = read && (given[0] >= 10ull) && (given[1] >= given[0]) && (given[1] < (1ull << 16u)) && (given[2] >= 16ull) &&
           (given[2] <= 1024ull) && (given[3] >= 16ull) && (given[3] <= 1024ull) && (given[4] > 0ull) &&
           (given[4] < (1ull << 16u));
    if (!read)
    {
        fprintf(stderr, "  exact_zeta_held: the cells run from 10 on, E and W from 16 to 1024, the rate from 1\n");
        return 2;
    }
    const unsigned long long first_cell = given[0];
    const unsigned long long last_cell = given[1];
    const unsigned long long rate = given[4];
    HeldConstants constants;
    constants.big_e = (unsigned int)given[2];
    constants.w = (unsigned int)given[3];
    constants.precision = ((constants.w > constants.big_e) ? constants.w : constants.big_e) + 64u;
    EngineError error;
    memset(&error, 0, sizeof(error));
    int ok = sim_job_submit(&job, "exact_zeta_held", count, arguments, HELD_DECLARED);
    ok = ok && held_constants(&job, &constants, &error);
    std::vector<HeldCount> cells;
    for (unsigned long long nu = first_cell; ok && (nu <= last_cell); nu += 1ull)
    {
        HeldCell cell;
        ok = held_inputs(&job, constants, nu, rate, &cell, &error);
        HeldCount got = {};
        ok = ok && held_cell(&job, cell, &got, &error);
        cells.push_back(got);
    }
    // each seam: N at cell nu's point e and cell nu + 1's point a against the changes after e, across the seam where
    // the last sign and the next cell's first differ, and before a
    int closed = ok;
    long long total = 0;
    for (size_t at = 0u; ok && (at < cells.size()); at += 1u)
    {
        const HeldCount &got = cells[at];
        closed = closed && got.checked && (got.between == got.high - got.low);
        total += got.between;
        if (at + 1u < cells.size())
        {
            const HeldCount &ahead = cells[at + 1u];
            const long long across = got.after + ((got.last_sign * ahead.first_sign < 0) ? 1 : 0) + ahead.before;
            const int meets = (across == ahead.low - got.high);
            printf("  seam %llu to %llu: %lld sign changes from point %llu to point %llu, N from %lld to %lld: %s\n",
                   first_cell + at, first_cell + at + 1u, across, got.e, ahead.a, got.high, ahead.low,
                   meets ? "they meet" : "they do not meet");
            closed = closed && meets;
            total += across;
        }
    }
    if (ok)
    {
        const long long low = cells.front().low;
        const long long high = cells.back().high;
        closed = closed && (total == high - low);
        printf("  cells %llu to %llu: %lld sign changes from N = %lld to N = %lld: %s\n", first_cell, last_cell, total, low,
               high, closed ? "every zero between is on the line and simple" : "the count does not close");
    }
    sim_check(&job, ok && closed, "every cell closes, every seam meets, and the sign changes number N's rise");
    return sim_close(&job, "exact_zeta_held");
}
