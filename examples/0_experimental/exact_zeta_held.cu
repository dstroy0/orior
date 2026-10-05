// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-035
//
// The point stage of Turing's method in held form, on the lattice in u, t = 2 pi u^4, one lane a point, swept on the
// device as programs of the record machine, one job on the device's tessera daemon. Every value is a mantissa in
// a register and a binary exponent the program holds: a product multiplies the mantissas and adds the exponents, a
// power of two is where a mantissa is laid in the record, and nothing is divided. No value is held at a fixed scale.
//
//   Usage:  exact_zeta_held <input> <output>
//
// Lane l stands at u = U / 2^b, U = U_0 + l, U_0 the least U with U^2 >= nu 4^b, over every U with U^2 < (nu + 1) 4^b.
// Then x = u^2 = U^2 / 4^b, s = u^4, z = 1 - 2 (x - nu) = Zt / 4^b with Zt = (2 nu + 1) 4^b - 2 U^2, and
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
// The input, little-endian: 64-bit words lanes, checked, nu, b, K, E, L, W, then U_0 and (2 nu + 1) 4^b as
// mantissas, then K + 1 words J_n, then the mantissas g_(n,j), then the L constants c_k, then theta's nine held
// constants, Lambda and 2L + 1; a mantissa is a 64-bit word count w, w 32-bit limbs of its magnitude least significant
// first, and a 64-bit sign word, -1, 0 or 1.
//
// The output, a stage at a time, the last curve, the logarithm, and theta / pi below and above: the line "stage name out_limbs L", one line an
// output "output name offset bits exponent", one line a lane of L hex limbs, least significant first, the line
// "host 1" where the host's records over the first `checked` lanes equal the device's word for word, else "host 0",
// and the line "steps P compiled C".

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
    // the records this one reads as member 1, a record a lane, and their width in limbs, or none
    const std::vector<unsigned int> *before;
    unsigned int before_limbs;
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

static void held_output(HeldStage *stage, const std::string &name, unsigned int step, long long exponent)
{
    stage->outputs.push_back(step);
    stage->names.push_back(name);
    stage->exponents.push_back(exponent);
}

// the lane, at most 2^b, and the and with 2^(b + 1) - 1 gives its register that width and keeps its value
static unsigned int held_lane(HeldProgram *program, unsigned int b)
{
    const unsigned int counted = held_step(program, ENGINE_RECORD_LANE, 0u, 0u, 0u);
    return held_step(program, ENGINE_RECORD_AND, counted, held_constant(program, (2ull << b) - 1ull), 0u);
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
// coefficients' widths gamma_bits[j], each laid `lift` bits further up; the field of U_0 `first_bits` wide and that of
// (2 nu + 1) 4^b `lifted_bits` wide. The first curve has no sum before; the last gives s times the sum, the
// denominator U^(2K + 1) and U^2
static void held_build(HeldStage *stage, unsigned int b, unsigned int n, unsigned int top, long long least,
                       unsigned int first_bits, unsigned int lifted_bits, const std::vector<unsigned int> &gamma_bits,
                       unsigned int lift, const HeldStage *before)
{
    HeldProgram *const program = &stage->program;
    program->shared_bits = 0u;
    const unsigned int sign_field = held_field(program, 2u);
    const unsigned int first_field = held_field(program, first_bits);
    const unsigned int lifted_field = held_field(program, lifted_bits);
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
    const unsigned int doubled = held_step(program, ENGINE_RECORD_SUM, square, square, 0u);
    const unsigned int zt = held_step(program, ENGINE_RECORD_DIFFERENCE, held_read(program, lifted_field), doubled, 0u);
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

// nu 2^b into the stage's field `field`

static void held_open_stage(HeldStage *stage, const char *name)
{
    stage->name = name;
    memset(&stage->key, 0, sizeof(stage->key));
    memset(&stage->layout, 0, sizeof(stage->layout));
    stage->record = NULL;
    stage->same = 0;
    stage->before = NULL;
    stage->before_limbs = 0u;
}

// imprints and lays out a stage's program, and says where the imprint stops when it does
static int held_lay(SimResults *job, HeldStage *stage, EngineError *error)
{
    HeldProgram *const program = &stage->program;
    const unsigned int fields = (unsigned int)program->field_bits.size();
    const KeymathRecordRequest encode = {program->steps.data(), (unsigned int)program->steps.size(),
                                         program->field_bits.data(),
                                         fields,
                                         (stage->before != NULL) ? 2u : 1u,
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
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {(program->shared_bits + 31u) / 32u, stage->before_limbs, 0u};
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
    }
    ok = laid;
    sim_check(job, ok, (std::string("the scheduler lays the ") + stage->name + " program out").c_str());
    stage->shared.resize((program->shared_bits + 31u) / 32u);
    return ok;
}

// the bytes a stage holds on the device over `lanes` lanes
static unsigned long long held_declared(const HeldStage *stage, unsigned long long lanes)
{
    return lanes * stage->layout.out_limbs * sizeof(unsigned int) + stage->shared.size() * sizeof(unsigned int) +
           (unsigned long long)stage->layout.steps * sizeof(DeviceRecordStep);
}

// loads a stage onto the device, runs every lane, and holds the host's first `checked` records against the device's
static int held_sweep(SimResults *job, HeldStage *stage, unsigned long long lanes, unsigned long long checked,
                       EngineError *error)
{
    const std::string name(stage->name);
    int ok = cycle_record_load(&stage->layout, &stage->record, error) != CYCLE_ERROR;
    sim_check(job, ok, ("the " + name + " program loads onto the device").c_str());
    const unsigned int out_limbs = stage->layout.out_limbs;
    unsigned int *device_shared = NULL;
    unsigned int *device_before = NULL;
    unsigned int *device_out = NULL;
    const std::vector<unsigned int> *const before = stage->before;
    ok = ok && (cudaMalloc((void **)&device_shared, stage->shared.size() * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, lanes * out_limbs * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_shared, stage->shared.data(), stage->shared.size() * sizeof(unsigned int),
                     cudaMemcpyHostToDevice) == cudaSuccess);
    ok = ok && ((before == NULL) ||
                ((cudaMalloc((void **)&device_before, before->size() * sizeof(unsigned int)) == cudaSuccess) &&
                 (cudaMemcpy(device_before, before->data(), before->size() * sizeof(unsigned int),
                             cudaMemcpyHostToDevice) == cudaSuccess)));
    const unsigned long long bodies = (before != NULL) ? lanes : 0ull;
    const CycleRecordRunRequest run = {stage->record, {device_shared, device_before, NULL}, {1ull, bodies, 0ull}, NULL,
                                       lanes, device_out, error};
    ok = ok && (cycle_record_run(&run) == (long)lanes);
    sim_check(job, ok, ("every lane of the cell runs the " + name + " program on the device").c_str());
    stage->records.assign((size_t)(ok ? lanes * out_limbs : 0ull), 0u);
    ok = ok && (cudaMemcpy(stage->records.data(), device_out, stage->records.size() * sizeof(unsigned int),
                           cudaMemcpyDeviceToHost) == cudaSuccess);
    std::vector<unsigned int> host_out((size_t)(checked * out_limbs));
    const CycleRecordHostRequest host = {&stage->layout, {stage->shared.data(), (before != NULL) ? before->data() : NULL, NULL},
                                         {1ull, bodies, 0ull}, NULL, checked, host_out.data(), error};
    stage->same = ok && (cycle_record_run_host(&host) == (long)checked) &&
                  (memcmp(host_out.data(), stage->records.data(), host_out.size() * sizeof(unsigned int)) == 0);
    sim_check(job, stage->same, ("the host's " + name + " records equal the device's word for word").c_str());
    cudaFree(device_shared);
    cudaFree(device_before);
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
    long long header[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int read = (in != NULL);
    for (unsigned int at = 0u; read && (at < 8u); at += 1u)
    {
        read = held_word(in, &header[at]);
    }
    // the input is read for its form alone: the lane is read at b + 1 bits, and a count is at least what it counts
    read = read && (header[0] > 0) && (header[1] > 0) && (header[1] <= header[0]) && (header[2] > 0) && (header[3] > 0) &&
           (header[3] < 63) && (header[0] <= (2ll << header[3])) && (header[4] >= 0) && (header[5] >= 0) &&
           (header[6] >= 2) && (header[7] > 0);
    const unsigned long long lanes = read ? (unsigned long long)header[0] : 0ull;
    const unsigned long long checked = read ? (unsigned long long)header[1] : 0ull;
    const unsigned long long nu = read ? (unsigned long long)header[2] : 0ull;
    const unsigned int b = read ? (unsigned int)header[3] : 0u;
    const unsigned int curves = read ? (unsigned int)header[4] + 1u : 0u;
    const long long big_e = read ? header[5] : 0;
    const unsigned int log_terms = read ? (unsigned int)header[6] : 0u;
    const unsigned int w = read ? (unsigned int)header[7] : 0u;
    std::vector<unsigned int> first, lifted;
    long long first_sign = 0, lifted_sign = 0;
    unsigned int first_bits = 0u, lifted_bits = 0u;
    read = read && held_mantissa(in, &first, &first_sign, &first_bits) && (first_sign > 0) &&
           held_mantissa(in, &lifted, &lifted_sign, &lifted_bits) && (lifted_sign > 0);
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
        held_build(stage, b, n, curves - 1u, least, first_bits, lifted_bits, gamma_bits[n], lift, before);
        stage->before = (before != NULL) ? &before->records : NULL;
        stage->before_limbs = (before != NULL) ? before->layout.out_limbs : 0u;
        held_open_shared(stage);
        held_put(stage->shared.data(), stage->program.field_offset[0], stage->program.field_bits[0], one_limb,
                 ((nu - 1ull) % 2ull) ? -1 : 1);
        held_put(stage->shared.data(), stage->program.field_offset[1], stage->program.field_bits[1], first, 1);
        held_put(stage->shared.data(), stage->program.field_offset[2], stage->program.field_bits[2], lifted, 1);
        const unsigned int held = (unsigned int)magnitudes[n].size();
        for (unsigned int j = 0u; j < held; j += 1u)
        {
            const std::vector<unsigned int> laid = held_shifted(magnitudes[n][j], 2u * b * (held - 1u - j) + lift);
            held_put(stage->shared.data(), stage->program.field_offset[3u + j], stage->program.field_bits[3u + j], laid,
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

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        held_write(out, &curve_stages[curves - 1u], lanes);
        held_write(out, &log_stage, lanes);
        held_write(out, &theta_stages[0], lanes);
        held_write(out, &theta_stages[1], lanes);
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
    return sim_close(&job, "exact_zeta_held");
}
