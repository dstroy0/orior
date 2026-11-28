// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank.cu: casmi_driver --rank. Each query molecule's train structures, ranked by their reference spectra against the
// molecule's own, read from the parquet files themselves. Every double is its 64 stored bits, m 2^(E - 1075) exactly:
// m its mantissa and E its exponent. Every value is an exact integer on the record machine and every verdict a sign
// the device returned; the host moves record numbers by those signs and does nothing else to a value.
//
//   ion:     I = 10 x sum (molecules x f_k + a_k) x M_k - charge x e, every candidate formula with every query adduct,
//            in 10^-16 Da, the CODATA 2022 electron
//   window:  the query's precursor x = n / d, n its mantissa and d = 2^(1075 - E): D = n |z| 10^16 - I d and N = I d;
//            the formula is inside where D / N is inside the envelope of the query's mode, by two COMPARE signs
//   match:   every query peak q against every peak r of every reference spectrum the window names, each m/z n / d:
//            e = n_q d_r - n_r d_q against n_r d_q by the same two signs, and w = (1 - c_high)(1 + c_low) I_q I_r, each
//            intensity m 2^s on its row's least power of two, s its exponent above the row's least
//   peak:    each query peak's heaviest w by a COMPARE tournament
//   sum:     M = the sum of a pair's retained w; Q and R = the sums of I^2 over each spectrum's peaks
//   score:   (M^2, Q R), put in order by COMPARE(M_a^2 Q_b R_b, M_b^2 Q_a R_a); a row's power of two cancels in it
//   list:    a molecule's structures, the scored by repeated tournaments heaviest first, then the unscored in
//            structure order, the first 25 written
//
// validate reserves the cfg's query library and places each molecule's answer in its list. Test reads the test file
// after the train file, ranks its molecules against every spectrum of the train file and writes their lists' SMILES

#include "../../../../../includes/platform/platform.h"
#include "rank.h"
#include "rank_internal.h"

#include "../../../../../src/cu/engine/engine.h"
#include "../../../../../includes/formats/cfg_json/cfg_json.h"
#include "../mass/mass.h"

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#define RANK_LIST_MAX 25u
#define RANK_MODES 2u
#define RANK_ENVELOPE_FIELDS 4u
#define RANK_ENVELOPE_LIMBS 8u
#define RANK_LOW_D 0u
#define RANK_LOW_N 1u
#define RANK_HIGH_D 2u
#define RANK_HIGH_N 3u
#define RANK_TOKENS_MAX 1024u
#define RANK_PATH_BYTES 1024u
#define RANK_TERMS_MAX (2u * CASMI_FORMULA_ELEMENTS_MAX)
#define RANK_ADDUCTS_MAX 64u

// 10^-16 Da in one dalton, the ion's unit
#define RANK_PER_DALTON 10000000000000000ull

// the device bytes the job declares: the widest sweep's records, members and index
#define RANK_DECLARED (2ull << 30u)

// the max a chunk of match lanes stores: on the device its index, records and order, 52 bytes a lane at the match's
// record of 8 limbs
#define RANK_CHUNK_LANES (1ull << 24u)

// a double's fields as IEEE 754 binary64 positions them, and the exponent its integer mantissa is at: x = m 2^(E -
// 1075), E the stored exponent, 1 for a subnormal, and m its fraction with the leading 1 where E is stored above 0. A
// record stores E in RANK_EXPONENT_BITS bits
#define RANK_MANTISSA_BITS 52u
#define RANK_EXPONENT_MASK 0x7FFull
#define RANK_UNIT_EXPONENT 1075u
#define RANK_EXPONENT_BITS 12u
#define RANK_SIGN_BIT 63u


typedef struct
{
    OriorExactInteger part[RANK_ENVELOPE_FIELDS];
} RankEnvelope;

// one isotope of an ion: the formula's and the adduct's counts of it
typedef struct
{
    unsigned int isotope;
    long long formula;
    long long adduct;
} RankTerm;

// groups of record numbers: group g contains entry[first[g] .. first[g] + count[g])
typedef struct
{
    const unsigned long long *first;
    const unsigned long long *count;
    const unsigned int *entry;
    unsigned long long group_count;
} RankGroups;

// the larger of two counts
static unsigned int rank_larger(unsigned int one, unsigned int other)
{
    return (one > other) ? one : other;
}

// the smaller of two counts
static unsigned int rank_smaller(unsigned int one, unsigned int other)
{
    return (one < other) ? one : other;
}

// the order of two record numbers, for memor.sort
static int rank_number_order(const void *left, const void *right)
{
    const unsigned int one = *(const unsigned int *)left;
    const unsigned int other = *(const unsigned int *)right;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

// 1 where `wanted` is among the `count` sorted numbers at `numbers`
static int rank_number_found(const unsigned int *numbers, unsigned long long count, unsigned int wanted)
{
    unsigned long long low = 0ull;
    unsigned long long high = count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        low = (numbers[middle] < wanted) ? (middle + 1ull) : low;
        high = (numbers[middle] < wanted) ? high : middle;
    }
    return (low < count) && (numbers[low] == wanted);
}

// the `count` cells at `cells` given back
static void rank_cells_given(void *const *cells, unsigned int count)
{
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        rank_cell_released(cells[each]);
    }
}

// ---- reading ----

static int rank_hex_read(const char *text, OriorExactInteger *value)
{
    orior_exact_zero(value);
    const char *at = text;
    const int negative = (*at == '-');
    at += negative ? 1 : 0;
    if ((at[0] != '0') || (at[1] != 'x'))
    {
        return 0;
    }
    at += 2;
    const size_t digits = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((digits == 0u) || (digits > (RANK_ENVELOPE_LIMBS * 8u)))
    {
        return 0;
    }
    for (size_t digit = 0u; digit < digits; digit += 1u)
    {
        // Explicit casts read the byte as its code point, and the folded letter as a count from 'a'
        const uint8_t digit_char = (uint8_t)at[digits - 1u - digit];
        const int nibble = !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_HEX, .byte = digit_char) ? -1
                           : EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = digit_char)
                               ? (int)(digit_char - '0')
                               : 10 + (int)(EMBED_CALL(ascii.lower, AsciiCfg, .byte = digit_char) - 'a');
        if (nibble < 0)
        {
            return 0;
        }
        value->limb[digit / 8u] |= (unsigned int)nibble << (4u * (unsigned int)(digit % 8u));
    }
    value->sign = 0;
    for (unsigned int limb = 0u; limb < RANK_ENVELOPE_LIMBS; limb += 1u)
    {
        value->sign = (value->limb[limb] != 0u) ? 1 : value->sign;
    }
    value->sign = negative ? -value->sign : value->sign;
    return 1;
}

static int rank_cfg_string(const char *text, const CfgJsonToken *tokens, unsigned int block, const char *name, char *out)
{
    const unsigned int value = (block != 0u) ? cfg_json_member(text, tokens, block, name) : 0u;
    return (value != 0u) && cfg_json_string(text, &tokens[value], out, RANK_PATH_BYTES);
}

static unsigned int rank_terms_merge(const CasmiFormula *formula, const CasmiAdduct *adduct, RankTerm *term)
{
    unsigned int count = 0u;
    for (unsigned int index = 0u; index < (formula->element_count + adduct->terms.element_count); index += 1u)
    {
        const int from_formula = index < formula->element_count;
        const unsigned int at = from_formula ? index : (index - formula->element_count);
        const unsigned int named = from_formula ? formula->isotope[at] : adduct->terms.isotope[at];
        const long long term_count = from_formula ? formula->count[at] : adduct->terms.count[at];
        unsigned int entry = 0u;
        while ((entry < count) && (term[entry].isotope != named))
        {
            entry += 1u;
        }
        if (entry == count)
        {
            term[entry].isotope = named;
            term[entry].formula = 0ll;
            term[entry].adduct = 0ll;
            count += 1u;
        }
        term[entry].formula += from_formula ? term_count : 0ll;
        term[entry].adduct += from_formula ? 0ll : term_count;
    }
    return count;
}

static unsigned int rank_magnitude_bits(long long value)
{
    return exact_record_bits_of((value < 0ll) ? (unsigned long long)(-value) : (unsigned long long)value);
}

static void rank_exact_signed(long long value, OriorExactInteger *out)
{
    rank_exact_of((value < 0ll) ? (unsigned long long)(-value) : (unsigned long long)value, out);
    out->sign = (value < 0ll) ? -out->sign : out->sign;
}

// The match's inside is (1 - high)(1 + low) of two signs of -1 or +1: 4 where the peaks match and 0 elsewhere. A
// retained weight is 4 I_q I_r and M^2 over Q R is 16 cos^2 on every pair alike
#define RANK_INSIDE_SQUARED 16ull

// a pair's cosine squared in parts per million, floored: 10^6 M^2 over 16 Q R, exact; ~0 where it does not fit
static unsigned long long rank_score_ppm(const unsigned int *record, RankField numerator, RankField denominator)
{
    OriorExactInteger top;
    OriorExactInteger bottom;
    OriorExactInteger million;
    OriorExactInteger scaled;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    rank_field_read(record, numerator, &top);
    rank_field_read(record, denominator, &bottom);
    rank_exact_of(1000000ull / RANK_INSIDE_SQUARED, &million);
    if ((orior_exact_multiply(&top, &million, &scaled) != ORIOR_EXACT_OK) ||
        (orior_exact_divide(&scaled, &bottom, &quotient, &remainder) != ORIOR_EXACT_OK))
    {
        return ~0ull;
    }
    return ((unsigned long long)quotient.limb[1] << 32u) | quotient.limb[0];
}

// ---- a stored double ----

// A double's 64 stored bits as its mantissa m and exponent E, x = m 2^(E - 1075); a zero is m = 0 at E = 1075, over 1
static void rank_double(unsigned long long stored, unsigned long long *mantissa, unsigned int *exponent)
{
    const unsigned long long biased = (stored >> RANK_MANTISSA_BITS) & RANK_EXPONENT_MASK;
    const unsigned long long fraction = stored & ((1ull << RANK_MANTISSA_BITS) - 1ull);
    *mantissa = fraction | ((biased != 0ull) ? (1ull << RANK_MANTISSA_BITS) : 0ull);
    *exponent = (*mantissa == 0ull) ? RANK_UNIT_EXPONENT : (unsigned int)((biased != 0ull) ? biased : 1ull);
}

// ---- programs ----

// sign(a - b) as a register: 1, 0 or -1
static unsigned int rank_compare(ExactRecordProgram *program, unsigned int a, unsigned int b)
{
    return exact_record_difference(program, exact_record_above(program, a, b), exact_record_above(program, b, a));
}

// a double's denominator 2^(1075 - E), 1075 - E known below 2^two_bits
static unsigned int rank_denominator_steps(ExactRecordProgram *program, unsigned int exponent, unsigned int two_bits)
{
    return exact_record_two_to(
        program, exact_record_difference(program, exact_record_constant(program, RANK_UNIT_EXPONENT), exponent),
        two_bits);
}


// The envelope's two signs for the ratio e / n, n > 0, the envelope's fields read from member `member`:
// c_high = COMPARE(2 e N_high, 2 D_high n + 1) and c_low = COMPARE(2 e N_low, 2 D_low n - 1). Neither side of either is
// ever equal to the other. Each sign is -1 or +1, and e / n is inside [D_low / N_low, D_high / N_high] exactly where
// c_high = -1 and c_low = +1
static void rank_envelope_steps(ExactRecordProgram *program, const unsigned int *fields, unsigned int member,
                                unsigned int error, unsigned int reference, unsigned int *high, unsigned int *low)
{
    const unsigned int low_error = exact_record_read(program, fields[RANK_LOW_D], member);
    const unsigned int low_mass = exact_record_read(program, fields[RANK_LOW_N], member);
    const unsigned int high_error = exact_record_read(program, fields[RANK_HIGH_D], member);
    const unsigned int high_mass = exact_record_read(program, fields[RANK_HIGH_N], member);
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    *high = rank_compare(program, exact_record_product(program, two, exact_record_product(program, error, high_mass)),
                         exact_record_sum(program,
                                          exact_record_product(program, two,
                                                               exact_record_product(program, high_error, reference)),
                                          one));
    *low = rank_compare(program, exact_record_product(program, two, exact_record_product(program, error, low_mass)),
                        exact_record_difference(program,
                                                exact_record_product(program, two,
                                                                     exact_record_product(program, low_error, reference)),
                                                one));
}

// the envelope's fields, their offsets and widths in its record
typedef struct
{
    unsigned int offset[RANK_ENVELOPE_FIELDS];
    unsigned int bits[RANK_ENVELOPE_FIELDS];
    unsigned int limbs;
} RankEnvelopeLayout;

static void rank_envelope_fields(ExactRecordProgram *program, const RankEnvelopeLayout *envelope, unsigned int *fields)
{
    for (unsigned int part = 0u; part < RANK_ENVELOPE_FIELDS; part += 1u)
    {
        fields[part] = exact_record_member_field(program, envelope->bits[part], envelope->offset[part]);
    }
}

// A peak's record: its m/z's mantissa at bit 0 and exponent at exponent_offset, and its intensity's mantissa at
// intensity_offset and shift at shift_offset, the intensity's exponent less its row's least: each row's intensities
// on the row's least power of two, a scale every score cancels
typedef struct
{
    unsigned int exponent_offset;
    unsigned int intensity_offset;
    unsigned int shift_offset;
    unsigned int shift_bits;
    unsigned int limbs;
} RankPeakLayout;

// a mantissa's bits, the leading 1 above the 52 stored
#define RANK_MANTISSA_FIELD_BITS 53u

// ---- groups on the device ----

// The sum program: three records' one field, summed
static int rank_sum_position(SimResults *results, RankField value, unsigned int limbs, RankMachine *machine)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int field = exact_record_member_field(&program, value.bits, value.offset);
    unsigned int sum = exact_record_read(&program, field, 0u);
    sum = exact_record_sum(&program, sum, exact_record_read(&program, field, 1u));
    sum = exact_record_sum(&program, sum, exact_record_read(&program, field, 2u));
    const unsigned int member_limbs[ENGINE_RECORD_MEMBERS_MAX] = {limbs, limbs, limbs};
    const int ok = rank_machine_load(results, "sum", &program, &sum, 1u, 3u, member_limbs, machine);
    exact_record_close(&program);
    return ok;
}

// Sums each group's records on the device, three to a lane, pass after pass, until each group is one record. Every
// group takes every pass, zero records filling a short lane; every sum ends in one layout. A pass positions its lanes'
// records as three planes, lane l's three records record l of each: the sweep reads them with no index. *summed
// stores group g's sum at g, then a zero record, a cell of the persistent tier that the caller gives back
static int rank_group_sum(SimResults *results, const char *name, RankField value, unsigned int value_limbs,
                          const unsigned int *records, unsigned long long record_count, const RankGroups *groups,
                          unsigned int **summed, RankField *summed_field, unsigned int *summed_limbs,
                          unsigned long long *microseconds)
{
    (void)record_count;
    unsigned long long entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        entry_total += groups->count[group];
    }
    // the groups' entries, firsts and counts, cells of the temporary tier that the call gives back. Each pass's sums
    // are a cell of the persistent tier, and the last of them goes to the caller
    unsigned int *current = NULL;
    const unsigned int *source = records;
    unsigned int *const entry = (unsigned int *)rank_cell(entry_total + 1ull, sizeof(unsigned int), 1);
    unsigned long long *const first =
        (unsigned long long *)rank_cell(groups->group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const count =
        (unsigned long long *)rank_cell(groups->group_count + 1ull, sizeof(unsigned long long), 1);
    void *const work[3] = {count, first, entry};
    if (rank_cells_refused(results))
    {
        rank_cells_given(work, 3u);
        return 0;
    }
    unsigned long long widest = 0ull;
    entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        first[group] = entry_total;
        count[group] = groups->count[group];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &entry[entry_total], .src = &groups->entry[groups->first[group]],
                   .bytes = (size_t)(count[group] * sizeof(unsigned int)));
        entry_total += count[group];
        widest = (count[group] > widest) ? count[group] : widest;
    }
    RankField field = value;
    unsigned int limbs = value_limbs;
    unsigned int passes = 0u;
    while ((passes == 0u) || (widest > 1ull))
    {
        unsigned long long lanes = 0ull;
        for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
        {
            const unsigned long long group_lanes = (count[group] + 2ull) / 3ull;
            lanes += (group_lanes == 0ull) ? 1ull : group_lanes;
        }
        RankMachine sum;
        if (!rank_sum_position(results, field, limbs, &sum))
        {
            rank_cell_released(current);
            rank_cells_given(work, 3u);
            return 0;
        }
        // the pass's three planes, cells of the temporary tier that the pass gives back
        unsigned int *const plane[3] = {(unsigned int *)rank_cell(lanes * limbs, sizeof(unsigned int), 1),
                                        (unsigned int *)rank_cell(lanes * limbs, sizeof(unsigned int), 1),
                                        (unsigned int *)rank_cell(lanes * limbs, sizeof(unsigned int), 1)};
        void *const planes[3] = {plane[2], plane[1], plane[0]};
        if (rank_cells_refused(results))
        {
            rank_cells_given(planes, 3u);
            rank_machine_release(&sum);
            rank_cell_released(current);
            rank_cells_given(work, 3u);
            return 0;
        }
        unsigned long long lane = 0ull;
        widest = 0ull;
        for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
        {
            const unsigned long long counted = (count[group] + 2ull) / 3ull;
            const unsigned long long group_lanes = (counted == 0ull) ? 1ull : counted;
            for (unsigned long long at = 0ull; at < count[group]; at += 1ull)
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &plane[at % 3ull][(lane + (at / 3ull)) * limbs],
                           .src = &source[(unsigned long long)entry[first[group] + at] * limbs],
                           .bytes = limbs * sizeof(unsigned int));
            }
            for (unsigned long long each = 0ull; (each < group_lanes) && (each < count[group]); each += 1ull)
            {
                entry[first[group] + each] = (unsigned int)(lane + each);
            }
            count[group] = (count[group] == 0ull) ? 0ull : group_lanes;
            widest = (count[group] > widest) ? count[group] : widest;
            lane += group_lanes;
        }
        const unsigned int out_limbs = sum.layout.out_limbs;
        unsigned int *const next = (unsigned int *)rank_cell((lanes + 1ull) * out_limbs, sizeof(unsigned int), 0);
        const unsigned int *const members[ENGINE_RECORD_MEMBERS_MAX] = {plane[0], plane[1], plane[2]};
        const unsigned long long member_bodies[ENGINE_RECORD_MEMBERS_MAX] = {lanes, lanes, lanes};
        const int swept =
            (next != NULL) && rank_sweep(results, name, &sum, members, member_bodies, NULL, lanes, next, microseconds);
        rank_cells_given(planes, 3u);
        if (!swept)
        {
            (void)rank_cells_refused(results);
            rank_cell_released(next);
            rank_machine_release(&sum);
            rank_cell_released(current);
            rank_cells_given(work, 3u);
            return 0;
        }
        field = rank_output(&sum, 0u);
        limbs = out_limbs;
        rank_machine_release(&sum);
        rank_cell_released(current);
        current = next;
        source = current;
        passes += 1u;
    }
    rank_cells_given(work, 3u);
    *summed = current;
    *summed_field = field;
    *summed_limbs = limbs;
    return 1;
}

// Each group's heaviest record by a COMPARE tournament on the device: the entries in pairs, the one the order program
// calls heavier going on, the first where the two are equal, until one is left. winner[g] is a record number,
// RANK_ABSENT for an empty group
static int rank_group_best(SimResults *results, const char *name, const RankMachine *order, RankField sign_field,
                           const unsigned int *records, unsigned long long record_count, const RankGroups *groups,
                           unsigned int *winner, unsigned long long *microseconds)
{
    unsigned long long entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        entry_total += groups->count[group];
    }
    // the groups' entries, firsts and counts, the pairs' index and their signs, cells of the temporary tier that the
    // call gives back
    const unsigned int out_limbs = order->layout.out_limbs;
    unsigned int *const entry = (unsigned int *)rank_cell(entry_total + 1ull, sizeof(unsigned int), 1);
    unsigned long long *const first =
        (unsigned long long *)rank_cell(groups->group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const count =
        (unsigned long long *)rank_cell(groups->group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned int *const index = (unsigned int *)rank_cell(entry_total + 2ull, sizeof(unsigned int), 1);
    unsigned int *const signs =
        (unsigned int *)rank_cell(((entry_total / 2ull) + 1ull) * out_limbs, sizeof(unsigned int), 1);
    void *const work[5] = {signs, index, count, first, entry};
    if (rank_cells_refused(results))
    {
        rank_cells_given(work, 5u);
        return 0;
    }
    entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        first[group] = entry_total;
        count[group] = groups->count[group];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &entry[entry_total], .src = &groups->entry[groups->first[group]],
                   .bytes = (size_t)(count[group] * sizeof(unsigned int)));
        entry_total += count[group];
    }
    for (;;)
    {
        unsigned long long lanes = 0ull;
        for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
        {
            for (unsigned long long pair = 0ull; pair < (count[group] / 2ull); pair += 1ull)
            {
                index[2ull * lanes] = entry[first[group] + (2ull * pair)];
                index[(2ull * lanes) + 1ull] = entry[first[group] + (2ull * pair) + 1ull];
                lanes += 1ull;
            }
        }
        if (lanes == 0ull)
        {
            break;
        }
        const unsigned int *const members[ENGINE_RECORD_MEMBERS_MAX] = {records, records, NULL};
        const unsigned long long member_bodies[ENGINE_RECORD_MEMBERS_MAX] = {record_count, record_count, 0ull};
        if (!rank_sweep(results, name, order, members, member_bodies, index, lanes, signs, microseconds))
        {
            rank_cells_given(work, 5u);
            return 0;
        }
        unsigned long long lane = 0ull;
        for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
        {
            const unsigned long long pairs = count[group] / 2ull;
            for (unsigned long long pair = 0ull; pair < pairs; pair += 1ull)
            {
                const int sign = rank_field_sign(&signs[lane * out_limbs], sign_field);
                entry[first[group] + pair] = (sign >= 0) ? index[2ull * lane] : index[(2ull * lane) + 1ull];
                lane += 1ull;
            }
            if ((count[group] % 2ull) != 0ull)
            {
                entry[first[group] + pairs] = entry[first[group] + count[group] - 1ull];
            }
            count[group] = pairs + (count[group] % 2ull);
        }
    }
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        winner[group] = (count[group] == 0ull) ? RANK_ABSENT : entry[first[group]];
    }
    rank_cells_given(work, 5u);
    return 1;
}

// the value order program: COMPARE(v_a, v_b), one field read from two records
static int rank_value_order_position(SimResults *results, RankField value, unsigned int limbs, RankMachine *machine)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int field = exact_record_member_field(&program, value.bits, value.offset);
    unsigned int sign =
        rank_compare(&program, exact_record_read(&program, field, 0u), exact_record_read(&program, field, 1u));
    const unsigned int member_limbs[ENGINE_RECORD_MEMBERS_MAX] = {limbs, limbs, 0u};
    const int ok = rank_machine_load(results, "value order", &program, &sign, 1u, 2u, member_limbs, machine);
    exact_record_close(&program);
    return ok;
}

// the ratio order program: COMPARE(n_a d_b, n_b d_a), the pair (n, d) read from two records
static int rank_ratio_order_position(SimResults *results, RankField numerator, RankField denominator, unsigned int limbs,
                                RankMachine *machine)
{
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int top = exact_record_member_field(&program, numerator.bits, numerator.offset);
    const unsigned int bottom = exact_record_member_field(&program, denominator.bits, denominator.offset);
    const unsigned int left = exact_record_product(&program, exact_record_read(&program, top, 0u),
                                                   exact_record_read(&program, bottom, 1u));
    const unsigned int right = exact_record_product(&program, exact_record_read(&program, top, 1u),
                                                    exact_record_read(&program, bottom, 0u));
    unsigned int sign = rank_compare(&program, left, right);
    const unsigned int member_limbs[ENGINE_RECORD_MEMBERS_MAX] = {limbs, limbs, 0u};
    const int ok = rank_machine_load(results, "score order", &program, &sign, 1u, 2u, member_limbs, machine);
    exact_record_close(&program);
    return ok;
}

// ---- the run ----

typedef struct
{
    unsigned long long lanes;
    unsigned long long microseconds;
} RankTally;

// the clock as the run starts, each stage's line reading its end against it
static unsigned long long s_rank_started;

static void rank_tally_line(SimResults *results, const char *name, const RankTally *tally)
{
    mmgr_line_put(&results->line, "  ");
    mmgr_line_put(&results->line, name);
    rank_line_decimal(results, ": ", tally->lanes);
    rank_line_decimal(results, " lanes, device ", tally->microseconds);
    rank_line_decimal(results, " us, ended at ", engine_clock_microseconds() - s_rank_started);
    mmgr_line_put(&results->line, " us of the run");
    rank_line_end(results);
}

int rank_run(SimResults *results, int count, char **arguments)
{
    s_rank_started = engine_clock_microseconds();
    const char *const train_path = arguments[2];
    const char *const cfg_path = arguments[3];
    const int validate =
        (count == 5) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "validate", .ci = EMBED_FALSE));
    const int test =
        (count == 6) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[4], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "test", .ci = EMBED_FALSE));
    if (!validate && !test)
    {
        mmgr_line_put(&results->line, "  --rank TRAIN.parquet CFG validate, or --rank TRAIN.parquet CFG test TEST.parquet");
        rank_line_end(results);
        return 0;
    }
    // the cfg: the envelope and the query library
    FILE *const cfg_file = fopen(cfg_path, "rb");
    static char cfg_text[1u << 16u];
    const size_t cfg_length = (cfg_file != NULL) ? fread(cfg_text, 1u, sizeof(cfg_text) - 1u, cfg_file) : 0u;
    if (cfg_file != NULL)
    {
        fclose(cfg_file);
    }
    static CfgJsonToken tokens[RANK_TOKENS_MAX];
    CfgJsonParse parse;
    if ((cfg_length == 0u) || !cfg_json_parse(cfg_text, cfg_length, tokens, RANK_TOKENS_MAX, &parse))
    {
        mmgr_line_put(&results->line, "  the cfg did not read: ");
        mmgr_line_put(&results->line, cfg_path);
        rank_line_end(results);
        return 0;
    }
    static const char *const mode_name[RANK_MODES] = {"positive", "negative"};
    static const char *const envelope_name[RANK_ENVELOPE_FIELDS] = {"low_D", "low_N", "high_D", "high_N"};
    const unsigned int envelope_block = cfg_json_member(cfg_text, tokens, 0u, "envelope");
    const unsigned int validate_block = cfg_json_member(cfg_text, tokens, 0u, "validate");
    const unsigned int output_block = cfg_json_member(cfg_text, tokens, 0u, "output");
    char query_library[RANK_PATH_BYTES];
    char submission_path[RANK_PATH_BYTES];
    int ok = rank_cfg_string(cfg_text, tokens, validate_block, "query_library", query_library) &&
             rank_cfg_string(cfg_text, tokens, output_block, "submission", submission_path);
    RankEnvelope envelope[RANK_MODES];
    RankEnvelopeLayout envelope_layout;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &envelope_layout, .bytes = sizeof(envelope_layout), .val = 0);
    for (unsigned int mode = 0u; ok && (mode < RANK_MODES); mode += 1u)
    {
        const unsigned int mode_block = cfg_json_member(cfg_text, tokens, envelope_block, mode_name[mode]);
        for (unsigned int part = 0u; ok && (part < RANK_ENVELOPE_FIELDS); part += 1u)
        {
            char hex[RANK_PATH_BYTES];
            ok = rank_cfg_string(cfg_text, tokens, mode_block, envelope_name[part], hex) &&
                 rank_hex_read(hex, &envelope[mode].part[part]);
            const unsigned int bits = rank_bits(&envelope[mode].part[part]) + 1u;
            envelope_layout.bits[part] = (bits > envelope_layout.bits[part]) ? bits : envelope_layout.bits[part];
        }
        ok = ok && (envelope[mode].part[RANK_LOW_N].sign > 0) && (envelope[mode].part[RANK_HIGH_N].sign > 0);
    }
    if (!ok)
    {
        mmgr_line_put(&results->line,
                       "  the cfg lacks validate.query_library, output.submission or an envelope with a positive N");
        rank_line_end(results);
        return 0;
    }
    unsigned int envelope_at = 0u;
    for (unsigned int part = 0u; part < RANK_ENVELOPE_FIELDS; part += 1u)
    {
        envelope_layout.offset[part] = envelope_at;
        envelope_at += envelope_layout.bits[part];
    }
    envelope_layout.limbs = (envelope_at + 31u) / 32u;
    // The envelopes' records, a cell of the temporary tier. Every table of the run below is one too, and the run gives
    // them back as it ends, except the lists that grow, which rank_host_room keeps on the persistent tier
    const unsigned long long envelope_words = (unsigned long long)RANK_MODES * envelope_layout.limbs;
    unsigned int *const envelope_records = (unsigned int *)rank_cell(envelope_words, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned int mode = 0u; mode < RANK_MODES; mode += 1u)
    {
        for (unsigned int part = 0u; part < RANK_ENVELOPE_FIELDS; part += 1u)
        {
            rank_field_put(&envelope_records[mode * envelope_layout.limbs], envelope_layout.offset[part],
                           envelope_layout.bits[part], &envelope[mode].part[part]);
        }
    }

    // the train file, and in test the test file read after it, whose spectra are the queries
    static RankSet set;
    const char *const paths[RANK_FILES] = {train_path, test ? arguments[5] : NULL};
    if (!rank_set_load(results, paths, test ? 2u : 1u, &set))
    {
        return 0;
    }
    const unsigned long long set_spectra = set.file_spectra[0];
    const unsigned long long spectra = set.spectra;
    if (test)
    {
        rank_line_decimal(results, "  test file: ", spectra - set_spectra);
        mmgr_line_put(&results->line, " spectra");
        rank_line_end(results);
    }
    rank_line_decimal(results, "  read: ", spectra);
    rank_line_decimal(results, " spectra in ", (unsigned long long)set.row_groups);
    rank_line_decimal(results, " row groups, ", set.mz.values);
    rank_line_decimal(results, " peaks, ", (unsigned long long)set.strings[RANK_KEY].count);
    rank_line_decimal(results, " structures, by ", engine_clock_microseconds() - s_rank_started);
    mmgr_line_put(&results->line, " us of the run");
    rank_line_end(results);

    // The bounds the programs are arranged by, from the stored bits: the max 1075 - E a precursor or an m/z takes, and
    // each row's least intensity exponent and the max any intensity is above its row's. Every value is above 0 or
    // 0, and below 2^53
    unsigned int two_max = 0u;
    int signs_agree = 1;
    for (int column = 0; column < 2; column += 1)
    {
        const RankColumn *const stored = (column == 0) ? &set.precursor : &set.mz;
        const unsigned long long *const bits = stored->bits;
        for (unsigned long long value = 0ull; value < stored->values; value += 1ull)
        {
            unsigned long long mantissa = 0ull;
            unsigned int exponent = 0u;
            rank_double(bits[value], &mantissa, &exponent);
            signs_agree = signs_agree && ((bits[value] >> RANK_SIGN_BIT) == 0ull) && (exponent <= RANK_UNIT_EXPONENT);
            two_max = rank_larger(two_max, RANK_UNIT_EXPONENT - rank_smaller(exponent, RANK_UNIT_EXPONENT));
        }
    }
    unsigned int *const intensity_floor = (unsigned int *)rank_cell(spectra + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long spectrum = 0ull; spectrum <= spectra; spectrum += 1ull)
    {
        intensity_floor[spectrum] = RANK_UNIT_EXPONENT;
    }
    unsigned int shift_max = 0u;
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        unsigned int highest = 0u;
        for (unsigned long long value = set.intensity.row_start[spectrum];
             value < set.intensity.row_start[spectrum + 1ull]; value += 1ull)
        {
            unsigned long long mantissa = 0ull;
            unsigned int exponent = 0u;
            rank_double(set.intensity.bits[value], &mantissa, &exponent);
            signs_agree = signs_agree && ((set.intensity.bits[value] >> RANK_SIGN_BIT) == 0ull);
            if (mantissa != 0ull)
            {
                intensity_floor[spectrum] = rank_smaller(intensity_floor[spectrum], exponent);
                highest = rank_larger(highest, exponent);
            }
        }
        shift_max =
            rank_larger(shift_max, (highest > intensity_floor[spectrum]) ? (highest - intensity_floor[spectrum]) : 0u);
    }
    const unsigned int two_bits = rank_larger(1u, exact_record_bits_of(two_max));
    const unsigned int shift_bits = rank_larger(1u, exact_record_bits_of(shift_max));
    rank_line_decimal(results, "  bounds: 1075 - E at max ", two_max);
    rank_line_decimal(results, ", an intensity's exponent above its row's least at max ", shift_max);
    rank_line_end(results);
    if (!signs_agree)
    {
        mmgr_line_put(&results->line, "  a value is below 0, or a precursor or an m/z is 2^53 or more");
        rank_line_end(results);
        return 0;
    }
    if (sim_job_submit(results, "casmi_driver", count, arguments, RANK_DECLARED) == 0)
    {
        return 0;
    }

    // each spectrum's mode, the reserved library, and the reference spectra of each structure
    unsigned int mode_of_string[2] = {rank_string_find(&set, RANK_MODE, mode_name[0]),
                                      rank_string_find(&set, RANK_MODE, mode_name[1])};
    // each spectrum's mode, whether it is a query and whether it is a reference, and each structure's first reference
    const unsigned long long structures = set.strings[RANK_KEY].count;
    unsigned int *const mode = (unsigned int *)rank_cell(spectra, sizeof(unsigned int), 1);
    unsigned char *const is_query = (unsigned char *)rank_cell(spectra, 1u, 1);
    unsigned char *const reference = (unsigned char *)rank_cell(spectra, 1u, 1);
    unsigned long long *const structure_reference_first =
        (unsigned long long *)rank_cell(structures + 1ull, sizeof(unsigned long long), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int named = set.text[RANK_MODE][spectrum];
        mode[spectrum] = (named == mode_of_string[0]) ? 0u : (named == mode_of_string[1]) ? 1u : RANK_ABSENT;
    }
    const unsigned int library_query = validate ? rank_string_find(&set, RANK_LIBRARY, query_library) : RANK_ABSENT;
    if (validate && (library_query == RANK_ABSENT))
    {
        mmgr_line_put(&results->line, "  validate.query_library is no library of the set: ");
        mmgr_line_put(&results->line, query_library);
        rank_line_end(results);
        return 0;
    }
    // each spectrum a query or a reference: in validate the reserved library's spectra and every other, in test the
    // test set's and the set's
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        is_query[spectrum] =
            (unsigned char)(validate ? (set.text[RANK_LIBRARY][spectrum] == library_query) : (spectrum >= set_spectra));
    }
    unsigned long long reference_count = 0ull;
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int structure = set.text[RANK_KEY][spectrum];
        reference[spectrum] =
            (unsigned char)((mode[spectrum] != RANK_ABSENT) && (structure != RANK_ABSENT) && !is_query[spectrum]);
        if (reference[spectrum])
        {
            structure_reference_first[structure + 1ull] += 1ull;
            reference_count += 1ull;
        }
    }
    for (unsigned long long structure = 0ull; structure < structures; structure += 1ull)
    {
        structure_reference_first[structure + 1ull] += structure_reference_first[structure];
    }
    unsigned int *const structure_reference =
        (unsigned int *)rank_cell(reference_count + 1ull, sizeof(unsigned int), 1);
    {
        // each structure's references so far, a cell of the temporary tier that the block gives back
        unsigned long long *const filled =
            (unsigned long long *)rank_cell(structures + 1ull, sizeof(unsigned long long), 1);
        if (rank_cells_refused(results))
        {
            return 0;
        }
        for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
        {
            if (reference[spectrum])
            {
                const unsigned int structure = set.text[RANK_KEY][spectrum];
                structure_reference[structure_reference_first[structure] + filled[structure]] = (unsigned int)spectrum;
                filled[structure] += 1ull;
            }
        }
        rank_cell_released(filled);
    }
    rank_line_decimal(results, "  reference spectra: ", reference_count);
    rank_line_end(results);

    // the queries and their molecules: the query spectra of a known mode, grouped by structure in validate and by
    // molecule_id in test
    const unsigned int molecule_text = validate ? RANK_KEY : RANK_MOLECULE;
    const unsigned long long names = set.strings[molecule_text].count;
    unsigned int *const query = (unsigned int *)rank_cell(spectra + 1ull, sizeof(unsigned int), 1);
    unsigned int *const molecule_of_name = (unsigned int *)rank_cell(names + 1ull, sizeof(unsigned int), 1);
    unsigned int *const molecule_name = (unsigned int *)rank_cell(names + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = molecule_of_name, .bytes = (size_t)((names + 1ull) * sizeof(unsigned int)),
               .val = 0xFF);
    unsigned long long query_count = 0ull;
    unsigned long long molecule_count = 0ull;
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int name = set.text[molecule_text][spectrum];
        if (!is_query[spectrum] || (mode[spectrum] == RANK_ABSENT) || (name == RANK_ABSENT))
        {
            continue;
        }
        if (molecule_of_name[name] == RANK_ABSENT)
        {
            molecule_of_name[name] = (unsigned int)molecule_count;
            molecule_name[molecule_count] = name;
            molecule_count += 1ull;
        }
        query[query_count] = (unsigned int)spectrum;
        query_count += 1ull;
    }
    unsigned long long *const molecule_query_first =
        (unsigned long long *)rank_cell(molecule_count + 2ull, sizeof(unsigned long long), 1);
    unsigned int *const molecule_query = (unsigned int *)rank_cell(query_count + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long each = 0ull; each < query_count; each += 1ull)
    {
        molecule_query_first[molecule_of_name[set.text[molecule_text][query[each]]] + 1ull] += 1ull;
    }
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        molecule_query_first[molecule + 1ull] += molecule_query_first[molecule];
    }
    {
        // each molecule's queries so far, a cell of the temporary tier that the block gives back
        unsigned long long *const filled =
            (unsigned long long *)rank_cell(molecule_count + 1ull, sizeof(unsigned long long), 1);
        if (rank_cells_refused(results))
        {
            return 0;
        }
        for (unsigned long long each = 0ull; each < query_count; each += 1ull)
        {
            const unsigned int molecule = molecule_of_name[set.text[molecule_text][query[each]]];
            molecule_query[molecule_query_first[molecule] + filled[molecule]] = (unsigned int)each;
            filled[molecule] += 1ull;
        }
        rank_cell_released(filled);
    }
    rank_line_decimal(results, "  queries: ", query_count);
    rank_line_decimal(results, " spectra of ", molecule_count);
    mmgr_line_put(&results->line, " molecules");
    rank_line_end(results);

    // the query adducts, each read once
    // the adducts, RANK_ADDUCTS_MAX at max, `adduct_count` of them, and each query's
    CasmiAdduct *const adduct = (CasmiAdduct *)rank_cell(RANK_ADDUCTS_MAX, sizeof(CasmiAdduct), 1);
    unsigned int adduct_name[RANK_ADDUCTS_MAX];
    unsigned int adduct_count = 0u;
    unsigned int *const query_adduct = (unsigned int *)rank_cell(query_count + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    unsigned long long refused_adduct = 0ull;
    for (unsigned long long each = 0ull; each < query_count; each += 1ull)
    {
        const unsigned int name = set.text[RANK_ADDUCT][query[each]];
        unsigned int position = 0u;
        while ((position < adduct_count) && (adduct_name[position] != name))
        {
            position += 1u;
        }
        if ((position == adduct_count) && (name != RANK_ABSENT) && (position < RANK_ADDUCTS_MAX))
        {
            CasmiAdduct read;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &read, .bytes = sizeof(read), .val = 0);
            const CasmiAdductRead request = {rank_string(&set, RANK_ADDUCT, name),
                                             (size_t)rank_string_length(&set, RANK_ADDUCT, name), &read};
            if (casmi_adduct_read(&request) == ENGINE_BYTES_ERROR)
            {
                read.charge = 0ll;
            }
            adduct[adduct_count] = read;
            adduct_name[adduct_count] = name;
            adduct_count += 1u;
        }
        const int readable = (position < adduct_count) && (adduct[position].charge != 0ll);
        query_adduct[each] = readable ? (unsigned int)position : RANK_ABSENT;
        refused_adduct += readable ? 0ull : 1ull;
    }
    rank_line_decimal(results, "  query adducts: ", adduct_count);
    rank_line_decimal(results, " read; query spectra whose adduct was refused ", refused_adduct);
    rank_line_end(results);

    // the candidate formulas: every structure containing a reference spectrum, its formula the first its spectra name
    const unsigned long long formula_texts = set.strings[RANK_FORMULA].count;
    unsigned int *const formula_of_structure = (unsigned int *)rank_cell(structures + 1ull, sizeof(unsigned int), 1);
    unsigned int *const formula_text_of_structure =
        (unsigned int *)rank_cell(structures + 1ull, sizeof(unsigned int), 1);
    unsigned int *const window_of_text = (unsigned int *)rank_cell(formula_texts + 1ull, sizeof(unsigned int), 1);
    CasmiFormula *const formula = (CasmiFormula *)rank_cell(formula_texts + 1ull, sizeof(CasmiFormula), 1);
    unsigned int *const formula_text = (unsigned int *)rank_cell(formula_texts + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = formula_of_structure,
               .bytes = (size_t)((structures + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = formula_text_of_structure,
               .bytes = (size_t)((structures + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = window_of_text,
               .bytes = (size_t)((formula_texts + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    unsigned long long formula_count = 0ull;
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int structure = set.text[RANK_KEY][spectrum];
        if ((structure != RANK_ABSENT) && (formula_text_of_structure[structure] == RANK_ABSENT))
        {
            formula_text_of_structure[structure] = set.text[RANK_FORMULA][spectrum];
        }
    }
    unsigned long long candidate_structures = 0ull;
    unsigned long long refused_formula = 0ull;
    for (unsigned long long structure = 0ull; structure < structures; structure += 1ull)
    {
        const unsigned int text = formula_text_of_structure[structure];
        if ((structure_reference_first[structure + 1ull] == structure_reference_first[structure]) ||
            (text == RANK_ABSENT))
        {
            continue;
        }
        candidate_structures += 1ull;
        if (window_of_text[text] == RANK_ABSENT)
        {
            CasmiFormula read;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &read, .bytes = sizeof(read), .val = 0);
            const unsigned long long formula_length = rank_string_length(&set, RANK_FORMULA, text);
            const CasmiFormulaRead request = {rank_string(&set, RANK_FORMULA, text), (size_t)formula_length, &read};
            int good = (formula_length != 0ull) && (casmi_formula_read(&request) >= 0ll) && (read.element_count > 0u);
            for (unsigned int element = 0u; good && (element < read.element_count); element += 1u)
            {
                good = read.count[element] >= 0ll;
            }
            if (good)
            {
                window_of_text[text] = (unsigned int)formula_count;
                formula[formula_count] = read;
                formula_text[formula_count] = text;
                formula_count += 1ull;
            }
            else
            {
                window_of_text[text] = RANK_ABSENT - 1u;
                refused_formula += 1ull;
            }
        }
        const unsigned int named = window_of_text[text];
        formula_of_structure[structure] = (named < (RANK_ABSENT - 1u)) ? named : RANK_ABSENT;
    }
    unsigned long long *const formula_structure_first =
        (unsigned long long *)rank_cell(formula_count + 2ull, sizeof(unsigned long long), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long structure = 0ull; structure < structures; structure += 1ull)
    {
        if (formula_of_structure[structure] != RANK_ABSENT)
        {
            formula_structure_first[formula_of_structure[structure] + 1ull] += 1ull;
        }
    }
    for (unsigned long long each = 0ull; each < formula_count; each += 1ull)
    {
        formula_structure_first[each + 1ull] += formula_structure_first[each];
    }
    unsigned int *const formula_structure =
        (unsigned int *)rank_cell(formula_structure_first[formula_count] + 1ull, sizeof(unsigned int), 1);
    {
        // each formula's structures so far, a cell of the temporary tier that the block gives back
        unsigned long long *const filled =
            (unsigned long long *)rank_cell(formula_count + 1ull, sizeof(unsigned long long), 1);
        if (rank_cells_refused(results))
        {
            return 0;
        }
        for (unsigned long long structure = 0ull; structure < structures; structure += 1ull)
        {
            const unsigned int each = formula_of_structure[structure];
            if (each != RANK_ABSENT)
            {
                formula_structure[formula_structure_first[each] + filled[each]] = (unsigned int)structure;
                filled[each] += 1ull;
            }
        }
        rank_cell_released(filled);
    }
    rank_line_decimal(results, "  candidate structures: ", candidate_structures);
    rank_line_decimal(results, "; formulas read ", formula_count);
    rank_line_decimal(results, ", refused ", refused_formula);
    rank_line_decimal(results, "; read by ", engine_clock_microseconds() - s_rank_started);
    mmgr_line_put(&results->line, " us of the run");
    rank_line_end(results);

    // ---- ion ----
    unsigned int isotope_count = 0u;
    const CasmiIsotope *const isotope = casmi_isotope_table(&isotope_count);
    unsigned int terms_max = 0u;
    unsigned int molecules_bits = 1u;
    unsigned int charge_bits = 1u;
    unsigned int formula_bits = 1u;
    unsigned int adduct_bits = 1u;
    unsigned int mass_bits = 1u;
    for (unsigned int position = 0u; position < adduct_count; position += 1u)
    {
        if (adduct[position].charge == 0ll)
        {
            continue;
        }
        molecules_bits = rank_larger(molecules_bits, rank_magnitude_bits(adduct[position].molecules));
        charge_bits = rank_larger(charge_bits, rank_magnitude_bits(adduct[position].charge));
        for (unsigned long long each = 0ull; each < formula_count; each += 1ull)
        {
            RankTerm term[RANK_TERMS_MAX];
            const unsigned int terms = rank_terms_merge(&formula[each], &adduct[position], term);
            terms_max = rank_larger(terms_max, terms);
            for (unsigned int entry = 0u; entry < terms; entry += 1u)
            {
                mass_bits = rank_larger(mass_bits, rank_magnitude_bits(isotope[term[entry].isotope].mass));
                formula_bits = rank_larger(formula_bits, rank_magnitude_bits(term[entry].formula));
                adduct_bits = rank_larger(adduct_bits, rank_magnitude_bits(term[entry].adduct));
            }
        }
    }
    // the ion record: molecules, charge, then each term's formula count, adduct count and mass, each signed
    const unsigned int ion_term_bits = (formula_bits + 1u) + (adduct_bits + 1u) + (mass_bits + 1u);
    const unsigned int ion_atom_bits = (molecules_bits + 1u) + (charge_bits + 1u) + (terms_max * ion_term_bits);
    const unsigned int ion_atom_limbs = (ion_atom_bits + 31u) / 32u;
    RankMachine ion;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        unsigned int at = 0u;
        const unsigned int molecules_field = exact_record_member_field(&program, molecules_bits + 1u, at);
        at += molecules_bits + 1u;
        const unsigned int charge_field = exact_record_member_field(&program, charge_bits + 1u, at);
        at += charge_bits + 1u;
        const unsigned int molecules = exact_record_read(&program, molecules_field, 0u);
        unsigned int sum = exact_record_constant(&program, 0ull);
        for (unsigned int term = 0u; term < terms_max; term += 1u)
        {
            const unsigned int formula_field = exact_record_member_field(&program, formula_bits + 1u, at);
            at += formula_bits + 1u;
            const unsigned int adduct_field = exact_record_member_field(&program, adduct_bits + 1u, at);
            at += adduct_bits + 1u;
            const unsigned int mass_field = exact_record_member_field(&program, mass_bits + 1u, at);
            at += mass_bits + 1u;
            const unsigned int coefficient =
                exact_record_sum(&program,
                                 exact_record_product(&program, molecules, exact_record_read(&program, formula_field, 0u)),
                                 exact_record_read(&program, adduct_field, 0u));
            sum = exact_record_sum(&program, sum,
                                   exact_record_product(&program, coefficient, exact_record_read(&program, mass_field, 0u)));
        }
        const unsigned int charge = exact_record_read(&program, charge_field, 0u);
        unsigned int outputs[3];
        outputs[0] = exact_record_difference(
            &program, exact_record_product(&program, sum, exact_record_constant(&program, CASMI_TENTHS_PER_FEMTODALTON)),
            exact_record_product(&program, charge, exact_record_constant(&program, CASMI_ELECTRON_MASS_2022)));
        outputs[1] = exact_record_absolute(&program, charge);
        outputs[2] = sum;
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {ion_atom_limbs, 0u, 0u};
        ok = rank_machine_load(results, "ion", &program, outputs, 3u, 1u, limbs, &ion);
        exact_record_close(&program);
        if (!ok)
        {
            return 0;
        }
    }
    const RankField ion_mass = rank_output(&ion, 0u);
    const RankField ion_charge = rank_output(&ion, 1u);
    const RankField ion_sum = rank_output(&ion, 2u);

    // ---- window ----
    // the precursor record: its mantissa at bit 0 and its exponent at 64, the precursor m 2^(E - 1075)
    const unsigned int precursor_limbs = 3u;
    RankMachine window;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int numerator_field = exact_record_member_field(&program, RANK_MANTISSA_FIELD_BITS, 0u);
        const unsigned int exponent_field = exact_record_member_field(&program, RANK_EXPONENT_BITS, 64u);
        const unsigned int mass_field = exact_record_member_field(&program, ion_mass.bits, ion_mass.offset);
        const unsigned int charge_field = exact_record_member_field(&program, ion_charge.bits, ion_charge.offset);
        unsigned int fields[RANK_ENVELOPE_FIELDS];
        rank_envelope_fields(&program, &envelope_layout, fields);
        const unsigned int numerator = exact_record_read_unsigned(&program, numerator_field, 0u);
        const unsigned int denominator =
            rank_denominator_steps(&program, exact_record_read_unsigned(&program, exponent_field, 0u), two_bits);
        const unsigned int mass = exact_record_read(&program, mass_field, 1u);
        const unsigned int charge = exact_record_read(&program, charge_field, 1u);
        // D = n |z| 10^16 - I d, N = I d
        const unsigned int measured =
            exact_record_product(&program, exact_record_product(&program, numerator, charge),
                                 exact_record_constant(&program, RANK_PER_DALTON));
        const unsigned int exact = exact_record_product(&program, mass, denominator);
        unsigned int outputs[2];
        rank_envelope_steps(&program, fields, 2u, exact_record_difference(&program, measured, exact), exact, &outputs[0],
                            &outputs[1]);
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {precursor_limbs, ion.layout.out_limbs,
                                                               envelope_layout.limbs};
        ok = rank_machine_load(results, "window", &program, outputs, 2u, 3u, limbs, &window);
        exact_record_close(&program);
        if (!ok)
        {
            return 0;
        }
    }
    const RankField window_high = rank_output(&window, 0u);
    const RankField window_low = rank_output(&window, 1u);

    // ion and window, one adduct at a time: the formulas inside each query's window, in formula order
    // The formulas inside each query's window, a list that grows by the device's verdicts. The ion's records, and a
    // chunk's queries and their precursor records at the most queries a chunk holds
    unsigned long long *const inside_first =
        (unsigned long long *)rank_cell(query_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const inside_count =
        (unsigned long long *)rank_cell(query_count + 1ull, sizeof(unsigned long long), 1);
    unsigned int *inside = NULL;
    unsigned long long inside_used = 0ull;
    unsigned long long inside_room = 0ull;
    const unsigned long long ion_words = (formula_count + 1ull) * ion_atom_limbs;
    unsigned int *const ion_atoms = (unsigned int *)rank_cell(ion_words, sizeof(unsigned int), 1);
    unsigned int *const ion_records =
        (unsigned int *)rank_cell((formula_count + 1ull) * ion.layout.out_limbs, sizeof(unsigned int), 1);
    const unsigned long long queries_max = (RANK_CHUNK_LANES / (formula_count + 1ull)) + 1ull;
    const unsigned long long chunk_queries = (queries_max < query_count) ? queries_max : query_count;
    unsigned int *const window_atoms =
        (unsigned int *)rank_cell(chunk_queries * precursor_limbs, sizeof(unsigned int), 1);
    unsigned int *const window_query = (unsigned int *)rank_cell(chunk_queries, sizeof(unsigned int), 1);
    ok = !rank_cells_refused(results);
    RankTally ion_tally = {0ull, 0ull};
    RankTally window_tally = {0ull, 0ull};
    unsigned long long ion_against_c = 0ull;
    unsigned long long window_mixed = 0ull;
    for (unsigned int position = 0u; ok && (position < adduct_count); position += 1u)
    {
        if (adduct[position].charge == 0ll)
        {
            continue;
        }
        EMBED_CALL(memor.set, MemoriaCfg, .dst = ion_atoms, .bytes = (size_t)(ion_words * sizeof(unsigned int)),
                   .val = 0);
        for (unsigned long long each = 0ull; each < formula_count; each += 1ull)
        {
            unsigned int *const atom = &ion_atoms[each * ion_atom_limbs];
            RankTerm term[RANK_TERMS_MAX];
            const unsigned int terms = rank_terms_merge(&formula[each], &adduct[position], term);
            OriorExactInteger value;
            unsigned int at = 0u;
            rank_exact_signed(adduct[position].molecules, &value);
            rank_field_put(atom, at, molecules_bits + 1u, &value);
            at += molecules_bits + 1u;
            rank_exact_signed(adduct[position].charge, &value);
            rank_field_put(atom, at, charge_bits + 1u, &value);
            at += charge_bits + 1u;
            for (unsigned int entry = 0u; entry < terms; entry += 1u)
            {
                rank_exact_signed(term[entry].formula, &value);
                rank_field_put(atom, at, formula_bits + 1u, &value);
                at += formula_bits + 1u;
                rank_exact_signed(term[entry].adduct, &value);
                rank_field_put(atom, at, adduct_bits + 1u, &value);
                at += adduct_bits + 1u;
                rank_exact_signed(isotope[term[entry].isotope].mass, &value);
                rank_field_put(atom, at, mass_bits + 1u, &value);
                at += mass_bits + 1u;
            }
        }
        const unsigned int *const ion_in[ENGINE_RECORD_MEMBERS_MAX] = {ion_atoms, NULL, NULL};
        const unsigned long long ion_bodies[ENGINE_RECORD_MEMBERS_MAX] = {formula_count, 0ull, 0ull};
        ok = rank_sweep(results, "ion", &ion, ion_in, ion_bodies, NULL, formula_count, ion_records,
                        &ion_tally.microseconds);
        ion_tally.lanes += formula_count;
        // the engine's own C sum beside the record machine's, for every formula
        for (unsigned long long each = 0ull; ok && (each < formula_count); each += 1ull)
        {
            const long long neutral = casmi_formula_mass(&formula[each]);
            const long long numerator = (neutral < 0ll) ? -1ll : casmi_ion_numerator(neutral, &adduct[position]);
            const long long expected =
                (numerator < 0ll) ? -1ll : (numerator + (adduct[position].charge * CASMI_ELECTRON_MASS));
            OriorExactInteger formula_entry;
            rank_field_read(&ion_records[each * ion.layout.out_limbs], ion_sum, &formula_entry);
            OriorExactInteger wanted;
            rank_exact_signed(expected, &wanted);
            ion_against_c += ((expected >= 0ll) && !orior_exact_equal(&formula_entry, &wanted)) ? 1ull : 0ull;
        }
        // the queries of this adduct against every formula, as many at a time as fill a chunk
        unsigned long long each = 0ull;
        while (ok && (each < query_count))
        {
            unsigned long long taken = 0ull;
            while ((each < query_count) && (taken < queries_max))
            {
                if (query_adduct[each] == position)
                {
                    const unsigned long long spectrum = query[each];
                    unsigned long long mantissa = 0ull;
                    unsigned int exponent = 0u;
                    rank_double(set.precursor.bits[set.precursor.row_start[spectrum]], &mantissa, &exponent);
                    unsigned int *const atom = &window_atoms[taken * precursor_limbs];
                    EMBED_CALL(memor.set, MemoriaCfg, .dst = atom, .bytes = precursor_limbs * sizeof(unsigned int),
                               .val = 0);
                    rank_put_unsigned(atom, 0u, RANK_MANTISSA_FIELD_BITS, mantissa);
                    rank_put_unsigned(atom, 64u, RANK_EXPONENT_BITS, exponent);
                    window_query[taken] = (unsigned int)each;
                    taken += 1ull;
                }
                each += 1ull;
            }
            if (taken == 0ull)
            {
                continue;
            }
            // the chunk's index and signs, cells of the temporary tier that the chunk gives back
            const unsigned long long lanes = taken * formula_count;
            unsigned int *const index = (unsigned int *)rank_cell(3ull * lanes, sizeof(unsigned int), 1);
            unsigned int *const signs =
                (unsigned int *)rank_cell((lanes + 1ull) * window.layout.out_limbs, sizeof(unsigned int), 1);
            ok = !rank_cells_refused(results);
            unsigned long long index_at = 0ull;
            for (unsigned long long local = 0ull; ok && (local < taken); local += 1ull)
            {
                const unsigned int query_mode = mode[query[window_query[local]]];
                for (unsigned long long candidate = 0ull; candidate < formula_count; candidate += 1ull)
                {
                    index[index_at] = (unsigned int)local;
                    index[index_at + 1ull] = (unsigned int)candidate;
                    index[index_at + 2ull] = query_mode;
                    index_at += 3ull;
                }
            }
            const unsigned int *const window_in[ENGINE_RECORD_MEMBERS_MAX] = {window_atoms, ion_records,
                                                                              envelope_records};
            const unsigned long long window_bodies[ENGINE_RECORD_MEMBERS_MAX] = {taken, formula_count, RANK_MODES};
            ok = ok && rank_sweep(results, "window", &window, window_in, window_bodies, index, lanes, signs,
                                  &window_tally.microseconds);
            window_tally.lanes += lanes;
            unsigned long long lane = 0ull;
            for (unsigned long long local = 0ull; ok && (local < taken); local += 1ull)
            {
                const unsigned int formula_entry = window_query[local];
                inside_first[formula_entry] = inside_used;
                for (unsigned long long candidate = 0ull; candidate < formula_count; candidate += 1ull)
                {
                    const unsigned int *const record = &signs[lane * window.layout.out_limbs];
                    const int high = rank_field_sign(record, window_high);
                    const int low = rank_field_sign(record, window_low);
                    window_mixed += ((high == 0) || (low == 0)) ? 1ull : 0ull;
                    if ((high < 0) && (low > 0))
                    {
                        ok = ok && rank_host_room((void **)&inside, &inside_room, inside_used + 1ull, inside_used,
                                                  sizeof(unsigned int));
                        if (ok)
                        {
                            inside[inside_used] = (unsigned int)candidate;
                            inside_used += 1ull;
                            inside_count[formula_entry] += 1ull;
                        }
                    }
                    lane += 1ull;
                }
            }
            rank_cell_released(signs);
            rank_cell_released(index);
        }
    }
    rank_machine_release(&ion);
    rank_machine_release(&window);
    if (!ok)
    {
        return 0;
    }
    rank_tally_line(results, "ion", &ion_tally);
    rank_line_decimal(results, "  ion against the engine's C sum: formulas differing ", ion_against_c);
    rank_line_end(results);
    rank_tally_line(results, "window", &window_tally);
    unsigned long long queries_empty = 0ull;
    for (unsigned long long each = 0ull; each < query_count; each += 1ull)
    {
        queries_empty += (inside_count[each] == 0ull) ? 1ull : 0ull;
    }
    rank_line_decimal(results, "  window: (query, formula) inside ", inside_used);
    rank_line_decimal(results, "; queries containing none ", queries_empty);
    rank_line_decimal(results, "; signs of 0 ", window_mixed);
    rank_line_end(results);

    // ---- peaks ----
    RankPeakLayout peak_layout;
    peak_layout.exponent_offset = 64u;
    peak_layout.intensity_offset = 96u;
    peak_layout.shift_offset = peak_layout.intensity_offset + RANK_MANTISSA_FIELD_BITS;
    peak_layout.shift_bits = shift_bits;
    peak_layout.limbs = (peak_layout.shift_offset + shift_bits + 31u) / 32u;
    rank_line_decimal(results, "  peaks: record ", peak_layout.limbs);
    mmgr_line_put(&results->line, " limbs");
    rank_line_end(results);
    // a spectrum's peaks as records: each m/z's mantissa and exponent, and each intensity's mantissa and its exponent
    // above its row's least, 0 for an intensity of 0
    auto peak_put = [&](unsigned long long spectrum, unsigned int *atoms) {
        const unsigned long long first = set.mz.row_start[spectrum];
        const unsigned long long past = set.mz.row_start[spectrum + 1ull];
        const unsigned long long intensity_first = set.intensity.row_start[spectrum];
        for (unsigned long long value = first; value < past; value += 1ull)
        {
            unsigned int *const atom = &atoms[(value - first) * peak_layout.limbs];
            unsigned long long mantissa = 0ull;
            unsigned int exponent = 0u;
            rank_double(set.mz.bits[value], &mantissa, &exponent);
            rank_put_unsigned(atom, 0u, RANK_MANTISSA_FIELD_BITS, mantissa);
            rank_put_unsigned(atom, peak_layout.exponent_offset, RANK_EXPONENT_BITS, exponent);
            rank_double(set.intensity.bits[intensity_first + (value - first)], &mantissa, &exponent);
            rank_put_unsigned(atom, peak_layout.intensity_offset, RANK_MANTISSA_FIELD_BITS, mantissa);
            rank_put_unsigned(atom, peak_layout.shift_offset, peak_layout.shift_bits,
                              (mantissa != 0ull) ? (exponent - intensity_floor[spectrum]) : 0u);
        }
    };

    RankMachine match;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int numerator_field = exact_record_member_field(&program, RANK_MANTISSA_FIELD_BITS, 0u);
        const unsigned int exponent_field =
            exact_record_member_field(&program, RANK_EXPONENT_BITS, peak_layout.exponent_offset);
        const unsigned int intensity_field =
            exact_record_member_field(&program, RANK_MANTISSA_FIELD_BITS, peak_layout.intensity_offset);
        const unsigned int shift_field = exact_record_member_field(&program, peak_layout.shift_bits, peak_layout.shift_offset);
        unsigned int fields[RANK_ENVELOPE_FIELDS];
        rank_envelope_fields(&program, &envelope_layout, fields);
        const unsigned int query_n = exact_record_read_unsigned(&program, numerator_field, 0u);
        const unsigned int reference_n = exact_record_read_unsigned(&program, numerator_field, 1u);
        const unsigned int query_d =
            rank_denominator_steps(&program, exact_record_read_unsigned(&program, exponent_field, 0u), two_bits);
        const unsigned int reference_d =
            rank_denominator_steps(&program, exact_record_read_unsigned(&program, exponent_field, 1u), two_bits);
        // e = n_q d_r - n_r d_q against n_r d_q
        const unsigned int referenced = exact_record_product(&program, reference_n, query_d);
        const unsigned int error =
            exact_record_difference(&program, exact_record_product(&program, query_n, reference_d), referenced);
        unsigned int high = 0u;
        unsigned int low = 0u;
        rank_envelope_steps(&program, fields, 2u, error, referenced, &high, &low);
        const unsigned int one = exact_record_constant(&program, 1ull);
        const unsigned int inside = exact_record_product(&program, exact_record_difference(&program, one, high),
                                                         exact_record_sum(&program, one, low));
        // the weight, I_q I_r = m_q m_r 2^(s_q + s_r) on the two rows' least powers of two, and 1 where it is above
        // zero: the lanes retained
        const unsigned int shifts = exact_record_sum(&program, exact_record_read_unsigned(&program, shift_field, 0u),
                                                     exact_record_read_unsigned(&program, shift_field, 1u));
        unsigned int outputs[2];
        outputs[0] = exact_record_product(
            &program, inside,
            exact_record_product(&program,
                                 exact_record_product(&program, exact_record_read_unsigned(&program, intensity_field, 0u),
                                                      exact_record_read_unsigned(&program, intensity_field, 1u)),
                                 exact_record_two_to(&program, shifts, peak_layout.shift_bits + 1u)));
        outputs[1] = exact_record_above(&program, outputs[0], exact_record_constant(&program, 0ull));
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {peak_layout.limbs, peak_layout.limbs,
                                                               envelope_layout.limbs};
        ok = rank_machine_load(results, "match", &program, outputs, 2u, 3u, limbs, &match);
        exact_record_close(&program);
        if (!ok)
        {
            return 0;
        }
    }
    const RankField weight_field = rank_output(&match, 0u);
    const RankField match_field = rank_output(&match, 1u);
    const unsigned int match_limbs = match.layout.out_limbs;
    rank_line_decimal(results, "  match: program loaded by ", engine_clock_microseconds() - s_rank_started);
    mmgr_line_put(&results->line, " us of the run");
    rank_line_end(results);

    // match: each query's peaks against every peak of every reference spectrum its window names, same mode
    // The retained records and their (pair, query peak) groups, and the pairs: lists that grow by the device's
    // verdicts. A query's reference spectra, a chunk's spectra, and a chunk's retained lanes and records, cells that
    // grow to the most any of them holds
    unsigned int *matched = NULL;
    unsigned long long matched_used = 0ull;
    unsigned long long matched_room = 0ull;
    unsigned int *matched_group_first = NULL;
    unsigned long long group_used = 0ull;
    unsigned long long group_room = 0ull;
    unsigned int *pair_query = NULL;
    unsigned int *pair_spectrum = NULL;
    unsigned int *pair_group_first = NULL;
    unsigned long long pair_used = 0ull;
    unsigned long long pair_rooms[3] = {0ull, 0ull, 0ull};
    unsigned long long *const query_pair_first =
        (unsigned long long *)rank_cell(query_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const query_pair_count =
        (unsigned long long *)rank_cell(query_count + 1ull, sizeof(unsigned long long), 1);
    RankTally match_tally = {0ull, 0ull};
    RankMatchDevice match_device;
    ok = !rank_cells_refused(results) && rank_match_open(&match_device, envelope_records, envelope_words);
    unsigned int *matched_lanes = NULL;
    unsigned long long lanes_room = 0ull;
    unsigned int *matched_records = NULL;
    unsigned long long records_room = 0ull;
    unsigned long long retained = 0ull;
    unsigned int *query_reference = NULL;
    unsigned long long query_reference_room = 0ull;
    unsigned int *chunk = NULL;
    unsigned long long chunk_room = 0ull;
    for (unsigned long long each = 0ull; ok && (each < query_count); each += 1ull)
    {
        const unsigned int spectrum_q = query[each];
        const unsigned int query_mode = mode[spectrum_q];
        const unsigned long long query_peaks = set.mz.row_start[spectrum_q + 1ull] - set.mz.row_start[spectrum_q];
        query_pair_first[each] = pair_used;
        if ((query_peaks == 0ull) || (inside_count[each] == 0ull))
        {
            continue;
        }
        // the query's peaks, a cell of the temporary tier that the query gives back
        unsigned int *const query_atoms =
            (unsigned int *)rank_cell(query_peaks * peak_layout.limbs, sizeof(unsigned int), 1);
        ok = !rank_cells_refused(results);
        if (ok)
        {
            peak_put(spectrum_q, query_atoms);
        }
        ok = ok && rank_match_query(&match_device, query_atoms, (size_t)(query_peaks * peak_layout.limbs));
        unsigned long long reference_used = 0ull;
        for (unsigned long long position = 0ull; ok && (position < inside_count[each]); position += 1ull)
        {
            const unsigned int named = inside[inside_first[each] + position];
            for (unsigned long long at = formula_structure_first[named]; at < formula_structure_first[named + 1ull];
                 at += 1ull)
            {
                const unsigned int structure = formula_structure[at];
                for (unsigned long long spectrum_at = structure_reference_first[structure];
                     spectrum_at < structure_reference_first[structure + 1ull]; spectrum_at += 1ull)
                {
                    const unsigned int spectrum = structure_reference[spectrum_at];
                    const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                    if (ok && (mode[spectrum] == query_mode) && (peaks > 0ull))
                    {
                        ok = rank_host_room((void **)&query_reference, &query_reference_room, reference_used + 1ull,
                                            reference_used, sizeof(unsigned int));
                        if (ok)
                        {
                            query_reference[reference_used] = spectrum;
                            reference_used += 1ull;
                        }
                    }
                }
            }
        }
        unsigned long long start = 0ull;
        while (ok && (start < reference_used))
        {
            // a chunk: reference spectra until their lanes would pass the chunk, at least one
            unsigned long long chunk_used = 0ull;
            unsigned long long lanes = 0ull;
            unsigned long long reference_peaks = 0ull;
            while (ok && (start < reference_used))
            {
                const unsigned int spectrum = query_reference[start];
                const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                if ((chunk_used != 0ull) && ((lanes + (query_peaks * peaks)) > RANK_CHUNK_LANES))
                {
                    break;
                }
                ok = rank_host_room((void **)&chunk, &chunk_room, chunk_used + 1ull, chunk_used, sizeof(unsigned int));
                if (ok)
                {
                    chunk[chunk_used] = spectrum;
                    chunk_used += 1ull;
                }
                lanes += query_peaks * peaks;
                reference_peaks += peaks;
                start += 1ull;
            }
            // the chunk's peaks, and each spectrum's first lane, first peak and peak count, cells of the temporary tier
            // that the chunk gives back
            unsigned int *const reference_atoms =
                (unsigned int *)rank_cell((reference_peaks * peak_layout.limbs) + 1ull, sizeof(unsigned int), 1);
            unsigned long long *const lane_first =
                (unsigned long long *)rank_cell(chunk_used + 1ull, sizeof(unsigned long long), 1);
            unsigned int *const atom_first = (unsigned int *)rank_cell(chunk_used, sizeof(unsigned int), 1);
            unsigned int *const chunk_peaks = (unsigned int *)rank_cell(chunk_used, sizeof(unsigned int), 1);
            void *const chunk_cells[4] = {chunk_peaks, atom_first, lane_first, reference_atoms};
            ok = ok && !rank_cells_refused(results);
            unsigned long long atom = 0ull;
            unsigned long long lane_at = 0ull;
            for (unsigned long long local = 0ull; ok && (local < chunk_used); local += 1ull)
            {
                const unsigned int spectrum = chunk[local];
                const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                peak_put(spectrum, &reference_atoms[atom * peak_layout.limbs]);
                lane_first[local] = lane_at;
                atom_first[local] = (unsigned int)atom;
                chunk_peaks[local] = (unsigned int)peaks;
                lane_at += query_peaks * peaks;
                atom += peaks;
            }
            if (ok)
            {
                lane_first[chunk_used] = lane_at;
            }
            const RankMatchChunk run = {&match,      match_field, reference_atoms, reference_peaks,
                                        lane_first,  atom_first,  chunk_peaks,     (size_t)chunk_used,
                                        query_peaks, RANK_MODES,  query_mode,      lanes};
            retained = 0ull;
            ok = ok && rank_match_chunk(results, &match_device, &run, &matched_lanes, &lanes_room, &matched_records,
                                        &records_room, &retained, &match_tally.microseconds);
            match_tally.lanes += lanes;
            // each retained lane, least first: its spectrum and query peak from the chunk's first lanes; a spectrum's
            // retained lanes are one pair, and a run of one (spectrum, query peak) is one group
            unsigned long long local = 0ull;
            unsigned long long open_spectrum = chunk_used;
            unsigned long long open_peak = ~0ull;
            for (unsigned long long matched_at = 0ull; ok && (matched_at < retained); matched_at += 1ull)
            {
                const unsigned long long lane = matched_lanes[matched_at];
                while (lane >= lane_first[local + 1ull])
                {
                    local += 1ull;
                }
                const unsigned long long query_peak = (lane - lane_first[local]) / chunk_peaks[local];
                if (local != open_spectrum)
                {
                    ok = rank_host_room((void **)&pair_query, &pair_rooms[0], pair_used + 1ull, pair_used,
                                        sizeof(unsigned int)) &&
                         rank_host_room((void **)&pair_spectrum, &pair_rooms[1], pair_used + 1ull, pair_used,
                                        sizeof(unsigned int)) &&
                         rank_host_room((void **)&pair_group_first, &pair_rooms[2], pair_used + 1ull, pair_used,
                                        sizeof(unsigned int));
                    if (!ok)
                    {
                        break;
                    }
                    pair_query[pair_used] = (unsigned int)each;
                    pair_spectrum[pair_used] = chunk[local];
                    pair_group_first[pair_used] = (unsigned int)group_used;
                    pair_used += 1ull;
                    query_pair_count[each] += 1ull;
                    open_spectrum = local;
                    open_peak = ~0ull;
                }
                if (query_peak != open_peak)
                {
                    ok = rank_host_room((void **)&matched_group_first, &group_room, group_used + 1ull, group_used,
                                        sizeof(unsigned int));
                    if (!ok)
                    {
                        break;
                    }
                    matched_group_first[group_used] = (unsigned int)(matched_used / match_limbs);
                    group_used += 1ull;
                    open_peak = query_peak;
                }
                ok = rank_host_room((void **)&matched, &matched_room, matched_used + match_limbs, matched_used,
                                    sizeof(unsigned int));
                if (ok)
                {
                    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &matched[matched_used],
                               .src = &matched_records[matched_at * match_limbs],
                               .bytes = match_limbs * sizeof(unsigned int));
                    matched_used += match_limbs;
                }
            }
            rank_cells_given(chunk_cells, 4u);
        }
        rank_cell_released(query_atoms);
    }
    rank_match_close(&match_device);
    rank_machine_release(&match);
    void *const match_cells[4] = {chunk, query_reference, matched_records, matched_lanes};
    rank_cells_given(match_cells, 4u);
    if (!ok)
    {
        (void)rank_cells_refused(results);
        return 0;
    }
    const unsigned long long matched_count = matched_used / match_limbs;
    const unsigned long long pair_count = pair_used;
    const unsigned long long peak_group_count = group_used;
    rank_tally_line(results, "match", &match_tally);
    rank_line_decimal(results, "  match: pairs containing a matched peak ", pair_count);
    rank_line_decimal(results, "; (pair, query peak) groups ", peak_group_count);
    rank_line_decimal(results, "; matched lanes retained ", matched_count);
    rank_line_end(results);

    // peak: each (pair, query peak) group's heaviest weight
    RankMachine value_order;
    ok = rank_value_order_position(results, weight_field, match_limbs, &value_order);
    unsigned long long *const peak_first =
        (unsigned long long *)rank_cell(peak_group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const peak_count =
        (unsigned long long *)rank_cell(peak_group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned int *const matched_entry = (unsigned int *)rank_cell(matched_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const peak_winner = (unsigned int *)rank_cell(peak_group_count + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long group = 0ull; group < peak_group_count; group += 1ull)
    {
        peak_first[group] = matched_group_first[group];
        const unsigned long long past = ((group + 1ull) < peak_group_count) ? matched_group_first[group + 1ull] : matched_count;
        peak_count[group] = past - peak_first[group];
    }
    for (unsigned long long record = 0ull; record < matched_count; record += 1ull)
    {
        matched_entry[record] = (unsigned int)record;
    }
    RankTally peak_tally = {0ull, 0ull};
    const RankGroups peak_groups = {peak_first, peak_count, matched_entry, peak_group_count};
    ok = ok && rank_group_best(results, "peak", &value_order, rank_output(&value_order, 0u), matched, matched_count,
                               &peak_groups, peak_winner, &peak_tally.microseconds);
    rank_machine_release(&value_order);
    rank_tally_line(results, "peak tournament", &peak_tally);

    // sum: M per pair, the sum of its query peaks' retained weights
    unsigned long long *const pair_first =
        (unsigned long long *)rank_cell(pair_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const pair_peaks =
        (unsigned long long *)rank_cell(pair_count + 1ull, sizeof(unsigned long long), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long pair = 0ull; pair < pair_count; pair += 1ull)
    {
        pair_first[pair] = pair_group_first[pair];
        const unsigned long long past = ((pair + 1ull) < pair_count) ? pair_group_first[pair + 1ull] : peak_group_count;
        pair_peaks[pair] = past - pair_first[pair];
    }
    RankTally weight_tally = {0ull, 0ull};
    const RankGroups pair_groups = {pair_first, pair_peaks, peak_winner, pair_count};
    unsigned int *weight_sums = NULL;
    RankField weight_sum_field = {0u, 0u};
    unsigned int weight_sum_limbs = 0u;
    ok = ok && rank_group_sum(results, "weight", weight_field, match_limbs, matched, matched_count, &pair_groups,
                              &weight_sums, &weight_sum_field, &weight_sum_limbs, &weight_tally.microseconds);
    rank_tally_line(results, "weight sums", &weight_tally);

    // norm: Q and R, the sum of I^2 over each spectrum a pair names
    // each pair names two spectra, and so the spectra of the norms number twice the pairs at max
    unsigned int *const norm_of_reference = (unsigned int *)rank_cell(spectra + 1ull, sizeof(unsigned int), 1);
    unsigned int *const norm_of_query = (unsigned int *)rank_cell(query_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const norm_spectrum = (unsigned int *)rank_cell((2ull * pair_count) + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = norm_of_reference,
               .bytes = (size_t)((spectra + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = norm_of_query,
               .bytes = (size_t)((query_count + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    unsigned long long norm_used = 0ull;
    for (unsigned long long pair = 0ull; pair < pair_count; pair += 1ull)
    {
        const unsigned int pair_query_at = pair_query[pair];
        const unsigned int pair_reference = pair_spectrum[pair];
        if (norm_of_query[pair_query_at] == RANK_ABSENT)
        {
            norm_of_query[pair_query_at] = (unsigned int)norm_used;
            norm_spectrum[norm_used] = query[pair_query_at];
            norm_used += 1ull;
        }
        if (norm_of_reference[pair_reference] == RANK_ABSENT)
        {
            norm_of_reference[pair_reference] = (unsigned int)norm_used;
            norm_spectrum[norm_used] = pair_reference;
            norm_used += 1ull;
        }
    }
    unsigned long long norm_peaks = 0ull;
    for (unsigned long long each = 0ull; each < norm_used; each += 1ull)
    {
        norm_peaks += set.mz.row_start[norm_spectrum[each] + 1ull] - set.mz.row_start[norm_spectrum[each]];
    }
    RankMachine norm;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        // I = m 2^s on the row's least power of two, as the match reads it
        const unsigned int intensity_field =
            exact_record_member_field(&program, RANK_MANTISSA_FIELD_BITS, peak_layout.intensity_offset);
        const unsigned int shift_field = exact_record_member_field(&program, peak_layout.shift_bits, peak_layout.shift_offset);
        const unsigned int intensity =
            exact_record_product(&program, exact_record_read_unsigned(&program, intensity_field, 0u),
                                 exact_record_two_to(&program, exact_record_read_unsigned(&program, shift_field, 0u),
                                                     peak_layout.shift_bits));
        unsigned int square = exact_record_product(&program, intensity, intensity);
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {peak_layout.limbs, 0u, 0u};
        ok = ok && rank_machine_load(results, "norm", &program, &square, 1u, 1u, limbs, &norm);
        exact_record_close(&program);
    }
    unsigned int *const norm_atoms =
        (unsigned int *)rank_cell((norm_peaks + 1ull) * peak_layout.limbs, sizeof(unsigned int), 1);
    unsigned long long *const norm_first =
        (unsigned long long *)rank_cell(norm_used + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const norm_count =
        (unsigned long long *)rank_cell(norm_used + 1ull, sizeof(unsigned long long), 1);
    unsigned int *const norm_entry = (unsigned int *)rank_cell(norm_peaks + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    unsigned long long norm_atom = 0ull;
    for (unsigned long long each = 0ull; each < norm_used; each += 1ull)
    {
        const unsigned int spectrum = norm_spectrum[each];
        const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
        norm_first[each] = norm_atom;
        norm_count[each] = peaks;
        peak_put(spectrum, &norm_atoms[norm_atom * peak_layout.limbs]);
        for (unsigned long long peak = 0ull; peak < peaks; peak += 1ull)
        {
            norm_entry[norm_atom + peak] = (unsigned int)(norm_atom + peak);
        }
        norm_atom += peaks;
    }
    const unsigned int norm_limbs = ok ? norm.layout.out_limbs : 1u;
    unsigned int *const norm_squares =
        (unsigned int *)rank_cell((norm_peaks + 1ull) * norm_limbs, sizeof(unsigned int), 1);
    ok = ok && !rank_cells_refused(results);
    RankTally square_tally = {norm_peaks, 0ull};
    const unsigned int *const norm_in[ENGINE_RECORD_MEMBERS_MAX] = {norm_atoms, NULL, NULL};
    const unsigned long long norm_bodies[ENGINE_RECORD_MEMBERS_MAX] = {norm_peaks, 0ull, 0ull};
    ok = ok && rank_sweep(results, "norm", &norm, norm_in, norm_bodies, NULL, norm_peaks, norm_squares,
                          &square_tally.microseconds);
    const RankField square_field = ok ? rank_output(&norm, 0u) : RankField{0u, 0u};
    rank_machine_release(&norm);
    RankTally norm_tally = {0ull, 0ull};
    const RankGroups norm_groups = {norm_first, norm_count, norm_entry, norm_used};
    unsigned int *norm_sums = NULL;
    RankField norm_sum_field = {0u, 0u};
    unsigned int norm_sum_limbs = 0u;
    ok = ok && rank_group_sum(results, "norm", square_field, norm_limbs, norm_squares, norm_peaks, &norm_groups,
                              &norm_sums, &norm_sum_field, &norm_sum_limbs, &norm_tally.microseconds);
    rank_tally_line(results, "norm squares", &square_tally);
    rank_tally_line(results, "norm sums", &norm_tally);

    // score: (M^2, Q R) per pair
    RankMachine score;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int matched_field = exact_record_member_field(&program, weight_sum_field.bits, weight_sum_field.offset);
        const unsigned int norm_field = exact_record_member_field(&program, norm_sum_field.bits, norm_sum_field.offset);
        const unsigned int m = exact_record_read(&program, matched_field, 0u);
        unsigned int outputs[2];
        outputs[0] = exact_record_product(&program, m, m);
        outputs[1] = exact_record_product(&program, exact_record_read(&program, norm_field, 1u),
                                          exact_record_read(&program, norm_field, 2u));
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {weight_sum_limbs, norm_sum_limbs, norm_sum_limbs};
        ok = ok && rank_machine_load(results, "score", &program, outputs, 2u, 3u, limbs, &score);
        exact_record_close(&program);
    }
    const unsigned int score_limbs = ok ? score.layout.out_limbs : 1u;
    unsigned int *const score_index = (unsigned int *)rank_cell((3ull * pair_count) + 3ull, sizeof(unsigned int), 1);
    unsigned int *const scores = (unsigned int *)rank_cell((pair_count + 1ull) * score_limbs, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long pair = 0ull; pair < pair_count; pair += 1ull)
    {
        score_index[3ull * pair] = (unsigned int)pair;
        score_index[(3ull * pair) + 1ull] = norm_of_query[pair_query[pair]];
        score_index[(3ull * pair) + 2ull] = norm_of_reference[pair_spectrum[pair]];
    }
    RankTally score_tally = {pair_count, 0ull};
    const unsigned int *const score_in[ENGINE_RECORD_MEMBERS_MAX] = {weight_sums, norm_sums, norm_sums};
    const unsigned long long score_bodies[ENGINE_RECORD_MEMBERS_MAX] = {pair_count + 1ull, norm_used + 1ull,
                                                                        norm_used + 1ull};
    ok = ok && rank_sweep(results, "score", &score, score_in, score_bodies, score_index, pair_count, scores,
                          &score_tally.microseconds);
    const RankField score_numerator = ok ? rank_output(&score, 0u) : RankField{0u, 0u};
    const RankField score_denominator = ok ? rank_output(&score, 1u) : RankField{0u, 0u};
    rank_machine_release(&score);
    rank_tally_line(results, "score", &score_tally);
    RankMachine ratio_order;
    ok = ok && rank_ratio_order_position(results, score_numerator, score_denominator, score_limbs, &ratio_order);
    if (!ok)
    {
        return 0;
    }
    const RankField order_sign = rank_output(&ratio_order, 0u);

    // structure: each (molecule, structure) group's heaviest pair
    // A group holds a pair at least, and each pair is of one query and so of one molecule. The groups and their entries
    // therefore number the pairs at max
    unsigned int *const structure_group = (unsigned int *)rank_cell(structures + 1ull, sizeof(unsigned int), 1);
    unsigned int *const group_structure = (unsigned int *)rank_cell(pair_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const group_entry = (unsigned int *)rank_cell(pair_count + 1ull, sizeof(unsigned int), 1);
    unsigned long long *const molecule_group_first =
        (unsigned long long *)rank_cell(molecule_count + 2ull, sizeof(unsigned long long), 1);
    unsigned int *const group_first_list = (unsigned int *)rank_cell(pair_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const group_count_list = (unsigned int *)rank_cell(pair_count + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = structure_group,
               .bytes = (size_t)((structures + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    unsigned long long structure_groups_used = 0ull;
    unsigned long long entry_used = 0ull;
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        molecule_group_first[molecule] = structure_groups_used;
        const unsigned long long base = structure_groups_used;
        for (unsigned long long at = molecule_query_first[molecule]; at < molecule_query_first[molecule + 1ull]; at += 1ull)
        {
            const unsigned int formula_entry = molecule_query[at];
            for (unsigned long long pair = query_pair_first[formula_entry];
                 pair < (query_pair_first[formula_entry] + query_pair_count[formula_entry]); pair += 1ull)
            {
                const unsigned int structure = set.text[RANK_KEY][pair_spectrum[pair]];
                if (structure_group[structure] == RANK_ABSENT)
                {
                    structure_group[structure] = (unsigned int)(structure_groups_used - base);
                    group_structure[structure_groups_used] = structure;
                    group_count_list[structure_groups_used] = 0u;
                    structure_groups_used += 1ull;
                }
                group_count_list[base + structure_group[structure]] += 1u;
            }
        }
        unsigned int running = (unsigned int)entry_used;
        for (unsigned long long group = base; group < structure_groups_used; group += 1ull)
        {
            group_first_list[group] = running;
            running += group_count_list[group];
        }
        entry_used = running;
        for (unsigned long long group = base; group < structure_groups_used; group += 1ull)
        {
            group_count_list[group] = 0u;
        }
        for (unsigned long long at = molecule_query_first[molecule]; at < molecule_query_first[molecule + 1ull]; at += 1ull)
        {
            const unsigned int formula_entry = molecule_query[at];
            for (unsigned long long pair = query_pair_first[formula_entry];
                 pair < (query_pair_first[formula_entry] + query_pair_count[formula_entry]); pair += 1ull)
            {
                const unsigned int structure = set.text[RANK_KEY][pair_spectrum[pair]];
                const unsigned long long group = base + structure_group[structure];
                group_entry[group_first_list[group] + group_count_list[group]] = (unsigned int)pair;
                group_count_list[group] += 1u;
            }
        }
        for (unsigned long long group = base; group < structure_groups_used; group += 1ull)
        {
            structure_group[group_structure[group]] = RANK_ABSENT;
        }
    }
    molecule_group_first[molecule_count] = structure_groups_used;
    const unsigned long long group_count = structure_groups_used;
    unsigned long long *const structure_first =
        (unsigned long long *)rank_cell(group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const structure_count =
        (unsigned long long *)rank_cell(group_count + 1ull, sizeof(unsigned long long), 1);
    unsigned int *const structure_winner = (unsigned int *)rank_cell(group_count + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long group = 0ull; group < group_count; group += 1ull)
    {
        structure_first[group] = group_first_list[group];
        structure_count[group] = group_count_list[group];
    }
    RankTally structure_tally = {0ull, 0ull};
    const RankGroups structure_groups = {structure_first, structure_count, group_entry, group_count};
    ok = rank_group_best(results, "structure", &ratio_order, order_sign, scores, pair_count, &structure_groups,
                         structure_winner, &structure_tally.microseconds);
    rank_tally_line(results, "structure tournament", &structure_tally);

    // list: per molecule, repeated tournaments over its structures' winning pairs, heaviest first
    unsigned long long *const list_first =
        (unsigned long long *)rank_cell(molecule_count + 1ull, sizeof(unsigned long long), 1);
    unsigned long long *const list_count =
        (unsigned long long *)rank_cell(molecule_count + 1ull, sizeof(unsigned long long), 1);
    unsigned int *const list_entry = (unsigned int *)rank_cell(group_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const list_group = (unsigned int *)rank_cell(group_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const list_winner = (unsigned int *)rank_cell(molecule_count + 1ull, sizeof(unsigned int), 1);
    unsigned int *const chosen =
        (unsigned int *)rank_cell((molecule_count * RANK_LIST_MAX) + 1ull, sizeof(unsigned int), 1);
    unsigned int *const chosen_count = (unsigned int *)rank_cell(molecule_count + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        list_first[molecule] = molecule_group_first[molecule];
        list_count[molecule] = molecule_group_first[molecule + 1ull] - molecule_group_first[molecule];
        for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
             group += 1ull)
        {
            list_entry[group] = structure_winner[group];
            list_group[group] = (unsigned int)group;
        }
    }
    RankTally list_tally = {0ull, 0ull};
    for (unsigned int pick = 0u; ok && (pick < RANK_LIST_MAX); pick += 1u)
    {
        const RankGroups list_groups = {list_first, list_count, list_entry, molecule_count};
        ok = rank_group_best(results, "list", &ratio_order, order_sign, scores, pair_count, &list_groups, list_winner,
                             &list_tally.microseconds);
        for (unsigned long long molecule = 0ull; ok && (molecule < molecule_count); molecule += 1ull)
        {
            if (list_winner[molecule] == RANK_ABSENT)
            {
                continue;
            }
            const unsigned long long base = list_first[molecule];
            unsigned long long at = 0ull;
            while (list_entry[base + at] != list_winner[molecule])
            {
                at += 1ull;
            }
            chosen[(molecule * RANK_LIST_MAX) + chosen_count[molecule]] = list_group[base + at];
            chosen_count[molecule] += 1u;
            for (unsigned long long move = at; (move + 1ull) < list_count[molecule]; move += 1ull)
            {
                list_entry[base + move] = list_entry[base + move + 1ull];
                list_group[base + move] = list_group[base + move + 1ull];
            }
            list_count[molecule] -= 1ull;
        }
    }
    rank_machine_release(&ratio_order);
    rank_tally_line(results, "list tournaments", &list_tally);
    if (!ok)
    {
        return 0;
    }

    // each molecule's list: the chosen structures, then its unscored candidates in structure order, to 25. In validate
    // the answer's place in it. In test the list's SMILES, each structure's from its first reference spectrum that
    // contains one, written as the molecule's line of the submission
    FILE *submission = NULL;
    const unsigned long long molecule_names = set.strings[RANK_MOLECULE].count;
    unsigned int *const structure_smiles = (unsigned int *)rank_cell(structures + 1ull, sizeof(unsigned int), 1);
    unsigned char *const molecule_written = (unsigned char *)rank_cell(molecule_names + 1ull, 1u, 1);
    // each structure's mark, and a molecule's candidates, each structure once, and the room their sort takes
    unsigned char *const structure_mark = (unsigned char *)rank_cell(structures + 1ull, 1u, 1);
    unsigned int *const candidate = (unsigned int *)rank_cell(structures + 1ull, sizeof(unsigned int), 1);
    unsigned int *const candidate_room = (unsigned int *)rank_cell(structures + 1ull, sizeof(unsigned int), 1);
    if (rank_cells_refused(results))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = structure_smiles,
               .bytes = (size_t)((structures + 1ull) * sizeof(unsigned int)), .val = 0xFF);
    if (test)
    {
        for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
        {
            const unsigned int structure = set.text[RANK_KEY][spectrum];
            if (reference[spectrum] && (structure_smiles[structure] == RANK_ABSENT))
            {
                structure_smiles[structure] = set.text[RANK_SMILES][spectrum];
            }
        }
        submission = (engine_directories_make(submission_path, 0) != 0) ? fopen(submission_path, "wb") : NULL;
        if (submission == NULL)
        {
            mmgr_line_put(&results->line, "  the submission did not open: ");
            mmgr_line_put(&results->line, submission_path);
            rank_line_end(results);
            return 0;
        }
        fprintf(submission, "molecule_id,smiles\n");
    }
    unsigned long long given_none = 0ull;
    unsigned long long rank_counts[RANK_LIST_MAX + 1u];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = rank_counts, .bytes = sizeof(rank_counts), .val = 0);
    unsigned long long truth_no_reference = 0ull;
    unsigned long long truth_outside = 0ull;
    unsigned long long truth_unscored = 0ull;
    unsigned long long truth_isomer = 0ull;
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        unsigned long long candidate_count = 0ull;
        for (unsigned long long at = molecule_query_first[molecule]; at < molecule_query_first[molecule + 1ull]; at += 1ull)
        {
            const unsigned int formula_entry = molecule_query[at];
            for (unsigned long long position = 0ull; position < inside_count[formula_entry]; position += 1ull)
            {
                const unsigned int named = inside[inside_first[formula_entry] + position];
                for (unsigned long long each = formula_structure_first[named]; each < formula_structure_first[named + 1ull];
                     each += 1ull)
                {
                    const unsigned int structure = formula_structure[each];
                    if (structure_mark[structure] == 0u)
                    {
                        structure_mark[structure] = 1u;
                        candidate[candidate_count] = structure;
                        candidate_count += 1ull;
                    }
                }
            }
        }
        EMBED_CALL(memor.sort, MemoriaCfg, .dst = candidate, .bytes = sizeof(unsigned int), .scratch = candidate_room,
                   .count = candidate_count, .order = rank_number_order);
        unsigned int list[RANK_LIST_MAX];
        unsigned int listed = 0u;
        for (unsigned int place = 0u; place < chosen_count[molecule]; place += 1u)
        {
            const unsigned int structure = group_structure[chosen[(molecule * RANK_LIST_MAX) + place]];
            list[listed] = structure;
            listed += 1u;
            structure_mark[structure] = 2u;
        }
        for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
             group += 1ull)
        {
            structure_mark[group_structure[group]] = 2u;
        }
        for (unsigned long long each = 0ull; (each < candidate_count) && (listed < RANK_LIST_MAX); each += 1ull)
        {
            if (structure_mark[candidate[each]] == 1u)
            {
                list[listed] = candidate[each];
                listed += 1u;
            }
        }
        if (test)
        {
            // the molecule's id, then its list's SMILES joined by ';', or C where it lists none
            unsigned int written = 0u;
            fprintf(submission, "%s,", rank_string(&set, RANK_MOLECULE, molecule_name[molecule]));
            for (unsigned int at = 0u; at < listed; at += 1u)
            {
                const unsigned int smiles = structure_smiles[list[at]];
                if (smiles != RANK_ABSENT)
                {
                    fprintf(submission, "%s%s", (written == 0u) ? "" : ";", rank_string(&set, RANK_SMILES, smiles));
                    written += 1u;
                }
            }
            fprintf(submission, "%s\n", (written == 0u) ? "C" : "");
            given_none += (written == 0u) ? 1ull : 0ull;
            molecule_written[molecule_name[molecule]] = 1u;
        }
        else
        {
            const unsigned int truth = molecule_name[molecule];
            unsigned int place = 0u;
            for (unsigned int at = 0u; at < listed; at += 1u)
            {
                place = ((place == 0u) && (list[at] == truth)) ? (at + 1u) : place;
            }
            rank_counts[place] += 1ull;
            const int has_reference = structure_reference_first[truth + 1ull] != structure_reference_first[truth];
            const int is_candidate = rank_number_found(candidate, candidate_count, truth);
            int is_scored = 0;
            for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
                 group += 1ull)
            {
                is_scored = is_scored || (group_structure[group] == truth);
            }
            truth_no_reference += has_reference ? 0ull : 1ull;
            truth_outside += (has_reference && !is_candidate) ? 1ull : 0ull;
            truth_unscored += (is_candidate && !is_scored) ? 1ull : 0ull;
            // a miss, the answer past place 1: its cause, and the structure the list puts first, each's cosine
            // squared in parts per million and whether the two share a formula
            if (place != 1u)
            {
                unsigned int truth_group = RANK_ABSENT;
                for (unsigned long long group = molecule_group_first[molecule];
                     group < molecule_group_first[molecule + 1ull]; group += 1ull)
                {
                    truth_group = (group_structure[group] == truth) ? (unsigned int)group : truth_group;
                }
                mmgr_line_put(&results->line, "  miss ");
                mmgr_line_put(&results->line, rank_string(&set, RANK_KEY, truth));
                if (place == 0u)
                {
                    mmgr_line_put(&results->line, " not listed: ");
                }
                else
                {
                    rank_line_decimal(results, " place ", place);
                    mmgr_line_put(&results->line, ": ");
                }
                mmgr_line_put(&results->line, !has_reference  ? "no reference spectrum"
                                                : !is_candidate ? "formula outside every window"
                                                : !is_scored    ? "no matched peak"
                                                                : "outranked");
                if (chosen_count[molecule] != 0u)
                {
                    const unsigned int first_group = chosen[(molecule * RANK_LIST_MAX)];
                    const unsigned int first = group_structure[first_group];
                    const int same_formula = formula_text_of_structure[first] == formula_text_of_structure[truth];
                    mmgr_line_put(&results->line, "; first ");
                    mmgr_line_put(&results->line, rank_string(&set, RANK_KEY, first));
                    rank_line_decimal(results, " cos2 ",
                                      rank_score_ppm(&scores[(size_t)structure_winner[first_group] * score_limbs],
                                                     score_numerator, score_denominator));
                    if (truth_group != RANK_ABSENT)
                    {
                        rank_line_decimal(results, ", answer cos2 ",
                                          rank_score_ppm(&scores[(size_t)structure_winner[truth_group] * score_limbs],
                                                         score_numerator, score_denominator));
                    }
                    mmgr_line_put(&results->line, same_formula ? ", same formula" : ", another formula");
                    truth_isomer += (is_scored && same_formula) ? 1ull : 0ull;
                }
                rank_line_decimal(results, "; candidates ", candidate_count);
                rank_line_end(results);
            }
        }
        for (unsigned long long each = 0ull; each < candidate_count; each += 1ull)
        {
            structure_mark[candidate[each]] = 0u;
        }
        for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
             group += 1ull)
        {
            structure_mark[group_structure[group]] = 0u;
        }
    }
    int finished = 1;
    if (test)
    {
        // a test molecule with no query of a known mode lists none: given C
        unsigned long long unranked = 0ull;
        for (unsigned long long spectrum = set_spectra; spectrum < spectra; spectrum += 1ull)
        {
            const unsigned int name = set.text[RANK_MOLECULE][spectrum];
            if ((name != RANK_ABSENT) && (molecule_written[name] == 0u))
            {
                fprintf(submission, "%s,C\n", rank_string(&set, RANK_MOLECULE, name));
                molecule_written[name] = 1u;
                unranked += 1ull;
            }
        }
        const int closed = fclose(submission) == 0;
        mmgr_line_put(&results->line, "  submission ");
        mmgr_line_put(&results->line, submission_path);
        rank_line_decimal(results, ": molecules ", molecule_count + unranked);
        rank_line_decimal(results, "; with no candidate, given C, ", given_none);
        rank_line_decimal(results, "; with no query of a known mode, given C, ", unranked);
        rank_line_end(results);
        sim_check(results, closed, "the submission is written");
        finished = closed;
    }
    else
    {
        rank_line_decimal(results, "  answers: molecules ", molecule_count);
        rank_line_decimal(results, "; with no reference spectrum ", truth_no_reference);
        rank_line_decimal(results, "; formula outside every window ", truth_outside);
        rank_line_decimal(results, "; candidate with no matched peak ", truth_unscored);
        rank_line_decimal(results, "; scored and outranked by a structure of its formula ", truth_isomer);
        rank_line_end(results);
        mmgr_line_put(&results->line, "  answers at each place, 1 to 25, then not listed:");
        for (unsigned int place = 1u; place <= RANK_LIST_MAX; place += 1u)
        {
            rank_line_decimal(results, " ", rank_counts[place]);
        }
        rank_line_decimal(results, "; ", rank_counts[0]);
        rank_line_end(results);
        // MRR@25 = sum count_k / k over the molecules: each count_k times lcm(1..25) / k summed, over lcm(1..25) times
        // the molecules, read in parts per million
        unsigned long long numerator = 0ull;
        unsigned long long common = 1ull;
        for (unsigned int place = 1u; place <= RANK_LIST_MAX; place += 1u)
        {
            unsigned long long a = common;
            unsigned long long b = place;
            while (b != 0ull)
            {
                const unsigned long long r = a % b;
                a = b;
                b = r;
            }
            common = (common / a) * place;
        }
        for (unsigned int place = 1u; place <= RANK_LIST_MAX; place += 1u)
        {
            numerator += rank_counts[place] * (common / place);
        }
        const unsigned long long parts =
            (molecule_count != 0ull) ? ((numerator * 1000000ull) / (common * molecule_count)) : 0ull;
        rank_line_decimal(results, "  MRR@25: ", parts);
        mmgr_line_put(&results->line, " parts per million, floored");
        rank_line_end(results);
    }
    // the run's cells given back, the newest first
    void *const run_cells[] = {candidate_room,
                               candidate,
                               structure_mark,
                               molecule_written,
                               structure_smiles,
                               chosen_count,
                               chosen,
                               list_winner,
                               list_group,
                               list_entry,
                               list_count,
                               list_first,
                               structure_winner,
                               structure_count,
                               structure_first,
                               group_count_list,
                               group_first_list,
                               molecule_group_first,
                               group_entry,
                               group_structure,
                               structure_group,
                               scores,
                               score_index,
                               norm_sums,
                               norm_squares,
                               norm_entry,
                               norm_count,
                               norm_first,
                               norm_atoms,
                               norm_spectrum,
                               norm_of_query,
                               norm_of_reference,
                               weight_sums,
                               pair_peaks,
                               pair_first,
                               peak_winner,
                               matched_entry,
                               peak_count,
                               peak_first,
                               query_pair_count,
                               query_pair_first,
                               pair_group_first,
                               pair_spectrum,
                               pair_query,
                               matched_group_first,
                               matched,
                               window_query,
                               window_atoms,
                               ion_records,
                               ion_atoms,
                               inside,
                               inside_count,
                               inside_first,
                               formula_structure,
                               formula_structure_first,
                               formula_text,
                               formula,
                               window_of_text,
                               formula_text_of_structure,
                               formula_of_structure,
                               query_adduct,
                               adduct,
                               molecule_query,
                               molecule_query_first,
                               molecule_name,
                               molecule_of_name,
                               query,
                               structure_reference,
                               structure_reference_first,
                               reference,
                               is_query,
                               mode,
                               intensity_floor,
                               envelope_records};
    rank_cells_given(run_cells, (unsigned int)(sizeof(run_cells) / sizeof(run_cells[0])));
    return finished;
}
